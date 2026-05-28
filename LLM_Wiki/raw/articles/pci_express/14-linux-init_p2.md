---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "14"
section: "14.2.1 ACPI驱动程序与AML解释器"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 14.2.1 ACPI驱动程序与AML解释器

ACPI驱动程序与AML解释器与操作系统实现相关，其主要目的是将操作系统与ACPI提供的资源进行隔离。如上文所述，ACPI使用一组标准的API函数访问ACPI表，目前Unix/Linux系统使用ACPICA规范实现这些接口函数，ACPICA的组成结构如图14-2所示。值得注意的是Windows使用了其他方式实现这些接口函数。

![[pci_express/95a8774786bc4f041474599b33d1d4e7de65920ddd1f6ec6ab090401eb265a5a.jpg]]  
图14-2 ACPICA的组成结构

由上图所示，当操作系统需要访问ACPI表时，将首先通过ACPICA接口函数，因此在一个操作系统中，仅需要关注ACPICA接口函数，而不必了解硬件的具体实现细节，并由BIOS管理硬件的具体实现细节。从而使得某x86处理器平台引入的某些“自定义”功能仅与BIOS有关，而与操作系统无关。

# 1. ACPICA 接口函数

ACPICA子系统提供了一系列标准的函数接口，Host OS可以通过这些函数接口向AML解释器传递数据，并由AML解释器访问ACPI的相关资源。在Linux系统的./drivers/acpi/acpica目录中，定义了这些ACPI接口函数，这些ACPI接口函数包括以下几大类。本章并不会详细介绍这些接口函数，而仅在分析相关代码时简要介绍对应的接口函数。

- ACPI Table Management 接口函数。这组 API 负责分析和管理在操作系统中存放的 DS-DT、FAT 等描述符，ACPI 表在操作系统引导时放入系统内存中，有关 ACPI 表的详细描述见第 14.2.2 节。  
- Namespace Management 接口函数。这组 API 负责创建 APCI 表在操作系统中存放的名字空间，并管理这些名字空间。  
- Resource Management 接口函数。在名字空间中包含一些硬件资源，如 I/O 地址空间和中断向量等，这组 API 负责管理名字空间中使用的各类资源。  
- Event Management 接口函数。这组 API 负责处理 ACPI 中的各类 Event, 包括 GPE 事件和 PM 事件。  
- ACPI Hardware Management 接口函数。这组 API 负责访问 ACPI 提供的各类硬件资源，包括寄存器和中断。

Host OS 通过 ACPICA 提供的接口函数最终可以访问 AML 解释器，并由 AML 解释器访问底层硬件。当一个系统使能了 ACPI 机制后，底层硬件的实现细节将被屏蔽，AML 解释器并不会直接访问底层硬件，而是直接访问 ACPI 提供的硬件抽象层。ACPI 使用 AML 语言描述这个硬件抽象层。

在一个处理器系统中，ACPI BIOS将提供ACPI表，并由BIOS存放到处理器系统的特定存储器空间中。操作系统在初始化时，需要通过某个地址找到这个ACPI表。

值得注意的是ACPI主要管理“非标准”外部设备的硬件资源，并不会管理一些标准外部设备，如标准PCI设备，因此在ACPI表中并不包含标准PCI设备使用的BAR地址空间等一系列标准信息，但是会管理PCI总线中的中断路由表。

ACPI表由AML解释器负责分析并维护，在ACPI表中除了存放底层硬件的资源描述外，还可以操作这些资源。有关ACPI表的详细介绍见下文。

如图14-2所示，在ACPICA中还存在一个OS服务层(OS Services Layer)接口，该接口的主要目的是保证ACPICA实现的“系统独立性”。目前在Unix/Linux系统中，ACPICA接口函数使用OS服务层接口，访问与ACPI相关的硬件资源或者执行相应的操作。

# 2.OS服务层接口函数

ACPICA的实现与操作系统无关，但是ACPI接口函数仍然需要使用一些操作系统资源，比如ACPI接口函数需要使用操作系统提供的分配与释放内存资源，访问PCI设备配置空间等一系列API函数。

但是在不同的操作系统中，访问系统内存资源、访问 PCI 配置空间所需要调用的 API 函数并不相同。为了保证 ACPICA 的实现与操作系统无关，ACPICA 抽象了 OS 服务层接口函数。ACPICA 的接口函数使用 OS 服务层接口函数，而不是操作系统提供的函数访问 HOST OS 的资源，以保证 ACPICA 的独立性。值得注意的是，在不同的操作系统中，ACPICA 定义这组函数的实现并不相同，目前 Linux/Unix 系统使用了 ACPICA 定义的这组函数，而 Windows 使用其他的方法实现这些功能。

在Linux系统中，OS服务层接口函数的实现在 ./drivers/acpi/osl.c 文件中，在该文件中提供了一系列访问系统资源的标准函数，这些函数实现的功能相对简单，如acpi\_os\_printf函数、acpi\_os\_sleep函数、acpi\_os\_write\_memory、acpi\_os\_read\_pci\_configuration。在该文件中，还包含一些最基本的与访问外部设备、内存管理和进程调度相关的操作函数。

例如在ACPICA程序释放中断服务例程时，需要调用acpi\_os\_remove\_interrupt\_handler函数，而不能直接调用Linux系统提供的free\_irq函数，即便在Linux系统中，这两个函数几乎等价。因为acpi\_os\_remove\_interrupt\_handler函数的主要工作就是调用free\_irq函数。该函数的实现如源代码14-11所示。

源代码14-11 acpi\_os\_remove\_interrupt\_handler函数  
acpi_status   
acpi_os_remove_interrupthandler(u32irq，acpi_os handler handler)   
{ if（irq）{ free_irq（irq，acpi_irq）; acpi_irqhandler $=$ NULL; acpi_irq_irq $= 0$ · } return AE_OK;

但是在开发与ACPICA相关的函数时，需要调用acpi\_os\_remove\_interrupt\_handler函数，而不能直接使用free\_irq函数，以保证ACPICA的平台无关性，因为在不同的操作系统中，释放中断服务例程使用的函数并不相同。

ACPICA使用OS服务层接口函数，极大降低了ACPICA接口函数的移植难度，在Linux系统中实现的ACPICA接口函数可以方便地移植到其他操作系统中。

# 14.2.2 ACPI表

ACPI规范使用了一系列描述符表管理处理器系统的部分硬件信息，而且包含与这些硬件相关的操作，并使用RSDP指针(Root System Description Pointer)指向这些描述符表。ACPI规范定义了以下描述符表。

- XSDT(Extended System Description Table)。XSDT包含ACPI规范的版本号和一些与OEM相关的信息，并含有其他描述符表的64位物理地址，如FADT(Fixed ACPI Description Table)和SSDT(Secondary System Description Table)等。  
- RSDT(Root System Description Table)。RSDT包含的信息与XSDT基本一致，只是在RSDT中存放的物理地址为32位。在V1.0之后的ACPI版本中，该描述符表被XSDT取代。但是有些BIOS可能会为操作系统同时提供RSDT和XSDT，并由操作系统选择使用RSDT还是XSDT。  
- FADT。FADT 包含 ACPI 寄存器组使用的系统 I/O 端口地址、FACS（Firmware ACPI Control Structure）和 DSDT（Differentiated System Description Table）的基地址等信息。FADT 中还存放了一个“Boot Architecture Flags”字段，在这个字段中存放一些有关处理器系统初始化的基本信息，详见[Advanced Configuration and Power Interface Specification 4.0]的Table 5-11。值得注意的是，FADT 的识别标识是“FACP”，在 ACPI 表中，FACP.dat 文件存放处理器系统的 FADT 表。  
- FACS。FACS包含OS与BIOS进行数据交换使用的一些参数，包括处理器系统的硬件签名，以及Firmware在处理器系统被唤醒后使用的、用来通知操作系统Firmware工作已经告一段落的中断向量，即FirmwareWaking Vector。在处理器被唤醒之后，Firmware将执行一些基本的加载操作，并通过FirmwareWaking中断向量，将控制权交还给操作系统，由操作系统完成其他的唤醒操作。在FACS中，还包含一个全局锁(Global Lock)，当Firmware和操作系统对某些临界资源进行访问时，需要使用该锁。  
- DSDT。该表是 ACPI 规范最复杂，同时也是最重要的一个表。该表包含处理器系统使用的硬件资源以及对这些硬件资源的管理操作。SSDT 可以对 DSDT 进行补充，在一个处理器系统中可以存在多个 SSDT。  
- ACPI规范还定义了一些其他表项，如MADT（Multiple APIC Description Table）、SBST（Smart Battery Table）、SRAT（System Resource Affinity Table）和SLIT（System Locality Information Table)等一系列表项。其中MADT描述处理器系统的中断资源和多处理器相关的配置信息；SBST与电池的管理相关；而SRAT和SLIT与NUMA系统的资源管理相关。在ACPI4.0中，上述这些表的组成结构如图14-3所示。

如上图所示，在RSDP中提供了两个物理地址分别指向RSDT和XSDT。其中在RSDT和在RSDT指向的其他描述符表中，如SSDT和FADT都使用32位物理地址，而在XSDT和在XSDT指向的其他描述符表中都使用64位物理地址。在ACPI2.0规范之后的版本，均提供

![[pci_express/f66f4c7e2b1c51df7624e5587f090a1652b0e19091725ea3f2d6082e50724a9a.jpg]]  
图14-3 ACPI表的组成结构

对XSDT的支持，即使用64位物理地址。

ACPI表存放在处理器的主存储器中，当处理器系统初始化时，BIOS将这些表放入特定物理内存，之后系统软件可以访问这些表项。Linux系统提供了一系列操作ACPI表的工具，用户可以使用这些工具读取在系统内存中的ACPI表，并将其分解为DSDT、XSDT等描述符表。其使用方法如源代码14-12所示。

# 源代码14-12 ACPI表的提取方法

```powershell
$ acpidump > tylersburg - hedt. out
$ acpixtract - a tylersburg - hedt. out
$ iasl -d APCI. dat
$ iasl -d DSDT. dat
...
$ iasl -d XSDT. dat 
```

首先用户可以使用 acpidump 命令将 ACPI 表从内存读出，之后存放到 tylersburg-hedt. out 文件中；然后使用 acpixtract 命令将 tylersburg-hedt. out 文件存放的 ACPI 表全部分解，并得到一系列后缀为.dat 的文件，其中 RSDP.dat 文件存放 RSDT 和 XSDT 表的物理地址；RSDT.dat 文件存放对 RSDT 的描述；而 XSDT.dat 文件存放对 XSDT 的描述。

这些.dat文件使用AML语法规范，操作系统中的AML解释器可以分析这些在.dat文件中的数据。但是这些.dat文件并不适合阅读，用户可以使用iasm命令将这些.dat文件转换为相应的.dsl文件。在.dsl文件中存放ASL(ACPI Source Language)源代码，ASL是一种高级语言，便于阅读和编写。在Linux系统中，可以使用以下方法调试ACPI表。通过源代码14-12，可以得到DSDT.dsl文件，之后可以使用源代码14-13所示的方法调试DSDT表。

# 源代码14-13 DSDT表的调试

```txt
$ iasl -tc DSDT. dsl
产生一个 DSDT. hex 文件
$ cp DSDT. hex $ SRC/include/
将这个文件复制到 Linux 源代码的 include 文件夹下
向 . config 文添加以下描述
CONFIG_STANDALONE = n
将原 . config 的 y 改写为 n
CONFIG ACPIcustom_DSDT = y
CONFIG ACPIcustom_DSDT_FILE = "DSDT. hex"
```

经过以上操作，重新编译Linux内核，并用这个内核重新引导Linux系统后，Linux系统将使用源代码14-13指定的DSDT.hex替代BIOS提供的DSDT表。采用这种方法，可以对DSDT表进行调试。

# 14.2.3 ACPI表的使用实例

在ACPI提供的各类表中，DSDT描述符表最为重要。DSDT描述符表包含当前处理器系统使用的一些硬件资源，如某些外部设备使用的地址空间，以及对这些硬件资源的操作等其他描述信息。

当操作系统收到ACPI中断请求，即SCI中断请求时，将根据DSDT中提供的代码对相应的ACPI寄存器进行操作，从而完成所需的功能。DSDT、ACPI寄存器和操作系统之间的关系如图14-4所示。

![[pci_express/5d6d1e12a0ad8c73b5b5ff02d2baf3d9e58e58ca1239d191137e3af15bb0935e.jpg]]  
图14-4 DSDT、ACPI寄存器和操作系统之间的关系

下文将举例说明图14-4中各模块之间的关系，以及系统软件如何处理ACPI表。假设在一个x86处理器系统中，电源按钮(Power Button)使用GPIO(General Purpose I/O)方式与处理器系统连接，而不是使用Fixed hardware方式。

在ICH9中定义了一个PWRBTN#信号，该信号用来处理电源按钮。如果在一个处理器系统中，电源按钮信号连接到ICH9的PWRBTN#信号时，处理器系统将使用PM寄存器组处理这个电源按钮，即电源按钮使用了Fixedhardware方式与处理器系统进行连接，而不是GPE方式与处理器系统进行连接。

使用Fixed hardware方式处理电源按钮的主要缺点是不够灵活，有时OEM厂商(Original Equipment Manufacturer)可以利用电源按键实现一些自定义的功能。此时主板设计者可以将电源按钮与 $\mathrm{EC}^{\ominus}$ (Embedded Controller)直接相连，并将电源按钮事件与GPE联系在一起。使用这种方式时OEM厂商可以灵活地控制电源按钮。

当用户按下电源按钮时，EC可以根据其按键时间的长短产生不同的电源按钮事件，如“按键小于4s”和“按键大于4s”所对应的电源按钮事件。而且在x86处理器系统处于不同的运行状态，如G0、G1和G2时，对电源按钮事件的解释也并不相同。

x86处理器规定了一系列休眠状态，其中G0为工作状态，与S0状态对应；G1为休眠状态，G1状态分为4个等级，分别为 $\mathrm{S}1\sim \mathrm{S}4$ ，其中编号越大，休眠的程度越深；G2状态为SoftOff状态，该状态与S5状态对应，表示当前处理器处于软下电状态，此时处理器除了一些最基本模块，如EC、ICH中的部分逻辑和Wake-on-LAN机制仍然保持电源供应之外，其他所有模块均不供电；G3状态为MechanicalOff状态，此时处理器处于完全断电状态，全部模块均不上电。

下文仅讨论x86处理器处于G0和G1状态时，如何处理电源按钮事件。在x86处理器中，处理电源按钮事件的解释程序在DSDT表中，如源代码 $14 - 14^{\text{念}}$ 所示。

源代码14-14 与电源按钮事件相关的ASL程序  
```txt
// Define a control method power button  
Device(\_SB.PWRB) {  
    Name(_HID, EISAID("PNPOCOC"))  
    Name(_PRW, Package() {0, 0x4})  
    OperationRegion(\PHO, SystemIO, 0x200, 0x1)  
    Field(\PHO, ByteAcc, NoLock, WriteAsZeros) {  
        PBP, 1, // sleep/off request  
        PBW, 1 // wakeup request  
    }  
} // end of power button device object  
Scope(\_GPE) { // Root level event handlers  
    Method(_L00) { // uses bit 0 of GPO_STS register  
        If (\PBP) {  
            Store(One, \PBP) // clear power button status  
            Notify(\_SB.PWRB, 0x80) // Notify OS of event  
        }  
    If (\PBW) {  
        Store(One, \PBW)  
        Notify(\_SB.PWRB, 0x2)  
    }  
} // end of _L00 handler  
} // end of \_GPE scope 
```

这段程序的说明如下：

\- 创建一个“PWRB”设备，其标识(\_HID)为“PNP0C0C”，即PWRB设备与电源按钮对应。在ACPI中，每一个设备使用的标识不相同。

- 将\_PRW $^{\ominus}$ 定义为 Package() $\{0, 0x4\}$ 。其含义为在处理器处于 S1 \~ S4 状态时，电源按钮可以将处理器系统唤醒，而且此时使用 GPEx\_STS 寄存器的第 0 位作为唤醒状态位。处理器在即将进入休眠模式时，需要检查对应设备的\_PRW 参数，并保证处理器系统进入的休眠等级不大于设备在\_PRW 的定义，同时需要保证 GPEx\_EN 寄存器的对应位是有效的，否则处理器进入休眠模式时，不能被这个按键事件唤醒。对于本节所提供的实例，如果处理器进入的休眠等级大于 S4，或者在进入休眠状态之前 GPEx\_EN 寄存器的第 0 位没有使能时，用户将不能使用“ACPI 机制提供的电源按钮事件”激活处理器系统。  
- 声明一个PHO变量，使用的I/O端口地址为 $0\mathrm{x}200$ ，这个I/O端口的第0位和第1位分别与PBP和PBW位对应。其中PBP位为1表示处理器系统处于S0状态时电源按钮被按下，此时处理器需要进入休眠状态；PBW位为1表示处理器系统处于S1～S4状态时，电源按钮被按下，此时处理器需要被唤醒。  
- 从 Scope(\_GPE)开始的这段程序描述对电源按钮事件的处理过程。当 GPEx\_STS 寄存器的第 0 位为 1 时，将进一步检查 PBP 和 PBW 位。  
- 如果PBP位为1，则首先向PBP位写1，清除这个状态位，之后通知OSPM当前电源按钮对应的回调号为 $0 \times 80^{\ominus}$ 。回调号为 $0 \times 80$ 表示在处理器处于S0状态时，电源按钮被按下。  
- 如果 PBW 位为 1, 则首先向 PBW 位写 1, 清除这个状态位, 之后通知 OSPM 当前电源按钮对应的回调号为 $0 \times 02$ 。回调号为 $0 \times 02$ 表示设备发出了一个唤醒信号。

由以上描述，可以发现在源代码14-14中，除了定义了一个PWRB设备之外，还使用ASL语言简单描述了当有PBW或者PBP事件发生时，处理器的执行操作。对于一个具体的操作系统，如Linux，需要将PBW/PBP事件和处理这些事件的执行操作联系在一起。

当电源按钮被按下时，如果GPEx\_EN寄存器的相应位被使能，则处理器的GPIO接口将置GPEx\_STS寄存器的对应位为1，同时向处理器提交SCI中断请求。Linux系统首先需要提供处理这个SCI中断请求的中断服务例程，然后进一步处理这些SCI中断请求。

在Linux系统中，这个中断服务例程为acpi\_ev\_sc\_i\_xrupthandler函数，该函数即为SCI中断服务例程。SCI中断服务例程具有3个输入参数，如下所示。

- gsi 参数为 acpi\_gbl\_FADT. sci\_interrupt，缺省值为 0x09。  
- handler 参数为 acpi\_ev\_sci\_xrupthandler。  
- context 参数为 acpi\_gbl\_gpe\_xrupt\_list\_head。

Linux 系统进行初始化，该中断服务例程由 acpi\_early\_init 函数调用 acpi\_enable\_subsystem 函数挂接到 Linux 系统的中断处理服务主程序 (do\_IRQ 函数) 中，acpi\_ev\_sci\_xrupt Handler 函数的详细说明在 ./drivers/acpi/acpica/evsci.c 文件中。

acpi\_enable\_subsystem函数的详细实现在 ./drivers/acpi/acpica/utxface.c 文件中，该函数将调用 acpi\_ev\_install\_xrupthandlers $\rightarrow$ acpi\_ev\_install\_scihandler 函数注册 SCI 中断服务例程。acpi\_ev\_install\_scihandler 函数如源代码 14-15 所示。

源代码14-15 acpi\_ev\_install\_scihandler函数  
```c
u32 acpi_ev_install_scihandler(void)  
{  
    u32 status = AE_OK;  
ACPI_FUNCTION_TRACE(ev_install_scihandler);  
status =  
    acpi_os_install_interrupt handler((u32) acpi_gbl_FADT. sci_interrupt,  
        acpi_ev_sci_xrupt_handler,  
        acpi_gbl_gpe_xrupt_list_head));  
return ACPI_STATUS(status);  
} 
```

acpi\_ev\_install\_scihandler函数将调用acpi\_os\_install\_interrupt handler函数，并将acpi\_gbl\_FADT. sci\_interrupt、acpi\_ev\_scicxrupthandler和acpi\_gbl\_gpe\_xrupt\_list\_head参数传递给该函数。acpi\_gbl\_FADT. sci\_interrupt为SCI中断使用的irq号，acpi\_ev\_scicxrupthandler即为SCI中断处理函数，acpi\_gbl\_gpe\_xrupt\_list\_head为SCI中断处理函数使用的入口参数。

acpi\_os\_install\_interrupt\_handler 函数的实现如源代码 14-16 所示。

源代码14-16 acpi\_os\_install\_interrupt\_handler函数  
acpi_status   
acpi_os_install_interrupthandler(u32 gsi, acpi_osdhandler handler, void \*context)   
{ unsigned intirq;   
acpi_irq.stats_init();   
\*/ \* Ignore the GSI from the core, and use the value in our copy of the \* FADT. It may not be the same if an interrupt source override exists \* for the SCI. \*/ gsi $=$ acpi_gbl_FADT. sci_interrupt; if (acpi_gsi_to_irq(gsi,&irq） $<  0$ ）{ printf(KERN_ERRPREFIX"SCI（ACPI GSI % d）not registered\n",gsi); return AE_OK;   
} acpi_irqhandler $=$ handler; acpi_irq_context $=$ context; if(request_irqirq，acpi_irq，IRQF_SHARED，"acpi"，acpi_irq)) { printf(KERN_ERRPREFIX"SCI（IRQ%d）allocation failed\n",irq）; return AE_NOT_ACQUIREDC

} acpi_irq_irq $\equiv$ irq; returnAE_OK;

这段程序首先调用acpi\_irq\_stats\_init函数建立sysfs中的kobject，之后从FADT中获得ACPI使用的中断向量，在绝大多数x86处理器系统中，SCI中断使用的irq号为0x9。之后这段程序调用request\_irq，将acpi\_irq函数与irq号0x9联系在一起。之后Linux系统将使用acpi\_irq函数处理SCI中断请求，该函数的实现如源代码14-17所示。

# 源代码14-17 acpi\_irq函数

staticirqreturn_tacpi_irq(intirq,void\*dev_id)   
{ u32handled; handled $=$ (\*acpi_irqhandler）（acpi_irq_context）; if（handled）{ acpi_irq_handled $+ +$ ； returnIRQ_HANDLED; }else{ acpi_irq_not_handled $+ +$ ； returnIRQ_NON; }

acpi\_irq函数的主要作用是执行(\*acpi\_irqhandler)(acpi\_irq\_context)函数，并检查执行结果是否正确。acpi\_irqhandler函数指针在acpi\_os\_install\_interrupthandler函数中被赋值为acpi\_ev\_sci\_xrupthandler函数。因此在Linux ACPI的实现中，acpi\_ev\_sci\_xrupthandler函数为真正的SCI中断服务例程，该函数在 ./drivers/acpi/acpica/evsci.c 文件中，如源代码14-18所示。

# 源代码14-18 acpi\_ev\_sci\_xrupthandler函数

```txt
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*  
\*FUNCTION: acpi_ev_sci_xrupthandler  
\*PARAMETERS: Context - Calling Context  
\*RETURN: Status code indicates whether interrupt was handled.  
\*DESCRIPTION: Interrupt handler that will figure out what function or 
```

\* control method to call to deal with a SCI.   
\*   
\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*   
static u32 ACPI_SYSTEM_XFACE acpi_ev专科xrupt_handler(void \*context)   
{ struct acpi_gpe_xrupt_info \* gpe_xrupt_list $=$ context; u32 interrupt_handle $=$ ACPI_INTERRUPT_NOT_HANDLED; ACPI_FUNCTION_TRACE(ev专科xrupthandler); /\* \* We are guaranteed by the ACPI CA initialization/shutdown code that \* if this interrupt handler is installed, ACPI is enabled. \*/ / \* \* Fixed Events: \* Check for and dispatch any Fixed Events that have occurred \*/ interrupt_handle | = acpi_ev_fixied_eventdetect(); /\* \* General Purpose Events: \* Check for and dispatch any GPEs that have occurred \*/ interrupt_handle | = acpi_ev_gpedetect(gpe_xrupt_list) ; return_UID32(interrupt_handle);

该函数首先调用acpi\_ev\_fix\_eventdetect函数检查PM寄存器组，判断是否存在PM事件，之后调用acpi\_ev\_gpedetect函数检查是否存在GPE事件。上文中描述的PBW和PBP事件由acpi\_ev\_gpedetect函数处理。

acpi\_ev\_gpedetect函数的执行逻辑较为简单，本节不再列出该函数的源代码，该函数在./drivers/acpi/acpica/evgpe.c文件中，属于ACPICA提供的Event Management接口函数。该函数首先获得一个自旋锁acpi\_gbl\_gpe\_lock，之后检查GPEx\_STS寄存器和GPEx\_EN寄存器以确定处理器系统中存在的GPE事件，然后调用acpi\_ev\_gpe\_dispatch函数执行源代码14-14中Method(\_L00)之后的程序。

acpi\_ev\_gpe\_dispatch函数在执行源代码14-14中的ASL程序时，采用解释执行的方法。而解释执行相比编译执行而言，执行效率较低，为此该函数调用acpi\_os\_execute函数，使用Linux系统提供的Work Queue机制，脱离中断处理程序的上下文环境，“异步”地分析并解释执行这些ASL程序。

在Linux系统中，与ACPI机制相关的程序虽然数量众多，处理的事务也较多，但是其逻辑结构较为简单，本章对此不做进一步分析和说明。

