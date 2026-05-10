# FlyOS Long-Term Direction Note

[简体中文](high-safety-rtos-roadmap.zh-CN.md)

> Note: This file is not a primary document for the first public `flyos-standard` release and does not constitute a capability promise for the current public branch.

## 1. Purpose of this file

This file only explains the kinds of longer-term engineering directions FlyOS may care about, so readers can understand:

- why the public release emphasizes continuous evolution;
- why the current public wording is intentionally restrained;
- why the public trial line and the long-term engineering direction should not be treated as the same thing.

## 2. Relationship to the current public release

The goal of the current public `flyos-standard` release is to:

- help outside developers understand the kernel mainline;
- support STM32F4 and S32K344 trial use and feedback;
- establish a clear, honest, and sustainably maintainable open-source entry path.

This file does not mean:

- the project already has certification-grade capability today;
- the long-term high-safety goal is already complete;
- the public repository will expose all internal development directions at once.

## 3. Long-term areas of interest

In the long term, FlyOS may continue to care about:

- stronger reliability engineering capabilities;
- stricter verification and quality-control methods;
- clearer platform capability boundaries;
- higher-value systematic engineering capabilities.

These are long-term evolution topics and should not be mistaken for current public launch capabilities.

## 4. Recommendation for public readers

If you are touching flyOS for the first time, prioritize:

1. `README.md`
2. `quick-start.md`
3. `configuration.md`
4. `porting-guide.md`
5. `api-reference.md`

Rather than using this file as your trial entry point.
