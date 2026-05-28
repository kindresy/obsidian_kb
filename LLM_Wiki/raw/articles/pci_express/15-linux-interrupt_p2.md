---
compiled: false
date: 2026-05-28
source: 《PCI Express 体系结构导读》
source-type: book-chapter
chapter: "15"
section: "15.2.2 Linux 如何使能 MSI-X 中断机制"
part: 2
tags: [pci, pci-express, computer-architecture]
---
# 15.2.2 Linux 如何使能 MSI-X 中断机制

在Linux系统中，如果PCI/PCIe设备需要使用MSI-X中断机制，需要调用pci\_enable\_msix函数，pci\_enable\_msix函数调用的大多数函数与pci\_enable\_msi类似，本节并不会重复解释这些函数，该函数的实现如源代码15-14所示。

intpci_enable_msix(structpci_dev\*dev，structmsix_entry\*entries，intnvec)   
{ intstatus，nr_entries; inti，j; if(!entries) return-EINVAL; status $\equiv$ pcimsi_check_device( dev，nvec，PCI_CAP_ID_MSIX）； if(status) return status; nr_entries $\equiv$ pcimsi_table_size(dev); if(nvec $>$ nr_entries) return nr_entries; /\*Check for any invalid entries \*/ for $(\mathrm{i} = 0;\mathrm{i} <   \mathrm{nvec};\mathrm{i} + + )$ { if (entries[i].entry $\rightharpoondown$ nr_entries) return -EINVAL;/\*invalid entry \*/ for $(\mathrm{j} = \mathrm{i} + 1;\mathrm{j} <   \mathrm{nvec};\mathrm{j} + + )$ { if (entries[i].entry $=$ entries[j].entry) return -EINVAL;/\* duplicate entry \*/ } }WARN_ON(!！dev- $\rightharpoonup$ msixenabled); /\* Check whether driver already requested for MSI irq \*/ if（dev- $\rightharpoondown$ msi-enabled）{ dev_info(&dev- $\rightharpoondown$ dev,"can't enableMSI-X" "MSI IRQalreadyassigned)\n"); return -EINVAL; } status $=$ msix_capacity_init(dev，entries，nvec）; return status;

与pci\_enable\_msi\_block函数不同，pci\_enable\_msix函数的入口参数包括一个msix\_entry结构的entries链表（在使用这个entries链表之前需要将msix\_entry entry参数赋值），而nvec参数保存entries链表的长度。该函数首先对入口参数进行检查，然后调用msix\_capability\_init函数为PCIe设备分配多个中断向量号。msix\_capability\_init函数的实现与msi\_capabil-

ity\_init函数的实现方法类似，本章对此不做进一步描述。

该函数成功返回后，PCIe设备将得到多个中断向量，并将结果放入pci\_dev $\rightarrow$ msi\_list和entries链表中，之后PCIe设备的Linux驱动程序可以使用多个request\_irq函数注册相应的中断服务例程。

下文将以Intel的e1000e网卡驱动程序说明如何使用MSI-X中断机制挂接中断服务例程。在Linux中，与e1000e网卡相关的驱动程序在./drivers/net/e1000e/netdev.c文件中。其中MSI-X中断机制的初始化在e1000probe $\rightarrow$ e1000\_sw\_init $\rightarrow$ e1000e\_set\_interrupt Capability函数中，该函数的实现如源代码15-15所示。

源代码15-15 e1000e\_set\_interrupt Capability函数  
void e1000e_set_interrupt Capability(struct e1000_adapter \*adapter)   
{   
... switch (adapter- $>$ int_mode）{ case E1000E_INT_MODE_MSIX: if(adapter- $\rightharpoondown$ flags&FLAG.Has_MSIX）{ numvecs $= 3$ /\*RxQO,TxQOand other\*/ adapter- $\rightharpoonup$ msix_entries $\equiv$ kcalloc(numvecssizeof(struct msix_entry）， GFP_KERNEL); if(adapter- $\rightharpoondown$ msix_entries）{ for $(\mathrm{i} = 0$ ；i $<$ numvecsc; $\mathrm{i + + }$ ) adapter- $\rightharpoonup$ msix_entries[i].entry $=$ i; err $\equiv$ pcie_enable_msix(adapter- $\rightharpoondown$ pdev， adapter- $\rightharpoonup$ msix_entries， numvecsc); if( $\operatorname {err} = = 0$ ） return; }   
…   
1

当e1000e\_set\_interrupt Capability函数返回后，MSI-X中断机制使用的中断向量将被保存在adapter- $\rightharpoondown$ msix\_entries数组中，之后e1000\_open $\rightarrow$ e1000\_request\_irq $\rightarrow$ e1000\_request\_msix函数将多次调用request\_irq函数将e1000e使用的中断服务例程挂接到系统中断服务程序中，e1000\_request\_msix函数的实现如源代码15-16所示。

源代码15-16 e1000\_request\_msix函数  
```lisp
static int e1000_request_msix(struct e1000_adapter *adapter) 
```

err $=$ request_irq(adapter- $\rightharpoondown$ msix_entries[ vector].vector, &e1000_intr_msis_rx，0，adapter- $\rightharpoonup$ rx_ring- $\rightharpoonup$ name, netdev）;   
err $=$ request_irq(adapter- $\rightharpoonup$ msix_entries[ vector].vector, &e1000_intr_msis_tx，0，adapter- $\rightharpoonup$ tx_ring- $\rightharpoonup$ name, netdev）;   
err $=$ request_irq(adapter- $\rightharpoonup$ msix_entries[ vector].vector, &e1000_msis_other，0，netdev- $\rightharpoonup$ name，netdev）;

e1000\_request\_msix 函数将“接收完成中断请求 e1000\_intr\_msix\_rx”、“发送完成中断请求 e1000\_intr\_msix\_tx”和“其他中断请求 e1000\_msix\_other”分别注册。当有中断事件发生时，驱动程序不需要读取中断状态寄存器之后再进行处理，从而有效降低了系统延时。

# 15.3 小结

本节主要介绍了PCI设备的中断请求在Linux系统中的处理过程。Linux系统的更新速度较快，并不断加入新的功能。本章的内容基于Linux系统，目前Linux系统支持多种架构的处理器系统，而且得到了极大的普及。对于有志于学习体系结构的工程师而言，深入了解几种操作系统是必须的。而在这些操作系统中，Linux无疑最为开放，读者也最容易了解其实现细节。但是值得注意的是，在体系结构的学习过程中，不要拘泥于Linux系统本身，Linux系统仅包含了体系结构的部分内容，也只是一种实现方法。

本书到此告一段落，而PCIe总线仍然继续向前发展，PCIe V3.0规范即将发布，其中增加了许多新的功能，而这些新的功能在许多处理器系统中并没有意义。这些新的功能在本书中多有提及，但并不是本书的重点。本书的重点是以PCIe总线为例说明处理器的体系结构，是对PCIe体系结构进行导读，更准确地说，是以PCIe总线为例说明处理器体系结构中局部总线的设计原理与使用方法。

# 参考文献

[1] PCISIG. PCI Local Bus Specification, Revision 3.0 [S]. 2003.   
[2] PCISIG. PCI-to-PCI Bridge Architecture Specification, Revision 1.2 [S]. 2003.   
[3] PCISIG. PCI Express Base Specification, Revision 2.1 [S]. 2009.   
[4] PCISIG. PCI Express Base Specification, Revision 3.0, Version 0.7 [S]. 2009.   
[5] PCISIG. PCI Express to PCI/PCI-X Bridge Specification, Revision 1.0 [S]. 2003.   
[6] PCISIG. PCI Bus Power Management Interface Specification, Revision 1.2 [S]. 2004.   
[7] PCISIG. Address Translation Services, Revision 1.1 [S]. 2009.   
[8] PCISIG. Single Root I/O Virtualization and Sharing Specification, Revision 1.0 [S]. 2007.   
[9] PCISIG. Multi-Root I/O Virtualization and Sharing Specification, Revision 1.0 [S]. 2008.   
[10] Tom Shanley, Don Anderson. "Chapter 25: Transaction Ordering & Deadlocks", PCI System Architecture, Fourth Edition, 651-671 [M]. 1999.   
[11] Ravi Budruk, Don Anderson, Tom Shanley. PCI Express System Architecture, Chapter 5 ACK/NAK Protocal [M]. 2003.   
[12] Intel. 21555 Non-Transparent PCI-to-PCI Bridge User Manual [S]. 2001.   
[13] Intel. Intel 64 and IA-32 Architectures Software Developer's Manual Volume 3A: System Programming Guide, Part 1 [S]. 2008.   
[14] Intel. Intel 64 and IA-32 Architectures Software Developer's Manual Volume 3B: System Programming Guide, Part 2 [S]. 2008.   
[15] Intel. Mobile Intel® 4 Series Express Chipset Family, Revision 2.1 [S]. 2008.   
[16] Intel. Intel I/O Controller Hub 9(ICH9) Family Datasheet [S]. 2008.   
[17] Intel. Intel Virtualization Technology for Directed I/O [S]. 2008.   
[18] Intel. Interrupt Swizzling Solution for Intel 5000 Chipset Series-based Platforms [OL]. 2006.   
[19] Intel. MultiProcessor Specification Version 1.4 [S]. 1997.   
[20] Intel. ACPI Component Architecture Programmer Reference—OS-Independent Subsystem, Debugger, and Utilities, Revision 1.22 [S]. 2009.   
[21] IBM. Power ISA, Version 2.05, October 2007 [S]. 2007   
[22] Freescale. PowerPC e500 Core Family Reference Manual, Revision 1 [S]. April 2005.   
[23] Freescale. MPC8548E PowerQUICC™ III Integrated Processor Family Reference Manual, Revision 2 [S]. 2007.   
[24] Freescale. MPC8572E PowerQUICC™ III Integrated Processor Family Reference Manual, Revision A [S]. 2008.   
[25] Freescale. QorIQ P4080 Communications Process Product Brief [S]. 2009.   
[26] Freescale. Embedded Multicore: An introduction [S]. 2009.   
[27] AMD. AMD64 Technology—AMD64 Architecture Programmer's Manual Volume 2: System Programming [S]. 2007.   
[28] AMD. AMD I/O Virtualization Technology (IOMMU) Specification [S]. 2009.   
[29] Xilinx. LogiCORE™ Endpoint PIPE v1.7 for PCI Express® User Guide [S]. 2007.   
[30] HP, Intel, Microsoft, Phoenix and Toshiba. Advanced Configuration and Power Interface Specification4.0

[S]. 2009.   
[31] Robert A Maddox, Gurbir Singh, Robert J Safranek. Weaving High Performance Multiprocessor Fabric [M]. Intel Press, ISBN 13: 978-1-934053-18-8. 2009.   
[32] Elliot Garbus, Peter Sankhagowit, Marc Goldschmidt, Nick Eskandari. Architecture for an I/O processor that Integrates a PCI to PCI Bridge [P], March 1999. US Patent 5,884,027. 1999.   
[33] Joe Winkles. Sizing of the Replay Buffer in PCI Express Devices [OL]. October, 2003. MindShare, Inc. 2003.   
[34] Joe Winkles. Elastic Buffer Implementations in PCI Express Devices [OL]. November, 2003. MindShare, Inc. 2003.   
[35] Roger E Tipley. Split transaction protocol for the peripheral component interconnect bus [P]. US Patent 5533204, Jul 2, 1996.   
[36] James E Smith. A Study of Branch Prediction Strategies, Proceedings of the 8th Annual Symposium on Computer Architecture [J], p.135-148, May 12-14, 1981, Minneapolis, Minnesota, United States. 1981.   
[37] T Y Yeh, Y N Patt. Alternative implementation of Two-level Adaptive Branch prediction [J], Proc. 19th Ann. Int '1 Symp. Computer Architecture, pp. 124-134, 1992.   
[38] Intel. The White Paper of Intel® Next Generation Nehalem Microarchitecture [OL]. 2009.   
[39] Steven P Vanderwiel, David J Lilja. Data Prefetch Mechanisms [J], ACM Computing Surverys, Vol. 32, No. 2. June 2000.   
[40] Norman P Jouppi. Improving Direct-mapped Cache Performance by the Addition of a Small Fully-associative Cache and Prefetch Buffers [J], Proceedings of the 17th Annual International Symposium on Computer Architecture, p. 364-373, May 28-31, 1990, Seattle, Washington, United States. 1990.   
[41] Mikko H Lipasti, Christopher B Wilkerson, John Paul Shen. Value locality and Load Value Prediction [J], Proceedings of the Seventh International Conference on Architectural Support for Programming Languages and Operating Systems, p. 138-147, October 01-04, 1996, Cambridge, Massachusetts, United States.   
[42] F Bonomi, K W Fendick. The Rate-based Flow Control Framework for Available Bit-rate ATM Services [J]. IEEE Network, pp. 25-39, March/April 1995.   
[43] William J Dally, Member, IEEE. Virtual-Channel Flow Control [J], IEEE Transactions On Parallel and Distributed System, Vol. 3, No. 2, March 1992.   
[44] H T Kung and Robert Morris. Credit-Based Flow Control for ATM Networks [J], IEEE Network, Pg. 40\~48, March/April 1995.   
[45] H T Kung, Trevor Blackwell, Alan Chapman. Credit-based Flow Control for ATM Networks: Credit Update Protocal, Adaptive Credit Allocation, and Statistical Multiplexing [J]. Proceedings of the ACM SIGCOMM 1994 Symposium on Communications Architectures, Protocols and Applications, Pg. 101 \~104 August 31-September 2, 1994.   
[46] H T Kuang, Alan Chapman. The FCVC (Flow-controlled Virtual Channels) Proposal for ATM Networks [S], Version 1.1, 1993.   
[47] A Parekh and R Gallager. A Generalized Processor Sharing Approach to Flow Control- The Single Node Case [J]. In Technical Report LIDS-TR-2040, Laboratory for Information and Decision Systems, Massachusetts Institute of Technology, 1991.   
[48] A Parekh. A Generalized Processor Sharing Approach to Flow Control in Integrated Services Networks [J], In Technical Report LIDS-TR-2089, Laboratory for Information and Decision Systems, Massachusetts Institute of Technology, 1992.

[49] P Kernami, L Kleinrock. Virtual Cut Through: A New Computer Communication Controller [J]. IEEE Computer Society Press, Oct. 1987, Pg. 230\~234.   
[50] M Shreedhar, George Varghese. Efficient fair queueing using deficit round-robin [J]. IEEE/ACM Transactions on Networking, Volume 4, Iuuse 3, June 1996.   
[51] Ferguson P and Huston G. Quality of Service: Delivering QoS on the Internet and in Corporate Networks [J]. John Wiley & Sons, Inc., 1998. ISBN 0-471-24358-2.   
[52] William J Dally. Performance Analysis of k-ary n-cube Interconnection Networks [J]. IEEE Transactions on Computers, v. 39 n. 6, p. 775-785, June 1990.   
[53] Jack Regula. Using PCIe in a Variety of Multiprocessor System Configurations [OL]. http://www.plxtech.com/about/news/archive/articles2007. PLX Technology. 2007.   
[54] Albert X, Widmer, Peter A Franaszak. A DC-Balanced, Partitioned-Block, 8B/10B Transmission Code [J]. IBM J. RES. DEVELOP. Vol. 27, No. 5. 1983,   
[55] James E J Bottomley. Dynamic DMA mapping using the generic device [OL]. http://www.kernel.org/doc/Documentation/DMA-API.txt.   
[56] Jim Handy. Cache Memory Book [M]. the $2^{\mathrm{nd}}$ version. 1998.   
[57] V Krishnan. Towards an Integrated IO and Clustering Solution using PCI Express [J]. IEEE International Conference on Cluster Computing (CLUSTER 2007), September, 2007.   
[58] Hum, Herbert H J, Goodman, James R. US Patent 6922756-Forward State for Use in Cache Coherency in a Multiprocessor system [P]. 2005.   
[59] Jack, Regula. Using PCIe in a Variety of Multiprocessor System Configurations [OL]. http://wwwembedded.com-columns/technicalinsights/196902357?\_requestid = 349156. 2007.   
[60] Brian Holden. Latency Comparison Between HyperTransport and PCI-Express In Communication Systems [OL]. http://enterprise2/amd.com/Downloads/Industry/Telecommunications/Latency\_Comparison\_HyperTransport.pdf. 2006.   
[61] Alex Goldhammer, John Ayer Jr. Understanding Performance of PCI Express System [OL]. http://www.xilinx.com/support/documentation/white\_papers/wp350.pdf. 2008.   
[62] SBS Implementers Forum. System Management Bus Specification Verison 2.0 [S]. August, 2000.   
[63] Application Notes from Maxim. Comparing the I²C Bus to the SMBus [OL]. http://pdfserv.maxim-ic.com/en/an/AN476.pdf. 2000.   
[64] PLX. ExpressLane PEX 8518AA/AB/AC 5-Port/16-Lane PCI Express Switch Data Book [S]. 2007.

# PCI Express 体系结构导读

![[pci_express/b45a6c35607e2457a17ac46a77633ae2938962cc28d0d34d0e7efe896193a977.jpg]]

本书讲述了与PCI及PCI Express总线相关的最为基础的内容，并介绍了一些必要的、与PCI总线相关的处理器体系结构知识，这也是本书的重点所在。深入理解处理器体系结构是理解PCI与PCI Express总线的重要基础。

读者通过对本书的学习，可超越PCI与PCI Express总线自身的内容，理解在一个通用处理器系统中局部总线的设计思路与实现方法，从而理解其他处理器系统使用的局部总线。

地址：北京市百万庄大街22号

邮政编码：100037

电话服务

社服务中心：010-88361066

销售一部：010-68326294

销售二部：010-88379649

读者购书热线：010-88379203

网络服务

教材网：http://www.cmpedu.com

机工官网：http://www.cmpbook.com

机工官博：http://weibo.com/cmp1952

封面无防伪标均为盗版
