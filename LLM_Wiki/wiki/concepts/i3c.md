---
date: 2026-06-09
tags: [i3c, mipi, bus, protocol]
type: concept
status: active
---

# I3C — Improved Inter-Integrated Circuit

I3C（Improved Inter-Integrated Circuit）是 MIPI 联盟制定的下一代串行总线协议，是 I2C 的演进替代方案，兼容 I2C 从设备，同时提供显著更高的性能和更丰富的功能。

## Details

### 概述
MIPI I3C 由 MIPI 联盟开发，v1.0 于 2016 年发布，v1.1.1 于 2021 年发布。设计目标是在保持 I2C 两线制（SCL + SDA）简单性的同时，大幅提升速度并增加带内中断、热插拔等新能力。

**术语演进**：早期文档使用 Master/Slave，新规范已改为 Controller/Target。本文档两者均可能出现，含义对应：Controller = Master，Target = Slave。

### 总线架构

I3C 总线同一时刻只有一个 Active Controller，但允许多个 Secondary Controller：

```text
I3C Bus
 ├── Active Controller      当前控制总线的设备（唯一驱动 SCL）
 ├── I3C Target             普通目标设备
 ├── Secondary Controller   平时作为 Target，可请求接管总线
 └── Legacy I2C Target      可选共存的传统 I2C 设备
```

| 角色 | 含义 |
|------|------|
| **Primary Controller** | 初始化 I3C Bus，成为第一个 Active Controller |
| **Secondary Controller** | 具备 Controller 能力但初始作为 Target，可通过 CRR 请求接管 |
| **Active Controller** | 当前真正控制总线的设备（运行时状态，非固定角色） |
| **I3C Target** | 支持 SDR，至少支持一种动态地址分配方式 |
| **SDR-Only Target** | 只支持 SDR，不支持 HDR |
| **I2C Target** | 传统 I2C 设备，不具备 I3C 能力 |

### 帧结构（Frame）

所有 I3C 通信以帧（Frame）为单位：

```text
START → HEADER → DATA → STOP
```

- **START (S)**：SCL 为高时 SDA 从高跳低
- **HEADER**：7-bit 地址 + RnW + ACK/NACK
  - START 后的 Header **可仲裁**（Open-Drain，允许 Target 发起 IBI/Hot-Join/CRR）
  - Repeated START 后的 Header **不仲裁**（Push-Pull 驱动，仅 ACK/NACK 为 Open-Drain）
- **DATA**：9-bit word（8-bit 数据 + T-bit），第 9 bit 语义与 I2C 完全不同
- **STOP (P)**：SCL 为高时 SDA 从低跳高

### 广播地址 7'h7E

`7'h7E` 是 I3C 广播地址，所有 I3C Target 必须识别。用途：
- **Broadcast CCC**：发给所有 Target 的命令（0x00~0x7F）
- **模式入口**：进入 HDR 模式前先发 7'h7E
- **DAA 入口**：ENTDAA 后跟 7'h7E + R 进入动态地址分配

> I2C 保留 7'h7E 未使用，传统 I2C 设备不会匹配此地址。

### 与 I2C 的关键差异

| 特性 | I2C | I3C |
|------|-----|-----|
| 最大速度 | 3.4 MHz (HS-mode) | 12.5 MHz (SDR) / 25+ MHz (HDR) |
| 中断 | 需额外引脚 | 带内中断（IBI），无需额外引脚 |
| 动态地址 | 否 | 是（动态地址分配 DAA） |
| 热插拔 | 否 | 是（Hot-Join） |
| 多主 | 受限 | 原生支持 |
| 第 9 bit | ACK/NACK | T-bit（写=奇校验，读=结束/继续） |
| I2C 兼容 | — | 兼容 I2C 从设备 |
| 功耗 | 较高 | 更低（推挽驱动，更少信号切换） |

### 协议模式

**SDR（Single Data Rate）模式** — 基础控制平面：
- 总线配置（BCR/DCR/LVR 特性寄存器）
- 数据传输（Private Write/Read）
- 动态地址分配（ENTDAA/SETDASA/SETAASA）
- 带内中断（IBI）
- 热插拔（Hot-Join）
- 第二主机切换（Controller Role Handoff）
- 通用命令代码（CCC）— 管理命令集
- 错误检测与恢复
- Clock Stalling：Controller 在特定相位拉低 SCL 进行流控

**HDR（High Data Rate）模式** — 高速数据面：
- **HDR-DDR**（双数据率）：每个时钟沿传输数据，18-bit word（16 数据位 + 2 奇偶校验位），前导码 + CRC
- **HDR-TSP**（三进制纯总线）：SCL 和 SDA 均传输数据，12 个三进制符号 → 16 位数据
- **HDR-TSL**（三进制+传统 I2C）：类似 HDR-TSP，额外插入"虚拟"符号确保传统 I2C 设备不误判

### T-bit 详解

SDR 模式下第 9 bit 的语义是 I3C 与 I2C 最大的区别之一：

| 方向 | T-bit 含义 | 驱动方 |
|------|-----------|--------|
| Controller 写数据 | 奇校验位（8 个数据位的 XOR 取反） | Controller |
| Target 读返回 | 1=继续读，0=结束读（Target 主动终止） | Target |
| Target 读返回（Controller 中止）| Controller 可在 T-bit 后发 Repeated START 中断 | Controller |

### Bus Conditions

I3C 定义了三个总线空闲层级，用于 Target 发起请求的时机判断：

| 条件 | 说明 | 典型时间 |
|------|------|---------|
| **Bus Free** | STOP 后最小等待时间，Controller 可再次发起传输 | tCAS / tBUF |
| **Bus Available** | 比 Bus Free 更"空"，Target 可以发 START Request (IBI/CRR) | ≥ tAVAL (1.0μs) |
| **Bus Idle** | 总线长时间空闲，Hot-Join Target 可判断总线稳定并发请求 | ≥ tIDLE (200μs) |

### 动态地址分配（DAA）

I3C 的通信依赖动态地址。三种方式：

| 方法 | 说明 |
|------|------|
| **SETDASA** | 根据已知静态地址设置动态地址 |
| **SETAASA** | 所有有静态地址的 Target 用静态地址作为动态地址 |
| **ENTDAA** | 通用分配流程：Target 用 48-bit Provisional ID + BCR + DCR 仲裁，最低值胜出 |

**动态地址还承载 IBI 优先级**：地址越小优先级越高（地址仲裁时 0 胜 1）。

### 通用命令代码（CCC）

CCC 是 I3C 的管理命令集，永远从 7'h7E + W 开始：

| 分类 | 码值范围 | 作用 |
|------|---------|------|
| **Broadcast CCC** | 0x00~0x7F | 发给所有 I3C Target |
| **Direct CCC** | 0x80~0xFE | 先广播命令码，再定向某个 Target |

**常用 Broadcast CCC**：

| CCC | 码值 | 场景 |
|-----|------|------|
| ENEC/DISEC | 0x00/0x01 | 启用/禁用 IBI、Hot-Join、CRR |
| ENTAS0~3 | 0x02~0x05 | 进入活动状态（电源管理提示） |
| RSTDAA | 0x06 | 重置所有 Target 动态地址 |
| ENTDAA | 0x07 | 进入动态地址分配流程 |
| ENTHDR0~7 | 0x20~0x27 | 进入 HDR 模式 |
| SETAASA | 0x29 | 设置静态地址为动态地址 |
| DEFSLVS | 0x0E | 定义从设备列表（同步第二主机）|

### 总线配置

有三类总线拓扑，影响时序和模式选择：

| 类型 | 说明 |
|------|------|
| **Pure Bus** | 只有 I3C 设备 |
| **Mixed Fast Bus** | I3C + 带 50ns 尖峰滤波器的 I2C 设备 |
| **Mixed Slow/Limited Bus** | I3C + 无合适滤波器的 I2C 设备 |

每个 I3C 设备通过特性寄存器公开能力：
- **BCR**（Bus Characteristics Register）— 设备角色和能力
- **DCR**（Device Characteristics Register）— 设备类型
- **LVR**（Legacy Virtual Register）— 传统 I2C 设备能力（虚拟存在）

### 错误处理

- 奇偶校验（SDR 模式写数据 T-bit）
- CRC 校验（HDR 模式）
- 错误恢复协议
- 超时检测
- 保留地址用于错误检测（7'h3E/5E/6E/76/7A/7C/7F 等与 7'h7E 差一位的地址）

## See Also

- [[mipi-i3c-basic-spec]] — MIPI I3C Basic Specification v1.1.1
- [[2026-06-06-mipi-i3c-introduction]] — NXP I3C 技术介绍摘要
- [[2026-06-06-mipi-i3c-chinese-doc]] — I3C 中文文档摘要
- [[i3c-lecture-notes]] — I3C 讲解笔记合集
- [[i3c-vs-i2c]] — I3C 和 I2C 核心差异对比
- [[i2c]] — I2C 总线（I3C 的前身）


## Implementations

- `i3c_master_do_daa()` — IMPLEMENTS
  - DAA - Dynamic Address Assignment
- `i3c_master_do_daa_ext()` — IMPLEMENTS
  - DAA extended version
- `i3c_master_entdaa_locked()` — IMPLEMENTS
  - ENTDAA CCC command
- `i3c_master_queue_ibi()` — IMPLEMENTS
  - In-Band Interrupt queue
- `i3c_master_send_ccc_cmd_locked()` — IMPLEMENTS
  - CCC command send
- `i3c_master_enec_locked()` — IMPLEMENTS
  - ENEC - enable events
- `i3c_master_disec_locked()` — IMPLEMENTS
  - DISEC - disable events
- `i3c_master_enable_hotjoin()` — IMPLEMENTS
  - Hot-Join enable
- `i3c_master_disable_hotjoin()` — IMPLEMENTS
  - Hot-Join disable
- `i3c_master_get_free_addr()` — IMPLEMENTS
  - Free dynamic address allocation
- `i3c_master_defslvs_locked()` — IMPLEMENTS
  - DEFSLVS CCC
- `i3c_master_register()` — IMPLEMENTS
  - I3C master registration

## Counter-Arguments and Gaps

...
