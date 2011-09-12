/* 
 * driver/i2c/chips/huawei_key_report.c
 *
 * report key for huawei phone.
 * currently U8300 only
 *
 * Copyright (C) 2009 HUAWEI.
 *
 * Author: Wuzhihui
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

/*
 * camera's open function is not in irq context.
 * timer function is not in irq context
 * report key function is not in irq context 
 * so we could use semaphore or spinlock to protect variable
 */

//KEY_REPORT_DEBUG("%s:\n",__FUNCTION__);

#include <linux/input.h>
//for bit operation
#include <asm/bitops.h>
#include <linux/kernel.h>
#include <linux/hrtimer.h>
#include <linux/time.h>
#include <linux/stddef.h>
#include <linux/slab_def.h>
#include <linux/slab.h>

#undef HUAWEI_KEY_REPORT
#ifdef HUAWEI_KEY_REPORT
#define KEY_REPORT_DEBUG(fmt, args...) printk(KERN_INFO fmt, ##args)
#else
#define KEY_REPORT_DEBUG(fmt, args...)
#endif

#define KEY_REPORT_ERROR(fmt, args...) printk(KERN_ERR fmt, ##args)

/*
 * bit operation(atomic operation)
 * definition:
 * bit3: if back key is pressed		1 pressed;	0 not pressed;
 * NOTE: when we presse KEY_BACK , the BACK_KEY_IS_PRESSED bit will be set
 *	 when we release KEY_BACK , the BACK_KEY_IS_PRESSED bit will be clear.
 */
#define	BACK_KEY_IS_PRESSED		1

volatile unsigned long key_pressed_flags=0;

// the long press time, about 0.5s. same as gpio_matrix.c
#define KEY_KTIME_SECS			0
#define KEY_KTIME_NSECS			500000000

struct huawei_timer_struct
{
  struct input_dev *dev;
  struct hrtimer back_key_timer;
};

static struct huawei_timer_struct *u8300_timer=NULL;

void huawei_report_key(struct input_dev *input_dev, unsigned int code, int value);
static void u8300_report_key(struct input_dev *input_dev, unsigned int code, int value);

static void u8300_back_timer_init(struct huawei_timer_struct *u8300_timer_struct);
static int u8300_back_timer_start(struct huawei_timer_struct *u8300_timer_struct);
static int u8300_back_timer_cancel(struct huawei_timer_struct *u8300_timer_struct);
static enum hrtimer_restart u8300_back_timer_func(struct hrtimer *timer);

/*
 * huawei_report_key - function used to report key.
 * @input_dev	the device the key will report to.
 * @code	the key value.
 * @value	signify the key event is key press(1) or key release(0).
 *
 * This function will used by other module.
 * NOTE value [0,1]
 */
void huawei_report_key(struct input_dev *input_dev, unsigned int code, int value)
{

  //sanity check
  if(NULL == input_dev)
    return;

  u8300_report_key(input_dev, code, value);

  return;
}

/*
 * u8300_report_key - u8300 private report key function
 * @input_dev	the device the key will report to.
 * @code	the key value.
 * @value	signify the key event is key press(1) or key release(0).
 *
 * NOTE: we do NOT check the parameter, the caller should do it.
 *	 value [0,1]
 */
static void u8300_report_key(struct input_dev *input_dev, unsigned int code, int value)
{
  if(NULL == u8300_timer)
  {
    u8300_timer=kzalloc(sizeof(struct huawei_timer_struct), GFP_KERNEL);

    if(NULL == u8300_timer)
    {
      KEY_REPORT_ERROR("%s: kzalloc ERROR!!!\n",__FUNCTION__);
      // just report the key and return
      input_report_key(input_dev, code, value);
      return;
    }
    // init key_pressed_flags;
    key_pressed_flags=0;

    // NOTE: enable KEY_HOME !!!
    set_bit(KEY_HOME & KEY_MAX, input_dev->keybit);

    // init other field in u8300_timer
    u8300_timer->dev=input_dev;
  }
  /*
   * deal with KEY_BACK and long press.
   */
  // KEY_BACK
  if(test_bit(BACK_KEY_IS_PRESSED, &key_pressed_flags) && code == KEY_BACK)
  {
    // It is must be a KEY_BACK release event.
    // If not, report error and return.
    if(value)
    {
      // should not happen here.
      KEY_REPORT_ERROR("%s: ERROR: BACK_KEY_IS_PRESSED keycode=%d, value=%d\n", __FUNCTION__, code, value);
      return;
    }

    /*
     * the KEY_BACK timer is not fired.
     * so we consider it as a normal KEY_BACK envnt.
     */
    // cancel the timer first
    if(-1==u8300_back_timer_cancel(u8300_timer))
    {
      /*
       * unlucky the timer is fired, which means the timer has
       * report KEY_HOME, so we just return.
       */
      KEY_REPORT_DEBUG("%s: KEY_BACK timer fired\n",__FUNCTION__);

      return;
    }
    // clear the bit
    clear_bit(BACK_KEY_IS_PRESSED, &key_pressed_flags);
    // report KEY_BACK event
    input_report_key(input_dev, code, 1);
    input_report_key(input_dev, code, 0);

    KEY_REPORT_DEBUG("%s: KEY_BACK timer canceled and report KEY_BACK\n",__FUNCTION__);

    return;
  }

  /*
   * here we will deal with normal operation,
   */

  // deal with the KEY_BACK press event
  if(code == KEY_BACK) 
  {
    if(!value)
    {
      KEY_REPORT_ERROR("%s: Discard unexpected KEY_BACK release event!!\n",__FUNCTION__);
      return;
    }

    // start the KEY_BACK timer
    u8300_back_timer_init(u8300_timer);
    u8300_back_timer_start(u8300_timer);
    // TODO if we need to check the return value of u8300_back_timer_start

    // set the corresponding bit
    set_bit(BACK_KEY_IS_PRESSED, &key_pressed_flags);

    KEY_REPORT_DEBUG("%s: KEY_BACK pressed\n",__FUNCTION__);
    return;
  }

  /*
   * OK, we report it to upper layer.
   */
  KEY_REPORT_DEBUG("%s: report key code=%d value=%d\n",__FUNCTION__, code, value);

  input_report_key(input_dev, code, value);

  return;
}


/*
 * NOTE: we do NOT check the parameter, the caller should do it.
 */
static void u8300_back_timer_init(struct huawei_timer_struct *u8300_timer_struct)
{
  hrtimer_init(&u8300_timer_struct->back_key_timer,  CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  u8300_timer_struct->back_key_timer.function = u8300_back_timer_func;
}

/*
 * NOTE: we do NOT check the parameter, the caller should do it.
 */
static int u8300_back_timer_start(struct huawei_timer_struct *u8300_timer_struct)
{
  return hrtimer_start(&u8300_timer_struct->back_key_timer, ktime_set(KEY_KTIME_SECS, KEY_KTIME_NSECS), HRTIMER_MODE_REL);
}

/*
 * NOTE: we do NOT check the parameter, the caller should do it.
 * Returns:
 *  0 when the timer was not active
 *  1 when the timer was active
 * -1 when the timer is currently excuting the callback function and
 *    cannot be stopped
 */
static int u8300_back_timer_cancel(struct huawei_timer_struct *u8300_timer_struct)
{
  return hrtimer_cancel(&u8300_timer_struct->back_key_timer);
}

/*
 * NOTE: we do NOT check the parameter, the caller should do it.
 */
static enum hrtimer_restart u8300_back_timer_func(struct hrtimer *timer)
{
  struct huawei_timer_struct *hrt_timer = container_of(timer, struct huawei_timer_struct, back_key_timer);

  // report KEY_HOME to upper layer
  input_report_key(hrt_timer->dev, KEY_HOME, 1);
  input_report_key(hrt_timer->dev, KEY_HOME, 0);

  // clear the bit
  clear_bit(BACK_KEY_IS_PRESSED, &key_pressed_flags);

  KEY_REPORT_DEBUG("%s: KEY_BACK timer fired! report KEY_HOME\n",__FUNCTION__);

  return HRTIMER_NORESTART;

}





