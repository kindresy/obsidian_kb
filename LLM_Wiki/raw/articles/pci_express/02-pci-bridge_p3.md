---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "02"
section: "（4）I/O Limit和I/O Base寄存器"
part: 3
tags: [pci, pci-express, computer-architecture]
---
# （4）I/O Limit和I/O Base寄存器

在PCI桥管理的PCI子树中包含许多PCI设备，而这些PCI设备可能会使用I/O地址空间。PCI桥使用这两个寄存器，存放PCI子树中所有设备使用的I/O地址空间集合的基地址和大小。

# (5）Memory Limit和Memory Base寄存器

在PCI桥管理的PCI子树中有许多PCI设备，这些PCI设备可能会使用存储器地址空间。这两个寄存器存放所有这些PCI设备使用的存储器地址空间集合的基地址和大小，PCI桥规定这个空间的大小至少为1MB。

# (6) Prefetchable Memory Limit 和 Prefetchable Memory Base 寄存器

在PCI桥管理的PCI子树中有许多PCI设备，如果这些PCI设备支持预读，则需要从PCI桥的可预读空间中获取地址空间。PCI桥的这两个寄存器存放这些PCI设备使用的可预取存储器空间的基地址和大小。

如果 PCI 桥不支持预读，则其下支持预读的 PCI 设备需要从 Memory Base 寄存器为基地址的存储器空间中获取地址空间。如果 PCI 桥支持预读，其下的 PCI 设备需要根据情况，决定使用可预读空间还是不可预读空间。PCI 总线建议 PCI 设备支持预读，但是支持预读的 PCI 设备并不多见。

# (7) I/O Base Upper 16 Bits and I/O Limit Upper 16 寄存器

如果 PCI 桥仅支持 16 位的 I/O 端口，这组寄存器只读，且其值为 0。如果 PCI 桥支持 32 位 I/O 端口，这组寄存器可以提供 I/O 端口的高 16 位地址。

# (8) Bridge Control Register

该寄存器用来管理PCI桥的SecondaryBus，其主要位的描述如下。

\- Secondary Bus Reset 位，第 6 位，可读写。当该位为 1 时，将使用下游总线提供的 RST#信号复位与 PCI 桥的下游总线连接的 PCI 设备。通常情况下与 PCI 桥下游总线连接的 PCI 设备，其复位信号需要与 PCI 桥提供的 RST#信号连接，而不能与 HOST 主桥提供的 RST#信号连接。

\- Primary Discard Timer 位，第 8 位，可读写。PCI 桥支持 Delayed 传送方式，当 PCI 桥的 Primary 总线上的主设备使用 Delayed 方式进行数据传递时，PCI 桥使用 Retry 周期结束 Primary 总线的 Non-Posted 数据请求，并将这个 Non-Posted 数据请求转换为 Delayed 数据请求，之后主设备需要择时重试相同的 Non-Posted 数据请求。当该位为 1

时，表示在 Primary Bus 上的主设备需要在 $2^{10}$ 个时钟周期之内重试这个数据请求，为 0 时，表示主设备需要在 $2^{15}$ 个时钟周期之内重试这个数据请求，否则 PCI 桥将丢弃 Delayed 数据请求。

\- Secondary Discard Timer 位，第 9 位，可读写。当该位为 1 时，表示在 Secondary Bus 上的主设备需要在 $2^{10}$ 个时钟周期之内重试这个数据请求，为 0 时，表示主设备需要在 $2^{15}$ 个时钟周期之内重试这个数据请求，如果主设备在规定的时间内没有进行重试时，PCI 桥将丢弃 Delayed 数据请求。

# 2.4 PCI总线的配置

PCI总线定义了两类配置请求，一类是Type00h配置请求，另一类是Type01h配置请求。PCI总线使用这些配置请求访问PCI总线树上的设备配置空间，包括PCI桥和PCI Agent设备的配置空间。

其中HOST主桥或者PCI桥使用Type00h配置请求，访问与HOST主桥或者PCI桥直接相连的PCI Agent设备或者PCI桥；而HOST主桥或者PCI桥使用Type01h配置请求，需要至少穿越一个PCI桥，访问没有与其直接相连的PCI Agent设备或者PCI桥。如图2-8所示，HOST主桥可以使用Type00h配置请求访问PCI设备01，而使用Type01h配置请求通过PCI桥1、2或者3转换为Type00h配置请求之后，访问PCI总线树上的PCI设备11、21、22、31和 $32^{\ominus}$ 。

当x86处理器对CONFIG\_DATA寄存器进行读写操作时，HOST主桥将决定向PCI总线发送Type00h配置请求还是Type01h配置请求。在PCI总线事务的地址周期中，这两种配置请求总线事务的不同反映在PCI总线的AD[31:0]信号线上。

值得注意的是，PCIe总线还可以使用ECAM（Enhanced Configuration Access Mechanism）机制访问PCIe设备的扩展配置空间，使用这种方式可以访问PCIe设备 $256\mathrm{B}\sim 4\mathrm{KB}$ 之间的扩展配置空间。但是本节仅介绍如何使用CONFIG\_ADDRESS和CONFIG\_FATA寄存器产生Type00h和Type01h配置请求。有关ECAM机制的详细说明见第5.3.2节。

处理器首先将目标 PCI 设备的 ID 号保存在 CONFIG\_ADDRESS 寄存器中，之后 HOST 主桥根据该寄存器的 Bus Number 字段，决定是产生 Type 00h 配置请求，还是 Type 01h 配置请求。当 Bus Number 字段为 0 时，将产生 Type 00h 配置请求，因为与 HOST 主桥直接相连的总线号为 0；大于 0 时，将产生 Type 01h 配置请求。

# 2.4.1 Type 01h 和 Type 00h 配置请求

本节首先介绍Type01h配置请求，并从PCI总线使用的信号线的角度上，讲述HOST主桥如何生成Type01配置请求。在PCI总线中，只有PCI桥能够接收Type01h配置请求。Type01h配置请求不能直接发向最终的PCI Agent设备，而只能由PCI桥将其转换为Type01h继续发向其他PCI桥，或者转换为Type00h配置请求发向PCI Agent设备。PCI桥还可

以将 Type 01h 配置请求转换为 Special Cycle 总线事务（HOST 主桥也可以实现该功能），本节对这种情况不做介绍。

在地址周期中，HOST主桥使用配置读写总线事务，将CONFIG\_ADDRESS寄存器的内容复制到PCI总线的AD[31:0]信号线中。CONFIG\_ADDRESS寄存器与Type01h配置请求的对应关系如图2-11所示。

![[pci_express/a5e4545b312f0bc88e45dffb2189818e437747b9d51b6b2674da37acbd6c30c6.jpg]]  
图2-11 CONFIG\_ADDRESS寄存器与Type01h配置请求的对应关系

从图2-11中可以发现，CONFIG\_ADDRESS寄存器的内容基本上是原封不动地复制到PCI总线的AD[31:0]信号线上的。其中CONFIG\_ADDRESS的Enable位不被复制，而AD总线的第0位必须为1，表示当前配置请求是Type01h。

当PCI总线接收到Type01配置请求时，将寻找合适的PCI桥接收这个配置信息。如果这个配置请求是直接发向PCI桥下的PCI设备时，PCI桥将接收这个Type01配置请求，并将其转换为Type00h配置请求；否则PCI桥将当前Type01h配置请求原封不动地传递给下一级PCI总线。

如果HOST主桥或者PCI桥发起的是Type00h配置请求，CONFIG\_ADDRESS寄存器与AD[31:0]的转换如图2-12所示。

![[pci_express/bcfea4ea185456277d4c278ea8727b7036fa8d5558798060dfa77b79128c5b6a.jpg]]  
图2-12 CONFIG\_ADDRESS寄存器与Type00h配置请求的对应关系

此时处理器对CONFIG\_DATA寄存器进行读写时，处理器将CONFIG\_ADDRESS寄存器中的Function Number和Register Number字段复制到PCI的AD总线的第 $10\sim 2$ 位；将AD总线的第 $1\sim 0$ 位赋值为0b00。PCI总线在配置请求总线事务的地址周期根据AD[1:0]判断当前配置请求是Type00h还是Type01h，如果AD[1:0]等于0b00表示是Type00h配置请求，如果AD[1:0]等于0b01表示是Type01h配置请求。

而AD[31:11]与CONFIG\_ADDRESS的Device Number字段有关，在Type00h配置请

求的地址周期中，AD[31:11]位有且只有一位为1，其中AD[31:11]的每一位选通一个PCI设备的配置空间。如第1.2.2节所述，PCI设备配置空间的片选信号是IDSEL，因此AD[31:11]将与PCI设备的IDSEL信号对应相连。

当以下两种请求之一满足时，HOST主桥或者PCI桥将生成Type00h配置头，并将其发送到指定的PCI总线上。

(1) CONFIG\_ADDRESS 寄存器的 Bus Number 字段为 0 时，处理器访问 CONFIG\_DATA 寄存器时，HOST 主桥将直接向 PCI 总线 0 发出 Type 00h 配置请求。因为与 HOST 主桥直接相连的 PCI 总线号为 0，此时表示 HOST 主桥需要访问与其直接相连的 PCI 设备。  
(2) 当 PCI 桥收到 Type 01h 配置头时，将检查 Type 01 配置头的 Bus Number 字段，如果这个 Bus Number 与 PCI 桥的 Secondary Bus Number 相同，则将这个 Type 01 配置头转换为 Type 00h 配置头，并发送到该 PCI 桥的 Secondary 总线上。

# 2.4.2 PCI总线配置请求的转换原则

当CONFIG\_ADDRESS寄存器的Enable位为1，系统软件访问CONFIG\_DATA寄存器时，HOST主桥可以产生两类PCI总线配置读写总线事务，分别为Type00h和Type01h配置读写总线事务。在配置读写总线事务的地址周期和数据周期中，CONFIG\_ADDRESS和CONFIG\_DATA寄存器中的数据将被放置到PCI总线的AD总线上。其中Type00h和Type01h配置读写总线事务映射到AD总线的数据并不相同。

其中Type00h配置请求可以直接读取PCI Agent设备的配置空间，而Type01h配置请求在通过PCI桥时，最终将被转换为Type00h配置请求，并读取PCI Agent设备的配置寄存器。本节重点讲述PCI桥如何将Type01h配置请求转换为Type00h配置请求。

首先 Type 00h 配置请求不会被转换成 Type 01h 配置请求，因为 Type 00h 配置请求是发向最终 PCI Agent 设备，这些 PCI Agent 设备不会转发这些配置请求。

当CONFIG\_ADDRESS寄存器的Bus Number字段为0时，处理器对CONFIG\_DATA寄存器操作时，HOST主桥将直接产生Type00h配置请求，挂接在PCI总线0上的某个设备将通过ID译码接收这个Type00h配置请求，并对配置寄存器进行读写操作。如果PCI总线上没有设备接收这个Type00h配置请求，将引发MasterAbort，详情见PCI总线规范，本节对此不做进一步说明。

如果 CONFIG\_ADDRESS 寄存器的 Bus Number 字段为 $n$ ( $n \neq 0$ )，即访问的 PCI 设备不是直接挂接在 PCI 总线 0 上的，此时 HOST 主桥对 CONFIG\_DATA 寄存器操作时，将产生 Type 01h 配置请求，PCI 总线 0 将遍历所有在这条总线上的 PCI 桥，确定由哪个 PCI 桥接收这个 Type 01h 配置请求。

如果 $n$ 大于或等于某个 PCI 桥的 Secondary Bus Number 寄存器，而且小于或等于 Subordinate Bus number 寄存器，那么这个 PCI 桥将接收在当前 PCI 总线上的 Type 01 配置请求，并采用以下规则进行递归处理。

（1）开始。  
(2）遍历当前PCI总线的所有PCI桥。  
(3) 如果 $n$ 等于某个 PCI 桥的 Secondary Bus Number 寄存器，说明这个 Type 01 配置请求的目标设备直接连接在该 PCI 桥的 Secondary bus 上。此时 PCI 桥将 Type 01 配置请求转

换为Type00h配置请求，并将这个配置请求发送到PCI桥的SecondaryBus上，SecondaryBus上的某个设备将响应这个Type00h配置请求，并与HOST主桥进行配置信息的交换，转（5）。

(4) 如果 $n$ 大于 PCI 桥的 Secondary Bus Number 寄存器，而且小于或等于 PCI 桥的 Subordinate Bus number 寄存器，说明这个 Type 01 配置请求的目标设备不与该 PCI 桥的 Secondary Bus 直接相连，但是由这个 PCI 桥下游总线上的某个 PCI 桥管理。此时 PCI 桥将首先认领这个 Type 01 配置请求，并将其转发到 Secondary Bus，转（2）。

# (5) 结束。

下面将举例说明PCI总线配置请求的转换原则，并以图2-8为例说明处理器如何访问PCI设备01和PCI设备31的配置空间。PCI设备01直接与HOST主桥相连，因此HOST主桥可以使用Type00h配置请求访问该设备。

而HOST主桥需要经过多级PCI桥才能访问PCI设备31，因此HOST主桥需要首先使用Type01h配置请求，之后通过PCI桥1、2和3将Type01h配置请求转换为Type00h配置请求，最终访问PCI设备31。

# 1. PCI设备01

这种情况较易处理，当HOST处理器访问PCI设备01的配置空间时，发现PCI设备01与HOST主桥直接相连，所以将直接使用Type00h配置请求访问该设备的配置空间，具体步骤如下。

首先HOST处理器将CONFIG\_ADDRESS寄存器的Enabled位置1，Bus Number号置为0，并对该寄存器的Device、Function和Register Number字段赋值。当处理器对CONFIG\_DATA寄存器访问时，HOST主桥将存放在CONFIG\_ADDRESS寄存器中的数值，转换为Type00h配置请求，并发送到PCI总线0上，PCI设备01将接收这个Type00h配置请求，并与处理器进行配置信息交换。

# 2. PCI设备31

HOST处理器对PCI设备31进行配置读写时，需要通过HOST主桥、PCI桥1、2和3，最终到达PCI设备31。

当处理器访问 PCI 设备 31 时，首先将 CONFIG\_ADDRESS 寄存器的 Enabled 位置 1，Bus Number 字段置为 3，并对 Device、Function 和 Register Number 字段赋值。之后当处理器对 CONFIG\_DATA 寄存器进行读写访问时，HOST 主桥、PCI 桥 1、2 和 3 将按照以下步骤进行处理，最后 PCI 设备 31 将接收这个配置请求。

（1）HOST主桥发现Bus Number字段的值为3，该总线号并不是与HOST主桥直接相连的PCI总线的Bus Number，所以HOST主桥将处理器对CONFIG\_DATA寄存器的读写访问直接转换为Type01h配置请求，并将这个配置请求发送到PCI总线0上。PCI总线规定Type01h配置请求只能由PCI桥负责处理。  
(2) 在 PCI 总线 0 上, PCI 桥 1 的 Secondary Bus Number 为 1 而 Subordinate Bus Number 为 3。而 $1 < \text{Bus Number} \leqslant 3$ , 所以 PCI 桥 1 将接收来自 PCI 总线 0 的 Type 01h 配置请求, 并将这个配置请求直接下推到 PCI 总线 1。  
(3) 在 PCI 总线 1 上, PCI 桥 2 的 Secondary Bus Number 为 2 而 Subordinate Bus Number 为 3。而 $1 < \text{Bus Number} \leqslant 3$ , 所以 PCI 桥 2 将接收来自 PCI 总线 0 的 Type 01h 配置请求, 并

将这个配置请求直接下推到PCI总线2。

(4) 在 PCI 总线 2 上, PCI 桥 3 的 Secondary Bus Number 为 3 , 因此 PCI 桥 3 将 “来自 PCI 总线 2 的 Type 01h 配置请求” 转换为 Type 00h 配置请求, 并将其下推到 PCI 总线 3 。 PCI 总线规定, 如果 PCI 桥的 Secondary Bus Number 与 Type 01h 配置请求中包含的 Bus Number 相同时, 该 PCI 桥将接收的 Type 01h 配置请求转换为 Type 00h 配置请求, 然后再发向其 Secondary Bus。  
(5) 在 PCI 总线 3 上, 有两个设备: PCI 设备 31 和 PCI 设备 32。在这两个设备中, 必然有一个设备将要响应这个 Type 00h 配置请求, 从而完成整个配置请求周期。在第 2.4.1 节中, 讨论了究竟是 PCI 设备 31 还是 PCI 设备 32 接收这个配置请求, 这个问题涉及 PCI 总线如何分配 PCI 设备使用的设备号。

# 2.4.3 PCI总线树Bus号的初始化

在一个处理器系统中，每一个HOST主桥都推出一棵PCI总线树。在一棵PCI总线树中有多少个PCI桥（包括HOST主桥），就含有多少条PCI总线。系统软件在遍历当前PCI总线树时，需要首先对这些PCI总线进行编号，即初始化PCI桥的Primary、Secondary和Sub-ordinateBusNumber寄存器。

在一个处理器系统中，一般将与HOST主桥直接相连的PCI总线命名为PCI总线0。然后系统软件使用DFS（DepthFirstSearch）算法，依次对其他PCI总线进行编号。值得注意的是，与HOST主桥直接相连的PCI总线，其编号都为0，因此当处理器系统中存在多个HOST主桥时，将有多个编号为0的PCI总线，但是这些编号为0的PCI总线分属不同的PCI总线域，其含义并不相同。

在一个处理器系统中，假设PCI总线树的结构如图2-13所示。当然在一个实际的处理器系统中，很少会出现这样复杂的PCI总线树结构，本节采用这个结构的目的是便于说明PCI总线号的分配过程。

![[pci_express/dceffec6a32cbf8d2621438bb9371251f2e9238edbc4835cf8a8c60faa8adf85.jpg]]  
图2-13 PCI总线树结构

在PCI总线中，系统软件使用深度优先DFS算法对PCI总线树进行遍历，DFS算法和广度优先BFS（BreadthFirstSearch）算法是遍历树型结构的常用算法。与BFS算法相比，DFS算法的空间复杂度较低，因此绝大多数系统在遍历PCI总线树时，都使用DFS算法而不是BFS算法。

DFS是搜索算法的一种，其实现机制是沿着一棵树的深度遍历各个节点，并尽可能深地搜索树的分支，DFS的算法为线性时间复杂度，适合对拓扑结构未知的树进行遍历。在一个处理器系统的初始化阶段，PCI总线树的拓扑结构是未知的，适合使用DFS算法进行遍历。下面以图2-13为例，说明系统软件如何使用DFS算法，分配PCI总线号，并初始化PCI桥中的PrimaryBusNumber、SecondaryBusNumber和SubordinateBusnumber寄存器。所谓DFS算法是指按照深度优先的原则遍历PCI胖树，其步骤如下。

(1) HOST 主桥扫描 PCI 总线 0 上的设备。系统软件首先忽略这条总线上的所有 PCI Agent 设备，因为在这些设备之下不会挂接新的 PCI 总线。例如 PCI 设备 01 下不可能挂接新的 PCI 总线。  
(2) HOST 主桥首先发现 PCI 桥 1, 并将 PCI 桥 1 的 Secondary Bus 命名为 PCI 总线 1。系统软件将初始化 PCI 桥 1 的配置空间, 将 PCI 桥 1 的 Primary Bus Number 寄存器赋值为 0 , 而将 Secondary Bus Number 寄存器赋值为 1 , 即 PCI 桥 1 的上游 PCI 总线号为 0 , 而下游 PCI 总线号为 1 。  
(3) 扫描 PCI 总线 1，发现 PCI 桥 2，并将 PCI 桥 2 的 Secondary Bus 命名为 PCI 总线 2。系统软件将初始化 PCI 桥 2 的配置空间，将 PCI 桥 2 的 Primary Bus Number 寄存器赋值为 1，而将 Secondary Bus Number 寄存器赋值为 2。  
(4) 扫描 PCI 总线 2，发现 PCI 桥 3，并将 PCI 桥 3 的 Secondary Bus 命名为 PCI 总线 3。系统软件将初始化 PCI 桥 3 的配置空间，将 PCI 桥 3 的 Primary Bus Number 寄存器赋值为 2，而将 Secondary Bus Number 寄存器赋值为 3。  
(5) 扫描 PCI 总线 3, 没有发现任何 PCI 桥, 这表示 PCI 总线 3 下不可能有新的总线,此时系统软件将 PCI 桥 3 的 Subordinate Bus number 寄存器赋值为 3。系统软件在完成 PCI 总线 3 的扫描后, 将回退到 PCI 总线 3 的上一级总线, 即 PCI 总线 2, 继续进行扫描。  
(6) 在重新扫描 PCI 总线 2 时，系统软件发现 PCI 总线 2 上除了 PCI 桥 3 之外没有发现新的 PCI 桥，而 PCI 桥 3 之下的所有设备已经完成了扫描过程，此时系统软件将 PCI 桥 2 的 Subordinate Bus number 寄存器赋值为 3。继续回退到 PCI 总线 1。  
(7) PCI 总线 1 上除了 PCI 桥 2 外, 没有其他桥片, 于是继续回退到 PCI 总线 0 , 并将 PCI 桥 1 的 Subordinate Bus number 寄存器赋值为 3 。  
（8）在PCI总线0上，系统软件扫描到PCI桥4，则首先将PCI桥4的PrimaryBusNumber寄存器赋值为0，而将SecondaryBusNumber寄存器赋值为4，即PCI桥1的上游PCI总线号为0，而下游PCI总线号为4。  
(9) 系统软件发现 PCI 总线 4 上没有任何 PCI 桥, 将结束对 PCI 总线 4 的扫描, 并将 PCI 桥 4 的 Subordinate Bus number 寄存器赋值为 4 , 之后回退到 PCI 总线 4 的上游总线, 即 PCI 总线 0 继续进行扫描。  
（10）系统软件发现在PCI总线0上的两个桥片PCI总线0和PCI总线4都已完成扫描后，将结束对PCI总线的DFS遍历全过程。

从以上算法可以看出，PCI桥的Primary Bus和Secondary Bus号的分配在遍历PCI总线树的过程中从上向下分配，而Subordinate Bus号是从下向上分配的，因为只有确定了一个PCI桥之下究竟有多少条PCI总线后，才能初始化该PCI桥的Subordinate Bus号。

# 2.4.4 PCI总线Device号的分配

一条PCI总线会挂接各种各样的PCI设备，而每一个PCI设备在PCI总线下具有唯一的设备号。系统软件通过总线号和设备号定位一个PCI设备之后，才能访问这个PCI设备的配置寄存器。值得注意的是，系统软件使用“地址寻址方式”访问PCI设备的存储器和I/O地址空间，这与访问配置空间使用的“ID寻址方式”不同。

PCI设备的IDSEL信号与PCI总线的AD[31:0]信号的连接关系决定了该设备在这条PCI总线的设备号。如上文所述，每一个PCI设备都使用独立的IDSEL信号，该信号将与PCI总线的AD[31:0]信号连接，IDSEL信号的含义见第1.2.2节。

在此我们简要回顾 PCI 的配置读写事务使用的时序。如图 1-3 所示，PCI 总线事务由一个地址周期加若干个数据周期组成。在进行配置读写请求总线事务时，C/BE#信号线的值在地址周期中为 $0 \times 1010$ 或者为 $0 \times 1011$ ，表示当前总线事务为配置读或者配置写请求。此时出现在 AD [31:0] 总线上的值并不是目标设备的 PCI 总线地址，而是目标设备的 ID 号，这与 PCI 总线进行 I/O 或者存储器请求时不同，因为 PCI 总线使用 ID 号而不是 PCI 总线地址对配置空间进行访问。

如图2-12所示，在配置读写总线事务的地址周期中，AD[10:0]信号已经被FunctionNumber和Register Number使用，因此PCI设备的IDSEL只能与AD[31:11]信号连接。

认真的读者一定可以发现在 CONFIG\_ADDRESS 寄存器中 Device Number 字段一共有 5 位可以表示 32 个设备，而 AD [31:11] 只有 21 位，显然在这两者之间无法建立一一对应的映射关系。因此在一条 PCI 总线上如果有 21 个以上的 PCI 设备，那么总是有几个设备无法与 AD [31:11] 信号线连接，从而 PCI 总线无法访问这些设备。因为 PCI 总线在配置请求的地址周期中，只能使用第 31\~11 这些 AD 信号，所以在一条总线上最多也只能挂接 21 个 PCI 设备。这 21 个设备可能是从 0 到 20，也可能是从 11 到 31 排列。从而系统软件在遍历 PCI 总线时，还是需要从 0 到 31 遍历整条 PCI 总线。

在实际的应用中，一条PCI总线能够挂接21个设备已经足够了，实际上由于PCI总线的负载能力有限，即便在总线频率为 $33\mathrm{MHz}$ 的情况下，在一条PCI总线中最多也只能挂接10个负载，一条PCI总线所能挂接的负载详见表1-1。AD信号线与PCI设备IDSEL线的连接关系如图2-14所示。

![[pci_express/6ad375b0ebb81ef25f37d95de4946f5784f605fca8434ca3140cbe0153a484ba.jpg]]  
图2-14 PCI总线设备号的分配

PCI总线推荐了一种Device Number字段与AD[31:16]之间的映射关系。其中PCI设备0与Device Number字段的0b00000对应；PCI设备1与Device Number字段的0b00001对应，并以此类推，PCI设备15与Device Number字段的0b01111对应。

在这种映射关系之下，一条PCI总线中，与信号线AD16相连的PCI设备的设备号为0；与信号线AD17相连的PCI设备的设备号为1；以此类推，与信号线AD31相连的PCI设备的设备号为15。在Type00h配置请求中，设备号并没有像Function Number和Register Number那样以编码的形式出现在AD总线上，而是与AD信号一一对应，如图2-12所示。

这里有一个原则需要读者注意，就是对PCI设备的配置寄存器进行访问时，一定要有确定的Bus Number、Device Number、Function Number和Register Number，这“四元组”缺一不可。在Type 00h配置请求中，Device Number由AD[31:11]信号线与PCI设备IDSEL信号的连接关系确定；Function Number保存在AD[10:8]字段中；而Register Number保存在AD[7:0]字段中；在Type 01h配置请求中，也有完整的四元组信息。

在一个处理器系统的设计中，如果在一条PCI总线上使用的PCI插槽少于4个时，笔者建议优先使用AD[17:20]信号与PCI设备的IDSEL信号连接。因为PCI-X总线规范建议使用AD17连接PCI设备1、AD18连接PCI设备2、AD19连接PCI设备3、AD20连接PCI设备4，采用这种方法便于实现PCI总线与PCI-X总线的兼容。

# 2.5 非透明PCI桥

PCI桥规范定义了透明桥的实现规则，在第2.3.1节中详细介绍了这种桥片。通过透明桥，处理器系统可以以HOST主桥为根节点，建立一颗PCI总线树，在这个树上的PCI设备共享同一个PCI总线域上的地址空间。

但是在某些场合下PCI透明桥并不适用。在图2-15所示的处理器系统中存在两个处理器，此时使用PCI桥1连接处理器2并不利于整个处理器系统的配置与管理。假定PCI总线使用32位地址空间，而处理器1和处理器2所使用的存储器大小都为2GB，同时假定处理器1和处理器2使用的存储器都可以被PCI设备访问。

![[pci_express/5fa5751edcd82ffbc137e1d902bfed87b3897fc6d812a8866c9e12e0586b441d.jpg]]  
图2-15 不适合PCI透明桥的处理器系统互连方式

此时处理器1和2使用的存储器空间必须映射到PCI总线的地址空间中，而32位的PCI总线只能提供4GB地址空间，此时PCI总线x0的地址空间将全部被处理器1和2的存储器空间占用，而没有额外的空间分配给PCI设备。

此外有些处理器不能作为PCI Agent设备，因此不能直接连接到PCI桥上，比如x86处理器就无法作为PCI Agent设备，因此使用PCI透明桥无法将两个x86处理器直接相连。如果处理器2有两个以上的PCI接口，其中一个可以与PCI桥1相连（此时处理器2将作为PCI Agent设备），而另一个作为HOST主桥y连接PCI设备。此时HOST主桥y挂接的PCI设备将无法被处理器1直接访问。

使用透明桥也不便于解决处理器1与处理器2间的地址冲突。对于图2-15所示的处理器系统，如果处理器1和2都将各自的存储器映射到PCI总线地址空间中，有可能出现地址冲突。虽然PowerPC处理器可以使用Inbound寄存器，将存储器地址空间映射到不同的PCI总线地址空间中，但并非所有的处理器都具有这种映射机制。许多处理器的存储器地址与PCI总线地址使用了“简单相等”这种映射方法，如果PCI总线连接了两个这样的处理器，将不可避免地出现PCI总线地址的映射冲突。

采用非透明桥将有效解决以上这些问题，非透明桥并不是PCI总线定义的标准桥片，但是这类桥片在连接两个处理器系统中得到了广泛的应用。一个使用非透明桥连接两个处理器系统的实例如图2-16所示。

![[pci_express/19718af37e988cece57e11b211cb93fb6b3217704ea522ed51d6be5bbb145abc.jpg]]  
图2-16 使用PCI非透明桥连接两个处理器系统

使用非透明PCI桥可以方便地连接两个处理器系统。从图2-16中我们可以发现非透明桥可以将PCI总线 $\mathbf{X}$ 域与PCI总线y域进行隔离。值得注意的是，非透明PCI桥的作用是对不同PCI总线域地址空间进行隔离，而不是隔离存储器域地址空间。而HOST主桥的作用才是将存储器域与PCI总线域进行隔离。

非透明PCI桥可以连接两条独立的PCI总线，一条被称为SecondaryPCI总线，另一条被称为PrimaryPCI总线，但是这两条总线没有从属关系，两边是对等的。从处理器 $\mathbf{X}$ 的角

度来看，与非透明 PCI 桥右边连接的总线叫 Secondary PCI 总线；而从处理器 y 的角度来看，非透明 PCI 桥左边连接的总线叫 Secondary PCI 总线。

HOST 处理器 x 和 PCI 设备可以通过非透明 PCI 桥直接访问 PCI 总线 y 域的地址空间，并通过 HOST 主桥 y 访问存储器 y；HOST 处理器 y 和 PCI 设备也可以通过非透明 PCI 桥，直接访问 PCI 总线 x 域的地址空间，并通过 HOST 主桥 x 访问存储器 x。为此非透明 PCI 桥需要对分属不同 PCI 总线域的地址空间进行转换。

目前有许多厂商可以提供非透明PCI桥的芯片，在具体实现上各有差异，但是其基本原理类似，下面以Intel21555为例说明非透明PCI桥。值得注意的是，在PCIe体系结构中，也存在非透明PCI桥的概念。

# 2.5.1 Intel 21555中的配置寄存器

Intel 21555 非透明 PCI 桥源于 DEC21554，井在此基础上做了一些改动。Intel 21555 桥片与其他透明桥在系统中的位置相同。如图 2-16 所示，这个桥片一边与 Primary PCI 总线相连，另一边与 Secondary PCI 总线相连。

在Intel21555桥片中，包含两个PCI设备配置空间，分别是PrimaryPCI总线配置空间和SecondaryPCI总线配置空间，处理器可以使用Type00h配置请求访问这些配置空间。在大多数情况之下，在PrimaryPCI总线上的HOST处理器管理PrimaryPCI配置空间；在SecondaryPCI总线上的HOST处理器管理SecondaryPCI配置空间。

在Intel21555桥片中，还有一组私有寄存器CSR（ControlandStatusRegister），系统软件使用这组寄存器对非透明桥进行管理并获得桥片的一些信息，这组寄存器可以被映射成为PCI总线的存储器地址空间或者I/O地址空间。

本章仅介绍 Primary PCI 总线这一边的配置寄存器，Secondary PCI 总线的配置寄存器虽然与 Primary PCI 总线的这些寄存器略有不同，但是基本对等，因此本节对此不做介绍。Primary PCI 总线的主要寄存器如表 2-6 所示。

表 2-6 Primary PCI 总线的配置寄存器

<table><tr><td>Offset</td><td>寄存器</td><td>PCI配置寄存器</td><td>复位值</td></tr><tr><td>0x13~0x10</td><td>Primary CSR and Memory 0 BAR</td><td>BAR0</td><td>0x0000-0000</td></tr><tr><td>0x17~0x14</td><td>Primary CSR I/O BAR</td><td>BAR1</td><td>0x0000-0001</td></tr><tr><td>0x1B~0x18</td><td>Downstream Memory 1 BAR</td><td>BAR2</td><td>0x0000-0000</td></tr><tr><td>0x1F~0x1C</td><td>Downstream Memory 2 BAR</td><td>BAR3</td><td>0x0000-0000</td></tr><tr><td>0x23~0x20</td><td>Downstream Memory 3 BAR</td><td>BAR4</td><td>0x0000-0000</td></tr><tr><td>0x27~0x24</td><td>Downstream Memory 3 Upper 32 Bits</td><td>BAR5</td><td>0x0000-0000</td></tr><tr><td>0x97~0x94</td><td>Downstream Memory 0 Translated Base</td><td>None</td><td>不确定</td></tr><tr><td>0x9B~0x98</td><td>Downstream I/O or Memory 1 Translated Base</td><td>None</td><td>不确定</td></tr><tr><td>0x9F~0x9C</td><td>Downstream Memory 2 Translated Base</td><td>None</td><td>不确定</td></tr><tr><td>0xA3~0xA0</td><td>Downstream Memory 3 Translated Base</td><td>None</td><td>不确定</td></tr></table>

从表2-6中，我们可以发现PrimaryPCI总线的这些配置寄存器共分为两组，一组寄存器与PCI设备的配置寄存器的 $\mathrm{BAR0}\sim 5$ 对应，这些寄存器与标准PCI配置寄存器 $\mathrm{BAR0}\sim 5$ 的功能相同；另一组寄存器是TranslatedBase寄存器，这组寄存器的主要作用是将来自PrimaryPCI总线的数据访问转换到SecondaryPCI总线。

其中 $\mathrm{BAR0} \sim 5$ 寄存器在系统初始化时由 Primary PCI 总线上的 HOST 处理器进行配置，配置过程与 PCI 总线上的普通设备完全相同。只是 Intel 21555 规定，BAR0 只能映射为 32 位存储器空间。

CSR 寄存器可以根据需要映射在 BAR0 空间中, 此时 BAR0 空间最小为 $4 \mathrm{KB}$ 。CSR 寄存器也可以根据需要使用 BAR1 寄存器映射为 I/O 地址空间, 同时 BAR1 寄存器还可以映射其他 I/O 空间; $\mathrm{BAR2} \sim 3$ 只能映射为 32 位存储器地址空间; 而 $\mathrm{BAR4} \sim 5$ 用来映射 64 位的存储器地址空间。

对于 Primary PCI 总线, 所有 BAR0\~5 寄存器映射的地址空间都将占用 Primary PCI 总线域, 然而这些地址空间中所对应的数据并不在 Primary PCI 总线域中, 而是在 Secondary PCI 总线域中。Translated Base 寄存器实现不同 PCI 总线域地址空间的转换, Intel 21555 将不同 PCI 总线域地址空间的转换过程称为“地址翻译”。

Intel 21555 支持两种地址翻译方法, 一种是直接地址翻译, 另一种是查表翻译。Primary PCI 总线的 BAR 空间只支持直接地址翻译, 而 Secondary PCI 总线的 Memory 2 BAR 空间支持查表翻译, 本节仅介绍直接地址翻译方法, 对查表翻译有兴趣的读者请阅读 Intel 21555 的数据手册。直接地址翻译过程如图 2-17 所示。

![[pci_express/20ecc1b1ddfd345b8fae26adf809cc9ed70d6723a4a4254510c79102b8a961b1.jpg]]  
图2-17 21555的直接地址翻译过程

当 Primary PCI 总线对非透明桥 21555 的 BAR0 \~ 5 地址空间进行数据请求时, 这个数据请求将被转换为对 Secondary PCI 总线的数据请求。Translated Base 寄存器将完成这个地址翻译过程, 下节将结合实例说明这个直接地址翻译过程。

# 2.5.2 通过非透明桥片进行数据传递

下面以图2-16中处理器 $\mathbf{X}$ 访问处理器y存储器地址空间的实例，说明非透明桥21555如何将PCI总线 $\mathbf{X}$ 域与PCI总线y域联系在一起。

处理器 $\mathbf{X}$ 在访问处理器y的存储器空间之前，需要做一些必要的准备工作。

(1) 首先确定由哪一个 BAR 寄存器空间映射处理器 y 的存储器地址空间。本节假定使用 BAR2 寄存器映射处理器 y 的存储器地址空间。  
(2) BAR2 寄存器使用 Downstream Memory 2 Translated Base 寄存器, 将来自 Primary PCI 总线的访问转换为对 Secondary PCI 总线地址空间的访问。其中 Downstream Memory 2 Translated Base 寄存器可以由处理器 x 或者处理器 y 根据需要进行设置。

假定处理器 $\mathbf{X}$ 和y的HOST主桥使用“直接相等”策略，建立存储器域与PCI总线域间的映射；而处理器 $\mathbf{X}$ 使用BAR2地址空间访问处理器 $\mathrm{y}$ 存储器空间 $0\mathrm{x}1000 - 000\sim 0\mathrm{x}1\mathrm{FFF}-$ FFFF;处理器 $\mathbf{X}$ 的系统软件事先将BAR2寄存器设置完毕。处理器 $\mathbf{X}$ 访问处理器 $\mathrm{y}$ 的这段存储器空间的步骤如下，读者可参考图2-18理解以下步骤。

(1) 首先处理器 $\mathrm{x}$ 访问在处理器 $\mathrm{x}$ 域中,且与非透明桥的 BAR2 空间相对应的存储器地址空间。  
(2) HOST 主桥将进行存储器域到 PCI 总线域的转换, 并将这个请求发送到 Primary PCI 总线上。  
(3) 非透明桥发现这个数据请求发向 BAR2 地址空间, 则接收这个数据请求, 并在桥片中暂存这个数据请求。  
(4) 非透明桥根据 Downstream Memory 2 Translated Base 寄存器的内容, 按照图 2-17 所示的规则进行地址转换。假设 Downstream Memory 2 Translated Base 寄存器的基地址被预先设置为 $0 \times 1000 - 0000$ , 大小为 $256 \mathrm{MB}$ (这个物理地址属于处理器 y 的主存储器地址空间)。  
(5) 经过非透明桥的转换后, 这个数据请求将穿越非透明桥, 从 Primary PCI 总线域进入 Secondary PCI 总线域, 然后访问处理器 y 的基地址为 $0 \times 1000 - 0000$ 的存储器区域。  
(6) 处理器 y 的 HOST 主桥接收这个存储器访问请求, 并最终将数据请求发向处理器 y 的存储器中。

![[pci_express/81a35f387727faa49f354952f024b964ea336d9d420d83cd1a7a4c85b106625b.jpg]]  
图2-18 通过非透明桥21555进行数据传递

非透明桥21555除了可以支持存储器到存储器之间的数据传递，还支持PCI总线域到存储器域，以及PCI总线域之间的数据传递，此外非透明桥21555还可以通过 $\mathrm{I}^2\mathrm{O}$ 和Doorbell寄存器进行PrimaryPCI总线与SecondaryPCI总线之间的中断信号传递。本节对这部分内容不做进一步介绍。

非透明桥有效解决了使用PCI总线连接两个处理器存在的问题，因而得到了广泛的应用。在PCIe体系结构中，也存在非透明PCI桥的概念。如在PLX的Switch芯片中，各个端口都可以设置为非透明模式。

# 2.6 小结

本章介绍了在PCI总线中使用的桥，包括HOST主桥和PCI桥，并较详细地介绍了如何使用这些桥访问PCI设备的配置空间。

其中HOST主桥并不在PCI总线规范的约束范围内，不同的处理器可以根据需要设计出不同的HOST主桥。本章更加侧重介绍PowerPC处理器使用的HOST主桥，在该主桥的设计中，提出了许多新的概念，并极大促进了PCI总线的发展，在这个桥片中出现的许多新的思想被PCI V3.0总线规范采纳。

在PowerPC处理器的HOST主桥中，明确了存储器域与PCI总线域的概念。而区分存储器域与PCI总线域也是本章的书写重点，本书将始终强调这两个域的不同。有些处理器系统并没有明确区分这两个域的差别，因此许多读者忽略了PCI总线域的存在，并错误地认为PCI总线域是存储器域的一部分。

本章还重点介绍了PCI桥的实现机制。在许多较简单的处理器系统中，并不包含PCI桥，但是读者仍然需要深入理解PCI桥这一重要概念。深入理解PCI桥的运行机制，是理解PCI体系结构的重要基础。

