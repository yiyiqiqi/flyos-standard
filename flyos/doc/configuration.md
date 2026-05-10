# flyOS Configuration Guide

[简体中文](configuration.zh-CN.md)

This document describes the build-time configuration items that matter most for trial use in the current public release.

## 1. Configuration entry points

Main configuration files:

- `flyos_config.h`
- `flyos_port_config.h`

Their roles are:

- `flyos_config.h` handles platform-independent RTOS-level configuration;
- `flyos_port_config.h` handles port-related configuration.

## 2. Key configuration items

### 2.1 Scheduler configuration

Pay particular attention to:

- `FLYOS_NUM_PRIORITY_LEVELS`
- `FLYOS_TICK_RATE_HZ`
- `FLYOS_TIME_SLICE_TICKS`
- `FLYOS_USE_PREEMPTION`
- `FLYOS_USE_TIME_SLICING`

### 2.2 Task and debug configuration

Pay particular attention to:

- `FLYOS_MAX_TASK_NAME`
- `FLYOS_IDLE_STACK_SIZE`
- `FLYOS_USE_TASK_STAT`
- `FLYOS_USE_STACK_CHECK`

### 2.3 IPC configuration

Pay particular attention to:

- `FLYOS_USE_SEMAPHORE`
- `FLYOS_USE_MUTEX`
- `FLYOS_USE_QUEUE`
- `FLYOS_USE_EVENT`
- `FLYOS_USE_MAILBOX`
- `FLYOS_USE_PRIORITY_INHERIT`

### 2.4 Memory configuration

Pay particular attention to:

- `FLYOS_USE_HEAP`
- `FLYOS_HEAP_SIZE`
- `FLYOS_MEM_ALIGNMENT`
- `FLYOS_USE_MEM_STATS`
- `FLYOS_USE_MEM_CHECK`

## 3. Rules for changing configuration

- Make the project minimally buildable before tuning.
- Rebuild the project fully after changing configuration.
- Avoid changing too many parameters at once during an early trial.
- Verify that the platform starts correctly before tuning performance or memory usage.

## 4. Current public recommendation

For first-time trial users, keep the default configuration first and verify:

- whether the project builds successfully;
- whether the scheduler starts;
- whether Tick and IPC behavior look correct;
- whether the platform-port boundary is clear.
