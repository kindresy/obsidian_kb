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

## Key Concepts Touched

- [[linux-pci-subsystem]] — Linux PCIe driver model: `pci_driver` probe/remove, `pci_enable_device`, `pci_request_regions`, `pci_set_master`
- [[msi-msi-x]] — MSI/MSI-X interrupt vector allocation (`pci_enable_msix_range`, `pci_alloc_irq_vectors_affinity`) and message generation
- [[pci-interrupt-mechanism]] — Fallback to legacy INTx when MSI/MSI-X unavailable; `IRQF_SHARED` for exception events
- [[pci-express]] — PCIe Address Translation (AT) for inbound/outbound region remapping; BAR management
- [[pci-bus]] — DMA operations (bus master enable, 64-bit DMA mask), scatter-gather DMA
- [[pcie-ecam]] — PCIe config space access via `pci_read_config_dword` (MSI capability registers)
- [[pci-virtualization]] — ACS redirection disable (P2P), SR-IOV interrupt registers
- [[pcie-link-training]] — `pcie_bus_configure_settings` for link speed/width optimization
