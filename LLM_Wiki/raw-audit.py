#!/usr/bin/env python3
"""
raw-audit.py - Check which files in raw/ have NOT been ingested/compiled.

Scans raw/articles/ and raw/code/, compares against wiki/ pages, reports:
  UNCOMPILED  - raw/ has files but wiki/ has no matching source-summary
  UNKNOWN     - no YAML frontmatter (dropped directly into raw/)
  MISSING     - raw/code/ repo without architecture/module pages
  OK          - everything accounted for

Usage:
  python3 LLM_Wiki/raw-audit.py
"""

import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
WIKI_DIR = os.path.join(SCRIPT_DIR, "wiki")
RAW_DIR = os.path.join(SCRIPT_DIR, "raw")
ARTICLES_DIR = os.path.join(RAW_DIR, "articles")
CODE_DIR = os.path.join(RAW_DIR, "code")
ATTACHMENTS_DIR = os.path.join(RAW_DIR, "attachments")

FM_RE = re.compile(r"^---\n(.*?)\n(?:---|\.\.\.)", re.DOTALL)


def parse_fm(content: str) -> dict:
    m = FM_RE.match(content)
    if not m:
        return {}
    fm = {}
    for line in m.group(1).split("\n"):
        kv = re.match(r"^(\w[\w-]*)\s*:\s*(.+)", line)
        if kv:
            val = kv.group(2).strip()
            if len(val) >= 2 and val[0] in ('"', "'") and val[0] == val[-1]:
                val = val[1:-1]
            fm[kv.group(1)] = val
    return fm


def get_source_summaries() -> dict:
    """Return {slug: rel_path} for all source-summary pages."""
    res = {}
    for root, _, files in os.walk(WIKI_DIR):
        for fn in files:
            if not fn.endswith(".md") or fn in ("index.md", "_context.md"):
                continue
            fp = os.path.join(root, fn)
            with open(fp, "r", encoding="utf-8") as f:
                fm = parse_fm(f.read())
            if fm.get("type") == "source-summary":
                slug = os.path.splitext(fn)[0].lower()
                res[slug] = os.path.relpath(fp, SCRIPT_DIR).replace("\\", "/")
    return res


def get_source_dirs() -> set:
    """Return set of subdirectory names (lowercase) that contain .md files."""
    dirs = set()
    for root, subs, _ in os.walk(ARTICLES_DIR):
        for s in subs:
            sub = os.path.join(root, s)
            if any(f.endswith(".md") for f in os.listdir(sub)):
                dirs.add(s.lower())
    return dirs


def audit_articles(summaries: dict) -> list:
    results = []
    source_dirs = get_source_dirs()

    for root, _, files in os.walk(ARTICLES_DIR):
        for fn in sorted(files):
            if not fn.endswith(".md"):
                continue
            fp = os.path.join(root, fn)
            rel = os.path.relpath(fp, SCRIPT_DIR).replace("\\", "/")

            with open(fp, "r", encoding="utf-8") as f:
                content = f.read()
            fm = parse_fm(content)

            # Determine if file is in a subdirectory (grouped source)
            parts = rel.replace("\\", "/").split("/")
            in_subdir = len(parts) >= 4  # LLM_Wiki/raw/articles/<dirname>/<file>
            parent_dir = parts[-2].lower() if in_subdir else None

            if not fm:
                results.append(("UNKNOWN", rel, "no frontmatter (dropped directly into raw/)"))
                continue

            compiled = fm.get("compiled")

            if parent_dir and parent_dir in source_dirs:
                # Grouped source - check if group has a summary
                pd = parent_dir.replace("_", "-")
                match = next(
                    (s for s in summaries if pd in s.replace("_", "-") or s.replace("_", "-") in pd),
                    None
                )
                if match:
                    results.append(("OK", rel, f"group [{parent_dir}], summary at {summaries[match]}"))
                else:
                    results.append(("UNCOMPILED", rel, f"in group [{parent_dir}] but no summary found"))
            elif compiled == "true":
                results.append(("OK", rel, "compiled"))
            elif compiled == "false":
                results.append(("UNCOMPILED", rel, "compiled:false (ingested but never compiled)"))
            else:
                results.append(("UNCOMPILED", rel, "missing compiled field"))
    return results


def audit_code(summaries: dict) -> list:
    results = []
    if not os.path.isdir(CODE_DIR):
        return results

    for repo in sorted(os.listdir(CODE_DIR)):
        repo_path = os.path.join(CODE_DIR, repo)
        if not os.path.isdir(repo_path):
            continue

        repo_name = repo.lower()
        code_rel = f"raw/code/{repo}"
        repo_pages = []

        for root, _, files in os.walk(WIKI_DIR):
            for fn in files:
                if not fn.endswith(".md") or fn in ("index.md", "_context.md"):
                    continue
                fp = os.path.join(root, fn)
                with open(fp, "r", encoding="utf-8") as f:
                    content = f.read()
                    fm = parse_fm(content)

                src = fm.get("source-repo", "").lower().strip()
                if src and (src == repo_name or repo_name in src or src in repo_name):
                    repo_pages.append(os.path.relpath(fp, SCRIPT_DIR).replace("\\", "/"))
                elif code_rel in content:
                    repo_pages.append(os.path.relpath(fp, SCRIPT_DIR).replace("\\", "/"))

        deduped = sorted(set(repo_pages))
        has_cg = os.path.isfile(os.path.join(repo_path, ".codegraph", "codegraph.db"))

        if deduped:
            results.append(("OK", f"raw/code/{repo}", f"{len(deduped)} page(s)" + (" + CodeGraph" if has_cg else "")))
        else:
            results.append(("MISSING", f"raw/code/{repo}", "no wiki pages" + (" (has CodeGraph index)" if has_cg else "")))
    return results


def audit_attachments() -> list:
    results = []
    if not os.path.isdir(ATTACHMENTS_DIR):
        return results
    for item in sorted(os.listdir(ATTACHMENTS_DIR)):
        p = os.path.join(ATTACHMENTS_DIR, item)
        if os.path.isdir(p):
            results.append(("ATTACH", f"raw/attachments/{item}", f"{len(os.listdir(p))} file(s)"))
    return results


def main():
    summaries = get_source_summaries()

    buckets = {}
    for status, path, note in (audit_articles(summaries) + audit_code(summaries) + audit_attachments()):
        buckets.setdefault(status, []).append((path, note))

    def dump(label, items, indent=2):
        for p, n in items:
            print(" " * indent + p)
            print(" " * (indent + 2) + "-> " + n)

    print("=" * 65)
    print("  raw/ Audit Report")
    print("=" * 65)

    for cat, title in [("UNCOMPILED", "[WARN] UNCOMPILED"),
                        ("UNKNOWN", "[??] UNKNOWN (no frontmatter)"),
                        ("MISSING", "[ERR] MISSING"),
                        ("OK", "[OK]"),
                        ("ATTACH", "[INFO] Attachments")]:
        items = buckets.get(cat, [])
        if items:
            print(f"\n{title} ({len(items)}):")
            dump("", items)

    issues = len(buckets.get("UNCOMPILED", [])) + len(buckets.get("UNKNOWN", [])) + len(buckets.get("MISSING", []))
    ok = len(buckets.get("OK", []))
    total = sum(len(v) for v in buckets.values())

    print(f"\n{'=' * 65}")
    print(f"  Total: {total}  |  Issues: {issues}  |  Clean: {ok}")
    print(f"{'=' * 65}")

    if buckets.get("UNCOMPILED"):
        print("=> Run: wiki compile")
    if buckets.get("MISSING"):
        print("=> Run: wiki walkthrough raw/code/<repo-name>")
    if buckets.get("UNKNOWN"):
        print("=> Run: wiki ingest <file> for each UNKNOWN file (or add frontmatter manually)")


if __name__ == "__main__":
    main()
