#!/usr/bin/env python3
"""
codegraph-export.py — Export CodeGraph SQLite → Standardized Knowledge Graph JSON

Produces a knowledge-graph.json in understand-anything compatible format:
  - nodes: [{id, kind, name, file, lines, signature, layer, properties}]
  - edges: [{source, target, kind, line}]
  - layers: [{name, description, node_count}]
  - meta: {project, language, stats}

Usage:
  python3 codegraph-export.py <codegraph.db> [--output <path>]
  python3 codegraph-export.py <codegraph.db> --layers       # Print detected layers
  python3 codegraph-export.py <codegraph.db> --stats        # Print statistics
"""

import argparse
import json
import os
import re
import sqlite3
import sys


def connect(db_path):
    if not os.path.isfile(db_path):
        print(f"Error: {db_path} not found", file=sys.stderr)
        sys.exit(1)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    return conn


def export_nodes(conn) -> list:
    """Export all nodes with full metadata."""
    nodes = []
    for row in conn.execute("SELECT * FROM nodes ORDER BY start_line"):
        kind = row["kind"]
        # Map CodeGraph kinds to standard knowledge graph kinds
        kind_map = {
            "function": "function",
            "struct": "class",
            "enum": "enum",
            "enum_member": "enum_member",
            "import": "import",
            "variable": "variable",
            "file": "file",
        }
        std_kind = kind_map.get(kind, kind)

        n = {
            "id": row["id"],
            "kind": std_kind,
            "name": row["name"],
            "file": row["file_path"],
            "lines": [row["start_line"], row["end_line"]],
            "language": row["language"],
        }
        if row["signature"]:
            n["signature"] = row["signature"]
        if row["docstring"]:
            n["docstring"] = row["docstring"]
        if row["is_exported"]:
            n["exported"] = True
        if row["is_static"]:
            n["static"] = True

        # Determine layer based on file path and naming conventions
        n["layer"] = detect_layer(row["file_path"], row["name"], kind)
        nodes.append(n)
    return nodes


def export_edges(conn, nodes_by_id: dict) -> list:
    """Export all edges as source/target/kind triples."""
    edges = []
    for row in conn.execute("SELECT * FROM edges ORDER BY kind, source"):
        # Skip 'contains' edges (file→symbol containment) — too noisy
        if row["kind"] == "contains":
            continue

        e = {
            "source": row["source"],
            "target": row["target"],
            "kind": row["kind"],
        }
        if row["line"]:
            e["line"] = row["line"]
        edges.append(e)
    return edges


def detect_layer(file_path: str, name: str, kind: str) -> str:
    """
    Detect architectural layer from file path and naming conventions.
    Returns one of: core, interface, utility, config, hardware, entry.
    """
    fp = file_path.lower()

    # Entry points
    if name in ("edge_probe", "edge_remove", "init_module", "cleanup_module",
                "edge_init", "edge_exit"):
        return "entry"
    if kind == "file":
        return "source"

    # Hardware / register layer
    if name.startswith("edge_readl") or name.startswith("edge_writel"):
        return "hardware"
    if "msigen" in name or "bar" in name or "pci_tbl" in name:
        return "hardware"
    if "reg" in name.lower() and ("ctrl" in name.lower() or "stat" in name.lower()):
        return "hardware"

    # Exception handling
    if "exception" in name.lower():
        return "monitor"

    # DMA / transfer
    if any(x in name.lower() for x in ["dma", "udma", "transfer", "sgm_", "p2p_"]):
        return "dma"

    # Interrupt
    if any(x in name.lower() for x in ["irq", "isr", "int", "msi"]):
        return "interrupt"

    # SMBus
    if "smbus" in name.lower():
        return "smbus"

    # Notify IRQ (cross-chip communication)
    if "notify_irq" in name.lower():
        return "notify"

    # IOCTL / userspace interface
    if name.startswith("ioctl_") or name in ("edge_open", "edge_release",
                                              "edge_read", "edge_write",
                                              "edge_mmap", "edge_llseek"):
        return "interface"

    # Character device / proc
    if "cdev" in name or "proc_" in name or "version" in name:
        return "interface"

    # Boot
    if "boot" in name.lower():
        return "boot"

    # LED
    if "led" in name.lower():
        return "config"

    # DMA pool
    if "dma_pool" in name.lower():
        return "dma"

    # Struct definitions
    if kind == "struct":
        return "data"
    if kind == "enum" or kind == "enum_member":
        return "data"

    # Import
    if kind == "import":
        return "dependency"

    # Default
    return "core"


def detect_layers(nodes: list) -> list:
    """Aggregate nodes into architectural layers."""
    layer_map = {}
    for n in nodes:
        l = n.get("layer", "core")
        if l not in layer_map:
            layer_map[l] = {"name": l, "node_count": 0, "kinds": set()}
        layer_map[l]["node_count"] += 1
        layer_map[l]["kinds"].add(n["kind"])

    # Layer descriptions
    descriptions = {
        "entry": "Entry points: probe/remove, module init/exit",
        "core": "Core logic: general driver functions",
        "hardware": "Hardware abstraction: register access, PCI config",
        "interrupt": "Interrupt handling: IRQ, MSI/MSI-X, ISR registration",
        "dma": "DMA engine: uDMA transfers, scatter-gather, P2P",
        "interface": "Userspace interface: IOCTL, char device, mmap",
        "monitor": "Exception monitoring: hardware error detection",
        "notify": "Cross-chip communication: notify IRQ channels",
        "smbus": "SMBus management: arbitration, hotplug, ARP",
        "boot": "Boot acceleration: pre-OS DMA",
        "config": "Configuration: module params, LED, GPIO",
        "data": "Data structures: struct, enum definitions",
        "dependency": "External imports: kernel headers",
        "source": "Source files",
    }

    layers = []
    for l_name in ["entry", "interface", "interrupt", "dma", "hardware",
                   "notify", "smbus", "monitor", "boot", "config",
                   "core", "data", "dependency", "source"]:
        if l_name in layer_map:
            info = layer_map[l_name]
            layers.append({
                "name": l_name,
                "description": descriptions.get(l_name, ""),
                "node_count": info["node_count"],
                "kinds": sorted(info["kinds"]),
            })
    return layers


def build_repo_tree(nodes: list) -> dict:
    """Build a file-level directory tree from nodes."""
    files = {}
    for n in nodes:
        if n["kind"] == "file":
            continue
        f = n["file"]
        if f not in files:
            files[f] = {"functions": 0, "structs": 0, "enums": 0, "imports": 0}
        k = n["kind"]
        if k == "function":
            files[f]["functions"] += 1
        elif k == "class":
            files[f]["structs"] += 1
        elif k in ("enum", "enum_member"):
            files[f]["enums"] += 1
        elif k == "import":
            files[f]["imports"] += 1
    return files


def main():
    parser = argparse.ArgumentParser(
        description="Export CodeGraph SQLite → Knowledge Graph JSON"
    )
    parser.add_argument("db_path", help="Path to .codegraph/codegraph.db")
    parser.add_argument("--output", "-o", default=None,
                        help="Output path (default: stdout)")
    parser.add_argument("--layers", action="store_true",
                        help="Print detected layers and exit")
    parser.add_argument("--stats", action="store_true",
                        help="Print statistics and exit")
    args = parser.parse_args()

    conn = connect(args.db_path)

    # Build
    nodes = export_nodes(conn)
    nodes_by_id = {n["id"]: n for n in nodes}
    edges = export_edges(conn, nodes_by_id)
    layers = detect_layers(nodes)
    repo_tree = build_repo_tree(nodes)

    # Project info
    proj_row = conn.execute(
        "SELECT value FROM project_metadata WHERE key = 'name'"
    ).fetchone()
    project_name = proj_row[0] if proj_row else os.path.basename(os.path.dirname(args.db_path))

    kg = {
        "meta": {
            "project": project_name,
            "source": os.path.relpath(args.db_path),
            "generated": "2026-06-06",
            "tool": "codegraph-export.py",
        },
        "stats": {
            "nodes": len(nodes),
            "edges": len(edges),
            "layers": len(layers),
            "files": len(repo_tree),
            "functions": sum(1 for n in nodes if n["kind"] == "function"),
            "structs": sum(1 for n in nodes if n["kind"] == "class"),
        },
        "layers": layers,
        "nodes": nodes,
        "edges": edges,
        "tree": repo_tree,
    }

    # Handle output modes
    if args.stats:
        print(f"Project: {project_name}")
        print(f"  Nodes: {kg['stats']['nodes']} ({kg['stats']['functions']} functions, {kg['stats']['structs']} structs)")
        print(f"  Edges: {kg['stats']['edges']}")
        print(f"  Files: {kg['stats']['files']}")
        print(f"  Layers: {kg['stats']['layers']}")
        print()
        print("Layers:")
        for l in layers:
            print(f"  {l['name']:15s} {l['node_count']:4d} nodes  {l['description']}")
        return

    if args.layers:
        print(f"Architectural Layers for {project_name}:")
        print()
        for l in layers:
            kinds = ", ".join(l["kinds"])
            print(f"  [{l['name']:15s}] {l['description']}")
            print(f"  {'':17s}{l['node_count']} nodes  [{kinds}]")
            print()
        return

    # Output full JSON
    output = args.output
    if output:
        with open(output, "w", encoding="utf-8") as f:
            json.dump(kg, f, ensure_ascii=False, indent=2)
        size = os.path.getsize(output)
        print(f"Written: {output} ({size:,} bytes, {kg['stats']['nodes']} nodes, {kg['stats']['edges']} edges)")
    else:
        print(json.dumps(kg, ensure_ascii=False, indent=2))

    conn.close()


if __name__ == "__main__":
    main()
