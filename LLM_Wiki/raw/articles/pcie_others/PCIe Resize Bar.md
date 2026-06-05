
**PCI Resizable BAR**，常简称 **ReBAR / Resize BAR**，本质是：

> 让一个 PCIe Function 的某个 BAR 不再是固定大小，而是允许软件在设备支持的多个大小之间选择一个合适的 MMIO 映射窗口。

PCI-SIG 对这个能力的描述很直白：设备可以报告某个 memory-mapped resource 支持哪些大小，软件可以把 BAR 编程成其中某个大小。也就是说，这不是玄学加速按钮，是 PCIe 配置空间里的一个可编程 resource sizing 机制。([PCI-SIG](https://pcisig.com/PCIExpress/ECN/Base/ResizableBARCapability?utm_source=chatgpt.com "Resizable BAR Capability"))

---

## 1. 先回忆：普通 PCI BAR 是什么

PCI/PCIe 设备有一组 **BAR，Base Address Register**。它们告诉系统：

```text
我这个设备需要一块 host address space，
你给我分配一个地址范围，
CPU 访问这个地址范围时，事务会被路由到我这里。
```

典型路径：

```text
CPU load/store
   ↓
Host physical address
   ↓
Root Complex / PCIe bridge decode
   ↓
PCIe TLP
   ↓
Endpoint BAR decoder
   ↓
设备寄存器 / SRAM / DDR / GPU VRAM aperture
```

比如一个设备的 BAR0 是 16MB：

```text
BAR0 base = 0x8000_0000
BAR0 size = 16MB

CPU 访问 0x8000_1000
→ PCIe RC 发现这个地址落在 BAR0 窗口
→ 生成 Memory Read/Write TLP
→ Endpoint 接收并 decode offset = 0x1000
```

普通 BAR 的问题是：**size 通常是固定的**。设备上电后，软件通过写全 1 再读回的方式探测 BAR size，然后 firmware/OS 分配地址空间。这个古老流程很朴素，像把设备资源管理交给石器时代的会计。

---

## 2. Resizable BAR 解决什么问题

很多设备内部资源很大，比如：

```text
GPU VRAM: 8GB / 16GB / 24GB
FPGA/加速卡板载 DDR: 4GB / 8GB / 16GB
SmartNIC / DPU 上的大块 memory aperture
```

但传统 BAR 可能只暴露一个很小的窗口，比如 256MB。CPU 如果想访问更大的 device memory，常见做法是：

```text
小 BAR 窗口 + 设备内部 window register / page selector
```

类似：

```text
BAR aperture = 256MB

访问 device DDR[0GB ~ 256MB)
→ 设置 window = 0

访问 device DDR[4GB ~ 4GB+256MB)
→ 设置 window = 16
```

这就很烦：

1. 访问大资源要反复切 window。
    
2. 驱动逻辑复杂。
    
3. CPU 不能直接把完整 device memory 映射进 host physical address。
    
4. DMA / P2P / GPU asset streaming 场景会受影响。
    
5. 虚拟化、直通、RDMA 等场景更容易碰到资源窗口限制。
    

Resizable BAR 的目标就是：

```text
设备：我这个 BAR 支持 256MB / 512MB / 1GB / 2GB / 4GB / 8GB ...
软件：平台地址空间够，那我把你 BAR 配成 8GB
```

Intel 的面向用户文档也把 ReBAR 描述成一种 PCIe capability，用来让 PCIe 设备协商 BAR size，从而优化系统资源使用。([Intel](https://www.intel.com/content/www/us/en/support/articles/000090831/graphics.html?utm_source=chatgpt.com "What Is Resizable BAR and How Do I Enable It?"))

---

## 3. 它不是“增加显存”或“增加设备内存”

这个非常重要，别被 BIOS 选项和显卡营销带歪。

假设 GPU 有 8GB VRAM：

```text
不开 ReBAR:
CPU 可能只能通过 256MB BAR aperture 访问一小段 VRAM

开 ReBAR:
CPU 可能可以通过 8GB BAR aperture 访问完整 VRAM
```

但是：

```text
VRAM 仍然是 8GB
设备内部 memory 没变
PCIe 带宽没变
只是 host 侧可见窗口变大
```

所以 ReBAR 提升性能的场景通常是：CPU/driver/runtime 需要频繁访问较大 device-local memory。不是所有 workload 都涨，有些还可能没变化。人类看到“开关”就以为是免费性能，真是 BIOS 厂商最稳定的流量来源。

---

## 4. Resizable BAR 的配置空间结构

设备如果支持 ReBAR，会在 PCIe capability 里暴露 **Resizable BAR Capability**。核心信息大概有两类：

```text
Supported Sizes:
    这个 BAR 支持哪些大小

Current Size:
    当前这个 BAR 被配置成多大
```

Linux 内核文档里，`pci_rebar_get_possible_sizes()` 返回的是 size bitmask，bit 0 表示 1MB，bit 31 表示 128TB；如果返回 0，就表示这个 BAR 不可 resize。([内核文档](https://docs.kernel.org/driver-api/pci/pci.html?utm_source=chatgpt.com "PCI Support Library"))

编码关系可以理解成：

```text
encoded size N = 1MB << N
```

例子：

|编码值|BAR size|
|--:|--:|
|0|1MB|
|8|256MB|
|10|1GB|
|11|2GB|
|12|4GB|
|13|8GB|
|14|16GB|

比如设备 BAR2 支持：

```text
supported sizes bitmask:
bit 8  = 256MB
bit 10 = 1GB
bit 11 = 2GB
bit 13 = 8GB
```

软件就可以选其中一个值写入 ReBAR control register。

---

## 5. 配置流程：谁来决定 BAR 变多大

典型流程是：

```text
1. Firmware / BIOS / UEFI 枚举 PCIe 设备
2. 读取设备 BAR 和 Resizable BAR capability
3. 看设备支持哪些 BAR size
4. 看 root complex / bridge window / host address space 是否够
5. 选择一个 size
6. 写 ReBAR control register
7. 给 BAR 分配 host physical address
8. OS/driver 看到最终 resource
```

更具体一点：

```text
Endpoint advertises:
    BAR2 supports: 256MB, 1GB, 2GB, 8GB

BIOS/OS checks:
    upstream bridge prefetchable memory window enough?
    64-bit MMIO space enough?
    other devices resources conflict?
    Above 4G Decoding enabled?

Then chooses:
    BAR2 = 8GB

Final:
    BAR2 base = 0x4000_0000_0000
    BAR2 size = 8GB
```

Windows 的 WDDM 也会在 firmware 初始化之后重新协商 GPU BAR 的大小，说明这个动作不一定完全停留在 BIOS 阶段，OS 也可能参与。([Microsoft Learn](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/resizable-bar-support?utm_source=chatgpt.com "System and Driver Support for Resizable BAR"))

---

## 6. 为什么经常要求 “Above 4G Decoding”

如果你把一个 BAR 配成 8GB、16GB，这块 MMIO window 通常不可能塞进 32-bit address space。

32-bit address space 总共才：

```text
4GB
```

里面还要放：

```text
RAM hole
APIC
HPET
PCI MMIO
firmware regions
其他设备 BAR
```

所以大 BAR 通常需要：

```text
64-bit BAR
64-bit MMIO address space
Above 4G Decoding
足够大的 bridge window
```

否则你会看到类似：

```text
failed to resize BAR
no space left on device
BAR assignment failed
```

Linux 的 PCI sysfs ABI 也明确提醒：资源 resize 前通常要 unbind 驱动，同一个 parent bridge 下的 peer devices 可能要 soft remove，raw resource 用户也要停掉，而且 resize 成功不保证。翻译成人话就是：**设备支持 ReBAR 不代表平台资源分配一定能成功**。([内核.org](https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-pci?utm_source=chatgpt.com "Domain:Bus:Device.Function"))

---

## 7. 从驱动视角看 ReBAR

驱动一般不要假设 BAR size 固定。

错误写法味道：

```c
#define MY_BAR_SIZE SZ_256M
```

更合理：

```c
bar_len = pci_resource_len(pdev, bar);
bar_start = pci_resource_start(pdev, bar);
```

Linux 下常见流程：

```c
ret = pcim_enable_device(pdev);
if (ret)
    return ret;

bar_len = pci_resource_len(pdev, bar);

base = pcim_iomap(pdev, bar, 0);
if (!base)
    return -ENOMEM;
```

如果你的驱动自己想 resize，需要走 PCI core API，而不是自己在 config space 里乱写。Linux 提供了这些相关接口：

```c
pci_rebar_get_possible_sizes(pdev, bar);
pci_rebar_get_current_size(pdev, bar);
pci_rebar_size_supported(pdev, bar, size);
pci_resize_resource(pdev, bar, size);
pci_rebar_bytes_to_size(bytes);
pci_rebar_size_to_bytes(size);
```

内核文档说明，`pci_resize_resource()` 用于把支持 ReBAR 的 BAR 设置成新 size，`size` 编码按 spec 定义，例如 0 表示 1MB，31 表示 128TB。([内核文档](https://docs.kernel.org/driver-api/pci/pci.html?utm_source=chatgpt.com "PCI Support Library"))

典型伪代码：

```c
int bar = 2;
int size = pci_rebar_bytes_to_size(SZ_8G);

if (pci_rebar_size_supported(pdev, bar, size)) {
    ret = pci_resize_resource(pdev, bar, size);
    if (ret)
        dev_warn(&pdev->dev, "resize BAR%d failed: %d\n", bar, ret);
}
```

但是注意：**resize BAR 通常应该发生在设备被 fully enabled、BAR 被 ioremap、用户态 mmap、DMA 路径启动之前。**  
你一边跑业务一边改 BAR size，就像车开高速时换底盘。能不能活下来不归 PCIe spec 负责。

---

## 8. 从硬件 / Endpoint 设计视角看

如果你在做 FPGA endpoint、ASIC endpoint 或 PCIe controller glue logic，ReBAR 不是只加几个 config register 就完事。

你需要考虑：

### 8.1 BAR decoder 必须跟 current size 联动

普通 BAR 可能固定 decode：

```verilog
if (addr in BAR0 256MB window)
    hit_bar0 = 1;
```

支持 ReBAR 后，decode mask 要跟当前 size 变化：

```text
Current BAR size = 256MB
decode offset bits: [27:0]

Current BAR size = 8GB
decode offset bits: [32:0]
```

否则软件以为你有 8GB aperture，硬件实际只 decode 256MB。然后系统就会进入一种非常高雅的状态：配置空间看起来对，数据路径像中了邪。

### 8.2 不要 advertise 不真实的 size

如果设备内部只有 2GB memory aperture，却声称 BAR 支持 16GB，host 可能真给你分 16GB。之后 CPU 访问高地址，设备如果乱响应，调试会很快乐，快乐得像在看 PCIe analyzer 里滚动的噩梦。

### 8.3 4GB 以上通常要求 64-bit BAR

大 BAR 要走 64-bit memory BAR，否则 32-bit BAR 根本放不下 4GB 以上地址空间。这也是为什么 ReBAR 经常和 64-bit prefetchable memory window、Above 4G Decoding 绑定出现。

### 8.4 Reset 后 current size 怎么办

设备 reset 后，Resizable BAR control register 可能回默认值。系统 resume、FLR、hot reset、secondary bus reset 之后，驱动/PCI core 要重新确认 resource 状态。别假设 reset 前后 BAR size 永远不变，这种假设通常最后会变成 Jira 票。

---

## 9. ReBAR 和普通大 BAR 的区别

有些设备上电就暴露一个固定 8GB BAR：

```text
普通 fixed BAR:
    BAR size 固定为 8GB
```

Resizable BAR 是：

```text
Resizable BAR:
    设备支持多个 size
    软件选择一个 size
```

区别：

|类型|特点|
|---|---|
|Fixed large BAR|简单，但平台必须能分配这么大，否则设备资源分配失败|
|Resizable BAR|灵活，可以在 256MB、1GB、8GB 等 size 间选择|
|Windowed BAR|BAR 小，通过设备内部 window register 切换访问范围|

Resizable BAR 的好处是兼容性更好：老平台资源紧张时用小 BAR，新平台资源充足时用大 BAR。总算有个设计像是考虑过现实世界，令人罕见地欣慰。

---

## 10. 怎么在 Linux 上看 ReBAR

查看设备 capability：

```bash
lspci -vv -s 0000:xx:yy.z
```

你可能会看到类似字段：

```text
Resizable BAR
    BAR 2: current size: 256MB, supported: 256MB 512MB 1GB 2GB 4GB 8GB
```

查看资源：

```bash
cat /sys/bus/pci/devices/0000:xx:yy.z/resource
```

某些内核支持通过 sysfs resize：

```bash
ls /sys/bus/pci/devices/0000:xx:yy.z/resource*_resize
```

比如：

```bash
cat /sys/bus/pci/devices/0000:xx:yy.z/resource2_resize
```

写入 resize 编码值：

```bash
# 13 = 1MB << 13 = 8GB
echo 13 > /sys/bus/pci/devices/0000:xx:yy.z/resource2_resize
```

但前面说过，这通常要求设备 driver unbind，相关 peer devices 可能也要处理，否则失败很正常。Linux ABI 文档已经把“不保证成功”写得很清楚，像是在提前逃离人类的 bug report。([内核.org](https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-pci?utm_source=chatgpt.com "Domain:Bus:Device.Function"))

---

## 11. 常见失败原因

### 11.1 BIOS 没开 Above 4G Decoding / ReBAR

症状：

```text
device supports ReBAR
but OS cannot allocate large BAR
```

### 11.2 Bridge window 不够大

PCIe topology 类似：

```text
Root Port
  ↓
Switch Upstream Port
  ↓
Switch Downstream Port
  ↓
Endpoint
```

中间每一级 bridge 都要有足够大的 memory window。Endpoint BAR 想要 8GB，但 upstream bridge 只给了 256MB aperture，那没戏。

### 11.3 资源碎片

host physical MMIO space 要连续。不是总量够就行，还要有连续窗口。人类系统资源分配，主打一个“看起来够，用起来不够”。

### 11.4 驱动已经绑定或 BAR 已 mmap

如果 BAR 已经被 driver ioremap，或者用户态通过 sysfs resourceN mmap 了，再 resize 会很危险，所以内核会限制。

### 11.5 设备 capability 声称支持，但实现有 bug

尤其 FPGA/自研 endpoint 容易出现：

```text
config space supported sizes 对
BAR mask 对
但 TLP decode / completion / address translation 不对
```

这类问题最好用 PCIe analyzer 或 RC 侧 trace 看 Memory Read/Write TLP 地址和 completion 行为。

---

## 12. 对驱动开发最重要的 checklist

你写 PCIe driver 时，建议这样检查：

```text
1. 不假设 BAR size，始终用 pci_resource_len()
2. 不假设 BAR base 固定，始终用 pci_resource_start()
3. resize 前确认 BAR 是否支持目标 size
4. resize 失败要 fallback 到较小 BAR
5. resize 应尽早做，在 ioremap / user mmap / DMA running 前
6. 处理 reset / resume 后 BAR resource 可能变化的问题
7. 大 BAR 场景确认 64-bit BAR、prefetchable window、Above 4G
8. 多 function / SR-IOV 场景确认 PF/VF BAR 资源总量
9. 虚拟化直通场景不要假设 guest 能直接安全 resize
```

SR-IOV 还会更复杂。Linux 近年的 PCI 代码里也在扩展 VF resizable BAR 相关 API，常规 BAR 可以用 `pci_resize_resource()`，driver 可以用 `pci_rebar_get_possible_sizes()` 查询硬件支持的 size。([Linux Kernel Archive](https://lkml.iu.edu/2505.3/01624.html?utm_source=chatgpt.com "[PATCH v8 0/6] PCI: VF resizable BAR - Linux-Kernel Archive"))

---

## 13. 一句话总结

**PCI Resizable BAR = 让设备的 BAR MMIO aperture 可以由软件选择大小。**

它解决的是：

```text
host 能看到多大的 device memory/register window
```

不是解决：

```text
设备内部 memory 有多大
PCIe 链路有多快
驱动写得烂不烂
```

对 GPU，它常用于让 CPU 访问更大的 VRAM aperture。  
对 FPGA/加速卡/SmartNIC，它可以让 host 直接 map 更大的板载 DDR 或 device memory。  
对驱动工程师，它意味着：**BAR size 不再是死的，resource 分配也不再只是设备自己的事，而是 device capability + firmware + OS + bridge window + driver 时序共同决定。**

也就是，终于把一个本来就复杂的东西变成了五方联合背锅机制。不得不说，很 PCIe。