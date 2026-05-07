/**
 * @file flyos_port.c
 * @brief S32K344的FlyOS移植层实现
 * @version 1.0.0
 * @author  yiyi
 * @date    2025-12-25
 * @details 概述
 *          本文件实现S32K344（ARM Cortex-M7）平台下的FlyOS移植层代码。
 *          包括临界区管理、任务栈初始化、移植层硬件初始化等。
 */

#include "../flyos_port.h"
#include "../../flyos_kernel.h"

/*===========================================================================*/
/* 当前TCB指针                                                               */
/*===========================================================================*/

volatile void *volatile px_current_tcb = NULL;

/*===========================================================================*/
/* 临界区实现                                                                */
/*===========================================================================*/

/**
 * @brief 临界区嵌套计数器
 */
static volatile uint32_t ux_critical_nesting = 0;

/**
 * @brief 进入临界区
 */
flyos_base_t flyos_enter_critical(void)
{
	/* 在优先级阈值处禁用中断 */
	uint32_t level = ul_port_raise_basepri();

	/* 递增嵌套计数器 */
	ux_critical_nesting ++;

	/* 返回之前的中断状态 */
	return level;
}

/**
 * @brief 退出临界区
 */
void flyos_exit_critical(flyos_base_t level)
{
	/* 验证嵌套深度 */
	if (ux_critical_nesting == 0) {
#if FLYOS_USE_ASSERT
		flyos_assert_failed(__FILE__, __LINE__);
#endif
		return;
	}

	/* 递减嵌套计数器 */
	ux_critical_nesting --;

	/* 仅在完全退出嵌套时恢复中断 */
	if (ux_critical_nesting == 0) {
		v_port_set_basepri(level);
	}
}

/**
 * @brief 获取临界区嵌套深度
 */
flyos_u32_t flyos_get_critical_nesting(void)
{
	return ux_critical_nesting;
}

/*===========================================================================*/
/* 移植层初始化                                                              */
/*===========================================================================*/

/**
 * @brief 初始化移植层硬件
 */
void flyos_port_init(void)
{
	/* 设置PendSV为最低优先级（0xFF） */
	*(volatile uint32_t *)0xE000ED20 |= (0xFFUL << 16);

	/* 设置SysTick为最低优先级（0xFF） */
	*(volatile uint32_t *)0xE000ED20 |= (0xFFUL << 24);

	/* 设置SVCall为最高优先级（0x00） */
	*(volatile uint32_t *)0xE000ED1C = 0;

	/* 如果配置了FPU，则启用 */
#if FLYOS_FPU_ENABLE
	S32_SCB->CPACR |= (0xFUL << 20); /* CP10/CP11完全访问 */
	MCAL_DATA_SYNC_BARRIER();
	MCAL_INSTRUCTION_SYNC_BARRIER();
#endif

	/* 清除当前TCB指针 */
	px_current_tcb = NULL;
	ux_critical_nesting = 0;
}

/*===========================================================================*/
/* 栈初始化                                                                  */
/*===========================================================================*/

/**
 * @brief 任务退出错误处理
 * @note 任务不应返回，此函数捕获编程错误
 */
void prv_task_exit_error(void)
{
	volatile uint32_t ul_dummy = 0;

	/* 防止编译器警告 */
	(void) ul_dummy;

	/* 在此挂起 */
	flyos_port_disable_interrupts();
	while (1) {
		ul_dummy++;
	}
}

/**
 * @brief 初始化任务栈
 */
void* flyos_port_init_stack(void *stack_buffer, flyos_u32_t stack_size,
						void (*entry)(void*), void *param)
{
	uint32_t *px_top_of_stack = (uint32_t*)((uint8_t*)stack_buffer + stack_size);

#if FLYOS_USE_STACK_CHECK
	/* 设置顶部保护魔数（在最高地址） */
	px_top_of_stack--;
	*px_top_of_stack = FLYOS_TASK_STACK_MAGIC_TOP;
#endif

	/* 对齐栈指针到8字节（AAPCS要求） */
	px_top_of_stack = (uint32_t*)(((uint32_t)px_top_of_stack) & ~0x7UL);
	px_top_of_stack--;

	/* -------------------- 硬件异常帧（Cortex-M自动保存） --------------------- */

	/* xPSR - 设置Thumb位（位24） */
	*px_top_of_stack = 0x01000000UL;
	px_top_of_stack--;

	/* PC - 任务入口点（清除LSB以适应Thumb模式） */
	*px_top_of_stack = ((uint32_t)entry) & 0xFFFFFFFEUL;
	px_top_of_stack--;

	/* LR - 任务退出错误处理 */
	*px_top_of_stack = (uint32_t)prv_task_exit_error;
	px_top_of_stack--;

	/* R12 */
	*px_top_of_stack = 0x12121212UL;
	px_top_of_stack--;

	/* R3 */
	*px_top_of_stack = 0x03030303UL;
	px_top_of_stack--;

	/* R2 */
	*px_top_of_stack = 0x02020202UL;
	px_top_of_stack--;

	/* R1 */
	*px_top_of_stack = 0x01010101UL;
	px_top_of_stack--;

	/* R0 - 任务参数 */
	*px_top_of_stack = (uint32_t)param;
	px_top_of_stack--;

    /* ARM Cortex-M7使用惰性堆栈，硬件将在首次使用时自动保存/恢复FPU上下文，无需手动预留FPU帧空间 */

	/* === EXC_RETURN值 === */

	*px_top_of_stack = 0xFFFFFFFDUL;  /* 线程模式，PSP，初始无FPU帧 */
	px_top_of_stack--;

	/* ----------------------- 软件帧（需手动保存） ----------------------- */

	/* 将R4-R11初始化为已知值以便调试 */
	*px_top_of_stack = 0x11111111UL;  /* R11 */
	px_top_of_stack--;
	*px_top_of_stack = 0x10101010UL;  /* R10 */
	px_top_of_stack--;
	*px_top_of_stack = 0x09090909UL;  /* R9 */
	px_top_of_stack--;
	*px_top_of_stack = 0x08080808UL;  /* R8 */
	px_top_of_stack--;
	*px_top_of_stack = 0x07070707UL;  /* R7 */
	px_top_of_stack--;
	*px_top_of_stack = 0x06060606UL;  /* R6 */
	px_top_of_stack--;
	*px_top_of_stack = 0x05050505UL;  /* R5 */
	px_top_of_stack--;
	*px_top_of_stack = 0x04040404UL;  /* R4 */

	/* 返回初始栈指针 */
	return px_top_of_stack;
}

/*===========================================================================*/
/* 调度器启动                                                                */
/*===========================================================================*/

/**
 * @brief 启动调度器
 */
void flyos_port_start_scheduler(void)
{
	/* 配置SysTick为1ms滴答 */
	uint32_t ulReloadValue = (FLYOS_CPU_CLOCK_HZ / FLYOS_TICK_RATE_HZ) - 1UL;

	/* SysTick寄存器 */
	*(volatile uint32_t *)0xE000E014 = 0UL;            /* 清除当前值 */
	*(volatile uint32_t *)0xE000E018 = 0UL;            /* 清除重载值 */
	*(volatile uint32_t *)0xE000E014 = ulReloadValue;  /* 设置重载值 */
	*(volatile uint32_t *)0xE000E010 = 0x07UL;         /* 使能SysTick：CLK_SRC=1，INT=1，ENABLE=1 */

	/* 验证px_current_tcb已设置 */
	if (px_current_tcb == NULL) {
		while (1);  /* 致命错误 */
	}

	/* 使用SVC启动第一个任务 */
	__asm volatile(
		"   ldr r0, =0xE000ED08             \n" /* 加载VTOR寄存器的物理地址到r0 */
		"   ldr r0, [r0]                    \n" /* 读取VTOR寄存器的值，即向量表的起始基地址 */
		"   ldr r0, [r0]                    \n" /* 从向量表基地址读取4字节的值，系统初始MSP的栈顶地址 */
		"   msr msp, r0                     \n" /* 将初始栈顶地址写入MSP寄存器，初始化主栈指针 */
		"   mov r0, #0                      \n"
		"   msr control, r0                 \n" /* 特权模式清除CONTROL寄存器，使用MSP */
		"   isb                             \n" /* 指令同步屏障，确保前面的MSR/CONTROL配置指令执行完成，无流水线乱序 */
		"   cpsie i                         \n" /* 全局使能IRQ普通中断 (Clear PRIMASK, Enable Interrupts) */
		"   cpsie f                         \n" /* 全局使能FIQ快速中断/故障异常 (Clear FAULTMASK, Enable Faults) */
		"   dsb                             \n" /* 数据同步屏障，确保所有数据访问操作完成，内存状态同步 */
		"   isb                             \n" /* 指令同步屏障，确保所有配置生效，刷新流水线，取指重新同步 */
		"   svc %0                          \n" /* 触发SVC软中断，传递SVC功能号 */
		"   nop                             \n" /* 占位 */
		:: "i" (FLYOS_SVC_START_SCHEDULER)  	/* %0 替换为宏定义的SVC功能号，立即数传参 */
	);

	/* 不会执行到当前位置 */
	while (1);
}
