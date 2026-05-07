/******************************************************************************
 * @file    flyos_ipc.h
 * @brief   FlyOS 进程间通信（IPC）子系统
 * @version 1.0.0
 * @date    2024
 *
 * @author  yiyi
 *
 *****************************************************************************
 * @details 概述
 *
 * 本文件实现 FlyOS 的进程间通信机制，基于统一等待队列实现 O(1) 时间复杂度
 * 的高性能同步与通信。系统提供五种核心 IPC 原语：
 *
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │                        FlyOS IPC 架构总览                                │
 * ├─────────────────────────────────────────────────────────────────────────┤
 * │ 信号量（Semaphore）    │ 计数型/二值型同步原语                             │
 * │ 互斥锁（Mutex）        │ 支持优先级继承的互斥访问机制                       │
 * │ 消息队列（Queue）      │ FIFO 先进先出消息传递                             │
 * │ 事件标志（Event）      │ 基于位操作的多事件同步                            │
 * │ 邮箱（Mailbox）        │ 轻量级零拷贝单消息传递                            │
 * └─────────────────────────────────────────────────────────────────────────┘
 *
 * @section ipc_scenarios 典型使用场景
 *
 * ■ 信号量（Semaphore）
 *   【资源计数】有限资源池管理（如 5 个串口缓冲区、10 个网络连接）
 *   【事件通知】中断触发任务处理（如 ADC 采样完成通知、DMA 传输完成通知）
 *   【生产消费】生产者-消费者模式的计数控制（如音频缓冲区填充/播放同步）
 *   【任务同步】多任务启动栅栏（binary semaphore 实现多任务同时启动）
 *   【限流控制】限制并发访问数量（如最多 3 个任务同时访问 SPI Flash）
 *
 * ■ 互斥锁（Mutex）
 *   【临界区保护】保护全局变量、状态机访问（如 LCD 显示缓冲区、系统配置参数）
 *   【硬件互斥】单例硬件资源访问（如 I2C/SPI 总线、UART、Flash 芯片）
 *   【数据结构】保护链表、队列等复杂数据结构的多步操作
 *   【文件系统】文件系统挂载点保护（防止并发读写冲突）
 *   【优先级反转】需要优先级继承的场景（高优先级任务等低优先级任务释放资源）
 *
 * ■ 消息队列（Queue）
 *   【任务通信】任务间传递结构化数据（如传感器数据、控制命令、日志条目）
 *   【异步处理】中断到任务的数据传递（如串口接收帧、按键事件、网络数据包）
 *   【命令分发】命令解析与执行分离（如 AT 命令队列、Shell 命令队列）
 *   【日志系统】多任务日志缓冲与批量处理（如实时日志收集后统一写 Flash）
 *   【状态机】事件驱动状态机（如协议栈事件队列、GUI 事件队列）
 *
 * ■ 事件标志（Event）
 *   【多条件等待】等待多个事件满足（如网络连接成功 AND 数据准备就绪）
 *   【任务同步】多任务在同步点汇合（如 3 个初始化任务完成后启动主任务）
 *   【状态通知】系统状态变化广播（如 WiFi 连接/断开、电量告警、按键状态）
 *   【中断聚合】多个中断事件聚合处理（如 UART+SPI+GPIO 中断统一通知）
 *   【复杂条件】灵活的位操作组合（如 bit0:准备就绪 | bit1:允许发送 | bit2:缓冲区空闲）
 *
 * ■ 邮箱（Mailbox）
 *   【大数据传递】传递大块数据指针（如图像帧、音频 buffer、文件数据）
 *   【内存池】传递内存池对象（如从内存池申请 buffer 后传递所有权）
 *   【零拷贝】高频单对单通信（如 DMA 接收 buffer 指针传递给解析任务）
 *   【命令响应】单次命令-响应模式（如发送命令指针，等待响应指针）
 *   【对象传递】传递复杂对象指针（如设备句柄、协议栈上下文）
 *
 * @section ipc_selection 选择指南
 *
 * ┌──────────────────┬──────────────────────────────────────────────────┐
 * │ 需求             │ 推荐方案                                           │
 * ├──────────────────┼──────────────────────────────────────────────────┤
 * │ 保护共享资源     │ Mutex（支持优先级继承，防止优先级反转）             │
 * │ 资源计数/事件计数│ Semaphore（计数型信号量）                          │
 * │ 传递小数据(<64B) │ Queue（按值拷贝，支持多消息缓冲）                   │
 * │ 传递大数据(>64B) │ Mailbox（零拷贝，只传指针）                        │
 * │ 等待多个条件     │ Event（位操作，支持 AND/OR 逻辑）                   │
 * │ 中断通知任务     │ Semaphore / Queue / Mailbox（均支持 ISR 接口）     │
 * │ 任务同步         │ Semaphore（二值）/ Event（多条件）                 │
 * │ 广播通知         │ Event（多任务可同时等待同一事件）                   │
 * └──────────────────┴──────────────────────────────────────────────────┘
 *
 * @section ipc_features 核心特性
 *
 * - 统一等待队列基础设施，插入/删除操作均为 O(1) 时间复杂度
 * - 基于位图加速的优先级调度机制
 * - 互斥锁支持链式优先级继承（最大深度 MAX_INHERIT_DEPTH）
 * - 支持静态和动态内存分配两种创建方式
 * - 提供 ISR 安全的中断上下文操作接口
 * - 可选的对象命名和调试跟踪功能
 *
 * @note   所有 IPC 对象均继承自 flyos_ipc_base_t 实现统一管理
 *
 * @warning 信号量不支持优先级继承！若需互斥保护以避免优先级反转问题，
 *          请使用互斥锁（Mutex）而非信号量。
 *
 *****************************************************************************/

#ifndef FLYOS_IPC_H
#define FLYOS_IPC_H

#include "flyos_kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================*/
/* 类型定义和数据结构                                                         */
/*===========================================================================*/

/**
 * struct flyos_wait_queue_t - 统一等待队列结构
 * @ready_bitmap: 优先级位图(32位，每位代表一个优先级)
 * @prio_list: 每个优先级的独立链表(32个优先级)
 * @note：优先级位图 + 分层链表，实现O(1)插入和查找。
 */
typedef struct {
	flyos_u32_t ready_bitmap;
	flyos_tcb_t *prio_list[FLYOS_NUM_PRIORITY_LEVELS];
} flyos_wait_queue_t;

/**
 * struct flyos_ipc_base_t - IPC对象基类
 * @magic: IPC魔数，防止野指针破坏
 * @type: 对象类型(SEMAPHORE/MUTEX/QUEUE/EVENT/MAILBOX)
 * @flags: 对象标志位(静态/动态创建标识)
 * @name: IPC名称
 * @file: 创建位置文件名(调试用)
 * @line: 创建位置行号(调试用)
 * @note: 所有IPC对象均继承此基类，实现统一管理
 */
typedef struct {
	flyos_u32_t magic;
	flyos_ipc_type_t type;
	flyos_u8_t flags;
	flyos_u8_t reserved1;
	flyos_u16_t reserved2;

#if FLYOS_USE_IPC_NAME
	const char *name;
#endif

#if FLYOS_USE_ASSERT
	const char *file;
	int line;
#endif
} flyos_ipc_base_t;

/*===========================================================================*/
/* 信号量 - 计数型同步机制                                                    */
/*===========================================================================*/

#if FLYOS_USE_SEMAPHORE

/**
 * struct flyos_semaphore_t - 信号量控制块
 * @base: 继承IPC基类
 * @wait_queue: 等待队列
 * @count: 当前计数值
 * @max_count: 最大计数值
 *
 * 信号量特性：
 * - 信号量不支持优先级继承。
 *      1) 信号量可以被任意任务signal，无明确的"拥有者"；
 *      2) 计数型信号量可能被多个任务持有，无法确定优先级继承目标；
 *      3) 优先级继承仅对互斥锁有意义(单一拥有者+递归锁)
 * - 二值信号量可通过初始化参数实现(init_count=1, max_count=1)
 * - 信号量的wait/signal操作均为原子操作，支持中断上下文调用
 * - 支持中断上下文signal操作，安全唤醒等待任务
 */
typedef struct {
	flyos_ipc_base_t   base;
	flyos_wait_queue_t wait_queue;
	flyos_u32_t        count;
	flyos_u32_t        max_count;
} flyos_semaphore_t;

/**
 * @brief 创建计数信号量（支持静态/动态创建）
 * @details 创建并初始化一个信号量对象，支持两种创建方式：
 *          - 动态创建：sem_buffer=NULL，从堆中分配内存（需要FLYOS_USE_HEAP=1）
 *          - 静态创建：sem_buffer!=NULL，使用用户提供的缓冲区（不依赖堆）
 *
 *          二值信号量：init_count=1, max_count=1
 *
 * @param[in,out] sem_buffer  信号量缓冲区指针
 *                            - NULL: 动态分配内存
 *                            - 非NULL: 使用用户提供的静态缓冲区
 * @param[in] init_count      初始计数值
 * @param[in] max_count       最大计数值
 *
 * @return 信号量句柄，失败返回NULL
 *
 * @note Context: 任务上下文或初始化阶段
 * @note 静态创建时，sem_buffer必须在信号量整个生命周期内有效
 * @note FLYOS_USE_HEAP=0时，只能使用静态创建（sem_buffer必须非NULL）
 *
 * @code
 *      // 动态创建示例（需要FLYOS_USE_HEAP=1）
 *      flyos_semaphore_t *sem = flyos_semaphore_create(NULL, 0, 10);
 *
 *      // 静态创建示例（始终可用）
 *      static flyos_semaphore_t sem_buffer;
 *      flyos_semaphore_t *sem = flyos_semaphore_create(&sem_buffer, 0, 10);
 * @endcode
 */
flyos_semaphore_t* flyos_semaphore_create(
	flyos_semaphore_t *sem_buffer,
	flyos_u32_t init_count,
	flyos_u32_t max_count
);

/**
 * @brief 删除信号量
 * @details 删除信号量并唤醒所有等待任务，返回错误码FLYOS_ERROR。
 *          自动识别静态/动态创建的对象：
 *          - 动态创建的对象：释放内存
 *          - 静态创建的对象：不释放内存（由用户管理）
 *
 * @param[in] sem  信号量句柄
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note 删除后不要再使用该信号量句柄
 * @note 静态创建的对象不会释放内存，用户需自行管理缓冲区生命周期
 */
flyos_err_t flyos_semaphore_delete(flyos_semaphore_t *sem);

/**
 * @brief 等待(递减)信号量
 * @details 尝试获取信号量。如果计数为零则阻塞等待。
 *
 * @param[in] sem      信号量句柄
 * @param[in] timeout  超时时间(tick)，可使用FLYOS_WAIT_FOREVER或FLYOS_NO_WAIT
 *
 * @return 成功返回FLYOS_OK，超时返回FLYOS_TIMEOUT
 *
 * @note Context: 任务上下文
 */
flyos_err_t flyos_semaphore_wait(flyos_semaphore_t *sem, flyos_tick_t timeout);

/**
 * @brief 发送(递增)信号量
 * @details 释放信号量，唤醒最高优先级等待任务。
 *
 * @param[in] sem  信号量句柄
 *
 * @return 成功返回FLYOS_OK，计数已达最大值时返回FLYOS_FULL
 *
 * @note Context: 任务上下文
 */
flyos_err_t flyos_semaphore_signal(flyos_semaphore_t *sem);

/**
 * @brief 从ISR发送信号量
 * @details 在中断中释放信号量，唤醒等待任务。
 *
 * @param[in] sem  信号量句柄
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 中断上下文，可安全调用
 */
flyos_err_t flyos_semaphore_signal_from_isr(flyos_semaphore_t *sem);

/**
 * @brief 获取当前信号量计数
 *
 * @param[in] sem  信号量句柄
 *
 * @return 当前计数值
 *
 * @note Context: 任意上下文
 */
flyos_u32_t flyos_semaphore_get_count(flyos_semaphore_t *sem);

#endif /* FLYOS_USE_SEMAPHORE */

/*===========================================================================*/
/* 互斥锁 - 支持优先级继承的互斥机制                                           */
/*===========================================================================*/

#if FLYOS_USE_MUTEX

/**
 * struct flyos_mutex_t - 互斥锁控制块(支持链式优先级继承)
 * @base: 继承IPC基类
 * @wait_queue: 等待队列
 * @owner: 当前持有任务(NULL=未锁定)
 * @original_priority: owner原始优先级
 * @inherited_priority: 继承的优先级
 * @nest_count: 递归锁计数(FLYOS_USE_RECURSIVE_MUTEX启用时)
 *
 * 互斥锁特性：
 * - 支持优先级继承，防止优先级反转
 * - 支持链式继承(A→B→C，最大深度FLYOS_MAX_INHERIT_DEPTH)
 * - 死锁检测(访问过的任务记录)
 * - 可选递归锁定(同一任务可多次lock)
 *
 * 使用场景：
 * - 保护共享资源(全局变量、硬件外设)
 * - 临界区保护
 * - 避免优先级反转
 */
typedef struct {
	flyos_ipc_base_t   base;
	flyos_wait_queue_t wait_queue;
	flyos_tcb_t       *owner;
	flyos_u8_t         original_priority;
	flyos_u8_t         inherited_priority;
	flyos_u16_t        nest_count;
} flyos_mutex_t;

/**
 * @brief 创建互斥锁（支持静态/动态创建）
 * @details 创建并初始化一个互斥锁对象，支持两种创建方式：
 *          - 动态创建：mutex_buffer=NULL，从堆中分配内存（需要FLYOS_USE_HEAP=1）
 *          - 静态创建：mutex_buffer!=NULL，使用用户提供的缓冲区（不依赖堆）
 *
 * @param[in,out] mutex_buffer  互斥锁缓冲区指针
 *                              - NULL: 动态分配内存
 *                              - 非NULL: 使用用户提供的静态缓冲区
 *
 * @return 互斥锁句柄，失败返回NULL
 *
 * @note Context: 任务上下文或初始化阶段
 * @note 静态创建时，mutex_buffer必须在互斥锁整个生命周期内有效
 * @note FLYOS_USE_HEAP=0时，只能使用静态创建（mutex_buffer必须非NULL）
 *
 * @code
 * // 动态创建示例（需要FLYOS_USE_HEAP=1）
 * flyos_mutex_t *mutex = flyos_mutex_create(NULL);
 *
 * // 静态创建示例（始终可用）
 * static flyos_mutex_t mutex_buffer;
 * flyos_mutex_t *mutex = flyos_mutex_create(&mutex_buffer);
 * @endcode
 */
flyos_mutex_t* flyos_mutex_create(flyos_mutex_t *mutex_buffer);

/**
 * @brief 删除互斥锁
 * @details 删除互斥锁并唤醒所有等待任务，返回错误码FLYOS_ERROR。
 *          自动识别静态/动态创建的对象：
 *          - 动态创建的对象：释放内存
 *          - 静态创建的对象：不释放内存（由用户管理）
 *
 * @param[in] mutex  互斥锁句柄
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note 删除后不要再使用该互斥锁句柄
 * @note 静态创建的对象不会释放内存，用户需自行管理缓冲区生命周期
 */
flyos_err_t flyos_mutex_delete(flyos_mutex_t *mutex);

/**
 * @brief 锁定(获取)互斥锁
 * @details 尝试获取互斥锁。如果已被其他任务持有则阻塞等待。
 *          支持优先级继承以防止优先级反转。
 *          如果启用FLYOS_USE_RECURSIVE_MUTEX，则支持递归锁定。
 *
 * @param[in] mutex    互斥锁句柄
 * @param[in] timeout  超时时间(tick)
 *
 * @return 成功返回FLYOS_OK，超时返回FLYOS_TIMEOUT
 *
 * @note Context: 任务上下文
 */
flyos_err_t flyos_mutex_lock(flyos_mutex_t *mutex, flyos_tick_t timeout);

/**
 * @brief 解锁(释放)互斥锁
 * @details 释放互斥锁。如果优先级被提升，则恢复原始优先级。
 *          唤醒最高优先级等待任务。
 *
 * @param[in] mutex  互斥锁句柄
 *
 * @return 成功返回FLYOS_OK，非持有者返回FLYOS_PERMISSION
 *
 * @note Context: 任务上下文，必须由持有者调用
 */
flyos_err_t flyos_mutex_unlock(flyos_mutex_t *mutex);

/**
 * @brief 获取互斥锁持有者
 *
 * @param[in] mutex  互斥锁句柄
 *
 * @return 持有者任务句柄，未锁定时返回NULL
 *
 * @note Context: 任意上下文
 */
flyos_tcb_t* flyos_mutex_get_owner(flyos_mutex_t *mutex);

#endif /* FLYOS_USE_MUTEX */

/*===========================================================================*/
/* 消息队列 - FIFO消息传递机制                                               */
/*===========================================================================*/

#if FLYOS_USE_QUEUE

/**
 * struct flyos_queue_t - 消息队列控制块
 * @send_queue: 发送等待队列(队列满时)
 * @recv_queue: 接收等待队列(队列空时)
 * @buffer: 队列缓冲区
 * @item_size: 每项大小(字节)
 * @max_items: 最大项数
 * @item_count: 当前项数
 * @read_index: 读索引
 * @write_index: 写索引
 * @flags: 对象标志位(静态/动态创建标识)
 * @name: 名称(FLYOS_USE_IPC_NAME启用时)
 *
 * 消息队列特性：
 * - FIFO顺序
 * - 按值复制(非引用)
 * - 支持阻塞发送/接收
 * - 支持ISR安全的发送接口
 * - 内存占用: 基础~296字节 + buffer_size
 *
 * 使用场景：
 * - 任务间数据传递
 * - 生产者-消费者模式
 * - 中断到任务的数据传递
 * - 命令队列
 * - 事件日志
 */
typedef struct {
	flyos_ipc_base_t base;
	flyos_wait_queue_t send_queue;
	flyos_wait_queue_t recv_queue;
	void *buffer;
	flyos_u32_t item_size;
	flyos_u32_t max_items;
	flyos_u32_t item_count;
	flyos_u32_t read_index;
	flyos_u32_t write_index;
} flyos_queue_t;

/**
 * @brief 创建消息队列（支持静态/动态创建）
 * @details 创建固定大小的消息队列，数据项按值复制。支持两种创建方式：
 *          - 动态创建：queue_buffer=NULL且item_buffer=NULL，从堆中分配内存（需要FLYOS_USE_HEAP=1）
 *          - 静态创建：queue_buffer和item_buffer都非NULL，使用用户提供的缓冲区（不依赖堆）
 *
 * @param[in,out] queue_buffer  队列控制块缓冲区指针
 *                              - NULL: 动态分配内存
 *                              - 非NULL: 使用用户提供的静态缓冲区
 * @param[in,out] item_buffer   队列数据缓冲区指针
 *                              - NULL: 动态分配内存
 *                              - 非NULL: 使用用户提供的静态缓冲区（大小=item_size*max_items）
 * @param[in] item_size         每项大小(字节)
 * @param[in] max_items         最大项数
 *
 * @return 队列句柄，失败返回NULL
 *
 * @note Context: 任务上下文或初始化阶段
 * @note 静态创建时，缓冲区必须在队列整个生命周期内有效
 * @note FLYOS_USE_HEAP=0时，只能使用静态创建（两个buffer都必须非NULL）
 * @note 数据项按值复制，非引用
 *
 * @code
 * // 动态创建示例（需要FLYOS_USE_HEAP=1）
 * flyos_queue_t *queue = flyos_queue_create(NULL, NULL, sizeof(int), 10);
 *
 * // 静态创建示例（始终可用）
 * static flyos_queue_t queue_buffer;
 * static uint8_t item_buffer[sizeof(int) * 10];
 * flyos_queue_t *queue = flyos_queue_create(&queue_buffer, item_buffer, sizeof(int), 10);
 * @endcode
 */
flyos_queue_t* flyos_queue_create(
	flyos_queue_t *queue_buffer,
	void *item_buffer,
	flyos_u32_t item_size,
	flyos_u32_t max_items
);

/**
 * @brief 删除消息队列
 * @details 删除消息队列并唤醒所有等待任务，返回错误码FLYOS_ERROR。
 *          自动识别静态/动态创建的对象：
 *          - 动态创建的对象：释放控制块和数据缓冲区内存
 *          - 静态创建的对象：不释放内存（由用户管理）
 *
 * @param[in] queue  队列句柄
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note 删除后不要再使用该队列句柄
 * @note 静态创建的对象不会释放内存，用户需自行管理缓冲区生命周期
 */
flyos_err_t flyos_queue_delete(flyos_queue_t *queue);

/**
 * @brief 发送数据项到队列
 * @details 将数据项复制到队列中。如果队列满则阻塞等待。
 *
 * @param[in] queue    队列句柄
 * @param[in] item     指向要发送数据项的指针
 * @param[in] timeout  超时时间(tick)
 *
 * @return 成功返回FLYOS_OK，超时返回FLYOS_TIMEOUT
 *
 * @note Context: 任务上下文
 * @note 数据项被复制到队列中
 */
flyos_err_t flyos_queue_send(flyos_queue_t *queue, const void *item, flyos_tick_t timeout);

/**
 * @brief 从ISR发送数据项到队列
 * @details 在中断中发送数据项。非阻塞。
 *
 * @param[in] queue  队列句柄
 * @param[in] item   指向要发送数据项的指针
 *
 * @return 成功返回FLYOS_OK，队列满时返回FLYOS_FULL
 *
 * @note Context: 中断上下文，可安全调用
 */
flyos_err_t flyos_queue_send_from_isr(flyos_queue_t *queue, const void *item);

/**
 * @brief 从队列接收数据项
 * @details 从队列接收数据项。如果队列空则阻塞等待。
 *
 * @param[in] queue    队列句柄
 * @param[out] item    指向接收缓冲区的指针
 * @param[in] timeout  超时时间(tick)
 *
 * @return 成功返回FLYOS_OK，超时返回FLYOS_TIMEOUT
 *
 * @note Context: 任务上下文
 * @note 数据项从队列复制到缓冲区
 */
flyos_err_t flyos_queue_receive(flyos_queue_t *queue, void *item, flyos_tick_t timeout);

/**
 * @brief 获取队列中的数据项数量
 *
 * @param[in] queue  队列句柄
 *
 * @return 数据项数量
 *
 * @note Context: 任意上下文
 */
flyos_u32_t flyos_queue_get_count(flyos_queue_t *queue);

/**
 * @brief 获取队列的空闲空间
 *
 * @param[in] queue  队列句柄
 *
 * @return 空闲槽位数量
 *
 * @note Context: 任意上下文
 */
flyos_u32_t flyos_queue_get_space(flyos_queue_t *queue);

#endif /* FLYOS_USE_QUEUE */

/*===========================================================================*/
/* 事件标志 - 基于位的同步机制                                               */
/*===========================================================================*/

#if FLYOS_USE_EVENT

/**
 * struct flyos_event_t - 事件标志控制块
 * @base: 继承IPC基类
 * @wait_queue: 等待队列
 * @flags: 32位标志位
 * @flag_waiters: 标志位索引位图(32个标志位，每个对应一个32位优先级位图)
 *
 * flag_waiters[i]表示有哪些优先级的任务等待第i个标志位。
 * 占用128字节内存，换取O(k)唤醒性能(k=设置的标志位数)。
 *
 * 事件标志特性：
 * - 支持32个独立事件位
 * - 支持等待任意事件(OR)或所有事件(AND)
 * - 支持自动清除或手动清除模式
 * - O(k)唤醒性能(k=设置的标志位数)
 * - 使用位图索引优化
 *
 * 使用场景：
 * - 多事件等待
 * - 任务同步点
 * - 状态机控制
 * - 中断事件通知
 * - 资源就绪通知
 */
typedef struct {
	flyos_ipc_base_t   base;
	flyos_wait_queue_t wait_queue;
	flyos_u32_t        flags;
	flyos_u32_t        flag_waiters[32];
} flyos_event_t;

/**
 * enum flyos_event_wait_mode_t - 事件等待模式
 * @FLYOS_EVENT_WAIT_ANY: 等待任意标志(OR)
 * @FLYOS_EVENT_WAIT_ALL: 等待所有标志(AND)
 * @FLYOS_EVENT_CLEAR_ON_EXIT: 等待后自动清除标志
 *
 * 可以组合使用：
 * FLYOS_EVENT_WAIT_ALL | FLYOS_EVENT_CLEAR_ON_EXIT
 */
typedef enum {
	FLYOS_EVENT_WAIT_ANY = 0,
	FLYOS_EVENT_WAIT_ALL = 1,
	FLYOS_EVENT_CLEAR_ON_EXIT = 2
} flyos_event_wait_mode_t;

/**
 * @brief 创建事件标志（支持静态/动态创建）
 * @details 创建并初始化一个事件标志对象，支持两种创建方式：
 *          - 动态创建：event_buffer=NULL，从堆中分配内存（需要FLYOS_USE_HEAP=1）
 *          - 静态创建：event_buffer!=NULL，使用用户提供的缓冲区（不依赖堆）
 *
 * @param[in,out] event_buffer  事件标志缓冲区指针
 *                              - NULL: 动态分配内存
 *                              - 非NULL: 使用用户提供的静态缓冲区
 *
 * @return 事件句柄，失败返回NULL
 *
 * @note Context: 任务上下文或初始化阶段
 * @note 静态创建时，event_buffer必须在事件整个生命周期内有效
 * @note FLYOS_USE_HEAP=0时，只能使用静态创建（event_buffer必须非NULL）
 *
 * @code
 * // 动态创建示例（需要FLYOS_USE_HEAP=1）
 * flyos_event_t *event = flyos_event_create(NULL);
 *
 * // 静态创建示例（始终可用）
 * static flyos_event_t event_buffer;
 * flyos_event_t *event = flyos_event_create(&event_buffer);
 * @endcode
 */
flyos_event_t* flyos_event_create(flyos_event_t *event_buffer);

/**
 * @brief 删除事件标志
 * @details 删除事件标志并唤醒所有等待任务，返回错误码FLYOS_ERROR。
 *          自动识别静态/动态创建的对象：
 *          - 动态创建的对象：释放内存
 *          - 静态创建的对象：不释放内存（由用户管理）
 *
 * @param[in] event  事件句柄
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note 删除后不要再使用该事件句柄
 * @note 静态创建的对象不会释放内存，用户需自行管理缓冲区生命周期
 */
flyos_err_t flyos_event_delete(flyos_event_t *event);

/**
 * @brief 设置事件标志
 * @details 设置指定的标志位，唤醒等待这些标志的任务。
 *          使用位图索引优化，只检查等待被设置标志的任务。
 *
 * @param[in] event  事件句柄
 * @param[in] flags  要设置的标志(按位或)
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note Time complexity: O(k)，k=设置的标志位数量
 */
flyos_err_t flyos_event_set(flyos_event_t *event, flyos_u32_t flags);

/**
 * @brief 清除事件标志
 *
 * @param[in] event  事件句柄
 * @param[in] flags  要清除的标志(按位与非)
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任意上下文
 */
flyos_err_t flyos_event_clear(flyos_event_t *event, flyos_u32_t flags);

/**
 * @brief 等待事件标志
 * @details 等待事件标志满足指定条件。
 *
 * @param[in] event         事件句柄
 * @param[in] flags         要等待的标志
 * @param[in] mode          等待模式(任意/全部，退出时清除)
 * @param[in] timeout       超时时间(tick)
 * @param[out] result_flags 指向存储结果标志的指针(可为NULL)
 *
 * @return 成功返回FLYOS_OK，超时返回FLYOS_TIMEOUT
 *
 * @note Context: 任务上下文
 *
 * Example:
 *     // 等待标志0或标志1，唤醒后清除它们
 *     flyos_u32_t result;
 *     flyos_event_wait(event, 0x03, FLYOS_EVENT_WAIT_ANY | FLYOS_EVENT_CLEAR_ON_EXIT,
 *                      1000, &result);
 */
flyos_err_t flyos_event_wait(flyos_event_t *event, flyos_u32_t flags,
			     flyos_event_wait_mode_t mode, flyos_tick_t timeout,
			     flyos_u32_t *result_flags);

/**
 * @brief 获取当前事件标志
 *
 * @param[in] event  事件句柄
 *
 * @return 当前标志值
 *
 * @note Context: 任意上下文
 */
flyos_u32_t flyos_event_get_flags(flyos_event_t *event);

#endif /* FLYOS_USE_EVENT */

/*===========================================================================*/
/* 邮箱 - 轻量级单消息传递                                                    */
/*===========================================================================*/

#if FLYOS_USE_MAILBOX

/**
 * struct flyos_mailbox_t - 邮箱控制块
 * @base: 继承IPC基类
 * @send_queue: 发送等待队列(邮箱满时)
 * @recv_queue: 接收等待队列(邮箱空时)
 * @message: 消息指针
 * @full: 邮箱是否满
 *
 * 邮箱特性：
 * - 零拷贝：只传递指针
 * - 轻量级：内存占用~172字节(vs消息队列~296字节)
 * - 单消息：只能容纳1个消息
 * - O(1)性能：发送/接收都是常数时间
 *
 * 使用场景：
 * - 大数据块传递
 * - 单生产者单消费者
 * - 中断通知任务
 * - 内存池对象传递
 * - 命令消息传递
 *
 * 选择指南：
 * - 数据较大(>64字节) → 使用邮箱(零拷贝)
 * - 需要缓冲多个消息 → 使用消息队列
 * - 单对单高频通信 → 使用邮箱
 */
typedef struct {
	flyos_ipc_base_t base;
	flyos_wait_queue_t send_queue;
	flyos_wait_queue_t recv_queue;
	void *message;
	bool full;
} flyos_mailbox_t;

/**
 * @brief 创建邮箱（支持静态/动态创建）
 * @details 创建并初始化一个邮箱对象，支持两种创建方式：
 *          - 动态创建：mbox_buffer=NULL，从堆中分配内存（需要FLYOS_USE_HEAP=1）
 *          - 静态创建：mbox_buffer!=NULL，使用用户提供的缓冲区（不依赖堆）
 *
 * @param[in,out] mbox_buffer  邮箱缓冲区指针
 *                             - NULL: 动态分配内存
 *                             - 非NULL: 使用用户提供的静态缓冲区
 *
 * @return 邮箱句柄，失败返回NULL
 *
 * @note Context: 任务上下文或初始化阶段
 * @note 静态创建时，mbox_buffer必须在邮箱整个生命周期内有效
 * @note FLYOS_USE_HEAP=0时，只能使用静态创建（mbox_buffer必须非NULL）
 * @note 邮箱只能容纳一个消息指针
 *
 * @code
 * // 动态创建示例（需要FLYOS_USE_HEAP=1）
 * flyos_mailbox_t *mbox = flyos_mailbox_create(NULL);
 *
 * // 静态创建示例（始终可用）
 * static flyos_mailbox_t mbox_buffer;
 * flyos_mailbox_t *mbox = flyos_mailbox_create(&mbox_buffer);
 * @endcode
 */
flyos_mailbox_t* flyos_mailbox_create(flyos_mailbox_t *mbox_buffer);

/**
 * @brief 删除邮箱
 * @details 删除邮箱并唤醒所有等待任务，返回错误码FLYOS_ERROR。
 *          自动识别静态/动态创建的对象：
 *          - 动态创建的对象：释放内存
 *          - 静态创建的对象：不释放内存（由用户管理）
 *
 * @param[in] mbox  邮箱句柄
 *
 * @return 成功返回FLYOS_OK
 *
 * @note Context: 任务上下文
 * @note 删除后不要再使用该邮箱句柄
 * @note 静态创建的对象不会释放内存，用户需自行管理缓冲区生命周期
 */
flyos_err_t flyos_mailbox_delete(flyos_mailbox_t *mbox);

/**
 * @brief 发送消息到邮箱
 * @details 尝试发送消息指针到邮箱。如果邮箱已满则阻塞等待。
 *          只传递指针，不拷贝数据(零拷贝)。
 *
 * @param[in] mbox     邮箱句柄
 * @param[in] msg      消息指针
 * @param[in] timeout  超时时间(tick)
 *
 * @return 成功返回FLYOS_OK，超时返回FLYOS_TIMEOUT
 *
 * @note Context: 任务上下文
 * @note Time complexity: O(1)
 */
flyos_err_t flyos_mailbox_send(flyos_mailbox_t *mbox, void *msg,
                               flyos_tick_t timeout);

/**
 * @brief 从邮箱接收消息
 * @details 从邮箱接收消息指针。如果邮箱空则阻塞等待。
 *
 * @param[in] mbox     邮箱句柄
 * @param[out] msg     指向接收指针的指针
 * @param[in] timeout  超时时间(tick)
 *
 * @return 成功返回FLYOS_OK，超时返回FLYOS_TIMEOUT
 *
 * @note Context: 任务上下文
 * @note Time complexity: O(1)
 */
flyos_err_t flyos_mailbox_receive(flyos_mailbox_t *mbox, void **msg,
                                  flyos_tick_t timeout);

/**
 * @brief 从ISR发送消息到邮箱
 * @details 在中断中发送消息指针。非阻塞。
 *
 * @param[in] mbox  邮箱句柄
 * @param[in] msg   消息指针
 *
 * @return 成功返回FLYOS_OK，邮箱满返回FLYOS_FULL
 *
 * @note Context: 中断上下文，可安全调用
 */
flyos_err_t flyos_mailbox_send_from_isr(flyos_mailbox_t *mbox, void *msg);

#endif /* FLYOS_USE_MAILBOX */

/*===========================================================================*/
/* 内核内部接口                                                               */
/*===========================================================================*/

/**
 * @brief 从阻塞对象的等待列表中移除任务
 * @details 仅供内部使用，在任务超时时调用。
 *          此函数处理信号量、互斥锁、队列、事件和邮箱的等待列表。
 *
 * @param[in] task  要移除的任务
 *
 * @note Context: 必须在临界区中调用
 */
void flyos_ipc_remove_task_from_wait_list(flyos_tcb_t *task);

#ifdef __cplusplus
}
#endif

#endif /* FLYOS_IPC_H */
