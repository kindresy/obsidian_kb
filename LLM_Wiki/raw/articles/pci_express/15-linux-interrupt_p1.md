---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "15"
section: "第15章 Linux PCI的中断处理"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第15章 Linux PCI的中断处理

Linux PCI 的中断处理包含两部分内容，一部分是 PCI 设备使用 INTx 信号，包括 PCIe 设备使用 INTx 消息，向处理器提交的中断请求，这种中断请求方式也被称为 PCI 设备的传统中断请求；而另一部分是处理 MSI/MSI-X 中断机制。

Linux PCI 在处理传统中断请求时，需要考虑 PCI 总线的中断路由。本章将首先介绍 PCI 总线的中断路由，并在第 15.2 节介绍 MSI 和 MSI-X 中断机制，而不再详细介绍 PCI 设备的传统中断请求。

# 15.1 PCI总线的中断路由

在多数x86处理器系统中，PCI设备的INTA～D#四个中断请求信号与LPC接口提供的外部引脚PIRQA～D#相连，之后PIRQA～D#与I/O APIC的中断请求信号IRQ\_PIN16～19#相连。如果PCIe设备没有使用MSI中断请求机制，而是使用了Legacy INTx方式模拟INTA～D#信号时，这些Assert INTx和Deassert INTx消息也由Chipset处理，并由Chipset将这些消息转换为一根硬件引脚，然后将这个硬件引脚与I/O APIC的中断输入引脚相连。其连接关系如图15-1所示。I/O APIC最终使用REDIR\_TBL表，将来自输入引脚的中断请求发送至Local APIC，并由CPU进一步处理这个中断请求。

![[pci_express/26bf46c87861f32289ea555b5e95c46246ce27aa7f0ea0289cb0a65a96ec328f.jpg]]  
图15-1 I/O APIC如何处理PCI设备的中断请求

本书并不关心I/O APIC如何使用APIC Message将中断消息传递给Local APIC，而重点关注PCI和PCIe设备使用的中断信号与I/O APIC输入引脚IRQ\_PIN16\~19的连接关系。如图15-1所示，LPC的PIRQA～D#分别与IRQ\_PIN16～19对应，但是PCI设备的INTA～D#与PIRQA～D#的连接关系并不是唯一的，图15-1所示的PCI设备与中断控制器连接方法只是其中一种连接方法。

而无论硬件采用何种连接结构，系统软件都需要能够正确识别是哪个PCI设备发出的中断请求，为此系统软件使用PCI中断路由表（PCI Interrupt Routing Table）记录PCI设备使用的INTA～D#与I/O APIC中断输入引脚IRQ16～19的对应关系。

如果在x86处理器系统中存在Switch，而这个Switch的每一个端口都相当于一个虚拟PCI桥，此时该Switch的下游端口连接的PCIe设备，在使用PCI Message INTx消息提交中断请求时，虚拟PCI桥可能将其转换为其他PCI Message INTx消息。在虚拟PCI桥中，Primary总线和Secondary总线PCI Message INTx消息的对应关系如表15-1所示。

表 15-1 虚拟 PCI 桥 Primary 总线与 Secondary 总线间 INTx 消息间的映射关系

<table><tr><td>设备号</td><td>PCI桥 Secondary 总线的虚拟中断信号 INTx#</td><td>PCI桥 Primary 总线的虚拟中断信号 INTx#</td></tr><tr><td rowspan="4">0,4,8,12,16,20,24,28</td><td>INTA#</td><td>INTA#</td></tr><tr><td>INTB#</td><td>INTB#</td></tr><tr><td>INTC#</td><td>INTC#</td></tr><tr><td>INTD#</td><td>INTD#</td></tr><tr><td rowspan="4">1,5,9,13,17,21,25,29</td><td>INTA#</td><td>INTB#</td></tr><tr><td>INTB#</td><td>INTC#</td></tr><tr><td>INTC#</td><td>INTD#</td></tr><tr><td>INTD#</td><td>INTA#</td></tr><tr><td rowspan="4">2,6,10,14,18,22,26,30</td><td>INTA#</td><td>INTC#</td></tr><tr><td>INTB#</td><td>INTD#</td></tr><tr><td>INTC#</td><td>INTA#</td></tr><tr><td>INTD#</td><td>INTB#</td></tr><tr><td rowspan="4">3,7,11,15,19,23,27,31</td><td>INTA#</td><td>INTD#</td></tr><tr><td>INTB#</td><td>INTA#</td></tr><tr><td>INTC#</td><td>INTB#</td></tr><tr><td>INTD#</td><td>INTC#</td></tr></table>

PCIe设备发送的PCI Message INTx消息首先到达虚拟PCI桥的Secondary总线，之后虚拟PCI桥根据PCIe设备的设备号将这些PCI Message INTx消息转换为Primary总线合适的虚拟中断信号。如设备号为1的PCIe设备使用PCI Message INTA消息进行中断请求时，该消息在通过虚拟PCI桥后，将被转换为PCI Message INTB消息，然后继续传递该消息报文，最终PCI Message INTx消息将到达RC，并由RC将该消息报文转换为虚拟中断信号INTx，并与I/O APIC的中断请求引脚IRQ\_PIN16\~19相连。

然而直接使用 PCIe 总线提供的标准方法会带来一些问题。因为一条 PCIe 链路只能挂接

一个EP，这个EP的设备号通常为0，而这些设备使用的虚拟中断信号多为INTA#，因此这些PCIe设备通过Switch的虚拟PCI-to-PCI桥进行中断路由后，将使用虚拟中断信号INTA#，并与I/O APIC的IRQ\_PIN16引脚相连，并不会使用其他IRQ\_PIN引脚，这造成了IRQ\_PIN16的负载过重。其连接拓扑结构如图15-2所示。

![[pci_express/b545b912243155f126c80946788acbf2e0e318c568fef97276b6c4b033f8fd72.jpg]]

![[pci_express/6349c36a9e77932ebf9bf1abbe65d4617884614e4e1bc0a5b7058ea384e8378e.jpg]]  
图15-2 PCI Message中断路由

如上图所示，PCIe设备使用的INTx中断请求都最终使用I/O APIC的IRQ\_PIN16引脚，从而造成了这个引脚所申请的中断过于密集，因此采用这种中断路由方法并不合理。为此Intel在5000系列的Chipset中使用了Interrupt Swizzling技术将这些来自PCIe设备的中断请求平均分配到I/O APIC的IRQ\_PIN16\~19引脚中。

在图15-2中，Chipset设置了一个INTSWZCTRL寄存器，通过这些寄存器可以将PCIe设备提交的中断请求均衡地发送至I/O APIC中。如果一个EP对应的INTSWZCTRL位为0，则该设备的INTA#将与IRQ\_PIN16相连；如果为1，将与IRQ\_PIN17相连，并以此类推，最终实现中断请求的负载均衡。

在一个x86处理器系统中，PCI设备或者PCIe设备使用的中断信号INTA～D#与I/O A-PIC的IRQ\_PIN16～19之间的对应关系并不明确，各个厂商完全可以按照需要定制其映射关系。这为系统软件的设计制造了不小的困难。为此BIOS为系统软件提供了一个PCI中断路由表，存放这个映射关系，ACPI规范将这个中断路由表存放在DSDT中。

值得注意的是，每一个HOST主桥和每一条PCI总线都含有一个中断路由表。在讲述PCI中断路由表之前，我们简要回顾Linux系统如何为PCI设备分配中断向量。

# 15.1.1 PCI设备如何获取irq号

在Linux系统中，PCI设备使用的irq号存放在pdev $\rightarrow$ irq参数中，该参数在Linux设备驱动程序进行初始化时，由pci\_enable\_device函数设置。本书在第12.3.2节曾简要介绍过这个函数，下文进一步说明如何使用该函数设置PCI设备的irq号。pci\_enable\_device函数将依次调用\_pci\_enable\_device\_flags $\rightarrow$ do\_pci\_enable\_device $\rightarrow$ pcibios\_enable\_device函数设置PCI设备使用的irq号。

pcibios\_enable\_device 函数将调用 pcibios\_enable\_irq 函数，设置 PCI 设备使用的 IRQ 号。如果处理器系统使能了 ACPI 机制，pcibios\_enable\_irq 函数将被赋值为 ACPI\_pci\_irq\_enable。acpi\_pci\_irq\_enable 函数在 ./drivers/acpi/pci\_irq.c 文件中，其实现过程如源代码 15-1 所示。

int acpi_pci_irq_enable(struct pcie_dev \*dev)   
{ struct acpi_prt_entry \*entry; int gsi; u8 pin; int triggering $=$ ACPI_LEVEL_SENSITIVE; int polarity $=$ ACPI_ACTIVE_LOW; char \* link $=$ NULL; char link_desc[16]; int rc; pin $=$ dev- $\rightharpoondown$ pin; if(!pin){ ACPI_DEBUG_PRINT((ACPI_DB_INFO, "No interrupt pin configured for device $\% \mathrm{s}\backslash \mathrm{n}^{\prime \prime}$ , pci_name(dev)); return 0; } entry $=$ acpi_pci_irq.lookup(dev，pin); if(!entry）{ /\* \* IDE legacy mode controller IRQs are magic. Why do compat \* extensions always make such a nasty mess. \*/ if (dev->class>>8 $= =$ PCI_CLASS_STORAGE_IDE&& (dev->class&0x05） $= = 0$ ） return 0; } if (entry）{ if (entry->link) gsi $=$ acpi_pci_link_allocate_irq( entry->link, entry->index, &triggering,&polarity, &link); else gsi $=$ entry- $\rightharpoonup$ index; } else gsi $= -1$

if $(\mathrm{gsi} < 0)$ { dev_warm(&dev->dev, "PCI INT %c: no GSI", pin_name(pin)); /* Interrupt Line values above 0xF are forbidden */ if (dev->irq > 0 && (dev->irq <= 0xF)) { printf(" - using IRQ %d\n", dev->irq); acpi_register_gsi(&dev->dev, dev->irq, ACPI_LEVEL_SENSITIVE, ACPI_ACTIVE_LOW); return 0; } else { printf("\\n"); return 0; } } rc = acpi_register_gsi(&dev->dev, gsi, triggering, polarity); if (rc < 0) { dev_warm(&dev->dev, "PCI INT %c: failed to register GSI\n", pin_name(pin)); return rc; } dev->irq = rc; return 0; }

该函数首先调用acpi\_pci\_irqLOOKUP $\rightarrow$ acpi\_pci\_irq\_find\_prt\_entry函数，从acpi\_prt\_list链表中获得一个acpi\_prt\_entry结构的Entry。在acpi\_prt\_list链表中存放PCI总线的中断路由表，本章将在第15.1.2节进一步介绍该表。在这个Entry中，存放PCI设备使用的Segment、Bus、Device和Function号，PCI设备使用的中断请求信号（INTA#\~INTD#）和GSI（Global System Interrupt）号。

这段程序在获得 Entry 后，将判断 Entry $\rightarrow$ link 是否为空，如果为空，表示当前 x86 处理器系统使用 I/O APIC 管理外部中断，而不是使用 8259A。在 Intel 的 ICH9 中集成了两个中断控制器，一个是 8259A，另一个是 I/O APIC。Linux x86 通过软件配置，决定究竟使用哪个中断控制器，在绝大多数情况下，Linux x86 使用 I/O APIC 而不是 8259A 管理外部中断请求 200。本章不再关心 8259A 中断控制器，因此也不再关心 Entry $\rightarrow$ link 不为空的处理情况。

这段程序在获得 GSI 号之后，将调用 acpi register\_gsi 函数，将 GSI 号转换为系统软件使

用的irq号。acpi\_register\_gsi函数使用三个入口参数，分别为GSI号，中断触发方式和采用电平触发时的极性。其中PCI设备使用低电平触发方式。

acpi register\_gsi 函数执行完毕后，将为 PCI 设备分配一个irq号，这个irq号是系统软件使用的，之后PCI设备的驱动程序可以使用request\_irq函数将中断服务例程与irq号建立映射关系；该函数还将设置I/O APIC的REDIR\_TB 表，将GSI号与REDIR\_TB表中的中断向量建立对应关系，同时初始化与操作系统相关的irq结构。为了深入理解acpi register\_gsi函数，读者需要理解GSI号、I/O APIC的REDIR\_TB表、IRQ\_PIN引脚和Linux使用的irq号之间的对应关系。

GSI号是ACPI规范引入的，用于记录I/O APIC的IRQ\_PIN引脚号的参数。如果x86处理器系统使用I/O APIC管理外部中断请求，而且在这个处理器系统中具有多个I/O APIC控制器，那么GSI号与I/O APIC中断引脚号的对应关系如图15-1所示。

![[pci_express/33b335513bc0bdf74a4b4ef13d00ba06e58df8d51f2bb097da4c1f09c65eb96c.jpg]]  
图15-3 GSI和IOAPIC中断引脚号的对应关系

假设在一个x86处理器系统中存在3个I/O APIC，其中有两个I/O APIC的外部中断引脚数为24根，另外一个I/O APIC的外部中断引脚数为16根。其中GSI号的 $0\sim 23$ 与I/OAPIC1的IRQ\_PIN0 $\sim 23$ 对应；GSI号的 $24\sim 39$ 与I/O APIC2的IRQ\_PIN0 $\sim 15$ 对应；而GSI号的 $40\sim 55$ 与I/O APIC3的IRQ\_PIN0 $\sim 23$ 对应。ACPI规范为统一起见使用GSI号描述外部设备与I/O APIC中断引脚的连接关系。

I/O APIC 的 IRQ\_PIN 引脚与外部设备的中断请求引脚相连，如 I/O APIC1 的 IRQ\_PIN16 与某个 PCI 设备的 INTA#相连。值得注意的是，PCI 设备的 INTA#信号首先与 LPC 的 PIRQA#信号相连，而 PIRQA#信号再与 I/O APIC1 的 IRQ\_PIN16 相连。其中 I/O APIC 集成

在 ICH 中，因此这些 IRQ\_PIN 引脚并没有从 ICH 中引出。

REDIR\_TB 表中存放对 IRQ\_PIN 引脚的描述，一个 I/O APIC 具有多少个 IRQ\_PIN 引脚，REDIR\_TB 表就由多少项组成。该表的每一个 Entry 由多个字段组成，其中本节仅对这个 Entry 的 Vector 字段感兴趣，Vector 字段是这个 Entry 的第 7\~0 位，存放对应 IRQ\_PIN 引脚使用的中断向量。

在Linux系统中，与IRQ\_PIN引脚对应的中断向量由acpi register\_gsi函数设置，当x86处理器系统使用I/O APIC管理外部中断时，acpi register\_gsi函数将调用mp register\_gsi函数。mp register\_gsi函数在./drivers/acpi/boot.c文件中定义，其实现机制如源代码15-2所示。我们假定在Linux系统中使能了CONFIG\_X86\_32选项。

源代码15-2 mp register\_gsi函数  
int mp_register_gsi(struct device \*dev，u32 gsi，int trigger，int polarity)   
{ int ioapic; int ioapic pinch;   
... ioapic $=$ mp_find_ioapic(gsi); if（ioapic $<  0$ ）{ printf(KERN WARNING "No IOAPIC for GSI %u\n"，gsi）; return gsi; } ioapic pinch $=$ mp_find_ioapic pinch( ioapic,gsi);   
#ifndef CONFIG_X86_32 if（ioapic_renumber_irq） gsi $=$ ioapic_renumber_irq( ioapic,gsi);   
#endif if（ioapic pinch $>$ MP_MAX_IOAPIC_PIN）{ printf(KERN_ERR "Invalid reference to IOAPIC pin" "%d-%d\n",mp_ioapics[ioapic].apicid, ioapic pinch); return gsi; } if (enable_update_mptable) mp_config_acpi_gsi(dev,gsi,trigger,polarity); set_IO_apic_irq_attr(&irq_attr,ioapic,ioapic pinch,

```c
trigger = ACPI_EDGESENSITIVE ? 0 : 1,
polarity = ACPIACTIVE_HIGH ? 0 : 1);
io_apic_set_pci Routing(dev, gsi, &irq_attr);
return gsi; 
```

这段程序首先根据GSI号，使用mp\_find Ioapic和mp\_find Ioapic pinch函数，确定当前PCI设备与I/O APIC中断控制器的哪个IRQ\_PIN引脚相连（GSI号与I/O APIC和IRQ\_PIN引脚的对应关系如图15-1所示）。

然后 mp register\_gsi 函数调用 io\_apic\_set\_pci Routing 函数设置 I/O APIC 中的寄存器。在 Linux x86 的源代码中，mp register\_gsi 函数调用 io\_apic\_set\_pci Routing 函数时，有一个并不恰当的处理，在 mp register\_gsi 函数中使用 GSI 号作为 io\_apic\_set\_pci Routing 函数的第二个入口参数，但是 io\_apic\_set\_pci Routing 函数要求的这个输入参数是irq号。

在Linux x86系统中，irq号是一个纯软件概念，而这段代码的作用实际上是令GSI号直接等于irq号。笔者认为这种方法并不十分恰当，因为GSI号用来描述I/O APIC的IRQ\_PIN输入引脚，而irq号是设备驱动程序用来挂接中断服务例程的。

本节在此强调这个问题，主要为了读者辨明 GSI 号和irq 号的关系，目前在Linux x86系统中，PCI设备使用的GSI号与irq号采用了“直接相等”的一一映射关系，实际上，GSI号并不等同于irq号。在系统软件的实现中，两者只要建立一一映射的对应关系即可，并不一定要“直接相等”。还有一点需要提醒读者注意，就是不同的PCI设备可以共享同一个GSI号，即共享I/O APIC的一个IRQ\_PIN引脚，从而在Linux系统中共享同一个irq号。

io\_apic\_set\_pci Routing 函数调用\_\_io\_apic\_set\_pci Routing $\rightarrow$ setup\_IO\_APIC\_irq 操作 I/O APIC 中的寄存器。setup\_IO\_APIC\_irq 是一个重要函数，如源代码 15-3 所示。

源代码15-3 setup\_IO\_API\_irq函数  
static void setup_IO_API_irq(int apic_id, int pin, unsigned intirq, structirq_desc \*desc,int trigger,int polarity)   
{ structirq_cfg\*cfg; struct IO_API-route_entry entry; unsigned intdest;   
... CFG $=$ desc- $\rightharpoondown$ chip_data; if assign_irq_vector(irq，cfg，TARGET_CPUS） return;   
if (setup Ioapic_entry(mp Ioapics[apic_id].apicid，irq,&entry,

```c
dest, trigger, polarity,cfg->vector, pin) {
    printf("Failed to setup ioapic entry for ioapic %d, pin %d\n",
        mpIoapics[apic_id].apicid, pin);
    __clear_irq_vector(irq,cfg);
    return;
} 
```

该函数首先调用 assign\_irq\_vector $\rightarrow$ \_assign\_irq\_vector 函数将外部设备使用的 GSI 号与 I/O APIC 中 REDIR\_TB 表建立联系，并将其结果记录到 CPU 的 vector\_irq 表中。这个步骤非常重要，在 Linux x86 系统中，如果存在多个 CPU，那么每一个 CPU 都有一个 vector\_irq 表，这张表中包含了 vector 号与 IRQ 号的对应关系。这张表也是处理器硬件与系统软件联系的桥梁。

处理器硬件并不知道irq号的存在，而仅仅知道vector号，而Linux x86系统使用的是irq号。在处理外部中断请求时，Linux系统需要通过vector\_irq表将vector号转换为irq号才能通过irq\_desc表找到相关设备的中断服务例程。

setup Ioapic\_entry 函数将初始化 entry 参数。该参数是一个 IO\_APIC-route\_entry 类型的结构。而 ioapic register intr 函数调用 set\_irq CHIP\_andhandler\_name 函数设置irq\_desc[irq] 变量，并将这个变量的 chip 参数设置为 ioapic CHIP, handle\_irq 参数设置为 handle\_fasteo\_IRQ，这个步骤对于 Linux x86 中断处理系统非常重要。

ioapic\_write\_entry 函数将保存在 entry 参数中的数据写入到与 GSI 号对应的 REDIR\_TB 表中，该函数将直接操作 I/O APIC 的寄存器。

由以上描述，我们可以发现当acpi\_pci\_irq\_enable函数执行完毕后，Linux系统将GSI号与irq号建立映射关系，同时又将irq号与I/O APIC中的vector号进行映射，并将这个映射关系记录到vector\_irq表中，这个映射表由操作系统使用。之后该程序还将初始化I/O APIC的REDIR\_TBL表，将PCI设备使用的GSI号与I/O APIC的vector号联系在一起。

在x86处理器系统中，PCI设备的INTx引脚首先与LPC的PIRQA～H引脚直接相连，而LPC中的PIRQA～H引脚将与I/O APIC的IRQ\_PIN16～23引脚相连。当PCI设备通过INTx引脚提交中断请求时，最终将传递到IRQ\_PIN16～23引脚。而I/O APIC接收到这个中断请求后，将根据REDIR\_TB表与“IRQ\_PIN16～23引脚”对应的Entry向Local APIC发送中断请求消息，处理器通过Local APIC收到这个中断请求后，将执行中断处理程序进一步处理这个来自PCI设备的中断请求。

Linux x86 系统使用 do\_IRQ 函数处理外部中断请求，该函数在 ./arch/x86/kernel/irq.c 文件中，如源代码 15-4 所示。

unsigned int __irq_entry do_IRQ(struct ptRegs *regs)   
{ struct ptRegs \* oldRegs $=$ set_irqRegs(regs); /\*high bit used in ret_from_code\*/ unsigned vector $=$ ~regs- $\rightharpoondown$ orig_ax; unsignedirq; exit idle(); IRQ-enter(); $\mathrm{irq} =$ _get_cpu_var( vector_irq)[vector]; if(!handle_irq(irq,regs)){ ack_APIC_irq(); if (printk_ratelimit()) pr_emerg("%s:%d.%d Noirq handler for vector (irq %d)\n", __func_,tmp Processor_id(),vector,irq); } IRQ_exit(); set_irqRegs(oldRegs); return 1;

do\_IRQ函数首先获得vector号，这个vector号由I/O APIC传递给Local APIC，并与某个IRQ\_PIN引脚对应，其描述在I/O APIC的REDIR\_TBL表中。vector号是一个硬件概念，x86处理器系统在处理外部中断请求时，仅仅知道vector号的存在，而不知道irq号。

Linux x86 系统通过 vector\_irq 表，将 vector 号转换为 IRQ 号，之后执行 handle\_irq 函数进一步处理这个中断请求。对于 PCI 设备，这个 handle\_irq 函数将调用 handle\_fastei\_irq 函数，而 handle\_fastei\_irq 函数将最终执行 PCI 设备使用的中断服务例程。handle\_fastei\_irq 函数的源代码在 ./kernel/irq/chip.c 文件中，本节对该函数不做进一步分析。

在PCI设备的Linux驱动程序中，将使用request\_irq函数将其中断服务例程挂接到系统中断服务处理程序中。

# 15.1.2 PCI中断路由表

上节简要介绍了PCI设备如何获取中断向量。由上文所述，PCI设备在获取中断向量之前需要从acpi\_prt\_list链表获得GSI号，在acpi\_prt\_list链表中存放PCI总线的中断路由表，

而这个中断路由表中存放PCI设备所使用的GSI号。

这个PCI中断路由表由BIOS提供，如果x86处理器系统支持ACPI机制，这个中断路由表存在于DSDT.dsl文件中，如源代码15-5所示。ACPI规范使用ASL语言描述PCI中断路由表。

源代码15-5 DSDT表中的PCI中断路由表  
```txt
Device (PCIO)   
{   
...   
Method (_PRT, 0, NotSerializable)   
{ If (LEqual (GPIC, Zero)) { Package (0x04) \{0x0001FFFF, 0x00, \_SB. PCIO. LPC. LNKA, 0x00\}, Package (0x04) \{0x0001FFFF, 0x01, \_SB. PCIO. LPC. LNKB, 0x00\}, Package (0x04) \{0x0001FFFF, 0x02, \_SB. PCIO. LPC. LNKC, 0x00\}, Package (0x04) \{0x0001FFFF, 0x03, \_SB. PCIO. LPC. LNKD, 0x00\}, ... } Else { Return (Package (0x47) { ... Package (0x04) \{ 0x001CFFFF, Zero, Zero, 0x11\}, Package (0x04) \{ 0x001CFFFF, One, Zero, 0x10\}, Package (0x04) \{ 0x001CFFFF, 0x02, Zero, 0x12\}, Package (0x04) \{ 0x001CFFFF, 0x03, Zero, 0x13\}, Package (0x04) \{ 0x001DFFFF, Zero, Zero, 0x17\}, Package (0x04) \{ 0x001DFFFF, One, Zero, 0x13\}, Package (0x04) \{ 0x001DFFFF, 0x02, Zero, 0x12\}, Package (0x04) \{ 0x001DFFFF, 0x03, Zero, 0x10\}, ... } } 
```

在以上源代码中，\_PRT存放x86处理器系统PCI总线0的中断路由表，在x86处理器体系结构中，每一条PCI总线都有一个中断路由表，因此在DSDT中，将存在多个中断路由表。在以上源代码中，我们仅列出PCI总线0使用的中断路由表，即RC使用的中断路由表，在一个处理器系统中还可能有其他中断路由表，如PCIe桥使用的中断路由表等。

在以上源代码中，首先判断GPIC是否为0，如果为0表示当前x86处理器系统使用PIC

模式，即使用8259A中断控制器管理外部中断，在第15.1.3节将介绍这种情况；如果为1表示当前x86处理器系统使用I/O APIC管理外部中断，此时“Package（0x04）” $^{204}$ 中含有四个参数，这四个参数的定义如表15-1所示。

表 15-2 PCI 中断路由表使用的参数

<table><tr><td>参数</td><td>类型</td><td>描述</td></tr><tr><td>Address</td><td>DWORD</td><td>设备地址，其中高两个字节表示PCI设备的Device号，低两个字节表示PCI设备的Function号，如果低两字节为0xFFFF表示全部Function号</td></tr><tr><td>Pin</td><td>Byte</td><td>其中0~3分别与INTA~D#引脚#对应</td></tr><tr><td>Source</td><td>Name Path或者Byte</td><td>该字段为0表示使用GSI号描述PCI设备使用的中断资源，否则该字段存放该设备与LPC的哪个PIRQ引脚，如LPC的PIRQA信号连接，在第15.1.3节将讲述LPC的PIRQ引脚的描述</td></tr><tr><td>Source Index</td><td>DWORD</td><td>Source字段为0时，该字段存放PCI设备使用的GSI号</td></tr></table>

通过以上描述，发现“Package（0x04）{0x0001FFFF，0x00，\_SB.PCIO.LPC.LNKA， $0x00$ ”的含义为，PCI总线0的某个设备，其Device号为 $0\mathrm{x}01$ ，而且这个设备的INTA#引脚与LPC的PIRQA相连，INTB#引脚与PIRQB相连，INTC#引脚与PIRQC相连，而INTD#引脚与PIRQD相连。

而“Package（0x04）{0x001CFFFF，Zero，Zero，0x11}”这段代码的含义为，PCI总线0的某个PCI设备，其Device号为0x1C，而且这个设备的INTA#引脚使用的GSI号为0x11；这个PCI设备的INTB#引脚使用的GSI号为0x10；这个PCI设备的INTC#使用的GSI号为0x12，这个PCI设备的INTD#引脚使用的GSI号为0x13。

Linux x86 系统进行初始化时，将\_PRT 表加载到 acpi\_prt\_list 链表中，操作系统首先执行 acpi\_pci\_root\_init 函数，之后调用 acpi\_deviceprobe $\rightarrow$ acpi\_BUS\_driver\_init $\rightarrow$ acpi\_pci\_root\_add 函数。acpi\_pci\_root\_add 函数将调用 acpi\_pci\_irq\_add\_prt $\rightarrow$ acpi\_pci\_irq\_add\_entry 函数将\_PRT 表中的中断路由表的每一个 Entry 加载到 acpi\_prt\_list 链表。

通过上文的分析，可以发现在每一个PCI桥中，包括Switch的虚拟PCI桥中都有一个中断路由表，因此acpi\_pci\_root\_add还会调用acpi\_pci\_bridge\_scan函数分析并加载每一个PCI桥的中断路由表。对Linux x86系统初始化PCI中断路由表感兴趣的读者可以自行分析这段代码，本节对此不做进一步介绍。

在Linux x86系统中，PCI设备在获取irq号时，将从这个链表中获得GSI号，从而最终获得irq号，具体过程见第15.1.1节。

# 15.1.3 PCI插槽使用的irq号

在 x86 处理器系统中，还有一类特殊的 PCI 设备，即 PCI 插槽。PCI 插槽无法确定其上的 PCI 设备如何使用 INTA# \~ INTD# 信号，因此必须处理全部中断请求引脚，而在其上的

PCI设备有选择地使用这些信号。

PCI插槽使用的中断请求信号将与LPC的PIRQA～F相连，如果处理器系统使能了I/O APIC，LPC的这些中断请求引脚将与IRQ\_PIN16～23相连，否则中断控制器8259A将管理这些中断引脚。在ACPI表中含有对这些PCI插槽中断请求信号的描述，这些描述主要针对处理器系统没有使用I/O APIC的处理情况，如源代码15-6所示。

# 源代码15-6 PCI插槽使用中断请求信号

```txt
Device (LPC)   
{   
... Device (LNKA)   
{ Name (_HID, EisaId ("PNPOC0F")) Name (_UID, 0x01) Method (_STA, 0, NotSerializable) { If (And (PIRA, 0x80)) { Return (0x09) Else { Return (0x0B) } } Method (_DIS, 0, NotSerializable) { Or (PIRA, 0x80, PIRA) } Method (_CRS, 0, NotSerializable) { Name (BUFO, ResourceTemplate()) IRQ (Level, ActiveLow, Shared, _Y02) {0} } CreateWordField (BUF0, \_SB. PCI0. LPC. LNKA. _CRS. _Y02. _INT, IRQW) If (And (PIRA, 0x80)) { Store (Zero, Local0) } Else 
```

```txt
{
    Store (One, Local0) {
        ShiftLeft (Local0, And (PIRA, 0x0F), IRQW)
        Return (BUFO)
    }
    Name (_PRS, ResourceTemplate())
    {
        IRQ (Level, ActiveLow, Shared, )
        {3,4,5,7,9,10,11,12}
    }
    Method (_SRS, 1, NotSerializable)
    {
        CreateWordField (Arg0, 0x01, IRQW)
        FindSetRightBit (IRQW, Local0)
        If (LNotEqual (IRQW, Zero))
            {
                And (Local0, 0x7F, Local0)
                Decrement (Local0)
            }
        Else
            {
                Or (Local0, 0x80, Local0)
            }
        Store (Local0, PIRA)
    }
} 
```

在ACPI规范中，PCI插槽的中断请求信号的标识符“PNPOC0F”。在这段源代码中LNKA与LPC的PIRQA引脚对应，这段代码的作用是描述LPC的PIRQA引脚。在ICH中，使用PIRQA\_ROUT寄存器描述PIRQA引脚。在以上这段源程序中，“\_STA”、“\_DIS”、“\_CRS”、“\_PRS”和“\_SRS”可以操作PIRQAROUT寄存器，具体含义如下所示。

\_STA用来测试当前PIRQA引脚的状态，这段代码判断PIRQA\_ROUT寄存器的第7位是否为1，如果为1表示当前PIRQ引脚并没有与8259A相连，此时I/OAPIC将管理该引脚，\_STA将返回0x09表示PIRQA没有与8259A相连；否则返回0x0B，表示PIRQA与8259A相连。\_STA的返回值在ACPI规范中具有明确的定义。

\_\_DIS用来关闭PIRQA引脚与8259A的联系，即使用I/O APIC管理该引脚。\_\_DIS的作用是将PIRQA\_ROUT寄存器的第7位置1。

\_CRS用来获得当前资源的描述，对于PIRQA引脚而言，这段描述表示PIRQA引脚使用“低电平有效的共享中断请求”，随后通过PIRQ[A]\_ROUT寄存器的最高位判断，该中断信号是由8259A中断控制器还是APIC中断控制器接管，最后将IRQW根据PIRQ[A]

ROUT 寄存器的 IRQ Routing 字段赋值，IRQ Routing 字段可以使用的资源在 \{3, 4, 5, 7, 9, 10, 11, 12\} 集合中。

\_PRS 描述 PCI 插槽的中断请求信号可能使用的中断资源，对于 PIRQA 而言，可能使用的 IRQ 号为 \{3, 4, 5, 7, 9, 10, 11, 12\}。这些 IRQ 号由 x86 处理器系统规定，这些 IRQ 号与 ISA 总线兼容，如果一个系统使用了 I/O APIC，这些规定将不再有效。

在Linux系统中，acpi\_pci\_link\_init函数处理PCI插槽的中断请求，该函数在./drivers/acpi/pci\_link.c文件中，其实现如源代码15-7所示。

源代码15-7 acpi\_pci\_link\_init函数  
```c
static int __init acpi_pci_link_init(void)  
{  
    ...  
    if (acpi_BUS_registerDriver(&acpi_pci_link_driver) < 0)  
        return -ENODEV;  
    return 0;  
}  
subsys_initcall(acpi_pci_link_init); 
```

acpi\_pci\_link\_init 函数调用 acpi\_BUS\_registerDriver $\rightarrow$ . . $\rightarrow$ acpi\_pci\_link\_add 函数将 LPC 的 PIRQA $\sim$ H 引脚与irq号对应在一起。acpi\_pci\_link\_add 函数的执行过程较为简单，首先该函数调用 acpi\_pci\_link\_get POSSIBLE 函数，运行\_PRS 代码获得{3,4,5,7,9,10,11,12} 这个集合；之后调用 acpi\_pci\_link\_get\_current 函数，运行\_CRS 代码并从{3,4,5,7,9,10,11,12} 集合中获得irq号。acpi\_pci\_link\_init 函数执行完毕后，Linux 系统将显示以下信息。

```txt
ACPI: PCI Interrupt Link [LNKA] (IRQs 3457910*1112)  
ACPI: PCI Interrupt Link [LNKB] (IRQs 34579*101112)  
ACPI: PCI Interrupt Link [LNKC] (IRQs 3457910*1112)  
ACPI: PCI Interrupt Link [LNKD] (IRQs 3457910*1112)  
ACPI: PCI Interrupt Link [LNKE] (IRQs 3457*9101112)  
ACPI: PCI Interrupt Link [LNKF] (IRQs 34579*101112)  
ACPI: PCI Interrupt Link [LNKG] (IRQs 3457*9101112)  
ACPI: PCI Interrupt Link [LNKH] (IRQs 3457910*1112) 
```

其中LNKA使用IRQ11，LNKB使用IRQ10，并以此类推。如果一个处理器系统使能了I/O APIC，acpi\_pci\_link\_init函数的执行结果并不重要，因为PCI设备在执行pci\_enable\_device函数后，该设备使用的irq号，还将发生变化。

目前Linux x86系统在大多数情况下，都会使能I/O APIC，在这种情况下，即便不执行acpi\_pci\_link\_add函数对系统也没有什么影响，也正是基于这个考虑，本节对acpi\_pci\_link\_init函数并不做深入研究。

# 15.2 使用MSI/MSIX中断机制申请中断向量

上文讲述了ACPI如何为PCI设备或者“使用INTx Emulation方式”的PCIe设备分配中断向量。本节讲述PCIe设备使用MSI/MSIX中断机制时，Linux系统如何分配中断向量。对于PCI设备，MSI/MSIX中断机制是可选的，但是PCIe设备必须支持MSI或者MSI-X中断机制，或者同时支持这两种中断机制。

# 15.2.1 Linux 如何使能 MSI 中断机制

如果PCI/PCIe设备需要使用MSI中断机制，将调用pci\_enable\_msi函数，在Linux2.6.31内核中，pci\_enable\_msi函数使用pci\_enable\_msi\_block(pdev,1)实现。pci\_enable\_msi\_block函数在 ./drivers/pci/msi.c文件中，如源代码15-8所示。pci\_enable\_msi\_block函数具有两个入口参数，其中dev参数存放PCIe设备的pci\_dev结构，而nvec参数为申请的irq号个数。

该函数返回值为0时，表示成功返回，此时该函数将更新pci\_dev $\rightarrow$ irq参数，此时在Linux设备驱动程序中，可以使用的irq号在pci\_dev $\rightarrow$ irq\~pci\_dev $\rightarrow$ irq+nvec-1之间；当函数返回值为负数时，表示出现错误；而为正数时，表示pci\_enable\_msi\_block函数没有成功返回，返回值为该PCIe设备MSICabalibilities结构的MultipleMessageCapable字段。

源代码15-8pci\_enable\_msi\_block函数  
intpci_enable_msi_block(structpci_dev \*dev，unsigned intnvec)   
{ intstatus，pos，maxvec; u16msgctrl; pos $=$ pcifindcapability（dev，PCI_CAP_ID_MSI）； if（！pos） return-EINVALID; pciread_config_word（dev，pos $^+$ PCI_MSI_FLAGS,&msgctrl）; maxvec $= 1 <   <   (($ msgctrl&PCI_MSI_FLAGS_QMASK） $> > 1$ ）; if(nvec $>$ maxvec) return maxvec;   
status $\equiv$ pcimsi_check_device（dev，nvec，PCI_CAP_ID_MSI）; if(status) return status;   
WARN_ON(!！dev- $\rightharpoondown$ msienabled); /\*CheckwhetherdriveralreadyrequestedMSI-Xirqs\*/ if（dev- $\rightharpoonup$ msix.enabled）{

```c
dev_info(&dev->dev, "can't enable MSI "
	" (MSI - X already enabled) \\n");
return - EINVALID;
\}
status = msiCapability_init (dev, nvec);
return status; 
```

这段代码首先检查 PCI 设备是否支持 MSI 中断机制，如果不支持将直接退出该函数。否则检查 nvec 参数和 Multiple Message Capable 字段的大小，如果 nvec 的值较大时，该函数直接使用 Multiple Message Capable 字段返回。

如果pci\_enable\_msi\_block函数通过了这些检查，将调用pci\_msi\_check\_device函数，检查Linux系统是否能够使能PCI设备的MSI中断机制。这个检查包含两方面内容，一方面是纯软件层面的，包括检查全局变量pci\_msi\_enable、pci\_dev $\rightarrow$ no\_msi参数等；一方面是硬件层面的检测，包括当前PCI设备的上游PCI桥是否支持MSI报文的转发，PCI设备是否具有Capabilities链表，是否具有MSI Capability结构。完成这些检查后，pci\_enable\_msi将进一步调用msi Capability\_init函数，完成与MSI中断相关的设置，msi Capability\_init函数的实现如源代码 $15 - 9\sim 10$ 所示。

源代码15-9 msi\_capability\_init函数片段1   
static int msicapability_init(struct pcie_dev *dev, int nvec)  
{ struct msi_desc *entry; int pos, ret; u16 control; unsigned mask; pos = pcifindcapability(dev,PCI_CAP_ID_MSB); msi_set_enable(dev，pos，0）；/\*DisableMSI during set up \*/ pcie_read_config_word(dev，msi_control_reg(pos)，&control）； /\*MSI Entry Initialization \*/ entry $=$ alloc_msi_entry(dev）; if（！entry） return -ENOMEM; entry- $\rightharpoondown$ msi_attribute.is_msis=0; entry- $\rightharpoonup$ msi_attribute.is_64 $\equiv$ is_64bit_address(control); entry- $\rightharpoondown$ msi_attribute.entry_NR $= 0$ ： entry- $\rightharpoondown$ msi_attribute-maskbit $=$ is_mask_bit_support(control);

```c
entry - > msi_attribute. default_irq = dev - >irq; /* Save IOAPIC IRQ */
entry - > msi_attribute. pos = pos;  
entry - > mask_pos = msi_mask_reg( pos, entry - > msi_attribute.is_64);  
/* All MSIs are unmasked by default, Mask them all */  
if (entry - > msi_attribute-maskbit)  
    pci_read_config_dword(dev, entry - > mask_pos, &entry - > masked);  
mask = msi_capable_mask(control);  
msi_mask_irq( entry, mask, mask);  
list_addTAIL(&entry - > list, &dev - > msi_list); 
```

msi\_capacity\_init函数具有两个入口参数，在Linux2.6.30内核中，该函数具有一个入口参数，仅能获得一个irq号。msi\_capacity\_init函数参考了msix\_capacity\_init函数的实现机制。这段代码置PCI设备的MSI Capability结构的Enable位为0，msi\_capacity\_init函数需要对MSI Capability结构进行读写操作，因此需要暂时禁止当前设备使用MSI中断机制。

这段程序随后读取MSI Capability结构的Message Control字段，并暂时保存在control变量中，在control变量中存放PCIe设备使用的MSI Capability结构的格式，如图10-1所示，MSI Capability结构可以使用4种格式。

最后这段程序调用alloc\_msi\_entry函数分配一个msi\_desc结构的entry参数，并将其初始化后，加入到pci\_dev $\rightarrow$ msi\_list链表中。在entry参数中存放该PCI设备使用的MSI中断机制的详细信息。

源代码15-10 msi\_capability\_init函数片段2   
/\*Configure MSI capability structure \*/ ret $=$ arch_setup_msi_irqs(dev,nvec,PCI_CAP_ID MSI); if(ret){ msi_mask_irq(entry，mask，\~mask）; msi_free_irqs(dev）; return ret;   
1   
/\*SetMSIenabledbits\*/ pcix_intx_for_msi(dev，0）; msi_set_enable(dev，pos，1）; dev- $\rightharpoondown$ msi-enabled $= 1$ · dev- $\rightharpoonup$ irq $\equiv$ entry- $\rightharpoondown$ irq; return0;

这段代码继续调用arch\_setup\_msi\_irqs函数设置MSICapability结构的其他字段，并设置

entry结构的irq参数，arch\_setup\_msi\_irqs函数的实现与体系结构相关，下文将分别介绍x86和PowerPC处理器的实现方式。

然后这段代码调用pci\_intx\_for\_msi函数，关闭PCI设备配置空间Command寄存器的Interrupt Disable位，因为该PCI设备将使用MSI中断机制，而不是传统的INTx中断机制；并调用msi\_set\_enable函数使能MSI Capability结构的Enable位；最后对pci\_dev $\rightarrow$ msi-enabled位置1，并将pci\_dev $\rightarrow$ irq参数赋值。

# 1. Linux x86

Linux x86 使用的 arch\_setup\_msi\_irqs 函数在 ./arch/x86/kernel/apic/io\_apic.c 文件中，其实现如源代码 15-1 所示，本节并不关心 intr\_remapping-enabled 参数为 1 的情况，该参数与 IOMMU 机制的 IRQ Remapping 相关。

源代码15-11 Linux x86使用的arch\_setup\_msi\_irqs函数  
int arch_setup_msi_irqs(structpci_dev \*dev，int nvec,int type)   
{ /\*x86 doesn't support multiple MSI yet \*/ if(type $=$ =PCI_CAP_ID MSI&&nvec $>1$ ） return 1; node $=$ dev_to_node(&dev- $\rightharpoondown$ dev); IRQ_want $\equiv$ nr_irqs_gsi; sub_handle $= 0$ . list_for_each_entry(msidesc，&dev- $\rightharpoonup$ msi_list，list）{IRQ $=$ create_irq_nr(irq_want，node）; if（irq $= = 0$ ） return -1; IRQ_want $\equiv$ irq $+1$ · if(!intr_remappingenabled) goto no_ir;   
...   
no_ir: ret $=$ setup_msi_irq(dev，msidesc，irq）; if（ret $<  0$ ） goto error; sub_handle $+ +$ · } return 0;   
error: destroy_irq(irq); return ret;

这段代码首先判断 type 是否为 PCI\_CAP\_ID MSI，而且 nvec 参数是否大于 1，如果满足这两个条件，该函数将直接返回 1。通过这段代码可以发现，虽然在 Linux 2.6.31 内核中定义了一个新的pci\_enable\_msi\_block函数，但是PCIe设备依然只能使用一个中断向量号。

这段程序随后调用 create\_irq(nr) 函数，分配 PCI 设备使用的 IRQ 号，并将其保存到 IRQ 变量中，之后调用 setup\_msi\_irq 函数初始化当前 PCI 设备的 MSI Capability 结构。setup\_msi\_irq 函数是一个重要函数，其实现如源代码 15-12 所示。

源代码15-12 setup\_msi\_irq函数  
static int setup_msi_irq(struct pci_dev \*dev, struct msi_desc \*msidesc, int irq)   
{ int ret; struct msi msg msg; ret $=$ msiCompose msg(dev,irq,&msg); ifret0 return ret; set_irq_msi(irq,msidesc); write_msi msg(irq,&msg); if (irq_remapped(irq)) { struct irq_desc \*desc $=$ irq_to_desc(irq); desc->status $\mid =$ IRQ_MOVE_PCNTXT; set_irq chip_andhandler_name(irq,&msi_ir_chip, handle_edge_irq,"edge"); } else set_irq chip and handler name(irq,&msi chip, handle_edge_irq,"edge"); dev_printk(KERN_DEBUG，&dev->dev，"irq%d for MSI/MSI-X\n",irq）; return 0;

这段代码首先调用 msi\_compose msg 函数，初始化 msg 结构的 address\_hi、address\_lo 和 data 参数，与 MSI Capability 结构的 Message Upper Address、Message Address 和 Message Data 字段对应。

对于x86处理器系统，Message Address字段的格式见图10-1，其中Destination ID字段与CPU的ACPI ID相关，Linux x86使用cpu\_mask\_to\_apicid\_and函数获得该字段的值；而Message Data字段的格式如图10-1所示，其Vector字段由assign\_irq\_vector函数设置，Trig-

ger Mode 字段为 0x00 表示使用边沿触发方式，而 Delivery Mode 字段为“Fixed Mode”或者“Lowest Priority”。值得注意的是，在 Message Data 字段中存放的是中断向量号（vector），是一个硬件的概念，而设备驱动程序中使用的irq号是一个软件关系，两者之间存在对应关系，但是并不等同。

set\_irq\_msi函数设置PCIe设备使用的irq\_desc结构；而write\_msi msg函数将msg结构中的参数写入到PCIe设备的对应寄存器中；而set\_irq CHIP\_andhandler\_name函数设置MSI中断使用的中断处理程序。本节对这些函数不做进一步介绍。

# 2. PowerPC

Linux PowerPC 使用的 arch\_setup\_msi\_irqs 函数在 ./arch/powerpc/kernel/msi.c 文件中，对于 Freescale 的 PowerPC 处理器，该函数等效与 fsl\_setup\_msi\_irqs。fsl\_setup\_msi\_irqs 函数的实现，如源代码 15-13 所示。

源代码15-13 fsl\_setup\_msi\_irqs函数  
```c
static int fsl_setup_msi_irqs(struct pci_dev *pdev, int nvec, int type)  
{  
    list_for_each_entry( entry, &pdev->msi_list, list) {  
        hirq = msi_bitmap_alloc_hirqs(&msi_data->bitmap, 1);  
    }  
    virq = irq_createmapping(msi_data->irqhost, hirq);  
    set_irq_msi(virq, entry);  
    fsl_compose_msi msg(pdev, hirq, &msg);  
    write_msi msg(virq, &msg);  
}  
return 0;  
out_free:  
return rc; 
```

该函数的实现机制与x86处理器类似，值得提醒读者注意的是在fslCompose\_msi msg函数中 $\mathrm{msg}\rightarrow$ address\_hi等于fsl\_msi $\rightarrow$ msi\_addr\_lo，其值为MSIIR寄存器在PCI总线域的物理地址。在该函数中使用的address\_hi、address\_lo和data参数的详细描述见第10.2节。本节对此不一一叙述。目前在PowerPC处理器系统中，PCIe设备也只能使用一个中断向量号。

