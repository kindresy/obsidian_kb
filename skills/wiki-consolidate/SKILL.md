---
name: wiki-consolidate
description: >-
  Batch process all un-ingested and un-compiled raw sources in one pass.
  Run raw-audit, ingest UNKNOWN files with frontmatter, compile UNCOMPILED
  sources into wiki pages, backlink audit, rebuild vector index, lint, and
  generate a consolidation report.
argument-hint: [--dry-run | --force]
---

# Wiki Consolidate — 批量预处理入口

将 `raw/articles/` 中所有未处理的原始资料一次性完成 **ingest → compile → lint → report** 全流程。

## 使用场景

| 场景 | 说明 |
|------|------|
| 批量导入多个笔记/文档后 | 一次性全部纳入 wiki 体系 |
| 从其他系统迁移资料后 | 无需逐个手动 ingest + compile |
| GitHub clone 后的首次初始同步 | 新环境需要建立完整知识库 |
| 定期维护 | 检查是否有遗漏的 raw source |

## 用法

```
wiki consolidate              — 全量处理（默认模式）
wiki consolidate --dry-run    — 仅预览，不执行任何写入
wiki consolidate --force      — 强制重新编译已编译的源（跳过已编译的 source-summary）
```

---

## 工作流程

### Phase 0：审计扫描

```bash
python3 LLM_Wiki/raw-audit.py
```

读取审计结果，分三类处理：

| 审计状态 | 含义 | 处理方式 |
|----------|------|---------|
| **UNKNOWN** | 文件无 frontmatter，直接丢入 `raw/` | 先 **ingest**（加 frontmatter），再 **compile** |
| **UNCOMPILED** | 有 `compiled: false` 或所在 group 无 summary | 直接 **compile** |
| **OK** | 已有对应的 source-summary | **跳过** |

### Phase 1：Ingest（仅 UNKNOWN 文件）

对每个 UNKNOWN 文件：

1. **读取文件内容**，识别标题（# 第一行或文件名推断）
2. **判断 source-type**：`article` / `paper` / `note` / `transcript`
3. **为文件添加 frontmatter**：
   ```yaml
   ---
   date: YYYY-MM-DD
   source-type: <classification>
   title: <inferred title>
   compiled: false
   ---
   ```
4. **记录日志**：追加到 `log.md`

> ⚠️ 虽然 `raw/` 神圣不可修改，但 UNKNOWN 文件是未经正规 ingest 流程的"野文件"。添加 frontmatter 使其可被 compile 识别是必要的合规操作。

### Phase 2：Compile（所有 UNCOMPILED + 新 ingest 的文件）

按 `llm-wiki` skill 中 `compile` 的定义对每个源执行：

1. **读 raw source** 内容
2. **创建/更新 source-summary 页面**（`wiki/concepts/`）
3. **实体提取**：扫描文章中的关键概念、人物、协议术语
   - 已有 → 更新，保留原有内容
   - 没有 → 创建新概念页
4. **加 `[[wikilinks]]`**：新页面之间、新页面与已有页面之间双向链接
5. **更新 `wiki/index.md`**：在对应的领域分类下添加条目

> **批量编译规则**：如果多个源属于同一领域（如 i3c/ 下的多个笔记），应一次全部读取后统一提取实体，避免对同一概念反复创建重复页面。

### Phase 3：后处理（全局一次性执行）

在所有 compile 完成后：

1. **Backlink 审计**：grep 所有已有 wiki 页面，查找提及新页面标题但未加 `[[wikilinks]]` 的地方，补上链接
2. **重建向量索引**：
   ```bash
   python3 LLM_Wiki/build-index.py
   ```
3. **追加 `log.md`**

### Phase 4：Lint + 报告

1. **运行确定性 lint 脚本**：
   ```bash
   python3 LLM_Wiki/lint-wiki.py LLM_Wiki/wiki/
   ```
2. **修复发现的问题**：死链 → 建 stub 页；孤立页 → 加反向链接；缺 section → 补空节
3. **写报告**到 `outputs/reports/YYYY-MM-DD-consolidate.md`

### 报告格式

```
# Wiki Consolidate Report — YYYY-MM-DD

## Summary
- UNKNOWN files ingested:  N
- UNCOMPILED files compiled: N
- Sources skipped (already compiled): N
- New wiki pages created: N
- Wiki pages updated: N
- Vector index rebuilt: yes/no
- Lint issues found/fixed: N

## Detail
### Ingested
- raw/articles/xxx.md → frontmatter added (type: article)

### Compiled
- xxx.md → source-summary + N new pages

### Lint
- [FIXED] dead link: [[yyy]] → created stub
- [OK] no orphans
```

## 注意与约束

| 约束 | 说明 |
|------|------|
| **raw/ 不可变性** | UNKNOWN 文件添加 frontmatter 是例外操作，仅此一次。之后该文件不再修改 |
| **重复概念检测** | compile 前先搜索 wiki/ 中是否已有同名/同义页面，避免重复创建 |
| **跨领域引用** | 新创建的概念页若引用其他领域的已有页面，必须加 `[[wikilinks]]` |
| **HDR/TSP 等复杂主题** | 对于 i3c/ 下密集的笔记文件，建议先全部读完再统一决定实体边界 |
| **PDF/图片附件** | PDF 不 compile（不可读），仅记录其存在。图片附件 (`raw/attachments/`) 不受影响 |
| **--dry-run** | 只打印审计结果和计划操作，不写文件、不改 frontmatter、不创建 wiki 页面 |
| **log.md 完整性** | 每次 log 追加遵循 `## [YYYY-MM-DD] consolidate | <title>` 格式 |
