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


#include <linux/gs_st.h>
#include "linux/hardware_self_adapt.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#define GS_POLLING   1

//#define gs_swap(x, y) do { typeof(x) z = x; x = y; y = z; } while (0)

static struct workqueue_struct *gs_wq;

struct input_dev *sensor_dev = NULL;

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
		printk(KERN_ERR "i2c_smbus_read_byte_data failed\n");

	mutex_unlock(&gs->mlock);

	return val;
}
static inline int reg_write(struct gs_data *gs, int reg, uint8_t val)
{
	int ret;

	mutex_lock(&gs->mlock);
	ret = i2c_smbus_write_byte_data(gs->client, reg, val);
	if(ret < 0) {
		printk(KERN_ERR "i2c_smbus_write_byte_data failed\n");
	}
	mutex_unlock(&gs->mlock);

	return ret;
}




/**************************************************************************************/



#define MG_PER_SAMPLE   720                       /*HAL: 720=1g*/                       
#define FILTER_SAMPLE_NUMBER  256           /*256LSB =1g*/  


static int sensor_data[4];
/*adjust device name */
static char st_device_id[] = "st_35de";

int gs_st_data_to_compass(int accel_data [3])
{
	memset(accel_data, 0, 3 );
	accel_data[0]=sensor_data[0]/2;
	accel_data[1]=sensor_data[1]/2;
	accel_data[2]=sensor_data[2]/2;
	return 0;

}

/**************************************************************************************/

static int gs_st_open(struct inode *inode, struct file *file)
{			
       reg_read(this_gs_data, GS_ST_REG_STATUS ); /* read status */
	
	reg_write(this_gs_data, GS_ST_REG_CTRL1, GS_ST_CTRL1_PD|
								          GS_ST_CTRL1_Zen|
		                                                  GS_ST_CTRL1_Yen|
		                                                  GS_ST_CTRL1_Xen); 
	
	reg_write(this_gs_data, GS_ST_REG_CTRL3, GS_INTMODE_DATA_READY); 
	reg_read(this_gs_data, GS_ST_REG_OUT_X ); /* read X */
	reg_read(this_gs_data, GS_ST_REG_OUT_Y ); /* read Y */
	reg_read(this_gs_data, GS_ST_REG_OUT_Z ); /* read Z*/

	if (this_gs_data->use_irq)
		enable_irq(this_gs_data->client->irq);
	else
		 hrtimer_start(&this_gs_data->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	
		
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
				gs_st_data_to_compass(accel_buf);
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
			if (copy_to_user(argp, st_device_id, sizeof(st_device_id)))
				return -EFAULT;
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
	int x,y,z;
	struct gs_data *gs = container_of(work, struct gs_data, work);
	int	sesc = accel_delay/1000;
	int nsesc = (accel_delay%1000)*1000000;

       
	 status = reg_read(gs, GS_ST_REG_STATUS ); /* read status */
	
	if(status & (1<<3))
	{
		 u8 u8x = reg_read(gs,GS_ST_REG_OUT_X);
		
		 u8  u8y=  reg_read(gs,GS_ST_REG_OUT_Y);
		
		 u8 u8z = reg_read(gs,GS_ST_REG_OUT_Z);
		 
		

#if 0

		x = MG_PER_SAMPLE *(s8)u8y  ;		

		y = MG_PER_SAMPLE * (s8)u8x ;			
		z = MG_PER_SAMPLE * (s8)u8z;
#endif	
		
		if(u8x&0x80)/*负值*/
		{
			x= u8x-256; 		/*负数按照补码计算 */  
		}
		else
		{
			x = u8x;
		}
					
		if(u8y&0x80)/*负值*/
		{
			 y=u8y-256; 		/*负数按照补码计算 */  	 
		}
		else
		{
			y = u8y;
		}
				
		if(u8z&0x80)/*负值*/
		{
			 z=u8z-256; 		/*负数按照补码计算 */   
		}
		else
		{
			  z = u8z;
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

		memset(sensor_data, 0, 3 );
		sensor_data[0]= 46*(s16)x/10;
		sensor_data[1]= 46*(s16)y/10 ;	
		sensor_data[2]= 46*(s16)z/10;
		/*(Decimal value/ 256) * 4.6 g,For (0g ~+2.3g)*/	
		x = (MG_PER_SAMPLE*46*(s16)x)/FILTER_SAMPLE_NUMBER/10;           
		y = (MG_PER_SAMPLE*46*(s16)y)/FILTER_SAMPLE_NUMBER/10;
		z = (MG_PER_SAMPLE*46*(s16)z)/FILTER_SAMPLE_NUMBER/10;
		x *=(-1);		

		input_report_abs(gs->input_dev, ABS_X, x);			
		input_report_abs(gs->input_dev, ABS_Y, y);			
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
	struct gs_platform_data *pdata;
    
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "gs_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	if(machine_is_msm7x25_c8150() && (get_hw_sub_board_id() == HW_VER_SUB_VG))
	{
		printk(KERN_ERR "gs_probe: M835 do not support ST LIS35DE g-sensor!!\n");
		return -ENODEV;
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
				printk(KERN_ERR "gs_st probe failed, because the othe gsensor has been probed.\n");
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

#if 0
	
    ret = i2c_smbus_write_byte_data(gs->client, 0x20, 0x47); /* device command = ctrl_reg1 */
	if (ret < 0) {
		printk(KERN_ERR "i2c_smbus_write_byte_data failed\n");
		/* fail? */
		goto err_detect_failed;
	}
#endif	


	ret = reg_write(gs, GS_ST_REG_CTRL2, 0x00); /* device command = ctrl_reg2 */
	if (ret < 0) {
		printk(KERN_ERR "i2c_smbus_write_byte_data failed\n");
		/* fail? */
		goto err_detect_failed;
	}

#if 0

	ret = i2c_smbus_write_byte_data(gs->client, 0x22, 0x04); /* device command = ctrl_reg3 */
	if (ret < 0) {
		printk(KERN_ERR "i2c_smbus_write_byte_data failed\n");
		/* fail? */
		goto err_detect_failed;
	}
#endif

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
	
	gs->input_dev->id.vendor = GS_ST35DE;//for  akm8973 compass detect.

	set_bit(EV_ABS,gs->input_dev->evbit);
	
	set_bit(ABS_X, gs->input_dev->absbit);
	set_bit(ABS_Y, gs->input_dev->absbit);
	set_bit(ABS_Z, gs->input_dev->absbit);
	
	set_bit(EV_SYN,gs->input_dev->evbit);


	gs->input_dev->id.bustype = BUS_I2C;
	//gs->input_dev->open = gs_st_input_open;
	//gs->input_dev->close = gs_st_input_close;
	
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

	if(pdata && pdata->init_flag)
		*(pdata->init_flag) = 1;
	printk(KERN_INFO "gs_probe: Start LIS35DE  in %s mode\n", gs->use_irq ? "interrupt" : "polling");
    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_G_SENSOR);
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

	reg_write(gs, GS_ST_REG_CTRL3, 0); /* disable interrupt */
	
	reg_write(gs, GS_ST_REG_CTRL1, 0x00); /* deep sleep */

	if (gs->power) {
		ret = gs->power(0);
		if (ret < 0)
			printk(KERN_ERR "gs_resume power off failed\n");
	}
	return 0;
}

static int gs_resume(struct i2c_client *client)
{
	struct gs_data *gs = i2c_get_clientdata(client);
	
	reg_write(gs, GS_ST_REG_CTRL1, GS_ST_CTRL1_PD|
						GS_ST_CTRL1_Zen|
						GS_ST_CTRL1_Yen|
						GS_ST_CTRL1_Xen); /* enable abs int */
	
       reg_write(gs, GS_ST_REG_CTRL3, GS_INTMODE_DATA_READY);/*active mode*/
    
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
	{ /*GS_I2C_NAME*/"gs_st", 0 },
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
		.name	="gs_st",
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

MODULE_DESCRIPTION("accessor  Driver");
MODULE_LICENSE("GPL");
