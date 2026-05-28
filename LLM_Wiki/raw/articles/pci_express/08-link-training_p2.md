---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "08"
section: "8.2.2 Polling状态"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 8.2.2 Polling状态

当PCIe设备在Detect状态中，识别完毕当前链路上可用的Lane资源之后，将进入Polling状态，Polling状态由Polling.Active、Polling.Compliance和Polling.Configuration子状态组成。PCIe设备可以从Polling状态进入到Configuration状态，继续进行链路训练，如果PCIe设备在Polling状态中出现某种错误时，将退回到Detect状态，重新进行PCIe链路的训练，Polling状态机的转换逻辑如图8-7所示。

如该图所示，PCIe设备将首先进入Polling.Active状态，然后进入Polling.Configuration状态，最后退出到Configuration状态。

PCIe 设备处于 Polling.Active 状态时，首先检查 Link Control 2 寄存器的“Enter Compliance Bit”位。如果该位为 1，PCIe 设备将进入 Polling.Compliance 状态，值得注意的是，PCIe 链路两端设备的“Enter Compliance Bit”需要被同时置 1，即 PCIe 链路两端的设备需要同时进入到 Polling.Compliance 状态。本节对 Polling.Compliance 状态不做进一步描述。

![[pci_express/7716660bb64265189ffb6be6a554bbc498182688ceb350d3191dc5968b54de10.jpg]]  
图8-7 Polling状态机

如果“Enter Compliance Bit”不为1，则PCIe链路两端设备的发送逻辑TX需要向对端至少发送1024个TS1序列，其中TS1序列的Lane/Link Number必须为“PAD”，即不设置Lane/Link Number。PCIe设备使用这些TS1序列，获得Bit/Symbol Lock，这个过程大约需要 $64\mu \mathrm{s}$ 。值得注意的是，PCIe链路两端设备退出Detect状态时，可能并不完全同步，因此两端设备交换TS1序列的过程也并不完全同步。

发送逻辑 TX 在发送 TS1 序列之前，需要保证 $\mathrm{D}+$ 和 $\mathrm{D}-$ 信号的 DC 共模电压恢复到正常工作值。因为发送逻辑 TX 在 Detect 状态进行“Receiver Detect”的过程中，曾经将 DC 共模电压提高了 $\mathrm{V}_{\mathrm{TX-RCV-DETECT}}$ 。

PCIe 设备在发送 1024 个 TS1 序列的同时，如果其接收逻辑 RX 从全部“已被正确识别的 Lane”中收到了以下任意一种 8 个连续的报文序列后，该 PCIe 设备将进入 Polling. Configuration 状态。

(1) TS1 序列③，其 Lane/Link Number 为 PAD，而“Compliance Receive”位为 0。  
(2) TS1 序列, 其 Lane/Link Number 为 PAD, 而 “Loopback” 位为 $1^{\text{圆}}$ 。值得注意的是, 发送逻辑 TX 发送 “Loopback” 位为 1 的 TS1 序列后, PCIe 链路对端设备的接收逻辑 RX 将收到该序列, 并将这个 TS1 序列使用内部 Loopback 逻辑直接回送给对端设备 (并不是该设备重新生成的 TS1 序列), 之后对端设备的接收逻辑 RX 将接收到之前发送逻辑 TX 发送的 TS1 序列。  
(3) TS2 序列, 其 Lane/Link Number 为 PAD。PCIe 链路两端设备进入的 LTSSM 状态机并不一定同步, 可能对端 PCIe 设备可能已经进入了 Polling. Configuration 状态, 此时该设备将向对端发送 TS2 序列, 详见下文。

如果上述条件没有成立，PCIe设备在经过 $20\mathrm{ms}$ 延时后，判断下列条件。如果这些条件同时成立时，PCIe设备也将进入Polling. Configuration状态。

(1) 任何一个“已被正确识别的 Lane”收到了8个连续的TS1序列，其中Lane/Link Number为PAD，而“Compliance Receive”位为0或者“Loopback”位为1；或者收到8个连续的TS2序列。而且在收到第一个TS1序列之前，发送逻辑TX至少已经发送出1024个TS1序列。  
(2) PCIe 设备从 Electrical Idle 状态中退出，并进入到 Polling. Active 状态时，所有“已被正确识别的 Lane”至少有一个 Lane 检测到对端设备。

如果该条件也没有成立，PCIe设备将可能进入Polling.Compliance状态。如果PCIe链路上任何一个Lane的发送链路上连接了一个“对地阻抗为 $50\Omega$ ”的电阻后，PCIe设备也将强制进入Polling.Compliance状态。该状态的主要作用是对PCIe链路进行检测，本节对此不做进一步描述。

当PCIe设备进入Polling.Configuration状态时，物理层首先处理所有“已识别的”Lane中，是否存在极性翻转（Lane Polarity Inversion）的现象，之后进行以下操作。

（1）置Link Control2寄存器的“Transmit Margin”字段为0b000。  
(2) 向对端“已识别的Lane”连续发送TS2序列，其中Link/Lane Number为PAD，而Loopback位为0。  
(3) 当收到 8 个连续的 TS2 序列, 而且一共收到 16 个 TS2 序列后, PCIe 设备将进入 Configuration 状态, 否则经过 $48\mathrm{ms}$ 延时后进入 Detect 状态。

当PCIe链路两端设备收齐TS2序列后，将基本同步地进入Configuration状态。从这个角度来说，TS2序列是为了同步“异步发送的TS1序列”。

在 Polling 状态机中，还有一个 Polling.Speed 子状态，该状态的主要作用是调整 PCIe 链路使用的数据传送率。当一个 PCIe 链路两端的设备可以支持高于 2.5GT/s 的数据传送率时，可以首先进入该状态，改变 PCIe 链路的数据传送率。

在许多PCIe设备的具体实现中，并没有使用Polling.Speed子状态。此时在PCIe链路的训练过程中，将缺省使用2.5GT/s的数据传送率。此时LTSSM状态机将首先沿着Detect $\rightarrow$ Polling $\rightarrow$ Configuration $\rightarrow$ L0的路径进入L0状态，并使用2.5GT/s的数据传送率，即便PCIe链路两端的设备都支持更高的数据传送率。

当PCIe设备改变数据传送率时，需要在L0状态，通过向对端设备发送TS1序列（其speed\_change位为1），使PCIe链路两端设备进入Recovery状态后，才能改变缺省使用的数据传送率。

# 8.2.3 Configuration状态

Configuratoin状态是LTSSM的重要状态，该状态完成PCIe链路的主要配置工作，包括Link Number和Lane Number的协商，并使PCIe链路进入正常工作状态L0。如图8-8所示，Configuration状态由多个子状态组成。

其中Configuration.Linkwidth.Start和Configuration.Linkwidth.Accept状态判断当前PCIe链路的有效宽度；Configuration.Lanenum.Wait和Configuration.Lanenum.Accept状态判断当前

PCIe链路的物理“Lane Number”与逻辑“Lane Number”的对应关系；而Configuration.Complete状态负责处理“Lane-to-Lane de-skew”。由上文所述，一条PCIe链路可能由多个Lane组成，而使用这几个Lane进行数据传递时，可能存在速度差异，即“Skew”，“Lane-to-Lane de-skew”就是消除这个速度差异的方法。

![[pci_express/6e97bc577a780341091058a427ecf907a7e9291c75c1760b289e418179e66e62.jpg]]  
图8-8 Configuration状态机

在LTSSM状态机中，Polling状态正常结束时将进入Configuration状态。此外当Recovery状态出现某些错误，没有正常进入L0状态时，也可能首先进入Configuration状态，然后经过错误处理之后，重新返回L0状态。

在PCIe总线中，LTSSM状态机从Polling状态进入Configuration状态时Linkup位为0，因为对应Lane不曾被激活；而从Recovery状态进入该状态时Linkup位为1。进入Configuration状态时，PCIe链路上游端口（包括RC端口或者Switch的下游端口）Link Status寄存器的Link Training位被硬件置1，从该状态进入LO状态时，该位被清零。

# 1. Link Number 的协商过程

PCIe总线使用自协商的方法，确认不同PCIe链路的Link Number。下文以一个具有两个端口的Switch为例，简要说明Link Number的协商机制，多端口RC的Link Number协商机制与此相似。该Swith的与EP的连接方法如图8-9所示。

![[pci_express/25c13c742c5721307a0149c4734332dfcb49094226a195720003173e9e8234f2.jpg]]  
图8-9 Link Number的协商过程

在该Switch中共有8个Lane，并且可以分解为2个PCIe链路，每个链路的最大宽度为4，即含有4个Lane，这4个Lane并不能自由组合形成两个PCIe链路，而是第 $0\sim 3$ 个Lane组成一个PCIe链路，而第 $4\sim 7$ 个Lane组成另一个PCIe链路。其中每一个PCIe链路可以独立使用，连接不同的EP，这些EP最多可以使用4个Lane。当然这8个Lane也可以合并成一个PCIe链路，与一个EP连接。

Switch 不会预知其下游链路与 EP 的连接拓扑结构。因此在 Configuration Linkwidth 状态时，Switch 将向第 0\~3 个 Lane 发送 TS1 序列，其 Link Number 为 N；而向第 4\~7 个 Lane 发送 TS1 序列，其 Link Number 为 $\mathrm{N} + 1$ 。如图 8-1 所示，Device A 收到的 TS1 序列，其 Link Number 为 N，而 Device B 收到的 TS1 序列，其 Link Number 为 $\mathrm{N} + 1$ 。Device A 或者 B 收到这些 TS1 序列后，将向 Switch 发送 TS1 序列，其 Link Number 为 N 或者 $\mathrm{N} + 1$ 。

而Device C收到的TS1序列，其Link Number为N或者 $\mathrm{N} + 1$ 。值得注意的是，此时Device C将选择唯一的Link Number“或者为N或者为 $\mathrm{N} + 1$ ”回传给Switch，而并不是将“N和 $\mathrm{N} + 1$ ”都回传给Switch。

Switch 根据收到的 TS1 序列判断其链路的连接拓扑结构，如果 Switch 收到的 TS1 序列中包含两个不同的 Link Number，则表示 8 个 Lane 被分解为两个 PCIe 链路与两个 PCIe 设备相连；如果只有一个 Link Number，则表示 8 个 Lane 被合并为 1 个 PCIe 链路与 1 个 PCIe 设备相连。如果这个 Switch 支持 4 路 PCIe 链路，则 Switch 需要向这 4 个 PCIe 链路发送的 Link Number 为 $\mathrm{N} \sim \mathrm{N} + 3$ ，共 4 种 TS1 序列；如果 Switch 支持 8 路 PCIe 链路，则 Switch 需要向这 8 个 PCIe 链路发送的 Link Number 为 $\mathrm{N} \sim \mathrm{N} + 7$ ，共 8 种 TS1 序列。

PCIe总线使用自协商的方法识别下游链路的拓扑结构。PCIe总线在Configuration状态中，使用Configuration.Linkwidth.Start和Configuration.Linkwidth.Accept两个子状态，进行PCIe链路两端设备LinkNumber的协商，以确定下游链路与PCIe设备的连接拓扑结构。

当PCIe链路进入Configuration.Linkwidth.Start状态时，RC端口或者Switch的下游端口将向其下游链路发送TS1序列以确定PCIe链路的LinkNumber，下游设备也将会回送TS1序列。上文以连接2个EP的Switch为例，说明LinkNumber的自协商过程。RC端口与其下游设备LinkNumber的协商过程与此类似，本节对此不做进一步说明。

当PCIe链路进入Configuration状态时，如果Linkup位为0时，Switch需要向下游链路的每一个Lane都发送若干个TS1序列，在这个TS1序列中的Link Number字段分别为0和 $1^{\text{念}}$ ，而LaneNumber字段为PAD。这里使用的Link Number由Switch内部由硬件逻辑进行编号，其编码在 $0\sim 255$ 之间，本节使用0和1作为Link Number。注意在这个TS1序列中，Loopback位不再为1，否则对端设备将接收到的TS1序列直接Loopback，回送给发送端。

值得注意的是，如果 Linkup 或者 upconfigure\_capable 位为 1 时，即从 Recovery 状态进入该状态时，TS1 序列中的 Link Number 字段为 PAD（K23.7），此时的处理方法与从 Polling 状态进入该状态有些区别。本节对这种情况不做进一步描述。

Switch 通过发送部件 TX，经过其下游链路发送完毕这些 TS1 序列后，开始监控其接收

部件 RX。如果接收部件 RX 的任何一个 Lane 收到至少 1 个来自下游设备的 TS1 序列后（该序列的 Link Number 和 Lane Number 字段都为 PAD），需要进一步判断是否任何一个 Lane 收到 2 个连续的 TS1 序列（该序列的 Link Number 为 0 或者 1，即与发送的 TS1 序列 Link Number 相同，而 Lane Number 为 PAD）。上述检测成功之后，Switch 的下游端口将进入 Configuration.Linkwidth.Accept 状态。

而与Switch连接的下游设备在进入Configuration.Linkwidth.Start状态时，首先向其上游链路发送若干个TS1序列（该序列的Link和Lane Number字段都为PAD），这也是Switch的下游端口首先收到这个TS1序列（Link和Lane Number都为PAD）的原因。

之后下游设备等待来自Switch的TS1序列，下游设备可能收到TS1序列中的Link Number为0或者1（图8-1中的Device C可能收到这样的TS1序列），但是下游设备仅使用其中一个Link Number（PCIe总线并没有规定是使用0还是1）组成若干个TS1序列，并通过其上游链路发送给Switch，这个TS1序列的Lane Number为PAD。

随后下游设备也进入Configuration.Linkwidth.Accept状态，至此在这条PCIe链路上的两个设备都将进入Configuration.Linkwidth.Accept状态。

在Configuration.Linkwidth.Accept状态时，Switch的下游端口将分析接收到的TS1序列，并将所有Link Number相同的Lane组合在一起，通过此步骤，Switch的下游端口可以确定下游链路的连接拓扑结构，即下游链路由哪些Lane组成。对于图8-1所示的实例，DeviceA使用Switch下游链路的Lane $0\sim 3$ ，DeviceB使用Switch下游链路的Lane $4\sim 7$ ，而DeviceC使用Switch下游链路的Lane $0\sim 7$ 。

Switch 在确定了连接拓扑结构后，将向 PCIe 链路的所有 Lane 发送若干个 TS1 序列，之后进入 Configuration.Lanenum.Wait 状态。值得注意的是，在这个 TS1 序列中 Link Number 为确定的值，而 Lane Number 在 $0 \sim n - 1$ 之间，此时使用的 Lane Number 为逻辑号。在图 8-1 中，虽然 Device B 使用 Switch 的物理 Lane 为 $4 \sim 7$ ，但是发向物理 Lane $4 \sim 7$ 的 TS1 序列的 Lane Number 为 $0 \sim 3$ 。

在进入Configuration.Linkwidth.Accept状态时，下游设备等待Switch端口的TS1序列。如果下游设备收到2个连续的TS1序列，其Link Number等于在Configuration.Linkwidth.Start状态发送给Switch的Link Number，而且Lane Number不为PAD时，下游设备将向Switch发送若干个TS1序列，之后下游设备进入Configuration.Lanenum.Wait状态。

该序列的 Link Number 为接收到的值，而 Lane Number 在 $0 \sim m - 1^{\ominus}$ 之间。如果下游设备没有使用错序连接方式，那么下游设备发送的 Lane Number 将与 Switch 的 Lane Number 直接对应；否则错序对应。本节虽然使用了一定篇幅讲述 PCIe 总线 Link Number 的自协商过程，但也仅说明了该过程的一个子集。PCIe 总线的自协商过程较为复杂，相对较难实现。

为此PLX公司采用了另外一种Link Number的协商机制。下面以PEX8518芯片为例说明这种协商机制，该芯片并没有采用PCIe总线规定的协商机制，而是使用寄存器配置其下

游端口的使用情况。

PEX8518 芯片的下游链路由 16 个 Lane 组成（分别为 $0 \sim 15$ ），最多可以支持 5 个端口，分别为 Port0\~4。这 5 个端口能够使用的 Lane 由 Port Configuration 寄存器规定，而不是通过 Configuration 状态自适应检测其下游链路的拓扑结构。该寄存器的值与其下游端口的对应关系如表 8-2 所示。

表 8-2 Port Configuration 寄存器与下游端口的对应关系

<table><tr><td rowspan="2">Port Configuration寄存器的值</td><td colspan="5">链路宽度</td></tr><tr><td>端口0</td><td>端口1</td><td>端口2</td><td>端口3</td><td>端口4</td></tr><tr><td>0x00</td><td>×4 (0~3)</td><td>×4 (4~7)</td><td>×4 (8~11)</td><td>×4 (12~15)</td><td>不使用</td></tr><tr><td>0x02</td><td>×8 (0~7)</td><td>×8 (8~15)</td><td>不使用</td><td>不使用</td><td>不使用</td></tr><tr><td>0x03</td><td>×8 (0~7)</td><td>×4 (8~11)</td><td>×4 (12~15)</td><td>不使用</td><td>不使用</td></tr><tr><td>0x04</td><td>×8 (0~7)</td><td>×4 (8~11)</td><td>×2 (12~13)</td><td>×2 (14~15)</td><td>不使用</td></tr><tr><td>0x05</td><td>×8 (0~7)</td><td>×2 (8~9)</td><td>×2 (10~11)</td><td>×4 (12~15)</td><td>不使用</td></tr><tr><td>0x06</td><td>×8 (0~7)</td><td>×2 (8~9)</td><td>×4 (10~13)</td><td>×2 (14~15)</td><td>不使用</td></tr><tr><td>0x08</td><td>×8 (0~7)</td><td>×2 (8~9)</td><td>×2 (10~11)</td><td>×2 (12~13)</td><td>×2 (14~15)</td></tr><tr><td>0x09</td><td>×4 (0~3)</td><td>×4 (4~7)</td><td>×4 (8~11)</td><td>×2 (12~13)</td><td>×2 (14~15)</td></tr></table>

由上表所示，PEX8518芯片使用静态表进行Link Number的确认，而没有使用PCIe总线规定的自协商机制，从而在保证配置灵活性的同时，极大简化了Link Number的协商难度。但是使用这种方法不适用于“下游链路拓扑结构不可预知”的应用。

因此在设计中，尽量避免使用PEX8518芯片连接一个PCIe插槽，因为这个插槽上的PCIe设备使用的Lane并不确定，使用静态分配Lane的方法并不合适，但是这并不妨碍PEX8518芯片在嵌入式领域，尤其在电信领域的大规模应用。

# 2. Lane Number 的协商过程

物理层在确认PCIe链路的Link Number之后，开始进行Lane Number的协商。PCIe总线使用Configuration.Lanenum.Wait和Configuration.Lanenum.Accept两个子状态完成Lane Number的协商。下面仍然以图8-9为例说明Lane Number的协商过程。

与 Link Number 的协商过程相比，Lane Number 的协商过程较为简单。本小节以一个实例说明 Lane Number 的实现过程，而并不深究 LTSSM 状态机的详细迁移过程。

如图8-1所示，PCIe链路允许错序连接，因此Switch的下游端口与下游设备的上游端口使用的Lane Number并不一定完全一致，因此物理层需要进行Lane Number的协商。当Switch与Device A完成Link Number的协商后，将进行Lane Number的协商。

首先 Switch 向 Device A 的 4 个 Lane 发送 TS1 序列，在这个 TS1 序列中的 Link Number 字段为 0，而 Lane Number 字段分别为 0、1、2 和 3。

当Device A收到这些TS1序列后，也将向Switch回送使用TS1序列，在这个TS1序列中的Link Number字段为0，而Lane Number字段为Device A的物理Lane Number号。如果Switch与Device A使用图8-1左图所示的错序连接方法时，Device A回送的TS1序列的Link Number字段为3、2、1和0，如果使用图8-1中图所示的连接方法时，Device A回送的TS1序列为0、1、2和3。

当 Switch 收到 Device A 的回送 TS1 序列后，可以获得 PCIe 链路的连接信息，从而确定

当前Lane的连接拓扑结构，并完成逻辑Lane Number与物理Lane Number的对应关系。当使用图8-1左图所示的错序连接方法时，Switch的逻辑Lane Number $0\sim 3$ 与DeviceA的物理Lane Number $3\sim 0$ 对应。

Switch 和 Device A 都可以处理 PCIe 链路的错序连接，本节所述的实例是 Switch 支持 PCIe 链路的错序连接而 Device A 不支持这种错序连接，此时 Device A 不进行物理 Lane Number 和逻辑 Lane Number 的转换，对于 Device A 而言，物理 Lane Number 等于逻辑 Lane Number。

当Switch的下游端口和对端设备的上游端口离开Configuration.Lanenum.Accept状态后，将进入Configuration.Complete状态进行Link Number和Lane Number的确认。

# 3. Link Number 与 Lane Number 的确认

PCIe总线使用Configuration.Complete和Configuration.Idle状态进行Link Number与Lane Number的确认。在Configuration.Complete状态中，PCIe链路的两端设备将使用TS2序列进行Link与Lane Number的确认。在这个TS2序列中的Link Number字段为之前确定的值，而Lane Number字段也为已经确认的号码。

当链路两端的设备收到这些TS2序列后，将进一步消除PCIe链路不同Lane的漂移（De-skew），并设置合理的N\_FTS的值，还有一个重要的操作是记录当前PCIe设备能够支持的数据传送率，然后进入Configuration.Idle状态。

PCIe设备处于Configuration.Idle状态时，PCIe链路已经设置完毕，此时将向对端至少发送16个Idle序列，当接收逻辑RX收到这些Idle序列后，将置LinkUp状态位为0b1，PCIe数据链路层将从DL\_Inactive状态迁移到DL\_Init状态，而物理层将进入正常工作状态L0。

# 8.2.4 Recovery状态

Recovery状态是LTSSM状态机的重要状态，其复杂程度超过Configuration状态。该状态可以从L0、L1和L0s状态进入，当PCIe设备进入低功耗状态，需要进行链路重训练时，将经过Recovery状态之后，才能重新进入正常工作状态，第8.3节将讲述如何从L1和L0s状态进入Recovery状态。

Recovery 状态机如图 8-10 所示，该状态机由 Recovery.RcvrLock、Recovery.Speed、Recovery.RcvrCfg 和 Recovery.Idle 共四个子状态组成。

# 1. 从 L0 状态进入到 Recovery 状态

当PCIe设备工作在L0状态时，出现以下情况时，将进入Recovery状态。

# （1）更改数据传送率

PCIe 设备需要改变数据传送率时，将进入 Recovery 状态。在 Configuration 状态中，PCIe 链路两端设备记录了“当前 PCIe 设备能够支持的数据传送率”，如果两端设备都支持大于 2.5GT/s 的数据传送率，可以从 L0 状态进入 Recovery 状态。

在PCIe设备中存在两个状态位。其中directed\_speed\_change位为1时，表示PCIe设备

希望更改数据传送率，而 changed\_speed\_recovery 位为 1 时表示 PCIe 链路已经完成数据传送率的更改。

![[pci_express/c4a57b92880d15b32d28dc2564f1c97733e1dc147ab1a4e871811a8440f97089.jpg]]  
图8-10Recovery状态机

当数据链路层处于DL\_Active状态时，系统软件可以置directed\_speed\_change位为1，同时复位changed\_speed\_recovery位为0，随后该设备将进入Recovery状态。值得注意的是PCIe链路的两端设备需要同时改变directed\_speed\_change和changed\_speed\_recovery位。

# (2）更改链路宽度

PCIe 设备在 Configuration 状态时，如果设置 upconfigure\_capable 状态位为 1，可以更改链路宽度。此时该设备需要从 L0 状态进入 Recovery 状态，之后才能进行该操作。

(3) 已配置完毕的 Lane 中收到 TS1 或者 TS2 序列。  
（4）检测或者推断出对端设备处于Idle状态。

当PCIe设备检测到对端处于Idle状态时，将有可能进入Recovery状态。此处的Idle状态由两部分内容组成，一个通过硬件检测对端设备的发送逻辑TX是否处于“Electrical Idle”状态；另一个通过逻辑推断对端设备的发送逻辑是否处于Logical Idle状态。

在正常情况下，发送逻辑 TX 在进入 Electrical Idle 状态时，需要发送 EIOS 序列。如果对端的接收逻辑 RX 检测到 PCIe 链路实际上已经进入 Idle 状态，但是并没有收到 EIOS 序列时，将认为 PCIe 链路出现某种故障，PCIe 总线并没有规定在这种情况下，PCIe 设备是进入 Recovery 状态还是继续保持在 L0 状态中，但是在多数实现中，都将进入 Recovery 状态。

值得注意的是，一个PCIe设备进入Recovery状态之后，将向对端发送TS1序列，而对端设备收到这个TS1序列后，也将从L0状态进入Recovery状态。因此在PCIe链路中，当一端设备进入Recovery状态后，对端设备也将进入该状态。

# 2. Recovery.RcvrLock 状态

PCIe 设备进入 Recovery 状态时，将首先到达 Recovery.RcvrLock 状态。PCIe 设备进入该状态时将连续向对端“已配置完毕的 Lane”发送 TS1 序列，该序列的 Link 和 Lane Number 为之前在 Configuration 状态中设置的值。

如果directed\_speed\_change位为1时，发送的TS1序列的speed\_change位也将设置为1。PCIe设备如果在Recovery.RcvrLock状态时，收到speed\_change位为1的TS1序列时，该设

备的directed\_speed\_change位也将改变为1。

下文以一个实例说明 speed\_change 位和 directed\_speed\_change 位的作用。假设在一条 PCIe 链路上连接了两个设备，EP A 和 Switch 的下游端口 B，而且这两个设备的工作状态的初始值为 L0。

如果EPA希望进入Recovery状态改变数据传送率，则置directed\_speed\_change位为1，随后进入Recovery.RcvLock状态，同时向端口B发送speed\_change位为1的TS1序列。

而当端口B处于L0状态时，收到这个TS1序列后，并不会检测speed\_change位，而直接进入Recovery状态。然后向EP A发送若干个speed\_change位为0的TS1序列，因为此时端口B的directed\_speed\_change位为0。

当端口B收到“8个连续”的从EPA发来的“speed\_change位为1”的TS1序列（还包括EPA支持的数据传送率种类）之后，directed\_speed\_change位将置为1，然后再向EPA发送“speed\_change位为1”的TS1序列。值得注意的是，在上述实例中，端口B在Recovery.RcvLock状态将发送两种不同的TS1序列，一种序列的speed\_change位为0，而另一种序列的speed\_change位为1。

PCIe 设备连续发送 TS1 序列的同时，将通过其接收逻辑 RX 检测，是否收到 8 个连续的 TS1 或者 TS2 序列，是否这些序列使用的 Link 和 Lane Number 与该设备发送的值相同，而且这些序列的 speed\_change 位是否与 directed\_speed\_change 位相同。如果检测成功，该设备将进入 Recovery.RcvrCfg 状态。在该状态中，PCIe 设备将重新获得 Bit/Symbol Lock，并处理不同 Lane 之间的 Skew，这也是该状态位被命名为“Lock”的原因。

PCIe 设备还可以从该状态直接进入 Recovery.Speed BFQ 状态, 如果当前 PCIe 链路已经工作在大于 2.5GT/s 的数据传送率, 但是并不能稳定工作 (并没有正确获得 Bit/Symbol Lock), 此时需要进入 Recovery.Speed 状态, 将数据传送率更改为 2.5GT/s。PCIe 设备还可以从该状态进入 Configuration 或者 Detect 状态。本节对此不做详细介绍。

# 3. Recovery.RcvrCfg状态

PCIe设备进入Recovery.RcvrCfg状态后，将向对端发送TS2序列，该序列的Link和Lane Number为之前在Configuration状态设置的值。如果directed\_speed\_change位为1时，该序列的speed\_change位也必须为1。发送这些TS2序列的主要目的是进一步确认Recovery.RcvrLock状态使用TS1序列所获得的结果。

之后PCIe设备通过其接收逻辑RX检测，是否收到了8个连续的TS2序列，这些序列使用的Link和Lane Number与该设备发送的值是否相同，而且这些序列的speed\_change位是否与directed\_speed\_change位相同。

如果检测成功，PCIe设备将根据TS2序列中的speed\_change位决定进入Recovery.Speed状态，还是Recovery.Idle状态。

如果该PCIe设备能够支持的数据传送率与接收的TS2序列中的数据传送率重合，比如都支持5GT/s的数据传送率，而且speed\_change位为1时，将进入Recovery.Speed状态，PCIe设备从Recovery.RcvrCfg状态迁移到Recovery.Speed状态还有一个补充条件，即接收到第一个TS2序列后，至少向对端发送32个连续的TS2序列。

如果Speed\_change位为0时，将进入Recovery.Idle状态，PCIe设备从Recovery.RcvrCfg状态迁移到Recovery.Idle状态还有一个补充条件，即从接收到第一个TS2序列后，向对端发

送16个连续的TS2序列。

如果在Recovery.RcvrLock状态中，PCIe设备没有处理不同Lane之间的Skew，在该状态中，PCIe设备可以处理不同Lane之间的Skew。PCIe设备还可以从该状态进入Configuration或者Detect状态。本节对此不做详细介绍。

# 4. Recovery.Speed 状态

PCIe 设备进入该状态时，如果使用的数据传送率为 $2.5\mathrm{GT/s}$ ，则其发送逻辑 TX 需要发送 1 个 EIOS 序列，然后进入 Electrical Idle 状态；如果使用的数据传送率为 $5.0\mathrm{GT/s}$ ，则其发送逻辑 TX 需要发送 2 个 EIOS 序列，然后进入 Electrical Idle 状态。此时该 PCIe 设备需要等待其接收逻辑 RX 也进入 Electrical Idle 状态，才能进一步工作。接收逻辑检测对端发送逻辑 TX 是否处于 Electrical Idle 状态，使用的方法如第 8.1.2 节所示。

当PCIe设备的发送与接收逻辑都进入到Electrical Idle状态后，至少需要等待800ns判断PCIe链路两端是否成功完成数据传送率的协商，或者等待1ms判断协商是否成功。协商成功后successful\_speed\_negotiation位被置为1，否则将置0。

如果协商成功，PCIe设备将置directed\_speed\_change位为0，然后从该状态回到Recovery.RcvrLock状态。值得注意的是在Recovery.Speed状态并不发送PLP检查新的数据传送率是否能够使用。而在Recovery.RcvrLock状态中，通过TS1序列判断对端设备是否能够获得Bit/SymbolLock，确定新的数据传送率是否正常工作。如果不能正常工作，该设备还将从Recovery.RcvrLock状态回到Recovery.Speed状态，将数据传送率降低为2.5GT/s后，再次回到Recovery.RcvrLock状态。

# 5. Recovery.Idle 状态

PCIe链路进入Recovery.Idle状态后，将连续向对端发送Idle序列，当接收逻辑RX的所有可用Lane收到8个连续的Idle序列，而且发送逻辑TX发送了16个Idle序列后，将进入L0状态，完成链路训练或者重训练过程；否则将视情况进入Disabled、Loopback、HotReset、Configuration和Detect状态，本节对Recovery.Idle进入这些状态不做详细分析。

# 8.2.5 LTSSM的其他状态

如图8-5所示，在LTSSM中还含有Disabled、Host Reset、Loopback、Recovery、L0、L1、L2和L0s状态。其中L0、L1、L2和L0s与PCIe的电源管理相关，在第8.3节将详细介绍这些状态的转换关系。

PCIe链路还可以从Configuration和Recovery状态进入Disabled状态。当系统软件需要关闭PCIe链路时，可以通过设置Link Control寄存器的Link Disabled位，使PCIe链路进入Disabled状态。物理层进入Disabled状态时，将禁止PCIe链路的使用，然后视情况进行进入PCIe链路的初始状态Detect，重新进行PCIe链路的训练工作。

而 Loopback 状态是一种调试状态，PCIe 总线测试仪器可以使 PCIe 链路的对端设备进入该状态，然后对 PCIe 链路进行测试。

Hot Reset 状态从 Recovery 状态进入，当系统软件对 PCIe 链路进行 Hot Reset 操作时，PCIe 链路将进入该状态，然后进入 PCIe 链路的初始状态 Detect。

# 8.3 PCIe总线的ASPM

PCIe总线的电源管理包含ASPM和软件电源管理两方面内容。所谓ASPM是指PCIe链路在没有系统软件参与的情况下，由PCIe链路自发进行的电源管理方式。而软件电源管理指PCI PM机制，PCIe总线的软件电源管理与PCI总线兼容。

对于一个通用处理器系统，电源管理的硬件实现与软件处理过程都较为复杂。而对于一个专用的处理器系统，如手机应用，在多数情况下，更侧重于在某些用户场景中，功耗的使用情况，其设计难度相对较小。而无论是对于通用还是专用处理器系统，电源管理都需要处理器与外部设备的参与，协调完成。

PCIe总线为PCIe设备提供了几种低功耗模式。在一个处理器系统中，PCIe设备的低功耗模式需要与处理器的低功耗模式协调工作，以最优化整个处理器系统的功耗。

对于某些专用的处理器系统，外部设备之间也需要协调工作，以最优化整个处理器系统的使用的功耗。目前电源管理已经成为处理器系统实现的一个热点。但是PCI/PCIe总线提供的电源管理机制远非完美，该管理机制仅考虑了外部设备与处理器系统之间的电源使用关系，并没有考虑外部设备之间电源的使用关系。

在一个处理器系统中，外部设备越来越智能化，处理器与外设间的通信基本等同于两个处理器间的通信，适用于这类处理器系统的电源管理机制也有待研究。在有些处理器系统中，专门设置了用于电源管理的微处理器，协调“主处理器”与“外部设备”及外部设备间的电源使用情况，以最优化整个处理器系统的电源管理。目前在智能手机的设计中，通常具有专门用于电源管理的微处理器。

# 8.3.1 与电源管理相关的链路状态

PCIe总线定义了一系列与电源管理相关的链路状态。

- L0 状态。PCIe 设备的正常工作状态  
- L0s 状态。PCIe 设备处于低功耗状态。系统软件不能控制 L0 状态和 L0s 状态间的迁移过程，这两个状态的迁移只能由 ASPM 控制。  
- L1 状态。PCIe 设备使用的功耗低于处于 L0s 状态时的功耗。  
- L2/L3 Ready 状态。PCI 设备进入 L2 或者 L3 状态之前使用的过渡状态。  
- L2 状态。PCIe 设备仅使用辅助电源工作，主电源已经被关闭。在 PCIe 总线中 L1 和 L2 状态是可选的。  
- L3 状态。该状态也被称为“Link off”状态，此时 PCIe 设备使用的 Vcc 电源已经被关闭。  
- LDn 状态。该状态是一个“伪”状态，PCIe 链路处于 L2、L3 状态时，需要通过 LDn 状态之后才能进入 L0 状态。该状态由 LTSSM 状态机的 Detect、Polling 和 Configuration 等状态组成。

这些与电源管理相关的状态机迁移模型如图8-11所示。

![[pci_express/8b86b36de4a0d5fd78e20ab1794081ee15fdb6aeb2f621cd574b71d27c0a43c0.jpg]]  
图8-11 电源管理状态机

本节重点说明L0、L0s和L1状态的工作原理以及如何使用ASPM机制进行状态迁移。在第8.4节将讲述系统软件如何设置寄存器使PCIe设备进入L0、L0s和L1状态。

在PCIe设备中，Link Capabilities寄存器的ASPM Support字段表示当前PCIe设备可以支持的链路状态，该字段只读。而Link Control寄存器的ASPM Control字段为可读写的，PCIe设备根据ASPM Support字段判断当前PCIe链路是否支持L0s和L1状态，还是同时支持这两种状态，并设置ASPM Control字段。

# 8.3.2 L0状态

PCIe 设备从 Configuration、Recovery 和 L0s 状态进入 L0 状态时，L0 状态是 PCIe 设备的正常工作状态。此时 PCIe 设备可以通过 PCIe 链路，发送和接收 TLP、DLLP 和 PLP，此时 LinkUp 状态位为 1，而数据链路层处于 DL\_Active 状态。PCIe 设备从 L0 状态可以进入 Recovery、L0s、L1 和 L2/L3 Ready 状态，本节重点讨论从 L0 状态进入 Recovery、L0s 和 L1 状态的情况。而从 L0 状态进入 Recovery 状态的条件见第 8.2.4 节。

# 1. 进入L0s状态

当PCIe设备发现链路“空闲”时，可以主动进入L0s状态。RC、EP和Switch进入L0s状态的“空闲”条件并不相同。对于含有多个Function的上游端口，只有所有Function都处于“空闲”状态时，才被认为“空闲”。此处“空闲”的定义与LTSSM的Electrical Idle和Logical Idle间没有任何联系。

对于RC或者EP的端口，当以下两个条件同时满足时，当前端口被认为是“空闲”的。

（1）没有准备发送的TLP，或者对端没有提供足够的Credit。  
(2）没有准备发送的DLLP。

对于Switch的上游端口，当以下三个条件同时满足时，发送逻辑TX认为PCIe链路是“空闲”的。

（1）Switch所有下游端口的接收链路处于L0s状态。  
（2）没有准备发送的TLP，或者对端没有提供足够的Credit。  
（3）没有准备发送的 DLLP。

对于Switch的下游端口，当以下三个条件同时满足时，发送逻辑TX认为PCIe链路是“空闲”的。

（1）Switch的所有上游端口的接收链路处于L0s状态。  
(2) 没有准备发送的 TLP，或者对端没有提供足够的 Credit。  
(3) 没有准备发送的 DLLP。

值得注意的是，一个接收端或者发送端的发送逻辑 TX 和接收逻辑 RX 在同一时刻，可能处于不同的 LTSSM 状态，一个处于 L0，而另一个处于 L0s。

# 2. 进入L1状态和L2状态

PCIe 设备可以通过上层软件，将链路状态从 L0 状态迁移到 L1 或者 L2 状态。当 PCIe 设备进入 D1 \~ D3 状态时，其上游链路将进入 L1 状态；而进入 $\mathrm{D}3_{\mathrm{Cold}}$ 状态时，其上游链路将进入 L2 状态。D1 \~ D3 和 $\mathrm{D}3_{\mathrm{Cold}}$ 状态的详细说明见第 8.4.1 节。而 PCIe 链路的两端设备需要同时进入 L1 或者 L2 状态。

首先两端设备的发送逻辑 TX 分别向对端发送 EIOS 序列，随后发送逻辑 TX 进入 Electrical Idle 状态。如果 PCIe 设备的接收逻辑 RX 从任意 Lane 中收到 1 个或者 2 个 EIOS 序列后，该设备将进入到 L1 或者 L2 状态。当 PCIe 链路工作在 $2.5\mathrm{GT} / \mathrm{s}$ 时，需要发送 1 个 EIOS 序列；如果工作在 $5\mathrm{GT} / \mathrm{s}$ 时，需要发送 2 个 EIOS 序列。

值得注意的是，PCIe设备处于L1状态和L2状态时， $\mathrm{D}+$ 和 $\mathrm{D}-$ 信号输出的DC共模电压并不相同。处于L1状态时，PCIe设备的发送逻辑TX仍然需要维持一个相对较低的DC共模电压，其伏值比其处于L0状态时低VTX- CM- DC-ACTIVE-IDLE-DELTA（最小值为0，最大值为 $100\mathrm{mv}$ ），如公式8-1所示。

$$
\left| V _ {T X - C M - C D [ D u r i n g L 0 ]} - V _ {T X - C M - I d l e - D C [ D u r i n g E l e c t r i c a l I d l e ]} \right| \leqslant 1 0 0 \mathrm {m V} \tag {8-1}
$$

而PCIe设备在L2状态时，其发送逻辑TX并没有这种限制，而且其发送逻辑和接收逻辑基本处于下电状态。这正是PCIe设备处于L2状态时使用的功耗低于L1状态的原因，同时也是从L2状态进入L0状态，更加耗时的原因。

如果PCIe设备支持Beacon机制，那么处于L2状态时，PCIe设备的发送逻辑TX能够发送Beacon信号，而接收逻辑RX能够检测Beacon信号。Beacon机制是一种唤醒机制。当PCIe设备处于L2状态时，可以使用该机制唤醒。

