---
date: 2026-05-28
tags: [pcie, virtualization, hardware]
type: concept
status: active
---

# PCIe 地址翻译服务（ATS / ATC）

ATS（Address Translation Services）是 PCIe 虚拟化核心技术，允许 PCIe 设备缓存地址翻译结果，减少 IOMMU 查询开销。本书第13.2节专门讨论。

## Details

### 工作原理

- PCIe 设备通过 ATC（Address Translation Cache）缓存 I/O 虚拟地址到物理地址的翻译结果
- 设备发出的 TLP 头部 AT 字段指示地址类型：
  - **0b00**：未翻译（Untranslated）— 普通地址，需 IOMMU 翻译
  - **0b01**：翻译请求（Translation Request）— 请求 IOMMU 翻译
  - **0b10**：已翻译（Translated）— 地址已翻译，IOMMU 直接转发
- 设备先发送 Translation Request，收到 Translated 完成后再用 Translated 地址发送数据

### TLP 格式

- **Translation Request**：使用 Message 请求，带 Requester ID 和要翻译的地址
- **Translated Address Completion**：返回翻译后的地址、大小及 Translation、Access、Non-Snoop 权限位

### ATC 无效化

- 操作系统切换地址映射时，需使设备 ATC 中的缓存条目无效
- 通过 **ATC Invalidation Request** Message 通知设备
- 设备收到后清除指定的 ATC 条目并回复 **ATC Invalidation Completion**

### 安全意义

- ATS 是安全虚拟化的关键：阻止恶意 VM 将 DMA 重定向到其他 VM 的设备
- 未经 ATS 翻译的地址由 IOMMU 捕获并翻译
- 不支持 ATS 的设备只能使用未翻译地址，完全依赖 IOMMU

### 相关技术

- **ATC**：设备端的地址翻译缓存（类似 CPU 的 TLB）
- **IOMMU**：系统级 I/O 内存管理单元（Intel VT-d / AMD IOMMU）
- **PASID**：进程地址空间 ID，允许设备访问进程虚拟地址空间

## See Also

- [[pci-express-体系结构导读]]
- [[pci-virtualization]]
- [[host-bridge]]
- [[root-complex]]

## Counter-Arguments and Gaps

- ATS 需要设备和 IOMMU 双方支持，实际部署有限
- ATC 一致性管理复杂，地址切换时开销较大

## See Also

- [[iommu]] — IOMMU 是 ATS 的服务端，提供地址翻译和页表管理
- [[pci-virtualization]] — PCI 虚拟化技术中 ATS 的集成
- [[iommu-tutorial-asplos-2016]] — IOMMU 教程中 ATS/PRI 部分
