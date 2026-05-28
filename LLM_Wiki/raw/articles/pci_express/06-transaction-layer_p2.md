---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "06"
section: "6.2.3 隐式路由"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 6.2.3 隐式路由

PCIe总线规定消息请求报文使用隐式路由方式。在PCIe总线中，有许多消息是直接发向RC或者来自RC的广播报文，这些报文不使用地址或者ID进行路由，而是使用Msg和MsgD报文的Route字段进行路由，这种路由方式称为隐式路由。

PCIe总线定义了一些用于中断请求、错误状态处理、锁定总线事务、热插拔信号处理和“Vendor Defined Messages”消息报文。这些消息报文需要使用隐式路由方式进行传递。消息报文的Route字段的含义如表6-4所示。

表 6-4 Route [4:0] 字段

<table><tr><td>Route [2:0]</td><td>描述</td></tr><tr><td>000</td><td>路由到 RC</td></tr><tr><td>001</td><td>使用地址路由</td></tr><tr><td>010</td><td>使用ID路由</td></tr><tr><td>011</td><td>来自 RC的广播报文</td></tr><tr><td>100</td><td>本地消息，在接收端结束（Legacy中断消息使用这种报文格式，传递来自 PCI总线的中断报文）</td></tr><tr><td>101</td><td>用于PCIe电源管理（PME_TO_Ack报文使用）</td></tr><tr><td>110~111</td><td>保留</td></tr></table>

使用隐式路由方式的 TLP，其 Route 字段为“000”，“011”，“100”或者“101”。当一个报文使用隐式路由向 EP 发送时，EP 将对 Route 字段进行检查，如果这个报文是“来自 RC 的广播报文”，或者是“本地报文”，EP 将接收此报文。

如果Switch收到一条使用隐式路由的TLP时，将根据报文Route字段的不同而分别处理。如果Switch的上游端口接收了一条来自RC的广播消息，则将该报文发向所有的下游端口；如果Switch接收了一条来自下游端口发向RC的消息报文时，Switch将此报文直接转发

到上游端口，直至 RC；如果 Switch 接收了一条使用隐式路由方式的本地消息报文，则 Switch 接收并终结此报文，不再上传或下推。

如果 RC 收到一个使用隐式路由的 TLP 时，将根据报文 Route 字段而分别处理这些 TLP。如果该 Route 字段为 0b000 和 0b101，RC 将接收该 TLP，并作相应的处理；如果为 0b100，RC 将接收该 TLP，并结束该 TLP 报文的传递。

# 6.3 存储器、I/O和配置读写请求TLP

本节讲述PCIe总线定义的各类TLP，并详细介绍这些TLP的格式。在这些TLP中，有些格式对于初学者来说较难理解。为此本书将在第12章中结合一个设计实例，进一步描述这些TLP格式。

但是在阅读第12章的内容之前，读者需要建立PCIe总线中与TLP相关的一些基本概念，特别是存储器读写相关的报文格式。在PCIe总线中，存储器读写，I/O读写和配置读写请求TLP由以下几类报文组成。

# （1）存储器读请求TLP和读完成TLP

当PCIe主设备，RC或者EP，访问目标设备的存储器空间时，使用Non-Posted总线事务向目标设备发出存储器读请求TLP，目标设备收到这个存储器读请求TLP后，使用存储器读完成TLP，主动向主设备传递数据。当主设备收到目标设备的存储器读完成TLP后，将完成一次DMA读操作。

# (2) 存储器写请求 TLP

在 PCIe 总线中，存储器写使用 Posted总线事务。PCIe 主设备仅使用存储器写请求 TLP 即可完成 DMA 写操作，主设备不需要目标设备的回应报文。

# (3) 原子操作请求和完成报文

原子操作由PCIe V2.1总线规范引入，一个完整的原子操作由原子操作请求和原子操作完成报文组成。原子操作的使用方法与其他Non-Posted总线事务类似，首先PCIe主设备向目标设备发送原子操作请求，之后目标设备向主设备发送原子操作完成报文，结束一次原子操作。有关原子操作的详细说明见第6.3.5节。

# (4) I/O 读写请求 TLP 和读写完成 TLP

在PCIe总线中，I/O读写操作使用Non-Posted总线事务，I/O读写TLP都需要完成报文做为回应。只是在I/O写请求的完成报文中不需要“带数据”，而仅含有I/O写请求是否成功的状态信息。

# (5) 配置读写请求 TLP 和配置读写完成 TLP

从总线事务的角度上看，配置读写请求操作的过程与I/O读写操作的过程类似。配置读写请求TLP都需要配置读写完成作为应答，从而完成一个完成的配置读写操作。

# (6) 消息报文

与PCI总线相比，PCIe总线增加了消息请求事务。PCIe总线使用基于报文的数据传送模式，所有总线事务都是通过报文实现的，PCIe总线取消了一些在PCI总线中存在的边带信号。在PCIe总线中，一些由PCI总线的边带信号完成的工作，如中断请求和电源管理等，在PCIe总线中由消息请求报文实现。

# 6.3.1 存储器读写请求 TLP

存储器读写请求TLP的格式如图6-8所示

![[pci_express/ad1495dfd1a1c9f97c5a80374a51f531cccb99894c2651bd1e8a4a7be6f77dac.jpg]]

![[pci_express/fb8a19612949e072d175491b8a9325ed04330a3118574d5ea7546c8b143ff57b.jpg]]  
图6-8 存储器和I/O读写请求TLP头格式

在 PCIe 总线中，存储器写请求 TLP 使用 Posted 数据传送方式。而其他与存储器和 I/O 相关的报文都使用 Split 方式进行数据传送，这些请求报文需要完成报文，通知发送端之前的数据请求报文已经处理完毕，有关完成报文的详细说明见第 6.3.2 节。

存储器读写请求 TLP 使用地址路由方式进行数据传递，在这类 TLP 头中包含 Address 字段，Address 字段具有两种地址格式，分别是 32 位和 64 位地址。在存储器读写和 I/O 读写请求的第 3 和第 4 个双字中，存放 TLP 的 32 或者 64 位地址。存储器、I/O 和原子操作读写请求使用的 TLP 头较为类似。本节仅介绍存储器、I/O 读写使用的 TLP 头，而在第 6.3.5 节详细介绍原子操作。

# 1. Length字段

在存储器读请求 TLP 中并不包含 Data Payload，在该报文中，Length 字段表示需要从目标设备数据区域读取的数据长度；而在存储器写 TLP 中，Length 字段表示当前报文的 Data Payload 长度。

Length字段的最小单位为DW。当该字段为n时，表示需要获得的数据长度或者当前报文的数据长度为 $\mathbf{n}$ 个DW，其中 $0\leqslant \mathrm{n}\leqslant 0\mathrm{x}3\mathrm{FF}$ 。值得注意的是，当 $\mathbf{n}$ 等于0时，表示数据长度为1024个DW。

# 2. DW BE 字段

PCIe总线以字节为基本单位进行数据传递，但是Length字段以DW为最小单位。为此TLP使用LastDWBE和FirstDWBE这两个字段进行字节使能，使得在一个TLP中，有效数据以字节为单位。

这两个DWBE字段各由4位组成，其中LastDWBE字段的每一位对应数据Payload最后一个双字的字节使能位；而FirstDWBE字段的每一位对应数据Payload第一个双字的字节使能位。其对应关系如表6-5所示。

表 6-5 First 和 Last DW BE 字段

<table><tr><td rowspan="4">Last DW BE</td><td>第3位</td><td>为1表示数据Payload的最后一个双字的字节3有效</td></tr><tr><td>第2位</td><td>为1表示数据Payload的最后一个双字的字节2有效</td></tr><tr><td>第1位</td><td>为1表示数据Payload的最后一个双字的字节1有效</td></tr><tr><td>第0位</td><td>为1表示数据Payload的最后一个双字的字节0有效</td></tr><tr><td rowspan="4">First DW BE</td><td>第3位</td><td>为1表示数据Payload的第一个双字的字节3有效</td></tr><tr><td>第2位</td><td>为1表示数据Payload的第一个双字的字节2有效</td></tr><tr><td>第1位</td><td>为1表示数据Payload的第一个双字的字节1有效</td></tr><tr><td>第0位</td><td>为1表示数据Payload的第一个双字的字节0有效</td></tr></table>

Last DW BE 和 First DW BE 这两个字段的使用规则如下。

- 如果传送的数据长度在一个对界的双字（DW）之内，则Last DW BE字段为0b0000，而First DW BE的对应位置1；如果数据长度超过1DW，Last DW BE字段一定不能为0b0000。PCIe总线使用Last DW BE字段为0b0000表示所传送的数据在一个对界的DW之内。  
- 如果传送的数据长度超过1DW，则First DW BE字段至少有一个位使能。不能出现First DW BE为0b0000的情况。  
- 如果传送的数据长度大于等于3DW，则在First DW BE和Last DW BE字段中不能出现不连续的置1位。  
- 如果传送的数据长度在1DW之内时，在First DW BE字段中允许有不连续的置1位。此时PCIe总线允许在TLP中传送1个DW的第1，3字节或者第0，2字节。  
- 如果传送的数据长度在2DW之内时，则First DW BE字段和Last DW BE字段允许有不连续的置1位。

值得注意的是，PCIe总线支持一种特殊的读操作，即“Zero-Length”读请求。此时Length字段的长度为1DW，而First DW BE字段和Last DW BE字段都为0b0000，即所有字节都不使能。此时与这个存储器读请求TLP对应的读完成TLP中不包含有效数据。再次提醒读者注意“Zero-Length”读请求使用的Length字段为1，而不为0，为0表示需要获得的数据长度为1024个DW。

“Zero-Length”读请求的引入是为了实现“读刷新”操作，该操作的主要目的是为了确保之前使用Posted方式所传送的数据，到达最终的目的地，与“Zero-Length”读对应的读完成报文中不含有负载，从而提高了PCIe链路的利用率。

在PCIe总线中，使用Posted方式进行存储器写时，目标设备不需要向主设备发送回应报文，因此主设备并不知道这个存储器写是否已经达到目的地。而主设备可以使用“读刷新”操作，向目标设备进行读操作来保证存储器写最终到达目的地。有关“读刷新”的详细说明及实现原理见第11章。

在PCIe总线中，标准的存储器读请求也可以完成同样的刷新操作。但是“Zero-Length”读请求与这种读请求相比，其完成报文不需要“Data Payload”，因此在一定程度上提高了PCIe总线的效率。如果一个存储器读请求TLP报文的TH位为1时，DWBE字段将被重新定义为ST[7:0]字段，有关ST字段的详细说明见第6.3.6节。

# 3. Requester ID 字段

Requester ID字段包含“生成这个TLP报文”的PCIe设备的总线号（Bus Number）、设备号（Device Number）和功能号（Function Number），其格式如图6-9所示。对于存储器写请求TLP，Requester ID字段并不是必须的，因为目标设备收到存储器写请求TLP后，不需要完成报文作为应答，因此Requester ID字段对于存储器写请求TLP并没有实际意义。

但是PCIe总线规范并没有明确说明存储器写请求TLP究竟需不需要Requester ID字段，为此IC设计者依然需要将存储器写TLP的Requester ID字段置为有效。值得注意的是，如果一个存储器写请求TLP报文的TH位为1时，Tag字段将被重新定义为ST[7:0]字段，有关ST字段的详细说明见第6.3.6节。

对于Non-Posted数据请求，目标设备需要使用完成报文做为回应。在这个完成报文中，需要使用源设备的Requester ID字段。因此在Non-Posted数据请求TLP中，如存储器读请求、I/O和配置读写请求TLP，必须使用Requester ID字段。

存储器，I/O读请求TLP中含有RequesterID和Tag字段。在PCIe总线中RequesterID和Tag字段合称为TransactionID，TransactionID字段的格式如图6-9所示。存储器读，I/O和配置读写请求TLP使用Transaction字段的主要目的是使接收端通过分析报文的TransactionID，确认完成报文的目的地。

![[pci_express/ddbd953dd03095296cdfc82dc7d1259bb1d8fbdbf3671b581e777a09e4d245b0.jpg]]  
图6-9 Transaction ID的格式

在 PCIe 总线中，所有 Non-Posted 数据请求都需要完成报文作为应答，才能结束一次完整的数据传递。一个源设备在发送 Non-Posted 数据请求之后，如果并没有收到目标设备回送的完成报文，TLP 报文的发送端需要保存这个 Non-Posted 数据请求，此时该设备使用的 Transaction ID（Tag 字段）不能被再次使用，直到一次数据传送结束，即数据发送端收齐与该 TLP 对应的所有完成报文。

在同一个时间段内，PCIe设备发出的每一个Non-Posted数据请求TLP，其TransactionID必须是唯一的。即在同一时间段内，在当前PCI总线域中不能存在两个或者两个以上的存储器读请求TLP，其TransactionID完全相同。

源设备发送Non-Posted数据请求后，在没有获得全部完成报文之前，不能释放这个Transaction ID占用的资源。在同一个PCIe设备发送的TLP中，其Requester ID字段是相同的，因此PCIe设备的设计者所能管理的资源是Tag字段。PCIe设备的设计者需要合理地管理Tag资源，以保证数据传送的正确性。

PCIe 设备在发送 Non-Posted 数据请求时，需要暂存这些 Non-Posted 数据请求。其中 Tag

字段的长度决定了发送端能够暂存多少个同类型的 TLP，如果 Tag 字段长度为 5，发送端能够暂存 32 个报文；如果 PCIe 设备使能了 Extended Tag 位（该位的详细描述见第 4.3.2 节），Tag 字段可以由 8 位组成，此时发送端能够暂存 256 个报文。

通过 Tag 字段的长度，可以发现每个 PCIe 设备最多可以暂存 256 个同类型的 Non-Posted 报文。但是在多数情况下，一个 PCIe 设备可能只包含 1 个 Function。因此 PCIe 设备还可以使用 Function 号扩展 Tag 字段，从而扩展“暂存 TLP 报文”的数目。

PCIe 设备在 PCI Express Capability 结构的 Device Control 寄存器中，设置了一个 Phantom Functions Enable 位，该位的详细说明见第 4.3.2 节。当一个 PCIe 设备仅支持一个 Function 时，Phantom Functions Enable 位可以被设置为 1，此时 PCIe 设备可以使用 Requester ID 的 Function Number 字段对 Tag 字段进一步扩展，此时一个 PCIe 设备最多可以支持 2048 个同类型的数据请求。

由以上分析可以发现，一个PCIe设备最多可以支持2048个存储器读数据请求，基本上可以满足绝大多数需要。但是在某些特殊应用场合，PCIe设备即使可以暂存2048个存储器读请求，也并不足够。

与PCI总线相比，PCIe总线的数据传送延时较长，而为了弥补这个传送延时，PCIe设备通常使用流水线技术。此时PCIe设备必须能够连续发送多个存储器读请求报文，随后RC也将连续回送多个存储器读完成报文，在PCIe设备的实现中，需要保证能够源源不断地从RC接收这些报文，以充分利用报文接收流水线，有关这部分内容详见第12.4.3节。

PCIe V2.1 总线规范还提出了另一种 Requester ID 格式，即 ARI（Alternative Routing-ID Interpretation）格式，除了 Requester ID 外，在完成报文中使用的 Completer ID 也可以使用这种格式。ARI 格式将 ID 号分为两个字段，分别为 Bus 号和 Function 号，而不使用 Device 号，ARI 格式如图 6-10 所示。

![[pci_express/8df8002ae99779bd209911ec1952212c57551b27f4fa6f4280a0c2189ea0fcdc.jpg]]  
图6-10ARI格式

PCIe总线引入ARI格式的依据是在一个PCIe链路上仅可能存在一个PCIe设备，因而其Device号一定为0。在多数PCIe设备中，RequesterID和CompletionID包含的Device号是没有意义的。使用ARI格式时，一个PCIe设备最多可以支持256个Function，而传统的PCIe设备最多只能支持8个Function。

# 4. I/O 读写请求 TLP 的规则

I/O 读写请求与存储器读写请求 TLP 的格式基本类似，只是 I/O 读写请求 TLP 只能使用 32 位地址模式和基于地址的路由方式，而且 I/O 读写请求 TLP 只能使用 Non-Posted 方式进行传递。PCIe 总线并不建议 PCIe 设备支持 I/O 地址空间，但是 Switch 和 RC 需要具备接收和发送 I/O 请求报文的能力，因为许多老的 PCI 设备依然使用 I/O 地址空间，这些 PCI 设备可以通过 PCIe 桥连接到 PCIe 总线中。因此虽然支持 I/O 读写请求的 PCIe 设备极少，但是在 PCIe 体系结构中，依然需要支持 PCI 总线域的 I/O 地址空间。

与存储器读写请求 TLP 不同，I/O 读写请求 TLP 的某些字段必须为以下值。

- TC [2:0] 必须为 0, I/O 请求报文使用的 TC 标签只能为 0。  
- TH 和 Attr2 位保留，而 Attr [1:0] 必须为 “0b00”，这表示 I/O 请求报文必须使用 PCI 总线的强序数据传送模式，而且在传送过程中，硬件保证其传送的数据与 Cache 保持一致，实际上 I/O 地址空间都是不可 Cache 的。  
- AT [1:0] 必须为 “0b00”, 表示不支持地址转换, 因此在虚拟化技术中, 并不处理 PCI 总线域中的 I/O 空间。  
- Length [9:0] 为 “0b00 0000 0001”, 表示 I/O 读写请求 TLP 最大的数据 Payload 为 1DW, 该类 TLP 不支持突发传送。  
- Last DW [3:0] 为 “0b0000”。

# 6.3.2 完成报文

PCIe总线支持Split传送方式，目标设备使用完成报文向源设备主动发送数据。完成报文使用ID路由方式，由TLP Prefix、报文头和Data Payload组成，但是某些完成报文可以不含有Data Payload，如I/O或者配置写完成和Zero-Length读完成报文。在PCIe总线中，有以下几类数据请求需要收到完成报文之后，才能完成整个数据传送过程，完成报文格式如图6-11所示。

- 所有的数据读请求，包括存储器、I/O 读请求、配置读请求和原子操作请求。当一个 PCIe 设备发出这些数据请求报文后，必须收到目标设备的完成报文后，才能结束一次数据传送。这一类完成报文必须包含 Data Payload。  
- 所有的 Non-Posted 数据写请求，包括 I/O 和配置写请求。当一个 PCIe 设备发出这些数据请求报文后，必须收到目标设备的完成报文后，才能结束数据传送。但是这一类完成报文不包含数据，仅包含应答信息。  
- 与 ATS 机制相关的一些报文，详见第 13.2 节。

![[pci_express/c6b51155c4bed076d801d6c966d23a80d1a40eaa87ae1050d192026468d8438e.jpg]]  
图6-11 完成报文头格式

完成报文“Byte 0”中的大部分字段与“存储器，I/O、配置请求报文”的对应字段的含义相同。完成报文一次最多能够传送的报文大小不能超过 Max\_Payload\_Size 参数。在多数处理器中，完成报文中包含的数据在一个 Cache 行之内，完成报文使用 RCB 参数来处理数据对界，RCB 参数的大小与处理器系统的 Cache 行长度和 DDR-SDRAM 的一次突发传送长度相关，这些参数的详细描述见第 6.4.3 节。在 x86 和 PowerPC 处理器中，一个存储器读完成报文一般不超过 RCB 参数。

# 1. Requester ID 和 Tag 字段

完成报文使用ID路由方式，ID路由方式详见第6.2.2节。完成报文头的长度为3DW，完成报文头中包含Transaction ID信息，由Requester ID和Tag字段组成，这个ID必须与源设备发送的数据请求报文的Transaction ID对应，完成报文使用Transaction ID进行ID路由，并将数据发送给源设备。

当 PCIe 设备收到存储器读、I/O 读写或者配置读写请求 TLP 时，需要首先保存这些报文的 Transaction ID，之后当该设备准备好完成报文后，将完成报文的 Requester ID 和 Tag 字段赋值为之前保存的 Transaction ID 字段。

# 2. Completer ID 字段

Completer ID字段的含义与RequesterID字段较为相似，只是该字段存放“发送完成报文”的PCIe设备的ID号。PCIe设备进行数据请求时需要在TLP字段中包含RequesterID字段；而使用完成报文结束数据请求时，需要提供CompleterID字段。

# 3. Status字段

Status字段保存当前完成报文的完成状态，表示当前TLP是正确地将数据传递给数据请求端；还是在数据传递过程中出现错误；或者要求数据请求方进行重试。PCIe总线规定了几类完成状态，如表6-6所示。

表 6-6 Status 字段

<table><tr><td>Status [2:0]</td><td>描述</td></tr><tr><td>0b000</td><td>SC (Successful Completion), 正常结束</td></tr><tr><td>0b001</td><td>UR (Unsupported Request), 不支持的数据请求</td></tr><tr><td>0b010</td><td>CRS (Configuration Request Retry Status), 要求数据请求方进行重试。当 RC 对一个 PCIe 目标设备发起配置请求时, 如果该目标设备没有准备好, 可以向 RC 发出 CRS 完成报文, 当 RC收到这类报文时, 不能结束本次配置请求, 必须择时重新发送配置请求</td></tr><tr><td>0b100</td><td>CA (Completion Abort), 数据夭折。表示目标设备无法完成本次数据请求</td></tr><tr><td>其他</td><td>保留</td></tr></table>

# 4. BCM 位与 Byte Count 字段

BCM（Byte Count Modified）字段由PCI-X设备设置。PCI-X设备也支持Split Transaction传送方式，当PCI-X设备进行存储器读请求时，目标设备不一定一次就能将所有数据传递给源设备。此时目标设备在进行第一次数据传送时，需要设置Byte Count字段和BCM位。

BCM 位表示 Byte Count 字段是否被更改，该位仅对 PCI-X 设备有效，而 PCIe 设备不能操纵 BCM 位，只有 PCI-X 设备或者 PCIe-to-PCI-X 桥可以改变该位。本节对此位不做进一步介绍，对此位感兴趣的读者可以参考 PCI-X Addendum to the PCI Local Bus Specification, Revision 2.0。

Byte Count 字段记录源设备还需要从目标设备中获得多少字节的数据就能完成全部数据传递，当前 TLP 中的有效负载也被 Byte Count 字段统计在内。该字段由 12 位组成。该字段为 0b0000-0000-0001 表示还剩一个字节，为 0b1111-1111-1111 表示还剩 4095 个字节，而为 0b0000-0000-0000 表示还剩 4096 个字节。除了存储器读请求的完成报文外，大多数完成报

文的 Byte Count 字段为 4。

如一个源设备向目标设备发送一个“读取128B的存储器读请求TLP”，而目标设备收到这个读请求TLP后，可能使用两个存储器读完成TLP传递数据。其中第1个存储器读完成TLP的有效数据为64B，而ByteCount字段为128；第2个存储器读完成TLP中的有效数据为64B，而ByteCount字段也为64。当数据请求端接收完毕第1个存储器读完成TLP后，发现还有64B的数据没有接收完毕，此时必须等待下一个存储器读完成TLP。等到数据请求端收齐所有数据后，才能结束整个存储器读请求。

目标设备发出的第2个读完成TLP中的有效数据为64B，而Byte Count字段为64，当数据请求端接收完毕这个读完成TLP后，将完成一个完整的存储器读过程，从而可以释放这个存储器读过程使用的Tag资源。

存储器读请求的完成报文的拆分方式较为复杂，Byte Count 字段的设置也相对较为复杂。在第 12 章将结合一个实例讲述该字段的使用方法。

# 5. Lower Address 字段

如果当前完成报文为存储器读完成TLP，该字段存放在存储器读完成TLP中第一个数据所对应地址的最低位。值得注意的是，在完成报文中，并不存在First DW BE和Last DW BE字段，因此接收端必须使用存储器读完成TLP的Low Address字段，识别一个TLP中包含数据的起始地址。第12.2.2节将详细介绍该字段的作用。

# 6.3.3 配置读写请求 TLP

配置读写请求TLP由RC发起，用来访问PCIe设备的配置空间。配置请求报文使用基于ID的路由方式。PCIe总线也支持两种配置请求报文，分别为Type00h和Type01h配置请求。配置请求TLP的格式如图6-12所示。

![[pci_express/480619aafc56b499c2eca61fdc0cc8d6de23caaae735463987da6e7c63db722f.jpg]]  
图6-12 配置请求报文头格式

配置请求 TLP 的第 4\~7 字节与存储器请求 TLP 类似。而第 8\~11 字节的 Bus、Device 和 Function Number 中存放该 TLP 访问的目标设备的相应的号码，而 Ext Register 和 Reigister Number 存放寄存器号。配置请求报文的其他字段必须为以下值。

- TC [2:0] 必须为 0, I/O 请求报文的传送类型 (TC) 只能为 0。  
- TH 位为保留位；Attr2 位为保留，而 Attr [1:0] 必须为 “00b”，这表示 I/O 请求报文使用 PCI 总线的强序数据传送模式；AT [1:0] 必须为 “0b00”，表示不进行地址转换。

- Length [9:0] 为 “0b00 0000 0001”, 表示配置读写请求最大 Payload 为 1DW。  
- Last DW BE 字段为“0b0000”。而 First DW BE 字段根据配置读写请求的大小设置。

# 6.3.4 消息请求报文

在PCIe总线中，多数消息报文使用隐式路由方式，其格式如图6-13所示。其中Byte0字段为通用TLP头，而Byte4的第3字节中存放MessageCode字段。

![[pci_express/c33ede13a99f160573489b8e09885dbb9e3d584f4b99011d08cc46c991eaa1b5.jpg]]  
图6-13 Message请求TLP头格式

PCIe总线规定了以下几类消息报文。

- INTx 中断消息报文（INTx Interrupt Signaling）。  
- 电源管理消息报文（Power Management）。  
- 错误消息报文（Error Signaling）。  
- 锁定事务消息报文（Locked Transaction Support）。  
- 插槽电源限制消息报文（Slot Power Limit Support）。  
- Vendor-Defined Messages。

本节将重点讲述 $\mathrm{INTx}$ 中断和错误信息相关的消息报文，请读者阅读PCIe总线规范了解其他消息报文。

# 1. INTx中断消息报文

PCIe总线推荐设备使用MSI或者MSI-X机制提交中断请求，但是MSI中断机制并不是由PCIe总线首先提出的，在PCI总线中就已经存在这种中断请求机制。

在PCI总线中，虽然提出了MSI中断机制，但是几乎没有PCI设备使用这种机制进行中断请求。MSI中断机制是一种基于存储器写的中断请求机制，而PCI设备提交MSI中断请求，将占用PCI总线的带宽，因此多数PCI设备使用INTx信号进行中断请求。

在PCIe总线中，PCIe设备可以使用Legacy中断方式提交中断请求，此时需要使用INTx中断消息报文向RC通知中断事件。除此之外在PCIe体系结构中仍然存在PCI设备，这些设备可能使用INTx信号提交中断请求。

例如在PCIe桥片上挂接的PCI设备可能并不支持MSI中断机制，因此需要使用 $\mathrm{INTx}$ 中断信号提交中断请求，此时PCIe桥需要将 $\mathrm{INTx}$ 信号转换为 $\mathrm{INTx}$ 中断消息报文，并向RC提交中断请求。在PCIe总线中，共有8种 $\mathrm{INTx}$ 中断消息报文，见表6-7。

表 6-7 INTx 中断消息报文

<table><tr><td>名称</td><td>Code [7:0]</td><td>Routing r [2:0]</td><td>Requester ID</td><td>描述</td></tr><tr><td>Assert_INTA</td><td>0010 0000</td><td>100</td><td>包括总线号和设备号,功能号保留</td><td>置INTA#信号有效。注意在PCIe总线中,并没有物理上的INTA#信号</td></tr><tr><td>Assert_INTB</td><td>0010 0001</td><td>100</td><td>同上</td><td>置INTB#信号有效</td></tr><tr><td>Assert_INTC</td><td>0010 0010</td><td>100</td><td>同上</td><td>置INTC#信号有效</td></tr><tr><td>Assert_INTD</td><td>0010 0011</td><td>100</td><td>同上</td><td>置INTD#信号有效</td></tr><tr><td>Deassert_INTA</td><td>0010 0100</td><td>100</td><td>同上</td><td>置INTA#信号无效</td></tr><tr><td>Deassert_INTB</td><td>0010 0101</td><td>100</td><td>同上</td><td>置INTB#信号无效</td></tr><tr><td>Deassert_INTC</td><td>0010 0110</td><td>100</td><td>同上</td><td>置INTC#信号无效</td></tr><tr><td>Deassert_INTD</td><td>0010 0111</td><td>100</td><td>同上</td><td>置INTD#信号无效</td></tr></table>

当PCIe设备不使用MSI报文向RC提交中断请求时，可以首先使用Assert\_INTx报文向处理器系统提交中断请求，当中断处理完毕，再使用Deassert\_INTx报文。这些INTx中断消息报文的r[2:0]字段为0b100，即为Local消息报文。设备收到该消息报文后，将结束收到的INTx中断消息报文，然后产生一个新的INTx中断消息报文。

在一个处理器系统中，PCI设备首先需要通过PCIe桥，之后可能通过多级Switch，最终到达RC。假设PCI设备使用INTA#信号进行中断请求，但是由于中断路由表的存在，PCIe桥可能将INTA#信号转换为INTB中断消息，而这个INTB中断消息通过Switch时，可能又被Switch的中断路由表转换为INTC中断消息。因此PCIe设备收到INTx中断消息后，首先需要结束当前中断消息，之后根据中断路由表产生一个新的INTx中断消息，直到这个中断消息传递到RC。

# 2. 错误消息报文

在第4.3.3节中简要介绍了AER Capability结构。如果PCIe设备支持AER Capability结构，当PCIe设备出现某种错误时，将向RC或者RC Event Collector发送错误消息报文，之后RC或者RC Event Collector将根据错误类型分别进行处理。

PCIe总线规范定义了两大类错误类型，分别是可恢复错误（Correctable Errors）和不可恢复错误（Uncorrectable Errors），不可恢复错误又细分为致命错误（Fatal）和非致命错误（Nonfatal）。当PCIe设备出现这些错误时，将使用ERR\_COR、ERR\_NONFATAL和ERR\_FATAL错误消息报文向RC或者RC Event Collector发送错误消息报文。

PCIe总线规范并没有详细描述“可恢复错误”和“不可恢复错误”的具体处理方法，也没有详细描述PCIe设备的错误恢复机制。对于PCIe设备，这些处理方法并不重要。PCIe总线定义AER机制的主要考虑是，由PCIe设备将错误信息“统一报告”给RC或者RC Event Collector，并由RC或者RC Event Collector“统一处理”这些错误。其中“统一报告”和“统一处理”才是AER机制的设计要点。

当PCIe设备出现某种错误后，首先将这些错误信息保留在设备的AER Capability结构中，之后RC或者RC Event Collector从“来自PCIe设备的错误信息报文”中获得相应的错误信息。为此在RC中设置了一个“Error Source Identification”寄存器保存究竟是哪个PCIe设备发出的错误信息报文，之后RC或者RC Event Collector向处理器系统提交中断请求，由

相应的中断服务例程统一处理所有PCIe设备的错误信息。

由RC统一处理所有PCIe设备错误信息的这种做法，势必加大RC和系统软件的设计复杂度。而外部设备的多样性与复杂程度决定了使用这种方法不一定能够取得较好的效果。目前Intel的Chipset已经支持AER机制，但是绝大多数PCIe设备并不支持AER机制。AER机制是Intel统一外部设备错误处理的一种方法，这为PCIe设备的设计提出了更高的要求，而这种方法是否能够取得理想的效果，仍需观察。

# 6.3.5 PCIe总线的原子操作

PCIe V2.1总线规范引入原子操作的概念，原子操作仅能在存储器访问中使用。其中RC和EP可以作为原子操作的请求者和接收者，而Switch和多端口RC支持原子操作的转发。PCIe总线支持三类原子操作，分别是EP-to-EP，EP-to-RC和RC-to-EP的原子操作。

原子操作有利于提高智能设备之间以及智能设备与处理器之间的数据传递效率。当智能设备与处理器进行数据交换时，将不可避免地使用某种锁机制访问临界资源。传统的做法是使用“带锁的”存储器总线事务实现这些锁机制。而使用原子操作可以在很大程度上降低“带锁的”存储器读写请求TLP的使用，从而提高PCIe总线的使用效率。

PCIe设备使用一次原子操作可以实现之前需要多次数据操作才能完成的数据交换任务，除此之外PCIe设备使用原子操作还可以避免使用带锁的PCIe总线事务。原子操作的基本过程如下所示。

(1) 源设备向目标设备发送原子操作请求 TLP。原子操作请求 TLP 使用 Non-Posted 方式进行数据传递，且使用基于地址的路由方式。  
(2) 当目标设备收到这个原子操作请求 TLP 之后，将从这个 TLP 指定的存储器空间中读取原始数据。  
(3) 目标设备将“原始数据”与“原子操作请求 TLP 中包含的操作数”进行某种规定的运算后产生一个新的数据。这一过程不可被其他总线事务中断，PCIe 设备保证这一过程为原子操作。这个步骤对于原子操作至关重要，也是原子操作的实现要点。  
(4) 当上述原子操作执行完毕后，目标设备使用原子操作完成报文向源设备传送数据，并将这个新的数据写入目标设备的存储器空间中。原子操作完成报文与存储器读完成的传递方式类似。

由以上分析，可以发现所谓原子操作是指PCIe设备“读取原始数据”、“运算”和“产生新的数据”这三个过程不可被其他操作打断。这三个过程将在目标设备中一次完成，并由目标设备的硬件逻辑保证这三个过程不会被其他TLP干扰。

由上文所述，一个完整的原子操作由原子操作请求 TLP 和完成 TLP 组成。其中原子操作请求 TLP 的报文头与存储器请求 TLP 类似，如图 6-8 所示。原子操作请求 TLP 具有 Data Payload 字段，在 Data Paylaod 中包含原子操作请求 TLP 使用的操作数。

目前，PCIe总线共支持3种原子操作，分别为FetchAdd、Swap和CAS原子操作。不同的原子操作使用的操作数个数并不相同，其中FetchAdd和Swap原子操作使用一个操作数，

而CAS原子操作使用两个操作数。

# 1. FetchAdd 操作

FetchAdd 操作支持 32b 或者 64b 的操作数。如果该 TLP 的 Length 字段为 1DW 时，操作数的长度为 32b；如果该 TLP 的 Length 字段为 2DW 时，操作数的长度为 64b。FetchAdd 操作的执行过程如下所示。

（1）PCIe设备从TLP的指定PCI总线地址中获得原始数据。  
(2) 将原始数据与 TLP 中的操作数相加，并得到一个新的数据。相加的结果忽略进位与溢出位。  
(3) 将这个新的数据写入 TLP 指定的 PCI 总线地址中。  
(4) 使用完成报文返回指定 PCI 总线地址中的原始数据。

# 2. Swap总线事务

Swap 操作也支持 32b 或者 64b 的操作数，其原则与 FetchAdd 操作完全一致。Swap 操作的执行过程如下所示。

（1）PCIe设备从TLP指定的PCI总线地址中读取原始数据。  
(2）将TLP中的操作数写入TLP指定的PCI总线地址。  
(3) 使用完成报文返回 PCI 总线地址中的原始数据。

# 3. CAS总线事务

CAS 操作支持 32b、64b 或者 128b 的操作数。如果该 TLP 的 Length 字段为 2DW 时，操作数的长度为 32b；如果该 TLP 的 Length 字段为 4DW 时，操作数的长度为 64b；如果该 TLP 的 Length 字段为 8DW 时，操作数的长度为 128b。CAS 总线事务含有两个操作数，分别为“Compare”和“Swap”。CAS 操作的执行过程如下所示。

（1）PCIe设备从TLP指定的PCI总线地址中获得原始数据。  
(2) 将原始数据与 “Compare” 操作数进行比较。  
(3) 如果结果相等，则将“Swap”操作数写入TLP指定的位置。  
(4) 使用完成报文返回 PCI 总线地址中的原始数据。

智能设备之间以及智能设备与处理器之间如果需要使用“Spin Lock”操作时，可以使用CAS原子操作实现。值得注意的是，在x86处理器的指令集中，也含有CAS类指令，该指令是实现“Spin Lock”的基础。但是在处理器系统中使用的“Spin Lock”操作与智能设备使用的“Spin Lock”操作在实现上有所不同。

