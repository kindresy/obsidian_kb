---
date: 2026-06-08
tags: [tools, kb, cli, code-analysis]
type: concept
status: active
---

# Engineering Knowledge Compiler — 使用指南

## 总览

系统分为四条链路：

```
┌──────────────────────────────────────────────────────────┐
│  L4: Query                                               │
│  kb ask "..."     ← 混合检索 wiki + 代码 + 绑定           │
├──────────────────────────────────────────────────────────┤
│  L3: Binding Graph                                       │
│  kb bind propose → kb bind review → kb bind promote      │
│  kb implementations push                                  │
├──────────────────────────────────────────────────────────┤
│  L2: Code Graph                                          │
│  codegraph init -i  →  kb code compile  →  kb flow gen   │
│  kb graph export    →  kb graph mermaid →  kb graph lay  │
├──────────────────────────────────────────────────────────┤
│  L1: LLM Wiki                                            │
│  wiki ingest → wiki compile → wiki query → wiki lint     │
└──────────────────────────────────────────────────────────┘
```

---

## 1. 知识库基础操作 (L1)

由 llm-wiki skill 提供：

```
wiki ingest <path|url>        摄入原始资料 (文档/PDF/网页)
wiki compile [<path>]         编译为 wiki 概念页
wiki query "<question>"       语义向量检索 + 答案归档
wiki lint                     12 项健康检查
wiki promote                  查询答案提升为概念页
```

---

## 2. 代码图 (L2)

### 代码仓库管理

```
raw/code/
├── pcie/          Edge PCIe 驱动
├── linux-i3c/     Linux I3C 子系统
└── <repo>/        后续添加的仓库
```

### CodeGraph 索引

```bash
# 在仓库目录初始化 (N 文件 → SQLite 图数据库)
codegraph init -i raw/code/<repo>
```

A 级查询工具（MCP，重启 Claude Code 后可用）：

```
codegraph_query "msi"              搜索符号
codegraph_callers "edge_probe"     查找调用者
codegraph_callees "edge_probe"     查找被调用者
codegraph_impact "edge_probe"      影响范围分析
```

### kb CLI 命令

```bash
# 代码仓库 → wiki 概览页
python3 scripts/kb/kb.py code compile <repo>
# 例如: python3 scripts/kb/kb.py code compile pcie
# 生成: wiki/codebases/<repo>.md

# 架构层表格
python3 scripts/kb/kb.py graph layers <repo>

# Mermaid 调用链图 (按层分色子图)
python3 scripts/kb/kb.py graph mermaid <function> [repo]
# 例如: python3 scripts/kb/kb.py graph mermaid edge_probe

# JSON 知识图导出 (understand-anything 兼容格式)
python3 scripts/kb/kb.py graph export <repo>
# 输出: .kb/graph_exports/<repo>-knowledge-graph.json
```

---

## 3. 概念↔代码绑定 (L3)

### 流程

```
1. 提取概念 → 2. 匹配符号 → 3. 审核 → 4. 回写
```

### 操作

```bash
# 扫描 wiki/concepts/ 提取概念名 + 代码引用
# Level 1: 精确符号匹配 (函数名/宏名)
# Level 2: 别名匹配 (alias_table.json)
python3 scripts/kb/kb.py bind propose <repo>

# 查看待审核绑定
python3 scripts/kb/kb.py bind review

# 确认绑定
python3 scripts/kb/kb.py bind promote <binding-id>

# 将已接受的绑定回写到概念页 ## Implementations
python3 scripts/kb/implementations_push.py
# 或指定单一概念: --concept i3c

# 手动添加绑定 (编辑 .kb/bindings.sqlite 或通过 kb bind propose)
```

### 别名表

路径: `.kb/alias_table.json`

```json
{
  "aliases": [
    {"canonical": "tx abort", "variants": ["tx_abrt", "TX_ABRT"]},
    {"canonical": "MMIO", "variants": ["mmio", "readl", "writel", "ioremap"]}
  ]
}
```

---

## 4. 流程页面 (L3)

### 生成规则

```bash
# 从 CodeGraph 调用链生成 Flow 教学页面
python3 scripts/kb/flow_generate.py <entry-func> <repo> [--title "标题"]
```

### 已生成的 Flow

| 页面 | 入口函数 | 覆盖范围 |
|------|---------|---------|
| `wiki/flows/edge_probe-flow.md` | `edge_probe()` | PCIe 驱动初始化：BAR 映射/DMA/中断/字符设备 |
| `wiki/flows/i3c_master_register-flow.md` | `i3c_master_register()` | I3C 总线注册：地址槽/CCC/DAA/设备枚举 |
| `wiki/flows/i3c_master_do_daa-flow.md` | `i3c_master_do_daa()` | 动态地址分配：48-bit ID 仲裁/地址分配 |

---

## 5. 混合查询 (L4)

```bash
# 同时搜索 wiki 概念页 + codegraph 代码符号 + 绑定关系
python3 scripts/kb/kb.py ask "<问题>"

# 限定仓库
python3 scripts/kb/kb.py ask "<问题>" --repo <repo>
```

示例：

```bash
python3 scripts/kb/kb.py ask "I3C 动态地址分配 DAA"
# 返回:
#   Wiki: i3c-vs-i2c.md (score: 0.59)
#   Code: renesas_i3c_daa()  (drivers/i3c/master/renesas-i3c.c)
#   Code: dw_i3c_master_daa() (drivers/i3c/master/dw-i3c-master.c)
#   Code: cdns_i3c_master_do_daa() (drivers/i3c/master/i3c-master-cdns.c)
#   13 accepted bindings
```

也可直接使用 wiki query (语义向量检索，精度更高)：

```
wiki query "I3C 和 I2C 的主要区别"
```

---

## 6. 审计与维护

```bash
# 检查 raw/ 中未被处理的文件
python3 scripts/kb/kb.py audit

# 12 项 wiki 健康检查 (死链/孤儿页/索引漂移/raw 不可变)
python3 LLM_Wiki/lint-wiki.py LLM_Wiki/wiki/

# 重建向量索引 (新增/修改页面后必须执行)
python3 LLM_Wiki/build-index.py
```

---

## 7. 端到端工作流示例

### 接入一个代码仓库

```bash
# 1. 放入代码
git clone --depth 1 <url> LLM_Wiki/raw/code/<repo>

# 2. 建 CodeGraph 索引
cd LLM_Wiki/raw/code/<repo>
codegraph init -i

# 3. 编译成 wiki codebases 页面
cd LLM_Wiki
python3 scripts/kb/kb.py code compile <repo>

# 4. 生成 Flow 页面
python3 scripts/kb/flow_generate.py <entry-func> <repo>

# 5. 概念↔代码绑定
python3 scripts/kb/kb.py bind propose <repo>
python3 scripts/kb/kb.py bind promote <id>
python3 scripts/kb/implementations_push.py

# 6. 重建索引
python3 LLM_Wiki/build-index.py
```

### 日常使用

```bash
# 查询
wiki query "<问题>"
python3 scripts/kb/kb.py ask "<问题>"

# 更新代码后重新索引 (增量更新)
cd LLM_Wiki/raw/code/<repo>
git pull
codegraph sync   # 增量更新 CodeGraph 索引
python3 scripts/kb/kb.py code compile <repo>
python3 scripts/kb/kb.py graph export <repo>
python3 LLM_Wiki/build-index.py
```

---

## 8. 目录参考

```
LLM_Wiki/
├── raw/                           ← 不可变原始材料
│   ├── articles/                  ← 文档/PDF 提取
│   └── code/                      ← 代码仓库
├── wiki/                          ← LLM 生成的页面
│   ├── concepts/                  ← 概念页
│   ├── codebases/                 ← 代码仓库概览页
│   ├── flows/                     ← 流程教学页
│   ├── queries/                   ← 归档查询
│   ├── implementations/           ← 绑定索引
│   ├── maps/                      ← 概念-代码映射
│   ├── tools/                     ← 工具文档
│   └── pending/                   ← 待审核绑定
├── scripts/kb/                    ← kb CLI 脚本
│   ├── kb.py                      ← 入口
│   ├── compile_codebase.py        ← 代码→wiki 编译
│   ├── flow_generate.py           ← Flow 页面生成
│   ├── graph_export.py            ← 图谱导出+Mermaid
│   ├── bind_concepts.py           ← 概念↔代码绑定
│   ├── implementions_push.py      ← 绑定回写概念页
│   ├── ask.py                     ← 混合查询
│   └── audit.py                   ← 审计
├── .kb/                           ← 可重建索引层
│   ├── bindings.sqlite            ← 绑定数据库
│   ├── alias_table.json           ← 别名表
│   └── graph_exports/             ← 知识图谱 JSON
├── build-index.py                 ← 向量索引构建
├── search-index.py                ← 语义搜索
└── lint-wiki.py                   ← 12 项健康检查
```

## See Also

- [[codegraph]] — CodeGraph 代码知识图谱工具参考
- [[understand-anything]] — Understand Anything 多智能体分析
- [[code-knowledge-graph]] — 代码知识图标准化格式
- [[pcie]] — Edge PCIe 驱动代码总览
- [[linux-i3c]] — Linux I3C 子系统代码总览
- [[engineering-kb-plan]] — 工程知识库搭建思路与实施步骤

## Counter-Arguments and Gaps

...
