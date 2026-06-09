#!/usr/bin/env python3
"""
start-dashboards.py — 通用知识图谱仪表盘启动器 (跨平台)

扫描指定目录下所有已分析的工程（/understand 或 /understand-knowledge 产出），
为每个工程自动启动一个仪表盘实例（Vite dev server），端口从 5173 递增。

用法:
  python start-dashboards.py                          # 扫描当前目录
  python start-dashboards.py --scan <目录>             # 扫描指定目录
  python start-dashboards.py <工程路径1> <工程路径2>    # 指定工程
  python start-dashboards.py --scan <目录> --port 5200 # 指定起始端口

依赖: Node.js >= 22, pnpm >= 10, understand-anything-plugin
"""

import argparse
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

# ---- 常量 ----
PLUGIN_ROOT_DEFAULT = os.path.expandvars(
    os.environ.get("UNDERSTAND_PLUGIN_ROOT",
                   os.path.join(os.path.expanduser("~"),
                                ".understand-anything/repo/understand-anything-plugin"))
)
START_PORT_DEFAULT = int(os.environ.get("START_PORT", "5173"))

# ---- 颜色 (Windows 兼容: 无 ANSI 时降级) ----
def _use_color():
    return sys.platform != "win32" or os.environ.get("TERM") == "xterm-256color"

if _use_color():
    RED = "\033[0;31m"; GREEN = "\033[0;32m"; YELLOW = "\033[1;33m"
    CYAN = "\033[0;36m"; BOLD = "\033[1m"; NC = "\033[0m"
else:
    RED = GREEN = YELLOW = CYAN = BOLD = NC = ""

def log_info(msg):  print(f"{GREEN}[INFO]{NC}  {msg}")
def log_warn(msg):  print(f"{YELLOW}[WARN]{NC}  {msg}")
def log_error(msg): print(f"{RED}[ERROR]{NC} {msg}")
def log_title(msg): print(f"\n{BOLD}{CYAN}{msg}{NC}")


# ---- 预检 ----
def preflight():
    """检查依赖是否就绪。返回 dashboard_dir 路径，失败则 sys.exit。"""
    # Node.js
    try:
        subprocess.run(["node", "--version"], capture_output=True, check=True,
                       shell=(sys.platform == "win32"))
    except (subprocess.CalledProcessError, FileNotFoundError):
        log_error("未找到 Node.js。请安装 Node.js >= 22。")
        sys.exit(1)

    # 仪表盘目录
    dashboard_dir = os.path.join(PLUGIN_ROOT_DEFAULT, "packages", "dashboard")
    if not os.path.isdir(dashboard_dir):
        log_error(f"找不到仪表盘目录: {dashboard_dir}")
        log_error("请确认 understand-anything-plugin 已安装。")
        log_error("")
        log_error("安装方法:")
        log_error(f"  git clone https://github.com/understand-anything/...")
        log_error(f"      {PLUGIN_ROOT_DEFAULT}")
        log_error(f"  cd {PLUGIN_ROOT_DEFAULT}")
        log_error("  pnpm install && pnpm --filter @understand-anything/core build")
        sys.exit(1)

    # 安装依赖
    log_info("检查仪表盘依赖...")
    try:
        subprocess.run(["pnpm", "install", "--frozen-lockfile"],
                       cwd=dashboard_dir, capture_output=True, check=False,
                       shell=(sys.platform == "win32"))
    except FileNotFoundError:
        log_error("未找到 pnpm。请安装 pnpm >= 10。")
        log_error("  npm install -g pnpm")
        sys.exit(1)

    return dashboard_dir


# ---- 发现工程 ----
def discover_projects(scan_root):
    """递归查找所有 .understand-anything/knowledge-graph.json，返回工程根目录列表。"""
    log_info(f"扫描 {scan_root} ...")
    projects = []
    root = Path(scan_root).resolve()

    if not root.exists():
        log_warn(f"目录不存在: {scan_root}")
        return projects

    for kg_file in root.rglob(".understand-anything/knowledge-graph.json"):
        # 跳过 .trash-* 临时目录
        if ".trash-" in str(kg_file):
            continue
        proj_dir = kg_file.parent.parent  # .understand-anything 的父目录
        projects.append(str(proj_dir))

    if not projects:
        log_warn(f"在 {scan_root} 下未找到任何已分析工程。")
        log_warn("请先对目标目录运行 /understand 或 /understand-knowledge。")

    return projects


# ---- 读取元信息 ----
def read_meta(kg_file):
    """从 knowledge-graph.json 读取项目名、节点/边数、kind。"""
    try:
        with open(kg_file, "r", encoding="utf-8") as f:
            g = json.load(f)
        proj = g.get("project", {})
        return {
            "name": proj.get("name", "Unknown"),
            "languages": proj.get("languages", []),
            "nodes": len(g.get("nodes", [])),
            "edges": len(g.get("edges", [])),
            "kind": g.get("kind", "code"),
            "version": g.get("version", "?"),
        }
    except Exception as e:
        return {"name": "Unknown", "error": str(e)}


# ---- 启动单个仪表盘 ----
def launch_one(proj_dir, port, dashboard_dir):
    """在后台启动一个 Vite dev server，返回 subprocess.Popen 对象。"""
    proj_abs = str(Path(proj_dir).resolve())
    kg_file = os.path.join(proj_abs, ".understand-anything", "knowledge-graph.json")

    if not os.path.isfile(kg_file):
        log_warn(f"跳过: {proj_abs} (无 knowledge-graph.json)")
        return None

    # 读取元信息
    meta = read_meta(kg_file)
    proj_name = meta.get("name", os.path.basename(proj_abs))
    nodes = meta.get("nodes", "?")
    edges = meta.get("edges", "?")
    kind = meta.get("kind", "code")

    if kind == "knowledge":
        kind_icon = "[K]"
        kind_label = "知识图谱 (力导向)"
    else:
        kind_icon = "[C]"
        kind_label = "代码图谱 (分层)"

    print(f"\n  {CYAN}{kind_icon} {BOLD}{proj_name}{NC}")
    print(f"     {kind_label} | {nodes} 节点 · {edges} 边 | 端口 {port}")
    print(f"     {proj_abs}")

    # 启动 vite
    env = os.environ.copy()
    env["GRAPH_DIR"] = proj_abs

    # Windows 上 npx 是 .cmd 文件，需要 shell=True
    use_shell = sys.platform == "win32"

    try:
        proc = subprocess.Popen(
            ["npx", "vite", "--host", "127.0.0.1", "--port", str(port)],
            cwd=dashboard_dir,
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            shell=use_shell,
        )
    except Exception as e:
        log_error(f"启动失败: {e}")
        return None

    # 非阻塞读取输出，提取关键行
    def _read_output():
        try:
            for line in iter(proc.stdout.readline, ""):
                if "Dashboard URL" in line or "ready in" in line:
                    print(f"     {line.strip()}")
                if proc.poll() is not None:
                    break
        except Exception:
            pass

    import threading
    t = threading.Thread(target=_read_output, daemon=True)
    t.start()

    return proc


# ---- 汇总报告 ----
def print_summary(processes):
    """打印已启动的仪表盘链接汇总。"""
    if not processes:
        return
    print(f"\n\n{BOLD}{'='*60}{NC}")
    print(f"  {GREEN}全部就绪。{NC} {len(processes)} 个仪表盘实例运行中。")
    print(f"")
    print(f"  查看终端输出上方的 {CYAN}🔑 Dashboard URL{NC} 行获取访问链接。")
    print(f"  按 {YELLOW}Ctrl+C{NC} 停止所有实例。")
    print(f"{BOLD}{'='*60}{NC}\n")


# ---- 主流程 ----
def main():
    parser = argparse.ArgumentParser(
        description="Understand Anything Dashboard 启动器",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s                                         扫描当前目录
  %(prog)s -s ~/Desktop/docs_writing                扫描整个 vault
  %(prog)s /path/to/pcie /path/to/wiki              指定工程路径
  %(prog)s -s ~/Desktop/docs_writing -p 5200        指定起始端口
        """,
    )
    parser.add_argument("-s", "--scan", metavar="DIR",
                        help="递归扫描指定目录下所有已分析工程")
    parser.add_argument("-p", "--port", type=int, default=START_PORT_DEFAULT,
                        help=f"起始端口号 (默认 {START_PORT_DEFAULT})")
    parser.add_argument("projects", nargs="*",
                        help="工程路径（可多个）")
    args = parser.parse_args()

    # 标题
    print(f"\n{BOLD}  Understand Anything · Dashboard Launcher  {NC}\n")

    # 预检
    dashboard_dir = preflight()

    # 收集工程列表
    projects = []

    if args.scan:
        projects.extend(discover_projects(args.scan))
    elif args.projects:
        projects = args.projects
    else:
        # 默认：扫描当前目录
        projects = discover_projects(os.getcwd())

    # 去重
    seen = set()
    unique = []
    for p in projects:
        abs_p = str(Path(p).resolve())
        if abs_p not in seen:
            seen.add(abs_p)
            unique.append(p)
    projects = unique

    if not projects:
        log_error("没有找到可启动的工程。")
        sys.exit(1)

    log_title(f"发现 {len(projects)} 个工程，准备启动仪表盘...")

    # 逐个启动
    port = args.port
    processes = []

    for proj in projects:
        proc = launch_one(proj, port, dashboard_dir)
        if proc:
            processes.append(proc)
            port += 1
            time.sleep(2)  # 错开启动避免端口竞争

    # 汇总
    print_summary(processes)

    if not processes:
        log_error("没有成功启动任何仪表盘。")
        sys.exit(1)

    # 信号处理 (非 Windows)
    def signal_handler(sig, frame):
        print(f"\n{GREEN}[INFO]{NC}  正在停止所有仪表盘...")
        for proc in processes:
            proc.terminate()
        for proc in processes:
            try:
                proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                proc.kill()
        print(f"{GREEN}[INFO]{NC}  已停止。")
        sys.exit(0)

    if sys.platform != "win32":
        signal.signal(signal.SIGINT, signal_handler)
        signal.signal(signal.SIGTERM, signal_handler)

    # 等待
    try:
        for proc in processes:
            proc.wait()
    except KeyboardInterrupt:
        signal_handler(None, None)


if __name__ == "__main__":
    main()
