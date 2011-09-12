/* drivers/input/accelerometer/gs_bosch.c
 *
 * Copyright (C) 2010-2011  Huawei.
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

//#define GS_DEBUG
#undef GS_DEBUG 

#ifdef GS_DEBUG
#define GS_DEBUG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define GS_DEBUG(fmt, args...)
#endif

#define GS_POLLING   1

#define BMA250_DRV_NAME	"gs_bma250"
#define BMA250_I2C_ADDR	0x19
#define BMA250_ID		0x03

/* register enum for BMA250 registers */

enum bma250_reg {
	
    BMA250_CHIP_ID_REG               = 0x00,
    BMA250_VERSION_REG               = 0x01,
    BMA250_X_AXIS_LSB_REG            = 0x02,
    BMA250_X_AXIS_MSB_REG            = 0x03,
    BMA250_Y_AXIS_LSB_REG            = 0x04,
    BMA250_Y_AXIS_MSB_REG            = 0x05,
    BMA250_Z_AXIS_LSB_REG            = 0x06,
    BMA250_Z_AXIS_MSB_REG            = 0x07,
    BMA250_TEMP_RD_REG               = 0x08,
    BMA250_STATUS1_REG               = 0x09,
    BMA250_STATUS2_REG               = 0x0A,
    BMA250_STATUS_TAP_SLOPE_REG      = 0x0B,
    BMA250_STATUS_ORIENT_HIGH_REG    = 0x0C,
    BMA250_RANGE_SEL_REG             = 0x0F,
    BMA250_BW_SEL_REG                = 0x10,
    BMA250_MODE_CTRL_REG             = 0x11,
    BMA250_LOW_NOISE_CTRL_REG        = 0x12,
    BMA250_DATA_CTRL_REG             = 0x13,
    BMA250_RESET_REG                 = 0x14,
    BMA250_INT_ENABLE1_REG           = 0x16,
    BMA250_INT_ENABLE2_REG           = 0x17,
    BMA250_INT1_PAD_SEL_REG          = 0x19,
    BMA250_INT_DATA_SEL_REG          = 0x1A,
    BMA250_INT2_PAD_SEL_REG          = 0x1B,
    BMA250_INT_SRC_REG               = 0x1E,
    BMA250_INT_SET_REG               = 0x20,
    BMA250_INT_CTRL_REG              = 0x21,
    BMA250_LOW_DURN_REG              = 0x22,
    BMA250_LOW_THRES_REG             = 0x23,
    BMA250_LOW_HIGH_HYST_REG         = 0x24,
    BMA250_HIGH_DURN_REG             = 0x25,
    BMA250_HIGH_THRES_REG            = 0x26,
    BMA250_SLOPE_DURN_REG            = 0x27,
    BMA250_SLOPE_THRES_REG           = 0x28,
    BMA250_TAP_PARAM_REG             = 0x2A,
    BMA250_TAP_THRES_REG             = 0x2B,
    BMA250_ORIENT_PARAM_REG          = 0x2C,
    BMA250_THETA_BLOCK_REG           = 0x2D,
    BMA250_THETA_FLAT_REG            = 0x2E,
    BMA250_FLAT_HOLD_TIME_REG        = 0x2F,
    BMA250_STATUS_LOW_POWER_REG      = 0x31,
    BMA250_SELF_TEST_REG             = 0x32,
    BMA250_EEPROM_CTRL_REG           = 0x33,
    BMA250_SERIAL_CTRL_REG           = 0x34,
    BMA250_CTRL_UNLOCK_REG           = 0x35,
    BMA250_OFFSET_CTRL_REG           = 0x36,
    BMA250_OFFSET_PARAMS_REG         = 0x37,
    BMA250_OFFSET_FILT_X_REG         = 0x38,
    BMA250_OFFSET_FILT_Y_REG         = 0x39,
    BMA250_OFFSET_FILT_Z_REG         = 0x3A,
    BMA250_OFFSET_UNFILT_X_REG       = 0x3B,
    BMA250_OFFSET_UNFILT_Y_REG       = 0x3C,
    BMA250_OFFSET_UNFILT_Z_REG       = 0x3D,
    BMA250_SPARE_0_REG               = 0x3E,
    BMA250_SPARE_1_REG               = 0x3F,
};

enum {
	MODE_NORMAL = 0,
	MODE_LOWPOWER,
	MODE_SUSPEND,
};
#define BMA250_EN_LOW_POWER__MSK          0x40
#define BMA250_EN_SUSPEND__MSK            0x80


#define MG_PER_SAMPLE				720            /*HAL: 720=1g*/                       
#define FILTER_SAMPLE_NUMBER		1024           /*1024LSB =4g*/  
#define	GPIO_INT1                   19
#define GPIO_INT2                   20
#define GS_TIMRER                    (1000)           /*1000ms*/

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

static struct workqueue_struct *gs_wq;
static signed short compass_sensor_data[3];
static char gs_device_id[] = BMA250_DRV_NAME;

extern struct input_dev *sensor_dev;
#ifdef CONFIG_MELFAS_UPDATE_TS_FIRMWARE
extern struct gs_data *TS_updateFW_gs_data;
#endif

static int accel_delay = GS_TIMRER;     /*1s*/

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
		printk(KERN_ERR "BMA250 chip i2c %s failed\n", __FUNCTION__);

	mutex_unlock(&gs->mlock);

	return val;
}
static inline int reg_write(struct gs_data *gs, int reg, uint8_t val)
{
	int ret;

	mutex_lock(&gs->mlock);
	ret = i2c_smbus_write_byte_data(gs->client, reg, val);
	if(ret < 0) {
		printk(KERN_ERR "BMA250 chip i2c %s failed\n", __FUNCTION__);
	}
	mutex_unlock(&gs->mlock);

	return ret;
}

static int bma250_set_mode(unsigned char Mode)
{
	unsigned char data;
	data = reg_read(this_gs_data,BMA250_MODE_CTRL_REG);
	switch (Mode)
	{
		case MODE_NORMAL:
			data = data & ~BMA250_EN_LOW_POWER__MSK;
			data = data & ~BMA250_EN_SUSPEND__MSK;
			break;
		case MODE_LOWPOWER:
			data = (data & ~BMA250_EN_LOW_POWER__MSK) | BMA250_EN_LOW_POWER__MSK;
			data = data & ~BMA250_EN_SUSPEND__MSK;
			break;
		case MODE_SUSPEND:
			data = data & ~BMA250_EN_LOW_POWER__MSK;
			data = (data & (~BMA250_EN_SUSPEND__MSK)) | BMA250_EN_SUSPEND__MSK;
			break;
            default:
               break;
	}
	reg_write(this_gs_data,BMA250_MODE_CTRL_REG,data);
	return 0;
}
/**************************************************************************************/

static int gs_data_to_compass(signed short accel_data [3])
{
	memset((void*)accel_data, 0, sizeof(accel_data));
	accel_data[0]=compass_sensor_data[0];
	accel_data[1]=compass_sensor_data[1];
	accel_data[2]=compass_sensor_data[2];
	return 0;
}

/**************************************************************************************/

static int gs_bma250_open(struct inode *inode, struct file *file)
{	
    /*gs active mode*/
	bma250_set_mode(MODE_NORMAL); 
	if (this_gs_data->use_irq)
		enable_irq(this_gs_data->client->irq);
	else
		hrtimer_start(&this_gs_data->timer, ktime_set(0, 500000000), HRTIMER_MODE_REL);

	return nonseekable_open(inode, file);
}

static int gs_bma250_release(struct inode *inode, struct file *file)
{
	
	bma250_set_mode(MODE_SUSPEND); 

	if (this_gs_data->use_irq)
		disable_irq(this_gs_data->client->irq);
	else
		hrtimer_cancel(&this_gs_data->timer);

	accel_delay = GS_TIMRER;	  

	return 0;
}

static int
gs_bma250_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	signed short accel_buf[3];
	short flag;
	
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
			if (copy_to_user(argp, gs_device_id, sizeof(gs_device_id)))
				return -EFAULT;
			break;
			
		default:
			break;
	}
	return 0;
}

static struct file_operations gs_bma250_fops = {
	.owner = THIS_MODULE,
	.open = gs_bma250_open,
	.release = gs_bma250_release,
	.ioctl = gs_bma250_ioctl,
};

static struct miscdevice gsensor_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "accel",
	.fops = &gs_bma250_fops,
};

static void gs_work_func(struct work_struct *work)
{
	int status_x;	
	int status_y;	
	int status_z;	
	int x = 0;
	int y = 0;
	int z = 0;
	u8 udata[2]={0};
	
	struct gs_data *gs = container_of(work, struct gs_data, work);
	int	sesc = accel_delay/1000;
	int nsesc = (accel_delay%1000)*1000000;
       
	status_x = reg_read(gs, BMA250_X_AXIS_LSB_REG ); /* read status */
	status_y = reg_read(gs, BMA250_Y_AXIS_LSB_REG ); /* read status */
	status_z = reg_read(gs, BMA250_Z_AXIS_LSB_REG ); /* read status */
	
	if((status_x & 0x01) || (status_y & 0x01) || (status_z & 0x01))
	{
		udata[0] = reg_read(gs, BMA250_X_AXIS_LSB_REG ); /* read status */
		GS_DEBUG("%s:A_x l 0x%x \n", __FUNCTION__, udata[0]);
		udata[1] = reg_read(gs, BMA250_X_AXIS_MSB_REG ); /* read status */
		GS_DEBUG("%s:A_x h 0x%x \n", __FUNCTION__, udata[1]);
		x = ((udata[1])<<2)|udata[0]>>6;

		udata[0]= 0;
		udata[1]= 0;
		udata[0] = reg_read(gs, BMA250_Y_AXIS_LSB_REG ); /* read status */
		GS_DEBUG("%s:A_y l 0x%x \n", __FUNCTION__, udata[0]);
		udata[1] = reg_read(gs, BMA250_Y_AXIS_MSB_REG ); /* read status */
		GS_DEBUG("%s:A_y h 0x%x \n", __FUNCTION__, udata[1]);
		y = ((udata[1])<<2)|udata[0]>>6;

		udata[0]= 0;
		udata[1]= 0;
		udata[0] = reg_read(gs, BMA250_Z_AXIS_LSB_REG ); /* read status */
		GS_DEBUG("%s:A_z l 0x%x \n", __FUNCTION__, udata[0]);
		udata[1] = reg_read(gs, BMA250_Z_AXIS_MSB_REG ); /* read status */
		GS_DEBUG("%s:A_z h 0x%x \n", __FUNCTION__, udata[1]);
		z = ((udata[1])<<2)|udata[0]>>6;
		
		GS_DEBUG("A  x :0x%x y :0x%x z :0x%x \n", x,y,z);
	 
		if(x&0x200)/**/
		{
			x -= 1024; 		/*2¡¯s complement 10-bit numbers*/
		}
					
		if(y&0x200)/**/
		{
			y -= 1024; 		/*2¡¯s complement 10-bit numbers*/  	 
		}
	
		if(z&0x200)/**/
		{
			z -= 1024; 		/*2¡¯s complement 10-bit numbers*/ 
		}

		memset((void*)compass_sensor_data, 0, sizeof(compass_sensor_data));
		//compass_sensor_data[0]= -x;
		//compass_sensor_data[1]= -y;	
		//compass_sensor_data[2]= -z;
		
		/*(Decimal value/ 4096) * 4.0 g,For (0g ~+2.0g)*/	
		x = (MG_PER_SAMPLE*40*(s16)x)/FILTER_SAMPLE_NUMBER/10;           
		y = (MG_PER_SAMPLE*40*(s16)y)/FILTER_SAMPLE_NUMBER/10;
		z = (MG_PER_SAMPLE*40*(s16)z)/FILTER_SAMPLE_NUMBER/10;
		
		compass_sensor_data[0]= -x;
		compass_sensor_data[1]= y;	
		compass_sensor_data[2]= -z;		
		
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

#ifndef GS_POLLING 	

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
#endif /*GS_POLLING*/

static int gs_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{	
	int ret = 0;
	s32 result = 0;
	struct gs_data *gs;
	struct gs_platform_data *pdata = NULL;
    
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "gs_BMA250_probe: need I2C_FUNC_I2C\n");
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
				printk(KERN_ERR "gs_bma250 probe failed, because the othe gsensor has been probed.\n");
				ret = -ENODEV;
				goto err_check_functionality_failed;
			}
		}
	}
	
	printk(KERN_INFO "%s:check Bosh bma250 chip ID\n", __FUNCTION__);
	result = i2c_smbus_read_byte_data(client, BMA250_CHIP_ID_REG);

	if (0 > result) {	//compare the address value 
		dev_err(&client->dev,"read chip ID 0x%x is not equal to 0x%x!\n", result,BMA250_ID);
		printk(KERN_INFO "read chip ID failed\n");
		result = -ENODEV;
		goto err_check_functionality_failed;
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

	
    /* Initialize the BMA250 chip */
	ret = reg_write(gs, BMA250_RESET_REG, 0xB6);   /*software reset chip*/
	if (ret < 0) {
		printk(KERN_ERR "%s:i2c_smbus_write_byte_data failed\n", __FUNCTION__);
		/* fail? */
		goto err_detect_failed;
	}
	mdelay(2);
	
	ret |= reg_write(gs, BMA250_DATA_CTRL_REG, 0x00); /*filter data and enable shadow procedure*/
	ret |= reg_write(gs, BMA250_BW_SEL_REG, 0x0C); /*bandwidth 125Hz*/

	ret |= reg_write(gs, BMA250_RANGE_SEL_REG, 0x03); /*range +/-2G */
	

	if (ret < 0) {
		printk(KERN_ERR "%s:i2c Initialize the BMA250 chip failed, err=%d\n",__FUNCTION__, ret);
		/* fail? */
		goto err_detect_failed;
	}

	if (sensor_dev == NULL)
	{
		gs->input_dev = input_allocate_device();
		if (gs->input_dev == NULL) {
			ret = -ENOMEM;
			printk(KERN_ERR "%s: Failed to allocate input device\n",__FUNCTION__);
			goto err_input_dev_alloc_failed;
		}
		
		gs->input_dev->name = "sensors";
		sensor_dev = gs->input_dev;
		
	}else{
	
		gs->input_dev = sensor_dev;
	}
	
	gs->input_dev->id.vendor = GS_BMA250;
	
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
	bma250_set_mode(MODE_SUSPEND);

	//set_st303_gs_support(true);
	if(pdata && pdata->init_flag)
		*(pdata->init_flag) = 1;   /*set initlized flag*/

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_G_SENSOR);
    #endif

	printk(KERN_INFO "gs_probe: Start BMA250  in %s mode\n", gs->use_irq ? "interrupt" : "polling");

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
	
	bma250_set_mode(MODE_SUSPEND); 
	return 0;
}

static int gs_resume(struct i2c_client *client)
{
	struct gs_data *gs = i2c_get_clientdata(client);
	
	bma250_set_mode(MODE_NORMAL); 	
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
	{ BMA250_DRV_NAME, 0 },
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
		.name	=BMA250_DRV_NAME,
	},
};

static int __devinit gs_bma250_init(void)
{
	return i2c_add_driver(&gs_driver);
}

static void __exit gs_bma250_exit(void)
{
	i2c_del_driver(&gs_driver);
	if (gs_wq)
		destroy_workqueue(gs_wq);
}

device_initcall_sync(gs_bma250_init);
module_exit(gs_bma250_exit);

MODULE_DESCRIPTION("gs_bma250 Driver");
MODULE_LICENSE("GPL");

