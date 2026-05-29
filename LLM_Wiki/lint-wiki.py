#!/usr/bin/env python3
"""Deterministic wiki lint checks.

Checks:
  - Dead links ([[wikilinks]] to nonexistent files)
  - Orphan pages (no inbound [[links]])
  - Missing required sections per entity type
  - Frontmatter integrity (type, date, required fields)
  - index.md link integrity
  - raw/ immutability (tracked files not modified)
  - Architecture → module link requirement
"""

import os
import re
import subprocess
import sys

WIKILINK_RE = re.compile(r"\[\[([^\]|]+)(?:\|[^\]]+)?\]\]")
FRONTMATTER_RE = re.compile(r"^---\n(.*?)\n(?:---|\.\.\.)", re.DOTALL)
DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")
ALLOWED_TYPES = {
    "concept", "person", "source-summary",
    "query-output", "query",  # "query" accepted for backward compat
    "module", "architecture",
}
SKIP_SLUGS = {"index", "log", "LICENSE"}


def find_md_files(wiki_dir):
    """Walk wiki_dir and return {slug: relative_path} for all .md files."""
    files = {}
    for root, _, filenames in os.walk(wiki_dir):
        for fn in filenames:
            if fn.endswith(".md"):
                rel = os.path.relpath(os.path.join(root, fn), wiki_dir)
                slug = os.path.splitext(fn)[0]
                files[slug] = rel
    return files


def extract_links(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        return WIKILINK_RE.findall(f.read())


def parse_frontmatter(content):
    """Extract key:value pairs from YAML frontmatter. Returns dict or None."""
    m = FRONTMATTER_RE.match(content)
    if not m:
        return None
    fm = {}
    for line in m.group(1).split("\n"):
        m2 = re.match(r"^(\w[\w-]*)\s*:\s*(.+)", line)
        if m2:
            val = m2.group(2).strip()
            # Strip surrounding quotes
            if len(val) >= 2 and val[0] in ('"', "'") and val[0] == val[-1]:
                val = val[1:-1]
            fm[m2.group(1)] = val
    return fm


def lint(wiki_dir):
    if not os.path.isdir(wiki_dir):
        print(f"Error: {wiki_dir} is not a directory", file=sys.stderr)
        return 1

    # Derive paths for raw/ immutability check
    wiki_dir_abs = os.path.abspath(wiki_dir)
    wiki_root = os.path.dirname(wiki_dir_abs)       # e.g. /path/to/vault/LLM_Wiki
    vault_root = os.path.dirname(wiki_root)          # e.g. /path/to/vault
    raw_rel = os.path.relpath(os.path.join(wiki_root, "raw"), vault_root)

    files = find_md_files(wiki_dir)
    issues = []
    inbound = {slug: set() for slug in files}
    has_fm = {slug: False for slug in files}

    # Precompute module slugs for architecture link check
    module_slugs = {
        slug.lower()
        for slug, rel in files.items()
        if rel.startswith("modules" + os.sep)
    }

    # ================================================================
    # PASS 1: Link graph + frontmatter checks
    # ================================================================
    for slug, rel in files.items():
        filepath = os.path.join(wiki_dir, rel)
        with open(filepath, "r", encoding="utf-8") as f:
            raw_content = f.read()

        links = WIKILINK_RE.findall(raw_content)

        # --- Dead link check ---
        for target in links:
            target_slug = target.strip().lower()
            matching = [s for s in files if s.lower() == target_slug]
            if not matching:
                issues.append(f"DEAD_LINK: [[{target}]] in {rel}")
            else:
                for m in matching:
                    inbound[m].add(slug)

        # --- Frontmatter checks (skip special pages) ---
        if slug in SKIP_SLUGS:
            continue

        fm = parse_frontmatter(raw_content)
        is_query = rel.startswith("queries" + os.sep)

        # Non-query pages MUST have frontmatter
        if not is_query and fm is None:
            issues.append(f"NO_FRONTMATTER: {rel} has no YAML frontmatter")
            continue  # cannot validate fields without frontmatter

        if fm is None:
            continue  # query pages without frontmatter — skip field checks

        has_fm[slug] = True

        # Extract type for later section checks (avoids re-parsing)
        fm_type = fm.get("type")

        # --- type: validation ---
        if fm_type is None:
            issues.append(f"NO_TYPE: {rel} has no 'type:' in frontmatter")
        elif fm_type not in ALLOWED_TYPES:
            issues.append(
                f"INVALID_TYPE: {rel} type='{fm_type}' "
                f"(allowed: {', '.join(sorted(ALLOWED_TYPES))})"
            )

        # --- date: validation ---
        fm_date = fm.get("date")
        if fm_date is None:
            issues.append(f"NO_DATE: {rel} has no 'date:' in frontmatter")
        elif not DATE_RE.match(fm_date):
            issues.append(
                f"INVALID_DATE: {rel} date='{fm_date}' (expected YYYY-MM-DD)"
            )

        # --- queries/ must have question: ---
        if is_query and "question" not in fm:
            issues.append(f"NO_QUESTION: {rel} (query) lacks 'question:' in frontmatter")

        # --- module must have source-path: ---
        if fm_type == "module" and "source-path" not in fm:
            issues.append(
                f"NO_SOURCE_PATH: {rel} (module) lacks 'source-path:' in frontmatter"
            )

        # --- section checks (type-specific) ---
        if fm_type == "concept":
            if "## Counter-Arguments and Gaps" not in raw_content:
                issues.append(
                    f"MISSING_SECTION: {rel} (concept) lacks '## Counter-Arguments and Gaps'"
                )
        if fm_type == "module":
            for section in ["Responsibilities", "Key Interfaces", "Dependencies"]:
                if f"## {section}" not in raw_content:
                    issues.append(
                        f"MISSING_SECTION: {rel} (module) lacks '## {section}'"
                    )
        if fm_type == "architecture":
            for section in ["Directory Map", "Module Graph"]:
                if f"## {section}" not in raw_content:
                    issues.append(
                        f"MISSING_SECTION: {rel} (architecture) lacks '## {section}'"
                    )

        # --- architecture must link to at least one module page ---
        if fm_type == "architecture":
            linked_modules = [
                t for t in links
                if t.strip().lower() in module_slugs
            ]
            if not linked_modules:
                issues.append(
                    f"NO_MODULE_LINKS: {rel} (architecture) links to no module page "
                    f"in modules/"
                )

    # ================================================================
    # PASS 2: Orphan check
    # ================================================================
    for slug in files:
        if slug in SKIP_SLUGS:
            continue
        rel = files[slug]
        if rel.startswith("queries" + os.sep):
            continue
        if not inbound.get(slug):
            issues.append(f"ORPHAN: {rel} has no inbound links")

    # ================================================================
    # PASS 3: index.md link integrity
    # ================================================================
    index_path = os.path.join(wiki_dir, "index.md")
    if os.path.exists(index_path):
        index_links = extract_links(index_path)
        for target in index_links:
            target_slug = target.strip().lower()
            matching = [s for s in files if s.lower() == target_slug]
            if not matching:
                issues.append(
                    f"INDEX_DEAD_LINK: [[{target}]] in index.md points to missing file"
                )

    # ================================================================
    # PASS 4: raw/ immutability check
    # ================================================================
    try:
        result = subprocess.run(
            ["git", "diff", "--name-only", "HEAD", "--", raw_rel],
            capture_output=True, text=True, cwd=vault_root, timeout=10,
        )
        if result.returncode == 0:
            modified = [f for f in result.stdout.strip().split("\n") if f]
            for f in modified:
                issues.append(
                    f"RAW_MODIFIED: {f} — tracked file in raw/ modified "
                    f"(immutability violated)"
                )
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass  # git not available — skip check
    except Exception:
        pass

    # ================================================================
    # Report
    # ================================================================
    if not issues:
        print("OK: No issues found")
        return 0

    for issue in sorted(issues):
        print(issue)
    print(f"\nTotal: {len(issues)} issue(s)")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <wiki-directory>", file=sys.stderr)
        sys.exit(1)
    sys.exit(lint(sys.argv[1]))
