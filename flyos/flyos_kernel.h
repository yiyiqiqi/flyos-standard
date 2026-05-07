/******************************************************************************
 * @file    flyos_kernel.h
 * @brief   FlyOS 内核核心接口
 * @version 1.0.0
 * @date    2024
 *
 * @author  yiyi
 *
 *****************************************************************************
 * @details 概述
 *
 * 本文件提供 FlyOS 实时操作系统的核心内核 API，是用户应用层与内核交互的主要接口。
 * 包含任务管理、调度器控制、内核初始化等关键功能。
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                      FlyOS 内核功能模块                                   │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ 内核初始化        │ 系统启动、内核配置、调度器启动                             │
 * │ 任务管理          │ 任务创建、删除、挂起、恢复、优先级控制                      │
 * │ 调度器控制        │ 调度器锁定、抢占控制、上下文切换                           │
 * │ 时间管理          │ Tick 处理、延时函数、时间片轮转                           │
 * │ 钩子函数          │ 空闲钩子、Tick 钩子、任务切换钩子                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * @section kernel_features 核心特性
 *
 * - 抢占式优先级调度器，支持 32 级优先级
 * - 时间片轮转调度，支持可配置的时间片长度
 * - O(1) 时间复杂度的任务调度算法
 * - 支持静态和动态任务创建
 * - 可配置的调度器锁定机制
 * - 完善的钩子函数支持
 *
 * @section kernel_organization 文件组织结构
 *
 * 1. 类型定义和数据结构
 * 2. 内核初始化和控制接口
 * 3. 任务管理接口
 * 4. 调度器控制接口
 * 5. 时间管理接口
 * 6. 内核内部接口
 * 7. 用户钩子函数
 *
 * @note   优先级数值越小优先级越高（0 = 最高优先级）
 *
 *****************************************************************************/

#ifndef FLYOS_KERNEL_H
#define FLYOS_KERNEL_H

#include "flyos_config.h"
#include "flyos_type.h"
#include "flyos_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* 类型定义和数据结构                                                         */
/*===========================================================================*/

/**
 * typedef flyos_task_proc_t - 任务执行过程函数原型
 * @param: 传递给任务的用户参数
 *
 * 任务函数一般不返回，若任务执行完成返回，应调用flyos_task_delete(NULL)。
 */
typedef void (*flyos_task_proc_t)(void *param);

/**
 * struct flyos_task_config_t - 任务配置结构
 * @name: 任务名称(最多15个字符+'\0')
 * @proc: 任务执行过程函数
 * @param: 传递给任务的参数
 * @stack_buffer: 栈缓冲区(NULL=动态分配，需要FLYOS_USE_HEAP=1)
 * @stack_size: 栈大小(字节)
 * @priority: 任务优先级(0=最高，31=最低)
 * @time_slice: 时间片(0=使用默认值FLYOS_TIME_SLICE_TICKS)
 *
 * 用于创建任务时传递配置参数。
 * 当FLYOS_USE_HEAP=1时，stack_buffer可以为NULL，系统将自动从堆分配栈。
 * 当FLYOS_USE_HEAP=0时，stack_buffer必须非NULL。
 */
typedef struct {
	const char          *name;
	flyos_task_proc_t    proc;
	void                *param;
	void                *stack_buffer;
	flyos_u32_t          stack_size;
	flyos_u8_t           priority;
	flyos_tick_t         time_slice;
} flyos_task_config_t;

/**
 * struct flyos_task_control_block - 任务控制块结构
 * @stack_pointer: 当前栈指针
 * @name: 任务名称
 * @priority: 当前优先级，可能被优先级继承改变
 * @base_priority: 原始优先级
 * @state: 任务状态(INIT/READY/BLOCK/CLOSE)
 * @flags: 任务对象静态/动态创建标识
 * @time_slice: 时间片长度
 * @remaining_time_slice: 剩余时间片
 * @block_reason: 阻塞原因，在state=BLOCK时有效
 * @reserved2: 保留字段用于对齐
 * @reserved3: 保留字段用于对齐
 * @reserved4: 保留字段用于对齐
 * @stack_base: 栈基地址
 * @stack_size: 栈大小(字节)
 * @rdy_wait_link: 就绪队列/等待队列链表节点
 * @timer_delta_link: 定时器队列链表节点(Delta队列)
 * @block_object: 任务阻塞的IPC对象
 * @error_code: 最后的错误码
 * @run_time: 总运行时间(tick数)
 * @switch_count: 上下文切换计数
 * @max_stack_used: 最大栈使用量(字节)
 * @user_data: 用户自定义数据指针
 */
typedef struct flyos_task_control_block {
	/* 硬件上下文 */
	void               *stack_pointer;

	/* 任务标识 */
	char                name[FLYOS_MAX_TASK_NAME];
	flyos_u8_t          priority;
	flyos_u8_t          base_priority;
	flyos_u8_t          state;
	flyos_u8_t          flags;

	/* 时间管理 */
	flyos_tick_t        time_slice;
	flyos_tick_t        remaining_time_slice;
	flyos_u8_t          block_reason;
	flyos_u8_t          reserved2;
	flyos_u8_t          reserved3;
	flyos_u8_t          reserved4;

	/* 栈信息 */
	void               *stack_base;
	flyos_u32_t         stack_size;

	/* 就绪队列/等待队列链表节点 */
	struct {
		struct flyos_task_control_block *next;
		struct flyos_task_control_block *prev;
	} rdy_wait_link;

	/* 定时器队列链表节点(Delta队列) */
	struct {
		flyos_tick_t delta;
		struct flyos_task_control_block *next;
		struct flyos_task_control_block *prev;
	} timer_delta_link;

	/* 阻塞信息 */
	void               *block_object;
	flyos_err_t         error_code;

	/* 统计信息 */
#if FLYOS_USE_TASK_STAT
	flyos_u32_t         run_time;
	flyos_u32_t         switch_count;
	flyos_u32_t         max_stack_used;
#endif

	/* 用户数据 */
	void                *user_data;
} flyos_tcb_t;

/*===========================================================================*/
/* 内核初始化和控制接口                                                       */
/*===========================================================================*/

/**
 * @brief 初始化FlyOS内核
 * @details 初始化内核数据结构、调度器、创建空闲任务并初始化硬件。
 *
 * @return 成功返回FLYOS_OK，否则返回错误码
 *
 * @note Context: 系统启动阶段，调度器未运行
 */
flyos_err_t flyos_kernel_init(void);

/**
 * @brief 启动FlyOS调度器
 * @details 启动多任务调度，选择最高优先级就绪任务开始执行。
 *          此函数永不返回，如果返回说明启动失败。
 *
 * @return 启动失败返回FLYOS_ERROR，成功时永不返回
 *
 * @note Context: 调用前至少要创建一个用户任务，调用后系统进入多任务模式
 */
flyos_err_t flyos_kernel_start(void);

/**
 * @brief 获取当前系统tick计数
 * @details 返回自内核启动以来的tick数。
 *
 * @return 当前tick计数
 *
 * @note Context: 任意上下文(任务/ISR)，线程安全
 */
flyos_tick_t flyos_kernel_get_tick_count(void);

/**
 * @brief 获取FlyOS版本号
 * @details 返回版本号，格式: (major << 16) | (minor << 8) | patch
 *
 * @return 32位整数版本号
 */
flyos_u32_t flyos_kernel_get_version(void);

/*===========================================================================*/
/* 任务管理接口                                                              */
/*===========================================================================*/

/**
 * @brief 创建新任务（支持静态/动态创建TCB与栈）
 * @details 创建一个新任务并将其加入就绪队列。支持灵活的资源分配方式：
 *
 *          TCB分配：
 *          - 动态创建TCB：tcb_buffer=NULL，从堆中分配TCB
 *          - 静态创建TCB：tcb_buffer!=NULL，使用用户提供的TCB缓冲区
 *
 *          栈分配：
 *          - 动态分配栈：config->stack_buffer=NULL，从堆中自动分配栈空间
 *          - 静态提供栈：config->stack_buffer!=NULL，使用用户提供的栈缓冲区
 *          注意：FLYOS_USE_HEAP=0时，stack_buffer必须非NULL，并且保证栈的生命周期与内存对齐
 *
 * @param[in,out] tcb_buffer  任务控制块缓冲区指针
 *                            - NULL: 动态分配TCB，需要FLYOS_USE_HEAP=1
 *                            - 非NULL: 使用用户提供的静态TCB缓冲区
 * @param[in] config          任务配置结构指针
 *                            - config->stack_buffer: NULL=动态分配栈，非NULL=静态栈
 *                            （动态栈需要FLYOS_USE_HEAP=1）
 *
 * @return 成功返回任务句柄(TCB指针)，失败返回NULL
 *
 * @note Context: 任务上下文或初始化阶段
 * @note FLYOS_USE_HEAP=0时，只能使用静态创建（tcb_buffer和stack_buffer必须非NULL）
 * @note 栈必须8字节对齐（静态栈使用 __attribute__((aligned(8)))）
 * @note 如果新任务优先级高于当前任务，会立即发生任务切换
 *
 * @code
 * // 1. 完全动态创建（需要FLYOS_USE_HEAP=1）
 * flyos_task_config_t cfg = {
 *     .name = "sensor",
 *     .proc = sensor_task,
 *     .param = NULL,
 *     .stack_buffer = NULL,
 *     .stack_size = 1024,
 *     .priority = 5,
 *     .time_slice = 0
 * };
 * flyos_tcb_t *task = flyos_task_create(NULL, &cfg);
 *
 * // 2. 动态TCB + 静态栈（需要FLYOS_USE_HEAP=1）
 * static uint8_t stack[1024] __attribute__((aligned(8)));
 * flyos_task_config_t cfg = {
 *     .name = "sensor",
 *     .proc = sensor_task,
 *     .param = NULL,
 *     .stack_buffer = stack,   // 静态栈
 *     .stack_size = 1024,
 *     .priority = 5,
 *     .time_slice = 0
 * };
 * flyos_tcb_t *task = flyos_task_create(NULL, &cfg);
 *
 * // 3. 完全静态创建（始终可用）
 * static flyos_tcb_t tcb_buffer;
 * static uint8_t stack[1024] __attribute__((aligned(8)));
 * flyos_task_config_t cfg = { ... };
 * flyos_tcb_t *task = flyos_task_create(&tcb_buffer, &cfg);
 * @endcode
 */
flyos_tcb_t* flyos_task_create(
	flyos_tcb_t *tcb_buffer,
	const flyos_task_config_t *config
);

/**
 * @brief 删除任务
 * @details 删除任务并自动释放其资源。智能识别资源分配方式：
 *
 *          TCB释放：
 *          - 动态创建的TCB：自动释放TCB内存
 *          - 静态创建的TCB：不释放TCB内存
 *
 *          栈释放：
 *          - 动态分配的栈：自动释放栈内存
 *          - 静态提供的栈：不释放栈内存
 *
 *          如果删除当前任务，立即调用调度器切换到其他任务。
 *
 * @param[in] task 任务句柄(NULL=删除当前任务)
 *
 * @return 成功返回FLYOS_OK，否则返回错误码
 *
 * @note Context: 任务上下文
 * @note 删除当前任务时永不返回
 * @note 静态创建的TCB不会释放内存，用户需自行管理缓冲区生命周期
 * @warning 不能删除空闲任务
 * @warning 删除其他任务时，确保该任务未持有资源(互斥锁等)
 */
flyos_err_t flyos_task_delete(flyos_tcb_t *task);

/**
 * @brief 挂起任务
 * @details 挂起任务，使其从就绪队列移除。
 *          挂起的任务不会运行，直到被flyos_task_resume()恢复。
 *
 * @param[in] task 任务句柄(NULL=挂起当前任务)
 *
 * @return 成功返回FLYOS_OK，否则返回错误码
 *
 * @note Context: 任务上下文
 * @note 挂起当前任务会立即触发调度
 * @note 挂起可以嵌套，需要调用相同次数的恢复
 * @note 挂起的任务仍可能被阻塞在IPC对象上
 */
flyos_err_t flyos_task_suspend(flyos_tcb_t *task);

/**
 * @brief 恢复挂起的任务
 * @details 恢复由flyos_task_suspend()挂起的任务。
 *          如果恢复的任务优先级高于当前任务，会立即发生任务切换。
 *
 * @param[in] task 任务句柄
 *
 * @return 成功返回FLYOS_OK，任务未挂起返回FLYOS_INVALID
 *
 * @note Context: 任务上下文
 */
flyos_err_t flyos_task_resume(flyos_tcb_t *task);

/**
 * @brief 延迟当前任务指定tick数
 * @details 将当前任务置于阻塞状态，延迟指定tick数后自动唤醒。
 *          延迟期间CPU可以执行其他任务。
 *
 * @param[in] ticks 延迟的tick数，当ticks为0时不延迟，立即返回)
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note 调用后立即触发调度
 */
flyos_err_t flyos_task_delay(flyos_tick_t ticks);

/**
 * @brief 延迟到绝对时间
 * @details 实现固定频率的周期性任务，不受任务执行时间影响。
 *          函数会自动更新previous_wake_time为下次唤醒时间。
 *
 * @param[in,out] previous_wake_time 上次唤醒时间指针，初始调用时应设置为当前tick数
 * @param[in] increment tick时间增量
 *
 * @return 成功返回FLYOS_OK，否则返回错误码
 *
 * @note Context: 任务上下文
 * @note 用于周期性任务，保证固定频率执行，适合控制循环等场景
 */
flyos_err_t flyos_task_delay_until(flyos_tick_t *previous_wake_time, flyos_tick_t increment);

/**
 * @brief 修改任务优先级
 * @details 动态修改任务的基础优先级。
 *          如果修改后优先级更高，可能导致立即上下文切换。
 *
 * @param[in] task 任务句柄(NULL=修改当前任务)
 * @param[in] priority 新的优先级(0=最高，31=最低)
 *
 * @return 成功返回FLYOS_OK，否则返回错误码
 *
 * @note Context: 任务上下文
 * @note 可能导致立即上下文切换
 * @note 修改的是base_priority，如果任务正在继承优先级，需要等优先级继承结束后才会生效
 */
flyos_err_t flyos_task_set_priority(flyos_tcb_t *task, flyos_u8_t priority);

/**
 * @brief 获取当前任务句柄
 *
 * @return 当前任务的TCB指针
 *
 * @note Context: 任务上下文，不能在中断或调度器未启动时调用
 */
flyos_tcb_t* flyos_task_get_current(void);

/**
 * @brief 获取任务名称
 *
 * @param[in] task 任务句柄(NULL=获取当前任务名称)
 *
 * @return 任务名称字符串指针
 *
 * @note Context: 任意上下文
 */
const char* flyos_task_get_name(flyos_tcb_t *task);

/**
 * @brief 获取任务状态
 *
 * @param[in] task 任务句柄(NULL=获取当前任务状态)
 *
 * @return 任务状态(INIT/READY/BLOCK/CLOSE)
 *
 * @note Context: 任意上下文
 */
flyos_task_state_t flyos_task_get_state(flyos_tcb_t *task);

/**
 * @brief 让出处理器给下一个就绪任务
 * @details 主动让出CPU，切换到相同或更高优先级的下一个就绪任务。
 *          当前任务仍保持就绪状态，但移到其优先级队列的尾部。
 *
 * @return FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note 强制切换，用于协作式多任务或减少任务饥饿
 */
flyos_err_t flyos_task_yield(void);

/*===========================================================================*/
/* 调度器控制接口                                                            */
/*===========================================================================*/

/**
 * @brief 锁定调度器，禁用任务切换
 * @details 禁用任务切换，保护临界区。
 *          可以嵌套调用，必须调用相同次数的解锁。
 *          中断仍然会发生，只是阻止上下文切换。
 *
 * @note Context: 任务上下文
 * @note 不能在中断中调用
 * @warning 不会禁用中断，只是延迟调度
 * @warning 锁定期间tick计数继续增加
 * @warning 不要长时间锁定，影响实时性
 */
void flyos_scheduler_lock(void);

/**
 * @brief 解锁调度器，启用任务切换
 * @details 恢复任务切换。如果嵌套锁定，只有锁计数为零时才真正解锁。
 *          解锁时如果有更高优先级任务就绪，会立即发生任务切换。
 *
 * @note Context: 任务上下文
 * @note 不能在中断中调用
 * @note 必须与flyos_scheduler_lock()配对使用
 */
void flyos_scheduler_unlock(void);

/**
 * @brief 获取调度器状态
 *
 * @return 运行中返回1，未启动返回0
 *
 * @note Context: 任意上下文
 */
flyos_u32_t flyos_scheduler_get_state(void);

/*===========================================================================*/
/* 内核内部接口                                                               */
/*===========================================================================*/

/**
 * @brief 将任务插入Delta队列
 * @details 将任务按延迟时间插入Delta队列。
 *          Delta队列用于实现定时器，每个节点存储与前一节点的时间差。
 *
 * @param[in,out] task 任务控制块
 * @param[in] delay_ticks 延迟tick数
 *
 * @note Context: 必须在临界区中调用
 * @note Time complexity: O(n)，n为队列中任务数
 */
void flyos_delta_queue_insert(flyos_tcb_t *task, flyos_tick_t delay_ticks);

/**
 * @brief 从Delta队列移除任务
 * @details 从Delta队列中移除任务，并调整后续节点的delta值。
 *
 * @param[in,out] task 任务控制块
 *
 * @note Context: 必须在临界区中调用
 * @note Time complexity: O(n)，n为队列中任务数
 */
void flyos_delta_queue_remove(flyos_tcb_t *task);

/* 就绪队列管理 */

/**
 * @brief 将任务插入就绪列表
 * @details 将任务加入对应优先级的就绪队列，并更新就绪位图。
 *
 * @param[in,out] tcb 任务控制块
 *
 * @note Context: 必须在临界区中调用
 * @note Time complexity: O(1)
 */
void flyos_insert_ready_list(flyos_tcb_t *tcb);

/**
 * @brief 从就绪列表移除任务
 * @details 从就绪队列移除任务，并更新就绪位图。
 *
 * @param[in,out] tcb 任务控制块
 *
 * @note Context: 必须在临界区中调用
 * @note Time complexity: O(1)
 */
void flyos_remove_ready_list(flyos_tcb_t *tcb);

/* 调度器核心 */

/**
 * @brief 触发调度器查找下一个任务
 * @details 根据就绪位图选择最高优先级任务，设置PendSV中断。
 *          实际的上下文切换在PendSV中断中完成。
 *
 * @note Context: 任务上下文或中断上下文
 * @note 必须在临界区中调用
 * @note Time complexity: O(1)
 */
void flyos_scheduler(void);

/**
 * @brief 上下文切换处理器
 * @details 由PendSV中断处理器调用，执行实际的上下文切换。
 *          保存当前任务的上下文，恢复下一个任务的上下文。
 *
 * @note Context: PendSV中断
 */
void flyos_context_switch(void);

/**
 * @brief 系统tick处理器
 * @details 在SysTick中断中调用，负责：
 *          1. 增加系统tick计数
 *          2. 更新Delta队列
 *          3. 处理时间片轮转
 *          4. 调用tick钩子函数
 *
 * @note Context: SysTick中断
 */
void flyos_systick_handler(void);

/*===========================================================================*/
/* 用户钩子函数                                                              */
/*===========================================================================*/

/**
 * @brief 空闲任务钩子
 * @details 在空闲任务循环中调用，可用于：
 *          - 进入低功耗模式
 *          - CPU使用率统计
 *          - 看门狗喂狗
 *
 *          弱符号，用户可以覆盖实现。
 *
 * @note Context: 空闲任务上下文
 * @note 保持简短，避免阻塞
 */
void flyos_idle_hook(void) FLYOS_WEAK;

/**
 * @brief Tick钩子
 * @details 每个系统tick调用一次，可用于：
 *          - 周期性任务触发
 *          - 时间戳更新
 *          - 定时器服务
 *
 *          弱符号，用户可以覆盖实现。
 *
 * @note Context: SysTick中断上下文
 * @note 必须保持极短，避免影响中断响应时间
 */
void flyos_tick_hook(void) FLYOS_WEAK;

/**
 * @brief 栈溢出钩子
 * @details 检测到栈溢出时调用，可用于：
 *          - 记录错误日志
 *          - 触发故障安全模式
 *          - 系统重启
 *
 *          弱符号，用户可以覆盖实现。
 *
 * @param[in] task 栈溢出的任务
 *
 * @note Context: 检测到栈溢出时(通常在任务切换时)
 * @note 系统处于不稳定状态，应尽快处理
 */
void flyos_stack_overflow_hook(flyos_tcb_t *task) FLYOS_WEAK;

/**
 * @brief 断言失败处理器
 * @details FLYOS_ASSERT()宏失败时调用，可用于：
 *          - 打印调试信息
 *          - 记录错误日志
 *          - 触发调试器断点
 *          - 系统复位
 *
 *          弱符号，用户可以覆盖实现。
 *
 * @param[in] file 源文件名
 * @param[in] line 行号
 *
 * @note Context: 断言失败时(可能在任意上下文)
 * @note 默认实现进入死循环
 */
void flyos_assert_failed(const char *file, int line) FLYOS_WEAK;

#ifdef __cplusplus
}
#endif

#endif /* FLYOS_KERNEL_H */
