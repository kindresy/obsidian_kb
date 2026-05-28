---
date: 2026-05-28
tags: [pci, hardware]
type: concept
status: active
---

# PCI 桥（PCI Bridge）

PCI 桥是 PCI 与 [[pci-express|PCI Express]] 体系结构的精华所在。本书第2章重点介绍了 PCI 桥和配置机制。

## Details

### Type 1 Header 配置寄存器

- **Primary/Secondary/Subordinate Bus Number**：定义桥片管理的总线范围，核心寄存器
- **Secondary Status**：记录下游总线状态（DEVSEL 时序、错误状态）
- **Secondary Latency Timer**：下游总线的 Latency Timer
- **Memory Base/Limit**：1MB 最小粒度，定义下游存储器地址范围
- **I/O Base/Limit**：16/32 位可选，定义下游 I/O 地址范围
- **Prefetchable Memory Base/Limit**：定义可预取存储器地址范围
- **I/O Base/Limit Upper 16 Bits**：32 位 I/O 地址扩展
- **Bridge Control Register**：Secondary Bus Reset、Primary/Secondary Discard Timer（2^10 或 2^15 时钟）

### 配置机制

- **Type 01h → Type 00h 转换**：当配置请求中的总线号等于 Secondary Bus Number 时，桥片将 Type 01h 转换为 Type 00h 发往下游
- **DFS 总线枚举**：自上而下分配 Primary/Secondary 总线号，自底向上分配 Subordinate 总线号
- **设备号分配**：IDSEL 连到 AD[31:11]，每条总线最多 21 个设备（实际约 10 个），推荐 AD[17:20] 用于 4 插槽设计

### 地址译码

- **正向译码**：主线发往副线 — 地址匹配 Memory Base/Limit 窗口
- **负向译码**：副线发往主线 — 地址不匹配任何窗口时转发上游
- **负向译码桥（Class Code 0x060401）**：特殊用于 Dock 场景
- **桥合并/合并/塌缩**：Combining（连续 DW 合为突发）、Merging（同一 DW 的字节合并，需可预取空间）、Collapsing（重叠写合并，极少实现）

### 非透明桥（Non-Transparent Bridge）

- 用于连接两个独立的处理器系统
- Intel 21555 为例：双 PCI 配置空间（Primary + Secondary）、CSR 寄存器、BAR0-5 映射
- 地址转换：Secondary_Address = (Primary_Address - BAR_Base) + Translated_Base
- 两种模式：直接翻译（所有 BAR）、查找表翻译（Secondary Memory 2 BAR）
- Doorbell/I2O 机制用于中断信号传递

### 在 PCIe 中的演进

- PCIe Switch 的每个端口对应一个虚拟 PCI-PCI 桥
- 保留 Type 01h → Type 00h 转换逻辑
- 配置空间扩展为 4KB

## See Also

- [[pci-express-体系结构导读]]
- [[pci-bus]]
- [[root-complex]]

## Counter-Arguments and Gaps

...
