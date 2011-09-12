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

#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include "lcdc_huawei_config.h"

#define TRACE_LCD_DEBUG 0
#if TRACE_LCD_DEBUG
#define LCD_DEBUG(x...) printk(KERN_ERR "[LCD_DEBUG] " x)
#else
#define LCD_DEBUG(x...) do {} while (0)
#endif

#define lCD_DRIVER_NAME "lcdc_hx8357a_qvga"

#define LCD_MIN_BACKLIGHT_LEVEL 0
#define LCD_MAX_BACKLIGHT_LEVEL	255

#define DEVICE_ID 				0x74 //BS0=1
#define WRITE_REGISTER 			0x00
#define WRITE_CONTENT 			0x02

#ifdef CONFIG_HUAWEI_BACKLIGHT_USE_CABC
static uint16 lcd_backlight_level[LCD_MAX_BACKLIGHT_LEVEL + 1] = 
  {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0xFF};
#endif  

struct hx8357a_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

extern void pwm_set_backlight(int level);

static int lcd_reset_gpio;
static struct hx8357a_state_type hx8357a_state = { 0 };
static struct msm_panel_common_pdata *lcdc_hx8357a_pdata;
static lcd_panel_type lcd_panel_qvga = LCD_NONE;

struct command{
	uint32 reg;
	uint32 value;
	uint32 time;
};

static const struct command hx8357a_init_table[] =
{
    /* Select Command Page */
    {0xFF, 0x00, 0},

    /* Internal reference voltage Adjust. It is not open.*/
    {0xE2, 0x0B, 0}, //Internal reference voltage Adjust. It is not open.
    {0xE3, 0x03, 0}, //Internal reference voltage Adjust. It is not open.
    /*Gamma OP Bias Current setup time. It is not open. */
    {0xF2, 0x00, 0},
    /* used to tuned the timing of EQ function to save power. */
    {0xE4, 0x00, 0},
    {0xE5, 0x1C, 0},
    {0xE6, 0x00, 0},
    {0xE7, 0x1C, 0},
    /* OSC_EN=1, Start to Oscillate */
    {0x19, 0x01, 10},
    /* in Idle(8-color) / Partial Idle mode: 80% x 5.62MHz */
    /* in Normal / Partial mode: 85% x 5.62MHz */
    {0x18, 0x45, 0},
    /* SRAM area setting */
    {0x02, 0x00, 0},
    {0x03, 0x00, 0},
    {0x04, 0x01, 0},
    {0x05, 0x3F, 0},
    {0x06, 0x00, 0},
    {0x07, 0x00, 0},
    {0x08, 0x00, 0},
    {0x09, 0xEF, 0},
    /* SRAM start address setting */
    {0x80, 0x00, 0},
    {0x81, 0x00, 0},
    {0x82, 0x00, 0},
    {0x83, 0x00, 0},

    /* VCOM control */
    {0x24, 0x5C, 0},
    {0x25, 0x62, 0},
    
    /* Power control */
    {0x1B, 0x12, 10}, //VRH=12,Vreg1=4.2V
    {0x1D, 0x11, 10}, //0.5x H Line Frequency
    
    /* Gamma control */
    {0x40, 0x02, 0},
    {0x41, 0x21, 0},
    {0x42, 0x1F, 0},
    {0x43, 0x21, 0},
    {0x44, 0x1F, 0},
    {0x45, 0x3D, 0},
    {0x46, 0x0F, 0},
    {0x47, 0x61, 0},
    {0x48, 0x06, 0},
    {0x49, 0x05, 0},
    {0x4A, 0x06, 0},
    {0x4B, 0x11, 0},
    {0x4C, 0x1E, 0},
    {0x50, 0x02, 0},
    {0x51, 0x20, 0},
    {0x52, 0x1E, 0},
    {0x53, 0x20, 0},
    {0x54, 0x1E, 0},
    {0x55, 0x3D, 0},
    {0x56, 0x1E, 0},
    {0x57, 0x70, 0},
    {0x58, 0x01, 0},
    {0x59, 0x0E, 0},
    {0x5A, 0x19, 0},
    {0x5B, 0x1A, 0},
    {0x5C, 0x19, 0},
    {0x5D, 0x00, 0},
    /* Display Mode Control. DP_STB=0, Exit deep standby mode */
    {0x01, 0x00, 0},
    {0x17, 0x60, 0},
    {0x31, 0x02, 0},
    //{0x32, 0x0F, 0},
    {0x32, 0x0E, 0},
    {0x33, 0x02, 0},
    {0x34, 0x02, 0},
    /* Power Control */
    {0x1C, 0x03, 10},
    {0x1F, 0x80, 10},
    {0x1F, 0x90, 10},
    {0x1F, 0xD4, 10},
    {0x28, 0x04, 40},
    {0x28, 0x38, 40},
    {0x28, 0x3C, 40},
    //{0x17, 0x06, 0},
    //{0xFF, 0x00, 0},
    {0x80, 0x00, 0},
    {0x81, 0x00, 0},
    {0x82, 0x00, 0},
    {0x83, 0x00, 0},    
};

static const struct command hx8357a_standby_enter_table[] = 
{
	/* Select command page */
    { 0xFF, 0x00, 0},

	/* Display off */
    { 0x28, 0x38, 40},	// GON=1, DTE=1, D[1:0]=10
    { 0x28, 0X04, 40},	// GON=0, DTE=0, D[1:0]=01

	/*Power setting*/
    { 0x1F, 0x90, 5}, 	// Stop VCOMG, GAS_EN=1, VCOMG=0, PON=1, DK=0, XDK=0, DDVDH_TRI=0, STB=0
    { 0x1F, 0x88, 0},	// Stop step-up circuit, // GAS_EN=1, VCOMG=1, PON=0, DK=1, XDK=1, DDVDH_TRI=0, STB=0
	
    { 0x1C, 0x00, 0},	// AP=000
    { 0x1F, 0x89, 5},	// Enter Standby mode, GAS_EN=1, VCOMG=0, PON=0, DK=1, XDK=0, DDVDH_TRI=0, STB=1
    
    /* Stop oscillation OSC_EN=0*/
    { 0x19, 0x00, 0},
};

static const struct command hx8357a_standby_exit_table[] = 
{
	/* Select command page */
    { 0xFF, 0x00, 0},
    { 0x31, 0x00, 0}, // system interface
     
    /* Stop oscillation OSC_EN=1*/
    { 0x19, 0x01, 10},

	/* Release standby, STB=0*/
    { 0x1F, 0x88, 10},	//GAS_EN=1, VCOMG=0, PON=0, DK=1, XDK=0, DDVDH_TRI=0, STB=0
    
    /* Power on setting */
    { 0X1C, 0x03, 0},	// AP=011
    { 0x1F, 0x80, 5},	// Exit standby mode and Step-up circuit 1 enable, GAS_EN=1, VCOMG=0, PON=0, DK=0, XDK=0, DDVDH_TRI=0, STB=0
    { 0x1F, 0x90, 5},   // Step-up circuit 2 enable, GAS_EN=1, VCOMG=0, PON=1, DK=0, XDK=0, DDVDH_TRI=0, STB=0
    { 0x1F, 0xD4, 5},	// GAS_EN=1, VCOMG=1, PON=1, DK=0, XDK=1, DDVDH_TRI=0, STB=0

	/*Display on setting*/
    { 0x28, 0x08, 40},	// GON=0, DTE=0, D[1:0]=01
    { 0x28, 0x38, 40},	// GON=1, DTE=1, D[1:0]=10
    { 0x28, 0x3C, 5},	// GON=1, DTE=1, D[1:0]=11
    { 0x31, 0x02, 0}, //RGB interface
};

static const struct command hx8357a_display_area_table[] = 
{
	/* Select command page */
	{ 0xFF, 0x00, 0},
		
	/*Column address start register*/
	{ 0x02, 0x00, 0},
	{ 0x03, 0x00, 0},
	
	/*Column address end register*/
	{ 0x04, 0x01, 0},
	{ 0x05, 0xDF, 0},
	
	/*Row address start register*/
	{ 0x06, 0x00, 0},
	{ 0x07, 0x00, 0},
	
	/*Row address end register*/
	{ 0x08, 0x01, 0},
	{ 0x09, 0x3F, 0},
};

static void _serigo(uint8 reg, uint8 data)
{
    uint8 start_byte_reg = DEVICE_ID | WRITE_REGISTER;
    uint8 start_byte_data = DEVICE_ID | WRITE_CONTENT;
    
    seriout_transfer_byte(reg, start_byte_reg);
    seriout_transfer_byte(data, start_byte_data);
}

static void process_lcdc_table(struct command *table, size_t count)
{
    int i;
    uint32 reg = 0;
    uint32 value = 0;
    uint32 time = 0;

    for (i = 0; i < count; i++) {
        reg = table[i].reg;
        value = table[i].value;
        time = table[i].time;

        _serigo(reg, value);

        if (time != 0)
        	mdelay(time);
    }
}

static void hx8357a_disp_powerup(void)
{
    if (!hx8357a_state.disp_powered_up && !hx8357a_state.display_on) {
        /* Reset the hardware first */
        /* Include DAC power up implementation here */
        hx8357a_state.disp_powered_up = TRUE;
    }
}

static void hx8357a_disp_on(void)
{
    if (hx8357a_state.disp_powered_up && !hx8357a_state.display_on) 
    {
        LCD_DEBUG("%s: disp on lcd\n", __func__);
        /* Initialize LCD */
        //process_lcdc_table((struct command*)&hx8357a_init_table, ARRAY_SIZE(hx8357a_init_table));
        //seriout_transfer_byte(0x22, DEVICE_ID | WRITE_REGISTER);
        hx8357a_state.display_on = TRUE;
    }
}

static void hx8357a_reset(void)
{
    /* Reset LCD*/
    lcdc_hx8357a_pdata->panel_config_gpio(1);
    lcd_reset_gpio = *(lcdc_hx8357a_pdata->gpio_num + 4);
    
    /*hx8357a_reset has been finished in OEMSBL*/
    //gpio_set_value(lcd_reset_gpio, 0);
    //mdelay(50);
    //gpio_set_value(lcd_reset_gpio, 1);
    //mdelay(150);
}

static int hx8357a_panel_on(struct platform_device *pdev)
{
    if (!hx8357a_state.disp_initialized) 
    {
        hx8357a_reset();
        lcd_spi_init(lcdc_hx8357a_pdata);	/* LCD needs SPI */
        hx8357a_disp_powerup();
        hx8357a_disp_on();
        hx8357a_state.disp_initialized = TRUE;
        LCD_DEBUG("%s: hx8357a lcd initialized\n", __func__);
    } 
    else if (!hx8357a_state.display_on) 
    {
        /* Exit Standby Mode */
        process_lcdc_table((struct command*)&hx8357a_standby_exit_table, ARRAY_SIZE(hx8357a_standby_exit_table));
        LCD_DEBUG("%s: Exit Standby Mode\n", __func__);
        hx8357a_state.display_on = TRUE;
    }
    
    return 0;
}

static int hx8357a_panel_off(struct platform_device *pdev)
{
    if (hx8357a_state.disp_powered_up && hx8357a_state.display_on) {
        /* Enter Standby Mode */
        process_lcdc_table((struct command*)&hx8357a_standby_enter_table, ARRAY_SIZE(hx8357a_standby_enter_table));
        hx8357a_state.display_on = FALSE;
        LCD_DEBUG("%s: Enter Standby Mode\n", __func__);
    }

    return 0;
}

static void hx8357a_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
       
#ifdef CONFIG_HUAWEI_BACKLIGHT_USE_CABC
    if(LCD_MAX_BACKLIGHT_LEVEL < mfd->bl_level) {
        mfd->bl_level = 1;
    }

    /*BCTRL	=1	(Backlight Control Block, This bit is always used to switch brightness for display.)
       DD	=0	(Display Dimming)
       BL		=1	(Backlight Control)*/ 
    serigo(0x3D, 0x24);
    /*Set backlight level*/
    serigo(0x3C, lcd_backlight_level[mfd->bl_level]);

    /* Set still picture mode for content adaptive image functionality*/
    serigo(0x3E, 0x02);

    /* set the minimum brightness value of the display for CABC function*/
    serigo(0x3F, 0x00);
#else
   // lcd_set_backlight_pwm(bl_level);
    pwm_set_backlight(bl_level);
#endif
     return;
}

static void hx8357a_panel_set_contrast(struct msm_fb_data_type *mfd, unsigned int contrast)
{

    return;
}

static int __init hx8357a_probe(struct platform_device *pdev)
{
    if (pdev->id == 0) {
        lcdc_hx8357a_pdata = pdev->dev.platform_data;
        return 0;
    }
    msm_fb_add_device(pdev);
    return 0;
}

static struct platform_driver this_driver = {
    .probe  = hx8357a_probe,
    .driver = {
    	.name   = lCD_DRIVER_NAME,
    },
};

static struct msm_fb_panel_data hx8357a_panel_data = {
    .on = hx8357a_panel_on,
    .off = hx8357a_panel_off,
    .set_backlight = hx8357a_set_backlight,
    .set_contrast = hx8357a_panel_set_contrast,
};

static struct platform_device this_device = {
    .name   = lCD_DRIVER_NAME,
    .id	= 1,
    .dev	= {
    	.platform_data = &hx8357a_panel_data,
    }
};

static int __init hx8357a_panel_init(void)
{
    int ret;
    struct msm_panel_info *pinfo;

    lcd_panel_qvga = lcd_panel_probe();
/* U8300 need to support the HX8368a ic driver of TRULY LCD */
    if((LCD_HX8357A_BYD_QVGA != lcd_panel_qvga) && \ 
       (msm_fb_detect_client(lCD_DRIVER_NAME))
      )
    {
        return 0;
    }

    LCD_DEBUG(" lcd_type=%s, lcd_panel_qvga = %d\n", lCD_DRIVER_NAME, lcd_panel_qvga);

    ret = platform_driver_register(&this_driver);
    if (ret)
        return ret;

    pinfo = &hx8357a_panel_data.panel_info;
    pinfo->xres = 320;
    pinfo->yres = 240;
    pinfo->type = LCDC_PANEL;
    pinfo->pdest = DISPLAY_1;
    pinfo->wait_cycle = 0;
    pinfo->bpp = 18;
    pinfo->fb_num = 2;
    pinfo->bl_max = LCD_MAX_BACKLIGHT_LEVEL;
    pinfo->bl_min = LCD_MIN_BACKLIGHT_LEVEL;

    pinfo->clk_rate = 6125000;  /*for QVGA pixel clk*/   
    pinfo->lcdc.h_back_porch = 2;
    pinfo->lcdc.h_front_porch = 2;
    pinfo->lcdc.h_pulse_width = 2;
    pinfo->lcdc.v_back_porch = 2;
    pinfo->lcdc.v_front_porch = 2;
    pinfo->lcdc.v_pulse_width = 2;

    pinfo->lcdc.border_clr = 0;     /* blk */
    pinfo->lcdc.underflow_clr = 0xff;       /* blue */
    pinfo->lcdc.hsync_skew = 0;

    ret = platform_device_register(&this_device);
    if (ret)
        platform_driver_unregister(&this_driver);

    return ret;
}

module_init(hx8357a_panel_init);

