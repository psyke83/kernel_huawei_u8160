/*add tianma ili9331b lcd driver*/
#include <linux/delay.h>
#include <mach/gpio.h>
#include "msm_fb.h"
#include "lcdc_huawei_config.h"

struct lcd_ili9331b_state_type{
	boolean disp_initialized;
	boolean display_on;
	boolean disp_powered_up;
};

extern void pwm_set_backlight(int level);

static int lcd_reset_gpio;
static struct lcd_ili9331b_state_type lcd_ili9331b_state = { 0 };
static struct msm_panel_common_pdata *lcd_ili9331b_pdata;
static lcd_panel_type lcd_panel_qvga = LCD_NONE;

static uint16 ili9331b_disp_off[][3] = 
{
    {0x0007, 0x0131, 10}, // Set D1=0, D0=1
    {0x0007, 0x0130, 10}, // Set D1=0, D0=0
    {0x0007, 0x0000, 0}, // display OFF
    //************* Power OFF sequence **************//
    {0x0010, 0x0080, 0}, // SAP, BT[3:0], APE, AP, DSTB, SLP
    {0x0011, 0x0000, 0}, // DC1[2:0], DC0[2:0], VC[2:0]
    {0x0012, 0x0000, 0}, // VREG1OUT voltage
    {0x0013, 0x0000, 200}, // VDV[4:0] for VCOM amplitude
    {0x0010, 0x0082, 0}, // SAP, BT[3:0], APE, AP, STB, SLP
};

static uint16 ili9331b_disp_on[][3] = 
{
//*************Power On sequence ******************//
    {0x0010, 0x0080, 0}, // SAP, BT[3:0], AP, DSTB, SLP
    {0x0011, 0x0000, 0}, // DC1[2:0], DC0[2:0], VC[2:0]
    {0x0012, 0x0000, 0}, // VREG1OUT voltage
    {0x0013, 0x0000, 200}, // VDV[4:0] for VCOM amplitude
    {0x0010, 0x1690, 0}, // SAP, BT[3:0], AP, DSTB, SLP, STB
    {0x0011, 0x0227, 50}, // DC1[2:0], DC0[2:0], VC[2:0]
    {0x0012, 0x008F, 50}, //Inernal reference voltage =Vci;
    {0x0013, 0x1200, 0}, // VDV[4:0] for VCOM amplitude
    {0x0029, 0x0029, 50}, // VCM[5:0] for VCOMH
    {0x0007, 0x0133, 0}, // 262K color and display ON
};

#undef ARRAY_SIZE
#define ARRAY_SIZE(a)  (sizeof(a) / (sizeof(uint16) * 3))

#define SIZE_ILI9331B_DISP_ON   ARRAY_SIZE(ili9331b_disp_on)
#define SIZE_ILI9331B_DISP_OFF  ARRAY_SIZE(ili9331b_disp_off)

#define DEVIDE_ID   0x70
#define WRITE_REG   0x00
#define WRITE_DATA  0x02

static void seriout(uint16 reg, uint16 data)
{
    uint8 start_byte_reg = DEVIDE_ID | WRITE_REG;
    uint8 start_byte_data = DEVIDE_ID | WRITE_DATA;
    
    seriout_cmd(reg, start_byte_reg);
    seriout_data(data, start_byte_data);
}
void lcdc_ili9331b_set_backlight(uint8 level)
{  
    seriout(0x00B1, (uint16)level);
    seriout(0x00B5, 0x0000);
    seriout(0x00B3, 0x0024);
    seriout(0x00BE, 0x0000);
}
static void lcd_ili9331b_disp_powerup(void)
{
	if (!lcd_ili9331b_state.disp_powered_up && !lcd_ili9331b_state.display_on) 
    {
		/* Reset the hardware first */
		/* Include DAC power up implementation here */
	    lcd_ili9331b_state.disp_powered_up = TRUE;
	}
}
static void lcd_ili9331b_disp_exit_sleep(void)
{
    unsigned char i = 0;
    if (lcd_ili9331b_state.disp_powered_up && !lcd_ili9331b_state.display_on)
    {
        for(; i < SIZE_ILI9331B_DISP_ON; ++i)
        {
            seriout(ili9331b_disp_on[i][0], ili9331b_disp_on[i][1]);
            mdelay(ili9331b_disp_on[i][2]);
        }
        lcdc_ili9331b_set_backlight(0xFF);
        lcd_ili9331b_state.display_on = TRUE;
    }
}
static int lcd_ili9331b_panel_on(struct platform_device *pdev)
{

	if (!lcd_ili9331b_state.disp_initialized) 
    {
		/* Configure reset GPIO that drives DAC */
		lcd_ili9331b_pdata->panel_config_gpio(1);
		lcd_reset_gpio = *(lcd_ili9331b_pdata->gpio_num + 4);
        
		lcd_spi_init(lcd_ili9331b_pdata);	/* LCD needs SPI */
		lcd_ili9331b_disp_powerup();
		lcd_ili9331b_state.display_on = TRUE;
		lcd_ili9331b_state.disp_initialized = TRUE;
	}
    else if(!lcd_ili9331b_state.display_on)
    {
        lcd_ili9331b_disp_exit_sleep();
    }
	return 0;
}
static int lcd_ili9331b_panel_off(struct platform_device *pdev)
{
    unsigned char i = 0;
    if (lcd_ili9331b_state.disp_powered_up && lcd_ili9331b_state.display_on)
    {
        for(; i < SIZE_ILI9331B_DISP_OFF; ++i)
        {
            seriout(ili9331b_disp_off[i][0], ili9331b_disp_off[i][1]);
            mdelay(ili9331b_disp_off[i][2]);
        }
        lcd_ili9331b_state.display_on = FALSE;
    }
    return 0;
}
static void lcdc_ili9331b_panel_set_contrast(struct msm_fb_data_type *mfd, unsigned int contrast)
{
    return;
}
static void lcd_ili9331b_panel_set_backlight(struct msm_fb_data_type *mfd)
{
    int bl_level = mfd->bl_level;
    pwm_set_backlight(bl_level);    
    return;
}
static int __init lcd_ili9331b_probe(struct platform_device *pdev)
{
	if (0 == pdev->id) {
		lcd_ili9331b_pdata = pdev->dev.platform_data;
		return 0;
	}
	msm_fb_add_device(pdev);
	return 0;
}
static struct platform_driver this_driver = 
{
    .probe = lcd_ili9331b_probe,
    .driver = {
        .name = "lcd_ili9331b_qvga",
    },
};
static struct msm_fb_panel_data ili9331b_panel_data = {
    .on = lcd_ili9331b_panel_on,
    .off = lcd_ili9331b_panel_off,
    .set_backlight = lcd_ili9331b_panel_set_backlight,
    .set_contrast = lcdc_ili9331b_panel_set_contrast,
};
static struct platform_device this_device = {
    .name   = "lcd_ili9331b_qvga",
    .id	= 1,
    .dev	= {
    	.platform_data = &ili9331b_panel_data,
    },
};
static int __init lcd_ili9331b_panel_init(void)
{
    int ret;
    struct msm_panel_info *pinfo;

    lcd_panel_qvga = lcd_panel_probe();
    if(LCD_ILI9331B_TIANMA_QVGA != lcd_panel_qvga)
    {
        return 0;
    }
    
    ret = platform_driver_register(&this_driver);
    if (ret)
        return ret;

    pinfo = &ili9331b_panel_data.panel_info;
    pinfo->xres = 240;
	pinfo->yres = 320;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 18;
	pinfo->fb_num = 2;
    pinfo->clk_rate = 6125000;  /*for QVGA pixel clk*/   
    pinfo->lcdc.h_back_porch = 7;
	pinfo->lcdc.h_front_porch = 4;
	pinfo->lcdc.h_pulse_width = 4;    
	pinfo->lcdc.v_back_porch = 1;
	pinfo->lcdc.v_front_porch = 2;
	pinfo->lcdc.v_pulse_width = 1;
	pinfo->lcdc.border_clr = 0;     /* blk */
	pinfo->lcdc.underflow_clr = 0xff;       /* blue */
	pinfo->lcdc.hsync_skew = 0;
    pinfo->bl_max = 255;

    ret = platform_device_register(&this_device);
    if (ret)
        platform_driver_unregister(&this_driver);

    return ret;
}

module_init(lcd_ili9331b_panel_init);
