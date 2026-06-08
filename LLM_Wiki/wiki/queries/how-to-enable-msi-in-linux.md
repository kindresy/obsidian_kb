---
date: 2026-05-29
tags: [pci, linux, interrupt, code]
type: query-output
question: "如何在 Linux 中启用 MSI 中断"
status: filed
---

# 如何在 Linux 中启用 MSI 中断

在 Linux 中启用 MSI/MSI-X 中断涉及**两个层面**：PCI 设备驱动层的 API 调用，以及硬件/内核层的 MSI 机制。以下综合理论和代码实践进行说明。

## 层面一：驱动开发者的视角（API 调用）

### MSI-X（优先选择）

```c
/* edge.c:4999-5041 — Edge PCIe Driver */
int req_nvec = EDGE_IRQ_MAX_NUM; // 49
struct msix_entry entry[EDGE_IRQ_MAX_NUM];

for (i = 0; i < req_nvec; i++)
    entry[i].entry = i;

rc = pci_enable_msix_range(pdev, entry, 1, req_nvec);
if (rc == req_nvec) {
    edev->msix_enabled = 1;  // ✓ MSI-X enabled
}
```

- 使用 `pci_enable_msix_range()`（旧版）或 `pci_alloc_irq_vectors_affinity()`（新版）
- 每个向量可独立配置目标地址和 Data（参见 [[msi-msi-x]]）
- MSI-X Table 存储在 BAR 空间中，非配置空间
- 最多支持 2048 个条目

### MSI（备选）

```c
/* edge.c:5023-5037 — Fallback */
rc = pci_alloc_irq_vectors_affinity(pdev, 1, EDGE_IRQ_MAX_NUM,
                                     PCI_IRQ_MSI, NULL);
if (rc >= 0) {
    edev->msi_enabled = 1;  // ✓ MSI enabled (single vector)
}
```

- 使用 `pci_alloc_irq_vectors_affinity()`（内核 ≥ 4.10）或 `pci_enable_msi_block()`（旧版）
- 通过 `msi_capability_init` → `arch_setup_msi_irqs` → `msi_compose_msg` 完成配置
- Message Address 格式：`0xFEEX-XXXX`（含 Destination ID、RH、DM）
- Message Data 格式：Vector 号 + Trigger Mode + Delivery Mode

### Legacy INTx（最后备选）

```c
/* edge.c:4918-4926 — INTx fallback with IRQF_SHARED */
rc = request_irq(pdev->irq, edge_udma_isr,
                 IRQF_SHARED, EDGE_DRV_NAME, edev);
```

- 当设备不支持 MSI/MSI-X 时使用
- 需要 `IRQF_SHARED` 标志（多个设备共享同一 INTx 线路）
- 存在同步问题：设备需先读中断状态寄存器确保所有 DMA 写完成（参见 [[pci-interrupt-mechanism]]）

### 优先级决策逻辑

```c
/* edge_enable_irq_vectors() — Three-tier priority */
if (pci_find_capability(pdev, PCI_CAP_ID_MSIX) && ...) {
    → 尝试 MSI-X
} else if (pci_find_capability(pdev, PCI_CAP_ID_MSI) && ...) {
    → 尝试 MSI  
} else {
    → 回退 Legacy INTx
}
```

## 层面二：ISR 注册与 Message 生成

### 中断服务例程注册

```c
/* edge_setup_irqs() — Wire ISR and configure MSI generator */
if (edev->msix_enabled) {
    edge_setup_msix_irqs(edev);  // Per-vector ISR via entry[0].vector
} else {
    request_irq(pdev->irq, edge_udma_isr, ...);
    if (edev->msi_enabled) {
        // Configure MSI generator register (device-side):
        edge_writel(0x1, &msigen_base->msigen_ctrl);  // Enable MSIGEN
    }
}
```

### 设备侧 MSI Message 生成（Edge PCIe 具体实现）

```
PCIe X8 Controller
    │
    └── msigen_regs @ REG_PCIE_MSIGEN_ADDR (0x200000 + 0x600)
        ├── msigen_ctrl       — Enable bit
        ├── msigen_atu_data   — Message Data (vector #)
        ├── msigen_atu_addr_lo— Message Address low 32b (0xFEEX-XXXX)
        └── msigen_atu_addr_hi— Message Address high 32b
```

当设备需要触发中断时，硬件自动将 MSI 存储器写事务发送到 `msigen_atu_addr` 指定的地址，CPU APIC 根据 Message Data 中的 Vector 号进行分发。

## 层面三：中断数据流

```
Device Event → PCIe MSI-X Message → CPU APIC
                                         │
                                         ▼
  edge_udma_isr() ──→ schedule engine_service_work()
  edge_notify_irq_isr() ──→ notify_irq notifier chain (cross-chip comm)
  edge_exception_isr() ──→ exception FIFO read → exception_work()
```

## 完整流程总结

| 步骤 | 调用/机制 | 理论参考 | 代码参考 |
|------|----------|---------|---------|
| 1. 检测 MSI/MSI-X 能力 | `pci_find_capability()` | [[msi-msi-x]]: Capability 结构 | `edge.c:5006` |
| 2. 分配中断向量 | `pci_enable_msix_range()` / `pci_alloc_irq_vectors_affinity()` | [[msi-msi-x]]: x86 MSI 实现 | `edge.c:5012-5037` |
| 3. 配置 MSI Message | 内核 `msi_compose_msg()` → 设备 MSIGEN 寄存器 | [[msi-msi-x]]: Message Address/Data | `edge.c:4945` |
| 4. 注册 ISR | `request_irq()` | | `edge.c:4918-4926` |
| 5. 启用设备中断 | 设置 MSI Enable bit + 设备中断掩码 | [[msi-msi-x]]: Message Control | `edge.c:4945` |
| 6. (可选) 多向量路由 | 每个 MSI-X 向量绑定独立通知通道 | [[msi-msi-x]]: MSI-X Table | `edge.c:662-704` |

## 注意事项

- **MSI-X 优于 MSI**：每个向量独立配置，最多 2048 个，且支持 Per-vector Masking
- **MSI 优于 INTx**：避免 INTx 的中断同步问题和线路共享开销
- **lazy IRQ 注册**：Edge 驱动在 `edge_open()` 时才注册 IRQ，而非 `probe()` 时（当检测到 `EDGE_LINUX_MAGIC` 表示设备已准备就绪）
- **版本兼容**：内核 API 有变化，如 `LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0)` 时使用旧版 `pci_enable_msi_range()`，新版使用 `pci_alloc_irq_vectors_affinity()`

## 参考资料

- [[msi-msi-x]] — MSI/MSI-X 中断机制理论
- [[linux-pci-subsystem]] — Linux PCI 子系统初始化与中断架构
- [[pci-interrupt-mechanism]] — PCI INTx vs MSI 对比
