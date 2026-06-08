---
date: 2026-05-29
tags: [iommu, virtualization, pcie]
type: source-summary
source-url: raw/articles/iommu/IOMMU_TUTORIAL_ASPLOS_2016.pdf
title: "Virtualizing IO Through the IO Memory Management Unit (IOMMU)"
status: compiled
---

# IOMMU Tutorial @ ASPLOS 2016

Tutorial by Andy Kegel, Paul Blinzer, Arka Basu, Maggie Chan at ASPLOS 2016. Covers IOMMU motivation, use cases, internals, and research opportunities.

## Key Topics
- IOMMU fundamentals: DMA remapping, interrupt remapping, address translation
- Virtualization: device pass-through, SR-IOV, guest physical address translation
- Performance: bounce buffers, IOTLB caching, ATS/PRI
- Heterogeneous systems: SVM, HSA, shared page tables
- IOMMU families: AMD IOMMU, Intel VT-d, ARM SMMU, IBM CAPI
