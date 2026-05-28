---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "09"
section: "第9章 流量控制"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第9章 流量控制

流量控制（Flow Control）的概念起源于网络通信。一个复杂的网络系统由各类设备（如交换机、路由器、核心网），与这些设备之间的连接通路组成。从数据传输的角度来看，整个网络中具有两类资源，一类是数据通路，另一类是数据缓冲。

数据通路是网络上最珍贵的资源，直接决定了数据链路可能的最大带宽；而数据缓冲是另外一个重要的资源。当一个网络上的设备从一点传送到另外一点时，需要通过网络上的若干节点才能最终到达目的地。在这些网络节点中含有缓冲区，暂存在这个节点中没有处理完毕的报文。网络设备使用这些数据缓冲，可以搭建数据传送的传送流水线，从而提高数据传送性能。最初在网络设备中只为一条链路提供了一个缓冲区，如图9-1所示。

![[pci_express/456966cc12f4c1b65a8f2bbaac70173f96717320490464bce0d8cfe3d6d1871f.jpg]]  
图9-1 基于单数据通路的数据传递

当网络设备使用单数据通路进行数据传递时，假设在该通路中正在传递两个数据报文，分别是A和B。其中数据报文B需要经过Node1\~5才能到达最终的目的地，而数据报文B在经过Node3时发现由于Node3正在向Node4发送一个数据报文A。从而数据报文B到达Node3后，由于Node4的接收缓存被数据报文A占用，而无法继续传递。此时虽然在整个数据通路中，Node4和Node5之间的通路是空闲的，但是报文B还是无法通过Node3和4，因为在Node4中只有一个数据缓冲，而这个数据缓冲正在被报文A使用。

使用这种数据传送规则会因为一个节点的数据缓冲被占用，而影响了后继报文的数据传递。为了解决这个问题，在现代网络节点中设置了多个虚通路VC，不同的数据报文可以使用不同的通路进行传递。从而有效解决了单数据通路带来的问题，基于多通路的数据传递如图9-2所示。

![[pci_express/29cfa580430ccc3c61c9f5d574cfeca7980f4d2a106e0d6bd7b48eb13f148c50.jpg]]  
图9-2 基于双数据通路的数据传递

如上图所示，所谓多通路是指在每一个节点内设置两个以上的缓存。上例中设置了两个缓存，报文A经过Node $1\sim 5$ 时使用缓存2进行缓存，然后进行数据传递，而报文B使用缓存1进行缓存。因此虽然报文A因为某种原因不能继续传递，也只是将报文阻塞在缓存2中，而不影响报文B的数据传递。

所谓VC是指缓存到缓存之间的传递通路。如图9-2所示的例子中含有两个VC，分别是VC1和VC2。其中VC1对应节点间缓存1到缓存1的数据传递，而VC2对应缓存2到缓存2的数据传递。VC间的数据传递，如Node1的缓存1/2到Node2的缓存1/2，都要使用实际的物理链路“链路1”，这也是将VC称为“虚”通路的主要原因。

在一个实际的系统中，虚通路的使用需要遵循一定的规则。如在PCIe总线中，将不同的数据报文根据TC分为8类，并约定这些TC报文可以使用哪些VC进行数据传递。在PCIe总线中使用TC/VC的映射表，决定TC与VC的对应关系。

PCIe总线规定同一类型的TC报文只能与一条VC对应，当然从理论上讲，不同的TC报文可以与不同的VC对应，也可以实现一种自适应的算法根据实际情况实现TC报文和VC的对应关系。只是使用这种方法需要付出额外的硬件代价，效果也不一定明显。

下文以图9-2所示的数据传递为例，进一步对此说明相同类型的报文使用不同VC的情况。假设报文A和B属于相同种类的报文，但是报文A使用VC1，而报文B使用VC2。首先报文A传递到Node4后被阻塞。而报文B使用的VC和报文A使用的VC不同，报文B最终也会到达Node4。

由于报文A和报文B属于相同类型的报文，Node4阻塞报文B的概率非常大，因为Node4已经阻塞了报文A。阻塞报文A的原因在很大概率上也会对报文B适用。此时两个虚通路都被同一种类型的报文阻塞，其他报文将无法通过。因此在实际应用中，相同类型的数据报文多使用同一个VC进行数据传递，而在PCIe总线中，一个TC只能对应一个VC。

目前多通路技术的应用已经普遍应用到网络传输中，虚通路是一种防止节点拥塞的有效方法。但是在网络传送中，还存在一种不可避免的拥塞现象，即某一条链路或者某个节点是整个系统的瓶颈。

我们假设图9-2中Node4将报文转发到Node5的速度低于Node3发送报文的速度。在这种情况下，Node4将成为整个传送路径上的瓶颈，无论Node4中的缓存1和2有多大，总会被填满，从而造成节点拥塞。

当缓存填满后，如果 Node 3 继续向 Node 4 发送报文时，Node 4 将丢弃这些报文，之后 Node 3 将会择时重发这个报文，而 Node 4 仍然会继续丢弃这个报文，这种重复丢弃的行为将极大降低网络带宽的利用率，而且 Node 3 也会成为网络中新的瓶颈，从而引发连锁反应，造成整个网络的拥塞。为了避免这类事件发生，网络中的各个组成部件需要对数据传送进行一定的流量控制，合理地接收和发送报文。

如上文所述，在网络中有两类资源，一类是数据通路，另一类是数据缓冲。而流量控制的作用是合理地管理这两类资源，使这些资源能够被有效利用。

# 9.1 流量控制的基本原理

目前流量控制从理论到实现大多基于多通道技术，本节也仅讨论基于多通道的流量控制（FCVC，Flow-Controlled Virtual Channels）的基本原理。

流量控制的主要作用是在发送端和接收端进行数据传递时，合理地使用物理链路，避免因为接收端缓冲区容量不足而丢弃来自发送端的数据，从而要求发送端重新发送已经发送过的报文，并最终有效地利用网络带宽。

目前几乎所有流量控制算法的核心都是根据接收端缓冲区的容量，向发送端提供反馈。而发送端根据这个反馈，决定向接收端发送多少数据。这些流量控制算法都力求发送的每一个数据报文都能够被接收端正确接收，而不会被接收端因为缓冲不足而丢弃。使用流量控制机制并不能提高网络的峰值带宽，相反还会降低网络的带宽，但是可以有效减少数据报文的重新发送，从而保证网络带宽被充分利用。

流量控制的目标是在充分利用网络带宽的前提下，尽量减少数据报文因为接收端缓存容量不足而被丢弃的情况，因为此时数据发送端将会择时重新传送这些丢弃的报文，从而降低了数据通路的利用率。

为了实现流量控制，数据接收端需要及时地向发送端反馈一些信息，发送端依此决定，是否能够向接收端继续发送数据。这些反馈信息也需要占用一定的数据通路带宽。但是采用这种方法可以有效避免数据报文的丢失与重发，从而在整体上提高了数据通路的利用率。流量控制针对端到端的数据传递，目前流行的流量控制方法共有两种，分别为Rate-Based机制和Credit-Based机制。

# 9.1.1 Rate-Based 流量控制

Rated-Based机制适合“可预知带宽”的数据传递方式，而Credit-Based机制更加适合“突发数据传送”。下面将以图9-3所示的实例简单介绍Rate-Based机制。

![[pci_express/4dd12d90838a8c75f10bbcbf4735aa679c7656fad3c17633adbc045007040713.jpg]]  
图9-3 Rate-based流量控制机制

假设 Node 1 与 Node 2 之间共存在 5 个 VC，即在链路 1 的两端设置了 5 组缓存。而这 5 个 VC 共享一个物理通路，即链路 1。为方便起见，假设链路 1 的带宽为 1，而在系统初始化时，将 VC1\~5 可以使用的带宽 BCVC 都设为 $1/4$ （即 Rate 值为 $1/4$ ），而且每一个 VC 使用的最大数据传送率不能超过 BCVC。

在某些情况下，由于在 Node 1 中，每个 VC 的 BCVC 最大值为 $1/4$ ，因此 Node 1 可以向 Node 2 发送的数据带宽总和为 $5/4$ ，大于链路 1 所能提供的峰值带宽，因此链路 1 将可能成为瓶颈，从而造成网络拥塞。此时来自 Node 1 的数据报文必然会阻塞在各自 VC 的发送缓冲中，并可能出现报文重传现象。

Rete-Based机制使用“自适应”调解的办法有效防止了这种网络拥塞。Rate-Based机制规定每一个VC在发送一定数量的报文后，将主动地将相应VC的Rate调整为Rate减去ADR（Additive Decrease Rate），直到Rate等于MCR（Minimum Cell Rate）；当Node2的

Egress端口并不拥塞时，Node 2 将向 Node 1 的对应 VC 发出正向反馈，通知该 VC 可以适当提高数据传送率，当 Node 1 的 VC 收到这个正向反馈后，将更新其 BCVC。

假设在本例中MCR为 $\mathbf{X}$ ，当链路1严重拥塞时，Node2不会向Node1的所有VC发出正向反馈，最终Node1所有VC的Rate都将降为MCR，此时Node1将不会向Node2发送过多的数据报文；当链路1并不拥塞时，Node2将向Node1的相应VC发出正反馈，通知Node1可以适当提高数据报文的数据传送率。

Rate-Based 流量控制机制可以使用漏桶（Leaky Bucket）算法或者令牌桶（Token Bucket）算法实现。使用令牌桶算法时，一个设备至少具有 MCR 个令牌，这个设备每发送一定数量的报文后，将令牌减少 ADR 个，但是总令牌数不低于 MCR。当这个设备收到下游设备的正反馈时，将增加令牌数。

采用Rate-Based流量控制机制可以有效解决“可预知带宽”的数据传递。比如Node1向Node2发送音频或者视频数据，这些音视频数据占用的数据带宽基本恒定，因此使用这种方法可以保证这类数据报文的流畅传递。

而对于多数长度“不可预知”的突发数据传递，该机制并不能完全适用。因为Rate-Based流量控制的实时性较弱，当一个VC需要瞬间传递大量报文时，Rate-Based机制不能及时地为这条VC提供足够的数据传送率；而当一个VC拥塞时，也不能及时地降低数据传送率。因此使用Rate-Based机制并不能满足网络上突发数据传送的需要，此时需要使用Credit-Based机制对流量进行控制。

# 9.1.2 Credit-Based 流量控制

为便于Credit-Based流量控制机制的讨论，假设在网络中存在三类节点，分别是Upstream节点、Current节点和Downstream节点，这些节点之间通过实际的物理链路互连，在每一个节点内部都使用两个VC，其结构如图9-4所示。

![[pci_express/5e03c35ec4198447e54614386b735426205564d1a01243c8a8bed4b71a5b6654.jpg]]  
图9-4 Upstream Current与Downstream节点的关系

其中 Upstream 节点通过物理链路将数据报文发向 Current 节点，而 Current 节点通过物理链路将数据报文发向 Downstream 节点。在虚通路的设计中，每个节点的发送端口和接收端口之间具有分属于不同 VC 的数据缓冲，这些数据缓冲之间的互连组成了不同的 VC。

Current节点首先将来自Upstream节点的报文暂存在数据缓冲中，然后再发送到Downstream节点。当Upstream节点通过Current节点，向Downstream节点发送数据报文时，流量控制发生在Upstream节点和Current节点、Current节点和Downstream节点之间，而不是Upstream节点到Downstream节点。简而言之，流量控制发生在链路的两端，基于端到端之

间，而不是基于节点到节点间。

在 Upstream 节点、Current 节点和 Downstream 节点中存在两个 VC，下文以其中的一个 VC 为例，说明如何使用 Credit-Based 机制进行数据传递。值得注意的是，Current 节点、Upstream 节点和 Downstream 节点只是一个相对概念。Current 节点也可以从 Downstream 节点接收数据报文，而向 Upstream 节点转发这些数据报文，从而组成一个双向通路。为简便起见，本章仅讨论在单向通路下，Credit-Based 流量控制机制的原理与实现。

Credit-Based机制基于“Credit”进行数据传递，当Upstream节点需要发送数据报文时，需要具备足够的Credit，才能向Current节点发送数据。这个Credit由Current节点提供，并与Upstream节点保存的Credit同步，为此Current节点需要定时向Upstream发送“传递Credit”的数据报文。

下文为简便起见，假设节点间传送的数据报文，其长度固定，而且每次只能传递一个数据报文。Credit-Based机制需要使用以下参数进行报文传递。

- Buf\_Alloc 参数。该参数保存在 Current 节点中接收数据缓冲的总大小。Upstream 节点能够发送的数据报文总数不能大于该参数。  
- Crd\_Bal 参数。该参数是 Upstream 节点可以发送的数据报文数，Current 节点需要定时向 Upstream 节点发送 Credit 报文。Upstream 节点收到该报文后，使用该报文中的 “Credit” 同步 Crd\_Bal 参数。Upstream 节点可以发送的数据报文数不能超过 Credit\_Bal 参数。  
- Tx\_Cnt 参数。该参数为 Upstream 节点已经发送的数据报文数。  
- Fwd\_Cnt 参数。该参数为 Current 节点转发到 Downstream 节点的数据报文数。

Credit-Based 流量控制使用的各个参数之间的关系如图 9-5 所示。

![[pci_express/d37e2d3fd6ae8a4a393b1a7181c8c51d2b383519c50f6d22bcf4224b00567518.jpg]]  
图9-5 使用Credit-Based机制进行数据传递

# 1.Upstream节点向Current节点发送报文

Upstream节点向Current节点发送报文时，Current节点必须有足够的缓冲，而且Current节点需要预先将其剩余的缓冲数量，即Credit（其值为一个正整数），及时地发送给Upstream节点。Upstream节点使用Crd\_Bal参数保存这个Credit。

Crd\_Bal 参数的初始值为 Buf\_Alloc，即 Current 节点能够接收的最大报文个数，该值在系统初始化时由 Current 节点发向 Upstream 节点。因此 Upstram 节点在流量控制机制初始化完毕后，Crd\_Bal 参数与 Current 节点中能够存放的最大报文数相同。

Upstream节点每成功发送一个数据报文后，Crd\_Bal值减1，当Crd\_Bal参数为0时，Upstream节点不能向Current节点发送数据报文。此时Upstream节点必须等待Current节点发

送Credit报文，更新Crd\_Bal参数后，才能继续发送数据报文。

# 2. Current节点向Upstream节点发送Credit

Current节点收到来自Upstream节点的数据报文后，将会根据链路的实际情况，将这些报文转发到Downstream节点。

假设在一段时间之内，Current节点共收到了Tx\_Cnt个报文，而转发了Fwd\_Cnt个报文，那么此时在Current节点中还能容纳Buf\_Aloc-（Tx\_Cnt-Cx\_Cnt)个报文空间。Current节点将这个值作为Credit，发送到Upstream节点。而Upstream节点将根据这个Credit的值更新Crd\_Bal参数。

# 3. Current节点将报文转发到Downstream节点

Current节点接收到报文之后再将其转发出去涉及一些路由算法。常用的算法有Cut-Through路由和Wormhole路由算法。

当使用 Wormhole 路由方式时，一个报文将被分解为若干个 Flit，包括 Header Flit、Data Flit 和 Tail Flit。当数据报文到达 Current 节点后，Current 节点立即对 Header Flit 进行分析，首先根据路由算法决定发向哪个 Downstream 节点。如果在对应的 Downstream 节点中，链路空闲且有足够的缓冲资源时，Current 节点将发送 Data Flit 直到 Tail Flit，即发送整个数据报文；如果对应的 Downstram 节点没有资源接收这个报文，数据报文将在 Current 节点中存储。

Cut-Through 路由与 Wormhole 路由类似。采用 Cut-Through 路由时，Downstream 节点必须具有接收整个报文的能力时，才能接收报文；而采用 Wormhole 路由时，Downstream 节点具有接收部分报文的能力时，就可以接收报文。采用 Wormhole 路由的优点是数据报文传送延时较短，每一个节点所需要的数据缓存相对较小。当网络发生拥塞时，采用 Wormhole 路由技术可能会使一个报文分别缓冲在 Current 节点和 Downstream 节点中，而使用 Cut-Though 路由技术数据报文最终缓冲在一个节点中。

巨型机一般使用 Wormhole 技术进行报文传递，而网络系统中多使用 Cut-Though 路由技术。有关 Wormhole 和 Cut-Though 技术的优劣分析超出了本书的范围，本书不会对此进行详细分析。巨型机应用针对的是可预知的网络拓扑结构，而网络系统的拓扑结构是变化的。在一个网络拓扑结构可预知的前提下，采用 Wormhole 技术可以在避免拥塞的前提下，降低网络报文的传递延时；而对于一个未知的网络拓扑结构，使用 Cut-Though 技术更为合理。

# 9.2 Credit-Based机制使用的算法

在第9.1.2节中提到的Credit-Based机制是一个较为理想的模型，在这个模型中，没有考虑网络的延时和拥塞，也没有考虑Current节点何时采用哪种策略将Credit传送给Upstream节点，同时也没有考虑Buf\_alloc缓冲是否会出现上溢出（Overrun）或者负载（Underrun）。本节将首先给出Overrun和Underrun的定义，然后讨论Credit-Based机制使用的各类算法以及这些算法中使用的各类参数。

\- 在本章中，Overrun 指 Current 节点没有足够的缓存接收来自 Upstream 节点的数据报

文；或者 Downstream 节点没有足够的缓存接收来自 Current 节点的数据报文。

\- 而 Underrun 指 Downstream 节点有足够的缓存可以接收 Current 节点的报文，但是 Current 节点的缓存中没有需要发送的报文；或者 Current 节点有足够的缓存可以接收 Upstream 节点的报文，但是在 Upstream 节点的缓存中没有需要发送的报文。

这两种溢出情况都将导致链路带宽的浪费，在实际设计中需要尽力避免这两种溢出。此外在一个设计中，还需要考虑链路的传送延时。由于传送延时的存在，Current节点向Upstream节点传送Credit信息时，这个Credit信息并不能瞬间到达，因而会造成这两个节点间，Credit的同步问题。如果一个设计将上述这些因素考虑进去，Credit-Based机制的实现更为复杂。为深入研究Credit-Based机制所使用的算法，我们首先定义以下系统参数。

# (1) $\mathbf{R}_{\mathrm{TT}}$ (Round Trip Time)

该参数记载数据通路的链路延时，单位为s（秒）。使用Credit-Based机制进行报文传递时，Upstream需要获得Credit然后才能发送报文。在链路中存在两个延时，一个是Current节点向Upstream节点发送Credit报文的线路延时 $\mathrm{T}_{\mathrm{CU}}$ ，另一个是Upstream节点向Current节点发送数据报文的延时 $\mathrm{T}_{\mathrm{UC}}$ 。

如果一个物理链路的发送与接收链路速度相等，而且Credit报文长度等于数据报文长度时， $\mathrm{T}_{\mathrm{CU}}$ 将等于 $\mathrm{T}_{\mathrm{UC}}$ 。但是在很多情况下这两个值并不相等。本章为简化起见，假设 $\mathrm{T}_{\mathrm{CU}}$ 与 $\mathrm{T}_{\mathrm{UC}}$ 的值相等。

而 $\mathbf{R}_{\mathrm{TT}}$ 是这两种延时之和， $\mathbf{R}_{\mathrm{TT}}$ 的值和物理链路的延时成正比。除此之外节点在发送数据报文时需要通过若干逻辑门，这也增加了传送延时。 $\mathbf{R}_{\mathrm{TT}}$ 的值可以在链路配置时计算出来，但是在具体实现中，设计者可能使用一个事先预估的数值作为 $\mathbf{R}_{\mathrm{TT}}$ 。值得注意的是，该参数不能估计得过低，否则将会造成Current节点的Overrun；也不能估计得过高，否则将可能引发Current节点的Underrun。

# (2) BLINK

该参数存放 Upstream、Current 和 Downstream 节点间数据传递的峰值带宽，即数据链路所能提供的最大物理带宽，单位为 bps（Bits Per Second）。为简便起见，本章认为这几个节点间进行数据传递时的峰值带宽相等。

# (3) Packet\_Size

该参数存放一个数据报文的大小，单位为 Bit。假定所有节点间进行数据通信时使用的数据报文的大小相等。值得注意的是，在 PCIe 总线中，数据报文并不等长，这为 PCIe 总线的流量控制带来了不小的麻烦。

# (4) F (In-flight Data)

该参数存放在 $\mathrm{R}_{\mathrm{TT}}$ 时间段内，Upstream节点和Current节点之间的双向链路上存在的报文，其单位为报文数。其最大值等于 $\mathrm{R}_{\mathrm{TT}} \times \mathrm{B}_{\mathrm{LINK}} / \mathrm{Packet\_Size}$ 。该值在链路进行远距离传送时必须要考虑。而PCIe链路通常在一个PCB内部，至多作为机箱到机箱之间的链路，因此RTT的值非常小，F参数几乎可以忽略不计。

# (5) $\mathrm{B_{VC}}$

该参数存放源节点到目标节点的有效数据带宽，单位为bps。在一个物理链路上除了要传递有效数据之外，还需要传递Credit报文，因此 $\mathrm{B_{VC} < B_{LINK}}$ 。

该参数不一定是一个常数，因为在一个实际的系统中，不同时间内的带宽并不一定相

等。为了简化模型，使用 $\mathrm{B_{VCR}}$ 参数替代 $\mathrm{B_{VC}}$ 参数， $\mathrm{B_{VCR}}$ 参数为一个常数。

本节力求简化流量控制的数学模型，并依此进行量化分析。有关流量控制的量化分析涉及一些相对复杂的数学推导，本章对此不做详细说明。

使用Credit-Based机制时，Buf\_Alloc参数可以被分解为三部分，分别由N1、N2和N3组成，Buf\_Aloc参数与 $\mathrm{N}1\sim 3$ 间的关系如图9-6所示。

![[pci_express/87d6799425694e104e2012cb54ee251961c0b17bdd6c0681d7f82b2ab3a0bb25.jpg]]  
图9-6Buf\_Alloc的组成

# （1）N1缓冲

该参数用来防止因为线路延时而造成的Overrun，Credit-Based流量控制机制所采用的算法对该参数的解释有略微区别，详见下文。

# (2）N2缓冲

N2的值与VC的设置有相关。当Current节点向Downstream节点每转发N2个报文后，将向Upstream节点发送一个Credit报文。Current节点可以将所有VC的N2值设为相同，也可以分别设置。

N2的值越大，Buf\_Alloc参数的值也越大，节点发送Credit报文的频率也越低，从而Credit报文占用数据通路带宽的比例越小；N2的值越小，则Buf\_Alloc的值也越小，向Upstream节点发送的Credit报文的频率越高，Credit报文占用数据通路带宽的比例也越大。如果N2的值为10，Current节点每转发10个报文后，才向Downstream发送一个Credit报文，从而整个系统用于传送Credit报文所占用的带宽不会超过 $10\%$ 。

# (3) N3 缓冲

该参数保证 Current 节点不会出现 Underrun，即出现 Downstream 节点仍有缓存接收报文，而在 Current 节点的缓存中没有报文需要发送这种情况。

为此在进行系统设计时，需要合理设置N3的值。使得在理想情况下，只要Upstream节点向Current节点不断地发送报文，且Downstream节点有足够的缓存时，就不会在Current节点中出现Underrun的现象。

当然Downstream节点接收数据报文的速度足够快，而且Current节点未能及时地从Upstream节点获得报文时，Current节点总会出现Underrun的现象。

假设Downstream节点从Current节点获取报文的速度为 $\mathrm{B_{VCR}}$ ，只要 $\mathrm{N3 = B_{VCR}\times R_{TT}/}$ Packet\_Size，那么在Downstream节点将N3中的报文取完之前，Current节点总能获得新的报文（其前提是Upstream节点在不断地向Current节点发送数据报文）。

由此可见 $\mathrm{B_{VCR}}$ 与N3的容量有关， $\mathrm{B_{VCR}}$ 越大，N3也越大。N3的容量可以影响数据通路的有效带宽，在流量控制机制的实现中，如果N3过小，那么Current节点将出现Under-

run 的现象，从而影响数据通路的有效带宽。根据上述数学模型，H. T. Kuang 与 Alan Chapman 提出了三种流量控制算法，分别为 N123、N123 + 和 N23 算法。

# 9.2.1 N123算法和N123+算法

N123算法的使用规则如下所示

(1) Current 节点发送上一个 Credit 报文之后，至少需要向 Downstream 节点转发 N2 个报文后，才能发送下一个 Credit 报文。  
(2) Credit 报文中存放的 Credit 为 Current 节点已经发送的报文数，其最大值为 $\mathrm{N}2 + \mathrm{N}3$ ，其中 N1 不参与“Credit”的计算，因为在 N123 算法中，N1 用来防止 Current 节点的溢出。当 Current 节点使用的缓冲超过 $\mathrm{N}2 + \mathrm{N}3$ 时，Current 节点不再向 Upstream 节点发送 Credit 报文，即便 Current 节点已经向 Downstream 转发了 N2 个报文。  
(3) Upstream 节点收到 Credit 报文后，使用 Crd\_Bal 参数保存这个报文中的 Credit，当 Crd\_Bal 参数不为 0 时，Upstream 节点可以发送数据报文。Upstream 节点每发送一个报文，Crd\_Bal 参数将减 1，当这个参数为 0 时，Upstream 节点停止发送报文。Crd\_Bal 参数的初始值为 $\mathrm{N}2 + \mathrm{N}3$ 。

采用以上算法时，必须保证 $\mathrm{N}1\leqslant \mathrm{R}_{\mathrm{TT}}\times \mathrm{B}_{\mathrm{LINK}} / \mathrm{Packet\_Size}$ ，此时Current节点的接收缓冲才不会溢出。下文将详细解释为什么N1的最小值为 $\mathrm{R_{TT}\times B_{LINK} / P a c k e t\_Size}$ ，并用一个实例说明N1最小值的设置原因，而不进行理论分析。

假设在某一个时间点，Upstream节点的Crd\_Bal参数为0，而Current节点的N2和N3缓冲区被完全占满，此时Upstream节点不能发送新的报文，直到获得新的Credit报文。之后Upstream、Current节点和Downstream节点按照以下步骤运行。

(1) 当 Current 节点向 Downstream 节点转发 $x (x \geqslant N2)$ 个报文后，将通过 Credit 报文将 $x$ 传递给 Upstream 节点。  
(2) 当 Upstream 节点收到这个 Credit 后，将更新 Crd\_Bal 参数为 x，之后可以向 Current 节点发送 x 个报文。这些报文将通过链路不断发向 Current 节点。  
(3) 假设此时 Current 节点接收到的报文个数为 $\mathrm{z1}(\mathrm{z1} \leqslant \mathrm{x})$ ，Current 节点在接收这些报文的同时，向 Downstream 节点又转发了 y 个报文，此时 Current 节点一共需要向 Upstream 节点发送的 Credit 为 $(\mathrm{x} + \mathrm{y} - \mathrm{z1})$ 。假定此时 Downstream 节点由于缓存不足，禁止接收来自 Current 节点的报文，Current 节点的空闲缓存将定格为 $\mathrm{x} + \mathrm{y} - \mathrm{z1}$ 。  
(4) 在 Current 节点向 Upstream 节点发送 Credit(该值为 $x + y - z1$ ) 的过程中, 该节点又陆续收到了一些报文, 其个数为 $z2$ 。值得注意的是 “Current 节点发送 Credit 报文” 到 “Upstream 节点收到这个报文” 有一段延时, 该延时等于 $\mathrm{T}_{\mathrm{CU}}$ 。  
(5) Upstream 节点收到新的 Credit 后, 将 $x + y - z1$ 个报文发向 Current 节点, 除此之外在从 Upstream 节点到 Current 节点间的链路上还留有一部分报文, 其个数为 z3。这段残留的报文数为在 $\mathrm{T}_{\mathrm{UC}}$ 时间段内传递的数据报文。  
(6）因此Current节点最终收到的报文个数为 $(\mathrm{x} + \mathrm{y} - \mathrm{z}1) + \mathrm{z}2 + \mathrm{z}3$ 个。   
(7) 其中 $x + y - z1$ 个报文可以被 Current 节点中空闲缓存吸收，而多出来的 $z2 + z3$ 个数据报文将放置到 N1 中。

$\operatorname{Max}(\mathrm{z}2 + \mathrm{z}3)$ 的计算方法如公式9-1所示。

$$
\begin{array}{l} \operatorname {M a x} (z 2 + z 3) = \left(T _ {\mathrm {C U}} \times B _ {\text {L I N K}} + T _ {\mathrm {U C}} \times B _ {\text {L I N K}}\right) / \text {P a c k e t ＿ S i z e} \\ = R _ {\mathrm {T T}} \times B _ {\mathrm {L I N K}} / \text {P a c k e t} \_ \text {S i z e} \tag {9-1} \\ \end{array}
$$

因此只要N1被设置为 $\mathrm{R_{TT}}\times \mathrm{B_{LINK}} / \mathrm{Packet\_Size}$ ，采用N123算法就一定不会在Current节点上产生Overrun。

通过以上分析，可以发现由于物理链路传送的延时，采用N123算法时，Current节点将多收到 $z2 + z3$ 个报文，其中z2是Current节点更新Upstream节点Credit的过程中产生；而z3是Upstream节点更新完毕Credit后，在网络线路上残留的报文。使用N123算法时，需要设置N1缓冲处理这些因为网络延时产生的报文。

以上过程并非严格的数学证明，只是使用较为简单的推理说明，在某些情况下，在Current节点中的N1至少为 $\mathrm{R_{TT} \times B_{LINK} / Packet\_Size}$ 时，才能够保证Current节点的接收缓冲不会溢出。希望读者认真理解N1缓冲在N123算法中的意义。

N123 + 算法基于 N123 算法，但是发送 Credit 报文的方式略有不同。N123 算法要求 Current 节点每转发 N2 个报文后，才能给 Upstream 节点发送一个 Credit 报文；而 N123 + 算法除了要求同样的规则之外，还要求 Current 节点在一个时间段 RTT 中至少发送一个 Credit 报文，即发送上一个 Credit 报文之后，即便 Current 节点没有向 Downstream 转发了 N2 报文，在一个时间段 RTT 中也至少要向 Upstream 节点发送一次 Credit 报文。

采用这种方法，因为在每一个 $\mathrm{R}_{\mathrm{TT}}$ 时间段里都会向Upstream节点发送Credit信息，因此F将小于这个Credit，而这个Credit又小于 $\mathrm{N2 + N3}$ 。为此使用这种方法时，N1的计算方法如公式9-2所示。

$$
N 1 = \operatorname {M i n} (N 2 + N 3, R _ {T T} \times B _ {L I N K} / P a c k e t \_ S i z e) \tag {9-2}
$$

使用这种方法，在 $(\mathrm{N}2 + \mathrm{N}3) < \mathrm{R}_{\mathrm{TT}} \times \mathrm{B}_{\mathrm{LINK}} / \mathrm{Packet\_Size}$ 时，Current节点将可以使用较小的N1缓冲，从而节约了接收缓冲。这种算法对于网络延时较长的通信网络有所帮助，而网络延时较短时，但是采用这种方法，发送Credit报文所占用的带宽较大。在PCIe总线中，端到端的延时相对较小，因此没有必要使用这种流量控制机制。

# 9.2.2 N23算法

N23算法是流量控制中常用的算法，使用该算法的优点是Current节点的缓存中不包含N1，从而降低了节点的缓存容量。该算法基于N123算法，区别在于使用该算法时Crd\_Bal参数的计算。基于该算法的实现方式如图9-7所示。

N23算法的使用规则如下。

- 当系统初始化时，Crd\_Bal 参数为 $\mathrm{N}2 + \mathrm{N}3 - \mathrm{E}$ ，E 为在时间段 $\mathrm{R}_{\mathrm{TT}}$ 中，Upstream 节点向 Current 节点发送的报文数，其值等于 $\mathrm{B_{VCR} \times R_{TT} / Packet\_Size}$ 。而 Upstream 节点每发送一个报文，Crd\_Bal 参数的值将减 1。当 Crd\_Bal 参数等于 0 时，Upstream 节点停止发送报文，直到重新获得 Current 节点的 Credit 报文，更新 Crd\_Bal 参数后，才能继续发送。  
- 使用 N23 算法时，Crd\_Bal 参数与 Current 节点发出的 Credit 并不相等，而是等于 Credit - E。  
- Current节点至少需要向Downstream节点发送N2个报文后，才能向Upstream节点发送Credit报文，与N123算法一致。

![[pci_express/5ca2c0783007fd454651b4f25202e8ad05fa3d9d0e4f1cad2760c1b044221a2f.jpg]]  
图9-7 基于N23算法的流量控制

通过以上介绍，可以发现之所以采用N23算法，不需要设置N1，是因为Upstream节点使用的Crd\_Bal参数与N123算法相比少E个包，所以N23算法虽然没有使用N1缓冲，也不会导致在传送过程中出现Overrun，因为采用N23算法，将N1隐含在E中。同时因为N3的存在，采用N23算法也不会导致在传送过程中出现Underrun。

综上所述，使用N23算法进行数据报文的传递时，只要N2和N3参数设置合理，将不会发生节点的Overrun和Underrun的情况。但是还需要继续讨论Current节点发送Credit报文时会不会引发Overrun和Underrun。

首先 Current 节点发送 Credit 报文不会引发 Upstream 节点的 Overrun，因为 Upstream 节点每次接收到新的 Credit 报文都会把 Crd\_Bal 参数更新，不可能因为 Credit 报文过多而无法处理。但是 Current 节点发送过多的 Credit 报文将严重影响物理链路的有效利用率，在流量机制的实现中，需要合理设置发送 Credit 报文的频率。

而 Current 节点向 Upstream 节点传递 Credit 报文延时过大时，可能会引发 Current 节点的 Underrun。在这种情况下，Upstream 节点虽然有很多报文等待发送，但是由于 Crd\_Bal 参数为 0，不能发送这些报文。

因此造成在 Current 节点的数据缓冲中，没有数据报文需要发向 Downstream 节点，尽管此时在 Current 节点中还有足够的缓存可以接收数据报文。在流量控制机制的设计中，需要考虑 Credit 报文的传送延时，合理设置 Current 节点中的缓冲，以保证 Upstream 节点在获得新的 Credit 之前，Current 节点的缓冲中具有一定的报文数，以避免 Underrun。

采用N23算法可以有效避免这种因为Credit报文传递不及时而引发的Underrun（N123算法与N23算法都使用了N3缓冲避免Current节点的Underrun）。我们首先基于N23算法做出以下假设以简化数学模型。

（1） $\mathrm{N3 = B_{VCR}\times R_{TT} / Packet\_Size}$ 。设置N3缓存的主要目的是保证Current节点不会出现Underrun。而 $\mathrm{B_{VCR}\times R_{TT} / Packet\_Size}$ 是N3缓存的最小值。  
(2）Upstream、Current和Downstream间VC的通信带宽为 $\mathrm{B}_{\mathrm{VCR}}$ ，其值为一个常数。  
(3) Downstream 节点始终有足够的缓冲接收报文。Current 节点可以通畅地将数据报文

转发到Downstream节点。

Upstream 和 Current 节点间的数据交换是基于“得到 Credit，然后发送数据”这样的循环。假定在一个发送循环的起始点中，Upstream 节点的 Crd\_Bal 参数为 N2，即 Upstream 节点刚收到的 Credit 为 $\mathrm{N}2 + \mathrm{N}3$ ，而采用 N23 算法时，Crd\_Bal = Credit - E = (N2 + N3) - E = N2。

我们定格 Upstream 节点刚刚收到 Current 节点 Credit 报文，更新 Crd\_Bal 参数完毕这个场景，其时间戳为 T2，而 Current 节点发送这个 Credit 报文的时间戳为 T1。假设从 T1 \~ T2 这段时间内，Current 节点收到 z2 个报文（Current 节点向 Upstream 节点发送 Credit 报文的过程中，仍然在持续地接收报文）。T1 \~ T3 的示意如图 9-8 所示。

![[pci_express/6d34e792a84ae9a55dadb54c6c2fd40b93acff4a55e662404043f1e16d659b3e.jpg]]  
图9-8 T1\~T3的示意图

由于各节点的带宽都为 BVCR，所以 Current 节点每收到一个报文，都会发送到 Downstream 节点，因此此时 Current 节点的可用缓存始终保持为 $\mathrm{N}2 + \mathrm{N}3$ 。Current 节点向 Upstream 节点发送的 Credit 报文始终为 $\mathrm{N}2 + \mathrm{N}3$ ，而 Upstream 节点收到这个 Credit 后，其 Crd\_Bal 参数将被置为 N2。

Upstream节点收到Credit报文后，开始向Current节点发送N2个报文，Upstream节点每发送一个报文，Current节点将收到一个报文（假设此时从Upstream节点到Current节点的链路之间已经堆积了z2个报文，此时物理链路已经充满数据报文）。

假设 Upstream 节点向 Current 节点发送完毕 N2 - z2 个报文（数据报文离开发送端口的时间戳为 T3）。那么从 T3 \~ T1 这段时间里，Current 节点将收到 N2 个报文，同时 Current 节点也将向 Downstream 节点转发完毕 N2 个报文，此时（T3 时间戳）Current 节点将向 Upstream 节点发送 Credit 报文（数值为 $\mathrm{N}2 + \mathrm{N}3$ ）。

而 Upstream 节点处于 T3 这个时刻时，还能向 Current 节点发送 z2 个报文，因为之前已经发送了 N2-z2 个报文，此时 Crd\_Bal 参数为 z2。等到 Upstream 节点将 z2 个报文发送完毕，来自 Current 节点的 Credit 报文恰好到达，因为 Upstream 节点将 z2 个报文发送完毕的时间刚好等于 Current 节点向 Upstream 节点发送 Credit 报文的时间。通过以上计算，可以发现采用 N23 算法不会因为 Credit 报文的传送时间而导致 Current 节点的 Underrun。

此外采用N23算法时还需要处理错误报文，N23算法规定当一个节点收到一个错误数据报文时，将丢弃这个报文，此时这个被丢弃的报文将不占用接收节点的缓存，但是发送节点的Crd\_Bal参数仍然需要考虑这个报文。

