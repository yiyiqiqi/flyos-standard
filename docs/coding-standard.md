# flyOS Coding Standard

[简体中文](coding-standard.zh-CN.md)

This document summarizes the coding conventions that external contributors and trial users are most likely to need in the public `flyos-standard` release.

## 1. Goals

The point of the coding standard is not formality for its own sake. It is meant to:

- make kernel code easier to read;
- keep interface boundaries clear;
- make trial feedback and contributions easier to land;
- reduce the cost of understanding code across files and modules.

## 2. Basic Principles

- Let names reflect responsibilities directly.
- Keep interface and implementation layers clear.
- Use comments to explain boundaries, constraints, and reasons rather than to restate the code line by line.
- Prefer consistency over local cleverness.
- Keep changes small and keep documentation in sync.

## 3. File Organization

Recommended structure:

- keep each `.h` file and its corresponding `.c` file in the same responsibility-oriented directory;
- use clear lowercase snake_case filenames;
- let each file carry one main responsibility whenever possible.

## 4. Naming Conventions

- Public types use `flyos_*_t`
- Public functions use `flyos_*`
- Macros use `FLYOS_*`
- Platform and port names should remain readable and easy to search

## 5. Commenting Conventions

Focus comments on:

- what the object or function is responsible for;
- what the input and output constraints are;
- which boundary conditions need special attention;
- why the implementation is done this way instead of another.

Avoid meaningless comments that merely repeat what the code already says clearly.

## 6. Extra Rules for the Public Branch

For the public `flyos-standard` branch:

- do not overstate security or certification conclusions in public docs;
- do not mix internal planning, private directions, or process drafts into public explanations;
- keep public docs updated when behavior changes;
- prefer wording that is honest, concise, and actionable for trial users.
