我们接着后面的话题：
普通 SDR private read/write 怎么下 command？
Transfer Command / Transfer Argument / Short Data Argument 分别怎么用？

Sir，这一节抓住一个核心规则就行：

```text
普通 I3C SDR private read/write：
  CP = 0
  CMD 字段无效
  Target 地址来自 DAT[DEV_INDX]
  SPEED = SDR0 ~ SDR4
  RnW = 0 表示 write
  RnW = 1 表示 read
```

DWC Databook 里明确说，Non-HCI 模式下 SDR transfer 由软件通过 **Command Data Structure** 发起；普通 I3C private write/read 的传输类型由 `Transfer Command.CP`、`SPEED`、`RnW` 以及 DAT entry 里的 `DEVICE` 位共同决定。`CP=0`、`SPEED=0~4`、`DEVICE=0` 时，`RnW=0` 解码成 I3C private write，`RnW=1` 解码成 I3C private read。

---

# 1. 三种 command 结构分别干什么？

DWC 的 command port 不是只写一个 DWORD。普通 SDR private transfer 通常会用到这三种结构：

|结构|`CMD_ATTR`|作用|
|---|--:|---|
|**Transfer Command**|`0`|真正启动一次 transfer|
|**Transfer Argument**|`1`|提供 `DATA_LENGTH`，大数据/读操作/DMA 必用|
|**Short Data Argument**|`2`|用于 write payload ≤ 3 bytes 的小写入|

Databook 说：`Transfer Command` 用于发起 CCC 和 private transfer；如果 transfer 带 data payload，还要在它之前写入 `Transfer Argument` 或 `Short Data Argument`。`Transfer Argument` 用来提供 payload 信息，使用外部 DMA 搬 payload 时必须用它；`Short Data Argument` 只适用于 write payload 小于等于 3 bytes 的场景，read transfer 不能用 Short Data Argument。

所以软件下命令的基本顺序是：

```text
先写 Argument
再写 Transfer Command
```

也就是：

```text
Transfer Argument / Short Data Argument
  ↓
Transfer Command
  ↓
DWC 开始在 I3C bus 上发起 transfer
```

---

# 2. Transfer Command：真正启动 transfer 的 DWORD

普通 SDR private read/write 的 **Transfer Command** 关键字段如下：

|字段|作用|private SDR 配置|
|---|---|---|
|`CMD_ATTR`|command 类型|`0`，Transfer Command|
|`TID`|transaction id|软件自定义，response 会带回来|
|`CMD`|CCC/HDR command|private transfer 不用，填 0|
|`CP`|Command Present|`0`，表示不看 `CMD` 字段|
|`DEV_INDX`|DAT index|指向目标 Target 的 DAT entry|
|`SPEED`|传输速率|`0~4`，即 SDR0~SDR4|
|`ROC`|Response On Completion|read 强烈建议 `1`|
|`SDAP`|Short Data Argument Present|`0` 用 Transfer Argument，`1` 用 Short Data Argument|
|`RnW`|读写方向|`0` write，`1` read|
|`TOC`|Termination On Completion|`1` STOP，`0` 后面接 RESTART|
|`PEC`|Packet Error Check|SDR private 可选|

最关键的是：

```text
CP = 0
```

因为 private transfer 不带 CCC command code。Target 地址不是来自 `CMD`，而是来自：

```text
DAT[DEV_INDX]
```

Databook 对 `DEV_INDX` 的描述也很重要：它只是 DAT 的 index，不是寄存器 offset，也不是 7-bit address 本身；取值范围要在 `0 ~ IC_DEV_ADDR_TABLE_BUF_DEPTH-1` 内。

---

# 3. Transfer Argument：大多数 read/write 都会用它

**Transfer Argument** 主要提供：

```text
DATA_LENGTH[15:0]
```

也就是这次 transfer 的数据长度。

它的格式可以简化成：

```text
CMD_ATTR = 1
DB       = Defining Byte，仅 CCC with defining byte 时有效
DL       = DATA_LENGTH
```

对普通 SDR private transfer 来说，`DB` 不用，重点是 `DL`：

```text
DL = write length 或 read length
```

## 什么时候用 Transfer Argument？

### 1. Private read 必用

因为 read 没有 immediate data，必须告诉 Controller 你期望读多少字节：

```text
Transfer Argument:
  DATA_LENGTH = N

Transfer Command:
  CP    = 0
  RnW   = 1
  SDAP  = 0
  ROC   = 1
  TOC   = 1 或 0
```

### 2. Write payload > 3 bytes 时用

因为 Short Data Argument 最多只能塞 3 个字节，超过 3 字节就要走 Tx FIFO 或 DMA：

```text
Transfer Argument:
  DATA_LENGTH = N

Tx FIFO:
  写入 N bytes payload

Transfer Command:
  CP    = 0
  RnW   = 0
  SDAP  = 0
```

### 3. 使用 DMA 时必须用

Databook 明确说，Transfer Argument 在使用 external DMA 获取 payload bytes 时必须使用。因为 DMA 只搬 Tx/Rx data，不搬 command，也不自动写 response。

---

# 4. Short Data Argument：小写入优化，最多 3 字节

**Short Data Argument** 是给小 payload write 用的。

格式简化为：

```text
CMD_ATTR    = 2
BYTE_STRB   = 哪些 DATA_BYTE 有效
DATA_BYTE_0
DATA_BYTE_1
DATA_BYTE_2
```

`BYTE_STRB` 合法组合：

```text
001：1 byte 有效
011：2 bytes 有效
111：3 bytes 有效
```

Databook 明确说 Short Data Argument 只对 write transfer 有效；read transfer 必须使用 Transfer Argument。

典型用途是写寄存器地址、写 1~3 字节小命令：

```text
Short Data Argument:
  BYTE_STRB   = 001
  DATA_BYTE_0 = register_offset

Transfer Command:
  CP    = 0
  RnW   = 0
  SDAP  = 1
```

注意这里：

```text
SDAP = 1
```

表示前一个写入 command port 的 DWORD 是 **Short Data Argument**。

如果：

```text
SDAP = 0
```

则表示前一个 DWORD 是 **Transfer Argument**。

---

# 5. 普通 SDR private write 怎么下？

## 场景 A：写 1~3 字节，用 Short Data Argument

例如向 Target 写 1 byte register offset `0x20`：

```text
Step 1：写 Short Data Argument
  CMD_ATTR    = 2
  BYTE_STRB   = 001
  DATA_BYTE_0 = 0x20

Step 2：写 Transfer Command
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target_dat_index
  SPEED    = SDR0/SDR1/SDR2/SDR3/SDR4
  SDAP     = 1
  RnW      = 0
  ROC      = 1，建议打开
  TOC      = 1 或 0
  PEC      = 0/1
```

总线行为大概是：

```text
S
Target_DA + W + ACK
0x20 + T
P 或 Sr
```

如果这是一个“先写寄存器地址、再读数据”的 combined transfer，那么这个 write command 通常设置：

```text
TOC = 0
```

这样后面接 read command 时，DWC 会生成 RESTART，而不是 STOP。

---

## 场景 B：写超过 3 字节，用 Transfer Argument + Tx FIFO

例如写 16 bytes payload：

```text
Step 1：写 Transfer Argument
  CMD_ATTR    = 1
  DATA_LENGTH = 16

Step 2：写 Tx FIFO / 或启动 DMA
  payload[0..15]

Step 3：写 Transfer Command
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target_dat_index
  SPEED    = SDR0~SDR4
  SDAP     = 0
  RnW      = 0
  ROC      = 1，建议打开
  TOC      = 1
  PEC      = 0/1
```

总线行为：

```text
S
Target_DA + W + ACK
Data0 + T
Data1 + T
...
Data15 + T
P
```

这里第 9 bit 是 I3C SDR write 的 T-bit/parity，不是 I2C 那种每字节 ACK。

---

# 6. 普通 SDR private read 怎么下？

Private read 一律用 **Transfer Argument**，不用 Short Data Argument。

例如读 32 bytes：

```text
Step 1：写 Transfer Argument
  CMD_ATTR    = 1
  DATA_LENGTH = 32

Step 2：写 Transfer Command
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target_dat_index
  SPEED    = SDR0~SDR4
  SDAP     = 0
  RnW      = 1
  ROC      = 1
  TOC      = 1
  PEC      = 0/1
```

总线行为：

```text
S
Target_DA + R + ACK
Data0 + T
Data1 + T
...
P
```

这里 `ROC=1` 很重要。Databook 建议 read command 总是设置 `ROC=1`，因为 I3C Target 可以通过 T-bit 比 Controller 期望更早结束读；response 里的 `DATA_LENGTH` 可以告诉软件实际收到了多少数据。

所以 read 完成后的软件逻辑应该是：

```text
1. 等 Response Queue
2. 检查 ERR_STS
3. 从 response.DATA_LENGTH 得到实际接收长度
4. 从 Rx FIFO 读取 actual_len bytes
```

不要简单认为：

```text
actual_len 一定等于 request_len
```

I3C SDR read 允许 Target 提前结束。

---

# 7. 典型 combined read：先写 offset，再 RESTART 读

很多设备的私有协议是：

```text
先写 register/sub-address
再 repeated START 读数据
```

在 DWC 里可以下两条 command。

## 第一条：private write，写 offset，`TOC=0`

```text
Short Data Argument:
  CMD_ATTR    = 2
  BYTE_STRB   = 001
  DATA_BYTE_0 = reg_offset

Transfer Command:
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target
  SPEED    = SDR0
  SDAP     = 1
  RnW      = 0
  ROC      = 0 或 1
  TOC      = 0
```

`TOC=0` 表示：

```text
不要 STOP，下一条 transfer 用 RESTART 接上
```

## 第二条：private read，读 N bytes，`TOC=1`

```text
Transfer Argument:
  CMD_ATTR    = 1
  DATA_LENGTH = N

Transfer Command:
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target
  SPEED    = SDR0
  SDAP     = 0
  RnW      = 1
  ROC      = 1
  TOC      = 1
```

总线效果：

```text
S
Target_DA + W + ACK
reg_offset + T
Sr
Target_DA + R + ACK
Data0 + T
Data1 + T
...
P
```

这就是 DWC 里最常见的 private register read flow。

---

# 8. TX/RX_START_THLD 对 private transfer 的影响

Databook 对 private transfer 有一个很实用的说明：为了避免 transfer 刚开始时出现内部延迟，Controller 会使用 `TX_START_THLD` / `RX_START_THLD`。

含义是：

```text
TX_START_THLD：
  write transfer 开始前，Tx buffer 至少要有这个阈值的数据

RX_START_THLD：
  read transfer 开始前，Rx buffer 至少要有这个阈值的空位
```

但这个 threshold 只适用于用 START 发起的 SDR transfer；如果是接在前一条后面的 RESTART transfer，则不适用。

这对驱动很关键：

```text
write 大包前：
  先填 Tx FIFO / 开 DMA，再下 Transfer Command

read 大包前：
  确保 Rx FIFO 有空间，否则可能起不来或 stall/error
```

---

# 9. IBA_INCLUDE 对 private transfer 的影响

Databook 还提到一个和 IBI 优先级相关的 bit：

```text
DEVICE_CTRL[IBA_INCLUDE]
```

如果打开它，DWC Controller 可以在 I3C private transfer 中包含 Address Header，以便让 Target IBI 有机会在地址头仲裁中获胜。因为 IBI 和 Address Header 仲裁时，IBI 总是可以赢。

理解成：

```text
不开 IBA_INCLUDE：
  Controller 更直接地发 private target address

开 IBA_INCLUDE：
  前面带可仲裁地址头，给 IBI 插入机会
```

如果系统对 IBI latency 敏感，可以关注这个配置。

---

# 10. 错误和 Response Queue 怎么看？

普通 private transfer 常见错误：

```text
Target address NACK
Address header NACK，如果 IBA_INCLUDE enabled
PEC error
Parity/frame error
Transfer abort/terminate
```

Databook 说，private transfer 地址阶段收到 NACK 时，Controller 会更新 response 里的 `ERR_STS`，然后 halt，把控制权交回软件；软件需要写 `DEVICE_CTRL[RESUME]` 恢复。

所以每条 private transfer 的完成处理建议是：

```c
rsp = read_response_queue();

if (rsp.err_sts) {
    dump_debug();
    flush_or_drain_queues_if_needed();
    write DEVICE_CTRL[RESUME] = 1;
    return error;
}

if (read_transfer) {
    actual_len = rsp.data_length;
    read_rx_fifo(buf, actual_len);
}
```

对于 write，如果你设置了：

```text
ROC = 1
```

成功完成也会有 response。  
如果：

```text
ROC = 0
```

正常完成可能没有 response，但出错仍然会产生 response。

---

# 11. 最简配置模板

## Private write，payload ≤ 3 bytes

```text
Short Data Argument:
  CMD_ATTR    = 2
  BYTE_STRB   = 001 / 011 / 111
  DATA_BYTE_x = payload bytes

Transfer Command:
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target DAT index
  SPEED    = SDR0~SDR4
  SDAP     = 1
  RnW      = 0
  ROC      = 1
  TOC      = 1 or 0
  PEC      = 0 or 1
```

## Private write，payload > 3 bytes

```text
Transfer Argument:
  CMD_ATTR    = 1
  DATA_LENGTH = N

Tx FIFO:
  N bytes payload

Transfer Command:
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target DAT index
  SPEED    = SDR0~SDR4
  SDAP     = 0
  RnW      = 0
  ROC      = 1
  TOC      = 1 or 0
  PEC      = 0 or 1
```

## Private read，read N bytes

```text
Transfer Argument:
  CMD_ATTR    = 1
  DATA_LENGTH = N

Transfer Command:
  CMD_ATTR = 0
  CP       = 0
  DEV_INDX = target DAT index
  SPEED    = SDR0~SDR4
  SDAP     = 0
  RnW      = 1
  ROC      = 1
  TOC      = 1 or 0
  PEC      = 0 or 1
```

一句话记忆：**Transfer Command 负责“启动谁、读写、速度、STOP/RESTART”；Transfer Argument 负责“大数据长度”；Short Data Argument 负责“≤3 字节的小写入”。普通 private transfer 永远 `CP=0`，因为它不是 CCC。**