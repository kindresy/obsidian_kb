---
tags: [tools, article-clip, web-clipper]
---

# article-clip 使用说明

将知乎、微信公众号等平台的文章一键下载到 vault 的 `raw/articles/`，供后续 `wiki ingest → wiki compile` 入库。

## 前置依赖

| 依赖 | 版本要求 | 安装方式 |
|------|---------|---------|
| Node.js | ≥ 18 | [nodejs.org](https://nodejs.org) |
| article-clip | 最新 | `npm install -g article-clip` |
| Microsoft Edge | 任意版本 | 通常系统自带 |

> 脚本启动时会自动安装 article-clip（如未安装），也可手动 `npm install -g article-clip`。

## 命令格式

```bash
./LLM_Wiki/scripts/article-clip.sh <url> [<url> ...]
```

### 参数

| 参数 | 说明 |
|------|------|
| `url` | 知乎专栏/回答、微信公众号文章等链接（可多个） |
| `--help` / `-h` | 显示帮助信息 |

### 固定配置（不可修改）

| 配置项 | 固定值 |
|--------|--------|
| 输出目录 `--out` | `LLM_Wiki/raw/articles/` |
| 浏览器 `--browser` | `edge` |

## 使用示例

### 下载单篇文章

```bash
./LLM_Wiki/scripts/article-clip.sh https://zhuanlan.zhihu.com/p/123456789
```

### 批量下载

```bash
./LLM_Wiki/scripts/article-clip.sh \
  https://zhuanlan.zhihu.com/p/123456 \
  https://zhuanlan.zhihu.com/p/789012 \
  https://mp.weixin.qq.com/s/xxxxx
```

### 查看帮助

```bash
./LLM_Wiki/scripts/article-clip.sh --help
```

## 重要注意事项

### 1. 关闭 Edge 后再运行 ⚠️

article-clip 需要读取 Edge 的 cookies 才能以你的登录态下载文章。**如果 Edge 正在运行，cookies 文件会被锁定，导致无法抓取正文内容。**

```text
❌ Edge 开启中 → 只能抓到 frontmatter，正文为空
✅ 关闭 Edge → 正常抓取全文 + 图片
```

### 2. 下载后的文件结构

```
LLM_Wiki/raw/articles/
  └── zhihu/
      └── 2026/
          └── 0610/
              └── <random-id>/
                  ├── content.md         ← 文章正文（Markdown）
                  └── images/            ← 自动下载的图片
```

### 3. 入库流程

下载完成后：

```bash
# 1. 查看下载了什么
ls LLM_Wiki/raw/articles/zhihu/

# 2. 查看 raw audit 确认状态
python3 LLM_Wiki/raw-audit.py

# 3. 纳入 wiki
wiki ingest LLM_Wiki/raw/articles/zhihu/2026/0610/xxxx/content.md

# 或使用 consolidate 批量处理
wiki consolidate
```

## 支持的平台

| 平台 | URL 示例 | 支持状态 |
|------|---------|---------|
| 知乎专栏 | `https://zhuanlan.zhihu.com/p/...` | ✅ 完整支持 |
| 知乎回答 | `https://www.zhihu.com/question/.../answer/...` | ✅ 完整支持 |
| 微信公众号 | `https://mp.weixin.qq.com/s/...` | ✅ 完整支持 |
| Twitter/X | `https://x.com/...` | ✅ 完整支持 |
| 其他 | 普通网页 | ❌ 不支持（article-clip 限制） |

## 常见问题

### Q: 下载后的 content.md 只有 frontmatter，正文空白？
**A:** Edge 没有完全关闭。关掉所有 Edge 窗口重试。

### Q: 提示 "No adapter found for URL"
**A:** article-clip 只支持特定平台（知乎、公众号、Twitter 等），不支持普通网页。
