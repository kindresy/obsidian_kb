---
date: 2026-05-28
tags: [linux, pci, driver]
type: concept
status: active
---

# Linux PCI 子系统

本书第III篇以 Linux 系统为例说明 PCI 总线在操作系统中的使用方法。

## Details

### Linux Initcall 系统
`do_initcalls` 循环执行 `__early_initcall_end` 到 `__initcall_end` 之间的函数。优先级：early → pure → core → postcore → arch → subsys → fs → device → late。`module_init()` 映射为 `device_initcall`（内置驱动）。

### 配置访问方法（pci_arch_init）
x86 支持三种：
1. **conf1**（pci_conf1_read/write）：I/O 端口 0xCF8/0xCFC，仅 256B
2. **conf2**：已废弃
3. **MMCFG/ECAM**（pci_mmcfg_read/write）：内存映射，支持完整 4KB，参见 [[pcie-ecam]]

### pci_subsys_init
- 非 ACPI 系统：`pcilegacy_init` → `pcibios_scan_root` → `pci_bus_add_devices`
- ACPI 系统：`pci_acpi_init` → 通过 ACPI 表枚举

### ACPI PCI 初始化
- `acpi_boot_table_init`：定位 RSDP/RSDT/XSDT
- `acpi_pci_init`：检查 FADT 的 MSI/ASPM 标志
- `acpi_pci_root_add`：读取 _SEG（Segment 号）和 _BBN（总线号），创建 `acpi_pci_root` 结构
- 支持多 HOST 桥（多 PCI Segment Group）
- 初始化链：`pci_acpi_scan_root` → `pci_create_bus` → `pci_scan_child_bus`

### PCI 总线枚举算法
`pci_scan_child_bus` 扫描 32 设备 × 8 功能：
1. `pci_scan_slot` → `pci_scan_single_device` → `pci_scan_device` + `pci_device_add`
2. `pci_scan_device` 读 Vendor ID（0xFFFFFF/0x000000/0xFFFF0000 为空槽，CRS 时指数退避最长 60s）
3. `pci_setup_device` 处理三种 Header：普通 Agent（6 BAR + ROM + IRQ）、PCI-PCI 桥（2 BAR + ROM）、CardBus

### 桥扫描算法
`pci_scan_bridge` 两遍：
- **Pass 0**：处理固件已枚举的桥（Subordinate/Secondary 非零）
- **Pass 1**：处理未枚举的桥，分配 Primary/Secondary/Subordinate 总线号
- DFS 递归遍历总线树

### BAR 空间分配
- `__pci_read_base`：写 ~0 → 读回确定大小（标准 PCI 方法）
- `pcibios_allocate_bus_resources`：DFS 检查桥资源包含关系
- `pci_bus_size_bridge` → `pci_bridge_check_ranges` → `pbus_size_io/mem`
- `pci_bus_assign_resources`：桥 Base/Limit 从下到上，设备 BAR 从上到下

### PowerPC PCI 初始化
- 使用 OpenFirmware / dts 文件（如 mpc8572ds.dts）
- `fsl_add_bridge` 创建 `pci_controller`（hose）结构
- `setup_indirect_pci`：配置访问方法（indirect 或 ECAM）
- `pci_process_bridge_OFRanges`：解析 dts `ranges` 属性
- `setup_pci_atmu`：配置 Inbound/Outbound 寄存器

### PCI 中断架构
`acpi_pci_irq_enable` → `acpi_pci_irq_lookup` 查找 _PRT → GSI → `acpi_register_gsi`（GSI 即 ACPI 的 I/O APIC IRQ_PIN 编号）→ `mp_register_gsi` 查找 I/O APIC → `io_apic_set_pci_routing` 编程 REDIR_TBL → `setup_IO_APIC_irq` → `assign_irq_vector` 在 vector_irq 表中映射 → `do_IRQ` 通过 vector_irq[vector] 分发

### MSI 使能
- `pci_enable_msi_block`：查 Multiple Message Capable，调用 `msi_capability_init`
- `arch_setup_msi_irqs`：创建 IRQ，`setup_msi_irq`
- `msi_compose_msg`：填充 Message Address（Destination ID）和 Data（Vector）
- Linux 2.6.31 仅支持单 MSI 向量

### MSI-X 使能
- `pci_enable_msix`：验证 msix_entry，去重，`msix_capability_init`
- e1000e 示例：3 向量（Rx/Tx/Other），消除中断状态寄存器读
- 比 MSI 更灵活，向量可多可少

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express]]
- [[msi-msi-x]]
- [[pcie-ecam]]
- [[host-bridge]]

## Counter-Arguments and Gaps

...
