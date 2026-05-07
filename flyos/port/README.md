# flyOS Port Layer

This directory contains the public port boundary for the `flyos-standard` release line.

## Scope

The public port layer is responsible for:

- context-switch support;
- tick and interrupt-related low-level hooks;
- platform-specific runtime glue;
- the boundary between kernel code and board-level HAL/SDK code.

## Public trial platforms

- `stm32f4`
- `s32k344`

## What a trial user should expect

A trial user should be able to:

- locate the platform-specific port files;
- understand which lower-level responsibilities remain in the board project;
- see how `FLYOS_PLATFORM` selects the target implementation.

## What this layer does not promise

The public port layer does not promise that every downstream board integration is turnkey. Users still need a valid startup environment, linker script, clock configuration, and HAL/SDK integration.
