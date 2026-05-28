---
name: file-prep
description: >-
  Preprocess large files (>50KB) for LLM Wiki ingest: extract and relocate
  embedded images to raw/attachments/, rewrite paths to Obsidian wikilink
  format (![[source-name/hash.ext]]), and split into <50KB chunks at natural
  section boundaries. Use when the user provides a large MD file, a book,
  a PDF export, or any file with embedded image references.
argument-hint: prep <file-path> [--source-name <name>]
---

# File Prep — Large File Preprocessor for LLM Wiki

Prepares large files for LLM Wiki ingest by handling two problems:

1. **Embedded images** — relocates them to `raw/attachments/<source-name>/` and rewrites paths from `![](relative/path)` to Obsidian `![[source-name/filename]]`
2. **File too large** — splits at natural section/chapter boundaries into `<50KB` chunks, each ready for `wiki ingest`

---

## Detection

Trigger this skill when ANY of these conditions are true:

- File size > **50KB** (the LLM Wiki large-source threshold)
- File contains embedded image references (`![](path/to/image)`)
- File has multiple sections/chapters that can be split
- User says "this file is too big", "slice it", "split it", "preprocess it"
- User provides a PDF export, a book-length document, or a merged Markdown file

---

## `prep <file-path> [--source-name <name>]`

Preprocess a file for wiki ingest.

### Parameters

| Parameter | Description |
|-----------|-------------|
| `<file-path>` | Path to the file to preprocess (relative to vault root) |
| `--source-name <name>` | (Optional) Name for the source. Auto-detected from filename if omitted. |

### Steps

#### Step 1: Detect Active Wiki & Validate

1. Locate active wiki by walking up from `cwd` looking for a directory with both `CLAUDE.md` and `wiki/` subfolder.
2. Read `CLAUDE.md` for schema reference.
3. Verify the target file exists. If not, abort with error.
4. Determine `source-name`:
   - Use `--source-name` if provided.
   - Otherwise, derive from filename: strip extension, use `kebab-case`.
5. Check file size. If `< 50KB` and no images, warn: "File is under 50KB and has no images. No preprocessing needed." and exit.

#### Step 2: Relocate Embedded Images

1. **Scan** the file for all embedded image references:
   - Pattern: `![](<path/to/image.ext>)` (standard Markdown)
   - Pattern: `<img src="<path/to/image.ext>">` (HTML)
   - Extract the filename (last path component) and its original relative path.
   - Check if the image files actually exist on disk at the resolved path.

2. **Notify**: Print the count of images found.

3. **Create destination**: `raw/attachments/<source-name>/` (if not exists).

4. **Copy images**: For each found image:
   - Copy the file from its original location to `raw/attachments/<source-name>/<filename>`.
   - If multiple source files have the same filename, append a suffix to avoid collisions.

5. **Verify copies**: Check destination directory has the expected count of files.

#### Step 3: Rewrite Image Paths

1. Replace all image references in the source file:
   - `![](path/to/image.ext)` → `![[source-name/image.ext]]`
   - `<img src="path/to/image.ext">` → `![[source-name/image.ext]]`

2. **Handle edge cases**:
   - If a path was already in `![[...]]` format but pointing to the old location, update it.
   - If a path is already correct (`![[source-name/...]]`), skip it.

3. **Verify**: Count old-style vs new-style references. Zero old-style = pass.

#### Step 4: Split File by Sections

1. **Analyze section structure**:
   - Scan for `# ` or `## ` headings (H1 or H2).
   - Identify chapter/section boundaries by numbered headings (e.g., `# 第1章`, `# 1.1`, `# Chapter 2`).
   - Also detect bullet headings like `# （1）`, `# (a)`.

2. **Determine split points**:
   - Each resulting chunk must be < `45KB` (safety margin under 50KB).
   - Split at section boundaries (headings). Average section size should be 20-40KB.
   - If sections are too large individually, split at sub-section boundaries.

3. **Create slice files**:
   - Output to the same directory as the original file.
   - Naming: `<source-name>_p<N>.md` (e.g., `pci-express_p1.md`).
   - Files with no images and <50KB: single file named `<source-name>.md` (no `_p1` suffix).
   - If a small chunk would be < 5KB, merge it with the adjacent chunk.

4. **Merge logic** (for very small chunks):
   - If a chunk is < `max_size * 0.3`, try to merge with the previous chunk.
   - If merging would exceed `max_size`, leave as standalone.

5. **Verify**: Confirm all slice files are < 50KB. Print a table:
   ```
   File                      Size   Status
   ───────────────────────────────────────
   pci-express_p1.md         29KB   ✅
   pci-express_p2.md         16KB   ✅
   ...
   ```

#### Step 5: Propagate Image Paths to All Slices

1. Since image path rewriting happened in Step 3 before splitting, all slice files inherit the correct `![[source-name/...]]` paths.
2. Verify by scanning all slice files for:
   - `![[` references → should all have the `source-name/` prefix.
   - `![](` references → count should be zero.

#### Step 6: Clean Up & Report

1. Optionally archive or remove the original parts/ directory if images were extracted from a nested structure.
2. Print final summary:

   ```
   ✅ File prep complete: <source-name>
   
   Images:     N relocated → raw/attachments/<source-name>/
   Slices:     M files → raw/articles/<source-folder>/
   Max slice:  XX KB
   Old paths:  0 remaining
   ```

3. Suggest next step: "Run `wiki ingest <slice-1>` then `wiki compile` for each slice."

---

## Error Handling

| Situation | Action |
|-----------|--------|
| **File not found** | Abort: "File `<path>` does not exist at `<resolved-path>`." |
| **Image file not found on disk** | Warn: "Image `<path>` referenced but not found on disk. Check the relative path." Rewrite the reference anyway so it doesn't break Obsidian rendering. |
| **No section headings found** | Fall back to line-count split: divide total lines by N to get N chunks of ~40KB each. |
| **All slices under 10KB** | Warn: "Slices are very small (<10KB each). Consider merging related sections for better compile context." |
| **File has no images and is under 50KB** | Report: "No preprocessing needed." Exit. |
| **Filename collision in attachments** | Append `.1`, `.2` etc. to the colliding filename. Log the collision. |
| **Source-name already exists in attachments** | Warn: "Directory `raw/attachments/<source-name>/` already exists. Images will be merged." Skip files that already exist (use modify time to decide). |

---

## Templates

### Image reference patterns to handle

| Input pattern | Output pattern |
|---|---|
| `![](parts/part_1/images/abc.jpg)` | `![[source-name/abc.jpg]]` |
| `![](./images/abc.png)` | `![[source-name/abc.png]]` |
| `![](../attachments/abc.jpg)` | `![[source-name/abc.jpg]]` |
| `<img src="images/abc.jpg">` | `![[source-name/abc.jpg]]` |

### Output directory structure

```
raw/
├── attachments/<source-name>/
│   ├── abc.jpg
│   ├── def.png
│   └── ...
└── articles/<source-folder>/
    ├── <source-name>_p1.md
    ├── <source-name>_p2.md
    └── ...
```

### Chunking strategy decision tree

```
File size > 50KB?
├── No → single file, no split needed
├── Yes → has section headings (# / ##)?
│   ├── Yes → split at section boundaries
│   │   └── Any section > 45KB?
│   │       ├── Yes → split at subsection boundaries
│   │       └── No → done
│   └── No → fallback to line-count split
│       └── target: ~40KB per chunk
└── Has images?
    ├── Yes → relocate to attachments/<source-name>/
    └── No → skip relocation
```
