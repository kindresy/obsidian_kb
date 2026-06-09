---
date: 2026-06-09
source-type: note
title: "I3C Slave 终止 Read 能力解析（T-bit）"
tags: ["i3c", "bus-protocol"]
compiled: false
---

问题：I3C与I2C的一个区别就在于slave具有终止master读数据传输的能力，具体的操作流程细节可参考5.1.2.3.4，T-bit的驱动方式永远为PP，这句话怎么理解？

这句话的核心是：**I2C 读传输由 Master 决定什么时候结束；I3C SDR 读传输允许 Target 通过第 9 bit，也就是 T-bit，主动告诉 Controller：“这次读到此为止”。**

新版术语里，`master/slave` 分别叫 **Controller/Target**。你这句话里的 slave 就是 Target，master 就是 Controller。规范 5.1.2.3.4 讲的正是：I3C SDR Read 中第 9 bit 是 End-of-Data / Continue 语义，而不是 I2C 的 ACK/NACK。

## 1. 先对比 I2C Read

I2C 读数据时：

```text
Slave  → 8-bit Data
Master → ACK / NACK
```

也就是说，第 9 bit 是 **Master 驱动**的：

```text
Master ACK  = 继续读
Master NACK = 结束读
```

所以 I2C 中，Slave 基本没有协议级能力主动说“我不想再发了”。Master 想读多少字节，Slave 就要继续吐数据，直到 Master NACK + STOP。

---

## 2. I3C SDR Read：第 9 bit 由 Target 驱动

I3C SDR Read 时：

```text
Target → 8-bit Data + T-bit
```

这里第 9 bit 是 **Target 驱动**的 T-bit。

它有两个核心语义：

|T-bit|含义|谁决定|
|---|---|---|
|`1`|Continue，后面还有数据|Target|
|`0`|End，Target 要结束本次 Read Message|Target|

所以 I3C Read 变成：

```text
Target 发 8-bit data
Target 再发 T-bit

T = 1：Target 表示可以继续
T = 0：Target 表示本次读结束
```

这就是“slave/Target 具有终止 master/Controller 读数据传输的能力”。

---

## 3. “T-bit 的驱动方式永远为 PP”怎么理解？

这里的 **PP** 是 **Push-Pull**。

这句话不要理解成“Target 永远从头到尾死死驱动 SDA”。更准确地说：

> I3C SDR Read 的第 9 bit 不再是 I2C 那种 Open-Drain ACK/NACK，而是 Target 按 I3C Read Data bit 的方式先用 Push-Pull 驱动 T-bit，然后在规定时刻释放 SDA，完成总线所有权切换。

也就是：

```text
I2C Read 第 9 bit：
    Master 用 ACK/NACK，通常 Open-Drain 语义

I3C SDR Read 第 9 bit：
    Target 用 T-bit，按 Push-Pull 数据位语义驱动
```

---

## 4. Target 用 T-bit = 0 结束读的具体流程

假设 Target 发送最后一个数据字节：

```text
Target → D7 D6 D5 D4 D3 D2 D1 D0
Target → T = 0
```

流程是：

```text
1. Target 在 T-bit 周期把 SDA 驱动为 Low，也就是 T = 0。

2. Controller 看到 T = 0，知道 Target 要结束 Read。

3. 在后续时序中，Target 释放 SDA。

4. Controller 接管 SDA。

5. Controller 发 STOP 或 Repeated START。
```

波形语义可以简化成：

```text
Target Data Byte
    ↓
T-bit = 0
    ↓
Target: 我结束了
    ↓
Controller: 接管总线
    ↓
P 或 Sr
```

其中：

```text
P  = STOP，结束本次 Frame
Sr = Repeated START，不释放总线，继续下一个 Message
```

---

## 5. Target 用 T-bit = 1 继续读的流程

如果 Target 还有数据：

```text
Target → 8-bit Data
Target → T = 1
```

含义是：

```text
我还有数据，可以继续读
```

如果 Controller 也愿意继续，它什么都不做，让总线保持 High，Target 下一拍继续发下一个字节。

简化为：

```text
T = 1
Controller 不打断
Target 继续发下一个 Data Byte
```

---

## 6. Controller 仍然可以中止读

虽然 I3C 允许 Target 主动结束读，但 Controller 也不是完全被动。

当 Target 给出：

```text
T = 1
```

表示 Target 想继续。但 Controller 如果不想继续读，可以在规定时序内拉低 SDA，产生 Repeated START，从而中止这次 Read。

所以 I3C SDR Read 是双向可控：

```text
Target 可用 T = 0 主动结束
Controller 可在 T = 1 时主动 abort
```

这比 I2C 更灵活。

---

## 7. 最关键的理解

你可以这样记：

```text
I2C Read:
    第 9 bit 是 Master ACK/NACK
    读多少由 Master 决定

I3C SDR Read:
    第 9 bit 是 Target T-bit
    T = 1 表示继续
    T = 0 表示 Target 结束
    T-bit 按 Push-Pull 数据位方式驱动，而不是 Open-Drain ACK
```

所以“**T-bit 的驱动方式永远为 PP**”这句话真正想强调的是：**不要把 I3C Read 的第 9 bit 当成 I2C ACK/NACK 去看；它是 Target 驱动的协议数据位/控制位，使用 Push-Pull 语义，并参与读传输的继续/结束控制。**