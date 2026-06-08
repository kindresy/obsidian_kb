---
compiled: true
date: 2026-05-28
tags: [pci, pcie, architecture, book]
type: source-summary
source-url: raw/articles/pci_express/ (39 chapter files: 00-preface + ch.01~ch.15 multi-part)
status: active
---

# PCI Express 体系结构导读

《PCI Express 体系结构导读》由王齐编著，机械工业出版社出版（2010年3月第1版，2016年6月第4次印刷）。本书以处理器体系结构为主线，深入介绍 PCI 与 [[pci-express|PCI Express]] 总线的组成与原理。

## Key Points

- 全书共三篇 15 章，约 704 千字
- **第I篇**（第1-3章）：PCI 总线基础知识 — 基本知识、PCI 桥与配置机制、数据交换与 Cache 一致性
- **第II篇**（第4-13章）：PCI Express 体系结构 — RC 组成（Montevina 平台）、事务层/数据链路层/物理层、链路训练、流量控制、MSI/MSI-X 中断、总线序、虚拟化技术
- **第III篇**（第14-15章）：Linux 系统中的 PCI 总线 — 初始化过程（ACPI/OpenFirmware）、中断处理
- 以 x86 和 [[powerpc|PowerPC]] 处理器为基础进行说明，详解 MPC8548/P4080 的 HOST 主桥实现
- 侧重 [[host-bridge|HOST 主桥]] 和 [[root-complex|RC（Root Complex）]] 等规范未详细定义的内容
- 第12章以 Capric 卡为例记录实际 PCIe 卡设计与 Linux 驱动开发

## Entities Mentioned

- [[wang-qi]] — 作者
- [[pci-bus]] — PCI 总线
- [[pci-express]] — PCI Express 总线
- [[root-complex]] — RC（Root Complex）
- [[host-bridge]] — HOST 主桥（PCI/PCIe 域与存储器域之间的桥梁）
- [[pci-bridge]] — PCI 桥与配置机制
- [[pci-interrupt-mechanism]] — PCI 中断机制（INTx 信号与中断同步）
- [[msi-msi-x]] — MSI/MSI-X 中断机制
- [[pci-express-transaction-layer]] — PCIe 事务层与 TLP
- [[pci-express-data-link-layer]] — PCIe 数据链路层与 ACK/NAK 协议
- [[pci-express-physical-layer]] — PCIe 物理层（8b/10b 编码、加扰器）
- [[pcie-link-training]] — PCIe 链路训练
- [[pcie-flow-control]] — PCIe 流量控制
- [[pcie-ordering]] — PCI/PCIe 事务排序规则
- [[pcie-ecam]] — PCIe 增强配置访问机制（ECAM/MMCFG）
- [[pcie-ats]] — PCIe 地址翻译服务（ATS/ATC）
- [[linux-pci-subsystem]] — Linux PCI 子系统
- [[capric-card]] — Capric 卡设计实践
- [[pci-virtualization]] — PCI 虚拟化技术（SR-IOV/MR-IOV/IOMMU）
- [[mesif-protocol]] — MESIF 协议与 QPI 互连
