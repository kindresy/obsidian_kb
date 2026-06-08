---
date: 2026-05-29
tags: [iommu, virtualization, hardware, pcie]
type: concept
status: active
---

# IOMMU — IO Memory Management Unit

IOMMU（IO Memory Management Unit）是系统芯片中负责设备 DMA 和中断管理的硬件单元，类似 CPU 的 MMU 但面向 IO 设备。它拦截所有 DMA 事务，提供地址翻译和内存保护。

## Details

### 核心功能
1. **DMA 重映射（DMA Remapping）**：将设备发起的 DMA 地址（IO Virtual Address / Bus Address）翻译为系统物理地址（SPA）
2. **内存保护（Memory Protection）**：防止恶意或出错的设备访问未授权的内存区域（DMA Attack 防御）
3. **中断重映射（Interrupt Remapping）**：将设备中断路由到正确的 vCPU，支持中断投递（Interrupt Posting）
4. **共享地址空间（Shared Virtual Memory）**：设备共享 CPU 进程页表，实现 "Pointer-is-a-Pointer"

### 工作原理

```
Device DMA Request
     │
     ▼
IOMMU (Translation Agent)
     │
     ├── Device Table (DevID → Domain)
     ├── Page Tables (IOVA → SPA)
     └── IOTLB (translation cache)
     │
     ▼
Memory Controller → SPA
```

- **Device Table**：每个设备（由 PCIe DevID 标识）分配一个 Domain ID
- **Page Tables**：Domain 级别的地址映射表，结构与 CPU 页表类似
- **IOTLB**：IOMMU 内部的翻译缓存，加速重复查询
- **ATC**（Address Translation Cache）：支持 ATS 协议的设备可缓存翻译结果在设备本地

### IOMMU 技术家族

| 实现 | 全称 | 主要应用 |
|------|------|---------|
| **AMD IOMMU** | IO Memory Management Unit | AMD 处理器平台 |
| **Intel VT-d** | Virtualization Technology for Directed IO | Intel 处理器平台 |
| **ARM SMMU** | System Memory Management Unit | ARM 处理器平台 |
| **IBM CAPI** | Coherent Accelerator Processor Interface | POWER 处理器平台 |

### 虚拟化中的角色

在虚拟化系统中，IOMMU 解决三个关键问题：

1. **设备直通（Device Passthrough）**：Guest OS 直接操作物理设备，IOMMU 确保 DMA 只能访问该 Guest 的内存
2. **中断虚拟化**：IOMMU 重映射设备中断到目标 vCPU，支持 Posted Interrupts 减少 VMM 介入
3. **地址翻译**：GVA → GPA → SPA 的两层翻译，IOMMU 承担第二层翻译

### 与 ATS/PRI 的协作

- **ATS（Address Translation Services）**：PCIe 协议扩展，允许设备通过 IOMMU 预翻译地址并缓存在 ATC 中
- **PRI（Page Request Interface）**：允许设备在缺页时向 OS 发起页请求，实现设备访问页式内存

### 性能考量

- 无 IOMMU 时的 Bounce Buffer 方案有 ~30% 性能开销
- IOTLB 缺失需遍历页表（page walk），代价较高
- ATC 将翻译缓存放到设备端，减少 IOMMU 查询次数

## See Also

- [[pci-virtualization]] — PCI 虚拟化技术（含 IOMMU 基本原理）
- [[pcie-ats]] — PCIe ATS/ATC 地址翻译服务
- [[pci-interrupt-mechanism]] — 中断机制（含 MSI/MSI-X）
- [[root-complex]] — Root Complex 与 IOMMU 的物理位置
- [[iommu-tutorial-asplos-2016]] — ASPLOS 2016 IOMMU 教程源

## Counter-Arguments and Gaps

- IOMMU 增加 DMA 延迟（每次翻译查表），对延迟敏感的加速器需要 ATC 缓解
- 不同厂商的 IOMMU 实现在 API 和功能上有较大差异，缺少统一抽象层
- SVM 在实践中的性能表现取决于 TLB 缺失率和 ATS/PRI 的硬件支持程度
