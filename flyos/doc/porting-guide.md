# flyOS Porting Guide

[简体中文](porting-guide.zh-CN.md)

This document is for developers who want to evaluate or understand the flyOS port boundary.

## 1. Current public ports

The current repository currently includes:

- `stm32f4`
- `s32k344`

These ports are available as reference implementations for the current codebase.

## 2. Port-layer responsibilities

The `port/` directory is responsible for:

- context switching;
- interrupt- and Tick-related low-level support;
- platform-specific low-level interfaces;
- boundary integration with the upper kernel layer.

## 3. What the upper project should already have

Before trying a port or integration, the upper project should usually already provide:

- startup files and a vector table;
- a linker script;
- clock initialization;
- the chip vendor HAL/SDK;
- low-level hardware initialization that matches the current port design.

## 4. What to read first when porting

Recommended reading order:

1. root `README.md`
2. `quick-start.md`
3. `configuration.md`
4. the implementation under `port/<platform>/`

## 5. Current scope of the repository

The current repository is intended to help developers:

- understand the platform boundary;
- try build and integration paths;
- provide porting feedback.

It does not currently claim broad platform coverage, certification-ready evidence, or frozen long-term APIs.
