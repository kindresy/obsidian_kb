# Understand Anything — 使用说明

## 概述

多智能体代码分析管道。分析代码仓库/知识库/文档，生成交互式知识图谱，提供探索、搜索、问答和可视化仪表盘。

## 命令格式

```
/understand [path] [options]         — 分析项目并生成知识图谱
/understand-dashboard                — 启动交互式 Web 仪表盘
/understand-chat <question>          — 基于知识图谱的代码问答
/understand-explain <file|function>  — 深入解释某个文件/函数
/understand-diff [commit-range]      — 差异/PR 影响分析
/understand-domain [path]            — 提取业务领域知识
/understand-knowledge <wiki-path>    — 分析 Karpathy 风格 LLM Wiki 知识库
/understand-onboard                  — 生成新成员上手指南
```

## 选项

| 选项 | 说明 |
|------|------|
| `--full` | 强制全量重建，忽略已有图谱 |
| `--language <lang>` | 设置语言（`zh`, `ja`, `ko`, `en` 等） |
| `--auto-update` | 启用 git commit 自动增量更新 |
| `--review` | 全 LLM 审查模式（替代确定性验证） |

## 使用场景

### 1. 分析代码仓库

```bash
# 分析当前项目
/understand

# 用中文分析
/understand --language zh

# 强制全量重建
/understand --full
```

### 2. 分析 wiki 知识库

```bash
# 分析 LLM Wiki 并生成交互式知识图
/understand-knowledge LLM_Wiki/

# 结合 wiki 的 index.md 自动提取 wikilinks 和分类
```

### 3. 查询与探索

```bash
# 交互式问答
/understand-chat MSI 中断是怎么实现的？

# 深入解释某个函数
/understand-explain edge_probe

# 可视化仪表盘
/understand-dashboard
```

### 4. 差异分析

```bash
# 分析未提交的变更影响
/understand-diff

# 分析特定 commit 范围
/understand-diff HEAD~3..HEAD
```

### 5. 上手指南

```bash
# 为新成员生成代码库学习路线
/understand-onboard
```

## 输出产物

存储在 `.understand-anything/` 目录：

| 文件 | 说明 |
|------|------|
| `knowledge-graph.json` | 最终交互式知识图 |
| `meta.json` | 上次分析时间、git commit hash |
| `config.json` | 语言偏好、autoUpdate 开关 |
| `.understandignore` | 自定义忽略规则 |
| `intermediate/` | 阶段中间文件（分析完后自动清理） |

## 知识图谱格式

与 `kb graph export` 导出的 JSON 兼容。详见 [[tools/codegraph]]。

## 本 Skill 文件

- SKILL.md: 完整技能定义（与 `.claude/skills/understand/SKILL.md` 同步）
- USAGE.md: 本使用说明

## 参考

完整项目地址：https://github.com/Lum1104/Understand-Anything
