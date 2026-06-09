---
date: 2026-06-09
source-type: article
title: "Understand Anything 多智能体代码分析工具"
tags: ["tools", "code-analysis"]
compiled: false
---

  ---
  一、运行原理

  understand-anything 采用 多智能体（Multi-Agent）管道架构，核心思想是：

  1. 把确定性的工作交给脚本（Node.js/Python），避免消耗 LLM token 与延时。例如文件遍历、语言检测、行统计、图合并等全通过脚本来完成。
  2. 仅在需要语义理解时才调遣（subagent）LLM（file-analyzer、architecture-analyzer、graph-reviewer 等），且支持 5 个并发文件分析子代理。
  3. 增量更新：对 git commit 未变更时跳过重分析，只更新变更文件的，并依赖 **结构指纹（fingerprint）**来精确判断文件是否真正影响图结构。
  4. 跨批次邻居图（neighborMap）：在分批分析正式，保留跨批次符号引用，确保边（edge）追踪不会断裂。
  5. 自动子域合并：支持 frontend-knowledge-graph.json、backend-knowledge-graph.json 等多份子域图合并成一个全图。

  输出产物（存储在 .understand-anything/）

  ┌──────────────────────┬───────────────────────────────────┐
  │         文件         │               含义                │
  ├──────────────────────┼───────────────────────────────────┤
  │ knowledge-graph.json │ 最终交互知识图                    │
  ├──────────────────────┼───────────────────────────────────┤
  │ meta.json            │ 上次分析时间、git commit hash     │
  ├──────────────────────┼───────────────────────────────────┤
  │ config.json          │ 语言偏好、autoUpdate 开关         │
  ├──────────────────────┼───────────────────────────────────┤
  │ .understandignore    │ 自定义忽略规则（类似 .gitignore） │
  ├──────────────────────┼───────────────────────────────────┤
  │ intermediate/        │ 阶段中间文件（分析完后自动清理）  │
  └──────────────────────┴───────────────────────────────────┘

  ---
  二、核心知识图结构

  Node 类型（13 种）

  ┌───────────────────────────────┬────────────────────────────────────────────────────────┐
  │             类型              │                          例子                          │
  ├───────────────────────────────┼────────────────────────────────────────────────────────┤
  │ file / config / document      │ file:src/main.py / config:docker-compose.yml           │
  ├───────────────────────────────┼────────────────────────────────────────────────────────┤
  │ function / class              │ function:utils.py:parse_log                            │
  ├───────────────────────────────┼────────────────────────────────────────────────────────┤
  │ module / concept              │ module:auth / concept:singleton                        │
  ├───────────────────────────────┼────────────────────────────────────────────────────────┤
  │ service / pipeline / resource │ service:Dockerfile / pipeline:.github/workflows/ci.yml │
  ├───────────────────────────────┼────────────────────────────────────────────────────────┤
  │ endpoint / table / schema     │ endpoint:routes.py:/api/v1/users                       │
  └───────────────────────────────┴────────────────────────────────────────────────────────┘

  Edge 类型（26 种）

  按语义分类：

  - 结构：imports, exports, contains, inherits, implements
  - 行为：calls, subscribes, publishes, middleware
  - 数据流：reads_from, writes_to, transforms, validates
  - 依赖：depends_on, tested_by, configures
  - 基础设施：deploys, serves, triggers, provisions
  - 语义：related, similar_to

  ---
  三、主 /understand 工作流程（7 个阶段详解）

  阶段 0 — 起飞（Pre-flight）

  - 解析参数（--full、--review，--language <lang>，--auto-update目标目录）
  - 工作树重导向：如果运行在 git worktree，自动把输出写回主 repo（防止 worktree 删除时丢失知识图）
  - 检测是否有既存 graph，比较 git commit hash，决定：
    - 全新 / --full → 跑全 7 阶段
    - 无变更 → 问用户是否重建、审查或不跑
    - 文件变更 → 增量更新（只重分析变更文件）

  核心决策表：

  ┌───────────────────────────────┬──────────────────────┐
  │             条件              │         行为         │
  ├───────────────────────────────┼──────────────────────┤
  │ --full                        │ 全重建               │
  ├───────────────────────────────┼──────────────────────┤
  │ 无图 / 无 meta                │ 全重建               │
  ├───────────────────────────────┼──────────────────────┤
  │ --review + 图在 + commit 未变 │ 跳到阶段 6（仅审查） │
  ├───────────────────────────────┼──────────────────────┤
  │ 图在 + commit 未变            │ 问用户选 a/b/c       │
  ├───────────────────────────────┼──────────────────────┤
  │ 图在 + commit 已变            │ 增量更新             │
  └───────────────────────────────┴──────────────────────┘

  阶段 0.5 — 忽略配置

  - 若不存在，自动生成 .understand-anything/.understandignore（基于 .gitignore 推断建议）
  - 用户确认后继续。

  阶段 1 — 项目扫描（SCAN）

  调用 project-scanner 子代理 + 本地 scan-project.mjs 脚本：
  - 枚举文件（优先 git ls-files，回退递归遍历）
  - 语言 & 框架检测（按扩展名映射）
  - 按文件类别分类：code、config，docs，infra，data，script，markup
  - 估算复杂度（小 / 中 / 大 / 超大）
  - 提取 import 映射

  阈值门：如果超过 100 文件建议用户用子目录限范围。

  阶段 1.5 — 分批计算（BATCH）

  执行 compute-batches.mjs：
  - 将文件按语义邻近性分组（利用 import map 与路径距）
  - 生成 batches.json，每批次包含：
    - batchFiles：本批次要分析的文件
    - batchImportData：当场外解好的 import 数据
    - neighborMap：跨批次邻居及其导出符号（用于跨批次边建立）

  阶段 2 — 文件分析（ANALYZE）

  调用 file-analyzer 子代理，最多 5 并发：
  - 对每个批次，子代理读取真实文件内容，提取 node/function/class/endpoint 等，生成 GraphNode 与 GraphEdge
  - 结果写出为 batch-<i>.json（太大则拆为 batch-<i>-part-<k>.json）
  - 全批次完成后后 → 运行 merge-batch-graphs.py 以合并：
    - 去重 node / edge
    - 规范化 ID、复杂度枚举
    - 砍斩边，添 tested_by link
    - 输出 assembled-graph.json

  增量路径差异：大纲现有 node，添加 batch-existing.json 与新 batch 一起合并。

  阶段 3 — 组装审查（ASSEMBLE REVIEW）

  调用 assemble-reviewer 子代理：
  - 审查合并后的图质量
  - 检查跨 batch edge 一致性（利用 import map 验证）
  - 写出 assemble-review.json

  阶段 4 — 架构识别（ARCHITECTURE）

  调用 architecture-analyzer 子代理：
  - 分析 assembled graph，按目录结构与 + 语言/框架规约（languages/*.md、frameworks/*.md）识别 架构层（layers）
  - 例：layer:application，layer:domain，layer:infrastructure
  - 将文件/配置/文档/服务等分入相应层
  - 输出 layers.json

  阶段 5 — 引导参观（TOUR）

  调用 tour-builder 子代理：
  - 基于层次、入口点与 README 生成 有步骤的学习参观
  - 例：
    a. 项目概览（README）
    b. 应用入口（src/main.py）
    c. 核心业务逻辑
    d. 数据层
  - 输出 tour.json

  阶段 6 — 审查（REVIEW）

  装配最终 KnowledgeGraph JSON（nodes + edges + layers + tour）。

  两条验证路径：
  - 默认：本地 Node.js 确定性验证脚本（检查必填字段、duplicate edge，dangling reference，orphan node）
  - --review：调用 graph-reviewer 子代理运行全 LLM 审查（交叉验证所有扫描文件是否对应 graph node）

  自动修复常见问题（补 tags / summary，删除斩边）。

  阶段 7 — 保存（SAVE）

  1. 写入 knowledge-graph.json
  2. 生成结构指纹基线（build-fingerprints.mjs）— 供未来增量更新比较
    - 若此步骤失败，中止并 不写 meta.json，防止后续提交误触发全更新
  3. 写入 meta.json（带 commit hash 与分析时间）
  4. 清理 tmp/ 与中间文件（保留 scan-result.json 以便下次增量直接跳过阶段 1）
  5. 向用户输出总结（文件数、node/edge 统计、层数、参观步数、警告）
  6. 若验证通过自动启动仪表盘 /understand-dashboard

  ---
  四、其他子 skill 简介

  ┌───────────────────────┬───────────────────────────────────────────────────┐
  │         Skill         │                       负责                        │
  ├───────────────────────┼───────────────────────────────────────────────────┤
  │ /understand-chat      │ 基于已生成知识图应答项目问答                      │
  ├───────────────────────┼───────────────────────────────────────────────────┤
  │ /understand-explain   │ 对某个文件/函数做深入点示                         │
  ├───────────────────────┼───────────────────────────────────────────────────┤
  │ /understand-diff      │ 分析 git diff/PR，评估波及影响与风险              │
  ├───────────────────────┼───────────────────────────────────────────────────┤
  │ /understand-domain    │ 提取业务领域知识与生成领域流程图（可单独运行）    │
  ├───────────────────────┼───────────────────────────────────────────────────┤
  │ /understand-knowledge │ 针对类似 Karpathy 风格 LLM wiki 知识库生成图      │
  ├───────────────────────┼───────────────────────────────────────────────────┤
  │ /understand-onboard   │ 为新团队成员生成上手指南                          │
  ├───────────────────────┼───────────────────────────────────────────────────┤
  │ /understand-dashboard │ 启动交互式 Web 仪表盘可视化（由主流程自动化调用） │
  └───────────────────────┴───────────────────────────────────────────────────┘

  ---
  五、关键技术细节

  - Language / Framework 注入：分析阶段从 skills/understand/languages/*.md 与 frameworks/*.md 中获得语言/框架特定模式指南（例：Python 的 import 风格，Django 的约定）
  - Locale 注入：支持 zh、ja，ko，en 等，从 locales/*.md 获取生成输出语言风格指南
  - Tree-sitter：用于结构指纹与代码结构提取（通过 extract-structure.mjs 与核心 plugin）
  - Louvain 社区检测：利用 graphology-communities-louvain（依赖）来帮助 batch 分组与架构层发现

  ---
  一句话总结：
  它是一个高度优化的、脚本化 + 多子智能体并行 代码分析管道，分阶段扫描、分批语义分析、合并审查，识别架构层并生成导游，最终输出一个标准化的 JSON 知识图；并支持 git-aware 增量更新与公式验证本地化。