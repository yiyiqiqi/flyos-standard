# FAQ

[简体中文](FAQ.zh-CN.md)

## Is flyOS production-ready?

Not yet as a general production-ready claim. The current release is better suited to evaluation, learning, porting practice, and feedback.

## Does flyOS already meet functional-safety or aerospace certification requirements?

No. The project direction values reliability and safety, but the current release does not claim certification compliance.

## Why is the repository named `flyos-standard` while the project name is `flyOS`?

`flyOS` is the project and technical brand. `flyos-standard` is the repository name used for the current open-source kernel line.

## Why is the public story focused on the kernel?

Because the current repository is centered on the kernel itself: understanding the core design, trying the available ports, running integration experiments, and providing feedback.

## Which platforms should I try first?

The current release currently targets:

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
