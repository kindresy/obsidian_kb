---
date: 2026-06-08
tags: [codegraph, tools, code-analysis]
type: concept
status: active
---

# CodeGraph — 代码知识图谱

CodeGraph 是 wiki 的代码分析引擎，基于 tree-sitter 将源码解析为 AST，提取符号和关系存入 SQLite，提供 MCP 工具查询和标准化 JSON 导出。

## 工作流

```bash
# 1. 建索引（一次性）
codegraph init -i raw/code/<repo>

# 2. 编译为 wiki 页面
python3 scripts/kb/kb.py code compile <repo>

# 3. 概念↔代码绑定
python3 scripts/kb/kb.py bind propose <repo>

# 4. 导出知识图谱 JSON
python3 scripts/kb/kb.py graph export <repo>

# 5. 可视化
python3 scripts/kb/kb.py graph layers <repo>
python3 scripts/kb/kb.py graph mermaid <function>
```

## kb CLI 命令参考

| 命令 | 用途 |
|------|------|
| `kb audit` | 审计 raw/ 未处理文件 |
| `kb code compile <repo>` | CodeGraph → wiki/codebases/ 页面 |
| `kb bind propose <repo>` | 概念↔代码绑定候选 |
| `kb bind review` | 查看待审核绑定 |
| `kb bind promote <id>` | 确认绑定 |
| `kb graph layers <repo>` | 打印架构层表格 |
| `kb graph mermaid <func>` | 打印 Mermaid 调用链图 |
| `kb graph export <repo>` | 导出 JSON 知识图谱 |

## 知识图谱 JSON 格式

`kb graph export <repo>` 输出到 `.kb/graph_exports/<repo>-knowledge-graph.json`。

### 结构

```json
{
  "meta": {
    "project": "pcie",
    "generated": "2026-06-08",
    "source": "CodeGraph",
    "tool": "kb graph export"
  },
  "stats": {
    "nodes": 308,
    "edges": 270,
    "layers": 10
  },
  "layers": [
    {"name": "dma", "node_count": 61, "kinds": ["class","enum","function"]},
    {"name": "core", "node_count": 97, "kinds": ["function","variable"]}
  ],
  "nodes": [
    {
      "id": "function:edge_probe",
      "kind": "function",
      "name": "edge_probe",
      "file": "edge.c",
      "lines": [5826, 5970],
      "layer": "entry",
      "language": "c"
    }
  ],
  "edges": [
    {
      "source": "function:edge_probe",
      "target": "function:edge_map_bars",
      "kind": "calls",
      "line": 5870
    }
  ]
}
```

### 字段说明

| 字段 | 类型 | 说明 |
|------|------|------|
| `meta` | object | 项目信息、生成时间、工具来源 |
| `stats` | object | 节点/边/层数量统计 |
| `layers` | array | 架构层列表（自动检测） |
| `nodes` | array | 全部符号节点（函数/结构体/枚举/文件/导入） |
| `edges` | array | 全部关系边（调用/导入/包含） |

### Node 类型

| kind | 对应 CodeGraph 类型 | 说明 |
|------|-------------------|------|
| `function` | function | 函数定义 |
| `class` | struct | 结构体定义 |
| `enum` | enum | 枚举类型 |
| `enum_member` | enum_member | 枚举成员 |
| `import` | import | 头文件导入 |
| `variable` | variable | 全局变量 |
| `file` | file | 源文件 |

### Edge 类型

| kind | 说明 |
|------|------|
| `calls` | 函数调用 |
| `imports` | 文件导入 |
| `contains` | 容器关系（已从导出中过滤） |

## 如何在 Python 中读取

```python
import json

kg = json.load(open('.kb/graph_exports/pcie-knowledge-graph.json'))

# 搜索函数
funcs = [n for n in kg['nodes'] if n['kind'] == 'function']
print(f"Total functions: {len(funcs)}")

# 查询特定函数
probe = [n for n in funcs if 'probe' in n['name'].lower()][0]
print(f"{probe['name']} at {probe['file']}:{probe['lines'][0]}")

# 统计调用关系
from collections import Counter
call_kinds = Counter(e['kind'] for e in kg['edges'])
print(f"Edge distribution: {call_kinds}")

# 列出某个函数的所有被调用者
callers_of = [e for e in kg['edges']
              if e['kind'] == 'calls' and 'edge_probe' in e['source']]
print(f"edge_probe calls {len(callers_of)} functions")
```

## 与 Understand Anything 的兼容性

此 JSON 格式与 Understand Anything 的 `knowledge-graph.json` 兼容，可用于：

- 导入 Understand Anything 仪表盘生成交互式 Web 界面
- 导入 D3.js / Cytoscape.js 自定义可视化
- 在 CI 流水线中比对代码图变化
- 作为 `wiki query` 之外的结构化查询接口

## See Also

- [[code-knowledge-graph]] — 代码知识图标准化格式
- [[multi-agent-pipeline]] — 多智能体管道架构模式
- [[pcie]] — Edge PCIe 驱动代码总览

## Counter-Arguments and Gaps

...
