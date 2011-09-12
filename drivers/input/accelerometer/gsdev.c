/* drivers/input/gsdev.c
 *
 * 
 *created by zlp 2009-2-18 11:10
 * 
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

#include <linux/gs_adxl345.h>

#include <linux/gs_st.h>
#include "linux/hardware_self_adapt.h"


#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

typedef  unsigned char      boolean;  
typedef u16 uint16;

static struct workqueue_struct *gs_wq;

extern struct input_dev *sensor_dev;

struct gs_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	
	struct mutex  mlock;
	
	
	struct hrtimer timer;
	struct work_struct  work;	
	uint32_t flags;
	int (*power)(int on);
	struct early_suspend early_suspend;
};

static struct gs_data  *this_gs_data;


static int accel_delay = GS_ADI_TIMRER;     



static atomic_t a_flag;

#define ID_ADXL345 	0xE5


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
		printk(KERN_ERR "ADXL345 chip: i2c_smbus_read_byte_data failed %x\n",reg);

	mutex_unlock(&gs->mlock);

	return val;
}

static inline int reg_write(struct gs_data *gs, int reg, uint8_t val)
{
	int ret;

	mutex_lock(&gs->mlock);
	ret = i2c_smbus_write_byte_data(gs->client, reg, val);
	if(ret < 0) {
		printk(KERN_ERR "ADXL345 chip: i2c_smbus_write_byte_data failed %x\n",reg);
	}
	mutex_unlock(&gs->mlock);

	return ret;
}

/*adjust device name */
static char adix_device_id[] = "gs_adix345";

/**************************************************************************************/

#define GS_POLLING    1

#define MG_PER_SAMPLE   720                      /*HAL: 720=1g*/      
#define FILTER_SAMPLE_NUMBER   1024          /*128=1g*/
volatile int GS_SENSOR_ADI_FLAG;       
static int sensor_data[3];
/**************************************************************************************/
int gs_adi_data_to_compass(int accel_data [3])
{
	memset((void*)accel_data, 0, sizeof(accel_data));
	accel_data[0]=sensor_data[0];
	accel_data[1]=sensor_data[1];
	accel_data[2]=sensor_data[2];
	return 0;

}

/**************************************************************************************/




static int gs_adi_open(struct inode *inode, struct file *file)
{			

	reg_write(this_gs_data,GS_ADI_REG_POWER_CTL,0x08);/* measure mode */
	reg_write(this_gs_data,GS_ADI_REG_INT_ENABLE,0x80);  /* enable  int Int En: Data Rdy */
	reg_read(this_gs_data, GS_ADI_REG_INT_SOURCE); /* read IRQ STATUS */
	if (this_gs_data->use_irq)
		enable_irq(this_gs_data->client->irq);
	else
		hrtimer_start(&this_gs_data->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	return nonseekable_open(inode, file);
	
}

static int gs_adi_release(struct inode *inode, struct file *file)
{
	reg_write(this_gs_data,GS_ADI_REG_INT_ENABLE,0x00) ; /*disable int*/
	reg_write(this_gs_data,GS_ADI_REG_POWER_CTL,0x00);   /* power down mode */
	if (this_gs_data->use_irq)
	disable_irq(this_gs_data->client->irq);
	else
		hrtimer_cancel(&this_gs_data->timer);

	accel_delay = GS_ADI_TIMRER;	 /*1s*/


	return 0;
}

static int
gs_adi_ioctl(struct inode *inode, struct file *file, unsigned int cmd,
	   unsigned long arg)
{

	int i;
	void __user *argp = (void __user *)arg;
	int accel_buf[3];
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
			for(i=0;i<3;i++)
				gs_adi_data_to_compass(accel_buf);
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
			if (copy_to_user(argp, adix_device_id, sizeof(adix_device_id)))
				return -EFAULT;
			break;
		default:
			break;
	}
	
	return 0;
}

static struct file_operations gs_adi_fops = {
	.owner = THIS_MODULE,
	.open = gs_adi_open,
	.release = gs_adi_release,
	.ioctl = gs_adi_ioctl,
};

static struct miscdevice gsensor_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "accel",
	.fops = &gs_adi_fops,
};

/**************************************************************************************/



#if 0
/**************************************************************************
ADI345X#8451849PHIL of PP1 phone  need set  offset, but ADI345B can't set offset value, 
if set the value will work MMITest fail
***************************************************************************/


static int gs_set_offset( void  )
{	
	int ret;
	ret = reg_write(this_gs_data,GS_ADI_REG_OFSX,0x03);   /* set  OFSX offset value*/
	if (ret < 0) {
		printk(KERN_DEBUG "reg_write  GS_ADI_REG_OFSX failed\n");
	}
	ret = reg_write(this_gs_data,GS_ADI_REG_OFSY,0x03);	 /* set OFSY offset value*/
	if (ret < 0) {
		printk(KERN_DEBUG "reg_write  GS_ADI_REG_OFSY failed\n");
	}

	ret = reg_write(this_gs_data,GS_ADI_REG_OFSZ,0x0B);	/* set OFSZ offset value*/

	if (ret < 0) {
		printk(KERN_DEBUG "reg_write GS_ADI_REG_OFSZ  failed\n");
	}
	return ret;
		
}

#endif


/********************************************************************/

static void gs_work_func(struct work_struct *work)
{
	
	int ret,i;
	struct gs_data *gs = container_of(work, struct gs_data, work);
	unsigned char dataX_low = 0;
	unsigned char dataX_high = 0;
	uint16        dataX = 0;
	unsigned char dataY_low = 0;
	unsigned char dataY_high = 0;
	uint16        dataY = 0;
	unsigned char dataZ_low = 0;
	unsigned char dataZ_high = 0;
	uint16        dataZ = 0;
	boolean data_is_err = 0;
	int x = 0, y = 0, z = 0;

	uint8_t buf[6];
	uint8_t start_reg;
	struct i2c_msg msg[2];
	int	sesc = accel_delay/1000;
	int nsesc = (accel_delay%1000)*1000000;
	msg[0].addr = gs->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = 0x32;

	msg[1].addr = gs->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;
#if 0
/**************************************************************************
ADI345X#8451849PHIL of PP1 phone  need set  offset, but ADI345B can't set offset value, 
if set the value will work MMITest fail
***************************************************************************/
	 ret = gs_set_offset();
	 if (ret < 0) {
		printk(KERN_ERR "gs_set_offset faild\n");
		/* fail? */
		goto restart_timer;
	}
#endif	 

	 ret =  reg_read(gs, GS_ADI_REG_INT_SOURCE); /* read IRQ STATUS */
	
	if (ret < 0) {
		printk(KERN_ERR "i2c_smbus_read_int status_0X30 failed\n");
		/* fail? */	
		goto restart_timer;
		
	}
	
	if((ret&0x20))//double tap
	{
//		printk(KERN_DEBUG"double tap\n");
		
	}	
	
	else if((ret&0x40))//sigle tap
	{
//		printk(KERN_DEBUG"sigle tap\n");
	}	
	
	else if((ret&0x80))
	{
	//printk(KERN_DEBUG"data ready\n");
		
	    for (i = 0; i < 10; i++) 
	    {
			ret = i2c_transfer(gs->client->adapter, msg, 2);
			if (ret < 0) 
			{
				printk(KERN_ERR "gs_ts_work_func: i2c_transfer failed\n");
				data_is_err = 1;
			}
			else
			{
				data_is_err = 0;
				break;
			}	
					 
		}
		
	
	}
	else
	{
		printk(KERN_ERR"data err\n");
		data_is_err = 1;
		
	} 
	dataX_low = msg[1].buf[0];
	dataX_high = msg[1].buf[1];
	dataY_low = msg[1].buf[2];
	dataY_high = msg[1].buf[3];
	dataZ_low  = msg[1].buf[4];
	dataZ_high = msg[1].buf[5];
	 

	if(!data_is_err)	
	{
	    dataX = ((dataX_high&0x1f) <<8) |dataX_low;
	    dataY = ((dataY_high&0x1f) <<8) |dataY_low;
	    dataZ = ((dataZ_high&0x1f) <<8) |dataZ_low;
		
	    dataX &= 0x3FF;  /*10 bit is sign bit, */
	    dataY &= 0x3FF;
	    dataZ &= 0x3FF;
		
		if(dataX&0x200)/*负值*/
		{
			 x=dataX -1024;         /*10 bit is sign bit, */  
			 
		}
		else
		{
			x = dataX;
		}
			
		if(dataY&0x200)/*负值*/
		{
			y=dataY - 1024;   /*10 bit is sign bit, */   
		}
		else
		{
			y = dataY;
		}
		
		if(dataZ&0x200)/*负值*/
		{
			z =dataZ - 1024;   /*10 bit is sign bit, */   
		}
		else
		{
			z = dataZ;
		}

		/* change the x,y,z for u8300 because orientation of accelerometer of u8300 is different.*/
		if(machine_is_msm7x25_u8300()) 
		{
			int x1,y1,z1;
			x1 = y * (-1);
			y1 = x * (-1);
			z1 = z * (-1);
			
			x = x1;
			y = y1;
			z = z1;
		}
		
		memset((void*)sensor_data, 0, sizeof(sensor_data));
		sensor_data[0] = (s16)x;
		sensor_data[1] = (s16)y;		
		sensor_data[2] = (s16)z;
		x = (MG_PER_SAMPLE*80*(s16)x)/FILTER_SAMPLE_NUMBER/10;	
		y = (MG_PER_SAMPLE*80*(s16)y)/FILTER_SAMPLE_NUMBER/10;
		z = (MG_PER_SAMPLE*80*(s16)z)/FILTER_SAMPLE_NUMBER/10;
		x *=(-1);
		input_report_abs(gs->input_dev, ABS_X, x);			
		input_report_abs(gs->input_dev, ABS_Y, y);			
		input_report_abs(gs->input_dev, ABS_Z, z);
		input_sync(gs->input_dev);
	}
	restart_timer:
	
	if (gs->use_irq)
		enable_irq(gs->client->irq);
	else
		hrtimer_start(&gs->timer, ktime_set(sesc, nsesc), HRTIMER_MODE_REL);
}

static enum hrtimer_restart gs_timer_func(struct hrtimer *timer)
{
	struct gs_data *gs = container_of(timer, struct gs_data, timer);		
	queue_work(gs_wq, &gs->work);
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

static int gs_config_int1_pin(void)
{
	int err;
	err = gpio_request(GPIO_INT1, "gpio_gs_int1");
	if (err) 
	{
	printk(KERN_ERR "gpio_request failed for gs int1\n");
	return -1;
	}	

	err = gpio_configure(GPIO_INT1, GPIOF_INPUT | IRQF_TRIGGER_HIGH );
	if (err) 
		{
			gpio_free(GPIO_INT1);
	printk(KERN_ERR "gpio_config failed for gs int1 HIGH\n");
	   return -1;
	}     

	return 0;
}

static void gs_free_int1(void)
{
	gpio_free(GPIO_INT1);
}

static int gs_config_int2_pin(void)
{
	int err;
     err = gpio_request(GPIO_INT2, "gpio_gs_cs");
     if (err) 
     {
		printk(KERN_ERR "gpio_request failed for gs int2\n");
		return -1;
	}	
     
     err = gpio_configure(GPIO_INT2, GPIOF_INPUT | IRQF_TRIGGER_RISING);
     if (err) 
     	{
     		gpio_free(GPIO_INT2);
		printk(KERN_ERR "gpio_config failed for gs int2 HIGH\n");
	       return -1;
	}     
	
	return 0;
}

static void gs_free_int2(void)
{
	gpio_free(GPIO_INT2);
}
#endif

static int gs_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{	
	int ret;
	struct gs_data *gs;

     	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "gs_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
#ifndef GS_POLLING
	ret = gs_config_int1_pin();
	if(ret <0)
	{
		goto err_check_functionality_failed;
	}
	
	ret = gs_config_int2_pin();
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
	

	ret =reg_read(gs,GS_ADI_REG_DEVID);
	printk(KERN_INFO "read adi345 reg[%d] = 0x%x \n",  GS_ADI_REG_DEVID, ret);
	if (ret < 0){
		goto err_detect_failed;
	}

	switch (ret) {
	case ID_ADXL345:
		break;
	default:
		printk(KERN_ERR, "ADXL345 Failed to probe \n" );
		goto err_detect_failed;
	}
	 
	ret = reg_write(gs,GS_ADI_REG_POWER_CTL,0x14);   /* auto low power ,deep sleep */
	if ( ret <0 )
		goto err_detect_failed;
	ret = reg_write(gs,GS_ADI_REG_BW,0x0a);    /* Rate: 100Hz, IDD: 130uA */
	if ( ret <0 )
		goto err_detect_failed;
	ret = reg_write(gs,GS_ADI_REG_DATA_FORMAT,0x01);  /* Data Format: +-4g right justified  	128=1g  8g*/
	if ( ret <0 )
		 goto err_detect_failed;

	ret = reg_write(gs,GS_ADI_REG_INT_ENABLE,0x00); /* disable  int Int Dis: Data Rdy*/
	if ( ret <0 )
		goto err_detect_failed;

	ret = reg_write(gs,GS_ADI_REG_TAP_AXES,0x01);	/* Z Axis Tap */
	if ( ret <0 )
		goto err_detect_failed;
	ret = reg_write(gs,GS_ADI_REG_THRESH_TAP,0x20);	/* Tap Threshold: 2G */
	if ( ret <0 )
		goto err_detect_failed;

	ret = reg_write(gs,GS_ADI_REG_DUR,0x50);	/* Dur:50ms */
	if ( ret <0 )
		 goto err_detect_failed;

	ret = reg_write(gs,GS_ADI_REG_LATENT,0x20);	 /* Latent: 40ms */
	if ( ret <0 )
		goto err_detect_failed;


	ret = reg_write(gs,GS_ADI_REG_WINDOW,0xF0);	 /* Window: 300ms */
	if ( ret <0 )
		goto err_detect_failed;
	 
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
	gs->input_dev->id.vendor = GS_ADIX345;//for akm8973 compass detect.

	set_bit(EV_ABS,gs->input_dev->evbit);
	
	set_bit(ABS_X, gs->input_dev->absbit);
	set_bit(ABS_Y, gs->input_dev->absbit);
	set_bit(ABS_Z, gs->input_dev->absbit);
	
	set_bit(EV_SYN,gs->input_dev->evbit);


	gs->input_dev->id.bustype = BUS_I2C;
	//gs->input_dev->open = gs_adi_input_open;
	//gs->input_dev->close = gs_adi_input_close;
	
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
	
#if 0
	else
		GS_SENSOR_ADI_FLAG =1;
#endif
      this_gs_data =gs;


    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_G_SENSOR);
    #endif

	printk(KERN_INFO "gs_probe: Start gs_adixl345  in %s mode\n", gs->use_irq ? "interrupt" : "polling");

	return 0;
	
err_misc_device_register_failed:
	
	misc_deregister(&gsensor_device);

err_input_register_device_failed:
	input_free_device(gs->input_dev);

err_input_dev_alloc_failed:


err_detect_failed:

	kfree(gs);
	
#ifndef   GS_POLLING 
	gs_free_int1();
	gs_free_int2();
#endif	
	
err_alloc_data_failed:
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
	input_unregister_device(gs->input_dev);

	misc_deregister(&gsensor_device);

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
	if (ret && gs->use_irq) /* if work was pending disable-count is now 2 */
		enable_irq(client->irq);
	
	reg_write(gs,GS_ADI_REG_INT_ENABLE,0x00) ;  /*disable int*/
	reg_write(gs,GS_ADI_REG_POWER_CTL,0x00);   /* power down mode */
	return 0;
}

static int gs_resume(struct i2c_client *client)
{
	
	struct gs_data *gs = i2c_get_clientdata(client);
	
	reg_write(gs,GS_ADI_REG_POWER_CTL,0x08);/* measure mode */
	reg_write(gs, GS_ADI_REG_INT_ENABLE, 0x80);
	    
	
	if (gs->use_irq)
	{
		reg_write(gs,GS_ADI_REG_INT_ENABLE,0xe0);/* enable	int */

		enable_irq(client->irq);
	}
	if (!gs->use_irq)
		hrtimer_start(&gs->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

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
	{ /*GS_I2C_NAME*/"GS", 0 },
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
		.name	=/*GS_I2C_NAME*/"GS",
	},
};

static int __devinit gs1_init(void)
{
	return i2c_add_driver(&gs_driver);
}

static void __exit gs1_exit(void)
{
	i2c_del_driver(&gs_driver);
	if (gs_wq)
		destroy_workqueue(gs_wq);
}

module_init(gs1_init);
module_exit(gs1_exit);

MODULE_DESCRIPTION("accessor  Driver");
MODULE_LICENSE("GPL");
