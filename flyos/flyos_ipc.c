/**
 * @file    flyos_ipc.c
 * @brief   FLYOS 进程间通信机制实现
 * @details 提供信号量、互斥锁、消息队列、事件标志、邮箱等IPC对象的完整实现。
 *          包含O(1)等待队列、优先级继承、位图索引优化等。
 * @version 1.0.12
 * @author  yiyi
 * @date    2024-9-15
 */

#include "flyos_ipc.h"
#include "flyos_kernel.h"
#include "flyos_mem.h"
#include <string.h>

/*===========================================================================*/
/* 通用IPC宏定义                                                             */
/*===========================================================================*/

/* 临界区管理宏 */
#define IPC_ENTER(obj, ret) \
	do { \
		if (!(obj)) return (ret); \
		level = flyos_enter_critical(); \
	} while(0)

#define IPC_EXIT() flyos_exit_critical(level)

/* ISR临界区管理宏 */
#define IPC_ENTER_ISR(obj, ret) \
	do { \
		if (!(obj)) return (ret); \
		level = flyos_enter_critical_from_isr(); \
	} while(0)

#define IPC_EXIT_ISR() flyos_exit_critical_from_isr(level)

/*===========================================================================*/
/* 等待队列                                                                   */
/*===========================================================================*/

/**
 * @brief   初始化等待队列
 * @details 初始化等待队列的位图和优先级链表数组，O(1)时间复杂度。
 *
 * @param[in,out] wq  等待队列指针
 *
 * @note    任意上下文初始化时调用
 */
static void wait_queue_init(flyos_wait_queue_t *wq)
{
	if (wq == NULL) {
		return;
	}

	wq->ready_bitmap = 0;
	memset(wq->prio_list, 0, sizeof(wq->prio_list));
}

/**
 * @brief   插入任务到等待队列
 * @details 将任务插入到对应优先级的链表头部，并更新位图。
 *          使用O(1)插入算法。
 *
 * @param[in,out] wq    等待队列指针
 * @param[in,out] task  要插入的任务TCB指针
 *
 * @note    临界区内调用
 */
static void wait_queue_insert(flyos_wait_queue_t *wq, flyos_tcb_t *task)
{
	if (wq == NULL || task == NULL) {
		return;
	}

	flyos_u8_t priority = task->priority;

	/* 插入到对应优先级链表头部 */
	task->rdy_wait_link.next = wq->prio_list[priority];
	task->rdy_wait_link.prev = NULL;

	if (wq->prio_list[priority] != NULL) {
		wq->prio_list[priority]->rdy_wait_link.prev = task;
	}

	wq->prio_list[priority] = task;

	/* 更新位图 */
	wq->ready_bitmap |= (1UL << priority);
}

/**
 * @brief   从等待队列移除任务
 * @details 从对应优先级的链表中移除任务，如果链表变空则清除位图标志。
 *          使用O(1)移除算法。
 *
 * @param[in,out] wq    等待队列指针
 * @param[in,out] task  要移除的任务TCB指针
 *
 * @note    临界区内调用
 */
static void wait_queue_remove(flyos_wait_queue_t *wq, flyos_tcb_t *task)
{
	if (wq == NULL || task == NULL) {
		return;
	}

	flyos_u8_t priority = task->priority;

	/* 从链表移除 */
	if (task->rdy_wait_link.prev != NULL) {
		task->rdy_wait_link.prev->rdy_wait_link.next = task->rdy_wait_link.next;
	} else {
		/* 任务是链表头 */
		wq->prio_list[priority] = task->rdy_wait_link.next;
	}

	if (task->rdy_wait_link.next != NULL) {
		task->rdy_wait_link.next->rdy_wait_link.prev = task->rdy_wait_link.prev;
	}

	/* 如果该优先级链表为空，清除位图 */
	if (wq->prio_list[priority] == NULL) {
		wq->ready_bitmap &= ~(1UL << priority);
	}

	/* 清除任务指针 */
	task->rdy_wait_link.next = NULL;
	task->rdy_wait_link.prev = NULL;
}

/**
 * @brief   移除并返回最高优先级任务
 * @details 使用__builtin_ctz硬件指令快速查找最高优先级，最低位，
 *          从队列移除该任务并返回，O(1)时间复杂度。
 *
 * @param[in,out] wq  等待队列指针
 *
 * @return  最高优先级任务TCB指针，队列为空时返回NULL
 *
 * @note    临界区内调用
 */
static flyos_tcb_t* wait_queue_remove_highest(flyos_wait_queue_t *wq)
{
	if (wq == NULL || wq->ready_bitmap == 0) {
		return NULL;
	}

	/* 查找最高优先级 */
	flyos_u8_t highest_priority = (flyos_u8_t)__builtin_ctz(wq->ready_bitmap);

	/* 获取该优先级链表头部任务 */
	flyos_tcb_t *task = wq->prio_list[highest_priority];

	if (task != NULL) {
		/* 从队列移除 */
		wait_queue_remove(wq, task);
	}

	return task;
}

/**
 * @brief   检查等待队列是否为空
 * @details 通过检查ready_bitmap是否为0来判断队列是否为空，O(1)时间复杂度。
 *
 * @param[in] wq  等待队列指针
 *
 * @retval true   队列为空
 * @retval false  队列非空
 */
static bool wait_queue_is_empty(flyos_wait_queue_t *wq)
{
	if (wq == NULL) {
		return true;
	}

	return (wq->ready_bitmap == 0);
}

/*===========================================================================*/
/* 类型安全检查                                                               */
/*===========================================================================*/

/**
 * @brief   验证IPC对象有效性
 * @details 检查IPC对象的魔术数字和类型标识，防止野指针和类型混淆。
 *
 * @param[in] base           IPC基类指针
 * @param[in] expected_type  期望的对象类型
 *
 * @retval true   有效对象
 * @retval false  无效对象（野指针或类型不匹配）
 */
static inline bool ipc_object_valid(const flyos_ipc_base_t *base,
                                    flyos_ipc_type_t expected_type)
{
	if (unlikely(base == NULL)) {
		return false;
	}

	if (unlikely(base->magic != FLYOS_IPC_MAGIC)) {
		FLYOS_ASSERT(0);  /* 魔术数字错误：可能是野指针或内存损坏 */
		return false;
	}

	if (unlikely(base->type != expected_type)) {
		FLYOS_ASSERT(0);  /* 类型错误：IPC对象类型不匹配 */
		return false;
	}

	return true;
}

/**
 * IPC_CHECK_TYPE - 类型检查宏
 * @obj: IPC对象指针
 * @expected_type: 期望的对象类型
 *
 * @note: 检查IPC对象的魔术数字和类型标识，检查失败时直接返回FLYOS_INVALID。
 */
#define IPC_CHECK_TYPE(obj, expected_type) \
	do { \
		if (unlikely(!ipc_object_valid(&(obj)->base, (expected_type)))) { \
			return FLYOS_INVALID; \
		} \
	} while(0)

/**
 * @brief   初始化IPC基类（静态内联函数）
 * @details 初始化IPC对象的公共字段：魔术数字、类型标识、标志位、等待队列。
 *          如果启用调试，记录创建位置。
 *
 * @param[in,out] base  IPC基类指针
 * @param[in] type      IPC对象类型（SEMAPHORE/MUTEX/QUEUE/EVENT/MAILBOX）
 * @param[in] flags     对象标志位
 *                      - 0: 动态创建的对象
 *                      - FLYOS_IPC_FLAG_STATIC: 静态创建的对象
 *                      - FLYOS_IPC_FLAG_QUEUE_BUFFER_STATIC: 队列缓冲区静态（仅队列使用）
 * @param[in] name      对象名称（可选，取决于FLYOS_USE_IPC_NAME配置）
 *
 * @note    创建IPC对象时调用，flags用于delete时判断是否释放内存
 */
static inline void ipc_base_init(flyos_ipc_base_t *base,
                                 flyos_ipc_type_t type,
                                 flyos_u8_t flags,
                                 const char *name)
{
	base->magic = FLYOS_IPC_MAGIC;
	base->type = type;
	base->flags = flags;
	base->reserved1 = 0;
	base->reserved2 = 0;

#if FLYOS_USE_IPC_NAME
	base->name = name;
#else
	(void)name;
#endif

#if FLYOS_USE_ASSERT
	base->file = __FILE__;
	base->line = __LINE__;
#endif
}

/*===========================================================================*/
/* 通用IPC辅助函数                                                           */
/*===========================================================================*/

/**
 * @brief   阻塞当前任务到IPC对象的等待队列
 * @details 将当前任务从就绪队列移除，设置为阻塞状态，加入等待队列，
 *          并根据timeout设置超时定时器。
 *
 * @param[in,out] wq      等待队列指针
 * @param[in] ipc_obj     IPC对象指针
 * @param[in] timeout     超时时间（tick）
 *
 * @note    临界区内调用，任务上下文
 */
static void ipc_block_current_task(flyos_wait_queue_t *wq, void *ipc_obj, flyos_tick_t timeout)
{
	flyos_tcb_t *current = flyos_task_get_current();

	/* 从就绪队列移除 */
	flyos_remove_ready_list(current);

	/* 设置阻塞状态 */
	current->state = FLYOS_TASK_BLOCK;
	current->block_reason = FLYOS_BLOCK_IPC;
	current->block_object = ipc_obj;
	current->error_code = FLYOS_OK;

	/* 插入等待队列 */
	wait_queue_insert(wq, current);

	/* 设置超时 */
	if (timeout != FLYOS_WAIT_FOREVER) {
		flyos_delta_queue_insert(current, timeout);
	}
}

/**
 * @brief   唤醒等待队列中的最高优先级任务
 * @details 从等待队列移除最高优先级任务，取消其超时定时器，
 *          设置为就绪状态并加入就绪队列。
 *
 * @param[in,out] wq  等待队列指针
 *
 * @return  被唤醒的任务TCB，队列为空时返回NULL
 *
 * @note    临界区内调用
 */
static flyos_tcb_t* ipc_wake_highest_task(flyos_wait_queue_t *wq)
{
	flyos_tcb_t *task = wait_queue_remove_highest(wq);

	if (task != NULL) {
		/* 从 Delta 队列移除 */
		flyos_delta_queue_remove(task);

		/* 设置就绪状态 */
		task->error_code = FLYOS_OK;
		task->state = FLYOS_TASK_READY;
		task->block_reason = FLYOS_BLOCK_NONE;
		task->block_object = NULL;

		/* 插入就绪队列 */
		flyos_insert_ready_list(task);
	}

	return task;
}

/*===========================================================================*/
/* 信号量实现                                                                */
/*===========================================================================*/

#if FLYOS_USE_SEMAPHORE

/**
 * 信号量使用场景说明：
 *
 * 1. 资源计数（Counting Semaphore）
 *    - 场景：管理有限数量的资源（如缓冲区池、连接池）
 *    - 示例：
 *      flyos_semaphore_t *buffer_pool = flyos_semaphore_create(5, 5); // 5个可用缓冲区
 *      flyos_semaphore_wait(buffer_pool, FLYOS_WAIT_FOREVER);  // 获取一个缓冲区
 *      // ... 使用缓冲区 ...
 *      flyos_semaphore_signal(buffer_pool);  // 释放缓冲区
 *
 * 2. 生产者-消费者同步（Producer-Consumer）
 *    - 场景：生产者生成数据，消费者消费数据
 *    - 示例：
 *      flyos_semaphore_t *items = flyos_semaphore_create(0, 10);  // 初始无数据
 *      // 生产者任务：
 *      produce_item();
 *      flyos_semaphore_signal(items);  // 通知有新数据
 *      // 消费者任务：
 *      flyos_semaphore_wait(items, FLYOS_WAIT_FOREVER);  // 等待数据
 *      consume_item();
 *
 * 3. 任务同步（Task Synchronization）
 *    - 场景：一个任务等待另一个任务完成某个操作
 *    - 示例：
 *      flyos_semaphore_t *done_signal = flyos_semaphore_create(0, 1);
 *      // 工作任务：
 *      do_work();
 *      flyos_semaphore_signal(done_signal);  // 通知完成
 *      // 等待任务：
 *      flyos_semaphore_wait(done_signal, FLYOS_WAIT_FOREVER);  // 等待完成
 *
 * 4. 中断与任务同步（ISR to Task）
 *    - 场景：中断服务程序通知任务处理数据
 *    - 示例：
 *      flyos_semaphore_t *isr_event = flyos_semaphore_create(0, 1);
 *      // 中断处理：
 *      void UART_IRQHandler(void) {
 *          flyos_semaphore_signal_from_isr(isr_event);
 *      }
 *      // 任务处理：
 *      flyos_semaphore_wait(isr_event, FLYOS_WAIT_FOREVER);
 *      process_uart_data();
 *
 * ⚠️ 注意事项：
 *    - 信号量不支持优先级继承！请勿用于临界区保护
 *    - 用于临界区保护请使用互斥锁（flyos_mutex_t）
 *    - 信号量可以被任意任务signal，没有"拥有者"概念
 */

/**
 * @brief 创建计数信号量
 * @details 根据sem_buffer参数自动选择创建方式：
 *          - sem_buffer=NULL: 从堆中动态分配（需要FLYOS_USE_HEAP=1）
 *          - sem_buffer!=NULL: 使用用户提供的静态缓冲区
 */
flyos_semaphore_t* flyos_semaphore_create(
	flyos_semaphore_t *sem_buffer,
	flyos_u32_t init_count,
	flyos_u32_t max_count)
{
	flyos_semaphore_t *sem;
	flyos_u8_t flags = 0;

	/* 参数验证 */
	if (unlikely(max_count == 0 || init_count > max_count)) {
		return NULL;
	}

	if (sem_buffer != NULL) {
		flags = FLYOS_IPC_FLAG_STATIC;
	}

	if (sem_buffer == NULL) {
		/* 动态创建：从堆分配内存 */
#if FLYOS_USE_HEAP
		sem = (flyos_semaphore_t *)flyos_mem_alloc(sizeof(flyos_semaphore_t));
		if (sem == NULL) {
			return NULL;  /* 内存分配失败 */
		}
#else
		/* FLYOS_USE_HEAP=0时不支持动态创建 */
		return NULL;
#endif
	} else {
		/* 静态创建：使用用户提供的缓冲区 */
		sem = sem_buffer;
	}

	/* 初始化IPC基类 */
	ipc_base_init(&sem->base, FLYOS_IPC_TYPE_SEMAPHORE, flags, "sem");
	wait_queue_init(&sem->wait_queue);

	/* 初始化信号量特有字段 */
	sem->count = init_count;
	sem->max_count = max_count;

	return sem;
}

/**
 * @brief 删除信号量
 * @details 删除信号量并唤醒所有等待的任务。
 *          自动识别静态/动态创建的对象并正确处理内存。
 */
flyos_err_t flyos_semaphore_delete(flyos_semaphore_t *sem)
{
	flyos_base_t level;
	IPC_ENTER(sem, FLYOS_INVALID);

	/* 唤醒所有等待任务，返回错误码 */
	while (!wait_queue_is_empty(&sem->wait_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&sem->wait_queue);
		if (task != NULL) {
			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_ERROR;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);
		}
	}

	IPC_EXIT();

	/* 根据flags判断是否需要释放内存 */
#if FLYOS_USE_HEAP
	if (!(sem->base.flags & FLYOS_IPC_FLAG_STATIC)) {
		/* 动态创建对象需要释放内存 */
		flyos_mem_free(sem);
	}
	/* 静态创建的对象，不释放内存，由用户管理 */
#endif

	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief   等待信号量
 * @details 尝试获取信号量。如果可用则递减计数并返回成功；
 *          如果不可用，根据timeout参数决定是否阻塞等待。
 *
 * @param[in,out] sem       信号量句柄
 * @param[in] timeout       超时时间（tick）
 * @param[in] from_isr      true=从ISR调用
 *
 * @return  FLYOS_OK=成功，FLYOS_TIMEOUT=超时，FLYOS_ERROR=ISR阻塞尝试
 *
 * @note    任务上下文或ISR（from_isr=true时）
 */
static flyos_err_t sem_wait_common(flyos_semaphore_t *sem, flyos_tick_t timeout, bool from_isr)
{
	if (from_isr && timeout != FLYOS_NO_WAIT) {
		return FLYOS_ERROR;
	}

	flyos_base_t level;
	if (from_isr) {
		IPC_ENTER_ISR(sem, FLYOS_INVALID);
	} else {
		IPC_ENTER(sem, FLYOS_INVALID);
	}

	if (sem->count > 0) {
		/* 信号量可用 */
		sem->count--;
		if (from_isr) {
			IPC_EXIT_ISR();
		} else {
			IPC_EXIT();
		}
		return FLYOS_OK;
	}

	/* 信号量不可用 */
	if (timeout == FLYOS_NO_WAIT) {
		if (from_isr) {
			IPC_EXIT_ISR();
		} else {
			IPC_EXIT();
		}
		return FLYOS_TIMEOUT;
	}

	/* 阻塞当前任务 */
	ipc_block_current_task(&sem->wait_queue, sem, timeout);

	IPC_EXIT();
	flyos_scheduler();

	return flyos_task_get_current()->error_code;
}

/**
 * @brief   发送信号量
 * @details 释放信号量。如果有任务等待则唤醒最高优先级任务，否则递增计数。
 *
 * @param[in,out] sem       信号量句柄
 * @param[in] from_isr      true=从ISR调用
 *
 * @return  FLYOS_OK=成功，FLYOS_FULL=计数已达最大值
 *
 * @note    任务上下文或ISR（from_isr=true时）
 */
static flyos_err_t sem_signal_common(flyos_semaphore_t *sem, bool from_isr)
{
	flyos_base_t level;
	if (from_isr) {
		IPC_ENTER_ISR(sem, FLYOS_INVALID);
	} else {
		IPC_ENTER(sem, FLYOS_INVALID);
	}

	if (!wait_queue_is_empty(&sem->wait_queue)) {
		/* 有任务等待，直接唤醒最高优先级任务 */
		flyos_tcb_t *task = ipc_wake_highest_task(&sem->wait_queue);

		if (task != NULL) {
			if (from_isr) {
				IPC_EXIT_ISR();
				flyos_port_yield(); /* ISR触发上下文切换 */
			} else {
				IPC_EXIT();
				flyos_scheduler();
			}
			return FLYOS_OK;
		}
	}

	/* 没有等待任务，增加计数 */
	if (sem->count < sem->max_count) {
		sem->count++;
		if (from_isr) {
			IPC_EXIT_ISR();
		} else {
			IPC_EXIT();
		}
		return FLYOS_OK;
	}

	/* 信号量已满 */
	if (from_isr) {
		IPC_EXIT_ISR();
	} else {
		IPC_EXIT();
	}
	return FLYOS_FULL;
}

/**
 * @brief 等待信号量
 */
flyos_err_t flyos_semaphore_wait(flyos_semaphore_t *sem, flyos_tick_t timeout)
{
	return sem_wait_common(sem, timeout, false);
}

/**
 * @brief 发送信号量
 */
flyos_err_t flyos_semaphore_signal(flyos_semaphore_t *sem)
{
	return sem_signal_common(sem, false);
}

/**
 * @brief 从ISR发送信号量
 */
flyos_err_t flyos_semaphore_signal_from_isr(flyos_semaphore_t *sem)
{
	return sem_signal_common(sem, true);
}

/**
 * @brief 获取信号量计数
 */
flyos_u32_t flyos_semaphore_get_count(flyos_semaphore_t *sem)
{
	if (sem == NULL) {
		return 0;
	}
	return sem->count;
}

#endif /* FLYOS_USE_SEMAPHORE */

/*===========================================================================*/
/* 互斥锁实现，支持链式优先级继承                                               */
/*===========================================================================*/

#if FLYOS_USE_MUTEX

/**
 * 互斥锁使用场景说明：
 *
 * 1. 临界区保护（Critical Section）
 *    - 场景：保护共享资源，防止多个任务同时访问
 *    - 示例：
 *      flyos_mutex_t *uart_lock = flyos_mutex_create();
 *      flyos_mutex_lock(uart_lock, FLYOS_WAIT_FOREVER);
 *      uart_send_data("Hello");  // 临界区代码
 *      flyos_mutex_unlock(uart_lock);
 *
 * 2. 共享数据结构保护（Shared Data Protection）
 *    - 场景：保护链表、队列等复杂数据结构
 *    - 示例：
 *      flyos_mutex_t *list_lock = flyos_mutex_create();
 *      flyos_mutex_lock(list_lock, FLYOS_WAIT_FOREVER);
 *      list_add(&shared_list, item);  // 修改共享链表
 *      flyos_mutex_unlock(list_lock);
 *
 * 3. 硬件资源互斥访问（Hardware Resource Access）
 *    - 场景：SPI、I2C等硬件外设的独占访问
 *    - 示例：
 *      flyos_mutex_t *spi_lock = flyos_mutex_create();
 *      flyos_mutex_lock(spi_lock, FLYOS_WAIT_FOREVER);
 *      spi_transfer(tx_buf, rx_buf, len);  // SPI传输
 *      flyos_mutex_unlock(spi_lock);
 *
 * 4. 递归锁定（Recursive Locking）
 *    - 场景：同一任务需要多次锁定同一个锁
 *    - 示例：
 *      flyos_mutex_t *recursive_lock = flyos_mutex_create();
 *      void func_a() {
 *          flyos_mutex_lock(recursive_lock, FLYOS_WAIT_FOREVER);
 *          func_b();  // 调用func_b，会再次锁定
 *          flyos_mutex_unlock(recursive_lock);
 *      }
 *      void func_b() {
 *          flyos_mutex_lock(recursive_lock, FLYOS_WAIT_FOREVER);  // 递归锁定
 *          // ... 临界区代码 ...
 *          flyos_mutex_unlock(recursive_lock);
 *      }
 *
 * 5. 避免优先级反转（Priority Inversion Prevention）
 *    - 场景：低优先级任务持有锁，高优先级任务等待时自动提升低优先级任务
 *    - 原理：链式优先级继承协议（最大深度8层）
 *    - 示例：
 *      // 低优先级任务L持有mutex
 *      // 高优先级任务H尝试获取mutex
 *      // → 系统自动将L的优先级提升到H的优先级
 *      // → L完成后释放mutex，优先级恢复到原始值
 *
 * ✅ 优势特性：
 *    - 支持优先级继承（避免优先级反转）
 *    - 支持递归锁定（同一任务可多次锁定）
 *    - 支持链式优先级继承（A→B→C最多8层）
 *    - 死锁检测（检测循环依赖）
 *    - 只有owner可以解锁（权限检查）
 *
 * ⚠️ 注意事项：
 *    - 必须在任务上下文中调用，不能在ISR中使用
 *    - 锁定和解锁必须成对出现
 *    - 只有锁的owner才能解锁
 *    - 避免长时间持有锁，影响系统实时性
 */

/**
 * @brief   链式优先级继承
 * @details 实现链式优先级继承协议，处理A→B→C的复杂场景。
 *          当高优先级任务H等待低优先级任务L持有的锁时，L的优先级临时提升到H，
 *          避免优先级反转。如果L又被阻塞在任务M持有的锁上，继续传播优先级到M。
 *
 *          安全特性：
 *          - 类型检查：确保继承链中的对象都是mutex
 *          - 死锁检测：检查循环依赖（A→B→C→A）
 *          - 深度限制：最大继承深度为FLYOS_MAX_INHERIT_DEPTH
 *
 * @param[in,out] mutex         互斥锁
 * @param[in] new_priority      新优先级
 *
 * @return  FLYOS_OK=成功，FLYOS_EDEADLK=死锁检测
 *
 * @note    持有临界区锁
 */
static flyos_err_t mutex_priority_inherit_chain(flyos_mutex_t *mutex, flyos_u8_t new_priority)
{
#if FLYOS_PRIORITY_INHERIT_CHAIN
	flyos_u32_t depth = 0;
	flyos_tcb_t *visited[FLYOS_MAX_INHERIT_DEPTH] = {NULL};
	flyos_mutex_t *current_mutex = mutex;

	while (current_mutex != NULL && depth < FLYOS_MAX_INHERIT_DEPTH) {
		/* 类型安全检查，确保是有效的mutex */
		if (unlikely(!ipc_object_valid(&current_mutex->base, FLYOS_IPC_TYPE_MUTEX))) {
			FLYOS_ASSERT(0);
			return FLYOS_INVALID;
		}

		flyos_tcb_t *owner = current_mutex->owner;

		if (owner == NULL || new_priority >= owner->priority) {
			break;      /* 无需继承或优先级不够高 */
		}

		/* 循环检测，检查owner是否已被访问过 */
		for (flyos_u32_t i = 0; i < depth; i++) {
			if (visited[i] == owner) {
				/* 检测到循环依赖：A→B→C→A */
				FLYOS_ASSERT(0);  /* 触发断言，在调试时报告死锁 */
				return FLYOS_EDEADLK;  /* 返回死锁错误码 */
			}
		}

		/* 记录当前访问的任务 */
		visited[depth] = owner;

		/* 提升owner优先级 */
		if (owner->state == FLYOS_TASK_READY) {
			flyos_remove_ready_list(owner);
			owner->priority = new_priority;
			flyos_insert_ready_list(owner);
		} else {
			/* 任务被阻塞，只更新优先级 */
			owner->priority = new_priority;
		}

		/* 更新mutex的继承优先级 */
		current_mutex->inherited_priority = new_priority;

		/* 追踪链：owner是否被阻塞在其他mutex */
		if (owner->state == FLYOS_TASK_BLOCK &&
			owner->block_reason == FLYOS_BLOCK_IPC &&
			owner->block_object != NULL) {

			/* 类型安全检查：确保block_object是mutex */
			flyos_ipc_base_t *base = (flyos_ipc_base_t *)owner->block_object;

			/* 检查魔术数字和类型 */
			if (base->magic == FLYOS_IPC_MAGIC &&
				base->type == FLYOS_IPC_TYPE_MUTEX) {
				current_mutex = (flyos_mutex_t *)owner->block_object;
			} else {
				/* 阻塞在非mutex对象（信号量/队列/事件），终止继承链 */
				break;
			}
		} else {
			break;
		}

		depth++;
	}

	/* 检测继承链过长 */
	if (unlikely(depth >= FLYOS_MAX_INHERIT_DEPTH)) {
		FLYOS_ASSERT(0);
		return FLYOS_ERROR;
	}

	return FLYOS_OK;
#else
	/* 单层优先级继承 */
	if (mutex->owner != NULL && new_priority < mutex->owner->priority) {
		flyos_remove_ready_list(mutex->owner);
		mutex->owner->priority = new_priority;
		flyos_insert_ready_list(mutex->owner);
	}
	return FLYOS_OK;
#endif
}

/**
 * @brief   恢复互斥锁持有者的原始优先级
 * @details 在释放互斥锁时调用，将owner的优先级恢复到original_priority。
 *          如果优先级被提升过，需要从就绪队列移除并重新插入。
 *
 * @param[in,out] mutex  互斥锁句柄
 *
 * @note    临界区内调用
 */
static void mutex_priority_restore(flyos_mutex_t *mutex)
{
	flyos_tcb_t *owner = mutex->owner;

	if (owner != NULL && owner->priority != mutex->original_priority) {
		flyos_remove_ready_list(owner);
		owner->priority = mutex->original_priority;
		flyos_insert_ready_list(owner);
	}
}

/**
 * @brief 创建互斥锁
 * @details 根据mutex_buffer参数自动选择创建方式：
 *          - mutex_buffer=NULL: 从堆中动态分配（需要FLYOS_USE_HEAP=1）
 *          - mutex_buffer!=NULL: 使用用户提供的静态缓冲区
 */
flyos_mutex_t* flyos_mutex_create(flyos_mutex_t *mutex_buffer)
{
	flyos_mutex_t *mutex;
	flyos_u8_t flags = 0;

	if (mutex_buffer != NULL) {
		flags = FLYOS_IPC_FLAG_STATIC;
	}

	if (mutex_buffer == NULL) {
		/* 动态创建，从堆分配内存 */
#if FLYOS_USE_HEAP
		mutex = (flyos_mutex_t *)flyos_mem_alloc(sizeof(flyos_mutex_t));
		if (mutex == NULL) {
			return NULL;  /* 内存分配失败 */
		}
#else
		/* FLYOS_USE_HEAP=0时不支持动态创建 */
		return NULL;
#endif
	} else {
		/* 静态创建，使用用户提供的缓冲区 */
		mutex = mutex_buffer;
	}

	/* 初始化IPC基类 */
	ipc_base_init(&mutex->base, FLYOS_IPC_TYPE_MUTEX, flags, "mutex");
	wait_queue_init(&mutex->wait_queue);

	/* 初始化互斥锁特有字段 */
	mutex->owner = NULL;
	mutex->original_priority = 0;
	mutex->inherited_priority = 0;
	mutex->nest_count = 0;

	return mutex;
}

/**
 * @brief 删除互斥锁
 * @details 删除互斥锁并唤醒所有等待的任务。
 *          自动识别静态/动态创建的对象并正确处理内存。
 */
flyos_err_t flyos_mutex_delete(flyos_mutex_t *mutex)
{
	flyos_base_t level;
	IPC_ENTER(mutex, FLYOS_INVALID);

	/* 恢复owner优先级 */
	mutex_priority_restore(mutex);

	/* 唤醒所有等待任务 */
	while (!wait_queue_is_empty(&mutex->wait_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&mutex->wait_queue);
		if (task != NULL) {
			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_ERROR;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);
		}
	}

	IPC_EXIT();

	/* 根据flags判断是否需要释放内存 */
#if FLYOS_USE_HEAP
	if (!(mutex->base.flags & FLYOS_IPC_FLAG_STATIC)) {
		/* 动态创建的对象：释放内存 */
		flyos_mem_free(mutex);
	}
	/* 静态创建的对象：不释放内存，由用户管理 */
#endif

	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief 锁定互斥锁
 */
flyos_err_t flyos_mutex_lock(flyos_mutex_t *mutex, flyos_tick_t timeout)
{
	flyos_base_t level;
	IPC_ENTER(mutex, FLYOS_INVALID);

	flyos_tcb_t *current = flyos_task_get_current();

	if (mutex->owner == NULL) {
		/* 互斥锁空闲 */
		mutex->owner = current;
		mutex->nest_count = 1;
		mutex->original_priority = current->priority;
		mutex->inherited_priority = current->priority;
		IPC_EXIT();
		return FLYOS_OK;
	}

	if (mutex->owner == current) {
		/* 递归锁定 */
#if FLYOS_USE_RECURSIVE_MUTEX
		mutex->nest_count++;
		IPC_EXIT();
		return FLYOS_OK;
#else
		IPC_EXIT();
		return FLYOS_ERROR;
#endif
	}

	/* 被其他任务持有 */
	if (timeout == FLYOS_NO_WAIT) {
		IPC_EXIT();
		return FLYOS_BUSY;
	}

	/* 优先级继承 */
#if FLYOS_USE_PRIORITY_INHERIT
	if (current->priority < mutex->owner->priority) {
		mutex_priority_inherit_chain(mutex, current->priority);
	}
#endif

	/* 阻塞当前任务 */
	ipc_block_current_task(&mutex->wait_queue, mutex, timeout);

	IPC_EXIT();
	flyos_scheduler();

	return current->error_code;
}

/**
 * @brief 解锁互斥锁
 */
flyos_err_t flyos_mutex_unlock(flyos_mutex_t *mutex)
{
	flyos_base_t level;
	IPC_ENTER(mutex, FLYOS_INVALID);

	flyos_tcb_t *current = flyos_task_get_current();

	if (mutex->owner != current) {
		IPC_EXIT();
		return FLYOS_PERMISSION;
	}

#if FLYOS_USE_RECURSIVE_MUTEX
	if (--mutex->nest_count > 0) {
		IPC_EXIT();
		return FLYOS_OK;
	}
#endif

	/* 恢复优先级 */
#if FLYOS_USE_PRIORITY_INHERIT
	mutex_priority_restore(mutex);
#endif

	mutex->owner = NULL;

	/* 唤醒下一个等待者 */
	if (!wait_queue_is_empty(&mutex->wait_queue)) {
		flyos_tcb_t *task = ipc_wake_highest_task(&mutex->wait_queue);

		if (task != NULL) {
			/* 新owner接管mutex */
			mutex->owner = task;
			mutex->nest_count = 1;
			mutex->original_priority = task->priority;
			mutex->inherited_priority = task->priority;

			IPC_EXIT();
			flyos_scheduler();
			return FLYOS_OK;
		}
	}

	IPC_EXIT();
	return FLYOS_OK;
}

/**
 * @brief 获取互斥锁owner
 */
flyos_tcb_t* flyos_mutex_get_owner(flyos_mutex_t *mutex)
{
	if (mutex == NULL) {
		return NULL;
	}
	return mutex->owner;
}

#endif /* FLYOS_USE_MUTEX */

/*===========================================================================*/
/* 消息队列实现                                                              */
/*===========================================================================*/

#if FLYOS_USE_QUEUE

/**
 * 消息队列使用场景说明：
 *
 * 1. 任务间数据传递（Inter-Task Communication）
 *    - 场景：在任务之间传递结构化数据
 *    - 示例：
 *      typedef struct {
 *          uint8_t sensor_id;
 *          float value;
 *      } sensor_data_t;
 *
 *      flyos_queue_t *sensor_queue = flyos_queue_create(sizeof(sensor_data_t), 10);
 *
 *      // 发送任务：
 *      sensor_data_t data = {.sensor_id = 1, .value = 25.5};
 *      flyos_queue_send(sensor_queue, &data, FLYOS_WAIT_FOREVER);
 *
 *      // 接收任务：
 *      sensor_data_t received;
 *      flyos_queue_receive(sensor_queue, &received, FLYOS_WAIT_FOREVER);
 *
 * 2. 生产者-消费者模式（Producer-Consumer）
 *    - 场景：多个生产者生成数据，多个消费者处理数据
 *    - 示例：
 *      flyos_queue_t *work_queue = flyos_queue_create(sizeof(work_item_t), 20);
 *
 *      // 多个生产者任务：
 *      work_item_t work = create_work();
 *      flyos_queue_send(work_queue, &work, 1000);  // 1秒超时
 *
 *      // 多个消费者任务：
 *      work_item_t work;
 *      if (flyos_queue_receive(work_queue, &work, FLYOS_WAIT_FOREVER) == FLYOS_OK) {
 *          process_work(&work);
 *      }
 *
 * 3. 中断到任务的数据传递（ISR to Task）
 *    - 场景：中断服务程序收集数据，任务处理数据
 *    - 示例：
 *      flyos_queue_t *uart_rx_queue = flyos_queue_create(sizeof(uint8_t), 64);
 *
 *      // 中断处理：
 *      void UART_IRQHandler(void) {
 *          uint8_t byte = UART_ReadByte();
 *          flyos_queue_send_from_isr(uart_rx_queue, &byte);
 *      }
 *
 *      // 任务处理：
 *      uint8_t byte;
 *      flyos_queue_receive(uart_rx_queue, &byte, FLYOS_WAIT_FOREVER);
 *      process_uart_byte(byte);
 *
 * 4. 命令队列（Command Queue）
 *    - 场景：将命令排队等待顺序执行
 *    - 示例：
 *      typedef enum { CMD_START, CMD_STOP, CMD_RESET } command_t;
 *      flyos_queue_t *cmd_queue = flyos_queue_create(sizeof(command_t), 5);
 *
 *      // UI任务发送命令：
 *      command_t cmd = CMD_START;
 *      flyos_queue_send(cmd_queue, &cmd, FLYOS_NO_WAIT);
 *
 *      // 执行任务处理命令：
 *      command_t cmd;
 *      flyos_queue_receive(cmd_queue, &cmd, FLYOS_WAIT_FOREVER);
 *      execute_command(cmd);
 *
 * 5. 事件日志（Event Logging）
 *    - 场景：收集系统事件，统一处理或存储
 *    - 示例：
 *      typedef struct {
 *          uint32_t timestamp;
 *          uint8_t level;
 *          char message[64];
 *      } log_entry_t;
 *
 *      flyos_queue_t *log_queue = flyos_queue_create(sizeof(log_entry_t), 50);
 *
 *      // 任意任务记录日志：
 *      log_entry_t log = {
 *          .timestamp = flyos_kernel_get_tick_count(),
 *          .level = LOG_INFO,
 *          .message = "System started"
 *      };
 *      flyos_queue_send(log_queue, &log, FLYOS_NO_WAIT);  // 不阻塞
 *
 *      // 日志任务写入存储：
 *      log_entry_t log;
 *      flyos_queue_receive(log_queue, &log, FLYOS_WAIT_FOREVER);
 *      write_log_to_flash(&log);
 *
 * ✅ 特性：
 *    - 数据按值复制传递（memcpy）
 *    - FIFO顺序（先进先出）
 *    - 支持阻塞发送和接收
 *    - 支持从ISR发送（非阻塞）
 *    - 自动唤醒等待的接收者/发送者
 *
 * ⚠️ 注意事项：
 *    - 数据会被复制，传递大结构体时考虑使用邮箱传递指针
 *    - 队列满时发送会阻塞（除非timeout=0）
 *    - 队列空时接收会阻塞（除非timeout=0）
 *    - ISR中只能使用flyos_queue_send_from_isr()，不能阻塞
 */

/**
 * @brief 创建消息队列
 * @details 根据queue_buffer和item_buffer参数自动选择创建方式：
 *          - 两者都为NULL: 从堆中动态分配（需要FLYOS_USE_HEAP=1）
 *          - 两者都非NULL: 使用用户提供的静态缓冲区
 *          注意：不支持混合模式（一个静态一个动态）
 */
flyos_queue_t* flyos_queue_create(
	flyos_queue_t *queue_buffer,
	void *item_buffer,
	flyos_u32_t item_size,
	flyos_u32_t max_items)
{
	flyos_queue_t *queue;
	flyos_u8_t flags = 0;

	/* 参数验证 */
	if (unlikely(item_size == 0 || max_items == 0)) {
		return NULL;
	}

	if (queue_buffer != NULL && item_buffer != NULL) {
		flags = FLYOS_IPC_FLAG_STATIC | FLYOS_IPC_FLAG_QUEUE_BUFFER_STATIC;
	} else if (queue_buffer != NULL || item_buffer != NULL) {
		/* 不支持混合模式 */
		return NULL;
	}

	if (queue_buffer == NULL && item_buffer == NULL) {
		/* 动态创建：从堆分配控制块和数据缓冲区 */
#if FLYOS_USE_HEAP
		queue = (flyos_queue_t *)flyos_mem_alloc(sizeof(flyos_queue_t));
		if (queue == NULL) {
			return NULL;  /* 控制块分配失败 */
		}

		queue->buffer = flyos_mem_alloc(item_size * max_items);
		if (queue->buffer == NULL) {
			flyos_mem_free(queue);  /* 释放控制块 */
			return NULL;  /* 缓冲区分配失败 */
		}
#else
		/* FLYOS_USE_HEAP=0时不支持动态创建 */
		return NULL;
#endif
	} else {
		/* 静态创建：使用用户提供的缓冲区 */
		queue = queue_buffer;
		queue->buffer = item_buffer;
	}

	/* 初始化IPC基类 */
	ipc_base_init(&queue->base, FLYOS_IPC_TYPE_QUEUE, flags, "queue");

	/* 初始化发送/接收等待队列 */
	wait_queue_init(&queue->send_queue);
	wait_queue_init(&queue->recv_queue);

	/* 初始化队列字段 */
	queue->item_size = item_size;
	queue->max_items = max_items;
	queue->item_count = 0;
	queue->read_index = 0;
	queue->write_index = 0;

	return queue;
}

/**
 * @brief 删除消息队列
 * @details 删除消息队列并唤醒所有等待的任务。
 *          自动识别静态/动态创建的对象并正确处理内存。
 */
flyos_err_t flyos_queue_delete(flyos_queue_t *queue)
{
	flyos_base_t level;
	IPC_ENTER(queue, FLYOS_INVALID);

	/* 唤醒所有发送等待者 */
	while (!wait_queue_is_empty(&queue->send_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&queue->send_queue);
		if (task != NULL) {
			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_ERROR;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);
		}
	}

	/* 唤醒所有接收等待者 */
	while (!wait_queue_is_empty(&queue->recv_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&queue->recv_queue);
		if (task != NULL) {
			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_ERROR;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);
		}
	}

	IPC_EXIT();

	/* 根据flags判断是否需要释放内存 */
#if FLYOS_USE_HEAP
	/* 释放数据缓冲区 */
	if (!(queue->base.flags & FLYOS_IPC_FLAG_QUEUE_BUFFER_STATIC)) {
		flyos_mem_free(queue->buffer);
	}

	/* 释放控制块 */
	if (!(queue->base.flags & FLYOS_IPC_FLAG_STATIC)) {
		flyos_mem_free(queue);
	}
	/* 静态创建的对象：不释放内存，由用户管理 */
#endif

	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief   发送数据到队列
 * @details 尝试将数据复制到队列。如果有接收者等待，直接传递给接收者；
 *          否则将数据复制到队列缓冲区。队列满时根据timeout决定是否阻塞。
 *
 * @param[in,out] queue     队列句柄
 * @param[in] item          指向要发送数据的指针
 * @param[in] timeout       超时时间（tick）
 * @param[in] from_isr      true=从ISR调用
 *
 * @return  FLYOS_OK=成功，FLYOS_FULL=队列满，FLYOS_TIMEOUT=超时，
 *          FLYOS_INVALID=无效参数，FLYOS_ERROR=ISR阻塞尝试
 *
 * @note    任务上下文或ISR（from_isr=true时）
 */
static flyos_err_t queue_send_common(flyos_queue_t *queue, const void *item,
									 flyos_tick_t timeout, bool from_isr)
{
	if (item == NULL) {
		return FLYOS_INVALID;
	}

	if (from_isr && timeout != FLYOS_NO_WAIT) {
		return FLYOS_ERROR;
	}

	flyos_base_t level;
	if (from_isr) {
		IPC_ENTER_ISR(queue, FLYOS_INVALID);
	} else {
		IPC_ENTER(queue, FLYOS_INVALID);
	}

	/* 直接传递给等待的接收者 */
	if (!wait_queue_is_empty(&queue->recv_queue) && queue->item_count == 0) {
		flyos_tcb_t *task = wait_queue_remove_highest(&queue->recv_queue);
		if (task != NULL) {
			memcpy(task->user_data, item, queue->item_size);

			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_OK;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);

			if (from_isr) {
				IPC_EXIT_ISR();
				flyos_port_yield();
			} else {
				IPC_EXIT();
				flyos_scheduler();
			}
			return FLYOS_OK;
		}
	}

	/* 队列已满 */
	if (queue->item_count >= queue->max_items) {
		if (timeout == FLYOS_NO_WAIT) {
			if (from_isr) {
				IPC_EXIT_ISR();
			} else {
				IPC_EXIT();
			}
			return FLYOS_FULL;
		}

		/* 阻塞发送者 */
		flyos_tcb_t *current = flyos_task_get_current();
		flyos_remove_ready_list(current);
		current->state = FLYOS_TASK_BLOCK;
		current->block_reason = FLYOS_BLOCK_IPC;
		current->block_object = queue;
		current->user_data = (void *)item; /* 保存发送数据指针 */
		current->error_code = FLYOS_OK;

		wait_queue_insert(&queue->send_queue, current);

		if (timeout != FLYOS_WAIT_FOREVER) {
			flyos_delta_queue_insert(current, timeout);
		}

		IPC_EXIT();
		flyos_scheduler();
		return current->error_code;
	}

	/* 添加到队列 */
	flyos_u8_t *dest = (flyos_u8_t *)queue->buffer + (queue->write_index * queue->item_size);
	memcpy(dest, item, queue->item_size);

	queue->write_index = (queue->write_index + 1) % queue->max_items;
	queue->item_count++;

	if (from_isr) {
		IPC_EXIT_ISR();
	} else {
		IPC_EXIT();
	}
	return FLYOS_OK;
}

/**
 * @brief 发送到队列
 */
flyos_err_t flyos_queue_send(flyos_queue_t *queue, const void *item, flyos_tick_t timeout)
{
	return queue_send_common(queue, item, timeout, false);
}

/**
 * @brief 从ISR发送到队列
 */
flyos_err_t flyos_queue_send_from_isr(flyos_queue_t *queue, const void *item)
{
	return queue_send_common(queue, item, FLYOS_NO_WAIT, true);
}

/**
 * @brief 从队列接收
 */
flyos_err_t flyos_queue_receive(flyos_queue_t *queue, void *item, flyos_tick_t timeout)
{
	flyos_base_t level;
	IPC_ENTER(queue, FLYOS_INVALID);

	if (item == NULL) {
		IPC_EXIT();
		return FLYOS_INVALID;
	}

	/* 队列为空 */
	if (queue->item_count == 0) {
		if (timeout == FLYOS_NO_WAIT) {
			IPC_EXIT();
			return FLYOS_EMPTY;
		}

		/* 阻塞接收者 */
		flyos_tcb_t *current = flyos_task_get_current();
		flyos_remove_ready_list(current);
		current->state = FLYOS_TASK_BLOCK;
		current->block_reason = FLYOS_BLOCK_IPC;
		current->block_object = queue;
		current->user_data = item; /* 保存接收缓冲区 */
		current->error_code = FLYOS_OK;

		wait_queue_insert(&queue->recv_queue, current);

		if (timeout != FLYOS_WAIT_FOREVER) {
			flyos_delta_queue_insert(current, timeout);
		}

		IPC_EXIT();
		flyos_scheduler();
		return current->error_code;
	}

	/* 从队列读取 */
	flyos_u8_t *src = (flyos_u8_t *)queue->buffer + (queue->read_index * queue->item_size);
	memcpy(item, src, queue->item_size);

	queue->read_index = (queue->read_index + 1) % queue->max_items;
	queue->item_count--;

	/* 唤醒等待的发送者 */
	if (!wait_queue_is_empty(&queue->send_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&queue->send_queue);
		if (task != NULL) {
			/* 将发送者的数据放入队列 */
			flyos_u8_t *dest = (flyos_u8_t *)queue->buffer + (queue->write_index * queue->item_size);
			memcpy(dest, task->user_data, queue->item_size);

			queue->write_index = (queue->write_index + 1) % queue->max_items;
			queue->item_count++;

			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_OK;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);

			IPC_EXIT();
			flyos_scheduler();
			return FLYOS_OK;
		}
	}

	IPC_EXIT();
	return FLYOS_OK;
}

/**
 * @brief 获取队列项数
 */
flyos_u32_t flyos_queue_get_count(flyos_queue_t *queue)
{
	if (queue == NULL) {
		return 0;
	}
	return queue->item_count;
}

/**
 * @brief 获取队列空闲空间
 */
flyos_u32_t flyos_queue_get_space(flyos_queue_t *queue)
{
	if (queue == NULL) {
		return 0;
	}
	return queue->max_items - queue->item_count;
}

#endif /* FLYOS_USE_QUEUE */

/*===========================================================================*/
/* 事件标志实现                                                              */
/*===========================================================================*/

#if FLYOS_USE_EVENT

/**
 * 事件标志使用场景说明：
 *
 * 1. 多事件等待（Multi-Event Synchronization）
 *    - 场景：任务需要等待多个事件发生才能继续执行
 *    - 示例：
 *      // 等待传感器数据就绪 + 控制指令到达
 *      flyos_event_t *flight_events = flyos_event_create();
 *      #define EVENT_SENSOR_READY  (1 << 0)
 *      #define EVENT_CMD_RECEIVED  (1 << 1)
 *      #define EVENT_GPS_LOCK      (1 << 2)
 *
 *      // 传感器任务
 *      void sensor_task(void) {
 *          read_sensor_data();
 *          flyos_event_set(flight_events, EVENT_SENSOR_READY);
 *      }
 *
 *      // 控制任务等待多个事件
 *      void control_task(void) {
 *          flyos_u32_t result;
 *          // 等待传感器和GPS都就绪（等待所有标志）
 *          flyos_event_wait(flight_events,
 *                          EVENT_SENSOR_READY | EVENT_GPS_LOCK,
 *                          FLYOS_EVENT_WAIT_ALL,
 *                          1000, &result);
 *          perform_control();
 *      }
 *
 * 2. 任务同步点（Task Synchronization Barrier）
 *    - 场景：多个任务需要在某个点同步执行
 *    - 示例：
 *      flyos_event_t *sync_barrier = flyos_event_create();
 *      #define TASK1_READY (1 << 0)
 *      #define TASK2_READY (1 << 1)
 *      #define TASK3_READY (1 << 2)
 *      #define ALL_READY   (TASK1_READY | TASK2_READY | TASK3_READY)
 *
 *      void task1(void) {
 *          // 完成初始化工作
 *          init_task1();
 *          // 通知已就绪
 *          flyos_event_set(sync_barrier, TASK1_READY);
 *          // 等待所有任务就绪
 *          flyos_event_wait(sync_barrier, ALL_READY,
 *                          FLYOS_EVENT_WAIT_ALL,
 *                          FLYOS_WAIT_FOREVER, NULL);
 *          // 同步执行
 *          start_task1();
 *      }
 *
 * 3. 状态机控制（State Machine Events）
 *    - 场景：基于事件驱动的状态机实现
 *    - 示例：
 *      flyos_event_t *motor_events = flyos_event_create();
 *      #define EVENT_START     (1 << 0)
 *      #define EVENT_STOP      (1 << 1)
 *      #define EVENT_EMERGENCY (1 << 2)
 *
 *      void motor_control_task(void) {
 *          flyos_u32_t events;
 *          while (1) {
 *              // 等待任意控制事件
 *              flyos_event_wait(motor_events, 0xFFFFFFFF,
 *                              FLYOS_EVENT_WAIT_ANY | FLYOS_EVENT_CLEAR_ON_EXIT,
 *                              FLYOS_WAIT_FOREVER, &events);
 *
 *              if (events & EVENT_EMERGENCY) {
 *                  emergency_stop();
 *              } else if (events & EVENT_START) {
 *                  start_motor();
 *              } else if (events & EVENT_STOP) {
 *                  stop_motor();
 *              }
 *          }
 *      }
 *
 * 4. 中断事件通知（ISR Event Notification）
 *    - 场景：中断通知任务某些事件发生
 *    - 示例：
 *      flyos_event_t *isr_events = flyos_event_create();
 *      #define EVENT_DMA_DONE  (1 << 0)
 *      #define EVENT_UART_RX   (1 << 1)
 *      #define EVENT_TIMER     (1 << 2)
 *
 *      // DMA中断服务程序
 *      void DMA_IRQHandler(void) {
 *          if (DMA_GetFlagStatus(DMA_FLAG_TC)) {
 *              flyos_event_set(isr_events, EVENT_DMA_DONE);
 *              DMA_ClearFlag(DMA_FLAG_TC);
 *          }
 *      }
 *
 *      // 数据处理任务
 *      void process_task(void) {
 *          flyos_u32_t events;
 *          while (1) {
 *              // 等待DMA完成或UART接收
 *              flyos_event_wait(isr_events,
 *                              EVENT_DMA_DONE | EVENT_UART_RX,
 *                              FLYOS_EVENT_WAIT_ANY | FLYOS_EVENT_CLEAR_ON_EXIT,
 *                              1000, &events);
 *
 *              if (events & EVENT_DMA_DONE) {
 *                  process_dma_data();
 *              }
 *              if (events & EVENT_UART_RX) {
 *                  process_uart_data();
 *              }
 *          }
 *      }
 *
 * 5. 资源就绪通知（Resource Ready Notification）
 *    - 场景：等待多个资源同时就绪后再处理
 *    - 示例：
 *      flyos_event_t *resource_events = flyos_event_create();
 *      #define EVENT_BUFFER_READY   (1 << 0)
 *      #define EVENT_DEVICE_READY   (1 << 1)
 *      #define EVENT_PERMISSION_OK  (1 << 2)
 *
 *      void writer_task(void) {
 *          // 等待所有条件满足（缓冲区就绪 + 设备就绪 + 权限通过）
 *          flyos_event_wait(resource_events,
 *                          EVENT_BUFFER_READY | EVENT_DEVICE_READY | EVENT_PERMISSION_OK,
 *                          FLYOS_EVENT_WAIT_ALL | FLYOS_EVENT_CLEAR_ON_EXIT,
 *                          5000, NULL);
 *
 *          // 所有条件满足，开始写入
 *          write_to_device();
 *      }
 *
 * ✅ 事件标志特性：
 *    - 支持32个独立事件位
 *    - 支持等待任意事件（OR）或所有事件（AND）
 *    - 支持自动清除或手动清除模式
 *    - O(k)唤醒性能（k=设置的标志位数）
 *    - 使用位图索引优化，只检查等待相关标志的任务
 *    - 适合多对多的事件通知场景
 *
 * ⚠️ 注意事项：
 *    - 事件标志是位操作，需要明确定义各位含义
 *    - FLYOS_EVENT_CLEAR_ON_EXIT会在wait成功后自动清除标志
 *    - 不带CLEAR_ON_EXIT需要手动调用flyos_event_clear()
 *    - 事件标志适合状态通知，不适合传递数据内容
 *    - 对于数据传递应使用消息队列或邮箱
 *    - 多个任务等待同一事件时，所有等待任务都会被唤醒
 */

/**
 * @brief 创建事件标志
 * @details 根据event_buffer参数自动选择创建方式：
 *          - event_buffer=NULL: 从堆中动态分配（需要FLYOS_USE_HEAP=1）
 *          - event_buffer!=NULL: 使用用户提供的静态缓冲区
 */
flyos_event_t* flyos_event_create(flyos_event_t *event_buffer)
{
	flyos_event_t *event;
	flyos_u8_t obj_flags = 0;

	if (event_buffer != NULL) {
		obj_flags = FLYOS_IPC_FLAG_STATIC;  /* 静态对象 */
	}

	if (event_buffer == NULL) {
		/* 动态创建：从堆分配内存 */
#if FLYOS_USE_HEAP
		event = (flyos_event_t *)flyos_mem_alloc(sizeof(flyos_event_t));
		if (event == NULL) {
			return NULL;  /* 内存分配失败 */
		}
#else
		/* FLYOS_USE_HEAP=0时不支持动态创建 */
		return NULL;
#endif
	} else {
		/* 静态创建：使用用户提供的缓冲区 */
		event = event_buffer;
	}

	/* 初始化IPC基类（obj_flags是对象标志位） */
	ipc_base_init(&event->base, FLYOS_IPC_TYPE_EVENT, obj_flags, "event");
	wait_queue_init(&event->wait_queue);

	/* 初始化事件标志字段（event->flags是事件标志位，32个事件位） */
	event->flags = 0;  /* 所有事件位初始为0 */
	memset(event->flag_waiters, 0, sizeof(event->flag_waiters));

	return event;
}

/**
 * @brief 删除事件标志
 * @details 删除事件标志并唤醒所有等待的任务。
 *          自动识别静态/动态创建的对象并正确处理内存。
 */
flyos_err_t flyos_event_delete(flyos_event_t *event)
{
	flyos_base_t level;
	IPC_ENTER(event, FLYOS_INVALID);

	/* 唤醒所有等待任务 */
	while (!wait_queue_is_empty(&event->wait_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&event->wait_queue);
		if (task != NULL) {
			flyos_delta_queue_remove(task);

			task->error_code = FLYOS_ERROR;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);
		}
	}

	IPC_EXIT();

	/* 根据对象标志位判断是否需要释放内存 */
#if FLYOS_USE_HEAP
	if (!(event->base.flags & FLYOS_IPC_FLAG_STATIC)) {
		/* 动态创建的对象：释放内存 */
		flyos_mem_free(event);
	}
	/* 静态创建的对象：不释放内存，由用户管理 */
#endif

	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief 设置事件标志
 */
flyos_err_t flyos_event_set(flyos_event_t *event, flyos_u32_t flags)
{
	flyos_base_t level;
	IPC_ENTER(event, FLYOS_INVALID);
	IPC_CHECK_TYPE(event, FLYOS_IPC_TYPE_EVENT);

	event->flags |= flags;

	/* 位图索引优化：只检查与设置标志相关的任务 */
	flyos_u32_t set_flags = flags;

	while (set_flags != 0) {
		/* 找到下一个被设置的标志位 */
		flyos_u8_t flag_bit = __builtin_ctz(set_flags);

		/* 检查是否有任务等待该标志位 */
		flyos_u32_t waiter_prio_bitmap = event->flag_waiters[flag_bit];

		if (waiter_prio_bitmap != 0) {
			/* 遍历等待该标志的各优先级任务 */
			while (waiter_prio_bitmap != 0) {
				flyos_u8_t prio = __builtin_ctz(waiter_prio_bitmap);
				flyos_tcb_t *task = event->wait_queue.prio_list[prio];
				flyos_tcb_t *next;

				/* 检查该优先级的所有任务 */
				while (task != NULL) {
					next = task->rdy_wait_link.next;

					flyos_u32_t wait_flags = (flyos_u32_t)task->user_data & 0xFFFF;
					flyos_event_wait_mode_t mode =
						(flyos_event_wait_mode_t)((flyos_u32_t)task->user_data >> 16);

					bool wakeup = false;

					/* 检查唤醒条件 */
					if (mode & FLYOS_EVENT_WAIT_ALL) {
						/* 等待所有标志 */
						if ((event->flags & wait_flags) == wait_flags) {
							wakeup = true;
						}
					} else {
						/* 等待任意标志 */
						if (event->flags & wait_flags) {
							wakeup = true;
						}
					}

					if (wakeup) {
						/* 从等待队列移除 */
						wait_queue_remove(&event->wait_queue, task);

						/* 更新位图索引：清除任务等待的所有标志位 */
						for (flyos_u8_t i = 0; i < 32; i++) {
							if (wait_flags & (1UL << i)) {
								event->flag_waiters[i] &= ~(1UL << task->priority);
							}
						}

						/* 清除标志（如果需要）*/
						if (mode & FLYOS_EVENT_CLEAR_ON_EXIT) {
							event->flags &= ~wait_flags;
						}

						/* 唤醒任务 */
						flyos_delta_queue_remove(task);
						task->error_code = FLYOS_OK;
						task->state = FLYOS_TASK_READY;
						task->block_reason = FLYOS_BLOCK_NONE;
						task->block_object = NULL;
						flyos_insert_ready_list(task);
					}

					task = next;
				}

				/* 清除已检查的优先级位 */
				waiter_prio_bitmap &= ~(1UL << prio);
			}
		}

		/* 清除已处理的标志位 */
		set_flags &= ~(1UL << flag_bit);
	}

	IPC_EXIT();
	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief 清除事件标志
 */
flyos_err_t flyos_event_clear(flyos_event_t *event, flyos_u32_t flags)
{
	flyos_base_t level;
	IPC_ENTER(event, FLYOS_INVALID);

	event->flags &= ~flags;

	IPC_EXIT();
	return FLYOS_OK;
}

/**
 * @brief 等待事件标志
 */
flyos_err_t flyos_event_wait(flyos_event_t *event, flyos_u32_t flags,
							 flyos_event_wait_mode_t mode, flyos_tick_t timeout,
							 flyos_u32_t *result_flags)
{
	if (unlikely(flags > 0xFFFF)) {
		return FLYOS_INVALID;
	}

	flyos_base_t level;
	IPC_ENTER(event, FLYOS_INVALID);

	bool condition_met = false;

	if (mode & FLYOS_EVENT_WAIT_ALL) {
		if ((event->flags & flags) == flags) {
			condition_met = true;
		}
	} else {
		if (event->flags & flags) {
			condition_met = true;
		}
	}

	if (condition_met) {
		if (result_flags != NULL) {
			*result_flags = event->flags & flags;
		}

		if (mode & FLYOS_EVENT_CLEAR_ON_EXIT) {
			event->flags &= ~flags;
		}

		IPC_EXIT();
		return FLYOS_OK;
	}

	if (timeout == FLYOS_NO_WAIT) {
		IPC_EXIT();
		return FLYOS_TIMEOUT;
	}

	/* 阻塞任务 */
	flyos_tcb_t *current = flyos_task_get_current();
	flyos_remove_ready_list(current);
	current->state = FLYOS_TASK_BLOCK;
	current->block_reason = FLYOS_BLOCK_IPC;
	current->block_object = event;
	current->user_data = (void *)((flags & 0xFFFF) | ((flyos_u32_t)mode << 16));
	current->error_code = FLYOS_OK;

	/* 更新位图索引：记录任务等待的标志位 */
	for (flyos_u8_t i = 0; i < 32; i++) {
		if (flags & (1UL << i)) {
			event->flag_waiters[i] |= (1UL << current->priority);
		}
	}

	wait_queue_insert(&event->wait_queue, current);

	if (timeout != FLYOS_WAIT_FOREVER) {
		flyos_delta_queue_insert(current, timeout);
	}

	IPC_EXIT();
	flyos_scheduler();

	if (current->error_code == FLYOS_OK && result_flags != NULL) {
		flyos_base_t level2 = flyos_enter_critical();
		*result_flags = event->flags & flags;
		flyos_exit_critical(level2);
	}

	return current->error_code;
}

/**
 * @brief 获取事件标志
 */
flyos_u32_t flyos_event_get_flags(flyos_event_t *event)
{
	if (event == NULL) {
		return 0;
	}
	return event->flags;
}

#endif /* FLYOS_USE_EVENT */

/*===========================================================================*/
/* 邮箱（Mailbox） - 轻量级单消息传递                                          */
/*===========================================================================*/

/**
 * 邮箱使用场景说明：
 *
 * 1. 大数据块传递（Large Data Block Transfer）
 *    - 场景：任务间传递大型数据结构（如图像帧、日志缓冲区）
 *    - 示例：
 *      typedef struct {
 *          uint8_t *data;
 *          uint32_t size;
 *          uint32_t timestamp;
 *      } image_frame_t;
 *
 *      flyos_mailbox_t *image_mbox = flyos_mailbox_create();
 *
 *      // 采集任务（生产者）
 *      void capture_task(void) {
 *          image_frame_t *frame = malloc(sizeof(image_frame_t));
 *          frame->data = capture_image();  // 大型数据
 *          frame->size = IMAGE_SIZE;
 *          frame->timestamp = get_timestamp();
 *
 *          // 只传递指针，零拷贝
 *          flyos_mailbox_send(image_mbox, frame, FLYOS_WAIT_FOREVER);
 *      }
 *
 *      // 处理任务（消费者）
 *      void process_task(void) {
 *          image_frame_t *frame;
 *          flyos_mailbox_receive(image_mbox, (void**)&frame, FLYOS_WAIT_FOREVER);
 *
 *          // 处理图像数据
 *          process_image(frame->data, frame->size);
 *
 *          // 释放资源
 *          free(frame->data);
 *          free(frame);
 *      }
 *
 * 2. 单生产者单消费者（Single Producer Single Consumer）
 *    - 场景：一个任务生成消息，另一个任务处理消息
 *    - 示例：
 *      typedef struct {
 *          char *log_buffer;
 *          uint32_t length;
 *      } log_msg_t;
 *
 *      flyos_mailbox_t *log_mbox = flyos_mailbox_create();
 *
 *      // 日志记录任务
 *      void logger_task(void) {
 *          log_msg_t *msg = malloc(sizeof(log_msg_t));
 *          msg->log_buffer = format_log_message();
 *          msg->length = strlen(msg->log_buffer);
 *
 *          // 非阻塞发送（如果邮箱满则丢弃）
 *          if (flyos_mailbox_send(log_mbox, msg, FLYOS_NO_WAIT) != FLYOS_OK) {
 *              free(msg->log_buffer);
 *              free(msg);
 *          }
 *      }
 *
 *      // 日志写入任务
 *      void log_writer_task(void) {
 *          log_msg_t *msg;
 *          while (1) {
 *              flyos_mailbox_receive(log_mbox, (void**)&msg, FLYOS_WAIT_FOREVER);
 *              write_to_flash(msg->log_buffer, msg->length);
 *              free(msg->log_buffer);
 *              free(msg);
 *          }
 *      }
 *
 * 3. 中断通知任务（ISR to Task Notification）
 *    - 场景：中断传递数据缓冲区指针给任务处理
 *    - 示例：
 *      typedef struct {
 *          uint8_t *rx_buffer;
 *          uint16_t rx_length;
 *      } uart_msg_t;
 *
 *      flyos_mailbox_t *uart_mbox = flyos_mailbox_create();
 *
 *      // UART中断服务程序
 *      void UART_IRQHandler(void) {
 *          if (UART_GetFlagStatus(UART_FLAG_RXNE)) {
 *              uart_msg_t *msg = malloc(sizeof(uart_msg_t));
 *              msg->rx_buffer = get_dma_buffer();
 *              msg->rx_length = get_rx_length();
 *
 *              // 从中断发送（非阻塞）
 *              flyos_mailbox_send_from_isr(uart_mbox, msg);
 *          }
 *      }
 *
 *      // UART处理任务
 *      void uart_process_task(void) {
 *          uart_msg_t *msg;
 *          while (1) {
 *              flyos_mailbox_receive(uart_mbox, (void**)&msg, FLYOS_WAIT_FOREVER);
 *              parse_uart_data(msg->rx_buffer, msg->rx_length);
 *              free(msg);
 *          }
 *      }
 *
 * 4. 内存池对象传递（Memory Pool Object Transfer）
 *    - 场景：配合内存池使用，避免频繁malloc/free
 *    - 示例：
 *      typedef struct {
 *          uint32_t sensor_data[10];
 *          uint32_t timestamp;
 *      } sensor_packet_t;
 *
 *      #define POOL_SIZE 10
 *      sensor_packet_t packet_pool[POOL_SIZE];
 *      flyos_semaphore_t *pool_sem;  // 可用对象计数
 *      flyos_mailbox_t *data_mbox;
 *
 *      void init_system(void) {
 *          pool_sem = flyos_semaphore_create(POOL_SIZE, POOL_SIZE);
 *          data_mbox = flyos_mailbox_create();
 *      }
 *
 *      // 传感器任务
 *      void sensor_task(void) {
 *          // 从池中获取对象
 *          flyos_semaphore_wait(pool_sem, FLYOS_WAIT_FOREVER);
 *          sensor_packet_t *packet = &packet_pool[get_free_index()];
 *
 *          // 填充数据
 *          read_sensor_data(packet->sensor_data);
 *          packet->timestamp = get_timestamp();
 *
 *          // 发送指针
 *          flyos_mailbox_send(data_mbox, packet, FLYOS_WAIT_FOREVER);
 *      }
 *
 *      // 处理任务
 *      void process_task(void) {
 *          sensor_packet_t *packet;
 *          flyos_mailbox_receive(data_mbox, (void**)&packet, FLYOS_WAIT_FOREVER);
 *
 *          // 处理数据
 *          process_sensor_data(packet);
 *
 *          // 归还对象到池
 *          flyos_semaphore_signal(pool_sem);
 *      }
 *
 * 5. 命令消息传递（Command Message Passing）
 *    - 场景：传递命令结构体指针，支持复杂参数
 *    - 示例：
 *      typedef enum {
 *          CMD_START, CMD_STOP, CMD_CONFIG, CMD_QUERY
 *      } cmd_type_t;
 *
 *      typedef struct {
 *          cmd_type_t type;
 *          void *params;
 *          uint32_t param_size;
 *          void (*callback)(void *result);
 *      } cmd_msg_t;
 *
 *      flyos_mailbox_t *cmd_mbox = flyos_mailbox_create();
 *
 *      // 控制任务
 *      void control_task(void) {
 *          cmd_msg_t *cmd = malloc(sizeof(cmd_msg_t));
 *          cmd->type = CMD_CONFIG;
 *          cmd->params = malloc(sizeof(config_t));
 *          cmd->callback = config_done_callback;
 *
 *          flyos_mailbox_send(cmd_mbox, cmd, 1000);
 *      }
 *
 *      // 执行任务
 *      void executor_task(void) {
 *          cmd_msg_t *cmd;
 *          while (1) {
 *              flyos_mailbox_receive(cmd_mbox, (void**)&cmd, FLYOS_WAIT_FOREVER);
 *
 *              switch (cmd->type) {
 *              case CMD_START:
 *                  start_operation();
 *                  break;
 *              case CMD_CONFIG:
 *                  apply_config(cmd->params);
 *                  break;
 *              // ...
 *              }
 *
 *              if (cmd->callback) {
 *                  cmd->callback(NULL);
 *              }
 *
 *              free(cmd->params);
 *              free(cmd);
 *          }
 *      }
 *
 * ✅ 邮箱特性：
 *    - 零拷贝：只传递指针，不拷贝数据
 *    - 轻量级：内存占用~172字节（vs消息队列~296字节）
 *    - 单消息：只能容纳1个消息，适合高频单对单通信
 *    - O(1)性能：发送/接收都是常数时间
 *    - 支持阻塞和非阻塞模式
 *    - 支持ISR安全的发送接口
 *
 * ⚠️ 注意事项：
 *    - 只能容纳1个消息，不能缓存多个消息
 *    - 需要管理消息的内存生命周期（谁分配谁释放）
 *    - 传递的是指针，发送者和接收者需要约定数据所有权
 *    - 发送者发送后不应再访问数据（除非明确约定）
 *    - 接收者负责处理完后释放内存
 *    - 不适合需要缓冲多个消息的场景（应使用消息队列）
 *    - 邮箱满时发送会阻塞，邮箱空时接收会阻塞
 *
 * 📊 邮箱 vs 消息队列选择指南：
 *    - 数据较大（>64字节）→ 使用邮箱（零拷贝）
 *    - 需要缓冲多个消息 → 使用消息队列
 *    - 单对单高频通信 → 使用邮箱
 *    - 多对多通信 → 使用消息队列
 *    - 数据较小且需要缓冲 → 使用消息队列
 */

#if FLYOS_USE_MAILBOX

/**
 * @brief 创建邮箱
 * @details 根据mbox_buffer参数自动选择创建方式：
 *          - mbox_buffer=NULL: 从堆中动态分配（需要FLYOS_USE_HEAP=1）
 *          - mbox_buffer!=NULL: 使用用户提供的静态缓冲区
 */
flyos_mailbox_t* flyos_mailbox_create(flyos_mailbox_t *mbox_buffer)
{
	flyos_mailbox_t *mbox;
	flyos_u8_t flags = 0;

	if (mbox_buffer != NULL) {
		flags = FLYOS_IPC_FLAG_STATIC;
	}

	if (mbox_buffer == NULL) {
		/* 动态创建：从堆分配内存 */
#if FLYOS_USE_HEAP
		mbox = (flyos_mailbox_t *)flyos_mem_alloc(sizeof(flyos_mailbox_t));
		if (mbox == NULL) {
			return NULL;  /* 内存分配失败 */
		}
#else
		/* FLYOS_USE_HEAP=0时不支持动态创建 */
		return NULL;
#endif
	} else {
		/* 静态创建：使用用户提供的缓冲区 */
		mbox = mbox_buffer;
	}

	/* 初始化IPC基类 */
	ipc_base_init(&mbox->base, FLYOS_IPC_TYPE_MAILBOX, flags, "mailbox");

	/* 初始化等待队列 */
	wait_queue_init(&mbox->send_queue);
	wait_queue_init(&mbox->recv_queue);

	/* 初始化邮箱字段 */
	mbox->message = NULL;
	mbox->full = false;

	return mbox;
}

/**
 * @brief 删除邮箱
 * @details 删除邮箱并唤醒所有等待的任务。
 *          自动识别静态/动态创建的对象并正确处理内存。
 */
flyos_err_t flyos_mailbox_delete(flyos_mailbox_t *mbox)
{
	flyos_base_t level;
	IPC_ENTER(mbox, FLYOS_INVALID);
	IPC_CHECK_TYPE(mbox, FLYOS_IPC_TYPE_MAILBOX);

	/* 唤醒所有发送等待者 */
	while (!wait_queue_is_empty(&mbox->send_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&mbox->send_queue);
		if (task != NULL) {
			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_ERROR;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);
		}
	}

	/* 唤醒所有接收等待者 */
	while (!wait_queue_is_empty(&mbox->recv_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&mbox->recv_queue);
		if (task != NULL) {
			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_ERROR;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);
		}
	}

	IPC_EXIT();

	/* 根据flags判断是否需要释放内存 */
#if FLYOS_USE_HEAP
	if (!(mbox->base.flags & FLYOS_IPC_FLAG_STATIC)) {
		/* 动态创建的对象：释放内存 */
		flyos_mem_free(mbox);
	}
	/* 静态创建的对象：不释放内存，由用户管理 */
#endif

	flyos_scheduler();

	return FLYOS_OK;
}

/**
 * @brief   发送消息到邮箱
 * @details 尝试将消息指针发送到邮箱。如果有接收者等待则直接传递；
 *          否则将指针存储到邮箱。邮箱满时根据timeout决定是否阻塞。
 *          采用零拷贝方式，只传递指针不复制数据。
 *
 * @param[in,out] mbox      邮箱句柄
 * @param[in] msg           消息指针
 * @param[in] timeout       超时时间（tick）
 * @param[in] from_isr      true=从ISR调用
 *
 * @return  FLYOS_OK=成功，FLYOS_FULL=邮箱满，FLYOS_TIMEOUT=超时，
 *          FLYOS_ERROR=ISR阻塞尝试
 *
 * @note    任务上下文或ISR（from_isr=true时）
 */
static flyos_err_t mailbox_send_common(flyos_mailbox_t *mbox, void *msg,
                                       flyos_tick_t timeout, bool from_isr)
{
	if (unlikely(from_isr && timeout != FLYOS_NO_WAIT)) {
		return FLYOS_ERROR;  /* ISR不允许阻塞 */
	}

	flyos_base_t level;
	if (from_isr) {
		IPC_ENTER_ISR(mbox, FLYOS_INVALID);
	} else {
		IPC_ENTER(mbox, FLYOS_INVALID);
	}

	IPC_CHECK_TYPE(mbox, FLYOS_IPC_TYPE_MAILBOX);

	/* 直接将信息传递给等待的接收者*/
	if (likely(!wait_queue_is_empty(&mbox->recv_queue) && !mbox->full)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&mbox->recv_queue);
		if (task != NULL) {
			/* 将消息指针写入接收者缓冲区 */
			void **recv_ptr = (void **)task->user_data;
			*recv_ptr = msg;

			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_OK;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);

			if (from_isr) {
				IPC_EXIT_ISR();
				flyos_port_yield();
			} else {
				IPC_EXIT();
				flyos_scheduler();
			}
			return FLYOS_OK;
		}
	}

	/* 慢速路径：邮箱已满 */
	if (unlikely(mbox->full)) {
		if (timeout == FLYOS_NO_WAIT) {
			if (from_isr) {
				IPC_EXIT_ISR();
			} else {
				IPC_EXIT();
			}
			return FLYOS_FULL;
		}

		/* 阻塞发送者 */
		flyos_tcb_t *current = flyos_task_get_current();
		flyos_remove_ready_list(current);
		current->state = FLYOS_TASK_BLOCK;
		current->block_reason = FLYOS_BLOCK_IPC;
		current->block_object = mbox;
		current->user_data = msg;
		current->error_code = FLYOS_OK;

		wait_queue_insert(&mbox->send_queue, current);

		if (timeout != FLYOS_WAIT_FOREVER) {
			flyos_delta_queue_insert(current, timeout);
		}

		IPC_EXIT();
		flyos_scheduler();
		return current->error_code;
	}

	/* 邮箱空闲，存储消息指针 */
	mbox->message = msg;
	mbox->full = true;

	if (from_isr) {
		IPC_EXIT_ISR();
	} else {
		IPC_EXIT();
	}
	return FLYOS_OK;
}

/**
 * @brief 发送消息到邮箱
 */
flyos_err_t flyos_mailbox_send(flyos_mailbox_t *mbox, void *msg,
                               flyos_tick_t timeout)
{
	return mailbox_send_common(mbox, msg, timeout, false);
}

/**
 * @brief 从ISR发送消息到邮箱
 */
flyos_err_t flyos_mailbox_send_from_isr(flyos_mailbox_t *mbox, void *msg)
{
	return mailbox_send_common(mbox, msg, FLYOS_NO_WAIT, true);
}

/**
 * @brief 从邮箱接收消息
 */
flyos_err_t flyos_mailbox_receive(flyos_mailbox_t *mbox, void **msg,
                                  flyos_tick_t timeout)
{
	flyos_base_t level;
	IPC_ENTER(mbox, FLYOS_INVALID);
	IPC_CHECK_TYPE(mbox, FLYOS_IPC_TYPE_MAILBOX);

	if (unlikely(msg == NULL)) {
		IPC_EXIT();
		return FLYOS_INVALID;
	}

	/* 快速路径：邮箱为空 */
	if (unlikely(!mbox->full)) {
		if (timeout == FLYOS_NO_WAIT) {
			IPC_EXIT();
			return FLYOS_EMPTY;
		}

		/* 阻塞接收者 */
		flyos_tcb_t *current = flyos_task_get_current();
		flyos_remove_ready_list(current);
		current->state = FLYOS_TASK_BLOCK;
		current->block_reason = FLYOS_BLOCK_IPC;
		current->block_object = mbox;
		current->user_data = (void *)msg;  /* 保存接收缓冲区指针 */
		current->error_code = FLYOS_OK;

		wait_queue_insert(&mbox->recv_queue, current);

		if (timeout != FLYOS_WAIT_FOREVER) {
			flyos_delta_queue_insert(current, timeout);
		}

		IPC_EXIT();
		flyos_scheduler();
		return current->error_code;
	}

	/* 从邮箱读取消息指针 */
	*msg = mbox->message;
	mbox->message = NULL;
	mbox->full = false;

	/* 唤醒等待的发送者 */
	if (!wait_queue_is_empty(&mbox->send_queue)) {
		flyos_tcb_t *task = wait_queue_remove_highest(&mbox->send_queue);
		if (task != NULL) {
			/* 将发送者的消息指针存入邮箱 */
			mbox->message = task->user_data;
			mbox->full = true;

			flyos_delta_queue_remove(task);
			task->error_code = FLYOS_OK;
			task->state = FLYOS_TASK_READY;
			task->block_reason = FLYOS_BLOCK_NONE;
			task->block_object = NULL;
			flyos_insert_ready_list(task);

			IPC_EXIT();
			flyos_scheduler();
			return FLYOS_OK;
		}
	}

	IPC_EXIT();
	return FLYOS_OK;
}

#endif /* FLYOS_USE_MAILBOX */

/*===========================================================================*/
/* 内部函数 - 超时处理                                                        */
/*===========================================================================*/

/**
 * @brief   从IPC等待队列移除任务
 * @details 当任务等待IPC对象超时时，由内核调用此函数从等待队列移除任务。
 *
 *          处理所有IPC类型：
 *          - 信号量/互斥锁/事件：从wait_queue移除
 *          - 消息队列：从send_queue和recv_queue中尝试移除
 *          - 邮箱：从send_queue和recv_queue中尝试移除
 *
 *          包含类型安全检查，验证IPC对象的魔术数字和类型标识。
 *
 * @param[in,out] task  要移除的任务
 *
 * @note    持有临界区锁
 */
void flyos_ipc_remove_task_from_wait_list(flyos_tcb_t *task)
{
	if (unlikely(task == NULL || task->block_object == NULL)) {
		return;
	}

	flyos_ipc_base_t *base = (flyos_ipc_base_t *)task->block_object;

	/* 防止内存损坏或野指针，验证魔术数字 */
	if (unlikely(base->magic != FLYOS_IPC_MAGIC)) {
		FLYOS_ASSERT(0);
		return;
	}

	/* 根据IPC类型进行移除操作 */
	switch (base->type) {
#if FLYOS_USE_SEMAPHORE
	case FLYOS_IPC_TYPE_SEMAPHORE:
		{
			flyos_semaphore_t *sem = (flyos_semaphore_t *)task->block_object;
			wait_queue_remove(&sem->wait_queue, task);
		}
		break;
#endif

#if FLYOS_USE_MUTEX
	case FLYOS_IPC_TYPE_MUTEX:
		{
			flyos_mutex_t *mutex = (flyos_mutex_t *)task->block_object;
			wait_queue_remove(&mutex->wait_queue, task);
		}
		break;
#endif

#if FLYOS_USE_QUEUE
	case FLYOS_IPC_TYPE_QUEUE:
		{
			flyos_queue_t *queue = (flyos_queue_t *)task->block_object;
			/* 从两个队列中尝试移除（任务只会在其中一个队列中）*/
			wait_queue_remove(&queue->send_queue, task);
			wait_queue_remove(&queue->recv_queue, task);
		}
		break;
#endif

#if FLYOS_USE_EVENT
	case FLYOS_IPC_TYPE_EVENT:
		{
			flyos_event_t *event = (flyos_event_t *)task->block_object;
			wait_queue_remove(&event->wait_queue, task);

			/* 更新位图索引：清除任务等待的标志位 */
			flyos_u32_t wait_flags = (flyos_u32_t)task->user_data & 0xFFFF;
			for (flyos_u8_t i = 0; i < 32; i++) {
				if (wait_flags & (1UL << i)) {
					event->flag_waiters[i] &= ~(1UL << task->priority);
				}
			}
		}
		break;
#endif

#if FLYOS_USE_MAILBOX
	case FLYOS_IPC_TYPE_MAILBOX:
		{
			flyos_mailbox_t *mbox = (flyos_mailbox_t *)task->block_object;
			/* 从两个队列中尝试移除 */
			wait_queue_remove(&mbox->send_queue, task);
			wait_queue_remove(&mbox->recv_queue, task);
		}
		break;
#endif

	default:
		/* 未知类型 */
		FLYOS_ASSERT(0);
		break;
	}

	task->block_object = NULL;
}

/*===========================================================================*/
