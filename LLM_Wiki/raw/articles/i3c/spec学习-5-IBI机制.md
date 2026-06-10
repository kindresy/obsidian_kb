---
date: 2026-06-10
source-type: note
title: "I3C IBI 在 DWC IP 中的实现"
tags: ["i3c", "bus-protocol"]
compiled: false
---

Sir，这个 topic 很关键，因为它把 **I3C 协议里的 IBI** 落到了 **DWC IP 的 IBI Queue / IBI Status / IBI Data 软件模型**上。

先给结论：

```text
IBI 进入 DWC IBI Queue 的本质流程：

Target 拉起 IBI 请求
  ↓
DWC Controller 检测到 SDA low / 地址仲裁丢失 / STOP 后 Target 请求
  ↓
DWC 提供 SCL，接收 IBI ID
  ↓
根据 IBI ID 判断是 HJ / SIR / MR
  ↓
根据配置 ACK 或 NACK
  ↓
写 IBI Status 到 IBI Queue
  ↓
如果是 SIR with payload 且被 ACK，则继续接收 MDB/payload，并把数据跟 status 关联放入 IBI Queue
  ↓
触发 IBI threshold interrupt，软件读取 IBI status/data
```

Databook 的 3.3 节说明，DWC Controller 会检测 Target 发起的 IBI，并支持三类 IBI：**Hot-Join Request，Target Interrupt Request/SIR，Controller Ownership Request/MR**；3.6.3 节则定义了 IBI Status/Data 的软件可见结构。

---

# 1. IBI 是怎么被 DWC Controller 捕获的？

DWC Controller 会在这些场景下检测 IBI：

```text
1. POR 后检测到 SDA input 为 low
2. Controller 自己发 START 后，在地址阶段发生 arbitration loss
3. STOP 后，Target 在 Bus Available 时主动拉低 SDA
```

一旦检测到 IBI 请求，DWC 会开始提供 SCL clock，接收 Target 发出的 **IBI ID**。Databook 里说 IBI ID 是 **START 后收到的那个 byte**，它包含：

```text
IBI_ID = {7-bit address, RnW}
```

也就是：

```text
IBI_ID[7:1] = IBI address
IBI_ID[0]   = RnW
```

然后 DWC 根据这个 byte 判断是哪一种 IBI。

---

# 2. 三种 IBI 的 IBI_ID 区别

## 2.1 SIR：Target Interrupt Request

SIR 是普通 Target 发中断。

```text
SIR:
  IBI_ID = {Target Dynamic Address, RnW=1}
```

例如 Target 动态地址是 `7'h12`，那么 IBI ID byte 是：

```text
IBI_ID = {7'h12, 1'b1} = 8'h25
```

DWC 收到后，会去 DAT 里查有没有匹配的 Dynamic Address。如果匹配，再根据 SIR response control 决定 ACK/NACK。

---

## 2.2 Hot-Join：HJ

Hot-Join 是还没有 Dynamic Address 的 Target 请求加入总线。

```text
HJ:
  IBI address = 7'h02
  RnW = 0
```

所以完整 IBI ID byte 是：

```text
IBI_ID = {7'h02, 1'b0} = 8'h04
```

Databook 明确说，如果收到的 IBI ID 匹配 Hot-Join ID `7'b0000010, RnW=0`，DWC 就按 Hot-Join response control 处理。ACK 后，软件应该随后发 ENTDAA 给这个新加入设备分配 Dynamic Address。

---

## 2.3 MR：Controller Ownership Request

MR 是 Secondary Controller 请求总线控制权，也就是 Controller Role Request。

```text
MR:
  IBI_ID = {Secondary Controller Dynamic Address, RnW=0}
```

注意它和 SIR 的区别只在 RnW：

```text
SIR: {DA, 1}
MR : {DA, 0}
```

所以同一个 Dynamic Address：

```text
DA = 7'h12

SIR IBI_ID = 8'h25
MR  IBI_ID = 8'h24
```

DWC 收到 MR 后，也会查 DAT。如果地址匹配，再根据 MR response control 决定 ACK/NACK。

---

# 3. IBI Queue 里最核心的是 IBI Status

不管是哪种 IBI，DWC 都至少会产生一个 **IBI Status**。

如果 IP 配置没有启用 IBI with payload：

```text
IC_HAS_IBI_DATA == 0
```

则 IBI Queue 里只有一个简单 status DWORD：

```text
bit[31:28]  IBI_STS
bit[27:16]  Reserved
bit[15:8]   IBI_ID
bit[7:0]    DATA_LENGTH
```

其中：

|字段|含义|
|---|---|
|`IBI_STS`|`0` 表示 DWC ACK 了该 IBI；`1` 表示 DWC NACK 了该 IBI|
|`IBI_ID`|START 后收到的 IBI byte，也就是 `{address, RnW}`|
|`DATA_LENGTH`|随 IBI 收到的数据长度；无 payload 时通常为 0|

Databook 把这个叫 **IBI Without Payload Status Structure**。

---

# 4. 启用 IBI with payload 后，Status 结构变复杂

如果 IP 配置启用了：

```text
IC_HAS_IBI_DATA == 1
```

DWC 使用 **IBI With Payload Status Structure**：

```text
bit[31]     IBI_STS
bit[30]     ERROR
bit[29:26]  Reserved
bit[25]     TS
bit[24]     LAST_STATUS
bit[23:16]  Reserved
bit[15:8]   IBI_ID
bit[7:0]    DATA_LENGTH
```

字段含义：

|字段|含义|
|---|---|
|`IBI_STS`|`0` ACK；`1` NACK|
|`ERROR`|Auto-command 阶段遇到错误，例如 CRC/Parity、Target Address NACK、`0x7E` NACK、IBI buffer overflow|
|`TS`|是否带 timestamp|
|`LAST_STATUS`|当前 status 是否是该 IBI 的最后一个 status|
|`IBI_ID`|`{address, RnW}`|
|`DATA_LENGTH`|本 chunk 对应的有效 IBI data byte 数|

这套结构主要服务于 **SIR with payload**，因为 SIR payload 可能比较长，需要被切成多个 chunk。

---

# 5. SIR with payload 在 Queue 里是什么样？

SIR with payload 的总线形态是：

```text
S
Target_DA + R
ACK by Controller
MDB
T
Payload byte 1
T
Payload byte 2
T
...
P / Sr
```

其中第一个 payload byte 是 **MDB，Mandatory Data Byte**。

DWC 的 queue 组织方式是：

```text
IBI Status
IBI Data DWORD(s)
IBI Status
IBI Data DWORD(s)
...
```

更准确地说：

```text
如果 payload <= IBI_DATA_THLD:
  IBI Status, LAST_STATUS=1, DATA_LENGTH=N
  IBI Data containing MDB + payload bytes

如果 payload > IBI_DATA_THLD:
  IBI Status, LAST_STATUS=0, DATA_LENGTH=chunk0_len
  IBI Data chunk0

  IBI Status, LAST_STATUS=0, DATA_LENGTH=chunk1_len
  IBI Data chunk1

  ...

  IBI Status, LAST_STATUS=1, DATA_LENGTH=last_chunk_len
  IBI Data last chunk
```

Databook 说，如果 SIR payload size 超过 programmed SIR Data Chunk Size，Controller 会把 incoming data bytes 切成多个 chunks，并为每个 chunk 生成一个 IBI status，最后一个 status 用 `LAST_STATUS=1` 标识。

---

# 6. SIR payload 的 byte packing

DWC 把收到的数据按总线顺序放入 IBI Queue：

```text
第 1 个收到的 byte → DWORD[7:0]
第 2 个收到的 byte → DWORD[15:8]
第 3 个收到的 byte → DWORD[23:16]
第 4 个收到的 byte → DWORD[31:24]
```

所以 SIR with payload 的第一个 data DWORD 通常是：

```text
DWORD[7:0]   = MDB
DWORD[15:8]  = IDB1
DWORD[23:16] = IDB2
DWORD[31:24] = IDB3
```

Databook 的 Figure 3-12 也画了这个关系：`MDB` 在第一个 data word 的 LSB，后续 IBI Data Byte 依次往高 byte 填。

所以软件读取时不能按大端直觉解析，要按：

```c
byte0 = word & 0xff;        // MDB
byte1 = (word >> 8) & 0xff;
byte2 = (word >> 16) & 0xff;
byte3 = (word >> 24) & 0xff;
```

---

# 7. SIR with payload 的使能条件

不是所有 SIR 都应该接 payload。

DWC 里要满足两个条件：

```text
1. Target 的 BCR[2] = 1
   表示 Target 支持 Mandatory Data Byte / IBI payload

2. Controller 侧配置允许接收 payload
   例如 DAT[IBI_PAYLOAD] = 1
```

Databook 特别提醒：只有当 Target 支持 mandatory byte，也就是 `BCR[2]=1` 时，application 才应该设置 IBI Payload Control；如果 `BCR[2]=0`，不要设置。

否则会出现：

```text
Controller ACK 了 SIR
Controller 继续给 clock 等 payload
Target 实际并不会发 MDB
协议状态错位
```

---

# 8. Hot-Join 在 Queue 里是什么样？

Hot-Join 没有 MDB，也没有 payload。

总线形态：

```text
S
7'h02 + W
ACK/NACK by Controller
P / Sr
```

DWC Queue 里通常只有一个 status：

```text
IBI Status:
  IBI_ID      = {7'h02, 1'b0} = 8'h04
  DATA_LENGTH = 0
  IBI_STS     = ACK 或 NACK
  LAST_STATUS = 1，如果使用 with payload status structure
```

如果 HJ response control 设置为 ACK：

```text
IBI_STS = ACK
```

软件看到这个 HJ status 后，下一步应该发：

```text
ENTDAA
```

给新加入的 Target 分配 Dynamic Address。

如果 HJ response control 设置为 NACK，DWC 会 NACK Hot-Join，并且可以跟一个 broadcast DISEC CCC，设置 `DISHJ`，禁止当前未分配设备继续 Hot-Join。是否把 rejected HJ 通知给软件，取决于 Hot-Join Reject Notify Control。

---

# 9. MR 在 Queue 里是什么样？

MR 也没有 MDB/payload。

总线形态：

```text
S
Secondary_Controller_DA + W
ACK/NACK by Controller
P / Sr
```

Queue 里也是只有 status：

```text
IBI Status:
  IBI_ID      = {Secondary_Controller_DA, 1'b0}
  DATA_LENGTH = 0
  IBI_STS     = ACK 或 NACK
  LAST_STATUS = 1，如果使用 with payload status structure
```

如果 MR 被 ACK：

```text
软件后续要执行 Controller ownership handoff 流程
```

如果 MR 被 NACK：

```text
DWC 可以发送 directed DISEC CCC，设置 DISMR bit，禁止该 Secondary Controller 继续发 MR
```

Databook 还建议，在 Controller-only / HCI Controller 配置中，MR response control 通常应设置为 reject，避免异常 Target 伪装成 Controller-capable device 干扰总线。

---

# 10. 三者在 IBI Queue 里的结构对比

|类型|IBI_ID 形式|是否查 DAT|是否有 payload|Queue 内容|
|---|---|---|---|---|
|**SIR without payload**|`{Target_DA, 1}`|是|否|1 个 IBI Status，`DATA_LENGTH=0`|
|**SIR with payload**|`{Target_DA, 1}`|是|是，MDB + payload|IBI Status + IBI Data；长 payload 会被切成多个 status/data chunk|
|**Hot-Join**|`{7'h02, 0}`|否，设备还没有 DA|否|1 个 IBI Status，`DATA_LENGTH=0`|
|**MR**|`{Secondary_Controller_DA, 0}`|是|否|1 个 IBI Status，`DATA_LENGTH=0`|

一句话：

```text
SIR with payload = status + data
HJ / MR          = status only
SIR without payload = status only
```

---

# 11. 软件读取 IBI Queue 的推荐 flow

## 11.1 收到 IBI interrupt

```text
INTR_STATUS[IBI_THLD_STS] 或类似 IBI threshold interrupt 置位
```

软件进入 ISR / threaded handler。

## 11.2 读取 IBI Status

```c
status = readl(IBI_QUEUE_PORT);
```

解析：

```c
ibi_id = FIELD_GET(IBI_ID, status);
ack    = FIELD_GET(IBI_STS, status) == 0;
len    = FIELD_GET(DATA_LENGTH, status);
last   = FIELD_GET(LAST_STATUS, status);
```

## 11.3 根据 IBI_ID 判断类型

```c
addr = ibi_id >> 1;
rnw  = ibi_id & 1;

if (addr == 0x02 && rnw == 0)
    type = HOT_JOIN;
else if (rnw == 1)
    type = SIR;
else
    type = MR;
```

## 11.4 如果是 SIR with payload，继续读 data

```c
while (len > 0) {
    word = readl(IBI_DATA_PORT);

    extract bytes from word LSB first;
    len -= min(len, 4);
}
```

如果 `LAST_STATUS=0`，说明后面还有下一段 chunk：

```text
继续读下一个 IBI Status
继续读对应 IBI Data
直到 LAST_STATUS=1
```

---

# 12. Queue 满时 DWC 怎么处理？

这个对硅后调试很有价值。

Databook 说：

## 12.1 非 SIR 请求时，IBI Status Queue 满

例如 HJ/MR：

```text
DWC 仍然提供 SCL 接收 IBI ID
然后 NACK
不发 DISEC
Target 下次 Bus Available 或 START 时可以重试
```

## 12.2 SIR 时，IBI Data Queue 满

如果 SIR data queue 满，DWC 会更激进：

```text
在 IBI ID ACK space 进入 clock stall
或者在 SIR data reception 中拉低 SCL stall
直到 IBI Data Queue 有空间
```

所以如果你在波形上看到 IBI 后 SCL 被 DWC 拉低很久，不一定是 Target 问题，可能是：

```text
IBI Data Queue 没被软件及时 drain
```

Databook 也建议把 IBI Status threshold 设置为 0，让第一个 IBI status 到来就通知软件，尽快清空 queue。

---

# 13. 实战 debug 判断

看到一个 IBI status 后，可以这么判断：

```text
IBI_ID[7:1] == 7'h02 && IBI_ID[0] == 0
  → Hot-Join
  → DATA_LENGTH 应为 0
  → ACK 后软件应 ENTDAA

IBI_ID[0] == 1
  → SIR
  → 如果 DATA_LENGTH = 0，是 SIR without payload 或 payload 未开启
  → 如果 DATA_LENGTH > 0，第一个 data byte 是 MDB

IBI_ID[0] == 0 && IBI_ID[7:1] != 7'h02
  → MR
  → DATA_LENGTH 应为 0
  → ACK 后进入 Controller handoff 流程
```

最核心的记忆：

```text
SIR 是 Target_DA + R，所以可能带 MDB/payload；
HJ 是 7'h02 + W，没有 payload；
MR 是 Secondary_Controller_DA + W，没有 payload。

DWC Queue 中：
  HJ / MR / SIR without payload 只有 status；
  SIR with payload 是 status + data，长 payload 会切 chunk，每个 chunk 前都有 status。
```