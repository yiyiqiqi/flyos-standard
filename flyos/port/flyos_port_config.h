/******************************************************************************
 * @file    flyos_port_config.h
 * @brief   FlyOS 平台特定配置
 * @version 1.1.5
 * @date    2025-12-25
 * @author  yiyi
 *
 *****************************************************************************
 * @details 概述
 *
 * 本文件包含 FlyOS 实时操作系统的平台特定硬件配置，与核心配置分离以
 * 保持系统的可移植性。支持多平台编译和快速平台切换。
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                  FlyOS 平台配置架构                                      │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ 平台选择          │ STM32F4, S32K344 等平台标识                          │
 * │ CPU 架构          │ Cortex-M4, Cortex-M7 架构定义                        │
 * │ 时钟配置          │ 系统时钟频率、Tick 定时器配置                        │
 * │ 中断配置          │ 中断优先级分组、屏蔽级别                             │
 * │ 硬件特性          │ FPU、MPU、缓存等硬件特性开关                         │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * @section platform_selection 平台选择
 *
 * 通过定义 FLYOS_PLATFORM 宏选择目标平台：
 *
 * **方法 1：在构建系统中定义（推荐）**
 * ```
 * -DFLYOS_PLATFORM=1  # STM32F4
 * -DFLYOS_PLATFORM=2  # S32K344
 * ```
 *
 * **方法 2：修改本文件**
 * ```c
 * #define FLYOS_PLATFORM    FLYOS_PLATFORM_STM32F4
 * // 或
 * #define FLYOS_PLATFORM    FLYOS_PLATFORM_S32K34X
 * ```
 *
 * @section platform_structure 目录结构
 *
 * ```
 * flyos/port/
 * ├── flyos_port_config.h      (本文件 - 平台选择)
 * ├── flyos_port.h              (通用 HAL 接口)
 * ├── s32k344/                  (S32K344 实现)
 * │   ├── flyos_port.c
 * │   ├── flyos_port_asm.s
 * │   └── README.md
 * └── stm32f4/                  (STM32F4 实现)
 *     ├── flyos_port.c
 *     ├── flyos_port_asm.s
 *     └── README.md
 * ```
 *
 * @section platform_list 支持的平台
 *
 * - **FLYOS_PLATFORM_STM32F4**: STM32F4xx 系列（Cortex-M4）
 * - **FLYOS_PLATFORM_S32K34X**: NXP S32K344（Cortex-M7）
 *
 * @note   平台配置应在项目开始时确定，避免频繁切换
 * @note   不同平台的时钟频率、中断配置可能不同
 *
 * @warning 切换平台后必须清理构建并重新编译
 * @warning 确保链接正确的平台实现文件（.c 和 .s）
 *
 *****************************************************************************/

#ifndef FLYOS_PORT_CONFIG_H
#define FLYOS_PORT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* 平台选择                                                                  */
/*===========================================================================*/

/**
 * @brief 支持的平台定义
 */
#define FLYOS_PLATFORM_STM32F4     1    /**< STM32F4 (ARM Cortex-M4) */
#define FLYOS_PLATFORM_S32K34X     2    /**< S32K344 (ARM Cortex-M7) */

/**
 * @brief 当前目标平台
 *
 * @note 平台由 CMake 构建系统自动定义，无需手动修改此文件
 *
 * @example CMake 会自动定义：
 *   stm32f4 → -DFLYOS_PLATFORM=1
 *   s32k344 → -DFLYOS_PLATFORM=2
 */
#ifndef FLYOS_PLATFORM
	/* 如果未通过 CMake 定义，使用默认平台 */
	#warning "FLYOS_PLATFORM not defined by build system, using default s32k344"
	#define FLYOS_PLATFORM          FLYOS_PLATFORM_S32K34X
#endif

/*===========================================================================*/
/* 平台特定配置                                                              */
/*===========================================================================*/

#if (FLYOS_PLATFORM == FLYOS_PLATFORM_STM32F4)

	/*-----------------------------------------------------------------------*/
	/* STM32F4平台配置                                                       */
	/*-----------------------------------------------------------------------*/

	/* CPU配置 */
	#define FLYOS_CPU_CLOCK_HZ      168000000UL   /**< CPU时钟：168 MHz */
    #define FLYOS_FPU_ENABLE        1             /**< 启用FPU支持 */

#elif (FLYOS_PLATFORM == FLYOS_PLATFORM_S32K34X)

	/*-----------------------------------------------------------------------*/
	/* S32K34X平台配置                                                       */
	/*-----------------------------------------------------------------------*/

	/* CPU配置 */
	#define FLYOS_CPU_CLOCK_HZ      160000000UL   /**< CPU时钟：160 MHz */
    #define FLYOS_FPU_ENABLE        1             /**< 启用FPU支持 */
#else
	#error "不支持的平台！请定义FLYOS_PLATFORM"
#endif

/*===========================================================================*/
/* 派生配置                                                                  */
/*===========================================================================*/

/**
 * @brief 系统默认心跳频率
 * @note 默认：1000 Hz
 */
#ifndef FLYOS_TICK_RATE_HZ
	#define FLYOS_TICK_RATE_HZ      1000
#endif

/**
 * @brief SysTick重载值
 */
#define FLYOS_SYSTICK_RELOAD_VALUE  ((FLYOS_CPU_CLOCK_HZ / FLYOS_TICK_RATE_HZ) - 1)

/*===========================================================================*/
/* 平台验证                                                                  */
/*===========================================================================*/

/* 编译时检查 */
#if (FLYOS_CPU_CLOCK_HZ < 1000000)
	#error "CPU时钟频率太低！"
#endif

#if (FLYOS_TICK_RATE_HZ < 10) || (FLYOS_TICK_RATE_HZ > 10000)
	#error "滴答频率超出范围（10-10000 Hz）"
#endif

#ifdef __cplusplus
}
#endif

#endif /* FLYOS_PORT_CONFIG_H */
