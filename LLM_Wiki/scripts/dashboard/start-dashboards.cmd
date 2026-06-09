@echo off
REM start-dashboards — 通用知识图谱仪表盘启动器
REM 用法: start-dashboards [--scan <目录>] [--port <端口>] [工程路径...]
python3 "%~dp0start-dashboards.py" %*
