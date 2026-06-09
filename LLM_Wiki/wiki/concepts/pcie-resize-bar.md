---
date: 2026-06-09
tags: [pci-express, computer-architecture]
type: concept
status: active
---

# PCIe Resizable BAR（ReBAR）

PCIe Resizable BAR（ReBAR/Resize BAR）允许 PCIe 设备的 BAR 不再是固定大小，而是由软件在设备支持的多个大小中选择合适的 MMIO 映射窗口。

## Details

### 基本原理

传统 PCI/PCIe 设备要求系统分配固定大小的 MMIO 地址空间，设备出厂时 BAR 寄存器的可寻址大小就已固定。ReBAR 使设备可以报告支持的多种 size（如 256MB、512MB、1GB），软件从中选择一个合适的进行配置。

- **硬件支持**：设备在配置空间中实现 Resizable BAR Capability 结构
- **软件配置**：BIOS/UEFI 或 OS 驱动在枚举阶段写入设备支持的 size 值
- **常见应用**：GPU 大显存映射（AMD Smart Access Memory/SAM）、高性能网卡、NVMe SSD

### 硬件能力结构

Resizable BAR Capability 位于 PCIe 扩展配置空间（Extended Configuration Space），每个 BAR 独立拥有 capability 寄存器：

- 支持的 size 列表以编码位图表示（2^N bytes，N 在特定范围内）
- 通过 Capability 中的 BAR Size 寄存器写入目标大小
- 与 64-bit BAR 配合使用，支持 >4GB 地址空间

### 实际影响

| 设备类型 | 典型场景 | 效果 |
|---------|---------|------|
| GPU | AMD SAM / NVIDIA ReBAR | CPU 可访问全部显存，提升部分游戏性能 5-15% |
| NVMe SSD | 大数据传输 | 减少 DMA 映射开销 |
| 网卡 | 大包处理 | 降低 MMIO 窗口切换次数 |

## See Also

- [[pci-express]] — PCI Express 总线概述
- [[pci-express-体系结构导读]] — 全书摘要

## Counter-Arguments and Gaps

...
