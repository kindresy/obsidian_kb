---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "13"
section: "第13章 PCIe总线与虚拟化技术"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第13章 PCIe总线与虚拟化技术

目前虚拟化技术在处理器体系结构中，已经占据一席之地。虚拟化技术由来已久，其含义也较为广泛，多个进程共享一个CPU，多个进程的虚拟空间共享同一个物理内存等一系列在体系结构中已经根深蒂固的概念，都可以归于虚拟化技术。

本章所强调的虚拟化技术是指在一个处理器系统中运行多个虚拟处理器系统的技术。其中每一个虚拟处理器系统都有独立的虚拟运行环境，包括CPU、内存和外部设备。在这个虚拟环境中运行的操作系统彼此独立，但是这些操作系统仍使用相同的物理资源。

因此处理器需要为虚拟化环境设置专门的硬件，以支持多个虚拟处理器系统在一个物理环境中的资源共享。虚拟化技术的核心是通过VMM（Virtual Machine Monitor）集中管理物理资源，而每个虚拟处理器系统通过VMM访问实际的物理资源。有时为了提高虚拟机访问外部设备的效率，虚拟处理器系统也可以直接访问物理资源。

在一个处理器系统中，这些物理资源包括CPU、主存储器、外部设备和中断。IA处理器使用EPT（ExtendedPageTable）和VPID技术对主存储器进行管理，而使用虚拟中断控制器接管中断请求以实现中断的虚拟化。目前这些技术较为成熟，对这些内容感兴趣的读者可参阅《系统虚拟化——原理与实现》，本章对此不做详细分析。

本章重点关注的是VMM对外部设备的管理，而在外部设备中重点关注对PCI设备的管理。在一个处理器系统中，设置了许多专用硬件，如IOMMU、PCIe总线的ATS机制、SR-IOV（SingleRootI/OVirtualization）和MR-IOV（Multi-RootI/OVirtualization）机制，便于VMM对外部设备的管理。

# 13.1 IOMMU

在多进程环境下，处理器使用MMU机制，使得每一个进程都有独立的虚拟地址空间，从而各个进程运行在独立的地址空间中，互不干扰。MMU具有两大功能，一是进行地址转换，将分属不同进程的虚拟地址转换为物理地址；二是对物理地址的访问进行权限检查，判断虚实地址转换的合理性。

在多数操作系统中，每一个进程都具有独立的页表存放虚拟地址到物理地址的映射关系和属性。但是如果进程每次访问物理内存时，都需要访问页表时，将严重影响进程的执行效率。为此处理器设置了TLB（Translation Lookaside Buffer）作为页表的Cache。如果进程的虚拟地址在TLB中命中时，则从TLB中直接获得物理地址，而不需要使用页表进行虚实地址转换，从而极大提高了访问存储器的效率。

从地址转换的角度来看，IOMMU与MMU较为类似。只是IOMMU完成的是外部设备地

址到存储器地址的转换。我们可以将一个PCI设备模拟成为处理器系统的一个特殊进程，当这个进程访问存储器时使用特殊的MMU，即IOMMU，进行虚实地址转换，然后再访问存储器。在这个IOMMU中，同样存在IO页表存放虚实地址转换关系和访问权限，而且处理器为了加速这种虚实地址的转换，还设置了IOTLB作为IO页表的Cache。单纯从这个角度来看，许多HOST主桥和RC也具备同样的功能，如PowerPC处理器的Inbound窗口和Out-bound窗口，也可以完成这种特殊的地址转换。但是这些窗口仅能完成PCI总线域到一个存储器域的地址转换，无法实现PCI总线域到多个存储器域的转换。

目前设置IOMMU的主要作用是支持虚拟化技术，当然使用IOMMU也可以实现其他功能，如使“仅支持32位地址的PCI设备”访问4GB以上的存储器空间。IA处理器和AMD处理器分别使用VT-d”和“IOMMU”，实现外部设备的地址转换。这两种技术都可以将PCI总线域地址空间转换为不同的存储器域地址空间，便于虚拟化技术的设计与实现。

# 13.1.1 IOMMU的工作原理

根据虚拟化的理论，假设在一个处理器系统中存在两个 Domain，其中一个为 Domain 1，而另一个为 Domain 2。这两个 Domain 分别对应不同的虚拟机，并使用独立的物理地址空间，分别为 GPA1（GPA 即 Guest Physical Address）和 GPA2 空间，其中在 Domain 1 上运行的所有进程使用 GPA1 空间，而在 Domain 2 上运行的所有进程使用 GPA2 空间。

GPA1和GPA2采用独立的编码格式，其地址都可以从各自GPA空间的0x0000-0000地址开始，只是GPA1和GPA2空间在SystemMemory中占用的实际物理地址HPA（HostPhysicalAddress）并不相同，HPA也被称为MPA（MachinePhysicalAddress)，是处理器系统中真实的物理地址。而PCI设备依然使用PCI总线域地址空间，PCI总线地址需要通过DMA-Remapping逻辑转换为HPA地址后，才能访问存储器。DMA-Remapping逻辑的组成结构如图13-1所示。

![[pci_express/6c5df3f3a5b99d647bdba1c135bb4068eccfcb00b14fb5aaf0c96c3bad66eae0.jpg]]  
图13-1 DMA-Remapping的实现

在以上处理器模型中，假设存在两个外部设备DeviceA和DeviceB。这两个外部设备分

属于不同的Domain，其中DeviceA属于Domain1，而DeviceB属于Domain2。在同一段时间内，DeviceA只能访问Domain1的GPA1空间，也只能被Domain1操作；而DeviceB只能访问GPA2空间，也只能被Domain2操作。DeviceA和DeviceB通过DMA-Remmaping机制最终访问不同Domain的存储器。

使用这种方法可以保证Device A/B访问的空间彼此独立，而且只能被指定的Domain访问，从而满足了虚拟化技术要求的空间隔离。这一模型远非完美，如果每个Domain都可以自由访问所有外部设备当然更加合理，但是单纯使用VT-d机制还不能实现这种访问机制。

在这种模型之下，Device A/B 进行 DMA 操作时使用的物理地址仍然属于 PCI 总线域的物理地址，Device A/B 仍然使用地址路由或者 ID 路由进行存储器读写 TLP 的传递。值得注意的是虽然在 x86 处理器系统中，这个 PCI 总线地址与 GPA 地址一一对应且相等，但是这两个地址所代表的含义仍然完全不同。

GPA地址为存储器域的地址，隶属于不同的Domain，而PCI设备使用的地址依然是PCI总线域的物理地址，只是在虚拟化环境下，PCI设备与Domain间有明确的对应关系。当这个PCI设备进行DMA读写时，TLP首先到达地址转换部件TA（Translation Agent），并通过 $\mathrm{ATPT}^{\ominus}$ （Address Translation and Protection Table）后将PCI总线域的物理地址转换为与GPA地址对应的HPA地址，然后对主存储器进行读写操作。其转换关系如图13-2所示。

![[pci_express/6ee594b086614696533da874c47ecd90ae69322129c710dcef65e4864f19c768.jpg]]  
图13-2 PCI总线域物理地址与HPA的关系

在上图所示的处理器系统中，存在两个虚拟机，其使用的地址空间分别为GPA Domain1和GPA Domain2。假设每个GPA Domain使用1GB大小的物理地址空间，而且Domain间使用的地址空间独立，其地址范围都为 $0\mathrm{x}0000 - 0000\sim 0\mathrm{x}4000 - 0000$ 。其中Domain1使用的GPA地址空间对应的HPA地址范围为 $0\mathrm{x}0000 - 0000\sim 0\mathrm{x}3\mathrm{FFF}$ -FFFF；Domain2使用的GPA地址空间对应的HPA地址范围为 $0\mathrm{x}4000 - 0000\sim 0\mathrm{x}7\mathrm{FFF}$ -FFFF。在一个处理器系统中，不同的虚拟机使用的物理空间是隔离的。

在这个处理器系统中存在两个PCIe设备，分别为EP1和EP2，其中EP1隶属于Domain1，而EP2隶属于Domain2，即EP1和EP2进行DMA操作时只能访问Domain1和Do

main2对应的HPA空间，但是EP1和EP2作为一个PCIe设备，并不知道处理器系统进行的这种绑定操作，EP1和EP2依然使用PCI总线域的地址进行正常的数据传送。因为处理器系统的这种绑定操作由TA和ATPT决定，而对PCIe设备透明。在EP1和EP2进行DMA操作时，当TLP到达TA和ATPT，经过地址转换后，才能访问实际的存储器空间。

下面以 EP1 和 EP2 进行 DMA 写操作为例，说明在这种虚拟化环境下，不同种类地址的转换关系，其步骤如下所示。

(1) Domain1 和 Domain2 填写 EP1 和 EP2 的 DMA 写地址和长度寄存器启动 DMA 操作。

其中 EP1 最终将数据写入到 GPA1 的 0x1000-0000 \~ 0x1000-007F 这段数据区域，而 EP2 最终将数据写入到 GPA2 的 0x1000-0000 \~ 0x1000-007F 这段数据区域。然而 EP1 和 EP2 仅能识别 PCI 总线域的地址。Domain1 和 Domain2 填入 EP1 和 EP2 的 DMA 写地址为 0x1000-0000，而长度为 0x80，这些地址都是 PCI 总线地址。

在x86处理器系统中，这个地址与GPA1和GPA2存储器域的地址恰好相等，但是这个地址仍然是PCI总线域的地址，只是由于IOMMU的存在，相同的PCI总线地址，可能被映射到相同的GPA地址空间，然而这些GPA地址空间对应的HPA地址空间不同。这个PCI总线地址仍然在RC中被转换为存储器域地址，并由TA转换为合适的HPA地址。

(2）EP1和EP2的存储器写TLP到达RC。

来自EP1和EP2存储器写TLP经过地址路由最终到达RC，并由RC将TLP的地址字段转发到TA和ATPT，进行地址翻译。

EP1 和 EP2 使用的 I/O 页表已经事先被 VMM 设置完毕，TA 将使用 Domain1 或者 Domain2 的 I/O 页表，进行地址翻译。EP1 隶属于 Domain1，其地址 0x1000-0000（PCI 总线地址）被翻译为 0x1000-0000（HPA）；而 EP2 隶属于 Domain2，其地址 0x1000-0000（PCI 总线地址）被翻译为 0x5000-0000（HPA）。值得注意的是在 TA 中设置了 IOTLB，以加速 I/O 页表的翻译效率，因此 TA 并不会每次都从存储器中查找 I/O 页表。

(3) 来自 EP1 和 EP2 存储器写 TLP 的数据将被分别写入到 $0 \times 1000 - 0000 \sim 0 \times 1000 - 007\mathrm{F}$ 和 $0 \times 5000 - 0000 \sim 0 \times 5000 - 007\mathrm{F}$ 这两段数据区域。  
(4) Domain1 和 Domain2 都使用 $0 \times 1000 - 0000 \sim 0 \times 1000 - 007\mathrm{F}$ 这段 GPA 地址访问来自 EP1 和 EP2 的数据，这个 GPA 地址将转换为 HPA 地址，然后发向存储器控制器。在 IA 处理器系统中，使用 EPT 和 VPID 技术进行 GPA 地址到 HPA 地址的转换。

IA处理器和AMD处理器使用不同的技术，实现TA和ATPT。其中IA处理器使用VT-d技术，而AMD使用IOMMU。从工作原理上看，这两种技术类似，但是在实现细节上，两者有较大区别。

# 13.1.2 IA处理器的VT-d

IA（Intel Architecture）处理器使用VT-d技术将PCI总线域的物理地址转换为HPA地址。这个映射过程也被称为DMA Remapping。IA处理器系统使用DMA Remapping机制可以辅助虚拟化技术对外部设备进行管理。

在 IA 处理器系统中，所有的外部设备都是 PCI 设备。每一个设备都唯一对应一个 Bus Number、Device Number 和 Function Number，为此 IA 处理器设置了一个专门的结构，即 Root

Entry Table，管理每一棵 PCI 总线树。在这种结构下，每一个 PCI 设备根据其 Bus、Device 和 Function 号唯一确定一个 Context Entry。VT-d 将这个结构称为“Device to Domain Mapping”结构，如图 13-3 所示。

![[pci_express/8b96722b166a9acd1259c1dc8bde815ca803a350a166c86e5b1cc1c1c1bdcf4c.jpg]]  
图13-3 Device to Domain Mapping结构

VT-d一共设置了两种结构描述PCI总线树结构，分别为Root Entry和Context Entry。其中Root Entry描述PCI总线，一棵PCI总线树最多有256条PCI总线，其中每一条PCI总线对应一个Root Entry；每条PCI总线中最多有32个设备，而每个设备最多有8个Function，其中每一个Function对应一个Context Entry，因此每个Context Entry表中共有256表项。

在一个处理器系统中，一个指定的 PCI Function 唯一对应一个 Context Entry，这个 Context Entry 指向这个 PCI Function 使用的地址转换结构（Address Translation Structures）。当一个 PCI Function 隶属于不同的 Domain 时，将使用不同的地址转换结构，但是在一个时间段里，PCI Function 只能使用一个地址转换结构，即 Context Entry 只能指向一个 Domain 的地址转换结构。这个地址转换结构的主要功能是完成 PCI 总线域到 HPA 存储器域的地址转换。

如图13-1所示，当一个设备进行DMA操作时，Domain使用PCI总线域的地址填写这个设备和与DMA转送相关的寄存器。当这个设备启动DMA操作时，将使用PCI总线地址，之后通过DMARemapping机制将PCI总线地址转换为HPA存储器域地址，然后将数据传送到实际的物理地址空间中。而Domain通过处理器的MMU机制将GPA转换为HPA，访问物理地址空间。

从图13-3中可以发现，每一个Function在每一个Domain中都可能有一个地址转换结构，以完成GPA到HPA的转换，因此在每一个Domain中最多有256个地址转换结构。这些结构无疑将占用部分内存，但是并不会产生较大的浪费。因为在实际设计中，同一个Do-

main 下的所有 PCI 设备使用的总线地址到 HPA 地址的转换结构可以相同。因此在实现中，每个 Domain 仅使用一个地址转换结构即可。

IA处理器使能VT-d机制后，PCI设备进行DMA操作需要根据Bus、Device和Function号确定ContextEntry，之后使用图13-4所示的方法完成PCI总线地址到HPA地址的转换。

![[pci_express/d6188d298e55d8221e20ee42d1b1ae02f947509ea663cf42bd47ab24da6f1b28.jpg]]  
图13-4 VT-d使用的PCI总线地址到HPA的转换机制

在上图中，4KB Page Table 中的每个 Entry 的大小为 8B，因此在计算偏移时，需要左移 3 位，PCI 总线地址通过 3 级目录，最终找到与 HPA 所对应的 4KB 大小的页面，从而完成 PCI 总线地址到 HPA 的转换。值得注意的是，IA 处理器还支持 2MB（ $\mathrm{SP} = 1$ ）、1GB（ $\mathrm{SP} = 2$ ）、512GB（ $\mathrm{SP} = 3$ ）和 1TB（ $\mathrm{SP} = 4$ ）大小的 Super Page，而本节仅使用了 4KB 大小的页面。

为了加快PCI总线地址到HPA地址的转换速度，IA处理器分别为Root Entry和Context Entry设置了Context Cache以加快Context Entry的获取速度，同时还设置了IOTLB加速PCI总线地址到HPA地址的转换速度。

IOTLB 相当于 I/O 页表的 Cache，当一个 PCI 设备进行 DMA 操作时，首先在 IOTLB 中查找 PCI 总线地址与 HPA 地址的映射关系，如果在 IOTLB 命中时，PCI 设备直接获得 HPA 地址进行 DMA 操作；如果没有在 IOTLB 命中，则需要使用图 13-4 中所示的算法进行 PCI 总线地址到 HPA 地址的转换。

Intel并没有公开“没有在IOTLB命中”的实现细节，当出现这种情况时，IA处理器可能使用内部的Microcode完成图13-4所示的算法。使用VT-d除了可以有效地支持虚拟化技术之外，还可以支持一些只能访问32位地址空间的PCI设备访问4GB之上的物理地址空间。

# 13.1.3 AMD处理器的IOMMU

AMD 处理器的 IOMMU 技术与 Intel 的 VT-d 技术类似，其完成的主要功能也类似。AMD

率先提出了IOMMU的概念，并发布了IOMMU的技术手册，但是Intel首先将这一技术在芯片中实现。由于AMD和Intel使用的x86体系结构略有不同，因此AMD的IOMMU技术在细节上与Intel的VT-d并不完全一致。

AMD 处理器使用 HT（Hyper Transport）总线连接 I/O Hub，其中每一个 I/O Hub 都含有一个 IOMMU，其结构如图 13-5 所示。

![[pci_express/1bf43a1ba05e75b2e8a01a0115f7327c41480d59ea5fac5ff6593ddca5c80f6b.jpg]]  
图13-5 AMD处理器的IOMMU结构

其中每一个IOMMU都使用一个Device Table。AMD处理器使用Device Table存放图13-3中的结构，Device Table最多由 $2^{16}$ 个Entry组成，其中每个Entry的大小为 $256\mathrm{b}$ ，因此Device Table最大将占用2MB内存空间，与Intel使用的Root/Context Entry结构相比，AMD使用的这种方法容易造成内存的浪费。

在I/O Hub中的设备使用16位的DeviceID在Device Table查找该设备所对应的Entry，并使用这个Entry，根据I/O Page Table结构最终找到IO PTE表，并完成GPA到HPA的转换。在AMD处理器中，GPA到HPA的转换与图13-4中所示的方法有类似之处，但实现细节不同。IOMMU使用一个新型的页表结构完成GPA到HPA的转换，这个页表结构基于AMD64使用的虚拟地址到物理地址的页表结构，但是做出了一些改动。AMD64进行虚拟地址到物理地址的转换时使用4级页表结构，如图13-6所示。

与Intel处理器的结构类似，一个进程首先从CR3寄存器中获得页表的基地址指针寄存器“Page Map Level-4 Base Address”，之后通过4级索引最终获得4KB大小的物理页面，完成虚拟地址到物理地址的转换。AMD处理器也支持大页面方式，如果使用三级索引，可以获得2MB大小的物理页面；使用二级索引，可以获得1GB大小的页面。

IOMMU 使用的 I/O 页表结构基于以上结构，但是做出了一定的改动。在 IOMMU 中，4 级 I/O 页表指针可以直接指向 2 级 I/O 页表指针，从而越过第 3 级 I/O 页表，使用这种方法可以节省 Page Table 的空间。如图 13-7 所示。

![[pci_express/6dc94a796e4b4feb6d0a38ff7b4cee5ea2fba47fc3a3522f82b8e8c3ab8f5cc1.jpg]]

图13-6 AMD64虚拟地址到物理地址的页表结构  
![[pci_express/4737f0b327bf0a31a5e8000532c8500ea98ab4b7de8bdb5762251a225b053161.jpg]]  
图13-7 IOMMU使用的GPA到HPA的转换机制

当设备进行 DMA 操作时，首先需要从相应的 Device Table 的 Entry 中获得“Level 4 Page Table Address”指针，并定位设备使用的 I/O 页表，最后使用多级页表结构，最终完成 PCI 总线地址到 HPA 地址的转换。Page Table 的 Entry 由 64 位组成，其主要字段如下所示。

- 第51\~12位为Next Table Address/Page Address字段，该字段存放下一级页表或者物理页面的地址，该地址为系统物理地址，属于HPA空间。  
- 第 $11\sim 9$ 位为NextLevel字段，表示下一级页表的级数，其中在Device Table中存放的级数一般为4，Level-N级页表中存放的NextLevel字段为N-1～1。

如图13-7所示，在第4级页表的Entry中的NextLevel字段为2，表示第4级页表直接指向第2级页表，而忽略第3级页表。当该字段为0b000或者0b111时，表示下一级指针指向物理页面而不是页表。NextLevel字段为0b000时，表示所指向的物理页面的大小是固定的，AMD64支持 $4\mathrm{KB}$ 、2MB、1GB、512GB和1TB（ $\mathrm{SP} = 4$ ）大小固定页面；如果Next

Level字段为0b111时，表示所指向的物理页面大小是浮动的。如果Level2PageTable的Entry中的Next Level字段为0b111，表示该Entry指向的物理页面大小浮动，其中物理页面大小和GPA的第 $29\sim 21$ 位相关，如表13-1所示。

表 13-1 Next Level 字段为 0b111 时的页表大小

<table><tr><td>29</td><td>28</td><td>27</td><td>26</td><td>25</td><td>24</td><td>23</td><td>22</td><td>21</td><td>Page Size</td><td>Default Page Size</td></tr><tr><td colspan="8">Page Address</td><td>0</td><td>4 MB</td><td>2 MB</td></tr><tr><td colspan="7">Page Address</td><td>0</td><td>1</td><td>8 MB</td><td>2 MB</td></tr><tr><td colspan="5">Page Address</td><td>0</td><td>1</td><td>1</td><td>16 MB</td><td>2 MB</td><td></td></tr><tr><td colspan="4">Page Address</td><td>0</td><td>1</td><td>1</td><td>1</td><td>32 MB</td><td>2 MB</td><td></td></tr><tr><td colspan="3">Page Address</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>64 MB</td><td>2 MB</td><td></td></tr><tr><td colspan="2">Page Address</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>128 MB</td><td>2 MB</td><td></td></tr><tr><td>...</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>256 MB</td><td>2 MB</td><td></td></tr><tr><td>...</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>512 MB</td><td>2 MB</td><td></td></tr></table>

AMD64处理器使用这种IO页表方式，可以方便地支持4KB、8KB、……、4GB大小的浮动物理页面。除了I/O页表外，IOMMU也设置了IOTLB以加快GPA到HPA地址的转换，这部分内容与IA处理器的实现方式类似，本章不对此继续进行描述。对IOMMU感兴趣的读者可以参考AMD I/O Virtualization Technology Specification。

# 13.2 ATS (Address Translation Services)

单纯使用IOMMU并不能充分发挥处理器系统的效率，从图13-2中可以发现，所有PCI设备在进行DMA操作时，都需要经过TA和ATPT进行地址翻译，然后才能访问主存储器。因而TA和ATPT很容易成为瓶颈，从而影响虚拟化系统的整体效率。

除此之外，在图13-2中，EP1和EP2分别隶属于Domain1和Domain2。在正常情况下，一个Domain并不能访问其他Domain的PCI设备。但是如果处理器系统中存在一个恶意的虚拟机，而且EP1隶属于该虚拟机（Domain1）。当EP1进行DMA写操作时，该虚拟机填写的DMA写地址可以与EP2的BAR地址空间重合，那么启动DMA写操作时，Domain1可以将数据传递到EP2，从而影响Domain2的正常运行。

解决这种异常的最合理的方法是，隶属于 Domain1 的 PCI 设备只能访问 GPA1 的空间，而仅使用 IOMMU 并不能解决该问题。解决该问题较为有效的方法是 PCI 设备进行数据传送的同时也进行地址转换，从而该 PCI 设备使用的地址是经过转换的 HPA 地址。此时再进行 DMA 写时，该数据将传递到与 Domain1 对应的 HPA 地址空间中，而不会将数据传送到 EP2。从而这个恶意的虚拟机并不会影响其他正常工作的虚拟机。

PCIe总线使用ATS机制实现PCIe设备的地址转换。支持ATS机制的PCIe设备，内部含有ATC（Address Translation Cache），ATC在PCIe设备中的位置如图13-8所示。

在ATC中存放ATPT的部分内容，当PCIe设备使用地址路由方式发送TLP时，其地址首先通过ATC转换为HPA地址。如果PCIe设备使用的地址没有在ATC中命中时，PCIe设备将通过存储器读TLP从ATPT中获得相应的地址转换信息，更新ATC后，再发送TLP。与

其他 Cache 类似，ATC 还可以被 Invalidate。当 ATPT 被更改时，处理器系统将发送 Invalidate 报文，同步在不同 PCIe 设备中的 ATC。

![[pci_express/e041e719862e173d57a8bf48ef4a789e6a6e48e8b670ee4b06755dde23ef72a3.jpg]]  
图13-8ATC在PCIe设备中的位置

PCIe总线在TLP中设置了AT字段以支持ATS机制。在PCIe总线中，只有与存储器相关的TLP支持AT字段。值得注意的是，只有处理器系统支持IOMMU时，PCIe设备才可以使用ATS机制。

# 13.2.1 TLP的AT字段

TLP的AT字段与ATS机制直接相关。根据AT字段的不同，PCIe设备可以发送三种类型的TLP。

# 1. AT字段为0b00

当AT字段为0b00时，当前TLP的Address字段没有通过ATC进行转换，存放的是PCI总线域的物理地址。如果PCIe设备不支持ATS机制，而且处理器系统也没有使能IOMMU时，当前TLP的Address字段为PCI总线域的物理地址。PCIe设备进行DMA操作时，该地址将被RC转换为存储器域的物理地址，然后对存储器进行读写操作。

如果PCIe设备不支持ATS机制，但是当前处理器支持IOMMU时，当前TLP的Address字段依然为PCI总线域的物理地址。PCIe设备进行DMA操作时，该地址将被TA根据I/O页表的设置，转换为合适的存储器域物理地址。

如果当前处理器系统支持虚拟化技术，当前PCIe设备将隶属于某一个Domain，此时该PCIe设备进行DMA操作时，数据将被传送到属于该Domain的存储器域中。

# 2. AT字段为0b01

当AT字段为0b01时，表示当前TLP报文为“Translation Request”报文。支持ATS机制的PCIe设备，必须支持这类报文。

该报文由PCIe设备通过存储器读请求TLP发出，其目的地为TA。TA收到该报文后，将根据I/O页表的设置，将合适的地址转换关系，通过存储器读完成TLP，发送给PCIe设备。而PCIe设备收到这个地址转换关系后，将更新ATC。

# 3. AT字段为 $0 \times 10$

当AT字段为0x01时，表示当前TLP的Address字段已经通过ATC进行地址转换。当PCIe设备使用存储器读写报文进行DMA操作，而RC收到这些报文时，将不再通过TA和ATPT进行地址转换，而直接将数据发送给存储器。从而减轻了ATPT进行地址转换的压力。

值得注意的是，经过ATC进行地址转换后，在TLP的Address字段中存放的依然是PCI总线域的物理地址，该物理地址为HPA地址在PCI总线域中的映像。

如果 TLP 中的 Address 字段没有经过 ATC 进行地址转换，而且处理器系统支持虚拟化技术，该地址为仍然对应 GPA 地址在 PCI 总线域中的映像，此时该 TLP 使用的 AT 字段为 0b00。这些地址在经过 RC 后，将被转换为存储器域的地址，然后进入 TA 和 ATPT 再次进行地址转换。由以上描述可以发现，PCIe 设备无论是否使用 ATC 机制，在 TLP 中存放的 Address 字段仍然保存的是 PCI 总线地址。

# 13.2.2 地址转换请求

PCIe设备可以使用地址转换请求（Translation Requests）TLP向TA提交地址转换请求。该TLP具有64位和32位两种地址格式。本节仅介绍64位地址格式，如图13-9所示。其中AT字段为0b01表示当前报文为地址转换请求TLP。

该报文的格式与存储器读请求 TLP 的报文格式基本类似，但是在地址转换请求 TLP 中，一些字段的含义与存储器读请求 TLP 并不相同。该报文的作用是将 Untranslated Address 字段发送到 TA，而 TA 根据 ATPT 将 Untranslated Address 数据区域进行翻译，然后通过存储器读完成 TLP 将地址转换关系发送给 PCIe 设备。PCIe 设备收到这个存储器读完成 TLP 后将这个地址转换关系保存在 PCIe 设备的 ATC 中。

Untranslated Address 数据区域的长度由 Length 字段确定。在地址转换请求 TLP 中，Length 字段的最低位和高 5 位为 0，而且 Length 字段不能为 0b00-0000-0000，因此该地址转换请求 TLP 所访问的数据区域最小为 8B，而最大不能超过 RCB。而且该数据区域为 1DW 对界，First DW BE 与 Last DW BE 字段都为 0b1111。

![[pci_express/c454e4942c591f708f98bd93bcbcb0932796f340c660148edb9a18813d9490c6.jpg]]  
图13-9 64位地址转换请求TLP的格式

当PCIe设备与某个虚拟机绑定时，Untranslated Address数据区域的GPA地址连续，但是其对应的HPA地址并不一定连续（在绝大多数虚拟机的实现中，为简化设计，GPA所对应的HPA地址区域地址连续）。因此PCIe设备发送一个地址转换请求后，可能会从TA得到

多个地址转换关系，这些地址转换关系可以使用一个存储器读完成 TLP 发送给 PCIe 设备。在 PCIe 总线中，一个地址转换关系由 8B 组成，这也是地址转换请求 TLP 的 Length 字段至少为 0b10 的原因。

当TA收到地址转换请求TLP后，将查找ATPT，然后通过存储器读完成TLP，将转换关系发送给PCIe设备。如果地址转换成功时，TA使用CplD（带数据的存储器读完成报文）将转换关系发送给PCIe设备；否则使用Cpl将失败信息发送给PCIe设备。本节仅讨论CplD报文，其格式如图13-10所示。

![[pci_express/e965b555c3c47cfe4116385b79c9971ba00c2682173a6df26903631ec65668f4.jpg]]

![[pci_express/47a6d78e1b034143b44a3bcac6e4580192725e4c658f1eaddeb0da6134901abd.jpg]]  
图13-10 地址转换完成TLP的格式

地址转换完成TLP的报文头与存储器读完成报文头完全相同，而Payload字段由一个或者多个地址转换关系组成。一个地址转换关系由以下字段与位组成。

- Translated Address [63:12] 字段保存与 Untranslated Address 对应的 HPA 地址，即经过转换的地址。  
- 而 U 位为 1 时，表示这段 HPA 空间只能使用 Untranslated 地址访问，即 PCIe 设备不能使用 AT 字段等于 0b10 的存储器读写 TLP；R 位为 1，表示 HPA 地址空间可读；W 位为 1 时，表示 HPA 地址空间可写。  
- N 位为 1 时, PCIe 设备访问这段数据区域时, 其存储器读写 TLP 的 No Snoop 位必须为 0 , 表示硬件需要进行 Cache 一致性操作; 如果该位为 0 , PCIe 设备将使用其他方法确定 No Snoop 位是否可以为 1 。  
- S 位需要与 Translated Address 字段联合使用，表示该段数据区域的大小，如表 13-2 所示。

由该表所示，Translated Address 数据区域的最小值为 4KB，此时 S 位必须为 0。如果 S 不为 0，则表示这段数据区域大于 4KB。当 Address31 为 0，而 Address [30:12] 和 S 位都为 1 时表示，这段区域为 4GB，但是 4GB 并不是 Translated Address 数据区域的最大值。PCIe

设置还可以使用 Address [63:32] 字段，继续扩展数据区域的大小。

表 13-2 Translated Address 区域的大小

<table><tr><td colspan="22">Tranlated Address</td><td rowspan="2">S</td><td>页面大小/B</td></tr><tr><td>63:32</td><td>31</td><td>30</td><td>29</td><td>28</td><td>27</td><td>26</td><td>25</td><td>24</td><td>23</td><td>22</td><td>21</td><td>20</td><td>19</td><td>18</td><td>17</td><td>16</td><td>15</td><td>14</td><td>13</td><td>12</td><td></td><td></td></tr><tr><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>0</td><td>4K</td><td></td></tr><tr><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>0</td><td>1</td><td>8K</td><td></td></tr><tr><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>0</td><td>1</td><td>16K</td><td></td></tr></table>

.

<table><tr><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>X</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>2M</td></tr></table>

··

<table><tr><td>X</td><td>X</td><td>X</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>2G</td></tr><tr><td>X</td><td>X</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>4G</td><td></td><td></td><td></td><td></td><td></td></tr><tr><td>X</td><td>0</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>1</td><td>4G</td><td></td><td></td><td></td><td></td><td></td></tr></table>

当PCIe设备支持ATS机制并进行DMA操作时，首先查找当前访问的地址是否在ATC中命中，如果命中则直接从ATC中获得TranslatedAddress，并使用AT等于0b10的存储器读写TLP与主存储器交换数据。TA收到AT等于0b10的TLP后，将不使用ATPT进行地址转换，而将报文直接发送到存储器控制器，与主存储器进行数据交换。

如果PCIe设备访问的地址区域没有在ATC中命中时，PCIe设备有两种处理方法，一是使用AT等于0b00的TLP，即使用Untranslated Address直接访问存储器，这个Untranslated Address到达TA后，TA根据ATPT的设置，将Untranslated Address进行地址转换，然后再将报文发送到存储器控制器中。

如果处理器系统中存在恶意的虚拟机时，PCIe设备使用这种方法时将会带来安全隐患。因为恶意的虚拟机可以直接将这个Untranslated Address与其他PCIe设备使用的BAR空间重合，从而该虚拟机可以破坏隶属于其他虚拟机的PCIe设备，干扰其他虚拟机的正常运行，这是虚拟机系统禁止的行为。

因而当PCIe设备访问的地址区间没有在ATC中命中时，应该首先进行地址转换。采用这种方式时，PCIe设备将首先向TA发送地址转换请求TLP，并从ATPT中获得地址转换关系后，使用TA等于0b10的存储器读写TLP，即使用Translated Address与主存储器进行数据交换，从而有效避免了上文所述的安全隐患。

目前尚无支持 ATS 机制的 PCIe 设备，但是通过本节的描述可以发现，使用 ATS 机制可以有效减轻 TA 进行地址转换的负担，同时避免虚拟机中存在的安全隐患。

