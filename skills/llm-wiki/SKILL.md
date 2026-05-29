---
name: wiki
description: >-
  LLM Wiki — persistent, compounding knowledge base inside Obsidian.
  Use when the user says "/llm-wiki:wiki", "wiki init", "wiki ingest",
  "wiki query", "wiki lint", or asks about managing a knowledge base wiki.
argument-hint: init <name> | ingest <path|url> | compile [<path>] | query <question> | lint | remove <name> | walkthrough <path|url>
---

# LLM Wiki

Persistent, compounding knowledge base inside an Obsidian vault.

## Operations

```
wiki init my-topic
wiki ingest ./raw/article.md
wiki ingest https://example.com/article
wiki query "What is X?"
wiki lint
```

---

## Active Wiki Detection

Walk up from `cwd` looking for a directory containing **both** `CLAUDE.md` and a `wiki/` subfolder.

1. Start at `cwd`. Check if `CLAUDE.md` and `wiki/` exist in the current directory.
2. If found → that directory is the **active wiki root**. Read `CLAUDE.md` for schema.
3. If not found → move to parent directory and repeat until filesystem root.
4. If no wiki found anywhere in the path, prompt the user:
   > "Which wiki should I use?"
   List available wikis.

---

## `init <name>`

Create a new wiki scaffold under the Obsidian vault.

### Steps

1. **Check if wiki already exists:** If `LLM_Wiki/<name>/` exists, abort.

2. Create directory structure:
    ```bash
    mkdir -p LLM_Wiki/<name>/raw/articles
    mkdir -p LLM_Wiki/<name>/raw/attachments
    mkdir -p LLM_Wiki/<name>/wiki/queries
    mkdir -p LLM_Wiki/<name>/outputs/reports
    ```

3. Write `LLM_Wiki/<name>/CLAUDE.md` using the CLAUDE.md template (fill in `<name>`).

4. Write `LLM_Wiki/<name>/wiki/index.md` using the index.md template.

5. Write `LLM_Wiki/<name>/log.md` using the log.md template.

6. Write `LLM_Wiki/<name>/.gitignore`.

7. Print setup completion message.

---

## `walkthrough <path|url>`

Analyze a code repository and create `module` and `architecture` wiki pages, linked to existing concepts.

### Steps

1. **Detect active wiki.** Read `CLAUDE.md` for schema.

2. **Phase 0 — Acquire code:**
   - If URL: `git clone --depth 1 <url> LLM_Wiki/raw/code/<repo-name>/`
   - If local path: copy to `LLM_Wiki/raw/code/<repo-name>/`
   - Write `SUMMARY.md` with metadata.

3. **Phase 1 — Explore:** Build directory map, detect language/build system, find entry points and module boundaries. Identify 5-15 key files (token budget: ~10-15 files). Do NOT read all files.

4. **Phase 2 — Understand:** Read identified key files. Extract exported functions, data structures, config options, dependencies, protocol references. Group by module (token budget: ~15-25 files).

5. **Phase 3 — Extract & Link:**
   - Create/update `wiki/modules/<name>.md` for each module using the `module` template.
   - Create/update `wiki/architecture/<repo-name>.md` using the `architecture` template.
   - Run `python3 LLM_Wiki/search-index.py "<module description>" --top-k 3` to find semantically related existing concept pages.
   - Add `[[wikilinks]]` to dependencies sections.

6. **Phase 4 — Backlink:** For each concept page linked from a module, add or update its `## Code References` section with `[[module-name]]` entries.

7. **Phase 5 — Update Infrastructure:**
   - Update `wiki/index.md` with new `## Code / <repo-name>` domain entries
   - Rebuild vector index: `python3 LLM_Wiki/build-index.py`
   - Append to `log.md`

### Token Budget
| Repo Size | Files Read |
|-----------|-----------|
| <100 files | 15-25 total |
| 100-500 | 25-40 total |
| 500+ | 35-50 total (architecture only + drill-down offer) |

---

## `ingest <path|url>`

Acquire a source and save it to the raw library. Does NOT create wiki pages.

### Steps

1. **Detect active wiki** (see Active Wiki Detection). Read `CLAUDE.md` for schema.

2. **Acquire source:**
   - If input is a **URL**: use the WebFetch tool to retrieve content.
   - If input is a **file path**: read the file directly.

3. **Classify** the source as one of: `article` | `paper` | `transcript` | `conversation` | `image-set`.

4. **Save to raw library:** Write to `raw/articles/YYYY-MM-DD-<slug>.md` with frontmatter:
   ```yaml
   ---
   date: YYYY-MM-DD
   source-type: <classification>
   source-url: <original URL or file path>
   title: <extracted or inferred title>
   compiled: false
   ---
   ```

5. **Append to `log.md`.**

6. **Print:** "Source saved to raw/articles/<filename>. Run `wiki compile` to integrate into the wiki."

---

## `compile [<path>]`

Read raw sources and create/update wiki pages with entity extraction and cross-references.

### Steps

1. **Detect active wiki.** Read `CLAUDE.md` for schema and templates.

2. **Identify sources to compile:**
   - If path argument given: use that file.
   - Otherwise: list files in `raw/articles/`. For each, check if a corresponding source-summary page exists in `wiki/`. Compile any source without a matching summary.
   - If nothing to compile: "All sources are already compiled. Nothing to do." Stop.

3. **For each source to compile:**

   a. Read the raw source content.

   b. **Write or update source-summary page** in `wiki/` using the `source-summary` template from `CLAUDE.md`.

   c. **Entity extraction:** For each mentioned entity (person, concept, event):
      - Check if a page already exists in `wiki/`.
      - If yes → update with new information, preserving existing content.
      - If no → create using the appropriate template (`concept.md` or `person.md`).
      - Add `[[wikilinks]]` to related pages in both directions.

   d. **Backlink audit** (CRITICAL — do not skip): grep all existing wiki pages for mentions of the new page title. For each file that mentions the new page title but does NOT contain `[[new-page-name]]`: add a `[[wikilink]]` at the first mention.

4. **Update `wiki/index.md`** with new/updated entries under the appropriate domain heading.

5. **Append to `log.md`.**

6. **Rebuild vector index** (CRITICAL): Run `python3 LLM_Wiki/build-index.py` to keep embedding search consistent with current wiki content.

---

## `query <question>`

Answer a question using wiki knowledge, with citations.

### Steps

1. **Detect active wiki.** Read `CLAUDE.md`.

2. **Find relevant pages via vector search:** Run `python3 LLM_Wiki/search-index.py "<question>" --top-k 5` and read the top-k result pages.

3. **Read all relevant pages.** Follow one level of `[[wikilinks]]` if targets look relevant.

4. **Synthesize answer** with `[[wikilinks]]` as citations.

5. **File the answer** to `wiki/queries/<slug>.md` using the `query-output` frontmatter schema. Always file — no prompt.

6. **Ask:** "Promote this answer to `wiki/<slug>.md` as a concept page? (y/n)"
   - If yes: move the file from `wiki/queries/` to `wiki/`, update frontmatter `status` from `filed` to `promoted`.

7. **Append to `log.md`.**

---

## `lint`

Audit wiki integrity and fix issues.

### Steps

1. **Read all files** in `wiki/`.

2. **Build a link graph:** for each `[[wikilink]]` on each page, record the edge (source → target).

3. **Run deterministic lint script** if available:
   ```bash
   python3 LLM_Wiki/lint-wiki.py LLM_Wiki/wiki/
   ```

4. **Report and fix:**

   | Check | Action |
   |-------|--------|
   | **Orphan pages** (no inbound links) | List them. Suggest adding links from related pages. |
   | **Dead links** (`[[wikilinks]]` to nonexistent files) | Create stub pages with appropriate template. |
   | **Unlinked concept mentions** | Scan pages for proper nouns appearing without `[[wikilinks]]`. |
   | **Contradictions** | Scan for `[!WARNING]` markers. List them. |
   | **Missing "Counter-Arguments and Gaps" sections** | Add empty section. |
   | **Stale pages** | Flag pages with `status: stale` in frontmatter. |
   | **Index drift** | Compare `index.md` entries vs actual files. Add missing, remove dead. |

5. **Write lint report** to `outputs/reports/YYYY-MM-DD-lint.md`.

6. **Append to `log.md`.**

---

## Error Handling

| Situation | Action |
|-----------|--------|
| **No active wiki found** | List available wikis in `LLM_Wiki/*/wiki`. Suggest `wiki init <name>` if none exist. |
| **Network error on URL ingest** | Retry once. If still failing, suggest saving content manually to `raw/articles/`. |
| **Raw source too large** (>50KB) | Warn: "Large source detected. Entity extraction may be incomplete." Proceed anyway. |
| **log.md missing** | Create fresh log.md from template. Warn: "log.md was missing — created a new one." |
| **No uncompiled sources** (on compile) | Report: "All sources are already compiled. Nothing to do." Stop. |

---

## Templates

### CLAUDE.md

```markdown
# <name> Wiki Schema

## Directory Layout
- raw/              -- immutable source drops. Never edit files here.
- raw/articles/     -- text source documents (articles, papers, transcripts).
- raw/attachments/  -- images and binary attachments.
- wiki/             -- LLM-owned pages. You have full write access here.
- wiki/index.md     -- catalog. Read this FIRST before opening any other page.
- wiki/queries/     -- filed query answers. Promote to wiki/ when durable.
- outputs/reports/  -- dated lint reports and other artifacts.
- log.md            -- append-only operation log. Never edit existing entries.

## Entity Types and Templates

### concept.md
---
date: YYYY-MM-DD
tags: [domain]
type: concept
status: active
---
# Concept Name
<one-paragraph summary>

## Details
...

## See Also
- [[related-concept]]

## Counter-Arguments and Gaps
...

### person.md
---
date: YYYY-MM-DD
tags: [domain, person]
type: person
status: active
---
# Person Name
Role / affiliation.

## Key Contributions
...

## See Also
- [[related-concept]]

### source-summary.md
---
date: YYYY-MM-DD
tags: [domain]
type: source-summary
source-url: https://...
---

### query-output.md
---
date: YYYY-MM-DD
tags: [domain]
type: query-output
question: "<original question>"
status: filed
---

## Naming Conventions

### Filenames
- All filenames: `lowercase-kebab-case.md` — English only, no Chinese, no spaces
- Stable, script-friendly slugs for `[[wikilinks]]`

### Titles
- Can be Chinese for readability: `# MSI/MSI-X 中断`
- Keep consistent with the page's core topic

### Body Language
- Chinese as primary language
- English terms retained as-is (e.g., "TLP", "BAR")
- No translation of standard technical terms

### Wikilinks
- Always use the file slug: `[[page-name]]`
- Never use Chinese title text as link target
- For display readability, use pipe alias: `[[page-name|显示标题]]`
- Never use standard markdown links for internal references

## Log Format
Append to log.md after every operation. Format:
  ## [YYYY-MM-DD] <operation> | <title>
  <one-line description>

## Index Format
wiki/index.md is a human- and LLM-readable catalog. Format:
  ## Domain Name
  - [[page-name]] -- one-line description (YYYY-MM-DD)
```

### .gitignore
```
.DS_Store
*.sqlite
*.sqlite-wal
*.sqlite-shm
```

### wiki/index.md
```markdown
# <name> Wiki Index

Last updated: YYYY-MM-DD

<!-- Add entries after each ingest. Format:
## Domain
- [[page-name]] -- description (YYYY-MM-DD)
-->
```

### log.md
```markdown
# <name> Wiki Log

<!-- Append only. Never edit existing entries. Format:
## [YYYY-MM-DD] ingest | Title
One-line description.
-->
```
