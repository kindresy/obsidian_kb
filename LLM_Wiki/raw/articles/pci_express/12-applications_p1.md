---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "12"
section: "第12章 PCIe总线的应用"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第12章 PCIe总线的应用

本章以分析一个EP的硬件设计原理和基于这个EP的Linux驱动程序为线索，说明PCIe设备和基于该设备的Linux驱动程序的设计流程。本章使用的PCIe设备基于Xilinx公司Vetex-5XC5VLX50T内嵌的PCIExpressEP模块，该模块也被Xilinx称为LogiCORE。

LogiCORE 提供了 PCIe 设备的物理层和数据链路层，并做了一些基本的与事务层相关的工作，这使得许多设计者在并不完全清楚 PCIe 体系结构的情况下，也可以实现具有 PCIe 总线接口的设备。本章所述的 PCIe 设备基于 LogiCORE，本章将该 PCIe 设备简称为 Capric 卡。

# 12.1 Capric卡的工作原理

Capric卡的组成结构如图12-1所示。

![[pci_express/024c15ad9fe354f28ca7691de5947c916b817cfd3eb31c36dbbd3d885dab0e81.jpg]]  
图12-1 Capric卡的组成结构

Capric卡基于PCIe总线，主要功能是通过DMA读写方式与HOST处理器进行数据交换。Capric卡由LogiCORE、发送部件、接收部件、BAR空间、DMA控制逻辑和FPGA片内SRAM组成，其工作原理较为简单。

Capric卡首先使用DMA读方式，将主存储器中的数据搬移到FPGA的片内SRAM中，然后使用DMA写方式，将FPGA的片内SRAM中的数据写入主存储器中。在Capric卡中，一次DMA操作可以传送的数据区域的最大值为0x7FFB（0x2047B）。

Capric卡的各个组成模块的功能描述如下所示。

\- LogiCORE。其主要功能是处理 PCIe 设备的物理层、链路层与部分事务层的逻辑，并向外提供必要的接口。PCIe 设备配置空间的初始化，以及与配置和中断请求相关的总线事务也由 LogiCORE 完成。LogiCORE 是 PCIe 总线的接管者，其他部件通过 Log-

iCORE 与 PCIe 链路进行通信。LogiCORE 通过 “Host Interface” 实现 PCIe 设备的初始化配置。

- Capric卡的发送部件负责发送TLP报文，包括“存储器读请求”和“存储器写请求TLP”，但是并不包含配置和消息报文的发送。MSI报文由发送部件通过LogiCORE发送。  
- 接收部件负责接收“存储器读完成TLP”。Capric卡不支持I/O读写TLP。  
- DMA 控制逻辑协调发送与接收部件，以完成 DMA 写与 DMA 读操作，该逻辑的实现是 Capric 卡的设计重点。  
- BAR空间中存放了一组操纵DMA控制逻辑的寄存器，这组寄存器由HOST处理器和Capric卡共同读写，从而完成相应的DMA操作。Capric卡仅使用了BAR0空间，处理器使用存储器映像寻址方式，而不是I/O映像寻址方式访问BAR0空间。

# 12.1.1 BAR空间

Capric卡仅使用BAR0空间，其大小为 $256\mathrm{B}$ ，在该空间中包含以下寄存器，这些寄存器使用小端编码方式，如表12-1所示。

表 12-1 Capric 卡的 BAR 空间寄存器

<table><tr><td>缩 写</td><td>偏 移</td><td>描 述</td></tr><tr><td>DCSR1( Device Control and Status Register 1)</td><td>0x00</td><td>设备控制和状态寄存器1</td></tr><tr><td>DCSR2( Device Control and Status Register 2)</td><td>0x04</td><td>设备控制和状态寄存器2</td></tr><tr><td>WR_DMA_ADR</td><td>0x08</td><td>DMA写地址寄存器</td></tr><tr><td>WR_DMA_SIZE</td><td>0x0C</td><td>DMA写传送大小寄存器</td></tr><tr><td>RD_DMA_ADR</td><td>0x1C</td><td>DMA读地址寄存器</td></tr><tr><td>RD_DMA_SIZE</td><td>0x20</td><td>DMA读传送大小寄存器</td></tr><tr><td>INT_REG</td><td>0x2C</td><td>中断状态寄存器</td></tr><tr><td>ERR</td><td>0x30</td><td>错误状态寄存器</td></tr></table>

（1）DCSR1寄存器，该寄存器由7个有效位组成

- init rst\_0，第 0 位，该位可读写，复位值为 0。为 1 表示 Capric 卡处于复位状态，为 0 表示 Capric 卡已经完成复位。软件通过操纵该位对 Capric 卡进行复位。其过程为首先向此位写 1，然后延时至少 $5 \mu \mathrm{s}$ 后（Capric 卡内逻辑和 FPGA 的片内 SRAM 需要至少 $5 \mu \mathrm{s}$ 的复位时间），再向此位写 0。  
- int\_rd\_enb，第8位，该位可读写，复位值为0。为1表示当DMA读完成后，Capric卡可以向处理器提交中断请求；为0表示DMA读完成后，Capric卡不能向处理器提交中断请求。该位为DMA读完成中断使能位。  
- int\_WR\_enb，第9位，该位可读写，复位值为0。为1表示当DMA写完成后，Capric卡可以向处理器提交中断请求；为0表示DMA写完成后，Capric卡不能向处理器提交中断请求。该位为DMA写完成中断使能位。  
- int\_rd\_msk，第16位，该位可读写，复位值为0。为1表示当DMA读完成后，Capric

卡不能向处理器提交中断请求，而是置int\_rd\_pending位为1；为0表示DMA读完成后，Capric卡可以向处理器提交中断请求。该位为DMA读完成中断屏蔽位。

• int\_rd\_pending，第17位，该位可读写，复位值为0。为1表示Capric卡含有未发出的DMA读完成中断请求，当int\_rd\_msk位由1变为0时，Capric卡发送该中断请求；为0表示Capric卡不含有未发出的DMA读完成中断请求。  
- intWr\_msk，第24位，该位可读写，复位值为0。为1表示当DMA写完成后，Capric卡不能向处理器提交中断请求，而是置intWr\_pending位为1；为0表示DMA写完成后，Capric卡可以向处理器提交中断请求。该位为DMA写完成中断屏蔽位。  
- int\_WR\_pending，第25位，该位可读写，复位值为0。为1表示Capric卡含有未发出的DMA写完成中断请求，当int\_WR\_msk位由1变为0时，Capric卡发送该中断请求；为0表示Capric卡不含有未发出的DMA写完成中断请求。

设置Mask和Pending位的主要目的是为了防止中断丢失和产生Spurious中断请求，这两位与MSI Capability结构中的Mask和Pending位的功能相似。LogiCORE并不支持MSI Capability结构中的Mask和Pending位，因此Capric卡引入了这两个位。

(2) DCSR2，该寄存器由4位组成。分别为mwr\_start，DMA写启动位；mwr\_done，DMA写结束位；mrd\_start，DMA读启动位；mrd\_done，DMA读结束位。

- mwr\_start，第0位，该位可读写，复位值为0。系统软件向该位写1时启动DMA写操作，软件对此位写0无意义。  
- wr\_done\_CLR，第1位，可读，写1清除。当一次DMA写完成后，wr\_done\_CLR位将置1，系统软件将wr\_done\_CLR位写1清零后，mwr\_start位也将清零，之后系统软件可以重新启动下一次DMA写操作。  
- mrd\_start，第16位，该位可读写，复位值为0。系统软件向该位写1时启动DMA读操作，软件对此位写0无意义。  
- rd\_done\_CLR，第17位，可读，写1清除。当一次DMA读完成后，rd\_done\_CLR位将置1，系统软件将rd\_done\_CLR位写1清零后，mrd\_start位也将清零，之后系统软件可以重新启动下一次DMA读操作。

(3) WR\_DMA\_ADR, DMA 写地址寄存器, 该寄存器存放 DMA 写操作的目的地址, 该寄存器的复位值无意义。再一次强调该寄存器存放的地址为 PCI 总线域的地址, 而不是存储器域的地址, 尽管在许多处理器系统中, 该地址的 PCI 总线域地址与存储器域地址相等。该寄存器由 32 位组成, 存放 32 位的 PCI 总线域地址。  
(4) WR\_DMA\_SIZE, DMA 写传送大小寄存器。该寄存器存放一次 DMA 写操作的传送大小, 以字节为单位。该寄存器的复位值为 0 , 由 32 位组成, 但是只有低 11 位有效, 因此 Capric 卡一次 DMA 传送的最大值为 $0 \times 7 \mathrm{FF}$ 。该寄存器为 N 时表示一次 DMA 写操作的传送大小为 N 个字节。该寄存器为 0 时, 即便 DCSR2 寄存器的 mwr\_start 位为 1 时, 也不能启动 DMA 写传送。当 mwr\_start 位为 1 , 该寄存器由 0 变为其他数据时, 也将启动 DMA 写传送。  
(5) RD\_DMA\_ADR, DMA 读地址寄存器, 该寄存器存放 DMA 读操作的目的地址, 该寄存器的复位值无意义。该寄存器存放的地址为 PCI 总线域地址, 由 32 位组成。Capric 仅支持 32 位的 PCI 总线地址。

(6) RD\_DMA\_SIZE, DMA 读传送大小寄存器, 存放一次 DMA 读操作的传送大小, 以字节为单位。该寄存器的复位值为 0 , 由 32 位组成, 但是只有最低 11 位有效, 因此 Capric 卡一次 DMA 读传送的最大值为 $0 \times 7 \mathrm{FF}$ 。该寄存器为 N 时表示一次 DMA 读操作的传送大小为 N 个字节。该寄存器为 0 时, 即便 DCSR2 寄存器的 mrd\_start 位为 1 时, 也不能启动 DMA 读传送。当 mrd\_start 位为 1 时, 该寄存器由 0 变为其他数据时, 将启动 DMA 读传送。  
（7）INT\_REG，中断控制状态寄存器，该寄存器存放Capric卡的中断状态，该寄存器共由5个有效位组成。

- int\_src\_rd，第0位，该位只读，复位值为0。当Capric卡的DMA读操作完成后，而且DCSR1寄存器的int\_rd\_enb位为1时，该位为1表示Capric卡已经向处理器提交了DMA读完成中断请求。值得注意的是当int\_rd\_msk位为1时，Capric卡不能发送DMA读完成中断，此时int\_src\_rd位需要等待Capric卡置int\_rd\_msk位为0，发送完毕DMA读完成中断后，才能置1。  
- int\_src\_WR, 第 1 位, 该位只读, 复位值为 0 。当 Capric 卡的 DMA 写操作完成后, 而且 DCSR1 寄存器的 int\_src\_enb 位为 1 时, 该位为 1 表示 Capric 卡已经向处理器提交了 DMA 写完成中断请求。值得注意的是当 int\_src\_msk 位为 1 时, Capric 卡不能发送 DMA 写完成中断, 此时 int\_src\_WR 位需要等待 Capric 卡置 int\_src\_msk 位为 0 , 发送完毕 DMA 写完成中断后, 才能置 1 。  
- rd\_done\_CLR，第8位，该位可读写，复位值为0，该位为1表示DMA读操作完成。DMA读结束后该位由硬件置1，软件将该位清零后，硬件才能进行下一次DMA读操作。该位置1后，如果DCSR1寄存器的int\_rd\_enb位为1时，Capric卡将向处理器提交中断请求。

系统软件在中断服务例程中，向该位写1将清除该位；向该位写0无意义。如果Capric卡不使用中断方式接收数据，系统软件可以查询该位确定当前DMA读操作是否已经完成。此位与DCSR2寄存器的rd\_done\_CLR位相同，软件清除INT\_REG寄存器的该位后，DCSR2寄存器的rd\_done\_CLR位也被清零。

\- wr\_done\_CLR，第9位，该位可读写，复位值为0，该位为1表示DMA写操作完成。DMA写完成后该位由硬件置1，软件将该位清零后，硬件才能进行下一次DMA写操作。该位置1后，如果DCSR1寄存器的intWr\_enb位为1时，Capric卡将向处理器提交中断请求。

系统软件在中断服务例程中，向该位写1将清除该位；向该位写0无意义。如果Capric卡不使用中断方式接收数据，系统软件可以查询该位确定当前DMA写操作是否已经完成。此位与DCSR2寄存器的wr\_done\_CLR位相同，软件清除INT\_REG寄存器的该位后，DCSR2寄存器的wr\_done\_CLR位也被清零。

\- int\_asserted，第31位，该位只读，复位值为0。当Capric卡向处理器提交中断请求时，该位由硬件逻辑置1。对此位进行写操作无意义。在逻辑实现中，int\_asserted = int\_src\_rd & int\_srcWr。

# 12.1.2 Capric卡的初始化

Capric卡在初始化时需要进行配置寄存器空间和Capric卡硬件逻辑的初始化。其中配置寄存器空间的初始化由软硬件联合完成。

Capric卡的设计基于Xilinx公司的LogiCORE。因此Capric卡需要使用Xilinx公司提供的“CORE Generator GUI”对LogiCORE进行基本的初始化，并设置一些必要的参数，包括Vendor ID、Device ID、Revision ID、Subsystem Vendor ID和Subsystem ID等参数。有关该工具的使用见［LogiCORE（tm）Endpoint PIPE v1.7]，本节对此不做进一步描述。Capric卡的配置寄存器空间的初始值如下所示。

- Vendor ID 为 0x10EE，Xilinx 使用的 Vendor ID。  
- Device ID 为 0x0007, LogiCORE 使用的 Device ID。  
- Revision ID 为 $0 \times 00$ 。  
- Subsystem ID 为 $0 \times 10 \mathrm{EE}$   
- Device ID 为 0x0007。  
- Base Class 为 $0 \times 05$ , 表示 Capric 卡为 “类存储器控制器”。  
- Sub Class 和 Interface 为 $0 \times 00$ , 进一步描述 Capric 卡为 RAM 控制器。  
- Card CIS Pointer 为 $0 \times 00$ , 表示不支持 Card Bus 接口。  
- BAR0为0xFFFF00。Capric卡仅支持BARO空间，该空间采用32位存储器映像寻址，其大小为 $256\mathrm{B}$ ，而且不支持预读。在初始化时，BARO寄存器存放该空间所需要的存储器空间大小，该寄存器由系统软件读取后，再写入一个新的数值。这个数值为BARO空间使用的基地址。  
- Max\_Payload\_SizeSupported参数为0b010，即Max\_Payload\_SizeSupported参数的最大值为512B。多数RC支持的Max\_Payload\_SizeSupported参数仅为128B或者256B。因此LogiCORE支持512B已经足够了。在Capric卡的初始化阶段，需要与对端设备进行协商，确认Max\_Payload\_Size参数的值，如果Capric卡与Intel的Chipset直接相连，该参数为128B或者256B。Capric卡需要根据协商后的Max\_Payload\_Size参数，而不是Max\_Payload\_SizeSupported参数，确定存储器写TLP有效负载的大小。当DMA写的数据区域超过Max\_Payload\_Size参数时，需要进行拆包处理，详见第12.2.1节。  
- Capric 卡不支持 Phantom 功能。即不能使用 Function 号，进一步扩展 Tag 字段。Phantom 功能的详细说明见第 4.3.2 节。  
- Multiple Message Capable 参数为 0b000，即支持一个中断向量。  
- Max\_Read\_Request\_Size 参数为 0b010，即存储器读请求 TLP 一次最多能够从目标设备中读取 512 B 大小的数据。如果 DMA 读的数据区域超过 512 B 时，需要进行拆包处理，详见第 12.2.2 节。

系统软件在Capric卡初始化时，将分析Capric卡的配置空间，并填写Capric卡的配置寄存器空间。值得注意的是，系统软件对Capric卡进行配置时，Capric卡将保留该设备在PCI总线树中的Bus Number、Device Number和Function Number，LogiCORE使用寄存器cfg\_BUS\_number[7:0]、cfg\_device\_number[4:0]和cfg\_function\_number[2:0]存放这组数值，当LogiCORE发起存储器读请求TLP时，需要使用这组数值。

在设备驱动程序中，Capric卡需要执行以下步骤完成硬件初始化。

（1）向DCSR1寄存器的init rst\_o位写1。  
(2）延时 $5\mu \mathrm{s}$ 。  
（3）向DCSR1寄存器的init rst\_o位写0。  
(4) 向 DCSR1 寄存器的 int\_rd\_enb 和 int\_WR\_enb 位写 1，使能 DMA 读写中断请求。

# 12.1.3 DMA写

Capric卡使用DMA写过程将Capric卡SRAM中的数据发送到HOST处理器。在设备驱动程序中，DMA写过程如下所示。

(1) 填写 WR\_DMA\_ADR 寄存器，注意填写的是 PCI 总线域的地址。  
(2) 填写 WR\_DMA\_SIZE 寄存器，以字节为单位。  
(3) 填写 DCSR2 寄存器的 mwr\_start 位，启动 DMA 写。  
(4) 等待 DMA 写完成中断后，结束 DMA 写。如果系统软件屏蔽了 DMA 写完成中断，可以通过查询 INT\_REG 寄存器的 wr\_done\_CLR 位判断 DMA 写是否已经完成。在 Capric 卡中，上一次 DMA 写操作没有完成之前，不能启动下一次 DMA 写操作。

(5) 最后将 wr\_done\_CLR 位清零。

从硬件设计的角度来看，DMA写过程较为复杂。Capric卡需要通过DMA控制逻辑，组织一个或者多个存储器写TLP，将SRAM中的数据进行封装然后传递给发送部件，再由发送部件将数据传送到LogiCORE，最后由LogiCORE将存储器写TLP传递给RC。

如果一次DMA写所传递的数据超过了 $512\mathrm{B}^{\ominus}$ ，那么DMA控制逻辑需要传递多个存储器写TLP给发送部件，才能完成一次完整的DMA写操作。而且在DMA操作中需要进行数据对界。其详细实现过程见第12.2.1节。

# 12.1.4 DMA读

Capric卡使用DMA读过程将主存储器中的数据读到Capric卡片内SRAM中。在设备驱动程序中，DMA读过程如下所示。

(1) 填写 RD\_DMA\_ADR 寄存器，注意此处填写的是 PCI 总线域的地址。  
(2) 填写 RD\_DMA\_SIZE 寄存器，以字节为单位。  
(3) 填写 DCSR2 寄存器的 mrd\_start 位，启动 DMA 读。  
（4）等待DMA读完成中断产生后，结束DMA读。如果系统软件屏蔽了DMA读完成中断，则系统软件可以通过查询INT\_REG寄存器的rd\_done\_CLR位判断是否DMA读已经完成。在Capric卡中，上一次DMA读操作没有完成之前，不能启动下一次DMA读操作。

(5) 最后将 rd\_done\_CLR 位清零。

从硬件设计的角度来看，DMA读过程比DMA写过程复杂。PCIe总线使用Split方式实现存储器读。Capric卡的1次DMA读操作使用两种TLP报文，并通过发送部件和接收部件协调完成。

(1) 首先 DMA 控制逻辑组织一个或者多个存储器读请求 TLP, 然后由发送部件将存储器读请求 TLP 传递给 RC。  
(2) RC 正确接收到这个请求报文后，使用一个或者多个存储器读完成 TLP 将数据传递给 Capric 卡。  
(3) Capric 卡接收逻辑从 RC 中获得这些存储器读完成 TLP 时，需要首先处理乱序，之后完成一个 DMA 读操作，并向 RC 提交中断请求。

如果一次DMA读请求的数据大于 $512\mathrm{B}^{\ominus}$ 时，DMA控制逻辑需要发送多个存储器读请求TLP给RC，而且在DMA读操作中需要进行数据对界。尤其值得注意的是这几个TLP的Tag字段不能相同，为此硬件逻辑必须正确维护存储器读请求使用的Tag字段。DMA读操作的详细实现过程见第12.2.2节。

# 12.1.5 中断请求

Capric 卡使用 MSI 机制提交中断请求，并只使用了一个中断向量处理 DMA 读/写完成和错误处理。本章为简便起见，忽略了错误处理的过程，但是在一个实际的设计中，错误处理及恢复过程非常重要。

当DCSR1寄存器的int\_rd\_enb和int\_WR\_enb位为1，而且int\_WR\_msk和int\_rd\_msk不为1时，DMA读写完成后，Capric卡将向处理器提交中断请求。当DMA读写完成后，硬件逻辑将INT\_REG寄存器的int\_asserted位置为1，表示有中断请求。此时系统软件需要进一步查询INT\_REG寄存器的int\_src\_rd和int\_src\_WR位，判断该中断请求为DMA读完成还是DMA写完成，其步骤如下。

（1）如果int\_asserted位为1，表示Capric卡提交了一个中断请求；否则转(6)。  
(2）int\_src\_rd位为1，表示Capric卡提交了一个DMA读完成中断请求，否则转（4）。此时rd\_done\_CLR位也应该为1。  
(3) 进行 DMA 读完成处理。向 rd\_done\_CLR 位写 1，清除 DMA 读完成请求位，转 (6)。  
(4) 如果 int\_src\_WR 位为 1, 表示 Capric 卡提交了一个 DMA 写完成中断请求。此时 wr\_done\_CLR 位也应该为 1。  
(5) 进行 DMA 写完成处理。向 wr\_done\_CLR 位写 1，清除 DMA 读完成请求位。

# (6) 结束。

以上过程仅为一个简单的中断服务例程的执行流程，一个具体设备驱动程序在 DMA 读写完成后，将检查一些返回状态，以确定 DMA 读写是否正确结束。

# 12.2 Capric卡的数据传递

本节主要介绍系统软件在启动 DMA 读写操作时，硬件逻辑的工作过程，包括软件启动 DMA 写时，Capric 卡如何向 RC 发送存储器写 TLP；软件启动 DMA 读时，Capric 卡如何向

RC发送存储器读请求TLP，以及Capric卡如何接收来自RC的存储器读完成TLP。

如果考虑DMA读写操作的数据对界，DMA读写操作的实现较为复杂。为此本节定义了两种操作处理对界问题。

# （1）向前X字节对界

向前对界操作使用 $\mathrm{Head}_{\mathrm{X}}$ （Y）函数，其中 Y 参数对应某个物理地址；而 X 参数对应对界单位，其值必须为 2 的幂。该函数的计算方法如公式 12-1 所示。

$$
\operatorname {H e a d} _ {X} (Y) = Y - (Y \bmod X) \tag {12-1}
$$

由以上公式，可以得出 $\mathrm{Head}_4(0\mathrm{x}1000) = 0\mathrm{x}1000$ ，而 $\mathrm{Head}_4(0\mathrm{x}1007) = 0\mathrm{x}1004$ 。该操作非常适合硬件实现，在硬件实现中 $\mathrm{Head}_{\mathrm{X}}(\mathrm{Y}) = \mathrm{Y}_{\mathrm{n}}\mathrm{Y}_{\mathrm{n - 1}}\dots \mathrm{Y}_{\mathrm{m}}0_{\mathrm{m - 1}}0_{\mathrm{m - 2}}\dots 0_{\mathrm{l}}0_{\mathrm{0}}$ 。如果Y的长度为32b，则 $\mathbf{n}$ 等于31，而 $\mathrm{m}$ 等于 $\operatorname {Log}_2(\mathrm{X})$ 。因此在硬件逻辑中，只要将Y的第 $0\sim \mathrm{m} - 1$ 位清零即可。

# (2) 向后 X 字节对界

向后对界操作使用 $\mathrm{Tail}_{\mathrm{X}}(\mathrm{Y})$ 函数，其中 $\mathrm{Y}$ 参数对应某个物理地址；而 $\mathrm{X}$ 参数对应对界单位，其值必须为2的幂。该函数的计算方法如公式12-2所示。

$$
\operatorname {T a i l} _ {X} (Y) = \operatorname {H e a d} _ {X} (Y) + X - 1 \tag {12-2}
$$

由以上公式，可以得出 $\mathrm{Tail}_4(0\mathrm{x}1000) = 0\mathrm{x}1003$ ，而 $\mathrm{Tail}_4(0\mathrm{x}1007) = 0\mathrm{x}1007$ 。该操作也非常适合硬件实现，在硬件实现中 $\mathrm{Tail}_{\mathrm{X}}(\mathrm{Y}) = \mathrm{Y}_{\mathrm{n}}\mathrm{Y}_{\mathrm{n - 1}}\dots \mathrm{Y}_{\mathrm{m}}1_{\mathrm{m - 1}}1_{\mathrm{m - 2}}\dots 1_{\mathrm{l}}1_{\mathrm{0}}$ 。如果 $\mathrm{Y}$ 的长度为 $32\mathrm{b}$ ，则 $\mathbf{n}$ 等于31，而 $\mathrm{m}$ 等于 $\operatorname {Log}_2(\mathrm{X})$ 。因此在硬件逻辑中，只要将 $\mathrm{Y}$ 的第 $0\sim \mathrm{m} - 1$ 位置1即可。

# 12.2.1 DMA写使用的TLP

当软件启动DMA写过程后，DMA控制逻辑将组织存储器写TLP发送给RC。PCIe总线使用Posted总线事务发送存储器写TLP。Capric卡使用4DW长度的TLP头，即使用64位地址编码格式。存储器写TLP由一个通用TLP头加上若干数据字段组成，存储器写TLP格式如图12-2所示。

![[pci_express/d8f193da82e4aeaf8e8b5413ee8e6fd5b27f08e819a6a6d0eee2d6e3803716f7.jpg]]  
图12-2 存储器写请求TLP

# 1. DMA写操作使用的实际长度

DMA写逻辑首先从WR\_DMA\_ADR寄存器中获得起始地址 $\mathrm{A(A_{31}A_{30}\dots A_1A_0)}$ ，然后从WR\_DMA\_SIZE寄存器中获得传送长度 $\mathrm{L(L_{15}L_{14}\dots L_1L_0)}$ ，并由A和L，通过计算获得本次DMA写的结束地址 $\mathrm{B(B_{31}B_{30}\dots B_1B_0)}$ ，其值如公式12-3所示。

$$
B = A + L - 1 \tag {12-3}
$$

系统软件需要保证B的计算结果不会出现进位，而DMA写逻辑由公式12-3获得本次数据传送的地址范围 $\mathrm{A}\sim \mathrm{B}$ 。在存储器读写TLP中，Length字段以DW为单位，因此向 $\mathrm{A}\sim \mathrm{B}$ 这段数据区域进行DMA写时，Capric卡实际上需要向 $\mathrm{Head}_4(\mathrm{A})\sim \mathrm{Tail}_4(\mathrm{B})$ 这段数据区域进行DMA写操作，同时使用TLP的FirstDWBE和LastDWBE字段进行对界处理。

如果一次DMA写操作向 $0\mathrm{x} \mathrm{FFF}0 - 0003 \sim 0\mathrm{x} \mathrm{FFF}0 - 0200$ 这段区域传送数据时，虽然这段数据区域的长度为 $0\mathrm{x}1\mathrm{FE}$ 字节，但是由于TLP的Length字段以DW为基本单位，因此Capric卡需要向 $0\mathrm{x} \mathrm{FFF}0 - 0000 \sim 0\mathrm{x} \mathrm{FFF}0 - 0203$ 这段区域进行写操作，然后通过First DW BE和Last DW BE字段屏蔽 $0\mathrm{x} \mathrm{FFF}0 - 0000 \sim 0\mathrm{x} \mathrm{FFF}0 - 0002$ 和 $0\mathrm{x} \mathrm{FFF}0 - 0201 \sim 0\mathrm{x} \mathrm{FFF}0 - 0203$ 这两段数据区域。

因此在L中存放的长度（A\~B数据区域的长度）并不是该TLP使用的实际长度。在该TLP中使用的实际长度为 $\mathrm{Head}_4(\mathrm{A})\sim \mathrm{Tail}_4(\mathrm{B})$ 这段数据区域的长度。本节使用M（ $\mathbf{M}_{15}\mathbf{M}_{14}$ $\dots \mathrm{M}_1\mathrm{M}_0$ ）保存TLP中使用的实际长度，其值如公式12-4所示。

$$
M = \left(\operatorname {T a i l} _ {4} (B) - \operatorname {H e a d} _ {4} (A) + 1\right) \gg 2 \tag {12-4}
$$

使用以上公式可以较为方便地得出DMA写操作使用的实际长度，但是公式12-3和公式12-4中使用了多个32位的加法器，非常耗费FPGA的内部资源。为此本次设计使用了另外一种算法计算M的值，如公式12-5所示。

$$
\begin{array}{l} M ^ {\prime} = \left(H e a d _ {4} \left(0 0 A _ {1} A _ {0} + 0 0 L _ {1} L _ {0} + 3\right) - H e a d _ {4} \left(0 0 A _ {1} A _ {0}\right)\right) \\ M = L \gg 2 + M ^ {\prime} \gg 2 \tag {12-5} \\ \end{array}
$$

我们可以使用公式12-5计算 $0\mathrm{xFF0} - 0003 \sim 0\mathrm{FFF0} - 0200$ 这段区域使用的实际长度M。在这段区域中，A为 $0\mathrm{xFF0} - 0003$ ，而 $\mathrm{L} = 0\mathrm{x}1\mathrm{FE}$ ，因此 $\mathbf{M}'$ 等于8，而 $\mathrm{M} = 0\mathrm{x}7\mathrm{F} + 0\mathrm{x}2 = 0\mathrm{x}81$ 。该结果与公式12-4计算结果相等。但是在公式12-5中， $\mathbf{M}'$ 的计算仅使用4位加法器，其实现代价比公式12-4所耗费的资源少得多，更为重要的是计算速度也比公式12-4快得多。

在 LogiCORE 中，Max\_Payload\_Size Supported 参数的最大值为 $512 \mathrm{~B}$ 。但是链路两端经过协商后，实际确认的 Max\_Payload\_Size 参数可能小于 $512 \mathrm{~B}$ ，在多数 x86 处理器系统中，该参数为 128B，因此下文假设 Max\_Payload\_Size 参数为 $128 \mathrm{~B}$ 。

当M大于0x20（即数据区域的实际长度超过128B）时，Capric卡进行DMA写时需要发送多个存储器写请求TLP，而M小于或等于0x20时仅需要发送1个存储器写请求TLP。下文分别讨论这两种情况。

# 2. M小于或等于0x20

Capric卡向数据区域 $\left[\mathrm{A}_{31} \mathrm{~A}_{30} \ldots \mathrm{A}_1 \mathrm{~A}_0 \sim \mathrm{B}_{31} \mathrm{~B}_{30} \ldots \mathrm{B}_1 \mathrm{~B}_0\right]$ 进行DMA写操作时，如果通过公式12-5的计算发现M小于或等于 $0 \times 20$ ，DMA控制逻辑将组织1个或者2个存储器写

TLP传递给LogiCORE。

如果这个 TLP 所传递的数据区域跨越了 4KB 边界，将组织 2 个存储器写 TLP，因为 PCIe 总线规定被传送的数据区域不能跨越 4KB 边界；如果没有跨越 4KB 边界，Capric 卡组织 1 个存储器写 TLP。我们首先讨论这段数据区域没有跨越 4KB 边界的情况，此时这个存储器写 TLP 的各个字段如下所示。

- Fmt 字段为 0b10 或者 0b11，表示使用 3DW 或者 4DW 的 TLP 头，而且带有数据。在 Capric 中，Fmt 字段为 0b011。  
- Type字段为0b00000，表示当前TLP为存储器写TLP。  
- TC字段为0b000，表示传送类型为TCO。  
- TD 位为 0b0，表示当前 TLP 不含有 ECRC 信息。  
- EP 位为 0b0，表示当前 TLP 是正常的，没有出现完整性问题。  
- Attr 字段为 0b00，表示当前 TLP 不使用 Relaxed Ordering，由硬件完成 Cache 一致性操作。有关 Cache 一致性的处理见第 3.3 节和第 12.3.6 节，而 Relaxed Ordering 的描述见第 11.4 节。  
- AT字段为0b00，表示不进行地址转换。  
- Length 字段由公式 12-5 计算而来，其值与 M 相等，单位为 DW，最大值为 0x20，即 $128 \mathrm{~B}$ 。假定在 Capric 卡中，Max\_Payload\_Size 参数为 $128 \mathrm{~B}$ 。在 x86 处理器系统中，多数 RC 的 Max\_Payload\_Size 参数为 $128 \mathrm{~B}$ 。  
- Address 字段为 $\mathrm{A}_{31} \mathrm{~A}_{30} \ldots \mathrm{A}_{2}$ 。Address 字段为 DW 对界的，共由 30 位组成。  
- 当 M 不等于 1 时, Last DW BE 字段与 TLP 报文的结束地址有关, 更准确地讲, 与 $\mathrm{B}_{1}$ 和 $\mathrm{B}_{0}$ 位有关, 如下所示。

$\mathrm{B_1B_0 = 0b11}$ ，则LastDWBE字段为 $0\mathrm{x}1111$

$\mathrm{B_1B_0 = 0b10}$ ，则LastDWBE字段为 $0\mathrm{x}0111$

$\mathrm{B_1B_0 = 0b01}$ ，则LastDWBE字段为 $0\mathrm{x}0011$

$\mathrm{B_1B_0 = 0b00}$ ，则LastDWBE字段为 $0\mathrm{x}0001$

\- 当 M 不等于 1 时, First DW BE 字段与 TLP 报文的起始地址有关, 更准确地讲, 与 $\mathrm{A}_{\mathrm{P}}$ 和 $\mathrm{A}_{0}$ 位有关, 如下所示。

$\mathrm{A_1A_0 = 0b00}$ ，则FirstDWBE字段为 $0\mathrm{x}1111$

$\mathrm{A_1A_0 = 0b01}$ ，则FirstDWBE字段为 $0\mathrm{x}1110$

$\mathrm{A_1A_0 = 0b10}$ ，则FirstDWBE字段为 $0\mathrm{x}1100$

$\mathrm{A_1A_0 = 0b11}$ ，则FirstDWBE字段为 $0\mathrm{x}1000$

当M等于1时，LastDWBE字段必须为0b0000；FirstDWBE字段的计算与 $\mathrm{A}_1\mathrm{A}_0$ 和 $\mathrm{B_1B_0}$ 相关，其中（FirstDWBE） $\mathrm{A}_{1}\mathrm{A}_{0}\sim$ （FirstDWBE） $\mathrm{B_1B_0}$ 字段为1，其他位为0。这两个字段的关系如表12-2所示。

表 12-2 First DW BE 与 A1A0, B1B0 之间的关系

<table><tr><td>A1A0</td><td>B1B0</td><td>First DW BE</td><td>Last DW BE</td></tr><tr><td>0b00</td><td>0b00</td><td>0b0001</td><td>0b0000</td></tr><tr><td>0b00</td><td>0b01</td><td>0b0011</td><td>0b0000</td></tr><tr><td>0b00</td><td>0b10</td><td>0b0111</td><td>0b0000</td></tr><tr><td>0b00</td><td>0b11</td><td>0b1111</td><td>0b0000</td></tr><tr><td>0b01</td><td>0b01</td><td>0b0010</td><td>0b0000</td></tr><tr><td>0b01</td><td>0b10</td><td>0b0110</td><td>0b0000</td></tr><tr><td>0b01</td><td>0b11</td><td>0b1110</td><td>0b0000</td></tr><tr><td>0b10</td><td>0b10</td><td>0b0100</td><td>0b0000</td></tr><tr><td>0b10</td><td>0b11</td><td>0b1100</td><td>0b0000</td></tr><tr><td>0b11</td><td>0b11</td><td>0b1000</td><td>0b0000</td></tr></table>

值得注意的是，当M小于或等于0x20时，TLP所传递的报文，其数据区域依然可能会跨越4KB边界。如Capric卡向0xFFFF-0FFF\~0xFFFF-1000数据区域进行DMA写时，虽然M等于0x2（实际长度只有两个字节），但是Capric卡需要使用两个存储器写请求TLP，分别向0xFFFF-0FFF\~0xFFFF-00-0FFF和0xFFFF-1000\~0xFFFF-1000这两段数据进行写操作。

由此可以发现，当Capric卡对A\~B这段数据区域进行DMA写时，首先需要判断这段区域是否跨越4KB边界。如果跨越则需要向A\~Tail4096(A)和Head4096(B)\~B这两段数据区域进行写操作，这两段数据区域一定都小于0x20，因此采用上文描述的方法组织TLP报文即可。

# 3. M 大于 0x20

如果M大于0x20，此时Capric卡进行一次DMA写操作时，LogiCORE需要向RC发送多个存储器写请求TLP。我们假设Capric卡需要向 $\left[\mathrm{A}_{31}\mathrm{A}_{30}\dots \mathrm{A}_1\mathrm{A}_0\sim \mathrm{B}_{31}\mathrm{B}_{30}\dots \mathrm{B}_1\mathrm{B}_0\right]$ 这段数据区域进行DMA写操作，而且这段数据区域的M大于 $0\mathrm{x}20$ 。此时DMA写逻辑需要进行拆包操作。这个拆包操作需要遵循以下原则。

