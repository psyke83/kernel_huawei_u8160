/* kernel\drivers\video\msm\lcd_hw_debug.h
 * this file is used by the driver team to change the 
 * LCD init parameters by putting a config file in the mobile,
 * this function can make the LCD parameter debug easier.
 * 
 * Copyright (C) 2010 HUAWEI Technology Co., ltd.
 * 
 * Date: 2010/12/10
 * By genghua
 * 
 */

#ifndef __HW_LCD_DEBUG__
#define __HW_LCD_DEBUG__

#include <linux/syscalls.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define HW_LCD_INIT_TEST_PARAM "/data/hw_lcd_init_param.txt"
#define HW_LCD_CONFIG_TABLE_MAX_NUM 600
#define HW_LCD_CONFIGLINE_MAX 100

bool lcd_debug_malloc_get_para(char *para_name, void **para_table,uint32_t *para_num);
bool lcd_debug_free_para(void *para_table);

#endif 

