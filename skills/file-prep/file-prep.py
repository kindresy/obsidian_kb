#!/usr/bin/env python3
"""
file-prep.py — LLM Wiki 大文件预处理器

将 >50KB 或含嵌入图片的 Markdown 文件拆分为 <50KB 的切片，
同时将图片迁移到 raw/attachments/<source-name>/ 并重写路径为 Obsidian 格式。

用法:
  python file-prep.py <file-path> [--source-name <名称>] [--vault <路径>]

示例:
  python file-prep.py LLM_Wiki/raw/articles/book/merged.md --source-name my-book
  python file-prep.py /path/to/large-file.md --vault /path/to/vault
"""

import argparse
import os
import re
import shutil
import sys
from pathlib import Path


# ------ 常量 ----------------------------------------------------------------------------------------------------------------------------------------

MAX_CHUNK_SIZE = 45 * 1024  # 45KB（预留安全余量至 50KB）
MERGE_THRESHOLD = 0.3       # 小于此比例的片段合并到前一块
SECTION_HEADING_RE = re.compile(r'^# (?:\d+\.\d+|（\d+）|\(\d+\)|第[一-鿿]+章)')
IMAGE_MD_RE = re.compile(r'!\[[^\]]*\]\(([^)]+\.(?:jpg|jpeg|png|gif|webp|bmp|svg))\)', re.IGNORECASE)
IMAGE_HTML_RE = re.compile(r'<img\s+[^>]*src="([^"]+\.(?:jpg|jpeg|png|gif|webp|bmp|svg))"[^>]*>', re.IGNORECASE)
WIKILINK_IMG_RE = re.compile(r'!\[\[([^\]|/\s]+\.(?:jpg|jpeg|png|gif|webp|bmp|svg))\]\]', re.IGNORECASE)
WIKILINK_IMG_PREFIXED_RE = re.compile(r'!\[\[([^/\]]+/[^\]|]+\.(?:jpg|jpeg|png|gif|webp|bmp|svg))\]\]', re.IGNORECASE)


# ------ 工具函数 --------------------------------------------------------------------------------------------------------------------------------

def detect_vault_root(start: str) -> str:
    """从 start 目录向上查找包含 CLAUDE.md + wiki/ 的 vault 根目录。"""
    current = Path(start).resolve()
    for _ in range(20):  # 最多向上 20 层
        claude_md = current / "CLAUDE.md"
        wiki_dir = current / "LLM_Wiki" / "wiki"
        if claude_md.exists() and wiki_dir.exists():
            return str(current)
        parent = current.parent
        if parent == current:
            break
        current = parent
    return str(Path(start).resolve())  # 未找到则用起始目录


def resolve_path(path_str: str, vault_root: str) -> str:
    """将相对路径解析为绝对路径。"""
    p = Path(path_str)
    if p.is_absolute():
        return str(p)
    # 相对 vault 根目录
    return str(Path(vault_root) / path_str)


def slugify(name: str) -> str:
    """将名称转为 kebab-case。"""
    name = re.sub(r'[^\w\s-]', '', name)
    name = re.sub(r'[\s_]+', '-', name)
    name = name.strip('-').lower()
    return name or "untitled"


def file_size_kb(path: str) -> float:
    return os.path.getsize(path) / 1024


def count_lines(path: str) -> int:
    with open(path, 'r', encoding='utf-8') as f:
        return sum(1 for _ in f)


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def print_step(step: str, msg: str = ""):
    """格式化输出步骤。"""
    print(f"\n  [{step}] {msg}" if msg else f"\n  [{step}]")


def print_ok(msg: str):
    print(f"    [OK] {msg}")


def print_warn(msg: str):
    print(f"    [WARN] {msg}")


def print_info(msg: str):
    print(f"    [INFO] {msg}")


def print_table(rows: list, headers: list):
    """打印对齐的表格。"""
    col_widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            col_widths[i] = max(col_widths[i], len(str(cell)))

    sep = "  ".join("--" * w for w in col_widths)
    header_line = "  ".join(h.ljust(w) for h, w in zip(headers, col_widths))
    print(f"    {header_line}")
    print(f"    {sep}")
    for row in rows:
        line = "  ".join(str(c).ljust(w) for c, w in zip(row, col_widths))
        print(f"    {line}")


# ------ 核心功能 --------------------------------------------------------------------------------------------------------------------------------

def scan_images(file_path: str) -> list:
    """
    扫描文件中的所有图片引用。
    返回 [(原始引用文本, 图片文件名, 图片相对路径), ...]
    """
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    found = []
    seen_filenames = set()

    # 匹配 ![](path/to/image.jpg)
    for match in IMAGE_MD_RE.finditer(content):
        img_path = match.group(1)
        filename = os.path.basename(img_path)
        found.append((match.group(0), filename, img_path))
        seen_filenames.add(filename)

    # 匹配 <img src="path/to/image.jpg">
    for match in IMAGE_HTML_RE.finditer(content):
        img_path = match.group(1)
        filename = os.path.basename(img_path)
        found.append((match.group(0), filename, img_path))
        seen_filenames.add(filename)

    # 匹配 ![[filename.jpg]]（无前缀的旧格式）
    for match in WIKILINK_IMG_RE.finditer(content):
        img_path = match.group(1)
        filename = os.path.basename(img_path)
        found.append((match.group(0), filename, img_path))
        seen_filenames.add(filename)

    return found


def relocate_images(images: list, file_dir: str, dest_dir: str) -> dict:
    """
    将图片从源位置复制到 raw/attachments/<source-name>/。
    返回 {filename: "copied" | "skipped" | "not_found"} 字典。
    """
    print_step(2, f"Relocating {len(images)} images -> {dest_dir}")
    ensure_dir(dest_dir)

    results = {}
    copied = 0
    skipped = 0
    not_found = 0

    for raw_ref, filename, rel_path in images:
        if filename in results:
            continue  # 已处理过同名文件

        # 尝试在多个位置寻找图片
        candidates = [
            Path(file_dir) / rel_path,           # 相对于 md 文件
            Path(file_dir) / os.path.basename(rel_path),  # 相对于 md 文件（仅文件名）
            Path(rel_path),                      # 绝对路径
        ]

        src_path = None
        for c in candidates:
            if c.exists() and c.is_file():
                src_path = str(c)
                break

        if not src_path:
            results[filename] = "not_found"
            print_warn(f"Image not found: {rel_path}")
            not_found += 1
            continue

        dst_path = os.path.join(dest_dir, filename)
        if os.path.exists(dst_path):
            # 已存在则跳过
            results[filename] = "skipped"
            skipped += 1
            continue

        shutil.copy2(src_path, dst_path)
        results[filename] = "copied"
        copied += 1

    print_ok(f"{copied} copied, {skipped} skipped, {not_found} not found")
    return results


def rewrite_image_paths(file_path: str, source_name: str, images: list) -> int:
    """
    将文件中的图片引用从 ![](path) / <img> 重写为 ![[source-name/filename]]。
    返回修改的次数。
    """
    print_step(3, "Rewriting image paths -> ![[source-name/...]]")

    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    changes = 0

    # 1. ![](path/to/file.jpg) -> ![[source-name/file.jpg]]
    def replace_md(match):
        nonlocal changes
        filename = os.path.basename(match.group(1))
        changes += 1
        return f'![[{source_name}/{filename}]]'

    content = IMAGE_MD_RE.sub(replace_md, content)

    # 2. <img src="path/to/file.jpg"> -> ![[source-name/file.jpg]]
    def replace_html(match):
        nonlocal changes
        filename = os.path.basename(match.group(1))
        changes += 1
        return f'![[{source_name}/{filename}]]'

    content = IMAGE_HTML_RE.sub(replace_html, content)

    # 3. ![[file.jpg]] -> ![[source-name/file.jpg]]（无前缀旧格式）
    def replace_wikilink(match):
        nonlocal changes
        img_path = match.group(1)
        # 如果已经有前缀则跳过
        if '/' in img_path:
            return match.group(0)
        changes += 1
        return f'![[{source_name}/{img_path}]]'

    content = WIKILINK_IMG_RE.sub(replace_wikilink, content)

    if changes > 0:
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(content)
        print_ok(f"{changes} paths rewritten")
    else:
        print_info("No image paths to rewrite")

    return changes


def split_by_sections(lines: list, source_name: str) -> list:
    """
    按章节标题将行列表拆分为 <MAX_CHUNK_SIZE 的块。
    返回 [行列表, ...]
    """
    total_text = ''.join(lines)
    total_size = len(total_text.encode('utf-8'))

    if total_size <= MAX_CHUNK_SIZE:
        return [lines]

    # 查找所有章节标题位置
    split_positions = []
    for i, line in enumerate(lines):
        if SECTION_HEADING_RE.match(line) and i > 0:
            split_positions.append(i)

    if not split_positions:
        # 无标题则按行数平分
        num_chunks = max(2, total_size // MAX_CHUNK_SIZE + 1)
        chunk_size = len(lines) // num_chunks
        chunks = []
        for i in range(0, len(lines), chunk_size):
            chunks.append(lines[i:i + chunk_size])
        return chunks

    # 按标题分割，合并小片段
    raw_chunks = []
    chunk_start = 0
    for pos in split_positions:
        chunk = lines[chunk_start:pos]
        if chunk:
            raw_chunks.append(chunk)
        chunk_start = pos
    if chunk_start < len(lines):
        raw_chunks.append(lines[chunk_start:])

    # 合并小片段至接近 MAX_CHUNK_SIZE
    merged = []
    buffer = []
    buffer_size = 0

    for chunk in raw_chunks:
        chunk_size = len(''.join(chunk).encode('utf-8'))
        if buffer_size + chunk_size <= MAX_CHUNK_SIZE:
            buffer.extend(chunk)
            buffer_size += chunk_size
        else:
            if buffer:
                merged.append(buffer)
            # 如果单块已经 > MAX_CHUNK_SIZE，需要二次拆分
            if chunk_size > MAX_CHUNK_SIZE:
                # 按行数平分
                sub_parts = []
                num_sub = max(2, chunk_size // MAX_CHUNK_SIZE + 1)
                sub_size = len(chunk) // num_sub
                for i in range(0, len(chunk), sub_size):
                    sub_parts.append(chunk[i:i + sub_size])
                merged.extend(sub_parts)
            else:
                buffer = chunk
                buffer_size = chunk_size

    if buffer:
        # 如果最后一块太小，合并到前一块
        if merged and buffer_size < MAX_CHUNK_SIZE * MERGE_THRESHOLD:
            merged[-1].extend(buffer)
        else:
            merged.append(buffer)

    return merged


def write_slice_files(chunks: list, out_dir: str, source_name: str) -> list:
    """
    将分割后的块写入文件。
    返回 [(文件名, 大小), ...]
    """
    print_step(4, f"Splitting into {len(chunks)} chunks -> {out_dir}")
    ensure_dir(out_dir)

    results = []
    for i, chunk_lines in enumerate(chunks):
        chunk_text = ''.join(chunk_lines)
        chunk_size = len(chunk_text.encode('utf-8'))

        if len(chunks) == 1:
            fname = f"{source_name}.md"
        else:
            fname = f"{source_name}_p{i + 1}.md"

        fpath = os.path.join(out_dir, fname)
        with open(fpath, 'w', encoding='utf-8') as f:
            f.write(chunk_text)

        results.append((fname, chunk_size))

    return results


def verify_image_paths(slice_dir: str, source_name: str) -> tuple:
    """验证所有切片文件的图片路径。返回 (旧格式数, 新格式数, 问题文件列表)。"""
    old_total = 0
    new_total = 0
    issues = []

    for fname in sorted(os.listdir(slice_dir)):
        if not fname.endswith('.md'):
            continue
        fpath = os.path.join(slice_dir, fname)
        with open(fpath, 'r', encoding='utf-8') as f:
            content = f.read()

        old = len(IMAGE_MD_RE.findall(content)) + len(IMAGE_HTML_RE.findall(content))
        wikilink_no_prefix = len(WIKILINK_IMG_RE.findall(content))
        wikilink_with_prefix = len(WIKILINK_IMG_PREFIXED_RE.findall(content))

        old_total += old + wikilink_no_prefix
        new_total += wikilink_with_prefix

        if old + wikilink_no_prefix > 0:
            issues.append(fname)

    return old_total, new_total, issues


def print_summary(source_name: str, file_path: str, images_result: dict,
                  slice_results: list):
    """打印最终总结报告。"""
    total_images = len(images_result)
    copied = sum(1 for v in images_result.values() if v == "copied")
    not_found_count = sum(1 for v in images_result.values() if v == "not_found")

    max_slice = max((s for _, s in slice_results), default=0)

    print("\n" + "=" * 60)
    print(f"  [OK]  File prep complete: {source_name}")
    print("=" * 60)

    info = [
        ("Source", os.path.basename(file_path)),
        ("Source size", f"{file_size_kb(file_path):.0f} KB ({count_lines(file_path)} lines)"),
        ("Images relocated", f"{copied} -> raw/attachments/{source_name}/"),
    ]
    if not_found_count > 0:
        info.append(("Images NOT found", str(not_found_count)))
    info.append(("Slice files", f"{len(slice_results)} -> {os.path.dirname(slice_results[0][0]) if slice_results else 'N/A'}"))
    info.append(("Max slice", f"{max_slice / 1024:.0f} KB"))

    for label, value in info:
        print(f"  {label:20s} {value}")

    # 切片大小表格
    print()
    rows = []
    for fname, size in slice_results:
        size_kb = size / 1024
        flag = "[!]" if size > 50 * 1024 else "[ok]"
        rows.append((fname, f"{size_kb:.0f} KB", flag))
    print_table(rows, ["File", "Size", "Status"])

    print()
    print(f"  >>  Next: wiki ingest each file, then wiki compile")


# ------ 主函数 ------------------------------------------------------------------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="LLM Wiki File Prep — 大文件预处理工具",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python file-prep.py raw/articles/book/merged.md --source-name my-book
  python file-prep.py /path/to/file.md --vault /path/to/vault
        """
    )
    parser.add_argument("file", help="要处理的 Markdown 文件路径")
    parser.add_argument("--source-name", "-n", help="来源名称（默认从文件名推断）")
    parser.add_argument("--vault", "-v", help="Obsidian vault 根目录（默认自动检测）")

    args = parser.parse_args()

    # ---- Step 0: Init ----
    print_step(0, "Initializing")

    # 检测 vault 根目录
    vault_root = args.vault or detect_vault_root(os.getcwd())
    print_info(f"Vault root: {vault_root}")

    # 解析文件路径
    file_path = resolve_path(args.file, vault_root)
    if not os.path.exists(file_path):
        print(f"\n  [ERR]  Error: File not found: {file_path}")
        sys.exit(1)

    file_size = file_size_kb(file_path)
    file_dir = os.path.dirname(file_path)

    print_info(f"File: {file_path}")
    print_info(f"Size: {file_size:.0f} KB ({count_lines(file_path)} lines)")

    # 确定 source-name
    source_name = args.source_name
    if not source_name:
        basename = os.path.splitext(os.path.basename(file_path))[0]
        source_name = slugify(basename)
    print_info(f"Source name: {source_name}")

    # 确定各类目录
    attachments_dir = os.path.join(vault_root, "LLM_Wiki", "raw", "attachments", source_name)
    out_dir = file_dir  # 切片输出到源文件同目录

    # ---- Step 1: Scan images ----
    print_step(1, "Scanning for embedded images")
    images = scan_images(file_path)

    if not images:
        print_info("No embedded images found")
    else:
        print_ok(f"Found {len(images)} image reference(s)")
        for raw_ref, filename, rel_path in images[:5]:
            print(f"      {filename}")
        if len(images) > 5:
            print(f"      ... and {len(images) - 5} more")

    # 判断是否需要预处理
    needs_split = file_size > 50
    needs_images = len(images) > 0

    if not needs_split and not needs_images:
        print_info("File is under 50KB and has no embedded images. No preprocessing needed.")
        print(f"\n  [OK]  Nothing to do. File is ready for wiki ingest.")
        sys.exit(0)

    # ---- Step 2: Relocate images ----
    image_results = {}
    if needs_images:
        image_results = relocate_images(images, file_dir, attachments_dir)

    # ---- Step 3: Rewrite image paths ----
    if needs_images:
        rewrite_image_paths(file_path, source_name, images)

    # ---- Step 4: Split file ----
    slice_results = []
    if needs_split:
        with open(file_path, 'r', encoding='utf-8') as f:
            lines = f.readlines()

        chunks = split_by_sections(lines, source_name)
        slice_results = write_slice_files(chunks, out_dir, source_name)

        # 验证
        print_step(5, "Verifying slices")
        over_50 = [(f, s) for f, s in slice_results if s > 50 * 1024]
        if over_50:
            for f, s in over_50:
                print_warn(f"{f}: {s / 1024:.0f} KB > 50KB")
        else:
            print_ok(f"All {len(slice_results)} slices are under 50KB")
    else:
        print_step(4, "Splitting")
        print_info(f"File is {file_size:.0f} KB, under 50KB threshold. No split needed.")
        slice_results = [(os.path.basename(file_path), os.path.getsize(file_path))]

    # ---- Step 5: Verify image paths ----
    if needs_images:
        old_style, new_style, issues = verify_image_paths(out_dir, source_name)
        print_step(6, "Verifying image paths")
        if old_style == 0:
            print_ok(f"All {new_style} image paths use ![[{source_name}/...]] format")
        else:
            print_warn(f"{old_style} old-style paths remaining in {issues}")

    # ---- Summary ----
    print_summary(source_name, file_path, image_results, slice_results)


if __name__ == "__main__":
    main()
