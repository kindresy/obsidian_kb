---
date: 2026-05-28
tags: [architecture, cache-coherence]
type: concept
status: draft
---

# MESIF 协议

MESIF 协议主要解决 ccNUMA（Cache Coherent NUMA）处理器结构的 Cache 共享一致性问题。

## Details

- 在 MESI 协议基础上增加了 F（Forward）状态
- 使用目录表（Directory-based），而非总线监听
- 与 QPI（Quick Path Interconnect）互连技术密切相关
- 参考书籍："Weaving High Performance Multiprocessor Fabric" by Robert A. Maddox et al.

## See Also

- [[pci-express-体系结构导读]]

## Counter-Arguments and Gaps

...
