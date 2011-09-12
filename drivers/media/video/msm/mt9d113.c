/*
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "mt9d113.h"

#include "linux/hardware_self_adapt.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "mt9d113.c: " fmt, ## args)

/*=============================================================
    SENSOR REGISTER DEFINES
==============================================================*/
#define MT9D113_REG_CHIP_ID 0x0000
#define MT9D113_CHIP_ID 0x2580
#define MT9D113_REG_RESET_REGISTER 0x12


enum mt9d113_test_mode_t
{
    TEST_OFF,
    TEST_1,
    TEST_2,
    TEST_3
};

enum mt9d113_resolution_t
{
    QTR_SIZE,
    FULL_SIZE,
    INVALID_SIZE
};

enum mt9d113_reg_update_t
{
    /* Sensor egisters that need to be updated during initialization */
    REG_INIT,

    /* Sensor egisters that needs periodic I2C writes */
    UPDATE_PERIODIC,

    /* All the sensor Registers will be updated */
    UPDATE_ALL,

    /* Not valid update */
    UPDATE_INVALID
};

enum mt9d113_setting_t
{
    RES_PREVIEW,
    RES_CAPTURE
};

typedef enum
{
  CAMERA_WB_MIN_MINUS_1,
  CAMERA_WB_AUTO = 1,
  CAMERA_WB_CUSTOM,
  CAMERA_WB_INCANDESCENT,
  CAMERA_WB_FLUORESCENT,
  CAMERA_WB_DAYLIGHT,
  CAMERA_WB_CLOUDY_DAYLIGHT,
  CAMERA_WB_TWILIGHT,
  CAMERA_WB_SHADE,
  CAMERA_WB_MAX_PLUS_1
} config3a_wb_t;

/*
 * Time in milisecs for waiting for the sensor to reset.
 */
#define MT9D113_RESET_DELAY_MSECS 66

/* for 30 fps preview */
#define MT9D113_DEFAULT_CLOCK_RATE 12000000

#define BYD_MODE_ID 0x02                
#define LITEON_MODE_ID 0x01             
#define SUNNY_MODE_ID 0x00    

#define HW_MT9D113_VER_2   2
#define HW_MT9D113_VER_3   3

#define MT9D113_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
/* FIXME: Changes from here */
struct mt9d113_work_t
{
    struct work_struct work;
};

struct mt9d113_ctrl_t
{
    const struct  msm_camera_sensor_info *sensordata;

    int sensormode;
    uint32_t           fps_divider; /* init to 1 * 0x00000400 */
    uint32_t           pict_fps_divider; /* init to 1 * 0x00000400 */

    uint16_t curr_lens_pos;
    uint16_t init_curr_lens_pos;
    uint16_t my_reg_gain;
    uint32_t my_reg_line_count;

    enum mt9d113_resolution_t prev_res;
    enum mt9d113_resolution_t pict_res;
    enum mt9d113_resolution_t curr_res;
    enum mt9d113_test_mode_t  set_test;

    unsigned short imgaddr;
};

struct mt9d113_i2c_reg_conf
{
	unsigned short waddr;
	unsigned short wdata;
};

const static char mt9d113_supported_effect[] = "none,mono,negative,solarize,sepia,aqua";

static struct mt9d113_i2c_reg_conf mt9d113_init_reg_config_comm[] =
{
    {0x0014, 0xA0FB},
    {0x0014, 0xA0F9},
    {0x0014, 0x21F9},
    {0x0010, 0x0462},
    {0x0012, 0x00F9},
                     
    {0x0014, 0x21FB},
    {0x0014, 0x20FB},
                     
    {0x0014, 0x20FA},
                     
    {0x321C, 0x0003},
                     
    {0x098C, 0x2703},
    {0x0990, 0x0320},
    {0x098C, 0x2705},
    {0x0990, 0x0258},
    {0x098C, 0x2707},
    {0x0990, 0x0640},
    {0x098C, 0x2709},
    {0x0990, 0x04B0},
    {0x098C, 0x270D},
    {0x0990, 0x0000},
    {0x098C, 0x270F},
    {0x0990, 0x0000},
    {0x098C, 0x2711},
    {0x0990, 0x04BD},
    {0x098C, 0x2713},
    {0x0990, 0x064D},
    {0x098C, 0x2715},
    {0x0990, 0x0111},
    {0x098C, 0x2717},
    {0x0990, 0x046C},
    {0x098C, 0x2719},
    {0x0990, 0x005A},
    {0x098C, 0x271B},
    {0x0990, 0x01BE},
    {0x098C, 0x271D},
    {0x0990, 0x0131},
    {0x098C, 0x271F},
    {0x0990, 0x02B3},
             
    {0x098C, 0x2721},
    {0x0990, 0x0897},
                     
    {0x098C, 0x2723},
    {0x0990, 0x0004},
    {0x098C, 0x2725},
    {0x0990, 0x0004},
    {0x098C, 0x2727},
    {0x0990, 0x04BB},
    {0x098C, 0x2729},
    {0x0990, 0x064B},
    {0x098C, 0x272B},
    {0x0990, 0x0111},
    {0x098C, 0x272D},
    {0x0990, 0x0024},
    {0x098C, 0x272F},
    {0x0990, 0x003A},
    {0x098C, 0x2731},
    {0x0990, 0x00F6},
    {0x098C, 0x2733},
    {0x0990, 0x008B},
    {0x098C, 0x2735},
    {0x0990, 0x050D},
    {0x098C, 0x2737},
    {0x0990, 0x0895},
    {0x098C, 0x2739},
    {0x0990, 0x0000},
    {0x098C, 0x273B},
    {0x0990, 0x031F},
    {0x098C, 0x273D},
    {0x0990, 0x0000},
    {0x098C, 0x273F},
    {0x0990, 0x0257},
    {0x098C, 0x2747},
    {0x0990, 0x0000},
    {0x098C, 0x2749},
    {0x0990, 0x063F},
    {0x098C, 0x274B},
    {0x0990, 0x0000},
    {0x098C, 0x274D},
    {0x0990, 0x04AF},
    {0x098C, 0x222D},
    {0x0990, 0x005B},
    {0x098C, 0xA408},
    {0x0990, 0x0015},
    {0x098C, 0xA409},
    {0x0990, 0x0018},
    {0x098C, 0xA40A},
    {0x0990, 0x001A},
    {0x098C, 0xA40B},
    {0x0990, 0x001D},

    {0x098C, 0x2411},
    {0x0990, 0x006d},
    {0x098C, 0x2413},
    {0x0990, 0x006D},
    {0x098C, 0x2415},
    {0x0990, 0x006d},
                      
    {0x098C, 0x2417},
    {0x0990, 0x006D},
                      
    {0x098C, 0xA404},
    {0x0990, 0x00C0},
                      
    {0x098C, 0xA40D},
    {0x0990, 0x0001},
    {0x098C, 0xA40E},
    {0x0990, 0x0002},
    {0x098C, 0xA410},
    {0x0990, 0x0005},
                      
    {0x098C, 0x2b62},
    {0x0990, 0xFFFF},
    {0x098C, 0x2b64},
    {0x0990, 0xFFFF},
                      
    {0x001E, 0x0505},
                      
    {0x098C, 0xA117 },
    {0x0990, 0x0002 },
    {0x098C, 0xA11D },
    {0x0990, 0x0002 },
    {0x098C, 0xA129 },
    {0x0990, 0x0002 },
                      
    {0x098C, 0xA118 },
    {0x0990, 0x0002 },
    {0x098C, 0xA11E },
    {0x0990, 0x0002 },
    {0x098C, 0xA12A },
    {0x0990, 0x0002 },
                      
    {0x098C, 0xA20C},
    {0x0990, 0x0008},
    {0x098C, 0xA215},
    {0x0990, 0x0008},
                      
    {0x098C, 0x271F},
    {0x0990, 0x0293},
                      
    {0x098C, 0x272D},
    {0x0990, 0x0027},
    {0x098C, 0x2717},
    {0x0990, 0x046F},
   

    {0x098C, 0xA768},
    {0x0990, 0x0006},

    {0x098C, 0xAB37},
    {0x0990, 0x0001},
                    
    {0x3084, 0x240C},
    {0x3092, 0x0A4C},
    {0x3094, 0x4C4C},
    {0x3096, 0x4C54},
    
    {0x098C, 0x2755},
    {0x0990, 0x0200},    
    {0x098C, 0x2757},
    {0x0990, 0x0200},    
    {0x098C, 0xa103},
    {0x0990, 0x0005},   
};

static struct mt9d113_i2c_reg_conf mt9d113_init_reg_config_comm_u8300[] =
{
    {0x0014, 0xA0FB},
    {0x0014, 0xA0F9},
    {0x0014, 0x21F9},
    {0x0010, 0x0462},
    {0x0012, 0x00F9},
                     
    {0x0014, 0x21FB},
    {0x0014, 0x20FB},
                     
    {0x0014, 0x20FA},
                     
    {0x321C, 0x0003},
                     
    {0x098C, 0x2703},
    {0x0990, 0x0320},
    {0x098C, 0x2705},
    {0x0990, 0x0258},
    {0x098C, 0x2707},
    {0x0990, 0x0640},
    {0x098C, 0x2709},
    {0x0990, 0x04B0},
    {0x098C, 0x270D},
    {0x0990, 0x0000},
    {0x098C, 0x270F},
    {0x0990, 0x0000},
    {0x098C, 0x2711},
    {0x0990, 0x04BD},
    {0x098C, 0x2713},
    {0x0990, 0x064D},
    {0x098C, 0x2715},
    {0x0990, 0x0111},
    {0x098C, 0x2717},
    {0x0990, 0x046C},
    {0x098C, 0x2719},
    {0x0990, 0x005A},
    {0x098C, 0x271B},
    {0x0990, 0x01BE},
    {0x098C, 0x271D},
    {0x0990, 0x0131},
    {0x098C, 0x271F},
    {0x0990, 0x02B3},
             
    {0x098C, 0x2721},
    {0x0990, 0x0897},
                     
    {0x098C, 0x2723},
    {0x0990, 0x0004},
    {0x098C, 0x2725},
    {0x0990, 0x0004},
    {0x098C, 0x2727},
    {0x0990, 0x04BB},
    {0x098C, 0x2729},
    {0x0990, 0x064B},
    {0x098C, 0x272B},
    {0x0990, 0x0111},
    {0x098C, 0x272D},
    {0x0990, 0x0024},
    {0x098C, 0x272F},
    {0x0990, 0x003A},
    {0x098C, 0x2731},
    {0x0990, 0x00F6},
    {0x098C, 0x2733},
    {0x0990, 0x008B},
    {0x098C, 0x2735},
    {0x0990, 0x050D},
    {0x098C, 0x2737},
    {0x0990, 0x0895},
    {0x098C, 0x2739},
    {0x0990, 0x0000},
    {0x098C, 0x273B},
    {0x0990, 0x031F},
    {0x098C, 0x273D},
    {0x0990, 0x0000},
    {0x098C, 0x273F},
    {0x0990, 0x0257},
    {0x098C, 0x2747},
    {0x0990, 0x0000},
    {0x098C, 0x2749},
    {0x0990, 0x063F},
    {0x098C, 0x274B},
    {0x0990, 0x0000},
    {0x098C, 0x274D},
    {0x0990, 0x04AF},
    {0x098C, 0x222D},
    {0x0990, 0x005B},
    {0x098C, 0xA408},
    {0x0990, 0x0015},
    {0x098C, 0xA409},
    {0x0990, 0x0018},
    {0x098C, 0xA40A},
    {0x0990, 0x001A},
    {0x098C, 0xA40B},
    {0x0990, 0x001D},

    {0x098C, 0x2411},
    {0x0990, 0x006d},
    {0x098C, 0x2413},
    {0x0990, 0x006D},
    {0x098C, 0x2415},
    {0x0990, 0x006d},
                      
    {0x098C, 0x2417},
    {0x0990, 0x006D},
                      
    {0x098C, 0xA404},
    {0x0990, 0x00C0},
                      
    {0x098C, 0xA40D},
    {0x0990, 0x0001},
    {0x098C, 0xA40E},
    {0x0990, 0x0002},
    {0x098C, 0xA410},
    {0x0990, 0x0005},
                      
    {0x098C, 0x2b62},
    {0x0990, 0xFFFF},
    {0x098C, 0x2b64},
    {0x0990, 0xFFFF},
                      
    {0x001E, 0x0505},
                      
    {0x098C, 0xA117 },
    {0x0990, 0x0002 },
    {0x098C, 0xA11D },
    {0x0990, 0x0002 },
    {0x098C, 0xA129 },
    {0x0990, 0x0002 },
                      
    {0x098C, 0xA118 },
    {0x0990, 0x0002 },
    {0x098C, 0xA11E },
    {0x0990, 0x0002 },
    {0x098C, 0xA12A },
    {0x0990, 0x0002 },
                      
    {0x098C, 0xA20C},
    {0x0990, 0x0008},
    {0x098C, 0xA215},
    {0x0990, 0x0008},
                      
    {0x098C, 0x271F},
    {0x0990, 0x0293},
                      
/* delete some register for data mirrored vertically and horizontally */
//    {0x098C, 0x272D},
//    {0x0990, 0x0027},
//    {0x098C, 0x2717},
//    {0x0990, 0x046F},

    {0x098C, 0xA768},
    {0x0990, 0x0006},

    {0x098C, 0xAB37},
    {0x0990, 0x0001},
                    
    {0x3084, 0x240C},
    {0x3092, 0x0A4C},
    {0x3094, 0x4C4C},
    {0x3096, 0x4C54},
    
    {0x098C, 0x2755},
    {0x0990, 0x0200},    
    {0x098C, 0x2757},
    {0x0990, 0x0200},    
    {0x098C, 0xa103},
    {0x0990, 0x0005},   
};
static struct mt9d113_i2c_reg_conf mt9d113_init_reg_config_no_ver2[] =
{
    {0x098C, 0xAB20},
    {0x0990, 0x0080},
    {0x098C, 0xAB24},
    {0x0990, 0x0020},
                     
    {0x098C, 0x2212},
    {0x0990, 0x0080},
    {0x098C, 0xA20E},
    {0x0990, 0x0080},
        
};
static struct mt9d113_i2c_reg_conf mt9d113_init_reg_config_ver2[] =
{
    {0x098C, 0xAB20},
    {0x0990, 0x0032},
};
static struct mt9d113_i2c_reg_conf mt9d113_lens_reg_config_byd[] =
{
    {0x3658, 0x01B0},
    {0x365A, 0x6A8B},
    {0x365C, 0x13D2},
    {0x365E, 0xB66E},
    {0x3660, 0x9D2F},
    {0x3680, 0x19CD},
    {0x3682, 0x722F},
    {0x3684, 0xA9AE},
    {0x3686, 0x9792},
    {0x3688, 0xAC12},
    {0x36A8, 0x0533},
    {0x36AA, 0xDA4D},
    {0x36AC, 0xB394},
    {0x36AE, 0xC294},
    {0x36B0, 0x1E17},
    {0x36D0, 0x9030},
    {0x36D2, 0x9730},
    {0x36D4, 0xD094},
    {0x36D6, 0xB393},
    {0x36D8, 0x2717},
    {0x36F8, 0xE6B3},
    {0x36FA, 0x88F5},
    {0x36FC, 0x2B17},
    {0x36FE, 0x1919},
    {0x3700, 0xE17A},
    {0x364E, 0x0790},
    {0x3650, 0xE309},
    {0x3652, 0x1ED2},
    {0x3654, 0x8B2F},
    {0x3656, 0x8713},
    {0x3676, 0x7FAB},
    {0x3678, 0xFDAF},
    {0x367A, 0x0570},
    {0x367C, 0x7B71},
    {0x367E, 0xF412},
    {0x369E, 0x0CD3},
    {0x36A0, 0xD86E},
    {0x36A2, 0x80F6},
    {0x36A4, 0xD9B3},
    {0x36A6, 0x2E38},
    {0x36C6, 0xA470},
    {0x36C8, 0x504E},
    {0x36CA, 0xD8B3},
    {0x36CC, 0x4733},
    {0x36CE, 0x6CD6},
    {0x36EE, 0xCD34},
    {0x36F0, 0xFC94},
    {0x36F2, 0x3A98},
    {0x36F4, 0x7F78},
    {0x36F6, 0x8B1B},
    {0x3662, 0x00B0},
    {0x3664, 0x88CB},
    {0x3666, 0x18F2},
    {0x3668, 0xA74C},
    {0x366A, 0xBF92},
    {0x368A, 0x80CB},
    {0x368C, 0xF4CF},
    {0x368E, 0x3130},
    {0x3690, 0x2232},
    {0x3692, 0x9EB3},
    {0x36B2, 0x0A53},
    {0x36B4, 0xC1ED},
    {0x36B6, 0xA095},
    {0x36B8, 0xB474},
    {0x36BA, 0x5DF7},
    {0x36DA, 0xE84D},
    {0x36DC, 0x0952},
    {0x36DE, 0xAEB4},
    {0x36E0, 0xED13},
    {0x36E2, 0x0C37},
    {0x3702, 0xA9B4},
    {0x3704, 0xD754},
    {0x3706, 0x1897},
    {0x3708, 0x0EF9},
    {0x370A, 0x951A},
    {0x366C, 0x0250},
    {0x366E, 0x302A},
    {0x3670, 0x1ED2},
    {0x3672, 0xB20F},
    {0x3674, 0x9BF3},
    {0x3694, 0x054D},
    {0x3696, 0x4B2F},
    {0x3698, 0x524D},
    {0x369A, 0xBDD1},
    {0x369C, 0xE112},
    {0x36BC, 0x0FF3},
    {0x36BE, 0xED2E},
    {0x36C0, 0xFBD5},
    {0x36C2, 0xF973},
    {0x36C4, 0x25B8},
    {0x36E4, 0xF42E},
    {0x36E6, 0x5790},
    {0x36E8, 0x8F55},
    {0x36EA, 0xD9F4},
    {0x36EC, 0x54D7},
    {0x370C, 0xCA74},
    {0x370E, 0x8415},
    {0x3710, 0x1FD8},
    {0x3712, 0x00D9},
    {0x3714, 0xE9FA},
    {0x3644, 0x0344},
    {0x3642, 0x0268},
    {0x3210, 0x01B8},
};

static struct mt9d113_i2c_reg_conf mt9d113_lens_reg_config_liteon[] =
{
    {0x3658, 0x00D0},
    {0x365A, 0x89CB},
    {0x365C, 0x55B1},
    {0x365E, 0x19AD},
    {0x3660, 0x1FAD},
    {0x3680, 0x6CC3},
    {0x3682, 0x788F},
    {0x3684, 0x2DF0},
    {0x3686, 0xDF11},
    {0x3688, 0x8111},
    {0x36A8, 0x36F2},
    {0x36AA, 0xC711},
    {0x36AC, 0x01F5},
    {0x36AE, 0x00B5},
    {0x36B0, 0x9ED8},
    {0x36D0, 0x102E},
    {0x36D2, 0xFD31},
    {0x36D4, 0xEB94},
    {0x36D6, 0x8733},
    {0x36D8, 0x16F6},
    {0x36F8, 0x15B3},
    {0x36FA, 0x0AB5},
    {0x36FC, 0xBCD9},
    {0x36FE, 0xB358},
    {0x3700, 0x119C},
    {0x364E, 0x06D0},
    {0x3650, 0xD0CA},
    {0x3652, 0x5D91},
    {0x3654, 0x37EE},
    {0x3656, 0x85D2},
    {0x3676, 0x012D},
    {0x3678, 0xA34F},
    {0x367A, 0xC40F},
    {0x367C, 0x1911},
    {0x367E, 0x5291},
    {0x369E, 0x3152},
    {0x36A0, 0xD470},
    {0x36A2, 0x43B1},
    {0x36A4, 0x0115},
    {0x36A6, 0x81B7},
    {0x36C6, 0x95EF},
    {0x36C8, 0xCE12},
    {0x36CA, 0xD873},
    {0x36CC, 0x1935},
    {0x36CE, 0x3115},
    {0x36EE, 0x5851},
    {0x36F0, 0x2634},
    {0x36F2, 0xC138},
    {0x36F4, 0xB158},
    {0x36F6, 0x2FDB},
    {0x3662, 0x00B0},
    {0x3664, 0x1BAA},
    {0x3666, 0x2D91},
    {0x3668, 0x3CCD},
    {0x366A, 0xCB10},
    {0x368A, 0x8089},
    {0x368C, 0x8F4F},
    {0x368E, 0x650F},
    {0x3690, 0x6791},
    {0x3692, 0xA630},
    {0x36B2, 0x2912},
    {0x36B4, 0x9612},
    {0x36B6, 0x4632},
    {0x36B8, 0x1FF5},
    {0x36BA, 0xBCF6},
    {0x36DA, 0x5870},
    {0x36DC, 0xBE72},
    {0x36DE, 0xBA75},
    {0x36E0, 0x2FB4},
    {0x36E2, 0x0157},
    {0x3702, 0x5DF2},
    {0x3704, 0x3DF5},
    {0x3706, 0xDB38},
    {0x3708, 0xF478},
    {0x370A, 0x1CBB},
    {0x366C, 0x01D0},
    {0x366E, 0xCC6A},
    {0x3670, 0x5551},
    {0x3672, 0x0D4E},
    {0x3674, 0x83F2},
    {0x3694, 0x240C},
    {0x3696, 0x4DEF},
    {0x3698, 0x536E},
    {0x369A, 0xE990},
    {0x369C, 0x8211},
    {0x36BC, 0x3C72},
    {0x36BE, 0xA1B0},
    {0x36C0, 0xA770},
    {0x36C2, 0x7234},
    {0x36C4, 0xDE36},
    {0x36E4, 0x8AAF},
    {0x36E6, 0xC70E},
    {0x36E8, 0xBF53},
    {0x36EA, 0x8855},
    {0x36EC, 0x0FB6},
    {0x370C, 0xCA2D},
    {0x370E, 0x19B4},
    {0x3710, 0xA9F8},
    {0x3712, 0xAEB8},
    {0x3714, 0x205B},
    {0x3644, 0x02F0},
    {0x3642, 0x025C},
    {0x3210, 0x01B8},
};
static struct mt9d113_i2c_reg_conf mt9d113_lens_reg_config_sunny[] =
{
    {0x3658, 0x0110},
    {0x365A, 0xE4CC},
    {0x365C, 0x1D72},
    {0x365E, 0x220D},
    {0x3660, 0x83B1},
    {0x3680, 0x14CD},
    {0x3682, 0x0250},
    {0x3684, 0x20CB},
    {0x3686, 0x9FD2},
    {0x3688, 0x84AE},
    {0x36A8, 0x7452},
    {0x36AA, 0xDA6F},
    {0x36AC, 0x52F3},
    {0x36AE, 0x4E92},
    {0x36B0, 0xAA57},
    {0x36D0, 0x18AE},
    {0x36D2, 0x0570},
    {0x36D4, 0xEB53},
    {0x36D6, 0x8355},
    {0x36D8, 0x1A75},
    {0x36F8, 0xBB2F},
    {0x36FA, 0x4172},
    {0x36FC, 0x8078},
    {0x36FE, 0xAC36},
    {0x3700, 0x025A},
    {0x364E, 0x05F0},
    {0x3650, 0x9C6C},
    {0x3652, 0x2032},
    {0x3654, 0xA46C},
    {0x3656, 0xCF72},
    {0x3676, 0x422C},
    {0x3678, 0xE98F},
    {0x367A, 0xDF0B},
    {0x367C, 0x4E91},
    {0x367E, 0x4BF1},
    {0x369E, 0x6D12},
    {0x36A0, 0xA1B1},
    {0x36A2, 0x01D3},
    {0x36A4, 0x27D3},
    {0x36A6, 0xFC96},
    {0x36C6, 0x3F8B},
    {0x36C8, 0x14EF},
    {0x36CA, 0xAF92},
    {0x36CC, 0x7FD1},
    {0x36CE, 0x78ED},
    {0x36EE, 0xB2D2},
    {0x36F0, 0x3CF3},
    {0x36F2, 0xEC97},
    {0x36F4, 0xEFB6},
    {0x36F6, 0x1BFA},
    {0x3662, 0x00D0},
    {0x3664, 0x836D},
    {0x3666, 0x0A92},
    {0x3668, 0x000E},
    {0x366A, 0x8A72},
    {0x368A, 0x1BAC},
    {0x368C, 0x954F},
    {0x368E, 0x2E8E},
    {0x3690, 0x5850},
    {0x3692, 0xAED0},
    {0x36B2, 0x53B2},
    {0x36B4, 0xF5B0},
    {0x36B6, 0x2213},
    {0x36B8, 0x4E93},
    {0x36BA, 0xF596},
    {0x36DA, 0x236F},
    {0x36DC, 0xE96C},
    {0x36DE, 0xBBD3},
    {0x36E0, 0x5571},
    {0x36E2, 0x6D15},
    {0x3702, 0xA671},
    {0x3704, 0x3E73},
    {0x3706, 0xDE97},
    {0x3708, 0x8437},
    {0x370A, 0x05DA},
    {0x366C, 0x00F0},
    {0x366E, 0x9A0C},
    {0x3670, 0x1AD2},
    {0x3672, 0xD0AB},
    {0x3674, 0xD392},
    {0x3694, 0x77CC},
    {0x3696, 0x02F0},
    {0x3698, 0xD3CE},
    {0x369A, 0x81B2},
    {0x369C, 0x3B31},
    {0x36BC, 0x6B12},
    {0x36BE, 0x88D1},
    {0x36C0, 0x76F2},
    {0x36C2, 0x01D3},
    {0x36C4, 0x82F7},
    {0x36E4, 0x40CF},
    {0x36E6, 0x6A2F},
    {0x36E8, 0x8034},
    {0x36EA, 0xE834},
    {0x36EC, 0x0D95},
    {0x370C, 0xBC12},
    {0x370E, 0x6E13},
    {0x3710, 0xEAB7},
    {0x3712, 0xF896},
    {0x3714, 0x1D7A},
    {0x3644, 0x032C},
    {0x3642, 0x0258},
    {0x3210, 0x01B8},
};

static struct mt9d113_i2c_reg_conf mt9d113_awb_ccm_reg_config_byd[] =
{
    {0x098C, 0x2306},
    {0x0990, 0x0218},
    {0x098C, 0x2308},
    {0x0990, 0xFF1C},
    {0x098C, 0x230A},
    {0x0990, 0xFFCC},
    {0x098C, 0x230C},
    {0x0990, 0xFFA0},
    {0x098C, 0x230E},
    {0x0990, 0x01D2},
    {0x098C, 0x2310},
    {0x0990, 0xFF8E},
    {0x098C, 0x2312},
    {0x0990, 0xFFF3},
    {0x098C, 0x2314},
    {0x0990, 0xFF22},
    {0x098C, 0x2316},
    {0x0990, 0x01EB},
    {0x098C, 0x2318},
    {0x0990, 0x001D},
    {0x098C, 0x231A},
    {0x0990, 0x0037},
    {0x098C, 0x231C},
    {0x0990, 0xFFF6},
    {0x098C, 0x231E},
    {0x0990, 0xFFE4},
    {0x098C, 0x2320},
    {0x0990, 0x0026},
    {0x098C, 0x2322},
    {0x0990, 0x002B},
    {0x098C, 0x2324},
    {0x0990, 0xFFFC},
    {0x098C, 0x2326},
    {0x0990, 0xFFD9},
    {0x098C, 0x2328},
    {0x0990, 0x000E},
    {0x098C, 0x232A},
    {0x0990, 0x0049},
    {0x098C, 0x232C},
    {0x0990, 0xFFAA},
    {0x098C, 0x232E},
    {0x0990, 0x0016},
    {0x098C, 0x2330},
    {0x0990, 0xFFEB},
    {0x098C, 0xA348},
    {0x0990, 0x0008},
    {0x098C, 0xA349},
    {0x0990, 0x0002},
    {0x098C, 0xA34A},
    {0x0990, 0x0059},
    {0x098C, 0xA34B},
    {0x0990, 0x00C8},
    {0x098C, 0xA351},
    {0x0990, 0x0000},
    {0x098C, 0xA352},
    {0x0990, 0x007F},
    {0x098C, 0xA355},
    {0x0990, 0x0001},
    {0x098C, 0xA35D},
    {0x0990, 0x0078},
    {0x098C, 0xA35E},
    {0x0990, 0x0086},
    {0x098C, 0xA35F},
    {0x0990, 0x007E},
    {0x098C, 0xA360},
    {0x0990, 0x0082},
    {0x098C, 0x2361},
    {0x0990, 0x0040},
    {0x098C, 0xA363},
    {0x0990, 0x00D2},
    {0x098C, 0xA364},
    {0x0990, 0x00F6},
    {0x098C, 0xA302},
    {0x0990, 0x0000},
    {0x098C, 0xA303},
    {0x0990, 0x00EF},
    {0x366E, 0x9A0C},
    {0x3670, 0x1AD2},
    {0x3672, 0xD0AB},
    {0x3674, 0xD392},
    {0x3694, 0x77CC},
    {0x3696, 0x02F0},
    {0x3698, 0xD3CE},
    {0x369A, 0x81B2},
    {0x369C, 0x3B31},
    {0x36BC, 0x6B12},
    {0x36BE, 0x88D1},
    {0x36C0, 0x76F2},
    {0x36C2, 0x01D3},
    {0x36C4, 0x82F7},
    {0x36E4, 0x40CF},
    {0x36E6, 0x6A2F},
    {0x36E8, 0x8034},
    {0x36EA, 0xE834},
    {0x36EC, 0x0D95},
    {0x370C, 0xBC12},
    {0x370E, 0x6E13},
    {0x3710, 0xEAB7},
    {0x3712, 0xF896},
    {0x3714, 0x1D7A},
    {0x3644, 0x032C},
    {0x3642, 0x0258},
    {0x3210, 0x01B8},
};

static struct mt9d113_i2c_reg_conf mt9d113_awb_ccm_reg_config_sunny[] =
{
    {0x098C, 0x2306},
    {0x0990, 0x0218},
    {0x098C, 0x2308},
    {0x0990, 0xFF1C},
    {0x098C, 0x230A},
    {0x0990, 0xFFCC},
    {0x098C, 0x230C},
    {0x0990, 0xFFA0},
    {0x098C, 0x230E},
    {0x0990, 0x01D2},
    {0x098C, 0x2310},
    {0x0990, 0xFF8E},
    {0x098C, 0x2312},
    {0x0990, 0xFFF3},
    {0x098C, 0x2314},
    {0x0990, 0xFF22},
    {0x098C, 0x2316},
    {0x0990, 0x01EB},
    {0x098C, 0x2318},
    {0x0990, 0x001D},
    {0x098C, 0x231A},
    {0x0990, 0x0037},
    {0x098C, 0x231C},
    {0x0990, 0xFFF6},
    {0x098C, 0x231E},
    {0x0990, 0xFFE4},
    {0x098C, 0x2320},
    {0x0990, 0x0026},
    {0x098C, 0x2322},
    {0x0990, 0x002B},
    {0x098C, 0x2324},
    {0x0990, 0xFFFC},
    {0x098C, 0x2326},
    {0x0990, 0xFFD9},
    {0x098C, 0x2328},
    {0x0990, 0x000E},
    {0x098C, 0x232A},
    {0x0990, 0x0049},
    {0x098C, 0x232C},
    {0x0990, 0xFFAA},
    {0x098C, 0x232E},
    {0x0990, 0x0016},
    {0x098C, 0x2330},
    {0x0990, 0xFFEB},
    {0x098C, 0xA348},
    {0x0990, 0x0008},
    {0x098C, 0xA349},
    {0x0990, 0x0002},
    {0x098C, 0xA34A},
    {0x0990, 0x0059},
    {0x098C, 0xA34B},
    {0x0990, 0x00C8},
    {0x098C, 0xA351},
    {0x0990, 0x0000},
    {0x098C, 0xA352},
    {0x0990, 0x007F},
    {0x098C, 0xA355},
    {0x0990, 0x0001},
    {0x098C, 0xA35D},
    {0x0990, 0x0078},
    {0x098C, 0xA35E},
    {0x0990, 0x0086},
    {0x098C, 0xA35F},
    {0x0990, 0x007E},
    {0x098C, 0xA360},
    {0x0990, 0x0082},
    {0x098C, 0x2361},
    {0x0990, 0x0040},
    {0x098C, 0xA363},
    {0x0990, 0x00D2},
    {0x098C, 0xA364},
    {0x0990, 0x00F6},
    {0x098C, 0xA302},
    {0x0990, 0x0000},
    {0x098C, 0xA303},
    {0x0990, 0x00EF},
    {0x366E, 0x9A0C},
    {0x3670, 0x1AD2},
    {0x3672, 0xD0AB},
    {0x3674, 0xD392},
    {0x3694, 0x77CC},
    {0x3696, 0x02F0},
    {0x3698, 0xD3CE},
    {0x369A, 0x81B2},
    {0x369C, 0x3B31},
    {0x36BC, 0x6B12},
    {0x36BE, 0x88D1},
    {0x36C0, 0x76F2},
    {0x36C2, 0x01D3},
    {0x36C4, 0x82F7},
    {0x36E4, 0x40CF},
    {0x36E6, 0x6A2F},
    {0x36E8, 0x8034},
    {0x36EA, 0xE834},
    {0x36EC, 0x0D95},
    {0x370C, 0xBC12},
    {0x370E, 0x6E13},
    {0x3710, 0xEAB7},
    {0x3712, 0xF896},
    {0x3714, 0x1D7A},
    {0x3644, 0x032C},
    {0x3642, 0x0258},
    {0x3210, 0x01B8},
};

static struct mt9d113_i2c_reg_conf mt9d113_awb_ccm_reg_config_liteon[] =
{
    {0x098C, 0x2306},
    {0x0990, 0x01F5},
    {0x098C, 0x2308},
    {0x0990, 0xFF41},
    {0x098C, 0x230A},
    {0x0990, 0xFFC9},
    {0x098C, 0x230C},
    {0x0990, 0xFF9F},
    {0x098C, 0x230E},
    {0x0990, 0x01D9},
    {0x098C, 0x2310},
    {0x0990, 0xFF88},
    {0x098C, 0x2312},
    {0x0990, 0xFFED},
    {0x098C, 0x2314},
    {0x0990, 0xFF3C},
    {0x098C, 0x2316},
    {0x0990, 0x01D7},
    {0x098C, 0x2318},
    {0x0990, 0x0020},
    {0x098C, 0x231A},
    {0x0990, 0x003B},
    {0x098C, 0x231C},
    {0x0990, 0xFFD3},
    {0x098C, 0x231E},
    {0x0990, 0x001D},
    {0x098C, 0x2320},
    {0x0990, 0x0011},
    {0x098C, 0x2322},
    {0x0990, 0x0024},
    {0x098C, 0x2324},
    {0x0990, 0x000C},
    {0x098C, 0x2326},
    {0x0990, 0xFFD0},
    {0x098C, 0x2328},
    {0x0990, 0x0010},
    {0x098C, 0x232A},
    {0x0990, 0x0036},
    {0x098C, 0x232C},
    {0x0990, 0xFFBA},
    {0x098C, 0x232E},
    {0x0990, 0x001A},
    {0x098C, 0x2330},
    {0x0990, 0xFFE8},
    {0x098C, 0xA348},
    {0x0990, 0x0008},
    {0x098C, 0xA349},
    {0x0990, 0x0002},
    {0x098C, 0xA34A},
    {0x0990, 0x0059},
    {0x098C, 0xA34B},
    {0x0990, 0x00C8},
    {0x098C, 0xA351},
    {0x0990, 0x0000},
    {0x098C, 0xA352},
    {0x0990, 0x007F},
    {0x098C, 0xA355},
    {0x0990, 0x0001},
    {0x098C, 0xA35D},
    {0x0990, 0x0078},
    {0x098C, 0xA35E},
    {0x0990, 0x0086},
    {0x098C, 0xA35F},
    {0x0990, 0x007E},
    {0x098C, 0xA360},
    {0x0990, 0x0082},
    {0x098C, 0x2361},
    {0x0990, 0x0040},
    {0x098C, 0xA363},
    {0x0990, 0x00D2},
    {0x098C, 0xA364},
    {0x0990, 0x00F6},
    {0x098C, 0xA302},
    {0x0990, 0x0000},
    {0x098C, 0xA303},
    {0x0990, 0x00EF},
    {0x366E, 0x9A0C},
    {0x3670, 0x1AD2},
    {0x3672, 0xD0AB},
    {0x3674, 0xD392},
    {0x3694, 0x77CC},
    {0x3696, 0x02F0},
    {0x3698, 0xD3CE},
    {0x369A, 0x81B2},
    {0x369C, 0x3B31},
    {0x36BC, 0x6B12},
    {0x36BE, 0x88D1},
    {0x36C0, 0x76F2},
    {0x36C2, 0x01D3},
    {0x36C4, 0x82F7},
    {0x36E4, 0x40CF},
    {0x36E6, 0x6A2F},
    {0x36E8, 0x8034},
    {0x36EA, 0xE834},
    {0x36EC, 0x0D95},
    {0x370C, 0xBC12},
    {0x370E, 0x6E13},
    {0x3710, 0xEAB7},
    {0x3712, 0xF896},
    {0x3714, 0x1D7A},
    {0x3644, 0x032C},
    {0x3642, 0x0258},
    {0x3210, 0x01B8},
};

static struct mt9d113_i2c_reg_conf mt9d113_uxga_config[] =
{
    {0x098C, 0xA115},
    {0x0990, 0x0002},
    {0x098C, 0xA103},
    {0x0990, 0x0002},
};

static struct mt9d113_i2c_reg_conf mt9d113_svga_config[] =
{
    {0x098C, 0xA115},
    {0x0990, 0x0000},
    {0x098C, 0xA103},
    {0x0990, 0x0001},
};

static struct mt9d113_i2c_reg_conf mt9d113_effect_off_reg_config[] =
{
    {0x098C, 0x2759},
    {0x0990, 0x6440},
    {0x098C, 0x275B},
    {0x0990, 0x6440},
    {0x098C, 0xA103},
    {0x0990, 0x0006},
};
static struct mt9d113_i2c_reg_conf mt9d113_effect_mono_reg_config[] =
{
    {0x098C, 0x2759},
    {0x0990, 0x6441},
    {0x098C, 0x275B},
    {0x0990, 0x6441},
    {0x098C, 0xA103},
    {0x0990, 0x0006},
};
static struct mt9d113_i2c_reg_conf mt9d113_effect_negative_reg_config[] =
{
    {0x098C, 0x2759},
    {0x0990, 0x6443},
    {0x098C, 0x275B},
    {0x0990, 0x6443},
    {0x098C, 0xA103},
    {0x0990, 0x0006},
};
static struct mt9d113_i2c_reg_conf mt9d113_effect_solarize_reg_config[] =
{
    {0x098C, 0x2759},
    {0x0990, 0x3244},
    {0x098C, 0x275B},
    {0x0990, 0x3244},
    {0x098C, 0xA103},
    {0x0990, 0x0006},
};
static struct mt9d113_i2c_reg_conf mt9d113_effect_sepia_reg_config[] =
{
    {0x098C, 0x2763},
    {0x0990, 0xCE1A},

    {0x098C, 0x2759},    
    {0x0990, 0x6442},
    {0x098C, 0x275B},
    {0x0990, 0x6442},
    {0x098C, 0xA103},
    {0x0990, 0x0006},
};
static struct mt9d113_i2c_reg_conf mt9d113_effect_aqua_reg_config[] =
{
    {0x098C, 0x2763},
    {0x0990, 0xE2E2},

    {0x098C, 0x2759},    
    {0x0990, 0x6442},
    {0x098C, 0x275B},
    {0x0990, 0x6442},
    {0x098C, 0xA103},
    {0x0990, 0x0006},
};

static struct  mt9d113_work_t *mt9d113sensorw = NULL;

static struct  i2c_client *mt9d113_client = NULL;
static struct mt9d113_ctrl_t *mt9d113_ctrl = NULL;

static uint16_t mt9d113_version_id = HW_MT9D113_VER_3;
static uint16_t mt9d113_mode_id = BYD_MODE_ID;

static DECLARE_WAIT_QUEUE_HEAD(mt9d113_wait_queue);
DECLARE_MUTEX(mt9d113_sem);

static int mt9d113_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr  = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(mt9d113_client->adapter, msgs, 2) < 0) {
		CDBG("mt9d113_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9d113_i2c_read_w(unsigned short raddr, unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = mt9d113_i2c_rxdata(mt9d113_client->addr, buf, 2);
	if (rc < 0)
		return rc;

	*rdata = buf[0] << 8 | buf[1];

	if (rc < 0)
		CDBG("mt9d113_i2c_read failed!\n");

	return rc;
}

static int32_t mt9d113_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
	{
		.addr = saddr,
		.flags = 0,
		.len = length,
		.buf = txdata,
	},
	};

	if (i2c_transfer(mt9d113_client->adapter, msg, 1) < 0) {
		CDBG("mt9d113_i2c_txdata faild\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9d113_i2c_write_w(unsigned short waddr, unsigned short wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];
	int32_t i = 0;

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00)>>8;
	buf[3] = (wdata & 0x00FF);
	
	/*write three times, if error, return -EIO*/
	for(i = 0; i < 10; i++)
	{
	  rc = mt9d113_i2c_txdata(mt9d113_client->addr, buf, 4);
	  if(0 <= rc)
	    return 0;
	}
	if(10 == i)
	{
	  CDBG("i2c_write_w failed, addr = 0x%x, val = 0x%x!\n", waddr, wdata);
	  return -EIO;
	}
	return 0;
}

static int32_t mt9d113_i2c_write_w_table(
	struct mt9d113_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9d113_i2c_write_w(reg_conf_tbl->waddr, reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}

	return rc;
}

int32_t mt9d113_board_set_exposure_mode(void)
{
    int32_t rc = 0;
    if(machine_is_msm7x25_u8150()
        ||machine_is_msm7x25_c8150()
        ||machine_is_msm7x25_c8500()
        )
    {
        /*large exposure*/
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA20C))){return rc;} // MCU_ADDRESS [AE_MAX_INDEX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x000F))){return rc;} // MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA215))){return rc;} // MCU_ADDRESS [AE_INDEX_TH23]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x000F))){return rc;} // MCU_DATA_0
    }
    else
    {
        /*normal exposure*/
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA20C))){return rc;} // MCU_ADDRESS [AE_MAX_INDEX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0008))){return rc;} // MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA215))){return rc;} // MCU_ADDRESS [AE_INDEX_TH23]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0008))){return rc;} // MCU_DATA_0
    }

    return rc;
}

int32_t mt9d113_set_default_focus(uint8_t af_step)
{
    int32_t rc = 0;

    return rc;
}

int32_t mt9d113_set_effect(int32_t effect)
{
	struct mt9d113_i2c_reg_conf const *reg_conf_tbl = NULL;
    int num_of_items_in_table = 0;
	long rc = 0;
    
	switch (effect) {
	case CAMERA_EFFECT_OFF:
        reg_conf_tbl = mt9d113_effect_off_reg_config;
        num_of_items_in_table = MT9D113_ARRAY_SIZE(mt9d113_effect_off_reg_config);
        break;

	case CAMERA_EFFECT_MONO:
        reg_conf_tbl = mt9d113_effect_mono_reg_config;
        num_of_items_in_table = MT9D113_ARRAY_SIZE(mt9d113_effect_mono_reg_config);
		break;

	case CAMERA_EFFECT_NEGATIVE:
        reg_conf_tbl = mt9d113_effect_negative_reg_config;
        num_of_items_in_table = MT9D113_ARRAY_SIZE(mt9d113_effect_negative_reg_config);
		break;

	case CAMERA_EFFECT_SOLARIZE:
        reg_conf_tbl = mt9d113_effect_solarize_reg_config;
        num_of_items_in_table = MT9D113_ARRAY_SIZE(mt9d113_effect_solarize_reg_config);
		break;

	case CAMERA_EFFECT_SEPIA:
        reg_conf_tbl = mt9d113_effect_sepia_reg_config;
        num_of_items_in_table = MT9D113_ARRAY_SIZE(mt9d113_effect_sepia_reg_config);
		break;
        
	case CAMERA_EFFECT_AQUA:
        reg_conf_tbl = mt9d113_effect_aqua_reg_config;
        num_of_items_in_table = MT9D113_ARRAY_SIZE(mt9d113_effect_aqua_reg_config);
		break;
              
	default: 
		return 0;
	}

    rc = mt9d113_i2c_write_w_table(reg_conf_tbl, num_of_items_in_table);
    return rc;

}

static long mt9d113_set_wb(int wb)
{
	long rc = 0;
    if(mt9d113_i2c_read_w(0x31FE, &mt9d113_version_id) <0)
    {
        rc = -ENODEV;
        CDBG("mt9d113_set_wb,mt9d113_version_id fail\n");
        return rc;
    }

	switch (wb) {
	case CAMERA_WB_AUTO:
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xAB22))){return rc;} // MCU_ADDRESS [HG_LL_APCORR1]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0003))){return rc;} // MCU_DATA_0
        if(HW_MT9D113_VER_2 == mt9d113_version_id)
        {
            if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xAB20))){return rc;} // MCU_ADDRESS [HG_LL_SAT1]
            if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0043))){return rc;} // MCU_DATA_0
        }
        else
        {
            if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xAB20 ))){return rc;} 	// MCU_ADDRESS [HG_LL_SAT1]
            if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080 ))){return rc;}	// MCU_DATA_0 0~255,normal use 120,150——————这个寄存器多处用到，请杨海民在每一处都判断V2和V3，是V3就修改成0x0080，具体可以咨询吴晓金
        }
        if(0 > (rc = mt9d113_board_set_exposure_mode())){return rc;}
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0x271F))){return rc;} // MCU_ADDRESS [MODE_SENSOR_FRAME_LENGTH_A]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0293))){return rc;} // MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA11F))){return rc;}// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0001))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA103))){return rc;}// MCU_ADDRESS [SEQ_CMD]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0005))){return rc;}// MCU_DATA_0
        msleep(100);
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA355))){return rc;}// MCU_ADDRESS [AWB_MODE]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x000A))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34A))){return rc;}// MCU_ADDRESS [AWB_GAIN_MIN]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0059))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34B))){return rc;}// MCU_ADDRESS [AWB_GAIN_MAX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x00C8))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34C))){return rc;}// MCU_ADDRESS [AWB_GAINMIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0059))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34D))){return rc;}// MCU_ADDRESS [AWB_GAINMAX_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x00A6))){return rc;}// MCU_DATA_0
        
        break;

	case CAMERA_WB_INCANDESCENT:
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA11F))){return rc;}// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0000))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA103))){return rc;}// MCU_ADDRESS [SEQ_CMD]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0005))){return rc;}// MCU_DATA_0
        msleep(100);
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA353))){return rc;}// MCU_ADDRESS
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0000))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34E))){return rc;}// MCU_ADDRESS [AWB_GAIN_R]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0072))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34F))){return rc;}// MCU_ADDRESS [AWB_GAIN_G]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA350))){return rc;}// MCU_ADDRESS [AWB_GAIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x00A0))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34A))){return rc;}// MCU_ADDRESS [AWB_GAIN_MIN]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0072))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34B))){return rc;}// MCU_ADDRESS [AWB_GAIN_MAX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0072))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34C))){return rc;}// MCU_ADDRESS [AWB_GAINMIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x00A0))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34D))){return rc;}// MCU_ADDRESS [AWB_GAINMAX_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x00A0))){return rc;}// MCU_DATA_0
		break;
        
	case CAMERA_WB_CUSTOM:
	case CAMERA_WB_FLUORESCENT:
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA11F))){return rc;}// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0000))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA103))){return rc;}// MCU_ADDRESS [SEQ_CMD]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0005))){return rc;}// MCU_DATA_0
        msleep(100);
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA353))){return rc;}// MCU_ADDRESS
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0032))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34E))){return rc;}// MCU_ADDRESS [AWB_GAIN_R]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x008C))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34F))){return rc;}// MCU_ADDRESS [AWB_GAIN_G]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA350))){return rc;}// MCU_ADDRESS [AWB_GAIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34A))){return rc;}// MCU_ADDRESS [AWB_GAIN_MIN]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x008C))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34B))){return rc;}// MCU_ADDRESS [AWB_GAIN_MAX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x008C))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34C))){return rc;}// MCU_ADDRESS [AWB_GAINMIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34D))){return rc;}// MCU_ADDRESS [AWB_GAINMAX_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0	
        break;
        
	case CAMERA_WB_DAYLIGHT:
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA11F))){return rc;}// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0000))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA103))){return rc;}// MCU_ADDRESS [SEQ_CMD]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0005))){return rc;}// MCU_DATA_0
        msleep(100);
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA353))){return rc;}// MCU_ADDRESS
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x007F))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34E))){return rc;}// MCU_ADDRESS [AWB_GAIN_R]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x007D))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34F))){return rc;}// MCU_ADDRESS [AWB_GAIN_G]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA350))){return rc;}// MCU_ADDRESS [AWB_GAIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0078))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34A))){return rc;}// MCU_ADDRESS [AWB_GAIN_MIN]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x007D))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34B))){return rc;}// MCU_ADDRESS [AWB_GAIN_MAX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x007D))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34C))){return rc;}// MCU_ADDRESS [AWB_GAINMIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0078))){return rc;} // MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34D))){return rc;}// MCU_ADDRESS [AWB_GAINMAX_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0078))){return rc;}// MCU_DATA_0
        break;
        
	case CAMERA_WB_CLOUDY_DAYLIGHT:
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA11F))){return rc;}// MCU_ADDRESS [SEQ_PREVIEW_1_AWB]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0000))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA103))){return rc;}// MCU_ADDRESS [SEQ_CMD]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0005))){return rc;}// MCU_DATA_0
        msleep(100);
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA353))){return rc;}// MCU_ADDRESS
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x007F))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34E))){return rc;}// MCU_ADDRESS [AWB_GAIN_R]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34F))){return rc;}// MCU_ADDRESS [AWB_GAIN_G]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0081))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA350))){return rc;}// MCU_ADDRESS [AWB_GAIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x006E))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34A))){return rc;}// MCU_ADDRESS [AWB_GAIN_MIN]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34B))){return rc;}// MCU_ADDRESS [AWB_GAIN_MAX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34C))){return rc;}// MCU_ADDRESS [AWB_GAINMIN_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x006E))){return rc;}// MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA34D))){return rc;}// MCU_ADDRESS [AWB_GAINMAX_B]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x006E))){return rc;}// MCU_DATA_0

		break;
        
	case CAMERA_WB_TWILIGHT:
        return 0;
		break;  

	case CAMERA_WB_SHADE:
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xAB22))){return rc;} // MCU_ADDRESS [HG_LL_APCORR1]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0001))){return rc;} // MCU_DATA_0
        if(HW_MT9D113_VER_2 == mt9d113_version_id)
        {
            if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xAB20))){return rc;} // MCU_ADDRESS [HG_LL_SAT1]
            if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x002D))){return rc;} // MCU_DATA_0
        }
        else
        {
            if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xAB20 ))){return rc;} 	// MCU_ADDRESS [HG_LL_SAT1]
            if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0080 ))){return rc;}	// MCU_DATA_0 0~255,normal use 120,150——————这个寄存器多处用到，请杨海民在每一处都判断V2和V3，是V3就修改成0x0080，具体可以咨询吴晓金
        }
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA20C))){return rc;} // MCU_ADDRESS [AE_MAX_INDEX]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0012))){return rc;} // MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA215))){return rc;} // MCU_ADDRESS [AE_INDEX_TH23]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0012))){return rc;} // MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0x271F))){return rc;} // MCU_ADDRESS [MODE_SENSOR_FRAME_LENGTH_A]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0293))){return rc;} // MCU_DATA_0
        if(0 > (rc = mt9d113_i2c_write_w( 0x098C, 0xA103))){return rc;} // MCU_ADDRESS [SEQ_CMD]
        if(0 > (rc = mt9d113_i2c_write_w( 0x0990, 0x0005))){return rc;} // MCU_DATA_0
        
		break;     
        
	default: 
		return 0;
	}

    return rc;

}

int32_t mt9d113_set_fps(struct fps_cfg    *fps)
{
    /* input is new fps in Q8 format */
    int32_t rc = 0;

    CDBG("mt9d113_set_fps\n");
    return rc;
}

int32_t mt9d113_write_exp_gain(uint16_t gain, uint32_t line)
{
    CDBG("mt9d113_write_exp_gain\n");
    return 0;
}

int32_t mt9d113_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
    int32_t rc = 0;

    CDBG("mt9d113_set_pict_exp_gain\n");

    mdelay(10);

    /* camera_timed_wait(snapshot_wait*exposure_ratio); */
    return rc;
}
static int32_t mt9d113_lens_correction(void)
{ 
    int32_t rc = -EFAULT;
    if(mt9d113_i2c_read_w(0x0024, &mt9d113_mode_id) <0)
    {
        CDBG("mt9d113_lens_correction,mt9d113_mode_id fail\n");
        return -ENODEV;
    }
    /* for color distortion;this reg val sometimes error */
	mt9d113_mode_id = mt9d113_mode_id & 0x03;
	
    if(mt9d113_mode_id == BYD_MODE_ID)
    {
        rc = mt9d113_i2c_write_w_table(mt9d113_lens_reg_config_byd,
                                    MT9D113_ARRAY_SIZE(mt9d113_lens_reg_config_byd));
    }
    else if(mt9d113_mode_id == LITEON_MODE_ID)
    {
        rc = mt9d113_i2c_write_w_table(mt9d113_lens_reg_config_liteon,
                                    MT9D113_ARRAY_SIZE(mt9d113_lens_reg_config_liteon));        
    }
    else if(mt9d113_mode_id == SUNNY_MODE_ID)
    {
        rc = mt9d113_i2c_write_w_table(mt9d113_lens_reg_config_sunny,
                                    MT9D113_ARRAY_SIZE(mt9d113_lens_reg_config_sunny));        
    }
    return rc;
}

static int32_t mt9d113_awb_ccm(void)
{ 
    int32_t rc = -EFAULT;
    if(mt9d113_i2c_read_w(0x0024, &mt9d113_mode_id) <0)
    {
        CDBG("mt9d113_awb_ccm,mt9d113_mode_id fail\n");
        return -ENODEV;
    }
    /* for color distortion;this reg val sometimes error */
	mt9d113_mode_id = mt9d113_mode_id & 0x03;


    if(mt9d113_mode_id == BYD_MODE_ID)
    {
        rc = mt9d113_i2c_write_w_table(mt9d113_awb_ccm_reg_config_byd,
                                    MT9D113_ARRAY_SIZE(mt9d113_awb_ccm_reg_config_byd));
    }
    else if(mt9d113_mode_id == LITEON_MODE_ID)
    {
        rc = mt9d113_i2c_write_w_table(mt9d113_awb_ccm_reg_config_liteon,
                                    MT9D113_ARRAY_SIZE(mt9d113_awb_ccm_reg_config_liteon));        
    }
    else if(mt9d113_mode_id == SUNNY_MODE_ID)
    {
        rc = mt9d113_i2c_write_w_table(mt9d113_awb_ccm_reg_config_sunny,
                                    MT9D113_ARRAY_SIZE(mt9d113_awb_ccm_reg_config_sunny));        
    }
    return rc;
}
static int32_t mt9d113_wait(uint16_t reg, uint16_t mode_value)
{
    uint16_t i =100;
    uint16_t mode_data = 0;
    while(i)
    {
        if(mt9d113_i2c_write_w(0x098C, reg) < 0)
        {
            return -EFAULT;
        }
        if(mt9d113_i2c_read_w(0x0990, &mode_data) < 0)
        {
            return -EFAULT;
        }
        if(mode_value == mode_data)
        {
            return 0;
        }
        i--;
        msleep(10);
    }
    CDBG("mt9d113_wait fail,reg=0x%x,mode_value=0x%x,mode_data=0x%x\n",reg,mode_value,mode_data);
    return -EFAULT;

}

static int32_t mt9d113_set_mirror_mode()
{
    int32_t rc = 0;

/* write register for data not mirrored vertically and horizontally */
    static struct mt9d113_i2c_reg_conf mt9d113_mirror_mode_reg_config0[] =
    {
        {0x098C, 0x2717},
        {0x0990, 0x046C},
        {0x098C, 0x272D},
        {0x0990, 0x0024},
    };

/* write register for data mirrored vertically and horizontally */
    static struct mt9d113_i2c_reg_conf mt9d113_mirror_mode_reg_config1[] =
    {
        {0x098C, 0x2717},
        {0x0990, 0x046F},
        {0x098C, 0x272D},
        {0x0990, 0x0027},
    };

    if (machine_is_msm7x25_u8300() || machine_is_msm7x25_u8350() ) {
        rc = mt9d113_i2c_write_w_table(mt9d113_mirror_mode_reg_config0,
            MT9D113_ARRAY_SIZE(mt9d113_mirror_mode_reg_config0));
    } 
    else{
         rc = mt9d113_i2c_write_w_table(mt9d113_mirror_mode_reg_config1,
            MT9D113_ARRAY_SIZE(mt9d113_mirror_mode_reg_config1));
    }

    return rc;
}

int32_t mt9d113_setting(enum mt9d113_reg_update_t rupdate,
                       enum mt9d113_setting_t    rt)
{
    int32_t rc = 0;

    switch (rupdate)
    {
        case UPDATE_PERIODIC:
            if(rt == RES_PREVIEW)
            {
                rc = mt9d113_i2c_write_w_table(mt9d113_svga_config,
                            MT9D113_ARRAY_SIZE(mt9d113_svga_config));
                if(rc < 0)
                {
                    return rc;
                }
                if(mt9d113_wait(0xA104,0x0003) < 0)
                {
                    return -EFAULT;
                }       
				
                rc = mt9d113_set_mirror_mode();
                if(rc < 0)
                {
                    return rc;
                }
            }
            else
            {
                CDBG("mt9d113_uxga_config in\n");
                rc = mt9d113_i2c_write_w_table(mt9d113_uxga_config,
                            MT9D113_ARRAY_SIZE(mt9d113_uxga_config));
                if(rc < 0)
                {
                    CDBG("mt9d113_uxga_config fail\n");
                    return rc;
                }
                 if(mt9d113_wait(0xA104,0x0007) < 0)
                {
                    CDBG("mt9d113_wait(0xA104,0x0007) fail\n");
                    return -EFAULT;
                }                
            }
            break;

        case REG_INIT:
            
            if(mt9d113_i2c_write_w(0x0018, 0x4028) < 0)
            {
                return -EFAULT;
            }
 
            if(mt9d113_wait(0xA104,0x0003) < 0)
            {
                return -EFAULT;
            }

    		if (machine_is_msm7x25_u8300() || machine_is_msm7x25_u8350() ) {
                rc = mt9d113_i2c_write_w_table(mt9d113_init_reg_config_comm_u8300,
                    MT9D113_ARRAY_SIZE(mt9d113_init_reg_config_comm_u8300));
            } 
            else{
                rc = mt9d113_i2c_write_w_table(mt9d113_init_reg_config_comm,
                    MT9D113_ARRAY_SIZE(mt9d113_init_reg_config_comm));
            }
        
            if(rc < 0)
            {
                return rc;
            }
            rc = mt9d113_board_set_exposure_mode();
            if(rc < 0)
            {
                return rc;
            }
            msleep(10);
            
            if(mt9d113_i2c_read_w(0x31FE, &mt9d113_version_id) <0)
            {
                rc = -ENODEV;
                CDBG("mt9d113_setting,mt9d113_version_id fail\n");
                break;

            }
            if(HW_MT9D113_VER_2 == mt9d113_version_id)
            {
                rc = mt9d113_i2c_write_w_table(mt9d113_init_reg_config_ver2,
                            MT9D113_ARRAY_SIZE(mt9d113_init_reg_config_ver2));

            }
            else
            {
                rc = mt9d113_i2c_write_w_table(mt9d113_init_reg_config_no_ver2,
                            MT9D113_ARRAY_SIZE(mt9d113_init_reg_config_no_ver2));
            }
            
            if(rc < 0)
            {
                return rc;
            }
            if (mt9d113_lens_correction() < 0)   
            {                                                      
              CDBG("mt9d113_setting,mt9d113_lens_correction fail\n");
              return -EFAULT;                                        
            }                                                      
                                                                   
            if (mt9d113_awb_ccm() < 0)           
            {                                                      
              CDBG("mt9d113_setting,mt9d113_awb_ccm fail\n");
              return -EFAULT;                                        
            }                                                      

            msleep(10);                                                                           
            /* REFRESH regster and make new setting take affect*/                                            
            if(mt9d113_i2c_write_w(0x098C, 0xA103) < 0)   
            {
                return -EFAULT;
            }    
            if(mt9d113_i2c_write_w(0x0990, 0x0006) < 0)
            {
                return -EFAULT;
            }
            
            if(mt9d113_wait(0xA103,0x0000) < 0)
            {
                return -EFAULT;
            }
            
            if(mt9d113_i2c_write_w(0x098C, 0xA103) < 0)
            {
                return -EFAULT;
            }  
            if(mt9d113_i2c_write_w(0x0990, 0x0005) < 0)
            {
                return -EFAULT;
            }                                                                   
                                                                           
            if(mt9d113_wait(0xA103,0x0000) < 0)
            {
                return -EFAULT;
            }

            break;

        default:
            rc = -EFAULT;
            break;
    } /* switch (rupdate) */

    return rc;
}

int32_t mt9d113_video_config(int mode, int res)
{
    int32_t rc;

    switch (res)
    {
        case QTR_SIZE:
            rc = mt9d113_setting(UPDATE_PERIODIC, RES_PREVIEW);
            if (rc < 0)
            {
                return rc;
            }

            CDBG("sensor configuration done!\n");
            break;

        case FULL_SIZE:
            rc = mt9d113_setting(UPDATE_PERIODIC, RES_CAPTURE);
            if (rc < 0)
            {
                return rc;
            }

            break;

        default:
            return 0;
    } /* switch */

    mt9d113_ctrl->prev_res   = res;
    mt9d113_ctrl->curr_res   = res;
    mt9d113_ctrl->sensormode = mode;

    return rc;
}

int32_t mt9d113_snapshot_config(int mode)
{
    int32_t rc = 0;
    CDBG("mt9d113_snapshot_config in\n");
    rc = mt9d113_setting(UPDATE_PERIODIC, RES_CAPTURE);
    msleep(50);
    if (rc < 0)
    {
        return rc;
    }

    mt9d113_ctrl->curr_res = mt9d113_ctrl->pict_res;

    mt9d113_ctrl->sensormode = mode;

    return rc;
}

int32_t mt9d113_power_down(void)
{
    int32_t rc = 0;

    mdelay(5);

    return rc;
}

int32_t mt9d113_move_focus(int direction, int32_t num_steps)
{
    return 0;
}

static int mt9d113_sensor_init_done(const struct msm_camera_sensor_info *data)
{
    gpio_direction_output(data->sensor_reset, 0);
    gpio_free(data->sensor_reset);

	gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
    msleep(100);
    if (data->vreg_disable_func)
    {
        data->vreg_disable_func(data->sensor_vreg, data->vreg_num);
    }
    
	return 0;
}

static int mt9d113_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;
	unsigned short chipid;

	/* pull down power down */
	rc = gpio_request(data->sensor_pwd, "mt9d113");
	if (!rc || rc == -EBUSY)
		gpio_direction_output(data->sensor_pwd, 1);
	else 
        goto init_probe_fail;
    
	rc = gpio_request(data->sensor_reset, "mt9d113");
	if (!rc) {
		rc = gpio_direction_output(data->sensor_reset, 0);
	}
	else
		goto init_probe_fail;

    mdelay(5);
    
    if (data->vreg_enable_func)
    {
        rc = data->vreg_enable_func(data->sensor_vreg, data->vreg_num);
        if (rc < 0)
        {
            goto init_probe_fail;
        }
    }
    
    msleep(20);
    
    if(data->master_init_control_slave == NULL 
        || data->master_init_control_slave(data) != 0
        )
    {

        rc = gpio_direction_output(data->sensor_pwd, 0);
         if (rc < 0)
            goto init_probe_fail;

        msleep(20);
        /*hardware reset*/
        rc = gpio_direction_output(data->sensor_reset, 1);
        if (rc < 0)
            goto init_probe_fail;

        msleep(20);
    }

	/* 3. Read sensor Model ID: */
	rc = mt9d113_i2c_read_w(MT9D113_REG_CHIP_ID, &chipid);

	if (rc < 0)
		goto init_probe_fail;

	CDBG("mt9d113 chipid = 0x%x\n", chipid);

	/* 4. Compare sensor ID to MT9T012VC ID: */
	if (chipid != MT9D113_CHIP_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}
#if 0
    if(mt9d113_i2c_read_w(0x31FE, &mt9d113_version_id) <0)
    {
        rc = -ENODEV;
        goto init_probe_fail;
    }
    mt9d113_version_id = mt9d113_version_id & 0x3;
    CDBG("mt9d113_version_id = 0x%x\n", mt9d113_version_id);
    
    if(mt9d113_i2c_read_w(0x0024, &mt9d113_mode_id) <0)
    {
        rc = -ENODEV;
        goto init_probe_fail;
    }
    mt9d113_mode_id = mt9d113_mode_id & 0x3;
    CDBG("mt9d113_mode_id = 0x%x\n", mt9d113_mode_id);
    if(mt9d113_mode_id > BYD_MODE_ID)
    {
        mt9d113_mode_id = BYD_MODE_ID;
    }
#endif
    goto init_probe_done;

init_probe_fail:
    mt9d113_sensor_init_done(data);
init_probe_done:
	return rc;
}

int mt9d113_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	mt9d113_ctrl = kzalloc(sizeof(struct mt9d113_ctrl_t), GFP_KERNEL);
	if (!mt9d113_ctrl) {
		CDBG("mt9d113_sensor_open_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	mt9d113_ctrl->fps_divider = 1 * 0x00000400;
	mt9d113_ctrl->pict_fps_divider = 1 * 0x00000400;
	mt9d113_ctrl->set_test = TEST_OFF;
	mt9d113_ctrl->prev_res = QTR_SIZE;
	mt9d113_ctrl->pict_res = FULL_SIZE;

	if (data)
		mt9d113_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(MT9D113_DEFAULT_CLOCK_RATE);
	mdelay(5);

	msm_camio_camif_pad_reg_reset();
	mdelay(5);

  rc = mt9d113_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	if (mt9d113_ctrl->prev_res == QTR_SIZE)
		rc = mt9d113_setting(REG_INIT, RES_PREVIEW);
	else
		rc = mt9d113_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0)
		goto init_fail;
	else
		goto init_done;

init_fail:
	kfree(mt9d113_ctrl);
init_done:
	return rc;
}

int mt9d113_init_client(struct i2c_client *client)
{
    /* Initialize the MSM_CAMI2C Chip */
    init_waitqueue_head(&mt9d113_wait_queue);
    return 0;
}

int32_t mt9d113_set_sensor_mode(int mode, int res)
{
    int32_t rc = 0;

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
            CDBG("SENSOR_PREVIEW_MODE\n");
            rc = mt9d113_video_config(mode, res);
            break;

        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
            CDBG("SENSOR_SNAPSHOT_MODE\n");
            rc = mt9d113_snapshot_config(mode);
            break;

        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}


int mt9d113_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	down(&mt9d113_sem);

  CDBG("mt9d113_sensor_config: cfgtype = %d\n",
	  cdata.cfgtype);
		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
			break;

		case CFG_GET_PREV_L_PF:
			break;

		case CFG_GET_PREV_P_PL:
			break;

		case CFG_GET_PICT_L_PF:
			break;

		case CFG_GET_PICT_P_PL:
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = mt9d113_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				mt9d113_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				mt9d113_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = mt9d113_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = mt9d113_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				mt9d113_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				mt9d113_set_default_focus(
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_EFFECT:
			rc = mt9d113_set_effect(
						cdata.cfg.effect);
			break;

		case CFG_SET_WB:
			rc = mt9d113_set_wb(
						cdata.cfg.effect);
			break;

		case CFG_MAX:
            if (copy_to_user((void *)(cdata.cfg.pict_max_exp_lc),
            		mt9d113_supported_effect,
            		MT9D113_ARRAY_SIZE(mt9d113_supported_effect)))
            {
                CDBG("copy mt9d113_supported_effect to user fail\n");
                rc = -EFAULT;
            }
            else
            {
                rc = 0;
            }
			break;
	
		default:
			rc = -EFAULT;
			break;
		}

	up(&mt9d113_sem);

	return rc;
}

int mt9d113_sensor_release(void)
{
	int rc = -EBADF;

	down(&mt9d113_sem);

	mt9d113_power_down();

    mt9d113_sensor_init_done(mt9d113_ctrl->sensordata);

	kfree(mt9d113_ctrl);

	up(&mt9d113_sem);
	CDBG("mt9d113_release completed!\n");
	return rc;
}

static int mt9d113_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9d113sensorw =
		kzalloc(sizeof(struct mt9d113_work_t), GFP_KERNEL);

	if (!mt9d113sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9d113sensorw);
	mt9d113_init_client(client);
	mt9d113_client = client;
	//mt9d113_client->addr = mt9d113_client->addr >> 1;
	msleep(50);

	CDBG("i2c probe ok\n");
	return 0;

probe_failure:
	kfree(mt9d113sensorw);
	mt9d113sensorw = NULL;
	pr_err("i2c probe failure %d\n", rc);
	return rc;
}

static const struct i2c_device_id mt9d113_i2c_id[] = {
	{ "mt9d113", 0},
	{ }
};

static struct i2c_driver mt9d113_i2c_driver = {
	.id_table = mt9d113_i2c_id,
	.probe  = mt9d113_i2c_probe,
	.remove = __exit_p(mt9d113_i2c_remove),
	.driver = {
		.name = "mt9d113",
	},
};

static int mt9d113_sensor_probe(
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	/* We expect this driver to match with the i2c device registered
	 * in the board file immediately. */
	int rc = i2c_add_driver(&mt9d113_i2c_driver);
	if (rc < 0 || mt9d113_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(MT9D113_DEFAULT_CLOCK_RATE);
	mdelay(5);

	rc = mt9d113_probe_init_sensor(info);
	if (rc < 0) {
		i2c_del_driver(&mt9d113_i2c_driver);
		goto probe_done;
	}

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CAMERA_MAIN);
    #endif

	s->s_init = mt9d113_sensor_open_init;
	s->s_release = mt9d113_sensor_release;
	s->s_config  = mt9d113_sensor_config;
	mt9d113_sensor_init_done(info);

	set_camera_support(true);
probe_done:
	return rc;
}

static int __mt9d113_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, mt9d113_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9d113_probe,
	.driver = {
		.name = "msm_camera_mt9d113",
		.owner = THIS_MODULE,
	},
};

static int __init mt9d113_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9d113_init);
