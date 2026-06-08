---
date: 2026-06-08
tags: [code-analysis, codegraph, architecture, tools]
type: concept
status: active
---

# CodeGraph + Understand Anything — 代码分析架构

当前 wiki 的代码走读和代码图谱功能基于 **CodeGraph** 引擎构建，并借鉴了 **Understand Anything** 的设计理念。

## 整体数据流

```
源文件 (edge.c / edge.h)
    │
    ▼
┌─────────────────────────────────────────────────┐
│              CodeGraph Engine                     │
│  (tree-sitter AST 解析 → SQLite 图数据库)          │
│  308 节点 (173 函数 + 48 结构体 + ...)             │
│  576 边 (246 调用 + 306 包含 + 24 导入)            │
└──────────────┬──────────────────────────────────┘
               │
       ┌───────┼───────────────┐
       ▼                       ▼
┌──────────────┐    ┌──────────────────────┐
│ MCP Server   │    │ codegraph-export.py   │
│ (8 个工具:    │    │ (understand-anything  │
│  query       │    │  兼容的知识图 JSON)    │
│  callers     │    │                      │
│  callees     │    │ 输出:                │
│  context     │    │ knowledge-graph.json │
│  ...)        │    │ - 308 nodes          │
│              │    │ - 270 edges          │
│              │    │ - 13 个架构层         │
└──────┬───────┘    └──────────┬───────────┘
       │                       │
       ▼                       ▼
┌──────────────────────────────────────────────┐
│           codegraph-mermaid.py                │
│  从 SQLite 读取 → 生成 Obsidian Mermaid 图     │
│                                              │
│  3 种视图:                                    │
│  ┌─── 架构总览 (--layers)                     │
│  │    13 层方块图 + 跨层调用箭头               │
│  ├─── 层内调用图 (--layer dma)                │
│  │    只看 DMA 层的内部函数调用                │
│  └─── 函数调用链 (--function edge_probe)       │
│      按层分色子图，标注每个函数的架构层          │
└──────────────────────┬───────────────────────┘
                       │
                       ▼
            Obsidian wiki 页面
   ┌──────────────────────────────────┐
   │ wiki/modules/edge-pcie-core.md    │ ← CODEGRAPH 标记
   │   ## Call Graph                   │    嵌入 Mermaid 图
   │   ```mermaid                      │
   │   subgraph entry[Entry Points]    │
   │     edge_probe                    │
   │   subgraph dma[DMA Engine]        │
   │     edge_create_udma_engines      │
   │   ```                             │
   ├──────────────────────────────────┤
   │ wiki/architecture/edge-pcie-      │
   │          driver.md                 │ ← --layers 标记
   │   ## Architecture Layers           │    嵌入架构层图
   │   ```mermaid                      │
   │   graph TD                        │
   │     entry --> dma                 │
   │     entry --> hardware            │
   │   ```                             │
   └──────────────────────────────────┘
```

## 三条使用路径

### 路径 1：交互式查询（MCP 工具）

需重启 Claude Code 会话后生效。CodeGraph MCP Server 提供 8 个工具：

```
codegraph_query "msi"           # 搜索符号
codegraph_callers "edge_probe"  # 查找调用者
codegraph_callees "edge_probe"  # 查找被调用者
codegraph_context <task>        # 构建代码上下文
codegraph_impact <symbol>       # 影响范围分析
codegraph_files                 # 索引文件结构
codegraph_node <id>             # 符号详细信息
codegraph_status                # 索引健康状态
```

用在 `wiki walkthrough` 的 Phase 1-2 中，代替手动读文件。

### 路径 2：知识图导出

```bash
# 统计信息
python3 LLM_Wiki/codegraph-export.py LLM_Wiki/raw/code/pcie/.codegraph/codegraph.db --stats

# 查看架构层
python3 LLM_Wiki/codegraph-export.py LLM_Wiki/raw/code/pcie/.codegraph/codegraph.db --layers

# 导出完整 JSON
python3 LLM_Wiki/codegraph-export.py LLM_Wiki/raw/code/pcie/.codegraph/codegraph.db -o LLM_Wiki/.vector_index/code-knowledge-graph.json
```

导出的 JSON 格式类似 understand-anything 的 `knowledge-graph.json`，包含 nodes/edges/layers/tree 四个部分。

### 路径 3：可视化嵌入

在 wiki 页面中插入 `<!-- CODEGRAPH: 函数名 -->` 标记，运行脚本自动生成 Mermaid 图：

```bash
# 支持的标记：
<!-- CODEGRAPH: edge_probe -->       # 函数调用链（按层分色子图）
<!-- CODEGRAPH: --layers -->          # 架构总览图（13 层 + 跨层调用）
<!-- CODEGRAPH: layer:dma -->         # 单层内部调用图

# 更新页面
python3 LLM_Wiki/codegraph-mermaid.py LLM_Wiki/raw/code/pcie/.codegraph/codegraph.db \
  --update-page LLM_Wiki/wiki/modules/edge-pcie-core.md
```

## 自动检测的架构层

当前的 Edge PCIe 驱动示例中，CodeGraph 自动识别了 13 个架构层：

| 层 | 节点数 | 描述 |
|-------|--------|-------------|
| **entry** | 2 | Probe/remove 入口点 |
| **interface** | 47 | IOCTL 分发、字符设备、mmap |
| **interrupt** | 20 | MSI/MSI-X/INTx、ISR 注册 |
| **dma** | 69 | uDMA 引擎、scatter-gather、P2P |
| **hardware** | 8 | 寄存器访问、PCI 配置 |
| **smbus** | 23 | SMBus 仲裁、热插拔 |
| **monitor** | 37 | 异常 FIFO、错误处理 |
| **boot** | 10 | 预操作系统 DMA 加速 |
| **config** | 5 | 模块参数、LED 控制 |
| **core** | 33 | 通用编排逻辑 |
| **data** | 36 | 结构体/枚举定义 |
| **dependency** | 16 | 内核头文件导入 |
| **source** | 2 | 源文件 |

## 与 Understand Anything 的对应关系

| Understand Anything | 我们的实现 |
|-------------------|-----------|
| `knowledge-graph.json`（13 种 node，26 种 edge） | `codegraph-export.py` 输出（8 种 node，3 种 edge） |
| 架构层自动识别（application/domain/infrastructure） | 13 层自动检测（entry/interface/dma/interrupt/...） |
| `/understand-chat`（基于图问答） | `wiki query` + 向量检索（可查代码页） |
| `/understand-explain`（深入解释某函数） | `codegraph query` + `codegraph callers/callees` |
| `/understand-diff`（PR 影响分析） | `codegraph impact` 工具 |
| `/understand-dashboard`（Web 仪表盘） | Obsidian Mermaid + Graph View |

**核心区别**：Understand Anything 产出 JSON 图 + Web 仪表盘；我们产出 **Markdown wiki 页面**，把代码知识持久化为可交叉引用的文档，与已有的理论概念页通过 wikilinks 连接。

## 工具脚本索引

| 脚本 | 用途 |
|------|------|
| `codegraph-export.py` | CodeGraph SQLite → 标准化知识图 JSON |
| `codegraph-mermaid.py` | CodeGraph → Obsidian Mermaid 可视化 |
| `raw-audit.py` | 检查 raw/ 中未处理的文件 |
| `build-index.py` | 向量索引构建（跨 wiki 页面 + 代码页） |
| `lint-wiki.py` | 12 项 wiki 健康检查 |

## See Also

- [[understand-anything]] — Understand Anything 工具概念
- [[code-knowledge-graph]] — 代码知识图标准化格式
- [[multi-agent-pipeline]] — 多智能体管道架构模式
- [[edge-pcie-core]] — 实际走读产出的模块页
- [[edge-pcie-driver]] — 实际走读产出的架构页

## Counter-Arguments and Gaps

...
