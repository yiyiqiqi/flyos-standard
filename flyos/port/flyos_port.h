/******************************************************************************
 * @file    flyos_port.h
 * @brief   FlyOS ARM Cortex-M4/M7 硬件抽象层
 * @version 1.1.2
 * @date    2025-12-25
 * @author  yiyi
 *
 *****************************************************************************
 * @details 概述
 *
 * 本文件定义 FlyOS 实时操作系统在 ARM Cortex-M4/M7 平台上的硬件抽象层
 * (HAL)，实现内核与底层硬件之间的接口，提供平台无关的移植层。
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                  FlyOS 移植层架构                                        │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ 临界区管理        │ 中断开关、嵌套计数、BASEPRI 控制                     │
 * │ 上下文切换        │ PendSV 异常、寄存器保存/恢复                         │
 * │ 栈初始化          │ 任务栈帧结构、初始寄存器值                           │
 * │ 系统滴答          │ SysTick 配置、Tick 中断处理                          │
 * │ 中断管理          │ 优先级配置、中断嵌套、ISR 上下文判断                 │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * @section port_features 核心特性
 *
 * - 支持 ARM Cortex-M4（STM32F4 系列）
 * - 支持 ARM Cortex-M7（S32K344 等）
 * - 基于 PendSV 的上下文切换机制
 * - 使用 BASEPRI 实现临界区保护
 * - SysTick 定时器作为系统时基
 * - 支持中断嵌套和优先级管理
 * - 兼容 CMSIS 标准接口
 *
 * @section port_platforms 支持平台
 *
 * - **STM32F4xx**: STM32F429, STM32F407 等（Cortex-M4）
 * - **S32K344**: NXP S32K3 系列（Cortex-M7）
 *
 * @section port_requirements 移植要求
 *
 * 移植到新平台需要实现以下功能：
 * 1. 临界区管理（flyos_enter_critical / flyos_exit_critical）
 * 2. 栈初始化（flyos_port_stack_init）
 * 3. 上下文切换（PendSV_Handler）
 * 4. 系统滴答处理（SysTick_Handler）
 * 5. 中断优先级配置（flyos_port_init）
 *
 * @note   临界区使用 BASEPRI 寄存器，不会屏蔽所有中断
 * @note   上下文切换在 PendSV 异常中完成，优先级最低
 * @note   SysTick 中断优先级应高于 PendSV
 *
 * @warning 临界区必须成对使用，避免嵌套不匹配
 * @warning PendSV 和 SysTick 优先级配置必须正确
 *
 *****************************************************************************/

#ifndef FLYOS_PORT_H
#define FLYOS_PORT_H

#include "flyos_type.h"
#include "flyos_port_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* 导入平台定义                                                              */
/*===========================================================================*/

#if (FLYOS_PLATFORM == FLYOS_PLATFORM_S32K34X)
	/* S32K344平台头文件 */
	#include "Platform_Types.h"
	#include "Mcal.h"
	#ifdef S32K344
		#include "S32K344_SCB.h"
		#include "S32K344_NVIC.h"
		#include "S32K344_SYSTICK.h"
		#include "S32K344_MSCM.h"
	#endif
#elif (FLYOS_PLATFORM == FLYOS_PLATFORM_STM32F4)
	/* STM32F4平台头文件 */
	/* 包含 STM32 HAL，提供 CMSIS 定义 */
	#include "stm32f4xx.h"

#else
	#error "flyos_port.h不支持的平台"
#endif

/*===========================================================================*/
/* 类CMSIS内联函数                                                           */
/*===========================================================================*/

#if (FLYOS_PLATFORM == FLYOS_PLATFORM_S32K34X)
/* S32K344平台需要自定义CMSIS函数 */

/**
 * @brief 获取PRIMASK寄存器值
 * @return PRIMASK值
 */
FLYOS_INLINE uint32_t __get_PRIMASK(void)
{
	uint32_t result;
	__asm volatile("MRS %0, primask" : "=r" (result));
	return result;
}

/**
 * @brief 设置PRIMASK寄存器值
 * @param pri_mask 要设置的PRIMASK值
 */
FLYOS_INLINE void __set_PRIMASK(uint32_t pri_mask)
{
	__asm volatile("MSR primask, %0" : : "r" (pri_mask) : "memory");
}

/**
 * @brief 禁用所有中断
 */
FLYOS_INLINE void __disable_irq(void)
{
	__asm volatile("cpsid i" : : : "memory");
}

/**
 * @brief 使能所有中断
 */
FLYOS_INLINE void __enable_irq(void)
{
	__asm volatile("cpsie i" : : : "memory");
}

#elif (FLYOS_PLATFORM == FLYOS_PLATFORM_STM32F4)
/* STM32F4平台使用 CMSIS 提供的函数，无需重复定义 */
/* __get_PRIMASK, __set_PRIMASK, __disable_irq, __enable_irq 已由 stm32f4xx.h 提供 */

#endif

/*===========================================================================*/
/* 移植层配置                                                                */
/*===========================================================================*/

/**
 * @brief 系统调用中断的BASEPRI阈值
 * @note 优先级大于此值的中断在临界区中被禁用
 */
#ifndef configMAX_SYSCALL_INTERRUPT_PRIORITY
	#define configMAX_SYSCALL_INTERRUPT_PRIORITY    0xB0
#endif

/**
 * @brief 栈增长方向（-1 = 向下增长）
 */
#define FLYOS_STACK_GROWTH              (-1)

/**
 * @brief 字节对齐要求
 */
#define FLYOS_BYTE_ALIGNMENT            8

/**
 * @brief FLYOS的SVC编号
 */
#define FLYOS_SVC_START_SCHEDULER       0xFF
#define FLYOS_SVC_YIELD                 0xFE

/*===========================================================================*/
/* 移植层宏                                                                  */
/*===========================================================================*/

/**
 * @brief 使用BASEPRI禁用中断
 * @note 仅禁用优先级不高于configMAX_SYSCALL_INTERRUPT_PRIORITY的中断
 */
#define flyos_port_disable_interrupts()    v_port_raise_basepri()

/**
 * @brief 使能中断
 */
#define flyos_port_enable_interrupts()     v_port_set_basepri(0)

/**
 * @brief 使用PendSV触发上下文切换
 */
#define portNVIC_INT_CTRL_REG          (*((volatile uint32_t *) 0xE000ED04))
#define portNVIC_PENDSVSET_BIT         (1UL << 28UL)

#define flyos_port_yield()                do { \
										   portNVIC_INT_CTRL_REG = portNVIC_PENDSVSET_BIT; \
										   __asm volatile("dsb" ::: "memory"); \
										   __asm volatile("isb"); \
									   } while (0)

/**
 * @brief 内存屏障
 */
#define flyos_port_memory_barrier()        __asm volatile("dmb" ::: "memory")
#define flyos_port_instruction_barrier()   __asm volatile("isb")
#define flyos_port_data_barrier()          __asm volatile("dsb" ::: "memory")

/*===========================================================================*/
/* 移植层函数                                                                */
/*===========================================================================*/

/**
 * @brief 初始化移植层硬件
 * @details 配置ARM Cortex-M处理器的硬件特性，为RTOS运行做准备。
 *
 *          配置内容：
 *          1. 配置中断优先级(PendSV、SysTick、SVCall)
 *          2. 配置FPU浮点单元(如果启用FLYOS_USE_FPU)
 *          3. 配置缓存(如果启用且平台支持)
 *          4. 配置TCM紧耦合存储器(如果可用)
 *
 * @note Context: 系统初始化阶段，在flyos_kernel_init()中调用
 * @note 必须在启动调度器之前调用
 */
void flyos_port_init(void);

/**
 * @brief 初始化任务栈
 * @details 在栈缓冲区中创建初始栈帧，使任务可以被调度执行。
 *          栈帧包含ARM Cortex-M异常返回时所需的寄存器值。
 *
 *          栈帧布局(从高地址到低地址):
 *          +------------------+
 *          | Stack Guard      |  <- FLYOS_TASK_STACK_MAGIC_TOP(如果启用)
 *          +------------------+
 *          | xPSR (0x01000000)|  <- 异常帧(硬件自动保存)
 *          | PC (entry point) |
 *          | LR (exit handler)|
 *          | R12              |
 *          | R3               |
 *          | R2               |
 *          | R1               |
 *          | R0 (param)       |
 *          +------------------+
 *          | EXC_RETURN       |  <- 0xFFFFFFFD(线程模式，PSP)
 *          +------------------+
 *          | R11              |  <- 软件帧(手动保存)
 *          | R10              |
 *          | R9               |
 *          | R8               |
 *          | R7               |
 *          | R6               |
 *          | R5               |
 *          | R4               |  <- 初始SP指向这里
 *          +------------------+
 *          | Stack Guard      |  <- FLYOS_TASK_STACK_MAGIC_BOTTOM(如果启用)
 *          +------------------+
 *
 *          FPU使用惰性上下文保存，硬件在首次使用FPU指令时
 *          自动保存FPU寄存器(S0-S15、FPSCR)
 *
 * @param[in] stack_buffer  指向栈缓冲区的指针
 * @param[in] stack_size    栈大小(字节)
 * @param[in] entry         任务入口函数指针
 * @param[in] param         传递给任务的参数
 *
 * @return 初始化后的栈指针值(指向栈帧底部)
 *
 * @note 栈缓冲区必须8字节对齐
 * @note 栈向下增长(栈顶在低地址)
 */
void* flyos_port_init_stack(void *stack_buffer, flyos_u32_t stack_size,
						void (*entry)(void*), void *param);

/**
 * @brief 启动调度器
 * @details 配置SysTick定时器并启动第一个任务，该函数永不返回。
 *
 *          执行步骤：
 *          1. 配置SysTick定时器(按FLYOS_TICK_RATE_HZ频率)
 *          2. 设置SysTick中断优先级
 *          3. 加载第一个任务的栈指针
 *          4. 恢复第一个任务的上下文
 *          5. 跳转到第一个任务执行
 *
 * @note Context: 系统初始化阶段，在flyos_kernel_start()中调用
 * @note 此函数永不返回，标记为FLYOS_NORETURN
 * @note 调用此函数后，系统进入多任务模式
 */
void flyos_port_start_scheduler(void) FLYOS_NORETURN;

/**
 * @brief 进入临界区
 * @details 通过提升BASEPRI寄存器来禁用系统调用中断，保护临界区代码。
 *          支持嵌套调用，可在ISR和任务上下文中安全使用。
 *
 *          工作原理：
 *          - 禁用优先级不高于configMAX_SYSCALL_INTERRUPT_PRIORITY的中断
 *          - 高优先级中断(如紧急故障处理)仍可响应
 *          - 维护嵌套计数器
 *
 * @return 进入临界区前的BASEPRI寄存器值，用于退出时恢复
 *
 * @note Context: 任务上下文或中断上下文
 * @note 必须与flyos_exit_critical()成对使用
 * @note 临界区应尽可能短，避免影响实时性
 */
flyos_base_t flyos_enter_critical(void);

/**
 * @brief 退出临界区
 * @details 恢复BASEPRI寄存器值，重新使能中断。
 *          必须与flyos_enter_critical()成对使用。
 *
 * @param[in] level  flyos_enter_critical()返回的之前的BASEPRI值
 *
 * @note Context: 任务上下文或中断上下文
 * @note 必须在对应的flyos_enter_critical()之后调用
 * @note level参数必须是flyos_enter_critical()的返回值
 */
void flyos_exit_critical(flyos_base_t level);

/**
 * @brief 从ISR进入临界区
 * @return 之前的pri_mask值
 * @note 在ISR上下文中使用，禁用所有中断
 */
FLYOS_INLINE flyos_base_t flyos_enter_critical_from_isr(void)
{
	uint32_t pri_mask = __get_PRIMASK();
	__disable_irq();
	return pri_mask;
}

/**
 * @brief 从ISR退出临界区
 * @param level flyos_enter_critical_from_isr()返回的之前的pri_mask值
 */
FLYOS_INLINE void flyos_exit_critical_from_isr(flyos_base_t level)
{
	__set_PRIMASK(level);
}

/**
 * @brief 获取临界区嵌套深度
 * @details 返回当前临界区的嵌套层数。
 *          0表示未处于临界区，>0表示嵌套深度。
 *
 * @return 当前临界区嵌套深度
 *
 * @note Context: 任意上下文
 * @note 主要用于调试和诊断
 */
flyos_u32_t flyos_get_critical_nesting(void);

/*===========================================================================*/
/* 外部变量                                                                   */
/*===========================================================================*/

/**
 * @brief 当前TCB指针
 * @note 被汇编代码使用，以实现快速上下文切换
 */
extern volatile void *volatile px_current_tcb;

/*===========================================================================*/
/* 内联移植层函数                                                            */
/*===========================================================================*/

/**
 * @brief 提升BASEPRI以禁用中断，禁用RTOS系统调用中断
 * @note 内联以提高性能
 */
FLYOS_INLINE void v_port_raise_basepri(void)
{
	uint32_t ul_new_basepri;

	__asm volatile(
		"   mov %0, %1                                  \n"
		"   cpsid i                                     \n"
		"   msr basepri, %0                             \n"
		"   isb                                         \n"
		"   dsb                                         \n"
		"   cpsie i                                     \n"
		: "=r" (ul_new_basepri)
		: "i" (configMAX_SYSCALL_INTERRUPT_PRIORITY)
		: "memory"
	);
}

/**
 * @brief 设置BASEPRI值
 * @param ul_new_mask_value 新的BASEPRI值（0 = 使能所有中断）
 */
FLYOS_INLINE void v_port_set_basepri(uint32_t ul_new_mask_value)
{
	__asm volatile(
		"   msr basepri, %0 "
		:: "r" (ul_new_mask_value)
		: "memory"
	);
}

/**
 * @brief 读取并提升BASEPRI
 * @return 原BASEPRI值
 */
FLYOS_INLINE uint32_t ul_port_raise_basepri(void)
{
	uint32_t ul_original_basepri, ul_new_basepri;

	__asm volatile(
		"   mrs %0, basepri                             \n"
		"   mov %1, %2                                  \n"
		"   cpsid i                                     \n"
		"   msr basepri, %1                             \n"
		"   isb                                         \n"
		"   dsb                                         \n"
		"   cpsie i                                     \n"
		: "=r" (ul_original_basepri), "=r" (ul_new_basepri)
		: "i" (configMAX_SYSCALL_INTERRUPT_PRIORITY)
		: "memory"
	);

	return ul_original_basepri;
}

#ifdef __cplusplus
}
#endif

#endif /* FLYOS_PORT_H */
