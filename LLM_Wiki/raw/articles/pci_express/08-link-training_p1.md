---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "08"
section: "第8章 PCIe总线的链路训练与电源管理"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第8章 PCIe总线的链路训练与电源管理

PCIe链路的初始化过程较为复杂。PCIe总线进行链路训练时将初始化PCIe设备的物理层、发送接收模块和相关的链路状态信息，当链路训练成功结束后，PCIe链路两端的设备可以进行正常的数据交换。

链路训练的过程由硬件逻辑完成，而无需系统软件的参与。此外当PCIe设备从低功耗状态返回到正常工作模式时，或者PCIe链路出现某些错误时，PCIe链路也需要重新进行链路训练。

# 8.1 PCIe链路训练简介

PCIe总线进行链路训练的主要目的是初始化PCIe链路的物理层、端口配置信息、相应的链路状态，并了解链路对端的拓扑结构，以便PCIe链路两端的设备进行数据通信。一条PCIe总线提供的链路带宽可以是 $\times 1$ 、 $\times 2$ 、 $\times 4$ 、 $\times 8$ 、 $\times 12$ 或者 $\times 16$ ，但是在这个PCIe链路上所挂接的PCIe设备并不会完全使用这些链路。如一个 $\times 4$ 的PCIe设备可能会连接到 $\times 16$ 的PCIe链路上。此时该PCIe设备在进行链路训练时，必须通知对端链路该设备实际使用的链路状态。

此外PCIe总线规定，PCIe链路两端的设备所使用的Lane可以错序进行连接，PCIe总线规范将该功能称为“LaneReversal”。在相同的Lane上，差分信号的极型也可以错序连接，PCIe总线规范将该功能称为PolarityInversion。这两种错序连接方式如图8-1所示。

![[pci_express/f6931d653ef6363905fbd0276cb7e1c7c9421101a21ffd2148d3c71f1822a26e.jpg]]  
正常连接

![[pci_express/60478df05fb3ce060c7d24bcb576af012acee91f0530fd64df41aebaf7340628.jpg]]  
Lane Reversal

![[pci_express/368d9fc9b3f45d642928fdb5e7a7bb0e939d6ecfe1132e9f3aabbad47d3f5e11.jpg]]  
Polarity Inversion   
图8-1 PCIe设备的错序连接

PCIe总线提供这些连接方式的主要目的是为了方便PCB走线，因为差分信号要求在PCB中等长而且等距。在一个系统中，如果存在多路差分信号时，PCB布线较为困难。PCIe链路允许“Lane Reversal”和“Polarity Inversion”这两个功能，便于PCBLayout工程师根据实际情况为差分信号选择更为合理的走线路径，从而降低PCB的层数。除了PCIe链路，还有许多使用差分信号的串行总线也支持“Lane Reversal”和“Polarity Inversion”这两个功能，但是称呼上有所区别。在一条PCIe链路中，可以同时支持“Lane Reversal”和“Polarity Inversion”这两个功能。

PCIe链路进行链路训练时，需要了解PCIe链路两端的连接拓扑结构。一条PCI链路可能使用多个Lane进行数据交换，而数据报文经过不同Lane的延时并不完全相同。PCIe总线进行链路训练时，需要处理这些不同Lane的延时差异，并进行补偿。PCIe总线规范将这个过程称为De-skew。

此外PCIe总线在链路训练过程中，还需要确定数据传送率。PCIe V1.x总线使用的数据传送率为2.5GT/s，PCIe V2.0总线使用5.0GT/s，而PCIe V3.0总线使用8GT/s的数据传送率。当分属不同规范的PCIe设备使用同一个PCIe链路进行连接时，需要统一数据传送率。如一个V1.x的PCIe设备与一个V2.0的RC或者Switch连接时，需要将数据传送率统一为2.5GT/s。在PCIe总线中，如果一个PCIe链路的两端分别连接不同类型的PCIe设备时，将选择较低的数据传送率。值得注意的是，PCIe链路在进行初始化时，首先使用2.5GT/s的数据传送率，之后切换到更高的数据传送率，如5GT/s或者8GT/s。

在讲述PCIe链路训练之前，读者需要了解一些与Link Number和Lane Number相关的基本概念。在多端口RC和Switch中具有多个下游端口，而每个端口可以支持 $\times 1$ 、 $\times 2$ 、 $\times 4$ 等不同宽度的Lane，如图8-2所示。

![[pci_express/8307e6cbb304d50dc2ac4e0cf4f2ca0ce2048c3d3db3a80ff629efa055dc70f6.jpg]]  
图8-2 Link Number和Lane Number

在一个Switch中存在多个下游链路，并使用 $0\sim \mathrm{n}$ 进行编号，其中 $\mathrm{n}\leqslant 255$ 。这些编号保存在Switch的硬件逻辑中，而不在Switch的配置空间中。这个编号也被称为Link Number，上图所示的Switch中含有两个Link Number，分别为1和2。

在Switch中，还有两类Lane Number，分别是物理“Lane Number”和逻辑“Lane Number”。其中物理“Lane Number”是链路训练之前使用的Lane number。一个PCIe链路的物理“Lane Number”编号为 $0\sim \mathrm{n}$ ，其中 $\mathbf{n}$ 为PCIe链路的最大Lane Number，如果一个PCIe链路上有8个Lane，则 $\mathrm{n}$ 等于7。

而逻辑“Lane Number”是链路训练结束后使用的Lane Number。如图8-1左图所示，PCIe链路允许错序连接，因此物理“Lane Number”与逻辑“Lane Number”并不相同。物

理“Lane Number”与逻辑“Lane Number”的对应关系在链路训练中确定。

除此之外，有些Switch支持多种链路配置方式，假设某个Switch的下游支持8个Lane。这8个Lane可以组成1个PCIe链路，其链路宽度为8；也可以组成2个PCIe链路，每个链路宽度为4；也可以组成4个链路，每个链路宽度为2。

该Switch的物理LaneNumber的编号方法不变，都是从 $0\sim 7$ ，但是逻辑LaneNumber的编号方法将有所区别。如图8-2所示的Switch中具有8个Lane，组成两个PCIe链路，其中每条PCIe链路的逻辑LaneNumber的编号都为 $0\sim 3$ 。

PCIe总线进行链路训练时，需要进行RC或者Switch的Link Number和Lane Number的初始化，在第8.2节中将详细介绍这些内容。

PCIe总线进行数据传递时，需要使用时钟进行同步，但是PCIe链路并没有提供这个时钟信号，因此在进行链路训练时，接收端需要从发送端的数据报文中提取接收时钟。PCIe总线规范将这个获得接收时钟的过程称为“Bit Lock”。

在链路训练过程中，PCIe链路需要首先确定COM字符，该字符也标志着链路训练或者链路重训练的开始，PCIe总线规范将确定COM字符的过程称为“SymbolLock”。如表7-6所示，COM字符为“0011111010”或者“1100000101”，该字符为2个“0”后5个“1”或者2个“1”后5个“0”，非常便于硬件识别。BitLock和SymbolLock的过程也需要在PCIe总线的链路训练中进行。

# 8.1.1 链路训练使用的字符序列

PCIe总线进行链路训练时，需要发送一些特殊的字符序列（Ordered-Sets），这些Oderer-Sets将在下文中详细介绍，PCIe总线规范定义了以下几类Ordered-Sets。有的书籍也将这些Ordered-Sets称为PLP，即物理层报文。

- Training Sequence 1 和 2，简称为 TS1 和 TS2 序列。这两种 PLP 在链路训练的多个状态机中使用，下文将进一步介绍这两种字符序列。  
- Idle 序列。在正常情况下，发送端进入 Electrical Idle 状态时，将首先向对端发送若干 Idle 序列，才能进入。Electrical Idle 状态是 PCIe 链路的一个低功耗状态，第 8.1.2 节将详细介绍该状态。  
- Fast Training Sequence，简称为 FTS。该字符序列协助接收逻辑获得 Bit/Symbol Lock，接收逻辑需要获得多个 FTS 后，才能确定 Bit/Symbol Lock。  
- SKIP 序列。该字符序列的主要作用是进行时钟补偿。

在 PCIe 总线中，字符序列的发送方式与 TLP 和 DLLP 有较大不同。假设一条 PCIe 链路由多个 Lane 组成，那么 TLP 和 DLLP 报文将分散到多个 Lane 中。而字符序列必须同时出现在这些不同的 Lane，这几个 Lane 必须“在同一个时间点”发送字符序列。而不能出现一个 Lane 正在发送这个字符序列进行与链路训练相关的操作，而其他 Lane 进行其他数据传递的情况。PCIe 链路发送 TLP 与发送字符序列的过程如图 8-3 所示。

如在一个 $\times 4$ 的PCIe链路中发送SKIP序列时，每一个Lane中都要出现“COM、SKP、SKP、SKIP”这样的数据流。其他字符序列的发送方法也与此类似。而TLP或者DLLP的发送分散到各个Lane上。

![[pci_express/5da77134b273a0529411b6e626d77e5d885c8be0c20069015cc963e919441496.jpg]]  
图8-3 特殊字符序列的发送

# 1. TS1 和 TS2 序列

在物理层的LTSSM状态机中，TS1和TS2序列的使用方法不同。其中TS1序列的主要作用是检测PCIe链路的配置信息，而TS2序列确认TS1序列的检测结果。

TS1 和 TS2 序列由 16 个字符组成，单纯从结构上看，TS1 和 TS2 仅仅是第 6\~15 个字符的含义不同，但是这两个序列在 LTSSM 状态机中的使用方法不同。TS1 的第 6\~15 个字符为 D10.2，而 TS2 的第 6\~15 个字符为 D5.2，其中 D10.2 和 D5.2 也是 TS1 和 TS2 的标识号。TS1 和 TS2 序列的其他字符如下所示。

- 第0字符为COM控制字符，表示TS1/TS2序列的开始。TS1/TS2字符序列将复位LF-SR寄存器。  
- 在链路训练的初始阶段，第 1 字符存放控制字符 PAD，即为空。而在链路的配置阶段，该字符存放端口使用的 Link Number。  
- 在链路训练的初始阶段，第2字符为控制字符PAD，即为空。而在链路的配置阶段，该字符存放端口使用的Lane Number。  
- 第3个字符为FTS序列的个数（N\_FTS）。不同的PCIe链路需要使用不同数目FTS序列，才能使接收端的PLL锁定接收时钟。  
- 第4个字符存放当前PCIe设备支持的数据传送率，第1位为1表示支持2.5GT/s传送率；第2位为1表示支持5GT/s传送率；第3位为1表示支持8GT/s的数据传送率（在PCIe V3.0规范中使用）；第4\~5位保留；第6位是一个多功能位，当PCIe链路的没有配置成功时可以作为Notification位，也可以用作发送链路De-emphasis的使能位；第7位为speed\_change位，当该位为1时，通知PCIe链路对端设备需要改变传送速率。  
- 第5个字符存放命令。第0位为“Hot Reset”，第1位为“Disable Link”，第2位为“Loopback”，第3位为“Disable Scrambling”，第4位为“Compliance Receive”。当接收逻辑RX收到TS1或者TS2序列后，将根据该字符的命令进行对应的操作。

# 2. Idle序列

在正常情况下，当发送端进入 Electrical Idle 状态之前，必须向对端发送 EIOS 序列。如果 PCIe 设备使用 2.5GT/s 的传送率时，Idle 序列由 1 个 COM 字符加 3 个 IDL 字符组成，即“COM IDL IDL IDL”；如果 PCIe 设备使用 5GT/s 的传送率时，Idle 序列由两组这样的字符序列组成，即“COM IDL IDL IDL COM IDL IDL IDL”。Electrical Idle 状态是一种特殊的 Idle 状态，处于该状态时，PCIe 链路使用的功耗最低，该状态的详细解释见第 8.1.2 节。

当发送端退出IDLE状态时，必须向对端发送EIEOS序列。EIEOS序列仅在链路传送率大于2.5GT时使用，该序列由1个COM字符、14个EIE字符和D10.2（TS1识别符）组成。

PCIe 设备可以根据链路的使用情况确定当前链路是否处于 Electrical Idle 状态，而不是必须收到 Idle 序列后进入该状态。如一个 PCIe 设备在很长一段时间没有收到流量控制报文或者链路处于 Electrical Idle 状态时，也可以推断出对端设备处于 Idle 状态。

# 3. FTS序列

单个 FTS 序列由 1 个 COM 字符加 3 个 FTS 字符组成，该序列的主要目的是使接收逻辑 RX 重新获得 Bit/Symbol Lock。发送逻辑需要向对端发送多少个 FTS 序列由接收到的 TS1/2 序列决定，TS1/2 序列的第 3 个字符为需要发送 FTS 序列的个数。

# 4. SKIP序列

SKIP序列由一个COM字符加3个SKIP字符组成。物理层提供SKIP序列的主要原因是进行时钟补偿。假设一个PCIe设备使用的时钟频率为 $2.5\mathrm{GHz} \pm 300\mathrm{ppm}$ ，其中 $300\mathrm{ppm}$ 意味着这个时钟源每发出1百万个时钟可能产生300个时钟漂移，即每3333个时钟将可能产生一个时钟漂移。如果PCIe链路不使用SKIP序列，本地时钟与“从报文中提取”的时钟存在的漂移，可能导致数据传送失败。

在PCIe设备的接收逻辑RX中，使用了两个时钟，一个时钟是通过PLL从接收报文中恢复的时钟，另一个时钟是接收逻辑RX使用的本地时钟。这两个时钟间的关系如图8-4所示。值得注意的是这两个时钟并不完全同步。

![[pci_express/53f60b1d2af88f0c1a1c6d5ce0345d8cec28fcb783df901fe520dc48519a445b.jpg]]  
图8-4 本地时钟域与从报文中恢复的时钟域

在PCIe总线中，使用ElasticBuffer技术处理这两个时钟之间的频率差和相位差。ElasticBuffer处于本地时钟域与“被恢复的”时钟域之间，由一个同步FIFO组成。该FIFO的一端使用本地时钟域、而另一端使用“被恢复的”时钟域。其中本地时钟与“被恢复的”时钟频率都为 $2.5\mathrm{GHz}\pm 300~\mathrm{ppm}$

但是如果PCIe设备从数据报文中恢复的时钟频率为 $2.5\mathrm{GHz} - 300\mathrm{ppm}$ , 而本地时钟频率

为 $2.5\mathrm{GHz} + 300\mathrm{ppm}$ 时，ElasticBuffer两端的时钟频率并不匹配，ElasticBuffer将出现Overrun的现象；而如果本地时钟频率小于“被恢复的”时钟时，ElasticBuffer将可能出现Underrun的现象。如果PCIe总线不采取一些必要的补救措施，那么无论ElasticBuffer的容量有多么大，都可能出现Overrun和Underrun的现象。

为此，PCIe总线规定，物理层的每个Lane发送 $1180\sim 1538$ 个字符之后，必须发送一个SKIP序列进行时钟补偿。因为在最恶劣的情况下，接收逻辑RX每过1667个时钟周期，本地时钟就可能与“被恢复的”时钟相差一个时钟周期。在PCIe总线中，当ElasticBuffer收到SKIP序列时，可以根据自身的状态选择是增加还是减少 $1\sim 2$ 个SKIP序列，从而补偿本地时钟与“被恢复的”时钟之间的频率与相位差。

在一个具体的实现中，可以通过计算，得到Elastic Buffer不出现Overrun和Underrun所需要的最小尺寸。这个最小尺寸与SKP序列的发送间隔（多少个时钟周期发送一次SKP序列），Max\_Payload\_Size参数和PCIe链路的数据传送率相关。对此有兴趣的读者可以参考Elastic Buffer Implementations in PCI Express Devices，以获得详细的量化分析结果，本节对此不做进一步说明。

Elastic Buffer 技术由来已久，除了 PCIe 总线之外，USB 总线、InfiniBand、Fibre Channel 和 Gigabit Ethernet 中也使用该技术处理分属不同时钟域的数据传递。

# 8.1.2 Electrical Idle 状态

当发送端或者接收端进入 Electrical Idle 状态后，两端的发送逻辑 TX 将驱动 $\mathrm{D}+$ 和 $\mathrm{D}-$ 信号的对地电压为相同的值，其值等于 DC 共模电压（DC 共模电压的定义见第 7.3.1 节），从而使发送逻辑 TX 处于“最低功耗”状态。

当发送端处于正常工作模式时， $\mathrm{D}+$ 和 $\mathrm{D}-$ 信号差值电压的峰峰值为 $\mathrm{V}_{\mathrm{TX-DIFF-PP}}$ ，其值在 $0.8\mathrm{~V}\sim 1.2\mathrm{~V}$ 之间，而当发送端处于 Electrical Idle 状态时， $\mathrm{D}+$ 和 $\mathrm{D}-$ 信号差值电压的峰峰值为 $\mathrm{V}_{\mathrm{TX-IDLE-DIFF-DC}}$ ，其值在 $0\sim 5\mathrm{~mv}$ 之间。

由此可以发现当发送端处于 Electrical Idle 状态时，在示波器中显示的 D + 和 D - 信号的对地电压基本相等，此时发送端处于完全“静止”状态，在发送逻辑 TX 中基本没有电流通过，因此从理论上讲处于“最低功耗”状态。

在正常情况下，当发送端或者接收端进入 Electrical Idle 状态之前，都需要使用发送逻辑 TX 向对端发送若干 EIOS 序列，之后才能进入该状态。当发送逻辑 TX 处于 Electrical Idle 状态时，可以处于高阻抗或者低阻抗模式，PCIe 总线规范对此并没有限制。而接收逻辑 RX 处于 Electrical Idle 状态时，其 DC 共模输入阻抗必须在 PCIe 总线规范要求的范围内。

当发送逻辑 TX 进入 Electrical Idle 状态后，至少需要经过 TTX-IDLE-MIN 这段时间延时后，才允许退出该状态，因为对端接收逻辑 RX 的 “Electrical Idle Exit detector” 部件从启动到正常工作的时间延时为 TTX-IDLE-MIN。如果在 “Electrical Idle Exit detector” 部件没有正常工作之前，发送逻辑 TX 就退出 Electrical Idle 状态，对端的接收逻辑 RX 将不能检查到这个状态变化，从而导致错误。

发送逻辑 TX 在退出 Electrical Idle 状态时，必须在 $\mathrm{T_{TX - IDLE - TO - DIFF - DATA}}$ 时间范围内，需要为 $\mathrm{D + }$ 和 $\mathrm{D - }$ 信号提供正常的工作伏值， $\mathrm{D + }$ 和 $\mathrm{D - }$ 信号的峰峰值至少为 $\mathrm{V_{TX - DIFF - PP}}$ ，其值在 $0.8\mathrm{V}\sim 1.2\mathrm{V}$ 之间。而对端的接收逻辑 RX 发现其 $\mathrm{V_{RX - IDLE - DET - DIFFp - p}}$ 的值小于 $75\mathrm{mV}$ 时，将不会退出 Electrical Idle 状态，只有当 $\mathrm{V_{RX - IDLE - DET - DIFFp - p}}$ 的值大于 $175\mathrm{mV}$ 时，对端的接收逻辑 RX 才会退出 Electrical Idle 状态。

值得注意的是，发送逻辑 TX 经过发送链路将信号传递到接收逻辑 RX 时，信号将会衰减。虽然发送逻辑 TX 输出的 VTX-DIFF-PP 在 $0.8\mathrm{V} \sim 1.2\mathrm{V}$ 之间，但是接收端收到的 VRX-IDLE-DET-DIFFp-p，其最小值可能只有 $175\mathrm{mV}$ 左右。

在正常情况下，接收逻辑 RX 在进入到 Electrical Idle 状态之前，需要收到发送逻辑 TX 提供的 EIOS 序列，但是有时接收逻辑 RX 在没有收到 EIOS 序列时，发现 VRX-IDLE-DET-DIFFp-p 的值小于 $75\mathrm{mv}$ 时，也可以进入 Electrical Idle 状态。PCIe 总线规范要求接收逻辑 RX 必须在 $10\mathrm{ms}$ 之内判断出当前链路是否处于 Electrical Idle 状态。

接收逻辑 RX 除了可以使用差分电压逻辑，检测当前链路是否处于 Electrical Idle 状态之外，还可以通过其他方式推断出当前链路是否处于 Idle 状态。当 PCIe 链路处于不同的状态时，检测方法有所不同，如表 8-1 所示。值得注意的是，这种推断出的 Idle 状态并不等同于 Electrical Idle 状态。

表 8-1 接收逻辑 RX 推断当前链路是否处于 Idle 状态

<table><tr><td>链路状态</td><td>2.5 GT/s</td><td>5.0 GT/s</td></tr><tr><td>L0</td><td colspan="2">在128 μs之内,接收逻辑RX没有收到UpdateFC DLLP或者SKP序列时,链路处于Electrical Idle状态</td></tr><tr><td>Recovery. RcvrCfg</td><td colspan="2">在1280个UI之内,接收逻辑RX没有收到TS1或者TS2序列</td></tr><tr><td>Recovery. Speed Successful_speed_negotiation = 1</td><td colspan="2">在1280个UI内,接收逻辑RX没有收到TS1或者TS2序列</td></tr><tr><td>Recovery. Speed Successful_speed_negotiation = 0</td><td>在2000个UI之内,PCIe链路没有退出Electrical Idle状态°</td><td>在16000个UI之内,PCIe链路没有退出Electrical Idle状态</td></tr><tr><td>Loopback. active</td><td>在128 μs之内,PCIe链路没有退出Electrical Idle状态</td><td>N/A</td></tr></table>

PCIe总线通过上表判断而得出的Idle状态称为Logical Idle状态。在该表中出现的链路状态，如L0、Loopback.active等，将在下文详细解释。Electrical Idle状态是LTSSM状态机的基础。Electrical Idle状态是PCIe链路的相对静止状态，使用的功耗较低。当一个PCIe设备没有上电，处于复位状态或某些低功耗状态时，其使用的PCIe链路将处于Electrical Idle状态，读者需要进一步阅读下文的内容以详细了解Electrical Idle状态。

# 8.1.3 Receiver Detect 识别逻辑

Receiver Detect识别逻辑的主要作用是检测对端的接收逻辑RX是否正常工作，Receiver Detect识别逻辑是发送逻辑TX的一部分。PCIe链路在初始状态时，需要检测对端设备是否

存在才能进行链路训练。Receiver Detect识别逻辑的实现机理是通过检测对端设备接收逻辑的DC共模输入阻抗，来判断接收端是否存在。如果发送逻辑TX发现其负载的DC阻抗在 $\mathrm{Z}_{\mathrm{RX - DC}}$ 范围之内或者小于 $40\Omega$ 时，认为对端的接收逻辑RX存在。

PCIe总线规范定义了接收逻辑RX在正常工作状态下的DC共模输入阻抗 $\mathbf{Z}_{\mathrm{RX - DC}}$ ，其值在 $40\sim 60\Omega$ 范围之内。当接收逻辑RX的 $\mathrm{V_{cc}}$ 没有上电， $\mathrm{V_{D + }}$ 信号或者 $\mathrm{V_{D - }}$ 信号的电压伏值大于0时，其DC共模输入阻抗 $\mathrm{Z}_{\mathrm{RX - HIGH - IMP - DC - POS}}$ 最小为 $50~\mathrm{k}\Omega$ ；当 $\mathrm{V_{D + }}$ 或者 $\mathrm{V_{D - }}$ 小于0时，其DC共模输入阻抗 $\mathrm{Z}_{\mathrm{RX - HIGH - IMP - DC - NEG}}$ 最小为 $1.0\mathrm{k}\Omega$ 。

由此可见当对端接收逻辑RX可以正常工作时，其DC共模输入阻抗远小于“没有上电”时的状态。发送逻辑TX通过监控 $\mathrm{V}_{\mathrm{D}+}$ 和 $\mathrm{V}_{\mathrm{D}-}$ 信号的电压伏值，可以获得接收逻辑在正常工作状态和 $\mathrm{V}_{\mathrm{cc}}$ 没有上电时的电流曲线，从而判断接收逻辑RX是否处于正常工作状态。在PCIe总线中，发送逻辑TX通过“发送Detect序列”判断对端接收逻辑RX是否存在。

PCIe总线发送“Detect序列”的原理是首先提高 $\mathrm{V}_{\mathrm{D}+}$ 和 $\mathrm{V}_{\mathrm{D}-}$ 信号的电压伏值，然后通过判断对端接收逻辑RX的阻抗变化，识别对端的接收逻辑RX是否正常工作。其具体实现方法如下所示。

(1) 发送逻辑 TX 在提高 $\mathrm{V}_{\mathrm{D}+}$ 和 $\mathrm{V}_{\mathrm{D}-}$ 信号的电压伏值之前，需要保持 $\mathrm{V}_{\mathrm{D}+}$ 和 $\mathrm{V}_{\mathrm{D}-}$ 信号的伏值为一个恒定的 DC 共模电压值。  
(2）发送逻辑TX暂时提高 $\mathrm{V}_{\mathrm{D}+}$ 和 $\mathrm{V}_{\mathrm{D}-}$ 信号的电压，但是其值不能超过原来共模电压伏值加上 $\mathrm{V}_{\mathrm{TX-RCV-DETECT}}$ 。此时发送端将产生一个脉冲波形至接收逻辑RX。这个脉冲波形将穿越发送链路上的AC耦合电容，最后到达接收逻辑RX。因为接收逻辑RC的 $\mathbf{Z}_{\mathrm{RX-DC}}$ 较小，因此 $\mathrm{V}_{\mathrm{TX - RCV - DETECT}}$ 的值不能过大，否则将在接收逻辑RX上产生过大的电流，从而可能损坏接收逻辑RX。PCIe总线规定 $\mathrm{V}_{\mathrm{TX - RCV - DETECT}}$ 的最大值为 $600~\mathrm{mV}$   
(3) 发送逻辑 TX 根据 $\mathrm{V}_{\mathrm{D}+}$ 和 $\mathrm{V}_{\mathrm{D}-}$ 信号的脉冲波形通过接收逻辑 RX 时的电流曲线, 判断接收逻辑 RX 是否正常工作。如果通过这个电流曲线, 发现是 $\mathrm{Z}_{\mathrm{RX-DC}}$ 起作用, 此时电流强度的有效值较大, 表示接收逻辑 RX 正常工作; 如果发现是 $\mathrm{Z}_{\mathrm{RX-HIGH-IMP-DC-POS}}$ 起作用, 此时电流强度的有效值较小, 表示接收逻辑 RX 不存在或者没有被加电。

值得注意的是，在PCIe V2.x规范中，并没有强行规定必须在 $\mathrm{V_{D + }}$ 和 $\mathrm{V}_{\mathrm{D - }}$ 信号上都进行这种Receiver Detect测试。在有些实现上，可能仅使用 $\mathrm{V}_{\mathrm{D + }}$ 或者 $\mathrm{V}_{\mathrm{D - }}$ 信号进行这种Receiver Detect测试。在LTSSM状态机中，从Detect到Polling状态的切换时，需要使用Receiver Detect识别逻辑。

# 8.2 LTSSM状态机

PCIe总线在进行链路训练时，将使用LTSSM状态机。LTSSM状态机由“Detect”、“Pol-ling”、“Configuration”、“Disabled”、“Hot Reset”、“Loopback”、“L0”、“L0s”、“L1”、“L2”和“Recovery”共11个状态组成。这些状态分别与PCIe总线的链路训练、链路重训练、ASPM（Active State Power Management）和系统软件的电源管理相关。LTSSM状态机的转换逻辑如图8-5所示，而各个状态的含义如下所示。

![[pci_express/c60934777226c18156c247a1ed0217dda7b236109862906b3e14b00a0ba20e5a.jpg]]  
图8-5 LTSSM示意图

- Detect 状态。当 PCIe 链路被复位或者数据链路层通过填写某些寄存器后，LTSSM 将进入该状态。该状态也是 LTSSM 的初始状态。当 PCIe 链路处于该状态时，发送逻辑 TX 并不知道对端接收逻辑 RX 的存在，因此需要使用 Receiver Detect 识别逻辑判断对端接收逻辑 RX 是否可以正常工作，之后才能进入其他状态。  
- Polling 状态。当 PCIe 链路进入该状态时，将向对端发送 TS1 和 TS2 序列，并接收对端的 TS1 和 TS2 序列，以确定 Bit/Symbol Lock、Lane 的极性。PCIe 链路处于该状态时，将进行 Loopback 测试，确定当前使用的 PCIe 链路可以正常工作。  
- Configuration 状态。当 PCIe 链路进入该状态时，将确定 PCIe 链路的宽度、Link Number、Lane reversal、Polarity inversion 和 Lane-to-Lane 的延时。该状态是 LTSSM 状态机最重要的状态。值得注意的是 PCIe 链路在进行初始化时，链路两端统一使用 2.5GT/s 的数据传送率，直到进入 L0 状态。  
- L0 状态。PCIe 链路的正常工作状态。PCIe 链路可以正常发送和接收 TLP、DLLP 和 PLP。PCIe 链路可以从该状态进入 Recovery 状态，以改变数据传送率。  
- Recovery 状态。PCIe 链路需要进行链路重训练时需要进入该状态。该状态是 LTSSM 状态机的重要状态。  
- L0s、L1 和 L2 状态。PCIe 链路的低功耗状态，其中 PCIe 链路处于 L0s 状态时，使用的功耗相对较高，而处于 L2 状态时，使用的功耗最低。  
- Disabled 状态。系统软件可以通过设置寄存器，使 PCIe 链路进入 Disabled 状态。当 PCIe 链路的对端设备被拔出时，LTSSM 也需要进入该状态。  
- Loopback 状态。PCIe 链路进入该状态时，发送端口将转发其接收端口接收到的数据，PCIe 测试仪器可以利用该状态进行数据测试。  
- Hot Reset 状态。当处理器系统进行 Hot Reset 操作时, PCIe 链路将进入 Recovery 状

态，然后进入Hot Reset状态进行PCIe链路的重训练。

LTSSM 的各个状态将在下文详细介绍，本章将重点介绍 Detect、Polling、Configuration、L0 和 Recovery 状态，囿于篇幅并不对 Disabled、Loopback 和 Hot Reset 状态进行详细说明，对这些状态有兴趣的读者可以参阅 PCIe 总线规范，以获得进一步信息。

PCIe设备的物理层进行复位后，LTSSM状态机首先沿着“Detect” $\rightarrow$ “Polling” $\rightarrow$ “Configuration” $\rightarrow$ “L0”的路径进入到正常工作状态“L0”，这也是链路训练的正常工作路径，也是最重要的一条路径。

物理层链路训练所涉及的内容非常复杂。为便于读者理解，本书仅介绍其主要工作流程，即PCIe设备的正常工作路径，以帮助读者掌握链路训练的概要，而不会介绍异常处理和一些并不重要的分支。

# 8.2.1 Detect状态

如图8-6所示，Detect状态由Detect.Quiet、Detect.Active两个子状态组成。该状态的主要功能是检测PCIe链路上是否有PCIe设备存在，如果存在，一共使用了多少可用的Lane资源。在正常情况下，PCIe链路将从Detect状态迁移到Polling状态。

![[pci_express/4f760801339e0d95d268e79261b8f0d11b8a0006c9abaffd7920271a7509a940.jpg]]  
图8-6 Detect状态机

当PCIe设备进行传统复位操作后，首先进入Detect.Quiet状态。在多数情况下，如果该PCIe设备的对端存在设备时，PCIe链路的两端将同时进入Detect.Quiet状态。因为PCIe链路的两端可能同时进行复位操作。

在PCIe设备进入Detect.Quiet状态时，其发送逻辑TX处于“Electrical Idle”状态，此时该设备发送链路的 $\mathrm{D}+$ 和D-信号的电压为DC共模电压，且为相同的值，此时发送逻辑TX使用的功耗最低。

值得注意的是，物理层也可以从L2、Loopback、Disabled、Polling、Configuration和Recovery状态进入Detect. Quiet状态，此时发送逻辑TX需要发送必要的Idle序列，通知对端设备的接收逻辑RX，然后经过一段延时后才能进入“Electrical Idle”状态。而在Detect状态中，PCIe设备的发送逻辑TX将直接进入到“Electrical Idle”状态，并不会使用Idle序列通知对端设备的接收逻辑RX。

当PCIe设备处于Detect.Quiet状态时，缺省使用2.5Gb/s的数据传送率，即PCIeV1.x规定的数据传送率，并置LinkUp、upconfigure\_capable等状态位为0，此时数据链路层处于DL\_Inactive状态。整个PCIe链路处于“完全静止”的状态。

当PCIe设备处于Detect.Quiet状态超过12ms之后，或者检测到PCIe链路上的任何一个

Lane退出“Electrical Idle”状态时，PCIe设备将进入Detect.Active状态。

PCIe 设备进入 Detect.Active 状态后，其发送逻辑 TX 将向该链路的所有“未配置过的 Lane”端发送“Receiver Detection 序列”，检测其对端的接收逻辑 RX 是否正常工作。如果所有 Lane 的接收逻辑 RX 都在正常工作时，PCIe 设备进入到 Polling 状态。

如果没有一个Lane的接收逻辑RX被检测到，PCIe设备将进入到Detect.Quiet状态，此时可能PCIe链路的对端没有连接PCIe设备，或者该PCIe设备并没有正常工作。

如果仅有部分Lane正确检测到对端接收逻辑RX的存在，物理层将首先等待 $12\mathrm{ms}$ ，然后使用该链路的所有“未检测成功过的Lane”向对端重新发送“Receiver Detection序列”，进一步识别可用的Lane。如果这次的检测结果与第一次检测结果相同，物理层将这些“不可用”的Lane置为Electrical Idle状态，并进入Polling状态，如果两次结果不相同，则进入Detect.Quiet状态。

这些被标识为“Electrical Idle”状态的Lane将不会被LTSSM状态机继续使用。有时PCIe链路的两端虽然都连接PCIe设备，但是这些设备不一定利用了PCIe链路上所有的Lane，下文将举例说明Detect.Active状态的运行机制。

假设一个PCIe链路由8个Lane组成，其上游端口与一个Switch连接，而下游端口连接一个EP，如果这个EP仅使用了2个Lane。那么第一次PCIe链路检测结果为6个Lane没有与EP进行连接，而经过12ms再次进行检测时，可能还检测到有6个Lane没有与EP进行连接，此时该PCIe链路认为EP仅使用了2个Lane。如果第2次检查发现有7个或者5个Lane没有与EP进行连接，说明两次检测结果不一致，此时PCIe设备将要退回到Detect.Quiet状态，并择时重新进入Detect.Active状态，重新进行链路探测。

Detect 状态是 PCIe 设备进入的第一个状态，在这个状态中，PCIe 设备需要识别 PCIe 链路的拓扑结构。在这个状态中，物理层使用的检测手段都是基于 PCIe 链路的物理特性，只有 PCIe 设备发现 PCIe 链路的对端具有合法设备后，才能进入 Polling 状态。

