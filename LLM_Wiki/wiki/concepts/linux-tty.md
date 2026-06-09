---
date: 2026-06-09
tags: [linux, tty, driver]
type: concept
status: active
---

# Linux TTY 子系统

Linux TTY 子系统是内核中管理终端设备的框架，覆盖串口、虚拟终端、伪终端（pty）等设备类型。

## Details

### TTY 驱动三层架构

```text
用户空间 (open/read/write/ioctl)
       │
  ┌────┴────┐
  │  TTY 核心  │  ── tty_io.c, tty_ldisc.c
  └────┬────┘
       │
  ┌────┴────┐
  │ 线路规程   │  ── n_tty（默认）, n_hdlc 等
  └────┬────┘
       │
  ┌────┴────┐
  │ TTY 驱动  │  ── 串口驱动（uart_ops）、pty 驱动等
  └─────────┘
       │
      硬件
```

### 关键概念

- **终端（Terminal）**：用户与系统交互的设备，可以是物理串口或虚拟终端
- **控制台（Console）**：系统消息输出的设备，通常是某一个特定终端
- **伪终端（Pseudo Terminal, PTY）**：成对出现的虚拟设备（master/slave），用于 SSH、xterm 等
- **线路规程（Line Discipline）**：位于 TTY 核心和 TTY 驱动之间的协议层，处理行编辑、回显等

## See Also

- [[i3c]] — MIPI I3C 总线协议（另一类驱动开发话题）
- [[linux-tty-notes]] — Linux TTY 框架讲解笔记

## Counter-Arguments and Gaps

...
