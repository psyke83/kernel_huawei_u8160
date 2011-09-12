/* Copyright (c) 2009, Code HUAWEI. All rights reserved.
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
#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include "lcdc_huawei_config.h"
#include <asm/mach-types.h>
#include <mach/vreg.h>

struct lcd_hx8357b_state_type{
    boolean disp_initialized;
    boolean display_on;
    boolean disp_powered_up;
};


extern void pwm_set_backlight(int level);

static int lcd_reset_gpio;
static struct lcd_hx8357b_state_type lcd_hx8357b_state = { 0 };
static struct msm_panel_common_pdata *lcdc_hx8357b_pdata;
static lcd_panel_type lcd_panel_hvga = LCD_NONE;

struct sequence{
    uint32 value;
    uint32 type;
    uint32 time; //unit is ms
};

typedef enum
{
    GPIO_LOW_VALUE  = 0,
    GPIO_HIGH_VALUE = 1
} gpio_value_type;

#define TYPE_COMMAND	         (1<<0)
#define TYPE_PARAMETER           (1<<1)
#define START_BYTE_COMMAND       0x00
#define START_BYTE_PARAMETER     0x01
#if TRACE_LCD_DEBUG
#define LCD_DEBUG(x...) printk(KERN_ERR "[LCD_DEBUG] " x)
#else
#define LCD_DEBUG(x...) do {} while (0)
#endif

static const struct sequence hx8357b_tianma_disp_off[] = 
{  
      {0x10, TYPE_COMMAND,120}
};

static const struct sequence hx8357b_tianma_disp_on[] = 
{
      {0x11, TYPE_COMMAND,120}
};

#define SPI_CLK_DELAY             1
#define SPI_CLK_PULSE_INTERVAL    5


#ifndef ARRAY_SIZE
#define  ARRAY_SIZE(a) (sizeof((a))/sizeof((a)[0]))
#endif

#define NUM_HX8357B_TIANMA_DISP_ON   ARRAY_SIZE(hx8357b_tianma_disp_on)
#define NUM_HX8357B_TIANMA_DISP_OFF   ARRAY_SIZE(hx8357b_tianma_disp_off)



static void process_lcdc_table(struct sequence *table, size_t count)
{
    int i;
    uint32 type = 0;
    uint32 value = 0;
    uint32 time = 0;
    uint8 start_byte = 0;

    for (i = 0; i < count; i++) {
        type = table[i].type;
        value = table[i].value;
        time = table[i].time;
        if (type & TYPE_COMMAND) {
            start_byte = START_BYTE_COMMAND;
        } 

        if (type & TYPE_PARAMETER) {
            start_byte = START_BYTE_PARAMETER;
        }

    seriout_byte_9bit(start_byte, value);	
    if (time != 0){
       
       /* if time > 10ms,use msleep function ,others use mdelay function */
       if (time > 10){
           msleep(time);
       }else{
           mdelay(time);
       }
    }
    }

}

static void lcd_hx8357b_disp_powerup(void)
{
    if (!lcd_hx8357b_state.disp_powered_up && !lcd_hx8357b_state.display_on) 
    {
        /* Reset the hardware first */
        /* Include DAC power up implementation here */
        lcd_hx8357b_state.disp_powered_up = TRUE;
    }
}

static void lcd_hx8357b_panel_exit_sleep(void)
{
        printk(KERN_INFO "%s:lcd_panel_hvga = %d\n", __func__, lcd_panel_hvga);

        switch(lcd_panel_hvga)
        {
            case LCD_HX8357B_TIANMA_HVGA:
                printk(KERN_INFO "%s:TIANMA_HX8357B\n", __func__);
                process_lcdc_table((struct sequence*)&hx8357b_tianma_disp_on, ARRAY_SIZE(hx8357b_tianma_disp_on));
                break;
            default:
                break;
        }
        LCD_DEBUG("%s: Exit Standby Mode\n", __func__);
        lcd_hx8357b_state.display_on = TRUE;
}

static int lcdc_hx8357b_panel_off(struct platform_device *pdev)
{

 if (lcd_hx8357b_state.disp_powered_up && lcd_hx8357b_state.display_on) {
        /* Enter Standby Mode */
        switch(lcd_panel_hvga)
        {
            case LCD_HX8357B_TIANMA_HVGA:
                process_lcdc_table((struct sequence*)&hx8357b_tianma_disp_off, ARRAY_SIZE(hx8357b_tianma_disp_off));
                break;
            default:
                break;
        }
        lcd_hx8357b_state.display_on = FALSE;
        LCD_DEBUG("%s: Enter Standby Mode\n", __func__);
    }

    return 0;
}

static void lcdc_hx8357b_panel_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
        
    pwm_set_backlight(bl_level);
    
    return;
}

static int lcdc_hx8357b_panel_on(struct platform_device *pdev)
{
	printk(KERN_INFO "%s\n", __func__);
    if (!lcd_hx8357b_state.disp_initialized) 
    {
        /* Configure reset GPIO that drives DAC */
        lcdc_hx8357b_pdata->panel_config_gpio(1);
        lcd_reset_gpio = *(lcdc_hx8357b_pdata->gpio_num + 4);
 
        lcd_spi_init(lcdc_hx8357b_pdata);	/* LCD needs SPI */
        lcd_hx8357b_disp_powerup();
        lcd_hx8357b_state.display_on = TRUE;
        lcd_hx8357b_state.disp_initialized = TRUE;
    }
    else if(!lcd_hx8357b_state.display_on)
    {
        lcd_hx8357b_panel_exit_sleep();
    }
    return 0;
}

static int __init lcdc_hx8357b_probe(struct platform_device *pdev)
{
    if (pdev->id == 0) {
        lcdc_hx8357b_pdata = pdev->dev.platform_data;
        return 0;
    }msm_fb_add_device(pdev);
    return 0;
}

static struct platform_driver this_driver = 
{
    .probe  = lcdc_hx8357b_probe,
    .driver = {
        .name   = "lcdc_hx8357b_tm",
    },
};
static struct msm_fb_panel_data hx8357b_panel_data =
{
    .on = lcdc_hx8357b_panel_on,
    .off = lcdc_hx8357b_panel_off,
    .set_backlight = lcdc_hx8357b_panel_set_backlight,
};

static struct platform_device this_device = 
{
    .name   = "lcdc_hx8357b_tm",
    .id = 1,
    .dev    =
    {
        .platform_data = &hx8357b_panel_data,
    }
};

static int __init lcdc_hx8357b_panel_init(void)
{
    int ret;
    struct msm_panel_info *pinfo;

    lcd_panel_hvga = lcd_panel_probe();
    printk(KERN_INFO "%s:lcd_panel_hvga = %d\n", __func__, lcd_panel_hvga);
    if((LCD_HX8357B_TIANMA_HVGA != lcd_panel_hvga) && \
       (msm_fb_detect_client("lcdc_hx8357b_tm"))
       )
    {
        return 0;
    }
    
    ret = platform_driver_register(&this_driver);
    printk(KERN_INFO "%s:register.ret = %d\n", __func__, ret);
    if (ret)
        return ret;

    pinfo = &hx8357b_panel_data.panel_info;

    pinfo->xres = 320;
    pinfo->yres = 480;
    pinfo->type = LCDC_PANEL;
    pinfo->pdest = DISPLAY_1;
    pinfo->wait_cycle = 0;
    pinfo->bpp = 18;
    pinfo->fb_num = 2;
    /*the pixel clk is different for different Resolution LCD*/
    
    pinfo->clk_rate = 8192000;    /*for HVGA pixel clk*/
    
    pinfo->lcdc.h_back_porch = 7;
    pinfo->lcdc.h_front_porch = 5;
    pinfo->lcdc.h_pulse_width = 4;
    pinfo->lcdc.v_back_porch = 2;
    pinfo->lcdc.v_front_porch = 4;
    pinfo->lcdc.v_pulse_width = 2;
    pinfo->lcdc.border_clr = 0;     /* blk */
    pinfo->lcdc.underflow_clr = 0xff;       /* blue */
    pinfo->lcdc.hsync_skew = 0;
    pinfo->bl_max = 255;

    ret = platform_device_register(&this_device);
    if (ret)
        platform_driver_unregister(&this_driver);
    return ret;
}

module_init(lcdc_hx8357b_panel_init);
