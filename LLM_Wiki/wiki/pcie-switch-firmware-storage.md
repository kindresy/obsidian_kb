---
date: 2026-05-28
tags: [pcie, switch, hardware, firmware]
type: concept
status: promoted
---

# PCIe Switch 固件存储位置

## 答案（源于查询：PCIe 总线的 Switch 固件一般存在哪里？）

PCIe Switch 的固件（Firmware）通常存储在以下位置：

### 1. 外部 SPI NOR Flash（最常见）

绝大多数商用 PCIe Switch（如 Microchip/PLX、Broadcom 等厂商的 Switch）通过外部 SPI Flash 芯片存储固件。上电复位时，Switch 内部硬件逻辑自动将 SPI Flash 中的固件加载到内部 SRAM 或寄存器中完成初始化。

这也是《[[pci-express-体系结构导读]]》在表述"基本PCI/PCIe设备"时所指的一般性做法：PCI/PCIe 设备的配置信息存放于 **E²PROM** 中，设备上电初始化时由硬件自动将 E²PROM 中的信息读到配置空间中作为初始值（参见第2章源码）。对于更复杂的 PCIe Switch 而言，这个过程不仅包括配置空间初始化，还涉及内部路由表、端口配置、VC 设置等完整固件加载。

> 注：书中提到"PCI设备通常将PCI配置信息存放在 E²PROM 中。PCI设备进行上电初始化时，将 E²PROM 中的信息读到PCI设备的配置空间中作为初始值。这个过程由硬件逻辑完成，绝大多数PCI设备使用这种方式初始化其配置空间。"（[[pci-bridge]] 相关章节）

### 2. PCIe 配置空间的 Expansion ROM

部分 Switch 可以支持通过 Expansion ROM 基地址寄存器指向的 ROM 空间存放固件／初始化代码，由系统软件在枚举过程中加载执行。

### 3. 配置引脚（Strapping Pins）

某些 Switch 的 "HwInit" 类型寄存器值由硬件配置引脚决定。如第4章所述："HwInit类型的寄存器...值由芯片的配置引脚决定，或者在上电复位后从 E²PROM 中获取。"

### 4. SMBus/I2C EEPROM（部分 Switch）

部分 Switch 支持通过 SMBus 接口从外部 EEPROM 加载配置数据（如端口配置、VC 映射表等），在复位后由 Switch 内部的 SMBus 控制器自动读取。

### 5. 内部嵌入式 Flash（较少见）

部分集成度较高的 Switch 芯片内部集成嵌入式 Flash，无需外挂存储芯片。

## 参考资料

- [[pci-express-体系结构导读]] 第2章、第4章 — PCI/PCIe 设备的配置初始化机制
- [[pci-bridge]] — PCI 桥与配置机制
- [[pci-express]] — PCIe 架构组件（含 Switch）
- [[capric-card]] — PCIe 端点设备的实际设计实现

## 未覆盖的内容

本书《[[pci-express-体系结构导读]]》主要关注 PCIe 体系结构的软件视角（配置空间、事务层协议、枚举流程等），对 Switch 固件的具体存储介质和加载流程着墨不多。上述信息结合了本书中关于设备初始化机制的描述与业界的通用实现。
