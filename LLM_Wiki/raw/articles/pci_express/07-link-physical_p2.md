---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "07"
section: "7.2.3 数据链路层发送报文的顺序"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 7.2.3 数据链路层发送报文的顺序

数据链路层还规定了报文发送的顺序。由上文的描述中，我们可以发现 DLLP 和 TLP 的发送共用一个 PCIe 链路，除此之外物理层的报文 PLP（Physical Layer Packet）也使用同样的链路。因此 PCIe 链路需要合理地安排报文的发送顺序，以避免死锁。其发送顺序如下所示。

(1) 正在发送的 TLP 或者 DLLP 具有最高的优先权。PCIe 总线为了保证数据的完整性, 不允许打断正在传送的报文。从理论上讲, 打断正在传送的报文是可行的, 但是硬件需要更大的代价, 也需要制定更加复杂的协议保证数据的完整性。  
(2) PLP 的传送。一般来说，处于协议底层的报文优先权高于处于协议高层的报文，这也是解决死锁的一个有效方法。  
(3) NAK DLLP。NAK DLLP 需要优先于 TLP 的发送，原理同上。  
(4) ACK DLLP。ACK DLLP 响应正确接收的报文，在绝大多数处理过程中，错误处理报文优先于正确的响应，这也是一种防止死锁的方法。  
(5) 重新传送 Replay Buffer 中的 TLP。也是一种发现错误后的恢复手段, 因此这种报文的传递优先权高于其他 TLP。因为在错误没有处理完毕之前, 其他 TLP 的传递是没有意义的, 接收端都将丢弃这些报文。  
(6) 其他在事务层等待的 TLP。  
(7) 其他 DLLP，这些 DLLP 包括地址路由，电源管理等报文，这些报文与数据报文的传递无关，是 PCIe 总线规定的一些控制报文，所以优先权最低。

# 7.3 物理层简介

如图4-4所示，物理层在数据链路层和PCIe链路之间，其主要作用有两个，一是发送数据链路层的TLP和DLLP；二是发送和接收在物理层产生的报文PLP（Physical Layer Packet）；三是从PCIe链路接收数据报文并传送到数据链路层。

物理层主要由物理层逻辑模块和物理层电气模块组成，本节主要介绍物理层的逻辑模块，包括8/10b编码、链路训练等一些最基础的内容，并通过介绍差分信号的工作原理，简要介绍物理层的电气模块。物理层的电气模块对于深入理解PCIe总线规范非常重要，但是许多系统软件工程师因为缺少必要的基础知识，很难理解这部分内容。

本节的内容是第8章的基础。如果读者需要深入理解PCIe的链路训练，必须掌握本节的全部内容。如果读者对第8章内容不感兴趣，可以略过第8章和本节。但是PCIe总线各个层

次间的联系较为紧密，读者很难在对物理层一无所知的情况下，深入理解PCIe总线规范。

物理层的电气模块与差分信号的工作原理密切相关，这部分原理包括一系列与信号完整性相关的课题。而信号完整性本身就是一个专门的话题，其难度与复杂程度较高。信号完整性所追求的目标如下。

（1）保证发送的信号可以被接收端正确接收。  
（2）保证发送的信号不会影响其他信号。  
（3）保证发送的信号不会损坏接收器件。  
(4) 保证发送的信号不会产生较大的 EMI 电磁噪声。

PCIe总线的物理层对信号传送进行了一系列约定，以保证信号传递的完整性。而这些约定建立在差分信号传送规则的基础上。如果读者能够深入理解差分信号的工作原理，理解这些约定并不困难。

# 7.3.1 PCIe链路的差分信号

PCIe链路使用差分信号进行数据传递，而差分信号由两个信号组成，这与PCI总线使用的单端信号有较大区别。

首先所有信号的传递都需要一个电流回路，而且流入一个节点的电流总和等于流出这个节点的电流总和。单端信号使用地平面作为电流回路，而这个地平面并不是稳定的，极易受到干扰，其中最重要的干扰为SSO（Simultaneous Switching Output）噪声。而减缓SSO噪声最有效的方法是为器件的电源提供退耦电容，这个方法也被西方的工程师称为“The rule of thumb”，在电路设计中，该方法极为普及。

即便如此单端信号使用的地平面仍不足以信赖，仍会给单端信号的传递带来不小的干扰。其次单端信号容易收到其他信号的干扰，当单端信号频率较高时，信号在传递过程中衰减较大，而采用差分信号可以有效避免使用单端信号的这些问题。差分信号是由驱动端发送两个等值、相位相反的信号，接收端通过这两个信号的电压差值来判断差分信号是逻辑状态“0”还是“1”。与单端信号相比，差分信号具有许多优势。

(1) 抗干扰能力较强。差分信号不受 SSO 噪声的影响, 其走线是等长的, 且距离较近。当外界存在噪声干扰时, 这些干扰同时被耦合到两个信号上, 因为接收端只关心这两个信号的差值, 这些干扰相减后可以忽略不计。  
(2) 能够有效抑制信号传递带来的 EMI 干扰。差分信号的极性相反, 对外界辐射的电磁场可以相互抵消, 因此产生的噪声较小。  
(3) 逻辑状态定位准确。由于差分信号的开关变化位于两个信号的交点，而不像单端信号使用高低电压两个阈值进行判断，因而受制造工艺、外部环境变化的影响较小，使用差分信号时，接收逻辑较易判断逻辑状态“0”和“1”。  
(4) 提供的数据带宽较高。由于差分信号受外界环境的影响较小，能够运行在更高的时钟频率上，从而提供的数据带宽较高。

差分信号也有缺点。首先差分信号使用两个信号传递数据，与单端信号相比使用的信号

线较多。但是考虑到单端信号为了保证信号质量，往往使用“两线加一地” $\leftarrow$ 的方式，因此差分信号使用的信号线数量与单端信号相比，并不是简单乘2的关系。其次差分信号的布线与单端信号相比具有较多的约束。差分信号对要求等长且平行走线，而且在实际的PCB中，最好做到同层等长，因为不同层间的特性阻抗并不完全相等，而是有一定的误差。

这些约束为差分信号的使用带来了一些困难，但是这些困难并不影响差分信号的大规模应用。目前已知的高速链路均使用差分信号，即将问世的 $40\mathrm{Gb / 100Gb}$ 以太网和PCIe V3.0规范也将继续使用差分信号进行数据传递。

差分信号进行传递时依然需要电流回路，而且这个回流路径仍然主要使用地平面，虽然差分信号对彼此也可以作为电流回路，但这并不是主要的回流路径。所有高频信号总是使用电感最小的电流回路，差分信号除了相互间的耦合之外，更多的是对地耦合。

因此差分信号在进行传递时，参考地平面仍然最为重要。如果参考地平面不连续或者不存在参考地平面时，差分信号间的耦合才作为回流通路，但是使用这种方法将极大降低差分信号的质量，而且会增加EMI干扰，因此在设计中并不建议使用这种方法。差分信号的传递方法如图7-10所示。

如该图所示，差分信号使用两根信号 $\mathrm{D}+$ 和 $\mathrm{D}-$ 进行信号传递，其中差分信号可以使用两种方法描述，分别为 $(\mathrm{V}_1,\mathrm{V}_2)$ 和 $(\mathrm{V}_{\mathrm{cm}},\mathrm{V}_{\mathrm{diff}})$ 。

假设信号 $\mathrm{D}+$ 的对地参考电压为 $\mathrm{V}_{1}$ ，而信号 $\mathrm{D}-$ 的对地参考电压为 $\mathrm{V}_2$ ，使用（ $\mathrm{V}_1,\mathrm{V}_2$ ）可以描述一个差分信号，但是这种方法并不常用。因为使用（ $\mathrm{V}_1,\mathrm{V}_2$ ）这种方法描述差分信号并不直观，接收端更关心这两个信号的差值，即 $\mathrm{V}_1 - \mathrm{V}_2$ 。

![[pci_express/d8373bb8ad0ff8aae106465f117ea813c9ed7f3a774c1cbf52ee3c5cdf034772.jpg]]  
图7-10 差分信号的传递

为此我们引入两个参数（ $\mathrm{V}_{\mathrm{cm}}$ ， $\mathrm{V}_{\mathrm{diff}}$ ），其中

$\mathrm{V}_{\mathrm{diff}}$ 参数代表信号 $\mathrm{D}+$ 和信号 $\mathrm{D}-$ 的差值电压（Differential Voltage），而 $\mathrm{V}_{\mathrm{cm}}$ 参数代表这两个信号的共模电压（Common Mode Voltage）。这两个参数的计算方法如公式7-1所示。

$$
\begin{array}{l} V _ {c m} = (V 1 + V 2) / 2 \\ V _ {d i f f} = V 1 - V 2 \\ \end{array}
$$

(7-1)

其中 $\mathrm{V1} = \mathrm{V_{cm}} + \mathrm{V_{diff}} / 2$ 而 $\mathrm{V2} = \mathrm{V_{cm}} + \mathrm{V_{diff}} / 2$ 。对于差分信号而言， $(\mathrm{V}_1,\mathrm{V}_2)$ 和（ $\mathrm{V_{cm}}$ ， $\mathrm{V_{diff}}$ ）这两种描述方式是完全等价的。如图4-1所示，在PCIe链路中，差分信号首先经过一个AC耦合电容后，才能到达接收端。因此接收端收到的差分信号，其直流电压已被滤去。因此在实际应用中，接收端并不关心 $\mathrm{V_{cm}}$ ，而仅关心 $\mathrm{V_{diff}}$ 。但是发送端需要置 $\mathrm{V_{cm}}$ 为一个合适的偏置电压，保证信号的传送。在理想情况下，差分信号 $\mathrm{D}+$ 和 $\mathrm{D}-$ 相位相反，幅值相等，且 $\mathrm{V_{cm}}$ 为一个常量，如图7-11所示。

在该图中实线部分为 $\mathrm{D}+$ 信号，而虚线部分为 $\mathrm{D}-$ 信号，这两个信号相位完全相反，且为非常理想的正弦波， $\mathrm{V}_{\mathrm{cm}}$ 为一个恒定的值，而差分电压 $\mathrm{V}_{\mathrm{diff}}$ 也是一个理想的正弦波，其峰值为 $\mathrm{D}+$ 或者 $\mathrm{D}-$ 信号的两倍。

![[pci_express/0df4dbe1dcad82f6758454f117fa5e258f6238cee11402f53d1d76a9b6b6daec.jpg]]

![[pci_express/e62e4c863fc54cc6da3caf0352483bcc250ff987264dd26d5f72833ec0c3546b.jpg]]  
图7-11 理想的差分信号

然而在差分信号的实际应用中，由于信号 $\mathrm{D}+$ 和 $\mathrm{D}-$ 并不会完全对称，相位也不会完全相反，可能会存在一些偏差，如图7-12所示。

![[pci_express/4b10615e855f81cc9629e0f5c68e673180263d13e9b7480476375028a904e023.jpg]]  
图7-12 次理想的差分信号

如上图所示，由于 $\mathrm{D}+$ 和 $\mathrm{D}-$ 并不完全对称，因此 $\mathrm{V}_{\mathrm{cm}}$ 并不是一个常数，而是以 $\mathrm{V}_{\mathrm{dccm}}$ 为中心进行上下波动。 $\mathrm{V}_{\mathrm{dccm}}$ 为DC共模电压（DCCommonModeVoltage），其值为 $\mathrm{V}_{\mathrm{cm}}$ 在一段时间内的平均直流电压。

与直流共模电压相对应，在对差分信号进行分析时，还使用AC共模电压（AC Common Mode Voltage），简写为 $\mathrm{Vaccm\_rms}^{\ominus}$ ，其值为 $\mathrm{RMS}((\mathrm{V1} + \mathrm{V2}) - \mathrm{V}_{\mathrm{dccm}})$ 。其中RMS（Root Mean Square）用来计算AC电压的有效值，对于正弦波而言，RMS值约等于峰值的0.707倍。

在差分信号传递中，使用UI（Unit Interval）计算单位时间，如图7-11所示，UI的值为一个正弦波的半个周期。在PCIeV1.x规范中，使用的时钟频率为 $1.25\mathrm{GHz}$ ，因此UI在 $399.88\sim 400.12\mathrm{ps}$ 之间；而PCIeV2.x规范使用的时钟频率为 $2.5\mathrm{GHz}$ ，此时UI在199.94 $\sim 200.06$ ps之间。PCIe总线还规定了一系列有关差分信号传递的参数，并分为发送逻辑和接收逻辑区别处理，如表7-3和表7-4所示。

值得注意的是，在本书中发送端与发送逻辑，接收端与接收逻辑是完全不同的概念。如图4-1所示，发送端和接收端都包含发送逻辑和接收逻辑，本书为强调发送逻辑和接收逻

辑的概念，使用“发送逻辑 TX（Transmitter）和接收逻辑 RX（Receiver）来表示发送端和接收端的发送逻辑和接收逻辑。有的书籍也将发送逻辑和接收逻辑称为发送模块和接收模块。

本节仅列出“发送逻辑 TX”和“接收逻辑 RX”使用的部分参数，对全部参数有兴趣的读者可以参阅PCIe V2.1总线规范的表4-9和表4-12。

表 7-3 PCIe 链路 “发送逻辑 TX” 差分信号的参数

<table><tr><td>符号名</td><td>2.5 GT/s</td><td>5 GT/s</td><td>单位</td><td>描述</td></tr><tr><td>UI</td><td>399.88 (Min)400.12 (Max)</td><td>199.94 (Min)200.06 (Max)</td><td>ps</td><td>时钟误差范围为±300 ppm,因此 UI存在少许误差</td></tr><tr><td>VTX-DIFF-PP</td><td>0.8 (Min)1.2 (Max)</td><td>0.8 (Min)1.2 (Max)</td><td>V</td><td>Vdiff的峰峰值,等于2 | VD+ - VD- |</td></tr><tr><td>VTX-DIFF-PP-LOW</td><td>0.4 (Min)1.2 (Max)</td><td>0.4 (Min)1.2 (Max)</td><td>V</td><td>在低电压模式下 Vdiff的峰峰值</td></tr><tr><td>T TX-EYE</td><td>0.75 (Min)</td><td>0.75 (Min)</td><td>UI</td><td>眼图的宽度</td></tr><tr><td>ZTX-DIFF-DC</td><td>80 (Min)120 (Max)</td><td>120 (Max)</td><td>Ω</td><td>差分信号的 DC 阻抗</td></tr><tr><td>VTX-CM-AC-PP</td><td>Not specified</td><td>100 (Max)</td><td>mV</td><td>AC共模电压的峰峰值,等于max(VD+ +VD-)/2-min(VD+ +VD-)/2</td></tr><tr><td>VTX-CM-AC-P</td><td>20</td><td>Not specified</td><td>mV</td><td>AC共模电压的有效值,其值等于RMS[(VD+ +VD-)/2-DCAVG(VD+ +VD-)/2]①</td></tr><tr><td>ITX-SHORT</td><td>90 (Max)</td><td>90 (Max)</td><td>mA</td><td>发送逻辑 TX 在短路状态下的输出电流</td></tr><tr><td>VTX-DC-CM</td><td>0 (Min)3.6 (Max)</td><td>0 (Min)3.6 (Max)</td><td>mV</td><td>DC共模电压</td></tr><tr><td>VTX-IDLE-DIFF-AC-p</td><td>0 (Min)20 (Max)</td><td>0 (Min)20 (Max)</td><td>mV</td><td>发送逻辑 TX 处于 Electrical Idle 状态时,VD+和VD-的交流电压差值</td></tr><tr><td>VTX-IDLE-DIFF-DC</td><td>Not specified</td><td>0 (Min)5 (Max)</td><td>mV</td><td>发送逻辑 TX 处于 Electrical Idle 状态时,VD+和VD-的直流电压差值。</td></tr><tr><td>VTX-RCV-DETECT</td><td>600 (Max)</td><td>600 (Max)</td><td>mV</td><td>该参数与 Receiver Detection 的过程相关。第8.1.3节将详细介绍 Receiver Detection 逻辑</td></tr><tr><td>T TX-IDLE-MIN</td><td>20 (Min)</td><td>20 (Min)</td><td>ns</td><td>发送逻辑 TX 处于 Electrical Idle 状态时的最短时间</td></tr><tr><td>T TX-IDLE-SET-TO-IDLE</td><td>8 (Max)</td><td>8 (Max)</td><td>ns</td><td>发送完毕 EIOS 序列后,发送逻辑 TX 进入 Electrical Idle 状态的最短时间</td></tr><tr><td>T TX-IDLE-TO-DIFF-DATA</td><td>8 (Max)</td><td>8 (Max)</td><td>ns</td><td>发送逻辑 TX 离开 Electrical Idle 状态,到可以发送正常差分信号需要的转换时间</td></tr><tr><td>TCROSSLINK</td><td>1.0 (Max)</td><td>1.0 (Max)</td><td>ms</td><td>在使用 Crosslink 连接两个 Switch 时使用</td></tr><tr><td>LTX-SKEW</td><td>500 +2UI (Max)</td><td>500 +4UI (Max)</td><td>ps</td><td>Lane-Lane 间的传送漂移</td></tr><tr><td>CTX</td><td>75 (Min)200 (Max)</td><td>75 (Min)200 (Max)</td><td>nF</td><td>发送逻辑 TX 使用的 AC 耦合电容</td></tr></table>

① $\mathrm{DC}_{\mathrm{AVG}}(\mathrm{V}_{\mathrm{D}+} + \mathrm{V}_{\mathrm{D}-}) / 2$ 为一段时间内 DC 共模电压的平均值，PCIe 总线要求这段时间至少为 $10^{6}$ 个 UI。

表 7-4 PCIe 链路“接收逻辑 RX”差分信号的参数

<table><tr><td>符号名</td><td>2.5 GT/s</td><td>5 GT/s</td><td>单位</td><td>描述</td></tr><tr><td>UI</td><td>399.88(Min)400.12(Max)</td><td>199.94(Min)200.06(Max)</td><td>ps</td><td>与发送逻辑类似</td></tr><tr><td> $V_{RX-DIFF-PP-CC}$ </td><td>0.175(Min)1.2(Max)</td><td>0.120(Min)1.2(Max)</td><td>V</td><td rowspan="2"> $V_{diff}$ 的峰峰值</td></tr><tr><td> $V_{RX-DIFF-PP-DC}$ </td><td>0.175(Min)1.2(Max)</td><td>0.100(Min)1.2(Max)</td><td>V</td></tr><tr><td> $T_{RX-EYE}$ </td><td>0.4(Min)</td><td>N/A</td><td>UI</td><td>眼图的宽度</td></tr><tr><td> $T_{RX-MIN-PULSE}$ </td><td>Not Specified</td><td>0.6(Min)</td><td>UI</td><td>接收端需要的信号最小脉冲间隔</td></tr><tr><td> $Z_{RX-DC}$ </td><td>40(Min)60(Max)</td><td>40(Min)l60(Max)</td><td>Ω</td><td>单端信号的 DC 阻抗。该参数的作用是便于发送逻辑 TX 进行 Receiver Detection</td></tr><tr><td> $Z_{RX-DIFF-DC}$ </td><td>80(Min)120(Max)</td><td>Not Specified</td><td>Ω</td><td>差分信号的 DC 阻抗</td></tr><tr><td> $V_{RX-CM-AC-P}$ </td><td>150(Max)</td><td>150(Max)</td><td>mV</td><td>AC共模电压</td></tr><tr><td> $Z_{RX-HIGH-IMP-DC-POS}$ </td><td>50k(Min)</td><td>50k(Min)</td><td>Ω</td><td>接收逻辑 RX 的  $V_{cc}$  没有上电时,当输入电压大于 0 时 DC 共模输入阻抗</td></tr><tr><td> $Z_{RX-HIGH-IMP-DC-NEG}$ </td><td>1.0k(Min)</td><td>1.0k(Min)</td><td>Ω</td><td>接收逻辑 RX 的  $V_{cc}$  没有上电时,当输入电压小于 0 时 DC 共模输入阻抗</td></tr><tr><td> $V_{RX-IDLE-DET-DIFFp-p}$ </td><td>65(Min)175(Max)</td><td>65(Min)175(Max)</td><td>mV</td><td>用于 Idle 状态检测的电压阈值。其值等于 2 | $V_{RX-D+}-V_{RX-D-}$  |</td></tr><tr><td> $T_{RX-IDLE-DET-DIFFENTERTIME}$ </td><td>10(Max)</td><td>10(Max)</td><td>ms</td><td>当  $V_{diff}$  小于 65mV 时,发送逻辑 TX 可能已经处于 Electrical Idle 状态。发送逻辑 TX需要在 10 ms 之内识别出这种“意外”的 Electrical Idle 状态①</td></tr></table>

① 在正常情况下，发送模块进入 Electrical Idle 状态之前，需要发送若干个 EIOS 序列。

以上这些参数将在PCIe链路训练和重新训练中使用。在PCIe总线中，和差分信号有关的内容还有许多，如阻抗的计算、Emphasis（预加重）、De-Emphasis（预去重）和PCB布线等。这些内容并非本书的重点，对此有兴趣的读者可参考Howard Johnson和Martin Graham合著的High-Speed Signal Propagation。

深入理解差分信号的工作原理是理解PCIe电气子层的重要基础，建议对处理器体系结构有兴趣的读者掌握一些基本的信号完整性的理论知识。从PCIe体系结构设计的角度来看，信号完整性这部分内容涉及了许多模拟电路的设计与实现知识，这部分内容是PCIe体系结构的精华所在，但并不是本书侧重的内容。

# 7.3.2 物理层的组成结构

PCIe总线的物理层通过LTSSM状态机对PCIe链路进行配置与管理，并与数据链路层进行数据交换，由逻辑子层（Logical Sub-block）和电气子层（Electrical Sub-block）组成。本节主要讲述逻辑子层。逻辑子层与数据链路层进行数据交换，由发送逻辑TX和接收逻辑RX组成，其结构如图7-13所示。

![[pci_express/743e006a8e7cde020948a4afa66b9851329f30036d3d41ca202e2d4db7a7b608.jpg]]  
图7-13 逻辑子层的组成结构

如上图所示，物理层发送报文的过程如下。

（1）物理层从数据链路层获得TLP或者DLLP，然后放入TxBuffer中。  
(2) 物理层将这些 TLP 或者 DLLP 加入物理层的前缀（Start Code）和后缀（End Code），后通过多路选择器 Mux，进入 Byte Stripping 部件。物理层也定义了一系列 PLP，这些 PLP 也可以通过 Mux，进入 Byte Stripping 部件。  
(3) PCIe 链路可能由多个 Lane 组成，Byte Stripping 部件可以将数据报文分发到不同的 Lane 中。在 PCIe 链路的不同 Lane 中传递的数据可能存在漂移，即 Skew，Byte Stripping 部件还有一个重要功能即消除这个漂移，即 De-skew。  
(4) 数据进入到各自 Lane 的加扰（Scrambler）部件，“加扰”后进行 8/10b 编码，最后通过并转串逻辑将数据发送到 PCIe 链路中。

物理层的接收过程是发送的逆过程，其步骤如下。

（1）物理层从PCIe链路的各个Lane获得串行数据，并通过8/10b解码和De-Scrambler部件，发送到“ByteUn-Stripping”部件。  
(2) “Byte Un-Stripping” 部件将来自不同 Lane 的数据合并，进行 De-skew 操作，然后取出物理层的前后缀并进行边界检查后，将数据放入 Rx Buffer 中。  
（3）物理层将在RxBuffer中的数据传递到数据链路层。

物理层的数据在通过 Byte Un-Stripping/Stripping 部件时，需要注意大小端模式的转换。而 Scrambler 和 De-Scrambler 部件的主要作用是对数据流进行“加扰”和“解扰”操作。在串行链路上进行数据传递时，如果在字符流中存在某些规律，这些“规律”将会叠加，并产生较大的 EMI（Electromagnetic interference）噪声。

Scrambler 部件的主要作用就是通过 “加扰” 的方法削减 EMI 噪声, 所谓加扰是指将源

数据流与一个随机序列进行异或操作后，再发送出去。此时被发送出的数据流也基本是伪随机的，从而降低了发送数据时产生的EMI噪声。

PCIe总线通过一个16位线性反馈移位寄存器（Linear Feedback Shift Register，LFSR），产生伪随机序列，该移位寄存器的表达式如公式7-2所示。

$$
G (x) = X ^ {1 6} + X ^ {5} + X ^ {4} + X ^ {3} + 1 \tag {7-2}
$$

该公式是一个本原多项式，使用该本原多项式可以产生一个周期为 $2^{16} - 1$ （这个周期是16位移位寄存器能够产生的最大周期）的伪随机序列。所谓本原多项式是“具有最大周期”的不可约多项式。对应的，由本原多项式作为生成多项式所产生的LFSR序列为最大周期序列。这些序列一般被称为 $\mathrm{m}-$ 序列，在 $\mathrm{m}-$ 序列中“0”和“1”所占的比例相对均衡，但是1的个数比0的个数多1，因为全0不能作为初始值，也不可能是中间状态。

来自 Byte Stripping 部件的字符流与这个伪随机序列中的字符流进行异或操作，从而生成一个相对较为随机的字符流，从而降低了数据流的 EMI 噪声。

De-Scrambler 部件的主要作用是进行解扰。值得注意的是，在 PCIe 链路的两端，加扰和解扰使用的编解码公式相同，而且完全同步，即 LFSR 使用相同的初始值，在 PCIe 链路的两端，该初始值为 0xFFFF。PCIe 链路两端设备每次加解扰一个 8b 数据后，LFSR 进行 8 次移位操作。在 PCIe 总线中，数据在发送时，首先经过“加扰”操作，然后进入 8/10b 编码模块；而接收数据时，首先经过 8/10b 解码模块，然后进行“解扰”操作。

# 7.3.3 8/10b编码与解码

IBM于1983年提出 $8 / 10\mathrm{b}$ 编码方法，这个编码方法也是IBM的专利。目前这个专利已经过期，以太网、ATM、Infiniband和FC（Fiber Channel）在物理链路的数据传送中也使用了 $8 / 10\mathrm{b}$ 编码技术。 $8 / 10\mathrm{b}$ 编码是高速串行总线常用的编码方式。

该编码将8位编码转化为10位，以平衡数据流中0与1的数量。使用这种方法可以保证数据流中1和0的数量相等，即保证直流平衡（DC Balance）。如果在一个高速串行数据流中有较多连续的“1”时，会将AC耦合电容充满，从而影响这些电容的正常工作，在PCIe链路上，AC耦合电容的位置如图4-1所示。

PCIe V1.x 和 2.x 规范使用了 8/10b 编码方式，而 V3.0 规范将使用 128/130b 编码方式。128/130b 编码方式与 8/10b 编码方式原理较为类似，使用 128/130b 编码可以进一步提高 PCIe 总线的利用率，但是需要更多的硬件资源。本节仅介绍 8/10b 编解码方式。在 PCIe 总线中，编码与解码的过程如图 7-14 所示。

8/10b编码的基本原理是将一个连续的8位数据流分为两组，其中一组由3位（FGH）组成，而另一组由5位（ABCDE）组成。8/10b编码将这两组数据流分别编码成一组4位（fghj）和一组6位（abcdei）的数据流。而解码过程是编码的逆过程，将一组4位和一组6位的数据流还原成为一组3位和一组5位的数据流。

PCIe设备采用这种编码方式可以保证数据流中出现的0和1的数量基本保持一致，同时保证在通过PCIe物理链路的数据流中，连续的“1”和“0”不会超过5个。虽然采用8/10b编码将降低总线的使用效率，但是能够保证高速串行信号的传送完整性。

如图7-13所示，物理层可以对两类字符进行8/10b编码，一类是数据字符，即从数据链路层获得的TLP和DLLP；一类是物理层使用的控制字符，如Start/End/Idle Code

和一些物理层中使用的PLP。为此PCIe总线在进行8/10b编解码需要区分数据和控制字符。

![[pci_express/1e9fb4b525cf5da1e6b7f6c0bc0406310be210dfe53de20d0f6f33549f3b86f7.jpg]]

![[pci_express/84a247eff75685934263b1a1461c4e29f79585b5f4e5a1d5e61cbc70e4a87106.jpg]]  
图7-14 8/10b编解码过程

数据字符与控制字符使用的8/10b编码不同，PCIe总线分别使用Dxx.y和Kxx.y表示数据字符（D）和控制字符（K)，其中xx记录字符的低5位ABCDE，而y记录字符的高3位FGH。

值得注意的是，PCIe总线使用8/10b编码可以保证每十位中，最多有6个1或者6个0，而不是传统8/10b编码中要求的“不超过5个连续的0或者1”。使用这种方法基本上可以保证数据流的DC平衡。但是使用这种编码方式无法保证在某些特殊情况中，连续发送的数据流中都含有“6个1，4个0”或者“6个0，4个1”。随着时间的累积，这些数据流依然会在链路中造成严重的DC失衡。

为此PCIe总线使用CRD（Current Running Disparity）技术进一步保证PCIe链路的DC平衡。PCIe总线在进行8/10b编码时，每一个Dxx.y和Kxx.y对应两个10b的编码，分别是 $\mathrm{CRD}+$ 和 $\mathrm{CRD}-$ 。对于多数编码， $\mathrm{CRD}-$ 和 $\mathrm{CRD}+$ 中含有的“0”和“1”的个数相同，如D1.0、D2.0等数据字符。但是在有些 $\mathrm{CRD}+$ 编码中，“1”的个数小于“0”的个数；而在 $\mathrm{CRD}-$ 编码中“0”的个数大于“1”的个数，如D1.1和D2.1的编码。

在 $\mathrm{CRD}+$ 编码中，“1”的个数小于或者等于“0”的个数；在 $\mathrm{CRD}-$ 编码中，“0”的个数小于或者等于“1”的个数。值得注意的是， $\mathrm{CRD}+$ 和 $\mathrm{CRD}-$ 编码并不是直接取反的关系，当 $\mathrm{CRD}$ 编码的“0”的个数与“1”的个数相同时， $\mathrm{CRD}+$ 与 $\mathrm{CRD}-$ 的编码有时是相同的，如 D3.5 的编码。

下文以一个数据发送的实例说明 $\mathrm{CRD}+$ 、 $\mathrm{CRD}-$ 编码的使用。在PCIe链路的发送端中，存在一个CRD状态位，其初始值可以为“正”或者“负”。随着通过PCIe链路的数据流增多，累积的“1”和“0”的个数可能并不平衡。当所有通过PCIe链路的字符流中，“1”的个数大于“0”的个数时，CRD状态为正；当所有通过PCIe链路的字符流中，“0”的个数大于“1”的个数时，CRD状态为负；当所有通过PCIe链路的字符“0”的个数等于“1”的个数时，CRD状态保持不变。

当CRD状态为正时，PCIe链路进行 $8 / 10\mathrm{b}$ 编码时使用 $\mathrm{CRD + }$ ，否则使用CRD－以维持PCIe链路的DC均衡。在PCIe总线中，数据字符使用的 $8 / 10\mathrm{b}$ 编码的格式如表7-5所示。

表 7-5 数据字符的 $8/{10}\mathrm{\;b}$ 编码

<table><tr><td>数据字符</td><td>Data Byte</td><td>HGF EDCBA</td><td>CRD - abcdei fghi</td><td>CRD + abcdei fghi</td></tr><tr><td>D0.0</td><td>0x00</td><td>000 00000</td><td>100111 0100</td><td>011000 1011</td></tr><tr><td>D1.0</td><td>0x01</td><td>000 00001</td><td>011101 0100</td><td>100010 1011</td></tr><tr><td>D2.0</td><td>0x02</td><td>000 00010</td><td>101101 0100</td><td>010010 1011</td></tr><tr><td>D3.0</td><td>0x03</td><td>000 00011</td><td>110001 1011</td><td>110001 0100</td></tr><tr><td>D4.0</td><td>0x04</td><td>000 00100</td><td>110101 0100</td><td>001010 1011</td></tr><tr><td>D5.0</td><td>0x05</td><td>000 00101</td><td>101001 1011</td><td>101001 0100</td></tr><tr><td>D6.0</td><td>0x06</td><td>000 00110</td><td>011001 1011</td><td>011001 0100</td></tr><tr><td>D7.0</td><td>0x07</td><td>000 00111</td><td>111000 1011</td><td>000111 0100</td></tr><tr><td colspan="5">...</td></tr><tr><td>D1.1</td><td>0x21</td><td>001 00001</td><td>011101 1001</td><td>100010 1001</td></tr><tr><td>D2.1</td><td>0x22</td><td>001 00010</td><td>101101 1001</td><td>010010 1001</td></tr><tr><td>D3.1</td><td>0x23</td><td>001 00011</td><td>110001 1001</td><td>110001 1001</td></tr><tr><td colspan="5">...</td></tr><tr><td>D3.5</td><td>0xA3</td><td>101 00011</td><td>110001 1010</td><td>110001 1010</td></tr><tr><td colspan="5">...</td></tr><tr><td>D23.7</td><td>0xF7</td><td>111 10111</td><td>111010 0001</td><td>000101 1110</td></tr><tr><td>D24.7</td><td>0xF8</td><td>111 11000</td><td>110011 0001</td><td>001100 1110</td></tr><tr><td>D25.7</td><td>0xF9</td><td>111 11001</td><td>100110 1110</td><td>100110 0001</td></tr><tr><td>D26.7</td><td>0xAA</td><td>111 11010</td><td>010110 1110</td><td>010110 0001</td></tr><tr><td>D27.7</td><td>0xFB</td><td>111 11011</td><td>110110 0001</td><td>001001 1110</td></tr><tr><td>D28.7</td><td>0xFC</td><td>111 11100</td><td>001110 1110</td><td>001110 0001</td></tr><tr><td>D29.7</td><td>0xFD</td><td>111 11101</td><td>101110 0001</td><td>010001 1110</td></tr><tr><td>D30.7</td><td>0xFE</td><td>111 11110</td><td>011110 0001</td><td>100001 1110</td></tr><tr><td>D31.7</td><td>0xFF</td><td>111 11111</td><td>101011 0001</td><td>010100 1110</td></tr></table>

PCIe总线还定义了一系列控制字符，这些字符从“Data Byte”的角度来看和数据字符完全相同，但是使用的CRD + 和CRD - 编码和数据字符不同。如数据字符D30.7和K30.7所对应的“Data Byte”都为0xFE（111 11110），但是CRD - 编码分别为0111100001和0111101000，而 $\mathrm{CRD}+$ 编码为1000011110和1000010111。

控制字符使用的8/10b编码的格式如表7-6所示。PCIe总线使用这些字符编码作为控制命令，和数据进行区别。

表 7-6 控制字符的 8/10b 编码

<table><tr><td>数据字符</td><td>Data Byte</td><td>HGF EDCBA</td><td>CRD - abcdei fghi</td><td>CRD + abcdei fghi</td></tr><tr><td>K28.0</td><td>0x1C</td><td>000 11100</td><td>001111 0100</td><td>110000 1011</td></tr><tr><td>K28.1</td><td>0x3C</td><td>001 11100</td><td>001111 1001</td><td>110000 0110</td></tr><tr><td>K28.2</td><td>0x5C</td><td>010 11100</td><td>001111 0101</td><td>110000 1010</td></tr><tr><td>K28.3</td><td>0x7C</td><td>011 11100</td><td>001111 0011</td><td>110000 1100</td></tr><tr><td>K28.4</td><td>0x9C</td><td>100 11100</td><td>001111 0010</td><td>110000 1101</td></tr><tr><td>K28.5</td><td>0xBC</td><td>101 11100</td><td>001111 1010</td><td>110000 0101</td></tr><tr><td>K28.6</td><td>0xCD</td><td>110 11100</td><td>001111 0110</td><td>110000 1001</td></tr><tr><td>K28.7</td><td>0xFC</td><td>111 11100</td><td>001111 1000</td><td>110000 0111</td></tr><tr><td>K23.7</td><td>0xF7</td><td>111 10111</td><td>111010 1000</td><td>000101 0111</td></tr><tr><td>K27.7</td><td>0xFB</td><td>111 11011</td><td>110110 1000</td><td>001001 0111</td></tr><tr><td>K29.7</td><td>0xFD</td><td>111 11101</td><td>101110 1000</td><td>010001 0111</td></tr><tr><td>K30.7</td><td>0xFE</td><td>111 11110</td><td>011110 1000</td><td>100001 0111</td></tr></table>

这些控制字符在PCIe总线中的定义如表7-7所示

表 7-7 控制字符的说明

<table><tr><td>数据字符</td><td>缩 写</td><td>符 号 名</td><td>说 明</td></tr><tr><td>K28.0</td><td>SKP</td><td>Skip</td><td>用于补偿 PCIe 链路不同 Lane 的延时，PCIe 总线的物理层收到该控制序列时，LFSR 不进行移位操作</td></tr><tr><td>K28.1</td><td>FTS</td><td>FTS (Fast Training Sequence)</td><td>在链路训练的 FTS 序列中使用</td></tr><tr><td>K28.2</td><td>SDP</td><td>Start DLLP</td><td>DLLP 的起始标记</td></tr><tr><td>K28.3</td><td>IDL</td><td>Idle</td><td>在 EIOS (Electrical Idle OrderedSet) 序列中使用</td></tr><tr><td>K28.4</td><td></td><td></td><td>保留</td></tr><tr><td>K28.5</td><td>COM</td><td>Comma</td><td>复位 PCIe 链路的 LFSR 为初始值</td></tr><tr><td>K28.6</td><td></td><td></td><td>保留</td></tr><tr><td>K28.7</td><td>EIE</td><td>Electrical Idle Exit</td><td>在 EIEOS (Electrical Idle Exit Sequence) 序列中使用</td></tr><tr><td>K23.7</td><td>PAD</td><td>Pad</td><td>填充字符</td></tr><tr><td>K27.7</td><td>STP</td><td>Start TLP</td><td>TLP 的起始标志</td></tr><tr><td>K29.7</td><td>END</td><td>End</td><td>TLP 和 DLLP 的结束标志</td></tr><tr><td>K30.7</td><td>EDB</td><td>EnD Bad</td><td>无效 TLP 的结束标志</td></tr></table>

下文以物理层发送一个TLP的实例说明，PCIe链路如何使用这些编码。一个TLP在通过物理层时，首先要加入物理层的前后缀，分别为STP和END。加入这些前后缀后的TLP报文格式如图7-15所示。

![[pci_express/009ee179353979f189e6f4f1c7bb9a4f6de697141b3e9de448a2dbb12e31b870.jpg]]  
图7-15 物理层TLP的格式

TLP 在通过物理层时首先在其前后加入 STP 和 END 控制字符，这两个控制字符分别为 K27.7 和 K29.7（如表 7-7 所示），它们通过物理层时，不需要进行加接扰操作。数据链路层前缀、TLP 和数据链路层后缀都属于数据字符，这些字符在通过物理层时需要进行加接扰操作，之后从表 7-5 中获得字符流，并由物理层发向 PCIe 链路。

值得注意的是，控制字符和数据字符需要根据物理层CRD状态，决定是使用 $\mathrm{CRD}+$ 还是CRD-编码。PCIe链路的两端在进行加解扰操作时，需要保证其使用的LFSR寄存器同步。LFSR寄存器的同步由控制字符COM决定，在初始复位时LFSR寄存器的初始值为0xFFFF，当收到控制字符COM后，物理层将LFSR寄存器的初始值置为0xFFFF，此外物理层收到控制字符SKP后，并不会对LFSR寄存器进行移位操作。

# 7.4 小结

本章重点介绍了数据链路层的状态，以及 ACK/NAK 协议，并简要介绍了 PCIe 总线的物理层。其中 PCIe 总线的物理层非常重要，深入理解物理层是深入理解 PCIe 体系结构的要点。在第 8 章讲述的内容以此为基础。

