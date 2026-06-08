---
date: 2026-05-28
tags: [pcie, virtualization]
type: concept
status: active
---

# PCI 虚拟化技术

[[pci-express|PCI Express]] 总线与虚拟化相关技术，本书第13章专门讨论。

## Details

### IOMMU 基本原理
- 类似 MMU 但用于设备地址：PCI 总线地址 → HPA（主机物理地址）
- I/O 页表 + IOTLB 作为缓存
- 域隔离：不同域的 PCI 设备可以使用相同 PCI 总线地址访问不同物理内存区域
- 三大核心功能：DMA 重映射、内存保护、中断重映射（参见 [[iommu]]）
- 与 PCIe ATS/PRI 协议协作，支持设备端地址翻译缓存和缺页请求

### 中断重映射（Interrupt Remapping）
- IOMMU 拦截设备 MSI/MSI-X 中断消息，重映射到目标 vCPU
- Posted Interrupts：中断直接投递到 Guest 的虚拟 APIC，无需 VMM 介入
- 解决 Guest OS 迁移后的中断路由更新问题
- 消除 VMM 处理中断时 5-10K cycles 的额外开销

### IOMMU 地址翻译流程
- Device Table：每个 PCIe 设备（由 DevID 标识）映射到一个 Domain
- 第一级翻译（如果启用）：GVA → GPA（Guest 管理）
- 第二级翻译：GPA → SPA（VMM/IOMMU 管理）
- IOTLB 缓存翻译结果，ATS 协议可将翻译缓存到设备端 ATC

### IOMMU 技术家族
- [[iommu]] 页面详述了 AMD IOMMU、Intel VT-d、ARM SMMU、IBM CAPI 四种实现

### Intel VT-d
- Root Entry Table（256 条，每 PCI 总线一条）+ Context Entry Table（256 条，每 Function 一条）
- Device-to-Domain Mapping
- 三级 4KB 页表，支持 Super Pages（2MB/1GB/512GB/1TB）
- Context Cache + IOTLB 缓存
- 使 32 位 PCI 设备可访问 >4GB 内存

### AMD IOMMU
- Device Table（最多 2^16 条，256b/条，最大 2MB）
- 四级 I/O 页表（基于 AMD64 修改）
- NextLevel 字段可跳过中间级（Level 4 可直接指向 Level 2）
- 浮动页大小（NextLevel=0b111 时 4KB~4GB）

### SR-IOV（第13.3.1节）
- PF（Physical Function）：独立配置空间
- VF（Virtual Function）：共享 PF 配置空间，独立 BAR0~5 空间
- 每个 VF 可绑定到不同 VM
- 类似多线程概念应用于 I/O 设备

### MR-IOV（第13.3.2节）
- 创建多个虚拟 PCI 总线域（VH - Virtual Hierarchy）
- MRA Switch：支持 0+ 上游端口、0+ 下游端口、多 P2P 桥集
- BF（Base Function，VH=0 仅限）：管理 MR-IOV Capability
- PF 格式："PF h:f"（h=VH, f=function）
- VF 格式："VF h:f,s"
- 截至本书写作时无实际硬件（无 MRA RP 或 Switch 芯片）

### ATS 集成
- ATS 是 PCIe 虚拟化的关键使能技术，参见 [[pcie-ats]]
- PCIe 设备通过 ATC 缓存地址翻译
- AT 字段标记地址类型（未翻译/翻译请求/已翻译）
- 阻止恶意 VM 将 DMA 重定向到其他 VM

## See Also

- [[pci-express-体系结构导读]]
- [[root-complex]]
- [[pcie-ats]]
- [[host-bridge]]
- [[iommu]] — IOMMU 详细原理
- [[interrupt-remapping]] — 中断重映射机制
- [[iommu-tutorial-asplos-2016]] — ASPLOS 2016 IOMMU 教程

## Counter-Arguments and Gaps

...

## Implementations

