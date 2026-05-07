# Security Policy

## Scope

Security-relevant reports are welcome for the public `flyos-standard` release line, especially in these areas:

- privilege or isolation failures inside the exposed kernel model;
- memory corruption triggered through public APIs;
- scheduler or IPC bugs that can produce unsafe runtime behavior;
- platform-port mistakes that break documented kernel assumptions.

## How to report

Please do not publish a full public issue for an unpatched vulnerability.

Instead, contact the maintainer privately through the agreed project contact path and include:

- affected commit or release;
- target platform;
- impact summary;
- reproduction steps or proof-of-concept;
- whether the issue affects scheduling, IPC, memory, or port behavior.

## Response expectations

Maintainers should aim to:

1. acknowledge receipt;
2. assess impact and affected scope;
3. prepare a fix or mitigation;
4. publish a coordinated disclosure when appropriate.

## Important boundary note

This repository does not currently claim certification-grade assurance. Reports should therefore be evaluated against the documented current behavior and limitations in `README.md`, `FAQ.md`, and `ROADMAP.md`.
