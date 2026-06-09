# Dashboard 仪表盘脚本

Understand Anything 知识图谱仪表盘的启动与管理工具。

## 背景

`/understand` 和 `/understand-knowledge` 分析完成后，将知识图谱写入 `<project>/.understand-anything/knowledge-graph.json`。这些 JSON 文件可通过 Web 仪表盘实现交互式可视化：

- **代码图谱** (`kind: "code"`) — dagre 分层布局，展示代码的调用层级、模块依赖、架构分层
- **知识图谱** (`kind: "knowledge"`) — 力导向布局，展示 Wiki 文章、概念、实体之间的关联

仪表盘本身是 Understand Anything 插件的一部分（`packages/dashboard/`，Vite + React 应用），通过环境变量 `GRAPH_DIR` 指定要可视化的工程根目录。

## 文件说明

```
scripts/dashboard/
├── start-dashboards.cmd  # Windows 入口（双击或 cmd 运行）
├── start-dashboards.py   # 核心逻辑（跨平台 Python 实现）
├── start-dashboards.sh   # Linux/macOS/Git Bash 入口
└── README.md             # 本文档
```

## 快速开始

**Windows (cmd / PowerShell):**

```cmd
cd C:\Users\admin\Desktop\docs_writing\LLM_Wiki\scripts\dashboard

:: 一键恢复所有仪表盘
start-dashboards --scan C:\Users\admin\Desktop\docs_writing

:: 指定单个工程
start-dashboards C:\Users\admin\Desktop\docs_writing\LLM_Wiki\raw\code\pcie

:: 查看帮助
start-dashboards --help
```

**Linux / macOS / Git Bash:**

```bash
cd ~/Desktop/docs_writing/LLM_Wiki/scripts/dashboard
./start-dashboards.sh --scan ~/Desktop/docs_writing
```

### 运行过程

脚本启动后依次执行：

1. **预检** — 确认 Node.js、pnpm、仪表盘代码就绪
2. **扫描** — 递归查找所有 `.understand-anything/knowledge-graph.json`
3. **识别** — 读取每个工程的项目名、图谱类型、节点/边数
4. **启动** — 每个工程一个 Vite 实例，端口自动递增（5173 → 5174 → ...）
5. **监控** — 实时显示 `🔑 Dashboard URL` 行，Ctrl+C 统一停止

终端输出示例：

```
  [K] LLM Wiki
     知识图谱 (力导向) | 333 节点 · 256 边 | 端口 5173
     🔑 Dashboard URL: http://127.0.0.1:5173/?token=abc123...

  [C] Edge PCIe Driver
     代码图谱 (分层) | 108 节点 · 141 边 | 端口 5174
     🔑 Dashboard URL: http://127.0.0.1:5174/?token=def456...
```

## 依赖

| 依赖 | 最低版本 | 说明 |
|------|---------|------|
| Node.js | ≥ 22 | JavaScript 运行时 |
| pnpm | ≥ 10 | 包管理器 |
| understand-anything-plugin | — | 仪表盘源代码（默认路径 `~/.understand-anything/repo/understand-anything-plugin`） |

安装：

```bash
# pnpm
npm install -g pnpm

# understand-anything-plugin（如未安装）
git clone https://github.com/understand-anything/understand-anything-plugin.git \
    ~/.understand-anything/repo/understand-anything-plugin
cd ~/.understand-anything/repo/understand-anything-plugin
pnpm install
pnpm --filter @understand-anything/core build
```

### 选项（Windows `.cmd` / Linux `.sh` / `.py` 通用）

| 选项 | 说明 |
|------|------|
| `-s, --scan <目录>` | 递归扫描目录下的所有 `.understand-anything/knowledge-graph.json` |
| `-p, --port <端口>` | 起始端口号（自动递增，默认 5173） |
| `-q, --quiet` | 非交互模式，减少输出 |
| `-h, --help` | 显示帮助信息 |

### 场景一：日常使用（重启电脑后恢复）

**Windows cmd:**
```cmd
cd C:\Users\admin\Desktop\docs_writing\LLM_Wiki\scripts\dashboard
start-dashboards --scan C:\Users\admin\Desktop\docs_writing
```

**Git Bash / Linux:**
```bash
cd ~/Desktop/docs_writing/LLM_Wiki/scripts/dashboard
./start-dashboards.sh --scan ~/Desktop/docs_writing
```

一键恢复所有仪表盘。

### 场景二：分析新工程后立即查看

```cmd
:: Claude Code 中运行 /understand <path>
:: 然后终端中：
cd C:\Users\admin\Desktop\docs_writing\LLM_Wiki\scripts\dashboard
start-dashboards C:\path\to\new-project
```

### 场景三：多工程对比

```bash
./start-dashboards.sh \
    ~/Desktop/docs_writing/LLM_Wiki/raw/code/pcie \
    ~/Desktop/docs_writing/LLM_Wiki/raw/code/another-repo
```

两个代码图谱并行查看，方便对比架构。

## 故障排除

| 症状 | 可能原因 | 解决方法 |
|------|---------|---------|
| `[ERROR] 未找到 Node.js` | Node.js 未安装或不在 PATH | 安装 Node.js ≥ 22，确保 `node --version` 可执行 |
| `[ERROR] 找不到仪表盘目录` | plugin 未安装或路径不同 | 检查 `~/.understand-anything/repo/` 或设置 `UNDERSTAND_PLUGIN_ROOT` 环境变量 |
| 依赖安装失败 | 网络问题或 pnpm 版本低 | 执行 `pnpm --version` 确认 ≥ 10，重试安装 |
| 端口被占用 | 其他进程占用了 5173 | 用 `--port 5200` 指定其他起始端口 |
| `未找到任何已分析工程` | 未运行过 /understand | 在 Claude Code 中对工程目录执行 `/understand` |
| 仪表盘空白 | token 过期或不匹配 | 检查终端输出中的完整 `?token=...` URL |

## 环境变量

| 变量 | 默认值 | 说明 |
|------|--------|------|
| `UNDERSTAND_PLUGIN_ROOT` | `~/.understand-anything/repo/understand-anything-plugin` | 插件安装根目录 |
| `START_PORT` | `5173` | 起始端口号（命令行 `--port` 覆盖此值） |
