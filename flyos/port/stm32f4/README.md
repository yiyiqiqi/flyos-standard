# STM32F4 Port Notes

[简体中文](README.zh-CN.md)

This directory contains the STM32F4 port for `flyos-standard`.

## Intended use

Use this port when evaluating the kernel on an STM32F4-based Cortex-M4 target.

## Expected surrounding environment

Your board project should already provide:

- startup code;
- vector table;
- clock initialization;
- STM32 HAL integration;
- linker-script support.

## Intended use

This port is provided for kernel evaluation, integration understanding, and bring-up work on STM32F4 targets.
