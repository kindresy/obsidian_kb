Sir，下面我按 **DWC_mipi_i3c_v111_basic Non-HCI Controller mode** 来讲实际寄存器配置 flow。先说一个边界：这本 Databook 从 1.03a 起把完整 **Register Descriptions / offset / reset value / bit 定义**移到单独的 **Reference Manual**，所以这里我会讲 **寄存器级配置顺序、关键寄存器/字段、命令队列数据结构**；具体 offset 要以你手上的 Reference Manual 或 coreConsultant 生成的 header 为准。

# 1. 总体寄存器 flow

你可以把 DWC I3C Controller 初始化拆成 12 个阶段：

```text
1.  确认 Controller 处于 disable
2.  清 interrupt / 清 queue / 清 FIFO
3.  配 timing：OD / PP / I2C / Bus Free / Bus Available
4.  配 Controller 自己的 Dynamic Address
5.  配 FIFO / Queue threshold
6.  配 DMA handshake，如果使用外部 DMA
7.  配 DAT：Device Address Table
8.  Enable Controller
9.  下发 DAA：SETDASA / ENTDAA / SETAASA
10. 读 Response Queue 和 DCT
11. 发 GETPID / GETBCR / GETDCR / GETSTATUS / GETMXDS 等 CCC
12. 配 IBI / Hot-Join / SIR / MR，进入正常 SDR/HDR 传输
```

对应的软件对象是：

```text
DEVICE_CTRL
DEVICE_ADDR
INTR_STATUS / INTR_STATUS_EN / INTR_SIGNAL_EN
QUEUE_THLD_CTRL
DATA_BUFFER_THLD_CTRL
I3C_OD_TIMING
I3C_PP_TIMING
I2C_FM_TIMING / I2C_FMP_TIMING
BUS_FREE_TIMING
DEVICE_ADDR_TABLE_POINTER
DEV_ADDR_TABLE_LOCx
DEV_CHAR_TABLE_LOCx
COMMAND_QUEUE_PORT
RESPONSE_QUEUE_PORT
RX_TX_DATA_PORT
IBI_QUEUE_STATUS / IBI_DATA
```

---

# 2. Phase 1：先 disable Controller

初始化时先不要让 IP 立刻驱动总线。

关键动作：

```c
DEVICE_CTRL[ENABLE] = 0;
poll DEVICE_CTRL[ENABLE] == 0;
```

Databook 说，如果 Controller 正在执行 transfer 或接收 IBI，清 `DEVICE_CTRL[ENABLE]` 后不会立刻停，而是等当前带 `TOC=1` 的 transfer 或 IBI reception 结束后才进入 disabled state；disabled 后，不再执行 Command Queue，也不会为新的 IBI 提供 SCL clock。重新 enable 前，软件应 flush/drain queues 和 FIFOs。

所以初始化开头建议做成：

```c
dw_i3c_clr_bits(DEVICE_CTRL, ENABLE);
dw_i3c_wait_until_disabled();

dw_i3c_reset_cmd_queue();
dw_i3c_reset_resp_queue();
dw_i3c_reset_tx_fifo();
dw_i3c_reset_rx_fifo();
dw_i3c_reset_ibi_status_queue();
dw_i3c_reset_ibi_data_queue();
```

实际 reset 位通常在 `RESET_CTRL` 或类似 register 中，具体 bit 名以 Reference Manual 为准。

---

# 3. Phase 2：清 interrupt，先关 signal 输出

DWC 是 combined interrupt pin。Databook 里说：

```text
INTR_STATUS     ：记录真实状态
INTR_STATUS_EN  ：控制对应状态是否参与 interrupt/status 逻辑
INTR_SIGNAL_EN  ：控制是否真正拉高外部 interrupt pin
INTR_FORCE      ：测试用，强制触发 interrupt
```

其中一部分 status interrupt auto-clear，其余 event interrupt 通过写 1 清除。

初始化建议顺序：

```c
/* 先避免初始化过程中误触发外部中断 */
dw_write(INTR_SIGNAL_EN, 0x0);

/* 关或配置 status enable */
dw_write(INTR_STATUS_EN, 0x0);

/* W1C 清历史状态 */
dw_write(INTR_STATUS, 0xffffffff);

/* 后面配置完 queue/fifo threshold 后再打开需要的 status */
dw_write(INTR_STATUS_EN, wanted_status_mask);
dw_write(INTR_SIGNAL_EN, wanted_signal_mask);
```

最小需要关注的中断类型通常包括：

```text
Command Response Ready / Response Queue threshold
Transfer Error
Transfer Abort
Tx FIFO threshold
Rx FIFO threshold
IBI threshold
DAA complete / DCT updated
Bus error / protocol error
```

不同配置下 bit 名可能不同，但思路就是：**先清旧状态，再配 mask，再 enable signal**。

---

# 4. Phase 3：配置 timing

这一步必须在发 DAA 之前完成，因为 DAA 本身就是 I3C 总线事务，尤其 ENTDAA 全流程使用 I3C Open-Drain mode。Databook 明确说完整 ENTDAA 从 START 到 STOP 都由 Controller 在 I3C Open-Drain mode 下生成，并且 ENTDAA 总是以 STOP 结束，推荐 `TOC=1`。

常见 timing register：

```text
I3C_OD_TIMING       ：Open-Drain SCL high/low count
I3C_PP_TIMING       ：Push-Pull SCL high/low count
I2C_FM_TIMING       ：Legacy I2C FM timing
I2C_FMP_TIMING      ：Legacy I2C FM+ timing
BUS_FREE_TIMING     ：Bus Free / Bus Available 相关
BUS_IDLE_TIMING     ：Bus Idle / Hot-Join 相关
SDA_HOLD_SWITCH_DLY ：SDA hold / switch delay
```

配置原则：

```text
ENTDAA / 7'h7E / address arbitration / IBI address phase：
  依赖 OD timing

SDR data phase：
  依赖 PP timing

Legacy I2C transfer：
  依赖 I2C FM/FM+ timing

IBI / Hot-Join 判断：
  依赖 Bus Free / Bus Available / Bus Idle timing
```

如果总线上有 Legacy I2C Target，还要先判断 bus type：

```text
Pure Bus：
  只有 I3C devices

Mixed Fast Bus：
  I3C + Legacy I2C Target，且 I2C Target 有 50ns spike filter

Mixed Slow/Limited Bus：
  I3C + Legacy I2C Target，但没有可靠 50ns spike filter 或未知
```

Databook 说，如果没有 spike filter 或 filter 状态未知，最大速率甚至访问 I3C device 时也可能只能限制到 FM/FM+。

---

# 5. Phase 4：配置 Controller 自身 Dynamic Address

这是 DWC 初始化非常关键的一步。

Databook 明确说，DWC 如果作为 Active Controller，需要 host application 在 `DEVICE_ADDR` 中配置自己的 Dynamic Address，并置位 `DYNAMIC_ADDR_VALID`；当这个 bit 为 1 时，DWC 在 power-up 后 owns bus，可以驱动 SCL 并在 SDA 上发起 transfer。

概念代码：

```c
u8 ctrl_da = 0x08;   /* 举例，不能使用 reserved address */

dw_write(DEVICE_ADDR,
         FIELD_PREP(DYNAMIC_ADDR, ctrl_da) |
         DYNAMIC_ADDR_VALID);
```

注意这个地址是：

```text
Active Controller 自己的 Dynamic Address
```

不是分配给 Target 的地址。

不要使用：

```text
7'h00
7'h01
7'h02    Hot-Join address
7'h7E    Broadcast address
7'h7F
```

也不要使用和 `7'h7E` 单 bit 错误相关的禁用地址。

---

# 6. Phase 5：配置 FIFO / Queue threshold

DWC 内部有 SPRAM，里面按配置切分成：

```text
Command Queue
Response Queue
Tx FIFO
Rx FIFO
IBI Status Queue
IBI Data Queue
Device Address Table, DAT
Device Characteristics Table, DCT
Extended Tx FIFO
```

Databook 给出的公式是：

```text
IC_RAM_DEPTH =
  IC_CMD_BUF_DEPTH
+ IC_TX_BUF_DEPTH
+ IC_RX_BUF_DEPTH
+ IC_IBI_STS_BUF_DEPTH
+ IC_IBI_DATA_BUF_DEPTH
+ IC_DEV_ADDR_TABLE_BUF_DEPTH
+ IC_DEV_CHAR_TABLE_BUF_DEPTH
+ IC_EXT_QUEUE_CNT * IC_EXT_TX_BUF_DEPTH
```

并且它提醒：DAT/DCT/FIFO/queue 的实际深度来自 coreConsultant 配置，不是所有实例都一样。

常见 threshold：

```text
QUEUE_THLD_CTRL[RESP_BUF_THLD]
QUEUE_THLD_CTRL[IBI_STATUS_THLD]
QUEUE_THLD_CTRL[IBI_DATA_THLD]

DATA_BUFFER_THLD_CTRL[TX_EMPTY_BUF_THLD]
DATA_BUFFER_THLD_CTRL[RX_BUF_THLD]
DATA_BUFFER_THLD_CTRL[TX_START_THLD]
DATA_BUFFER_THLD_CTRL[RX_START_THLD]
```

推荐理解：

```text
TX_EMPTY_BUF_THLD：
  Tx FIFO 空到一定程度，触发填数据或 DMA request

RX_BUF_THLD：
  Rx FIFO 数据达到一定程度，触发软件读取或 DMA request

TX_START_THLD：
  启动写 transfer 前，Tx FIFO 至少要有多少数据

RX_START_THLD：
  启动读 transfer 前，Rx FIFO 至少要有多少空间

IBI_STATUS_THLD：
  IBI status queue 到达多少条后通知软件
```

Databook 对 IBI 特别建议：为了避免 IBI Status/Data Queue 满，最好尽快 drain IBI queue，并把 IBI Status threshold 设为 0，这样第一个 IBI status 到达就通知软件。

---

# 7. Phase 6：配置 DMA handshake，可选

如果使用外部 DMA，注意 DWC 的 SDMA handshake 只负责 **Tx FIFO / Rx FIFO payload 数据搬运**：

```text
DMA 不负责 fetch I3C command
DMA 不负责 post I3C response
```

Databook 明确说，SDMA transfer exclusively for fetching transmit and receive data，不用于 I3C command，也不用于 response。

配置点：

```c
/* 具体寄存器名依配置而定 */
dw_write(DATA_BUFFER_THLD_CTRL, tx_rx_thresholds);

/* 外部 DMAC */
dmac.program_src_dst();
dmac.program_block_ts();
dmac.program_msize_same_as_i3c_threshold();
dmac.enable_channel();

/* DWC */
dw_i3c_enable_dma_handshake();
```

关键匹配关系：

```text
DW_ahb_dmac CTLx.SRC_MSIZE / DEST_MSIZE
必须和 DWC DATA_BUFFER_THLD_CTRL 里的 RX_BUF_THLD / TX_EMPTY_BUF_THLD 匹配
```

否则 DMA burst 粒度和 I3C FIFO request 粒度不一致，容易出现 underrun / overrun / 尾包处理错误。

---

# 8. Phase 7：配置 DAT

DAT 是后续所有 `DEV_INDX` 的基础。

DWC 的 Command Data Structure 里，`DEV_INDX` 不是地址本身，而是 **Device Address Table 的 index**。Databook 反复提醒：`DEV_INDX` 的范围是 `0 ~ IC_DEV_ADDR_TABLE_BUF_DEPTH-1`，它和寄存器 offset 无关。

先读：

```text
DEVICE_ADDR_TABLE_POINTER
```

拿到 DAT start address / depth。Databook 说 DAT register offset 会随配置变化，正确 offset 要从 coreConsultant GUI 或 `DEVICE_ADDR_TABLE_POINTER` 获取。

## 8.1 ENTDAA 用 DAT 预填

对没有 Static Address 的 I3C device，ENTDAA 前你要在 DAT 里预填准备分配出去的 Dynamic Address 和 parity：

```text
DAT[n]:
  DYN_ADDR = assigned_da
  DYN_ADDR_PARITY = ~XOR(DYN_ADDR)
```

Databook 明确说：application 必须在填 DAT 前自己计算 Dynamic Address parity。

示意代码：

```c
static u8 odd_parity_for_da(u8 da)
{
    da &= 0x7f;
    return !( __builtin_parity(da) ); /* ~XOR(DYN_ADDR) */
}

for (i = 0; i < entdaa_slots; i++) {
    u8 da = dynamic_addr_pool[i];

    dat = 0;
    dat |= FIELD_PREP(DAT_DYN_ADDR, da);
    dat |= FIELD_PREP(DAT_DYN_ADDR_PARITY, odd_parity_for_da(da));

    dw_write(DEV_ADDR_TABLE_LOC(i), dat);
}
```

## 8.2 SETDASA 用 DAT 预填

对已知 Static Address 的 I3C-capable Target，SETDASA 前 DAT 要包含：

```text
Static Address
Assigned Dynamic Address
```

Databook 的 SETDASA 流程是：软件初始化 Controller，编程 DAT，然后下发 Address Assignment Command，`CMD=SETDASA`，`DEV_INDEX` 指向 DAT 起始项，`DEVICE_COUNT` 表示要连续分配几个设备。

## 8.3 Legacy I2C Target 的 DAT

Legacy I2C 不参与 DAA。Databook 明确说 Legacy I2C device 不参与 Dynamic Address Assignment，直接用已知 Static Address 访问。

所以 Legacy I2C Target 的 DAT entry 要表达：

```text
LEGACY_I2C_DEVICE = 1
STATIC_ADDR       = i2c_addr
```

后续 private transfer 中，DWC 通过 DAT 的 `LEGACY_I2C_DEVICE` 判断是 I2C private transfer 还是 I3C private transfer。

---

# 9. Phase 8：Enable Controller

前面都配置完后，再 enable：

```c
dw_set_bits(DEVICE_CTRL, ENABLE);
```

此时 DWC 才开始：

```text
驱动 SCL
发 START / STOP
执行 Command Queue
处理 IBI / Hot-Join
执行 DAA / CCC / private transfer
```

---

# 10. Phase 9：下发 DAA 命令

DWC 有专门的 **Address Assignment Command Data Structure**，用于 `ENTDAA` 和 `SETDASA`。Databook 说 `SETAASA` 和 `SETNEWDA` 不用 Address Assignment Command，而是用 regular transfer command。

Address Assignment Command 关键字段：

```text
CMD_ATTR   = 3
TID        = 软件自定义 transaction id
CMD        = ENTDAA 或 SETDASA CCC code
DEV_INDX   = DAT 起始 index
DEV_COUNT  = 要分配的 device 数量
ROC        = 1，建议要 response
TOC        = 1，尤其 ENTDAA 推荐 1
```

Databook 还说 Address Assignment Command 一次最多给 31 个设备分配 Dynamic Address；第 32 个要再发一条命令。

## 10.1 ENTDAA 命令示意

```c
u32 cmd = 0;

cmd |= FIELD_PREP(CMD_ATTR, CMD_ATTR_ADDR_ASSIGN); /* 3 */
cmd |= FIELD_PREP(TID, tid);
cmd |= FIELD_PREP(CMD, CCC_ENTDAA);
cmd |= FIELD_PREP(DEV_INDX, first_dat_index);
cmd |= FIELD_PREP(DEV_COUNT, count);
cmd |= ROC;
cmd |= TOC;   /* DWC recommends TOC=1 for ENTDAA */

dw_write(COMMAND_QUEUE_PORT, cmd);
```

DWC 会自动在线上生成：

```text
S
7'h7E + W + ACK
ENTDAA
Sr / S
7'h7E + R + ACK
Target returns PID/BCR/DCR
Controller assigns Dynamic Address + PAR
...
7'h7E + R + NACK
P
```

ENTDAA 结束条件包括：

```text
0x7E/W NACK：没有 I3C device
0x7E/R NACK：所有参与设备都已拿到 Dynamic Address
Assigned Dynamic Address NACK：Target 检测到地址错误
DEVICE_COUNT 到 0：本次命令分配数量已满
```

## 10.2 SETDASA 命令示意

```c
u32 cmd = 0;

cmd |= FIELD_PREP(CMD_ATTR, CMD_ATTR_ADDR_ASSIGN); /* 3 */
cmd |= FIELD_PREP(TID, tid);
cmd |= FIELD_PREP(CMD, CCC_SETDASA);
cmd |= FIELD_PREP(DEV_INDX, first_dat_index);
cmd |= FIELD_PREP(DEV_COUNT, count);
cmd |= ROC;
cmd |= TOC;

dw_write(COMMAND_QUEUE_PORT, cmd);
```

DWC 会生成：

```text
S/Sr
7'h7E + W + ACK
SETDASA
Sr
Static Address + W + ACK
Assigned Dynamic Address
P/Sr
```

## 10.3 SETAASA 命令示意

SETAASA 是 Broadcast CCC，没有 data payload，Databook 说它要用 regular transfer command，不用 Address Assignment Command。

```c
u32 xfer = 0;

xfer |= FIELD_PREP(CMD_ATTR, CMD_ATTR_TRANSFER); /* 0 */
xfer |= FIELD_PREP(TID, tid);
xfer |= FIELD_PREP(CMD, CCC_SETAASA);            /* 0x29 */
xfer |= CP;                                      /* command present */
xfer |= FIELD_PREP(SPEED, SDR0);
xfer |= FIELD_PREP(RnW, 0);
xfer |= ROC;
xfer |= TOC;

dw_write(COMMAND_QUEUE_PORT, xfer);
```

SETAASA 成功后，Databook 特别提醒：软件要更新 DAT，让 DAT 中的 Dynamic Address 等于 Static Address。

---

# 11. Phase 10：读 Response Queue 和 DCT

每条 command 完成后，DWC 会写 Response Queue。对 DAA，Response 里重点看：

```text
TID          ：对应哪条命令
ERR_STS      ：是否 NACK / protocol error / abort 等
DATA_LENGTH  ：DAA 异常结束时剩余 device count
```

Databook 说 SETDASA/ENTDAA 如果因为 NACK 异常终止，Response Data Structure 的 Data Length field 会表示 remaining device count。

概念代码：

```c
rsp = dw_read(RESPONSE_QUEUE_PORT);

if (RSP_ERR_STS(rsp)) {
    handle_daa_error(rsp);
}

remaining = RSP_DATA_LENGTH(rsp);
assigned = requested_count - remaining;
```

ENTDAA 期间，DWC 会把参与分配的 winning device 信息写到 DCT：

```text
PID[47:0]
BCR
DCR
Assigned Dynamic Address
```

Databook 的 DCT 结构中，ENTDAA 捕获信息按 4 个 DWORD 存放；其中包含 PID、BCR、DCR 和 DA。它还提醒：application 写 DAT，controller 写 DCT；SPRAM 未初始化时，写之前读 DAT/DCT 会得到 undefined values。

概念代码：

```c
for (i = 0; i < assigned; i++) {
    dct0 = dw_read(DEV_CHAR_TABLE_LOC(i, 0));
    dct1 = dw_read(DEV_CHAR_TABLE_LOC(i, 1));
    dct2 = dw_read(DEV_CHAR_TABLE_LOC(i, 2));
    dct3 = dw_read(DEV_CHAR_TABLE_LOC(i, 3));

    pid = parse_pid(dct0, dct1);
    bcr = parse_bcr(dct2);
    dcr = parse_dcr(dct2);
    da  = parse_da(dct3);

    sw_devs[i].pid = pid;
    sw_devs[i].bcr = bcr;
    sw_devs[i].dcr = dcr;
    sw_devs[i].da  = da;
}
```

DCT 读完后要回写/修正 DAT，例如：

```text
BCR[2] = 1 → Target 支持 IBI MDB/payload
           → DAT[IBI_PAYLOAD] 应该设置

某设备不允许 SIR
           → DAT[SIR_REJECT] 设置为 reject

Secondary Controller
           → 配 MR response policy
```

Databook 也明确建议，ENTDAA 后可以发 `GETBCR / GETDCR / GETPID` 到该 Dynamic Address，并和 DCT 中捕获的值比较，确认响应的是同一个设备。

---

# 12. Phase 11：发 Direct CCC 做能力枚举

DAA 完成只是“有地址了”。接下来还要枚举能力。

常用 Direct CCC：

```text
GETPID
GETBCR
GETDCR
GETSTATUS
GETMXDS
GETCAPS
GETMRL / GETMWL
```

Direct CCC Read 的 DWC 配置方式：

```text
Transfer Argument：
  DATA_LENGTH = expected read length

Transfer Command：
  CMD_ATTR = 0
  CP       = 1
  CMD      = Direct CCC code，例如 GETBCR/GETDCR/GETPID
  DEV_INDX = DAT index
  SPEED    = SDR0
  RnW      = 1
  ROC      = 1
  TOC      = 1 或按 pipeline 需要设置
```

概念代码：

```c
/* 1. 先写 Transfer Argument */
arg = 0;
arg |= FIELD_PREP(CMD_ATTR, CMD_ATTR_TRANSFER_ARG); /* 1 */
arg |= FIELD_PREP(DATA_LENGTH, expected_len);
dw_write(COMMAND_QUEUE_PORT, arg);

/* 2. 再写 Transfer Command */
cmd = 0;
cmd |= FIELD_PREP(CMD_ATTR, CMD_ATTR_TRANSFER);     /* 0 */
cmd |= FIELD_PREP(TID, tid);
cmd |= FIELD_PREP(CMD, CCC_GETBCR);
cmd |= CP;
cmd |= FIELD_PREP(DEV_INDX, dat_index);
cmd |= FIELD_PREP(SPEED, SDR0);
cmd |= FIELD_PREP(RnW, 1);
cmd |= ROC;
cmd |= TOC;

dw_write(COMMAND_QUEUE_PORT, cmd);

/* 3. 等 response，再从 RX FIFO 读 expected/actual bytes */
rsp = dw_read(RESPONSE_QUEUE_PORT);
len = RSP_DATA_LENGTH(rsp);
dw_read_rx_fifo(buf, len);
```

Databook 说 `Transfer Argument` 必须在 `Transfer Command` 之前写入 command port；如果是 write 且 payload ≤ 3 bytes，可以用 `Short Data Argument`，但 read 必须用 `Transfer Argument`。

---

# 13. Phase 12：配置 IBI / Hot-Join / MR / SIR

DWC 把 IBI 分成：

```text
HJ  ：Hot-Join Request
SIR ：Target Interrupt Request
MR  ：Controller Ownership Request
```

Controller 收到 Target 拉低 SDA 后，会提供 SCL clock 接收 IBI ID。Databook 描述的检测场景包括：POR 后检测 SDA low、Controller 发 START 后地址阶段 arbitration loss、STOP 后 SDA 被 Target 拉低。

关键配置对象：

```text
DEVICE_CTRL[HOT_JOIN_CTRL]
IBI_QUEUE_CTRL[NOTIFY_HJ_REJECTED]
IBI_QUEUE_CTRL[NOTIFY_SIR_REJECTED]
IBI_QUEUE_CTRL[NOTIFY_MR_REJECTED]
IBI_SIR_REQ_REJECT
IBI_MR_REQ_REJECT
DAT[SIR_REJECT]
DAT[MR_REJECT]
DAT[IBI_PAYLOAD]
QUEUE_THLD_CTRL[IBI_STATUS_THLD]
QUEUE_THLD_CTRL[IBI_DATA_THLD]
```

有两种配置路径：

```text
IC_HAS_IBI_DATA = 0：
  SIR/MR reject 可能通过 32-bit vector register 控制

IC_HAS_IBI_DATA = 1：
  SIR/MR reject 和 IBI payload 通常放在对应 DAT entry 中
```

SIR with payload 时尤其注意：

```text
只有 Target 的 BCR[2] = 1，才表示支持 Mandatory Data Byte / IBI payload；
此时才应该设置 DAT[IBI_PAYLOAD] 或 IBI Payload Control。
```

Databook 明确说，如果 Target 不支持 mandatory byte，也就是 BCR[2]=0，application 不应设置 IBI Payload Control。

然后用 CCC 开事件：

```text
ENEC：Enable Event Command
DISEC：Disable Event Command
```

例如开启 SIR：

```c
/* Broadcast or Directed ENEC，具体看策略 */
dw_i3c_ccc_enec(target, ENINT);
```

开启 Hot-Join：

```c
dw_i3c_ccc_enec_broadcast(ENHJ);
```

---

# 14. 正常 SDR private transfer 的寄存器配置

## 14.1 I3C Private Write

DWC 的 private transfer 用 `CP=0`，即不使用 `CMD` 字段。

步骤：

```text
1. 如果 payload > 3 bytes：
     写 Transfer Argument，DATA_LENGTH = write_len
     把 payload 放入 Tx FIFO，或启动 DMA 填 Tx FIFO

2. 如果 payload <= 3 bytes：
     可以写 Short Data Argument，BYTE_STRB 指示有效字节

3. 写 Transfer Command：
     CMD_ATTR = 0
     CP       = 0
     DEV_INDX = target DAT index
     SPEED    = SDR0~SDR4
     RnW      = 0
     ROC      = 1，建议开
     TOC      = 1 或 0
     PEC      = 0/1
```

对应概念代码：

```c
/* payload 大于 3 bytes */
arg = FIELD_PREP(CMD_ATTR, CMD_ATTR_TRANSFER_ARG) |
      FIELD_PREP(DATA_LENGTH, len);
dw_write(COMMAND_QUEUE_PORT, arg);

dw_write_tx_fifo(buf, len);

cmd = FIELD_PREP(CMD_ATTR, CMD_ATTR_TRANSFER) |
      FIELD_PREP(TID, tid) |
      FIELD_PREP(DEV_INDX, dat_index) |
      FIELD_PREP(SPEED, SDR0) |
      FIELD_PREP(RnW, 0) |
      ROC | TOC;

dw_write(COMMAND_QUEUE_PORT, cmd);
```

Databook 的表 3-25 对 I3C private read/write 的要求是：`CP=0`，`DEV_INDX` 指向 DAT，`SPEED=0~4`，`RnW=0/1`，`PEC=0/1`，`DATA_LENGTH=0~65535`。

## 14.2 I3C Private Read

步骤：

```text
1. 写 Transfer Argument：
     DATA_LENGTH = 希望读取的最大长度

2. 写 Transfer Command：
     CP       = 0
     DEV_INDX = target DAT index
     SPEED    = SDR0~SDR4
     RnW      = 1
     ROC      = 1，强烈建议
     TOC      = 1 或 0

3. 等 response

4. 从 Rx FIFO 读取实际长度
```

为什么 `ROC=1` 很重要？Databook 说 read command 建议总是设置 `ROC=1`，这样如果 Target 比 Controller 期望更早终止读，Response 的 `DATA_LENGTH` 能指出实际收到的数据长度。

---

# 15. 正常 CCC transfer 的寄存器配置

## 15.1 Broadcast CCC Write

```text
Transfer Command：
  CP       = 1
  CMD[14]  = 0，表示 Broadcast CCC
  CMD[13:7]= CCC code，0x00~0x7F
  DEV_INDX = NA
  SPEED    = SDR0
  DBP      = 0/1
  RnW      = 0
  PEC      = 0/1
```

如果有 payload：

```text
payload > 3 bytes：
  用 Transfer Argument + Tx FIFO

payload <= 3 bytes：
  用 Short Data Argument
```

## 15.2 Directed CCC Read/Write

```text
Transfer Command：
  CP       = 1
  CMD[14]  = 1，表示 Directed CCC
  CMD[13:7]= CCC code
  DEV_INDX = target DAT index
  SPEED    = SDR0
  DBP      = 0/1
  RnW      = 0/1
  ROC      = 1
  TOC      = 1 或 0
```

如果要连续对多个 Target 发同一个 Directed CCC，可以：

```text
Target0 command: TOC=0, ROC=0
Target1 command: TOC=0, ROC=0
Target2 command: TOC=1, ROC=1
```

这样总线上会形成 repeated START 串联，减少不必要 response。Databook 在 directed CCC 章节也提到，面向多个设备时，后续 directed CCC command 可以关闭 ROC，只在最后一个 command 打开 ROC，避免多余 responses。

---

# 16. HDR 配置入口

## 16.1 HDR-DDR

HDR-DDR 使用普通 Transfer Command，但必须使用 `Transfer Argument`，不能用 `Short Data Argument`。

关键字段：

```text
Transfer Argument：
  DATA_LENGTH = payload length

Transfer Command：
  CMD_ATTR = 0
  CP       = 1
  CMD      = HDR command code
  DEV_INDX = target DAT index
  SPEED    = HDR-DDR
  RnW      = 0/1
  ROC      = 1
  TOC      = 1 或 0，决定 HDR Exit / HDR Restart 语义
```

Databook 的 Transfer Command 表里说 `SPEED=6` 表示 HDR-DDR，并说明 HDR-DDR 只支持 Transfer Argument，不支持 Short Data Argument。

## 16.2 HDR-BT

HDR-BT 不用普通 Transfer Command，而用 **Unified Command Structure**。

Databook 说 Unified Command Structure 是 4 DWORD，当前 release 中限制为只用于 HDR-BT transfers。

概念：

```text
DWORD0：Unified Transfer Command
DWORD1~3：IDB0/CCC/CMD0、IDB1/DB/CMD1、HDR command/control 等
```

HDR-BT 以后我们可以单独拆，因为它和普通 SDR/HDR-DDR 的 command model 不一样。

---

# 17. 错误处理寄存器 flow

如果 transfer 出错，DWC 通常会：

```text
1. 写 Response Queue
2. ERR_STS 给出错误原因
3. Controller 进入 halt state
4. 软件处理后写 DEVICE_CTRL[RESUME] 恢复
```

Databook 多处说明，收到 NACK、协议错误等情况下，controller 会更新 Response 的 `ERR_STS`，halt controller，然后软件通过写 `DEVICE_CTRL[RESUME]=1` 恢复。

典型错误处理：

```c
rsp = dw_read(RESPONSE_QUEUE_PORT);

if (RSP_ERR_STS(rsp)) {
    err = RSP_ERR_STS(rsp);

    dw_i3c_dump_debug_regs();
    dw_i3c_flush_cmd_queue();
    dw_i3c_flush_resp_queue();
    dw_i3c_flush_tx_fifo();
    dw_i3c_flush_rx_fifo();

    dw_set_bits(DEVICE_CTRL, RESUME);
}
```

主动终止 transfer：

```c
dw_set_bits(DEVICE_CTRL, ABORT);
wait INTR_STATUS[TRANSFER_ABORT_STAT];

dw_i3c_flush_queues_and_fifos();
dw_set_bits(DEVICE_CTRL, RESUME);
```

Databook 说设置 `DEVICE_CTRL[ABORT]` 后，Controller 会在当前 data byte 发送/接收完成后发 STOP，置 `INTR_STATUS[TRANSFER_ABORT_STAT]`，进入 halt state，然后等待软件写 `DEVICE_CTRL[RESUME]`。

---

# 18. 一个完整 cold boot 伪代码

```c
int dw_i3c_controller_init(void)
{
    /* 1. Disable controller */
    clr_bits(DEVICE_CTRL, ENABLE);
    wait_until(!(read(DEVICE_CTRL) & ENABLE));

    /* 2. Mask interrupt and clear old status */
    write(INTR_SIGNAL_EN, 0);
    write(INTR_STATUS_EN, 0);
    write(INTR_STATUS, 0xffffffff);   /* W1C */

    /* 3. Reset queues/FIFOs */
    reset_cmd_queue();
    reset_resp_queue();
    reset_tx_fifo();
    reset_rx_fifo();
    reset_ibi_status_queue();
    reset_ibi_data_queue();

    /* 4. Program timing */
    write(I3C_OD_TIMING, calc_od_timing(core_clk, od_freq));
    write(I3C_PP_TIMING, calc_pp_timing(core_clk, pp_freq));
    write(I2C_FM_TIMING, calc_i2c_fm_timing(core_clk));
    write(I2C_FMP_TIMING, calc_i2c_fmp_timing(core_clk));
    write(BUS_FREE_TIMING, calc_bus_free_available(core_clk));
    write(BUS_IDLE_TIMING, calc_bus_idle(core_clk));
    write(SDA_HOLD_SWITCH_DLY_TIMING, sda_hold_cfg);

    /* 5. Self dynamic address */
    write(DEVICE_ADDR,
          FIELD_PREP(DYNAMIC_ADDR, ctrl_da) |
          DYNAMIC_ADDR_VALID);

    /* 6. FIFO/Queue threshold */
    write(QUEUE_THLD_CTRL,
          FIELD_PREP(RESP_BUF_THLD, resp_thld) |
          FIELD_PREP(IBI_STATUS_THLD, 0) |
          FIELD_PREP(IBI_DATA_THLD, ibi_data_thld));

    write(DATA_BUFFER_THLD_CTRL,
          FIELD_PREP(TX_EMPTY_BUF_THLD, tx_empty_thld) |
          FIELD_PREP(RX_BUF_THLD, rx_thld) |
          FIELD_PREP(TX_START_THLD, tx_start_thld) |
          FIELD_PREP(RX_START_THLD, rx_start_thld));

    /* 7. Optional DMA */
    if (use_dma)
        setup_dma_handshake_and_dmac();

    /* 8. Program DAT for known devices and DAA pool */
    dat_base = read(DEVICE_ADDR_TABLE_POINTER);

    for_each_static_i3c_target(t)
        write(DAT(t.index), make_setdasa_dat(t.static_addr, t.dynamic_addr));

    for_each_entdaa_slot(s)
        write(DAT(s.index), make_entdaa_dat(s.dynamic_addr));

    for_each_legacy_i2c_target(i2c)
        write(DAT(i2c.index), make_i2c_dat(i2c.static_addr));

    /* 9. Enable interrupts */
    write(INTR_STATUS, 0xffffffff);
    write(INTR_STATUS_EN, wanted_status_mask);
    write(INTR_SIGNAL_EN, wanted_signal_mask);

    /* 10. Enable controller */
    set_bits(DEVICE_CTRL, ENABLE);

    /* 11. DAA */
    if (has_static_i3c_targets)
        issue_addr_assign_cmd(CCC_SETDASA, first_static_index, static_count);

    if (use_setaasa)
        issue_broadcast_ccc_write(CCC_SETAASA, no_payload);

    if (has_dynamic_i3c_targets)
        issue_addr_assign_cmd(CCC_ENTDAA, first_entdaa_index, entdaa_count);

    /* 12. Read response and DCT */
    drain_response_queue();
    read_dct_and_build_device_list();

    /* 13. Verify and enumerate */
    for_each_i3c_target(dev) {
        dev->pid = ccc_getpid(dev);
        dev->bcr = ccc_getbcr(dev);
        dev->dcr = ccc_getdcr(dev);
        dev->status = ccc_getstatus(dev);
        dev->mxds = ccc_getmxds(dev);

        update_dat_from_bcr(dev);
    }

    /* 14. Configure IBI/HJ/MR/SIR */
    program_ibi_response_policy();
    issue_enec_or_disec();

    return 0;
}
```

---

# 19. 最关键的寄存器依赖关系

你最后要把这些关系记牢：

```text
DEVICE_ADDR[DYNAMIC_ADDR_VALID]
  决定 DWC 是否有 Active Controller 自身地址

DEVICE_CTRL[ENABLE]
  决定 Controller 是否开始工作

DAT
  决定 DEV_INDX 最终访问哪个 Target

DCT
  是 ENTDAA 的结果表，由 Controller 填

COMMAND_QUEUE_PORT
  软件下命令入口

RESPONSE_QUEUE_PORT
  软件判断命令结果入口

RX_TX_DATA_PORT
  PIO 数据入口/出口

QUEUE_THLD_CTRL / DATA_BUFFER_THLD_CTRL
  决定 interrupt / DMA / FIFO 水位行为

INTR_STATUS / INTR_STATUS_EN / INTR_SIGNAL_EN
  决定软件何时被通知

DEVICE_CTRL[RESUME]
  出错 halt 后恢复 Controller

DEVICE_CTRL[ABORT]
  主动终止正在进行的 transfer
```

一句话：**DWC I3C Controller 的初始化不是单纯 enable，而是先把 timing、self dynamic address、DAT、queue/FIFO/interrupt 准备好，再 enable，最后通过 Command Queue 发 DAA 和 CCC，把 DCT/DAT/软件 device list 建起来。**