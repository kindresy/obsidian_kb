---
name: article-clip
description: >-
  Download articles (Zhihu, WeChat public account, Twitter/X) to the vault's
  raw/articles/ using article-clip + Edge browser. Trigger when the user says
  "帮我下载这个文章", "下载文章", "clip this article", or provides a URL
  from a supported platform.
argument-hint: <url> [<url> ...]
---

# Article Clip — 文章下载 Skill

将知乎、微信公众号、Twitter 等平台的文章一键下载到 `LLM_Wiki/raw/articles/`，后续可走 `wiki ingest → wiki compile` 入库。

## 触发词

当用户说以下任意一句话时触发：
- "帮我下载这个文章"
- "下载这篇文章"
- "帮我存一下这个链接"
- "clip this article"
- 直接提供一个知乎/公众号/Twitter 链接

## 前提条件

1. **Microsoft Edge 已安装**（系统自带）
2. **article-clip 已安装**（`npm install -g article-clip`，如未安装脚本会自动安装）
3. **Edge 浏览器已完全关闭** — 这是关键！Edge 开着时无法读取 cookies，下载结果只有空 frontmatter

## 工作流程

### Step 1: 检查 URL

判断用户提供的 URL 是否在支持列表中：

| 平台 | URL 模式 | 支持 |
|------|---------|------|
| 知乎专栏 | `zhuanlan.zhihu.com/p/...` | ✅ |
| 知乎回答 | `www.zhihu.com/question/.../answer/...` | ✅ |
| 微信公众号 | `mp.weixin.qq.com/s/...` | ✅ |
| Twitter/X | `x.com/...` 或 `twitter.com/...` | ✅ |
| 其他 | 普通网页 | ❌ 不支持，告知用户 article-clip 的限制 |

### Step 2: 下载文章

```bash
article-clip --out "LLM_Wiki/raw/articles" --browser edge <url>
```

- 如果 article-clip 未安装，先执行 `npm install -g article-clip`
- 如果有多个 URL，逐个下载

### Step 3: 告知结果

告知用户：
- 下载成功/失败
- 文件保存路径
- 下一步建议：`wiki ingest <路径>` → `wiki compile` 入库

### Step 4: 异常处理

| 情况 | 处理 |
|------|------|
| **下载成功但 content.md 只有 frontmatter 正文为空** | 提示用户关闭 Edge 浏览器后重试 |
| **提示 "No adapter found for URL"** | 告知用户该平台不受支持，建议手动复制粘贴 |
| **URL 格式不正确** | 要求用户提供完整 URL |
| **多个 URL** | 批量下载，最后汇总成功/失败数量 |
