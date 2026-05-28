---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "02"
section: "2.2.4 x86处理器的HOST主桥"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 2.2.4 x86处理器的HOST主桥

x86处理器使用南北桥结构连接CPU和PCI设备。其中北桥（North Bridge）连接快速设备，如显卡和内存条，并推出PCI总线，HOST主桥包含在北桥中。而南桥（South Bridge）连接慢速设备。x86处理器使用的南北桥结构如图2-6所示。

![[pci_express/3a7614e2d957d35ca9a8291892edba893e6dfaa2d4c81998b59ccbbe56d1da66.jpg]]  
图2-6 x86处理器的南北桥结构

Intel 使用南北桥概念统一 PC 架构。但是从体系结构的角度上看，南北桥架构并不重要，北桥中存放的主要部件不过是存储器控制器、显卡控制器和 HOST 主桥而已，而南桥存放的是一些慢速设备，如 ISA 总线和中断控制器等。

不同的处理器系统集成这些组成部件的方式并不相同，如PowerPC、MIPS和ARM处理器系统通常将CPU和主要外部设备都集成到一颗芯片中，组成一颗基于SoC架构的处理器系统。这些集成方式并不重要，每一个处理器系统都有其针对的应用领域，不同应用领域的需求对处理器系统的集成方式有较大的影响。Intel采用的南北桥架构针对x86处理器的应用领域而设计，并不能说采用这种结构一定比MPC8548处理器中既含有HOST-to-PCI主桥也含有HOST-to-PCIe主桥更为合理。

在许多嵌入式处理器系统中，既含有PCI设备也含有PCIe设备，为此MPC8548处理器同时提供了PCI总线和PCIe总线接口，在这个处理器系统中，PCI设备可以与PCI总线直接相连，而PCIe设备可以与PCIe总线直接相连，因此并不需要使用PCIe桥扩展PCI总线，从而在一定程度上简化了嵌入式系统的设计。

嵌入式系统所面对的应用千变万化，进行芯片设计时所要考虑的因素相对较多，因而在某种程度上为设计带来了一些难度。而x86处理器系统所面对的应用领域针对个人PC和服务器，向前兼容和通用性显得更加重要。在多数情况下，一个通用处理器系统的设计难度超过专用处理器系统的设计，Intel为此付出了极大的代价。

在一些相对较老的北桥中，如Intel440系列芯片组中包含了HOST主桥，从系统软件的角度上看HOST-to-PCI主桥实现的功能与HOST-to-PCIe主桥实现的功能相近。本节仅简单介绍Intel的HOST-to-PCI主桥如何产生PCI的配置周期，有关IntelHOST-to-PCIe主桥的详细信息参见第5章。

x86处理器定义了两个I/O端口寄存器，分别为CONFIG\_ADDRESS和CONFIG\_DATA寄存器，其地址为0xEF8和0xCFC。x86处理器使用这两个I/O端口访问PCI设备的配置空间。PCI总线规范也以这两个寄存器为例，说明处理器如何访问PCI设备的配置空间。其中CONFIG\_ADDRESS寄存器存放PCI设备的ID号，而CONFIG\_DATA寄存器存放进行配置读写的数据。

CONFIG\_ADDRESS 寄存器与 PowerPC 处理器中的 CFG\_ADDR 寄存器的使用方法类似，而 CONFIG\_DATA 寄存器与 PowerPC 处理器中的 CFG\_DATA 寄存器的使用方法类似。CONFIG\_ADDRESS 寄存器的结构如图 2-7 所示。

![[pci_express/2157babec69b2fbc87e8ad3c6cc2f5e00979d3d39899dd31d18a091d9dc8bc4a.jpg]]  
图2-7 CONFIG\_ADDRESS寄存器的结构

CONFIG\_ADDRESS 寄存器的各个字段和位的说明如下所示。

- Enable 位，第 31 位。该位为 1 时，对 CONFIG\_DATA 寄存器进行读写时将引发 PCI 总线的配置周期。  
- Bus Number字段，第 $23\sim 16$ 位，记录PCI设备的总线号。  
- Device Number 字段，第 15\~11 位，记录 PCI 设备的设备号。  
- Function Number字段，第 $10\sim 8$ 位，记录PCI设备的功能号。  
- Register Number 字段，第 7\~2 位，记录 PCI 设备的寄存器号。

当x86处理器对CONFIG\_DATA寄存器进行I/O读写访问，且CONFIG\_ADDR寄存器的Enable位为1时，HOST主桥将这个I/O读写访问转换为PCI配置读写总线事务，然后发送到PCI总线上，PCI总线根据保存在CONFIG\_ADDR寄存器中的ID号，将PCI配置读写请求发送到指定PCI设备的指定配置寄存器中。

x86处理器使用小端地址模式，因此从CONFIG\_DATA寄存器中读出的数据不需要进行模式转换，这点和PowerPC处理器不同，此外x86处理器的HOST主桥也实现了存储器域到PCI总线域的地址转换，但是这个概念在x86处理器中并不明晰。

本书将在第5章以HOST-to-PCIe主桥为例，详细介绍Intel处理器的存储器地址与PCI总线地址的转换关系，而在本节不对x86处理器的HOST主桥做进一步说明。x86处理器系统的升级速度较快，目前在x86的处理器体系结构中，已很难发现HOST主桥的身影。

目前Intel对南北桥架构进行了升级，其中北桥被升级为MCH（Memory Controller Hub），而南桥被升级为ICH（I/O Controller Hub）。x86处理器系统在MCH中集成了存储器控制器、

显卡芯片和HOST-to-PCIe主桥，并通过Hub Link与ICH相连；而在ICH中集成了一些相对低速总线接口，如AC'97、LPC（Low Pin Count）、IDE和USB总线，当然也包括一些低带宽的PCIe总线接口。

在Intel最新的Nehalem处理器系统中，MCH被一分为二，存储器控制器和图形控制器已经与CPU内核集成在一个DIE中，而MCH剩余的部分与ICH合并成为PCH（Peripheral Controller Hub）。但是从体系结构的角度上看，这些升级与整合并不重要。

目前Intel在Menlow平台基础上，计划推出基于SoC架构的x86处理器，以进军手持设备市场。在基于SoC构架的x86处理器中将逐渐淡化Chipset的概念，其拓扑结构与典型的SoC处理器，如ARM和PowerPC处理器，较为类似。

# 2.3 PCI桥与PCI设备的配置空间

PCI设备都有独立的配置空间，HOST主桥通过配置读写总线事务访问这段空间。PCI总线规定了三种类型的PCI配置空间，分别是PCI Agent设备使用的配置空间，PCI桥使用的配置空间和Cardbus桥片使用的配置空间。

本节重点介绍PCI Agent和PCI桥使用的配置空间，而并不介绍Cardbus桥片使用的配置空间。值得注意的是，在PCI设备配置空间中出现的地址都是PCI总线地址，属于PCI总线域地址空间。

# 2.3.1 PCI桥

PCI桥的引入使PCI总线极具扩展性，也极大地增加了PCI总线的复杂度。PCI总线的电气特性决定了在一条PCI总线上挂接的负载有限，当PCI总线需要连接多个PCI设备时，需要使用PCI桥进行总线扩展，扩展出的PCI总线可以连接其他PCI设备，包括PCI桥。在一棵PCI总线树上，最多可以挂接256个PCI设备，包括PCI桥。PCI桥在PCI总线树中的位置如图2-8所示。

PCI桥作为一个特殊的PCI设备，具有独立的配置空间。但是PCI桥配置空间的定义与PCI Agent设备有所不同。PCI桥的配置空间可以管理其下PCI总线子树的PCI设备，并可以优化这些PCI设备通过PCI桥的数据访问。PCI桥的配置空间在系统软件遍历PCI总线树时进行配置，系统软件不需要专门的驱动程序设置PCI桥的使用方法，这也是PCI桥被称为透明桥的主要原因。

在某些处理器系统中，还有一类PCI桥，叫做非透明桥。非透明桥不是PCI总线定义的标准桥片，但是在使用PCI总线挂接另外一个处理器系统时非常有用，非透明桥片的主要作用是连接两个不同的PCI总线域，进而连接两个处理器系统，本章将在第2.5节中详细介绍PCI非透明桥。

使用 PCI 桥可以扩展出新的 PCI 总线，在这条 PCI 总线上还可以继续挂接多个 PCI 设

备。PCI桥跨接在两个PCI总线之间，其中距离HOST主桥较近的PCI总线被称为该桥片的上游总线（Primary Bus），距离HOST主桥较远的PCI总线被称为该桥片的下游总线（Secondary Bus）。如图2-8所示，PCI桥1的上游总线为PCI总线x0，而PCI桥1的下游总线为PCI总线x1。这两条总线间的数据通信需要通过PCI桥1。

![[pci_express/eb7f3b17709c6eb98ac2108d2abfce655cf823499d3987a33c316d26e16e940d.jpg]]  
图2-8 使用PCI桥扩展PCI总线

通过PCI桥连接的PCI总线属于同一个PCI总线域，在图2-8中，PCI桥1、2和3连接的PCI总线都属于PCI总线 $\mathbf{x}$ 域。在这些PCI总线域上的设备可以通过PCI桥直接进行数据交换而不需要进行地址转换；而分属不同PCI总线域的设备间的通信需要进行地址转换，如与PCI非透明桥两端连接的设备之间的通信。

如图2-8所示，每一个PCI总线的下方都可以挂接一个到多个PCI桥，每一个PCI桥都可以推出一条新的PCI总线。在同一条PCI总线上的设备之间的数据交换不会影响其他PCI总线。如PCI设备21与PCI设备22之间的数据通信仅占用PCI总线x2的带宽，而不会影响PCI总线 $\mathrm{x0}$ 、 $\mathrm{x1}$ 与 $\mathrm{x3}$ ，这也是引入PCI桥的一个重要原因。

由图2-8还可以发现PCI总线可以通过PCI桥组成一个胖树结构，其中每一个桥片都是父节点，而PCI Agent设备只能是子节点。当PCI桥出现故障时，其下的设备不能将数据传递给上游总线，但是并不影响PCI桥下游设备间的通信。当PCI桥1出现故障时，PCI设备11、PCI设备21和PCI设备22将不能与PCI设备01和存储器进行通信，但是PCI设备21和PCI设备22之间的通信可以正常进行。

使用PCI桥可以扩展一条新的PCI总线，但是不能扩展新的PCI总线域。如果当前系统使用32位的PCI总线地址，那么这个系统的PCI总线域的地址空间为4GB大小，在这个总线域上的所有设备将共享这个4GB大小的空间。如在PCI总线 $\mathbf{X}$ 域上的PCI桥1、PCI设备01、PCI设备11、PCI桥2、PCI设备22和PCI设备22等都将共享一个4GB大小的空间。再次强调这个4GB空间是PCI总线 $\mathbf{X}$ 域的“PCI总线地址空间”，和存储器域地址空间和PCI总线y域没有直接联系。

处理器系统可以通过HOST主桥扩展出新的PCI总线域，如MPC8548处理器的HOST主

桥 $\mathbf{X}$ 和y可以扩展出两个PCI总线域 $\mathrm{x}$ 和y。这两个PCI总线域 $\mathbf{X}$ 和y之间的PCI空间在正常情况下不能直接进行数据交换，但是PowerPC处理器可以通过设置PIWARn寄存器的TGI字段使得不同PCI总线域的设备直接通信，详见第2.2.3节。

许多处理器系统使用的 PCI 设备较少，因而并不需要使用 PCI 桥。因此在这些处理器系统中，PCI 设备都是直接挂接在 HOST 主桥上，而不需要使用 PCI 桥扩展新的 PCI 总线。即便如此读者也需要深入理解 PCI 桥的知识。

PCI桥对于理解PCI和PCIe总线都非常重要。在PCIe总线中，虽然在物理结构上并不含有PCI桥，但是与PCI桥相关的知识在PCIe总线中无处不在，比如在PCIe总线的Switch中，每一个端口都与一个虚拟PCI桥对应，Switch使用这个虚拟PCI桥管理其下PCI总线子树的地址空间。

# 2.3.2 PCI Agent 设备的配置空间

在一个具体的处理器应用中，PCI设备通常将PCI配置信息存放在 $\mathrm{E}^2\mathrm{PROM}$ 中。PCI设备进行上电初始化时，将 $\mathrm{E}^2\mathrm{PROM}$ 中的信息读到PCI设备的配置空间中作为初始值。这个过程由硬件逻辑完成，绝大多数PCI设备使用这种方式初始化其配置空间。

读者可能会对这种机制产生一个疑问，如果系统软件在 PCI 设备将 $\mathrm{E}^2\mathrm{PROM}$ 中的信息读到配置空间之前，就开始操作配置空间，会不会带来问题？因为此时 PCI 设备的初始值并不“正确”，仅仅是 PCI 设备使用的复位值。

读者的这种担心是多余的，因为 PCI 设备在配置寄存器没有初始化完毕之前，即 $\mathrm{E}^2\mathrm{PROM}$ 中的内容没有导入 PCI 设备的配置空间之前，可以使用 PCI 总线规定的“Retry”周期使 HOST 主桥在合适的时机重新发起配置读写请求。

在x86处理器中，系统软件使用CONFIG\_ADDR和CONFIG\_DATA寄存器，读取PCI设备配置空间的这些初始化信息，然后根据处理器系统的实际情况使用DFS算法，初始化处理器系统中所有PCI设备的配置空间。

在PCI Agent设备的配置空间中包含了许多寄存器，这些寄存器决定了该设备在PCI总线中的使用方法，本节不会全部介绍这些寄存器，因为系统软件只对部分配置寄存器感兴趣。PCI Agent设备使用的配置空间如图2-9所示。

在PCI Agent设备配置空间中包含的寄存器如下所示

# （1）DeviceID和VendorID寄存器

这两个寄存器的值由PCISIG分配，只读。其中VendorID代表PCI设备的生产厂商，而DeviceID代表这个厂商所生产的具体设备。如Intel公司的基于82571EB芯片的系列网卡，其VendorID为 $0\mathrm{x}8086^{\ominus}$ ，而DeviceID为 $0\mathrm{x}105\mathrm{E}^{\ominus}$ 。

其中0x8086代表Intel，0x105E代表82571EB网卡芯片。Intel将0x10xx作为LAN设备的DeviceID。Intel在PCISIG上注册了多如牛毛的DeviceID，这些DeviceID放在一起，几页纸也列不完。不过16位的DeviceID即便对于Intel这样大的公司，也基本没有用完的可能。当VendorID寄存器为0xFFFF时，表示为无效VendorID。

![[pci_express/59e108d0a50e23d1a4f45690aa3176184e5cdafe21f5a5775e572a50f429fadc.jpg]]  
图2-9 PCI Agent设备的配置空间

# (2) Revision ID 和 Class Code 寄存器

这两个寄存器只读。其中 Revision ID 寄存器记载 PCI 设备的版本号。该寄存器可以被认为是 Device ID 寄存器的扩展。

而Class Code寄存器记载PCI设备的分类，该寄存器由三个字段组成，分别是Base Class Code、Sub Class Code和Interface。其中Base Class Code将PCI设备分类为显卡、网卡、PCI桥等设备；Sub Class Code对这些设备进一步细分；而Interface定义编程接口。Class Code寄存器可供系统软件识别当前PCI设备的分类。

除此之外硬件逻辑设计也需要使用该寄存器识别不同的设备。当Base Class Code寄存器为0x06，Sub Class Code寄存器为0x04时，如果Interface寄存器为0x00，表示当前PCI设备为一个标准的PCI桥；如果Interface寄存器为0x01，表示当前PCI设备为一个使用“负向译码”的PCI桥。

硬件逻辑需要根据这些寄存器判断当前 PCI 桥的使用方式，许多 PCI 桥既可以支持“正向”译码，也可以支持“负向”译码，系统软件必须合理设置 Class Code 寄存器。有关正向译码与负向译码的详细说明见第 3.2.1 节。

# (3) Header Type 寄存器

该寄存器只读，由8位组成。

- 第7位为1表示当前PCI设备是多功能设备，为0表示为单功能设备。  
- 第6\~0位表示当前配置空间的类型，为0表示该设备使用PCI Agent设备的配置空间，普通PCI设备都使用这种配置头；为1表示使用PCI桥的配置空间，PCI桥使用这种配置头；为2表示使用Cardbus桥片的配置空间，CardBus桥片使用这种配置头，本书对这类配置头不作详解。

系统软件需要使用该寄存器区分不同类型的 PCI 配置空间，该寄存器的初始化必须与 PCI 设备的实际情况对应，而且必须为一个合法值。

# (4) Cache Line Size 寄存器

该寄存器记录HOST处理器使用的Cache行长度。在PCI总线中和Cache相关的总线事务，如存储器写并无效和Cache多行读等总线事务需要使用这个寄存器。值得注意的是，该寄存器由系统软件设置，但是在PCI设备的运行过程中，只有其硬件逻辑才会使用该寄存器，比如PCI设备的硬件逻辑需要得知处理器系统Cache行的大小，才能进行存储器写并无效总线事务，单行读和多行读总线事务。

如果PCI设备不支持与Cache相关的总线事务，系统软件可以不设置该寄存器，此时该寄存器为初始值 $0\mathrm{x}00$ 。对于PCIe设备，该寄存器的值无意义，因为PCIe设备在进行数据传送时，在其报文中含有一次数据传送的大小，PCIe总线控制器可以使用这个“大小”，判断数据区域与Cache行的对应关系。

# (5）SubsystemID和SubsystemVendorID寄存器

这两个寄存器和Device ID及Vendor ID类似，也是记录PCI设备的生产厂商和设备名称。但是这两个寄存器和Device ID及Vendor ID寄存器略有不同。下面以一个实例说明Subsystem ID和Subsystem Vendor ID的用途。

Xilinx公司在FGPA中集成了一个PCIe总线接口的IP核，即LogiCORE。用户可以使用LogiCORE设计各种各样基于PCIe总线的设备，但是这些设备的DeviceID都是0x10EE，而VendorID为 $0\mathrm{x}0007^{\ominus}$ 。

因此仅使用Device ID和Vendor ID寄存器无法区分这些设备。此时必须使用Subsystem ID和Subsystem Vendor ID。如果Intel也使用LogiCORE设计一款网卡适配器，那么这个基于LogiCORE的网卡适配器的Subsystem Vendor ID寄存器为0x8086，而Subsystem ID寄存器将是 $0\mathrm{x}10\mathrm{xx}$

# (6) Expansion ROM base address 寄存器

有些PCI设备在处理器还没有运行操作系统之前，就需要完成基本的初始化设置，比如显卡、键盘和硬盘等设备。为了实现这个“预先执行”功能，PCI设备需要提供一段ROM程序，而处理器在初始化过程中将运行这段ROM程序，初始化这些PCI设备。Expansion ROM base address记载这段ROM程序的基地址。

# （7）Capabilities Pointer 寄存器

在PCI设备中，该寄存器是可选的，但是在PCI-X和PCIe设备中必须支持这个寄存器，Capabilities Pointer寄存器存放Capabilities寄存器组的基地址，PCI设备使用Capabilities寄存器组存放一些与PCI设备相关的扩展配置信息。该组寄存器的详细说明见第4.3节。

# (8) Interrupt Line 寄存器

这个寄存器是系统软件对 PCI 设备进行配置时写入的，该寄存器记录当前 PCI 设备使用的中断向量号，设备驱动程序可以通过这个寄存器，判断当前 PCI 设备使用处理器系统中的哪个中断向量号，并将驱动程序的中断服务例程注册到操作系统中。

该寄存器由系统软件初始化，其保存的值与8259A中断控制器相关，该寄存器的值也是由PCI设备与8259A中断控制器的连接关系决定的。如果在一个处理器系统中，没有使

用8259A中断控制器管理PCI设备的中断，则该寄存器中的数据并没有意义。

在多数PowerPC处理器系统中，并不使用8259A中断控制器管理PCI设备的中断请求，因此该寄存器没有意义。即使在x86处理器系统中，如果使用I/O APIC中断控制器，该寄存器保存的内容仍然无效。目前在绝大多数处理器系统中，并没有使用该寄存器存放PCI设备使用的中断向量号。

# (9) Interrupt Pin 寄存器

这个寄存器保存 PCI 设备使用的中断引脚。PCI 总线提供了四个中断引脚：INTA#、INTB#、INTC# 和 INTD#。Interrupt Pin 寄存器为 1 时表示使用 INTA# 引脚向中断控制器提交中断请求，为 2 表示使用 INTB#，为 3 表示使用 INTC#，为 4 表示使用 INTD#。

如果PCI设备只有一个子设备时，该设备只能使用INTA#；如果有多个子设备时，可以使用INTB～D#信号。如果PCI设备不使用这些中断引脚，向处理器提交中断请求时，该寄存器的值必须为0。值得注意的是，虽然在PCIe设备中并不含有INTA～D#信号，但是依然可以使用该寄存器，因为PCIe设备可以使用INTx中断消息，模拟PCI设备的INTA～D#信号，详见第6.3.4节。

# （10）Base Address Register $0\sim 5$ 寄存器

该组寄存器简称为 BAR 寄存器，BAR 寄存器保存 PCI 设备使用的地址空间的基地址，该基地址保存的是该设备在 PCI 总线域中的地址。其中每一个设备最多可以有 6 个基址空间，但多数设备不会使用这么多组地址空间。

在PCI设备复位之后，该寄存器将存放PCI设备需要使用的基址空间大小，这段空间是I/O空间还是存储器空间，如果是存储器空间该空间是否可预取，有关PCI总线预读机制的详细说明见第3.4.5节。

系统软件对PCI总线进行配置时，首先获得BAR寄存器中的初始化信息，之后根据处理器系统的配置，将合理的基地址写入相应的BAR寄存器中。系统软件还可以使用该寄存器，获得PCI设备使用的BAR空间的长度，其方法是向BAR寄存器写入0xFFFF-FFFF，之后再读取该寄存器。Linux系统使用\_pci\_read\_base函数获得BAR寄存器的长度，其步骤详见第14.3.2节。

处理器访问PCI设备的BAR空间时，需要使用BAR寄存器提供的基地址。值得注意的是，处理器使用存储器域的地址，而BAR寄存器存放PCI总线域的地址。因此处理器系统并不能直接使用“BAR寄存器 + 偏移”的方式访问PCI设备的寄存器空间，而需要将PCI总线域的地址转换为存储器域的地址。在Linux系统中，一个处理器系统使用BAR空间的正确方式如源代码2-2所示。

源代码2-2 Linux系统使用BAR空间的正确方法  
pciaddr $\equiv$ pci_resource_start(pdev,1);   
if(!pciaddr){ rc=-EIO; dev_err(&pdev->dev,"no MMIO resource\n");

goto err_out_res;   
}   
... $\mathrm{regs} = \mathrm{ioremap(pciaddr,CP_REGS_SIZE)}$

在Linux系统中，使用pci\_dev $\rightarrow$ resource[bar].start参数保存BAR寄存器在存储器域的地址。在编写Linux设备驱动程序时，必须使用pci\_resource\_start函数获得BAR空间对应的存储器域的物理地址，而不能使用从BAR寄存器中读出的地址。

当驱动程序获得 BAR 空间在存储器域的物理地址后，再使用 ioremap 函数将这个物理地址转换为虚拟地址。Linux 系统直接使用 BAR 空间的方法是不正确的，如源代码 2-3 所示。

# 源代码2-3 Linux系统使用BAR空间的错误方法

ret $=$ pci_read_config_dword(pdev,1,&pciaddr); if(!pciaddr){ rc $= -$ EIO; dev_err(&pdev- $\rightharpoondown$ dev,"no MMIO resource\n"); goto err_out_res; } ... regs $=$ ioremap(pciaddr,BAR_SIZE);

在Linux系统中，使用pci\_read\_config\_dword函数获得的是PCI总线域的物理地址，在许多处理器系统中，如Alpha和PowerPC处理器系统，PCI总线域的物理地址与存储器域的物理地址并不相等。

如果x86处理器系统使能了IOMMU后，这两个地址也并不一定相等，因此处理器系统直接使用这个PCI总线域的物理地址，并不能确保访问PCI设备的BAR空间的正确性。除此之外在Linux系统中，ioremap函数的输入参数为存储器域的物理地址，而不能使用PCI总线域的物理地址。

而在pci\_dev $\rightarrow$ resource[bar].start参数中保存的地址已经经过PCI总线域到存储器域的地址转换，因此在编写Linux系统的设备驱动程序时，需要使用pci\_dev $\rightarrow$ resource[bar].start参数中的物理地址，再用ioremap函数将物理地址转换为“存储器域”的虚拟地址。

# （11）Command 寄存器

该寄存器为PCI设备的命令寄存器，在初始化时，其值为0，此时这个PCI设备除了能够接收配置请求总线事务之外，不能接收任何存储器或者I/O请求。系统软件需要合理设置该寄存器之后，才能访问该设备的存储器或者I/O空间。在Linux系统中，设备驱动程序调用pci\_enable\_device函数，使能该寄存器的I/O和Memory Space位之后，才能访问该设备的存储器或者I/O地址空间。Command寄存器的各位的含义如表2-4所示。

表 2-4 Command 寄存器

<table><tr><td>位</td><td>描述</td></tr><tr><td>0</td><td>I/O Space 位,该位表示 PCI 设备是否响应 I/O 请求,为1时响应,为0时不响应。如果 PCI 设备支持I/O 地址空间,系统软件会将该位置1。复位值为0</td></tr><tr><td>1</td><td>Memory Space 位,该位表示 PCI 设备是否响应存储器请求,为1时响应,为0时不响应。如果 PCI 设备支持存储器地址空间,系统软件会将该位置1。复位值为0</td></tr><tr><td>2</td><td>Bus Master 位。该位表示 PCI 设备是否可以作为主设备,为1时 PCI 设备可以作为主设备,为0时不能。复位值为0</td></tr><tr><td>3</td><td>Special Cycle 位,该位表示 PCI 设备是否响应 Special 总线事务,为1时响应,为0时不响应。PCI 设备可以使用 Special 总线事务,将一些信息广播发送到多个目标设备,Specail 总线事务不能穿越 PCI 桥。如果一个 PCI 设备需要将 Special 总线事务发送到 PCI 桥之下的总线时,必须使用 Type 01h 配置周期。PCI桥可以将 Type 01h 配置周期转换为 Special 周期。该位的复位值为0</td></tr><tr><td>4</td><td>Memory Write and Invalidate 位,该位表示 PCI 设备是否支持 Memory Write and Invalidate 总线事务,为1时支持,为0时不支持。许多低端的 PCI 设备不支持这种总线事务。该位对 PCIe 设备无意义</td></tr><tr><td>5</td><td>VGA Palette Snoop 位。该位为1时支持 Palette Snoop 功能,为0时不支持</td></tr><tr><td>6</td><td>Parity Error Response 位,复位值为0。该位为1,而且 PCI 设备在传送过程中出现奇偶校验错误时,PCI 设备将 PERR#信号设置为1;该位为0时,即便出现奇偶检验错误,PCI 设备也仅会将 Status 寄存器的“Detected Parity Error”位置1</td></tr><tr><td>8</td><td>SERR# Enable 位,复位值为0。该位为1,而且 PCI 设备出现错误时,将使用 SERR#信号,将这个错误信息发送给 HOST 主桥,为0时,不能使用 SERR#信号</td></tr><tr><td>9</td><td>Fast Back-to-Back 位。该位为1时,PCI 设备使用 Fast Back-to-Back(快速背靠背)总线周期,这种周期是一种提高传送效率的方法。但并不是所有的 PCI 设备都支持 Fast Back-to-Back 传送周期。该位的复位值为0</td></tr><tr><td>10</td><td>Interrupt Disable 位,复位值为0。该位为1时,PCI 设备不能通过 INTx 信号向 HOST 主桥提交中断请求,为0时可以使用 INTx 信号提交中断请求。当 PCI 设备使用 MSI 中断方式提交中断请求时,该位将被置为1</td></tr></table>

# （12）Status 寄存器

该寄存器的绝大多数位都是只读位，保存PCI设备的状态，其含义如表2-5所示。

表 2-5 Status 寄存器

<table><tr><td>位</td><td>描述</td></tr><tr><td>3</td><td>Interrupt Status位,该位只读。该位为1且Command寄存器的Interrupt Disable位为0时,表示PCI设备已经使用INTx信号向处理器提交了中断请求。在多数PCI设备中的BAR空间,存在自定义的中断状态寄存器,因此设备驱动程序很少使用该位判断PCI设备是否提交了中断请求</td></tr><tr><td>4</td><td>Capabilities List位,该位只读。该位为1时Capability Pointer寄存器中的值有效。本书在第4.3节详细介绍PCI设备的Capability Pointer寄存器</td></tr><tr><td>5</td><td>66MHz Capability位,该位只读。为1时表示此设备支持66 MHz的PCI总线</td></tr><tr><td>7</td><td>Fast Back-to-Back Capable位。该位只读,该位为1表示此设备支持快速背靠背总线周期</td></tr><tr><td>8</td><td>Master Data Parity Error位。PCI总线的PERR#信号有效时将置该位为1;当PCI总线出现数据传送错误时置此位为1;当Command寄存器的Parity Error Response位为1时,此位为1</td></tr><tr><td>9~10</td><td>DEVSEL timing字段。该字段为0b00时表示PCI设备为快速设备;为0b01时表示PCI设备为中速设备;为0b10时表示PCI设备为慢速设备。快速设备要求PCI总线主设备置FRAME#信号有效的一个时钟周期后,置DEVSEL#信号有效;中速设备要求PCI总线主设备置FRAME#信号有效的两个时钟周期后,置DEVSEL#信号有效;慢速设备要求PCI总线主设备置FRAME#信号有效的三个时钟周期后,置DEVSEL#信号有效</td></tr></table>

（续）

<table><tr><td>位</td><td>描述</td></tr><tr><td>9~10</td><td>在一条PCI总线上,如果快速设备、中速设备和慢速设备都没有使用DEVSEL#信号响应当前总线事务时,这条总线上的负向译码设备,将被动地接收这个总线事务。如果在这条总线上没有负向译码设备,主设备在FRAME#信号有效后的第4个时钟周期,使用主设备夭折时序,结束当前总线事务。有关负向译码设备的详细说明见第3.2.1节。值得注意的是,在PCI-X总线中,该字段的含义与PCI总线有所不同</td></tr><tr><td>11</td><td>Signaled Target Abort位。该位由PCI目标设备设置,当目标设备使用目标设备夭折(Target Abort)时序,结束当前总线周期时,PCI设备将置该位为1</td></tr><tr><td>12</td><td>Received Target Abort位。该位由PCI主设备设置,当发生目标设备夭折时序时,该位被置为1</td></tr><tr><td>13</td><td>Received Master Abort位。该位由PCI主设备设置,当发生主设备夭折时序,该位被置为1。当以上几个Abort位有效时,表示PCI总线的数据传送通路出现了较为严重的问题</td></tr><tr><td>14</td><td>Signaled System Error位。当设备置SERR#信号有效时,该位被置1</td></tr><tr><td>15</td><td>Detected Parity Error位。当设备发现奇偶校验错时,该位被置1</td></tr></table>

# (13) Latency Timer 寄存器

在PCI总线中，多个设备共享同一条总线带宽。该寄存器用来控制PCI设备占用PCI总线的时间，当PCI设备获得总线使用权，并使能Frame#信号后，Latency Timer寄存器将递减，当该寄存器归零后，该设备将使用超时机制停止对当前总线的使用。

如果当前总线事务为 Memeory Write and Invalidate 时，需要保证对一个完整 Cache 行的操作结束后才能停止当前总线事务。对于多数 PCI 设备而言，该寄存器的值为 32 或者 64，以保证一次突发传送的基本单位为一个 Cache 行。

PCIe 设备不需要使用该寄存器，该寄存器的值必须为 0。因为 PCIe 总线的仲裁方法与 PCI 总线不同，使用的连接方法也与 PCI 总线不同。

# 2.3.3 PCI桥的配置空间

PCI桥使用的配置空间的寄存器如图2-10所示。PCI桥作为一个PCI设备，使用的许多配置寄存器与PCI Agent的寄存器是类似的，如Device ID、Vendor ID、Status、Command、Interrupt Pin、Interrupt Line寄存器等，本节不再重复介绍这些寄存器。下面将重点介绍在PCI桥中与PCI Agent的配置空间不相同的寄存器。

与PCI Agent设备不同，在PCI桥中只含有两组BAR寄存器，即Base Address Register 0\~1寄存器。这两组寄存器与PCI Agent设备配置空间的对应寄存器的含义一致。但是在PCI桥中，这两组寄存器是可选的。如果在PCI桥中不存在私有寄存器，那么可以不使用这两组寄存器设置BAR空间。

在大多数PCI桥中都不存在私有寄存器，操作系统也不需要为PCI桥提供专门的驱动程序，这也是这类桥被称为透明桥的原因。如果在PCI桥中不存在私有空间时，PCI桥将这两个BAR寄存器初始化为0。在PCI桥的配置空间中使用两个BAR寄存器的原因是这两个32位的寄存器可以组成一个64位地址空间。

在 PCI 桥的配置空间中，有许多寄存器是 PCI 桥所特有的。PCI 桥除了作为 PCI 设备之

外，还需要管理其下连接的 PCI 总线子树使用的各类资源，即 Secondary Bus 所连接 PCI 总线子树使用的资源。这些资源包括存储器、I/O 地址空间和总线号。

![[pci_express/984af7697a6b287053a9ddb854220b248336017cdfaff137b4993dc0c96bd9c9.jpg]]  
图2-10 PCI桥的配置空间

在PCI桥中，与Secondarybus相关的寄存器包括两大类。一类寄存器管理SecondaryBus之下PCI子树的总线号，如Secondary和SubordinateBusNumber寄存器；另一类寄存器管理下游PCI总线的I/O和存储器地址空间，如I/O和MemoryLimit、I/O和MemoryBase寄存器。在PCI桥中还使用PrimaryBus寄存器保存上游的PCI总线号。

其中存储器地址空间还分为可预读空间和不可预读空间，PrefetchableMemoryLimit和PrefetchableMemoryBase寄存器管理可预读空间，而MemoryLimit、MemoryBase管理不可预读空间。在PCI体系结构中，除了ROM地址空间之外，PCI设备使用的地址空间大多都是不可预读的。

(1) Subordinate Bus Number、Secondary Bus Number 和 Primary Bus Number 寄存器

PCI桥可以管理其下的PCI总线子树。其中SubordinateBusNumber寄存器存放当前PCI子树中，编号最大的PCI总线号。而SecondaryBusNumber寄存器存放当前PCI桥SecondaryBus使用的总线号，这个PCI总线号也是该PCI桥管理的PCI子树中编号最小的PCI总线号。因此一个PCI桥能够管理的PCI总线号在SecondaryBusNumber\~SubordinateBusNumber之间。这两个寄存器的值由系统软件遍历PCI总线树时设置。

Primary Bus Number 寄存器存放该 PCI 桥上游的 PCI 总线号，该寄存器可读写。Primary Bus Number、Subordinate Bus Number 和 Secondary Bus Number 寄存器在初始化时必须为 0，系统软件将根据这几个寄存器是否为 0，判断 PCI 桥是否被配置过。

不同的操作系统使用不同的 Bootloader 引导，有的 Bootloader 可能会对 PCI 总线树进行遍历，此时操作系统不必重新遍历 PCI 总线树。在 x86 处理器系统中，BIOS 会遍历处理器系统中的所有 PCI 总线树，操作系统可以直接使用 BIOS 的结果，也可以重新遍历 PCI 总线树。而 PowerPC 处理器系统中的 Bootloader，如 U-Boot 并没有完全遍历 PCI 总线树，此时操作系统必须重新遍历 PCI 总线树。本书将在第 14 章以 Linux 系统为例说明 PCI 总线树的遍

历过程。

# (2）SecondaryStatus寄存器

该寄存器的含义与PCI Agent配置空间的Status寄存器的含义相近，PCI桥的Secondary Status寄存器记录Secondary Bus的状态，而不是PCI桥作为PCI设备时使用的状态。在PCI桥配置空间中还存在一个Status寄存器，该寄存器保存PCI桥作为PCI设备时的状态。

# (3) Secondary Latency Timer 寄存器

该寄存器的含义与PCI Agent配置空间的Latency Timer寄存器的含义相近，PCI桥的Secondary Latency Timer寄存器管理Secondary Bus的超时机制，即PCI桥发向下游的总线事务；在PCI桥配置空间中还存在一个Latency Timer寄存器，该寄存器管理PCI桥发向上游的总线事务。

