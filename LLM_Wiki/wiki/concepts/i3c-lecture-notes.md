---
date: 2026-06-09
tags: [i3c, mipi, bus]
type: source-summary
source-url: raw/articles/i3c/ (7 lecture notes)
compiled: true
---

# I3C 总线协议 — 讲解笔记合集

I3C 协议学习过程中产生的 7 篇讲解笔记，覆盖第 3 章技术概述、第 4 章 SDR 核心协议、HDR 高速模式、Broadcast CCC 用法、T-bit/从机终止读能力分析，以及一份学习路径建议和一份驱动开发内部分享讲义。

## Key Points

- **术语变更**：Master/Slave → Controller/Target（新规范术语）
- **总线模型**：Primary Controller / Secondary Controller / Active Controller 三层角色
- **帧结构**：START → HEADER → DATA → STOP，START 后 Header 可仲裁
- **T-bit**：写时为奇校验位，读时由 Target 控制结束/继续
- **Broadcast CCC**：通过 7'h7E + W 发送给所有 Target，涵盖地址管理/事件控制/活动状态/HDR 入口
- **Bus Conditions**：Bus Free / Bus Available / Bus Idle，Target 发起请求的时序条件
- **IBI 优先级**：由动态地址编码决定，越低地址优先级越高
- **Clock Stalling**：Controller 在特定相位拉低 SCL 的流控机制
- **HDR 高速模式**：HDR-DDR（双边沿）/ HDR-TSP（三进制纯总线）/ HDR-TSL（三进制+传统 I2C）

## Entities Mentioned

- [[i3c]] — I3C 总线协议
- [[i2c]] — I2C 总线协议
