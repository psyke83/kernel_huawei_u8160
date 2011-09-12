/* drivers/input/touchscreen/synaptics_i2c_rmi_tm.c
 *
 * Copyright (C) 2009 HUAWEI.
 *
 * NOTICE by TJ Style 2011
 *
 * This is the multitouch patched driver for C8150 and U8150 devices
 **/

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
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/namei.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif



//----------------------------------------------
//ENABLE DEBUGING
//----------------------------------------------

#define TS_DEBUG
//#undef TS_DEBUG

#ifdef TS_DEBUG
#define SYNAPITICS_DEBUG(fmt, args...) printk(KERN_DEBUG fmt, ##args)
#else
#define SYNAPITICS_DEBUG(fmt, args...)
#endif

#define GPIO_TOUCH_INT   29
#define TS_RESET_GPIO    96

#define LCD_X_MAX    240
#define LCD_Y_MAX    320

enum
{
    F01_RMI_DATA00  = 0x13,
    F01_RMI_CTRL00  = 0x25,
    F01_RMI_CTRL01_00 = 0x26,
    F01_RMI_CMD00   = 0x63,
    F01_RMI_QUERY00 = 0x6E,

    F01_RMI_DATA0 = 0x13,
    F01_RMI_CTLR0 = 0x25,
    F01_RMI_QUERY0 = 0x6E,

    F34_RMI_QUERY0 = 0x65,
    F34_RMI_QUERY1 = 0x66,
    F34_RMI_QUERY2 = 0x67,
    F34_RMI_QUERY3 = 0x68,
    F34_RMI_QUERY4 = 0x69,
    F34_RMI_QUERY5 = 0x6A,
    F34_RMI_QUERY6 = 0x6B,
    F34_RMI_QUERY7 = 0x6C,
    F34_RMI_QUERY8 = 0x6D,
    
    F34_RMI_DATA0 = 0x00,
    F34_RMI_DATA1 = 0x01,
    F34_RMI_DATA2 = 0x02,
    F34_RMI_DATA3 = 0x12,
	
};

#define  Manufacturer_ID  0x01

//----------------------------------------------
//TOUCHSCREEN_EXTRA_KEY
//----------------------------------------------
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef min
#define min(a,b) ((a)>(b)?(b):(a))
#endif
#ifndef max
#define max(a,b) ((b)>(a)?(b):(a))
#endif
#ifndef abs
#define abs(a)  ((0 < (a)) ? (a) : -(a))
#endif

#define TS_X_MAX     1759
#define TS_Y_MAX     2584
#define TS_KEY_Y_MAX 248
#define TOUCH_KEY_X_REGION_3key     226		/* for 3 touch key: U8180 and C8510 */
#define TOUCH_KEY_X_REGION_4key     169		/* for 4 touch key: U8160 */
/* delete X_START, X_END, Y_START, Y_END */

#define EXTRA_MAX_TOUCH_KEY    4
#define EXTRA_MAX_TOUCH_KEY_U8180    3
#define TS_KEY_DEBOUNCE_TIMER_MS 60

//----------------------------------------------
//Removing firmware update function
//to preventing from wrong update
//----------------------------------------------

/* to define a region of touch panel */
typedef struct
{
    u16 touch_x_start;
    u16 touch_x_end;
    u16 touch_y_start;
    u16 touch_y_end;
} touch_region;

/* to define virt button of touch panel */
typedef struct 
{
    u16  center_x;
    u16  center_y;
    u16  x_width;
    u16  y_width;
    u32   touch_keycode;
} button_region;

/* to define extra touch region and virt key region */
typedef struct
{
    touch_region   extra_touch_region;
    button_region  extra_key[EXTRA_MAX_TOUCH_KEY];
} extra_key_region;

/* to record keycode */
typedef struct {
	u32                 record_extra_key;             /*key value*/   
	bool                bRelease;                     /*be released?*/   
	bool                bSentPress;                  
	bool                touch_region_first;           /* to record first touch event*/
} RECORD_EXTRA_KEYCODE;

static extra_key_region   *touch_extra_key_region = NULL;

/* to init extra region and touch virt key region */
static extra_key_region   touch_extra_key_region_normal =
{
    {0, TS_X_MAX, TS_Y_MAX-TS_KEY_Y_MAX+50, TS_Y_MAX},			/* extra region: X_START=0, X_END, Y_START, Y_END */
    {
       {(TS_X_MAX*1/8),   (TS_Y_MAX-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_BACK},  /* back key */
       {(TS_X_MAX*3/8),   (TS_Y_MAX-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_MENU},  /* menu key */
       {(TS_X_MAX*5/8),   (TS_Y_MAX-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_HOME},  /* home key */
       {(TS_X_MAX*7/8),   (TS_Y_MAX-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_SEARCH},  /* Search key */
    },
};

static uint16_t   touch_key_number = 0;
static u32 touch_extra_key_index_normal[4] = {KEY_BACK, KEY_MENU, KEY_HOME, KEY_SEARCH};
static u32 touch_extra_key_index_u8180[4]  = {KEY_BACK, KEY_MENU, KEY_SEARCH, KEY_RESERVED};	/* for U8180 and C8510 */
static u32 touch_extra_key_index_u8160[4]  = {KEY_HOME, KEY_MENU, KEY_BACK, KEY_SEARCH};
/* delete touch_extra_key_region_u8180 and touch_extra_key_region_u8160 */

/* to record the key pressed */
static RECORD_EXTRA_KEYCODE  record_extra_keycode = {KEY_RESERVED, TRUE, TRUE, FALSE};
#endif

struct synaptics_ts_data {
	uint16_t addr;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct workqueue_struct *synaptics_wq;
	struct work_struct  work;
	int use_irq;
	struct hrtimer timer;	
	int (*power)(struct i2c_client* client, int on);
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	int reported_finger_count;
	int max_x;
	int max_y;
	int last_x;
	int last_y;
	int x_offset;
	int y_offset;
	bool is_first_point;
	bool use_touch_key;
	bool move_fast;
	bool is_surport_fingers;
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
	struct input_dev *key_input;
	struct timer_list key_timer;
#endif
};
struct synaptics_ts_data *ts = NULL;
#ifdef CONFIG_HAS_EARLYSUSPEND
static void synaptics_ts_early_suspend(struct early_suspend *h);
static void synaptics_ts_late_resume(struct early_suspend *h);
#endif

static int synaptics_ts_power(struct i2c_client *client, int on);

#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
/*===========================================================================
FUNCTION      is_in_extra_region
DESCRIPTION
              是否在附加TOUCH区
DEPENDENCIES
  None
RETURN VALUE
  true or false
SIDE EFFECTS
  None
===========================================================================*/
static bool is_in_extra_region(int pos_x, int pos_y)
{
    if (pos_x >= touch_extra_key_region->extra_touch_region.touch_x_start
        && pos_x <= touch_extra_key_region->extra_touch_region.touch_x_end
        && pos_y >= touch_extra_key_region->extra_touch_region.touch_y_start
        && pos_y <= touch_extra_key_region->extra_touch_region.touch_y_end)
    {
		SYNAPITICS_DEBUG("is_in_extra_region \n");
        return TRUE;
    }

    return FALSE;
}

/*===========================================================================
FUNCTION      touch_get_extra_keycode
DESCRIPTION
              取得附加区键值
DEPENDENCIES
  None
RETURN VALUE
  KEY_VALUE
SIDE EFFECTS
  None
===========================================================================*/
static u32 touch_get_extra_keycode(int pos_x, int pos_y)
{
    int i = 0;
    u32  touch_keycode = KEY_RESERVED;
    for (i=0; i < touch_key_number; i++)
    {
        if (abs(pos_x - touch_extra_key_region->extra_key[i].center_x) <= touch_extra_key_region->extra_key[i].x_width
         && abs(pos_y - touch_extra_key_region->extra_key[i].center_y) <= touch_extra_key_region->extra_key[i].y_width )
        {
	        touch_keycode = touch_extra_key_region->extra_key[i].touch_keycode;
	        break;
        }
    }
	
	SYNAPITICS_DEBUG("touch_keycode = %d \n",touch_keycode);
    return touch_keycode;
}

/*===========================================================================
FUNCTION      touch_pass_extra_keycode
DESCRIPTION:  
              附加区域键值上报处理
DEPENDENCIES
  None
RETURN VALUE
  None
SIDE EFFECTS
  None
===========================================================================*/
static void touch_pass_extra_keycode(struct synaptics_ts_data *ts)
{
    u32 key_code = record_extra_keycode.record_extra_key;

    if(KEY_RESERVED != key_code)
    {
        input_report_key(ts->key_input, key_code, !record_extra_keycode.bRelease);
        input_sync(ts->key_input);
		SYNAPITICS_DEBUG("input_report_key=%d, release=%d	\n", key_code, record_extra_keycode.bRelease);
    }

    return;
}

/*===========================================================================
FUNCTION      touch_extra_key_proc
DESCRIPTION:  
              定时器处理函数，确定是否上报记录键值的DOWN事件
DEPENDENCIES
  None
RETURN VALUE
  KEY_VALUE
SIDE EFFECTS
  None
===========================================================================*/
static void touch_extra_key_proc(struct synaptics_ts_data *ts)
{
    u32  key_tmp = KEY_RESERVED;

    /* 判断当前键值是否与记录键值为同一键值，
     * 如果是则上报该键值DOWN 事件
     */
     
    SYNAPITICS_DEBUG("touch_extra_key_proc   \n");
    key_tmp = touch_get_extra_keycode(ts->last_x, ts->last_y) ;

    if (key_tmp == record_extra_keycode.record_extra_key 
        && key_tmp != KEY_RESERVED)    
    {
        record_extra_keycode.bRelease = FALSE; 
        touch_pass_extra_keycode(ts);
        record_extra_keycode.bSentPress= TRUE; 
    }
    else
    {
        record_extra_keycode.bRelease = TRUE;
        record_extra_keycode.record_extra_key = KEY_RESERVED;
        record_extra_keycode.bSentPress= FALSE;  
    }

    return;
} 

/*===========================================================================
FUNCTION      ts_key_timer
DESCRIPTION:  
              键盘定时器，去除滑动造成判断错误
DEPENDENCIES
  None
RETURN VALUE
  None
SIDE EFFECTS
  None
===========================================================================*/
static void ts_key_timer(unsigned long arg)
{
    struct synaptics_ts_data *ts = (struct synaptics_ts_data *)arg;
	
	SYNAPITICS_DEBUG("ts_key_timer  \n");
	touch_extra_key_proc(ts);
}

static void ts_update_pen_state(struct synaptics_ts_data *ts, int x, int y, int pressure, int w)
{
    SYNAPITICS_DEBUG("ts_update_pen_state x=%d, y=%d pressure = %3d  \n", x, y, pressure);
    if (pressure) {
		if (ts->is_surport_fingers) {
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, pressure);
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
			input_mt_sync(ts->input_dev);
		} else {
			input_report_abs(ts->input_dev, ABS_X, x);
    	    input_report_abs(ts->input_dev, ABS_Y, y);
			input_report_abs(ts->input_dev, ABS_PRESSURE, pressure);
			input_report_key(ts->input_dev, BTN_TOUCH, !!pressure);
		}
    } else {
    	if (ts->is_surport_fingers) {
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, pressure);
			input_mt_sync(ts->input_dev);
    	} else {
			input_report_abs(ts->input_dev, ABS_PRESSURE, 0);
			input_report_key(ts->input_dev, BTN_TOUCH, 0);
    	}
    }

    input_sync(ts->input_dev);
}

/*===========================================================================
FUNCTION      update_pen_and_key_state
DESCRIPTION:  
              上报touch的坐标值或按键键值
DEPENDENCIES
  None
RETURN VALUE
  None
SIDE EFFECTS
  None
===========================================================================*/
static void update_pen_and_key_state(struct synaptics_ts_data *ts, int x, int y, int pressure, int w)
{
    u32  key_tmp = KEY_RESERVED;
    
    if(pressure)  /*press*/
    {
		SYNAPITICS_DEBUG("update_pen_and_key_state x=%d, y=%d   pressure = %d  \n", x, y, pressure);
		//pr_err("update_pen_and_key_state x=%d, y=%d   pressure = %d  \n", x, y, pressure);
        if(is_in_extra_region(ts->last_x, ts->last_y))
        {
            /* 如果记录键值还没有释放，则返回 */
			if ((FALSE == record_extra_keycode.bRelease && KEY_RESERVED != record_extra_keycode.record_extra_key)
				 || true == record_extra_keycode.touch_region_first )
            {
				SYNAPITICS_DEBUG("update_pen_and_key_state return  \n");
                if((ts->move_fast)&&(record_extra_keycode.touch_region_first == false))
                {
                     /* start timer */
                     mod_timer(&ts->key_timer,
                        jiffies + msecs_to_jiffies(TS_KEY_DEBOUNCE_TIMER_MS));
                }
                return;
            }

            key_tmp = touch_get_extra_keycode(x, y) ;

            if (KEY_RESERVED != key_tmp)
            {
                record_extra_keycode.record_extra_key = key_tmp;
                record_extra_keycode.bRelease = FALSE;
                record_extra_keycode.bSentPress = FALSE;
				SYNAPITICS_DEBUG("update_pen_and_key_state KEY_RESERVED != key_tmp  \n");

                /* start timer */
                mod_timer(&ts->key_timer,jiffies + msecs_to_jiffies(TS_KEY_DEBOUNCE_TIMER_MS));
            }
        }
        else
        {
			SYNAPITICS_DEBUG("update_pen_and_key_state pressure else \n");
			record_extra_keycode.touch_region_first = true;
            if (KEY_RESERVED != record_extra_keycode.record_extra_key 
                && FALSE == record_extra_keycode.bRelease
                && TRUE == record_extra_keycode.bSentPress)
            {
                /*当上报了键值后再进入touch区时，要上报release键*/
                record_extra_keycode.bRelease = TRUE;
                touch_pass_extra_keycode(ts);
                
                record_extra_keycode.bRelease = FALSE;
                record_extra_keycode.record_extra_key = KEY_RESERVED;
                record_extra_keycode.bSentPress= FALSE;
            }
			ts_update_pen_state(ts, x, y, pressure, w);
        }
    }
    else /*release*/
    {
		SYNAPITICS_DEBUG("update_pen_and_key_state  else x=%d, y=%d pressure = %d  \n", x, y, pressure);
        if(is_in_extra_region(ts->last_x, ts->last_y))
        {
			SYNAPITICS_DEBUG("update_pen_and_key_state	is_in_extra_region \n");
            del_timer(&ts->key_timer);
            if (KEY_RESERVED != record_extra_keycode.record_extra_key 
                && FALSE == record_extra_keycode.bRelease
                && TRUE == record_extra_keycode.bSentPress)
            {
                record_extra_keycode.bRelease = TRUE;
                touch_pass_extra_keycode(ts);
            }
            else
            {
				SYNAPITICS_DEBUG("update_pen_and_key_state	ts_update_pen_state \n");
                /* up 按键不可丢弃*/
                ts_update_pen_state(ts, x, y, pressure, w);
            }

            record_extra_keycode.bRelease = FALSE;
            record_extra_keycode.record_extra_key = KEY_RESERVED;
            record_extra_keycode.bSentPress= FALSE;
        }
        else
        {
			SYNAPITICS_DEBUG("update_pen_and_key_state	else else \n");
            ts_update_pen_state(ts, x, y, pressure, w);
        }

		record_extra_keycode.touch_region_first = false;
    }
}
#endif /*CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY*/

static int synaptics_init_panel_data(struct synaptics_ts_data *ts)
{
	int ret = 0;
	int i = 0;
	int value[4] = {0};

	ts->max_x = TS_X_MAX;
	ts->max_y = TS_Y_MAX;
	ts->is_first_point = true;
	if(board_surport_fingers(&ts->is_surport_fingers)){
		SYNAPITICS_DEBUG("%s: Cannot support multi fingers!\n", __FUNCTION__);
		ts->is_surport_fingers = false;
	}
	for(i=0; i<4; i++){
		value[i] = i2c_smbus_read_byte_data(ts->client, 0x2D+i);
		if (value[i] < 0) {
			ret = value[i];
			printk(KERN_ERR "i2c_smbus_read_byte_data failed\n");
			goto err_init_panel_data_failed;
		}
	}
	ts->max_x = value[0] | (value[1] << 8);
	ts->max_y = value[2] | (value[3] << 8);

	ts->x_offset = 4*(ts->max_x/LCD_X_MAX);  /*4 pix*/

	ts->y_offset = ts->x_offset;

	if(board_use_tssc_touch(&ts->use_touch_key)){
		SYNAPITICS_DEBUG("%s: Cannot support touch_keypad!\n", __FUNCTION__);
		ts->use_touch_key = false;
	}
#ifndef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    ts->use_touch_key = false;
#endif
	printk(KERN_INFO "%s: use_touch_key=%d, is_surport_fingers=%d,\n TS_X_MAX=%d, TX_Y_MAX=%d, offset=%d\n",
	                 __FUNCTION__, ts->use_touch_key, ts->is_surport_fingers, ts->max_x, ts->max_y, ts->x_offset);
    
err_init_panel_data_failed:
	return ret;
}

static void synaptics_ts_work_func(struct work_struct *work)
{
	int i;
	int ret;
	int bad_data = 0;
	struct i2c_msg msg[2];
	uint8_t start_reg;
	uint8_t buf[18];
	uint16_t x, y;
	uint8_t z,wx,wy;
	uint8_t finger;
	uint8_t gesture0;
	uint8_t gesture1;
	uint8_t device_st;
	uint8_t irq_st;

	uint16_t x2, y2, wx2,wy2,z2;
	uint8_t finger2_pressed = 0;

	struct synaptics_ts_data *ts = container_of(work, struct synaptics_ts_data, work);

	start_reg = F01_RMI_DATA00;
	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &start_reg;
	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;
	SYNAPITICS_DEBUG("synaptics_ts_work_func\n"); 

	for (i = 0; i < ((ts->use_irq && !bad_data) ? 1 : 5); i++) { 	
		ret = i2c_transfer(ts->client->adapter, msg, 2);
		if (ret < 0) 
		{
			pr_err("%d times i2c_transfer failed\n",i);
			bad_data = 1;
			continue;
		}
		else
		{
		    bad_data = 0;
		}
		if (i == 5) 
		{
			pr_err("%d times i2c_transfer error\n", i);
			if (ts->power) {
				ret = ts->power(ts->client,1);
				if (ret < 0)
					pr_err("%s:synaptics_ts_resume power off failed\n", __FUNCTION__);
			}
			break;
		}
		
		if (ret < 0) {
			SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_work_func: i2c_transfer failed\n");
			bad_data = 1;
		} else {
			bad_data = 0;	
			device_st = buf[0];
			irq_st = buf[1];
			x = (buf[5] & 0x0f) | ((uint16_t)buf[3] << 4); /* x aixs */ 
			y= ((buf[5] & 0xf0) >> 4) | ((uint16_t)buf[4] << 4);  /* y aixs */ 
			z = buf[7];				    /* pressure */	
			wx = buf[6] & 0x0f;			    /* x width */
			wy = (buf[6] & 0xf0) >> 4;			    /* y width */
			finger = buf[2] & 0x0f;                        /* numbers of fingers */  
			gesture0 = buf[13];		            /* code of gesture */ 
			gesture1 = buf[14];                         /* enhanced data of gesture */  
			SYNAPITICS_DEBUG("device_st = 0x%x irq_st = 0x%x x = %d y = %d z = %d wx = %d wy = %d finger = %d gesture0 = 0x%x gesture1 = 0x%x\n",
					device_st, irq_st, x, y, z, wx, wy, finger,gesture0,gesture1);
			x2 = (buf[10] & 0x0f) | ((uint16_t)buf[8] << 4);
			y2 = ((buf[10] & 0xf0) >> 4) | ((uint16_t)buf[9] << 4);  /* y2 aixs */ 
			z2 = buf[12];
			wx2 = buf[11] & 0x0f;			    /* x width */ 	
			wy2 = (buf[11] & 0xf0) >> 4;			    /* y width */
			finger2_pressed = (finger&0x0C)>>2;
			SYNAPITICS_DEBUG("finger0_state = 0x%x, x = %d y = %d z = %d wx = %d wy = %d\n",
					finger&0x03, x, y, z, wx, wy);
			SYNAPITICS_DEBUG("finger1_state = 0x%x, x2 = %d y2 = %d z2 = %d wx2 = %d wy2 = %d\n",
					(finger&0x0C)>>2, x2, y2, z2, wx2, wy2);
			if(machine_is_msm7x25_c8500() || machine_is_msm7x25_c8150() \
			   || machine_is_msm7x25_u8130() || machine_is_msm7x25_u8160() \
			   || machine_is_msm7x25_u8150() || machine_is_msm7x25_u8159() \
			   || machine_is_msm7x25_c8510() || machine_is_msm7x25_u8160() )
			{
			  x2 = x2;
			  y2 = ts->max_y - y2;

			  x = x;
			  y = ts->max_y- y;
			}

			if(ts->is_surport_fingers)
			{
				if ((!ts->use_touch_key)||(finger2_pressed)||(ts->reported_finger_count)) {
					if (!finger) {
						z = 0;
					}
					input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, z);
					input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, (wx+wy)/2);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
					input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
					input_mt_sync(ts->input_dev);
					if (finger2_pressed) {
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, z2);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, (wx2+wy2)/2);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x2);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y2);
						input_mt_sync(ts->input_dev);
						ts->reported_finger_count = finger;
					} else if (ts->reported_finger_count > 3) {		/* second finger press last time */
						input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0);
						input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0);
						input_mt_sync(ts->input_dev);
						ts->reported_finger_count = 0;
					}
					input_sync(ts->input_dev);
//----------------------------------------------
//Remove the jump
//that causing Touchpanel not working
//----------------------------------------------
				} 
			}
            
			if (z) 
			{
				/* 
				* always report the first point  whether slip	or click
				*/ 
				if (ts->is_first_point) {

					SYNAPITICS_DEBUG("is_first_point  \n");
					ts->last_x = x;
					ts->last_y = y;
					ts->move_fast = false;
					ts->is_first_point = false;
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
					if (ts->use_touch_key) {
						update_pen_and_key_state(ts, x, y, z, (wx+wy)/2);
					} else
#endif
					{
						input_report_abs(ts->input_dev, ABS_X, x);
						input_report_abs(ts->input_dev, ABS_Y, y);
					}
				} else {
					SYNAPITICS_DEBUG("else is_first_point  \n");
					if((abs(ts->last_x - x) > ts->x_offset) || (abs(ts->last_y - y) > ts->y_offset))
					{
						if(abs(ts->last_y - y) > (6*ts->max_y/LCD_Y_MAX))
						{
							ts->move_fast = true;
						}
						else
						{
							ts->move_fast = false;
						}
						ts->last_x = x;
						ts->last_y = y;
						
						SYNAPITICS_DEBUG("else update_pen_and_key_state  \n");
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
						if (ts->use_touch_key) {
							update_pen_and_key_state(ts, x, y, z, (wx+wy)/2);
						} else
#endif
						{
							input_report_abs(ts->input_dev, ABS_X, x);
							input_report_abs(ts->input_dev, ABS_Y, y);
						}
					}
				}
			}
			else
			{
				/* 
				* The next point must be first point whether slip or click after 
				* this up event
				*/ 		
				ts->is_first_point = true;
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
				if (ts->use_touch_key) {
					update_pen_and_key_state(ts, x, y, z, (wx+wy)/2);
				}
#endif
			}

			if(!ts->use_touch_key)
			{
				input_report_abs(ts->input_dev, ABS_PRESSURE, z);
				//input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, w);
				input_report_key(ts->input_dev, BTN_TOUCH, finger);
				input_sync(ts->input_dev);
			}
		}

	}
	
//----------------------------------------------
//Remove the jump
//that causing Touchpanel not working
//----------------------------------------------
	if (ts->use_irq) {
		enable_irq(ts->client->irq);
		SYNAPITICS_DEBUG("enable irq\n");
	}
}
static enum hrtimer_restart synaptics_ts_timer_func(struct hrtimer *timer)
{
	struct synaptics_ts_data *ts = container_of(timer, struct synaptics_ts_data, timer);
	SYNAPITICS_DEBUG("synaptics_ts_timer_func\n");
	queue_work(ts->synaptics_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

static irqreturn_t synaptics_ts_irq_handler(int irq, void *dev_id)
{
	struct synaptics_ts_data *ts = dev_id;
	/* gaohuajiang modify for kernel 2.6.32 20100514 */
	disable_irq_nosync(ts->client->irq);
//	disable_irq(ts->client->irq);
	SYNAPITICS_DEBUG("synaptics_ts_irq_handler,disable irq\n");
	queue_work(ts->synaptics_wq, &ts->work);
	return IRQ_HANDLED;
}

//touch_extra_key_region_normal =
//{
//    {0, ts->max_x, ts->y_start, ts->max_y},				/* extra region: X_START, X_END, Y_START, Y_END */
//    {
//       {(ts->max_x*1/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_BACK},  /* back key */
//       {(ts->max_x*3/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_MENU},  /* menu key */
//       {(ts->max_x*5/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_HOME},  /* home key */
//       {(ts->max_x*7/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_SEARCH},  /* Search key */
//   },
//};
//touch_extra_key_region_u8180 =
//{
//    {0, ts->max_x, ts->y_start, ts->max_y},				/* extra region */
//    {
//       {(ts->max_x*1/6),   (ts->max_y-TS_KEY_Y_MAX/2+80), 226, TS_KEY_Y_MAX/2, KEY_BACK},    /* back key */
//       {(ts->max_x*3/6),   (ts->max_y-TS_KEY_Y_MAX/2+80), 226, TS_KEY_Y_MAX/2, KEY_MENU},    /* menu key */
//       {(ts->max_x*5/6),   (ts->max_y-TS_KEY_Y_MAX/2+80), 226, TS_KEY_Y_MAX/2, KEY_SEARCH},  /* Search key */
//    },
//};
//touch_extra_key_region_u8160 =
//{
//   {0, ts->max_x, ts->y_start, ts->max_y},				/* extra region */
//   {
//      {(ts->max_x*1/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_HOME},  /* home key */
//      {(ts->max_x*3/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_MENU},  /* menu key */
//      {(ts->max_x*5/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_BACK},  /* back key */
//      {(ts->max_x*7/8),   (ts->max_y-TS_KEY_Y_MAX/2+80), 169, TS_KEY_Y_MAX/2, KEY_SEARCH},  /* Search key */
//    },
//};
static void synaptics_data_init(struct synaptics_ts_data *ts, int key_num, u32 *key_index)
{
	int i;
	
	touch_extra_key_region_normal.extra_touch_region.touch_x_start = 0;
	touch_extra_key_region_normal.extra_touch_region.touch_x_end = ts->max_x;
	touch_extra_key_region_normal.extra_touch_region.touch_y_start = (ts->max_y)-TS_KEY_Y_MAX+50;	/* y start */
	touch_extra_key_region_normal.extra_touch_region.touch_y_end = ts->max_y;
	
	for (i=0; i< key_num; i++) {
		touch_extra_key_region_normal.extra_key[i].center_x = (ts->max_x)*(2*i + 1) / (2*key_num);
		touch_extra_key_region_normal.extra_key[i].center_y = (ts->max_y)-TS_KEY_Y_MAX/2+80;
		if (key_num == 3) {
			touch_extra_key_region_normal.extra_key[i].x_width = TOUCH_KEY_X_REGION_3key;
		} else if (key_num == 4) {
			touch_extra_key_region_normal.extra_key[i].x_width = TOUCH_KEY_X_REGION_4key;
		}
		touch_extra_key_region_normal.extra_key[i].y_width = TS_KEY_Y_MAX/2;
		touch_extra_key_region_normal.extra_key[i].touch_keycode = key_index[i];
	}	
}

static int synaptics_ts_probe(
	struct i2c_client *client, const struct i2c_device_id *id)
{
	//delete this pointer,replace it with global pointer
	//delete struct vreg *v_gp5
	int ret = 0;
	int gpio_config;
	int i;
	struct synaptics_i2c_rmi_platform_data *pdata;
	
	if (touch_is_supported())
		return -ENODEV;

	SYNAPITICS_DEBUG(" In synaptics_ts_probe: \n");
	
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("synaptics_ts_probe: need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}
	//delete gp5 power on
    /* driver  detect its device  */  
	for(i = 0; i < 3; i++) {
		if(machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
		{
			ret = i2c_smbus_read_byte_data(client, 0x6D);
		}
		else
		{
			ret = i2c_smbus_read_byte_data(client, F01_RMI_QUERY00);
		}

		if (ret == Manufacturer_ID){
			SYNAPITICS_DEBUG("synaptics_ts manufacturer id = %d\n", ret); 
			goto succeed_find_device;
		}
	}
	if( i == 3) {
		pr_err("no synaptics-tm device\n ");	
		goto err_find_touchpanel_failed;
	}

succeed_find_device:
	set_touch_support(true);
	ret = gpio_tlmm_config(GPIO_CFG(96, 0, GPIO_INPUT, GPIO_PULL_DOWN, GPIO_2MA), GPIO_ENABLE);
	if (ret < 0)
	{
		SYNAPITICS_DEBUG("synaptics_ts_config TS_RESET_GPIO failed\n");
	}

	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	ts->client = client;
	i2c_set_clientdata(client, ts);

	pdata = client->dev.platform_data;

	ts->power = synaptics_ts_power;
	if (ts->power) {
		ret = ts->power(ts->client, 1);
		if (ret < 0) {
			pr_err("synaptics_ts_probe reset failed\n");
			goto err_power_failed;
		}
	}
//----------------------------------------------
//Removing firmware update function
//to preventing from wrong update
//----------------------------------------------
	ret = i2c_smbus_write_byte_data(ts->client, F01_RMI_CTRL01_00, 0); /* disable interrupt */
	if(ret < 0){
	    pr_err("%s: fail to disable interrupt!\n", __FUNCTION__);
	    goto err_power_failed;
	}

	ret = synaptics_init_panel_data(ts);
	if(ret < 0){
   	    pr_err("%s: fail to synaptics_init_panel_data()!\n", __FUNCTION__);
	    goto err_power_failed;
	}
	
	ts->synaptics_wq = create_singlethread_workqueue("synaptics_wq");
	if (!ts->synaptics_wq) {
		pr_err("create synaptics_wq error\n");
		ret = -ENOMEM;
		goto err_destroy_wq;
	}
	INIT_WORK(&ts->work, synaptics_ts_work_func);
	
	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		pr_err("synaptics_ts_probe: Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "synaptics-rmi-touchscreen";
	
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_2, ts->input_dev->keybit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	input_set_abs_params(ts->input_dev, ABS_X, 0, ts->max_x, 0, 0);
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
	if(ts->use_touch_key)
	{
		input_set_abs_params(ts->input_dev, ABS_Y, 0, ts->max_y-TS_KEY_Y_MAX, 0, 0);
	}
	else
#endif
	{
		input_set_abs_params(ts->input_dev, ABS_Y, 0, ts->max_y, 0, 0);
	}    
	input_set_abs_params(ts->input_dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_TOOL_WIDTH, 0, 15, 0, 0);

	if(ts->is_surport_fingers)
	{
		input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->max_x, 0, 0);
	#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
		if(ts->use_touch_key)
		{
			input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, (ts->max_y-TS_KEY_Y_MAX), 0, 0);
		}
		else
	#endif
		{
			input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->max_y, 0, 0);
		}    
		input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
		input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
	}
	ret = input_register_device(ts->input_dev);
	if (ret) {
		SYNAPITICS_DEBUG(KERN_ERR "synaptics_ts_probe: Unable to register %s input device\n", ts->input_dev->name);
		goto err_input_register_device_failed;
	}
	
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
	if(ts->use_touch_key)
	{
		if(machine_is_msm7x25_u8160())
		{
			touch_key_number = EXTRA_MAX_TOUCH_KEY;		/* 4 touch key */
			synaptics_data_init(ts, touch_key_number, touch_extra_key_index_u8160);
		}
		else if (machine_is_msm7x25_u8130() || machine_is_msm7x25_c8510())
		{
			touch_key_number = EXTRA_MAX_TOUCH_KEY_U8180;		/* 3 touch key */
			synaptics_data_init(ts, touch_key_number, touch_extra_key_index_u8180);
		}
		else
		{
			touch_key_number = EXTRA_MAX_TOUCH_KEY;		/* 4 touch key */
			synaptics_data_init(ts, touch_key_number, touch_extra_key_index_normal);
		}
		touch_extra_key_region = &touch_extra_key_region_normal;

		ts->key_input = input_allocate_device();
		if (!ts->key_input  || !ts) {
			ret = -ENOMEM;
			goto err_key_input_dev_alloc_failed;
		}
		/*rename touchscreen-keypad to use touchscreen-keypad's menu to entry safe mode*/
		ts->key_input->name = "touchscreen-keypad";
		
		set_bit(EV_KEY, ts->key_input->evbit);
		for (i = 0; i < touch_key_number; i++)
		{
			set_bit(touch_extra_key_region->extra_key[i].touch_keycode & KEY_MAX, ts->key_input->keybit);
		}

		ret = input_register_device(ts->key_input);
		if (ret)
			goto err_key_input_register_device_failed;

		setup_timer(&ts->key_timer, ts_key_timer, (unsigned long)ts);
	}
#endif
	
	gpio_config = GPIO_CFG(GPIO_TOUCH_INT, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA);
	ret = gpio_tlmm_config(gpio_config, GPIO_ENABLE);
	if (ret) 
	{
		ret = -EIO;
		pr_err("%s: gpio_tlmm_config(#%d)=%d\n", __func__, GPIO_TOUCH_INT, ret);
		goto err_key_input_register_device_failed;
	}
	if (gpio_request(GPIO_TOUCH_INT, "synaptics_ts_int\n"))
		pr_err("failed to request gpio synaptics_ts_int\n");
	
	ret = gpio_configure(GPIO_TOUCH_INT, GPIOF_INPUT | IRQF_TRIGGER_LOW);/*gpio 29is interupt for touchscreen.*/
	if (ret) {
		pr_err("synaptics_ts_probe: gpio_configure %d failed\n", GPIO_TOUCH_INT);
		goto err_key_input_register_device_failed;
	}

	if (client->irq) {
		ret = request_irq(client->irq, synaptics_ts_irq_handler, 0, client->name, ts);
		if (ret == 0) {
			ret = i2c_smbus_write_byte_data(ts->client, F01_RMI_CTRL01_00, 0x07); /* enable  int */
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
	ts->reported_finger_count = 0;

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_TOUCH_PANEL);
    #endif
    
	printk(KERN_INFO "synaptics_ts_probe: Start touchscreen %s in %s mode\n", ts->input_dev->name, ts->use_irq ? "interrupt" : "polling");

	return 0;

err_key_input_register_device_failed:
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    if(ts->use_touch_key)
    {
	    input_free_device(ts->key_input);
	}
#endif

err_key_input_dev_alloc_failed:
err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
err_destroy_wq:
   	destroy_workqueue(ts->synaptics_wq);
    
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
		ret = i2c_smbus_write_byte_data(client, F01_RMI_CTRL00, 0x80);/*sensor on*/	
		if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics sensor can not wake up\n");
		if(machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
		{
		  ret = i2c_smbus_write_byte_data(client, 0x62, 0x01);/*touchscreen reset*/
		  if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics chip can not reset\n");

		  msleep(200); /* wait for device reset; */
		}
		else
		{
		  ret = i2c_smbus_write_byte_data(client, F01_RMI_CMD00, 0x01);/*touchscreen reset*/
		  if (ret < 0)
			SYNAPITICS_DEBUG(KERN_ERR "synaptics chip can not reset\n");

		  msleep(200); /* wait for device reset; */
		}
	}
	else {
		ret = i2c_smbus_write_byte_data(client, F01_RMI_CTRL00, 0x81); /* set touchscreen to deep sleep mode*/
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

#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
	if(ts->use_touch_key)
	{
	   input_unregister_device(ts->key_input);
	}
#endif

	if(ts->synaptics_wq)
	{
	   destroy_workqueue(ts->synaptics_wq); 
	}
	
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
	ret = i2c_smbus_write_byte_data(ts->client, F01_RMI_CTRL01_00, 0); /* disable interrupt */
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
	int ret = 0;
	struct synaptics_ts_data *ts = i2c_get_clientdata(client);

	SYNAPITICS_DEBUG("In synaptics_ts_resume\n");
	if (ts->power) {
		ret = ts->power(client, 1);
		if (ret < 0)
		{
			SYNAPITICS_DEBUG("synaptics_ts_resume power on failed\n");
		}
	}
	
	if (ts->use_irq) {
		enable_irq(client->irq);
		ret =i2c_smbus_write_byte_data(ts->client, F01_RMI_CTRL01_00, 0x07); /* enable abs int */
		{
			if (ret < 0)
			{
				SYNAPITICS_DEBUG("enable asb interrupt failed\n");		
				return ret;	
			}
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
//----------------------------------------------
//Removing firmware update function
//to preventing from wrong update
//----------------------------------------------
static const struct i2c_device_id synaptics_ts_id[] = {
	{ "synaptics-tm", 0 },
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
		.name	= "synaptics-tm",
	},
};

static int __devinit synaptics_ts_init(void)
{
	return i2c_add_driver(&synaptics_ts_driver);
}

static void __exit synaptics_ts_exit(void)
{
	i2c_del_driver(&synaptics_ts_driver);
}

module_init(synaptics_ts_init);
module_exit(synaptics_ts_exit);

MODULE_DESCRIPTION("Synaptics Touchscreen Driver");
MODULE_LICENSE("GPL");

