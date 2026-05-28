---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "12"
section: "12.2.3 Capric卡的中断请求"
part: 3
tags: [pci, pci-express, computer-architecture]
---
# 12.2.3 Capric卡的中断请求

Capric卡支持两种中断请求方式，一种是Legacy INTx方式，另一种是MSI中断方式。Capric卡需要向RC发送两个Legacy INTx中断消息，一个是Assert INTx，另一个是Deassert INTx，以实现Legacy INTx中断方式。第6.3.4节详细介绍了这种中断请求方式。这种中断请求方式虽然使用了INTx消息，但是其原理与电平触发方式类似，而MSI中断方式的工作原理与边沿触发方式类似。因此系统软件对Capric卡的这两种中断请求的处理并不相同。

在第10.3节中，我们曾详细讨论了电平触发与边沿触发的区别。其中采用电平触发不会丢失中断请求，而采用边沿触发将会丢失中断请求。Capric卡可以保证即便使用了MSI中断机制，也不会丢失中断请求。

MSI中断机制使用存储器写TLP实现，这个存储器写TLP的目的地址为MSI Capability结构中的Message Address字段，而数据为Message Data寄存器中的值。Message Address字段和Message Data字段由系统软件在初始化时填写。在不同的处理器体系结构中，系统软件填写的这两个字段的数据并不相同，详见第10.2节和第10.3节。

LogiCORE 内部实现了 MSI 中断机制，Capric 卡仅需一些简单的组合逻辑即可实现 MSI 中断机制。Capric 卡需要根据 INT\_REG 寄存器的信息，决定如何发送中断请求，这部分中断逻辑的实现较为简单，本节对此不做进一步说明。

# 12.3 基于 PCIe 总线的设备驱动

本节简要介绍Capric卡在Linux系统中使用的Char类型设备驱动程序。为便于读者理解Linux系统PCIe总线驱动程序的实现构架，本节将详细介绍Capric卡的初始化、DMA读、

DMA写、中断服务例程和关闭过程，但是并不会过多介绍和Linux系统相关的知识，而是重点介绍系统软件如何管理和配置PCIe设备。

# 12.3.1 Capric卡驱动程序的加载与卸载

在Linux系统中，Capric卡驱动程序的加载与卸载的过程如源代码12-1所示。这部分程序并不会直接操作PCIe设备，而是通过pci register driver函数向内核注册一个pci Driver结构，即capric\_drv，并由capricprobe函数完成Capric卡的初始化。

源代码12-1 Capric卡驱动程序的加载与卸载  
```c
static struct pcie_device_id capric_ids[] = {
    {PCIE_DEVICE(PCIE_VENDOR_ID_XILINX, PCIE_DEVICE_ID_EPPIPE),},
    {0,}
};
...
static struct pcie_driver capric_drv = {
    .name = DEV_NAME,
    .id_table = capric.ids,
    .probe = capricProbe,
    .remove = capric_remove,
}; 
```

在上述源代码中，pci register driver 函数的主要作用是将 capric\_drv 结构与 PCI 设备的pci\_dev 结构进行绑定，并在初始化时执行 capricprobe 函数，而在结束时执行 capric\_re-

move函数。这段源代码的主要作用是将Capric卡驱动程序使用的“软件结构pci\_driver”与“硬件结构pci\_dev”建立联系。本文并不会深入分析pci register driver和pci\_unregister Driver函数的实现细节，而仅介绍该函数的执行顺序。对Linux系统有一定经验的读者，可以从中获得必要的知识。

pci register driver 函数首先调用 \_\_pci register driver $\rightarrow$ driver register $\rightarrow$ bus\_add Driver 函数。bus\_add Driver 函数进行一些必要的初始化操作后，调用 driver.attach $\rightarrow$ bus\_for\_each\_dev 函数查找 Capric 卡的pci\_dev 结构。

在Linux系统中，bus\_for\_each\_dev函数是一个重要的函数，该函数将遍历Capric卡所在PCI总线树上的所有pci\_dev结构，并依次判断pci\_dev结构中的Device ID、Vendor ID等信息是否与capric\_ids结构中包含的对应信息相同，如果相同则调用capricprobe函数。bus\_for\_each\_dev函数调用driver Attached函数实现该过程。

driver Attach 函数调用 $\mathrm{drv} \rightarrow \mathrm{bus} \rightarrow \mathrm{match}$ 函数（即pci BUS match函数），而pci BUS match函数将继续调用pci match device $\rightarrow$ pcimatch\_id函数，判断capric\_ids所包含的内容是否在当前PCI总线树的pci\_dev中出现。如果出现，将capric\_drv结构与实际的PCI设备进行绑定。之后继续调用driver Probe\_device $\rightarrow$ really Probe函数。

reallyprobe函数将调用 $\mathrm{dev}\rightarrow \mathrm{bus}\rightarrow$ probe函数（即pci\_device Probe函数），pci\_deviceProbe函数将调用\_pci\_device Probe $\rightarrow$ pci\_call Probe $\rightarrow$ local\_pci Probe函数，并最终调用Capric卡的probe函数，即capric Probe函数。

Capric卡的卸载过程是加载的逆过程，其调用顺序为pci\_unregister\_driver函数、driver\_unregister函数、bus\_remove\_driver函数、driver\_detach函数和\_\_device\_release\_driver函数，并最终调用capric\_remove函数。

对于Capric卡，初始化与结束操作是在capricprobe和capric\_remove函数中完成的。在capric\_ids结构中使用的id号，是联系Capric卡的pci\_driver结构和pci\_dev结构的桥梁。在该结构中的PCI\_VENDOR\_ID\_Xilinx和PCI\_DEVICE\_ID\_EPPIPE即为Capric卡的VendorID和Device ID，分别为0x10EE和0x0007。

# 12.3.2 Capric卡的初始化与关闭

Capric卡的probe函数完成硬件初始化和一些Linux系统相关的初始化操作。当LinuxPCI在当前PCI总线树中，发现Capric卡后，由local\_pciprobe调用capricprobe函数，该函数具有两个入口参数pci\_dev和ids，其执行过程如源代码12-2\~5所示。

源代码12-2 Capric卡的硬件初始化片段1   
```c
static struct capric_private \*adapter;   
...   
static int capricprobe(structpci_dev\*pci_dev, conststructpci_device_id\*ids)   
{ int result; resource_size_t base_addr; 
```

unsigned long length;  
adapter $=$ kmalloc(sizeof(struct capric_private),GFP_KERNEL);if(unlikely(!adapter))return-ENOMEM;adapter->pci_dev $\equiv$ pciev;  
result $\equiv$ pcie_enable_device(pci_dev);if(unlikely(result))goto free_adapter;

首先 capric Probe 函数从 local\_pci Probe 函数中获得 Capric 卡对应的pci\_dev 描述符，在 Linux 系统中每一个 PCI/PCIe 设备都与唯一的pci\_dev 描述符对应。pci\_dev 描述符包含 PCIe 设备的全部信息，该结构较为简单，在 ./include/linux/pci.h 文件中定义，本节并不对该结构进行详细介绍。

这段函数首先为全局指针adapter分配空间，在全局指针adapter中记录了一些Capric卡需要使用的私有参数，包括Capric卡使用的pci\_dev。这段程序为adapter分配完内存空间后，将调用pci\_enable\_device函数使能PCIe设备。pci\_enable\_device函数的主要作用是修改Capric卡PCI配置空间Command寄存器的I/O Space位和Memory Space位。

pci\_enable\_device 函数最终调用 pcie\_enable-resources 函数，并由 pcie\_enable-resources 函数扫描 Capric 卡的 BAR0 \~ 5 空间，如果这些 BAR0 \~ 5 空间用到了 I/O 或者 Memory 空间，则将 I/O Space 位和 Memory Space 位置 1。pci\_enable\_device 函数最后调用 pcibios\_enable\_irq 函数分配 PCI 设备使用的中断向量号。此后处理器可以使用存储器或者 I/O 指令与 Capric 卡通信。pci\_enable\_device 函数还有一个用途是置 PCI 设备的 D-State 为 D0。

Linux PowerPC与Linux x86在此处的处理基本类似。只是在PowerPC处理器系统中，某些PCI设备支持存储器写并无效周期，此时pci\_enable\_device函数还需要使能PCI设备的Memory Write and Invalidate位，同时需要填写配置空间的Cache Line Size寄存器。

Linux x86 的 MSI 中断机制处理过程与 Linux PowerPC 也不尽相同。在 Linux x86 中，一个 PCIe 设备使能或者不使能 MSI 中断机制时，其pci\_dev $\rightarrow$ irq 参数并不相同。

如果其他设备驱动程序再次调用pci\_enable\_device函数使能该设备时，该函数仅增加pci\_dev的引用计数，并不会重新使能该PCI设备。与此对应pci\_disable\_device函数只有在pci\_dev的引用计数为0之后，才能关闭pci\_dev结构。当两个以上的设备驱动程序操纵相同的硬件时，会出现这种情况。

源代码12-3 Capric卡的硬件初始化片段2   
pci_set/master(pci_dev);   
// pcitry_set_mwi(pci_dev)； result $=$ pcitype_set_DMA_mask(pci_dev，DMA_MASK）; if(unlikely(result)){ PDEBUG("can not set dma mask...\\n"); goto disable_pci_dev;

pci\_set/master 函数将 Capric 卡 PCI 配置空间 Command 寄存器的 Bus Master 位置 1，表示 Capric 卡可以作为 PCI 总线的主设备。Capric 卡是基于 PCIe 总线的设备，而 PCIe 总线不支持存储器写并无效操作，因此这段程序不需要使用 pc try\_set\_mwi 函数设置 Command 寄存器的 Memory Write and Invalidate 位。

pci\_set\_dma\_mask 函数设置 PCIe 设备使用的 DMA 掩码。Capric 卡对一段内存进行 DMA 操作时，需要使用这段内存在 PCI 总线域的物理地址pci\_address，如果这段内存在存储器域的物理地址 physical\_address & DMA\_MASK = physical\_address 时，表示 Capric 卡可以对这段内存进行 DMA 操作。

x86处理器可以使用pci dmasupported函数获得最合适的DMA\_MASK参数；而在PowerPC处理器中允许PCIe设备访问的主存储器地址范围在Inbound寄存器组中定义，只有在Inbound寄存器窗口中映射的物理地址才能被PCIe设备访问，有关Inbound寄存器组的详细说明见第2.2节。在x86和PowerPC处理器中，pci\_set dma\_mask函数的实现方法不同。

源代码12-4 Capric卡的硬件初始化片段3   
result $=$ pcie_request_regions(pci_dev,DEV_NAME);   
if(unlikely(result)) goto disable_pci_dev;   
base_addr $\equiv$ pcie_resource_start(pci_dev,0);   
if(unlikely(!base_addr)){ result $\equiv$ -EIO; PDEBUG("no MMIO...\\n"); goto release_regions;   
}   
if(unlikely((length $=$ pcieResource_len(pci_dev,0)<BARO_BYTE_SIZE)) { result $\equiv$ -EIO; PDEBUG("MMIO is too small...\\n"); goto release_regions;

}   
adapter->pci_bar0 $=$ ioremap(base_addr,length); if(unlikely(!adapter->pci_barO)) $\}$ result $= -\mathrm{EIO}$ PDEBUG("cannot map MMIO..\\n"); goto releaseRegions;

这段源代码调用pci\_request\_regions函数使DEV\_NAME对应的驱动程序成为pci\_dev存储器资源的拥有者。在Linux系统中所有存储器映射的寄存器和I/O映射的寄存器都使用ioreources进行管理。每一组存储器空间都对应一个resource结构，pci\_request\_regions函数经过一系列函数调用，最终调用\_\_request\_region函数，将Capric卡的BARO空间使用的resource结构，其name参数设置为DEV\_NAME，其flags参数设置为IORESOURCE\_BUSY。

因此一个PCIe设备的驱动程序使用pci\_request\_regions函数对pci\_dev结构进行设置，将其使用的flags位设置为IORESOURCE\_BUSY之后，其他驱动程序不能再次设置这个flags位。在实际应用中可能存在一个硬件设备对应两个设备驱动程序的情况，此时只有一个设备驱动程序可以使用pci\_request\_regions函数对资源进行管理。

pciResource\_start函数从resource结构获得BAR0空间的基地址，该地址为存储器域的物理地址，而不是PCI总线域的物理地址，该值与直接使用pci\_read\_config\_word函数读取PCI设备BAR寄存器所获得的值即便相等，也没有本质的联系，因为从resource结构获得的是该设备BAR寄存器在存储器域的物理地址，而使用pci\_read\_config\_word函数获得的是PCI总线域的物理地址。在Linux驱动程序中，需要使用的是存储器域的物理地址。

本书从始至终一直强调PCI总线域物理地址和存储器域物理地址的区别，希望读者真正理解这两个地址的区别。在x86处理器中，并没有显式区分这两个物理地址的区别，这在某种程度上误导了部分系统程序员。

在这段程序的最后，使用 ioremap 函数将存储器域的物理地址映射成为 Linux 系统中的虚拟地址，之后 Capric 卡的设备驱动程序可以使用 adapter $\rightarrow$ pci\_bar0 指针访问 Capric 卡中存储器映射的寄存器。

除了 ioremap 函数之外，Linux 系统还提供了 ioremap\_nocache 和 ioremap\_cache 函数用于存储器域虚实地址的转换。其中 ioremap\_nocache 函数将存储器域的物理地址空间映射到一段“不可 Cache”的虚拟地址空间，在绝大多数体系结构中，这个函数与 ioremap 函数的实现一致，因为绝大多数外部设备都需要映射到“不可 Cache”的虚拟地址空间中。由于历史原因， $99\%$ 以上的程序员已经使用了 ioremap 函数而不是 ioremap\_nocache 函数进行总线地址到虚拟地址的转换，因此 ioremap\_nocache 函数显得冗余。

值得注意的是，对于PCIe设备的Linux驱动程序，ioremap函数使用的物理地址必须从pciResource\_start函数获得，而不能使用“通过pci\_read\_config\_xxxx函数”获得的BARx基地址，因为PCIe设备的BARx基地址空间属于PCI总线域，而不是存储器域。

某些设备还可能使用“可Cache的”虚拟地址空间，此时需要使用ioremap\_cache函数

将存储器域地址转换为虚拟地址，目前为止，仅有极少数外部设备需要使用这一函数，如PCIe设备中的ROM空间。

Linux 系统还提供了一个 ioremap\_flags 函数，使用这个函数可以自定义存储器域地址转换到哪种类型的虚拟地址，该函数提供了一个入口参数 flags，系统程序员可以使用该参数确定所申请虚拟地址空间的类型。对于 x86 处理器，flags 参数的定义见 ./arch/x86/include/asm/pgtable.h 文件；而对于 PowerPC 处理器，flags 参数的定义见 ./arch/powerpc/include/asm/pgtable-ppc32.h 文件。

源代码12-5 Capric卡的硬件初始化片段4   
result $=$ register_chrdev(test_drimajor，DEV_NAME,&capric_fops）;  
result $=$ pci_enable_msi(pci_dev);if(unlikely(result)){PDEBUG("can not enable msi...\\n");goto chrdev_unregister;  
}  
result $=$ request_irq(pci_dev->irq，capric_interrupt，0，DEV_NAME，NULL);if(unlikely(result)){PDEBUG("request interrupt failed...\\n");goto err_disable_msi;  
}  
capric_reset();return 0;  
}  
static const struct file_operations capric_char_fops $\equiv$ {.owner $\equiv$ THISMODULE, .ioct1 $\equiv$ capric_iocl, .open $\equiv$ capric_open, .release $\equiv$ capric_release, .write $\equiv$ capric_write, .read $\equiv$ capric_read,

这段源代码首先使用register\_chrdev函数注册一个char类型的设备驱动程序，包括打开、关闭、读写操作和ioct1函数。之后该程序调用pci\_enable\_msi函数使能Capric卡的MSI

中断请求机制，该函数将在第12.3.5节中详细介绍。

随后这段程序使用request\_irq函数注册Capric卡使用的中断服务例程capric\_interrupt，  
并使用pci\_dev $\rightarrow$ irq作为这个函数的irq入口参数。Capric卡的pci\_dev $\rightarrow$ irq参数在Linux系统对PCI总线进行初始化时分配，在x86处理器中，如果一个PCIe设备支持MSI中断，驱动程序执行完毕pci\_enable\_msi函数后，pci\_dev $\rightarrow$ irq参数还会发生变化。因此request\_irq函数必须在pci\_enable\_msi函数之后运行。

这段源代码的最后将调用 capric\_reset 函数，对 Capric 卡进行硬件初始化，该函数执行的操作见第 12.1.2 节。

# 12.3.3 Capric卡的DMA读写操作

Capric卡的DMA读/写过程与capric\_write/capric\_read函数对应。

# 1. DMA写的操作流程

Capric卡的数据传送方法较为简单，其DMA读写的硬件操作流程如第12.1.3节所示。DMA写的实现过程与capric\_read函数对应，如源代码 $12 - 6\sim 7$ 所示。

# 源代码12-6 Capric卡的DMA写片段1

static ssize_t capric_read(struct file \* file, char \_\_user \*buff,size_t count，loff_t \*f_pos)   
{ int err $=$ -EINVALID; void \*virt_addr $=$ NULL; dma_addr_t dma_write_addr;   
... virt_addr $=$ kmalloc(count,GFP_KERNEL); if((unlikely(!virt_addr)){ PDEBUG("can not alloc rx memory you want...\\n"); return -EIO; } dma_write_addr $=$ pcmap_single(adapter->pci_dev, virt_addr,count,PCI_DMA_FROMDEVICE); if((unlikely(pci_dma Mapping_error(adapter->pci_dev,dma_write_addr))) { PDEBUG("RX DMA MAPPING FAIL...\\n"); goto err_kmalloc;

这段源代码首先对 count 字段进行检查，因为 Capric 卡规定一次 DMA 操作所传递的数据不超过 2KB，之后使用 kmalloc 函数分配 DMA 写使用的数据缓存。kmalloc 函数所能分配的内存大小受限于 Linux 系统中的 SLAB/SLUB 内存分配器，在系统内存紧张时，有可能失

败，因此在此必须进行参数检查。值得注意的是，在一个实际驱动程序中，很少在读写服务例程中使用 kmalloc 函数申请内存，然后使用 kfree 函数释放这段内存，因为这样做容易产生内存碎片。而且 kmalloc 函数的执行时间也相对较长，影响数据传送的效率。

随后这段代码调用pci\_map\_single函数将存储器域的虚拟地址virt\_addr转换为PCI总线域的物理地址dma\_write\_addr，供Capric卡的DMA控制器使用。Linux系统提供了一组将虚拟地址转换为设备域物理地址的方法，参见第12.3.5节。

源代码12-7 Capric卡的DMA写片段2   
ifdef CONFIG_NOT_COHERENT_CACHE $\text{念}$ dmaSync_single(adapter->pci_dev, virt_addr，count，PCI_DMA_FROMDEVICE);   
endif   
capric_w32(dma_write_addr，WR_DMA_ADR）;   
capric_w32(count，WR_DMA_SIZE）;   
capric_w32(MWR_START,DCSR2）;   
if(unlikely(interruptible_sleep_on(adapter->dma_write_wait))) goto err_pci_map;   
... if((unlikely.copy_to_user(buffer,virt_addr,count))） goto err_pci_map;   
pci_unmap_single(adapter->pci_dev，virt_addr，count，PCI_DMA_FROMDEVICE); kfree(virt_addr）; return count;   
}

如果当前 DMA 写操作不与 Cache 进行一致性操作，将首先执行 dma\_sync\_single 函数进行存储器与 Cache 的同步操作，该函数的详细说明见第 12.3.6 节。随后这段程序使用 capric\_w32 函数执行第 12.1.3 节中要求的寄存器操作，之后可以使用轮询方式，或者使用中断方式唤醒这个 DMA 写进程。当进程被唤醒后，表示 DMA 写操作已经完成，此时这段程序使用 copy\_to\_user 函数将数据复制到用户空间。

值得注意的是，这段代码使用了interruptible\_sleep\_on函数将当前进程休眠，而在中断处理程序中使用wake\_up\_interruptible函数将其唤醒。这是一种非常糟糕的实现方式，而且存在相当大的隐患。

interruptible\_sleep\_on 函数的主要工作是将当前进程放入等待队列中睡眠，目前在 Linux 系统中，该函数已经逐步被 wait\_event\_interruptible 函数取代，但这并不是问题的关键。在源

代码12-7中，即便使用wait\_event\_interruptible函数也存在同样的问题。

因为interruptible\_sleep\_on函数的执行路径较长，很可能在当前进程还没有被该函数放入adapter->dma\_write\_wait队列时，处理器已经执行中断服务例程，打断interruptible\_sleep\_on函数，并执行wake\_up\_interruptible函数。Capric中断服务例程的详细说明见第12.3.4节。

wake\_up\_interruptible 函数将唤醒在 adapter -> dma\_write\_wait 队列中休眠的进程，而此时当前进程可能还没有被加入到等待队列中。当该函数执行完毕退出中断处理例程之后，处理器继续执行 capric\_read 函数，并完成 interruptible\_sleep\_on 函数的执行，将自身加入到等待队列中睡眠。此时由于中断服务例程已经被提前执行，因此当前进程不会被 wake\_up\_interruptible 函数唤醒，从而造成死锁。

程序员可以使用 DCSR1 寄存器的 msk 和 pending 位解决这个死锁问题。采用这种方法时，设备驱动程序需要保证当前进程进入等待队列后，再允许 Capric 卡提交中断请求。但是这种方法将产生较长的中断延时，从而极大影响 Capric 卡的 DMA 读写效率。

程序员还可以使用interruptible\_sleep\_on\_timeout或者wait\_event\_interruptible\_timeout函数进行超时处理，使用该方法也可以解决上述死锁问题。

以上这两种方法都不是完美的解决方案，因为产生这种死锁的主要原因是Capric卡的逻辑设计并不合理。Capric卡使用的数据传送模型较为简单，系统程序员很难基于此模型写出高效的驱动程序。

# 2. DMA 读的操作流程

Capric卡DMA读使用的函数与DMA写的类似，其流程如源代码12-8所示。

源代码12-8 Capric卡的DMA读   
static ssize_t capric_write(struct file \*file, const char \*user \*buff,size_t count，loff_t \*f_pos)   
{ int err $=$ -EINVALID; void \*virt_addr $=$ NULL; dma_addr_t dma_write_addr; virt_addr $=$ kmalloc(count,GFP_KERNEL); if((unlikely(copy_from_user(virt_addr，buff,count))) return err; dma_write_addr $=$ pcimap_single(adapter->pci_dev,virt_addr,count,PCI_DMA_TODEVICE);   
... #ifdef CONFIG_NOT_COHERENT_CACHE dma-sync_single(adapter->pci_dev,virt_addr,count,PCI_DMA_TODEVICE);

endif  
capric_w32（dma_write_addr，RD_DMA_ADR）；capric_w32（count，RD_DMA_SIZE）；capric_w32(MRD_START，DCSR2）；adapter->dma_read_done $= 0$ ·if(unlikely(interruptible_sleep_on(adapter->dma_read_wait)))goto err_pci_map;  
.…pci_unmap_single(adapter->pci_dev，dma_write_addr，count，PCI_DMA_TODEVICE)；kfree(virt_addr)；return count;  
1

读者如果正确理解了上文关于DMA写的执行过程，DMA读的执行过程并不难理解。Capric卡DMA读的硬件操作流程如第12.1.4节所示。上述源代码使用capric\_w32函数完成硬件寄存器的填写，然后可以使用轮询或者中断方式确定DMA读是否已经完成。在DMA读完成之后该例程将释放使用的内存资源后返回。与DMA写的操作流程类似，这段程序依然存在隐患。

# 12.3.4 Capric卡的中断处理

Capric 卡一共需要处理三种中断请求，分别为 DMA 写完成、DMA 读完成和错误中断请求。Capric 卡使用了一个中断服务例程处理这些中断请求，其执行流程如源代码 12-9 所示。

源代码12-9 Capric卡的中断服务例程  
staticirqreturn_tcapric_interrupt(intirq,void\*dev)   
{ unsigned int statue; statue $=$ capric_r32(INT_REG); if(!（ statue&INT_ASSERT））{ PDEBUG("irq_nonel..n"）; return IRQ_NONE; } if statue&INTvosrtERT_R）{

```c
capric_w32（statue，INT_REG）； wake_up_interruptible(&adapter->dma_read_wait)；   
} else{ capric_w32（statue，INT_REG）; wake_up_interruptible(&adapter->dma_write_wait)； PDEBUG("irqhandled...\\n"); return IRQ_HANDLED;
```

在capric Probe函数中，capric\_interrupt中断服务例程被request\_irq函数注册到Linux系统的irq\_desc中断描述符表中，并与Linux系统的外部中断处理函数do\_IRQ挂接，当Capric卡通过MSI中断方式提交外部中断请求后，do\_IRQ函数将最终调用capric\_interrupt函数完成相应的中断处理。Capric卡处理中断请求的硬件操作流程如第12.1.5节所示。

目前Linux系统对MSI机制的支持并不理想，pci\_enable\_msi函数仅可以获得一个irq号，这为中断服务例程的设计带来了一定的困难。如果pci\_enable\_msi函数可以获得多个irq号，那么在capricesses函数中，可以使用多个中断服务程序，其中DMA写完成、读完成和错误处理分别使用三个中断服务例程，而不必使用capric\_r32函数读取INT\_REG寄存器。在Linux系统中，将pci\_enable\_msi函数改写为支持多个irq号并不困难。对于许多PCIe设备，这种改写是必须的，因为RC从PCIe设备中读取寄存器的代价是非常昂贵的。

# 12.3.5 存储器地址到PCI总线地址的转换

在Linux系统中，支持一系列API实现存储器地址到PCI总线地址的转换，这些API的详细定义见./Documentation/DMA-API.txt文件。本节仅以pci\_map\_single函数为例说明这种地址转换的工作原理。pci\_map\_single函数在./include/asm-generic/pci-dma-compat.h文件中，如源代码12-10所示。

源代码12-10pci\_map\_single函数  
```c
static inline dma_addr_t  
pci_map_single(struct pcie_dev *hwdev, void *ptr, size_t size, int direction)  
{ return dma_map_single(hwdev == NULL ? NULL : &hwdev->dev, ptr, size, (enum dma_data_direction) direction); } 
```

该函数共有4个输入参数，其中hwdev参数与PCI设备的pci\_dev对应，ptr参数对应存储器域的虚拟地址，size字段对应数据区域的大小。而direction参数与数据区域的使用方法

对应，PCI\_DMA\_NON 用于调试，较少使用；PCI\_DMA\_TODEVICE 表示这段数据的传递方向是从存储器到 PCI 设备；PCI\_DMA\_FROMDEVICE 表示这段数据的传递方向是从 PCI 设备到存储器；PCI\_DMA\_BIDIRECTIONAL 表示方向未知。该函数的返回值为 dma\_addr，即 PCI 总线域的物理地址。

pci\_map\_single 函数的主要作用是通过 ptr 参数，获得与之对应的 dma\_addr，即进行存储器域虚拟地址到 PCI 总线域物理地址的转换。值得注意的是存储器域物理地址与 PCI 总线域物理地址的区别。

在Linux系统中，使用virt\_to\_phys函数将存储器域的虚拟地址转换为存储器域的物理地址，但是通过该函数仅能获得存储器域的物理地址，因此该地址不能填写到PCI设备中进行DMA操作。值得注意的是，进行DMA操作的地址是由PCI设备使用的，而且这个地址只能是PCI总线域的物理地址，尽管在许多处理器中，virt\_to\_phys函数和pci\_map\_single函数的返回值相同。

不同的处理器使用不同的方式实现pci\_map\_single函数。起初在x86处理器中，存储器域物理地址到PCI总线域物理地址的转换非常简单，是直接相等的关系。但是x86处理器为了支持虚拟化技术，使用了VT-d/IOMMU $^{\text{©}}$ 技术，使得该函数的实现略微复杂。

同样是基于x86架构，AMD处理器使用的IOMMU技术与Intel有所区别，AMD的x86处理器使用./arch/x86/kernel/amd\_iommu.c文件中的map\_single函数，进行存储器域地址空间到PCI总线域地址空间的转换；而Intel的x86处理器使用./drivers/pci/intel-iommu.c文件中的intel\_map\_single函数实现存储器地址空间到PCI域地址空间的转换。IOMMU技术略微有些复杂，在第13.1节中将专门描述这部分内容。

在PowerPC处理器中，存在一组Inbound寄存器，通过该组寄存器可以将PCI总线地址转换为PowePC处理器规定的存储器地址，详见第2.2节。这组Inbound寄存器也可以看作一种IOMMU，只是该IOMMU机制仅支持段式映射而不支持页式映射。

Linux PowerPC 使用 dma\_direct\_map\_page 函数实现这个地址转换，该函数的定义详见 ./arch/powerpc/kernel/dma.c。在 Linux PowerPC 中，PCI 总线域的物理地址也与存储器域的物理地址相等。

Linux PowerPC 还需要设置 Inbound 寄存器组，这段代码在 ./arch/power/sysdev/fsl\_pci.c 文件的 setup\_pci\_atmu 函数中，如源代码 12-11 所示。目前这段代码对 Inbound 寄存器组的 Entry 2 进行设置，允许 PCIe 设备访问 $0 \sim 0 \times 7FFF - FFFF$ （2GB）这段存储器域物理地址空间，而且 PCI 总线地址与存储器地址一一对应而且相等。

源代码12-11 setup\_pci\_atmu函数

```c
static void __init setup_pci_atmu(struct pci_controller *hose, struct resource *rsrc)  
{  
    /* Setup 2G inbound Memory Window @ 1 */ 
```

```perl
out_be32(&pci->piw[2].pitar,0x00000000）; out_be32(&pci->piw[2].piwbar,0x00000000）; out_be32(&pci->piw[2].piwar，PIWAR_2G）; 
```

这段代码源于Linux 2.6.30，在这个版本中，PCIe设备不能访问PowerPC处理器2GB之上的物理内存。而在Linux 2.6.31.6中，该函数被大规模修改，以支持超过2GB的存储器系统，本节对Linux内核的这些改动不做进一步描述，对此有兴趣的读者可以参考Linux2.6.31.6内核中setup\_pci\_atmu函数的最新实现。

有些支持IOMMU机制的PowerPC处理器，如IBM的PowerPC处理器系列，可以使用dma\_iommu\_map\_page或者ibmebus\_map\_page函数实现pci\_map\_single函数，而cell处理器使用dma\_fixmap\_page函数实现该功能。pci\_map\_single函数函数在IBM的PowerPC处理器上已经移植完毕，但是Freescale除了P4080处理器之外，还没有支持IOMMU的处理器。目前对P4080处理器的支持并没有加入到Linux PowerPC中。

# 12.3.6 存储器与Cache的同步

Linux 系统还提供了一组 sync 函数，如 dma\_SYNC\_single、dma\_SYNC\_sg 等函数，这组 sync 函数的主要作用是为了支持“不进行 Cache 共享一致性”的 DMA 操作。

如果设备进行 DMA 操作时，不需要硬件进行 Cache 一致性操作，那么处理器在 DMA 操作之前，需要使用软件指令将操作的数据区域与 Cache 进行同步，之后进行 DMA 操作。PCIe 设备启动 DMA 请求时，如果其 TLP 头部 Attr 字段的 No Snoop Attribute 位为 $1^{\text{①}}$ 时，驱动程序也需要进行这种同步操作。在 PowerPC 处理器中，有一些非 PCIe 设备，如 QE（QUICC Engine）中的一些内嵌设备，这些设备可以通过设置 snoop 位决定在 DMA 传送过程中，是否需要硬件进行 Cache 一致性操作。

目前多数 RC 或者 HOST 主桥都可以通过总线监听，解决 PCI 设备进行 DMA 操作的 Cache 一致性问题。但是有些 RC，如 MPC8572 处理器的可以通过设置 Inbound 寄存器决定当前访问是否支持 Cache 一致性操作。

如果硬件不支持 Cache 共享一致性，那么 PCI 设备进行 DMA 操作时，必须使用软件指令维护存储器与 Cache 的同步，从而避免 Cache 与主存储器不一致的现象发生。

系统软件程序员使用软件指令维护 Cache 时，务必深入理解这些指令的特点和使用方法。在处理器系统的设计中，有两类错误最难被发现，一类是 Cache 与存储器系统的不一致，另一类是数据传送的序。

即使是对资深的系统程序员，也很难从这些错误表现形式中，发现是 Cache 不一致或者数据传送的序引发的系统错误。因此系统程序员需要重视在一个处理器系统中的 Cache 一致性（Cache Cohency）和数据完成性（Data Consistency）。

因此虽然对于多数PCI设备，Cache一致性可以由硬件保证，本节也必须讲述如何通过软件指令维护Cache的一致性。Linux系统使用dma-sync\_single函数维护Cache的一致性。dma-sync\_single函数的实现如源代码12-12所示。

源代码12-12 \_\_dma\_sync函数  
define dma_sync_single dma_sync_single_for_cpu   
static inline void   
dma_sync_single_for_cpu(struct device \*hwdev，dma_addr_t dma_handle, size_t size，enum dma_data_direction dir) struct dma_map ops \*ops $=$ get dmaOps(hwdev); BUG_ON(!valid dma_direction(dir)); if (ops->sync_single_for_cpu) ops->sync_single_for_cpu(hwdev，dma_handle,size，dir）; debug dmaSyncsingle_for_cpu(hwdev，dma_handle,size，dir）; flush_writeBuffers();

不同的处理器系统使用不同的 ops -> sync\_single\_for\_cpu 操作函数。值得注意的是，Linux x86 并没有实现 ops -> sync\_single\_for\_cpu 函数，因为使用软件指令维护 Cache 一致性的情况在 x86 处理器系统中并不多见。而 Linux PowerPC 使用 dma\_direct-sync\_single\_range 函数实现 ops -> sync\_single\_for\_cpu 函数，该函数最终将调用 \_\_dma\_sync 函数。这两个函数的实现如源代码 12-13 所示。

源代码12-13 \_\_dmaSync函数  
static inline void dma_direct_sync_single_range(struct device \*dev, dma_addr_t dma_handle，unsigned long offset,size_t size,enum dma_data_direction direction)   
{ __dma_sync bus_to_virtDMA_handle $^+$ offset）,size,direction）;   
1 $\begin{array}{rl} & {\mathrm{~ / ~*}}\\ & {\mathrm{~*~make~an~area~consistent.}}\\ & {\mathrm{~* / }}\\ & {\mathrm{void\_dma\_sync(void\*vaddr,size_t size,int~direction)}\\ & {\mathrm{~\{}}\\ & {\mathrm{~\text{unsigned~long~start} = (\text{unsigned~long})vaddr};}\\ & {\mathrm{~\text{unsigned~long~end} = \text{start} + \text{size};}} \end{array}$

```c
switch（direction）{   
case DMA_NONEBUG();   
case DMA_FROM_DEVICE: /\* \* invalidate only when cache-line aligned otherwise there is \* the potential for discarding uncommitted data from the cache \*/ if（（start&（L1_CACHEBytes-1)）||（size&（L1_CACHEBytes-1)）） flush_dcache_range(start，end); else invalidate_dcache_range(start，end); break;   
case DMA_TO_DEVICE:/\*writeback only\*/ clean_dcache_range(start，end); break;   
case DMA_BIDIRECTIONAL:/\*writeback and invalidate\*/ flush_dcache_range(start，end); break;   
}   
}   
EXPORT_SYMBOL(_dma_sync); 
```

在Linux PowerPC中，flush\_dcache\_range、invalidate\_dcache\_range和clean\_dcache\_range函数分别使用dcbf、dcbi和dcbst指令实现。dcbi/dcbf/dcbst指令的格式为dcbi/dcbf/dcbstrA，rB，如源代码12-14所示。

源代码12-14 dcbi/dcbf/dbst指令格式  
if rA $= 0$ then a<-640 else a<-rA EA<-32011(a+rB)32:63 InvalidateDataCacheBlock(EA) // dcbi FlushDataCacheBlock // dcbf StoreDataCacheBlock(EA) // dcbst

debf、dcbi和dcbst指令的详细说明如下。

- dcbi指令首先在Cache中检查EA。如果EA在Cache中命中，将直接将EA所对应的Cache行的状态改变为I，无论这个Cache行原来的状态是什么，都不将数据回写到存储器中。  
- dcbf指令首先在Cache中检查EA。如果EA在Cache中命中，则继续检查Cache的状态，如果为M，则将Cache行刷新到内存，然后 Invalidate该Cache行，将Cache行的

状态改变为I；否则直接 Invalidate该Cache行。

\- dcbst 指令首先在 Cache 中检查 EA。如果地址在 Cache 中命中，则继续检查 Cache 的状态，如果为 M，则将 Cache 行回写到内存，然后将 Cache 行的状态改变为 E；否则不做任何操作。

在x86处理器系统中，也存在类似的Cache指令。如INVD、WBINVD和CLFLUSH指令。其中INVD指令的作用是 Invalidate处理器中的内部Cache，并通过FSB总线周期 Invalidate外部Cache；而WBINVD指令是在 Invalidate内部和外部Cache之前，先将Cache中的数据回写，然后进行 Invalidate操作。

但是这两条指令都是针对整个 Cache，而不是针对某个 Cache 行。如果需要对 Cache 行进行刷新时，x86 处理器可以使用 CLFUSH 指令操作某个 Cache 行，该指令所实现的功能与 dcbf 指令类似。单从操作 Cache 行的指令的角度上看，PowerPC 处理器比 x86 处理器好得多，因此本节以 PowerPC 处理器为例说明这些 Cache 指令在不同情况下的使用方法。

下文将分别介绍在DMA写和DMA读操作中\_\_dma\_sync函数的工作流程。假设在一个单处理器系统中，Cache行长度为512b，而且PCI设备进行DMA读写操作时，硬件不进行Cache一致性操作。

# 1. DMA写

在外部设备进行 DMA 写操作之前，需要使用 \_\_dma\_sync 函数同步 Cache 与存储器中的数据。有许多书籍包括 Linux 系统中的 DMA - API 文档，都认为在 DMA 写操作完成后，调用 \_\_dma\_sync 函数 Invalidate 数据区域所对应的 Cache 行，处理器就可以使用来自设备的数据。这种说法是基于设备访问的数据区域头尾都是 Cache 行对界的情况而言的，如果数据区域并不是 Cache 行对界时，这种做法将引发系统错误。

假设在一个处理器系统中，Cache行长度为64B。当一个PCI设备通过DMA写操作，访问 $0\mathrm{x}1001\sim 10\mathrm{FE}$ 这段数据区域时，这段数据区域将占用4个Cache行，而且并不是Cache行对界的，如图12-9所示。

![[pci_express/b0a5d03561c25e46ac85ef797145fdb54d1e449009e16e1d979a4d22aca40bb1.jpg]]  
图12-9 DMA写访问的数据区域不对界

如果在 $0 \times 1000 \sim 0 \times 10\mathrm{FF}$ 这段数据区域中， $0 \times 1000$ 和 $0 \times 10\mathrm{FF}$ 字节曾经被改写过，那么 $0 \times 1000 \sim 103\mathrm{F}$ 和 $0 \times 10\mathrm{CO} \sim 10\mathrm{FF}$ 这两个 Cache 行的状态为 M，因此图 12-9 中阴影部分的数据与存储器不一致，而且为处理器系统中最新的数据。

而DMA写结束后， $0\mathrm{x}1001\sim 10\mathrm{FE}$ 这段数据区域被PCI设备改写，且为处理器系统中最新的数据，此时这段数据区域对应的Cache行状态仍然为M。此时如果处理器 Invalidate $0\mathrm{x}1001\sim 10\mathrm{FE}$ 这段数据区域对应的Cache行，即 Invalidate $0\mathrm{x}1000\sim 0\mathrm{x}10\mathrm{FF}$ 这段数据区域的Cache行时，将会丢失 $0\mathrm{x}1000$ 和 $0\mathrm{x}10\mathrm{FF}$ 这两个字节中保存的合法数据。如果处理器刷新

0x1001 \~ 10FE 这段数据区域对应的 Cache 行，即刷新 $0 \times 1000 \sim 0 \times 10\mathrm{FF}$ 这段数据区域的 Cache 行时，将丢失所有来自 PCI 设备的数据，采用这种方法问题更大。

通过上述分析可以发现，如果在DMA写完成后，再对访问的数据区域进行Cache同步操作，将可能引发严重的Cache一致性问题，从而导致整个系统异常。

为此正确的方法是在DMA写操作之前，将其访问的数据区域与Cache进行一致性操作，如源代码12-13中“case DMA\_FROM\_DEVICE”所示。但是这段源代码仍然有较大的问题，因为这段代码在处理数据区域不对界的情况时，将刷新整个数据区域对应的Cache行。

这种做法不会产生错误，但是会影响效率。假设与 $0 \times 1000 \sim 0 \times 10\mathrm{FF}$ 这段数据区域对应的 Cache 行的状态都为 M，那么这段程序将 $0 \times 1000 \sim 103\mathrm{F}, 0 \times 1040 \sim 0 \times 107\mathrm{F}, 0 \times 1080 \sim 0 \times 10\mathrm{BF}$ 和 $0 \times 10\mathrm{CO} \sim 10\mathrm{FF}$ 对应的 Cache 行都刷新到存储器中，然后再 Invalidate 这些 Cache 行。而实际上 $0 \times 1040 \sim 0 \times 10\mathrm{CO}$ 这段数据区域将由 PCI 设备重新填写，因而将这部分区域进行刷新然后再 Invalidate 是没有意义的。

正确的做法是刷新不对界的数据区域 $0 \times 1000 \sim 1040$ 和 $0 \times 10\mathrm{C}0 \sim 10\mathrm{FF}$ ，即将这段数据区域的头尾刷新即可，而直接 Invalidate 中间的数据区域 $0 \times 1040 \sim 10\mathrm{BF}$ 。采用这种方法将有效地提高系统效率。

# 2. DMA 读

外部设备进行 DMA 读操作之前，处理器必须保证该设备访问的数据区域与 Cache 一致。因此需要调用 \_\_dmaSync 函数（dir 参数为 DMA\_TO\_DEVICE）将 Cache 中的数据回写到存储器。该函数执行完毕后，Cache 中的所有数据都与存储器一致，而之前状态位为 M 的 Cache 行将更改为 E。经过这个 Cache 同步操作后，设备进行 DMA 读操作就可以从存储器中获得正确的数据。

此处还有一个细节问题值得考虑，就是DMA读操作是调用dcbst指令回写Cache行，还是调用dcbf指令刷新Cache行。如上文所述dcbf和dcbst指令都将状态位为M的Cache行与存储器进行同步，只是dcbf将Cache行的M位更新为I，而dcbst指令将Cache行的状态更新为E。使用这两个指令都可以保证DMA读的数据不会出现一致性问题。

此时在处理器系统中，如果DMA读的数据将不会被处理器使用时，应该使用dcbf指令， Invalidate该Cache行，从而该Cache行可以被其他进程使用；如果DMA读的数据将很快被处理器使用时，应该使用dcbst指令，回写该Cache行，从而处理器使用该数据时，可以从Cache而不是存储器中获得。

