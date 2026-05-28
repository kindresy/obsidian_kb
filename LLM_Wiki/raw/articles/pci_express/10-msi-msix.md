---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "10"
section: "第10章 MSI和MSI-X中断机制"
tags: [pci, pci-express, computer-architecture]
---
# 第10章 MSI和MSI-X中断机制

在PCI总线中，所有需要提交中断请求的设备，必须能够通过 $\mathrm{INTx}$ 引脚提交中断请求，而MSI机制是一个可选机制。而在PCIe总线中，PCIe设备必须支持MSI或者MSI-X中断请求机制，而可以不支持 $\mathrm{INTx}$ 中断消息。

在PCIe总线中，MSI和MSI-X中断机制使用存储器写请求TLP向处理器提交中断请求，下文为简便起见将传递MSI/MSI-X中断消息的存储器写报文简称为MSI/MSI-X报文。不同的处理器使用了不同的机制处理这些MSI/MSI-X中断请求，如PowerPC处理器使用MPIC中断控制器处理MSI/MSI-X中断请求，在第10.2节中将介绍这种处理情况；而x86处理器使用FSBInterruptMessage方式处理MSI/MSI-X中断请求。

不同的处理器对PCIe设备发出的MSI报文的解释并不相同。但是PCIe设备在提交MSI中断请求时，都是向MSI/MSI-X Capability结构中的Message Address的地址写Message Data数据，从而组成一个存储器写TLP，向处理器提交中断请求。

有些 PCIe 设备还可以支持 Legacy 中断方式。但是 PCIe 总线并不鼓励其设备使用 Legacy 中断方式，在绝大多数情况下，PCIe 设备使用 MSI 或者 MSI/X 方式进行中断请求。

PCIe总线提供Legacy中断方式的主要原因是，在PCIe体系结构中，存在许多PCI设备，而这些设备通过PCIe桥连接到PCIe总线中。这些PCI设备可能并不支持MSI/MSI-X中断机制，因此必须使用INTx信号进行中断请求。

当PCIe桥收到PCI设备的INTx信号后，并不能将其直接转换为MSI/MSI-X中断报文，因为PCI设备使用INTx信号进行中断请求的机制与电平触发方式类似，而MSI/MSI-X中断机制与边沿触发方式类似。这两种中断触发方式不能直接进行转换。因此当PCI设备的INTx信号有效时，PCIe桥将该信号转换为Assert\_INTx报文，当这些INTx信号无效时，PCIe桥将该信号转换为Deassert\_INTx报文。

与Legacy中断方式相比，PCIe设备使用MSI或者MSI-X中断机制，可以消除INTx这个边带信号，而且可以更加合理地处理PCIe总线的“序”。目前绝大多数PCIe设备使用MSI或者MSI-X中断机制提交中断请求。

MSI和MSI-X机制的基本原理相同，其中MSI中断机制最多只能支持32个中断请求，而且要求中断向量连续，而MSI-X中断机制可以支持更多的中断请求，而并不要求中断向量连续。与MSI中断机制相比，MSI-X中断机制更为合理。本章将首先介绍MSI/MSI-XCapability结构，之后分别以PowerPC处理器和x86处理器为例介绍MSI和MSI-X中断机制。

# 10.1 MSI/MSI-X Capability结构

PCIe 设备可以使用 MSI 或者 MSI-X 报文向处理器提交中断请求，但是对于某个具体的

PCIe设备，可能仅支持一种方式。在PCIe设备中含有两个Capability结构，一个是MSI Capability结构，另一个是MSI-X Capability结构。通常情况下一个PCIe设备仅包含一种结构，或者为MSI Capability结构，或者为MSI-X Capability结构。

# 10.1.1 MSI Capability结构

MSI Capability 结构共有四种组成方式，分别是 32 和 64 位的 Message 结构，32 位和 64 位带中断 Masking 的结构。MSI 报文可以使用 32 位地址或者 64 位地址，而且可以使用 Masking 机制使能或者禁止某个中断源。MSI Capability 寄存器的结构如图 10-1 所示。

![[pci_express/c4134323246500b0668def6b49f3d6e15d2e0def6acf076de584350cdde907a6.jpg]]  
图10-1 MSICapability结构

- Capability ID 字段记载 MSI Capability 结构的 ID 号，其值为 0x05。在 PCIe 设备中，每一个 Capability 结构都有唯一的 ID 号。  
- Next Pointer 字段存放下一个 Capability 结构的地址。   
- Message Control 字段。该字段存放当前 PCIe 设备使用 MSI 机制进行中断请求的状态与控制信息，如表 10-1 所示。

表 10-1 MSI Cabalibilities 结构的 Message Control 字段

<table><tr><td>Bits</td><td>定 义</td><td>描 述</td></tr><tr><td>15:9</td><td>Reserved</td><td>保留位。系统软件读取该字段时将返回全零,对此字段写无意义</td></tr><tr><td>8</td><td>Per-vector Masking Capable</td><td>该位为1时,表示支持带中断Masking的结构;如果为0,表示不支持带中断Masking的结构。该位对系统软件只读,在PCIe设备初始化时设置</td></tr><tr><td>7</td><td>64 bit Address Capable</td><td>该位为1时,表示支持64位地址结构;如果为0,表示只能支持带32位地址结构。该位对系统软件只读,在PCIe设备初始化时设置</td></tr><tr><td>6:4</td><td>Multiple Message Enable</td><td>该字段可读写,表示软件分配给当前PCIe设备的中断向量数目。系统软件根据Multiple Message Capable字段的大小确定该字段的值。在系统的中断向量资源并不紧张时,Multiple Message Capable字段和该字段的值相等;而资源紧张时,该字段的值可能小于Multiple Message Capable字段的值</td></tr><tr><td>3:1</td><td>Multiple Message Capable</td><td>该字段对系统软件只读,表示当前PCIe设备可以使用几个中断向量号,在不同的PCIe设备中该字段的值不同。当该字段为0b000时,表示PCIe设备可以使用1个中断向量;为0b001、0b010、0b011、0b100和0b101时,表示使用4、8、16和32个中断向量;而0b110和0b111为保留位。该字段与Multiple Message Enable字段的含义不同,该字段表示当前PCIe设备支持的中断向量个数,而Multiple Message Enable字段是系统软件分配给PCIe设备实际使用的中断向量个数</td></tr><tr><td>0</td><td>MSI Enable</td><td>该位可读写,是MSI中断机制的使能位。该位为1而且MSI-X Enable位为0时,当前PCIe设备可以使用MSI中断机制,此时Legacy中断机制被禁止。一个PCIe设备的MSI Enable和MSI-X Enable位都被禁止时,将使用INTx中断消息报文发出/结束中断请求</td></tr></table>

- Message Address 字段。当 MSI Enable 位有效时，该字段存放 MSI 存储器写事务的目的地址的低 32 位。该字段的 31:2 字段有效，系统软件可以对该字段进行读写操作；该字段的第 1 \~ 0 位为 0。  
- Message Upper Address 字段。如果 64 bit Address Capable 位有效，该字段存放 MSI 存储器写事务的目的地址的高 32 位。  
- Message Data 字段，该字段可读写。当 MSI Enable 位有效时，该字段存放 MSI 报文使用的数据。该字段保存的数值与处理器系统相关，在 PCIe 设备进行初始化时，处理器将初始化该字段，而且不同的处理器填写该字段的规则并不相同。如果 Multiple Message Enable 字段不为 0b000 时（即该设备支持多个中断请求时），PCIe 设备可以通过改变 Message Data 字段的低位数据发送不同的中断请求。  
- MaskBits字段。PCIe总线规定当一个设备使用MSI中断机制时，最多可以使用32个中断向量，从而一个设备最多可以发送32种中断请求。MaskBits字段由32位组成，其中每一位对应一种中断请求。当相应位为1时表示对应的中断请求被屏蔽，为0时表示允许该中断请求。系统软件可读写该字段，系统初始化时该字段为全0，表示允许所有中断请求。该字段和PendingBits字段对于MSI中断机制是可选字段，但是PCIe总线规范强烈建议所有PCIe设备支持这两个字段。  
- Pending Bits 字段。该字段对于系统软件是只读位，PCIe 设备内部逻辑可以改变该字段的值。该字段由 32 位组成，并与 PCIe 设备使用的 MSI 中断一一对应。该字段需要

与MaskBits字段联合使用。

当MaskBits字段的相应位为1时，如果PCIe设备需要发送对应的中断请求，PendingBits字段的对应位将被PCIe设备的内部逻辑置1，此时PCIe设备并不会使用MSI报文向中断控制器提交中断请求；当系统软件将MaskBits字段的相应位从1改写为0时，PCIe设备将发送MSI报文向处理器提交中断请求，同时将PendingBit字段的对应位清零。在设备驱动程序的开发中，有时需要联合使用MaskBits和PendingBits字段防止处理器丢弃中断请求。

# 10.1.2 MSI-X Capability 结构

MSI-X Capability 中断机制与 MSI Capability 的中断机制类似。PCIe 总线引出 MSI-X 机制的主要目的是为了扩展 PCIe 设备使用中断向量的个数，同时解决 MSI 中断机制要求使用中断向量号连续所带来的问题。

MSI中断机制最多只能使用32个中断向量，而MSI-X可以使用更多的中断向量。目前Intel的许多PCIe设备支持MSI-X中断机制。与MSI中断机制相比，MSI-X机制更为合理。首先MSI-X可以支持更多的中断请求，但这并不是引入MSI-X中断机制最重要的原因。因为对于多数PCIe设备，32种中断请求已经足够了。而引入MSI-X中断机制的主要原因是，使用该机制不需要中断控制器分配给该设备的中断向量号连续。

如果一个PCIe设备需要使用8个中断请求且使用MSI机制时，Message Data的[2:0]字段可以为 $0\mathrm{b}000\sim 0\mathrm{b}111$ ，因此可以发送8个中断请求，但是这8个中断请求的Message Data字段必须连续。在许多中断控制器中，Message Data字段连续也意味着中断控制器需要为这个PCIe设备分配8个连续的中断向量号。

有时在一个中断控制器中，虽然具有8个以上的中断向量号，但是很难保证这些中断向量号是连续的。因此中断控制器将无法为这些PCIe设备分配足够的中断请求，此时该设备的“Multiple Message Enable”字段将小于“Multiple Message Capable”。

而使用MSI-X机制可以合理解决该问题。在MSI-X Capability结构中，每一个中断请求都使用独立的Message Address字段和Message Data字段，从而中断控制器可以更加合理地为该设备分配中断资源。

与MSI Capability寄存器相比，MSI-X Capability寄存器使用一个数组存放Message Address字段和Message Data字段，而不是将这两个字段放入Capability寄存器中，本书将这个数组称为MSI-X Table。从而当PCIe设备使用MSI-X机制时，每一个中断请求可以使用独立的Message Address字段和Message Data字段。

除此之外MSI-X中断机制还使用了独立的Pending Table表，该表用来存放与每一个中断向量对应的Pending位。这个Pending位的定义与MSI Capability寄存器的Pending位类似。MSI-XTable和PendingTable存放在PCIe设备的BAR空间中。MSI-X机制必须支持这个PendingTable，而MSI机制的PendingBits字段是可选的。

# 1. MSI-X Capability 结构

MSI-X Capability 结构比 MSI Capability 结构复杂一些。在该结构中，使用 MSI-X Table 存

放该设备使用的所有 Message Address 和 Message Data 字段，这个表格存放在该设备的 BAR 空间中，从而 PCIe 设备可以使用 MSI-X 机制时，中断向量号可以不连续，也可以申请更多的中断向量号。MSI-X Capability 结构的组成方式如图 10-2 所示。

![[pci_express/d88a82a91d94ec02a6a1245aa90b5eea8d9c4f3c39723a9eeb29d42e29abf260.jpg]]  
图10-2 MSI-XCapability结构的组成方式

上图中各字段的含义如下所示。

- Capability ID 字段记载 MSI-X Capability 结构的 ID 号，其值为 0x11。在 PCIe 设备中，每个 Capability 都有唯一的 ID 号。  
- Next Pointer 字段存放下一个 Capability 结构的地址。   
- Message Control 字段，该字段存放当前 PCIe 设备使用 MSI-X 机制进行中断请求的状态与控制信息，如表 10-2 所示。

表 10-2 MSI-X Capability 结构的 Message Control 字段

<table><tr><td>Bits</td><td>定 义</td><td>描 述</td></tr><tr><td>15</td><td>MSI-X Enable</td><td>该位可读写,是 MSI-X中断机制的使能位,复位值为0,表示不使能 MSI-X中断机制。该位为1且 MSI Enable位为0时,当前PCIe设备使用MSI-X中断机制,此时INTx和MSI中断机制被禁止。当PCIe设备的MSI Enable和MSI-X Enable位为0时,将使用INTx中断消息报文发出/结束中断请求</td></tr><tr><td>14</td><td>Function Mask</td><td>该位可读写,是中断请求的全局Mask位,复位值为0。如果该位为1,该设备所有的中断请求都将被屏蔽;如果该位为0,则由Per Vector Mask位决定是否屏蔽相应的中断请求。Per Vector Mask位在MSI-X Table中定义,详见下文</td></tr><tr><td>10:0</td><td>Table Size</td><td>MSI-X中断机制使用MSI-X Table存放Message Address字段和Message Data字段。该字段用来存放MSI-X Table的大小,该字段对系统软件只读</td></tr></table>

- Table BIR（BAR Indicator Register）。该字段存放MSI-X Table所在的位置，PCIe总线规范规定MSI-X Table存放在设备的BAR空间中。该字段表示设备使用 $\mathrm{BAR0}\sim 5$ 寄存器中的哪个空间存放MSI-X table。该字段由三位组成，其中 $0\mathrm{b}000\sim 0\mathrm{b}101$ 与 $\mathrm{BAR0}\sim 5$ 空间一一对应。  
- Table Offset 字段。该字段存放 MSI-X Table 在相应 BAR 空间中的偏移。  
- PBA（Pending Bit Array）BIR字段。该字段表示Pending Table存放在PCIe设备的哪个BAR空间中，0表示BAR0空间，1表示BAR1空间，依此类推。在通常情况下，Pending Table和MSI-X Table存放在PCIe设备的同一个BAR空间中。  
- PBA Offset 字段。该字段存放 Pending Table 在相应 BAR 空间中的偏移。

# 2. MSI-X Table

MSI-X Table 的组成结构如图 10-3 所示。

由该图可见，MSI-X Table 由多个 Entry 组成，其中每个 Entry 与一个中断请求对应。每个 Entry 中有四个参数，其含义如下所示。

![[pci_express/0326952be3a4859f9e2caa905acd7c49c93f11c2ddc8867f25fb365514a8bfd9.jpg]]  
图10-3 MSI-X Table的组成结构

- MsgBox。当MSI-X Enable位有效时，该字段存放MSI-X存储器写事务的目的地址的低32位。该双字的31:2字段有效，系统软件可读写；1:0字段复位时为0，PCIe设备可以根据需要将这个字段设为只读，或者可读写。不同的处理器填入该寄存器的数据并不相同。  
- MsgBox Upper Addr，该字段可读写，存放MSI-X存储器写事务的目的地址的高32位。  
- MsgBox Data，该字段可读写，存放MSI-X报文使用的数据。其定义与处理器系统使用的中断控制器和PCIe设备相关。  
- Vector Control，该字段可读写。该字段只有第0位（即Per Vector Mask位）有效，其他位保留。当该位为1时，PCIe设备不能使用该Entry提交中断请求；为0时可以提交中断请求。该位在复位时为0。Per Vector Mask位的使用方法与MSI机制的Mask位类似。

# 3. Pending Table

Pending Table 的组成结构如图 10-4 所示。

![[pci_express/a3443ee04425a3fad9555637a0f4d14d7e58a9bbb27c0c6e7d5cb4050ea8fa32.jpg]]  
图10-4 PendingTable的组成结构

如上图所示，在 Pending Table 中，一个 Entry 由 64 位组成，其中每一位与 MSI-X Table 中的一个 Entry 对应，即 Pending Table 中的每一个 Entry 与 MSI-X Table 的 64 个 Entry 对应。与 MSI 机制类似，Pending 位需要与 Per Vector Mask 位配置使用。

当Per Vector Mask位为1时，PCIe设备不能立即发送MSI-X中断请求，而是将对应的Pending位置1；当系统软件将Per Vector Mask位清零时，PCIe设备需要提交MSI-X中断请求，同时将Pending位清零。

# 10.2 PowerPC处理器如何处理MSI中断请求

PowerPC 处理器使用 OpenPIC 中断控制器或者 MPIC 中断控制器，处理外部中断请求。其中 MPIC 中断控制器基于 OpenPIC 中断控制器，但是做出了许多增强，目前 Freescale 新推出的 PowerPC 处理器，其中断控制器多与 MPIC 兼容。

值得注意的是，PowerPC 处理器和 x86 处理器处理 MSI 报文的方式有较大的不同。其中

x86处理器使用的机制比PowerPC处理器更为合理，但是PowerPC处理器的方法使用的硬件资源相对较少。本节将MPC8572处理器为例说明MSI机制的处理过程，在第10.3节介绍x86处理器如何实现MSI机制。

MPIC中断控制器是Freescale的PowerPC处理器使用的通用中断控制器，目前基于E500内核的处理器，如MPC854x、8572等处理器使用这种中断控制器。目前Freescale使用QorIP架构，该架构使用的中断控制器与MPIC兼容。

使用 MPIC 中断控制器处理 MSI 中断时，PCIe 设备的 MSI 报文，其目的地址为 MPIC 中断控制器的 MSIIR 寄存器。当该寄存器被 PCIe 设备写入后，MPIC 中断控制器将向处理器内核提交中断请求，之后处理器再通过读取 MPIC 中断控制器的 ACK 寄存器获得中断向量号，并进行相应的中断处理。这种方式与 x86 处理器的 FSB Interrupt Message 机制相比，处理器需要读取 ACK 寄存器，从而中断处理的延时较大。

目前 Freescale 的 P4080 处理器对 MPIC 中断控制器进行了优化。在 P4080 处理器中，MPIC 中断控制器向处理器提交中断请求的同时，也向处理器内核提交中断向量，处理器内核不必读取 ACK 寄存器获得中断向量，从而缩短了中断处理延时。使用这种方法的效率与 x86 处理器使用的 FSB Interrupt Message 机制相当。

目前 Freescale 并没有完全公开 P4080 处理器的实现细节，因此本节仍以 MPC8572 处理器为例介绍 PCIe 设备的 MSI 中断请求。在 MPC8572 处理器中，MPIC 中断控制器的拓扑结构如图 10-5 所示。

![[pci_express/f698f592243d9124b5fcd196dd8cd49c3affcf5af2fa35846883ee744d7be98a.jpg]]  
图10-5 MPIC中断控制器的拓扑结构

由上图所示，MPIC中断控制器可以处理内部中断请求、外部中断请求，Message、处理器间中断请求和ShareMSI中断请求等。而MPIC中断控制器使用Int0、Int1等中断线向处理器提交这些中断请求。其中InternalInterrupts和ExternalInterrupts模块处理MPC8572内部和外部的中断请求，而ShareMSI处理来自PCIe设备的MSI或者MSI-X中断请求。

当 MPIC 中断控制器收到 MSI 报文后，将使用中断线 Int0、Int1 或者 cintn 向处理器内核提交中断请求。处理器内核被中断后，将读取 ACK 寄存器获得中断向量，然后执行相应的中断服务例程。为此 PowerPC 处理器设置了一系列寄存器，如下文所示。

# 10.2.1 MSI中断机制使用的寄存器

PowerPC 处理器设置了一系列寄存器，处理来自 PCIe 设备的 MSI 报文，其中最重要的寄存器是 MSIIR 寄存器。在 PowerPC 处理器系统中，PCIe 设备 Message Address 寄存器中存放的值都为 MSIIR 寄存器的物理地址，而 Message Data 寄存器中存放的数据也与 MSIIR 寄存器相关。

在PowerPC处理器系统中，MSI机制的实现过程是PCIe设备向MSIIR寄存器写入指定的数据。MPIC中断控制器发现该寄存器被写入后，将向处理器提交中断请求。处理器收到这个中断请求后，将通过读取MPIC中断控制器的ACK寄存器确定中断向量，并依此确定中断源。为此PowerPC处理器还设置了其他寄存器实现MSI中断机制。

# 1. MSIIR 寄存器

在PowerPC处理器中，MSIIR（Shared Message Signaled Interrupt Index Register）寄存器是实现MSI机制的重要寄存器。

当PCIe设备对MSIIR寄存器进行写操作时，MPC8572处理器将使能MSIRO-MSIR7寄存器的相应位，从而向MPIC中断控制器提交中断请求，而中断控制器将转发这个中断请求，由处理器进一步处理。该寄存器各字段的详细描述如表10-3所示。

表 10-3 MSIIR 寄存器

<table><tr><td>Bits</td><td>定义</td><td>描述</td></tr><tr><td>27~31</td><td>IBS</td><td>该字段用来选择MSIRO~MSIR7寄存器的对应位。0b00000对应SH0；0b00001对应SH1；0b00010对应SH2；以此类推0b11111对应SH31</td></tr><tr><td>24~26</td><td>SRS</td><td>该字段用来选择MSIRO~MSIR7寄存器。0b000对应MSIRO；0b001对应MSIR1；0b010对应MSIR2；以此类推0b111对应MSIR7</td></tr><tr><td>0~24</td><td></td><td>保留。</td></tr></table>

PCIe设备通过MSI机制，向此寄存器写入数据时，MSIRO\~7寄存器的相应位SH0\~31将有一位置1。例如PCIe设备向MSIIR寄存器写入 $0\mathrm{xFF00000}$ 时，MSIR7寄存器的SH31位将置1（SRS字段为0b111用来选择MSIR7，而IBS字段为0b11111用来选择SH31）。

# 2. MSIR 寄存器组

MSIR（Shared Message Signaled Interrupt Registers）寄存器组共由8个寄存器组成，分别

为 $\mathrm{MSIR0} \sim \mathrm{MSIR7}$ 。其中每一个 $\mathrm{MSIRx}$ 寄存器中有 32 个有效位，分别为 $\mathrm{SH0} \sim 31$ 。当 PCIe 设备对 $\mathrm{MSIIR}$ 寄存器进行写操作时，某一个 $\mathrm{MSIIRx}$ 寄存器的某个 SH 位将被置为有效。系统软件通过读取该寄存器获得中断源，该寄存器读清除，对此寄存器进行写操作没有意义。

该寄存器组的大小决定了一个PowerPC处理器支持的MSI中断请求的个数。在MPC8572处理器中，有8个 $\mathrm{MSIRx}$ 寄存器，每个寄存器由32个有效位组成，因此MPC8572处理器最多能够处理256个MSI中断请求。该寄存器的结构如图10-6所示。

<table><tr><td>0</td><td>1</td><td>2</td><td>3</td><td>4</td><td>5</td><td>6</td><td>7</td><td>8</td><td>9</td><td>10</td><td>11</td><td>12</td><td>13</td><td>14</td><td>15</td></tr><tr><td>SH31</td><td>SH30</td><td>SH29</td><td>SH28</td><td>SH27</td><td>SH26</td><td>SH25</td><td>SH24</td><td>SH23</td><td>SH22</td><td>SH21</td><td>SH20</td><td>SH19</td><td>SH18</td><td>SH17</td><td>SH16</td></tr><tr><td>16</td><td>17</td><td>18</td><td>19</td><td>20</td><td>21</td><td>22</td><td>23</td><td>24</td><td>25</td><td>26</td><td>27</td><td>28</td><td>29</td><td>30</td><td>31</td></tr><tr><td>SH15</td><td>SH14</td><td>SH13</td><td>SH12</td><td>SH11</td><td>SH10</td><td>SH9</td><td>SH8</td><td>SH7</td><td>SH6</td><td>SH5</td><td>SH4</td><td>SH3</td><td>SH2</td><td>SH1</td><td>SH0</td></tr></table>

图10-6 MSIRx寄存器的结构

# 3. MSISR 寄存器

MSISR 寄存器（Shared Message Signaled Interrupt Status Register）共由 8 个有效位组成，每一位对应一个 MSIR 寄存器。MPC8572 处理器设置该寄存器的主要目的是方便系统软件定位究竟是哪个 MSIR 寄存器中存在有效的中断请求。首先系统软件通过 MSISR 寄存器判断是哪个 $\mathrm{MSIRx}$ 寄存器存在有效请求，之后读取相应的 $\mathrm{MSIRx}$ 寄存器，该寄存器各字段的详细描述如表 10-4 所示。

表 10-4 MSISR 寄存器

<table><tr><td>Bits</td><td>定义</td><td>描述</td></tr><tr><td>0~23</td><td></td><td>保留</td></tr><tr><td>24~31</td><td>Sn</td><td>该字段由8位组成，每一位与一个MSIR0~7寄存器对应。该位为0时表示在MSIRn寄存器中没有有效位，即没有中断请求；该位为1时表示MSIRn寄存器中至少有一个有效位，即存在中断请求。Sn位是MSIRn寄存器各个位的“与”，当MSIRn寄存器的相应位清除时，Sn也将被清除</td></tr></table>

# 4. MSIVPR 寄存器组

MSIVPR（Shared Message Signaled Interrupt Vector/Priority Register）寄存器组由8个寄存器组成，分别为MSIVPR0\~7寄存器。该组寄存器设置对应中断请求的优先级别和中断向量。其中每个MSIVPR寄存器对应一个MSIR寄存器，MSIVPR寄存器各字段的详细解释如表10-5所示。

表 10-5 MSIVPR 寄存器

<table><tr><td>Bits</td><td>定义</td><td>描述</td></tr><tr><td>0</td><td>MSK</td><td>该位为0，且MSIR寄存器的对应位为1时，则将向中断控制器提交中断请求；如果为1屏蔽该中断请求</td></tr><tr><td>1</td><td>A</td><td>该位为0时，表示MPIC中断控制器没有处理该中断请求；该位为1时，表示MPIC中断控制器正在处理该中断请求，或者该中断控制器准备处理该中断请求，这个中断请求将在IPR（Interrupt Pending Regsiter）寄存器中排队等待处理，或者在ISR（Interrupt Service Register）寄存器中正在被处理。该位的详细描述见MPC8572的数据手册</td></tr><tr><td>12~15</td><td>PRIORITY</td><td>OpenPIC和MPIC中断控制器中为每一个中断请求设置了0~15，共16个优先级。其中1的优先权最低，15的优先权最高，0表示禁止中断请求</td></tr><tr><td>16~31</td><td>VECTOR</td><td>该字段存放该中断的中断向量。当处理器读取IACK寄存器时，将获得对应中断请求的中断向量。</td></tr></table>

通过该组寄存器可以发现，在MPC8572处理器系统中，PCIe设备最多可以使用8个中断向量，并可以共享这些中断向量。

# 5. MSIDR 寄存器组

MSIDR（Shared Message Signaled Interrupt Destination Registers）寄存器组共由8个寄存器组成，分别为MSIDRO\~7。其中每一个MSIDRn寄存器对应一个MSIR寄存器。

MPIC中断控制器支持Pass-through方式，在这种方式下，PowerPC处理器可以使用外部中断控制器处理中断请求（这种方法极少使用），而不使用内部中断控制器。MPIC中断控制器可以使用cint#和int#信号提交中断请求，但是绝大多数系统软件都使用int#信号向处理器提交中断请求。

此外在MPC8572处理器中有两个CPU，分别为CPU0和CPU1，MSI机制提交的中断请求可以由CPU0或者CPU1处理。系统软件可以通过设置MSIDRn寄存器完成这些功能，该寄存器各字段的详细描述如表10-6所示。

表 10-6 MSIDRn 寄存器

<table><tr><td>Bits</td><td>定 义</td><td>描 述</td></tr><tr><td>0</td><td>EP</td><td>为1时,表示中断请求输出到IRQ_OUT由外部中断控制器处理;为0时,表示由MPIC中断控制器处理</td></tr><tr><td>1</td><td>CIO</td><td>为1时,表示中断控制器使用cint#信号向CPU0提交中断请求</td></tr><tr><td>2</td><td>CI1</td><td>为1时,表示中断控制器使用cint#信号向CPU1提交中断请求</td></tr><tr><td>30</td><td>P1</td><td>为1时,表示中断控制器使用int#信号向CPU0提交中断请求</td></tr><tr><td>31</td><td>P0</td><td>为1时,表示中断控制器使用int#信号向CPU1提交中断请求</td></tr></table>

# 10.2.2 系统软件如何初始化PCIe设备的MSI Capability结构

如果PCIe设备支持MSI机制，系统软件首先设置该设备MSI Capability结构的Message Address和Message Data字段。如果该PCIe设备支持64位地址空间，即MSI Capability寄存器的64bit Address Capable位有效时，系统软件还需要设置Message Upper Address字段。系统软件完成这些设置后，将置MSI Capability结构的MSI Enable位有效，使能该PCIe设备的MSI机制。

其中 Message Address 字段所填写的值是 MSIIR 寄存器在 PCI 总线域中的物理地址。在 PowerPC 处理器中，PCI 总线域与存储器域地址空间独立，当 PCIe 设备访问存储器域的地址空间时，需要通过 Inbound 寄存器组将 PCI 总线域地址空间转换为存储器域地址空间。

在PowerPC处理器中，PCIe设备使用MSI机制访问MSIIR寄存器时，可以不使用In-bound寄存器组进行PCI总线地址到处理器地址的转换。在MPC8572处理器中，专门设置了一个PEXCSRBAR窗口，进行PCI总线域到存储器域的地址转换，使用这种方法可以节省

Inbound 寄存器窗口，Linux PowerPC 使用了这种实现方式。

在MPC8572处理器中，MSIIR寄存器的基地址为CCSRBAR（Configuration，Control, and Status Base Address Register)，其偏移为0x1740。为支持MSI中断机制，系统软件需要使用PEXCSRBAR窗口将MSIIR寄存器映射到PCI总线域地址空间，即将CCSRBAR寄存器空间映射到PCI总线域地址空间。之后PCIe设备就可以通过MSIIR寄存器在PCI总线域的地址访问MSIIR寄存器。

Linux PowerPC 使用 setup\_pci\_pcsrbar 函数设置 PEXCSRBAR 窗口，该函数的源代码在 ./arch/powerpc/sysdev/fsl\_pci.c 文件中，如源代码 10-1 所示，这段代码来自 Linux 2.6.30.5。

源代码10-1 setup\_pci\_pcsrbar函数

static void __init setup_pci_pcsrbar(structpci_controller\*hose)   
{ #ifdef CONFIG_PCI MSI phys_addr_t immr_base; immr_base $\equiv$ get-immrbase(); early_write_config_dword（hose，0，0，PCI_BASE_ADDRESS_0，immr_base）; #endif }

系统软件除了需要设置PCIe设备的Message Address字段和PEXCSRBAR窗口之外，还需要设置PCIe设备的Message Data字段。PCIe设备向MSIIR寄存器进行存储器写操作的数据存放在Message Data字段中。

系统软件在初始化 Message Data 字段之前，首先根据 Multiple Message Capable 字段预先存放的数据初始化 Multiple Message Enable 字段。一个 PCIe 设备最多可以申请 32 个中断请求，但是系统软件根据当前处理器系统的中断资源的使用情况，决定给这个 PCIe 设备提供多少个中断向量，并将这个结果存放到 Multiple Message Enable 字段。

MPC8572处理器最多可以为PCIe设备提供256个MSI中断请求。但是在某些极端的情况下，可能会出现PCIe设备需要的中断请求超过系统所能提供的中断请求。此时某些PCIe设备的Multiple Message Enable字段可能会小于Multiple Message Capable字段。

如果在PCIe设备中，使用了多个中断请求，那么Message Data字段存放的是一组中断向量号，而Message Data字段存放这组中断向量号的基地址。MSI机制要求“这组数据”连续，其范围在Message Data～Message Data + Multiple Message Enable-1之间。在多数情况下，MPC8572处理器系统仅为一个PCIe设备分配1个中断向量号。

由上所述，在MPC8572处理器系统中，PCIe设备使用存储器写TLP传送MSI中断报文，这个存储器写TLP使用的地址为PCIe设备Capability结构的MessageAddress字段，而

数据为 Message Data \~ Message Data + Multiple Message Enable-1 之间。其中 Message Data 字段与 MSIIR 寄存器要求的格式相同。

这个特殊的存储器写TLP报文通过若干Switch，并穿越RC后，最终将数据写入MSIIR寄存器中，并设置MSIIR寄存器的SRS和IBS字段，同时将使能MSIRO\~MSIR7寄存器的相应位，从而向中断控制器提交中断请求（如果MSIVPR寄存器的MSK位为1）。MPIC中断控制器获得该中断请求后，向处理器系统转发这个中断请求，并由处理器系统执行相应的中断服务例程进行中断处理。MPC8572处理器也可以处理PCIe设备的MSI-X中断机制，本节对此不做进一步介绍。

# 10.3 x86处理器如何处理MSI-X中断请求

PCIe 设备发出 MSI-X 中断请求的方法与发出 MSI 中断请求的方法类似，都是向 Message Address 所在的地址写 Message Data 字段包含的数据。只是 MSI-X 中断机制为了支持更多的中断请求，在 MSI-X Capability 结构中存放了一个指向一组 Message Address 和 Message Data 字段的指针，从而一个 PCIe 设备可以支持的 MSI-X 中断请求数目大于 32 个，而且并不要求中断向量号连续。MSI-X 机制使用的这组 Message Address 和 Message Data 字段存放在 PCIe 设备的 BAR 空间中，而不是在 PCIe 设备的配置空间中，从而可以由用户决定使用 MSI-X 中断请求的数目。

当系统软件初始化PCIe设备时，如果该PCIe设备使用MSI-X机制传递中断请求，需要对MSI-X Capability结构指向的Message Address和Message Data字段进行设置，并使能MSI-X Enable位。x86处理器在此处的实现与PowerPC处理器有较大的不同。

# 10.3.1 Message Address 字段和 Message Data 字段的格式

在 x86 处理器系统中，PCIe 设备也是通过向 Message Address 写入 Message Data 指定的数值实现 MSI/MSI-X 机制。在 x86 处理器系统中，PCIe 设备使用的 Message Adress 字段和 Message Data 字段与 PowerPC 处理器不同。

# 1. PCIe设备使用Message Adress字段

在x86处理器系统中，PCIe设备使用的Message Address字段仍然保存PCI总线域的地址，其格式如图10-7所示。

![[pci_express/1a426b6c03077b49100752f29d15dd687d13e8f40860643bfbcc7a0fa0e239fa.jpg]]  
图10-7 Message Address字段的格式

其中第31\~20位存放FSBInterrupts存储器空间的基地址，其值为0xFEED。当PCIe设备对0xFEEX-XXXX这段“PCI总线域”的地址空间进行写操作时，MCH/ICH会首先进行“PCI总线域”到“存储器域”的地址转换，之后将这个写操作翻译为FSB总线的InterruptMessage总线事务，从而向CPU内核提交中断请求。

x86处理器使用FSBInterruptMessage总线事务转发MSI/MSI-X中断请求。使用这种方法的优点是向CPU内核提交中断请求的同时，提交PCIe设备使用的中断向量，从而CPU不

需要使用中断响应周期从寄存器中获得中断向量。FSB Interrupt Message 总线事务的详细说明见下文。

Message Address 字段其他位的含义如下所示。

- Destination ID 字段保存目标 CPU 的 ID 号，目标 CPU 的 ID 与该字段相等时，目标 CPU 将接收这个 Interrupt Message。FSB Interrupt Message 总线事务可以向不同的 CPU 提交中断请求。  
- RH（Redirection Hint Indication）位为0时，表示Interrupt Message将直接发向与Destination ID字段相同的目标CPU；如果RH为1时，将使能中断转发功能。  
- DM（Destination Mode）位表示在传递优先权最低的中断请求时，Destination ID字段是否被翻译为Logical或者Physical APIC ID。在x86处理器中APIC ID有三种模式，分别为Physical、Logical和Cluster ID模式。  
- 如果 RH 位为 1 且 DM 位为 0 时，Destination ID 字段使用 Physical 模式；如果 RH 位为 1 且 DM 位为 1，Destination ID 字段使用 Logical 模式；如果 RH 位为 0，DM 位将被忽略。

以上这些字段的描述与x86处理器使用的APIC中断控制器相关。对APIC的详细说明超出了本书的范围，对此部分感兴趣的读者请参阅Intel 64 and IA-32 Architectures Software Developer's Manual Volume 3A：System Programming Guide，Part 1。

# 2. Message Data 字段

Message Data 字段的格式如图 10-8 所示。

![[pci_express/5a6ad65c1fbd97728de3b86046de9193c8df1de632496d0944d631d251f8ff0f.jpg]]  
图10-8 Message Data字段的格式

Trigger Mode字段为0b0x时，PCIe设备使用边沿触发方式申请中断；为0b10时使用低电平触发方式；为0b11时使用高电平触发方式。MSI/MSI-X中断请求使用边沿触发方式，但是FSB Interrupt Message总线事务还支持Legacy INTx中断请求方式，因此在Message Data字段中仍然支持电平触发方式。但是对于PCIe设备而言，该字段为0b0x。

Vector字段表示这个中断请求使用的中断向量。FSB Interrupt Message总线事务在提交中断请求的同时，将中断向量也通知给处理器。因此使用FSB Interrupt Message总线事务时，处理器不需要使用中断响应周期通过读取中断控制器获得中断向量号。与PowerPC的传统方式相比，x86处理器的这种中断请求的效率较高。

值得注意的是，在x86处理器中，MSI机制使用的Message Data字段与MSI-X机制相同。但是当一个PCIe设备支持多个MSI中断请求时，其Message Data字段必须是连续的，因而其使用的Vector字段也必须是连续的，这也是在x86处理器系统中，PCIe设备支持多个MSI中断请求的问题所在，而使用MSI-X机制有效避免了该问题。

Delivery Mode 字段表示如何处理来自 PCIe 设备的中断请求。

- 该字段为0b000时，表示使用“Fixed Mode”方式。此时这个中断请求将被DestINATION ID字段指定的CPU处理。  
- 该字段为0b001时，表示使用“Lowest Priority”方式。此时这个中断请求将被优先权最低的CPU处理。当使用“Fixed Mode”和“Lowest Priority”方式时，如果Vector字段有效，CPU接收到这个中断请求之后，将使用Vector字段指定的中断向量处理这些中断请求；而当Delivery Mode字段为其他值时，Message Data字段中所包含的Vector字段无效。  
- 该字段为 0b010 时，表示使用 SMI 方式传递中断请求，而且必须使用边沿触发，此时 Vector 字段必须为 0。这个中断请求将被 Destination ID 字段指定的 CPU 处理。  
- 该字段为0b100时，表示使用NMI方式传递中断请求，而且必须使用边沿触发，此时 Vector字段和Trigger字段的内容将被忽略。这个中断请求将被Destination ID字段指定的CPU处理。  
- 该字段为 0b101 时，表示使用 INIT 方式传递中断请求，Vector 字段和 Trigger 字段的内容将被忽略。这个中断请求将被 Destination ID 字段指定的 CPU 处理。  
- 该字段为0b111时，表示使用INTR信号传递中断请求且使用边沿触发。此时MSI中断信息首先传递给中断控制器，然后中断控制器通过INTR信号向CPU传递中断请求，之后CPU通过中断响应周期获得中断向量。上文中PowerPC处理器使用的方法与此方法类似。而在x86处理器中多使用Interrupt Message总线事务进行MSI中断信息的传递，因此这种模式很少使用。

边沿触发和电平触发是中断请求常用的两种方式。其中电平触发指外部设备使用逻辑电平1（高电平触发）或者0（低电平触发），提交中断请求。使用电平或者边沿方式提交中断请求时，外部设备一般通过中断线（IRQ\_PIN#）与中断控制器相连，其中多个外部设备可能通过相同的中断线与中断控制器相连（线与或者与门）。

外部设备在使用低电平触发提交中断请求的过程中，首先需要将IRQ\_PIN#信号驱动为低。当中断控制器将该中断请求提交给处理器，而且处理器将这个中断请求处理完毕后，处理器将通过写外部设备的某个寄存器来清除此中断源，此时外部设备将不再驱动IRQ\_PIN#信号线，从而结束整个中断请求。

IRQ\_PIN#信号线可以被多个外部设备共享，在这种情况之下，只有所有外部设备都不驱动IRQ\_PIN#信号线时，IRQ\_PIN#信号才为高电平。采用电平触发方式进行中断请求的优点是不会丢失中断请求，而缺点是一个优先权较高的中断请求有可能会长期占用中断资源，从而使其他优先权较低的中断不能被及时提交。因为优先级别较高的中断源可能会持续不断地驱动IRQ\_PIN#信号。

而边沿触发使用上升沿（0 到 1）或者下降沿（1 到 0）作为触发条件，但是中断控制器并不是使用这个“边沿”作为触发条件。中断控制器使用内部时钟对 IRQ\_PIN#信号进行采样，如果在前一个时钟周期，IRQ\_PIN#信号为 0，而后一个时钟周期，IRQ\_PIN#信号为 1，中断控制器认为外部设备提交了一个有效“上升沿”，中断控制器会锁定这个“上升沿”并向处理器发出中断请求。这也是外部设备至少需要将 IRQ\_PIN#信号保持一个时钟采样周期的原因，否则中断控制器可能无法识别本次边沿触发的中断请求，从而产生 Spurious 中断

请求。

外部设备使用“上升沿”进行中断申请时，不需要持续地将IRQ\_PIN#信号驱动为1，而只需要保证中断控制器可以进行正确采样这些中断信号即可。在处理边沿触发中断请求时，处理器不需要清除中断源。

使用边沿触发可以有效避免“优先级别”较高的中断源长期占用IRQ\_PIN#信号的情况，使用“下降沿”触发进行中断请求与“上升沿”触发类似。

但是外部设备使用边沿触发方式时，有可能会丢失一些中断请求。例如在一个处理器系统中，存在一个定时器，这个定时器使用上升沿触发方式向中断控制器定时提交中断。当处理器正在处理这个定时器的上一个中断请求时，将不会处理这个定时器发出的其他“边沿”中断请求，从而导致中断丢失。而使用电平触发方式不会出现这类问题，因为电平触发方式是一个“持续”过程，处理器只有处理完毕当前中断，并清除相应中断源之后，才会处理下一个中断源。

MSI中断请求实际上和边沿触发方式非常类似，MSI中断请求通过存储器写TLP实现，这个写动作是一个瞬间的动作，并不是一个持续请求，因此在x86处理器中MSI中断请求使用边沿触发进行中断请求。

还有一些外部设备可以通过I/O APIC进行中断请求，这些I/O APIC接收的外部中断需要标明是使用边沿或者电平触发，I/O APIC使用FSB Interrupt Message总线事务将中断请求发向Local APIC，并由Local APIC向处理器提交中断请求。

# 10.3.2 FSB Interrupt Message 总线事务

与MPC8572处理器处理MSI中断请求不同，x86处理器使用FSB的Interrupt Message总线事务，处理PCIe设备的MSI/MSI-X中断请求。由上文所示，MPC8572处理器处理MSI中断请求时，首先由MPIC中断控制器截获这个MSI中断请求，之后由MPIC中断控制器向CPU提交中断请求，而CPU通过中断响应周期从MPIC中断控制器的ACK寄存器中获得中断向量。

采用这种方式的主要问题是，当一个处理器中存在多个CPU时，这些CPU都需要通过中断响应周期从MPIC中断控制器的ACK寄存器中获得中断向量。在一个中断较为密集的应用中，ACK寄存器很可能会成为系统瓶颈。而采用Interrupt Message总线事务可以有效地避免这种系统瓶颈，因为使用这种方式中断信息和中断向量将同时到达指定的CPU，而不需要使用中断响应周期获得中断向量。

x86处理器也具有通过中断控制器提交MSI/MSI-X中断请求的方法，在I/O APIC具有一个“The IRQ Pin Assertion Register”寄存器，该寄存器地址为 $0\mathrm{xFEC00020}^{\ominus}$ ，其第4\~0位存放IRQ Number。系统软件可以将PCIe设备的Message Address寄存器设置为0xFEC00020，将Meaasge Data寄存器设置为相应的IRQ Number。

当PCIe设备需要提交MSI中断请求时，将向PCI总线域的0xFEC00020地址写入Message Data寄存器中的数据。此时这个存储器写请求将数据写入I/O APIC的The IRQ Pin As-

sertion Register 中，并由 I/O APIC 将这个 MSI 中断请求最终发向 Local APIC，之后再由 Local APIC 通过 INTR#信号向 CPU 提交中断请求。

上述步骤与MPC8572处理器传递MSI中断的方法类似。在x86处理器中，这种方式基本上已被弃用。下面以图10-9为例，说明x86处理器如何使用FSB总线的Interrupt Message总线事务，向CPU提交MSI/MSI-X中断请求。

![[pci_express/3e452c5d36f0641f354db5d67b1211708318b15eededa648ea9310b22a0853a4.jpg]]  
图10-9 使用InterruptMessage总线事务传递MSI中断请求

PCIe 设备在发送 MSI/MSI-X 中断请求之前，系统软件需要合理设置 PCIe 设备 MSI/MSI-X Capability 寄存器，使 Message Address 寄存器的值为 $0 \times \mathrm{FEExx}00 \mathrm{y}^{\ominus}$ ，同时合理地设置 Message Data 寄存器 Vector 字段。

PCIe 设备提交 MSI/MSI-X 中断请求时，需要向 0xFEExx00y 地址写 Message Data 寄存器中包含的数据，并以存储器写 TLP 的形式发送到 RC。如果 ICH 收到这个存储器写 TLP 时，将通过 DMI 接口将这个 TLP 提交到 MCH。MCH 收到这个 TLP 后，发现这个 TLP 的目的地址在 FSB Interrupts 存储器空间中，则将 PCIe 总线的存储器写请求转换为 Interrupt Message 总线事务，并在 FSB 总线上广播。

FSB总线上的CPU，根据APICID信息，选择是否接收这个InterruptMessage总线事务，并进入中断状态，之后该CPU将直接从这个总线事务中获得中断向量号，执行相应的中断服务例程，而不需要从APIC中断控制器获得中断向量。与PowerPC处理器的MPIC中断控制器相比，这种方法更具优势。

# 10.4 小结

本章详细描述了MSI/MSI-X中断机制的原理，并以PowerPC和x86两个处理器系统为例说明这两种中断机制实现机制。本章因为篇幅有限，并没有详细讲述这两个处理器使用的中断控制器。而理解这些中断控制器的实现机制是进一步理解MSI/MSI-X中断机制的要点。对此部分有兴趣的读者可以继续阅读MPIC中断控制器和APIC中断控制器的实现机制，以加深对MSI/MSI-X中断机制的理解。

设备的中断处理是局部总线的设计难点和重要组成部分，而中断处理的效率直接决定了局部总线的数据传送效率。在一个处理器系统的设计与实现中，中断处理的优化贯彻始终。

