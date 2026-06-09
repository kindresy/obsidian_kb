好，Sir。我们就先攻克 **DWC I3C Controller 初始化顺序**。这一节的目标不是背寄存器，而是建立一条清晰主线：

```text
复位/时钟
 → 基础配置
 → Controller 自身动态地址
 → timing / bus config
 → FIFO / interrupt / DMA / queue
 → DAT 预填
 → DAA
 → DCT 校验
 → 能力枚举
 → 事件使能
 → 进入正常 SDR/HDR 传输
```

下面以 **DWC_mipi_i3c_v111_basic 非 HCI Controller mode** 为主讲。HCI PIO mode 以后单独看，因为它的数据结构和普通 non-HCI 不一样，而且 databook 明确说 HCI DMA operation 不支持。

---

# 1. 初始化的本质：先让 IP 成为 Active Controller

MIPI 协议层面说，I3C 总线上任意时刻只能有一个 **Active Controller**。DWC databook 里也说，如果 IP 被配置成 Active Controller，并且 `DEVICE_ADDR[DYNAMIC_ADDR_VALID] = 1`，它在 power-up 后拥有总线，负责驱动 SCL 并发起 SDA 传输。

所以 DWC Controller 初始化第一目标是：

```text
让 DWC IP 具备合法 Controller 身份，并拥有自己的 Dynamic Address。
```

不是一上来就 ENTDAA，也不是一上来就访问 Target。必须先让 Controller 自己状态合法。

---

# 2. 推荐初始化顺序总览

可以先记这个顺序：

```text
Step 0  确认 IP 配置参数和工作模式
Step 1  复位释放，确认 APB/pclk/core_clk 可访问
Step 2  保持 Controller disabled
Step 3  清中断、清队列、清 FIFO
Step 4  配置总线类型和 timing
Step 5  配置 Controller 自身 Dynamic Address
Step 6  配置 FIFO / Queue threshold / DMA / interrupt
Step 7  预填 DAT
Step 8  Enable Controller
Step 9  执行 DAA：SETDASA / SETAASA / ENTDAA
Step 10 读取 DCT / Response Queue，确认 DAA 结果
Step 11 发 GETPID / GETBCR / GETDCR / GETSTATUS 做二次确认
Step 12 配置 Target 能力：MRL/MWL/MXDS/IBI/HDR/PEC 等
Step 13 使能事件：ENEC / DISEC
Step 14 进入正常 SDR / HDR 传输
```

这条线就是把 spec 里的初始化、DAA、CCC、SDR/HDR 管理动作落到 DWC IP 上。

---

# 3. Step 0：先确认 IP build-time 配置

DWC 这个 IP 很多 feature 是 **coreConsultant 生成 RTL 时决定的**，不是软件运行时想开就开。databook 明确提醒，不是每个 feature 在每种配置里都可用，最终要看具体配置。

初始化前，你要先知道这些：

```text
当前 IP 是：
  Controller-only？
  Secondary Controller？
  Dual-role？
  APB Target？
  Target-Lite？
  Autonomous Target？

是否支持：
  IBI with data？
  DMA handshake？
  HDR-DDR？
  HDR-BT？
  PEC？
  Target Reset？
  Group Address？
  Virtual Target？
  HCI mode？
```

尤其注意几个限制：

```text
HCI mode 不支持 DMA operation
HDR-TSP / HDR-TSL 不支持
HDR mode CCC transmission 不支持
Multi-Lane 不支持
HDR-BT Target sourcing SCL for read 不支持
```

这会决定后面驱动路径。

---

# 4. Step 1/2：复位后保持 Controller disabled

初始化阶段建议先不要 enable Controller。原因是：enable 后 IP 可能开始响应 IBI、执行 command queue 或驱动总线。正确做法是：

```text
1. 释放 reset
2. 确认 pclk / core_clk / dma_clk 等时钟稳定
3. APB 寄存器可访问
4. 保持 DEVICE_CTRL[ENABLE] = 0
```

如果之前 Controller 运行过，要先 flush/drain：

```text
Command Queue
Response Queue
Tx FIFO
Rx FIFO
IBI Status Queue
IBI Data Queue
```

databook 在 disable/abort 章节也提到，重新 enable 前软件预期要 flush/drain 队列和 FIFO。

---

# 5. Step 3：清中断、配置 interrupt mask

DWC 是一个 combined interrupt pin，内部各类中断源 OR 到一个 interrupt 输出。软件要关注三类寄存器：

```text
INTR_STATUS      当前中断状态
INTR_STATUS_EN   是否允许该状态置位/触发状态逻辑
INTR_SIGNAL_EN   是否允许该状态真正拉高外部 interrupt pin
```

初始化时推荐：

```text
1. 清掉历史 INTR_STATUS
2. 配置 INTR_STATUS_EN
3. 配置 INTR_SIGNAL_EN
4. 配置各 queue threshold
```

特别是 IBI，databook 建议 IBI status threshold 设低一点，让第一个 IBI status 进队列后就能尽快通知软件，避免 IBI queue 满导致 clock stall 或 NACK。

---

# 6. Step 4：配置 bus type 和 timing

这一步是硬件 bring-up 的关键。

DWC 支持三类总线配置：

```text
Pure Bus
Mixed Fast Bus
Mixed Slow/Limited Bus
```

它们和 MIPI spec 对应。Pure Bus 只有 I3C 设备；Mixed Fast Bus 有 Legacy I2C Target，但这些 I2C Target 需要有 50 ns spike filter；Mixed Slow/Limited Bus 则是 I2C Target 没有可靠 50 ns spike filter 或未知。

这一步要配置：

```text
I3C Open-Drain timing
I3C Push-Pull timing
I2C FM/FM+ timing
Bus Free timing
Bus Available timing
Bus Idle timing
SDA hold / switch delay
HDR-DDR / HDR-BT timing，如果支持
```

为什么 timing 要在 DAA 前配？因为 DAA 本身就需要跑总线，尤其 ENTDAA 是 Open-Drain 流程。DWC databook 明确说完整 ENTDAA 从 START 到 STOP 都是在 I3C Open-Drain mode 下生成的。

这里要记住一个实战判断：

```text
如果板上有 Legacy I2C，并且不确定有没有 50ns spike filter，
先按 Mixed Slow/Limited Bus 处理，不要贸然上高 I3C speed。
```

---

# 7. Step 5：配置 Controller 自身 Dynamic Address

这是 DWC 初始化很容易忽略的一步。

MIPI spec 里 Primary Controller 也需要一个 Dynamic Address。DWC databook 说，host application 要在 `DEVICE_ADDR` 里配置 Controller 自己的 `DYNAMIC_ADDR`，并设置 `DYNAMIC_ADDR_VALID`。

概念上是：

```text
DEVICE_ADDR[DYNAMIC_ADDR]       = controller_da
DEVICE_ADDR[DYNAMIC_ADDR_VALID] = 1
```

注意：

```text
这个地址不是分给 Target 的；
这是 Active Controller 自己的 Dynamic Address。
```

不要用 reserved address：

```text
7'h00
7'h01
7'h02
7'h7E
7'h7F
```

也不要用那些和 `7'h7E` 单 bit error 相关的禁止地址。MIPI spec 里地址限制表对此有明确要求。

---

# 8. Step 6：配置 FIFO / Queue / DMA threshold

DWC 架构里有：

```text
Command Queue
Response Queue
Tx FIFO
Rx FIFO
IBI Status Queue
IBI Data Queue
DAT
DCT
```

这些都位于 SPRAM 或队列/FIFO 结构中。databook 还给了 buffer depth 选择建议，例如 Tx/Rx buffer 可配置为 8 locations = 32 bytes，或者 64 locations = 256 bytes；Command Queue、Response Queue、DAT、DCT、IBI Queue 也都要根据系统目标数和并发度选深度。

如果启用 external DMA handshake，要注意：

```text
DMA 只用于 Tx/Rx FIFO 数据搬运；
不用于取 I3C command；
不用于把 response 自动写回内存。
```

也就是说：

```text
Command / Response 仍由软件通过 APB 管；
Payload data 可以通过 DMA 搬。
```

这点非常重要。databook 明确说 SDMA transfer 只用于 transmit 和 receive data，不用于 fetching I3C command，也不用于 posting response。

---

# 9. Step 7：预填 DAT，准备地址分配

DAT 是 **Device Address Table**。DWC 的 `DEV_INDX` 字段不是直接填地址，而是填 DAT index。后续命令通过 `DEV_INDX` 找到 Target 地址。

不同 DAA 方法，DAT 预填内容不同。

## 9.1 SETDASA：已知 Static Address 的 I3C Target

适合这种设备：

```text
有 I2C static address
但也是 I3C-capable target
```

DAT 里要放：

```text
Static Address
准备分配的 Dynamic Address
```

然后通过 Address Assignment Command 发 SETDASA。DWC 会生成：

```text
S/Sr
7'h7E + W + ACK
SETDASA CCC
Sr
Static Address + W
Assigned Dynamic Address
P/Sr
```

databook 说 SETDASA 比 ENTDAA 更快，适合 cold power-up 后先处理已知 static address 的 I3C 设备。

---

## 9.2 ENTDAA：未知 Dynamic Address 的 I3C Target

适合真正 I3C 设备：

```text
没有 static address
需要通过 PID/BCR/DCR 仲裁拿地址
```

DAT 里要提前放准备分配出去的：

```text
Dynamic Address
Dynamic Address Parity
```

DWC databook 特别提醒：**application 必须自己计算 Dynamic Address parity，然后填到 DAT location。**

ENTDAA 后，DWC 会把 winning device 的：

```text
PID[47:0]
BCR
DCR
Assigned Dynamic Address
```

捕获到 DCT，也就是 Device Characteristics Table。

---

## 9.3 SETAASA：让 Static Address 直接变成 Dynamic Address

这个最快，但只适合有 static address 的 I3C Target。

DWC databook 说，成功发出 SETAASA 后，application 应该更新 DAT，让 DAT 里的 Dynamic Address 等于 Static Address。

这个在 JEDEC Sideband / SPD Hub 相关场景比较常见。

---

# 10. Step 8：Enable Controller

DAT、timing、interrupt、self DA 都准备好之后，再 enable Controller：

```text
DEVICE_CTRL[ENABLE] = 1
```

此时 DWC 才开始真正作为 Controller 工作：

```text
驱动 SCL
发 START / STOP
执行 Command Queue
接收 IBI
执行 DAA
```

databook 对 Controller role 的描述是：Active Controller 负责发 clock、发 command、控制 data transfer，Dynamic Address Assignment 也由 Controller 完成。

---

# 11. Step 9：执行 DAA

推荐顺序：

```text
1. SETDASA：先处理已知 static address 的 I3C Target
2. SETAASA：如果系统策略允许，把 static address 直接作为 dynamic address
3. ENTDAA：处理剩余未分配地址的 I3C Target
```

实际不是三个都必须用。典型情况：

```text
纯 I3C sensor：
  ENTDAA

I3C-capable 且有 static address：
  SETDASA 或 SETAASA

Legacy I2C target：
  不参与 DAA，直接用 static address
```

ENTDAA 结束条件包括：

```text
0x7E/W NACK：没有 I3C 设备
0x7E/R NACK：所有设备都已经获得 Dynamic Address
Assigned Dynamic Address NACK：Target 检测到地址错误
DEVICE_COUNT 到 0：本次命令分配数量达到上限
```

DWC databook 还说，DWC Controller 总是用 STOP 结束 ENTDAA，所以建议 ENTDAA command 设置 `TOC=1`。

---

# 12. Step 10：读 Response Queue 和 DCT

DAA 不是发完就完了。软件必须检查结果。

DWC 每个 command 完成后会写 Response Queue。对 DAA 来说，Response 里的信息能告诉你：

```text
是否成功
是否 NACK
剩余 device count
错误状态
```

ENTDAA 期间，DWC 会把每个 winning device 的特征信息放进 DCT：

```text
PID
BCR
DCR
Assigned Dynamic Address
```

databook 建议 DAA 后发 GETBCR / GETDCR / GETPID 到对应 Dynamic Address，再和 DCT 里的捕获值比较，确认确实是同一个设备响应。

这一步在硅后非常实用：

```text
如果 DAA 成功但后续 private read NACK，
先查 DCT 里有没有这个设备；
再查 GETBCR/GETDCR 是否能读回；
最后查 DAT 中 dynamic address 是否更新正确。
```

---

# 13. Step 11：能力枚举和参数配置

DAA 后，Controller 知道了设备地址，但还不知道每个设备能跑多快、支持哪些事件、是否支持 payload、是否支持 HDR。

需要发一组 Direct CCC：

```text
GETPID
GETBCR
GETDCR
GETSTATUS
GETMXDS
GETCAPS
GETMRL / GETMWL
SETMRL / SETMWL
```

目的如下：

|CCC|作用|
|---|---|
|GETBCR|判断 Target role、IBI capability、IBI payload、HDR/advanced capability 等|
|GETDCR|判断设备类型|
|GETPID|获取唯一 ID|
|GETSTATUS|获取设备状态|
|GETMXDS|获取最大数据速率/turnaround 能力|
|GETCAPS|获取高级能力，例如 HDR、virtual target 等|
|GETMRL/SETMRL|读最大长度、IBI payload 相关|
|GETMWL/SETMWL|写最大长度|

对 DAT 的后续配置也依赖这些信息。例如：

```text
如果 BCR[2] = 1，说明 IBI 带 MDB/payload；
才应该设置 DAT[IBI_PAYLOAD]。
```

databook 明确提醒，application 只有在 Target 支持 mandatory byte，也就是 BCR[2]=1 时，才应设置 IBI Payload Control；如果 BCR[2]=0，不应设置。

---

# 14. Step 12：配置 IBI / Hot-Join / Controller Request

I3C 的事件包括：

```text
SIR = Target Interrupt Request
HJ  = Hot-Join Request
MR  = Controller Ownership Request
```

DWC Controller 侧有 response control：

```text
SIR Response Control
HJ Response Control
MR Response Control
Reject Notify Control
IBI Payload Control
IBI Data Chunk Size
```

简化理解：

```text
ACK：接受事件
NACK：拒绝事件
NACK + DISEC：拒绝并自动发 disable event CCC
Notify：即使拒绝，也通知 application
```

databook 里说，SIR/HJ/MR 都通过 IBI 机制进入 Controller；Controller 收到后会根据配置 ACK/NACK，并把结果写 IBI Queue。

一般初始化后可以做：

```text
1. 对支持 IBI 的 Target 配 DAT[SIR_REJECT] 或 IBI_SIR_REQ_REJECT
2. 对支持 Controller Request 的 Secondary Controller 配 MR response
3. 对 Hot-Join 配 HOT_JOIN_CTRL
4. 发 ENEC 开启目标事件
5. 配 IBI queue threshold
```

---

# 15. Step 13：进入正常传输

此时才进入日常业务：

```text
SDR Private Write
SDR Private Read
Broadcast CCC
Directed CCC
I2C Private Transfer
HDR-DDR
HDR-BT
IBI handling
Hot-Join handling
```

DWC 的普通 transfer 主要靠 Command Data Structure：

```text
Transfer Argument 或 Short Data Argument
        ↓
Transfer Command
        ↓
Tx/Rx FIFO data
        ↓
Response Queue
```

其中几个字段非常关键：

```text
DEV_INDX  指向 DAT
SPEED     SDR0~SDR4 / HDR-DDR / I2C FM 等
RnW       读写方向
CP        是否带 CCC/HDR command
CMD       CCC code 或 HDR command code
TOC       完成后 STOP 还是 RESTART
ROC       是否生成 response
```

databook 还建议：读命令 `ROC=1`，因为如果 Target 提前结束读，Response 的 `DATA_LENGTH` 能告诉软件实际接收了多少数据。这个和我们前面讲的 I3C SDR Read T-bit 完全对应。

---

# 16. 一份更像驱动的伪代码

```c
/* 0. reset released, clocks stable */

/* 1. keep controller disabled */
dw_i3c_clr_bits(DEVICE_CTRL, ENABLE);

/* 2. clear stale states */
dw_i3c_clear_interrupts();
dw_i3c_flush_cmd_queue();
dw_i3c_flush_resp_queue();
dw_i3c_flush_tx_fifo();
dw_i3c_flush_rx_fifo();
dw_i3c_flush_ibi_queues();

/* 3. program timing */
dw_i3c_program_od_timing();        // for 7'h7E, DAA, arbitration
dw_i3c_program_pp_timing();        // for SDR push-pull data
dw_i3c_program_i2c_timing();       // if legacy I2C exists
dw_i3c_program_bus_free_avail();   // tCAS / tAVAL / tIDLE equivalent

/* 4. program controller self dynamic address */
dw_i3c_write(DEVICE_ADDR,
             FIELD_DYNAMIC_ADDR(controller_da) |
             DYNAMIC_ADDR_VALID);

/* 5. configure thresholds and interrupts */
dw_i3c_program_fifo_thresholds();
dw_i3c_program_ibi_thresholds();
dw_i3c_enable_interrupt_sources();

/* 6. optional DMA handshake */
dw_i3c_config_dma_handshake();

/* 7. prepare DAT */
for each known_static_i3c_target:
    dat[i] = static_addr + assigned_dynamic_addr;

for each entdaa_candidate:
    dat[j] = assigned_dynamic_addr + parity;

/* 8. enable controller */
dw_i3c_set_bits(DEVICE_CTRL, ENABLE);

/* 9. address assignment */
if (has_static_i3c_targets)
    dw_i3c_issue_setdasa();

if (use_setaasa_policy)
    dw_i3c_issue_setaasa();

dw_i3c_issue_entdaa_with_toc_1();

/* 10. check response and DCT */
dw_i3c_read_response_queue();
dw_i3c_read_dct();

/* 11. verify devices */
for each assigned_target:
    getpid = dw_i3c_ccc_getpid();
    getbcr = dw_i3c_ccc_getbcr();
    getdcr = dw_i3c_ccc_getdcr();
    compare_with_dct();

/* 12. configure capabilities */
for each target:
    dw_i3c_ccc_getstatus();
    dw_i3c_ccc_getmxds();
    dw_i3c_ccc_getcaps();
    dw_i3c_ccc_setmrl_setmwl_if_needed();

/* 13. enable events */
dw_i3c_config_ibi_response_policy();
dw_i3c_ccc_enec_for_sir_hj_mr();

/* 14. normal operation */
dw_i3c_private_read_write();
dw_i3c_hdr_ddr_or_hdr_bt_if_supported();
```

---

# 17. 最容易出问题的点

## 1. 忘记配置 Controller 自身 Dynamic Address

现象：

```text
Controller enable 后不真正拥有总线
DAA 不工作
Secondary Controller / Active Controller 状态异常
```

## 2. ENTDAA 前 DAT 没填 Dynamic Address parity

databook 明确要求 application 计算 parity。没填对，Target 可能 NACK assigned dynamic address。

## 3. DAA 后没有更新 DAT

尤其 SETAASA / SETNEWDA 后，软件必须让 DAT 里的 Dynamic Address 反映最新地址。databook 对 SETAASA 和 SETNEWDA 都特别提醒了这一点。

## 4. Mixed Bus timing 配错

如果板上 Legacy I2C 没有 50ns spike filter，却按 Mixed Fast/Pure Bus 跑高速，I2C 设备可能误识别 I3C traffic。

## 5. IBI Payload Control 配错

BCR[2]=0 的 Target，不要设置 IBI payload receive。否则 Controller 可能继续 clock 等 payload，协议就错了。

## 6. 读命令不设 ROC

I3C Target 可以通过 T-bit 提前结束 read。如果 `ROC=0`，软件可能拿不到准确 actual length。databook 建议 read command 设置 `ROC=1`。

---

# 18. 本节先记住的主线

```text
DWC I3C Controller 初始化不是简单 enable。

它必须先完成：
  1. 自身 Controller 动态地址
  2. 总线 timing
  3. FIFO/queue/interrupt/DMA
  4. DAT 预填
  5. DAA
  6. DCT/CCC 验证
  7. 事件能力配置

然后才进入 SDR/HDR 正常业务传输。
```

下一步我们最适合继续看 **ENTDAA 在 DWC IP 中怎么落地**：DAT 具体怎么填、Address Assignment Command 怎么下、DCT 怎么解析、Response Queue 怎么判断成功。