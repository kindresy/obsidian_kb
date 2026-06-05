---
date: 2026-05-29
tags: [avsbus, power, hardware, synopsys]
type: source-summary
source-url: raw/articles/2026-05-29-dwc-avsbus-databook.pdf
title: "DWC_avsbus Databook — Adaptive Voltage Scaling Bus Controller"
status: compiled
---

# DWC_avsbus Databook

Synopsys DWC_avsbus 控制器 IP 数据手册（v1.00a-lca00, May 2025）。AVSBus 是一种串行总线协议，用于 SoC 与外部电源管理 IC（PMIC）之间的自适应电压调节通信。

## Key Topics
- AVSBus 协议：3-wire 串行接口（AVS_Clock, AVS_CDATA, AVS_TDATA）
- 帧结构：写帧、读帧、状态响应帧，含 Start Code、Command、Data、CRC 字段
- 传输模式：管线化传输、CRC 校验、目标重新同步
- 寄存器编程模型：时钟分频、采样延迟、FIFO 阈值、中断管理
- DMA 接口：与 DW_axi_dmac 的握手与突发传输
