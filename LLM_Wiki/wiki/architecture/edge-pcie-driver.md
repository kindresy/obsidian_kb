---
date: 2026-05-29
tags: [code, c, pci, linux]
type: architecture
status: active
source-repo: edge-driver
---

# Edge PCIe Driver Architecture

The Edge PCIe driver is a Linux loadable kernel module for Edge SoCs (x6000 series), providing PCIe endpoint functionality with uDMA engine, MSI/MSI-X interrupt support, and character device interface for userspace communication. It supports PCIe peer-to-peer (P2P) transfers, boot-time DMA acceleration, SMBus management, hardware exception monitoring, and LED control.

## Directory Map

```
raw/code/pcie/
├── edge.c         — Main driver source (PCI probe/remove, DMA, IRQ, IOCTL, sysfs)
├── edge.h         — Header: data structures, register defs, IOCTL constants, enums
├── Makefile       — kbuild integration (obj-m += edge.o)
├── version.h      — Auto-generated version string (x6000-v0.4.0)
├── README.md      — Build instructions (x86 host, arm64 cross-compile)
└── SUMMARY.md     — Walkthrough metadata
```

## Build System

- **Type**: Linux Kernel Module Makefile (kbuild)
- **Entry**: `Makefile` — invokes `$(MAKE) -C $(KER_DIR) M=$(DRV_DIR) modules`
- **Target kernel**: `/lib/modules/$(uname -r)/build` (or cross-compile via `ARCH=arm64 CROSS_COMPILE=aarch64-none-linux-gnu- KER_DIR=...`)
- **Output**: `edge.ko` — single kernel module binary

## Module Graph

- [[edge-pcie-core]] — single monolithic module covering all driver responsibilities (PCI, DMA, IRQ, char device, SMBus, exception handling)

## Architecture Layers

The driver consists of 13 architectural layers, with ~270 cross-layer call relationships:

<!-- CODEGRAPH: --layers -->

```mermaid
graph TD
    entry["Entry Points (2 funcs)"]
    style entry fill:#1a237e,color:#fff
    interface["Userspace Interface (42 funcs)"]
    style interface fill:#01579b,color:#fff
    interrupt["Interrupt Handling (15 funcs)"]
    style interrupt fill:#bf360c,color:#fff
    dma["DMA Engine (48 funcs)"]
    style dma fill:#1b5e20,color:#fff
    hardware["Hardware Abstraction (5 funcs)"]
    style hardware fill:#4a148c,color:#fff
    smbus["SMBus Management (15 funcs)"]
    style smbus fill:#e65100,color:#fff
    monitor["Exception Monitor (13 funcs)"]
    style monitor fill:#b71c1c,color:#fff
    boot["Boot Acceleration (1 funcs)"]
    style boot fill:#004d40,color:#fff
    config["Configuration (1 funcs)"]
    style config fill:#3e2723,color:#fff
    core["Core Logic (31 funcs)"]
    style core fill:#37474f,color:#fff
    dma -->|ioctl_do_udma_p2pob_xfer → edge_readl_safe| hardware
    smbus -->|smbus_ioctl → ioctl_do_hot_reset_bus_unlock| interface
    core -->|edge_ioctl → ioctl_do_get_ep_flags| interface
    interface -->|ioctl_do_boot_read → boot_wait_state| boot
    core -->|engine_service_resume → udma_start_engine| dma
    core -->|edge_ioctl → ioctl_do_exception_owner| monitor
    interface -->|ioctl_do_write_ext_addr → edge_readl_safe| hardware
    entry -->|edge_probe → edge_dma_pool_destory| dma
    core -->|edge_set_public_data → edge_readl_safe| hardware
    entry -->|edge_probe → edge_device_online| core
    core -->|(void) → smbus_misc_register| smbus
    interface -->|ioctl_do_free_block_mem → edge_dma_pool_free| dma
    entry -->|edge_probe → edge_create_cdev| interface
    interrupt -->|edge_pcie_host_notify_irq_deinit → edge_readl_safe| hardware
    dma -->|edge_udma_poll_state → engine_service_resume| core
    entry -->|edge_remove → edge_teardown_irqs| interrupt
    monitor -->|ioctl_do_config_exception → edge_readl_safe| hardware
    interface -->|edge_open → edge_enable_irq_vectors| interrupt
    interrupt -->|edge_pcie_host_notify_irq_send → edge_test_bit| core
    smbus -->|smbus_ioctl → ioctl_do_p2p_init| dma
    config -->|edge_led_mode → edge_readl_safe| hardware
    core -->|edge_event_timer_work → edge_setup_irqs| interrupt
    interface -->|edge_create_cdev → edge_set_public_data| core
    interface -->|ioctl_do_led → edge_led_mode| config
    interrupt -->|edge_setup_irqs → edge_setup_dma_int_bridge| dma
    entry -->|edge_remove → edge_unmap_bars| hardware
    boot -->|boot_wait_state → edge_readl_safe| hardware
    dma -->|ioctl_do_boot_udma_h2d_xfer → boot_wait_state| boot
    entry -->|edge_probe → edge_exception_event_init| monitor
```

| Layer | Nodes | Description |
|-------|-------|-------------|
| **entry** | 2 | Probe/remove, module init/exit |
| **interface** | 47 | IOCTL dispatch, char device, mmap |
| **interrupt** | 20 | MSI/MSI-X/INTx, ISR registration |
| **dma** | 69 | uDMA engines, scatter-gather, P2P |
| **hardware** | 8 | Register access, PCI config |
| **smbus** | 23 | SMBus arbitration, hotplug |
| **monitor** | 37 | Exception FIFO, error handling |
| **boot** | 10 | Pre-OS DMA acceleration |
| **config** | 5 | Module params, LED control |
| **core** | 33 | General orchestration |
| **data** | 36 | Struct/enum definitions |
| **dependency** | 16 | Kernel header imports |

## Key Concepts Touched

- [[linux-pci-subsystem]] — Linux PCIe driver model: `pci_driver` probe/remove, `pci_enable_device`, `pci_request_regions`, `pci_set_master`
- [[msi-msi-x]] — MSI/MSI-X interrupt vector allocation (`pci_enable_msix_range`, `pci_alloc_irq_vectors_affinity`) and message generation
- [[pci-interrupt-mechanism]] — Fallback to legacy INTx when MSI/MSI-X unavailable; `IRQF_SHARED` for exception events
- [[pci-express]] — PCIe Address Translation (AT) for inbound/outbound region remapping; BAR management
- [[pci-bus]] — DMA operations (bus master enable, 64-bit DMA mask), scatter-gather DMA
- [[pcie-ecam]] — PCIe config space access via `pci_read_config_dword` (MSI capability registers)
- [[pci-virtualization]] — ACS redirection disable (P2P), SR-IOV interrupt registers
- [[pcie-link-training]] — `pcie_bus_configure_settings` for link speed/width optimization
