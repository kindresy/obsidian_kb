---
date: 2026-06-08
tags: [code, linux-i3c]
type: concept
status: active
source-repo: linux-i3c
code-commit: 4549871118cf
---

# linux-i3c — Codebase Overview

## Purpose
(TODO)

## Directory Map
```
  drivers/i3c/device.c           nodes=20 lang=c
  drivers/i3c/internals.h        nodes=5 lang=c
  drivers/i3c/master.c           nodes=152 lang=c
  drivers/i3c/master/adi-i3c-master.c nodes=64 lang=c
  drivers/i3c/master/ast2600-i3c-master.c nodes=14 lang=c
  drivers/i3c/master/dw-i3c-master.c nodes=87 lang=c
  drivers/i3c/master/dw-i3c-master.h nodes=10 lang=c
  drivers/i3c/master/i3c-master-cdns.c nodes=68 lang=c
  drivers/i3c/master/mipi-i3c-hci/cmd.h nodes=19 lang=c
  drivers/i3c/master/mipi-i3c-hci/cmd_v1.c nodes=29 lang=c
  drivers/i3c/master/mipi-i3c-hci/cmd_v2.c nodes=13 lang=c
  drivers/i3c/master/mipi-i3c-hci/core.c nodes=57 lang=c
  drivers/i3c/master/mipi-i3c-hci/dat.h nodes=3 lang=c
  drivers/i3c/master/mipi-i3c-hci/dat_v1.c nodes=20 lang=c
  drivers/i3c/master/mipi-i3c-hci/dct.h nodes=1 lang=c
  drivers/i3c/master/mipi-i3c-hci/dct_v1.c nodes=8 lang=c
  drivers/i3c/master/mipi-i3c-hci/dma.c nodes=33 lang=c
  drivers/i3c/master/mipi-i3c-hci/ext_caps.c nodes=28 lang=c
  drivers/i3c/master/mipi-i3c-hci/ext_caps.h nodes=1 lang=c
  drivers/i3c/master/mipi-i3c-hci/hci.h nodes=13 lang=c
  drivers/i3c/master/mipi-i3c-hci/hci_quirks.c nodes=5 lang=c
  drivers/i3c/master/mipi-i3c-hci/ibi.h nodes=2 lang=c
  drivers/i3c/master/mipi-i3c-hci/mipi-i3c-hci-pci.c nodes=45 lang=c
  drivers/i3c/master/mipi-i3c-hci/pio.c nodes=41 lang=c
  drivers/i3c/master/mipi-i3c-hci/xfer_mode_rate.h nodes=1 lang=c
  drivers/i3c/master/renesas-i3c.c nodes=82 lang=c
  drivers/i3c/master/svc-i3c-master.c nodes=87 lang=c
```

## Build System
(TODO)

## Architecture Layers

| Layer | Nodes | Description |
|-------|-------|-------------|
| **interface** | 2 | Userspace interface |
| **interrupt** | 31 | Interrupt handling |
| **dma** | 21 | DMA engine |
| **core** | 501 | Core logic |

## Entry Points

- `hci_extcap_hardware_id()`
- `i3c_device_do_xfers()`
- `hci_dat_w0_write()`
- `hci_extcap_master_config()`
- `hci_dat_w1_write()`
- `hci_extcap_multi_bus()`
- `hci_extcap_xfer_modes()`
- `i3c_device_do_setdasa()`
- `ast2600_i3c_init()`
- `intel_ltr_set()`

## Key Symbols (top by call complexity)

| Symbol | Call Count |
|--------|-----------|
| `renesas_writel()` | 44 |
| `renesas_set_bit()` | 25 |
| `i3c_master_add_i3c_dev_locked()` | 22 |
| `renesas_clear_bit()` | 22 |
| `i3c_bus_normaluse_lock()` | 21 |
| `i3c_bus_normaluse_unlock()` | 21 |
| `i3c_master_register()` | 20 |
| `renesas_i3c_hw_init()` | 20 |
| `svc_i3c_master_ibi_isr()` | 20 |
| `i3c_bus_set_addr_slot_status()` | 18 |
| `to_dw_i3c_master()` | 18 |
| `to_svc_i3c_master()` | 18 |
| `to_adi_i3c_master()` | 16 |
| `to_i3c_hci()` | 16 |
| `to_cdns_i3c_master()` | 15 |
| `dev_to_i3cbus()` | 14 |
| `i3c_master_get_free_addr()` | 14 |
| `i3c_master_bus_init()` | 14 |
| `i3c_master_send_ccc_cmd_locked()` | 13 |
| `renesas_i3c_rx_isr()` | 13 |

## Related Concepts
(Populated by `kb bind propose`)

## Generated Graphs
See: `kb graph export linux-i3c`

## Counter-Arguments and Gaps

...
