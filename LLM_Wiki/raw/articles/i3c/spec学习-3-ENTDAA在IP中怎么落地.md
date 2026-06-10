---
date: 2026-06-10
source-type: note
title: "ENTDAA 在 DWC IP 中的实现"
tags: ["i3c", "bus-protocol"]
compiled: false
---

ENTDAA 在 DWC IP 中怎么落地？
DAT 预填什么？DCT 捕获什么？Response Queue 怎么判断成功？

|缩写|全称|中文含义|在 DWC I3C IP 里的作用|
|---|---|---|---|
|**ENTDAA**|**Enter Dynamic Address Assignment**|进入动态地址分配|一个 Broadcast CCC，用来启动 I3C 动态地址分配流程|
|**DAT**|**Device Address Table**|设备地址表|软件预填/维护，用来告诉 DWC Controller 每个 `DEV_INDEX` 对应哪个 Target 地址和控制属性|
|**DCT**|**Device Characteristics Table**|设备特征表|ENTDAA 过程中由硬件捕获，用来保存 Target 的 `PID / BCR / DCR / Dynamic Address`|

Sir，**ENTDAA 在 DWC IP 中的落地方式**可以一句话概括：

```text
软件预先在 DAT 里放一组“准备分配出去的动态地址”
  ↓
软件向 Command Queue 写一条 Address Assignment Command，CMD=ENTDAA
  ↓
DWC 硬件自动生成完整 ENTDAA 总线波形
  ↓
硬件把每个赢得仲裁的 Target 的 PID/BCR/DCR/DA 写入 DCT
  ↓
硬件在 Response Queue 写入完成状态
  ↓
软件读 Response + DCT，建立自己的 device list，并回填/修正 DAT
```

这部分主要对应 Databook 的 **3.2.2.2 ENTDAA Procedure、3.6.1.4 Address Assignment Command、3.6.2 Response Data Structure、3.6.4 DAT、3.6.5 DCT**。

---

# 1. ENTDAA 在 DWC 中不是软件 bit-bang

你不需要手动发：

```text
S
7'h7E + W
ENTDAA
Sr
7'h7E + R
PID/BCR/DCR
Assigned DA
...
P
```

DWC 的做法是：**软件只下发一条 Address Assignment Command**，硬件自动完成整个 ENTDAA 过程。

DWC 在线上自动做这些事：

```text
1. 发送 7'h7E + W
2. 发送 ENTDAA CCC
3. 发送 7'h7E + R
4. 接收 winning Target 返回的 48-bit PID + BCR + DCR
5. 从 DAT 中取一个 Dynamic Address 分配给该 Target
6. 捕获该 Target 信息到 DCT
7. 继续下一轮 7'h7E + R
8. 直到没有 Target ACK，或 DEVICE_COUNT 用完，或出错
9. STOP 结束
```

Databook 还特别说明：**完整 ENTDAA 从 START 到 STOP 都在 I3C Open-Drain mode 下生成**，并且 DWC Controller 总是用 STOP 结束 ENTDAA，所以建议 Address Assignment Command 里的 `TOC=1`。

---

# 2. ENTDAA 前 DAT 预填什么？

对 ENTDAA 来说，DAT 不是填“已经发现的设备”，而是填：

```text
准备分配给未来 winning Target 的 Dynamic Address 池
```

DWC 的规则是：

```text
第 1 个赢得 ENTDAA 仲裁的 Target → 拿 DAT[DEV_INDEX + 0] 里的 Dynamic Address
第 2 个赢得 ENTDAA 仲裁的 Target → 拿 DAT[DEV_INDEX + 1] 里的 Dynamic Address
第 3 个赢得 ENTDAA 仲裁的 Target → 拿 DAT[DEV_INDEX + 2] 里的 Dynamic Address
...
```

所以 ENTDAA 前你至少要预填这些字段：

```text
DAT entry for pure I3C dynamic device:

bit[31]    DEVICE = 0
bit[23]    DYN = Dynamic Address Parity
bit[22:16] Dynamic Address
bit[15]    TS，可先置 0
bit[14]    MR_REJECT，可先置 1 或按策略
bit[13]    SIR_REJECT，可先置 1 或按策略
bit[12]    IBI_PAYLOAD，通常 ENTDAA 前先置 0，等读到 BCR[2] 后再更新
bit[11]    PEC，按是否启用 PEC 设置
bit[6:0]   Static Address，纯 dynamic I3C device 通常无意义，可置 0
```

最关键的是：

```text
DYN = ~XOR(DynamicAddress[6:0])
```

也就是 7-bit Dynamic Address 的奇校验位。Databook 明确说：**application 必须在填 DAT location 之前自己计算 Dynamic Address parity**。

示意代码：

```c
static u8 dyn_addr_parity(u8 da)
{
    da &= 0x7f;
    return !__builtin_parity(da);  // ~XOR(DA[6:0])
}

u32 make_entdaa_dat(u8 da)
{
    u32 dat = 0;

    dat |= 0u << 31;                         // I3C device, not legacy I2C
    dat |= dyn_addr_parity(da) << 23;        // DYN parity
    dat |= (da & 0x7f) << 16;                // Dynamic Address

    /*
     * These can be updated after DCT/GETBCR:
     * TS, MR_REJECT, SIR_REJECT, IBI_PAYLOAD, PEC
     */

    return dat;
}
```

地址池要避开 reserved address，例如：

```text
7'h00
7'h01
7'h02  // Hot-Join Address
7'h7E  // Broadcast Address
7'h7F
```

也要避开和 `7'h7E` 单 bit 错误相关的禁用地址。

---

# 3. Address Assignment Command 怎么下？

ENTDAA 使用 **Address Assignment Command Data Structure**，不是普通 Transfer Command。

关键字段是：

|字段|配置|
|---|---|
|`CMD_ATTR`|`3`，表示 Address Assignment Command|
|`TID`|软件自定义 transaction id，Response 会带回来|
|`CMD`|`ENTDAA` CCC|
|`DEV_INDX`|DAT 起始 index|
|`DEV_COUNT`|本次最多分配几个设备|
|`ROC`|建议 `1`，需要 Response|
|`TOC`|建议 `1`，DWC ENTDAA 总是 STOP 结束|

Databook 还限制：一条 Address Assignment Command 最多给 **31 个设备**分配动态地址；如果要处理第 32 个，需要再发一条 Address Assignment Command。并且 `DEVICE_COUNT` 不能超过 `IC_DEV_CHAR_TABLE_BUF_DEPTH / 4`，同时 `DEVICE_COUNT + DEV_INDEX` 不能越过 DAT 深度。

示意代码：

```c
u32 cmd = 0;

cmd |= FIELD_PREP(CMD_ATTR, 3);             // Address Assignment Command
cmd |= FIELD_PREP(TID, tid);
cmd |= FIELD_PREP(CMD, CCC_ENTDAA);
cmd |= FIELD_PREP(DEV_INDX, first_dat_idx);
cmd |= FIELD_PREP(DEV_COUNT, dev_count);
cmd |= BIT(26);                             // ROC = 1
cmd |= BIT(30);                             // TOC = 1

writel(cmd, COMMAND_QUEUE_PORT);
```

实际 bit 宏和寄存器 offset 要以你的 Reference Manual / 生成头文件为准，因为 Databook 从 1.03a 起把完整 Register Descriptions 移到了 Reference Manual。

---

# 4. DWC 硬件执行 ENTDAA 时发生什么？

假设你这样预填：

```text
DEV_INDEX = 0
DEVICE_COUNT = 4

DAT[0] = DA 0x10 + parity
DAT[1] = DA 0x12 + parity
DAT[2] = DA 0x14 + parity
DAT[3] = DA 0x16 + parity
```

总线上有 3 个未分配地址的 I3C Target。

那么 DWC 执行结果可能是：

```text
第 1 轮 ENTDAA 仲裁 winner → 分配 0x10 → DCT[0] 捕获它的 PID/BCR/DCR/0x10
第 2 轮 ENTDAA 仲裁 winner → 分配 0x12 → DCT[1] 捕获它的 PID/BCR/DCR/0x12
第 3 轮 ENTDAA 仲裁 winner → 分配 0x14 → DCT[2] 捕获它的 PID/BCR/DCR/0x14
第 4 轮 7'h7E + R → 没有 Target ACK → ENTDAA 结束
```

此时：

```text
requested count = 4
actual assigned = 3
remaining count = 1
```

这个 `remaining count` 会体现在 Response Queue 的 `DATA_LENGTH` 字段里。

---

# 5. DCT 捕获什么？

DCT 是 **Device Characteristics Table**。ENTDAA 时，DWC 硬件把每个参与并赢得仲裁的 Target 的信息写进去。

每个 ENTDAA device 占 **4 个 DWORD**：

```text
Base + 0x0:
  PID[47:40]
  PID[39:32]
  PID[31:24]
  PID[23:16]

Base + 0x4:
  RESERVED
  PID[15:8]
  PID[7:0]

Base + 0x8:
  RESERVED
  BCR[7:0]
  DCR[7:0]

Base + 0xC:
  RESERVED
  DA
```

也就是核心捕获：

```text
48-bit PID
8-bit BCR
8-bit DCR
Assigned Dynamic Address
```

Databook 说明，DCT 中 ENTDAA 捕获的信息可以让软件知道：**某个 PID/BCR/DCR 的 Target 被分配到了哪个 Dynamic Address**。同时软件应根据 DCT 捕获的特征回写 DAT，例如 BCR[2] 表示设备是否支持 IBI payload，如果支持，就应反映到对应 DAT entry 的 `IBI_PAYLOAD` bit。

示意解析：

```c
struct i3c_dev_info {
    u64 pid;
    u8  bcr;
    u8  dcr;
    u8  dyn_addr;
};

void read_entdaa_dct(int idx, struct i3c_dev_info *info)
{
    u32 d0 = readl(DCT_BASE + idx * 16 + 0x0);
    u32 d1 = readl(DCT_BASE + idx * 16 + 0x4);
    u32 d2 = readl(DCT_BASE + idx * 16 + 0x8);
    u32 d3 = readl(DCT_BASE + idx * 16 + 0xC);

    info->pid =
        ((u64)((d0 >> 24) & 0xff) << 40) |
        ((u64)((d0 >> 16) & 0xff) << 32) |
        ((u64)((d0 >>  8) & 0xff) << 24) |
        ((u64)((d0 >>  0) & 0xff) << 16) |
        ((u64)((d1 >>  8) & 0xff) <<  8) |
        ((u64)((d1 >>  0) & 0xff) <<  0);

    info->bcr      = (d2 >> 8) & 0xff;
    info->dcr      = (d2 >> 0) & 0xff;
    info->dyn_addr = d3 & 0x7f;
}
```

---

# 6. Response Queue 怎么判断成功？

DWC 的 Response Data Structure 里，关键字段是：

```text
ERR_STS
TID
DATA_LENGTH
```

对 Address Assignment Command 来说：

```text
DATA_LENGTH = remaining device count
```

也就是：

```text
assigned_count = requested_DEVICE_COUNT - response.DATA_LENGTH
```

判断逻辑不能简单写成：

```c
if (DATA_LENGTH != 0)
    fail;
```

这是错的。

因为你可能预留了 8 个 DAT slot，但总线上实际只有 3 个未分配 Target。此时 ENTDAA 正常结束，`DATA_LENGTH = 5` 是合理的，表示还有 5 个地址没有用出去。

更合理的判断是：

```c
rsp = read_response();

if (rsp.tid != tid)
    handle_unexpected_response();

if (rsp.err_sts == 0) {
    assigned = requested_count - rsp.data_length;
    // ENTDAA command 成功完成
    // assigned 个设备的信息在 DCT 里
} else {
    handle_entdaa_error(rsp.err_sts, rsp.data_length);
}
```

常见 `ERR_STS` 要这样看：

|`ERR_STS`|含义|对 ENTDAA 的理解|
|---|---|---|
|`0`|No Error|命令正常完成；看 `DATA_LENGTH` 算实际分配数量|
|`4`|I3C Broadcast Address NACK Error|`7'h7E/W` NACK，可能没有 I3C 设备响应|
|`5`|Address NACK'd|ENTDAA 分配 Dynamic Address 时 Target NACK，通常是错误|
|`2`|Parity Error|PID/BCR/DCR 或相关阶段出现 parity 问题|
|`3`|Frame Error|帧错误|
|`8`|Transfer Terminated|被终止|

Databook 明确说：对 Address Assignment Command，Response 的 `DATA_LENGTH` 表示 remaining device count；如果 DAA 因 NACK 异常终止，也会通过 Response Queue 报告。

---

# 7. 三种“成功/异常”场景举例

## 场景 A：总线上刚好 4 个 Target，请求 4 个地址

```text
DEVICE_COUNT = 4
实际未分配 Target = 4

Response:
  ERR_STS = 0
  DATA_LENGTH = 0

assigned_count = 4 - 0 = 4
```

这是最直观的成功。

---

## 场景 B：请求 8 个地址，但总线上只有 3 个 Target

```text
DEVICE_COUNT = 8
实际未分配 Target = 3

Response:
  ERR_STS = 0
  DATA_LENGTH = 5

assigned_count = 8 - 5 = 3
```

这也是成功。不是错误。

原因是 DWC 发到第 4 轮 `7'h7E + R` 时，没有未分配 Target ACK，于是 ENTDAA 正常结束。

---

## 场景 C：Target NACK 了分配的 Dynamic Address

```text
DEVICE_COUNT = 4

Response:
  ERR_STS = 5
  DATA_LENGTH = remaining count
```

这通常是错误。常见原因：

```text
Dynamic Address parity 填错
Dynamic Address 非法
Dynamic Address 冲突
Target 检测到地址分配阶段错误
总线波形/OD timing 有问题
```

这个时候不要继续把该地址当成可用地址。应该 dump Response、DCT、DAT、波形，并重新执行 DAA 或复位总线状态。

---

# 8. DCT 读完后为什么还要更新 DAT？

ENTDAA 前 DAT 里只有“准备分配的地址”和一些默认控制位。ENTDAA 后，软件才知道每个地址对应的设备能力。

例如 DCT 里读到：

```text
DA  = 0x12
BCR = ...
DCR = ...
PID = ...
```

如果：

```text
BCR[2] = 1
```

表示 Target 支持 IBI Mandatory Data Byte / payload。那么软件应该把对应 DAT entry 的：

```text
IBI_PAYLOAD = 1
```

否则后续 SIR with payload 时，Controller 可能不会正确接收 MDB/payload。

类似地，你还可以根据策略更新：

```text
SIR_REJECT
MR_REJECT
TS
PEC
HDR-DDR flow-control 相关字段
DEV_NACK_RETRY_CNT
```

所以 ENTDAA 后的正确软件动作是：

```text
1. 读 Response Queue
2. 算 assigned_count
3. 读 DCT[0..assigned_count-1]
4. 建立 software device list
5. 根据 PID/BCR/DCR/DA 回写 DAT 控制位
6. 发 GETPID/GETBCR/GETDCR 二次验证
```

---

# 9. 推荐完整软件 flow

```c
int dw_i3c_entdaa(u8 first_dat_idx, u8 *addr_pool, int count)
{
    int i;
    u32 cmd, rsp;
    int remaining, assigned;

    /*
     * 1. 预填 DAT
     */
    for (i = 0; i < count; i++) {
        u8 da = addr_pool[i];
        u32 dat = make_entdaa_dat(da);

        writel(dat, DAT_BASE + (first_dat_idx + i) * 4);
    }

    /*
     * 2. 下发 Address Assignment Command
     */
    cmd = 0;
    cmd |= FIELD_PREP(CMD_ATTR, CMD_ATTR_ADDR_ASSIGN);
    cmd |= FIELD_PREP(TID, tid);
    cmd |= FIELD_PREP(CMD, CCC_ENTDAA);
    cmd |= FIELD_PREP(DEV_INDX, first_dat_idx);
    cmd |= FIELD_PREP(DEV_COUNT, count);
    cmd |= ROC;
    cmd |= TOC;

    writel(cmd, COMMAND_QUEUE_PORT);

    /*
     * 3. 等 Response
     */
    rsp = dw_i3c_wait_response(tid);

    if (RSP_ERR_STS(rsp)) {
        pr_err("ENTDAA failed: err=%u remaining=%u\n",
               RSP_ERR_STS(rsp), RSP_DATA_LENGTH(rsp));
        return -EIO;
    }

    /*
     * 4. 计算实际分配数量
     */
    remaining = RSP_DATA_LENGTH(rsp);
    assigned = count - remaining;

    /*
     * 5. 读取 DCT
     */
    for (i = 0; i < assigned; i++) {
        struct i3c_dev_info info;

        read_entdaa_dct(i, &info);

        /*
         * 6. 建立软件 device list
         */
        add_i3c_device(info.pid, info.bcr, info.dcr, info.dyn_addr);

        /*
         * 7. 根据 BCR/DCR 更新 DAT 控制位
         */
        update_dat_policy_from_bcr(first_dat_idx + i, info.bcr);
    }

    return assigned;
}
```

---

# 10. Debug checklist

如果 ENTDAA 不工作，按这个顺序查：

```text
1. Controller 自己的 DEVICE_ADDR[DYNAMIC_ADDR_VALID] 是否已置 1
2. DEVICE_CTRL[ENABLE] 是否已置 1
3. I3C_OD_TIMING 是否合理，因为 ENTDAA 全程 OD
4. DAT[DEV_INDEX + i] 的 Dynamic Address 是否非法或冲突
5. DAT[DEV_INDEX + i].DYN parity 是否为 ~XOR(DA)
6. DEVICE_COUNT 是否超过 DCT depth / 4
7. DEVICE_COUNT + DEV_INDEX 是否越过 DAT depth
8. Address Assignment Command 是否 CMD_ATTR=3
9. CMD 是否为 ENTDAA，而不是普通 Transfer Command
10. ROC 是否置 1，能否读到 Response
11. ERR_STS 是 4、5、2、3 还是 0
12. DCT 是否有 PID/BCR/DCR/DA 写入
13. 波形上是否看到 7'h7E/W、ENTDAA、7'h7E/R
```

最常见的问题通常是这几个：

```text
DAT parity 没填或填错
使用了 reserved Dynamic Address
DEVICE_COUNT 超过 DCT 能力
ENTDAA 前 Controller 没有 self dynamic address
OD timing / pull-up / IO 模式不对
Response Queue 没有正确 drain，导致误判旧 response
```

一句话总结：**DWC ENTDAA 的核心是“软件预填 DAT 地址池，硬件自动跑 ENTDAA，硬件把发现结果写 DCT，软件通过 Response Queue 判断命令状态并用 DATA_LENGTH 计算实际分配数量”。**