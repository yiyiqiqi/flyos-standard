/**
 * @file    flyos_kernel.c
 * @brief   FLYOS内核实现
 * @details 实现FLYOS内核的核心功能，包括任务调度、时间片轮转、
 *          优先级管理、Delta队列、系统监控等。支持抢占式调度、
 *          优先级继承、栈溢出检测、CPU使用率统计等高级特性。
 * @version 1.0.8
 * @author  yiyi
 * @date    2024-07-15
 */

#include <string.h>
#include "flyos_kernel.h"
#include "flyos_mem.h"
#include "flyos_port.h"
#include "flyos_ipc.h"

/*===========================================================================*/
/* 移植层外部变量                                                              */
/*===========================================================================*/

extern volatile void *volatile px_current_tcb;

/*===========================================================================*/
/* 调度器私有变量                                                              */
/*===========================================================================*/

/* 调度器状态结构 */
static struct {
	flyos_tcb_t         *current_task;           /* 当前运行任务 */

    /* 各优先级的就绪列表 */
	flyos_tcb_t         *ready_list[FLYOS_NUM_PRIORITY_LEVELS];
	flyos_u32_t          ready_priorities;

	flyos_tcb_t         *delta_queue_head;       /* Delta 队列头指针 */
	flyos_tick_t         tick_count;             /* 系统tick计数器 */

	flyos_u32_t          lock_nesting;           /* 调度器锁嵌套计数 */
	bool                 started;                /* 调度器启动标志 */
} scheduler = {0};

#if FLYOS_USE_TASK_WATCHDOG || FLYOS_USE_STACK_MONITOR || FLYOS_USE_CPU_USAGE
/* 监控状态结构 */
static struct {
#if FLYOS_USE_TASK_WATCHDOG
	flyos_tick_t last_watchdog_check;           /* 上次看门狗检查时间 */
#endif

#if FLYOS_USE_STACK_MONITOR
	flyos_tick_t last_stack_check;              /* 上次栈检查时间 */
#endif

#if FLYOS_USE_CPU_USAGE
	flyos_u64_t  idle_runtime;          /* idle任务运行时间 */
	flyos_u64_t  total_runtime;         /* 总运行时间 */
	flyos_u32_t  cpu_usage_percent;     /* CPU使用率(0-10000，表示0.00%-100.00%) */
	flyos_u32_t  last_switch_cycles;    /* 上次切换时的周期计数 */
#endif
} monitor_state = {0};
#endif

/* 空闲任务栈 */
static flyos_u8_t idle_task_stack[FLYOS_IDLE_STACK_SIZE]    FLYOS_ALIGNED(8);

/*===========================================================================*/
/* 内核内部函数原型                                                            */
/*===========================================================================*/

/* Delta 队列相关函数 */
static void delta_queue_deal(void (*wake_callback)(flyos_tcb_t*));
static void delta_queue_wake_task(flyos_tcb_t *task);

/* TCB同步和任务管理函数 */
static void sync_current_task_to_port_layer(void);
static flyos_tcb_t* get_current_task_from_port_layer(void);
static bool verify_task_integrity(const flyos_tcb_t *task);
static void idle_task_proc(void *param);

/* 调度器函数 */
FLYOS_INLINE flyos_u8_t find_highest_priority(void);

/* 监控功能函数 */
#if FLYOS_USE_STACK_MONITOR
static flyos_u32_t calc_stack_usage(flyos_tcb_t *task);
static void monitor_check_stack(void);
#endif

#if FLYOS_USE_TASK_WATCHDOG
static void monitor_check_watchdog(void);
#endif

#if FLYOS_USE_CPU_USAGE
static void monitor_update_cpu_usage(void);
#endif

/*===========================================================================*/
/* Delta 队列函数                                                            */
/*===========================================================================*/

/**
 * @brief   将任务插入Delta队列
 * @details 将任务以指定延迟插入Delta队列。Delta队列使用相对增量维护任务的唤醒时间排序。
 *          如果任务已在队列中，将先移除再插入。
 *
 * @param[in,out] task         要插入的任务控制块指针
 * @param[in]     delay_ticks  延迟的tick数
 *
 * @return    无
 *
 * @note      可在中断使能的任务上下文中调用
 * @note      如果task为NULL或delay_ticks为0，函数直接返回
 * @pre       调用前应确保task指针有效
 */
void flyos_delta_queue_insert(flyos_tcb_t *task, flyos_tick_t delay_ticks)
{
	if (task == NULL || delay_ticks == 0) {
		return;
	}

	/* 如果任务已在队列中，先移除 */
	if (task->timer_delta_link.delta > 0 || task->timer_delta_link.next != NULL
        || task->timer_delta_link.prev != NULL) {
		flyos_delta_queue_remove(task);
	}

	/* 初始化 delta 值 */
	task->timer_delta_link.delta = delay_ticks;

	/* 找到插入位置 */
	flyos_tcb_t **insert_location_ref = &scheduler.delta_queue_head;
	flyos_tcb_t *prev_node = NULL;

	while (*insert_location_ref != NULL &&
        task->timer_delta_link.delta >= (*insert_location_ref)->timer_delta_link.delta) {
		task->timer_delta_link.delta -= (*insert_location_ref)->timer_delta_link.delta;
		prev_node = *insert_location_ref;
		insert_location_ref = &(*insert_location_ref)->timer_delta_link.next;
	}

	/* 调整后续节点的 delta */
	if (*insert_location_ref != NULL) {
		(*insert_location_ref)->timer_delta_link.delta -= task->timer_delta_link.delta;
		(*insert_location_ref)->timer_delta_link.prev = task;
	}

	/* 插入节点 */
	task->timer_delta_link.next = *insert_location_ref;
	task->timer_delta_link.prev = prev_node;
	*insert_location_ref = task;
}

/**
 * @brief   从Delta队列移除任务
 * @details 从Delta队列移除任务并调整后续任务的delta值。
 *          如果任务不在队列中，此函数立即返回且不报错。
 *
 * @param[in,out] task  要移除的任务控制块指针
 *
 * @return    无
 *
 * @note      可在中断使能的任务上下文中调用
 * @note      如果task为NULL或不在队列中，函数直接返回
 */
void flyos_delta_queue_remove(flyos_tcb_t *task)
{
	if (task == NULL) {
		return;
	}

	/* 如果不在队列中，直接返回 */
	if (task->timer_delta_link.delta == 0 && task->timer_delta_link.prev == NULL &&
	    scheduler.delta_queue_head != task) {
		return;
	}

	/* 调整后续节点的 delta */
	if (task->timer_delta_link.next != NULL) {
		task->timer_delta_link.next->timer_delta_link.delta += task->timer_delta_link.delta;
		task->timer_delta_link.next->timer_delta_link.prev = task->timer_delta_link.prev;
	}

	/* 从delta队列链表中移除 */
	if (task->timer_delta_link.prev != NULL) {
		task->timer_delta_link.prev->timer_delta_link.next = task->timer_delta_link.next;
	} else {
		/* 任务是队列头 */
		scheduler.delta_queue_head = task->timer_delta_link.next;
	}

	/* 清空节点信息 */
	task->timer_delta_link.next = NULL;
	task->timer_delta_link.prev = NULL;
	task->timer_delta_link.delta = 0;
}

/**
 * @brief   更新Delta队列，并处理到时任务
 *
 * @param[in] wake_callback  唤醒到期任务的回调函数指针
 *
 * @return    无
 *
 * @warning   时间复杂度O(n)，其中n为到期任务数
 */
static void delta_queue_deal(void (*wake_callback)(flyos_tcb_t*))
{
	if (wake_callback == NULL) {
		return;
	}

	/* delta队列为空 */
	if (scheduler.delta_queue_head == NULL) {
		return;
	}

	/* 递减队头的 delta */
	scheduler.delta_queue_head->timer_delta_link.delta --;

	/* 处理所有 delta = 0 的任务 */
	while (scheduler.delta_queue_head != NULL &&
	       scheduler.delta_queue_head->timer_delta_link.delta == 0) {
		flyos_tcb_t *task = scheduler.delta_queue_head;

		/* 移出delta队列，并更新delta队头 */
		scheduler.delta_queue_head = task->timer_delta_link.next;
		if (scheduler.delta_queue_head != NULL) {
			scheduler.delta_queue_head->timer_delta_link.prev = NULL;
		}

        /* 清空节点信息 */
		task->timer_delta_link.next = NULL;
		task->timer_delta_link.prev = NULL;
		task->timer_delta_link.delta = 0;

		/* 唤醒任务 */
		wake_callback(task);
	}
}

/**
 * @brief   从Delta队列唤醒任务
 * @details 将任务从Delta队列移至就绪队列,
 *          并处理超时唤醒（IPC对象）和正常延迟唤醒两种情况。
 *
 * @param[in,out] task  要唤醒的任务控制块指针
 */
static void delta_queue_wake_task(flyos_tcb_t *task)
{
	if (task == NULL) {
		return;
	}

	/* 设置就绪状态 */
	if (task->block_object != NULL) {
		/* 任务等待IPC对象时超时唤醒 */
		task->error_code = FLYOS_TIMEOUT;
		task->state = FLYOS_TASK_READY;

        /* 等待IPC超时唤醒，将任务同步从等待队列中删除 */
        flyos_ipc_remove_task_from_wait_list(task);
	} else {
		/* 任务delay正常唤醒 */
		task->error_code = FLYOS_OK;
		task->state = FLYOS_TASK_READY;
	}
    task->block_reason = FLYOS_BLOCK_NONE;

	flyos_insert_ready_list(task);
}

/*===========================================================================*/
/* TCB 同步私有函数                                                           */
/*===========================================================================*/

/**
 * @brief   同步当前任务到移植层
 * @details 将调度器的当前任务指针同步到移植层的全局变量px_current_tcb；
 *
 * @note    从任务上下文或ISR调用
 */
static void sync_current_task_to_port_layer(void)
{
	px_current_tcb = scheduler.current_task;
}

/**
 * @brief   从移植层获取当前任务
 * @details 从移植层的全局变量px_current_tcb中获取当前任务指针；
 *
 * @return  指向当前任务控制块的指针
 *
 * @note    从任务上下文或ISR调用
 */
static flyos_tcb_t* get_current_task_from_port_layer(void)
{
	return (flyos_tcb_t*)px_current_tcb;
}

/**
 * @brief   验证任务控制块完整性
 * @details 对任务控制块进行完整性检查，包括：
 *          - 栈边界检查
 *          - 栈指针有效性
 *          - 优先级范围
 *          - 栈魔数验证，启用FLYOS_USE_STACK_CHECK时检查栈溢出
 *
 * @param[in] task  要验证的任务控制块指针
 *
 * @retval true   任务有效
 * @retval false  任务无效，栈溢出或参数错误
 *
 * @note    从任务上下文调用
 */
static bool verify_task_integrity(const flyos_tcb_t *task)
{
	if (task == NULL) {
		return false;
	}

	/* 检查栈字段 */
	if (task->stack_base == NULL || task->stack_size == 0) {
		return false;
	}

	if (task->stack_pointer == NULL) {
		return false;
	}

	/* 验证栈指针在边界内 */
	if (task->stack_pointer < task->stack_base) {
		return false;
	}

	if (task->stack_pointer >= (void*)((uint8_t*)task->stack_base + task->stack_size)) {
		return false;
	}

	/* 检查优先级 */
	if (task->priority >= FLYOS_NUM_PRIORITY_LEVELS) {
		return false;
	}

#if FLYOS_USE_STACK_CHECK
	/* 分别检查栈顶和栈底保护魔数 */
	flyos_u32_t *stack_bottom = (flyos_u32_t*)task->stack_base;
	flyos_u32_t *stack_top = (flyos_u32_t*)((uint8_t*)task->stack_base + task->stack_size - sizeof(flyos_u32_t));

	if (*stack_bottom != FLYOS_TASK_STACK_MAGIC_BOTTOM) {
		return false;  /* 底部溢出 */
	}

	if (*stack_top != FLYOS_TASK_STACK_MAGIC_TOP) {
		return false;  /* 顶部溢出 */
	}
#endif

	return true;
}

/*===========================================================================*/
/* 空闲任务                                                                   */
/*===========================================================================*/

/**
 * @brief   空闲任务入口点
 *
 * @param[in] param  未使用的参数
 *
 * @note    在最低优先级(FLYOS_IDLE_PRIORITY)的任务上下文中运行
 * @note    此函数永不返回
 */
static void idle_task_proc(void *param)
{
	FLYOS_UNUSED(param);

	while (1) {
		/* 调用空闲任务钩子函数 */
		flyos_idle_hook();
	}
}

/*===========================================================================*/
/* 优先级管理                                                                 */
/*===========================================================================*/

/**
 * @brief   查找最高优先级就绪任务
 * @details 使用计数尾随零（CTZ）指令从就绪优先级位图中快速查找
 *          具有就绪任务的最高优先级级别
 *
 * @return  最高优先级就绪任务的优先级级别，无就绪任务时返回FLYOS_IDLE_PRIORITY
 *
 * @note    从任务上下文或ISR调用
 */
FLYOS_INLINE flyos_u8_t find_highest_priority(void)
{
    /* 无就绪任务时返回空闲优先级 */
    if (scheduler.ready_priorities == 0) {
		return FLYOS_IDLE_PRIORITY;
	}

	/* 使用 CLZ 指令快速位搜索 */
	return (flyos_u8_t)__builtin_ctz(scheduler.ready_priorities);
}

/*===========================================================================*/
/* 就绪列表管理                                                               */
/*===========================================================================*/

/**
 * @brief 将任务插入就绪列表
 */
void flyos_insert_ready_list(flyos_tcb_t *task)
{
	if (task == NULL) {
		return;
	}

	if (task->rdy_wait_link.next != NULL || task->rdy_wait_link.prev != NULL) {
		flyos_remove_ready_list(task);
	}

	flyos_u8_t priority = task->priority;

	/* 在位图中标记优先级为就绪 */
	scheduler.ready_priorities |= (1UL << priority);

	/* 将任务插入到指定优先级的任务循环链表尾部 */
	if (scheduler.ready_list[priority] == NULL) {
		scheduler.ready_list[priority] = task;
		task->rdy_wait_link.next = task;
		task->rdy_wait_link.prev = task;
	} else {
		flyos_tcb_t *head = scheduler.ready_list[priority];
		flyos_tcb_t *tail = head->rdy_wait_link.prev;

		tail->rdy_wait_link.next = task;
		task->rdy_wait_link.prev = tail;
		task->rdy_wait_link.next = head;
		head->rdy_wait_link.prev = task;
	}

	task->state = FLYOS_TASK_READY;
}

/**
 * @brief 从就绪列表移除任务
 */
void flyos_remove_ready_list(flyos_tcb_t *task)
{
	if (task == NULL) {
		return;
	}

    /* 验证任务是否已在就绪列表中 */
	if (task->rdy_wait_link.next == NULL && task->rdy_wait_link.prev == NULL
        && scheduler.ready_list[task->priority] != task) {
		return;
	}

	flyos_u8_t priority = task->priority;

    /* 从就绪列表中移除任务 */
	if (task->rdy_wait_link.next == task) {
		scheduler.ready_list[priority] = NULL;
		scheduler.ready_priorities &= ~(1UL << priority);
	} else {
		task->rdy_wait_link.prev->rdy_wait_link.next = task->rdy_wait_link.next;
		task->rdy_wait_link.next->rdy_wait_link.prev = task->rdy_wait_link.prev;

		/* 更新头指针 */
		if (scheduler.ready_list[priority] == task) {
			scheduler.ready_list[priority] = task->rdy_wait_link.next;
		}
	}

	task->rdy_wait_link.next = NULL;
	task->rdy_wait_link.prev = NULL;
}

/*===========================================================================*/
/* 内核管理                                                                  */
/*===========================================================================*/

/**
 * @brief 初始化FLYOS内核
 */
flyos_err_t flyos_kernel_init(void)
{
	/* 清除调度器状态 */
	memset(&scheduler, 0, sizeof(scheduler));

	/* 初始化移植层硬件 */
	flyos_port_init();

	/* 初始化堆内存管理器 */
#if FLYOS_USE_HEAP
	flyos_mem_init();
#endif

	/* 创建空闲任务 */
	flyos_task_config_t idle_config = {
		.name = "IDLE",
		.proc = idle_task_proc,
		.param = NULL,
		.stack_buffer = idle_task_stack,
		.stack_size = FLYOS_IDLE_STACK_SIZE,
		.priority = FLYOS_IDLE_PRIORITY,
		.time_slice = FLYOS_TIME_SLICE_TICKS
	};

	flyos_tcb_t *idle_task = flyos_task_create(NULL, &idle_config);
	if (idle_task == NULL || !verify_task_integrity(idle_task)) {
		return FLYOS_ERROR;
	}

	return FLYOS_OK;
}

/**
 * @brief 启动FLYOS调度器
 */
flyos_err_t flyos_kernel_start(void)
{
	/* 选择第一个执行的任务 */
	flyos_u8_t highest_priority = find_highest_priority();
	flyos_tcb_t *first_task = scheduler.ready_list[highest_priority];

	if (first_task == NULL || !verify_task_integrity(first_task)) {
		FLYOS_ASSERT(0);
		return FLYOS_ERROR;
	}

	/* 设置当前任务 */
	scheduler.current_task = first_task;
	scheduler.current_task->state = FLYOS_TASK_READY;

	/* 同步到移植层 */
	sync_current_task_to_port_layer();

	/* 验证同步 */
	if (get_current_task_from_port_layer() != scheduler.current_task) {
		FLYOS_ASSERT(0);
		return FLYOS_ERROR;
	}

	/* 标记内核启动状态 */
	scheduler.started = true;

	/* 启动调度器 */
	flyos_port_start_scheduler();

	/* 正常情况下不会到达此处 */
	return FLYOS_ERROR;
}

/**
 * @brief 获取系统tick计数
 */
flyos_tick_t flyos_kernel_get_tick_count(void)
{
	return scheduler.tick_count;
}

/**
 * @brief 获取内核版本号
 */
flyos_u32_t flyos_kernel_get_version(void)
{
	return (FLYOS_VERSION_MAJOR << 16) | (FLYOS_VERSION_MINOR << 8) | FLYOS_VERSION_PATCH;
}

/*===========================================================================*/
/* 任务管理                                                                   */
/*===========================================================================*/

/**
 * @brief 创建新任务
 */
flyos_tcb_t* flyos_task_create(
	flyos_tcb_t *tcb_buffer,
	const flyos_task_config_t *config)
{
	flyos_tcb_t *task;
	flyos_u8_t flags = 0;
	void *stack_buffer = config->stack_buffer;

	if (config == NULL || config->proc == NULL ||
        config->priority > FLYOS_IDLE_PRIORITY) {
		return NULL;
	}

	/* 验证栈大小，栈不能过小 */
	if (config->stack_size < 128) {
		return NULL;
	}

	if (tcb_buffer != NULL) {
		flags |= FLYOS_TASK_FLAG_TCB_STATIC;
	}

#if FLYOS_USE_HEAP
	/* 动态栈分配支持，从堆中分配栈空间 */
	if (stack_buffer == NULL) {
		stack_buffer = flyos_mem_alloc(config->stack_size);
		if (stack_buffer == NULL) {
			return NULL;
		}
		flags |= FLYOS_TASK_FLAG_STACK_DYNAMIC;
	}
#else
	/* 无堆支持，栈必须由用户提供 */
	if (stack_buffer == NULL) {
		return NULL;
	}
#endif

	if (tcb_buffer == NULL) {
#if FLYOS_USE_HEAP
		/* 动态创建，从堆中分配TCB */
		task = (flyos_tcb_t *)flyos_mem_alloc(sizeof(flyos_tcb_t));
		if (task == NULL) {
			if (flags & FLYOS_TASK_FLAG_STACK_DYNAMIC) {
				flyos_mem_free(stack_buffer);
			}
			return NULL;
		}
#else
		/* 堆未启用时，动态创建失败 */
		if (flags & FLYOS_TASK_FLAG_STACK_DYNAMIC) {
			flyos_mem_free(stack_buffer);
		}
		return NULL;
#endif
	} else {
		/* 静态创建，使用用户提供的TCB缓冲区 */
		task = tcb_buffer;
	}

	/* 初始化 TCB */
	memset(task, 0, sizeof(flyos_tcb_t));

	strncpy(task->name, config->name, FLYOS_MAX_TASK_NAME - 1);
	task->name[FLYOS_MAX_TASK_NAME - 1] = '\0';

	task->priority = config->priority;
	task->base_priority = config->priority;
	task->state = FLYOS_TASK_INIT;
	task->flags = flags;
	task->time_slice = config->time_slice ? config->time_slice : FLYOS_TIME_SLICE_TICKS;
	task->remaining_time_slice = task->time_slice;

	/* 设置栈字段 */
	task->stack_base = stack_buffer;
	task->stack_size = config->stack_size;

	/* 添加栈保护 */
#if FLYOS_USE_STACK_CHECK
	/* 填充整个栈空间为魔数，用于栈使用率监控 */
	flyos_u32_t *stack_fill_ptr = (flyos_u32_t *)stack_buffer;
	flyos_u32_t stack_words = config->stack_size / sizeof(flyos_u32_t);
	for (flyos_u32_t i = 0; i < stack_words; i++) {
		stack_fill_ptr[i] = FLYOS_TASK_STACK_MAGIC_BOTTOM;
	}
#endif

	/* 初始化栈 */
	task->stack_pointer = flyos_port_init_stack(
		stack_buffer,
		config->stack_size,
		config->proc,
		config->param
	);

	/* 验证任务完整性 */
	if (!verify_task_integrity(task)) {
#if FLYOS_USE_HEAP
		/* 释放动态分配的栈 */
		if (task->flags & FLYOS_TASK_FLAG_STACK_DYNAMIC) {
			flyos_mem_free(stack_buffer);
		}
		/* 释放动态分配的TCB */
		if (!(task->flags & FLYOS_TASK_FLAG_TCB_STATIC)) {
			flyos_mem_free(task);
		}
#endif
		return NULL;
	}

	/* 添加新任务到就绪列表 */
	flyos_base_t level = flyos_enter_critical();
	flyos_insert_ready_list(task);
	flyos_exit_critical(level);

	/* 判断是否需要立即进行调度 */
	if (scheduler.started && task->priority < scheduler.current_task->priority) {
		flyos_scheduler();
	}

	return task;
}

/**
 * @brief 删除任务
 */
flyos_err_t flyos_task_delete(flyos_tcb_t *task)
{
	if (task == NULL) {
		task = scheduler.current_task;
	}

	flyos_base_t level = flyos_enter_critical();

	/* 从列表中移除 */
	if (task->state == FLYOS_TASK_READY) {
		flyos_remove_ready_list(task);
	}

	/* 从 Delta 队列移除 */
	flyos_delta_queue_remove(task);

	task->state = FLYOS_TASK_CLOSE;

	flyos_exit_critical(level);

	/* 释放资源 */
#if FLYOS_USE_HEAP
	/* 释放动态分配的栈 */
	if (task->flags & FLYOS_TASK_FLAG_STACK_DYNAMIC) {
		flyos_mem_free(task->stack_base);
	}
	/* 释放动态分配的TCB */
	if (!(task->flags & FLYOS_TASK_FLAG_TCB_STATIC)) {
		flyos_mem_free(task);
	}
#endif

	/* 判断是否为任务自身操作删除 */
	if (task == scheduler.current_task) {
		flyos_scheduler();
        /* 防止任务被删除后继续执行，访问已释放资源 */
		while (1);
	}

	return FLYOS_OK;
}

/**
 * @brief 挂起任务
 */
flyos_err_t flyos_task_suspend(flyos_tcb_t *task)
{
	if (task == NULL) {
		task = scheduler.current_task;
	}

	flyos_base_t level = flyos_enter_critical();

    /* 若任务已就绪，则将其从就绪列表中移除并标记为阻塞状态 */
	if (task->state == FLYOS_TASK_READY) {
		flyos_remove_ready_list(task);
		task->state = FLYOS_TASK_BLOCK;
		task->block_reason = FLYOS_BLOCK_SUSPEND;
	}

	flyos_exit_critical(level);

    /* 若挂起的是当前任务，则进行任务切换 */
	if (task == scheduler.current_task) {
		flyos_scheduler();
	}

	return FLYOS_OK;
}

/**
 * @brief 恢复挂起的任务
 */
flyos_err_t flyos_task_resume(flyos_tcb_t *task)
{
	if (task == NULL || task->state != FLYOS_TASK_BLOCK ||
        task->block_reason != FLYOS_BLOCK_SUSPEND) {
		return FLYOS_INVALID;
	}

	flyos_base_t level = flyos_enter_critical();

	task->block_reason = FLYOS_BLOCK_NONE;

    /* 恢复任务后，插入就绪列表 */
	flyos_insert_ready_list(task);

	flyos_exit_critical(level);

    /* 只在必要时进行任务切换 */
    if (scheduler.started && task->priority < scheduler.current_task->priority) {
        flyos_scheduler();
    }

	return FLYOS_OK;
}

/**
 * @brief 更改任务优先级
 */
flyos_err_t flyos_task_set_priority(flyos_tcb_t *task, flyos_u8_t priority)
{
	if (task == NULL || priority >= FLYOS_IDLE_PRIORITY) {
		return FLYOS_INVALID;
	}

	flyos_base_t level = flyos_enter_critical();

    /* 若任务已就绪，则将其从就绪列表中移除并重新插入 */
	if (task->state == FLYOS_TASK_READY) {
		flyos_remove_ready_list(task);
		task->priority = priority;
		task->base_priority = priority;
		flyos_insert_ready_list(task);
	} else {
		task->priority = priority;
		task->base_priority = priority;
	}

	flyos_exit_critical(level);

	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief 延迟当前任务指定tick数
 */
flyos_err_t flyos_task_delay(flyos_tick_t ticks)
{
	if (ticks == 0) {
		return FLYOS_OK;
	}

	flyos_base_t level = flyos_enter_critical();

    /* 主动阻塞当前任务，将当前任务从就绪列表中移除 */
	flyos_tcb_t *task = scheduler.current_task;
	flyos_remove_ready_list(task);
	task->state = FLYOS_TASK_BLOCK;
	task->block_reason = FLYOS_BLOCK_DELAY;
    task->block_object = NULL;

    /* 将任务插入Delta队列 */
	flyos_delta_queue_insert(task, ticks);

	flyos_exit_critical(level);

    /* 立即进行任务切换 */
	flyos_scheduler();

	return task->error_code;
}

/**
 * @brief 延迟到绝对时间点
 */
flyos_err_t flyos_task_delay_until(flyos_tick_t *previous_wake_time, flyos_tick_t increment)
{
	if (previous_wake_time == NULL) {
		return FLYOS_INVALID;
	}

	flyos_base_t level = flyos_enter_critical();

	flyos_tick_t current_time = flyos_kernel_get_tick_count();
	flyos_tick_t target_time = *previous_wake_time + increment;

	/* 更新上次唤醒时间 */
	*previous_wake_time = target_time;

	/* 计算延迟，delay为无符号数，无需额外处理tick溢出情况 */
	flyos_tick_t delay;
	if (target_time > current_time) {
		delay = target_time - current_time;
	} else {
		/* 错过截止时间时，延迟 1 tick */
		delay = 1;
	}

	flyos_exit_critical(level);

	return flyos_task_delay(delay);
}

/**
 * @brief 获取当前运行任务
 */
flyos_tcb_t* flyos_task_get_current(void)
{
	return scheduler.current_task;
}

/**
 * @brief 主动让出处理器给其他任务
 */
flyos_err_t flyos_task_yield(void)
{
	flyos_base_t level = flyos_enter_critical();

	/* 将当前任务移到队列末尾，当前优先级只有当前一个任务则不会进行任务切换 */
	flyos_tcb_t *task = scheduler.current_task;
	if (scheduler.ready_list[task->priority] != task || task->rdy_wait_link.next != task) {
		scheduler.ready_list[task->priority] = task->rdy_wait_link.next;
	}

	flyos_exit_critical(level);

    /* 执行任务切换 */
	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief 获取任务名称
 */
const char* flyos_task_get_name(flyos_tcb_t *task)
{
	if (task == NULL) {
		task = scheduler.current_task;
	}
	return task->name;
}

/**
 * @brief 获取任务状态
 */
flyos_task_state_t flyos_task_get_state(flyos_tcb_t *task)
{
	if (task == NULL) {
		task = scheduler.current_task;
	}
	return (flyos_task_state_t)task->state;
}

/*===========================================================================*/
/*调度器控制                                                                  */
/*===========================================================================*/

/**
 * @brief 触发任务调度
 */
void flyos_scheduler(void)
{
	if (scheduler.lock_nesting > 0 || !scheduler.started) {
		return;
	}

	flyos_base_t level = flyos_enter_critical();

	flyos_u8_t highest_priority = find_highest_priority();
	flyos_tcb_t *next_task = scheduler.ready_list[highest_priority];

	if (next_task != scheduler.current_task) {
		/* 触发上下文切换 */
		flyos_port_yield();
	}

	flyos_exit_critical(level);
}

/**
 * @brief 锁定调度器以防止任务切换
 */
void flyos_scheduler_lock(void)
{
	flyos_base_t level = flyos_enter_critical();
	scheduler.lock_nesting ++;
	flyos_exit_critical(level);
}

/**
 * @brief 解锁调度器以允许任务切换
 */
void flyos_scheduler_unlock(void)
{
    flyos_base_t level = flyos_enter_critical();

    if (scheduler.lock_nesting > 0) {
        scheduler.lock_nesting --;
    }

    /* 临界区保护整个检查和调度过程 */
    if (scheduler.lock_nesting == 0 && scheduler.started) {
        flyos_u8_t highest_priority = find_highest_priority();
        flyos_tcb_t *next_task = scheduler.ready_list[highest_priority];

        if (next_task != scheduler.current_task) {
            flyos_port_yield();
            return;
        }
    }

    flyos_exit_critical(level);
}

/**
 * @brief 获取调度器运行状态
 */
flyos_u32_t flyos_scheduler_get_state(void)
{
	return scheduler.started ? 1 : 0;
}

/*===========================================================================*/
/* 监控功能实现                                                               */
/*===========================================================================*/

#if FLYOS_USE_STACK_MONITOR

/**
 * @brief   计算任务栈使用量
 * @details 从栈底扫描以找到第一个非魔数值，计算已使用的栈空间。
 *          如果启用FLYOS_USE_TASK_STAT，更新任务的最大栈使用统计。
 *
 * @param[in] task  要检查的任务控制块指针
 *
 * @return  栈上已使用的字节数
 *
 * @note    从监控函数调用
 */
static flyos_u32_t calc_stack_usage(flyos_tcb_t *task)
{
	if (task == NULL || task->stack_base == NULL) {
		return 0;
	}

	/* 从栈底向上扫描，找到第一个非填充值 */
	flyos_u32_t *stack_ptr = (flyos_u32_t *)task->stack_base;
	flyos_u32_t stack_words = task->stack_size / sizeof(flyos_u32_t);
	flyos_u32_t used_words = 0;

	/* 跳过底部magic word进行计算 */
	for (flyos_u32_t i = 1; i < stack_words; i++) {
		if (stack_ptr[i] != FLYOS_TASK_STACK_MAGIC_BOTTOM) {
			used_words = stack_words - i;
			break;
		}
	}

	flyos_u32_t used_bytes = used_words * sizeof(flyos_u32_t);

#if FLYOS_USE_TASK_STAT
	/* 更新最大栈使用量 */
	if (used_bytes > task->max_stack_used) {
		task->max_stack_used = used_bytes;
	}
#endif

	return used_bytes;
}

/**
 * @brief   检查所有任务的栈使用量
 * @details 遍历所有就绪任务并检查其栈使用量是否超过警告阈值。
 *          如果超过阈值，调用flyos_stack_overflow_hook()钩子函数。
 *
 * @note    从SysTick处理程序定期调用
 */
static void monitor_check_stack(void)
{
	/* 遍历所有就绪任务，检查栈使用率 */
	for (flyos_u8_t prio = 0; prio <= FLYOS_IDLE_PRIORITY; prio ++) {
		flyos_tcb_t *task = scheduler.ready_list[prio];
		if (task != NULL) {
			do {
				flyos_u32_t usage = calc_stack_usage(task);
				flyos_u32_t threshold = (task->stack_size * FLYOS_STACK_WARNING_THRESHOLD) / 100;

				if (usage > threshold) {
                    /* 触发栈溢出钩子 */
					flyos_stack_overflow_hook(task);
				}

				task = task->rdy_wait_link.next;
			} while (task != scheduler.ready_list[prio] && task != NULL);
		}
	}
}

#endif

#if FLYOS_USE_TASK_WATCHDOG

/**
 * @brief   检查任务看门狗超时
 * @details 检查关键任务是否阻塞时间过长。
 *
 * @note    从SysTick处理程序定期调用
 * @todo    实现看门狗检查功能
 */
static void monitor_check_watchdog(void)
{
	/* TODO: 实现看门狗检查 */
}

#endif

#if FLYOS_USE_CPU_USAGE

/**
 * @brief   更新CPU使用率统计
 * @details 基于空闲任务运行时间计算CPU使用率百分比，每秒调用一次。
 *
 * @note    从SysTick处理程序调用
 * @todo    实现CPU占用率统计功能
 */
static void monitor_update_cpu_usage(void)
{
	/* TODO: 实现CPU占用率统计 */
}

/**
 * @brief 获取当前CPU使用率
 */
flyos_u32_t flyos_get_cpu_usage(void)
{
	return monitor_state.cpu_usage_percent;
}

#endif

/*===========================================================================*/
/* 中断处理器                                                                */
/*===========================================================================*/

/**
 * @brief 系统tick中断处理程序
 */
void flyos_systick_handler(void)
{
	flyos_base_t level = flyos_enter_critical();

	scheduler.tick_count ++;
	flyos_tick_t current_tick = scheduler.tick_count;

	/* 处理 Delta 队列到期任务 */
	delta_queue_deal(delta_queue_wake_task);

	/* 处理当前任务的时间片 */
	flyos_tcb_t *curr = scheduler.current_task;
	if (curr == NULL) {
        goto timeslice_end;
	}
	if (curr->remaining_time_slice == 0) {
        goto timeslice_end;
	}

	/* 递减时间片 */
	curr->remaining_time_slice --;
	if (curr->remaining_time_slice > 0) {
        goto timeslice_end;
	}

	/* 时间片耗尽，重置当前任务时间片，并调整当前任务所在优先级队列的链表头 */
	curr->remaining_time_slice = curr->time_slice;
	/* 仅当任务仍在就绪列表中且不是唯一节点时才轮转链表头 */
	if (curr->rdy_wait_link.next != NULL && curr->rdy_wait_link.next != curr) {
		scheduler.ready_list[curr->priority] = curr->rdy_wait_link.next;
	}

timeslice_end:

	flyos_exit_critical(level);

	/* 监控功能在临界区外执行，减少中断延迟 */
#if FLYOS_USE_TASK_WATCHDOG
	/* 看门狗检查 */
	if (current_tick - monitor_state.last_watchdog_check >= FLYOS_WATCHDOG_CHECK_INTERVAL) {
		monitor_state.last_watchdog_check = current_tick;
		monitor_check_watchdog();
	}
#endif

#if FLYOS_USE_STACK_MONITOR
	/* 栈监控检查 */
	if (current_tick - monitor_state.last_stack_check >= FLYOS_STACK_CHECK_INTERVAL) {
		monitor_state.last_stack_check = current_tick;
		monitor_check_stack();
	}
#endif

#if FLYOS_USE_CPU_USAGE
	/* CPU使用率统计 */
	if (current_tick % 1000 == 0 && current_tick > 0) {
		monitor_update_cpu_usage();
	}
#endif

	/* 调用tick钩子 */
	flyos_tick_hook();

	/* 内核调度器 */
	flyos_scheduler();
}

/**
 * @brief 执行上下文切换到下一个任务
 */
void flyos_context_switch(void)
{
	/* 选择下一个任务 */
	flyos_u8_t highest_priority = find_highest_priority();
	flyos_tcb_t *next_task = scheduler.ready_list[highest_priority];

	if (next_task == NULL || !verify_task_integrity(next_task)) {
		FLYOS_ASSERT(0);
		flyos_port_disable_interrupts();
		while (1);
	}

	/* 任务更新 */
	scheduler.current_task = next_task;
	scheduler.current_task->state = FLYOS_TASK_READY;

	/* 同步到移植层 */
	sync_current_task_to_port_layer();

	/* 更新统计信息 */
#if FLYOS_USE_TASK_STAT
	scheduler.current_task->switch_count++;
#endif

	/* 检查栈溢出 */
#if FLYOS_USE_STACK_CHECK
	flyos_u32_t *stack_bottom = (flyos_u32_t*)scheduler.current_task->stack_base;
	flyos_u32_t *stack_top = (flyos_u32_t*)((uint8_t*)scheduler.current_task->stack_base +
								   scheduler.current_task->stack_size - sizeof (flyos_u32_t));

	if (*stack_bottom != FLYOS_TASK_STACK_MAGIC_BOTTOM ||
		*stack_top != FLYOS_TASK_STACK_MAGIC_TOP) {
		flyos_stack_overflow_hook(scheduler.current_task);
	}
#endif
}

/*===========================================================================*/
/* 弱钩子实现                                                                */
/*===========================================================================*/

/**
 * @brief 空闲任务钩子函数
 */
FLYOS_WEAK void flyos_idle_hook(void)
{
	/* 用户可重写 */
}

/**
 * @brief 栈溢出钩子函数
 */
FLYOS_WEAK void flyos_stack_overflow_hook(flyos_tcb_t *task)
{
	FLYOS_UNUSED(task);
	while (1);
}

/**
 * @brief 断言失败钩子函数
 */
FLYOS_WEAK void flyos_assert_failed(const char *file, int line)
{
	FLYOS_UNUSED(file);
	FLYOS_UNUSED(line);
	flyos_port_disable_interrupts();
	while (1);
}

/*===========================================================================*/
