---
date: 2026-05-28
tags: [pcie, hardware, protocol]
type: concept
status: active
---

# PCI/PCIe 事务排序规则（Ordering）

PCI 和 PCIe 总线定义了严格的事务排序规则，以保证数据一致性和避免死锁。本书第11章专门讨论此主题。

## Details

### 生产者-消费者模型

PCI/PCIe 使用生产者-消费者模型管理共享数据：

- **生产者**：写入数据后设置 Flag 位
- **消费者**：检测 Flag 位后读取数据
- **两种竞态条件**：
  1. Flag=1 在数据完全写入之前可见（跨越桥片时可能发生）
  2. Status=1 在数据完全读取之前可见

排序规则的设计目标正是防止这两种竞态条件。

### PCI 死锁场景

1. **缓冲管理死锁**：设备 A 和 B 共享 TX/RX 缓冲，各自重试对方的写操作。解决方案：分离 TX/RX 缓冲
2. **排序死锁**：发往设备 B 的 Posted 写卡在桥 A 的缓冲中，B 等待读完成，而读完成无法跨越桥片直到 Posted 写被刷出。解决方案：规则允许 PMW 超越 DRR/DWR

### PCI 总线排序规则（表 11-4）

PCI 总线定义五种事务类型：
- **PMW**（Posted Memory Write）— 存储器写
- **DRR**（Delayed Read Request）— 延迟读请求
- **DWR**（Delayed Write Request）— 延迟写请求
- **DRC**（Delayed Read Completion）— 延迟读完成
- **DWC**（Delayed Write Completion）— 延迟写完成

关键规则：
- PMW 不可超越 PMW（FIFO 排序）
- DRR/DWR 不可超越 PMW（读后写排序）
- DRC 不可超越 PMW
- PMW 可以超越 DRR/DWR（避免死锁）
- 延迟完成可以超越延迟请求

### PCIe VC 排序规则（表 11-5）

PCIe 在 VC（Virtual Channel）层面定义了更复杂的排序规则：

| 条件 | 描述 |
|------|------|
| Posted 请求 vs Posted 请求 | RO=0 时不可超越；RO=1 时可超越 |
| Posted 请求 vs 完成 | 默认 Posted 不可超越完成（与 PCI 兼容）；RO=1 时可超越 |
| 完成 vs Posted 请求 | 默认不可超越；RO=1 时可超越 |
| 读请求 vs Posted 请求 | 读请求不可超越 Posted 请求 |
| 完成 vs 读请求 | 完成不可超越读请求 |
| 不同 VC 间 | 无排序约束 |

### Relaxed Ordering（RO）

- 由 TLP Header 的 Attr[1] 位控制，Device Control 寄存器的 "Enable Relaxed Ordering" 位启用
- 允许同类型 TLP 乱序（后发存储器写可超越前一个被阻塞的存储器写）
- 从 PCI-X 引入，PCIe 继承
- Switch 的 "No RO-enabled PR-PR Passing" 位可强制严格排序
- RO 仅在同一 VC 内有效

### ID-Based Ordering（IDO）

- PCIe V2.1 引入，由 Attr[2] 位控制
- 当 IDO=1 时，不同 Requester ID 的 TLP 可以自由重排序
- 基于"数据流"概念，实现 VOQ（Virtual Output Queue）优化
- 减少 HOL（Head-of-Line）阻塞
- PLX PEX8518 使用了前身技术 "PLX-Specific Relaxed Ordering"

### MSI 排序陷阱

- 若数据 TLP 使用 TC0 而 MSI 使用 TC1（不同 VC），MSI 可能在数据之前到达
- 即使设备先发送数据 TLP，MSI 仍可能先抵达
- 解决方案：
  1. 确保 MSI 和数据使用相同 TC
  2. 在发送 MSI 前插入零长度读请求并等待完成（读刷新）

### 延迟事务规则（PCI）

- 只有 Non-Posted 事务使用延迟（Delayed）机制
- Retry 后，主设备必须重试
- 可预取空间的读完成可被丢弃
- 延迟请求与延迟完成之间无排序约束
- 延迟完成不可超越 Posted 写

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express-transaction-layer]]
- [[pcie-flow-control]]
- [[pci-express]]
- [[pci-bus]]

## Counter-Arguments and Gaps

- 实际硬件实现中，多数设备仅使用 TC0/VC0，排序问题不如理论分析复杂
- 多 VC 场景常见于高端交换机，普通 endpoint 很少涉及
