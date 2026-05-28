---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "08"
section: "8.3.3 L0s状态"
part: 3
tags: [pci, pci-express, computer-architecture]
---
# 8.3.3 L0s状态

PCIe 设备必须支持 L0s 状态。L0s 状态是一个低功耗状态，PCIe 设备进入或者退出该状态不需要系统软件的干预，其状态转换由硬件控制完成。L0s 的状态转换由两部分组成，一个是接收状态机，另一个是发送状态机。

同一个PCIe设备的发送逻辑TX和接收逻辑RX，在同一时刻可能处于不同的链路状态，其中一个为L0，而另一个为L0s。例如当一个EP进行DMA写操作时，其发送逻辑TX一直被使用，因此处于L0状态，而接收逻辑RX可能长时间没有被使用，从而可以暂时处

于L0s状态，以降低功耗。

# 1. 发送逻辑 TX 状态机

L0s 的发送状态机如图 8-12 所示，该状态机由 Tx\_L0s.Entry、Tx\_L0s.Idle 和 Tx\_L0s.FTS 状态组成。

![[pci_express/225b675c7394ee169a9a60c13f94e2eaf2385b784da261048cc87219bcaec8f3.jpg]]  
图8-12 L0s的发送状态机

PCIe 设备处于 L0 状态发现链路为临时“空闲”状态时，将进入 Tx\_L0s.Entry 状态。处于该状态时，发送逻辑 TX 首先向对端发送 1 或者 2 个 EIOS 序列，之后进入 Electrical Idle 状态。再经过 20ns 延时后，发送逻辑 TX 进入 Tx\_L0sIdle 状态。

当发送逻辑 TX 处于 Tx\_L0s.Idle 状态时，如果 PCIe 设备需要发送数据报文，发送逻辑 TX 将退出 Tx\_L0s.Idle 状态，进入 Tx\_L0s.FTS 状态。发送逻辑 TX 处于 Tx\_L0s.FTS 状态时，向对端顺序发送 N\_FTS 个 FTS 序列和 1 个 SKP 序列之后，将进入 L0 状态。

# 2. 接收逻辑RX状态机

L0s的接收状态机如图8-13所示，该状态机由Rx\_L0s.Entry、Rx\_L0s.Idle和Rx\_L0s.FTS状态组成。PCIe设备可以从L0s状态进入L0或者Recovery状态。

![[pci_express/a4c4503bc86eafd53759ca3f663c87cbfda691640f54bf3e43d4bdbbb8e6f4f0.jpg]]  
图8-13 L0s的接收状态机

接收逻辑RX处于L0状态时，如果收到1个EIOS序列后，将进入Rx\_L0s.Entry状态。接收逻辑RX在Rx\_L0s.Entry状态经过一段延时后，将进入Rx\_L0s.Idle状态。

接收逻辑RX在Rx\_L0s.Idle状态中将持续监测接收链路，如果发现对端设备的发送逻辑TX退出“Electrical Idle”状态时，接收逻辑RX将进入Rx\_L0s.FTS状态。

当接收逻辑RX处于Rx\_L0s.FTS状态时，PCIe链路的每一个Lane都将收到N\_FTS个FTS序列，接收逻辑RX使用这些FTS序列重新获得Bit/SymbolLock。如果对端发送逻辑TX发送的FTS序列不足，接收逻辑RX将无法成功获得Bit/SymbolLock，此时PCIe设备将进入Recovery状态。

当接收逻辑 RX 收到足够数量的 FTS 序列，又收到了一个 SKP 序列后（该 SKP 序列的作用是 De-Skew），将从 Rx\_L0s.FTS 状态迁移到 L0 状态。

# 8.3.4 L1状态

L1状态是一个比LOs状态使用功耗更低的状态，PCIe设备从L1状态恢复到L0状态，比LOs状态恢复到L0状态的延时更长。PCIe设备进入或者退出该状态可以不需要系统软件

的干预。当然系统软件也可以通过设置某些寄存器，使PCIe链路的两端设备同时进入L1状态。在PCIe总线中，L1状态是一个可选状态。

其中只有下游设备（EP或者Switch的上游端口）可以主动“进入L1状态”，而上游设备（RC或者Switch的下游端口）必须与下游设备进行协商后才能进入L1状态。当下游设备满足以下条件时，可以进入L1状态。

（1）PCIe设备支持L1状态  
(2) PCIe 设备没有准备发送的 TLP 和 DLLP。  
(3) 如果下游设备是一个 Switch，这个 Switch 的所有下游端口处于 L1 或者更高一级的节电状态。

而上游设备需要经过协商才能进入L1状态。

（1）首先下游设备向上游设备发送PM\_Active\_State\_Request\_L1报文。  
(2) 上游设备收到这个 DLLP 报文后，如果该上游设备可以进入 L1 状态，则向下游设备发送 PM\_Request\_Ack 报文；如果不能进入，则发送 PM\_Active\_State\_Nak 报文。

在PCIe总线中，L1状态由L1.Entry和L1 Idle两个子状态组成，如图8-14所示。

![[pci_express/d693c744c257edb207b546182dc02a704469ff8aa7d995cece79f022c53dd5b9.jpg]]  
图8-14 L1状态机

PCIe 设备从 L0 状态首先进入 L1.Entry 状态。PCIe 设备处于 L1.Entry 状态时，发送逻辑 TX 处于 Electrical Idle 状态。PCIe 设备在此状态停留 20ns 后，进入 L1.Idle 状态。接收逻辑在 L1.Idle 状态中将持续监测接收链路，如果发现其对端发送逻辑 TX 退出“Electrical Idle”状态时，将从 L1.Idle 状态首先迁移到 Recovery 状态，而不是 L0 状态。

PCIe 设备处于该状态时，其发送逻辑 TX 可以处于高阻抗或者低阻抗模式，而其接收逻辑 RX 必须处于低阻抗模式。

# 8.3.5 L2状态

当PCIe设备处于L2状态时，使用的功耗低于L1状态，但是恢复到L0状态的延时更长。当PCIe设备处于L2状态时，需要首先迁移到Detect状态，重新进行链路训练。L2状态是一个可选状态。L2状态机由L2.Idle和L2 TransmitWake两个子状态组成，如图8-15所示。

![[pci_express/c2fcf241bfb241cbc9fbdd87d55bf43ac1bbde73793829b02311d97b77060617.jpg]]  
图8-15 L2状态机

在L2.Idle状态中，接收逻辑RX的端接必须处于低阻抗模式，而发送逻辑TX必须在Electrical Idle状态中至少停留20ns。当一个EP、Switch或者RC的某个端口被唤醒后，将首先从L2.Idle状态迁移到L2 TransmitWake状态。

PCIe 设备进入 L2 TransmitWake 状态后，将向 RC 端口或者 Switch 的下游端口发送 Beacon 信号。当 RC 端口收到这个 Beacon 信号后，将进入 Detect 状态进行链路训练；而当 Switch 收到 Beacon 信号被唤醒后，其上游端口将进入 L2 TransmitWake 状态，并向上游链路转发这个 Beacon 信号，并逐级唤醒 PCIe 链路的上游设备。

当 EP 或者 Switch 上游端口的接收逻辑 RX，发现其对端发送逻辑 TX 退出 Electrical Idle

状态时，将从L2 TransmitWake状态迁移到Detect状态，重新进行链路训练。

# 8.4 PCI PM 机制

PCIe总线与PCI总线使用的PCI-PM管理机制兼容。在PCIe设备的扩展配置空间中定义了Power Management Capabilities结构，该结构中含有一系列寄存器，这些寄存器的详细说明见第4.3.1节。

系统软件通过修改PMCSR寄存器的Power State字段，可以使PCIe设备进入不同的节能状态D-State，如D0、D1、D2和D3状态。其中D0是正常工作状态，功耗最高，而D1、D2和D3为低功耗状态。其中D1的休眠等级最低，功耗相对较高，而D3的休眠等级最高，功耗相对较低。D-State的状态转换关系如图8-16所示。

![[pci_express/faa9e08928a4d76a712e8d63593849ad0c6f1d657a6a36c7be699ff179a7b5ac.jpg]]  
图8-16 D-State状态机

值得注意的是，当PCIe设备进行状态迁移时，PCIe链路也需要视情况进行相应的状态，如进入L1或者L2等状态。

# 8.4.1 PCIe设备的D-State

PCIe设备的D-State由D0、D1、D2和D3状态组成。其中D0状态由 $\mathrm{D0}_{\mathrm{initialized}}$ 和 $\mathrm{D0}_{\mathrm{active}}$ 两个子状态组成，而D3状态由 $\mathrm{D3}_{\mathrm{hot}}$ 和 $\mathrm{D3}_{\mathrm{cold}}$ 两个子状态组成。

# 1. D0状态

PCIe 设备必须支持 D0 状态，该状态由 $\mathrm{D0}_{\mathrm{initialized}}$ 和 $\mathrm{D0}_{\mathrm{active}}$ 两个子状态组成。当 PCIe 设备处于 $\mathrm{D0}_{\mathrm{initialized}}$ 状态时，该 PCIe 设备并没有被系统软件使能，此时该 PCIe 设备仅能接收配置读写请求 TLP，不能主动发出其他 TLP。此时该 PCIe 设备配置寄存器的 Command 寄存器为复位值 $0 \times 00$ 。此时虽然 PCIe 设备已经被加电，但是并不能正常使用。

当PCIe设备处于 $\mathrm{D0}_{\mathrm{active}}$ 状态时，PCIe设备处于正常工作模式，并没有任何节电措施。但是PCIe设备仍然可以使用ASPM机制，将链路状态迁移到L0s或者L1状态，以降低功耗。值得注意的是，ASPM机制与PCI PM机制是独立的。

当PCIe设备进行复位后，该设备将首先进入D0\_uninitialized状态。系统软件通过修改PMC-SR寄存器的Power State字段，也可以使设备从D3hot状态迁移到该状态。值得注意的是D3cold状态迁移到该状态的过程与复位操作等效。当系统软件改写Command寄存器的状态

位使能PCIe设备后，该设备从 $\mathrm{D0}_{\mathrm{uninitialized}}$ 迁移到 $\mathrm{D0}_{\mathrm{active}}$ 状态。

# 2. D1和D2状态

D1和D2状态分别为PCIe设备的轻度和重度休眠状态。这两个状态为PCIe设备的可选状态，PCIe设备处于D1状态时的功耗高于D2状态。

PCIe设备处于这两个状态时，除了PME消息之外，不能主动发送其他TLP；除了接收配置请求TLP外，不能接收其他TLP。当PCIe设备处于这两种状态时，可以向RC发送PME消息，通知系统软件该PCIe设备进入休眠状态。当PCIe设备进入D1或者D2状态时，PCIe链路将进入L1状态。PCIe设备可以从D1和D2状态直接返回到 $\mathrm{DO}_{\mathrm{active}}$ 状态。

# 3. D3状态

PCIe 设备必须支持 D3 状态，D3 状态由 $\mathrm{D}3_{\mathrm{hot}}$ 和 $\mathrm{D}3_{\mathrm{cold}}$ 两个子状态组成。PCIe 设备处于 $\mathrm{D}3_{\mathrm{hot}}$ 状态与处于 D1/D2 状态时的功能类似，只是 PCIe 设备只能从 $\mathrm{D}3_{\mathrm{hot}}$ 状态返回 $\mathrm{D0}_{\mathrm{uninitialized}}$ 状态，而不能返回 $\mathrm{D0}_{\mathrm{active}}$ 状态。对于 PCIe 设备，从 $\mathrm{D}3_{\mathrm{hot}}$ 状态返回 $\mathrm{D0}_{\mathrm{uninitialized}}$ 状态的过程相当于热复位。

当PCIe设备的Vcc电源被移除时，PCIe设备无论处于何种状态，都将进入 $\mathrm{D3}_{\mathrm{cold}}$ 状态。值得注意的是一个PCIe设备使用两种电源 $\mathrm{V_{cc}}$ 和 $\mathrm{V_{aux}}$ ， $\mathrm{V_{cc}}$ 电源被移除并不意味着PCIe设备被完全下电。

有些PCIe设备在处于 $\mathrm{D3}_{\mathrm{cold}}$ 状态时仍然可以发出PME消息，此时这个PCIe设备负责发送PME消息的功能模块必须使用 $\mathrm{V}_{\mathrm{aux}}$ 而不是 $\mathrm{V}_{\mathrm{cc}}$ 进行供电。

# 8.4.2 D-State的状态迁移

如图8-16所示，PCIe设备可以进行D-State的状态迁移。大多数D-State的状态迁移都是系统软件通过修改PMCSR寄存器的Power State字段实现的，但是仍然有些状态迁移采用了其他方式。

- 使能 Command 寄存器的命令位，可以使设备从 $\mathrm{D0}_{\mathrm{initialized}}$ 状态迁移到 $\mathrm{D0}_{\mathrm{active}}$ 状态。  
- PCIe设备的 $\mathrm{V_{cc}}$ 被移除时，D3hot状态将迁移到D3cold状态。  
- 当PCIe被唤醒， $\mathrm{V_{cc}}$ 重新上电之后，PCIe设备将从D3cold状态迁移到 $\mathrm{D0}_{\mathrm{uninitialized}}$ 状态。

当PCIe设备进行D-State状态迁移时，PCIe链路的状态也可能随之变化。PCIe设备的D-State状态与PCIe链路状态的对应关系如表8-3所示。

表 8-3 D-State 状态与 PCIe 链路状态的对应关系

<table><tr><td>下游设备的D-State</td><td>上游设备可能的D-State</td><td>可能的链路状态</td></tr><tr><td>D0</td><td>D0</td><td>L0, L0s, L1, L2/L3 Ready</td></tr><tr><td>D1</td><td>D0 ~ D1</td><td>L1, L2/L3 Ready</td></tr><tr><td>D2</td><td>D0 ~ D2</td><td>L1, L2/L3 Ready</td></tr><tr><td>D3hot</td><td>D0 ~ D3hot</td><td>L1, L2/L3 Ready</td></tr><tr><td>D3cold</td><td>D0 ~ D3cold</td><td>L2, L3</td></tr></table>

由上表可以发现，上游设备所处的D-State等级小于或等于下游设备的休眠等级。如下游设备处于D1状态时，上游设备不能处于比D1更高的休眠等级，如D2或者D3状态。

当设备处于 $\mathrm{D0}\sim \mathrm{D3}_{\mathrm{cold}}$ 状态时，ASPM机制可以根据链路的使用情况进行链路状态的迁移，而无需软件的干预。下文以PCIe设备从D0迁移到D1说明D-State进行状态迁移时，

PCIe链路如何进行状态迁移

当系统软件修改PMCSR寄存器的Power State字段，将PCIe设备从D0迁移到D1状态时，上游设备与下游设备将协调工作，完成PCIe设备的状态切换，并改变链路的状态，其实现过程如图8-17所示。

![[pci_express/64a2bd282bf8a90c8df2df241423b9bd1c711f0ebd26b2da256b2606e9589239.jpg]]  
图8-17 PCIe设备从D0到D1的状态迁移

（1）上游设备向下游设备发送配置写请求，改变下游设备PMCSR寄存器的Power State字段，从而使下游设备从D0状态迁移到D1状态。  
(2) 下游设备收到这个配置写请求 TLP 后, 将改变 PMCSR 寄存器的 Power State 字段, 并向上游设备发送配置写完成 TLP。这个配置写完成 TLP 首先需要经过数据链路层, 并从对端获得足够的发送 Credit $\text{念}$ 后, 将这个配置写完成 TLP 通过数据链路层发送到对端。  
(3) 下游设备的事务层收到数据链路层的确认后，得知配置写完成 TLP 已经被上游设备正确接收后（详见 ACK/NAK 协议），将挂起下游设备的事务层。并向上游设备连续发送 PM\_Enter\_L1 DLLP，同时等待来自上游设备的 PM\_Request\_Ack 报文。  
(4) 上游设备收到下游设备的 PM\_Enter\_L1 DLLP 后，首先禁止发送新的 TLP，并等待之前发送的 Non-Post TLP 得到确认后，挂起上游设备的事务层，并向下游设备连续发送 PM\_Request\_Ack DLLP。  
(5) 下游设备在没有收到上游设备的 PM\_Request\_Ack DLLP 之前，虽然事务层已经被挂起，但是数据链路层和物理层仍然可以正常工作，此时数据链路层可以正确接收来自上游

端口的 DLLP，并发送 ACK/NAK 和流量控制相关的一些 DLLP。

(6) 当下游设备收到 PM\_Request\_Ack DLLP 后，将停止发送 PM\_Enter\_L1 DLLP，挂起数据链路层，然后将物理层置为 Electrical Idle 状态。  
（7）上游设备发现其接收链路处于 Electrical Idle 状态时，将停止发送 PM\_Request\_Ack DLLP，并挂起数据链路层，然后将物理层置为 Electrical Idle 状态。此时 PCIe 链路将进入 L1 状态。

当PCIe链路处于L1状态时，如果系统软件需要改变下游PCIe设备PMCSR寄存器的Power State字段，PCIe链路需要首先从L1状态迁移到正常工作状态L0，下游设备才能接收这个配置写请求TLP。

PCIe设备其他D-State状态的迁移过程与此大同小异，详见PCIe总线规范，本节对此不做进一步描述。

# 8.5 小结

本章重点介绍PCIe总线的LTSSM状态机，该状态机的迁移模型较为复杂。本章仅介绍了该状态机的基本工作路径，对此部分有兴趣的读者可以阅读PCIe总线规范，进一步了解相关内容。

LTSSM 状态机在 PCIe 总线规范中处于核心地位，深入理解该状态机的运转模型，有利于底层软件工程师深入理解 PCIe 设备的工作状态，从而开发出质量较高的程序。

本章还使用一定篇幅介绍了PCIe总线的电源管理模型。目前电源管理已经成为计算机体系结构的热点。一个合理的电源管理模型需要软硬件的共同参与，但是硬件设计仍然决定了电源管理模型的节电效率。

