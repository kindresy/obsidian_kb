---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "06"
section: "第6章 PCIe总线的事务层"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第6章 PCIe总线的事务层

事务层是PCIe总线层次结构的最高层，该层次将接收PCIe设备核心层的数据请求，并将其转换为PCIe总线事务，PCIe总线使用的这些总线事务在TLP头中定义。PCIe总线继承了PCI/PCI-X总线的大多数总线事务，如存储器读写、I/O读写、配置读写总线事务，并增加了Message总线事务和原子操作等总线事务。

本节重点介绍与数据传送密切相关的总线事务，如存储器、I/O、配置读写总线事务。在PCIe总线中，Non-Posted总线事务分两部分进行，首先是发送端向接收端提交总线读写请求，之后接收端再向发送端发送完成（Completion）报文。PCIe总线使用Split传送方式处理所有Non-Posted总线事务，存储器读、I/O读写和配置读写这些Non-Posted总线事务都使用Split传送方式。PCIe的事务层还支持流量控制和虚通路管理等一系列特性，而PCI总线并不支持这些新的特性。

在PCIe总线中，不同的总线事务采用的路由方式不相同。PCIe总线继承了PCI总线的地址路由和ID路由方式，并添加了“隐式路由”方式。

PCIe总线使用的数据报文首先在事务层中形成，这个数据报文也称为事务层数据报文，即TLP。TLP在经过数据链路层时被加上Sequence Number前缀和CRC后缀，然后发向物理层。

数据链路层还可以产生 DLLP（Data Link Layer Packet）。DLLP 和 TLP 没有直接关系。DLLP 是产生于数据链路层，终止于数据链路层，并不会传递到事务层。DLLP 不是 TLP 加上前缀和后缀形成的。数据链路层的报文 DLLP 通过物理层时，需要经过 $8/10\mathrm{b}$ 编码，然后再进行发送。而数据的接收过程是发送过程的逆过程。

# 6.1 TLP的格式

当处理器或者其他PCIe设备访问PCIe设备时，所传送的数据报文首先通过事务层被封装为一个或者多个TLP，之后才能通过PCIe总线的各个层次发送出去。TLP的基本格式如图6-1所示。

<table><tr><td>TLP Prefix (Optional)</td><td>TLP Prefix (Optional)</td><td>TLP Head</td><td>Data Payload</td><td>TLP Digest(Optional)</td></tr></table>

图6-1 TLP的格式

一个完整的 TLP 由 1 个或者多个 TLP Prefix、TLP 头、Data Payload（数据有效负载）和 TLP Digest 组成。TLP 头是 TLP 最重要的标志，不同的 TLP 其头的定义并不相同。TLP 头包含了当前 TLP 的总线事务类型、路由信息等一系列信息。在一个 TLP 中，Data Payload 的长度可变，最小为 0，最大为 1024DW。

TLP Digest 是一个可选项，一个 TLP 是否需要 TLP Digest 由 TLP 头决定。Data Payload 也是一个可选项，有些 TLP 并不需要 Data Payload，如存储器读请求、配置和 I/O 写完成 TLP 并不需要 Data Payload。

TLP Prefix 由 PCIe V2.1 总线规范引入，分为 Local TLP Prefix 和 EP-EP TLP Prefix 两类。其中 Local TLP Prefix 的主要作用是在 PCIe 链路的两端传递消息，而 EP-EP TLP Prefix 的主要作用是在发送设备和接收设备之间传递消息。设置 TLP Prefix 的主要目的是为了扩展 TLP 头，并以此支持 PCIe V2.1 规范的一些新的功能。

TLP头由3个或者4个双字（DW）组成。其中第一个双字中保存通用TLP头，其他字段与通用TLP头的Type字段相关。一个通用TLP头由Fmt、Type、TC、Length等字段组成，如图6-2所示。

![[pci_express/30ee9101af470ce826f9ac6d5e1cc7ea8918f11594e41afb00f27bae0aa64ffe.jpg]]  
图6-2 通用TLP头格式

如果存储器读写 TLP 支持 64 位地址模式时，TLP 头的长度为 4DW，否则为 3DW。而完成报文的 TLP 头不含有地址信息，使用的 TLP 头长度为 3DW。其中 Byte 4 \~ Byte 15 的格式与 TLP 相关，下文将结合具体的 TLP 介绍这些字段。

# 6.1.1 通用TLP头的Fmt字段和Type字段

Fmt 和 Type 字段确认当前 TLP 使用的总线事务，TLP 头的大小是由 3 个双字还是 4 个双字组成，当前 TLP 是否包含有效负载。其具体含义如表 6-1 所示。

表 6-1 Fmt [1:0] 字段

<table><tr><td>Fmt [2:0]</td><td>TLP 的格式</td></tr><tr><td>0b000</td><td>TLP 大小为 3 个双字，不带数据</td></tr><tr><td>0b001</td><td>TLP 大小为 4 个双字，不带数据</td></tr><tr><td>0b010</td><td>TLP 大小为 3 个双字，带数据</td></tr><tr><td>0b011</td><td>TLP 大小为 4 个双字，带数据</td></tr><tr><td>0b100</td><td>TLP Prefix</td></tr><tr><td>其他</td><td>PCIe 总线保留</td></tr></table>

其中所有读请求 TLP 都不带数据，而写请求 TLP 带数据，而其他 TLP 可能带数据也可能不带数据，如完成报文可能含有数据，也可能仅含有完成标志而并不携带数据。在 TLP 的 Type 字段中存放 TLP 的类型，即 PCIe 总线支持的总线事务。该字段共由 5 位组成，其含

义如表6-2所示。

表 6-2 Type [4:0] 字段

<table><tr><td>TLP 类型</td><td>Fmt [2:0]</td><td>Type [4:0]</td><td>描述</td></tr><tr><td>MRd</td><td>0b0000b001</td><td>0b0 0000</td><td>存储器读请求; TLP 头大小为 3 个或者 4 个双字,不带数据</td></tr><tr><td>MRdLk</td><td>0b0000b001</td><td>0b0 0001</td><td>带锁的存储器读请求; TLP 头大小为 3 个或者 4 个双字,不带数据</td></tr><tr><td>MWr</td><td>0b0100b011</td><td>0b0 0000</td><td>存储器写请求; TLP 头大小为 3 个或者 4 个双字,带数据</td></tr><tr><td>IORd</td><td>0b000</td><td>0b0 0010</td><td>IO 读请求; TLP 头大小为 3 个双字,不带数据</td></tr><tr><td>IOWr</td><td>0b010</td><td>0b0 0010</td><td>IO 写请求; TLP 头大小为 3 个双字,带数据</td></tr><tr><td>CfgRd0</td><td>0b000</td><td>0b0 0100</td><td>配置 0 读请求; TLP 头大小为 3 个双字,不带数据</td></tr><tr><td>CfgWr0</td><td>0b010</td><td>0b0 0100</td><td>配置 0 写请求; TLP 头大小为 3 个双字,带数据</td></tr><tr><td>CfgRd1</td><td>0b000</td><td>0b0 0101</td><td>配置 1 读请求; 不带数据</td></tr><tr><td>CfgWr1</td><td>0b010</td><td>0b0 0101</td><td>配置 1 写请求; 带数据</td></tr><tr><td>TCfgRd</td><td>0b010</td><td>0b1 1011</td><td rowspan="2">本书对这两种总线事务不做介绍</td></tr><tr><td>TCfgWr</td><td>0b001</td><td>0b1 1011</td></tr><tr><td>Msg</td><td>0b001</td><td>0b1 0r2r1r0</td><td>消息请求; TLP 头大小为 4 个双字,不带数据。“rr”字段是消息请求报文的 Route 字段,下文将详细介绍该字段</td></tr><tr><td>MsgD</td><td>0b011</td><td>0b1 0r2r1r0</td><td>消息请求; TLP 头大小为 4 个双字,带数据</td></tr><tr><td>Cpl</td><td>0b000</td><td>0b0 1010</td><td>完成报文; TLP 头大小为 3 个双字,不带数据。包括存储器、配置和 I/O 写完成</td></tr><tr><td>CplD</td><td>0b001</td><td>0b0 1010</td><td>带数据的完成报文, TLP 头大小为 3 个双字,包括存储器读、I/O 读、配置读和原子操作读完成</td></tr><tr><td>CplLk</td><td>0b000</td><td>0b0 1011</td><td>锁定的完成报文,TLP 头大小为 3 个双字,不带数据</td></tr><tr><td>CplDLk</td><td>0b010</td><td>0b0 1011</td><td>带数据的锁定完成报文,TLP 头大小为 3 个双字,带数据</td></tr><tr><td>FetchAdd</td><td>0b0100b011</td><td>0b0 1100</td><td>Fetch and Add 原子操作</td></tr><tr><td>Swap</td><td>0b0100b011</td><td>0b0 1101</td><td>Swap 原子操作</td></tr><tr><td>CAS</td><td>0b0100b011</td><td>0b0 1110</td><td>CAS 原子操作</td></tr><tr><td>LPrfx</td><td>0b100</td><td>0b0 L3L2L1L0</td><td>Local TLP Prefix</td></tr><tr><td>EPrfx</td><td>0b100</td><td>0b1 E3E2E1E0</td><td>End-End TLP Prefix</td></tr></table>

存储器读和写请求，IO读和写请求，及配置读和写请求的type字段相同，如存储器读和写请求的Type字段都为0b0000。此时PCIe总线规范使用Fmt字段区分读写请求，当Fmt字段是“带数据”的报文，一定是“写报文”；当Fmt字段是“不带数据”的报文，一定是“读报文”。

PCIe总线的数据报文传送方式与PCI总线数据传送有类似之处。其中存储器写TLP使用Posted方式进行传送，而其他总线事务使用Non-Posted方式。

PCIe总线规定所有Non-Posted存储器请求使用Split总线方式进行数据传递。当PCIe设备进行存储器读、I/O读写或者配置读写请求时，首先向目标设备发送数据读写请求TLP，当目标设备收到这些读写请求TLP后，将数据和完成信息通过完成报文（Cpl或者CplD）发送给源设备。

其中存储器读、I/O 读和配置读需要使用 CplD 报文，因为目标设备需要将数据传递给源设备；而 I/O 写和配置写需要使用 Cpl 报文，因为目标设备不需要将任何数据传递给源设备，但是需要通知源设备，写操作已经完成，数据已经成功地传递给目标设备。

在PCIe总线中，进行存储器或者I/O写操作时，数据与数据包头一起传递；而进行存储器或者I/O读操作时，源设备首先向目标设备发送读请求TLP，而目标设备在准备好数据后，向源设备发出完成报文。

PCIe总线规范还定义了MRdLk报文，该报文的主要作用是与PCI总线的锁操作相兼容，但是PCIe总线规范并不建议用户使用这种功能，因为使用这种功能将极大影响PCIe总线的数据传送效率。

与PCI总线不同，PCIe总线规范定义了Msg报文，即消息报文，分别为Msg和 $\mathrm{MsgD}$ 。这两种报文的区别在于一个报文可以传递数据，一个不能传递数据。

PCIe V2.1 总线规范还补充了一些总线事务，如 FetchAdd、Swap、CAS、LPrfx 和 EPrfx。其中 LPrfx 和 EPrfx 总线事务分别与 Local TLP Prefix 和 EP-EP TLP Prefix 对应。在 PCIe 总线规范 V2.0 中，TLP 头的大小为 1DW，而使用 LPrfx 和 EPrfx 总线事务可以对 TLP 头进行扩展，本节不对这些 TLP Prefix 做进一步介绍。PCIe 设备可以使用 FetchAdd、Swap 和 CAS 总线事务进行原子操作，第 6.3.5 节将详细介绍该类总线事务。

# 6.1.2 TC字段

TC字段表示当前TLP的传送类型，PCIe总线规定了8种传输类型，分别为 $\mathrm{TC0}\sim \mathrm{TC7}$ 缺省值为TC0，该字段与PCIe的QoS相关。PCIe设备使用TC区分不同类型的数据传递，而多数EP中只含有一个VC，因此这些EP在发送TLP时，也仅仅使用TC0，但是有些对实时性要求较高的EP中，含有可以设置TC字段的寄存器。

在Intel的高精度声卡控制器（High Definition Audio Controller）的扩展配置空间中含有一个TCSEL寄存器。系统软件可以设置该寄存器，使声卡控制器发出的TLP使用合适的TC。声卡控制器可以使用TC7传送一些对实时性要求较强的控制信息，而使用TC0传送一般的数据信息。在具体实现中，一个EP也可以将控制TC字段的寄存器放入到设备的BAR空间中，而不必和Intel的高精度声卡控制器相同，存放在PCI配置空间中。

目前许多处理器系统的RC仅支持一个VC通路，此时EP使用不同的TC进行传递数据的意义不大。x86处理器的MCH中一般支持两个VC通路，而多数PowerPC处理器仅支持一个VC通路。PLX公司的多数Switch也仅支持两个VC通路。

有些 RC，如 MPC8572 处理器，也能决定其发出 TLP 使用的 TC。在该处理器的 PCIe Outbound 窗口寄存器（PEXOWARn）中，含有一个 TC 字段，通过设置该字段可以确定 RC 发出的 TLP 使用的 TC 字段。不同的 TC 可以使用 PCIe 链路中的不同 VC，而不同的 VC 的仲裁级别并不相同。EP 或者 RC 通过调整其发出 TLP 的 TC 字段，可以调整 TLP 使用的 VC，从而调整 TLP 的优先级。

# 6.1.3 Attr字段

Attr字段由3位组成，其中第2位表示该TLP是否支持PCIe总线的ID-basedOrdering；

第1位表示是否支持Relaxed Ordering；而第0位表示该TLP在经过RC到达存储器时，是否需要进行Cache共享一致性处理。Attr字段如图6-3所示。

![[pci_express/e88e826a146ff620535a5f09cda34f36f78d5b0876b9bd6125a537e125e64338.jpg]]  
图6-3 Attr字段格式

一个 TLP 可以同时支持 ID-based Ordering 和 Relaxed Ordering 两种位序。Relaxed Ordering 最早在 PCI-X 总线规范中提出，用来提高 PCI-X 总线的数据传送效率；而 ID-based Ordering 由 PCIe V2.1 总线规范提出。TLP 支持的序如表 6-3 所示。

表 6-3 TLP 支持的序

<table><tr><td>Attr [2]</td><td>Attr [1]</td><td>类型</td></tr><tr><td>0</td><td>0</td><td>缺省序,即强序模型</td></tr><tr><td>0</td><td>1</td><td>PCI-X Relaxed Ordering 模型</td></tr><tr><td>1</td><td>0</td><td>ID-Based Ordering (IDO) 模型</td></tr><tr><td>1</td><td>1</td><td>同时支持 Relaxed Ordering 和 IDO 模型</td></tr></table>

当使用标准的强序模型时，在数据的整个传送路径中，PCIe设备在处理相同类型的TLP时，如PCIe设备发送两个存储器写TLP时，后面的存储器写TLP必须等待前一个存储器写TLP完成后才能被处理，即便当前报文在传送过程中被阻塞，后一个报文也必须等待。

如果使用Relaxed Ordering模型，后一个存储器写TLP可以穿越前一个存储器写TLP，提前执行，从而提高了PCIe总线的利用率。有时一个PCIe设备发出的TLP，其目的地址并不相同，可能先进入发送队列的TLP，在某种情况下无法发送，但这并不影响后续TLP的发送，因为这两个TLP的目的地址并不相同，发送条件也并不相同。

值得注意的是，在使用PCI总线强序模型时，不同种类的TLP间也可以乱序通过同一条PCIe链路，比如存储器写TLP可以超越存储器读请求TLP提前进行。而PCIe总线支持Relaxed Ordering模型之后，在TLP的传递过程中出现乱序种类更多，但是这些乱序仍然是有条件限制的。在PCIe总线规范中为了避免死锁，还规定了不同报文的传送数据规则，即Ordering Rules。有关PCIe总线序的详细说明见第11章。

PCIe V2.1 总线规范引入了一种新的“序”模型，即 IDO（ID-Based Ordering）模型，IDO 模型与数据传送的数据流相关，是 PCIe V2.1 规范引入的序模型。有关 PCIe 总线的 Relaxed Ordering 和 IDO 模型的详细说明见第 11.4.2 节。

Attr字段的第0位是“No Snoop Attribute”位。当该位为0时表示当前TLP所传送的数据在通过FSB时，需要与Cache保持一致，这种一致性由FSB通过总线监听自动完成而不需要软件干预；如果为1，表示FSB并不会将TLP中的数据与Cache进行一致，在这种情况下，进行数据传送时，必须使用软件保证Cache的一致性。

在PCI总线中没有与这个“No Snoop Attribute”位对应的概念，因此一个PCI设备对存

储器进行 DMA 操作时会进行 Cache 一致性操作。这种“自动的” Cache 一致性行为在某些特殊情况下并不能带来更高的效率。

当一个PCIe设备对存储器进行DMA读操作时，如果传送的数据非常大，比如512MB，Cache的一致性操作不但不会提高DMA写的效率，反而会降低。因为这个DMA读访问的数据在绝大多数情况下，并不会在Cache中命中，但是FSB依然需要使用Snoop Phase进行总线监听。而处理器在进行Cache一致性操作时仍然需要占用一定的时钟周期，即在Snoop Phase中占用的时钟周期，Snoop Phase是FSB总线事务的一个阶段，如图3-6所示。

对于这类情况，一个较好的做法是，首先使用软件指令保证 Cache 与主存储器的一致性，并置“No Snoop Attribute”位为 $1^{\text{②}}$ ，然后再进行 DMA 读操作。同理使用这种方法对一段较大的数据区域进行 DMA 写时，也可以提高效率。

除此之外，当PCIe设备访问的存储器，不是“可Cache空间”时，也可以通过设置“No Snoop Attribute”位，避免FSB的Cache共享一致性操作，从而提高FSB的效率。“No Snoop Attribute”位是PCIe总线针对PCI总线的不足作出的重要改动。

# 6.1.4 通用TLP头中的其他字段

除了 Fmt 和 Type 字段外，通用 TLP 头还含有以下字段。

# 1. TH位、TD位和EP位

TH 位为 1 表示当前 TLP 中含有 TPH（TLP Processing Hint）信息，TPH 是 PCIe V2.1 总线规范引入的一个重要功能。TLP 的发送端可以使用 TPH 信息，通知接收端即将访问数据的特性，以便接收端合理地预读和管理数据，TPH 的详细介绍见第 6.3.6 节。

TD 位表示 TLP 中的 TLP Digest 是否有效，为 1 表示有效，为 0 表示无效。而 EP 位表示当前 TLP 中的数据是否有效，为 1 表示无效，为 0 表示有效。

# 2. AT字段

AT字段与PCIe总线的地址转换相关。在一些PCIe设备中设置了ATC（Address Translation Cache）部件，这个部件的主要功能是进行地址转换。只有在支持IOMMU技术的处理器系统中，PCIe设备才能使用该字段。

AT字段可以用作存储器域与PCI总线域之间的地址转换，但是设置这个字段的主要目的是为了方便多个虚拟主机共享同一个PCIe设备。对这个字段有兴趣的读者可以参考Address Translation Services规范，这个规范是PCI的IO Virtualization规范的重要组成部分。对虚拟化技术有兴趣的读者可以参考清华大学出版社的《系统虚拟化——原理与实现》，以获得基本的关于虚拟化的入门知识。该书主要针对处理器系统的虚拟化技术。而本书将在第13.2.1节详细介绍AT字段和PCI总线相关的虚拟化技术。

# 3. Length字段

Length字段用来描述TLP的有效负载（DataPayload）大小。PCIe总线规范规定一个TLP的DataPayload的大小在 $0\sim 4096$ B之间。PCIe总线设置Length字段的目的是提高总

线的传送效率。

当PCI设备在进行数据传送时，其目标设备并不知道实际的数据传送大小，这在一定程度上影响了PCI总线的数据传送效率。而在PCIe总线中，目标设备可以通过Length字段提前获知源设备需要发送或者请求的数据长度，从而合理地管理接收缓冲，并根据实际情况进行Cache一致性操作。

当PCI设备进行DMA写操作，将PCI设备中4KB大小的数据传送到主存储器时，这个PCI设备的DMA控制器将存放传送的目的地址和传送大小，然后启动DMA写操作，将数据写入到主存储器。由于PCI总线是一条共享总线，因此传送4KB大小的数据，可能会使用若干个PCI总线写事务才能完成，而每一个PCI总线写事务都不知道DMA控制器何时才能将数据传送完毕。

如果这些总线写事务还通过一系列PCI桥才能到达存储器，在这个路径上的每一个PCI桥也无法预知这个DMA操作何时才能结束，那么这种“不可预知”将导致PCI总线的带宽不能被充分利用，而且极易造成PCI桥数据缓冲的浪费。

而PCIe总线通过TLP的Length字段，可以有效避免PCIe链路带宽的浪费。值得注意的是，Length字段以DW为单位，其最小单位为1个DW。如果PCIe主设备传送的单位小于1个DW或者传送的数据并不以DW对界时，需要使用字节使能字段，即“DWBE”字段。有关“DWBE”字段的详细说明见第6.3.1节。

# 6.2 TLP的路由

TLP 的路由是指 TLP 通过 Switch 或者 PCIe 桥片时采用哪条路径，最终到达 EP 或者 RC 的方法。PCIe 总线一共定义了三种路由方法，分别是基于地址（Address）的路由，基于 ID 的路由和隐式路由（Implicit）方式。

存储器和I/O读写请求TLP使用基于地址的路由方式，这种方式使用TLP中的Address字段进行路由选径，最终到达目的地。

而配置读写报文、“Vendor\_Defined Messages”报文、Cpl和CplD报文使用基于ID的路由方式，这种方式使用PCI总线号（Bus Number）进行路由选径。在Switch或者多端口RC的虚拟PCI-to-PCI桥配置空间中，包含如何使用PCI总线号进行路由选径的信息。

而隐式路由方式主要用于 Message 报文的传递。在 PCIe 总线中定义了一系列消息报文，包括“INTx Interrupt Signaling”，“Power Management Messages”和“Error Signal Messages”等报文。在这些报文中，除了“Vendor\_Defined Messages”报文，其他所有消息报文都使用隐式路由方式，隐式路由方式是指从下游端口到上游端口进行数据传递的使用路由方式，或者用于 RC 向 EP 发出广播报文。

# 6.2.1 基于地址的路由

在 PCIe 总线中，存储器读写和 I/O 读写 TLP 使用基于地址的路由方式。PCIe 设备使用

的地址路由方式与 PCI 设备使用的地址路由方式类似。只是 PCIe 设备使用 TLP 进行数据传送，而 PCI 设备使用总线周期进行数据传送。使用地址路由方式进行数据传递的 TLP 格式如第 6.3.1 节的图 6-8 所示，在这类 TLP 中包含目的设备的地址。

当一个 TLP 进行数据传递时，可能会经过多级 Switch，最终到达目的地。Switch 将根据存储器读写和 I/O 读写请求 TLP 的目的地址将报文传递到合适的 Egress 端口上。如图 4-10 所示，在一个 Switch 中包含了多个虚拟 PCI-to-PCI 桥。在 Switch 中有几个端口，就包含几个虚拟 PCI-to-PCI 桥。

在虚拟PCI-to-PCI桥的配置寄存器空间中，包含一个桥片能够接收的物理地址范围。PCIe总线通过这个物理地址范围实现基于地址的路由。这段配置寄存器如图6-4所示。当系统软件初始化PCI总线时，将合理地设置这些寄存器，之后当TLP通过这些Switch时将根据这些寄存器选择合适的路径。

![[pci_express/41396dcc258192fe83f8ca39fe1a32bca3ea197f56915ff4f27143d9bf934a00.jpg]]  
图6-4 与地址路由相关的PCI桥片配置寄存器

图6-4中的配置寄存器描述了该虚拟PCI-to-PCI桥下游PCI子树使用的三组空间范围，分别为I/O、存储器和可预取的存储器空间，分别用Base和Limit两类寄存器描述，其中Base寄存器表示可访问空间的基地址，Limit寄存器表示可访问空间的大小。TLP使用基于地址的路由时，一定要通过查询这组寄存器之后，再决定传送路径。这组寄存器的使用方法与PCI总线中的PCI桥兼容。

其中TLP“从上游端口发送到下游端口”与“从下游端口发送到上游端口”的路由过程略有不同，如图6-5所示。下文以TLP1～TLP3的发送过程对地址路由过程进行说明。TLP1～TLP3的描述如下。

- TLP1 是一个存储器或者 I/O 请求 TLP，由 RC 发出，并通过一个 Switch 发向 EP1。存储器和 I/O 读写请求 TLP 使用这种地址路由方式。TLP1 将从 Switch 的上游端口传送到下游端口。  
- TLP2 是一个存储器或者 I/O 请求 TLP，由 EP2 发出，并通过一个 Switch 发向 RC。当 PCIe 设备进行 DMA 读写操作时，将使用这种地址路由方式。TLP2 将从 Switch 的下游端口传送到上游端口。  
- TLP3 是一个存储器或者 I/O 请求 TLP，由一个 EP2 发出，并通过一个 Switch 后发送到另外一个 EP。在 x86 处理器系统中，这种用法并不常见。但是在某些大规模处理器系统中，具有这种应用方式。此时 TLP3 将从 Switch 的下游端口传送到另外一个下游端口。

# 1. TLP1 的传送过程

当 TLP1 从 RC 发向 EP1 时，这个 TLP1 为 I/O 或者存储器报文，其中 TLP1 目的地址在 EP1 的 BAR 空间中。当处理器访问 EP 的 BAR 空间时，需要使用该类 TLP。值得注意的是

![[pci_express/c2e42ff344db0b5501f9fbd61f4064f29384c308f4a069565381ad8606b8dabb.jpg]]  
图6-5 基于地址的路由寻径方式

这个数据报文在通过RC时需要进行地址转换

TLP1 首先通过 PCI Bus0 发向 Switch，并通过 Switch 的 Upstream 端口到达 P-P1 桥片，P-P1 桥片首先根据配置寄存器中的 Limit 和 Base 寄存器决定是否接收 TLP1。如果 Switch 不接收 TLP1，则将该 TLP 作为不支持的请求（Unsupported Request）处理，此时如果 TLP1 需要回应报文，Switch 将发出完成报文，该报文的状态为 UR（Unsupported Request）。

如果Switch接收TLP1，则表示TLP1所访问的地址在该Switch下游端口所连接的EP或者Switch中，此时Switch将TLP1从PCI Bus0推至PCI Bus1中，即穿越P-P1桥片。TLP1到达PCI Bus1后将同时查找P-P2和P-P3桥片配置寄存器中的Limit和Base寄存器，决定是P-P2还是P-P3桥片接收TLP1。本小节中的例子将使用P-P2桥片接收TLP1，并将TLP1推至PCI Bus2，而PCI Bus2上的EP1将接收TLP1，完成整个地址路由。

# 2. TLP2 的传送过程

当 TLP2 从 EP2 发向 RC 时，一般来说该 TLP 将访问处理器系统的主存储器。此时 TLP2 首先将请求发至 P-P3 桥片，在 P-P3 桥片配置寄存器的 Limit 和 Base 寄存器中当然不会包含 TLP2 所访问的地址，此时 P-P3 桥片将 TLP2 推至 PCI Bus1。

TLP“从下游端口向上游端口”与“从TLP从上游端口向下游端口”进行传递时，桥片的处理机制有所不同，从上游端口向下游端口传递时，如果桥片配置寄存器的Limit和Base寄存器包含该TLP的访问地址时，桥片将接收此TLP，否则不接收该TLP。而从下游端口向上游端口传递时，如果桥片配置寄存器的Limit和Base寄存器不包含该TLP的访问地址时，桥片将接收该TLP，并将其推至桥片的上游PCI总线。值得注意的是，这两种地址译码方式都属于PCI总线的正向译码。

当 TLP2 到达 PCI Bus1 时，首先检查在 PCI Bus1 总线上的 P-P2 桥片是否可以接收此 TLP，如果不能接收则 TLP2 通过 P-P1 桥片传递到 PCI Bus0，即到达 RC。

在MPC8548处理器中，到达RC的TLP首先通过Inbound寄存器进行地址转换，将TLP的PCI总线地址转换为处理器的地址，然后访问处理器中相应的存储器空间；对于x86处理器而言，MCH也会完成PCI域地址空间到存储器域地址空间的转换，然后访问处理器中相应的存储器空间。

# 3. TLP3 的传送过程

TLP3 的传递方式与 TLP2 的传递方式有些类似，当 TLP3 传递到 PCI Bus1 时，P-P2 桥片将接收 TLP3，并将 TLP3 传递到 PCI Bus2 上的 EP1 中。由以上叙述可以发现，PCIe 总线中基于地址的路由方式与 PCI 总线上的基于地址的数据传递流程十分相近。TLP3 在 PCI 总线域上进行数据传递，因此不需要进行 PCI 总线域到存储器域的地址转换。

# 6.2.2 基于ID的路由

在PCIe总线中，基于ID的路由方式主要用于配置读写请求TLP、Cpl和CplD报文，此外Vendor\_Defined消息报文也可以使用这种基于ID的路由方式。而在PCI总线中，只有配置读写周期才使用ID进行数据传递。

基于ID的路由方式与基于地址的路由方式有较大的不同，基于ID路由方式的TLP头格式也与基于地址路由方式的头格式不同，其报文格式如图6-6所示。

![[pci_express/cee67e7f1b50e15d02a07aef48d95f46da24c40827443d7a1c470c0a97830187.jpg]]  
图6-6 使用ID路由的TLP头格式

使用ID路由方式的TLP头，其Byte8～Byte11字段与基于地址路由的TLP不同。基于ID路由的TLP，使用Bus Number、Device Number和Function Number进行路由寻址。从软件的角度来看，PCIe总线与PCI总线兼容，只是在PCIe总线中，每一个PCIe设备使用唯一的PCI设备号，但是每一个设备仍然可以有多个子设备（Function）。

PCIe总线规定，在一个PCI总线域空间中，最多只能有256条PCI总线，因此在一个TLP中，Bus Number由五位组成；而在每一条总线中最多包含32个设备，因此TLP中的Device Number由5位组成；而每一个设备中最多包含8个功能，因此一个TLP的FunctionNumber由3位组成。

配置读写请求TLP是使用“基于ID路由”的一组重要报文，其主要作用是读写PCIe总线的EP、Switch及PCIe桥片的配置寄存器，以完成PCIe总线的配置。在处理器系统上电之后需要进行PCI总线系统的枚举，为PCI总线分配总线号，并设置Switch、PCIe桥片或者EP的配置寄存器，如Limit寄存器组、Base寄存器组、BAR寄存器、SubordinateBusNumber、SecondaryBusNumber和PrimaryBusNumber等一系列配置寄存器。

在上文中我们简单介绍了Limit寄存器组和Base寄存器组的用法，下文将重点描述Sub-

ordinate Bus Number、Secondary Bus Number 和 Primary Bus Number 寄存器。Subordinate Bus Number、Secondary Bus Number 和 Primary Bus Number 寄存器在 Type 01h 配置寄存器中，用来描述 PCI-to-PCI 桥片的上游及下游总线号。这段寄存器在 PCI 配置寄存器中的位置如所图 6-7 示。

<table><tr><td>Secondary Latency Timer</td><td>Subordinate Bus Number</td><td>Secondary Bus Number</td><td>Primary Bus Number</td></tr></table>

图6-7 与ID路由相关的PCI桥片配置寄存器

与PCI总线中的桥片类似，PrimaryBusNumber记录PCI-to-PCI桥上游的PCI总线号，SecondaryBusNumber记录PCI-to-PCI桥下游的第一个PCI总线号，而SubordinateBusNumber记录PCI-to-PCI桥下游的最后一个PCI总线号。

如图6-5所示，P-P1桥片的PrimaryBusNumber为0，SecondaryBusNumber为1，而SubordinateBusNumber为3。这些总线号，在处理器系统对PCI总线进行枚举时由系统初始化程序设置，从系统初始化程序的角度来看，PCIe总线与PCI总线基本兼容，只是PCIe总线对配置空间进行了一些扩展。

在如表6-2所示中，RC可以使用Type00h和Type01h读写请求TLP，对PCIe设备的配置寄存器进行读写访问，配置读写请求TLP只能由RC发出，配置读写请求TLP使用基于ID的路由方式。

在如图6-5所示的例子中，RC首先使用Type00h配置请求TLP访问在PCI Bus0总线上的设备，PCI Bus0上的所有设备，包括桥片都要监听PCI Bus0上的配置请求，在本例中只有Switch挂接在PCI Bus0上，实际上是Switch的上游端口与PCI Bus0直接相连。因此Switch的上游端口将接收RC发出的Type00h配置请求TLP，之后Switch将向RC发出完成报文，结束配置请求。与PCI总线相同，PCIe总线的Type00h类型配置请求TLP不能够穿越桥片，在图6-5中这类请求只能访问Switch上游端口的配置空间。

PCI总线是基于共享总线的数据传送方式，在一条PCI总线上可以连接多个PCI Agent设备，其中每一个PCI Agent都提供了一个IDSEL#信号，这个信号与PCI-to-PCI桥片或者HOST主桥的地址线直接相连，PCI总线根据与IDSEL#信号与地址线的连接关系决定相应设备的Device Number。

这与PCIe总线的使用方法不同，PCIe总线使用“端对端”的连接方式，PCIe链路只能连接一个下游设备，而这个下游设备的Device Number只能为0。而只有在Switch的虚拟PCI总线上可以连接多个Device Number不同的端口。

当一个虚拟PCI总线上挂接PCI-to-PCI桥时，系统配置软件将使用Type01h配置请求TLP访问PCI-PCI桥下游的PCI设备。如图6-5所示，RC可以通过Type01h配置请求TLP访问P-P2桥片、P-P3桥片，EP1和EP2。

当RC使用Type01h配置请求TLP，直接访问P-P1桥的下游设备时，首先需要检查该TLP的Bus Number是否为1，如果为1表示该TLP的访问目标在PCI Bus 1总线上，此时PCI-to-PCI桥将这个Type01h类型的TLP转换为Type00h类型的TLP，然后推至PCI Bus 1总线，并访问其下的设备。

如果该 TLP 的 Bus Number 在 P-P1 桥片的 Secondary Bus Number 和 Subordinate Bus Num-

ber寄存器之间，则P-P1桥片将该Type01h类型的TLP直接透传到PCI Bus1上，并不改变该TLP的类型，之后Type01h类型的TLP将继续检查P-P2和P-P3桥片的配置空间，决定由P-P2还是P-P3接收该TLP。如果TLP的PCI Bus Number为2时，P-P2桥片将接收该TLP，并将该Type01h类型TLP转换为Type00h类型的TLP，然后发送给EP1，并由EP1处理该TLP。

上文简要讲述了配置请求 TLP 使用 ID 路由方式从上游端口向下游端口的传递规则，但是 Vendor\_Defined 消息报文和 Cpl 和 CplD 报文还可能从下游端口向上游端口进行传递。此时 PCIe 总线处理方法略有不同。下文仍以图 6-5 为例说明这种情况。

当一个 TLP 从 EP2 传送到 EP1 或者 RC 时，首先检查 P-P3 桥片的配置空间，P-P3 桥片发现该 TLP 不是发向自己时，将该 TLP 推至上游总线，即 PCI Bus1。如果 PCI Bus1 上 P-P1 桥片没有认领该 TLP，该 TLP 将继续向 P-P2 桥片传递，并由这个桥片将 TLP 转发给合适的 EP；如果 P-P1 桥片认领该 TLP，该 TLP 将继续向上游总线传递，直至 RC。

由以上描述可以发现，PCIe总线使用的基于ID的路由方式与PCI总线中配置读写总线事务通过PCI桥的方法较为类似。

