---
date: 2026-05-28
tags: [pci, interrupt, hardware]
type: concept
status: active
---

# PCI 中断机制（INTx）

PCI 总线定义了一套基于边带信号的中断机制，通过四根中断请求信号 INTA#/B#/C#/D# 向处理器提交中断请求。这是 [[msi-msi-x|MSI/MSI-X]] 中断机制的前身。

## Details

### INTx 信号

- 四个中断请求信号：INTA#、INTB#、INTC#、INTD#
- 支持信号共享：多个 PCI 设备可以通过"线与"方式连接到同一根中断线
- 属于边带信号（Sideband Signals），与数据路径分离

### 中断路由

PCI 桥的 INTx 映射规则（基于 Device Number mod 4）：

| Device # | INTA | INTB | INTC | INTD |
|----------|------|------|------|------|
| 0,4,8.. | INTA | INTB | INTC | INTD |
| 1,5,9.. | INTB | INTC | INTD | INTA |
| 2,6,10.. | INTC | INTD | INTA | INTB |
| 3,7,11.. | INTD | INTA | INTB | INTC |

### 中断同步问题

INTx 是异步信号 — 它不遵循 DMA 写数据的路径。因此 DMA 数据可能尚未到达内存时中断已经触发。

两种解决方案：
1. **设备读刷新（Read Flush）**：设备在发送中断前，先读自己的写目标地址（强制写完成）
2. **驱动读状态寄存器**：标准做法 — 驱动总是先读设备中断状态寄存器（该读操作强制未完成的写操作完成）

### MSI 作为替代

PCI 总线在 2.2 规范中引入了 MSI（Message Signaled Interrupt）作为 INTx 的可选替代。MSI 使用存储器写事务代替边带信号，从根本上解决了同步问题。在 PCIe 中，MSI/MSI-X 成为必选，INTx 仅用于向前兼容。

## See Also

- [[pci-express-体系结构导读]]
- [[msi-msi-x]]
- [[pci-bus]]
- [[pci-bridge]]

## Counter-Arguments and Gaps

- INTx 中断在线路共享时无法区分中断来源，驱动需要逐个设备查询中断状态寄存器
- 中断同步问题仅靠软件解决增加了驱动复杂度

## Code References

- [[edge-pcie-core]] — edge-driver/edge.c:4999 — INTx fallback when MSI/MSI-X unavailable (IRQF_SHARED for exception events)
- [[edge-pcie-core]] — edge-driver/edge.c:4903 — MSI vs MSI-X vs INTx IRQ setup hierarchy
