"""
ask.py — Hybrid Query: wiki 概念页 + codegraph 代码符号

用法:
  python3 scripts/kb/ask.py "<question>"                     # 查询所有仓库
  python3 scripts/kb/ask.py "<question>" --repo <repo-name>   # 限定仓库
"""

import json
import os
import subprocess
import sys
from datetime import date

WIKI_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "wiki"))
RAW_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "raw"))
KB_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".kb"))
BINDING_DB = os.path.join(KB_DIR, "bindings.sqlite")


def vector_search(query, top_k=5):
    """Search wiki pages via vector index."""
    result = subprocess.run(
        ["python3", "search-index.py", query, "--top-k", str(top_k)],
        capture_output=True, text=True, cwd=os.path.dirname(__file__) + "/../..",
        timeout=120
    )
    if result.returncode == 0 and result.stdout.strip():
        try:
            return json.loads(result.stdout)
        except json.JSONDecodeError:
            return []
    return []


def codegraph_query(query, repo_name):
    """Search code symbols via CodeGraph."""
    code_dir = os.path.join(RAW_ROOT, "code", repo_name)
    if not os.path.isdir(code_dir):
        return None

    try:
        result = subprocess.run(
            ["npx.cmd", "@colbymchenry/codegraph", "query", query, "-p", code_dir, "-j"],
            capture_output=True, text=True, timeout=30
        )
        if result.returncode == 0 and result.stdout.strip():
            return json.loads(result.stdout)
    except (FileNotFoundError, subprocess.TimeoutExpired, json.JSONDecodeError):
        pass
    return None


def find_code_repos():
    """List available code repos."""
    code_dir = os.path.join(RAW_ROOT, "code")
    if not os.path.isdir(code_dir):
        return []
    return [d for d in os.listdir(code_dir) if os.path.isdir(os.path.join(code_dir, d, ".codegraph"))]


def main(args):
    if not args or args[0] in ("-h", "--help"):
        print(__doc__.strip())
        sys.exit(0)

    # Parse args
    question = args[0]
    repo_filter = None
    if "--repo" in args:
        idx = args.index("--repo")
        if idx + 1 < len(args):
            repo_filter = args[idx + 1]

    print(f"Query: {question}\n")

    # 1. Vector search wiki
    print("=== Wiki Pages (Vector Search) ===")
    results = vector_search(question)
    if results:
        seen = set()
        for r in results:
            page = r["page"]
            if page not in seen:
                seen.add(page)
                score = r.get("score", 0)
                stars = "HIGH" if score > 0.7 else "  ok"
                print(f"  {stars} {page}  (score: {score:.3f})")
    else:
        print("  (no wiki results)")
    print()

    # 2. CodeGraph symbol search
    repos = find_code_repos()
    if repo_filter:
        repos = [r for r in repos if repo_filter in r]

    if repos:
        print("=== Code Symbols (CodeGraph) ===")
        for repo in repos:
            cg = codegraph_query(question, repo)
            if cg:
                for sym in cg[:5]:
                    node = sym.get("node", {})
                    name = node.get("name", sym.get("name", ""))
                    kind = node.get("kind", sym.get("kind", ""))
                    file_p = node.get("filePath", sym.get("file", ""))
                    score = sym.get("score", 0)
                    print(f"  [{repo}] {kind}: {name}  ({file_p})")
            else:
                print(f"  [{repo}] (no matches)")
    else:
        print("  (no code repos indexed)")

    # 3. Check bindings
    if os.path.isfile(BINDING_DB):
        import sqlite3
        conn = sqlite3.connect(BINDING_DB)
        bindings = conn.execute(
            "SELECT concept_id, code_node_id, status FROM bindings WHERE status='accepted'"
        ).fetchall()
        if bindings:
            print(f"\n=== Related Bindings ({len(bindings)} accepted) ===")
            conn.close()

    print()


if __name__ == "__main__":
    main(sys.argv[1:])
