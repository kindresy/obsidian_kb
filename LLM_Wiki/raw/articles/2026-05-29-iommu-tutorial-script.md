---
date: 2026-05-29
source-type: transcript
source-url: raw/articles/iommu/IOMMU_TUTORIAL_ASPLOS_2016.pdf
title: "IOMMU 授课台词 — 基于 ASPLOS 2016 Tutorial"
compiled: true
---

# IOMMU 授课台词 — 基于 ASPLOS 2016 Tutorial

**授课人角度**：芯片设计 / 驱动开发工程师背景
**受众**：高年级本科生 / 研究生，已了解基本计算机组成和操作系统
**时长估计**：约 90 分钟（含提问）

---

## Slide 1 — 封面

（展示标题页）

"同学们好。今天我们来聊一个在计算机体系结构中非常关键、但在课堂里往往被一笔带过的硬件单元——**IOMMU**，全称 IO Memory Management Unit。

这份教材来自 ASPLOS 2016 的一个 Tutorial，四位讲者来自 AMD 和 Intel，都是 IOMMU 的一线架构师。虽然时间过去几年了，但 IOMMU 的基本原理没有变，而且它在今天的云原生、AI 加速器场景下比以往任何时候都更重要。

我们先定一个基调。今天不讲数学公式，不讲繁复的状态机，我们讲：**问题是什么，硬件怎么解决，软件怎么配合**。"

---

## Slide 2 — 范围定义

（展示 "What This Tutorial Will and Will Not Cover"）

"首先定义一下我们说的 'IO' 是什么。传统 IO 包括 GPU（图形）、网卡、存储控制器、USB 控制器这些。新型 IO 包括 GPGPU 通用计算、加密加速器、DSP——换句话说，凡是挂在 PCIe 总线上的、能发起 DMA 的设备，都在我们今天讨论的范围内。

把一个 IO 设备虚拟化，分成两部分工作：

第一，**设备侧**——让一个物理设备看起来像多个虚拟设备。这是 SR-IOV 和 MR-IOV 做的事，今天不展开。

第二，**系统侧**——也就是 **IOMMU**。它负责两件事：**DMA 地址翻译和保护**，以及**中断重映射和虚拟化**。

我们今天讲的是系统侧，也就是 IOMMU。"

---

## Slide 3 — Agenda

（展示 Agenda）

"四个部分。**Andy Kegel** 讲 motivation 和 introduction——为什么需要 IOMMU。**Paul Blinzer** 讲 use cases——IOMMU 到底能解决哪些实际问题。**Arka Basu 和 Maggie Chan** 讲 internals——IOMMU 内部怎么工作的。最后 **Arka Basu** 讲 research directions——还有哪些开放问题。"

---

## Slide 4-9 — 传统 DMA 的问题

（展示逐步构建的图示：Core → Device → Memory）

"我们先看一个**没有 IOMMU** 的系统。这张图一步步展示传统的 DMA 流程。

这里有一个 CPU Core、一个 IO 设备、一个 MMU（CPU 的内存管理单元）、以及物理内存。

CPU 访问内存时走 MMU——虚拟地址转物理地址、权限检查——一切正常。

但注意看 IO 设备。设备发起 DMA 时，**它直接使用物理地址**。没有翻译、没有权限检查。驱动告诉设备 '把数据写到地址 0x1000'，设备就直接往 0x1000 写。

问题是什么？

第一，**恶意设备可以任意读写内存**。这不是理论问题——FinSpy 这个软件就利用 Firewire 设备的 DMA 能力直接读取系统内存，完全绕过操作系统。

第二，**有 bug 的驱动可以导致设备写到错误的内存位置**，把操作系统最关键的数据结构踩坏。

第三，**侧信道攻击**——设备可以遍历物理内存读取敏感信息。

所有这些问题的根源是同一个：**DMA 没有硬件强制保护**。"

---

## Slide 10 — 虚拟化趋势

（展示 "Virtual Machines Are Trending"）

"为什么要强调这个问题？因为虚拟化在服务器端已经普及了。2005 到 2012 年，虚拟化市场翻了十倍以上。IDC 的数据显示，到 2012 年，超过 70% 的服务器工作负载已经运行在虚拟机上。

虚拟化的核心诉求是：**让 Guest OS 直接访问硬件，同时不破坏隔离性**。这对 CPU 和内存，我们有 Intel VT-x 和 AMD-V 的 nested page tables。但对 IO，我们一直没有一个好的硬件方案——直到 IOMMU。"

---

## Slide 11 — 虚拟化系统中的地址翻译

（展示 "Translation in Virtualized System" 图）

"讲 IOMMU 之前，我们需要理解虚拟化系统中的地址层次。

在虚拟化系统中，我们有三个地址空间：
- **GVA**（Guest Virtual Address）——客户操作系统里的虚拟地址
- **GPA**（Guest Physical Address）——客户操作系统看到的物理地址
- **SPA**（System Physical Address）——真正的硬件物理地址

客户 OS 管理 GVA → GPA 的翻译。Hypervisor（VMM）管理 GPA → SPA 的翻译。关键是：**客户 OS 不知道 SPA，它只能看到 GPA**。"

---

## Slide 12 — 传统 DMA 在虚拟机中的问题

（展示 "Traditional DMA in Virtual Machines"）

"那问题来了：如果我把一个物理设备直通给某个虚拟机，这个虚拟机里的驱动会告诉设备 'DMA 到 GPA 0x1000'。但设备是物理设备，它做 DMA 时用的是 SPA，不是 GPA。

那它就会把数据写到 SPA 0x1000——这可能是另一个虚拟机的内存。

传统的解决方案是：**每次 DMA 都由 VMM 介入做地址翻译**。但这意味着每一次设备访问内存都要触发 VM exit，带来约 30% 的性能开销。

对于网络、存储这类高 IO 的工作负载，30% 是不可接受的。"

---

## Slide 13 — IOMMU 的逻辑视图

（展示 "Introduction of IOMMU: The Logical View"）

"IOMMU 的出现就是为了解决这个问题。

IOMMU 是一个硬件单元，**插在设备和内存之间**。所有 DMA 事务都必须经过 IOMMU。

两个核心能力：

1. **DMA 内存保护**——设备只能访问它被授权的内存区域
2. **DMA 地址翻译**——把设备使用的 IO 地址翻译成系统物理地址

IOMMU 的配置由 **IOMMU Driver**（运行在 OS 或 VMM 中）负责。驱动告诉 IOMMU：'设备 A 属于 Domain 1，它的地址映射表在这里。'然后 IOMMU 硬件自动做翻译和检查。

这就像给 DMA 世界装了一个 MMU。实际上你可以认为：CPU 有 MMU，设备有 IOMMU。"

---

## Slide 14 — 传统中断的问题

（展示 "Traditional IO Interrupt"）

"DMA 之外，另一个大问题是中断。

在没有虚拟化的系统中，设备发中断很简单：MSI/MSI-X 消息里带一个中断号和目标 CPU 的 ID，APIC 直接投递。

但虚拟化以后问题来了：

第一，**客户 OS 迁移后**，中断目标 CPU 变了，但设备的 MSI 配置可能还是老的——需要动态重映射。

第二，**IPI（处理器间中断）的处理开销很大**——每个额外的中断处理需要 5K 到 10K 个 CPU 周期。

第三，最严重的是：**每次设备中断都导致 VM exit，VMM 来处理**。如果客户 OS 刚好被调度走了，这个中断可能导致不必要的 VMM 唤醒。

你想想，一个高吞吐量的网卡每秒可能产生几十万次中断，如果每次都要 VMM 介入，性能灾难。

解决方案是什么？**中断重映射**——让 IOMMU 把设备中断直接投递到目标 vCPU，VMM 不需要参与。"

---

## Slide 15 — 增加中断处理能力

（展示 "Adding Interrupt Handling Capability"）

"这就引出了 IOMMU 的第三个核心能力：

3. **中断重映射和虚拟化**

加上前面的 DMA 翻译和保护，IOMMU 现在有了三大能力。

注意这个演进顺序不是随意的——历史上 DMA remapping 最先出现（2004 年），中断 remapping 在 2006 年左右加入，然后才是虚拟化和 HSA 相关的特性。"

---

## Slide 16 — 异构系统的出现

（展示 "Heterogeneous System Architecture"）

"第三个驱动因素是异构计算。

传统 GPU 编程有这几个痛点：**多个内存池**（系统内存、GPU 显存）、**多个地址空间**、**高开销的任务分发**、**需要通过 PCIe 拷贝数据**。

程序员要学 OpenCL、CUDA 这些新语言和新 API。结果是只有专家才能写出高性能的异构程序。

HSA（Heterogeneous System Architecture）的理念是：让 GPU 成为系统里的一等公民，CPU 和 GPU 共享统一的地址空间。

这就引出了 IOMMU 的第四个能力：**共享地址空间**。"

---

## Slide 17 — IO 共享 CPU 页表

（展示 "IO Can Share CPU Page Tables"）

"4. **IO 可以共享 CPU 的页表**

在 HSA 中，GPU 指针就是 CPU 指针——所谓 'Pointer-is-a-Pointer'。GPU 可以直接解引用 CPU 的虚拟地址，IOMMU 负责翻译。

这意味着开发者在 GPU 上写代码时，不需要显式管理内存拷贝，不需要 pin 住页面，不需要操心地址转换。

这背后是 OS 通过 IOMMU 把进程页表共享给设备。设备访问一个虚拟地址，IOMMU 查页表，找到物理地址，发起 DMA。整个流程对设备来说是透明的。"

---

## Slide 18 — IOMMU 的物理视图

（展示 "Physical View"）

"从物理架构上看，IOMMU 位于 **Processor Complex** 内部，在 **Root Complex**（也叫 IOHUB）和内存控制器之间。

每个从设备发起 DMA 请求，穿过 PCIe 交换结构到达 Root Complex，然后在 IOMMU 这里被拦截做翻译和检查，之后才到达内存控制器。

从 PCIe 规范的角度看，IOMMU 是 **Translation Agent**。它维护 **Address Translation and Protection Table**。支持 ATS（Address Translation Service）的设备还可以在本地缓存翻译结果，这个缓存叫 **ATC**（Address Translation Cache）。"

---

## Slide 19 — CPU MMU vs IOMMU

（展示 "Comparing CPU MMU and IOMMU"）

"这是一个非常有趣的对比。CPU MMU 和 IOMMU 做的都是地址翻译和权限检查，但有几个关键区别：

| 方面 | CPU MMU | IOMMU |
|------|---------|-------|
| 翻译方向 | VA → PA / GVA → GPA → SPA | 设备地址 → SPA，同样支持两层翻译 |
| 中断处理 | 无 | 中断重映射和虚拟化 |
| 并发度 | 大多单线程 | 高度多线程（要处理多个设备同时请求） |
| 缺页处理 | 同步（CPU 直接处理 page fault） | **异步**（设备发 ATS 请求，等响应） |

最后一点尤其重要。CPU 缺页时，MMU 触发一个同步异常，CPU 立即处理。但设备缺页时，IOMMU 不能 '暂停' 设备。它必须通过 ATS/PRI 协议异步处理——设备发一个请求，IOMMU 通知驱动，驱动处理完后再回复设备。这个异步机制是 IOMMU 设计中最复杂的部分之一。"

---

## Slide 20 — IOMMU 发展历史

（展示 "History: A Simplified View"）

"看看 IOMMU 的演进：

- **v1 (2004)**：基本的 DMA 地址翻译和验证，替代软件 bounce buffer
- **v1.2 (2006)**：加入中断重映射，支持 IO 虚拟化
- **v2 (2008)**：加入 nested paging（嵌套页表）、中断虚拟化、改进管理功能
- **v3 (2010)**：为完整异构计算加入特性——PASID、ATS/PRI、SVM 支持

这个演进反映了 IOMMU 从'解决 DMA 安全问题'到'成为异构计算基础设施'的转变。"

---

## Slide 21 — IOMMU 技术家族

（展示 "IOMMU Technology Families"）

"目前业界有四种主要的 IOMMU 实现：

1. **AMD IOMMU**（也叫 IOMMU）—— AMD 平台
2. **Intel VT-d**（Virtualization Technology for Directed IO）—— Intel 平台
3. **ARM SMMU**（System MMU）—— ARM 平台
4. **IBM CAPI**（Coherent Accelerator Processor Interface）—— POWER 平台

它们的核心思想相同，但具体数据结构和编程接口各有差异。"

---

## Slide 22-23 — 五大应用场景

（展示 "Five Use Cases"）

"接下来 Paul Blinzer 会带我们走一遍 IOMMU 的五个实际应用场景：

1. **Legacy IO**——支持老设备（32 位 DMA 设备在 64 位系统中）
2. **Security & Protection**——防止 DMA 攻击
3. **Secure Boot**——在固件阶段就保护内存
4. **Direct IO Devices**——虚拟机设备直通
5. **Heterogeneous Computing**——共享虚拟内存"

---

## Slide 24-26 — 场景一：支持 Legacy 设备

（展示 "Supporting Legacy Devices"）

"第一个场景。

很多老设备——比如旧的 PCI 卡、IEEE-1284 并口控制器——只能做 32 位的 DMA。但现代系统内存可能在 64 位地址空间的某个位置，远高于 4GB。

软件方案是 **bounce buffer**：在 32 位可寻址的物理内存里划一块区域，设备 DMA 到这里，CPU 再拷贝到最终目的地。慢、需要同步、占用 CPU 周期。

IOMMU 的方案更优雅：设备还是往它的 32 位地址写——比如 0x10002004——IOMMU 把这个地址翻译成 64 位的系统物理地址。DMA 直接到达目标，不需要 CPU 搬运数据。

Linux 里这叫 DMA redirect 功能。"

---

## Slide 27-29 — 场景二：安全与保护

（展示 "Security and Protection"）

"第二个场景——这也是 IOMMU 最原始的动机。

我们知道 DMA 设备直接读写物理内存。如果驱动有 bug，或者被恶意利用，设备可以改写操作系统的关键数据——密码、安全策略、页面表——操作系统完全无法检测。

IOMMU 的解决方案是给每个设备划定一个 **DMA 域**。设备驱动向 OS 申请 DMA buffer，OS 在 IOMMU 的页表中建立映射。设备只能访问它被授权的物理页面，访问范围之外的地址直接被 IOMMU 阻止，并通知 OS。

这就像一个硬件防火墙，架在设备和内存之间。"

---

## Slide 30 — 场景三：Secure Boot

（展示 "Yet Another Use for an IOMMU"）

"第三个场景更有意思——**Secure Boot**。

Secure Boot 的架构确保只有经过签名的 OS 内核才能运行。但在早期的启动阶段——UEFI 固件运行时——有些设备已经可以做 DMA 了。

比如 1394/Firewire 接口、PXE 网络启动——它们可以在 OS 还没有机会设置保护之前，通过 DMA 修改内存。

IOMMU 的解决方案是：**在固件阶段就初始化 IOMMU**，设定好关键内存区域的保护策略。从最早期就开始保护，不给攻击者窗口。"

---

## Slide 31-37 — 场景四：虚拟化中的高效 IO

（展示 "Efficient IO in Virtualized Environment"）

"第四个场景——也是 IOMMU 最核心的应用。

虚拟化 IO 有两种方式：

**半虚拟化（para-virtualization）**：Guest 的驱动通过 hypercall 让 VMM 代为操作硬件。每次 IO 操作都经过 VMM，安全但慢。

**直通（direct-mapped / SR-IOV）**：物理设备（或 VF）直接映射给 Guest。Guest 用原生驱动直接操作硬件，性能接近 native。问题在于：Guest 的驱动设置的是 GPA，不是 SPA。如果不用 IOMMU，设备会写到错误的物理地址。

IOMMU 解决这个问题：它把 Guest 物理地址翻译成系统物理地址。Guest 驱动写 GPA 0x1000，IOMMU 翻译成 SPA 0x7F000，写入正确的物理位置。同时 IOMMU 确保设备不能越界访问。

效果：**接近原生的 IO 性能** + **完全的 VMM 隔离保护**。

这就是为什么今天所有云平台（AWS Nitro、Azure、Google Cloud）都依赖 IOMMU 做设备直通。"

---

## Slide 38-44 — 场景五：异构计算

（展示 "Enabling Heterogeneous Computing"）

"第五个场景——异构计算。这部分大家可能更熟悉了。

在 HSA 之前，GPU 编程有这几个痛点：

**多个内存池，多个地址空间。** 系统内存和 GPU 显存是分开的。数据要先拷贝到 GPU 显存，GPU 计算结果，再拷贝回来。这带来了大量冗余的数据搬运和代码复杂度。

**传统软件栈对比：**
左边是传统的 GPU 驱动栈——应用通过 OpenCL/DirectX 运行时、用户态驱动、内核态驱动，层层调用。每次 GPU 调用都要经过 OS 驱动链，调度开销很大。

右边是 HSA 的软件栈——用户态的任务队列可以直接把计算任务发给 GPU，不需要内核介入。IOMMU 提供硬件保护，保证即使是在用户态调度，设备也不能越界。

从代码行数和性能的对比图可以看到：HSA 方案（Bolt）在总代码行数和执行时间上都有数量级的优势。Init + Compile + Copy + Algorithm 整个过程更加紧凑。

性能测试方面，FIR（内存密集型）和 AES（计算密集型）两个 benchmark 都显示 HSA 的 unified memory 没有性能损失，同时编程模型大幅简化。

Black-Scholes 期权的例子也验证了：C++ on HSA 在带宽利用率、kernel 分发速度和共享数据结构支持上都优于传统的 OpenCL buffer 方案。

IOMMU 在这里扮演的角色：**OS 通过 IOMMU 把进程页表共享给 GPU**。GPU 访问进程虚拟地址时，IOMMU 做翻译。如果发生缺页，IOMMU 通过 PRI 协议通知 OS 处理。整个流程对程序员透明。"

---

## Slide 45-46 — 下半场预告

（展示 "Recap: IOMMU and Its Capabilities" 和 "What Is Coming Up"）

"好，休息前我们快速总结一下 IOMMU 的四大能力：

1. **内存保护**——防止恶意或出错的设备搞破坏
2. **共享虚拟内存**——让加速器共享 CPU 进程地址空间
3. **IO 虚拟化**——安全高效的设备直通
4. **Legacy IO 支持 + Secure Boot**——让老设备在新系统里也能工作

休息后 Arka Basu 和 Maggie Chan 会深入 IOMMU 的内部机制：地址翻译怎么做、缓存怎么设计、缺页怎么处理、中断怎么重映射、TLB shootdown 怎么做。"

---

## Slide 47-50 — 地址翻译和内存保护

（展示 "Address Translation and Memory Protection"）

"好，下半场。我们深入到 IOMMU 的内部工作原理。

地址翻译的流程三步走：

第一步，**Device Table Lookup**。设备发起 DMA 请求时，IOMMU 根据设备的 PCIe **Device ID**（Bus:Device:Function）查找设备表。每个设备属于一个 **Domain**（域），由 **Domain ID** 标识。

第二步，**Page Table Walk**。找到 Domain 的页表根指针后，IOMMU 硬件遍历页表，把设备的 IO 虚拟地址翻译成系统物理地址。

第三步，**权限检查**。如果设备没有足够的权限访问目标地址，IOMMU **中止请求**，设备拿不到数据。OS 可以被通知。

为了让翻译更快，IOMMU 内部有 **IOTLB**（IO TLB）——一个硬件缓存，缓存最近用过的翻译结果。和 CPU 的 TLB 非常像。

这是基本流程。下面我们看几个进阶场景。"

---

## Slide 51-52 — 共享地址空间

（展示 "Sharing Address Space with CPU"）

"刚才讲到 HSA 场景中设备需要访问 CPU 进程的地址空间。

这里有一个关键问题：IOMMU 怎么知道设备当前应该用哪个进程的页表？

答案是一个叫 **PASID**（Process Address Space ID）的东西。设备在 DMA 请求中附带 PASID，IOMMU 根据 PASID 找到对应进程的 **gCR3 Table**，从中取出进程页表根指针，完成翻译。

一个设备同时可以有多个 PASID，对应多个进程的地址空间。这跟 CPU 的 PCID 非常相似。"

---

## Slide 53-54 — ATS：在设备端缓存翻译

（展示 "Caching Address Translation in Devices"）

"IOTLB 缓存 IOMMU 侧的翻译。但每次 DMA 还是需要经过 IOMMU。能不能更进一步，把翻译缓存到设备端？

这就是 **ATS（Address Translation Service）**。支持 ATS 的设备有自己的 **ATC**（Address Translation Cache），可以缓存翻译结果。

流程是这样的：

1. 设备有地址要翻译 → 发 **ATS Translation Request**（包含 DevID, PASID, VA, 读写权限）
2. IOMMU 查页表 → 返回 **ATS Translation Response**（包含 PA 和属性）
3. 设备把翻译结果缓存到 ATC 中
4. 后续对该地址的 DMA 直接走 **Pre-translated Request**——不再经过 IOMMU 翻译
5. 如果设备不支持 pre-translation → 请求被中止

好处是减少了 IOMMU 的查询压力，对高性能设备尤其重要。"

---

## Slide 55 — PRI：设备端缺页处理

（展示 "Enabling Demand Paging from IO"）

"ATS 能预翻译地址。但如果页不在内存里呢？——设备需要 **page fault** 机制。这就是 **PRI（Page Request Interface）**。

流程：

1. 设备 ATC miss → IOMMU 查页表发现页不在 → 返回 NACK
2. 设备发送 **PPR（Peripheral Page Request）** 给 IOMMU（包含 DevID, PASID, VA）
3. IOMMU 把请求写入 **PPR Log**（内存中的环形缓冲区）
4. OS 的工作线程在 PPR Log 里看到请求 → 处理缺页（换入页面、更新页表）
5. OS 通过 **Command Buffer** 写入 PPR completion 命令
6. IOMMU 通知设备重试原始请求

这意味着设备不需要 OS 在每次 DMA 前就把所有页面 pin 在物理内存里。**按需调页**成为可能——设备可以像 CPU 一样透明地处理缺页。

SVM（Shared Virtual Memory）+ ATS/PRI = GPU 可以像 CPU 一样访问进程的完整虚拟地址空间。

这个机制就是刚才 HSA 中 'Pointer-is-a-Pointer' 的硬件基础。"

---

## Slide 56-57 — 嵌套地址翻译

（展示 "Nested (Two-Level) Address Translation"）

"回到虚拟化场景。

当 IOMMU 加上两层翻译（nested translation）后，地址翻译变成了四级：

**GVA** → Guest Page Table → **GPA** → Host Page Table → **SPA**

客户 OS 的驱动看到的是 GVA（进程虚拟地址）。驱动设置的 DMA buffer 地址是 GPA。IOMMU 需要：
1. 先用 **gCR3 Table** + PASID 找到客户 OS 的页表（GPT），把 GVA 翻译成 GPA
2. 再用 **Device Table** + DevID 找到 VMM 的页表（HPT），把 GPA 翻译成 SPA

客户 OS 和 VMM 各自管理自己的页表层次，互不干扰。IOMMU 硬件做两层 walk。

图中标注了页表 walk 的各级索引——nL4 到 nL1 是 host 页表，GL4 到 GL1 是 guest 页表。

这是真正的 magic 所在。Guest 可以自由管理自己的页表，VMM 管理 host 页表，IOMMU 把两者串起来。"

---

## Slide 58 — 向 IOMMU 发送命令

（展示 "Commands to IOMMU"）

"IOMMU 驱动通过 **Command Buffer** 向 IOMMU 硬件发送命令。

Command Buffer 是一个**内存中的环形缓冲区**。驱动用三个 MMIO 寄存器管理它：
- **Base**：缓冲区基地址
- **Head**：硬件已经处理到的位置
- **Tail**：驱动已经写入的位置

驱动往 Tail 处写入命令，然后更新 Tail 寄存器。IOMMU 硬件从 Head 处取命令执行，执行完后更新 Head 寄存器。

常见命令包括：invalidate IOTLB entry、invalidate device table entry、complete PPR、completion wait 等。"

---

## Slide 59 — TLB Shootdown

（展示 "IOMMU TLB Shootdown"）

"这是 IOMMU 里一个非常重要但很容易出 bug 的操作——**TLB Shootdown**。

当 OS 修改了页表映射关系后，必须在所有相关的 TLB 中把旧映射刷掉。对于 CPU，x86 用 INVLPG 指令。对于 IOMMU，情况更复杂，因为还有设备端的 ATC。

典型的 IOMMU TLB shootdown 需要三步：

1. **Invalidate IOMMU TLB Entry**——OS 写 invalidation 命令到 Command Buffer（包含需要刷新的地址范围）。IOMMU 处理完后更新 Head 指针。

2. **Invalidate Device TLB (ATC) Entry**——OS 写 invalidation 命令，IOMMU 通过 ATS 协议通知设备刷新其 ATC 中的对应条目。设备完成后回复确认。

3. **Completion Wait**——OS 写入 completion wait 命令，等待前面所有命令完成。IOMMU 在确认所有 invalidate 完成后，向指定的 Store Address 写入确认数据，或者触发一个中断。

设计目标是在 '做对了' 和 '性能好' 之间取得平衡。如果你刷多了（比如每次刷新整个 TLB），性能差。如果你刷少了，设备读到的是过期映射，数据可能写到错误的位置。"

---

## Slide 60-61 — 中断重映射和虚拟化

（展示 "Interrupt Remapping and Virtualization"）

"接下来看 IOMMU 如何处理中断。

**中断重映射**：设备发起 MSI/MSI-X 中断消息时，IOMMU 拦截这个消息，查 **Remapping Table**，把原来的中断号 + 目标 CPU 重映射为最终应该投递到的 vCPU + 中断向量。

**中断虚拟化**：更进一步，IOMMU 支持 **Posted Interrupts**——中断直接写入 Guest 的 **vAPIC 页面**。当 Guest 正在运行时，中断直接投递到它的虚拟 APIC，完全不需要 VMM 介入。

图中下半部分展示了 vAPIC 的两种工作模式：
- **Running**——Guest 正在运行，中断直接写入 Guest 的 vAPIC backing page
- **Inactive**——Guest 不在运行，中断记录到 vAPIC log，等 Guest 被调度时再处理

这是一个极大的性能提升——传统方案每次中断都需要 VM exit，而 posted interrupt 实现了真正的 'interrupt without VM exit'。"

---

## Slide 62-63 — IOMMU 硬件设计示例

（展示 "Example of IOMMU Hardware Design"）

"这里是一个典型的 IOMMU 硬件设计示意图。

IOMMU 内部有多级缓存：
- **L1 TLB**、**L2 TLB**——翻译缓存
- **DTC**（Device Table Cache）——设备表缓存
- **ITC**（Interrupt Table Cache）——中断表缓存
- **gPDC**、**gPTC**——Guest 页表相关缓存
- **nPDC**、**nPTC**——Host 页表相关缓存

不同的产品类型有不同的缓存大小设计：

**客户端产品**（笔记本、台式机）：非虚拟化场景为主，IO 隔离为主，工作集较小。缓存不需要太大。

**服务器产品**：虚拟化场景，大量 VM 同时做 DMA，工作集大。需要更大的缓存和更多翻译引擎。"

---

## Slide 64 — 关键数据结构

（展示 "IOMMU's Key Data Structures"）

"我们把 IOMMU 的关键数据结构汇总一下：

- **Base Register**——指向整个 IOMMU 配置结构体的根指针
- **Device Table**——每个 PCIe 设备一个条目（32 bytes），包含 IOTLB 控制、中断信息、GCR3 表指针、Domain ID、host 翻译信息等
- **GCR3 Table**——Guest 页表根指针表（用 PASID 索引）
- **Guest Page Tables**——客户 OS 的页表
- **Host Page Tables**——VMM 的页表
- **Interrupt Remapping Table**——每个条目 128 bits，支持两种模式：基本中断重映射和中断虚拟化（Posted Interrupts）
- **Event Log / PPR Log / Guest vAPIC Log**——各种日志缓冲区
- **Command Buffer**——驱动给 IOMMU 发命令的通道

Device Table Entry 里包含的字段包括：IOTLB 开关、中断表根指针、GCR3 表根指针、Domain ID、host 翻译模式、页表根指针等。每个设备一个。"

---

## Slide 65-72 — 研究方向

（展示 "Research Directions"）

"最后，Arka Basu 会介绍几个当前 IOMMU 方向上的开放研究问题。

**1. 第三方加速器的隔离问题**

今天越来越多的系统集成第三方加速器——NPU、VPU、FPGA 等。这些设备来自不同厂商，有些我们不完全信任。问题是：如何保证一个不信任的加速器不能通过 DMA 破坏系统？

传统方案假设 IOMMU 可以挡住一切恶意 DMA。但如果加速器有自己的缓存（比如 L1/L2 cache），缓存一致性流量可以绕过 IOMMU——这是最新的研究挑战。

**2. 面向设备和加速器的 IOMMU 优化**

当前的 IOMMU 设计类似 CPU MMU，但设备的访问模式和 CPU 很不一样。GPU 是流式访问，NIC 是环形 buffer 访问——一个通用设计不能同时最优。

Malka 等人在 ASPLOS'15 上提出的 **rIOMMU** 利用了设备的可预测访问模式：对于环形 buffer 类的设备，用循环平面表替代多级页表，page walk 只需要一次访存，IOTLB miss 几乎为零。

**3. 用保护换性能**

IOMMU 提供了安全保护，但每次翻译都有代价。有些场景下我们可以选择降低保护级别来换取性能。

例如：受信任的设备可以使用 pre-translated DMA 绕过 IOMMU。问题是：OS 应该根据什么策略来决定信任某个设备？管理员的决定？设备证书？运行时的行为监控？

**4. IOMMULite——面向嵌入式低功耗加速器**

很多嵌入式加速器（FPGA、DSP）想享受虚拟内存带来的编程便利，但完整的 IOMMU 太贵（功耗、面积、延迟）。

Vogel 等人在 CODES'15 上提出轻量级虚拟内存方案——用软件管理 IOMMU，硬件不做 page walk、不做缺页处理。软件显式管理 TLB 内容。简单但高效。

**5. IOMMU 中的干扰问题**

IOMMU 是共享资源——多个设备共享同一个 IOTLB 和 page walker。一个高吞吐量的设备（比如 GPU）可以占满 IOMMU，导致其他设备（比如 NVMe SSD）的 DMA 延迟大幅增加。

QoS 问题来了：如何保证关键设备的 DMA 延迟？IOMMU 需不需要类似 CPU 的 QoS 机制？

**研究工具：**
软件研究很简单——Linux IOMMU driver 是开源的。
硬件研究需要模拟器——gem5 正在加入 IOMMU 模型。"

---

## Slide 73 — 总结

（展示 "Summary"）

"我们总结一下今天的核心内容。

IOMMU 的四个重要角色：

1. **内存保护**——防止恶意或出错的 DMA 设备破坏系统。这是基础。
2. **共享虚拟内存**——让加速器共享 CPU 的地址空间，'Pointer-is-a-Pointer'，通过 PASID + ATS/PRI 实现。
3. **IO 虚拟化**——安全高效的设备直通，让虚拟机获得接近原生的 IO 性能。
4. **Legacy IO 支持 + Secure Boot**——让老设备在新系统里工作，从一开始就保护系统。"

---

## Slide 74 — 参考文献

（展示 "References"）

"最后是一些关键参考文献，如果课后想深入的话：

- AMD IOMMU 规范：http://support.amd.com/TechDocs/48882_IOMMU.pdf
- Olson et al. "Border Control" — MICRO'15（加速器隔离）
- Amit et al. "vIOMMU" — USENIX ATC'11（IOMMU 模拟）
- Malka et al. "rIOMMU" — ASPLOS'15（环形 buffer 优化）
- Markuze et al. "True IOMMU Protection" — ASPLOS'16（DMA 攻击防御）

**谢谢大家。有问题欢迎提问。**"

---

## 附录：课堂提问预设

如果课上有提问，这里准备几个可能的方向：

**Q: IOMMU 和 CPU MMU 的主要区别？**
A: 并发度（IOMMU 高度多线程）、缺页处理（异步 vs 同步）、中断处理能力。详见 Slide 19 的对比表。

**Q: ATS 和 PRI 是必须一起使用的吗？**
A: 不必须。ATS 单独使用可以让设备缓存翻译减少延迟。PRI 在 ATS 基础上增加缺页处理能力，需要设备和 IOMMU 都支持。SVM 场景下两者一起用才能实现透明的按需调页。

**Q: ARM SMMU 和 Intel VT-d 能直接替换吗？**
A: 不能。虽然核心思想相同，但数据结构（页表格式、设备表格式）、编程接口（驱动模型、Command buffer 格式）、特性集都有差异。这也是为什么 IOMMU 驱动在 Linux 内核中是平台相关的。