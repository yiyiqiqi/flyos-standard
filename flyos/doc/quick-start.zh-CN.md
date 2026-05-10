# flyOS 快速开始

[English](quick-start.md)

本文档面向首次试用 `flyos-standard` 的嵌入式开发者，目标是在最少背景知识下完成一次最基本的集成理解。

## 1. 适用范围

当前公开版本主要面向以下用途：

- 学习 RTOS 内核的基础结构；
- 在 Cortex-M 工程中尝试接入内核；
- 评估调度、IPC、内存管理与移植边界；
- 形成试用反馈。

当前公开版本不承诺任何认证级安全或长期 ABI/API 稳定性。

## 2. 先准备什么

在接入 flyOS 前，你需要已经具备：

- 一个可构建的 Cortex-M 工程；
- 板级启动文件、链接脚本和基础时钟初始化；
- 对应平台的 HAL 或 SDK；
- 能与当前端口实现配套的中断、时钟和底层硬件初始化能力。

## 3. 把 flyOS 加入工程

在上层 `CMakeLists.txt` 中加入：

```cmake
set(FLYOS_PLATFORM "stm32f4" CACHE STRING "Target platform")
add_subdirectory(path/to/flyos-standard)

target_link_libraries(your_app PRIVATE flyos_kernel)
```

也可以在配置阶段指定平台：

```bash
cmake -S . -B build -DFLYOS_PLATFORM=stm32f4
```

当前公开试用平台：

- `stm32f4`
- `s32k344`

## 4. 目录怎么看

公开版核心目录如下：

```text
flyos/
├─ flyos_kernel.c / .h
├─ flyos_ipc.c / .h
├─ flyos_mem.c / .h
├─ flyos_config.h
├─ flyos_type.h
├─ port/
└─ doc/
```

建议理解顺序：

1. `README.zh-CN.md`
2. `quick-start.zh-CN.md`
3. `configuration.zh-CN.md`
4. `porting-guide.zh-CN.md`
5. `api-reference.zh-CN.md`
6. `architecture.zh-CN.md`

## 5. 最小构建接入

公开分支的最小构建目标是：

- 让上层工程能引用 `flyos_kernel`；
- 正确选择 `FLYOS_PLATFORM`；
- 让平台端口源码与上层 HAL/SDK 一起完成构建。

## 6. 试用时重点看什么

首次试用建议优先关注：

- 调度器是否按预期启动；
- Tick 与 delay 是否行为正确；
- IPC 原语是否满足最基本用法；
- 当前端口边界是否足够清晰；
- 文档是否足以支撑一次独立试用。

## 7. 下一步读什么

- 想看可调参数：读 `configuration.zh-CN.md`
- 想看端口边界：读 `porting-guide.zh-CN.md`
- 想看公开 API：读 `api-reference.zh-CN.md`
- 想看整体职责：读 `architecture.zh-CN.md`

## 8. Demo 仓库

如果你想直接从板级 STM32 演示工程开始，可以查看：

- GitHub：`https://github.com/yiyiqiqi/flyos-stm32-demo.git`
- Gitee：`https://gitee.com/yangyang__zhang_admin/flyos-stm32-demo.git`
