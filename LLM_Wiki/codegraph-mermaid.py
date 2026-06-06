#!/usr/bin/env python3
"""
codegraph-mermaid.py — CodeGraph → Mermaid Call Graph Generator

Generates Mermaid flowcharts from CodeGraph's SQLite database, with
architectural layer detection and subgraph grouping.

Usage:
  python3 codegraph-mermaid.py <db-path>                        # List entry functions
  python3 codegraph-mermaid.py <db-path> --function <name>      # Call graph for one function
  python3 codegraph-mermaid.py <db-path> --layers               # Full layer diagram
  python3 codegraph-mermaid.py <db-path> --update-page <page.md> # Embed in wiki page
  python3 codegraph-mermaid.py <db-path> --layer <name>         # Show one layer's graph
"""

import argparse
import os
import re
import sqlite3
import sys

# Layer → Mermaid styling
LAYER_STYLES = {
    "entry":      {"fill": "#1a237e", "color": "#fff", "label": "Entry Points"},
    "interface":  {"fill": "#01579b", "color": "#fff", "label": "Userspace Interface"},
    "interrupt":  {"fill": "#bf360c", "color": "#fff", "label": "Interrupt Handling"},
    "dma":        {"fill": "#1b5e20", "color": "#fff", "label": "DMA Engine"},
    "hardware":   {"fill": "#4a148c", "color": "#fff", "label": "Hardware Abstraction"},
    "smbus":      {"fill": "#e65100", "color": "#fff", "label": "SMBus Management"},
    "monitor":    {"fill": "#b71c1c", "color": "#fff", "label": "Exception Monitor"},
    "boot":       {"fill": "#004d40", "color": "#fff", "label": "Boot Acceleration"},
    "config":     {"fill": "#3e2723", "color": "#fff", "label": "Configuration"},
    "core":       {"fill": "#37474f", "color": "#fff", "label": "Core Logic"},
    "notify":     {"fill": "#006064", "color": "#fff", "label": "Cross-chip Notify"},
}


def connect_db(db_path: str):
    if not os.path.isfile(db_path):
        print(f"Error: database not found at {db_path}", file=sys.stderr)
        sys.exit(1)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    return conn


def detect_layer(file_path: str, name: str, kind: str) -> str:
    """Detect architectural layer (mirrors codegraph-export.py logic)."""
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
    if "led" in name.lower():
        return "config"
    if "dma_pool" in name.lower():
        return "dma"
    if kind in ("struct", "enum", "enum_member"):
        return "data"
    if kind == "import":
        return "dependency"
    return "core"


def get_node_info(conn, node_id: str) -> dict:
    row = conn.execute(
        "SELECT id, name, kind, file_path, start_line, signature FROM nodes WHERE id = ?",
        (node_id,)
    ).fetchone()
    if not row:
        return {"name": node_id.split(":")[-1], "kind": "unknown", "layer": "core"}
    return {
        "name": row["name"],
        "kind": row["kind"],
        "file": row["file_path"],
        "line": row["start_line"],
        "sig": row["signature"] or "",
        "layer": detect_layer(row["file_path"], row["name"], row["kind"]),
    }


def get_callees(conn, func_id: str) -> list[str]:
    return [r[0] for r in conn.execute(
        "SELECT DISTINCT target FROM edges WHERE kind='calls' AND source = ?", (func_id,)
    ).fetchall()]


def get_callers(conn, func_id: str) -> list[str]:
    return [r[0] for r in conn.execute(
        "SELECT DISTINCT source FROM edges WHERE kind='calls' AND target = ?", (func_id,)
    ).fetchall()]


def slugify(name: str) -> str:
    return re.sub(r'[^a-zA-Z0-9_]', '_', name)


def generate_function_graph(conn, func_name: str, depth: int = 1, max_nodes: int = 40) -> str:
    """Enhanced: call graph with layer-colored subgraphs."""
    row = conn.execute(
        "SELECT id, name FROM nodes WHERE kind='function' AND name = ?", (func_name,)
    ).fetchone()
    if not row:
        return f"<!-- Function '{func_name}' not found -->\n"

    func_id = row["id"]
    visited = set()
    edges_set = set()
    nodes_to_visit = [(func_id, 0)]
    node_layers = {}

    while nodes_to_visit:
        current_id, current_depth = nodes_to_visit.pop(0)
        if current_id in visited:
            continue
        visited.add(current_id)

        info = get_node_info(conn, current_id)
        node_layers[current_id] = info["layer"]

        if current_depth >= depth:
            continue

        for callee_id in get_callees(conn, current_id):
            callee_info = get_node_info(conn, callee_id)
            caller_info = get_node_info(conn, current_id)
            edges_set.add((caller_info["name"], callee_info["name"]))
            if callee_id not in visited and len(visited) < max_nodes:
                if callee_info["kind"] == "function":
                    nodes_to_visit.append((callee_id, current_depth + 1))

    if not edges_set:
        return f"<!-- No calls found for '{func_name}' -->\n"

    # Collect layers of involved nodes
    for e in edges_set:
        for n in e:
            r = conn.execute(
                "SELECT id FROM nodes WHERE kind='function' AND name = ?", (n,)
            ).fetchone()
            if r:
                info = get_node_info(conn, r["id"])
                node_layers[r["id"]] = info["layer"]

    # Group nodes by layer
    layer_nodes = {}
    for nid, layer in node_layers.items():
        info = get_node_info(conn, nid)
        layer_nodes.setdefault(layer, []).append(info["name"])

    lines = ["```mermaid", "graph LR"]

    # Subgraphs by layer
    layer_order = ["entry", "interface", "interrupt", "dma", "hardware",
                   "notify", "smbus", "monitor", "boot", "config",
                   "core", "data", "dependency", "source"]
    active_layers = [l for l in layer_order if l in layer_nodes]

    if len(active_layers) >= 2:
        for layer in active_layers:
            style = LAYER_STYLES.get(layer, {"fill": "#555", "color": "#fff", "label": layer})
            safe_label = style["label"].replace('"', "'")
            lines.append(f"    subgraph {layer}[{safe_label}]")
            for name in sorted(set(layer_nodes[layer])):
                s = slugify(name)
                lines.append(f'        {s}["{name}"]')
            lines.append("    end")
            # Style the subgraph
            lines.append(f'    style {layer} fill:{style["fill"]},color:{style["color"]}')

    # Edges
    for caller, callee in sorted(edges_set):
        lines.append(f'    {slugify(caller)}["{caller}"] --> {slugify(callee)}["{callee}"]')

    lines.append("```")
    return "\n".join(lines) + "\n"


def generate_layer_graph(conn, layer_name: str) -> str:
    """Generate a graph showing all functions in one layer and their inter-calls."""
    rows = conn.execute(
        "SELECT id, name, file_path FROM nodes WHERE kind='function' ORDER BY start_line"
    ).fetchall()

    layer_funcs = {}
    for r in rows:
        l = detect_layer(r["file_path"], r["name"], "function")
        if l == layer_name:
            layer_funcs[r["id"]] = r["name"]

    if not layer_funcs:
        return f"<!-- No functions found in layer '{layer_name}' -->\n"

    lines = ["```mermaid", "graph LR"]
    style = LAYER_STYLES.get(layer_name, {"fill": "#555", "color": "#fff", "label": layer_name})

    for fid, fname in layer_funcs.items():
        s = slugify(fname)
        callees = get_callees(conn, fid)
        for cid in callees:
            if cid in layer_funcs:
                cname = layer_funcs[cid]
                lines.append(f'    {slugify(fname)}["{fname}"] --> {slugify(cname)}["{cname}"]')

    # Ensure isolated functions still appear
    for fid, fname in layer_funcs.items():
        s = slugify(fname)
        if not any(f'["{fname}"] -->' in l for l in lines):
            lines.append(f'    {s}["{fname}"]')

    lines.append("```")
    return "\n".join(lines) + "\n"


def generate_all_layers_diagram(conn) -> str:
    """Generate a top-down architecture diagram showing layers and call flow between them."""
    rows = conn.execute(
        "SELECT id, name, file_path FROM nodes WHERE kind='function' ORDER BY start_line"
    ).fetchall()

    # Map each function to its layer
    func_layer = {}
    func_id_name = {}
    for r in rows:
        l = detect_layer(r["file_path"], r["name"], "function")
        func_layer[r["id"]] = l
        func_id_name[r["id"]] = r["name"]

    # Count cross-layer calls
    cross_edges = set()
    funcs_by_layer = {}
    for r in rows:
        funcs_by_layer.setdefault(func_layer[r["id"]], []).append(r["name"])

    for r in rows:
        src_layer = func_layer.get(r["id"])
        if not src_layer:
            continue
        for cid in get_callees(conn, r["id"]):
            tgt_layer = func_layer.get(cid)
            if tgt_layer and src_layer != tgt_layer:
                cross_edges.add((src_layer, tgt_layer,
                                 f"{r['name']} → {func_id_name.get(cid, '?')}"))

    lines = ["```mermaid", "graph TD"]
    layer_order = ["entry", "interface", "interrupt", "dma", "hardware",
                   "notify", "smbus", "monitor", "boot", "config",
                   "core", "data", "dependency"]
    active = [l for l in layer_order if l in funcs_by_layer]

    # Layer nodes
    for l in active:
        style = LAYER_STYLES.get(l, {"fill": "#555", "color": "#fff", "label": l})
        count = len(funcs_by_layer[l])
        lines.append(f'    {l}["{style["label"]} ({count} funcs)"]')
        lines.append(f'    style {l} fill:{style["fill"]},color:{style["color"]}')

    # Cross-layer edges (deduplicated)
    seen_edges = set()
    for src, tgt, label in cross_edges:
        if src in active and tgt in active:
            edge = (src, tgt)
            if edge not in seen_edges:
                seen_edges.add(edge)
                lines.append(f'    {src} -->|{label}| {tgt}')

    lines.append("```")
    return "\n".join(lines) + "\n"


def list_entry_functions(conn):
    """Print entry functions with layer info."""
    all_callees = set()
    for r in conn.execute("SELECT DISTINCT target FROM edges WHERE kind='calls'"):
        all_callees.add(r[0])

    rows = conn.execute(
        "SELECT id, name, kind, file_path, start_line FROM nodes WHERE kind='function' ORDER BY start_line"
    ).fetchall()

    print(f"{'Function':35s} {'Layer':15s} {'Calls':>5s} {'Line':>5s}")
    print("-" * 65)

    for r in rows:
        fid = r["id"]
        layer = detect_layer(r["file_path"], r["name"], r["kind"])
        callee_count = len(get_callees(conn, fid))
        marker = ""
        if r["name"] in ("edge_probe", "edge_remove", "edge_ioctl",
                         "edge_open", "edge_mmap", "edge_udma_isr"):
            marker = "  ⬅ entry"
        elif fid not in all_callees:
            marker = "  (unused)"

        print(f"{r['name']:35s} {layer:15s} {callee_count:5d} {r['start_line']:5d}{marker}")


def update_wiki_page(conn, page_path: str) -> bool:
    """Insert/update CODEGRAPH blocks in a wiki page."""
    if not os.path.isfile(page_path):
        print(f"Error: page not found at {page_path}", file=sys.stderr)
        return False

    with open(page_path, "r", encoding="utf-8") as f:
        content = f.read()

    markers = re.findall(r'\s*<!-- CODEGRAPH:\s*([^\s]+)\s*-->', content)
    if not markers:
        print(f"  No CODEGRAPH markers found in {page_path}")
        print(f"  Add: <!-- CODEGRAPH: function_name -->")
        return False

    for func_name in markers:
        if func_name == "--layers":
            graph = generate_all_layers_diagram(conn)
        elif func_name.startswith("layer:"):
            layer_name = func_name.split(":", 1)[1]
            graph = generate_layer_graph(conn, layer_name)
        else:
            graph = generate_function_graph(conn, func_name, depth=1)

        pattern = re.compile(
            rf'(\s*<!-- CODEGRAPH:\s*{re.escape(func_name)}\s*-->(?:\r?\n)?)(?:```mermaid.*?```\r?\n)?',
            re.DOTALL
        )
        if pattern.search(content):
            content = pattern.sub(r'\1\n' + graph, content)
            print(f"  Updated graph for '{func_name}'")
        else:
            print(f"  Warning: could not update graph for '{func_name}'", file=sys.stderr)

    with open(page_path, "w", encoding="utf-8") as f:
        f.write(content)
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Generate Mermaid call graphs from CodeGraph (with layer support)"
    )
    parser.add_argument("db_path", help="Path to .codegraph/codegraph.db")
    parser.add_argument("--function", "-f", help="Call graph for a function (with layer subgraphs)")
    parser.add_argument("--layers", action="store_true", help="Top-down architecture layer diagram")
    parser.add_argument("--layer", help="Show all internal calls within one layer")
    parser.add_argument("--update-page", help="Update CODEGRAPH markers in a wiki page")
    parser.add_argument("--depth", type=int, default=1, help="Call graph depth (default: 1)")
    args = parser.parse_args()

    conn = connect_db(args.db_path)

    if args.function:
        print(generate_function_graph(conn, args.function, depth=args.depth))
    elif args.layers:
        print(generate_all_layers_diagram(conn))
    elif args.layer:
        print(generate_layer_graph(conn, args.layer))
    elif args.update_page:
        update_wiki_page(conn, args.update_page)
    else:
        list_entry_functions(conn)

    conn.close()


if __name__ == "__main__":
    main()
