/* drivers\video\msm\mddi_tc358723xbg.c
 *  MDDI2RGB driver for TC358723XBG
 *
 * Copyright (C) 2009 HUAWEI Technology Co., ltd.
 * Date: 2009/03/21
 * By Liangzonglian
 * 
 */
 
/* this file was borrowed from mddi_toshiba.c */

#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"

#include "mddi_tc358723xbg.h"
#include <mach/gpio.h>
#include <mach/vreg.h>

#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
static u8 bio_get_hw_sub_ver(void);
static u32 u32_hw_sub_version = -1;
#endif


/* all panels supported tc358723xbg */
extern s_panel_seq_t panel_lms350df04;
/* add others connected to tc358723xbg here */

/* current panel connected */
static ps_panel_seq_t panel_sel; 


#define TC358723XBG_VGA_PRIM 1
#define TC358723XBG_VGA_SECD 2

static struct msm_panel_common_pdata *mddi_tc358723xbg_pdata;
/* state machine op */
typedef enum {
	TC358723_STATE_OFF,
	TC358723_STATE_PRIM_SEC_STANDBY,
	TC358723_STATE_PRIM_SEC_READY,
	TC358723_STATE_PRIM_NORMAL_MODE,
	TC358723_STATE_SEC_NORMAL_MODE
} mddi_tc358723xbg_state_t;

static mddi_tc358723xbg_state_t tc358723xbg_state = TC358723_STATE_PRIM_NORMAL_MODE;//TC358723_STATE_OFF;
static void mddi_tc358723xbg_prim_init(void);
/* pre define */
static void mddi_tc358723xbg_poweron(void);
static void mddi_tc358723xbg_poweroff(void);
static void mddi_tc358723xbg_state_normal2stop(void);
static void mddi_tc358723xbg_state_stop2normal(void);
static void mddi_tc358723xbg_reset(void);

/*ms per frame */
#define panel_mspf		17  /* defined ms per frame */

/* define sequence struct */
typedef struct  {
	uint16 reg;
	uint16 val;
} s_pairs_seq_t;

/* define poweron sequence */
static s_pairs_seq_t 
s_poweron_seq[] = {
#if 1
	{0, 		1			},	//wait 1ms
	{0x07,	0x0000		},	//reset
	{0, 		10			},	//wait 10ms
	{0x11,	0x222f		},	//
	{0x12,	0x0f00		},	//
	{0x13,	0x7fe3		},	//
	{0x76,	0x2213		},	//
	{0x74,	0x0001		},	//
	{0x76,	0x0000		},	//
	{0x10,	0x5604		},	//
	{0, 		6*panel_mspf	},	//wait for 6frames
	{0x12,	0x0c63		},
	{0, 		2*panel_mspf	},	//wait for 2frames
	{0x01,	0x083b		},	// inversion setting
	{0x02,	0x0300		},	//
	{0x03,	0xf000		},	// polarity setting
	{0x08,	0x0002		},	// vsync back porch
	{0x09,	0x000C		},	// hsync back porch
	{0x76,	0x2213		},	//
	{0x0b,	0x3340		},	//
	{0x0c,	0x0020		},	//RIM 24bit
	{0x1c,	0x7770		},	//
	{0x76,	0x0000		},	//
	{0x0d,	0x0000		},	//
	{0x0e,	0x0500		},	//
	{0x14,	0x0000		},	//
	{0x15,	0x0803		},	//
	{0x16,	0x0000		},	//
	{0x30,	0x0005		},	//
	{0x31,	0x070f		},	//
	{0x32,	0x0300		},	//
	{0x33,	0x0003		},	//
	{0x34,	0x090c		},	//
	{0x35,	0x0001		},	//
	{0x36,	0x0001		},	//
	{0x37,	0x0303		},	//
	{0x38,	0x0f09		},	//
	{0x39,	0x0105		}	//

#else
	{0, 		1			},	//wait 1ms
	{0x07, 	0x0000		},	//reset
	{0, 		10			},	//wait 10ms
	{0x11, 	0x222f		}, 	//
	{0x12, 	0x0c00		}, 	//
	{0x13, 	0x7fd9		}, 	//
	{0x76, 	0x2213		}, 	//
	{0x74, 	0x0001		}, 	//
	{0x76, 	0x0000		}, 	//
	{0x10, 	0x5604		}, 	//
	{0, 		6*panel_mspf	},	//wait for 6frames
	{0x12, 	0x0c63		},
	{0, 		2*panel_mspf	},	//wait for 2frames
	{0x01, 	0x083b		}, 	// inversion setting
	{0x02,	0x0300		},	//
	{0x03, 	0xf040		}, 	// polarity setting
	{0x08,	0x0002		},	// vsync back porch
	{0x09,	0x000B		},	// hsync back porch
	{0x76, 	0x2213		}, 	//
	{0x0b, 	0x3340		}, 	//
	{0x0c, 	0x002C		}, 	//RIM 18bit
	{0x1c, 	0x7770		}, 	//
	{0x76, 	0x0000		}, 	//
	{0x0d, 	0x0000		}, 	//
	{0x0e, 	0x0500		}, 	//
	{0x14, 	0x0000		}, 	//
	{0x15, 	0x0803		}, 	//
	{0x16, 	0x0000		}, 	//
#if 1
//GAMA 1
	{0x30, 	0x0003		}, 	//
	{0x31, 	0x070f		}, 	//
	{0x32, 	0x0d05		}, 	//
	{0x33, 	0x0405		}, 	//
	{0x34, 	0x090d		}, 	//
	{0x35, 	0x0501		}, 	//
	{0x36, 	0x0400		}, 	//
	{0x37, 	0x0504		}, 	//
	{0x38, 	0x0c09		}, 	//
	{0x39, 	0x010c		}	//
#else
//gama 2
	{0x30,	0x0300		},	//
	{0x31, 	0x0007		}, 	//
	{0x32, 	0x0202		}, 	//
	{0x33, 	0x0000		}, 	//
	{0x34, 	0x0705		}, 	//
	{0x35, 	0x0205		}, 	//
	{0x36, 	0x0707		}, 	//
	{0x37, 	0x0000		}, 	//
	{0x38, 	0x1803		}, 	//
	{0x39, 	0x1502		}	
	
#endif
#endif
};

/* define poweroff sequence */
static s_pairs_seq_t 
s_poweroff_seq[] = {
	//{0x10, 	0x0001		}, 	//
	{0x0b, 	0x3000		}, 	//
	{0x07, 	0x0102		}, 	//
	{0, 		2*panel_mspf	}, 	//wait for 2frames
	{0x07, 	0x0000		}, 	//
	{0, 		2*panel_mspf	}, 	//wait for 2frames
	{0x12, 	0x0000		},	//
	{0x10, 	0x0600		},	//
//	{0x10, 	0x0601		}	//			
};

/* define displayon sequence */
static s_pairs_seq_t 
s_displayon_seq[] = {
	{0x07, 	0x0001		}, 	//
	{0, 		2*panel_mspf	},	//wait for 1frames
	{0x07, 	0x0101		}, 	//
	{0, 		2*panel_mspf	},	//wait for 2frames
	{0x07, 	0x0103		}	//			
};

/* define displayoff sequence */
static s_pairs_seq_t 
s_displayoff_seq[] = {
	// display on status
	{0x0b, 	0x3000		}, 	//
	{0x07,	0x0102		},	//
	{0, 		2*panel_mspf	},	//wait for 1frames
	{0x07, 	0x0000		}, 	//
	{0, 		2*panel_mspf	},	//wait for 2frames
	{0x12,	0x0000		},	//
	{0x10, 	0x0600		}	//	
	// display off status
};

static s_pairs_seq_t 
s_standby_in_seq[] = {
	// display on status
	// display off seq;
	//{0x10, 	0x0001		}	//
	{0x10, 	0x0601		}	//
};

/* define standby out sequence */
static s_pairs_seq_t 
s_standby_out_seq[] = {
	// standby status
	{0x10, 	0x0000		}	//
	// poweron seq;
	// displayon seq;
	// displayon status
};

/* state machine define */
typedef enum {
	E_STATE_POWERON,
	E_STATE_POWEROFF,
	E_STATE_DISPLAYON,
	E_STATE_DISPLAYOFF,
	E_STATE_STANDBY
} e_panel_state_t;
static e_panel_state_t s_panel_state = E_STATE_DISPLAYON;
static int err_counter = 0;
/* GPIO and Software simulate SPI signals */
#if 1
static void serigo_reg(uint8 reg)
{
	uint32 spid[2], ssiints, ssictl, i;
	spid[0] = 0x00080074;
	spid[1] = 0x00010000 | reg; 
	mddi_queue_register_read(SSIINTS, &ssiints, TRUE, 0);
	if((ssiints & 0x00001000) || ((ssiints & 0x00700000)>>20) <2)
		mddi_wait(1);
	// SSICTL.SETACT=0
	//mddi_queue_register_read(SSICTL, &ssictl, TRUE, 0);
	//ssictl &= 0xFFFFFFFD;
	mddi_queue_register_write(SSICTL,0x00000171,TRUE,0);
	
	// SSITX(2)
	for(i=0; i < sizeof(spid)/sizeof(uint32); i++)
		mddi_queue_register_write(SSITX,spid[i],TRUE,0);
	
	// SSICTL.SETACT=1
	//mddi_queue_register_read(SSICTL, &ssictl, TRUE, 0);
	//ssictl |= 0x00000002;
	mddi_queue_register_write(SSICTL,0x00000173,TRUE,0);	
}

static void serigo_val(uint16 value)
{
	uint32 spid[2], ssiints, ssictl, i;
	spid[0] = 0x00080076;
	spid[1] = 0x00010000 | value; 
	mddi_queue_register_read(SSIINTS, &ssiints, TRUE, 0);
	if((ssiints & 0x00001000) || ((ssiints & 0x00700000)>>20) < 2)
		mddi_wait(1);
	// SSICTL.SETACT=0
	//mddi_queue_register_read(SSICTL, &ssictl, TRUE, 0);
	//ssictl &= 0xFFFFFFFD;
	mddi_queue_register_write(SSICTL,0x00000171,TRUE,0);
	
	// SSITX(2)
	for(i=0; i < sizeof(spid)/sizeof(uint32); i++)
		mddi_queue_register_write(SSITX,spid[i],TRUE,0);
	
	// SSICTL.SETACT=1
	//mddi_queue_register_read(SSICTL, &ssictl, TRUE, 0);
	//ssictl |= 0x00000002;
	mddi_queue_register_write(SSICTL,0x00000173,TRUE,0);	
}


inline void s_reg_val_op(uint8 reg, uint16 val)
{
	if(0 == reg)
		mddi_wait(val);
	else
	{
		serigo_reg(reg);
		udelay(10);
		serigo_val(val);
		udelay(10);
	}
}
#else
inline void s_reg_val_op(uint8 reg, uint16 val)
{
	uint32 i;
	
	if(0 == reg)
		mddi_wait(val);
	else
	{
		uint32 spid[4], ssiints, ssictl;
		
		spid[0] = 0x00080074;
		spid[1] = 0x00010000 | reg;
		spid[2] = 0x00080076;
		spid[3] = 0x00010000 | val;
        // if check SSIINTS invalid, we can still go continue.
//check_again:
		// SSIINTS.TXNUM[2:0]==000 && SSIINTS.SIFACT==0
		udelay(10);
		
#ifdef CONFIG_FB_MSM_MDDI_TC358723XBG_VGA_QCIF
		mddi_queue_register_read_timeout(SSIINTS, &ssiints, TRUE, 0, 1*HZ);
		  	if((ssiints & 0x00701000))
		  	{	  	
				switch(tc358723xbg_state) 
				{
                		case TC358723_STATE_PRIM_SEC_STANDBY:
                		case TC358723_STATE_OFF:
                		case TC358723_STATE_PRIM_SEC_READY:	
                			printk(KERN_INFO "err_counter=%d, ssiints =0x%x \n", err_counter, ssiints);
                			err_counter++;
                			if(err_counter < 3)
                				{
                    					mddi_tc358723xbg_reset();          
                    					printk(KERN_ERR"mddi_tc358723xbg_reset \n");
                    				}
                			else
                				{
                					arm_pm_restart(0);
                				}
                    			return;
                		default:
                    			printk("tc358723xbg_state is 0x%x ssiints is 0x%x \n", tc358723xbg_state, ssiints);
                    			break;
			    }
		  	}
#endif  //CONFIG_FB_MSM_MDDI_TC358723XBG_VGA_QCIF

		// SSICTL.SETACT=0
		//mddi_queue_register_read(SSICTL, &ssictl, TRUE, 0);
		//ssictl &= 0xFFFFFFFD;
		mddi_queue_register_write(SSICTL,0x00000171,TRUE,0);

		// SSITX(4)
		for(i=0; i < sizeof(spid)/sizeof(uint32); i++)
			mddi_queue_register_write(SSITX,spid[i],TRUE,0);
		
		// SSICTL.SETACT=1
		//mddi_queue_register_read(SSICTL, &ssictl, TRUE, 0);
		//ssictl |= 0x00000002;
		mddi_queue_register_write(SSICTL,0x00000173,TRUE,0);
		// wait 10us to decrease ratio of check failure
		udelay(10);
	}
}
#endif

/* power on op */
inline void s_lms350df04_power_on(void)
{
	int i = 0;
	int count = 0;
	printk(KERN_INFO"Panel state (%d)\n", s_panel_state);
	
	// power off status and standby status
	//MDDI_MSG_ERR("Panel state (%d)\n", s_panel_state);
	switch(s_panel_state){
	case E_STATE_STANDBY:
		count = sizeof(s_standby_out_seq)/sizeof(s_pairs_seq_t);
		for(i=0; i<count; i++)
			s_reg_val_op(
			s_standby_out_seq[i].reg, 
			s_standby_out_seq[i].val);
	case E_STATE_POWEROFF:
		count = sizeof(s_poweron_seq)/sizeof(s_pairs_seq_t);
		for(i=0; i<count; i++)
			s_reg_val_op(
			s_poweron_seq[i].reg, 
			s_poweron_seq[i].val);
		s_panel_state = E_STATE_POWERON;
		break;
	default:
		MDDI_MSG_ERR("Panel state should be poweroff/standby (%d)\n", s_panel_state);
	}		
	printk(KERN_INFO"state_end (%d)\n", s_panel_state);
}

/* power off  op*/ 
inline void s_lms350df04_power_off(void)
{
	int i = 0;
	int count = sizeof(s_poweroff_seq)/
		sizeof(s_pairs_seq_t);
	
	// display off status
	//MDDI_MSG_ERR("Panel state (%d)\n", s_panel_state);
	if(s_panel_state != E_STATE_DISPLAYOFF){
		MDDI_MSG_ERR("Panel state should be displayoff  (%d)\n", s_panel_state);
		return;
	}
	
	for(i=0; i<count; i++)
		s_reg_val_op(
		s_poweroff_seq[i].reg, 
		s_poweroff_seq[i].val);

	s_panel_state = E_STATE_POWEROFF;	
}

/* display on  op*/ 
inline void s_lms350df04_display_on(void)
{
	int i = 0;
	int count = sizeof(s_displayon_seq)/
		sizeof(s_pairs_seq_t);
	
	// state check
	//MDDI_MSG_ERR("Panel state (%d)\n", s_panel_state);
	if((s_panel_state != E_STATE_POWERON) &&
		(s_panel_state != E_STATE_DISPLAYOFF)){
		MDDI_MSG_ERR("Panel state should be poweron/displayoff (%d)\n", s_panel_state);
		return;
	}
		
	for(i=0; i<count; i++)
		s_reg_val_op(
		s_displayon_seq[i].reg, 
		s_displayon_seq[i].val);
	
	s_panel_state = E_STATE_DISPLAYON;
}

/* display off  op*/ 
inline void s_lms350df04_display_off(void)
{
	int i = 0;
	int count = sizeof(s_displayoff_seq)/
		sizeof(s_pairs_seq_t);
	
	// state check
	//MDDI_MSG_ERR("Panel state (%d)\n", s_panel_state);
	if(s_panel_state != E_STATE_DISPLAYON){
		MDDI_MSG_ERR("Panel state should be displayon (%d)\n", s_panel_state);
		return;
	}
		
	for(i=0; i<count; i++)
		s_reg_val_op(
		s_displayoff_seq[i].reg, 
		s_displayoff_seq[i].val);
	
	s_panel_state = E_STATE_DISPLAYOFF;
}

/* standby in  op*/ 
inline void s_lms350df04_standby_in(void)
{
	int i = 0;
	int count = sizeof(s_standby_in_seq)/
		sizeof(s_pairs_seq_t);

	// state check
	//MDDI_MSG_ERR("Panel state (%d)\n", s_panel_state);
	if(s_panel_state != E_STATE_DISPLAYOFF){
		MDDI_MSG_ERR("Panel state should be display off (%d)\n", s_panel_state);
		return;
	}
	
	//
	for(i=0; i<count; i++)
		s_reg_val_op(
		s_standby_in_seq[i].reg, 
		s_standby_in_seq[i].val);
	
	s_panel_state = E_STATE_STANDBY;
}

/* standby out  op*/ 
inline void s_lms350df04_standby_out(void)
{
	// state check
	//MDDI_MSG_ERR("Panel state (%d)\n", s_panel_state);
	if(s_panel_state != E_STATE_STANDBY){
		MDDI_MSG_ERR("Panel state should be stanby in (%d)\n", s_panel_state);
		return;
	}
		
	// poweron
	s_lms350df04_power_on();
	// displayon
	s_lms350df04_display_on();
	
	s_panel_state = E_STATE_DISPLAYON;	
}

/* about refresh */
#define MDDI_TC358723_60HZ_REFRESH

static uint32 mddi_tc358723xbg_curr_vpos;
static boolean mddi_tc358723xbg_monitor_refresh_value = FALSE;
static boolean mddi_tc358723xbg_report_refresh_measurements = FALSE;

/* Modifications to timing to increase refresh rate to > 60Hz.
 *   9.8Hz; //20MHz dot clock.
 *   489; //646 total rows.
 *   336; //506 total columns.
 *   refresh rate = 60; //61.19Hz
 */
#ifdef MDDI_TC358723_60HZ_REFRESH
static uint32 mddi_tc358723xbg_rows_per_second = 29167; // 9800000/336 ; 39526;
static uint32 mddi_tc358723xbg_rows_per_refresh = 489; //646;
static uint32 mddi_tc358723xbg_usecs_per_refresh = 16766; //489/29167 ;16344;
#else
/*
 *   12.5MHz dot clock
 *    486 total rows
 *    360 total cols
 *     refresh rate = 71.44Hz
 */
static uint32 mddi_tc358723xbg_rows_per_second = 34722; // = 12500000/360
static uint32 mddi_tc358723xbg_rows_per_refresh = 486;
static uint32 mddi_tc358723xbg_usecs_per_refresh = 13997; // = 486/34722
#endif

extern boolean mddi_vsync_detect_enabled;

/* vsync handle */
static msm_fb_vsync_handler_type mddi_tc358723xbg_vsync_handler = NULL;
static void *mddi_tc358723xbg_vsync_handler_arg;
static uint16 mddi_tc358723xbg_vsync_attempts;
#if 0
/* state machine op */
typedef enum {
	TC358723_STATE_OFF,
	TC358723_STATE_PRIM_SEC_STANDBY,
	TC358723_STATE_PRIM_SEC_READY,
	TC358723_STATE_PRIM_NORMAL_MODE,
	TC358723_STATE_SEC_NORMAL_MODE
} mddi_tc358723xbg_state_t;

static mddi_tc358723xbg_state_t tc358723xbg_state = TC358723_STATE_PRIM_NORMAL_MODE;//TC358723_STATE_OFF;
#endif

static void mddi_tc358723xbg_state_transition(mddi_tc358723xbg_state_t a,
					  mddi_tc358723xbg_state_t b)
{
	if (tc358723xbg_state != a) {
		MDDI_MSG_ERR("toshiba state trans. (%d->%d) found %d\n", a, b,
			     tc358723xbg_state);
	}
	tc358723xbg_state = b;
}

/* lcd panel register access func */
#define write_client_reg(__X,__Y,__Z) {\
  mddi_queue_register_write(__X,__Y,TRUE,0);\
}
static uint32 read_client_reg(uint32 addr)
{
	uint32 val;
	mddi_queue_register_read(addr, &val, TRUE, 0);
	return val;
}

/* register setting */ 
typedef struct {
	uint32 reg;
	uint32 val;
} s_reg_val_pair_t;

static void s_reg_val_pair_op(uint32 reg, uint32 val)
{
	if(0 == reg)
		mddi_wait(val);
	else
		write_client_reg(reg, val, TRUE);
}
//#define M_SWITCH_REFRESH_RATE  
/* sequence op followed */
static s_reg_val_pair_t 
s_seq_init_setup[]={
#ifdef M_SWITCH_REFRESH_RATE
	{DPSET0,			0x4EA8009E},	// # MDC.DPSET0  # Setup DPLL parameters 12.2MHz Dclk
	{DPSET1,			0x00000110},	//	 # MDC.DPSET1 
#else
	{DPSET0,			0x0BAE0062},	// # MDC.DPSET0  # Setup DPLL parameters 9.8MHz Dclk
	{DPSET1,			0x00000114},	//	 # MDC.DPSET1 
#endif
	{DPSUS, 			0x00000000},	//	 # MDC.DPSUS  # Set DPLL oscillation enable
	{DPRUN, 			0x00000001},	//	 # MDC.DPRUN  # Release reset signal for DPLL
	{0,				100		},	// wait_ms(500)
	{SYSCKENA, 		0x00000001},	//   # MDC.SYSCKENA  # Enable system clock output
	{CLKENB,			0x000AF0E9},	//	 # SYS.CLKENB  # Enable clocks for each module (without DCLK , i2cCLK, eDram)
	{GPIODATA,		0x00030000},	//	 # GPI .GPIODATA  # GPIO2(RESET_LCD_N) set to 0 , GPIO3(eDRAM_Power) set to 0
	{GPIODIR,		0x00000005},	//	 # GPI .GPIODIR  # Select direction of GPIO port (0,2 output)
	{GPIOSEL,		0x00000001},	//	 # SYS.GPIOSEL	# GPIO port multiplexing control
	{GPIODATA,		0x00040004},	//	 # GPI .GPIODATA  # Release LCDD reset
	{INTMSK,			0x00000001},	//	 # LCDC.INTMASK.INTM = 0 ; VWakeOUT enable
	{WAKEUP,		0x00000000},	//
	{INTMASK,		0x00000001},	 //   # SYS.INTMASK
	{DRAMPWR, 		0x00000001},	//   # SYS.DRAMPWR  # eDRAM power up
	{CLKENB,			0x000AF0EB},	//	 # SYS.CLKENB  # Enable clocks for	eDram
	{0, 				1		},	//  wait_ms(1);
	{SSICTL,			0x00000171},	//	 # SPI .SSICTL	# SPI operation mode setting
	{SSITIME,		0x00000100},	//	 # SPI .SSITIME  # SPI serial interface timing setting
	{CNT_DIS, 		0x00000000},	//	 # SYS.CNT_DIS  # Enable control pins to LCD/SPI module
	{SSICTL,			0x00000173},	//	 # SPI .SSICTL	# Set SPI active mode
	{SSITX, 			0x00000000},
	{0, 				1		},	//  wait_ms(1);
	{SSITX, 			0x00000000},
	{0, 				1		},	//  wait_ms(1);
	{SSITX, 			0x00000000},
	{0, 				1		},	//  wait_ms(1);
};

static void tc358723xbg_common_initial_setup(void)
{
	int i = 0;
	int count = sizeof(s_seq_init_setup) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_init_setup[i].reg, 
			s_seq_init_setup[i].val);

	// primary driver setting
	s_lms350df04_power_on();

	mddi_tc358723xbg_state_transition(TC358723_STATE_PRIM_SEC_STANDBY,
				      TC358723_STATE_PRIM_SEC_READY);
}

static s_reg_val_pair_t
s_seq_sec_cont_update_start[] = {
};

static void tc358723xbg_sec_cont_update_start(void)
{
	int i = 0;
	int count = sizeof(s_seq_sec_cont_update_start) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_sec_cont_update_start[i].reg, 
			s_seq_sec_cont_update_start[i].val);
}

static s_reg_val_pair_t
s_seq_sec_cont_update_stop[]={
};

static void tc358723xbg_sec_cont_update_stop(void)
{
	int i = 0;
	int count = sizeof(s_seq_sec_cont_update_stop) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_sec_cont_update_stop[i].reg, 
			s_seq_sec_cont_update_stop[i].val);
}

static s_reg_val_pair_t
s_seq_sec_sleep_in[]={
};

static void tc358723xbg_sec_sleep_in(void)
{
	int i = 0;
	int count = sizeof(s_seq_sec_sleep_in) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_sec_sleep_in[i].reg, 
			s_seq_sec_sleep_in[i].val);
}

static s_reg_val_pair_t
s_seq_sec_sleep_out[]={
};

static void tc358723xbg_sec_sleep_out(void)
{
	int i = 0;
	int count = sizeof(s_seq_sec_sleep_out) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_sec_sleep_out[i].reg, 
			s_seq_sec_sleep_out[i].val);
}

static s_reg_val_pair_t
s_seq_prim_lcd_off[]={
	{PORT,		0x00000000},
	{REGENB,		0x00000001},
	{0, 			16		},
	{PXL,		0x00000000},
	{START, 		0x00000000},
	{REGENB,		0x00000001},
	{0, 			32		},
};

static void tc358723xbg_prim_lcd_off(void)
{
	int i = 0;
	int count = sizeof(s_seq_prim_lcd_off) / 
		sizeof(s_reg_val_pair_t);
	
	// Enable control pins to LCD/SPI module
	s_reg_val_pair_op(CNT_DIS, 0x00000000);
	
	// primary sleep
	s_lms350df04_display_off();
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_prim_lcd_off[i].reg, 
			s_seq_prim_lcd_off[i].val);
			
	// primary poweroff
	s_lms350df04_standby_in();
	
	mddi_tc358723xbg_state_transition(TC358723_STATE_PRIM_NORMAL_MODE,
				      TC358723_STATE_PRIM_SEC_STANDBY);	
}

static s_reg_val_pair_t
s_seq_sec_lcd_off[]={
};

static void tc358723xbg_sec_lcd_off(void)
{
	int i = 0;
	int count = sizeof(s_seq_sec_lcd_off) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_sec_lcd_off[i].reg, 
			s_seq_sec_lcd_off[i].val);

	mddi_tc358723xbg_state_transition(TC358723_STATE_SEC_NORMAL_MODE,
				      TC358723_STATE_PRIM_SEC_STANDBY);
}

static s_reg_val_pair_t
s_seq_prim_start[]={
	{BITMAP0,		0x01E00140},	// MDC.BITMAP1	); // Setup of PITCH size to Frame buffer1(VGA)
	{CLKENB,			0x000AF1EB},	// SYS.CLKENB  ); // DCLK supply 9.8MHz
	{PORT_ENB, 		0x00000001},	// LCD.PORT_ENB  ); // Synchronous port enable
	{PORT, 			0x00000004},	// LCD.PORT  ); // Polarity of DE is set to high active
	{CMN, 			0x00000000},	// LCD.CMN  ); // for rotate 180'
#ifdef CONFIG_HUAWEI_MDDI_LCD_24BIT // for BGR setting
	{PXL, 			0x0000023A},	// LCD.PXL  ); // 24bit and ACTMODE 2 set (1st frame black data output)
#else
	{PXL, 			0x0000003A},	// LCD.PXL  ); // 24bit and ACTMODE 2 set (1st frame black data output)
#endif
	{MPLFBUF, 		0x00000000},	// LCD.MPLFBUF  ); // Select the reading buffer

#ifdef MDDI_TC358723_60HZ_REFRESH
	{HCYCLE,			0x000000A7},	// LCD.HCYCLE  ); // 336
#else
	{HCYCLE,			0x000000b3},	// LCD.HCYCLE  ); // 360=30+320+10
#endif

	{HSW,			0x00000001},	// LCD.HSW //10
	{HDE_START, 		0x00000005},	// LCD.HDE_START //30
	{HDE_SIZE,		0x0000009F},	// LCD.HDE_SIZE
	{VCYCLE,			0x000001E7},	// LCD.VCYCLE
	{VSW,			0x00000001},	// LCD.VSW 
	{VDE_START, 		0x00000003},	// LCD.VDE_START//6
	{VDE_SIZE,		0x000001DF},	// LCD.VDE_SIZE
	{CNT_DIS, 		0x00000000},	//	 # SYS.CNT_DIS  # Enable control pins to LCD/SPI module
	{REGENB, 		0x00000001},
	{START, 			0x00000001},	// LCD.START  ); // LCDC - Pixel data transfer start
	{INTMSK,		        0x00000000},	//	 # LCDC.INTMASK.INTM = 0 ; VWakeOUT enable	
	{0,				32		},	//  wait_ms( 32  );
};

static void tc358723xbg_prim_start(void)
{
	int i = 0;
	int count = sizeof(s_seq_prim_start) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_prim_start[i].reg, 
			s_seq_prim_start[i].val);

	// primary display on
	s_lms350df04_display_on();
	
	mddi_tc358723xbg_state_transition(TC358723_STATE_PRIM_SEC_READY,
				      TC358723_STATE_PRIM_NORMAL_MODE);
}

static s_reg_val_pair_t
s_seq_sec_start[]={
};

static void tc358723xbg_sec_start(void)
{
	int i = 0;
	int count = sizeof(s_seq_sec_start) / 
		sizeof(s_reg_val_pair_t);
	
	for(i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_sec_start[i].reg, 
			s_seq_sec_start[i].val);

	mddi_tc358723xbg_state_transition(TC358723_STATE_PRIM_SEC_READY,
				      TC358723_STATE_SEC_NORMAL_MODE);
}

static s_reg_val_pair_t
s_seq_sec_backlight_on[]={
};

static void tc358723xbg_sec_backlight_on(void)
{
	int i = 0;
	int count = sizeof(s_seq_sec_backlight_on) / 
		sizeof(s_reg_val_pair_t);
	
	for( i=0; i<count; i++)
		s_reg_val_pair_op(s_seq_sec_backlight_on[i].reg, 
			s_seq_sec_backlight_on[i].val);
}

static void mddi_tc358723xbg_prim_init(void)
{
	printk("prim_init:%d \n",tc358723xbg_state);
	switch (tc358723xbg_state) {
	case TC358723_STATE_PRIM_NORMAL_MODE:
		return;
	case TC358723_STATE_PRIM_SEC_READY:
		break;
	case TC358723_STATE_OFF:
		tc358723xbg_state = TC358723_STATE_PRIM_SEC_STANDBY;
		tc358723xbg_common_initial_setup();
		break;
	case TC358723_STATE_PRIM_SEC_STANDBY:
		mddi_tc358723xbg_poweron();
		mddi_tc358723xbg_state_stop2normal();
		//s_lms350df04_power_on();
		tc358723xbg_common_initial_setup();
		break;
	case TC358723_STATE_SEC_NORMAL_MODE:
		tc358723xbg_sec_cont_update_stop();
		tc358723xbg_sec_sleep_in();
		tc358723xbg_sec_sleep_out();
		tc358723xbg_sec_lcd_off();
		tc358723xbg_common_initial_setup();
		break;
	default:
		MDDI_MSG_ERR("mddi_tc358723xbg_prim_init from state %d\n",
			     tc358723xbg_state);
	}

	tc358723xbg_prim_start();
       printk("prim_init_end:%d \n",tc358723xbg_state);
	mddi_host_write_pix_attr_reg(0x00C3);
}

static void mddi_tc358723xbg_sec_init(void)
{

	switch (tc358723xbg_state) {
	case TC358723_STATE_PRIM_SEC_READY:
		break;
	case TC358723_STATE_PRIM_SEC_STANDBY:
		tc358723xbg_common_initial_setup();
		break;
	case TC358723_STATE_PRIM_NORMAL_MODE:
		tc358723xbg_prim_lcd_off();
		tc358723xbg_common_initial_setup();
		break;
	default:
		MDDI_MSG_ERR("mddi_tc358723xbg_sec_init from state %d\n",
			     tc358723xbg_state);
	}

	tc358723xbg_sec_start();
	tc358723xbg_sec_backlight_on();
	tc358723xbg_sec_cont_update_start();
	mddi_host_write_pix_attr_reg(0x0400);
}

static void mddi_tc358723xbg_lcd_powerdown(void)
{
	switch (tc358723xbg_state) {
	case TC358723_STATE_PRIM_SEC_READY:
		mddi_tc358723xbg_prim_init();
		mddi_tc358723xbg_lcd_powerdown();
		return;
	case TC358723_STATE_PRIM_SEC_STANDBY:
		break;
	case TC358723_STATE_PRIM_NORMAL_MODE:
		tc358723xbg_prim_lcd_off();
		mddi_tc358723xbg_state_normal2stop();
		break;
	case TC358723_STATE_SEC_NORMAL_MODE:
		tc358723xbg_sec_cont_update_stop();
		tc358723xbg_sec_sleep_in();
		tc358723xbg_sec_sleep_out();
		tc358723xbg_sec_lcd_off();
		break;
	default:
		MDDI_MSG_ERR("mddi_tc358723xbg_lcd_powerdown from state %d\n",
			     tc358723xbg_state);
	}
}

static void mddi_tc358723xbg_lcd_vsync_detected(boolean detected)
{
	// static timetick_type start_time = 0;
	static struct timeval start_time;
	static boolean first_time = TRUE;
	// uint32 mdp_cnt_val = 0;
	// timetick_type elapsed_us;
	struct timeval now;
	uint32 elapsed_us;
	uint32 num_vsyncs;

	if ((detected) || (mddi_tc358723xbg_vsync_attempts > 5)) {
		if ((detected) && (mddi_tc358723xbg_monitor_refresh_value)) {
			// if (start_time != 0)
			if (!first_time) {
				// elapsed_us = timetick_get_elapsed(start_time, T_USEC);
				jiffies_to_timeval(jiffies, &now);
				elapsed_us =
				    (now.tv_sec - start_time.tv_sec) * 1000000 +
				    now.tv_usec - start_time.tv_usec;
				/* LCD is configured for a refresh every * usecs, so to
				 * determine the number of vsyncs that have occurred
				 * since the last measurement add half that to the
				 * time difference and divide by the refresh rate. */
				num_vsyncs = (elapsed_us +
					      (mddi_tc358723xbg_usecs_per_refresh >>
					       1)) /
				    mddi_tc358723xbg_usecs_per_refresh;
				/* LCD is configured for * hsyncs (rows) per refresh cycle.
				 * Calculate new rows_per_second value based upon these
				 * new measurements. MDP can update with this new value. */
				mddi_tc358723xbg_rows_per_second =
				    (mddi_tc358723xbg_rows_per_refresh * 1000 *
				     num_vsyncs) / (elapsed_us / 1000);
			}
			// start_time = timetick_get();
			first_time = FALSE;
			jiffies_to_timeval(jiffies, &start_time);
			if (mddi_tc358723xbg_report_refresh_measurements) {
				(void)mddi_queue_register_read_int(VPOS,
								   &mddi_tc358723xbg_curr_vpos);
				// mdp_cnt_val = MDP_LINE_COUNT;
			}
		}
		/* if detected = TRUE, client initiated wakeup was detected */
		if (mddi_tc358723xbg_vsync_handler != NULL) {
			(*mddi_tc358723xbg_vsync_handler)
			    (mddi_tc358723xbg_vsync_handler_arg);
			mddi_tc358723xbg_vsync_handler = NULL;
		}
		mddi_vsync_detect_enabled = FALSE;
		mddi_tc358723xbg_vsync_attempts = 0;
		/* need to disable the interrupt wakeup */
		if (!mddi_queue_register_write_int(INTMSK, 0x0001)) {
			MDDI_MSG_ERR("Vsync interrupt disable failed!\n");
		}
		if (!detected) {
			/* give up after 5 failed attempts but show error */
			MDDI_MSG_NOTICE("Vsync detection failed!\n");
		} else if ((mddi_tc358723xbg_monitor_refresh_value) &&
			   (mddi_tc358723xbg_report_refresh_measurements)) {
			MDDI_MSG_NOTICE("  Last Line Counter=%d!\n",
					mddi_tc358723xbg_curr_vpos);
			// MDDI_MSG_NOTICE("  MDP Line Counter=%d!\n",mdp_cnt_val);
			MDDI_MSG_NOTICE("  Lines Per Second=%d!\n",
					mddi_tc358723xbg_rows_per_second);
		}
		/* clear the interrupt */
		if (!mddi_queue_register_write_int(INTFLG, 0x0001)) {
			MDDI_MSG_ERR("Vsync interrupt clear failed!\n");
		}
	} else {
		/* if detected = FALSE, we woke up from hibernation, but did not
		 * detect client initiated wakeup.
		 */
		mddi_tc358723xbg_vsync_attempts++;
	}
}


static int panel_detect ;
static void mddi_tc358723xbg_panel_detect(void)
{
	mddi_host_type host_idx = MDDI_HOST_PRIM;
	if (!panel_detect) {
		/* Toshiba display requires larger drive_lo value */
		mddi_host_reg_out(DRIVE_LO, 0x0050);
		//panel_sel = &panel_lms350df04;

		panel_detect = 1;
	}

}
// lcd on
static int32 level_pre ;
static int mddi_tc358723xbg_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	mfd = (struct msm_fb_data_type *)platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mddi_tc358723xbg_panel_detect();
	//mddi_tc358723xbg_poweron();
	if (mfd->panel.id == TC358723XBG_VGA_PRIM) {
		mddi_tc358723xbg_prim_init();
	} else {
		mddi_tc358723xbg_sec_init();
	}

	return 0;
}

// lcd off
static int mddi_tc358723xbg_lcd_off(struct platform_device *pdev)
{
	mddi_tc358723xbg_panel_detect();
	mddi_tc358723xbg_lcd_powerdown();	
	mddi_tc358723xbg_poweroff();
	return 0;
}
#ifdef CONFIG_U8220_BACKLIGHT
static void mddi_tc358723xbg_lcd_set_backlight(struct msm_fb_data_type *mfd)
{
}
#else
// set backlight
//static int32 level_pre = 15;
static int32 first_use = 1;
static void mddi_tc358723xbg_lcd_set_backlight(struct msm_fb_data_type *mfd)
{
	int32 level, counter=0;

	if (mddi_tc358723xbg_pdata && mddi_tc358723xbg_pdata->backlight_level) {
		level = mddi_tc358723xbg_pdata->backlight_level(mfd->bl_level);
		//printk("LCD set backlight level=%d\n",level);

		// condition test
		if(level<0) level =0;
		if(level>15) level = 15;
		
		if(first_use){
			first_use = 0;
			level_pre = level;
			if(level == 0){
				gpio_direction_output(LCD_GPIO_BL_CL,0);
				mdelay(3);
			}
			else if(level == 15){
				gpio_direction_output(LCD_GPIO_BL_CL,1);
				udelay(30);
			}
			else{
				counter = 15 - level;
				while(counter-- > 0){
					udelay(1);
					gpio_direction_output(LCD_GPIO_BL_CL,0);
					udelay(1);
					gpio_direction_output(LCD_GPIO_BL_CL,1);
				};				
			}
			return;
		}

		if (level_pre == level) return;
		
		counter = (16 - level + level_pre)%16;
		while(counter > 0){
			counter--;
			udelay(1);
			gpio_direction_output(LCD_GPIO_BL_CL,0);
			udelay(1);
			gpio_direction_output(LCD_GPIO_BL_CL,1);
		};				
		level_pre = level;
		
		if(level == 0){
			gpio_direction_output(LCD_GPIO_BL_CL,0);
			mdelay(3);
		}
	}
}
#endif

// set handler
static void mddi_tc358723xbg_vsync_set_handler(msm_fb_vsync_handler_type handler,	/* ISR to be executed */
					   void *arg)
{
	boolean error = FALSE;
	unsigned long flags;

	/* Disable interrupts */
	spin_lock_irqsave(&mddi_host_spin_lock, flags);
	// INTLOCK();

	if (mddi_tc358723xbg_vsync_handler != NULL) {
		error = TRUE;
	} else {
		/* Register the handler for this particular GROUP interrupt source */
		mddi_tc358723xbg_vsync_handler = handler;
		mddi_tc358723xbg_vsync_handler_arg = arg;
	}

	/* Restore interrupts */
	spin_unlock_irqrestore(&mddi_host_spin_lock, flags);
	// MDDI_INTFREE();
	if (error) {
		MDDI_MSG_ERR("MDDI: Previous Vsync handler never called\n");
	} else {
		/* Enable the vsync wakeup */
		mddi_queue_register_write(INTMSK, 0x0000, FALSE, 0);

		mddi_tc358723xbg_vsync_attempts = 1;
		mddi_vsync_detect_enabled = TRUE;
	}
}				/* mddi_toshiba_vsync_set_handler */

static int __init mddi_tc358723xbg_lcd_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		mddi_tc358723xbg_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}


static struct platform_driver this_driver = {
	.probe  = mddi_tc358723xbg_lcd_probe,
	.driver = {
		.name   = "mddi_tc23xbg_vga",
	},
};

static struct msm_fb_panel_data tc358723xbg_panel_data0 = {
	.on 		= mddi_tc358723xbg_lcd_on,
	.off 		= mddi_tc358723xbg_lcd_off,
	.set_backlight 	= mddi_tc358723xbg_lcd_set_backlight,
//	.set_vsync_notifier = mddi_tc358723xbg_vsync_set_handler,

};

static struct platform_device this_device_0 = {
	.name   = "mddi_tc23xbg_vga",
	.id	= TC358723XBG_VGA_PRIM,
	.dev	= {
		.platform_data = &tc358723xbg_panel_data0,
	}
};
static void mddi_tc358723xbg_poweron(void)
{
	// Power ON
	struct vreg   *v_pa, *v_gp4, *v_gp6;
	
	// VDD1E v1.5 for eDram
	v_pa = vreg_get(NULL,"pa");
	if(IS_ERR(v_pa)) return;
	if(vreg_set_level(v_pa,2300)) return;
	if(vreg_enable(v_pa)) return;
	
	v_gp4 = vreg_get(NULL,"gp4");
	if(IS_ERR(v_gp4)) return;		
	if(vreg_set_level(v_gp4,1500)) return;
	if(vreg_enable(v_gp4)) return;
	
	// VDD2E v2.6 for eDram (VREG_2V6)
	gpio_tlmm_config(GPIO_CFG
	(41, 0, GPIO_OUTPUT,
	GPIO_NO_PULL, GPIO_2MA),
	GPIO_ENABLE);
	gpio_direction_output(LCD_GPIO_MSMP_CL,0);
	
#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
      
	if(u32_hw_sub_version == 1)
       {
             //LCD3P3 output 1 to be HIGH
             //gpio_direction_output(19,1);
       }
#endif
}

static void mddi_tc358723xbg_poweroff(void)
{
	// Power OFF
	struct vreg  *v_gp4, *v_gp6;
#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
	if(u32_hw_sub_version == 1)
	{
		// LCD3P3
		//GPIO19 ouput 0 to be low
		//gpio_direction_output(19,0);
	}
#endif
	// VDD2E v2.6 for eDram (VREG_2V6)
	gpio_direction_output(LCD_GPIO_MSMP_CL,1);

	// VDD1E v1.5 for eDram
	v_gp4 = vreg_get(NULL,"gp4");
	if(IS_ERR(v_gp4)) return;	
	if(vreg_disable(v_gp4)) return;
}

static void mddi_tc358723xbg_state_normal2stop(void)
{
	mddi_host_type host_idx = MDDI_HOST_PRIM;
	// tc358721xbg changes state to stop
	//printk(KERN_INFO"tc358721xbg changes state to stop\n");
        //eDram power off
        //{CLKENB,			0x000000E9},	//	 # SYS.CLKENB  # Enable clocks for each module (without DCLK , i2cCLK, eDram)
        mddi_queue_register_write(0x160004,0x00000000,TRUE,0); 
        //{DRAMPWR, 		0x00000000},	//   # SYS.DRAMPWR  # eDRAM power off
        mddi_queue_register_write(0x160008,0x00000000,TRUE,0); 

	mddi_wait(100);
	//Disable system clock output -SYSCLKENA 	0x00000000
	mddi_queue_register_write(0x10802C,0x00000000,TRUE,0); 
	//Suspend DPLL oscillation -DPSUS 	0x00000001
	mddi_queue_register_write(0x108024,0x00000001,TRUE,0);

	//Send the Link Shutdown Packet
	mddi_host_reg_out(CMD, MDDI_CMD_POWERDOWN);

}

static void mddi_tc358723xbg_state_stop2normal(void)
{
	mddi_host_type host_idx = MDDI_HOST_PRIM;
	printk( KERN_INFO "stop2normal start \n");

	//Send the Link Active Packet
	mddi_host_reg_out(CMD, MDDI_CMD_LINK_ACTIVE);
#if 1
	//Set DPLL oscillation enable -DPSUS	0x00000000
	mddi_queue_register_write(0x108024,0x00000000,TRUE,0);
	udelay(100);
	//Enable system clock output -SYSCLKENA 0x00000001
	mddi_queue_register_write(0x10802C,0x00000001,TRUE,0); 
	//eDram power on
	//{DRAMPWR, 		0x00000000},	//	 # SYS.DRAMPWR
	mddi_queue_register_write(0x160008,0x00000001,TRUE,0); 
	//{CLKENB,			0x000000E9},	//	 # SYS.CLKENB
	mddi_queue_register_write(0x160004,0x000AF1EB,TRUE,0); 
	//printk( KERN_INFO "MDDI/LCD power on: resume end \n");
#endif
	printk( KERN_INFO "stop2normal resume end \n");

}

#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
static void mddi_tc358723xbg_suspend( struct early_suspend *h)
{
	//mddi_tc358723xbg_state_normal2stop();
	mddi_tc358723xbg_poweroff();
}

static void mddi_tc358723xbg_resume( struct early_suspend *h)
{
	mddi_tc358723xbg_poweron();
	//mddi_tc358723xbg_state_stop2normal();
}

static struct early_suspend mddi_tc358723xbg_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 1,
	.suspend = mddi_tc358723xbg_suspend,
	.resume = mddi_tc358723xbg_resume,
};
#endif
#endif


static void mddi_tc358723xbg_reset(void)
{
	// reset
	s_panel_state = E_STATE_STANDBY;
	tc358723xbg_state = TC358723_STATE_PRIM_SEC_STANDBY;
	
	// GPIO91 for Reset (LCD_VEGA_RESET_N)
	gpio_tlmm_config(GPIO_CFG
		(LCD_GPIO_MDDI_RESET, 0, GPIO_OUTPUT,
		GPIO_PULL_UP, GPIO_2MA),
		GPIO_ENABLE);
	mdelay(1);
	gpio_direction_output(LCD_GPIO_MDDI_RESET,1);
	mdelay(1);
	gpio_direction_output(LCD_GPIO_MDDI_RESET,0);
	mdelay(1);
	gpio_direction_output(LCD_GPIO_MDDI_RESET,1);
	mdelay(1);

	// init 
	mddi_tc358723xbg_poweron();
	tc358723xbg_common_initial_setup();
	tc358723xbg_prim_start();
}

static int __init mddi_tc358723xbg_lcd_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
	gpio_tlmm_config(GPIO_CFG
	(19, 0, GPIO_OUTPUT,
	GPIO_PULL_DOWN, GPIO_2MA),
	GPIO_ENABLE);
#endif

	gpio_tlmm_config(GPIO_CFG
	(41, 0, GPIO_OUTPUT,
	GPIO_NO_PULL, GPIO_2MA),
	GPIO_ENABLE);

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT
{
	u32 id;

	id = mddi_get_client_id();
	if ((id >> 16) != 0xD263)
		return 0;
}
#endif

	ret = platform_driver_register(&this_driver);
	if (!ret) {
		pinfo = &tc358723xbg_panel_data0.panel_info;
		pinfo->xres = 320;
		pinfo->yres = 480;
		pinfo->type = MDDI_PANEL;
		pinfo->pdest = DISPLAY_1;
		pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
		pinfo->wait_cycle = 0;
		#ifdef CONFIG_HUAWEI_MDDI_LCD_24BIT
		pinfo->bpp = 32;
		#else
		pinfo->bpp = 18;
		#endif
		pinfo->lcd.vsync_enable = TRUE;
		/*pinfo->lcd.refx100 =
		    (mddi_tc358723xbg_rows_per_second * 100) /
		    mddi_tc358723xbg_rows_per_refresh;*/
		#ifdef M_SWITCH_REFRESH_RATE
		pinfo->lcd.refx100 =7498; 
		pinfo->lcd.v_back_porch = 2;
		pinfo->lcd.v_front_porch = 4;
		pinfo->lcd.v_pulse_width = 2;
		pinfo->lcd.hw_vsync_mode = TRUE; //FALSE;
		pinfo->lcd.vsync_notifier_period = 0;// (1 * HZ);
		pinfo->bl_max = 15;
		pinfo->bl_min = 0;
	        pinfo->clk_rate = 120000000;
	        pinfo->clk_min =  120000000;
	        pinfo->clk_max =  120000000;
		#else
		pinfo->lcd.refx100 =5975; 
		pinfo->lcd.v_back_porch = 2;
		pinfo->lcd.v_front_porch = 4;
		pinfo->lcd.v_pulse_width = 2;
		pinfo->lcd.hw_vsync_mode = TRUE; //FALSE;
		pinfo->lcd.vsync_notifier_period = 0;// (1 * HZ);
		pinfo->bl_max = 15;
		pinfo->bl_min = 0;
	        pinfo->clk_rate = 81920000;
	        pinfo->clk_min =  81920000;
	        pinfo->clk_max =  81920000;
		#endif
		pinfo->fb_num = 2;

		ret = platform_device_register(&this_device_0);
		if (ret)
			platform_driver_unregister(&this_driver);

	}

	if (!ret)
		mddi_lcd.vsync_detected = mddi_tc358723xbg_lcd_vsync_detected;

#if 0
#ifdef CONFIG_HAS_EARLYSUSPEND
	register_early_suspend(&mddi_tc358723xbg_early_suspend);
#endif
#endif

#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
  	u32_hw_sub_version = bio_get_hw_sub_ver();
#endif

	return ret;
}

#ifdef CONFIG_HUAWEI_FEATURE_U8220_PP1
static u8 bio_get_hw_sub_ver(void)
{
     u32 u_gpio_20_val;

     u32 u_gpio_101_val;

     u32 u_hw_sub_version;

     // GPIO 20 
     gpio_tlmm_config(GPIO_CFG(20,0,GPIO_INPUT,GPIO_PULL_UP,GPIO_2MA), GPIO_ENABLE);

     gpio_tlmm_config(GPIO_CFG(101,0,GPIO_INPUT,GPIO_PULL_UP,GPIO_2MA), GPIO_ENABLE);

     u_gpio_20_val = gpio_get_value(20);

     u_gpio_101_val = gpio_get_value(101);

     if((u_gpio_20_val == 0)&&(u_gpio_101_val == 0))
     {
         u_hw_sub_version = 0;
     }
     else if((u_gpio_20_val != 0)&&(u_gpio_101_val == 0))
     {
         u_hw_sub_version = 1;
     }
	 else if((u_gpio_20_val == 0)&&(u_gpio_101_val != 0))
	 {
        u_hw_sub_version = 2;
     }
	 else
	 {
	     u_hw_sub_version = 3;
	 }

     gpio_tlmm_config(GPIO_CFG(20,0,GPIO_INPUT,GPIO_PULL_DOWN,GPIO_2MA), GPIO_ENABLE);

     gpio_tlmm_config(GPIO_CFG(101,0,GPIO_INPUT,GPIO_PULL_DOWN,GPIO_2MA), GPIO_ENABLE);

     return u_hw_sub_version;
}

#endif
module_init(mddi_tc358723xbg_lcd_init);

