---
date: 2026-05-28
tags: [pcie]
type: query
question: "PCIe 物理层和事务层的关系是什么？"
informed-by:
  - "[[pci-express]]"
  - "[[pci-express-transaction-layer]]"
  - "[[pci-express-data-link-layer]]"
  - "[[pci-express-physical-layer]]"
status: filed
---

# PCIe 物理层和事务层的关系？

[[pci-express|PCI Express]] 采用严格的分层架构设计，物理层和事务层并非直接通信，而是通过中间的数据链路层衔接。

## 分层架构

PCIe 的三层结构从顶到底为：

```
Transaction Layer（事务层）   ← 生成/解析 TLP
        ↓ 传递 TLP
Data Link Layer（数据链路层） ← Ack/Nak 可靠传输、DLLP
        ↓ 传递（添加序列号+CRC）
Physical Layer（物理层）     ← 差分串行信号、链路训练
```

## 各层职责

**[[pci-express-transaction-layer|事务层]]**（最上层）：
- 接收来自软件层的内存读写、配置请求、I/O 请求
- 将这些请求封装为 **TLP（Transaction Layer Packet）**
- 负责基于信用（Credit）的 [[pcie-flow-control|流量控制]]
- 将 TLP 向下传递给数据链路层

**[[pci-express-data-link-layer|数据链路层]]**（中间层）：
- 在 TLP 头部添加序列号（Sequence Number）和尾部添加 CRC（LCRC）
- 使用 **Ack/Nak 协议**确保 TLP 可靠传输
- 管理 **DLLP（Data Link Layer Packet）** — 用于链路控制、电源管理、流量控制更新
- 将处理后的报文传递给物理层
- 接收来自物理层的报文，校验后传递给事务层

**[[pci-express-physical-layer|物理层]]**（最底层）：
- 负责将数字化数据转换为 **差分串行信号**发送和接收
- 管理 [[pcie-link-training|链路训练]] — 上电时自动协商链路宽度（x1/x4/x8/x16）和速率代际（Gen1~Gen5）
- 每条 Lane 包含一对差分 TX 和一对差分 RX
- 物理层是 PCIe 的"真正核心"（据《[[pci-express-体系结构导读]]》），但也是很多工程师最接触不到的内容

## 关键关系总结

| 维度 | 事务层 | 数据链路层 | 物理层 |
|------|--------|-----------|--------|
| **关注点** | 事务语义（读/写/配置） | 可靠性（重传/校验） | 信号完整性（电信号） |
| **数据单元** | TLP | DLLP / TLP + Seq + CRC | 串行比特流 |
| **通信方向** | 与软件/驱动交互 | 上下桥接 | 与对端设备物理连接 |
| **错误处理** | ECRC（端到端） | LCRC + Ack/Nak（链路级） | 8b/10b 或 128b/130b 编码 |

事务层和物理层之间**不直接交互**——所有 TLP 都必须经过数据链路层的序列号添加、CRC 校验和 Ack/Nak 重传机制。但隐含的关系是：**事务层的效率高度依赖物理层的信号质量**——物理层链路降速（如 Gen4→Gen3 协商失败）会直接影响事务层的带宽表现；而事务层的流控信用更新通过 DLLP 承载，需要物理层链路正常工作。
