
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "s5k4cdgx.h"
#include "linux/hardware_self_adapt.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "s5k4cdgx.c: " fmt, ## args)

#define S5K4CDGX_DEFAULT_CLOCK_RATE	24000000

typedef enum
{
  CAMERA_AF_OFF,
  CAMERA_AF_ON,
} camera_af_mode;

#define S5K4CDGX_CHIP_ID   0x4cd
#define S5K4CDGX_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define MODEL_LITEON 0
#define MODEL_BYD 1
#define MODEL_SUNNY 2
#define MODEL_FOXCOM 3

static camera_af_mode s5k4cdgx_af_mode = CAMERA_AF_ON;
const static char s5k4cdgx_supported_effect[] = "none,mono,negative,sepia,aqua";
static uint16_t s5k4cdgx_model_id = MODEL_LITEON;
static struct  s5k4cdgx_work_t *s5k4cdgxsensorw = NULL;
static struct  i2c_client *s5k4cdgx_client = NULL;
static struct s5k4cdgx_ctrl_t *s5k4cdgx_ctrl = NULL;
static enum s5k4cdgx_reg_update_t last_rupdate = -1;
static enum s5k4cdgx_setting_t last_rt = -1;
static DECLARE_WAIT_QUEUE_HEAD(s5k4cdgx_wait_queue);
DECLARE_MUTEX(s5k4cdgx_sem);
DECLARE_MUTEX(s5k4cdgx_sem2);

//================================================================================================
//	Run capture
//================================================================================================
const static struct s5k4cdgx_i2c_reg_conf s5k4cd_effect_off_reg_config[] = {
	{0x0028,0x7000},
	{0x002A,0x0286},
	{0x0F12,0x0000}
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cd_effect_Mono_reg_config[] = {
	{0x0028,0x7000},
	{0x002A,0x0286},
	{0x0F12,0x0001}
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cd_effect_neg_reg_config[] = {
	{0x0028,0x7000},
	{0x002A,0x0286},
	{0x0F12,0x0002} //neg color 0x0003
};

const static struct s5k4cdgx_i2c_reg_conf s5k4cd_effect_sepia_reg_config[] = {
	{0x0028,0x7000},
	{0x002A,0x0286},
	{0x0F12,0x0004} 
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cd_effect_aqua_reg_config[] = {
	{0x0028,0x7000},
	{0x002A,0x0286},
	{0x0F12,0x0005}
};

const static struct s5k4cdgx_i2c_reg_conf s5k4cd_wb_auto_reg_config[] = {
		{0x0028,0x7000},
		{0x002A,0x052E},
		{0x0F12,0x077F}

};

const static struct s5k4cdgx_i2c_reg_conf 	s5k4cd_wb_incabd_reg_config[] = {
		{0x0028,0x7000},
		{0x002A,0x052E},
		{0x0F12,0x0777},
		{0x002A,0x04FA},
		{0x0F12,0x0380},//Rgain
		{0x0F12,0x0001},
		{0x0F12,0x0400},//Ggain
		{0x0F12,0x0001},
		{0x0F12,0x09C0},//Bgain
		{0x0F12,0x0001}
};

const static struct s5k4cdgx_i2c_reg_conf s5k4cd_wb_fluore_reg_config[] = {
		{0x0028,0x7000},
		{0x002A,0x052E},
		{0x0F12,0x0777},
		{0x002A,0x04FA},
		{0x0F12,0x0400},//Rgain
		{0x0F12,0x0001},
		{0x0F12,0x0400},//Ggain
		{0x0F12,0x0001},
		{0x0F12,0x083C},//Bgain
		{0x0F12,0x0001}

};
const static struct s5k4cdgx_i2c_reg_conf s5k4cd_wb_daylight_reg_config[] = {
		{0x0028,0x7000},
		{0x002A,0x052E},
		{0x0F12,0x0777},
		{0x002A,0x04FA},
		{0x0F12,0x05A0},//Rgain
		{0x0F12,0x0001},
		{0x0F12,0x0400},//Ggain
		{0x0F12,0x0001},
		{0x0F12,0x05F0},//Bgain
		{0x0F12,0x0001}

};

const static struct s5k4cdgx_i2c_reg_conf s5k4cd_wb_clouday_reg_config[] = {
		{0x0028,0x7000},
		{0x002A,0x052E},
		{0x0F12,0x0777},
		{0x002A,0x04FA},
		{0x0F12,0x0540},//Rgain
		{0x0F12,0x0001},
		{0x0F12,0x0400},//Ggain
		{0x0F12,0x0001},
		{0x0F12,0x0500},//Bgain
		{0x0F12,0x0001}

};

const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_snapshot_reg_config[] ={
{0x0028, 0x7000},
{0x002a, 0x02b4}, 
{0x0F12, 0x0000},  
{0x002a, 0x0298}, 
{0x0F12, 0x0001},  
{0x002a, 0x02b6}, 
{0x0F12, 0x0001},  
{0x002a, 0x028C}, 
{0x0F12, 0x0001},  
{0x0F12, 0x0001}, 
                                                                                                                                                        
//Active first capture config.                                                                                                                          
//WRITE	#REG_TC_GP_ActiveCapConfig 		0000 //0 // capture configuration 										
//WRITE 	#REG_TC_GP_NewConfigSync		0001 // update configuration                                                                            
//WRITE 	#REG_TC_GP_CapConfigChanged 		0001 													
//WRITE 	#REG_TC_GP_EnableCapture  		0001 //  capture                                                                                        
//WRITE 	#REG_TC_GP_EnableCaptureChanged		0001 //                                                                                                 
      
{0x0028, 0x7000},
{0x002A, 0x02B4},   // capture configuration 
{0x0F12, 0x0000},
{0x002A, 0x0298},   // update configuration  
{0x0F12, 0x0001},  
{0x002A, 0x02B6},  //REG_TC_GP_CapConfigChanged    
{0x0F12, 0x0001},
{0x002A, 0x028C}, //REG_TC_GP_EnableCapture  
{0x0F12, 0x0001},
{0x002A, 0x028E},  //REG_TC_GP_EnableCaptureChanged
{0x0F12, 0x0001},                                                                                                                                        

};

const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_before_preview_reg_config[] =
{
{0x0028 , 0x7000},                                                                                                                       
{0x002A , 0x0F30},
//change home position,reduce the AF module current
{0x0F12 , 0x0000},
{0x0F12 , 0x0000},
{0x002A , 0x02C2},
{0x0F12 , 0x0003},
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_preview_reg_config[] =
{
//================================================================================================
// APPLY PREVIEW CONFIGURATION & RUN PREVIEW
//================================================================================================
// preview // 10~15FPS
{0x0028 , 0x7000},                                                                                                                       
{0x002A , 0x0302},
{0x0F12 , 0x03E8},	// #REG_0TC_PCFG_usMaxFrTimeMsecMult10	: 10fps
{0x0F12 , 0x029A},	// #REG_0TC_PCFG_usMinFrTimeMsecMult10	: 15fps 

{0x002A , 0x02AC}, //#REG_TC_GP_ActivePrevConfig                                       
{0x0F12 , 0x0000}, 
{0x002A , 0x02B0}, //#REG_TC_GP_PrevOpenAfterChange   
{0x0F12 , 0x0001},     
{0x002A , 0x0298}, //#REG_TC_GP_NewConfigSync                                       
{0x0F12 , 0x0001},        
{0x002A , 0x02AE}, //#REG_TC_GP_PrevConfigChanged                                        
{0x0F12 , 0x0001}, 

};

//VERSION 4
//=================================================================================================
//	* Name		:	4CDGX EVT1.0 Initial Setfile
//	* PLL mode	:	MCLK=24MHz / SYSCLK=30MHz / PCLK=60MHz
//	* FPS		:	Preview YUV 640X480 15~7.5fps Capture YUV 2048x1536 7.5fps
//	* Made by	:	SYS.LSI Sang-il Park
//	* Date		:	2009.11.03
//	* History
//						: 09.11.03	Initial draft (based LG-SB210 tuning value)
//						: 09.11.06	Changed GAS LUT & Alpha & AFIT for Shading compensation
//=================================================================================================

//=================================================================================================
//	ARM Go
//=================================================================================================
const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_init_reg_config1[] ={
// Direct mode
{0xFCFC , 0xD000},
{0x0010 , 0x0001},
{0xFCFC , 0xD000},
{0x1030 , 0x0000},
{0xFCFC , 0xD000},
{0x0014 , 0x0001},
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_init_reg_config2[] ={
//p20								
// Set IO driving current
{0x0028 , 0xD000},
{0x002A , 0x1082},
{0x0F12 , 0x0155}, // [9:8] D4, [7:6] D3, [5:4] D2, [3:2] D1, [1:0] D0
{0x0F12 , 0x0155}, // [9:8] D9, [7:6] D8, [5:4] D7, [3:2] D6, [1:0] D5
{0x0F12 , 0x00A9}, // [5:4] GPIO3, [3:2] GPIO2, [1:0] GPIO1
{0x0F12 , 0x0555}, // [11:10] SDA, [9:8] SCA, [7:6] PCLK, [3:2] VSYNC, [1:0] HSYNC
//=================================================================================================
//	Trap & Patch
//	(need in case of using standby mode)
//=================================================================================================
{0x0028 , 0x7000},
{0x002A , 0x28C8},
{0x0F12 , 0xB570},
{0x0F12 , 0x4C15},
{0x0F12 , 0x2607},
{0x0F12 , 0x6821},
{0x0F12 , 0x0736},
{0x0F12 , 0x42B1},
{0x0F12 , 0xDA05},
{0x0F12 , 0x4813},
{0x0F12 , 0x22D8},
{0x0F12 , 0x1C05},
{0x0F12 , 0xF000},
{0x0F12 , 0xF854},
{0x0F12 , 0x6025},
{0x0F12 , 0x68A1},
{0x0F12 , 0x42B1},
{0x0F12 , 0xDA07},
{0x0F12 , 0x480E},
{0x0F12 , 0x2224},
{0x0F12 , 0x3824},
{0x0F12 , 0xF000},
{0x0F12 , 0xF84B},
{0x0F12 , 0x480C},
{0x0F12 , 0x3824},
{0x0F12 , 0x60A0},
{0x0F12 , 0x4C0B},
{0x0F12 , 0x6961},
{0x0F12 , 0x42B1},
{0x0F12 , 0xDA07},
{0x0F12 , 0x4808},
{0x0F12 , 0x228F},
{0x0F12 , 0x00D2},
{0x0F12 , 0x30D8},
{0x0F12 , 0x1C05},
{0x0F12 , 0xF000},
{0x0F12 , 0xF83D},
{0x0F12 , 0x6165},
{0x0F12 , 0x4906},
{0x0F12 , 0x4807},
{0x0F12 , 0x2200},
{0x0F12 , 0xF000},
{0x0F12 , 0xF83D},
{0x0F12 , 0xBC70},
{0x0F12 , 0xBC08},
{0x0F12 , 0x4718},
{0x0F12 , 0x06D8},
{0x0F12 , 0x7000},
{0x0F12 , 0x33A4},
{0x0F12 , 0x7000},
{0x0F12 , 0x0778},
{0x0F12 , 0x7000},
{0x0F12 , 0x2935},
{0x0F12 , 0x7000},
{0x0F12 , 0xF00F},
{0x0F12 , 0x0000},
{0x0F12 , 0xB5F8},
{0x0F12 , 0x1C04},
{0x0F12 , 0x2001},
{0x0F12 , 0x1C05},
{0x0F12 , 0x1C21},
{0x0F12 , 0x3910},
{0x0F12 , 0x4088},
{0x0F12 , 0x1C06},
{0x0F12 , 0x40A5},
{0x0F12 , 0x4F0E},
{0x0F12 , 0x2C10},
{0x0F12 , 0xDA03},
{0x0F12 , 0x8838},
{0x0F12 , 0x43A8},
{0x0F12 , 0x8038},
{0x0F12 , 0xE002},
{0x0F12 , 0x8878},
{0x0F12 , 0x43B0},
{0x0F12 , 0x8078},
{0x0F12 , 0xF000},
{0x0F12 , 0xF823},
{0x0F12 , 0x4909},
{0x0F12 , 0x2000},
{0x0F12 , 0x8188},
{0x0F12 , 0x80C8},
{0x0F12 , 0x2C10},
{0x0F12 , 0xDA05},
{0x0F12 , 0x8838},
{0x0F12 , 0x4328},
{0x0F12 , 0x8038},
{0x0F12 , 0xBCF8},
{0x0F12 , 0xBC08},
{0x0F12 , 0x4718},
{0x0F12 , 0x8878},
{0x0F12 , 0x4330},
{0x0F12 , 0x8078},
{0x0F12 , 0xE7F8},
{0x0F12 , 0x0000},
{0x0F12 , 0x1100},
{0x0F12 , 0xD000},
{0x0F12 , 0x0060},
{0x0F12 , 0xD000},
{0x0F12 , 0x4778},
{0x0F12 , 0x46C0},
{0x0F12 , 0xF004},
{0x0F12 , 0xE51F},
{0x0F12 , 0xFA28},
{0x0F12 , 0x0000},
{0x0F12 , 0x4778},
{0x0F12 , 0x46C0},
{0x0F12 , 0xC000},
{0x0F12 , 0xE59F},
{0x0F12 , 0xFF1C},
{0x0F12 , 0xE12F},
{0x0F12 , 0xF563},
{0x0F12 , 0x0000},
{0x0F12 , 0x4778},
{0x0F12 , 0x46C0},
{0x0F12 , 0xC000},
{0x0F12 , 0xE59F},
{0x0F12 , 0xFF1C},
{0x0F12 , 0xE12F},
{0x0F12 , 0xF00F},
{0x0F12 , 0x0000},

//================================================================================================
//	AF Initialize
//================================================================================================
{0x0028 , 0x7000},
{0x002A	, 0x0242},//#REG_TC_IPRM_CM_Init_AfModeType   
{0x0F12 , 0x0003},
{0x002A	, 0x0248},// #REG_TC_IPRM_CM_Init_GpioConfig1    
{0x0F12 , 0x0021},
{0x002A	, 0x0250},//#REG_TC_IPRM_CM_Init_Mi2cBits         
{0x0F12 , 0x450C},
{0x0F12 , 0x0190},////#REG_TC_IPRM_CM_Init_Mi2cRateKhz

//delete focus window setting
//{0x002A ,0x02CA},
//{0x0F12 ,0x0100},
//{0x0F12 ,0x00E3},
//{0x0F12 ,0x0200},
//{0x0F12 ,0x0238},
//{0x0F12 ,0x018C},
//{0x0F12 ,0x0166},
//{0x0F12 ,0x00E6},
//{0x0F12 ,0x0132},
//{0x0F12 ,0x0001},

// AF searching position table
{0x0028 ,0x7000},
{0x002A ,0x0F44},
{0x0F12 ,0x000C},

// AF searching position table
{0x0028, 0x7000},
{0x002A, 0x0F46},
{0x0F12 ,0x0000},	// index 0
{0x0F12 ,0x0038},	// index 1
{0x0F12 ,0x0060},	// index 2
{0x0F12 ,0x0068},	// index 3
{0x0F12 ,0x006E},	// index 4
{0x0F12 ,0x0074},	// index 5
{0x0F12 ,0x007A},	// index 6
{0x0F12 ,0x0080},	// index 7
{0x0F12 ,0x0086},	// index 8
{0x0F12 ,0x008C},	// index 9
{0x0F12 ,0x0092},	// index 10
{0x0F12 ,0x0098},	// index 11
{0x0F12 ,0x00A0},	// index 12
{0x0F12 ,0x0000},	// index 13
{0x0F12 ,0x0000},	// index 14
{0x0F12 ,0x0000},	// index 15
{0x0F12 ,0x0000},	// index 16

// Min. peak detection number
{0x0028 ,0x7000},
{0x002A ,0x0FA8},
{0x0F12 ,0x0003},
//
{0x002A ,0x0F30},
{0x0F12 ,0x0003},
{0x0F12 ,0x0003},
//add focus windows
{0x002A ,0x1084},
{0x0F12 ,0x03B8},
{0x0F12 ,0x03CC},
{0x0F12 ,0x03AA},
{0x002A ,0x1036},
{0x0F12 ,0x4040},
{0x0F12 ,0x9090},

//=================================================================================================
//	Set CIS/APS/Analog
//=================================================================================================
// This registers are for FACTORY ONLY. If you change it without prior notification 
// YOU are RESPONSIBLE for the FAILURE that will happen in the future.
//=================================================================================================
{0x0028, 0xD000},
{0x002A, 0xF52E},	// aig_gain_offset_3 (ADC_SAT level)
{0x0F12, 0x0023},              
{0x002A, 0xF536},	// aig_ref_ramp_1
{0x0F12, 0x0619},
{0x0028, 0x7000},	// #start add MSW
{0x002A, 0x1166},	// #start add LSW of senHal_ContPtrs_pSenModesRegsArray
{0x0F12, 0x0006},	// #senHal_ContPtrs_pSenModesRegsArray[0][0]
{0x0F12, 0x0006},	// #senHal_ContPtrs_pSenModesRegsArray[0][1]
{0x0F12, 0x0866},	// #senHal_ContPtrs_pSenModesRegsArray[1][0]
{0x0F12, 0x0866},	// #senHal_ContPtrs_pSenModesRegsArray[1][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[2][0]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[2][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[3][0]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[3][1]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[4][0]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[4][1]
{0x0F12, 0x084C},	// #senHal_ContPtrs_pSenModesRegsArray[5][0]
{0x0F12, 0x0368},	// #senHal_ContPtrs_pSenModesRegsArray[5][1]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[6][0]
{0x0F12, 0x0364},	// #senHal_ContPtrs_pSenModesRegsArray[6][1]
{0x0F12, 0x084C},	// #senHal_ContPtrs_pSenModesRegsArray[7][0]
{0x0F12, 0x06C4},	// #senHal_ContPtrs_pSenModesRegsArray[7][1]
{0x0F12, 0x001E},	// #senHal_ContPtrs_pSenModesRegsArray[8][0]
{0x0F12, 0x001E},	// #senHal_ContPtrs_pSenModesRegsArray[8][1]
{0x0F12, 0x084C},	// #senHal_ContPtrs_pSenModesRegsArray[9][0]
{0x0F12, 0x0364},	// #senHal_ContPtrs_pSenModesRegsArray[9][1]
{0x0F12, 0x001E},	// #senHal_ContPtrs_pSenModesRegsArray[10][0]
{0x0F12, 0x037E},	// #senHal_ContPtrs_pSenModesRegsArray[10][1]
{0x0F12, 0x084C},	// #senHal_ContPtrs_pSenModesRegsArray[11][0]
{0x0F12, 0x06C4},	// #senHal_ContPtrs_pSenModesRegsArray[11][1]
{0x0F12, 0x01FC},	// #senHal_ContPtrs_pSenModesRegsArray[12][0]
{0x0F12, 0x0138},	// #senHal_ContPtrs_pSenModesRegsArray[12][1]
{0x0F12, 0x0256},	// #senHal_ContPtrs_pSenModesRegsArray[13][0]
{0x0F12, 0x0192},	// #senHal_ContPtrs_pSenModesRegsArray[13][1]
{0x0F12, 0x01FC},	// #senHal_ContPtrs_pSenModesRegsArray[14][0]
{0x0F12, 0x0498},	// #senHal_ContPtrs_pSenModesRegsArray[14][1]
{0x0F12, 0x0256},	// #senHal_ContPtrs_pSenModesRegsArray[15][0]
{0x0F12, 0x04F2},	// #senHal_ContPtrs_pSenModesRegsArray[15][1]
{0x0F12, 0x0256},	// #senHal_ContPtrs_pSenModesRegsArray[16][0]
{0x0F12, 0x0192},	// #senHal_ContPtrs_pSenModesRegsArray[16][1]
{0x0F12, 0x084A},	// #senHal_ContPtrs_pSenModesRegsArray[17][0]
{0x0F12, 0x0362},	// #senHal_ContPtrs_pSenModesRegsArray[17][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[18][0]
{0x0F12, 0x04F2},	// #senHal_ContPtrs_pSenModesRegsArray[18][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[19][0]
{0x0F12, 0x06C2},	// #senHal_ContPtrs_pSenModesRegsArray[19][1]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[20][0]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[20][1]
{0x0F12, 0x01F8},	// #senHal_ContPtrs_pSenModesRegsArray[21][0]
{0x0F12, 0x0134},	// #senHal_ContPtrs_pSenModesRegsArray[21][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[22][0]
{0x0F12, 0x0366},	// #senHal_ContPtrs_pSenModesRegsArray[22][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[23][0]
{0x0F12, 0x0494},	// #senHal_ContPtrs_pSenModesRegsArray[23][1]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[24][0]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[24][1]
{0x0F12, 0x00DC},	// #senHal_ContPtrs_pSenModesRegsArray[25][0]
{0x0F12, 0x00DC},	// #senHal_ContPtrs_pSenModesRegsArray[25][1]
{0x0F12, 0x025E},	// #senHal_ContPtrs_pSenModesRegsArray[26][0]
{0x0F12, 0x0192},	// #senHal_ContPtrs_pSenModesRegsArray[26][1]
{0x0F12, 0x029A},	// #senHal_ContPtrs_pSenModesRegsArray[27][0]
{0x0F12, 0x01D6},	// #senHal_ContPtrs_pSenModesRegsArray[27][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[28][0]
{0x0F12, 0x0380},	// #senHal_ContPtrs_pSenModesRegsArray[28][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[29][0]
{0x0F12, 0x0402},	// #senHal_ContPtrs_pSenModesRegsArray[29][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[30][0]
{0x0F12, 0x04FA},	// #senHal_ContPtrs_pSenModesRegsArray[30][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[31][0]
{0x0F12, 0x0536},	// #senHal_ContPtrs_pSenModesRegsArray[31][1]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[32][0]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[32][1]
{0x0F12, 0x007A},	// #senHal_ContPtrs_pSenModesRegsArray[33][0]
{0x0F12, 0x007A},	// #senHal_ContPtrs_pSenModesRegsArray[33][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[34][0]
{0x0F12, 0x0380},	// #senHal_ContPtrs_pSenModesRegsArray[34][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[35][0]
{0x0F12, 0x03DA},	// #senHal_ContPtrs_pSenModesRegsArray[35][1]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[36][0]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[36][1]
{0x0F12, 0x00DC},	// #senHal_ContPtrs_pSenModesRegsArray[37][0]
{0x0F12, 0x00DC},	// #senHal_ContPtrs_pSenModesRegsArray[37][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[38][0]
{0x0F12, 0x0380},	// #senHal_ContPtrs_pSenModesRegsArray[38][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[39][0]
{0x0F12, 0x0402},	// #senHal_ContPtrs_pSenModesRegsArray[39][1]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[40][0]
{0x0F12, 0x005A},	// #senHal_ContPtrs_pSenModesRegsArray[40][1]
{0x0F12, 0x01F8},	// #senHal_ContPtrs_pSenModesRegsArray[41][0]
{0x0F12, 0x0134},	// #senHal_ContPtrs_pSenModesRegsArray[41][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[42][0]
{0x0F12, 0x0380},	// #senHal_ContPtrs_pSenModesRegsArray[42][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[43][0]
{0x0F12, 0x0494},	// #senHal_ContPtrs_pSenModesRegsArray[43][1]
{0x0F12, 0x02A2},	// #senHal_ContPtrs_pSenModesRegsArray[44][0]
{0x0F12, 0x01DE},	// #senHal_ContPtrs_pSenModesRegsArray[44][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[45][0]
{0x0F12, 0x0366},	// #senHal_ContPtrs_pSenModesRegsArray[45][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[46][0]
{0x0F12, 0x053E},	// #senHal_ContPtrs_pSenModesRegsArray[46][1]
{0x0F12, 0x00E4},	// #senHal_ContPtrs_pSenModesRegsArray[47][0]
{0x0F12, 0x00E4},	// #senHal_ContPtrs_pSenModesRegsArray[47][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[48][0]
{0x0F12, 0x0366},	// #senHal_ContPtrs_pSenModesRegsArray[48][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[49][0]
{0x0F12, 0x040A},	// #senHal_ContPtrs_pSenModesRegsArray[49][1]
{0x0F12, 0x00E4},	// #senHal_ContPtrs_pSenModesRegsArray[50][0]
{0x0F12, 0x00E4},	// #senHal_ContPtrs_pSenModesRegsArray[50][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[51][0]
{0x0F12, 0x0366},	// #senHal_ContPtrs_pSenModesRegsArray[51][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[52][0]
{0x0F12, 0x040A},	// #senHal_ContPtrs_pSenModesRegsArray[52][1]
{0x0F12, 0x00F0},	// #senHal_ContPtrs_pSenModesRegsArray[53][0]
{0x0F12, 0x00EF},	// #senHal_ContPtrs_pSenModesRegsArray[53][1]
{0x0F12, 0x01F0},	// #senHal_ContPtrs_pSenModesRegsArray[54][0]
{0x0F12, 0x012F},	// #senHal_ContPtrs_pSenModesRegsArray[54][1]
{0x0F12, 0x02C2},	// #senHal_ContPtrs_pSenModesRegsArray[55][0]
{0x0F12, 0x01FD},	// #senHal_ContPtrs_pSenModesRegsArray[55][1]
{0x0F12, 0x0842},	// #senHal_ContPtrs_pSenModesRegsArray[56][0]
{0x0F12, 0x035D},	// #senHal_ContPtrs_pSenModesRegsArray[56][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[57][0]
{0x0F12, 0x044F},	// #senHal_ContPtrs_pSenModesRegsArray[57][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[58][0]
{0x0F12, 0x048F},	// #senHal_ContPtrs_pSenModesRegsArray[58][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[59][0]
{0x0F12, 0x055D},	// #senHal_ContPtrs_pSenModesRegsArray[59][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[60][0]
{0x0F12, 0x06BD},	// #senHal_ContPtrs_pSenModesRegsArray[60][1]
{0x0F12, 0x01FE},	// #senHal_ContPtrs_pSenModesRegsArray[61][0]
{0x0F12, 0x013A},	// #senHal_ContPtrs_pSenModesRegsArray[61][1]
{0x0F12, 0x022A},	// #senHal_ContPtrs_pSenModesRegsArray[62][0]
{0x0F12, 0x0166},	// #senHal_ContPtrs_pSenModesRegsArray[62][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[63][0]
{0x0F12, 0x0362},	// #senHal_ContPtrs_pSenModesRegsArray[63][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[64][0]
{0x0F12, 0x0378},	// #senHal_ContPtrs_pSenModesRegsArray[64][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[65][0]
{0x0F12, 0x049A},	// #senHal_ContPtrs_pSenModesRegsArray[65][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[66][0]
{0x0F12, 0x04C6},	// #senHal_ContPtrs_pSenModesRegsArray[66][1]
{0x0F12, 0x020A},	// #senHal_ContPtrs_pSenModesRegsArray[67][0]
{0x0F12, 0x0146},	// #senHal_ContPtrs_pSenModesRegsArray[67][1]
{0x0F12, 0x023E},	// #senHal_ContPtrs_pSenModesRegsArray[68][0]
{0x0F12, 0x017A},	// #senHal_ContPtrs_pSenModesRegsArray[68][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[69][0]
{0x0F12, 0x0368},	// #senHal_ContPtrs_pSenModesRegsArray[69][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[70][0]
{0x0F12, 0x037C},	// #senHal_ContPtrs_pSenModesRegsArray[70][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[71][0]
{0x0F12, 0x04A6},	// #senHal_ContPtrs_pSenModesRegsArray[71][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[72][0]
{0x0F12, 0x04DA},	// #senHal_ContPtrs_pSenModesRegsArray[72][1]
{0x0F12, 0x0216},	// #senHal_ContPtrs_pSenModesRegsArray[73][0]
{0x0F12, 0x0152},	// #senHal_ContPtrs_pSenModesRegsArray[73][1]
{0x0F12, 0x023E},	// #senHal_ContPtrs_pSenModesRegsArray[74][0]
{0x0F12, 0x017A},	// #senHal_ContPtrs_pSenModesRegsArray[74][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[75][0]
{0x0F12, 0x036C},	// #senHal_ContPtrs_pSenModesRegsArray[75][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[76][0]
{0x0F12, 0x037C},	// #senHal_ContPtrs_pSenModesRegsArray[76][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[77][0]
{0x0F12, 0x04B2},	// #senHal_ContPtrs_pSenModesRegsArray[77][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[78][0]
{0x0F12, 0x04DA},	// #senHal_ContPtrs_pSenModesRegsArray[78][1]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[79][0]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[79][1]
{0x0F12, 0x0011},	// #senHal_ContPtrs_pSenModesRegsArray[80][0]
{0x0F12, 0x0011},	// #senHal_ContPtrs_pSenModesRegsArray[80][1]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[81][0]
{0x0F12, 0x0004},	// #senHal_ContPtrs_pSenModesRegsArray[81][1]
{0x0F12, 0x0011},	// #senHal_ContPtrs_pSenModesRegsArray[82][0]
{0x0F12, 0x0011},	// #senHal_ContPtrs_pSenModesRegsArray[82][1]
{0x0F12, 0x01F8},	// #senHal_ContPtrs_pSenModesRegsArray[83][0]
{0x0F12, 0x0134},	// #senHal_ContPtrs_pSenModesRegsArray[83][1]
{0x0F12, 0x0200},	// #senHal_ContPtrs_pSenModesRegsArray[84][0]
{0x0F12, 0x013C},	// #senHal_ContPtrs_pSenModesRegsArray[84][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[85][0]
{0x0F12, 0x0362},	// #senHal_ContPtrs_pSenModesRegsArray[85][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[86][0]
{0x0F12, 0x036A},	// #senHal_ContPtrs_pSenModesRegsArray[86][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[87][0]
{0x0F12, 0x0494},	// #senHal_ContPtrs_pSenModesRegsArray[87][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[88][0]
{0x0F12, 0x049C},	// #senHal_ContPtrs_pSenModesRegsArray[88][1]
{0x0F12, 0x00EE},	// #senHal_ContPtrs_pSenModesRegsArray[89][0]
{0x0F12, 0x00EE},	// #senHal_ContPtrs_pSenModesRegsArray[89][1]
{0x0F12, 0x01F6},	// #senHal_ContPtrs_pSenModesRegsArray[90][0]
{0x0F12, 0x0132},	// #senHal_ContPtrs_pSenModesRegsArray[90][1]
{0x0F12, 0x02C0},	// #senHal_ContPtrs_pSenModesRegsArray[91][0]
{0x0F12, 0x01FC},	// #senHal_ContPtrs_pSenModesRegsArray[91][1]
{0x0F12, 0x0848},	// #senHal_ContPtrs_pSenModesRegsArray[92][0]
{0x0F12, 0x0360},	// #senHal_ContPtrs_pSenModesRegsArray[92][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[93][0]
{0x0F12, 0x044E},	// #senHal_ContPtrs_pSenModesRegsArray[93][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[94][0]
{0x0F12, 0x0492},	// #senHal_ContPtrs_pSenModesRegsArray[94][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[95][0]
{0x0F12, 0x055C},	// #senHal_ContPtrs_pSenModesRegsArray[95][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[96][0]
{0x0F12, 0x06C0},	// #senHal_ContPtrs_pSenModesRegsArray[96][1]
{0x0F12, 0x01F8},	// #senHal_ContPtrs_pSenModesRegsArray[97][0]
{0x0F12, 0x0134},	// #senHal_ContPtrs_pSenModesRegsArray[97][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[98][0]
{0x0F12, 0x0362},	// #senHal_ContPtrs_pSenModesRegsArray[98][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[99][0]
{0x0F12, 0x0494},	// #senHal_ContPtrs_pSenModesRegsArray[99][1]
{0x0F12, 0x0008},	// #senHal_ContPtrs_pSenModesRegsArray[100][0]
{0x0F12, 0x0008},	// #senHal_ContPtrs_pSenModesRegsArray[100][1]
{0x0F12, 0x2D90},	// #senHal_ContPtrs_pSenModesRegsArray[101][0]
{0x0F12, 0x2D90},	// #senHal_ContPtrs_pSenModesRegsArray[101][1]
{0x0F12, 0x6531},	// #senHal_ContPtrs_pSenModesRegsArray[102][0]
{0x0F12, 0x6531},	// #senHal_ContPtrs_pSenModesRegsArray[102][1]
{0x0F12, 0x3E5A},	// #senHal_ContPtrs_pSenModesRegsArray[103][0]
{0x0F12, 0x3E5A},	// #senHal_ContPtrs_pSenModesRegsArray[103][1]
{0x0F12, 0x1422},	// #senHal_ContPtrs_pSenModesRegsArray[104][0]
{0x0F12, 0x1422},	// #senHal_ContPtrs_pSenModesRegsArray[104][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[105][0]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[105][1]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[106][0]
{0x0F12, 0x0000},	// #senHal_ContPtrs_pSenModesRegsArray[106][1]
{0x002A, 0x1320},	// #gisp_dadlc_config
{0x0F12, 0xAAF0},

{0x002A, 0x0B32},	// #setot_usSetRomWaitStateThreshold4KHz
{0x0F12, 0xFFFF},
{0x002A, 0x13EE},	// #pll_uMaxDivFreqMhz
{0x0F12, 0x0001},
{0x002A, 0x0B2C},	// #setot_uOnlineClocksDiv40
{0x0F12, 0x0EA6},

{0x002A, 0x1368},
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[0]
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[1]
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[2]
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[3]
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[4]
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[5]
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[6]
{0x0F12, 0x2000},	// #gisp_msm_uGainNoBin[7]
{0x0F12, 0x2000},	// #gisp_msm_uGainBin[0]
{0x0F12, 0x2000},	// #gisp_msm_uGainBin[1]
{0x0F12, 0x2000},	// #gisp_msm_uGainBin[2]
{0x0F12, 0x2000},	// #gisp_msm_uGainBin[3]

{0x002A, 0x1152},	// #senHal_NExpLinesCheckFine
{0x0F12, 0x0001},
{0x002A, 0x05B2},	// #skl_usConfigStbySettings
{0x0F12, 0x0007},

{0x002A, 0x323C},	// #TuneHWRegs_gtg_aig_ref_ramp_1
{0x0F12, 0x0637},
{0x002a, 0x114E},
{0x0F12, 0x0AC0},	// #SenHal_ExpMinPixles : 2752 (added 0730)

// New MSM Config  for SNR
{0x002A,0x1390},	
{0x0F12,0x0100},	// #gisp_msm_NonLinearOfsInput_0_
{0x0F12,0x0200},	// #gisp_msm_NonLinearOfsInput_1_
{0x0F12,0x0300},	// #gisp_msm_NonLinearOfsInput_2_
{0x0F12,0x0400},	// #gisp_msm_NonLinearOfsInput_3_
{0x0F12,0x0500},	// #gisp_msm_NonLinearOfsInput_4_
{0x0F12,0x0800},	// #gisp_msm_NonLinearOfsInput_5_
{0x0F12,0x0000},	// #gisp_msm_NonLinearOfsOutput_0_
{0x0F12,0x0000},	// #gisp_msm_NonLinearOfsOutput_1_
{0x0F12,0x0001},	// #gisp_msm_NonLinearOfsOutput_2_
{0x0F12,0x0002},	// #gisp_msm_NonLinearOfsOutput_3_
{0x0F12,0x0003},	// #gisp_msm_NonLinearOfsOutput_4_
{0x0F12,0x0002},	// #gisp_msm_NonLinearOfsOutput_5_


//================================================================================================
//	Set Frame Rate
//================================================================================================
//	How to set
//	1. Exposure value
//			dec2hex((1 / (frame rate you want)) * 1000d * 500d)
//	2. Analog Digital gain
//			dec2hex((Analog gain you want) * 256d)
//================================================================================================
// Preview exposure time
{0x0028,0x7000},
{0x002A,0x0574},
{0x0F12,0x61A8},	// #lt_uMaxExp1		: 50ms
{0x0F12,0x0000},
{0x0F12,0xC350},	// #lt_uMaxExp2		: 100ms
{0x0F12,0x0000},
{0x002A,0x1408},
{0x0F12,0xC350},	// #lt_uMaxExp3		: 100ms
{0x0F12,0x0000},
// Capure exposure time
{0x002A,0x057C},
{0x0F12,0x5214},	// #lt_uCapMaxExp1	: 42ms
{0x0F12,0x0000},
{0x0F12,0x8280},	// #lt_uCapMaxExp2	: 66.8ms
{0x0F12,0x0000},
{0x002A,0x140C},
{0x0F12,0x8280},	// #lt_uCapMaxExp3	: 66.8ms
{0x0F12,0x0000},
// Aaxalog gain
{0x002A,0x0584},
{0x0F12,0x0240},	// #lt_uMaxAnGain1	: Analog gain1 = x2
{0x0F12,0x0600},	// #lt_uMaxAnGain2	: Analog gain2 = x6
{0x002A,0x1410},
{0x0F12,0x0600},	// #lt_uMaxAnGain3	: Analog gain3 = x6
// Dixgital gain
{0x002A,0x0588},
{0x0F12,0x0300},	// #lt_uMaxDigGain	: Digital gain = x3
{0x002A,0x114E},
{0x0F12,0x0AC0},	// #SenHal_ExpMinPixels	: 2752

//================================================================================================
//	Set PLL
//================================================================================================
//	How to set
//	1. MCLK
//		dec2hex(CLK you want) * 1000)
//	2. System CLK
//		dec2hex((CLK you want) * 1000 / 4)
//	3. PCLK
//		dec2hex((CLK you want) * 1000 / 4)
//================================================================================================
// MCLK : 24MHz
{0x002A,0x023C},
{0x0F12,0x5DC0},	// #REG_TC_IPRM_InClockLSBs
{0x0F12,0x0000},	// #REG_TC_IPRM_InClockMSBs
{0x002A,0x0256},
{0x0F12,0x0002},	// #REG_TC_IPRM_UseNPviClocks
// System CLK : 48MHz
{0x002A,0x025E},
{0x0F12,0x2710}, //2ee0 //1D4C	// #REG_TC_IPRM_OpClk4KHz_0
// Pixel CLK : 46M
{0x0F12,0x3A88}, //2CCC //3998	// #REG_TC_IPRM_MinOutRate4KHz_0
{0x0F12,0x3AA8}, //2CEC //3b98	// #REG_TC_IPRM_MaxOutRate4KHz_0
{0x0F12,0x2710}, //2ee0 //1D4C	// #REG_TC_IPRM_OpClk4KHz_1     
// Pixel CLK : 46M                                      
{0x0F12,0x3A88}, //2CCC //3998	// #REG_TC_IPRM_MinOutRate4KHz_1
{0x0F12,0x3AA8}, //2CEC //3b98	// #REG_TC_IPRM_MaxOutRate4KHz_1

{0x002A,0x0272},
{0x0F12,0x0001},	// #REG_TC_IPRM_InitParamsUpdated
//// Manual 60Hz flicker
//002a 0b76
//0f12 0001
//002A 052E
//0F12 075F
//002A 0514
//0F12 0002
//0F12 0001
{0x002A,0x052E},
{0x0F12,0x077F},
//================================================================================================
//	Set preview configuration0
//	# Preview foramt	: YUV422
//	# Preview size		: 64480
//	# Preview FPS		: 22-10fps
//================================================================================================
{0x002A, 0x02E6},
{0x0F12, 0x0400},	// #REG_0TC_PCFG_usWidth		: 640
{0x0F12, 0x0300},	// #REG_0TC_PCFG_usHeight		: 480
{0x0F12, 0x0005},	// #REG_0TC_PCFG_Format			: YUV output
{0x002A, 0x02FC},
{0x0F12, 0x0000},	// #REG_0TC_PCFG_uClockInd
{0x002A, 0x02EC},
{0x0F12, 0x3AA8}, 	// #REG_0TC_PCFG_usMaxOut4KHzRate
{0x0F12, 0x3A88}, 	// #REG_0TC_PCFG_usMinOut4KHzRate
{0x002A, 0x02F4},
{0x0F12, 0x0052}, 	// #REG_0TC_PCFG_PVIMask
{0x002A, 0x0300},
{0x0F12, 0x0000},	// #REG_0TC_PCFG_FrRateQualityType
{0x002A, 0x02FE},
{0x0F12, 0x0000},	// #REG_0TC_PCFG_usFrTimeType
{0x002A, 0x0302},
{0x0F12, 0x03E8}, // #REG_0TC_PCFG_usMaxFrTimeMsecMult10	: 7.5fps
{0x0F12, 0x01C6}, //	// #REG_0TC_PCFG_usMinFrTimeMsecMult10	: 15fps

 {0x002A,0x0310}, //edison: 
 {0x0F12,0x0003},		//preview mirro, 0,1,2,3 different directions
 {0x0F12,0x0003},  //Capture mirro, 0,1,2,3 different directions

//================================================================================================
//	Set capture configuration0
//	# Capture foramt	: YUV
//	# Capture size		: 2048X1536 
//	# Capture FPS			: 7.5 ~ 7.5fps
//================================================================================================
{0x002A, 0x03D6},
{0x0F12, 0x0000}, //1	// #REG_0TC_CCFG_uCaptureMode		: AE/AWB off
{0x0F12, 0x0800},	// #REG_0TC_CCFG_usWidth				: 2048
{0x0F12, 0x0600},	// #REG_0TC_CCFG_usHeight				: 1536
{0x0F12, 0x0005},	// #REG_0TC_CCFG_Format					: JPEG output
{0x0F12, 0x3AA8},	// #REG_0TC_CCFG_usMaxOut4KHzRate
{0x0F12, 0x3A88},	// #REG_0TC_CCFG_usMinOut4KHzRate
{0x002A, 0x03E6},
{0x0F12, 0x0052},	// #REG_0TC_CCFG_PVIMask
{0x002A, 0x03EE},
{0x0F12, 0x0001},       //#REG_0TC_CCFG_uClockInd 
//002A ,03F0
{0x0F12, 0x0000},	// #REG_0TC_CCFG_usFrTimeType
{0x0F12, 0x0002},	// #REG_0TC_CCFG_FrRateQualityType
{0x0F12, 0x0535},	// #REG_0TC_CCFG_usMaxFrTimeMsecMult10	: 7.5fps
{0x0F12, 0x0535},	// #REG_0TC_CCFG_usMinFrTimeMsecMult10	: 7.5fps





// Update preview & capture configuration
{0x002A, 0x02AC},
{0x0F12, 0x0000},	// #REG_TC_GP_ActivePrevConfig
{0x002A, 0x02B4},
{0x0F12, 0x0000},	// #REG_TC_GP_ActiveCapConfig
{0x002A, 0x02B0},
{0x0F12, 0x0001},	// #REG_TC_GP_PrevOpenAfterChange
{0x002A, 0x0298},
{0x0F12, 0x0001},	// #REG_TC_GP_NewConfigSync
{0x002A, 0x02AE},
{0x0F12, 0x0001},	// #REG_TC_GP_PrevConfigChanged
{0x002A, 0x02B6},
{0x0F12, 0x0001},	// #REG_TC_GP_CapConfigChanged

// Run preivew
{0x002A, 0x0288},
{0x0F12, 0x0001},	// #REG_TC_GP_EnablePreview
{0x002A, 0x028A},
{0x0F12, 0x0001},	// #REG_TC_GP_EnablePreviewChanged

// Set host interrupt so main start run
{0x0028, 0xD000},
{0x002A, 0x1000},
{0x0F12, 0x0001},
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_init_reg_config3[] ={
//p10


//0028 D000  // color bar
//002A 3100
//0F12 0002

//================================================================================================
//	Set JPEG option
//================================================================================================
// Set Q-value
{0x0028,0x7000},
{0x002A,0x04B6},
{0x0F12,0x0000},	// #REG_RC_BRC_BRC_type
{0x0F12,0x005A},	// #Preview_BRC : Super Fine (90d)
{0x0F12,0x005A},	// #Capture_BRC : Super Fine (90d)
//// Set thumbnail
//002A 04BC
//0F12 0001	// #REG_TC_THUMB_Thumb_bActive
//0F12 0140 // 0280	// #REG_TC_THUMB_Thumb_uWidth
//0F12 00F0 // 01E0	// #REG_TC_THUMB_Thumb_uHeight
//0F12 0000	// #REG_TC_THUMB_Thumb_Format
//// Set spoof size
//002A 03E8
//0F12 0050	// #REG_0TC_CCFG_OIFMask : SPOOF_EN + JPEG8
//0F12 03C0	// #REG_0TC_CCFG_usJpegPacketSize
//0F12 08FC	// #REG_0TC_CCFG_usJpegTotalPackets


//================================================================================================
//	Set GAS (Grid Anti-shading)
//================================================================================================
// Set GAS alpha
{0x002A, 0x0706},
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[0]	// Horizon
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[1]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[2]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[3]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[4]	// IncandA
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[5]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[6]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[7]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[8]	// WW
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[9]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[10]
{0x0F12, 0x00C0},	// #TVAR_ash_GASAlpha[11]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[12]	// CWF
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[13]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[14]
{0x0F12, 0x00E0},	// #TVAR_ash_GASAlpha[15]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[16]	// D50
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[17]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[18]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[19]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[20]	// D65
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[21]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[22]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[23]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[24]	// D75
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[25]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[26]
{0x0F12, 0x0100},	// #TVAR_ash_GASAlpha[27]
{0x0F12, 0x0100},	// #TVAR_ash_GASOutdoorAlph	// Outdoor
{0x0F12, 0x0100},	// #TVAR_ash_GASOutdoorAlph
{0x0F12, 0x0100},	// #TVAR_ash_GASOutdoorAlph
{0x0F12, 0x0100},	// #TVAR_ash_GASOutdoorAlph

//{0x002A, 0x0768}
//{0x0F12, 0x0001}
//002A 0786
//0F12 0001	// #ash_bLumaMode : off (Not use beta coeff)

//002A 0766
//0F12 0000	// #ash_GASBeta[4][0]

{0x002A, 0x0792},
{0x0F12, 0x0001},	// #ash_bParabolicEstimatio
{0x0F12, 0x044C},	// #ash_uParabolicCenterX
{0x0F12, 0x02E4},	// #ash_uParabolicCenterY
{0x0F12, 0x000D},	// #ash_uParabolicScalingA
{0x0F12, 0x0010},	// #ash_uParabolicScalingB

// Set GAS LUT
{0x002A, 0x347C},
{0x0F12, 0x016A},	// #TVAR_ash_pGAS[0]
{0x0F12, 0x011B},	// #TVAR_ash_pGAS[1]
{0x0F12, 0x00E9},	// #TVAR_ash_pGAS[2]
{0x0F12, 0x00C3},	// #TVAR_ash_pGAS[3]
{0x0F12, 0x00A8},	// #TVAR_ash_pGAS[4]
{0x0F12, 0x0099},	// #TVAR_ash_pGAS[5]
{0x0F12, 0x0094},	// #TVAR_ash_pGAS[6]
{0x0F12, 0x009D},	// #TVAR_ash_pGAS[7]
{0x0F12, 0x00B1},	// #TVAR_ash_pGAS[8]
{0x0F12, 0x00CF},	// #TVAR_ash_pGAS[9]
{0x0F12, 0x00F8},	// #TVAR_ash_pGAS[10]
{0x0F12, 0x0133},	// #TVAR_ash_pGAS[11]
{0x0F12, 0x019F},	// #TVAR_ash_pGAS[12]
{0x0F12, 0x012C},	// #TVAR_ash_pGAS[13]
{0x0F12, 0x00F0},	// #TVAR_ash_pGAS[14]
{0x0F12, 0x00C0},	// #TVAR_ash_pGAS[15]
{0x0F12, 0x009A},	// #TVAR_ash_pGAS[16]
{0x0F12, 0x007D},	// #TVAR_ash_pGAS[17]
{0x0F12, 0x0069},	// #TVAR_ash_pGAS[18]
{0x0F12, 0x0065},	// #TVAR_ash_pGAS[19]
{0x0F12, 0x0070},	// #TVAR_ash_pGAS[20]
{0x0F12, 0x0085},	// #TVAR_ash_pGAS[21]
{0x0F12, 0x00A7},	// #TVAR_ash_pGAS[22]
{0x0F12, 0x00D0},	// #TVAR_ash_pGAS[23]
{0x0F12, 0x0107},	// #TVAR_ash_pGAS[24]
{0x0F12, 0x014F},	// #TVAR_ash_pGAS[25]
{0x0F12, 0x0103},	// #TVAR_ash_pGAS[26]
{0x0F12, 0x00CC},	// #TVAR_ash_pGAS[27]
{0x0F12, 0x009D},	// #TVAR_ash_pGAS[28]
{0x0F12, 0x0074},	// #TVAR_ash_pGAS[29]
{0x0F12, 0x0052},	// #TVAR_ash_pGAS[30]
{0x0F12, 0x003E},	// #TVAR_ash_pGAS[31]
{0x0F12, 0x0038},	// #TVAR_ash_pGAS[32]
{0x0F12, 0x0043},	// #TVAR_ash_pGAS[33]
{0x0F12, 0x005C},	// #TVAR_ash_pGAS[34]
{0x0F12, 0x0081},	// #TVAR_ash_pGAS[35]
{0x0F12, 0x00AD},	// #TVAR_ash_pGAS[36]
{0x0F12, 0x00E3},	// #TVAR_ash_pGAS[37]
{0x0F12, 0x0121},	// #TVAR_ash_pGAS[38]
{0x0F12, 0x00EA},	// #TVAR_ash_pGAS[39]
{0x0F12, 0x00B4},	// #TVAR_ash_pGAS[40]
{0x0F12, 0x0082},	// #TVAR_ash_pGAS[41]
{0x0F12, 0x0055},	// #TVAR_ash_pGAS[42]
{0x0F12, 0x0033},	// #TVAR_ash_pGAS[43]
{0x0F12, 0x001D},	// #TVAR_ash_pGAS[44]
{0x0F12, 0x0018},	// #TVAR_ash_pGAS[45]
{0x0F12, 0x0022},	// #TVAR_ash_pGAS[46]
{0x0F12, 0x003C},	// #TVAR_ash_pGAS[47]
{0x0F12, 0x0064},	// #TVAR_ash_pGAS[48]
{0x0F12, 0x0094},	// #TVAR_ash_pGAS[49]
{0x0F12, 0x00CA},	// #TVAR_ash_pGAS[50]
{0x0F12, 0x0105},	// #TVAR_ash_pGAS[51]
{0x0F12, 0x00DC},	// #TVAR_ash_pGAS[52]
{0x0F12, 0x00A6},	// #TVAR_ash_pGAS[53]
{0x0F12, 0x0072},	// #TVAR_ash_pGAS[54]
{0x0F12, 0x0044},	// #TVAR_ash_pGAS[55]
{0x0F12, 0x0021},	// #TVAR_ash_pGAS[56]
{0x0F12, 0x000B},	// #TVAR_ash_pGAS[57]
{0x0F12, 0x0005},	// #TVAR_ash_pGAS[58]
{0x0F12, 0x0010},	// #TVAR_ash_pGAS[59]
{0x0F12, 0x002B},	// #TVAR_ash_pGAS[60]
{0x0F12, 0x0054},	// #TVAR_ash_pGAS[61]
{0x0F12, 0x0086},	// #TVAR_ash_pGAS[62]
{0x0F12, 0x00BE},	// #TVAR_ash_pGAS[63]
{0x0F12, 0x00F7},	// #TVAR_ash_pGAS[64]
{0x0F12, 0x00D7},	// #TVAR_ash_pGAS[65]
{0x0F12, 0x00A3},	// #TVAR_ash_pGAS[66]
{0x0F12, 0x006E},	// #TVAR_ash_pGAS[67]
{0x0F12, 0x0040},	// #TVAR_ash_pGAS[68]
{0x0F12, 0x001B},	// #TVAR_ash_pGAS[69]
{0x0F12, 0x0005},	// #TVAR_ash_pGAS[70]
{0x0F12, 0x0000},	// #TVAR_ash_pGAS[71]
{0x0F12, 0x000B},	// #TVAR_ash_pGAS[72]
{0x0F12, 0x0027},	// #TVAR_ash_pGAS[73]
{0x0F12, 0x0051},	// #TVAR_ash_pGAS[74]
{0x0F12, 0x0083},	// #TVAR_ash_pGAS[75]
{0x0F12, 0x00BC},	// #TVAR_ash_pGAS[76]
{0x0F12, 0x00F5},	// #TVAR_ash_pGAS[77]
{0x0F12, 0x00DE},	// #TVAR_ash_pGAS[78]
{0x0F12, 0x00A9},	// #TVAR_ash_pGAS[79]
{0x0F12, 0x0075},	// #TVAR_ash_pGAS[80]
{0x0F12, 0x0048},	// #TVAR_ash_pGAS[81]
{0x0F12, 0x0024},	// #TVAR_ash_pGAS[82]
{0x0F12, 0x000E},	// #TVAR_ash_pGAS[83]
{0x0F12, 0x0009},	// #TVAR_ash_pGAS[84]
{0x0F12, 0x0015},	// #TVAR_ash_pGAS[85]
{0x0F12, 0x0030},	// #TVAR_ash_pGAS[86]
{0x0F12, 0x005A},	// #TVAR_ash_pGAS[87]
{0x0F12, 0x008C},	// #TVAR_ash_pGAS[88]
{0x0F12, 0x00C7},	// #TVAR_ash_pGAS[89]
{0x0F12, 0x00FE},	// #TVAR_ash_pGAS[90]
{0x0F12, 0x00EE},	// #TVAR_ash_pGAS[91]
{0x0F12, 0x00BB},	// #TVAR_ash_pGAS[92]
{0x0F12, 0x0088},	// #TVAR_ash_pGAS[93]
{0x0F12, 0x005B},	// #TVAR_ash_pGAS[94]
{0x0F12, 0x003A},	// #TVAR_ash_pGAS[95]
{0x0F12, 0x0025},	// #TVAR_ash_pGAS[96]
{0x0F12, 0x0020},	// #TVAR_ash_pGAS[97]
{0x0F12, 0x002B},	// #TVAR_ash_pGAS[98]
{0x0F12, 0x0047},	// #TVAR_ash_pGAS[99]
{0x0F12, 0x0070},	// #TVAR_ash_pGAS[100]
{0x0F12, 0x00A3},	// #TVAR_ash_pGAS[101]
{0x0F12, 0x00D9},	// #TVAR_ash_pGAS[102]
{0x0F12, 0x0111},	// #TVAR_ash_pGAS[103]
{0x0F12, 0x010B},	// #TVAR_ash_pGAS[104]
{0x0F12, 0x00D5},	// #TVAR_ash_pGAS[105]
{0x0F12, 0x00A5},	// #TVAR_ash_pGAS[106]
{0x0F12, 0x007A},	// #TVAR_ash_pGAS[107]
{0x0F12, 0x005A},	// #TVAR_ash_pGAS[108]
{0x0F12, 0x0047},	// #TVAR_ash_pGAS[109]
{0x0F12, 0x0043},	// #TVAR_ash_pGAS[110]
{0x0F12, 0x004E},	// #TVAR_ash_pGAS[111]
{0x0F12, 0x0069},	// #TVAR_ash_pGAS[112]
{0x0F12, 0x0091},	// #TVAR_ash_pGAS[113]
{0x0F12, 0x00C0},	// #TVAR_ash_pGAS[114]
{0x0F12, 0x00F6},	// #TVAR_ash_pGAS[115]
{0x0F12, 0x0130},	// #TVAR_ash_pGAS[116]
{0x0F12, 0x0135},	// #TVAR_ash_pGAS[117]
{0x0F12, 0x00F7},	// #TVAR_ash_pGAS[118]
{0x0F12, 0x00C8},	// #TVAR_ash_pGAS[119]
{0x0F12, 0x00A3},	// #TVAR_ash_pGAS[120]
{0x0F12, 0x0086},	// #TVAR_ash_pGAS[121]
{0x0F12, 0x0074},	// #TVAR_ash_pGAS[122]
{0x0F12, 0x0071},	// #TVAR_ash_pGAS[123]
{0x0F12, 0x007D},	// #TVAR_ash_pGAS[124]
{0x0F12, 0x0096},	// #TVAR_ash_pGAS[125]
{0x0F12, 0x00BB},	// #TVAR_ash_pGAS[126]
{0x0F12, 0x00E7},	// #TVAR_ash_pGAS[127]
{0x0F12, 0x011B},	// #TVAR_ash_pGAS[128]
{0x0F12, 0x0161},	// #TVAR_ash_pGAS[129]
{0x0F12, 0x016C},	// #TVAR_ash_pGAS[130]
{0x0F12, 0x0121},	// #TVAR_ash_pGAS[131]
{0x0F12, 0x00F1},	// #TVAR_ash_pGAS[132]
{0x0F12, 0x00CF},	// #TVAR_ash_pGAS[133]
{0x0F12, 0x00B3},	// #TVAR_ash_pGAS[134]
{0x0F12, 0x00A4},	// #TVAR_ash_pGAS[135]
{0x0F12, 0x00A2},	// #TVAR_ash_pGAS[136]
{0x0F12, 0x00AC},	// #TVAR_ash_pGAS[137]
{0x0F12, 0x00C2},	// #TVAR_ash_pGAS[138]
{0x0F12, 0x00E5},	// #TVAR_ash_pGAS[139]
{0x0F12, 0x0110},	// #TVAR_ash_pGAS[140]
{0x0F12, 0x014A},	// #TVAR_ash_pGAS[141]
{0x0F12, 0x01B7},	// #TVAR_ash_pGAS[142]
{0x0F12, 0x013D},	// #TVAR_ash_pGAS[143]
{0x0F12, 0x00E7},	// #TVAR_ash_pGAS[144]
{0x0F12, 0x00B8},	// #TVAR_ash_pGAS[145]
{0x0F12, 0x0097},	// #TVAR_ash_pGAS[146]
{0x0F12, 0x0082},	// #TVAR_ash_pGAS[147]
{0x0F12, 0x0076},	// #TVAR_ash_pGAS[148]
{0x0F12, 0x0071},	// #TVAR_ash_pGAS[149]
{0x0F12, 0x0076},	// #TVAR_ash_pGAS[150]
{0x0F12, 0x0082},	// #TVAR_ash_pGAS[151]
{0x0F12, 0x0098},	// #TVAR_ash_pGAS[152]
{0x0F12, 0x00B9},	// #TVAR_ash_pGAS[153]
{0x0F12, 0x00ED},	// #TVAR_ash_pGAS[154]
{0x0F12, 0x0153},	// #TVAR_ash_pGAS[155]
{0x0F12, 0x00FC},	// #TVAR_ash_pGAS[156]
{0x0F12, 0x00C1},	// #TVAR_ash_pGAS[157]
{0x0F12, 0x0096},	// #TVAR_ash_pGAS[158]
{0x0F12, 0x0076},	// #TVAR_ash_pGAS[159]
{0x0F12, 0x0060},	// #TVAR_ash_pGAS[160]
{0x0F12, 0x0051},	// #TVAR_ash_pGAS[161]
{0x0F12, 0x004D},	// #TVAR_ash_pGAS[162]
{0x0F12, 0x0053},	// #TVAR_ash_pGAS[163]
{0x0F12, 0x0061},	// #TVAR_ash_pGAS[164]
{0x0F12, 0x0077},	// #TVAR_ash_pGAS[165]
{0x0F12, 0x0095},	// #TVAR_ash_pGAS[166]
{0x0F12, 0x00C3},	// #TVAR_ash_pGAS[167]
{0x0F12, 0x0108},	// #TVAR_ash_pGAS[168]
{0x0F12, 0x00D7},	// #TVAR_ash_pGAS[169]
{0x0F12, 0x00A4},	// #TVAR_ash_pGAS[170]
{0x0F12, 0x007B},	// #TVAR_ash_pGAS[171]
{0x0F12, 0x005A},	// #TVAR_ash_pGAS[172]
{0x0F12, 0x003F},	// #TVAR_ash_pGAS[173]
{0x0F12, 0x002F},	// #TVAR_ash_pGAS[174]
{0x0F12, 0x002A},	// #TVAR_ash_pGAS[175]
{0x0F12, 0x0030},	// #TVAR_ash_pGAS[176]
{0x0F12, 0x0043},	// #TVAR_ash_pGAS[177]
{0x0F12, 0x005D},	// #TVAR_ash_pGAS[178]
{0x0F12, 0x007C},	// #TVAR_ash_pGAS[179]
{0x0F12, 0x00A6},	// #TVAR_ash_pGAS[180]
{0x0F12, 0x00DE},	// #TVAR_ash_pGAS[181]
{0x0F12, 0x00C1},	// #TVAR_ash_pGAS[182]
{0x0F12, 0x0091},	// #TVAR_ash_pGAS[183]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[184]
{0x0F12, 0x0043},	// #TVAR_ash_pGAS[185]
{0x0F12, 0x0027},	// #TVAR_ash_pGAS[186]
{0x0F12, 0x0016},	// #TVAR_ash_pGAS[187]
{0x0F12, 0x0011},	// #TVAR_ash_pGAS[188]
{0x0F12, 0x0018},	// #TVAR_ash_pGAS[189]
{0x0F12, 0x002B},	// #TVAR_ash_pGAS[190]
{0x0F12, 0x0048},	// #TVAR_ash_pGAS[191]
{0x0F12, 0x006B},	// #TVAR_ash_pGAS[192]
{0x0F12, 0x0094},	// #TVAR_ash_pGAS[193]
{0x0F12, 0x00C7},	// #TVAR_ash_pGAS[194]
{0x0F12, 0x00B6},	// #TVAR_ash_pGAS[195]
{0x0F12, 0x0087},	// #TVAR_ash_pGAS[196]
{0x0F12, 0x005C},	// #TVAR_ash_pGAS[197]
{0x0F12, 0x0036},	// #TVAR_ash_pGAS[198]
{0x0F12, 0x001A},	// #TVAR_ash_pGAS[199]
{0x0F12, 0x0007},	// #TVAR_ash_pGAS[200]
{0x0F12, 0x0003},	// #TVAR_ash_pGAS[201]
{0x0F12, 0x000A},	// #TVAR_ash_pGAS[202]
{0x0F12, 0x001E},	// #TVAR_ash_pGAS[203]
{0x0F12, 0x003D},	// #TVAR_ash_pGAS[204]
{0x0F12, 0x0062},	// #TVAR_ash_pGAS[205]
{0x0F12, 0x008D},	// #TVAR_ash_pGAS[206]
{0x0F12, 0x00BC},	// #TVAR_ash_pGAS[207]
{0x0F12, 0x00B4},	// #TVAR_ash_pGAS[208]
{0x0F12, 0x0085},	// #TVAR_ash_pGAS[209]
{0x0F12, 0x005B},	// #TVAR_ash_pGAS[210]
{0x0F12, 0x0033},	// #TVAR_ash_pGAS[211]
{0x0F12, 0x0016},	// #TVAR_ash_pGAS[212]
{0x0F12, 0x0004},	// #TVAR_ash_pGAS[213]
{0x0F12, 0x0000},	// #TVAR_ash_pGAS[214]
{0x0F12, 0x0008},	// #TVAR_ash_pGAS[215]
{0x0F12, 0x001D},	// #TVAR_ash_pGAS[216]
{0x0F12, 0x003C},	// #TVAR_ash_pGAS[217]
{0x0F12, 0x0062},	// #TVAR_ash_pGAS[218]
{0x0F12, 0x008D},	// #TVAR_ash_pGAS[219]
{0x0F12, 0x00BD},	// #TVAR_ash_pGAS[220]
{0x0F12, 0x00BB},	// #TVAR_ash_pGAS[221]
{0x0F12, 0x008C},	// #TVAR_ash_pGAS[222]
{0x0F12, 0x0061},	// #TVAR_ash_pGAS[223]
{0x0F12, 0x003A},	// #TVAR_ash_pGAS[224]
{0x0F12, 0x001D},	// #TVAR_ash_pGAS[225]
{0x0F12, 0x000C},	// #TVAR_ash_pGAS[226]
{0x0F12, 0x0008},	// #TVAR_ash_pGAS[227]
{0x0F12, 0x0010},	// #TVAR_ash_pGAS[228]
{0x0F12, 0x0025},	// #TVAR_ash_pGAS[229]
{0x0F12, 0x0045},	// #TVAR_ash_pGAS[230]
{0x0F12, 0x006B},	// #TVAR_ash_pGAS[231]
{0x0F12, 0x0096},	// #TVAR_ash_pGAS[232]
{0x0F12, 0x00C5},	// #TVAR_ash_pGAS[233]
{0x0F12, 0x00CA},	// #TVAR_ash_pGAS[234]
{0x0F12, 0x009A},	// #TVAR_ash_pGAS[235]
{0x0F12, 0x0071},	// #TVAR_ash_pGAS[236]
{0x0F12, 0x004D},	// #TVAR_ash_pGAS[237]
{0x0F12, 0x0031},	// #TVAR_ash_pGAS[238]
{0x0F12, 0x001F},	// #TVAR_ash_pGAS[239]
{0x0F12, 0x001C},	// #TVAR_ash_pGAS[240]
{0x0F12, 0x0024},	// #TVAR_ash_pGAS[241]
{0x0F12, 0x003A},	// #TVAR_ash_pGAS[242]
{0x0F12, 0x0059},	// #TVAR_ash_pGAS[243]
{0x0F12, 0x007E},	// #TVAR_ash_pGAS[244]
{0x0F12, 0x00A7},	// #TVAR_ash_pGAS[245]
{0x0F12, 0x00D9},	// #TVAR_ash_pGAS[246]
{0x0F12, 0x00E6},	// #TVAR_ash_pGAS[247]
{0x0F12, 0x00B1},	// #TVAR_ash_pGAS[248]
{0x0F12, 0x0088},	// #TVAR_ash_pGAS[249]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[250]
{0x0F12, 0x004C},	// #TVAR_ash_pGAS[251]
{0x0F12, 0x003D},	// #TVAR_ash_pGAS[252]
{0x0F12, 0x003B},	// #TVAR_ash_pGAS[253]
{0x0F12, 0x0043},	// #TVAR_ash_pGAS[254]
{0x0F12, 0x0058},	// #TVAR_ash_pGAS[255]
{0x0F12, 0x0076},	// #TVAR_ash_pGAS[256]
{0x0F12, 0x0098},	// #TVAR_ash_pGAS[257]
{0x0F12, 0x00C2},	// #TVAR_ash_pGAS[258]
{0x0F12, 0x00F7},	// #TVAR_ash_pGAS[259]
{0x0F12, 0x010D},	// #TVAR_ash_pGAS[260]
{0x0F12, 0x00D1},	// #TVAR_ash_pGAS[261]
{0x0F12, 0x00A5},	// #TVAR_ash_pGAS[262]
{0x0F12, 0x0089},	// #TVAR_ash_pGAS[263]
{0x0F12, 0x0073},	// #TVAR_ash_pGAS[264]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[265]
{0x0F12, 0x0064},	// #TVAR_ash_pGAS[266]
{0x0F12, 0x006D},	// #TVAR_ash_pGAS[267]
{0x0F12, 0x0080},	// #TVAR_ash_pGAS[268]
{0x0F12, 0x009A},	// #TVAR_ash_pGAS[269]
{0x0F12, 0x00BA},	// #TVAR_ash_pGAS[270]
{0x0F12, 0x00E5},	// #TVAR_ash_pGAS[271]
{0x0F12, 0x0127},	// #TVAR_ash_pGAS[272]
{0x0F12, 0x0141},	// #TVAR_ash_pGAS[273]
{0x0F12, 0x00F5},	// #TVAR_ash_pGAS[274]
{0x0F12, 0x00CB},	// #TVAR_ash_pGAS[275]
{0x0F12, 0x00AF},	// #TVAR_ash_pGAS[276]
{0x0F12, 0x009A},	// #TVAR_ash_pGAS[277]
{0x0F12, 0x008F},	// #TVAR_ash_pGAS[278]
{0x0F12, 0x008F},	// #TVAR_ash_pGAS[279]
{0x0F12, 0x0096},	// #TVAR_ash_pGAS[280]
{0x0F12, 0x00A7},	// #TVAR_ash_pGAS[281]
{0x0F12, 0x00BF},	// #TVAR_ash_pGAS[282]
{0x0F12, 0x00E0},	// #TVAR_ash_pGAS[283]
{0x0F12, 0x0114},	// #TVAR_ash_pGAS[284]
{0x0F12, 0x017B},	// #TVAR_ash_pGAS[285]
{0x0F12, 0x0143},	// #TVAR_ash_pGAS[286]
{0x0F12, 0x00ED},	// #TVAR_ash_pGAS[287]
{0x0F12, 0x00BA},	// #TVAR_ash_pGAS[288]
{0x0F12, 0x0097},	// #TVAR_ash_pGAS[289]
{0x0F12, 0x007F},	// #TVAR_ash_pGAS[290]
{0x0F12, 0x0073},	// #TVAR_ash_pGAS[291]
{0x0F12, 0x0070},	// #TVAR_ash_pGAS[292]
{0x0F12, 0x0077},	// #TVAR_ash_pGAS[293]
{0x0F12, 0x0089},	// #TVAR_ash_pGAS[294]
{0x0F12, 0x00A2},	// #TVAR_ash_pGAS[295]
{0x0F12, 0x00C8},	// #TVAR_ash_pGAS[296]
{0x0F12, 0x00FF},	// #TVAR_ash_pGAS[297]
{0x0F12, 0x016B},	// #TVAR_ash_pGAS[298]
{0x0F12, 0x0108},	// #TVAR_ash_pGAS[299]
{0x0F12, 0x00C8},	// #TVAR_ash_pGAS[300]
{0x0F12, 0x0099},	// #TVAR_ash_pGAS[301]
{0x0F12, 0x0077},	// #TVAR_ash_pGAS[302]
{0x0F12, 0x005F},	// #TVAR_ash_pGAS[303]
{0x0F12, 0x004F},	// #TVAR_ash_pGAS[304]
{0x0F12, 0x004C},	// #TVAR_ash_pGAS[305]
{0x0F12, 0x0054},	// #TVAR_ash_pGAS[306]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[307]
{0x0F12, 0x0082},	// #TVAR_ash_pGAS[308]
{0x0F12, 0x00A4},	// #TVAR_ash_pGAS[309]
{0x0F12, 0x00D4},	// #TVAR_ash_pGAS[310]
{0x0F12, 0x011C},	// #TVAR_ash_pGAS[311]
{0x0F12, 0x00E5},	// #TVAR_ash_pGAS[312]
{0x0F12, 0x00AD},	// #TVAR_ash_pGAS[313]
{0x0F12, 0x0080},	// #TVAR_ash_pGAS[314]
{0x0F12, 0x005D},	// #TVAR_ash_pGAS[315]
{0x0F12, 0x0040},	// #TVAR_ash_pGAS[316]
{0x0F12, 0x002E},	// #TVAR_ash_pGAS[317]
{0x0F12, 0x002A},	// #TVAR_ash_pGAS[318]
{0x0F12, 0x0033},	// #TVAR_ash_pGAS[319]
{0x0F12, 0x0048},	// #TVAR_ash_pGAS[320]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[321]
{0x0F12, 0x008A},	// #TVAR_ash_pGAS[322]
{0x0F12, 0x00B6},	// #TVAR_ash_pGAS[323]
{0x0F12, 0x00F1},	// #TVAR_ash_pGAS[324]
{0x0F12, 0x00D0},	// #TVAR_ash_pGAS[325]
{0x0F12, 0x009C},	// #TVAR_ash_pGAS[326]
{0x0F12, 0x006F},	// #TVAR_ash_pGAS[327]
{0x0F12, 0x0047},	// #TVAR_ash_pGAS[328]
{0x0F12, 0x0029},	// #TVAR_ash_pGAS[329]
{0x0F12, 0x0016},	// #TVAR_ash_pGAS[330]
{0x0F12, 0x0011},	// #TVAR_ash_pGAS[331]
{0x0F12, 0x001A},	// #TVAR_ash_pGAS[332]
{0x0F12, 0x0030},	// #TVAR_ash_pGAS[333]
{0x0F12, 0x0051},	// #TVAR_ash_pGAS[334]
{0x0F12, 0x0078},	// #TVAR_ash_pGAS[335]
{0x0F12, 0x00A3},	// #TVAR_ash_pGAS[336]
{0x0F12, 0x00D7},	// #TVAR_ash_pGAS[337]
{0x0F12, 0x00C6},	// #TVAR_ash_pGAS[338]
{0x0F12, 0x0093},	// #TVAR_ash_pGAS[339]
{0x0F12, 0x0065},	// #TVAR_ash_pGAS[340]
{0x0F12, 0x003B},	// #TVAR_ash_pGAS[341]
{0x0F12, 0x001C},	// #TVAR_ash_pGAS[342]
{0x0F12, 0x0008},	// #TVAR_ash_pGAS[343]
{0x0F12, 0x0003},	// #TVAR_ash_pGAS[344]
{0x0F12, 0x000C},	// #TVAR_ash_pGAS[345]
{0x0F12, 0x0022},	// #TVAR_ash_pGAS[346]
{0x0F12, 0x0043},	// #TVAR_ash_pGAS[347]
{0x0F12, 0x006C},	// #TVAR_ash_pGAS[348]
{0x0F12, 0x0098},	// #TVAR_ash_pGAS[349]
{0x0F12, 0x00C9},	// #TVAR_ash_pGAS[350]
{0x0F12, 0x00C5},	// #TVAR_ash_pGAS[351]
{0x0F12, 0x0093},	// #TVAR_ash_pGAS[352]
{0x0F12, 0x0063},	// #TVAR_ash_pGAS[353]
{0x0F12, 0x0037},	// #TVAR_ash_pGAS[354]
{0x0F12, 0x0018},	// #TVAR_ash_pGAS[355]
{0x0F12, 0x0005},	// #TVAR_ash_pGAS[356]
{0x0F12, 0x0000},	// #TVAR_ash_pGAS[357]
{0x0F12, 0x0008},	// #TVAR_ash_pGAS[358]
{0x0F12, 0x001E},	// #TVAR_ash_pGAS[359]
{0x0F12, 0x003E},	// #TVAR_ash_pGAS[360]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[361]
{0x0F12, 0x0094},	// #TVAR_ash_pGAS[362]
{0x0F12, 0x00C4},	// #TVAR_ash_pGAS[363]
{0x0F12, 0x00CB},	// #TVAR_ash_pGAS[364]
{0x0F12, 0x0098},	// #TVAR_ash_pGAS[365]
{0x0F12, 0x0069},	// #TVAR_ash_pGAS[366]
{0x0F12, 0x003F},	// #TVAR_ash_pGAS[367]
{0x0F12, 0x0020},	// #TVAR_ash_pGAS[368]
{0x0F12, 0x000C},	// #TVAR_ash_pGAS[369]
{0x0F12, 0x0007},	// #TVAR_ash_pGAS[370]
{0x0F12, 0x000F},	// #TVAR_ash_pGAS[371]
{0x0F12, 0x0024},	// #TVAR_ash_pGAS[372]
{0x0F12, 0x0044},	// #TVAR_ash_pGAS[373]
{0x0F12, 0x006C},	// #TVAR_ash_pGAS[374]
{0x0F12, 0x0097},	// #TVAR_ash_pGAS[375]
{0x0F12, 0x00C9},	// #TVAR_ash_pGAS[376]
{0x0F12, 0x00D9},	// #TVAR_ash_pGAS[377]
{0x0F12, 0x00A6},	// #TVAR_ash_pGAS[378]
{0x0F12, 0x0078},	// #TVAR_ash_pGAS[379]
{0x0F12, 0x0051},	// #TVAR_ash_pGAS[380]
{0x0F12, 0x0032},	// #TVAR_ash_pGAS[381]
{0x0F12, 0x001E},	// #TVAR_ash_pGAS[382]
{0x0F12, 0x0019},	// #TVAR_ash_pGAS[383]
{0x0F12, 0x0020},	// #TVAR_ash_pGAS[384]
{0x0F12, 0x0035},	// #TVAR_ash_pGAS[385]
{0x0F12, 0x0055},	// #TVAR_ash_pGAS[386]
{0x0F12, 0x0079},	// #TVAR_ash_pGAS[387]
{0x0F12, 0x00A2},	// #TVAR_ash_pGAS[388]
{0x0F12, 0x00D7},	// #TVAR_ash_pGAS[389]
{0x0F12, 0x00F4},	// #TVAR_ash_pGAS[390]
{0x0F12, 0x00BD},	// #TVAR_ash_pGAS[391]
{0x0F12, 0x0090},	// #TVAR_ash_pGAS[392]
{0x0F12, 0x006B},	// #TVAR_ash_pGAS[393]
{0x0F12, 0x004E},	// #TVAR_ash_pGAS[394]
{0x0F12, 0x003C},	// #TVAR_ash_pGAS[395]
{0x0F12, 0x0037},	// #TVAR_ash_pGAS[396]
{0x0F12, 0x003E},	// #TVAR_ash_pGAS[397]
{0x0F12, 0x0051},	// #TVAR_ash_pGAS[398]
{0x0F12, 0x006E},	// #TVAR_ash_pGAS[399]
{0x0F12, 0x008D},	// #TVAR_ash_pGAS[400]
{0x0F12, 0x00B9},	// #TVAR_ash_pGAS[401]
{0x0F12, 0x00EF},	// #TVAR_ash_pGAS[402]
{0x0F12, 0x011C},	// #TVAR_ash_pGAS[403]
{0x0F12, 0x00DB},	// #TVAR_ash_pGAS[404]
{0x0F12, 0x00AC},	// #TVAR_ash_pGAS[405]
{0x0F12, 0x008D},	// #TVAR_ash_pGAS[406]
{0x0F12, 0x0074},	// #TVAR_ash_pGAS[407]
{0x0F12, 0x0064},	// #TVAR_ash_pGAS[408]
{0x0F12, 0x005F},	// #TVAR_ash_pGAS[409]
{0x0F12, 0x0065},	// #TVAR_ash_pGAS[410]
{0x0F12, 0x0076},	// #TVAR_ash_pGAS[411]
{0x0F12, 0x008E},	// #TVAR_ash_pGAS[412]
{0x0F12, 0x00AC},	// #TVAR_ash_pGAS[413]
{0x0F12, 0x00D8},	// #TVAR_ash_pGAS[414]
{0x0F12, 0x011D},	// #TVAR_ash_pGAS[415]
{0x0F12, 0x014F},	// #TVAR_ash_pGAS[416]
{0x0F12, 0x0101},	// #TVAR_ash_pGAS[417]
{0x0F12, 0x00D2},	// #TVAR_ash_pGAS[418]
{0x0F12, 0x00B3},	// #TVAR_ash_pGAS[419]
{0x0F12, 0x009C},	// #TVAR_ash_pGAS[420]
{0x0F12, 0x008D},	// #TVAR_ash_pGAS[421]
{0x0F12, 0x008A},	// #TVAR_ash_pGAS[422]
{0x0F12, 0x008F},	// #TVAR_ash_pGAS[423]
{0x0F12, 0x009C},	// #TVAR_ash_pGAS[424]
{0x0F12, 0x00B0},	// #TVAR_ash_pGAS[425]
{0x0F12, 0x00D2},	// #TVAR_ash_pGAS[426]
{0x0F12, 0x0106},	// #TVAR_ash_pGAS[427]
{0x0F12, 0x016D},	// #TVAR_ash_pGAS[428]
{0x0F12, 0x00F9},	// #TVAR_ash_pGAS[429]
{0x0F12, 0x00B1},	// #TVAR_ash_pGAS[430]
{0x0F12, 0x0089},	// #TVAR_ash_pGAS[431]
{0x0F12, 0x0070},	// #TVAR_ash_pGAS[432]
{0x0F12, 0x0061},	// #TVAR_ash_pGAS[433]
{0x0F12, 0x005A},	// #TVAR_ash_pGAS[434]
{0x0F12, 0x005A},	// #TVAR_ash_pGAS[435]
{0x0F12, 0x0062},	// #TVAR_ash_pGAS[436]
{0x0F12, 0x0070},	// #TVAR_ash_pGAS[437]
{0x0F12, 0x0085},	// #TVAR_ash_pGAS[438]
{0x0F12, 0x00A3},	// #TVAR_ash_pGAS[439]
{0x0F12, 0x00D4},	// #TVAR_ash_pGAS[440]
{0x0F12, 0x0130},	// #TVAR_ash_pGAS[441]
{0x0F12, 0x00C3},	// #TVAR_ash_pGAS[442]
{0x0F12, 0x0091},	// #TVAR_ash_pGAS[443]
{0x0F12, 0x006F},	// #TVAR_ash_pGAS[444]
{0x0F12, 0x0058},	// #TVAR_ash_pGAS[445]
{0x0F12, 0x0049},	// #TVAR_ash_pGAS[446]
{0x0F12, 0x003F},	// #TVAR_ash_pGAS[447]
{0x0F12, 0x003F},	// #TVAR_ash_pGAS[448]
{0x0F12, 0x0047},	// #TVAR_ash_pGAS[449]
{0x0F12, 0x0056},	// #TVAR_ash_pGAS[450]
{0x0F12, 0x006D},	// #TVAR_ash_pGAS[451]
{0x0F12, 0x0088},	// #TVAR_ash_pGAS[452]
{0x0F12, 0x00B2},	// #TVAR_ash_pGAS[453]
{0x0F12, 0x00EE},	// #TVAR_ash_pGAS[454]
{0x0F12, 0x00A1},	// #TVAR_ash_pGAS[455]
{0x0F12, 0x0076},	// #TVAR_ash_pGAS[456]
{0x0F12, 0x0059},	// #TVAR_ash_pGAS[457]
{0x0F12, 0x0042},	// #TVAR_ash_pGAS[458]
{0x0F12, 0x002F},	// #TVAR_ash_pGAS[459]
{0x0F12, 0x0025},	// #TVAR_ash_pGAS[460]
{0x0F12, 0x0023},	// #TVAR_ash_pGAS[461]
{0x0F12, 0x002C},	// #TVAR_ash_pGAS[462]
{0x0F12, 0x003D},	// #TVAR_ash_pGAS[463]
{0x0F12, 0x0056},	// #TVAR_ash_pGAS[464]
{0x0F12, 0x0071},	// #TVAR_ash_pGAS[465]
{0x0F12, 0x0094},	// #TVAR_ash_pGAS[466]
{0x0F12, 0x00C6},	// #TVAR_ash_pGAS[467]
{0x0F12, 0x008E},	// #TVAR_ash_pGAS[468]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[469]
{0x0F12, 0x0049},	// #TVAR_ash_pGAS[470]
{0x0F12, 0x0030},	// #TVAR_ash_pGAS[471]
{0x0F12, 0x001D},	// #TVAR_ash_pGAS[472]
{0x0F12, 0x0011},	// #TVAR_ash_pGAS[473]
{0x0F12, 0x000F},	// #TVAR_ash_pGAS[474]
{0x0F12, 0x0018},	// #TVAR_ash_pGAS[475]
{0x0F12, 0x0029},	// #TVAR_ash_pGAS[476]
{0x0F12, 0x0043},	// #TVAR_ash_pGAS[477]
{0x0F12, 0x0060},	// #TVAR_ash_pGAS[478]
{0x0F12, 0x0081},	// #TVAR_ash_pGAS[479]
{0x0F12, 0x00AC},	// #TVAR_ash_pGAS[480]
{0x0F12, 0x0084},	// #TVAR_ash_pGAS[481]
{0x0F12, 0x005F},	// #TVAR_ash_pGAS[482]
{0x0F12, 0x0041},	// #TVAR_ash_pGAS[483]
{0x0F12, 0x0027},	// #TVAR_ash_pGAS[484]
{0x0F12, 0x0013},	// #TVAR_ash_pGAS[485]
{0x0F12, 0x0006},	// #TVAR_ash_pGAS[486]
{0x0F12, 0x0003},	// #TVAR_ash_pGAS[487]
{0x0F12, 0x000B},	// #TVAR_ash_pGAS[488]
{0x0F12, 0x001D},	// #TVAR_ash_pGAS[489]
{0x0F12, 0x0037},	// #TVAR_ash_pGAS[490]
{0x0F12, 0x0054},	// #TVAR_ash_pGAS[491]
{0x0F12, 0x0075},	// #TVAR_ash_pGAS[492]
{0x0F12, 0x009C},	// #TVAR_ash_pGAS[493]
{0x0F12, 0x0081},	// #TVAR_ash_pGAS[494]
{0x0F12, 0x005E},	// #TVAR_ash_pGAS[495]
{0x0F12, 0x003F},	// #TVAR_ash_pGAS[496]
{0x0F12, 0x0026},	// #TVAR_ash_pGAS[497]
{0x0F12, 0x0011},	// #TVAR_ash_pGAS[498]
{0x0F12, 0x0003},	// #TVAR_ash_pGAS[499]
{0x0F12, 0x0000},	// #TVAR_ash_pGAS[500]
{0x0F12, 0x0007},	// #TVAR_ash_pGAS[501]
{0x0F12, 0x0018},	// #TVAR_ash_pGAS[502]
{0x0F12, 0x0031},	// #TVAR_ash_pGAS[503]
{0x0F12, 0x004E},	// #TVAR_ash_pGAS[504]
{0x0F12, 0x006F},	// #TVAR_ash_pGAS[505]
{0x0F12, 0x0096},	// #TVAR_ash_pGAS[506]
{0x0F12, 0x0086},	// #TVAR_ash_pGAS[507]
{0x0F12, 0x0064},	// #TVAR_ash_pGAS[508]
{0x0F12, 0x0046},	// #TVAR_ash_pGAS[509]
{0x0F12, 0x002C},	// #TVAR_ash_pGAS[510]
{0x0F12, 0x0017},	// #TVAR_ash_pGAS[511]
{0x0F12, 0x000A},	// #TVAR_ash_pGAS[512]
{0x0F12, 0x0006},	// #TVAR_ash_pGAS[513]
{0x0F12, 0x000D},	// #TVAR_ash_pGAS[514]
{0x0F12, 0x001D},	// #TVAR_ash_pGAS[515]
{0x0F12, 0x0035},	// #TVAR_ash_pGAS[516]
{0x0F12, 0x0050},	// #TVAR_ash_pGAS[517]
{0x0F12, 0x0071},	// #TVAR_ash_pGAS[518]
{0x0F12, 0x0098},	// #TVAR_ash_pGAS[519]
{0x0F12, 0x0095},	// #TVAR_ash_pGAS[520]
{0x0F12, 0x0070},	// #TVAR_ash_pGAS[521]
{0x0F12, 0x0052},	// #TVAR_ash_pGAS[522]
{0x0F12, 0x003A},	// #TVAR_ash_pGAS[523]
{0x0F12, 0x0027},	// #TVAR_ash_pGAS[524]
{0x0F12, 0x0019},	// #TVAR_ash_pGAS[525]
{0x0F12, 0x0016},	// #TVAR_ash_pGAS[526]
{0x0F12, 0x001C},	// #TVAR_ash_pGAS[527]
{0x0F12, 0x002B},	// #TVAR_ash_pGAS[528]
{0x0F12, 0x0040},	// #TVAR_ash_pGAS[529]
{0x0F12, 0x005A},	// #TVAR_ash_pGAS[530]
{0x0F12, 0x007A},	// #TVAR_ash_pGAS[531]
{0x0F12, 0x00A4},	// #TVAR_ash_pGAS[532]
{0x0F12, 0x00AD},	// #TVAR_ash_pGAS[533]
{0x0F12, 0x0084},	// #TVAR_ash_pGAS[534]
{0x0F12, 0x0067},	// #TVAR_ash_pGAS[535]
{0x0F12, 0x004F},	// #TVAR_ash_pGAS[536]
{0x0F12, 0x003D},	// #TVAR_ash_pGAS[537]
{0x0F12, 0x0031},	// #TVAR_ash_pGAS[538]
{0x0F12, 0x002E},	// #TVAR_ash_pGAS[539]
{0x0F12, 0x0033},	// #TVAR_ash_pGAS[540]
{0x0F12, 0x0040},	// #TVAR_ash_pGAS[541]
{0x0F12, 0x0055},	// #TVAR_ash_pGAS[542]
{0x0F12, 0x006C},	// #TVAR_ash_pGAS[543]
{0x0F12, 0x008C},	// #TVAR_ash_pGAS[544]
{0x0F12, 0x00BA},	// #TVAR_ash_pGAS[545]
{0x0F12, 0x00D3},	// #TVAR_ash_pGAS[546]
{0x0F12, 0x00A0},	// #TVAR_ash_pGAS[547]
{0x0F12, 0x007E},	// #TVAR_ash_pGAS[548]
{0x0F12, 0x006C},	// #TVAR_ash_pGAS[549]
{0x0F12, 0x005C},	// #TVAR_ash_pGAS[550]
{0x0F12, 0x0053},	// #TVAR_ash_pGAS[551]
{0x0F12, 0x0050},	// #TVAR_ash_pGAS[552]
{0x0F12, 0x0056},	// #TVAR_ash_pGAS[553]
{0x0F12, 0x0061},	// #TVAR_ash_pGAS[554]
{0x0F12, 0x0071},	// #TVAR_ash_pGAS[555]
{0x0F12, 0x0086},	// #TVAR_ash_pGAS[556]
{0x0F12, 0x00A9},	// #TVAR_ash_pGAS[557]
{0x0F12, 0x00E3},	// #TVAR_ash_pGAS[558]
{0x0F12, 0x0102},	// #TVAR_ash_pGAS[559]
{0x0F12, 0x00C1},	// #TVAR_ash_pGAS[560]
{0x0F12, 0x009E},	// #TVAR_ash_pGAS[561]
{0x0F12, 0x008C},	// #TVAR_ash_pGAS[562]
{0x0F12, 0x007E},	// #TVAR_ash_pGAS[563]
{0x0F12, 0x0076},	// #TVAR_ash_pGAS[564]
{0x0F12, 0x0074},	// #TVAR_ash_pGAS[565]
{0x0F12, 0x0077},	// #TVAR_ash_pGAS[566]
{0x0F12, 0x0080},	// #TVAR_ash_pGAS[567]
{0x0F12, 0x008E},	// #TVAR_ash_pGAS[568]
{0x0F12, 0x00A6},	// #TVAR_ash_pGAS[569]
{0x0F12, 0x00D2},	// #TVAR_ash_pGAS[570]
{0x0F12, 0x0128},	// #TVAR_ash_pGAS[571]

{0x002A, 0x0D5C},
{0x0F12, 0x02A7},	// #awbb_GLocusR
{0x0F12, 0x0343},	// #awbb_GLocusB

{0x002A, 0x06F8},
{0x0F12, 0x00AA},	// #TVAR_ash_AwbAshCord[0]
{0x0F12, 0x00B5},	// #TVAR_ash_AwbAshCord[1]
{0x0F12, 0x00BE},	// #TVAR_ash_AwbAshCord[2]
{0x0F12, 0x011D},	// #TVAR_ash_AwbAshCord[3]
{0x0F12, 0x0144},	// #TVAR_ash_AwbAshCord[4]
{0x0F12, 0x0173},	// #TVAR_ash_AwbAshCord[5]
{0x0F12, 0x0180},	// #TVAR_ash_AwbAshCord[6]

{0x002A, 0x0B96},
{0x0F12, 0x0002},	// #THSTAT_Mon_u16_StartX
{0x0F12, 0x0000},	// #THSTAT_Mon_u16_StartY
{0x0F12, 0x0006},	// #THSTAT_Mon_u16_StepX
{0x0F12, 0x0009},	// #THSTAT_Mon_u16_StepY
{0x002A, 0x0B94},
{0x0F12, 0x0002},	// #THSTAT_Mon_u16_CaptureType
{0x002A, 0x0B92},
{0x0F12, 0x0001},	// #THSTAT_Mon_u16_CaptureFrameRequest

{0x002A, 0x078C},
{0x0F12, 0x347C},	// #TVAR_ash_pGAS : 7000_347C
{0x0F12, 0x7000},


//================================================================================================
//	Set CCM
//================================================================================================
// Horizon
{0x002A, 0x33A4},
{0x0F12, 0x0165},	// #TVAR_wbt_pBaseCcms[0]	// R
{0x0F12, 0xFF8A},	// #TVAR_wbt_pBaseCcms[1]
{0x0F12, 0xFFB8},	// #TVAR_wbt_pBaseCcms[2]
{0x0F12, 0xFFC8},	// #TVAR_wbt_pBaseCcms[3] 	// G
{0x0F12, 0x01AE},	// #TVAR_wbt_pBaseCcms[4]
{0x0F12, 0xFECF},	// #TVAR_wbt_pBaseCcms[5]
{0x0F12, 0x000B},	// #TVAR_wbt_pBaseCcms[6] 	// B
{0x0F12, 0x002A},	// #TVAR_wbt_pBaseCcms[7]
{0x0F12, 0x027D},	// #TVAR_wbt_pBaseCcms[8]
{0x0F12, 0x005D},	// #TVAR_wbt_pBaseCcms[9] 	// Y
{0x0F12, 0x003E},	// #TVAR_wbt_pBaseCcms[10]
{0x0F12, 0xFE64},	// #TVAR_wbt_pBaseCcms[11]
{0x0F12, 0x00C5},	// #TVAR_wbt_pBaseCcms[12]	// M
{0x0F12, 0xFF67},	// #TVAR_wbt_pBaseCcms[13]
{0x0F12, 0x0229},	// #TVAR_wbt_pBaseCcms[14]
{0x0F12, 0xFF35},	// #TVAR_wbt_pBaseCcms[15]	// C
{0x0F12, 0x0143},	// #TVAR_wbt_pBaseCcms[16]
{0x0F12, 0x0100},	// #TVAR_wbt_pBaseCcms[17]
// IncaA  	    	
{0x0F12, 0x0165},	// #TVAR_wbt_pBaseCcms[18]	// R
{0x0F12, 0xFF8A},	// #TVAR_wbt_pBaseCcms[19]
{0x0F12, 0xFFB8},	// #TVAR_wbt_pBaseCcms[20]
{0x0F12, 0xFFC8},	// #TVAR_wbt_pBaseCcms[21]      // G
{0x0F12, 0x01AE},	// #TVAR_wbt_pBaseCcms[22]
{0x0F12, 0xFECF},	// #TVAR_wbt_pBaseCcms[23]
{0x0F12, 0x000B},	// #TVAR_wbt_pBaseCcms[24]      // B
{0x0F12, 0x002A},	// #TVAR_wbt_pBaseCcms[25]
{0x0F12, 0x027D},	// #TVAR_wbt_pBaseCcms[26]
{0x0F12, 0x005D},	// #TVAR_wbt_pBaseCcms[27]      // Y
{0x0F12, 0x003E},	// #TVAR_wbt_pBaseCcms[28]
{0x0F12, 0xFE64},	// #TVAR_wbt_pBaseCcms[29]
{0x0F12, 0x00C5},	// #TVAR_wbt_pBaseCcms[30]      // M
{0x0F12, 0xFF67},	// #TVAR_wbt_pBaseCcms[31]
{0x0F12, 0x0229},	// #TVAR_wbt_pBaseCcms[32]
{0x0F12, 0xFF35},	// #TVAR_wbt_pBaseCcms[33]      // C
{0x0F12, 0x0143},	// #TVAR_wbt_pBaseCcms[34]
{0x0F12, 0x0100},	// #TVAR_wbt_pBaseCcms[35]
// WW     	    	            
{0x0F12 ,0x01E8},	// #TVAR_wbt_pBaseCcms[36]	// R
{0x0F12 ,0xFF91},	// #TVAR_wbt_pBaseCcms[37]
{0x0F12 ,0xFFE0},	// #TVAR_wbt_pBaseCcms[38]
{0x0F12 ,0xFFA4},	// #TVAR_wbt_pBaseCcms[39]      // G
{0x0F12 ,0x0230},	// #TVAR_wbt_pBaseCcms[40]
{0x0F12 ,0xFF94},	// #TVAR_wbt_pBaseCcms[41]
{0x0F12 ,0x0011},	// #TVAR_wbt_pBaseCcms[42]      // B
{0x0F12 ,0x001C},	// #TVAR_wbt_pBaseCcms[43]
{0x0F12 ,0x023C},	// #TVAR_wbt_pBaseCcms[44]
{0x0F12 ,0x011F},	// #TVAR_wbt_pBaseCcms[45]      // Y
{0x0F12 ,0x00F4},	// #TVAR_wbt_pBaseCcms[46]
{0x0F12 ,0xFF21},	// #TVAR_wbt_pBaseCcms[47]
{0x0F12 ,0x021E},	// #TVAR_wbt_pBaseCcms[48]      // M
{0x0F12 ,0xFFB7},	// #TVAR_wbt_pBaseCcms[49]
{0x0F12 ,0x028D},	// #TVAR_wbt_pBaseCcms[50]
{0x0F12 ,0xFF4F},	// #TVAR_wbt_pBaseCcms[51]      // C
{0x0F12 ,0x0237},	// #TVAR_wbt_pBaseCcms[52]
{0x0F12 ,0x01A9},	// #TVAR_wbt_pBaseCcms[53]
// CW,0xF    	    	
{0x0F12 ,0x01E8},	// #TVAR_wbt_pBaseCcms[54]	// R
{0x0F12 ,0xFF91},	// #TVAR_wbt_pBaseCcms[55]
{0x0F12 ,0xFFE0},	// #TVAR_wbt_pBaseCcms[56]
{0x0F12 ,0xFFA4},	// #TVAR_wbt_pBaseCcms[57]	// G
{0x0F12 ,0x0230},	// #TVAR_wbt_pBaseCcms[58]
{0x0F12 ,0xFF94},	// #TVAR_wbt_pBaseCcms[59]
{0x0F12 ,0x0011},	// #TVAR_wbt_pBaseCcms[60]	// B
{0x0F12 ,0x001C},	// #TVAR_wbt_pBaseCcms[61]
{0x0F12 ,0x023C},	// #TVAR_wbt_pBaseCcms[62]
{0x0F12 ,0x011F},	// #TVAR_wbt_pBaseCcms[63]	// Y
{0x0F12 ,0x00F4},	// #TVAR_wbt_pBaseCcms[64]
{0x0F12 ,0xFF21},	// #TVAR_wbt_pBaseCcms[65]
{0x0F12 ,0x021E},	// #TVAR_wbt_pBaseCcms[66]	// M
{0x0F12 ,0xFFB7},	// #TVAR_wbt_pBaseCcms[67]
{0x0F12 ,0x028D},	// #TVAR_wbt_pBaseCcms[68]
{0x0F12 ,0xFF4F},	// #TVAR_wbt_pBaseCcms[69]	// C
{0x0F12 ,0x0237},	// #TVAR_wbt_pBaseCcms[70]
{0x0F12 ,0x01A9},	// #TVAR_wbt_pBaseCcms[71]
// D5,0x0
{0x0F12 ,0x019D},	// #TVAR_wbt_pBaseCcms[72]
{0x0F12 ,0xFFA6},	// #TVAR_wbt_pBaseCcms[73]
{0x0F12 ,0xFFE8},	// #TVAR_wbt_pBaseCcms[74]
{0x0F12 ,0xFF8A},	// #TVAR_wbt_pBaseCcms[75]
{0x0F12 ,0x015D},	// #TVAR_wbt_pBaseCcms[76]
{0x0F12 ,0xFF65},	// #TVAR_wbt_pBaseCcms[77]
{0x0F12 ,0xFFE6},	// #TVAR_wbt_pBaseCcms[78]
{0x0F12 ,0x000A},	// #TVAR_wbt_pBaseCcms[79]
{0x0F12 ,0x019A},	// #TVAR_wbt_pBaseCcms[80]
{0x0F12 ,0x0103},	// #TVAR_wbt_pBaseCcms[81]
{0x0F12 ,0x0112},	// #TVAR_wbt_pBaseCcms[82]
{0x0F12 ,0xFF61},	// #TVAR_wbt_pBaseCcms[83]
{0x0F12 ,0x0164},	// #TVAR_wbt_pBaseCcms[84]
{0x0F12 ,0xFF82},	// #TVAR_wbt_pBaseCcms[85]
{0x0F12 ,0x01CE},	// #TVAR_wbt_pBaseCcms[86]
{0x0F12 ,0xFF2F},	// #TVAR_wbt_pBaseCcms[87]
{0x0F12 ,0x0234},	// #TVAR_wbt_pBaseCcms[88]
{0x0F12 ,0x01AC},	// #TVAR_wbt_pBaseCcms[89]
// D6,0x5 
{0x0F12 ,0x019D},	// #TVAR_wbt_pBaseCcms[90]	// R
{0x0F12 ,0xFFA6},	// #TVAR_wbt_pBaseCcms[91]
{0x0F12 ,0xFFE8},	// #TVAR_wbt_pBaseCcms[92]
{0x0F12 ,0xFF8A},	// #TVAR_wbt_pBaseCcms[93]	// G
{0x0F12 ,0x015D},	// #TVAR_wbt_pBaseCcms[94]
{0x0F12 ,0xFF65},	// #TVAR_wbt_pBaseCcms[95]
{0x0F12 ,0xFFE6},	// #TVAR_wbt_pBaseCcms[96]	// B
{0x0F12 ,0x000A},	// #TVAR_wbt_pBaseCcms[97]
{0x0F12 ,0x019A},	// #TVAR_wbt_pBaseCcms[98]
{0x0F12 ,0x0103},	// #TVAR_wbt_pBaseCcms[99]	// Y
{0x0F12 ,0x0112},	// #TVAR_wbt_pBaseCcms[100]
{0x0F12 ,0xFF61},	// #TVAR_wbt_pBaseCcms[101]
{0x0F12 ,0x0164},	// #TVAR_wbt_pBaseCcms[102]	// M
{0x0F12 ,0xFF82},	// #TVAR_wbt_pBaseCcms[103]
{0x0F12 ,0x01CE},	// #TVAR_wbt_pBaseCcms[104]
{0x0F12 ,0xFF2F},	// #TVAR_wbt_pBaseCcms[105]	// C
{0x0F12 ,0x0234},	// #TVAR_wbt_pBaseCcms[106]
{0x0F12 ,0x01AC},	// #TVAR_wbt_pBaseCcms[107]

{0x002A ,0x06D8},
{0x0F12 ,0x33A4},	// #TVAR_wbt_pBaseCcms 700033A4
{0x0F12 ,0x7000},

// Outdoor
{0x002A, 0x3380},
{0x0F12, 0x0165},	// #TVAR_wbt_pOutdoorCcm[0]
{0x0F12, 0xFF8A},	// #TVAR_wbt_pOutdoorCcm[1]
{0x0F12, 0xFFB8},	// #TVAR_wbt_pOutdoorCcm[2]
{0x0F12, 0xFFC8},	// #TVAR_wbt_pOutdoorCcm[3]
{0x0F12, 0x01AE},	// #TVAR_wbt_pOutdoorCcm[4]
{0x0F12, 0xFECF},	// #TVAR_wbt_pOutdoorCcm[5]
{0x0F12, 0x000B},	// #TVAR_wbt_pOutdoorCcm[6]
{0x0F12, 0x002A},	// #TVAR_wbt_pOutdoorCcm[7]
{0x0F12, 0x027D},	// #TVAR_wbt_pOutdoorCcm[8]
{0x0F12, 0x005D},	// #TVAR_wbt_pOutdoorCcm[9]
{0x0F12, 0x003E},	// #TVAR_wbt_pOutdoorCcm[10]
{0x0F12, 0xFE64},	// #TVAR_wbt_pOutdoorCcm[11]
{0x0F12, 0x00C5},	// #TVAR_wbt_pOutdoorCcm[12]
{0x0F12, 0xFF67},	// #TVAR_wbt_pOutdoorCcm[13]
{0x0F12, 0x0229},	// #TVAR_wbt_pOutdoorCcm[14]
{0x0F12, 0xFF35},	// #TVAR_wbt_pOutdoorCcm[15]
{0x0F12, 0x0143},	// #TVAR_wbt_pOutdoorCcm[16]
{0x0F12, 0x0100},	// #TVAR_wbt_pOutdoorCcm[17]

{0x002A, 0x06E0},
{0x0F12, 0x3380},	// #TVAR_wbt_pOutdoorCcm
{0x0F12, 0x7000},

{0x002A, 0x1388},
{0x0F12, 0x0200},
{0x0F12, 0x0200},
{0x0F12, 0x0200},
{0x0F12, 0x0200},

// reInit Core  
{0x002A, 0x0532},
{0x0F12, 0x0001},	// #REG_TC_DBG_ReInitCmd

               
//================================================================================================
//	Set AWB
//================================================================================================
// Indoor      
{0x002A, 0x0C74},
{0x0F12, 0x0411},	// #awbb_IndoorGrZones_m_BGrid[0]
{0x0F12, 0x0449},	// #awbb_IndoorGrZones_m_BGrid[1]
{0x0F12, 0x03C4},	// #awbb_IndoorGrZones_m_BGrid[2]
{0x0F12, 0x0448},	// #awbb_IndoorGrZones_m_BGrid[3]
{0x0F12, 0x0373},	// #awbb_IndoorGrZones_m_BGrid[4]
{0x0F12, 0x0420},	// #awbb_IndoorGrZones_m_BGrid[5]
{0x0F12, 0x0322},	// #awbb_IndoorGrZones_m_BGrid[6]
{0x0F12, 0x03F0},	// #awbb_IndoorGrZones_m_BGrid[7]
{0x0F12, 0x02E1},	// #awbb_IndoorGrZones_m_BGrid[8]
{0x0F12, 0x03C4},	// #awbb_IndoorGrZones_m_BGrid[9]
{0x0F12, 0x02C2},	// #awbb_IndoorGrZones_m_BGrid[10]
{0x0F12, 0x038B},	// #awbb_IndoorGrZones_m_BGrid[11]
{0x0F12, 0x02A9},	// #awbb_IndoorGrZones_m_BGrid[12]
{0x0F12, 0x0356},	// #awbb_IndoorGrZones_m_BGrid[13]
{0x0F12, 0x0293},	// #awbb_IndoorGrZones_m_BGrid[14]
{0x0F12, 0x0322},	// #awbb_IndoorGrZones_m_BGrid[15]
{0x0F12, 0x0278},	// #awbb_IndoorGrZones_m_BGrid[16]
{0x0F12, 0x0303},	// #awbb_IndoorGrZones_m_BGrid[17]
{0x0F12, 0x0263},	// #awbb_IndoorGrZones_m_BGrid[18]
{0x0F12, 0x02EE},	// #awbb_IndoorGrZones_m_BGrid[19]
{0x0F12, 0x024F},	// #awbb_IndoorGrZones_m_BGrid[20]
{0x0F12, 0x02DD},	// #awbb_IndoorGrZones_m_BGrid[21]
{0x0F12, 0x023B},	// #awbb_IndoorGrZones_m_BGrid[22]
{0x0F12, 0x02CF},	// #awbb_IndoorGrZones_m_BGrid[23]
{0x0F12, 0x022B},	// #awbb_IndoorGrZones_m_BGrid[24]
{0x0F12, 0x02B9},	// #awbb_IndoorGrZones_m_BGrid[25]
{0x0F12, 0x0222},	// #awbb_IndoorGrZones_m_BGrid[26]
{0x0F12, 0x02A4},	// #awbb_IndoorGrZones_m_BGrid[27]
{0x0F12, 0x0227},	// #awbb_IndoorGrZones_m_BGrid[28]
{0x0F12, 0x028A},	// #awbb_IndoorGrZones_m_BGrid[29]
{0x0F12, 0x023B},	// #awbb_IndoorGrZones_m_BGrid[30]
{0x0F12, 0x026A},	// #awbb_IndoorGrZones_m_BGrid[31]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[32]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[33]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[34]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[35]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[36]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[37]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[38]
{0x0F12, 0x0000},	// #awbb_IndoorGrZones_m_BGrid[39]
{0x0F12, 0x0005},	// #awbb_IndoorGrZones_m_GridStep
{0x002A, 0x0CCC},
{0x0F12, 0x00E0},	// #awbb_IndoorGrZones_m_Boffs
               
// LowBr       
{0x002A, 0x0D0C},
{0x0F12, 0x03D8},	// #awbb_LowBrGrZones_m_BGrid[0]
{0x0F12, 0x0487},	// #awbb_LowBrGrZones_m_BGrid[1]
{0x0F12, 0x0332},	// #awbb_LowBrGrZones_m_BGrid[2]
{0x0F12, 0x0498},	// #awbb_LowBrGrZones_m_BGrid[3]
{0x0F12, 0x02BF},	// #awbb_LowBrGrZones_m_BGrid[4]
{0x0F12, 0x045E},	// #awbb_LowBrGrZones_m_BGrid[5]
{0x0F12, 0x027A},	// #awbb_LowBrGrZones_m_BGrid[6]
{0x0F12, 0x03EC},	// #awbb_LowBrGrZones_m_BGrid[7]
{0x0F12, 0x0249},	// #awbb_LowBrGrZones_m_BGrid[8]
{0x0F12, 0x0382},	// #awbb_LowBrGrZones_m_BGrid[9]
{0x0F12, 0x0215},	// #awbb_LowBrGrZones_m_BGrid[10]
{0x0F12, 0x033B},	// #awbb_LowBrGrZones_m_BGrid[11]
{0x0F12, 0x01F6},	// #awbb_LowBrGrZones_m_BGrid[12]
{0x0F12, 0x0307},	// #awbb_LowBrGrZones_m_BGrid[13]
{0x0F12, 0x01E6},	// #awbb_LowBrGrZones_m_BGrid[14]
{0x0F12, 0x02EC},	// #awbb_LowBrGrZones_m_BGrid[15]
{0x0F12, 0x01F1},	// #awbb_LowBrGrZones_m_BGrid[16]
{0x0F12, 0x02C4},	// #awbb_LowBrGrZones_m_BGrid[17]
{0x0F12, 0x021E},	// #awbb_LowBrGrZones_m_BGrid[18]
{0x0F12, 0x0278},	// #awbb_LowBrGrZones_m_BGrid[19]
{0x0F12, 0x0000},	// #awbb_LowBrGrZones_m_BGrid[20]
{0x0F12, 0x0000},	// #awbb_LowBrGrZones_m_BGrid[21]
{0x0F12, 0x0000},	// #awbb_LowBrGrZones_m_BGrid[22]
{0x0F12, 0x0000},	// #awbb_LowBrGrZones_m_BGrid[23]
{0x0F12, 0x0006},	// #awbb_LowBrGrZones_m_GridStep
{0x002A, 0x0D44},
{0x0F12, 0x00B7},	// #awbb_LowBrGrZones_m_Boffs
               
// Outdoor     
{0x002A, 0x0CD0},
{0x0F12, 0x03E9},	// #awbb_OutdoorGrZones_m_BGrid[0]
{0x0F12, 0x0424},	// #awbb_OutdoorGrZones_m_BGrid[1]
{0x0F12, 0x0353},	// #awbb_OutdoorGrZones_m_BGrid[2]
{0x0F12, 0x0420},	// #awbb_OutdoorGrZones_m_BGrid[3]
{0x0F12, 0x02DF},	// #awbb_OutdoorGrZones_m_BGrid[4]
{0x0F12, 0x03C6},	// #awbb_OutdoorGrZones_m_BGrid[5]
{0x0F12, 0x029C},	// #awbb_OutdoorGrZones_m_BGrid[6]
{0x0F12, 0x0358},	// #awbb_OutdoorGrZones_m_BGrid[7]
{0x0F12, 0x0268},	// #awbb_OutdoorGrZones_m_BGrid[8]
{0x0F12, 0x0305},	// #awbb_OutdoorGrZones_m_BGrid[9]
{0x0F12, 0x023C},	// #awbb_OutdoorGrZones_m_BGrid[10]
{0x0F12, 0x02E1},	// #awbb_OutdoorGrZones_m_BGrid[11]
{0x0F12, 0x021E},	// #awbb_OutdoorGrZones_m_BGrid[12]
{0x0F12, 0x02BC},	// #awbb_OutdoorGrZones_m_BGrid[13]
{0x0F12, 0x022E},	// #awbb_OutdoorGrZones_m_BGrid[14]
{0x0F12, 0x028B},	// #awbb_OutdoorGrZones_m_BGrid[15]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[16]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[17]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[18]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[19]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[20]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[21]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[22]
{0x0F12, 0x0000},	// #awbb_OutdoorGrZones_m_BGrid[23]
{0x0F12, 0x0006},	// #awbb_OutdoorGrZones_m_GridStep
{0x002A, 0x0D08},
{0x0F12, 0x00E6},	// #awbb_OutdoorGrZones_m_Boffs
               
// Low temperature
{0x002A, 0x0D48},
{0x0F12, 0x03BF},	// #awbb_CrclLowT_R_c
{0x002A, 0x0D4C},
{0x0F12, 0x011F},	// #awbb_CrclLowT_B_c
{0x002A, 0x0D50},
{0x0F12, 0x5875},	// #awbb_CrclLowT_Rad_c

{0x002A, 0x202A},
{0x0F12, 0x0004},	// #Mon_awb_ByPassMode : LowTemp bypass
               
               
// White locus 
{0x002A, 0x0D58},
{0x0F12, 0x0151},	// #awbb_IntcR
{0x0F12, 0x010D},	// #awbb_IntcB
               
// Gamut threshold
{0x002A, 0x0D78},
{0x0F12, 0x0187},	//#awbb_GamutWidthThr1
{0x0F12, 0x00CF},	//#awbb_GamutHeightThr1
{0x0F12, 0x000D},	//#awbb_GamutWidthThr2
{0x0F12, 0x000A},	//#awbb_GamutHeightThr2
               
// Set scene threshold               
{0x002A, 0x0D88},
{0x0F12, 0x05AA},	// #awbb_LowTempRB
{0x0F12, 0x0050},	// #awbb_LowTemp_RBzone
{0x002A, 0x0D72},
{0x0F12, 0x011C},	// #awbb_MvEq_RBthresh
{0x002A, 0x0DD4},       
{0x0F12, 0x05dc}, //9C4	////awbb_OutdoorDetectionZone_ZInfo_m_MaxNB

{0x002A, 0x0E6A},
{0x0F12, 0x0000},	// #awbb_rpl_InvalidOutdoor off
               
               
//================================================================================================
//	Set AE 
//================================================================================================
// AE Target   
{0x002A, 0x0E88},
{0x0F12, 0x003C},	// #TVAR_ae_BrAve

// AE State Mode
{0x002A, 0x0E8E},
{0x0F12, 0x000F},	// #Ae_StatMode
               
// AE Weight   
{0x002A, 0x0E96},
{0x0F12, 0x0101},	// #ae_WeightTbl_16_0_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_1_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_2_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_3_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_4_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_5_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_6_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_7_
{0x0F12, 0x0101},	// #ae_WeightTbl_16_8_
{0x0F12, 0x0303},	// #ae_WeightTbl_16_9_
{0x0F12, 0x0303},	// #ae_WeightTbl_16_10
{0x0F12, 0x0101},	// #ae_WeightTbl_16_11
{0x0F12, 0x0101},	// #ae_WeightTbl_16_12
{0x0F12, 0x0303},	// #ae_WeightTbl_16_13
{0x0F12, 0x0303},	// #ae_WeightTbl_16_14
{0x0F12, 0x0101},	// #ae_WeightTbl_16_15
{0x0F12, 0x0101},	// #ae_WeightTbl_16_16
{0x0F12, 0x0303},	// #ae_WeightTbl_16_17
{0x0F12, 0x0303},	// #ae_WeightTbl_16_18
{0x0F12, 0x0101},	// #ae_WeightTbl_16_19
{0x0F12, 0x0101},	// #ae_WeightTbl_16_20
{0x0F12, 0x0303},	// #ae_WeightTbl_16_21
{0x0F12, 0x0303},	// #ae_WeightTbl_16_22
{0x0F12, 0x0101},	// #ae_WeightTbl_16_23
{0x0F12, 0x0101},	// #ae_WeightTbl_16_24
{0x0F12, 0x0101},	// #ae_WeightTbl_16_25
{0x0F12, 0x0101},	// #ae_WeightTbl_16_26
{0x0F12, 0x0101},	// #ae_WeightTbl_16_27
{0x0F12, 0x0101},	// #ae_WeightTbl_16_28
{0x0F12, 0x0101},	// #ae_WeightTbl_16_29
{0x0F12, 0x0101},	// #ae_WeightTbl_16_30
{0x0F12, 0x0101},	// #ae_WeightTbl_16_31
               
               
//================================================================================================
//	Set Gamma LUT
//================================================================================================
{0x002A, 0x05DC},
{0x0F12, 0x0000},	// #SARR_usGammaLutRGBIndoor[0]
{0x0F12, 0x0005},	// #SARR_usGammaLutRGBIndoor[1]
{0x0F12, 0x000D},	// #SARR_usGammaLutRGBIndoor[2]
{0x0F12, 0x0020},	// #SARR_usGammaLutRGBIndoor[3]
{0x0F12, 0x004F},	// #SARR_usGammaLutRGBIndoor[4]
{0x0F12, 0x00B5},	// #SARR_usGammaLutRGBIndoor[5]
{0x0F12, 0x010B},	// #SARR_usGammaLutRGBIndoor[6]
{0x0F12, 0x0130},	// #SARR_usGammaLutRGBIndoor[7]
{0x0F12, 0x0152},	// #SARR_usGammaLutRGBIndoor[8]
{0x0F12, 0x0186},	// #SARR_usGammaLutRGBIndoor[9]
{0x0F12, 0x01B5},	// #SARR_usGammaLutRGBIndoor[10]
{0x0F12, 0x01E1},	// #SARR_usGammaLutRGBIndoor[11]
{0x0F12, 0x020A},	// #SARR_usGammaLutRGBIndoor[12]
{0x0F12, 0x0252},	// #SARR_usGammaLutRGBIndoor[13]
{0x0F12, 0x0293},	// #SARR_usGammaLutRGBIndoor[14]
{0x0F12, 0x02EE},	// #SARR_usGammaLutRGBIndoor[15]
{0x0F12, 0x033D},	// #SARR_usGammaLutRGBIndoor[16]
{0x0F12, 0x0384},	// #SARR_usGammaLutRGBIndoor[17]
{0x0F12, 0x03C4},	// #SARR_usGammaLutRGBIndoor[18]
{0x0F12, 0x03FF},	// #SARR_usGammaLutRGBIndoor[19]
{0x0F12, 0x0000},	// #SARR_usGammaLutRGBIndoor[20]
{0x0F12, 0x0005},	// #SARR_usGammaLutRGBIndoor[21]
{0x0F12, 0x000D},	// #SARR_usGammaLutRGBIndoor[22]
{0x0F12, 0x0020},	// #SARR_usGammaLutRGBIndoor[23]
{0x0F12, 0x004F},	// #SARR_usGammaLutRGBIndoor[24]
{0x0F12, 0x00B5},	// #SARR_usGammaLutRGBIndoor[25]
{0x0F12, 0x010B},	// #SARR_usGammaLutRGBIndoor[26]
{0x0F12, 0x0130},	// #SARR_usGammaLutRGBIndoor[27]
{0x0F12, 0x0152},	// #SARR_usGammaLutRGBIndoor[28]
{0x0F12, 0x0186},	// #SARR_usGammaLutRGBIndoor[29]
{0x0F12, 0x01B5},	// #SARR_usGammaLutRGBIndoor[30]
{0x0F12, 0x01E1},	// #SARR_usGammaLutRGBIndoor[31]
{0x0F12, 0x020A},	// #SARR_usGammaLutRGBIndoor[32]
{0x0F12, 0x0252},	// #SARR_usGammaLutRGBIndoor[33]
{0x0F12, 0x0293},	// #SARR_usGammaLutRGBIndoor[34]
{0x0F12, 0x02EE},	// #SARR_usGammaLutRGBIndoor[35]
{0x0F12, 0x033D},	// #SARR_usGammaLutRGBIndoor[36]
{0x0F12, 0x0384},	// #SARR_usGammaLutRGBIndoor[37]
{0x0F12, 0x03C4},	// #SARR_usGammaLutRGBIndoor[38]
{0x0F12, 0x03FF},	// #SARR_usGammaLutRGBIndoor[39]
{0x0F12, 0x0000},	// #SARR_usGammaLutRGBIndoor[40]
{0x0F12, 0x0005},	// #SARR_usGammaLutRGBIndoor[41]
{0x0F12, 0x000D},	// #SARR_usGammaLutRGBIndoor[42]
{0x0F12, 0x0020},	// #SARR_usGammaLutRGBIndoor[43]
{0x0F12, 0x004F},	// #SARR_usGammaLutRGBIndoor[44]
{0x0F12, 0x00B5},	// #SARR_usGammaLutRGBIndoor[45]
{0x0F12, 0x010B},	// #SARR_usGammaLutRGBIndoor[46]
{0x0F12, 0x0130},	// #SARR_usGammaLutRGBIndoor[47]
{0x0F12, 0x0152},	// #SARR_usGammaLutRGBIndoor[48]
{0x0F12, 0x0186},	// #SARR_usGammaLutRGBIndoor[49]
{0x0F12, 0x01B5},	// #SARR_usGammaLutRGBIndoor[50]
{0x0F12, 0x01E1},	// #SARR_usGammaLutRGBIndoor[51]
{0x0F12, 0x020A},	// #SARR_usGammaLutRGBIndoor[52]
{0x0F12, 0x0252},	// #SARR_usGammaLutRGBIndoor[53]
{0x0F12, 0x0293},	// #SARR_usGammaLutRGBIndoor[54]
{0x0F12, 0x02EE},	// #SARR_usGammaLutRGBIndoor[55]
{0x0F12, 0x033D},	// #SARR_usGammaLutRGBIndoor[56]
{0x0F12, 0x0384},	// #SARR_usGammaLutRGBIndoor[57]
{0x0F12, 0x03C4},	// #SARR_usGammaLutRGBIndoor[58]
{0x0F12, 0x03FF},	// #SARR_usGammaLutRGBIndoor[59]

{0x002A, 0x06E6},
{0x0F12, 0x00A0},	// #SARR_AwbCcmCord[0]
{0x0F12, 0x00D0},	// #SARR_AwbCcmCord[1]
{0x0F12, 0x00E0},	// #SARR_AwbCcmCord[2]
{0x0F12, 0x0112},	// #SARR_AwbCcmCord[3]
{0x0F12, 0x013B},	// #SARR_AwbCcmCord[4]
{0x0F12, 0x0165},	// #SARR_AwbCcmCord[5]

{0x002A, 0x0F06},
{0x0F12, 0x00AD},	// #SARR_IllumType[0]
{0x0F12, 0x00BA},	// #SARR_IllumType[1]
{0x0F12, 0x00D4},	// #SARR_IllumType[2]
{0x0F12, 0x0104},	// #SARR_IllumType[3]
{0x0F12, 0x013B},	// #SARR_IllumType[4]
{0x0F12, 0x016D},	// #SARR_IllumType[5]
{0x0F12, 0x0185},	// #SARR_IllumType[6]

{0x0F12, 0x01D6},	// #SARR_IllumTypeF[0]
{0x0F12, 0x0100},	// #SARR_IllumTypeF[1]
{0x0F12, 0x0122},	// #SARR_IllumTypeF[2]
{0x0F12, 0x00F1},	// #SARR_IllumTypeF[3]
{0x0F12, 0x0100},	// #SARR_IllumTypeF[4]
{0x0F12, 0x0100},	// #SARR_IllumTypeF[5]
{0x0F12, 0x0100},	// #SARR_IllumTypeF[6]


//==, 0x====},=========================================================================================
//	 0xSet },AFIT table
//==, 0x====},=========================================================================================
//	 0xpara},m_start	afit_uNoiseIndInDoor
{0x002A, 0x07A8},
{0x0F12, 0x000A},	// #afit_uNoiseIndInDoor[0]      //                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          
{0x0F12, 0x0019},	// #afit_uNoiseIndInDoor[1]
{0x0F12, 0x007D},	// #afit_uNoiseIndInDoor[2]
{0x0F12, 0x01F4},	// #afit_uNoiseIndInDoor[3]
{0x0F12, 0x1388},	// #afit_uNoiseIndInDoor[4]

{0x002A, 0x07EC},	
{0x0F12, 0x0005},	// #TVAR_afit_pBaseVals[0]	// brightness
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[1]	// contrast
{0x0F12, 0x000A},	// #TVAR_afit_pBaseVals[2]	// saturation
{0x0F12, 0x0009},	// #TVAR_afit_pBaseVals[3]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[4]
{0x0F12, 0x0032},	// #TVAR_afit_pBaseVals[5]
{0x0F12, 0x03FF},	// #TVAR_afit_pBaseVals[6]
{0x0F12, 0x007A},	// #TVAR_afit_pBaseVals[7]
{0x0F12, 0x012C},	// #TVAR_afit_pBaseVals[8]
{0x0F12, 0x002A},	// #TVAR_afit_pBaseVals[9]
{0x0F12, 0x0078},	// #TVAR_afit_pBaseVals[10]
{0x0F12, 0x0019},	// #TVAR_afit_pBaseVals[11]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[12]
{0x0F12, 0x01F4},	// #TVAR_afit_pBaseVals[13]
{0x0F12, 0x00D5},	// #TVAR_afit_pBaseVals[14]
{0x0F12, 0x00DE},	// #TVAR_afit_pBaseVals[15]
{0x0F12, 0x0100},	// #TVAR_afit_pBaseVals[16]
{0x0F12, 0x00F3},	// #TVAR_afit_pBaseVals[17]
{0x0F12, 0x0020},	// #TVAR_afit_pBaseVals[18]
{0x0F12, 0x0078},	// #TVAR_afit_pBaseVals[19]
{0x0F12, 0x0070},	// #TVAR_afit_pBaseVals[20]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[21]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[22]
{0x0F12, 0x01CE},	// #TVAR_afit_pBaseVals[23]
{0x0F12, 0x003A},	// #TVAR_afit_pBaseVals[24]
{0x0F12, 0xF804},	// #TVAR_afit_pBaseVals[25]
{0x0F12, 0x010C},	// #TVAR_afit_pBaseVals[26]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[27]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[28]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[29]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[30]
{0x0F12, 0x0200},	// #TVAR_afit_pBaseVals[31]
{0x0F12, 0x3202},	// #TVAR_afit_pBaseVals[32]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[33]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[34]
{0x0F12, 0x8E1E},	// #TVAR_afit_pBaseVals[35]
{0x0F12, 0x481E},	// #TVAR_afit_pBaseVals[36]
{0x0F12, 0x4C33},	// #TVAR_afit_pBaseVals[37]
{0x0F12, 0xC83C},	// #TVAR_afit_pBaseVals[38]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[39]
{0x0F12, 0x00FF},	// #TVAR_afit_pBaseVals[40]
{0x0F12, 0x0F32},	// #TVAR_afit_pBaseVals[41]
{0x0F12, 0x1414},	// #TVAR_afit_pBaseVals[42]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[43]
{0x0F12, 0x8002},	// #TVAR_afit_pBaseVals[44]
{0x0F12, 0x061E},	// #TVAR_afit_pBaseVals[45]
{0x0F12, 0x0A1E},	// #TVAR_afit_pBaseVals[46]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[47]
{0x0F12, 0x0505},	// #TVAR_afit_pBaseVals[48]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[49]
{0x0F12, 0x0A0A},	// #TVAR_afit_pBaseVals[50]
{0x0F12, 0x0404},	// #TVAR_afit_pBaseVals[51]
{0x0F12, 0x0007},	// #TVAR_afit_pBaseVals[52]
{0x0F12, 0x0500},	// #TVAR_afit_pBaseVals[53]
{0x0F12, 0xB200},	// #TVAR_afit_pBaseVals[54]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[55]
{0x0F12, 0x0808},	// #TVAR_afit_pBaseVals[56]
{0x0F12, 0x1008},	// #TVAR_afit_pBaseVals[57]
{0x0F12, 0xFF91},	// #TVAR_afit_pBaseVals[58]
{0x0F12, 0xFF9B},	// #TVAR_afit_pBaseVals[59]
{0x0F12, 0x0606},	// #TVAR_afit_pBaseVals[60]
{0x0F12, 0x4123},	// #TVAR_afit_pBaseVals[61]
{0x0F12, 0x4123},	// #TVAR_afit_pBaseVals[62]
{0x0F12, 0x0A90},	// #TVAR_afit_pBaseVals[63]
{0x0F12, 0x0A23},	// #TVAR_afit_pBaseVals[64]
{0x0F12, 0x2C00},	// #TVAR_afit_pBaseVals[65]
{0x0F12, 0x2E00},	// #TVAR_afit_pBaseVals[66]
{0x0F12, 0x0200},	// #TVAR_afit_pBaseVals[67]
{0x0F12, 0x40FF},	// #TVAR_afit_pBaseVals[68]
{0x0F12, 0x0F40},	// #TVAR_afit_pBaseVals[69]
{0x0F12, 0x100F},	// #TVAR_afit_pBaseVals[70]
{0x0F12, 0x0F00},	// #TVAR_afit_pBaseVals[71]
{0x0F12, 0x0018},	// #TVAR_afit_pBaseVals[72]
{0x0F12, 0x0900},	// #TVAR_afit_pBaseVals[73]
{0x0F12, 0x0902},	// #TVAR_afit_pBaseVals[74]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[75]
{0x0F12, 0x0600},	// #TVAR_afit_pBaseVals[76]
{0x0F12, 0x0201},	// #TVAR_afit_pBaseVals[77]
{0x0F12, 0x4003},	// #TVAR_afit_pBaseVals[78]
{0x0F12, 0x005A},	// #TVAR_afit_pBaseVals[79]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[80]
{0x0F12, 0x8080},	// #TVAR_afit_pBaseVals[81]
{0x0F12, 0x0005},	// #TVAR_afit_pBaseVals[82]		// brightness
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[83]     	// contrast  
{0x0F12, 0x000A},	// #TVAR_afit_pBaseVals[84]     	// saturation
{0x0F12, 0x000F},	// #TVAR_afit_pBaseVals[85]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[86]
{0x0F12, 0x0032},	// #TVAR_afit_pBaseVals[87]
{0x0F12, 0x03FF},	// #TVAR_afit_pBaseVals[88]
{0x0F12, 0x00DF},	// #TVAR_afit_pBaseVals[89]
{0x0F12, 0x012C},	// #TVAR_afit_pBaseVals[90]
{0x0F12, 0x004B},	// #TVAR_afit_pBaseVals[91]
{0x0F12, 0x0078},	// #TVAR_afit_pBaseVals[92]
{0x0F12, 0x0019},	// #TVAR_afit_pBaseVals[93]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[94]
{0x0F12, 0x01F4},	// #TVAR_afit_pBaseVals[95]
{0x0F12, 0x00D5},	// #TVAR_afit_pBaseVals[96]
{0x0F12, 0x00DE},	// #TVAR_afit_pBaseVals[97]
{0x0F12, 0x0100},	// #TVAR_afit_pBaseVals[98]
{0x0F12, 0x00F3},	// #TVAR_afit_pBaseVals[99]
{0x0F12, 0x0020},	// #TVAR_afit_pBaseVals[100]
{0x0F12, 0x0078},	// #TVAR_afit_pBaseVals[101]
{0x0F12, 0x0070},	// #TVAR_afit_pBaseVals[102]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[103]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[104]
{0x0F12, 0x01CE},	// #TVAR_afit_pBaseVals[105]
{0x0F12, 0x003A},	// #TVAR_afit_pBaseVals[106]
{0x0F12, 0xF804},	// #TVAR_afit_pBaseVals[107]
{0x0F12, 0x010C},	// #TVAR_afit_pBaseVals[108]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[109]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[110]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[111]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[112]
{0x0F12, 0x0200},	// #TVAR_afit_pBaseVals[113]
{0x0F12, 0x3202},	// #TVAR_afit_pBaseVals[114]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[115]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[116]
{0x0F12, 0x4C1E},	// #TVAR_afit_pBaseVals[117]
{0x0F12, 0x271D},	// #TVAR_afit_pBaseVals[118]
{0x0F12, 0x342A},	// #TVAR_afit_pBaseVals[119]
{0x0F12, 0x6533},	// #TVAR_afit_pBaseVals[120]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[121]
{0x0F12, 0x00FF},	// #TVAR_afit_pBaseVals[122]
{0x0F12, 0x0F32},	// #TVAR_afit_pBaseVals[123]
{0x0F12, 0x1414},	// #TVAR_afit_pBaseVals[124]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[125]
{0x0F12, 0x8002},	// #TVAR_afit_pBaseVals[126]
{0x0F12, 0x061E},	// #TVAR_afit_pBaseVals[127]
{0x0F12, 0x0A1E},	// #TVAR_afit_pBaseVals[128]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[129]
{0x0F12, 0x0505},	// #TVAR_afit_pBaseVals[130]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[131]
{0x0F12, 0x0A0A},	// #TVAR_afit_pBaseVals[132]
{0x0F12, 0x0404},	// #TVAR_afit_pBaseVals[133]
{0x0F12, 0x0006},	// #TVAR_afit_pBaseVals[134]
{0x0F12, 0x0408},	// #TVAR_afit_pBaseVals[135]
{0x0F12, 0x7010},	// #TVAR_afit_pBaseVals[136]
{0x0F12, 0x0880},	// #TVAR_afit_pBaseVals[137]
{0x0F12, 0x0808},	// #TVAR_afit_pBaseVals[138]
{0x0F12, 0x1008},	// #TVAR_afit_pBaseVals[139]
{0x0F12, 0xFF91},	// #TVAR_afit_pBaseVals[140]
{0x0F12, 0xFF9B},	// #TVAR_afit_pBaseVals[141]
{0x0F12, 0x0606},	// #TVAR_afit_pBaseVals[142]
{0x0F12, 0x4132},	// #TVAR_afit_pBaseVals[143]
{0x0F12, 0x4132},	// #TVAR_afit_pBaseVals[144]
{0x0F12, 0x0590},	// #TVAR_afit_pBaseVals[145]
{0x0F12, 0x0523},	// #TVAR_afit_pBaseVals[146]
{0x0F12, 0x2700},	// #TVAR_afit_pBaseVals[147]
{0x0F12, 0x2700},	// #TVAR_afit_pBaseVals[148]
{0x0F12, 0x0200},	// #TVAR_afit_pBaseVals[149]
{0x0F12, 0x40FF},	// #TVAR_afit_pBaseVals[150]
{0x0F12, 0x0F40},	// #TVAR_afit_pBaseVals[151]
{0x0F12, 0x100F},	// #TVAR_afit_pBaseVals[152]
{0x0F12, 0x0F00},	// #TVAR_afit_pBaseVals[153]
{0x0F12, 0x0018},	// #TVAR_afit_pBaseVals[154]
{0x0F12, 0x0900},	// #TVAR_afit_pBaseVals[155]
{0x0F12, 0x0902},	// #TVAR_afit_pBaseVals[156]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[157]
{0x0F12, 0x0600},	// #TVAR_afit_pBaseVals[158]
{0x0F12, 0x0201},	// #TVAR_afit_pBaseVals[159]
{0x0F12, 0x3703},	// #TVAR_afit_pBaseVals[160]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[161]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[162]
{0x0F12, 0x8080},	// #TVAR_afit_pBaseVals[163]
{0x0F12, 0x0005},	// #TVAR_afit_pBaseVals[164]		// brightness
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[165]    	// contrast  
{0x0F12, 0x0014},   // #TVAR_afit_pBaseVals[166]    	// saturation
{0x0F12, 0x000F},	// #TVAR_afit_pBaseVals[167]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[168]
{0x0F12, 0x0032},	// #TVAR_afit_pBaseVals[169]
{0x0F12, 0x03FF},	// #TVAR_afit_pBaseVals[170]
{0x0F12, 0x0130},	// #TVAR_afit_pBaseVals[171]
{0x0F12, 0x012C},	// #TVAR_afit_pBaseVals[172]
{0x0F12, 0x0074},	// #TVAR_afit_pBaseVals[173]
{0x0F12, 0x0078},	// #TVAR_afit_pBaseVals[174]
{0x0F12, 0x0019},	// #TVAR_afit_pBaseVals[175]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[176]
{0x0F12, 0x01F4},	// #TVAR_afit_pBaseVals[177]
{0x0F12, 0x0064},	// #TVAR_afit_pBaseVals[178]
{0x0F12, 0x0064},	// #TVAR_afit_pBaseVals[179]
{0x0F12, 0x0100},	// #TVAR_afit_pBaseVals[180]
{0x0F12, 0x00AA},	// #TVAR_afit_pBaseVals[181]
{0x0F12, 0x0020},	// #TVAR_afit_pBaseVals[182]
{0x0F12, 0x003C},	// #TVAR_afit_pBaseVals[183]
{0x0F12, 0x0070},	// #TVAR_afit_pBaseVals[184]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[185]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[186]
{0x0F12, 0x01CE},	// #TVAR_afit_pBaseVals[187]
{0x0F12, 0x003A},	// #TVAR_afit_pBaseVals[188]
{0x0F12, 0xF804},	// #TVAR_afit_pBaseVals[189]
{0x0F12, 0x010C},	// #TVAR_afit_pBaseVals[190]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[191]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[192]
{0x0F12, 0x0500},	// #TVAR_afit_pBaseVals[193]
{0x0F12, 0x0505},	// #TVAR_afit_pBaseVals[194]
{0x0F12, 0x0205},	// #TVAR_afit_pBaseVals[195]
{0x0F12, 0x3202},	// #TVAR_afit_pBaseVals[196]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[197]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[198]
{0x0F12, 0x191C},	// #TVAR_afit_pBaseVals[199]
{0x0F12, 0x191B},	// #TVAR_afit_pBaseVals[200]
{0x0F12, 0x2222},	// #TVAR_afit_pBaseVals[201]
{0x0F12, 0x3228},	// #TVAR_afit_pBaseVals[202]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[203]
{0x0F12, 0x00FF},	// #TVAR_afit_pBaseVals[204]
{0x0F12, 0x0F32},	// #TVAR_afit_pBaseVals[205]
{0x0F12, 0x1414},	// #TVAR_afit_pBaseVals[206]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[207]
{0x0F12, 0x8002},	// #TVAR_afit_pBaseVals[208]
{0x0F12, 0x061E},	// #TVAR_afit_pBaseVals[209]
{0x0F12, 0x0A1E},	// #TVAR_afit_pBaseVals[210]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[211]
{0x0F12, 0x0505},	// #TVAR_afit_pBaseVals[212]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[213]
{0x0F12, 0x0A0A},	// #TVAR_afit_pBaseVals[214]
{0x0F12, 0x0404},	// #TVAR_afit_pBaseVals[215]
{0x0F12, 0x1106},	// #TVAR_afit_pBaseVals[216]
{0x0F12, 0x0F08},	// #TVAR_afit_pBaseVals[217]
{0x0F12, 0x2510},	// #TVAR_afit_pBaseVals[218]
{0x0F12, 0x0880},	// #TVAR_afit_pBaseVals[219]
{0x0F12, 0x0808},	// #TVAR_afit_pBaseVals[220]
{0x0F12, 0x1008},	// #TVAR_afit_pBaseVals[221]
{0x0F12, 0xFF33},	// #TVAR_afit_pBaseVals[222]
{0x0F12, 0x007E},	// #TVAR_afit_pBaseVals[223]
{0x0F12, 0x0605},	// #TVAR_afit_pBaseVals[224]
{0x0F12, 0x4137},	// #TVAR_afit_pBaseVals[225]
{0x0F12, 0x4137},	// #TVAR_afit_pBaseVals[226]
{0x0F12, 0x0A90},	// #TVAR_afit_pBaseVals[227]
{0x0F12, 0x0923},	// #TVAR_afit_pBaseVals[228]
{0x0F12, 0x1E00},	// #TVAR_afit_pBaseVals[229]
{0x0F12, 0x1B00},	// #TVAR_afit_pBaseVals[230]
{0x0F12, 0x0200},	// #TVAR_afit_pBaseVals[231]
{0x0F12, 0x40FF},	// #TVAR_afit_pBaseVals[232]
{0x0F12, 0x0F40},	// #TVAR_afit_pBaseVals[233]
{0x0F12, 0x100F},	// #TVAR_afit_pBaseVals[234]
{0x0F12, 0x0F00},	// #TVAR_afit_pBaseVals[235]
{0x0F12, 0x0018},	// #TVAR_afit_pBaseVals[236]
{0x0F12, 0x0900},	// #TVAR_afit_pBaseVals[237]
{0x0F12, 0x0902},	// #TVAR_afit_pBaseVals[238]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[239]
{0x0F12, 0x0600},	// #TVAR_afit_pBaseVals[240]
{0x0F12, 0x0201},	// #TVAR_afit_pBaseVals[241]
{0x0F12, 0x3403},	// #TVAR_afit_pBaseVals[242]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[243]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[244]
{0x0F12, 0x8080},	// #TVAR_afit_pBaseVals[245]
{0x0F12, 0x0005},	// #TVAR_afit_pBaseVals[246]		// brightness
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[247]    	// contrast  
{0x0F12, 0x0014},   // #TVAR_afit_pBaseVals[248]    	// saturation
{0x0F12, 0x000F},	// #TVAR_afit_pBaseVals[249]
{0x0F12, 0xFFF8},	// #TVAR_afit_pBaseVals[250]
{0x0F12, 0x0032},	// #TVAR_afit_pBaseVals[251]
{0x0F12, 0x03FF},	// #TVAR_afit_pBaseVals[252]
{0x0F12, 0x0144},	// #TVAR_afit_pBaseVals[253]
{0x0F12, 0x012C},	// #TVAR_afit_pBaseVals[254]
{0x0F12, 0x0078},	// #TVAR_afit_pBaseVals[255]
{0x0F12, 0x0078},	// #TVAR_afit_pBaseVals[256]
{0x0F12, 0x0019},	// #TVAR_afit_pBaseVals[257]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[258]
{0x0F12, 0x01F4},	// #TVAR_afit_pBaseVals[259]
{0x0F12, 0x0009},	// #TVAR_afit_pBaseVals[260]
{0x0F12, 0x0012},	// #TVAR_afit_pBaseVals[261]
{0x0F12, 0x0100},	// #TVAR_afit_pBaseVals[262]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[263]
{0x0F12, 0x0020},	// #TVAR_afit_pBaseVals[264]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[265]
{0x0F12, 0x0070},	// #TVAR_afit_pBaseVals[266]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[267]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[268]
{0x0F12, 0x01CE},	// #TVAR_afit_pBaseVals[269]
{0x0F12, 0x003A},	// #TVAR_afit_pBaseVals[270]
{0x0F12, 0xF804},	// #TVAR_afit_pBaseVals[271]
{0x0F12, 0x010C},	// #TVAR_afit_pBaseVals[272]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[273]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[274]
{0x0F12, 0x0500},	// #TVAR_afit_pBaseVals[275]
{0x0F12, 0x0505},	// #TVAR_afit_pBaseVals[276]
{0x0F12, 0x0205},	// #TVAR_afit_pBaseVals[277]
{0x0F12, 0x3202},	// #TVAR_afit_pBaseVals[278]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[279]
{0x0F12, 0x0007},	// #TVAR_afit_pBaseVals[280]
{0x0F12, 0x1913},	// #TVAR_afit_pBaseVals[281]
{0x0F12, 0x1913},	// #TVAR_afit_pBaseVals[282]
{0x0F12, 0x140A},	// #TVAR_afit_pBaseVals[283]
{0x0F12, 0x140F},	// #TVAR_afit_pBaseVals[284]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[285]
{0x0F12, 0x00FF},	// #TVAR_afit_pBaseVals[286]
{0x0F12, 0x0F32},	// #TVAR_afit_pBaseVals[287]
{0x0F12, 0x1414},	// #TVAR_afit_pBaseVals[288]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[289]
{0x0F12, 0x8007},	// #TVAR_afit_pBaseVals[290]
{0x0F12, 0x061E},	// #TVAR_afit_pBaseVals[291]
{0x0F12, 0x0A1E},	// #TVAR_afit_pBaseVals[292]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[293]
{0x0F12, 0x0A05},	// #TVAR_afit_pBaseVals[294]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[295]
{0x0F12, 0x0A0A},	// #TVAR_afit_pBaseVals[296]
{0x0F12, 0x0404},	// #TVAR_afit_pBaseVals[297]
{0x0F12, 0x1306},	// #TVAR_afit_pBaseVals[298]
{0x0F12, 0x1110},	// #TVAR_afit_pBaseVals[299]
{0x0F12, 0x0F18},	// #TVAR_afit_pBaseVals[300]
{0x0F12, 0x0880},	// #TVAR_afit_pBaseVals[301]
{0x0F12, 0x0808},	// #TVAR_afit_pBaseVals[302]
{0x0F12, 0x1008},	// #TVAR_afit_pBaseVals[303]
{0x0F12, 0x001E},	// #TVAR_afit_pBaseVals[304]
{0x0F12, 0x001E},	// #TVAR_afit_pBaseVals[305]
{0x0F12, 0x0605},	// #TVAR_afit_pBaseVals[306]
{0x0F12, 0x414C},	// #TVAR_afit_pBaseVals[307]
{0x0F12, 0x4148},	// #TVAR_afit_pBaseVals[308]
{0x0F12, 0x2F86},	// #TVAR_afit_pBaseVals[309]
{0x0F12, 0x291F},	// #TVAR_afit_pBaseVals[310]
{0x0F12, 0x0A00},	// #TVAR_afit_pBaseVals[311]
{0x0F12, 0x0A00},	// #TVAR_afit_pBaseVals[312]
{0x0F12, 0x0200},	// #TVAR_afit_pBaseVals[313]
{0x0F12, 0x40FF},	// #TVAR_afit_pBaseVals[314]
{0x0F12, 0x0F40},	// #TVAR_afit_pBaseVals[315]
{0x0F12, 0x100F},	// #TVAR_afit_pBaseVals[316]
{0x0F12, 0x0F00},	// #TVAR_afit_pBaseVals[317]
{0x0F12, 0x0018},	// #TVAR_afit_pBaseVals[318]
{0x0F12, 0x0900},	// #TVAR_afit_pBaseVals[319]
{0x0F12, 0x0903},	// #TVAR_afit_pBaseVals[320]
{0x0F12, 0x0002},	// #TVAR_afit_pBaseVals[321]
{0x0F12, 0x0800},	// #TVAR_afit_pBaseVals[322]
{0x0F12, 0x0106},	// #TVAR_afit_pBaseVals[323]
{0x0F12, 0x3402},	// #TVAR_afit_pBaseVals[324]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[325]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[326]
{0x0F12, 0x8080},	// #TVAR_afit_pBaseVals[327]
{0x0F12, 0x0005},	// #TVAR_afit_pBaseVals[328]		// brightness
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[329]    	// contrast  
{0x0F12, 0x0014}, 	// #TVAR_afit_pBaseVals[330]    	// saturation
{0x0F12, 0x0014},	// #TVAR_afit_pBaseVals[331]
{0x0F12, 0xFFF1},	// #TVAR_afit_pBaseVals[332]
{0x0F12, 0x0032},	// #TVAR_afit_pBaseVals[333]
{0x0F12, 0x03FF},	// #TVAR_afit_pBaseVals[334]
{0x0F12, 0x0158},	// #TVAR_afit_pBaseVals[335]
{0x0F12, 0x0158},	// #TVAR_afit_pBaseVals[336]
{0x0F12, 0x007E},	// #TVAR_afit_pBaseVals[337]
{0x0F12, 0x007E},	// #TVAR_afit_pBaseVals[338]
{0x0F12, 0x0019},	// #TVAR_afit_pBaseVals[339]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[340]
{0x0F12, 0x01F4},	// #TVAR_afit_pBaseVals[341]
{0x0F12, 0x0009},	// #TVAR_afit_pBaseVals[342]
{0x0F12, 0x0012},	// #TVAR_afit_pBaseVals[343]
{0x0F12, 0x0100},	// #TVAR_afit_pBaseVals[344]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[345]
{0x0F12, 0x0020},	// #TVAR_afit_pBaseVals[346]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[347]
{0x0F12, 0x0070},	// #TVAR_afit_pBaseVals[348]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[349]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[350]
{0x0F12, 0x01CE},	// #TVAR_afit_pBaseVals[351]
{0x0F12, 0x003A},	// #TVAR_afit_pBaseVals[352]
{0x0F12, 0xF804},	// #TVAR_afit_pBaseVals[353]
{0x0F12, 0x010C},	// #TVAR_afit_pBaseVals[354]
{0x0F12, 0x0003},	// #TVAR_afit_pBaseVals[355]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[356]
{0x0F12, 0x0500},	// #TVAR_afit_pBaseVals[357]
{0x0F12, 0x0505},	// #TVAR_afit_pBaseVals[358]
{0x0F12, 0x0205},	// #TVAR_afit_pBaseVals[359]
{0x0F12, 0x3202},	// #TVAR_afit_pBaseVals[360]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[361]
{0x0F12, 0x0007},	// #TVAR_afit_pBaseVals[362]
{0x0F12, 0x190F},	// #TVAR_afit_pBaseVals[363]
{0x0F12, 0x190F},	// #TVAR_afit_pBaseVals[364]
{0x0F12, 0x1403},	// #TVAR_afit_pBaseVals[365]
{0x0F12, 0x0A05},	// #TVAR_afit_pBaseVals[366]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[367]
{0x0F12, 0x00FF},	// #TVAR_afit_pBaseVals[368]
{0x0F12, 0x0F32},	// #TVAR_afit_pBaseVals[369]
{0x0F12, 0x1414},	// #TVAR_afit_pBaseVals[370]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[371]
{0x0F12, 0x8007},	// #TVAR_afit_pBaseVals[372]
{0x0F12, 0x061E},	// #TVAR_afit_pBaseVals[373]
{0x0F12, 0x0A1E},	// #TVAR_afit_pBaseVals[374]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[375]
{0x0F12, 0x0505},	// #TVAR_afit_pBaseVals[376]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[377]
{0x0F12, 0x0A0A},	// #TVAR_afit_pBaseVals[378]
{0x0F12, 0x0404},	// #TVAR_afit_pBaseVals[379]
{0x0F12, 0x1706},	// #TVAR_afit_pBaseVals[380]
{0x0F12, 0x1318},	// #TVAR_afit_pBaseVals[381]
{0x0F12, 0x0F18},	// #TVAR_afit_pBaseVals[382]
{0x0F12, 0x0880},	// #TVAR_afit_pBaseVals[383]
{0x0F12, 0x0808},	// #TVAR_afit_pBaseVals[384]
{0x0F12, 0x1008},	// #TVAR_afit_pBaseVals[385]
{0x0F12, 0x000F},	// #TVAR_afit_pBaseVals[386]
{0x0F12, 0x0000},	// #TVAR_afit_pBaseVals[387]
{0x0F12, 0x0605},	// #TVAR_afit_pBaseVals[388]
{0x0F12, 0x4163},	// #TVAR_afit_pBaseVals[389]
{0x0F12, 0x415E},	// #TVAR_afit_pBaseVals[390]
{0x0F12, 0x3C6B},	// #TVAR_afit_pBaseVals[391]
{0x0F12, 0x3C16},	// #TVAR_afit_pBaseVals[392]
{0x0F12, 0x0700},	// #TVAR_afit_pBaseVals[393]
{0x0F12, 0x0500},	// #TVAR_afit_pBaseVals[394]
{0x0F12, 0x0200},	// #TVAR_afit_pBaseVals[395]
{0x0F12, 0x40FF},	// #TVAR_afit_pBaseVals[396]
{0x0F12, 0x0F40},	// #TVAR_afit_pBaseVals[397]
{0x0F12, 0x100F},	// #TVAR_afit_pBaseVals[398]
{0x0F12, 0x0F00},	// #TVAR_afit_pBaseVals[399]
{0x0F12, 0x0018},	// #TVAR_afit_pBaseVals[400]
{0x0F12, 0x0900},	// #TVAR_afit_pBaseVals[401]
{0x0F12, 0x0903},	// #TVAR_afit_pBaseVals[402]
{0x0F12, 0x0002},	// #TVAR_afit_pBaseVals[403]
{0x0F12, 0x0800},	// #TVAR_afit_pBaseVals[404]
{0x0F12, 0x0106},	// #TVAR_afit_pBaseVals[405]
{0x0F12, 0x3402},	// #TVAR_afit_pBaseVals[406]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[407]
{0x0F12, 0x0080},	// #TVAR_afit_pBaseVals[408]
{0x0F12, 0x8080},	// #TVAR_afit_pBaseVals[409]

{0x0F12, 0x7C7E},	// #afit_pConstBaseVals[0]
{0x0F12, 0xF7BD},	// #afit_pConstBaseVals[1]
{0x0F12, 0xBE7C},	// #afit_pConstBaseVals[2]
{0x0F12, 0x95BD},	// #afit_pConstBaseVals[3]
{0x0F12, 0x7C32},	// #afit_pConstBaseVals[4]
{0x0F12, 0x0003},	// #afit_pConstBaseVals[5]
            
//002A 0E62
//0F12 000	// Global Rgain_offset
//0F12 0020	// Global Bgain_offset

// Update Changed Registers
{0x002A, 0x0532},
{0x0F12, 0x0001},	// #REG_TC_DBG_ReInitCmd
{0x002A, 0x0532},
{0x0F12, 0x0001},	// #REG_TC_DBG_ReInitCmd
{0x002A, 0x0532},
{0x0F12, 0x0001},	// #REG_TC_DBG_ReInitCmd
// ---------------------------------------------------------------------------- End of 4CDGX EVT1.0 initializationion

};

const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_antibanding_off_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x04BA},
    {0x0F12,0x0000}, // #REG_SF_USER_FlickerQuant
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_antibanding_50hz_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x04BA},
    {0x0F12,0x0001}, // #REG_SF_USER_FlickerQuant
};
const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_antibanding_60hz_reg_config[] =
{
    {0x0028,0x7000},
    {0x002A,0x04BA},
    {0x0F12,0x0002}, // #REG_SF_USER_FlickerQuant
};

static int s5k4cdgx_i2c_rxdata(unsigned short saddr,
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

	if (i2c_transfer(s5k4cdgx_client->adapter, msgs, 2) < 0) {
		CDBG("s5k4cdgx_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t s5k4cdgx_i2c_read_w(unsigned short raddr, unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = s5k4cdgx_i2c_rxdata(s5k4cdgx_client->addr, buf, 2);
	if (rc < 0)
		return rc;

	*rdata = buf[0] << 8 | buf[1];

	if (rc < 0)
		CDBG("s5k4cdgx_i2c_read failed!\n");

	return rc;
}

static int32_t s5k4cdgx_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	int i=0;
	struct i2c_msg msg[] = {
	{
		.addr = saddr,
		.flags = 0,
		.len = length,
		.buf = txdata,
	},
	};

	for( i = 0; i < 10; i++) {
		if (i2c_transfer(s5k4cdgx_client->adapter, msg, 1) >= 0) {
		    return 0;
		}else{
		  CDBG("s5k4cdgx_i2c_txdata faild(%d)\n",i);
		}
	}

	CDBG("s5k4cdgx_i2c_txdata faild\n");
	return -EIO;
}

static int32_t s5k4cdgx_i2c_write_w(unsigned short waddr, unsigned short wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00)>>8;
	buf[3] = (wdata & 0x00FF);

	rc = s5k4cdgx_i2c_txdata(s5k4cdgx_client->addr, buf, 4);

	if (rc < 0)
		CDBG("i2c_write_w failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t s5k4cdgx_i2c_write_w_table(
	struct s5k4cdgx_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = s5k4cdgx_i2c_write_w(reg_conf_tbl->waddr, reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}

	return rc;
}

int32_t s5k4cdgx_set_default_focus(uint8_t af_step)
{
    s5k4cdgx_i2c_write_w(0x0028 , 0x7000);                    
    s5k4cdgx_i2c_write_w(0x002A , 0x02C2);  //REG_TC_AF_AfCmd                    
    s5k4cdgx_i2c_write_w(0x0F12 , 0x0003);
    mdelay(10);

    if(af_step)
    {		
        s5k4cdgx_i2c_write_w(0x002A , 0x02C2);
        s5k4cdgx_i2c_write_w(0x0F12 , 0x0006);			
        s5k4cdgx_af_mode = CAMERA_AF_ON;
    }
    else
    {
        s5k4cdgx_i2c_write_w(0x002A , 0x02C2);
        s5k4cdgx_i2c_write_w(0x0F12 , 0x0002);		
        s5k4cdgx_af_mode = CAMERA_AF_OFF;
    }

    return 0;
}

int32_t s5k4cdgx_set_fps(struct fps_cfg    *fps)
{
    /* input is new fps in Q8 format */
    int32_t rc = 0;

    CDBG("s5k4cdgx_set_fps\n");
    return rc;
}

int32_t s5k4cdgx_write_exp_gain(uint16_t gain, uint32_t line)
{
    CDBG("s5k4cdgx_write_exp_gain\n");
    return 0;
}

int32_t s5k4cdgx_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
    int32_t rc = 0;

    CDBG("s5k4cdgx_set_pict_exp_gain\n");

    mdelay(10);

    /* camera_timed_wait(snapshot_wait*exposure_ratio); */
    return rc;
}

static int32_t s5k4cdgx_set_mirror_mode(void)
{
    int32_t rc = 0;

    const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_mirror_mode_reg_config0[] =
    {
        {0x002A,0x0310},
        {0x0F12,0x0000}, //#REG_0TC_PCFG_uPrevMirror
        {0x0F12,0x0000}, //#REG_0TC_PCFG_uCaptureMirror
    };
//    const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_mirror_mode_reg_config1[] =
//    {
//        {0x002A,0x310},	
//        {0x0F12,0x0003}, //#REG_0TC_PCFG_uPrevMirror
//        {0x0F12,0x0003}, //#REG_0TC_PCFG_uCaptureMirror
//    };
        rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_mirror_mode_reg_config0,
                    S5K4CDGX_ARRAY_SIZE(s5k4cdgx_mirror_mode_reg_config0));

    return rc;
}


int32_t s5k4cdgx_setting(enum s5k4cdgx_reg_update_t rupdate,
                       enum s5k4cdgx_setting_t    rt)
{
    int32_t rc = 0;


	down(&s5k4cdgx_sem2);
    if(rupdate == last_rupdate && rt == last_rt)
    {
        CDBG("s5k4cdgx_setting exit\n");
        up(&s5k4cdgx_sem2);
        return rc;
    }
    CDBG("s5k4cdgx_setting in rupdate=%d,rt=%d\n",rupdate,rt);
    switch (rupdate)
    {
        case UPDATE_PERIODIC:

            if(rt == RES_PREVIEW)
            {

                if(last_rt == RES_CAPTURE)
                {
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_before_preview_reg_config,
                                    S5K4CDGX_ARRAY_SIZE(s5k4cdgx_before_preview_reg_config));
                    mdelay(80);
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_preview_reg_config,
                                    S5K4CDGX_ARRAY_SIZE(s5k4cdgx_preview_reg_config));                                                   
                    //continue AF command 
                    s5k4cdgx_i2c_write_w(0x0028 , 0x7000);
                    s5k4cdgx_i2c_write_w(0x002A , 0x02C2);  //REG_TC_AF_AfCmd
                    s5k4cdgx_i2c_write_w(0x0F12 , 0x0003);
                    mdelay(10);
                    if(s5k4cdgx_af_mode == CAMERA_AF_ON)
                    {	
                        s5k4cdgx_i2c_write_w(0x002A , 0x02C2);
                        s5k4cdgx_i2c_write_w(0x0F12 , 0x0006);
                    }
                    mdelay(10);
                    break;
                }

                if(MODEL_LITEON == s5k4cdgx_model_id || MODEL_SUNNY== s5k4cdgx_model_id)
                {
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_init_reg_config1,
                                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_init_reg_config1));
                    mdelay(20);
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_init_reg_config2,
                                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_init_reg_config2));
                    mdelay(10);
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_init_reg_config3,
                                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_init_reg_config3));
                }else{
			printk("###########%s----%d: WARNNING  check modle id,now write s5k4cdgx reg config###############\n",__FUNCTION__,__LINE__);
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_init_reg_config1,
                                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_init_reg_config1));
                    mdelay(20);
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_init_reg_config2,
                                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_init_reg_config2));
                    mdelay(10);
                    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_init_reg_config3,
                                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_init_reg_config3));

		}
                CDBG("we are here s5k4cdgx_setting in rupdate=%d,rt=%d\n",rupdate,rt);
                rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_before_preview_reg_config,
                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_before_preview_reg_config));
                mdelay(80);
                rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_preview_reg_config,
                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_preview_reg_config));  
                //continue AF command 
                s5k4cdgx_i2c_write_w(0x0028 , 0x7000);
                s5k4cdgx_i2c_write_w(0x002A , 0x02C2);  //REG_TC_AF_AfCmd
                s5k4cdgx_i2c_write_w(0x0F12 , 0x0003);
                mdelay(10);
                if(s5k4cdgx_af_mode == CAMERA_AF_ON)
                {	
                    s5k4cdgx_i2c_write_w(0x002A , 0x02C2);
                    s5k4cdgx_i2c_write_w(0x0F12 , 0x0006);
                }
                mdelay(10);
            }
            else
            {
                //delete single focus
#if 0
            	//AF command 
                s5k4cdgx_i2c_write_w(0x0028 , 0x7000);
                s5k4cdgx_i2c_write_w(0x002A , 0x0F98);
                s5k4cdgx_i2c_write_w(0x0F12 , 0x00C0);
                s5k4cdgx_i2c_write_w(0x002A , 0x02C2);  //REG_TC_AF_AfCmd
                s5k4cdgx_i2c_write_w(0x0F12 , 0x0003);
                mdelay(10);
                s5k4cdgx_i2c_write_w(0x002A , 0x02C2);
                s5k4cdgx_i2c_write_w(0x0F12 , 0x0005);
                mdelay(10);
#endif
                s5k4cdgx_i2c_write_w(0x0028 , 0x7000);
                s5k4cdgx_i2c_write_w(0x002A , 0x02C2);  //REG_TC_AF_AfCmd
                s5k4cdgx_i2c_write_w(0x0F12 , 0x0001);
                mdelay(10);
                rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_snapshot_reg_config,
                                S5K4CDGX_ARRAY_SIZE(s5k4cdgx_snapshot_reg_config));
#if 0
                for(i = 0; i < 10; i++)
                {
                	s5k4cdgx_i2c_write_w(0x002C , 0x7000);
                	s5k4cdgx_i2c_write_w(0x02E , 0x233A);
                	rc = s5k4cdgx_i2c_read_w(0x0F12, &afstatus);
                	
                	if(0x0002 == afstatus)
                	  break;
                }
                if(10 == i)
                {
                	
                }
#endif
            }
            //mdelay(10);

            break;

        case REG_INIT:
            break;

        default:
            rc = -EFAULT;
            break;
    } /* switch (rupdate) */
    if(rc == 0)
    {
        last_rupdate = rupdate;
        last_rt = rt; 
    }
	up(&s5k4cdgx_sem2);

    return rc;
}

int32_t s5k4cdgx_video_config(int mode, int res)
{
    int32_t rc;

    switch (res)
    {
        case QTR_SIZE:
            rc = s5k4cdgx_setting(UPDATE_PERIODIC, RES_PREVIEW);
            if (rc < 0)
            {
                return rc;
            }
            break;

        case FULL_SIZE:
            rc = s5k4cdgx_setting(UPDATE_PERIODIC, RES_CAPTURE);
            if (rc < 0)
            {
                return rc;
            }

            break;

        default:
            return 0;
    } /* switch */

    s5k4cdgx_ctrl->prev_res   = res;
    s5k4cdgx_ctrl->curr_res   = res;
    s5k4cdgx_ctrl->sensormode = mode;

    return rc;
}

int32_t s5k4cdgx_snapshot_config(int mode)
{
    int32_t rc = 0;
    CDBG("s5k4cdgx_snapshot_config in\n");
    rc = s5k4cdgx_setting(UPDATE_PERIODIC, RES_CAPTURE);
    mdelay(50);
    if (rc < 0)
    {
        return rc;
    }

    s5k4cdgx_ctrl->curr_res = s5k4cdgx_ctrl->pict_res;

    s5k4cdgx_ctrl->sensormode = mode;

    return rc;
}
const static struct s5k4cdgx_i2c_reg_conf s5k4cdgx_standby_reg_config [] = {
		{0x0028,0xD000},
		{0x002A,0x107E},
		{0x0F12,0x0001}

};

int32_t s5k4cdgx_power_down(void)
{
    int32_t rc = 0;

//    rc = s5k4cdgx_i2c_write_w_table(s5k4cdgx_standby_reg_config ,
//                    S5K4CDGX_ARRAY_SIZE(s5k4cdgx_standby_reg_config ));
//del one line

    return rc;
}

int32_t s5k4cdgx_move_focus(int direction, int32_t num_steps)
{
    return 0;
}

static int s5k4cdgx_sensor_init_done(const struct msm_camera_sensor_info *data)
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
static int s5k4cdgx_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;
	unsigned short chipid;

	/* pull down power down */
	rc = gpio_request(data->sensor_pwd, "s5k4cdgx");
	if (!rc || rc == -EBUSY)
		gpio_direction_output(data->sensor_pwd, 1);
	else 
        goto init_probe_fail;

	rc = gpio_request(data->sensor_reset, "s5k4cdgx");
	if (!rc) {
		rc = gpio_direction_output(data->sensor_reset, 0);
	}
	else{
		printk("#############%s %d  gpio request error\n###########",__FUNCTION__,__LINE__);	
		goto init_probe_fail;
	}
 	if(rc){
		printk("#############%s %d  gpio output error\n###########",__FUNCTION__,__LINE__);	
	}else{
		printk("#############%s %d  gpio request OK\n###########",__FUNCTION__,__LINE__);	
	}


    mdelay(1);

    if (data->vreg_enable_func)
    {
        rc = data->vreg_enable_func(data->sensor_vreg, data->vreg_num);
        if (rc < 0)
        {
            goto init_probe_fail;
        }
    }
    
    mdelay(10);
    
    if(data->master_init_control_slave == NULL 
        || data->master_init_control_slave(data) == 0
        )
    {

        CDBG("s5k4cd hardware reset!!!");
        rc = gpio_direction_output(data->sensor_pwd, 0);
         if (rc < 0)
            goto init_probe_fail;

        mdelay(1);
        /*hardware reset*/
        rc = gpio_direction_output(data->sensor_reset, 1);
        if (rc < 0)
            goto init_probe_fail;

        mdelay(5);
                /*hardware reset*/
        rc = gpio_direction_output(data->sensor_reset, 0);
        if (rc < 0)
            goto init_probe_fail;

        mdelay(5);
        /*hardware reset*/
        rc = gpio_direction_output(data->sensor_reset, 1);
        if (rc < 0)
            goto init_probe_fail;

        mdelay(5);
    }

/*
	rc = s5k4cdgx_i2c_write_w(0x0010, 0x0001);
	if (rc < 0)
	{
        CDBG("s5k4cdgx_i2c_write_w 0x0010 0x0001 rc=%d", rc);
		goto init_probe_fail;
	}
    mdelay(10);
	rc = s5k4cdgx_i2c_write_w(0x0010, 0x0000);
	if (rc < 0)
	{
        CDBG("s5k4cdgx_i2c_write_w 0x0010 0x0000 rc=%d", rc);
		goto init_probe_fail;
	}
    mdelay(10);
*/
    
	rc = s5k4cdgx_i2c_write_w(0x002c, 0x0000);
	if (rc < 0)
	{
        CDBG("s5k4cdgx_i2c_write_w 0x002c rc=%d\n", rc);
		goto init_probe_fail;
	}
	rc = s5k4cdgx_i2c_write_w(0x002e, 0x0040);
	if (rc < 0)
	{
        CDBG("s5k4cdgx_i2c_write_w 0x002e rc=%d\n", rc);

		goto init_probe_fail;
	}
	/* 3. Read sensor Model ID: */
	rc = s5k4cdgx_i2c_read_w(0x0f12, &chipid);
	if (rc < 0)
	{
        CDBG("s5k4cdgx_i2c_read_w Model_ID failed!! rc=%d", rc);
		goto init_probe_fail;
	}
	CDBG("s5k4cdgx chipid = 0x%x\n", chipid);

	/* 4. Compare sensor ID to S5K5CA ID: */
	if (chipid != S5K4CDGX_CHIP_ID)
	{
		CDBG("s5k4cdgx Model_ID  error!!");
		rc = -ENODEV;
		goto init_probe_fail;
	}

	rc = s5k4cdgx_i2c_write_w(0x0028, 0xD000);
	if (rc < 0)
		goto init_probe_fail;
	rc = s5k4cdgx_i2c_write_w(0x002A, 0x108E);
	if (rc < 0)
		goto init_probe_fail;
	rc = s5k4cdgx_i2c_write_w(0x0F12, 0x0033);
	if (rc < 0)
		goto init_probe_fail;    
	rc = s5k4cdgx_i2c_write_w(0x0F12, 0x0066);
	if (rc < 0)
		goto init_probe_fail;     
    mdelay(2);
    
	rc = s5k4cdgx_i2c_write_w(0x002C, 0xD000);
	if (rc < 0)
		goto init_probe_fail;
	rc = s5k4cdgx_i2c_write_w(0x002E, 0x100C);
	if (rc < 0)
		goto init_probe_fail;
	rc = s5k4cdgx_i2c_read_w(0x0F12, &s5k4cdgx_model_id);
	if (rc < 0)
	{
        CDBG("s5k4cdgx_i2c_read_w 0x002e rc=%d", rc);
	}
    CDBG("s5k4cdgx model = 0x%x\n", s5k4cdgx_model_id);
    if(s5k4cdgx_model_id > MODEL_FOXCOM)
    {
        s5k4cdgx_model_id = MODEL_LITEON;
    }
    
 	rc = s5k4cdgx_i2c_write_w(0x0028, 0xD000);
	if (rc < 0)
		goto init_probe_fail;
	rc = s5k4cdgx_i2c_write_w(0x002A, 0x108E);
	if (rc < 0)
		goto init_probe_fail;
	rc = s5k4cdgx_i2c_write_w(0x0F12, 0x0000);
	if (rc < 0)
		goto init_probe_fail;    
	rc = s5k4cdgx_i2c_write_w(0x0F12, 0x0000);
	if (rc < 0)
		goto init_probe_fail;

    goto init_probe_done;

init_probe_fail:
	printk("###########%s----%d: init fail###############\n",__FUNCTION__,__LINE__);
    s5k4cdgx_sensor_init_done(data);
init_probe_done:

	return rc;
}

int s5k4cdgx_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	s5k4cdgx_ctrl = kzalloc(sizeof(struct s5k4cdgx_ctrl_t), GFP_KERNEL);
	if (!s5k4cdgx_ctrl ) {
		CDBG("s5k4cdgx_sensor_open_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	s5k4cdgx_ctrl ->fps_divider = 1 * 0x00000400;
	s5k4cdgx_ctrl ->pict_fps_divider = 1 * 0x00000400;
	s5k4cdgx_ctrl ->set_test = TEST_OFF;
	s5k4cdgx_ctrl ->prev_res = QTR_SIZE;
	s5k4cdgx_ctrl ->pict_res = FULL_SIZE;

	if (data)
		s5k4cdgx_ctrl ->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(S5K4CDGX_DEFAULT_CLOCK_RATE);
	mdelay(1);

	msm_camio_camif_pad_reg_reset();
	mdelay(1);

  rc = s5k4cdgx_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;
    
	if (s5k4cdgx_ctrl->prev_res == QTR_SIZE)
		rc = s5k4cdgx_setting(REG_INIT, RES_PREVIEW);
	else
		rc = s5k4cdgx_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0)
		goto init_fail;
	else
		goto init_done;

init_fail:
	kfree(s5k4cdgx_ctrl);
	printk("###########%s----%d:init fail###############\n",__FUNCTION__,__LINE__);
init_done:
	printk("###########%s----%d:init done###############\n",__FUNCTION__,__LINE__);
	return rc;
}

int s54cdgx_init_client(struct i2c_client *client)
{
    /* Initialize the MSM_CAMI2C Chip */
    init_waitqueue_head(&s5k4cdgx_wait_queue);
    return 0;
}

int32_t s5k4cdgx_set_sensor_mode(int mode, int res)
{
    int32_t rc = 0;

printk("###########%s----%d###############\n",__FUNCTION__,__LINE__);
    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
            CDBG("SENSOR_PREVIEW_MODE,res=%d\n",res);
            rc = s5k4cdgx_video_config(mode, res);
            break;

        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
            CDBG("SENSOR_SNAPSHOT_MODE\n");
            rc = s5k4cdgx_snapshot_config(mode);
            break;

        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}


static long s5k4cdgx_set_effect(int mode, int effect)
{
	struct s5k4cdgx_i2c_reg_conf const *reg_conf_tbl = NULL;
    int num_of_items_in_table = 0;
	long rc = 0;

printk("###########%s----%d###############\n",__FUNCTION__,__LINE__);
	switch (effect) {
	case CAMERA_EFFECT_OFF:
	        reg_conf_tbl = s5k4cd_effect_off_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_effect_off_reg_config);
       	 break;

	case CAMERA_EFFECT_MONO:
	        reg_conf_tbl = s5k4cd_effect_Mono_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_effect_Mono_reg_config);
		break;

	case CAMERA_EFFECT_NEGATIVE:
	        reg_conf_tbl = s5k4cd_effect_neg_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_effect_neg_reg_config);
		break;

#if 0
	case CAMERA_EFFECT_SOLARIZE:
        reg_conf_tbl = s5k4cdgx_effect_solarize_reg_config;
        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cdgx_effect_solarize_reg_config);
		break;
#endif		

	case CAMERA_EFFECT_SEPIA:
	        reg_conf_tbl = s5k4cd_effect_sepia_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_effect_sepia_reg_config);
		break;
        
	case CAMERA_EFFECT_AQUA:
       	 reg_conf_tbl = s5k4cd_effect_aqua_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_effect_aqua_reg_config);
		break;
        
	case CAMERA_EFFECT_WHITEBOARD:
//        reg_conf_tbl = s5k4cd_effect_aqua_reg_config;
//        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_effect_aqua_reg_config);
		break;  

	case CAMERA_EFFECT_BLACKBOARD:
//        reg_conf_tbl = s5k4cd_effect_aqua_reg_config;
//        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_effect_aqua_reg_config);
		break;     
        
	default: 
		return 0;
	}

    rc = s5k4cdgx_i2c_write_w_table(reg_conf_tbl, num_of_items_in_table);
    return rc;

    
}

static long s5k4cdgx_set_wb(int wb)
{
	struct s5k4cdgx_i2c_reg_conf const *reg_conf_tbl = NULL;
    int num_of_items_in_table = 0;
	long rc = 0;

printk("###########%s----%d in,wb=%d###############\n",__FUNCTION__,__LINE__,wb);
	switch (wb) {
	case CAMERA_WB_AUTO:
	        reg_conf_tbl = s5k4cd_wb_auto_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_wb_auto_reg_config);
        break;

	case CAMERA_WB_INCANDESCENT:
	      	 reg_conf_tbl = s5k4cd_wb_incabd_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_wb_incabd_reg_config);
		break;
        
	case CAMERA_WB_CUSTOM:
//	        reg_conf_tbl = s5k4cd_wb_cus_reg_config;
//	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_wb_cus_reg_config);
		break;	
	case CAMERA_WB_FLUORESCENT:
	        reg_conf_tbl = s5k4cd_wb_fluore_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_wb_fluore_reg_config);
		break;

	case CAMERA_WB_DAYLIGHT:
	        reg_conf_tbl = s5k4cd_wb_daylight_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_wb_daylight_reg_config);
		break;
        
	case CAMERA_WB_CLOUDY_DAYLIGHT:
	        reg_conf_tbl = s5k4cd_wb_clouday_reg_config;
	        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cd_wb_clouday_reg_config);
		break;
        
	case CAMERA_WB_TWILIGHT:
		break;  

	case CAMERA_WB_SHADE:
		break;     
        
	default: 
		return 0;
	}

    rc = s5k4cdgx_i2c_write_w_table(reg_conf_tbl, num_of_items_in_table);
    printk("###########%s----%d out ,rc=%d###############\n",__FUNCTION__,__LINE__,rc);
    return rc;

}

static long s5k4cdgx_set_antibanding(int antibanding)
{
	struct s5k4cdgx_i2c_reg_conf const *reg_conf_tbl = NULL;
    int num_of_items_in_table = 0;
	long rc = 0;

printk("###########%s----%d###############\n",__FUNCTION__,__LINE__);
	switch (antibanding) {
	case CAMERA_ANTIBANDING_OFF:
        reg_conf_tbl = s5k4cdgx_antibanding_off_reg_config;
        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cdgx_antibanding_off_reg_config);
        break;

	case CAMERA_ANTIBANDING_60HZ:
        reg_conf_tbl = s5k4cdgx_antibanding_60hz_reg_config;
        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cdgx_antibanding_60hz_reg_config);
        break;
        
	case CAMERA_ANTIBANDING_50HZ:
        reg_conf_tbl = s5k4cdgx_antibanding_50hz_reg_config;
        num_of_items_in_table = S5K4CDGX_ARRAY_SIZE(s5k4cdgx_antibanding_50hz_reg_config);
        
        break;

	default: 
		return 0;        
	}
    rc = s5k4cdgx_i2c_write_w_table(reg_conf_tbl, num_of_items_in_table);
    return rc;
    
}


int s5k4cdgx_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;
printk("###########%s----%d###############\n",__FUNCTION__,__LINE__);

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	down(&s5k4cdgx_sem);

  CDBG("s5k4cdgx_sensor_config: cfgtype = %d\n",
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
			rc = s5k4cdgx_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				s5k4cdgx_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				s5k4cdgx_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = s5k4cdgx_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = s5k4cdgx_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				s5k4cdgx_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				s5k4cdgx_set_default_focus(
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_EFFECT:
			rc = s5k4cdgx_set_effect(cdata.mode,
						cdata.cfg.effect);            
			break;
            
		case CFG_SET_WB:
			rc = s5k4cdgx_set_wb(cdata.cfg.effect);            
			break;
            
		case CFG_SET_ANTIBANDING:
			rc = s5k4cdgx_set_antibanding(cdata.cfg.effect);            
			break;


		case CFG_MAX:

            if (copy_to_user((void *)(cdata.cfg.pict_max_exp_lc),
            		s5k4cdgx_supported_effect,
            		S5K4CDGX_ARRAY_SIZE(s5k4cdgx_supported_effect))) 
            {
                CDBG("copy s5k4cdgx_supported_effect to user fail\n");
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

	up(&s5k4cdgx_sem);

	return rc;
}

int s5k4cdgx_sensor_release(void)
{
	int rc = -EBADF;

	down(&s5k4cdgx_sem);

	s5k4cdgx_power_down();

    s5k4cdgx_sensor_init_done(s5k4cdgx_ctrl->sensordata);

	kfree(s5k4cdgx_ctrl);

	up(&s5k4cdgx_sem);
	CDBG("s5k4cdgx_release completed!\n");
	return rc;
}

static int s5k4cdgx_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	s5k4cdgxsensorw =
		kzalloc(sizeof(struct s5k4cdgx_work_t), GFP_KERNEL);

	if (!s5k4cdgxsensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, s5k4cdgxsensorw);
	s54cdgx_init_client(client);
	s5k4cdgx_client  = client;
	//s5k4cdgx_client->addr = s5k4cdgx_client->addr >> 1;
	mdelay(50);

	CDBG("i2c probe ok\n");
	return 0;

probe_failure:
	kfree(s5k4cdgxsensorw);
	s5k4cdgxsensorw = NULL;
	pr_err("i2c probe failure %d\n", rc);
	return rc;
}

static const struct i2c_device_id s5k4cdgx_i2c_id[] = {
	{ "s5k4cdgx", 0},
	{ }
};

static struct i2c_driver s5k4cdgx_i2c_driver = {
	.id_table = s5k4cdgx_i2c_id,
	.probe  = s5k4cdgx_i2c_probe,
	.remove = __exit_p(s5k4cdgx_i2c_remove),
	.driver = {
		.name = "s5k4cdgx",
	},
};



static int s5k4cdgx_sensor_probe(
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	/* We expect this driver to match with the i2c device registered
	 * in the board file immediately. */
	int rc = i2c_add_driver(&s5k4cdgx_i2c_driver);
	if (rc < 0 || s5k4cdgx_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(S5K4CDGX_DEFAULT_CLOCK_RATE);
	mdelay(1);

    //penghai
    //probe success
	rc = s5k4cdgx_probe_init_sensor(info);
	if (rc < 0)
	{
		CDBG("camera sensor s5k4cdgx probe is failed!!!\n");
		i2c_del_driver(&s5k4cdgx_i2c_driver);
		goto probe_done;
	}
	//probe failed
	else
	{
		CDBG("camera sensor s5k4cdgx probe is succeed!!!\n");
	}

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CAMERA_MAIN);
    #endif

	s->s_init = s5k4cdgx_sensor_open_init;
	s->s_release = s5k4cdgx_sensor_release;
	s->s_config  = s5k4cdgx_sensor_config;
	s5k4cdgx_sensor_init_done(info);
	set_camera_support(true);

probe_done:
	return rc;
}

static int __s5k4cdgx_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, s5k4cdgx_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __s5k4cdgx_probe,
	.driver = {
		.name = "msm_camera_s5k4cdgx",
		.owner = THIS_MODULE,
	},
};

static int __init s5k4cdgx_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(s5k4cdgx_init);


