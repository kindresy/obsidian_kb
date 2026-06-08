"""
compile_codebase.py — CodeGraph 索引 → wiki/codebases/ + wiki/flows/ 页面

Phase 1: 代码仓库编译为 wiki 页面。

用法:
  python3 scripts/kb/compile_codebase.py <repo-name> [--codegraph-db <path>]
"""

import json
import os
import re
import sqlite3
import sys
from datetime import date

WIKI_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "wiki"))
CG_DB = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "raw", "code"))

LAYER_ORDER = [
    "entry", "interface", "interrupt", "dma", "hardware",
    "notify", "smbus", "monitor", "boot", "config",
    "core", "data", "dependency", "source"
]

LAYER_DESC = {
    "entry": "Entry points", "interface": "Userspace interface",
    "interrupt": "Interrupt handling", "dma": "DMA engine",
    "hardware": "Hardware abstraction", "notify": "Cross-chip notify",
    "smbus": "SMBus management", "monitor": "Exception monitor",
    "boot": "Boot acceleration", "config": "Configuration",
    "core": "Core logic", "data": "Data structures",
    "dependency": "External imports", "source": "Source files",
}


def detect_layer(file_path, name, kind):
    fp = file_path.lower()
    if name in ("edge_probe", "edge_remove", "edge_init", "edge_exit"):
        return "entry"
    if kind == "file":
        return "source"
    if name.startswith("edge_readl") or name.startswith("edge_writel"):
        return "hardware"
    if "msigen" in name or "bar" in name or "pci_tbl" in name:
        return "hardware"
    if "exception" in name.lower():
        return "monitor"
    if any(x in name.lower() for x in ["dma", "udma", "transfer", "sgm_", "p2p_"]):
        return "dma"
    if any(x in name.lower() for x in ["irq", "isr", "int", "msi"]):
        return "interrupt"
    if "smbus" in name.lower():
        return "smbus"
    if "notify_irq" in name.lower():
        return "notify"
    if name.startswith("ioctl_") or name in ("edge_open", "edge_release",
                                              "edge_read", "edge_write",
                                              "edge_mmap", "edge_llseek"):
        return "interface"
    if "cdev" in name or "proc_" in name or "version" in name:
        return "interface"
    if "boot" in name.lower():
        return "boot"
    if "dma_pool" in name.lower():
        return "dma"
    if kind in ("struct", "enum", "enum_member"):
        return "data"
    if kind == "import":
        return "dependency"
    return "core"


def get_callers(conn, func_id):
    return [r[0] for r in conn.execute(
        "SELECT DISTINCT source FROM edges WHERE kind='calls' AND target = ?", (func_id,))]


def get_callees(conn, func_id):
    return [r[0] for r in conn.execute(
        "SELECT DISTINCT target FROM edges WHERE kind='calls' AND source = ?", (func_id,))]


def get_node_name(conn, node_id):
    r = conn.execute("SELECT name FROM nodes WHERE id = ?", (node_id,)).fetchone()
    return r[0] if r else node_id.split(":")[-1]


def compile_codebase(db_path, repo_name):
    """Main entry: read CodeGraph DB, write codebase page + flow pages."""
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    # --- Gather data ---
    # All functions
    funcs = conn.execute(
        "SELECT id, name, file_path, start_line, end_line, signature, is_static, is_exported "
        "FROM nodes WHERE kind='function' ORDER BY start_line"
    ).fetchall()

    # All call edges
    calls = conn.execute(
        "SELECT source, target FROM edges WHERE kind='calls'"
    ).fetchall()

    # Files
    files = conn.execute("SELECT path, node_count, language FROM files").fetchall()

    # Layer assignment
    func_by_layer = {}
    for f in funcs:
        l = detect_layer(f["file_path"], f["name"], "function")
        func_by_layer.setdefault(l, []).append(f["name"])

    # Entry point identification (functions with no callers)
    all_callees = set(r["target"] for r in calls)
    entry_calls = []
    for f in funcs:
        if f["name"] in ("edge_probe", "edge_remove", "edge_ioctl",
                         "edge_open", "edge_mmap", "edge_udma_isr"):
            entry_calls.append(f["name"])
        elif f["id"] not in all_callees:
            entry_calls.append(f["name"])

    # Top symbols by complexity (inbound + outbound edges)
    call_count = {}
    for r in calls:
        call_count[r["source"]] = call_count.get(r["source"], 0) + 1
        call_count[r["target"]] = call_count.get(r["target"], 0) + 1
    top_symbols = sorted(
        [(get_node_name(conn, nid), cnt) for nid, cnt in call_count.items()
         if nid.startswith("function:")],
        key=lambda x: -x[1]
    )[:20]

    # Commit info
    commit_row = conn.execute(
        "SELECT value FROM project_metadata WHERE key = 'lastCommit'"
    ).fetchone()
    commit_sha = commit_row[0] if commit_row else "unknown"

    today = date.today().isoformat()

    # --- Write codebase page ---
    codebase_dir = os.path.join(WIKI_ROOT, "codebases")
    os.makedirs(codebase_dir, exist_ok=True)
    page_path = os.path.join(codebase_dir, f"{repo_name}.md")

    with open(page_path, "w", encoding="utf-8") as f:
        f.write("---\n")
        f.write(f"date: {today}\n")
        f.write(f"tags: [code, {repo_name}]\n")
        f.write("type: concept\n")
        f.write("status: active\n")
        f.write(f"source-repo: {repo_name}\n")
        f.write(f"code-commit: {commit_sha}\n")
        f.write("---\n\n")
        f.write(f"# {repo_name} — Codebase Overview\n\n")

        f.write("## Purpose\n")
        f.write("(TODO)\n\n")

        f.write("## Directory Map\n")
        f.write("```\n")
        for r in files:
            f.write(f"  {r['path']:30s} nodes={r['node_count']} lang={r['language']}\n")
        f.write("```\n\n")

        f.write(f"## Build System\n")
        f.write("(TODO)\n\n")

        f.write("## Architecture Layers\n\n")
        f.write("| Layer | Nodes | Description |\n")
        f.write("|-------|-------|-------------|\n")
        for l in LAYER_ORDER:
            if l in func_by_layer:
                f.write(f"| **{l}** | {len(func_by_layer[l])} | {LAYER_DESC.get(l, '')} |\n")
        f.write("\n")

        f.write("## Entry Points\n\n")
        for name in entry_calls[:10]:
            f.write(f"- `{name}()`\n")
        f.write("\n")

        f.write("## Key Symbols (top by call complexity)\n\n")
        f.write("| Symbol | Call Count |\n")
        f.write("|--------|-----------|\n")
        for name, cnt in top_symbols:
            f.write(f"| `{name}()` | {cnt} |\n")
        f.write("\n")

        f.write("## Related Concepts\n")
        f.write("(Populated by `kb bind propose`)\n\n")

        f.write("## Generated Graphs\n")
        f.write(f"See: `kb graph export {repo_name}`\n\n")
        f.write("## Counter-Arguments and Gaps\n\n")
        f.write("...\n")

    print(f"  [OK] {page_path}")

    # --- Write _context.md for codebases/ ---
    ctx_path = os.path.join(codebase_dir, "_context.md")
    if not os.path.exists(ctx_path):
        with open(ctx_path, "w") as f:
            f.write("This directory contains codebase overview pages (one per repository).\n")
            f.write("Each page is generated by `kb code compile <repo-name>` and provides\n")
            f.write("a high-level summary of the repo's architecture, layers, and entry points.\n")

    conn.close()
    print(f"  Compiled {len(funcs)} functions, {len(calls)} call edges, {len(files)} files")
    return True


def main(args):
    # args[0] is the sub-command ("compile"), repo name is args[1]
    repo_idx = 1 if args and args[0] == "compile" else 0
    if len(args) <= repo_idx:
        print("Usage: kb code compile <repo-name> [--codegraph-db <path>]")
        sys.exit(1)

    repo_name = args[repo_idx]

    # Find CodeGraph DB
    if "--codegraph-db" in args:
        idx = args.index("--codegraph-db")
        db_path = args[idx + 1]
    else:
        db_path = os.path.join(CG_DB, repo_name, ".codegraph", "codegraph.db")

    if not os.path.isfile(db_path):
        print(f"Error: CodeGraph DB not found at {db_path}")
        print(f"Run 'kb code index {repo_name}' first.")
        sys.exit(1)

    compile_codebase(db_path, repo_name)


if __name__ == "__main__":
    main(sys.argv[1:])
