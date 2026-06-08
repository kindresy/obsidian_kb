---
date: 2026-05-29
tags: [avsbus, power, hardware, protocol]
type: concept
status: active
---

# AVSBus — Adaptive Voltage Scaling Bus

AVSBus（Adaptive Voltage Scaling Bus）是 Synopsys 定义的一种串行总线协议，用于 SoC 与外部电源管理 IC（PMIC）之间的通信。它支持动态电压调节（AVS），在高性能 SoC（含 PCIe retimer、CPU/GPU 集群）中用于优化功耗与性能的平衡。

## Details

### 物理层

3-wire 串行接口：

| 信号 | 方向 | 说明 |
|------|------|------|
| AVS_Clock | Controller → Target | 串行时钟，由控制器产生 |
| AVS_CDATA | Controller → Target | 控制器发往目标的数据（命令/数据） |
| AVS_TDATA | Target → Controller | 目标发往控制器的数据（状态/应答） |

### 帧结构

**写帧：** Start Code → Command → Command Data → Select → Command Data → CRC → Target ACK → Status Response

**读帧：** Start Code → Command → Command Data → Select → Target ACK → Status Response → Reserved

### 命令数据类型

| 类型 | 值 | 说明 |
|------|-----|------|
| Voltage Read/Write | 0x0 | 电压读写 |
| Vout Transition Rate | 0x1 | 输出电压转换速率 |
| Current Read | 0x2 | 电流读取 |
| Temperature Read | 0x3 | 温度读取 |
| Voltage Reset | 0x4 | 电压复位 |
| Power Mode Read/Write | 0x5 | 电源模式读写 |
| AVSBus Status | 0xE | 总线状态读写 |
| Manufacturer-Specific | 0xF | 厂商自定义 |

### CRC 校验

- 生成多项式：CRC(x) = x^0 + x^1 + x^3
- 支持可选的 CRC 校验使能和 CRC 错误暂停功能

### 应用场景

- PCIe retimer 的电压调节
- SoC CPU/GPU 集群的动态电压频率调整（DVFS）
- 服务器平台的电源管理

## See Also

- [[dwc-avsbus-databook]] — Synopsys AVSBus 控制器数据手册
- [[pcie-switch-firmware-storage]] — PCIe 设备电源管理相关
- [[pci-express]] — PCIe 总线（retimer 常使用 AVSBus）

## Counter-Arguments and Gaps

- AVSBus 是 Synopsys 专有协议，与 PMBus 有竞争关系，缺少统一标准
- 公开资料有限，主要依赖 Synopsys IP 文档
