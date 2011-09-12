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

#include <mach/msm_iomap.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <asm/io.h>
#include "msm_fb.h"
#include "lcdc_huawei_config.h"
#include <linux/hardware_self_adapt.h>
#include <mach/pmic.h>

/* delete DISP_BACKLIGHT_STEP */

#define GPIO_OUT_27                    27
#define SPI_CLK_DELAY                  1
#define SPI_CLK_PULSE_INTERVAL         5
typedef enum
{
    GPIO_LOW_VALUE  = 0,
    GPIO_HIGH_VALUE = 1
} gpio_value_type;

//extern __s16 hw_get_lcd_panel_id(void);
static int spi_cs;
static int spi_sclk;
static int spi_sdo;
static int spi_sdi;

static void seriout_byte(uint8 data)
{
    unsigned char i = 0;
    uint8 byte_mask = 0x80;
    uint8 tx_byte = data;

    for(i = 0; i < 8; i++)
    {
		gpio_set_value(spi_sclk, 0);
        udelay(SPI_CLK_DELAY);
        if(tx_byte & byte_mask)
        {
			gpio_set_value(spi_sdo, 1);
        }
        else
        {
			gpio_set_value(spi_sdo, 0);
        }
        udelay(SPI_CLK_DELAY);
		gpio_set_value(spi_sclk, 1);
        tx_byte = tx_byte << 1;
        udelay(SPI_CLK_DELAY);

    }
}

static void seriout_word(uint16 wdata)
{
    uint8 high_byte = ((wdata & 0xFF00) >> 8);
    uint8 low_byte = (wdata & 0x00FF);
 
    seriout_byte(high_byte);
    seriout_byte(low_byte);
}

void seriout_byte_9bit(uint8 start_byte, uint8 data)
{
    unsigned char i = 0;
    uint8 byte_mask = 0x80;
    uint8 tx_byte = data;

    /* Enable the Chip Select */
    gpio_set_value(spi_cs, GPIO_HIGH_VALUE);
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_sclk, GPIO_HIGH_VALUE);
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_cs, GPIO_LOW_VALUE);
    udelay(SPI_CLK_PULSE_INTERVAL);

    /*send the start bit before send the data*/
    gpio_set_value(spi_sclk, GPIO_LOW_VALUE);
    udelay(SPI_CLK_DELAY);

    if (start_byte & 0x1)
    {
        gpio_set_value(spi_sdo, GPIO_HIGH_VALUE);
    }
    else 
    {
        gpio_set_value(spi_sdo, GPIO_LOW_VALUE);
    }

    udelay(SPI_CLK_DELAY);
    gpio_set_value(spi_sclk, GPIO_HIGH_VALUE);
    udelay(SPI_CLK_DELAY);

    /*send data*/
    for(i = 0; i < 8; i++)
    {
        gpio_set_value(spi_sclk, GPIO_LOW_VALUE);
        udelay(SPI_CLK_DELAY);
        if(tx_byte & byte_mask)
        {
            gpio_set_value(spi_sdo, GPIO_HIGH_VALUE);
        }
        else
        {
            gpio_set_value(spi_sdo, GPIO_LOW_VALUE);
        }
        udelay(SPI_CLK_DELAY);
        gpio_set_value(spi_sclk, GPIO_HIGH_VALUE);
        tx_byte <<= 1;
        udelay(SPI_CLK_DELAY);

    }
    /*disable the chip*/
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_cs, GPIO_HIGH_VALUE);
}


void seriout_transfer_byte(uint8 reg, uint8 start_byte)
{    
    /* Enable the Chip Select */
    gpio_set_value(spi_cs, 1);
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_sclk, 1);
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_cs, 0);
    udelay(SPI_CLK_PULSE_INTERVAL);

    /*transfer cmd start byte*/
    seriout_byte(start_byte);
    /*transfer cmd*/
    seriout_byte(reg);
    
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_cs, 1);
}

static uint8 seriin_byte(void)
{
    unsigned char i = 0;
    uint8 data = 0;

    /*config input to receive data*/
    gpio_tlmm_config(GPIO_CFG(spi_sdo, 0, GPIO_INPUT, GPIO_PULL_UP, GPIO_2MA),GPIO_ENABLE);

    for(i = 0; i < 8; i++)
    {
        gpio_set_value(spi_sclk, 0);
        udelay(SPI_CLK_DELAY);
        data = data << 1;
        if(gpio_get_value(spi_sdo))
        {
            data |= 0x01;  /*get HIGH*/
        }
        else
        {
            data |= 0x00;  /*get LOW*/
        }
        udelay(SPI_CLK_DELAY);
        gpio_set_value(spi_sclk, 1);
        udelay(SPI_CLK_DELAY);
    }

    gpio_tlmm_config(GPIO_CFG(spi_sdo, 0, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),GPIO_ENABLE);
    
    return data;
}

uint8 seri_read_byte(uint8 start_byte)
{   
    uint8 data = 0;
    
    /* Enable the Chip Select */
    gpio_set_value(spi_cs, 1);
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_sclk, 1);
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_cs, 0);
    udelay(SPI_CLK_PULSE_INTERVAL);

    /*transfer cmd start byte*/
    seriout_byte(start_byte);

    /*read register data*/
    data = seriin_byte();
    
    udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_cs, 1);

    return data;
}

void seriout_cmd(uint16 reg, uint8 start_byte)
{    
	/* Enable the Chip Select */
	gpio_set_value(spi_cs, 1);
	udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_sclk, 1);
	udelay(SPI_CLK_PULSE_INTERVAL);
	gpio_set_value(spi_cs, 0);
	udelay(SPI_CLK_PULSE_INTERVAL);

    /*transfer cmd start byte*/
    seriout_byte(start_byte);
    /*transfer cmd*/
    seriout_word(reg);
    
    udelay(SPI_CLK_PULSE_INTERVAL);
	gpio_set_value(spi_cs, 1);

}

void seriout_data(uint16 data, uint8 start_byte)
{    
	/* Enable the Chip Select */
	gpio_set_value(spi_cs, 1);
	udelay(SPI_CLK_PULSE_INTERVAL);
    gpio_set_value(spi_sclk, 1);
	udelay(SPI_CLK_PULSE_INTERVAL);
	gpio_set_value(spi_cs, 0);
	udelay(SPI_CLK_PULSE_INTERVAL);

    /*transfer data start byte*/
    seriout_byte(start_byte);
    /*transfer data*/
    seriout_word(data);  
    
    udelay(SPI_CLK_PULSE_INTERVAL);
	gpio_set_value(spi_cs, 1);
}

void lcd_spi_init(struct msm_panel_common_pdata * lcdc_pnael_data)
{
	/* Setting the Default GPIO's */
	spi_sclk = *(lcdc_pnael_data->gpio_num);
	spi_cs   = *(lcdc_pnael_data->gpio_num + 1);
	spi_sdi  = *(lcdc_pnael_data->gpio_num + 2);
	spi_sdo  = *(lcdc_pnael_data->gpio_num + 3);

	/* Set the output so that we dont disturb the slave device */
	gpio_set_value(spi_sclk, 0);
	gpio_set_value(spi_sdo, 0);

	/* Set the Chip Select De-asserted */
	gpio_set_value(spi_cs, 1);

}

#define TOUCH_EXTA_KEY_BACKLIGHT_STEP    (32)
#define TOUCH_EXTA_KEY_BACKLIGHT_MAX     (32*PM_MPP__I_SINK__LEVEL_40mA)

void set_touch_exta_key_backlight(int level)
{
    int ret = 0;
    bool use_touch_key_light = false;
    if(machine_is_msm7x25_u8150() || machine_is_msm7x25_c8150() \ 
      || machine_is_msm7x25_u8160() || machine_is_msm7x25_u8130() \ 
      || machine_is_msm7x25_c8500() || machine_is_msm7x25_u8159() \
	  || machine_is_msm7x25_c8510() )
    {
        ret = pmic_set_led_intensity(LED_LCD, (int)(!!level));
    }
    else
    {
        if(!board_use_tssc_touch(&use_touch_key_light))
        {
            if(use_touch_key_light)
            {
                if(level > TOUCH_EXTA_KEY_BACKLIGHT_MAX)
                {
                    level = TOUCH_EXTA_KEY_BACKLIGHT_MAX;
                }

                /*all product use 5mA I_SINK*/
                ret = pmic_secure_mpp_config_i_sink(PM_MPP_7,  \
                             PM_MPP__I_SINK__LEVEL_5mA, \
                             (!!level) ? PM_MPP__I_SINK__SWITCH_ENA : PM_MPP__I_SINK__SWITCH_DIS);
            }
        }
    }

    if(ret)
    {
        printk(KERN_ERR "can't set touch exta_key backlight\n");
    }
}

static int display_backlight_step(int level)
{
	if (machine_is_msm7x25_u8500() || machine_is_msm7x25_um840()) {
		if ((level >= 0)&&(level < 200)) {
			return ( (level)*3 + 18 );
		} else if ((level >= 200)&&(level <= 255)) {
			return ( (level)*2 + 218 );		/* lower backlight level */
		} else {
			printk(KERN_ERR "backlight level error.\n");
			return -1;
		}
	} else {
		return ( (level)*3 + 18 );
	}
}

void lcd_set_backlight_pwm(int level)
{
    static uint8 last_level = 0;
    uint16 duty_cycle = 0xFFFF;
    uint32 gp_reg_value = 0; 
    unsigned long flags;

    if (last_level == level)
    {
        return ;
    }
    last_level = level;

    /*used by product u8110*/
	if ( !(machine_is_msm7x25_u8150() || machine_is_msm7x25_c8150() \
      || machine_is_msm7x25_u8160() || machine_is_msm7x25_u8130() \ 
      || machine_is_msm7x25_c8510() || machine_is_msm7x25_u8159()) )
	{
		set_touch_exta_key_backlight(level);
	}
    
    /*config gpio 27 as gp_clk */
    gpio_tlmm_config(GPIO_CFG(GPIO_OUT_27, 1, GPIO_OUTPUT, GPIO_NO_PULL, GPIO_2MA),GPIO_ENABLE);

    if(0 == level)
    {
        duty_cycle= 0xFFFF;
        writel(0x0, MSM_GP_NS_REG_VIRT_ADD);
        return;
    }
    else
    {
        if(display_backlight_step(level) > 0)
        {
            duty_cycle = 0xFFFF - display_backlight_step(level);
        }
        else
        {
            duty_cycle = 0xFFFE;   /*min clk cycle*/
        }
    }
    
    /* To reconfig the M/N registers. It's need to disable GP_CLK and delay. */
    /*disable GP CLK and M/N conuter*/
/* solve backlight flare,use lock function to save irq*/
    if (machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
    {
        local_irq_save(flags);
    }
    gp_reg_value = readl(MSM_GP_NS_REG_VIRT_ADD);
    writel(gp_reg_value & (~(GP_ROOT_ENA | GP_CLK_BRANCH_ENA |GP_MNCNTR_EN)), MSM_GP_NS_REG_VIRT_ADD);
    udelay(50);

    /*config GP_NS_REG*/
    gp_reg_value = readl(MSM_GP_NS_REG_VIRT_ADD);
    writel(gp_reg_value | (GP_NS_REG_SRC_SEL | GP_NS_REG_PRE_DIV_SEL | GP_NS_REG_MNCNTR_MODE | GP_NS_REG_GP_N_VAL), \
           MSM_GP_NS_REG_VIRT_ADD);

    /*config GP_MD_REG*/
    writel(GP_MD_REG_M_VAL | duty_cycle, MSM_GP_MD_REG_VIRT_ADD);

    /*enabale GP CLK and M/N conuter*/
    gp_reg_value = readl(MSM_GP_NS_REG_VIRT_ADD);
    writel(gp_reg_value | (GP_ROOT_ENA | GP_CLK_BRANCH_ENA |GP_MNCNTR_EN), MSM_GP_NS_REG_VIRT_ADD);
    if (machine_is_msm7x25_u8500() || machine_is_msm7x25_um840())
    {
        local_irq_restore(flags);
    }
}
/*delete one function*/

