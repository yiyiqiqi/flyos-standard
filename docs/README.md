# Documentation Map

[简体中文](README.zh-CN.md)

This directory contains the public-facing documentation for the `flyos-standard` open-source release.

## Public files in `docs/`

- `flyOS-kernel-architecture.png` - kernel architecture image used by the root README
- `coding-standard.md` - repository coding conventions
- `coding-standard.zh-CN.md` - Simplified Chinese translation of the coding standard
- `README.md` - this documentation map
- `README.zh-CN.md` - Simplified Chinese translation of this map

## Public kernel docs in `flyos/doc/`

- `quick-start.md` - first integration path
- `quick-start.zh-CN.md` - Simplified Chinese quick-start guide
- `api-reference.md` - public API overview
- `api-reference.zh-CN.md` - Simplified Chinese API overview
- `architecture.md` - runtime architecture summary
- `architecture.zh-CN.md` - Simplified Chinese architecture summary
- `configuration.md` - build-time configuration guide
- `configuration.zh-CN.md` - Simplified Chinese configuration guide
- `porting-guide.md` - public porting guidance
- `porting-guide.zh-CN.md` - Simplified Chinese porting guidance
- `high-safety-rtos-roadmap.md` - long-term direction note, not a first-trial entry document
- `high-safety-rtos-roadmap.zh-CN.md` - Simplified Chinese translation of the long-term direction note
- `open-source-audit.md` - open-source release audit note
- `open-source-audit.zh-CN.md` - Simplified Chinese audit note

## Why both `docs/` and `flyos/doc/`

This release keeps two documentation levels:

- `docs/` for repository-level public guidance;
- `flyos/doc/` for kernel-specific technical notes.

English filenames are primary. Simplified Chinese translations are provided as sibling files with the `.zh-CN.md` suffix.

## What is intentionally not part of the public doc surface

The first public release does not treat internal planning notes, collaboration traces, or process drafts as public user documentation.

That means the public story is centered on:

- the kernel itself;
- trial integration;
- platform boundaries;
- contribution and feedback.
