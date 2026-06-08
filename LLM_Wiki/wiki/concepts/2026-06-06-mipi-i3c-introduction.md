---
date: 2026-06-06
tags: [i3c, mipi, bus]
type: source-summary
source-url: raw/articles/i3c/AMF-DES-T2686.pdf
compiled: true
---

# MIPI I3C Technology — An Introduction

NXP 的 Michael Joehren 在 2017 年发表的 I3C 技术介绍，涵盖基本信令、协议、仲裁、HDR 模式、错误检测与恢复等。

## Key Points

- I3C 是 I2C 的下一代演进，兼容 I2C 从设备
- 支持单主和多主模式，带内中断（IBI），热插拔（Hot-Join）
- 三种 HDR 模式：DDR（双数据率）、TSP（三进制半精度）、TSL（三进制全精度）
- 使用 Provisional ID 进行设备识别和动态地址分配
- NXP 提供免费的 I3C Slave RTL 实现

## Entities Mentioned

- [[i3c]] — I3C 总线协议
- [[mipi-i3c-basic-spec]] — MIPI I3C Basic 规范
