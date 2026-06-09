---
name: code-graph
description: Analyze a code repository (local path or remote git URL) and launch the interactive knowledge graph dashboard — all in one command.
argument-hint: ["<path-or-url> [--language <lang>]"]
---

# /code-graph

一键分析代码仓库（本地路径或远程 git URL），生成交互式知识图谱并自动打开仪表盘。

## 参数

- `<path-or-url>` — **必填**。本地代码目录路径，或远程 git 仓库 URL（GitHub/GitLab/其他 git 服务）。
- `--language <lang>` — 可选。输出语言（`zh`, `en`, `ja`, `ko` 等）。默认 `zh`（中文）。

## 执行流程

### Phase 0 — 获取代码

**如果是远程 URL：**

1. 从 URL 中提取仓库名（例 `https://github.com/user/repo.git` → `repo`）
2. 处理 URL 格式：
   - `https://github.com/user/repo` → 自动追加 `.git`
   - `git@github.com:user/repo.git` → 直接使用
   - `https://github.com/user/repo/tree/main/sub` → 仅克隆仓库根，不处理子目录（提示用户手动克隆后指定本地路径）
3. Clone 到 `LLM_Wiki/raw/code/<repo-name>/`：
   ```bash
   git clone --depth 1 <url> "$WIKI_ROOT/raw/code/<repo-name>/"
   ```
   `$WIKI_ROOT` 为 `LLM_Wiki/` 目录（通过检查当前目录或 `LLM_Wiki/CLAUDE.md` 定位）。
4. 克隆完成后，**询问用户**是否需要保留 clone（默认保留）。不询问则保留。
5. 设置 `$TARGET_PATH = LLM_Wiki/raw/code/<repo-name>/`

**如果是本地路径：**

1. 解析为绝对路径：
   - 相对路径 → 相对于当前工作目录解析
   - 绝对路径 → 直接使用
2. 验证目录存在且包含代码文件（至少有一个源文件）
3. 设置 `$TARGET_PATH = <解析后的绝对路径>`

### Phase 1 — 分析代码

报告：`> 正在分析 <repo-name> ...`

调用 `/understand` skill，参数为 `$TARGET_PATH`：

```
/understand $TARGET_PATH --language zh
```

> `/understand` 会执行完整的 7 阶段管道：
> SCAN → BATCH → ANALYZE → ASSEMBLE REVIEW → ARCHITECTURE → TOUR → SAVE
>
> 产出 `$TARGET_PATH/.understand-anything/knowledge-graph.json`

分析完成后，向用户报告关键统计：节点数、边数、层级数、导览步骤数。

### Phase 2 — 启动仪表盘

报告：`> 正在启动仪表盘...`

调用 `/understand-dashboard` skill，参数为 `$TARGET_PATH`：

```
/understand-dashboard $TARGET_PATH
```

仪表盘会自动启动 Vite dev server 并输出访问链接（含 `?token=` 参数）。

## 完整示例

```bash
# 本地项目
/code-graph /path/to/my-project

# GitHub 仓库
/code-graph https://github.com/torvalds/linux

# GitLab 仓库
/code-graph git@gitlab.com:user/project.git

# 指定语言
/code-graph https://github.com/rust-lang/rust --language en
```

## 错误处理

| 场景 | 处理方式 |
|------|---------|
| URL 无法 clone | 报告错误，提示检查 URL 和网络；如果是私有仓库，提示先用 `git clone` 手动克隆后指定本地路径 |
| 本地路径不存在 | 报告错误，停止执行 |
| 目录已存在（重复 clone） | 检测到 `raw/code/<name>/` 已存在，询问用户：(a) `git pull` 更新 (b) 直接分析现有代码 (c) 删除重新 clone |
| `/understand` 分析失败 | 报告失败原因，保留已生成的中间产物（`scan-result.json` 等），建议用户手动排查 |
| 仪表盘端口被占用 | 仪表盘自动选择下一个可用端口（Vite 默认行为） |

## 注意事项

- 远程 URL 克隆使用 `--depth 1`（浅克隆），节省带宽和磁盘空间
- 克隆位置固定为 `LLM_Wiki/raw/code/<repo-name>/`，遵循 LLM Wiki 的 `raw/code/` 不可变约定
- 分析产物写入 `<repo>/.understand-anything/knowledge-graph.json`，不会被 git 追踪（已在 `.gitignore` 中）
- 如果 `LLM_Wiki/raw/code/` 下有同名的旧 clone，会提示用户选择如何处理
