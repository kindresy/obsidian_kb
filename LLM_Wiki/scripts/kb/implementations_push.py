"""
implementations_push.py — 将接受的绑定回写到概念页 ## Implementations

用法:
  python3 scripts/kb/implementations_push.py              # 一次推全部
  python3 scripts/kb/implementations_push.py --concept i3c # 只推指定概念
"""

import json
import os
import re
import sqlite3
import sys

WIKI_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "wiki"))
BINDING_DB = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".kb", "bindings.sqlite"))


def find_concept_page(concept_id):
    """Find a concept page by slug in any subdirectory under wiki/."""
    for root, _, files in os.walk(WIKI_ROOT):
        for fn in files:
            if fn.endswith(".md") and os.path.splitext(fn)[0].lower() == concept_id.lower():
                return os.path.join(root, fn)
    return None


def push_bindings(concept_filter=None):
    conn = sqlite3.connect(BINDING_DB)
    conn.row_factory = sqlite3.Row

    if concept_filter:
        bindings = conn.execute(
            "SELECT * FROM bindings WHERE status='accepted' AND concept_id=?", (concept_filter,)
        ).fetchall()
    else:
        bindings = conn.execute(
            "SELECT * FROM bindings WHERE status='accepted' ORDER BY concept_id"
        ).fetchall()

    if not bindings:
        print("  No accepted bindings to push.")
        return

    # Group by concept
    groups = {}
    for b in bindings:
        groups.setdefault(b["concept_id"], []).append(b)

    for concept_id, items in groups.items():
        page_path = find_concept_page(concept_id)
        if not page_path:
            print(f"  [SKIP] concept '{concept_id}' — page not found")
            continue

        with open(page_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Build implementations section
        impl_lines = ["\n## Implementations\n"]
        for b in items:
            evidence = json.loads(b["evidence_json"]) if b["evidence_json"] else {}
            note = evidence.get("note", "")
            impl_lines.append(f"- `{b['code_node_id']}()` — {b['relation']}")
            if note:
                impl_lines.append(f"  - {note}")
        impl_lines.append("")

        new_section = "\n".join(impl_lines)

        # Replace existing ## Implementations section or add before ## Counter-Arguments
        if "## Implementations" in content:
            content = re.sub(
                r"## Implementations\n.*?(?=\n## |\Z)",
                new_section.lstrip("\n"),
                content,
                count=1,
                flags=re.DOTALL
            )
        else:
            content = content.replace("## Counter-Arguments and Gaps", new_section + "\n## Counter-Arguments and Gaps")

        with open(page_path, "w", encoding="utf-8") as f:
            f.write(content)

        print(f"  [OK] {concept_id} ({len(items)} bindings) -> {os.path.relpath(page_path, WIKI_ROOT)}")

    conn.close()


def main(args):
    concept_filter = None
    if "--concept" in args:
        idx = args.index("--concept")
        if idx + 1 < len(args):
            concept_filter = args[idx + 1]
    push_bindings(concept_filter)


if __name__ == "__main__":
    main(sys.argv[1:])
