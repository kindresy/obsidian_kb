# LLM Wiki Schema

## Directory Layout
- `raw/` — immutable source drops. Never edit files here.
- `raw/articles/` — text source documents (articles, papers, transcripts).
- `raw/attachments/` — images and binary attachments.
- `raw/code/` — immutable code repository drops (cloned or copied sources).
- `wiki/` — LLM-owned pages. You have full write access here.
- `wiki/index.md` — catalog. Read this FIRST before opening any other page.
- `wiki/queries/` — filed query answers. Promote to `wiki/` when durable.
- `wiki/modules/` — code module pages (one per logical module in a repo).
- `wiki/architecture/` — repository architecture overview pages (one per repo).
- `outputs/reports/` — dated lint reports and other artifacts.
- `log.md` — append-only operation log. Never edit existing entries.

## Entity Types and Templates

### concept.md
```yaml
---
date: YYYY-MM-DD
tags: [domain]
type: concept
status: active
---
```
- Body: one-paragraph summary, `## Details`, `## See Also` with `[[wikilinks]]`, `## Counter-Arguments and Gaps`

### person.md
```yaml
---
date: YYYY-MM-DD
tags: [domain, person]
type: person
status: active
---
```

### source-summary.md
```yaml
---
date: YYYY-MM-DD
tags: [domain]
type: source-summary
source-url: https://...
---
```

### query-output.md
```yaml
---
date: YYYY-MM-DD
tags: [domain]
type: query-output
question: "<original question>"
status: filed
---
```

### module.md
```yaml
---
date: YYYY-MM-DD
tags: [code, <language>, <domain>]
type: module
status: active
source-repo: <repo-name>
source-path: <relative-path>
---
```
- Body: one-paragraph summary, `## Responsibilities` (bullet list), `## Key Interfaces` (table),
  `## Dependencies` with `[[wikilinks]]` to other modules and concept pages,
  `## Data Flow` (text or ASCII diagram), `## Code References` (file paths with line ranges).

### architecture.md
```yaml
---
date: YYYY-MM-DD
tags: [code, <language>, <domain>]
type: architecture
status: active
source-repo: <repo-name>
---
```
- Body: one-paragraph overview, `## Directory Map` (tree), `## Build System` (type, entry, targets),
  `## Module Graph` with `[[wikilinks]]` showing dependencies between modules,
  `## Key Concepts Touched` with `[[wikilinks]]` to existing concept pages.

## Naming Conventions

### Filenames
- All filenames: `lowercase-kebab-case.md` — English only, no Chinese, no spaces
- Exception: deliberately kept short, stable, and script-friendly
- Examples: `msi-msi-x.md` ✓, `pcie-switch-firmware-storage.md` ✓, `pci-express-体系结构导读.md` ← existing exception

### Titles
- Can be Chinese for readability: `# MSI/MSI-X 中断机制`
- Keep consistent with the page's core topic

### Body Language
- Chinese as primary language
- English terms retained as-is (e.g., "MSI/MSI-X", "BAR", "TLP")
- No translation of standard technical terms

### Wikilinks
- Always use the file slug (without `.md`): `[[msi-msi-x]]`
- Never use Chinese title text as the link target: `[[MSI/MSI-X 中断机制]]` ✗
- For display readability, use the pipe alias syntax:
  - `[[msi-msi-x|MSI/MSI-X 中断机制]]` — renders as "MSI/MSI-X 中断机制"
  - `[[edge-pcie-core|Edge PCIe 核心模块]]` — renders as "Edge PCIe 核心模块"
- Never use standard markdown links `[text](url)` for internal references

## Log Format
Append to `log.md` after every operation. Format:
```
## [YYYY-MM-DD] <operation> | <title>
<one-line description>
```
Operations: `ingest | compile | query | lint | promote | remove | walkthrough`

## Index Format
`wiki/index.md` is a human- and LLM-readable catalog. Format:
```
## Domain Name
- [[page-name]] — one-line description (YYYY-MM-DD)
```
Keep entries under 80 chars. Update after every compile/ingest.

## Cross-Reference Rules
- Every page must link to at least one other page when content warrants it
- When creating or updating a concept page, scan `wiki/index.md` for related entities and add `[[wikilinks]]`
- Flag contradictions inline: `> [!WARNING] Contradiction with [[other-page]]`

## Ingest Rules
1. Acquire the source (URL or file)
2. Classify the source type
3. Save to `raw/articles/` with frontmatter (`compiled: false`)
4. Append to `log.md`
5. Ingest does NOT create wiki pages. Use compile for that.

## Compile Rules
1. Identify uncompiled raw sources (no matching source-summary in `wiki/`)
2. For each source: write source-summary, extract entities, create/update pages
3. **Backlink audit** (CRITICAL): grep existing pages for mentions of new page titles
4. Update `wiki/index.md`
5. Append to `log.md`
6. **Rebuild vector index** (CRITICAL): Run `python3 LLM_Wiki/build-index.py` to keep embedding search consistent with current wiki content.
One source typically touches 5-15 pages. This is normal.

## Walkthrough Rules

Analyze a code repository (local path or remote URL) and create `module` and `architecture` wiki pages with cross-references to existing concept pages. Code is never modified — only analyzed.

### Walkthrough Algorithm (5 Phases)

#### Phase 0: Acquire Code
1. If **URL** (GitHub/git remote): `git clone --depth 1 <url> raw/code/<repo-name>/`
2. If **local path**: copy or symlink to `raw/code/<repo-name>/`
3. Verify `raw/code/<repo-name>/` exists with source files
4. Write `raw/code/<repo-name>/SUMMARY.md` with metadata (source, clone date, etc.)

#### Phase 1: Explore (Structure Discovery)
**Token budget**: ~10-15 files. Do NOT read all files.
1. **Directory tree**: List files by extension (`find` or `glob`). Build a directory map.
2. **Language detection**: Check file extensions (`.c`/`.h` → C, `.rs` → Rust, `.py` → Python, etc.)
3. **Build system detection**: Look for `Makefile`, `CMakeLists.txt`, `Cargo.toml`, `Kconfig`, etc. Read entry point.
4. **Entry point detection**: Search for `main(`, `module_init(`, `fn main(`, `if __name__` in key dirs.
5. **Module boundaries**: Use language-specific heuristics:
   - **C**: subdirs with `Makefile`/`Kconfig` = module; one `.c` per driver
   - **Rust**: `src/` subdirs with `mod.rs`; `Cargo.toml` `[workspace]` members
   - **Python**: packages with `__init__.py`
6. **Identify 5-15 key files** to read deeply: entry points, core headers, main implementations.

#### Phase 2: Understand (Deep Read)
**Token budget**: ~15-25 files. For each key file identified in Phase 1:
1. Read the file
2. Extract:
   - **Exported functions** (public API) — name, signature, purpose
   - **Data structures** — struct/enum name, fields, purpose
   - **Configuration options** — `#define`, Kconfig, feature flags
   - **Dependencies** — `#include`, `use`, `import`
   - **Protocol/hardware references** — comments mentioning specs (e.g., "PCIe spec 4.2.3", "MSI-X")
3. Group findings by module

#### Phase 3: Extract & Link (Entity Creation)
For each **module** identified:
1. Check if `wiki/modules/<module-name>.md` already exists
   - If yes: update with new findings, preserve existing content
   - If no: create using the `module` template
2. Match keywords from module descriptions against existing concept pages (scan `wiki/concepts/` and `wiki/architecture/`). If available, run `python3 LLM_Wiki/search-index.py "<module description>" --top-k 3` for semantic matches.
3. Add `[[wikilinks]]` in the `## Dependencies` section for matched concept pages.

For the **architecture** page:
1. Check if `wiki/architecture/<repo-name>.md` already exists → update; otherwise create using `architecture` template
2. List all modules as `[[wikilinks]]`; draw module dependency graph

#### Phase 4: Backlink (Reverse Link from Concepts to Code)
For each existing **concept page** linked FROM a module page:
1. Read the concept page
2. If it lacks a `## Code References` section, add one at the end
3. Add: `- [[module-name]] — <repo>/<path> — <one-line how this code implements the concept>`

#### Phase 5: Update Infrastructure
1. **Update `wiki/index.md`**: Add new domain heading `## Code / <repo-name>` with entries for architecture and module pages
2. **Rebuild vector index**: `python3 LLM_Wiki/build-index.py`
3. **Append to `log.md`**: `## [YYYY-MM-DD] walkthrough | <repo-name>`
4. **Update `raw/code/<repo-name>/SUMMARY.md`** with walkthrough metadata

### Token Budget Strategy
| Repo Size | Phase 1 (Explore) | Phase 2 (Understand) | Phase 3-5 (Link) |
|-----------|-------------------|---------------------|-------------------|
| Small (<100 files) | 5-10 files | 10-15 files | All new pages |
| Medium (100-500) | 10-15 files | 15-25 files | Top modules only |
| Large (500+) | 15-20 files | 20-30 files | Architecture + key modules only |
| Huge (1000+) | 20 files max | 25 files max | Architecture overview only + offer to drill into subdirectories |

After completing a walkthrough, offer: "Walkthrough complete. Analyze a specific subdirectory deeper? (e.g., `wiki walkthrough <repo>/drivers/pci/ --deep`)"

## Query Rules
1. Read `wiki/index.md` first
2. **Vector search**: Run `python3 LLM_Wiki/search-index.py "<question>" --top-k 5` and read the top-k result pages
3. Follow `[[wikilinks]]` one level deep from the result pages if relevant
4. **Before descending into any subdirectory** (e.g., `modules/`, `architecture/`, `queries/`), check for a `_context.md` file in that directory. Use its description to decide whether the directory is worth searching and which pages to read.
5. Synthesize answer with `[[wikilinks]]` as citations
6. Always file answer to `wiki/queries/` (mandatory, no prompt)
7. Offer promotion to `wiki/` as a concept page (y/n)
8. Append to `log.md` (both query and optional promote events)

## Lint Rules
Run `python3 LLM_Wiki/lint-wiki.py LLM_Wiki/wiki/` (deterministic script) then resolve all issues. The script checks:

| # | Check | What it flags |
|---|-------|-------------|
| 1 | **Dead links** | `[[wikilinks]]` pointing to nonexistent files |
| 2 | **Orphan pages** | Pages in `wiki/` (except queries/) with no inbound `[[links]]` |
| 3 | **index.md link integrity** | `[[links]]` in `index.md` that have no matching file on disk |
| 4 | **Frontmatter required** | Non-query pages missing YAML `---` frontmatter block |
| 5 | **`type:` validation** | `type:` not in allowed set: `concept`, `person`, `source-summary`, `query-output`, `module`, `architecture` |
| 6 | **`date:` validation** | `date:` missing or not in `YYYY-MM-DD` format |
| 7 | **Missing sections** | Concept pages lack `## Counter-Arguments and Gaps`; module pages lack `## Responsibilities`, `## Key Interfaces`, `## Dependencies`; architecture pages lack `## Directory Map`, `## Module Graph` |
| 8 | **Query `question:` field** | Files in `wiki/queries/` missing the `question:` frontmatter field |
| 9 | **Module `source-path:` field** | Pages with `type: module` missing the `source-path:` frontmatter field |
| 10 | **Architecture → module links** | Pages with `type: architecture` that don't link to any page in `modules/` via `[[wikilink]]` |
| 11 | **`raw/` immutability** | Tracked files in `raw/` that have been modified (git diff HEAD) |
| 12 | **`_context.md`** | Subdirectories with >10 pages lacking a `_context.md` file that describes the directory's purpose |

After fixing, append to `log.md`.
