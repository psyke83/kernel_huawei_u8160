/* drivers/i2c/chips/st303_gs.c
 *
 * Copyright (C) 2007-2010  Huawei.
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


#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include "linux/hardware_self_adapt.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

//#define GS303_DEBUG
#undef GS303_DEBUG 

#ifdef GS303_DEBUG
#define GS303_DEBUG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define GS303_DEBUG(fmt, args...)
#endif
#define GS_POLLING   1

static struct workqueue_struct *gs_wq;
static signed short st_sensor_data[3];

#define ST303DLH_I2C_NAME "st_303dlh"
#define ST303DLM_I2C_NAME "st_303dlm"

enum
{
    DEV_ID_NONE = 0,
    DEV_ID_303DLH = 1,
    DEV_ID_303DLM = 2,
};

extern int st303_dev_id;

extern struct input_dev *sensor_dev;

enum st303_reg {
	
	GS_ST_REG_CTRL1				= 0x20,
	GS_ST_REG_CTRL2				= 0x21,
	GS_ST_REG_CTRL3				= 0x22,
	GS_ST_REG_CTRL4				= 0x23,
	GS_ST_REG_CTRL5				= 0x24,
	GS_ST_REG_HP_FILTER_A		= 0x25,
	GS_ST_REFERENCE_A			= 0x26,
	GS_ST_REG_STATUS			= 0x27,
	GS_ST_OUT_X_L_A				= 0x28,
	GS_ST_OUT_X_H_A				= 0x29,
	GS_ST_OUT_Y_L_A				= 0x2a,
	GS_ST_OUT_Y_H_A				= 0x2b,
	GS_ST_OUT_Z_L_A 			= 0x2c,
	GS_ST_OUT_Z_H_A 			= 0x2d,
	GS_ST_REG_FF_WU_CFG_1		= 0x30,
	GS_ST_REG_FF_WU_SRC_1		= 0x31,
	GS_ST_REG_FF_WU_THS_1		= 0x32,
	GS_ST_REG_FF_WU_DURATION_1	= 0x33,
	GS_ST_REG_FF_WU_CFG_2		= 0x34,
	GS_ST_REG_FF_WU_SRC_2		= 0x35,
	GS_ST_REG_FF_WU_THS_2		= 0x36,
	GS_ST_REG_FF_WU_DURATION_2	= 0x37,
	GS_ST_REG_CLICK_CFG			= 0x38,
	GS_ST_REG_CLICK_SRC			= 0x39,
	GS_ST_REG_CLICK_THSY_X		= 0x3b,
	GS_ST_REG_CLICK_THSZ		= 0x3c,
	GS_ST_REG_CLICK_TIME_LIMIT	= 0x3d,
	GS_ST_REG_CLICK_LATENCY		= 0x3e,
	GS_ST_REG_CLICK_WINDOW		= 0x3f,
};

#define MG_PER_SAMPLE				720            /*HAL: 720=1g*/                       
#define FILTER_SAMPLE_NUMBER		4096           /*256LSB =1g*/  
#define	GPIO_INT1                   19
#define GPIO_INT2                   20
#define GS_ST_TIMRER                    (1000*1000000)           /*1000000s*/

#define ECS_IOCTL_READ_ACCEL_XYZ			_IOR(0xA1, 0x06, char[3])
#define ECS_IOCTL_APP_SET_DELAY 			_IOW(0xA1, 0x18, short)
#define ECS_IOCTL_APP_GET_DELAY 			_IOR(0xA1, 0x30, short)
#define ECS_IOCTL_APP_SET_AFLAG 			_IOW(0xA1, 0x13, short)
#define ECS_IOCTL_APP_GET_AFLAG				_IOR(0xA1, 0x14, short)
#define ECS_IOCTL_READ_DEVICEID				_IOR(0xA1, 0x31, char[20])

struct gs_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	
	
	struct mutex  mlock;
	
	
	struct hrtimer timer;
	struct work_struct  work;	
	uint32_t flags;
	struct early_suspend early_suspend;
};


static struct gs_data  *this_gs_data;

#ifdef CONFIG_MELFAS_UPDATE_TS_FIRMWARE
	struct gs_data *TS_updateFW_gs_data;
#endif

static int accel_delay = GS_ST_TIMRER;     /*1s*/

static atomic_t a_flag;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gs_early_suspend(struct early_suspend *h);
static void gs_late_resume(struct early_suspend *h);
#endif

/**************************************************************************************/
static inline int reg_read(struct gs_data *gs , int reg)
{
	int val;

	mutex_lock(&gs->mlock);

	val = i2c_smbus_read_byte_data(gs->client, reg);
	if (val < 0)
		printk(KERN_ERR "st303_gs.c failed\n");

	mutex_unlock(&gs->mlock);

	return val;
}
static inline int reg_write(struct gs_data *gs, int reg, uint8_t val)
{
	int ret;

	mutex_lock(&gs->mlock);
	ret = i2c_smbus_write_byte_data(gs->client, reg, val);
	if(ret < 0) {
		printk(KERN_ERR "st303_gs.c failed \n");
	}
	mutex_unlock(&gs->mlock);

	return ret;
}

/**************************************************************************************/

static int gs_data_to_compass(signed short accel_data [3])
{
	memset((void*)accel_data, 0, sizeof(accel_data));
	accel_data[0]=st_sensor_data[0];
	accel_data[1]=st_sensor_data[1];
	accel_data[2]=st_sensor_data[2];
	return 0;
}

/**************************************************************************************/

static int gs_st_open(struct inode *inode, struct file *file)
{	
	reg_write(this_gs_data, GS_ST_REG_CTRL1, 0x2f);
	if (this_gs_data->use_irq)
		enable_irq(this_gs_data->client->irq);
	else
		hrtimer_start(&this_gs_data->timer, ktime_set(0, 500000000), HRTIMER_MODE_REL);

	return nonseekable_open(inode, file);
}

static int gs_st_release(struct inode *inode, struct file *file)
{
	
	reg_write(this_gs_data, GS_ST_REG_CTRL1, 0x00); 

	if (this_gs_data->use_irq)
		disable_irq(this_gs_data->client->irq);
	else
		hrtimer_cancel(&this_gs_data->timer);

	accel_delay = GS_ST_TIMRER;	  

	return 0;
}

static int
gs_st_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	signed short accel_buf[3];
	short flag;
	int i=0;
	
	switch (cmd) 
	{
		case ECS_IOCTL_APP_SET_AFLAG:     /*set open acceleration sensor flag*/
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
			break;
				
		case ECS_IOCTL_APP_SET_DELAY:
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
			break;
		
			default:
				break;
	}
	switch (cmd) 
	{
		case ECS_IOCTL_APP_SET_AFLAG:
			atomic_set(&a_flag, flag);
			break;
			
		case ECS_IOCTL_APP_GET_AFLAG:  /*get open acceleration sensor flag*/
			flag = atomic_read(&a_flag);
			break;
			
		case ECS_IOCTL_APP_SET_DELAY:
			if(flag)
				accel_delay = flag;
			else
				accel_delay = 10;   /*10ms*/
			break;
			
		case ECS_IOCTL_APP_GET_DELAY:
			flag = accel_delay;
			break;
			
		case ECS_IOCTL_READ_ACCEL_XYZ:
			gs_data_to_compass(accel_buf);
			break;
			
		default:
			break;
	}
	/*get device ID*/
	switch (cmd) 
	{
		case ECS_IOCTL_APP_GET_AFLAG:
			if (copy_to_user(argp, &flag, sizeof(flag)))
				return -EFAULT;
			break;

		case ECS_IOCTL_APP_GET_DELAY:
			if (copy_to_user(argp, &flag, sizeof(flag)))
				return -EFAULT;
			break;
			
		case ECS_IOCTL_READ_ACCEL_XYZ:
			if (copy_to_user(argp, &accel_buf, sizeof(accel_buf)))
				return -EFAULT;
			break;
			
		case ECS_IOCTL_READ_DEVICEID:
			if((st303_dev_id == DEV_ID_303DLH) )
			{
				if (copy_to_user(argp, ST303DLH_I2C_NAME, strlen(ST303DLH_I2C_NAME)+1))
					return -EFAULT;
			}
			else if(st303_dev_id == DEV_ID_303DLM)
			{
				if (copy_to_user(argp, ST303DLM_I2C_NAME, strlen(ST303DLM_I2C_NAME)+1))
					return -EFAULT;
			}
			break;
			
		default:
			break;
	}
	return 0;
}

static struct file_operations gs_st_fops = {
	.owner = THIS_MODULE,
	.open = gs_st_open,
	.release = gs_st_release,
	.ioctl = gs_st_ioctl,
};

static struct miscdevice gsensor_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "accel",
	.fops = &gs_st_fops,
};

static void gs_work_func(struct work_struct *work)
{
	int status;	
	signed short x = 0;
	signed short y = 0;
	signed short z = 0;
	u8 udata[2]={0};
	
	struct gs_data *gs = container_of(work, struct gs_data, work);
	int	sesc = accel_delay/1000;
	int nsesc = (accel_delay%1000)*1000000;
       
	 status = reg_read(gs, GS_ST_REG_STATUS ); /* read status */
	
	if(status & (1<<3))
	{
		udata[0] = reg_read(gs, GS_ST_OUT_X_L_A ); /* read status */
		GS303_DEBUG(KERN_ERR "A_x l 0x%x \n", udata[0]);
		udata[1] = reg_read(gs, GS_ST_OUT_X_H_A ); /* read status */
		GS303_DEBUG(KERN_ERR "A_x h 0x%x \n", udata[1]);
		x = ((udata[1])<<4)|udata[0]>>4;

		udata[0]= 0;
		udata[1]= 0;
		udata[0] = reg_read(gs, GS_ST_OUT_Y_L_A ); /* read status */
		GS303_DEBUG(KERN_ERR "A_y l 0x%x \n", udata[0]);
		udata[1] = reg_read(gs, GS_ST_OUT_Y_H_A ); /* read status */
		GS303_DEBUG(KERN_ERR "A_y h 0x%x \n", udata[1]);
		y = ((udata[1])<<4)|udata[0]>>4;

		udata[0]= 0;
		udata[1]= 0;
		udata[0] = reg_read(gs, GS_ST_OUT_Z_L_A ); /* read status */
		GS303_DEBUG(KERN_ERR "A_z l 0x%x \n", udata[0]);
		udata[1] = reg_read(gs, GS_ST_OUT_Z_H_A ); /* read status */
		GS303_DEBUG(KERN_ERR "A_z h 0x%x \n", udata[1]);
		z = ((udata[1])<<4)|udata[0]>>4;
		
		GS303_DEBUG(KERN_ERR "A  x :0x%x y :0x%x z :0x%x \n", x,y,z);
	 
		if(x&0x800)/*负值*/
		{
			x -= 4096; 		/*负数按照补码计算 */  
		}
					
		if(y&0x800)/*负值*/
		{
			y -= 4096; 		/*负数按照补码计算 */  	 
		}
	
		if(z&0x800)/*负值*/
		{
			z -= 4096; 		/*负数按照补码计算 */   
		}

		memset((void*)st_sensor_data, 0, sizeof(st_sensor_data));
		st_sensor_data[0]= -x;
		st_sensor_data[1]= -y;	
		st_sensor_data[2]= -z;
		
		/*(Decimal value/ 4096) * 4.0 g,For (0g ~+2.0g)*/	
		x = (MG_PER_SAMPLE*40*(s16)x)/FILTER_SAMPLE_NUMBER/10;           
		y = (MG_PER_SAMPLE*40*(s16)y)/FILTER_SAMPLE_NUMBER/10;
		z = (MG_PER_SAMPLE*40*(s16)z)/FILTER_SAMPLE_NUMBER/10;
				
		x *=(-1);
		y *=(-1);

		input_report_abs(gs->input_dev, ABS_X, y);//cross x,y adapter hal sensors_akm8973.c			
		input_report_abs(gs->input_dev, ABS_Y, x);			
		input_report_abs(gs->input_dev, ABS_Z, z);
		input_sync(gs->input_dev);

	}
	if (gs->use_irq)
		enable_irq(gs->client->irq);
	else
		hrtimer_start(&gs->timer, ktime_set(sesc, nsesc), HRTIMER_MODE_REL);
	
}


static enum hrtimer_restart gs_timer_func(struct hrtimer *timer)
{
	struct gs_data *gs = container_of(timer, struct gs_data, timer);		
	queue_work(gs_wq, &gs->work);
	//hrtimer_start(&gs->timer, ktime_set(0, 512500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

#ifndef   GS_POLLING 	

static irqreturn_t gs_irq_handler(int irq, void *dev_id)
{
	struct gs_data *gs = dev_id;
	disable_irq(gs->client->irq);
	queue_work(gs_wq, &gs->work);
	return IRQ_HANDLED;
}

static int gs_config_int_pin(void)
{
	int err;

	err = gpio_request(GPIO_INT1, "gpio_gs_int");
	if (err) 
	{
		printk(KERN_ERR "gpio_request failed for st gs int\n");
		return -1;
	}	

	err = gpio_configure(GPIO_INT1, GPIOF_INPUT | IRQF_TRIGGER_HIGH);
	if (err) 
	{
		gpio_free(GPIO_INT1);
		printk(KERN_ERR "gpio_config failed for gs int HIGH\n");
		return -1;
	}     

	return 0;
}

static void gs_free_int(void)
{

	gpio_free(GPIO_INT1);

}
#endif
static int gs_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{	
	int ret;
	struct gs_data *gs;
	struct gs_platform_data *pdata = NULL;
    
	printk(KERN_ERR "st303_gs probe \n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "gs_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	
	pdata = client->dev.platform_data;
	if (pdata){
		if(pdata->adapt_fn != NULL){
			ret = pdata->adapt_fn();
			if(ret > 0){
				client->addr = pdata->slave_addr;//actual address
				printk(KERN_INFO "%s:change i2c addr to actrual address = %d\n", __FUNCTION__, pdata->slave_addr);
				if(client->addr == 0){
					printk(KERN_ERR "%s: bad i2c address = %d\n", __FUNCTION__, client->addr);
					ret = -EFAULT;
					goto err_check_functionality_failed;
				}
			}
		}

		if(pdata->init_flag != NULL){
			if(*(pdata->init_flag)){
				printk(KERN_ERR "st303_gs probe failed, because the othe gsensor has been probed.\n");
				ret = -ENODEV;
				goto err_check_functionality_failed;
			}
		}
	}
	
#ifndef   GS_POLLING 	
	ret = gs_config_int_pin();
	if(ret <0)
	{
		goto err_check_functionality_failed;
	}
#endif

	gs = kzalloc(sizeof(*gs), GFP_KERNEL);
	if (gs == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	mutex_init(&gs->mlock);

	INIT_WORK(&gs->work, gs_work_func);
	gs->client = client;
	i2c_set_clientdata(client, gs);

	ret = reg_write(gs, GS_ST_REG_CTRL1, 0x2f);
	if (ret < 0) {
		printk(KERN_ERR "i2c_smbus_write_byte_data failed\n");
		/* fail? */
		goto err_detect_failed;
	}
	
	ret = reg_write(gs, GS_ST_REG_CTRL2, 0x00);
	ret = reg_write(gs, GS_ST_REG_CTRL3, 0x00);
	ret = reg_write(gs, GS_ST_REG_CTRL4, 0x00);
	ret = reg_write(gs, GS_ST_REG_FF_WU_CFG_1, 0); /* disable interrupt1 */
	ret = reg_write(gs, GS_ST_REG_FF_WU_CFG_2, 0); /* disable interrupt2 */

	if (ret < 0) {
		printk(KERN_ERR "i2c_smbus_write_byte_data failed\n");
		/* fail? */
		goto err_detect_failed;
	}

	if (sensor_dev == NULL)
	{
		gs->input_dev = input_allocate_device();
		if (gs->input_dev == NULL) {
			ret = -ENOMEM;
			printk(KERN_ERR "gs_probe: Failed to allocate input device\n");
			goto err_input_dev_alloc_failed;
		}
		
		gs->input_dev->name = "sensors";
		sensor_dev = gs->input_dev;
		
	}else{
	
		gs->input_dev = sensor_dev;
	}
	
	gs->input_dev->id.vendor = GS_ST303DLH;//for st303_compass detect.
	
	set_bit(EV_ABS,gs->input_dev->evbit);
	set_bit(ABS_X, gs->input_dev->absbit);
	set_bit(ABS_Y, gs->input_dev->absbit);
	set_bit(ABS_Z, gs->input_dev->absbit);
	set_bit(EV_SYN,gs->input_dev->evbit);

	gs->input_dev->id.bustype = BUS_I2C;
	
	input_set_drvdata(gs->input_dev, gs);
	
	ret = input_register_device(gs->input_dev);
	if (ret) {
		printk(KERN_ERR "gs_probe: Unable to register %s input device\n", gs->input_dev->name);
		goto err_input_register_device_failed;
	}
	
	ret = misc_register(&gsensor_device);
	if (ret) {
		printk(KERN_ERR "gs_probe: gsensor_device register failed\n");
		goto err_misc_device_register_failed;
	}

#ifndef   GS_POLLING 
	if (client->irq) {
		ret = request_irq(client->irq, gs_irq_handler, 0, client->name, gs);
		
		if (ret == 0)
			gs->use_irq = 1;
		else
			dev_err(&client->dev, "request_irq failed\n");
	}
#endif 

	if (!gs->use_irq) {
		hrtimer_init(&gs->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		gs->timer.function = gs_timer_func;
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	gs->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	gs->early_suspend.suspend = gs_early_suspend;
	gs->early_suspend.resume = gs_late_resume;
	register_early_suspend(&gs->early_suspend);
#endif

	gs_wq = create_singlethread_workqueue("gs_wq");
	if (!gs_wq)
		return -ENOMEM;
	
	this_gs_data =gs;

	set_st303_gs_support(true);
	if(pdata && pdata->init_flag)
		*(pdata->init_flag) = 1;

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_G_SENSOR);
    #endif

	printk(KERN_INFO "gs_probe: Start LSM303DLH  in %s mode\n", gs->use_irq ? "interrupt" : "polling");

#ifdef CONFIG_MELFAS_UPDATE_TS_FIRMWARE
	TS_updateFW_gs_data = this_gs_data;
#endif

	return 0;
	
err_misc_device_register_failed:
	misc_deregister(&gsensor_device);

err_input_register_device_failed:
	input_free_device(gs->input_dev);

err_input_dev_alloc_failed:
err_detect_failed:
	kfree(gs);
err_alloc_data_failed:
#ifndef   GS_POLLING 
	gs_free_int();
#endif
err_check_functionality_failed:
	return ret;
}

static int gs_remove(struct i2c_client *client)
{
	struct gs_data *gs = i2c_get_clientdata(client);
	unregister_early_suspend(&gs->early_suspend);
	if (gs->use_irq)
		free_irq(client->irq, gs);
	else
		hrtimer_cancel(&gs->timer);
	misc_deregister(&gsensor_device);
	input_unregister_device(gs->input_dev);
	kfree(gs);
	return 0;
}

static int gs_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct gs_data *gs = i2c_get_clientdata(client);

	if (gs->use_irq)
		disable_irq(client->irq);
	else
		hrtimer_cancel(&gs->timer);
	ret = cancel_work_sync(&gs->work);
	if (ret && gs->use_irq) 
		enable_irq(client->irq);
	
	reg_write(gs, GS_ST_REG_CTRL1, 0x00); /* power down */
	return 0;
}

static int gs_resume(struct i2c_client *client)
{
	struct gs_data *gs = i2c_get_clientdata(client);
	
	reg_write(gs, GS_ST_REG_CTRL1, 0x2f);/*normal mode 100hz  */
	
	if (!gs->use_irq)
		hrtimer_start(&gs->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	else
		enable_irq(client->irq);
	

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void gs_early_suspend(struct early_suspend *h)
{
	struct gs_data *gs;
	gs = container_of(h, struct gs_data, early_suspend);
	gs_suspend(gs->client, PMSG_SUSPEND);
}

static void gs_late_resume(struct early_suspend *h)
{
	struct gs_data *gs;
	gs = container_of(h, struct gs_data, early_suspend);
	gs_resume(gs->client);
}
#endif

static const struct i2c_device_id gs_id[] = {
	{ "st303_gs", 0 },
	{ }
};

static struct i2c_driver gs_driver = {
	.probe		=gs_probe,
	.remove		= gs_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= gs_suspend,
	.resume		= gs_resume,
#endif
	.id_table	= gs_id,
	.driver = {
		.name	="st303_gs",
	},
};

static int __devinit gs_init(void)
{
	return i2c_add_driver(&gs_driver);
}

static void __exit gs_exit(void)
{
	i2c_del_driver(&gs_driver);
	if (gs_wq)
		destroy_workqueue(gs_wq);
}

module_init(gs_init);
module_exit(gs_exit);

MODULE_DESCRIPTION("ST303_gs Driver");
MODULE_LICENSE("GPL");

