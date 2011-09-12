/* drivers/input/touchscreen/synaptics_i2c_rmi_tm1319.c
 *
 * Copyright (C) 2009 HUAWEI.
 **
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/synaptics_i2c_rmi.h>

#include <mach/gpio.h>
#include <mach/vreg.h>

#include <linux/hardware_self_adapt.h>

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

/*
 * DEBUG SWITCH
 *
 */ 

// #define TS_DEBUG 
#undef TS_DEBUG 

#ifdef TS_DEBUG
#define SYNAPITICS_DEBUG(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#else
#define SYNAPITICS_DEBUG(fmt, args...)
#endif
#define TS_X_OFFSET  3*(TS_X_MAX/LCD_X_MAX)
#define TS_Y_OFFSET  TS_X_OFFSET

#define TS_X_MAX 3537
#define TS_Y_MAX 5880
#define LCD_X_MAX 319

static struct workqueue_struct *synaptics_wq;

struct synaptics_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct work_struct  work;
	int use_irq;
	struct hrtimer timer;	
	int (*power)(struct i2c_client* client, int on);
	struct early_suspend early_suspend;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_ts_early_suspend(struct early_suspend *h);
static void synaptics_ts_late_resume(struct early_suspend *h);
#endif

static int synaptics_ts_power(struct i2c_client *client, int on);


static void synaptics_ts_work_func(struct work_struct *work)
{
	int i;
	int ret;
	int bad_data = 0;
	struct i2c_msg msg[2];
	uint8_t start_reg;
	uint8_t buf[8];
	uint16_t x, y;
	uint8_t z,w;
	uint8_t finger;
	uint8_t gesture;
	uint8_t magnitude;
	static uint16_t last_x = 0; 
	static uint16_t last_y = 0;
	static bool is_first_point = true;
	struct synaptics_ts_data *ts = container_of(work, struct synaptics_ts_data, work);

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = 0x00;
	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;
	SYNAPITICS_DEBUG("synaptics_ts_work_func\n"); 
	
	for (i = 0; i < ((ts->use_irq && !bad_data) ? 1 : 3); i++) { 	
	ret = i2c_transfer(ts->client->adapter, msg, 2);
	if (ret < 0) {
			SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_work_func: i2c_transfer failed\n");
			bad_data = 1;
	} else {
				bad_data = 0;	
				x = buf[5] | (uint16_t)(buf[4] & 0x1f) << 8; /* x aixs */ 
				y= buf[3] | (uint16_t)(buf[2] & 0x1f) << 8;  /* y aixs */ 
				z = buf[1];				    /* pressure */	
				w = buf[0] >> 4;			    /* width */ 	
				finger = buf[0] & 7;                        /* numbers of fingers */  
				gesture = buf[6];		            /* code of gesture */ 
				magnitude = buf[7];                         /* enhanced data of gesture */  
	SYNAPITICS_DEBUG("x = %d y = %d z = %d w = %d finger = %d gesture = %x magnitude = %d\n",
					x, y, z, w, finger,gesture,magnitude);
				if (z) {
		/* 
		 * always report the first point  whether slip  or click
		 */ 
					if (is_first_point) {
						
						input_report_abs(ts->input_dev, ABS_X, x);
					input_report_abs(ts->input_dev, ABS_Y, 5880-y);
						last_x = x;
						last_y = y;
						is_first_point = false;
					}
					else {
		/* 
		 * except the first point, also report the following points  
		 * 1) x aixes is 3 larger or smaller than the last reported point 
		 * 2) y aixes is 3 larger or smaller than the last reported point.
		 *
		 */
						if ((( x - last_x) >= TS_X_OFFSET) 
						|| ((last_x - x) >= TS_X_OFFSET) 
						|| ((y - last_y) >= TS_Y_OFFSET) 
						|| ((last_y - y) >= TS_Y_OFFSET)) { 
												
						input_report_abs(ts->input_dev, ABS_X, x);
						input_report_abs(ts->input_dev, ABS_Y, 5880-y);
						last_x = x;
						last_y = y;
						
						}
					}
				}
				else 
		/* 
		 * The next point must be first point whether slip or click after 
		 * this up event
		 */			
					is_first_point = true;
		

				input_report_abs(ts->input_dev, ABS_PRESSURE, z);
				input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, w);
				input_report_key(ts->input_dev, BTN_TOUCH, finger);
				input_sync(ts->input_dev);
		}

	}
	if (ts->use_irq) {
		enable_irq(ts->client->irq);
		SYNAPITICS_DEBUG("enable irq\n");
	}
}
static enum hrtimer_restart synaptics_ts_timer_func(struct hrtimer *timer)
{
	struct synaptics_ts_data *ts = container_of(timer, struct synaptics_ts_data, timer);
	SYNAPITICS_DEBUG("synaptics_ts_timer_func\n");
	queue_work(synaptics_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t synaptics_ts_irq_handler(int irq, void *dev_id)
{
	struct synaptics_ts_data *ts = dev_id;
	disable_irq_nosync(ts->client->irq);
	SYNAPITICS_DEBUG("synaptics_ts_irq_handler,disable irq\n");
	queue_work(synaptics_wq, &ts->work);
	return IRQ_HANDLED;
}

static int synaptics_ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct synaptics_ts_data *ts;
	//delete vreg *v_gp5
	int ret = 0;
	int gpio_config;
	int i;
	struct synaptics_i2c_rmi_platform_data *pdata;
	
	SYNAPITICS_DEBUG(" In synaptics_ts_probe: \n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
    /* delete gp5 power on*/
    if (touch_is_supported())
        return -ENODEV;

/* driver  detect its device  */  
	for(i = 0; i < 3; i++) {
		
		ret = i2c_smbus_read_byte_data(client, 0xe1);
		if (ret == 1){
			SYNAPITICS_DEBUG("synaptics_ts manufacturer id = %d\n", ret); 
			goto succeed_find_device;
		}
	}
	if ( i == 3) {
	
		SYNAPITICS_DEBUG("no synaptics_ts device\n ");	
		goto err_find_touchpanel_failed;
	}

succeed_find_device:
	set_touch_support(true);
	synaptics_wq = create_singlethread_workqueue("synaptics_wq");
	if (!synaptics_wq) {
		SYNAPITICS_DEBUG("create synaptics_wq error\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);
	INIT_WORK(&ts->work, synaptics_ts_work_func);

	pdata = client->dev.platform_data;

	ts->power = synaptics_ts_power;
	if (ts->power) {
		ret = ts->power(ts->client, 1);
		if (ret < 0) {
			SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_probe reset failed\n");
			goto err_power_failed;
		}
		msleep(200);
		ret = i2c_smbus_write_byte_data(client, 0x24, 0x0D);   /* Set Sensitivity Adjust */
	}
	
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "synaptics-rmi-touchscreen";
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	input_set_abs_params(ts->input_dev, ABS_X, 0, TS_X_MAX, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, TS_Y_MAX, 0, 0);

	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_TOOL_WIDTH, 0, 15, 0, 0);
	ret = input_register_device(ts->input_dev);
	if (ret) {
		SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}   

	
	gpio_config = GPIO_CFG(29, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
	ret = gpio_tlmm_config(gpio_config, GPIO_ENABLE);
	SYNAPITICS_DEBUG(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n", __func__, 29, ret);
	if (ret) 
	{
		ret = -EIO;
		goto err_input_register_device_failed;
	}
	if (gpio_request(29, "synaptics_ts_int\n"))
		pr_err("failed to request gpio synaptics_ts_int\n");
	
	ret = gpio_configure(29, GPIOF_INPUT | IRQF_TRIGGER_LOW);/*gpio 29is interupt for touchscreen.*/
	if (ret) {
		SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_probe: gpio_configure 29 failed\n");
		goto err_input_register_device_failed;
	}


	if (client->irq) {
		ret = request_irq(client->irq, synaptics_ts_irq_handler, 0, client->name, ts);
		if (ret == 0) {
			ret = i2c_smbus_write_byte_data(ts->client, 0xf1, 0x03); /* enable  int */
			if (ret) {
				free_irq(client->irq, ts);
				SYNAPITICS_DEBUG("synaptics_ts_probe: enable abs int failed");
			}			
			else
				SYNAPITICS_DEBUG("synaptics_ts_probe: enable abs int succeed!");
		}
		if (ret == 0)
			ts->use_irq = 1;
		else
			dev_err(&client->dev, "synaptics_ts_probe: request_irq failed\n");
	}
	if (!ts->use_irq) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = synaptics_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = synaptics_ts_early_suspend;
	ts->early_suspend.resume = synaptics_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_TOUCH_PANEL);
    #endif
    
	SYNAPITICS_DEBUG(KERN_INFO "synaptics_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");

	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_power_failed:
	kfree(ts);
err_alloc_data_failed:
	
err_find_touchpanel_failed:
    //delete vreg_disable(v_gp5)

err_check_functionality_failed:
	return ret;
}

static int synaptics_ts_power(struct i2c_client *client, int on)
{
	int ret;
	if (on) {		
		ret = i2c_smbus_write_byte_data(client, 0xf0, 0x81);/*sensor on*/	
		if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics sensor can not wake up\n");
        ret = i2c_smbus_write_byte_data(client, 0xf4, 0x01);/*touchscreen reset*/
		if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics chip can not reset\n");	
	}

	else {
		ret = i2c_smbus_write_byte_data(client, 0xf0, 0x86); /* set touchscreen to deep sleep mode*/
		if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics touch can not enter very-low power state\n");
	}
	return ret;	
}

static int synaptics_ts_remove(struct i2c_client *client)
{
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int synaptics_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);
	SYNAPITICS_DEBUG("In synaptics_ts_suspend\n");
	if (ts->use_irq)
		disable_irq(client->irq);
	else
		hrtimer_cancel(&ts->timer);
	ret = cancel_work_sync(&ts->work);
	if (ret && ts->use_irq) /* if work was pending disable-count is now 2 */
		enable_irq(client->irq);
	ret = i2c_smbus_write_byte_data(ts->client, 0xf1, 0); /* disable interrupt */
	if (ret < 0)
		SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_suspend disable interrupt failed\n");
	if (ts->power) {
		ret = ts->power(client,0);
		if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_resume power off failed\n");
	}
	return 0;
}

static int synaptics_ts_resume(struct i2c_client *client)
{
	int ret;
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	SYNAPITICS_DEBUG("In synaptics_ts_resume\n");
	if (ts->power) {
		ret = ts->power(client, 1);
		if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_resume power on failed\n");
//			return -1;
	}

	msleep(200);  /* wait for device reset; */
	ret = i2c_smbus_write_byte_data(client, 0x24, 0x0D);  /* Set Sensitivity Adjust */
	
	if (ts->use_irq) {
		enable_irq(client->irq);
		ret =i2c_smbus_write_byte_data(ts->client, 0xf1, 0x03); /* enable abs int */
		{
			if (ret < 0)
				SYNAPITICS_DEBUG("enable asb interrupt failed\n");		
				return -1;	
		}
	}

	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_ts_early_suspend(struct early_suspend *h)
{
	struct synaptics_ts_data *ts;
	ts = container_of(h, struct synaptics_ts_data, early_suspend);
	synaptics_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void synaptics_ts_late_resume(struct early_suspend *h)
{
	struct synaptics_ts_data *ts;
	ts = container_of(h, struct synaptics_ts_data, early_suspend);
	synaptics_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id synaptics_ts_id[] = {
	{ SYNAPTICS_I2C_RMI_NAME, 0 },
	{ }
};

static struct i2c_driver synaptics_ts_driver = {
	.probe		= synaptics_ts_probe,
	.remove		= synaptics_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= synaptics_ts_suspend,
	.resume		= synaptics_ts_resume,
#endif
	.id_table	= synaptics_ts_id,
	.driver = {
		.name	= SYNAPTICS_I2C_RMI_NAME,
	},
};

static int __devinit synaptics_ts_init(void)
{
	return i2c_add_driver(&synaptics_ts_driver);
}

static void __exit synaptics_ts_exit(void)
{
	i2c_del_driver(&synaptics_ts_driver);
	if (synaptics_wq)
		destroy_workqueue(synaptics_wq);
}

module_init(synaptics_ts_init);
module_exit(synaptics_ts_exit);

MODULE_DESCRIPTION("Synaptics Touchscreen Driver");
MODULE_LICENSE("GPL");
