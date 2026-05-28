---
date: 2026-05-28
tags: [pcie, hardware, design]
type: concept
status: active
---

# Capric 卡

《[[pci-express-体系结构导读|PCI Express 体系结构导读]]》第12章中记录的实际 PCIe 卡设计，用于 PCIe 总线延时与带宽分析。

## Details

### 硬件架构
- FPGA：Xilinx Vertex-5 XC5VLX50T（LogiCORE IP）
- 组成：LogiCORE + Transmit unit + Receive unit + BAR0 空间（256B）+ DMA 控制逻辑 + FPGA on-chip SRAM
- Vendor ID 0x10EE（Xilinx），Device ID 0x0007，Base Class 0x05（内存控制器）
- Max_Payload_Size 512B，Max_Read_Request_Size 512B

### BAR0 寄存器
| 偏移 | 名称 | 关键字段 |
|------|------|----------|
| 0x00 | DCSR1 | init_rst_0, int_rd_enb, int_wr_enb, int_rd_msk, int_rd_pending, int_wr_msk, int_wr_pending |
| 0x04 | DCSR2 | mwr_start, wr_done_clr, mrd_start, rd_done_clr |
| 0x08 | WR_DMA_ADR | DMA 写地址 |
| 0x0C | WR_DMA_SIZE | 11-bit，max 0x7FF |
| 0x1C | RD_DMA_ADR | DMA 读地址 |
| 0x20 | RD_DMA_SIZE | 11-bit，max 0x7FF |
| 0x2C | INT_REG | int_src_rd, int_src_wr, rd_done_clr, wr_done_clr, int_asserted |

Mask/Pending 位防止丢失和虚假中断（类似 MSI 机制）。

### TLP 处理
- 使用 4DW TLP Header（64 位地址）
- 对齐函数 Head_X(Y) = Y & ~(X-1)，Tail_X(Y) = Y | (X-1)（硬件高效位操作）
- DMA 写长度用 4 位加法器优化（非 32 位），节省 FPGA 资源

### 4KB 边界规则
PCIe 规定 TLP 不可跨越 4KB 边界。Capric 将跨越 4KB 的 DMA 拆为两个 TLP。当 M > 128B 时需额外 128B 对齐拆分：首 TLP 从 A[31:2] 开始，其余从 0x80 对齐处开始；末 TLP 在 B[31:2] 结束。

### DMA 读与 tag_queue
- 循环链表 tag_queue，tag_front/tag_rear 指针
- Tag 8 位（Extended Tag），每个条目 = 一个 Tag 资源
- 状态：(rear+1)%256==front（满），rear==front（空）
- 每个条目有 U（Used）和 L（Last）位
- 同一 Tag 的完成有序到达，不同 Tag 可交错乱序
- tag_queue 被作者描述为 Capric 的"奢侈品"——主要为 Cornus 卡的多通道并发 DMA 设计

### 三种 LogiCORE 流控模式
1. **Infinite Credit**：符合规范，但 LogiCORE 缓冲小（33-36 Header / 2176-2304B Data）导致流水线停顿
2. **One Posted/Non-Posted Header**：调试模式，一次只发一个读请求（不合规）
3. **Non-Infinite Credit**：Capric 采用，根据实际缓冲通告信用，避免停顿（技术上游离规范）

### 性能
- 作者评价 Capric 是"一个很糟糕的设计"——缺少流水线机制掩码延迟
- 驱动使用 `interruptible_sleep_on` 存在竞态（中断可能在进程进入等待队列前到达）
- DCSR1 的 mask/pending 位正是为此引入

### Legacy INTx 支持
除 MSI 外也支持 Legacy INTx（Assert INTx / Deassert INTx 消息）

## See Also

- [[pci-express-体系结构导读]]
- [[pcie-ordering]]
- [[pcie-ecam]]
- [[pcie-flow-control]]

## Counter-Arguments and Gaps

...
