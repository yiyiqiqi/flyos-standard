# flyOS

[简体中文](README.zh-CN.md)

flyOS is a lightweight RTOS kernel focused on task scheduling, IPC, memory management, and Cortex-M port abstraction, while evolving toward high-reliability and high-safety embedded scenarios.

![flyOS Kernel Architecture](flyos/doc/flyOS-kernel-architecture.png)

## Current Status

flyOS is in an early open-source stage.

It is intended for embedded developers who want to:

- study a compact RTOS kernel implementation;
- evaluate task scheduling, IPC, and memory-management design choices;
- try basic porting and board bring-up on Cortex-M platforms;
- provide feedback as the project evolves.

The current version does not claim compliance with any functional-safety, information-security, or aerospace certification standard.

## Core Capabilities

- Preemptive priority-based scheduling for Cortex-M targets.
- Time-slice scheduling and tick-driven delay handling.
- Core IPC primitives including semaphore, mutex, queue, event, and mailbox.
- Heap-based memory management with optional statistics and integrity checks.
- Portable platform boundary for STM32F4 and S32K344.

## Supported Platforms

| Platform | MCU family | Status |
| --- | --- | --- |
| `stm32f4` | Cortex-M4 | Available |
| `s32k344` | Cortex-M7 | Available |

## Quick Start

### 1. Add flyOS to your project

```cmake
set(FLYOS_PLATFORM "stm32f4" CACHE STRING "Target platform")
add_subdirectory(path/to/flyos-standard)

target_link_libraries(your_app PRIVATE flyos_kernel)
```

### 2. Select a platform when configuring

```bash
cmake -S . -B build -DFLYOS_PLATFORM=stm32f4
```

Available values:

- `stm32f4`
- `s32k344`

### 3. Read the onboarding docs

Start here:

- `flyos/doc/quick-start.md`
- `flyos/doc/configuration.md`
- `flyos/doc/porting-guide.md`
- `flyos/doc/api-reference.md`

## Demo Repositories

If you want a board-level STM32 demo project built around flyOS, start here:

- GitHub: `https://github.com/yiyiqiqi/flyos-stm32-demo.git`
- Gitee: `https://gitee.com/yangyang__zhang_admin/flyos-stm32-demo.git`

## Repository Layout

```text
.
├─ CMakeLists.txt
├─ README.md
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
   │  ├─ api-reference.md
   │  ├─ architecture.md
   │  ├─ configuration.md
   │  ├─ porting-guide.md
   │  └─ flyOS-kernel-architecture.png
   └─ port/
```

## Documentation

The main technical documents are:

- `flyos/doc/quick-start.md` - first integration path
- `flyos/doc/api-reference.md` - public kernel API overview
- `flyos/doc/architecture.md` - runtime architecture and subsystem responsibilities
- `flyos/doc/configuration.md` - build-time configuration guide
- `flyos/doc/porting-guide.md` - platform porting guidance

Additional repository guidance remains at the root, including `CONTRIBUTING.md`, `FAQ.md`, `ROADMAP.md`, and `SECURITY.md`.

## Current Scope

The current repository content focuses on:

- kernel runtime;
- IPC primitives;
- memory management;
- Cortex-M platform abstraction;
- STM32F4 and S32K344 ports.

It does not currently claim certification-grade safety coverage or frozen long-term ABI/API stability.

## Roadmap

See `ROADMAP.md` for the current development roadmap.

## Contributing

See `CONTRIBUTING.md` before opening issues or pull requests.

## Support

Feedback and issue reports are welcome.

Contact: `yangyang_zhang@yeah.net`

## Dual-Platform Repositories

- GitHub: `https://github.com/yiyiqiqi/flyos-standard.git`
- Gitee: `https://gitee.com/yangyang__zhang_admin/flyos-standard.git`

The two public repositories are intended to stay aligned. Feedback can be filed on either platform.

## License

This project is released under the Apache License 2.0. See `LICENSE` for details.
