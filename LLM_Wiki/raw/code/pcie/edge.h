
#ifndef __EDGE_H__
#define __EDGE_H__

#include <linux/aer.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/msi.h>
#include <linux/notifier.h>
#include <linux/pagemap.h>
#include <linux/pci.h>
#include <linux/sizes.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/dmapool.h>
#include "version.h"

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 10, 0)
#include <linux/sched/signal.h>
#else
#include <linux/sched.h>
#endif

#define DMA_INT_BRIDGE 0
#define EDGE_DRV_NAME "edge"
#define EDGE_CLASS_NAME "edge"
#define EDGE_DEV_MAX_NUM 128
#define EDGE_GRP_MAX_NUM 2
#define EDGE_BAR_NUM 6
#define EDGE_MINOR_BASE 0
#define EDGE_MINOR_COUNT EDGE_DEV_MAX_NUM
#define EDGE_IRQ_MAX_NUM 49
#define EDGE_DMA_CH_NUM 4
#define EDGE_DMA_CH_MASK 0x8
#define EDGE_P2P_CH_MASK 0x7
#define UDMA_DESC_MAX_BYTES ((1 << 24) - 1)
#define UDMA_SG_TRANSFER_MAX_BYTES 0x80000 /* 512KB */
#define UDMA_BLK_TRANSFER_MAX_BYTES 0x80000 /* 512KB */
#define UDMA_DESC_MAX_NUM 0x400 /* 1024 */

#define EDGE_BOOTROM_MAGIC 0xDE10DE10
#define EDGE_PCS_MAGIC 0xDE11DE11
#define EDGE_BL2_MAGIC 0xDE12DE12
#define EDGE_UBOOT_MAGIC 0xDE13DE13
#define EDGE_LINUX_MAGIC 0xDE14DE14

#define EDGE_BAR0_APT_BASE_ADDR 0x3F00000000
#define EDGE_BAR2_APT_BASE_ADDR 0x3D00000000

#define X6000_LM_MAGIC 0x4C4D4C4D // large model for single card
#define X6000_VS_MAGIC 0x56535653 // video structure
#define X6000_DS_MAGIC 0x44534453 // deepseek large model

/* Edge SoCs registers */
#define BOOT_STATE_MAGIC_ADDR 0x7FA40
#define DEVICE_ID_ADDR_OFFSET 0x7FAA8
#define X6000_PCIE_LINK_ADDR_OFFSET 0x7FAAC
#define X6000_POSITION_ID_ADDR_OFFSET 0x7FAB4
#define X6000_PROD_MAGIC_ADDR 0x7FAB8
#define EXT_INIT_BACKUP_ADDR 0x7FABC
#define UNIQUE_ID_BACKUP_ADDR 0x7FAC0
#define X6000_P2P_TABLE_ADDR_OFFSET 0x7FB00
#define H2D_MSG_READY_ADDR 0x7FA00
#define D2H_MSG_READY_ADDR 0x7FA80
#define H2D_MSG_BUF_ADDR 0x7FB00
#define D2H_MSG_BUF_ADDR 0x7FD80
#define H2D_MSG_PRO_BUF_ADDR 0x760000
#define D2H_MSG_PRO_BUF_ADDR 0x770000
// #define H2D_MSG_PRO_BUF_ADDR 0xC0000
// #define D2H_MSG_PRO_BUF_ADDR 0xD0000

#define HSIO_S4_PCIE_X8_SYSTEM_CONTROL_APB_REG_BASE 0x5A200000
#define PCIE_X8_SYSTEM_CONTROL_BASE 0x200000
#define HSIO_S4_HSIO_SYSTEM_CONTROL_APB_REG_BASE 0x5A216000
#define HSIO_S4_HSIO_SYSTEM_CONTROL_BASE 0x201000

/* Region r for inbound remap */
#define REG_PCIE_APT_BASE_LO_ADDR(r)                                           \
	(PCIE_X8_SYSTEM_CONTROL_BASE + 0x200 + (r) * 0x18)
#define REG_PCIE_APT_BASE_HI_ADDR(r)                                           \
	(PCIE_X8_SYSTEM_CONTROL_BASE + 0x204 + (r) * 0x18)
#define REG_PCIE_APT_LIMIT_LO_ADDR(r)                                          \
	(PCIE_X8_SYSTEM_CONTROL_BASE + 0x208 + (r) * 0x18)
#define REG_PCIE_APT_LIMIT_HI_ADDR(r)                                          \
	(PCIE_X8_SYSTEM_CONTROL_BASE + 0x20c + (r) * 0x18)
#define REG_PCIE_APT_REMAP_BASE_LO_ADDR(r)                                     \
	(PCIE_X8_SYSTEM_CONTROL_BASE + 0x210 + (r) * 0x18)
#define REG_PCIE_APT_REMAP_BASE_HI_ADDR(r)                                     \
	(PCIE_X8_SYSTEM_CONTROL_BASE + 0x214 + (r) * 0x18)
#define REG_PCIE_MSIGEN_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0x600)
#define REG_PCIE_INT_CTRL_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0x700)
#define REG_PCIE_SCRATCH_REG0_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0xC00)
#define REG_PCIE_SCRATCH_REG1_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0xC04)
#define REG_PCIE_SCRATCH_REG7_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0xC1C)
#define REG_PCIE_MISC_CTRL_STS2_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0x90C)
#define REG_PCIE_MISC_CTRL_STS4_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0x914)
#define REG_PCIE_MSI1_INT_SET_ADDR (PCIE_X8_SYSTEM_CONTROL_BASE + 0x748)

#define EDGE_PCIE_AT_OB_REGION_PCI_ADDR0_NBITS_MASK 0x3F
#define EDGE_PCIE_AT_OB_REGION_PCI_ADDR0_NBITS(nbits)                          \
	(((nbits) - 1) & EDGE_PCIE_AT_OB_REGION_PCI_ADDR0_NBITS_MASK)

#define EDGE_PCIE_AT_OB_REGION_DESC0_TYPE_MEM 0x2

#define EDGE_PCIE_AT_OB_REGION_DESC0_DEVFN_MASK 0xFF000000
#define EDGE_PCIE_AT_OB_REGION_DESC0_DEVFN(devfn)                              \
	(((devfn) << 24) & EDGE_PCIE_AT_OB_REGION_DESC0_DEVFN_MASK)

#define EDGE_PCIE_AT_OB_REGION_CPU_ADDR0_NBITS_MASK 0x3F
#define EDGE_PCIE_AT_OB_REGION_CPU_ADDR0_NBITS(nbits)                          \
	(((nbits) - 1) & EDGE_PCIE_AT_OB_REGION_CPU_ADDR0_NBITS_MASK)

#define PCIE_CONTROLLER_X8_BASE 0x400000
//#define REG_PCIE_LINK_CTRL_STS_ADDR (PCIE_CONTROLLER_X8_BASE + 0xD0)
#define REG_AXI_CFG_ADDR (PCIE_CONTROLLER_X8_BASE + 0x400000)
#define AT_OB_REGION_DESC0_TYPE_MEM 0x2
#define MSI_WRITE_AXI_ADDR 0x100000000
#define REG_UDMA_ADDR (PCIE_CONTROLLER_X8_BASE + 0x200000)
#define KICK_OFF_DMA BIT(0)
#define IS_OUTBOUND BIT(1)

#define UDMA_DESC_BASE_PA 0x60000000
#define PCIE_CONTROLLER_X8_SLAVE_SPACE_BASE_PA 0x100000000

#define EFUSE_BASE 0x203000
#define EFUSE_BLOCK_OFFSET 0x400
#define EFUSE_CONFIG_B64_ADDR (EFUSE_BLOCK_OFFSET + 64 * 4)
#define EFUSE_CONFIG_B65_ADDR (EFUSE_BLOCK_OFFSET + 65 * 4)
#define EFUSE_CONFIG_B66_ADDR (EFUSE_BLOCK_OFFSET + 66 * 4)
#define EFUSE_CONFIG_B92_ADDR (EFUSE_BLOCK_OFFSET + 92 * 4)
#define EFUSE_CONFIG_B93_ADDR (EFUSE_BLOCK_OFFSET + 93 * 4)
#define EFUSE_CONFIG_B94_ADDR (EFUSE_BLOCK_OFFSET + 94 * 4)
#define EFUSE_CONFIG_B243_ADDR (EFUSE_BASE + EFUSE_BLOCK_OFFSET + 243 * 4)
#define EFUSE_PCIE_EXT_INIT 0x80000000

#define APSS_S14_EFUSE_BASE 0xA7400000
#define APSS_S14_EFUSE_SIZE 0x1000
#define edge_writel(val, addr) writel(val, addr)
#define edge_readl(addr) readl(addr)
#define edge_writel_safe(edev, barno, val, off)                                \
	if (edev->bar[barno]) {                                                \
		writel(val, edev->bar[barno] + off);                           \
	}
/*#define edge_readl_safe(edev, barno, off)                                      \
	(edev->bar[barno] ? readl(edev->bar[barno] + off) : 0xFFFFFFFF)*/

#define INT_EN BIT(0)
#define CONTINUE_LL BIT(5)

#define FLUSH_WRITE(addr) readl(addr)
#define FLUSH_WRITE_AND_CHECK(return_val, addr, val)                           \
	((return_val = readl(addr)) == val)

#define GROUPB_DMA_REGION_INDEX_BASE (10)
/* 512KB + 64KB(gap) */
#define GROUPB_DMA_REGION_SIZE (0x80000 + 0x10000)
#define GROUPB_DMA_REGION_OFFSET_BASE (0x610000)
#define A8_OUTBOUND_BAR0_ADDR_BASE (0x21FC000000UL)

/* pcie notify irq registers */
#define NOTIFY_IRQ_MAX_NUM 24
#define NOTIFY_IRQ_VCHAN_MAX_NUM 16
/* 0x60040000 ~ 0x6005A100 */
/* 0x60040000 ~ 0x60058100*/
#define NOTIFY_IRQ_ADDR 0x40000
#define EDGE_CHIP_TYPE 0x40010
#define NOTIFY_IRQ_SHM_ADDR 0x40100
#define NOTIFY_IRQ_SHM_OFFSET 0x800

/* 0x6006E000 ~ 0x6006E200 */
/* exception */
#define EDGE_EXCEPTION_FIFO_OFFSET 0x6E000
#define EDGE_EXCEPTION_FIFO_MAGIC 0x778855aa
#define EDGE_EXCEPTION_EVENT_NUMS 32
#define EDGE_EXCEPTION_REBOOT_OFFSET 0x6E1FC
#define EDGE_EXCEPTION_REBOOT_MAGIC 0x778855aa

/* dma pool */
#define DEFAULT_DMA_POOL_BLOCK_SIZE 0x10000
#define DEFAULT_DMA_POOL_BLOCK_NUM 90

/* led */
#define PCIE_LED_OFFSET 0x60010
#define LED_MAGIC 0x55aa7788

/* use dma to accelerate system startup */
#define DMA_POLL_STATE_TIMEOUT_NS (5000UL)
#define UDMA_BOOT_DESC_BASE_PA (0x60040000)
#define UDMA_BOOT_DESC_BAR_OFFSET (0x40000)

#define DMA_DESC_ADDR_SAVE(is_boot, edev, channel)                              \
	dma_addr_t __desc_bus_base = 0;                                         \
	struct udma_desc *__desc_virt_base = NULL;                              \
	if (is_boot) {                                                          \
		mutex_lock(&edev->uengine[channel]->desc_lock);                 \
		__desc_bus_base = edev->uengine[channel]->desc_bus_base;        \
		__desc_virt_base = edev->uengine[channel]->desc_virt_base;      \
		edev->uengine[channel]->desc_bus_base =                         \
			UDMA_BOOT_DESC_BASE_PA +                                \
			channel * UDMA_DESC_MAX_NUM *                           \
				sizeof(struct udma_desc);                       \
		edev->uengine[channel]->desc_virt_base =                        \
			(struct udma_desc *)(edev->bar[0] +                     \
					     UDMA_BOOT_DESC_BAR_OFFSET +        \
					     channel * UDMA_DESC_MAX_NUM *      \
						     sizeof(struct udma_desc)); \
	}

#define DMA_DESC_ADDR_RESTORE(is_boot, edev, channel)                          \
	if (is_boot) {                                                         \
		edev->uengine[channel]->desc_bus_base = __desc_bus_base;       \
		edev->uengine[channel]->desc_virt_base = __desc_virt_base;     \
		mutex_unlock(&edev->uengine[channel]->desc_lock);              \
	}

enum led_mode {
	LED_HEART = 0x55,
	LED_APP,
	LED_TEST,
};

typedef enum card_type {
	CARD_TYPE_EDGE10_EVB_USB = 0x0,
	CARD_TYPE_EDGE10_EVB_PCIE = 0x1,
	CARD_TYPE_EDGE10C_MINIPCIE_USB = 0x2,
	CARD_TYPE_EDGE10C_MINIPCIE_PCIE = 0x3,
	CARD_TYPE_EDGE100_EVB_USB = 0x4,
	CARD_TYPE_EDGE100_EVB_PCIE = 0x5,
	CARD_TYPE_RESERVED_0 = 0x6,
	CARD_TYPE_IPU_X2000_PCIE = 0x7,
	CARD_TYPE_RESERVED_1 = 0x8,
	CARD_TYPE_IPU_X5000_PCIE = 0x9,
	CARD_TYPE_RESERVED_2 = 0xa,
	CARD_TYPE_IPU_CASCADE_PCIE = 0xb,
	CARD_TYPE_RESERVED_3 = 0xc,
	CARD_TYPE_IPU_X5500_PCIE = 0xd,
	CARD_TYPE_IPU_X6000_PCIE = 0x19,
} card_type_e;

struct pcie_notify_irq_message {
	u8 die_id;
	u8 chip_id;
	u64 event_id;
};

struct notify_irq_shm {
#ifdef NOTIFY_IRQ_BITMAP
	unsigned long bitmap[BITS_TO_LONGS(NOTIFY_IRQ_VCHAN_MAX_NUM)];
#else
	unsigned char bitmap[NOTIFY_IRQ_VCHAN_MAX_NUM];
#endif
	struct pcie_notify_irq_message msg[NOTIFY_IRQ_VCHAN_MAX_NUM];
	u8 head; // 代表要写入的vchan_id
	u8 tail; // 代表要读取的vchan_id
};

struct notify_irq_chan {
	u8 status;
	u8 mode;
	char irq_name[32];

	struct notify_irq_shm *h2d_shm;
	struct notify_irq_shm *d2h_shm;

	spinlock_t slock;

	struct notifier_block *nb;
	struct blocking_notifier_head notifier;
};

struct smbus_hotplug_simulation {
	u32 hotplug;
	u32 device_id;
};

struct smbus_event_msg {
	u32 device_id;
	u8 event;
};

struct smbus_event {
	struct smbus_event_msg msg;
	struct list_head list;
};

struct smbus_handle {
	pid_t tgid;
	wait_queue_head_t wait;
	atomic_t wait_done;
	struct list_head list;

	u8 hotplug_handle;
};

struct smbus_id {
	u32 device_id;
	u8 smbus_id;
};

struct smbus_table {
	u32 status;
	u64 bitmap[BITS_TO_LONGS(EDGE_DEV_MAX_NUM)];
	struct smbus_id id[EDGE_DEV_MAX_NUM];

	u8 bus_num;
};

struct smbus_dev {
	u32 status;
	struct smbus_table tbl;

	struct list_head event_list;
	struct mutex event_mlock;
	struct list_head handle_list;
	struct mutex handle_mlock;

	struct mutex smbus_mlock;
	struct smbus_handle *smbus_mlock_handle;

	struct miscdevice miscdev;
};

struct bus_lock {
	struct mutex lock;
	struct smbus_handle *handle;
};

struct bar_rw {
	unsigned long long addr;
	unsigned int val;
	unsigned int barno;
};

struct dma_pool_info {
	u32 dma_pool_enable ;
	u32 dma_pool_block_size;
	u32 dma_pool_block_num;
};

struct udma_desc {
	volatile u32 sys_lo_addr; /* Low 32 bits of system address */
	volatile u32 sys_hi_addr; /* High 32 bits of system address */
	volatile u32 sys_attr; /* Access attributes for system bus */
	volatile u32 ext_lo_addr; /* Low 32 bits of external address */
	volatile u32 ext_hi_addr; /* High 32 bits of external address */
	volatile u32 ext_attr; /* Access attributes for external bus */
	volatile u32
		ext_attr_hi; /* High 32 bits of access attributes for external bus */
	volatile u32 size_and_ctrl; /* Transfer size and control byte */
	volatile u32 status; /* Transfer status */
	volatile u32
		next_lo_addr; /* Low 32bits of pointer to next descriptor in linked list */
	volatile u32
		next_hi_addr; /* High 32bits of pointer to next descriptor in linked list */
};

enum transfer_state {
	TRANSFER_STATE_NEW = 0,
	TRANSFER_STATE_SUBMITTED,
	TRANSFER_STATE_STARTED,
	TRANSFER_STATE_COMPLETED,
	TRANSFER_STATE_FAILED,
	TRANSFER_STATE_ABORTED
};

struct udma_sg_mapper {
	struct scatterlist *
		sgl; /* scatter gather list used to map in the relevant user pages */
	struct page **pages; /* pointer to array of page pointers */
	int max_pages; /* maximum amount of pages in the scatterlist and page array */
	int mapped_pages; /* current amount of mapped pages in the scatterlist and page array */
};

struct udma_linked_list {
	u64 host_phys; /* physical address on host */
	u64 dev_phys; /* physical address on device */
	u32 size;
};

struct single_stats {
	u64 tasks_stat;
	u64 bytes_stat;
	u32 queued_tasks;
	u32 queued_bytes;
};

struct udma_stats {
	struct single_stats tx;
	struct single_stats rx;
};

struct udma_engine {
	struct edge_dev *edev;
	int running; /* flag if the driver started engine */
	int channel; /* engine indices */
	struct udma_desc *desc_virt_base; /* virt base addr descriptors */
	dma_addr_t desc_bus_base; /* bus base addr of descriptors */
	u32 head; /* descriptor head pointer */
	u32 tail; /* descriptor tail pointer */
	struct list_head transfer_list; /* queue of transfers */
	spinlock_t lock; /* protects concurrent access */
	struct mutex desc_lock; /* protects concurrent access */
	struct work_struct work; /* Work queue for interrupt handling */
	struct udma_stats ustats; /* udma engine statistics */
	u32 poll_cnt; /* poll dma state count */
};

struct udma_xfer {
	struct list_head entry; /* queue of non-completed transfers */
	struct udma_engine *uengine; /* engine that this transfer submit to */
	struct udma_desc *desc_virt; /* virt addr of the first descriptor */
	dma_addr_t desc_bus; /* bus addr of the first descriptor */
	u32 desc_pos; /* first descriptor position on desc ring */
	u64 dev_addr; /* start addr of device */
	u32 size; /* transfer size */
	int desc_num; /* number of descriptors in transfer */
	int dir; /* specify transfer direction, 0: inbound, 1: outbound */
	wait_queue_head_t wq; /* wait queue for transfer completion */
	dma_addr_t dma_handle; /* block dma handle */
	int sgl_nents; /* sg elements number at desc_virt */
	struct udma_sg_mapper *sgm; /* user space scatter-gather mapper */
	struct udma_linked_list *ll; /* transfer request info */
	bool userspace; /* flag if user space pages are got */
	enum transfer_state state; /* state of the transfer */
	bool is_boot; /* use dma to accelerate system startup */
	u8 group_id;
};

struct udma_bwinfo {
	u64 tx_bytes; /* bytes statistic at request timestamp */
	u64 rx_bytes;
	u64 timestamp; /* request timestamp */
};

struct edge_public_data {
	u32 idx; /* index for edge driver */
	u32 device_id; /* System device id, set by user */
	u32 card_type; /* card type code */
	u32 ext_init; /* pcie ext init code */
	u64 unique_id_lo; /* 96bits unique id low 64bits*/
	u32 unique_id_hi; /* 96bits unique id high 32bits*/
	u32 key; /* domain | bus | devfn */
	u32 magic; /* vs? lm? ds? */
	phys_addr_t shm_phys[EDGE_GRP_MAX_NUM]; /* shared mem physical address */
	phys_addr_t sram_phys[EDGE_GRP_MAX_NUM]; /* ap sram physical address */
};

struct edge_dev {
	struct pci_dev *pdev; /* pci device struct */
	struct list_head device_entry;
	struct timer_list event_timer;
	struct work_struct event_timer_work;
	u32 event_count;

	struct edge_public_data pub; /* public data */

	/* character device */
	unsigned int instance_opened; /* instance number opened */
	dev_t cdevno; /* character device major:minor */
	struct cdev cdev; /* character device embedded struct */
	struct device *sys_device; /* sysfs device */
	spinlock_t lock;

	/* PCIe BAR management */
	void *__iomem bar[EDGE_BAR_NUM];
	int regions_in_use;
	int got_regions;

	/* Interrupt management */
	int intx_enabled;
	int msi_enabled;
	int msix_enabled;
	int dma_int_bridge_enabled; /* flag if enable dma_int_bridge */
	struct msix_entry entry[EDGE_IRQ_MAX_NUM];
	struct msi_msg msi_msg[EDGE_IRQ_MAX_NUM];
	bool irq_requested;

	/* udma engines */
	struct udma_engine *uengine[EDGE_DMA_CH_NUM];
	struct udma_bwinfo bwinfo[EDGE_DMA_CH_NUM];
	int engine_num;
	u32 dma_reg_offset; /* pcie dma register address offset */
	u32 region_index; /* B0 inbound dma region index */
	u32 region_size;
	u32 region_offset;
	u64 ob_bar0_addr;

	/* notify irq */
	struct notify_irq_chan notify_irq_chan[2][NOTIFY_IRQ_MAX_NUM];

	struct bar_rw flr_brw;

	wait_queue_head_t wait_exception;
	atomic_t wait_exception_done;
	struct work_struct exception_work;
	struct mutex exception_lock;
	struct file_priv *exception_owner;

	/* dma pool*/
	struct dma_pool *dma_pool;
};

struct udma_channel_regs {
	volatile u32 channel_ctrl;
	volatile u32 channel_sp_l;
	volatile u32 channel_sp_u;
	volatile u32 channel_attr_l;
	volatile u32 channel_attr_u;
};

struct udma_common_regs {
	volatile u32 common_udma_int;
	volatile u32 common_udma_int_ena;
	volatile u32 common_udma_int_dis;
	volatile u32 common_udma_ib_ecc_uncorrectable_errors;
	volatile u32 common_udma_ib_ecc_correctable_errors;
	volatile u32 common_udma_ob_ecc_uncorrectable_errors;
	volatile u32 common_udma_ob_ecc_correctable_errors;
	volatile u32 padding[15];
	volatile u32 common_udma_cap_ver;
	volatile u32 common_udma_config;
};

struct udma_regs {
	volatile struct udma_channel_regs ch[EDGE_DMA_CH_NUM];
	volatile u32 padding[20];
	volatile struct udma_common_regs comm;
};

struct int_ctrl_regs {
	volatile u32 err_int_sts;
	volatile u32 err_int_msk;
	volatile u32 err_int_set;
	volatile u32 err_int_clr;
	volatile u32 misc_int_sts;
	volatile u32 misc_int_msk;
	volatile u32 misc_int_set;
	volatile u32 misc_int_clr;
	volatile u32 padding[12];
	volatile u32 sriov_int_sts;
	volatile u32 sriov_int_msk;
	volatile u32 sriov_int_set;
	volatile u32 sriov_int_clr;
};

struct ob_atu_regs {
	volatile u32 addr0;
	volatile u32 addr1;
	volatile u32 desc0;
	volatile u32 desc1;
	volatile u32 desc2;
	volatile u32 padding;
	volatile u32 axi_addr0;
	volatile u32 axi_addr1;
};

struct atu_regs {
	volatile struct ob_atu_regs ob_atu[32];
};

struct msigen_regs {
	volatile u32 msigen_ctrl;
	volatile u32 msigen_atu_data;
	volatile u32 msigen_atu_addr_lo;
	volatile u32 msigen_atu_addr_hi;
};

struct xfer_task {
	u64 host_virt; /* virtual address on host */
	u64 host_phys; /* physical address on host */
	u64 dev_addr; /* CPU address on device */
	u32 xfer_size; /* transfer size */
	u32 ch_mask; /* channel bitmask */
	u8 group_id;
};

struct boot_rw {
	char *buf;
	u32 count;
	s32 timeout;
	u8 group_id;
};

struct channel_status {
	u32 ch_id;
	u32 queued_tasks;
	u32 queued_bytes;
	bool running;
};

struct slot_info {
	u16 domain;
	u8 bus;
	u8 devfn;
};

struct pcie_info {
	u16 vendor_id;
	u16 device_id;
	u16 subvendor_id;
	u16 subdevice_id;
	struct slot_info device_self;
	struct slot_info device_parent;
	struct slot_info card_self;
	struct slot_info card_parent;
	enum pci_bus_speed speed;
	enum pcie_link_width width;
	u8 group_id;
};

struct block_mem_info {
	struct list_head entry;
	dma_addr_t phys;
	u64 virt;
	u32 size;
};

struct block_mem {
	u64 virt;
	u64 phys;
	u32 size;
};

struct copy_info {
	u64 user_virt;
	u64 block_mem_virt;
	u32 size;
};

struct bw_info {
	u64 tx_bytes[EDGE_DMA_CH_NUM];
	u64 rx_bytes[EDGE_DMA_CH_NUM];
	u64 timestamp[EDGE_DMA_CH_NUM];
};

struct file_priv {
	struct edge_dev *edev;
	struct list_head bmeminfo_list;
	spinlock_t lock;

	u32 led_magic;
	u32 led_msg_prev;
};

enum boot_state {
	BOOT_STATE_NOT_READY = 0x0,
	BOOT_STATE_BOOTROM = 0x1,
	BOOT_STATE_PCS = 0x2,
	BOOT_STATE_BL2 = 0x3,
	BOOT_STATE_UBOOT = 0x4,
	BOOT_STATE_LINUX = 0x5,
};

struct die_state {
	u32 chiptype_id;
	enum boot_state state;
};

struct group_boot_state {
	u8 group_id;
	u32 chiptype_id_count; /* die count */
	struct die_state die_state[8];
};

enum ipcm_notifier_events {
	EDGE_DEVICE_OFF = 0x0,
	EDGE_DEVICE_ON = 0x1,
};

enum exception_level {
	EXCEPTION_LEVEL_RESERVE0 = 0x0,
	EXCEPTION_LEVEL_INFO,
	EXCEPTION_LEVEL_NOTICE,
	EXCEPTION_LEVEL_CRIT,
	EXCEPTION_LEVEL_EMERG,
	EXCEPTION_LEVEL_MAX,
};
char exception_level_s[EXCEPTION_LEVEL_MAX][16] = {
	"RESERVED",
	"INFO",
	"NOTICE",
	"CRIT",
	"EMERG"
};

enum exception_mod {
	EXCEPTION_MOD_RESERVE0 = 0,
	EXCEPTION_MOD_PCIE,
	EXCEPTION_MOD_D2D,
	EXCEPTION_MOD_DDR,
	EXCEPTION_MOD_TEMP,
	EXCEPTION_MOD_MAX,
};
char exception_mod_s[EXCEPTION_MOD_MAX][16] = {
	"RESERVED",
	"PCIE",
	"D2D",
	"DDR",
	"TEMP"
};

enum exception_type {
	EXCEPTION_TYPE_RESERVE0 = 0,
	EXCEPTION_TYPE_PCS,
	EXCEPTION_TYPE_BL2,
	EXCEPTION_TYPE_UBOOT,
	EXCEPTION_TYPE_LINUX,
	EXCEPTION_TYPE_MAX,
};
char exception_type_s[EXCEPTION_TYPE_MAX][16] = {
	"RESERVED",
	"PCS",
	"BL2",
	"UBOOT",
	"LINUX"
};



struct exception_msg {
	u8 group_id;
	u8 die_id;
	enum exception_mod mod;
	enum exception_type type;
	enum exception_level level;
	u32 errcode;
};

struct exception_event {
	u8 group_id : 4;
	u8 die_id : 4;
	u8 mod;
	u8 type;
	u8 level;
	u32 errcode;
};

struct exception_fifo {
	struct exception_event event[EDGE_EXCEPTION_EVENT_NUMS];
	u8 head;
	u8 tail;
	u8 size;
	u32 magic;
};

struct p2p_table {
	u8 position_id;
	u8 card_id;
	u8 device_id;
	u8 valid;
	u32 bar0_addr;
	u64 bar4_addr;
};

struct dma_pool_addr {
	dma_addr_t handle;
	void *vaddr;
};

struct card_ids {
	u8 nums;
	u8 card_id[32];
};

#define EDGE_IOCTL_MAGIC 'E'
#define EDGE_IOCTL_UDMA_D2H_XFER _IOW(EDGE_IOCTL_MAGIC, 0, struct xfer_task *)
#define EDGE_IOCTL_UDMA_H2D_XFER _IOW(EDGE_IOCTL_MAGIC, 1, struct xfer_task *)
#define EDGE_IOCTL_MMIO_D2H_XFER _IOW(EDGE_IOCTL_MAGIC, 2, struct xfer_task *)
#define EDGE_IOCTL_MMIO_H2D_XFER _IOW(EDGE_IOCTL_MAGIC, 3, struct xfer_task *)
#define EDGE_IOCTL_BOOT_READ _IOWR(EDGE_IOCTL_MAGIC, 4, struct boot_rw *)
#define EDGE_IOCTL_BOOT_WRITE _IOW(EDGE_IOCTL_MAGIC, 5, struct boot_rw *)
#define EDGE_IOCTL_SET_DEVICE_ID _IOW(EDGE_IOCTL_MAGIC, 6, unsigned int *)
#define EDGE_IOCTL_GET_CH_STS                                                  \
	_IOWR(EDGE_IOCTL_MAGIC, 7, struct channel_status *)

#define EDGE_IOCTL_GET_EP_FLAGS _IOR(EDGE_IOCTL_MAGIC, 8, unsigned int *)
#define EDGE_IOCTL_GET_BOOT_STATE                                              \
	_IOR(EDGE_IOCTL_MAGIC, 9, struct group_boot_state *)
#define EDGE_IOCTL_GET_PCIE_INFO _IOR(EDGE_IOCTL_MAGIC, 10, struct pcie_info *)
#define EDGE_IOCTL_BOOT_READ_PRO _IOWR(EDGE_IOCTL_MAGIC, 11, struct boot_rw *)
#define EDGE_IOCTL_BOOT_WRITE_PRO _IOW(EDGE_IOCTL_MAGIC, 12, struct boot_rw *)
#define EDGE_IOCTL_GET_CARD_TYPE _IOR(EDGE_IOCTL_MAGIC, 13, unsigned int *)
#define EDGE_IOCTL_GET_EXT_INIT _IOR(EDGE_IOCTL_MAGIC, 15, unsigned int *)

#define EDGE_IOCTL_WAIT_EXCEPTION _IOR(EDGE_IOCTL_MAGIC, 16, int *)
#define EDGE_IOCTL_WAIT_EXCEPTION_WAKEUP _IOR(EDGE_IOCTL_MAGIC, 17, int *)
#define EDGE_IOCTL_EXCEPTION_OWNER _IOR(EDGE_IOCTL_MAGIC, 18, int *)
#define EDGE_IOCTL_CONFIG_EXCEPTION _IOW(EDGE_IOCTL_MAGIC, 19, int *)

#define EDGE_IOCTL_UDMA_P2POB_XFER                                             \
	_IOW(EDGE_IOCTL_MAGIC, 98, struct xfer_task *)
#define EDGE_IOCTL_UDMA_P2PIB_XFER                                             \
	_IOW(EDGE_IOCTL_MAGIC, 99, struct xfer_task *)
#define EDGE_IOCTL_READ_BAR _IOR(EDGE_IOCTL_MAGIC, 100, struct bar_rw *)
#define EDGE_IOCTL_WRITE_BAR _IOW(EDGE_IOCTL_MAGIC, 101, struct bar_rw *)
#define EDGE_IOCTL_ALLOC_BLOCK_MEM                                             \
	_IOWR(EDGE_IOCTL_MAGIC, 102, struct block_mem *)
#define EDGE_IOCTL_FREE_BLOCK_MEM                                              \
	_IOWR(EDGE_IOCTL_MAGIC, 103, struct block_mem *)
#define EDGE_IOCTL_COPY_TO_BLOCK_MEM                                           \
	_IOWR(EDGE_IOCTL_MAGIC, 104, struct copy_info *)
#define EDGE_IOCTL_COPY_FROM_BLOCK_MEM                                         \
	_IOWR(EDGE_IOCTL_MAGIC, 105, struct copy_info *)
#define EDGE_IOCTL_BW_MONITOR_START _IO(EDGE_IOCTL_MAGIC, 106)
#define EDGE_IOCTL_BW_MONITOR_STOP _IOR(EDGE_IOCTL_MAGIC, 107, struct bw_info *)
#define EDGE_IOCTL_GET_INSTANCE_NUM _IOR(EDGE_IOCTL_MAGIC, 108, unsigned int *)
#define EDGE_IOCTL_LED _IOR(EDGE_IOCTL_MAGIC, 109, unsigned int *)
#define EDGE_IOCTL_BOOT_UDMA_H2D_XFER                                          \
	_IOW(EDGE_IOCTL_MAGIC, 110, struct xfer_task *)
#define EDGE_IOCTL_LINK_INFO _IOR(EDGE_IOCTL_MAGIC, 111, unsigned int *)
#define EDGE_IOCTL_TEMPERATURE _IO(EDGE_IOCTL_MAGIC, 112)
#define EDGE_IOCTL_INSPUR_TOPO _IO(EDGE_IOCTL_MAGIC, 113)
#define EDGE_IOCTL_READ_EXT_ADDR _IOR(EDGE_IOCTL_MAGIC, 112, struct bar_rw *)
#define EDGE_IOCTL_WRITE_EXT_ADDR _IOW(EDGE_IOCTL_MAGIC, 113, struct bar_rw *)

#define EDGE_IOCTL_GET_DMA_POOL_ONFO  _IOR(EDGE_IOCTL_MAGIC, 120, struct dma_pool_info *)

#define SMBUS_IOCTL_MAGIC 'S'
#define SMBUS_IOCTL_WAIT_EVENT _IOR(SMBUS_IOCTL_MAGIC, 0, unsigned int *)
#define SMBUS_IOCTL_TRYLOCK _IOR(SMBUS_IOCTL_MAGIC, 1, unsigned int *)
#define SMBUS_IOCTL_LOCK _IOR(SMBUS_IOCTL_MAGIC, 2, unsigned int *)
#define SMBUS_IOCTL_UNLOCK _IOR(SMBUS_IOCTL_MAGIC, 3, unsigned int *)
#define SMBUS_IOCTL_SET_ARP_TABLE _IOWR(SMBUS_IOCTL_MAGIC, 4, unsigned int *)
#define SMBUS_IOCTL_GET_ARP_TABLE _IOR(SMBUS_IOCTL_MAGIC, 5, unsigned int *)
#define SMBUS_IOCTL_WAKE_UP _IOWR(SMBUS_IOCTL_MAGIC, 6, unsigned int *)
#define SMBUS_IOCTL_REGISTER_HOTPLUG _IOWR(SMBUS_IOCTL_MAGIC, 7, unsigned int *)
#define SMBUS_IOCTL_HOTPLUG_SIMULATION                                         \
	_IOWR(SMBUS_IOCTL_MAGIC, 8, unsigned int *)

#define CTRL_IOCTL_MAGIC 'C'
#define CTRL_IOCTL_HOT_RESET_BUSLOCK _IOW(CTRL_IOCTL_MAGIC, 0, unsigned int *)
#define CTRL_IOCTL_HOT_RESET_BUSUNLOCK _IOW(CTRL_IOCTL_MAGIC, 1, unsigned int *)
#define CTRL_IOCTL_P2P_INIT _IOW(CTRL_IOCTL_MAGIC, 2, unsigned int *)
#define CTRL_IOCTL_POSITION_ID_REINIT _IOW(CTRL_IOCTL_MAGIC, 3, unsigned int *)

static inline int edge_test_bit(int nr, const volatile unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG - 1)));
}

#endif /* __EDGE_H__ */
