"""
audit.py — raw/ 目录审计 + 绑定状态检查

用法:
  python3 scripts/kb/audit.py
  python3 scripts/kb/audit.py --stale
"""

import os
import re
import sys
from datetime import date

WIKI_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "wiki"))
RAW_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "raw"))
KB_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".kb"))

FM_RE = re.compile(r"^---\n(.*?)\n(?:---|\.\.\.)", re.DOTALL)


def parse_fm(content):
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


def main(args):
    check_stale = "--stale" in args
    issues = 0

    # --- 1. Audit raw/articles/ ---
    articles_dir = os.path.join(RAW_ROOT, "articles")
    uncompiled = []
    unknown = []

    for root, _, files in os.walk(articles_dir):
        for fn in sorted(files):
            if not fn.endswith(".md"):
                continue
            fp = os.path.join(root, fn)
            rel = os.path.relpath(fp, os.path.join(os.path.dirname(__file__), "..", ".."))
            with open(fp, "r", encoding="utf-8") as f:
                fm = parse_fm(f.read())

            if not fm:
                unknown.append(rel)
            elif fm.get("compiled") == "false":
                uncompiled.append(rel)

    if uncompiled:
        print(f"[WARN] UNCOMPILED ({len(uncompiled)}):")
        for f in uncompiled:
            print(f"       {f}")
        issues += len(uncompiled)

    if unknown:
        print(f"[??] UNKNOWN ({len(unknown)}): no frontmatter")
        for f in unknown:
            print(f"       {f}")
        issues += len(unknown)

    # --- 2. Audit raw/code/ ---
    code_dir = os.path.join(RAW_ROOT, "code")
    if os.path.isdir(code_dir):
        for repo in sorted(os.listdir(code_dir)):
            repo_path = os.path.join(code_dir, repo)
            if not os.path.isdir(repo_path):
                continue
            cg = os.path.isfile(os.path.join(repo_path, ".codegraph", "codegraph.db"))
            codebase_page = os.path.isfile(os.path.join(WIKI_ROOT, "codebases", f"{repo}.md"))
            if not codebase_page:
                print(f"[WARN] raw/code/{repo}: indexed={cg}, but no wiki/codebases/{repo}.md")
                issues += 1

    # --- 3. Check bindings database ---
    bindings_db = os.path.join(KB_DIR, "bindings.sqlite")
    if os.path.isfile(bindings_db):
        import sqlite3
        conn = sqlite3.connect(bindings_db)
        pending = conn.execute("SELECT COUNT(*) FROM bindings WHERE status='pending'").fetchone()[0]
        accepted = conn.execute("SELECT COUNT(*) FROM bindings WHERE status='accepted'").fetchone()[0]
        if pending:
            print(f"[INFO] Bindings: {accepted} accepted, {pending} pending (kb bind review)")
        else:
            print(f"[OK] Bindings: {accepted} accepted, 0 pending")
        conn.close()

    print(f"\nTotal: {issues} issue(s)" if issues else "\nAll clean.")


if __name__ == "__main__":
    main(sys.argv[1:])
