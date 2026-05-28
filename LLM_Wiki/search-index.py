#!/usr/bin/env python3
"""
LLM Wiki — Vector Index Search

Embeds a query string, finds top-k most similar chunks via cosine similarity,
and outputs results as JSON to stdout.

Usage:
    python3 search-index.py "<query>" [--top-k N]
"""

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from sentence_transformers import SentenceTransformer

INDEX_DIR = Path(__file__).parent / ".vector_index"


def load_index():
    """Load chunks metadata and embeddings from .vector_index/."""
    chunks_path = INDEX_DIR / "chunks.json"
    embeddings_path = INDEX_DIR / "embeddings.npy"
    meta_path = INDEX_DIR / "meta.json"

    if not chunks_path.exists():
        print("Error: chunks.json not found. Run build-index.py first.", file=sys.stderr)
        sys.exit(1)
    if not embeddings_path.exists():
        print("Error: embeddings.npy not found. Run build-index.py first.", file=sys.stderr)
        sys.exit(1)

    with open(chunks_path, "r", encoding="utf-8") as f:
        chunks = json.load(f)

    embeddings = np.load(str(embeddings_path))

    meta = {}
    if meta_path.exists():
        with open(meta_path, "r", encoding="utf-8") as f:
            meta = json.load(f)

    return chunks, embeddings, meta


def search(query: str, top_k: int = 5) -> list[dict]:
    """
    Embed the query, compute cosine similarity, return top-k results.

    Result format:
      {"page": "...", "section": "...", "score": 0.95, "text": "...", "char_count": N}
    """
    chunks, embeddings, meta = load_index()

    if len(chunks) == 0 or embeddings.shape[0] == 0:
        print("[]")
        return []

    # Load model (same as used for building index)
    model_name = meta.get("model", "all-MiniLM-L6-v2")
    model = SentenceTransformer(model_name)

    # Embed query
    query_emb = model.encode([query], normalize_embeddings=True)
    query_emb = query_emb.astype(np.float32)

    # Cosine similarity (since both are L2-normalized, dot product = cosine)
    scores = np.dot(embeddings, query_emb.T).flatten()

    # Top-k indices
    if top_k > len(scores):
        top_k = len(scores)
    top_indices = np.argsort(scores)[::-1][:top_k]

    # Build results
    results = []
    for idx in top_indices:
        chunk = chunks[idx]
        results.append({
            "page": chunk["page"],
            "section": chunk["section"],
            "score": round(float(scores[idx]), 4),
            "text": chunk["text"],
            "char_count": chunk["char_count"],
        })

    return results


def main():
    parser = argparse.ArgumentParser(description="Search the LLM Wiki vector index")
    parser.add_argument("query", type=str, help="Search query text")
    parser.add_argument("--top-k", type=int, default=5, help="Number of results (default: 5)")
    args = parser.parse_args()

    results = search(args.query, top_k=args.top_k)

    # Output as JSON array to stdout
    print(json.dumps(results, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
