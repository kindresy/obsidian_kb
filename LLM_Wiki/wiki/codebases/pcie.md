---
date: 2026-06-08
tags: [code, pcie]
type: concept
status: active
source-repo: pcie
code-commit: unknown
---

# pcie — Codebase Overview

## Purpose
(TODO)

## Directory Map
```
  edge.c                         nodes=177 lang=c
  edge.h                         nodes=131 lang=c
```

## Build System
(TODO)

## Architecture Layers

| Layer | Nodes | Description |
|-------|-------|-------------|
| **entry** | 2 | Entry points |
| **interface** | 42 | Userspace interface |
| **interrupt** | 15 | Interrupt handling |
| **dma** | 48 | DMA engine |
| **hardware** | 5 | Hardware abstraction |
| **smbus** | 15 | SMBus management |
| **monitor** | 13 | Exception monitor |
| **boot** | 1 | Boot acceleration |
| **core** | 32 | Core logic |

## Entry Points

- `edge_pcie_exception_event_isr()`
- `edge_pcie_exception_event_work()`
- `edge_pcie_host_notify_irq_init()`
- `edge_pcie_host_notify_irq_deinit()`
- `edge_pcie_host_notify_irq_send()`
- `edge_pcie_host_notify_irq_register()`
- `edge_notify_irq_isr()`
- `smbus_open()`
- `smbus_release()`
- `smbus_ioctl()`

## Key Symbols (top by call complexity)

| Symbol | Call Count |
|--------|-----------|
| `edge_readl_safe()` | 62 |
| `edge_ioctl()` | 40 |
| `smbus_ioctl()` | 13 |
| `edge_probe()` | 12 |
| `transfer_submit()` | 9 |
| `udma_transfer()` | 7 |
| `mmio_xfer()` | 7 |
| `boot_wait_state()` | 7 |
| `edge_set_public_data()` | 7 |
| `edge_remove()` | 7 |
| `edge_exception_event_dump()` | 6 |
| `ioctl_do_read_ext_addr()` | 6 |
| `edge_exception_event_read()` | 5 |
| `edge_setup_irqs()` | 5 |
| `edge_test_bit()` | 5 |
| `ioctl_do_p2p_init()` | 5 |
| `apt_remap_addr()` | 5 |
| `sg_transfer_create()` | 5 |
| `udma_block_transfer()` | 5 |
| `ioctl_do_set_device_id()` | 5 |

## Related Concepts
(Populated by `kb bind propose`)

## Generated Graphs
See: `kb graph export pcie`
