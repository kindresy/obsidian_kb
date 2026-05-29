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

## Counter-Arguments and Gaps

...

## Code References

- [[edge-pcie-core]] — edge-driver/edge.c — ACS redirection disable for P2P transfers; SR-IOV interrupt register handling
- [[edge-pcie-core]] — edge-driver/edge.h — SRIOV interrupt status/mask register definitions
