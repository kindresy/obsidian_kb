---
date: 2026-05-28
tags: [pcie, hardware, protocol]
type: concept
status: active
---

# PCIe 事务层（Transaction Layer）

[[pci-express|PCI Express]] 分层架构的最上层，负责处理 TLP（Transaction Layer Packet）的组装与解析。

## Details

#### TLP 格式
TLP 由四部分组成：TLP Prefix（可选） + TLP Header（3-4 DW） + Data Payload（0-1024 DW） + TLP Digest（可选）

#### TLP Header 字段
- **Fmt/Type**：标识 TLP 类型（MRd、MWr、CfgRd0、Cpl 等），存储器读写通过 Fmt 的"带/不带数据"区分
- **TC（Traffic Class）**：3 位，TC0-TC7，缺省 TC0，与 QoS 和 VC 仲裁相关
- **Attr**：3 位 — [2]=IDO、[1]=Relaxed Ordering、[0]=No Snoop
- **Length**：以 DW 为单位，0=1024DW，使目标可预分配缓冲
- **Transaction ID**：Requester ID（Bus/Dev/Fun）+ Tag。5位（32）/8位扩展Tag（256），Phantom Function 扩展至 2048
- **DW BE**：First DW BE + Last DW BE，字节粒度寻址。Zero-Length 读用于"读刷新"
- **Completion 特有**：Completer ID、Status（SC/UR/CRS/CA）、BCM、Byte Count（12 位）、Lower Address

#### 三种路由方式详解
- **地址路由**：存储器/I/O TLP，通过虚拟 PCI-PCI 桥的 Base/Limit 寄存器正向译码
- **ID 路由**：配置请求、完成报文。Primary/Secondary/Subordinate Bus Number。Type00h（同级）/ Type01h（下游）
- **隐式路由**：Message 报文。Route 字段：RC 路由/广播/本地/PME_TO_Ack

#### ARI（Alternative Routing-ID Interpretation）
- 消除 Device Number（PCIe 点对点，Device Number 必定为 0）
- Function 从 8 个扩展到 256 个（8 位）
- 与 Phantom Function 不同：Phantom 复用 Function Number 位扩展 Tag，Function 数不变

#### 原子操作（PCIe V2.1）
- FetchAdd：加后返回原值
- Swap：交换
- CAS：比较后条件交换
- 支持 32/64/128 位操作数
- 支持 EP-to-EP、EP-to-RC、RC-to-EP

#### TLP Processing Hint（TPH）
- PH[1:0] 字段：双向/请求者/目标者/高优先级目标者
- Steering Tag ST[7:0] 复用 Tag 或 DW BE 字段，ST[15:8] 通过 TPH TLP Prefix 扩展
- 三种 ST 模式：不支持 / 中断向量模式 / 设备自定义

#### 数据负载参数
- **Max_Payload_Size**：协商取最小值，x86 通常 128B，PowerPC 通常 256B
- **Max_Read_Request_Size**：最大 4KB
- **RCB（Read Completion Boundary）**：RC 缺省 64B，其他 128B。完成报文不可跨越 RCB 边界

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express]]
- [[pcie-flow-control]]

## Counter-Arguments and Gaps

...
