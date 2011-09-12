/* Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <mach/pmic.h>
#include <mach/camera.h>

#ifdef CONFIG_HUAWEI_EVALUATE_POWER_CONSUMPTION
#include <mach/huawei_battery.h>
#define CAMERA_FLASH_CUR_DIV 10
#endif

#include <mach/vreg.h>

int32_t msm_camera_flash_enable_vreg(void);
int32_t msm_camera_flash_disable_vreg(void);


#ifdef CONFIG_HUAWEI_CAMERA
#define FLASH_HIGH_DRIVE_CURRENT_DELAY_MS 200
#define FLASH_HIGH_DRIVE_CURRENT_MAX_MS 500
#define FLASH_LOW_DRIVE_CURRENT_MA 100
#define FLASH_HIGH_DRIVE_CURRENT_MA 250
static int  flash_state = 0;
/* Add variable verg_enable_times to memorize 
 * how many times the verg "boost" was enabled 
 */
static int vreg_enable_times = 0;

static struct hrtimer flash_timer;

static struct work_struct flash_work;
static enum hrtimer_restart flash_timer_func(struct hrtimer *timer)
{
	schedule_work(&flash_work);
	return HRTIMER_NORESTART;
}
static void flash_on(struct work_struct *work)
{   
    if(!flash_state)
    {
        pmic_flash_led_set_current(FLASH_HIGH_DRIVE_CURRENT_MA);
#ifdef CONFIG_HUAWEI_EVALUATE_POWER_CONSUMPTION        
        huawei_rpc_current_consuem_notify(EVENT_CAMERA_FLASH_NOTIFY, (FLASH_HIGH_DRIVE_CURRENT_MA/CAMERA_FLASH_CUR_DIV));
#endif
        flash_state = 1;
		hrtimer_start(&flash_timer,
	      ktime_set(FLASH_HIGH_DRIVE_CURRENT_MAX_MS / 1000, (FLASH_HIGH_DRIVE_CURRENT_MAX_MS % 1000) * 1000000),
	      HRTIMER_MODE_REL);
        
    }
    else
    {
        pmic_flash_led_set_current(0);		
		msm_camera_flash_disable_vreg();
        flash_state = 0;
    }
}
#endif

int32_t msm_camera_flash_enable_vreg(void)
{
	int ret = 0;
	struct vreg *vreg_5v = NULL;
	
	vreg_5v = vreg_get(NULL,"boost");
	ret = IS_ERR(vreg_5v);
	if(ret) {
		printk(KERN_ERR "%s: vreg_5v get failed (%d)\n", __func__, ret);
    	return ret;
	}

	ret = vreg_set_level(vreg_5v,5000);	
	if(ret) {
		printk(KERN_ERR "%s: vreg_5v set level failed (%d)\n", __func__, ret);
    	return ret;
	}

	ret = vreg_enable(vreg_5v);
	if(ret) {
		printk(KERN_ERR "%s: vreg_5v enable failed (%d)\n", __func__, ret);
    	return ret;
	}
	vreg_enable_times++;

	return ret;
}

int32_t msm_camera_flash_disable_vreg(void)
{
	int ret = 0;
	struct vreg *vreg_5v = NULL;
	
	vreg_5v = vreg_get(NULL,"boost");
	ret = IS_ERR(vreg_5v);
	if(ret) {
		printk(KERN_ERR "%s: vreg_5v get failed (%d)\n", __func__, ret);
    	return ret;
	}

	while(vreg_enable_times)
	{
		ret = vreg_disable(vreg_5v);
		if(ret) {
			printk(KERN_ERR "%s: vreg_5v disable failed (%d)\n", __func__, ret);
	    	return ret;
		}
		vreg_enable_times--;
	}

	return ret;
}

int32_t msm_camera_flash_set_led_state(unsigned led_state)
{
	int32_t rc;
#ifdef CONFIG_HUAWEI_CAMERA
    static int init = 0;
    if(!init)
    {
        init = 1;
        INIT_WORK(&flash_work, flash_on);
        hrtimer_init(&flash_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        flash_timer.function= flash_timer_func;
    }
#endif

	CDBG("flash_set_led_state: %d\n", led_state);
	switch (led_state) {
	case MSM_CAMERA_LED_OFF:
#ifdef CONFIG_HUAWEI_CAMERA
        hrtimer_cancel(&flash_timer);
#endif
		rc = pmic_flash_led_set_current(0);
		rc = msm_camera_flash_disable_vreg();
		break;

	case MSM_CAMERA_LED_LOW:
#ifndef CONFIG_HUAWEI_CAMERA
		rc = pmic_flash_led_set_current(30);
#else
        hrtimer_cancel(&flash_timer);
		rc = msm_camera_flash_enable_vreg();
        if(machine_is_msm7x25_u8300())
        {
            rc = pmic_flash_led_set_current(FLASH_HIGH_DRIVE_CURRENT_MA);
        }
        else
        {
		    rc = pmic_flash_led_set_current(FLASH_LOW_DRIVE_CURRENT_MA);
        }
#endif
		break;

	case MSM_CAMERA_LED_HIGH:
#ifndef CONFIG_HUAWEI_CAMERA
		rc = pmic_flash_led_set_current(100);
#else
        hrtimer_cancel(&flash_timer);
        flash_state = 0;
		//It takes time to eanble voltage regulator. So the voltage regulator should be enabled in advance before turning on flash LED.
		msm_camera_flash_enable_vreg();
		hrtimer_start(&flash_timer,
	      ktime_set(FLASH_HIGH_DRIVE_CURRENT_DELAY_MS / 1000, (FLASH_HIGH_DRIVE_CURRENT_DELAY_MS % 1000) * 1000000),
	      HRTIMER_MODE_REL);

		rc = 0;
#endif
		break;
	default:
		rc = -EFAULT;
		break;
	}
	CDBG("flash_set_led_state: return %d\n", rc);


#ifdef CONFIG_HUAWEI_EVALUATE_POWER_CONSUMPTION
	switch (led_state) {
	case MSM_CAMERA_LED_OFF:
        huawei_rpc_current_consuem_notify(EVENT_CAMERA_FLASH_NOTIFY, 0);
		break;
#ifndef CONFIG_HUAWEI_CAMERA
	case MSM_CAMERA_LED_LOW:
        huawei_rpc_current_consuem_notify(EVENT_CAMERA_FLASH_NOTIFY, (30/CAMERA_FLASH_CUR_DIV));
		break;

	case MSM_CAMERA_LED_HIGH:
        huawei_rpc_current_consuem_notify(EVENT_CAMERA_FLASH_NOTIFY, (100/CAMERA_FLASH_CUR_DIV));
		break;
#else
	case MSM_CAMERA_LED_LOW:
        huawei_rpc_current_consuem_notify(EVENT_CAMERA_FLASH_NOTIFY, (FLASH_LOW_DRIVE_CURRENT_MA/CAMERA_FLASH_CUR_DIV));
		break;

	case MSM_CAMERA_LED_HIGH:
		break;
#endif
	default:
		break;
	}
#endif

	return rc;
}
