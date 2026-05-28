---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "04"
section: "4.3.1 Power Management Capability 结构"
part: 3
tags: [pci, pci-express, computer-architecture]
---
# 4.3.1 Power Management Capability 结构

PCIe总线使用的软件电源管理机制与PCI PM（Power Management）兼容。而PCI总线的电源管理机制需要使用Power Management Capability结构，该结构由一些和PCI/PCI-X和PCIe总线的电源管理相关的寄存器组成，包括PMCR（Power Management Capabilities Register）和PMCSR（Power Management Control and Status Register），其结构如图4-15所示。

<table><tr><td colspan="2">PMCR</td><td>Pointer</td><td>ID</td></tr><tr><td>Data</td><td colspan="3">PMCSR</td></tr></table>

图4-15 Power Management Capability结构

Capability ID字段记载Power Management Capability结构的ID号，其值为0x01。在PCIe设备中，每一个Capability都有唯一的一个ID号，而Next Capability Pointer字段存放下一个Capability结构的地址。

# 1. PMCR 寄存器

PMCR寄存器由16位组成，其中所有位和字段都是只读的。该寄存器的主要目的是记录当前PCIe设备的物理属性，系统软件需要从PMCR寄存器中获得当前PCIe设备的信息后，才能对PMCSR寄存器进行修改。该寄存器的结构如图4-16所示，其中PMCR寄存器

在Power Management Capability结构的第 $3\sim 2$ 字节中。

![[pci_express/072b8225ee9ee2371dda2b1b0b5263977c24e99f42e6924144e3910c1558db05.jpg]]  
图4-16 Power Management Capabilities 寄存器

- Version 字段只读，记录 Power Management Capability 结构的版本号。  
- PME Clock 位只读，该位没有被 PCIe 总线使用，硬件逻辑必须将其接为 0。PCI 设备可以使用 PME#信号通知设备改变电源状态，在 PCI 总线中，如果 PME#信号需要使用时钟（PCI Clock）时，该位为 1；否则该位为 0。PCI 设备改变电源状态时，将 PME#信号置为有效，向处理器系统提交请求。系统软件将这个请求处理完毕后，将通知这个 PCI 设备，之后该 PCI 设备将 PME#信号置为无效。  
- RsvdP 字段为系统保留字段。  
- DSI（Device Specific Initialization）位只读。某些PCIe设备在上电时处于某种工作模式，之后可以通过重新配置运行在其他工作模式中，此时该设备需要使用DSI位表示该设备可以使用自定义的电源工作方式。  
- AUX（Auxiliary device）Current字段只读，表示PCIe设备需要使用辅助电源的电流强度。PCIe设备需要使用两种电源，一个是主电源 $\mathrm{V}_{\mathrm{ce}}$ ，另一个是辅助电源 $\mathrm{V}_{\mathrm{aux}}$ 。当PCIe设备进入某种节能状态时，主电源将停止供电，而辅助电源需要继续供电。该字段记录 $\mathrm{V}_{\mathrm{aux}}$ 使用的电流强度，其最大值为 $375\mathrm{mA}$ ，最小值为0，即不使用 $\mathrm{V}_{\mathrm{aux}}$ 。  
- D2 和 D1 位只读。D2 位为 1 表示 PCIe 设备支持 D2 状态；D1 位为 1 表示 PCIe 设备支持 D1 状态。PCI PM 机制规定 PCIe 设备可以支持四种状态，分别为 $\mathrm{D0} \sim \mathrm{D3}$ 状态。PCIe 设备处于 D0 状态时的功耗最高，处于 D3 状态时最低。多数支持电源管理的 PCIe 设备仅支持 D0 状态和 D3 状态，而 D1 和 D2 状态可选，有关这四种状态的详细说明见第 8.4.1 节。  
- PME Support 字段只读，存放 PCIe 设备支持的电源状态。第 27 位为 1 时，表示 PCIe 设备处于 D0 状态时，可以发送 PME 消息；第 28 位为 1 时，表示 PCIe 设备处于 D1 状态时，可以发送 PME 消息；第 29 位为 1 时，表示 PCIe 设备处于 D2 状态时可以发送 PME 消息；第 30 位为 1 时，表示 PCIe 设备处于 $\mathrm{D}3_{\text {hot }}$ 状态时可以发送 PME 消息；第 31 位为 1 时，表示 PCIe 设备处于 $\mathrm{D}3_{\text {cold }}$ 状态时可以发送 PME 消息。

# 2. PMCSR 寄存器

系统软件可以通过操作PMCSR寄存器，完成PCIe设备电源状态的迁移。该寄存器的结构如图4-17所示。

![[pci_express/dcfb5375cd86197d9b8c390b023e9e56ad31a265a02a3c4ffedd50ffe72c9998.jpg]]  
图4-17 Power Management Status/Control 寄存器

- Power State 字段可读写，该字段记录 PCIe 设备所处的状态。“0b00”与D0状态对应；“0b01”与D1状态对应；“0b10”与D2状态对应；“0b11”与D3状态对应。系统软件改变该字段时，PCIe设备将进行电源状态迁移。  
- No\_Soft\_Set Reset 位只读。如果该位为 1，PCIe 设备从 $\mathrm{D3}_{\mathrm{hot}}$ 状态迁移到 D0 状态时，并不需要进行内部复位操作，有关 PCIe 设备配置的现场信息可以由 PCIe 设备的硬件逻辑保存，此时当设备从 D0 状态迁移到 $\mathrm{D3}_{\mathrm{hot}}$ 状态时，不需要系统软件的干预，其现场由 PCIe 设备主动保存；而该位为 0 时，PCIe 设备从 $\mathrm{D3}_{\mathrm{hot}}$ 状态迁移到 D0 状态时，需要进行复位操作，因此系统软件在通过改变 Power State 字段使 PCIe 设备从 D0 状态迁移到 $\mathrm{D3}_{\mathrm{hot}}$ 状态之前，需要保存 PCIe 设备的相关上下文，当 PCIe 设备从 $\mathrm{D3}_{\mathrm{hot}}$ 状态迁移到 D0 状态时，再进行上下文的恢复操作。  
- PME Enable 位，可读写。该位为 1 时，PCIe 设备可以发送 PME 消息；如果为 0，不可以发出 PME 消息。当 PCIe 设备处于 D3<sub>cold</sub> 状态不能发送 PME 消息时，该位由系统软件设为 0。支持远程唤醒模式的网卡需要将此位使能。  
- Data Select 字段可读写，而 Data Scale 字段和 Data 字段只读。系统软件通过这组字段，读取 PCIe 设备处于不同状态时的功耗。首先系统软件置 Data Select 字段为 0\~7 之间的数值，其中 0 和 4 与 D0 状态对应，1 和 5 与 D1 状态对应，2 和 6 与 D2 状态对应，3 和 7 与 D3 状态对应；之后系统软件读取 Data Select 和 Data 字段并以此计算在不同状态下 PCIe 设备的功耗。其中 Data Scale 字段记录精度，为 0 时表示该 PCIe 设备不支持这组字段；为 1 时表示 Data 字段的数据需要乘以 0.1 后，才能到得 PCIe 设备的功耗，其单位为 W；为 2 时表示 Data 字段的数据需要乘以 0.01；为 3 时表示 Data 字段的数据需要乘以 0.001。  
- PME Status 位，该位只读且写 1 清除，对此位写 0 无意义。该位为 1 时表示 PCIe 设备可以正常发送 PME 消息，系统软件对此位写 1 时，将该位清除。该位由硬件逻辑控制，系统软件仅能清除该位，而不能将该位置 1。  
- PCIe 总线没有实现 B2/B3 Support 和 Bus Power/Clock Control Enable 位。在 PCI 总线中，Bus Power/Clock Control Enable 位为 1 时使能 PCI 总线的电源和时钟管理，为 0 时表示关闭；当 Bus Power/Clock Control Enable 位为 1 时，B2/B3 Support 位才有意

义，B2/B3 Support 位为 1 时表示，当 PCI 桥片处于 $\mathrm{D}3_{\mathrm{hot}}$ 状态时，这个桥片将停止为 Secondary PCI 总线提供时钟；为 0 时表示，当 PCI 桥片处于 $\mathrm{D}3_{\mathrm{hot}}$ 状态时，将停止为 Secondary PCI 总线提供电源。

# 4.3.2 PCI Express Capability 结构

PCI Express Capability 结构存放一些和 PCIe 总线相关的信息，包括 PCIe 链路和插槽的信息。有些 PCIe 设备不一定实现了 PCI Express Capability 结构中的所有寄存器，或者并没有提供这些配置寄存器供系统软件访问。

PCI Express Capability 结构的部分寄存器及其相应字段与硬件的具体实现细节相关，本节仅介绍其中一些系统软件程序员需要了解的字段。在该结构中，Cap ID 字段为 PCI Express Capability 结构使用的 ID 号，其值为 0x10。而 Next Capability 字段存放下一个 Capability 寄存器的地址。PCI Express Capability 结构由 PCI Express Capability、Device Capability、Device Control、Device Status、Link Capabilities、Link Status、Link Control、Slot Capabilities 和 Slot Status 等一系列寄存器组成。本节仅介绍该结构中常用的寄存器。PCI Express Capability 的组成结构如图 4-18 所示。

![[pci_express/3ec6c6c47ce5e9ddc675b89c36df1e34eb5a87e1383a4e035ac0ba0b9687885c.jpg]]  
图4-18 PCIExpressCapability结构的组成结构

# 1. PCI Express Capability 寄存器

PCI Express Capability 寄存器存放与 PCIe 设备相关的一些参数，包括版本号信息、端口描述，当前 PCIe 链路是与 PCIe 插槽直接连接还是作为内置的 PCIe 设备等一系列信息。这些参数的详细定义如表 4-3 所示。

① Event Collector 是 RC 集成的一个功能部件，进行错误检查和处理 PME 消息，该部件可选。  
表 4-3 PCI Express Capability 寄存器

<table><tr><td>Bits</td><td>定义</td><td>描述</td></tr><tr><td>3:0</td><td>Capability Version</td><td>存放PCIe设备的版本号，如果该设备基于PCIe总线规范2.x，该字段的值为0x2；如果该设备基于PCIe总线规范1.x，该字段的值为0x1。该字段只读</td></tr><tr><td>7:4</td><td>Device/Port Type</td><td>存放PCIe设备的属性。0b0000对应PCIe总线的EP;0b0001对应Legacy PCIe总线的EP;0b0100对应RC的Root port;0b0101对应Switch的上游端口;0b0110对应Switch的下游端口;0b0111对应PCIe桥片;0b1000对应PCI/PCI-X-to-PCIe桥片;0b1001对应RC中集成的EP;0b1010对应RC中的Event Collector①。该字段只读</td></tr><tr><td>8</td><td>Slot Implemented</td><td>当该位为1时,表示和当前端口相连的是一个PCIe插槽,而不是PCIe设备</td></tr><tr><td>13:9</td><td>Interrupt Message Number</td><td>当PCI Express Capability结构的Slot Status寄存器或者Root Status寄存器的状态发生变化时,该PCIe设备可以通过MSI/MSI-X中断机制向处理器提交中断请求。该字段存放MSI/MSI-X中断机制需要的Message Data字段。有关MSI中断机制的详细描述见第10章</td></tr></table>

# 2. Device Capability 寄存器

该寄存器的第2：0字段为“Max\_Payload\_SizeSupported”字段，该字段存放该设备支持的Max\_Payload\_Size参数的大小，该字段只读，如表4-4所示。

表 4-4 PCIe 设备支持的 Max\_Payload\_Size

<table><tr><td>Bit [2:0]</td><td>支持的 Max_Payload_Size</td></tr><tr><td>0b000</td><td>128B</td></tr><tr><td>0b001</td><td>256B</td></tr><tr><td>0b010</td><td>512B</td></tr><tr><td>0b011</td><td>1024B</td></tr><tr><td>0b100</td><td>2048B</td></tr><tr><td>0b101</td><td>4096B</td></tr></table>

“Max\_Payload\_SizeSupported”字段决定了一个TLP报文可能使用的最大有效负载，PCIe总线规定Max\_Payload\_Size参数的最大值为4096B，但是许多PCIe设备并不一定能够支持这么大的有效负载。在实际应用中，一个PCIe设备支持的Max\_Payload\_Size参数通常为128B、256B或者512B。

“Max\_Payload\_SizeSupported”字段仅表示该PCIe设备允许使用的Max\_Payload\_Size参数。在Device Control寄存器中，还有一个Max\_Payload\_Size参数，该字段可以由软件设置，表示实际使用的Max\_Payload\_Size参数大小。

值得注意的是，在PCIe设备中，“Max\_Payload\_SizeSupported”参数和Max\_Payload\_Size参数并不相同，前者是一个PCIe设备能够支持的最大Payload的大小，而后者是链路两端的PCIe设备进行协商，确定的实际使用值。有关这两个参数的详细说明见第6.4节。

该寄存器的第 $4 \sim 3$ 位为 Phantom Functions Supported 字段，该字段只读。当 Device Control 寄存器的 Phantom Functions Enable 位为 1 时，该字段才有意义。

- 该字段为0b00时表示不支持Phantom功能，PCIe设备不能使用Function Number扩展数据报文的Tag字段。  
- 该字段为 0b01 时表示支持 Phantom 功能, PCIe 设备可以使用 Function Number 的最高

位扩展TLP的Tag字段。

- 该字段为0b10时表示支持Phantom功能，PCIe设备可以使用Function Number的最高两位扩展TLP的Tag字段。  
- 该字段为0b11时表示支持Phantom功能，PCIe设备可以使用Function Number的全部三位扩展TLP的Tag字段。

该寄存器的第5位为Extended Tag Field Supported位，该位为1时表示TLP的Tag字段为8位；否则为5位。有关Tag字段的详细说明见第6.3.1节。本节不对该寄存器的其他位进行说明。

# 3. Device Control 寄存器

该寄存器各字段的描述如表4-5所示

表 4-5 Device Control 寄存器

<table><tr><td>Bit</td><td>定义</td><td>描述</td></tr><tr><td>0</td><td>Correctable Error Reporting Enable</td><td>该位可读写,其复位值为0。当此位为1时,PCIe设备可以发出ERR_COR Messages报文。而当此位为0时,不支持这种操作</td></tr><tr><td>1</td><td>Non-Fatal Error Reporting Enable</td><td>该位可读写,其复位值为0。当此位为1时,PCIe设备可以发出ERR_NON-FATAL Messages报文。而当此位为0时,不支持这种操作</td></tr><tr><td>2</td><td>Fatal Error Reporting Enable</td><td>该位可读写,其复位值为0。当此位为1时,PCIe设备可以发出ERR_FATAL Messages报文。而当此位为0时,不支持这种操作</td></tr><tr><td>3</td><td>Unsupported Request Reporting Enable</td><td>该位可读写,其复位值为0。当此位为1时,PCIe设备可以发出Unsupported Requests Error Messages报文;而当此位为0时,不支持这种操作</td></tr><tr><td>4</td><td>Enable Relaxed Ordering</td><td>该位为1时,使能PCIe设备的Relaxed Order模式,即PCIe设备在发送TLP时,可以根据需要设置TLP的Attr字段为Relaxed Ordering;该位为0时,TLP的Attr字段不能设置为Relaxed Ordering。该位复位时为1,可读写</td></tr><tr><td>7:5</td><td>Max_Payload_Size</td><td>该字段可读写,PCIe设备根据Device Capability寄存器的Bit[2:0]字段设置PCIe设备TLP的最大Payload。系统软件根据PCIe链路两端的实际情况,确认该字段的值。但是该值不能大于Device Capability寄存器的“Max_Payload_Size Supported”字段PCIe设备发送TLP时,其最大Payload不能大于Max_Payload_Size;当PCIe设备接收TLP时,必须能够处理小于该字段的TLP,而大于该字段的TLP将被认做错误报文</td></tr><tr><td>8</td><td>Extended Tag Field Enable</td><td>该位为1时,发送端可以使用8位的Tag字段;该位为0时,可以使用5位的Tag字段。该字段的复位值为1,可读写。Tag字段的详细描述见第6.3.2节</td></tr><tr><td>9</td><td>Phantom Functions Enable</td><td>该位为1,发送端可以使能Phantom Function功能;为0,不使能这个功能。该字段的复位值为0,可读写。Phantom Function功能的详细描述见上文</td></tr><tr><td>10</td><td>Auxiliary (AUX) Power PM Enable</td><td>该位为1时,PCIe设备可以使用总线提供的辅助电源</td></tr><tr><td>11</td><td>Enable No Snoop</td><td>此位为1时,PCIe设备在发送TLP时,该TLP的Attr字段可以设置为No Snoop;该位为0时,TLP的Attr字段不能设置为No Snoop。该位复位时为1,可读写。该位与Cache共享一致性相关</td></tr><tr><td>14:12</td><td>Max_Read_Request_Size</td><td>该字段记录在一个PCIe设备中,存储器读请求TLP可以请求的最大数据区域。当PCIe设备发送存储器读请求TLP时,该TLP所请求的数据大小不能超过Max_Read_Request_Size参数该字段的关系与表4-4中的描述相同</td></tr></table>

# 4. Device Status 寄存器

Device Status 寄存器主要字段的含义如表 4-6 所示。

表 4-6 Device Status 寄存器

<table><tr><td>Bit</td><td>定义</td><td>描述</td></tr><tr><td>0</td><td>Correctable Error Detected</td><td>该位为1时表示PCIe设备检测到Correctable Error,对该位写1将清除此位</td></tr><tr><td>1</td><td>Non-Fatal Error Detected</td><td>该位为1时表示PCIe设备检测到Non-Fatal Error,对该位写1将清除此位</td></tr><tr><td>2</td><td>Fatal Error Detected</td><td>该位为1时表示PCIe设备检测到Fatal Error,对该位写1将清除此位</td></tr><tr><td>3</td><td>Unsupported Request Detected</td><td>该位为1时表示PCIe设备收到一个PCIe总线并不支持的报文请求,对该位写1将清除此位</td></tr><tr><td>4</td><td>AUX Power Detected</td><td>当PCIe设备检查到辅助电源的存在时,而且如果该设备需要使用辅助电源,则将该位置1</td></tr><tr><td>5</td><td>Transactions Pending</td><td>对于EP而言,该位为1表示当前PCIe设备发送了一个Non-Posted的数据请求,但是没有收到完成报文应答;对于RC和Switch而言,该位为1表示RC和Switch自身(并不是转发其他设备的Non-Posted数据请求)发出了一个Non-Posted的数据请求,但是没有收到完成报文应答</td></tr></table>

# 5. Link Capabilities 寄存器

Link Capabilities 寄存器描述 PCIe 链路的属性，其主要字段的含义如下。

- Supported Link Speeds 字段。为 0b0001 表示 PCIe 链路支持 2.5GT（gigatransfers）/s；为 0b0010 表示 PCIe 链路支持 5GT/s；为 0b0100 表示 PCIe 链路支持 8GT/s。  
- Maximum Link Width 字段。该字段存放该 PCIe 设备支持的最大链路宽度。该字段为 0b000001 表示最大支持 $\times 1$ 的 PCIe 链路；为 0b000010 表示最大支持 $\times 2$ 的 PCIe 链路；为 0b000100 表示最大支持 $\times 4$ 的 PCIe 链路；为 0b001000 表示最大支持 $\times 8$ 的 PCIe 链路；为 0b001100 表示最大支持 $\times 12$ 的 PCIe 链路；为 0b010000 表示最大支持 $\times 16$ 的 PCIe 链路；为 0b100000 表示最大支持 $\times 32$ 的 PCIe 链路。  
- ASPM（Active State Power Management）Support字段，该字段只读。0b00和0b10为系统保留字段。当该字段为0b01时，表示ASPM支持L0s状态；当该字段为0b11时，表示ASPM支持L0s和L1状态。PCIe设备除了支持PCI PM电源管理方式之外，还支持ASPM机制进行电源管理。ASPM机制是PCIe设备进行的主动电源管理方式，与系统软件没有直接联系。有关ASPM的详细描述见第8.3节。  
- L0s Exit Latency 和 L1 Exit Latency 字段。这两个字段定义了 PCIe 设备从 L0s 和 L1 状态退出的最小延时。  
- Port Number字段。如果多端口RC和Switch支持多个下游端口，则使用该字段对这些端口进行编号。PCIe设备进行链路训练时，需要使用这个端口号。

# 6. Link Control 寄存器

Link Control 寄存器主要字段的解释如下。

\- ASPM Control 字段，该字段可读写。该字段为 0b00 时表示禁止 PCIe 设备的 ASPM 机制；为 0b01 时表示 PCIe 设备可以进入 L0s 状态；为 0b10 时表示 PCIe 设备可以进入 L1 状态；为 0b11 时表示 PCIe 设备可以进入 L0s 和 L1 状态。值得注意的是系统软件不能通过修改该字段使 PCIe 链路进入相应的状态，仅是通知硬件逻辑，可以进入相应的状态。ASPM 的详细描述见第 8.3 节。

- RCB((Read Completion Boundary))位。该位为0时，表示RCB为64B，该位为1时，表示RCB为128B。RCB的大小与完成报文的有效负载相关。对于RC而言，该字段只读，而Switch和EP可以读写该字段。有关该位的进一步说明见第6.4.3节。  
- Link Disable 位。向此位写 1，将禁止 PCIe 链路。此时链路状态机将进入 Disabled 状态，有关 PCIe 链路状态机的详细说明见第 8.2 节。  
- Retrain Link 位。向此位写 1，将重新训练 PCIe 链路。此时 PCIe 链路状态机将进入 Recovery 状态。  
- Common Clock Configuration 位，该位可读写。当该位为1时，表示PCIe链路两端的设备使用同源的参考时钟，即相同的REFCLK差分时钟；如果该位为0，表示PCIe链路两端的设备使用的参考时钟并不同源，即使用异步时钟。  
- Extended Sync 位，该位可读写。当该位为 1 时，表示 PCIe 设备退出 L0s 和进入 Recovery 状态时，需要额外发出一些同步序列。  
- Hardware Autonomous Width Disable 位，该位可读写。当该位为1时，PCIe设备不能改变当前已经协商好的PCIe链路宽度，除非为了修正PCIe链路中已经出现错误的Lane。  
- Link Bandwidth Management Interrupt Enable 位，该位可读写。当该位为1时，且Link Status寄存器的Link Bandwidth Management Status位为1时，PCIe设备将向处理器提交中断请求。此时这个中断请求使用的中断向量由PCI Express Capability寄存器的Interrupt Message Number字段确定。  
- Link Autonomous Bandwidth Interrupt Enable 位，该位可读写。当该位为1时，且Link Status寄存器的Link Autonomous Bandwidth Status位为1时，PCIe设备将向处理器提交中断请求。

# 7. Link Status 寄存器

Link Status 寄存器存放 PCIe 设备正在使用的 PCIe 链路的状态，由链路宽度和速度等参数组成，其主要字段的含义如表 4-7 所示。

表 4-7 Link Status 寄存器

<table><tr><td>Bit</td><td>定义</td><td>描述</td></tr><tr><td>3:0</td><td>Current Link Speeds</td><td>为0b0001表示PCIe链路的传输率为2.5GT/s;为0b0010表示PCIe链路的传输率为5GT/s;为0b0100表示PCIe链路的传输率为8GT/s。该字段只读</td></tr><tr><td>9:4</td><td>Negotiated Link Width</td><td>该字段存放当前PCIe设备和其上游PCIe设备进行链路协商后使用的链路宽度。该字段为0b000001表示使用×1的PCIe链路;为0b000010表示使用×2的PCIe链路;为0b000100表示使用×4的PCIe链路;为0b001000表示使用×8的PCIe链路;为0b001100表示使用×12的PCIe链路;为0b010000表示使用×16的PCIe链路;为0b100000表示使用×32的PCIe链路。该字段在PCIe链路进行训练的过程中,由硬件逻辑写入,系统软件只能读取该字段</td></tr><tr><td>11</td><td>Link Training</td><td>该位只读,为1时,表示PCIe链路正处于重新配置和重新训练阶段,当PCIe链路结束上述操作时,将此位清零</td></tr><tr><td>12</td><td>Slot Clock Configuration</td><td>该位由PCIe设备在初始化时确定,该位为1表示PCIe插槽与Add-In卡使用的参考时钟源相同。读者需要留意该位与Common Clock Configuration位的差别</td></tr><tr><td>13</td><td>Data Link Layer Link Active</td><td>该位表示PCIe链路的状态。该位为1时,表示PCIe链路处于DL_Active,即正常工作状态</td></tr><tr><td>14</td><td>Link Bandwidth Management Status</td><td>该位由PCIe硬件设置。当PCIe链路重训练结束，或者PCIe设备完成PCIe链路的链路宽度和链路速度的设定后，该位置1。该位写1清除</td></tr><tr><td>15</td><td>Link Autonomous Bandwidth Status</td><td>该位由PCIe设备确定。当PCIe链路自主完成链路宽度和速度的协商后，将该位置1。该位写1清除</td></tr></table>

# 8. Device Capabilities 2 寄存器

该寄存器定义了一些PCIeV2.1总线规范使用的字段，其主要字段如表4-8所示。

表 4-8 Device Capabilities 2 寄存器

<table><tr><td>Bit</td><td>定义</td><td>描述</td></tr><tr><td>6</td><td>AtomicOp Routing Supported</td><td>Switch的上下游端口和RC端口支持该位,在PCIe V2.1总线规范定义了原子操作。当该位为1时,表示原子操作TLP可以通过当前Switch或者RC。有关原子操作的详细描述见第6.3.5节</td></tr><tr><td>7</td><td>32-bit AtomicOp Completer Supported</td><td>该位为1时,表示EP或者RC支持32位原子操作</td></tr><tr><td>8</td><td>64-bit AtomicOp Completer Supported</td><td>该位为1时,表示EP或者RC支持64位原子操作</td></tr><tr><td>9</td><td>128-bit CAS Completer Supported</td><td>该位为1时,表示EP或者RC支持128位原子操作。在PCIe总线规范中,只有CAS(Compare and Swap)支持128位操作</td></tr><tr><td>13:12</td><td>TPH Completer Supported</td><td>该字段为0b00时,表示接收端不支持TPH和扩展TPH报文;为0b01时,表示接收端仅支持TPH报文;为0b11时,表示接收端支持TPH和扩展TPH报文;而0b10保留。有关TPH的详细描述见第6.3.6节</td></tr><tr><td>20</td><td>Extended Fmt Field Supported</td><td>该位为1时,表示TLP的Fmt字段为3位,即支持TLP Prefix;为0时,Fmt字段为2位。Fmt字段的详细描述见第6.1.1节。该位由V2.1规范引入,其目的是为了扩展TLP头</td></tr><tr><td>21</td><td>End-End TLP Prefix Supported</td><td>该位为1时,表示EP可以接收含有End-End TLP Prefix的TLP</td></tr><tr><td>23~22</td><td>Max End-End TLP Prefixes</td><td>该字段限制一个TLP中End-End TLP Prefix的个数。该字段为0b01表示TLP中最多含有1个End-End TLP Prefix;为0b10表示最多含有2个End-End TLP Prefix;为0b11表示最多含有3个End-End TLP Prefix;为0b00表示最多含有4个End-End TLP Prefix</td></tr></table>

# 9. Device Control 2 寄存器

Device Control 2 寄存器主要字段的含义如表 4-9 所示。

表 4-9 Device Control 2 寄存器

<table><tr><td>Bit</td><td>定义</td><td>描述</td></tr><tr><td>6</td><td>AtomicOp Requester Enable</td><td>该位可读写,对 EP 和 RC 有效。如果该位和 Command 寄存器的 “Bus Master Enable” 位同时有效,EP 或者 RC 可以发出原子操作请求 TLP</td></tr><tr><td>7</td><td>AtomicOp Egress Blocking</td><td>该位可读写,对 Switch 的上下游端口和 RC 端口有效。当 AtomicOp Routing Supported 位为1时,该位可以为1,否则该位只能为0。该位为1时,Egress 端口将阻止原子操作 TLP 通过</td></tr><tr><td>8</td><td>IDO Request Enable</td><td>该位可读写,对 EP 和 RC 有效。当该位为1时,TLP 中的 IDO (ID-Based Ordering) 位可以根据实际情况(即 TLP 的 Attr2 位)设置为1。IDO 是 PCIe V2.1 总线规范引入的新的“序”模型。有关 IDO 机制的详细说明见第6.1.3节</td></tr><tr><td>9</td><td>IDO Completion Enable</td><td>该位可读写。当该位为1时，EP可以处理IDO位为1的完成报文</td></tr><tr><td>15</td><td>End-End TLP Prefix Blocking</td><td>该位可读写。当该位为0时，EP不能发送带有End-End TLP Prefix的TLP；为1时，可以发送</td></tr></table>

# 10. Root Control 和 Root Status 寄存器

这两个寄存器与PCIe总线的AER（Advanced Error Reporting）机制相关。其中Root Control寄存器由以下位组成。

- System Error on Correctable Error Enable。该位为1时，表示RC端口管理的PCI树或者RC端口发送ERR\_COR信息后，将向处理器提交System Error信息。  
- System Error on Non-Fatal Error Enable。该位为1时表示RC端口管理的PCI树或者RC端口发送ERR\_NONFATAL信息后，将向处理器提交System Error信息。  
- PME Interrupt Enable。该位为1时，如果RC端口收到Root Status寄存器的PME Status为1的信息后，将向处理器提交PME中断信息。  
- CRS Software Visibility Enable。当此位为 1 时，系统软件发送配置请求 TLP 后，RC 端口可以要求该配置请求 TLP 择时重试。如当 PCIe 总线没有初始化完毕时，不能接收处理器的配置请求，此时将该位置 1；初始化完毕后，将该位置 0。

而Root Status寄存器由以下位和字段组成

- PME Requester ID。该字段记录最后发送 PME 消息的 PCIe 设备的 Requester ID 号。  
- PME Status。该位为1时，表示PCIe设备（ID号为PME Requester ID）已经向RC发送了PME消息，但是并没有被处理完毕，该位写1清除。  
- PME Pending。当 PME Status 位为 1 而且该位也 1 时，表示 RC 中有尚未处理的 PME 消息。当 RC 清除 PME Status 位后，硬件将向 RC 提交 PME 消息，更新 PME Requester ID，并将 PME Status 重新置为 1，同时清除 PME Pending。PCIe 总线设置该位的主要目的是为了防止丢失 PME 消息。

PCI Express Capability 结构中还含有 Slot Capabilities、Slot Control 和 Slot Status 等寄存器。为节约篇幅，本节并不对这些寄存器一一进行介绍，在一个指定的 PCIe 设备中，PCI Express Capability 结构的这些寄存器并不会全部实现。许多 PCIe 设备甚至不存在 PCI Express Capability 结构，但是在这些 PCIe 设备中依然存在与 PCI Express Capability 结构相关的概念，只是这些结构没有以寄存器的形式表现出来，供系统程序员使用而已。

# 4.3.3 PCI Express Extended Capabilities 结构

PCI Express Extended Capabilities 结构存放在 PCI 配置空间 0x100 之后的位置，该结构是 PCIe 设备独有的，PCI 设备并不支持该结构。实际上绝大多数 PCIe 设备也并不支持该结构。在一个 PCIe 设备中可能含有多个 PCI Express Extended Capabilities 结构，并形成一个单向链表，其中第一个 Capability 结构的基地址为 0x100，其结构如图 4-19 所示。

在这个单向链表的尾部，其 Next Capability Offset、Capability ID 和 Capability Version 字段的值都为 0。如果在 PCIe 设备中不含有 PCI Express Extended Capabilities 结构，则 0x100 指

针所指向的结构，其Capability ID字段为0xFFFF，而Next Capability Offset字段为 $0\mathrm{x}0$ 。

![[pci_express/60f240889225bc7184f7d78c59a766e6441a1084075f1d0d39a1814a0fdee7c2.jpg]]  
图4-19 PCI Express Extended Capabilities结构

一个PCI Express Extended Capabilities结构由以下参数组成。

- PCI Express Capability ID 字段存放 Extended Capability 结构的 ID 号。  
- Capability Version 字段存放 Extended Capability 结构的版本号。  
- Next Capability Offset 字段存放下一个 Extended Capability 结构的偏移。

PCIe总线定义了一系列PCIExpressExtendedCapabilities结构，如下所示。

- AER Capability 结构。该结构定义了所有 PCIe 设备可能遇到的错误，包括 Uncorrectable Error（不可恢复错误）和 Correctable Error（可恢复错误）。当 PCIe 设备发现这些错误时，可以根据该寄存器的设置使用 Error Message 将错误状态发送给 Event Collector，并由 Event Collector 统一处理这些错误。系统软件必须认真处理每一个 Error Message，并进行恢复。对一个实际的工程项目，错误处理是保证整个项目可靠性的重要一环，不可忽视。AER 机制与 Error Message 报文的处理相关，第 6.3.4 节将进一步介绍 AER 机制。  
- Device Serial Number Capability 结构。该结构记载 PCIe 设备使用的序列号。IEEE 定义了一个 64 位宽度的 PCIe 序列号，其中前 24 位作为 PCIe 设备提供商使用的序列号，而后 40 位由厂商选择使用。  
- PCIe RC Link Declaration Capability 结构。在 RC、RC 内部集成的设备或者 RCRB 中可以包含该结构。该结构存放 RC 的拓扑结构，如 RC 使用的 PCI 链路宽度。如果 RC 支持多个 PCIe 链路，该结构还包含每一个链路的描述和端口命名。  
- PCIe RC Internal Link Control Capability 结构。该结构的主要作用是描述 RC 内部互连使用的 PCIe 链路。该结构由 Root Complex Link Status 和 Root Complex Link Control 寄存器组成。  
- Power Budget Capability 结构。当处理器系统为一些动态加入的 PCIe 设备分配电源配额时，将使用该设备的 Power Budget Capability 结构。  
- ACS（Access Control Services）Capability结构。该结构对PCIe设备进行访问控制管

理。RC端口、Switch的下游端口和多功能PCIe设备可以支持该结构。该结构与PCIe总线的ACS机制相关。ACS机制定义了一组与收到的TLP相关的操作，该机制的原理较为简单，本节对此不做进一步分析。

- RCRB Header Capability 结构。该结构存放 RC 中的 RCRB，第 5.1 节将以 Montivina 平台为例介绍该结构的组成结构。  
- RC Event Collector EP Association Extended Capability。在 x86 处理器系统中，RC 包含一个 Event Collector 控制器，该控制器处理 PCIe 设备发向 RC 的各类消息，如 PME 消息和 Error 消息。该结构用来描述 Event Collector 控制器。  
- Multicast Capability 结构。PCIe 总线上的 RC、Switch 或者 EP 如果支持 Multicast 消息，需要使用该结构描述支持哪些 Multicast 组。PCIe 体系结构支持 Multicast 功能，在 PCIe 总线中，除了 PCIe 桥一定不支持 Multicast 功能外，其他设备都可以支持该功能。本节对 Multicast 功能不做进一步说明。

此外PCIe总线还可以支持其他Capability结构，如Vendor-Defined Capability、Resizable BAR Capability、DPA（Dynamic Power Allocate）和MFVC（Multi-Function Virtual Channel）Capability结构等其他结构。但是在PCIe总线中，这些扩展的Capability结构并没有得到充分利用。在一个实际的PCIe设备中可能并不包含这些结构。

PCIe 设备定义的 Capability 结构有些过多，使用这种方法可以概括所有 PCIe 设备的使用特性。许多 PCIe 设备在支持这些 Capability 结构后，几乎可以不使用 BAR 寄存器空间存放与 PCIe 总线相关的任何信息。但是过多的 Capability 结构为软硬件工程师在设计上带来了不小的麻烦。一般说来，事务的发展过程是由简入繁，由繁化简。目前 PCIe 总线的发展仍处在由简入繁的过程。

本节仅详细介绍PCI Express Extended Capabilities结构组中的MFVC结构。MFVC结构是PCIe总线的一个可选结构，其结构如图4-20所示。TLP在通过Switch时需要通过TC/VCMapping，而且在进行VC仲裁和端口仲裁时，需要使用某些仲裁策略。

在PCIe总线中，TC/VCMapping表和VC/端口仲裁策略在MFVC结构中定义。其结构如图4-20所示。值得注意的是，在许多PCIe设备中，可能只具有一个VC，而且其VC仲裁的算法固定，那么在这个PCIe设备中，MFVC结构并没有存在的必要。目前支持多VC的PCIe设备极少，仅有一些RC和Switch中存在多个VC，而且也仅支持两个VC。

VC Capability 的 ID 为 0x02 或者 0x09，VC Capability 结构由两部分组成，分别是一个 VC Capability 寄存器组和 n 个 VC Resource 寄存器组，其中 VC Resource 寄存器是可选的。如果 PCIe 设备仅支持一个 VC 时，该结构中不含有 VC Resource 寄存器，而在 VC Capability 寄存器组中包含该 VC 的描述信息。当一个 PCIe 设备支持 8 个 Function 时，则 n 为 7；如果支持 7 个设备，则 n 为 6，并以此类推。其中每一个 VC Resource 寄存器组中都包含一个 VC 仲裁表、端口仲裁表和 VC/TC 的映射表。

# 1. VC Capability 寄存器组

该组寄存器由PortVCCapabilityRegister1、PortVCCapabilityRegister2、PortVCControlRegister和PortVCStatusRegister寄存器组成。

(1) Port VC Capability Register 1 主要字段的含义如表 4-10 所示。

![[pci_express/24f434433adc466393366a73db2cf9b125467b3eb7d4c14bf69e86f32e481435.jpg]]  
图4-20 MFVC结构

表 4-10 Port VC Capability Register 1

<table><tr><td>Bit</td><td>定义</td><td>描述</td></tr><tr><td>2:0</td><td>Extended VC Count</td><td>扩展的VC个数,最小值为0,表示只支持VC0;最大值为7,表示支持8个VC,VC0~VC7</td></tr><tr><td>6:4</td><td>Low Priority Extended VC Count</td><td>和VC0优先级相同的扩展VC的个数。在PCIe总线中,VC0的级别最低。该字段的最小值为0,最大值为7</td></tr><tr><td>9:8</td><td>Reference Clock</td><td>如果VC使用Time-based WRR算法时,需要使用一个参考时钟。PCIe总线规定当该字段为0b00时,这个参考时钟的周期为100ns,即时钟频率为10 MHz</td></tr><tr><td>11:10</td><td>Port Arbitration Table Entry Size</td><td>表示Port Arbitration Table Entry的大小。0b00表示Entry的长度为1位;0b01表示Entry的长度为2位;0b10表示Entry的长度为4位;0b11表示Entry的长度为8位。</td></tr></table>

# (2) Port VC Capability Register 2

该寄存器由两个字段组成。其中VC Arbitration Capability字段存放PCIe设备支持的VC调度算法，PCIe总线提供的调度算法包括Hardware-fixed仲裁策略和WRR仲裁策略。其中

WRR 仲裁策略分为 32、64 或者 128 个 Phase, VC 仲裁不支持 Time-based WRR 算法, 有关 WRR 算法的详细说明见下文。

而VC Arbitration Table Offset字段存放VC Arbitration Table的地址偏移，如果在PCIe设备中不含有VC Arbitration Table，该字段为0。

# (3) Port VC Control Register

该寄存器由LoadVCArbitrationTable位和VCArbitrationSelect字段组成。系统软件通过操纵该寄存器更改VC的仲裁算法，其中VCArbitrationSelect字段用来选择VCArbitrationTable的长度，其关系如表4-11所示。

表 4-11 VC Arbitration Select 字段的说明

<table><tr><td>VC Arbitration Select 字段</td><td>VC Arbitration Table 的长度</td></tr><tr><td>0b001</td><td>32</td></tr><tr><td>0b010</td><td>64</td></tr><tr><td>0b011</td><td>128</td></tr></table>

Load VC Arbitration Table 位用来更新 VC 的仲裁算法。系统软件向 Load VC Arbitration Table 位写 1 时更新 VC 的仲裁算法，PCIe 设备可以根据 VC Arbitration Select 字段选择合适的仲裁算法，系统软件向 Load VC Arbitration Table 位写 0 没有意义。

# (4) Port VC Status Register

Port VC Status Register 使用 VC Arbitration Table Status 位，控制更新 VC 仲裁算法的进度。当系统软件向 Load VC Arbitration Table 位写 1 时，PCIe 设备将更新 VC 的仲裁算法，并将 VC Arbitration Table Status 位置 1。PCIe 设备在没有完成仲裁算法的更换之前，VC Arbitration Table Status 位一直为 1，当 PCIe 设备完成仲裁算法的更换后，该位被 PCIe 设备清零。

# 2. VC Resource 寄存器组

在PCIe设备中，每一个VC都有一组VC Resource寄存器组，这组寄存器设置每一个VC的属性和端口仲裁算法。该组寄存器由VC Resource Capability Register、VC Resource Control Register和VC Resource Status Register寄存器组成。

(1) VC Resource Capability Register 主要字段的含义如表 4-12 所示。

表 4-12 VC Resource Capability Register

<table><tr><td>Bit</td><td>定义</td><td>描述</td></tr><tr><td>7:0</td><td>Port Arbitration Capability</td><td>存放当前VC支持的端口仲裁算法。该字段对Switch和支持Peer-to-Peer传送的RC端口有效Bit 0:硬件固化的算法,如RRBit 1:WRR with 32 phasesBit 2:WRR with 64 phasesBit 3:WRR with 128 phasesBit 4:Time-based WRR with 128 phasesBit 5:WRR with 256 phasesBit 6~7:保留</td></tr><tr><td>15</td><td>Reject Snoop Transactions</td><td>此位为0时,TLP的No Snoop位无论是0还是1都可以通过该VC;此位为1时,如果TLP的No Snoop位为0,该TLP不能通过该VC。该位对RC或者RCRB有意义</td></tr></table>

(2)VC Resource Control Register主要字段的含义如下。

\- TC/VC Map 字段，第 7\~0 位。该字段的每一位对应一个 TC，其中第 7 位对应 TC7，

该位有效时表示 TC7 使用该 VC 进行数据传递；第 6 位对应 TC6，该位有效时表示 TC6 使用该 VC 进行数据传递，并以此类推。对于 VCO 通路，该字段的复位值为 $0 \times \mathrm{FF}$ ，对于其他 VC 通路，该字段的复位值为 $0 \times 00$ 。因此在系统初始化时，所有 TC 都使用 VCO 进行数据传递，而 PCIe 链路必须支持 VCO。使用该字段可以保证 TC 不同的 TLP 可以使用同一个 VC，但是在 PCIe 总线中，一个 TC 与一条 VC 建立了映射关系后，不能与其他 VC 建立映射关系。

- Load Port Arbitration Table 位，第16位。当该位被置1后，PCIe设备将使用Port Arbitration Table更新端口仲裁的算法，当该位置1后，VC Resource Status寄存器的Port Arbitration Table Status位也将置1，当端口仲裁算法更新完毕后，Port Arbitration Table Status位将清零；对此位写1没有意义。  
- Port Arbitration Select 字段，第 $19 \sim 17$ 位。该字段描述 Port Arbitration Table 表的 Entry 个数。下文将详细解释该字段。  
- VC ID字段，第 $26\sim 24$ 位。该字段存放当前VC的ID号，PCIe设备的第一个VCID必须为0。  
- VC Enbale 位，第 31 位。该位为 1 时，当前 VC 通路有效，否则无效。在系统初始化完毕后，VCO 的 VC Enable 位为 1，而其他 VC 的 VC Enable 位为 0。

(3) VC Resource Status Register 主要字段的含义如下。

- Port Arbitration Table Status 位，第0位。该位表示当前VC更新端口仲裁算法的状态，该位由PCIe设备维护，对系统软件只读。  
- VC Negotiation Pending 位，第 1 位。该位为 1 表示当前 VC 通路正在进行初始化或者处于正在关闭的状态，此时当前 VC 并没有准备好，还没有从 FC INIT2 状态中退出；为 0 表示当前 VC 准备好，PCIe 链路已经完成流量控制的初始化。系统软件必须保证该位为 0 后，才能对该 VC 进行操作。有关 FC INIT2 状态的详细说明见第 9.3.3 节。

# 3. VC Arbitration Table

VC Arbitration Table 的长度由 Port VC Control 寄存器的 VC Arbitration Select 字段确定，最小为 32 个 Entry，最大为 128 个 Entry。VC Arbitration Table 实现 VC 仲裁的 WRR 算法。在 VC Arbitration Table 中，每一个 Entry 由 4 位组成，其中最高位保留，最低三位记录 VC 号。下文举例说明 32 个 Phase 的 WRR 算法，在这种情况下 VC Arbitration Table 的长度为 32，这个表中每一个 Entry 记录一个 VC 号，如图 4-21 所示。

在图4-21中，VC Arbitration Table的每一个Entry都记录一个VC号。假定VC仲裁时从Phase0开始使用，该Entry存放的VC号为VCO，则VC仲裁的结果是传送虚通路VC0中的总线事务，当这个总线事务传送结束后，将处理Phase1中的VC；如果该Entry存放的VC号为VC2，则VC仲裁的结果是传送虚通路VC2中的总线事务，并以此类推直到Phase31后，再对Phase0重新进行处理。

使用这种加权处理的方法，可以保证PCIe总线QoS。值得注意的是，使用该方法时，如果当前Entry存放的VC中，不存在总线事务时，将迅速移动到下一个Entry；如果VC间并没有出现冲突时，不需要使用该表进行仲裁，使用64个Phase和128个Phase的WRR算法的实现机制与此类似。由上文的分析可以发现，与RR算法相比，PCIe总线使用WRR算法可以在保证QoS的基础上，使各个VC公平使用端口资源。

![[pci_express/ddd20b542952db9cec27ce9c01b456b0c7ae9b6f720a632e041cd27cb83f32ee.jpg]]  
图4-21 32 Phases的VCArbitrationTable

# 4. Port Arbitration Table

每一个VC都有一个Port Arbitration Table，如图4-12所示。每一个TLP都首先需要进行端口仲裁之后，才能进行VC仲裁，然后通过端口发送。Port Arbitration Table的主要作用是确定端口仲裁的策略。其长度由VC Resource Capability Register的Port Arbitration Capability字段确认，如表4-12所示。

在该表中，每一个 Entry 的大小由该设备支持的端口数目有关，如果一个设备支持 N 个端口，则该表 Entry 的大小为 $\lceil \mathrm{Log}_2\mathrm{N}\rceil$ 。如果一个设备有 6 个端口，则 Port Arbitration Table 的 Entry 大小为 3。PCIe 总线支持 RR、WRR 和 Time-based WRR 端口仲裁策略。

Time-based WRR 端口仲裁策略的引入是为了支持 PCIe 总线的 isochronous 数据传送方式。在 PCIe 总线中使用 WRR 算法每处理完一个总线事务将移动一个 Phase，而 Time-based WRR 算法需要至少经过一个时间槽后才能移动一个 Phase。PCIe 总线为 Time-based WRR 算法使用的基准时钟周期在 Port VC Capability Register 1 的 Reference Clock 字段中定义，目前该值为 100ns。

PCIe总线中使用的这些仲裁算法源于网络通信，这几种算法都是基于轮询的仲裁算法。在网络中，还经常使用DWRR（Deficit Weighted Round Robin）算法。

WRR算法在支持长度不同的报文时，会出现带宽分配不公平的现象，为此M.Shreedhar与GeorgeVarghese提出了DWRR调度算法。DWRR算法给每一个队列分配的权值不是基于报文的个数，而是基于报文的比特数。因此可以使各个队列公平地获得带宽。但是这种算法并不适用于PCIe总线，因为PCIe总线基于报文进行数据传递，而不是基于数据流。该算法在ATM分组交换网中得到了广泛的应用。

# 4.4 小结

本章简要介绍了PCIe总线的各个组成部件，包括RC、Switch和EP等，并介绍了PCIe总线的层次组成结构，和PCIe设备使用的Capability结构。本章是读者了解PCIe体系结构的基础。

