# flyOS API 参考

[English](api-reference.md)

本文档给出当前公开 `flyos-standard` 版本的核心 API 视图，帮助试用者快速理解公开能力边界。

## 1. 头文件入口

公开试用阶段的主要头文件包括：

- `flyos_kernel.h` - 内核生命周期、任务与调度相关接口
- `flyos_ipc.h` - IPC 原语接口
- `flyos_mem.h` - 动态内存管理接口
- `flyos_config.h` - 编译期配置项
- `flyos_type.h` - 基础类型、错误码与公共常量

## 2. 内核接口

重点关注：

- `flyos_kernel_init()`
- `flyos_kernel_start()`
- 任务创建、删除、挂起、恢复、延时相关接口
- Tick、调度器、优先级相关接口

## 3. IPC 接口

当前公开 IPC 能力包括：

- semaphore
- mutex
- queue
- event
- mailbox

试用时建议优先验证：

- 创建与删除流程；
- 阻塞 / 超时语义；
- 中断上下文与任务上下文的使用边界；
- 错误码是否可预期。

## 4. 内存接口

公开内存能力主要包括：

- `flyos_mem_alloc()`
- `flyos_mem_free()`
- `flyos_mem_realloc()`
- `flyos_mem_calloc()`

如果启用了内存统计，还可以关注统计相关接口。

## 5. 错误码与类型

常见公共内容位于 `flyos_type.h`，包括：

- `flyos_err_t`
- `flyos_tick_t`
- `flyos_task_config_t`
- 任务状态、阻塞原因、IPC 类型等枚举

## 6. 当前边界说明

本文件是公开试用导向的 API 说明入口，不替代逐函数注释。

若需要更细的接口语义，请直接阅读对应头文件：

- `flyos_kernel.h`
- `flyos_ipc.h`
- `flyos_mem.h`
