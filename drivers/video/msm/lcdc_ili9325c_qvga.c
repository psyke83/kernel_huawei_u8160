

#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include "lcdc_huawei_config.h"

#undef  LCD_ILI9325C_REAL_TIME_DEGUG
//#define LCD_ILI9325C_REAL_TIME_DEGUG

#ifdef LCD_ILI9325C_REAL_TIME_DEGUG
#include "lcd_hw_debug.h"
#endif

struct lcd_ili9325c_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

#ifdef LCD_ILI9325C_REAL_TIME_DEGUG
struct sequence{
    uint32 reg;
    uint32 value;
    uint32 time; //unit is ms
};
#endif

extern void pwm_set_backlight(int level);

static int lcd_reset_gpio;
static struct lcd_ili9325c_state_type lcd_ili9325c_state = { 0 };
static struct msm_panel_common_pdata *lcdc_ili9325c_pdata;
static lcd_panel_type lcd_panel_qvga = LCD_NONE;

#ifdef LCD_ILI9325C_REAL_TIME_DEGUG
struct sequence *lcdc_ili9325c_hvga_init_table = NULL;
#endif

static uint16 ili9325c_wintek_disp_off[][3] = 
{
        {0x0007, 0x0131, 10},
        {0x0007, 0x0130, 10},
        {0x0007, 0x0000, 0},
        /*power off sequence*/
        {0x0010, 0x0080, 0},
        {0x0011, 0x0000, 0},
        {0x0012, 0x0000, 0},
        {0x0013, 0x0000, 200},
        {0x0010, 0x0082, 0},
};
static uint16 ili9325c_wintek_disp_on[][3] = 
{
        /*power on sequence*/
        {0x0010, 0x0080, 0},
        {0x0011, 0x0000, 0},
        {0x0012, 0x0000, 0},
        {0x0013, 0x0000, 100},
        {0x0010, 0x1490, 0},
        {0x0011, 0x0225, 20},
        {0x0011, 0x0227, 50},
        {0x0012, 0x008B, 50},
        {0x0013, 0x1300, 0},
        {0x0029, 0x0032, 50},
        {0x0007, 0x0133, 0},

};


#define NUM_ILI9325C_WINTEK_DISP_OFF          (sizeof(ili9325c_wintek_disp_off)/(sizeof(uint16) * 3))
#define NUM_ILI9325C_WINTEK_DISP_ON           (sizeof(ili9325c_wintek_disp_on)/(sizeof(uint16) * 3)) 

static void seriout(uint16 reg, uint16 data)
{
    uint8 start_byte_reg = 0x70;
    uint8 start_byte_data = 0x72;
    
    seriout_cmd(reg, start_byte_reg);
    seriout_data(data, start_byte_data);
}

#ifdef LCD_ILI9325C_REAL_TIME_DEGUG
static void process_lcdc_table(struct sequence *table, size_t count)
{
    int i;
    uint32 reg = 0;
    uint32 value = 0;
    uint32 time = 0;

    for (i = 0; i < count; i++) {
        reg = table[i].reg;
        value = table[i].value;
        time = table[i].time;

        seriout(reg, value);

        if (time != 0)
        	mdelay(time);
    }
}
#endif

void lcdc_ili9325c_cabc_set_backlight(uint8 level)
{  
    seriout(0x00B1, (uint16)level);
    seriout(0x00B3, 0x0024);
    /*close cabc*/
    seriout(0x00B5, 0x0000);
    seriout(0x00BE, 0x0030);
    seriout(0x00C8, 0x0000);
}

static void lcd_ili9325c_disp_powerup(void)
{
	if (!lcd_ili9325c_state.disp_powered_up && !lcd_ili9325c_state.display_on) 
    {
		/* Reset the hardware first */
		/* Include DAC power up implementation here */
	    lcd_ili9325c_state.disp_powered_up = TRUE;
	}
}

static void lcd_ili9325c_disp_exit_sleep(void)
{
	unsigned char i = 0;

	if (lcd_ili9325c_state.disp_powered_up && !lcd_ili9325c_state.display_on)
    {
        switch (lcd_panel_qvga) 
        {
            case LCD_ILI9325C_WINTEK_QVGA:
                for(i = 0; i < NUM_ILI9325C_WINTEK_DISP_ON; i++)
                {
                    seriout(ili9325c_wintek_disp_on[i][0], ili9325c_wintek_disp_on[i][1]);
                    mdelay(ili9325c_wintek_disp_on[i][2]);
                }
                lcdc_ili9325c_cabc_set_backlight(0xFF);
                break;

            default :
                break;
              
        }
        printk(KERN_ERR "lcd_ili9325c_disp_exit_sleep:  LCD should be on, LCD_Panel = %d!\n", lcd_panel_qvga);

        lcd_ili9325c_state.display_on = TRUE;
    }   

}
static int lcdc_ili9325c_panel_on(struct platform_device *pdev)
{
    #ifdef LCD_ILI9325C_REAL_TIME_DEGUG
    boolean para_debug_flag = FALSE;
    uint32 para_num = 0;

    para_debug_flag = lcd_debug_malloc_get_para( "lcdc_ili9325c_hvga_init_table", 
    (void**)&lcdc_ili9325c_hvga_init_table,&para_num);
    #endif
	if (!lcd_ili9325c_state.disp_initialized) 
    {
		/* Configure reset GPIO that drives DAC */
		lcdc_ili9325c_pdata->panel_config_gpio(1);
		lcd_reset_gpio = *(lcdc_ili9325c_pdata->gpio_num + 4);
        
		lcd_spi_init(lcdc_ili9325c_pdata);	/* LCD needs SPI */
		lcd_ili9325c_disp_powerup();
		lcd_ili9325c_state.display_on = TRUE;
		lcd_ili9325c_state.disp_initialized = TRUE;

	}
    else if(!lcd_ili9325c_state.display_on)
    {
        lcd_ili9325c_disp_exit_sleep();
    }
    
    #ifdef LCD_ILI9325C_REAL_TIME_DEGUG
    if((TRUE == para_debug_flag)&&(NULL != lcdc_ili9325c_hvga_init_table))
    {
		process_lcdc_table(lcdc_ili9325c_hvga_init_table, para_num);
		printk("lcdc_ili9325c_hvga_init_table\n");
		lcd_debug_free_para((void *)lcdc_ili9325c_hvga_init_table);
    }
    #endif
	return 0;
}

static int lcdc_ili9325c_panel_off(struct platform_device *pdev)
{
	unsigned char i = 0;

	if (lcd_ili9325c_state.disp_powered_up && lcd_ili9325c_state.display_on)
    {
        switch (lcd_panel_qvga) 
        {
              
            case LCD_ILI9325C_WINTEK_QVGA:
                for(i = 0; i < NUM_ILI9325C_WINTEK_DISP_OFF; i++)
                {
                    seriout(ili9325c_wintek_disp_off[i][0], ili9325c_wintek_disp_off[i][1]);
                    mdelay(ili9325c_wintek_disp_off[i][2]);
                }
                break;

            default :
                break;
              
        }
		lcd_ili9325c_state.display_on = FALSE;
	}
	return 0;
}

static void lcdc_ili9325c_panel_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
       
   // lcd_set_backlight_pwm(bl_level);
    pwm_set_backlight(bl_level);
    
    return;
}

static void lcdc_ili9325c_panel_set_contrast(struct msm_fb_data_type *mfd, unsigned int contrast)
{

    return;
}

static int __init lcdc_ili9325c_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		lcdc_ili9325c_pdata = pdev->dev.platform_data;
		return 0;
	}
	msm_fb_add_device(pdev);
	return 0;
}

static struct platform_driver this_driver = 
{
	.probe  = lcdc_ili9325c_probe,
	.driver = {
		.name   = "lcd_ili9325c_qvga",
	},
};

static struct msm_fb_panel_data ili9325c_panel_data =
{
	.on = lcdc_ili9325c_panel_on,
	.off = lcdc_ili9325c_panel_off,
	.set_backlight = lcdc_ili9325c_panel_set_backlight,
	.set_contrast = lcdc_ili9325c_panel_set_contrast,
};

static struct platform_device this_device = 
{
	.name   = "lcd_ili9325c_qvga",
	.id	= 1,
	.dev	=
	{
		.platform_data = &ili9325c_panel_data,
	}
};

static int __init lcdc_ili9325c_panel_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;


    lcd_panel_qvga = lcd_panel_probe();
    if((LCD_ILI9325C_WINTEK_QVGA != lcd_panel_qvga) &&  \
       (msm_fb_detect_client("lcd_ili9325c_qvga"))
      )
    {
        return 0;
    }

	ret = platform_driver_register(&this_driver);
	if (ret)
		return ret;

	pinfo = &ili9325c_panel_data.panel_info;
    pinfo->xres = 240;
	pinfo->yres = 320;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
    /*the pixel clk is different for different Resolution LCD*/
	//pinfo->clk_rate = 24500000; /*for VGA pixel clk*/
    pinfo->clk_rate = 6125000;  /*for QVGA pixel clk*/   
    pinfo->lcdc.h_back_porch = 3;
	pinfo->lcdc.h_front_porch = 5;
	pinfo->lcdc.h_pulse_width = 5;    
	pinfo->lcdc.v_back_porch = 3;
	pinfo->lcdc.v_front_porch = 3;
	pinfo->lcdc.v_pulse_width = 3;
	pinfo->lcdc.border_clr = 0;     /* blk */
	pinfo->lcdc.underflow_clr = 0xff;       /* blue */
	pinfo->lcdc.hsync_skew = 0;
    pinfo->bl_max = 255;

	ret = platform_device_register(&this_device);
	if (ret)
		platform_driver_unregister(&this_driver);

	return ret;
}


module_init(lcdc_ili9325c_panel_init);
