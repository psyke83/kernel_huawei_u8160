

#ifndef _LINUX_GS_FREESCALE_H
#define _LINUX_GS_FREESCALE_H



enum gs_freescale_reg {

	GS_MMA7455L_XOUTL		             = 0x00,    /* 10 bits output value X LSB*/
	GS_MMA7455L_XOUTH				=0x01,    /* 10 bits output value X MSB*/
	GS_MMA7455L_YOUTL				=0x02,   /* 10 bits output value Y LSB*/
	GS_MMA7455L_YOUTH				=0x03,   /* 10 bits output value Y MSB*/
	GS_MMA7455L_ZOUTL				=0x04,  /* 10 bits output value Z LSB*/
	GS_MMA7455L_ZOUTH				=0x05,  /* 10 bits output value Z MSB*/
	GS_MMA7455L_XOUT8		             = 0x06,  /*   8 bits output value X*/			
	GS_MMA7455L_YOUT8				=0x07,  /*   8 bits output value Y*/
	GS_MMA7455L_ZOUT8				=0x08,  /*   8 bits output value Z*/
	GS_MMA7455L_STATUS				=0x09,  /*  Status registers*/
	GS_MMA7455L_DETSRC				=0x0A,  /*  Detection source registers*/
	GS_MMA7455L_TOUT					=0x0B,  /*  "Temperature output value" (Optional)*/
	GS_MMA7455L_I2CAD					=0x0D,  /*  I2C device address I2CDIS*/
	GS_MMA7455L_USRINF				=0x0E,  /*  User information (Optional)*/
	GS_MMA7455L_WHOAMI				=0x0F,  /*  "Who am I" value (Optional)*/
	GS_MMA7455L_XOFFL					=0x10,  /*  Offset drift X value (LSB)*/
	GS_MMA7455L_XOFFH				=0x11,  /*  Offset drift X value (MSB)*/
	GS_MMA7455L_YOFFL					=0x12,  /*  Offset drift Y value (LSB)*/
	GS_MMA7455L_YOFFH				=0x13,  /*  Offset drift Y value (MSB)*/
	GS_MMA7455L_ZOFFL					=0x14,  /*  Offset drift Z value (LSB)*/
	GS_MMA7455L_ZOFFH				=0x15,  /*  Offset drift Z value (MSB)*/
	GS_MMA7455L_MCTL					=0x16,  /*  Mode control*/
	GS_MMA7455L_INTRST				=0x17,  /*  Interrupt latch reset*/
	GS_MMA7455L_CTL1					=0x18,  /*  Control 1*/
	GS_MMA7455L_CTL2					=0x19,  /*  Control 2*/
	GS_MMA7455L_LDTH					=0x1A,  /*  Level detection threshold limit value*/
	GS_MMA7455L_PDTH					=0x1B,  /*  Pulse detection threshold limit value*/
	GS_MMA7455L_PW					=0x1C,  /*  Pulse duration value*/
	GS_MMA7455L_LT					=0x1D,  /*  Latency time value*/
	GS_MMA7455L_TW					=0x1E  /*  Time window for 2nd pulse value*/
	
};


#define     GPIO_INT1                         19
#define     GPIO_INT2                         20


#define     GS_FREESCALE_TIMRER                   (1000*1000000)   /* 1s */


#define  ECS_IOCTL_READ_ACCEL_XYZ     _IOR(0xA1, 0x06, char[3])

#define ECS_IOCTL_APP_SET_DELAY	  _IOW(0xA1, 0x18, short)
#define ECS_IOCTL_APP_GET_DELAY       _IOR(0xA1, 0x30, short)

#define ECS_IOCTL_APP_SET_AFLAG		        _IOW(0xA1, 0x13, short)
#define ECS_IOCTL_APP_GET_AFLAG		        _IOR(0xA1, 0x14, short)



#endif /* _LINUX_GS_FREESCALE_H */




