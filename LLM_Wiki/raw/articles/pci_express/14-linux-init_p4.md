---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "14"
section: "14.3.3 Linux PCI检查PCI设备使用的BAR空间"
part: 4
tags: [pci, pci-express, computer-architecture]
---
# 14.3.3 Linux PCI检查PCI设备使用的BAR空间

当acpi\_pci\_link\_init函数执行完毕后，LinuxPCI开始执行pci\_subsys\_init函数。在第14.1.3节曾简要介绍了该函数的实现，该函数如源代码14-9所示。

当一个处理器系统使能了ACPI机制，pci\_subsys\_init函数的执行路径将会发生变化。该函数将首先执行pci\_acpi\_init函数，并跳过pcilegacy\_init和pcibios\_irq\_init函数之后，执行pcibios\_init函数。pci\_acpi\_init函数的实现较为简单，其源代码在 ./arch/x86/pci/acpi.c 文件中，如源代码14-33所示。

源代码14-33pci\_acpi\_init函数  
int_initpci_acpi_init(void)   
{ structpci_dev \*dev $=$ NULL; if(pcibios_scanned) return0; if(acpi_noirq) return0; printf(KERN_INFO"PCI:UsingACPIforIRQrouting\n"); acpi_irq_penalty_init(); pcibios_scanned $+ +$ . pcibios_enable_irq $=$ acpi_pci_irq_enable; pcibios_disable_irq $=$ acpi_pci_irq_disable; if(pciROUTEirq){ for_each_pci_dev(dev) acpi_pci_irq_enable(dev); } return0;

该函数首先调用acpi\_irq\_penalty\_init函数更新acpi\_irq\_penalty表，该函数与Linux系统使用的IRQ Balance技术相关，对此感兴趣的读者可以从http://www.irqbalance.org网站获得更多的信息，本书并不关心这部分内容。

这段程序将 pcibios\_scanned 参数置 1，并将 pcibios\_enable\_irq 和 pcibios\_disable\_irq 参数初始化为 acpi\_pci\_irq\_enable 和 acpi\_pci\_irq\_disable。这也是 Linux 系统使能 ACPI 机制后，

Linux PCI 并不执行 pcillegacy\_init 和 pcibios\_irq\_init 函数的原因。最后这段程序使用 acpi\_pci\_irq\_enable 函数为当前 PCI 总线树上的所有 PCI 设备分配 IRQ 号。

如果当前处理器系统使能了ACPI机制，pci\_acpi\_init函数执行后，pci\_subsys\_init函数将执行pcibios\_init函数。pcibios\_init $\rightarrow$ pcibios\_resourcesurvey函数将检查当前处理器系统的所有PCI设备的BAR空间，该函数并不会操作PCI设备的BAR寄存器，而只是检查当前处理器系统中所有PCI设备的pci\_dev $\rightarrow$ resource参数是否合法。

由第14.3.2节所示，pci\_scan\_slot函数已经将pci\_dev $\rightarrow$ resource参数进行基本的初始化工作，但是对于不同的处理器系统，resource $\rightarrow$ start参数的值并不一定有效。

pcibios\_resourcesurvey函数在 ./arch/x86/pci/i386.c 文件中，如源代码14-34所示。

# 源代码14-34 pcbios\_resourcesurvey函数

```c
void_init pcibios_resourcesurvey(void)   
{ DBG("PCI:Allocating resources\n"); pcibios_allocate_BUS-resources(&pci_root_buses); pcibios_allocate-resources(0); pcibios_allocate-resources(1); e820-reserve-resources_late(); /\* \*Insert the IO APIC resources after PCI initialization has \*occurred to handle IO APICS that are mapped in on a BAR in \*PCI space,but before trying to assign unassigned pci res. \*/ ioapic_insert-resources(); 
```

在Linux x86中，所有PCI总线树的根节点使用一个双向链表连接在一起，pci\_root\_buses指向这个链表的起始地址。pcibios\_allocate\_BUS-resources函数使用DFS算法检查并分配PCI总线树中的所有PCI桥使用的系统资源，函数的源代码在 ./arch/x86/pci/i386.c文件中，如源代码14-35所示。

# 源代码14-35 pcbios\_allocate\_BUS-resources函数

```c
static void __init pcibios_allocate_BUS-resources(struct list_head *bus_list)  
{  
    struct pci_BUS *bus;  
    struct pci_dev *dev;  
    int idx;  
    struct resource *r;  
    /* Depth - First Search on bus tree */  
    list_for_each_entry.bus, bus_list, node); 
```

if((dev $=$ bus $\rightarrow$ self)) { for(idx $=$ PCI_BRIDGE_RESOURCES; idx $<$ PCI_NUM_RESOURCES;idx $+ +$ ）{ r $=$ &dev $\rightarrow$ resource[ idx]； if（！r->flags） continue; if（！r $\rightarrow$ start| pci_claim_resource(dev，idx） $<  0$ ）{ r->flags $= 0$ · } } pcibios_allocate_BUS-resources(&bus $\rightarrow$ children）;

pcibios\_allocate\_BUS-resources 函数首先遍历链表pci\_root\_buses中的所有pci\_BUS结构，之后调用pci\_claimResource $\rightarrow$ pcifind\_parentResource函数对pci\_BUS结构进行检查。pci\_find\_parentResource函数在 ./driver/pci/pci.c文件中，如源代码14-36所示，该函数成功返回时，将获得当前PCI桥的上游PCI桥使用的resource参数。

源代码14-36pci\_find\_parent\_resource函数  
```c
struct resource * 
pci_find_parentResource(const struct pc_i_dev *dev, struct resource *res) 
{
    const struct pc_i.bus *bus = dev->bus;
    int i;
    struct resource *best = NULL;
    for(i=0; i<PCI_BUS_NUM_RESOURCES; i++) {
        struct resource *r = bus->resource[i];
        if(!r) continue;
        if(res->start &&!(res->start >= r->start &&res->end <= r->end)) continue; /* Not contained */
        if((res->flags^r->flags) & (IORESOURCE_IO | IORESOURCE_MEM)) continue; /* Wrong type */
        if(!((res->flags^r->flags) & IORESOURCE_CONFETCH)) return r; /* Exact match */
        if((res->flags & IORESOURCE_CONFETCH) &&!(r->flags & IORESOURCE_CONFETCH)) best = r; /* Approximating prefetchable by non-prefetchable */
    }
    return best;
} 
```

pci\_find\_parent\_resource 首先对 PCI 桥管理的地址空间进行检查。如图 3-2 所示，每一个 PCI 桥都管理一段 PCI 总线地址空间，而且这段地址空间必须隶属于上游 PCI 桥管理的地址空间，其中 PCI 桥 2 管理的地址空间隶属于 PCI 桥 1，而 PCI 桥 1 管理的地址空间隶属于 HOST 主桥，而且这些地址空间的类型需要一致。

之后这段代码检查上下游 PCI 桥的预读设置位，PCI 总线规定下游设备“不可预读空间”不能使用 PCI 桥的“可预读空间”；而下游设备“可预读空间”可以使用 PCI 桥的“不可预读空间”和“可预读空间”，下游设备的“可预读空间”优先使用 PCI 桥的“可预读空间”。

当完成这些检查后 pcbios Allocate\_BUSResources $\rightarrow$ requestResource 函数将从上游 PCI 桥管理的地址空间中为当前 PCI 桥分配地址空间，如果该函数返回失败，则将 $r \rightarrow$ flags 参数置 0，标记资源没有被正确分配，这种情况可能是因为 BIOS 的 bug，也可能因为其他原因。之后 pcbios Allocate\_BUSResources 函数将递归调用 pcbios Allocate\_BUSResources 函数遍历其下游的 PCI 总线树。

我们再次回到 pcbios\_resourcesurvey 函数，发现该函数分别使用两个不同的入口参数 0 和 1 调用了 pcbios\_allocate-resources 函数。当入口参数为 0 时，pcbios\_allocate-resources 函数为“在 BIOS 中已经启用了 PCI 设备”优先分配资源；当入口参数为 1 时，该函数为其他 PCI 设备分配资源。

该函数的实现较为简单，其主要过程依然是调用pci\_find\_parent\_resource函数获得上游PCI桥管理的资源，并使用requestResource函数为当前PCI设备分配地址空间。值得注意的是，当入口参数为0时，pcibiosResourcesurvey函数将暂时禁止PCI设备的ROM空间，ROM空间的初始化将在下文介绍。

pcibios\_init函数主要操作Linux系统中的数据结构，并没有对PCI设备的BAR寄存器进行读写操作。在x86处理器系统中，BIOS会枚举PCI总线树，并初始化PCI设备的BAR寄存器；但是在其他处理器系统中，Firmware可能并没有做出这些操作，为此Linux系统将继续遍历PCI总线树，并初始化这些PCI设备的BAR寄存器。

# 14.3.4 Linux PCI分配PCI设备使用的BAR寄存器

pci\_subsys\_init函数执行完毕后，Linux PCI将调用pcbios\_assign-resources函数，设置PCI设备的BAR寄存器。pcbios\_assign-resources函数首先处理PCI设备的ROM空间，并进行资源分配，之后调用pci\_assign\_unassigned-resources函数设置PCI设备的BAR寄存器。该函数在./drivers/pci/setup-bus.c文件中，如源代码14-37所示。

源代码14-37pci\_assign\_unassigned-resources函数  
```c
void_init  
pci_assign_unassigned-resources(void)  
{  
    struct pcibus *bus;  
    /* Depth first, calculate sizes and alignments of all subordinate buses. */  
    list_for_each_entry.bus, &pci_root_buses, node); 
```

```c
pci_BUS_size_bridge bus);   
\*/ \*Depth last,allocate resources and update the hardware.\*/ list_for_each_entry bus,&pci_root_buses,node）{ pci_BUS_assign-resources(bus)； pci_enable_bridge bus);   
\*/ \*dump the resource on buses \*/ list_for_each_entry bus,&pci_root_buses,node）{ pci_BUS_dump-resources(bus)；   
1
```

该函数依次调用pci\_BUS\_size\_bridge、pci\_BUS\_assign-resources和pci\_enable\_bridge函数，下文将分别讨论这些函数。而pci\_BUS\_dump-resources函数的主要作用是将Linux系统分配的PCI设备的资源信息打印出来，本节对该函数不做介绍。

# 1.pci\_BUS\_size\_bridge函数

pci\_BUS\_size\_bridge函数的主要作用是修复和对界当前PCI总线树下的所有PCI设备（包括PCI桥)所使用的I/O和存储器地址空间。该函数的实现如源代码14-38所示。

源代码14-38pci\_BUS\_size\_bridge函数  
```c
void _ref pci_BUS_size_bridge(struct pci_BUS *bus)  
{  
    struct pci_dev *dev;  
    unsigned long mask, Prefmask;  
    list_for_each_entry(dev, &bus -> devices, bus_list) {  
        struct pci_BUS *b = dev -> subordinate;  
    }  
    switch (dev -> class >> 8) {  
    case PCI_CLASS_BRIDGEPCI:  
        default:  
            pci_BUS_size_bridge(b);  
        break;  
    }  
}  
/* The root bus? */  
if (!bus -> self)  
    return;  
switch (bus -> self -> class >> 8) { 
```

case PCI_CLASS_BRIDGE_PC1:pci_bridge_checkRanges bus);default:pbus_size_io bus）;mask $=$ IORESOURCE_MEM;prefmask $=$ IORESOURCE_MEM丨IORESOURCE_PREFETCH；if（pbus_size_mem（bus，prefmask，prefmask））mask $=$ prefmask;/\*Success,size non-prefetch only.\*/pbus_size_mem（bus，mask，IORESOURCE_MEM）；break;1

这段代码首先递归调用pci\_BUS\_size\_bridge函数，直到找到当前PCI总线树最底层的PCI桥，然后调用pci\_bridge\_checkRanges函数检查这个PCI桥所管理的地址空间是否支持I/O或者可预读的存储器空间，如果支持，则将当前PCI桥的pci\_BUS $\rightarrow$ self $\rightarrow$ resource参数的相应状态位置1。这段代码随后调用pbus\_size\_io和pbus\_size\_mem函数修复并对界当前PCI桥的I/O空间和存储器空间，并从低到高逐层递归调用pci\_BUS\_size\_bridge函数。

# 2.pci\_BUS\_assign-resources函数

pci\_BUS AssignResources函数在 ./drivers/pci/setup - bus.c 文件中，如源代码14-39和源代码14-41所示。

源代码14-39pci BUS assign resources函数片段1   
```c
void __ref pci_BUS_assignments(const struct pci_BUS *bus)  
{  
    struct pci_BUS *b;  
    struct pci_dev *dev;  
    pbus_assignments_sorted.bus); 
```

该函数首先调用 pbus\_assign-resources Sorted 函数遍历并初始化当前 PCI 总线上的所有 PCI 设备的 BAR 寄存器，包括 PCI Agent 设备和 PCI 桥，之后递归调用自身遍历当前 PCI 总线的所有下游总线，最后调用pci\_setup\_bridge 函数初始化 PCI 桥的存储器和 I/O Base、Limit 寄存器。

在第14.3.2中曾经使用pci\_scan\_bridge函数，将PCI桥的PrimaryBusNumber、SecondaryBusNumber和SubordinateBusNumber寄存器初始化完毕，此时LinuxPCI可以访问HOST主桥之下的所有PCI设备的配置空间，但是不能访问“未初始化的PCI设备”的BAR空间。PCI/PCIe总线规定使用ID寻址方式访问配置空间，而使用地址寻址方式访问存储器空间，因此处理器虽然不能访问BAR空间，但是依然能够访问PCI设备的配置空间。

值得注意的是这段代码中的一个细节问题。其中 pbus\_assignments sortied 函数在pci BUS\_assignments 函数递归调用之前执行，而pci\_setup\_bridge 函数在递归调用之后执行。Linux PCI 采用这种方式，可以保证 PCI 设备 BAR 寄存器初始化是从上游 PCI 总线到下游 PCI

总线，而PCI桥Base、Limit寄存器的初始化是从下游PCI总线到上游PCI总线。

这一细节对PCI设备的初始化非常重要，因为PCI桥所管理的地址空间是其下所有PCI设备使用地址空间的合集，因此PCI桥Base、Limit寄存器的初始化需要从下而上进行。而PCI设备的BAR寄存器的初始化的方向并没有严格规定，PCI规范并没有对此做具体的要求，在实现中只要保证系统软件在初始化PCI桥的Base、Limit寄存器之前，其下所有PCI设备的BAR寄存器已经完成初始化即可。目前Linux系统使用从上游总线到下游总线的方法初始化PCI设备的BAR寄存器。

对于x86处理器系统，PCI设备的BAR空间已经被BIOS初始化，因此只要BIOS正确分配了PCI设备的BAR寄存器，Linux系统不执行pci\_assign\_unassigned-resources函数也没有什么关系。不过对于一些处理器系统，其Firmware并没有完全枚举PCI总线树上的PCI设备，此时必须调用pci\_assign\_unassigned-resources中的pci\_BUS Assign-resources[0]函数初始化“未初始化BAR空间”的PCI设备。

pbs\_assign-resources\_sorted函数负责分配“未初始化PCI设备的BAR寄存器”，该函数将对这些PCI设备的BAR寄存器进行写操作。该函数的实现如源代码14-40所示。

源代码14-40 pbus\_assign-resources\_sorted函数  
static void pbus_assignResources Sorted(const struct pcibus \*bus)   
{ structpci_dev\*dev; struct resource \*res; struct resource_list head，\*list，\*tmp; int idx; head.next $=$ NULL; list_for_each_entry( dev, &bus->devices, bus_list）{ u16 class $=$ dev->class>>8; pdev_sort-resources( dev,&head); } for（list $=$ head.next；list;）{ res $=$ list->res; idx $=$ res-&list->dev->resource[0]; if（pci_assignResource(list->dev,idx)) { res->start $= 0$ · res->end $= 0$ · res->flags $= 0$ · } tmp $=$ list; list $=$ list->next; kfree(tmp);

这段代码首先调用pdev\_sort-resources函数，该函数的实现过程较为简单，其主要作用是将“未初始化”的PCI设备使用的资源进行排序对齐，然后加入到head链表中，随后调用pci\_assignResource函数初始化这些PCI设备的BAR寄存器。

pci\_assign\_resource 函数在 ./drivers/pci/setup-res.c 文件中，该函数的实现逻辑较为简单，本节并不列出这段源代码。该函数两次调用了pci\_BUS\_alloc\_resource 函数，第一次试图从上游总线的可预读存储器空间为当前PCI设备分配资源，第二次从“不可预读的存储器空间”分配资源。当资源分配成功后，pci\_assign\_resource $\rightarrow$ pcupdateResource 函数将初始化PCI设备的BAR寄存器，这些代码并不复杂，本节将这些代码留给读者。

源代码14-41pci BUS assign resources函数片段2  
```txt
list_for_each_entry(dev, &bus -> devices, bus_list) {  
b = dev -> subordinate;  
if (!b)  
continue;  
pci_BUS_assignResources(b);  
switch (dev -> class >> 8) {  
case PCI_CLASS_BRIDGEPCI:  
pci_setup_bridge(b);  
break;  
case PCI_CLASS_BRIDGE.cardBUS:  
pci_setup_cardbus(b);  
break;  
default:  
dev_info(&dev -> dev, "not setting up bridge for bus ")  
"%04x:%02x\n", pc_idomain Nr(b), b->number);  
break;  
} 
```

再次回到pci\_BUS\_assign-resources函数，该函数开始递归调用自身，寻找当前PCI总线子树的最后一个PCI桥，之后调用pci\_setup\_bridge函数初始化这个PCI桥的Base、Limit寄存器。pci\_setup\_bridge函数的源代码在./drivers/pci/set-bus.c文件中，本节对此不作介绍。

当Linux PCI执行pci\_setup\_bridge函数初始化当前PCI桥之后，这个桥的上游设备和这个桥管理的PCI设备的BAR寄存器已经初始化完毕。因此pci\_setup\_bridge函数通过简单的计算，即可得出当前PCI桥Base、Limit寄存器的值，之后调用pci\_write\_config\_dword函数将这个数据对PCI桥的这些寄存器更新即可。

至此，PCI总线树上的所有PCI设备的BAR寄存器，以及PCI桥的Base、Limit寄存器全部初始化完毕，从硬件的角度来看，PCI总线系统已经初始化完毕。

# 3.pci\_enable\_bridge函数

我们再次回到pci\_assign\_unassigned-resources函数，如源代码14-37所示，该函数将调用pci\_enable bridges函数，使能所有PCI桥设备。pci\_enable bridges函数的实现如源代码14-42所示。

源代码14-42pci\_enable\_bridge函数  
```c
voidpci_enable_bridge(structpci_BUS \*bus)   
{ structpci_dev\*dev; intretval; list_for_each_entry(dev,&bus->devices，bus_list）{ if（dev->subordinate）{ if(!pci_isenabled(dev)) { retval=pci_enable_device(dev); pcis_set/master(dev); } pcie_enable_bridge(dev->subordinate）; 1 
```

该函数的实现较为简单，分别调用pci\_enable\_device、pci\_set\_master函数启动当前PCI桥，之后递归调用pci\_enable\_bridge函数启动当前PCI桥下游的PCI桥。至此Linux PCI完成对当前PCI总线树的主要初始化工作。

# 14.4 LinuxPowerPC如何初始化PCI总线树

Linux PowerPC 初始化 PCI 总线树的步骤与 Linux x86 类似，也调用了一些 Linux 系统中与 PCI 总线相关的通用函数。但是 PowerPC 处理器使用的 HOST 主桥与 x86 处理器并不相同，因此 Linux PowerPC 初始化 PCI 总线树的过程与 Linux x86 有些差别。本节以 MPC8572 处理器为例，说明 Linux PowerPC 初始化 PCI 总线树的过程。

MPC8572处理器共有三个PCIe总线控制器，其中每一个总线控制器都可以管理一个独立的PCI总线树。在每一个总线控制器中都包含一组独立的寄存器，MPC8572处理器可以通过设置Inbound和Outbound寄存器，访问对应PCI总线树上所有PCI设备的配置空间。这组寄存器与MPC8548处理器提供的对应寄存器较为类似，详见第2.2节。

Linux PowerPC 在引入了 Open Firmware 机制后，使用 dts 文件管理 PCI 总线控制器。MPC8572 处理器系统使用的 dts 文件为 ./arch/powerpc/boot/dts/mpc8572ds.dts 文件，其中与 PCI 总线控制器相关的部分如源代码 14-43 所示。

源代码14-43 MPC8572处理器系统使用的dt文件  
```perl
pci0: pcie@ ffe08000 {
compatible = "fsl, mpc8548 - pcie";
device_type = "pci";
#interrupt - cells = <1 >;
#size - cells = <2 >;
#address - cells = <3 >;
reg = <0 0xffe08000 0 0x1000 >;
bus - range = <0 255 >;
ranges = <0x2000000 0x0 0x8000000 0 0x8000000 0x0 0x2000000
0x1000000 0x0 0x0000000 0 0xffc:00000 0x0 0x001000 >;
}
pci1: pcie@ ffe09000 {
compatible = "fsl, mpc8548 - pcie";
device_type = "pci";
#interrupt - cells = <1 >;
#size - cells = <2 >;
#address - cells = <3 >;
reg = <0 0xffe09000 0 0x1000 >;
bus - range = <0 255 >;
ranges = <0x2000000 0x0 0xa0000000 0 0xa0000000 0x0 0x2000000
0x1000000 0x0 0x0000000 0 0xffc:10000 0x0 0x001000 >;
}
pci2: pcie@ ffeo a 1
compatible = "fsl, mpc8548 - pcie";
device_type = "pci";
#interrupt - cells = <1 >;
#size - cells = <2 >;
#address - cells = <3 >;
reg = <0 0xffe o a 1 1 1 1
bus - range = <0 255 >;
ranges = <o x2o o o o o o o o o o o o o o o o o o o o o
o x1o o o o o o o o o o o o o o o o o o o o 
```

以上代码分别描述了MPC8572处理器系统的3个PCIe控制器，其使用的寄存器空间为 $0\mathrm{xFFE08000}\sim 0\mathrm{xFFE08FFFF}$ 、 $0\mathrm{xFFE09000}\sim 0\mathrm{xFFE09FFFF}$ 和 $0\mathrm{xFFE0A000}\sim 0\mathrm{xFFE0AFFF}$ 。这三个PCIe控制器分别管理3棵PCI总线树，其PCI总线的编号都为 $0\sim 255$ ，在这个dt文件中还包含了PCIe控制器的其他信息，本节并不关心这些内容。

Linux PowerPC 在调用 setup\_arch $\rightarrow$ mpc85xx\_cds\_setup\_arch 函数时，分别初始化这三个 PCIe 控制器，该函数的实现如源代码 14-44 所示。

源代码14-44 mpc85xx\_cds\_setup\_arch函数   
static void __init mpc85xx_cds_setup_arch(void)   
{   
#ifndef CONFIG_CPU struct device_node \* np;   
#endif   
...   
#ifndef CONFIG_CPU for_each_node_by_type(np, "pci") { if(of_device_isCompatible(np, "fsl,mpc8540 -pci") || of_device_isCompatible(np, "fsl,mpc8548 - pcie")) { struct resource rsrc; of_address_to_resource(np, 0, &rspc); if((rsrc.start&0xFFFF) ==0x8000) fsl_add_bridge(np,1); else fsl_add_bridge(np,0); } } ppc_md.pci_irq_fixup $=$ mpc85xx_cds_pci_irq_fixup; ppc_md.pci Exclude_device $=$ mpc85xx Exclude_device;   
#endif

mpc85xx\_cds\_setup\_arch函数分析mpc8572ds.dts文件，并将“pci0”作为主PCI总线控制器，并调用fsl\_add\_bridge(np,1)函数进行初始化操作，“pci1”和“pci2”作为从PCI总线控制器调用fsl\_add\_bridge(np,0)函数进行初始化操作。在MPC8572处理器中，主PCI总线控制器需要处理ISA总线使用的存储器和I/O地址空间。

fsl\_add\_bridge 函数在 ./arch/powerpc/sysdev/fsl\_pci.c 文件中, 该函数的实现如源代码 14-45 \~ 26 所示。Linux PowerPC 使用 pci\_controller 结构描述 HOST 主桥, 包括这个主桥管理的 PCI 总线域地址范围、PCI 总线号和访问配置寄存器的方法等一系列信息, pci\_controller 结构在 ./arch/powerpc/include/asm/pci-bridge.h 文件中。

源代码14-45 fsl\_add\_bridge函数片段1  
int_init fsl_add_bridge(struct device_node \*dev，int is_primary)   
{ intlen; structpci_controller\*hose; structresourcersrc; constint $^*$ bus_range;   
1 /\*Fetch host bridge registers address \*/ if(of_address_toResource(dev,0,&rsrc）}

printk(KERN_WARNINGS "Can't getpciregisterbase!"); return-ENOMEM;   
}   
... ppc_pci_add_flags(PPC_PCI_REASSIGN_ALL_BUS)； hose $\equiv$ pcibios_alloc_controller( dev）; if（！hose) return-ENOMEM; hose->first_BUSno $=$ bus_range?bus_range[0]：0x0; hose->last_BUSno $=$ bus_range?bus_range[1]：0xff; setup_indirect_pci(hose，rsrc.start，rsrc.start $+0\mathrm{x}4$ ， PPC_INDIRECT_TYPEeousBIG_ENDIAN);

这段代码首先分析mpc8572ds.dts文件，然后获得PCIe主桥管理的PCI总线范围，对于pci0控制器，bus\_range[0]为0，而bus\_range[1]为255。之后为当前PCIe主桥使用的hose结构分配空间，并初始化其first\_BUSno和last\_BUSno参数。

setup\_indirect\_pci 函数设置在 Linux PowerPC 中间接访问 PCI 设备配置空间的函数，如第 2.2 节所示，在 PowerPC 处理器中，访问 PCI 设备配置空间有两种方式，一种是使用间接访问方式，一种是使用 ECAM 方式。与 x86 处理器略有不同，PowerPC 处理器使用间接访问方式也可以访问 PCIe 设备的扩展配置空间，因此 ECAM 方式对于 PowerPC 处理器而言，并不是必须的。

# 源代码14-46 fsl\_add\_bridge函数片段2

```c
/* Interpret the "ranges" property */
/* This also maps the I/O region and sets isa_io/mem_base */
pci_process_bridge_OFRanges(hose, dev, is_primary);
/* Setup PEX window registers */
setup_pci_atmu(hose, &rsrc);
return 0; 
```

pci\_process\_bridge\_OFRanges 函数分析 mpc8572ds.dts 文件的 “ranges” 字段, 在 dts 文件中, ranges 字段的解释如下。

ranges $= < 0\mathrm{x}2000000$ 0x0 0x8000000 0 0x8000000 0x0 0x2000000 0x1000000 0x0 0x0000000 0 0xffc00000 0x0 0x001000 >;

ranges字段共由14个双字组成，每7个双字为1组，每一组描述一段PCI总线域地址空间与存储器域地址空间的对应关系。

\- 每一组的第一个双字代表pci\_space，为0x200-0000表示这段PCI总线地址空间为存储器地址空间，为0x100-0000表示这段PCI总线地址空间为I/O地址空间。

- 每一组的第 $2 \sim 3$ 个双字存放pci\_address，即PCI域地址空间。  
- 每一组的第 $4 \sim 5$ 个双字存放 cpu\_address，即存储器域地址空间。  
- 每一组的第 $6 \sim 7$ 个双字存放 size，即这段地址空间的大小。

pci\_process\_bridge\_OFRanges函数的主要作用就是根据dt文件中的ranges字段，初始化hose结构的对应参数，本节对该函数不做进一步介绍。

setup\_pci\_atmu函数首先设置MPC8572处理器中的Outbound和Inbound寄存器组，这两组寄存器的描述见第2.2节。然后设置PEXCSRBAR寄存器。

如果MPC8572处理器作为 $\mathrm{RC}^{\ominus}$ ，而且支持MSI中断机制时，需要设置PCIe主桥的BAR0寄存器，即PEXCSRBAR（PCIExpressBaseAddressRegister)寄存器。在PCI规范中，MSI中断机制以存储器写的方式实现，当这个MSI存储器写最终到达RC时，需要能够被RC接收。在PowerPC处理器中，MSI存储器写的目的地址为MSIIR寄存器在PCI总线域的物理地址。此时PowerPC处理器可以采用两种方式接收这个MSI存储器写，一种是设置Inbound寄存器，映射MSIIR寄存器所在的PCI总线空间，另一种是设置RC的BAR0寄存器。LinuxPowerPC使用了后一种方式。

Linux PowerPC 执行完毕 setup\_arch 函数后，还会执行一些和 PCI 总线初始化相关的函数，如下所示。

```txt
c053e04c t __initcall_pcibus_class_init2  
c053e050 t __initcall_pci_driver_init2  
c053e088 t __initcall_pcibios_init4  
c053e0ac t __initcall_pci_slot_init4  
c053e28c t __initcall_pci_init6  
c053e290 t __initcall_pciproc_init6  
c053e3ec t __initcall_pci_resource Alignment_sysfs_init7  
c053e3f0 t __initcall_pci_sysfs_init7 
```

这些函数在第14.3节中都有介绍，虽然Linux PowerPC执行这些函数的过程与Linux x86略有不同，但大体类似，本章对此不做进一步说明。

# 14.5 小结

本章使用了一定的篇幅介绍Linux PCI的实现过程。Linux PCI中的源代码对于读者理解PCI体系结构有较大的帮助，但希望读者不要拘泥于此。Linux PCI只是PCI软件体系结构的一种实现方式，这种实现并不是最合理的。

Linux PCI子系统在其发展过程中，遇到了各种各样的问题与Bug，这些代码经历了一遍又一遍的修改。这种修改有如向一个满是补丁的衣服上继续打补丁，最后已无法识别衣服的原本模样。同许多通用代码类似，Linux PCI需要兼容各类处理器系统，目前的实现远非完美，而这些不完美将继续。

