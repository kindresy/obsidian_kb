---
date: 2026-06-06
tags: [i2c, bus, protocol]
type: concept
status: draft
---

# I2C — Inter-Integrated Circuit

I2C（Inter-Integrated Circuit）是 Philips（现 NXP）在 1980 年代开发的两线制串行总线协议，广泛用于连接低速外设。I3C 是其下一代演进。

## Details

### 典型应用示例：AT24C16C EEPROM

AT24C16C 是 16Kbit I2C EEPROM，地址由器件地址（固定部分）和页地址组成：

- 器件地址为 `1010`（固定），后跟 A2/A1/A0 引脚电平
- 16Kbit = 2048 字节，分为 8 页（256 字节/页），寻址需要 A2/A1/A0 + 页选择
- 写操作：先发器件地址 + 字地址，再发数据

## See Also

- [[i3c]] — I3C 总线协议（I2C 的下一代）

## Counter-Arguments and Gaps

...
