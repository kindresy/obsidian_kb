---
date: 2026-06-09
source-type: note
title: "I3C Broadcast CCC 典型用法与场景"
tags: ["i3c", "bus-protocol"]
compiled: false
---

## Broadcast CCC 在 I3C 中的用途

Broadcast CCC 的命令码范围是 **`0x00 ~ 0x7F`**（bit[7] = 0），发送给 **总线上所有 I3C Target**。所有 Target 即使不支持某条命令，也必须检查并正确识别边界。

### 核心特征

```
S / Sr ─ 7'h7E + W ─ 广播 CCC 码 ─ [可选数据] ─ P / Sr
         ↑
    所有 I3C Target 必须识别
```

---

### 实际用途与场景

#### 1️⃣ 动态地址管理

| Broadcast CCC | 码值 | 场景 |
|---|---|---|
| **`ENTDAA`** | `0x07` | 总线初始化时，告诉所有未分配地址的 Target 开始动态地址分配仲裁流程 |
| **`RSTDAA`** | `0x06` | 清除所有 Target 的动态地址，让总线回到"未分配"状态重新开始 |
| **`SETAASA`** | `0x29` | 让所有拥有静态 I2C 地址的 I3C Target，直接用静态地址作为动态地址 |

**场景示例——系统冷启动：**
```
1. Power-up → Controller 发 RSTDAA（所有人清空旧地址）
2. Controller 发 SETAASA（有静态地址的 Target 直接当动态地址用）
3. Controller 发 ENTDAA（剩余设备进入仲裁，逐个拿动态地址）
```

#### 2️⃣ 事件使能/禁用

| Broadcast CCC | 码值 | 场景 |
|---|---|---|
| **`ENEC`** | `0x00` | 全局打开 IBI（带内中断）、Hot-Join、CRR（主控权请求） |
| **`DISEC`** | `0x01` | 全局关闭上述事件 |

**场景示例——热插拔管控：**
```
系统启动初始化完成 → Controller 资源暂时紧张

Controller 发 DISEC(DISHJ=1)  ← 禁止所有 Target 发 Hot-Join 请求
...（一段时间后，资源就绪）...
Controller 发 ENEC(ENHJ=1)    ← 允许 Hot-Join，新设备可以加入
```

#### 3️⃣ 活动状态提示（电源管理）

| Broadcast CCC | 码值 | 场景 |
|---|---|---|
| **`ENTAS0`** | `0x02` | 总线即将高频率活动，Target 保持全速就绪 |
| **`ENTAS1`** | `0x03` | 一般活动 |
| **`ENTAS2`** | `0x04` | 总线即将空闲 ≥ 1ms，Target 可进入低功耗 |
| **`ENTAS3`** | `0x05` | 总线即将长时间空闲 ≥ 50ms，Target 可深度睡眠 |

**场景示例——传感器间歇上报：**
```
Target 每 100ms 上报一次数据

在上报间隙：
Controller 发 ENTAS3  ← 告诉所有 Target "后面至少 50ms 没活"
Target 收 ENTAS3 →
  关闭内部高速振荡器
  进入休眠
  等待下次 START 唤醒
```

#### 4️⃣ 进入 HDR 高速模式

| Broadcast CCC | 码值 | 场景 |
|---|---|---|
| **`ENTHDR0`** | `0x20` | 进入 HDR-DDR（双倍数据速率）模式 |
| **`ENTHDR1`** | `0x21` | 进入 HDR-TSP（三进制纯总线）模式 |
| **`ENTHDR3`** | `0x23` | 进入 HDR-TSL（三进制+传统 I2C）模式 |

**场景示例——批量传输大数据：**
```
Controller 需要从摄像头传感器读取一帧图像

S → 7'h7E + W → ENTHDR0 → [进入 HDR-DDR 模式]
  → HDR-DDR Command Word → Data Words ... → CRC
  → HDR Restart → 第二个 HDR 命令 → Data ...
  → HDR Exit Pattern → STOP

这样跑完一帧数据，速度比 SDR 快接近 2 倍
```

#### 5️⃣ 第二主机信息同步

| Broadcast CCC | 码值 | 场景 |
|---|---|---|
| **`DEFSLVS`** | `0x0E` | 当前 Active Controller 把总线上所有从设备信息告诉第二主机 |

**场景示例——主控权交接：**
```
Primary Controller 准备把总线控制权交给某个 Secondary Controller

Primary 发 DEFSLVS
  └─ 包含所有 Target 的: {BCR, DCR, StaticAddr, DynamicAddr}
        └─ 把自己的信息编为 7'h7E

Secondary 收到后更新本地设备表
Primary 发 GETACCCR → Secondary 请求接管总线
→ 主控权平稳切换
```

---

### 📊 一句话总结

| 操作类别 | Broadcast CCC | **≈ 等价概念** |
|---|---|---|
| 地址管理 | `RSTDAA / ENTDAA / SETAASA` | "全员清空地址" / "全员开始仲裁" |
| 事件开关 | `ENEC / DISEC` | "全员开中断" / "全员关中断" |
| 电源提示 | `ENTAS0~3` | "全员注意，后面忙/闲了" |
| 模式切换 | `ENTHDR0~7` | "全员准备，进高速模式" |
| 信息广播 | `DEFSLVS` | "全员听好，总线设备列表如下" |

> 所有 Broadcast CCC 都走 `7'h7E + W` 路径，**不需要** Repeated START 到某个特定 Target 地址，这是它和 Direct CCC 在波形上的根本区别。