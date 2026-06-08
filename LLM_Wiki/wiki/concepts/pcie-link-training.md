---
date: 2026-05-28
tags: [pcie, hardware]
type: concept
status: active
---

# PCIe 链路训练（Link Training）

[[pci-express|PCI Express]] 上电或复位后，物理层自动执行的链路初始化过程。

## Details

#### LTSSM 状态机
PCIe 物理层的 LTSSM（Link Training and Status State Machine）管理链路训练全生命周期：
- **Detect**：检测接收端是否存在（RX 阻抗检测，40-60Ω 有效）
- **Polling**：建立 Bit Lock 和 Symbol Lock，交换 TS1/TS2 序列
- **Configuration**：确定链路宽度、Lane 映射、De-skew（支持 Lane Reversal 和 Polarity Inversion）
- **L0**：正常工作状态
- **L0s/L1**：低功耗状态（ASPM 硬件自动管理）
- **L2**：主电源关闭状态（需 Beacon 或 WAKE# 唤醒）
- **Recovery**：速率更改、错误恢复
- **Hot Reset**：通过 TS1 序列复位下游设备
- **Disabled**：端口禁用
- **Loopback**：调试测试模式

#### 关键训练步骤
1. **Bit Lock**：接收器从数据流提取时钟（CDR 锁定）
2. **Symbol Lock**：识别 COM 字符（K28.5，0011111010/1100000101）建立字符对齐
3. **Receiver Detect**：TX 通过测量 DC 共模输入阻抗（40-60Ω 在线 vs >50kΩ 离线）检测 RX 是否存在
4. **De-skew**：补偿 Lane 间传播延迟差异（在 Configuration.Complete 完成）
5. **Lane Number 协商**：物理 Lane → 逻辑 Lane 映射，支持 Lane Reversal（PCB 走线优化）

#### 速率协商
- 初始训练始终在 2.5 GT/s（Gen1）进行
- 达到 L0 后通过 Recovery 状态升级到更高速率
- speed_change 位在 TS1/TS2 序列中传递

#### Elastic Buffer 与 SKIP
- 每个 Lane 每 1180-1538 字符发送一次 SKIP 序列（K28.0）
- Elastic Buffer 是同步 FIFO，使用 SKIP 吸收 ±300ppm 时钟漂移
- 防止缓冲区上溢/下溢

#### Ordered-Sets（有序集）
| 类型 | 名称 | 用途 |
|------|------|------|
| TS1 | Training Sequence 1 | 链路训练初始协商 |
| TS2 | Training Sequence 2 | 链路配置确认 |
| EIOS | Electrical Idle Ordered Set | 进入电气空闲 |
| EIEOS | Electrical Idle Exit Ordered Set | 退出电气空闲 |
| FTS | Fast Training Sequence | 退出 L0s 状态 |
| SKP | Skip Ordered Set | 时钟补偿 |

#### Polarity Inversion & Lane Reversal
- Polarity Inversion：D+/D- 极性交换（PCB 走线优化，训练时自动检测）
- Lane Reversal：Lane 顺序反转（如 x8 链路的 Lane 7↔Lane 0）
- 两者可同时使用
- 对软件透明

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express-physical-layer]]

## Counter-Arguments and Gaps

...
