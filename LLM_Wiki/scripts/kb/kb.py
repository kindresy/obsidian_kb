#!/usr/bin/env python3
"""
kb — Engineering Knowledge Compiler CLI

Usage:
  kb code index <repo-path>          Build CodeGraph index
  kb code compile <repo-name>        Compile codebase → wiki pages
  kb concept extract                  Extract concepts from wiki/concepts/
  kb bind propose <repo-name>        Generate candidate bindings
  kb bind review                     List pending bindings
  kb bind promote <binding-id>       Accept a binding
  kb flow generate <func-name>       Generate Flow page from call chain
  kb graph export <repo-name>        Export JSON knowledge graph
  kb graph mermaid <func-name>       Generate Mermaid call graph
  kb audit                           Audit raw/ for unprocessed files
  kb ask "<question>"                Hybrid query across wiki + code
"""
import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1]
    args = sys.argv[2:]

    commands = {
        "code":     {"index": "code_index", "compile": "compile_codebase"},
        "concept":  {"extract": "concept_extract"},
        "bind":     {"propose": "bind_concepts", "review": "bind_review", "promote": "bind_promote"},
        "flow":     {"generate": "flow_generate"},
        "graph":    {"export": "graph_export", "mermaid": "graph_mermaid"},
        "audit":    {"run": "audit"},
        "ask":      {"run": "ask"},
    }

    if cmd in commands:
        sub_cmds = commands[cmd]
        if not args:
            print(f"kb {cmd}: need sub-command: {', '.join(sub_cmds.keys())}")
            sys.exit(1)
        sub = args[0]
        if sub in sub_cmds:
            module_name = sub_cmds[sub]
            try:
                module = __import__(module_name)
                module.main(args[1:])
            except ImportError as e:
                print(f"Error: module {module_name} not found ({e})")
                print(f"Expected at: {os.path.join(SCRIPT_DIR, module_name + '.py')}")
                sys.exit(1)
        else:
            print(f"kb {cmd}: unknown sub-command '{sub}'. Options: {', '.join(sub_cmds.keys())}")
            sys.exit(1)
    elif cmd in ("-h", "--help"):
        print(__doc__)
    else:
        print(f"kb: unknown command '{cmd}'")
        print("See: kb --help")
        sys.exit(1)

if __name__ == "__main__":
    main()
