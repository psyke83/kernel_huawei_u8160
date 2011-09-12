/* drivers/video/msm/src/panel/ebi2/ebi2_tmd20.c
 *
 * 
 *   creat by zhangliping for kgm803a0 ebi2 lcd driver.
 */

#include "msm_fb.h"

#include <linux/memory.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include "linux/proc_fs.h"

#include <linux/delay.h>

#include <mach/hardware.h>
#include <asm/io.h>

#include <asm/system.h>
#include <asm/mach-types.h>

#include <linux/gpio.h>

#define QVGA_WIDTH        240
#define QVGA_HEIGHT       320

#define INPORT(x)   inpdw(x)
#define OUTPORT(x,y)  outpdw(x,y)

//#define DISP_QVGA_18BPP(x)  ((( (x)<<2) & 0x3FC00)|(( (x)<<1)& 0x1FE))
#define DISP_QVGA_16_TO_18BPP(x)  ((((x) & 0x3ff00)<<2)     |(((x) & 0xff)<<1))

#define DISP_CMD_OUT(cmd) OUTPORT(DISP_CMD_PORT, DISP_QVGA_16_TO_18BPP(cmd))
#define DISP_DATA_OUT(data) OUTPORT(DISP_DATA_PORT, data)
#define DISP_DATA_IN() INPORT(DISP_DATA_PORT)



#define DISP_WRITE_OUT(addr, data) \ 
   DISP_CMD_OUT(addr); \
   DISP_DATA_OUT(DISP_QVGA_16_TO_18BPP(data));
/*
static void DISP_WRITE_OUT()
{
     outpdw();
}
*/

#define DISP_SET_RECT(ulhc_row, lrhc_row, ulhc_col, lrhc_col ) \
{\
	  DISP_WRITE_OUT(0x0052,ulhc_row) \
	  DISP_WRITE_OUT(0x0053,lrhc_row) \
	  DISP_WRITE_OUT(0x0050,ulhc_col) \
	  DISP_WRITE_OUT(0x0051,lrhc_col)  \
	  DISP_WRITE_OUT(0x0003,0x1030)  \	  
	  DISP_WRITE_OUT(0x0020,ulhc_col)  \
	 DISP_WRITE_OUT(0x0021,ulhc_row)  \
}

#define WAIT_SEC(sec) mdelay((sec)/1000)
#define WAIT_USEC(sec)    udelay(sec)


static void *DISP_CMD_PORT = NULL;
static void *DISP_DATA_PORT = NULL;

static boolean display_on = FALSE;
//static panel_info_type panel_info;
static boolean disp_initialized = FALSE;


static void kgm803a0_disp_set_rect(int x, int y, int xres, int yres);
static int kgm803a0_disp_off(struct platform_device *pdev);
static int kgm803a0_disp_on(struct platform_device *pdev);

static int kgm803a0_set_backlight(boolean on)
{
     int err;
     err = gpio_request(94, "gpio_backlight");
     if (err) 
     	{
		printk(KERN_ERR "gpio_request failed for 94\n");
		return -1;
	}
     
	if(on)
	{
	     err = gpio_configure(94, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
	     if (err) 
	     	{
			printk(KERN_ERR "gpio_config failed for 94 HIGH\n");
		       return -1;
		}
      }
      else
      {
    	     err = gpio_configure(94, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
	     if (err) 
	     	{
			printk(KERN_ERR "gpio_config failed for 94 low\n");
		       return -1;
		}
      }
}
static int kgm803a0_panel_reset(void)
{
     int err;
     err = gpio_request(102, "gpio_panel_reset");
     if (err) 
     	{
		printk(KERN_ERR "gpio_request failed for 102\n");
		return -1;
	}
     
     err = gpio_configure(102, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
     if (err) 
     	{
		printk(KERN_ERR "gpio_config failed for 102 HIGH\n");
	       return -1;
	}
     WAIT_USEC(5);
     err = gpio_configure(102, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_LOW);
     if (err) 
     	{
		printk(KERN_ERR "gpio_config failed for 102 LOW\n");
	       return -1;
	}
       WAIT_SEC(10000);
       err = gpio_configure(102, GPIOF_DRIVE_OUTPUT | GPIOF_OUTPUT_HIGH);
     if (err) 
     	{
		printk(KERN_ERR "gpio_config failed for 102\n");
	       return -1;
	}
        WAIT_SEC(10000);
}
static int kgm803a0_disp_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	if (disp_initialized)
		return 0;

	kgm803a0_panel_reset();/*lcd panel reset*/
	kgm803a0_set_backlight(TRUE);/*´ò¿ª±³¹â*/

	mfd = platform_get_drvdata(pdev);

	DISP_CMD_PORT = mfd->cmd_port;
	DISP_DATA_PORT = mfd->data_port;
	
      DISP_WRITE_OUT(0x0000,0x0000);
      DISP_WRITE_OUT(0x0001,0x0100);
      DISP_WRITE_OUT(0x0002,0x0700);
      DISP_WRITE_OUT(0x0003,0x10B0);
      DISP_WRITE_OUT(0x0004,0x0000);
      DISP_WRITE_OUT(0x0008,0x0204);
      DISP_WRITE_OUT(0x0009,0x0000);
      DISP_WRITE_OUT(0x000A,0x0000);
      DISP_WRITE_OUT(0x000C,0x0000);
      DISP_WRITE_OUT(0x000D,0x0000);
      DISP_WRITE_OUT(0x000F,0x0002);
      /********GAMMA CONTROL*******/
     DISP_WRITE_OUT(0x0030,0x0707);
     DISP_WRITE_OUT(0x0031,0x1421);
     DISP_WRITE_OUT(0x0032,0x1A24);
     DISP_WRITE_OUT(0x0033,0x241A);
     DISP_WRITE_OUT(0x0034,0x2114);
     DISP_WRITE_OUT(0x0035,0x0707);
     DISP_WRITE_OUT(0x0036,0x1504);
     DISP_WRITE_OUT(0x0037,0x0515);
     DISP_WRITE_OUT(0x0038,0x0706);
     DISP_WRITE_OUT(0x0039,0x0304);
     DISP_WRITE_OUT(0x003A,0x0F04);
     DISP_WRITE_OUT(0x003B,0x0F00);
     DISP_WRITE_OUT(0x003C,0x000F);
     DISP_WRITE_OUT(0x003D,0x040F);
     DISP_WRITE_OUT(0x003E,0x0403);
     DISP_WRITE_OUT(0x003F,0x0607);
      /*******RAM ADDR CONTROL******/
     DISP_WRITE_OUT(0x0050,0x0000);
     DISP_WRITE_OUT(0x0051,0x00EF);
     DISP_WRITE_OUT(0x0052,0x0000);
     DISP_WRITE_OUT(0x0053,0x013F);
      /*****PANEL IMAGE CONTROL*****/
      DISP_WRITE_OUT(0x0060,0xA700);
      DISP_WRITE_OUT(0x0061,0x0001);
      DISP_WRITE_OUT(0x006A,0x0000);
      /*****PANEL IMAGE CONTROL*****/
      DISP_WRITE_OUT(0x0080,0x0000);
      DISP_WRITE_OUT(0x0081,0x0000);
      DISP_WRITE_OUT(0x0082,0x0000);
      DISP_WRITE_OUT(0x0083,0x0000);
      DISP_WRITE_OUT(0x0084,0x0000);
      DISP_WRITE_OUT(0x0085,0x0000);
      /******panel interface control*****/
      DISP_WRITE_OUT(0x0090,0x0018);
      DISP_WRITE_OUT(0x0092,0x0000);
      DISP_WRITE_OUT(0x0093,0x0103);
      DISP_WRITE_OUT(0x0095,0x0110);
      DISP_WRITE_OUT(0x0097,0x0000);
      DISP_WRITE_OUT(0x0098,0x0000);
      /*****orise mode*****/
      DISP_WRITE_OUT(0x00F0,0x5408);
      DISP_WRITE_OUT(0x00E0,0x0006);
      DISP_WRITE_OUT(0x00F2,0x00DF);
      DISP_WRITE_OUT(0x00F3,0x6D06);
      DISP_WRITE_OUT(0x00F4,0x0011);
      DISP_WRITE_OUT(0x00F0,0x0000);
      /*****POWER ON SEQUENCE*****/
      DISP_WRITE_OUT(0x0011,0x0007);
      WAIT_SEC(5000);
      DISP_WRITE_OUT(0x0010,0x12B0);
      WAIT_SEC(5000);
      DISP_WRITE_OUT(0x0012,0x01BD);
      /*****Vcom setting*****/
      DISP_WRITE_OUT(0x0013,0x1200);
      DISP_WRITE_OUT(0x0029,0x000c);
      DISP_WRITE_OUT(0x0007,0x0112);


	  disp_initialized = TRUE;
	
}

static void kgm803a0_disp_set_rect(int x, int y, int xres, int yres)
{
	if (!disp_initialized)
		return;
    if((y + yres  <= QVGA_HEIGHT) &&( yres >= 0x04) && (x + xres <= QVGA_WIDTH))
    {
      	     DISP_SET_RECT(y, y + yres - 1, x, x + xres - 1);
    }
    DISP_CMD_OUT(0x0022);      
}




static int kgm803a0_disp_off(struct platform_device *pdev)
{
	if (!disp_initialized)
		return -EFAULT;

	if (display_on) 
	{
		DISP_WRITE_OUT(0x0007,0x0000);
		WAIT_SEC(50000);
		DISP_WRITE_OUT(0x0010,0x0004);
		display_on = FALSE;
	}

	return 0;
}


static int __init tmd20qvga_probe(struct platform_device *pdev)
{
	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = tmd20qvga_probe,
	.driver = {
		.name   = "ebi2_kgm803a0_qvga",
	},
};

static struct msm_fb_panel_data kgm803a0_panel_data = {
	.on = kgm803a0_disp_on,
	.off = kgm803a0_disp_off,
	.set_rect = kgm803a0_disp_set_rect,
};

static struct platform_device this_device = {
	.name   = "ebi2_kgm803a0_qvga",
	.id	= 0,
	.dev	= {
		.platform_data = &kgm803a0_panel_data,
	}
};

static int __init kgm803a0_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	ret = platform_driver_register(&this_driver);
	if (!ret) {
		pinfo = &kgm803a0_panel_data.panel_info;
		pinfo->xres = QVGA_WIDTH;
		pinfo->yres = QVGA_HEIGHT;
		pinfo->type = EBI2_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->wait_cycle = 0x808000;
		pinfo->bpp = 18;

		pinfo->fb_num = 2;
		

		ret = platform_device_register(&this_device);
		if (ret)
			platform_driver_unregister(&this_driver);
	}

	return ret;
}

module_init(kgm803a0_init);

