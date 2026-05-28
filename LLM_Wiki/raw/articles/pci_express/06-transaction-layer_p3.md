---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "06"
section: "6.3.6 TLP Processing Hint"
part: 3
tags: [pci, pci-express, computer-architecture]
---
# 6.3.6 TLP Processing Hint

当 TLP 的 TH 位为 1 时，表示在当前 TLP 中包含 Processing Hint 字段，PH 字段由 PCIe V2.1 总线规范引入。该字段的引入可以使目标设备根据源设备对数据的使用情况，合理地安排数据缓冲，从而降低 PCIe 设备的访问延时，并最大化地利用 PCIe 设备中的数据缓冲。

ProcessingHint字段的产生与智能设备的大量涌现密切相关。在智能设备中，含有一个运算能力相当强的处理器。智能设备与处理器之间的数据交换，实质上等效于两个处理器系统之间的数据传递。有些智能设备，如在显卡中使用的GPU（GraphicProcessingUnit）和GP-GPU（GerneralPurposeGPU）的处理能力甚至超过多数通用处理器。智能设备与处理器系统可以采用图6-14所示的拓扑结构连接。

![[pci_express/ed99a61808c1b4b6477d3868865bfd630b433d3f3c24c84f279cdad16b2420df.jpg]]  
图6-14 智能外设与处理器系统的连接

在这种处理器系统中，内部互连网络处于核心地位，上图所示的网络是一个理想的全互连结构。在这种互连结构中，处理器、存储器和智能设备与网络节点相连。在这种结构中，在网络节点上连接的设备都含有一个处理器，包括存储器。

当智能设备与处理器进行通信时，如果能够预知数据的使用情况，无疑能够减低数据的访问延时。如智能设备1将一组数据写入智能存储器1之后，如果这个智能存储器1能够预测这组数据将很快被再次使用，则可以将这组数据放入到高速缓冲中，而不必放入低速缓冲中。当这组数据被再次使用时，智能存储器1可以很快将数据从高速缓冲中读出，从而缩短了访问延时。

PCIe总线引入了PH字段的主要目的是为了加速外部设备访问主存储器，即DMA操作，同时也兼顾SMP处理器系统对PCIe设备的访问。但是PH字段仍然没有完全包含上述模型的所有内容，因为在上述模型中，智能存储器是独立与PCIe体系结构的RC的，而且智能外设之间具有独立连接通路，在这种模型中，芯片内部的互连网络是真正的设计核心。目前在P4080处理器和Boxboro-EX处理器系统中，具有全互连网络结构。

# 1. PH字段

PCIe总线使用PH字段，使智能设备或者处理器提前预知数据的使用方法。PH字段仅在与存储器访问相关的TLP中出现，该字段由两位组成，在TLP中的位置如图6-8所示，其详细说明如表6-8所示。

表 6-8 PH 字段

<table><tr><td>PH [1:0]</td><td>Processing Hint</td><td>描述</td></tr><tr><td>00</td><td>双向数据结构</td><td>表示该 TLP 中的数据，经常被源设备和目标设备使用</td></tr><tr><td>01</td><td>Requester</td><td>表示该 TLP 中的数据，经常被源设备使用</td></tr><tr><td>10</td><td>Target</td><td>表示该 TLP 中的数据，经常被目标设备使用</td></tr><tr><td>11</td><td>Target with Priority</td><td>与“Target”类似，但是级别更高</td></tr></table>

当PH[1:0]为0b01时，表示TLP中的数据经常被设备使用，包括以下四种类型。

\- DWDW（Device Write after Device Write）。外部设备对一段数据进行写操作后，很快

还会再次进行写操作。

- DWDR（Device Read after Device Write）。外部设备对一段数据进行写操作后，很快还会对这段数据进行读操作。  
- DRDW（Device Write after Device Read）。外部设备对一段数据进行读操作后，很快还会对这段数据进行写操作。  
- DRDR（Device Read after Device Read）。外部设备对一段数据进行读操作后，很快还会再次进行读操作。

当PH[1:0]为0b10时，表示TLP中的数据经常被目标设备使用，包括以下两种类型。在进行DMA操作时，HOST处理器为目标设备，本节也以此为例说明PH字段的使用规则。

- DWHR（Host Read after Device Write）。外部设备对一段数据进行写操作后，Host 处理器将很快读取这段数据。  
- HWDR（Device Read after Host Write）。Host 处理器对一段数据进行写操作后，外部设备将很快读取这段数据。

# 2. Steering Tag

通过上文对PH字段的描述，可以发现PH字段提供的ProcessingHint控制能力较弱，仅是一个粗颗粒的控制机制。为此PCIe总线规范提供了一个16位的ST（Steering Tag）字段，目标设备通过TLP中的Steering Tag字段可以获得较为详细的信息。

当TH位有效时，存储器写请求TLP的Tag字段被重新定义为ST[7:0]字段，因为存储器写请求并不需要Tag字段；而对于存储器读请求TLP，TH位有效时DWBE字段被重新定义为ST[7:0]字段，由于一些存储器读请求TLP仍然需要使用DWBE字段处理对界，因此ST字段只能应用于不需要对界的存储器读请求TLP。

当TH位有效时，这些“不需要对界”的存储器读请求TLP，将使用默认的DWBE值。如果该存储器读请求TLP的Length字段为1时，FirstDWBE字段的默认值为0b1111，而LastDWBE字段的默认值为0b0000；如果Length字段不为1，First和LastDWBE的默认值都为0b1111。

TLP 还可以支持 ST [15:8] 字段，该字段是 ST 的扩展字段。如果一个 TLP 需要使用 ST [15:8] 字段，必须使用 TLP Prefix，因为在 TLP 头中已经没有足够的空间放置这些字段。该 TLP Prefix 也被称为 TPH TLP Prefix，其格式如图 6-15 所示。

![[pci_express/438af888f7cdda81ccbbcf066862914e9a3fa50a853ac586a9c925e0937b5980.jpg]]  
图6-15 TPH TLP Prefix格式

如上图所示，在TPH TLP Prefix的Byte 1中存放ST[15:8]字段。在PCIe总线中，ST[15:8]字段是可选的，实际上整个ST字段都是可选的。TPH Requester Capability结构使用ST Mode Select字段定义了ST字段的三种使用模式。

\- 当该字段为 0b000 时，表示当前 PCIe 设备不支持 ST 字段，此时 TLP 的 ST 字段为

全0。

- 当该字段为 0b001 时，表示 ST 模式为 “Interrupt Vector Mode”。此时 TLP 的 ST 字段由 MSI/MSI-X 的中断向量号确定。  
- 当该字段为 0b010 时，表示 ST 字段由 PCIe 设备决定。

PCIe 设备可以根据 TLP 的属性，决定 ST 字段的值，为此在一个 PCIe 设备中，将使用 ST 表，存放这个设备使用的所有 ST 字段。这个 ST 表可以存放在 TPH Requester Capability 结构中，也可以存放在 MSI-X 表中。实际上 ST 表的存放位置并不重要，只要 PCIe 设备能够根据发出的 TLP 类型，选择合适的 ST 字段即可。

目前尚无支持ST字段的PCIe设备，这些PCIe设备发出的TLP中都不包含ST字段。而从PCIe总线规范V2.1中，可以发现MSI/MSI-X中断请求可以使用ST字段，当PCIe设备使用MSI或者MSI-X中断请求时，可以根据中断向量的不同，从ST表中MSI报文选择合适的ST字段，然后再发向处理器系统。处理器系统收到这个MSI报文后，可以根据ST字段的不同，分别处理PCIe设备发出的中断请求。

综上所述，ST字段的主要目的是细分TLP的属性，处理器系统可以使用该字段优化数据缓冲，减小数据访问延时。ST字段的支持需要多个PCIe设备共同参与。如EP进行DMA写操作时，数据将发向RC，RC和EP都需要具有解释这个TLP所携带的ST字段的能力。

因此处理器系统在初始化PCIe设备时，需要合理地设置该设备的ST表。目前尚无支持TPH位和ST字段的PCIe设备，但是这种方法可以有效降低PCIe设备访问存储器，以及PCIe设备间数据访问的延时，从而提高PCIe链路的利用率。

# 6.4 TLP中与数据负载相关的参数

在PCIe总线中，有些TLP含有DataPayload，如存储器写请求、存储器读完成TLP等。在PCIe总线中，TLP含有的DataPayload大小与Max\_Payload\_Size、Max\_Read\_Request\_Size和RCB参数相关。下面将分别介绍这些参数的使用。

# 6.4.1 Max\_Payload\_Size 参数

PCIe总线规定在TLP报文中，数据有效负载的最大值为4KB，但是PCIe设备并不一定能够发送这么大的数据报文。PCIe设备含有“Max\_Payload\_Size”和“Max\_Payload\_SizeSupported”参数，这两个参数分别在Device Capability寄存器和Device Control寄存器中定义，这两个寄存器在PCIExpressCapability结构中的位置见第4.3.2节。

“Max\_Payload\_SizeSupported”参数存放一个PCIe设备中TLP有效负载的最大值，该参数由PCIe设备的硬件逻辑确定，系统软件不能改写该参数。而Max\_Payload\_Size参数存放PCIe设备实际使用的TLP有效负载的最大值。该参数由PCIe链路两端的设备协商决定，是PCIe设备进行数据传送时实际使用的参数。

PCIe设备发送数据报文时，使用Max\_Payload\_Size参数决定TLP的最大有效负载。当

PCIe设备的所传送的数据大小超过Max\_Payload\_Size参数时，这段数据将被分割为多个TLP进行发送。当PCIe设备接收TLP时，该TLP的最大有效负载也不能超过Max\_Payload\_Size参数，如果接收的TLP，其Length字段超过Max\_Payload\_Size参数，该PCIe设备将认为该TLP非法。

RC或者EP在发送存储器读完成TLP时，这个存储器读完成TLP的最大Payload也不能超过Max\_Payload\_Size参数，如果超过该参数，PCIe设备需要发送多个读完成报文。值得注意的是，这些读完成报文需要满足RCB参数的要求，有关RCB参数的详细说明见下文。

在实际应用中，尽管有些PCIe设备的Max\_Payload\_SizeSupported参数可以为256B、512B、1024B或者更高，但是如果PCIe链路的对端设备可以支持的Max\_Payload\_Size参数为128B时，系统软件将使用对端设备的Max\_Payload\_SizeSupported参数，初始化该设备的Max\_Payload\_Size参数，即选用PCIe链路两端最小的Max\_Payload\_SizeSupported参数初始化Max\_Payload\_Size参数。

在多数x86处理器系统的MCH或者ICH中，Max\_Payload\_SizeSupported参数为 $128\mathrm{B}$ 。这也意味着在x86处理器中，与MCH或者ICH直接相连的PCIe设备进行DMA读写时，数据的有效负载不能超过 $128\mathrm{B}$ ，同时读完成携带的Payload也不能超过 $128\mathrm{B}$ 。而在PowerPC处理器系统中，该参数大多为 $256\mathrm{B}$ 。

目前在大多数EP中，Max\_Payload\_SizeSupported参数不大于 $512\mathrm{B}$ ，因为在大多数处理器系统的RC中，Max\_Payload\_SizeSupported参数也不大于 $512\mathrm{B}$ 。因此即便EP支持较大的Max\_Payload\_SizeSupported参数，并不会提高数据传送效率。

而Max\_Payload\_Size参数的大小与PCIe链路的传送效率成正比，该参数越大，PCIe链路带宽的利用率越高，该参数越小，PCIe链路带宽的利用率越低。

PCIe总线规范规定，对于实时性要求较高的PCIe设备，Max\_Payload\_Size参数不应设置过大，因此这个参数有时会低于PCIe链路允许使用的最大值。

# 6.4.2 Max\_Read\_Request\_Size 参数

Max\_Read\_Request\_Size 参数由 PCIe 设备决定，该参数规定了 PCIe 设备一次能从目标设备读取多少数据。

Max\_Read\_Request\_Size 参数在 Device Control 寄存器中定义，详见第 4.3.2 节。该参数与存储器读请求 TLP 的 Length 字段相关，其中 Length 字段不能大于 Max\_Read\_Request\_Size 参数。在存储器读请求 TLP 中，Length 字段表示需要从目标设备读取多少数据。

值得注意的是，Max\_Read\_Request\_Size 参数与 Max\_Payload\_Size 参数间没有直接联系，Max\_Payload\_Size 参数仅与存储器写请求和存储器读完成报文相关。

PCIe总线规定存储器读请求，其读取的数据长度不能超过Max\_Read\_Request\_Size参数，即存储器读TLP中的Length字段不能大于这个参数。如果一次存储器读操作需要读取的数据范围大于Max\_Read\_Request\_Size参数时，该PCIe设备需要向目标设备发送多个存储器读请求TLP。

PCIe总线规定Max\_Read\_Request\_Size参数的最大值为4KB，但是系统软件需要根据硬

件特性决定该参数的值。因为PCIe总线规定EP在进行存储器读请求时，需要具有足够大的缓冲接收来自目标设备的数据。

如果一个EP的Max\_Read\_Request\_Size参数被设置为4KB，而且这个EP每发出一个4KB大小存储器读请求时，EP都需要准备一个4KB大小的缓冲。这对于绝大多数EP都是一个相当苛刻的条件。为此在实际设计中，一个EP会对Max\_Read\_Request\_Size参数的大小进行限制。

# 6.4.3 RCB参数

RCB位在Link Control寄存器中定义，见第4.3.2节。RCB位决定了RCB参数的值，在PCIe总线中，RCB参数的大小为64B或者128B，如果一个PCIe设备没有设置RCB的大小，则RC的RCB参数缺省值为64B，而其他PCIe设备的RCB参数的缺省值为128B。PCIe总线规定RC的RCB参数的值为64B或者128B，其他PCIe设备的RCB参数为128B。

在PCIe总线中，一个存储器读请求TLP可能收到目标设备发出的多个完成报文后，才能完成一次存储器读操作。因为在PCIe总线中，一个存储器读请求最多可以请求4KB大小的数据报文，而目标设备可能会使用多个存储器读完成TLP才能将数据传递完毕。

当一个EP向RC或者其他EP读取数据时，这个EP首先向RC或者其他EP发送存储器读请求TLP；之后由RC或者其他EP发送存储器读完成TLP，将数据传递给这个EP。

如果存储器读完成报文所传递数据的地址范围没有跨越RCB参数的边界，那么数据发送端只能使用一个存储器完成报文将数据传递给请求方，否则可以使用多个存储器读完成TLP。

假定一个EP向地址范围为 $0\mathrm{xFFFF - }0000\sim 0\mathrm{xFFFF - }0010$ 的这段区域进行DMA读操作，RC收到这个存储器读请求TLP后，将组织存储器读完成TLP，由于这段区域并没有跨越RCB边界，因此RC只能使用一个存储器读完成TLP完成数据传递。

如果存储器读完成报文所传递数据的地址范围跨越了RCB边界，那么数据发送端（目标设备）可以使用一个或者多个完成报文进行数据传递。数据发送端使用多个存储器读完成报文完成数据传递时，需要遵循以下原则。

- 第一个完成报文所传送的数据，其起始地址与要求的起始地址相同。其结束地址或者为要求的结束地址（使用一个完成报文传递所有数据），或者为RCB参数的整数倍（使用多个完成报文传递数据）。  
- 最后一个完成报文的起始地址或者为要求的起始地址（使用一个完成报文传递所有数据），或者为RCB参数的整数倍（使用多个完成报文传递数据）。其结束地址必须为要求的结束地址。  
- 中间的完成报文的起始地址和结束地址必须为RCB参数的整数倍。当RC或者EP需要使用多个存储器读完成报文将0xFFFE-FFF0～0xFFFF-00C7之间的

数据发送给数据请求方时，可以将这些完成报文按照表6-9方式组织。

表 6-9 存储器读完成报文的拆分方法

<table><tr><td>方式1</td><td>方式2</td><td>方式3</td></tr><tr><td>0xFFFF-FFF0 ~ 0xFFFF-FFFF</td><td>0xFFFF-FFF0 ~ 0xFFFF-FFFF</td><td>0xFFFF-FFF0 ~ 0xFFFF-FFFF</td></tr><tr><td>0xFFFF-0000 ~ 0xFFFF-003F</td><td>0xFFFF-0000 ~ 0xFFFF-007F</td><td>0xFFFF-0000 ~ 0xFFFF-00C7</td></tr><tr><td>0xFFFF-0040 ~ 0xFFFF-007F</td><td>0xFFFF-0080 ~ 0xFFFF-00C7</td><td></td></tr><tr><td>0xFFFF-0080 ~ 0xFFFF-00BF</td><td></td><td></td></tr><tr><td>0xFFFF-00C0 ~ 0xFFFF-00C7</td><td></td><td></td></tr></table>

上表提供的方式仅供参考，目标设备还可以使用其他拆分方法发送存储器读完成TLP。PCIe总线使用多个完成报文实现一次数据读请求的主要原因是考虑Cache行长度和流量控制。在多数x86处理器系统中，存储器读完成报文的数据长度为一个Cache行，即一次传送 $64\mathrm{B}$ 。除此之外，较短的数据完成报文占用流量控制的资源较少，而且可以有效避免数据拥塞。有关流量控制的内容详见第9章。

# 6.5 小结

本章重点介绍PCIe总线的事务层。在PCIe总线层次结构中，事务层最易理解，同时也与系统软件直接相关。但是事务层的知识较为琐碎，在第12章将结合一个EP的设计实例，进一步说明PCIe总线事务层的具体实现机制。

