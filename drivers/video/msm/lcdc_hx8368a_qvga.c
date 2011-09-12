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
#include "linux/hardware_self_adapt.h"
#include "msm_fb.h"
#include "lcdc_huawei_config.h"

#undef  LCD_HX8368A_REAL_TIME_DEGUG
//#define LCD_HX8368A_REAL_TIME_DEGUG

#ifdef LCD_HX8368A_REAL_TIME_DEGUG
#include "lcd_hw_debug.h"
#endif

#define TRACE_LCD_DEBUG 0
#if TRACE_LCD_DEBUG
#define LCD_DEBUG(x...) printk(KERN_ERR "[LCD_DEBUG] " x)
#else
#define LCD_DEBUG(x...) do {} while (0)
#endif

#define lCD_DRIVER_NAME "lcdc_hx8368a_qvga"

#define LCD_MIN_BACKLIGHT_LEVEL 0
#define LCD_MAX_BACKLIGHT_LEVEL	255

#ifdef CONFIG_HUAWEI_BACKLIGHT_USE_CABC
static uint16 lcd_backlight_level[LCD_MAX_BACKLIGHT_LEVEL + 1] = 
  {0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0xFF};
#endif  

struct hx8368a_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

extern void pwm_set_backlight(int level);

static struct hx8368a_state_type hx8368a_state = { 0 };
static struct msm_panel_common_pdata *lcdc_hx8368a_pdata;
static lcd_panel_type lcd_panel_qvga = LCD_NONE;

/*===========================================================================

                      LCD command sequences 

===========================================================================*/
#define TYPE_COMMAND	1<<0
#define TYPE_PARAMETER  1<<1

struct sequence{
	uint32 value;
	uint32 type;
	uint32 time; //unit is ms
};

#ifdef LCD_HX8368A_REAL_TIME_DEGUG
struct sequence *lcdc_hx8368a_hvga_init_table = NULL;
#endif

/*===========================================================================

                      SEIKO LCD command sequence

===========================================================================*/

static const struct sequence hx8368a_seiko_init_table[] =
{
    //Sleep out
    {0x11, TYPE_COMMAND, 	120},

    //Set CPCRC Related Setting
    {0xB9, TYPE_COMMAND, 	0},
	{0xFF, TYPE_PARAMETER, 	0},
	{0x83, TYPE_PARAMETER, 	0},
	{0x68, TYPE_PARAMETER, 	5},

	//SETRGBIF
	{0xB3, TYPE_COMMAND,	0},
	{0x0E, TYPE_PARAMETER, 	5},

	//Set VCOM Voltage
	{0xB6, TYPE_COMMAND, 	0},
    {0x95, TYPE_PARAMETER, 	0},
    {0x46, TYPE_PARAMETER, 	0},
    {0x53, TYPE_PARAMETER, 	100},

	//Set Panel Setting
	{0xCC, TYPE_COMMAND, 	0},
    {0x0D, TYPE_PARAMETER, 	100},

	//Set Gamma Curve Related Setting
	{0xE0, TYPE_COMMAND, 	0},
	{0x00, TYPE_PARAMETER, 	0}, //VRN0
    {0x2B, TYPE_PARAMETER, 	0}, //VRN1
    {0x28, TYPE_PARAMETER, 	0}, //VRN2
    {0x27, TYPE_PARAMETER, 	0}, //VRN3
    {0x27, TYPE_PARAMETER, 	0}, //VRN4
    {0x3F, TYPE_PARAMETER, 	0}, //VRN5
    {0x21, TYPE_PARAMETER, 	0}, //PRN0
    {0x6B, TYPE_PARAMETER, 	0}, //PRN1
    {0x08, TYPE_PARAMETER, 	0}, //PKN0
    {0x05, TYPE_PARAMETER, 	0}, //PKN1
    {0x07, TYPE_PARAMETER, 	0}, //PKN2
    {0x0D, TYPE_PARAMETER, 	0}, //PKN3
    {0x19, TYPE_PARAMETER, 	0}, //PKN4
    {0x00, TYPE_PARAMETER, 	0}, //VRP0
    {0x18, TYPE_PARAMETER, 	0}, //VRP1
    {0x18, TYPE_PARAMETER, 	0}, //VRP2
    {0x17, TYPE_PARAMETER, 	0}, //VRP3
    {0x14, TYPE_PARAMETER, 	0}, //VRP4
    {0x3F, TYPE_PARAMETER, 	0}, //VRP5
    {0x14, TYPE_PARAMETER, 	0}, //PRP0
    {0x5E, TYPE_PARAMETER, 	0}, //PRP1
    {0x06, TYPE_PARAMETER, 	0}, //PKP0
    {0x12, TYPE_PARAMETER, 	0}, //PKP1
    {0x18, TYPE_PARAMETER, 	0}, //PKP2
    {0x1A, TYPE_PARAMETER, 	0}, //PKP3
    {0x17, TYPE_PARAMETER, 	0}, //PKP4
    {0xFF, TYPE_PARAMETER, 	5}, //CGMP1,CGMP0,CGMN1,CGMN0

	//SETMESSI
    {0xEA, TYPE_COMMAND, 	0}, 
    {0x00, TYPE_PARAMETER, 	5},

	//DISPON
	{0x29, TYPE_COMMAND, 	100},

	//Start to write data
	{0x2C, TYPE_COMMAND, 	0},
};

static const struct sequence hx8368a_seiko_standby_enter_table[] = 
{
	{0x10, TYPE_COMMAND, 	120}
};

static const struct sequence hx8368a_seiko_Panel_reverse_table[] = 
{
	//Set Panel Setting
	{0xCC, TYPE_COMMAND, 	0},
    {0x01, TYPE_PARAMETER, 	0},
};

static const struct sequence hx8368a_seiko_standby_exit_table[] = 
{
    //Sleep out
    {0x11, TYPE_COMMAND, 	120},
	/*delete initially setting  */
};

/*===========================================================================

                      TRULY LCD command sequence

===========================================================================*/

static const struct sequence hx8368a_truly_init_table[] =
{	
    //Set CPCRC Related Setting
    {0xB9, TYPE_COMMAND, 	0},
    {0xFF, TYPE_PARAMETER, 	0},
    {0x83, TYPE_PARAMETER, 	0},
    {0x68, TYPE_PARAMETER, 	5},
    {0xBB, TYPE_COMMAND,        0}, //-set otp
    {0x00, TYPE_PARAMETER,      0},
    {0x00, TYPE_PARAMETER,      0},
    {0x80, TYPE_PARAMETER,      5}, //otp load disable
                 
    //Gama curve Selected
    {0xB6, TYPE_COMMAND,    0},
    {0xC9, TYPE_PARAMETER,  0},
    {0x37, TYPE_PARAMETER, 	0},
    {0x10, TYPE_PARAMETER, 	10},
    
    //Set Gamma Curve Related Setting
    {0xE0, TYPE_COMMAND, 	0},
    {0x04, TYPE_PARAMETER, 	0}, //VRN0
    {0x23, TYPE_PARAMETER, 	0}, //VRN1
    {0x21, TYPE_PARAMETER, 	0}, //VRN2
    {0x27, TYPE_PARAMETER, 	0}, //VRN3
    {0x27, TYPE_PARAMETER, 	0}, //VRN4
    {0x3F, TYPE_PARAMETER, 	0}, //VRN5
    {0x14, TYPE_PARAMETER, 	0}, //PRN0
    {0x5D, TYPE_PARAMETER, 	0}, //PRN1
    {0x08, TYPE_PARAMETER, 	0}, //PKN0
    {0x06, TYPE_PARAMETER, 	0}, //PKN1
    {0x08, TYPE_PARAMETER, 	0}, //PKN2
    {0x0D, TYPE_PARAMETER, 	0}, //PKN3
    {0x19, TYPE_PARAMETER, 	0}, //PKN4
    {0x00, TYPE_PARAMETER, 	0}, //VRP0
    {0x18, TYPE_PARAMETER, 	0}, //VRP1
    {0x18, TYPE_PARAMETER, 	0}, //VRP2
    {0x1E, TYPE_PARAMETER, 	0}, //VRP3
    {0x1C, TYPE_PARAMETER, 	0}, //VRP4
    {0x3B, TYPE_PARAMETER, 	0}, //VRP5
    {0x22, TYPE_PARAMETER, 	0}, //PRP0
    {0x75, TYPE_PARAMETER, 	0}, //PRP1
    {0x06, TYPE_PARAMETER, 	0}, //PKP0
    {0x12, TYPE_PARAMETER, 	0}, //PKP1
    {0x17, TYPE_PARAMETER, 	0}, //PKP2
    {0x19, TYPE_PARAMETER, 	0}, //PKP3
    {0x00, TYPE_PARAMETER, 	0}, //PKP4
    {0xFF, TYPE_PARAMETER, 	5}, //CGMP1,CGMP0,CGMN1,CGMN0
    
    //Sleep out
    {0x11, TYPE_COMMAND, 	100},
    
    //SETMESSI
    {0xEA, TYPE_COMMAND, 	0}, 
    {0x02, TYPE_PARAMETER, 	5},
    {0x3A, TYPE_COMMAND, 	0}, 
    {0x66, TYPE_PARAMETER, 	5},
        
    //For Himax internel use
    {0xE3, TYPE_COMMAND, 	0},
    {0x00, TYPE_PARAMETER, 	0},
    {0x40, TYPE_PARAMETER, 	0},
    {0x40, TYPE_PARAMETER, 	0},
    {0x00, TYPE_PARAMETER, 	5},
    	
    //Look up table for 65K color
    {0x2D, TYPE_COMMAND, 	0},
    //Red
    {0x00, TYPE_PARAMETER, 	0},
    {0x02, TYPE_PARAMETER, 	0},
    {0x04, TYPE_PARAMETER, 	0},
    {0x06, TYPE_PARAMETER, 	0},
    {0x08, TYPE_PARAMETER, 	0},
    {0x0A, TYPE_PARAMETER, 	0},
    {0x0C, TYPE_PARAMETER, 	0},
    {0x0E, TYPE_PARAMETER, 	0},
    {0x10, TYPE_PARAMETER, 	0},
    {0x12, TYPE_PARAMETER, 	0},
    {0x14, TYPE_PARAMETER, 	0},
    {0x16, TYPE_PARAMETER, 	0},
    {0x18, TYPE_PARAMETER, 	0},
    {0x1A, TYPE_PARAMETER, 	0},
    {0x1C, TYPE_PARAMETER, 	0},
    {0x1E, TYPE_PARAMETER, 	0},
    {0x20, TYPE_PARAMETER, 	0},
    {0x22, TYPE_PARAMETER, 	0},
    {0x24, TYPE_PARAMETER, 	0},
    {0x26, TYPE_PARAMETER, 	0},
    {0x28, TYPE_PARAMETER, 	0},
    {0x2A, TYPE_PARAMETER, 	0},
    {0x2C, TYPE_PARAMETER, 	0},
    {0x2E, TYPE_PARAMETER, 	0},
    {0x30, TYPE_PARAMETER, 	0},
    {0x32, TYPE_PARAMETER, 	0},
    {0x34, TYPE_PARAMETER, 	0},
    {0x36, TYPE_PARAMETER, 	0},
    {0x38, TYPE_PARAMETER, 	0},
    {0x3A, TYPE_PARAMETER, 	0},
    {0x3C, TYPE_PARAMETER, 	0},
    {0x3E, TYPE_PARAMETER, 	0},
    
    //Green
    {0x00, TYPE_PARAMETER, 	0},
    {0x01, TYPE_PARAMETER, 	0},
    {0x02, TYPE_PARAMETER, 	0},
    {0x03, TYPE_PARAMETER, 	0},
    {0x04, TYPE_PARAMETER, 	0},
    {0x05, TYPE_PARAMETER, 	0},
    {0x06, TYPE_PARAMETER, 	0},
    {0x07, TYPE_PARAMETER, 	0},
    {0x08, TYPE_PARAMETER, 	0},
    {0x09, TYPE_PARAMETER, 	0},
    {0x0A, TYPE_PARAMETER, 	0},
    {0x0B, TYPE_PARAMETER, 	0},
    {0x0C, TYPE_PARAMETER, 	0},
    {0x0D, TYPE_PARAMETER, 	0},
    {0x0E, TYPE_PARAMETER, 	0},
    {0x0F, TYPE_PARAMETER, 	0},
    {0x10, TYPE_PARAMETER, 	0},
    {0x11, TYPE_PARAMETER, 	0},
    {0x12, TYPE_PARAMETER, 	0},
    {0x13, TYPE_PARAMETER, 	0},
    {0x14, TYPE_PARAMETER, 	0},
    {0x15, TYPE_PARAMETER, 	0},
    {0x16, TYPE_PARAMETER, 	0},
    {0x17, TYPE_PARAMETER, 	0},
    {0x18, TYPE_PARAMETER, 	0},
    {0x19, TYPE_PARAMETER, 	0},
    {0x1A, TYPE_PARAMETER, 	0},
    {0x1B, TYPE_PARAMETER, 	0},
    {0x1C, TYPE_PARAMETER, 	0},
    {0x1D, TYPE_PARAMETER, 	0},
    {0x1E, TYPE_PARAMETER, 	0},
    {0x1F, TYPE_PARAMETER, 	0},
    {0x20, TYPE_PARAMETER, 	0},
    {0x21, TYPE_PARAMETER, 	0},
    {0x22, TYPE_PARAMETER, 	0},
    {0x23, TYPE_PARAMETER, 	0},
    {0x24, TYPE_PARAMETER, 	0},
    {0x25, TYPE_PARAMETER, 	0},
    {0x26, TYPE_PARAMETER, 	0},
    {0x27, TYPE_PARAMETER, 	0},
    {0x28, TYPE_PARAMETER, 	0},
    {0x29, TYPE_PARAMETER, 	0},
    {0x2A, TYPE_PARAMETER, 	0},
    {0x2B, TYPE_PARAMETER, 	0},
    {0x2C, TYPE_PARAMETER, 	0},
    {0x2D, TYPE_PARAMETER, 	0},
    {0x2E, TYPE_PARAMETER, 	0},
    {0x2F, TYPE_PARAMETER, 	0},
    {0x30, TYPE_PARAMETER, 	0},
    {0x31, TYPE_PARAMETER, 	0},
    {0x32, TYPE_PARAMETER, 	0},
    {0x33, TYPE_PARAMETER, 	0},
    {0x34, TYPE_PARAMETER, 	0},
    {0x35, TYPE_PARAMETER, 	0},
    {0x36, TYPE_PARAMETER, 	0},
    {0x37, TYPE_PARAMETER, 	0},
    {0x38, TYPE_PARAMETER, 	0},
    {0x39, TYPE_PARAMETER, 	0},
    {0x3A, TYPE_PARAMETER, 	0},
    {0x3B, TYPE_PARAMETER, 	0},
    {0x3C, TYPE_PARAMETER, 	0},
    {0x3D, TYPE_PARAMETER, 	0},
    {0x3E, TYPE_PARAMETER, 	0},
    {0x3F, TYPE_PARAMETER, 	0},
    
    //Blue
    {0x00, TYPE_PARAMETER, 	0},
    {0x02, TYPE_PARAMETER, 	0},
    {0x04, TYPE_PARAMETER, 	0},
    {0x06, TYPE_PARAMETER, 	0},
    {0x08, TYPE_PARAMETER, 	0},
    {0x0A, TYPE_PARAMETER, 	0},
    {0x0C, TYPE_PARAMETER, 	0},
    {0x0E, TYPE_PARAMETER, 	0},
    {0x10, TYPE_PARAMETER, 	0},
    {0x12, TYPE_PARAMETER, 	0},
    {0x14, TYPE_PARAMETER, 	0},
    {0x16, TYPE_PARAMETER, 	0},
    {0x18, TYPE_PARAMETER, 	0},
    {0x1A, TYPE_PARAMETER, 	0},
    {0x1C, TYPE_PARAMETER, 	0},
    {0x1E, TYPE_PARAMETER, 	0},
    {0x20, TYPE_PARAMETER, 	0},
    {0x22, TYPE_PARAMETER, 	0},
    {0x24, TYPE_PARAMETER, 	0},
    {0x26, TYPE_PARAMETER, 	0},
    {0x28, TYPE_PARAMETER, 	0},
    {0x2A, TYPE_PARAMETER, 	0},
    {0x2C, TYPE_PARAMETER, 	0},
    {0x2E, TYPE_PARAMETER, 	0},
    {0x30, TYPE_PARAMETER, 	0},
    {0x32, TYPE_PARAMETER, 	0},
    {0x34, TYPE_PARAMETER, 	0},
    {0x36, TYPE_PARAMETER, 	0},
    {0x38, TYPE_PARAMETER, 	0},
    {0x3A, TYPE_PARAMETER, 	0},
    {0x3C, TYPE_PARAMETER, 	0},
    {0x3E, TYPE_PARAMETER, 	5},
    
    //For power
    {0xB1, TYPE_COMMAND, 	0},
    {0x00, TYPE_PARAMETER, 	0},
    {0x02, TYPE_PARAMETER, 	0},
    {0x30, TYPE_PARAMETER, 	0},
    {0x04, TYPE_PARAMETER, 	0},
    {0x22, TYPE_PARAMETER, 	0},
    {0x11, TYPE_PARAMETER, 	0},
    {0xD4, TYPE_PARAMETER,	10},
    
    //SETRGBIF
    {0xB3, TYPE_COMMAND,	0},
    {0x0E, TYPE_PARAMETER, 	5},
    
    //Set OSC
    {0xB0, TYPE_COMMAND,	0},
    {0x0D, TYPE_PARAMETER,	0},
    
    {0xCC, TYPE_COMMAND,	0},
    {0x0D, TYPE_PARAMETER,	0},
    
    {0x29, TYPE_COMMAND,	5},
    
    {0x2A, TYPE_COMMAND,	0},
    {0x00, TYPE_PARAMETER,	0},
    {0x00, TYPE_PARAMETER, 	0},
    {0x01, TYPE_PARAMETER, 	0},
    {0x3F, TYPE_PARAMETER, 	0},
    
    {0x2B, TYPE_COMMAND, 	0},
    {0x00, TYPE_PARAMETER, 	0},
    {0x00, TYPE_PARAMETER,	0},
    {0x00, TYPE_PARAMETER,	0},
    {0xEF, TYPE_PARAMETER,	0},
    
    //Start to write data
    {0x2C, TYPE_COMMAND, 	0},
};

static const struct sequence hx8368a_truly_standby_enter_table[] = 
{
	{0x10, TYPE_COMMAND, 	120}
};

static const struct sequence hx8368a_truly_Panel_reverse_table[] = 
{
	//Set Panel Setting
	{0xCC, TYPE_COMMAND, 	0},
    {0x01, TYPE_PARAMETER, 	0},
};

static const struct sequence hx8368a_truly_standby_exit_table[] = 
{	
	/*delete initially setting  */
    //Sleep out
    {0x11, TYPE_COMMAND, 	120},
};

/*===========================================================================

                      3-Wire Serial Port : Type C -Option 2

===========================================================================*/
#define START_BYTE_COMMAND 		0x00
#define START_BYTE_PARAMETER	0x01

#ifndef ARRAY_SIZE
#define  ARRAY_SIZE(a) (sizeof((a))/sizeof((a)[0]))
#endif

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

        seriout_transfer_byte(value, start_byte);
        
        if (time != 0)
        {
            /* if the time will be delayed is more than 10ms,msleep should be used */
            if (time > 10)
            {
            	msleep(time);
            }
            else
            {
              	mdelay(time);
            }
        }
    }
}

static void hx8368a_set_reset_low(void)
{
	int lcd_reset_gpio = 0;
    lcd_reset_gpio = *(lcdc_hx8368a_pdata->gpio_num + 4);

	gpio_set_value(lcd_reset_gpio, 0);
}

static void hx8368a_set_reset_high(void)
{
	int lcd_reset_gpio = 0;

	/* delete this line ,do the work at initial part */
    lcd_reset_gpio = *(lcdc_hx8368a_pdata->gpio_num + 4);

	gpio_set_value(lcd_reset_gpio, 1);
}

static int hx8368a_panel_on(struct platform_device *pdev)
{
    #ifdef LCD_HX8368A_REAL_TIME_DEGUG
    boolean para_debug_flag = FALSE;
    uint32 para_num = 0;

    para_debug_flag = lcd_debug_malloc_get_para( "lcdc_hx8368a_hvga_init_table", 
    (void**)&lcdc_hx8368a_hvga_init_table,&para_num);
    #endif
        
    if (!hx8368a_state.disp_initialized) {
        lcdc_hx8368a_pdata->panel_config_gpio(1);

        lcd_spi_init(lcdc_hx8368a_pdata);	/* LCD needs SPI */
		//LCD is initialized by OEMSBL
        hx8368a_state.disp_initialized = TRUE;
		hx8368a_state.display_on = TRUE;
        LCD_DEBUG("%s: LCD initialized\n", __func__);
    } else if (!hx8368a_state.display_on) {
        /* Exit Standby Mode */
		/*delete code  four lines*/

		switch (lcd_panel_qvga) {
			case LCD_HX8368A_SEIKO_QVGA:
				process_lcdc_table((struct sequence*)&hx8368a_seiko_standby_exit_table, ARRAY_SIZE(hx8368a_seiko_standby_exit_table));
				/* u8350 Panel_reverse */
				if(machine_is_msm7x25_u8350())
				{
					process_lcdc_table((struct sequence*)&hx8368a_seiko_Panel_reverse_table, ARRAY_SIZE(hx8368a_seiko_Panel_reverse_table));
				}
				hx8368a_state.display_on = TRUE;
				break;
				
			case LCD_HX8368A_TRULY_QVGA:
			default:
        		process_lcdc_table((struct sequence*)&hx8368a_truly_standby_exit_table, ARRAY_SIZE(hx8368a_truly_standby_exit_table));
				/* u8350 Panel_reverse */
				if(machine_is_msm7x25_u8350())
				{
					process_lcdc_table((struct sequence*)&hx8368a_truly_Panel_reverse_table, ARRAY_SIZE(hx8368a_truly_Panel_reverse_table));
				}
				hx8368a_state.display_on = TRUE;
				break;
		}	
        LCD_DEBUG("%s: Exit Standby Mode\n", __func__);
		
    }
    #ifdef LCD_HX8368A_REAL_TIME_DEGUG
    if((TRUE == para_debug_flag)&&(NULL != lcdc_hx8368a_hvga_init_table))
	{
	    process_lcdc_table(lcdc_hx8368a_hvga_init_table, para_num);
	    printk("lcdc_hx8368a_hvga_init_table\n");
	    lcd_debug_free_para((void *)lcdc_hx8368a_hvga_init_table);
	}
    #endif
    return 0;
}

static int hx8368a_panel_off(struct platform_device *pdev)
{
	/* Enter Standby Mode */
    if (hx8368a_state.display_on) {
		switch (lcd_panel_qvga) {
			case LCD_HX8368A_SEIKO_QVGA:
				process_lcdc_table((struct sequence*)&hx8368a_seiko_standby_enter_table, ARRAY_SIZE(hx8368a_seiko_standby_enter_table));
				hx8368a_state.display_on = FALSE;
				break;
				
			case LCD_HX8368A_TRULY_QVGA:
			default:
		        process_lcdc_table((struct sequence*)&hx8368a_truly_standby_enter_table, ARRAY_SIZE(hx8368a_truly_standby_enter_table));
		        hx8368a_state.display_on = FALSE;
				break;
    	}
        LCD_DEBUG("%s: Enter Standby Mode\n", __func__);
		/*delete code one line */
    }

    return 0;
}

static void hx8368a_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
      
    pwm_set_backlight(bl_level);

    return;
}

static int __init hx8368a_probe(struct platform_device *pdev)
{
    if (pdev->id == 0) {
        lcdc_hx8368a_pdata = pdev->dev.platform_data;
        return 0;
    }
    msm_fb_add_device(pdev);
    return 0;
}

static struct platform_driver this_driver = {
    .probe  = hx8368a_probe,
    .driver = {
    	.name   = lCD_DRIVER_NAME,
    },
};

static struct msm_fb_panel_data hx8368a_panel_data = {
    .on = hx8368a_panel_on,
    .off = hx8368a_panel_off,
    .set_backlight = hx8368a_set_backlight,
};

static struct platform_device this_device = {
    .name   = lCD_DRIVER_NAME,
    .id	= 1,
    .dev	= {
    	.platform_data = &hx8368a_panel_data,
    }
};

static int __init hx8368a_panel_init(void)
{
    int ret;
    struct msm_panel_info *pinfo;

    lcd_panel_qvga = lcd_panel_probe();
    if((LCD_HX8368A_SEIKO_QVGA != lcd_panel_qvga) && (LCD_HX8368A_TRULY_QVGA!=lcd_panel_qvga) )
   {
        return 0;
    }

    LCD_DEBUG(" lcd_type=%s, lcd_panel_qvga = %d\n", lCD_DRIVER_NAME, lcd_panel_qvga);

    ret = platform_driver_register(&this_driver);
    if (ret)
        return ret;

    pinfo = &hx8368a_panel_data.panel_info;
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

module_init(hx8368a_panel_init);

