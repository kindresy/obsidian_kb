# LLM Wiki Schema

## Directory Layout
- `raw/` — immutable source drops. Never edit files here.
- `raw/articles/` — text source documents (articles, papers, transcripts).
- `raw/attachments/` — images and binary attachments.
- `wiki/` — LLM-owned pages. You have full write access here.
- `wiki/index.md` — catalog. Read this FIRST before opening any other page.
- `wiki/queries/` — filed query answers. Promote to `wiki/` when durable.
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

## Naming Conventions
- All filenames: `lowercase-kebab-case.md`
- Wikilinks: `[[filename-without-extension]]`
- Never use standard markdown links for internal links

## Log Format
Append to `log.md` after every operation. Format:
```
## [YYYY-MM-DD] <operation> | <title>
<one-line description>
```
Operations: `ingest | compile | query | lint | promote | remove`

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

## Query Rules
1. Read `wiki/index.md` first
2. **Vector search**: Run `python3 LLM_Wiki/search-index.py "<question>" --top-k 5` and read the top-k result pages
3. Follow `[[wikilinks]]` one level deep from the result pages if relevant
4. Synthesize answer with `[[wikilinks]]` as citations
5. Always file answer to `wiki/queries/` (mandatory, no prompt)
6. Offer promotion to `wiki/` as a concept page (y/n)
7. Append to `log.md` (both query and optional promote events)

## Lint Rules
Scan all pages in `wiki/` and report:
- Contradictions between pages
- Orphan pages (no inbound `[[links]]`)
- Pages with `status: stale` older than 90 days
- Missing Counter-Arguments and Gaps sections
- Index entries pointing to missing files
- Run the deterministic lint script: `python3 lint-wiki.py wiki/`
After fixing, append to `log.md`.
