# Contributing to flyOS

Thanks for considering a contribution to the public `flyos-standard` release line.

## What this repository is optimizing for

This first public line is optimized for:

- trial use on Cortex-M platforms;
- clear kernel-level learning and evaluation;
- honest capability boundaries;
- structured external feedback.

It is not optimized for large speculative feature drops or undocumented subsystem expansion.

## Before you contribute

Please read:

- `README.md`
- `ROADMAP.md`
- `FAQ.md`
- `flyos/doc/快速开始.md`
- `flyos/doc/移植指南.md`

## Good contribution types

- bug fixes with clear reproduction steps;
- documentation corrections and readability improvements;
- porting fixes for the currently advertised public targets;
- small, reviewable improvements to scheduler, IPC, memory, or platform-boundary code.

## Contribution rules

- Keep changes small and reviewable.
- Update docs when behavior or usage changes.
- Do not widen safety or certification claims in public docs.
- Avoid mixing unrelated cleanup with functional changes.
- Preserve the current public focus on the kernel rather than turning this branch into an internal dumping ground.

## Pull request checklist

- [ ] The change has a clear problem statement.
- [ ] The affected public docs are updated when needed.
- [ ] The target platform or reproduction context is described.
- [ ] The change does not add unsupported public claims.
- [ ] The change stays within the public kernel trial scope.

## Security issues

Do not disclose unpatched security-sensitive issues in public issue threads. Use the process described in `SECURITY.md`.
