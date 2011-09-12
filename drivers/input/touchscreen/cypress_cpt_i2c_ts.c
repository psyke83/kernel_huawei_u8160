/* drivers/input/touchscreen/cypress_cpt_i2c_ts.c
 *
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
#include <linux/kernel.h>
#include <mach/gpio.h>
#include <mach/mpp.h>

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/namei.h>
#include <mach/vreg.h>

#ifdef CONFIG_UPDATE_TS_FIRMWARE 
static ssize_t update_firmware_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf);
static ssize_t update_firmware_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);

static int ts_firmware_file(void);
static int i2c_update_firmware(void); 
static int decode_cmd(char *start, char *end);

static struct kobj_attribute update_firmware_attribute = {
	.attr = {.name = "update_firmware", .mode = 0666},
	.show = update_firmware_show,
	.store = update_firmware_store,
};
#endif
typedef  unsigned char boolean;  
static struct workqueue_struct *cypress_cpt_wq;

struct cypress_cpt_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	struct hrtimer timer;
	struct work_struct  work;
	struct early_suspend early_suspend;
};
#ifdef CONFIG_UPDATE_TS_FIRMWARE 
    static struct i2c_client *g_client = NULL;
    static struct cypress_cpt_ts_data *ts = NULL;
#endif

typedef enum {
	NO_GESTURE = 0,
	SINGLE_TOUCH_PAN_UP,
	SINGLE_TOUCH_PAN_RIGHT,
	SINGLE_TOUCH_PAN_DOWN,
	SINGLE_TOUCH_PAN_LEFT,
	SINGLE_TOUCH_ROTATE_CW,
	SINGLE_TOUCH_ROTATE_CCW,
	SINGLE_TOUCH_CLICK,
	MULTI_TOUCH_PAN_UP,
	MULTI_TOUCH_PAN_RIGHT,
	MULTI_TOUCH_PAN_DOWN,
	MULTI_TOUCH_PAN_LEFT,
	ZOOM_IN,
	ZOOM_OUT,
	SINGLE_TOUCH_DOUBLE_CLICK,
	NUM_OF_GESTURE
} GESTURE_TYPE;

#define TRUE	 1												/* Boolean true value.	 */
#define FALSE  0												/* Boolean false value.  */

#ifdef CONFIG_CYPRESS_TOUCHSCREEN_MULTIPOINT		
#define I2C_READ_LEN 9
#else
#define I2C_READ_LEN 5
#endif


#ifdef TS_DEBUG
#define CYPRESS_DEBUG(fmt, args...) printk(fmt, ##args)
#else
#define CYPRESS_DEBUG(fmt, args...)
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_cpt_ts_early_suspend(struct early_suspend *h);
static void cypress_cpt_ts_late_resume(struct early_suspend *h);
#endif
#define TS_X_OFFSET  3
#define TS_Y_OFFSET  TS_X_OFFSET

static int cypress_ts_power_on(struct cypress_cpt_ts_data *ts, boolean on);

static void cypress_cpt_ts_work_func(struct work_struct *work)
{
	int i;
	int ret;
	int bad_data = 0;	
	struct i2c_msg msg[2];
	uint8_t start_reg;
	static uint16_t last_x = -1; 
	static uint16_t last_y = -1;
	static bool is_first_point = TRUE;

	uint8_t buf[I2C_READ_LEN];
	uint16_t position[2][2];
	uint8_t finger;

	struct cypress_cpt_ts_data *ts = container_of(work, struct cypress_cpt_ts_data, work);

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	start_reg = 0x00;
	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;
	

	for (i = 0; i < ((ts->use_irq && !bad_data)? 1 : 5 ); i++) {
		ret = i2c_transfer(ts->client->adapter, msg, 2);
		if (ret < 0) {
			CYPRESS_DEBUG("%d times i2c_transfer failed\n",i);
			bad_data = 1;
			continue;
		}
		if (i == 5) {
			CYPRESS_DEBUG("five times i2c_transfer error\n");
			cypress_ts_power_on(ts, 1);
			goto out;
		}
		bad_data = 0;
                finger = buf[0];
#ifdef CONFIG_CYPRESS_TOUCHSCREEN_MULTIPOINT		
		if (finger == 0x82) {
		position[0][0] = buf[2] | (uint16_t)(buf[1] & 0x03) << 8;
		position[0][1] = buf[4] | (uint16_t)(buf[3] & 0x03) << 8;
		position[1][0] = buf[6] | (uint16_t)(buf[5] & 0x03) << 8;
		position[1][1] = buf[8] | (uint16_t)(buf[7] & 0x03) << 8;
		
		CYPRESS_DEBUG("%u %u %u %u\n", position[0][0],position[0][1], position[1][0],position[1][1]);
		input_report_abs(ts->input_dev, ABS_X, position[0][0]);
		input_report_abs(ts->input_dev, ABS_Y, position[0][1]);	
		input_report_key(ts->input_dev, BTN_TOUCH, 2);
		
		input_report_key(ts->input_dev, BTN_2, 1);					
		input_report_abs(ts->input_dev, ABS_HAT0X, position[1][0]);	
		input_report_abs(ts->input_dev, ABS_HAT0Y, position[1][1]);	
		input_sync(ts->input_dev);
		}

		else if (finger == 0x81) {
		position[0][0] = buf[2] | (uint16_t)(buf[1] & 0x03) << 8;
		position[0][1] = buf[4] | (uint16_t)(buf[3] & 0x03) << 8;
		input_report_abs(ts->input_dev, ABS_X, position[0][0]);
		input_report_abs(ts->input_dev, ABS_Y, position[0][1]);	
		input_report_key(ts->input_dev, BTN_TOUCH, 1);					
		input_sync(ts->input_dev);
	       }
#else 
		if ((finger == 0x81)|| (finger == 0x82)) {
			position[0][0] = buf[2] | (uint16_t)(buf[1] & 0x03) << 8;

			position[0][1] = buf[4] | (uint16_t)(buf[3] & 0x03) << 8;
		/* 
		 * always report the first point  whether slip  or click
		 * 
		 */
#if 0
		input_report_abs(ts->input_dev, ABS_PRESSURE, 300);
		input_report_abs(ts->input_dev, ABS_X, position[0][0]);
		input_report_abs(ts->input_dev, ABS_Y, position[0][1]);	
		input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, 1);
		input_report_key(ts->input_dev, BTN_TOUCH, 1);					
		input_sync(ts->input_dev);
#else
		if (is_first_point) {
			input_report_abs(ts->input_dev, ABS_PRESSURE, 300);
			input_report_abs(ts->input_dev, ABS_X, position[0][0]);
			input_report_abs(ts->input_dev, ABS_Y, position[0][1]);					 	 input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, 1);
			input_report_key(ts->input_dev, BTN_TOUCH, 1);
			input_sync(ts->input_dev);
			last_x = position[0][0];
			last_y = position[0][1];
			is_first_point = FALSE;
			}
		       
		/* 
		 * except the first point, also report the following points  
		 * 1) x aixes is 3 larger or smaller than the last reported point 
		 * 2) y aixes is 3 larger or smaller than the last reported point.
		 *
		 */
		else {
		if (((position[0][0]-last_x) >= TS_X_OFFSET) 
			|| ((last_x-position[0][0]) >= TS_X_OFFSET) 			
			|| ((position[0][1]-last_y) >= TS_Y_OFFSET) 
			|| ((last_y-position[0][1]) >= TS_Y_OFFSET)) 
			 {
				input_report_abs(ts->input_dev, ABS_PRESSURE, 300);
				input_report_abs(ts->input_dev, ABS_X, position[0][0]);
				input_report_abs(ts->input_dev, ABS_Y, position[0][1]);					 	 input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, 1);
				input_report_key(ts->input_dev, BTN_TOUCH, 1);
				input_sync(ts->input_dev);
				last_x = position[0][0];
				last_y = position[0][1];
			}
		}
	}
#endif
#endif
#ifdef CONFIG_CYPRESS_TOUCHSCREEN_GESTRUE
		GESTURE_TYPE gesture;
		gesture = i2c_smbus_read_byte_data(ts->client, 0x09);
		if (gesture < 0) {
			CYPRESS_DEBUG("i2c_smbus_read_byte_data failed\n");
		}
		else if ((gesture > 0) && (gesture < 16)) {
			CYPRESS_DEBUG("gesture = %d\n", gesture);
			input_report_gesture(ts->input_dev, gesture,0);
			input_sync(ts->input_dev);
		}
#endif
		if(finger == 0x80) {	
			int k;
			input_report_key(ts->input_dev, BTN_TOUCH, 0);				
			input_sync(ts->input_dev);
		/* 
		 * The next point must be first point whether slip or click after 
		 * this up event
		 */
			is_first_point = TRUE;
			for (k = 0; k < 3 ; k++) {
                      		ret = i2c_smbus_write_byte_data(ts->client, 0x0b, 0x80); 
				if (!ret) 
					break;
				else 
					CYPRESS_DEBUG("cpt write 0x0b 0x80 failed\n");
			}
			if (k == 3) 
				cypress_ts_power_on(ts, 1);
  		}
	}
out:
	if (ts->use_irq)
		enable_irq(ts->client->irq);
}

static enum hrtimer_restart cypress_cpt_ts_timer_func(struct hrtimer *timer)
{
	struct cypress_cpt_ts_data *ts = container_of(timer, struct cypress_cpt_ts_data, timer);
	

	queue_work(cypress_cpt_wq, &ts->work);

	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t cypress_cpt_ts_irq_handler(int irq, void *dev_id)
{
	struct cypress_cpt_ts_data *ts = dev_id;
	disable_irq(ts->client->irq);
	queue_work(cypress_cpt_wq, &ts->work);
	return IRQ_HANDLED;
}

/*ts reset pin is MPP14*/
static int cypress_ts_power_on(struct cypress_cpt_ts_data *ts, boolean on)
{	
/*把mpp14配置为MPP_DLOGIC_LVL_MMC，以对应触摸屏reset引脚的驱动电压*/
	
	struct mpp *mpp_ts_reset;
	int ret;
	if(TRUE == on)/*wake up ts from standby*/
	{
		mpp_ts_reset = mpp_get(NULL, "mpp14");
		if (!mpp_ts_reset)
		{
			CYPRESS_DEBUG(KERN_ERR "%s: mpp14 get failed\n", __func__);
			return -EIO;
		}
		
		ret = mpp_config_digital_out(mpp_ts_reset,
		      MPP_CFG(MPP_DLOGIC_LVL_MSMP,MPP_DLOGIC_OUT_CTRL_LOW ));/*set mpp14 low for 50ms to power down ts chip*/
		if (ret) 
		{
			CYPRESS_DEBUG(KERN_ERR
				"%s: Failed to configure mpp (%d)\n",
				__func__, ret);
			return -1;
		}
		
		msleep(50);

		ret = mpp_config_digital_out(mpp_ts_reset,
		      MPP_CFG(MPP_DLOGIC_LVL_MSMP,MPP_DLOGIC_OUT_CTRL_HIGH ));	
		if (ret) 
		{
			CYPRESS_DEBUG(KERN_ERR
				"%s: Failed to configure mpp (%d)\n",
				__func__, ret);
			return -1;
		}       	
	}	
	else/* make ts standby */
	{
	
		mpp_ts_reset = mpp_get(NULL, "mpp14");
		if (!mpp_ts_reset)
		{
			CYPRESS_DEBUG(KERN_ERR "%s: mpp14 get failed\n", __func__);
			return -EIO;
		}
		ret = mpp_config_digital_out(mpp_ts_reset,
			  MPP_CFG(MPP_DLOGIC_LVL_MSMP,MPP_DLOGIC_OUT_CTRL_HIGH ));	 
		if (ret) 
		{
			CYPRESS_DEBUG(KERN_ERR
				"%s: Failed to configure mpp (%d)\n",
				__func__, ret);
			return -1;
		}			
		ret = i2c_smbus_write_byte_data(ts->client, 0x0a, 0x08); 
		if (ret < 0) {
			CYPRESS_DEBUG(KERN_ERR "cypress sleep failed for \n");			
		}		
	}

	return(ret);
}
static int cypress_cpt_ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	struct vreg *v_gp6;
	int ret = 0;
#ifdef CONFIG_UPDATE_TS_FIRMWARE 
	static char version[10];
	int firmware_id;
#else
    struct cypress_cpt_ts_data *ts = NULL;
#endif    
	unsigned gpio_config, rc;
	int i;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CYPRESS_DEBUG(KERN_ERR "cypress_cpt_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	/* power on touchscreen */
    v_gp6 = vreg_get(NULL,"gp6");
    ret = IS_ERR(v_gp6);
    if(ret) 
        return ret;
        
    ret = vreg_set_level(v_gp6,2800);
    if (ret)
        return ret;
    ret = vreg_enable(v_gp6);
    if (ret)
        return ret;

#ifdef CONFIG_UPDATE_TS_FIRMWARE     
                g_client = client;      
#endif
	/* detect device */

    mdelay(800);

	for (i = 0; i < 10 ; i++) { 

		ret = i2c_smbus_read_byte_data(client, 0x00);
		if (ret < 0)
			continue;
		else
			goto find_device;
	}
	if (i == 10) 
	{
		CYPRESS_DEBUG("no cpt touchscreen\n");
		goto err_find_touchpanel__failed;
	}
find_device:

#ifdef CONFIG_UPDATE_TS_FIRMWARE 
       for (i = 0 ; i < 3; i++) {
            ret= ts_firmware_file();   
            if (!ret)
                break;
       }
#endif

	cypress_cpt_wq = create_singlethread_workqueue("cypress_cpt_wq");
	if (!cypress_cpt_wq)
		return -ENOMEM;
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}
	INIT_WORK(&ts->work, cypress_cpt_ts_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		CYPRESS_DEBUG(KERN_ERR "cypress_cpt_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "cypress_cpt-rmi-touchscreen";
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);	
	set_bit(EV_ABS, ts->input_dev->evbit);	
#if 1 	
	input_set_abs_params(ts->input_dev, ABS_X, 0, 320, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, 480, 0, 0);
#endif 
//	input_set_abs_params(ts->input_dev, ABS_X, 0, 1999, 0, 0);
//	input_set_abs_params(ts->input_dev, ABS_Y, 0, 2999, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 3000, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_TOOL_WIDTH, 0, 15, 0, 0);
	
#ifdef CONFIG_CYPRESS_TOUCHSCREEN_MULTIPOINT		
	set_bit(BTN_2, ts->input_dev->keybit);
	input_set_abs_params(ts->input_dev, ABS_HAT0X, 0, 320, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_HAT0Y, 0, 480, 0, 0);
#endif
#ifdef CONFIG_CYPRESS_TOUCHSCREEN_GESTRUE		
	set_bit(EV_GESTURE, ts->input_dev->evbit);
	for (i = GESTURE_NO_GESTURE; i < GESTURE_MAX; i++)
		set_bit(i, ts->input_dev->gesturebit);	
#endif	/*CONFIG_CYPRESS_TOUCHSCREEN_GESTRUE*/

 /* query version number */
#ifdef CONFIG_UPDATE_TS_FIRMWARE 
    ts->input_dev->uniq = version;
    firmware_id = i2c_smbus_read_byte_data(g_client, 0x0c);
    version[0] = (firmware_id / 16) + '0';
    version[1] = '.';
    version[2] = (firmware_id % 16) + '0';
    version[3] = '\0'; 
#endif 
	ret = input_register_device(ts->input_dev);
	if (ret) {
		CYPRESS_DEBUG(KERN_ERR "cypress_cpt_ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

      gpio_config = GPIO_CFG(57, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
	rc = gpio_tlmm_config(gpio_config, GPIO_ENABLE);
	
      CYPRESS_DEBUG(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n", __func__, 57, rc);

      if (rc)
      	{
		return -EIO;
	}
		
	if (gpio_request(57, "cypress_cpt_ts_int\n"))
		pr_err("failed to request gpio cypress_cpt_ts_int\n");
	
	ret = gpio_configure(57, GPIOF_INPUT | IRQF_TRIGGER_LOW);/*gpio 57 is interupt for touchscreen.*/
	if (ret) {
		CYPRESS_DEBUG(KERN_ERR "cypress_cpt_ts_probe: gpio_configure failed for %d\n", 57);
		goto err_input_register_device_failed;
	}
	

	if (client->irq) {
		ret = request_irq(client->irq, cypress_cpt_ts_irq_handler, 0, client->name, ts);
		
		if (ret == 0)
			ts->use_irq = 1;
		else
			dev_err(&client->dev, "cypress_cpt_ts_probe: request_irq failed\n");
	}
 
	if (!ts->use_irq) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = cypress_cpt_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = cypress_cpt_ts_early_suspend;
	ts->early_suspend.resume = cypress_cpt_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	CYPRESS_DEBUG(KERN_INFO "cpt_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");
	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:

	kfree(ts);
err_alloc_data_failed:
err_find_touchpanel__failed:
err_check_functionality_failed:
	return ret;
}

#ifdef CONFIG_UPDATE_TS_FIRMWARE 
static int ts_firmware_file(void)
{
	int ret;
	struct kobject *kobject_ts;
	kobject_ts = kobject_create_and_add("touch_screen", NULL);
	if (!kobject_ts) {
		printk("create kobjetct error!\n");
		return -1;
	}
	ret = sysfs_create_file(kobject_ts, &update_firmware_attribute.attr);
	if (ret) {
		kobject_put(kobject_ts);
		printk("create file error\n");
		return -1;
	}
	return 0;	
}
/*
 * The "update_firmware" file where a static variable is read from and written to.
 */
 
static ssize_t update_firmware_show(struct kobject *kobj, struct kobj_attribute *attr,char *buf)
{
	return 1;
}
static ssize_t 
update_firmware_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
		int ret0, ret1;	
		int i, j;
        ret0 = i2c_smbus_read_byte_data(g_client,0x00); 
        ret1 = i2c_smbus_read_byte_data(g_client,0x01);
        
        if (!strcmp(buf, "1")) {  
            
         if ((ret1 == 0xf1) && (ret0 != 0x80) && (ret0 != 0x81) && (ret0 != 0x82))

         { /*application chesmu error, update */

               for(i = 0; i < 3; i++) {    /* max update tries time is set 3 */

		            ret0 = i2c_update_firmware(); /*update firmware*/
		            if (ret0 == 0) { 
		                mdelay(800);   /*wait for ts reset*/
                        break;
                    }
               }
          }

        }
    //    else if (!strcmp(buf, "2")) {   /*update firmware from project menu*/      

        else if ( buf[0] == '2') {   /*update firmware from project menu*/      

            for(i = 0; i < 3; i++) {    /* max update tries time is set 3 */

                    ret0 = i2c_smbus_write_byte_data(g_client, 0x0d, 0x20); /* inform app switch to upgrade mode*/

                    printk("ret0 = %d\n", ret0);
                    if (!ret0) { 
                    
                    ret0 = i2c_update_firmware();   /*update firmware*/

                    if (ret0 == 0) { /*update firmware success */ 
                            printk("update firmware success\n!");
                            mdelay(800); /*wait for ts reset*/
                            return 1;
                    }
                    else if (ret0 == -2)   /*update error,reboot*/ 
                        arm_pm_restart(0);

                }
           }
        }
        
       return ret0;
 }

static int i2c_update_firmware(void) 
{
	char *buf;
	char *p;     //record a cmd start
	char *q;     //record a cmd end
	struct file	*filp;
    struct inode *inode = NULL;
	mm_segment_t oldfs;
        int	length;
	int i;
	int ret;
		const char filename[]="/system/touch/touchscreen_upgrade.txt";
        /* open file */
        oldfs = get_fs();
        set_fs(KERNEL_DS);
        filp = filp_open(filename, O_RDONLY, S_IRUSR);
        if (IS_ERR(filp)) {
            printk("%s: file %s filp_open error\n", __FUNCTION__,filename);
            set_fs(oldfs);
            return -1;
        }

        if (!filp->f_op) {
            printk("%s: File Operation Method Error\n", __FUNCTION__);
            filp_close(filp, NULL);
            set_fs(oldfs);
            return -1;
        }

        inode = filp->f_path.dentry->d_inode;
        if (!inode) {
            printk("%s: Get inode from filp failed\n", __FUNCTION__);
            filp_close(filp, NULL);
            set_fs(oldfs);
            return -1;
        }
        printk("%s file offset opsition: %u\n", __FUNCTION__, (unsigned)filp->f_pos);

        /* file's size */
        length = i_size_read(inode->i_mapping->host);
       	printk("%s: length=%d\n", __FUNCTION__, length);

	
	/* allocation buff size */
	buf = kmalloc((length+1), GFP_KERNEL);
	if (!buf) {
		
		printk("alloctation memory failed\n");
		return -1;
	}
	buf[length]= '\0'; 
		
     /* read data */
        if (filp->f_op->read(filp, buf, length, &filp->f_pos)!=length) {
            printk("%s: file read error\n", __FUNCTION__);
            filp_close(filp, NULL);
            set_fs(oldfs);
            return -1;
        }
		
		p=buf;
		q=p+1;
		i=1;
		do{
			while( (*q != 'w') && i < length)
			{
				q++;
			}
			ret = decode_cmd(p,q-1); //decode  i2c commd from update file
			if (ret == -2)  /*flash error */
				goto out; 
			else if (ret == 1)  { //last cmd success
			        ret = 0;
			        break;
            }		        
			p = q;
			q = p+1;
			i++;
		}while( i < length); // file end flag
#if 0
        mdelay(1000);
		ret = i2c_smbus_read_byte_data(g_client,0x01); 
		printk("ret2=%x\n",ret);  
        if (ret == 0xf0) {  /*cksum right */
                ret = sys_unlink(filename); /*delete firmware name*/
                printk("ret3=%d\n",ret);  
                ret = 0;
        }
        else 
                ret = -2;
#endif
        
 out: 
 	 filp_close(filp, NULL);
     set_fs(oldfs);
	 kfree(buf);
	 return ret;
}

static int decode_cmd(char *start, char *end)
{
	unsigned char buf[30];
	unsigned char m,n;
	struct i2c_msg msg;
	int tries;
	int ret;
	char *p;
	int i;
	int cmd_sizes = 0;
//	static int count = 0;
	
	p=start+5;      
	if(*p != '[')    //I2C write command 
	{
		switch(*p) 
		{
	
			case '7':
					cmd_sizes=25;    
					break;
			case '8':
					cmd_sizes=3;
					break;

			case '0':
					if(*(p+31)=='8' || *(p+31)=='B')
						cmd_sizes=11;
			
					else
						cmd_sizes=21;

					break;
			default:
					cmd_sizes=21;
					break;
		
		}
	//  count++;
	//	printk("g_client_addr = %d,cmd_size=%d", g_client->addr, cmd_sizes);
		for(i=0; i<cmd_sizes; i++)
		{	
			m= (*p) < 65? (*p-'0'): (*p-55);
			n= (*(p+1)) < 65? (*(p+1)-'0'): (*(p+1)-55);
			buf[i]=m*16+n;
			p+=3;
		}
		msg.addr = g_client->addr;
		msg.flags = 0;
		msg.buf = buf;
		msg.len = cmd_sizes;
		for(tries=0; tries < 10; tries++) {
				ret=i2c_transfer(g_client->adapter, &msg,1);
				if(ret == 1 ) {
				    if (cmd_sizes == 3)
				        return 1 ;     //the last cmd success
				    else 
				        return 0;
				}
		}	
	}
	else   //i2c read command
	{
			mdelay(100);
			ret = i2c_smbus_read_byte_data(g_client,0x90);
	//		printk("ret = %d", ret);
			if((ret == 0x20) || (ret < 0))  
				return 0;
			else
			   printk("i2c read err,ret=%d\n",ret);
    }
    
	return -2;

}
#endif
static int cypress_cpt_ts_remove(struct i2c_client *client)
{
	struct cypress_cpt_ts_data *ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int cypress_cpt_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct cypress_cpt_ts_data *ts = i2c_get_clientdata(client);

	if (ts->use_irq)
		disable_irq(client->irq);
	else
		hrtimer_cancel(&ts->timer);
	ret = cancel_work_sync(&ts->work);
	if (ret && ts->use_irq) /* if work was pending disable-count is now 2 */
		enable_irq(client->irq);	
	
	ret = cypress_ts_power_on(ts,0);	
	if (ret < 0) {
			CYPRESS_DEBUG(KERN_ERR "cypress_cpt_ts_probe power off failed\n");
			
	}
	
	
	return 0;
}

static int cypress_cpt_ts_resume(struct i2c_client *client)
{
	int ret;
	struct cypress_cpt_ts_data *ts = i2c_get_clientdata(client);

	ret = cypress_ts_power_on(ts,1);	
	if (ret < 0) 
	{
			CYPRESS_DEBUG(KERN_ERR "cpt_ts_probe power on failed\n");			
	}
	if (ts->use_irq)
		enable_irq(client->irq);

	if (!ts->use_irq)
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);	
    
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_cpt_ts_early_suspend(struct early_suspend *h)
{
	struct cypress_cpt_ts_data *ts;
	ts = container_of(h, struct cypress_cpt_ts_data, early_suspend);
	cypress_cpt_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void cypress_cpt_ts_late_resume(struct early_suspend *h)
{
	struct cypress_cpt_ts_data *ts;
	ts = container_of(h, struct cypress_cpt_ts_data, early_suspend);
	cypress_cpt_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id cypress_cpt_ts_id[] = {
	{ "cpt_ts", 0 },
	{ }
};

static struct i2c_driver cypress_cpt_ts_driver = {
	.probe		= cypress_cpt_ts_probe,
	.remove		= cypress_cpt_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend	= cypress_cpt_ts_suspend,
	.resume		= cypress_cpt_ts_resume,
#endif
	.id_table	= cypress_cpt_ts_id,
	.driver = {
		.name	= "cpt_ts",
	},
};

static int __devinit cypress_cpt_ts_init(void)
{
	return i2c_add_driver(&cypress_cpt_ts_driver);
}

static void __exit cypress_cpt_ts_exit(void)
{
	i2c_del_driver(&cypress_cpt_ts_driver);
	if (cypress_cpt_wq)
		destroy_workqueue(cypress_cpt_wq);
}

module_init(cypress_cpt_ts_init);
module_exit(cypress_cpt_ts_exit);

MODULE_DESCRIPTION("cypress_cpt Touchscreen Driver");
MODULE_LICENSE("GPL");
