/* < AQSC00007 lixiangyu 20090623 begin */
#ifndef ANDMSM_SHARE_H
#define ANDMSM_SHARE_H
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                    ANDROID AND MSM SHARED HEADER FILE

DESCRIPTION
  This header file is shared by both android and msm project. The header file
  in both projects should be identical. 

REFERENCES
  Copyright (c) 2009-2012 HUAWEI Incorporated. 
  All Rights Reserved.
  Huawei Confidential and Proprietary
*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*/

/*===========================================================================

                      EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to this file.
  Notice that changes are listed in reverse chronological order.

when       who     what, where, why
--------   ---     ----------------------------------------------------------
06/23/09   lxy     Created

===========================================================================*/
/*===========================================================================
   data structure for exchange data between android and modem

   android 与 modem 共享空间的结构如下所示:

                -------------
                |   HEAD    |
                |   512     |   struct SHARE_MEM_STRU
                |           |
                -------------
                |           |
                |   TX      |   struct MSG_Q_STRU
                |  4096     |
                |           |
                -------------
                |           |
                |   RX      |   struct MSG_Q_STRU
                |  4096     |
                |           |
                -------------
                |           |
                |   PAD     |
                |  4096-512 |
                |           |
                -------------
                |           |
                |   LCD1    |    struct DISP_BUF_STRU
                | 320*240*2 |
                |           |
                -------------
                |           |
                |   LCD2    |
                | 320*240*2 |
                |           |
                -------------

===========================================================================*/
/*===========================================================================

                        INCLUDE FILES FOR MODULE

===========================================================================*/
/*===========================================================================

                        DEFINITIONS

===========================================================================*/

typedef unsigned long uint32;
typedef unsigned char byte;
typedef unsigned char boolean;

/* 命令包头的长度,即命令包的前三个字段，
   即结构体andmsm_cmd_stru的前三个字段,total_length,type,port_num */
#define ANDMSM_PACKET_HEAD_LEN                  12

#define VIRTMSM_SHAREMEM_MAGIC0                 0x51525354
#define VIRTMSM_SHAREMEM_MAGIC1                 0x61626364

/* 整个TX/RX的大小，包括8字节(ulRD, ulWR)的header */
#define ANDMSM_SIO_MSG_BUFFER_LEN               4096

/* TX/RX的header的大小 */
#define ANDMSM_SIO_MSG_HEAD_LEN                 8

/* TX/RX实际buffer的大小 */
#define ANDMSM_SIO_MSM_CONTENT_LEN              (ANDMSM_SIO_MSG_BUFFER_LEN - ANDMSM_SIO_MSG_HEAD_LEN)

#define LCD_WIDTH                               240
#define LCD_HEIGHT                              320

/* cmd 字段32bit, 低16bit表示实际命令, 最高bit(bit31)表示收到命令后是否需要应答，
   1: 需要应答; 0: 不需要应答 */
#define ANDMSM_CMD_MASK                         0x0000FFFF
#define ANDMSM_CMD_RESPONSE_MASK                0x80000000

/* android 与 modem 传递数据的整个共享空间的头结构 */
struct SHARE_MEM_STRU
{
    uint32  ulMagicNum[2];
    uint32  ulFlag;
    uint32  ulBulkAddr;
    uint32  ulBulkSize;
    uint32  ulReserved[7];
    uint32  ulBlockNSize[16];
};

/* android 与 modem 传递数据(TX/RX)的buffer的结构 */
struct MSG_Q_STRU
{
    uint32  ulRD;
    uint32  ulWR;
    uint32  ulContent[ANDMSM_SIO_MSM_CONTENT_LEN / 4];
};

/* android 与 modem 传递lcd显示数据的结构 */
struct DISP_BUF_STRU
{
    byte    aBuf_1[LCD_HEIGHT * LCD_WIDTH * 2];
    byte    aBuf_2[LCD_HEIGHT * LCD_WIDTH * 2];
};

/* android 与 modem 传递的一条命令/数据的结构
   TX/RX 的buffer中的数据结构
*/
typedef struct
{
    uint32 total_length;    /* 整个命令包的长度，以byte为单位 */
    uint32 type;            /* 低16bit由andmsm_cmd_type_enum定义，最高bit表示是否需要应答 */
    uint32 port_num;        /* andmsm_port_num_enum */
    uint32 cmd_length;      /* 发送过来的命令长度，以byte为单位 */
    uint32 data;            /* 发送过来的命令和数据, 这里只定义首个数据的地址 */
}andmsm_cmd_stru;

struct MSG2_Q_STRU
{
  uint32  ulRD;
  uint32  ulWR;
  uint32  ulTotalLen;
  uint32  ulContentOffset;
};

/* < BQ5D00041 lixiangyu 20090729 begin, 修改PCLINT的Info 826: 
  Suspicious pointer-to-pointer conversion (area too small)*/
#define MSG2_INIT_QUEUE(p, addr, len) \
  do\
  {\
    (p) = (void *)(addr);\
    (p)->ulRD = 0;\
    (p)->ulWR = 0;\
    (p)->ulTotalLen = (len) - sizeof(struct MSG2_Q_STRU);\
  }while(0)
/* BQ5D00041 lixiangyu 20090729 end > */


/* define the exchanged data type between android and modem,
   corresponding to the 2nd field(type) in data structure.
*/
typedef enum
{
  VIRTMSM_MSG_SIO_WRITE = 0,
  VIRTMSM_MSG_SIO_READ,  
  VIRTMSM_MSG_SIO_CMD,
  VIRTMSM_MSG_MAX
}andmsm_cmd_type_enum;

/*  define the port_num, 
    corresponding to the 3rd field (port_num) in data structure 
*/
typedef enum
{
    ANDMSM_SIO_PORT_SRV_CHN_0 = 0,
    ANDMSM_SIO_PORT_SRV_CHN_1,
    ANDMSM_SIO_PORT_SRV_CHN_2,
    ANDMSM_SIO_PORT_SRV_CHN_3,
    ANDMSM_SIO_PORT_SRV_CHN_4,
    ANDMSM_SIO_PORT_SRV_CHN_5,
    ANDMSM_SIO_PORT_SRV_CHN_6,
    ANDMSM_SIO_PORT_SRV_CHN_7,
    ANDMSM_SIO_PORT_SRV_CHN_8,
    ANDMSM_SIO_PORT_SRV_CHN_9,
    ANDMSM_SIO_PORT_SRV_CHN_A,
    ANDMSM_SIO_PORT_SRV_CHN_B,
    ANDMSM_SIO_PORT_SRV_CHN_C,
    ANDMSM_SIO_PORT_SRV_CHN_D,
    ANDMSM_SIO_PORT_SRV_CHN_E,
    ANDMSM_SIO_PORT_SRV_CHN_F,
    ANDMSM_SIO_PORT_SRV_CHN_10,
    ANDMSM_SIO_PORT_SRV_CHN_11,
    ANDMSM_SIO_PORT_SRV_CHN_12,
    ANDMSM_SIO_PORT_SRV_CHN_13,
    ANDMSM_SIO_PORT_SRV_CHN_14,
    ANDMSM_SIO_PORT_SRV_CHN_15,
    ANDMSM_SIO_PORT_SRV_CHN_16,
    ANDMSM_SIO_PORT_SRV_CHN_17,
    ANDMSM_SIO_PORT_SRV_CHN_18,
    ANDMSM_SIO_PORT_SRV_CHN_19,
    ANDMSM_SIO_PORT_SRV_CHN_1A,
    ANDMSM_SIO_PORT_SRV_CHN_1B,
    ANDMSM_SIO_PORT_SRV_CHN_1C,
    ANDMSM_SIO_PORT_SRV_CHN_1D,
    ANDMSM_SIO_PORT_SRV_CHN_1E,
    ANDMSM_SIO_PORT_SRV_CHN_1F,
    ANDMSM_SIO_PORT_MAX
}andmsm_port_num_enum;


/*===========================================================================

                        MACRO DEFINITIONS

===========================================================================*/
/*  kernel进入睡眠状态   */
#define KERNEL_SUSPEND_SLEEP_STATUS     0xFFFFEEEE
/*  kernel进入arch_idle状态   */
#define KERNEL_ARCH_IDLE_STATUS         0xFF123456

/*===========================================================================

                        DATA DECLARATIONS

===========================================================================*/
/*===========================================================================

                        FUNCTION DECLARATIONS

===========================================================================*/

#endif  /* #ifndef ANDMSM_SHARE_H */
/* AQSC00007 lixiangyu 20090623 end > */

