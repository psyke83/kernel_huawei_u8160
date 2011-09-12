

#ifndef _LINUX_GS_ADI_H
#define _LINUX_GS_ADI_H


enum gs_adi_reg {

	GS_ADI_REG_DEVID		    = 0x00,
	GS_ADI_REG_THRESH_TAP		= 0x1D,
	GS_ADI_REG_OFSX				= 0x1E,
	GS_ADI_REG_OFSY				= 0x1F,
	GS_ADI_REG_OFSZ             = 0x20,
	GS_ADI_REG_DUR				= 0x21,
	GS_ADI_REG_LATENT		    = 0x22,
	GS_ADI_REG_WINDOW			= 0x23,
	GS_ADI_REG_THRESH_ACT		= 0x24,
	GS_ADI_REG_THRESH_INACT		= 0x25,
	GS_ADI_REG_TIME_INACT		= 0x26,
	GS_ADI_REG_ACT_CONTROL		= 0x27,
	GS_ADI_REG_THRESH_FF               =0x28,
	GS_ADI_REG_TIME_FF                   =0x29,
	GS_ADI_REG_TAP_AXES		       = 0x2A,
	GS_ADI_REG_ACT_TAP_STATUS	= 0x2B,
	GS_ADI_REG_BW		                    = 0x2C,
	GS_ADI_REG_POWER_CTL	       = 0x2D,
	GS_ADI_REG_INT_ENABLE		= 0x2E,
	
	GS_ADI_REG_INT_MAP			= 0x2F,
	GS_ADI_REG_INT_SOURCE		= 0x30,
	GS_ADI_REG_DATA_FORMAT		= 0x31,
	GS_ADI_REG_DATAX0	                    = 0x32,
	GS_ADI_REG_DATAX1		             = 0x33,
	GS_ADI_REG_DATAY0		             = 0x34,
	GS_ADI_REG_DATAY1                    =0x35,
	GS_ADI_REG_DATAZ0		= 0x36,
	GS_ADI_REG_DATAZ1                     =0x37,
	GS_ADI_REG_FIFO_CTL                  =0x38,
	GS_ADI_REG_FIFO_STATUS		= 0x39,
	GS_ADI_REG_ADI345_END		= GS_ADI_REG_FIFO_STATUS,
	GS_ADI_REG_TAP_SIGN		= 0x3a,
	GS_ADI_REG_ORIENT_CONF		= 0x3b,
	GS_ADI_REG_ORIENT		= 0x3c,
};

#define     GPIO_INT1                         19
#define     GPIO_INT2                         20


#define     GS_ADI_TIMRER                   (1000*1000000)    /*1s*/




#define  ECS_IOCTL_READ_ACCEL_XYZ     _IOR(0xA1, 0x06, char[3])

#define ECS_IOCTL_APP_SET_DELAY		   _IOW(0xA1, 0x18, short)
#define ECS_IOCTL_APP_GET_DELAY                _IOR(0xA1, 0x30, short)

#define ECS_IOCTL_APP_SET_AFLAG		         _IOW(0xA1, 0x13, short)
#define ECS_IOCTL_APP_GET_AFLAG		         _IOR(0xA1, 0x14, short)



#endif /* _LINUX_GS_ADI_H */







