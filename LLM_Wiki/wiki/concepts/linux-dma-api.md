---
date: 2026-06-10
tags: [linux, dma, kernel, api]
type: concept
status: active
---

# Linux DMA API

Linux 内核提供的 DMA 映射 API，用于设备驱动程序进行 DMA 传输。涵盖地址映射、一致性管理、同步操作等。

## Details

### 三种地址类型

| 地址类型 | 说明 | 管理方 |
|---------|------|--------|
| **虚拟地址** (void *) | CPU 使用的地址，kmalloc/vmalloc 返回 | 内核 VM 系统 |
| **物理地址** (phys_addr_t) | CPU 物理地址空间 | /proc/iomem |
| **总线地址** (dma_addr_t) | 设备 DMA 使用的地址 | IOMMU / 主机桥 |

### DMA 映射类型

**一致映射（Coherent DMA）：**
- 驱动初始化时分配，传输完成后才释放
- CPU 和设备可并行访问，无需显式同步
- 适用于：网卡 DMA 环、SCSI 邮箱、设备固件
- API：`dma_alloc_coherent()` / `dma_free_coherent()`

**流式映射（Streaming DMA）：**
- 单次传输映射，传输完成后立即解映射
- 硬件可针对顺序访问优化
- 适用于：网络收发缓冲区、文件系统 I/O
- API：`dma_map_single()` / `dma_map_page()` / `dma_map_sg()`
- 同步：`dma_sync_single_for_cpu/device()`

### 寻址能力设置

```c
if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(64))) {
    dev_warn(dev, "No suitable DMA available\n");
    goto ignore;
}
```

## See Also

- [[zhihu-linux-dma-api]] — DMA API 使用指导（知乎译文）
- [[iommu]] — IOMMU 原理（DMA 地址翻译）

## Counter-Arguments and Gaps

...
