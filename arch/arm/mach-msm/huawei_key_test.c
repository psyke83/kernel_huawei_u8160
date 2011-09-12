/* 
 * key auto test
 *
 * Copyright (C) 2009 HUAWEI.
 *
 * Author: Mazhenhua
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/synaptics_i2c_rmi.h>
#include <linux/gpio_event.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#define KEY_TEST_NAME "key_test"
#define KTIME_SECS 0  /* define long_key_timer's seconds */
#define KTIME_NSECS 600000000  /* define long_key_timer's nseconds  500ms*/
#define HUAWEI_KEY_END 62

//#define TEST_DEBUG 

#ifdef TEST_DEBUG
#define KEY_TEST_DEBUG(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#else
#define KEY_TEST_DEBUG(fmt, args...)
#endif

struct key_test_data {
	struct input_dev *ts_input_dev;
	struct input_dev *kp_input_dev;
	struct cdev cdev;
	struct device *pdevice; 
	struct class *kt_class;
	struct semaphore open_sem;
	struct work_struct  work;
	struct hrtimer long_key_timer;
	u16 long_pressed_key_code;
	void *cache;
	int xyp[3];
};

/*prot from gpio_event.c*/
struct gpio_event {
	struct gpio_event_input_devs *input_devs;
	const struct gpio_event_platform_data *info;
	struct early_suspend early_suspend;
	void *state[0];
};

static struct workqueue_struct *ts_wq;
extern struct kset *devices_kset;
struct key_test_data *key_test_dev;

struct usr_key
{
	char s_key[10];
	unsigned short key_code;
};

struct usr_key usr_key_map[] ={
	{"back",  KEY_BACK},
	{"home",  KEY_HOME},
	{"menu",  KEY_MENU},
	{"power", KEY_POWER},
	{"left",  KEY_LEFT},
	{"right", KEY_RIGHT},
	{"up",    KEY_UP},
	{"down",  KEY_DOWN},
	{"send",  KEY_SEND},
	{"end",   62},
	{"vup",   KEY_VOLUMEUP},
	{"vdown", KEY_VOLUMEDOWN},
	{"camera",KEY_CAMERA},
	{"ok",    232},
};

/*these keys are report from touch driver or  are hs key, so append them to normal key map*/
static const unsigned short additional_keys[]={
	KEY_BACK,
	KEY_HOME,
	KEY_MENU,
	KEY_POWER,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_UP,
	KEY_DOWN,
	KEY_SEND,
	HUAWEI_KEY_END,
	
};
static int add_additional_keys(void)
{
	int i;
	int key_cnt = sizeof(additional_keys)/sizeof(unsigned short);

	for(i = 0; i < key_cnt; i++)
	{
		input_set_capability(key_test_dev->kp_input_dev,
						EV_KEY, additional_keys[i]);
	}

	return 0;
}

#if 0
static int init_kp_input_dev(struct gpio_event_matrix_info *mi, struct device	*pdev)
{
	struct gpio_event *ip;
	int err;

	int i;
	int key_count;

	if(mi == NULL || pdev == NULL)
	{
		KEY_TEST_DEBUG("init_kp_input_dev: invalid mi = 0x%x, pdev = 0x%x\n", (unsigned)mi, (unsigned)pdev);
		err = -EINVAL;
		goto err_bad_keymap;
	}
	key_test_dev->kp_input_dev->name = "kp_test_input";
	key_count = mi->ninputs * mi->noutputs;
	KEY_TEST_DEBUG("init_kp_input_dev: key_count = %d\n", key_count);
	
	ip = (struct gpio_event *)pdev->driver_data;
	if(ip == NULL)
	{
		KEY_TEST_DEBUG("init_kp_input_dev: ip invalid, IP = NULL\n");
		err = -EINVAL;
		goto err_bad_keymap;
	}

	/*add keys from keypad driver*/
	for (i = 0; i < key_count; i++) {
		unsigned short keyentry = mi->keymap[i];
		unsigned short keycode = keyentry & MATRIX_KEY_MASK;
		unsigned short dev = keyentry >> MATRIX_CODE_BITS;
		if (dev >= ip->input_devs->count) {
			KEY_TEST_DEBUG("gpiomatrix: bad device index %d >= "
				"%d for key code %d\n",
				dev, ip->input_devs->count, keycode);
			err = -EINVAL;
			goto err_bad_keymap;
		}
		KEY_TEST_DEBUG("init_kp_input_dev: keycode = %d\n", keycode);
		if (keycode && keycode <= KEY_MAX)
			input_set_capability(key_test_dev->kp_input_dev,
						EV_KEY, keycode);
	}

	/*add additional keys*/
	add_additional_keys();
	return 0;
	
err_bad_keymap:
	return err;

}
#endif

static void report_keypad_key(struct key_test_data *dev,const char *command, u8 long_pressed)
{
	char s_key[10] = {0};
	u32 len;
	int i;
	u16 keycode = 0;
	
	/*get keycode string*/
	strcpy(s_key, command);
	len = strlen(s_key);
	KEY_TEST_DEBUG("report_keypad_key: s_key = %s\n", s_key);

	/*scan key code*/
	for(i = 0; i < sizeof(usr_key_map)/sizeof(struct usr_key); i++)
	{
		if(!strcmp(usr_key_map[i].s_key, s_key))
			keycode = usr_key_map[i].key_code;
	}

	/*if the key not mapped, report the key directly*/
	if(i == sizeof(usr_key_map)/sizeof(struct usr_key))
	{
		sscanf(s_key, "%hd", &keycode);
	}
	
	KEY_TEST_DEBUG("report_keypad_key: scan keycode shot! keycode = %d, long_pressed = %d\n", keycode, long_pressed);
	/*report key*/
	if(!long_pressed)
	{
		input_report_key(dev->kp_input_dev, keycode, 1); 
		input_report_key(dev->kp_input_dev, keycode, 0); 
	}
	else
	{	
		dev->long_pressed_key_code = keycode;
		input_report_key(dev->kp_input_dev, keycode, 1); 
		hrtimer_start(&dev->long_key_timer, ktime_set(KTIME_SECS, KTIME_NSECS), HRTIMER_MODE_REL);
	}
}

static void ts_work_fun(struct work_struct *work)
{
	int xyp[3];/*x, y, pressed*/
	u32 len;
	int j = 0;
	struct key_test_data *dev = container_of(work, struct key_test_data, work);
	char *command = dev->cache;
	
	while(*command != '}')
	{
		if(*command == '(')
		{
			int i = 0;
			while(i < 3)
			{
				command ++;
				len = strlen(command);
				sscanf(command, "%d", &xyp[i]);
				command += len;
				i++;
			}
			input_report_abs(dev->ts_input_dev, ABS_X, xyp[0]);
			input_report_abs(dev->ts_input_dev, ABS_Y, xyp[1]);
			input_report_key(dev->ts_input_dev, BTN_TOUCH, xyp[2]);

			input_sync(dev->ts_input_dev);
			mdelay(10);
		}
		command++;
		j++;
	}
}

static void report_keys(struct key_test_data *dev, char *command)
{
	u32 i;
	u32 com_len = strlen(command);
	char com_type[10] = {0};
	u32 len;
	
	for(i = 0; i < com_len; i++)
		if(command[i] == ',' || command[i] == ')')
			command[i] = 0x0;

	strcpy(com_type,command);
	len = strlen(com_type);
	
	KEY_TEST_DEBUG("report_keys: com_type = %s\n", com_type);
	if(!strncmp(com_type, "key", len))
	{
		report_keypad_key(dev ,command + len + 1, 0);
	}
	else if(!strncmp(com_type, "lkey", len))
	{
		report_keypad_key(dev ,command + len + 1, 1);
	}
	else if(!strncmp(com_type, "ts", len))
	{
		queue_work(ts_wq, &dev->work);
	}
	else
	{
		KEY_TEST_DEBUG("report_keys: key type error!\n");
	}
}

static ssize_t key_write(struct file *file, const char __user *buf,
						size_t count, loff_t *ppos)
{
	struct key_test_data *dev = (struct key_test_data *)file->private_data;

	memset(dev->cache, 0, PAGE_SIZE);
	KEY_TEST_DEBUG("key_write: count = %d\n", count); 
	if(count >= PAGE_SIZE)
	{
		return 0;
	}
	
	if(copy_from_user(dev->cache, buf, count-1))
		return -EFAULT;
	
	KEY_TEST_DEBUG("key_write: command = %s\n", (char*)dev->cache); 
	
	report_keys(dev, (char*)dev->cache);
	
	return count;
}

static int key_open(struct inode *inode, struct file *filp)
{
	KEY_TEST_DEBUG("key_open: key test opened!\n");
    if (down_interruptible(&key_test_dev->open_sem))
    {
    	KEY_TEST_DEBUG(KERN_ERR "key_open: can not get open_sem!\n");
        return -ERESTARTSYS; 
    }
	filp->private_data = key_test_dev; 
	return 0;
}

static int key_release(struct inode *pnode, struct file *filp)
{
	struct key_test_data *dev = (struct key_test_data *)filp->private_data;

	KEY_TEST_DEBUG("key_release: key test end!\n");
	up(&dev->open_sem);
	return 0;
}
static const struct file_operations key_fops = {
	.open       = key_open,
	.write		= key_write,
	.release    = key_release,
};

static int init_key_test_cdev(struct key_test_data * dev)
{
	int error;
	dev_t dev_no;
	
	error = alloc_chrdev_region(&dev_no,0, 1, KEY_TEST_NAME);
	if(error)
	{
		KEY_TEST_DEBUG(KERN_ERR "init_key_test_cdev: Failed  alloc_chrdev_region\n");
		return -ENOENT;
	}
		
	/*create cdev*/
	dev->kt_class= class_create(THIS_MODULE, KEY_TEST_NAME); 
    if (IS_ERR(dev->kt_class)) 
    {
        KEY_TEST_DEBUG("failed to class_create\n"); 
		goto can_not_create_class; 
    }
	dev->pdevice = device_create(dev->kt_class, NULL, dev_no, "%s", KEY_TEST_NAME); 

	cdev_init(&(dev->cdev), &key_fops);
	dev->cdev.owner = THIS_MODULE;
	
	error = cdev_add(&dev->cdev, dev_no, 1);
	if (error) {
		KEY_TEST_DEBUG(KERN_ERR "init_key_test_cdev: Failed  cdev_add\n");
		error = ENOENT;
		goto can_not_add_cdev;
	}
	return 0;
can_not_add_cdev:
	class_unregister(dev->kt_class); 
can_not_create_class:
	unregister_chrdev_region(dev_no, 1);
	return error;
}

enum hrtimer_restart long_key_timer_func(struct hrtimer *timer)
{
    struct key_test_data *data = container_of(timer, struct key_test_data, long_key_timer);   

	KEY_TEST_DEBUG("long_key_timer_func:   key code = %d\n",data->long_pressed_key_code);
	input_report_key(data->kp_input_dev, data->long_pressed_key_code, 0);
    return HRTIMER_NORESTART;
}

struct input_dev *kset_find_input_devices(struct kset *kset)
{
	struct kobject *k;
	struct device *pdev;
	struct input_dev *pin_dev;
	struct input_dev *ret = NULL;
	
	spin_lock(&kset->list_lock);
	list_for_each_entry(k, &kset->list, entry) {
			pdev = container_of(k, struct device, kobj);
			//KEY_TEST_DEBUG("kset_find_input_devices: touch dev = %s\n", pdev->bus->name);
			
			KEY_TEST_DEBUG("kset_find_input_devices: obj name = %s\n", pdev->kobj.name);
			
			if(!strncmp(pdev->kobj.name, "input",5))
			{
				pin_dev = container_of(pdev, struct input_dev, dev);
				if((pin_dev->keybit) && (test_bit(BTN_TOUCH, pin_dev->keybit)))
				{
					KEY_TEST_DEBUG("kset_find_input_devices: touch dev = %s\n", pin_dev->name);
					ret = pin_dev;
					break;
				}
			}
		}

	spin_unlock(&kset->list_lock);
	return ret;
}

static void init_kp_devices(struct kset *kset, struct input_dev* in)
{
	struct kobject *k;
	struct device *pdev;
	struct input_dev *pin_dev;
	unsigned long key_bitmask[BITS_TO_LONGS(KEY_CNT)] = {0};
	u32 i;
	u32 bits_to_longs = BITS_TO_LONGS(KEY_CNT);

	__set_bit(EV_KEY, in->evbit);
	spin_lock(&kset->list_lock);
	list_for_each_entry(k, &kset->list, entry) {
			pdev = container_of(k, struct device, kobj);
			if(!strncmp(pdev->kobj.name, "input",5))
			{
				pin_dev = container_of(pdev, struct input_dev, dev);
				KEY_TEST_DEBUG("init_kp_devices: find a input dev, name = %s\n", pin_dev->name);

				#ifdef TEST_DEBUG
					for(i = 0; i < KEY_MAX; i++)
						if(test_bit(i, pin_dev->keybit))
							KEY_TEST_DEBUG("%s:keybit[%d]\n",pin_dev->name, i);
				#endif
				
				if(pin_dev->keybit)
				{
					memset(key_bitmask, 0 , sizeof(unsigned long)*bits_to_longs);
					memcpy(key_bitmask, pin_dev->keybit, sizeof(unsigned long)*bits_to_longs);
					for (i = 0; i < BITS_TO_LONGS(BTN_MISC); i++) 
					{
						if (key_bitmask[i] != 0)
						{
							KEY_TEST_DEBUG("init_kp_devices: find keypad dev, name = %s\n", pin_dev->name);
							in->keybit[i] = (in->keybit[i] | key_bitmask[i]);
						}
					}
					
				}
			}
		}
	spin_unlock(&kset->list_lock);
	add_additional_keys();

#ifdef TEST_DEBUG
	for(i = 0; i < KEY_MAX; i++)
		if(test_bit(i, in->keybit))
			KEY_TEST_DEBUG("init_kp_devices: keybit[%d] mapped\n", i);
#endif
}
static int do_late_probe(void)
{
	int ret = 0;
	struct input_dev* current_ts_dev;
	
	#if 0
	struct kobject *kp_kobj = NULL;
	struct device	*dev;
	struct gpio_event_matrix_info * mi;
	struct gpio_event_info **info;
	#endif
	
	current_ts_dev = kset_find_input_devices(devices_kset);
	if(!current_ts_dev)
	{
		goto prob_fail;
	}
	
	/*touch screen input dev init*/
	key_test_dev->ts_input_dev = input_allocate_device();
	if (key_test_dev->ts_input_dev == NULL) {
		ret = -ENOMEM;
		KEY_TEST_DEBUG(KERN_ERR "huawei_key_test_probe: Failed to allocate ts test input device\n");
		goto prob_fail;
	}
	key_test_dev->ts_input_dev->name = "ts_test_input";
	
	memcpy(key_test_dev->ts_input_dev->evbit, current_ts_dev->evbit, sizeof(current_ts_dev->evbit));
	memcpy(key_test_dev->ts_input_dev->keybit, current_ts_dev->keybit, sizeof(current_ts_dev->keybit));
	memcpy(key_test_dev->ts_input_dev->absmin, current_ts_dev->absmin, sizeof(current_ts_dev->absmin));
	memcpy(key_test_dev->ts_input_dev->absmax, current_ts_dev->absmax, sizeof(current_ts_dev->absmax));
	memcpy(key_test_dev->ts_input_dev->absfuzz, current_ts_dev->absfuzz, sizeof(current_ts_dev->absfuzz));
	memcpy(key_test_dev->ts_input_dev->absflat, current_ts_dev->absflat, sizeof(current_ts_dev->absflat));
	memcpy(key_test_dev->ts_input_dev->absbit, current_ts_dev->absbit, sizeof(current_ts_dev->absbit));
	
	ret = input_register_device(key_test_dev->ts_input_dev);
	if (ret) {
		KEY_TEST_DEBUG(KERN_ERR "huawei_key_test_probe: Unable to register %s input device\n", key_test_dev->ts_input_dev->name);
		goto err_ts_register_device_failed;
	}   

	
	/*keypad input dev init*/
	key_test_dev->kp_input_dev = input_allocate_device();
	if (key_test_dev->kp_input_dev == NULL) {
		ret = -ENOMEM;
		KEY_TEST_DEBUG(KERN_ERR "huawei_key_test_probe: Failed to allocate keypad test input device\n");
		goto err_ts_register_device_failed;
	}

	key_test_dev->kp_input_dev->name = "kp_test_input";

	#if 0
	/*find keypad kobj*/
	kp_kobj = kset_find_obj(devices_kset, GPIO_EVENT_DEV_NAME);
	if(kp_kobj == NULL)
	{
		ret = -ENOENT;
		KEY_TEST_DEBUG(KERN_ERR "huawei_key_test_probe: Failed to find kobj\n");
		goto err_can_not_find_obj;
	}
	KEY_TEST_DEBUG("huawei_key_test_probe: get kp_kobj's name = %s\n", kp_kobj->name);

	/*get dev from kobj*/
	dev = container_of(kp_kobj,struct device, kobj);

	info = ((struct gpio_event_platform_data*)dev->platform_data)->info;
	mi = container_of(*info, struct gpio_event_matrix_info, info);

	/*init kp input dev*/
	ret = init_kp_input_dev(mi, dev);
	if(ret)
	{
		ret = -ENOENT;
		KEY_TEST_DEBUG(KERN_ERR "huawei_key_test_probe: Failed to init kp input dev\n");
		goto err_can_not_find_obj;
	}

	#endif
	init_kp_devices(devices_kset, key_test_dev->kp_input_dev);
	
	/*register keypad*/
	ret = input_register_device(key_test_dev->kp_input_dev);
	if (ret) {
		KEY_TEST_DEBUG(KERN_ERR "huawei_key_test_probe: Unable to register %s input device\n", key_test_dev->ts_input_dev->name);
		goto err_ts_register_device_failed;
	}   

	/*init long key timer*/
	hrtimer_init(&key_test_dev->long_key_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	key_test_dev->long_key_timer.function = long_key_timer_func;

	/*init mutex*/
	init_MUTEX(&key_test_dev->open_sem); 

	/*init work for ts*/
	ts_wq = create_singlethread_workqueue("key_test_wq");
	
	if (!ts_wq) {
		KEY_TEST_DEBUG("create synaptics_wq error\n");
		ret = -ENOMEM;
		goto err_can_not_find_obj;
	}
	INIT_WORK(&key_test_dev->work, ts_work_fun);

	/*init cdev*/
	ret = init_key_test_cdev(key_test_dev);
	if(ret)
	{
		ret = -ENOENT;
		KEY_TEST_DEBUG(KERN_ERR "huawei_key_test_probe: Failed to init kp input dev\n");
		goto err_can_not_find_obj;
	}

	return 0;

err_can_not_find_obj:
	input_free_device(key_test_dev->kp_input_dev);

err_ts_register_device_failed:
	input_free_device(key_test_dev->ts_input_dev);
prob_fail:
	return ret;

}

/*this function must be invoked after gpio_event driver init*/
static int __init init_huawei_key_test_dev(void)
{
	int ret;
	ret = do_late_probe();
	return ret;
}

static int __init huawei_key_test_init(void)
{
	int ret = 0;
	key_test_dev = kzalloc(sizeof(struct key_test_data), GFP_KERNEL);
	
    if (!key_test_dev)
    {
        KEY_TEST_DEBUG("huawei_key_test_init: Unable to alloc memory for device\n"); 
		ret = -ENOMEM;
    }
	
	key_test_dev->cache= kzalloc(PAGE_SIZE , GFP_KERNEL);
	
    if (!key_test_dev->cache)
    {
        KEY_TEST_DEBUG("huawei_key_test_init: Unable to alloc cache memory for device\n"); 
		ret = -ENOMEM;
    }
	return ret;
}

static void __exit huawei_key_test_exit(void)
{
	kfree(key_test_dev->cache);
	kfree(key_test_dev);
	
	if (ts_wq)
		destroy_workqueue(ts_wq);
}

late_initcall(init_huawei_key_test_dev);
module_init(huawei_key_test_init);
module_exit(huawei_key_test_exit);

MODULE_DESCRIPTION("Synaptics Touchscreen Driver");
MODULE_LICENSE("GPL");

