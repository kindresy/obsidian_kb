---
date: 2026-05-29
tags: [pcie, interrupt, iommu, virtualization]
type: concept
status: active
---

# Interrupt Remapping — 中断重映射

Interrupt Remapping 是 IOMMU 的核心功能之一，将设备发出的中断请求重映射到正确的目标 CPU/vCPU，是 PCIe 设备直通和中断虚拟化的关键基础设施。

## Details

### 问题背景

在虚拟化系统中，设备直通给 Guest OS 后，其中断需要直接投递到目标 vCPU，而不是先经过 VMM 再转发。传统 INTx/MSI/MSI-X 中断不感知虚拟化：

- Guest OS 迁移后，中断目标需要动态更新
- VMM 介入每个中断会引入 5-10K cycles 的开销
- Guest OS 被调度走时，中断不应该唤醒 VMM

### 工作原理

IOMMU 中的 Interrupt Remapping 硬件将设备中断 reroute：

```
Device MSI/MSI-X Message
     │
     ▼
IOMMU (Interrupt Remapping)
     │
     ├── Interrupt Table (DeviceID → vCPU/Vector)
     ├── Interrupt Entry Cache
     └── Posted Interrupt Descriptor
     │
     ▼
Target vCPU (Direct delivery, no VMM exit)
```

### Linux 中的 Interrupt Remapping

- Intel VT-d 的 Interrupt Remapping 通过 `Disable` 字段和 Interrupt Table 实现
- AMD IOMMU 提供类似的 Interrupt Remapping 能力
- Posted Interrupts（APICv/AVIC）允许中断直接写入 Guest 的虚拟 APIC 页面

### 与 MSI/MSI-X 的关系

- MSI/MSI-X 是设备生成中断消息的协议
- Interrupt Remapping 是对这些中断消息进行重定向的硬件机制
- 两者协同工作：MSI-X 提供灵活的中断源，IOMMU 提供灵活的路由目标

## See Also

- [[iommu]] — IOMMU 整体架构（Interrupt Remapping 是其三大核心功能之一）
- [[msi-msi-x]] — MSI/MSI-X 中断机制
- [[pci-interrupt-mechanism]] — PCI 中断机制概述
- [[pci-virtualization]] — PCI 虚拟化技术
- [[iommu-tutorial-asplos-2016]] — 源文献

## Counter-Arguments and Gaps

- 不同 IOMMU 架构（AMD/Intel/ARM）的 Interrupt Remapping 实现差异较大，缺少统一编程接口
- Posted Interrupts 依赖 CPU 的 APICv/AVIC 硬件支持，老旧平台不支持
