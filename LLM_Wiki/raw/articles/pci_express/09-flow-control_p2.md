---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "09"
section: "9.2.3 流量控制机制的缓冲管理"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 9.2.3 流量控制机制的缓冲管理

上文讲述了基于单个VC的流控机制，实际上在Upstream、Current和Downstream节点中一般含有多个VC。多个VC之间如何合理地使用缓存值得重点关注，在实际设计中，可以为每一个VC设置独立接收缓存，也可以使多个VC共享同一个接收缓存。在FCVC的实现中，可以根据实际情况选择独立缓存或者共享缓存。

其中，每一个VC都使用独立接收缓存的流量控制方法称为静态（Static）流量控制；

而使用共享缓存的流量控制方法称为自适应（Adaptive）流量控制。

假定在一个系统中，一共具有 $n$ 条VC，而且这几条VC都使用N23算法进行流量控制，那么在使用Static流量控制方式时，该系统一共需要的缓冲大小为 $(n \times N2 + N3) \times \text{Packet\_Size}^{\ominus}$ 。如果 $(n \times N2 + N3) \times \text{Packet\_Size}$ 并不是很大时，为了使数据链路获得更大的带宽，可以使用Static流量控制。使用这种方法，也将极大地简化缓冲管理的设计难度。

值得注意的是，在一个系统工程的架构设计中，应当重点关注“Critical Path”的设计，需要容忍非“Critical Path”的不完美。当 $(\mathrm{n} \times \mathrm{N}2 + \mathrm{N}3) \times \mathrm{Packet\_Size}$ 的值大到了可以容忍的范围之外时，设计者必须考虑如何减少 Current 节点的接收缓存大小。Static 流量控制是针对每一个 VC 都是按照全负荷运转的情况，在绝大多数应用中，几乎不可能出现每一条 VC 都被充分利用的情况，因为多条 VC 共享一个物理链路，不可能出现所有 VC 都在全负荷运行的情况。为此在系统设计时可以使用 Adaptive 流量控制方法。

Adaptive 流量控制的本质是 Current 节点中，所有 VC 共享一个接收缓存，从而这个缓存可以远小于 $(\mathrm{n} \times \mathrm{N}2 + \mathrm{N}3) \times \mathrm{Packet\_Size}$ 。因为在绝大多数时间内，数据链路的多条 VC 不可能都被充分使用，因此并不需要为每条 VC 都提供 N2 缓冲，而是为所有 VC 统一提供接收缓冲，从而合理使用这些接收缓冲。

目前接收缓存的分配常使用两种算法，分别是Sender-Oriented和Receiver-Oriented管理算法。这两种算法的缓冲设置如图9-9所示。使用Sender-Oriented管理算法时，AdaptiveBuffer的分配在Upstream节点中完成，如果系统中有多个Upstream节点，Current节点需要在其接收端点处为每个Upstream节点准备AdaptiveBuffer，而且Current节点并不知道Upstream节点的使用情况，这为Current节点的缓冲管理带来了不小的困难。

![[pci_express/cce623da97875a2341ac9396ed915f5e0807786e7f388a3fedc9fd9dbf4cb1b3.jpg]]  
图9-9 Sender-Oriented和Receiver-Oriented管理算法

而使用 Receiver-Oriented 管理算法可以有效避免这类困难，使用这种算法时，所有来自 Upstream 节点的数据报文在 Current 节点的发送端准备一个 Adaptive Buffer，对这个 Buffer 的分

配也在 Current 节点内部完成。这两种算法的具体实现都较为复杂，本节并不详细介绍这些算法，在 PCIe 总线中，RC 和 Switch 的硬件设计将会涉及这些内容，而 EP 无需关心这些问题。

图中的上半部分是Sender-Oriented管理算法的示意图，而下半部分是Receiver-Oriented管理算法的示意图。由图9-9可以发现，使用这两种算法时Adaptive Buffer都在Current节点中，只是位置不同。

# 9.3 PCIe总线的流量控制

我们仍然使用上文的 Upstream、Current 和 Downstream 节点模型分析 PCIe 总线使用的流量控制机制。在 PCIe 总线中，Upstream 节点和 Downstream 节点可以为 RC 或者 EP，而 Current 节点只能为 Switch 或 $\mathrm{RC}^{\ominus}$ 。

此外在PCIe总线中，允许Upstream节点和Downstream节点直接相连，而不需要经过Current节点，如RC的某个端口可以和EP直接相连。当然这种情况也可以理解为Upstream和Current节点直接相连，但是Current节点不需要与Downstream节点相连。

与传统的流量控制机制相比，PCIe总线的流量控制机制有所不同。流量控制机制首先出现在互联网中，使用流量控制机制最典型的应用是基于ATM（Asynchronous Transfer Mode）的分组交换网。在ATM分组交换网系统中，数据传递以报文为单位进行，每一个报文都可以独立地通过分组交换网，到达目的地。而PCIe总线的数据传递基于节点到节点间的数据传递，一个完整的PCIe总线传输需要使用多个报文，而且这些报文和报文之间还有一定的联系，如一个完整的存储器读由存储器读请求TLP和存储器读完成TLP组成。

在PCIe总线中，RC端口和EP之间可以直接互连，而不需要中间节点，这和基于分组交换的网络有很大的不同。此外在PCIe总线中，报文的大小并不固定，如数据报文的大小可以为128B、256B，只要数据报文的有效负载小于Max\_Payload\_Size参数即可。

这些长度不确定的数据报文，为PCIe总线的流量控制带来了不小的困难，也是因为这个原因，PCIe总线的流量控制机制将一个TLP分为TLP头和Payload两部分，并分别为这两部分提供不同的接收缓冲，以合理利用PCIe链路的带宽。

PCIe总线的这些特性实际上不利于流量控制的实现，为此PCIe总线在传统流量控制理论的基础上，做出了许多改动。在PCIe总线中存在多条VC，其流量控制也是基于FCVC的，但是PCIe总线在接收缓冲的设计上与传统的流量控制机制有很大的不同。

PCIe总线的主要应用领域在PC或者服务器中进行板内互连，在这个应用领域中，流量控制并不是最重要的。PCIe总线的流控机制远非完美，这在某种程度上影响了PCIe总线在大规模互连结构中的使用。但是PCIe总线的流量控制机制仍有其闪光之处，因此读者仍有必要了解PCIe总线的流量控制机制。

与传统流量控制机制相比，PCIe总线在实现流量控制机制时需要更多的接收缓存，因此PCIe总线实现流控的代价相对较大。值得庆幸的是，目前PCIe总线上的设备，包括RC、EP和Switch，很少有支持两个以上VC通路的。一般来说PCIe总线上的设备，如RC和

Switch上也只有两个VC，多数EP仅支持一个VC。

PCIe总线的流量控制机制由事务层和数据链路层协调实现，而对系统软件透明。PCIe总线使用Credit-Based流量控制机制，其中Credit报文以DLLP的形式从Current节点反馈到Upstream节点。在PCIe总线中，数据报文首先以TLP的形式通过数据链路层，而到达数据缓存时被分解为Header和Data两个部分，分别存放到不同的接收缓存队列中。

# 9.3.1 PCIe总线流量控制的缓存管理

在PCIe总线的节点中，一个VC的接收缓存由PH（PostedHeader）缓存、PD（PostedData）缓存、NPH（Non-PostedHeader）缓存、NPD（Non-PostedData）缓存、CplH（CompletionHeader）缓存和CplD（CompletionData）缓存组成。

- PH 缓存存放存储器写请求 TLP 和 Message 报文使用的 TLP 头。  
- PD 缓存存放存储器写请求 TLP 和 Message 报文使用的 Payload。  
- NPH 缓存存放 Non-Posted 请求 TLP 使用的 TLP 头。  
- NPD 缓存存放 Non-Posted 请求 TLP 使用的 Payload。在 Non-Posted 请求 TLP 中，如存储器读请求 TLP 并不含有 Payload 字段，但是 I/O 和配置写请求 TLP 使用 Payload 字段。  
- CplH 缓存存放完成报文使用的 TLP 头。  
- CplD 缓存存放完成报文使用的 Payload。如上文所述，完成报文分为两大类，带数据的完成报文和不带数据的完成报文。其中不带数据的完成报文不需要使用 CplD 缓存。

在PCIe总线中，一个TLP从Upstream节点传送到Current节点时，必须同时具备多个缓存的Credit后才能发送。如存储器写请求TLP，需要同时具备PH和PD缓存的Credit，才能发送；而“不带数据的”存储器读完成TLP，仅需要具备CplH缓存即可。这些缓存在PCIe设备中的组成结构如图9-10所示。

![[pci_express/bbc9adc8a8c062fa04bf27d952b440464aa2e7cdd3ad788f81519fe407c2d473.jpg]]  
图9-10 PCIe总线Current节点的缓冲管理

如表6-2所示，PCIe总线根据Type字段将TLP分为15种，而根据这些TLP的传输特性，可以将这些TLP分为PostedTransaction、Non-PostedTransaction和Completion三大类。在PCIe总线中，这三大类数据传送需要遵循各自的规则，这些Transaction也有各自的特点。这三大类Transaction在进行数据传递时需要使用不同的缓存，这些缓存由多个单元（Unit）组成。每个Unit的大小与缓存类型相关，如表9-1所示。

表 9-1 PCIe 总线缓存的单元大小

<table><tr><td>类 型</td><td>Unit 大小</td></tr><tr><td>PH 缓存</td><td>5 个 DW, Posted Transaction TLP 头的最大值为 4DW,再加上一个 Digest</td></tr><tr><td>PD 缓存</td><td>4 个 DW</td></tr><tr><td>NPH 缓存</td><td>5 个 DW, Non-Posted Transaction 的 TLP 头的最大值为 4DW,再加上一个 Digest</td></tr><tr><td>NPD 缓存</td><td>1 个 DW。NPD 缓存用于 IO 和配置读写的数据</td></tr><tr><td>CplH 缓存</td><td>4 个 DW。3DW 的完成报文的头,加上一个 Digest</td></tr><tr><td>CplD 缓存</td><td>4 个 DW。该缓存至少能够存放一个完整的带数据的完成报文。其总单元数的最小值为 min(RCB/4B, Max_Payload_Size/4B)</td></tr></table>

PCIe总线将Header和Data缓存分离的主要原因是，一个TLP的Header大小是固定的，如PH和NPH大小在5DW之内，CplH大小在4DW之内；而Data的大小并不固定，除了NPD的大小为1个DW之外，其他数据报文的长度由TLP的Length字段确定，并不固定。PCIe总线将Header和Data缓存分离有利于Data缓存的合理使用。

PCIe总线规范将这些缓存使用的Unit统称为FC（Flow Control）Unit，下文将以FC Unit简称这些对应缓存的Unit。因为Header的大小固定，所以Header缓存能够精确地预知可以容纳几个Header；而由于Data的大小并不固定，Data缓存无法精确预知可以存放几个Data，当然Data缓存也不可能将Data的基本单位设置为Max\_Payload\_Size参数。因为这样做不仅不合理，而且非常浪费资源。将Header和Data缓存分离，有利于Data缓存使用类似Adaptive流量控制的方法使所有Data共用一个缓存，从而提高了Data缓存的利用率。但是也造成TLP因为不能同时具有Head和Data缓存，而无法传送。

在PCIe总线中，不同的TLP使用对应缓存的Unit数量也不同，Current节点有时需要两种缓存才能接收一个TLP。不同TLP使用的缓存数目如表9-2所示。

表 9-2 不同 TLP 使用的缓存

<table><tr><td>TLP</td><td>缓存种类</td></tr><tr><td>存储器、IO和配置读请求</td><td>使用1个NPH单元。Non-Posted数据请求的TLP类型通过完成报文获得数据</td></tr><tr><td>存储器写请求</td><td>使用1个PH单元和若干PD单元。存储器写请求使用的PD单元与Payload的长度相关</td></tr><tr><td>配置、I/O写请求</td><td>使用1个NPH单元和1个NPD单元。Non-Posted数据传送类型通过完成报文通知I/O写结束</td></tr><tr><td>不带数据的消息请求</td><td>使用1个PH单元</td></tr><tr><td>带数据的消息请求</td><td>使用1个PH单元和若干个PD单元</td></tr><tr><td>存储器读完成</td><td>使用1个CplH单元和若干CplD单元。读完成报文使用的CplD单元与Payload的长度相关</td></tr><tr><td>I/O和配置读完成</td><td>使用1个CplH单元和1个CplD单元</td></tr><tr><td>配置、I/O写完成</td><td>使用1个CplH单元</td></tr></table>

由上表所示，一个 TLP 可能需要使用两种缓存，如存储器写请求需要使用 PH 和 PD 缓存。这也意味着在 PCIe 设备的 VC 中，缓存之间存在依赖关系。当 Upstream 节点向 Current 节点进行存储器写时，发送方必须同时具有 PH 和 PD 两个缓存的 Credit 才能进行；而向 Current 节点发送读完成 TLP 时，Upstream 节点必须同时具有 CplH 和 CplD 两个缓存的 Credit 才能进行。这为 PCIe 总线的流量控制带来了额外的麻烦。

此外，在PCIe总线中，进行存储器写和存储器读完成TLP时，究竟需要多少个PD或者CplD单元是随TLP而变的，VC无法预知确切的单元数量。无论VC采用何种缓冲分配策

略，这种“不可预知”都会给流量控制带来巨大的麻烦。

报文长度的不确定性为PCIe总线流量控制机制带来了许多难以解决的问题。造成这种现象的主要原因是PCIe总线源于PCI总线，其主要应用来自PC领域而不是通信领域。PCIe总线为了向前兼容PCI总线，做出了许多功能上的牺牲。

PCIe总线使用Credit-Based流控机制，Upstream节点在发送TLP时，必须首先获得Current节点相应缓存的Credit。如Upstream节点发送存储器写请求时，需要同时具有Current节点中PH缓存和PD缓存的Credit，才能进行。

# 9.3.2 Current节点的Credit

PCIe总线规范没有强行规定采用哪种算法实现Credit-Based流量控制机制，因此PCIe总线上的Current节点可以选择使用合理的Credit-Based流量控制算法，如上文提到的N123算法或者 $\mathrm{N}123+$ 算法。PCIe总线规范没有规定，各类节点如何处理因为链路延时而产生的额外报文和在链路上残留的报文。

在PCIe流量管理中还存在一个问题就是PCIe总线上各个节点所采用的流量控制算法并不完全统一，虽然PCIe总线对此进行了一定程度的约定，但是这个约定较为模糊。因为在PCIe总线中，Upstream节点、Current节点和Downstream节点可能来自不同的生产厂商。虽然这种流量控制算法的“不统一”会为流量控制的整体效率带来影响，也可能会造成接收缓冲的浪费。

但是这种“不统一”并不会严重影响PCIe总线的流量控制机制。因为PCIe总线的流量控制基于“链路到链路”进行的，只要链路的两端采用相同的协议满足PCIe总线的基本约定即可。PCIe总线上的流量控制机制以Intel的RC实现作为事实标准。

在PCIe总线中，Credit-Based流量控制的数据传送规则仍然是Upstream节点获得Credit，之后向Current节点发送数据；而Current节点将一定数量的报文（N2）转发到Downstream节点之后，将向Upstream节点反馈Credit。PCIe总线规范也将向Upstream节点反馈Credit的过程称为Advertisement。

PCIe总线规范并没有规定如何设置Credit，但是规定了Credit在初始化之后的最小值，即Current节点发向Upstream节点的Credit的最小值，如表9-3所示。PCIe总线中，不同的缓存使用不同的Credit值。

表 9-3 PCIe 总线初始化后 Credit 的最小值

<table><tr><td>Credit 的类型</td><td>Credit 的最小值</td></tr><tr><td>PH 缓存</td><td>Credit (PH) 的最小值为 0x01, 即一个 PH 单元, 大小为 5DW</td></tr><tr><td>PD 缓存</td><td>Credit (PD) 的最小值为 Max_Payload_Size/Unit(PD)</td></tr><tr><td>NPH 缓存</td><td>Credit (NPH) 的最小值为 0x01, 即一个 NPD 单元, 大小为 5DW</td></tr><tr><td>NPD 缓存</td><td>Credit (NPD) 的最小值为 0x01, 即一个 NPH 单元, 大小为 1DW。值得注意的是原子操作请求使用的 Credit (NPD) 可以为 2DW</td></tr><tr><td>CplH 缓存</td><td>如果 Current 节点不是 “最终” 节点, 则 Credit (CplH) 的最小值为 0x01, 即一个 CplH 单元, 大小为 4DW; 否则 Credit (CplH) 为 0, 该值为 0, 表示 “不限带宽 (Infinite FC Unit)”, 其详细解释见下文</td></tr><tr><td>CplD 缓存</td><td>如果 Current 节点不是 “最终” 节点, 则 Credit (CplD) 的最小值为 Max_Payload_Size/Unit(CplD)①; 否则 Credit (CplH) 为 0</td></tr></table>

① 注意不是RCB/Unit（CPLD）。第一个“最终”节点的RCB并不相等，但是都小于Max\_Payload\_Size。

“最终节点”共有两类组成，一个是 EP，因为当一个报文到达 EP 后，将不会被再次转发，此时 FC Unit 为 0，表示该节点将无条件接收来自 Upstream 节点的报文，而且保证一定不会出现 Overrun 的现象，这就是 PCIe 总线规范提出的 Infinite FC Unit 的含义。

这也意味着 EP 在发起存储器、I/O 和配置读请求时，必须为存储器、I/O 和配置读完成报文预留必要的缓存，保证这些完成报文一定能够被 EP 接收。值得注意的是，只有设备在接收完成报文时，才存在 Infinite FC Unit 的概念。

在许多应用中，“最终节点”支持这种“Infinite FC Unit”将会影响PCIe链路的效率。假设一个PCIe设备进行DMA读，从主存储器获得数据，如果该PCIe设备支持Infinite FC Unit，其传送流程如下所示。

（1）PCIe设备向RC连续发送存储器读请求 $\mathrm{TLP}$   
(2) PCIe 设备每发送一个读请求 TLP 时，将从接收缓冲中为读完成 TLP 预留空间。因为如果 PCIe 设备支持 Infinite FC Unit，必须能够接收这些读完成 TLP。  
(3) 当 PCIe 设备的接收缓冲使用完毕后, 将不能发送存储器读请求 TLP。  
(4) PCIe 设备接收来自 RC 的读请求完成 TLP，并将其传送到上层，同时释放该读完成 TLP 使用的接收缓存。  
(5) PCIe 设备获得可用的接收缓存后，可以继续发送存储器读请求 TLP。直到完成所有数据请求。  
(6) PCIe 设备获得所有存储器读完成 TLP 后, 结束整个 DMA 读过程。

由以上过程可以发现在（3）和（5）中，由于接收缓冲不足，整个DMA读过程无法形成连续的流水操作，从而影响DMA读的数据传送效率。如果PCIe设备的接收缓冲无限大时，可以合理地解决这个问题，但是更为合理的方法是确定接收缓冲的最小值。接收缓冲的最小值由PCIe链路的延时决定，有关PCIe总线延时的详细分析见第12.4节。

还有一类“最终节点”是RC，但RC并非在任何情况下都是最终节点。如果多端口RC的各个端口之间不支持转发（即PCIe总线规范中的Peer-to-Peer传送方式）时，RC将成为“最终节点”；如果多端口RC的各个端口之间支持转发，RC也可能成为“最终节点”。数据通过多端口RC的流程如图9-11所示。

![[pci_express/829d4420d3baaf2713ce90b39c560d329b6b32ec0af4f25f86c3c00a87a9c004.jpg]]  
图9-11 多端口RC的数据传递

假设在一个 RC 中有 3 个下游端口，分别是端口 1，2，3，这些端口与 EP 或者 Switch 相连；还有一个上游端口，端口 4，与 FSB 相连。如果 RC 支持端口转发，当到达端口 1 的

TLP 的目的地是和端口 2 或者 3 相连的某个设备时，Credit（CplH）的最小值为 1，而 Credit（CplD）的最小值为 Max\_Payload\_Size/Unit（CplD），此时 RC 并不是最终节点。

如果到达端口1的TLP，其目的地是端口4时，RC就是“最终”节点，此时Credit（CplH）和Credit（CplD）的最小值为0，流量控制机制不起作用，来自Upstream节点的报文可以不进行流量检测，直接进入RC，而RC必须保证有足够的缓存接收这些报文。这也意味着RC发起读请求报文时，必须保证在RC中有足够的缓冲接收完成报文。

如果 RC 支持端口间的相互转发，必须设置必要的缓冲以支持流量控制机制，此时 RC 可能成为某个 TLP 的 Current 节点，因此 RC 不将 Credit（CplH）和 Credit（CplD）设为 0，此时使用流量控制机制进行报文转发。

# 9.3.3 VC的初始化

PCIe总线使用FCP（Flow Control Packets）传递Credit信息，FCP是一种DLLP，该报文的使用与事务层的接收缓存直接相关，但是对事务层透明，该报文产生于数据链路层，终止于数据链路层。PCIe总线定义了以下FCP，如表9-4所示。

表 9-4 PCIe 总线定义的 FCP

<table><tr><td>DLLP 类型</td><td>编 码</td></tr><tr><td>InitFC1-P</td><td> $0b0100 0V_2V_1V_0^1$ </td></tr><tr><td>InitFC1-NP</td><td> $0b0101 0V_2V_1V_0$ </td></tr><tr><td>InitFC1-Cpl</td><td> $0b0110 0V_2V_1V_0$ </td></tr><tr><td>InitFC2-P</td><td> $0b1100 0V_2V_1V_0$ </td></tr><tr><td>InitFC2-NP</td><td> $0b1101 0V_2V_1V_0$ </td></tr><tr><td>InitFC2-Cpl</td><td> $0b1110 0V_2V_1V_0$ </td></tr><tr><td>UpdateFC-P</td><td> $0b1000 0V_2V_1V_0$ </td></tr><tr><td>UpdateFC-NP</td><td> $0b1001 0V_2V_1V_0$ </td></tr><tr><td>UpdateFC-Cpl</td><td> $0b1010 0V_2V_1V_0$ </td></tr></table>

① $\mathrm{V}_2\mathrm{V}_1\mathrm{V}_0$ 与VC对应，PCIe总线规定VC个数的最大值为8，因此使用3位存放VC号。

由上表所示 FCP 共分为三大类 InitFC1、InitFC2 和 UpdateFC。在这三类报文中有 3 个重要的字段。其中 HdrFC 字段存放 Header 的 Credit；DataFC 字段存放 Data 的 Credit；而 VC ID 字段存放不同的 VC 号。其报文格式如图 9-12 所示。

![[pci_express/df4d950c829b7efa1a526aeb05521a0bd7219473a4628b68f8f7cf3c540ac50f.jpg]]  
图9-12 FCP的格式

Current节点使用以上报文向Upstream节点发送Credit信息，其中InitFC1和InitFC2与VC的初始化相关，而UpdateFC负责向Upstream节点反馈Credit信息。我们首先讲述Current节点如何使用InitFC1和InitFC2报文初始化VC的流量控制，并在第9.3.4节讲述如何使用UpdateFC报文实现PCIe总线的流量控制。

在各个节点能够正常使用之前，首先需要对VC0进行初始化。当VC0初始化完毕之后，PCIe设备可以对VC1\~7进行初始化。在VC0初始化完成之前，设备不能接收任何TLP，而VC0初始化完毕后，PCIe总线的相应节点在使用VC0接收TLP的同时，初始化其他VC。在VC的初始化过程中存在两个状态，FC\_INIT1和FC\_INIT2。

PCIe总线的数据链路层共有三个状态，分别为DL\_Inactive（PCIe链路无效状态，链路不可用或者链路上没有连接有效设备）、DL\_Init（PCIe链路可用，此时将进行VCO的初始化）和DL\_Active（链路可以正常使用）。

当Current节点的数据链路层进入DL\_Init状态时，该节点的VC0将进入FC\_INIT1状态。Current节点将在FC\_INIT1状态首先初始化VC0，之后才能初始化其他VC。其他VC的使能位在Current节点的配置空间中，当系统软件打开这些使能位时，Current节点将初始化其他VC。

当 Current 节点的 VC 进入 FC\_INIT1 状态时，事务层需要首先禁止该 VC 发送数据报文，随后 Current 节点将向 Upstream 节点依此发送 InitFC1-P、InitFC1-NP 和 InitFC1-Cpl<sup>⑦</sup> 报文初始化 Upstream 节点使用的 Credit。

此时 Current 节点还可能收到来自 Downstream 节点的 InitFC1-P、InitFC1-NP 和 InitFC1-Cpl 报文，并初始化 Current 节点的 Credit。当 Current 节点收到这些来自 Downstream 节点的 FCP 后，将设置对应缓冲的 FI1 状态位为 1。

当 Current 节点所有缓存的 FI1 状态位有效后，VC 将进入 FC\_INIT2 状态。Current 节点进入 FC\_INIT2 状态之前，Upstream 节点获得的 Credit 和 Current 节点的空余缓存大小相等，Current 节点获得的 Credit 和 Downstream 节点的空余缓存大小相等。这一点和其他流量控制机制类似，即 Crd\_Bal 的初始值为 Buf\_Alloc。

PCIe总线还提供了FC\_INIT2状态，该状态的主要功能是验证FC\_INIT1的结果。当节点进入FC\_INIT2状态时，与流量控制相关的缓存已经初始化完毕。PCIe总线设置FC\_INIT1和FC\_INIT2这两个状态与数据链路层的状态机相关。

如第7.1.1节所示，当数据链路层处于DL\_Init状态时，将初始化PCIe总线的流量控制机制。当VC处于FC\_Init1状态时，数据链路层通知事务层DL\_Down状态位有效，此时事务层不能向对端设备发送TLP，从而流量控制机制的初始化可以在一个“相对没有干扰的环境”下进行。

而当 Current 节点的 VC 进入 FC\_INIT2 状态时，事务层需要首先禁止使用这条 VC 发送报文，之后 Current 节点向 Upstream 节点依此发送 InitFC2-P、InitFC2-NP 和 InitFC2-Cpl 报文初始化 Upstream 节点的发送缓冲。当 Upstream 节点收到这些报文之后，将丢弃这些报文中包含的 Credit 信息，并设置相应的 FI2 状态位。

同理 Current 节点也将收到来自 Downstream 节点的 InitFC2-P、InitFC2-NP 和 InitFC2-Cpl

报文，并设置 Current 节点的 FI2 状态位。当所有数据缓存的 FI2 状态位有效后，将完成 PCIe 链路流量控制机制的初始化。最后数据链路层通知事务层 DL\_Active 状态位有效，此时事务层可以使用这个 VC 发送 TLP。

# 9.3.4 PCIe设备如何使用FCP

PCIe总线完成流量控制的初始化之后，Current节点、Upstream节点和Downstream节点通过发送UpdateFC-P、UpdateFC-NP和UpdateFC-Cpl报文进行流量控制。

# 1.Upstream节点向Current节点发送报文

Upstream节点向Current节点发送报文时，需要设置一些参数。

（1）CREDITS\_CONSUMED，简称为 CC。该参数存放 Upstream 节点已经发送了多少个 FC Unit，不同数据缓存的 FC Unit 的大小见表 9-3。在 PCIe 设备初始化时该参数为 0，之后 PCIe 设备每发送一个 FC Unit，该值将加 1。PCIe 总线规定

$$
\mathrm {C C} = (\mathrm {C C} + \text {I n c r e m e n t}) \mod 2 ^ {\text {F i e l d S i z e}}
$$

其中Increment指发送的TLP含有的FCUnit个数。其中PH、NPH和CplH的FieldSize参数为8，而PD、NPD和CplD的FieldSize参数为12。

(2) CREDIT\_LIMIT，简称为CL。该参数存放当前节点的Credit，在VC初始化时，该参数通过收到的InitFC1报文赋值。此后每收到UpdateFC报文时，Current节点将此参数与UpdateFC报文中的Credit比较，如果结果不等，则使用UpdateFC报文中的Credit对此参数重新赋值。

在 Upstream 节点中，设置了一个 Credit 检查逻辑（Transmitter Gating Function），用来判断在 Current 节点中是否有足够的缓存接收即将发送的 TLP。如果 Upstream 节点没有足够的 Credit 时，则不能向对端设备发送这个 TLP。

Upstream节点检查缓存的算法如下。首先将CUMULATIVE\_CREDITS\_REQUIRED参数简称为CR，而 $\mathrm{CR} = \mathrm{CC} + < \mathrm{PTLP}$ （即将发送TLP所需要的Credit）>。当以下公式

$$
(\mathrm {C L} - \mathrm {C R}) \mathrm {M o d 2} ^ {\text {F i e l d S i z e}} \leqslant 2 ^ {\text {F i e l d S i z e}} / 2
$$

成立时，表示 Upstream 节点可以向 Current 节点发送 TLP 报文。

使该公式成立有一个额外需求，即每次发送的Credit小于 $2^{\text{Field Size}} / 2^{\ominus}$ ，此时CL不可能比CC大 $2^{\text{Field Size}} / 2$ 。当（CL-CR）Mod $2^{\text{Field Size}}$ 不大于 $2^{\text{Field Size}} / 2$ 时，表示Current节点有足够的缓冲接收来自Upstream节点的报文；如果（CL-CR）Mod $2^{\text{Field Size}}$ 大于 $2^{\text{Field Size}} / 2$ 时，运算结果是一个负数，表示Current节点没有足够的缓冲接收来自Upstream节点的报文。

# 2. Current节点从Upstream节点接收报文

Current节点接收来自Upstream节点发送报文时，也需要设置一些参数。

(1) CREDITS\_ALLOCATED，简称为 CA。该参数存放 Current 节点一共允许 Upstream 节点发送多少个 FC Unit，不同数据缓存的 FC Unit 初始化后使用的最小值见表 9-3。在初始化时该参数为 Current 节点接收缓冲的大小，之后 Current 节点每分配一个 FC Unit，该值将加

1。PCIe总线规定：

$$
\mathrm {C A} = (\mathrm {C A} + \text {I n c r e m e n t}) \mod 2 ^ {\text {F i e l d S i z e}}
$$

其中Increment指Current节点新分配的FCUnit个数。PH、NPH和CplH的FieldSize参数为8，而PD、NPD和CplD的FieldSize参数为12。Current节点使用UpdateFC DLLP将CA传递给Upstream节点，之后Upstream节点使用该值更新CL参数。

(2) CREDIT\_RECEIVED, 简称为 CRCV。该参数存放 Current 节点一共接收了多少个 FC Unit。在初始化时该参数为 0 , 之后 Current 节点每接收一个 FC Unit, 该值将加 1 。PCIe 总线规定:

$$
\operatorname {C R C V} = (\operatorname {C R C V} + \text {I n c r e m e n t}) \mod 2 ^ {\text {F i e l d S i z e}}
$$

其中Increment指Current节点新接收的FCUnit个数。

Current节点可以设置一个逻辑检查部件，检查来自Upstream节点的TLP报文是否会造成CA的溢出。这个逻辑检查部件是一个可选件，因为Upstream节点在发送TLP时，已经保证了Current节点的CA不会溢出。当以下公式

$$
\text {(C A - C R C V)} \text {M o d} 2 ^ {\text {F i e l d S i z e}} \geqslant 2 ^ {\text {F i e l d S i z e}} / 2
$$

成立时，Current节点认为CA溢出，此时Current节点将抛弃来自Upstream节点的TLP，而并不改变CRCV的值，同时释放为这个TLP预先分配的缓存空间。

# 9.4 小结

本章仅使用了较少的篇幅讲述PCIe总线的流量控制机制，而重点讨论通用流量控制机制的基本原理。PCIe总线由于强调与PCI总线的兼容，流量控制机制的设计并不完美。PCIe总线的主要应用领域依然是PC，在这个领域中，流量控制并不是最重要的。

