# S32K344 Port Notes

[简体中文](README.zh-CN.md)

This directory contains the S32K344 port for `flyos-standard`.

## Intended use

Use this port when evaluating the kernel on an S32K344-based Cortex-M7 target.

## Expected surrounding environment

Your board project should already provide:

- startup code;
- vector table;
- clock initialization;
- NXP SDK integration;
- linker-script support.

## Intended use

This port is provided for kernel evaluation, integration understanding, and bring-up work on S32K344 targets.
