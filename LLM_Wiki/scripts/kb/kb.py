#!/usr/bin/env python3
"""
kb — Engineering Knowledge Compiler CLI

Usage: kb <command> [sub-command] [args]

Commands:
  code index <repo>          Build CodeGraph index
  code compile <repo>        Compile codebase → wiki pages
  concept extract             Extract concepts from wiki/concepts/
  bind propose <repo>        Generate candidate bindings
  bind review                List pending bindings
  bind promote <id>          Accept a binding
  flow generate <func>       Generate Flow page
  graph export <repo>        Export JSON knowledge graph
  graph mermaid <func>       Generate Mermaid call graph
  graph layers <repo>        Show architecture layers
  audit                      Audit raw/ for unprocessed files
"""
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

# (command, sub-command) → module_name
ROUTES = {
    ("code", "index"):    "code_index",
    ("code", "compile"):  "compile_codebase",
    ("concept", "extract"): "concept_extract",
    ("bind", "propose"):  "bind_concepts",
    ("bind", "review"):   "bind_concepts",
    ("bind", "promote"):  "bind_concepts",
    ("flow", "generate"): "flow_generate",
    ("graph", "export"):  "graph_export",
    ("graph", "mermaid"): "graph_export",
    ("graph", "layers"):  "graph_export",
    ("audit", None):      "audit",     # no sub-command needed
}

def main():
    if len(sys.argv) < 2 or sys.argv[1] in ("-h", "--help"):
        print(__doc__.strip())
        sys.exit(0)

    cmd = sys.argv[1]
    sub = sys.argv[2] if len(sys.argv) > 2 else None
    # Pass args including sub-command; modules handle their own parsing
    args = sys.argv[2:] if sub else sys.argv[1:]

    # Look up route
    key = (cmd, sub)
    if key not in ROUTES:
        # Try with no sub-command
        key = (cmd, None)
        if key not in ROUTES:
            print(f"kb: unknown command '{cmd}'", file=sys.stderr)
            if sub:
                print(f"     sub-command '{sub}' not recognized", file=sys.stderr)
            sys.exit(1)
        args = []

    module_name = ROUTES[key]
    try:
        module = __import__(module_name)
        module.main(args)
    except ImportError as e:
        print(f"Error: module {module_name} not found ({e})", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
