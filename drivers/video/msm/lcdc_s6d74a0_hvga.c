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

struct lcd_s6d74a0_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

extern void pwm_set_backlight(int level);

static int lcd_reset_gpio;
static struct lcd_s6d74a0_state_type lcd_s6d74a0_state = { 0 };
static struct msm_panel_common_pdata *lcdc_s6d74a0_pdata;
static lcd_panel_type lcd_panel_hvga = LCD_NONE;
static uint16 s6d74a0_samsung_init[][3] = 
{
        {0x0007, 0x0000, 15},
        {0x0011, 0x222F, 0},
        {0x0012, 0x0F00, 0},
        {0x0013, 0x7FE3, 0},
        {0x0076, 0x2213, 0},
        {0x0074, 0x0001, 0},
        {0x0076, 0x0000, 0},
        {0x0010, 0x5604, 150},
        {0x0012, 0x0C63, 150},
        {0x0001, 0x083B, 0},
        {0x0002, 0x0300, 0},
        {0x0003, 0xD000, 0},
        //{0x0003, 0x2000, 0},
        {0x0008, 0x0002, 0},
        {0x0009, 0x000B, 0},
        {0x0076, 0x2213, 0},
        {0x000B, 0x3340, 0},
        {0x000C, 0x0020, 0},
        {0x001C, 0x7770, 0},
        {0x0076, 0x0000, 0},
        {0x000D, 0x0000, 0},
        {0x000E, 0x0500, 0},
        {0x0014, 0x0000, 0},
        {0x0015, 0x0803, 0},
        {0x0016, 0x0000, 0},
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
        {0x0007, 0x0001, 50},
        {0x0007, 0x0101, 50},
        {0x0007, 0x0103, 0},
};
static uint16 s6d74a0_samsung_gamma_for_camrea[][3] = 
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

static uint16 s6d74a0_samsung_gamma_normal[][3] = 
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

static uint16 s6d74a0_samsung_disp_off[][3] = 
{
        {0x000B, 0x3000, 0},
        {0x0007, 0x0102, 50},
        {0x0007, 0x0000, 50},
        {0x0012, 0x0000, 0},
        {0x0010, 0x0600, 10},
        {0x0010, 0x0601, 30},//0
};
#define NUM_S6D74A0_SAMSUNG_INIT             (sizeof(s6d74a0_samsung_init)/(sizeof(uint16) * 3))
#define NUM_S6D74A0_SAMSUNG_DISP_OFF         (sizeof(s6d74a0_samsung_disp_off)/(sizeof(uint16) * 3))
#define NUM_S6D74A0_SAMSUNG_GAMMA_CAMREA     (sizeof(s6d74a0_samsung_gamma_for_camrea)/(sizeof(uint16) * 3))
#define NUM_S6D74A0_SAMSUNG_GAMMA_NORMAL     (sizeof(s6d74a0_samsung_gamma_normal)/(sizeof(uint16) * 3))


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

static void lcd_s6d74a0_disp_powerup(void)
{
	if (!lcd_s6d74a0_state.disp_powered_up && !lcd_s6d74a0_state.display_on) 
    {
		/* Reset the hardware first */
		/* Include DAC power up implementation here */
	    lcd_s6d74a0_state.disp_powered_up = TRUE;
	}
}
/*delete*/

static void lcd_s6d74a0_panel_exit_sleep(void)
{
	unsigned char i = 0;

	if (lcd_s6d74a0_state.disp_powered_up && !lcd_s6d74a0_state.display_on)
    {
        seriout(0x0010, 0x0000);
        for(i = 0; i < NUM_S6D74A0_SAMSUNG_INIT; i++)
        {
            seriout(s6d74a0_samsung_init[i][0], s6d74a0_samsung_init[i][1]);
            
            
            /* if time-delayed less than 10ms,use mdelay function ,others use msleep function */
            if (s6d74a0_samsung_init[i][2] < 10){	 
                mdelay(s6d74a0_samsung_init[i][2]);
            }else {
                msleep(s6d74a0_samsung_init[i][2]);
            }
        }
        
        printk(KERN_ERR "lcd_s6d74a0_panel_exit_sleep:  LCD should be on!\n");
        
        lcd_s6d74a0_state.display_on = TRUE;    
    }
}

static int lcdc_s6d74a0_panel_off(struct platform_device *pdev)
{
	unsigned char i = 0;

	if (lcd_s6d74a0_state.disp_powered_up && lcd_s6d74a0_state.display_on)
    {
        for(i = 0; i < NUM_S6D74A0_SAMSUNG_DISP_OFF; i++)
        {
            seriout(s6d74a0_samsung_disp_off[i][0], s6d74a0_samsung_disp_off[i][1]);
            mdelay(s6d74a0_samsung_disp_off[i][2]);
        }     
		lcd_s6d74a0_state.display_on = FALSE;
	}
	return 0;
}

static void lcdc_s6d74a0_panel_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
        
   // lcd_set_backlight_pwm(bl_level);
    pwm_set_backlight(bl_level);
    
    return;
}

static void s6d74a0_reset(void)
{
    /* Reset LCD*/
    lcdc_s6d74a0_pdata->panel_config_gpio(1);
    lcd_reset_gpio = *(lcdc_s6d74a0_pdata->gpio_num + 4);
    
    /*s6d74a0_reset has been finished in OEMSBL*/
    
    gpio_set_value(lcd_reset_gpio, 0);
    msleep(10);
    gpio_set_value(lcd_reset_gpio, 1);
    msleep(50);
}
static int lcdc_s6d74a0_panel_on(struct platform_device *pdev)
{
	if (!lcd_s6d74a0_state.disp_initialized) 
    {
		/* Configure reset GPIO that drives DAC */
		lcdc_s6d74a0_pdata->panel_config_gpio(1);
		lcd_reset_gpio = *(lcdc_s6d74a0_pdata->gpio_num + 4);

		lcd_spi_init(lcdc_s6d74a0_pdata);	/* LCD needs SPI */
		lcd_s6d74a0_disp_powerup();
        lcd_s6d74a0_state.display_on = TRUE;
		lcd_s6d74a0_state.disp_initialized = TRUE;
	}
    else if(!lcd_s6d74a0_state.display_on)
    {
    	s6d74a0_reset();
        lcd_s6d74a0_panel_exit_sleep();
    }
	return 0;
}

static void lcdc_s6d74a0_panel_set_contrast(struct msm_fb_data_type *mfd, unsigned int contrast)
{
	unsigned char i = 0;

    if(0 == contrast)
    {
        if (lcd_s6d74a0_state.disp_powered_up && lcd_s6d74a0_state.display_on)
        {
            for(i = 0; i < NUM_S6D74A0_SAMSUNG_GAMMA_NORMAL; i++)
            {
                seriout(s6d74a0_samsung_gamma_normal[i][0], s6d74a0_samsung_gamma_normal[i][1]);
                mdelay(s6d74a0_samsung_gamma_normal[i][2]);
            }     
        }
    }
    else
    {
        if (lcd_s6d74a0_state.disp_powered_up && lcd_s6d74a0_state.display_on)
        {
            for(i = 0; i < NUM_S6D74A0_SAMSUNG_GAMMA_CAMREA; i++)
            {
                seriout(s6d74a0_samsung_gamma_for_camrea[i][0], s6d74a0_samsung_gamma_for_camrea[i][1]);
                mdelay(s6d74a0_samsung_gamma_for_camrea[i][2]);
            }     
        }
    }
    return;
}

static int __init lcdc_s6d74a0_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_s6d74a0_pdata = pdev->dev.platform_data;
		return 0;
	}
	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = 
{
	.probe  = lcdc_s6d74a0_probe,
	.driver = {
		.name   = "lcdc_s6d74a0_hvga",
	},
};
static struct msm_fb_panel_data s6d74a0_panel_data =
{
	.on = lcdc_s6d74a0_panel_on,
	.off = lcdc_s6d74a0_panel_off,
	.set_backlight = lcdc_s6d74a0_panel_set_backlight,
	.set_contrast = lcdc_s6d74a0_panel_set_contrast,
};

static struct platform_device this_device = 
{
	.name   = "lcdc_s6d74a0_hvga",
	.id	= 1,
	.dev	=
	{
		.platform_data = &s6d74a0_panel_data,
	}
};

static int __init lcdc_s6d74a0_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_FB_MSM_TRY_MDDI_CATCH_LCDC_PRISM
	//if (msm_fb_detect_client("lcdc_s6d74a0_hvga"))
	//	return 0;
#endif

    lcd_panel_hvga = lcd_panel_probe();
    if((LCD_S6D74A0_SAMSUNG_HVGA != lcd_panel_hvga) && \
       (msm_fb_detect_client("lcdc_s6d74a0_hvga"))
       )
    {
        return 0;
    }
    
	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &s6d74a0_panel_data.panel_info;

    pinfo->xres = 320;
	pinfo->yres = 480;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
/*modify HVGA LCD pclk frequency to 8.192MHz*/
    /*the pixel clk is different for different Resolution LCD*/
	//pinfo->clk_rate = 24500000; /*for VGA pixel clk*/
	pinfo->clk_rate = 9660000;    /*for HVGA pixel clk*/
    //pinfo->clk_rate = 6125000;  /*for QVGA pixel clk*/

    pinfo->lcdc.h_back_porch = 7;
	pinfo->lcdc.h_front_porch = 4;
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

module_init(lcdc_s6d74a0_panel_init);
