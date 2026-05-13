# flyOS Port Layer

[简体中文](README.zh-CN.md)

This directory contains the port boundary for the `flyos-standard` release line.

## Scope

The port layer is responsible for:

- context-switch support;
- tick and interrupt-related low-level hooks;
- platform-specific runtime glue;
- the boundary between kernel code and board-level HAL/SDK code.

## Supported platforms

- `stm32f4`
- `s32k344`

## What a user should expect

A user should be able to:

- locate the platform-specific port files;
- understand which lower-level responsibilities remain in the board project;
- see how `FLYOS_PLATFORM` selects the target implementation.

## What this layer does not promise

The port layer does not promise that every downstream board integration is turnkey. Users still need a valid startup environment, linker script, clock configuration, and HAL/SDK integration.
