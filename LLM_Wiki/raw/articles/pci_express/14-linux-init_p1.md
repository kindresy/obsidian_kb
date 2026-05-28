---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "14"
section: "第14章 Linux PCI的初始化过程"
part: 1
tags: [pci, pci-express, computer-architecture]
---
# 第14章 Linux PCI的初始化过程

Linux PCI 初始化的主要工作是遍历当前处理器系统中的所有 PCI 总线树，并初始化 PCI 总线树上的全部设备，包括 PCI 桥与 PCI Agent 设备。在 Linux 系统中，多次使用了 DFS 算法对 PCI 总线树进行遍历查找，并分配相关的 PCI 总线号与 PCI 总线地址资源。

单纯从一种处理器系统的角度来看，Linux PCI 的实现机制远非完美。其中有许多冗余的代码和多余的步骤，比如 Linux PCI 中对 PCI 总线树的遍历次数过多，从而影响 Linux PCI 的初始化代码的执行效率。产生这些不完美的主要原因是，Linux PCI 首先以 x86 处理器为蓝本编写，而后作为通用代码逐渐支持其他处理器，如 ARM、PowerPC 和 MIPS 等。不同的处理器对 PCI 总线树的遍历机制并不完全相同，而 Linux PCI 作为通用代码必须兼顾这些不同，从而在某种程度上造成了这段代码的混乱。这种混乱是通用代码的无奈之举。

本章以x86处理器系统为例，介绍Linux PCI的执行流程。目前ACPI机制在x86处理器系统已经得到大规模的普及，而且在x86处理器中，只能使用ACPI机制支持处理器最新的特性。因此掌握ACPI机制，对于深入理解x86处理器的软件架构，已经不可或缺。为此本章将重点介绍Linux PCI在ACPI机制下的初始化过程，而不再介绍Linux PCI的传统初始化方式。

# 14.1 Linux x86 对 PCI 总线的初始化

一个处理器系统首先从 Firmware 开始执行，并由 Firmware 开始引导 Linux 内核。Linux 系统首先从 ./init/main.c 文件的 start\_kernel 函数开始执行。不同的处理器系统使用的 Firmware 并不相同，如 x86 处理器系统使用 BIOS，而 PowerPC 处理器系统使用 U-Boot。有些处理器系统，最初的初始化操作可能由 $\mathrm{E}^2\mathrm{PROM}$ 完成，之后执行 Firmware 中的程序。值得注意的是，在 x86 处理器中常用的 Grub 并不是 Firmware，而是 Linux 系统的引导程序。

start\_kernel函数在调用rest\_init函数之前，其主要工作与操作系统核心层相关，包括进程调度、内存管理和中断系统等主要模块的初始化。而rest\_init函数将创建kernel\_init进程，并由该进程调用do\_basic\_setup $\rightarrow$ do\_initcalls函数完成所有外部设备的初始化，包括PCI总线的初始化，该函数如源代码14-1所示。

源代码14-1 do\_initcalls函数  
```txt
static void __init do_initcalls(void)  
{  
    initcall_t * call;  
    for (call = __early_initcall_end; call < __initcall_end; call++)  
        do_one_initcall(* call); 
```

```txt
/* Make sure there is no pending stuff from the initcall sequence */ flush Scheduled_work(); 
```

do\_initcalls函数的主体是将\_\_early\_initcall\_end和\_\_initcall\_end指针之间的函数全部执行一遍，这两个指针在vmlinux.lds文件中定义。在生成操作系统内核时，一些需要在Linux系统初始化时执行的函数指针被加入到\_\_early\_initcall\_end和\_\_initcall\_end参数之间，之后由do\_initcalls函数统一调用这些函数。Linux系统定义了一系列需要在系统初始化时执行的模块，如源代码14-2所示。这段代码在 ./include/linux/INIT.h 文件中。

源代码14-2 Linux系统的初始化模块   
```c
/*   
\* module_init() - driver initialization entry point \* @ x: function to be run at kernel boot time or module insertion   
\*   
\* module_init() will either be called during do_initcalls() (if   
\* builtin) or at module insertion time (if a module). There can only   
\* be one per module.   
*/   
#define module_init(x) _initcall(x);   
...   
#define _define_initcall(level,fn,id) \\ static initcall_t _initcall_##fn##id __used \_attribute_(( section_(".initcall" level ".init")) ) = fn   
/ \* Early initcalls run before initializing SMP.   
\*   
\* Only for built - in code, not modules.   
\*/   
#define early_initcall(fn) _define_initcall("early",fn,early)   
/ \*   
\* A "pure" initcall has no dependencies on anything else, and purely   
\* initializes variables that couldn't be statically initialized.   
\*   
\* This only exists for built - in code, not for modules.   
\*/   
#define pure_initcall(fn) _define_initcall("0",fn,0)   
#define core_initcall(fn) _define_initcall("1",fn,1)   
#define core_initcall sync(fn) _define_initcall("1s",fn,1s)   
#define postcore_initcall(fn) _define_initcall("2",fn,2)   
#define postcore_initcall sync(fn) _define_initcall("2s",fn,2s)   
#define arch_initcall(fn) _define_initcall("3",fn,3) 
```

```c
define arch_initcall_SYNC(fn) __define_initcall("3s",fn,3s)  
#define subsys_initcall(fn) __define_initcall("4",fn,4)  
#define subsys_initcall_SYNC(fn) __define_initcall("4s",fn,4s)  
#define fs_initcall(fn) __define_initcall("5",fn,5)  
#define fs_initcall_SYNC(fn) __define_initcall("5s",fn,5s)  
#define rootfs_initcall(fn) __define_initcall("rootfs",fn,rootfs)  
#define device_initcall(fn) __define_initcall("6",fn,6)  
#define device_initcall_SYNC(fn) __define_initcall("6s",fn,6s)  
#define late_initcall(fn) __define_initcall("7",fn,7)  
#define late_initcall_SYNC(fn) __define_initcall("7s",fn,7s)  
#define __initcall(fn) device_initcall(fn)  
#define __exitcall(fn) \  
static exitcall_t __exitcall__##fn __exit_call = fn 
```

以上初始化模块按照\_defined\_initcall 定义的顺序执行，首先执行 early\_initcall 初始化模块，之后是 pure\_initcall 模块、core\_initcall 模块等，最后执行 late\_initcall\_SYNC。如果 Linux 设备驱动程序采用 built-in 的方式而不是作为 Module 形式加载时，将使用 device\_initcall 函数或者 device\_initcall\_SYNC 函数进行加载。

在Linux系统初始化时运行的模块需要使用以上的xxx\_initcall宏，定义该模块的函数指针，之后该模块的函数指针将加入到Linux内核的\_\_early\_initcall\_end和\_\_initcall\_end之间。我们以xyz\_init模块的加载为例说明这些xxx\_initcall函数的使用，xyz\_init函数用来加载某个模块。该函数的初始化过程如源代码14-3所示。

源代码14-3 xxx\_initcall函数  
```c
static int _init xyz_init(void)  
{  
    ...  
}  
xxx_initcall (xyz_init); 
```

这段代码首先使用宏xxx\_initcall定义了一个\_\_initcall\_xyz\_initx函数，该函数存放xyz函数的指针。在生成Linux系统内核时，链接器将这个函数指针存放在\_\_early\_initcall\_end和\_\_initcall\_end参数之间。

Linux 系统在初始化时，将在 do\_initcalls 函数中执行 \_\_initcall\_xyz\_initx 函数，从而执行 xyz\_init 函数。Linux 系统使用这种方法规范初始化模块的执行，并保证这些模块可以按照指定的顺序依次执行。

在Linux内核的System.map文件中，可以找到在\_early\_initcall\_end和\_initcall\_end之间

所有的函数指针，其中与PCI总线初始化相关的函数如源代码14-4所示，这些函数将按照在以下源代码中出现的顺序依次执行。

源代码14-4 System.map文件中与PCI总线初始化相关的函数  
```txt
c0836ba4 t __initcall_pcibus_class_init2  
c0836ba8 t __initcall_pci_driver_init2  
c0836bd4 t __initcall_acpi_pci_init3  
c0836bec t __initcall_pci_arch_init3  
c0836c1 c t __initcall_pci_slot_init4  
c0836c34 t __initcall_acpi_pci_root_init4  
c0836c38 t __initcall_acpi_pci_link_init4  
c0836c70 t __initcall_pci_subsys_init4  
c0836ca4 t __initcall_pci_iommu_init5  
c0836cf0 t __initcall_pcibios_assigneousResources5  
c0836ebc t __initcall_pci_init6  
c0836ec0 t __initcall_pciproc_init6  
c0836ec4 t __initcall_pcie_portdrv_init6  
c0836ecc t __initcall_pci.hotplug_init6  
c083706c t __initcall_pci_sysfs_init7  
c0837084 t __initcall_pci_mmcfglate_insert-resources7 
```

每一次编译Linux内核时，都可能会产生一个新的System.map，但是源代码14-4中函数指针的顺序不会发生变化，其执行顺序也不会发生变化。下面将依次分析这些函数的功能。并在后续章节，逐步解析这些函数的实现方法。

# 14.1.1 pcibus\_class\_init与pci\_driver\_init函数

pcibus\_class\_init函数在 ./driver/pci/probe.c 文件中，如源代码14-5所示。该函数的主要作用是注册一个名为“pci\_BUS”的class结构。在Linux系统中，为了便于测试将所有的设备使用一个文件系统进行管理，这个文件系统也被称为sysfs文件系统。

最初Linux系统将与设备相关的信息都存放在proc文件系统中，而随着Linux系统的不断演变，proc文件系统变得异常混乱而复杂，难以维护，于是sysfs文件系统应运而生。与proc文件系统相比，sysfs文件系统的组织结构较为清晰。

目前与设备相关的模块基本上都由sysfs文件系统维护，而proc文件系统留给真正的系统进程使用。本书不会详细介绍sysfs文件系统的详细实现机制，因为sysfs文件系统与PCI体系结构并没有太大的关系，只是Linux系统使用的一种对设备模块进行管理的方法。

源代码14-5 pcibus\_class\_init函数  
```txt
static struct class pcibus_class = {  
    .name = "pci_BUS",  
    .dev_release = &release_pcibus_dev,  
}; 
```

```txt
static int __init pcibus_class_init(void)  
{ return class register(&pcibus_class); } postcore_initcall(pcibus_class_init); 
```

pcibus\_class\_init函数执行完毕后，将会在/sys/class目录下产生一个“pci\_BUS”的目录，有兴趣的读者可以使用“ls -l /sys/class”命令找到这个目录。该函数执行完毕后，将很快执行pci\_driver\_init函数，如源代码14-6所示。

源代码14-6pci\_driver\_init函数  
```c
struct bus_type pci_BUS_type = {
    .name = "pci",
    .match = pci_BUS_MATCH,
    .uevent = pci_uevent,
    .probe = pci_device Probe,
    .remove = pci_device_remove,
    .shutdown = pci_device_shutdown,
    .dev_attrs = pci_dev_attrs,
    .bus_attrs = pci_BUS_attrs,
    .pm = PCI_PM_OPS_PTR,
};  
static int __init(pci_driver_init(void)
{
    return busRegister(&pci_BUS_type);
} 
```

该函数也与sysfs文件系统相关，该函数执行完毕后，将在/sys/bus目录下建立一个“pci”目录，之后当Linux系统的PCI设备使用device register函数注册一个新的pci设备时，将在/sys/bus/pci/drivers目录下创建这个设备使用的目录。

如在第12章源代码12-1中，pci register driver函数将最终调用device register函数，并在/sys/bus/pci/drivers下建立“capric”目录。在这个capric目录里包含capric卡在Linux系统中使用的一系列资源。

在源代码14-4中也有一些和ACPI机制初始化相关的函数，包括acpi\_pci\_init、acpi\_pci\_root\_init和acpi\_pci\_link函数。有关ACPI机制的介绍见第14.2节。

# 14.1.2 pci\_arch\_init函数

pci\_arch\_init函数是Linux x86系统执行的第一个与PCI总线初始化相关的函数。该函数的定义在./arch/x86/pci init.c文件中，如源代码14-7所示。

源代码14-7 pciarch\_init函数  
/\*arch_initcall has too random ordering, so call the initializers in the right sequence from here. \*/   
static _init int pci_arch_init(void)   
{   
#ifdef CONFIG_PCI_DIRECT int type $= 0$ . type $=$ pcidirectprobe();   
#endif if(! (pciProbe&PCI_PROBE_NOEARLY)) pcimmcfg_early_init();   
#ifdef CONFIG_PCI_OLPC if(!pci_olpc_init()) return 0; /\* skip additional checks if it's an XO \*/   
#endif   
#ifdef CONFIG_PCI_BIOS pcipcbios_init();   
#endif / \* \* don't check for raw_pci ops here because we want pcbios as last \* fallback, yet it's needed to run first to set pcibios_last_BUS \* in case legacy PCI probing is used. otherwise detecting peer busses \* fails. \*/   
#ifndef CONFIG_PCI_DIRECT pcidirect_init(type);   
#endif if(!raw_pci ops &&!raw_pci_extOps) printf(KERN_ERR "PCI:Fatal:No config space access function found\n"); dmi_check_pciprobe(); dmi_check skips_isa_align(); return 0;   
} arch_initcall(pci_arch_init);

该函数使用了一些编译选项，如果使能 CONFIG\_PCI\_BIOS 选项表示 Linux x86 系统将使用 BIOS 对 PCI 总线的枚举结果；如果使能 CONFIG\_PCI\_DIRECT 选项表示由 Linux x86 系统重新枚举 PCI 总线；如果使能 CONFIG\_PCI\_OLPC 选项表示当前处理器系统属于 OLPC (One Laptop per Child)。本节仅讲述使能 CONFIG\_PCI\_DIRECT 选项的情况。pci\_arch\_init 函数首先调用pci\_directProbe 函数，pci\_direct Probe 函数如源代码 14-8 所示。

源代码14-8pci\_directprobe函数  
int_initpci_directprobe(void)   
{ struct resource \*region，\*region2; if((pciProbe&PCI_PROBE_CONF1） $= = 0$ ） goto type2; region $=$ request_region(0xEF8,8，"PCI conf1"); if（！region） goto type2; if(pci_check_type1()){ raw_pci ops $\equiv$ &pci_direct_conf1; port_cf9_safe $\equiv$ true; return 1; } releaseResource(region);   
type2: if((pci Probe&PCI_PROBE_CONF2） $= = 0$ ） return 0; region $=$ request_region(0xEF8,4，"PCI conf2"); if（！region） return 0; region2 $=$ request_region(0xC000,0x1000，"PCI conf2"); if（！region2） goto fail2; if(pci_check_type2()){ raw_pci ops $\equiv$ &pci_direct_conf2; port_cf9_safe $\equiv$ true; return 2; } releaseResource(region2);   
fail2: releaseResource(region); return 0;

pci\_directProbe函数首先根据全局变量pciProbe判断raw\_pci ops函数使用的函数指针。全局变量pciProbe的缺省值在./arch/x86/pci/common.c中定义，如下所示。

unsigned int pcieprobe $=$ PCI_PROBE_BIOS | PCI_PROBE_CONF1 | PCI_PROBE_CONF2 | PCI_PROBE_MMCONF;

如果 Boot loader 程序(如 Grub)在引导 Linux 内核时没有加入 “pci = xxxx” 参数, 全局变

量pciprobe将使用缺省值。此时pci\_direct probe函数仅使用“conf1类型”而不使用“conf2类型”对raw\_pci ops函数赋值。

x86处理器提供了三种方式访问PCI设备的配置空间。一种方法是使用“0xEF8和0xCFC”这两个I/O端口，这两个端口的详细描述见第2.2.4节，Linux x86系统使用pci\_conf1\_read和pci\_conf1\_write函数操作这两个I/O端口，这两个函数的定义见 ./arch/x86/pci/direct.c文件。

另一种方法是使用“conf2”方法，目前这种方法不再被Linux x86继续使用，对这种方法有兴趣的读者可以参考pci\_conf2\_read和pci\_conf2\_write函数，本节对这种方法不做介绍。

unix x86 使用pci\_mmcfg\_read 和pci\_mmcfg\_write 函数实现ECAM方式，这两个函数的定义见 ./arch/x86/pci/mmconfig\_32.c 文件中。

其中使用pci\_conf1\_read和pci\_conf1\_write函数只能访问PCI设备配置空间的前256个字节，而使用pci\_mmcfg\_read和pci\_mmcfg\_write函数可以访问PCI设备的全部配置空间。在Linux系统中，可以使用这两种方式访问不同的配置空间。

pci\_directProbe函数执行完毕，pci\_arch\_init函数将继续调用pci\_direct\_init函数，然后依次调用dmi\_check\_pciprobe()和dmi\_check Skip\_isa\_align()函数，这两个dmi\_xxx函数与x86处理器的DMI(Desktop Management Interface)接口和SM(System Management)总线相关，本节对此不做进一步说明。

# 14.1.3 pcislot\_init和pci\_subsys\_init函数

Linux x86 系统执行完毕pci\_arch\_init函数后，将调用pci\_slot\_init函数，该函数的主要作用是在sysfs文件系统中，建立slots目录及其kobject结构。pci\_subsys\_init函数是一个重要的函数，其定义在./arch/x86/pci/legacy.c文件中，如源代码14-9所示。

源代码14-9pci\_subsys\_init函数  
```c
int __init pci_subsys_init(void)  
{  
#ifdef CONFIG_X86_NUMAQ  
pci_numaq_init();  
#endif  
#ifdef CONFIG ACPI  
pci_acpi_init();  
#endif  
#ifdef CONFIG_X86_VISWS  
pci.visws_init();  
#endif  
pcilegacy_init();  
pcibios_fixuppeer_bridge();  
pcibios_irq_init();  
pcibios_init();  
return 0;  
}  
subsys_initcall(pci_subsys_init); 
```

本书并不关心CONFIG\_X86\_NUMAQ和CONFIG\_X86\_VISWS选项。在第14.3.3节将详细介绍CONFIG ACPI选项使能时使用的pci ACPI\_init函数。

pcilegacy\_init函数完成对PCI总线的枚举，并在proc文件系统和sysfs文件系统中建立相应的结构。如果当前处理器系统没有使能ACPI机制，则该函数是Linux x86对PCI总线进行初始化的一个重要函数，其实现机制如源代码14-10所示。

# 源代码14-10pcilegacy\_init函数

```c
static int __init pcielegacy_init(void)  
{  
    if (!raw_pciOps) {  
        printf("PCI: System does not support PCI\n");  
        return 0;  
    }  
    if (pcibios_scanned++)  
        return 0;  
    printf("PCI: Probing PCI hardware\n");  
    pcie_root_BUS = pcibios_scan_root(0);  
    if (pci_root_BUS)  
        pcie_BUS_add/devices(pci_root_BUS);  
    return 0; 
```

pcilegacy\_init函数首先调用pcibios\_scan\_root函数完成对PCI总线树的枚举，该函数的输入参数为0表示这次枚举将从总线号0开始进行。在完成PCI总线的枚举后，该函数将调用pci\_BUS\_add Devices函数将PCI总线上的设备加入到sysfs文件系统中。

Linux x86 引入 ACPI 机制之后，pcibios\_scanned 参数将被置为 1，从而 pcilegacy\_init 函数将直接使用 0 作为返回值，并不会执行 pcibios\_scan\_root 和 pci\_BUS\_add Devices 函数。

当pcilegacy\_init函数执行完毕后，pcibios\_irq\_init函数将使用BIOS提供的中断路由表，初始化当前处理器系统的中断路由表，同时确定PCI设备使用的中断向量，本章并不会对该函数进行详细分析，因为Linux x86目前大多使用ACPI提供的中断路由表，而不再使用BIOS中的中断路由表。如果ACPI机制被使能，该函数也将直接使用0作为返回值，并不会被完全执行。

pcibios\_init函数的主要工作是调用pcibiosResourcesurvey函数，检查PCI设备使用的存储器及I/O资源。pcibiosResourcesurvey函数将在第14.3.3节中详细介绍。

# 14.1.4 与PCI总线初始化相关的其他函数

pci\_iommu\_init函数在 ./arch/x86/kernel/pci-dma.c 文件中，该函数用来初始化处理器系统的IOMMU，可以配置IBM X-Series刀片服务器使用的Calgary IOMMU、Intel的Vtd和AMD的IOMMU使用的I/O页表。如果在Linux系统中没有使能IOMMU选项，pci\_iommu\_init函

数将调用 no\_iommu\_init 函数，并将 dmaOps 函数设置为 nommu dmaOps。本节不进一步介绍该函数的详细实现机制。

pcbios\_assign-resources 函数主要处理 PCI 设备使用的 ROM 空间和 PCI 设备使用的存储器和 I/O 资源。该函数的主要功能是调用pci\_assign\_unassigned-resources 函数对 PCI 设备使用的存储器和 I/O 资源进行设置。对于 Linux x86 而言，BIOS 已经将 PCI 设备使用的存储器和 I/O 资源设置完毕，而其他 Linux 系统，如 Linux PowerPC，需要使用该函数设置 PCI 设备使用的存储器和 I/O 资源。

pci\_init函数的主要作用是对已经完成枚举的PCI设备进行修复工作，用于修补一些BI-OS中对PCI设备有影响的Bugs。

pci\_proc\_init函数的主要功能是在proc文件系统中建立 ./bus/pci目录，并将proc.fs默认提供的file\_operations更换为proc.bus\_pci\_dev\_operations。

pci\_portdrv\_init 函数首先在 ./sys/bus 中建立pciExpress目录，然后使用pci register driver函数向内核注册一个名为pci\_portdriver的pci\_driver结构。在Linux x86中，pciexpress目录中的设备都是从sysfs文件系统的pci目录中链接过来的。该函数的实现较为简单。

pci.hotplug\_init函数主要用来支持CompactPCI的热插拔功能。CompactPCI总线在通信系统中较为常见。

而pci\_sysfs\_init函数与sysfs文件系统相关，主要功能是将每一个PCI设备加入到sysfs文件系统的相应目录中，本节对此不做进一步介绍。pci\_mmcfglate\_insert-resources函数的主要功能是将MMCFG使用的资源放入系统的ResourceTree中，并标记这些资源已经被使用，之后其他驱动程序不能再使用这个资源。

本章并不会对Linux x86使用的Legacy PCI总线枚举方法进一步描述，x86处理器为了实现向前兼容，付出了巨大的努力。x86处理器在实现新的功能的同时，需要向前兼容古董级别的功能，有时BIOS无所适从。Linux x86对PCI总线进行初始化时，使用了许多不完美的源代码。而这些貌似不完美的源代码背后，都有许多与向前兼容有关的故事。

# 14.2 x86处理器的ACPI

在x86处理器中，ACPI(Advanced Configuration and Power Interface)是一个非常重要而且较为复杂的概念。最初ACPI规范由Intel、Microsoft和Toshiba公司共同制定，后来HP和Phoenix公司也参与了ACPI规范的制定，该规范主要包括x86处理器系统的资源配置和电源管理两方面内容。

ACPI规范整合了之前的OSPM（Operating System directed Power Management）、MultiProcessor规范和Plug and Play BIOS规范，并定义了一系列数据结构与电源管理状态，提供了电源管理接口、硬件及其Firmware接口，以描述处理器系统的设备和电源管理策略。ACPI规范在1996年12月发布1.0版本，目前的稳定版是4.0版。

ACPI规范是x86处理器使用的Firmware接口标准，操作系统需要获得的处理器底层信息基本上都可以从ACPI表中获得。在ACPI诞生之前，基于x86处理器的操作系统需要使用BIOS才能获得相应的信息。而不同厂商提供的BIOS之间并没有一个统一标准，从而在某种程度上造成硬件资源管理与使用上的混乱。

产生这种混乱的主要原因是由于x86处理器系统为了实现向前兼容，有许多不得已；而部分原因是在x86处理器系统中，有许多外部设备本身就挂接在一些“不可配置的”总线上，如ISA/EISA总线，还有一些外部设备本身就是不标准的，如电源按钮和在笔记本上使用的一些“特殊功能键”。

在ACPI规范没有出现之前，BIOS厂商通常按照某种“自定义”的方式使用这些外部设备，因此需要为操作系统提供各类驱动程序，并由操作系统集成这些并不属于任何标准的驱动程序，从而给操作系统的开发与维护带来了极大的困难。

ACPI机制在这种背景下应运而生。ACPI机制提供了一组与处理器硬件和操作系统的相关接口，对处理器平台以及设备的电源进行管理，并可以配置和管理外部设备使用的系统资源。ACPI机制主要管理以下系统资源。

- LegacyPNP设备，如ISA设备、串并口等设备。  
- 笔记本使用的一些外部设备，如电源开关、风扇、电源和一些快捷键。  
- 系统电源管理，包括处理器和外部设备的电源管理。这部分内容也是ACPI规范的设计重点。  
- 系统的热插拔管理。热插拔是 ACPI 规范的设计重点，ACPI 系统热插拔可以覆盖小到“从笔记本插拔 CDROM”，大到“NUMA(Non-uniform Memory Access)结构处理器系统热插拔 PE(Processor Element)、存储器节点(Memory Node)和 I/O 节点”这些应用。不过许多 NUMA 处理器系统并没有使用 ACPI 规范进行热拔插管理。本节对 ACPI 的热插拔管理不做深入介绍。  
- PCI 设备的中断向量分配。在 x86 处理器平台中, 中断向量的分配始终是一个问题。x86处理器由于一些历史遗留问题, 中断向量的分配并不尽善尽美, 而 x86 处理器为了实现向前兼容, 必须保留这些不完美。本章将在第 15.1.1 节详细介绍中断向量的分配。  
- 一些集成在 MCH 和 ICH 中的外部设备。x86 处理器使用 PCI 配置空间存放这些外部设备的寄存器，但是这些设备并不都是严格意义上的 PCI 设备，甚至不是一个外部设备。如在 x86 处理器中的使用存储器映射寻址的一些寄存器，这些寄存器被存放在某个 PCI 设备的配置空间中，如 TOLUD 寄存器存放在 Bus 号、Device 号和 Function 号都为 0 的 PCI 设备中，但是这个 PCI 设备并不是处理器系统的标准 PCI 设备。ACPI 规范需要管理这类“伪 PCI 设备”中的寄存器。

目前在x86处理器中，新引入的一些与处理器体系结构相关的特性，基本上都只使用ACPI机制进行描述，而不再使用BIOS。因此为了深入理解x86处理器平台，需要了解一些与ACPI相关的基本知识。ACPI规范所涉及的内容非常广泛，与ACPI规范有关的全部知识可以独立成书，本节仅简要介绍ACPI规范中与PCI总线相关的部分内容。ACPI规范的各部分内容相对较为独立，除了ACPI规范的开发者和从事与此相关的系统程序员之外，绝大多数程序员不需要了解与ACPI规范相关的全部知识。

从系统软件的角度上看，ACPI的组成结构如图14-1所示。从图中可以发现，ACPI用以连接系统硬件平台与操作系统，在屏蔽了硬件的实现细节的同时，提供了一系列系统资源，包括ACPI寄存器（ACPI Registers）、ACPI BIOS和ACPI表（ACPI Tables）。

![[pci_express/4d0961ac9730e679a055ffeb921f86d03769b05485ee86745548d785c2cbb3b2.jpg]]  
图14-1 ACPI的组成结构

当一个处理器系统使能ACPI机制后，操作系统访问ACPI管理的“非标”外设时，首先通过ACPI机制提供的一套标准API，并由这些API将访问存放在系统内存中的ACPI表，并通过执行ACPI表中的程序读写ACPI寄存器，或者对这些寄存器进行操作。在ACPI表中，除了含有处理器系统的资源信息之外，还包括管理这些资源信息的操作函数，因此BIOS厂商通过ACPI表即可实现一些简单的设备驱动程序。

操作系统使用这些API，并通过ACPI Driver/AML Interpreter访问ACPI提供的系统资源，包括ACPI Registers、ACPI BIOS和ACPI表。ACPI表对于理解ACPI机制较为重要，在第14.2.2节和第14.2.3节将专门介绍该表的组成与实现。

在x86处理器系统中，系统资源由ACPI BIOS维护，操作系统使用ACPI提供的标准API从ACPI BIOS中获得这些资源，而不必关心这些“非标”外设的具体实现方式，因此也不需要使用特定的驱动程序访问这些“非标”外设。

因为这些“非标”外设的管理由ACPI BIOS完成，x86处理器使用ACPI表描述这些“非标”外设，并将这个ACPI表存放到BIOS中。OEM厂商需要在标准PC处理器平台上添加一些“自定义”的功能时，只需改动ACPI表即可，而不需要改动操作系统，从而极大地降低了操作系统的集成与维护难度。

ACPI机制使用的“标准API”在ACPICA(ACPI Component Architecture Programmer Reference)规范中定义。这些标准API与操作系统相对独立，从而便于ACPICA在不同操作系统中的实现。

如图14-1所示，ACPI机制所覆盖的内容包括ACPI寄存器组、ACPIBIOS和ACPI表。在ACPI寄存器组中含有电源管理、处理器控制和GPE(General-PurposeEvent)寄存器组。其中处理器控制寄存器组是可选的，而电源管理和GPE寄存器组是必须实现的。这些与ACPI

机制相关的寄存器在Intel的ICH中定义。

电源管理寄存器组由PM1a\_STS、PM1a\_EN、PM1b\_STS、PM1b\_EN、PM1\_CNTa、PM1\_CNTb等寄存器组成。在这些寄存器中，PM1a\_STS、PM1\_CNTa寄存器和PM1a\_EN是必须支持的，而其他寄存器是可选的。在Intel的ICH9中，PM1a\_STS寄存器与PM1\_STS寄存器对应，PM1\_CNTa寄存器与PM1\_CNT寄存器对应，而PM1a\_EN寄存器与PM1\_EN寄存器对应。这些寄存器的简单描述如下，有兴趣的读者可以参考[Intel I/O Controller Hub 9]的第13.8节以获得这些寄存器的详细说明。

- PM1\_STS 寄存器包含当前处理器被唤醒的原因，以及一些与电源按键（Power Button）相关的信息。  
- 通过操作 PM1\_CNT 寄存器可以使处理器进入不同的休眠状态，并确定 ACPI 中断请求使用 SCI 中断请求还是 $\mathrm{SMI}^{\ominus}$ 中断请求。  
- PM1\_EN 寄存器设置 SCI(System Control Interrupt) 中断使能位，决定当 PM1\_STS 寄存器的状态有效时，是否向处理器提交 SCI 中断请求。SCI 中断请求由 ACPI 规定的相应事件使用，并缺省使用中断向量 9 向处理器提交中断请求，操作系统使用中断服务程序进一步处理来自 ACPI 的中断请求。

GPE寄存器组是ACPI寄存器组的一个重要组成部分，由GPE0\_STS、GPE0\_EN、GPE1\_STS和GPE1\_EN寄存器组成。在Intel的ICH9中，由GeneralPurposeI/O寄存器组实现ACPI规范的GPE寄存器组。GeneralPurposeI/O寄存器组的使用方法非常简单，对此有兴趣的读者可以参考[IntelI/OControllerHub9]的第13.10节。

ACPI的GPEx\_STS和GPEx\_EN寄存器的使用方法较为简单。其中GPEx\_STS寄存器的每一位在GPEx\_EN寄存器中都有一个使能位，当GPEx\_STS寄存器的某位有效时，表示产生了一个GPE事件，如果与该位相对应的使能位也有效时，ICH将向处理器提交SCI中断请求，由中断服务程序进一步处理这个GPE事件。

ACPI BIOS与操作系统中的ACPI驱动程序/AML解释器(ACPI Driver/ACPI Machine Language Interpreter)密切相关。操作系统可以使用ACPICA提供的标准API，再通过ACPI驱动程序/AML解释器访问ACPI BIOS，并从ACPI BIOS获得相应的信息后执行与底层硬件相关的代码操纵实际的设备，有关这部分内容的详细说明见下文。

ACPI表描述处理器平台使用的资源和管理这些资源的执行操作，操作系统可以通过标准的API函数访问ACPI表并执行相关的程序，从而维护整个ACPI系统的运转。ACPI表是BIOS提供给系统软件的重要资源。

