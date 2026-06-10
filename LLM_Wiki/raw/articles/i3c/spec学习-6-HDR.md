---
date: 2026-06-10
source-type: note
title: "I3C HDR 模式详解"
tags: ["i3c", "bus-protocol"]
compiled: false
---

Sir，这一节要分清两条路径：

```text
HDR-DDR：
  仍然走传统 Transfer Command + Transfer Argument

HDR-BT：
  必须走 Unified Command Structure，4 DWORD
```

原因也很直接：**HDR-DDR 本质还是 command + data word stream；HDR-BT 是 header block + 32B data blocks + CRC block 的 bulk transport，字段更多，老的 Transfer Command 容不下。**

---

# 1. HDR-DDR 怎么编程？

DWC 的 HDR-DDR 通过 **COMMAND_QUEUE_PORT** 下发，使用：

```text
Transfer Argument
  ↓
Transfer Command
```

不能用 Short Data Argument。Databook 明确说 HDR transfers 只支持 **Transfer Argument command**，不支持 **Short Data Argument command**，因为 Short Data Argument 最多只能表达很短的数据，对 HDR 的 ENTHDR、HDR command、entry/exit pattern 开销来说不划算。

## 1.1 HDR-DDR Transfer Argument

先写 Transfer Argument：

```text
CMD_ATTR    = 1
DATA_LENGTH = 本次 HDR-DDR 传输长度
```

对 write 来说，`DATA_LENGTH` 表示要从 Tx FIFO 发送多少数据。  
对 read 来说，`DATA_LENGTH` 表示期望接收多少数据到 Rx FIFO。

---

## 1.2 HDR-DDR Transfer Command

再写 Transfer Command：

```text
CMD_ATTR = 0
CP       = 1
CMD      = HDR-DDR command code
DEV_INDX = Target 在 DAT 中的 index
SPEED    = 6，表示 HDR-DDR
SDAP     = 0，表示使用 Transfer Argument，不用 Short Data Argument
RnW      = 0/1
ROC      = 0/1
TOC      = 0/1
```

关键字段解释：

|字段|含义|
|---|---|
|`CP=1`|HDR-DDR 需要使用 `CMD` 字段形成 HDR command|
|`CMD[14:7]`|HDR-DDR command code，`0x00~0x1F` 为 I3C reserved write，`0x20~0x7F` 为 vendor write，`0x80~0x9F` 为 I3C reserved read，`0xA0~0xFF` 为 vendor read|
|`DEV_INDX`|指向 DAT，DWC 从 DAT 取 Target Dynamic Address|
|`SPEED=6`|使用 HDR-DDR|
|`SDAP=0`|使用 Transfer Argument|
|`RnW=0`|write|
|`RnW=1`|read|
|`TOC=1`|HDR transfer 结束后退出/终止|
|`TOC=0`|后续接 HDR-RESTART / pipeline transfer|

Databook 说，DWC Controller 会自动发 `ENTHDR0` CCC 进入 HDR-DDR，并用 Transfer Command 的 `CMD` 字段和 Target 地址形成 HDR-DDR command；preamble、parity 等协议字段由 Controller 自动生成。

---

## 1.3 HDR-DDR write flow

```text
1. 写 Transfer Argument
   DATA_LENGTH = N

2. 把 N bytes 数据写入 Tx FIFO
   或配置 DMA 把数据搬到 Tx FIFO

3. 写 Transfer Command
   CP       = 1
   CMD      = HDR-DDR write command
   DEV_INDX = target index
   SPEED    = HDR-DDR
   RnW      = 0
   SDAP     = 0
   TOC      = 1 or 0
   ROC      = 1，建议调试阶段打开

4. 等 Response Queue
5. 检查 ERR_STS
```

总线侧大致是：

```text
SDR:
  7'h7E + W
  ENTHDR0

HDR-DDR:
  HDR-DDR Command Word
  Write Data Word(s)
  CRC / Exit or Restart
```

---

## 1.4 HDR-DDR read flow

```text
1. 写 Transfer Argument
   DATA_LENGTH = N

2. 写 Transfer Command
   CP       = 1
   CMD      = HDR-DDR read command
   DEV_INDX = target index
   SPEED    = HDR-DDR
   RnW      = 1
   SDAP     = 0
   TOC      = 1 or 0
   ROC      = 1

3. 等 Response Queue
4. 检查 ERR_STS
5. 根据 Response 的 DATA_LENGTH / 实际接收长度读 Rx FIFO
```

DWC 在 HDR-DDR read 中会校验 parity、CRC、frame mismatch 等错误，并把错误反映到 Response 的 `ERR_STS` 字段；出错后 Controller 会 halt，软件需要通过 `DEVICE_CTRL[RESUME]` 恢复。

---

## 1.5 HDR-DDR threshold 注意点

HDR 模式下不能像 SDR 那样随便 clock stall，所以 DWC 会用：

```text
TX_START_THLD
RX_START_THLD
```

来避免起始阶段 FIFO 不够。

含义是：

```text
HDR-DDR write：
  Tx FIFO 里要有足够数据，Controller 才启动传输

HDR-DDR read：
  Rx FIFO 要有足够空位，Controller 才启动传输
```

如果是必须以 RESTART 发起的 HDR-DDR transfer，但 threshold 条件不满足，DWC 会先生成 HDR EXIT pattern + STOP，等 FIFO 条件满足后再用 START 发起下一笔。

---

# 2. HDR-BT 怎么编程？

HDR-BT 不走普通 Transfer Command。它走：

```text
Unified Command Structure
```

也就是 **CMD_ATTR = 4** 的 4 DWORD 命令结构。

Databook 说 Unified Command Structure 是 Transfer Command 的升级版本，设计上可以容纳新 I3C spec 的更多特性；但在当前 release 中，它被限制为只用于 **HDR-BT transfers**。

---

# 3. HDR-BT Unified Command Structure

HDR-BT 要连续写 4 个 DWORD 到 COMMAND_QUEUE_PORT。概念上是：

```text
DWORD0：Unified Transfer Command
DWORD1：DATA_LENGTH + lane 配置
DWORD2：HDR-BT Header Block 里的 CMD0~CMD3
DWORD3：CONTROL byte + IDB4/reserved
```

## 3.1 DWORD0

```text
CMD_ATTR   = 4
TID        = transaction id
I2C        = 0，I3C transfer
RnW        = 0/1
DEV_INDEX  = Target 在 DAT 中的 index
SPD_MODE   = 0x8，HDR-BT
XFER_TYPE  = 0，private transfer
IDBC       = 0，HDR-BT 不支持 immediate data bytes
WROC       = 0/1，write 是否需要 response
TOC        = 0/1
```

重点：

```text
SPD_MODE = 0x8
XFER_TYPE = 0
IDBC = 0
```

Databook 明确说当前版本 HDR-BT CCC 不支持，因此 `XFER_TYPE=0` 表示 private transfer。

---

## 3.2 DWORD1

```text
DATA_LENGTH = 1 ~ 2^23 - 1
MLANE_MODE  = 0
MLANE_CODE  = 0
```

注意：

```text
HDR-BT private transfer 不允许 DATA_LENGTH = 0
```

虽然 Unified Command Structure 里有 multi-lane 字段，但这份 DWC databook 的 unsupported features 里说明 Multi-Lane 不支持，所以这里按 single lane 编程：

```text
MLANE_MODE = 0
MLANE_CODE = 0
```

---

## 3.3 DWORD2

DWORD2 放 HDR-BT Header Block 里的 command bytes：

```text
CMD0 = 0x00 ~ 0xFF
CMD1 = 0x00 ~ 0xFF
CMD2 = 0x00 ~ 0xFF
CMD3 = 0x00 ~ 0xFF
```

这四个 byte 是 HDR-BT Header Block 的一部分。它们的具体业务含义通常由你的 Target 私有协议定义，比如：

```text
CMD0 = opcode
CMD1 = register offset low
CMD2 = register offset high
CMD3 = flags / length / stream id
```

DWC 不解释这四个 byte 的业务语义，它只负责把它们放进 HDR-BT Header Block。

---

## 3.4 DWORD3

DWORD3 关键是 `CONTROL` byte：

```text
CONTROL[0]   = 与 I3C v1.2 支持相关，按目标协议/配置
CONTROL[1]   = CRC 选择，CRC16 或 CRC32
CONTROL[2]   = Delay byte mechanism enable
CONTROL[7:3] = reserved，写 0

IDB4         = 0
RSVD[31:16]  = 0
```

其中最常用的是：

```text
CONTROL[1]：选择 CRC16 / CRC32
CONTROL[2]：是否允许 Target 在 read 时使用 Delay Byte
```

Databook 说 DWC 会根据 `SPD_MODE=0x8` 自动生成 `ENTHDR3` CCC 进入 HDR-BT，并用 Unified Command Structure 字段和 Target address 形成 HDR-BT Header Block。

---

# 4. HDR-BT write flow

```text
1. 准备 Tx FIFO / DMA
   至少准备 max(32 bytes, TX_START_THLD)
   或者准备完整 DATA_LENGTH 对应的数据

2. 写 Unified Command DWORD0
   CMD_ATTR  = 4
   RnW       = 0
   DEV_INDEX = target
   SPD_MODE  = 0x8
   XFER_TYPE = 0
   IDBC      = 0
   WROC      = 1，建议调试阶段打开
   TOC       = 1 or 0

3. 写 DWORD1
   DATA_LENGTH = N
   MLANE_MODE  = 0
   MLANE_CODE  = 0

4. 写 DWORD2
   CMD0~CMD3

5. 写 DWORD3
   CONTROL
   IDB4 = 0
   Reserved = 0

6. DWC 自动发：
   ENTHDR3
   HDR-BT Header Block
   Transition byte
   Data Blocks
   CRC Block

7. 等 Response Queue
8. 检查 ERR_STS
```

HDR-BT write 的数据是按 **32 bytes Data Block** 发的。Databook 说，Target 接受每个 HDR-BT data block 后，Controller 从 Tx FIFO 取 32 bytes 发出去，直到传输完成或 receiver 提前终止；如果 receiver 在 Transition Control Byte 里终止某个 Data Block，Controller 会进入 CRC block 发送。

---

# 5. HDR-BT read flow

```text
1. 确保 Rx FIFO 有足够空间
   至少 max(32 bytes, RX_START_THLD)
   或 DATA_LENGTH 对应空间

2. 写 Unified Command DWORD0
   CMD_ATTR  = 4
   RnW       = 1
   DEV_INDEX = target
   SPD_MODE  = 0x8
   XFER_TYPE = 0
   IDBC      = 0
   TOC       = 1 or 0

3. 写 DWORD1
   DATA_LENGTH = N
   MLANE_MODE  = 0
   MLANE_CODE  = 0

4. 写 DWORD2
   CMD0~CMD3

5. 写 DWORD3
   CONTROL[1] = CRC type
   CONTROL[2] = 是否允许 Delay Byte

6. DWC 自动发：
   ENTHDR3
   HDR-BT Header Block

7. Target 返回：
   32B Data Block(s)
   CRC Block

8. DWC 校验 CRC / parity / frame
9. Response Queue 返回状态
10. 软件从 Rx FIFO 读取数据
```

HDR-BT read 中，Target 按 32B block 返回数据。DWC 默认支持 optional Delay Byte，并通过 Unified Command Structure 的 `CONTROL` byte 告诉 Target 是否允许使用 Delay Byte；最大 delay byte 数通过 `BT_DLY_BYTE_CNT.HDR_BT_DELAY_BYTE_CNT` 配置。

---

# 6. HDR-BT 的错误处理

HDR-BT 出错时，DWC 会把错误写到 Response Queue 的 `ERR_STS`，并进入 halt state。常见 halt 条件包括：

```text
Target address / Transition Byte NACK
Transfer underflow
Receiver overflow
软件主动 terminate
Transition Control byte parity error
CRC block control byte parity error
Framing error
```

Databook 还提到，如果 data block 或 CRC block 出现 frame/parity error，Controller 会通过对应 Response 的 `ERR_STS` 通知 application，并进入 Halt State；application resume 后，应在 150us 后发 HDR-EXIT pattern 让 Target 恢复。

---

# 7. 为什么 HDR-BT 要用 Unified Command Structure？

最本质原因：**HDR-BT 的命令信息量远大于 HDR-DDR，老的 Transfer Command + Transfer Argument 表达不下。**

## 7.1 老 Transfer Command 适合 HDR-DDR

HDR-DDR 需要表达的内容相对简单：

```text
Target 是谁      → DEV_INDX
读还是写        → RnW
HDR command 是什么 → CMD
速度            → SPEED=HDR-DDR
长度            → Transfer Argument.DATA_LENGTH
结束方式        → TOC
是否要 response → ROC
```

这些字段老结构已经够用。

---

## 7.2 HDR-BT 需要表达 Header Block

HDR-BT 不是简单 command + word stream，而是：

```text
Header Block
Data Block(s)
CRC Block
```

Header Block 里至少需要：

```text
Target address
CMD0
CMD1
CMD2
CMD3
CONTROL
Transition
```

其中 `CMD0~CMD3` 和 `CONTROL` 都需要软件明确给出。老的 Transfer Command 只有一个 `CMD` 字段，无法自然表达 HDR-BT Header Block 的 4 个 command byte 和 control byte。

---

## 7.3 HDR-BT 需要更大的 DATA_LENGTH

老的 Transfer Argument 里 `DATA_LENGTH` 是 16-bit 级别。HDR-BT 的 Unified Command Structure 里 `DATA_LENGTH` 是 23-bit：

```text
1 ~ 2^23 - 1
```

Bulk transfer 本来就是为了大块数据传输，所以需要更大的长度字段。

---

## 7.4 HDR-BT 需要 lane / transfer type / immediate byte control

Unified Command Structure 还包含：

```text
SPD_MODE
XFER_TYPE
IDBC
MLANE_MODE
MLANE_CODE
WROC
TOC
```

即使这个 DWC 当前只支持 single lane、HDR-BT private transfer，不支持 HDR-BT CCC / Multi-Lane，这套结构也给新协议字段留好了位置。

所以可以这样理解：

```text
Transfer Command：
  旧模型，适合 SDR / CCC / HDR-DDR 这类相对简单 transfer

Unified Command Structure：
  新模型，适合 HDR-BT 这类 header/block/CRC/bulk transfer
```

一句话：**HDR-DDR 是“高速 word transfer”，所以老命令结构够用；HDR-BT 是“块传输协议”，需要描述 Header Block、长数据长度、CRC/delay/lane/control 等额外字段，所以必须使用 4-DWORD Unified Command Structure。**