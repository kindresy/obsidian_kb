---
name: git-commit
description: >-
  Git commit workflow for the docs_writing Obsidian vault.
  Enforces pre-commit linting, gitignore hygiene, selective staging,
  and detailed commit messages. Use when the user says "commit",
  "提交", "git commit", or asks to save changes to the repository.
argument-hint: [--dry-run]
---

# Git Commit — docs_writing Vault

Safe, lint-verified, traceable commits for the Obsidian LLM Wiki vault.

## Usage

```
git commit          — stage changes, run lint, create detailed commit
git commit --dry-run — preview what would be committed without making changes
```

## Guardrails

### 1. Pre-commit Lint (MANDATORY — blocking)
Before any commit is created, the lint MUST pass:
```bash
python3 LLM_Wiki/lint-wiki.py LLM_Wiki/wiki/
```
- If lint finds issues, fix them first. Commit is BLOCKED until lint is clean.
- This ensures no dead links, orphans, or missing sections enter the repository.

### 2. Gitignore Hygiene (MANDATORY — checked every commit)
Before staging, verify `.gitignore` covers:

| Category | Patterns | Rationale |
|----------|----------|-----------|
| **Kernel build artifacts** | `raw/code/**/*.o`, `*.ko`, `*.mod`, `*.mod.c`, `*.cmd`, `Module.symvers`, `modules.order` | Rebuildable from source; 2.9MB+ in raw/code/ |
| **User Obsidian config** | `.obsidian/graph.json`, `.obsidian/workspace.json`, `.obsidian/plugins/*/data.json` | User-specific; differs per machine |
| **Vector index** | `LLM_Wiki/.vector_index/` | Rebuildable via build-index.py |
| **Python cache** | `__pycache__/`, `*.pyc` | Generated at runtime |
| **OS files** | `.DS_Store`, `Thumbs.db` | Platform garbage |

If any untracked file matches these patterns but is NOT in `.gitignore`, update `.gitignore` before committing.

### 3. Selective Staging (MANDATORY — never `git add -A`)
Do NOT stage everything blindly. Stage only files that were INTENTIONALLY changed:

- ✅ **Wiki pages** (`wiki/*.md`, `wiki/modules/*.md`, `wiki/architecture/*.md`, `wiki/queries/*.md`)
- ✅ **Wiki schema** (`LLM_Wiki/CLAUDE.md`, `LLM_Wiki/build-index.py`, `LLM_Wiki/lint-wiki.py`)
- ✅ **Skills and docs** (`skills/*/SKILL.md`, `skills/*/USAGE.md`, `CLAUDE.md`)
- ✅ **New raw sources** (`raw/articles/*.md`) — only committed sources, not auto-generated
- ✅ **Lint reports** (`outputs/reports/*.md`)
- ✅ **Operation log** (`LLM_Wiki/log.md`)
- ✅ **Index** (`LLM_Wiki/wiki/index.md`)
- ❌ **Obsidian config** (`.obsidian/*`) — user-specific, never commit
- ❌ **Build artifacts** (`*.o`, `*.ko`, `*.mod.*`) — rebuildable
- ❌ **Vector index** (`.vector_index/`) — rebuildable
- ❌ **Whitespace-only changes** on untracked template files — revert if unintended

### 4. Large File Check (MANDATORY)
Check for any staged file >1MB. If found:
- Confirm it's intentional (e.g., a binary attachment in `raw/attachments/`)
- Otherwise add the pattern to `.gitignore`

### 5. Commit Message Format (MANDATORY)

```
<type>(<scope>): <subject>

<body>

<footnotes>
```

**Types**: `feat` | `fix` | `docs` | `refactor` | `chore` | `style` | `test`
**Scope**: `wiki` | `walkthrough` | `query` | `lint` | `skill` | `config` | `raw`
**Subject**: ≤72 chars, imperative mood, no period
**Body**: Bullet list of WHAT changed and WHY. Include file paths.
**Footnotes**: Optional references (e.g., "Part of #issue")

Example:
```
feat(walkthrough): add code walkthrough infrastructure

- LLM_Wiki/CLAUDE.md: add walkthrough rules, module/architecture entity types
- LLM_Wiki/build-index.py: extend glob to wiki/**/*.md
- LLM_Wiki/lint-wiki.py: add section checks for module/architecture pages
- skills/llm-wiki/SKILL.md: add walkthrough operation definition
```

### 6. Safety Rules (NEVER)
- NEVER `git push --force` to main/master. Ask the user first.
- NEVER commit `.env`, credentials files, or API keys. If found, warn.
- NEVER `git add -A` — always stage individual files by name.
- NEVER skip hooks (`--no-verify`, `--no-gpg-sign`).

## Workflow Summary

```
1. git status          → see what's changed and what's untracked
2. Update .gitignore   → ensure build artifacts and user configs are excluded
3. python3 lint-wiki.py → run lint (BLOCKING if fails)
4. Fix any lint issues → repeat until clean
5. git add <files>     → stage ONLY intentionally changed files
6. Check for large files → warn if >1MB
7. git commit          → write detailed message per format above
8. git status          → verify clean state
9. git push (optional) → only if user asks, never --force
```
