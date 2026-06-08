"""
bind_concepts.py — 概念 ↔ 代码符号绑定引擎 (Phase 2)

三级绑定:
  Level 1: 精确符号匹配 (函数名/结构体名/宏名)
  Level 2: 别名匹配 (alias_table.json)
  Level 3: 语义候选 (待 LLM 判断)

用法:
  python3 scripts/kb/bind_concepts.py propose <repo-name>
  python3 scripts/kb/bind_concepts.py review
  python3 scripts/kb/bind_concepts.py promote <binding-id>
"""

import json
import os
import re
import sqlite3
import sys
from datetime import date

WIKI_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "wiki"))
RAW_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "raw"))
KB_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".kb"))
ALIAS_FILE = os.path.join(KB_DIR, "alias_table.json")
BINDING_DB = os.path.join(KB_DIR, "bindings.sqlite")

SYMBOL_PATTERN = re.compile(r'`([^`]+)`|\[\[([^\]]+)\]\]')


def load_aliases():
    """Load alias table from JSON."""
    if not os.path.isfile(ALIAS_FILE):
        return []
    with open(ALIAS_FILE, "r", encoding="utf-8") as f:
        data = json.load(f)
    return data.get("aliases", [])


def get_db_connection():
    conn = sqlite3.connect(BINDING_DB)
    conn.row_factory = sqlite3.Row
    return conn


def find_codegraph_db(repo_name):
    """Locate the CodeGraph DB for a repo."""
    path = os.path.join(RAW_ROOT, "code", repo_name, ".codegraph", "codegraph.db")
    return path if os.path.isfile(path) else None


def extract_concepts_from_wiki():
    """
    Scan wiki/concepts/ and extract concept names + aliases.
    Returns list of {name, aliases, code_patterns, file}
    """
    concepts = []
    concepts_dir = os.path.join(WIKI_ROOT, "concepts")
    if not os.path.isdir(concepts_dir):
        return concepts

    for fn in sorted(os.listdir(concepts_dir)):
        if not fn.endswith(".md") or fn == "_context.md":
            continue
        fp = os.path.join(concepts_dir, fn)
        with open(fp, "r", encoding="utf-8") as f:
            content = f.read()

        # Extract concept name from title
        title_match = re.search(r"^#\s+(.+)", content, re.MULTILINE)
        name = title_match.group(1).strip() if title_match else fn[:-3]

        # Extract frontmatter
        fm = {}
        fm_match = re.match(r"^---\n(.*?)\n---", content, re.DOTALL)
        if fm_match:
            for line in fm_match.group(1).split("\n"):
                kv = re.match(r"^(\w+):\s*(.+)", line)
                if kv:
                    fm[kv.group(1)] = kv.group(2).strip().strip('"').strip("'")

        # Extract aliases from frontmatter
        aliases = []
        if "aliases" in fm:
            raw = fm["aliases"]
            aliases = [a.strip().strip('"') for a in raw.strip("[]").split(",")]

        # Extract code_patterns from frontmatter
        patterns = []
        if "code_patterns" in fm:
            raw = fm["code_patterns"]
            patterns = [p.strip().strip('"') for p in raw.strip("[]").split(",")]

        # Extract all `code` mentions for exact matching
        code_refs = SYMBOL_PATTERN.findall(content)
        backtick_refs = [r[0] for r in code_refs if r[0]]

        concepts.append({
            "name": name,
            "slug": fn[:-3],
            "file": f"concepts/{fn}",
            "type": fm.get("type", "unknown"),
            "aliases": aliases,
            "code_patterns": patterns,
            "code_refs": backtick_refs,
        })

    return concepts


def level1_exact_match(concepts, db_path):
    """Level 1: Match concept names/refs to CodeGraph symbol names."""
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    # Get all function names
    symbols = {}
    for r in conn.execute("SELECT id, name, kind, file_path, start_line FROM nodes WHERE kind='function' ORDER BY name"):
        symbols[r["name"].lower()] = {
            "id": r["id"], "name": r["name"], "kind": r["kind"],
            "file": r["file_path"], "line": r["start_line"],
        }

    matches = []
    for c in concepts:
        # Match backtick refs against symbol names
        for ref in c["code_refs"]:
            ref_lower = ref.lower()
            if ref_lower in symbols:
                s = symbols[ref_lower]
                matches.append({
                    "concept": c["name"],
                    "concept_file": c["file"],
                    "symbol": s["name"],
                    "symbol_id": s["id"],
                    "kind": s["kind"],
                    "file": s["file"],
                    "line": s["line"],
                    "relation": "IMPLEMENTS",
                    "level": 1,
                    "confidence": 0.95,
                    "source": "exact_symbol_match",
                })

    conn.close()
    return matches


def level2_alias_match(concepts, db_path):
    """Level 2: Match aliases to CodeGraph symbol names."""
    aliases = load_aliases()
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    symbols = {}
    for r in conn.execute("SELECT id, name, kind, file_path, start_line FROM nodes WHERE kind='function' ORDER BY name"):
        symbols[r["name"].lower()] = r["name"]

    matches = []
    for c in concepts:
        cname_lower = c["name"].lower()
        # Check alias table for this concept
        for entry in aliases:
            canonical_lower = entry["canonical"].lower()
            if canonical_lower in cname_lower or cname_lower in canonical_lower:
                for variant in entry["variants"]:
                    vl = variant.lower()
                    if vl in symbols:
                        matches.append({
                            "concept": c["name"],
                            "concept_file": c["file"],
                            "symbol": symbols[vl],
                            "relation": "IMPLEMENTS",
                            "level": 2,
                            "confidence": 0.80,
                            "source": f"alias: {entry['canonical']} -> {variant}",
                        })

    conn.close()
    return matches


def save_bindings(matches):
    """Save binding candidates to bindings.sqlite and pending/ page."""
    conn = get_db_connection()

    new_count = 0
    for m in matches:
        # Check for existing binding
        existing = conn.execute(
            "SELECT id FROM bindings WHERE concept_id=? AND code_node_id=? AND relation=?",
            (m["concept"], m.get("symbol_id", m.get("symbol", "")), m["relation"])
        ).fetchone()
        if existing:
            continue

        conn.execute(
            "INSERT INTO bindings (concept_id, code_node_id, relation, confidence, status, evidence_json) "
            "VALUES (?, ?, ?, ?, 'pending', ?)",
            (
                m["concept"],
                m.get("symbol_id", m.get("symbol", "")),
                m["relation"],
                m["confidence"],
                json.dumps({"level": m["level"], "source": m.get("source", ""), "file": m.get("file", "")}, ensure_ascii=False)
            )
        )
        new_count += 1

    conn.commit()
    conn.close()
    return new_count


def write_pending_page():
    """Write pending bindings to wiki/pending/code-bindings.md."""
    conn = get_db_connection()
    pending = conn.execute(
        "SELECT * FROM bindings WHERE status='pending' ORDER BY confidence DESC"
    ).fetchall()

    pending_dir = os.path.join(WIKI_ROOT, "pending")
    os.makedirs(pending_dir, exist_ok=True)
    page_path = os.path.join(pending_dir, "code-bindings.md")

    with open(page_path, "w", encoding="utf-8") as f:
        f.write("---\n")
        f.write(f"date: {date.today().isoformat()}\n")
        f.write("tags: [code, bindings, pending]\n")
        f.write("type: concept\n")
        f.write("status: draft\n")
        f.write("---\n\n")
        f.write("# Pending Code Bindings\n\n")
        f.write(f"Total: {len(pending)} candidates awaiting review.\n\n")

        for row in pending:
            evidence = json.loads(row["evidence_json"]) if row["evidence_json"] else {}
            f.write(f"## Binding #{row['id']}: {row['concept_id']}\n\n")
            f.write(f"- **Code symbol**: `{row['code_node_id']}`\n")
            f.write(f"- **Relation**: {row['relation']}\n")
            f.write(f"- **Confidence**: {row['confidence']}\n")
            f.write(f"- **Level**: {evidence.get('level', '?')}\n")
            f.write(f"- **Source**: {evidence.get('source', '?')}\n")
            f.write(f"- **File**: {evidence.get('file', '?')}\n\n")
            f.write("Decision: `kb bind promote %d` → accept\n\n" % row["id"])
            f.write("---\n\n")

    conn.close()
    print(f"  [OK] {page_path} ({len(pending)} pending)")


def propose_bindings(repo_name):
    """Run all binding levels for a repo."""
    db_path = find_codegraph_db(repo_name)
    if not db_path:
        print(f"Error: CodeGraph DB not found for '{repo_name}'")
        sys.exit(1)

    concepts = extract_concepts_from_wiki()
    print(f"  Concepts: {len(concepts)} from wiki/concepts/")

    # Level 1: exact match
    l1 = level1_exact_match(concepts, db_path)
    print(f"  Level 1 (exact): {len(l1)} matches")

    # Level 2: alias match
    l2 = level2_alias_match(concepts, db_path)
    print(f"  Level 2 (alias): {len(l2)} matches")

    all_matches = l1 + l2

    if not all_matches:
        print("  No new bindings found.")
        return

    new = save_bindings(all_matches)
    write_pending_page()
    print(f"\n  Saved: {new} new bindings ({len(all_matches)} total candidates)")


def review_bindings():
    """List all pending bindings."""
    conn = get_db_connection()
    pending = conn.execute(
        "SELECT id, concept_id, code_node_id, relation, confidence, created_at "
        "FROM bindings WHERE status='pending' ORDER BY confidence DESC"
    ).fetchall()
    conn.close()

    if not pending:
        print("  No pending bindings.")
        return

    print(f"  {'ID':<5} {'Concept':<25} {'Symbol':<35} {'Relation':<15} {'Conf':<6} {'Created':<12}")
    print(f"  {'-'*98}")
    for r in pending:
        print(f"  {r['id']:<5} {r['concept_id']:<25} {r['code_node_id']:<35} {r['relation']:<15} {r['confidence']:<6.2f} {r['created_at']:<12}")


def promote_binding(binding_id):
    """Accept a pending binding."""
    conn = get_db_connection()
    conn.execute("UPDATE bindings SET status='accepted' WHERE id=? AND status='pending'", (binding_id,))
    if conn.changes > 0:
        conn.commit()
        print(f"  Binding #{binding_id} promoted to accepted.")
    else:
        print(f"  Binding #{binding_id} not found or already accepted.")
    conn.close()


def main(args):
    if not args:
        print("Usage: kb bind propose/review/promote [args]")
        sys.exit(1)

    cmd = args[0]

    if cmd == "propose":
        repo_name = args[1] if len(args) > 1 else None
        if not repo_name:
            print("Usage: kb bind propose <repo-name>")
            sys.exit(1)
        propose_bindings(repo_name)

    elif cmd == "review":
        review_bindings()

    elif cmd == "promote":
        binding_id = args[1] if len(args) > 1 else None
        if not binding_id:
            print("Usage: kb bind promote <binding-id>")
            sys.exit(1)
        promote_binding(binding_id)

    else:
        print(f"Unknown bind sub-command: {cmd}")


if __name__ == "__main__":
    main(sys.argv[1:])
