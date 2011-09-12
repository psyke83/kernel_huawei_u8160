/*add code to support JOY(U8120) */
/* drivers/input/mouse/ofn_avago_a320.c
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
#include <mach/gpio.h>
#include <mach/vreg.h>
#include "linux/hardware_self_adapt.h"
#include <linux/miscdevice.h>
#include <linux/gs_st.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

/*
 * DEBUG SWITCH
 *
 */ 

//#define AVAGO_DEBUG 
//#undef AVAGO_DEBUG 

#ifdef AVAGO_DEBUG
#define AVAGO_DEBUG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define AVAGO_DEBUG(fmt, args...)
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define AVAGO_OFN_NAME "avago_OFN"

#define ADBM_A320_POWERUP_RETRIES  		 10

// Configuration Register Individual Bit Field Settings
#define ADBM_A320_MOTION_MOT       		 0x80
#define ADBM_A320_MOTION_PIXRDY    		 0x40
#define ADBM_A320_MOTION_PIXFIRST  		 0x20
#define ADBM_A320_MOTION_OVF       		 0x10
#define ADBM_A320_MOTION_GPIO      		 0x01

// Configuration Register Settings
#define ADBM_A320_SOFTRESET_INIT  		 0x5A
#define ADBM_A320_SELF_TEST     		 0x01
#define ADBM_A320_NO_REST              	 0x00
#define ADBM_A320_REST1              	 0x10
#define ADBM_A320_REST2              	 0x20
#define ADBM_A320_REST3              	 0x30
#define ADBM_A320_RES500CPI          	 0x00
#define ADBM_A320_RES1000CPI          	 0x80
#define ADBM_A320_LED0               	 0x01     
#define ADBM_A320_LED3             	 	 0x08     //0:13mA; 1:20mA
#define ADBM_A320_BURST               	 0x10
#define ADBM_A320_SPI               	 0x04
#define ADBM_A320_TWI              	 	 0x01
#define ADBM_A320_RUN_MODE          	 0x00
#define ADBM_A320_REST1_MODE          	 0x40
#define ADBM_A320_REST2_MODE          	 0x80
#define ADBM_A320_REST3_MODE          	 0xC0

// ADBM_A320 Register Addresses
#define ADBM_A320_PRODUCTID_ADDR         0x00     //0x83
#define ADBM_A320_REVISIONID_ADDR        0x01     //0x00
#define ADBM_A320_MOTION_ADDR            0x02
#define ADBM_A320_DELTAX_ADDR            0x03
#define ADBM_A320_DELTAY_ADDR            0x04
#define ADBM_A320_SQUAL_ADDR             0x05
#define ADBM_A320_SHUTTERUPPER_ADDR      0x06
#define ADBM_A320_SHUTTERLOWER_ADDR      0x07
#define ADBM_A320_MAXIMUMPIXEL_ADDR      0x08
#define ADBM_A320_PIXELSUM_ADDR          0x09
#define ADBM_A320_MINIMUMPIXEL_ADDR      0x0A
#define ADBM_A320_PIXELGRAB_ADDR         0x0B
#define ADBM_A320_CRC0_ADDR              0x0C
#define ADBM_A320_CRC1_ADDR              0x0D
#define ADBM_A320_CRC2_ADDR              0x0E
#define ADBM_A320_CRC3_ADDR              0x0F
#define ADBM_A320_SELFTEST_ADDR          0x10
#define ADBM_A320_CONFIGURATIONBITS_ADDR 0x11
#define ADBM_A320_LED_CONTROL_ADDR       0x1A
#define ADBM_A320_IO_MODE_ADDR           0x1C
#define ADBM_A320_OBSERVATION_ADDR       0x2E
#define ADBM_A320_SOFTRESET_ADDR      	 0x3A     //0x5A
#define ADBM_A320_SHUTTER_MAX_HI_ADDR    0x3B
#define ADBM_A320_SHUTTER_MAX_LO_ADDR    0x3C
#define ADBM_A320_INVERSEREVISIONID_ADDR 0x3E     //0xFF
#define ADBM_A320_INVERSEPRODUCTID_ADDR  0x3F     //0x7C

#define ADBM_A320_OFN_ENGINE_ADDR  				0x60
#define ADBM_A320_OFN_RESOLUTION_ADDR  			0x62
#define ADBM_A320_OFN_SPEED_CONTROL_ADDR  		0x63
#define ADBM_A320_OFN_SPEED_ST12_ADDR  			0x64
#define ADBM_A320_OFN_SPEED_ST21_ADDR  			0x65
#define ADBM_A320_OFN_SPEED_ST23_ADDR  			0x66
#define ADBM_A320_OFN_SPEED_ST32_ADDR  			0x67
#define ADBM_A320_OFN_SPEED_ST34_ADDR  			0x68
#define ADBM_A320_OFN_SPEED_ST43_ADDR  			0x69
#define ADBM_A320_OFN_SPEED_ST45_ADDR  			0x6A
#define ADBM_A320_OFN_SPEED_ST54_ADDR  			0x6B
#define ADBM_A320_OFN_AD_CTRL_ADDR  			0x6D
#define ADBM_A320_OFN_AD_ATH_HIGH_ADDR  		0x6E
#define ADBM_A320_OFN_AD_DTH_HIGH_ADDR  		0x6F
#define ADBM_A320_OFN_AD_ATH_LOW_ADDR  			0x70
#define ADBM_A320_OFN_AD_DTH_LOW_ADDR  			0x71
#define ADBM_A320_OFN_QUANTIZE_CTRL_ADDR  		0x73
#define ADBM_A320_OFN_XYQ_THRESH_ADDR  			0x74
#define ADBM_A320_OFN_FPD_CTRL_ADDR  			0x75
#define ADBM_A320_OFN_ORIENTATION_CTRL_ADDR  	0x77
#define ADBM_A320_OFN_ID                        0x83

#define OFN_GPIO_RESET                          31
static int  OFN_GPIO_SHUT_DOWN = 36;
static int  OFN_GPIO_INT = 37;
#define OPTNAV_STABILIZE_DELAY_MS               30
#define OFN_VREG_GP5_LEVEL                      2850
#define OFN_READ_DATA_INTERVAL_MS               30
#define OFN_DEVICE_DETECT_TIMES                  3

#define ARRAY_LEN								5
static struct workqueue_struct *avago_OFN_wq;
/* delete one line */
static int index = 2;
static struct i2c_client *u8500_client = NULL;

#define SHUTTER_VALUE		70
#define PIXEL_VALUE			225

struct avago_OFN_data {
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
static void avago_OFN_early_suspend(struct early_suspend *h);
static void avago_OFN_late_resume(struct early_suspend *h);
#endif

static int avago_OFN_power(struct i2c_client *client, int on);

static u8 avago_OFN_powerup(struct i2c_client *client )
{
	int ret;	

	ret = gpio_tlmm_config(GPIO_CFG(OFN_GPIO_SHUT_DOWN, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	ret = gpio_direction_output(OFN_GPIO_SHUT_DOWN, 0);

    mdelay(1);

	ret = gpio_tlmm_config(GPIO_CFG(OFN_GPIO_RESET, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	AVAGO_DEBUG("avago_OFN_powerup gpio_tlmm_config ret=%d \n",ret); 
    
	ret = gpio_direction_output(OFN_GPIO_RESET, 0);
	mdelay(1);
	ret = gpio_direction_output(OFN_GPIO_RESET, 1);
	mdelay(5);

	ret = i2c_smbus_read_byte_data(client, ADBM_A320_PRODUCTID_ADDR);
	AVAGO_DEBUG("avago_OFN_powerup id =0x%x \n",ret); 
/* delete some lines,we donot distinguish the production */
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_SOFTRESET_ADDR, 0x5A); /* enable	int */
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_ENGINE_ADDR, 0xb4);  //disable speed switch,enable XYQuantization
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_RESOLUTION_ADDR, 0x02); //500cpi
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_SPEED_CONTROL_ADDR, 0x03);  //set checking interval for 4ms
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_AD_CTRL_ADDR, 0xc5);
/* we modified 4 values here according to the results of our hardware test */
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_AD_ATH_HIGH_ADDR, 0x33); 
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_AD_DTH_HIGH_ADDR, 0x40); 
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_AD_ATH_LOW_ADDR, 0x33); 
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_AD_DTH_LOW_ADDR, 0x40);     
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_FPD_CTRL_ADDR, 0x50); 
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_QUANTIZE_CTRL_ADDR, 0x88); 
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_XYQ_THRESH_ADDR, 0x03);  //set slope for 45
	ret = i2c_smbus_write_byte_data(client, ADBM_A320_OFN_FPD_CTRL_ADDR, 0x50); 
	return ret;  
}

static void avago_OFN_work_func(struct work_struct *work)
{
	int ret;
	s8 deltax = 0;
	s8 deltay = 0;
	
	int report_x = 0;
	int report_y = 0;
	int ret_up = 0,ret_low = 0;
	int ret_pixelmax = 0,ret_pixelmin = 0;
    struct avago_OFN_data *ofn = container_of(work, struct avago_OFN_data, work);
	
	ret = i2c_smbus_read_byte_data(ofn->client, ADBM_A320_MOTION_ADDR);
	if (ret < 0)
	{
		printk(KERN_ERR"avago_OFN_work_func get motion addr failed \n");
		goto on_workf_exit;
	}

	if (!(ret&ADBM_A320_MOTION_MOT))
	{
		AVAGO_DEBUG(" data not ready \n");
		goto on_workf_exit;            
	}
/* read two registers to get shutter value,if shutter value less than 0x50,
 * we can consider the motion data as sunlight inerference.
 */
	ret_up = i2c_smbus_read_byte_data(ofn->client, ADBM_A320_SHUTTERUPPER_ADDR);
	ret_low = i2c_smbus_read_byte_data(ofn->client, ADBM_A320_SHUTTERLOWER_ADDR);
	ret = (ret_up << 8) | ret_low;
	AVAGO_DEBUG("shutter value = 0x%x\n",ret);
	
	if (ret < SHUTTER_VALUE)
	{
		AVAGO_DEBUG("shutter value below 0x50\n");
		goto on_workf_exit;
	}
/*  if finger off under sunlight but the intensity is not very strong,sometimes the
 *  shutter value will be 0x50-0xFF, which is similar to the shutter value of finger on; 
 *  There is the algorithm to check "Pixel_Max-Pixel_Min" .
 */
	ret_pixelmax =  i2c_smbus_read_byte_data(ofn->client, ADBM_A320_MAXIMUMPIXEL_ADDR);
	ret_pixelmin =  i2c_smbus_read_byte_data(ofn->client, ADBM_A320_MINIMUMPIXEL_ADDR);
	AVAGO_DEBUG("a320:pixelmax = %d,pixelmin = %d\n",ret_pixelmax,ret_pixelmin);
	ret = ret_pixelmax - ret_pixelmin;
	
	if (ret > PIXEL_VALUE)
	{
		AVAGO_DEBUG("a320:pixelmax - pixelmin > 225\n");
		goto on_workf_exit;
	}

	deltay = i2c_smbus_read_byte_data(ofn->client, ADBM_A320_DELTAX_ADDR);
	deltax = i2c_smbus_read_byte_data(ofn->client, ADBM_A320_DELTAY_ADDR);
	AVAGO_DEBUG("deltay:%d deltax:%d \n", deltay, deltax);	   

	if (deltay > 0)
		report_x = 1;
	else if (deltay < 0)
		report_x = -1;
	else
		report_x = 0;

	if (-deltax > 0)
		report_y = 1;
	else if (-deltax < 0)
		report_y = -1;
	else
		report_y = 0;
	if(machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
	{   
		report_x = -report_x;
		input_report_rel(ofn->input_dev, REL_X, report_y);
		input_report_rel(ofn->input_dev, REL_Y, report_x);
	}
	else
	{
		input_report_rel(ofn->input_dev, REL_X, report_x);
		input_report_rel(ofn->input_dev, REL_Y, report_y);
	}
	input_sync(ofn->input_dev);
/* delete some lines,we donot use time to reduce the avago speed now*/
	ret = i2c_smbus_write_byte_data(ofn->client, ADBM_A320_MOTION_ADDR, 0xFF); 
    
on_workf_exit:
	enable_irq(ofn->client->irq);
    return;
}
static enum hrtimer_restart avago_OFN_timer_func(struct hrtimer *timer)
{
	struct avago_OFN_data *ofn = container_of(timer, struct avago_OFN_data, timer);
	AVAGO_DEBUG("avago_OFN_timer_func\n");
	queue_work(avago_OFN_wq, &ofn->work);
	hrtimer_start(&ofn->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t avago_OFN_irq_handler(int irq, void *dev_id)
{
	struct avago_OFN_data *ofn = dev_id;
	disable_irq_nosync(ofn->client->irq);
	AVAGO_DEBUG("avago_OFN_irq_handler,disable irq\n");
	queue_work(avago_OFN_wq, &ofn->work);
	return IRQ_HANDLED;
}

static int ofn_open(struct inode *inode, struct file *file)
{			
	AVAGO_DEBUG("ofn_opens \n");
	return nonseekable_open(inode, file);
}


static int ofn_release(struct inode *inode, struct file *file)
{
	AVAGO_DEBUG("ofn_release \n");
	return 0;
}


static int
ofn_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	   unsigned long arg)
{
	int i;
	void __user *argp = (void __user *)arg;
	
	short flag;

	switch (cmd) 
	{
		case ECS_IOCTL_APP_SET_DELAY:
			if (copy_from_user(&flag, argp, sizeof(flag)))
				return -EFAULT;
			break;
		
		default:
			break;
	}
	switch (cmd) 
	{
		case ECS_IOCTL_APP_SET_DELAY:
			AVAGO_DEBUG("%s set index:%d \n",__FUNCTION__,flag);
			if((flag > 0) && (flag <= ARRAY_LEN) )
				index = flag-1;
			else
				index = 2;
			break;
			
		case ECS_IOCTL_APP_GET_DELAY:
			AVAGO_DEBUG("%s get index:%d \n",__FUNCTION__,index);
			flag = index+1;
			break;
			
		default:
			break;
	}
	switch (cmd) 
	{
		case ECS_IOCTL_APP_SET_DELAY:
/* delete some lines,we donot distinguish the production */
				if (index == 0)
				{
					i2c_smbus_write_byte_data(u8500_client, ADBM_A320_OFN_RESOLUTION_ADDR, 0x04);  //set cpi for 1000
				}	
				if (index == 1)
				{
					i2c_smbus_write_byte_data(u8500_client ,ADBM_A320_OFN_RESOLUTION_ADDR, 0x03);  //set cpi for 750
				}
				if (index == 2)
				{
					i2c_smbus_write_byte_data(u8500_client, ADBM_A320_OFN_RESOLUTION_ADDR, 0x02);  //set cpi for 500
				}	
				if (index == 3)
				{
					i2c_smbus_write_byte_data(u8500_client, ADBM_A320_OFN_RESOLUTION_ADDR, 0x01);  //set cpi for 250
				}
				if (index == 4)
				{
					i2c_smbus_write_byte_data(u8500_client, ADBM_A320_OFN_RESOLUTION_ADDR, 0x00);  //not set cpi
				}
			break;
		case ECS_IOCTL_APP_GET_DELAY:
			if (copy_to_user(argp, &flag, sizeof(flag)))
				return -EFAULT;
			break;
			
		default:
			break;
	}
	return 0;
}

static struct file_operations ofn_fops = {
	.owner = THIS_MODULE,
	.open = ofn_open,
	.release = ofn_release,
	.ioctl = ofn_ioctl,
};

static struct miscdevice ofn_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "ofn",
	.fops = &ofn_fops,
};

static int avago_OFN_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct avago_OFN_data *ofn;
	struct vreg *v_gp5;
	int ret = 0;
	int gpio_config, rc;
	int i;
    bool msm_ofn_support = false;

	AVAGO_DEBUG(KERN_ERR "In avago_OFN_probe: \n");
    board_support_ofn(&msm_ofn_support);
    if(false == msm_ofn_support)
    {
        /*this board don't support OFN, and don't need OFN key */
        return 0;
    }
    if(machine_is_msm7x25_u8500() && (HW_VER_SUB_VA == get_hw_sub_board_id()))
    {   
        OFN_GPIO_SHUT_DOWN = 37;
        OFN_GPIO_INT = 36;
        client->irq = MSM_GPIO_TO_INT(36);
    }

	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		AVAGO_DEBUG(KERN_ERR "avago_OFN_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	
	/* power on ofn */
    v_gp5 = vreg_get(NULL,"gp5");
    ret = IS_ERR(v_gp5);
    if(ret) 
        return ret;

    ret = vreg_set_level(v_gp5, OFN_VREG_GP5_LEVEL);
    if (ret)
        return ret;
    ret = vreg_enable(v_gp5);
    if (ret)
        return ret;
        
    msleep(10);

	avago_OFN_powerup(client);
/* delete some lines,um840 use this point */
 		u8500_client = client;

	/* driver  detect its device  */  
	for(i = 0; i < OFN_DEVICE_DETECT_TIMES; i++) {
		
		AVAGO_DEBUG("avago_OFN_powerup ok \n"); 
		ret = i2c_smbus_read_byte_data(client, 0x0);
		if (ret == ADBM_A320_OFN_ID){
			printk(KERN_ERR "avago_OFN_probe manufacturer success id=0x%x\n", ret); 
			goto succeed_find_device;
		}
	}
	if ( i == OFN_DEVICE_DETECT_TIMES) {

        ret = vreg_disable(v_gp5);
		printk(KERN_ERR "no avago_OFN_probe device,vreg_disable: gp5 = %d \n ", ret);	
		goto err_find_touchpanel_failed;
	}

succeed_find_device:
	avago_OFN_wq = create_singlethread_workqueue("avago_OFN_wq");
	if (!avago_OFN_wq) {
		AVAGO_DEBUG("create avago_OFN_wq error\n");
		return -ENOMEM;
	}
	ofn = kzalloc(sizeof(*ofn), GFP_KERNEL);
	if (ofn == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
		AVAGO_DEBUG("no avago_OFN_probe   22 \n ");	

	ofn->client = client;
	i2c_set_clientdata(client, ofn);
	INIT_WORK(&ofn->work, avago_OFN_work_func);

	ofn->power = avago_OFN_power;
	if (ofn->power) {
		ret = ofn->power(ofn->client, 1);
		if (ret < 0) {
			AVAGO_DEBUG(KERN_ERR "avago_OFN_probe reset failed\n");
			goto err_power_failed;
		}
	}
    
	ofn->input_dev = input_allocate_device();
	if (ofn->input_dev == NULL) {
		ret = -ENOMEM;
		AVAGO_DEBUG(KERN_ERR "avago_OFN_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ofn->input_dev->name = "avago-OFN";
	
	input_set_capability(ofn->input_dev, EV_KEY, BTN_MOUSE);
    input_set_capability(ofn->input_dev, EV_REL, REL_X);
    input_set_capability(ofn->input_dev, EV_REL, REL_Y);
	
	ret = input_register_device(ofn->input_dev);
	if (ret) {
		AVAGO_DEBUG(KERN_ERR "avago_OFN_probe: Unable to register %s input device\n", ofn->input_dev->name);
		goto err_input_register_device_failed;
	}
	
	ret = misc_register(&ofn_device);
	if (ret) {
		AVAGO_DEBUG(KERN_ERR "avago_OFN_probe: Unable to register miscdevice\n");
		goto err_misc_device_register_failed;
	}
	gpio_config = GPIO_CFG(OFN_GPIO_INT, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
	rc = gpio_tlmm_config(gpio_config, GPIO_ENABLE);
	AVAGO_DEBUG(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n", __func__, 37, rc);
	if (rc) 
		return -EIO;
	if (gpio_request(OFN_GPIO_INT, "avago_OFN_int\n"))
		pr_err("failed to request gpio avago_OFN_int\n");
	
	ret = gpio_configure(OFN_GPIO_INT, GPIOF_INPUT | IRQF_TRIGGER_LOW);/*gpio 20is interupt for touchscreen.*/
	if (ret) {
		AVAGO_DEBUG(KERN_ERR "avago_OFN_probe: gpio_configure 20 failed\n");
		goto err_input_register_device_failed;
	}

	if (client->irq) {
		
		AVAGO_DEBUG(KERN_ERR "client->irq   \n");
		ret = request_irq(client->irq, avago_OFN_irq_handler, 0, client->name, ofn);
		if (ret != 0) {
			free_irq(client->irq, ofn);
			AVAGO_DEBUG("avago_OFN_probe: enable abs int failed");
		}
		if (ret == 0)
			ofn->use_irq = 1;
		else
			dev_err(&client->dev, "avago_OFN_probe: request_irq failed\n");
	}
	if (!ofn->use_irq) {
		hrtimer_init(&ofn->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ofn->timer.function = avago_OFN_timer_func;
		hrtimer_start(&ofn->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ofn->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 1;
	ofn->early_suspend.suspend = avago_OFN_early_suspend;
	ofn->early_suspend.resume = avago_OFN_late_resume;
	register_early_suspend(&ofn->early_suspend);
#endif

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_OFN);
    #endif
    
	AVAGO_DEBUG(KERN_INFO "avago_OFN_probe: Start touchscreen %s in %s mode\n", ofn->input_dev->name, ofn->use_irq ? "interrupt" : "polling");

	return 0;
err_misc_device_register_failed:
err_input_register_device_failed:
	input_free_device(ofn->input_dev);

err_input_dev_alloc_failed:
err_power_failed:
	kfree(ofn);
err_alloc_data_failed:
err_find_touchpanel_failed:
err_check_functionality_failed:
	return ret;
}

static int avago_OFN_power(struct i2c_client *client, int on)
{
    int ret = 0;

    ret = gpio_tlmm_config(GPIO_CFG(OFN_GPIO_SHUT_DOWN, 0, GPIO_OUTPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
    if(ret < 0)
    {
       ret = -EIO;
       pr_err("failed to config gpio OFN_GPIO_SHUT_DOWN\n");
       goto power_fail;
    }
    
    if(on)
    {
        ret = gpio_direction_output(OFN_GPIO_SHUT_DOWN, 0);
        AVAGO_DEBUG(KERN_ERR "avago_OFN_power disable shutdown\n");

        /*you must read I2c after 100ms */
    }
    else
    {
        ret = gpio_direction_output(OFN_GPIO_SHUT_DOWN, 1);
        AVAGO_DEBUG(KERN_ERR "avago_OFN_power enable shutdown\n");
    }

    return 0;
    
power_fail:
    return ret;
}

static int avago_OFN_remove(struct i2c_client *client)
{
	struct avago_OFN_data *ofn = i2c_get_clientdata(client);
	unregister_early_suspend(&ofn->early_suspend);
	if (ofn->use_irq)
		free_irq(client->irq, ofn);
	else
		hrtimer_cancel(&ofn->timer);
	input_unregister_device(ofn->input_dev);
	kfree(ofn);
	return 0;
}

static int avago_OFN_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret=1;
	struct avago_OFN_data *ofn = i2c_get_clientdata(client);
	AVAGO_DEBUG("In avago_OFN_suspend\n");
	if (ofn->use_irq)
		disable_irq(client->irq);
	else
		hrtimer_cancel(&ofn->timer);
	ret = cancel_work_sync(&ofn->work);
	if (ret && ofn->use_irq) /* if work was pending disable-count is now 2 */
		enable_irq(client->irq);
	
	if (ofn->power) {
		ret = ofn->power(client,0);
		if (ret < 0)
			AVAGO_DEBUG(KERN_ERR "avago_OFN_suspend power off failed\n");
	}
	return ret;
}

static int avago_OFN_resume(struct i2c_client *client)
{
	int ret=1;
	struct avago_OFN_data *ofn = i2c_get_clientdata(client);

	AVAGO_DEBUG("In avago_OFN_resume\n");
	if (ofn->power) {
		ret = ofn->power(client, 1);
		if (ret < 0)
			AVAGO_DEBUG(KERN_ERR "avago_OFN_resume power on failed\n");
	}	

	/*you must read I2c after 100ms */
	msleep(100);
	
	if (ofn->use_irq) {
		enable_irq(client->irq);
	}
	else
		hrtimer_start(&ofn->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void avago_OFN_early_suspend(struct early_suspend *h)
{
	struct avago_OFN_data *ts;
	ts = container_of(h, struct avago_OFN_data, early_suspend);
	avago_OFN_suspend(ts->client, PMSG_SUSPEND);
}

static void avago_OFN_late_resume(struct early_suspend *h)
{
	struct avago_OFN_data *ofn;
	ofn = container_of(h, struct avago_OFN_data, early_suspend);
	avago_OFN_resume(ofn->client);
}
#endif

static const struct i2c_device_id avago_OFN_id[] = {
	{ AVAGO_OFN_NAME, 0 },
	{ }
};

static struct i2c_driver avago_OFN_driver = {
	.probe		= avago_OFN_probe,
	.remove		= avago_OFN_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= avago_OFN_suspend,
	.resume		= avago_OFN_resume,
#endif
	.id_table	= avago_OFN_id,
	.driver = {
		.name	= AVAGO_OFN_NAME,
	},
};

static int __devinit avago_OFN_init(void)
{
	return i2c_add_driver(&avago_OFN_driver);
}

static void __exit avago_OFN_exit(void)
{
	i2c_del_driver(&avago_OFN_driver);
	if (avago_OFN_wq)
		destroy_workqueue(avago_OFN_wq);
}

module_init(avago_OFN_init);
module_exit(avago_OFN_exit);

MODULE_DESCRIPTION("Avago OFN Driver");
MODULE_LICENSE("GPL");
