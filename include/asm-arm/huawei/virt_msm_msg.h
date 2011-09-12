#ifndef _HUAWEI_VIRT_MSM_MSG_H_
#define _HUAWEI_VIRT_MSM_MSG_H_
/******************************************************************************

  Copyright (C), 2001-2011, Huawei Tech. Co., Ltd.

 ******************************************************************************
  File Name     : virt_msm_msg.h
  Version       : Initial Draft
  Author        : lihongxi
  Created       : 2009/7/3
  Last Modified :
  Description   : The virtual msm message header file
  Function List :
  History       :
  1.Date        : 2009/7/3
    Author      : lihongxi
    Modification: Created file

******************************************************************************/
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <asm-arm/huawei/andmsm_share.h>

//uint32_t virtmsm_setmsgdword(uint32_t offset, uint32_t data);
//uint32_t virtmsm_setmsgbytes(uint32_t offset, char* data, uint32_t length);
//uint32_t virtmsm_updatemsgWR(uint32_t len);


/* Message head strut define */
typedef struct _virt_msm_msg_head_
{
	uint32_t enCmd;	
} VIRT_MSM_MSG_HD;

/* Abstact message struct define */
typedef struct _virt_msm_msg_
{
    VIRT_MSM_MSG_HD msg_head;	
} VIRT_MSM_MSG;

/* Macro to define the message type */
#define DEFINE_VIRT_MSM_MSG_BEGIN(iname) \
	typedef struct _##iname##_ \
	{ \
		VIRT_MSM_MSG_HD msg_head;

#define DEFINE_VIRT_MSM_MSG_END(iname) \
	} iname;

/* Macor to make the message package */
#define VIRT_MSM_MSG_PG_LEN_POS		0
#define VIRT_MSM_MSG_PG_HEAD_LEN    4
#define VIRT_MSM_MSG_PG_CMD_POS		4
#define VIRT_MSM_MSG_PG_DATA_POS	8

/* Error code define */
#ifndef SUCCESS
#define SUCCESS                     0       /* Success */
#endif

extern uint32_t virt_msm_msg_send(VIRT_MSM_MSG* pMsg, uint32_t unMsgSize);

/* Send the message */
#define VIRT_MSM_MSG_SEND(msg)   \
    virt_msm_msg_send((VIRT_MSM_MSG*)(&msg), sizeof(msg))\
/* Get the message recevied */
#define VIRT_MSM_MSG_GET(pMsg, type)    \
    (*((type*)pMsg))
/* Get the message data pointer */
#define VIRT_MSM_GET_MSG_DATA_PTR(pmsg)  \
    (((char*)pmsg) + sizeof(VIRT_MSM_MSG_HD))

#endif

