---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "07"
section: "第7章 PCIe总线的数据链路层与物理层"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第7章 PCIe总线的数据链路层与物理层

PCIe总线的数据链路层处于事务层和物理层之间，主要功能是保证来自事务层的TLP在PCIe链路中的正确传递，为此数据链路层定义了一系列数据链路层报文，即DLLP。数据链路层使用了容错和重传机制保证数据传送的完整性与一致性，此外数据链路层还需要对PCIe链路进行管理与监控。数据链路层将从物理层中获得报文，并将其传递给事务层；同时接收事务层的报文，并将其转发到物理层。

与事务层不同，数据链路层主要处理端到端的数据传送。在事务层中，源设备与目标设备间的传送距离较长，设备之间可能经过若干级Switch；而在数据链路层中，源设备与目标设备在一条PCIe链路的两端。因此本章在描述数据链路层时，将使用发送端与接收端的概念，而不再使用源设备与目标设备。

物理层是PCIe总线的最底层，也是PCIe总线体系结构的核心。在物理层中涉及许多与差分信号传递有关的模拟电路知识。PCIe总线的物理层由逻辑层和电气层组成，其中电气层更为重要。在PCIe总线的物理层中，使用LTSSM状态机维护PCIe链路的正常运转，该状态机的迁移过程较为复杂，在第8章将详细介绍该状态机。

# 7.1 数据链路层的组成结构

数据链路层使用 ACK/NAK 协议发送和接收 TLP，由发送部件和接收部件组成。其中发送部件由 Replay Buffer、ACK/NAK DLLP 接收逻辑和 TLP 发送逻辑组成；而接收部件由“Error Check”逻辑、ACK/NAK 发送逻辑和 TLP 接收逻辑组成。数据链路层的拓扑结构如图 7-1 所示。在该图中含有两个 PCIe 设备，分别为 Device A 和 Device B，使用的 PCIe 链路为 Device A 的发送链路，同时也为 Device B 的接收链路。

![[pci_express/7fa999a67cc81d350b6705dbf7558f119adf4120c11afa564a69009cc3ffe9d5.jpg]]  
图7-1 数据链路层的拓扑结构

实际上每个PCIe设备的数据链路层都含有发送部件和接收部件。而上图为简化起见，仅含有DeviceA的发送部件和DeviceB的接收部件，即DeviceA发送链路两端使用的两个

部件。Device A 和 Device B 也具有接收部件和发送部件，这两个部件由 Device B 的发送链路使用，Device B 发送链路的工作原理与 Device A 类似，本节对此不做详细介绍。

当PCIe设备进行数据传递时，首先在事务层中产生TLP，然后通过事务层将这个TLP发送给数据链路层，数据链路层将这个TLP加上Sequence前缀和LCRC后缀后，首先将这个TLP放入到ReplayBuffer中，然后再发送到物理层。

目标设备（Device B）从物理层接收 TLP 时，将首先获得带前后缀的 TLP，该 TLP 经过数据链路层传递给事务层时，将被去掉 Sequence 前缀和 LCRC 后缀。在数据链路层中，TLP 的格式如图 7-2 所示。

![[pci_express/c7bb7bdee3df908e1f95250096e659da5c16244913823fb96cddbb786a4f8219.jpg]]  
图7-2 数据链路层TLP的格式

数据链路层使用 ACK/NAK 协议保证 TLP 的正确传送，ACK/NAK 协议是一种滑动窗口协议，该协议的详细介绍见第 7.2 节。其中 Sequence 前缀存放当前 TLP 的序列号，滑动窗口协议需要使用这个序列号。该序列号可以循环使用，但在同一个时间段内，一条 PCIe 链路不能含有 Sequence 前缀相同的多个 TLP。而 LCRC 后缀存放当前 TLP 的校验和。

PCIe总线的数据链路层使用ReplayBuffer和ErrorCheck部件共同保证数据传送的可靠性和完整性。来自事务层的TLP首先暂存在ReplayBuffer中，然后发送到目标设备。源设备的数据链路层根据来自目标设备的ACK/NAK DLLP报文决定是重发这些TLP，还是清除保存在ReplayBuffer中的TLP。

Replay Buffer 的大小决定了事务层可以暂存在数据链路层的报文数，Replay Buffer 的容量越大，在 PCIe 设备发送流水线中容纳的报文越多，从而也容易保证流水线不会因为发送部件出现 underrun 而中断，但是 Replay Buffer 的容量越大，占用的系统资源也越多，从而影响 PCIe 设备的功耗。在一个实际应用中，芯片设计者需要根据 PCIe 链路的延时确定数据链路层 Replay Buffer 的大小，在第 12.4.1 节中将进一步介绍 Replay Buffer 的大小与 PCIe 链路延时间的关系。

在PCIe设备的数据链路层中，还含有一个Error Check单元。PCIe设备使用Error Check单元检查接收到的TLP，并决定如何向对端设备进行报文回应。如果TLP被正确接收，PCIe设备将向对端设备发送ACK DLLP；如果TLP没有被正确接收，PCIe设备将向对端设备发送NAK DLLP。

除了 ACK/NAK DLLP 之外，数据链路层还定义了一系列数据链路层报文 DLLP，以保证 PCIe 链路的正常工作。这些 DLLP 都产生于数据链路层，并终止于数据链路层，并不会传送到事务层。有关 DLLP 格式的详细描述见第 7.1.3 节。

# 7.1.1 数据链路层的状态

数据链路层需要通过物理层监控PCIe链路的状态，并维护数据链路层的“控制与管理

状态机”（Data Link Control and Management State Machine，DLCMSM）。DLCMSM 状态机可以从物理层获得以下与当前 PCIe 链路相关的状态。

- DL\_Inactive 状态。物理层通知数据链路层当前 PCIe 链路不可用。在当前 PCIe 链路的对端没有连接任何 PCIe 设备，或者没有检测到对端设备的存在时，数据链路层处于该状态。  
- DL\_Init 状态。物理层通知数据链路层当前 PCIe 链路可用，且物理层正处于链路初始化状态，此时数据链路层不能接收或者发送 TLP 和 DLLP。此时 PCIe 链路首先需要初始化 VCO 的流量控制机制，然后再对其他虚通路进行流量控制的初始化。有关流量控制的详细描述见第 9 章。  
- DL\_Active 状态。当前 PCIe 链路处于正常工作模式。此时物理层已完成 PCIe 链路训练或者重训练。有关链路训练的详细描述见第 8 章。

DLCMSM 状态机的迁移模型如图 7-3 所示。

![[pci_express/1114c3a6b2baef54b8660e733ed75acd45bfe756526331e8fc52b0417e7831f5.jpg]]  
图7-3 DLCMSM的状态机模型

DLCMSM 状态机除了可以使用上述状态位，从物理层获得当前 PCIe 链路状态外，还可以使用以下状态位，向事务层通知数据链路层所处的状态。事务层通过这些状态位获知数据链路层所处的工作状态。

- DL\_Down。数据链路层处于该状态时，表示在PCIe链路的对端没有发现其他设备。当数据链路层处于DL\_Inactive状态时，该状态位有效。值得注意的是DL\_Down有效时，并不意味着对端不存在物理设备。数据链路层仅是使用该状态位通知事务层，暂时没有从对端中发现PCIe设备，需要进一步检测。  
- DL\_Up。数据链路层处于该状态表示在PCIe链路的对端连接了其他设备。当数据链路层处于DL\_Active状态时，该状态位有效。

当数据链路层收到物理层的状态信息后，DLCMSM状态机将进行状态转换，并向事务层通知PCIe链路的状态。如果在PCIe链路的两端都连接着PCIe设备，那么这两个PCIe设备的数据链路层，在绝大多数时间内状态相同。数据链路层各个状态的详细说明，及PCIe链路的状态迁移过程如下。

# 1. DL\_Inactive 状态

当PCIe设备复位时，将进入该状态。值得注意的是，只有传统复位方式才能使PCIe设备进入DL\_Inactive状态，而FLR方式并不会影响DLCMSM状态机。

当PCIe设备从复位状态进入DL\_Inactive状态时，将对PCIe数据链路层进行彻底复位，将与PCIe链路相关的寄存器置为复位值，并丢弃在Replay Buffer中保存的所有报文。当PCIe设备处于DL\_Inactive状态时，数据链路层将向事务层提交DL\_Down状态信息，并丢弃来自数据链路层和物理层的所有TLP，而且不接收对端设备发送的DLLP。

PCIe设备的物理层设置了一个LinkUp位，该位为1时表示PCIe链路的对端与一个PCIe

设备相连。当物理层的 LinkUp 状态位为 1，而且事务层没有禁用当前 PCIe 链路时，PCIe 数据链路层将从 DL\_Inactive 状态迁移到 DL\_Init 状态。

PCIe 设备在进行链路训练时，将检查 PCIe 链路的对端是否存在 PCIe 设备，如果对端不存在 PCIe 设备，物理层的 LinkUp 位将为 0，此时数据链路层将一直处于 DL\_Inactive 状态。系统软件可以设置 Switch 下游端口 Link Control 寄存器的“Link Disable”位为 1，禁用该端口连接的 PCIe 链路，此时即便 PCIe 链路对端存在 PCIe 设备，数据链路层的状态也仍然为 DL\_Inactive。

# 2. DL\_Init状态

当数据链路层处于DL\_Init状态时，将对PCIe链路的虚通道VC0进行流量控制初始化。在PCIe总线中，流量控制的初始化分为两个阶段，分别为FC\_INIT1和FC\_INIT2。在流量控制的FC\_INIT1阶段，数据链路层将向事务层提交DL\_Down状态信息；而在流量控制的FC\_INIT2阶段，数据链路层将向事务层提交DL\_Up状态信息。流量控制的初始化部分详见第9.3.3节。

当PCIe链路处于DL\_Down状态时，发送端可以丢弃任何没有被ACK/NAK确认的TLP，此时数据链路层几乎不会受到事务层的干扰，从而可以保证流量控制初始化的正常进行。这也是PCIe链路的流量控制分为FC\_INIT1和FC\_INIT2的主要原因。

当VC0的流量控制初始化完毕，而且物理层的LinkUp状态位为0b1时，数据链路层将从DL\_Init状态迁移到DL\_Active状态；如果在进行流量控制初始化时，物理层的LinkUp状态位被更改为0b0时，数据链路层将从DL\_Init状态迁移到DL\_Inactive状态。

# 3. DL\_Active 状态

当数据链路层处于DL\_Active状态时，PCIe链路可以正常工作，此时数据链路层可以从事务层和物理层正常接收和发送TLP，并处理DLLP，此时数据链路层向事务层提交DL\_Up状态信息。

当发生以下事件后，数据链路层可以从DL\_Active状态迁移到DL\_Inactive状态，但是不能迁移到DL\_Init状态。这也意味着数据链路层从DL\_Active状态迁移出去后，必须重新进行对端设备的识别和流量控制初始化，之后才能进入DL\_Active状态。

在多数情况下，数据链路层从DL\_Active状态迁移到DL\_Inactive状态时，意味着处理器系统出现了异常，系统软件需要处理这些异常。但是在下列情况时，数据链路层状态从DL\_Active状态迁移到DL\_Inactive状态时并不会引发异常。

- Bridge Control Register 的 Secondary Bus Reset 位被系统软件置为 1 时，数据链路层将迁移到 DL\_Inactive 状态。  
- Link Disable 位被系统软件置为 1 时，数据链路层迁移到 DI\_Inactive 状态。  
- 当一个PCIe端口向对端设备发送“PME\_Turn\_Off”消息之后，其数据链路层经过一段时间，可以迁移到DL\_Inactive状态。RC和Switch在进入低功耗状态之前，将向其下游端口广播PME\_Turn\_Off消息，下游PCIe设备收到该消息后，将向RC和Switch发出PME\_TO\_Ack回应。当RC和Switch的下游端口收到这个回应报文后，数据链路层可以迁移到DL\_Inactive状态。  
- 如果 PCIe 链路连接了一个支持 “热插拔” 功能的 PCIe 插槽, 而当这个插槽的 Slot Capability 寄存器的 “Hot Plug Surprise” 位为 1 时, 数据链路层将迁移到 DL\_Inactive

状态。

\- 如果 PCIe 链路连接一个热插拔插槽，当这个插槽的 Slot Control 寄存器的 “Power Controller Control” 位为 1 时，数据链路层也将迁移到 DL\_Inactive 状态。

在PCIe总线中，还有一些系统事件也可以引发数据链路层的状态转换，本书对此不进行一一描述。

# 7.1.2 事务层如何处理DL\_Down和DL\_Up状态

当事务层收到数据链路层的DL\_Down状态信息时，表示出现了以下情况。

- PCIe 链路的对端没有连接设备。  
- PCIe 链路丢失了与对端设备的连接。  
- 数据链路层和物理层出现某种错误，PCIe链路不能正常工作。  
- 系统软件禁用 PCIe 链路。

当事务层收到DL\_Down状态信息后，将不再从数据链路层中接收TLP，除非是数据链路层已经使用ACK/NAK报文确认过的TLP。这些被确认过的TLP已经被数据链路层接收完毕，因此事务层可以接收这些TLP。

当链路处于DL\_Down状态时，RC或者Switch的下游端口将复位与链路相关的内部逻辑和状态。此时下游端口收到上游端口的Non-Posted请求TLP后，并不会将这个TLP转发到数据链路层，因为数据链路层已经出现故障，而组织状态位为UR（UnsupportedRequest）的完成报文，通知上游端口无法发送这个Non-Posted数据请求，该事务层将丢弃这个Non-Posted请求TLP；此外该事务层还将丢弃来自上游端口的Posted请求TLP和完成报文。当链路为DL\_Down状态时，RC或者Switch的下游端口还必须结束“PME Turn\_Off”握手请求。

当链路为DL\_Down状态时，Switch和PCIe桥的上游端口，将复位相关的内部逻辑和状态，并丢弃所有正在处理的TLP。此时Switch和PCIe桥将使用Hot Reset方式复位所有下游端口。

事务层处于DL\_Up状态时，表示该设备与PCIe链路的对端设备已经建立连接，链路两端可以正常收发报文。当事务层发现PCIe链路从DL\_Down迁移到DL\_Up状态时，将向PCIe链路的对端设备重新发送Set\_Slot\_Power\_Limit消息，并重新初始化相关的寄存器。

# 7.1.3 DLLP的格式

DLLP与TLP的概念并不相同，DLLP产生于数据链路层，终止于数据链路层，这些报文不会出现在事务层中，而且对系统软件透明。设置DLLP的目的是为了保证TLP的正确传送和管理PCIe链路。

值得注意的是，DLLP并不是由TLP加上Sequence前缀和LCRC后缀组成的，而具有单独的格式。一个DLLP由6个字节组成，其中第1个字节存放DLLP的类型，第 $2\sim 4$ 个字节存放的数据与DLLP类型相关，而最后两个字节存放当前DLLP的CRC校验。DLLP的格式如图7-4所示。

![[pci_express/2303165c849f68b2ed527d664127bb166d34bf3e69e16a3d97f374449041fde7.jpg]]  
图7-4 DLLP的格式

大多数 DLLP 由 PCIe 设备自动产生，而与事务层没有直接联系。PCIe 总线定义了以下几类 DLLP 报文，如表 7-1 所示。

表 7-1 DLLP 的编码

<table><tr><td>Type字段</td><td>DLLP类型</td></tr><tr><td>0000-0000</td><td>Ack</td></tr><tr><td>0001-0000</td><td>Nak</td></tr><tr><td>0010-0000</td><td>PM_Enter_L1</td></tr><tr><td>0010-0001</td><td>PM_Enter_L23</td></tr><tr><td>0010-0011</td><td>PM_Active_State_Request_L1</td></tr><tr><td>0010-0100</td><td>PM_Request_Ack</td></tr><tr><td>0011-0000</td><td>Vendor Specific-Not used in normal operation</td></tr><tr><td>0100-0V2V1V0</td><td>InitFC1-P,其中V[2:0]由三位组成,因为一条PCIe链路最多支持8个VC</td></tr><tr><td>0101-0V2V1V0</td><td>InitFC1-NP</td></tr><tr><td>0110-0V2V1V0</td><td>InitFC1-Cpl</td></tr><tr><td>1100-0V2V1V0</td><td>InitFC2-P</td></tr><tr><td>1101-0V2V1V0</td><td>InitFC2-NP</td></tr><tr><td>1110-0V2V1V0</td><td>InitFC2-Cpl</td></tr><tr><td>1000-0V2V1V0</td><td>UpdateFC-P</td></tr><tr><td>1001-0V2V1V0</td><td>UpdateFC-NP</td></tr><tr><td>1010-0V2V1V0</td><td>UpdateFC-Cpl</td></tr></table>

这些DLLP报文的描述如下。

- ACK DLLP。该 DLLP 由接收端发送向发送端。接收端收到 TLP 报文后，将根据数据链路层的阈值设置，向对端设备发送 ACK DLLP，而不是每接收到一个 TLP，都向对端发送一个 ACK DLLP。该 DLLP 表示接收端正确收到来自对端的 TLP。  
- NAK DLLP。该DLLP由接收端发向发送端。该DLLP表示接收端有哪些TLP没有被正确接收，发送端收到NAK DLLP后，将重传没有被正确接收的TLP，同时释放已经被正确接收的TLP。ACK和NAK DLLP与ACK/NAK协议相关，是数据链路层的两个重要DLLP。这两个DLLP的详细作用如第7.2节所述。  
- Power Management DLLPs。PCIe 设备使用该组 DLLP 进行电源管理，并向对端设备通知当前 PCIe 链路的状态。PCIe 总线还定义了一组与电源管理相关的 TLP，这些 TLP

与这组 DLLPs 有一定的联系，但是其作用并不相同。PCIe 总线使用该组 DLLP 保证电源管理状态机的正确运行。  
- Flow Control Packet DLLPs。该组 DLLP 包括 InitFC1、InitFC2、UpdateFC DLLP，PCIe 总线使用这些 DLLPs 进行流量控制。在 PCIe 总线中，数据传送由三大类组成，分别为 Posted、Non-Posted 和 Completion。这三种数据传送方式有些细微区别，PCIe 设备为这三种数据传送设置了不同的数据缓冲。流量控制是 PCIe 总线的一个重要特性，第 9 章将重点介绍这些内容。  
- Vendor-specific DLLP。一些定制的 DLLP，PCIe 总线规范并未对此约束。这些 DLLP 由用户自定义使用。

本节将重点介绍 ACK/NAK DLLP，这两个 DLLP 与 PCIe 总线的 ACK/NAK 协议直接相关。在 PCIe 总线中，数据链路层使用 ACK/NAK 协议保证 TLP 的可靠传送。ACK/NAK DLLP 的格式如图 7-5 所示。

![[pci_express/49e67faa1146001de1c7074db58c88d9f9e816f091625a76c9d262f50cb77eb3.jpg]]  
图7-5 ACK/NAK DLLP

ACK/NAK DLLP 各字段的详细说明如表 7-2 所示。

表 7-2 ACK/NAK DLLP 的详细说明

<table><tr><td>名称</td><td>位置</td><td>描述</td></tr><tr><td>DLLP Type</td><td>Byte 0 的 7 ~ 0 位</td><td>0b0000-0000 表示为 ACK 报文
0b0001-0000 表示为 NAK 报文</td></tr><tr><td>AckNak_Seq_Num</td><td>Byte 2 的 3 ~ 0 位，Byte 3 的 7 ~ 0 位</td><td>该字段表示接收端成功接收的报文序号，下文将详细解释该字段。</td></tr><tr><td>CRC</td><td>Byte 4 ~ Byte 5</td><td>保存 DLLP 的 CRC 校验和。</td></tr></table>

发送端的数据链路层负责将TLP传送给接收端，而接收端的数据链路层在收到TLP之后，将向发送端发送ACK/NAK DLLP。发送端和接收端通过某种传送协议，完成数据链路层的数据交换，在PCIe总线中，这个协议称为ACK/NAK协议。

# 7.2 ACK/NAK协议

ACK/NAK协议是一种滑动窗口协议。PCIe设备的发送端和接收端分别设置了两个窗口。发送端在发送TLP时，首先将这个TLP放入发送窗口中（这个窗口即Replay Buffer），并对这些TLP从 $0\sim \mathrm{n}$ 进行编号。只要发送窗口不满，发送端就可以持续地从事务层中接收报文，然后将其放入Replay Buffer中。

发送端需要保留在这个窗口中的数据报文，并在收到来自接收端的 ACK/NAK 确认报文

之后，统一释放保存在发送窗口中的报文，并滑动这个发送窗口。当发送端收到接收端对第 $n$ 个报文的确认后，表示第 $n$ 、 $n-1$ 、 $n-2$ 等在窗口中的报文都已经被正确收到，然后统一滑动这个窗口。PCIe总线使用这种方法可以提高窗口的利用率。

与此对应，接收端也维护了一个窗口，该窗口记录数据报文的发送序列号范围。当数据报文到达后，如果其序列号在接收窗口范围内，接收端将接收该报文，并根据根据实际情况，向发送端发送回应报文。这个回应报文包括 ACK 和 NAK DLLP。下文将分别讨论发送端和接收端如何使用 ACK/NAK 协议。

# 7.2.1 发送端如何使用ACK/NAK协议

数据链路层在发送TLP之前，发送端首先需要将TLP进行封装，加上Sequence前缀和LCRC后缀，之后再将这个TLP放入ReplayBuffer中。发送端设置了一个12位的计数器NEXT\_TRANSMIT\_SEQ，这个计数器的初始值为0，当数据链路层处于DL\_Inactive状态时，该计数器将保持为0。为简化起见，本节只讲述数据链路层处于DL\_Active状态时的情况，而不讲述处于DL\_Inactive状态时的情况。

发送端使用计数器 NEXT\_TRANMIT\_SEQ 的当前值设置 TLP 的 Sequence 号，该计数器的初始值为 0。PCIe 设备每发送完毕一个 TLP，这个计数器将加 1，直到该计数器的值为 4095（NEXT\_TRANMIT\_SEQ 的最大值）。当计数器的值为 4095 后，再进行加 1 操作时，该计数器将回归为 0。而 LCRC 是根据 TLP 的内容计算出来的，用来保证数据传递的完整性，本节不介绍 LCRC 的计算过程。对此有兴趣的读者请参考 PCIe 总线规范。

与此对应，接收端也设置了一个12位的计数器NEXT\_RCV\_SEQ。这个计数器记录接收端即将接收的TLP的Sequence号。这个计数器的初始值为0，当数据链路层处于DL\_Inactive状态时，该计数器保持为0。在正常情况下，到达接收端的TLP，其Sequence号和这个计数器中的内容一致。当接收端将这个TLP转发到事务层后，这个计数器将加1，当计数器的值为4095后，再进行加1操作时，该计数器将回归为0。如果到达接收端的TLP，其序号与这个计数器中的值不一致时，接收端需要进行特殊处理，详见下文。

发送端为处理来自接收端的 ACK/NAK DLLP，设置了一个12位的计数器ACKD\_SEQ。这个计数器记载最近接收到的 ACK/NAK DLLP的AckNak\_Seq\_Num字段。这个计数器的初始值为全1，当数据链路层处于Inactive状态时，该计数器保持为全1。发送端收到ACK/NAK DLLP后，将使用这些DLLP中的AckNak\_Seq\_Num字段更新ACKD\_SEQ计数器。

如果(NEXT\_TRANMIT\_SEQ-ACKD\_SEQ) mod 4096 > = 2048 时，发送端将不会从事务层继续接收新的 TLP，因为此时发送端已经发送了许多 TLP，但是接收端可能并没有成功接收这些 TLP，因此并没有及时发送 ACK/NAK DLLP 作为回应。在多数情况下，当 PCIe 链路出现了某些问题时，才可能导致该公式成立。此外 ACKD\_SEQ 计数器还可以帮助发送端重发错误的 TLP，下文将详细解释这个功能。

发送端首先将从事务层获得的TLP存放到Replay Buffer中，在Relpay Buffer中可以存放多个TLP，这个Replay Buffer为发送端使用的发送窗口。PCIe总线并没有规定在Replay Buffer中存放TLP的个数，不同的设计可以采用的大小不同，其中有一个重要的原则就是不能使Replay Buffer成为整个设计的瓶颈，Replay Buffer应该始终保证有足够的空间接收来自事务层的报文。

TLP 进入 Replay Buffer 之后，发送端首先将这个 TLP 封装，然后从 Replay Buffer 中发送到物理层，最终达到接收端。发送端将 TLP 发送出去之后，将等待来自接收端的应答，接收端使用 ACK/NAK DLLP 发送这个应答。发送端根据应答结果决定是将 TLP 从 Replay Buffer 中清除，还是重发在 Replay Buffer 中的 TLP。下文将以几个实例说明发送端如何处理来自接收端的应答。

# 1. 发送端收到 ACK DLLP 报文

如图7-6所示，假设发送端从ReplayBuffer中向接收端发送Sequence号为 $3\sim 7$ 的报文。接收端收到这些报文后将发送ACK DLLP作为回应，其详细步骤如下。

![[pci_express/50521e7f3a04d560bb880f1d0e66b063de792c33a2f8eeb668bd664bb9939ff5.jpg]]  
图7-6 发送端收到ACK DLLP

(1) 发送端向接收端发送 TLP3 \~ 7, 其中 TLP3 是第一个报文, TLP7 是最后一个报文。此时发送端的 NEXT\_TRANSMIT\_SEQ 计数器为 8 , 表示即将填入到 Replay Buffer 中的报文序列号为 8 。  
(2) 接收端按序收到 TLP3 \~ 5，而 TLP6 和 7 仍在传送过程中。接收端的 NEXT\_RCV\_SEQ 计数器为 6，表示即将接收的报文序列号为 6。  
(3) 接收端通过报文检查决定接收 TLP3 \~ 5，然后发送 ACK DLLP，此时这个 ACK DLLP 的 AckNak\_Seq\_Num 字段为 5。为了提高总线的利用率，接收端不会为每一个接收到的 TLP 都做出应答。在这个例子中，AckNak\_Seq\_Num 字段为 5 表示 TLP3 \~ 5 都已经被接收。  
(4) 发送端收到 AckNak\_Seq\_Num 字段为 5 的 ACK DLLP 后, 得知 TLP3 \~ 5 都被成功接收。此时发送端将 TLP3 \~ 5 从 Replay Buffer 中清除。  
(5) 接收端陆续收到 TLP6\~7 后, 接收端的 NEXT\_RCV\_SEQ 计数器为 8 , 表示即将接收的报文序列号为 8 。然后接收端向发送端发送 ACK DLLP, 这个 DLLP 的 AckNak\_Seq\_Num 字段为 7 , 即为 NEXT\_RCV\_SEQ -1。  
(6) 发送端收到 AckNak\_Seq\_Num 字段为 7 的 ACK DLLP 后, 得知 TLP6 \~ 7 都被成功接收。此时发送端将 TLP6 \~ 7 从 Replay Buffer 中清除。

# 2. 发送端收到NAK DLLP报文

如图7-7所示，假设发送端从Replay Buffer中向接收端发送Sequence号为3\~7的报文。接收端收到这些报文后，发现有错误的TLP，此时将发送NAK DLLP而不是ACK DLLP，

其详细步骤如下。

![[pci_express/71763017e1ecd8d444138338670b367aa187fb61802168fe99b891becfd8811d.jpg]]  
图7-7 发送端收到NAK DLLP

(1) 发送端向接收端发送 TLP3 \~ 7, 其中 TLP3 是第一个报文, 而 TLP7 是最后一个报文。  
(2）接收端按序收到 $\mathrm{TLP3}\sim 5$ ，而TLP6和7仍在传送过程中。  
(3) 接收端通过报文检查决定接收 TLP3 \~ 4, 此时 NEXT\_RCV\_SEQ 为 5, 表示即将接收 TLP5。  
(4) TLP5 没有通过完整性验证，此时接收端将向对端发送 NAK DLLP，这个 DLLP 的 AckNak\_Seq\_Num 字段为 4，即为 NEXT\_RCV\_SEQ - 1。AckNak\_Seq\_Num 字段为 4 表示接收端最后一个接收正确的 TLP，其 Sequence 号为 4。  
(5) 发送端收到 AckNak\_Seq\_Num 字段为 4 的 NAK DLLP 后, 得知 TLP3 \~ 4 已被成功接收。此时发送端首先停止从事务层接收新的 TLP, 之后将 TLP3 \~ 4 从 Replay Buffer 中清除。  
(6) 发送端重新发送在 Replay Buffer 中从 TLP5 开始的报文。在这个例子中，发送端将重新发送 TLP5 \~ 7。

发送端每一次收到NAK DLLP后，都将重发在Replay Buffer中剩余的TLP。但是发送端不能无限次重发同一个TLP，因为出现这种情况意味着链路出现了某些问题，必须修复这些问题后，才能继续重发这些TLP。为此在发送端中设置了一个2位计数器REPLAY\_NUM，这个计数器的初始值为0，当数据链路层处于Inactive状态时，该计数器保持为0。

REPLAY\_NUM计数器按照以下几个原则进行更新

- 当发送端第一次收到 NAK DLLP 后，REPLAY\_NUM 计数器将加 1，此时 ACK\_SEQ 计数器被赋值为这个 NAK DLLP 的 AckNak\_Seq\_Num 字段。之后当发送端收到新的 ACK/NAK DLLP，而且其 AckNak\_Seq\_Num 字段大于 ACK\_SEQ 计数器的值时（表示发送端至少重传成功一个 TLP），REPLAY\_NUM 计数器将被重置为 0，ACK\_SEQ 计数器的值也更新为相应的值。  
- PCIe总线规定发送端新收到的ACK/NAK DLLP，其AckNak\_Seq\_Num字段不能小于ACKD\_SEQ计数器，如果出现这种问题，将是芯片的Bug。  
- 如果新的 ACK/NAK DLLP，其 AckNak\_Seq\_Num 字段值等于 ACKD\_SEQ 计数器的值时，表示发送端正在反复地重新发送同一个 TLP，此时 REPLAY\_NUM 计数器将加 1，

当这个计数器溢出时，发送端将不再重复发送这个 TLP，而是重新进行链路训练，当PCIe链路恢复正常后，再重新发送这个TLP。

发送端除了设置REPLAY\_NUM计数器，判断PCIe链路可能出现的故障之外，还设置了另一个计数器REPLAY\_TIMER，进一步识别PCIe链路可能出现的故障。因为使用ACKD\_SEQ计数器判断链路故障的基础是发送端可以收到ACK/NAK DLLP。但是在某种情况下，发送端虽然发送了TLP，但是接收端没有回应ACK/NAK DLLP，或者由于PCIe链路故障发送端没有收到这个回应。

此时发送端需要使用REPLAY\_TIMER计数器，以判断在TLP的传送过程中是否出现异常。REPLAY\_TIMER计数器记载一个TLP报文从发送到获得ACK/NAK DLLP回应的时间，当这个时间过长时，发送端认为PCIe链路出现故障。REPLAY\_TIMER计数器的更新规则如下所示。

(1) REPLAY\_TIMER 计数器的初始值为 0, 在发送端发出或者重新发出一个 TLP 后以一个固定的时钟频率开始计数, 当 REPLAY\_TIMER 计数器到达设定的阈值时, 将认为 PCIe 链路出现故障, 此时 PCIe 设备将进行 PCIe 链路的恢复工作。如果发送端可以正常收到 ACK/NAK DLLP 时, 该计数器不会溢出。  
(2) 发送端收到 ACK DLLP，而且在 Replay Buffer 中没有“已经发送而且尚未被确认的 TLP”后，REPLAY\_TIMER 计数器被重置并重新开始计数。在正常情况下，发送端每发送一个 TLP 后，REPLAY\_TIMER 计数器将被重置并重新计数，但是有时在 Replay Buffer 中存在一些 TLP，这些 TLP 已经被发送出去，但是并没有收到相应的 ACK DLLP，此时 REPLAY\_TIMER 计数器不能被重置。  
(3) 当 Replay Buffer 中没有任何 TLP 时，REPLAY\_TIMER 计数器将被重置而且其值保持不变。  
(4) 发送端收到 NAK DLLP 之后，REPLAY\_TIMER 计数器将被重置而且其值保持不变。当数据链路层重新发送 TLP 时，REPLAY\_TIMER 计数器才重新开始计数。  
(5) REPLAY\_TIMER 计数器到达设定的阈值后，该计数器的值将被重置，且保持不变。此时 PCIe 链路可能出现某种异常，当这些异常被处理完毕后，REPLAY\_TIMER 计数器才可能重新计数。  
(6) PCIe 链路重新训练时，REPLAY\_TIMER 计数器的值保持不变。准确地说，PCIe 链路处于 Recovery 或者 Configuration 状态时，REPLAY\_TIMER 计数器的值保持不变。有关 Recovery 或者 Configuration 状态的详细说明见第 8.2 节。

PCIe总线规范提供了计算REPLAY\_TIMER计数器阈值的经验公式，这个阈值和PCIe设备和主桥提供的Max\_Payload\_Size成正比，与PCIe链路宽度成反比，本节对此公式不进行详细说明。值得注意的是，这个经验公式是基于x86处理器计算得出的，不同的处理器在此处的实现不尽相同。

# 7.2.2 接收端如何使用ACK/NAK协议

接收端首先从物理层获得 TLP，此时在这个 TLP 中包含 Sequence 号前缀和 LCRC 后缀。接收端收到这个 TLP 后，首先将这个报文放入 Receive Buffer 中，然后进行 CRC 检查。如果 CRC 检查成功，接收端将根据接收缓冲的阈值发送 ACK DLLP 给发送端，并将这个 TLP 传

给事务层。除了CRC校验外，接收端还需要做其他检查，本节对此不进行介绍。

# 1. 接收端发送 ACK DLLP

当接收端收到的 TLP 没有出现 LCRC 错误，而且 TLP 的 Sequence 号和 NEXT\_RCV SEQ 计数器的值相同时，接收端将正确接收这个 TLP，并将其转发给事务层，随后接收端将 NEXT\_RCVSEQ 计数器加 1。

接收端根据具体情况，决定向发送端立即发送 ACK DLLP，还是等待接收到更多的 TLP后再发送 ACK DLLP。如果接收端决定发送 ACK DLLP，则该 ACK DLLP 的 AckNak\_Seq\_Num 字段为 NEXT\_RCV\_SEQ -1，即已经正确接收 TLP 的 Sequence 号。

接收端不会对每一个正确接收的 TLP 发出 ACK DLLP 回应，因为这样将严重影响 PCIe 总线链路的使用效率，而是收集一定数量的 TLP 后，统一发出一个 ACK DLLP 回应表示之前的 TLP 都已正确接收。

为此接收端使用了一个 ACKNAK\_LATENCY\_TIMER 计数器，当这个计数器超时或者接收的报文数超过一个阈值后，向发送端发送一个 ACK DLLP 回应。此时这个 ACK DLLP 的 AckNak\_Seq\_Num 字段为在这段时间以来，最后一个被正确接收的 TLP 的 Sequence 号。不同的设计在此处的实现不尽相同。但是这些实现都要遵循以下两个原则。

（1）接收端在收到一定数量的报文后，统一发送一个ACK DLLP做为回应。  
(2）接收端收到的报文虽然没有到达阈值，但是ACKNAK\_LATENCY\_TIMER计数器超时后，仍然要发出ACK DLLP作为回应。在某些情况下，发送端可能在发送一个TLP后，在很长一段时间内，都不会发送新的TLP，此时接收端必须及时给出ACK DLLP回应，以免发送端的REPLAY\_TIMER计数器溢出。

下面将以一个实例说明接收端如何发送 ACK DLLP 回应，该实例如图 7-8 所示，其描述如下所示。

![[pci_express/aaf78df498432249a116d29b5e2108629b1c422a8c6d663892339f33ecd2403d.jpg]]  
图7-8 接收端发送ACK DLLP

(1) 发送端发送 TLP3 \~ 7 给接收端, 其中 TLP3 是第一个报文, 而 TLP7 是最后一个报文。  
(2) 接收端按序收到 TLP3 \~ 5, 而 TLP6 和 7 仍在传送过程中。此时 NEXT\_RCV\_SEQ 的值被更新为 6, 表示下一个即将接收的 TLP, 其 Sequence 号为 6。  
(3) 接收端通过报文检查决定接收 TLP3 \~ 5, 然后发送 ACK DLLP, 这个 DLLP 的 Ack-

Nak\_Seq\_Num 字段为 5。为了提高总线的利用率，接收端不会对每一个接收到的 TLP 都做出应答。在这个例子中，AckNak\_Seq\_Num 字段为 5 表示 TLP3 \~ 5 都已经被接收。

（4）接收端将接收到的TLP3\~5传递给事务层。  
(5) 接收端陆续收到 TLP6\~7 后，继续执行步骤 $3 \sim 4$ 。

# 2. 接收端发送NAK DLLP

接收端设置了一个NAK\_SCHEDUED位，该位用来判断接收端如何发送NAK DLLP，该位的初始值为0。当接收端接收TLP出现错误时，将该位置为1；当出现“错误的TLP”被重新接收成功后，该位被置为0。

如果接收端发现TLP的CRC错误后，将丢弃这个TLP，并发送NAK DLLP，这个DLLP的AckNak\_Se\_Num字段为NEXT\_RCV\_SEQ-1，即最后一个正确接收TLP的Sequence号。此时NEXT\_RCV\_SEQ计算器将保持不变，NAK\_SCHEDULED位将被置为1。

当这个NAK DLLP到达发送端之前，PCIe链路还有一些正在传送的TLP，这些报文将陆续到达接收端，这些报文的Sequence号都将大于NEXT\_RCV\_SEQ，此时接收端不会接收这些报文，而且接收端也不会为这些报文发送NAK DLLP，因为此时NAK\_SCHEDULED位继续保持有效，即为1。

PCIe总线规范没有规定接收端如何拒收后续的TLP报文，较为合理的实现方式是这些后续的报文无需进入接收端的ReceiveBuffer就被拒绝。这样接收端只为已经进入ReceiveBuffer中的TLP发出ACK/NAK回应。

如果接收端收到一个重试的 TLP 报文，其 Sequence 号与 NEXT\_RCV\_SEQ 相等，而且报文没有出现错误，接收端将 NEXT\_RCV\_SEQ 加 1 同时清除 NAK\_SCHEDULED 位。此时表示重试的报文已经被正确接收。

如果接收端收到的 TLP，其 Sequence 号小于 NEXT\_RCV\_SEQ，这种情况通常是由于 TLP 传送过程中的延时，产生的重复 TLP，此时接收端将丢弃这个报文，NEXT\_RCV\_SEQ 计数器的值也不会改变，随后接收端将向对端发送 ACK DLLP，这个 DLLP 的 AckNak\_Seq\_Num 为 NEXT\_RCV\_SEQ -1。

[Ravi Budruk, Don Anderson and Tom Shanley]列举了一个实例说明在某种情况下，接收端将收到此类报文。接收端正确接收到TLP后，将发送ACK DLLP，但是由于链路故障，这个报文并没有到达发送端，此时已经发送的TLP将不会从发送端的Replay Buffer中清除，最终REPLAY\_TIMER将溢出，此时发送端有可能重新进行链路训练，当链路恢复正常后，发送端将重新发送Replay Buffer中的所有TLP。在这种情况下，接收端将收到Sequence号比NEXT\_RCV\_SEQ小的TLP。

下面将使用一个实例进一步说明接收端如何发送NAK DLLP，该实例如图7-9所示，其描述如下所示。

(1) 发送端向接收端发送 TLP3 \~ 7，其中 TLP3 是第一个报文，而 TLP7 是最后一个报文。  
(2) 接收端按序收到 TLP3 \~ 5, 并将这些报文放入 Receive Buffer, 当然也可以在这些报文通过完整性检查后, 再决定是否将这些 TLP 放入 Receive Buffer 中, 而 TLP6 和 7 仍在传送过程中。  
(3) 接收端通过报文检查决定接收 TLP3 \~ 4, 此时 NEXT\_RCV\_SEQ 为 5 , 表示即将接收

TLP5。此时接收端将 TLP3 \~ 4 传递给事务层

![[pci_express/35d840127ead2676d5f0157832a7cc0177bed9645914fb89491b597762e7fecf.jpg]]  
图7-9 接收端如何发送NAK DLLP

(4) 而 TLP5 没有通过完整性验证，此时接收端将发送 NAK DLLP，这个 DLLP 的 AckNak\_Seq\_Num 字段为 4，即 NEXT\_RCV\_SEQ -1。AckNak\_Seq\_Num 字段为 4 表示接收端最后一个正确接收的 TLP，其 Sequence 号为 4。此时接收端将设置 NAK\_SCHEDULED 位为 1，而 NEXT\_RCV\_SEQ 保持不变，即为 5。  
(5) 接收端将丢弃 TLP5。当 TLP6 \~ 7 到达时，接收端仍然丢弃这些报文，即便这些报文通过了完整性检查，因为这些报文的 Sequence 号大于 NEXT\_RCV\_SEQ。接收端不会为 TLP6 \~ 7 发送 NAK DLLP，因为此时 NAK\_SCHEDUED 位有效。  
(6) 发送端收到 NAK DLLP, 其序号为 4 , 此时发送端首先将 TLP3 \~ 4 从 Replay Buffer 中清除, 因为 TLP3 \~ 4 已经被接收端正确接收, 然后重新发送 TLP5 \~ 7。  
（7）接收端如果正确接收到 TLP5 时，发现其 Sequence 号与 NEXT\_RCV\_SEQ 相等，将清除 NAK\_SCHEDULED 位。  
(8) 接收端陆续接收到 TLP6 \~ 7, 并根据 CRC 的检查结果决定发送 ACK DLLP 或者 NAK DLLP。

在某些情况下，接收端发送的NAK DLLP可能并没有被发送端正确接收，因此接收端在很长一段时间内都不会得到“发送端重试的”TLP。此时接收端将会择时重发NAK DLLP，为此接收端设置了一个AckNak\_LATENCY\_TIMER计数器，当该计数器溢出时，接收端将重发NAK DLLP。该计数器的更新规则如下。

（1）当接收端发送ACK或者NAK DLLP时，该计数器重置并开始计数。  
(2) 接收端为“所有已接收的 TLP”发送了 ACK DLLP 报文时，或者数据链路层状态为 DL\_Inactive 时，该计数器被重置且保持为 0。

AckNak\_LATENCY\_TIMER计数器的阈值是REPLAY\_TIMER计数器阈值的 $1 / 3^{\text{一}}$ 。当接收端等待的时间超过AckNak\_LATENCY\_TIMER计数器的阈值后，接收端将重发一个ACK

DLLP°

当发送端发送若干个TLP之后，接收端将发送一个ACK DLLP作为回应。但是在某些情况下，发送端并没有收到接收端的ACK DLLP。此时接收端需要在AckNak\_LATENCY\_TIMER计数器溢出时，重新发送ACK DLLP。从而防止“因为发送端的REPLAY\_TIMER计数器溢出”，重新进行PCIe链路训练，重发更多的TLP。

