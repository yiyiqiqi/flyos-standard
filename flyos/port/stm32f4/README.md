# STM32F4 Port Notes

This directory contains the STM32F4 public trial port for `flyos-standard`.

## Intended use

Use this port when evaluating the public kernel line on an STM32F4-based Cortex-M4 target.

## Expected surrounding environment

Your board project should already provide:

- startup code;
- vector table;
- clock initialization;
- STM32 HAL integration;
- linker-script support.

## Public trial goal

The goal of this port in the first public release is to support kernel trial, integration understanding, and feedback collection.
