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
#include "s5k5ca.h"
#include "linux/hardware_self_adapt.h"
#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif


#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "s5k5ca.c: " fmt, ## args)

/*=============================================================
    SENSOR REGISTER DEFINES
==============================================================*/
#define S5K5CA_CHIP_ID 0x05ca

enum s5k5ca_test_mode_t
{
    TEST_OFF,
    TEST_1,
    TEST_2,
    TEST_3
};

enum s5k5ca_resolution_t
{
    QTR_SIZE,
    FULL_SIZE,
    INVALID_SIZE
};

enum s5k5ca_reg_update_t
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

enum s5k5ca_setting_t
{
    RES_PREVIEW,
    RES_CAPTURE
};

/*
 * Time in milisecs for waiting for the sensor to reset.
 */
#define S5K5CA_RESET_DELAY_MSECS 66

/* for 30 fps preview */
#define S5K5CA_DEFAULT_CLOCK_RATE 24500000

#define S5K5CA_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
/* FIXME: Changes from here */
struct s5k5ca_work_t
{
    struct work_struct work;
};

struct s5k5ca_ctrl_t
{
    const struct  msm_camera_sensor_info *sensordata;

    int sensormode;
    uint32_t           fps_divider; /* init to 1 * 0x00000400 */
    uint32_t           pict_fps_divider; /* init to 1 * 0x00000400 */

    uint16_t curr_lens_pos;
    uint16_t init_curr_lens_pos;
    uint16_t my_reg_gain;
    uint32_t my_reg_line_count;

    enum s5k5ca_resolution_t prev_res;
    enum s5k5ca_resolution_t pict_res;
    enum s5k5ca_resolution_t curr_res;
    enum s5k5ca_test_mode_t  set_test;

    unsigned short imgaddr;
};

struct s5k5ca_i2c_reg_conf
{
	unsigned short waddr;
	unsigned short wdata;
};
typedef enum
{
  CAMERA_WB_MIN_MINUS_1,
  CAMERA_WB_AUTO = 1,  /* This list must match aeecamera.h */
  CAMERA_WB_CUSTOM,
  CAMERA_WB_INCANDESCENT,
  CAMERA_WB_FLUORESCENT,
  CAMERA_WB_DAYLIGHT,
  CAMERA_WB_CLOUDY_DAYLIGHT,
  CAMERA_WB_TWILIGHT,
  CAMERA_WB_SHADE,
  CAMERA_WB_MAX_PLUS_1
} config3a_wb_t;

typedef enum
{
  CAMERA_ANTIBANDING_OFF,
  CAMERA_ANTIBANDING_60HZ,
  CAMERA_ANTIBANDING_50HZ,
  CAMERA_ANTIBANDING_AUTO,
  CAMERA_MAX_ANTIBANDING,
} camera_antibanding_type;
typedef enum
{
  CAMERA_AF_OFF,
  CAMERA_AF_ON,
} camera_af_mode;


const static char s5k5ca_supported_effect[] = "none,mono,negative,sepia,aqua";

#define MODEL_LITEON 0
#define MODEL_BYD 1
#define MODEL_SUNNY 2
#define MODEL_FOXCOM 3
#define MODEL_AF_LITEON 4
/* add three FPC packing models */
#define MODEL_LITEON_FPC 8
#define MODEL_SUNNY_FPC 10
#define MODEL_FOXCOM_FPC 11
static uint16_t s5k5ca_model_id = MODEL_AF_LITEON;
static camera_af_mode s5k5ca_af_mode = CAMERA_AF_ON;
static struct s5k5ca_i2c_reg_conf * p_s5k5ca_init_reg_config;
static unsigned int reg_num;
const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_liteon[] =
{ 
    #include "s5k5ca_liteon.h"
};


const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_liteon_FPC_1[] =
{
    //=================================================================================================  
	// * Name: S5K5CAGX EVT1.0 Initial Setfile                                                           
	// * important : TnP and Analog Setting Version is 091216                                            
	// * PLL mode: MCLK=24.5MHz / SYSCLK=40MHz / PCLK=60MHz                                              
	// * FPS : Preview YUV QVGA 22-10fps / Capture YUV QXGA 7.5fps                                       
	//=================================================================================================  
	////Ver 0.1 Nnalog Settings for 5CA EVT0 30FPS in Binning, including latest TnP for EVT1             
	////20091310 ded analog settings for 430LSB, long exposure mode only. Settings are for 32MHz Sys. CLK
	////20091102 ded all calibration data, final settings for STW EVT1 module, SCLK 32MHz, PCLK 60 MHz.  
	////20091104 anged the shading alpha &Near off                                                       
	////20091104 anged awbb_GridEnable from 0001h to 0002h	//awbb_GridEnable                            
	////aetarget4, gamma change for improving contrast                                                   
	////20100113 w TnP updated//                                                                         
	{0xFCFC,0xD000},	//Reset                                    
	{0x0010,0x0001}, //Clear host interrupt so main will wait 
	{0x1030,0x0000}, //ARM go                                 
	{0x0014,0x0001}, //Wait100mSec                            
	//p100	// Wait100mSec
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_liteon_FPC_2[] =
{ 
    //Auto Flicker 60Hz Start                                                                          
	{0x0028,0x7000},                                                                                           
	{0x002A,0x0C18},  //#AFC_Default60Hz                                                                       
	{0x0F12,0x0001},  // #AFC_Default60Hz  1: Auto Flicker 60Hz start 0: Auto Flicker 50Hz start               
	{0x002A,0x04D2},  // #REG_TC_DBG_AutoAlgEnBits                                                             
	{0x0F12,0x067F},                                                                                           
	
	//================================================================================================
	// SET PREVIEW CONFIGURATION_0
	// # Foramt : YUV422
	// # Size: 1024*768
	// # FPS :10-15fps(using normal_mode preview)
	//================================================================================================
	{0x002A,0x026C}, 
	{0x0F12,0x0400}, //0800//#REG_0TC_PCFG_usWidth 
	{0x0F12,0x0300}, //0600 //#REG_0TC_PCFG_usHeight 
	{0x0F12,0x0005}, //#REG_0TC_PCFG_Format            
	{0x0F12,0x3AA8},  //#REG_0TC_PCFG_usMaxOut4KHzRate  
	{0x0F12,0x3A88},  //#REG_0TC_PCFG_usMinOut4KHzRate  
	{0x002A,0x027A}, 
	{0x0F12,0x0052},  //#REG_0TC_PCFG_PVIMask //s0050 = FALSE in MSM6290 : s0052 = TRUE in MSM6800 //reg 027A
	{0x002A,0x0282}, 
	{0x0F12,0x0000},  //#REG_0TC_PCFG_uClockInd
	{0x0F12,0x0000},  //#REG_0TC_PCFG_usFrTimeType
	{0x0F12,0x0001},  //#REG_0TC_PCFG_FrRateQualityType
	{0x0F12,0x03E8},   //#REG_0TC_PCFG_usMaxFrTimeMsecMult10 //10fps
	{0x0F12,0x029A},   //#REG_0TC_PCFG_usMinFrTimeMsecMult10 //15fps
    {0x0028,0x7000},
    {0x002A,0x0296},
    {0x0F12,0x0003}, //#REG_0TC_PCFG_uPrevMirror
    {0x0F12,0x0003}, //#REG_0TC_PCFG_uCaptureMirror
	                                                                                                    
	//================================================================================================                                                                                          
	// SET Capture CONFIGURATION_0                                                                                                                                                              
	// # Foramt : YUV422                                                                                                                                                                        
	// # Size: QXGA,                                         
	// # FPS :7.5fps                                                
	//================================================================================================  
	{0x002A,0x035E},                                                     
	{0x0F12,0x0800},  //#REG_0TC_CCFG_usWidth                            
	{0x0F12,0x0600},  //#REG_0TC_CCFG_usHeight                           
	{0x0F12,0x0005},  //#REG_0TC_CCFG_Format//5:YUV9:JPEG                
	{0x0F12,0x3AA8}, //#REG_0TC_CCFG_usMaxOut4KHzRate       
	{0x0F12,0x3A88}, //#REG_0TC_CCFG_usMinOut4KHzRate       
	{0x002A,0x036C},                                                     
	{0x0F12,0x0052},  //#REG_0TC_CCFG_PVIMask                            
	{0x002A,0x0374},                                                     
	{0x0F12,0x0000}, //#REG_0TC_CCFG_uClockInd                          
	{0x0F12,0x0000},  //#REG_0TC_CCFG_usFrTimeType                       
	{0x0F12,0x0002},  //#REG_0TC_CCFG_FrRateQualityType                  
	{0x0F12,0x0535}, //0535//#REG_0TC_CCFG_usMaxFrTimeMsecMult10 //5fps  
	{0x0F12,0x0535},  //#REG_0TC_CCFG_usMinFrTimeMsecMult10 //6.5fps     
	//================================================================================================
	// APPLY PREVIEW CONFIGURATION & RUN PREVIEW 
	//================================================================================================
	{0x002A,0x023C},                                                                                                                                                                                                      
	{0x0F12,0x0000}, // #REG_TC_GP_ActivePrevConfig // Select preview configuration_0                                                                                                                                     
	{0x002A,0x0240},                                                                                                                                                                                                      
	{0x0F12,0x0001},  // #REG_TC_GP_PrevOpenAfterChange                                                                                                                                                                   
	{0x002A,0x0230},                                                                                                                                                                                                      
	{0x0F12,0x0001},  // #REG_TC_GP_NewConfigSync // Update preview configuration                                                                                                                                         
	{0x002A,0x023E},                                                                                                                                                                                                      
	{0x0F12,0x0001},  // #REG_TC_GP_PrevConfigChanged                                                                                                                                                                     
	{0x002A,0x0220},                                                                                                                                                                                                      
	{0x0F12,0x0001},  // #REG_TC_GP_EnablePreview // Start preview                                                                                                                                                        
	{0x0F12,0x0001},  // #REG_TC_GP_EnablePreviewChanged                                                                                                                                                                  
	//================================================================================================                                                                                                                     
	{0x0028,0xD000}, 	     
	{0x002A,0x1000}, 	  // Set host interrupt so main start run          
	{0x0F12,0x0001}, 	     
	//p10   // Wait10mSec 
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_liteon_FPC[] =
{
    #include "s5k5ca_liteon_FPC.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_sunny_FPC_1[] =
{    
    //#ARM GO  
    //#Direct mode  
    {0xFCFC,0xD000},    
    {0x0010,0x0001},//Reset 
    {0x1030,0x0000},//Clear host interrupt so main will wait    
    {0x0014,0x0001},//ARM go    
    //p100 //delay 100ms
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_sunny_FPC_2[] =
{     
    //================================================================================================	
    //#SET PREVIEW CONFIGURATION_0	
    //## Foramt : YUV422	
    //## Size: 1024*768	
    //## FPS : 10-15fps	
    //================================================================================================	
    {0x002A,0x026C},	
    {0x0F12,0x0400}, //#REG_0TC_PCFG_usWidth//1024	
    {0x0F12,0x0300}, //#REG_0TC_PCFG_usHeight//768	
    {0x0F12,0x0005}, //#REG_0TC_PCFG_Format	
    {0x0F12,0x3AA8}, //#REG_0TC_PCFG_usMaxOut4KHzRate	
    {0x0F12,0x3A88}, //#REG_0TC_PCFG_usMinOut4KHzRate	
    {0x0F12,0x0100}, //#REG_0TC_PCFG_OutClkPerPix88	
    {0x0F12,0x0800}, //#REG_0TC_PCFG_uMaxBpp88	
    {0x0F12,0x0052}, //#REG_0TC_PCFG_PVIMask //s0050 = FALSE in MSM6290 : s0052 = TRUE in MSM6800 //reg	
    {0x0F12,0x4000}, //#REG_0TC_PCFG_OIFMask	
    {0x0F12,0x01E0}, //#REG_0TC_PCFG_usJpegPacketSize	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_usJpegTotalPackets	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_uClockInd	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_usFrTimeType	
    {0x0F12,0x0001}, //#REG_0TC_PCFG_FrRateQualityType	
    {0x0F12,0x03E8}, //#REG_0TC_PCFG_usMaxFrTimeMsecMult10 //10fps	
    {0x0F12,0x029A}, //#REG_0TC_PCFG_usMinFrTimeMsecMult10 //22fps	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_bSmearOutput	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_sSaturation	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_sSharpBlur	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_sColorTemp	
    {0x0F12,0x0000}, //#REG_0TC_PCFG_uDeviceGammaIndex	
	{0x0F12,0x0003}, //#REG_0TC_PCFG_uPrevMirror
	{0x0F12,0x0003}, //#REG_0TC_PCFG_uCaptureMirror
    {0x0F12,0x0000}, //#REG_0TC_PCFG_uRotation		
    //================================================================================================	
    //#APPLY PREVIEW CONFIGURATION & RUN PREVIEW	
    //================================================================================================	
    {0x002A,0x023C},	
    {0x0F12,0x0000}, //#REG_TC_GP_ActivePrevConfig //#Select preview configuration_0	
    {0x002A,0x0240},	
    {0x0F12,0x0001}, //#REG_TC_GP_PrevOpenAfterChange	
    {0x002A,0x0230},	
    {0x0F12,0x0001}, //#REG_TC_GP_NewConfigSync //#Update preview configuration	
    {0x002A,0x023E},	
    {0x0F12,0x0001}, //#REG_TC_GP_PrevConfigChanged	
    {0x002A,0x0220},	
    {0x0F12,0x0001}, //#REG_TC_GP_EnablePreview //#Start preview	
    {0x0F12,0x0001}, //#REG_TC_GP_EnablePreviewChanged	
    //p100 //delay 100ms
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_sunny_FPC_3[] =
{ 
    //================================================================================================
	//#SET CAPTURE CONFIGURATION_0
	//## Foramt :YUV
	//## Size: QXGA
	//## FPS : 7.5fps
	//================================================================================================
	{0x002A,0x035C}, 
	{0x0F12,0x0000}, //#REG_0TC_CCFG_uCaptureModeJpEG
	{0x0F12,0x0800}, //#REG_0TC_CCFG_usWidth 
	{0x0F12,0x0600}, //#REG_0TC_CCFG_usHeight
	{0x0F12,0x0005}, //#REG_0TC_CCFG_Format//5:YUV9:JPEG 
	{0x0F12,0x3AA8}, //#REG_0TC_CCFG_usMaxOut4KHzRate
	{0x0F12,0x3A88}, //#REG_0TC_CCFG_usMinOut4KHzRate
	{0x0F12,0x0100}, //#REG_0TC_CCFG_OutClkPerPix88
	{0x0F12,0x0800}, //#REG_0TC_CCFG_uMaxBpp88 
	{0x0F12,0x0052}, //#REG_0TC_CCFG_PVIMask 
	{0x0F12,0x0050}, //#REG_0TC_CCFG_OIFMask 
	{0x0F12,0x03C0}, //#REG_0TC_CCFG_usJpegPacketSize
	{0x0F12,0x0000}, //#REG_0TC_CCFG_usJpegTotalPackets
	{0x0F12,0x0000}, //#REG_0TC_CCFG_uClockInd 
	{0x0F12,0x0000}, //#REG_0TC_CCFG_usFrTimeType
	{0x0F12,0x0002}, //#REG_0TC_CCFG_FrRateQualityType 
	{0x0F12,0x0535}, //#REG_0TC_CCFG_usMaxFrTimeMsecMult10 //7.5fps
	{0x0F12,0x0535}, //#REG_0TC_CCFG_usMinFrTimeMsecMult10 //7.5fps 
	{0x0F12,0x0000}, //#REG_0TC_CCFG_bSmearOutput
	{0x0F12,0x0000}, //#REG_0TC_CCFG_sSaturation 
	{0x0F12,0x0000}, //#REG_0TC_CCFG_sSharpBlur
	{0x0F12,0x0000}, //#REG_0TC_CCFG_sColorTemp
	{0x0F12,0x0000}, //#REG_0TC_CCFG_uDeviceGammaIndex 
	
	/*delete some lines*/
	{0x0028,0xD000}, 
	{0x002A,0x1000}, 
	{0x0F12,0x0001}, 
	
	{0x0028,0x7000},

};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_sunny_FPC[] =
{     
    #include "s5k5ca_sunny_FPC.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_sunny[] =
{ 
    #include "s5k5ca_sunny.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_foxcom[] =
{ 
    #include "s5k5ca_foxcom.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_foxcom_FPC[] =
{ 
    #include "s5k5ca_foxcom_FPC.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config1_liteon_AF[] =
{
    //=================================================================================================  
    // * Name: S5K5CAGX EVT1.0 Initial Setfile                                                           
    // * important : TnP and Analog Setting Version is 091216                                            
    // * PLL mode: MCLK=24.5MHz / SYSCLK=40MHz / PCLK=60MHz                                              
    // * FPS : Preview YUV QVGA 22-10fps / Capture YUV QXGA 7.5fps                                       
    //=================================================================================================  
    {0x0010,0x0001},	// Reset                 
    {0x1030,0x0000},	// Clear host interrupt so main will wait
    {0x0014,0x0001},	// ARM go           
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config2_liteon_AF[] =
{
     #include "s5k5ca_liteon_AF.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config3_liteon_AF[] =
{
    //Auto Flicker 60Hz Start                                                                          
    {0x0028, 0x7000},                                                                                           
    {0x002A, 0x0C18},  //#AFC_Default60Hz                                                                       
    {0x0F12, 0x0001},  // #AFC_Default60Hz  1: Auto Flicker 60Hz start 0: Auto Flicker 50Hz start               
    {0x002A, 0x04D2},  // #REG_TC_DBG_AutoAlgEnBits                                                             
    {0x0F12, 0x067F},                                                                                           

    //================================================================================================
    // SET PREVIEW CONFIGURATION_0
    // # Foramt : YUV422
    // # Size: 1024*768
    // # FPS :10-15fps(using normal_mode preview)
    //================================================================================================
    {0x002A, 0x026C},                              
    {0x0F12, 0x0400},//0800//#REG_0TC_PCFG_usWidth 
    {0x0F12, 0x0300},//0600 //#REG_0TC_PCFG_usHeight 
    {0x0F12, 0x0005}, //#REG_0TC_PCFG_Format            
    {0x0F12, 0x3AA8}, //#REG_0TC_PCFG_usMaxOut4KHzRate  
    {0x0F12, 0x3A88}, //#REG_0TC_PCFG_usMinOut4KHzRate  
    {0x002A, 0x027A},                              
    {0x0F12, 0x0052}, //#REG_0TC_PCFG_PVIMask //s0050 = FALSE in MSM6290 : s0052 = TRUE in MSM6800 //reg 027A
    {0x002A, 0x0282},                              
    {0x0F12, 0x0000}, //#REG_0TC_PCFG_uClockInd    
    {0x0F12, 0x0000}, //#REG_0TC_PCFG_usFrTimeType 
    {0x0F12, 0x0001}, //#REG_0TC_PCFG_FrRateQualityType
    {0x0F12, 0x03E8},  //#REG_0TC_PCFG_usMaxFrTimeMsecMult10 //10fps
    {0x0F12, 0x029A},  //#REG_0TC_PCFG_usMinFrTimeMsecMult10 //15fps
    {0x002A, 0x0296},
    {0x0F12, 0x0003},  //REG_0TC_PCFG_uPrevMirror
    {0x0F12, 0x0003},  //REG_0TC_PCFG_uCaptureMirror      

                                                                                                        
    //================================================================================================                                                                                          
    // SET Capture CONFIGURATION_0                                                                                                                                                              
    // # Foramt : YUV422                                                                                                                                                                        
    // # Size: QXGA,                                         
    // # FPS :7.5fps                                                
    //================================================================================================  
    {0x002A, 0x035E},                                                    
    {0x0F12, 0x0800}, //#REG_0TC_CCFG_usWidth                            
    {0x0F12, 0x0600}, //#REG_0TC_CCFG_usHeight                           
    {0x0F12, 0x0005}, //#REG_0TC_CCFG_Format//5:YUV9:JPEG                
    {0x0F12, 0x3AA8},//#REG_0TC_CCFG_usMaxOut4KHzRate       
    {0x0F12, 0x3A88},//#REG_0TC_CCFG_usMinOut4KHzRate       
    {0x002A, 0x036C},                                                    
    {0x0F12, 0x0052}, //#REG_0TC_CCFG_PVIMask                            
    {0x002A, 0x0374},                                                    
    {0x0F12, 0x0000},//#REG_0TC_CCFG_uClockInd                          
    {0x0F12, 0x0000}, //#REG_0TC_CCFG_usFrTimeType                       
    {0x0F12, 0x0002}, //#REG_0TC_CCFG_FrRateQualityType                  
    {0x0F12, 0x0535},//0535//#REG_0TC_CCFG_usMaxFrTimeMsecMult10 //5fps  
    {0x0F12, 0x0535}, //#REG_0TC_CCFG_usMinFrTimeMsecMult10 //6.5fps     
    //================================================================================================
    // APPLY PREVIEW CONFIGURATION & RUN PREVIEW 
    //================================================================================================
    {0x002A, 0x023C},                                                                                                                                                                                                     
    {0x0F12, 0x0000},// #REG_TC_GP_ActivePrevConfig // Select preview configuration_0                                                                                                                                     
    {0x002A, 0x0240},                                                                                                                                                                                                     
    {0x0F12, 0x0001}, // #REG_TC_GP_PrevOpenAfterChange                                                                                                                                                                   
    {0x002A, 0x0230},                                                                                                                                                                                                     
    {0x0F12, 0x0001}, // #REG_TC_GP_NewConfigSync // Update preview configuration                                                                                                                                         
    {0x002A, 0x023E},                                                                                                                                                                                                     
    {0x0F12, 0x0001}, // #REG_TC_GP_PrevConfigChanged                                                                                                                                                                     
    {0x002A, 0x0220},                                                                                                                                                                                                     
    {0x0F12, 0x0001}, // #REG_TC_GP_EnablePreview // Start preview                                                                                                                                                        
    {0x0F12, 0x0001}, // #REG_TC_GP_EnablePreviewChanged                                                                                                                                                                  
    //================================================================================================                                                                                                                     
    {0x0028, 0xD000},                              
    {0x002A, 0x1000},  // Set host interrupt so main start run          
    {0x0F12, 0x0001},                              
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_liteon_and_byd_u8300[] =
{
    #include "s5k5ca_liteon_and_byd_u8300.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_sunny_u8300[] =
{
    #include "s5k5ca_sunny_u8300.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_init_reg_config_foxcom_u8300[] =
{
    #include "s5k5ca_foxcom_u8300.h"
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_before_preview_reg_config[] =
{
{0x0028, 0x7000},                                                                    
{0x002A, 0x1060}, //af_pos_usHomePos                                                 
{0x0F12, 0x0008}, //define the lens position for preview (0000-00FF).                
{0x0F12, 0x0008}, //af_pos_usLowConfPos
{0x002A, 0x0252},                                                                    
{0x0F12, 0x0003},    
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_preview_reg_config[] =
{

//================================================================================================
// APPLY PREVIEW CONFIGURATION & RUN PREVIEW
//================================================================================================
{0x0028, 0x7000},
{0x002A, 0x023C},
{0x0F12, 0x0000}, // #REG_TC_GP_ActivePrevConfig // Select preview configuration_0
{0x002A, 0x0240},
{0x0F12, 0x0001}, // #REG_TC_GP_PrevOpenAfterChange
{0x002A, 0x0230},
{0x0F12, 0x0001}, // #REG_TC_GP_NewConfigSync // Update preview configuration
{0x002A, 0x023E},
{0x0F12, 0x0001}, // #REG_TC_GP_PrevConfigChanged
{0x002A, 0x0220},
{0x0F12, 0x0001}, // #REG_TC_GP_EnablePreview // Start preview
{0x0F12, 0x0001}, // #REG_TC_GP_EnablePreviewChanged

};


const static struct s5k5ca_i2c_reg_conf s5k5ca_snapshot_reg_config[] =
{
{0x0028, 0x7000},
{0x002a, 0x0244},    //#REG_TC_GP_ActiveCapConfig
{0x0F12, 0x0000}, 
{0x002a, 0x0230},   //#REG_TC_GP_NewConfigSync
{0x0F12, 0x0001},  
{0x002a, 0x0246},    //#REG_TC_GP_CapConfigChanged
{0x0F12, 0x0001},  
{0x002a, 0x0224},    //#REG_TC_GP_EnableCapture
{0x0F12, 0x0001},  
{0x0F12, 0x0001},    //REG_TC_GP_EnableCaptureChanged 
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_off_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0000}, // #REG_TC_GP_SpecialEffects
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_mono_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0001}, // #REG_TC_GP_SpecialEffects
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_negative_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0003}, // #REG_TC_GP_SpecialEffects
};


#if 0
const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_solarize_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0003}, // #REG_TC_GP_SpecialEffects
};
#endif
const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_sepia_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0004}, // #REG_TC_GP_SpecialEffects
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_aqua_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0005}, // #REG_TC_GP_SpecialEffects
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_whiteboard_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0006}, // #REG_TC_GP_SpecialEffects
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_effect_blackboard_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x021E},
    {0x0F12,0x0008}, // #REG_TC_GP_SpecialEffects
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_auto_reg_config[] =
{
{0x0028, 0x7000},
{0x002A, 0x04D2},
{0x0F12, 0x067F},
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_a_reg_config[] =
{
    {0x002A,0x04D2},
    {0x0F12,0x0677}, // #REG_TC_DBG_AutoAlgEnBits
    {0x002A,0x04A0},
    {0x0F12,0x03E6}, // #REG_SF_USER_Rgain
    {0x0F12,0x0001}, // #REG_SF_USER_RgainChanged
    {0x002A,0x04A4},
    {0x0F12,0x0400}, // #REG_SF_USER_Ggain
    {0x0F12,0x0001}, // #REG_SF_USER_GgainChanged
    {0x002A,0x04A8},
    {0x0F12,0x0975}, // #REG_SF_USER_Bgain
    {0x0F12,0x0001}, // #REG_SF_USER_BgainChanged            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_tl84_reg_config[] =
{
{0x0028, 0x7000},
{0x002A, 0x04D2},
{0x0F12, 0x0677},
{0x002A, 0x04A0},
{0x0F12, 0x0400},
{0x0F12, 0x0001},
{0x0F12, 0x0400},
{0x0F12, 0x0001},
{0x0F12, 0x083C},
{0x0F12, 0x0001},
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_f_reg_config[] =
{
    {0x002A,0x04D2},
    {0x0F12,0x0677}, // #REG_TC_DBG_AutoAlgEnBits
    {0x002A,0x04A0},
    {0x0F12,0x054C}, // #REG_SF_USER_Rgain
    {0x0F12,0x0001}, // #REG_SF_USER_RgainChanged
    {0x002A,0x04A4},
    {0x0F12,0x0400}, // #REG_SF_USER_Ggain
    {0x0F12,0x0001}, // #REG_SF_USER_GgainChanged
    {0x002A,0x04A8},
    {0x0F12,0x085A}, // #REG_SF_USER_Bgain
    {0x0F12,0x0001}, // #REG_SF_USER_BgainChanged            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_d65_reg_config[] =
{
{0x0028, 0x7000},
{0x002A, 0x04D2},
{0x0F12, 0x0677},
{0x002A, 0x04A0},
{0x0F12, 0x05A0},
{0x0F12, 0x0001},
{0x0F12, 0x0400},
{0x0F12, 0x0001},
{0x0F12, 0x05F0},
{0x0F12, 0x0001},
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_d50_reg_config[] =
{
{0x0028, 0x7000},
{0x002A, 0x04D2},
{0x0F12, 0x0677},
{0x002A, 0x04A0},
{0x0F12, 0x0540},
{0x0F12, 0x0001},
{0x0F12, 0x0400},
{0x0F12, 0x0001},
{0x0F12, 0x0500},
{0x0F12, 0x0001},
};

/* add the antibanding of AF */
const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_off_reg_config[] =
{
    {0x0028,0x7000},                                                                                                                                   
    {0x002A,0x04D2}, //#REG_TC_DBG_AutoAlgEnBits 
    {0x0F12,0x065F},                                                                                 
    {0x002A,0x04BA}, //#REG_SF_USER_FlickerQuant 
    {0x0F12,0x0000},   //Flicker 50Hz:0001/60Hz:0002/off:0000                                                          
    {0x002A,0x04BC}, //#REG_SF_USER_FlickerQuantChanged  
    {0x0F12,0x0001},  //if change 0001 write            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_auto_reg_config[] =
{                         
    {0x0028, 0x7000},
    {0x002A, 0x0C18},  //#AFC_Default60Hz                                                                
    {0x0F12, 0x0001},  // #AFC_Default60Hz  1: Auto Flicker 60Hz start 0: Auto Flicker 50Hz start                               
    {0x002A, 0x04D2},  // #REG_TC_DBG_AutoAlgEnBits                                                                               
    {0x0F12, 0x067F},                   
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_50hz_reg_config[] =
{
    {0x0028, 0x7000},                                                                                                                           
    {0x002A, 0x04D2},  //#REG_TC_DBG_AutoAlgEnBits 
    {0x0F12, 0x065F},                                                                                  
    {0x002A, 0x04BA},  //#REG_SF_USER_FlickerQuant 
    {0x0F12, 0x0001},  //  //Flicker 50Hz:0001/60Hz:0002/off:0000                                                          
    {0x002A, 0x04BC},  //#REG_SF_USER_FlickerQuantChanged 
    {0x0F12, 0x0001},  //  //if change 0001 write      
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_60hz_reg_config[] =
{
    {0x0028,0x7000},                                                                                                                           
    {0x002A,0x04D2},  //#REG_TC_DBG_AutoAlgEnBits 
    {0x0F12,0x065F},                                                                                
    {0x002A,0x04BA},  //#REG_SF_USER_FlickerQuant 
    {0x0F12,0x0002},  //Flicker 50Hz:0001/60Hz:0002/off:0000                                                          
    {0x002A,0x04BC},  //#REG_SF_USER_FlickerQuantChanged 
    {0x0F12,0x0001},  //if change 0001 write  
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_preview_reg_config_FF[] =
{

//================================================================================================
// APPLY PREVIEW CONFIGURATION & RUN PREVIEW
//================================================================================================
{0x0028,0x7000},
{0x002A,0x0224},
{0x0F12,0x0000}, // #REG_TC_GP_EnableCapture // disable Capture
{0x0F12,0x0000}, // #REG_TC_GP_EnableCaptureChanged    

{0x002A,0x023C},
{0x0F12,0x0000}, // #REG_TC_GP_ActivePrevConfig // Select preview configuration_0
{0x002A,0x0240},
{0x0F12,0x0001}, // #REG_TC_GP_PrevOpenAfterChange
{0x002A,0x0230},
{0x0F12,0x0001}, // #REG_TC_GP_NewConfigSync // Update preview configuration
{0x002A,0x023E},
{0x0F12,0x0001}, // #REG_TC_GP_PrevConfigChanged
{0x002A,0x0220},
{0x0F12,0x0001}, // #REG_TC_GP_EnablePreview // Start preview
{0x0F12,0x0001}, // #REG_TC_GP_EnablePreviewChanged

};

const static struct s5k5ca_i2c_reg_conf s5k5ca_snapshot_reg_config_FF[] =
{
{0x0028,0x7000},
{0x002A,0x0220},
{0x0F12,0x0000}, // #REG_TC_GP_EnablePreview // disable preview
{0x0F12,0x0000}, // #REG_TC_GP_EnablePreviewChanged  

{0x002A,0x0244},
{0x0F12,0x0000}, // #REG_TC_GP_ActiveCapConfig // Select capture configuration_0

{0x002A,0x0230},
{0x0F12,0x0001}, // #REG_TC_GP_NewConfigSync // Update preview configuration
{0x002A,0x0246},
{0x0F12,0x0001}, // #REG_TC_GP_CapConfigChanged
{0x002A,0x0224},
{0x0F12,0x0001}, // #REG_TC_GP_EnableCapture // Start Capture
{0x0F12,0x0001}, // #REG_TC_GP_EnableCaptureChanged

};


const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_auto_reg_config_FF[] =
{
    {0x0028, 0x7000},
    {0x002A,0x04D2},
    {0x0F12,0x067F}, // #REG_TC_DBG_AutoAlgEnBits
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_a_reg_config_FF[] =
{
    {0x002A,0x04D2},
    {0x0F12,0x0677}, // #REG_TC_DBG_AutoAlgEnBits
    {0x002A,0x04A0},
    {0x0F12,0x03E6}, // #REG_SF_USER_Rgain
    {0x0F12,0x0001}, // #REG_SF_USER_RgainChanged
    {0x002A,0x04A4},
    {0x0F12,0x0400}, // #REG_SF_USER_Ggain
    {0x0F12,0x0001}, // #REG_SF_USER_GgainChanged
    {0x002A,0x04A8},
    {0x0F12,0x0975}, // #REG_SF_USER_Bgain
    {0x0F12,0x0001}, // #REG_SF_USER_BgainChanged            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_tl84_reg_config_FF[] =
{
    {0x002A,0x04D2},
    {0x0F12,0x0677}, // #REG_TC_DBG_AutoAlgEnBits
    {0x002A,0x04A0},
    {0x0F12,0x04CD}, // #REG_SF_USER_Rgain
    {0x0F12,0x0001}, // #REG_SF_USER_RgainChanged
    {0x002A,0x04A4},
    {0x0F12,0x0400}, // #REG_SF_USER_Ggain
    {0x0F12,0x0001}, // #REG_SF_USER_GgainChanged
    {0x002A,0x04A8},
    {0x0F12,0x076C}, // #REG_SF_USER_Bgain
    {0x0F12,0x0001}, // #REG_SF_USER_BgainChanged            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_f_reg_config_FF[] =
{
    {0x002A,0x04D2},
    {0x0F12,0x0677}, // #REG_TC_DBG_AutoAlgEnBits
    {0x002A,0x04A0},
    {0x0F12,0x054C}, // #REG_SF_USER_Rgain
    {0x0F12,0x0001}, // #REG_SF_USER_RgainChanged
    {0x002A,0x04A4},
    {0x0F12,0x0400}, // #REG_SF_USER_Ggain
    {0x0F12,0x0001}, // #REG_SF_USER_GgainChanged
    {0x002A,0x04A8},
    {0x0F12,0x085A}, // #REG_SF_USER_Bgain
    {0x0F12,0x0001}, // #REG_SF_USER_BgainChanged            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_d65_reg_config_FF[] =
{
    {0x002A,0x04D2},
    {0x0F12,0x0677}, // #REG_TC_DBG_AutoAlgEnBits
    {0x002A,0x04A0},
    {0x0F12,0x0631}, // #REG_SF_USER_Rgain
    {0x0F12,0x0001}, // #REG_SF_USER_RgainChanged
    {0x002A,0x04A4},
    {0x0F12,0x0400}, // #REG_SF_USER_Ggain
    {0x0F12,0x0001}, // #REG_SF_USER_GgainChanged
    {0x002A,0x04A8},
    {0x0F12,0x04E6}, // #REG_SF_USER_Bgain
    {0x0F12,0x0001}, // #REG_SF_USER_BgainChanged            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_wb_d50_reg_config_FF[] =
{
    {0x002A,0x04D2},
    {0x0F12,0x0677}, // #REG_TC_DBG_AutoAlgEnBits
    {0x002A,0x04A0},
    {0x0F12,0x0584}, // #REG_SF_USER_Rgain
    {0x0F12,0x0001}, // #REG_SF_USER_RgainChanged
    {0x002A,0x04A4},
    {0x0F12,0x0400}, // #REG_SF_USER_Ggain
    {0x0F12,0x0001}, // #REG_SF_USER_GgainChanged
    {0x002A,0x04A8},
    {0x0F12,0x060A}, // #REG_SF_USER_Bgain
    {0x0F12,0x0001}, // #REG_SF_USER_BgainChanged            
};

const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_off_reg_config_FF[] =
{
    {0x0028,0x7000},
    {0x002A,0x04BA},
    {0x0F12,0x0000}, // #REG_SF_USER_FlickerQuant
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_auto_reg_config_FF[] =
{                         
    {0x0028, 0x7000},
    {0x002A, 0x0C18},  //#AFC_Default60Hz                                                                
    {0x0F12, 0x0001},  // #AFC_Default60Hz  1: Auto Flicker 60Hz start 0: Auto Flicker 50Hz start                               
    {0x002A, 0x04D2},  // #REG_TC_DBG_AutoAlgEnBits                                                                               
    {0x0F12, 0x067F},                   
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_50hz_reg_config_FF[] =
{
    {0x0028,0x7000},
    {0x002A,0x04BA},
    {0x0F12,0x0001}, // #REG_SF_USER_FlickerQuant
};
const static struct s5k5ca_i2c_reg_conf s5k5ca_antibanding_60hz_reg_config_FF[] =
{
    {0x0028,0x7000},
    {0x002A,0x04BA},
    {0x0F12,0x0002}, // #REG_SF_USER_FlickerQuant
};

static struct  s5k5ca_work_t *s5k5casensorw = NULL;

static struct  i2c_client *s5k5ca_client = NULL;
static struct s5k5ca_ctrl_t *s5k5ca_ctrl = NULL;
static enum s5k5ca_reg_update_t last_rupdate = -1;
static enum s5k5ca_setting_t last_rt = -1;
static DECLARE_WAIT_QUEUE_HEAD(s5k5ca_wait_queue);
DECLARE_MUTEX(s5k5ca_sem);
DECLARE_MUTEX(s5k5ca_sem2);

static int s5k5ca_i2c_rxdata(unsigned short saddr,
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

	if (i2c_transfer(s5k5ca_client->adapter, msgs, 2) < 0) {
		CDBG("s5k5ca_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t s5k5ca_i2c_read_w(unsigned short raddr, unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = s5k5ca_i2c_rxdata(s5k5ca_client->addr, buf, 2);
	if (rc < 0)
		return rc;

	*rdata = buf[0] << 8 | buf[1];

	if (rc < 0)
		CDBG("s5k5ca_i2c_read failed!\n");

	return rc;
}

static int32_t s5k5ca_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	int32_t i = 0;
	int32_t rc = -EFAULT;
	struct i2c_msg msg[] = {
	{
		.addr = saddr,
		.flags = 0,
		.len = length,
		.buf = txdata,
	},
	};
	for(i = 0; i < 3; i++)
	{
	  rc = i2c_transfer(s5k5ca_client->adapter, msg, 1);
	  if(0 <= rc)
	    return 0;
	}
	if(3 == i)
	{
	  CDBG("s5k5ca_i2c_txdata faild\n");
	  return -EIO;
	}
	return 0;
}

static int32_t s5k5ca_i2c_write_w(unsigned short waddr, unsigned short wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00)>>8;
	buf[3] = (wdata & 0x00FF);

	rc = s5k5ca_i2c_txdata(s5k5ca_client->addr, buf, 4);

	if (rc < 0)
		CDBG("i2c_write_w failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t s5k5ca_i2c_write_w_table(
	struct s5k5ca_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = s5k5ca_i2c_write_w(reg_conf_tbl->waddr, reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}

	return rc;
}

int32_t s5k5ca_set_default_focus(uint8_t af_step)
{
    int32_t rc = 0;

    return rc;
}

int32_t s5k5ca_set_fps(struct fps_cfg    *fps)
{
    /* input is new fps in Q8 format */
    int32_t rc = 0;

    CDBG("s5k5ca_set_fps\n");
    return rc;
}

int32_t s5k5ca_write_exp_gain(uint16_t gain, uint32_t line)
{
    CDBG("s5k5ca_write_exp_gain\n");
    return 0;
}

int32_t s5k5ca_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
    int32_t rc = 0;

    CDBG("s5k5ca_set_pict_exp_gain\n");

    mdelay(10);

    /* camera_timed_wait(snapshot_wait*exposure_ratio); */
    return rc;
}

static int32_t s5k5ca_set_mirror_mode(void)
{
    int32_t rc = 0;

    const static struct s5k5ca_i2c_reg_conf s5k5ca_mirror_mode_reg_config0[] =
    {
        {0x0028,0x7000},
        {0x002A,0x0296},
        {0x0F12,0x0000}, //#REG_0TC_PCFG_uPrevMirror
        {0x0F12,0x0000}, //#REG_0TC_PCFG_uCaptureMirror
    };
    const static struct s5k5ca_i2c_reg_conf s5k5ca_mirror_mode_reg_config1[] =
    {
        {0x0028,0x7000},
        {0x002A,0x0296},	
        {0x0F12,0x0003}, //#REG_0TC_PCFG_uPrevMirror
        {0x0F12,0x0003}, //#REG_0TC_PCFG_uCaptureMirror
    
    };	
    if (machine_is_msm7x25_u8300()||machine_is_msm7x25_u8350()) {
        rc = s5k5ca_i2c_write_w_table(s5k5ca_mirror_mode_reg_config0,
                    S5K5CA_ARRAY_SIZE(s5k5ca_mirror_mode_reg_config0));
    } else {
        rc = s5k5ca_i2c_write_w_table(s5k5ca_mirror_mode_reg_config1,
                    S5K5CA_ARRAY_SIZE(s5k5ca_mirror_mode_reg_config1));
    }

    return rc;
}

int32_t s5k5ca_setting(enum s5k5ca_reg_update_t rupdate,
                       enum s5k5ca_setting_t    rt)
{
    int32_t rc = 0;
#if 0
    static enum s5k5ca_reg_update_t last_rupdate = -1;
    static enum s5k5ca_setting_t last_rt = -1;
#endif
    
	down(&s5k5ca_sem2);
    if(rupdate == last_rupdate && rt == last_rt)
    {
        CDBG("s5k5ca_setting exit\n");
        up(&s5k5ca_sem2);
        return rc;
    }    
    CDBG("s5k5ca_setting in rupdate=%d,rt=%d\n",rupdate,rt);
    switch (rupdate)
    {
        case UPDATE_PERIODIC:
        /*if model is AF*/
            if(MODEL_AF_LITEON == s5k5ca_model_id)
	        {
	           CDBG("s5k5ca MODEL_AF_LITEON set");
            /*preview setting*/	          
            if(rt == RES_PREVIEW)
            {
                if(last_rt == RES_CAPTURE)
                {
                    rc = s5k5ca_i2c_write_w_table(s5k5ca_before_preview_reg_config,
	                                    S5K5CA_ARRAY_SIZE(s5k5ca_before_preview_reg_config));
	                mdelay(80);
	                rc = s5k5ca_i2c_write_w_table(s5k5ca_preview_reg_config,
	                                    S5K5CA_ARRAY_SIZE(s5k5ca_preview_reg_config));
                        //continue AF command 
	                s5k5ca_i2c_write_w(0x0028 , 0x7000);                    
	                s5k5ca_i2c_write_w(0x002A , 0x0252);  //REG_TC_AF_AfCmd                    
                    s5k5ca_i2c_write_w(0x0F12 , 0x0003);				
	                mdelay(10);
	                if(s5k5ca_af_mode == CAMERA_AF_ON)
	                {
	                    s5k5ca_i2c_write_w(0x002A , 0x0252);
	                    s5k5ca_i2c_write_w(0x0F12 , 0x0006);
	                 }
	                    
	                    mdelay(10);
                        break;
                        
                }
                    rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config1_liteon_AF,
	                                            S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config1_liteon_AF));
	                mdelay(20);                       
	                rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config2_liteon_AF,
	                                            S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config2_liteon_AF));
	                mdelay(20);
	                rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config3_liteon_AF,
	                                            S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config3_liteon_AF));  	
			
			  		rc = s5k5ca_i2c_write_w_table(s5k5ca_before_preview_reg_config,
	                            S5K5CA_ARRAY_SIZE(s5k5ca_before_preview_reg_config));
	                mdelay(80);
	                rc = s5k5ca_i2c_write_w_table(s5k5ca_preview_reg_config,
	                            S5K5CA_ARRAY_SIZE(s5k5ca_preview_reg_config));
                    s5k5ca_i2c_write_w(0x0028 , 0x7000);                    
	                s5k5ca_i2c_write_w(0x002A , 0x0252);  //REG_TC_AF_AfCmd                    
	                s5k5ca_i2c_write_w(0x0F12 , 0x0003);
	                mdelay(10);	
	                if(s5k5ca_af_mode == CAMERA_AF_ON)
	                {
	                    s5k5ca_i2c_write_w(0x002A , 0x0252);
	                    s5k5ca_i2c_write_w(0x0F12 , 0x0006);
	                }
	                
	                mdelay(10); 
                }
                else
                {
	            	 s5k5ca_i2c_write_w(0x0028 , 0x7000);
			   		 s5k5ca_i2c_write_w(0x002A , 0x0252);
	                 s5k5ca_i2c_write_w(0x0F12 , 0x0002);
					 
	            	 mdelay(10);				 
	                 rc = s5k5ca_i2c_write_w_table(s5k5ca_snapshot_reg_config,
	                                S5K5CA_ARRAY_SIZE(s5k5ca_snapshot_reg_config));         
	            }
            }
            

               /* add FPC packing model,sunny ,liteon and foxcom */

			   else 
               {
			    CDBG("s5k5ca FF set model:%d", s5k5ca_model_id);
				/*preview setting*/
				if(rt == RES_PREVIEW)
	            {
	                if(last_rt == RES_CAPTURE)
	                {
	                    rc = s5k5ca_i2c_write_w_table(s5k5ca_preview_reg_config_FF,
	                                    S5K5CA_ARRAY_SIZE(s5k5ca_preview_reg_config_FF));
	                    break;
	                }                  
                    if(MODEL_SUNNY_FPC == s5k5ca_model_id)
				    {
					    rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_sunny_FPC_1,
	                                    S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_sunny_FPC_1));
						mdelay(100);
                     }
				    else if(MODEL_LITEON_FPC == s5k5ca_model_id)
				    {
					    rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_liteon_FPC_1,
	                                    S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_liteon_FPC_1));
				        mdelay(100);
				    }
                    rc = s5k5ca_i2c_write_w_table(p_s5k5ca_init_reg_config, reg_num);

                    if(MODEL_SUNNY_FPC == s5k5ca_model_id)
                    {
                        mdelay(10);
				        rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_sunny_FPC_2,
	                                    S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_sunny_FPC_2));
						mdelay(10);
    					rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_sunny_FPC_3,
	                                    S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_sunny_FPC_3));
                     }
    				else if(MODEL_LITEON_FPC == s5k5ca_model_id)
    				{
    					mdelay(100);
    					rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_liteon_FPC_2,
    	                                    S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_liteon_FPC_2));
    				}
                    if(rc >= 0)
                    {
                        rc = s5k5ca_set_mirror_mode();
                    }
                   
                    mdelay(20);
                }
                else
                {
    	                rc = s5k5ca_i2c_write_w_table(s5k5ca_snapshot_reg_config_FF,
    						         S5K5CA_ARRAY_SIZE(s5k5ca_snapshot_reg_config_FF));
    	                
    	         }
                mdelay(10);
                }
            break;
        case REG_INIT:
#if 0
            if(MODEL_LITEON == s5k5ca_model_id || MODEL_BYD == s5k5ca_model_id)
            {
            
                rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_liteon_and_byd,
                                            S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_liteon_and_byd));
            }
            else if(MODEL_SUNNY == s5k5ca_model_id)
            {
                rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_sunny,
                                            S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_sunny));
                
            }
            else if(MODEL_FOXCOM == s5k5ca_model_id)
            {
                rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_foxcom,
                                            S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_foxcom));
                
            }
			else
			{
                CDBG("s5k5ca_model_id is error and use s5k5ca_init_reg_config_liteon_and_byd\n");
                rc = s5k5ca_i2c_write_w_table(s5k5ca_init_reg_config_liteon_and_byd,
                                            S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_liteon_and_byd));
			}

            if(rc >= 0)
            {
                rc = s5k5ca_set_mirror_mode();
            }
            mdelay(20);
#endif
            break;

        default:
            rc = -EFAULT;
            break;
    } /* switch (rupdate) */
	CDBG("s5k5ca_setting out rupdate=%d,rt=%d\n",rupdate,rt);
    if(rc == 0)
    {
        last_rupdate = rupdate;
        last_rt = rt; 
    }
	up(&s5k5ca_sem2);
    return rc;
}

int32_t s5k5ca_video_config(int mode, int res)
{
    int32_t rc;

    switch (res)
    {
        case QTR_SIZE:
            rc = s5k5ca_setting(UPDATE_PERIODIC, RES_PREVIEW);
            if (rc < 0)
            {
                return rc;
            }

            CDBG("sensor configuration done!\n");
            break;

        case FULL_SIZE:
            rc = s5k5ca_setting(UPDATE_PERIODIC, RES_CAPTURE);
            if (rc < 0)
            {
                return rc;
            }

            break;

        default:
            return 0;
    } /* switch */

    s5k5ca_ctrl->prev_res   = res;
    s5k5ca_ctrl->curr_res   = res;
    s5k5ca_ctrl->sensormode = mode;

    return rc;
}

int32_t s5k5ca_snapshot_config(int mode)
{
    int32_t rc = 0;
    CDBG("s5k5ca_snapshot_config in\n");
    rc = s5k5ca_setting(UPDATE_PERIODIC, RES_CAPTURE);
    mdelay(50);
    if (rc < 0)
    {
        return rc;
    }

    s5k5ca_ctrl->curr_res = s5k5ca_ctrl->pict_res;

    s5k5ca_ctrl->sensormode = mode;

    return rc;
}

int32_t s5k5ca_power_down(void)
{
    int32_t rc = 0;

    mdelay(5);

    return rc;
}

int32_t s5k5ca_move_focus(int direction, int32_t num_steps)
{
    return 0;
}

static int s5k5ca_sensor_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
    
    gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
   
    if (data->vreg_disable_func)
    {
        data->vreg_disable_func(data->sensor_vreg, data->vreg_num);
    }
    last_rupdate = -1;
    last_rt = -1;
	return 0;
}

static int s5k5ca_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;
	unsigned short chipid;

	/* pull down power down */
	rc = gpio_request(data->sensor_pwd, "s5k5ca");
	if (!rc || rc == -EBUSY)
		gpio_direction_output(data->sensor_pwd, 1);
	else 
        goto init_probe_fail;
    
	rc = gpio_request(data->sensor_reset, "s5k5ca");
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
    
    mdelay(20);
    
    if(data->master_init_control_slave == NULL 
        || data->master_init_control_slave(data) != 0
        )
    {

        rc = gpio_direction_output(data->sensor_pwd, 0);
         if (rc < 0)
            goto init_probe_fail;

        mdelay(20);
        /*hardware reset*/
        rc = gpio_direction_output(data->sensor_reset, 1);
        if (rc < 0)
            goto init_probe_fail;

        mdelay(20);
    }

	rc = s5k5ca_i2c_write_w(0x0010, 0x0001);
	if (rc < 0)
	{
        CDBG("s5k5ca_i2c_write_w 0x0010 0x0001 rc=%d", rc);
		goto init_probe_fail;
	}
    mdelay(10);
	rc = s5k5ca_i2c_write_w(0x0010, 0x0000);
	if (rc < 0)
	{
        CDBG("s5k5ca_i2c_write_w 0x0010 0x0000 rc=%d", rc);
		goto init_probe_fail;
	}
    mdelay(10);
    
	rc = s5k5ca_i2c_write_w(0x002c, 0x0000);
	if (rc < 0)
	{
        CDBG("s5k5ca_i2c_write_w 0x002c rc=%d", rc);
		goto init_probe_fail;
	}
	rc = s5k5ca_i2c_write_w(0x002e, 0x0040);
	if (rc < 0)
	{
        CDBG("s5k5ca_i2c_write_w 0x002e rc=%d", rc);

		goto init_probe_fail;
	}
	/* 3. Read sensor Model ID: */
	rc = s5k5ca_i2c_read_w(0x0f12, &chipid);

	if (rc < 0)
	{
        CDBG("s5k5ca_i2c_read_w 0x002e rc=%d", rc);
		goto init_probe_fail;
	}
	CDBG("s5k5ca chipid = 0x%x\n", chipid);

	/* 4. Compare sensor ID to S5K5CA ID: */
	if (chipid != S5K5CA_CHIP_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}
    if(machine_is_msm7x25_um840())
	{
		s5k5ca_model_id = MODEL_AF_LITEON;
		CDBG("s5k5ca is MODEL_AF_LITEON!!!");
	}
	else
    {
    /* Change the method of reading model id to fit socket and FPC packing models
     * Socket model : 0--3
     * FPC model : 8--11
     */
    	rc = s5k5ca_i2c_write_w(0xFCFC, 0xD000);
    	if (rc < 0)
    		goto init_probe_fail;    
    	rc = s5k5ca_i2c_write_w(0x108E, 0x3333);
    	if (rc < 0)
    		goto init_probe_fail;     
        mdelay(2);
        
    	rc = s5k5ca_i2c_write_w(0x1090, 0x8888);
    	if (rc < 0)
    		goto init_probe_fail;
    	rc = s5k5ca_i2c_read_w(0x100C, &s5k5ca_model_id);
    	if (rc < 0)
    	{
            CDBG("s5k5ca_i2c_read_w 0x002e rc=%d", rc);
        }	
        CDBG("s5k5ca model = 0x%x\n", s5k5ca_model_id);
    	/* If ID out of range ,set model_id MODEL_LITEON_FPC as default */
        if((s5k5ca_model_id > MODEL_FOXCOM_FPC)||
    		((s5k5ca_model_id > MODEL_FOXCOM)&&(s5k5ca_model_id < MODEL_LITEON_FPC)))
        {
            s5k5ca_model_id = MODEL_LITEON_FPC;
        }
    	rc = s5k5ca_i2c_write_w(0x108E, 0x0000);
    	if (rc < 0)
    		goto init_probe_fail;
        switch(s5k5ca_model_id)
        {
        	case MODEL_LITEON:
                if (machine_is_msm7x25_u8300())
                {
                     p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_liteon_and_byd_u8300;
                     reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_liteon_and_byd_u8300);
                     CDBG("s5k5ca is MODEL_LITEON!!!machine is msm7x25 u8300!!!");
                }
                else
                {
    				p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_liteon;
    				reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_liteon);
    				CDBG("s5k5ca is MODEL_LITEON!!!");
                }
    			break;
    		case MODEL_SUNNY:
                if (machine_is_msm7x25_u8300())
                {
                    p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_sunny_u8300;
                    reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_sunny_u8300);
                    CDBG("s5k5ca is MODEL_SUNNY!!!machine is msm7x25 u8300!!!");
                }
                else
                {
    				p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_sunny;
    				reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_sunny);
    				CDBG("s5k5ca is MODEL_SUNNY!!!");
                }
    			break;
    		case MODEL_FOXCOM:
                if (machine_is_msm7x25_u8300())
                {
                    p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_foxcom_u8300;
                    reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_foxcom_u8300);
                        CDBG("s5k5ca is MODEL_FOXCOM!!!,machine is msm7x25 u8300!!!");
                }
                else
                {
    				p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_foxcom;
    				reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_foxcom);
    				CDBG("s5k5ca is MODEL_FOXCOM!!!");
                }
    			break;
    		case MODEL_LITEON_FPC:
    			p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_liteon_FPC;
    			reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_liteon_FPC);
    			CDBG("s5k5ca is MODEL_LITEON_FPC!!!");
    			break;
    		case MODEL_SUNNY_FPC:
    			p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_sunny_FPC;
    			reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_sunny_FPC);
    			CDBG("s5k5ca is MODEL_SUNNY_FPC!!!");
    			break;
    		case MODEL_FOXCOM_FPC:
    			p_s5k5ca_init_reg_config = s5k5ca_init_reg_config_foxcom_FPC;
    			reg_num = S5K5CA_ARRAY_SIZE(s5k5ca_init_reg_config_foxcom_FPC);
    			CDBG("s5k5ca is MODEL_FOXCOM_FPC!!!");
    			break;
    		default:
    			goto init_probe_fail;
    			CDBG("s5k5ca is no this sensor model!!!!!!\n");
    			break;
        }
    }
    goto init_probe_done;

init_probe_fail:
    s5k5ca_sensor_init_done(data);
init_probe_done:
	return rc;
}

int s5k5ca_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	s5k5ca_ctrl = kzalloc(sizeof(struct s5k5ca_ctrl_t), GFP_KERNEL);
	if (!s5k5ca_ctrl) {
		CDBG("s5k5ca_sensor_open_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	s5k5ca_ctrl->fps_divider = 1 * 0x00000400;
	s5k5ca_ctrl->pict_fps_divider = 1 * 0x00000400;
	s5k5ca_ctrl->set_test = TEST_OFF;
	s5k5ca_ctrl->prev_res = QTR_SIZE;
	s5k5ca_ctrl->pict_res = FULL_SIZE;

	if (data)
		s5k5ca_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(S5K5CA_DEFAULT_CLOCK_RATE);
	mdelay(20);

	msm_camio_camif_pad_reg_reset();
	mdelay(20);

  rc = s5k5ca_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;
    
	if (s5k5ca_ctrl->prev_res == QTR_SIZE)
		rc = s5k5ca_setting(REG_INIT, RES_PREVIEW);
	else
		rc = s5k5ca_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0)
		goto init_fail;
	else
		goto init_done;

init_fail:
	kfree(s5k5ca_ctrl);
init_done:
	return rc;
}

int s5k5ca_init_client(struct i2c_client *client)
{
    /* Initialize the MSM_CAMI2C Chip */
    init_waitqueue_head(&s5k5ca_wait_queue);
    return 0;
}

int32_t s5k5ca_set_sensor_mode(int mode, int res)
{
    int32_t rc = 0;

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
            CDBG("SENSOR_PREVIEW_MODE,res=%d\n",res);
            rc = s5k5ca_video_config(mode, res);
            break;

        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
            CDBG("SENSOR_SNAPSHOT_MODE\n");
            rc = s5k5ca_snapshot_config(mode);
            break;

        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}

static long s5k5ca_set_effect(int mode, int effect)
{
	struct s5k5ca_i2c_reg_conf const *reg_conf_tbl = NULL;
    int num_of_items_in_table = 0;
	long rc = 0;

	switch (effect) {
	case CAMERA_EFFECT_OFF:
        reg_conf_tbl = s5k5ca_effect_off_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_off_reg_config);
        break;

	case CAMERA_EFFECT_MONO:
        reg_conf_tbl = s5k5ca_effect_mono_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_mono_reg_config);
		break;

	case CAMERA_EFFECT_NEGATIVE:
        reg_conf_tbl = s5k5ca_effect_negative_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_negative_reg_config);
		break;
#if 0
	case CAMERA_EFFECT_SOLARIZE:
        reg_conf_tbl = s5k5ca_effect_solarize_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_solarize_reg_config);
		break;
#endif		
	case CAMERA_EFFECT_SEPIA:
        reg_conf_tbl = s5k5ca_effect_sepia_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_sepia_reg_config);
		break;
        
	case CAMERA_EFFECT_AQUA:
        reg_conf_tbl = s5k5ca_effect_aqua_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_aqua_reg_config);
		break;
        
	case CAMERA_EFFECT_WHITEBOARD:
        reg_conf_tbl = s5k5ca_effect_whiteboard_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_whiteboard_reg_config);
		break;  

	case CAMERA_EFFECT_BLACKBOARD:
        reg_conf_tbl = s5k5ca_effect_blackboard_reg_config;
        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_effect_blackboard_reg_config);
		break;     
        
	default: 
		return 0;
	}

    rc = s5k5ca_i2c_write_w_table(reg_conf_tbl, num_of_items_in_table);
    return rc;

    
}

static long s5k5ca_set_wb(int wb)
{
	struct s5k5ca_i2c_reg_conf const *reg_conf_tbl = NULL;
    int num_of_items_in_table = 0;
	long rc = 0;
    /*AF*/
    if(MODEL_AF_LITEON == s5k5ca_model_id)
    {
        CDBG("~~~~~~~~~penghai s5k5ca_set_wb AF wb:%d", wb);
    	switch (wb)
		{
			case CAMERA_WB_AUTO:
		        reg_conf_tbl = s5k5ca_wb_auto_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_auto_reg_config);
                CDBG("<=========s5k5ca_set_wb AF wb:%d s5k5ca_wb_auto_reg_config", wb);
		        break;

			case CAMERA_WB_INCANDESCENT:
		        reg_conf_tbl = s5k5ca_wb_a_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_a_reg_config);
                CDBG("<=========s5k5ca_set_wb AF wb:%d s5k5ca_wb_a_reg_config", wb);
				break;
		        
			case CAMERA_WB_CUSTOM:
		        reg_conf_tbl = s5k5ca_wb_f_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_f_reg_config);
                CDBG("<=========s5k5ca_set_wb AF wb:%d s5k5ca_wb_f_reg_config", wb);
				break;	
			case CAMERA_WB_FLUORESCENT:
		        reg_conf_tbl = s5k5ca_wb_tl84_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_tl84_reg_config);
                CDBG("<=========s5k5ca_set_wb AF wb:%d s5k5ca_wb_tl84_reg_config", wb);
				break;

			case CAMERA_WB_DAYLIGHT:
		        reg_conf_tbl = s5k5ca_wb_d65_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_d65_reg_config);
                CDBG("<=========s5k5ca_set_wb AF wb:%d s5k5ca_wb_d65_reg_config", wb);
				break;
		        
			case CAMERA_WB_CLOUDY_DAYLIGHT:
		        reg_conf_tbl = s5k5ca_wb_d50_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_d50_reg_config);
                CDBG("<=========s5k5ca_set_wb AF wb:%d s5k5ca_wb_d50_reg_config", wb);
				break;
		        
			case CAMERA_WB_TWILIGHT:
		        return 0;
				break;  

			case CAMERA_WB_SHADE:
		        return 0;
				break;     
		        
			default: 
				return 0;
		}
    }
    /*FF*/
    else
    {
		CDBG("~~~~~~~~~penghai s5k5ca_set_wb FF wb:%d", wb);
		switch (wb)
		{
			case CAMERA_WB_AUTO:
		        reg_conf_tbl = s5k5ca_wb_auto_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_auto_reg_config_FF);
                CDBG("<========s5k5ca_set_wb FF wb:%d s5k5ca_wb_auto_reg_config_FF", wb);
		        break;

			case CAMERA_WB_INCANDESCENT:
		        reg_conf_tbl = s5k5ca_wb_a_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_a_reg_config_FF);
                CDBG("<========s5k5ca_set_wb FF wb:%d s5k5ca_wb_a_reg_config_FF", wb);
				break;
		        
			case CAMERA_WB_CUSTOM:
		        reg_conf_tbl = s5k5ca_wb_f_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_f_reg_config_FF);
                CDBG("<========s5k5ca_set_wb FF wb:%d s5k5ca_wb_f_reg_config_FF", wb);
				break;	
			case CAMERA_WB_FLUORESCENT:
		        reg_conf_tbl = s5k5ca_wb_tl84_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_tl84_reg_config_FF);
                CDBG("<========s5k5ca_set_wb FF wb:%d s5k5ca_wb_tl84_reg_config_FF", wb);
				break;

			case CAMERA_WB_DAYLIGHT:
		        reg_conf_tbl = s5k5ca_wb_d65_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_d65_reg_config_FF);
                CDBG("<========s5k5ca_set_wb FF wb:%d s5k5ca_wb_d65_reg_config_FF", wb);
				break;
		        
			case CAMERA_WB_CLOUDY_DAYLIGHT:
		        reg_conf_tbl = s5k5ca_wb_d50_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_wb_d50_reg_config_FF);
                CDBG("<========s5k5ca_set_wb FF wb:%d s5k5ca_wb_d50_reg_config_FF", wb);
				break;
		        
			case CAMERA_WB_TWILIGHT:
		        return 0;
				break;  

			case CAMERA_WB_SHADE:
		        return 0;
				break;     
		        
			default: 
				return 0;
		}
	}
    rc = s5k5ca_i2c_write_w_table(reg_conf_tbl, num_of_items_in_table);
    return rc;

}

static long s5k5ca_set_antibanding(int antibanding)
{
	struct s5k5ca_i2c_reg_conf const *reg_conf_tbl = NULL;
    int num_of_items_in_table = 0;
	long rc = 0;
    /*AF*/
    if(MODEL_AF_LITEON == s5k5ca_model_id)
    {
    	CDBG("~~~~~~~~~penghai s5k5ca_set_antibanding AF antibanding:%d", antibanding);
    	switch (antibanding)
		{
			case CAMERA_ANTIBANDING_OFF:
		        reg_conf_tbl = s5k5ca_antibanding_off_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_off_reg_config);
                CDBG("<============s5k5ca_set_antibanding AF antibanding:%d s5k5ca_antibanding_off_reg_config", antibanding);
		        break;

			case CAMERA_ANTIBANDING_60HZ:
		        reg_conf_tbl = s5k5ca_antibanding_60hz_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_60hz_reg_config);
                CDBG("<============s5k5ca_set_antibanding AF antibanding:%d s5k5ca_antibanding_60hz_reg_config", antibanding);
		        break;
		        
			case CAMERA_ANTIBANDING_50HZ:
		        reg_conf_tbl = s5k5ca_antibanding_50hz_reg_config;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_50hz_reg_config);
		        CDBG("<============s5k5ca_set_antibanding AF antibanding:%d s5k5ca_antibanding_50hz_reg_config", antibanding);
		        break;

			default: 
				return 0;        
		}   
	    reg_conf_tbl = s5k5ca_antibanding_auto_reg_config;
	    num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_auto_reg_config);	 
    }
    /*FF*/
    else
    {
		CDBG("~~~~~~~~~penghai s5k5ca_set_antibanding FF antibanding:%d", antibanding);
		switch (antibanding)
		{
			case CAMERA_ANTIBANDING_OFF:
		        reg_conf_tbl = s5k5ca_antibanding_off_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_off_reg_config_FF);
                CDBG("<==============s5k5ca_set_antibanding FF antibanding:%d s5k5ca_antibanding_off_reg_config_FF", antibanding);
		        break;

			case CAMERA_ANTIBANDING_60HZ:
		        reg_conf_tbl = s5k5ca_antibanding_60hz_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_60hz_reg_config_FF);
                CDBG("<==============s5k5ca_set_antibanding FF antibanding:%d s5k5ca_antibanding_60hz_reg_config_FF", antibanding);
		        break;
		        
			case CAMERA_ANTIBANDING_50HZ:
		        reg_conf_tbl = s5k5ca_antibanding_50hz_reg_config_FF;
		        num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_50hz_reg_config_FF);
		        CDBG("<==============s5k5ca_set_antibanding FF antibanding:%d s5k5ca_antibanding_50hz_reg_config_FF", antibanding);
		        break;

			default: 
				return 0;        
		}	   
	    reg_conf_tbl = s5k5ca_antibanding_auto_reg_config_FF;
	    num_of_items_in_table = S5K5CA_ARRAY_SIZE(s5k5ca_antibanding_auto_reg_config_FF);	    
	}
    rc = s5k5ca_i2c_write_w_table(reg_conf_tbl, num_of_items_in_table);
    return rc;
    
}

int s5k5ca_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	down(&s5k5ca_sem);

  CDBG("s5k5ca_sensor_config: cfgtype = %d\n",
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
			rc = s5k5ca_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				s5k5ca_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				s5k5ca_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = s5k5ca_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = s5k5ca_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				s5k5ca_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				s5k5ca_set_default_focus(
					cdata.cfg.focus.steps);
			break;
		case CFG_SET_EFFECT:
			rc = s5k5ca_set_effect(cdata.mode,
						cdata.cfg.effect);            
			break;
            
		case CFG_SET_WB:
			rc = s5k5ca_set_wb(cdata.cfg.effect);            
			break;
            
		case CFG_SET_ANTIBANDING:
			rc = s5k5ca_set_antibanding(cdata.cfg.effect);            
			break;

		case CFG_MAX:
            if (copy_to_user((void *)(cdata.cfg.pict_max_exp_lc),
            		s5k5ca_supported_effect,
            		S5K5CA_ARRAY_SIZE(s5k5ca_supported_effect))) 
            {
                CDBG("copy s5k5ca_supported_effect to user fail\n");
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

	up(&s5k5ca_sem);

	return rc;
}

int s5k5ca_sensor_release(void)
{
	int rc = -EBADF;

	down(&s5k5ca_sem);

	s5k5ca_power_down();

    s5k5ca_sensor_init_done(s5k5ca_ctrl->sensordata);

	kfree(s5k5ca_ctrl);

	up(&s5k5ca_sem);
	CDBG("s5k5ca_release completed!\n");
	return rc;
}

static int s5k5ca_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	s5k5casensorw =
		kzalloc(sizeof(struct s5k5ca_work_t), GFP_KERNEL);

	if (!s5k5casensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k5casensorw);
	s5k5ca_init_client(client);
	s5k5ca_client = client;
	//s5k5ca_client->addr = s5k5ca_client->addr >> 1;
	mdelay(50);

	CDBG("i2c probe ok\n");
	return 0;

probe_failure:
	kfree(s5k5casensorw);
	s5k5casensorw = NULL;
	pr_err("i2c probe failure %d\n", rc);
	return rc;
}

static const struct i2c_device_id s5k5ca_i2c_id[] = {
	{ "s5k5ca", 0},
	{ }
};

static struct i2c_driver s5k5ca_i2c_driver = {
	.id_table = s5k5ca_i2c_id,
	.probe  = s5k5ca_i2c_probe,
	.remove = __exit_p(s5k5ca_i2c_remove),
	.driver = {
		.name = "s5k5ca",
	},
};

static int s5k5ca_sensor_probe(
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	/* We expect this driver to match with the i2c device registered
	 * in the board file immediately. */
	int rc = i2c_add_driver(&s5k5ca_i2c_driver);
	if (rc < 0 || s5k5ca_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(S5K5CA_DEFAULT_CLOCK_RATE);
	mdelay(20);

	rc = s5k5ca_probe_init_sensor(info);
	if (rc < 0) {
		i2c_del_driver(&s5k5ca_i2c_driver);
		goto probe_done;
	}

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CAMERA_MAIN);
    #endif


	s->s_init = s5k5ca_sensor_open_init;
	s->s_release = s5k5ca_sensor_release;
	s->s_config  = s5k5ca_sensor_config;
	s5k5ca_sensor_init_done(info);
	set_camera_support(true);
probe_done:
	return rc;
}

static int __s5k5ca_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, s5k5ca_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __s5k5ca_probe,
	.driver = {
		.name = "msm_camera_s5k5ca",
		.owner = THIS_MODULE,
	},
};

static int __init s5k5ca_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(s5k5ca_init);
