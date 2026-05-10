# flyOS 配置指南

[English](configuration.md)

本文档说明当前公开版本中，与试用最相关的编译期配置项。

## 1. 配置入口

主要配置文件：

- `flyos_config.h`
- `flyos_port_config.h`

其中：

- `flyos_config.h` 负责平台无关的 RTOS 级配置；
- `flyos_port_config.h` 负责端口相关配置。

## 2. 重点配置项

### 2.1 调度配置

重点关注：

- `FLYOS_NUM_PRIORITY_LEVELS`
- `FLYOS_TICK_RATE_HZ`
- `FLYOS_TIME_SLICE_TICKS`
- `FLYOS_USE_PREEMPTION`
- `FLYOS_USE_TIME_SLICING`

### 2.2 任务与调试配置

重点关注：

- `FLYOS_MAX_TASK_NAME`
- `FLYOS_IDLE_STACK_SIZE`
- `FLYOS_USE_TASK_STAT`
- `FLYOS_USE_STACK_CHECK`

### 2.3 IPC 配置

重点关注：

- `FLYOS_USE_SEMAPHORE`
- `FLYOS_USE_MUTEX`
- `FLYOS_USE_QUEUE`
- `FLYOS_USE_EVENT`
- `FLYOS_USE_MAILBOX`
- `FLYOS_USE_PRIORITY_INHERIT`

### 2.4 内存配置

重点关注：

- `FLYOS_USE_HEAP`
- `FLYOS_HEAP_SIZE`
- `FLYOS_MEM_ALIGNMENT`
- `FLYOS_USE_MEM_STATS`
- `FLYOS_USE_MEM_CHECK`

## 3. 配置修改原则

- 先保证最小可构建，再做优化；
- 修改配置后重新完整编译；
- 避免在试用初期同时改太多参数；
- 先验证平台是否能正常启动，再调优性能或内存占用。

## 4. 当前公开建议

对于首次试用者，建议先保持默认配置，优先验证：

- 能否完成构建；
- 调度是否启动；
- Tick 和 IPC 是否符合预期；
- 当前平台端口边界是否清晰。
