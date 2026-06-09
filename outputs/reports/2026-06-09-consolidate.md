# Wiki Consolidate Report — 2026-06-09

## Summary

| Metric | Value |
|--------|-------|
| UNKNOWN files ingested | 16 |
| UNCOMPILED files compiled | 3 |
| Sources skipped (already compiled) | 0 |
| New wiki pages created | 7 |
| Wiki pages updated | 3 |
| Vector index rebuilt | yes (61 pages, 324 chunks) |
| Lint issues found/fixed | 3 found / 3 fixed |

## Detail

### Ingested (frontmatter added)
| Source | Type |
|--------|------|
| `raw/articles/i3c/第四章讲解.md` | note |
| `raw/articles/i3c/第三章讲解.md` | note |
| `raw/articles/i3c/第四章节第二部分讲解.md` | note |
| `raw/articles/i3c/I3C讲义_驱动组内分享.md` | article |
| `raw/articles/i3c/Broadcast CCC典型用法.md` | note |
| `raw/articles/i3c/Slave终止read能力解析.md` | note |
| `raw/articles/i3c/学习建议.md` | note |
| `raw/articles/tools/understand-anything.md` | article |
| `raw/articles/tools/工程知识库实施步骤.md` | article |
| `raw/articles/tools/工程知识库搭建思路.md` | article |
| `raw/articles/pci_express/merged.md` | book |
| `raw/articles/pcie_others/PCIe Resize Bar.md` | article |
| `raw/articles/avsbus/avsbus_extracted.md` | article |
| `raw/articles/i2c/at24c16c 地址计算.md` | note |
| `raw/articles/Humanities_and_Reflection/GPT对于《禅与摩托车维修艺术》的理解.md` | article |
| `raw/articles/Humanities_and_Reflection/圆觉经.md` | article |

### Compiled (wiki pages created)
| Source | Pages Created |
|--------|-------------|
| I3C 讲解笔记 (7 files) | `wiki/concepts/i3c-lecture-notes.md` (source-summary) |
| Linux TTY notes | `wiki/concepts/linux-tty-notes.md` (source-summary), `wiki/concepts/linux-tty.md` (concept) |
| 工程知识库搭建思路/实施步骤 | `wiki/concepts/engineering-kb-plan.md` (source-summary) |
| Humanities notes | `wiki/concepts/humanities-notes.md` (source-summary) |
| PCIe Resize BAR | `wiki/concepts/pcie-resize-bar.md` (concept) |

### Updated (wiki pages modified)
| Page | Changes |
|------|---------|
| `wiki/concepts/i3c.md` | Expanded with: bus architecture diagram, terminology evolution, frame structure detail, T-bit semantics, Broadcast CCC table, Bus Conditions, DAA details, bus config types, IBI priority |
| `wiki/concepts/i2c.md` | Added AT24C16C EEPROM example |
| `wiki/tools/kb-usage.md` | Added See Also link to engineering-kb-plan |
| `wiki/concepts/linux-tty.md` | Added See Also link to linux-tty-notes |

### Lint
| Issue | Status |
|-------|--------|
| DEAD_LINK: `[[pci-express体系结构导读]]` in pcie-resize-bar.md | FIXED — missing hyphen, corrected to `[[pci-express-体系结构导读]]` |
| ORPHAN: engineering-kb-plan.md | FIXED — added link from kb-usage.md |
| ORPHAN: linux-tty-notes.md | FIXED — added link from linux-tty.md |

### Vector Index
- 61 pages → 324 chunks
- Embedding model: all-MiniLM-L6-v2 (384-dim)
- Index location: `LLM_Wiki/.vector_index/`
