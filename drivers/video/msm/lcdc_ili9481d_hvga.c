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
/*add qunchuang and tianma lcd driver*/
#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include "lcdc_huawei_config.h"
#include <asm/mach-types.h>
#include <mach/vreg.h>

struct lcd_ili9481d_state_type{
    boolean disp_initialized;
    boolean display_on;
    boolean disp_powered_up;
};


extern void pwm_set_backlight(int level);

static int lcd_reset_gpio;
static struct lcd_ili9481d_state_type lcd_ili9481d_state = { 0 };
static struct msm_panel_common_pdata *lcdc_ili9481d_pdata;
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

#ifdef    CAMERA_SET_GAMMA
#undef   CAMERA_SET_GAMMA
#endif
#define TYPE_COMMAND	         (1<<0)
#define TYPE_PARAMETER           (1<<1)
#define START_BYTE_COMMAND       0x00
#define START_BYTE_PARAMETER     0x01
#if TRACE_LCD_DEBUG
#define LCD_DEBUG(x...) printk(KERN_ERR "[LCD_DEBUG] " x)
#else
#define LCD_DEBUG(x...) do {} while (0)
#endif

/* delete  ili9481d_innolux_init[],it's useless */


static const struct sequence ili9481d_innolux_read[] =
{
    {0xBF, TYPE_COMMAND, 0},
};

static const struct sequence ili9481d_innolux_disp_off[] = 
{
      {0x28, TYPE_COMMAND,0},   
      {0x10, TYPE_COMMAND, 50},
};

static const struct sequence ili9481ds_innolux_disp_on[] = 
{
      {0x29, TYPE_COMMAND,0},   
      {0x11, TYPE_COMMAND,150}
};

static uint16 ili9481d_innolux_gamma_for_camrea[][3] = 
{
        {0x0013, 0x7FD9, 0},
        {0x0030, 0x0003, 0},
        {0x0031, 0x070F, 0},
        {0x0032, 0x0D05, 0},
        {0x0033, 0x0405, 0},
        {0x0034, 0x090D, 0},
        {0x0035, 0x0501, 0},        
        {0x0036, 0x0400, 0},
        {0x0037, 0x0504, 0},
        {0x0038, 0x0C09, 0},
        {0x0039, 0x010C, 1},
};

static uint16 ili9481d_innolux_gamma_normal[][3] = 
{
        {0x0013, 0x7FE3, 0},
        {0x0030, 0x0005, 0},
        {0x0031, 0x070F, 0},
        {0x0032, 0x0300, 0},
        {0x0033, 0x0003, 0},
        {0x0034, 0x090C, 0},
        {0x0035, 0x0001, 0},        
        {0x0036, 0x0001, 0},
        {0x0037, 0x0303, 0},
        {0x0038, 0x0F09, 0},
        {0x0039, 0x0105, 0},
};


/* delete  ili9481ds_tianma_init[],it's useless */

static const struct sequence ili9481ds_tianma_read[] = 
{
      {0xBF, TYPE_COMMAND,0},
   
};
static const struct sequence ili9481ds_tianma_disp_off[] = 
{
      {0x28, TYPE_COMMAND,0},   
      {0x10, TYPE_COMMAND,50}
 
};

static const struct sequence ili9481ds_tianma_disp_on[] = 
{
       {0x29, TYPE_COMMAND,0},   
       {0x11, TYPE_COMMAND,150}
};



#define NUM_ILI9481D_INNOLUX_GAMMA_CAMREA     (sizeof(ili9481d_innolux_gamma_for_camrea)/(sizeof(uint16) * 3))
#define NUM_ILI9481D_INNOLUX_GAMMA_NORMAL     (sizeof(ili9481d_innolux_gamma_normal)/(sizeof(uint16) * 3))


#define SPI_CLK_DELAY             1
#define SPI_CLK_PULSE_INTERVAL    5


static void seriout(uint16 reg, uint16 data)
{
    /*start byte is different form LCD panel driver IC,pls ref LCD SPEC */
    uint8 start_byte_reg = 0x74;
    uint8 start_byte_data = 0x76;

    seriout_cmd(reg, start_byte_reg);
    seriout_data(data, start_byte_data);
}


#ifndef ARRAY_SIZE
#define  ARRAY_SIZE(a) (sizeof((a))/sizeof((a)[0]))
#endif

#define NUM_ILI9481DS_TIANMA_INIT   ARRAY_SIZE(ili9481ds_tianma_init)
#define NUM_ILI9481DS_TIANMA_DISP_OFF   ARRAY_SIZE(ili9481ds_tianma_disp_off)



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

static void lcd_ili9481d_disp_powerup(void)
{
    if (!lcd_ili9481d_state.disp_powered_up && !lcd_ili9481d_state.display_on) 
    {
        /* Reset the hardware first */
        /* Include DAC power up implementation here */
        lcd_ili9481d_state.disp_powered_up = TRUE;
    }
}

static void lcd_ili9481d_panel_exit_sleep(void)
{
        printk(KERN_INFO "%s:lcd_panel_hvga = %d\n", __func__, lcd_panel_hvga);

        switch(lcd_panel_hvga)
        {
            case LCD_ILI9481D_INNOLUX_HVGA:
                printk(KERN_INFO "%s:\n", __func__);
                process_lcdc_table((struct sequence*)&ili9481ds_innolux_disp_on, ARRAY_SIZE(ili9481ds_innolux_disp_on));
                break;
            case LCD_ILI9481DS_TIANMA_HVGA:
                printk(KERN_INFO "%s:TIANMA\n", __func__);
                process_lcdc_table((struct sequence*)&ili9481ds_tianma_disp_on, ARRAY_SIZE(ili9481ds_tianma_disp_on));
                break;
            default:
                break;
        }
        LCD_DEBUG("%s: Exit Standby Mode\n", __func__);
        lcd_ili9481d_state.display_on = TRUE;
}


static int lcdc_ili9481d_panel_off(struct platform_device *pdev)
{

 if (lcd_ili9481d_state.disp_powered_up && lcd_ili9481d_state.display_on) {
        /* Enter Standby Mode */
        switch(lcd_panel_hvga)
        {
            case LCD_ILI9481D_INNOLUX_HVGA:
                process_lcdc_table((struct sequence*)&ili9481d_innolux_disp_off, ARRAY_SIZE(ili9481d_innolux_disp_off));
                break;
            case LCD_ILI9481DS_TIANMA_HVGA:
                process_lcdc_table((struct sequence*)&ili9481ds_tianma_disp_off, ARRAY_SIZE(ili9481ds_tianma_disp_off));
                break;
            default:
                break;
        }
        lcd_ili9481d_state.display_on = FALSE;
        LCD_DEBUG("%s: Enter Standby Mode\n", __func__);
    }

    return 0;
}

static void lcdc_ili9481d_panel_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
        
    pwm_set_backlight(bl_level);
    
    return;
}

static int lcdc_ili9481d_panel_on(struct platform_device *pdev)
{
	printk(KERN_INFO "%s\n", __func__);
    if (!lcd_ili9481d_state.disp_initialized) 
    {
        /* Configure reset GPIO that drives DAC */
        lcdc_ili9481d_pdata->panel_config_gpio(1);
        lcd_reset_gpio = *(lcdc_ili9481d_pdata->gpio_num + 4);
 
        lcd_spi_init(lcdc_ili9481d_pdata);	/* LCD needs SPI */
        lcd_ili9481d_disp_powerup();
        lcd_ili9481d_state.display_on = TRUE;
        lcd_ili9481d_state.disp_initialized = TRUE;
    }
    else if(!lcd_ili9481d_state.display_on)
    {
        lcd_ili9481d_panel_exit_sleep();
    }
    return 0;
}

static void lcdc_ili9481d_panel_set_contrast(struct msm_fb_data_type *mfd, unsigned int contrast)
{
#ifdef  CAMERA_SET_GAMMA
    unsigned char i = 0;

    if(0 == contrast)
    {
        if (lcd_ili9481d_state.disp_powered_up && lcd_ili9481d_state.display_on)
        {
            for(i = 0; i < NUM_ILI9481D_INNOLUX_GAMMA_NORMAL; i++)
            {
                seriout(ili9481d_innolux_gamma_normal[i][0], ili9481d_innolux_gamma_normal[i][1]);
                mdelay(ili9481d_innolux_gamma_normal[i][2]);
            }     
        }
    }
    else
    {
        if (lcd_ili9481d_state.disp_powered_up && lcd_ili9481d_state.display_on)
        {
            for(i = 0; i < NUM_ILI9481D_INNOLUX_GAMMA_CAMREA; i++)
            {
                seriout(ili9481d_innolux_gamma_for_camrea[i][0], ili9481d_innolux_gamma_for_camrea[i][1]);
                mdelay(ili9481d_innolux_gamma_for_camrea[i][2]);
            }     
        }
    }
#endif
    return;
}

static int __init lcdc_ili9481d_probe(struct platform_device *pdev)
{
    if (pdev->id == 0) {
        lcdc_ili9481d_pdata = pdev->dev.platform_data;
        return 0;
    }msm_fb_add_device(pdev);
    return 0;
}

static struct platform_driver this_driver = 
{
    .probe  = lcdc_ili9481d_probe,
    .driver = {
        .name   = "lcdc_ili9481_inn",
    },
};
static struct msm_fb_panel_data ili9481d_panel_data =
{
    .on = lcdc_ili9481d_panel_on,
    .off = lcdc_ili9481d_panel_off,
    .set_backlight = lcdc_ili9481d_panel_set_backlight,
    .set_contrast = lcdc_ili9481d_panel_set_contrast,
};

static struct platform_device this_device = 
{
    .name   = "lcdc_ili9481_inn",
    .id = 1,
    .dev    =
    {
        .platform_data = &ili9481d_panel_data,
    }
};

static int __init lcdc_ili9481d_panel_init(void)
{
    int ret;
    struct msm_panel_info *pinfo;

    lcd_panel_hvga = lcd_panel_probe();
    printk(KERN_INFO "%s:lcd_panel_hvga = %d\n", __func__, lcd_panel_hvga);
    if((LCD_ILI9481D_INNOLUX_HVGA != lcd_panel_hvga) && \
        (LCD_ILI9481DS_TIANMA_HVGA != lcd_panel_hvga) && \
       (msm_fb_detect_client("lcdc_ili9481_inn"))
       )
    {
        return 0;
    }
    
    ret = platform_driver_register(&this_driver);
    printk(KERN_INFO "%s:register.ret = %d\n", __func__, ret);
    if (ret)
        return ret;

    pinfo = &ili9481d_panel_data.panel_info;

    pinfo->xres = 320;
    pinfo->yres = 480;
    pinfo->type = LCDC_PANEL;
    pinfo->pdest = DISPLAY_1;
    pinfo->wait_cycle = 0;
    pinfo->bpp = 18;
    pinfo->fb_num = 2;
    /*the pixel clk is different for different Resolution LCD*/
    pinfo->clk_rate = 9452000;    /*for HVGA pixel clk*/

    pinfo->lcdc.h_back_porch  = 3;
    pinfo->lcdc.h_front_porch = 3;
    pinfo->lcdc.h_pulse_width = 2;
    pinfo->lcdc.v_back_porch  = 2;
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

module_init(lcdc_ili9481d_panel_init);
