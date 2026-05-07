/**
 * @file flyos_port_asm.s
 * @brief FlyOS ARM Cortex-M4 汇编移植层（STM32F4）
 * @version 1.0.0
 * @platform STM32F429 (ARM Cortex-M4)
 * @author  yiyi
 * @date    2024-6-22
 * @details 概述
 *    主要实现移植层的汇编代码：
 *          - SVC处理器用于启动调度器
 *          - PendSV处理器用于上下文切换
 */

/*---------------------------------------------------------------------------*/
/* 汇编器指令：指定汇编语法，目标处理器架构及使用Thumb指令集                          */
/*---------------------------------------------------------------------------*/
    .syntax unified
    .arch armv7e-m
    .thumb

/*---------------------------------------------------------------------------*/
/* 外部符号声明：声明汇编外部使用的函数/变量                                        */
/*---------------------------------------------------------------------------*/
    .extern flyos_context_switch
    .extern px_current_tcb
    .extern flyos_systick_handler

/*---------------------------------------------------------------------------*/
/* 全局符号声明：将当前文件中定义的函数/变量进行全局声明                              */
/*---------------------------------------------------------------------------*/
    .global SVC_Handler
    .global PendSV_Handler
    .global flyos_svc_handler
    .global flyos_pendsv_handler

/*---------------------------------------------------------------------------*/
/* 常量定义                                                                   */
/*---------------------------------------------------------------------------*/
    .equ configMAX_SYSCALL_INTERRUPT_PRIORITY, 0xB0
    .equ FLYOS_SVC_START_SCHEDULER, 0xFF

/*---------------------------------------------------------------------------*/
/* SVC中断 - 启动第一个任务                                                     */
/*---------------------------------------------------------------------------*/

    .type SVC_Handler, %function
SVC_Handler:
flyos_svc_handler:
    /* 确定使用的栈（MSP或PSP）类型 */
    tst lr, #4
    ite eq
    mrseq r0, MSP                    /* 如果LR的第2位为0，使用MSP */
    mrsne r0, PSP                    /* 如果LR的第2位为1，使用PSP */

    /* 从指令中获取SVC号 */
    ldr r1, [r0, #24]                /* 加载异常时压栈的PC值 */
    ldrb r1, [r1, #-2]               /* 获取SVC号（PC前的字节） */

    /* 检查SVC功能号，是否为flyos启动码，不相等跳转到SVC_NotFYOS */
    cmp r1, #FLYOS_SVC_START_SCHEDULER
    bne SVC_NotFYOS

SVC_StartScheduler:
    /* 获取当前TCB指针 */
    ldr r3, =px_current_tcb          /* 第一个任务的TCB指针 */

    ldr r1, [r3]                     /* 加载TCB指针 */
    ldr r0, [r1]                     /* 从TCB加载栈指针 */

    /* 恢复软件帧（R4-R11, EXC_RETURN） */
    ldmia r0!, {r4-r11, r14}         /* 弹出R4-R11及EXC_RETURN到LR */

    /* 设置PSP为硬件帧顶部 */
    msr psp, r0
    isb

    /* 使能中断 */
    mov r0, #0
    msr basepri, r0

    /* 使用LR中的EXC_RETURN值返回，恢复硬件帧（R0-R3, R12, LR, PC, xPSR） */
    bx r14

SVC_NotFYOS:
    /* 非flyos启动SVC，返回允许非RTOS操作 */
    bx lr

/*---------------------------------------------------------------------------*/
/* PendSV处理器 - 上下文切换                                                  */
/*---------------------------------------------------------------------------*/

    .type PendSV_Handler, %function
PendSV_Handler:
flyos_pendsv_handler:
    /* 使用BASEPRI禁用中断 */
    mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY
    cpsid i                          /* 全局禁用中断 */
    msr basepri, r0                  /* 设置BASEPRI阈值 */
    dsb                              /* 数据同步屏障 */
    isb                              /* 指令同步屏障 */
    cpsie i                          /* 使能中断 */

    /* 保存当前任务上下文 */
    mrs r0, psp						 /* 获取当前任务的栈指针PSP */
    isb

    /* STM32F4 FPU是单精度，惰性保存会自动处理S0-S15 */
    /* Cortex-M4没有S16-S31寄存器，此为M7专属 */

    stmdb r0!, {r4-r11, r14}         /* 保存软件帧，压入R4-R11和LR（EXC_RETURN） */

    ldr r3, =px_current_tcb			 /* 获取当前TCB指针，r3指向px_current_tcb */

    ldr r2, [r3]                     /* 加载当前TCB指针 */
    str r0, [r2]                     /* 保存SP到TCB->stackPointer */

    /* 选择下一个任务，恢复下一个任务的上下文 */
    stmdb sp!, {r3, r14}			 /* 函数调用前保存需要使用的寄存器 */
    bl flyos_context_switch			 /* 调用C函数选择下一个任务 */
    ldmia sp!, {r3, r14}			 /* 恢复寄存器 */

    ldr r1, [r3]                     /* 从r3加载新的TCB指针到r1 */
    ldr r0, [r1]                     /* 从新TCB加载SP */

    ldmia r0!, {r4-r11, r14}         /* 恢复软件帧，弹出R4-R11和EXC_RETURN */

    /* FPU上下文会被硬件惰性恢复 */

    msr psp, r0                      /* 设置PSP为新任务的栈指针 */
    isb

    mov r0, #0                       /* 使能中断 */
    msr basepri, r0

    /* 返回到新任务，硬件将恢复R0-R3, R12, LR, PC, xPSR */
    bx r14

    .end
