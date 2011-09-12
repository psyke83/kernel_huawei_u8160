/* 
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <asm/io.h>
#include <asm/string.h>
#include <asm/uaccess.h>
#ifdef CONFIG_MSM_RPC_OEM_RAPI
#include <mach/oem_rapi_client.h>
#endif

#define DEBUG_SHARE_MEM

#ifdef DEBUG_SHARE_MEM
#define SHARE_MEM_DEBUG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define SHARE_MEM_DEBUG(fmt, args...)
#endif

#ifdef CONFIG_MSM_RPC_OEM_RAPI
static struct msm_rpc_client *client = NULL;
static struct oem_rapi_client_streaming_func_arg rapi_arg;
static struct oem_rapi_client_streaming_func_ret rapi_ret;
#endif

/* equals to definition in oem_rapi.h */
#define HUAWEI_OEM_RAPI_SAVE_OEM_LOGO  48

/* ioctl command */
enum SHARE_MEM_IOCTL_TYPE{
	SHARE_MEM_IOCTL_MIN = 0x1014,
	SHARE_MEM_DUMP_MEM,
	SHARE_MEM_LOADLOGO_OK,
	SHARE_MEM_IOCTL_MAX,
};

/*
 * this module is used to transfer large data from arm11 to arm 9
 */

struct share_mem_device{
	const char *name;
	unsigned int mem_base;
	unsigned int mem_size;
	void *dump_base;
	atomic_t open_count;
	struct miscdevice *mem_dev;
	int length;
	int current_position;
};

static inline int _lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void _unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

static struct share_mem_device *share_mem_device = NULL;
static char *name = "hw_share_mem";

static int share_mem_open (struct inode *node, struct file *file);
static int share_mem_release (struct inode *node, struct file *file);
static ssize_t share_mem_read(struct file *filp, char __user *buff, size_t count, loff_t *offp);
static ssize_t share_mem_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp);
static int share_mem_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);
//static int share_mem_map (struct file *file, struct vm_area_struct *vm);

static int share_mem_open (struct inode *node, struct file *file)
{
	//unsigned char *mem_base = 0;

	SHARE_MEM_DEBUG("share_mem_open enter.");

	/*
	 * just allow one process uses this module.
	 */
	if(_lock(&share_mem_device->open_count))
	{
		return -EBUSY;
	}

	//mem_base = (unsigned char *)(share_mem_device->mem_base);
	//share_mem_device->dump_base = ioremap((unsigned long)mem_base, share_mem_device->mem_size);
	share_mem_device->dump_base = ioremap(share_mem_device->mem_base, share_mem_device->mem_size);

	if (!share_mem_device->dump_base)
	{
		printk("share_mem_open remap failed\n");
		return -EBUSY;
	}

	share_mem_device->current_position = 0;
	share_mem_device->length = 0;

	memset(share_mem_device->dump_base, 0, share_mem_device->mem_size);

	return 0;
}

static int share_mem_release (struct inode *node, struct file *file)
{
	SHARE_MEM_DEBUG("share_mem_release enter.");

	iounmap(share_mem_device->dump_base);
	share_mem_device->current_position = 0;
	share_mem_device->length = 0;

	_unlock(&share_mem_device->open_count);

	return 0;
}

static ssize_t share_mem_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
	return 0;
}

static ssize_t share_mem_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{
	int real_count = 0;
	//memcpy(share_mem_device->dump_base + share_mem_device->current_position, buff, count);

	real_count = count;
	if(share_mem_device->current_position >= share_mem_device->mem_size)
	{
		return -ENOMEM;
	}

	if(share_mem_device->current_position + real_count > share_mem_device->mem_size)
	{
		real_count = share_mem_device->mem_size - share_mem_device->current_position;
	}

	if(copy_from_user(share_mem_device->dump_base + share_mem_device->current_position, buff, real_count))
	{
		return -EFAULT;
	}

	share_mem_device->current_position += real_count;
	share_mem_device->length += real_count;

	return real_count;
}
int share_mem_ioctl (struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	int i = 0;
#ifdef DEBUG_SHARE_MEM
	unsigned char *ptr = NULL;
#endif
	int result = 0;

	SHARE_MEM_DEBUG("share_mem_ioctl enter.");

	switch(cmd)
	{
	case SHARE_MEM_DUMP_MEM:
#ifdef DEBUG_SHARE_MEM
		ptr = share_mem_device->dump_base;
		printk("\n");
		for(i = 0; i < share_mem_device->length ; i++)
		{
			if(i%16 == 0)
			{
				printk("\n");
			}
			printk("%02x ", *(ptr + i));
		}
#endif
		break;
	case SHARE_MEM_LOADLOGO_OK:
	{
#ifdef CONFIG_MSM_RPC_OEM_RAPI
		client = oem_rapi_client_init();
		if(NULL == client)
		{
			printk("init oem_rapi_client error!\n");
			return -EFAULT;
		}

		memset(&rapi_arg, 0, sizeof(struct oem_rapi_client_streaming_func_arg));
		memset(&rapi_ret, 0, sizeof(struct oem_rapi_client_streaming_func_ret));

		rapi_arg.event = HUAWEI_OEM_RAPI_SAVE_OEM_LOGO;
		rapi_arg.cb_func = 0;
		rapi_arg.handle = 0;
		rapi_arg.in_len = 4;
		rapi_arg.input = (char *)&share_mem_device->length;
		rapi_arg.out_len_valid = 1;
		rapi_arg.output_valid = 1;
		rapi_arg.output_size = 128;
		rapi_ret.out_len = NULL;
		rapi_ret.output = NULL;

		result = oem_rapi_client_streaming_function(client, &rapi_arg, &rapi_ret);

		kfree(rapi_ret.out_len);
		kfree(rapi_ret.output);

		if(result)
		{
			printk("share memory oem_rapi_client function error!\n");
			result = -EINVAL;
		}
		else
		{
			printk("share memory oem_rapi_client function passed!\n");
		}
#else
		printk("unsupported ioctl SHARE_MEM_LOADLOGO_OK!\n");
#endif
		break;
	}
	default:
		break;
	}

	return result;
}

static struct file_operations share_mem_fops = {
	.owner 		= THIS_MODULE,
	.open 		= share_mem_open,
	.release 	= share_mem_release,
	.read		= share_mem_read,
	.write		= share_mem_write,
	.ioctl		= share_mem_ioctl,
};

static struct miscdevice misc_share_mem_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= "share_mem",
	.fops 	= &share_mem_fops,
};

static int share_mem_probe(struct platform_device *pdev)
{
	int ret=0;

	SHARE_MEM_DEBUG("share_mem_probe enter.\n");

	if(!(pdev->num_resources >0)){
		printk("share mem device: no such devices! resources num=%d.\n", pdev->num_resources);
		return -ENXIO;
	}

	share_mem_device = kzalloc(sizeof(struct share_mem_device), GFP_KERNEL); 
	if (!share_mem_device)
	{
		printk("share mem device: Unable to alloc memory for share memory device.\n"); 
		return -ENOMEM;
	}

	share_mem_device->name = name;
	share_mem_device->mem_base = pdev->resource[0].start;
	share_mem_device->mem_size = pdev->resource[0].end - pdev->resource[0].start + 1 ;
	share_mem_device->mem_dev = &misc_share_mem_device;
	share_mem_device->dump_base = NULL;
	share_mem_device->current_position = 0;
	share_mem_device->length = 0;
	atomic_set(&share_mem_device->open_count, 0);

	SHARE_MEM_DEBUG("name = %s\n", share_mem_device->name);
	SHARE_MEM_DEBUG("mem_base = %08x\n", share_mem_device->mem_base);
	SHARE_MEM_DEBUG("mem_size = %08x\n", share_mem_device->mem_size);

	ret = misc_register(share_mem_device->mem_dev);
	if(ret)
	{
		printk("share mem device: misc device register failed: %d\n", ret);
		goto  alloc_device_fail;
	}

	SHARE_MEM_DEBUG("share_mem_probe OK.\n");

	return 0;

alloc_device_fail:
	kfree(share_mem_device);
	return ret;
}

static int share_mem_remove(struct platform_device *pdev)
{
	SHARE_MEM_DEBUG("share_mem_remove enter.");

	misc_deregister(share_mem_device->mem_dev);

	kfree(share_mem_device);

	return 0;
}

static struct platform_driver share_mem_driver = {
	.probe = share_mem_probe,
	.remove = share_mem_remove,
	.driver = { 
		.name = "hw_share_mem"
		}
};

static int __init share_mem_init(void)
{
	SHARE_MEM_DEBUG("share_mem_init enter.");
	return platform_driver_register(&share_mem_driver);
}

static void __exit share_mem_exit(void)
{
	SHARE_MEM_DEBUG("share_mem_exit enter.");
	platform_driver_unregister(&share_mem_driver);
}

module_init(share_mem_init);
module_exit(share_mem_exit);
