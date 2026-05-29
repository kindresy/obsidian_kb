#include "edge.h"

struct class *g_edge_class; /* sys filesystem */
unsigned int g_edge_major = 0; /* major number */
struct bus_lock g_edge_hotreset_buslock[256] = { 0 }; /* bus lock */
static LIST_HEAD(edev_list);
static DECLARE_BITMAP(minors, EDGE_MINOR_COUNT);
static DECLARE_BITMAP(edev_states, EDGE_DEV_MAX_NUM);
static DEFINE_MUTEX(edev_mutex);
static DECLARE_WAIT_QUEUE_HEAD(g_states_wq); /* wait queue for device online */
static struct smbus_dev smbus_dev;
static struct proc_dir_entry *proc_entry;
static struct p2p_table p2p_table[32];

BLOCKING_NOTIFIER_HEAD(edge_ipcm_notifier);
EXPORT_SYMBOL_GPL(edge_ipcm_notifier);

static void edge_disable_acs_redir(struct edge_dev *edev);
static int edge_set_p2p_ob_regions(struct edge_dev *edev, u32 device_num);
static void edge_disable_switch_vdn_acs_redir(struct edge_dev *edev);
static int edge_enable_irq_vectors(struct edge_dev *edev);
static int edge_setup_irqs(struct edge_dev *edev);
int edge_pcie_host_notify_irq_init(u8 device_id, u8 group_id, u8 chan_id,
				   u8 mode);
int edge_pcie_host_notify_irq_deinit(u8 device_id, u8 group_id, u8 chan_id);
int edge_pcie_host_notify_irq_send(u8 device_id, u8 group_id, u8 chan_id,
				   struct pcie_notify_irq_message *msg,
				   int timeout);
int edge_pcie_host_notify_irq_register(u8 device_id, u8 group_id, u8 chan_id,
				       struct notifier_block *nb);
u32 edge_get_device_state(u32 device_id, u8 group_id);
u32 edge_get_device_cardtype(u32 device_id);
phys_addr_t edge_get_device_shm_phys(u32 device_id, u8 group_id);
phys_addr_t edge_get_device_sram_phys(u32 device_id, u8 group_id);
int edge_get_device_shm_addr(u32 device_id, u8 group_id, phys_addr_t *ipcm_addr,
			     phys_addr_t *mmz_pcie_addr);

static enum pci_bus_speed pcie_link_speed[4] = {
	PCI_SPEED_UNKNOWN,
	PCIE_SPEED_2_5GT,
	PCIE_SPEED_5_0GT,
	PCIE_SPEED_8_0GT
};

static inline u32 edge_readl_safe(struct edge_dev *edev, int barno, u32 off)
{
	u32 val = 0xffffffff;

	if (barno)
		if ((off >= BOOT_STATE_MAGIC_ADDR &&
		     off <= BOOT_STATE_MAGIC_ADDR + 4 * 8) ||
		    (off == X6000_PCIE_LINK_ADDR_OFFSET) ||
		    (off == REG_PCIE_SCRATCH_REG1_ADDR) ||
		    (off == REG_PCIE_MISC_CTRL_STS2_ADDR)) {
			val = edev->bar[0] ?
				      readl(edev->bar[0] +
					    BOOT_STATE_MAGIC_ADDR + 4 * 8) :
				      0xFFFFFFFF;
			if (!(val == EDGE_BL2_MAGIC ||
			      val == EDGE_UBOOT_MAGIC ||
			      val == EDGE_LINUX_MAGIC))
				return 0xffffffff;
		}
	return edev->bar[barno] ? readl(edev->bar[barno] + off) : 0xFFFFFFFF;
}

static irqreturn_t edge_notify_irq_isr(int irq, void *dev_id);
static int edge_udma_poll_state(struct udma_engine *uengine);
static int edge_dma_pool_create(struct edge_dev *edev, u32 size, u32 nums);
static void edge_dma_pool_destory(struct edge_dev *edev);
static void *edge_dma_pool_alloc(struct edge_dev *edev, dma_addr_t *handle);
static void edge_dma_pool_free(struct edge_dev *edev, void *addr,
			       dma_addr_t handle);

static u32 dma_pool_enable = 0;
static u32 dma_pool_block_size = DEFAULT_DMA_POOL_BLOCK_SIZE;
static u32 dma_pool_block_num = DEFAULT_DMA_POOL_BLOCK_NUM;

module_param(dma_pool_enable, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dma_pool_enable, "edge pcie dma pool enable (default 0)");
module_param(dma_pool_block_size, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dma_pool_block_size,
		 "edge pcie dma pool block size (default 0x80000)");
module_param(dma_pool_block_num, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dma_pool_block_num,
		 "edge pcie dma pool block number (default 90)");

// -----------------------------------
// exception irq
// -----------------------------------
static void __edge_exception_msg_dump(struct exception_msg *msg)
{
	printk("-----------------\n");
	printk("pcie exception:\n");
	printk("group_id=%d\n", msg->group_id);
	printk("die_id=%d\n", msg->die_id);
	if (msg->mod < EXCEPTION_MOD_MAX && msg->mod >= 0)
		printk("mod=%s\n", exception_mod_s[msg->mod]);
	else
		printk("mod=%d\n", msg->mod);

	if (msg->type < EXCEPTION_TYPE_MAX && msg->type >= 0)
		printk("type=%s\n", exception_type_s[msg->type]);
	else
		printk("type=%d\n", msg->type);

	if (msg->level < EXCEPTION_LEVEL_MAX && msg->level >= 0)
		printk("level=%s\n", exception_level_s[msg->level]);
	else
		printk("level=%d\n", msg->level);

	printk("errcode=%d\n", msg->errcode);
	printk("-----------------\n");
}

static void __edge_exception_event_to_msg(struct exception_event *event,
					  struct exception_msg *msg)
{
	msg->group_id = event->group_id;
	msg->die_id = event->die_id;
	msg->mod = event->mod;
	msg->type = event->type;
	msg->level = event->level;
	msg->errcode = event->errcode;
}

static int edge_exception_event_init(struct edge_dev *edev,
				     irq_handler_t handler,
				     work_func_t work_func)
{
	int ret;

	if ((edev->pub.card_type == CARD_TYPE_IPU_X5500_PCIE) ||
	    (edev->pub.card_type == CARD_TYPE_IPU_X6000_PCIE)) {
		edev->exception_owner = 0;
		mutex_init(&edev->exception_lock);
		init_waitqueue_head(&edev->wait_exception);
		atomic_set(&edev->wait_exception_done, 0);
		INIT_WORK(&edev->exception_work, work_func);
		ret = request_irq(edev->pdev->irq, handler, IRQF_SHARED,
				  "pcie_exception_event", edev);
		if (ret) {
			pr_err("failed to request_irq intx(%d) in %s\n", ret,
			       __func__);
			edev->intx_enabled = 0;
			return -1;
		}
		edev->pdev->dev_flags |= PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG;
		edev->intx_enabled = 1;
		return 0;
	} else {
		return 0;
	}
}

static void edge_exception_event_deinit(struct edge_dev *edev)
{
	if (edev->intx_enabled) {
		free_irq(edev->pdev->irq, edev);
		edev->intx_enabled = 0;
	}
}

static int edge_exception_event_read(struct edge_dev *edev,
				     struct exception_msg *msg)
{
	int ret = 0;
	u32 val;
	struct exception_fifo *fifo;
	struct exception_event event;

	fifo = edev->bar[0] + EDGE_EXCEPTION_FIFO_OFFSET;
	if (fifo->magic != EDGE_EXCEPTION_FIFO_MAGIC) {
		pr_err("failed to read bar0 fifo magic(0x%x) in %s\n",
		       fifo->magic, __func__);
		return -ENOTSUPP;
	}

	if (fifo->tail != fifo->head) {
		event = fifo->event[fifo->tail];
		rmb();
		fifo->tail = (fifo->tail + 1) % fifo->size;
		wmb();
		__edge_exception_event_to_msg(&event, msg);
	} else {
		val = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR + 4 * 8);
		if (val >= EDGE_BL2_MAGIC && val != 0xFFFFFFFF &&
		    edev->bar[1]) {
			fifo = edev->bar[1] + EDGE_EXCEPTION_FIFO_OFFSET;
			if (fifo->magic != EDGE_EXCEPTION_FIFO_MAGIC) {
				pr_err("failed to read bar1 fifo magic(0x%x) in %s\n",
				       fifo->magic, __func__);
				return -ENODATA; // Maybe bar[0] is normal
			}

			if (fifo->tail != fifo->head) {
				event = fifo->event[fifo->tail];
				rmb();
				fifo->tail = (fifo->tail + 1) % fifo->size;
				wmb();
				__edge_exception_event_to_msg(&event, msg);
			} else {
				ret = -ENODATA;
			}
		} else {
			ret = -ENODATA;
		}
	}

	return ret;
}

static int edge_exception_event_dump(struct edge_dev *edev)
{
	int ret = 0;
	u8 tail, head;
	u32 val;
	struct exception_fifo *fifo;
	struct exception_event event;
	struct exception_msg msg;

	printk("$$$$$$$$$$$$$$$$$$$$$$$%d\n", edev->pub.device_id);
	fifo = edev->bar[0] + EDGE_EXCEPTION_FIFO_OFFSET;
	if (fifo->magic != EDGE_EXCEPTION_FIFO_MAGIC) {
		pr_err("failed to read bar0 fifo magic(0x%x) in %s\n",
		       fifo->magic, __func__);
		return -ENOTSUPP;
	}
	tail = fifo->tail;
	head = fifo->head;
	while (tail != head) {
		event = fifo->event[tail];
		rmb();
		tail = (tail + 1) % fifo->size;
		wmb();
		__edge_exception_event_to_msg(&event, &msg);
		__edge_exception_msg_dump(&msg);
	}
	printk("$$$$$$$$$$$$$$$$$$$$$$$%d\n", edev->pub.device_id);

	val = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR + 4 * 8);
	if (val >= EDGE_BL2_MAGIC && val != 0xFFFFFFFF && edev->bar[1]) {
		printk("#######################%d\n", edev->pub.device_id);
		fifo = edev->bar[1] + EDGE_EXCEPTION_FIFO_OFFSET;
		if (fifo->magic != EDGE_EXCEPTION_FIFO_MAGIC) {
			pr_err("failed to read bar1 fifo magic(0x%x) in %s\n",
			       fifo->magic, __func__);
			return -ENODATA; // Maybe bar[0] is normal
		}

		tail = fifo->tail;
		head = fifo->head;
		while (tail != head) {
			event = fifo->event[tail];
			rmb();
			tail = (tail + 1) % fifo->size;
			wmb();
			__edge_exception_event_to_msg(&event, &msg);
			__edge_exception_msg_dump(&msg);
		}
		printk("#######################%d\n", edev->pub.device_id);
	} else {
		ret = -ENODATA;
	}

	return ret;
}

static int ioctl_do_wait_exception(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;
	struct exception_msg msg;

	rc = edge_exception_event_read(edev, &msg);
	if (rc == -ENODATA) {
		rc = wait_event_interruptible(
			edev->wait_exception,
			atomic_read(&edev->wait_exception_done) > 0);
		if (rc < 0) {
			return (rc == -ERESTARTSYS) ? -EINTR : rc;
		}

		if (atomic_read(&edev->wait_exception_done) == 0xFFFFFFF) {
			atomic_set(&edev->wait_exception_done, 0);
			return -ECANCELED;
		}

		atomic_dec(&edev->wait_exception_done);
		rc = edge_exception_event_read(edev, &msg);
		if (rc < 0)
			return -EAGAIN;
	} else if (rc < 0)
		return rc;

	rc = copy_to_user((void __user *)arg, &msg,
			  sizeof(struct exception_msg));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_do_wait_exception_wakeup(struct edge_dev *edev,
					  unsigned long arg)
{
	atomic_set(&edev->wait_exception_done, 0xFFFFFFF);
	wake_up(&edev->wait_exception);

	return 0;
}

static int ioctl_do_exception_owner(struct file_priv *priv,
				    struct edge_dev *edev, unsigned long arg)
{
	int ret = 0;

	mutex_lock(&edev->exception_lock);
	if (edev->exception_owner == 0)
		edev->exception_owner = priv;
	else
		ret = -EBUSY;
	mutex_unlock(&edev->exception_lock);

	return ret;
}

static int ioctl_do_config_exception(struct file_priv *priv,
				     struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;
	unsigned char onoff;
	struct exception_fifo *fifo;

	rc = copy_from_user(&onoff, (unsigned char __user *)arg,
			    sizeof(unsigned char));
	if (rc) {
		pr_err("fail to copy_from_user in %s\n", __func__);
		return -EFAULT;
	}

	mutex_lock(&edev->exception_lock);
	fifo = edev->bar[0] + EDGE_EXCEPTION_FIFO_OFFSET;
	if (onoff == 0)
		fifo->magic = 0;
	else
		fifo->magic = EDGE_EXCEPTION_FIFO_MAGIC;

	rc = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR + 4 * 8);
	if (rc >= EDGE_BL2_MAGIC && rc != 0xFFFFFFFF) {
		fifo = edev->bar[1] + EDGE_EXCEPTION_FIFO_OFFSET;
		if (onoff == 0)
			fifo->magic = 0;
		else
			fifo->magic = EDGE_EXCEPTION_FIFO_MAGIC;
	}
	mutex_unlock(&edev->exception_lock);

	return 0;
}

static irqreturn_t edge_pcie_exception_event_isr(int irq, void *dev_id)
{
	u32 val;
	struct edge_dev *edev = (struct edge_dev *)dev_id;

	val = edge_readl_safe(edev, 0, REG_PCIE_MISC_CTRL_STS4_ADDR);
	val &= ~(0x1 << 2);
	edge_writel_safe(edev, 0, val, REG_PCIE_MISC_CTRL_STS4_ADDR);

	atomic_inc(&edev->wait_exception_done);
	wake_up(&edev->wait_exception);
	schedule_work(&edev->exception_work);

	return IRQ_HANDLED;
}

static int edge_exception_event_reboot(struct edge_dev *edev)
{
	int ret = 0;
	u32 val;
	int ops;

	val = edge_readl_safe(edev, 0, EDGE_EXCEPTION_REBOOT_OFFSET);
	if (val == EDGE_EXCEPTION_REBOOT_MAGIC) {
		/* Mask SDES in AER */
		ops = pci_find_ext_capability(edev->pdev->bus->self,
					      PCI_EXT_CAP_ID_ERR);
		if (ops) {
			pci_read_config_dword(edev->pdev->bus->self,
					      ops + PCI_ERR_UNCOR_MASK, &val);
			val |= PCI_ERR_UNC_SURPDN;
			pci_write_config_dword(edev->pdev->bus->self,
					       ops + PCI_ERR_UNCOR_MASK, val);
		}

		blocking_notifier_call_chain(&edge_ipcm_notifier,
					     EDGE_DEVICE_OFF,
					     &edev->pub.device_id);
		if (pci_is_enabled(edev->pdev))
			pci_disable_device(edev->pdev);
		edge_writel_safe(edev, 0, 0, EDGE_EXCEPTION_REBOOT_OFFSET);
		ret = 1;
#if 0
		pci_read_config_word(edev->pdev, PCI_COMMAND, &cmd);
		cmd &= ~PCI_COMMAND_MEMORY;
		cmd &= ~PCI_COMMAND_MASTER;
		pci_write_config_word(edev->pdev, PCI_COMMAND, cmd);
#endif
	}

	val = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR + 4 * 8);
	if (val >= EDGE_BL2_MAGIC && val != 0xFFFFFFFF) {
		val = edge_readl_safe(edev, 1, EDGE_EXCEPTION_REBOOT_OFFSET);
		if (val == EDGE_EXCEPTION_REBOOT_MAGIC) {
			blocking_notifier_call_chain(&edge_ipcm_notifier,
						     EDGE_DEVICE_OFF,
						     &edev->pub.device_id);
			if (pci_is_enabled(edev->pdev))
				pci_disable_device(edev->pdev);
			edge_writel_safe(edev, 1, 0,
					 EDGE_EXCEPTION_REBOOT_OFFSET);
			ret = 1;
#if 0
			pci_read_config_word(edev->pdev, PCI_COMMAND, &cmd);
			cmd &= ~PCI_COMMAND_MEMORY;
			cmd &= ~PCI_COMMAND_MASTER;
			pci_write_config_word(edev->pdev, PCI_COMMAND, cmd);
#endif
		}
	}

	return ret;
}

static void edge_pcie_exception_event_work(struct work_struct *work)
{
	int ret;
	struct edge_dev *edev;

	edev = container_of(work, struct edge_dev, exception_work);
	if (!edev) {
		pr_err("Invalid edev in %s\n", __func__);
		return;
	}

	ret = edge_exception_event_reboot(edev);
	if (ret == 0)
		edge_exception_event_dump(edev);
}

// -----------------------------------
// notify irq
// -----------------------------------
static void __notify_irq_lock_init(struct edge_dev *edev, u8 group_id,
				   u8 chan_id)
{
	spin_lock_init(&edev->notify_irq_chan[group_id][chan_id].slock);
}

static int __notify_irq_lock_timeout(struct edge_dev *edev, u8 group_id,
				     u8 chan_id, int timeout,
				     unsigned long *flags)
{
	int ret;
	unsigned long expire;
	expire = msecs_to_jiffies(timeout) + jiffies;

	if (timeout != -1) {
		for (;;) {
			ret = spin_trylock(
				&edev->notify_irq_chan[group_id][chan_id].slock);
			if (!ret) {
				if (time_is_before_eq_jiffies(expire))
					return -ETIMEDOUT;
				else {
					ndelay(10);
					continue;
				}
			}
			break;
		}
	} else {
		spin_lock_irqsave(
			&edev->notify_irq_chan[group_id][chan_id].slock,
			*flags);
	}

	return 0;
}

static void __notify_irq_unlock(struct edge_dev *edev, u8 group_id, u8 chan_id,
				int timeout, unsigned long *flags)
{
	if (timeout != -1) {
		spin_unlock(&edev->notify_irq_chan[group_id][chan_id].slock);
	} else {
		spin_unlock_irqrestore(
			&edev->notify_irq_chan[group_id][chan_id].slock,
			*flags);
	}
}

static void __notify_irq_start(struct edge_dev *edev, u8 group_id, u8 chan_id)
{
	wmb();
	edge_writel_safe(edev, group_id, chan_id, NOTIFY_IRQ_ADDR);
}

static struct pcie_notify_irq_message *
__notify_irq_shm_read(struct edge_dev *edev, u8 group_id, u8 chan_id,
		      u64 vchan_id)
{
	rmb();
	return (struct pcie_notify_irq_message
			*)(&edev->notify_irq_chan[group_id][chan_id]
				    .d2h_shm->msg[vchan_id]);
}

static int __notify_irq_shm_write(struct edge_dev *edev, u8 group_id,
				  u8 chan_id,
				  struct pcie_notify_irq_message *msg,
				  int timeout)
{
	int ret;
	u64 vchan_id;
	unsigned long flags = 0;
	bool is_full = false;
#ifdef NOTIFY_IRQ_BITMAP
	unsigned long bitmap[BITS_TO_LONGS(NOTIFY_IRQ_VCHAN_MAX_NUM)];
#endif

	ret = __notify_irq_lock_timeout(edev, group_id, chan_id, timeout,
					&flags);
	if (ret == -ETIMEDOUT)
		return ret;

#ifdef NOTIFY_IRQ_BITMAP
	memcpy(bitmap, edev->notify_irq_chan[group_id][chan_id].h2d_shm->bitmap,
	       sizeof(bitmap));
	vchan_id = find_first_zero_bit(bitmap, NOTIFY_IRQ_VCHAN_MAX_NUM);
#else
	// 如果vchan_id用完，覆盖上一次的消息
	if ((edev->notify_irq_chan[group_id][chan_id].h2d_shm->head + 1) %
		    NOTIFY_IRQ_VCHAN_MAX_NUM ==
	    edev->notify_irq_chan[group_id][chan_id].h2d_shm->tail) {
		is_full = true;
		if (edev->notify_irq_chan[group_id][chan_id].mode) {
			__notify_irq_unlock(edev, group_id, chan_id, timeout,
					    &flags);
			return -EBUSY;
		}
	}
#endif
	vchan_id = edev->notify_irq_chan[group_id][chan_id].h2d_shm->head;
	memcpy(&edev->notify_irq_chan[group_id][chan_id].h2d_shm->msg[vchan_id],
	       msg, sizeof(struct pcie_notify_irq_message));
	wmb();
#ifdef NOTIFY_IRQ_BITMAP
	edev->notify_irq_chan[group_id][chan_id]
		.h2d_shm->bitmap[vchan_id / BITS_PER_LONG] |=
		(1ULL << (vchan_id % BITS_PER_LONG));
#else
	edev->notify_irq_chan[group_id][chan_id].h2d_shm->bitmap[vchan_id] +=
		0x1;
	if (!is_full) {
		edev->notify_irq_chan[group_id][chan_id].h2d_shm->head =
			(edev->notify_irq_chan[group_id][chan_id].h2d_shm->head +
			 1) %
			NOTIFY_IRQ_VCHAN_MAX_NUM;
	}
#endif
	__notify_irq_unlock(edev, group_id, chan_id, timeout, &flags);

	return 0;
}

int edge_pcie_host_notify_irq_init(u8 device_id, u8 group_id, u8 chan_id,
				   u8 mode)
{
	int ret;
	u8 device_id_flag = 0;
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			device_id_flag = 1;
			break;
		}
	}
	mutex_unlock(&edev_mutex);

	if (device_id_flag == 0) {
		pr_err("device_id=%d is not exist in %s\n", device_id,
		       __func__);
		return -ENODEV;
	}

	ret = edge_readl_safe(edev, group_id, BOOT_STATE_MAGIC_ADDR);
	if (ret != EDGE_LINUX_MAGIC) {
		pr_err("device_id=%d group_id=%d is not in linux state in %s\n",
		       device_id, group_id, __func__);
		return -EHOSTDOWN;
	}

	if (edev->notify_irq_chan[group_id][chan_id].status) {
#if 0
		pr_err("device_id=%d group_id=%d chan_id=%d is initialized in %s\n",
				device_id, group_id, chan_id, __func__);
		return -EBUSY;
#else
		return 0;
#endif
	}

	edev->notify_irq_chan[group_id][chan_id].mode = mode;

	__notify_irq_lock_init(edev, group_id, chan_id);

	edev->notify_irq_chan[group_id][chan_id].h2d_shm =
		(struct notify_irq_shm *)(edev->bar[group_id] +
					  (NOTIFY_IRQ_SHM_ADDR +
					   chan_id * NOTIFY_IRQ_SHM_OFFSET));
	edev->notify_irq_chan[group_id][chan_id].d2h_shm =
		(struct notify_irq_shm *)(edev->bar[group_id] +
					  (NOTIFY_IRQ_SHM_ADDR +
					   (chan_id + NOTIFY_IRQ_MAX_NUM) *
						   NOTIFY_IRQ_SHM_OFFSET));

	edev->notify_irq_chan[group_id][chan_id].h2d_shm->head = 0;
	edev->notify_irq_chan[group_id][chan_id].h2d_shm->tail = 0;

	BLOCKING_INIT_NOTIFIER_HEAD(
		&edev->notify_irq_chan[group_id][chan_id].notifier);
	sprintf(edev->notify_irq_chan[group_id][chan_id].irq_name,
		"%s_%d_%d_%d", "notify_irq", device_id, group_id, chan_id);

	if ((edev->msix_enabled == 0) && (edev->msi_enabled == 0)) {
		mutex_lock(&edev_mutex);
		if (!edev->irq_requested) {
			ret = edge_enable_irq_vectors(edev);
			if (ret < 0) {
				mutex_unlock(&edev_mutex);
				pr_err("failed to edge_enable_irq_vectors(%d %d) in %s\n",
				       device_id, chan_id, __func__);
				return -1;
			}
			ret = edge_setup_irqs(edev);
			if (ret) {
				mutex_unlock(&edev_mutex);
				pr_err("failed to edge_setup_irqs(%d %d) in %s\n",
				       device_id, chan_id, __func__);
				return -1;
			}
			edev->irq_requested = true;
		}
		mutex_unlock(&edev_mutex);
	}

	if (edev->msix_enabled) {
		ret = request_irq(
			edev->entry[1 + group_id * NOTIFY_IRQ_MAX_NUM + chan_id]
				.vector,
			edge_notify_irq_isr, IRQF_SHARED,
			edev->notify_irq_chan[group_id][chan_id].irq_name,
			edev);
		if (ret) {
			pr_err("Couldn't use msix IRQ #%d, ret = %d\n",
			       edev->entry[1 + group_id * NOTIFY_IRQ_MAX_NUM +
					   chan_id]
				       .vector,
			       ret);
			return -1;
		}
		pr_info("MSIX IRQ #%d requested for edev 0x%px.\n",
			edev->entry[1 + group_id * NOTIFY_IRQ_MAX_NUM + chan_id]
				.vector,
			edev);
	} else if (edev->msi_enabled) {
		ret = request_irq(
			edev->pdev->irq + 1 + group_id * NOTIFY_IRQ_MAX_NUM +
				chan_id,
			edge_notify_irq_isr, IRQF_SHARED,
			edev->notify_irq_chan[group_id][chan_id].irq_name,
			edev);
		if (ret) {
			pr_err("Couldn't use msi IRQ #%d, ret = %d\n",
			       edev->pdev->irq + 1 +
				       group_id * NOTIFY_IRQ_MAX_NUM + chan_id,
			       ret);
			return -1;
		}
		pr_info("MSI IRQ #%d requested for edev 0x%px.\n",
			edev->pdev->irq + 1 + group_id * NOTIFY_IRQ_MAX_NUM +
				chan_id,
			edev);
	} else {
		pr_err("Device%d msix/msi not enable in %s\n", device_id,
		       __func__);
		return -1;
	}

	edev->notify_irq_chan[group_id][chan_id].status = 1;

	return 0;
}
EXPORT_SYMBOL_GPL(edge_pcie_host_notify_irq_init);

int edge_pcie_host_notify_irq_deinit(u8 device_id, u8 group_id, u8 chan_id)
{
	u8 device_id_flag = 0;
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			device_id_flag = 1;
			break;
		}
	}
	mutex_unlock(&edev_mutex);

	if (device_id_flag == 0) {
		pr_err("device_id=%d is not exist in %s\n", device_id,
		       __func__);
		return -ENODEV;
	}

	edev->notify_irq_chan[group_id][chan_id].mode = 0;

	if (edev->notify_irq_chan[group_id][chan_id].status == 0) {
#if 0
		pr_err("device_id=%d group_id=%d chan_id=%d is not initialized in %s\n",
				device_id, group_id, chan_id, __func__);
		return -EINVAL;
#else
		return 0;
#endif
	}

#if 0
	ret = edge_readl_safe(edev, group_id, BOOT_STATE_MAGIC_ADDR);
	if (ret != EDGE_LINUX_MAGIC) {
		pr_err("device_id=%d group_id=%d is not in linux state in %s\n", device_id,
				group_id, __func__);
		return -EHOSTDOWN;
	}
#endif

	if (edev->msix_enabled == 1) {
		free_irq(
			edev->entry[1 + group_id * NOTIFY_IRQ_MAX_NUM + chan_id]
				.vector,
			edev);
		memset(edev->notify_irq_chan[group_id][chan_id].irq_name, 0,
		       sizeof(edev->notify_irq_chan[group_id][chan_id]
				      .irq_name));
	} else if (edev->msi_enabled == 1) {
		free_irq(edev->pdev->irq + 1 + group_id * NOTIFY_IRQ_MAX_NUM +
				 chan_id,
			 edev);
		memset(edev->notify_irq_chan[group_id][chan_id].irq_name, 0,
		       sizeof(edev->notify_irq_chan[group_id][chan_id]
				      .irq_name));
	}

	if (edev->notify_irq_chan[group_id][chan_id].nb) {
		blocking_notifier_chain_unregister(
			&edev->notify_irq_chan[group_id][chan_id].notifier,
			edev->notify_irq_chan[group_id][chan_id].nb);
		edev->notify_irq_chan[group_id][chan_id].nb = NULL;
	}

	edev->notify_irq_chan[group_id][chan_id].status = 0;

	return 0;
}
EXPORT_SYMBOL_GPL(edge_pcie_host_notify_irq_deinit);

int edge_pcie_host_notify_irq_send(u8 device_id, u8 group_id, u8 chan_id,
				   struct pcie_notify_irq_message *msg,
				   int timeout)
{
	int ret, minor;
	u8 device_id_flag = 0;
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			device_id_flag = 1;
			break;
		}
	}
	mutex_unlock(&edev_mutex);

	if (device_id_flag == 0) {
		pr_err("device_id=%d is not exist in %s\n", device_id,
		       __func__);
		return -ENODEV;
	}

	ret = edge_readl_safe(edev, group_id, BOOT_STATE_MAGIC_ADDR);
	if (ret != EDGE_LINUX_MAGIC) {
		pr_err("device_id=%d group_id=%d is not in linux state in %s\n",
		       device_id, group_id, __func__);
		return -EHOSTDOWN;
	}

	if (edev->notify_irq_chan[group_id][chan_id].status == 0) {
		pr_err("device_id=%d group_id=%d chan_id=%d is not initialized in %s\n",
		       device_id, group_id, chan_id, __func__);
		return -EINVAL;
	}

	minor = MINOR(edev->cdevno);
	if (!edge_test_bit(minor, edev_states)) {
		pr_err("device_id=%d group_id=%d may be offline in %s\n",
		       device_id, group_id, __func__);
		return -ECANCELED;
	}

	ret = __notify_irq_shm_write(edev, group_id, chan_id, msg, timeout);
	if (ret < 0) {
		if (ret != -EBUSY)
			pr_err("fail to __notify_irq_shm_write(%d) device_id=%d "
			       "group_id=%d chan_id=%d in %s\n",
			       ret, device_id, group_id, chan_id, __func__);
		return ret;
	}

	__notify_irq_start(edev, group_id, chan_id);

	return 0;
}
EXPORT_SYMBOL_GPL(edge_pcie_host_notify_irq_send);

int edge_pcie_host_notify_irq_register(u8 device_id, u8 group_id, u8 chan_id,
				       struct notifier_block *nb)
{
	int ret;
	u8 device_id_flag = 0;
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			device_id_flag = 1;
			break;
		}
	}
	mutex_unlock(&edev_mutex);

	if (device_id_flag == 0) {
		pr_err("device_id=%d is not exist in %s\n", device_id,
		       __func__);
		return -ENODEV;
	}

	if (edev->notify_irq_chan[group_id][chan_id].status == 0) {
		pr_err("device_id=%d group_id=%d chan_id=%d is not initialized in %s\n",
		       device_id, group_id, chan_id, __func__);
		return -EINVAL;
	}

	ret = edge_readl_safe(edev, group_id, BOOT_STATE_MAGIC_ADDR);
	if (ret != EDGE_LINUX_MAGIC) {
		pr_err("device_id=%d group_id=%d is not in linux state in %s\n",
		       device_id, group_id, __func__);
		return -EHOSTDOWN;
	}

	if (edev->notify_irq_chan[group_id][chan_id].nb) {
		blocking_notifier_chain_unregister(
			&edev->notify_irq_chan[group_id][chan_id].notifier,
			edev->notify_irq_chan[group_id][chan_id].nb);
	}

	edev->notify_irq_chan[group_id][chan_id].nb = nb;
	return blocking_notifier_chain_register(
		&edev->notify_irq_chan[group_id][chan_id].notifier, nb);
}
EXPORT_SYMBOL_GPL(edge_pcie_host_notify_irq_register);

static irqreturn_t edge_notify_irq_isr(int irq, void *dev_id)
{
	int i;
	u8 group_id;
	u8 chan_id;
	u64 vchan_id;
	u32 count = 0;
	int minor;
#ifdef NOTIFY_IRQ_BITMAP
	unsigned long bitmap[BITS_TO_LONGS(NOTIFY_IRQ_VCHAN_MAX_NUM)];
#endif
	struct edge_dev *edev = (struct edge_dev *)dev_id;

	minor = MINOR(edev->cdevno);
	if (!edge_test_bit(minor, edev_states)) {
		pr_err("device_id=%d may be offline in %s\n",
		       edev->pub.device_id, __func__);
		return IRQ_HANDLED;
	}

	for (i = 1; i < EDGE_IRQ_MAX_NUM; i++) {
		if (edev->msix_enabled == 1) {
			if (edev->entry[i].vector == irq) {
				group_id =
					(i < (NOTIFY_IRQ_MAX_NUM + 1)) ? 0 : 1;
				chan_id = (i < (NOTIFY_IRQ_MAX_NUM + 1)) ?
						  (i - 1) :
						  (i - NOTIFY_IRQ_MAX_NUM - 1);
				break;
			}
		} else if (edev->msi_enabled == 1) {
			if (edev->pdev->irq + i == irq) {
				group_id =
					(i < (NOTIFY_IRQ_MAX_NUM + 1)) ? 0 : 1;
				chan_id = (i < (NOTIFY_IRQ_MAX_NUM + 1)) ?
						  (i - 1) :
						  (i - NOTIFY_IRQ_MAX_NUM - 1);
				break;
			}
		}
	}
	if (i >= EDGE_IRQ_MAX_NUM) {
		pr_err("device_id=%d irq=%d is not valid in %s\n",
		       edev->pub.device_id, irq, __func__);
		return IRQ_HANDLED;
	}

	for (i = 0; i < 1; i++) {
		count = 0;
#ifdef NOTIFY_IRQ_BITMAP
		memcpy(bitmap,
		       edev->notify_irq_chan[group_id][chan_id].d2h_shm->bitmap,
		       sizeof(bitmap));
		for_each_set_bit (vchan_id, bitmap, NOTIFY_IRQ_VCHAN_MAX_NUM) {
			blocking_notifier_call_chain(
				&edev->notify_irq_chan[group_id][chan_id]
					 .notifier,
				edev->pub.device_id,
				__notify_irq_shm_read(edev, group_id, chan_id,
						      vchan_id));
			edev->notify_irq_chan[group_id][chan_id]
				.d2h_shm->bitmap[vchan_id / BITS_PER_LONG] &=
				~(1ULL
				  << (vchan_id % BITS_PER_LONG)); // 可能丢数据
			count++;
		}
#else
		// 读取所有可读的vchan_id
		while (edev->notify_irq_chan[group_id][chan_id].d2h_shm->head !=
		       edev->notify_irq_chan[group_id][chan_id].d2h_shm->tail) {
			vchan_id = edev->notify_irq_chan[group_id][chan_id]
					   .d2h_shm->tail;
			if (edev->notify_irq_chan[group_id][chan_id]
				    .d2h_shm->bitmap[vchan_id] > 0) {
				blocking_notifier_call_chain(
					&edev->notify_irq_chan[group_id][chan_id]
						 .notifier,
					edev->pub.device_id,
					__notify_irq_shm_read(edev, group_id,
							      chan_id,
							      vchan_id));
			}
			edev->notify_irq_chan[group_id][chan_id]
				.d2h_shm->bitmap[vchan_id] = 0;
			edev->notify_irq_chan[group_id][chan_id].d2h_shm->tail =
				(edev->notify_irq_chan[group_id][chan_id]
					 .d2h_shm->tail +
				 1) %
				NOTIFY_IRQ_VCHAN_MAX_NUM;
		}
#endif
		if (count == 0)
			break;
	}

	return IRQ_HANDLED;
}

// -----------------------------------
// smbus
// -----------------------------------

static int ioctl_do_smbus_wait_event(unsigned long arg, struct file *file)
{
	int rc;
	struct smbus_event *event;
	struct smbus_handle *handle;

	handle = file->private_data;
	if (handle->hotplug_handle == 0)
		return -ENOTSUPP;

	rc = wait_event_interruptible(handle->wait,
				      atomic_read(&handle->wait_done) > 0);
	if (rc < 0) {
		return (rc == -ERESTARTSYS) ? -EINTR : rc;
	}

	if (atomic_read(&handle->wait_done) == 0xFFFFFFF) {
		atomic_set(&handle->wait_done, 0);
		return -ECANCELED;
	}

	atomic_dec(&handle->wait_done);
	mutex_lock(&smbus_dev.event_mlock);
	event = list_first_entry_or_null(&smbus_dev.event_list,
					 struct smbus_event, list);
	if (event == NULL) {
		mutex_unlock(&smbus_dev.event_mlock);
		return -EAGAIN;
	}
	list_del(&event->list);
	mutex_unlock(&smbus_dev.event_mlock);

	rc = copy_to_user((void __user *)arg, &event->msg,
			  sizeof(struct smbus_event_msg));
	kfree(event);
	if (rc) {
		pr_err("fail to copy_to_user in %s\n", __func__);
		return -EFAULT;
	}

	return 0;
}

static int ioctl_do_smbus_trylock(struct file *file)
{
	struct smbus_handle *handle;

	handle = file->private_data;
	if (!mutex_trylock(&smbus_dev.smbus_mlock))
		return -1;
	else {
		smbus_dev.smbus_mlock_handle = handle;
		return 0;
	}
}

static int ioctl_do_smbus_lock(struct file *file)
{
	struct smbus_handle *handle;

	handle = file->private_data;
	mutex_lock(&smbus_dev.smbus_mlock);
	smbus_dev.smbus_mlock_handle = handle;
	return 0;
}

static int ioctl_do_smbus_unlock(struct file *file)
{
	struct smbus_handle *handle;

	handle = file->private_data;
	if (smbus_dev.smbus_mlock_handle == handle) {
		smbus_dev.smbus_mlock_handle = NULL;
		mutex_unlock(&smbus_dev.smbus_mlock);
	}
	return 0;
}

static int ioctl_do_smbus_set_arp_table(unsigned long arg)
{
	int rc = 0;

	rc = copy_from_user(&smbus_dev.tbl, (void __user *)arg,
			    sizeof(struct smbus_table));
	if (rc) {
		pr_err("fail to copy_from_user in %s\n", __func__);
		return -EFAULT;
	}
	return 0;
}

static int ioctl_do_smbus_get_arp_table(unsigned long arg)
{
	int rc = 0;

	rc = copy_to_user((void __user *)arg, &smbus_dev.tbl,
			  sizeof(struct smbus_table));
	if (rc) {
		pr_err("fail to copy_to_user in %s\n", __func__);
		return -EFAULT;
	}
	return 0;
}

static int ioctl_do_smbus_wake_up(void)
{
	int rc = 0;
	struct smbus_handle *handle;

	mutex_lock(&smbus_dev.handle_mlock);
	list_for_each_entry (handle, &smbus_dev.handle_list, list) {
		if (handle->tgid == task_tgid_nr(current)) {
			atomic_set(&handle->wait_done, 0xFFFFFFF);
			wake_up(&handle->wait);
			break;
		}
	}
	mutex_unlock(&smbus_dev.handle_mlock);

	return rc;
}

static int ioctl_do_smbus_register_hotplug(struct file *file)
{
	struct smbus_handle *handle;

	handle = file->private_data;
	mutex_lock(&smbus_dev.handle_mlock);
	init_waitqueue_head(&handle->wait);
	atomic_set(&handle->wait_done, 0);
	list_add_tail(&handle->list, &smbus_dev.handle_list);
	handle->hotplug_handle = 1;
	mutex_unlock(&smbus_dev.handle_mlock);

	return 0;
}

static int ioctl_do_smbus_hostplug_simulation(unsigned long arg)
{
	int rc = 0;
	struct smbus_hotplug_simulation hotplug;

	rc = copy_from_user(&hotplug, (void __user *)arg,
			    sizeof(struct smbus_hotplug_simulation));
	if (rc) {
		pr_err("fail to copy_from_user in %s\n", __func__);
		return -EFAULT;
	}

	if (hotplug.hotplug == 0)
		blocking_notifier_call_chain(&edge_ipcm_notifier,
					     EDGE_DEVICE_OFF,
					     &hotplug.device_id);
	else
		blocking_notifier_call_chain(&edge_ipcm_notifier,
					     EDGE_DEVICE_ON,
					     &hotplug.device_id);

	return rc;
}

static int ioctl_do_hot_reset_bus_lock(struct file *file, unsigned long arg)
{
	int rc = 0;
	int bus_num;

	rc = copy_from_user(&bus_num, (unsigned int __user *)arg,
			    sizeof(unsigned int));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&g_edge_hotreset_buslock[bus_num].lock))
		return -ERESTARTSYS;
	g_edge_hotreset_buslock[bus_num].handle = file->private_data;
	return 0;
}

static int ioctl_do_hot_reset_bus_unlock(struct file *file, unsigned long arg)
{
	int rc = 0;
	int bus_num;

	rc = copy_from_user(&bus_num, (unsigned int __user *)arg,
			    sizeof(unsigned int));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	g_edge_hotreset_buslock[bus_num].handle = NULL;
	mutex_unlock(&g_edge_hotreset_buslock[bus_num].lock);
	return 0;
}

static int ioctl_do_p2p_init(struct file *file, unsigned long arg)
{
	int i;
	int rc;
	int found;
	struct edge_dev *edev, *tmp;
	struct card_ids card_ids;
	struct p2p_table *table;

	rc = copy_from_user(&card_ids, (struct card_ids *)arg,
			sizeof(struct card_ids));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	memset(p2p_table, 0, sizeof(p2p_table));
	for (i = 0; i < card_ids.nums; i++) {
		found = 0;
		list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
			if (edev->pub.magic != X6000_DS_MAGIC) {
				return 0;
			}
			
			if (i ==
					edge_readl_safe(edev, 0,
						X6000_POSITION_ID_ADDR_OFFSET)) {
				p2p_table[i].position_id = i;
				p2p_table[i].card_id =
					card_ids.card_id[edev->pub.device_id];
				p2p_table[i].device_id = edev->pub.device_id;
				p2p_table[i].valid = 0x5A;
				p2p_table[i].bar0_addr =
					pci_resource_start(edev->pdev, 0);
				p2p_table[i].bar4_addr =
					pci_resource_start(edev->pdev, 4);
				found = 1;
				break;
			}
		}
		if (found == 0) {
			pr_err("Failed to found position_id=%d in %s:%d\n", i,
			       __func__, __LINE__);
			return -1;
		}
	}

	for (i = 0; i < card_ids.nums; i++) {
		found = 0;
		list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
			if (edev->pub.magic != X6000_DS_MAGIC) {
				return 0;
			}

			if (i ==
			    edge_readl_safe(edev, 0,
					    X6000_POSITION_ID_ADDR_OFFSET)) {
				table = edev->bar[0] +
					X6000_P2P_TABLE_ADDR_OFFSET;
				memcpy(table, p2p_table, sizeof(p2p_table));
				wmb();
				found = 1;
				edge_disable_acs_redir(edev);
				rc = edge_set_p2p_ob_regions(edev,
							     card_ids.nums);
				if (rc)
					return -1;
				break;
			}
		}
		if (found == 0) {
			pr_err("Failed to found position_id=%d in %s:%d\n", i,
			       __func__, __LINE__);
			return -1;
		}
	}

	return 0;
}

static int ioctl_do_position_id_reinit(struct file *file, unsigned long arg)
{
	int rc;
	struct edge_dev *edev, *tmp;

	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		edge_writel_safe(edev, 0, edev->pub.device_id,
				 X6000_POSITION_ID_ADDR_OFFSET);
		rc = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR + 4 * 8);
		if (rc >= EDGE_BL2_MAGIC && rc != 0xFFFFFFFF) {
			edge_writel_safe(edev, 1, edev->pub.device_id,
					 X6000_POSITION_ID_ADDR_OFFSET);
		}
	}

	return 0;
}

static int smbus_open(struct inode *inode, struct file *file)
{
	struct smbus_handle *handle;

	handle = kzalloc(sizeof(struct smbus_handle), GFP_KERNEL);
	if (handle == NULL) {
		pr_err("fail to kzalloc %ld in %s\n",
		       sizeof(struct smbus_handle), __func__);
		return -ENOMEM;
	}

	handle->hotplug_handle = 0;
	handle->tgid = task_tgid_nr(current);
	file->private_data = handle;

	return 0;
}

static int smbus_release(struct inode *inode, struct file *file)
{
	struct smbus_event *event;
	struct smbus_event *event_tmp;
	struct smbus_handle *handle;
	int bus_num;

	handle = file->private_data;

	if (smbus_dev.smbus_mlock_handle == handle) {
		smbus_dev.smbus_mlock_handle = NULL;
		mutex_unlock(&smbus_dev.smbus_mlock);
	}

	for (bus_num = 0; bus_num < 256; bus_num++) {
		if (handle == g_edge_hotreset_buslock[bus_num].handle) {
			g_edge_hotreset_buslock[bus_num].handle = NULL;
			mutex_unlock(&g_edge_hotreset_buslock[bus_num].lock);
			/* one handle hold only one bus lock */
			break;
		}
	}

	if (handle) {
		if (handle->hotplug_handle == 1) {
			mutex_lock(&smbus_dev.handle_mlock);
			list_del(&handle->list);
			mutex_unlock(&smbus_dev.handle_mlock);
		}

		kfree(handle);
		handle = NULL;
	}

	if (list_empty(&smbus_dev.handle_list)) {
		mutex_lock(&smbus_dev.event_mlock);
		list_for_each_entry_safe (event, event_tmp,
					  &smbus_dev.event_list, list) {
			list_del(&event->list);
			kfree(event);
		}
		mutex_unlock(&smbus_dev.event_mlock);
	}

	return 0;
}

static long smbus_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int rc;

	switch (cmd) {
	case SMBUS_IOCTL_WAIT_EVENT:
		rc = ioctl_do_smbus_wait_event(arg, file);
		break;
	case SMBUS_IOCTL_TRYLOCK:
		rc = ioctl_do_smbus_trylock(file);
		break;
	case SMBUS_IOCTL_LOCK:
		rc = ioctl_do_smbus_lock(file);
		break;
	case SMBUS_IOCTL_UNLOCK:
		rc = ioctl_do_smbus_unlock(file);
		break;
	case SMBUS_IOCTL_SET_ARP_TABLE:
		rc = ioctl_do_smbus_set_arp_table(arg);
		break;
	case SMBUS_IOCTL_GET_ARP_TABLE:
		rc = ioctl_do_smbus_get_arp_table(arg);
		break;
	case SMBUS_IOCTL_WAKE_UP:
		rc = ioctl_do_smbus_wake_up();
		break;
	case SMBUS_IOCTL_REGISTER_HOTPLUG:
		rc = ioctl_do_smbus_register_hotplug(file);
		break;
	case SMBUS_IOCTL_HOTPLUG_SIMULATION:
		rc = ioctl_do_smbus_hostplug_simulation(arg);
		break;
	case CTRL_IOCTL_HOT_RESET_BUSLOCK:
		rc = ioctl_do_hot_reset_bus_lock(file, arg);
		break;
	case CTRL_IOCTL_HOT_RESET_BUSUNLOCK:
		rc = ioctl_do_hot_reset_bus_unlock(file, arg);
		break;
	case CTRL_IOCTL_P2P_INIT:
		rc = ioctl_do_p2p_init(file, arg);
		break;
	case CTRL_IOCTL_POSITION_ID_REINIT:
		rc = ioctl_do_position_id_reinit(file, arg);
		break;
	default:
		pr_err("Unsupported operation in %s\n", __func__);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static const struct file_operations smbus_fops = {
	.owner = THIS_MODULE,
	.open = smbus_open,
	.unlocked_ioctl = smbus_ioctl,
	.release = smbus_release,
};

static int smbus_event_callback(struct notifier_block *nb, unsigned long action,
				void *data)
{
	u32 device_id;
	struct smbus_event *event;
	struct smbus_handle *handle;

	device_id = *(unsigned int *)data;

	if ((device_id >= EDGE_DEV_MAX_NUM) || ((int)device_id < 0)) {
		//pr_err("Invalid param, device_id=%d in %s\n", device_id, __func__);
		return 0;
	}

	mutex_lock(&smbus_dev.handle_mlock);
	handle = list_first_entry_or_null(&smbus_dev.handle_list,
					  struct smbus_handle, list);
	if (handle == NULL) {
		mutex_unlock(&smbus_dev.handle_mlock);

		// all hotplug thread is exit. clear the smbus id when hot reset
		if ((smbus_dev.tbl.status == 1) && (action == EDGE_DEVICE_OFF))
			smbus_dev.tbl.id[device_id].smbus_id = 0;
		return 0;
	}
	mutex_unlock(&smbus_dev.handle_mlock);

	event = kzalloc(sizeof(struct smbus_event), GFP_KERNEL);
	if (event == NULL) {
		pr_err("fail to kzalloc %ld in %s\n",
		       sizeof(struct smbus_event), __func__);
		return -ENOMEM;
	}
	event->msg.device_id = device_id;
	event->msg.event = (action == EDGE_DEVICE_OFF) ? 0 : 1;

	mutex_lock(&smbus_dev.event_mlock);
	list_add_tail(&event->list, &smbus_dev.event_list);
	mutex_unlock(&smbus_dev.event_mlock);

	atomic_inc(&handle->wait_done);
	wake_up(&handle->wait);

	return 0;
}

static struct notifier_block smbus_event_nb = {
	.notifier_call = smbus_event_callback,
};

static int smbus_misc_register(void)
{
	int ret;

	memset(&smbus_dev, 0, sizeof(struct smbus_dev));
	smbus_dev.miscdev.minor = MISC_DYNAMIC_MINOR;
	smbus_dev.miscdev.name = "edge_smbus";
	smbus_dev.miscdev.fops = &smbus_fops;
	ret = misc_register(&smbus_dev.miscdev);
	if (ret) {
		pr_err("fail to misc_register in %s\n", __func__);
		return -1;
	}

	INIT_LIST_HEAD(&smbus_dev.event_list);
	INIT_LIST_HEAD(&smbus_dev.handle_list);
	mutex_init(&smbus_dev.smbus_mlock);
	mutex_init(&smbus_dev.event_mlock);
	mutex_init(&smbus_dev.handle_mlock);

	blocking_notifier_chain_register(&edge_ipcm_notifier, &smbus_event_nb);

	smbus_dev.status = 1;

	return 0;
}

static int smbus_misc_deregister(void)
{
	if (smbus_dev.status) {
		misc_deregister(&smbus_dev.miscdev);
		blocking_notifier_chain_unregister(&edge_ipcm_notifier,
						   &smbus_event_nb);
	}

	memset(&smbus_dev, 0, sizeof(struct smbus_dev));

	return 0;
}

// -----------------------------------
// edge
// -----------------------------------

#define HORIZON_LINE_FMT                                                       \
	"------------------------------------------------------------------------------------------------------------------------------\n"

static ssize_t dma_stats_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct edge_dev *edev = pci_get_drvdata(pdev);
	char *start = buf;
	int ch;

	buf += sprintf(buf, HORIZON_LINE_FMT);
	buf += sprintf(buf, "|%2s|%20s|%17s|%17s|%20s|%17s|%17s|%7s|\n", "ch",
		       "tx-bytes", "tx-bytes-in-queue", "tx-tasks-in-queue",
		       "rx-bytes", "rx-bytes-in-queue", "rx-tasks-in-queue",
		       "status");
	buf += sprintf(buf, HORIZON_LINE_FMT);
	for (ch = 0; ch < EDGE_DMA_CH_NUM; ch++) {
		buf += sprintf(buf,
			       "|%2d|%20llu|%17u|%17u|%20llu|%17u|%17u|%7s|\n",
			       ch, edev->uengine[ch]->ustats.tx.bytes_stat,
			       edev->uengine[ch]->ustats.tx.queued_bytes,
			       edev->uengine[ch]->ustats.tx.queued_tasks,
			       edev->uengine[ch]->ustats.rx.bytes_stat,
			       edev->uengine[ch]->ustats.rx.queued_bytes,
			       edev->uengine[ch]->ustats.rx.queued_tasks,
			       edev->uengine[ch]->running ? "Running" : "Idle");
	}
	buf += sprintf(buf, HORIZON_LINE_FMT);

	return buf - start;
}
static DEVICE_ATTR(dma_stats, 0444, dma_stats_show, NULL);

u32 edge_get_device_state(u32 device_id, u8 group_id)
{
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			mutex_unlock(&edev_mutex);
			if (edge_test_bit(MINOR(edev->cdevno), edev_states))
				return edge_readl_safe(edev, group_id,
						       BOOT_STATE_MAGIC_ADDR);
			else
				return 0;
		}
	}
	mutex_unlock(&edev_mutex);
	return 0;
}
EXPORT_SYMBOL(edge_get_device_state);

u32 edge_get_device_cardtype(u32 device_id)
{
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			mutex_unlock(&edev_mutex);
			return edev->pub.card_type;
		}
	}
	mutex_unlock(&edev_mutex);
	return 0;
}
EXPORT_SYMBOL(edge_get_device_cardtype);

phys_addr_t edge_get_device_shm_phys(u32 device_id, u8 group_id)
{
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			mutex_unlock(&edev_mutex);
			return edev->pub.shm_phys[group_id];
		}
	}
	mutex_unlock(&edev_mutex);
	return 0;
}
EXPORT_SYMBOL(edge_get_device_shm_phys);

phys_addr_t edge_get_device_sram_phys(u32 device_id, u8 group_id)
{
	struct edge_dev *edev, *tmp;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			mutex_unlock(&edev_mutex);
			return edev->pub.sram_phys[group_id];
		}
	}
	mutex_unlock(&edev_mutex);
	return 0;
}
EXPORT_SYMBOL(edge_get_device_sram_phys);

static u64 apt_remap_addr(struct edge_dev *edev, u8 group_id, u32 r)
{
	return ((u64)edge_readl_safe(edev, group_id,
				     REG_PCIE_APT_REMAP_BASE_LO_ADDR(r))) |
	       ((u64)edge_readl_safe(edev, group_id,
				     REG_PCIE_APT_REMAP_BASE_HI_ADDR(r))
		<< 32);
}

int edge_get_device_shm_addr(u32 device_id, u8 group_id, phys_addr_t *ipcm_addr,
			     phys_addr_t *mmz_pcie_addr)
{
	struct edge_dev *edev, *tmp;
	u32 val;

	mutex_lock(&edev_mutex);
	list_for_each_entry_safe (edev, tmp, &edev_list, device_entry) {
		if (edev->pub.device_id == device_id) {
			mutex_unlock(&edev_mutex);
			val = edge_readl_safe(edev, group_id,
					      BOOT_STATE_MAGIC_ADDR);
			if (val != EDGE_LINUX_MAGIC) {
				pr_err("Device %d is not in linux state in %s.\n",
				       device_id, __FUNCTION__);
				return -EPERM;
			}

			*ipcm_addr = apt_remap_addr(edev, group_id, 13);

			if (group_id == 0) {
				*mmz_pcie_addr =
					apt_remap_addr(edev, group_id, 28);
			} else if (group_id == 1) {
				*mmz_pcie_addr =
					apt_remap_addr(edev, group_id, 12);
			}

			return 0;
		}
	}
	mutex_unlock(&edev_mutex);
	return 0;
}
EXPORT_SYMBOL(edge_get_device_shm_addr);

static void edge_set_outbound_region(struct edge_dev *edev, u32 r, u64 cpu_addr,
				     u64 pci_addr, size_t size)
{
	u64 sz = 1ULL << fls64(size - 1);
	int nbits = ilog2(sz);
	u32 addr0, addr1, desc0, desc1;
	struct atu_regs *atu_base = edev->bar[0] + REG_AXI_CFG_ADDR;

	if (nbits < 8)
		nbits = 8;

	/* Set the PCI address */
	addr0 = EDGE_PCIE_AT_OB_REGION_PCI_ADDR0_NBITS(nbits) |
		(lower_32_bits(pci_addr) & 0xFFFFFF00);
	addr1 = upper_32_bits(pci_addr);

	edge_writel(addr0, &atu_base->ob_atu[r].addr0);
	edge_writel(addr1, &atu_base->ob_atu[r].addr1);

	/* Set the PCIe header descriptor */
	desc0 = EDGE_PCIE_AT_OB_REGION_DESC0_TYPE_MEM;
	desc1 = 0;

	desc0 |= EDGE_PCIE_AT_OB_REGION_DESC0_DEVFN(0);

	edge_writel(desc0, &atu_base->ob_atu[r].desc0);
	edge_writel(desc1, &atu_base->ob_atu[r].desc1);

	/* Set the CPU address */
	cpu_addr -= PCIE_CONTROLLER_X8_SLAVE_SPACE_BASE_PA;
	addr0 = EDGE_PCIE_AT_OB_REGION_CPU_ADDR0_NBITS(nbits) |
		(lower_32_bits(cpu_addr) & 0xFFFFFF00);
	addr1 = upper_32_bits(cpu_addr);

	edge_writel(addr0, &atu_base->ob_atu[r].axi_addr0);
	edge_writel(addr1, &atu_base->ob_atu[r].axi_addr1);
}

static int edge_set_p2p_ob_regions(struct edge_dev *edev, u32 device_num)
{
	int i, r = 1;
	u64 cpu_base = PCIE_CONTROLLER_X8_SLAVE_SPACE_BASE_PA + 0x1000000;
	u32 position_id =
		edge_readl_safe(edev, 0, X6000_POSITION_ID_ADDR_OFFSET);
	for (i = 0; i < device_num; i++) {
		if (p2p_table[i].valid != 0x5A) {
			pr_err("Failed to found position_id=%d in %s:%d\n", i,
			       __func__, __LINE__);
			return -1;
		}
#if 0
		if (position_id / 4 == i / 4)
#else
		if (position_id == i)
#endif
			continue;
		edge_set_outbound_region(edev, r, cpu_base + 0x1000000 * i,
					 (u64)p2p_table[i].bar0_addr,
					 0x1000000);
		r++;
	}

	return 0;
}

static void edge_disable_acs_redir(struct edge_dev *edev)
{
	int pos;
	u16 ctrl;
	struct pci_dev *dev;
	struct pci_bus *bus;

	if (edev->pub.card_type == CARD_TYPE_IPU_X6000_PCIE) {
		if (edev->pdev->bus->parent->parent &&
		    edev->pdev->bus->parent->parent->parent->parent->parent) {
#if 0
			bus = edev->pdev->bus->parent->parent->parent->parent;
#else
			bus = edev->pdev->bus;
#endif
			while (1) {
				dev = bus->self;
				pos = pci_find_ext_capability(
					dev, PCI_EXT_CAP_ID_ACS);
				if (pos) {
					pci_read_config_word(
						dev, pos + PCI_ACS_CTRL, &ctrl);
					if (ctrl & (PCI_ACS_RR | PCI_ACS_CR |
						    PCI_ACS_EC)) {
						ctrl &= ~(PCI_ACS_RR |
							  PCI_ACS_CR |
							  PCI_ACS_EC);
						pci_write_config_word(
							dev, pos + PCI_ACS_CTRL,
							ctrl);
					}
				}
				if (bus->parent->parent != NULL)
					bus = bus->parent;
				else
					break;
			}
		}
	}

	return;
}

static void edge_disable_switch_vdn_acs_redir(struct edge_dev *edev)
{
	int pos;
	u16 ctrl;
	struct pci_dev *dev;
	struct pci_bus *bus;

	if (edev->pub.card_type == CARD_TYPE_IPU_X6000_PCIE) {
		bus = edev->pdev->bus->parent->parent;
		dev = bus->self;
		pos = pci_find_ext_capability(dev, PCI_EXT_CAP_ID_ACS);
		if (pos) {
			pci_read_config_word(dev, pos + PCI_ACS_CTRL, &ctrl);
			if (ctrl & PCI_ACS_SV) {
				ctrl &= ~PCI_ACS_SV;
				pci_write_config_word(dev, pos + PCI_ACS_CTRL,
						      ctrl);
			}
		}
	}

	return;
}

static void edge_disable_relax_order(struct edge_dev *edev)
{
	int pos;
	u16 ctrl;
	struct pci_dev *dev;
	struct pci_bus *bus;

	if (edev->pub.card_type == CARD_TYPE_IPU_X6000_PCIE) {
		if (edev->pdev->bus->parent->parent &&
		    edev->pdev->bus->parent->parent->parent->parent->parent) {
			bus = edev->pdev->bus;
			while (1) {
				dev = bus->self;
				pos = pci_find_capability(dev, PCI_CAP_ID_EXP);
				if (pos) {
					pci_read_config_word(
						dev, pos + PCI_EXP_DEVCTL,
						&ctrl);
					if (ctrl & PCI_EXP_DEVCTL_RELAX_EN) {
						ctrl &= ~PCI_EXP_DEVCTL_RELAX_EN;
						pci_write_config_word(
							dev,
							pos + PCI_EXP_DEVCTL,
							ctrl);
					}
				}
				if (bus->parent->parent != NULL)
					bus = bus->parent;
				else
					break;
			}
		}
	}

	return;
}

/*
 * sgm_create_mapper() - Create a mapper for virtual memory to scatterlist.
 *
 * @max_len: Maximum number of bytes that can be mapped at a time.
 *
 * Allocates a book keeping structure, array to page pointers and a scatter
 * list to map virtual user memory into.
 *
 */
static struct udma_sg_mapper *sgm_create_mapper(unsigned long max_len)
{
	struct udma_sg_mapper *sgm;

	if (max_len == 0)
		return NULL;

	/* allocate bookkeeping */
	sgm = kcalloc(1, sizeof(struct udma_sg_mapper), GFP_KERNEL);
	if (sgm == NULL)
		return NULL;

	/* upper bound of pages */
	sgm->max_pages = max_len / PAGE_SIZE + 2;
	/* allocate an array of struct page pointers */
	sgm->pages = kcalloc(sgm->max_pages, sizeof(*sgm->pages), GFP_KERNEL);
	if (sgm->pages == NULL) {
		kfree(sgm);
		return NULL;
	}

	/* allocate a scatter gather list */
	sgm->sgl =
		kcalloc(sgm->max_pages, sizeof(struct scatterlist), GFP_KERNEL);
	if (sgm->sgl == NULL) {
		kfree(sgm->pages);
		kfree(sgm);
		return NULL;
	}

	sg_init_table(sgm->sgl, sgm->max_pages);

	return sgm;
};

/*
 * sgm_destroy_mapper() - Destroy a mapper for virtual memory to scatterlist.
 *
 * @sgm scattergather mapper handle.
 */
static void sgm_destroy_mapper(struct udma_sg_mapper *sgm)
{
	/* user failed to call sgm_unmap_user_pages() */
	BUG_ON(sgm->mapped_pages > 0);
	/* free scatterlist */
	kfree(sgm->sgl);
	/* free page array */
	kfree(sgm->pages);
	/* free mapper handle */
	kfree(sgm);
	sgm = NULL;
	pr_debug("Freed page pointer and scatterlist.\n");
};

/*
 * sgm_get_user_pages() - Get user pages and build a scatterlist.
 *
 * @sgm scattergather mapper handle.
 * @buf User space buffer (virtual memory) address.
 * @count Number of bytes in buffer.
 * @to_user !0 if data direction is from device to user space.
 *
 * Returns Number of entries in the table on success, -1 on error.
 */
static int sgm_get_user_pages(struct udma_sg_mapper *sgm, void *start,
			      size_t count, int to_user)
{
	/* the number of pages we want to map in */
	unsigned int nr_pages;
	int rc, i;
	struct scatterlist *sgl = sgm->sgl;
	struct page **pages = sgm->pages;

	nr_pages = (((unsigned long)start + count + PAGE_SIZE - 1) -
		    ((unsigned long)start & PAGE_MASK)) >>
		   PAGE_SHIFT;

	/* no pages should currently be mapped */
	BUG_ON(sgm->mapped_pages > 0);
	if (start + count < start)
		return -EINVAL;
	if (nr_pages > sgm->max_pages)
		return -EINVAL;
	if (count == 0)
		return 0;

	/* initialize scatter gather list */
	sg_init_table(sgl, nr_pages);

	for (i = 0; i < nr_pages; i++) {
		pages[i] = NULL;
	}

	/* try to fault in all of the necessary pages */
	rc = get_user_pages_fast((unsigned long)start, nr_pages, to_user,
				 pages);
	if (rc < nr_pages) {
		if (rc > 0)
			sgm->mapped_pages = rc;
		pr_debug("get user pages failed, %d.\n", rc);
		goto out_unmap;
	}

	BUG_ON(rc != nr_pages);
	sgm->mapped_pages = rc;

	for (i = 0; i < nr_pages; i++) {
		unsigned int offset = offset_in_page(start);
		unsigned int nbytes =
			min_t(unsigned int, PAGE_SIZE - offset, count);

		flush_dcache_page(pages[i]);
		sg_set_page(&sgl[i], pages[i], nbytes, offset);

		start += nbytes;
		count -= nbytes;
	}

	return nr_pages;

out_unmap:
	/* { rc < 0 means errors, >= 0 means not all pages could be mapped } */
	/* did we map any pages? */
	for (i = 0; i < sgm->mapped_pages; i++)
		put_page(pages[i]);
	rc = -ENOMEM;
	sgm->mapped_pages = 0;

	return rc;
}

/*
 * sgm_put_user_pages() - Mark the pages dirty and release them.
 *
 * Pages mapped earlier with sgm_map_user_pages() are released here.
 * after being marked dirtied by the DMA.
 *
 */
static int sgm_put_user_pages(struct udma_sg_mapper *sgm, int dirtied)
{
	int i;

	/* mark page dirty */
	if (dirtied) {
		/* iterate over all mapped pages */
		for (i = 0; i < sgm->mapped_pages; i++) {
			/* mark page dirty */
			SetPageDirty(sgm->pages[i]);
		}
	}

	/* put (i.e. release) pages */
	for (i = 0; i < sgm->mapped_pages; i++) {
		put_page(sgm->pages[i]);
	}

	/* remember we have zero pages */
	sgm->mapped_pages = 0;

	return 0;
}

/* sgm_get_kernel_pages() -- create a sgm map from a vmalloc()ed memory */
static int sgm_get_kernel_pages(struct udma_sg_mapper *sgm, void *start,
				size_t count, int to_user)
{
	/* the number of pages we want to map in */
	unsigned int nr_pages;
	int rc, i;
	struct scatterlist *sgl = sgm->sgl;
	/* pointer to array of page pointers */
	struct page **pages = sgm->pages;
	unsigned char *virt = (unsigned char *)start;

	/* calculate page frame number @todo use macro's */
	nr_pages = (((unsigned long)start + count + PAGE_SIZE - 1) -
		    ((unsigned long)start & PAGE_MASK)) >>
		   PAGE_SHIFT;

	/* no pages should currently be mapped */
	BUG_ON(sgm->mapped_pages > 0);
	if (start + count < start)
		return -EINVAL;
	if (nr_pages > sgm->max_pages)
		return -EINVAL;
	if (count == 0)
		return 0;

	/* initialize scatter gather list */
	sg_init_table(sgl, nr_pages);

	/* get pages belonging to vmalloc()ed space */
	for (i = 0; i < nr_pages; i++, virt += PAGE_SIZE) {
		pages[i] = vmalloc_to_page(virt);
		if (pages[i] == NULL)
			goto err;
		/* make sure page was allocated using vmalloc_32() */
		BUG_ON(PageHighMem(pages[i]));
	}

	sgm->mapped_pages = nr_pages;
	pr_debug("sgm->mapped_pages = %d.\n", sgm->mapped_pages);

	for (i = 0; i < nr_pages; i++) {
		unsigned int offset = offset_in_page(start);
		unsigned int nbytes =
			min_t(unsigned int, PAGE_SIZE - offset, count);

		flush_dcache_page(pages[i]);
		sg_set_page(&sgl[i], pages[i], nbytes, offset);

		start += nbytes;
		count -= nbytes;
	}

	return nr_pages;

err:
	rc = -ENOMEM;
	sgm->mapped_pages = 0;

	return rc;
}

static void udma_dump(struct udma_engine *uengine)
{
	struct udma_xfer *xfer, *tmp;
	int i;

	pr_info("device %d channel %d (TAIL %d, HEAD %d):\n",
		uengine->edev->pub.device_id, uengine->channel, uengine->tail,
		uengine->head);

	list_for_each_entry_safe (xfer, tmp, &uengine->transfer_list, entry) {
		for (i = 0; i < xfer->desc_num; i++)
			pr_info("\t[transfer 0x%px desc %d desc_pos %d host_addr 0x%llx dev_addr 0x%llx size %d is_ob %d]\n",
				xfer, i,
				(xfer->desc_pos + i) % UDMA_DESC_MAX_NUM,
				xfer->ll[i].host_phys, xfer->ll[i].dev_phys,
				xfer->ll[i].size, xfer->dir);
	}
}

/* set B0 inbound region to forward A0 DMA transfer */
static void udma_set_inbound_region(struct edge_dev *edev, u32 barno, u32 r,
				    u64 start_addr, u64 remap_addr, size_t size)
{
	u64 end_addr = start_addr + size - 1;
	u32 val = 0;

	edge_writel_safe(edev, barno, lower_32_bits(start_addr),
			 REG_PCIE_APT_BASE_LO_ADDR(r));
	edge_writel_safe(edev, barno, upper_32_bits(start_addr),
			 REG_PCIE_APT_BASE_HI_ADDR(r));
	edge_writel_safe(edev, barno, lower_32_bits(end_addr),
			 REG_PCIE_APT_LIMIT_LO_ADDR(r));
	edge_writel_safe(edev, barno, upper_32_bits(end_addr),
			 REG_PCIE_APT_LIMIT_HI_ADDR(r));
	edge_writel_safe(edev, barno, lower_32_bits(remap_addr),
			 REG_PCIE_APT_REMAP_BASE_LO_ADDR(r));
	edge_writel_safe(edev, barno, upper_32_bits(remap_addr),
			 REG_PCIE_APT_REMAP_BASE_HI_ADDR(r));

	if (!FLUSH_WRITE_AND_CHECK(
		    val, edev->bar[barno] + REG_PCIE_APT_REMAP_BASE_LO_ADDR(r),
		    lower_32_bits(remap_addr))) {
		pr_err("Failed to check remap lower addr(0x%x - 0x%x)\n",
		       lower_32_bits(remap_addr), val);
	}
}

static struct udma_xfer *udma_start_engine(struct udma_engine *uengine)
{
	struct udma_xfer *transfer;
	struct udma_regs *udma_base =
		uengine->edev->bar[0] + uengine->edev->dma_reg_offset;
	int channel = uengine->channel;

	/* engine must be idle */
	BUG_ON(uengine->running);
	/* engine transfer queue must not be empty */
	BUG_ON(list_empty(&uengine->transfer_list));
	/* inspect first transfer queued on the engine */
	transfer = list_entry(uengine->transfer_list.next, struct udma_xfer,
			      entry);
	BUG_ON(!transfer);
	transfer->state = TRANSFER_STATE_STARTED;

	if (transfer->group_id == 1) {
		struct edge_dev *edev = transfer->uengine->edev;
		/* start addr, remap addr and size must 4KB align */
		u64 unaligned_len = transfer->ll->dev_phys -
				    (transfer->ll->dev_phys & (~0xFFF));

		u64 start_addr = EDGE_BAR0_APT_BASE_ADDR + edev->region_offset;
		u64 remap_addr = transfer->ll->dev_phys - unaligned_len;
		size_t size = (transfer->ll->size + unaligned_len + 0x1000) &
			      (~0xFFF);

		if (size > edev->region_size) {
			pr_warn("%s: GroupB DMA transfer region size error, "
				"size 0x%lx\n",
				__func__, size);
			size = edev->region_size;
		}

		/*
		 * edev->region was been set as 10 in 'edge_create_udma_engines'
		 * if DS:
		 * 	channel id is fixed to 3, region id will must be 11
		 *      start_addr = EDGE_BAR0_APT_BASE_ADDR + edev->region_offset + edev->region_size (e.g 0x6A0000)
		 * if VS/LM:
		 * 	channel id will likely be one of 2 or 3, region id will be 10 or 11
		 *      start_addr:
		 *      	channel == 2 --> EDGE_BAR0_APT_BASE_ADDR + edev->region_offset  (e.g 0x610000)
		 *      	channel == 3 --> EDGE_BAR0_APT_BASE_ADDR + edev->region_offset + edev->region_size (e.g 0x6A0000)
		 * */
		udma_set_inbound_region(edev, 1, (edev->region_index + (channel - 2)),
			start_addr + (channel - 2) * (edev->region_size), remap_addr, edev->region_size);
	}

	edge_writel(cpu_to_le32(lower_32_bits(transfer->desc_bus)),
		    &udma_base->ch[channel].channel_sp_l);
	edge_writel(cpu_to_le32(upper_32_bits(transfer->desc_bus)),
		    &udma_base->ch[channel].channel_sp_u);
	//mmiowb();
	wmb();

	/* Kicks off the uDMA channel controller to fetch linked list */
	edge_writel(transfer->dir ? (KICK_OFF_DMA | IS_OUTBOUND) : KICK_OFF_DMA,
		    &udma_base->ch[channel].channel_ctrl);

	/* remember the engine is running */
	uengine->running = 1;

	return transfer;
}

static struct udma_xfer *engine_service_transfer(struct udma_engine *uengine)
{
	struct udma_xfer *transfer = NULL;
	struct udma_desc *desc_base = uengine->desc_virt_base;
	u32 desc_idx;
	int i;
	bool err_flag = false;
	bool abort_flag = false;

	uengine->running = 0;

	if (!list_empty(&uengine->transfer_list)) {
		/* pick first transfer on queue (was submitted to the engine) */
		transfer = list_entry(uengine->transfer_list.next,
				      struct udma_xfer, entry);
	} else {
		return NULL;
	}

	if (transfer) {
		pr_debug("service for head of queue transfer 0x%px "
			 "with %d descriptors\n",
			 transfer, transfer->desc_num);
		if (transfer->state == TRANSFER_STATE_ABORTED)
			abort_flag = true;

		desc_idx = transfer->desc_pos;
		/* read status fields of each descriptor in the linked list */
		for (i = 0; i < transfer->desc_num; i++) {
			if ((desc_base[desc_idx].status & 0xFF0000) !=
			    (0x1 << 16)) {
				pr_err("transfer 0x%px DESC %3d, idx = %d:\n"
				       " unexpected channel status=0x%x!\n"
				       " sys_status=0x%x, ext_status=0x%x.\n",
				       transfer, i, desc_idx,
				       (desc_base[desc_idx].status &
					0xFF0000) >>
					       16,
				       (desc_base[desc_idx].status & 0xFF),
				       (desc_base[desc_idx].status & 0xFF00) >>
					       8);
				err_flag = true;
			}
			desc_idx = (desc_idx + 1) % UDMA_DESC_MAX_NUM;
			/* update tail with the last completed/failed descriptor index */
			uengine->tail = desc_idx;
		}

		if (err_flag == false)
			transfer->state = TRANSFER_STATE_COMPLETED;
		else
			transfer->state = TRANSFER_STATE_FAILED;

		/* remove completed transfer from list */
		list_del(uengine->transfer_list.next);

		if (abort_flag == false)
			wake_up_interruptible(&transfer->wq);
		else
			wake_up(&transfer->wq);

		/* if exists, get the next transfer on the list */
		if (!list_empty(&uengine->transfer_list)) {
			transfer = list_entry(uengine->transfer_list.next,
					      struct udma_xfer, entry);
			pr_debug("next transfer %px has %d descriptors\n",
				 transfer, transfer->desc_num);
		} else {
			/* no further transfers? */
			transfer = NULL;
		}
	}

	return transfer;
}

static void engine_service_resume(struct udma_engine *uengine)
{
	struct udma_xfer *transfer_started;

	/* engine stopped? */
	if (!uengine->running) {
		if (!list_empty(&uengine->transfer_list)) {
			/* (re)start engine */
			transfer_started = udma_start_engine(uengine);
			pr_debug("re-started channel %d "
				 "with pending transfer 0x%px\n",
				 uengine->channel, transfer_started);
		} else {
			pr_debug("no pending transfers, channel %d is idle.\n",
				 uengine->channel);
		}
	} else {
		/* engine is still running? */
		if (list_empty(&uengine->transfer_list))
			pr_warn("no queued transfers "
				"but channel %d is running!\n",
				uengine->channel);
	}
}

static void engine_service_work(struct work_struct *work)
{
	struct udma_engine *uengine;
	struct udma_xfer *transfer = NULL;

	uengine = container_of(work, struct udma_engine, work);
	BUG_ON(!uengine);

	/* lock the engine */
	spin_lock(&uengine->lock);
	if (!uengine->running) {
		pr_warn("channel %d was not running!\n", uengine->channel);
		goto unlock;
	}

	/* Process all transfer */
	transfer = engine_service_transfer(uengine);

	/* Restart the engine following the servicing */
	engine_service_resume(uengine);
unlock:
	spin_unlock(&uengine->lock);
}

static void p2p_transfer_destroy(struct udma_xfer *transfer)
{
	/* free linked list */
	kfree(transfer->ll);
	transfer->ll = NULL;
	transfer->dma_handle = 0;

	/* free transfer */
	kfree(transfer);
	transfer = NULL;
}

static void transfer_destroy(struct udma_xfer *transfer)
{
	struct udma_engine *uengine = transfer->uengine;

	/* free linked list */
	kfree(transfer->ll);
	transfer->ll = NULL;

	/* user space buffer was locked in on account of transfer? */
	if (transfer->sgm) {
		/* unmap scatterlist */
		/* the direction is needed to synchronize caches */
		dma_unmap_sg(&uengine->edev->pdev->dev, transfer->sgm->sgl,
			     transfer->sgm->mapped_pages,
			     transfer->dir ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		if (transfer->userspace) {
			/* dirty and unlock the pages */
			sgm_put_user_pages(transfer->sgm, transfer->dir);
		}
		transfer->sgm->mapped_pages = 0;
		sgm_destroy_mapper(transfer->sgm);
		transfer->sgl_nents = 0;
		transfer->sgm = NULL;
	}

	if (transfer->dma_handle) {
		if (!dma_pool_enable || transfer->is_boot) {
			dma_unmap_single(&uengine->edev->pdev->dev,
					 transfer->dma_handle, transfer->size,
					 transfer->dir ? DMA_FROM_DEVICE :
							 DMA_TO_DEVICE);
		}
		transfer->dma_handle = 0;
	}

	/* free transfer */
	kfree(transfer);
	transfer = NULL;
}

static int transfer_build_block(struct udma_xfer *transfer)
{
	transfer->ll = kzalloc(sizeof(struct udma_linked_list), GFP_KERNEL);
	if (!transfer->ll) {
		pr_info("No space for linked list.\n");
		return 0;
	}

	transfer->ll->host_phys = transfer->dma_handle;
	transfer->ll->dev_phys = transfer->dev_addr;
	transfer->ll->size = transfer->size;

	return 1;
}

static int transfer_build_ll(struct udma_xfer *transfer)
{
	int i = 0, j = 0;
	struct scatterlist *sgl = transfer->sgm->sgl;
	int new_desc;
	dma_addr_t cont_addr, addr;
	u32 cont_len, len = 0;
	u64 dev_addr = transfer->dev_addr;

	transfer->ll =
		kzalloc(transfer->sgl_nents * sizeof(struct udma_linked_list),
			GFP_KERNEL);
	if (!transfer->ll) {
		pr_info("No space for linked list.\n");
		return 0;
	}

	/* start first contiguous block */
	cont_addr = addr = sg_dma_address(&transfer->sgm->sgl[0]);
	cont_len = 0;

	/* iterate over all remaining entries but the last */
	/* merge contiguous blocks */
	for (i = 0; i < transfer->sgl_nents - 1; i++) {
		/* bus address of next entry i + 1 */
		dma_addr_t next = sg_dma_address(&sgl[i + 1]);
		/* length of this entry i */
		len = sg_dma_len(&sgl[i]);

		/* add entry i to current contiguous block length */
		cont_len += len;

		new_desc = 0;
		/* entry i + 1 is non-contiguous with entry i? */
		if (next != addr + len) {
			pr_debug("non-contiguous with sg entry %d\n", i + 1);
			new_desc = 1;
		} else if (cont_len > (UDMA_DESC_MAX_BYTES - PAGE_SIZE)) {
			/* entry i reached maximum transfer size? */
			pr_debug("break\n");
			new_desc = 1;
		}

		if (new_desc) {
			transfer->ll[j].host_phys = cont_addr;
			transfer->ll[j].dev_phys = dev_addr;
			transfer->ll[j].size = cont_len;

			/* proceed device address for next contiguous block */
			dev_addr += cont_len;

			/* start new contiguous block */
			cont_addr = next;
			cont_len = 0;
			j++;
		}
		/* goto entry i + 1 */
		addr = next;
	}

	/* i is the last entry in the scatterlist, add it to the last block */
	len = sg_dma_len(&sgl[i]);
	cont_len += len;
	BUG_ON(j > transfer->sgl_nents);

	transfer->ll[j].host_phys = cont_addr;
	transfer->ll[j].dev_phys = dev_addr;
	transfer->ll[j].size = cont_len;

	return (j + 1);
}

static struct udma_xfer *p2p_block_transfer_create(struct udma_engine *uengine,
						   u64 peer_addr, u32 size,
						   u64 dev_addr, int dir,
						   bool userspace)
{
	struct udma_xfer *transfer;

	/* allocate transfer data structure */
	transfer = kzalloc(sizeof(struct udma_xfer), GFP_KERNEL);
	if (!transfer)
		return NULL;

	/* remember direction of transfer, dir = 1 means outbound */
	transfer->dir = dir;
	transfer->dev_addr = dev_addr;
	transfer->size = size;
	transfer->uengine = uengine;

	transfer->dma_handle = peer_addr;

	pr_debug("transfer 0x%px mapped, dma address: 0x%llx.\n", transfer,
		 transfer->dma_handle);

	transfer->desc_num = transfer_build_block(transfer);

	pr_debug("transfer 0x%px has %d descriptors.\n", transfer,
		 transfer->desc_num);

	/* initialize wait queue */
	init_waitqueue_head(&transfer->wq);

	return transfer;
}

static struct udma_xfer *block_transfer_create(struct udma_engine *uengine,
					       u64 host_addr, u32 size,
					       u64 dev_addr, u8 group_id,
					       int dir, bool is_boot,
					       bool userspace)
{
	struct udma_xfer *transfer;

	/* allocate transfer data structure */
	transfer = kzalloc(sizeof(struct udma_xfer), GFP_KERNEL);
	if (!transfer)
		return NULL;

	/* remember direction of transfer, dir = 1 means outbound */
	transfer->dir = dir;
	transfer->dev_addr = dev_addr;
	transfer->size = size;
	transfer->uengine = uengine;
	transfer->group_id = group_id;
	transfer->is_boot = is_boot;
	if (dma_pool_enable && !is_boot)
		transfer->dma_handle = host_addr;
	else
		transfer->dma_handle =
			dma_map_single(&uengine->edev->pdev->dev,
				       (void *)host_addr, size,
				       dir ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	pr_debug("transfer 0x%px mapped, dma address: 0x%llx.\n", transfer,
		 transfer->dma_handle);

	transfer->desc_num = transfer_build_block(transfer);

	pr_debug("transfer 0x%px has %d descriptors.\n", transfer,
		 transfer->desc_num);

	/* initialize wait queue */
	init_waitqueue_head(&transfer->wq);

	return transfer;
}

static struct udma_xfer *sg_transfer_create(struct udma_engine *uengine,
					    u64 host_addr, u32 size,
					    u64 dev_addr, int dir,
					    bool userspace)
{
	int rc;
	struct scatterlist *sgl;
	struct udma_xfer *transfer;

	/* allocate transfer data structure */
	transfer = kzalloc(sizeof(struct udma_xfer), GFP_KERNEL);
	if (!transfer)
		return NULL;

	/* remember direction of transfer, dir = 1 means outbound */
	transfer->dir = dir;
	transfer->dev_addr = dev_addr;
	transfer->size = size;
	transfer->uengine = uengine;

	/* create virtual memory mapper */
	transfer->sgm = sgm_create_mapper(size);
	BUG_ON(!transfer->sgm);
	transfer->userspace = userspace;

	/* lock user pages in memory and create a scatter gather list */
	if (userspace)
		rc = sgm_get_user_pages(transfer->sgm, (void *)host_addr, size,
					dir);
	else
		rc = sgm_get_kernel_pages(transfer->sgm, (void *)host_addr,
					  size, dir);

	BUG_ON(rc < 0);

	sgl = transfer->sgm->sgl;

	BUG_ON(!sgl);
	BUG_ON(!transfer->sgm->mapped_pages);
	/* map all SG entries into DMA memory */
	transfer->sgl_nents = dma_map_sg(&uengine->edev->pdev->dev, sgl,
					 transfer->sgm->mapped_pages,
					 dir ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	pr_debug("transfer 0x%px has %d sg elements, "
		 "first page: 0x%px, offset: 0x%x, sg 0 dma address: 0x%llx.\n",
		 transfer, transfer->sgl_nents, (void *)sg_page(&sgl[0]),
		 sgl[0].offset, sg_dma_address(&sgl[0]));

	transfer->desc_num = transfer_build_ll(transfer);

	pr_debug("transfer 0x%px has %d descriptors.\n", transfer,
		 transfer->desc_num);

	/* initialize wait queue */
	init_waitqueue_head(&transfer->wq);

	return transfer;
}

static void transfer_set_desc(struct edge_dev *edev, u8 group_id, int channel,
			      struct udma_desc *desc,
			      struct udma_linked_list *ll, bool last)
{
	dma_addr_t pcie_addr = (dma_addr_t)(ll->host_phys);
	u64 axi_addr = ll->dev_phys;
	u32 len = ll->size;

	if (group_id == 1) {
		u64 unaligned_len = ll->dev_phys - (ll->dev_phys & (~0xFFF));
		axi_addr = edev->ob_bar0_addr + edev->region_offset +
			   (channel - 2) * edev->region_size + unaligned_len;
		desc->sys_lo_addr = cpu_to_le32(lower_32_bits(axi_addr));
		desc->sys_hi_addr = cpu_to_le32(upper_32_bits(axi_addr));
	} else {
		desc->sys_lo_addr = cpu_to_le32(lower_32_bits(axi_addr));
		desc->sys_hi_addr = cpu_to_le32(upper_32_bits(axi_addr));
	}
	desc->sys_attr = 0;
	desc->ext_lo_addr = cpu_to_le32(lower_32_bits(pcie_addr));
	desc->ext_hi_addr = cpu_to_le32(upper_32_bits(pcie_addr));
	desc->ext_attr = 0; // TODO:
	desc->ext_attr_hi = 0;
	desc->size_and_ctrl =
		cpu_to_le32((len & 0xFFFFFF) |
			    ((last == true) ? INT_EN : CONTINUE_LL) << 24);
	desc->status = 0;
}

static int calculate_free_entries(int size, int head, int tail)
{
	if (head == tail)
		return size;
	else if (head > tail)
		return size - (head - tail);
	else
		return tail - head - 1;
}

static int transfer_init(struct udma_xfer *transfer)
{
	struct udma_engine *uengine = transfer->uengine;
	struct udma_linked_list *ll = transfer->ll;
	struct udma_desc *desc_base = uengine->desc_virt_base;
	dma_addr_t desc_bus;
	u32 desc_idx;
	int i, rc = 0;

	transfer->desc_virt = uengine->desc_virt_base + uengine->head;
	transfer->desc_bus = uengine->desc_bus_base +
			     (sizeof(struct udma_desc) * uengine->head);
	transfer->desc_pos = uengine->head;

	pr_debug("transfer 0x%px desc bus = 0x%llx, first desc idx = %d.\n",
		 transfer, (u64)transfer->desc_bus, uengine->head);

	/* desc ring buffer full? */
	if (transfer->desc_num > calculate_free_entries(UDMA_DESC_MAX_NUM,
							uengine->head,
							uengine->tail)) {
		pr_err("Channel %d descriptor ring buffer is full!\n",
		       uengine->channel);
		return -EBUSY;
	}

	/* create singly-linked list for SG DMA controller */
	for (i = 0; i < transfer->desc_num - 1; i++) {
		desc_idx = uengine->head;
		transfer_set_desc(uengine->edev, transfer->group_id,
				  uengine->channel, &desc_base[desc_idx],
				  &ll[i], false);
		uengine->head = (uengine->head + 1) % UDMA_DESC_MAX_NUM;

		/* increment bus address to next in array */
		desc_bus = uengine->desc_bus_base +
			   (sizeof(struct udma_desc) * uengine->head);

		/* singly-linked list uses bus addresses */
		desc_base[desc_idx].next_lo_addr =
			cpu_to_le32(lower_32_bits(desc_bus));
		desc_base[desc_idx].next_hi_addr =
			cpu_to_le32(upper_32_bits(desc_bus));
	}
	/* { i = number - 1 } */
	/* zero the last descriptor next pointer */
	desc_idx = uengine->head;
	transfer_set_desc(uengine->edev, transfer->group_id, uengine->channel,
			  &desc_base[desc_idx], &ll[i], true);
	uengine->head = (uengine->head + 1) % UDMA_DESC_MAX_NUM;

	desc_base[desc_idx].next_lo_addr = 0;
	desc_base[desc_idx].next_hi_addr = 0;

	return rc;
}

static void transfer_dump(struct udma_xfer *transfer)
{
	int i, j;
	u32 *p;
	struct udma_engine *uengine = transfer->uengine;
	struct udma_desc *desc_base = uengine->desc_virt_base;
	u32 first_desc_idx = ((u64)transfer->desc_virt - (u64)desc_base) /
			     sizeof(struct udma_desc);
	dma_addr_t desc_bus;
	u32 desc_idx;

	char *const desc_ele_name[] = {
		"sys_lo_addr", "sys_hi_addr",  "sys_attr",    "ext_lo_addr",
		"ext_hi_addr", "ext_attr",     "ext_attr_hi", "size_and_ctrl",
		"status",      "next_lo_addr", "next_hi_addr"
	};

	pr_info("Descriptor Entry:\n");
	for (i = 0; i < transfer->desc_num; i++) {
		desc_idx = (first_desc_idx + i) % UDMA_DESC_MAX_NUM;
		p = (u32 *)(desc_base + desc_idx);
		desc_bus = uengine->desc_bus_base +
			   desc_idx * sizeof(struct udma_desc);
		for (j = 0; j < 11; j++, p++)
			pr_info("\t[transfer 0x%px desc %d] 0x%llx/0x%02x: "
				"0x%08x %s\n",
				transfer, i, desc_bus, j * 0x4, le32_to_cpu(*p),
				desc_ele_name[j]);
	}
}

#if 0
static void transfer_append(struct udma_xfer *transfer)
{
	struct udma_engine *uengine = transfer->uengine;
	struct udma_xfer *prev;
	struct udma_desc *desc_base = uengine->desc_virt_base;
	u32 first_desc_idx, last_desc_idx;

	/* queue is not empty? try to chain the descriptor lists */
	if (!list_empty(&uengine->transfer_list)) {
		pr_debug("transfer list not empty, append transfer 0x%px\n",
			transfer);
		/* get prev transfer queued on the engine */
		prev = list_entry(uengine->transfer_list.prev,
					struct udma_xfer, entry);
		first_desc_idx = ((u64)prev->desc_virt -
			(u64)uengine->desc_virt_base) /
			sizeof(struct udma_desc);
		last_desc_idx = (first_desc_idx + prev->desc_num - 1) %
			UDMA_DESC_MAX_NUM;

		/* link the last transfer's last descriptor to this transfer */
		desc_base[last_desc_idx].next_lo_addr =
			cpu_to_le32(lower_32_bits(transfer->desc_bus));
		desc_base[last_desc_idx].next_hi_addr =
			cpu_to_le32(upper_32_bits(transfer->desc_bus));
		/* do not stop now that there is a linked transfers */
		desc_base[last_desc_idx].size_and_ctrl.ctrl_bits =
			CONTINUE_LL | INT_EN;

		pr_debug("transfer 0x%px with %d descriptors "
			"appended to channel %d\n",
			transfer, transfer->desc_num, uengine->channel);

		/* queue is empty */
	} else {
		if (uengine->running)
			pr_debug("queue empty, but engine is running!\n");
		else
			pr_debug("queue empty and engine idle\n");
		/* engine should not be running */
		WARN_ON(uengine->running);
	}
}
#endif

static int transfer_queue(struct udma_xfer *transfer)
{
	struct udma_engine *uengine = transfer->uengine;
	struct udma_xfer *transfer_started;

	BUG_ON(transfer->desc_num == 0);

	/*
	 * either the engine is still busy and we will end up in the
	 * service handler later, or the engine is idle and we have to
	 * start it with this transfer here
	 */
	//transfer_append(transfer);

	/* add transfer to the tail of the engine transfer queue */
	list_add_tail(&transfer->entry, &uengine->transfer_list);
	/* mark the transfer as submitted */
	transfer->state = TRANSFER_STATE_SUBMITTED;

	/* engine is idle? */
	if (!uengine->running) {
		/* start engine */
		transfer_started = udma_start_engine(uengine);
		pr_debug("transfer 0x%px started, with channel %d engine.\n",
			 transfer_started, uengine->channel);
	} else {
		pr_debug("transfer 0x%px queued, with channel %d engine.\n",
			 transfer, uengine->channel);
	}

	return 0;
}

/*
 * should hold the engine->lock;
 */
static void transfer_abort(struct udma_xfer *transfer)
{
	struct udma_engine *uengine = transfer->uengine;
	struct udma_xfer *xfer, *tmp;
	int found = 0;

	pr_info("abort transfer 0x%px with %d descriptors in device %d channel %d, "
		"desc_pos %d\n",
		transfer, transfer->desc_num, uengine->edev->pub.device_id,
		uengine->channel, transfer->desc_pos);

	transfer_dump(transfer);
	udma_dump(uengine);

	list_for_each_entry_safe (xfer, tmp, &uengine->transfer_list, entry) {
		if (xfer == transfer) {
			found = 1;
			if (transfer->state == TRANSFER_STATE_SUBMITTED ||
			    transfer->state == TRANSFER_STATE_STARTED)
				transfer->state = TRANSFER_STATE_ABORTED;
			break;
		}
	}

	if (found == 0)
		pr_info("abort fail: transfer 0x%p NOT found in channel %d\n",
			transfer, uengine->channel);
}

/*
 * should hold the engine->lock;
 */
static void transfer_timeout(struct udma_xfer *transfer)
{
	struct udma_engine *uengine = transfer->uengine;
	struct udma_xfer *xfer, *tmp;
	int found = 0;

	pr_info("transfer 0x%px with %d descriptors in device %d channel %d timeout, "
		"desc_pos %d\n",
		transfer, transfer->desc_num, uengine->edev->pub.device_id,
		uengine->channel, transfer->desc_pos);

	transfer_dump(transfer);
	udma_dump(uengine);

	list_for_each_entry_safe (xfer, tmp, &uengine->transfer_list, entry) {
		if (xfer == transfer) {
			found = 1;
			list_del(&xfer->entry);
			/* do not update tail for timeout transfer */
			//uengine->tail =
			//	(transfer->desc_pos + transfer->desc_num) %
			//	UDMA_DESC_MAX_NUM;

			transfer->state = TRANSFER_STATE_ABORTED;
			break;
		}
	}

	if (found == 0)
		pr_info("abort fail: transfer 0x%p NOT found in channel %d\n",
			transfer, uengine->channel);
}

static int transfer_submit(struct udma_xfer *transfer, u32 timeout_ms)
{
	struct udma_engine *uengine = transfer->uengine;
	struct udma_xfer *xfer, *tmp;
	int rc = 0;

	/* lock the engine state */
	spin_lock(&uengine->lock);
	rc = transfer_init(transfer);
	if (rc < 0)
		goto unlock;

	//transfer_dump(transfer);

	rc = transfer_queue(transfer);
	if (rc < 0) {
		pr_err("unable to submit transfer 0x%px to channel %d.\n",
		       transfer, transfer->uengine->channel);
		goto unlock;
	}

	/* poll dma state */
	if (transfer->is_boot)
		edge_udma_poll_state(uengine);

	/* unlock the engine state */
	spin_unlock(&uengine->lock);

	rc = wait_event_interruptible_timeout(
		transfer->wq,
		(transfer->state == TRANSFER_STATE_COMPLETED) ||
			(transfer->state == TRANSFER_STATE_FAILED),
		msecs_to_jiffies(timeout_ms));

	spin_lock(&uengine->lock);
	switch (transfer->state) {
	case TRANSFER_STATE_COMPLETED:
		pr_debug("transfer %px completed\n", transfer);
		rc = 0;
		break;
	case TRANSFER_STATE_FAILED:
		pr_info("transfer %px failed\n", transfer);
		rc = -EIO;
		break;
	default:
		if (rc == 0) {
			/* transfer timeout:
			   if transfer not started,
			   delete transfer from transfer list.
			   if transfer started,
			   in this case, done/error interrupt will never come,
			   try to resume the engine.
			*/
			pr_info("transfer 0x%px timeout\n", transfer);
			if (transfer->state == TRANSFER_STATE_STARTED)
				uengine->running = 0;

			transfer_timeout(transfer);
			engine_service_resume(uengine);
			rc = -EIO;
		} else if (rc == -ERESTARTSYS) {
			/* transfer is interrupted by signal,
			   let it go on, but ignore the transfer */
			pr_info("transfer 0x%px interrupted by signal\n",
				transfer);
			transfer_abort(transfer);
		}
		break;
	}
unlock:
	spin_unlock(&uengine->lock);
	if (rc == -ERESTARTSYS) {
		/* wait interrupt service routine to wake up,
		   to avoid early freeing of struct transfer */
		rc = wait_event_timeout(
			transfer->wq,
			(transfer->state == TRANSFER_STATE_COMPLETED) ||
				(transfer->state == TRANSFER_STATE_FAILED),
			msecs_to_jiffies(timeout_ms));
		if (rc == 0) {
			/* timeout: dma channel is frozen and interrupt will never come,
			   we should delete tranfer from transfer list */
			pr_warn("device %d channel %d is frozen, delete transfer 0x%px from list\n",
				uengine->edev->pub.device_id, uengine->channel,
				transfer);
			list_for_each_entry_safe (
				xfer, tmp, &uengine->transfer_list, entry) {
				if (xfer == transfer) {
					list_del(&xfer->entry);
					/* channel is frozen, updating tail is not necessary */
					//uengine->tail = (transfer->desc_pos +
					//		 transfer->desc_num) %
					//		UDMA_DESC_MAX_NUM;
					break;
				}
			}
		}
		pr_debug("transfer 0x%px state=%d, rc=%d\n", transfer,
			 transfer->state, rc);
		rc = -EIO;
	}
	return rc;
}

static int udma_p2p_block_transfer(struct edge_dev *edev, u64 peer_phys,
				   u64 dev_addr, u32 size, bool is_outbound,
				   u32 channel)
{
	int rc = 0;
	u32 remaining = size;
	struct udma_engine *uengine = edev->uengine[channel];
	struct udma_xfer *transfer;
	u32 done = 0;
	u32 xfer_len;

	spin_lock(&uengine->lock);
	if (is_outbound) {
		uengine->ustats.tx.queued_tasks++;
		uengine->ustats.tx.queued_bytes += remaining;
	} else {
		uengine->ustats.rx.queued_tasks++;
		uengine->ustats.rx.queued_bytes += remaining;
	}
	spin_unlock(&uengine->lock);

	while ((rc == 0) && (remaining > 0)) {
		/* max transfer size for each transfer request */
		if (remaining > UDMA_BLK_TRANSFER_MAX_BYTES)
			xfer_len = UDMA_BLK_TRANSFER_MAX_BYTES;
		else
			xfer_len = remaining;

		transfer =
			p2p_block_transfer_create(uengine, peer_phys, xfer_len,
						  dev_addr, is_outbound, true);
		pr_debug("create channel %d transfer 0x%px.\n",
			 uengine->channel, transfer);

		if (!transfer) {
			spin_lock(&uengine->lock);
			if (is_outbound) {
				uengine->ustats.tx.queued_bytes -= remaining;
			} else {
				uengine->ustats.rx.queued_bytes -= remaining;
			}
			spin_unlock(&uengine->lock);
			remaining = 0;
			rc = -EIO;
			break;
		}

		rc = transfer_submit(transfer, 10000);
		if (rc) {
			spin_lock(&uengine->lock);
			if (is_outbound) {
				uengine->ustats.tx.queued_bytes -= remaining;
			} else {
				uengine->ustats.rx.queued_bytes -= remaining;
			}
			spin_unlock(&uengine->lock);
			xfer_len = 0;
			remaining = 0;
		}

		p2p_transfer_destroy(transfer);

		/* calculate the next transfer */
		peer_phys += xfer_len;
		remaining -= xfer_len;
		done += xfer_len;
		dev_addr += xfer_len;
		spin_lock(&uengine->lock);
		if (is_outbound) {
			uengine->ustats.tx.queued_bytes -= xfer_len;
			uengine->ustats.tx.bytes_stat += xfer_len;
		} else {
			uengine->ustats.rx.queued_bytes -= xfer_len;
			uengine->ustats.rx.bytes_stat += xfer_len;
		}
		spin_unlock(&uengine->lock);
		pr_debug("block dma: remain 0x%x bytes, done 0x%x bytes\n",
			 remaining, done);
	}

	spin_lock(&uengine->lock);
	if (is_outbound) {
		uengine->ustats.tx.queued_tasks--;
		uengine->ustats.tx.tasks_stat++;
	} else {
		uengine->ustats.rx.queued_tasks--;
		uengine->ustats.rx.tasks_stat++;
	}
	spin_unlock(&uengine->lock);

	/* return error or else number of bytes */
	return rc ? rc : done;
}

static int udma_block_transfer(struct edge_dev *edev, u64 host_virt,
			       u64 dev_addr, u32 size, u8 group_id,
			       bool is_outbound, bool is_boot, u32 channel)
{
	int rc = 0;
	u32 remaining = size;
	struct udma_engine *uengine = edev->uengine[channel];
	struct udma_xfer *transfer;
	u32 done = 0;
	u32 xfer_len;

	spin_lock(&uengine->lock);
	if (is_outbound) {
		uengine->ustats.tx.queued_tasks++;
		uengine->ustats.tx.queued_bytes += remaining;
	} else {
		uengine->ustats.rx.queued_tasks++;
		uengine->ustats.rx.queued_bytes += remaining;
	}
	spin_unlock(&uengine->lock);

	while ((rc == 0) && (remaining > 0)) {
		/* max transfer size for each transfer request */
		if (remaining > UDMA_BLK_TRANSFER_MAX_BYTES)
			xfer_len = UDMA_BLK_TRANSFER_MAX_BYTES;
		else
			xfer_len = remaining;

		transfer = block_transfer_create(uengine, host_virt, xfer_len,
						 dev_addr, group_id,
						 is_outbound, is_boot, true);
		pr_debug("create channel %d transfer 0x%px.\n",
			 uengine->channel, transfer);

		if (!transfer) {
			spin_lock(&uengine->lock);
			if (is_outbound) {
				uengine->ustats.tx.queued_bytes -= remaining;
			} else {
				uengine->ustats.rx.queued_bytes -= remaining;
			}
			spin_unlock(&uengine->lock);
			remaining = 0;
			rc = -EIO;
			break;
		}

		rc = transfer_submit(transfer, 10000);
		if (rc) {
			spin_lock(&uengine->lock);
			if (is_outbound) {
				uengine->ustats.tx.queued_bytes -= remaining;
			} else {
				uengine->ustats.rx.queued_bytes -= remaining;
			}
			spin_unlock(&uengine->lock);
			xfer_len = 0;
			remaining = 0;
		}

		transfer_destroy(transfer);

		/* calculate the next transfer */
		host_virt += xfer_len;
		remaining -= xfer_len;
		done += xfer_len;
		dev_addr += xfer_len;
		spin_lock(&uengine->lock);
		if (is_outbound) {
			uengine->ustats.tx.queued_bytes -= xfer_len;
			uengine->ustats.tx.bytes_stat += xfer_len;
		} else {
			uengine->ustats.rx.queued_bytes -= xfer_len;
			uengine->ustats.rx.bytes_stat += xfer_len;
		}
		spin_unlock(&uengine->lock);
		pr_debug("block dma: remain 0x%x bytes, done 0x%x bytes\n",
			 remaining, done);
	}

	spin_lock(&uengine->lock);
	if (is_outbound) {
		uengine->ustats.tx.queued_tasks--;
		uengine->ustats.tx.tasks_stat++;
	} else {
		uengine->ustats.rx.queued_tasks--;
		uengine->ustats.rx.tasks_stat++;
	}
	spin_unlock(&uengine->lock);

	/* return error or else number of bytes */
	return rc ? rc : done;
}

static int udma_sg_transfer(struct edge_dev *edev, u64 host_virt, u64 dev_addr,
			    u32 size, bool is_outbound, u32 channel)
{
	int rc = 0;
	u32 remaining = size;
	struct udma_engine *uengine = edev->uengine[channel];
	struct udma_xfer *transfer;
	u32 done = 0;
	u32 xfer_len;

	spin_lock(&uengine->lock);
	if (is_outbound) {
		uengine->ustats.tx.queued_tasks++;
		uengine->ustats.tx.queued_bytes += remaining;
	} else {
		uengine->ustats.rx.queued_tasks++;
		uengine->ustats.rx.queued_bytes += remaining;
	}
	spin_unlock(&uengine->lock);

	while ((rc == 0) && (remaining > 0)) {
		/* max transfer size for each transfer request */
		if (remaining > UDMA_SG_TRANSFER_MAX_BYTES)
			xfer_len = UDMA_SG_TRANSFER_MAX_BYTES;
		else
			xfer_len = remaining;

		transfer = sg_transfer_create(uengine, host_virt, xfer_len,
					      dev_addr, is_outbound, true);
		pr_debug("create channel %d transfer 0x%px.\n",
			 uengine->channel, transfer);

		if (!transfer) {
			spin_lock(&uengine->lock);
			if (is_outbound) {
				uengine->ustats.tx.queued_bytes -= remaining;
			} else {
				uengine->ustats.rx.queued_bytes -= remaining;
			}
			spin_unlock(&uengine->lock);
			remaining = 0;
			rc = -EIO;
			break;
		}

		rc = transfer_submit(transfer, 10000);
		if (rc) {
			spin_lock(&uengine->lock);
			if (is_outbound) {
				uengine->ustats.tx.queued_bytes -= remaining;
			} else {
				uengine->ustats.rx.queued_bytes -= remaining;
			}
			spin_unlock(&uengine->lock);
			xfer_len = 0;
			remaining = 0;
		}

		transfer_destroy(transfer);

		/* calculate the next transfer */
		host_virt += xfer_len;
		remaining -= xfer_len;
		done += xfer_len;
		dev_addr += xfer_len;
		spin_lock(&uengine->lock);
		if (is_outbound) {
			uengine->ustats.tx.queued_bytes -= xfer_len;
			uengine->ustats.tx.bytes_stat += xfer_len;
		} else {
			uengine->ustats.rx.queued_bytes -= xfer_len;
			uengine->ustats.rx.bytes_stat += xfer_len;
		}
		spin_unlock(&uengine->lock);
		pr_debug("sg dma: remain 0x%x bytes, done 0x%x bytes\n",
			 remaining, done);
	}

	spin_lock(&uengine->lock);
	if (is_outbound) {
		uengine->ustats.tx.queued_tasks--;
		uengine->ustats.tx.tasks_stat++;
	} else {
		uengine->ustats.rx.queued_tasks--;
		uengine->ustats.rx.tasks_stat++;
	}
	spin_unlock(&uengine->lock);

	/* return error or else number of bytes */
	return rc ? rc : done;
}

static u32 udma_alloc_channel(struct edge_dev *edev, unsigned long mask)
{
	struct udma_engine *uengine;
	u32 ch, candidate = 0;
	u32 min_queued_bytes = 0xFFFFFFFF;

	for_each_set_bit (ch, (unsigned long *)&mask, EDGE_DMA_CH_NUM) {
		uengine = edev->uengine[ch];
		spin_lock(&uengine->lock);
		if (min_queued_bytes > (uengine->ustats.tx.queued_bytes +
					uengine->ustats.rx.queued_bytes)) {
			min_queued_bytes = (uengine->ustats.tx.queued_bytes +
					    uengine->ustats.rx.queued_bytes);
			candidate = ch;
		}
		spin_unlock(&uengine->lock);
	}

	return candidate;
}

static int udma_p2p_transfer(struct edge_dev *edev, struct xfer_task *dma_task,
			     bool is_outbound)
{
	u64 peer_phys = dma_task->host_phys;
	u64 dev_addr = dma_task->dev_addr;
	u32 size = dma_task->xfer_size;
	u32 ch_mask = dma_task->ch_mask & EDGE_P2P_CH_MASK;
	u32 channel = 0;

	if (!ch_mask)
		ch_mask = EDGE_P2P_CH_MASK;

	channel = udma_alloc_channel(edev, ch_mask);

	if (peer_phys)
		return udma_p2p_block_transfer(edev, peer_phys, dev_addr, size,
					       is_outbound, channel);

	pr_err("dma address invalid.\n");
	return -EINVAL;
}

static int udma_transfer(struct edge_dev *edev, struct xfer_task *dma_task,
			 bool is_outbound, bool is_boot)
{
	u64 host_virt = dma_task->host_virt;
	u64 host_phys = dma_task->host_phys;
	u64 dev_addr = dma_task->dev_addr;
	u32 size = dma_task->xfer_size;
	u32 ch_mask = 0;
	u8 group_id = dma_task->group_id;
	u32 channel = 0;
	int rc;

	if (edev->pub.magic == X6000_DS_MAGIC) {
		ch_mask = (dma_task->ch_mask & EDGE_DMA_CH_MASK) ?
				  (dma_task->ch_mask & EDGE_DMA_CH_MASK) :
				  EDGE_DMA_CH_MASK;
	} else {
		if (group_id == 0)
			ch_mask = (dma_task->ch_mask & 0x3) ?
					  (dma_task->ch_mask & 0x3) :
					  0x3;
		else if (group_id == 1)
			ch_mask = (dma_task->ch_mask & 0xC) ?
					  (dma_task->ch_mask & 0xC) :
					  0xC;
	}

	if (ch_mask != dma_task->ch_mask) {
		pr_debug("channel mask 0x%x not support, reset to 0x%x\n",
			 dma_task->ch_mask, ch_mask);
	}

	channel = udma_alloc_channel(edev, ch_mask);

	if (host_phys) {
		DMA_DESC_ADDR_SAVE(is_boot, edev, channel);
		if (host_phys) {
			if (dma_pool_enable && !is_boot) {
				rc = udma_block_transfer(edev, host_phys,
							 dev_addr, size,
							 group_id, is_outbound,
							 is_boot, channel);
			} else
				rc = udma_block_transfer(edev, host_virt,
							 dev_addr, size,
							 group_id, is_outbound,
							 is_boot, channel);
		}
		DMA_DESC_ADDR_RESTORE(is_boot, edev, channel);
	} else if (host_virt) {
		rc = udma_sg_transfer(edev, host_virt, dev_addr, size,
				      is_outbound, channel);
	} else {
		rc = -EINVAL;
		pr_err("dma address invalid.\n");
	}
	return rc;
}

static int ioctl_do_udma_p2pob_xfer(struct edge_dev *edev, unsigned long arg)
{
	struct xfer_task dma_task;
	int rc = 0;
	u32 val;

	if (edev->pub.magic != X6000_DS_MAGIC) {
		return -1;
	}


	val = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR);
	if (val != EDGE_LINUX_MAGIC) {
		pr_err("Device is not in linux state.\n");
		return -EPERM;
	}

	rc = copy_from_user(&dma_task, (struct xfer_task __user *)arg,
			    sizeof(struct xfer_task));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	rc = udma_p2p_transfer(edev, &dma_task, true);
	return (rc < 0) ? rc : 0;
}

static int ioctl_do_udma_p2pib_xfer(struct edge_dev *edev, unsigned long arg)
{
	struct xfer_task dma_task;
	int rc = 0;
	u32 val;
	
	if (edev->pub.magic != X6000_DS_MAGIC) {
		return -1;
	}

	val = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR);
	if (val != EDGE_LINUX_MAGIC) {
		pr_err("Device is not in linux state.\n");
		return -EPERM;
	}

	rc = copy_from_user(&dma_task, (struct xfer_task __user *)arg,
			    sizeof(struct xfer_task));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	rc = udma_p2p_transfer(edev, &dma_task, false);
	return (rc < 0) ? rc : 0;
}

static int ioctl_do_udma_d2h_xfer(struct edge_dev *edev, unsigned long arg)
{
	struct xfer_task dma_task;
	int rc = 0, barno;
	u32 val;

	rc = copy_from_user(&dma_task, (struct xfer_task __user *)arg,
			    sizeof(struct xfer_task));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	barno = dma_task.group_id == 1 ? 1 : 0;
	val = edge_readl_safe(edev, barno, BOOT_STATE_MAGIC_ADDR);
	if (val != EDGE_LINUX_MAGIC) {
		pr_err("Device %d is not in linux state in %s.\n",
		       edev->pub.device_id, __FUNCTION__);
		return -EPERM;
	}

	rc = udma_transfer(edev, &dma_task, true, false);
	return (rc < 0) ? rc : 0;
}

static int ioctl_do_udma_h2d_xfer(struct edge_dev *edev, unsigned long arg)
{
	struct xfer_task dma_task;
	int rc = 0, barno;
	u32 val;

	rc = copy_from_user(&dma_task, (struct xfer_task __user *)arg,
			    sizeof(struct xfer_task));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	barno = dma_task.group_id == 1 ? 1 : 0;
	val = edge_readl_safe(edev, barno, BOOT_STATE_MAGIC_ADDR);
	if (val != EDGE_LINUX_MAGIC) {
		pr_err("Device %d is not in linux state in %s.\n",
		       edev->pub.device_id, __FUNCTION__);
		return -EPERM;
	}

	rc = udma_transfer(edev, &dma_task, false, false);
	return (rc < 0) ? rc : 0;
}

static int mmio_xfer(struct edge_dev *edev, unsigned long arg, bool is_h2d)
{
	struct xfer_task mmio_task;
	phys_addr_t xfer_phys, vmm_base_phys, xfer_off;
	phys_addr_t bar2_phys, region_base_phys, region_base_off;
	u32 die_id, apt_region;
	int rc = 0, mmz_barno, boot_barno;
	u32 val;

	rc = copy_from_user(&mmio_task, (struct xfer_task __user *)arg,
			    sizeof(struct xfer_task));
	if (rc < 0) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}
	boot_barno = mmio_task.group_id ? 1 : 0;
	val = edge_readl_safe(edev, boot_barno, BOOT_STATE_MAGIC_ADDR);
	if (val != EDGE_LINUX_MAGIC) {
		pr_err("Device %d group %u is not in linux state in %s.\n",
		       edev->pub.device_id, mmio_task.group_id, __FUNCTION__);
		return -EPERM;
	}

	xfer_phys = mmio_task.dev_addr;
	region_base_off = 0;
	xfer_off = xfer_phys - 0x2B0000000;
	die_id = (xfer_phys >> 32) / 4;
	if (edev->pub.magic == X6000_DS_MAGIC) {
		mmz_barno = 2;
		apt_region = mmio_task.group_id ? 29 : 28;
	} else if (edev->pub.magic == X6000_LM_MAGIC) {
		mmz_barno = mmio_task.group_id ? 4 : 2;
		apt_region = mmio_task.group_id ? 12 : 28;
	} else {
		mmz_barno = mmio_task.group_id ? 4 : 2;
		switch (die_id) {
		case 0:
			apt_region = mmio_task.group_id ? 12 : 28;
			break;
		case 1:
			apt_region = 29;
			break;
		case 7:
			apt_region = 30;
			break;
		case 8:
			apt_region = 31;
			break;
		default:
			pr_err("Unsupported device %d group %u address 0x%llx\n",
			       edev->pub.device_id, mmio_task.group_id,
			       mmio_task.dev_addr);
			return -EINVAL;
		}
	}

	vmm_base_phys = ((u64)edge_readl_safe(
				edev, boot_barno,
				REG_PCIE_APT_REMAP_BASE_LO_ADDR(apt_region))) |
			((u64)edge_readl_safe(
				 edev, boot_barno,
				 REG_PCIE_APT_REMAP_BASE_HI_ADDR(apt_region))
			 << 32);

	if (xfer_phys < vmm_base_phys) {
		pr_err("Device %d group %u address 0x%llx invalid.\n",
		       edev->pub.device_id, mmio_task.group_id, xfer_phys);
		return -EINVAL;
	}
	xfer_off = xfer_phys - vmm_base_phys;

	bar2_phys = EDGE_BAR2_APT_BASE_ADDR;
	region_base_phys =
		((u64)edge_readl_safe(edev, boot_barno,
				      REG_PCIE_APT_BASE_LO_ADDR(apt_region))) |
		((u64)edge_readl_safe(edev, boot_barno,
				      REG_PCIE_APT_BASE_HI_ADDR(apt_region))
		 << 32);
	region_base_off = region_base_phys - bar2_phys;

	if (edev->bar[mmz_barno]) {
		if (is_h2d) {
			rc = copy_from_user(edev->bar[mmz_barno] +
						    region_base_off + xfer_off,
					    (void *)mmio_task.host_virt,
					    mmio_task.xfer_size);
		} else {
			rc = copy_to_user((void *)mmio_task.host_virt,
					  edev->bar[mmz_barno] +
						  region_base_off + xfer_off,
					  mmio_task.xfer_size);
		}
		if (rc) {
			pr_err("Failed to copy device %d group %u to user space %p\n",
			       edev->pub.device_id, mmio_task.group_id,
			       (void *)mmio_task.host_virt);
			return -EFAULT;
		}
	} else {
		pr_err("Device %d group %u bar[%d] address is invalid\n",
		       edev->pub.device_id, mmio_task.group_id, mmz_barno);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_do_mmio_d2h_xfer(struct edge_dev *edev, unsigned long arg)
{
	return mmio_xfer(edev, arg, false);
}

static int ioctl_do_mmio_h2d_xfer(struct edge_dev *edev, unsigned long arg)
{
	return mmio_xfer(edev, arg, true);
}

static int boot_wait_state(struct edge_dev *edev, int barno, bool is_write,
			   int timeout)
{
	u32 offset = D2H_MSG_READY_ADDR, cond = 1;

	if (is_write) { /* write */
		offset = H2D_MSG_READY_ADDR;
		cond = 0;
	}

	if (timeout <= 0) {
		do {
			if (edge_readl_safe(edev, barno, offset) == cond)
				break;

			usleep_range(50, 100);
			if (signal_pending(current))
				return -ERESTARTSYS;
		} while (1);
	} else {
		unsigned long t = jiffies + msecs_to_jiffies(timeout);
		do {
			if (edge_readl_safe(edev, barno, offset) == cond)
				break;

			if (time_after(jiffies, t)) {
				pr_err("Boot %s timeout.\n",
				       is_write ? "write" : "read");
				return -ETIMEDOUT;
			}

			usleep_range(50, 100);
			if (signal_pending(current))
				return -ERESTARTSYS;
		} while (1);
	}
	return 0;
}

static int ioctl_do_boot_read(struct edge_dev *edev, unsigned long arg)
{
	struct boot_rw brw;
	int rc = 0, barno;

	rc = copy_from_user(&brw, (struct boot_rw __user *)arg,
			    sizeof(struct boot_rw));
	if (rc < 0) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}
	barno = brw.group_id ? 1 : 0;

	rc = boot_wait_state(edev, barno, false, brw.timeout);
	if (rc < 0)
		return rc;

	if (edev->bar[barno]) {
		rc = copy_to_user(brw.buf, edev->bar[barno] + D2H_MSG_BUF_ADDR,
				  brw.count);
		if (rc) {
			pr_err("Failed to copy to user space %p\n", brw.buf);
			return -EFAULT;
		}
	} else {
		pr_err("Failed to copy to user space %p\n", brw.buf);
		return -EINVAL;
	}

	edge_writel_safe(edev, barno, 0, D2H_MSG_READY_ADDR);
	return 0;
}

static int ioctl_do_boot_write(struct edge_dev *edev, unsigned long arg)
{
	struct boot_rw brw;
	int rc = 0, barno;

	rc = copy_from_user(&brw, (struct boot_rw __user *)arg,
			    sizeof(struct boot_rw));
	if (rc < 0) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}
	barno = brw.group_id ? 1 : 0;

	rc = boot_wait_state(edev, barno, true, brw.timeout);
	if (rc < 0)
		return rc;

	if (edev->bar[barno]) {
		rc = copy_from_user(edev->bar[barno] + H2D_MSG_BUF_ADDR,
				    brw.buf, brw.count);
		if (rc < 0) {
			pr_err("Failed to copy from user space %p\n", brw.buf);
			return -EFAULT;
		}
	} else {
		pr_err("Failed to copy from user space %p\n", brw.buf);
		return -EINVAL;
	}

	edge_writel_safe(edev, barno, 1, H2D_MSG_READY_ADDR);
	return 0;
}

static int ioctl_do_set_device_id(struct edge_dev *edev, unsigned long arg)
{
	u32 device_id, val;
	int rc = 0;

	rc = copy_from_user(&device_id, (unsigned int __user *)arg,
			    sizeof(unsigned int));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	edev->pub.device_id = device_id;

	rc = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR);
	if (edev->pub.ext_init == 1 || rc != EDGE_BOOTROM_MAGIC) {
		val = edge_readl_safe(edev, 0, REG_PCIE_SCRATCH_REG1_ADDR);
		edge_writel_safe(edev, 0, val | (device_id << 24) | BIT(31),
				 REG_PCIE_SCRATCH_REG1_ADDR);
		edge_writel_safe(edev, 0, (0x5A5A << 16) | device_id,
				 DEVICE_ID_ADDR_OFFSET);
		rc = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR + 4 * 8);
		if (rc >= EDGE_BL2_MAGIC && rc != 0xFFFFFFFF) {
			val = edge_readl_safe(edev, 1,
					      REG_PCIE_SCRATCH_REG1_ADDR);
			edge_writel_safe(edev, 1,
					 val | (device_id << 24) | BIT(31),
					 REG_PCIE_SCRATCH_REG1_ADDR);
			edge_writel_safe(edev, 1, (0x5A5A << 16) | device_id,
					 DEVICE_ID_ADDR_OFFSET);
		}
		wmb();
	}

	pr_info("device id %d bind to /dev/%08x_%08x%016llx.\n",
		edev->pub.device_id, edev->pub.key, edev->pub.unique_id_hi,
		edev->pub.unique_id_lo);

	mod_timer(&edev->event_timer, jiffies + msecs_to_jiffies(500));
	return 0;
}

static int ioctl_do_get_channel_status(struct edge_dev *edev, unsigned long arg)
{
	struct channel_status chsts;
	struct udma_engine *uengine;
	int rc = 0;

	rc = copy_from_user(&chsts, (struct channel_status __user *)arg,
			    sizeof(struct channel_status));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	if (chsts.ch_id >= EDGE_DMA_CH_NUM || !edev->uengine[chsts.ch_id]) {
		pr_err("Invalid channel id=%d\n", chsts.ch_id);
		return -EINVAL;
	}

	uengine = edev->uengine[chsts.ch_id];
	spin_lock(&uengine->lock);
	chsts.running = uengine->running ? true : false;
	chsts.queued_tasks = uengine->ustats.tx.queued_tasks +
			     uengine->ustats.rx.queued_tasks;
	chsts.queued_bytes = uengine->ustats.tx.queued_bytes +
			     uengine->ustats.rx.queued_bytes;
	spin_unlock(&uengine->lock);

	rc = copy_to_user((void __user *)arg, &chsts,
			  sizeof(struct channel_status));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EFAULT;
	}

	return 0;
}

static int ioctl_do_get_ep_flags(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;

	if (edev->bar[0]) {
		rc = copy_to_user((void __user *)arg,
				  edev->bar[0] + REG_PCIE_SCRATCH_REG1_ADDR,
				  sizeof(unsigned int));
		if (rc) {
			pr_err("Failed to copy to user space 0x%lx\n", arg);
			return -EINVAL;
		}
	} else {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_do_get_boot_state(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0, barno, i;
	u32 val, die_ids[8], card_type = edev->pub.card_type;
	struct group_boot_state group_state;
	enum boot_state state;
	uint32_t die_ids_4[] = { 0, 1, 7, 8 };

	rc = copy_from_user(&group_state, (struct group_boot_state __user *)arg,
			    sizeof(group_state));
	if (rc) {
		pr_err("Failed to copy group boot state from user space 0x%lx\n",
		       arg);
		return -EFAULT;
	}

	if (card_type == CARD_TYPE_IPU_X6000_PCIE) {
		group_state.chiptype_id_count = 4;
		memcpy(die_ids, die_ids_4, sizeof(die_ids_4));
	} else {
		group_state.chiptype_id_count = 1;
		die_ids[0] = 0;
	}

	barno = group_state.group_id ? 1 : 0;
	for (i = 0; i < group_state.chiptype_id_count; i++) {
		val = edge_readl_safe(edev, barno,
				      BOOT_STATE_MAGIC_ADDR + 4 * die_ids[i]);

		switch (val) {
		case EDGE_BOOTROM_MAGIC:
			state = BOOT_STATE_BOOTROM;
			break;
		case EDGE_PCS_MAGIC:
			state = BOOT_STATE_PCS;
			break;
		case EDGE_BL2_MAGIC:
			state = BOOT_STATE_BL2;
			break;
		case EDGE_UBOOT_MAGIC:
			state = BOOT_STATE_UBOOT;
			break;
		case EDGE_LINUX_MAGIC:
			state = BOOT_STATE_LINUX;
			break;
		default:
			state = BOOT_STATE_NOT_READY;
			break;
		}
		group_state.die_state[i].chiptype_id = die_ids[i];
		group_state.die_state[i].state = state;
	}
	rc = copy_to_user((void __user *)arg, &group_state,
			  sizeof(group_state));
	if (rc) {
		pr_err("Failed to copy group boot state to user space 0x%lx\n",
		       arg);
		return -EFAULT;
	}

	return 0;
}

static int ioctl_do_get_pcie_info(struct edge_dev *edev, unsigned long arg)
{
	struct pci_dev *pdev = edev->pdev;
	struct pcie_info pinfo = { 0 };
	u16 linkstat;
	int rc = 0;

	rc = copy_from_user(&pinfo, (struct pcie_info __user *)arg,
			    sizeof(struct pcie_info));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	if (pinfo.group_id == 0) {
		pinfo.vendor_id = pdev->vendor;
		pinfo.device_id = pdev->device;
		pinfo.subvendor_id = pdev->subsystem_vendor;
		pinfo.subdevice_id = pdev->subsystem_device;
		pinfo.device_self.domain = pci_domain_nr(pdev->bus);
		pinfo.device_self.bus = pdev->bus->number;
		pinfo.device_self.devfn = pdev->devfn;
		pinfo.device_parent.domain = pci_domain_nr(pdev->bus->parent);
		pinfo.device_parent.bus = pdev->bus->parent->number;
		pinfo.device_parent.devfn = pdev->bus->self->devfn;
		switch (edev->pub.card_type) {
		case CARD_TYPE_IPU_X2000_PCIE:
			if (pdev->bus->parent->parent) {
				pinfo.card_self.domain = pci_domain_nr(
					pdev->bus->parent->parent);
				pinfo.card_self.bus =
					pdev->bus->parent->parent->number;
				pinfo.card_self.devfn =
					pdev->bus->parent->self->devfn;
				if (pdev->bus->parent->parent->parent) {
					pinfo.card_parent.domain = pci_domain_nr(
						pdev->bus->parent->parent
							->parent);
					pinfo.card_parent.bus =
						pdev->bus->parent->parent
							->parent->number;
					pinfo.card_parent.devfn =
						pdev->bus->parent->parent->self
							->devfn;
				}
			}
			break;
		case CARD_TYPE_IPU_X6000_PCIE:
			if (pdev->bus->parent->parent) {
				pinfo.card_self.domain =
					pci_domain_nr(pdev->bus->parent->parent
							      ->parent->parent);
				pinfo.card_self.bus =
					pdev->bus->parent->parent->parent
						->parent->number;
				pinfo.card_self.devfn =
					pdev->bus->parent->parent->parent->self
						->devfn;
				if (pdev->bus->parent->parent->parent->parent
					    ->parent) {
					pinfo.card_parent.domain = pci_domain_nr(
						pdev->bus->parent->parent
							->parent->parent
							->parent);
					pinfo.card_parent.bus =
						pdev->bus->parent->parent
							->parent->parent->parent
							->number;
					pinfo.card_parent.devfn =
						pdev->bus->parent->parent
							->parent->parent->self
							->devfn;
				}
			}
			break;
		default:
			pinfo.card_self.domain = pci_domain_nr(pdev->bus);
			pinfo.card_self.bus = pdev->bus->number;
			pinfo.card_self.devfn = pdev->devfn;
			pinfo.card_parent.domain =
				pci_domain_nr(pdev->bus->parent);
			pinfo.card_parent.bus = pdev->bus->parent->number;
			pinfo.card_parent.devfn = pdev->bus->self->devfn;
			break;
		}

		rc = pcie_capability_read_word(pdev, PCI_EXP_LNKSTA, &linkstat);
		if (rc)
			return -EINVAL;

		if (linkstat == 0xFFFF)
			linkstat = 0;
		pinfo.speed = pcie_link_speed[linkstat & PCI_EXP_LNKSTA_CLS];
		pinfo.width = (linkstat & PCI_EXP_LNKSTA_NLW) >>
			      PCI_EXP_LNKSTA_NLW_SHIFT;
	} else if (pinfo.group_id == 1) {
		pinfo.vendor_id = pdev->vendor;
		pinfo.device_id = pdev->device;
		pinfo.subvendor_id = pdev->subsystem_vendor;
		pinfo.subdevice_id = pdev->subsystem_device;
		pinfo.device_self.domain = 0x0;
		pinfo.device_self.bus = 0x1;
		pinfo.device_self.devfn = 0x0;
		pinfo.device_parent.domain = 0x0;
		pinfo.device_parent.bus = 0x0;
		pinfo.device_parent.devfn = 0x0;
		pinfo.card_self.domain = 0x0;
		pinfo.card_self.bus = 0x1;
		pinfo.card_self.devfn = 0x0;
		pinfo.card_parent.domain = 0x0;
		pinfo.card_parent.bus = 0x0;
		pinfo.card_parent.devfn = 0x0;

		linkstat = (0xFFFF &
			    edge_readl_safe(edev, 1,
					    REG_PCIE_MISC_CTRL_STS2_ADDR));
		if (linkstat == 0xFFFF) {
			pinfo.speed = PCI_SPEED_UNKNOWN;
			pinfo.width = PCIE_LNK_WIDTH_UNKNOWN;
		} else {
			pinfo.speed =
				pcie_link_speed[((linkstat & 0x700) >> 8) + 1];
			pinfo.width = 1 << ((linkstat & 0x70) >> 4);
		}
	}

	rc = copy_to_user((void __user *)arg, &pinfo, sizeof(struct pcie_info));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_do_boot_read_pro(struct edge_dev *edev, unsigned long arg)
{
	struct boot_rw brw;
	int rc = 0, barno;

	rc = copy_from_user(&brw, (struct boot_rw __user *)arg,
			    sizeof(struct boot_rw));
	if (rc < 0) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	barno = brw.group_id ? 1 : 0;
	rc = boot_wait_state(edev, barno, false, brw.timeout);
	if (rc < 0)
		return rc;

	if (edev->bar[barno]) {
		rc = copy_to_user(brw.buf,
				  edev->bar[barno] + D2H_MSG_PRO_BUF_ADDR,
				  brw.count);
		if (rc) {
			pr_err("Failed to copy to user space %p\n", brw.buf);
			return -EFAULT;
		}
	} else {
		pr_err("Failed to copy to user space %p\n", brw.buf);
		return -EINVAL;
	}

	edge_writel_safe(edev, barno, 0, D2H_MSG_READY_ADDR);
	return 0;
}

static int ioctl_do_boot_write_pro(struct edge_dev *edev, unsigned long arg)
{
	struct boot_rw brw;
	int rc = 0, barno;

	rc = copy_from_user(&brw, (struct boot_rw __user *)arg,
			    sizeof(struct boot_rw));
	if (rc < 0) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	barno = brw.group_id ? 1 : 0;
	rc = boot_wait_state(edev, barno, true, brw.timeout);
	if (rc < 0)
		return rc;

	if (edev->bar[barno]) {
		rc = copy_from_user(edev->bar[barno] + H2D_MSG_PRO_BUF_ADDR,
				    brw.buf, brw.count);
		if (rc < 0) {
			pr_err("Failed to copy from user space %p\n", brw.buf);
			return -EFAULT;
		}
	} else {
		pr_err("Failed to copy from user space %p\n", brw.buf);
		return -EINVAL;
	}

	edge_writel_safe(edev, barno, 1, H2D_MSG_READY_ADDR);
	return 0;
}

static int ioctl_do_get_card_type(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;

	rc = copy_to_user((void __user *)arg, &edev->pub.card_type,
			  sizeof(unsigned int));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_do_get_ext_init(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;

	rc = copy_to_user((void __user *)arg, &edev->pub.ext_init,
			  sizeof(unsigned int));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_do_read_bar(struct edge_dev *edev, unsigned long arg)
{
	struct bar_rw brw;
	int rc = 0;

	rc = copy_from_user(&brw, (struct bar_rw __user *)arg,
			    sizeof(struct bar_rw));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	brw.val = edge_readl_safe(edev, brw.barno, brw.addr);
	rc = copy_to_user((void __user *)arg, &brw, sizeof(struct bar_rw));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int ioctl_do_write_bar(struct edge_dev *edev, unsigned long arg)
{
	struct bar_rw brw;
	int rc = 0;

	rc = copy_from_user(&brw, (struct bar_rw __user *)arg,
			    sizeof(struct bar_rw));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	// 0x80758: sriov_int_set register(0x5a200758)
	// 0x10000: bit 16(FLR_ACTIVE) in sriov_int_set register
	if ((brw.addr == 0x80758) && (brw.val == 0x10000))
		edev->flr_brw = brw;
	else
		edge_writel_safe(edev, brw.barno, brw.val, brw.addr);

	return 0;
}

static int ioctl_do_read_ext_addr(struct edge_dev *edev, unsigned long arg)
{
	struct bar_rw brw;
	int rc = 0;
	u32 val;
	u32 addr_lo_base;
	u32 addr_lo_offset;
	u32 addr_hi;

	rc = copy_from_user(&brw, (struct bar_rw __user *)arg,
			    sizeof(struct bar_rw));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	addr_lo_base = (brw.addr & 0xffffffff) & (~0xfff); // aligned to 4K
	addr_lo_offset = (brw.addr & 0xffffffff) & 0xfff;
	addr_hi = (brw.addr >> 32) & 0xffffffff;

	spin_lock(&edev->lock);

	edge_writel_safe(edev, brw.barno, addr_lo_base,
			 REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	if (val != addr_lo_base) {
		pr_err("Failed to remap apt2 lo in %s\n", __func__);
		rc = -1;
		goto recovery_lo;
	}

	edge_writel_safe(edev, brw.barno, addr_hi,
			 REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	if (val != addr_hi) {
		pr_err("Failed to remap apt2 hi in %s\n", __func__);
		rc = -1;
		goto recovery_hi;
	}

	brw.val = edge_readl_safe(edev, brw.barno,
				  HSIO_S4_HSIO_SYSTEM_CONTROL_BASE +
					  addr_lo_offset);

recovery_hi:
	edge_writel_safe(edev, brw.barno, 0x0,
			 REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	if (val != 0x0) {
		pr_err("Failed to recovery apt2 hi in %s\n", __func__);
		rc = -1;
	}

recovery_lo:
	edge_writel_safe(edev, brw.barno,
			 HSIO_S4_HSIO_SYSTEM_CONTROL_APB_REG_BASE,
			 REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	if (val != HSIO_S4_HSIO_SYSTEM_CONTROL_APB_REG_BASE) {
		pr_err("Failed to recovery apt2 lo in %s\n", __func__);
		rc = -1;
	}

	spin_unlock(&edev->lock);

	if (rc == 0) {
		rc = copy_to_user((void __user *)arg, &brw,
				  sizeof(struct bar_rw));
		if (rc) {
			pr_err("Failed to copy to user space 0x%lx\n", arg);
			rc = -EINVAL;
		}
	}

	return rc;
}

static int ioctl_do_write_ext_addr(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;
	u32 val;
	u32 addr_lo_base;
	u32 addr_lo_offset;
	u32 addr_hi;
	struct bar_rw brw;

	rc = copy_from_user(&brw, (struct bar_rw __user *)arg,
			    sizeof(struct bar_rw));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	addr_lo_base = (brw.addr & 0xffffffff) & (~0xfff); // aligned to 4K
	addr_lo_offset = (brw.addr & 0xffffffff) & 0xfff;
	addr_hi = (brw.addr >> 32) & 0xffffffff;

	spin_lock(&edev->lock);
	edge_writel_safe(edev, brw.barno, addr_lo_base,
			 REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	if (val != addr_lo_base) {
		pr_err("Failed to remap apt2 lo in %s\n", __func__);
		rc = -1;
		goto recovery_lo;
	}

	edge_writel_safe(edev, brw.barno, addr_hi,
			 REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	if (val != addr_hi) {
		pr_err("Failed to remap apt2 hi in %s\n", __func__);
		rc = -1;
		goto recovery_hi;
	}

	edge_writel_safe(edev, brw.barno, brw.val,
			 HSIO_S4_HSIO_SYSTEM_CONTROL_BASE + addr_lo_offset);

recovery_hi:
	edge_writel_safe(edev, brw.barno, 0x0,
			 REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_HI_ADDR(5));
	if (val != 0x0) {
		pr_err("Failed to recovery apt2 hi in %s\n", __func__);
		rc = -1;
	}

recovery_lo:
	edge_writel_safe(edev, brw.barno,
			 HSIO_S4_HSIO_SYSTEM_CONTROL_APB_REG_BASE,
			 REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	val = edge_readl_safe(edev, brw.barno,
			      REG_PCIE_APT_REMAP_BASE_LO_ADDR(5));
	if (val != HSIO_S4_HSIO_SYSTEM_CONTROL_APB_REG_BASE) {
		pr_err("Failed to recovery apt2 lo in %s\n", __func__);
		rc = -1;
	}

	spin_unlock(&edev->lock);

	return rc;
}

static int ioctl_do_alloc_block_mem(struct file_priv *priv, unsigned long arg)
{
	struct block_mem bmem;
	struct block_mem_info *bmeminfo;
	int rc = 0;
	struct edge_dev *edev = priv->edev;

	rc = copy_from_user(&bmem, (struct block_mem __user *)arg,
			    sizeof(struct block_mem));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	if (dma_pool_enable) {
		if (bmem.size > dma_pool_block_size) {
			pr_err("Invalid parameter, bmem.size=0x%x > 0x%x in %s\n",
			       bmem.size, dma_pool_block_size, __func__);
			return -EINVAL;
		}
		bmem.virt = (u64)edge_dma_pool_alloc(edev, &bmem.phys);
		if (!bmem.virt) {
			pr_err("Failed to call edge_dma_pool_alloc\n");
			return -ENOMEM;
		}
	} else {
		bmem.virt = __get_free_pages(GFP_KERNEL, get_order(bmem.size));
		if (!bmem.virt) {
			pr_err("Failed to call __get_free_pages\n");
			return -ENOMEM;
		}
		bmem.phys = virt_to_phys((void *)bmem.virt);
	}

	bmeminfo = kzalloc(sizeof(struct block_mem_info), GFP_KERNEL);
	if (!bmeminfo) {
		pr_err("Failed to alloc bmeminfo space\n");
		if (dma_pool_enable)
			edge_dma_pool_free(edev, (void *)bmem.virt, bmem.phys);
		else
			__free_pages(virt_to_page((void *)bmem.virt),
				     get_order(bmem.size));
		return -ENOMEM;
	}

	bmeminfo->virt = bmem.virt;
	bmeminfo->phys = bmem.phys;
	bmeminfo->size = bmem.size;
	spin_lock(&priv->lock);
	list_add_tail(&bmeminfo->entry, &priv->bmeminfo_list);
	spin_unlock(&priv->lock);

	rc = copy_to_user((void __user *)arg, &bmem, sizeof(struct block_mem));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		spin_lock(&priv->lock);
		list_del(&bmeminfo->entry);
		spin_unlock(&priv->lock);
		kfree(bmeminfo);
		if (dma_pool_enable)
			edge_dma_pool_free(edev, (void *)bmem.virt, bmem.phys);
		else
			__free_pages(virt_to_page((void *)bmem.virt),
				     get_order(bmem.size));
		return -EFAULT;
	}

	return 0;
}

static int ioctl_do_free_block_mem(struct file_priv *priv, unsigned long arg)
{
	struct block_mem_info *bmeminfo, *tmp;
	struct block_mem bmem;
	int rc = 0, found = 0;
	struct edge_dev *edev = priv->edev;

	rc = copy_from_user(&bmem, (struct block_mem __user *)arg,
			    sizeof(struct block_mem));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	/* First verify ownership under lock, then free */
	spin_lock(&priv->lock);
	list_for_each_entry_safe(bmeminfo, tmp, &priv->bmeminfo_list, entry) {
		if (bmeminfo->virt == bmem.virt) {
			found = 1;
			list_del(&bmeminfo->entry);
			kfree(bmeminfo);
			break;
		}
	}
	spin_unlock(&priv->lock);

	if (found) {
		if (dma_pool_enable)
			edge_dma_pool_free(edev, (void *)bmem.virt, bmem.phys);
		else
			__free_pages(virt_to_page((void *)bmem.virt),
				     get_order(bmem.size));
	} else {
		pr_warn("block mem node found or authorization failed!\n");
		return -ENOENT;
	}

	return 0;
}

static int ioctl_do_copy_to_block_mem(struct edge_dev *edev, unsigned long arg)
{
	struct copy_info cinfo;
	int rc = 0;

	rc = copy_from_user(&cinfo, (struct block_mem __user *)arg,
			    sizeof(struct copy_info));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	rc = copy_from_user((void *)cinfo.block_mem_virt,
			    (void __user *)cinfo.user_virt, cinfo.size);
	if (rc) {
		pr_err("Failed to copy from user space 0x%llx\n",
		       cinfo.user_virt);
		return -EINVAL;
	}

	return rc;
}

static int ioctl_do_copy_from_block_mem(struct edge_dev *edev,
					unsigned long arg)
{
	struct copy_info cinfo;
	int rc = 0;

	rc = copy_from_user(&cinfo, (struct block_mem __user *)arg,
			    sizeof(struct copy_info));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	rc = copy_to_user((void __user *)cinfo.user_virt,
			  (void *)cinfo.block_mem_virt, cinfo.size);
	if (rc) {
		pr_err("Failed to copy to user space 0x%llx\n",
		       cinfo.user_virt);
		return -EINVAL;
	}

	return rc;
}

static int ioctl_do_bw_monitor_start(struct edge_dev *edev, unsigned long arg)
{
	int ch;

	for (ch = 0; ch < EDGE_DMA_CH_NUM; ch++) {
		edev->bwinfo[ch].tx_bytes =
			edev->uengine[ch]->ustats.tx.bytes_stat;
		edev->bwinfo[ch].rx_bytes =
			edev->uengine[ch]->ustats.rx.bytes_stat;
		edev->bwinfo[ch].timestamp = ktime_get_ns();
	}

	return 0;
}

static int ioctl_do_bw_monitor_stop(struct edge_dev *edev, unsigned long arg)
{
	struct bw_info bwinfo = { 0 };
	int ch;
	int rc = 0;

	for (ch = 0; ch < EDGE_DMA_CH_NUM; ch++) {
		edev->bwinfo[ch].tx_bytes =
			edev->uengine[ch]->ustats.tx.bytes_stat -
			edev->bwinfo[ch].tx_bytes;
		edev->bwinfo[ch].rx_bytes =
			edev->uengine[ch]->ustats.rx.bytes_stat -
			edev->bwinfo[ch].rx_bytes;
		edev->bwinfo[ch].timestamp =
			ktime_get_ns() - edev->bwinfo[ch].timestamp;
		bwinfo.tx_bytes[ch] = edev->bwinfo[ch].tx_bytes;
		bwinfo.rx_bytes[ch] = edev->bwinfo[ch].rx_bytes;
		bwinfo.timestamp[ch] = edev->bwinfo[ch].timestamp;
	}

	rc = copy_to_user((void __user *)arg, &bwinfo, sizeof(struct bw_info));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return rc;
}

static int ioctl_do_get_instance_num(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;

	rc = copy_to_user((void __user *)arg, &edev->instance_opened,
			  sizeof(unsigned int));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}
	return 0;
}

static int ioctl_do_link_info(struct file_priv *priv, struct edge_dev *edev,
			      unsigned long arg)
{
	int rc;
	u32 group_id;
	u32 val;

	rc = copy_from_user(&group_id, (u32 *)arg, sizeof(u32));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	group_id = !!group_id;

	if (edge_readl_safe(edev, group_id, BOOT_STATE_MAGIC_ADDR) ==
	    EDGE_LINUX_MAGIC) {
		val = edge_readl_safe(edev, group_id,
				      REG_PCIE_MSI1_INT_SET_ADDR);
		val |= 0x40000000;
		edge_writel_safe(edev, group_id, val,
				 REG_PCIE_MSI1_INT_SET_ADDR);
		return 0;
	} else
		return -ENOTSUPP;
}

static int ioctl_do_temperature(struct edge_dev *edev, unsigned long arg)
{
	u32 val;
	int barno;

	for (barno = 0; barno < 2; barno++) {
		val = edge_readl_safe(edev, barno, REG_PCIE_MSI1_INT_SET_ADDR);
		val |= 0x20000000;
		edge_writel_safe(edev, barno, val, REG_PCIE_MSI1_INT_SET_ADDR);
	}

	return 0;
}

static int ioctl_do_inspur_topo(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;
	u32 vid;
	u8 broadcom1_mp = 0;
	u8 broadcom1_dp = 0;
	u8 broadcom2_mp = 0;
	u8 broadcom2_dp = 0;
	u8 broadcom_two_level = 0;
	u32 broadcom_topo;
	struct pci_dev *pdev;

#define BROADCOM_PEX890xx_VID 0xc0301000

	pdev = edev->pdev;
	if (pdev->bus->parent->parent->parent->parent->parent->parent->parent
		    ->parent) {
		pci_read_config_dword(
			pdev->bus->parent->parent->parent->parent->parent
				->parent->parent->parent->self,
			PCI_VENDOR_ID, &vid);
		if (vid == BROADCOM_PEX890xx_VID) {
			broadcom_two_level = 1;
			broadcom1_dp = pdev->bus->parent->parent->parent->parent
					       ->parent->parent->parent->parent
					       ->self->devfn >>
				       3;
		}
	}

	if ((broadcom_two_level == 1) &&
	    pdev->bus->parent->parent->parent->parent->parent->parent->parent
		    ->parent->parent->parent) {
		pci_read_config_dword(
			pdev->bus->parent->parent->parent->parent->parent
				->parent->parent->parent->parent->parent->self,
			PCI_VENDOR_ID, &vid);
		if (vid == BROADCOM_PEX890xx_VID) {
			broadcom1_mp = pdev->bus->parent->parent->parent->parent
					       ->parent->parent->parent->parent
					       ->parent->parent->self->devfn >>
				       3;
		}
	}

	if (pdev->bus->parent->parent->parent->parent->parent->parent) {
		if (broadcom_two_level == 1)
			broadcom2_mp = pdev->bus->parent->parent->parent->parent
					       ->parent->parent->self->devfn >>
				       3;
		else
			broadcom1_mp = pdev->bus->parent->parent->parent->parent
					       ->parent->parent->self->devfn >>
				       3;
	}

	if (pdev->bus->parent->parent->parent->parent) {
		if (broadcom_two_level == 1)
			broadcom2_dp = pdev->bus->parent->parent->parent->parent
					       ->self->devfn >>
				       3;
		else
			broadcom1_dp = pdev->bus->parent->parent->parent->parent
					       ->self->devfn >>
				       3;
	}

	broadcom_topo = broadcom1_mp << 24 | broadcom1_dp << 16 |
			broadcom2_mp << 8 | broadcom2_dp;

	rc = copy_to_user((void __user *)arg, &broadcom_topo, sizeof(u32));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int edge_led_mode(struct file_priv *priv, struct edge_dev *edev, u8 msg)
{
	u32 val;

	val = edge_readl_safe(edev, 0, PCIE_LED_OFFSET);
	priv->led_msg_prev = val;

	edge_writel_safe(edev, 0, msg, PCIE_LED_OFFSET);

	wmb();

	val = edge_readl_safe(edev, 0, REG_PCIE_MSI1_INT_SET_ADDR);
	val |= 0x80000000;
	edge_writel_safe(edev, 0, val, REG_PCIE_MSI1_INT_SET_ADDR);

	return 0;
}

static int ioctl_do_led(struct file_priv *priv, struct edge_dev *edev,
			unsigned long arg)
{
	int rc = 0;
	u8 msg;

	rc = copy_from_user(&msg, (u8 *)arg, sizeof(u8));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	priv->led_magic = LED_MAGIC;

	edge_led_mode(priv, edev, msg);

	return 0;
}

static int ioctl_do_get_dma_pool_info(struct edge_dev *edev, unsigned long arg)
{
	int rc = 0;
	struct dma_pool_info dma_info;

	dma_info.dma_pool_enable = dma_pool_enable;
	dma_info.dma_pool_block_size = dma_pool_block_size;
	dma_info.dma_pool_block_num = dma_pool_block_num;

	rc = copy_to_user((void __user *)arg, &dma_info, sizeof(struct dma_pool_info));
	if (rc) {
		pr_err("Failed to copy to user space 0x%lx\n", arg);
		return -EINVAL;
	}

	return 0;
}

static int edge_udma_poll_state(struct udma_engine *uengine)
{
	struct udma_xfer *transfer = NULL;
	struct udma_regs *udma_base =
		uengine->edev->bar[0] + uengine->edev->dma_reg_offset;
	u32 channel = uengine->channel;
	unsigned long val;

	uengine->poll_cnt = 0;
	while (1) {
		val = edge_readl(&udma_base->comm.common_udma_int);
		if (val) {
			edge_writel(val & ((1 << channel) |
					   (1 << (channel + 8))),
				    &udma_base->comm.common_udma_int);
			/* done interrupt */
			if (val & (1 << channel)) {
				break;
			}

			/* error interrupt */
			if (val & (1 << (channel + 8))) {
				pr_err("Poll channel %d DMA error!\n", channel);
				break;
			}
		}

		mdelay(1);
		if (uengine->poll_cnt++ >= DMA_POLL_STATE_TIMEOUT_NS) {
			pr_err("Poll channel %d DMA timeout!\n", channel);
			return -1;
		}
	}

	/* Process all transfer */
	transfer = engine_service_transfer(uengine);
	/* Restart the engine following the servicing */
	engine_service_resume(uengine);

	return 0;
}

static int ioctl_do_boot_udma_h2d_xfer(struct edge_dev *edev, unsigned long arg)
{
	struct xfer_task dma_task;
	int rc = 0;
	u64 k_host_virt, k_host_phys;
	u32 barno;

	rc = copy_from_user(&dma_task, (struct xfer_task __user *)arg,
			    sizeof(struct xfer_task));
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		return -EFAULT;
	}

	k_host_virt =
		__get_free_pages(GFP_KERNEL, get_order(dma_task.xfer_size));
	if (!dma_task.host_virt) {
		pr_err("Failed to call __get_free_pages\n");
		return -ENOMEM;
	}
	k_host_phys = virt_to_phys((void *)dma_task.host_virt);
	rc = copy_from_user((void *)k_host_virt, (void *)dma_task.host_virt,
			    dma_task.xfer_size);
	if (rc) {
		pr_err("Failed to copy from user space 0x%lx\n", arg);
		rc = -EFAULT;
		goto out;
	}
	dma_task.host_virt = k_host_virt;
	dma_task.host_phys = k_host_phys;

	barno = dma_task.group_id ? 1 : 0;
	rc = boot_wait_state(edev, barno, true, 1000 * 60 * 5);
	if (rc < 0)
		goto out;

	rc = udma_transfer(edev, &dma_task, false, true);
	if (rc > 0)
		edge_writel_safe(edev, barno, 1, H2D_MSG_READY_ADDR);

out:
	free_pages(dma_task.host_virt, get_order(dma_task.xfer_size));
	return (rc < 0) ? rc : 0;
}

static irqreturn_t edge_udma_isr(int irq, void *dev_id)
{
	struct edge_dev *edev = (struct edge_dev *)dev_id;
	struct udma_regs *udma_base = edev->bar[0] + edev->dma_reg_offset;
	struct udma_engine *uengine;
	unsigned long val;
	int ch;
	u32 mask;

	if (edev->pub.magic == X6000_DS_MAGIC) {
		mask = EDGE_DMA_CH_MASK | (EDGE_DMA_CH_MASK << 8);
		/* for host p2p dma test */
		if (edge_readl_safe(edev, 0, REG_PCIE_SCRATCH_REG7_ADDR) ==
		    0x5A)
			mask |= EDGE_P2P_CH_MASK | (EDGE_P2P_CH_MASK << 8);
	} else {
		mask = 0xFFFF;
	}

#if DMA_INT_BRIDGE
	/* disable all DMA interrupts */
	edge_writel(0xFFFF, &udma_base->comm.common_udma_int_dis);

	val = edge_readl(&udma_base->comm.common_udma_int);
	if (val) {
		pr_debug("dma int status = 0x%08lx.\n", val);
		/* done interrupt */
		for_each_set_bit (ch, &val, EDGE_DMA_CH_NUM) {
			uengine = edev->uengine[ch];
			/* Schedule the bottom half */
			schedule_work(&uengine->work);
		}
		/* error interrupt */
		ch = 8;
		for_each_set_bit_from (ch, &val, EDGE_DMA_CH_NUM) {
			uengine = edev->uengine[ch - 8];
			/* Schedule the bottom half */
			schedule_work(&uengine->work);
		}
		edge_writel(val, &udma_base->comm.common_udma_int);
	}

	/* enable all DMA interrupts */
	edge_writel(0xFFFF, &udma_base->comm.common_udma_int_ena);
#else
	val = mask & edge_readl(&udma_base->comm.common_udma_int);
	if (val) {
		pr_debug("dma int status = 0x%08lx.\n", val);
		edge_writel(val & mask, &udma_base->comm.common_udma_int);
		/* done interrupt */
		for_each_set_bit (ch, &val, EDGE_DMA_CH_NUM) {
			uengine = edev->uengine[ch];
			/* Schedule the bottom half */
			schedule_work(&uengine->work);
		}
		/* error interrupt */
		ch = 8;
		for_each_set_bit_from (ch, &val, EDGE_DMA_CH_NUM + 8) {
			uengine = edev->uengine[ch - 8];
			pr_err("Channel %d DMA error!\n", ch - 8);
			/* Schedule the bottom half */
			schedule_work(&uengine->work);
		}
	}
	/* enable all DMA interrupts */
	edge_writel(0xFFFF, &udma_base->comm.common_udma_int_ena);
#endif

	return IRQ_HANDLED;
}

#if DMA_INT_BRIDGE
static void edge_setup_dma_int_bridge(struct edge_dev *edev, u64 msg_addr,
				      u32 msg_data)
{
	u64 pci_addr_mask = 0xFF;
	u64 pci_addr = msg_addr & ~pci_addr_mask;
	u64 cpu_addr = MSI_WRITE_AXI_ADDR;
	int nbits = ilog2(pci_addr_mask + 1);
	u32 addr0, addr1, desc0, desc1;
	struct atu_regs *atu_base = edev->bar[0] + REG_AXI_CFG_ADDR;
	struct msigen_regs *msigen_base = edev->bar[0] + REG_PCIE_MSIGEN_ADDR;

	if (nbits < 8)
		nbits = 8;

	/* 1. setup the first oubbound region for dma interrupt bridge */
	/* Set the PCI address */
	addr0 = ((nbits - 1) & GENMASK(5, 0)) |
		(lower_32_bits(pci_addr) & GENMASK(31, 8));
	addr1 = upper_32_bits(pci_addr);

	edge_writel(addr0, &atu_base->ob_atu[0].addr0);
	edge_writel(addr1, &atu_base->ob_atu[0].addr1);

	/*
	 * Whatever Bit [23] is set or not inside DESC0 register of the outbound
	 * PCIe descriptor, the PCI function number must be set into
	 * Bits [26:24] of DESC0 anyway.
	 *
	 * In Root Complex mode, the function number is always 0 but in Endpoint
	 * mode, the PCIe controller may support more than one function. This
	 * function number needs to be set properly into the outbound PCIe
	 * descriptor.
	 *
	 * Besides, setting Bit [23] is mandatory when in Root Complex mode:
	 * then the driver must provide the bus, resp. device, number in
	 * Bits [7:0] of DESC1, resp. Bits[31:27] of DESC0. Like the function
	 * number, the device number is always 0 in Root Complex mode.
	 *
	 * However when in Endpoint mode, we can clear Bit [23] of DESC0, hence
	 * the PCIe controller will use the captured values for the bus and
	 * device numbers.
	 */
	desc0 = AT_OB_REGION_DESC0_TYPE_MEM |
		((edev->pdev->devfn << 24) & GENMASK(31, 24));
	desc1 = 0;
	edge_writel(desc0, &atu_base->ob_atu[0].desc0);
	edge_writel(desc1, &atu_base->ob_atu[0].desc1);

	/* Set the CPU address */
	addr0 = ((nbits - 1) & GENMASK(5, 0)) |
		(lower_32_bits(cpu_addr -
			       PCIE_CONTROLLER_X8_SLAVE_SPACE_BASE_PA) &
		 GENMASK(31, 8));
	addr1 = upper_32_bits(cpu_addr -
			      PCIE_CONTROLLER_X8_SLAVE_SPACE_BASE_PA);

	edge_writel(addr0, &atu_base->ob_atu[0].axi_addr0);
	edge_writel(addr1, &atu_base->ob_atu[0].axi_addr1);

	/* 2. setup dma interrupt bridge */
	edge_writel(msg_data, &msigen_base->msigen_atu_data);
	edge_writel(lower_32_bits(cpu_addr) + (msg_addr & pci_addr_mask),
		    &msigen_base->msigen_atu_addr_lo);
	edge_writel(upper_32_bits(cpu_addr), &msigen_base->msigen_atu_addr_hi);
	edge_writel(0x2, &msigen_base->msigen_ctrl);
}
#endif

static int edge_setup_msix_irqs(struct edge_dev *edev)
{
	int i, rc = 0;

#if DMA_INT_BRIDGE
	u64 msg_addr;
	u32 msg_data;
#else
	struct msigen_regs *msigen_base = edev->bar[0] + REG_PCIE_MSIGEN_ADDR;
#endif

	if (!edev->msix_enabled) {
		pr_info("MSI-X disabled! Setup MSI-X irqs failed.\n");
		return 0;
	}

	rc = request_irq(edev->entry[0].vector, edge_udma_isr, IRQF_SHARED,
			 EDGE_DRV_NAME, edev);
	if (rc)
		pr_err("Couldn't use IRQ #%d, rc = %d\n", edev->entry[0].vector,
		       rc);

	get_cached_msi_msg(edev->entry[0].vector, &edev->msi_msg[0]);

#if DMA_INT_BRIDGE
	msg_addr = edev->msi_msg[0].address_lo |
		   ((u64)edev->msi_msg[0].address_hi << 32);
	msg_data = edev->msi_msg[0].data;
	edge_setup_dma_int_bridge(edev, msg_addr, msg_data);
#else
	edge_writel(0x1, &msigen_base->msigen_ctrl);
#endif

	// TODO: request for other irqs.
	/* If any errors occur, free IRQs that were successfully requested */
	if (rc) {
		for (i = 0; i < 1; i++) {
			if (edev->entry[i].vector)
				free_irq(edev->entry[i].vector, edev);
		}
	} else {
		pr_info("IRQ #%d requested for edev 0x%px.\n",
			edev->entry[0].vector, edev);
	}

	return rc;
}

static int edge_setup_irqs(struct edge_dev *edev)
{
	struct pci_dev *pdev = edev->pdev;
	int rc = 0;
#if DMA_INT_BRIDGE
	u64 msg_addr;
	u32 msg_addr_lo, msg_addr_hi;
	u32 msg_data;
#else
	struct msigen_regs *msigen_base = edev->bar[0] + REG_PCIE_MSIGEN_ADDR;
#endif

	if (edev->msix_enabled) {
		rc = edge_setup_msix_irqs(edev);
	} else {
		rc = request_irq(pdev->irq, edge_udma_isr,
				 edev->msi_enabled ? 0 : IRQF_SHARED,
				 EDGE_DRV_NAME, edev);
		if (rc)
			pr_err("Couldn't use IRQ #%d, rc = %d\n", pdev->irq,
			       rc);
		else
			pr_info("IRQ #%d requested for edev 0x%px.\n",
				pdev->irq, edev);

		if (edev->msi_enabled) {
#if DMA_INT_BRIDGE
			pci_read_config_dword(edev->pdev,
					      edev->pdev->msi_cap +
						      PCI_MSI_ADDRESS_LO,
					      &msg_addr_lo);
			pci_read_config_dword(edev->pdev,
					      edev->pdev->msi_cap +
						      PCI_MSI_ADDRESS_HI,
					      &msg_addr_hi);
			msg_addr = msg_addr_lo | ((u64)msg_addr_hi << 32);
			pci_read_config_dword(edev->pdev,
					      edev->pdev->msi_cap +
						      PCI_MSI_DATA_64,
					      &msg_data);
			edge_setup_dma_int_bridge(edev, msg_addr, msg_data);
#else
			edge_writel(0x1, &msigen_base->msigen_ctrl);
#endif
		}
	}
	return rc;
}

static void edge_teardown_irqs(struct edge_dev *edev)
{
	struct pci_dev *pdev = edev->pdev;
	int i;
	u8 group_id;
	u8 chan_id;

	if (edev->msix_enabled) {
		if (edev->entry[0].vector) {
			free_irq(edev->entry[0].vector, edev);
			pr_info("MSIX IRQ #%d freed for edev 0x%px.\n",
				edev->entry[0].vector, edev);
		}

		for (i = 1; i < EDGE_IRQ_MAX_NUM; i++) {
			group_id = (i < (NOTIFY_IRQ_MAX_NUM + 1)) ? 0 : 1;
			chan_id = (i < (NOTIFY_IRQ_MAX_NUM + 1)) ?
					  (i - 1) :
					  (i - NOTIFY_IRQ_MAX_NUM - 1);
			if (edev->notify_irq_chan[group_id][chan_id].status) {
				free_irq(edev->entry[i].vector, edev);
				pr_info("MSIX IRQ #%d freed for edev 0x%px.\n",
					edev->entry[i].vector, edev);
			}
		}
	} else if (edev->msi_enabled) {
		free_irq(pdev->irq, edev);
		pr_info("MSI IRQ #%d freed for edev 0x%px.\n", pdev->irq, edev);

		for (i = 1; i < EDGE_IRQ_MAX_NUM; i++) {
			group_id = (i < (NOTIFY_IRQ_MAX_NUM + 1)) ? 0 : 1;
			chan_id = (i < (NOTIFY_IRQ_MAX_NUM + 1)) ?
					  (i - 1) :
					  (i - NOTIFY_IRQ_MAX_NUM - 1);
			if (edev->notify_irq_chan[group_id][chan_id].status) {
				free_irq(pdev->irq + i, edev);
				pr_info("MSI IRQ #%d freed for edev 0x%px.\n",
					pdev->irq + i, edev);
			}
		}
	} else if (pdev->irq) {
		free_irq(pdev->irq, edev);
		pr_info("Legacy IRQ #%d freed for edev 0x%px.\n", pdev->irq,
			edev);
	}
}

static int edge_enable_irq_vectors(struct edge_dev *edev)
{
	struct pci_dev *pdev = edev->pdev;
	int rc = 0;
	int i;
	int req_nvec = EDGE_IRQ_MAX_NUM;

	if (pci_find_capability(pdev, PCI_CAP_ID_MSIX) &&
	    (pci_msix_vec_count(pdev) >= EDGE_IRQ_MAX_NUM)) {
		pr_debug("Enabling MSI-X, request %d vectors.\n", req_nvec);
		for (i = 0; i < req_nvec; i++)
			edev->entry[i].entry = i;

		rc = pci_enable_msix_range(pdev, edev->entry, 1, req_nvec);
		if (rc != req_nvec) {
			pr_err("Request %d MSI-X vectors failed, rc = %d.\n",
			       req_nvec, rc);
			if (rc > 0)
				pci_disable_msix(pdev);

			edev->msix_enabled = 0;
		} else {
			edev->msix_enabled = 1;
		}
	} else if (pci_find_capability(pdev, PCI_CAP_ID_MSI) &&
		   (pci_msi_vec_count(pdev) >= EDGE_IRQ_MAX_NUM)) {
		pr_debug("Enabling MSI, request %d vector.\n", req_nvec);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0)
		rc = pci_enable_msi_range(pdev, 1, EDGE_IRQ_MAX_NUM);
#else
		rc = pci_alloc_irq_vectors_affinity(pdev, 1, EDGE_IRQ_MAX_NUM,
						    PCI_IRQ_MSI, NULL);
#endif
		if (rc < 0) {
			pr_err("Request MSI vector failed.\n");
			edev->msi_enabled = 0;
		} else {
			edev->msi_enabled = 1;
		}
	} else {
		pr_info("MSI/MSI-X not detected, using lagacy interrupts.\n");
	}
	return rc;
}

static int edge_open(struct inode *inode, struct file *file)
{
	struct file_priv *priv;
	struct edge_dev *edev;
	int rc = 0;
	u32 val;

	priv = kzalloc(sizeof(struct file_priv), GFP_KERNEL);
	if (!priv) {
		pr_err("No spaces for file private data.\n");
		return -ENOMEM;
	}

	edev = container_of(inode->i_cdev, struct edge_dev, cdev);
	priv->edev = edev;
	spin_lock_init(&priv->lock);
	INIT_LIST_HEAD(&priv->bmeminfo_list);
	file->private_data = priv;

	if (!edev->bar[0]) {
		pr_err("Device is not available.\n");
		return -ECANCELED;
	}

	val = edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR);
	mutex_lock(&edev_mutex);
	if ((!edev->irq_requested) && (val == EDGE_LINUX_MAGIC)) {
		rc = edge_enable_irq_vectors(edev);
		if (rc < 0)
			goto err;
		rc = edge_setup_irqs(edev);
		if (rc)
			goto err;

		edev->irq_requested = true;
	}

	edev->instance_opened++;
	pr_debug("edev 0x%px instance opened: %d in %s.\n", edev,
		 edev->instance_opened, __FUNCTION__);
	rc = 0;
err:
	mutex_unlock(&edev_mutex);
	return rc;
}

static int edge_release(struct inode *inode, struct file *file)
{
	struct file_priv *priv = (struct file_priv *)file->private_data;
	struct edge_dev *edev = priv->edev;
	struct block_mem_info *bmeminfo;

	spin_lock(&priv->lock);
	while (!list_empty(&priv->bmeminfo_list)) {
		bmeminfo = list_entry(priv->bmeminfo_list.next,
				      struct block_mem_info, entry);
		if (dma_pool_enable)
			edge_dma_pool_free(edev, (void *)bmeminfo->virt,
					   bmeminfo->phys);
		else
			__free_pages(virt_to_page((void *)bmeminfo->virt),
				     get_order(bmeminfo->size));
		list_del(&bmeminfo->entry);
		kfree(bmeminfo);
	}
	spin_unlock(&priv->lock);

	if (priv == edev->exception_owner) {
		edev->exception_owner = 0;
	}

	if (priv->led_magic == LED_MAGIC) {
		edge_led_mode(priv, edev,
			      ((priv->led_msg_prev >= LED_HEART) &&
			       (priv->led_msg_prev <= LED_TEST)) ?
				      priv->led_msg_prev :
				      LED_HEART);
	}
	kfree(priv);

	mutex_lock(&edev_mutex);
	edev->instance_opened--;
	mutex_unlock(&edev_mutex);

	pr_debug("edev 0x%px instance opened: %d in %s.\n", edev,
		 edev->instance_opened, __FUNCTION__);

	return 0;
}

static ssize_t edge_read(struct file *file, char __user *buf, size_t count,
			 loff_t *pos)
{
	return 0;
}

static ssize_t edge_write(struct file *file, const char __user *buf,
			  size_t count, loff_t *pos)
{
	return 0;
}

static loff_t edge_llseek(struct file *file, loff_t off, int whence)
{
	loff_t newpos = 0;

	switch (whence) {
	case 0: /* SEEK_SET */
		newpos = off;
		break;
	case 1: /* SEEK_CUR */
		newpos = file->f_pos + off;
		break;
	case 2: /* SEEK_END */
		pr_err("%s: SEEK_END is not support\n", __func__);
		return -EINVAL;
	default: /* can't happen */
		return -EINVAL;
	}

	if (newpos < 0)
		return -EINVAL;
	file->f_pos = newpos;
	pr_debug("%s: pos=0x%llx\n", __func__, (signed long long)newpos);

	return newpos;
}

static long edge_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct file_priv *priv = (struct file_priv *)file->private_data;
	struct edge_dev *edev = priv->edev;
	int rc = 0;
	int minor;

	if (!edev)
		return -EINVAL;

	minor = MINOR(edev->cdevno);
	if (!edge_test_bit(minor, edev_states)) {
		wait_event_interruptible(g_states_wq,
					 edge_test_bit(minor, edev_states));
		return -ECANCELED;
	}

	switch (cmd) {
	case EDGE_IOCTL_UDMA_D2H_XFER:
		rc = ioctl_do_udma_d2h_xfer(edev, arg);
		break;
	case EDGE_IOCTL_UDMA_H2D_XFER:
		rc = ioctl_do_udma_h2d_xfer(edev, arg);
		break;
	case EDGE_IOCTL_UDMA_P2POB_XFER:
		rc = ioctl_do_udma_p2pob_xfer(edev, arg);
		break;
	case EDGE_IOCTL_UDMA_P2PIB_XFER:
		rc = ioctl_do_udma_p2pib_xfer(edev, arg);
		break;
	case EDGE_IOCTL_MMIO_D2H_XFER:
		rc = ioctl_do_mmio_d2h_xfer(edev, arg);
		break;
	case EDGE_IOCTL_MMIO_H2D_XFER:
		rc = ioctl_do_mmio_h2d_xfer(edev, arg);
		break;
	case EDGE_IOCTL_BOOT_READ:
		rc = ioctl_do_boot_read(edev, arg);
		break;
	case EDGE_IOCTL_BOOT_WRITE:
		rc = ioctl_do_boot_write(edev, arg);
		break;
	case EDGE_IOCTL_SET_DEVICE_ID:
		rc = ioctl_do_set_device_id(edev, arg);
		break;
	case EDGE_IOCTL_GET_CH_STS:
		rc = ioctl_do_get_channel_status(edev, arg);
		break;
	case EDGE_IOCTL_GET_EP_FLAGS:
		rc = ioctl_do_get_ep_flags(edev, arg);
		break;
	case EDGE_IOCTL_GET_BOOT_STATE:
		rc = ioctl_do_get_boot_state(edev, arg);
		break;
	case EDGE_IOCTL_GET_PCIE_INFO:
		rc = ioctl_do_get_pcie_info(edev, arg);
		break;
	case EDGE_IOCTL_BOOT_READ_PRO:
		rc = ioctl_do_boot_read_pro(edev, arg);
		break;
	case EDGE_IOCTL_BOOT_WRITE_PRO:
		rc = ioctl_do_boot_write_pro(edev, arg);
		break;
	case EDGE_IOCTL_GET_CARD_TYPE:
		rc = ioctl_do_get_card_type(edev, arg);
		break;
	case EDGE_IOCTL_GET_EXT_INIT:
		rc = ioctl_do_get_ext_init(edev, arg);
		break;
	case EDGE_IOCTL_READ_BAR:
		rc = ioctl_do_read_bar(edev, arg);
		break;
	case EDGE_IOCTL_WRITE_BAR:
		rc = ioctl_do_write_bar(edev, arg);
		break;
	case EDGE_IOCTL_ALLOC_BLOCK_MEM:
		rc = ioctl_do_alloc_block_mem(priv, arg);
		break;
	case EDGE_IOCTL_FREE_BLOCK_MEM:
		rc = ioctl_do_free_block_mem(priv, arg);
		break;
	case EDGE_IOCTL_COPY_TO_BLOCK_MEM:
		rc = ioctl_do_copy_to_block_mem(edev, arg);
		break;
	case EDGE_IOCTL_COPY_FROM_BLOCK_MEM:
		rc = ioctl_do_copy_from_block_mem(edev, arg);
		break;
	case EDGE_IOCTL_BW_MONITOR_START:
		rc = ioctl_do_bw_monitor_start(edev, arg);
		break;
	case EDGE_IOCTL_BW_MONITOR_STOP:
		rc = ioctl_do_bw_monitor_stop(edev, arg);
		break;
	case EDGE_IOCTL_GET_INSTANCE_NUM:
		rc = ioctl_do_get_instance_num(edev, arg);
		break;
	case EDGE_IOCTL_WAIT_EXCEPTION:
		rc = ioctl_do_wait_exception(edev, arg);
		break;
	case EDGE_IOCTL_WAIT_EXCEPTION_WAKEUP:
		rc = ioctl_do_wait_exception_wakeup(edev, arg);
		break;
	case EDGE_IOCTL_EXCEPTION_OWNER:
		rc = ioctl_do_exception_owner(priv, edev, arg);
		break;
	case EDGE_IOCTL_CONFIG_EXCEPTION:
		rc = ioctl_do_config_exception(priv, edev, arg);
		break;
	case EDGE_IOCTL_LED:
		rc = ioctl_do_led(priv, edev, arg);
		break;
	case EDGE_IOCTL_LINK_INFO:
		rc = ioctl_do_link_info(priv, edev, arg);
		break;
	case EDGE_IOCTL_BOOT_UDMA_H2D_XFER:
		rc = ioctl_do_boot_udma_h2d_xfer(edev, arg);
		break;
	case EDGE_IOCTL_TEMPERATURE:
		rc = ioctl_do_temperature(edev, arg);
		break;
	case EDGE_IOCTL_INSPUR_TOPO:
		rc = ioctl_do_inspur_topo(edev, arg);
		break;
	case EDGE_IOCTL_READ_EXT_ADDR:
		rc = ioctl_do_read_ext_addr(edev, arg);
		break;
	case EDGE_IOCTL_WRITE_EXT_ADDR:
		rc = ioctl_do_write_ext_addr(edev, arg);
		break;
	case EDGE_IOCTL_GET_DMA_POOL_ONFO:
		rc = ioctl_do_get_dma_pool_info(edev, arg);
		break;		
	default:
		pr_err("Unsupported operation\n");
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int edge_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct file_priv *priv = (struct file_priv *)file->private_data;
	struct edge_dev *edev = priv->edev;
	unsigned long off;
	unsigned long vsize = vma->vm_end - vma->vm_start;
	int rc = 0;

	if (!edev) {
		pr_err("Invalid edev in %s\n", __func__);
		return -EINVAL;
	}
	off = vma->vm_pgoff << PAGE_SHIFT;
	if (off < 0x8000000) {
		unsigned long phys;
		unsigned long psize;
		int barno;

		if (edev->pub.magic == X6000_DS_MAGIC) {
			/* off from 0 to 0x1FFFFFF */
			off = vma->vm_pgoff << PAGE_SHIFT;
			barno = 2;
		} else {
			if (off >= 0x4000000) {
				off -= 0x4000000;
				barno = 4;
			} else {
				barno = 2;
			}
		}

		/* BAR physical address */
		phys = pci_resource_start(edev->pdev, barno) + off;

		/* complete resource */
		psize = pci_resource_len(edev->pdev, barno) - off;

		if (vsize > psize)
			return -EINVAL;

		/*
		 * pages must not be cached as this would result in cache line
		 * sized accesses to the end point
		 */
		//vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		//vma->vm_page_prot = pgprot_cached(vma->vm_page_prot);

		/*
		 * prevent touching the pages (byte access) for swap-in,
		 * and prevent the pages from being swapped out
		 */
		//vma->vm_flags |= VMEM_FLAGS;
		/* avoid to swap out this VMA */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
		vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
#else
		vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
#endif

		rc = remap_pfn_range(vma, vma->vm_start, phys >> PAGE_SHIFT,
				     vsize, vma->vm_page_prot);
		pr_debug(
			"vma 0x%px, vma->vm_start: 0x%lx, phys: 0x%lx, size: %lu\n",
			vma, vma->vm_start, phys >> PAGE_SHIFT, vsize);
	} else {
		/* mmap dma pool */
		struct block_mem_info *bmeminfo, *tmp;
		int found = 0;
		spin_lock(&priv->lock);
		list_for_each_entry_safe (bmeminfo, tmp, &priv->bmeminfo_list,
					  entry) {
			if ((bmeminfo->virt >> PAGE_SHIFT) == vma->vm_pgoff) {
				found = 1;
				break;
			}
		}

		if (found) {
			if (vsize > bmeminfo->size) {
				spin_unlock(&priv->lock);
				return -EINVAL;
			}
			if (dma_pool_enable) {
				/* dma_mmap_coherent() requires vm_pgoff as 0 */
				vma->vm_pgoff = 0;
				rc = dma_mmap_coherent(&edev->pdev->dev, vma,
						       (void *)bmeminfo->virt,
						       bmeminfo->phys, vsize);
			} else {
				vma->vm_page_prot =
					pgprot_noncached(vma->vm_page_prot);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 3, 0)
				vma->vm_flags |= (VM_DONTEXPAND | VM_DONTDUMP);
#else
				vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);
#endif
				rc = remap_pfn_range(vma, vma->vm_start,
						     bmeminfo->phys >>
							     PAGE_SHIFT,
						     vsize, vma->vm_page_prot);
			}
		} else {
			pr_warn("block mem node missing!\n");
			rc = EINVAL;
		}
		spin_unlock(&priv->lock);
	}

	return rc;
}

static const struct file_operations edge_fops = {
	.owner = THIS_MODULE,
	.open = edge_open,
	.release = edge_release,
	.read = edge_read,
	.write = edge_write,
	.llseek = edge_llseek,
	.unlocked_ioctl = edge_ioctl,
	.mmap = edge_mmap,
};

static struct udma_engine *edge_create_engine(struct edge_dev *edev,
					      int channel)
{
	struct udma_engine *uengine;

	uengine = kzalloc(sizeof(struct udma_engine), GFP_KERNEL);
	if (!uengine)
		return NULL;

	uengine->edev = edev;
	uengine->channel = channel;
	uengine->head = 0;
	uengine->tail = 0;
	uengine->desc_bus_base =
		UDMA_DESC_BASE_PA +
		uengine->channel * UDMA_DESC_MAX_NUM * sizeof(struct udma_desc);
	uengine->desc_virt_base =
		(struct udma_desc *)(edev->bar[0] +
				     uengine->channel * UDMA_DESC_MAX_NUM *
					     sizeof(struct udma_desc));
	memset(&uengine->ustats, 0, sizeof(struct udma_stats));

	/* initialize spinlock */
	spin_lock_init(&uengine->lock);

	mutex_init(&uengine->desc_lock);
	/* initialize transfer_list */
	INIT_LIST_HEAD(&uengine->transfer_list);

	/* initialize the deferred work for transfer completion */
	INIT_WORK(&uengine->work, engine_service_work);

	edev->engine_num++;
	return uengine;
}

static void edge_destroy_udma_engines(struct edge_dev *edev)
{
	int i;

	for (i = 0; i < EDGE_DMA_CH_NUM; i++) {
		if (edev->uengine[i]) {
			kfree(edev->uengine[i]);
		}
	}
}

static int edge_create_udma_engines(struct edge_dev *edev)
{
	int i;

	edev->dma_reg_offset = REG_UDMA_ADDR;
	edev->region_index = GROUPB_DMA_REGION_INDEX_BASE;
	edev->region_size = GROUPB_DMA_REGION_SIZE;
	edev->region_offset = GROUPB_DMA_REGION_OFFSET_BASE;
	edev->ob_bar0_addr = A8_OUTBOUND_BAR0_ADDR_BASE;

	for (i = 0; i < EDGE_DMA_CH_NUM; i++)
		edev->uengine[i] = edge_create_engine(edev, i);

	if (edev->engine_num != EDGE_DMA_CH_NUM)
		goto fail;

	return 0;
fail:
	edge_destroy_udma_engines(edev);
	return -1;
}

static void edge_set_public_data(struct edge_dev *edev)
{
	struct pci_dev *pdev = edev->pdev;
	int i;

	edev->pub.device_id = -1;
	edev->pub.idx = MINOR(edev->cdevno);

	if (edev->bar[0]) {
		edev->pub.unique_id_lo =
			edge_readl_safe(edev, 0,
					EFUSE_BASE + EFUSE_CONFIG_B64_ADDR) |
			((u64)edge_readl_safe(
				 edev, 0, EFUSE_BASE + EFUSE_CONFIG_B65_ADDR)
			 << 32);
		edev->pub.unique_id_hi = edge_readl_safe(
			edev, 0, EFUSE_BASE + EFUSE_CONFIG_B66_ADDR);
		edev->pub.ext_init =
			((edge_readl_safe(edev, 0, EXT_INIT_BACKUP_ADDR) &
			  EFUSE_PCIE_EXT_INIT) == 0) ?
				0 :
				1;
		edev->pub.card_type = edge_readl_safe(
			edev, 0, EFUSE_BASE + EFUSE_CONFIG_B92_ADDR);
		if (edev->pub.card_type == 0xFFFFFFFF)
			edev->pub.card_type = edge_readl_safe(
				edev, 0, EFUSE_BASE + EFUSE_CONFIG_B94_ADDR);
		edev->pub.magic = 0;
	} else {
		pr_warn("Unrecognized device!\n");
	}

	edev->pub.key = (pci_domain_nr(pdev->bus) << 16) |
			(PCI_DEVID(pdev->bus->number, pdev->devfn));
	for (i = 0; i < EDGE_GRP_MAX_NUM; i++) {
		edev->pub.shm_phys[i] =
			(phys_addr_t)pdev->resource[i].start + 0x800000;
		edev->pub.sram_phys[i] = (phys_addr_t)pdev->resource[i].start;
	}
}

static void edge_destroy_cdev(struct edge_dev *edev)
{
	if (edev->sys_device)
		device_destroy(g_edge_class, edev->cdevno);

	cdev_del(&edev->cdev);
	mutex_lock(&edev_mutex);
	list_del(&edev->device_entry);
	mutex_unlock(&edev_mutex);
	clear_bit(MINOR(edev->cdevno), minors);
	unregister_chrdev_region(edev->cdevno, 1);
	pr_debug("destroy chrdev for edev 0x%px, %u:%u.\n", edev,
		 MAJOR(edev->cdevno), MINOR(edev->cdevno));
}

static int edge_create_cdev(struct edge_dev *edev)
{
	int rc;
	unsigned int minor;
	dev_t dev;

	spin_lock_init(&edev->lock);
	if (!g_edge_major) {
		/* allocate a dynamically allocated char device node */
		rc = alloc_chrdev_region(&dev, EDGE_MINOR_BASE, 1,
					 EDGE_CLASS_NAME);
		if (rc) {
			pr_err("Allocate chrdev region fail.\n");
			return rc;
		}

		g_edge_major = MAJOR(dev);
		minor = MINOR(dev);
		edev->cdevno = MKDEV(g_edge_major, minor);
		set_bit(minor, minors);
	} else {
		/* find an available minor */
		minor = find_first_zero_bit(minors, EDGE_MINOR_COUNT);
		if (minor < EDGE_MINOR_COUNT) {
			edev->cdevno = MKDEV(g_edge_major, minor);
			rc = register_chrdev_region(edev->cdevno, 1,
						    EDGE_CLASS_NAME);
			if (rc) {
				pr_err("Register chrdev region fail.\n");
				return rc;
			}
			set_bit(minor, minors);
		} else {
			pr_err("no minor number available!\n");
			return -ENODEV;
		}
	}

	/*
	 * do not register yet, create kobjects and name them,
	 */
	edev->cdev.owner = THIS_MODULE;

	INIT_LIST_HEAD(&edev->device_entry);

	cdev_init(&edev->cdev, &edge_fops);
	mutex_lock(&edev_mutex);
	list_add(&edev->device_entry, &edev_list);
	mutex_unlock(&edev_mutex);

	rc = kobject_set_name(&edev->cdev.kobj, "edge%d", minor);
	if (rc < 0)
		goto clr_bitmap;

	/* bring character device live */
	rc = cdev_add(&edev->cdev, edev->cdevno, 1);
	if (rc < 0) {
		pr_err("cdev_add failed, rc = %d.\n", rc);
		goto clr_bitmap;
	}

	pr_debug("create chrdev for edev 0x%px, %u:%u, %s.\n", edev,
		 g_edge_major, minor, edev->cdev.kobj.name);

	edge_set_public_data(edev);

	/* create device on our class */
	if (g_edge_class) {
		edev->sys_device =
			device_create(g_edge_class, &edev->pdev->dev,
				      edev->cdevno, NULL, "%08x_%08x%016llx",
				      edev->pub.key, edev->pub.unique_id_hi,
				      edev->pub.unique_id_lo);
		if (IS_ERR(edev->sys_device)) {
			pr_err("Create char device failed.\n");
			goto del_cdev;
		}
		dev_set_drvdata(edev->sys_device, edev);
	}

	return 0;
del_cdev:
	cdev_del(&edev->cdev);
clr_bitmap:
	mutex_lock(&edev_mutex);
	list_del(&edev->device_entry);
	mutex_unlock(&edev_mutex);
	clear_bit(minor, minors);
	unregister_chrdev_region(edev->cdevno, 1);
	return rc;
}

static int edge_map_bars(struct edge_dev *edev)
{
	struct pci_dev *pdev = edev->pdev;
	u32 bar_no;

	for (bar_no = 0; bar_no < EDGE_BAR_NUM; bar_no++) {
		if (!pci_resource_len(pdev, bar_no))
			continue;

		if (bar_no == 2 || bar_no == 4)
			edev->bar[bar_no] = pci_iomap_wc(
				pdev, bar_no, pci_resource_len(pdev, bar_no));
		else
			edev->bar[bar_no] = pci_iomap(
				pdev, bar_no, pci_resource_len(pdev, bar_no));

		if (!edev->bar[bar_no]) {
			pr_err("Map BAR%d failed.", bar_no);
			goto unmap;
		}

		pr_debug("BAR %d 0x%llx mapped to 0x%pxx, size = 0x%llx.\n",
			 bar_no, pci_resource_start(pdev, bar_no),
			 edev->bar[bar_no], pci_resource_len(pdev, bar_no));

		if (pci_resource_flags(pdev, bar_no) & IORESOURCE_MEM_64)
			bar_no++;
	}

	return 0;
unmap:
	while (--bar_no >= 0) {
		if (edev->bar[bar_no]) {
#if (!IS_ENABLED(CONFIG_GENERIC_IOMAP)) &&                                     \
	(LINUX_VERSION_CODE <= KERNEL_VERSION(5, 9, 0))
			iounmap(edev->bar[bar_no]);
#else
			pci_iounmap(pdev, edev->bar[bar_no]);
#endif
			edev->bar[bar_no] = NULL;
		}
	}

	return -1;
}

static void edge_unmap_bars(struct edge_dev *edev)
{
	u32 bar_no;

	for (bar_no = 0; bar_no < EDGE_BAR_NUM; bar_no++) {
		if (edev->bar[bar_no]) {
#if (!IS_ENABLED(CONFIG_GENERIC_IOMAP)) &&                                     \
	(LINUX_VERSION_CODE <= KERNEL_VERSION(5, 9, 0))
			iounmap(edev->bar[bar_no]);
#else
			pci_iounmap(edev->pdev, edev->bar[bar_no]);
#endif
			edev->bar[bar_no] = NULL;
		}
	}
}

static void edge_device_offline(struct edge_dev *edev)
{
	int i;
	struct udma_engine *uengine;

	clear_bit(MINOR(edev->cdevno), edev_states);

	for (i = 0; i < EDGE_DMA_CH_NUM; i++) {
		uengine = edev->uengine[i];
		while (!list_empty(&uengine->transfer_list))
			usleep_range(100, 1000);
	}

	/* wait current bar accessing finished */
	msleep(100);
}

static void edge_device_online(struct edge_dev *edev)
{
	set_bit(MINOR(edev->cdevno), edev_states);
	wake_up_interruptible(&g_states_wq);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
static void edge_event_timer_cb(struct timer_list *t)
{
	struct edge_dev *edev = timer_container_of(edev, t, event_timer);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static void edge_event_timer_cb(struct timer_list *t)
{
	struct edge_dev *edev = from_timer(edev, t, event_timer);
#else
static void edge_event_timer_cb(unsigned long data)
{
	struct edge_dev *edev = (struct edge_dev *)data;
#endif

	if (edev->event_count >= 600) {
		pr_err("EDGE_DEVICE_ON timeout, may be caused by: "
		       "device%d failed to enter linux; "
		       "boot to pcie boot mode; "
		       "terminate callback when removing edge.\n",
		       edev->pub.device_id);
		return;
	}

	if ((edev->pub.magic == 0) &&
	    edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR) >
		    EDGE_BOOTROM_MAGIC) {
		edev->pub.magic =
			edge_readl_safe(edev, 0, X6000_PROD_MAGIC_ADDR);
	}

	if (edge_readl_safe(edev, 0, BOOT_STATE_MAGIC_ADDR) !=
	    EDGE_LINUX_MAGIC) {
		edev->event_count++;
		mod_timer(&edev->event_timer, jiffies + msecs_to_jiffies(500));
	} else {
		edev->event_count = 0;
		schedule_work(&edev->event_timer_work);
	}
}

static void edge_event_timer_work(struct work_struct *work)
{
	struct edge_dev *edev;
	int rc = -1;

	edev = container_of(work, struct edge_dev, event_timer_work);
	BUG_ON(!edev);

	mutex_lock(&edev_mutex);
	if (!edev->irq_requested) {
		rc = edge_enable_irq_vectors(edev);
		if (rc < 0) {
			mutex_unlock(&edev_mutex);
			pr_err("enable irq vectors fail.\n");
			return;
		}
		rc = edge_setup_irqs(edev);
		if (rc) {
			/* Clean up vectors that were enabled */
			if (edev->msix_enabled) {
				pci_disable_msix(edev->pdev);
				edev->msix_enabled = 0;
			} else if (edev->msi_enabled) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0)
				pci_disable_msi(edev->pdev);
#else
				pci_free_irq_vectors(edev->pdev);
#endif
				edev->msi_enabled = 0;
			}
			mutex_unlock(&edev_mutex);
			pr_err("setup irq fail.\n");
			return;
		}
		edev->irq_requested = true;
	}
	mutex_unlock(&edev_mutex);
	pcie_capability_clear_and_set_word(edev->pdev, PCI_EXP_DEVCTL,
					   PCI_EXP_DEVCTL_READRQ,
					   PCI_EXP_DEVCTL_READRQ_1024B);
	pr_info("blocking notifier call chain, device %d online, edev 0x%px...\n",
		edev->pub.device_id, edev);
	blocking_notifier_call_chain(&edge_ipcm_notifier, EDGE_DEVICE_ON,
				     &edev->pub.device_id);
	pr_info("blocking notifer callback return.\n");
}

static int edge_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int rc = 0;
	struct edge_dev *edev;

	BUG_ON(!pdev);

	/* Allocate zeroed device book keeping structure */
	edev = kzalloc(sizeof(struct edge_dev), GFP_KERNEL);
	if (!edev) {
		pr_err("No spaces for edge_dev.\n");
		return -ENOMEM;
	}

	/* Create a device to driver reference */
	pci_set_drvdata(pdev, edev);
	edev->pdev = pdev;
	edev->irq_requested = false;

	/* Enable pci device */
	rc = pci_enable_device(pdev);
	if (rc) {
		pr_err("PCI enable device failed, rc = %d.\n", rc);
		goto free_alloc;
	}

	/* Enable bus master capability */
	pci_set_master(pdev);

	if (dma_pool_enable == 1) {
		rc = edge_dma_pool_create(edev, dma_pool_block_size,
					  dma_pool_block_num);
		if (rc < 0) {
			pr_err("fail to edge_dma_pool_create(size=0x%x nums=%d) in %s\n",
			       dma_pool_block_size, dma_pool_block_num,
			       __func__);
			goto disable_device;
		}
	} else
		edev->dma_pool = NULL;

	rc = pci_request_regions(pdev, EDGE_DRV_NAME);
	if (rc) {
		pr_err("PCI request regions failed, rc = %d.\n", rc);
		edev->regions_in_use = 1;
		goto destroy_dma_pool;
	} else {
		edev->got_regions = 1;
	}

	rc = edge_map_bars(edev);
	if (rc)
		goto release_region;

	dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));

	/* Enable AER */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(6, 5, 0)
	pci_enable_pcie_error_reporting(pdev);
#endif

	rc = edge_create_udma_engines(edev);
	if (rc < 0) {
		pr_err("Create udma engines failed.\n");
		goto unmap_bars;
	}

	if (!g_edge_class) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
		g_edge_class = class_create(THIS_MODULE, EDGE_CLASS_NAME);
#else
		g_edge_class = class_create(EDGE_CLASS_NAME);
#endif
		if (IS_ERR(g_edge_class)) {
			pr_err("Failed to create class.\n");
			goto destroy_uengines;
		}
	}

	rc = edge_create_cdev(edev);
	if (rc < 0) {
		pr_err("Create character device failed.\n");
		goto destroy_class;
	}

	rc = device_create_file(&pdev->dev, &dev_attr_dma_stats);
	if (rc < 0) {
		pr_err("sysfs add edge_stats failed(%d)\n", rc);
		goto destory_cdev;
	}

	rc = edge_exception_event_init(edev, edge_pcie_exception_event_isr,
				       edge_pcie_exception_event_work);
	if (rc < 0) {
		pr_err("failed to edge_exception_event_init(%d)\n", rc);
		goto sysfs_remove;
	}

	edev->event_count = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
	timer_setup(&edev->event_timer, edge_event_timer_cb, 0);
#else
	setup_timer(&edev->event_timer, edge_event_timer_cb,
		    (unsigned long)edev);
#endif
	INIT_WORK(&edev->event_timer_work, edge_event_timer_work);

	edge_device_online(edev);
	pcie_bus_configure_settings(pdev->bus->parent->parent->parent);

	edge_disable_switch_vdn_acs_redir(edev);
	edge_disable_relax_order(edev);
	pr_info("Edge /dev/%08x_%08x%016llx probe success, %u:%u, edev 0x%px.\n",
		edev->pub.key, edev->pub.unique_id_hi, edev->pub.unique_id_lo,
		MAJOR(edev->cdevno), MINOR(edev->cdevno), edev);

	return 0;

sysfs_remove:
	device_remove_file(&pdev->dev, &dev_attr_dma_stats);
destory_cdev:
	edge_destroy_cdev(edev);
destroy_class:
	if (g_edge_class && list_empty(&edev_list)) {
		class_destroy(g_edge_class);
		g_edge_class = NULL;
	}
destroy_uengines:
	edge_destroy_udma_engines(edev);
unmap_bars:
	edge_unmap_bars(edev);
release_region:
	if (edev->got_regions)
		pci_release_regions(pdev);
destroy_dma_pool:
	if (dma_pool_enable == 1) {
		edge_dma_pool_destory(edev);
	}
disable_device:
	pci_disable_device(pdev);
free_alloc:
	kfree(edev);
	pr_err("Edge probe fail, rc = %d.\n", rc);
	return -1;
}

static void edge_remove(struct pci_dev *pdev)
{
	struct edge_dev *edev;
	u64 unique_id_lo;
	u32 unique_id_hi;
	u32 key;
	u32 major, minor;
	u64 pedev;
	u32 count = 20;

	BUG_ON(!pdev);
	edev = (struct edge_dev *)dev_get_drvdata(&pdev->dev);
	if (!edev)
		return;

	unique_id_lo = edev->pub.unique_id_lo;
	unique_id_hi = edev->pub.unique_id_hi;
	key = edev->pub.key;
	major = MAJOR(edev->cdevno);
	minor = MINOR(edev->cdevno);
	pedev = (u64)edev;

	edge_device_offline(edev);

	edev->event_count = 120;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 17, 0)
	timer_delete_sync(&edev->event_timer);
#else
	del_timer_sync(&edev->event_timer);
#endif
	pr_info("blocking notifier call chain, device %d offline, edev 0x%px...\n",
		edev->pub.device_id, edev);
	blocking_notifier_call_chain(&edge_ipcm_notifier, EDGE_DEVICE_OFF,
				     &edev->pub.device_id);
	pr_info("blocking notifer callback return.\n");
	/* wait all instance closed */
	do {
		if (edev->instance_opened == 0)
			break;
		msleep_interruptible(500);
		pr_info("waiting instance closed...\n");
		if (signal_pending(current)) {
			pr_info("signal pending\n");
			break;
		}
	} while (--count);

	if (count == 0)
		pr_warn("WARNING: %d instances not closed for edev 0x%px!\n",
			edev->instance_opened, edev);

	// 0x80758: sriov_int_set register(0x5a200758)
	// 0x10000: bit 16(FLR_ACTIVE) in sriov_int_set register
	if ((edev->flr_brw.addr == 0x80758) && (edev->flr_brw.val == 0x10000)) {
		edge_writel_safe(edev, edev->flr_brw.barno, edev->flr_brw.val,
				 edev->flr_brw.addr);
		memset(&edev->flr_brw, 0, sizeof(edev->flr_brw));
	}

	if (edev->irq_requested) {
		edge_teardown_irqs(edev);
		if (edev->msix_enabled) {
			pci_disable_msix(edev->pdev);
			edev->msix_enabled = 0;
		} else if (edev->msi_enabled) {
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0)
			pci_disable_msi(edev->pdev);
#else
			pci_free_irq_vectors(edev->pdev);
#endif
			edev->msi_enabled = 0;
		}
		edev->irq_requested = false;
	}

	edge_exception_event_deinit(edev);

	device_remove_file(&pdev->dev, &dev_attr_dma_stats);

	edge_destroy_cdev(edev);

	if (g_edge_class && list_empty(&edev_list)) {
		class_destroy(g_edge_class);
		/* THIS is important for remove/rescan */
		g_edge_class = NULL;
		g_edge_major = 0;
	}

	edge_destroy_udma_engines(edev);

	edge_unmap_bars(edev);

	if (edev->got_regions)
		pci_release_regions(pdev);

	if (dma_pool_enable == 1) {
		edge_dma_pool_destory(edev);
	}

	pci_disable_device(pdev);

	edev->irq_requested = false;
	kfree(edev);
	edev = NULL;

	pr_info("Edge /dev/%08x_%08x%016llx remove success, %u:%u, edev 0x%llx.\n",
		key, unique_id_hi, unique_id_lo, major, minor, pedev);
}

static pci_ers_result_t edge_error_detected(struct pci_dev *pdev,
					    pci_channel_state_t state)
{
	struct edge_dev *edev = dev_get_drvdata(&pdev->dev);

	switch (state) {
	case pci_channel_io_normal:
		return PCI_ERS_RESULT_CAN_RECOVER;
	case pci_channel_io_frozen:
		pr_warn("dev 0x%p,0x%p, frozen state error, reset controller\n",
			pdev, edev);
		edge_device_offline(edev);
		pci_disable_device(pdev);
		return PCI_ERS_RESULT_NEED_RESET;
	case pci_channel_io_perm_failure:
		pr_warn("dev 0x%p,0x%p, failure state error, req. disconnect\n",
			pdev, edev);
		return PCI_ERS_RESULT_DISCONNECT;
	}
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t edge_slot_reset(struct pci_dev *pdev)
{
	struct edge_dev *edev = dev_get_drvdata(&pdev->dev);

	pr_info("0x%p restart after slot reset\n", edev);
	if (pci_enable_device_mem(pdev)) {
		pr_info("0x%p failed to renable after slot reset\n", edev);
		return PCI_ERS_RESULT_DISCONNECT;
	}

	pci_set_master(pdev);
	pci_restore_state(pdev);
	pci_save_state(pdev);
	edge_device_online(edev);

	return PCI_ERS_RESULT_RECOVERED;
}

static void edge_error_resume(struct pci_dev *pdev)
{
	struct edge_dev *edev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, edev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
	pci_aer_clear_nonfatal_status(pdev);
#else
	pci_cleanup_aer_uncorrect_error_status(pdev);
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
static void edge_reset_prepare(struct pci_dev *pdev)
{
	struct edge_dev *edev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, edev);
	edge_device_offline(edev);
}

static void edge_reset_done(struct pci_dev *pdev)
{
	struct edge_dev *edev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p.\n", pdev, edev);
	edge_device_online(edev);
}

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static void edge_reset_notify(struct pci_dev *pdev, bool prepare)
{
	struct edge_dev *edev = dev_get_drvdata(&pdev->dev);

	pr_info("dev 0x%p,0x%p, prepare %d.\n", pdev, edev, prepare);

	if (prepare)
		edge_device_offline(edev);
	else
		edge_device_online(edev);
}
#endif

static const struct pci_error_handlers edge_err_handler = {
	.error_detected = edge_error_detected,
	.slot_reset = edge_slot_reset,
	.resume = edge_error_resume,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
	.reset_prepare = edge_reset_prepare,
	.reset_done = edge_reset_done,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
	.reset_notify = edge_reset_notify,
#endif
};

static const struct dev_pm_ops edge_pm_ops = {
	//.suspend	= NULL,
	//.resume	= NULL,
};

static const struct pci_device_id edge_pci_tbl[] = {
	{
		PCI_DEVICE(0x17cd, 0x0100),
	},
	{
		PCI_DEVICE(0x17cd, 0x2000),
	},
	{ 0 },
};
MODULE_DEVICE_TABLE(pci, edge_pci_tbl);

static struct pci_driver edge_pci_driver = {
	.name		= EDGE_DRV_NAME,
	.id_table	= edge_pci_tbl,
	.probe		= edge_probe,
	.remove		= edge_remove,
	.err_handler	= &edge_err_handler,
	.driver = {
		.pm	= &edge_pm_ops,
	},
};

static void hot_reset_buslock_init(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		mutex_init(&g_edge_hotreset_buslock[i].lock);
		g_edge_hotreset_buslock[i].handle = NULL;
	}
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 17, 0)
static int edge_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", EDGE_VERSION);
	return 0;
}
#else
static int edge_version_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%s\n", EDGE_VERSION);
	return 0;
}

static int edge_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, edge_version_show, NULL);
}

static const struct file_operations edge_proc_fops = {
	.open = edge_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};
#endif

static int edge_proc_register(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 17, 0)
	proc_entry =
		proc_create_single("edge_version", 0, NULL, edge_version_show);
#else
	proc_entry = proc_create("edge_version", 0, NULL, &edge_proc_fops);
#endif
	if (proc_entry == NULL) {
		pr_err("fail to proc_create /proc/edge_version in %s\n",
		       __func__);
		return -1;
	}

	return 0;
}
static void edge_proc_unregister(void)
{
	proc_remove(proc_entry);
}

static int edge_dma_pool_create(struct edge_dev *edev, u32 size, u32 nums)
{
	int i;
	int ret;
	int alloc_nums;
	struct dma_pool_addr *dma_pool_addr;

	dma_pool_addr =
		kzalloc(sizeof(struct dma_pool_addr) * nums, GFP_KERNEL);
	if (!dma_pool_addr) {
		pr_err("fail to kzalloc dma_pool_addr nums=%d in %s\n", nums,
		       __func__);
		return -1;
	}

	ret = dma_coerce_mask_and_coherent(&edev->pdev->dev, DMA_BIT_MASK(64));
	if (ret) {
		kfree(dma_pool_addr);
		pr_err("fail to dma_coerce_mask_and_coherent(ret=%d) in %s\n",
		       ret, __func__);
		return -1;
	}

	edev->dma_pool = dma_pool_create("edge", &edev->pdev->dev, size, 0, 0);
	if (edev->dma_pool == NULL) {
		kfree(dma_pool_addr);
		pr_err("fail to dma_pool_create size=0x%x in %s\n", size,
		       __func__);
		return -1;
	}

	for (i = 0; i < nums; i++) {
		dma_pool_addr[i].vaddr = dma_pool_alloc(
			edev->dma_pool, GFP_KERNEL, &dma_pool_addr[i].handle);
		if (dma_pool_addr[i].vaddr == NULL) {
			pr_err("fail to dma_pool_alloc (i=%d size=0x%x nums=%d) in %s\n",
			       i, dma_pool_block_size, dma_pool_block_num,
			       __func__);
			break;
		}
	}

	alloc_nums = i;
	for (i = 0; i < alloc_nums; i++) {
		dma_pool_free(edev->dma_pool, dma_pool_addr[i].vaddr,
			      dma_pool_addr[i].handle);
	}

	if (alloc_nums < nums) {
		kfree(dma_pool_addr);
		dma_pool_destroy(edev->dma_pool);
		edev->dma_pool = NULL;
		return -1;
	}

	kfree(dma_pool_addr);

	pr_info("Edge dma_pool_block_size=0x%x dma_pool_block_num=%d\n",
		dma_pool_block_size, dma_pool_block_num);

	return 0;
}

static void edge_dma_pool_destory(struct edge_dev *edev)
{
	if (edev->dma_pool) {
		dma_pool_destroy(edev->dma_pool);
		edev->dma_pool = NULL;
	}
}

static void *edge_dma_pool_alloc(struct edge_dev *edev, dma_addr_t *dma)
{
	if (edev->dma_pool)
		return dma_pool_alloc(edev->dma_pool, GFP_KERNEL, dma);
	else
		return NULL;
}

static void edge_dma_pool_free(struct edge_dev *edev, void *vaddr,
			       dma_addr_t dma)
{
	if (edev->dma_pool)
		dma_pool_free(edev->dma_pool, vaddr, dma);
}

#if 0
module_pci_driver(edge_pci_driver);
#else
static int __init edge_pci_driver_init(void)
{
	smbus_misc_register();
	hot_reset_buslock_init();
	edge_proc_register();
	return pci_register_driver(&edge_pci_driver);
}
module_init(edge_pci_driver_init);
static void __exit edge_pci_driver_exit(void)
{
	smbus_misc_deregister();
	edge_proc_unregister();
	pci_unregister_driver(&edge_pci_driver);
}
module_exit(edge_pci_driver_exit);
#endif

MODULE_VERSION(EDGE_VERSION);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Li Shiqiu <li.shiqiu@intellif.com>");
MODULE_DESCRIPTION("edge - loadable module for Edge SoCs");
