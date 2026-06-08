---
date: 2026-05-29
tags: [iommu, transcript]
type: source-summary
source-url: raw/articles/2026-05-29-iommu-tutorial-script.md
title: "IOMMU 授课台词 — 基于 ASPLOS 2016 Tutorial"
status: compiled
---

# IOMMU 授课台词

Lecture script for a ~90min class on IOMMU, based on the ASPLOS 2016 tutorial. Written from the perspective of a chip design / driver engineer. Covers motivation, use cases, internals, and research directions.

## Key Topics
- IOMMU fundamentals: DMA remapping, interrupt remapping, PASID, ATS/PRI
- Virtualization: device pass-through, nested translation, posted interrupts
- Heterogeneous computing: HSA, Shared Virtual Memory (SVM)
- Use cases: legacy IO, security, secure boot, device pass-through
- Research: rIOMMU, IOMMULite, interference, third-party accelerator isolation
