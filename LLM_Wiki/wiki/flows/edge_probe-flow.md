---
date: 2026-06-08
tags: [code, flow, pcie]
type: concept
status: active
source-repo: pcie
entry-point: edge_probe
---

# Edge PCIe Probe Flow

## What this flow does
TBD — Entry function: `edge_probe()` at `edge.c:5826`

## Entry Point
- **Function**: `edge_probe()`
- **File**: `edge.c:5826`

## Call Chain

```
edge_probe()  [core]  edge.c:5826
  edge_dma_pool_create()  [dma]  edge.c:6260
  edge_map_bars()  [core]  edge.c:5653
  edge_create_udma_engines()  [dma]  edge.c:5483
    edge_create_engine()  [core]  edge.c:5436
    edge_destroy_udma_engines()  [dma]  edge.c:5472
  edge_create_cdev()  [core]  edge.c:5561
    edge_set_public_data()  [core]  edge.c:5505
      edge_readl_safe()  [core]  edge.c:45
  edge_exception_event_init()  [core]  edge.c:127
  edge_device_online()  [core]  edge.c:5733
  edge_disable_switch_vdn_acs_redir()  [core]  edge.c:1757
  edge_disable_relax_order()  [core]  edge.c:1781
  edge_destroy_cdev()  [core]  edge.c:5546
  edge_destroy_udma_engines()  [dma]  edge.c:5472
  edge_unmap_bars()  [core]  edge.c:5699
  edge_dma_pool_destory()  [dma]  edge.c:6323
```

## Key Functions in This Flow

| Function | File | Line | Layer |
|----------|------|------|-------|
| `edge_dma_pool_create()` | `edge.c` | 6260 | dma |
| `edge_map_bars()` | `edge.c` | 5653 | core |
| `edge_create_udma_engines()` | `edge.c` | 5483 | dma |
| `edge_create_cdev()` | `edge.c` | 5561 | core |
| `edge_exception_event_init()` | `edge.c` | 127 | core |
| `edge_device_online()` | `edge.c` | 5733 | core |
| `edge_disable_switch_vdn_acs_redir()` | `edge.c` | 1757 | core |
| `edge_disable_relax_order()` | `edge.c` | 1781 | core |
| `edge_destroy_cdev()` | `edge.c` | 5546 | core |
| `edge_destroy_udma_engines()` | `edge.c` | 5472 | dma |
| `edge_unmap_bars()` | `edge.c` | 5699 | core |
| `edge_dma_pool_destory()` | `edge.c` | 6323 | dma |

## Register Access
(To be populated from code analysis)

## Related Concepts
- [[pcie]] — pcie codebase overview

## Debug Checklist
- [ ] Verify entry point conditions
- [ ] Check error return paths
- [ ] Validate register configurations

## Counter-Arguments and Gaps

...
