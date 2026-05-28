---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "04"
section: "4.2 PCIe体系结构的组成部件"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 4.2 PCIe体系结构的组成部件

PCIe总线作为处理器系统的局部总线，其作用与PCI总线类似，主要目的是为了连接处理器系统中的外部设备，当然PCIe总线也可以连接其他处理器系统。在不同的处理器系统中，PCIe体系结构的实现方法略有不同。但是在大多数处理器系统中，都使用了RC、Switch和PCIe-to-PCI桥这些基本模块连接PCIe和PCI设备。在PCIe总线中，基于PCIe总线的设备，也称为EP（Endpoint）。

# 4.2.1 基于PCIe架构的处理器系统

在不同的处理器系统中，PCIe体系结构的实现方式不尽相同。PCIe体系结构以Intel的x86处理器为蓝本实现，已深深地烙下x86处理器的印记。在PCIe总线规范中，有许多内容是x86处理器独有的，也仅在x86处理器的Chipset中存在。在PCIe总线规范中，一些最新的功能也在Intel的Chipset中率先实现。

本节将以一个虚拟的处理器系统A和PowerPC处理器为例简要介绍RC的实现，并简单归纳RC的通用实现机制。

# 1. 处理器系统A

在有些处理器系统中，没有直接提供PCI总线，此时需要使用PCIe桥，将PCIe链路转

换为PCI总线之后，才能连接PCI设备。在PCIe体系结构中，也存在PCI总线号的概念，其编号方式与PCI总线兼容。一个基于PCIe架构的处理器系统A如图4-7所示。

![[pci_express/8ab230800de9c4dbaf5ddde5d2be4ac10f377feed56dc5007aa993c6b086be6d.jpg]]  
图4-7 基于PCIe总线的处理器系统A

在图4-7的结构中，处理器系统首先使用一个虚拟的PCI桥分离处理器系统的存储器域与PCI总线域。FSB总线下的所有外部设备都属于PCI总线域。与这个虚拟PCI桥直接相连的总线为PCI总线0。这种架构与Intel的x86处理器系统较为类似。

在这种结构中，RC由两个FSB-to-PCIe桥和存储器控制器组成。值得注意的是在图4-7中，虚拟PCI桥的作用只是分离存储器域与PCI总线域，但是并不会改变信号的电气特性。RC与处理器通过FSB连接，而从电气特性上看，PCI总线0与FSB兼容，因此在PCI总线0上挂接的是FSB-to-PCIe桥，而不是PCI-to-PCIe桥。

在PCI总线0上有一个存储器控制器和两个FSB-to-PCIe桥。这两个FSB-to-PCIe桥分别推出一个 $\times 16$ 和 $\times 8$ 的PCIe链路，其中 $\times 16$ 的PCIe链路连接显卡控制器（GFX），其编号为PCI总线1； $\times 8$ 的PCIe链路连接一个Switch进行PCIe链路扩展。而存储器控制器作为PCI总线0的一个Agent设备，连接DDR插槽或者颗粒。

此外在这个PCI总线上还可能连接了一些使用“PCI配置空间”管理的设备，这些设备的访问方法与PCI总线兼容，在x86处理器的Chipset中集成了一些内嵌的设备。这些内嵌的设备均使用“PCI配置空间”进行管理，包括存储器控制器。

PCIe总线使用端到端的连接方式，因此只有使用Switch才能对PCIe链路进行扩展，而每扩展一条PCIe链路将产生一个新的PCI总线号。如图4-7所示，Switch可以将1个 $\times 8$ 的PCIe端口扩展为4个 $\times 2$ 的PCIe端口，其中每一个PCIe端口都可以挂接EP。除此之外PCIe总线还可以使用PCIe桥，将PCIe总线转换为PCI总线或者PCI-X总线，之后挂接PCI/

PCI-X 设备。多数 x86 处理器系统使用这种结构连接 PCIe 或者 PCI 设备。

采用这种结构，有利于处理器系统对外部设备进行统一管理，因为所有外部设备都属于同一个PCI总线域，系统软件可以使用PCI总线协议统一管理所有外部设备。然而这种外部设备管理方法并非尽善尽美，使用这种结构时，需要注意存储器控制器使用的寄存器也被映射为PCI总线空间，从而属于PCI总线域，而主存储器（如DDR内存空间）仍然属于存储器域。因此在这种结构中，存储器域与PCI总线域的划分并不明晰。

在PCIe总线规范中并没有明确提及PCIe主桥，而使用RC概括除了处理器之外的所有与PCIe总线相关的内容。在PCIe体系结构中，RC是一个很模糊也很混乱的概念。Intel使用PCI总线的概念管理所有外部设备，包括与这些外部设备相关的寄存器，因此在RC中包含一些实际上与PCIe总线无关的寄存器。使用这种方式有利于系统软件使用相同的平台管理所有外部设备，也利于平台软件的一致性，但是仍有其不足之处。

PCIe总线在x86处理器中始终处于核心地位。Intel也借PCIe总线统一管理所有外部设备，并以此构建基于PCIe总线的PC生态系统（Ecosystem）。PCI/PCIe总线在x86处理器系统中的地位超乎想象，而且并不仅局限于硬件层面。

# 2. PowerPC 处理器

PowerPC 处理器挂接外部设备使用的拓扑结构与 x86 处理器不同。在 PowerPC 处理器中，虽然也含有 PCI/PCIe 总线，但是仍然有许多外部设备并不是连接在 PCI 总线上的。在 PowerPC 处理器中，PCI/PCIe 总线并没有在 x86 处理器中的地位。在 PowerPC 处理器中，还含有许多内部设备，如 TSEC（Three Speed Ethernet Controller）和一些内部集成的快速设备，与 SoC 平台总线直接相连，而不与 PCI/PCIe 总线相连。在 PowerPC 处理器中，PCI/PCIe 总线控制器连接在 SoC 平台总线的下方。

Freescale即将发布的P4080处理器，采用的互连结构与之前的PowerPC处理器有较大的不同。P4080处理器是Freescale第一颗基于E500mc内核的处理器。E500mc内核与之前的E500V2和V1相比，从指令流水线结构、内存管理和中断处理上说并没有本质的不同。E500mc内核内置了一个128KB大小的L2 Cache，该Cache连接在BSB总线上；而E500V1/V2内核中并不含有L2 Cache，而仅含有L1 Cache，而且与FSB直接相连。在E500mc内核中，还引入了虚拟化的概念。

P4080处理器共集成了8个E500mc内核，并使用CoreNet连接这8个E500mc内核，由CoreNet互连的处理器使用交换结构进行数据交换，而不是基于共享总线结构。在P4080处理器中，一些快速外部设备，如DDR控制器、以太网控制器和PCI/PCIe总线接口控制器也是直接或者间接地连接到CoreNet中，在P4080处理器，L3 Cache也是连接到CoreNet中。P4080处理器的拓扑结构如图4-8所示。

目前 Freescale 并没有公开 P4080 处理器的 L1、L2 和 L3 Cache 如何进行 Cache 共享一致性。多数采用 CoreNet 架构互连的处理器系统使用目录表法进行 Cache 共享一致性。但是 P4080 处理器并不是一个追求峰值运算的 SMP 处理器系统，而针对 Data Plane 的应用，因此 P4080 处理器可能并没有使用基于目录表的 Cache 一致性协议。在基于全互连网络的处理器系统中如果使用“类总线监听法”进行 Cache 共享一致性，将不利于多个 CPU 共享同一个存储器系统，在 Cache 一致性的处理过程中容易形成瓶颈。

如图4-8所示，P4080处理器的设计重点并不是E500mc内核，而是CoreNet。CoreNet内部由全互连网络组成，其中任意两个端口间的通信并不会影响其他端口间的通信。与MPC8548处理器相同，P4080处理器也使用OceaN结构连接PCIe与RapidIO接口。

![[pci_express/3f39d39bd4202b84e22fa244e22e46c598100d8608c73fa129debe69b5ed31cf.jpg]]  
图4-8 基于PCIe总线的PowerPC处理器系统

在P4080处理器中不存在RC的概念，而仅存在PCIe总线控制器，当然也可以认为在P4080处理器中，PCIe总线控制器即为RC。P4080处理器内部含有3个PCIe总线控制器，如果该处理器需要连接更多的PCIe设备时，需要使用Switch扩展PCIe链路。

在P4080处理器中，所有外部设备与处理器内核都连接在CoreNet中，而不使用传统的SoC平台总线进行连接，从而在有效提高了处理器与外部设备间通信带宽的同时，极大降低了访问延时。此外P4080处理器系统使用PAMU（Peripheral Access Management Unit）分隔外设地址空间与CoreNet地址空间。在这种结构下，10GE/1GE接口使用的地址空间与PCI总线空间独立。

P4080处理器使用的PAMU是对MPC8548处理器ATMU的进一步升级。使用这种结构时，外部设备使用的地址空间、PCI总线域地址空间和存储器域地址空间的划分更加明晰。

在 P4080 处理器中，存储器控制器和存储器都属于一个地址空间，即存储器域地址空间。此外这种结构还使用 OCeaN 连接 SRIO 和 PCIe 总线控制器，使得在 OCeaN 中的 PCIe 端口之间可以直接通信，而不需要通过 CoreNet，从而减轻了 CoreNet 的负载。

从内核互连和外部设备互连的结构上看，这种结构具有较大的优势。但是采用这种结构需要使用占用芯片更多的资源，CoreNet的设计也十分复杂。而最具挑战的问题是，在这种结构之下，Cache共享一致性模型的设计与实现。

在Boxboro EX处理器系统中，可以使用QPI将多个处理器系统进行点到点连接，也可以组成一个全互连的处理器系统。这种结构与P4080处理器使用的结构类似，但是Boxboro EX处理器系统包含的CPU更多。

这种全互连的处理器结构也许是未来多核处理器发展的趋势，但是在没有合理地解决多核处理器可编程性问题之前，这种结构很可能不会被系统软件高效地利用，这也是这一结构所面临的挑战。

# 3. 基于 PCIe 总线的通用处理器结构

在不同的处理器系统中，RC的实现有较大差异。PCIe总线规范并没有规定RC的实现细则。在有些处理器系统中，RC相当于PCIe主桥，也有的处理器系统也将PCIe主桥称为PCIe总线控制器。而在x86处理器系统中，RC除了包含PCIe总线控制器之外，还包含一些其他组成部件，因此RC并不等同于PCIe总线控制器。

如果一个 RC 可以提供多个 PCIe 端口，这种 RC 也被称为多端口 RC。如 MPC8572 处理器的 RC 可以直接提供 3 条 PCIe 链路，因此可以直接连接 3 个 EP。如果 MPC8572 处理器需要连接更多 EP 时，需要使用 Switch 进行链路扩展。

而在x86处理器系统中，RC并不是存在于一个芯片中，如在Montevina平台中，RC由MCH和ICH两个芯片组成。有关Montevina平台的详细说明见第5章。本节并不对x86和PowerPC处理器使用的PCIe总线结构做进一步讨论，而只介绍这两种结构的相同之处。一个通用的、基于PCIe总线的处理器系统如图4-9所示。

![[pci_express/bb913873709ecc2c9798beb44ac674f5c0f1bddc4c53ffd2c9ef1039a2175659.jpg]]  
图4-9 基于PCIe总线的通用处理器系统

图中所示的结构将 PCIe 总线端口、存储器控制器等一系列与外部设备有关的接口都集成在一起，并统称为 RC。RC 具有一个或者多个 PCIe 端口，可以连接各类 PCIe 设备。PCIe

设备包括 EP（如网卡、显卡等设备）、Switch 和 PCIe 桥。

PCIe总线采用端到端的连接方式，每一个PCIe端口只能连接一个EP，当然PCIe端口也可以连接Switch进行链路扩展。通过Switch扩展出的PCIe链路可以继续挂接EP或者其他Switch。

# 4.2.2 RC的组成结构

RC是PCIe体系结构的一个重要组成部件，也是一个较为混乱的概念。RC的提出与x86处理器系统密切相关。事实上，只有x86处理器才存在PCIe总线规范定义的“标准RC”，而在多数处理器系统中，并不含有在PCIe总线规范中涉及的与RC相关的全部概念。

不同处理器系统的RC设计并不相同，在图4-7中的处理器系统中，RC包括存储器控制器、两个FSB-to-PCIe桥。而在图4-8中的PowerPC处理器系统中，RC的概念并不明晰。在PowerPC处理器中并不存在真正意义上的RC，而仅包含PCIe总线控制器。

在x86处理器系统中，RC内部集成了一些PCI设备、RCRB（RCRegisterBlock）和EventCollector等组成部件。其中RCRB由一系列“管理存储器系统”的寄存器组成，而仅存在于x86处理器中；而EventCollector用来处理来自PCIe设备的错误消息报文和PME消息报文。RCRB寄存器组属于PCI总线域地址空间，x86处理器访问RCRB的方法与访问PCI设备的配置寄存器相同。在有些x86处理器系统中，RCRB在PCI总线0的设备0中。

RCRB是x86处理器所独有的，PowerPC处理器也含有一组“管理存储器系统”的寄存器，这组寄存器与RCRB所实现的功能类似。但是在PowerPC处理器中，该组寄存器以CCSRBAR寄存器为基地址，处理器采用存储器映像方式访问这组寄存器。

如果将RC中的RCRB、内置的PCI设备和EventCollector去除，该RC的主要功能与PCI总线中的HOST主桥类似，其主要作用是完成存储器域到PCI总线域的地址转换。但是随着虚拟化技术的引入，尤其是引入MR-IOV技术之后，RC的实现变得异常复杂。有关MR-IOV技术的详细说明见第13.3.2节。

但是 RC 与 HOST 主桥并不相同，RC 除了完成地址空间的转换之外，还需要完成物理信号的转换。在 PowerPC 处理器的 RC 中，来自 OCeaN 或者 FSB 的信号协议与 PCIe 总线信号使用的电气特性并不兼容，使用的总线事务也并不相同，因此必须进行信号协议和总线事务的转换。

在P4080处理器中，RC的下游端口可以挂接Switch扩展更多的PCIe端口，也可以只挂接一个EP。在P4080处理器的RC中，设置了一组Inbound和Outbound寄存器组，用于存储器域与PCI总线域之间地址空间的转换；而P4080处理器的RC还可以使用Outbound寄存器组将PCI设备的配置空间直接映射到存储器域中。PowerPC处理器在处理PCI/PCIe接口时，都使用这组Inbound和Outbound寄存器组。

在P4080处理器中，RC可以使用PEX\_CONFIG\_ADDR与PEX\_CONFIG\_DATA寄存器对EP进行配置读写，这两个寄存器与MPC8548处理器HOST主桥的PCI\_CONFIG\_ADDR和PCI\_CONFIG\_DATA寄存器类似，本章不再详细介绍这组寄存器。

而x86处理器的RC设计与PowerPC处理器有较大的不同，实际上和大多数处理器系统都不相同。x86处理器赋予了RC新的含义，PCIe总线规范中涉及的RC也以x86处理器为例进行说明，而且一些在PCIe总线规范中出现的最新功能也在Intel的x86处理器系统中率

先实现。在 x86 处理器系统中的 RC 实现也比其他处理器系统复杂得多。深入理解 x86 处理器系统的 RC 对于理解 PCIe 体系结构非常重要，因此本书将以 Montivina 平台为例详细介绍 x86 处理器中的 RC，其详细描述见第 5 章。

# 4.2.3 Switch

第4.1.4节简单介绍了在PCIe总线中，如何使用Switch进行链路扩展，本节主要介绍Switch的内部结构。从系统软件的角度上看，每一个PCIe链路都占用一个PCI总线号，但是一条PCIe链路只能连接一个PCI设备、Switch、EP或者PCIe桥片。PCIe总线使用端到端的连接方式，一条PCIe链路只能连接一个设备。

一个PCIe链路需要挂接多个EP时，需要使用Switch进行链路扩展。一个标准Switch具有一个上游端口和多个下游端口。上游端口与RC或者其他Switch的下游端口相连，而下游端口可以与EP、PCIe-to-PCI-X/PCI桥或者下游Switch的上游端口相连。

PCIe总线规范还支持一种特殊的连接方式，即Crosslink连接方式。使用这种方式时，Switch的下游端口可以与其他Switch的下游端口直接连接，上游端口也可以其他Switch的上游端口直接连接。在PCIe总线规范中，Crosslink连接方式是可选的，并不要求PCIe设备一定支持这种连接方式。

在PCIe体系结构中，Switch的设计难度仅次于RC，Switch也是PCIe体系结构的核心所在。而从系统软件的角度上看，Switch内部由多个PCI-to-PCI桥组成，其中每一个上游和下游端口都对应一个虚拟PCI桥。在一个Switch中有多少个端口，在其内部就有多少个虚拟PCI桥，就有多少个PCI桥配置空间。值得注意的是，在Switch内部还具有一条虚拟的PCI总线，用于连接各个虚拟PCI桥，系统软件在初始化Switch时，需要为这条虚拟PCI总线编号。Switch的组成结构如图4-10所示。

![[pci_express/f27628187df3f154975ae1cb417cf51561cdeed6eabc2712673af241f15bf149.jpg]]  
图4-10 Switch的等效逻辑图

Switch需要处理PCIe总线传输过程中的QoS问题。PCIe总线的QoS要求PCIe总线区别对待优先权不同的数据报文，而且无论PCIe总线的某一个链路多么拥塞，优先级高的报文，如等时报文（Isochronous Packet）都可以获得额定的数据带宽。而且PCIe总线需要保证优先级较高的报文优先到达。PCIe总线采用虚拟多通路VC技术，并在这些数据报文中设定一个TC（Traffic Class）标签，该标签由3位组成，将数据报文根据优先权分为8类，这8类数据报文可以根据需要选择不同的VC进行传递。

在PCIe总线中，每一条数据链路上最多可以支持8个独立的VC。每个VC可以设置独立的缓冲，用来接收和发送数据报文。在PCIe体系结构中，TC和VC紧密相连，TC与VC之间的关系是“多对一”。

TC 可以由软件设置，系统软件可以选择某类 TC 由哪个 VC 进行传递。其中一个 VC 可以传递 TC 不相同的数据报文，而 TC 相同的数据报文在指定一个 VC 传递之后，不能再使用其他 VC。在许多处理器系统中，Switch 和 RC 仅支持一个 VC，而 x86 处理器系统和 PLX 的 Switch 中可以支持两个 VC。

下文将以一个简单的例子说明如何使用 TC 标签和多个 VC，以保证数据传送的服务质量。将 PCIe 总线的端到端数据传递过程模拟为使用汽车将一批货物从 A 点运送到 B 点。如果不考虑服务质量，可以使用一辆汽车运送所有这些货物，经过多次往返就可以将所有货物从 A 点运到 B 点。但是这样做会耽误一些需要在指定时间内到达 B 点的货物。有些货物，如一些急救物资、EMS 等其他优先级别较高的货物，必须及时地从 A 点运送到 B 点。这些急救物资的运送应该有别于其他普通物资的运送。

为此首先将不同种类的货物进行分类，将急救物资定义为TC3类货物，EMS定义为TC2类货物，平信定义为TC1类货物，一般包裹定义为TC0类货物，我们最多可以提供8种TC类标签进行货物分类。

之后我们使用8辆汽车，分别是VC0～VC7运送这些货物，其中VC7的速度最快，而VC0的速度最慢。当发生堵车事件时，VC7优先行驶，VC0最后行驶。然后我们使用VC3运送急救物资，VC2运送EMS，VC1运送平信，VC0运送包裹，当然使用VC0同时运送平信和包裹也是可以的，但是平信或者包裹不能使用一种以上的汽车运送，如平信如果使用了VC1运输，就不能使用VC0。因为TC与VC的对应关系是“多对一”的关系。

采用这种分类运输的方法，可以做到在A点到B点带宽有限的情况下，仍然可以保证急救物资和EMS可以及时到达B点，从而提高了服务质量。

PCIe总线除了解决数据传送的QoS问题之外，还进一步考虑如何在链路传递过程中，使用流量控制机制防止拥塞。PCIe总线的流量控制机制较为复杂，第9章将介绍和流量控制相关的内容。

在PCIe体系结构中，Switch处于核心地位。PCIe总线使用Switch进行链路扩展，在Switch中，每一个端口对应一个虚拟PCI桥。深入理解PCI桥是理解Switch软件组成结构的基础。目前PCIe总线提出了MRA-Switch的概念，这种Switch与传统Switch有较大的区别，

有关这部分内容详见第13.3节。

# 4.2.4 VC和端口仲裁

在Switch中存在多个端口，其中来自不同Ingress端口的报文可以发向同一个Egress端口，因此Switch必须要解决端口仲裁和路由选径的问题。所谓端口仲裁指来自不同Ingress端口的报文到达同一个Egress端口的报文通过顺序，端口仲裁机制如图4-11所示。

![[pci_express/c30035a5170f87e2f0f503f8849cfa57d25c84ac72faa7e380c43eaafd189c99.jpg]]  
图4-11 PCIe总线基于端口的仲裁机制

在一个Switch中设有仲裁器，该仲裁器规定了数据报文通过Switch的规则。在PCIe总线中存在两种仲裁机制，分别是基于VC和基于端口的仲裁机制。端口仲裁机制主要针对RC和Switch，当多个Ingress端口需要向同一个Egress端口发送数据报文时需要进行端口仲裁。具体地讲，在PCIe体系结构中有三个端口，需要进行端口仲裁。

- Switch 的 Egress 端口。当 EP A 和 EP B 同时访问 EP C，D 或者 DDR-SDRAM 时，需要通过 Switch 的 Egress 端口 C。此时 Switch 需要进行端口仲裁确定是 EP A 的数据报文还是 EP B 的数据报文优先通过 Egress 端口 C。  
- 多端口 RC 的 Egress 端口。当 RC 的端口 1 和端口 3 同时访问 Endpoint C 时，RC 的端口 2 需要进行端口仲裁，决定来自 RC 哪个端口的数据可以率先通过。  
- RC 通往主存储器的端口。当 RC 的端口 1、端口 2 和端口 3 同时访问 DDR 控制器时，这些数据报文将通过 RC 的 Egress 端口 4，此时需要进行端口仲裁。

在 PCIe 体系结构中，链路的端口仲裁需要根据每一个 VC 独立设置，而且可以使用不同的算法进行端口仲裁。

下文以图4-11中，Switch的两个Ingress端口A和B向Egress端口C发送数据报文为例，简要说明端口仲裁和VC仲裁的使用方法，其过程如图4-12所示。

基于VC的仲裁是指发向同一个端口的数据报文，根据使用的VC而进行仲裁的方式。当来自端口B和端口A数据报文（分别使用VC0和VC1通路）在到达端口C之前，需要首先进行端口仲裁后，才能进行VC仲裁。PCIe总线规定了3种VC仲裁方式，分别为StrictPriority，RR（Round Robin）和WRR（Weighted Round Robin）算法。

![[pci_express/276eef69c63ec0e99b13862d5ab5d1ab8edcf0c2e077bd4067f66ea97d824bb8.jpg]]  
图4-12 VC仲裁示意图

当使用StrictPriority仲裁方式时，发向VC7的数据报文具有最高的优先级，而发向VC0的数据报文优先级最低。PCIe总线允许对Switch或者RC的部分VC采用StrictPriority方式进行仲裁，而对其他VC采用RR和WRR算法，如VC7～VC4采用StrictPriority方式，而采用其他方式处理VC3～VC0。

使用RR方式时，所有VC具有相同的优先级，所有VC轮流使用PCIe链路。WRR方式与RR算法类似，但是可以对每一个VC进行加权处理，采用这种方式可以适当提高VC7的优先权，而将VCO的优先权适当降低。

我们假定Ingress端口A和Ingress B向Egress端口C进行数据传递时，使用两个VC通路，分别是VC0和VC1。其中标签为 $\mathrm{TC0}\sim \mathrm{TC3}$ 的数据报文使用VC0传送，而标签为 $\mathrm{TC4}\sim$ TC7数据报文使用VC1传送。

而数据报文在离开Egress端口C时，需要首先进行端口仲裁，之后再通过VC仲裁，决定哪个报文优先传送。数据报文从Ingress A/B端口发送到Egress C端口时，将按照以下步骤进行处理。

（1）首先到达Ingress A/B端口的数据报文，将根据该端口的TC/VC映射表决定使用该端口的哪个VC通道。如图4-12所示，假设发向端口A的数据报文使用 $\mathrm{TC0}\sim \mathrm{TC3}$ ，而发向端口B的数据报文使用 $\mathrm{TC0}\sim \mathrm{TC7}$ ，这些数据报文在端口A中仅使用了VCO通道，而在端口B中使用了VCO和VC1两个通道。  
(2) 数据报文在端口中传递时，将通过路由部件（Routing Logic），将报文发送到合适的端口。如图 4-12 所示，端口 C 可以接收来自端口 A 或端口 B 的数据报文。

(3) 当数据报文到达端口 C 时，首先需要经过 TC/VC 映射表，确定在端口 C 中使用哪个 VC 通路接收不同类型的数据报文。  
(4) 对于端口 C, 其 VCO 通道可能会被来自端口 A 的数据报文使用, 也可能会被来自端口 B 的数据报文使用。因此在 PCIe 的 Switch 中必须设置一个端口仲裁器, 决定来自不同数据端口的数据报文如何使用 VC 通路。  
(5) 数据报文通过端口仲裁后，获得 VC 通路的使用权之后，还需要经过 Switch 中的 VC 仲裁器，将数据报文发送到实际的物理链路中。

PCIe总线规定，系统设计者可以使用以下三种方式进行端口仲裁。

(1) Hardware-fixed 仲裁策略。如在系统设计时，采用固化的 RR 仲裁方法。这种方法的硬件实现原理较为简单，此时系统软件不能对端口仲裁器进行配置。  
(2) WRR 仲裁策略，即加权的 RR 仲裁策略，该算法和 Time-Based WRR 算法的描述见第 4.3.3 节。  
(3) Time-Based WRR 仲裁策略，基于时间片的 WRR 仲裁策略，PCIe 总线可以将一个时间段分为若干个时间片（Phase），每个端口占用其中的一个时间片，并根据端口使用这些时间片的多少对端口进行加权的一种方法。使用 WRR 和 Time-Based WRR 仲裁策略，可以在某种程度上提高 PCIe 总线的 QoS。

PCIe设备的Capability寄存器规定了端口仲裁使用的算法，详见第4.3.3节。有些PCIe设备并没有提供多种端口仲裁算法，可能也并不含有Capability寄存器。此时该PCIe设备使用Hardware-fixed仲裁策略。

# 4.2.5 PCIe-to-PCI/PCI-X桥片

本书将PCIe-to-PCI/PCI-X桥片简称为PCIe桥片。该桥片有两个作用。

- 将PCIe总线转换为PCI总线，以连接PCI设备。在一个没有提供PCI总线接口的处理器中，需要使用这类桥片连接PCI总线的外设。许多PowerPC处理器在提供PCIe总线的同时，也提供了PCI总线，因此PCIe-to-PCI桥片对基于PowerPC处理器系统并不是必须的。  
- 将PCI总线转换为PCIe总线（这也被称为Reverse Bridge），连接PCIe设备。一些低端的处理器并没有提供PCIe总线，此时需要使用PCIe桥将PCI总线转换为PCIe总线，才能与其他PCIe设备互连。这种用法初看比较奇怪，但是在实际应用中，确实有使用这一功能的可能。本节主要讲解PCIe桥的第一个作用。

PCIe桥的一端与PCIe总线相连，而另一端可以与一条或者多条PCI总线连接。其中可以连接多个PCI总线的PCIe桥也被称为多端口PCIe桥。

PCIe总线规范提供了两种多端口PCIe桥片的扩展方法。多端口PCIe桥片指具有一个上游端口和多个下游端口的桥片。其中上游端口连接PCIe链路，而下游端口推出PCI总线，连接PCI设备。这种桥片的结构如图4-13所示。

PCIe总线规范并没有强制厂商实现多端口PCIe桥的办法。但是值得注意的是，使用右图扩展多条PCI总线时，在多端口PCIe桥中包含一个虚拟的PCI总线，即Bus2。系统软件对PCI总线进行深度优先搜索DFS（Depth-First Search）时，对左图和右图的处理有些区别。目前多端口PCIe桥多使用右图进行端口扩展。

![[pci_express/d719cdfd3532bc77f5c69dbbd5cc06d588506379fb258e7e6bbd1d01266b4b95.jpg]]

![[pci_express/d41d11ee9a3929bf81e3bdd67100503e06d8da5bf62ee868dfd64822ada7bd17.jpg]]  
PCI-X Bus 3 PCI-X Bus 4

图4-13 多端口PCIe桥的扩展方法

目前虽然PCIe总线非常普及，但是仍然有许多基于PCI总线的设计，这些基于PCI总线的设计可以通过PCIe桥，方便地接入到PCIe体系结构中。目前有多家半导体厂商可以提供PCIe桥片，如PLX、NXP、Tundra和Intel。就功能的完善和性能而言，Intel的PCIe桥无疑是最佳选择，而PLX和Tundra的PCIe桥在嵌入式系统中得到了广泛的应用。

# 4.3 PCIe设备的扩展配置空间

本书在第2.3.2节讲述了PCI设备使用的基本配置空间。这个基本配置空间共由64个字节组成，其地址范围为 $0\mathrm{x}00\sim 0\mathrm{x}3\mathrm{F}$ ，这64个字节是所有PCI设备必须支持的。事实上，许多PCI设备也仅支持这64个配置寄存器。

此外PCI/PCI-X和PCIe设备还扩展了 $0\mathrm{x}40\sim 0\mathrm{x}\mathrm{FF}$ 这段配置空间，在这段空间主要存放一些与MSI或者MSI-X中断机制和电源管理相关的Capability结构。其中所有能够提交中断请求的PCIe设备，必须支持MSI或者MSI-XCapability结构。

PCIe 设备还支持 $0 \times 100 \sim 0 \times \mathrm{FFF}$ 这段扩展配置空间。PCIe 设备使用的扩展配置空间最大为 4KB，在 PCIe 总线的扩展配置空间中，存放 PCIe 设备所独有的一些 Capability 结构，而 PCI 设备不能使用这段空间。

在x86处理器中，使用CONFIG\_ADDRESS寄存器与CONFIG\_DATA寄存器访问PCIe配置空间的 $0\mathrm{x}00\sim 0\mathrm{x}\mathrm{FF}$ ，而使用ECAM方式访问 $0\mathrm{x}000\sim 0\mathrm{x}\mathrm{FFF}$ 这段空间；而在PowerPC处理器中，可以使用CFG\_ADDR和CFG\_DATA寄存器访问 $0\mathrm{x}000\sim 0\mathrm{x}\mathrm{FFF}$ ，详见第2.2节。

PCI-X和PCIe总线规范要求其设备必须支持Capabilities结构。在PCI总线的基本配置空间中，包含一个Capabilities Pointer寄存器，该寄存器存放Capabilities结构链表的头指针。在一个PCIe设备中，可能含有多个Capability结构，这些寄存器组成一个链表，其结构如图4-14所示。

其中每一个 Capability 结构都有唯一的 ID 号，每一个 Capability 寄存器都有一个指针，这个指针指向下一个 Capability 结构，从而组成一个单向链表结构，这个链表的最后一个

Capability结构的指针为0。

![[pci_express/fd534eb7c74e5d01e2a3317fa3603556ec7ad0d880e36ce226ef205598e5d7a1.jpg]]  
图4-14PCIe总线Capability结构的组成

一个PCIe设备可以包含多个Capability结构，包括与电源管理相关、与PCIe总线相关的结构、与中断请求相关的Capability结构、PCIeCapability结构和PCIe扩展的Capability结构。在本书的其他章节也将讲述这些Capability结构，读者在继续其他章节的学习之前，需要简单了解这些Capability结构的寄存器组成和使用方法。

其中读者需要重点关注的是Power Management和MSI/MSI-X Capability结构，Power Management结构将在本节介绍，而在第10章将详细讨论MSI/MSI-X Capability结构。在PCIe总线规范中，定义了较多的Capability结构，这些结构适用于不同的应用场合，在一个指定的PCIe设备中，并不一定支持本书涉及的所有Capability结构。系统软件程序员也不需要完全掌握PCIe总线规范定义的这些Capability结构。

