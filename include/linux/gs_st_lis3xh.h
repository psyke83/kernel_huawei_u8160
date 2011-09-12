
#ifndef _LINUX_GS_ST_H
#define _LINUX_GS_ST_H

enum gs_st_reg {
	GS_ST_REG_STATUS_AUX		= 0x07,
	GS_ST_REG_OUT_1L		= 0x08,
	GS_ST_REG_OUT_1H		= 0x09,
	GS_ST_REG_OUT_2L		= 0x0a,
	GS_ST_REG_OUT_2H		= 0x0b,
	GS_ST_REG_OUT_3L		= 0x0c,
	GS_ST_REG_OUT_3H		= 0x0d,
	GS_ST_REG_INT_COUNTER		= 0x0e,
	GS_ST_REG_WHO_AM_I		= 0x0f,
	GS_ST_REG_TEMP_CFG_REG		= 0x1f,
	GS_ST_REG_CTRL1			= 0x20,
	GS_ST_REG_CTRL2			= 0x21,
	GS_ST_REG_CTRL3			= 0x22,
	GS_ST_REG_CTRL4			= 0x23,
	GS_ST_REG_CTRL5			= 0x24,
	GS_ST_REG_CTRL6			= 0x25,
	GS_ST_REG_REF			= 0x26,
	GS_ST_REG_STATUS		= 0x27,
	GS_ST_REG_OUT_XL		= 0x28,
	GS_ST_REG_OUT_XH		= 0x29,
	GS_ST_REG_OUT_YL		= 0x2a,
	GS_ST_REG_OUT_YH		= 0x2b,
	GS_ST_REG_OUT_ZL		= 0x2c,
	GS_ST_REG_OUT_ZH		= 0x2d,
	GS_ST_REG_FF_WU_CFG_1		= 0x30,
	GS_ST_REG_FF_WU_SRC_1		= 0x31,
	GS_ST_REG_FF_WU_THS_1		= 0x32,
	GS_ST_REG_FF_WU_DURATION_1	= 0x33,
	GS_ST_REG_FF_WU_CFG_2		= 0x34,
	GS_ST_REG_FF_WU_SRC_2		= 0x35,
	GS_ST_REG_FF_WU_THS_2		= 0x36,
	GS_ST_REG_FF_WU_DURATION_2	= 0x37,
	GS_ST_REG_CLICK_CFG		= 0x38,
	GS_ST_REG_CLICK_SRC		= 0x39,
	GS_ST_REG_CLICK_THSY_X		= 0x3b,
	GS_ST_REG_CLICK_THSZ		= 0x3c,
	GS_ST_REG_CLICK_TIME_LIMIT	= 0x3d,
	GS_ST_REG_CLICK_LATENCY		= 0x3e,
	GS_ST_REG_CLICK_WINDOW		= 0x3f,


};

enum 	gs_st_reg_ctrl1 {
	GS_ST_CTRL1_Xen			= 0x01,
	GS_ST_CTRL1_Yen			= 0x02,
	GS_ST_CTRL1_Zen			= 0x04,
	GS_ST_CTRL1_FS			= 0x20,
	GS_ST_CTRL1_PD			= 0x40,
	GS_ST_CTRL1_DR			= 0x80,
};

enum 	gs_st_reg_ctrl2 {
	GS_ST_CTRL2_HPCOEFF1		= 0x01,
	GS_ST_CTRL2_HPCOEFF2		= 0x02,
	GS_ST_CTRL2_HPFFWU1		= 0x04,
	GS_ST_CTRL2_HPFFWU2		= 0x08,
	GS_ST_CTRL2_FDS			= 0x10,
	GS_ST_CTRL2_BOOT		= 0x40,
	GS_ST_CTRL2_SIM			= 0x80,
};
enum    gs_st_reg_ctrl3 {
	GS_ST_CTRL3_PP_OD		= 0x40,
	GS_ST_CTRL3_IHL			= 0x80,
};

/* interrupt handling related */

enum gs_intmode {
	GS_INTMODE_GND			= 0x00,
	GS_INTMODE_FF_WU_1		= 0x01,
	GS_INTMODE_FF_WU_2		= 0x02,
	GS_INTMODE_FF_WU_12		= 0x03,
	GS_INTMODE_DATA_READY		= 0x04,
	GS_INTMODE_CLICK		= 0x07,
};

enum	gs_st_status {
	GS_ST_STATUS_XDA		= 0x01,
	GS_ST_STATUS_YDA		= 0x02,
	GS_ST_STATUS_ZDA		= 0x04,
	GS_ST_STATUS_XYZDA		= 0x08,
	GS_ST_STATUS_XOR		= 0x10,
	GS_ST_STATUS_YOR		= 0x20,
	GS_ST_STATUS_ZOR		= 0x40,
	GS_ST_STATUS_XYZOR		= 0x80,
};

/* Wakeup/freefall interrupt defs */
enum gs_st_reg_ffwucfg {
	GS_ST_FFWUCFG_XLIE		= 0x01,
	GS_ST_FFWUCFG_XHIE		= 0x02,
	GS_ST_FFWUCFG_YLIE		= 0x04,
	GS_ST_FFWUCFG_YHIE		= 0x08,
	GS_ST_FFWUCFG_ZLIE		= 0x10,
	GS_ST_FFWUCFG_ZHIE		= 0x20,
	GS_ST_FFWUCFG_LIR		= 0x40,
	GS_ST_FFWUCFG_AOI		= 0x80,
};

enum  gs_st_reg_ffwuths {
	GS_ST_FFWUTHS_DCRM		= 0x80,
};

enum  gs_st_reg_click_cfg {
	GS_ST_CLICK_CFG_SINGLE_X	= 0x01,
	GS_ST_CLICK_CFG_DOUBLE_X	= 0x02,
	GS_ST_CLICK_CFG_SINGLE_Y	= 0x04,
	GS_ST_CLICK_CFG_DOUBLE_Y	= 0x08,
	GS_ST_CLICK_CFG_SINGLE_Z	= 0x10,
	GS_ST_CLICK_CFG_DOUBLE_Z	= 0x20,
	GS_ST_CLICK_CFG_LIR		= 0x40,
};

enum gs_st_reg_ffwusrc {
	GS_ST_FFWUSRC_XL		= 0x01,
	GS_ST_FFWUSRC_XH		= 0x02,
	GS_ST_FFWUSRC_YL		= 0x04,
	GS_ST_FFWUSRC_YH		= 0x08,
	GS_ST_FFWUSRC_ZL		= 0x10,
	GS_ST_FFWUSRC_ZH		= 0x20,
	GS_ST_FFWUSRC_IA		= 0x40,
};

enum  gs_st_reg_click_src {
	GS_ST_CLICKSRC_SINGLE_X		= 0x01,
	GS_ST_CLICKSRC_DOUBLE_X		= 0x02,
	GS_ST_CLICKSRC_SINGLE_Y		= 0x04,
	GS_ST_CLICKSRC_DOUBLE_Y		= 0x08,
	GS_ST_CLICKSRC_SINGLE_Z		= 0x10,
	GS_ST_CLICKSRC_DOUBLE_Z		= 0x20,
	GS_ST_CLICKSRC_IA		= 0x40,
};



#define GS_ST_F_WUP_FF_1		0x0001	/* wake up from free fall */
#define GS_ST_F_WUP_FF_2		0x0002
#define GS_ST_F_WUP_FF			0x0003
#define GS_ST_F_WUP_CLICK		0x0004
#define GS_ST_F_POWER			0x0010
#define GS_ST_F_FS			0x0020 	/* ADC full scale */
#define GS_ST_F_INPUT_OPEN 		0x0040  /* Set if input device is opened */
#define GS_ST_F_IRQ_WAKE 		0x0080  /* IRQ is setup in wake mode */

#define     GPIO_INT1                         19
#define     GPIO_INT2                         20


#define     GS_ST_TIMRER                   (1000*1000000)           /*1000000s*/



#define  ECS_IOCTL_READ_ACCEL_XYZ     _IOR(0xA1, 0x06, char[3])


#define ECS_IOCTL_APP_SET_DELAY		        _IOW(0xA1, 0x18, short)
#define ECS_IOCTL_APP_GET_DELAY                   _IOR(0xA1, 0x30, short)

#define ECS_IOCTL_APP_SET_AFLAG		        _IOW(0xA1, 0x13, short)
#define ECS_IOCTL_APP_GET_AFLAG		        _IOR(0xA1, 0x14, short)


#define ECS_IOCTL_READ_DEVICEID				_IOR(0xA1, 0x31, char[20])

#endif /* _LINUX_GS_ST_H */






