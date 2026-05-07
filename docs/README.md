# Documentation Map

This directory contains the public-facing documentation for the first `flyos-standard` open-source release.

## Public files in `docs/`

- `flyOS-kernel-architecture.png` - kernel architecture image used by the root README
- `编码规范.md` - repository coding conventions
- `README.md` - this documentation map

## Public kernel docs in `flyos/doc/`

- `快速开始.md` - first integration path
- `API参考.md` - public API overview
- `架构设计.md` - runtime architecture summary
- `配置指南.md` - build-time configuration guide
- `移植指南.md` - public porting guidance
- `FlyOS高安全性RTOS演进计划.md` - long-term direction note, not a first-trial entry document
- `公开代码审计记录.md` - open-source release audit note

## Why both `docs/` and `flyos/doc/`

This release keeps two documentation levels:

- `docs/` for repository-level public guidance;
- `flyos/doc/` for kernel-specific technical notes.

For a first public release, this is acceptable and lower risk than moving files right before launch. A later cleanup can unify naming if the project wants a stricter open-source layout.

## What is intentionally not part of the public doc surface

The first public release does not treat internal planning notes, collaboration traces, or process drafts as public user documentation.

That means the public story is centered on:

- the kernel itself;
- trial integration;
- platform boundaries;
- contribution and feedback.
