# Wiki Consolidate Report — 2026-06-10

## Summary

| Metric | Value |
|--------|-------|
| UNKNOWN files ingested | 8 |
| UNCOMPILED files compiled | 2 (zhihu articles) |
| Sources skipped (already compiled) | 9 |
| New wiki pages created | 4 |
| Wiki pages updated | 2 |
| Vector index rebuilt | yes (64 pages, 334 chunks) |
| Lint issues found/fixed | 0 (8 RAW_MODIFIED, temporary) |

## Detail

### Ingested
| Source | Type |
|--------|------|
| `raw/articles/i3c/I3C_Basic_Spec_组内分享.md` | note |
| `raw/articles/i3c/spce学习-2-初始化顺序.md` | note |
| `raw/articles/i3c/spec学习-1.md` | note |
| `raw/articles/i3c/spec学习-2.1-初始化寄存器配置.md` | note |
| `raw/articles/i3c/spec学习-3-ENTDAA在IP中怎么落地.md` | note |
| `raw/articles/i3c/spec学习-4-transfer编程.md` | note |
| `raw/articles/i3c/spec学习-5-IBI机制.md` | note |
| `raw/articles/i3c/spec学习-6-HDR.md` | note |

### Compiled
| Source | Pages Created |
|--------|-------------|
| zhihu PCIe Link Up article | `wiki/concepts/zhihu-pcie-link-up.md` (source-summary) |
| zhihu Linux DMA API article | `wiki/concepts/zhihu-linux-dma-api.md` (source-summary) |
| Linux DMA API concept | `wiki/concepts/linux-dma-api.md` (concept) |

### Updated
| Page | Changes |
|------|---------|
| `wiki/concepts/pcie-link-training.md` | Added See Also → zhihu-pcie-link-up |
| `wiki/index.md` | Added Linux/DMA section; updated date |

### Lint
- All 8 issues are RAW_MODIFIED (expected — from adding frontmatter to raw files)
- No dead links, no orphans, no missing sections

### Vector Index
- 64 pages → 334 chunks (up from 61/324)
