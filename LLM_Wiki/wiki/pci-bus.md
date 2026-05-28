---
date: 2026-05-28
tags: [pci, hardware, bus]
type: concept
status: active
---

# PCI 总线

PCI（Peripheral Component Interconnect）总线是 PCI Express 的前身，是一种并行总线标准。

## Details

### 总线信号

- **AD[31:0]**：地址/数据复用信号。地址周期传地址，数据周期传数据
- **C/BE[3:0]#**：命令/字节选通复用。地址周期表示总线命令类型，数据周期为字节选通
- **PAR**：AD[31:0] 和 C/BE[3:0] 的奇偶校验
- **FRAME#**：事务开始/结束指示（主设备驱动）
- **IRDY#**：主设备数据就绪
- **TRDY#**：目标设备数据就绪（与 IRDY# 配合插入等待周期）
- **STOP#**：目标设备请求终止事务（Retry/Disconnect/Target Abort）
- **IDSEL**：配置空间片选信号，由 AD[31:11] 线的不同位实现设备号选择
- **DEVSEL#**：目标设备地址译码完成（分快速/中速/慢速译码）
- **LOCK#**：锁定目标设备资源（仅 [[host-bridge|HOST 主桥]] 和 PCI 桥可使用）
- **PERR# / SERR#**：数据/地址奇偶校验错误
- **64位扩展**：AD[63:32]、C/BE[7:4]#、REQ64#、ACK64#、PAR64
- **中断信号**：INTA#/B#/C#/D#（四根中断请求信号，支持共享）

### 总线事务类型

C/BE[3:0]# 命令类型：

| 命令 | 类型 | 说明 |
|------|------|------|
| 0000 | Interrupt Acknowledge | 中断响应 |
| 0001 | Special Cycle | 信息广播 |
| 0010 | I/O Read | I/O 读 |
| 0011 | I/O Write | I/O 写 |
| 0110 | Memory Read | 存储器读 |
| 0111 | Memory Write | 存储器写 |
| 1010 | Configuration Read | 配置读 |
| 1011 | Configuration Write | 配置写 |
| 1100 | Memory Read Multiple | 多行存储器读 |
| 1101 | Dual Address Cycle | 64位地址周期 |
| 1110 | Memory Read Line | 单行存储器读 |
| 1111 | Memory Write and Invalidate | 存储器写并无效 |

### 三种数据传送方式

- **Posted（发布式）**：仅 Memory Write，写后立即释放总线资源，最快
- **Non-Posted（非发布式）**：Memory Read、I/O、Config — 阻塞总线直到完成
- **Delayed（延迟式）**：16 Clock 内无法完成时 Retry。DRR/DRC/DWR/DWC 四种延迟事务类型
- **Split（拆分式）**：PCI-X/PCIe 演进，Completer 主动发数据，Requester 无需重试

### 地址空间与 DMA

- **PCI 总线域 vs 存储器域**：PCI 设备使用 PCI 总线域地址，处理器使用存储器域地址，通过 [[host-bridge|HOST 主桥]] 转换
- **DMA 流程**：PCI 设备通过 HOST 主桥（Inbound 转换）访问主存储器，需与 Cache 做一致性操作
- **PCI 设备间通信**：同一总线上的设备可直接通信，不同总线需经 PCI 桥转发
- **BAR 寄存器**：6 个基地址寄存器，保存 PCI 总线域地址。通过写 0xFFFFFFFF 后读回确定大小

### 配置空间

- 每个 PCI 设备 256 字节配置空间（前 64 字节标准 Header）
- Type 0 Header：Endpoint，含 6 个 BAR、ROM 基址、中断线/引脚
- Type 1 Header：Bridge，含 2 个 BAR、Primary/Secondary/Subordinate Bus Number
- 通过 CONFIG_ADDRESS（0xCF8）+ CONFIG_DATA（0xCFC）访问

### PCI-X 总线

- PCI 到 PCIe 的过渡总线
- 新增 Split 事务、533MHz 最高频率、ADB 突发传输
- 引入 Relaxed Ordering 概念
- 不要求 DMA 缓存一致性（与 PCIe 一致）

### 中断机制

- 四根中断请求信号 INTA#~D#（PCI 桥通过 Device Number mod 4 映射）
- INTx 是异步信号 — DMA 数据可能尚未到达内存时中断已触发
- 两种同步方法：设备读刷新（read flush）；驱动先读中断状态寄存器（标准做法）
- MSI（Message Signaled Interrupt）作为替代方案在 PCI 中首次提出

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express]]
- [[pci-bridge]]
- [[root-complex]]

## Counter-Arguments and Gaps

...
