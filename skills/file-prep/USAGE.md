# File Prep — 使用说明

## 概述

大文件预处理器。用于将 >50KB 或含嵌入图片的 Markdown 文件拆分为适合 LLM Wiki ingest 的小块，同时处理好图片迁移和路径重写。

## 使用场景

| 场景 | 说明 |
|------|------|
| 整本书/长文档 | PDF 导出、全书 Markdown，按章节切片到 <50KB |
| 含图片的文档 | 将 `![](path)` 重写为 `![[source-name/filename]]`，图片归入 `raw/attachments/` |
| 合并大文件 | 从 `edge_wiki_to_md.py` 等工具生成的超大合并文件 |

## 命令格式

```
prep <file-path> [--source-name <name>]
```

参数：
- `<file-path>` — 文件路径（相对 vault 根目录）
- `--source-name` — （可选）来源名称，默认从文件名推断

## 处理流程

```
输入: 大文件 (>50KB 或含图片)
  │
  1. 扫描图片引用 → 提取所有 ![](path)
  2. 复制图片 → raw/attachments/<source-name>/
  3. 重写路径 → ![](path) → ![[source-name/filename]]
  4. 按章节切片 → 以 # / ## 标题为界，每块 <45KB
  5. 小片合并 → <5KB 的切片合并到相邻块
  │
  ▼
输出: N 个切片文件 + raw/attachments/<source-name>/ 中的图片
  │
  ▼
下一步: wiki ingest → wiki compile
```

## 输出结构

```
raw/
├── attachments/<source-name>/
│   ├── abc.jpg
│   ├── def.png
│   └── ...
└── articles/<source-folder>/
    ├── <source-name>_p1.md
    ├── <source-name>_p2.md
    └── ...
```

## 示例

```
prep LLM_Wiki/raw/articles/pci_express/merged.md --source-name pci-express
```

结果：
- 181 张图片 → `raw/attachments/pci-express/`
- 39 个切片 → `raw/articles/pci_express/`
- 所有图片路径：`![[pci-express/hash.jpg]]`

## 注意事项

- 切片以 `#` / `##` 标题为分割点，无标题时按行数平分
- 超过 50KB 的章节按二级子标题再拆
- 图片找不到源文件时会保留引用但给出警告
- 预处理完成后建议先用 `wiki ingest` 摄入第一个切片测试

## 参考

完整 Skill 定义：[[skills/file-prep/SKILL.md]]
Python 脚本实现：[[skills/file-prep/file-prep.py]]

```bash
# 命令行直接使用
python skills/file-prep/file-prep.py <file-path> [--source-name <name>]
```
