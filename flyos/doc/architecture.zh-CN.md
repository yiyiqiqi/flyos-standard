# flyOS 架构设计

[English](architecture.md)

本文档描述 `flyos-standard` 当前公开版本中，外部试用者需要理解的核心架构边界。

## 1. 架构定位

当前公开版本聚焦一个轻量 RTOS 内核的基础主线：

- 任务调度；
- Tick 与延时；
- IPC 原语；
- 动态内存管理；
- Cortex-M 平台移植边界。

## 2. 公开主线

可以把当前公开主线理解为：

```text
application -> flyOS kernel -> port abstraction -> board / HAL / SDK
```

其中：

- 应用层负责业务逻辑与板级集成；
- `flyOS kernel` 负责调度、IPC 与内核运行时；
- `port abstraction` 负责上下文切换、底层时钟、中断和平台绑定；
- 板级代码负责芯片、驱动与启动环境。

## 3. 子模块职责

### 3.1 `flyos_kernel.*`

负责：

- 内核初始化与启动；
- 任务生命周期管理；
- 调度器行为；
- Tick 与时间片基础机制。

### 3.2 `flyos_ipc.*`

负责：

- semaphore、mutex、queue、event、mailbox 等 IPC 原语；
- 阻塞与唤醒的基础机制；
- 面向公开试用的基础同步和通信能力。

### 3.3 `flyos_mem.*`

负责：

- 堆内存初始化；
- 动态分配、释放、重分配；
- 可选的统计与完整性检查。

### 3.4 `port/`

负责：

- 处理器与平台相关的上下文切换实现；
- 中断、Tick 与底层移植接口；
- 与具体 HAL / SDK 的对接边界。

## 4. 当前公开边界

当前仓库重点是帮助使用者理解和评估 kernel 主线。

因此：

- 当前文档围绕 kernel 集成与行为组织；
- 平台端口通过实现边界进行说明；
- 长期工程目标不表述为当前能力承诺。
