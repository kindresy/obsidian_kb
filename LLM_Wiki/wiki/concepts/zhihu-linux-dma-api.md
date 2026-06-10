---
date: 2026-06-10
tags: [linux, dma, driver, zhihu]
type: source-summary
source-url: https://zhuanlan.zhihu.com/p/496060255
compiled: true
---

# Linux DMA API 使用指导

知乎专栏文章，翻译自 Linux 内核 DMA-API-HOWTO。涵盖 DMA 地址类型、寻址能力设置、一致映射 vs 流式映射、DMA 池、散点/聚集映射、同步操作及错误处理。

## Key Points

- DMA 使用总线地址空间，需通过 IOMMU 或主机桥映射到物理地址
- `dma_set_mask_and_coherent()` 设置设备 DMA 寻址能力，不检查返回值不得使用 DMA
- 一致映射（`dma_alloc_coherent`）：驱动初始化时分配，CPU 和设备可并行访问，适合环描述符等
- 流式映射（`dma_map_single`/`dma_map_page`/`dma_map_sg`）：单次传输映射，用后需解映射
- `dma_sync_single_for_cpu/device`：流式映射在 CPU 和设备交替访问时需同步

## Entities Mentioned

- [[linux-dma-api]] — Linux DMA API
