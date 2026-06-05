---
date: 2026-06-05
tags: [pci, linux, interrupt, code]
type: concept
status: promoted
---

# Linux MSI/MSI-X 中断启用流程

在 Linux 中启用 MSI/MSI-X 中断涉及**三个层面**：驱动层的 API 调用、设备侧的 MSI Message 生成、内核侧的中断生命周期管理。以下综合理论和代码实践进行说明。

## 驱动侧：三级优先级决策

### MSI-X（最优先）

```c
/* edge.c:4999-5041 — Edge PCIe Driver */
rc = pci_alloc_irq_vectors_affinity(pdev, 1, EDGE_IRQ_MAX_NUM,
                                     PCI_IRQ_MSIX, NULL);
```

- 最多 2048 个独立可配置向量
- MSI-X Table 存储在 **BAR 空间**（而非配置空间），每个条目有独立 Address/Data/Vector Control
- 每个向量可独立掩码（Per-vector Masking）

### MSI（备选）

```c
/* edge.c:5023-5037 — Fallback */
rc = pci_alloc_irq_vectors_affinity(pdev, 1, EDGE_IRQ_MAX_NUM,
                                     PCI_IRQ_MSI, NULL);
```

- 内核通过 `msi_capability_init` → `arch_setup_msi_irqs` → `msi_compose_msg` 为设备分配 IRQ 并构造 MSI Message
- Message Address 格式 `0xFEEX-XXXX`（含 Destination ID、RH、DM）
- Data 字段编码 Vector 号 + Trigger Mode + Delivery Mode
- 相比 MSI-X 的限制：向量数较少（1-32），所有向量共享一个 Message Address

### Legacy INTx（最后选择）

```c
/* edge.c:4918-4926 — INTx fallback */
rc = request_irq(pdev->irq, edge_udma_isr, IRQF_SHARED, ...);
```

- 需要 `IRQF_SHARED` 标志（INTx 线路在多个设备间共享）
- 需要读中断状态寄存器做同步（参见 [[pci-interrupt-mechanism]]）

## 设备侧：MSI Message 的生成

PCIe 设备需要内部 MSI Generator 模块来生成 MSI 存储器写事务：

```
PCIe Controller → msigen_regs
   ├── msigen_ctrl       — Enable bit
   ├── msigen_atu_data   — Message Data（向量号）
   ├── msigen_atu_addr_lo— Message Address 低 32 位（0xFEEX-XXXX）
   └── msigen_atu_addr_hi— Message Address 高 32 位
```

设备触发中断时，硬件自动发出 MSI 存储器写事务到 `msigen_atu_addr` 指定的地址，CPU APIC 根据 Data 中的 Vector 号分发到对应 ISR。

## 内核侧：中断生命周期

```
pci_alloc_irq_vectors_affinity()
  → 内核检测设备 MSI/MSI-X Capability（pci_find_capability）
  → 分配 IRQ 号并创建 msi_desc（arch_setup_msi_irqs）
  → 构造 MSI Message Address/Data（msi_compose_msg）
  → 写入设备 MSI-X Table 或 MSI 寄存器
  → 设置 MSI Enable bit（Message Control 寄存器）
  ↓
驱动 request_irq() 注册 ISR
  ↓
设备事件 → MSI Memory Write → CPU APIC → do_IRQ → ISR
```

## 三种中断方式对比

| 维度 | MSI-X | MSI | INTx |
|------|-------|-----|------|
| 最大向量数 | 2048 | 1-32 | 4 (共享) |
| 向量独立配置 | ✅ 每个向量独立 Address/Data | ❌ 共享 Address | N/A |
| Per-vector Masking | ✅ 硬件级支持 | 部分版本 | ❌ |
| 配置空间位置 | BAR 空间 | Capability 寄存器 | N/A |
| 内核推荐 | ⭐ `pci_alloc_irq_vectors_affinity()` | 备选 | 不推荐 |

## See Also

- [[msi-msi-x]] — MSI/MSI-X 中断机制理论详解
- [[linux-pci-subsystem]] — Linux PCI 子系统初始化与中断框架
- [[pci-interrupt-mechanism]] — INTx vs MSI 详细对比
- [[edge-pcie-core]] — Edge PCIe 驱动代码实现（`edge.c:4999-5041`）
- [[how-to-enable-msi-in-linux]] — 原始归档答案（更详细的代码示例和数据流）

## Counter-Arguments and Gaps

...

## Code References

- [[edge-pcie-core]] — edge-driver/edge.c:4999 — MSI-X/MSI allocation and INTx fallback
- [[edge-pcie-core]] — edge-driver/edge.c:4903 — MSI generator register configuration
