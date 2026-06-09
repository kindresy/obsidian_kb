# Wiki Consolidate — 使用说明

## 概述

批量处理 `raw/articles/` 中所有未入库的原始资料。一条命令完成 **审计 → 录入 → 编译 → 检核 → 报告** 全流程。

## 命令格式

```
wiki consolidate              — 全量处理（自动跳过已编译源）
wiki consolidate --dry-run    — 仅预览，不执行任何写入
wiki consolidate --force      — 强制重新编译所有源
```

## 使用场景

| 场景 | 示例 |
|------|------|
| 批量导入多个笔记后 | 刚在 `raw/articles/i3c/` 下新增了 8 篇笔记，一次全部入库 |
| 新环境同步后 | 从 Git 拉取后首次运行，让 wiki 与 raw 同步 |
| 定期维护 | 每周运行一次，检查是否有遗漏未编译的源 |
| 不确定状态时 | 先 `--dry-run` 预览计划操作 |

## 操作流程

```
wiki consolidate
  │
  ├─ Phase 0: 审计扫描 (raw-audit.py)
  │   → 识别 UNKNOWN (需ingest) + UNCOMPILED (需compile)
  │
  ├─ Phase 1: Ingest（为 UNKNOWN 文件添加 frontmatter）
  │   → 分类 → 加 frontmatter → 记 log
  │
  ├─ Phase 2: Compile（所有待编译源）
  │   → 读源 → 创建/更新 source-summary → 实体提取
  │   → 创建/更新概念页 → 加 [[wikilinks]] → 更新 index.md
  │
  ├─ Phase 3: 后处理
  │   → Backlink 审计 → 重建向量索引 → 记 log
  │
  └─ Phase 4: Lint + 报告
      → lint-wiki.py → 修复问题 → 写 consolidate 报告
```

## 实际示例

### 预览模式

```
wiki consolidate --dry-run
```

输出：

```
[DRY RUN] Wiki Consolidate — 2026-06-09

raw-audit results:
  UNKNOWN (5):  raw/articles/i3c/第三章讲解.md ...
  UNCOMPILED (2): raw/articles/2026-05-29-dwc-avsbus-databook.md ...

Plan:
  Ingest:  5 files → add frontmatter
  Compile: 7 sources → ~15 new/updated pages
  Lint:    run + report
```

### 全量执行

```
wiki consolidate
```

## 注意事项

1. **UNKNOWN 文件加 frontmatter 是例外操作**——raw/ 原则上是只读的，但这些文件未经正规 ingest 流程，加 frontmatter 是唯一使其可被 compile 的方法。后续不会再修改。
2. **PDF 文件不 compile**——PDF 二进制的特性使 LLM 可读取但无法精确提取实体，仅记录其存在。
3. **同领域批量编译**——多个相关源（如同一目录下的多个笔记）会一起读取，统一提取实体，避免重复创建概念页。
4. **log.md 完整性**——每次 consolidate 操作都会记入 log，格式：
   ```
   ## [YYYY-MM-DD] consolidate | N sources processed
   Ingested N files, compiled N sources, updated N wiki pages.
   ```
