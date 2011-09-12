/* drivers/input/touchscreen/cypress_i2c_ts.c
 *
 * Copyright (C) 2010 Huawei, Inc.
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
#include <mach/mpp.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <linux/vmalloc.h>
#include <linux/hardware_self_adapt.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/namei.h>

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#define TS_SCL_GPIO		60
#define TS_SDA_GPIO		61
//#define TS_DEBUG
#ifdef TS_DEBUG
#define CYPRESS_DEBUG(fmt, args...) printk(KERN_ERR fmt, ##args)
#else
#define CYPRESS_DEBUG(fmt, args...)
#endif

#define TS_X_OFFSET		1
#define TS_Y_OFFSET		TS_X_OFFSET

#define TS_INT_GPIO		29
#define TS_RESET_GPIO	96

#define CYPRESS_I2C_NAME "cypress-ts"

enum
{
   INPUT_INFO_NONE     = 0,
   INPUT_INFO_TOUCH_UP = 1,
   INPUT_INFO_KEY_UP   = 2,
};

enum
{
   TOUCH_KEY_INDEX0     = 0,
   TOUCH_KEY_INDEX1     = 1,
   TOUCH_KEY_INDEX2     = 2,
   TOUCH_KEY_INDEX3     = 3,
   TOUCH_KEY_INDEX_NONE = 4
};

struct cypress_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *key_input;
	struct workqueue_struct *cypress_wq;
	struct work_struct  work;
	int use_irq;
	struct hrtimer timer;
	struct early_suspend early_suspend;
	bool is_first_point;
	bool use_touch_key;
	int reported_finger_count;
	bool support_multi_touch;
	uint16_t last_x; 
	uint16_t last_y;
	uint8_t key_index_save;
	unsigned int x_max;
	unsigned int y_max;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_ts_early_suspend(struct early_suspend *h);
static void cypress_ts_late_resume(struct early_suspend *h);
#endif

static uint8_t *touch_key_value = NULL;
static uint8_t touch_key_value_num = 0;
static uint8_t touch_key_value_normal[] = {KEY_BACK, KEY_MENU, KEY_HOME, KEY_SEARCH, KEY_RESERVED};
static uint8_t touch_key_value_u8160[] = {KEY_HOME, KEY_MENU, KEY_BACK, KEY_SEARCH, KEY_RESERVED};

static char name[30];
static struct i2c_client *g_client = NULL;
static struct cypress_ts_data *ts = NULL;
/* this codes update cypress`s touch  */
#ifdef CONFIG_CYPRESS_UPDATE_TS_FIRMWARE
#define CYPRESS_FILE_LENGTH 184320		/* MAX file length */
 
static ssize_t cyp_update_firmware_show(struct kobject *kobj, 
struct kobj_attribute *attr,char *buf);
static ssize_t cyp_update_firmware_store(struct kobject *kobj, 
struct kobj_attribute *attr, const char *buf, size_t count);
static int cyp_ts_firmware_file(void);
static int i2c_update_firmware(void);
static int cyp_i2c_transfer(char *buf, uint8_t num, uint8_t flag);
static int chang_ASCII_byte(uint8_t msb_data, uint8_t lsb_data, uint8_t *ret_data);
static int chang_ASCII(uint8_t *input_data);

static struct kobj_attribute cyp_update_firmware_attribute = {
	.attr = {.name = "update_firmware", .mode = 0666},
	.show = cyp_update_firmware_show,
	.store = cyp_update_firmware_store,
};
static  int cyp_ts_firmware_file(void)
{
	int ret;
	struct kobject *kobject_ts;
	kobject_ts = kobject_create_and_add("touch_screen", NULL);
	if (!kobject_ts) {
		printk("create kobjetct error!\n");
		return -1;
	}
	ret = sysfs_create_file(kobject_ts, &cyp_update_firmware_attribute.attr);
	if (ret) {
		kobject_put(kobject_ts);
		printk("create file error\n");
		return -1;
	}
	return 0;	 
}

static ssize_t cyp_update_firmware_show(struct kobject *kobj,
							struct kobj_attribute *attr,char *buf)
{
	int ret = -1;
	ret = i2c_smbus_read_byte_data(g_client, 0x01);	
	printk("0x01 = 0x%x\n", ret);
	printk("addr = 0x%x\n", g_client->addr);
	return 0;
}

static ssize_t cyp_update_firmware_store(struct kobject *kobj, 
							struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret = -1;
	struct i2c_msg msg;
	uint8_t out_bootloader_buffer[20] = {0x00,0x00,0xFF,0xA5,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07};

	printk("Update_firmware_store.\n");
	if (buf[0] == '2') {
		disable_irq(g_client->irq);
		/* into bootloader mode */
		ret = gpio_tlmm_config(GPIO_CFG(TS_INT_GPIO, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		ret = gpio_direction_output(TS_INT_GPIO, 0);
		ret = i2c_smbus_write_byte_data(g_client, 0x00, 0x01);
		msleep(200);
		ret = i2c_smbus_read_byte_data(g_client, 0x01);		/* read status bit */
		if (ret < 0) {
			printk("Into bootloader mode error1.\n");
			return -1;
		} else if ( 0x10 == (ret&0x10) ) {
			printk("Into bootloader mode success.\n");
			ret = gpio_direction_output(TS_INT_GPIO, 1);
			ret = gpio_tlmm_config(GPIO_CFG(TS_INT_GPIO, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
		} else {
			printk("Into bootloader mode error2.\n");
			return -1;
		}

		/*update firmware*/
		ret = i2c_update_firmware();
		if (ret < 0) {
			printk("i2c_update_firmware_error.\n");
			return -1;
		}

		/* out of bootloader mode */
		msleep(200);
		msg.addr = g_client->addr;
		msg.flags = 0;
		msg.buf = out_bootloader_buffer;
		msg.len = 12;
		ret = i2c_transfer(g_client->adapter, &msg, 1);
		if (ret < 0) {
			printk("Out of bootloader mode error1.\n");
			return -1;
		}
		msleep(200);
		enable_irq(g_client->irq);
		ret = i2c_smbus_read_byte_data(g_client, 0x01);
		if ( 0 == (ret&0x10) ) {
			printk("Out of bootloader mode success.\n");
			msleep(5000);
			return 1;
		} else {
			printk("Out of bootloader mode error2.\n");
			return -1;
		}
	}
	return -1;
}

static int i2c_update_firmware(void)
{
	char *file_buf;
	struct file *filp;
	struct inode *inode = NULL;
	mm_segment_t oldfs;
	uint32_t length;
	int ret = 0;
	const char filename[]="/sdcard/update/cypress_firmware.iic";
	int i = 0;
	uint8_t num = 0;
	uint8_t i2c_buffer[20];

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
	if (!( length > 0 && length < CYPRESS_FILE_LENGTH )){
		printk("file size error\n");
		filp_close(filp, NULL);
		set_fs(oldfs);
		return -1;
	}

	/* allocation buff size */
	file_buf = vmalloc(length);
	if (!file_buf) {
		printk("alloctation memory failed\n");
		filp_close(filp, NULL);
	set_fs(oldfs);
		return -1;
	}

	/* read data */
	if (filp->f_op->read(filp, file_buf, length, &filp->f_pos) != length) {
	printk("%s: file read error\n", __FUNCTION__);
	filp_close(filp, NULL);
	set_fs(oldfs);
	vfree(file_buf);
	return -1;
	}

	i = 0;
	do {
		if (file_buf[i] == 'w') {
			if ((file_buf[i+2] == '2')&&(file_buf[i+3] == '5')) {
				num = 0;
				i += 5;/* the first data position */
				do {
					ret = chang_ASCII_byte(file_buf[i], file_buf[i+1], &i2c_buffer[num]);
					if (ret < 0) {
						printk("chang ASCII error.\n");
						return -1;
					}
					num++;
					i += 3;/* next byte */
				} while(file_buf[i] != 'p');

				ret = cyp_i2c_transfer(i2c_buffer, num, 0);
				if (ret < 0) {
					printk("i2c transfer read failed.\n");
					return -1;
				}
			} else {
				printk("read file failed_w: 0x25\n");
				return -1;
			}
		} else if (file_buf[i] == 'r') {		/* read 3 bytes */
			if ((file_buf[i+2] == '2')&&(file_buf[i+3] == '5')) {
				num = 3;
				ret = cyp_i2c_transfer(i2c_buffer, num, 1);
				if (ret < 0) {
					printk("i2c transfer read failed.\n");
					return -1;
				}
				i += 11; /* "r 23 x x x p" */
			} else {
				printk("read file failed_r: 0x25\n");
				return -1;
			}
		} else if (file_buf[i] == '[') {
			if ((file_buf[i+1] == 'd')&&(file_buf[i+2] == 'e')) {	/* "delay" */
				if (i < 100) {
					printk("delay 12Sec.\n");
					mdelay(12000);
					i += 12;					/* "[delay=12000]" */
				} else {
					printk("#");
					mdelay(100);
					i += 10;					/* "[delay=100]" */
				}
			} else {
				printk("delay error.\n");
				return -1;
			}
		} else {
			printk("read error.\n");
			return -1;
		}
		i += 3;									/* 13:CR,10:LF */
	} while(i < (length - 1));

	filp_close(filp, NULL);
	set_fs(oldfs);
	vfree(file_buf);
	printk("%s: free file buffer\n", __FUNCTION__);

	return 0;
}

/**
 * cyp_i2c_transfer
 * @buf: the data to be transfered
 * @flag: "0" means write,"1" means read.
 * @num: Number of datas to be transfered.
 * Returns negative if error.
 */
static int cyp_i2c_transfer(char *buf, uint8_t num, uint8_t flag)
{
	int ret = -1;
	struct i2c_msg msg;

	msg.addr = g_client->addr;
	msg.buf = buf;
	msg.len = num;
	if (flag == 1) {				/* read */
		msg.flags = I2C_M_RD;
	} else if (flag == 0) {			/* write */
		msg.flags = 0;
	}

	ret = i2c_transfer(g_client->adapter, &msg, 1);
	if (ret < 0) {
		printk("cyp i2c transfer error.\n");
		return -1;
	} else if (flag == 1) {
		if (0x20 == buf[2]) {		/* if  the 3rd data is 0x20, the operation is success */
			return 0;
		} else {
			printk("cyp i2c transfer read 0x20 error.\n");
			return -1;
		}
	} else {
		return 0;
	}
}

static int chang_ASCII_byte(uint8_t msb_data, uint8_t lsb_data, uint8_t *ret_data)
{
	int ret;
	uint8_t data_msb = msb_data;
	uint8_t data_lsb = lsb_data;

	ret = chang_ASCII(&data_msb);
	if (ret < 0) {
		return -1;
	}
	ret = chang_ASCII(&data_lsb);
	if (ret < 0) {
		return -1;
	}

	*ret_data = ((data_msb<<4)&0xFF)|data_lsb;
	return 0;
}

static int chang_ASCII(uint8_t *input_data)
{
	if ((*input_data >= '0')&&(*input_data <= '9')) {
		*input_data = *input_data - '0';
		return 0;
	} else if ((*input_data >= 'A')&&(*input_data <= 'F')) {
		*input_data = *input_data - 'A' + 10;
		return 0;
	} else {
		return -1;
	}
}
#endif
static void cypress_ts_work_func(struct work_struct *work)
{
	int i,k;
	int ret;
	int bad_data = 0;
	struct i2c_msg msg[2];
	uint8_t start_reg;
	uint8_t z = 0;
	uint8_t touch1_z = 0;
	uint8_t input_info = 0;
	uint8_t buf[16];
	uint8_t key_info = 0;
	uint8_t key_index = 0;
	uint8_t key_pressed = 0;
	uint16_t position[2][2];
	uint8_t w = 0;	
	struct cypress_ts_data *ts = container_of(work, struct cypress_ts_data, work);
	start_reg = 0x02;
	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;

	for (i = 0; i < ((ts->use_irq && !bad_data)? 1 : 5 ); i++)
	{
		ret = i2c_transfer(ts->client->adapter, msg, 2);
		if (ret < 0) 
		{
			CYPRESS_DEBUG("%d times i2c_transfer failed\n",i);
			bad_data = 1;
			continue;
		}
		else
		{
		    bad_data = 0;
		}
	if (i == 5) 
		{
			pr_err("five times i2c_transfer error\n");
			break;
		}		
#ifdef TS_DEBUG
        /*printf debug info*/
        for(k=0; k < sizeof(buf); k++)
        {
            CYPRESS_DEBUG("%s:register[0x%x]= 0x%x \n",__FUNCTION__, 0x10+k, buf[k]);
        }
#endif

		input_info = buf[0] & 0x0f;//number of touches	   
		position[0][0] = buf[2] | (uint16_t)(buf[1] ) << 8; //this is 1st point x positon
		position[0][1] = buf[4] | (uint16_t)(buf[3] ) << 8; //this is 1st point y positon
		position[1][0] = buf[8] | (uint16_t)(buf[7]) << 8;  //this is 2st point x positon
		position[1][1] = buf[10] | (uint16_t)(buf[9] ) << 8;   //this is 2st point y positon     
		z = buf[5];//reading pressure register 
		touch1_z = buf[5];
		key_info = buf[0x0e] & 0x0f;//button status
		if(!input_info) {
		/*touch key */
		switch(key_info) {
			case 1 :
				key_index=3;
				break;
			case 2 :
				key_index=2;
				break;
			case 4 :
				key_index=1;
				break;
			case 8 :	
				key_index=0;
				break;
			default: 
				break;
			}
			}
		key_pressed = (buf[0x0e] & 0x0f) ; 
		if (input_info == 1) //single point touch
		{
		CYPRESS_DEBUG("Touch_Area: X = %d Y = %d \n",position[0][0],position[0][1]);	        
			if (ts->is_first_point) 
			{
				input_report_abs(ts->input_dev, ABS_X, position[0][0]);
				input_report_abs(ts->input_dev, ABS_Y, position[0][1]);
				/* reports pressure value for inputdevice*/
				input_report_abs(ts->input_dev, ABS_PRESSURE, z);
				input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, z>>5);
				input_report_key(ts->input_dev, BTN_TOUCH, 1);
				input_sync(ts->input_dev);
				ts->last_x = position[0][0];
				ts->last_y = position[0][1];
				ts->is_first_point = false;
			}
			else 
			{
				 if (((position[0][0]-ts->last_x) >= TS_X_OFFSET) 
					     || ((ts->last_x-position[0][0]) >= TS_X_OFFSET) 			
					     || ((position[0][1]-ts->last_y) >= TS_Y_OFFSET) 
					     || ((ts->last_y-position[0][1]) >= TS_Y_OFFSET)) 
				 {
					input_report_abs(ts->input_dev, ABS_X, position[0][0]);
					input_report_abs(ts->input_dev, ABS_Y, position[0][1]);	
					input_report_abs(ts->input_dev, ABS_PRESSURE, z);
					input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, z>>5);
					input_report_key(ts->input_dev, BTN_TOUCH, 1);
					input_sync(ts->input_dev);
					ts->last_x = position[0][0];
					ts->last_y = position[0][1];
				}
			}
        }
        else if (input_info == 0)
        {
            if(!ts->use_touch_key)
            {
                CYPRESS_DEBUG("Touch_Area: touch release!! \n");
                ts->is_first_point = true;
                input_report_abs(ts->input_dev, ABS_PRESSURE, z);
                input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, z>>5);
                input_report_key(ts->input_dev, BTN_TOUCH, 0);	
                input_sync(ts->input_dev);
            }
            else
            {
                if(key_info != 0)
                {
                    if(ts->key_index_save != key_index)
                    {
                        ts->key_index_save = key_index;
                        CYPRESS_DEBUG("Touch_key_Area: touch_key_value[%d]= %d , pressed = %d\n",key_index,touch_key_value[key_index],key_info);
                        input_report_key(ts->key_input, touch_key_value[key_index], key_info);	
                        input_sync(ts->key_input);
                    }
                }
                else
                {
                   if(ts->key_index_save != TOUCH_KEY_INDEX_NONE)
                    {
                        if(ts->key_index_save < TOUCH_KEY_INDEX_NONE)
                        {
                            CYPRESS_DEBUG("Touch_key release, touch_key_value[%d]= %d, released = %d \n",ts->key_index_save,touch_key_value[ts->key_index_save], key_info);
                            input_report_key(ts->key_input, touch_key_value[ts->key_index_save], key_info);	
                            input_sync(ts->key_input);
                        }
                    }
                    else
                    {
                        CYPRESS_DEBUG("Touch_Area: touch release!! \n");
                        ts->is_first_point = true;
                        input_report_abs(ts->input_dev, ABS_PRESSURE, z);
                        input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, w);
                        input_report_key(ts->input_dev, BTN_TOUCH, 0);
                        input_sync(ts->input_dev);
                    }

                    ts->key_index_save = TOUCH_KEY_INDEX_NONE;
                }
                }
            }
}
	if (ts->use_irq)
	{
		enable_irq(ts->client->irq);
		CYPRESS_DEBUG("cypress_ts_work_func,enable_irq\n");
	}
}

static enum hrtimer_restart cypress_ts_timer_func(struct hrtimer *timer)
{
	struct cypress_ts_data *ts = container_of(timer, struct cypress_ts_data, timer);
	CYPRESS_DEBUG("cypress_ts_timer_func\n");
	queue_work(ts->cypress_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t cypress_ts_irq_handler(int irq, void *dev_id)
{
	struct cypress_ts_data *ts = dev_id;
	disable_irq_nosync(ts->client->irq);
	CYPRESS_DEBUG("cypress_ts_irq_handler: disable irq\n");
	queue_work(ts->cypress_wq, &ts->work);
	return IRQ_HANDLED;
}

static int cypress_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
#ifndef CONFIG_CYPRESS_UPDATE_TS_FIRMWARE
	struct cypress_ts_data *ts;
#endif
	int ret = 0;
	int gpio_config;
	int i;
	int cypid=0;
	int verh=0;
	int verl=0;
	
	int bad_data = 0;
	struct i2c_msg msg[2];
	uint8_t start_reg=0x13;	
	/* buf[12] is normal model serial*/
	uint8_t buf[12] = {0x00, 0x00, 0xFF, 0xA5, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
	struct i2c_msg msgz[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 12,
			.buf = buf,
		},
	};

	if (touch_is_supported())
		return -ENODEV;

	CYPRESS_DEBUG(" In cypress_ts_probe: \n");	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
	{
		pr_err(KERN_ERR "cypress_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	/* delete gp5 power on*/
#ifdef CONFIG_CYPRESS_RESTORE_FIRMWARE
	goto restore_firmware;
#endif

	if(machine_is_msm7x25_m860() || machine_is_msm7x25_c8600())
	{
		/* out of deep sleep mode */
		gpio_tlmm_config(GPIO_CFG(TS_INT_GPIO, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_direction_output(TS_INT_GPIO, 0);
		mdelay(2);
		gpio_direction_output(TS_INT_GPIO, 1);
		mdelay(2);
		gpio_direction_output(TS_INT_GPIO, 0);
		gpio_tlmm_config(GPIO_CFG(TS_INT_GPIO, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
	}
	else
	{
		ret = gpio_tlmm_config(GPIO_CFG(TS_RESET_GPIO, 0, GPIO_OUTPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
		ret = gpio_direction_output(TS_RESET_GPIO, 1);
		mdelay(10);
		ret = gpio_direction_output(TS_RESET_GPIO, 0);
	}
	mdelay(200);	
	/* driver  detect its device  */  
	for(i = 0; i < 3; i++) 
	{
		ret = i2c_smbus_read_byte_data(client, 0x00);
		CYPRESS_DEBUG("id:%d\n",ret);
		if (ret < 0)
			continue;
		else
			break; //goto  succeed_find_device;
	}
	if (i == 3) 
	{	
		pr_err("%s:check %d times, but dont find cypress_ts device\n",__FUNCTION__, i);	
		goto err_find_touchpanel_failed;
	}

	for(i = 0; i < 3; i++) 
	{
		ret = i2c_smbus_read_byte_data(client, 0x01);
		printk(KERN_ERR "mode:0x%x\n",ret);
		if (ret < 0)
			continue;
		else
			break;//goto  succeed_find_device;
	}
	if (i == 3)
	{
		pr_err("%s:check %d times, but dont find melfas_ts device\n",__FUNCTION__, i);	
		goto err_find_touchpanel_failed;
	}

	if(ret & 0x10) /*check bootloader mode*/
	{
		printk(KERN_ERR "%s: in bootloader mode\n",__FUNCTION__);
		ret = i2c_transfer(client->adapter, msgz, 1);
		if(0 <= ret)
		{
			mdelay(100);
			ret = i2c_smbus_read_byte_data(client, 0x01);
			printk(KERN_ERR "mode:0x%x\n",ret);
			if(ret & 0x10)
			{
				printk(KERN_ERR "%s: transfer to normal mode failed\n",__FUNCTION__);
				goto err_check_functionality_failed;
			}
		}
		else
			goto err_check_functionality_failed;
	}

	/*succeed_find_device*/
	set_touch_support(true);
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) 
	{
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);
	/*reading cypress`s version and ID*/	
		msg[0].addr = ts->client->addr;
		printk("i2c addr=%d\n",ts->client->addr);
		msg[0].flags = 0;
		msg[0].len = 1;
		msg[0].buf = &start_reg;
		msg[1].addr = ts->client->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = 4; //reading version needs 4 bytes 
		msg[1].buf = buf;	
	for (i = 0; i < (( !bad_data)? 1 : 5 ); i++)
	{
		ret = i2c_smbus_write_byte_data(client, 0x00,(0x04&0x8f)|0x10);//in System Information mode
		if(ret < 0)
		{
		printk(" i2c_transfer failed\n");
		bad_data = 1;
		continue;
		}
		mdelay(50);
		ret = i2c_transfer(ts->client->adapter, msg, 2);
		if (ret < 0) 
		{
			printk("%d times i2c_transfer failed\n",i);
			bad_data = 1;
			continue;
		}
		else
		{
			bad_data = 0;
		}
		if (i == 5) 
		{
			pr_err("five times i2c_transfer error\n");
			break;
		}	
		cypid=(buf[0]<<8)|buf[1]; //ID number
		verh=buf[2]; //version high
		verl=buf[3];  //version low
		ret = i2c_smbus_write_byte_data(client, 0x00,(0x14&0x8f)); //in Normal operating mode
		if(ret < 0)
		{
			printk(" i2c_transfer failed\n");
			ret = i2c_smbus_write_byte_data(client, 0x00,(0x14&0x8f));
		}
		mdelay(50);
	}	
	CYPRESS_DEBUG("i2c addr=%d\n",ts->client->addr);	
	ts->cypress_wq = create_singlethread_workqueue("cypress_wq");
	if (!ts->cypress_wq) 
	{
		pr_err("%s:create cypress_wq error\n",__FUNCTION__);
		ret = -ENOMEM;
		goto err_destroy_wq;
	}
	INIT_WORK(&ts->work, cypress_ts_work_func);

    ts->is_first_point = true;
    ts->use_touch_key = false;
    ts->key_index_save = TOUCH_KEY_INDEX_NONE;
    if(board_use_tssc_touch(&ts->use_touch_key))
    {
        printk(KERN_ERR "%s: Cannot support cypress touch_keypad!\n", __FUNCTION__);
        ret = -ENODEV;
        goto err_destroy_wq;
    }
    if(machine_is_msm7x25_m860() || machine_is_msm7x25_c8600())
    {
    	ts->x_max = 320;
    	ts->y_max = 480;	
    }
	else if(machine_is_msm7x25_u8350()){
		ts->x_max = 320;
		ts->y_max = 240;
	} 
    else
    {
    	ts->x_max = 240;
    	ts->y_max = 320;	
    }
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		pr_err("cypress_ts_probe: Failed to allocate touch input device\n");
		goto err_input_dev_alloc_failed;
	}
	/*setting inputdev name,displaying name ,version  id*/
	{
		memset(name,0,30);
		/* cypid 0x1386 is used by U8150/C8150,0xaa1d for M860/C8600 */
		sprintf(name, "cypress-ts-%s.Ver%x%x",((cypid==0x1386 || cypid==0xaa1d) ? "innolux":"byd"),verh,verl);	
		ts->input_dev->name =name;		
	}
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	input_set_abs_params(ts->input_dev, ABS_X, 0, ts->x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_Y, 0, ts->y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	/*modify width reported max value*/
	input_set_abs_params(ts->input_dev, ABS_TOOL_WIDTH, 0, 16, 0, 0);
	ret = input_register_device(ts->input_dev);
	if (ret) 
	{
		pr_err("cypress_ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}

    if(ts->use_touch_key)
    {
    	ts->key_input = input_allocate_device();
    	if (!ts->key_input  || !ts) {
    		ret = -ENOMEM;
    		pr_err("cypress_ts_probe: Failed to allocate key input device\n");
    		goto err_key_input_dev_alloc_failed;
    	}

	    if(machine_is_msm7x25_u8160())
	    {
		    touch_key_value = &touch_key_value_u8160;
		    touch_key_value_num = sizeof(touch_key_value_u8160);
	    }
	    else
	    {
		    touch_key_value = &touch_key_value_normal;
		    touch_key_value_num = sizeof(touch_key_value_normal);
	    }
		
		/*rename touchscreen-keypad to use touchscreen-keypad's menu to entry safe mode*/
	    ts->key_input->name = "touchscreen-keypad";
	
	    set_bit(EV_KEY, ts->key_input->evbit);
	    for (i = 0; i < touch_key_value_num; i++)
	    {
		    set_bit(touch_key_value[i] & KEY_MAX, ts->key_input->keybit);
	    }

		ret = input_register_device(ts->key_input);
		if (ret)
			goto err_key_input_register_device_failed;
	}
	gpio_config = GPIO_CFG(TS_INT_GPIO, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
	ret = gpio_tlmm_config(gpio_config, GPIO_ENABLE);
	if (ret < 0) 
	{
		pr_err("%s: gpio_tlmm_config(%#x)=%d\n", __func__, TS_INT_GPIO, ret);
		ret = -EIO;
		goto err_key_input_register_device_failed; 
	}
	
	if (gpio_request(TS_INT_GPIO, "cypress_ts_int\n"))
		pr_err("failed to request gpio cypress_ts_int\n");
	
	ret = gpio_configure(TS_INT_GPIO, GPIOF_INPUT | IRQF_TRIGGER_FALLING);/*gpio 29 is interupt for touchscreen.*/
	if (ret) 
	{
		pr_err("cypress_ts_probe: gpio_configure %d irq failed\n", TS_INT_GPIO);
		goto err_key_input_register_device_failed;
	}

	if (client->irq) 
	{
		ret = request_irq(client->irq, cypress_ts_irq_handler, 0, client->name, ts);		
		if (ret == 0)
			ts->use_irq = 1;
		else
			dev_err(&client->dev, "cypress_ts_probe: request_irq failed\n");
	}
	if (!ts->use_irq) 
	{
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = cypress_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = cypress_ts_early_suspend;
	ts->early_suspend.resume = cypress_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif


    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_TOUCH_PANEL);
    #endif
    
	printk(KERN_INFO "cypress_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");

#ifdef CONFIG_CYPRESS_UPDATE_TS_FIRMWARE 
restore_firmware:
	g_client = client;
	{
		for (i = 0 ; i < 3; i++) {
			ret= cyp_ts_firmware_file();
			if (!ret)
			break;
		}
	}
#endif
	return 0;

err_key_input_register_device_failed:
	if(ts->use_touch_key)
	{
		input_free_device(ts->key_input);
	}
err_key_input_dev_alloc_failed:
err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_destroy_wq:
	destroy_workqueue(ts->cypress_wq);	
	kfree(ts);
err_alloc_data_failed:
err_find_touchpanel_failed:
	//delete vreg_disable(v_gp5)
err_check_functionality_failed:
	return ret;
}

static int cypress_ts_remove(struct i2c_client *client)
{
	struct cypress_ts_data *ts = i2c_get_clientdata(client);
	unregister_early_suspend(&ts->early_suspend);
	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

static int cypress_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	struct cypress_ts_data *ts = i2c_get_clientdata(client);
	CYPRESS_DEBUG("In cypress_ts_suspend\n");
	
	if (ts->use_irq)
		disable_irq(client->irq);
	else
		hrtimer_cancel(&ts->timer);
	ret = cancel_work_sync(&ts->work);
	if (ret && ts->use_irq) /* if work was pending disable-count is now 2 */
	{
		enable_irq(client->irq);
	}	
	ret = i2c_smbus_write_byte_data(client, 0x00, 0x02);	/* deep sleep */
		if (ret<0) {
			pr_err("cypress_ts_suspend: deep sheep failed\n");
		}
	
	return 0;
}

static int cypress_ts_resume(struct i2c_client *client)
{
	struct cypress_ts_data *ts = i2c_get_clientdata(client);
	CYPRESS_DEBUG("In cypress_ts_resume\n");
	/*setting TS_INT resume touch*/
	{
		gpio_tlmm_config(GPIO_CFG(TS_INT_GPIO, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
		gpio_direction_output(TS_INT_GPIO, 0);
		msleep(2);
		gpio_direction_output(TS_INT_GPIO, 1);
		msleep(2);
		gpio_direction_output(TS_INT_GPIO, 0);
		gpio_tlmm_config(GPIO_CFG(TS_INT_GPIO, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA), GPIO_ENABLE);
		gpio_configure(TS_INT_GPIO, GPIOF_INPUT | IRQF_TRIGGER_FALLING);
	}
	msleep(200);  
	
	if (ts->use_irq) 
	{
		enable_irq(client->irq);
	}
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cypress_ts_early_suspend(struct early_suspend *h)
{
	struct cypress_ts_data *ts;
	CYPRESS_DEBUG("cypress_ts_early_suspend\n");
	ts = container_of(h, struct cypress_ts_data, early_suspend);
	cypress_ts_suspend(ts->client, PMSG_SUSPEND);  
}

static void cypress_ts_late_resume(struct early_suspend *h)
{
	struct cypress_ts_data *ts;
	CYPRESS_DEBUG("cypress_ts_late_resume\n");
	ts = container_of(h, struct cypress_ts_data, early_suspend);
	cypress_ts_resume(ts->client);	
}
#endif
static const struct i2c_device_id cypress_ts_id[] = {
	{ CYPRESS_I2C_NAME, 0 },
	{ }
};
static struct i2c_driver cypress_ts_driver = {
	.probe		= cypress_ts_probe,
	.remove		= cypress_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= cypress_ts_suspend,
	.resume		= cypress_ts_resume,
#endif
	.id_table	= cypress_ts_id,
	.driver = {
		.name	= CYPRESS_I2C_NAME,
	},
};

static int __devinit cypress_ts_init(void)
{
	CYPRESS_DEBUG(KERN_ERR "cypress_ts_init\n ");
	return i2c_add_driver(&cypress_ts_driver);
}

static void __exit cypress_ts_exit(void)
{
	i2c_del_driver(&cypress_ts_driver);
}
module_init(cypress_ts_init);
module_exit(cypress_ts_exit);

MODULE_DESCRIPTION("Cypress Touchscreen Driver");
MODULE_LICENSE("GPL");
