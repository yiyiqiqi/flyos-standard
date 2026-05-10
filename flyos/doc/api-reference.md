# flyOS API Reference

[简体中文](api-reference.zh-CN.md)

This document provides a high-level view of the core APIs in the current public `flyos-standard` release, so trial users can quickly understand the exposed capability boundary.

## 1. Header entry points

The main public headers in the trial stage include:

- `flyos_kernel.h` - kernel lifecycle, task, and scheduling interfaces
- `flyos_ipc.h` - IPC primitive interfaces
- `flyos_mem.h` - dynamic memory management interfaces
- `flyos_config.h` - build-time configuration items
- `flyos_type.h` - basic types, error codes, and shared constants

## 2. Kernel interfaces

Key areas to focus on:

- `flyos_kernel_init()`
- `flyos_kernel_start()`
- task create, delete, suspend, resume, and delay interfaces
- Tick, scheduler, and priority-related interfaces

## 3. IPC interfaces

The current public IPC capability includes:

- semaphore
- mutex
- queue
- event
- mailbox

During trial use, it is recommended to verify:

- create and delete flows;
- blocking and timeout semantics;
- usage boundaries between ISR context and task context;
- whether error codes behave as expected.

## 4. Memory interfaces

The public memory capability mainly includes:

- `flyos_mem_alloc()`
- `flyos_mem_free()`
- `flyos_mem_realloc()`
- `flyos_mem_calloc()`

If memory statistics are enabled, you can also review the related statistic interfaces.

## 5. Error codes and types

Common public types and constants live in `flyos_type.h`, including:

- `flyos_err_t`
- `flyos_tick_t`
- `flyos_task_config_t`
- enums for task state, block reasons, IPC types, and related concepts

## 6. Current boundary note

This file is an API-oriented entry document for public trial use. It is not a replacement for per-function comments.

If you need more detailed interface semantics, read the corresponding headers directly:

- `flyos_kernel.h`
- `flyos_ipc.h`
- `flyos_mem.h`
