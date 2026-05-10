# 常见问题

[English](FAQ.md)

## flyOS 现在适合直接量产吗？

不能作为通用公开承诺。当前公开版本是早期开源试用线，主要用于评估、学习、移植实践和反馈收集。

## flyOS 现在已经满足功能安全或航天认证要求了吗？

没有。项目方向确实关注可靠性和安全性，但当前公开版本不宣称满足认证要求。

## 为什么仓库名是 `flyos-standard`，而项目名是 `flyOS`？

`flyOS` 是项目和技术品牌名称。`flyos-standard` 是首条公开发布线使用的仓库名，用来让公开范围更克制，并与私有研发方向保持清晰边界。

## 为什么当前公开叙事聚焦 kernel？

因为首个公开目标是提供一个干净的试用入口：理解 kernel、尝试端口、做集成实验并提供反馈。更宽的内部框架并不是当前公开承诺。

## 我应该先试哪些平台？

当前公开试用线主要面向：

- `stm32f4`
- `s32k344`

## 我应该先读哪些文档？

建议从这里开始：

1. `README.zh-CN.md`
2. `flyos/doc/quick-start.zh-CN.md`
3. `flyos/doc/configuration.zh-CN.md`
4. `flyos/doc/porting-guide.zh-CN.md`
5. `flyos/doc/api-reference.zh-CN.md`

## 我可以商用吗？

代码计划基于 Apache License 2.0 发布。这个许可证通常对商用友好，但你仍然需要自行评估完整许可证文本，以及你的法律和工程责任。

## 试用反馈应该发到哪里？

请使用本仓库中的 issue 模板提交反馈。如果问题涉及安全敏感内容，请按 `SECURITY.zh-CN.md` 中描述的私下报告路径处理。
