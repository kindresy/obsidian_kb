# /code-graph — 使用说明

## 命令格式

```
/code-graph <路径或URL> [--language <语言>]
```

## 使用场景

你有一个代码仓库想快速了解其架构，但又不想手动执行"克隆 → 分析 → 开仪表盘"三步。`/code-graph` 把这三步合并成一个命令。

| 场景 | 命令 |
|------|------|
| 分析本地项目 | `/code-graph ~/my-project` |
| 分析 GitHub 开源项目 | `/code-graph https://github.com/ggerganov/llama.cpp` |
| 分析并指定英文输出 | `/code-graph https://github.com/rust-lang/rust --language en` |

## 操作步骤

1. 在 Claude Code 中输入 `/code-graph <url>` 或 `/code-graph <本地路径>`
2. 等待 Claude 执行：
   - 如果是 URL → 自动 `git clone --depth 1` 到 `LLM_Wiki/raw/code/<repo>/`
   - 调用 `/understand` 进行 7 阶段分析（SCAN → ... → SAVE）
   - 调用 `/understand-dashboard` 启动 Web 仪表盘
3. 复制终端输出的 `🔑 Dashboard URL` 在浏览器中打开
4. 交互式浏览代码架构：调用图、层级关系、导览路径

## 实际示例

```
用户: /code-graph https://github.com/openssl/openssl

Claude:
  > 正在克隆 openssl ...
  > 正在分析 openssl (231 文件, ~800K 行) ...
  [Phase 1/7] 扫描项目文件...
  [Phase 2/7] 分析文件 — 231 文件 15 批次...
  ...
  [Phase 7/7] 保存知识图谱...
  
  Dashboard: http://127.0.0.1:5173/?token=abc123...
```

## 与其他 Skill 的关系

| Skill | 做了什么 |
|-------|---------|
| `/understand` | 仅分析，不启动仪表盘 |
| `/understand-dashboard` | 仅启动仪表盘，不分析 |
| **`/code-graph`** | **克隆(如需) → 分析 → 仪表盘，一键完成** |
| `/understand-chat` | 基于已有图谱问答 |
| `/understand-explain` | 深入解释某个文件/函数 |

## 产出文件

分析完成后，以下文件会写入目标目录的 `.understand-anything/` 子目录：

```
<repo>/.understand-anything/
├── knowledge-graph.json    # 知识图谱（108+ 节点，141+ 边）
├── meta.json               # 分析元数据（commit hash、时间戳）
├── config.json             # 配置（语言、自动更新）
└── .understandignore       # 忽略规则
```
