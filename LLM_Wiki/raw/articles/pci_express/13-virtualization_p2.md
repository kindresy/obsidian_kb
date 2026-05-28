---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "13"
section: "13.2.3 Invalidate ATC"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 13.2.3 Invalidate ATC

处理器系统更改ATPT时，需要使用MsgD报文通知相应PCIe设备，并 InvalidateATC中相应的Entry，该MsgD报文也被称为“ InvalidateRequestMessage”，其格式如图13-11所示。其中PCIe设备每收到一个MsgD只能 InvalidateATC中的一个Entry。

![[pci_express/d7e3ff05d7971cef37acf08f1d03ecdd0f885375080a378b9880d149f36df819.jpg]]  
图13-11 InvalidateRequestMessage的格式

Invalidate Request Message 各个字段的描述如下所示。

- Fmt 字段为 0b011，表示报文头为 4DW，而且含有 Payload 字段。Length 字段为 0b10，表示该报文 Payload 的大小为 8B。而 TC、Attr、TD 和 EP 的含义与通用 TLP 头相同，详见第 6.1 节。  
- Type字段为0b10010，表示该消息报文使用ID路由方式。其中Requester ID字段保存TA的ID号。Device ID保存目标设备使用的ID号，即存放ATC的PCIe设备ID，Message报文使用该字段进行ID路由。  
- ITag 字段与 Tag 字段的功能类似，取值范围为 $0 \sim 31$ 。当 TA 需要连续发送多个 “Invalidate Request Message” 报文时，使用该字段区别不同的 $\mathrm{MsgD}$ 。

该MsgD报文的Payload字段中存放“Unstranlated Address”，而S位用来表示数据区域的大小，如表13-2所示。当PCIe设备收到 Invalidate Request Message报文后，根据Unstranlated Address字段 Invalidate ATC中对应的Entry。PCIe设备 Invalidate ATC中对应的Entry之后，将向TA发送 Invalidate Completion Message报文，表示已经 Invalidate ATC中的对应Entry，该报文的格式如图13-12所示。

![[pci_express/af2cdb26ad10311d361797c1a8e0a729b699e950c7d0c7545acd25637339cbfc.jpg]]  
图13-12 InvalidateCompletionMessage的格式

Invalidate Completion Message 报文的各个字段的描述如下所示。

- Fmt、Type 等字段与 Invalidate Request Message 的对应字段类似。  
- Requester ID 字段保存 TA 的 ID 号，而 Device ID 字段保存 PCIe 设备的 ID 号。  
- CC字段表示PCIe设备需要向TA发送 Invalidate Completion Message报文的个数。当CC字段为0时表示需要8个这样的报文，为n时表示需要n个这样的报文。n的最大值为 $0 \times 07$ 。  
- ITag Vector字段由32位组成，其中每一位对应 Invalidate Request Message报文的一个ITag字段， Invalidate Completion Message通过ITag Vector字段可以向多个 InvalidateRequest Message报文发出回应，表示已经 Invalidate ATC中的多个Entry。

# 13.3 SR-IOV与MR-IOV

PCIe总线除了提供了ATS机制外，还使用SR-IOV和MR-IOV机制，进一步优化虚拟化技术的实现。其中SR-IOV技术的主要作用是将一个物理PCIe设备模拟成多个虚拟设备，其中每一个虚拟设备可以与一个虚拟机绑定，从而便于不同的虚拟机访问同一个物理PCIe设备。在PCIe体系结构中，即便使用了ATS和SR-IOV技术，在处理器系统中仍然只有一个PCIe总线域，所有的虚拟机共享这个PCI总线域，这为虚拟化技术的实现带来了不小的障碍。使用SR-IOV技术，可以解决单个PCIe设备被多个虚拟机共享的问题，但是并没有对管理PCIe设备的Switch进行约束。

提出MR-IOV技术的主要目的是解决多个处理器系统对一个PCI总线域共享的问题，其本质是将一个物理PCI总线域，分解为多个虚拟的PCI总线域，多个处理器系统可以与多个PCI总线域对应，从而实现了不同PCI总线域间的隔离。MR-IOV技术对PCIe总线进行了大规模的扩展，提出了MRA（Multi-Root Aware）Switch、MRA（Multi\_Root Aware）Device和MRA RP（Rort Port）的概念，同时对PCIe总线的数据链路层和流量控制进行了细微改动。

# 13.3.1 SR-IOV技术

在SR-IOV技术没有引入之前，一个PCIe设备在一个指定的时间段内，只能与一个虚拟机（Domain1）绑定，而其他虚拟机（Domain2）访问与Domain1绑定的PCIe设备时，需要首先向Domain1发送请求，由Domain1从PCIe设备获得数据后，再传送给Domain2。使用这种方法将极大增加在虚拟化环境下，虚拟机访问PCIe设备的延时，同时也干扰了其他虚拟机的正常运行。

而在处理器系统中并行设置多个同样的物理设备，不仅增加了系统成本，而且增加了处理器系统的规模，从而造成不必要的浪费。SR-IOV技术在此背景下诞生。支持SR-IOV的PCIe设备，由多组虚拟子设备组成，其拓扑结构如图13-13所示。

由上图所示，基于SR-IOV的PCIe设备由多个物理子设备PF（Physical Function）和多组虚拟子设备VF（Virtual Function）组成。其中每一组VF与一个PF对应。在上图中存在M个PF，分别为 $\mathrm{PFO}\sim \mathrm{M}$ 。其中“ $\mathrm{VFO},1\sim \mathrm{N}1$ ”与PFO对应；而“VFM， $1\sim \mathrm{N}2$ ”与PFM对应。

![[pci_express/d45e8868fd94ba2b86ddbd44052977355de16f27db932d1ea734fe352c3330f4.jpg]]  
图13-13 基于SR-IOV的PCIe设备

其中每个PF都有唯一的配置空间，而与PF对应的VF共享该配置空间，每一个VF都有独立的BAR空间，分别为VF BAR0\~5。从逻辑关系上看，这种做法相当于在一个PF中，存在多个虚拟设备。

在虚拟化环境中，每个虚拟机可以与一个VF绑定。假设在一个处理器系统中，网卡使用了SR-IOV技术，该网卡由一个PF和多个VF组成。其中每个虚拟机可以使用一个VF，从而实现多个虚拟机使用一个物理网卡的目的。

PCIe总线设置了SingleRootI/OVirtualizationExtendedCapabilities结构以支持SR-IOV机制。对此感兴趣的读者可参阅SingleRootI/O VirtulizationandSharingSpecification。这些细节对于非虚拟化技术的开发者并不重要。从本质上说，SR-IOV技术与多线程处理器技术类似，只是多线程处理器技术应用于处理器领域，而SR-IOV将同样的概念应用于PCIe设备。

# 13.3.2 MR-IOV技术

MR-IOV技术的主要功能是将处理器系统的PCI总线域划分为多个虚拟PCI总线域，从而多个处理器系统可以共享同一个物理PCI总线域。MR-IOV技术引入了几个新的概念，MRA RP、MRA Devices和MRA Switch。其中MRA Switch的结构如图13-14所示。

MRA Switch 与传统的 Switch 相比，其结构有较大不同。

- MRA Switch 由 0 个或者多个上游端口组成，如图 13-14 所示 MRA Switch 可以与多个 RP 连接，这个 RP 可以是 MRA RP 也可以是传统的 RP。在某些应用中，MRA Switch 可以作为中间节点与其他 MRA RP 相连，此时该 MRA Switch 不需要上游端口。  
- MRA Switch 由 0 个或者多个下游端口组成，MRA Switch 可以与多个 MRA 设备连接，

也可以连接SR-IOV设备和传统的PCIe设备。MRA Switch可以作为中间节点与其他MRA Switch相连，此时该MRA Switch可以不需要下游端口。

![[pci_express/4ed9694d8d421ac3f0cc24928ff6de642c5f71aff5a756555cd082b6027fa6fd.jpg]]  
图13-14 MRA Switch的结构

- 使用MRA Switch可以组成多个虚拟PCI总线域VHs（Virtual Hierarchies）。如图13-14所示，MRA Switch由3套P2P桥组成，每一套P2P可以组成1个PCI总线域，这3个PCI总线域的地址空间独立。  
- MRA Switch 可支持若干个双向端口，与其他 MRA Switch 和 MRA RP 相连。处理器系统之间可以使用这些双向端口组成复杂的服务器系统。

MRA Switch 的设计与实现较为复杂，而且该类芯片的应用范围较为有限，目前仅可能应用在支持虚拟化技术的高端服务器上。更为重要的是，Intel 并没有制作 Switch 芯片的传统，因此在短时间之内 MRA Switch 仅可能出现在 MR-IOV 规范中，而很难有实际的芯片。Intel 目前还没有支持 MRA RP 的 Chipset，但是可以预计 MRA RP 一定出现在 MRA Switch 之前，因而目前应该关注 MRA RP 与 MRA Devices 的连接拓扑结构。

MRA Devices 与 SR-IOV 设备相比略有不同，MRA Devices 略微更改了数据链路层。此外 MRA Devices 还重新定义了 SR-IOV 设备的 PF 和 VF，使得这些 PF 或者 VF 可以分属于不同的 PCI 总线域。

MRA Devices 可以与 MRA RP 或者 MRA Switch 联合使用，从而组成独立的 PCI 总线域。这些独立的 PCI 总线域可以与多个独立的虚拟机组成多个虚拟处理器系统。在这种结构下，不同的存储器域可以使用独立的 PCI 总线域，以最大限度地实现虚拟机对外部设备的隔离访问。MRA Device 的组成结构如图 13-15 所示。

在MRA Device中含有1个新的子设备BF（Base Function），该设备存放管理MRA Devices的MR-IOV的Capability结构，该结构用来管理在MRA Device的PCI总线域。除此之外，在该结构中还可以存放与设备相关的寄存器，如网卡使用的MAC地址。BF使用“BF0：f”进行描述，其中“0”表示BF使用的VH号，而“f”表示BF使用的Function号，其

值在 $0 \sim 255$ 之间。值得注意的是BF使用的VH号只能为0。

![[pci_express/420ed563ceb6f088e7aa6d83ad19642a84ed34928d035895534166057b269ac2.jpg]]  
图13-15 MRA Device的结构

在MRA Device中，如果含有多个PF设备，这种MRA Device也被称为SR-IOV/MRA Devices。这些PF设备使用“PF h：f”进行编码，其中“h”表示PF使用的VH号，而“f”表示PF使用的Function号，其值在0\~255之间。

而与SR-IOV类似，在MRA Device中还存在多组VF，其中每一组VF与一个PF对应，并使用“VF h：f，s”描述，其中“h”表示VF使用的VH号，而“f”表示PF使用的Function号，“s”表示VF号。

由以上描述可见，在MRA Device中，PF和VF可以分别属于不同的虚拟PCI总线域VH，并与MRA RP或者MRA Switch连接，形成一个完整的PCI总线域。为简便起见，本节仅介绍MRA RP与SR-IOV设备、SR-IOV/MRA Devices和MRA Devices的连接关系，如图13-16所示。

![[pci_express/a2509ff821d6c334c0c7447416730a00c9088c28050cc3e176df4e12425cc5ad.jpg]]  
图13-16 MRA RP与MRA Device的连接

该图所示的 MRA RC 支持三个虚拟 PCI 总线域，分别为 VHA、VHB 和 VHC，并包含三

个MRA RP，其中RP1与SR-IOV Device X、RP2与SR-IOV/MRA Device Y、RP3与MRA Device Z连接。

Device X仅含有一个PCI总线域，因此只能指派给一个虚拟PCI总线域，假设该设备使用VHA；Device Y中含有2个PCI总线域，假设该设备的VH1与VHB对应，而VHO与VHA对应；Device Z中含有3个PCI总线域，假设该设备的VH2与VHC对应，VH1与VHB对应，而VHO与VHA对应。

由此可知，在当前处理器系统中，虚拟PCI总线域VHA中包含DeviceX中的全部子设备、DeviceY中的“PF0:1”、“VF0:1，1”和“VF0:1，2”和DeviceZ中的“F0:0”；虚拟PCI总线域VHB中包含DeviceY的“PF1:0”和DeviceZ中的“F1:0”；而虚拟PCI总线域VHC中包含DeviceZ中的“F2:0”。

假设在处理器系统中含有3个虚拟机，这3个虚拟机可以分别使用VHA、B和C这3个不同的虚拟PCI总线域，从而实现对PCI设备的隔离访问。MR-IOV技术的实现细节较为复杂，本节对此不做深入介绍。

通过以上描述，可以发现使用MR-IOV技术，通过为虚拟机提供独立的PCI总线域，较好地解决了虚拟机对外部设备的隔离访问。而且处理器系统还可以使用MRA Switch组成更为复杂的网络拓扑结构，从而便于实现基于多个SMP系统的虚拟机。但是目前尚无支持MR-IOV技术的RP和Switch。

# 13.4 小结

本章简单介绍了PCIe总线与虚拟化技术相关的内容。读者需要获得与处理器相关的虚拟化知识后，才能进一步理解这些内容。囿于篇幅，本章没有进一步介绍虚拟化技术的实现细节。

第II篇的内容到此告一段落，在本篇中较为详细地介绍了PCIe总线的层次结构，流量控制机制，电源管理、序和死锁以及虚拟化技术等一系列内容。本篇的内容并不局限于PCIe总线本身，希望读者可以从本篇中了解通用总线的设计与实现过程，以及值得注意的实现细节。

# 第Ⅲ篇 Linux与PCI总线

本篇主要讲述Linux系统与PCI/PCIe总线相关的一些内容，其重点在于Linux系统PCI/PCIe总线驱动程序的实现。并以此为基础说明PCI总线控制器及其相关设备在系统软件的初始化过程。本篇并不会拘泥于Linux系统的实现细节，但是仍将介绍一些与Linux系统相关的基本知识。本篇内容基于Linux2.6.31.6内核。

值得注意的是，在不同处理器体系结构中，Linux系统初始化PCI总线的过程并不相同。如在Linux x86系统中，BIOS为PCI总线的初始化做出了许多辅助工作，而在Linux PowerPC或者Linux ARM中使用的Firmware，如U-Boot，并没有做类似的工作。

从系统软件的角度来看，PCI总线与PCIe总线的初始化过程和资源分配较为类似，为节约篇幅，本篇将PCI和PCIe总线统称为PCI总线，并将Linux系统的PCI和PCIe子系统简称为Linux PCI。

在第12.3节中讲述了一个最基本的、基于PCI总线的Linux设备驱动程序。这个PCI设备驱动程序使用了一些Linux系统提供的标准API和数据结构，例如使用pciResource\_start和pciResource\_len函数获得该设备BAR空间的基地址和长度，并在request\_irq函数中使用pci\_dev $\rightarrow$ IRQ参数注册该设备使用的中断服务例程。

该 PCI 设备（Capric 卡）在驱动程序中使用的这些存储器资源，由系统软件对 PCI 总线进行初始化时确定，而中断资源在使能相应的 PCI 设备时由系统软件分配。这个系统软件包括操作系统和 Firmware<sup>①</sup>。

与其他处理器系统相比，x86处理器作为一个通用处理器平台，始终强调向前兼容的重要性。而实现向前兼容需要做出许多牺牲，这也造成了Linux x86对PCI总线的初始化过程最为复杂也最为繁琐，x86处理器在引入了ACPI（Advanced Configuration and Power Interface Specification）机制之后，方便了处理器系统对“不规范外部设备”的管理，但是使得PCI总线的初始化过程更为复杂。

下文将以Linux x86为主线说明PCI总线的初始化过程。Linux x86在对PCI总线进行初始化之前，BIOS对PCI总线做出了部分初始化工作，如创建ACPI表、预先分配PCI设备使用的存储器资源，并执行PCI设备ROM中的初始化代码等一系列步骤。

Linux x86 将继承 BIOS 对 PCI 总线的初始化成果，并在此基础上进行 Linux PCI 子系统

的初始化，并执行PCI设备的Linux驱动程序的初始化模块。在Linux x86中，PCI总线的初始化由一系列模块协调完成。

Linux x86 首先使用“make menuconfig”命令对内核进行必要的配置，然后产生.config文件。假定在.config文件中，CONFIG\_PCI、CONFIG\_PCI\_MSB、CONFIG\_PCI\_GOANY、CONFIG\_PCI\_BIOS、CONFIG\_PCI\_DIRECT、CONFIG\_PCI\_MMCONFIG等一些必要的参数为“y”，即使能PCI总线驱动、使能MSI中断请求机制等，而且对x86处理器非常重要的CONFIG ACPI参数也为“y”。

在Linux PCI中，有两个常用的数据结构，分别为pci\_dev和pci BUS结构。这两个数据结构的定义在./include/linux/pci.h文件中。其中pci\_dev结构描述PCI设备，包括这个PCI设备的配置寄存器信息，使用的中断资源，还有一些和SR-IOV相关的参数。而pci BUS结构描述PCI桥，包括这个PCI桥的配置寄存器信息和一些状态信息。该结构中self参数值得注意，pci BUS $\rightarrow$ self指向一个pci\_dev结构，该结构用于PCI桥的上游总线访问PCI桥，此时PCI桥被当作一个设备。

