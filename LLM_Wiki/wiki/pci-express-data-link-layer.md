---
date: 2026-05-28
tags: [pcie, hardware, protocol]
type: concept
status: active
---

# PCIe 数据链路层（Data Link Layer）

[[pci-express|PCI Express]] 分层架构的中间层，位于事务层与物理层之间。

## Details

#### DLCMSM 状态机
数据链路层有三个主要状态：
- **DL_Inactive**：复位后/链路断开/被禁用
- **DL_Init**：VC0 流量控制初始化（FC_INIT1 → FC_INIT2）
- **DL_Active**：正常工作状态

#### DL_Down / DL_Up 通知
- **DL_Down**：下游端口丢弃 Posted 请求，UR 回应 Non-Posted 请求；上游复位内部逻辑并发 Hot Reset
- **DL_Up**：重新发送 Set_Slot_Power_Limit 消息

#### TLP 数据链路层封装
TLP 穿过数据链路层时添加：**2 字节 Sequence Number 前缀 + TLP + 4 字节 LCRC 后缀**
- Sequence Number：12 位计数器 NEXT_TRANSMIT_SEQ（0-4095，循环）

#### DLLP 格式与类型
DLLP 共 6 字节（1 Type + 3 数据 + 2 CRC），共 18 种类型：

| 类型 | 用途 |
|:--|:--|
| ACK | 确认接收成功（含 AckNak_Seq_Num） |
| NAK | 拒绝接收（含 AckNak_Seq_Num） |
| PM_Enter_L1 | 进入 L1 状态请求 |
| PM_Enter_L23 | 进入 L2/L3 状态请求 |
| PM_Active_State_Request_L1 | ASPM L1 请求 |
| PM_Request_Ack | 电源管理确认 |
| InitFC1/2 (P/NP/Cpl) | 流量控制初始化 |
| UpdateFC (P/NP/Cpl) | 信用更新通告 |
| Vendor Specific | 厂商自定义 |

#### ACK/NAK 滑动窗口协议
这是数据链路层最核心的可靠传输机制：

**发送端维护：**
- **NEXT_TRANSMIT_SEQ**：下一个发送 TLP 序号
- **ACKD_SEQ**：已确认的最后一个序号
- **Replay Buffer**：保存已发送但未确认的 TLP
- 当 (NEXT_TRANSMIT_SEQ - ACKD_SEQ) mod 4096 >= 2048 时停止发送（窗口满）

**接收端维护：**
- **NEXT_RCV_SEQ**：期望接收的下一个 TLP 序号
- **CRC 校验**：检查 LCRC
- **NAK_SCHEDULED 位**：设 1 后丢弃所有后续 TLP 直到重传成功

**重试机制：**
- **REPLAY_NUM 计数器**（2 位）：NAK 重传次数限制，溢出触发链路训练
- **REPLAY_TIMER**：发送后计时，超时触发链路恢复。经验公式与 Max_Payload_Size 成正比、与链路宽度成反比
- **ACKNAK_LATENCY_TIMER = REPLAY_TIMER / 3**：防止发送端因丢失 ACK 超时

**流程示例：**
1. 发送端发送 TLP1-7，序号 1-7
2. 接收端发现 TLP5 错误 → 发送 NAK（AckNak_Seq_Num=4）
3. 发送端收到 NAK → 清除 TLP3-4 的 Replay Buffer → 重传 TLP5-7
4. 接收端收到重传 → 发送 ACK（AckNak_Seq_Num=7）

**发送优先级：**
正在发送的 TLP/DLLP > PLP > NAK > ACK > 重传 TLP > 新 TLP > 其他 DLLP

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express]]
- [[pci-express-physical-layer]]

## Counter-Arguments and Gaps

...
