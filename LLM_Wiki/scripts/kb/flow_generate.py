"""
flow_generate.py — 从 CodeGraph 调用链生成 Flow 教学页面 (Phase 3)

用法:
  python3 scripts/kb/flow_generate.py <entry-func> <repo-name> [--title <title>]
  python3 scripts/kb/flow_generate.py i3c_master_register linux-i3c --title "I3C Master Probe Flow"
"""

import json
import os
import sqlite3
import sys
import re
from datetime import date

WIKI_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "wiki"))
RAW_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "raw"))
FLOWS_DIR = os.path.join(WIKI_ROOT, "flows")


def find_db(repo_name):
    path = os.path.join(RAW_ROOT, "code", repo_name, ".codegraph", "codegraph.db")
    return path if os.path.isfile(path) else None


def get_node(conn, node_id):
    r = conn.execute("SELECT id, name, kind, file_path, start_line, end_line, signature FROM nodes WHERE id=?", (node_id,)).fetchone()
    return dict(r) if r else None


def get_callees(conn, func_id, depth=0, max_depth=3, visited=None):
    """递归获取调用链，返回层次结构."""
    if visited is None:
        visited = set()
    if func_id in visited or depth > max_depth:
        return []
    visited.add(func_id)

    callees = conn.execute(
        "SELECT DISTINCT target FROM edges WHERE kind='calls' AND source=?", (func_id,)
    ).fetchall()

    result = []
    for (cid,) in callees:
        node = get_node(conn, cid)
        if node and node["kind"] == "function":
            sub = get_callees(conn, cid, depth + 1, max_depth, visited)
            result.append({"name": node["name"], "file": node["file_path"],
                           "line": node["start_line"], "signature": node.get("signature", ""),
                           "calls": sub})
    return result


def detect_layer(fp, name, kind):
    fp_lower = fp.lower() if fp else ""
    if name in ("i3c_master_register", "i3c_master_unregister"): return "entry"
    if name.startswith("i3c_master_do_daa"): return "daa"
    if "ibi" in name.lower() or "irq" in name.lower(): return "interrupt"
    if "dma" in name.lower(): return "dma"
    if "xfers" in name.lower() or "transfer" in name.lower(): return "transfer"
    if name.startswith("i3c_device"): return "device_api"
    if name.startswith("cdns_") or name.startswith("dw_") or name.startswith("svc_"): return "controller"
    return "core"


def build_call_chain_text(call_list, indent=0):
    """将调用链格式化为可读文本."""
    lines = []
    for c in call_list:
        prefix = "  " * indent
        layer = detect_layer(c["file"], c["name"], "function")
        lines.append(f"{prefix}{c['name']}()  [{layer}]  {c['file']}:{c['line']}")
        if c.get("calls"):
            lines.extend(build_call_chain_text(c["calls"], indent + 1))
    return lines


def generate_flow_page(entry_func, repo_name, title=None):
    """生成 Flow 教学页面."""
    db_path = find_db(repo_name)
    if not db_path:
        print(f"Error: CodeGraph DB not found for '{repo_name}'")
        return False

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    # Find entry function
    row = conn.execute(
        "SELECT id, name, file_path, start_line, signature FROM nodes WHERE kind='function' AND name=?",
        (entry_func,)
    ).fetchone()
    if not row:
        print(f"Error: function '{entry_func}' not found in CodeGraph index")
        conn.close()
        return False

    func_id = row["id"]
    entry_name = row["name"]
    entry_file = row["file_path"]
    entry_line = row["start_line"]
    entry_sig = row["signature"] or ""

    # Get call chain
    call_chain = get_callees(conn, func_id, max_depth=2)
    call_lines = build_call_chain_text([{
        "name": entry_name, "file": entry_file, "line": entry_line,
        "signature": entry_sig, "calls": call_chain
    }])

    # Get all symbols in call chain for references
    all_funcs = set()
    def collect_names(cl, s):
        for c in cl:
            s.add(c["name"])
            if c.get("calls"): collect_names(c["calls"], s)
    collect_names(call_chain, all_funcs)

    # Get callees at depth 1 for table
    direct = []
    for (cid,) in conn.execute(
        "SELECT DISTINCT target FROM edges WHERE kind='calls' AND source=?", (func_id,)
    ).fetchall():
        n = get_node(conn, cid)
        if n and n["kind"] == "function":
            direct.append(n)

    # Error paths (functions with 'err' or 'fail' in name)
    error_funcs = [n["name"] for n in direct if 'err' in n["name"].lower() or 'fail' in n["name"].lower()]

    today = date.today().isoformat()
    if not title:
        title = f"{entry_name}() Flow"

    slug = re.sub(r'[^a-zA-Z0-9_]', '-', entry_name.lower()) + "-flow"
    page_path = os.path.join(FLOWS_DIR, f"{slug}.md")

    os.makedirs(FLOWS_DIR, exist_ok=True)
    with open(page_path, "w", encoding="utf-8") as f:
        f.write("---\n")
        f.write(f"date: {today}\n")
        f.write(f"tags: [code, flow, {repo_name}]\n")
        f.write(f"type: concept\n")
        f.write(f"status: active\n")
        f.write(f"source-repo: {repo_name}\n")
        f.write(f"entry-point: {entry_name}\n")
        f.write("---\n\n")
        f.write(f"# {title}\n\n")

        f.write("## What this flow does\n")
        f.write(f"TBD — Entry function: `{entry_name}()` at `{entry_file}:{entry_line}`\n\n")

        f.write("## Entry Point\n")
        f.write(f"- **Function**: `{entry_name}()`\n")
        f.write(f"- **File**: `{entry_file}:{entry_line}`\n")
        if entry_sig:
            f.write(f"- **Signature**: `{entry_sig}`\n")
        f.write("\n")

        f.write("## Call Chain\n\n")
        f.write("```\n")
        for l in call_lines:
            f.write(l + "\n")
        f.write("```\n\n")

        f.write("## Key Functions in This Flow\n\n")
        f.write("| Function | File | Line | Layer |\n")
        f.write("|----------|------|------|-------|\n")
        for n in direct:
            layer = detect_layer(n["file_path"], n["name"], "function")
            f.write(f"| `{n['name']}()` | `{n['file_path']}` | {n['start_line']} | {layer} |\n")
        f.write("\n")

        if error_funcs:
            f.write("## Error Paths\n\n")
            for name in error_funcs:
                f.write(f"- `{name}()` — error handling path\n")
            f.write("\n")

        f.write("## Register Access\n")
        f.write("(To be populated from code analysis)\n\n")

        f.write("## Related Concepts\n")
        f.write("- [[codebases/" + repo_name + "]] — " + repo_name + " codebase overview\n")
        f.write("\n")

        f.write("## Debug Checklist\n")
        f.write("- [ ] Verify entry point conditions\n")
        f.write("- [ ] Check error return paths\n")
        f.write("- [ ] Validate register configurations\n\n")

        f.write("## Counter-Arguments and Gaps\n\n")
        f.write("...\n")

    conn.close()
    print(f"  [OK] {page_path}")
    print(f"  Flow: {entry_name}() → {len(direct)} direct callees, {len(all_funcs)} total symbols")
    return True


def main(args):
    if len(args) < 2:
        print("Usage: kb flow generate <entry-func> <repo-name> [--title <title>]")
        sys.exit(1)

    entry_func = args[0]
    repo_name = args[1]
    title = None
    if "--title" in args:
        idx = args.index("--title")
        if idx + 1 < len(args):
            title = args[idx + 1]

    generate_flow_page(entry_func, repo_name, title)


if __name__ == "__main__":
    main(sys.argv[1:])
