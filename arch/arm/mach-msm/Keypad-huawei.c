/*
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/gpio_event.h>

#include <asm/mach-types.h>

/*                                          c8600 keypad begin                                                                 */
static unsigned int keypad_row_gpios_c8600[] = { 35, 34, 33 };//driver
static unsigned int keypad_col_gpios_c8600[] = { 41, 40, 39 };//sensor
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_c8600) + (col))
static const unsigned short keypad_keymap_c8600[ARRAY_SIZE(keypad_col_gpios_c8600) *
					      ARRAY_SIZE(keypad_row_gpios_c8600)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_SEND,   /* KEY_SEND 231 */ 
	[KEYMAP_INDEX(0, 1)] = 62,   /* ENDCALL 62 */ 
	[KEYMAP_INDEX(0, 2)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115 */ 
	[KEYMAP_INDEX(1, 0)] = KEY_MENU,  /* KEY_MENU  139  */ 
	#ifndef CONFIG_HUAWEI_BACK_KEY_MULTI
	[KEYMAP_INDEX(1, 1)] = 250,  /* KEY_BACKLIGHT 250 ,250 will be deleted in u8230 */
	#else
	[KEYMAP_INDEX(1, 1)] = KEY_BACK,
	#endif
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN 114 */ 
	[KEYMAP_INDEX(2, 0)] = 232,  /* DPAD_CENTER 232 */ 
	[KEYMAP_INDEX(2, 1)] = 249,  /* KEY_FOCUS  249   */ 
	[KEYMAP_INDEX(2, 2)] = KEY_CAMERA,  /* KEY_CAMERA 212 */
};

/*  keypad platform device information */
static struct gpio_event_matrix_info c8600_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_c8600,
	.output_gpios	= keypad_row_gpios_c8600,
	.input_gpios	= keypad_col_gpios_c8600,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_c8600),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_c8600),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};

static struct gpio_event_info *c8600_keypad_info[] = {
	&c8600_keypad_matrix_info.info
};

static struct gpio_event_platform_data c8600_keypad_data = {
	.name		= "surf_keypad",
	.info		= c8600_keypad_info,
	.info_count	= ARRAY_SIZE(c8600_keypad_info)
};

struct platform_device keypad_device_c8600 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &c8600_keypad_data,
	},
};
/*                                          c8600 keypad end                                                                 */

/*                                    u8100/u8110 keypad begin                                                                 */
static unsigned int keypad_row_gpios_u8100[] = { 35, 34, 33 };//driver
static unsigned int keypad_col_gpios_u8100[] = { 41, 40, 39, 38 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8100) + (col))
static const unsigned short keypad_keymap_u8100[ARRAY_SIZE(keypad_col_gpios_u8100) *
					      ARRAY_SIZE(keypad_row_gpios_u8100)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_SEND,   /* KEY_SEND 231 */ 
	[KEYMAP_INDEX(0, 1)] = KEY_DOWN,  /* KEY_DOWN 108   */ 
	[KEYMAP_INDEX(0, 2)] = 62,   /* ENDCALL 62 */ 
	[KEYMAP_INDEX(0, 3)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115  */ 

	[KEYMAP_INDEX(1, 0)] = KEY_LEFT,  /* KEY_LEFT 105  */ 
	[KEYMAP_INDEX(1, 1)] = 232,  /* DPAD_CENTER 232 */ 
	[KEYMAP_INDEX(1, 2)] = KEY_RIGHT,  /* KEY_RIGHT 106 */ 
	[KEYMAP_INDEX(1, 3)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114 */

	[KEYMAP_INDEX(2, 0)] = KEY_CAMERA,  /* KEY_CAMERA 212 */
	[KEYMAP_INDEX(2, 1)] = KEY_UP,  /* KEY_UP 103  */ 
	[KEYMAP_INDEX(2, 2)] = 0,  /* KEY_RESERVED */ 
	[KEYMAP_INDEX(2, 3)] = 0,  /* KEY_RESERVED */ 
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8100_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8100,
	.output_gpios	= keypad_row_gpios_u8100,
	.input_gpios	= keypad_col_gpios_u8100,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8100),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8100),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};

static struct gpio_event_info *u8100_keypad_info[] = {
	&u8100_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8100_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8100_keypad_info,
	.info_count	= ARRAY_SIZE(u8100_keypad_info)
};

struct platform_device keypad_device_u8100 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8100_keypad_data,
	},
};
/*                                     u8100/u8110 keypad end                                                                */

/*                                          u7610 keypad begin                                                                 */
static unsigned int keypad_row_gpios_u7610[] = { 35, 34, 33, 32 };//driver
static unsigned int keypad_col_gpios_u7610[] = { 41, 40 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u7610) + (col))
static const unsigned short keypad_keymap_u7610[ARRAY_SIZE(keypad_col_gpios_u7610) *
					      ARRAY_SIZE(keypad_row_gpios_u7610)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_SEND,   /* KEY_SEND 231 */  
	[KEYMAP_INDEX(0, 1)] = 62,   /* ENDCALL 62 */ 

	[KEYMAP_INDEX(1, 0)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115 */ 
	[KEYMAP_INDEX(1, 1)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114  */ 

	[KEYMAP_INDEX(2, 0)] = 249,  /* KEY_FOCUS ,249 */
	[KEYMAP_INDEX(2, 1)] = KEY_CAMERA,  /* KEY_CAMERA 212 */ 

	[KEYMAP_INDEX(3, 0)] = 0,  /*  KEY_RESERVED */ 
	[KEYMAP_INDEX(3, 1)] = 232,  /* DPAD_CENTER 232 */ 
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u7610_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u7610,
	.output_gpios	= keypad_row_gpios_u7610,
	.input_gpios	= keypad_col_gpios_u7610,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u7610),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u7610),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};

static struct gpio_event_info *u7610_keypad_info[] = {
	&u7610_keypad_matrix_info.info
};

static struct gpio_event_platform_data u7610_keypad_data = {
	.name		= "surf_keypad",
	.info		= u7610_keypad_info,
	.info_count	= ARRAY_SIZE(u7610_keypad_info)
};

struct platform_device keypad_device_u7610 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u7610_keypad_data,
	},
};
/*                                          u7610 keypad end                                                                 */

/*add code to support JOY(U8120) */
static unsigned int keypad_row_gpios_u8120[] = { 35, 34 };//driver
static unsigned int keypad_col_gpios_u8120[] = { 41, 39, 38 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8120) + (col))
static const unsigned short keypad_keymap_u8120[ARRAY_SIZE(keypad_col_gpios_u8120) *
					      ARRAY_SIZE(keypad_row_gpios_u8120)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_SEND,   /* KEY_SEND 231 */ 
	[KEYMAP_INDEX(0, 1)] = 62,         /* ENDCALL 62 */ 
	[KEYMAP_INDEX(0, 2)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114*/ 

	[KEYMAP_INDEX(1, 0)] = KEY_MENU,  /* KEY_MENU 139  */ 
	
	#ifndef CONFIG_HUAWEI_BACK_KEY_MULTI
	[KEYMAP_INDEX(1, 1)] = 250,  /* KEY_BACKLIGHT 250 ,250 will be deleted in u8230 */
	#else
	[KEYMAP_INDEX(1, 1)] = KEY_BACK,
	#endif
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115*/

};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8120_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8120,
	.output_gpios	= keypad_row_gpios_u8120,
	.input_gpios	= keypad_col_gpios_u8120,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8120),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8120),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};

static struct gpio_event_info *u8120_keypad_info[] = {
	&u8120_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8120_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8120_keypad_info,
	.info_count	= ARRAY_SIZE(u8120_keypad_info)
};

struct platform_device keypad_device_u8120 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8120_keypad_data,
	},
};

/*                                          u8300 keypad begin                                                                 */
static unsigned int keypad_row_gpios_u8300[] = { 35 };//driver
static unsigned int keypad_col_gpios_u8300[] = { 41, 40 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8300) + (col))
static const unsigned short keypad_keymap_u8300[ARRAY_SIZE(keypad_col_gpios_u8300) *
					      ARRAY_SIZE(keypad_row_gpios_u8300)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114  */
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115 */ 
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8300_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8300,
	.output_gpios	= keypad_row_gpios_u8300,
	.input_gpios	= keypad_col_gpios_u8300,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8300),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8300),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};

static struct gpio_event_info *u8300_keypad_info[] = {
	&u8300_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8300_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8300_keypad_info,
	.info_count	= ARRAY_SIZE(u8300_keypad_info)
};

struct platform_device keypad_device_u8300 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8300_keypad_data,
	},
};
/*                                          u8300 keypad end                                                                 */

/* U8350 keypad begin  */
static unsigned int keypad_row_gpios_u8350[] = { 35 };//driver
static unsigned int keypad_col_gpios_u8350[] = { 41, 40 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8350) + (col))
static const unsigned short keypad_keymap_u8350[ARRAY_SIZE(keypad_col_gpios_u8350) *
					      ARRAY_SIZE(keypad_row_gpios_u8350)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115 */ 
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114  */
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8350_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8350,
	.output_gpios	= keypad_row_gpios_u8350,
	.input_gpios	= keypad_col_gpios_u8350,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8350),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8350),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};
static struct gpio_event_info *u8350_keypad_info[] = {
	&u8350_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8350_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8350_keypad_info,
	.info_count	= ARRAY_SIZE(u8350_keypad_info)
};

struct platform_device keypad_device_u8350 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8350_keypad_data,
	},
};
/* u8350 keypad end */
/*U8150_EU and U8150_JP*/
/*------------------------------- U8150_EU/U8150_JP keypad begin ----------------------------------------*/
static unsigned int keypad_row_gpios_u8150[] = { 35, 34, 33 };//driver
static unsigned int keypad_col_gpios_u8150[] = { 41, 40, 39, 38 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8150) + (col))
static const unsigned short keypad_keymap_u8150[ARRAY_SIZE(keypad_col_gpios_u8150) *
					      ARRAY_SIZE(keypad_row_gpios_u8150)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_RIGHT,  /* KEY_RIGHT 106 */ 
	[KEYMAP_INDEX(0, 1)] = KEY_UP,  /* KEY_UP 103  */
	[KEYMAP_INDEX(0, 2)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115  */ 
	[KEYMAP_INDEX(0, 3)] = 62,   /* ENDCALL 62 */ 

	[KEYMAP_INDEX(1, 0)] = KEY_SEND,   /* KEY_SEND 231 */  
	[KEYMAP_INDEX(1, 1)] = KEY_LEFT,  /* KEY_LEFT 105  */ 
	[KEYMAP_INDEX(1, 2)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114 */
	[KEYMAP_INDEX(1, 3)] = 232,  /* DPAD_CENTER 232 */ 

	[KEYMAP_INDEX(2, 0)] = 0,  /* KEY_RESERVED */ 
	[KEYMAP_INDEX(2, 1)] = 0,  /* KEY_RESERVED */ 
	[KEYMAP_INDEX(2, 2)] = 0,  /* KEY_RESERVED */ 
	[KEYMAP_INDEX(2, 3)] = KEY_DOWN,  /* KEY_DOWN 108   */  
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8150_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8150,
	.output_gpios	= keypad_row_gpios_u8150,
	.input_gpios	= keypad_col_gpios_u8150,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8150),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8150),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};

static struct gpio_event_info *u8150_keypad_info[] = {
	&u8150_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8150_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8150_keypad_info,
	.info_count	= ARRAY_SIZE(u8150_keypad_info)
};

struct platform_device keypad_device_u8150 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8150_keypad_data,
	},
};
/*------------------------------- U8150_EU/U8150_JP keypad end ----------------------------------------*/
/*          U8160 keypad begin                                                                 */
static unsigned int keypad_row_gpios_u8160[] = { 35 };//driver
static unsigned int keypad_col_gpios_u8160[] = { 41, 40 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8160) + (col))
static const unsigned short keypad_keymap_u8160[ARRAY_SIZE(keypad_col_gpios_u8160) *
					      ARRAY_SIZE(keypad_row_gpios_u8160)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115 */ 
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114  */
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8160_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8160,
	.output_gpios	= keypad_row_gpios_u8160,
	.input_gpios	= keypad_col_gpios_u8160,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8160),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8160),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};
static struct gpio_event_info *u8160_keypad_info[] = {
	&u8160_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8160_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8160_keypad_info,
	.info_count	= ARRAY_SIZE(u8160_keypad_info)
};

struct platform_device keypad_device_u8160 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8160_keypad_data,
	},
};
/* u8160 keypad end          */

/*          U8130 keypad begin                                                                 */
static unsigned int keypad_row_gpios_u8130[] = { 35 };//driver
static unsigned int keypad_col_gpios_u8130[] = { 41, 40, 39 };//sense
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8130) + (col))
static const unsigned short keypad_keymap_u8130[ARRAY_SIZE(keypad_col_gpios_u8130) *
					      ARRAY_SIZE(keypad_row_gpios_u8130)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115 */ 
	[KEYMAP_INDEX(0, 1)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN  114  */
	[KEYMAP_INDEX(0, 2)] = KEY_HOME,  /* KEY_HOME  102  */
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8130_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8130,
	.output_gpios	= keypad_row_gpios_u8130,
	.input_gpios	= keypad_col_gpios_u8130,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8130),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8130),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};
static struct gpio_event_info *u8130_keypad_info[] = {
	&u8130_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8130_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8130_keypad_info,
	.info_count	= ARRAY_SIZE(u8130_keypad_info)
};

struct platform_device keypad_device_u8130 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8130_keypad_data,
	},
};
/* u8130 keypad end          */

/*                                          u8500 keypad begin                                                                 */
static unsigned int keypad_row_gpios_u8500[] = { 35, 34, 32 };//driver
static unsigned int keypad_col_gpios_u8500[] = { 41, 40 };//sensor
#undef  KEYMAP_INDEX
#define KEYMAP_INDEX(row, col) ((row)*ARRAY_SIZE(keypad_col_gpios_u8500) + (col))
static const unsigned short keypad_keymap_u8500[ARRAY_SIZE(keypad_col_gpios_u8500) *
					      ARRAY_SIZE(keypad_row_gpios_u8500)] = {
	[KEYMAP_INDEX(0, 0)] = KEY_SEND,   /* KEY_SEND 231 */ 
	[KEYMAP_INDEX(0, 1)] = KEY_MENU,  /* KEY_MENU  139  */ 
	
	[KEYMAP_INDEX(1, 0)] = KEY_VOLUMEUP,  /* KEY_VOLUMEUP 115 */ 
	[KEYMAP_INDEX(1, 1)] = KEY_VOLUMEDOWN,  /* KEY_VOLUMEDOWN 114 */ 

	[KEYMAP_INDEX(2, 0)] = 62,   /* ENDCALL 62 */ 
	[KEYMAP_INDEX(2, 1)] = KEY_BACK,
};

/*  keypad platform device information */
static struct gpio_event_matrix_info u8500_keypad_matrix_info = {
	.info.func	= gpio_event_matrix_func,
	.keymap		= keypad_keymap_u8500,
	.output_gpios	= keypad_row_gpios_u8500,
	.input_gpios	= keypad_col_gpios_u8500,
	.noutputs	= ARRAY_SIZE(keypad_row_gpios_u8500),
	.ninputs	= ARRAY_SIZE(keypad_col_gpios_u8500),
	.settle_time.tv.nsec = 40 * NSEC_PER_USEC,
	.poll_time.tv.nsec = 20 * NSEC_PER_MSEC,
	.flags		= GPIOKPF_LEVEL_TRIGGERED_IRQ | GPIOKPF_PRINT_UNMAPPED_KEYS | GPIOKPF_PRINT_MAPPED_KEYS
};

static struct gpio_event_info *u8500_keypad_info[] = {
	&u8500_keypad_matrix_info.info
};

static struct gpio_event_platform_data u8500_keypad_data = {
	.name		= "surf_keypad",
	.info		= u8500_keypad_info,
	.info_count	= ARRAY_SIZE(u8500_keypad_info)
};

struct platform_device keypad_device_u8500 = {
	.name	= GPIO_EVENT_DEV_NAME,
	.id	= -1,
	.dev	= {
		.platform_data	= &u8500_keypad_data,
	},
};
