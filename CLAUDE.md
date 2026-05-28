# docs_writing Vault — Operating Instructions

You are operating in an **Obsidian vault** that also serves as an **LLM Wiki** — a persistent, compounding knowledge base following the Karpathy LLM Wiki pattern.

## LLM Wiki Mode

This vault implements the **LLM Wiki pattern**: raw sources are ingested, compiled into structured wiki pages with `[[wikilinks]]`, queried with filed answers, and periodically linted for gaps.

### Wiki Location
The active wiki is at: `LLM_Wiki/` (relative to vault root)

### Active Wiki Detection
When performing wiki operations:
1. Check if `LLM_Wiki/CLAUDE.md` and `LLM_Wiki/wiki/` exist → active wiki found
2. Read `LLM_Wiki/CLAUDE.md` for the full schema before operating
3. All wiki pages go under `LLM_Wiki/wiki/`
4. All raw sources go under `LLM_Wiki/raw/`

### Core Operations

| Operation | Description |
|-----------|-------------|
| **Ingest** | Save a source (file or URL) to `raw/articles/`. Does NOT create wiki pages. |
| **Compile** | Read raw sources, extract entities, create/update wiki pages with cross-references. |
| **Query** | Search the wiki, synthesize answer with `[[wikilink]]` citations. Always file to `wiki/queries/`. |
| **Lint** | Audit for dead links, orphans, contradictions, index drift. |
| **Promote** | Move a filed query answer from `wiki/queries/` to `wiki/` as a first-class concept page. |

### Key Rules
- **`raw/` is sacred** — never edit files in `raw/`. LLM reads but never writes there.
- **`wiki/` is LLM-owned** — full write access for creating/updating pages.
- **`log.md` is append-only** — never edit existing entries.
- **Backlink audit after every compile** — grep for mentions of new page titles and add missing `[[wikilinks]]`.
- **Flag contradictions** with `> [!WARNING] Contradiction with [[other-page]]`.

### Available Skills
- **llm-wiki** (`.claude/skills/llm-wiki/SKILL.md`) — wiki 核心操作：ingest / compile / query / lint
- **file-prep** (`.claude/skills/file-prep/SKILL.md`) — 大文件预处理：图片迁移、路径重写、文件切片
  - 自动在文件 >50KB 或含嵌入图片时启用
  - 命令格式: `prep <文件路径> [--source-name <名称>]`

## Skill 管理规则

每次新增或更新 Skill，必须同步执行以下操作：

1. 在 `skills/<skill-name>/` 目录下保存一份副本（与 `.claude/skills/<skill-name>/SKILL.md` 同步）
2. 在 `skills/<skill-name>/` 下写入 `USAGE.md` — 即该 Skill 的使用说明，包含：
   - 命令格式
   - 使用场景
   - 操作步骤
   - 实际示例

这样 Skill 文件在 Obsidian 中可见可读，不隐藏在 `.claude/` 目录下。

## Vault Notes
- This vault contains both personal notes and engineering documentation (baremetal SoC verification).
- The `AGENTS.md` file is for Codex working on a baremetal project — do not modify it.
- Use `[[wikilinks]]` for internal cross-references.
- Use Dataview-compatible YAML frontmatter for metadata.
