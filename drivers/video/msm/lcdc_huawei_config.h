/* Copyright (c), Code HUAWEI. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef LCDC_HUAWEI_CONFIG_H
#include "msm_fb.h"
#include <linux/hardware_self_adapt.h>

#define LCDC_HUAWEI_CONFIG_H

#define GP_MD_REG_ADDR_OFFSET              0x0058
#define GP_NS_REG_ADDR_OFFSET              0x005C
#define MSM_GP_MD_REG_VIRT_ADD            (MSM_CLK_CTL_BASE + GP_MD_REG_ADDR_OFFSET)
#define MSM_GP_NS_REG_VIRT_ADD            (MSM_CLK_CTL_BASE + GP_NS_REG_ADDR_OFFSET)

// Êä³öÆµÂÊÎª22.05KHZ
#define PWM_LCD_NOT_N_M_VAL                0xFE4D
#define PWM_LCD_M_VAL                      0x0001

#define GP_ROOT_ENA                        (1 << 11)
#define GP_CLK_BRANCH_ENA                  (1 << 9)
#define GP_MNCNTR_EN                       (1 << 8)
#define GP_NS_REG_SRC_SEL                  (0 << 0)
#define GP_NS_REG_PRE_DIV_SEL              (1 << 3)
#define GP_NS_REG_MNCNTR_MODE              (3 << 5)
#define GP_NS_REG_GP_N_VAL                 (PWM_LCD_NOT_N_M_VAL << 16)
#define GP_MD_REG_M_VAL                    (PWM_LCD_M_VAL << 16)

/*delete*/

void lcd_spi_init(struct msm_panel_common_pdata *lcdc_pnael_data);
void seriout_transfer_byte(uint8 reg, uint8 start_byte);
void seriout_cmd(uint16 reg, uint8 start_byte);
void seriout_data(uint16 data, uint8 start_byte);
void lcd_set_backlight_pwm(int level);
void seriout_byte_9bit(uint8 start_byte, uint8 data);
uint8 seri_read_byte(uint8 start_byte);

#endif
