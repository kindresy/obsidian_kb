---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "12"
section: "（1）TLP传递的数据区域不能跨越4KB边界。"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# （1）TLP传递的数据区域不能跨越4KB边界。

为此Capric卡首先需要分析 $\mathrm{A}\sim \mathrm{B}$ 这段是否跨越4KB边界。值得注意的是，在Capric卡中，一次DMA写的长度小于2048，因此其传递的数据区域至多会跨越一个4KB边界。此时需要向 $\mathrm{A}\sim \mathrm{Tail}_{4096}(\mathrm{A})$ 和 $\mathrm{Head}_{4096}(\mathrm{B})\sim \mathrm{B}$ 这两段数据区域进行写操作，而且这两段数据区域的M都可能大于 $0\mathrm{x}20$ 。下文采用的拆包方法可以保证在不进行4KB边界检查的情况下，保证拆分后的TLP不会跨越4KB边界。

# (2）尽量减少拆分后TLP的总个数。

比如，可以将 $0\mathrm{x}1000\sim 10\mathrm{FF}$ 这段数据区域拆分为 $0\mathrm{x}1000\sim 107\mathrm{F}$ 和 $0\mathrm{x}1080\sim 0\mathrm{x}10\mathrm{FF}$ ，尽量利用Max\_Payload\_Size参数，而不使用更多的TLP进行数据传递。

(3) 拆分后的 TLP 尽量不跨越 Cache 行边界。虽然 PCIe 总线规范并没有规定拆分 TLP 的方法。但是将 $0 \times 1000 \sim 10\mathrm{FF}$ 这段数据区域拆分为 $0 \times 1000 \sim 0 \times 107\mathrm{E}$ , $0 \times 107\mathrm{F} \sim 0 \times 108\mathrm{F}$ 和 $0 \times 1090 \sim 0 \times 10\mathrm{FF}$ , 从原理上讲是可行的, 可是并不合理。

在Capric卡中，为了简化设计，当M大于0x20时将采用以下规则进行拆包处理。

- 第一个 TLP 的起始地址必须为 $0 \times \mathrm{A}_{31} \mathrm{~A}_{30} \dots \mathrm{A}_{2}$ , 而其他 TLP 的起始地址必须为 0x80 字节对界。  
- 最后一个 TLP 的结束地址必须为 $0 \times \mathrm{B}_{31} \mathrm{~B}_{30} \dots \mathrm{B}_{2}$ , 而其他 TLP 的结束地址必须为 0x80 字节对界。

根据以上规则，我们可以将 $\mathrm{A}_{31}\mathrm{A}_{30}\dots \mathrm{A}_1\mathrm{A}_0\sim \mathrm{B}_{31}\mathrm{B}_{30}\dots \mathrm{B}_1\mathrm{B}_0$ 这段数据区域划分为多个数据区域，其中每一个区域的Length字段不超过 $0\mathrm{x}20$ ，而且每段数据区域以 $128\mathrm{~B}$ 对界。

$$
\begin{array}{l} \mathrm {A} _ {3 1} \mathrm {A} _ {3 0} \dots \mathrm {A} _ {1} \mathrm {A} _ {0} \sim \text {T a i l} _ {1 2 8} (\mathrm {A}) \\ \operatorname {H e a d} _ {1 2 8} (\mathrm {A} + 1 2 8) \sim \operatorname {T a i l} _ {1 2 8} (\mathrm {A} + 1 2 8) \\ \end{array}
$$

··

$$
\operatorname {H e a d} _ {1 2 8} (\mathrm {A} + \mathrm {n} \times 1 2 8) \sim \operatorname {T a i l} _ {1 2 8} (\mathrm {A} + \mathrm {n} \times 1 2 8)
$$

···

$$
\operatorname {H e a d} _ {1 2 8} (\mathrm {B}) \sim \mathrm {B} _ {3 1} \mathrm {B} _ {3 0} \dots \mathrm {B} _ {1} \mathrm {B} _ {0}
$$

以上这些数据区域的M都小于或等于 $0\mathrm{x}20$ ，而且都不会跨越128B边界，因此也不可能跨越4KB边界。向这些数据区域传送数据时，TLP各字段的设置参见M小于或等于 $0\mathrm{x}20$ 的情况。当Capric卡将这些存储器写请求TLP发送完毕后，可以向处理器提交中断请求。

# 12.2.2 DMA 读使用的 TLP

与DMA写模块相比，DMA读模块的逻辑设计较为复杂。在PCIe总线中，存储器写TLP使用Posted总线传送方式，实现DMA写操作只需要使用存储器写TLP即可。而PCIe总线使用Split总线传送方式进行存储器读操作。因此一个DMA读过程由EP向RC发送“存储器读请求TLP”，之后再由RC使用“存储器读完成TLP”将数据传递给EP。

当软件启动DMA读操作后，DMA控制逻辑将根据需要读取数据区域的大小，决定发送存储器读请求TLP的个数，如果所读取数据区域的实际长度超过Max\_Read\_Request\_Size参数时，DMA控制逻辑需要进行拆包处理，向RC发送多个存储器读请求TLP，这些存储器读请求TLP将使用不同的Tag。

当RC收到这些存储器读请求TLP后，将使用存储器读完成TLP，将数据传递给Capric卡。其中一个存储器读请求TLP（Tag不同的报文）可能对应多个存储器读完成TLP，而且这些存储器读完成TLP可以乱序到达。

在Capric卡的DMA读模块的设计中，首先需要进行拆包处理，其次需要合理地管理Tag资源，而最值得注意的是对存储器读完成TLP的乱序处理。为此在Capric卡中设置了一个单向循环链表tag\_queue，以便Capric卡发送存储器读请求TLP，进行Tag资源的管理，并处理存储器读完成TLP的序。

# 1. tag\_queue

Capric卡的DMA读模块使用了一个单向循环队列tag\_queue，当然设计者也可以使用其他逻辑实现同样的功能。实际上对于Capric卡而言，设置这样的循环队列是奢侈的，因为

Capric 卡仅实现了基本的 DMA 读写操作。该循环队列实际上是为笔者的另一个设计，即 Cornus 卡 $\ominus$ 准备的。

在tag\_queue队列中，设置了头尾指针，分别为tag\_front和tag\_rear，Capric卡使用8位寄存器存放这两个指针。该队列的每一个Entry对应一个tag资源，其Entry号与Tag号一一对应。DMA读模块从tag\_rear指针获得当前可以使用的tag资源（相当于将获得的tag字段加入tag\_queue中），并从tag\_front指针处释放tag资源（相当于从tag\_queue的头部释放资源），在tag\_front和tag\_rear之间的Entry保存正在使用的tag资源。tag\_queue队列的组成结构如图12-3所示。

在Capric卡复位时，tag\_front与tag\_rear指针同时指向Entry0，此时tag\_queue为空。当Capric卡需要使用tag资源时，首先判断tag\_queue是否为满，如公式12-6所示。

$$
\left(t a g \_ r e a r + 1\right) \bmod 2 5 6 = t a g \_ f r o n t \tag {12-6}
$$

当tag\_queue队列不满时，Capric卡可以从tag\_queue中获得tag资源，其值等于tag\_rear，然后将tag\_rear更新为（ $\mathrm{tag\_rear + 1}$ ）mod256（相当于将获得的tag加入到tag\_queue队列中)。当Capric卡释放tag资源时，需要判断tag\_queue是否为空，如公式12-7所示。

$$
t a g \_ r e a r = t a g \_ f r o n t \tag {12-7}
$$

在Capric卡中，到达的存储器读完成TLP因为乱序的原因，其tag字段不一定与tag\_front相等。Capric卡错误处理逻辑需要判断到达的存储器读完成TLP的tag字段是否在tag\_front和tag\_rear之间（在图12-3d中，阴影部分的Entry在当前tag\_queue中有效），其判断条件如公式12-8所示。

$$
(t a g - t a g \_ f r o n t) \bmod 2 5 6 <   (t a g \_ r e a r - t a g \_ f r o n t) \bmod 2 5 6 \tag {12-8}
$$

在 Cornus 卡中，tag\_queue 队列的 Entry 由许多字段组成，在这些字段中“L”和“U”位对于 Capric 卡有意义。其中 U 为 Used 位，当该位为 1 时，表示对应 Entry 正在被使用，为 0 时表示没有被使用；而 L 为 Last 位，当该位为 1 时，表示对应 Entry 保存 DMA 操作最后一个存储器读请求 TLP。这些位的详细解释见下文。

![[pci_express/c5435637b7c14a040a2864b29cacbbca4a0add79ff4982bc1a583f27176041ec.jpg]]

![[pci_express/a7d06dee282c5498350e66b897f331fb37514e75d29dc3d0711955ad86d1759b.jpg]]

![[pci_express/b9c4fe8e8009778e6843e878fe7bc517faf14fc30bc6282e4f5f661b0a735ffa.jpg]]

![[pci_express/9a0cc6e9eab7338c52d5717941fde2e871c2f01d0dda860e7af7fdcd85eba7e7.jpg]]  
图12-3 循环队列tag\_queue的组成结构  
a）tag\_queue的初始化b）tag\_queue为空c）tag\_queue为满d）tag\_queue中的有效Entry

# 2. Capric 卡发送存储器读请求 TLP

Capric卡发送存储器读请求TLP与发送存储器写TLP的步骤较为类似，只是存储器读请求不含有Data Payload，存储器读请求TLP的格式如图6-8所示。与存储器写请求TLP相

比，存储器读报文多了两个字段分别为 Requester ID 字段和 Tag 字段，这两个字段合称为 Transaction ID 字段，该字段的结构如图 6-9 所示。

从第6.2.1节中，可以获知存储器读请求TLP使用地址路由方式，Capric卡将存储器读请求TLP发送给RC时，并不需要使用Transaction ID字段进行ID路由。但是存储器读完成TLP需要使用ID路由方式进行传送，因而需要使用Capric卡的Transaction ID字段将存储器读完成TLP发送给Capric卡。

在PCIe总线中，每一个数据传送都有唯一的Transaction ID，Transaction ID由Requester ID和Tag字段组成，其中Requester ID由HOST处理器在系统初始化时设置。Capric卡需要记录这个Requester ID，以便传送存储器读请求TLP。

在存储器读请求 TLP 中其他字段的设置与存储器写请求 TLP 类似，这些字段的设置参见上文。其中存储器读请求 TLP 的 Fmt 字段为 0b00/0b01，表示使用 3DW/4DW 的 TLP 头，而且不带数据；而 Type 字段与存储器写请求 TLP 的 type 字段相等，都为 0b0000。在 PCIe 总线中，存储器写报文一定带有数据，而存储器读请求一定不带数据，这是 PCIe 总线区分存储器读请求 TLP 和存储器写 TLP 的方法。在设计中，Capric 卡使用 4DW 的读请求 TLP 头，因为 Capric 卡内部使用 64b 数据总线，使用 4DW 的报文头便于对界处理；而存储器读完成报文只能使用 3DW 的报文头，这为 Capric 卡的设计也带来了一些困难。

在存储器读请求 TLP 中，需要重点处理的字段是 Tag。在 PCIe 总线中，每个设备都有唯一的 Requester ID，而且每一次数据传送使用不同的 Transaction ID，在一次数据传送没有完成之前，其他数据传送不能使用相同的 Transaction ID。在 PCIe 总线中，使用 Tag 字段区分不同的 Transaction ID，因为对于同一个 PCIe 设备发出的 TLP，Requester ID 字段都是相同的，只有 Tag 字段不同。

当Capric卡向RC发送存储器读请求TLP时，将从tag\_queue中选择一个未用的Tag资源；当Capric卡收齐与“存储器读请求TLP”对应的“存储器读完成TLP”后，将释放这个Tag资源，之后其他存储器读请求TLP可以使用这个Tag。

为此Capric卡需要使用一组数据缓冲维护这个Tag字段。在PCIe总线中，Tag字段为5或者8位，如果使能了Phantom功能，一个PCIe设备可以使用更多的Tag资源，详见第4.3.2节。在Capric卡中，并没有使能Phantom功能，而且使用的Tag字段为8位，即DeviceCapability寄存器的Extended Tag Field Supported位为1，该寄存器的详细描述见第4.3.2节。

Capric卡使用tag\_rear指针从tag\_queue队列中获得未用的tag资源，并设置tag\_queue中对应Entry的L和U位。Capric卡发送存储器读请求TLP时，首先需要判断本次DMA读操作一共需要向RC发送几个存储器读请求TLP。在Capric卡中，一次DMA读可以使用的最大传送单位为2047B，该值超过Capric卡的Max\_Read\_Request\_Size参数（512B），因此Capric卡发送存储器读请求TLP时需要进行拆包操作。

如果Capric卡读取 $\mathrm{A(A_{31}A_{30}\dots A_1A_0)\sim B(B_{31}B_{30}\dots B_1B_0)}$ 这段数据区域时，需要首先计算每段数据区域的实际长度M，该长度的计算与公式12-4相同。如果M小于或等于512B，需要继续检查这段数据区域是否超过4KB边界，如果超过需要将这段数据区域分为A～

Tail<sub>4096</sub>(A) 和 $\mathrm{Head}_{4096}(\mathrm{B}) \sim \mathrm{B}$ 这两段数据区域进行读操作。

如果 M 大于 512 B, Capric 卡需要进行拆包处理, 将 A \~ B 这段数据区域分割为若干个数据区域, 其中每一段数据区域的 Length 字段不超过 0x80 , 而且为 512B 对界。Capric 卡使用的拆包方法如下所示。

第1个存储器读请求TLP对应的数据区域： $\mathrm{A}_{31}\mathrm{A}_{30}\dots \mathrm{A}_1\mathrm{A}_0\sim \mathrm{Tail}_{512}(\mathrm{A})$

第2个存储器读请求TLP对应的数据区域： $\mathrm{Head}_{512}(\mathrm{A} + 512)\sim \mathrm{Tail}_{512}(\mathrm{A} + 512)$

···

第 $\mathbf{n}$ 个存储器读请求TLP对应的数据区域： $\mathrm{Head}_{512}(\mathrm{A} + \mathrm{n}\times 512)\sim \mathrm{Tail}_{512}(\mathrm{A} + \mathrm{n}\times 512)$

···

最后1个存储器读请求TLP对应的数据区域： $\mathrm{Head}_{512}(\mathrm{B})\sim \mathrm{B}_{31}\mathrm{B}_{30}\dots \mathrm{B}_1\mathrm{B}_0$

以上这些数据区域的M都小于0x80，而且都不会跨越512B边界，因此也不可能超过4KB边界。使用以上拆包方法，Capric卡可以获得若干个M小于或等于0x80的数据区域，因此Capric卡可以使用一个存储器读请求从主存储器获得以上每段数据区域对应的数据。Capric卡使用4DW的TLP头，其格式如图12-4所示。

![[pci_express/59d5ed66aa18cd556b64c884407576e71fc4ce819fb040d6abe5216b21e78576.jpg]]  
图12-4 存储器读请求TLP头格式

Capric卡向RC发送这些存储器读请求TLP的详细步骤如下。

（1）组织存储器读请求TLP，其中Byte0中的字段、First/LastDWBE字段和Address字段与存储器写请求TLP的对应字段相同，本小节对此不做详细描述。而RequesterID字段由Host处理器在初始化时设置。  
(2) Capric 卡从 tag\_queue 队列中获得 tag 字段，首先通过公式 12-6 判断 tag\_queue 队列是否有可用 tag 资源，如果没有则循环等待公式 12-6 成立；如果有可用 tag，则继续。在 Capric 卡中，Address[63:2] 较小的存储器读请求使用的 Tag 字段也较小。  
(3) 当前存储器读请求使用 tag\_rear 作为 tag 字段，同时置 tag\_queue[ tag\_rear].U 为 1，表示当前 Entry 已被使用。  
(4) 在一次 DMA 读操作时, Capric 卡可能需要进行拆包操作。如果当前存储器读请求 TLP 是最后一个 TLP, 则将 tag\_queue[ tag\_rear].L 位置为 1 , 否则置为 0 。  
(5) 将 tag\_rear 赋值为（tag\_rear + 1）mod 256，然后发送该存储器读请求 TLP。如果 L 位为 0 时转（1），表示与当前 DMA 操作对应的存储器读请求 TLP 还没有发送完毕；否则结束存储器读请求报文的发送。

Capric卡发送存储器读请求TLP时，还需要考虑一个细节问题。在PCIe总线中，EP为

CplH 和 CplD 提供的 Credit 为 0，即 Infinite Credit，详见第 9.3.2 节。这意味着 EP 每发送一个存储器读请求，必须为对应的存储器读完成的报文头和数据预留缓冲。

假设Capric卡连续向RC发送了256个存储器读请求TLP，其中每个存储器读请求TLP访问的数据区域为512B，而在RC发送的存储器读完成TLP中一次只能携带64B。此时即便不考虑对界的问题，Capric卡也需要为存储器读完成预留较大的缓冲空间，该空间由两部分组成，如下所示。

（1）预留存储器读完成TLP头的空间大小为 $8192\mathrm{B}$ （ $256\times 512\times 4 / 64)$ 。  
(2）预留存储器读完成TLP数据的空间大小 $128\mathrm{KB}$ （ $256\times 512$ B）。

在硬件设计中，为了提高DMA读的数据传送效率，还可以使能Phantom功能，此时EP能够发送的存储器读请求TLP更多。如果EP经过若干级Switch后，才能到达RC，此时RC可能正在处理其他EP的存储器读请求而不会立即处理这些存储器读请求，此时该EP可能长时间不能收到存储器读完成TLP，从而无法释放预留的数据缓冲。因此EP可能会因为没有数据缓冲，而无法继续发送存储器读请求TLP。

实际上，硬件为存储器读完成报文预留的数据缓冲是有限的，一般不会预留 $136\mathrm{KB}$ 大小的空间，在LogiCORE中，为CplH预留的缓冲单元为 $33\sim 36$ 个，而为CplD中的数据预留的缓冲为 $2176\sim 2304\mathrm{B}$ 。

由此可以发现如果 RC 没有及时地将存储器读完成 TLP 发送回来，Capric 卡最多在连续发送 33 个存储器读请求后（假设存储器读请求使用 4DW 的报文头），就因为无法为存储器读完成的报文头提供足够的缓冲而不能继续发送；此外如果每个存储器读请求所访问的数据区域都是 $512 \mathrm{~B}$ ，Capric 卡最多在连续发送 4 个这样存储器读请求后，就不能继续发送这样的存储器读请求，从而造成发送流水线的中断。

由此可以发现，在LogiCORE中，由于预留的缓冲有限，Capric卡在使用PCIe总线要求的InfiniteCredit机制时，将因为预留缓冲不足而造成流水线的中断。为此LogiCORE提供了三种流量控制机制，这三种流量控制机制在重构LogiCORE时选择使用，系统软件不能通过修改寄存器动态配置这些流控方式。

# (1) Infinite Credit

该方式与PCIe总线规范兼容。如果Capric卡使用InfiniteCredit方式，当LogiCORE内部的接收缓冲不足时，Capric卡不能向RC发送存储器读请求报文。根据上文的讨论，由于LogiCORE内部的接收缓冲不足，因此使用该方式在某种程度上，将造成DMA读流水线的中断，从而影响DMA读的效率。

# (2) One Posted/Non-Posted Header

该方式与PCIe总线规范不兼容，使用这种方式时，EP每次为上游端口发送的Posted请求和Non-Posted请求提供最小的Credit，相当于EP每一次只能发送一个存储器读请求TLP，而得到与之对应的存储器读完成TLP后，再提交下一个存储器读请求TLP。使用这种方法将严重影响DMA读的效率。LogiCORE提供的这种方法可能是用于调试目的。在正常情况下设计者不应该使用这种方式。

# (3) Non-Infinite Credit

该方式与PCIe总线规范不兼容，使用这种方式时，EP并没有给上游端口提供无限量的Credit，而是根据预留接收缓冲的实际使用情况，为上游端口提供Credit。Capric卡采用这种

方式，发送存储器读请求 TLP 时，并不会为存储器读完成 TLP 事先预留接收缓冲，从而在发送存储器读请求 TLP 时，并不会因为接收缓冲不足而被中断，因此提高了 Capric 卡发送存储器读请求 TLP 的效率。

LogiCORE 在接收存储器读完成报文时，将根据预留缓冲的实际大小为对端提供 Credit。虽然采用这种方法与 PCIe 总线规范要求的 Infinite Credit 并不兼容。但是使用这种方法避免了在发送存储器读请求时，因为接收缓冲不足而引发的流水线中断，从而 Capric 卡可以连续发送多个存储器读请求，无论是否具有足够的接收缓冲。

而Capric卡将以较快的速度从LogiCORE的预留缓冲中获得数据，因此在多数时间里，不会因为预留缓冲被对端设备耗尽而引发接收流水线的中断。因此在实际设计中，Capric卡使用了这种流量控制方式。

# 3. Capric卡接收存储器读完成 TLP

在PCIe总线中，EP发出的存储器读请求可以超越之前的存储器读请求，而且当存储器完成报文使用的Transaction ID不同时，存储器读完成TLP也可能超越之前的存储器读完成TLP，这将造成存储器读完成TLP乱序到达Capric卡。

Capric卡必须注意处理这个乱序问题。下面举例说明这个序的问题，假设Capric卡向处理器的 $0\mathrm{x}1000\sim 0\mathrm{x}11\mathrm{FF}$ 这段数据区域发送存储器读请求TLP，RC将通过存储器读完成TLP向Capric卡传递数据。如果这个RC使用的RCB为 $64\mathrm{B}$ ，则RC可以使用4个存储器读完成TLP发送这些数据，如图12-5所示。

![[pci_express/25b01a4bd56a392f953fec4c4fec31a4defcce66688320be8f7c61ed94e7726d.jpg]]  
图12-5 使用一个存储器读TLP对 $0\mathrm{x}100\sim 0\mathrm{x}11\mathrm{FF}$ 进行DMA读操作

其中第1个存储器读完成TLP的数据来自 $0\mathrm{x}1000\sim 0\mathrm{x}107\mathrm{F}$ 这段数据区域；第2个存储器读完成TLP的数据来自 $0\mathrm{x}1080\sim 0\mathrm{x}10\mathrm{FF}$ 这段数据区域；第3个存储器读完成TLP的数据来自 $0\mathrm{x}1100\sim 0\mathrm{x}117\mathrm{F}$ 这段数据区域；而第4个存储器读完成TLP的数据来自 $0\mathrm{x}1180\sim$ $0\mathrm{x}11\mathrm{FF}$ 这段数据区域。在这种情况下，存储器读完成报文将按序到达Capric卡，从而并不会对Capric卡的硬件逻辑造成影响。

如果Capric卡使用2个存储器读请求TLP向处理器的 $0\mathrm{x}1000\sim 0\mathrm{x}11\mathrm{FF}$ 这段数据区域发起存储器读请求。其中第1个存储器读请求TLP（tag0）向处理器的 $0\mathrm{x}1000\sim 0\mathrm{x}10\mathrm{FF}$ 这段数据区域发起存储器读请求，而第2个存储器读请求TLP（tag1）向处理器的 $0\mathrm{x}1100\sim$ $0\mathrm{x}11\mathrm{FF}$ 这段数据区域发起存储器读请求。此时来自RC的存储器读完成报文可能乱序到达Capric卡，如图12-6所示。

此时 RC 依然使用 4 个存储器读完成 TLP 向 Capric 卡发送这些数据，但是由于序的问题，这 4 个存储器读完成 TLP 可能以 2 种不同的顺序发向 Capric 卡。当然 RC 还可以以其他顺序向 Capric 卡发送这些 TLP，本节并不列出所有可能的顺序。

# (1) 第 1 种序

\- 第1个存储器读完成TLP的数据来自 $0\mathrm{x}1000\sim 0\mathrm{x}107\mathrm{F}$ 这段数据区域（tag0）。

![[pci_express/234a23dd007440cd5b70bfe2f0e36778c337b2b03c234ab034d1f203bd6fd797.jpg]]  
图12-6 使用两个存储器读TLP对 $0\mathrm{x}100\sim 0\mathrm{x}11\mathrm{FF}$ 进行DMA读操作

- 第2个存储器读完成TLP的数据来自 $0 \times 1080 \sim 0 \times 10\mathrm{FF}$ 这段数据区域（tag0）。  
- 第3个存储器读完成TLP的数据来自 $0\mathrm{x}1100\sim 0\mathrm{x}117\mathrm{F}$ 这段数据区域（tag1）。  
- 第4个存储器读完成TLP的数据来自 $0\mathrm{x}1180\sim 0\mathrm{x}11\mathrm{FF}$ 这段数据区域（tag1）。

# (2）第2种序

- 第1个存储器读完成TLP的数据来自 $0\mathrm{x}1100\sim 0\mathrm{x}117\mathrm{F}$ 这段数据区域（tag1）。  
- 第2个存储器读完成TLP的数据来自 $0 \times 1000 \sim 0 \times 107\mathrm{F}$ 这段数据区域（tag0）。  
- 第3个存储器读完成TLP的数据来自 $0\mathrm{x}1180\sim 0\mathrm{x}11\mathrm{FF}$ 这段数据区域（tag1）。  
- 第4个存储器读完成TLP的数据来自 $0\mathrm{x}1080\sim 0\mathrm{x}10\mathrm{FF}$ 这段数据区域（tag0）。

这个乱序问题为Capric卡的DMA读机制的设计带来了不小的麻烦。因为在“第2种序”的情况下，先发出去的存储器读请求TLP，后接收到与之对应的存储器完成报文。不过值得庆幸的是，对于一个存储器读请求TLP，其对应的存储器完成报文虽然也有多个，但是这些报文将以地址顺序先后到达。如向 $0\mathrm{x}1000\sim 0\mathrm{x}10\mathrm{FF}$ 这段数据区域发送的存储器读请求，其存储器完成报文虽然被分解为两个，但一定是传送 $0\mathrm{x}1000\sim 107\mathrm{F}$ 这段区域的存储器读完成TLP率先到达，而传送 $0\mathrm{x}1080\sim 0\mathrm{x}10\mathrm{FF}$ 这段区域的存储器读完成TLP随后到达。

在Capric卡的设计中必须考虑这个乱序问题，因为Capric卡进行DMA读操作时，所读取的数据区域可能超过Max\_Read\_Request\_Size参数，此时Capric卡对这段数据区域进行DMA读时，必须向RC发出多个存储器读请求TLP，参见上文。

与Capric卡发送存储器读请求TLP相比，Capric卡处理存储器读完成TLP的过程更为复杂。当Capric卡收到来自RC的存储器完成报文后，需要进行一系列检查。存储器读完成TLP的格式如图12-7所示。

Capric卡接收到存储器读完成TLP后，首先需要检查报文头。其中Fmt字段必须为0b010，Type字段必须为0b01010。除此之外Capric卡还需要进行以下检查。

(1) 存储器读完成 TLP 的 Requester ID 字段必须与 Capric 卡的 Requester ID 字段相等。否则该存储器读完成 TLP 被认为是 “Unexpected Completion” 报文, Capric 卡需要丢弃该存储器读完成 TLP, 并将 ERR 寄存器的 UC 位置 1。  
(2) 检查存储器读完成 TLP 的 Status 字段, 如果 Status 字段不为 0b000 , 则表示接收到的 TLP 出现错误。如果 Status 字段为 0b001 或者 0b100 时, Capric 卡需要丢弃该存储器读完成 TLP, 并将 ERR 寄存器的相应位置 1 。  
(3) 检查存储器读完成 TLP 的 Tag 字段, 确认当前报文是否与已经发出的存储器读请求

![[pci_express/c2ed6c7d861ab26475b69e4f2dc6e985845148ba8975165ee22b48c1ded226c7.jpg]]  
图12-7 存储器读完成TLP

TLP 对应，检查方法如公式 12-8 所示。

(4) 此外 Capric 卡还需要检查 EP 位, TD 位、TC 字段和 Attr 字段。

Capric卡的接收部件成功完成这些检查之后，将从存储器读完成TLP中获取数据。PCIe总线规定，一个存储器读请求TLP，可以对应多个存储器读完成TLP。这为Capric卡的设计带来了一定的困难，为此Capric卡需要将存储器读完成TLP全部收齐后，才能释放相应的Tag资源，最后将tag\_queue对应Entry的U位和L位清零。

存储器读完成报文虽然可能有多个，但是这些报文将以地址顺序先后到达。因此Capric卡首先需要分析Tag字段，从而确定当前存储器读完成TLP与哪个存储器读请求TLP对应。其中第1个存储器读完成TLP与存储器读请求TLP起始地址对应，之后的存储器读完成TLP将地址顺序依次到达。假定向 $\left[\mathrm{A}_{31}\mathrm{A}_{30}\dots \mathrm{A}_1\mathrm{A}_0\sim \mathrm{B}_{31}\mathrm{B}_{30}\dots \mathrm{B}_1\mathrm{B}_0\right]$ 这段数据区域发起存储器读请求时，RC将发送多个存储器读完成TLP，并以下列顺序到达。

$$
\left[ \mathrm {A} _ {3 1} \mathrm {A} _ {3 0} \dots \mathrm {A} _ {1} \mathrm {A} _ {0} \sim \text {T a i l} _ {6 4} \left(\mathrm {A} _ {3 1} \mathrm {A} _ {3 0} \dots \mathrm {A} _ {1} \mathrm {A} _ {0}\right) \right]
$$

$$
\left[ \mathrm {H e a d} _ {6 4} \left(\mathrm {A} _ {3 1} \mathrm {A} _ {3 0} \dots \mathrm {A} _ {1} \mathrm {A} _ {0} + 6 4\right) \sim \text {T a i l} _ {6 4} \left(\mathrm {A} _ {3 1} \mathrm {A} _ {3 0} \dots \mathrm {A} _ {1} \mathrm {A} _ {0} + 6 4\right) \right]
$$

$$
\dots
$$

$$
\left[ \operatorname {H e a d} _ {6 4} \left(\mathrm {A} _ {3 1} \mathrm {A} _ {3 0} \dots \mathrm {A} _ {1} \mathrm {A} _ {0} + \mathrm {n} * 6 4\right) \sim \operatorname {T a i l} _ {6 4} \left(\mathrm {A} _ {3 1} \mathrm {A} _ {3 0} \dots \mathrm {A} _ {1} \mathrm {A} _ {0} + \mathrm {n} * 6 4\right) \right]
$$

$$
\dots
$$

$$
\left[ \mathrm {H e a d} _ {6 4} \left(\mathrm {B} _ {3 1} \mathrm {B} _ {3 0} \dots \mathrm {B} _ {1} \mathrm {B} _ {0}\right) \sim \mathrm {B} _ {3 1} \mathrm {B} _ {3 0} \dots \mathrm {B} _ {1} \mathrm {B} _ {0} \right]
$$

当然RC也可能向Capric发送一个存储器读完成TLP，传递 $\left[\mathrm{A}_{31}\mathrm{A}_{30}\dots \mathrm{A}_1\mathrm{A}_0\sim \mathrm{B}_{31}\mathrm{B}_{30}\right.$ ... $\mathrm{B_1B_0}]$ 数据区域中的所有数据。无论这些存储器读完成TLP以什么样的形式到达，Capric卡都需要正确接收这个存储器读完成TLP。

Capric卡首先分析存储器读完成TLP的Length字段，在该字段中存放当前存储器读完成TLP的长度，值得注意的是Length字段所存放的长度，可能超过这个存储器完成报文的包含的有效数据长度，因为地址 $\mathrm{A}_{31}\mathrm{A}_{30}\dots \mathrm{A}_1\mathrm{A}_0$ 很可能不是1DW对界，而Length字段存放的最小数据单位为1DW。此时Capric卡必须正确识别存储器读完成TLP中Data0（即第一个双字）中包含的有效数据，以及Data（Length-1）（即最后一个双字）中包含的有效数据。

在 RC 发送给 Capric 卡的多个存储器读完成 TLP 中，只有第 1 个存储器读完成 TLP 所对应的存储器区域的起始地址可能不是 DW 对界；而如果存在其他存储器读完成 TLP，那么这些报文所对应存储器区域的起始地址至少是 64B 对界的，也可能是 128B 对界的。

但是存储器读完成TLP并不含有FirstDWBE字段，此时Capric卡需要使用存储器读完成TLP中的LowerAddress字段识别Data0中的有效字节。

在第1个存储器读完成TLP中，LowerAddress[1:0] = $\mathrm{A}_1\mathrm{A}_0$ ，对于其他存储器读完成TLP，其LowAddress[1:0] = 0b00。因此通过LowerAddress字段，可以识别Data0中第一个有效数据，即Data0[A1A0]为第一个有效数据。

存储器读完成TLP并没有设置LastDWBE字段，Capric卡需要使用ByteCount和LowerAddress字段联合识别Data(Length-1)中的有效数据。如果当前存储器读完成TLP不是最后一个TLP,那么其Data(Length-1)中的数据全部有效。因为PCIe总线规定，如果RC为1个存储器读请求TLP发送多个存储器读完成TLP，如果这个存储器读完成TLP不是最后一个报文，那么其结束地址必须64B对界。

如果当前存储器读完成TLP不是第1个TLP，那么其Lower Address[1:0] = 0b00。在这两种情况下，Data（Length-1）中的有效数据较易计算。但是有一个特例情况，就是RC只发出了一个存储器读完成TLP给Capric卡，此时这个TLP既是第一个存储器读完成TLP也是最后一个存储器读完成TLP。但是无论是上述哪种方式，依然存在计算Data（Length-1）中的有效数据的通用方法，如公式12-9所示。

$$
0 b X _ {1} X _ {0} = \text {L o w A d d r e s s} [ 1: 0 ] + \text {B y t e C o u n t} [ 1: 0 ] - 0 b 0 1 \tag {12-9}
$$

其中 $Data(\text{Length} - 1)[X_1X_0]$ 为存储器读完成 TLP 中最后一个有效数据。Capric 卡计算完毕存储器读完成 TLP 的 Data0 和 Data(Length-1) 中的有效数据后，还需要判断当前存储器读完成 TLP 是不是 RC 发出的最后一个与当前 Tag 对应的存储器读完成 TLP。为直观起见，以图 12-8 为例说明如何计算当前存储器读完成 TLP 是否为最后一个报文。

![[pci_express/ad1c2d6536137809fed65a41d0d4546942ab9905e37816a8364d987fda58485f.jpg]]  
图12-8 最后一个存储器读完成TLP的判断方法

如上图所示，Start Address 为存储器读完成 TLP 的起始地址，而 End Address 为存储器读完成 TLP 的结束地址。在一个存储器读完成 TLP 中，我们无法得到 Start Address 和 End Address 的确切的数值，因为存储器读完成 TLP 不包含 Address 字段，但是可以得到阴影 A 和阴影 B 的大小。其中阴影 A 的大小为 Low Address[1:0]，而阴影 B 的大小为 $0\mathrm{b}11 - 0\mathrm{bX}_1\mathrm{X}_0$ 。

如果当前 TLP 的 Byte Count 字段加上阴影 A 和 B 的大小与 Length × 4 相等，即公式 12-10 成立时，该 TLP 为 RC 发给 Capric 卡的最后一个存储器读完成 TLP。

$$
(\text {B y t e C o u n t} + \text {L o w A d d r e s} [ 1: 0 ] + 0 b 1 1 - 0 b X _ {1} X _ {0}) = \text {L e n g t h} \ll 2 \tag {12-10}
$$

请读者重新阅读第6.3.2节，深入理解Byte Count参数的含义，以加深对公式12-10的理解。在Capric卡的硬件设计中，需要使用该公式识别最后一个到达的存储器读完成TLP。

在Capric卡接收到最后一个存储器完成TLP之后，将完成一次存储器读请求。当最后一个存储器读请求完成后，将完成一次DMA读操作。Capric卡接收存储器读完成TLP的详细步骤如下所示。

(1) 首先进行报文检查。如果通过这些检查后, 将从存储器读完成报文中获得数据填入相应 SRAM 的对应区域中。  
(2) DMA 读逻辑通过存储器读完成 TLP 的 Tag 字段在 tag\_queue 中查找对应的 Entry。如果当前存储器读完成是最后一个 TLP，将该 Entry 的 U 位清零。此时如果该 Entry 的 L 位为 1，表示本次 DMA 读结束，并向处理器提交中断请求，同时清除 L 位，并置相应的中断状态寄存器。Cormus 卡支持多路并发的 DMA 读操作，因此需要在 Entry 中设置 L 位。  
(3) DMA 读模块可能会更新 tag\_front 指针，如果 Tag 字段不等于 tag\_front 指针，读模块不能更新 tag\_front，而仅是将对应 Entry 的 U 位清零；如果相同则将 tag\_front 更新为（tag\_front + 1）mod 256，同时将 U 位清零。  
（4）之后DMA读模块继续判断tag\_queue[tag\_front]的U位是否为0。如果该位为0，将tag\_front更新为（tag\_front + 1）mod 256，然后继续判断Tag\_queue[tag\_front]的U位是否为0，直到公式12-7成立，或者Tag\_queue[tag\_front]的U位为1。

