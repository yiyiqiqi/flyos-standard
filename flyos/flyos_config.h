/******************************************************************************
 * @file    flyos_config.h
 * @brief   FlyOS 系统配置中心
 * @version 1.0.0
 * @date    2024
 *
 * @author  yiyi
 *
 *****************************************************************************
 * @details 概述
 *
 * 本文件是 FlyOS 实时操作系统的核心配置文件，包含所有 RTOS 级别的
 * 编译时配置参数。用户可根据应用需求调整系统行为和功能特性。
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    FlyOS 配置模块                                        │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ 调度器配置        │ 优先级数量、时间片、抢占开关                         │
 * │ 内存配置          │ 堆大小、动态栈、内存统计                             │
 * │ IPC 配置          │ 信号量、互斥锁、队列、事件、邮箱                     │
 * │ 功能开关          │ 调试、断言、命名、统计                               │
 * │ 性能调优          │ 超时精度、优先级继承深度                             │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * @section config_features 配置类别
 *
 * - **调度器配置**: 优先级级数、Tick 频率、时间片大小
 * - **内存管理配置**: 堆大小、动态栈支持、内存统计开关
 * - **IPC 功能配置**: 各类 IPC 机制的启用/禁用
 * - **调试配置**: 断言、对象命名、运行时检查
 * - **钩子函数配置**: 空闲钩子、Tick 钩子、任务切换钩子
 *
 * @section config_usage 使用说明
 *
 * 1. 根据应用需求修改本文件中的宏定义
 * 2. 平台相关配置请修改 flyos_port_config.h
 * 3. 修改配置后需重新编译整个系统
 * 4. 建议在项目初期确定配置，避免频繁变更
 *
 * @note   本文件包含平台无关配置，平台配置位于 flyos_port_config.h
 * @note   部分配置会影响内存占用和性能，请仔细评估
 * @note   优先级数量必须是 2 的幂次方（8, 16, 32）
 *
 * @warning 修改配置后必须重新编译所有源文件
 * @warning 运行时不可修改这些配置，它们是编译时常量
 *
 *****************************************************************************/

#ifndef FLYOS_CONFIG_H
#define FLYOS_CONFIG_H

/* ==============================平台配置引入================================ */

#include "flyos_port_config.h"

/* ===============================调度器配置================================= */

/**
 * @brief 优先级级别总数
 *
 * 有效优先级范围: 0 ~ (FLYOS_NUM_PRIORITY_LEVELS - 1)
 * 必须是2的幂次方，用于位图优化。
 */
#define FLYOS_NUM_PRIORITY_LEVELS           32

/**
 * @brief 最大优先级值，数值越小，优先级越高
 *
 * 最高优先级 = 0，最低优先级 = FLYOS_MAX_PRIORITY_VALUE
 */
#define FLYOS_MAX_PRIORITY_VALUE            (FLYOS_NUM_PRIORITY_LEVELS - 1)

/**
 * @brief 空闲任务优先级(最低优先级)
 */
#define FLYOS_IDLE_PRIORITY                 (FLYOS_NUM_PRIORITY_LEVELS - 1)

/**
 * @brief 系统tick频率(Hz)
 */
#define FLYOS_TICK_RATE_HZ                  1000

/**
 * @brief 时间片轮转调度的默认时间片(tick数)
 */
#define FLYOS_TIME_SLICE_TICKS              2

/**
 * @brief 空闲任务栈大小(字节)
 */
#define FLYOS_IDLE_STACK_SIZE               512

/**
 * @brief 启用抢占式调度
 */
#define FLYOS_USE_PREEMPTION                1

/* @brief 启用时间片轮转调度 */
#define FLYOS_USE_TIME_SLICING              1

/* =================================任务配置=================================== */

/**
 * @brief 任务名称最大长度，包含字符串结束符
 */
#define FLYOS_MAX_TASK_NAME                 16

/**
 * @brief 栈保护魔术数字，用于溢出检测
 *
 * 底部魔术数字在最低地址，顶部魔术数字在最高地址。
 */
#define FLYOS_TASK_STACK_MAGIC_BOTTOM       0xDEADC0DE
#define FLYOS_TASK_STACK_MAGIC_TOP          0xC0DEC0DE

/**
 * @brief 启用任务统计信息收集
 */
#define FLYOS_USE_TASK_STAT                 1

/**
 * @brief 启用栈溢出检查
 */
#define FLYOS_USE_STACK_CHECK               1

/* =================================IPC配置==================================== */

/**
 * @brief 启用信号量支持
 */
#define FLYOS_USE_SEMAPHORE                 1

/**
 * @brief 启用互斥锁支持
 */
#define FLYOS_USE_MUTEX                     1

/**
 * @brief 启用消息队列支持
 */
#define FLYOS_USE_QUEUE                     1

/**
 * @brief 启用事件标志支持
 */
#define FLYOS_USE_EVENT                     1

/**
 * @brief 启用邮箱支持
 */
#define FLYOS_USE_MAILBOX                   1

/**
 * @brief 启用IPC对象名称支持
 */
#define FLYOS_USE_IPC_NAME                  0

/**
 * @brief 启用互斥锁优先级继承，避免优先级反转
 */
#define FLYOS_USE_PRIORITY_INHERIT          1

/**
 * @brief 启用链式优先级继承
 */
#define FLYOS_PRIORITY_INHERIT_CHAIN        1

/**
 * @brief 优先级继承链最大深度，防止死锁，超过此深度停止继承
 */
#define FLYOS_MAX_INHERIT_DEPTH             8

/**
 * @brief 启用递归互斥锁
 */
#define FLYOS_USE_RECURSIVE_MUTEX           1

/**
 * @brief 队列内存优化模式
 *
 * 0 = 双等待队列(默认，性能最佳)
 * 1 = 单等待队列(节省132字节内存)
 */
#define FLYOS_QUEUE_MEMORY_MODE             0

/* ===============================内存管理配置================================== */

/**
 * @brief 启用动态内存分配
 *
 * 启用后支持：
 * - TCB动态分配；
 * - 栈动态分配；
 * - IPC对象动态分配；
 *
 * 禁用时，所有 buffer 参数必须由用户提供静态缓冲区，为非NULL
 */
#define FLYOS_USE_HEAP                      1

/**
 * @brief 堆大小64KB
 */
#define FLYOS_HEAP_SIZE                     (64 * 1024)

/**
 * @brief 内存对齐字节数
 */
#define FLYOS_MEM_ALIGNMENT                 8

/**
 * @brief 启用内存统计跟踪
 */
#define FLYOS_USE_MEM_STATS                 1

/**
 * @brief 启用内存损坏检查
 */
#define FLYOS_USE_MEM_CHECK                 1

/* =================================调试配置=================================== */

/**
 * @brief 启用断言
 */
#define FLYOS_USE_ASSERT                    1

/* @brief 断言宏 */
#if FLYOS_USE_ASSERT
	/**
	 * FLYOS_ASSERT - 运行时检查断言宏
	 * @expr: 要检查的表达式
	 */
	#define FLYOS_ASSERT(expr) \
		do { \
			if (!(expr)) { \
				flyos_assert_failed(__FILE__, __LINE__); \
			} \
		} while (0)
#else
	#define FLYOS_ASSERT(expr) ((void)0)
#endif

/**
 * @brief 启用CPU使用率计算
 */
#define FLYOS_USE_CPU_USAGE                 1

/* ==============================安全特性配置================================== */

/**
 * @brief 启用任务看门狗监控
 */
#define FLYOS_USE_TASK_WATCHDOG             1

/**
 * @brief 看门狗检查间隔
 *
 * 每100ms检查一次，对调度效率影响<5%。
 */
#define FLYOS_WATCHDOG_CHECK_INTERVAL       100

/**
 * @brief 启用栈监控
 */
#define FLYOS_USE_STACK_MONITOR             1

/**
 * @brief 栈监控检查间隔
 */
#define FLYOS_STACK_CHECK_INTERVAL          1000

/**
 * @brief 栈溢出警告阈值
 *
 * 栈使用超过此百分比时触发警告。
 */
#define FLYOS_STACK_WARNING_THRESHOLD       90

/**
 * @brief 启用通用看门狗监控
 */
#define FLYOS_USE_WATCHDOG                  1

/**
 * @brief 触发故障安全前的最大连续故障数
 */
#define FLYOS_MAX_FAULTS                    3

/**
 * @brief 心跳超时时间
 */
#define FLYOS_HEARTBEAT_TIMEOUT_MS          100

#endif /* FLYOS_CONFIG_H */
