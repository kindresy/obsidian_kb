---
date: 2026-06-10
tags: [pci-express, linux, zhihu]
type: source-summary
source-url: https://zhuanlan.zhihu.com/p/2047733990099047128
compiled: true
---

# Linux 启动 5 秒后才 Link Up — PCIe 为什么没有超时？

知乎专栏文章，讨论 PCIe 链路训练（LTSSM）在 Linux 启动流程中的时序，以及为什么 PCIe 规范允许长达数秒的 Link Up 延迟。

## Key Points

- PCIe LTSSM 链路训练可能耗时数秒，尤其是在多槽位、多 Switch 的复杂拓扑中
- PCIe 规范没有对 Link Up 时间设硬性限制，Detect/ Polling/ Configuration 状态各有超时但总和可达秒级
- Linux 内核的 pci_enable_device() 会在链路未就绪时等待，但最终由硬件 LTSSM 状态机决定

## Entities Mentioned

- [[pci-express]] — PCI Express 总线概述
