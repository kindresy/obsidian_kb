# docs_writing — Obsidian LLM Wiki Vault

个人知识库 / 技术笔记仓库。基于 [Karpathy LLM Wiki 模式](https://github.com/karpathy/llm-wiki)，通过 Claude 管理原始资料、编译知识页面、分析代码仓库、向量检索查询。

## 目录结构

```
.
├── CLAUDE.md              ← Vault 根操作说明
├── LLM_Wiki/              ← LLM Wiki 知识库
│   ├── CLAUDE.md          ← Wiki Schema（实体类型、操作规则）
│   ├── raw/               ← 原始资料（不可变）
│   │   ├── articles/      ← 文档/文章源
│   │   ├── attachments/   ← 图片附件
│   │   └── code/          ← 代码仓库快照
│   ├── wiki/              ← LLM 生成的页面
│   │   ├── index.md       ← 索引目录
│   │   ├── concepts/      ← 概念页
│   │   ├── modules/       ← 代码模块页
│   │   ├── architecture/  ← 架构概览页
│   │   └── queries/       ← 归档问答
│   ├── build-index.py     ← 向量索引构建
│   ├── search-index.py    ← 向量索引搜索
│   ├── lint-wiki.py       ← 知识库健康检查
│   ├── .vector_index/     ← 本地向量索引（gitignored，可重建）
│   └── log.md             ← 操作日志
├── skills/                ← Claude Skills 定义
│   ├── llm-wiki/          ← Wiki 操作核心 skill
│   └── git-commit/        ← Git 提交流程 skill
└── .gitignore
```

## 前置依赖

### Python（向量检索用）

```bash
pip install sentence-transformers numpy
```

验证安装：

```bash
python3 -c "import sentence_transformers; print('OK')"
python3 -c "import numpy; print('OK')"
```

### Obsidian（可选，推荐）

打开 Obsidian → 选择"打开本地仓库" → 指向本目录。插件清单在 `.obsidian/community-plugins.json`，Obsidian 会自动检测并提示安装。

## 快速开始

### 1. 重建向量索引

```bash
python3 LLM_Wiki/build-index.py
```

### 2. 搜索测试

```bash
python3 LLM_Wiki/search-index.py "MSI interrupt handler" --top-k 3
```

### 3. 运行健康检查

```bash
python3 LLM_Wiki/lint-wiki.py LLM_Wiki/wiki/
```

### 4. 查询知识库

（通过 Claude，使用 `wiki query "<问题>"` 命令）

## 技术栈

| 组件 | 用途 | 版本 |
|------|------|------|
| Python 3 | 脚本运行 | ≥ 3.10 |
| sentence-transformers | 向量嵌入 | 5.5.1+ |
| all-MiniLM-L6-v2 | 嵌入模型 | 384 维 |
| numpy | 向量计算 | 2.x |
| Obsidian | 知识库可视化 | 可选 |

## 注意事项

- `.vector_index/` 不提交 Git，每次 clone 后需 `python3 LLM_Wiki/build-index.py` 重建
- `.claude/` 不提交 Git，各机器独立配置
- `raw/` 目录不可变，LLM 只读不写
- `wiki/` 由 LLM 全权管理
- `.obsidian/plugins/*/data.json` 不提交 Git，各机器独立

## License

Private — personal knowledge base
