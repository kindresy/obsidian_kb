---
date: 2026-06-06
tags: [tools, code-analysis, ai]
type: source-summary
source-url: raw/articles/tools/understand-anything.md
compiled: true
---

# Understand Anything — 多智能体代码分析管道

一篇介绍 **understand-anything** 工具的技术文档。该工具采用脚本化 + 多 LLM 子智能体并行的管道架构，对代码仓库进行深度分析，生成标准化 JSON 知识图。

## Key Points

- **核心思想**：确定性工作交给脚本（Node.js/Python），语义理解交给 LLM 子代理
- **输出产物**：`knowledge-graph.json`（13 种 Node、26 种 Edge）、`layers.json`（架构层）、`tour.json`（导游）
- **增量更新**：git-aware，通过结构指纹（fingerprint）判断文件是否影响图结构
- **并行能力**：支持 5 个并发 file-analyzer 子代理
- **架构识别**：自动识别 application/domain/infrastructure 等架构层
- **子技能体系**：chat / explain / diff / domain / knowledge / onboard / dashboard

## Entities Mentioned

- [[understand-anything]] — understand-anything 工具概念
- [[multi-agent-pipeline]] — 多智能体管道架构模式
- [[code-knowledge-graph]] — 代码知识图标准格式
