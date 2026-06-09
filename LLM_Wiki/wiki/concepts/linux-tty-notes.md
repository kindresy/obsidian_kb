---
date: 2026-06-09
tags: [tty, linux, driver]
type: source-summary
source-url: http://www.wowotech.net/tty_framework/tty_concept.html
compiled: true
---

# Linux TTY Framework (1) — 基本概念

Linux TTY 子系统的基本概念讲解，摘自 wowotech 的 TTY 框架系列文章。涵盖 TTY 设备模型、线路规程（line discipline）、控制台（console）、伪终端（pty）等核心概念。

## Key Points

- TTY 是 Linux 中最普遍的设备类型之一，从串口到虚拟终端都属此类
- TTY 驱动架构分为三层：TTY 核心、线路规程（n_tty/n_hdlc 等）、TTY 驱动
- 终端（terminal）与控制台（console）是不同概念

## Entities Mentioned

- [[linux-tty]] — Linux TTY 子系统
