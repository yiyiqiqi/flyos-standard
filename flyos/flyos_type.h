/******************************************************************************
 * @file    flyos_type.h
 * @brief   FlyOS 基础类型定义与系统常量
 * @version 1.0.0
 * @date    2024
 *
 * @author  yiyi
 *
 *****************************************************************************
 * @details 概述
 *
 * 本文件定义 FlyOS 实时操作系统中使用的所有基础数据类型、枚举常量和
 * 宏定义，为不同硬件平台提供统一的类型抽象层，确保代码的可移植性。
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                     FlyOS 类型系统                                       │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ 基础整数类型      │ u8/s8, u16/s16, u32/s32, u64/s64                     │
 * │ 易失性类型        │ vu8, vu16, vu32（用于硬件寄存器访问）                │
 * │ 系统类型          │ tick_t, err_t, tcb_t, base_t                         │
 * │ 枚举类型          │ 任务状态、IPC 类型、错误码                            │
 * │ 配置常量          │ 优先级、超时、魔术数字                               │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * @section type_features 核心特性
 *
 * - 跨平台统一类型定义，基于标准 C99 stdint.h
 * - 明确的类型大小和符号性，避免歧义
 * - 易失性类型支持，适用于硬件寄存器操作
 * - 完整的错误码枚举，便于错误处理
 * - 任务控制块（TCB）结构定义
 * - 系统版本信息宏
 *
 * @section type_naming 命名规范
 *
 * - flyos_u<n>_t  : 无符号 n 位整数（n = 8, 16, 32, 64）
 * - flyos_s<n>_t  : 有符号 n 位整数
 * - flyos_v<u/s><n>_t : 易失性整数类型
 * - flyos_<name>_t : 系统专用类型
 * - FLYOS_<NAME> : 系统常量宏
 *
 * @note   所有 FlyOS 类型均以 flyos_ 为前缀，避免命名冲突
 * @note   易失性类型应用于访问硬件寄存器或中断共享变量
 *
 *****************************************************************************/

#ifndef FLYOS_TYPE_H
#define FLYOS_TYPE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 版本信息 */
#define FLYOS_VERSION_MAJOR      1
#define FLYOS_VERSION_MINOR      1
#define FLYOS_VERSION_PATCH      0

/* 基本类型 */

/* 无符号8位整数 */
typedef uint8_t             flyos_u8_t;
/* 有符号8位整数 */
typedef int8_t              flyos_s8_t;
/* 无符号16位整数 */
typedef uint16_t            flyos_u16_t;
/* 有符号16位整数 */
typedef int16_t             flyos_s16_t;
/* 无符号32位整数 */
typedef uint32_t            flyos_u32_t;
/* 有符号32位整数 */
typedef int32_t             flyos_s32_t;
/* 无符号64位整数 */
typedef uint64_t            flyos_u64_t;
/* 有符号64位整数 */
typedef int64_t             flyos_s64_t;

/* 易失性无符号8位整数 */
typedef volatile uint8_t    flyos_vu8_t;
/* 易失性无符号16位整数 */
typedef volatile uint16_t   flyos_vu16_t;
/* 易失性无符号32位整数 */
typedef volatile uint32_t   flyos_vu32_t;

/* 32位浮点数 */
typedef float               flyos_f32_t;
/* 64位浮点数 */
typedef double              flyos_f64_t;

/* 通用句柄类型 */
typedef void*               flyos_handle_t;
/* 系统tick计数器类型 */
typedef uint32_t            flyos_tick_t;
/* 中断优先级类型 */
typedef uint32_t            flyos_base_t;
/* 错误码类型 */
typedef int32_t             flyos_err_t;

/* 错误码 */

/**
 * enum flyos_error_t - FlyOS错误码定义
 * @FLYOS_OK: 操作成功
 * @FLYOS_ERROR: 通用错误
 * @FLYOS_TIMEOUT: 操作超时
 * @FLYOS_BUSY: 资源忙
 * @FLYOS_FULL: 队列/缓冲区满
 * @FLYOS_EMPTY: 队列/缓冲区空
 * @FLYOS_NOMEM: 内存不足
 * @FLYOS_INVALID: 无效参数
 * @FLYOS_NOT_READY: 未就绪
 * @FLYOS_PERMISSION: 权限拒绝
 * @FLYOS_STACK_OVERFLOW: 检测到栈溢出
 * @FLYOS_EPERM: 操作不允许(不是锁的owner)
 * @FLYOS_EINTR: 被中断
 * @FLYOS_EAGAIN: 重试(非阻塞操作失败)
 * @FLYOS_EDEADLK: 死锁检测
 */
typedef enum {
	FLYOS_OK                 = 0,
	FLYOS_ERROR             = -1,
	FLYOS_TIMEOUT           = -2,
	FLYOS_BUSY              = -3,
	FLYOS_FULL              = -4,
	FLYOS_EMPTY             = -5,
	FLYOS_NOMEM             = -6,
	FLYOS_INVALID           = -7,
	FLYOS_NOT_READY         = -8,
	FLYOS_PERMISSION        = -9,
	FLYOS_STACK_OVERFLOW    = -10,
	FLYOS_EPERM             = -11,
	FLYOS_EINTR             = -12,
	FLYOS_EAGAIN            = -13,
	FLYOS_EDEADLK           = -14
} flyos_error_t;

/* 任务状态 */

/**
 * enum flyos_task_state_t - 任务状态枚举
 * @FLYOS_TASK_INIT: 任务已初始化
 * @FLYOS_TASK_READY: 任务就绪(包含运行中)
 * @FLYOS_TASK_BLOCK: 任务阻塞(统一所有阻塞情况)
 * @FLYOS_TASK_CLOSE: 任务已关闭/删除
 */
typedef enum {
	FLYOS_TASK_INIT         = 0x00,
	FLYOS_TASK_READY        = 0x01,
	FLYOS_TASK_BLOCK        = 0x02,
	FLYOS_TASK_CLOSE        = 0x04
} flyos_task_state_t;

/**
 * enum flyos_block_reason_t - 阻塞原因枚举(可组合)
 * @FLYOS_BLOCK_NONE: 未阻塞
 * @FLYOS_BLOCK_DELAY: 延迟阻塞
 * @FLYOS_BLOCK_IPC: IPC阻塞(信号量/互斥锁/队列/事件)
 * @FLYOS_BLOCK_SUSPEND: 挂起阻塞
 */
typedef enum {
	FLYOS_BLOCK_NONE        = 0x00,
	FLYOS_BLOCK_DELAY       = 0x01,
	FLYOS_BLOCK_IPC         = 0x02,
	FLYOS_BLOCK_SUSPEND     = 0x04
} flyos_block_reason_t;

/* 超时值 */

/* 不等待超时 */
#define FLYOS_NO_WAIT           0
/* 永久等待超时 */
#define FLYOS_WAIT_FOREVER      0xFFFFFFFF

/* 编译器属性 */

/* 强制内联函数 */
#define FLYOS_INLINE            static inline
/* 弱符号(可被覆盖) */
#define FLYOS_WEAK              __attribute__((weak))
/* 函数永不返回 */
#define FLYOS_NORETURN          __attribute__((noreturn))
/* 紧凑结构体 */
#define FLYOS_PACKED            __attribute__((packed))
/* 对齐到指定边界 */
#define FLYOS_ALIGNED(x)        __attribute__((aligned(x)))
/* 放置在指定段 */
#define FLYOS_SECTION(x)        __attribute__((section(x)))
/* 标记参数未使用 */
#define FLYOS_UNUSED(x)         ((void)(x))

/* 实用宏 */

/**
 * FLYOS_CONTAINER_OF - 从成员指针获取容器结构体
 * @ptr: 成员指针
 * @type: 容器结构体类型
 * @member: 成员名称
 */
#define FLYOS_CONTAINER_OF(ptr, type, member) \
	((type *)((char *)(ptr) - offsetof(type, member)))

/* 获取两个值的最小值 */
#define FLYOS_MIN(a, b)         ((a) < (b) ? (a) : (b))
/* 获取两个值的最大值 */
#define FLYOS_MAX(a, b)         ((a) > (b) ? (a) : (b))
/* 获取绝对值 */
#define FLYOS_ABS(x)            ((x) < 0 ? -(x) : (x))

/* 获取数组大小 */
#define FLYOS_ARRAY_SIZE(arr)   (sizeof (arr) / sizeof ((arr)[0]))

/* 创建位掩码 */
#define FLYOS_BIT(n)            (1UL << (n))
/* 设置位 */
#define FLYOS_BIT_SET(x, n)     ((x) |= FLYOS_BIT(n))
/* 清除位 */
#define FLYOS_BIT_CLEAR(x, n)   ((x) &= ~FLYOS_BIT(n))
/* 测试位 */
#define FLYOS_BIT_TEST(x, n)    ((x) & FLYOS_BIT(n))

/* 分支预测提示(编译器优化) */

/**
 * likely - 分支预测: 表达式很可能为真
 * @x: 要判断的表达式
 *
 * 用于优化热路径代码，减少分支预测失败。
 */
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif

/**
 * unlikely - 分支预测: 表达式很可能为假
 * @x: 要判断的表达式
 *
 * 用于标记错误处理和异常情况。
 */
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

/* IPC类型定义 */

/**
 * enum flyos_ipc_type_t - IPC对象类型标识
 * @FLYOS_IPC_TYPE_NONE: 无类型
 * @FLYOS_IPC_TYPE_SEMAPHORE: 信号量 "SEM\0"
 * @FLYOS_IPC_TYPE_MUTEX: 互斥锁 "MUT\0"
 * @FLYOS_IPC_TYPE_QUEUE: 消息队列 "QUE\0"
 * @FLYOS_IPC_TYPE_EVENT: 事件标志 "EVT\0"
 * @FLYOS_IPC_TYPE_MAILBOX: 邮箱 "MBO\0"
 *
 * 使用4字节ASCII魔术数字，方便内存调试。
 */
typedef enum {
	FLYOS_IPC_TYPE_NONE      = 0x00000000,
	FLYOS_IPC_TYPE_SEMAPHORE = 0x53454D00,
	FLYOS_IPC_TYPE_MUTEX     = 0x4D555400,
	FLYOS_IPC_TYPE_QUEUE     = 0x51554500,
	FLYOS_IPC_TYPE_EVENT     = 0x45565400,
	FLYOS_IPC_TYPE_MAILBOX   = 0x4D424F00
} flyos_ipc_type_t;

/**
 * IPC对象魔术数字
 *
 * 用于检测野指针和内存损坏: 0xF1E05A5A = "FLYOS_AS"
 */
#define FLYOS_IPC_MAGIC         0xF1E05A5A

/*===========================================================================*/
/* IPC对象和任务标志位定义                                                    */
/*===========================================================================*/

/**
 * IPC对象标志位
 * @FLYOS_IPC_FLAG_STATIC: 对象静态创建
 * @FLYOS_IPC_FLAG_QUEUE_BUFFER_STATIC: 队列缓冲区静态创建
 */
#define FLYOS_IPC_FLAG_STATIC               0x01
#define FLYOS_IPC_FLAG_QUEUE_BUFFER_STATIC  0x02

/**
 * 任务标志位
 * @FLYOS_TASK_FLAG_TCB_STATIC: TCB是静态分配的
 * @FLYOS_TASK_FLAG_STACK_DYNAMIC: 栈是动态分配的
 */
#define FLYOS_TASK_FLAG_TCB_STATIC          0x01
#define FLYOS_TASK_FLAG_STACK_DYNAMIC       0x02

#ifdef __cplusplus
}
#endif

#endif /* FLYOS_TYPE_H */
