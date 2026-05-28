# LLM Wiki — 使用说明

## 概述

基于 Karpathy LLM Wiki 模式的持久化知识库。在 Obsidian 中通过 Claude 管理原始资料、编译知识页面、查询和检查。

## 命令格式

```
wiki init <topic>                    — 创建新 wiki
wiki ingest <path|url>              — 摄入原始资料（文件或 URL）
wiki compile [<path>]               — 编译为结构化 wiki 页面（自动重建向量索引）
wiki query "<question>"             — 查询知识库（使用向量检索定位相关页面）
wiki lint                           — 健康检查（死链/孤立页/矛盾）
```

## 向量检索（新）

从 v2 开始，wiki query 使用 **语义向量检索** (embedding + cosine similarity) 替代纯 LLM 语义猜测：

- **Embedding 模型**: `all-MiniLM-L6-v2` (384 维, ~80MB)
- **索引位置**: `LLM_Wiki/.vector_index/`
- **重建时机**: 每次 `wiki compile` 和 `wiki promote` 后自动重建
- **查询流程**: embed 问题 → top-5 chunks → 读取对应页面 → 合成回答

### 手动操作

```bash
# 重建索引（compile/promote 后自动执行，一般不需要手动）
python3 LLM_Wiki/build-index.py

# 搜索测试
python3 LLM_Wiki/search-index.py "搜索关键词" --top-k 5
```

## 目录结构

```
LLM_Wiki/
├── raw/articles/          ← 原始资料（不可变，LLM 只读）
├── raw/attachments/       ← 图片等附件（按来源分类存放）
├── wiki/                  ← LLM 生成的页面（可读写）
│   ├── index.md           ← 索引目录
│   ├── queries/           ← 归档的问答
│   ├── concept pages      ← 概念页、人物页等
├── .vector_index/         ← 向量索引（由 build-index.py 生成）
├── outputs/reports/       ← lint 报告
├── build-index.py         ← 索引构建脚本
├── search-index.py        ← 索引搜索脚本
├── CLAUDE.md              ← wiki schema
└── log.md                 ← 操作日志（append-only）
```

## 工作流程

```
新资料 → ingest → compile → query → lint → promote
          ↓         ↓         ↓       ↓       ↓
        raw/      wiki/    queries/ 报告    wiki/concept
                    ↓
             向量索引重建
```

## 注意事项

- `raw/` 目录永不修改，LLM 只读
- compile 后必须执行 backlink audit（自动）+ 向量索引重建（自动）
- query 使用向量检索定位页面，不再需要逐页阅读 index
- query 答案自动归档到 `wiki/queries/`
- 图片使用 `![[source-name/filename]]` 格式

## 示例

```
wiki ingest raw/articles/pci_express/01-pci-basics_p1.md
wiki compile
wiki query "PCIe 链路训练的速率协商流程"
wiki lint
```

## 参考

完整 Skill 定义：[[skills/llm-wiki/SKILL.md]]
