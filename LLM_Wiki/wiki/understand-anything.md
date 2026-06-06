---
date: 2026-06-06
tags: [tools, code-analysis, ai]
type: concept
status: active
---

# Understand Anything

Understand Anything 是一个多智能体（Multi-Agent）代码分析管道工具，通过对代码仓库的深度分析生成标准化的 JSON 知识图（knowledge-graph.json）。

## Details

### 架构
该工具的核心设计原则是将**确定性工作**与**语义理解**分离：
- **脚本层**（Node.js/Python）：文件遍历、语言检测、行统计、图合并等，避免消耗 LLM token
- **LLM 子代理层**：file-analyzer（文件分析）、architecture-analyzer（架构识别）、tour-builder（导游生成）、graph-reviewer（图审查），仅在需要语义理解时调用

### 七阶段管道
1. **Pre-flight**（起飞）：参数解析、git commit 比对、决定全量/增量/跳过
2. **SCAN**（项目扫描）：枚举文件、语言检测、复杂度估算、import 映射提取
3. **BATCH**（分批计算）：按语义邻近性分组
4. **ANALYZE**（文件分析）：5 并发子代理读取文件、提取 Node/Edge
5. **ASSEMBLE REVIEW**（组装审查）：跨 batch edge 一致性校验
6. **ARCHITECTURE**（架构识别）：自动分层
7. **TOUR**（导游）：生成引导式学习参观

### 知识图格式
- **13 种 Node 类型**：file / function / class / module / concept / service / endpoint / …
- **26 种 Edge 类型**：imports / calls / contains / inherits / depends_on / tested_by / deploys / …
- 按语义分类：结构、行为、数据流、依赖、基础设施、语义

### 子技能体系
- `/understand-chat` — 基于知识图的项目问答
- `/understand-explain` — 深入解释某文件/函数
- `/understand-diff` — PR/差异影响分析
- `/understand-knowledge` — 面向 Karpathy 风格 LLM wiki 的知识图生成
- `/understand-onboard` — 新成员上手指南
- `/understand-dashboard` — Web 仪表盘可视化

### 关键技术
- **Tree-sitter**：用于结构指纹与代码结构提取
- **Louvain 社区检测**：辅助 batch 分组与架构层发现
- **增量更新**：git commit hash + 结构指纹判断变更，只重分析受影响文件

## See Also

- [[2026-06-06-understand-anything]] — 源文档摘要
- [[multi-agent-pipeline]] — 多智能体管道架构模式
- [[code-knowledge-graph]] — 代码知识图格式
- [[edge-pcie-core]] — 现有代码走读模块页

## Counter-Arguments and Gaps

...
