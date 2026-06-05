#!/usr/bin/env python3
"""
CodeGraph → Mermaid Call Graph Generator

Reads CodeGraph's SQLite symbol graph and generates Mermaid flowcharts
for embedding in wiki module/architecture pages.

Usage:
  python3 codegraph-mermaid.py <db-path>                        # List available entry functions
  python3 codegraph-mermaid.py <db-path> --function <name>      # Generate graph for one function
  python3 codegraph-mermaid.py <db-path> --file <path>          # Generate graph for all calls in a file
  python3 codegraph-mermaid.py <db-path> --update-page <page.md> # Insert/update Mermaid block in wiki page
  python3 codegraph-mermaid.py <db-path> --all-modules          # Generate graphs for all entry functions
"""

import argparse
import os
import re
import sqlite3
import sys


def connect_db(db_path: str) -> sqlite3.Connection:
    if not os.path.isfile(db_path):
        print(f"Error: database not found at {db_path}", file=sys.stderr)
        sys.exit(1)
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    return conn


def get_functions(conn: sqlite3.Connection) -> list[dict]:
    """Return all function nodes, ordered by start_line."""
    rows = conn.execute("""
        SELECT id, name, qualified_name, file_path, start_line, end_line,
               signature, is_exported, is_static
        FROM nodes
        WHERE kind = 'function'
        ORDER BY start_line
    """).fetchall()
    return [dict(r) for r in rows]


def get_callers(conn: sqlite3.Connection, func_id: str) -> list[str]:
    """Find all functions that call the given function."""
    rows = conn.execute("""
        SELECT DISTINCT e.source
        FROM edges e
        WHERE e.kind = 'calls' AND e.target = ?
    """, (func_id,)).fetchall()
    return [r[0] for r in rows]


def get_callees(conn: sqlite3.Connection, func_id: str) -> list[str]:
    """Find all functions called by the given function."""
    rows = conn.execute("""
        SELECT DISTINCT e.target
        FROM edges e
        WHERE e.kind = 'calls' AND e.source = ?
    """, (func_id,)).fetchall()
    return [r[0] for r in rows]


def node_id_to_name(conn: sqlite3.Connection, node_id: str) -> str:
    """Convert a node ID like 'function:edge_probe' to a display name."""
    row = conn.execute(
        "SELECT name FROM nodes WHERE id = ?", (node_id,)
    ).fetchone()
    return row[0] if row else node_id.split(":")[-1]


def generate_function_graph(conn: sqlite3.Connection, func_name: str,
                            depth: int = 1, max_nodes: int = 30) -> str:
    """
    Generate a Mermaid graph for a function and its callees (1 level deep).
    Returns a Mermaid code block string.
    """
    # Find the function node
    row = conn.execute(
        "SELECT id, name FROM nodes WHERE kind='function' AND name = ?",
        (func_name,)
    ).fetchone()

    if not row:
        return f"<!-- Function '{func_name}' not found in CodeGraph index -->\n"

    func_id = row["id"]
    visited = set()
    edges_set = set()
    nodes_to_visit = [(func_id, 0)]

    while nodes_to_visit:
        current_id, current_depth = nodes_to_visit.pop(0)
        if current_id in visited:
            continue
        visited.add(current_id)
        if current_depth >= depth:
            continue

        callees = get_callees(conn, current_id)
        for callee_id in callees:
            callee_name = node_id_to_name(conn, callee_id)
            current_name = node_id_to_name(conn, current_id)
            edge = (current_name, callee_name)
            if edge not in edges_set:
                edges_set.add(edge)
            if callee_id not in visited and len(visited) < max_nodes:
                # Check if callee is a function (not import/file/etc)
                callee_row = conn.execute(
                    "SELECT kind FROM nodes WHERE id = ?", (callee_id,)
                ).fetchone()
                if callee_row and callee_row["kind"] == "function":
                    nodes_to_visit.append((callee_id, current_depth + 1))

    if not edges_set:
        return f"<!-- No call graph found for '{func_name}' -->\n"

    lines = ["```mermaid", "graph LR"]
    for caller, callee in sorted(edges_set):
        caller_slug = re.sub(r'[^a-zA-Z0-9_]', '_', caller)
        callee_slug = re.sub(r'[^a-zA-Z0-9_]', '_', callee)
        lines.append(f'    {caller_slug}["{caller}"] --> {callee_slug}["{callee}"]')

    lines.append("```")
    return "\n".join(lines) + "\n"


def generate_all_graphs(conn: sqlite3.Connection) -> dict[str, str]:
    """Generate graphs for all top-level entry functions."""
    conn.row_factory = sqlite3.Row
    funcs = get_functions(conn)

    # Identify entry functions: called by nothing in the graph, or named edge_probe/edge_remove etc.
    all_callees = set()
    for row in conn.execute("SELECT DISTINCT target FROM edges WHERE kind='calls'"):
        all_callees.add(row[0])

    entry_funcs = []
    for f in funcs:
        func_id = f"function:{f['name']}"
        if func_id not in all_callees and f['is_static'] == 0:
            entry_funcs.append(f['name'])

    # Always include key entry points even if called
    for name in ['edge_probe', 'edge_remove', 'edge_ioctl', 'edge_open',
                 'edge_mmap', 'edge_udma_isr', 'module_init', 'module_exit']:
        if name not in entry_funcs:
            match = [f for f in funcs if f['name'] == name]
            if match:
                entry_funcs.append(name)

    results = {}
    for name in entry_funcs:
        graph = generate_function_graph(conn, name, depth=1)
        if graph and 'No call graph' not in graph:
            results[name] = graph
    return results


def update_wiki_page(conn: sqlite3.Connection, page_path: str) -> bool:
    """
    Insert or update a Mermaid call graph block in a wiki page.
    Looks for a `<!-- CODEGRAPH: <func_name> -->` marker in the page.
    """
    if not os.path.isfile(page_path):
        print(f"Error: page not found at {page_path}", file=sys.stderr)
        return False

    with open(page_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # Look for CODEGRAPH markers (with optional leading whitespace)
    markers = re.findall(r'\s*<!-- CODEGRAPH:\s*([^\s]+)\s*-->', content)

    if not markers:
        print(f"  No CODEGRAPH markers found in {page_path}")
        print(f"  Add a comment like: <!-- CODEGRAPH: edge_probe -->")
        return False

    for func_name in markers:
        graph = generate_function_graph(conn, func_name, depth=1)

        # Replace content between CODEGRAPH marker and next ```mermaid...``` block
        pattern = re.compile(
            rf'(\s*<!-- CODEGRAPH:\s*{re.escape(func_name)}\s*-->(?:\r?\n)?)(?:```mermaid.*?```\r?\n)?',
            re.DOTALL
        )

        if pattern.search(content):
            content = pattern.sub(r'\1\n' + graph, content)
            print(f"  Updated graph for '{func_name}' in {page_path}")
        else:
            print(f"  Warning: could not update graph for '{func_name}'", file=sys.stderr)

    with open(page_path, 'w', encoding='utf-8') as f:
        f.write(content)
    return True


def list_entry_functions(conn: sqlite3.Connection):
    """Print all entry-level functions for reference."""
    funcs = get_functions(conn)

    all_callees = set()
    for row in conn.execute("SELECT DISTINCT target FROM edges WHERE kind='calls'"):
        all_callees.add(row[0])

    print(f"Total functions: {len(funcs)}")
    print(f"\nEntry functions (not called by anything in the graph):")
    for f in funcs:
        func_id = f"function:{f['name']}"
        callees = get_callees(conn, func_id)
        callers = get_callers(conn, func_id)
        suffix = ""
        if f['name'] in ('edge_probe', 'edge_remove', 'edge_ioctl', 'edge_open',
                         'edge_mmap', 'edge_udma_isr'):
            suffix = "  ← KEY ENTRY POINT"
        elif f['is_exported']:
            suffix = "  (exported)"
        elif func_id not in all_callees:
            suffix = "  (no callers)"
        print(f"  {f['name']:35s} calls={len(callees):3d} line={f['start_line']:5d}{suffix}")


def main():
    parser = argparse.ArgumentParser(
        description="Generate Mermaid call graphs from CodeGraph SQLite database"
    )
    parser.add_argument("db_path", help="Path to .codegraph/codegraph.db")
    parser.add_argument("--function", "-f", help="Generate graph for a specific function")
    parser.add_argument("--file", help="Generate graph for all functions in a file")
    parser.add_argument("--update-page", help="Update a wiki page with Mermaid graphs (uses CODEGRAPH markers)")
    parser.add_argument("--all-modules", action="store_true", help="Generate graphs for all entry functions")
    parser.add_argument("--depth", type=int, default=1, help="Call graph depth (default: 1)")
    args = parser.parse_args()

    conn = connect_db(args.db_path)

    if args.function:
        graph = generate_function_graph(conn, args.function, depth=args.depth)
        print(graph)

    elif args.file:
        funcs = get_functions(conn)
        file_funcs = [f for f in funcs if f['file_path'] == args.file]
        for f in file_funcs:
            graph = generate_function_graph(conn, f['name'], depth=args.depth)
            if graph and 'No call graph' not in graph:
                print(f"\n<!-- {f['name']} -->")
                print(graph)

    elif args.update_page:
        update_wiki_page(conn, args.update_page)

    elif args.all_modules:
        graphs = generate_all_graphs(conn)
        for name, graph in sorted(graphs.items()):
            print(f"<!-- {name} -->")
            print(graph)
            print()

    else:
        list_entry_functions(conn)

    conn.close()


if __name__ == "__main__":
    main()
