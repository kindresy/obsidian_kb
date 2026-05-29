---
date: 2026-05-28
tags: [pci, hardware, architecture]
type: concept
status: active
---

# Root Complex（RC）

Root Complex（RC）是 [[pci-express|PCI Express]] 层次结构中连接 CPU 与 PCIe 域的根复合体。RC 的概念在 x86 处理器平台中真正存在，其他处理器系统没有严格意义上的 RC。

## Details

#### 组成结构（以 Montevina 为例）
- RC 由两个芯片组成：GMCH（Contiga）+ ICH9M
- 虚拟 FSB-to-PCI 桥将 FSB 总线与 PCI 总线 0 分离
- MCH 连接高带宽设备（DDR 控制器、显卡），ICH 连接低速设备（USB、低速 PCIe）
- DMI 接口连接 MCH 与 ICH
- MCH 使用正向译码，未命中时 ICH 通过负向译码转发

#### Device 0（RCRB — Root Complex Register Block）
Device 0 实质上是 HOST 主桥（VID=0x8086，DID=0x2A40，Class Code=0x060000），其扩展配置空间即为 RCRB：
- **PCIEXBAR**：ECAM 基地址（256MB，256MB 对齐）
- **MCHBAR**：GMCH 内部寄存器（16KB），含 DRAM 通道控制、时钟管理、ACPI 电源、温度管理
- **EPBAR**：Egress 端口控制（4KB）
- **DMIBAR**：DMI 接口控制（4KB）
- **TOM/TOLUD/TOUUD**：内存大小与边界控制
- **PAM0-6**：Shadow BIOS 属性
- **REMAPBASE/REMAPLIMIT**：Reclaim 机制
- **GGC**：显卡显存借用（GFX Stolen Memory）
- **SMRAM/ESMRAMC**：SMM 内存控制

#### 内存域地址拓扑
x86 的内存域分为三段：
1. **1MB-TOLUD**：主存 + GFX Stolen/TSEG（CPU 不可见部分）
2. **TOLUD-4GB**：PCI 总线地址空间（含 Legacy 1MB、ECAM、中断向量区）
3. **4GB-TOUUD**：>4GB 主存 + Reclaim 空间
4. **TOUUD-64GB**：扩展 PCI 空间（可选）

#### Reclaim 机制
x86 特有的地址转换：当物理内存 >4GB 时，TOLUD-4GB 的"PCI 空洞"中原来映射的 DRAM 可通过 ReclaimBase 重新映射到 4GB 以上。ReclaimBase = TOM - ME，大小 = 4GB - TOLUD（64MB 对齐）- GFX Stolen。

#### PowerPC 对比
- P4080 无严格意义的 RC，只有 PCIe 总线控制器
- 8 个 E500mc 核心通过 CoreNet 全互联网络互连
- OCeaN 连接 SRIO 和 PCIe，允许 PCIe 端口间直接通信
- PAMU 替代 ATMU，管理外设地址空间隔离
- PEX_CONFIG_ADDR/DATA 寄存器访问扩展配置空间（EXT_REGN 字段）
- CCSRBAR 以内存映射方式访问（而非 x86 的 PCI 配置周期）

#### x86 vs PowerPC 关键区别
- x86 的 RC 包含 Event Collector（错误消息和 PME 处理）
- x86 内存控制器属于 PCI 总线域；PowerPC 属于存储器域
- x86 的 RCRB 通过 PCI 配置周期访问；PowerPC 通过 CCSRBAR 内存映射

#### MSI 在 RC 中的处理
- x86：FSB Interrupt Message 直接写目标 CPU 的 Local APIC
- PowerPC：通过 PEXCSRBAR（RC BAR0）接收 MSI 写请求，再转发到 MPIC

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express]]
- [[pci-bridge]]
- [[pci-virtualization]]
- [[iommu]] — IOMMU 是 Root Complex 的一部分，处理设备 DMA 和中断翻译

## Counter-Arguments and Gaps

...

## Code References

- [[edge-pcie-driver]] — edge-driver — PCIe endpoint device under Root Complex; platform settings via `pcie_bus_configure_settings`
