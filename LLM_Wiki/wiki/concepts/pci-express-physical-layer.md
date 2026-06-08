---
date: 2026-05-28
tags: [pcie, hardware, protocol]
type: concept
status: active
---

# PCIe 物理层（Physical Layer）

[[pci-express|PCI Express]] 分层架构的最底层，书中指出这是 PCIe 总线的"真正核心，也是中国工程师最没有机会接触的内容"。

## Details

#### 物理层结构
逻辑子层 + 电气子层：
- **逻辑子层**：TxBuffer → Mux → Byte Stripping → Scrambler → 8b/10b 编码 → 并转串
- **电气子层**：差分信号发送/接收

#### Byte Stripping / De-skew
- Byte Stripping：多 Lane 链路上分发数据字节
- De-skew：消除 Lane 间传播延迟差异

#### 加扰器（Scrambler）
- 16 位 LFSR，本原多项式 G(x) = X^16 + X^5 + X^4 + X^3 + 1
- 初始值 0xFFFF，周期 2^16-1
- 与伪随机序列异或后发送以降低 EMI
- 每处理 8b 后移位 8 次
- COM 字符（K28.5）重置 LFSR，SKP 不移位 LFSR
- 两端设备解码公式相同且完全同步

#### 8b/10b 编码
IBM 1983 年提出（已过期）。8 位拆分为 3 位（FGH）和 5 位（ABCDE），分别编码为 4 位（fghj）和 6 位（abcdei）。保证 DC 平衡，连续 1/0 不超过 5 个。

**CRD+ / CRD- 不平衡控制**：
- 字符流中 1>0 时 CRD 为正 → 使用 CRD- 编码
- 字符流中 0>1 时 CRD 为负 → 使用 CRD+ 编码
- 相等时 CRD 不变

#### 控制字符
| 符号 | 编码 | 用途 |
|------|------|------|
| STP | K27.7 | TLP 起始 |
| END | K29.7 | TLP/DLLP 结束 |
| COM | K28.5 | 复位 LFSR，建立 Symbol Lock |
| SKP | K28.0 | 时钟补偿（不移位 LFSR） |
| FTS | K28.1 | 快速训练序列（L0s 退出） |
| IDL | K28.3 | EIOS（电气空闲） |
| EIE | K28.7 | EIEOS（电气空闲退出） |
| SDP | K28.2 | DLLP 起始 |
| PAD | K23.7 | 填充字符 |
| EDB | K30.7 | 无效 TLP 结束 |

#### 物理层 TLP 格式
STP（K27.7，不扰码） + 扰码后的 Sequence# + 扰码后的 TLP Header/Payload + 扰码后的 LCRC + END（K29.7，不扰码）

#### 差分信号
- 共模电压 Vcm = (V1+V2)/2，差分电压 Vdiff = V1-V2
- UI（单位间隔）：Gen1 ~400ps，Gen2 ~200ps，时钟误差 ±300ppm
- 优点：抗 SSO 噪声、EMI 抑制、高带宽
- 缺点：两根线传一位、布线约束严格（等长/等宽/同层）
- 参考地平面仍是关键回流路径（常见误解：差分信号不需要地平面）

#### TX/RX 电气参数
| 参数 | 典型值 |
|------|--------|
| VTX-DIFF-PP | 0.8-1.2V |
| TTX-EYE | ≥0.75 UI |
| VTX-IDLE-DIFF-AC-p | 0-20mV |
| ZRX-DC | 40-60Ω |
| VRX-IDLE-DET-DIFFp-p | 65-175mV |

## See Also

- [[pci-express-体系结构导读]]
- [[pci-express]]
- [[pci-express-data-link-layer]]

## Counter-Arguments and Gaps

...
