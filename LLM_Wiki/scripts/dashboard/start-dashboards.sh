#!/bin/bash
#===============================================================================
# start-dashboards.sh — 通用知识图谱仪表盘启动器
#===============================================================================
# 扫描指定目录下所有已分析的工程（/understand 或 /understand-knowledge 产出），
# 为每个工程自动启动一个仪表盘实例（Vite dev server），端口从 5173 递增。
#
# 依赖: Node.js ≥ 22, pnpm ≥ 10, understand-anything-plugin
# 平台: Linux / macOS / Windows (Git Bash / WSL)
#===============================================================================

set -euo pipefail

# ---- 常量 ----
DASHBOARD_DIR="${UNDERSTAND_PLUGIN_ROOT:-$HOME/.understand-anything/repo/understand-anything-plugin}/packages/dashboard"
START_PORT="${START_PORT:-5173}"

# ---- 颜色输出 ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

log_info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }
log_title() { echo -e "\n${BOLD}${CYAN}$*${NC}"; }

# ---- 预检 ----
preflight() {
    # 检查 Node.js
    if ! command -v node &>/dev/null; then
        log_error "未找到 Node.js。请安装 Node.js ≥ 22。"
        exit 1
    fi

    # 检查仪表盘目录
    if [ ! -d "$DASHBOARD_DIR" ]; then
        log_error "找不到仪表盘目录: $DASHBOARD_DIR"
        log_error "请确认 understand-anything-plugin 已正确安装。"
        log_error ""
        log_error "安装方法（如未安装）："
        log_error "  git clone https://github.com/understand-anything/understand-anything-plugin.git \\"
        log_error "      \$HOME/.understand-anything/repo/understand-anything-plugin"
        log_error "  cd \$HOME/.understand-anything/repo/understand-anything-plugin"
        log_error "  pnpm install && pnpm --filter @understand-anything/core build"
        exit 1
    fi

    cd "$DASHBOARD_DIR"

    # 安装依赖（静默）
    log_info "检查仪表盘依赖..."
    pnpm install --frozen-lockfile 2>/dev/null || pnpm install 2>/dev/null || {
        log_warn "依赖安装有警告，尝试继续..."
    }
}

# ---- 发现工程 ----
# 递归查找所有 .understand-anything/knowledge-graph.json
discover_projects() {
    local scan_root="$1"
    log_info "扫描 $scan_root ..."

    local found=()
    while IFS= read -r -d '' kg_file; do
        # 向上两级：.understand-anything/knowledge-graph.json → 工程根目录
        local proj_dir
        proj_dir="$(dirname "$(dirname "$kg_file")")"
        found+=("$proj_dir")
    done < <(find "$scan_root" -name "knowledge-graph.json" \
        -path "*/.understand-anything/*" \
        -not -path "*/.trash-*/*" \
        -print0 2>/dev/null || true)

    if [ ${#found[@]} -eq 0 ]; then
        log_warn "未在 $scan_root 下找到任何已分析工程。"
        log_warn "请先对目标目录运行 /understand 或 /understand-knowledge。"
        return 1
    fi

    printf '%s\n' "${found[@]}"
}

# ---- 读取图谱元信息 ----
read_meta() {
    local kg_file="$1"
    python3 -c "
import json, sys
try:
    with open('$kg_file', encoding='utf-8') as f:
        g = json.load(f)
    proj = g.get('project', {})
    print(json.dumps({
        'name': proj.get('name', 'Unknown'),
        'languages': proj.get('languages', []),
        'nodes': len(g.get('nodes', [])),
        'edges': len(g.get('edges', [])),
        'kind': g.get('kind', 'code'),
        'version': g.get('version', '?')
    }, ensure_ascii=False))
except Exception as e:
    print(json.dumps({'name': 'Unknown', 'error': str(e)}))
" 2>/dev/null || echo '{"name":"Unknown","error":"cannot read"}'
}

# ---- 启动单个仪表盘 ----
launch_one() {
    local proj_dir="$1"
    local port="$2"

    # 解析绝对路径
    local proj_abs
    proj_abs="$(cd "$proj_dir" 2>/dev/null && pwd || echo "")"
    if [ -z "$proj_abs" ]; then
        log_warn "跳过无效路径: $proj_dir"
        return 1
    fi

    local kg_file="$proj_abs/.understand-anything/knowledge-graph.json"
    if [ ! -f "$kg_file" ]; then
        log_warn "跳过: $proj_abs (无 knowledge-graph.json)"
        return 1
    fi

    # 读取元信息
    local meta
    meta="$(read_meta "$kg_file")"
    local proj_name nodes edges kind
    proj_name="$(echo "$meta" | python3 -c "import json,sys; print(json.load(sys.stdin).get('name','Unknown'))" 2>/dev/null || basename "$proj_abs")"
    nodes="$(echo "$meta" | python3 -c "import json,sys; print(json.load(sys.stdin).get('nodes','?'))" 2>/dev/null || echo "?")"
    edges="$(echo "$meta" | python3 -c "import json,sys; print(json.load(sys.stdin).get('edges','?'))" 2>/dev/null || echo "?")"
    kind="$(echo "$meta" | python3 -c "import json,sys; print(json.load(sys.stdin).get('kind','code'))" 2>/dev/null || echo "code")"

    local kind_label kind_icon
    if [ "$kind" = "knowledge" ]; then
        kind_label="知识图谱 (力导向)"
        kind_icon="🧠"
    else
        kind_label="代码图谱 (分层)"
        kind_icon="💻"
    fi

    echo ""
    echo -e "  ${CYAN}${kind_icon}  ${BOLD}${proj_name}${NC}"
    echo -e "     ${kind_label} | ${nodes} 节点 · ${edges} 边 | 端口 ${port}"
    echo -e "     ${proj_abs}"

    GRAPH_DIR="$proj_abs" npx vite --host 127.0.0.1 --port "$port" 2>&1 | while IFS= read -r line; do
        # 只打印关键行（Dashboard URL），其余静默
        if [[ "$line" == *"Dashboard URL"* ]] || [[ "$line" == *"ready in"* ]]; then
            echo "     $line" | sed 's/^[[:space:]]*//'
        fi
    done &

    echo "$!"  # 返回 PID
}

# ---- 主流程 ----
main() {
    local projects=()
    local scan_root=""
    local port="$START_PORT"
    local interactive=true

    # ---- 参数解析 ----
    while [ $# -gt 0 ]; do
        case "$1" in
            -s|--scan)
                scan_root="$2"
                shift 2
                ;;
            -p|--port)
                port="$2"
                shift 2
                ;;
            -q|--quiet)
                interactive=false
                shift
                ;;
            -h|--help)
                echo "用法: $0 [选项] [工程路径...]"
                echo ""
                echo "选项:"
                echo "  -s, --scan <目录>    递归扫描目录下所有已分析工程"
                echo "  -p, --port <端口>    起始端口号（默认 5173）"
                echo "  -q, --quiet          非交互模式"
                echo "  -h, --help           显示帮助"
                echo ""
                echo "示例:"
                echo "  $0                                      # 扫描当前目录"
                echo "  $0 -s ~/Desktop/docs_writing             # 扫描整个 vault"
                echo "  $0 /path/to/pcie /path/to/wiki           # 指定工程"
                echo "  $0 -s ~/Desktop/docs_writing -p 5200    # 指定起始端口"
                exit 0
                ;;
            -*)
                log_error "未知选项: $1"
                echo "使用 -h 查看帮助"
                exit 1
                ;;
            *)
                projects+=("$1")
                shift
                ;;
        esac
    done

    # 标题
    echo ""
    echo -e "${BOLD}╔══════════════════════════════════════╗${NC}"
    echo -e "${BOLD}║   Understand Anything · Dashboard   ║${NC}"
    echo -e "${BOLD}╚══════════════════════════════════════╝${NC}"

    # 预检
    preflight

    # 扫描或使用指定路径
    if [ -n "$scan_root" ]; then
        mapfile -t projects < <(discover_projects "$scan_root" || true)
    elif [ ${#projects[@]} -eq 0 ]; then
        # 默认：扫描当前目录
        mapfile -t projects < <(discover_projects "$(pwd)" || true)
    fi

    if [ ${#projects[@]} -eq 0 ]; then
        log_error "没有找到可启动的工程。"
        exit 1
    fi

    # 去重（按绝对路径）
    local -A seen
    local -a unique_projects=()
    for p in "${projects[@]}"; do
        local abs
        abs="$(cd "$p" 2>/dev/null && pwd || echo "$p")"
        if [ -z "${seen[$abs]:-}" ]; then
            seen[$abs]=1
            unique_projects+=("$p")
        fi
    done
    projects=("${unique_projects[@]}")

    log_title "发现 ${#projects[@]} 个工程，准备启动仪表盘..."

    # 逐个启动
    local pids=()
    for proj in "${projects[@]}"; do
        local pid
        pid="$(launch_one "$proj" "$port")"
        if [ -n "$pid" ]; then
            pids+=("$pid")
            port=$((port + 1))
            sleep 2  # 错开启动避免端口竞争
        fi
    done

    # 汇总
    echo ""
    echo -e "${BOLD}============================================${NC}"
    echo -e "  ${GREEN}全部就绪。${NC} ${#pids[@]} 个仪表盘实例运行中。"
    echo ""
    echo -e "  查看终端输出上方的 ${CYAN}🔑 Dashboard URL${NC} 行获取访问链接。"
    echo -e "  按 ${YELLOW}Ctrl+C${NC} 停止所有实例。"
    echo -e "${BOLD}============================================${NC}"
    echo ""

    # 捕获 Ctrl+C 优雅退出
    cleanup() {
        echo ""
        log_info "正在停止所有仪表盘..."
        for pid in "${pids[@]}"; do
            kill "$pid" 2>/dev/null || true
        done
        log_info "已停止。"
        exit 0
    }
    trap cleanup INT TERM

    # 等待所有后台进程
    wait
}

main "$@"
