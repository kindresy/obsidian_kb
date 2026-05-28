# 向量检索测试案例

## 环境准备

```bash
cd LLM_Wiki
```

所有命令从 `LLM_Wiki/` 目录执行。

> **注意**: 当前使用 `all-MiniLM-L6-v2` 模型（英文优化），中英文混合查询效果最佳。
> 纯中文短查询可能召回不准确。如需更好中文支持，可改用 `paraphrase-multilingual-MiniLM-L12-v2`。

---

## 基础测试（已验证可用）

### 1. 单页强匹配 — PCIe Switch 固件

```bash
python3 search-index.py "PCIe Switch 固件存储位置" --top-k 3
```

**预期**: top-1 为 `wiki/pcie-switch-firmware-storage.md`，score ≈ 0.91

### 2. 单页强匹配 — MSI 中断

```bash
python3 search-index.py "MSI 中断机制 x86 实现" --top-k 3
```

**预期**: top-2 为 `wiki/msi-msi-x.md`，score ≈ 0.75

### 3. 单页强匹配 — 链路训练

```bash
python3 search-index.py "PCIe 链路训练 Link Training LTSSM" --top-k 3
```

**预期**: `wiki/pcie-link-training.md` 在 top-3，score ≈ 0.77

### 4. 单页强匹配 — 流量控制

```bash
python3 search-index.py "PCIe 流量控制 Flow Control 信用机制" --top-k 3
```

**预期**: top-1 为 `wiki/pcie-flow-control.md`

### 5. 单页强匹配 — 事务层 TLP

```bash
python3 search-index.py "TLP Transaction Layer Packet 格式 路由" --top-k 3
```

**预期**: top-1 为 `wiki/pci-express-transaction-layer.md`，score ≈ 0.77

### 6. 单页强匹配 — 数据链路层

```bash
python3 search-index.py "PCI Express Data Link Layer ACK NAK" --top-k 3
```

**预期**: `wiki/pci-express-data-link-layer.md` 应为 top-1

### 7. 单页强匹配 — ECAM 配置空间

```bash
python3 search-index.py "ECAM MMCFG 增强配置访问机制 内存映射" --top-k 3
```

**预期**: top-1 为 `wiki/pcie-ecam.md`，score ≈ 0.55

### 8. 单页强匹配 — ATS 地址翻译

```bash
python3 search-index.py "ATS ATC 地址翻译服务 IOMMU" --top-k 3
```

**预期**: top-1 为 `wiki/pcie-ats.md`

### 9. 单页强匹配 — Linux PCI

```bash
python3 search-index.py "Linux PCI 子系统 初始化 中断" --top-k 3
```

**预期**: `wiki/linux-pci-subsystem.md` 在 top-3

### 10. 单页强匹配 — 排序规则

```bash
python3 search-index.py "PCIe 事务排序规则 Ordering" --top-k 3
```

**预期**: top-1 为 `wiki/pcie-ordering.md`

### 11. 单页强匹配 — Root Complex

```bash
python3 search-index.py "Root Complex Montevina 平台" --top-k 3
```

**预期**: top-1 为 `wiki/root-complex.md`，score ≈ 0.55

### 12. 单页强匹配 — Capric 卡

```bash
python3 search-index.py "Capric 卡设计实践 FPGA" --top-k 3
```

**预期**: top-1 为 `wiki/capric-card.md`

### 13. 单页强匹配 — 虚拟化

```bash
python3 search-index.py "SR-IOV IOMMU PCI 虚拟化" --top-k 3
```

**预期**: top-1 为 `wiki/pci-virtualization.md`

### 14. 单页强匹配 — PowerPC

```bash
python3 search-index.py "PowerPC RISC 处理器架构" --top-k 3
```

**预期**: top-1 为 `wiki/powerpc.md`

### 15. 单页强匹配 — MESIF 协议

```bash
python3 search-index.py "MESIF 缓存一致性协议 ccNUMA" --top-k 3
```

**预期**: top-1 为 `wiki/mesif-protocol.md`

---

## Top-K 参数测试

### 16. 只取最相关 1 个结果

```bash
python3 search-index.py "PowerPC 处理器架构" --top-k 1
```

**预期**: 只返回 `wiki/powerpc.md`

### 17. 取全部结果（不限量）

```bash
python3 search-index.py "PCIe flow control" --top-k 50
```

**预期**: 所有含相关内容的 chunks 按相似度排序

### 18. 不指定 top-k（默认 5）

```bash
python3 search-index.py "PCI express transaction layer"
```

**预期**: 返回 top-5 结果，top-1 为 `wiki/pci-express-transaction-layer.md`

---

## 边界情况测试

### 19. 短查询（1-2 个英文词）

```bash
python3 search-index.py "interrupt" --top-k 5
```

**预期**: 应返回 `wiki/pci-interrupt-mechanism.md` 和 `wiki/msi-msi-x.md`

### 20. 长查询（一段描述）

```bash
python3 search-index.py "In PCI Express architecture the Data Link Layer sits between Transaction Layer and Physical Layer responsible for reliable TLP transmission via ACK NAK protocol" --top-k 3
```

**预期**: `wiki/pci-express-data-link-layer.md` 应为 top-1（语义匹配而非关键词）

### 21. 领域外查询

```bash
python3 search-index.py "machine learning transformer" --top-k 3
```

**预期**: 分数普遍偏低（< 0.3），但会返回向量空间中最近的 chunks

### 22. 英文缩写

```bash
python3 search-index.py "TLP header format fields" --top-k 3
```

**预期**: top-1 为 `wiki/pci-express-transaction-layer.md`

---

## 端到端工作流测试

### 23. 重建索引 → 搜索

```bash
# 先重建索引
python3 build-index.py

# 然后搜索
python3 search-index.py "PCI bridge configuration mechanism" --top-k 3
```

**预期结果**:
- build-index 输出: 23 pages, ~121 chunks, ~54KB
- search top-1: `wiki/pci-bridge.md`

### 24. 全量搜索 + JSON 提取

```bash
# 提取所有匹配的页面名（去重）
python3 -W ignore search-index.py "PCIe physical layer encoding" --top-k 10 2>/dev/null | python3 -c "
import sys, json
results = json.load(sys.stdin)
seen = set()
for r in results:
    page = r['page']
    if page not in seen:
        seen.add(page)
        name = page.replace('wiki/', '').replace('.md', '')
        print(f'  [[{name}]]  (score: {r[\"score\"]:.4f})')
"
```

**预期**: 输出 top-10 中的去重页面，`pci-express-physical-layer` 在列

---

## 自动验证脚本

创建 `test-retrieval.sh`（保存到 `LLM_Wiki/` 目录下）：

```bash
#!/usr/bin/env bash
# test-retrieval.sh — 一键验证向量检索功能
set -e

cd "$(dirname "$0")"

echo "========================================"
echo " LLM Wiki 向量检索 — 功能验证"
echo "========================================"

echo ""
echo "=== 1. 重建索引 ==="
python3 build-index.py | tail -3

echo ""
echo "=== 2. 搜索测试 ==="
run_test() {
    local query="$1"
    local expected="$2"
    result=$(python3 -W ignore search-index.py "$query" --top-k 5 2>/dev/null)
    top_page=$(echo "$result" | python3 -c "
import sys, json
r = json.load(sys.stdin)
if r: print(r[0]['page'])
else: print('NO_RESULTS')
" 2>/dev/null)
    if echo "$top_page" | grep -q "$expected"; then
        echo "  [PASS] \"$query\" → $top_page"
    else
        echo "  [FAIL] \"$query\" → $top_page (expected: $expected)"
    fi
}

run_test "PCIe Switch 固件存储位置" "pcie-switch-firmware-storage"
run_test "MSI interrupt mechanism" "msi-msi-x"
run_test "PCIe link training LTSSM" "pcie-link-training"
run_test "PCIe flow control credit" "pcie-flow-control"
run_test "TLP transaction layer packet" "transaction-layer"
run_test "ECAM MMCFG enhanced configuration" "pcie-ecam"
run_test "Root Complex Montevina" "root-complex"
run_test "PowerPC architecture" "powerpc"
run_test "PCI bridge configuration" "pci-bridge"
run_test "PCIe ordering rules" "pcie-ordering"

echo ""
echo "=== 3. 统计 ==="
python3 -c "
import json, numpy as np
with open('.vector_index/chunks.json', encoding='utf-8') as f:
    chunks = json.load(f)
emb = np.load('.vector_index/embeddings.npy')
pages = set(c['page'] for c in chunks)
print(f'  页面数: {len(pages)}')
print(f'  Chunks: {len(chunks)}')
print(f'  向量维度: {emb.shape}')
print(f'  索引大小: {emb.nbytes} bytes (embeddings)')
"

echo ""
echo "========================================"
echo " 验证完成"
echo "========================================"
```

运行方式：

```bash
# 保存脚本后执行
bash LLM_Wiki/test-retrieval.sh
```

---

## 嵌入模型说明

当前使用 `all-MiniLM-L6-v2`（384 维，~80MB），优缺点：

| 特性 | 说明 |
|------|------|
| **速度** | 快。121 chunks 全量嵌入 ~4s |
| **英文查询** | 效果好。关键词 + 语义均佳 |
| **中英文混合** | 较好。中英混合查询效果接近英文 |
| **纯中文短查询** | 较差。模型以英文为主，中文语义捕捉有限 |
| **改进方案** | 改用 `paraphrase-multilingual-MiniLM-L12-v2`（多语言，~470MB）|

如需切换到多语言模型，修改 `build-index.py` 和 `search-index.py` 中的模型名即可：

```python
model = SentenceTransformer("paraphrase-multilingual-MiniLM-L12-v2")
```

然后重新运行 `python3 LLM_Wiki/build-index.py`。

---

## JSON 输出格式

`search-index.py` 输出 JSON 数组，每个元素包含：

| 字段 | 类型 | 说明 |
|------|------|------|
| `page` | string | 页面路径（相对 `LLM_Wiki/`），如 `wiki/pcie-flow-control.md` |
| `section` | string | 匹配的 `##` 小节标题，无标题时为 `(no heading)` |
| `score` | float | Cosine similarity（0~1），越高越相关 |
| `text` | string | chunk 文本内容 |
| `char_count` | int | 字符数 |

在 `wiki query` 操作中使用时，提取 `page` 字段后用 `Read` 工具读取对应页面即可。
