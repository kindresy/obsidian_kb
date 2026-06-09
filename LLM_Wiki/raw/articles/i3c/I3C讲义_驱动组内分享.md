---
date: 2026-06-09
source-type: article
title: "MIPI I3C 总线协议 — 驱动开发讲义"
tags: ["i3c", "bus-protocol"]
compiled: false
---

# MIPI I3C 总线协议 — 驱动开发讲义

> 适合驱动组内分享，面向有 I2C/SPI 基础的嵌入式工程师

---

## 一、为什么需要 I3C？

### 背景
智能手机和 IoT 设备中传感器数量激增（加速度计、陀螺仪、气压计、ToF 等），传统的 I2C/SPI/UART 各有利弊，但都面临共同问题：

- **I2C**：速度慢（3.4MHz HS-mode）、需要额外中断引脚、无热插拔
- **SPI**：引脚多（4+N）、无标准设备枚举
- **额外引脚**：每个传感器还需要独立的中断、使能、片选，GPIO 不够用

**I3C 的目标**：用两根线（SCL + SDA）解决所有问题——高速、低功耗、带中断、可热插拔、多主、兼容 I2C。

### 核心优势
| 维度 | I2C | I3C |
|------|-----|-----|
| 最大速度 | 3.4 MHz | 12.5 MHz (SDR) / 25+ MHz (HDR) |
| 中断 | 需额外引脚 | **带内中断（IBI）**，不需要额外引脚 |
| 动态地址 | 否（静态地址，有冲突风险） | **动态地址分配（DAA）** |
| 热插拔 | 不支持 | **Hot-Join** 机制 |
| 多主 | 有限支持 | **原生多主，第二主机切换** |
| 功耗 | 较高 | 约 I2C 的 1/6（推挽驱动） |
| I2C 兼容 | — | 兼容 I2C 从设备 |

---

## 二、总线架构

### 2.1 物理层
- **两线制**：SDA（数据）+ SCL（时钟）
- **混合驱动**：开漏（OD）用于仲裁 + 推挽（PP）用于高速数据传输
- **三种总线配置**：

| 类型 | 说明 | 速度 |
|------|------|------|
| 纯总线 (Pure Bus) | 只有 I3C 设备 | 最快 |
| 混合快速总线 | I3C + I2C 设备，I2C 带 50ns 滤波器 | 中等 |
| 混合慢速总线 | I3C + I2C 设备，I2C 无 50ns 滤波器 | 最慢 |

### 2.2 设备角色

**主设备角色：**

| 角色 | 说明 |
|------|------|
| **Main Master** | 总线初始化、动态地址分配、全局管理，唯一且不可转移 |
| **Secondary Master** | 可从 Main Master 获得总线控制权，临时成为 Current Master |
| **Current Master** | 当前正在控制总线的设备（不固定） |

**从设备角色：**

| 角色 | 说明 |
|------|------|
| **I3C Slave** | 普通从设备，支持 SDR + HDR |
| **SDR-Only Slave** | 仅支持 SDR 模式，不支持 HDR |

### 2.3 关键寄存器
| 寄存器 | 全称                             | 用途                |
| --- | ------------------------------ | ----------------- |
| BCR | Bus Characteristic Register    | 设备角色与功能（只读）       |
| DCR | Device Characteristic Register | 设备类型如加速度计/陀螺仪（只读） |
| LVR | Legacy Virtual Register        | 传统 I2C 设备的虚拟寄存器   |

---

## 三、协议模式

### 3.1 SDR（Single Data Rate）— 默认模式
- 类似 I2C 但速度更高（最高 12.5MHz）
- 9 位数据字：8 位数据 + **T-Bit（替代 ACK/NACK）**
- 写操作 T-Bit = 奇偶校验位
- 读操作 T-Bit = 数据结束标志（从机或主机可控制）
- 所有控制功能都在 SDR 模式下完成

### 3.2 HDR（High Data Rate）— 高速模式

| 子模式     | 全称                    | 速度         | 特点            |
| ------- | --------------------- | ---------- | ------------- |
| HDR-DDR | Double Data Rate      | ~SDR 的 2 倍 | 双边沿采样，信令同 SDR |
| HDR-TSP | Ternary Symbol Pure   | 最高         | 三进制编码，纯总线专用   |
| HDR-TSL | Ternary Symbol Legacy | 较高         | 三进制编码，兼容 I2C  |

进入 HDR 模式：主机先发送 `7'h7E`（广播地址），然后用 CCC 命令 `ENTHDR0~3` 切换。

---

## 四、核心机制详解

### 4.1 动态地址分配（DAA）

这是 I3C 最重要的新特性。流程：

```
1. 主机从自身或 NVM 获取总线设备信息
2. 有静态地址的 I3C 设备通过 SETDASA CCC 分配
3. 主机发送 ENTDAA CCC → RESTART → 7'h7E/R（读广播）
4. 所有未分配地址的从设备在 SDA 上驱动 48-bit 临时 ID
5. 仲裁规则：临时 ID 最小的设备赢
6. 主机为赢家分配 7 位动态地址（带奇偶校验）
7. 从机 ACK → 完成；NACK → 重试（最多 3 次）
```

**48-bit 临时 ID 格式：**
- `[47:33]` — MIPI 制造商 ID（15 位）
- `[32]` — 0 = 供应商固定值 / 1 = 随机值
- `[31:16]` — Part ID
- `[15:12]` — Instance ID
- `[11:0]` — 自定义

### 4.2 带内中断（IBI）

I3C 设备不需要额外中断引脚。从设备直接在总线上发中断请求：

```
1. 从设备等待 Bus Available Condition
2. 从设备将 SDA 拉低 → 主机检测到 START
3. 从设备发送自己的动态地址 + RnW=1
4. 主机可选择：
   a) ACK → 处理中断（若 BCR[2]=1 则还有数据载荷）
   b) NACK → 拒绝（不禁用中断）
   c) NACK + DISEC → 拒绝并禁用中断
5. 优先级：动态地址越低的设备优先级越高
```

### 4.3 热插拔（Hot-Join）

设备可在总线运行中接入：

```
1. 从设备等待 Bus Idle Condition
2. 从设备发送 7'h02/W（热连接地址）作为 IBI
3. 主机响应：
   a) NACK → 从机下次再试
   b) ACK + DISHJ → 接受但禁止后续热连接
   c) ACK + ENTDAA → 启动动态地址分配
```

### 4.4 通用命令代码（CCC）

CCC 是 I3C 的命令通道，全部以广播地址 `7'h7E` 开头：

| 分类   | 命令码范围       | 说明             |
| ---- | ----------- | -------------- |
| 广播命令 | `0x00~0x7F` | 发给总线上所有 I3C 从机 |
| 直接命令 | `0x80~0xFE` | 发给指定动态地址的从机    |

**常用 CCC 命令：**

| 命令            | 码值        | 功能                    |
| ------------- | --------- | --------------------- |
| ENTDAA        | 0x07      | 进入动态地址分配              |
| RSTDAA        | 0x06      | 重置动态地址                |
| SETDASA       | 0x87      | 用静态地址设置动态地址           |
| ENEC/DISEC    | 0x00/0x01 | 启用/禁用从机事件（中断/热连接/主控权） |
| ENTAS0~3      | 0x80~0x83 | 设置活动状态（用于电源管理）        |
| SETMWL/SETMRL | 0x89/0x8A | 设置最大写/读长度             |
| GETBCR/GETDCR | 0x9C/0x9D | 读取 BCR/DCR 寄存器        |
| DEFSLVS       | 0x84      | 定义从设备信息给第二主机          |

### 4.5 SDR 数据字与 I2C 的关键差异

| 方面        | I2C    | I3C SDR                |
| --------- | ------ | ---------------------- |
| 地址头 ACK 后 | 保持开漏   | **切换为推挽**驱动            |
| 写数据第 9 位  | 从机 ACK | **T-Bit = 奇偶校验**（主机发）  |
| 读数据第 9 位  | 主机 ACK | **T-Bit = 结束标志**（从机控制） |
| SCL 驱动    | 开漏     | **推挽**（仅 Master 驱动）    |
| 读数据终止     | 仅主机可终止 | **主机和从机都可终止**          |

### 4.6 地址仲裁

START 后的地址头可仲裁（Repeat START 后的不可仲裁）：

```
仲裁规则（开漏）：
- 发送位 1 → 不驱动 SDA，监视 SDA 电平
- 如果 SDA 被其他设备拉低 → 丢失仲裁，退出
- 发送位 0 → 驱动 SDA 为低

优化：I3C 主机可将第二主机/IBI 从机的地址限制在
7'h03~7'h3F（A6=0），这样当看到 A6=1 时就知道总线空闲，
可将剩余位切换为推挽高速发送。
```

---

## 五、地址空间与限制

I3C 支持最多 11 个从设备（实际受总线电容限制）。

| 地址 | 用途 |
|------|------|
| 7'h00, 7'h01 | 保留 |
| 7'h02 | 热连接地址 |
| 7'h03~7'h3D | **可用（54 个）** |
| 7'h3E, 7'h7E, 7'h7F | 保留 |
| 7'h7E | **广播地址**（CCC 命令） |
| 7'h3F~7'h7D | 有条件可用（需检查是否与 I2C 设备冲突） |

**针对第二主机和 IBI 从机的地址分配建议：** 限制在 `7'h03~7'h3F`（A6=0），这样主机能用推挽模式优化仲裁。

---

## 六、时钟延迟（Clock Stall）

主机可在特定状态下延长 SCL 低电平：

| 条件 | 最长延迟 |
|------|---------|
| ACK/NACK 阶段 | 100 μs |
| 写数据奇偶校验位 | 100 μs |
| 读数据 T-Bit | 100 μs |
| 动态地址第一位 | **15 ms** |

总线活动状态通过 ENTAS0~3 调整 tCAS 最大值（从机拉低 SDA 后主机的响应时间）：
- ENTAS3：50 ms（默认）
- ENTAS0：最短响应

---

## 七、驱动开发注意点

### 7.1 初始化流程
```
1. 主机上电 → 拉高 SCL/SDA
2. 获取总线设备信息（静态地址、设备类型）
3. 用 SETDASA 为有静态地址的 I3C 设备分配动态地址
4. 发送广播 ENTDAA → RESTART → 7'h7E/R
5. 仲裁 → 为赢家分配动态地址 → 重复直到全部完成
6. 用 DEFSLVS 通知第二主机总线设备信息
7. 正常通信
```

### 7.2 寄存器操作注意
- **T-Bit 意义不同**：写数据时要算奇偶校验，读数据时要注意从机用 T-Bit 表示数据结束
- **ACK 切换推挽**：地址 ACK 后 SDA 驱动模式从开漏切换为推挽，时序不同
- **SCL 仅主机驱动**：传统 I2C 的时钟扩展（clock stretching）机制在 I3C SDR 中不存在
- **时钟延迟**：主机可延长 SCL 低电平，但有限制（最长 15ms）

### 7.3 与传统 I2C 设备共存
- **混合快速总线**：I2C 设备带 50ns 尖峰滤波器 → I3C 的 SCL 高电平小于 50ns 时 I2C 设备不感知
- **混合慢速总线**：I2C 设备无 50ns 滤波器 → I3C 发送到 I3C 从机时需考虑 I2C 可见性
- **I3C 不支持 I2C 主机**：总线上不能有 I2C 主机设备

### 7.4 调试建议
1. **先确认总线类型**：纯总线还是混合总线？决定了速度上限
2. **检查设备角色**：读 BCR 和 DCR 寄存器确认设备能力
3. **注意动态地址**：每次上电可能不同，不要硬编码
4. **IBI 优先级**：低地址优先级高，高频中断设备分配低地址
5. **活动状态管理**：正确使用 ENTASx 可显著省电
6. **Clock Stall 不是 Clock Stretch**：I3C 中只有主机可以延迟时钟

---

## 八、总结

### I3C 的核心理念
```
两根线（SCL + SDA）解决所有传感器总线需求：
高速 × 低功耗 × 中断 × 热插拔 × 多主 × 兼容 I2C
```

### 对驱动工程师的价值
- **省引脚**：不需要为每个设备分配独立的中断引脚
- **省成本**：动态地址消除了地址冲突风险，简化 PCB 设计
- **省调试**：标准 CCC 命令使设备枚举和配置规范化
- **灵活**：第二主机切换和热插拔支持动态系统配置

### 主要挑战
1. 动态地址管理比静态地址复杂
2. 混合总线配置需要仔细权衡速度与兼容性
3. T-Bit 模式需要与 I2C 的 ACK/NACK 思维模式切换
4. I3C 不支持传统 I2C 主设备

---

---

## 九、代码实现对照—Linux I3C 驱动框架

基于对 Linux 内核 `drivers/i3c/` 子系统的代码分析（27 源文件，17,615 行，555 个函数）。

### 9.1 整体架构

```
drivers/i3c/
├── master.c          ─── I3C 核心框架（总线管理、DAA、IBI、CCC）
├── device.c          ─── I3C 设备驱动 API
├── internals.h       ─── 内部头文件
├── Kconfig / Makefile
└── master/
    ├── dw-i3c-master.c      ─── Synopsys DesignWare
    ├── svc-i3c-master.c     ─── Silvaco
    ├── i3c-master-cdns.c    ─── Cadence（参考实现）
    ├── adi-i3c-master.c     ─── Analog Devices
    ├── ast2600-i3c-master.c ─── ASpeed（依赖 DesignWare）
    ├── renesas-i3c.c        ─── Renesas
    └── mipi-i3c-hci/        ─── MIPI HCI 标准实现
        ├── core.c           ─── HCI 核心
        ├── pio.c            ─── PIO 传输
        ├── dma.c            ─── DMA 传输
        ├── cmd_v1.c/v2.c    ─── 命令队列
        ├── dat_v1.c         ─── 设备地址表
        └── dct_v1.c         ─── 设备配置表
```

### 9.2 核心框架核心函数

| I3C 概念 | 内核函数 | 文件:行号 | 说明 |
|---------|---------|----------|------|
| **总线注册** | `i3c_master_register()` | `master.c:3138` | 注册 I3C master，初始化 bus，扫描已有设备 |
| **总线注销** | `i3c_master_unregister()` | `master.c:3159` | 注销 master，清理 IBI/设备/DAA |
| **DAA** | `i3c_master_do_daa()` | `master.c:1851` | 动态地址分配入口 |
| **DAA 扩展** | `i3c_master_do_daa_ext()` | `master.c:1819` | 支持 DAA 前重置地址的版本 |
| **ENTDAA** | `i3c_master_entdaa_locked()` | `master.c:1083` | 发送 ENTDAA CCC 进入 DAA 模式 |
| **IBI 队列** | `i3c_master_queue_ibi()` | `master.c:2785` | 将 IBI 槽排入中断处理队列 |
| **IBI 池分配** | `i3c_generic_ibi_alloc_pool()` | `master.c:2918` | 分配通用 IBI 槽内存池 |
| **热插拔启用** | `i3c_master_enable_hotjoin()` | `master.c:700` | 使能 Hot-Join 事件处理 |
| **热插拔禁用** | `i3c_master_disable_hotjoin()` | `master.c:712` | 禁用 Hot-Join |
| **CCC 发送** | `i3c_master_send_ccc_cmd_locked()` | `master.c:?` | 发送 CCC 通用命令 |
| **ENEC/DISEC** | `i3c_master_enec_locked()` | `master.c:1151` | 使能从机事件（中断/热插入） |
| **ENEC/DISEC** | `i3c_master_disec_locked()` | `master.c:1131` | 禁能从机事件 |
| **DEFSLVS** | `i3c_master_defslvs_locked()` | `master.c:1242` | 定义从设备信息给第二主机 |
| **地址分配** | `i3c_master_get_free_addr()` | `master.c:988` | 获取空闲动态地址 |
| **设置设备信息** | `i3c_master_set_info()` | `master.c:1996` | 设置 master 设备信息和能力 |
| **设备操作** | `i3c_device_do_xfers()` | `device.c:?` | I3C 设备数据传输 |
| **DMA 映射** | `i3c_master_dma_map_single()` | `master.c:1916` | DMA 单缓冲区映射（一致性） |

### 9.3 Master 控制器接口

每个硬件驱动必须实现 `i3c_master_controller_ops`，这是硬件抽象层：

| 回调 | 用途 | 参考实现（Cadence） |
|------|------|-------------------|
| `send_ccc_cmd` | 发送 CCC 命令 | `cdns_i3c_master_send_ccc_cmd()` |
| `do_daa` | 执行动态地址分配 | `cdns_i3c_master_do_daa()` |
| `priv_xfers` | 私有数据传输 | `cdns_i3c_master_priv_xfers()` |
| `i3c_xfers` | I3C 帧传输（带 IBI 支持） | `cdns_i3c_master_i3c_xfers()` |
| `enable_ibi` | 使能带内中断 | `cdns_i3c_master_enable_ibi()` |
| `disable_ibi` | 禁能带内中断 | `cdns_i3c_master_disable_ibi()` |
| `recycle_ibi_slot` | 回收 IBI 数据槽 | `cdns_i3c_master_recycle_ibi_slot()` |
| `free_ibi` | 释放 IBI | `cdns_i3c_master_free_ibi()` |
| `hotjoin` | 热插拔事件使能/禁能 | `dw_i3c_master_enable_hotjoin()` |

### 9.4 协议概念到代码的映射

#### DAA 流程代码路径
```
i3c_master_do_daa()
  → i3c_master_entdaa_locked()      // 发送 ENTDAA + 广播地址 7'h7E/R
    → master->ops->do_daa()          // 硬件层：从设备 48-bit 临时 ID 仲裁
  → i3c_master_add_i3c_dev_locked()  // 为赢得仲裁的设备创建设备
```

#### IBI 中断流程代码路径
```
硬件检测 START + 地址仲裁
  → 控制器 ISR → i3c_master_queue_ibi()  // 排入 IBI 队列
    → i3c_master_handle_ibi()             // 内核 worker 处理
      → 匹配设备 → 调用设备的 IBI 回调
```

#### Hot-Join 流程代码路径
```
从设备发送 7'h02/W
  → i3c_master_enable_hotjoin()     // master 使能热插拔检测
    → 检测到事件 → i3c_master_do_daa()  // 触发 DAA 分配地址
```

### 9.5 七个控制器驱动的对比

| 控制器 | 文件 | 行数 | 特点 |
|--------|------|------|------|
| **Cadence** | `i3c-master-cdns.c` | 1,647 | 最早的参考实现，功能完整 |
| **DesignWare** | `dw-i3c-master.c` | 1,850 | Synopsys IP，支持 HDR 模式 |
| **Silvaco** | `svc-i3c-master.c` | 2,170 | 最复杂，双角色（master/slave） |
| **ADI** | `adi-i3c-master.c` | 1,016 | AXI 接口，FPGA 集成友好 |
| **Renesas** | `renesas-i3c.c` | 1,482 | 面向 RZ 系列 SoC |
| **MIPI HCI** | `mipi-i3c-hci/core.c` | 1,078 | 遵循 MIPI HCI 标准，可移植性强 |
| **ASpeed** | `ast2600-i3c-master.c` | 187 | 最精简，复用 DesignWare 驱动层 |

MIPI HCI 驱动内部又分为：
- **PIO 模式** (`pio.c`, 1,070 行)：CPU 直接搬运数据，延迟低
- **DMA 模式** (`dma.c`, 905 行)：大数据量传输，CPU 开销小
- **CMD v1/v2**：命令队列的不同版本实现
- **DAT/DCT**：设备地址表（Device Address Table）和设备配置表（Device Configuration Table）

### 9.6 从代码看 I3C 协议实现的特点

1. **总线模型**：Linux I3C 子系统复用 I2C 总线框架，`i3c_bus_type` 与 `i2c_bus_type` 协同工作，通过 Kconfig `I3C_OR_I2C` 处理兼容性
2. **地址管理**：使用位图管理动态地址池（`i3c_bus->addrs`），并通过 `i3c_master_get_free_addr()` 分配
3. **IBI 数据槽**：采用预分配内存池模式（`i3c_generic_ibi_alloc_pool()`），避免中断路径中动态分配
4. **同步模型**：使用读写锁 + `completion` 机制协调总线维护操作和数据传输
5. **设备匹配**：支持 `i3c_device_id`（DCR/manufacturer/part/extra 四级匹配）和 OF 设备树匹配

### 9.7 对驱动开发者的建议

阅读内核 I3C 驱动的推荐顺序：
```
master.c           → 核心框架，理解总线模型和协议流程
device.c           → 设备驱动 API
i3c-master-cdns.c  → 参考实现，看 ops 如何映射硬件
dw-i3c-master.c    → 另一完整实现，与 Cadence 对比
mipi-i3c-hci/      → 标准实现，适合做新平台驱动的模板
```

## 参考资料
- MIPI I3C Basic Specification v1.1.1
- NXP: AMF-DES-T2686 — An Introduction to MIPI I3C
- Linux kernel `drivers/i3c/` (torvalds/linux)
- [[i3c]] — Wiki 概念页
- [[i3c-vs-i2c]] — I3C 与 I2C 对比详情
