/* drivers\video\msm\c8600_backlight.c
 * backlight driver for 7x25 platform
 *
 * Copyright (C) 2009 HUAWEI Technology Co., ltd.
 * 
 * Date: 2009/12/29
 * By Jia Lin
 * 
 */

#include <mach/gpio.h>
#include <linux/earlysuspend.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include "lcdc_huawei_config.h"
#include <mach/pmic.h>
atomic_t kp_suspend_flag = ATOMIC_INIT(0); 		/* the value is "1" when suspend */
extern atomic_t button_flag;
static atomic_t suspend_flag = ATOMIC_INIT(0); 
static int restore_level=1;

void pwm_set_backlight(int level)
{
	if(atomic_read(&suspend_flag)) 
    	{
    	    restore_level = level;
	    return;
    	}

	lcd_set_backlight_pwm(level);
	return;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void pwm_backlight_suspend( struct early_suspend *h)
{
	atomic_set(&suspend_flag,1);
}

static void pwm_backlight_resume( struct early_suspend *h)
{
	lcd_set_backlight_pwm(restore_level);
	atomic_set(&suspend_flag,0);	
}

static struct early_suspend pwm_backlight_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend = pwm_backlight_suspend,
	.resume = pwm_backlight_resume,
};
static void keypad_led_suspend( struct early_suspend *h)
{
	atomic_set(&kp_suspend_flag, 1);
	pmic_set_led_intensity(LED_LCD, 0);			/* turn off touchpad light when suspend */
}
static void keypad_led_resume( struct early_suspend *h)
{
	atomic_set(&kp_suspend_flag, 0);
	if (atomic_read(&button_flag))
	{
		pmic_set_led_intensity(LED_LCD, 1);		/* turn on touchpad light when resume */
	}
}
static struct early_suspend keypad_led_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+2,
	.suspend = keypad_led_suspend,
	.resume = keypad_led_resume,
};
#endif


static int __init pwm_backlight_init(void)
{
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&pwm_backlight_early_suspend);
	register_early_suspend(&keypad_led_early_suspend);
#endif

	return 0;
}

module_init(pwm_backlight_init);
