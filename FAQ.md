# FAQ

[简体中文](FAQ.zh-CN.md)

## Is flyOS production-ready?

Not as a general public promise. The current public line is an early open-source trial release intended for evaluation, learning, porting practice, and feedback.

## Does flyOS already meet functional-safety or aerospace certification requirements?

No. The project direction is reliability- and safety-oriented, but the current public release does not claim certification compliance.

## Why is the repository named `flyos-standard` while the project name is `flyOS`?

`flyOS` is the project and technical brand. `flyos-standard` is the name chosen for the first public release line to keep the scope conservative and clearly separated from private development directions.

## Why is the public story focused on the kernel?

Because the first public goal is a clean trial experience: understand the kernel, try the ports, run integration experiments, and provide feedback. Broader internal frameworks are not the first public promise.

## Which platforms should I try first?

The public trial line currently targets:

- `stm32f4`
- `s32k344`

## Where should I start reading?

Start with:

1. `README.md`
2. `flyos/doc/quick-start.md`
3. `flyos/doc/configuration.md`
4. `flyos/doc/porting-guide.md`
5. `flyos/doc/api-reference.md`

## Can I use it commercially?

The code is planned for release under Apache License 2.0. That license is generally commercial-friendly, but you should review the full license text and evaluate your own legal and engineering obligations.

## Where should I send trial feedback?

Use the issue templates in this repository. If the problem is security-sensitive, use the private reporting path described in `SECURITY.md`.
