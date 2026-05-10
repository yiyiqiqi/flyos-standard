# flyOS Architecture

[简体中文](architecture.zh-CN.md)

This document describes the core architecture boundaries that external trial users should understand in the current public `flyos-standard` release.

## 1. Architectural focus

The current public release focuses on the basic mainline of a lightweight RTOS kernel:

- task scheduling;
- Tick and delay handling;
- IPC primitives;
- dynamic memory management;
- Cortex-M platform port boundaries.

## 2. Public mainline

The current public mainline can be understood as:

```text
application -> flyOS kernel -> port abstraction -> board / HAL / SDK
```

Where:

- the application layer handles business logic and board integration;
- `flyOS kernel` handles scheduling, IPC, and kernel runtime;
- `port abstraction` handles context switching, low-level clocks, interrupts, and platform binding;
- board-level code handles chips, drivers, and the startup environment.

## 3. Subsystem responsibilities

### 3.1 `flyos_kernel.*`

Responsible for:

- kernel initialization and startup;
- task lifecycle management;
- scheduler behavior;
- Tick and time-slice fundamentals.

### 3.2 `flyos_ipc.*`

Responsible for:

- IPC primitives such as semaphore, mutex, queue, event, and mailbox;
- the basic mechanisms for blocking and wake-up;
- the foundational synchronization and communication capabilities exposed for public trial use.

### 3.3 `flyos_mem.*`

Responsible for:

- heap initialization;
- dynamic allocation, free, and reallocation;
- optional statistics and integrity checks.

### 3.4 `port/`

Responsible for:

- processor- and platform-specific context-switch implementation;
- interrupt, Tick, and low-level port interfaces;
- boundary integration with concrete HAL / SDK layers.

## 4. Current public boundary

The current public branch is meant to help outside users understand and try the kernel mainline rather than expose the complete internal development roadmap.

Therefore:

- public documentation is currently organized around the kernel;
- internal planning notes, drafts, or higher-value private directions are not treated as the launch narrative;
- long-term safety ambitions are not presented as current capabilities.
