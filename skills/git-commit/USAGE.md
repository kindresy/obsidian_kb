# Git Commit — 使用说明

## 概述

为 docs_writing Obsidian 库定制的安全 Git 提交流程。确保每次提交前 lint 通过、gitignore 正确、只提交有意义的文件、commit message 详细可追踪。

## 命令格式

```
git commit                    — 常规提交流程
git commit --dry-run          — 预览将要提交的内容（不实际提交）
```

## 使用场景

- 完成一次 wiki 操作（compile / walkthrough / query / lint）后保存变更
- 在多个操作后做一次汇总提交
- 需要将本地变更推送到远程仓库前

## 操作步骤

### Step 1: 查看当前状态

```bash
git status
```

查看哪些文件被修改、哪些是未跟踪的新文件。

### Step 2: 更新 .gitignore

确保以下类型的文件不会被意外提交：

```
# Kernel build artifacts
raw/code/**/*.o
raw/code/**/*.ko
raw/code/**/*.mod
raw/code/**/*.mod.c
raw/code/**/*.cmd
raw/code/**/Module.symvers
raw/code/**/modules.order

# User-specific Obsidian config
.obsidian/graph.json
.obsidian/workspace.json
.obsidian/plugins/*/data.json
```

### Step 3: 运行 lint（阻塞检查）

```bash
python3 LLM_Wiki/lint-wiki.py LLM_Wiki/wiki/
```

如果 lint 报错，必须先修复才能提交。

### Step 4: 选择性暂存文件

只暂存有意修改的文件，逐个添加：

```bash
git add LLM_Wiki/CLAUDE.md
git add LLM_Wiki/build-index.py
git add LLM_Wiki/lint-wiki.py
git add LLM_Wiki/log.md
# ... 等
```

❌ 绝不使用 `git add -A` 或 `git add .`

### Step 5: 检查大文件

检查暂存区是否有 >1MB 的文件。如果有，确认是否应该提交。

### Step 6: 提交

```bash
git commit -m "$(cat <<'EOF'
<type>(<scope>): <subject>

- <file-path>: <what changed and why>
- <file-path>: <what changed and why>
EOF
)"
```

### Step 7: 验证

```bash
git status          # 确认工作区干净
git log --oneline -3  # 确认提交记录
```

## Commit Message 规范

```
<type>(<scope>): <subject>

<body>
```

| 字段 | 规则 | 示例 |
|------|------|------|
| **type** | `feat` / `fix` / `docs` / `refactor` / `chore` / `style` / `test` | `feat` |
| **scope** | `wiki` / `walkthrough` / `query` / `lint` / `skill` / `config` / `raw` | `walkthrough` |
| **subject** | ≤72 字，祈使句，无句号 | `add code walkthrough infrastructure` |
| **body** | 每行列出改了什么文件以及原因 | `- LLM_Wiki/CLAUDE.md: add walkthrough rules` |

### 类型说明

| 类型 | 何时使用 |
|------|---------|
| `feat` | 新功能（新操作、新实体类型、新脚本） |
| `fix` | 修复 bug（lint 发现的问题、链接修复） |
| `docs` | 文档变更（README、USAGE、注释） |
| `refactor` | 重构（不改功能的重写） |
| `chore` | 杂项（gitignore、目录创建、配置变更） |
| `style` | 格式变更（缩进、重命名、frontmatter 修正） |
| `test` | 测试 |

## 安全规则

| 规则 | 说明 |
|------|------|
| ❌ 绝不 `git push --force` 到 main/master | 必须先询问用户 |
| ❌ 绝不提交 `.env` / 密码 / API key | 发现时警告 |
| ❌ 绝不 `git add -A` | 必须逐个文件暂存 |
| ❌ 绝不用 `--no-verify` / `--no-gpg-sign` | 跳过 hooks 是不安全的 |
| ✅ 提交前必做 lint | lint 失败则阻塞提交 |
| ✅ 提交前检查 gitignore | 确保构建产物不被包含 |

## 示例

### 提交新功能

```
feat(walkthrough): implement code walkthrough for PCIe driver

- LLM_Wiki/CLAUDE.md: add walkthrough rules and module/architecture entity types
- LLM_Wiki/build-index.py: extend glob to wiki/**/*.md
- LLM_Wiki/lint-wiki.py: add section checks for module and architecture pages
- skills/llm-wiki/SKILL.md: add walkthrough operation definition
- skills/llm-wiki/USAGE.md: add walkthrough examples and directory structure
```

### 提交 lint 修复

```
fix(wiki): rename section header to match schema convention

- wiki/pcie-switch-firmware-storage.md: rename "未覆盖的内容" to "Counter-Arguments and Gaps"
```

### 提交查询结果

```
feat(query): query MSI enablement across theory and code

- wiki/queries/how-to-enable-msi-in-linux.md: file cross-domain query answer
- LLM_Wiki/log.md: append query entry
```

## 注意事项

- 如果 `.obsidian/` 下的文件被意外修改，不要提交。用 `git checkout .obsidian/` 恢复。
- raw/code/ 下的构建产物（`.o`、`.ko` 等）不要提交，它们可以从源码重新构建。
- 提交前务必 `git status` 确认只包含想要提交的文件。
- 如果 lint 报告了 pre-existing 问题（与你本次修改无关），可以单独创建一个 `fix` 提交来解决。
