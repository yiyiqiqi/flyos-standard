# 文档索引

[English](README.md)

本目录保存 `flyos-standard` 公开发布线面向外部使用者的文档。

## `docs/` 中的公开文档

- `flyOS-kernel-architecture.png` - 根 README 使用的内核架构图
- `coding-standard.md` - 仓库编码规范英文版
- `coding-standard.zh-CN.md` - 仓库编码规范简体中文版
- `README.md` - 当前文档索引英文版
- `README.zh-CN.md` - 当前文档索引简体中文版

## `flyos/doc/` 中的公开内核文档

- `quick-start.md` - 快速开始英文版
- `quick-start.zh-CN.md` - 快速开始简体中文版
- `api-reference.md` - 公开 API 概览英文版
- `api-reference.zh-CN.md` - 公开 API 概览简体中文版
- `architecture.md` - 运行时架构说明英文版
- `architecture.zh-CN.md` - 运行时架构说明简体中文版
- `configuration.md` - 构建期配置说明英文版
- `configuration.zh-CN.md` - 构建期配置说明简体中文版
- `porting-guide.md` - 平台移植说明英文版
- `porting-guide.zh-CN.md` - 平台移植说明简体中文版
- `high-safety-rtos-roadmap.md` - 长期方向说明英文版，不属于首次试用入口
- `high-safety-rtos-roadmap.zh-CN.md` - 长期方向说明简体中文版
- `open-source-audit.md` - 开源发布审计记录英文版
- `open-source-audit.zh-CN.md` - 开源发布审计记录简体中文版

## 为什么同时保留 `docs/` 和 `flyos/doc/`

当前公开版本保留两层文档结构：

- `docs/` 用于仓库级公开说明；
- `flyos/doc/` 用于内核相关技术文档。

英文文件名作为主文件名，简体中文版使用同名 `.zh-CN.md` 配对文件。

## 哪些内容刻意不作为公开文档面的一部分

首次公开发布不会把内部规划、协作痕迹或过程稿当作面向用户的正式文档。

也就是说，当前公开叙事聚焦于：

- kernel 本身；
- 试用接入；
- 平台边界；
- 贡献与反馈。
