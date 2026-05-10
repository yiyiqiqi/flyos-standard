# flyOS Quick Start

[简体中文](quick-start.zh-CN.md)

This document is intended for embedded developers who are trying `flyos-standard` for the first time. Its goal is to help you form a basic integration-level understanding with minimal background assumptions.

## 1. Scope

The current public release is mainly intended for:

- learning the basic structure of an RTOS kernel;
- trying a kernel integration path inside a Cortex-M project;
- evaluating scheduling, IPC, memory management, and port boundaries;
- producing trial feedback.

The current public release does not promise certification-grade safety or long-term ABI/API stability.

## 2. What you should prepare first

Before integrating flyOS, you should already have:

- a buildable Cortex-M project;
- board startup files, a linker script, and basic clock initialization;
- the HAL or SDK for the target platform;
- low-level interrupt, clock, and hardware initialization support that matches the current port design.

## 3. Add flyOS to your project

In the upper-level `CMakeLists.txt`, add:

```cmake
set(FLYOS_PLATFORM "stm32f4" CACHE STRING "Target platform")
add_subdirectory(path/to/flyos-standard)

target_link_libraries(your_app PRIVATE flyos_kernel)
```

You can also select the platform during configuration:

```bash
cmake -S . -B build -DFLYOS_PLATFORM=stm32f4
```

Current public trial platforms:

- `stm32f4`
- `s32k344`

## 4. How to read the repository

The core public directories are:

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

Recommended reading order:

1. `README.md`
2. `quick-start.md`
3. `configuration.md`
4. `porting-guide.md`
5. `api-reference.md`
6. `architecture.md`

## 5. Minimum build integration target

The minimum build target of the public branch is to:

- let the upper project link against `flyos_kernel`;
- select `FLYOS_PLATFORM` correctly;
- build the platform port sources together with the upper HAL/SDK project.

## 6. What to focus on during trial use

For a first trial, focus on:

- whether the scheduler starts as expected;
- whether Tick and delay behavior are correct;
- whether the IPC primitives satisfy the most basic use cases;
- whether the current port boundary is clear enough;
- whether the documentation is sufficient for an independent trial.

## 7. What to read next

- If you want tunable parameters: read `configuration.md`
- If you want port boundaries: read `porting-guide.md`
- If you want the public API surface: read `api-reference.md`
- If you want subsystem responsibilities: read `architecture.md`

## 8. Demo repositories

If you want to start from a board-level STM32 demonstration project, see:

- GitHub: `https://github.com/yiyiqiqi/flyos-stm32-demo.git`
- Gitee: `https://gitee.com/yangyang__zhang_admin/flyos-stm32-demo.git`
