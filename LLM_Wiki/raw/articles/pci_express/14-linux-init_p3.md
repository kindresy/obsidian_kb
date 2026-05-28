---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "14"
section: "14.3 基于ACPI机制的LinuxPCI的初始化"
part: 3
tags: [pci, pci-express, computer-architecture]
---
# 14.3 基于ACPI机制的LinuxPCI的初始化

本节重点介绍Linux系统如何使用ACPI机制，对PCI总线树进行枚举。Linux的ACPI系统的初始化较为复杂。本节重点介绍与PCI总线相关的一些基本模块，并不会介绍与ACPI系统初始化相关的全部内容。

在Linux系统中，ACPI系统的初始化由两部分组成，一部分由start\_kernel $\rightarrow$ setup\_arch函数执行，另一部分作为模块由do\_initcalls函数执行。

# 14.3.1 基本的准备工作

setup\_arch函数将分别调用acpi.boot\_table\_init、early\_acpi.boot\_init和acpi.boot\_init函数完成ACPI系统的初始化，这几个函数的源代码在 ./arch/x86/kernel/acpi/boot.c文件中。

acpi.boot\_table\_init 函数调用 acpi\_table\_init 函数在内存中找到 RSDP 和 RSDT/XSDT，从而定位 ACPI 表。BIOS 在系统初始化时将 ACPI 表放到一块固定物理地址区域中；early\_acpi.boot\_init 函数调用 early\_acpi\_process\_madt 函数进一步处理 MADT；而 acpi.boot\_init 函数依次分析 SBFT（Simple Boot Flag Table）、FADT 和 HPET（IA-PC High Precision Event Timer Table），其中 HPET 是 Intel 定义的一个高精度定时器。

setup\_arch函数执行完毕后，Linux系统将调用do\_initcalls函数执行与ACPI系统相关的一些模块，其中与PCI总线有关的模块有acpi\_pci\_init、acpi\_pci\_root\_init和acpi\_pci\_link\_init函数。这些函数的说明如下。

# 1.acpi\_pci\_init函数

acpi\_pci\_init函数的执行过程较为简单，该函数在 ./drivers/pci/pci-acpi.c 文件中，如源代码14-19所示。

源代码14-19 acpi\_pci\_init函数  
```c
static int _init acpi_pci_init(void)   
{ int ret; if (acpi_gbl_FADT.boot_flags&ACPI_FADT_NO_MSBI){ printf(KERN_INFO"ACPI FADT declares the system doesn't support MSI, so disable it\n"); pci_no_msi(); } if(acpi_gbl_FADT.boot_flags&ACPI_FADT_NO_ASPM）{ printf(KERN_INFO"ACPI FADT declares the system doesn't support PCIe 
```

ASPM，so disable it\n"）;pcie_no_aspm();}ret $\equiv$ register_acpi_BUS_type(&acpi_pci_BUS)；if(ret)return 0;pci_setplatform_pm(&acpi_pciplatform_pm);return 0;  
}arch_initcall(acpi_pci_init);

该函数首先分析“Boot Architecture Flags”字段，确定当前处理器系统是否需要使能MSI中断机制和PCIe设备的ASPM(Active State Power Management)机制，ASPM机制的详细描述见第8.3节，而MSI机制的详细说明见第10章。该函数调用register\_acpi\_BUS\_type函数，将acpi\_pci\_BUS结构加入到全局链表bus\_type\_list，最后调用pci\_setplatform\_pm函数将全局变量pci-platform\_pm赋值为acpi\_pciplatform\_pm。

# 2.acpi\_pci\_root\_init函数

acpi\_pci\_root\_init 函数调用 acpi\_pci\_root\_add 和 acpi\_pci\_root\_start 函数遍历处理器系统中的 PCI 总线树。在 Linux 系统中，acpi\_pci\_root\_init 函数的调用关系较为复杂，本节仅介绍其调用过程，并不详细介绍其实现机制。

acpi\_pci\_root\_init函数的调用过程如源代码14-20所示。

源代码14-20 acpi\_pci\_root\_init函数的调用过程

```ocaml
acpi_pci_root_init -> acpi_BUS_registerDriver -> driver_register -> bus_add_driver -> driver Attached -> _driver Attached -> driverprobe_device -> reallyprobe -> (dev -> bus -> probe) 
```

由以上过程可见acpi\_pci\_root\_init函数将调用reallyprobe函数中的（dev->bus->probe)函数，而dev->bus->probe函数在acpi\_device register函数中被赋值为acpi\_device probe函数。

acpi\_deviceprobe函数又经过了一系列复杂的调用，最终调用acpi\_pci\_root\_add和acpi\_pci\_root\_start函数，其调用过程如源代码14-21所示。

源代码14-21 acpi\_pci\_root\_init函数的调用过程

acpi_deviceProbe   
|---- $\rightarrow$ acpi_BUS_driver_init   
1 $1 - - - \rightarrow$ driver->ops.add   
1--- $\Rightarrow$ acpi_start_single_object   
1---- $\rightarrow$ driver->ops.start

其中 driver -> ops.add 函数与 acpi\_pci\_root\_add 函数对应；而 driver -> ops.start 函数与 acpi\_pci\_root\_start 函数对应。acpi\_pci\_root\_add 函数在 ./drivers/acpi/pci\_root.c 文件中，该函数的主要功能是遍历 PCI 总线树，如源代码 14-22 \~ 23 和源代码 14-31 所示。

static int _devinit acpi_pci_root_add(struct acpi_device \*device)   
{ unsigned long long segment, bus; acpi_status status; int result; struct acpi_pci_root \* root; acpi_handle handle; struct acpi_device \* child; u32 flags, base_flags; segment $= 0$ . status $=$ acpi Evaluate_integer( device->handle, METHOD_NAME_NAME, NULL, &segment); if (ACPI_FAILURE(status)&&status！ $=$ AE_NOTFOUND){ printf(KERN_ERR Prefix "can't evaluate _SEG\n"); return -ENODEV; } /* Check_CRS first, then_BBN. If no_BBN, default to zero. */ bus $= 0$ . status $=$ try_get_root_bridge_BUSNr(device->handle,&bus); if (ACPI_FAILURE(status)){ status $=$ acpi Evaluate_integer( device->handle, METHOD_NAME_BBN, NULL,&bus); if (ACPI_FAILURE(status)&&status! $=$ AE_NOT FOUND){ printf(KERN_ERR Prefix "no bus number in _CRS and can't evaluate _BBN\n"); return -ENODEV; } } root $=$ kzalloc(sizeof(struct acpi_pci_root),GFP_KERNEL); if(!root) return -ENOMEM; INIT_LIST_HEAD(&root->node); root->device $=$ device; root->segment $=$ segment&0xFFFF; root->bus_nr $=$ bus&0xFF; strcpy(acpi_device_name(device),ACPI_PCI_ROOT_DEVICE_NAME); strcpy(acpi_device_class(device),ACPI_PCI_ROOT_CLASS); device->driver_data $=$ root; / * All supported architectures that use ACPI have support for

\*PCI domains, so we indicate this in _OSC support capabilities. \*/ flags $=$ base_flags $=$ OSC_PCI_SEGMENTATIONGROUPS_support; acpi_pci_osc_support(root，flags）; /\* TBD: Need PCI interface for enumeration/configuration of roots. \*/ /\* TBD:Locking \*/ list_addTAIL(&root->node,&acpi_pci_roots）; printf(KERN_INFOPREFIX"%s[%s](%04x:%02x)\n"， acpi_device_name(device)，acpi_device_bid(device）， root->segment，root->bus_nr);

这段代码通过ACPI表中的\_SEG和\_BBN参数获得HOST主桥使用的Segment和Bus号，创建一个acpi\_pci\_root结构，并对该结构进行初始化，随后将acpi\_pci\_root结构加入到acpi\_pci\_rootss队列中。acpi\_pci\_root结构的主要功能是对当前HOST主桥控制器进行描述，而在acpi\_pci\_rootss队列中包含当前x86处理器系统所有HOST主桥的信息。

当x86处理器系统中只有一个HOST主桥时，acpi\_pci\_root\_add函数仅会被Linux调用一次，此时acpi\_pci\_roots队列中只有一个数据成员，即root，其Segment和Bus号均为0；如果存在多个HOST主桥时，acpi\_pci\_root\_add函数将在PCI总线初始化时被调用多次，并将所有主桥信息加入到acpi\_pci\_roots队列中。

这段代码还将HOST主桥的\_OSC参数的PCI Segment Groups supported位设置为1，该参数在ACPI规范中定义，该位为1时表示当前处理器系统支持PCI Segment Group。

源代码14-23 acpi\_pci\_root\_add函数片段2  
/\*   
\* Scan the Root Bridge   
\*   
\* Must do this prior to any attempt to bind the root device, as the \* PCI namespace does not get created until this call is made (and \* thus the root bridge'spci_dev does not exist).   
\*/   
root->bus $=$ pcicacpi_scan_rootdevice,segment,bus);   
if(!root->bus){ printf(KERN_ERRPREFIX "Bus $\% 04\mathrm{x}$ ： $\% 02\mathrm{x}$ not present in PCI namespace\n", root->segment,root->bus_nr); result $=$ -ENODEV; goto end;

在一个x86处理器系统中，如果没有使能ACPI机制，则Linux系统调用pcilegacy\_init $\rightarrow$ pcbios\_scan\_root函数枚举PCI设备。如果Linux系统使能了ACPI机制，则由这段程序调用pci\_acpi\_scan\_root函数完成PCI设备的枚举。pci\_acpi\_scan\_root和pcbios\_scan\_root函数对PCI总线树的枚举过程类似。

pci ACPI\_scan\_root 函数在 ./arch/x86/pci/acpi.c 文件中，如源代码 14-24 所示。

# 源代码14-24pci\_acpi\_scan\_root函数

```c
struct pcibus * _devinit
pci_acpi_scan_root(struct acpi_device * device, int domain, int busnum)
{
    struct pcibus * bus;
    struct pcisysdata * sd;
    int node;
    /* Allocate per-root-bus (not per bus) arch-specific data. */
        *Todo: leak; this memory is never freed.
        * It's arguable whether it's worth the trouble to care.
        */
    sd = kzalloc(sizeof(*sd), GFP_KERNEL);
    if (!sd) {
        printf(KERN_ERR "PCI: OOM, not probing PCI bus %02x\n", busnum);
        return NULL;
    }
    sd->domain = domain;
    sd->node = node;
    /* Maybe the desired pcibus has been already scanned. In such case */
        * it is unnecessary to scan the pcibus with the given domain, busnum.
        */
    bus = pcifind BUS(domain, busnum);
    if (bus) {
        /* If the desired bus exits, the content of bus ->sysdata will */
            * be replaced by sd.
            */
            memcpy.bus ->sysdata, sd, sizeof(*sd));
            kfree(sd);
    } else {
        bus = pcifreate BUS(NULL, busnum, &pci_rootOps, sd);
        if (bus) {
            if (pciprobe & PCIUSE_CRS)
                get_current-resources(device, busnum, domain, bus);
        }
    }
}; 
```

bus $\rightarrow$ subordinate $=$ pci_scan_child_BUS bus); } 1 return bus;

这段代码首先判断当前总线号是否已经存在，如果存在说明这条总线已经被遍历过，该函数将直接退出。否则将首先调用pci\_create BUS函数，pci\_create BUS函数的源代码在./drivers/pci/probe.c文件中，其主要作用是为当前HOST主桥创建pci\_BUS结构，并初始化这个pci\_BUS结构的部分参数如resource[0/1]，secondary参数等，然后将这个pci\_BUS结构加入到全局链表pci\_root\_buses中，最后进行一些与sysfs相关的初始化工作。

之后调用pci\_scan\_child\_BUS函数对当前PCI总线上的设备进行枚举，pci\_scan\_child\_BUS函数将完成对PCI总线树的枚举操作，该函数是Linux遍历PCI总线树的要点，下一节将专门介绍讨论该函数的实现机制。

# 14.3.2 LinuxPCI初始化PCI总线号

PCI总线树的枚举由pci\_scan\_child\_BUS函数完成，该函数的主要作用是分配PCI总线树的PCI总线号，而并不初始化PCI设备使用的BAR空间。

pci\_scan\_child\_BUS 函数在第一次执行时，首先遍历当前 HOST 主桥之下所有的 PCI 设备，如果在 HOST 主桥下含有 PCI 桥，将再次遍历这个 PCI 桥下的 PCI 设备。并以此递归，直到将当前 PCI 总线树遍历完毕，并返回当前 HOST 主桥的 subordinate 总线号。subordinate 总线号记载当前 PCI 总线树中最后一个 PCI 总线号，因此只有完成了对 PCI 总线树的枚举后，才能获得该参数。pci\_scan\_child\_BUS 函数如源代码 14-25 和源代码 14-29 所示。

# 源代码14-25pci\_scan\_child\_BUS函数片段1

```c
unsigned int _devinit pcie_scan_child_BUS(struct pcie_BUS *bus)  
{  
    unsigned int devfn, pass, max = bus -> secondary;  
    struct pcie_dev *dev;  
    prDebug("PCI: Scanning bus %04x:%02x\n", pcie_domain Nr(bus), bus -> number);  
    /* Go find them, Rover! */  
    for (devfn = 0; devfn < 0x100; devfn + = 8)  
        pcie_scan_slot(bus, devfn);  
} 
```

该函数首先调用pci\_scan\_slot函数，扫描当前PCI总线的所有设备，并将其加入到对应总线的设备队列中。在pci\_scan.bus\_parented函数调用pci\_scan\_child.bus函数时，其输入参数为HOST主桥的pci\_BUS结构，此时pci\_scan\_slot函数首先初始化与HOST主桥直接相连的PCI设备，即Bus号为0的PCI设备。

# 1.pci\_scan\_slot函数

一条PCI总线上最多有32个设备，每个设备最多有8个Function。pci\_scan\_child\_BUS函数需要枚举每一个可能存在的Function。因此对于一条PCI总线，pci\_scan\_child\_BUS函数需要调用0x100次pci\_scan\_slot函数。而pci\_scan\_slot函数调用pci\_scan\_single\_device函数配置对当前PCI总线上的所有PCI设备。

pci\_scan\_single\_device 函数进一步调用了pci\_scan\_device和pci\_device\_add函数。其中pci\_scan\_device函数主要对PCI设备的配置寄存器进行读写操作，侧重于PCI设备进行硬件层面的初始化操作，而pci\_device\_add函数侧重于软件层面的初始化。pci\_scan\_device函数如源代码14-26所示。

源代码14-26pci\_scan\_device函数  
static struct pcie_dev \*pci_scan_device(struct pcibus \*bus,int devfn)   
{ struct pcie_dev \*dev; u321; int delay $= 1$ ： if(pci_BUS_read_config_dword（bus，devfn，PCI_VENDOR_ID，&l)) return NULL; / \*some broken boards return O or \~0if a slot is empty: \*/ if(1==0xFFFFFF||1==0x00000000|| 1==0x0000ffff||1==0xfffff0000） return NULL; /*ConfigurationrequestRetryStatus\*/ while(1 $= = 0$ xffff0001）{ msleep(delay); delay $\ast = 2$ ： if(pci_BUS_read_config_dword（bus，devfn，PCI_VENDOR_ID，&l)) return NULL; /\*Card hasn't responded in 60 seconds?Must be stuck.\*/ if(delay>60\*1000）{ printf(KERN WARNING "pci %04x:%02x:%02x.%d:not" "responding\n",pci_domain_nr（bus）， bus->number,PCI_SLOT（devfn）， PCI FUNC（devfn））; return NULL; }

dev $=$ alloc_pci_dev(); if(!dev) return NULL; dev->bus $=$ bus; dev->devfn $\equiv$ devfn; dev->vendor $= 1$ &0xffff; dev->device $= (1\gg 16)$ &0xffff; if(pci_setup_device(dev)){ kfree(dev); return NULL; } return dev;

pci\_scan\_device 函数首先读取 PCI 设备的 Vendor ID 和 Header Type 寄存器，并根据这两个寄存器的内容对 PCI 设备进行完整性检查，之后创建pci\_dev结构，并对该结构进行基本的初始化。

set\_pcie\_port\_type函数的主要作用是处理PCI Express Extended Capabilities结构，并将其保存在pci\_dev $\rightarrow$ pcie\_type参数中，该结构的详细描述见第4.3.2节。值得注意的是，在Linux系统中，许多PCIe设备并没有提供该结构。在这段源代码的最后将调用pci\_setup\_device函数，其实现如源代码14-27所示。

源代码14-27pci\_setup\_device函数  
static intpci_setup_device(structpci_dev\*dev)   
{ u32class;   
... switch（dev->hdr_type）{//\*header type\*/ casePCIHEADER_TYPENORMAL:/\*standard header\*/ if（class $\equiv =$ PCI_CLASS_BRIDGE_PCIGoto bad; pciread_irq（dev）； pciread bases（dev，6，PCI ROM ADDRESS）； pciread_config_word（dev, PCI_SUBSYSTEM_VENDOR_ID，&dev->subsystemvendor); pciread_config_word（dev，PCI_SUBSYSTEM_ID，&dev->subsystem_device）;   
1 break; casePCIHEADER_TYPE_BRIDGE:/\*bridge header\*/

if (class != PCI_CLASS_BRIDGEPCI) goto bad; / \* The PCI-to-PCI bridge spec requires that subtractive decoding (i.e. transparent) bridge must have programming interface code of 0x01. \*/ pcie_read_irq(dev); dev->transparent $=$ ((dev->class&0xff) $= = 1$ ); pcie_readbases(dev,2，PCI ROM ADDRESS1）； break;   
case PCIHEADER_TYPE_CARDBUS: /\* CardBus bridge header \*/ if(class！=PCI_CLASS_BRIDGE_CARDBUS) goto bad; pcie_read_irq(dev); pcie_readbases(dev,1,0); pcie_read_config_word(dev, PCI_CB_SUBSYSTEM_VENDOR_ID,&dev->subsystemvendor); pcie_read_config_word(dev, PCI_CB_SUBSYSTEM_ID,&dev->subsystem_device); break;   
return 0;

pci\_setup\_device 函数首先根据 Header Type 寄存器，判断当前 PCI 设备是 PCI Agent 设备、PCI 桥还是 Card Bus。PCI Agent 设备使用的配置空间与 PCI 桥所使用的配置空间并不相同，因此 Linux PCI 需要区别处理这两种配置空间。本节忽略 Card Bus 的处理过程。

pci\_setup\_device 函数需要调用 pcie\_read\_irq 和 pcie\_read bases 函数访问 PCI 设备的配置空间，并进一步初始化 pcie\_dev 结构的其他参数。

pci\_read\_irq函数的主要作用是读取PCI设备配置空间的Interrupt Pin和Interrupt Line寄存器，并将结构赋值到pci\_dev $\rightarrow$ pin和irq参数中。其中pin参数记录当前PCI设备使用的中断引脚，而irq参数存放系统软件使用的irq号。

值得注意的是，在pci\_setup\_devic函数中初始化的pci\_dev $\rightarrow$ irq参数并不一定是PCI设备驱动程序在request\_irq函数中使用的irq入口参数。如果当前Linux x86系统使用了I/O APIC控制器时，Linux设备驱动程序调用pci\_enable\_device函数将会改变pci\_dev $\rightarrow$ irq参数，详见第15.1.1节。

而如果PCIe设备使能了MSI/MSI-X中断处理机制，pci\_dev $\rightarrow$ irq参数在设备驱动程序调用pci\_enable\_msi/pci\_enable\_msix函数后也将会发生变化，详见第15.2节。只有x86处理器使用8259A中断控制器处理PCI设备的中断请求时，pci\_dev $\rightarrow$ irq参数才与InterruptLine寄存器中的值一致。

pci\_readbases函数访问PCI设备的BAR空间和ROM空间，并初始化pci\_dev $\rightarrow$ resource

参数。在第12.3.2节Capric卡的初始化中使用的pciResource\_start和pciResource\_len函数就是从pci\_dev $\rightarrow$ resource参数中获得BAR空间使用的基地址与长度。

这里有一个细节需要提醒读者注意，在pci\_dev $\rightarrow$ resource参数中存放的BAR空间的基地址属于存储器域，而在PCI设备的BAR寄存器中存放的基地址属于PCI总线域。在x86处理器中，这两个值虽然相同，但是所代表的含义不同。

pci\_readbases函数调用\_pci\_read\_base函数对pci\_dev $\rightarrow$ resource参数进行初始化，\_pci\_read\_base函数的实现方式如源代码14-28所示。

源代码14-28 \_\_pci\_read\_base函数  
```c
int __pci_read_base(struct pcie_dev *dev, enum pcie_bar_type type, struct resource *res, unsigned int pos)  
{  
    u32 l, sz, mask;  
    mask = type ? ~ PCI-ROM_ADDRESS_ENABLE : ~0;  
    res -> name = pcie_name(dev);  
    pcie_read_config_dword(dev, pos, &l);  
    pcie_write_config_dword(dev, pos, mask);  
    pcie_read_config_dword(dev, pos, &sz);  
    pcie_write_config_dword(dev, pos, l);  
}  
if (type == pcie_bar_mem64) {  
} else {  
    sz = pcie_size(1, sz, mask);  
    if (!sz)  
        goto fail;  
    res -> start = 1;  
    res -> end = 1 + sz;  
}  
out:  
return (type == pcie_bar_mem64) ? 1 : 0;  
fail:  
res -> flags = 0;  
goto out; 
```

\_pci\_read\_base函数的实现较为简单，本节仅介绍该函数获取BAR空间长度的方法。PCI总线规范规定了获取BAR空间的标准实现方法。其步骤是首先向BAR寄存器写全1，之后再读取BAR寄存器的内容，即可获得BAR空间的大小。

我们以Capric卡为例说明该过程，由上文所示Capric卡的BAR0空间为不可预读的存储器空间，大小为 $0\mathrm{x}10000$ 字节。这个设备在被初始化之前，其BAR0寄存器的值由硬件预置，

其值为0xFFFF-0000，其中BAR0寄存器的第 $15\sim 0$ 位只读，其 $15\sim 4$ 字段为0表示所申请的空间大小为64KB；第3位为0表示不可预读；第 $2\sim 1$ 字段为0x00表示BAR0空间必须映射到PCI总线域的32位地址空间中；第0位为0表示为存储器空间。

当系统初始化完毕后，将 BAR0 寄存器重新进行赋值，其值为 PCI 总线域的地址，如 0x9030-0000。当软件对这个寄存器写入“\~0x0”之后，该寄存器的值将变为 0xFFFF-0000，因为最后 16 位只读。采用此方法可以获得 Capric 卡 BAR0 空间的大小。在 Linux 系统中，可以使用pci\_size 函数将 0xFFFF-0000 转换为 BAR0 空间使用的实际大小，即 64 KB。这段程序在获得 BAR 空间的基地址和长度后，继续判断当前 BAR 空间为 64 位 PCI 总线地址空间，还是 32 位 PCI 总线地址空间。为简化程序，本节仅列出处理“32 位 PCI 总线地址这种情况”的源代码。

如果是当前PCI设备使用32位地址空间，则这段程序将初始化pci\_dev $\rightarrow$ resource的start和end参数；如果是64位地址空间，该函数也需要初始化pci\_dev $\rightarrow$ resource的start和end参数，只是过程稍微复杂。这段代码留给读者分析。

细心的读者在分析\_pci\_read\_base函数后，会对“pci\_read\_config\_dword(dev, pos, &1)”语句产生疑问。因为从Linux PCI的初始化过程，我们并没有发现处理器何时将PCI设备的BAR寄存器初始化，此时读到变量1的究竟是什么数值？

在x86处理器系统中，虽然Linux PCI并没有对PCI设备的BAR空间进行初始化操作，但是BIOS已经完成了对PCI总线树的枚举过程，因此变量1将保存有效的BAR空间基地址。对于其他处理器体系，负责初始化引导的Firmware可能并没有实现PCI总线树的枚举，此时变量1将保存PCI设备的硬件复位值。

无论对于哪种处理器系统，执行\_pci\_read\_base函数总能获得正确BAR空间的大小。但是如果有些处理器系统的Firmware没有对PCI总线树进行枚举时，PCI设备的BAR空间中仅为上电复位值。在这些处理器系统中，\_pci\_read\_base函数执行完毕后，在pci\_dev $\rightarrow$ resource中保存的start和end参数仅是PCI设备从E2PROM中获得的初始值。

# 2.pci\_scan\_bridge函数

再次回到pci\_scan\_child\_BUS函数，分析剩余的程序，如源代码14-29所示。

源代码14-29pci\_scan\_child\_BUS函数片段2

$\begin{array}{rl} & \text{After performing arch - dependent fixup of the bus, look behind}\\ & \text{all PCI - to - PCI bridges on this bus.}\\ & \text{if(!bus->is-added)}\\ & \text{prDebug("PCI:Fixups for bus}\% 04\mathrm{x}:\%\ 02\mathrm{x}\backslash \mathrm{n"}\\ & \text{pci_domain\_nr(bus),bus->number);}\\ & \text{pcibios_fixup_BUS(bus);}\\ & \text{if(pci_is_root_BUS(bus))}\\ & \text{bus->is-added = 1;}\\ & \end{array}$

for (pass $= 0$ pass $<  2$ pass $+ +$ ） list_for_each_entry(dev,&bus->devices，bus_list）{ if（dev->hdr_type $\equiv =$ PCIHEADER_TYPE_BRIDGE|| dev->hdr_type $\equiv =$ PCIHEADER_TYPE_CARDBUS) max $\equiv$ pciscan_bridge（bus，dev，max，pass）;   
1   
/\* \*We've scanned the bus and so we know all about what's on \*the other side of any bridges that may be on this bus plus \*any devices. \* \*Return how far we've got finding sub - buses. \*/   
prDebug("PCI：Bus scan for $\% 04\mathrm{x}$ ： $\% 02\mathrm{x}$ returning with max $=$ $\% 02\mathrm{x}\backslash \mathrm{n}^{\prime \prime}$ ，pci_domain_nr（bus），bus->number，max）； return max;

pci\_scan\_child\_BUS 函数执行完毕 pcie\_scan\_slot 函数后，将首先调用 pcibios\_fixup\_BUS 函数。pcibios\_fixup\_BUS 函数的主要目的是为一些 PCI 设备中的 errata 提供 work-around，但是在该函数中还含有一个非常重要的函数，即 pcie\_read\_bridgebases 函数。

因为历史原因pci\_read\_bridgebases函数一直存在于pcibios\_fixup\_BUS函数中，但是这个函数更应该直接放入到pci\_scan\_child\_BUS函数中。pci\_read\_bridgebases函数将读取当前PCI桥的I/O Limit、I/O Base、Memory Limit、Memory Base、Prefetchable Memory Limit和Prefetchable Memory Base寄存器，并根据这些寄存器的值，初始化pci\_BUS $\rightarrow$ resource参数，该参数存放当前PCI桥所能管理的地址空间。

之后pci\_scan\_child\_BUS函数将调用pci\_scan\_bridge函数处理当前PCI总线上所挂接的PCI桥，并初始化在这个桥片SecondaryPCI总线上的PCI设备。值得注意的是pci\_scan\_bridge函数被调用了两次，一次pass参数等于0，另外一次pass参数等于1。

在一个处理器系统中，有些负责初始化引导的 Firmware 可能已经完成对 PCI 总线树的枚举操作，而有些 Firmware 没有做这样的操作。当 pass 参数等于 0 时，pci\_scan\_bridge 函数处理“已完成枚举”的 PCI 桥；当 pass 参数等于 1 时，pci\_scan\_bridge 函数处理“尚未完成枚举”的 PCI 桥。对于 x86 处理器系统而言，BIOS 将预先对 PCI 总线树进行枚举；而对于其他处理器系统，如 PowerPC 处理器系统，U-Boot 并没有进行这个枚举操作；当然还存在一种可能，就是 Firmware 完成了部分枚举。无论是哪种情况，通过两次调用pci\_scan\_bridge函数，都将完成对处理器系统中所有 PCI 桥的处理。

在Linux PCI中有许多函数都是通用函数，即各类处理器系统都需要使用的函数，这些

通用函数给Linux PCI的设计带来了不小的麻烦。为不同的处理器平台开发通用架构，是对任何资深系统程序员的巨大考验。在Linux PCI中有许多这样的程序。pci\_scan\_bridge函数是其中之一，该函数的主体实现如源代码14-30所示。

源代码14-30pci\_scan\_bridge函数  
```txt
int _devinit pci_scan_bridge(struct pci_BUS *bus, struct pci_dev *dev, int max, int pass)  
{  
    struct pci_BUS *child;  
    int is_cardbus = (dev->hdr_type == PCIHEADER_TYPE_CARDBUS);  
    u32 buses, i, j = 0;  
    u16 bctl;  
    int broken = 0;  
    pci_read_config_dword(dev, PCIPRIMARY_BUS, &buses);  
...  
if ((buses & 0xffff00) && ! pcibios_assign_all_busses() && ! is_cardbus && ! broken) {  
    if (pass) goto out; busnr = (buses >> 8) & 0xFF; goto out; busnr = (buses >> 8) & 0xFF;  
    child = pci_find/bus(pci_domain(nr(bus), busnr); if (!child) { child = pci_add_new/bus(bus, dev, busnr); if (!child) goto out; child -> primary = buses & 0xFF; child -> subordinate = (buses >> 16) & 0xFF; child -> bridgeCtl = bctl; } cmax = pci_scan_child_BUS(child); if (cmax > max) max = cmax; if (child -> subordinate > max) max = child -> subordinate; } else { if (!pass) } 
```

if(!pass) { if(pcibios_assign_all_busses() || broken)   
... pci_write_config_dword(dev,PCIPRIMARY_BUS, buses& \~0xfffff); goto out;   
1 /\*Clear errors\*/ pci_write_config_word(dev,PCI_STATUS,0xffff); /\*Prevent assigningabusnumberthatalreadyexists. \*This can happen when a bridge is hot-plugged\*/ if(pci_find/bus(pci_domain_nr.bus),max+1)) goto out; child $=$ pci_add_new/busbus,dev，++max）； buses $=$ (buses&0xff000000) 1((unsigned int)(child->primary)<<0) 1((unsigned int)(child->secondary)<<8) 1((unsigned int)(child->subordinate)<<16);   
... pci_write_config_dword(dev,PCIPRIMARY_BUS,buses);   
... child ->subordinate $=$ max; pci_write_config_byte(dev,PCI_SUBORDINATE_BUS,max);   
}   
out: pci_write_config_word(dev,PCI_BRIDGE_CONTROL,bctl); return max;

pci\_scan\_bridge 函数首先读取当前 PCI/HOST 主桥配置空间的第 21\~18 字节，这段数据的描述如第 2.3 节所示。在这段数据中，依次存放 PCI 桥配置寄存器的 Secondary Latency Timer、Subordinate Bus Number、Secondary Bus Number 和 Primary Bus Number 寄存器。

这段程序通过判断 PCI 桥的 Subordinate 和 Secondary 总线号是否为 0，判断当前 PCI 桥是否已经被初始化。如果 Subordinate 或者 Secondary 总线号不为 0，则表示该 PCI 桥已经被 Firmware 遍历；如果为 0，表示没有被 Firmware 遍历。

如果当前 PCI 桥已经被 Firmware 遍历，即((bus & 0xffff00)...)的计算结果为 True 时，这段程序将继续判断 pass 参数，如果为 1 则跳出；否则这段程序将直接调用pci\_add\_new\_BUS函数为这个 PCI 桥创建pci\_BUS结构，然后递归调用pci\_scan\_child\_BUS函数初始化该 PCI 桥管理的 PCI 子树。当pci\_scan\_child\_BUS函数递归执行完毕后，这段程序将重新修正pci\_BUS→subordinate 参数。

如果当前 PCI 桥没有被 Firmware 遍历，即((bus & 0xffff00)...)的计算结果为 False 时，这段程序将执行“else”分支，并首先判断 pass 参数是否为 0，如果为 0 则跳出；否则这段程

序将调用pci\_add\_new\_BUS函数为这个PCI桥创建并初始化pci\_BUS结构，同时还需要初始化PCI桥的SubordinateBusNumber、SecondaryBusNumber和PrimaryBusNumber寄存器，之后这段程序也递归调用pci\_scan\_child\_BUS函数。当pci\_scan\_child\_BUS函数递归完毕后，重新修正pci\_BUS $\rightarrow$ subordinate参数。

# 3.acpi\_pci\_root\_add函数的剩余操作

当pci\_scan\_bridge函数执行完毕后，我们再次回到acpi\_pci\_root\_add函数，如源代码14-31所示。

源代码14-31 acpi\_pci\_root\_add函数片段3  
result = acpi_pci_bind_root(device); if(result) goto end;   
... status $=$ acpi_get_handle(device->handle，METHOD_NAME_PRT,&handle）； if(ACPI_SUCCESS(status)) result $\equiv$ acpi_pci_irq_add_prt(device->handle，root->bus）;   
... list_for_each_entry(child,&device->children,node) acpi_pci_bridge_scan(child); /\*Indicate support for various_OSC capabilities. \*/ if(pci_ext_cfg_avail(root->bus->self)) flags $\mid =$ OSC_EXT_PCI_CONFIG.SupportPORT; if(pcie_aspmenabled()) flags $\mid =$ OSCACTIVE_STATE_PWRSupport| OSC_CLOCK_PWR_CAPABILITYSupport; if(pci_msi.enabled()) flags $\mid =$ OSC_MSI-supported; if（flags！=base_flags） acpi_pci_osc_support(root,flags）; return 0;   
end: if(!list_empty(&root->node)) list_del(&root->node); kfree(root); return result;

这段代码首先调用acpi\_pci\_bind\_root函数绑定acpi\_device与pci\_BUS结构。该函数还将acpi\_device $\rightarrow$ ops.bind和ops.unbind参数分别赋值为acpi\_pci\_bind和acpi\_pci\_unbind。然后这段代码调用acpi\_pci\_irq\_add\_prt和acpi\_pci\_bridge\_scan函数分析当前处理器系统的中断路由表，这部分内容将在第15.1.2节介绍。

这段代码在 pcie\_aspm Enabled、pci\_msi-enabled 函数成功返回后将 HOST 主桥的\_OSC 参数的“MSI supported 位”和“Active State Power Management supported”位设置1。

# 4.acpi\_pci\_root\_start函数

acpi\_pci\_root\_add函数执行完毕后，Linux x86将调用acpi\_pci\_root\_start函数。该函数首先扫描acpi\_pci\_ROOTs链表，并调用pci\_BUS\_add/devices函数处理这个链表中的每一个HOST主桥。pci\_BUS\_add/devices函数在 ./driver/pci/bus.c 文件中，其实现如源代码14-32所示。

源代码14-32pci BUS\_adddevices函数  
voidpci BUS_add Devices(const structpci bus \*bus)   
{ structpci_dev\*dev; structpci BUS \*child; intretval; list_for_each_entry(dev,&bus->devices，bus_list）{//\*Skipalready-addeddevices\*/ if（dev->is-added) continue; retval $=$ pcibus_add_device（dev）； if（retval） dev_err(&dev->dev，"Error adding device，continuing\n"); } list_for_each_entry(dev,&bus->devices，bus_list） { BUG_ON(!dev->is-added); child $=$ dev->subordinate; if(!child) continue; if(list_empty(&child->node)) { down_write(&pci BUS_sem）; list_addTAIL(&child->node，&dev->bus->children）; up_write(&pci BUS_sem）; }pci BUS_addDevices(child)； if(child->is-added) continue; retval $=$ pcibus_add_child(child）; if（retval） dev_err(&dev->dev，"Error adding bus，continuing\n"); 1

这段代码首先调用pci BUS\_add\_device函数，将当前PCI总线(pci BUS结构)上的所有PCI设备的相关信息(pci\_dev结构)加入到proc和sysfs文件系统中。

之后这段代码递归调用pci\_BUS\_add/devices函数遍历当前PCI总线上所有PCI子桥。这段代码最后调用pci\_BUS\_add\_child函数初始化PCI子桥pci\_BUS结构的dev.parent参数，并将一些相关信息加入到sysfs文件系统中。

当acpi\_pci\_root\_start函数返回后，acpi\_pci\_root\_init函数将执行完毕。Linux系统将继续调用acpi\_pci\_link\_init函数进一步初始化PCI总线，该函数与PCI总线的中断路由相关，在第15.1.3节将详细介绍该函数的实现。

