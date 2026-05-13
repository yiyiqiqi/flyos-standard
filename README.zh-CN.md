# flyOS

[English](README.md)

flyOS 是一个轻量级 RTOS 内核，当前聚焦于任务调度、IPC、内存管理和 Cortex-M 平台抽象，并朝着高可靠、高安全的嵌入式工程方向持续演进。

![flyOS Kernel Architecture](flyos/doc/flyOS-kernel-architecture.png)

## 当前状态

flyOS 当前处于开源早期试用阶段。

它适合以下类型的嵌入式开发者：

- 学习紧凑 RTOS 内核实现；
- 评估任务调度、IPC 和内存管理的设计取舍；
- 在 Cortex-M 平台上尝试基础移植和板级 bring-up；
- 在公开 API 进一步稳定前提供结构化试用反馈。

当前公开版本不宣称满足任何功能安全、信息安全或航天认证标准。高可靠和高安全仍然是长期工程目标，而不是当前版本承诺。

## 核心能力

- 面向 Cortex-M 目标的抢占式优先级调度。
- 时间片调度和基于 Tick 的延时处理。
- 包含 semaphore、mutex、queue、event、mailbox 在内的核心 IPC 原语。
- 带可选统计与完整性检查的堆内存管理。
- 面向 STM32F4 和 S32K344 的可移植平台边界。

## 支持平台

| 平台 | MCU 家族 | 状态 |
| --- | --- | --- |
| `stm32f4` | Cortex-M4 | 公开试用目标 |
| `s32k344` | Cortex-M7 | 公开试用目标 |

## 快速开始

### 1. 将 flyOS 加入工程

```cmake
set(FLYOS_PLATFORM "stm32f4" CACHE STRING "Target platform")
add_subdirectory(path/to/flyos-standard)

target_link_libraries(your_app PRIVATE flyos_kernel)
```

### 2. 在配置阶段选择平台

```bash
cmake -S . -B build -DFLYOS_PLATFORM=stm32f4
```

当前可选值：

- `stm32f4`
- `s32k344`

### 3. 阅读上手文档

建议从这里开始：

- `flyos/doc/quick-start.zh-CN.md`
- `flyos/doc/configuration.zh-CN.md`
- `flyos/doc/porting-guide.zh-CN.md`
- `flyos/doc/api-reference.zh-CN.md`

## Demo 仓库

如果你想直接查看基于 flyOS 的 STM32 板级 demo 工程，可以从这里开始：

- GitHub：`https://github.com/yiyiqiqi/flyos-stm32-demo.git`
- Gitee：`https://gitee.com/yangyang__zhang_admin/flyos-stm32-demo.git`

## 仓库结构

```text
.
├─ CMakeLists.txt
├─ README.md
├─ README.zh-CN.md
└─ flyos/
   ├─ CMakeLists.txt
   ├─ flyos_kernel.c
   ├─ flyos_kernel.h
   ├─ flyos_ipc.c
   ├─ flyos_ipc.h
   ├─ flyos_mem.c
   ├─ flyos_mem.h
   ├─ flyos_config.h
   ├─ flyos_type.h
   ├─ doc/
   │  ├─ quick-start.md
   │  ├─ quick-start.zh-CN.md
   │  ├─ api-reference.md
   │  ├─ api-reference.zh-CN.md
   │  ├─ flyOS-kernel-architecture.png
   │  └─ ...
   └─ port/
```

## 文档入口

当前公开试用文档只保留首次上手和能力边界相关内容：

- `flyos/doc/quick-start.zh-CN.md` - 首次接入路径
- `flyos/doc/api-reference.zh-CN.md` - 公开内核 API 概览
- `flyos/doc/architecture.zh-CN.md` - 运行时架构与子模块职责
- `flyos/doc/configuration.zh-CN.md` - 构建期配置说明
- `flyos/doc/porting-guide.zh-CN.md` - 平台移植说明

仓库级支撑文档继续保留在根目录，例如 `CONTRIBUTING.zh-CN.md`、`FAQ.zh-CN.md`、`ROADMAP.zh-CN.md`、`SECURITY.zh-CN.md`。

## 当前公开边界

当前首个公开版本聚焦于内核试用体验。

当前公开内容包括：

- kernel runtime；
- IPC 原语；
- 内存管理；
- Cortex-M 平台抽象；
- STM32F4 与 S32K344 试用端口。

当前公开版本不承诺：

- 认证级安全结论；
- 成熟的长期 ABI/API 稳定性；
- 完整商业功能线；
- 私有规划笔记或内部协作过程材料。

## 路线图

当前公开路线图见 `ROADMAP.zh-CN.md`。

## 贡献

提交 issue 或 pull request 前，请先阅读 `CONTRIBUTING.zh-CN.md`。

## 支持

维护者欢迎公开试用反馈，并可为合适的公开试用场景提供免费的芯片扩展支持。

联系方式：`yangyang_zhang@yeah.net`

## 双平台仓库

- GitHub：`https://github.com/yiyiqiqi/flyos-standard.git`
- Gitee：`https://gitee.com/yangyang__zhang_admin/flyos-standard.git`

两个公开仓库的内容应保持同步，反馈可以提交到任一平台。

## 许可证

本项目基于 Apache License 2.0 发布，详见 `LICENSE`。
