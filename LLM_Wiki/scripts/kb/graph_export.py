"""
graph_export.py — 知识图谱导出 + Mermaid 可视化 (Phase 3)

用法:
  python3 scripts/kb/graph_export.py export <repo-name>     → JSON
  python3 scripts/kb/graph_export.py mermaid <func-name>    → Mermaid
  python3 scripts/kb/graph_export.py layers <repo-name>     → Layer table
"""

import json
import os
import sqlite3
import re
import sys
from datetime import date

WIKI_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "wiki"))
RAW_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "raw"))
KB_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".kb"))
EXPORT_DIR = os.path.join(KB_DIR, "graph_exports")

LAYER_STYLES = {
    "entry": "fill:#1a237e,color:#fff", "interface": "fill:#01579b,color:#fff",
    "interrupt": "fill:#bf360c,color:#fff", "dma": "fill:#1b5e20,color:#fff",
    "hardware": "fill:#4a148c,color:#fff", "smbus": "fill:#e65100,color:#fff",
    "monitor": "fill:#b71c1c,color:#fff", "boot": "fill:#004d40,color:#fff",
    "config": "fill:#3e2723,color:#fff", "core": "fill:#37474f,color:#fff",
    "notify": "fill:#006064,color:#fff", "data": "fill:#555,color:#fff",
    "dependency": "fill:#666,color:#fff",
}


def find_db(repo_name):
    path = os.path.join(RAW_ROOT, "code", repo_name, ".codegraph", "codegraph.db")
    return path if os.path.isfile(path) else None


def detect_layer(fp, name, kind):
    fp_lower = fp.lower()
    if name in ("edge_probe", "edge_remove"): return "entry"
    if name.startswith("edge_readl") or name.startswith("edge_writel"): return "hardware"
    if "exception" in name.lower(): return "monitor"
    if any(x in name.lower() for x in ["dma", "udma", "transfer"]): return "dma"
    if any(x in name.lower() for x in ["irq", "isr", "msi"]): return "interrupt"
    if "smbus" in name.lower(): return "smbus"
    if name.startswith("ioctl_") or name in ("edge_open", "edge_mmap"): return "interface"
    if "boot" in name.lower(): return "boot"
    if kind in ("struct", "enum"): return "data"
    return "core"


def export_json(repo_name):
    db_path = find_db(repo_name)
    if not db_path:
        print(f"Error: no CodeGraph DB for '{repo_name}'")
        return

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    nodes = []
    for r in conn.execute("SELECT * FROM nodes ORDER BY start_line"):
        kind_map = {"function": "function", "struct": "class", "enum": "enum",
                     "enum_member": "enum_member", "import": "import",
                     "variable": "variable", "file": "file"}
        n = {
            "id": r["id"], "kind": kind_map.get(r["kind"], r["kind"]),
            "name": r["name"], "file": r["file_path"],
            "lines": [r["start_line"], r["end_line"]],
            "layer": detect_layer(r["file_path"], r["name"], r["kind"]),
            "language": r["language"],
        }
        if r["signature"]: n["signature"] = r["signature"]
        if r["is_exported"]: n["exported"] = True
        nodes.append(n)

    edges = []
    for r in conn.execute("SELECT * FROM edges WHERE kind != 'contains' ORDER BY kind"):
        edges.append({"source": r["source"], "target": r["target"],
                       "kind": r["kind"], "line": r["line"]})

    # Layer aggregation
    layers = {}
    for n in nodes:
        l = n["layer"]
        if l not in layers:
            layers[l] = {"name": l, "node_count": 0, "kinds": set()}
        layers[l]["node_count"] += 1
        layers[l]["kinds"].add(n["kind"])
    layers_list = sorted(
        [{"name": k, "node_count": v["node_count"], "kinds": sorted(v["kinds"])}
         for k, v in layers.items()],
        key=lambda x: -x["node_count"]
    )

    kg = {
        "meta": {"project": repo_name, "generated": str(date.today()),
                  "source": "CodeGraph", "tool": "kb graph export"},
        "stats": {"nodes": len(nodes), "edges": len(edges),
                   "layers": len(layers_list)},
        "layers": layers_list,
        "nodes": nodes,
        "edges": edges,
    }

    os.makedirs(EXPORT_DIR, exist_ok=True)
    out_path = os.path.join(EXPORT_DIR, f"{repo_name}-knowledge-graph.json")
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(kg, f, ensure_ascii=False, indent=2)

    size = os.path.getsize(out_path)
    print(f"  [OK] {out_path} ({size:,} bytes, {len(nodes)} nodes, {len(edges)} edges)")
    conn.close()
    return kg


def mermaid_call_graph(func_name, db_path):
    """Generate Mermaid with layer subgraphs for a function."""
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    row = conn.execute(
        "SELECT id, name FROM nodes WHERE kind='function' AND name=?", (func_name,)
    ).fetchone()
    if not row:
        print(f"  Function '{func_name}' not found")
        return

    func_id = row["id"]
    visited = set()
    edges_set = set()
    node_layers = {}
    queue = [(func_id, 0)]

    while queue:
        cur, depth = queue.pop(0)
        if cur in visited:
            continue
        visited.add(cur)
        r = conn.execute("SELECT name, file_path FROM nodes WHERE id=?", (cur,)).fetchone()
        if r:
            l = detect_layer(r["file_path"], r["name"], "function")
            node_layers[cur] = l
        if depth >= 1:
            continue
        for callee in conn.execute(
            "SELECT DISTINCT target FROM edges WHERE kind='calls' AND source=?", (cur,)
        ).fetchall():
            cid = callee[0]
            caller_name = r["name"] if r else func_name
            cr = conn.execute("SELECT name FROM nodes WHERE id=?", (cid,)).fetchone()
            callee_name = cr[0] if cr else cid.split(":")[-1]
            edges_set.add((caller_name, callee_name))
            if cid not in visited:
                queue.append((cid, depth + 1))

    if not edges_set:
        print(f"  No calls found for '{func_name}'")
        return

    # Build layer→nodes map
    lnodes = {}
    for e in edges_set:
        for n in e:
            r = conn.execute("SELECT id, file_path FROM nodes WHERE kind='function' AND name=?", (n,)).fetchone()
            if r:
                l = detect_layer(r["file_path"], n, "function")
                lnodes.setdefault(l, set()).add(n)

    lines = ["```mermaid", "graph LR"]
    layer_order = ["entry", "interface", "interrupt", "dma", "hardware",
                   "notify", "smbus", "monitor", "boot", "config", "core"]
    for l in layer_order:
        if l in lnodes:
            style = LAYER_STYLES.get(l, "fill:#555,color:#fff")
            lines.append(f"    subgraph {l}[{l}]")
            for n in sorted(lnodes[l]):
                sl = re.sub(r'[^a-zA-Z0-9_]', '_', n)
                lines.append(f"        {sl}[\"{n}\"]")
            lines.append("    end")
            lines.append(f"    style {l} {style}")

    for c, e in sorted(edges_set):
        cs = re.sub(r'[^a-zA-Z0-9_]', '_', c)
        es2 = re.sub(r'[^a-zA-Z0-9_]', '_', e)
        lines.append(f'    {cs}["{c}"] --> {es2}["{e}"]')

    lines.append("```")
    conn.close()
    print("\n".join(lines))


def layers_table(repo_name):
    """Print architecture layers as markdown table."""
    db_path = find_db(repo_name)
    if not db_path:
        print(f"Error: no CodeGraph DB for '{repo_name}'")
        return

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    func_layer = {}
    for r in conn.execute("SELECT id, name, file_path, kind FROM nodes"):
        l = detect_layer(r["file_path"], r["name"], r["kind"])
        func_layer.setdefault(l, 0)
        func_layer[l] += 1

    conn.close()
    print(f"| Layer | Nodes |")
    print(f"|-------|-------|")
    for l, cnt in sorted(func_layer.items(), key=lambda x: -x[1]):
        print(f"| {l} | {cnt} |")


def main(args):
    if not args:
        print("Usage: kb graph export/mermaid/layers [args]")
        sys.exit(1)

    cmd = args[0]

    if cmd == "export":
        repo_name = args[1] if len(args) > 1 else None
        if not repo_name:
            print("Usage: kb graph export <repo-name>")
            sys.exit(1)
        export_json(repo_name)

    elif cmd == "mermaid":
        func_name = args[1] if len(args) > 1 else None
        repo_name = args[2] if len(args) > 2 else "pcie"
        if not func_name:
            print("Usage: kb graph mermaid <func-name> [repo-name]")
            sys.exit(1)
        db_path = find_db(repo_name)
        if not db_path:
            print(f"Error: no CodeGraph DB for '{repo_name}'")
            sys.exit(1)
        mermaid_call_graph(func_name, db_path)

    elif cmd == "layers":
        repo_name = args[1] if len(args) > 1 else "pcie"
        layers_table(repo_name)

    else:
        print(f"Unknown graph sub-command: {cmd}")


if __name__ == "__main__":
    main(sys.argv[1:])
