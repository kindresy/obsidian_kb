# Article Clip — 使用说明

## 概述

将知乎、微信公众号、Twitter 等平台的文章一键下载到 vault 的 `raw/articles/`，供后续 wiki 入库。

## 触发方式

说以下任意一句即可触发：

```
帮我下载这个文章
下载这篇文章 https://zhuanlan.zhihu.com/p/xxxxx
帮我存一下这个链接 https://mp.weixin.qq.com/s/xxxx
clip this article https://x.com/xxx/status/xxx
```

也可以直接发链接，skill 会自动识别。

## 支持的平台

| 平台 | 示例 URL |
|------|---------|
| 知乎专栏 | `https://zhuanlan.zhihu.com/p/...` |
| 知乎回答 | `https://www.zhihu.com/question/.../answer/...` |
| 微信公众号 | `https://mp.weixin.qq.com/s/...` |
| Twitter/X | `https://x.com/...` / `https://twitter.com/...` |

## 注意事项

### ⚠️ 下载前关闭 Edge

下载时依赖 Edge 的登录态 cookies。**Edge 浏览器开着会导致抓取结果为空**，只有 frontmatter 没有正文。

```text
❌ Edge 开着 → 抓不到正文
✅ Edge 关掉 → 正常下载全文 + 图片
```

### 下载后自动入库建议

每次下载完成后，可以接着执行：

```
wiki ingest <保存路径>
wiki compile
```

或者用 consolidate 批量处理：

```
wiki consolidate
```

## 示例会话

```
你: 帮我下载这个文章 https://zhuanlan.zhihu.com/p/259255036
我: 正在下载...
    ✅ 下载成功
    保存位置: LLM_Wiki/raw/articles/zhihu/2026/0610/xxx/content.md
    下一步: wiki ingest → wiki compile 入库
```
