# LLM Wiki Log

<!-- Append only. Never edit existing entries. Format:
## [YYYY-MM-DD] ingest | Title
One-line description.
-->

## [2026-05-28] ingest | PCI Express 体系结构导读
Saved book source to raw/articles/pci_express/merged.md.

## [2026-05-28] compile | 1 source → 17 pages
Compiled PCI Express 体系结构导读. Created/updated 17 pages: source-summary, author, 11 concept pages, 4 stub pages (capric-card, pci-virtualization, mesif-protocol, powerpc).

## [2026-05-28] ingest | 第1章 PCI总线的基本知识 (part 1)
Ingested Chapter 1 part 1 from 《PCI Express 体系结构导读》— PCI bus fundamentals, composition, signal definitions.

## [2026-05-28] ingest | bulk: 全書剩餘 38 個章節檔案
Bulk-ingested all remaining chapter files (00-preface + ch.01-p3~ch.15-p2) with frontmatter. Total: 39 files (incl. 01-pci-basics_p1) now have compiled: false, ready for compile.

## [2026-05-28] compile | 39 sources → 21 pages
Compiled all 39 chapter files. Updated source-summary (compiled: true). Created 4 new pages: host-bridge, pcie-ordering, pcie-ecam, pcie-ats. Massively expanded all 13 existing concept pages with detailed content from chapter splits. Updated index with 21 entries.

## [2026-05-28] lint | full wiki audit
Ran deterministic lint script + manual checks. Fixed 1 dead link (created pci-interrupt-mechanism page). All 21 pages clean: no stale pages, no orphans, no contradictions, no index drift. Report: outputs/reports/2026-05-28-lint.md.

## [2026-05-28] query | pcie-switch-firmware-storage
Answered question about PCIe Switch firmware storage locations. Referenced 4 pages (pci-express-体系结构导读, pci-bridge, pci-express, capric-card). Filed to queries/pcie-switch-firmware-storage.md.

## [2026-05-28] promote | pcie-switch-firmware-storage
Promoted query answer to wiki/pcie-switch-firmware-storage.md as a first-class concept page.

## [2026-05-28] query | pcie-physical-vs-transaction-layer
Answered question. Referenced 4 pages. Filed to queries/pcie-physical-vs-transaction-layer.md.

## [2026-05-29] walkthrough | Edge PCIe Driver
Walkthrough of pcie driver (x6000-v0.4.0): created architecture page ([[edge-pcie-driver]]) and module page ([[edge-pcie-core]]), backlinked to 8 existing concept pages including [[msi-msi-x]], [[linux-pci-subsystem]], [[pci-interrupt-mechanism]].

## [2026-05-29] query | how-to-enable-msi-in-linux
Cross-code-and-theory query: "如何在 Linux 中启用 MSI 中断" — synthesized answer citing [[msi-msi-x]], [[linux-pci-subsystem]], [[pci-interrupt-mechanism]], [[edge-pcie-core]]. Filed to wiki/queries/how-to-enable-msi-in-linux.md.

## [2026-05-29] lint | full wiki audit
Ran deterministic lint script + manual checks. Fixed 1 issue: pcie-switch-firmware-storage.md — renamed section to `## Counter-Arguments and Gaps`. All 26 pages clean: no dead links, no orphans, no contradictions, no stale pages, no index drift. Report: outputs/reports/2026-05-29-lint.md.

## [2026-05-29] ingest | IOMMU Tutorial @ ASPLOS 2016
Ingested IOMMU tutorial paper (PDF) from raw/articles/iommu/IOMMU_TUTORIAL_ASPLOS_2016.pdf — Virtualizing IO Through the IOMMU by Kegel, Blinzer, Basu, Chan. Saved to raw/articles/2026-05-29-iommu-tutorial-asplos-2016.md.

## [2026-05-29] compile | 1 source → 8 pages
Compiled IOMMU Tutorial @ ASPLOS 2016. Created/updated 8 pages: source-summary (iommu-tutorial-asplos-2016), 2 new concept pages (iommu, interrupt-remapping), 4 person pages (andy-kegel, paul-blinzer, arka-basu, maggie-chan), updated 3 existing pages (pci-virtualization, pcie-ats, root-complex).

## [2026-05-29] ingest | IOMMU 授课台词
Ingested lecture script based on IOMMU Tutorial @ ASPLOS 2016 (74 slides, ~90min). Saved to raw/articles/2026-05-29-iommu-tutorial-script.md.

## [2026-05-29] compile | 1 source → 1 page
Compiled IOMMU 授课台词. Created source-summary page (iommu-tutorial-script).

## [2026-05-29] ingest | DWC_avsbus Databook
Ingested Synopsys AVSBus Controller Databook v1.00a (May 2025) — Adaptive Voltage Scaling Bus controller IP document. Saved to raw/articles/2026-05-29-dwc-avsbus-databook.md.

## [2026-05-29] compile | 1 source → 2 pages
Compiled DWC_avsbus Databook. Created source-summary (dwc-avsbus-databook) and concept page (avsbus).

## [2026-06-06] ingest | Understand Anything
Ingested tool documentation: Understand Anything — 多智能体代码分析管道. Saved to raw/articles/2026-06-06-understand-anything.md.

## [2026-06-06] compile | 1 source → 2 pages
Compiled Understand Anything documentation. Created source-summary (2026-06-06-understand-anything) and concept page (understand-anything).

## [2026-06-06] ingest | MIPI I3C documentation (3 sources)
Extracted PDFs from i3c repo doc/: Introduction (NXP, 2017), Chinese translation doc, and I3C Basic spec v1.1.1. Saved to raw/articles/i3c/.

## [2026-06-06] compile | 3 sources → 5 pages
Compiled I3C documentation. Created 2 source-summaries (mipi-i3c-introduction, mipi-i3c-chinese-doc), concept page (i3c), and 2 stub pages (mipi-i3c-basic-spec, i2c).

## [2026-06-05] query | how-to-enable-msi-in-linux (复查)
Cross-domain query复查: "如何在 Linux 中启用 MSI 中断" — 综合 [[how-to-enable-msi-in-linux]]（原始答案）、[[msi-msi-x]]、[[linux-pci-subsystem]]、[[edge-pcie-core]] 四页内容。Filed to wiki/queries/how-to-enable-msi-in-linux-v2.md.

## [2026-06-05] promote | linux-msi-enable
Promoted query answer (v2) to wiki/linux-msi-enable.md as a first-class concept page covering driver API, device-side MSI generator, and kernel interrupt lifecycle.

## [2026-06-05] lint | full wiki audit
Deterministic: OK. Manual: 0 dead links, 0 orphans, 0 contradictions, no index drift. 2 draft stubs (mesif-protocol, powerpc). 16 empty Counter-Arguments (normal). Report: outputs/reports/2026-06-05-lint.md.