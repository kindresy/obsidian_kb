#!/usr/bin/env python3
"""
LLM Wiki — Vector Index Builder

Reads wiki/*.md pages, splits into chunks, generates embeddings,
and stores the index in .vector_index/ for fast retrieval.
"""

import json
import os
import re
import sys
from pathlib import Path

import numpy as np
from sentence_transformers import SentenceTransformer

WIKI_DIR = Path(__file__).parent / "wiki"
INDEX_DIR = Path(__file__).parent / ".vector_index"
EXCLUDE_FILES = {"index.md"}


def strip_frontmatter(text: str) -> str:
    """Remove YAML frontmatter (--- ... ---) from the beginning of text."""
    return re.sub(r"^---\n.*?\n---\n", "", text, count=1, flags=re.DOTALL)


def chunk_text(text: str, source_file: str, max_chars: int = 800) -> list[dict]:
    """
    Split text into chunks by ## headings, then by paragraphs for large chunks.

    Returns list of:
      {"page": source_file, "section": heading, "chunk_id": int, "text": str, "char_count": int}
    """
    text = text.strip()
    if not text:
        return []

    # Split by ## headings (keep the heading as part of content)
    # Pattern: match lines starting with ## (but not ### or more)
    sections = re.split(r"^(?=## )", text, flags=re.MULTILINE)

    chunks = []
    chunk_id = 0

    for section in sections:
        section = section.strip()
        if not section:
            continue

        # Extract heading name
        heading_match = re.match(r"^## (.+)", section)
        heading = heading_match.group(1).strip() if heading_match else "(no heading)"

        if len(section) <= max_chars:
            chunks.append({
                "page": source_file,
                "section": heading,
                "chunk_id": chunk_id,
                "text": section,
                "char_count": len(section),
            })
            chunk_id += 1
        else:
            # Split long section by paragraphs (\n\n)
            paragraphs = re.split(r"\n\n+", section)
            buffer = ""
            for para in paragraphs:
                para = para.strip()
                if not para:
                    continue
                if len(buffer) + len(para) + 2 <= max_chars:
                    if buffer:
                        buffer += "\n\n" + para
                    else:
                        buffer = para
                else:
                    if buffer:
                        chunks.append({
                            "page": source_file,
                            "section": heading,
                            "chunk_id": chunk_id,
                            "text": buffer,
                            "char_count": len(buffer),
                        })
                        chunk_id += 1

                    # Overlap: last sentence of previous chunk (up to 50 chars)
                    overlay = ""
                    sentences = re.split(r"(?<=[。.!?！？])", buffer)
                    if sentences:
                        last_sent = sentences[-1].strip()
                        if len(last_sent) > 50:
                            overlay = "..." + last_sent[-50:]
                        else:
                            overlay = last_sent

                    if overlay:
                        buffer = overlay + "\n\n" + para
                    else:
                        buffer = para

            # Flush remaining buffer
            if buffer:
                chunks.append({
                    "page": source_file,
                    "section": heading,
                    "chunk_id": chunk_id,
                    "text": buffer,
                    "char_count": len(buffer),
                })
                chunk_id += 1

    return chunks


def collect_all_chunks() -> tuple[list[dict], int]:
    """Read all wiki pages and produce a flat list of chunks."""
    all_chunks = []
    total_bytes = 0

    md_files = sorted(WIKI_DIR.glob("**/*.md"))
    for md_path in md_files:
        if md_path.name in EXCLUDE_FILES:
            continue

        page_name = md_path.stem  # e.g. "pci-express" (no extension)
        rel_path = str(md_path.relative_to(WIKI_DIR.parent)).replace("\\", "/")

        content = md_path.read_text(encoding="utf-8")
        content = strip_frontmatter(content)
        total_bytes += len(content.encode("utf-8"))

        chunks = chunk_text(content, rel_path)
        all_chunks.extend(chunks)

    return all_chunks, total_bytes


def main():
    print("=" * 60)
    print("LLM Wiki — Vector Index Builder")
    print("=" * 60)

    # Ensure index directory exists
    INDEX_DIR.mkdir(parents=True, exist_ok=True)

    # Step 1: collect chunks from all wiki pages
    print("\n[1/4] Reading wiki pages and splitting into chunks...")
    chunks, total_bytes = collect_all_chunks()

    if not chunks:
        print("No chunks found. Nothing to index.")
        sys.exit(0)

    # Count unique pages
    pages = set(c["page"] for c in chunks)
    print(f"  Pages: {len(pages)}")
    print(f"  Chunks: {len(chunks)}")
    print(f"  Total source bytes: {total_bytes}")

    # Step 2: extract texts for embedding
    print("\n[2/4] Preparing texts for embedding...")
    texts = [c["text"] for c in chunks]
    print(f"  Text strings: {len(texts)}")

    # Step 3: generate embeddings
    print("\n[3/4] Generating embeddings (all-MiniLM-L6-v2)...")
    print("  (This may take a moment on first run while downloading the model...)")
    model = SentenceTransformer("all-MiniLM-L6-v2")
    embeddings = model.encode(texts, show_progress_bar=True, normalize_embeddings=True)
    print(f"  Embedding shape: {embeddings.shape}")

    # Step 4: save index
    print("\n[4/4] Saving index to .vector_index/...")

    # Save chunks metadata (exclude full text from JSON to keep it lean,
    # but include it since search needs the text)
    chunks_meta = []
    for c in chunks:
        chunks_meta.append({
            "page": c["page"],
            "section": c["section"],
            "chunk_id": c["chunk_id"],
            "text": c["text"],
            "char_count": c["char_count"],
        })

    chunks_path = INDEX_DIR / "chunks.json"
    with open(chunks_path, "w", encoding="utf-8") as f:
        json.dump(chunks_meta, f, ensure_ascii=False, indent=2)
    print(f"  chunks.json: {chunks_path} ({os.path.getsize(chunks_path)} bytes)")

    embeddings_path = INDEX_DIR / "embeddings.npy"
    np.save(str(embeddings_path), embeddings.astype(np.float32))
    print(f"  embeddings.npy: {embeddings_path} ({os.path.getsize(embeddings_path)} bytes)")

    # Save model info
    meta = {
        "model": "all-MiniLM-L6-v2",
        "dimension": embeddings.shape[1],
        "num_chunks": len(chunks),
        "num_pages": len(pages),
        "normalized": True,
    }
    meta_path = INDEX_DIR / "meta.json"
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, ensure_ascii=False, indent=2)
    print(f"  meta.json: {meta_path}")

    print("\n" + "=" * 60)
    print(f"Index built successfully: {len(chunks)} chunks from {len(pages)} pages")
    print("=" * 60)


if __name__ == "__main__":
    main()
