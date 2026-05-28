---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "11"
section: "11.3.4 LOCK，Delayed和Posted总线事务间的关系"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 11.3.4 LOCK，Delayed和Posted总线事务间的关系

PCI桥可以将一个LOCK总线事务从上游传递到下游，但是不能将一个LOCK总线事务从下游传递到上游。PCI桥将下游传递到上游的LOCK总线事务转换为普通的总线事务，即去掉这个LOCK标志。

当 PCI 桥从一个主设备接收一个 LOCK 总线事务之后，将这个总线事务发送到下游总线的目标设备。如果该 LOCK 总线事务是 Non-Posted 总线事务，此时 PCI 桥并不能立即完成这个 LOCK 总线事务，因为 PCI 桥还需要将 “Non-Posted 总线请求” 对应的总线完成信息传递给发起者之后，LOCK 总线事务才能最终完成。

在 PCI 桥没有将 LOCK 总线完成传递给发起者之前这段时间里，PCI 桥仅接收这个发起 LOCK 总线事务的主设备的总线请求，而重试其他主设备发出的总线事务。该 PCI 桥不会使用 Delayed 总线事务接收其他 Non-Posted 总线事务，也不会暂存这些总线事务。

一个 LOCK 总线事务从 PCI 桥的上游到达 PCI 桥，并在 PCI 桥将这个 LOCK 总线事务传递到下游总线这段时间里，PCI 桥需要进行以下处理。

（1）将所有同方向的PMW总线事务刷新到下游总线。

(2) 对于 Delayed 总线事务, PCI 桥需要进行特别处理。丢弃所有暂存在 PCI 桥中的 Delayed 总线事务; 允许 LOCK 总线事务超越任何读写请求。或者完成所有 Delayed 读写请求, 再将 LOCK 总线事务发送给下游总线。

在 PCI 桥的下游总线接收 LOCK 总线请求之前，PCI 桥仍然可以暂存来自下游总线的数据请求；而在下游总线接收 LOCK 总线请求之后，PCI 桥不能接收任何来自下游总线的数据请求，直到发起 LOCK 总线请求的主设备解锁。

在一个最简单的 PCI 桥的实现中，一个 LOCK 总线事务在 PCI 桥的下游总线建立完毕后，PCI 桥不能接收上/下游总线的数据请求，除了来自发起 LOCK 总线事务的 PCI 主设备

的数据请求。而且 PCI 桥必须完成发向上游总线的 PMW，DRC 和 DWC 总线请求。采用这个规则可以保证使用 LOCK 总线事务时不会引发死锁。但是使用这些规则将极大影响 PCI 总线的传送性能，为此在处理器系统的设计中，最好不使用 LOCK 总线事务。

# 11.4 PCIe总线的序

PCIe总线的序基于PCI总线的序，并进行了许多扩展。在PCI总线上，仅能使用强序传送规则，而PCIe总线支持Relaxedordering方式进行数据传递，使用这种方法时，不同的TLP在通过RC和Switch到达EP时，不一定遵循PCI总线的强序原则，这也意味着先发出去的TLP并不一定能够最先到达目的地。PCIe总线使用Relaxedordering数据传送方式，在一定程度上可以提高数据传送效率。

在 TLP 的 Attr 字段中有一个 Relaxed Ordering 位，表示该 TLP 是否支持 PCIe 总线的 Relaxed Ordering 方式，但是 TLP 是否可以使用 Relaxed Ordering 还与这个 TLP 经过的设备有关。如果一个 TLP 经过的 Switch 不支持 PCIe 的 Relaxed Ordering 数据传送方式，通过这个 Switch 的 TLP 报文依然需要使用强序方式通过这个 Switch。

系统软件可以通过使能 Device Control 寄存器中的 Enable Relaxed ordering 位，来禁止或者使能 TLP 报文的 Relaxed ordering 功能，Device Control 寄存器在 PCIe 设备的 PCI Express Capability 结构中。目前大多数 PCIe 设备不支持 Relaxed ordering 方式进行 TLP 的传递。

PCIe总线的Relaxed Ordering数据传送方式是有条件的，PCIe总线的每一个TLP报文都有一个唯一的TC，而这个TC又和一个唯一的VC对应。Relaxed Ordering与报文使用的VC相关。VC相同的TLP间的传送遵循Relaxed Ordering的原则，而VC不同的TLP间没有序的要求。在PCIe总线中，所有数据传送类型，如存储器、I/O、配置和Message总线事务都需要遵循规定的传送顺序。

# 11.4.1 TLP 传送的序

VC 不同的 TLP 间没有序的要求，在 PCIe 总线中，“序”是指 VC 相同的 TLP 之间的传送顺序，其关系如表 11-5 所示。

表 11-5 PCIe 总线的序

<table><tr><td>Row Pass Col?</td><td>Posted Request Col 2</td><td>Read Request Col 3</td><td>NPR with Data Col 4</td><td>Completion Col 5</td></tr><tr><td>Posted Request Row A</td><td rowspan="4">a. No b. Y/N</td><td>Yes</td><td>Yes</td><td>a. Y/N b. Yes</td></tr><tr><td>Read Request Row B</td><td rowspan="2" colspan="3">Y/N</td></tr><tr><td>NPR with Data Row C</td></tr><tr><td>Completion RowD</td><td colspan="2">Yes</td><td>a. Y/N b. No</td></tr></table>

各个表项的含义如下。

- Posted Request 由存储器写请求 TLP 或者 Message 使用。  
- Read Request 由 I/O、配置和存储器读请求使用。  
- NPR（Non-Posted Request）with Data 由 I/O、配置写和原子操作使用。  
- a. 表示 TLP 的 RO 位为 0，即不使能 Relaxed Order 的情况。  
- b. 表示 TLP 的 RO 位为 1 或者 IDO 位为 1，即使能 Relaxed Ordering 或者使能 ID-Based Ordering 的情况。不同的规则使用 a，b 子规则略有差异。  
- Yes 表示 Row 中的 TLP 必须能够穿越 Col 中的 TLP。  
- Y/N 表示 Row 中的 TLP 和 Col 中的 TLP 没有序的关系。  
- No 表示 Row 中的 TLP 一定不能穿越 Col 中的 TLP。

下文出现的XYa/b中，X与行对应，其值为 $\mathrm{A}\sim \mathrm{D}$ ，Y与列对应，其值为 $2\sim 5$ 。如A2a表示“PostedRequest”在RO位为0的情况下是否能够超越“PostedRequest”。

通过表11-5与表11-4的比较，可以发现在RO位为0时（即不使用Relax Ordering），PCIe总线的序与PCI的序基本兼容。但是因为在一个TLP中有时RO位和IDO位不为0，因此PCIe总线的序需要根据a和b两种情况分别进行讨论。

# 1. A2

A2 需要分为两种情况讨论，其中 a 对应 TLP 的 RO 和 IDO 位都为 0 情况，而 b 对应 TLP 的 RO 或者 IDO 位为 1 的情况。  
A2 a 的值为 No，表示 Posted Request 报文不能超越之前的 Posted Request 报文，这与 PCI 总线中 PMW 不能超越之前的 PMW 要求相同。PCI 总线的 PMW 与 PCIe 的 Posted Request 报文基本一致，存储器写和 Message 使用这类报文。  
A2 b 的值为 Y/N，该规则需要分为两种情况进行讨论，RO 位为 1 或者 IDO 位为 1。当 RO 位为 1 时，该 Posted Request 报文可以超越之前的 Posted Request 报文。在设计中应用该规则是十分危险的，该规则也意味着“写”可以超越“写”。

如在第11.1.1节描述的生产/消费者模型中，生产者首先将数据写入数据缓冲，然后将Flag位置1。如果“将Flag位置1”的写操作可以超越“写入数据缓冲”的写操作，那么消费者可能会从无效的数据缓冲中读取数据，从而出现错误。

在Switch和支持Peer-to-Peer传送的RC中，设置了一个寄存器位“No RO-enabled PR-PR Passing”，当该位为1时，当TLP通过这些Switch和RC时，Posted Request报文不能超越之前的Posted Request报文，即便这些TLP的RO位为1。

当IDO位为1时，该PostedRequest报文可以超越之前的PostedRequest报文。使用该规则的前提是，这两个PostedRequest报文使用的RequesterID号不同，即这两个PostedRequest报文是由不同的PCIe设备发出的，有关IDO序的详细说明见第11.4.2节。

# 2. A3和A4

A3和A4的值为Yes，表示PostedRequest报文可以超越之前的Non-Posted读和写请求。该规则与PCI总线的PMW可以超越DRR和DWR兼容，其主要目的是避免死锁。详见PCI

总线序的规则5。

# 3. A5

A5 需要分为两种情况讨论，a 适用于 PCIe 总线中的 RC 和 Switch；而 b 适用于 PCIe 桥。

A5 a 的值为 $\mathrm{Y} / \mathrm{N}$ 。在PCIe总线中，Posted Request报文可以超越之前的完成报文也可以不超越，在这两种情况下，都不会造成死锁。该规则与PCI总线中PMW必须超越DRC和DWC不同（PCI总线中的规则7），因为在PCI体系结构中会出现不同版本的PCI桥，而在PCIe体系结构中不会出现这种情况。  
A5 b 的值为 Yes。表示在 PCIe 桥中，PCIe 总线向 PCI 总线的方向传递报文时，Posted Request 报文必须可以超越完成报文，以避免死锁。PCIe 桥内部由多个虚拟 PCI 桥组成，参见图 4-13，因此 PCIe 总线中的 A5 b 与 PCI 总线中的规则 7 兼容。

# 4. B2, C2

B2 需要分为两种情况讨论，其中 a 对应 TLP 的 IDO 位为 0 情况，而 b 对应 TLP 的 IDO 位为 1 的情况。  
B2 a 的值为 No，表示 Read Request 报文不能超越之前的 Posted Request 报文，这与 PCI 总线中 DRR 不能超越之前的 PMW 要求相同。PCI 总线的 DRR 与 PCIe 总线的 Read Request 报文基本一致，存储器、I/O 和配置读使用这类报文。  
B2 b 对应 TLP 的 IDO 位为 1 的情况。当 IDO 位为 1 时，该 Read Request 报文可以超越之前的 Posted Request 报文，否则不能超越。使用该规则的前提是，Read Requests 报文和 Posted Request 报文使用的 Requester ID 号不同。  
C2也需要分为两种情况讨论，其中a对应TLP的IDO位为0情况，而b对应TLP的IDO位为1的情况。其实现机制与B2类似，本节对此不做进一步说明。

# 5. B3, B4, C3, C4

在PCIe总线中，Non-PostedRequest报文可以超越之前的Non-PostedRequest报文，也可以不进行这种超越。该规则从PCI总线中继承而来，而在PCI总线中DRR/DWR可以超越之前的DRR/DWR。PCIe设备在实现中，需要与该规则兼容。

但是在PCIe总线中，存储器读操作使用Split方式进行传送，因此该规则的引入为PCIe设备的设计带来了不小的麻烦。当一个EP进行DMA读操作时，需要首先向RC发送存储器读请求，如 $\mathrm{R1}\sim \mathrm{R4}$ ，而RC收到这些读请求时，将回送读完成TLP，如 $\mathrm{C1}\sim \mathrm{C4}$ 。为简便起见，我们认为每一个存储器读请求的大小为64B，而读完成的大小也为64B，而且不考虑对界的问题，这样EP发送的存储器读请求将与RC的读完成一一对应，我们假设R1对应C1，R2对应C2，并以此类推R4对应C4。

EP首先按照R1～R4的顺序发送这些存储器读请求。但是R1～R4在通过Switch和RC之后可能出现乱序，如果Non-PostedRequest报文可以超越之前的Non-PostedRequest报文，RC最终收到的存储器读请求可能是乱序的，如R2，R4，R3和R1，因此RC发送给EP的读完成报文可能为C2，C4，C3和C1，这个顺序与EP发向RC的存储器读请求的顺序并不相同。因此EP必须处理这种乱序，这为EP的设计带来了不小的困难。

# 6. B5, C5

在PCIe总线中，Non-PostedRequest报文与之前的完成报文没有序的要求。该规则从PCI总线中继承而来，在PCI总线中DRR/DWR可以超越之前的DRC/DWC，也可以不超

越。这些报文的传递不会影响生产/消费者模型的正常运行。

# 7. D2

D2 需要分为两种情况进行讨论，分别是 D2 a 和 D2 b。

其中D2a为No，表示在PCIe总线中，CplD报文不能超越PostedRequest报文，该规则与PCI总线中的规则4兼容。这也是保证生产/消费者模型正常运行的必要条件。

而D2b为 $\mathrm{Y / N}$ 。如果TLP的RO位为1时，该CplD报文可以超越PostedRequest报文。设计者需要慎重使用该规则，因为该规则的应用有可能破坏生产/消费者模型的正常运转。只有传递与生产/消费者模型无关的报文时，才能应用该规则。

此外如果 TLP 的 IDO 位为 1 时，该 CplD 报文可以超越之前的 Posted Request 报文，否则不能超越。使用该规则的前提是，CplD 报文和 Posted Request 报文使用的 Requester ID 号不同。值得注意的是，Cpl 报文由 I/O 或者配置写完成报文使用，该报文中不含有数据，仅包含完成信息。该报文的使用方法与 PCI 总线的 DWC 类似。该报文与 Posted Request 报文没有序的要求，该规则与 PCI 总线的规则 4 兼容。

# 8. D3, D4

在PCIe总线中，完成报文可以超越之前的Non-PostedRequest报文。该规则从PCI总线中继承而来，与规则6对应。该规则的引入主要为了防止死锁。

# 9. D5

如果完成报文与之前的完成报文的 Transaction ID 不同时，该报文可以超越之前的完成报文；如果相同，不能进行这样的超越。

当一个PCIe设备向目标设备发送存储器读请求时，目标设备可能会使用一个或者多个存储器读完成报文将数据回送。如果使用多个存储器读完成报文时，这些存储器完成报文按“地址升序”顺序先后到达源设备。

如果设备A需要从设备B读取 $256\mathrm{B}^{\ominus}$ 的数据，其访问的地址为 $0\mathrm{x}1000 - 0000\sim 0\mathrm{x}1000-$ 00FF时，设备A可以向设备B发送一个存储器读请求TLP，而设备B将以64B为单位向设备A发送存储器读完成TLP，这些完成报文必须以 $\mathrm{C1}\sim \mathrm{C4}$ 的顺序到达设备A， $\mathrm{C1}\sim \mathrm{C4}$ 存储器读完成TLP对应的数据区域如下。

- C1 与 $0 \times 1000 - 0000 \sim 0 \times 1000 - 003 \mathrm{~F}$ 对应。  
- C2 与 $0 \times 1000 - 0040 \sim 0 \times 1000 - 007\mathrm{F}$ 对应。  
- C3 与 $0 \times 1000 - 0080 \sim 0 \times 1000 - 00\mathrm{BF}$ 对应。  
- C4 与 $0 \times 1000 - 00 \mathrm{CO} \sim 0 \times 1000 - 00 \mathrm{FF}$ 对应。

如果设备A需要从设备B读取512B的数据，访问的地址为 $0\mathrm{x}1000 - 0000\sim 0\mathrm{x}1000 - 01\mathrm{FF}$ 时，这段数据区域大于大于设备A的Max\_Read\_Request\_Size参数，因此设备A需要向设备B发出两个存储器读请求，这两个存储器读请求使用两个不同的Tag字段进行区分，分别为 $\mathbf{R}_{\mathrm{T0}}$ 和 $\mathbf{R}_{\mathrm{T1}}$ 。假设 $\mathbf{R}_{\mathrm{T0}}$ 的Tag字段为0，其请求的数据区域为 $0\mathrm{x}1000 - 0000\sim 0\mathrm{x}1000 - 00\mathrm{FF}$ ；而RT1的Tag字段为1，其请求的数据区域为 $0\mathrm{x}1000 - 0100\sim 0\mathrm{x}1000 - 01\mathrm{FF}$ 。

假设设备A首先发送 $\mathbf{R}_{\mathrm{T0}}$ ，然后再发送 $\mathbf{R}_{\mathrm{T1}}$ 。但是设备B仍然可能首先收到 $\mathbf{R}_{\mathrm{T1}}$ ，然后再

收到 $\mathrm{R}_{\mathrm{T0}}$ ，因为PCIe总线允许存储器读请求超越存储器读请求。设备B收到这些存储器读请求后，向设备A发送存储器读完成TLP（以64B为单位），分别为 $\mathrm{Cl}_{\mathrm{T0}} \sim \mathrm{C4}_{\mathrm{T0}}$ 和 $\mathrm{Cl}_{\mathrm{T1}} \sim \mathrm{C4}_{\mathrm{T1}}$ 。

其中 $\mathrm{C1}_{\mathrm{T0}} \sim \mathrm{C4}_{\mathrm{T0}}$ 使用的 Tag 字段为 0，而 $\mathrm{C1}_{\mathrm{T1}} \sim \mathrm{C4}_{\mathrm{T1}}$ 使用的 Tag 字段为 1，分别与 $\mathbf{R}_{\mathrm{T0}}$ 和 $\mathbf{R}_{\mathrm{T1}}$ 对应，这也意味着这两组存储器读完成使用的 Transaction ID 不同，因此可以彼此超越，这两组存储器读完成 TLP 对应的数据区域如下。

- $\mathrm{C1}_{\mathrm{T0}}$ 与 $0\mathrm{x}1000 - 0000\sim 0\mathrm{x}1000 - 003\mathrm{F}$ 对应； $\mathrm{C2}_{\mathrm{T0}}$ 与 $0\mathrm{x}1000 - 0040\sim 0\mathrm{x}1000 - 007\mathrm{F}$ 对应； $\mathrm{C3}_{\mathrm{T0}}$ 与 $0\mathrm{x}1000 - 0080\sim 0\mathrm{x}1000 - 00\mathrm{BF}$ 对应； $\mathrm{C4}_{\mathrm{T0}}$ 与 $0\mathrm{x}1000 - 00\mathrm{CO}\sim 0\mathrm{x}1000 - 00\mathrm{FF}$ 数据区域对应。  
- $\mathrm{C1}_{\mathrm{Tl}}$ 与 $0\mathrm{x}1000 - 0100\sim 0\mathrm{x}1000 - 013\mathrm{F}$ 对应； $\mathrm{C2}_{\mathrm{Tl}}$ 与 $0\mathrm{x}1000 - 0140\sim 0\mathrm{x}1000 - 017\mathrm{F}$ 对应； $\mathrm{C3}_{\mathrm{Tl}}$ 与 $0\mathrm{x}1000 - 0180\sim 0\mathrm{x}1000 - 01\mathrm{BF}$ 对应； $\mathrm{C4}_{\mathrm{Tl}}$ 与 $0\mathrm{x}1000 - 01\mathrm{CO}\sim 0\mathrm{x}1000 - 01\mathrm{FF}$ 数据区域对应。

此时设备A收到的存储器读完成，有多种可能，如表11-6所示。

表 11-6 设备 A 收到的存储器读完成序列

<table><tr><td>序列1</td><td>序列2</td><td>序列3</td><td>序列4</td><td>序列5</td><td>序列6</td><td>序列7</td><td>序列8</td></tr><tr><td>C1T0</td><td>C1T0</td><td>C1T1</td><td>C1T1</td><td>C1T0</td><td>C1T0</td><td>C1T1</td><td>C1T1</td></tr><tr><td>C2T0</td><td>C1T1</td><td>C2T1</td><td>C2T1</td><td>C1T1</td><td>C2T0</td><td>C2T1</td><td>C1T0</td></tr><tr><td>C3T0</td><td>C2T0</td><td>C3T1</td><td>C1T0</td><td>C2T1</td><td>C3T0</td><td>C3T1</td><td>C2T0</td></tr><tr><td>C4T0</td><td>C2T1</td><td>C4T1</td><td>C2T0</td><td>C2T0</td><td>C1T1</td><td>C1T0</td><td>C2T1</td></tr><tr><td>C1T1</td><td>C3T0</td><td>C1T0</td><td>C3T1</td><td>C3T0</td><td>C2T1</td><td>C2T0</td><td>C3T0</td></tr><tr><td>C2T1</td><td>C3T1</td><td>C2T0</td><td>C4T1</td><td>C4T0</td><td>C3T1</td><td>C3T0</td><td>C4T0</td></tr><tr><td>C3T1</td><td>C4T0</td><td>C3T0</td><td>C3T0</td><td>C3T1</td><td>C4T1</td><td>C4T0</td><td>C3T1</td></tr><tr><td>C4T1</td><td>C4T1</td><td>C4T0</td><td>C4T0</td><td>C4T1</td><td>C4T0</td><td>C4T1</td><td>C4T1</td></tr></table>

上表仅列出了设备A可能从设备B中收到的存储器读完成序列。由此可见对于Tag字段不同的存储器完成报文，在到达设备A时顺序并不确定。但是对于Tag字段相同的存储器读完成TLP，这些存储器完成报文是严格按照地址“升序”的顺序到达设备A。PCIe总线的这种乱序为PCIe设备的设计带来了不小的麻烦，设计者必须认真地处理这些乱序可能。

# 11.4.2 ID-Base Ordering

IDO 模型由 PCIe V2.1 版本引入。该模型引入了“数据流”的概念，即相同的数据源发出的 TLP 属于相同的“数据流”，而不同数据源发出的 TLP 属于不同的“数据流”。PCIe 链路可以根据“数据流”对 TLP 进行区分。IDO 模型允许分属不同“数据流”的 TLP 之间没有序的要求，即可以“自由乱序”。

IDO 模型的引入有利于 Switch 对发向不同 Egress 端口的报文进行优化处理。我们假设 Switch 的一个 Ingress 端口收到了若干个数据报文，这些报文分别发向不同的 Egress 端口，如图 11-5 所示。

其中 TLP1 \~ 3 分别发向 Egress 端口 $1 \sim 3$ ，在不使用 IDO 模型的情况下，Ingress 端口需要等待之前的报文被发送出去之后，才能发送之后的报文。TLP3、TLP2 和 TLP1 依次进入 Ingress 端口，如果不使用 IDO 模式时，TLP2 需要等待 TLP3 完全离开 Ingress 端口后，才能

被发送；同理 TLP1 需要等待 TLP2 离开 Ingress 端口才能被发送。显然这种数据传送方式并不合理。

![[pci_express/6f0814450acf5587b5c162d129876988454328a8dd462286623eb3568e064da4.jpg]]

![[pci_express/e60703b1906cc0e2aaf721a01677b962e5e2722fd6abbfc0e9988ca6355398b7.jpg]]  
图11-5 IDO模型的优点

这种拥塞也被称为HOL（Head-of-Line）Blocking。引起这种现象的主要原因是Ingress端口每次只能处理一个报文引起的。如果在Ingress端口只有一个发送部件与Egress端口 $1\sim 3$ 对应时，即便使用IDO模型并不会提高效率，因为这些报文依然需要通过这一个发送部件串行发送。由此可见对于这种类型的Switch，使用IDO模型并不会提高效率。

但是如果 Ingress 端口中有 3 个发送部件，分别与 Egress 端口 $1 \sim 3$ 对应时，实际上是 Ingress 端口为每一个 Egress 端口提供分离的发送缓冲，这个缓冲也被称为 VOQ（Virtual Output Queue）。此时 TLP1 \~ 3 的发送可以同时进行，而 Ingress 端口使用 IDO 模型，可以不考虑传送“序”而同时发送 TLP1 \~ 3，从而极大地提高了 Switch 的转发效率。目前多数 Switch 的 Ingress 端口都支持 VOQ 技术。

在PCIe V2.1总线规范提出之前， $\mathrm{PLX}^{\ominus}$ 公司已经使用类似IDO模型的技术以优化Switch的数据传送，即PLX-Specific Relaxed Ordering技术。下文以PEX8518芯片为例说明该技术的具体实现，该芯片是PLX公司设计的PCIe Switch。在PEX8518中，每一个Ingress端口都为不同的TC设置了一个“PLX-Specific Relaxed Ordering”使能位，当该位为1时，当一个Ingress端口收到的TLPs发向的Egress端口不同，则没有序的要求；而Egress端口相同的TLPs必须按序进行。PLX使用的这种技术与IDO模型类似。

# 11.4.3 MSI报文的序

在PCIe总线中，还有一种序引发的数据完整性问题需要特别注意，即由MSI报文引发的数据完整性问题。PCIe设备使用MSI机制时，通过向中断控制器发送MSI报文以提交中断请求。然而对于PCIe体系结构而言，这个MSI报文与普通的存储器写报文并没有本质的区别，这个报文也可以使用不同的TC。如果设备的数据传送使用TC0，而MSI报文使用TC1时，将可能引发数据完整性的问题。

假设一个PCIe设备正在使用DMA写操作，将一组数据传递到主存储器，此时该设备将使用存储器写TLP进行数据传送，当数据传送完成后，使用MSI报文通知处理器DMA写操

作已经结束。

如果进行数据传递的 TLP 使用 TC0，而 MSI 报文使用 TC1 时，这两种 TC 可能使用的 VC 并不相同，而不同 VC 间的数据传递并没有序的要求。因此该 PCIe 设备虽然从设计逻辑上保证，将传递数据的存储器写 TLP 发送完毕后再发送 MSI 报文，但是 RC 仍然会首先收到 MSI 报文，然后再收到传递数据的存储器写 TLP。

此时如果处理器在收到MSI报文后，立即在中断处理服务例程中使用该PCIe设备写入的数据，将可能引发数据完整性问题。

PCIe总线规范并没有约定如何处理传递MSI报文而产生的数据完整性问题。在上述实例中，如果MSI报文使用的TC与数据传递使用的TC相同，将不会出现这个数据完整性问题。如果在设计中MSI报文使用的TC与数据传递使用的TC不一致，需要注意该问题。

一个可行的方法是在数据传递结束后，使用“zero-length存储器读请求TLP”对目标设备进行读操作，这个读操作可以将数据写入最终目的地。当该读操作结束后，即PCIe设备收到存储器读完成TLP后，再发送MSI报文。

如一个 PCIe 设备完成 DMA 写操作之后，再向目标地址（某个存储器地址）发送一个“zero-length 存储器读请求”报文，该报文可以保证之前的存储器写报文都被刷新到主存储器后，才能从主存储器获得应答信息，因为存储器读请求 TLP 不能超越存储器写报文。当 PCIe 设备收到与这个存储器读请求对应的存储器读完成 TLP 后，再发送 MSI 报文进行中断请求。

使用上述方法虽然可以避免因为传送MSI报文带来的数据完整性问题，但是将带来较大的中断延时。因为在PCIe体系结构中，一个设备从“发送存储器读请求TLP”到“获得存储器读完成TLP”的延时较长。而且使用这种方法也增加了硬件逻辑设计的难度。

目前支持多个VC的PCIe设备，通常将MSI报文和数据传送报文使用的TC设置为相同的值，以避免数据完整性问题。如在Intel的高精度声卡控制器中，数据传送使用的报文和MSI报文都只能使用TC0。

# 11.5 小结

本章重点介绍PCI/PCIe总线的序。在局部总线中，数据传送顺序以及与其相关的数据完整性一直是系统程序员的学习重点与难点。对于系统程序员而言，这部分内容必须熟练掌握。有许多系统Bug仍然因为系统程序员的疏忽，或者并没有深入理解数据完整性的原理，而无意产生，并很难复现。这些Bug将对一个处理器系统的稳定运行产生致命影响。

在PCI/PCIe总线中规定了数据传送序，使用生产/消费者模型进行数据传递，合理解决了数据完整性问题。而设计者在实现PCI/PCIe体系结构时，必须遵循这些传送序，并且符合生产/消费者模型要求的数据传送方式。

