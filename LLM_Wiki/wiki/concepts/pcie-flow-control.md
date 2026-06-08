---
date: 2026-05-28
tags: [pcie, hardware, protocol]
type: concept
status: active
---

# PCIe 流量控制（Flow Control）

[[pci-express|PCI Express]] 总线使用基于信用（Credit）的流量控制机制，管理层与层之间的数据传输。

## Details

#### 六种缓冲区类型
每个 VC 有独立跟踪的六种缓冲区：

| 类型 | 全称 | 单元大小 |
|------|------|----------|
| PH | Posted Header | 5 DW |
| PD | Posted Data | 4 DW |
| NPH | Non-Posted Header | 5 DW |
| NPD | Non-Posted Data | 1 DW |
| CplH | Completion Header | 4 DW |
| CplD | Completion Data | 4 DW |

TLP 需要同时拥有所有所需缓冲区类型的信用才能发送。例如 Memory Write 需要 1 PH + N PD 单元。

#### FCP（Flow Control Packet）
FC 信息通过 DLLP 传递：
- **InitFC1**：初始化信用值为 Buf_Alloc 值
- **InitFC2**：确认/验证初始化
- **UpdateFC**：持续信用通告
- 每种类型分 P/NP/Cpl 三个子类型，含 HdrFC、DataFC 和 VC ID 字段

#### 信用计算参数
- 发送端：CC（已消耗信用）、CL（信用上限）、CR（累计所需信用）
- 接收端：CA（已分配信用）、CRCV（已接收信用）
- Header 信用字段 8 位，Data 信用字段 12 位
- 允许发送条件：(CL - CR) mod 2^FieldSize <= 2^(FieldSize-1)

#### FC_INIT 状态
VC0 在数据链路层进入 DL_Init 后开始 FC 初始化：
1. **FC_INIT1**：发送 InitFC1-P/NP/Cpl，接收并记录对方信用。此阶段仍报告 DL_Down，允许丢弃未确认 TLP
2. **FC_INIT2**：发送 InitFC2 确认，等待 FI2 位置位后进入 DL_Active
3. **DL_Active**：开始正常 TLP 传输

#### Infinite FC
- 最终节点（EP、以及不转发 TLP 时的 RC）的 CplH/CplD 信用可设为 0
- 0 = "无限缓冲"，表示节点保证始终可以接收完成报文
- 实际需要内部预留足够缓冲空间

#### 缓冲管理
- **静态分配**：每个 VC 独立 N2+N3 缓冲区
- **自适应分配**：所有 VC 共享缓冲区池
- 发送端导向 vs 接收端导向的分配算法

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express-transaction-layer]]

## Counter-Arguments and Gaps

...
