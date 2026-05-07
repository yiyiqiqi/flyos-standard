/**
 * @file flyos_mem.c
 * @brief FLYOS内存管理实现
 * @version 1.0.0
 *
 * 实现使用First-fit算法和立即合并策略
 * - First-fit: O(n)平均情况，查找第一个足够大的空闲块
 * - 立即合并: 在free()时合并相邻的空闲块
 */

#include "flyos_mem.h"
#include "flyos_kernel.h"
#include <string.h>

#if FLYOS_USE_HEAP

/*===========================================================================*/
/* 内存块结构                                                                 */
/*===========================================================================*/

/**
 * @brief 内存块头部
 */
typedef struct mem_block {
	struct mem_block     *next;       /**< 链表中的下一个块 */
	struct mem_block     *prev;       /**< 链表中的上一个块 */
	flyos_u32_t           size;       /**< 块大小（包含头部） */
	flyos_u32_t           used;       /**< 1表示已使用，0表示空闲 */

#if FLYOS_USE_MEM_CHECK
	flyos_u32_t           magic;      /**< 用于损坏检查的魔术数字 */
#endif

} mem_block_t;

#define MEM_BLOCK_HEADER_SIZE   sizeof (mem_block_t)
#define MEM_BLOCK_MIN_SIZE      32
#define MEM_BLOCK_MAGIC         0xABCDEF00

/*===========================================================================*/
/* 私有变量                                                                   */
/*===========================================================================*/

/**
 * @brief 堆内存数组
 */
static flyos_u8_t heap_memory[FLYOS_HEAP_SIZE] FLYOS_ALIGNED(FLYOS_MEM_ALIGNMENT);

/**
 * @brief 堆起始指针
 */
static mem_block_t *heap_start = NULL;

/**
 * @brief 空闲链表头
 */
static mem_block_t *free_list = NULL;

#if FLYOS_USE_MEM_STATS

/**
 * @brief 内存统计信息
 */
static flyos_mem_stats_t mem_stats = {0};

#endif

/*===========================================================================*/
/* 私有函数                                                                   */
/*===========================================================================*/

/**
 * @brief   将大小对齐到内存对齐要求
 * @details 使用位运算将给定大小向上对齐到FLYOS_MEM_ALIGNMENT边界。
 *          对齐后的大小保证是FLYOS_MEM_ALIGNMENT的整数倍。
 *
 * @param[in] size  需要对齐的大小（字节）
 *
 * @return 对齐后的大小
 *
 * @note Context: 可在任何上下文中调用
 * @note Time complexity: O(1)
 */
FLYOS_INLINE flyos_u32_t align_size(flyos_u32_t size)
{
	return (size + FLYOS_MEM_ALIGNMENT - 1) & ~(FLYOS_MEM_ALIGNMENT - 1);
}

/**
 * @brief   合并相邻的空闲块
 * @details 检查指定块的前后相邻块，如果相邻块为空闲状态，则将它们合并为一个
 *          更大的空闲块。这是立即合并策略的实现，用于减少内存碎片。
 *          合并操作会更新双向链表结构和块大小信息。
 *
 * @param[in,out] block  需要合并的内存块指针
 *
 * @note Context: 必须在临界区内调用
 * @note Time complexity: O(1)
 */
static void merge_free_blocks(mem_block_t *block)
{
	/** 与下一个块合并 */
	if (block->next != NULL && !block->next->used) {
		block->size += block->next->size;
		block->next = block->next->next;
		if (block->next != NULL) {
			block->next->prev = block;
		}

#if FLYOS_USE_MEM_STATS
		mem_stats.fragment_count --;
#endif
	}

	/** 与上一个块合并 */
	if (block->prev != NULL && !block->prev->used) {
		block->prev->size += block->size;
		block->prev->next = block->next;
		if (block->next != NULL) {
			block->next->prev = block->prev;
		}

#if FLYOS_USE_MEM_STATS
		mem_stats.fragment_count--;
#endif
	}
}

/**
 * @brief   分割内存块
 * @details 如果内存块足够大，将其分割为两个块：第一个块大小为size，
 *          剩余部分成为新的空闲块。只有当剩余空间大于等于
 *          (MEM_BLOCK_HEADER_SIZE + MEM_BLOCK_MIN_SIZE)时才会执行分割。
 *          分割后会更新双向链表结构。
 *
 * @param[in,out] block  需要分割的内存块指针
 * @param[in] size       分割后第一个块的大小（字节）
 *
 * @return 返回第一个块的指针
 *
 * @note Context: 必须在临界区内调用
 * @note Time complexity: O(1)
 */
static mem_block_t* split_block(mem_block_t *block, flyos_u32_t size)
{
	flyos_u32_t remaining_size = block->size - size;

	if (remaining_size >= MEM_BLOCK_HEADER_SIZE + MEM_BLOCK_MIN_SIZE) {
		/** 创建新块 */
		mem_block_t *new_block = (mem_block_t *)((flyos_u8_t *)block + size);

		new_block->size = remaining_size;
		new_block->used = 0;
		new_block->next = block->next;
		new_block->prev = block;

#if FLYOS_USE_MEM_CHECK
		new_block->magic = MEM_BLOCK_MAGIC;
#endif

		if (block->next != NULL) {
			block->next->prev = new_block;
		}

		block->next = new_block;
		block->size = size;

#if FLYOS_USE_MEM_STATS
		mem_stats.fragment_count++;
#endif
	}

	return block;
}

/**
 * @brief   查找首次适配块（First-fit算法，静态函数）
 * @details 从堆起始位置开始遍历内存块链表，返回第一个满足大小要求的空闲块。
 *          这是First-fit分配策略的实现，平均时间复杂度为O(n)。
 *
 * @param[in] size  需要的内存块大小（字节）
 *
 * @return 成功返回找到的空闲块指针，失败返回NULL
 *
 * @note Context: 必须在临界区内调用
 * @note Time complexity: O(n)，n为内存块数量
 */
static mem_block_t* find_first_fit(flyos_u32_t size)
{
	mem_block_t *current = heap_start;

	while (current != NULL) {
		if (!current->used && current->size >= size) {
			return current;  /** 返回第一个足够大的块 */
		}
		current = current->next;
	}

	return NULL;
}

/*===========================================================================*/
/* 公共函数                                                                   */
/*===========================================================================*/

/**
 * @brief 初始化内存堆
 */
void flyos_mem_init(void)
{
	/** 初始化第一个块 */
	heap_start = (mem_block_t *)heap_memory;
	heap_start->size = FLYOS_HEAP_SIZE;
	heap_start->used = 0;
	heap_start->next = NULL;
	heap_start->prev = NULL;

#if FLYOS_USE_MEM_CHECK
	heap_start->magic = MEM_BLOCK_MAGIC;
#endif

	free_list = heap_start;

#if FLYOS_USE_MEM_STATS
	mem_stats.total_size = FLYOS_HEAP_SIZE;
	mem_stats.used_size = MEM_BLOCK_HEADER_SIZE;
	mem_stats.free_size = FLYOS_HEAP_SIZE - MEM_BLOCK_HEADER_SIZE;
	mem_stats.peak_usage = MEM_BLOCK_HEADER_SIZE;
	mem_stats.alloc_count = 0;
	mem_stats.free_count = 0;
	mem_stats.fragment_count = 1;
#endif
}

/**
 * @brief 分配内存
 */
void* flyos_mem_alloc(flyos_u32_t size)
{
	if (size == 0) {
		return NULL;
	}

	/** 对齐大小并添加头部 */
	size = align_size(size) + MEM_BLOCK_HEADER_SIZE;

	flyos_base_t level = flyos_enter_critical();

	/** 查找首次适配 */
	mem_block_t *block = find_first_fit(size);

	if (block == NULL) {
		flyos_exit_critical(level);
		return NULL;
	}

	/** 必要时分割 */
	block = split_block(block, size);

	/** 标记为已使用 */
	block->used = 1;

#if FLYOS_USE_MEM_STATS
	mem_stats.used_size += block->size;
	mem_stats.free_size -= block->size;
	mem_stats.alloc_count++;

	if (mem_stats.used_size > mem_stats.peak_usage) {
		mem_stats.peak_usage = mem_stats.used_size;
	}
#endif

	flyos_exit_critical(level);

	/** 返回提供给用户的内存 */
	return (void *)((flyos_u8_t *)block + MEM_BLOCK_HEADER_SIZE);
}

/**
 * @brief 释放内存
 */
void flyos_mem_free(void *ptr)
{
	if (ptr == NULL) {
		return;
	}

	flyos_base_t level = flyos_enter_critical();

	/** 获取块头部 */
	mem_block_t *block = (mem_block_t *)((flyos_u8_t *)ptr - MEM_BLOCK_HEADER_SIZE);

#if FLYOS_USE_MEM_CHECK
	/** 检查魔术数字 */
	if (block->magic != MEM_BLOCK_MAGIC) {
		flyos_exit_critical(level);
		return;
	}
#endif

	/** 检查是否已释放 */
	if (!block->used) {
		flyos_exit_critical(level);
		return;
	}

	/** 标记为空闲 */
	block->used = 0;

#if FLYOS_USE_MEM_STATS
	mem_stats.used_size -= block->size;
	mem_stats.free_size += block->size;
	mem_stats.free_count++;
#endif

	/** 合并相邻块 */
	merge_free_blocks(block);

	flyos_exit_critical(level);
}

/**
 * @brief 重新分配内存
 */
void* flyos_mem_realloc(void *ptr, flyos_u32_t new_size)
{
	if (ptr == NULL) {
		return flyos_mem_alloc(new_size);
	}

	if (new_size == 0) {
		flyos_mem_free(ptr);
		return NULL;
	}

	flyos_base_t level = flyos_enter_critical();

	mem_block_t *block = (mem_block_t *)((flyos_u8_t *)ptr - MEM_BLOCK_HEADER_SIZE);

#if FLYOS_USE_MEM_CHECK
	if (block->magic != MEM_BLOCK_MAGIC) {
		flyos_exit_critical(level);
		return NULL;
	}
#endif

	flyos_u32_t old_size = block->size - MEM_BLOCK_HEADER_SIZE;

	flyos_exit_critical(level);

	/** 如果更小，直接返回 */
	if (new_size <= old_size) {
		return ptr;
	}

	/** 分配新块 */
	void *new_ptr = flyos_mem_alloc(new_size);
	if (new_ptr != NULL) {
		memcpy(new_ptr, ptr, old_size);
		flyos_mem_free(ptr);
	}

	return new_ptr;
}

/**
 * @brief 分配并清零内存
 */
void* flyos_mem_calloc(flyos_u32_t count, flyos_u32_t size)
{
	if (count > 0 && size > UINT32_MAX / count) {
		return NULL;
	}

	flyos_u32_t total_size = count * size;
	void *ptr = flyos_mem_alloc(total_size);

	if (ptr != NULL) {
		memset(ptr, 0, total_size);
	}

	return ptr;
}

#if FLYOS_USE_MEM_STATS
/**
 * @brief 获取内存统计信息
 */
void flyos_mem_get_stats(flyos_mem_stats_t *stats)
{
	if (stats != NULL) {
		flyos_base_t level = flyos_enter_critical();
		*stats = mem_stats;
		flyos_exit_critical(level);
	}
}
#endif

#endif /* FLYOS_USE_HEAP */
