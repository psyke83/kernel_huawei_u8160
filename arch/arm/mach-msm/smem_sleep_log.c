/*
 * SMEM Sleep log driver.
 * Allows a user space process to get the SMEM log of sleep.
 *
 * Copyright (c) 2011 HUAWEI <hujun@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the smems of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/fs.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/smp_lock.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>
#include <linux/utsname.h>
#include <asm/processor.h> 
#include <asm/system.h>  


#define NAME			"smem_sleep_log"

MODULE_AUTHOR("hw <hw@huawei.com>");
MODULE_DESCRIPTION("SMEM sleep log Driver");
MODULE_LICENSE("GPL");

static int major;
module_param(major, int, 0);
MODULE_PARM_DESC(major, "Major device number");

//static struct cdev smem_sleep_log_cdev;
//static struct class *smem_sleep_log_class;
struct smem_sleep_log_data {
	struct cdev cdev;
	struct device *pdevice; 
	struct class *kt_class;
    struct semaphore open_sem;
    void *smem_buf_base; 
    size_t buf_offset;
    size_t buf_size;    
};

struct smem_sleep_log_data *smem_sleep_log_dev;
unsigned long smem_buf_size;  
static int smem_sleep_log_debug = 1; 

/* reserve 32 entries even though some aren't usable */
#define SMEM_SLEEP_LOG_COUNT	32

#define SMEM_SLEEP_LOG_DEBUG(fmt, args...) \
    if (smem_sleep_log_debug) \
        printk(KERN_DEBUG fmt, ##args); \
    else { \
    } 

static ssize_t smem_sleep_log_read(struct file *file, char __user *buf,
				size_t len, loff_t *ppos)
{
    struct smem_sleep_log_data *dev = (struct smem_sleep_log_data *)file->private_data;
    size_t copy_len =0;
    SMEM_SLEEP_LOG_DEBUG("%s base = 0x%x, len =%d, offset = %d, buf_size=%d \r\n",__FUNCTION__,dev->smem_buf_base, len,dev->buf_offset,dev->buf_size);
    
    if(dev->buf_offset >= dev->buf_size)
    {
        /*copy is finished */
        return 0; 
    }
    else
    {
       copy_len = ((dev->buf_size - dev->buf_offset) > len) ? len : (dev->buf_size - dev->buf_offset);
    }

    if(NULL != dev->smem_buf_base)
    {   
        /*copy data to user space */
        if(copy_to_user((void __user *)buf, dev->smem_buf_base + dev->buf_offset, copy_len))
        {
            SMEM_SLEEP_LOG_DEBUG("smem_sleep_log_read: Fail to copy memory to user space !\n");
            return -EFAULT;       
        }
        dev->buf_offset += copy_len;
    }
    else
    {
        return -ENOMEM;
    }

	return copy_len;
}

static int smem_sleep_log_open(struct inode *inode, struct file *file)
{
    SMEM_SLEEP_LOG_DEBUG("%s \r\n",__FUNCTION__);
    long buf_len = 0;

    if (down_interruptible(&smem_sleep_log_dev->open_sem))
    {
    	SMEM_SLEEP_LOG_DEBUG(KERN_ERR "smem_open: can not get open_sem!\n");
        return -ERESTARTSYS; 
    }
	file->private_data = smem_sleep_log_dev;
    

    if(NULL == smem_sleep_log_dev->smem_buf_base)
    {
        return -ENOMEM;
    }
    /*get the buffer actul length and actul offset*/
    memcpy(&buf_len,smem_sleep_log_dev->smem_buf_base,sizeof(long));
    smem_sleep_log_dev->buf_size = (buf_len < (smem_buf_size - sizeof(long))) ? buf_len :(smem_buf_size - sizeof(long));
    smem_sleep_log_dev->buf_offset = sizeof(long);
 
	return 0;

}
static int smem_sleep_log_release(struct inode *inode, struct file *file)
{
    SMEM_SLEEP_LOG_DEBUG("%s \r\n",__FUNCTION__);

    up(&smem_sleep_log_dev->open_sem);
	return 0;
}


static const struct file_operations smem_sleep_log_fops = {
	.owner	= THIS_MODULE,
	.read	= smem_sleep_log_read,
	.open	= smem_sleep_log_open,
	.release = smem_sleep_log_release
};

static int smem_sleep_log_prob(struct platform_device *pdev)
{
    dev_t   dev_id;
    int retval;
    int error;
    unsigned char *mem_base = 0;
        
    smem_sleep_log_dev = kzalloc(sizeof(struct smem_sleep_log_data), GFP_KERNEL);   
    if (!smem_sleep_log_dev)
    {
        SMEM_SLEEP_LOG_DEBUG(KERN_ERR "smem_sleep_log_prob: Unable to alloc memory for device\n"); 
        return (-ENOMEM);
    }

    /* map sleep log memory */
    if ((pdev->id == 0) && (pdev->num_resources > 0)) {
      smem_buf_size = pdev->resource[0].end - pdev->resource[0].start + 1;
	  mem_base = (unsigned char *)(pdev->resource[0].start);
	  
	  SMEM_SLEEP_LOG_DEBUG("%s: phys addr = 0x%x, size = 0x%x\n", __FUNCTION__, mem_base, smem_buf_size);
      smem_sleep_log_dev->smem_buf_base = ioremap((unsigned long)mem_base, (unsigned long)smem_buf_size); 
	  SMEM_SLEEP_LOG_DEBUG("%s: mapped virt address = 0x%x\n", __FUNCTION__, smem_sleep_log_dev->smem_buf_base);
      if (!smem_sleep_log_dev->smem_buf_base)
      {
		printk("smem_sleep_log_prob failed\n");
        return -EBUSY;
      }

    }

    /*init mutex*/
    init_MUTEX(&smem_sleep_log_dev->open_sem); 

    if (major) {
        dev_id = MKDEV(major, 0);
        retval = register_chrdev_region(dev_id, SMEM_SLEEP_LOG_COUNT,
                        NAME);
    } else {
        retval = alloc_chrdev_region(&dev_id, 0, SMEM_SLEEP_LOG_COUNT,
                         NAME);
        major = MAJOR(dev_id);
    }

    if (retval) {
        SMEM_SLEEP_LOG_DEBUG(KERN_ERR "smem_sleep_log cant get major\n");
        kfree(smem_sleep_log_dev);
        return -1;
    }
    
    smem_sleep_log_dev->kt_class = class_create(THIS_MODULE, NAME); 
    if (IS_ERR(smem_sleep_log_dev->kt_class)) 
    {
        SMEM_SLEEP_LOG_DEBUG(KERN_ERR "failed to class_create\n"); 
        goto can_not_create_class; 
    }

    smem_sleep_log_dev->pdevice = device_create(smem_sleep_log_dev->kt_class, NULL, dev_id, "%s", NAME); 
    if (IS_ERR(smem_sleep_log_dev->pdevice)) {
        SMEM_SLEEP_LOG_DEBUG(KERN_ERR "Can't create smem log device\n");
        goto can_not_create_class;
    }

    cdev_init(&(smem_sleep_log_dev->cdev), &smem_sleep_log_fops);
    smem_sleep_log_dev->cdev.owner = THIS_MODULE;
    
    error = cdev_add(&(smem_sleep_log_dev->cdev), dev_id, SMEM_SLEEP_LOG_COUNT);
    if (error) {
        SMEM_SLEEP_LOG_DEBUG(KERN_ERR "smem_sleep_log_prob: Failed  cdev_add\n");
        error = ENOENT;
        goto can_not_add_cdev;
    }
  
    return 0;
    can_not_add_cdev:
       class_unregister(smem_sleep_log_dev->kt_class); 
    can_not_create_class:
       unregister_chrdev_region(dev_id, 1);
       iounmap((void *)(smem_sleep_log_dev->smem_buf_base));
       kfree(smem_sleep_log_dev);
       return error;
}

void smem_sleep_log_cleanup(void)
{
	dev_t dev_id = MKDEV(major, 0);
    if(smem_sleep_log_dev)
    {
        iounmap((void *)(smem_sleep_log_dev->smem_buf_base));
        kfree(smem_sleep_log_dev);
        smem_sleep_log_dev = NULL;
    }

	cdev_del(&(smem_sleep_log_dev->cdev));
    class_unregister(smem_sleep_log_dev->kt_class);     
	unregister_chrdev_region(dev_id, SMEM_SLEEP_LOG_COUNT);
}

module_param(smem_sleep_log_debug, int, S_IRWXU); 

static struct platform_driver smem_sleep_log_driver = {
	.probe =  smem_sleep_log_prob,
	.driver = {
		   .name = "smem_sleep_log",
		   },
};

static int __init smem_sleep_log_init(void)
{
    return platform_driver_register(&smem_sleep_log_driver);
}

static void __exit smem_sleep_log_exit(void)
{
  smem_sleep_log_cleanup();
  platform_driver_unregister(&smem_sleep_log_driver);
}


module_init(smem_sleep_log_init);
module_exit(smem_sleep_log_exit);

