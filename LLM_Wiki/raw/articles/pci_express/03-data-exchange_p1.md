---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "03"
section: "第3章 PCI总线的数据交换"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第3章 PCI总线的数据交换

PCI Agent 设备之间以及 HOST 处理器和 PCI Agent 设备之间可以使用存储器读写和 I/O 读写等总线事务进行数据传送。在大多数情况下，PCI 桥不直接与 PCI 设备或者 HOST 主桥进行数据交换，而仅转发来自 PCI Agent 设备或者 HOST 主桥的数据。

PCI Agent 设备间的数据交换并不是本章讨论的重点。本章更侧重讲述 PCI Agent 设备使用 DMA 机制读写主存储器的数据，以及 HOST 处理器如何访问 PCI 设备的 BAR 空间。本章还将使用一定的篇幅讨论在 PCI 总线中与 Cache 相关的总线事务，并在最后介绍预读机制。

# 3.1 PCI设备BAR空间的初始化

在PCI Agent设备进行数据传送之前，系统软件需要初始化PCI Agent设备的BAR0\~5寄存器和PCI桥的Base、Limit寄存器。系统软件使用DFS算法对PCI总线进行遍历时，完成这些寄存器的初始化，即分配这些设备在PCI总线域的地址空间。当这些寄存器初始化完毕后，PCI设备可以使用PCI总线地址进行数据传递。

值得注意的是，PCI Agent 设备的 BAR0\~5 寄存器和 PCI 桥的 Base 寄存器保存的地址都是 PCI 总线地址。而这些地址在处理器系统的存储器域中具有映像，如果一个 PCI 设备的 BAR 空间在存储器域中没有映像，处理器将不能访问该 PCI 设备的 BAR 空间。

如上文所述，处理器通过HOST主桥将PCI总线域与存储器域隔离。当处理器访问PCI设备的地址空间时，需要首先访问该设备在存储器域中的地址空间，并通过HOST主桥将这个存储器域的地址空间转换为PCI总线域的地址空间之后，再使用PCI总线事务将数据发送到指定的PCI设备中。

PCI设备访问存储器域的地址空间，即进行DMA操作时，也是首先访问该存储器地址空间所对应的PCI总线地址空间，之后通过HOST主桥将这个PCI总线地址空间转换为存储器地址空间，再由DDR控制器对存储器进行读写访问。

不同的处理器系统采用不同的机制实现存储器域和PCI总线域的转换。如PowerPC处理器使用Outbound寄存器组实现存储器域到PCI总线域间的转换，并使用Inbound寄存器组实现PCI总线域到存储器域间的转换。

而x86处理器没有这种地址空间域的转换机制，因此从PCI设备的角度来看，PCI设备可以直接访问存储器地址；从处理器的角度来看，处理器可以直接访问PCI总线地址空间。但是读者需要注意，在x86处理器的HOST主桥中仍然有存储器域与PCI总线域这个概念。只是在x86处理器的HOST主桥中，存储器域的存储器地址与PCI总线地址相等，这种“简单相等”也是一种映射关系。

# 3.1.1 存储器地址与PCI总线地址的转换

下面根据PowerPC和x86处理器的主桥，抽象出一个虚拟的HOST主桥，并以此为例讲述PCI Agent设备之间以及PCI Agent设备与主存储器间的数据传送过程。

假设在一个32位处理器中，存储器域的0xF000-0000\~0xF7FF-FFFF（共128MB）这段物理地址空间与PCI总线的地址空间存在映射关系。

当处理器访问这段存储器地址空间时，HOST主桥会认领这个存储器访问，并将该存储器访问使用的物理地址空间转换为PCI总线地址空间，并与 $0\mathrm{x}7000 - 0000\sim 0\mathrm{x}77\mathrm{FF}$ -FFFF这段PCI总线地址空间对应。

为简化起见，假定在存储器域中只映射了PCI设备的存储器地址空间，而不映射PCI设备的I/O地址空间。而PCI设备的BAR空间使用 $0\mathrm{x}7000 - 0000\sim 0\mathrm{x}77\mathrm{FF}$ -FFFF这段PCI总线域的存储器地址空间。

在这个HOST主桥中，存储器域与PCI总线域的对应关系如图3-1所示。

![[pci_express/0a88553f3b52770a2cc8850e54110f5bed0422d8081c85a1a9d3a2d8bd8b5ebf.jpg]]  
图3-1 存储器域与PCI总线域的映射关系

当PCI设备使用DMA机制访问存储器域地址空间时，处理器系统同样需要将存储器域的地址空间反向映射到PCI总线地址空间。假设在一个处理器系统中，主存储器大小为2GB，其在存储器域的地址范围为 $0\mathrm{x}0000 - 0000\sim 0\mathrm{x}7\mathrm{FFF}$ -FFFF，而这段地址在PCI总线域中对应的“PCI总线地址空间”为 $0\mathrm{x}8000 - 0000\sim 0\mathrm{x}FFF - \mathrm{FFF}$

PCI设备进行DMA操作时，必须使用 $0\mathrm{x}8000 - 0000\sim 0\mathrm{x}FFF - \mathrm{FFF}$ 这段PCI总线域的地址，HOST主桥才能认领这个PCI总线事务，并将这个总线事务使用的PCI总线地址转换为存储器地址，并与 $0\mathrm{x}0000 - 0000\sim 0\mathrm{x}7\mathrm{FFF} - \mathrm{FFF}$ 这段存储器区域进行数据传递。

在一个实际的处理器系统中，很少有系统软件采用这样的方法，实现存储器域与PCI总线域之间的映射，“简单相等”还是最常用的映射方法。本章采用图3-1的映射关系，虽然增加了映射复杂度，却便于读者深入理解存储器域到PCI总线域之间的映射关系。下面将以这种映射关系为例，详细讲述PCI设备BAR0\~5寄存器的初始化。

# 3.1.2 PCI设备BAR寄存器和PCI桥Base、Limit寄存器的初始化

PCI桥的Base、Limit寄存器保存“该桥所管理的PCI子树”的存储器或者I/O空间的

基地址和长度。值得注意的是，PCI桥也是PCI总线上的一个设备，在其配置空间中也有BAR寄存器，本节不对PCI桥BAR寄存器进行说明，因为在多数情况下透明桥并不使用其内部的BAR寄存器。下面以图3-2所示的处理器系统为例说明上述寄存器的初始化过程，该处理器系统使用的存储器域与PCI总线域的映射关系如图3-1所示。

![[pci_express/36ca35a0cf0e78de4d4618a40a08628141e689d5d6ee407884a30b612612fe26.jpg]]  
图3-2 BAR寄存器的初始化

在PCI设备的BAR寄存器中，包含该设备使用的PCI总线域的地址范围。在PCI设备的配置空间中共有6个BAR寄存器，因此一个PCI设备最多可以使用6组32位的PCI总线地址空间，或者3组64位的PCI总线地址空间。这些BAR空间可以保存PCI总线域的存储器地址空间或者I/O地址空间，目前多数PCI设备仅使用存储器地址空间。而在通常情况下，一个PCI设备使用2到3个BAR寄存器就足够了。

为简化起见，首先假定在图3-2中所示的PCI总线树中，所有PCI Agent设备只使用了BAR0寄存器，其申请的数据空间大小为16MB（即 $0\mathrm{x}1000000\mathrm{B}$ ）而且不可预读，而且PCI桥不占用PCI总线地址空间，即PCI桥不含有BAR空间。并且假定当前HOST主桥已经完成了对PCI总线树的编号。

根据以上假设，该PCI总线树的遍历过程如下所示

（1）系统软件根据DFS算法，首先寻找到第一组PCI设备，分别为PCI设备31和PCI设备 $32^{\ominus}$ ，并根据这两个PCI设备需要的PCI空间大小，从PCI总线地址空间中（0x7000-

0000 \~ 0x77FF-FFFF）为这两个PCI设备的BAR0寄存器分配基地址，分别为0x7000-0000和0x7100-0000。

(2) 当系统软件完成 PCI 总线 3 下所有设备的 BAR 空间的分配后，将初始化 PCI 桥 3 的配置空间。这个桥片的 Memory Base 寄存器保存其下所有 PCI 设备使用的“PCI 总线域地址空间的基地址”，而 Memory Limit 寄存器保存其下 PCI 设备使用的“PCI 总线域地址空间的大小”。系统软件将 Memory Base 寄存器赋值为 $0 \times 7000 - 0000$ ，而将 Memory Limit 寄存器赋值为 $0 \times 200 - 0000$ 。  
(3) 系统软件回溯到 PCI 总线 2，并找到 PCI 总线 2 上的 PCI 设备 21，并将 PCI 设备 21 的 BAR0 寄存器赋值为 $0 \times 7200 - 0000$ 。  
(4) 完成 PCI 总线 2 的遍历后，系统软件初始化 PCI 桥 2 的配置寄存器，将 Memory Base 寄存器赋值为 0x7000-0000，Memory Limit 寄存器赋值为 0x300-0000。  
(5) 系统软件回溯到 PCI 总线 1, 并找到 PCI 设备 11, 并将这个设备的 BAR0 寄存器赋值为 $0 \times 7300 - 0000$ 。并将 PCI 桥 1 的 Memory Base 寄存器赋值为 $0 \times 7000 - 0000$ , Memory Limit 寄存器赋值为 $0 \times 400 - 0000$ 。  
(6) 系统软件回溯到 PCI 总线 0, 并在这条总线上发现另外一个 PCI 桥, 即 PCI 桥 4。并使用 DFS 算法继续遍历 PCI 桥 4。首先系统软件将遍历 PCI 总线 4, 并发现 PCI 设备 41 和 PCI 设备 42, 并将这两个 PCI 设备的 BAR0 寄存器分别赋值为 $0 \times 7400 - 0000$ 和 $0 \times 7500 - 0000$ 。  
(7) 系统软件初始化 PCI 桥 4 的配置寄存器, 将 Memory Base 寄存器赋值为 $0 \times 7400 - 0000$ , Memory Limit 寄存器赋值为 $0 \times 200 - 0000$ 。系统软件再次回到 PCI 总线 0 , 这一次系统软件没有发现新的 PCI 桥, 于是将初始化这条总线上的所有 PCI 设备。  
(8) PCI 总线 0 上只有一个 PCI 设备, 即 PCI 设备 01。系统软件将这个设备的 BAR0 寄存器赋值为 $0 \times 7600 - 0000$ , 并结束整个 DFS 遍历过程。

# 3.2 PCI设备的数据传递

PCI设备的数据传递使用地址译码方式，当一个存储器读写总线事务到达PCI总线时，在这条总线上的所有PCI设备将进行地址译码，如果当前总线事务使用的地址在某个PCI设备的BAR空间中时，该PCI设备将使能DEVSEL#信号，认领这个总线事务。

如果 PCI 总线上的所有设备都不能通过地址译码，认领这个总线事务时，这条总线的“负向译码”设备将认领这个总线事务，如果在这条 PCI 总线上没有“负向译码”设备，该总线事务的发起者将使用 Master Abort 总线周期结束当前 PCI 总线事务。

# 3.2.1 PCI设备的正向译码与负向译码

如上文所述，PCI设备使用“地址译码”方式接收存储器读写总线请求。在PCI总线中定义了两种“地址译码”方式，一种是正向译码，另一种是负向译码。

下面仍以图3-2所示的处理器系统为例，说明数据传送使用的寻址方法。当HOST主桥通过存储器或者I/O读写总线事务访问其下所有PCI设备时，PCI总线0下的所有PCI设备都将对出现在地址周期中的PCI总线地址进行译码。如果这个地址在某个PCI设备的BAR空间中命中时，这个PCI设备将接收这个PCI总线请求。这个过程也被称为PCI总线的正向

译码，这种方式也是大多数PCI设备所采用的译码方式。

但是在PCI总线上的某些设备，如PCI-to-(E)ISA桥并不使用正向译码接收来自PCI总线的请求，PCI-to-ISA桥在处理器系统中的位置如图1-1所示。PCI总线0上的总线事务在三个时钟周期后，没有得到任何PCI设备响应时（即总线请求的PCI总线地址不在这些设备的BAR空间中），PCI-to-ISA桥将被动地接收这个数据请求。这个过程被称为PCI总线的负向译码。可以进行负向译码的设备也被称为负向译码设备。

在PCI总线中，除了PCI-to-(E)ISA桥可以作为负向译码设备，PCI桥也可以作为负向译码设备，但是PCI桥并不是在任何时候都可以作为负向译码设备。在绝大多数情况下，PCI桥无论是处理“来自上游总线”，还是处理“来自下游总线”的总线事务时，都使用正向译码方式，但是在某些特殊应用中，PCI桥也可以作为负向译码设备。

笔记本在连接 Dock 插座时，也使用了 PCI 桥。因为在大多数情况下，笔记本与 Dock 插座是分离使用的，而且 Dock 插座上连接的设备多为慢速设备，此时用于连接 Dock 插座的 PCI 桥使用负向译码。Dock 插座在笔记本系统中的位置如图 3-3 所示。

![[pci_express/1c408d2e16fc1b6eb12d0bc4045c876e38ed42c2f59e7b5628aaaac2cdd14f81.jpg]]  
图3-3 Dock插座与笔记本的连接关系

当笔记本与 Dock 建立连接之后，如果处理器需要访问 Dock 中的外部设备时，Dock 中的 PCI 桥将首先使用负向译码方式接收 PCI 总线事务，之后将这个 PCI 总线事务转发到 Dock 的 PCI 总线中，再访问相应的 PCI 设备。

在 Dock 中使用负向译码 PCI 桥的优点是，该桥管理的设备并不参与处理器系统对 PCI 总线的枚举过程。当笔记本插入到 Dock 之后，系统软件并不需要重新枚举 Dock 中的设备并为这些设备分配系统资源，而仅需要使用负向译码 PCI 桥管理好其下的设备即可，从而极大降低了 Dock 对系统软件的影响。

当HOST处理器访问Dock中的设备时，负向译码PCI桥将首先接管这些存储器读写总线事务，然后发送到Dock设备中。值得注意的是，在许多笔记本的Dock实现中，并没有使用负向译码PCI桥，而使用PCI-to-ISA桥。

PCI总线规定使用负向译码的PCI桥，其Base Class Code寄存器为0x06，Sub Class Code寄存器为0x04，而Interface寄存器为0x01；使用正向译码方式的PCI桥的Interface寄存器为0x00。系统软件（ $\mathrm{E}^2\mathrm{PROM}$ ）在初始化Interface寄存器时务必注意这个细节。

综上所述，在PCI总线中有两种负向译码设备，PCI-to-E(ISA)桥和PCI桥。但PCI桥并非在任何时候都是负向译码设备，只有PCI桥连接Dock插座时，PCI桥的PrimaryBus才使用负向译码方式。而这个PCI桥的SecondaryBus在接收Dock设备的请求时仍然使用正向译码方式。

PCI桥使用的正向译码方式与PCI设备使用的正向译码方式有所不同。如图3-4所示，当一个总线事务是从PCI桥的PrimaryBus到SecondaryBus时，PCI桥使用的正向译码方式与PCI设备使用的方式类似。如果该总线事务使用的地址在PCI桥任意一个MemoryBase窗口命中时，该PCI桥将使用正向译码方式接收该总线事务，并根据实际情况决定是否将这个总线事务转发到SecondaryBus。

![[pci_express/894936e6cacf32955706a14b71db7133ef3c2a55bab2f52747b296ec433c497e.jpg]]  
图3-4 PCI桥使用的正向译码方式

当一个总线事务是从PCI桥的SecondaryBus到PrimaryBus时，如果该总线事务使用的地址没有在PCI桥所有的MemoryBase窗口命中，表明当前总线事务不是访问该PCI桥管理的PCI子树中的设备，因此PCI桥将接收当前总线事务，并根据实际情况决定是否将这个总线事务转发到PrimaryBus。

以图3-2为例，当PCI设备11访问主存储器空间时，首先将存储器读写总线事务发送到PCI总线1上，而这个存储器地址显然不会在PCI总线1的任何PCI设备的BAR空间中，此时PCI桥1将认领这个PCI总线的数据请求，并将这个总线事务转发到PCI总线0上。最后HOST主桥将接收这个总线事务，并将PCI总线地址转换为存储器域的地址，与主存储器进行读写操作。

值得注意的是，PCI总线并没有规定HOST主桥使用正向还是负向译码方式接收这个存储器读写总线事务，但是绝大多数HOST主桥使用正向译码方式接收来自下游的存储器读写总线事务。在PowerPC处理器中，如果当前存储器读写总线事务使用的地址在Inbound窗口内，HOST主桥将接收这个总线事务，并将其转换为存储器域的读写总线事务，与主存储器进行数据交换。

# 3.2.2 处理器到PCI设备的数据传送

下面以图3-2所示的处理器系统为例，说明处理器向PCI设备11进行存储器写的数据传送过程。处理器向PCI设备进行读过程与写过程略有区别，因为存储器写使用Posted方式，而存储器读使用Non-Posted方式，但是存储器读使用的地址译码方式与存储器写类似，因此本节对处理器向PCI设备进行存储器读的过程不做进一步介绍。

PCI设备11在PCI总线域的地址范围是 $0\mathrm{x}7300 - 0000\sim 0\mathrm{x}73\mathrm{FF}$ -FFFF。这段空间在存储器域中对应的地址范围是 $0\mathrm{xF300 - 0000}\sim 0\mathrm{xF3FF - FFFF}$ 。下面假设处理器使用存储器写指令，访问 $0\mathrm{xF300 - 0008}$ 这个存储器地址，其步骤如下。

(1) 存储器域将 0xF300-0008 这个地址发向 HOST 主桥, $0 \times \mathrm{F} 000 - 0000 \sim 0 \times \mathrm{F} 7 \mathrm{~F F} - \mathrm{F F F}$ 这

段地址已经由 HOST 主桥映射到 PCI 总线域地址空间，所以 HOST 主桥认为这是一个对 PCI 设备的访问。因此 HOST 主桥将首先接管这个存储器写请求。

(2）HOST主桥将存储器域的地址0xF300-0008转换为PCI总线域的地址0x7300-0008，并通过总线仲裁获得PCI总线0的使用权，启动PCI存储器写周期，并将这个存储器写总线事务发送到PCI总线0上。值得注意的是，这个存储器读写总线事务使用的地址为0x7300-0008，而不是0xF300-0008。  
(3) PCI 总线 0 的 PCI 桥 1 发现 $0 \times 7300 - 0008$ 在自己管理的地址范围内, 于是接管这个存储器写请求, 并通过总线仲裁逻辑获得 PCI 总线 1 的使用权, 并将这个请求转发到 PCI 总线 1 上。  
(4) PCI 总线 1 的 PCI 设备 11 发现 $0 \times 7300 - 0008$ 在自己的 BAR0 寄存器中命中, 于是接收这个 PCI 写请求, 并完成存储器写总线事务。

# 3.2.3 PCI设备的DMA操作

下面以图3-2所示的处理器系统为例，说明PCI设备11向存储器进行DMA写的数据传送过程。PCI设备的DMA写使用Posted方式而DMA读使用Non-Posted方式。本节不介绍PCI设备进行DMA读的过程，而将这部分内容留给读者分析。

假定PCI设备11需要将一组数据发送到 $0\mathrm{x}1000 - 0000\sim 0\mathrm{x}1000\text{-}\mathrm{FFF}$ 这段存储器域的地址空间中。由上文所述，存储器域的 $0\mathrm{x}0000 - 0000\sim 0\mathrm{x}7\mathrm{FFF}\text{-}\mathrm{FFF}$ 这段存储器空间与PCI总线域的 $0\mathrm{x}8000 - 0000\sim 0\mathrm{x}\mathrm{FFF}\text{-}\mathrm{FFF}$ 这段PCI总线地址空间对应。

PCI设备11并不能直接操作 $0\mathrm{x}1000 - 0000\sim 0\mathrm{x}1000\text{-}\mathrm{FFF}$ 这段存储器域的地址空间，PCI设备11需要对PCI总线域的地址空间 $0\mathrm{x}9000 - 0000\sim 0\mathrm{x}9000\text{-}\mathrm{FFF}$ 进行写操作，因为PCI总线地址空间 $0\mathrm{x}9000 - 0000\sim 0\mathrm{x}9000\text{-}\mathrm{FFF}$ 已经被HOST主桥映射到 $0\mathrm{x}1000 - 0000\sim 0\mathrm{x}1000\text{-}\mathrm{FFF}$ 这段存储器域。这个DMA写具体的操作流程如下。

(1) 首先 PCI 设备 11 通过总线仲裁逻辑获得 PCI 总线 1 的使用权, 之后将存储器写总线事务发送到 PCI 总线 1 上。值得注意的是, 这个存储器写总线事务的目的地址是 PCI 总线域的地址空间 $0 \times 9000 - 0000 \sim 0 \times 9000-FFFF$ , 这个地址是主存储器在 PCI 总线域的地址映像。  
(2) PCI 总线 1 上的设备将进行地址译码, 确定这个写请求是不是发送到自己的 BAR 空间, 在 PCI 总线 1 上的设备除了 PCI 设备 11 之外, 还有 PCI 桥 2 和 PCI 桥 1。  
(3) 首先 PCI 桥 1、2 和 PCI 设备 11 对这个地址同时进行正向译码。PCI 桥 1 发现这个 PCI 地址并不在自己管理的 PCI 总线地址范围之内，因为 PCI 桥片 1 所管理的 PCI 总线地址空间为 $0 \times 7000 - 0000 \sim 0 \times 73 \mathrm{~FF} - \mathrm{FFFF}$ 。此时 PCI 桥 1 将接收这个存储器写总线事务，因为 PCI 桥 1 所管理的 PCI 总线地址范围并不包含当前存储器写总线事务的地址，所以其下所有 PCI 设备都不可能接收这个存储器写总线事务。  
(4) PCI 桥 1 发现自己并不能处理当前这个存储器写总线事务, 则将这个存储器写总线事务转发到上游总线。PCI 桥 1 首先通过总线仲裁逻辑获得 PCI 总线 0 的使用权后, 然后将这个总线事务转发到 PCI 总线 0。  
(5) HOST 主桥发现 0x9000-0000 \~ 0x9000-FFFF 这段 PCI 总线地址空间与存储器域的存储器地址空间 0x1000-0000 \~ 0x1000-FFFF 对应, 于是将这段 PCI 总线地址空间转换成为存储器域的存储器地址空间, 并完成对这段存储器的写操作。

(6) 存储器控制器将从 HOST 主桥接收数据，并将其写入到主存储器。

PCI设备间的数据传递与PCI设备到存储器的数据传送大体类似。我们以PCI设备11将数据传递到PCI设备42为例说明这个转递过程。我们假定PCI设备11将一组数据发送到PCI设备42的PCI总线地址 $0\mathrm{x}7500 - 0000\sim 0\mathrm{x}7500\text{-}\mathrm{FFF}$ 这段地址空间中。这个过程与PCI设备11将数据发送到存储器的第 $1\sim 5$ 步基本类似，只是第5、6步不同。PCI设备11将数据发送到PCI设备42的第5、6步如下所示。

(5) PCI 总线 0 发现其下的设备 PCI 桥 4 能够处理来自 PCI 总线 0 的数据请求，则 PCI 桥 4 将接管这个 PCI 写请求，并通过总线仲裁逻辑获得 PCI 总线 4 的使用权，之后将这个存储器写请求发向 PCI 总线 4。此时 HOST 主桥不会接收当前存储器写总线事务，因为 $0 \times 7500 - 0000 \sim 0 \times 7500 - FFFF$ 这段地址空间并不是 HOST 主桥管理的地址范围。  
(6) PCI 总线 4 的 PCI 设备 42 将接收这个存储器写请求，并完成这个 PCI 存储器写请求总线事务。

PCI总线树内的数据传送始终都在PCI总线域中进行，不存在不同域之间的地址转换，因此PCI设备11向PCI设备42进行数据传递时，并不会进行PCI总线地址空间到存储器地址空间的转换。

# 3.2.4 PCI桥的Combining、Merging和Collapsing

由上所述，PCI设备间的数据传递有时将通过PCI桥。在某些情况下，PCI桥可以合并一些数据传递，以提高数据传递的效率。PCI桥可以采用Combining、Merging和Collapsing三种方式，优化数据通过PCI桥的效率。

# 1. Combining

PCI桥可以将接收到的多个存储器写总线事务合并为一个突发存储器写总线事务。PCI桥进行这种Combining操作时需要注意数据传送的“顺序”。当PCI桥接收到一组物理地址连续的存储器写访问时，如对PCI设备的某段空间的DW1、2和4进行存储器写访问时，PCI桥可以将这组访问转化为一个对DW1\~4的突发存储器写访问，并使用字节使能信号C/BE[3:0]#进行控制，其过程如下所示。

PCI桥将在数据周期1中，置C/BE[3:0]#信号为有效表示传递数据DW1；在数据周期2中，置C/BE[3:0]#信号为有效表示传递数据DW2；在数据周期3中，置C/BE[3:0]#信号为无效表示在这个周期中所传递的数据无效，从而跳过DW3；并在数据周期4中，置C/BE[3:0]#信号为有效表示传递数据DW4。

目标设备将最终按照发送端的顺序，接收DW1、DW2和DW4，采用这种方法在不改变传送序的前提下，提高了数据的传送效率。值得注意的是，有些HOST主桥也提供这种Combining方式，合并多次数据访问。如果目标设备不支持突发传送方式，该设备可以使用Disconnect周期，终止突发传送，此时PCI桥/HOST主桥可以使用多个存储器写总线事务分别传送DW1、DW2和DW4，而不会影响数据传送。

如果 PCI 桥收到“乱序”的存储器写访问，如对 PCI 设备的某段空间的 DW4、3 和 1 进行存储器写访问时，PCI 桥不能将这组访问转化为一个对 DW1 \~ 4 的突发存储器写访问，此时 PCI 桥必须使用三个存储器写总线事务转发这些存储器写访问。

# 2. Merge

PCI桥可以将收到的多个对同一个DW地址的Byte、Word进行的存储器写总线事务，合并为一个对这个DW地址的存储器写总线事务。PCI规范并没有要求这些对Byte、Word进行的存储器写在一个DW的边界之内，但是建议PCI桥仅处理这种情况。本节也仅介绍在这种情况下，PCI桥的处理过程。

PCI规范允许PCI桥进行Merge操作的存储器区域必须是可预读的，而可预读的存储器区域必须支持这种Merge操作。Merge操作可以不考虑访问顺序，可以将对Byte0、Byte1、Byte3的存储器访问合并为一个DW，也可以将对Byte3、Byte1、Byte0的存储器访问合并为一个DW。在这种情况下，PCI总线事务仅需屏蔽与Byte2相关的字节使能信号C/BE2#即可。

如果 PCI 设备对 Byte1 进行存储器写、然后再对 Byte1、Byte2、Byte3 进行存储器写时，PCI 桥不能将这两组存储器写合并为一次对 DW 进行存储器写操作。但是 PCI 桥可以合并后一组存储器写，即首先对 Byte1 进行存储器写，然后合并后一组存储器写（Byte1、Byte2 和 Byte3）为一个 DW 写，并屏蔽相应的 C/BE0#信号。Combining 与 Merge 操作之间没有直接联系，PCI 桥可以同时支持这两种方式，也可以支持任何一种方式。

# 3. Collapsing

Collapsing 指 PCI 桥可以将对同一个地址进行的 Byte、Word 和 DW 存储器写总线事务合并为一个存储器写操作。使用 PCI 桥的 Collapsing 方式是，具有某些条件限制，在多数情况下，PCI 桥不能使用 Collapsing 方式合并多个存储器写总线事务。

当 PCI 桥收到一个对 “DW 地址 X” 的 Byte3 进行的存储器写总线事务，之后又收到一个对 “DW 地址 X” 的 Byte、Word 或者 DW 存储器写总线事务，而且后一个对 DW 地址 X 进行的存储器写仍然包含 Byte3 时，如果 PCI 桥支持 Collapsing 方式，就可以将这两个存储器写合并为一个存储器写。

PCI桥在绝大多数情况下不能支持这种方式，因为很少有PCI设备支持这种数据合并方式。通常情况下，对PCI设备的同一地址的两次写操作代表不同的含义，因此PCI桥不能使用Collapsing方式将这两次写操作合并。PCI规范仅是提出了Collapsing方式的概念，几乎没有PCI桥支持这种数据合并方式。

# 3.3 与Cache相关的PCI总线事务

PCI总线规范定义了一系列与Cache相关的总线事务，以提高PCI设备与主存储器进行数据交换的效率，即DMA读写的效率。当PCI设备使用DMA方式向存储器进行读写操作时，一定需要经过HOST主桥，而HOST主桥通过FSB总线 $\text{一}$ 向存储器控制器进行读写操作时，需要进行Cache共享一致性操作。

PCI 设备与主存储器进行的 Cache 共享一致性增加了 HOST 主桥的设计复杂度。在高性能处理器中 Cache 状态机的转换模型十分复杂。而 HOST 主桥是 FSB 上的一个设备，需要按照 FSB 规定的协议处理这个 Cache 一致性，而多级 Cache 的一致性和状态转换模型一直是高

性能处理器设计中的难点。

不同的HOST主桥处理PCI设备进行的DMA操作时，使用的Cache一致性的方法并不相同。因为Cache一致性操作不仅与HOST主桥的设计相关，而且主要与处理器和CacheMemory系统设计密切相关。

PowerPC和x86处理器可以对PCI设备所访问的存储器进行设置，其设置方法并不相同。其中PowerPC处理器，如MPC8548处理器，可以使用Inbound寄存器的RTT字段和WTT字段，设置在PCI设备进行DMA操作时，是否需要进行Cache一致性操作，是否可以将数据直接写入Cache中。RTT字段和WTT字段的详细说明见第2.2.3节。

而 x86 处理器可以使用 MTRR（Memory Type Range Registers）设置物理存储器区间的属性是否为可 Cache 空间。下文分别讨论在 PowerPC 与 x86 处理器中，PCI 设备进行 DMA 写操作时，如何进行 Cache 一致性操作。

但是与PowerPC处理器相比，x86处理器在处理PCI设备的Cache一致性上略有不足，特别是网络设备与存储器系统进行数据交换的效率。因为x86处理器重点优化的是PCIe设备，目前x86处理器使用的IOAT（I/O Acceleration Technology）技术，显著提高了PCIe设备与主存储器进行数据通信的效率。

# 3.3.1 Cache 一致性的基本概念

PCI设备对可Cache的存储器空间进行DMA读写操作的过程较为复杂，有关CacheMemory的话题可以独立成书。而不同的处理器系统使用的CacheMemory的层次结构和访问机制有较大的差异，这部分内容也是现代处理器系统设计的重中之重。

本节仅介绍在 Cache Memory 系统中与 PCI 设备进行 DMA 操作相关的一些最为基础的概念。在多数处理器系统中，使用了以下概念描述 Cache 一致性的实现过程。

# 1. Cache一致性协议

多数SMP处理器系统使用了MESI协议处理多个处理器之间的Cache一致性。该协议也称为Illinois protocol，在SMP处理器系统中得到了广泛的应用。MESI协议使用四个状态位描述每一个Cache行。

- M（Modified）位。M 位为 1 时表示当前 Cache 行中包含的数据与存储器中的数据不一致，而且它仅在本 CPU 的 Cache 中有效，不在其他 CPU 的 Cache 中存在副本，在这个 Cache 行的数据是当前处理器系统中最新的数据副本。当 CPU 对这个 Cache 行进行替换操作时，必然会引发系统总线的写周期，将 Cache 行中数据与内存中的数据同步。  
- E（Exclusive）位。E位为1时表示当前Cache行中包含的数据有效，而且该数据仅在当前CPU的Cache中有效，而不在其他CPU的Cache中存在副本。在该Cache行中的数据是当前处理器系统中最新的数据副本，而且与存储器中的数据一致。  
- S（Shared）位。S 位为 1 表示 Cache 行中包含的数据有效，而且在当前 CPU 和至少其他一个 CPU 中具有副本。在该 Cache 行中的数据是当前处理器系统中最新的数据副本，而且与存储器中的数据一致。

\- I（Invalid）位。I位为1表示当前Cache行中没有有效数据或者该Cache行没有使能。MESI协议在进行Cache行替换时，将优先使用I位为1的Cache行。

MESI协议还存在一些变种，如MOESI协议和MESIF协议。基于MOESI协议的Cache一致性模型如图3-5所示。AMD处理器就使用MOESI协议。不同的处理器在实现MOESI协议时，状态机的转换原理类似，但是在处理上仍有较大的区别。

![[pci_express/69a787568413d9c928557bd3aabb6990bbe7b5e631bf400a12145f1c4f26164d.jpg]]  
图3-5 基于MOESI协议的Cache一致性模型

MOESI协议引入了一个O（Owned）状态，并在MESI协议的基础上，重新定义了S状态，而E、M和I状态和MESI协议的对应状态相同。

- O 位。O 位为 1 表示在当前 Cache 行中包含的数据是当前处理器系统最新的数据副本，而且在其他 CPU 中一定具有该 Cache 行的副本，其他 CPU 的 Cache 行状态为 S。如果主存储器的数据在多个 CPU 的 Cache 中都具有副本时，有且仅有一个 CPU 的 Cache 行状态为 O，其他 CPU 的 Cache 行状态只能为 S。与 MESI 协议中的 S 状态不同，状态为 O 的 Cache 行中的数据与存储器中的数据并不一致。  
- S 位。在 MOESI 协议中，S 状态的定义发生了细微的变化。当一个 Cache 行状态为 S 时，其包含的数据并不一定与存储器一致。如果在其他 CPU 的 Cache 中不存在状态为 O 的副本时，该 Cache 行中的数据与存储器一致；如果在其他 CPU 的 Cache 中存在状态为 O 的副本时，Cache 行中的数据与存储器不一致。

在一个处理器系统中，主设备（CPU或者外部设备）进行存储器访问时，将试图从存储器系统（主存储器或者其他CPU的Cache）中获得最新的数据副本。如果该主设备访问的数据没有在本地命中时，将从其他CPU的Cache中获取数据，如果这些数据仍然没有在其他CPU的Cache中命中，主存储器将提供数据。外设设备进行存储器访问时，也需要进行Cache共享一致性。

在MOESI模型中，“Probe Read”表示主设备从其他CPU中获取数据副本的目的是为了读取数据；而“Probe Write”表示主设备从其他CPU中获取数据副本的目的是为了写入数据；“Read Hit”和“Write Hit”表示主设备在本地Cache中获得数据副本；“Read Miss”和

“Write Miss”表示主设备没有在本地Cache中获得数据副本；“Probe Read Hit”和“Probe Write Hit”表示主设备在其他CPU的Cache中获得数据副本。

本节为简便起见，仅介绍CPU进行存储器写和与O状态相关的Cache行状态迁移，CPU进行存储器读的情况相对较为简单，请读者自行分析这个过程。

当CPU对一段存储器进行写操作时，如果这些数据在本地Cache中命中时，其状态可能为E、S、M或者O。

- 状态为 E 或者 M 时，数据将直接写入到 Cache 中，并将状态改为 M。  
- 状态为 S 时，数据将直接写入到 Cache 中，并将状态改为 M，同时其他 CPU 保存该数据副本的 Cache 行状态将从 S 或者 O 迁移到 I（Probe Write Hit）。  
- 状态为 O 时, 数据将直接写入到 Cache 中, 并将状态改为 M, 同时其他 CPU 保存该数据副本的 Cache 行状态将从 S 迁移到 I (Probe Write Hit)。

当CPU A对一段存储器进行写操作时，如果这些数据没有在本地Cache中命中时，而在其他CPU，如CPU B的Cache中命中时，其状态可能为E、S、M或者O。其中CPU A使用CPU B在同一个Cache共享域中。

- Cache 行状态为 E 时，CPU B 将该 Cache 行状态改为 I；而 CPU A 将从本地申请一个新的 Cache 行，将数据写入，并该 Cache 行状态更新为 M。  
- Cache 行状态为 S 时，CPU B 将该 Cache 行状态改为 I，而且具有同样副本的其他 CPU 的 Cache 行也需要将状态改为 I；而 CPU A 将从本地申请一个 Cache 行，将数据写入，并该 Cache 行状态更新为 M。  
- Cache 行状态为 M 时, CPU B 将原 Cache 行中的数据回写到主存储器, 并将该 Cache 行状态改为 I; 而 CPU A 将从本地申请一个 Cache 行, 将数据写入, 并该 Cache 行状态更新为 M。  
- Cache 行状态为 O 时, CPU B 将原 Cache 行中的数据回写到主存储器, 并将该 Cache 行状态改为 I, 具有同样数据副本的其他 CPU 的 Cache 行也需要将状态从 S 更改为 I; CPU A 将从本地申请一个 Cache 行, 将数据写入, 并该 Cache 行状态更新为 M。

Cache 行状态可以从 M 迁移到 O。例如当 CPU A 读取的数据从 CPU B 中命中时，如果在 CPU B 中 Cache 行的状态为 M 时，将迁移到 O，同时 CPU B 将数据传送给 CPU A 新申请的 Cache 行中，而且 CPU A 的 Cache 行状态将被更改为 S。

当CPU读取的数据在本地Cache中命中，而且Cache行状态为O时，数据将从本地Cache获得，并不会改变Cache行状态。如果CPU A读取的数据在其他Cache中命中，如在CPU B的Cache中命中而且其状态为O时，CPU B将该Cache行状态保持为O，同时CPU B将数据传送给CPU A新申请的Cache行中，而且CPU A的Cache行状态将被更改为S。

在某些应用场合，使用MOESI协议将极大提高Cache的利用率，因为该协议引入了O状态，从而在发送Read Hit的情况时，不必将状态为M的Cache回写到主存储器，而是直接从一个CPU的Cache将数据传递到另外一个CPU。目前MOESI协议在AMD和RMI公司的处理器中得到了广泛的应用。

Intel 提出了另外一种 MESI 协议的变种，即 MESIF 协议，该协议与 MOESI 协议有较大的不同，也远比 MOESI 协议复杂，该协议由 Intel 的 QPI（QuickPath Interconnect）技术引入，其主要目的是解决“基于点到点的全互连处理器系统”的 Cache 共享一致性问题，而

不是“基于共享总线的处理器系统”的Cache共享一致性问题。

在基于点到点互连的NUMA（Non-Uniform Memory Architecture）处理器系统中，包含多个子处理器系统，这些子处理器系统由多个CPU组成。如果这个处理器系统需要进行全机Cache共享一致性，该处理器系统也被称为ccNUMA（CacheCohenrentNUMA）处理器系统。MESIF协议主要解决ccNUMA处理器结构的Cache共享一致性问题，这种结构通常使用目录表，而不使用总线监听处理Cache的共享一致性。

MESIF协议引入了一个F（Forward）状态。在ccNUMA处理器系统中，可能在多个处理器的Cache中存在相同的数据副本，在这些数据副本中，只有一个Cache行的状态为F，其他Cache行的状态都为S。Cache行的状态位为F时，Cache中的数据与存储器一致。

当一个数据请求方读取这个数据副本时，只有状态为F的Cache行，可以将数据副本转发给数据请求方，而状态位为S的Cache不能转发数据副本。从而MESIF协议有效解决了在ccNUMA处理器结构中，所有状态位为S的Cache同时转发数据副本给数据请求方，而造成的数据拥塞。

在ccNUMA处理器系统中，如果状态位为F的数据副本，被其他CPU复制时，F状态位将会被迁移，新建的数据副本的状态位将为F，而老的数据副本的状态位将改变为S。当状态位为F的Cache行被改写后，ccNUMA处理器系统需要首先 Invalidate状态位为S其他的Cache行，之后将Cache行的状态更新为M。

独立地研究MESIF协议并没有太大意义，该协议由Boxboro-EX处理器系统引入，目前Intel并没有公开Boxboro-EX处理器系统的详细设计文档。MESIF协议仅是解决该处理器系统中Cache一致性的一个功能，该功能的详细实现与QPI的Protocal Layer相关，QPI由多个层次组成，而Protocal Layer是QPI的最高层。

对MESIF协议QPI互连技术有兴趣的读者，可以在深入理解“基于目录表的Cache一致性协议”的基础上，阅读Robert A. Maddox，Gurbir Singh and Robert J. Safranek合著的书籍“Weaving High Performance Multiprocessor Fabric”以了解该协议的实现过程和与QPI互连技术相关的背景知识。

值得注意的是，MESIF协议解决主要的问题是ccNUMA架构中SMP子系统与SMP子系统之间Cache一致性。而在SMP处理器系统中，依然需要使用传统的MESI协议。NehalemEX处理器也可以使用MOESI协议进一步优化SMP系统使用的Cache一致性协议，但是并没有使用该协议。

为简化起见，本章假设处理器系统使用MESI协议进行Cache共享一致性，而不是MOESI协议或者MESIF协议。

# 2. HIT#和HITM#信号

在SMP处理器系统中，每一个CPU都使用HIT#和HITM#信号反映HOST主桥访问的地址是否在各自的Cache中命中。当HOST主桥访问存储器时，CPU将驱动HITM#和HIT#信号，其描述如表3-1所示。

表 3-1 HIM#和 HIT#信号的含义

<table><tr><td>HITM#</td><td>HIT#</td><td>描述</td></tr><tr><td>1</td><td>1</td><td>表示HOST主桥访问的地址没有在CPU的Cache中命中</td></tr><tr><td>1</td><td>0</td><td>表示HOST主桥访问的地址在CPU的Cache中命中，而且Cache的状态为S(Shared)或者E(Exclusive)，即Cache中的数据与存储器的数据一致</td></tr><tr><td>0</td><td>1</td><td>表示HOST主桥访问的地址在CPU的Cache中命中，而且Cache的状态为M(Modified)，即Cache中的数据与存储器的数据不一致，在Cache中保存最新的数据副本</td></tr><tr><td>0</td><td>0</td><td>MESI协议规定这种情况不允许出现，但是在有些处理器系统中仍然使用了这种状态，表示暂时没有获得是否在Cache中命中的信息，需要等待几拍后重试</td></tr></table>

HIT#和HITM#信号是FSB中非常重要的两个信号，各个CPU的HIT#和HITM#信号通过“线与”方式直接相连。而在一个实际FSB中，还包括许多信号，本节并不会详细介绍这些信号。

# 3. Cache一致性协议中使用的Agent

在处理器系统中，与Cache一致性相关的Agent如下所示。

- Request Agent。FSB总线事务的发起设备。在本节中，Request Agent特指HOST主桥。实际上在FSB总线上的其他设备也可以成为Request Agent，但这些Request Agent并不是本节的研究重点。Request Agent需要进行总线仲裁后，才能使用FSB，在多数处理器的FSB中，需要对地址总线与数据总线分别进行仲裁。  
- Snoop Agents。FSB 总线事务的监听设备。Snoop Agents 为 CPU，在一个 SMP 处理器系统中，有多个 CPU 共享同一个 FSB，此时这些 CPU 都是这条 FSB 上的 Snoop Agents。Snoop Agents 监听 FSB 上的存储器读写事务，并判断这些总线事务访问的地址是否在 Cache 中命中。Snoop Agents 通过 HIT# 和 HIM# 信号向 FSB 通知 Cache 命中的结果。在某些情况下，Snoop Agents 需要将 Cache 中的数据回写到存储器，同时为 Request Agent 提供数据。  
- Response Agent。FSB 总线事务的目标设备。在本节中，Response Agent 特指存储器控制器。Response Agent 根据 Snoop Agents 提供的监听结果，决定如何接收数据或者向 Request Agent 设备提供数据。在多数情况下，当前数据访问没有在 Snoop Agents 中命中时，Response Agent 需要提供数据，此外 Snoop Agents 有时需要将数据回写到 Response Agent 中。

# 4. FSB的总线事务

一个FSB的总线事务由多个阶段组成，包括Request Phase、Snoop Phase、Response Phase和Data Phase。目前在多数高端处理器中，FSB支持流水操作，即在同一个时间段内，不同的阶段可以重叠，如图3-6所示。

在一个实际的FSB中，一个总线事务还可能包含Arbitration Phase和Error Phase。而本节仅讲述图3-6中所示的4个基本阶段。

\- Request Phase。Request Agent 在获得 FSB 的地址总线的使用权后，在该阶段将访问数据区域的地址和总线事务类型发送到 FSB 上。

![[pci_express/a824343d054943c0b7c930e05caae3719df9d3c13b52118b89d88a396e784388.jpg]]  
图3-6 FSB的流水操作

- Snoop Phase。Snoop Agents 根据访问数据区域在 Cache 中的命中情况，使用 HIT# 和 HITM# 信号，向其他 Agents 通知 Cache 一致性的结果。有时 Snoop Agent 需要将数据回写到存储器。  
- Reponse Phase。Response Agent 根据 Request 和 Snoop Phase 提供的信号，可以要求 Request Agent 重试（Retry），或者 Response Agent 延时处理（Defer）当前总线事务。在 FSB 总线事务的各个阶段中，该步骤的处理过程最为复杂。本章将在下文结合 PCI 设备的 DMA 读写执行过程，说明该阶段的实现原理。  
- Data Phase。一些不传递数据的FSB总线事务不包含该阶段。该阶段用来进行数据传递，包括Request Agent向Response Agent写入数据；Response Agent为Request Agent提供数据；和Snoop Agent将数据回写到Response Agent。

下面将使用本小节中的概念，描述在PCI总线中，与Cache相关的总线事务，并讲述相关的FSB的操作流程。

