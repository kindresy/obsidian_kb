---
date: 2026-06-10
source-type: note
title: "DWC MIPI I3C Databook 概述"
tags: ["i3c", "bus-protocol"]
compiled: false
---

Sir，这本 **DWC MIPI I3C Controller and Target Databook** 和 MIPI spec 的关系可以这样理解：

```text
MIPI I3C spec：定义协议本身
DWC databook：定义 Synopsys 这个 I3C IP 如何把协议落到寄存器、FIFO、命令队列、响应队列、DAT/DCT、DMA handshake、Target/Controller 状态机上
```

所以我们接下来不应再只学“协议波形”，而要转成 **驱动/IP 视角**：软件怎么配置这个 IP，让它生成 DAA、CCC、SDR、HDR-DDR、HDR-BT、IBI、Hot-Join 等事务。

这个 databook 也特别提醒：不是所有提到的 feature 都在所有配置里可用，最终要看 coreConsultant 生成时的配置参数。比如 HCI DMA、HDR-TSP/TSL、HDR mode CCC transmission、Multi-Lane 等都有不支持或限制项。

---

# 1. 我建议先抽这 9 块来谈

## 第一块：IP 架构总览

对应章节：

```text
2.1 DWC_mipi_i3c_v111_basic Architecture
2.1.1 Block Diagram
2.1.2 Bus Interfaces
2.1.7 SPRAM and Buffer Depth
```

这部分回答一个问题：

```text
这个 I3C IP 内部到底长什么样？
```

Databook 的架构图里有这些核心模块：

```text
APB Register Bank
Command FIFO
Response Queue
Tx FIFO / Rx FIFO
IBI Status FIFO / IBI Data FIFO
Device Address Table, DAT
Device Characteristics Table, DCT
Controller Transaction Module
Target Transaction Module
Bus Monitor
Packer / Unpacker
DMA Handshake Interface
I3C SCL/SDA Drivers
```

从驱动角度，可以先建立这个模型：

```text
软件写 APB 寄存器 / Command FIFO
        ↓
IP 从 DAT 找 Target 地址
        ↓
IP 从 Tx FIFO 取数据，或把数据放 Rx FIFO
        ↓
Transaction Module 生成 I3C 总线时序
        ↓
完成后写 Response Queue / IBI Queue / Interrupt
```

这块非常重要，因为后面所有初始化、SDR、HDR 都是围绕 **Command Queue + DAT + Tx/Rx FIFO + Response Queue** 展开的。

---

## 第二块：Device Role 和 Active Controller 初始化

对应章节：

```text
1.1 General Product Description
2.1.6 Operation Modes
3.1 Overview of Controller Role
```

这个 IP 是 Dual Role，可以配置成：

```text
I3C Controller
I3C Secondary Controller
I3C Target
I2C Target
Target-Lite / Autonomous Target
```

Databook 里说，Active Controller 在 power-on 后负责动态地址分配；Secondary Controller 上电后先作为 Target，必须通过 ownership handoff 成为 Active Controller 后，才能主动发起 transfer。

对驱动而言，最关键的是这个点：

```text
Active Controller 自己的 Dynamic Address 要由 host application 自分配。
```

也就是要配置类似：

```text
DEVICE_ADDR[DYNAMIC_ADDR]
DEVICE_ADDR[DYNAMIC_ADDR_VALID] = 1
```

这和 MIPI spec 里的 **Primary Controller self dynamic address assignment** 对应。否则这个 IP 即使是 Controller 配置，也不一定真正拥有总线控制权。

---

## 第三块：Bus Configuration，Pure / Mixed Fast / Mixed Slow

对应章节：

```text
2.1.5 Bus Configuration
3.8.5 SCL Generation and Timings Based on Bus Configuration
3.8.6 Bus Free and Available Timing Register
```

Databook 明确支持三类总线：

```text
Pure Bus
Mixed Fast Bus
Mixed Slow/Limited Bus
```

这和我们前面学的 spec 完全对应。重点是：

```text
Pure Bus：只有 I3C devices
Mixed Fast：I3C + Legacy I2C，且 I2C target 有 50ns spike filter
Mixed Slow/Limited：I3C + Legacy I2C，但 I2C target 没有可靠 50ns spike filter
```

这会直接影响 SCL speed、HDR 是否可用、I2C 设备是否能被“隐藏”。Databook 还提到，如果没有 spike filter 或者未知，就算访问 I3C device，也可能只能限制到 FM/FM+。

这一块我们后面可以重点谈：

```text
如何根据板级拓扑配置 I3C_OD_HCNT / I3C_OD_LCNT / PP timing / BUS_FREE_TIME / BUS_AVAILABLE_TIME
```

---

## 第四块：DAA 动态地址分配

对应章节：

```text
3.2 Dynamic Address Assignment
3.2.2 SETDASA / ENTDAA / SETAASA / SETNEWDA
3.6.4 Device Address Table, DAT
3.6.5 Device Characteristics Table, DCT
```

这是 Controller 初始化的主线。Databook 里讲得比 spec 更“驱动化”：

```text
软件先初始化 Controller
软件预填 DAT
软件写 Address Assignment Command
IP 自动生成 SETDASA / ENTDAA / SETAASA 总线事务
IP 把结果写 Response Queue
ENTDAA 捕获到的 PID/BCR/DCR 写入 DCT
```

例如 ENTDAA 时，DAT 里要预放准备分配出去的 Dynamic Address 和 parity；DCT 会保存 winning device 的：

```text
PID[47:0]
BCR
DCR
Assigned Dynamic Address
```

Databook 还建议 DAA 成功后，可以再发 GETBCR / GETDCR / GETPID 到对应 Dynamic Address，和 DCT 捕获值比对，确认响应的是同一个设备。这个建议很适合做硅后 bring-up checklist。

---

## 第五块：Command / Response / DAT / DCT 数据结构

对应章节：

```text
3.6 Controller Data Structures in Non-HCI Mode
3.6.1 Command Data Structure
3.6.2 Response Data Structure
3.6.4 Device Address Table
3.6.5 Device Characteristics Table
3.7 Controller Data Structures in HCI Mode
```

这是驱动开发最核心的部分。Databook 把 Controller 发起事务抽象成几类 command structure：

```text
Transfer Command
Transfer Argument
Short Data Argument
Address Assignment Command
Unified Command Structure
```

你可以这样记：

```text
普通 SDR / CCC / HDR-DDR：
  Transfer Command + Transfer Argument 或 Short Data Argument

地址分配：
  Address Assignment Command

HDR-BT：
  Unified Command Structure
```

几个字段非常关键：

|字段|驱动意义|
|---|---|
|`CMD_ATTR`|当前写入 command port 的结构类型|
|`TID`|transaction ID，response 会带回来|
|`CMD`|CCC code 或 HDR command|
|`CP`|command present，是否使用 CMD 字段|
|`DEV_INDX`|指向 DAT 中的 Target|
|`SPEED`|SDR0~SDR4、HDR-DDR、HDR-BT 等|
|`RnW`|读/写方向|
|`TOC`|结束后 STOP / HDR EXIT，还是继续 RESTART / HDR RESTART|
|`ROC`|是否生成 response|
|`PEC`|SDR PEC enable|

这部分值得单独攻克，因为它把“协议事务”变成了“软件如何下发命令”。

---

## 第六块：SDR Transfer 落地

对应章节：

```text
3.8.1 Bus Communication Protocols in Non-HCI Modes
3.8.1.3 Broadcast CCC Write Transfers
3.8.1.4 Directed Write and Read Transfers
3.8.1.6 I3C Private Write or Read Transfers
3.8.1.7 I2C Private Write or Read Transfers
3.8.1.9 TOC and ROC Bit Settings for SDR Transfers
3.8.3 Clock Stalling Conditions
3.8.4 Data Packing and Unpacking
```

这块就是我们前面聊的 SDR 协议在 Synopsys IP 中如何实现。

几个特别值得谈的点：

### 1. CCC transfer

Broadcast CCC 和 Directed CCC 都通过 Transfer Command 发起。Directed CCC 的 `DEV_INDX` 指向 DAT 中某个 Target。若要对多个 Target 发同一个 Directed CCC，需要 pipeline 多个 Transfer Command，并把前面几个 command 的 `TOC=0`，最后一个再结束。

### 2. Private SDR transfer

I3C private read/write 的 `CP=0`，表示不使用 `CMD` 字段；Target 由 `DEV_INDX` 指向 DAT；`SPEED=0~4` 表示 SDR0~SDR4。

### 3. `TOC` 和 `ROC`

`TOC=1`：本次传输结束后生成 STOP。  
`TOC=0`：继续后续 transfer，SDR 下通常生成 RESTART。  
`ROC=1`：完成后生成 response，读命令建议打开，因为如果 Target 提前结束读，response 的 data length 能告诉软件实际收到多少。

### 4. SDR clock stalling

Databook 说 SDR 下如果 TX FIFO 空、RX FIFO 满，Controller 可以拉低 SCL 延长时钟；但 HDR 模式不支持 stalling。所以 SDR 能靠 stall 缓冲系统延迟，HDR 则必须靠 FIFO threshold、DMA、系统带宽保证。

---

## 第七块：IBI / Hot-Join / Controller Request

对应章节：

```text
3.3 In-Band Interrupt Detection and Handling
3.3.1 Hot-Join Request
3.3.2 Controller Request
3.3.3 Target Interrupt Request, SIR
3.3.4 IBI Queue Data Structure
3.3.5 IBI Notification Control
4.2.8 Hot-Join Request Generation
4.2.9 Target Interrupt Request Generation
4.2.10 Controller Request Generation
```

这部分对应我们前面聊的：

```text
Target 拉低 SDA
Controller 提供 SCL
Target 发送 IBI ID
Controller ACK/NACK
```

Databook 把 IBI 分成三类：

```text
HJ  = Hot-Join Request
SIR = Target Interrupt Request
MR  = Controller Ownership Request
```

Controller 侧有很多 response control：

```text
HJ response control
SIR response control
MR response control
Reject notify control
IBI payload control
SIR data chunk size
```

非常实用的点是：如果 SIR 带 payload，Controller 会把 MDB 和 payload 分 chunk 放入 IBI queue，每个 chunk 前面有 IBI status，最后一个 chunk 用 `LAST_STATUS` 表示。软件必须按 status 里的 data length 去读，不要盲读固定长度。

另一个很硅后相关的点：如果 IBI Data Queue 满，Controller 可能在 ACK space 或 IBI data reception 过程中进入 clock stall，直到队列有空间。Databook 建议尽快 drain IBI queue，并把 IBI status threshold 设得很低，以便第一个 status 来了就通知软件。

---

## 第八块：HDR-DDR

对应章节：

```text
3.8.1.10 High Data Rate Transfers
3.8.1.10 HDR-DDR Transfer Required Programming Values
3.8.1.13 TOC and ROC Bit Settings for HDR Transfers
```

Databook 对 HDR-DDR 的落地方式是：

```text
软件下发 Transfer Command
SPEED = HDR-DDR
CP = 1
CMD 字段携带 HDR command code
DEV_INDX 指向 Target
RnW 决定读写方向
DATA_LENGTH 指定传输长度
IP 自动发 ENTHDR0 进入 HDR-DDR
IP 自动生成 preamble / parity / CRC 等协议字段
```

它还强调：HDR transfer 不支持 Short Data Argument，因为进入 HDR 的开销较大，用 short data 没意义；需要使用 Transfer Argument。

HDR-DDR 的关键驱动风险：

```text
TX underflow
RX overflow
Target NACK HDR command
CRC / parity / frame mismatch
HDR EXIT / HDR RESTART 决策
```

如果发生这些错误，DWC controller 会通过 response 的 `ERR_STS` 报告，并进入 halt，软件需要处理后写 `RESUME` 恢复。

---

## 第九块：HDR-BT

对应章节：

```text
3.8.1.11 High Data Rate – Bulk Transfers, HDR-BT
3.6.1 Unified Command Structure
4.2.7 Handling Private HDR-BT Transfers
4.3.7.3 Handling HDR-DDR and HDR-BT Transfers
```

HDR-BT 是这本书里很值得谈的一块，因为它和 DMA/FIFO/大块数据传输最相关。

Databook 的落地模型是：

```text
HDR-BT 使用 Unified Command Structure
数据按 32 bytes block 传输
传输结束有 CRC block
Controller 侧用 TX_START_THLD / RX_START_THLD 避免初始 latency
Read 方向支持 delay byte mechanism
```

Controller 写 Target 时：

```text
Controller 等 Tx FIFO 至少有 max(32B, TX_START_THLD) 或满足 data length
发 HDR-BT header
Target accept 后，Controller 一块一块发 32B data block
最后发 CRC block
```

Controller 读 Target 时：

```text
Controller 先确认 Rx FIFO 有足够空间
发 HDR-BT header
Target 以 32B block 返回数据
如果 Target 数据暂时没准备好，可使用 Delay Byte
最后 CRC block
```

Databook 也明确有配置限制：HDR mode CCC transmission 不支持；Target 侧当前支持 private HDR-BT read/write，不支持 HDR-BT CCC transfer。

---

# 2. 我建议我们的学习顺序

不要按页码平铺读。按“驱动 bring-up 主线”学效率最高：

```text
1. IP 架构
   APB / FIFO / Command Queue / Response Queue / DAT / DCT / IBI Queue

2. Controller 初始化
   DEVICE_CTRL / DEVICE_ADDR / timing / bus configuration / interrupt enable

3. DAA
   SETDASA / ENTDAA / SETAASA / SETNEWDA
   DAT 怎么填，DCT 怎么读

4. SDR
   Broadcast CCC
   Directed CCC
   Private Read/Write
   I2C Private Transfer
   TOC / ROC / PEC / IBA_INCLUDE

5. IBI/HJ/MR
   SIR/HJ/MR response control
   IBI queue data structure
   MDB/payload/chunk
   Reject notify

6. HDR-DDR
   ENTHDR0
   SPEED=HDR-DDR
   Transfer Argument
   threshold / error / halt / resume

7. HDR-BT
   Unified Command Structure
   32B Data Block
   Delay Byte
   CRC
   threshold
   Target private HDR-BT

8. Target / Autonomous Target
   Address assignment handling
   CCC handling
   Private SDR read/write
   HDR-BT private transfer
   Hot-Join/SIR/MR generation
```

---

# 3. 先抛出几个最值得讨论的问题

我建议我们下一轮先从这几个问题里选一个：

```text
问题 1：
DWC Controller 初始化时，软件到底要按什么顺序配置？
DEVICE_ADDR、DEVICE_CTRL、timing、DAT、interrupt、DAA 的顺序是什么？

问题 2：
ENTDAA 在 DWC IP 中怎么落地？
DAT 预填什么？DCT 捕获什么？Response Queue 怎么判断成功？

问题 3：
普通 SDR private read/write 怎么下 command？
Transfer Command / Transfer Argument / Short Data Argument 分别怎么用？

问题 4：
TOC / ROC 是什么？
为什么 TOC=0 时会影响 RESTART、HDR-RESTART、clock stall 和 command pipeline？

问题 5：
IBI 到底怎么进 IBI Queue？
SIR with payload、Hot-Join、MR 在 queue 里的结构有什么区别？

问题 6：
HDR-DDR 和 HDR-BT 在这个 IP 里分别怎么编程？
为什么 HDR-BT 要用 Unified Command Structure？
```

最适合作为下一步的是 **问题 1：DWC Controller 初始化顺序**。因为它能把 spec 里的“总线原则/DAA/CCC/SDR”真正落到驱动初始化流程上。