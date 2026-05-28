---
date: 2026-05-28
tags: [pci, interrupt, hardware]
type: concept
status: active
---

# MSI/MSI-X 中断机制

MSI（Message Signaled Interrupt）和 MSI-X 是 PCI/PCIe 总线中的消息 signaled 中断机制，取代传统的 INTx 边带中断引脚。

## Details

#### MSI Capability 结构
MSI 有四种变体（32/64 位地址，带/不带 Masking）：
- **Message Control**：MSI Enable、Multiple Message Capable（1-32 向量）、Multiple Message Enable、64-bit Address Capable、Per-vector Masking Capable
- **Message Address**：31:2 有效，1:0=0。x86 格式 0xFEEX-XXXX 含 Destination ID
- **Message Data**：低字节编码向量号（多 MSI 时）
- **Mask Bits / Pending Bits**：各 32 位，可选但推荐

#### MSI-X Capability 结构
- **Table Size**：最多 2048 个条目
- **Table BIR/Offset**：存储在 BAR 空间（非配置空间）
- **PBA BIR/Offset**：Pending Bit Array
- 每个条目：MsgBox（32b 地址）+ Upper Addr + Data + Vector Control（位 0 = Per Vector Mask）
- 每个向量可独立配置目标地址和 Data

#### x86 MSI 实现
- FSB Interrupt Message 总线事务直接发送到 CPU
- Message Address 0xFEEX-XXXX，含 Destination ID、RH、DM
- Message Data 含 Vector、Trigger Mode（0x00=edge）、Delivery Mode（Fixed/Lowest Priority）
- 优势：CPU 直接从报文获取中断向量号，无需读 ACK 寄存器

#### PowerPC MSI 实现（MPC8572）
- MPIC 中断控制器
- MSIIR 寄存器：SRS（MSIR 组选择）+ IBS（位选择）
- MSIR0-7：8 组 × 32 位 = 256 个中断
- MSIVPR0-7：优先级/向量/掩码/触发方式
- MSIDR0-7：路由到 CPU0/CPU1 或外部控制器
- PEXCSRBAR 将 CCSRBAR 映射到 PCI 总线域供设备写 MSIIR

#### Linux MSI/MSI-X 实现
- `pci_enable_msi_block`：检查 Multiple Message Capable，分配 `msi_desc`
- `arch_setup_msi_irqs`：创建 IRQ 号，调用 `setup_msi_irq`
- `msi_compose_msg`：填充 Message Address（Destination ID）和 Data（Vector）
- `pci_enable_msix`：验证 msix_entry，调用 `msix_capability_init`
- e1000e 示例：3 个 MSI-X 向量（Rx/Tx/Other），消除中断状态寄存器读取
- Linux 2.6.31 限制：MSI 仅支持单向量，MSI-X 支持多向量

#### Legacy INTx 与 MSI 的桥接
- PCIe 桥将 Legacy PCI 设备的 INTx 转换为 Assert_INTx/Deassert_INTx Message
- 不能直接转为 MSI（INTx 电平触发 vs MSI 边沿触发，语义不同）

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express]]
- [[linux-pci-subsystem]]

## Counter-Arguments and Gaps

...
