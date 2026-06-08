---
date: 2026-06-06
tags: [i3c, mipi, bus, protocol]
type: concept
status: active
---

# I3C — Improved Inter-Integrated Circuit

I3C（Improved Inter-Integrated Circuit）是 MIPI 联盟制定的下一代串行总线协议，是 I2C 的演进替代方案，兼容 I2C 从设备，同时提供显著更高的性能和更丰富的功能。

## Details

### 概述
MIPI I3C 由 MIPI 联盟开发，v1.0 于 2016 年发布，v1.1.1 于 2021 年发布。设计目标是在保持 I2C 两线制（SCL + SDA）简单性的同时，大幅提升速度并增加带内中断、热插拔等新能力。

### 与 I2C 的关键差异

| 特性 | I2C | I3C |
|------|-----|-----|
| 最大速度 | 3.4 MHz (HS-mode) | 12.5 MHz (SDR) / 25+ MHz (HDR) |
| 中断 | 需要额外引脚 | 带内中断（IBI），无需额外引脚 |
| 动态地址 | 否 | 是（动态地址分配） |
| 热插拔 | 否 | 是（Hot-Join） |
| 多主 | 受限 | 原生支持 |
| 功耗 | 较高 | 更低（更少信号切换） |
| I2C 兼容 | — | 兼容 I2C 从设备（FM+ 模式） |

### 协议模式

**SDR（Single Data Rate）模式** — 基本操作模式：
- 总线配置和初始化
- 数据传输（读/写）
- 动态地址分配（DAA）
- 带内中断（IBI）
- 热插拔（Hot-Join）
- 第二主机切换（Secondary Master）
- 通用命令代码（CCC）
- 错误检测与恢复

**HDR（High Data Rate）模式** — 高速模式：
- **HDR-DDR**（双数据率）：每个时钟沿传输数据
- **HDR-TSP**（三进制半精度）：三进制信令，半精度
- **HDR-TSL**（三进制全精度）：三进制信令，全精度

### 总线特性
- 两线制：SCL（时钟）+ SDA（数据）
- 开漏 + 推挽混合驱动
- 地址仲裁（类似 I2C 多主仲裁）
- Provisional ID 用于设备识别

### 通用命令代码（CCC）
CCC 是 I3C 中用于总线管理和配置的命令帧，涵盖：
- 动态地址分配（ENTDAA）
- 总线重置（RSTACT）
- 设备配置（SETMWL、SETMRL 等）
- 模式切换（ENTHDR0-3）

### 错误处理
- 奇偶校验（SDR 模式）
- CRC 校验（HDR 模式）
- 错误恢复协议
- 超时检测

## See Also

- [[mipi-i3c-basic-spec]] — MIPI I3C Basic Specification v1.1.1
- [[2026-06-06-mipi-i3c-introduction]] — NXP I3C 技术介绍摘要
- [[2026-06-06-mipi-i3c-chinese-doc]] — I3C 中文文档摘要
- [[pci-express]] — PCI Express（另一类高速总线，对比参考）
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
