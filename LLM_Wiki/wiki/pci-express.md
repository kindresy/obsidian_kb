---
date: 2026-05-28
tags: [pci, hardware, bus]
type: concept
status: active
---

# PCI Express

PCI Express（PCIe）是替代传统 PCI 总线的高速串行计算机扩展总线标准，采用点对点拓扑结构。

## Details

#### 规范版本演进
| 版本 | 编码 | 单 Lane 速率 | x16 带宽 |
|------|------|-------------|----------|
| V1.x | 8b/10b | 2.5 GT/s | ~4 GB/s |
| V2.x | 8b/10b | 5.0 GT/s | ~8 GB/s |
| V3.0 | 128b/130b | 8.0 GT/s | ~16 GB/s |

#### 物理层特性
- **差分串行传输**：每条 Lane 为一对差分信号（TX+TX-/RX+RX-）
- **CDR（时钟数据恢复）**：无专用时钟线，接收端从数据流恢复时钟
- **REFCLK+/-**：100MHz 差分参考时钟，时滞差 15 英寸以内
- **AC 耦合电容**：发送端串联 75-200nF
- **GT/s 计算**：峰值带宽 = 总线频率 × 数据位宽 × 2（双沿采样）

#### 辅助信号
- PERST#：全局复位
- WAKE#：可选唤醒信号（Open Drain）
- Beacon：带内唤醒（L2 状态退出）
- SMCLK/SMDAT：SMBus 管理接口
- PRSNT1#/PRSNT2#：热插拔检测（长短针结构）

#### 架构组件
- **RC（Root Complex）**：CPU 与 PCIe 之间的根复合体 → [[root-complex]]
- **Switch**：扩展端口，内部包含多个虚拟 PCI-PCI 桥 → [[pci-bridge]]
- **Endpoint**：功能设备
- **DMI 接口**：MCH 与 ICH 间连接，不产生新总线号，MCH 正向译码/DMI 负向译码

#### 分层架构
- [[pci-express-transaction-layer|事务层]]：TLP 组装解析、流量控制、[[pcie-ordering|排序规则]]
- [[pci-express-data-link-layer|数据链路层]]：DLLP、ACK/NAK 协议、链路管理
- [[pci-express-physical-layer|物理层]]：差分信号、8b/10b 编码、链路训练

#### PCIe V2.1 新特性
- 原子操作（FetchAdd、Swap、CAS）
- TLP Prefix（Local 和 EP-EP）
- IDO（ID-Based Ordering）
- TPH/Steering Tag
- ARI（替代路由 ID 解释，支持最多 256 Function）

#### 三种路由方式
- **地址路由**：存储器/I/O TLP，通过虚拟 PCI-PCI 桥的 Base/Limit 寄存器译码
- **ID 路由**：配置请求、完成报文，通过 Bus/Device/Function 寻址
- **隐式路由**：Message 报文，使用 Route 字段（RC 路由/广播/本地/PME_TO_Ack）

#### VC 和 QoS
- 最多 8 个 VC（VC0-VC7）
- TC（Traffic Class）3 位共 8 级，缺省 TC0
- TC 与 VC 为"多对一"映射关系

## See Also

- [[pci-express-体系结构导读]]
- [[pci-bus]]
- [[root-complex]]
- [[linux-pci-subsystem]]

## Counter-Arguments and Gaps

...
