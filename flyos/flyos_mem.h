/******************************************************************************
 * @file    flyos_mem.h
 * @brief   FlyOS 动态内存管理子系统
 * @version 1.0.0
 * @date    2024
 *
 * @author  yiyi
 *
 *****************************************************************************
 * @details 概述
 *
 * 本文件实现 FlyOS 的动态内存管理功能，提供堆内存的分配和释放接口。
 * 采用 First-Fit 分配算法，支持内存块自动合并和碎片管理。
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                    FlyOS 内存管理特性                                    │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ 动态分配          │ malloc/free 标准接口                                 │
 * │ First-Fit 算法    │ 快速查找首个满足条件的内存块                         │
 * │ 自动合并          │ 释放时自动合并相邻空闲块                             │
 * │ 内存统计          │ 跟踪使用量、峰值、碎片数量                           │
 * │ 损坏检测          │ 魔术数字验证，检测内存越界                           │
 * │ 线程安全          │ 临界区保护，支持多任务并发访问                       │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * @section mem_features 核心特性
 *
 * - First-Fit 分配策略，平衡速度与碎片
 * - 释放时自动合并相邻空闲块，减少碎片
 * - 可选的内存统计功能（FLYOS_USE_MEM_STATS）
 * - 魔术数字边界检查，检测内存损坏
 * - 临界区保护，确保多任务环境下的安全性
 * - 轻量级实现，适合嵌入式系统
 *
 * @section mem_usage 使用说明
 *
 * 1. 在 flyos_config.h 中设置 FLYOS_USE_HEAP=1
 * 2. 配置 FLYOS_HEAP_SIZE 定义堆大小
 * 3. 使用 flyos_malloc() 分配内存
 * 4. 使用 flyos_free() 释放内存
 * 5. 可选启用 FLYOS_USE_MEM_STATS 查看统计信息
 *
 * @note   内存分配失败时返回 NULL
 * @note   释放 NULL 指针是安全的（不执行任何操作）
 *
 * @warning 不要释放未通过 flyos_malloc() 分配的内存
 * @warning 不要重复释放同一块内存
 *
 *****************************************************************************/

#ifndef FLYOS_MEM_H
#define FLYOS_MEM_H

#include "flyos_config.h"
#include "flyos_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#if FLYOS_USE_HEAP

/* 内存统计结构 */

#if FLYOS_USE_MEM_STATS
/**
 * struct flyos_mem_stats_t - 内存统计结构
 * @total_size: 堆总大小
 * @used_size: 当前已用字节数
 * @free_size: 当前空闲字节数
 * @peak_usage: 峰值内存使用量
 * @alloc_count: 总分配次数
 * @free_count: 总释放次数
 * @fragment_count: 空闲块数量
 */
typedef struct {
	flyos_u32_t total_size;
	flyos_u32_t used_size;
	flyos_u32_t free_size;
	flyos_u32_t peak_usage;
	flyos_u32_t alloc_count;
	flyos_u32_t free_count;
	flyos_u32_t fragment_count;
} flyos_mem_stats_t;
#endif

/* 内存管理API */

/**
 * @brief 初始化内存堆
 * @details 初始化动态内存管理器，设置堆内存池。
 *          必须在使用任何内存分配函数之前调用。
 *
 *          初始化步骤：
 *          1. 配置堆起始地址和大小
 *          2. 初始化空闲块链表
 *          3. 设置内存统计信息(如果启用)
 *
 * @note Context: 系统初始化阶段
 * @note 此函数应在flyos_kernel_init()内部被调用
 */
void flyos_mem_init(void);

/**
 * @brief 分配内存
 * @details 从堆中分配指定大小的内存块。
 *          使用First-fit分配算法查找第一个足够大的空闲块。
 *
 * @param[in] size  要分配的字节数
 *
 * @return 指向分配内存的指针，内存不足时返回NULL
 *
 * @note Context: 任务上下文或初始化阶段
 * @note 分配的内存不会被初始化，内容不确定
 * @note 需要手动调用flyos_mem_free()释放内存
 *
 * @warning 不要在中断上下文中调用
 */
void* flyos_mem_alloc(flyos_u32_t size);

/**
 * @brief 释放内存
 * @details 释放之前分配的内存块，并自动合并相邻的空闲块以减少碎片。
 *
 *          合并策略：
 *          1. 检查并合并前一个相邻空闲块
 *          2. 检查并合并后一个相邻空闲块
 *          3. 更新空闲块链表
 *
 * @param[in] ptr  要释放的内存块指针
 *
 * @note Context: 任务上下文
 * @note 传入NULL指针时安全返回，不执行任何操作
 * @note 不要重复释放同一块内存(double-free)
 *
 * @warning 释放后不要再访问该内存块
 * @warning 不要在中断上下文中调用
 */
void flyos_mem_free(void *ptr);

/**
 * @brief 重新分配内存
 * @details 改变已分配内存块的大小。
 *          如果新大小更大，会尝试扩展当前块或分配新块并复制数据。
 *          如果新大小更小，会缩小当前块。
 *
 *          行为特性：
 *          - ptr为NULL时，等同于flyos_mem_alloc(new_size)
 *          - new_size为0时，等同于flyos_mem_free(ptr)，返回NULL
 *          - 重新分配成功后，原指针可能失效
 *          - 数据会被保留(复制到新位置)，保留大小为min(old_size, new_size)
 *
 * @param[in] ptr       原内存块指针
 * @param[in] new_size  新的大小(字节)
 *
 * @return 指向重新分配内存的指针，失败时返回NULL且原内存块保持不变
 *
 * @note Context: 任务上下文
 * @note 如果返回值不为NULL且与ptr不同，需要更新指针
 *
 * @warning 不要在中断上下文中调用
 */
void* flyos_mem_realloc(void *ptr, flyos_u32_t new_size);

/**
 * @brief 分配并清零内存
 * @details 分配count * size字节的内存，并将所有字节初始化为0。
 *          等同于flyos_mem_alloc()后紧接memset(..., 0, ...)。
 *
 * @param[in] count  元素数量
 * @param[in] size   每个元素的大小(字节)
 *
 * @return 指向清零内存的指针，失败返回NULL
 *
 * @note Context: 任务上下文
 * @note 适合分配数组或结构体，确保初始值为0
 * @note 如果count * size溢出，返回NULL
 *
 * @warning 不要在中断上下文中调用
 */
void* flyos_mem_calloc(flyos_u32_t count, flyos_u32_t size);

#if FLYOS_USE_MEM_STATS
/**
 * @brief 获取内存统计信息
 * @details 获取当前堆内存的统计数据，包括总大小、使用量、峰值等。
 *          用于内存监控和调试。
 *
 *          统计信息包括：
 *          - total_size: 堆总大小
 *          - used_size: 当前已用字节数
 *          - free_size: 当前空闲字节数
 *          - peak_usage: 峰值内存使用量
 *          - alloc_count: 总分配次数
 *          - free_count: 总释放次数
 *          - fragment_count: 空闲块数量(碎片指标)
 *
 * @param[out] stats  统计结构指针，用于存储统计信息
 *
 * @note Context: 任意上下文
 * @note 此函数仅在FLYOS_USE_MEM_STATS=1时可用
 */
void flyos_mem_get_stats(flyos_mem_stats_t *stats);
#endif

#endif /* FLYOS_USE_HEAP */

#ifdef __cplusplus
}
#endif

#endif /* FLYOS_MEM_H */
