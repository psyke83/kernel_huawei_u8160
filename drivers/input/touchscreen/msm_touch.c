/* drivers/input/touchscreen/msm_touch.c
 *
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/earlysuspend.h>

#include <mach/msm_touch.h>

#include <linux/hardware_self_adapt.h>

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
//#define CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY

#define TRACE_MSM_TOUCH 0
#if TRACE_MSM_TOUCH
#define TSSC(x...) printk(KERN_ERR "[TSSC] " x)
#else
#define TSSC(x...) do {} while (0)
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

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* HW register map */
#define TSSC_CTL_REG      0x100
#define TSSC_SI_REG       0x108
#define TSSC_OPN_REG      0x104
#define TSSC_STATUS_REG   0x10C
#define TSSC_AVG12_REG    0x110

/* status bits */
#define TSSC_STS_OPN_SHIFT 0x6
#define TSSC_STS_OPN_BMSK  0x1C0
#define TSSC_STS_NUMSAMP_SHFT 0x1
#define TSSC_STS_NUMSAMP_BMSK 0x3E

/* CTL bits */
#define TSSC_CTL_EN     (0x1 << 0)
#define TSSC_CTL_SW_RESET   (0x1 << 2)
#define TSSC_CTL_MASTER_MODE    (0x3 << 3)
#define TSSC_CTL_AVG_EN     (0x1 << 5)
#define TSSC_CTL_DEB_EN     (0x1 << 6)
#define TSSC_CTL_DEB_12_MS  (0x2 << 7)  /* 1.2 ms */
#define TSSC_CTL_DEB_16_MS  (0x3 << 7)  /* 1.6 ms */
#define TSSC_CTL_DEB_2_MS   (0x4 << 7)  /* 2 ms */
#define TSSC_CTL_DEB_3_MS   (0x5 << 7)  /* 3 ms */
#define TSSC_CTL_DEB_4_MS   (0x6 << 7)  /* 4 ms */
#define TSSC_CTL_DEB_6_MS   (0x7 << 7)  /* 6 ms */
#define TSSC_CTL_INTR_FLAG1 (0x1 << 10)
#define TSSC_CTL_DATA       (0x1 << 11)
#define TSSC_CTL_SSBI_CTRL_EN   (0x1 << 13)

/* control reg's default state */
#define TSSC_CTL_STATE    ( \
        TSSC_CTL_DEB_12_MS | \
        TSSC_CTL_DEB_EN | \
        TSSC_CTL_AVG_EN | \
        TSSC_CTL_MASTER_MODE | \
        TSSC_CTL_EN)

#define TSSC_NUMBER_OF_OPERATIONS 2
#define TS_PENUP_TIMEOUT_MS 20
#define TS_KEY_DEBOUNCE_TIMER_MS 70
#define TS_PENUP_COUNTER   (60/TS_PENUP_TIMEOUT_MS)

#define TS_DRIVER_NAME "msm_touchscreen"

#define X_MAX   1024
#define Y_MAX   1024
#define P_MAX   256
#define TOUCH_KEY_NULL_REGION  16
#define TOUCH_KEY_MAX_REGION    (Y_MAX - 824)

#define TOUCH_OFFSET    4

struct ts {
    struct input_dev *input;
    struct timer_list timer;
    int irq;
    unsigned int x_max;
    unsigned int y_max;
    struct early_suspend early_suspend;

    /*touch extra param*/
    u32 pen_up_count;
    u32 last_x;
    u32 last_y;
    u32 save_x;
    u32 save_y;
    bool is_first_point;
    bool use_touch_key;
    bool move_fast;
    
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    /*touch key param*/
    struct timer_list key_timer;
    struct input_dev *key_input;
#endif
};

static void __iomem *virt;
#define TSSC_REG(reg) (virt + TSSC_##reg##_REG)

#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
#define EXTRA_MAX_TOUCH_KEY    3

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

#define EXTRA_X_START    (5)
#define EXTRA_X_END      (X_MAX-5) 
#define EXTRA_Y_START    (Y_MAX-120)
#define EXTRA_Y_END      (Y_MAX-1)

/* to init extra region and touch virt key region */
static extra_key_region   touch_extra_key_region =
{
    {EXTRA_X_START, EXTRA_X_END,EXTRA_Y_START,EXTRA_Y_END},                          /* extra region */
    {
       {(X_MAX/6),   (Y_MAX-52), 130, 52, KEY_HOME},  /* home key */
       {(X_MAX/2),   (Y_MAX-52), 130, 52, KEY_MENU},  /* menu key */
       {(X_MAX*5/6), (Y_MAX-52), 130, 52, KEY_BACK},  /* back key */
    },
};

/* to record keycode */
typedef struct 
{
    u32                 record_extra_key;             /*key value*/
    bool                bRelease;                     /*be released?*/
    bool                bSentPress;                    
    bool                touch_region_first;           /* to record first touch event*/
} RECORD_EXTRA_KEYCODE;

/* to record the key pressed */
static RECORD_EXTRA_KEYCODE  record_extra_keycode = {KEY_RESERVED, TRUE, TRUE, FALSE};

static void ts_update_pen_state(struct ts *ts, int x, int y, int pressure);


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
    if (pos_x > touch_extra_key_region.extra_touch_region.touch_x_start
        && pos_x < touch_extra_key_region.extra_touch_region.touch_x_end
        && pos_y > touch_extra_key_region.extra_touch_region.touch_y_start
        && pos_y < touch_extra_key_region.extra_touch_region.touch_y_end)
    {
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
    for (i=0; i<EXTRA_MAX_TOUCH_KEY; i++)
    {
        if (abs(pos_x - touch_extra_key_region.extra_key[i].center_x) < touch_extra_key_region.extra_key[i].x_width
         && abs(pos_y - touch_extra_key_region.extra_key[i].center_y) < touch_extra_key_region.extra_key[i].y_width )
        {
        touch_keycode = touch_extra_key_region.extra_key[i].touch_keycode;
        break;
        }
    }

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
static void touch_pass_extra_keycode(struct ts *ts)
{
    u32 key_code = record_extra_keycode.record_extra_key;

    if(KEY_RESERVED != key_code)
    {
        input_report_key(ts->key_input, key_code, !record_extra_keycode.bRelease);
        input_sync(ts->key_input);
    }

    TSSC("***EXTRA KEY TEST:key_code=%d, release=%d***\n", key_code, record_extra_keycode.bRelease);

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
static void touch_extra_key_proc(struct ts *ts)
{
    u32  key_tmp = KEY_RESERVED;

    /* 判断当前键值是否与记录键值为同一键值，
     * 如果是则上报该键值DOWN 事件
     */
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
    struct ts *ts = (struct ts *)arg;

    TSSC("%s: key timer out!\n", __FUNCTION__);
    touch_extra_key_proc(ts);
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
static void update_pen_and_key_state(struct ts *ts, int x, int y, int pressure)
{
    u32  key_tmp = KEY_RESERVED;
    
    if(pressure)  /*press*/
    {
        if(is_in_extra_region(ts->last_x, ts->last_y))
        {
            /* 如果记录键值还没有释放，则返回 */
            if ((FALSE == record_extra_keycode.bRelease && KEY_RESERVED != record_extra_keycode.record_extra_key)
                || true == record_extra_keycode.touch_region_first )
            {
                if((ts->move_fast)&&(record_extra_keycode.touch_region_first == false))
                {
                     /* start timer */
                     TSSC("move fast, reset mod_key_timer. \n");
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

                /* start timer */
                mod_timer(&ts->key_timer,
                    jiffies + msecs_to_jiffies(TS_KEY_DEBOUNCE_TIMER_MS));
            }
        }
        else
        {
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
            ts_update_pen_state(ts, x, y, pressure);
        }
    }
    else /*release*/
    {
        
        if(is_in_extra_region(ts->last_x, ts->last_y))
        {
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
                /* up 按键不可丢弃*/
                ts_update_pen_state(ts, x, y, pressure);
            }

            record_extra_keycode.bRelease = FALSE;
            record_extra_keycode.record_extra_key = KEY_RESERVED;
            record_extra_keycode.bSentPress= FALSE;
        }
        else
        {
            ts_update_pen_state(ts, x, y, pressure);
        }

        record_extra_keycode.touch_region_first = false;
    }
}
#endif /*CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY*/

static void ts_update_pen_state(struct ts *ts, int x, int y, int pressure)
{
    TSSC("{%d, %d}, pressure = %3d\n", x, y, pressure);
    if (pressure) {
        input_report_abs(ts->input, ABS_X, x);
        input_report_abs(ts->input, ABS_Y, y);
        input_report_abs(ts->input, ABS_PRESSURE, pressure);
        input_report_key(ts->input, BTN_TOUCH, !!pressure);
    } else {
        input_report_abs(ts->input, ABS_PRESSURE, 0);
        input_report_key(ts->input, BTN_TOUCH, 0);
    }

    input_sync(ts->input);
}

static void ts_timer(unsigned long arg)
{
    struct ts *ts = (struct ts *)arg;
    
    /* Data has been read, OK to clear the data flag */
    writel(TSSC_CTL_STATE, TSSC_REG(CTL));
    
    /*detect pen up status*/
    if(ts->pen_up_count++ >= TS_PENUP_COUNTER)
    {
        if(ts->use_touch_key)
        {
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
            update_pen_and_key_state(ts, 0, 0, 0);
#else
            ts_update_pen_state(ts, 0, 0, 0);
#endif
        }
        else
        {
            ts_update_pen_state(ts, 0, 0, 0);
        }

        ts->is_first_point = true;
    }
    else
    {
        mod_timer(&ts->timer,
            jiffies + msecs_to_jiffies(TS_PENUP_TIMEOUT_MS));
    }
}

static int sqr (int x)
{
    return x * x;
}

static irqreturn_t ts_interrupt(int irq, void *dev_id)
{
    u32 avgs, x, y, lx, ly;
    u32 num_op, num_samp;
    u32 status;

    struct ts *ts = dev_id;

    status = readl(TSSC_REG(STATUS));
    avgs = readl(TSSC_REG(AVG12));
    x = avgs & 0xFFFF;
    y = avgs >> 16;

    ts->pen_up_count = 0;
    TSSC("ts_interrupt: status=0x%x, avgs=0x%x\n", status, avgs);
    /* For pen down make sure that the data just read is still valid.
     * The DATA bit will still be set if the ARM9 hasn't clobbered
     * the TSSC. If it's not set, then it doesn't need to be cleared
     * here, so just return.
     */
    if (!(readl(TSSC_REG(CTL)) & TSSC_CTL_DATA))
        goto out;

    /* Data has been read, OK to clear the data flag */
    //writel(TSSC_CTL_STATE, TSSC_REG(CTL));

    /* Valid samples are indicated by the sample number in the status
     * register being the number of expected samples and the number of
     * samples collected being zero (this check is due to ADC contention).
     */
    num_op = (status & TSSC_STS_OPN_BMSK) >> TSSC_STS_OPN_SHIFT;
    num_samp = (status & TSSC_STS_NUMSAMP_BMSK) >> TSSC_STS_NUMSAMP_SHFT;

    if ((num_op == TSSC_NUMBER_OF_OPERATIONS) && (num_samp == 0)) {
        /* TSSC can do Z axis measurment, but driver doesn't support
         * this yet.
         */
         
        /*
         * REMOVE THIS:
         * These x, y co-ordinates adjustments will be removed once
         * Android framework adds calibration framework.
         */
#ifdef CONFIG_ANDROID_TOUCHSCREEN_MSM_HACKS
        lx = ts->x_max - x;
        ly = ts->y_max - y;
#else
        lx = x;
        ly = y;
#endif

        TSSC("%s: x=%d, y=%d\n", __FUNCTION__, lx, ly);
        
        /*get rid of some wrong point*/
        if((lx == 0) || (lx == X_MAX) || (ly == 0) || (ly == Y_MAX))
        {
            TSSC("ts_outscale: x=%d, y=%d\n", lx, ly);
        }
        else
        {
            if(ts->is_first_point)
            {     
                ts->last_x = ts->save_x = lx;
                ts->last_y = ts->save_y = ly;
                ts->move_fast = false;
                ts->is_first_point = false; 
                if(ts->use_touch_key)
                {
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
                    update_pen_and_key_state(ts, lx, ly, 255);
#else
                    ts_update_pen_state(ts, lx, ly, 255);
#endif
                }
                else
                {
                    ts_update_pen_state(ts, lx, ly, 255);
                }
            }
            else
            {
                if(((max(ts->last_x, lx)-min(ts->last_x, lx)) > TOUCH_OFFSET)
                    || ((max(ts->last_y, ly)-min(ts->last_y, ly)) > TOUCH_OFFSET) )
                {
                    ts->last_x = lx;
                    ts->last_y = ly;

                    if(sqr((int)ts->last_y - (int)ts->save_y) > 60)
                    {
                        ts->move_fast = true;
                    }
                    else
                    {
                        ts->move_fast = false;
                    }
                    ts->save_y = ly;
                    ts->save_x = lx;
                    
                    if(ts->use_touch_key)
                    {
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
                        update_pen_and_key_state(ts, lx, ly, 255);
#else
                        ts_update_pen_state(ts, lx, ly, 255);
#endif
                    }
                    else
                    {
                        ts_update_pen_state(ts, lx, ly, 255);
                    }
                }
            }
        }
        //ts_update_pen_state(ts, lx, ly, 255);
        /* kick pen up timer - to make sure it expires again(!) */
        mod_timer(&ts->timer,
            jiffies + msecs_to_jiffies(TS_PENUP_TIMEOUT_MS));

    } else
        printk(KERN_INFO "Ignored interrupt: {%3d, %3d},"
                " op = %3d samp = %3d\n",
                 x, y, num_op, num_samp);

out:
    return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_early_suspend(struct early_suspend *h)
{
    struct ts *ts;
    ts = container_of(h, struct ts, early_suspend);

    del_timer_sync(&ts->timer);

#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    if(ts->use_touch_key)
    {
        del_timer_sync(&ts->key_timer);
    }
#endif
    
    writel(0, TSSC_REG(CTL));

    disable_irq(ts->irq);

    TSSC("%s:msm_touch early suspend!\n", __FUNCTION__);
}

static void ts_late_resume(struct early_suspend *h)
{
    struct ts *ts;
    ts = container_of(h, struct ts, early_suspend);

    enable_irq(ts->irq);
    /* Data has been read, OK to clear the data flag */
    writel(TSSC_CTL_STATE, TSSC_REG(CTL));

    TSSC("%s:msm_touch late resume\n!", __FUNCTION__);
}
#endif

static int __devinit ts_probe(struct platform_device *pdev)
{
    int result;
    struct input_dev *input_dev;
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    struct input_dev *key_dev;
#endif
    struct resource *res, *ioarea;
    struct ts *ts;
    unsigned int x_max, y_max, pressure_max;
    struct msm_ts_platform_data *pdata = pdev->dev.platform_data;
  
    /* The primary initialization of the TS Hardware
     * is taken care of by the ADC code on the modem side
     */

    ts = kzalloc(sizeof(struct ts), GFP_KERNEL);
    input_dev = input_allocate_device();
    if (!input_dev || !ts) {
        result = -ENOMEM;
        goto fail_alloc_mem;
    }

    ts->is_first_point = true;
    ts->pen_up_count = 0;
    /*if use touch for keycode, set true*/
    ts->use_touch_key = false;

    if(board_use_tssc_touch(&ts->use_touch_key))
    {
        printk(KERN_ERR "%s: Cannot support MSM_TOUCH!\n", __FUNCTION__);
        result = -ENODEV;
        goto fail_alloc_mem;
    }

#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    key_dev = input_allocate_device();
    if (!key_dev || !ts) {
        result = -ENOMEM;
        goto fail_alloc_mem;
    }
#endif
    
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) {
        dev_err(&pdev->dev, "Cannot get IORESOURCE_MEM\n");
        result = -ENOENT;
        goto fail_alloc_mem;
    }

    ts->irq = platform_get_irq(pdev, 0);
    if (!ts->irq) {
        dev_err(&pdev->dev, "Could not get IORESOURCE_IRQ\n");
        result = -ENODEV;
        goto fail_alloc_mem;
    }

    ioarea = request_mem_region(res->start, resource_size(res), pdev->name);
    if (!ioarea) {
        dev_err(&pdev->dev, "Could not allocate io region\n");
        result = -EBUSY;
        goto fail_alloc_mem;
    }

    virt = ioremap(res->start, resource_size(res));
    if (!virt) {
        dev_err(&pdev->dev, "Could not ioremap region\n");
        result = -ENOMEM;
        goto fail_ioremap;
    }

    input_dev->name = TS_DRIVER_NAME;
    input_dev->phys = "msm_touch/input0";
    input_dev->id.bustype = BUS_HOST;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = 0x0002;
    input_dev->id.version = 0x0100;
    input_dev->dev.parent = &pdev->dev;

    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    input_dev->absbit[0] = BIT(ABS_X) | BIT(ABS_Y) | BIT(ABS_PRESSURE);
    input_dev->absbit[BIT_WORD(ABS_MISC)] = BIT_MASK(ABS_MISC);
    input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

    if (pdata) {
        x_max = pdata->x_max ? : X_MAX;
        y_max = pdata->y_max ? : Y_MAX;
        pressure_max = pdata->pressure_max ? : P_MAX;
    } else {
        x_max = X_MAX;
        y_max = Y_MAX;
        pressure_max = P_MAX;
    }

    ts->x_max = x_max;
    ts->y_max = y_max;

    if(ts->use_touch_key)
    {
        input_set_abs_params(input_dev, ABS_X, 0, 960, 0, 0);
        input_set_abs_params(input_dev, ABS_Y, 0, 920, 0, 0);
    }
    else
    {
        input_set_abs_params(input_dev, ABS_X, 0, x_max, 0, 0);
        input_set_abs_params(input_dev, ABS_Y, 0, y_max, 0, 0);
    }
    input_set_abs_params(input_dev, ABS_PRESSURE, 0, pressure_max, 0, 0);

    result = input_register_device(input_dev);
    if (result)
        goto fail_ip_reg;

    ts->input = input_dev;

    setup_timer(&ts->timer, ts_timer, (unsigned long)ts);
    
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    if(ts->use_touch_key)
    {
        int i;

        set_bit(EV_KEY, key_dev->evbit);
        for (i = 0; i < EXTRA_MAX_TOUCH_KEY; i++)
        {
            set_bit(touch_extra_key_region.extra_key[i].touch_keycode & KEY_MAX, 
                                      key_dev->keybit);
        }

        result = input_register_device(key_dev);
        if (result)
            goto fail_ip_reg;
        
        ts->key_input = key_dev;
       
        setup_timer(&ts->key_timer, ts_key_timer, (unsigned long)ts);
    }
#endif
    result = request_irq(ts->irq, ts_interrupt, IRQF_TRIGGER_RISING,
                 "touchscreen", ts);
    if (result)
        goto fail_req_irq;

#ifdef CONFIG_HAS_EARLYSUSPEND
    ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
    ts->early_suspend.suspend = ts_early_suspend;
    ts->early_suspend.resume = ts_late_resume;
    register_early_suspend(&ts->early_suspend);
#endif

    platform_set_drvdata(pdev, ts);

    return 0;

fail_req_irq:
    input_unregister_device(input_dev);
    input_dev = NULL;
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    if(ts->use_touch_key)
    {
        input_unregister_device(key_dev);
        key_dev = NULL;
    }
#endif
fail_ip_reg:
    iounmap(virt);
fail_ioremap:
    release_mem_region(res->start, resource_size(res));
fail_alloc_mem:
    input_free_device(input_dev);
    kfree(ts);
    return result;
}

static int __devexit ts_remove(struct platform_device *pdev)
{
    struct resource *res;
    struct ts *ts = platform_get_drvdata(pdev);

#ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&ts->early_suspend);
#endif

    free_irq(ts->irq, ts);
    del_timer_sync(&ts->timer);

    input_unregister_device(ts->input);
    
#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    if(ts->use_touch_key)
    {
        del_timer_sync(&ts->key_timer);
        input_unregister_device(ts->key_input);
    }
#endif

    iounmap(virt);
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    release_mem_region(res->start, resource_size(res));
    platform_set_drvdata(pdev, NULL);
    kfree(ts);

    return 0;
}

/* The /proc entry for the backlight. */
static struct proc_dir_entry *key_cal_proc_entry = NULL;
static int virt_key_calibration_y = 900;

static int virt_key_write_proc(struct file *file, const char __user *buffer,
	unsigned long count, void *data)
{
    int ret = 0;
    int i = 0;

    if (!count) {
        ret = -EINVAL;
        goto write_proc_failed;
    }
    
    if (copy_from_user(&virt_key_calibration_y, buffer, sizeof(virt_key_calibration_y))) {
        TSSC("copy from user error!\n");
        ret = -EFAULT;
        goto write_proc_failed;
    }
    else
    {
        TSSC("%s:virt_key_calibration_y = %d\n!", __FUNCTION__, virt_key_calibration_y);
    }

    if(virt_key_calibration_y > (Y_MAX-TOUCH_KEY_NULL_REGION))
    {
        virt_key_calibration_y = (Y_MAX-TOUCH_KEY_NULL_REGION);
        printk(KERN_ERR "error key calibraion oversacel = %d\n",
                 virt_key_calibration_y);
    }
    else if(virt_key_calibration_y < (Y_MAX-TOUCH_KEY_MAX_REGION))
    {
        printk(KERN_ERR "key calibraion value over TOUCH_KEY_MAX_REGION= %d\n",
                 virt_key_calibration_y);
        virt_key_calibration_y = (Y_MAX-TOUCH_KEY_MAX_REGION);
    }

#ifdef CONFIG_HUAWEI_TOUCHSCREEN_EXTRA_KEY
    touch_extra_key_region.extra_touch_region.touch_y_start = virt_key_calibration_y + TOUCH_KEY_NULL_REGION/2;
    
    for(i=0; i<EXTRA_MAX_TOUCH_KEY; i++)
    {
        touch_extra_key_region.extra_key[i].center_y = (Y_MAX + (virt_key_calibration_y + TOUCH_KEY_NULL_REGION)) >> 1;
        touch_extra_key_region.extra_key[i].y_width = Y_MAX - touch_extra_key_region.extra_key[i].center_y - 4;
    }
#endif

    return count;

write_proc_failed:
    printk(KERN_ERR "%s:-= virt key write proc ERROR! =-\n", __FUNCTION__);
	return ret;
}

static int virt_key_read_proc(char *buf, char **start, off_t offset, 
                          int len, int *unused_i, void *unused_v)
{
    int outlen = 0;
    len = sprintf(buf,"%d\n",virt_key_calibration_y);
    buf += len;
    outlen += len;
	
    return outlen;
}

static char ts_calibration_stats = '0';
static int msm_ts_read_proc(char *buf, char **start, off_t offset, 
                          int len, int *unused_i, void *unused_v)
{
    int outlen = 0;
	len = sprintf(buf,"%c\n",ts_calibration_stats);
	buf += len;
	outlen += len;
	
	return outlen;
}

static int msm_ts_write_proc(struct file *file, const char __user *buffer,
	unsigned long count, void *data)
{
	char buf[sizeof(ts_calibration_stats)];
	int ret;

	if (count < sizeof(ts_calibration_stats)) {
		ret = -EINVAL;
		goto write_proc_failed;
	}

	if (copy_from_user(buf, buffer, sizeof(ts_calibration_stats))) {
		ret = -EFAULT;
		goto write_proc_failed;
	}

	ts_calibration_stats = buf[0];

	return count;

write_proc_failed:
    printk(KERN_ERR "%s:-= ts write proc ERROR! =-\n", __FUNCTION__);
	return ret;
}

static struct platform_driver ts_driver = {
    .probe      = ts_probe,
    .remove     = __devexit_p(ts_remove),
    .driver     = {
        .name = TS_DRIVER_NAME,
        .owner = THIS_MODULE,
    },
};

static int __init ts_init(void)
{
    struct proc_dir_entry *d_entry;
    d_entry = create_proc_entry("msm_ts_cal", 0666, NULL);
    if (d_entry) {
        d_entry->read_proc = msm_ts_read_proc;
        d_entry->write_proc = msm_ts_write_proc;
        d_entry->data = NULL;
    }

    key_cal_proc_entry = create_proc_entry("virt_key_cal", 0777, NULL);
    if (key_cal_proc_entry) {
        key_cal_proc_entry->read_proc = virt_key_read_proc;
        key_cal_proc_entry->write_proc = virt_key_write_proc;
        key_cal_proc_entry->data = NULL;
    }

	
    return platform_driver_register(&ts_driver);
}
module_init(ts_init);

static void __exit ts_exit(void)
{
    platform_driver_unregister(&ts_driver);
    
    remove_proc_entry("msm_ts_cal", 0);
}
module_exit(ts_exit);

MODULE_DESCRIPTION("MSM Touch Screen driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:msm_touchscreen");
