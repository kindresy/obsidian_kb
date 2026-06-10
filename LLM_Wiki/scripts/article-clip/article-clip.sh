#!/usr/bin/env bash
#===============================================================================
# article-clip wrapper — 知乎/微信公众号/网页文章下载器
#
# 固定配置:
#   --out       → LLM_Wiki/raw/articles/
#   --browser   → edge
#
# 用法:
#   ./article-clip.sh <url> [<url> ...]
#   ./article-clip.sh --help
#
# 依赖:
#   - Node.js ≥ 18 (npm install -g article-clip)
#   - Microsoft Edge (用于读取登录态)
#   - Edge 需完全关闭后才能读取 cookies
#===============================================================================

set -euo pipefail

# ---- 路径 ----
VAULT_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OUT_DIR="$VAULT_ROOT/LLM_Wiki/raw/articles"

# ---- 颜色 ----
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

log_info()  { echo -e "${GREEN}[INFO]${NC}  $*"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
log_error() { echo -e "${RED}[ERROR]${NC} $*"; }

# ---- help ----
if [ $# -eq 0 ] || [ "$1" = "--help" ] || [ "$1" = "-h" ]; then
    echo ""
    echo -e "${BOLD}article-clip wrapper${NC} — 下载知乎/微信公众号等平台文章到 vault"
    echo ""
    echo "用法:"
    echo "  $0 <url> [<url> ...]"
    echo ""
    echo "示例:"
    echo "  $0 https://zhuanlan.zhihu.com/p/123456"
    echo "  $0 https://zhuanlan.zhihu.com/p/123456 https://mp.weixin.qq.com/s/xxx"
    echo ""
    echo "说明:"
    echo "  - 输出目录固定为 LLM_Wiki/raw/articles/"
    echo "  - 使用 Edge 浏览器读取登录态"
    echo "  - 下载前请关闭 Edge 浏览器以确保能读取 cookies"
    echo "  - 文章保存后可通过 wiki ingest → wiki compile 纳入知识库"
    echo ""
    exit 0
fi

# ---- 预检 ----
if ! command -v article-clip &>/dev/null; then
    log_warn "article-clip 未安装，正在安装..."
    npm install -g article-clip || {
        log_error "安装失败，请手动执行: npm install -g article-clip"
        exit 1
    }
    log_info "安装完成"
fi

if ! command -v node &>/dev/null; then
    log_error "未找到 Node.js，请安装 Node.js ≥ 18"
    exit 1
fi

# 确保输出目录存在
mkdir -p "$OUT_DIR"

# ---- 逐个下载 ----
SUCCESS=0
FAIL=0

for URL in "$@"; do
    echo ""
    log_info "正在下载: $URL"

    if article-clip --out "$OUT_DIR" --browser edge "$URL"; then
        echo -e "  ${GREEN}✓${NC} 成功"
        SUCCESS=$((SUCCESS + 1))
    else
        echo -e "  ${RED}✗${NC} 失败"
        FAIL=$((FAIL + 1))
    fi
done

# ---- 汇总 ----
echo ""
echo -e "${BOLD}============================================${NC}"
echo -e "  下载完成: $SUCCESS 成功, $FAIL 失败"
echo -e "  保存位置: ${CYAN}$OUT_DIR${NC}"
echo -e "${BOLD}============================================${NC}"
echo ""
echo "  下一步: wiki ingest → wiki compile 入库"
echo ""
