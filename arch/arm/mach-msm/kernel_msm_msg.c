/* linux/arch/arm/mach-msm/board-msm7x25.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Brian Swetland <swetland@google.com>
 *
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
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

#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <asm-arm/huawei/virt_msm_msg.h>
#include <asm-arm/huawei/andmsm_share.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

extern char* get_msm_to_app_msg_buf(void);
extern char* get_app_to_msm_msg_buf(void);
extern int huawei_serial_notify_modem_done(int data);

//extern void virtmsm_batt_handle_info_cb(void* pmsg);

static boolean virtmsm_getmsg(byte **ppMsg, boolean *pbIsMalloc);
static uint32 virtmsm_getmsgdword(uint32 offset);

static uint32_t virtmsm_setmsgdword(uint32_t offset, uint32_t data);
static uint32_t virtmsm_setmsgbytes(uint32_t offset, char* data, uint32_t length);

static void virtmsm_updatemsgRD(uint32 len);

static void virtmsm_sio_read_cb(int port_num, void *pData,uint32 length);

void virtmsm_handlemsg(void);
uint32_t virtmsm_sio_write(int port_num, void* pData, uint32_t length);

struct MSG_Q_STRU     *pVirtMsmMsgTx   = NULL;
struct MSG_Q_STRU     *pVirtMsmMsgRx   = NULL;

wait_queue_head_t virtmsm_msg_rcv_evt;

#define HAVE_RCV_MSG    (pVirtMsmMsgRx->ulRD != pVirtMsmMsgRx->ulWR)

static int  virtmsm_msg_rcv(void * unused)
{
  struct sched_param param = { .sched_priority = 1 };
  sched_setscheduler(current, SCHED_FIFO, &param);
  
  do
  {
    freezer_do_not_count();
    msleep(100);
    virtmsm_handlemsg();
    freezer_count();    
  }while( !kthread_should_stop() );

  return 0;
}

static struct task_struct *virtmsm_msg_kthread = NULL;
void virtmsm_msg_run(void)
{
  virtmsm_msg_kthread = kthread_run(virtmsm_msg_rcv, NULL, "msg_recv");
}
void virtmsm_msg_stop(void)
{
  kthread_stop(virtmsm_msg_kthread);
}

void virtmsm_msg_init(void)
{
  printk("virtmsm_msg_init is called!\n");
  pVirtMsmMsgRx = (struct MSG_Q_STRU *)get_msm_to_app_msg_buf();
  pVirtMsmMsgTx = (struct MSG_Q_STRU *)get_app_to_msm_msg_buf();
  
  init_waitqueue_head(&virtmsm_msg_rcv_evt);

//  kthread_run(virtmsm_msg_rcv, NULL, "msg_recv");
  printk("virtmsm_msg_init call end!\n");
}

static int32_t virtmsm_FreeMsgspace(void)
{
  /* < BQ5D00391 yanzhijun stay in huawei logo 20091020 begin */
  if(pVirtMsmMsgTx->ulRD == pVirtMsmMsgTx->ulWR)
  {
    return (4096-8);
  }
  /* BQ5D00391 yanzhijun stay in huawei logo 20091020 end> */
  if (pVirtMsmMsgTx->ulRD < pVirtMsmMsgTx->ulWR)
  {
    return 4096 - 8 - (pVirtMsmMsgTx->ulWR - pVirtMsmMsgTx->ulRD) - 8;
  }
  else
  {
    return (pVirtMsmMsgTx->ulRD - pVirtMsmMsgTx->ulWR) - 12;
  }
}

static uint32_t virtmsm_updatemsgWR(uint32_t len)
{
  if (virtmsm_FreeMsgspace() < len)
  {
    return -1;
  }

  if ((pVirtMsmMsgTx->ulWR + len) >= (4096 - 8))
  {
    pVirtMsmMsgTx->ulWR = pVirtMsmMsgTx->ulWR + len - (4096 - 8);
  }
  else
  {
    pVirtMsmMsgTx->ulWR = pVirtMsmMsgTx->ulWR + len;
  }

  return 0;
}

extern int shmem_serial_recv_data(int port_num, char *data, unsigned int size);

static void virtmsm_sio_read_cb(int port_num, void *pData,uint32 length)
{
  uint32 unLoop = 0;
  char* pBuf = (char*)pData;
  
  printk(KERN_ERR"virtmsm_sio_read_cb is called, port:%d, length=%d\n",
    port_num, (int)length);
  
  while (unLoop < length)
  {
    printk(KERN_ERR"data[%d]=%d\n", (int)unLoop, 
        (int)(*(pBuf + unLoop)));

    unLoop ++;
  }

  shmem_serial_recv_data(port_num, pData, length);
}

void virtmsm_sio_write_cb(uint32 status)
{
}

void virtmsm_sio_read(void* pData, uint32_t *length)
{
}

uint32_t virtmsm_sio_write(int port_num, void* pData, uint32_t length)
{
    uint32 msg_len;

    unsigned long flags;

    if (length > 4000)
    {
    length = 4000;
    }
    
    raw_local_irq_save(flags);
    msg_len = ((length + 3) / 4) * 4 + 16;

    virtmsm_setmsgdword(0, msg_len);
    virtmsm_setmsgdword(4, VIRTMSM_MSG_SIO_CMD);
    virtmsm_setmsgdword(8, port_num);
    virtmsm_setmsgdword(12,length);
    virtmsm_setmsgbytes(16,(char *)pData,length);
    virtmsm_updatemsgWR(msg_len);
    raw_local_irq_restore(flags);
  
    return length;
}

void virtmsm_handlemsg(void)
{
  uint32 *pMsg = NULL;
  boolean rsp;
  uint8_t *pSioBuf;
  
  boolean bIsMalloc = 0;
  
  static atomic_t read_inprogress;
  atomic_set(&read_inprogress, 1);

  while (HAVE_RCV_MSG)
  {
    rsp = virtmsm_getmsg((byte **)&pMsg, &bIsMalloc);

    if (0 == rsp)
    {
        return;    
    }  

    switch (pMsg[1])
    {
        case VIRTMSM_MSG_SIO_CMD:
            virtmsm_sio_write_cb(pMsg[2]);
            break;
            
        case VIRTMSM_MSG_SIO_READ:
            if (0 != atomic_dec_and_test(&read_inprogress))
            {
                pSioBuf = kmalloc(pMsg[3], GFP_KERNEL);

                memcpy(pSioBuf, &pMsg[4], pMsg[3]);
                virtmsm_sio_read_cb(pMsg[2], pSioBuf, pMsg[3]);
                kfree(pSioBuf);
                pSioBuf = NULL;
                atomic_inc(&read_inprogress);
            }
            else
            {
                /*如果发现已有SIO还在处理中，那么重新进行消息处理等待*/
                atomic_set(&read_inprogress, 0);
                
                 if (bIsMalloc)
                 {
                   kfree(pMsg);
                 }
                 
                 continue;
            }
            
            break;

        default:
            break;
    }
    

    virtmsm_updatemsgRD(pMsg[0]);

    if (bIsMalloc)
    {
        kfree(pMsg);
    }
  }
}

void virtmsm_updatemsgRD(uint32 len)
{
  printk("virtmsm_updatemsgRD is called with %d;\n", (int)len);

  if ((pVirtMsmMsgRx->ulRD + len) >= (4096 - 8))
  {
     uint32 r_new = pVirtMsmMsgRx->ulRD + len - (4096 - 8);

     if(pVirtMsmMsgRx->ulRD <= pVirtMsmMsgRx->ulWR && r_new <= pVirtMsmMsgRx->ulWR)
     {
        printk("error1: r want over w,r:%d,w:%d,len:%d\n",
            (int)(pVirtMsmMsgRx->ulRD),(int)(pVirtMsmMsgRx->ulWR),
            (int)len);
        return;
     }

     pVirtMsmMsgRx->ulRD = pVirtMsmMsgRx->ulRD + len - (4096 - 8);
  }
  else
  {
    uint32 r = pVirtMsmMsgRx->ulRD;

    if(r <= pVirtMsmMsgRx->ulWR && (pVirtMsmMsgRx->ulRD + len) > pVirtMsmMsgRx->ulWR)
    {
        printk("error2: r want over w,r:%d,w:%d,len:%d\n",
            (int)r,(int)(pVirtMsmMsgRx->ulWR),(int)len);
        return;
    }
    
    pVirtMsmMsgRx->ulRD = pVirtMsmMsgRx->ulRD + len;
  }

  printk("pVirtMsmMsgRx->ulRD=%d; pVirtMsmMsgRx->ulWR=%d\n",
    (int)pVirtMsmMsgRx->ulRD, (int)pVirtMsmMsgRx->ulWR);
}

static byte * virtmsm_get_copymsg(uint32 msg_len)
{
   uint32 *pMsgTemp = NULL;
   uint32 loop = 0;
   
   pMsgTemp = kmalloc(msg_len, GFP_KERNEL);
   if(pMsgTemp==NULL)
   {
        return (byte *)NULL;
   }
   for (loop = 0; loop < msg_len; loop += 4)
   {
       pMsgTemp[loop / 4] = virtmsm_getmsgdword(loop);
   }
          
   return (byte *)pMsgTemp;
}

static byte *virtmsm_get_origmsg(void)
{
    return (byte *)&(pVirtMsmMsgRx->ulContent[pVirtMsmMsgRx->ulRD/4]);
}

static boolean virtmsm_getmsg(byte **ppMsg,boolean *pbIsMalloc)
{
  uint32 ulMsgLen = 0;
  
  if (HAVE_RCV_MSG)
  {
    ulMsgLen = virtmsm_getmsgdword(0);
    
    if (ulMsgLen < 8) 
    {
        printk("virtmsm_getmsg get ulMsgLen=%d error\n", (int)ulMsgLen);
        
        /* MSG 长度小于8, 丢弃该MSG */
        virtmsm_updatemsgRD(ulMsgLen);
        return FALSE;
    }
    
     if (pVirtMsmMsgRx->ulRD+ulMsgLen > (4096-8))
     {
          *ppMsg = virtmsm_get_copymsg(ulMsgLen);
          if (NULL==*ppMsg)
          {
              return FALSE;
          }
          
          *pbIsMalloc = TRUE;
     }
     else
     {
          *ppMsg = virtmsm_get_origmsg();
          *pbIsMalloc = FALSE;
     }
    
     return TRUE;
  }
  
  return FALSE;
}

static uint32 virtmsm_getmsgdword(uint32 offset)
{
  uint32 offsetTemp;

  if ((pVirtMsmMsgRx->ulRD + offset) >= (4096 - 8))
  {
    offsetTemp = pVirtMsmMsgRx->ulRD + offset - (4096 - 8);
  }
  else
  {
    offsetTemp = pVirtMsmMsgRx->ulRD + offset;
  }
  return pVirtMsmMsgRx->ulContent[offsetTemp / 4];
}

static uint32_t virtmsm_setmsgbytes(uint32_t offset, char* data, uint32_t length)
{
  
  uint32_t offsetTemp;
  uint32_t length_to_tail;
  char * pbuf = (char *)pVirtMsmMsgTx->ulContent;
  
  /*determine the write position*/
  offsetTemp = pVirtMsmMsgTx->ulWR + offset;

  if(offsetTemp < pVirtMsmMsgTx->ulRD)
  {
	memcpy(pbuf+offsetTemp, data, length);
  }
  else if (offsetTemp >= (4096 - 8))
  {
    offsetTemp = offsetTemp - (4096 - 8);
	
	memcpy(pbuf+offsetTemp, data, length);
  }
  else
  {
    length_to_tail = 4096 - 8 - offsetTemp;
	if (length_to_tail < length)
	{
      memcpy(pbuf+offsetTemp, data ,length_to_tail);
      memcpy(pbuf, data+length_to_tail, length-length_to_tail);
	}
	else
	{
	  memcpy(pbuf+offsetTemp, data ,length);
	}
  }

  barrier();

  return 0;
}

static uint32_t virtmsm_setmsgdword(uint32_t offset, uint32_t data)
{
  uint32 offsetTemp;

  if (virtmsm_FreeMsgspace() < offset)
  {
    return -1;
  }

  if ((pVirtMsmMsgTx->ulWR + offset) >= (4096 - 8))
  {
    offsetTemp = pVirtMsmMsgTx->ulWR + offset - (4096 - 8);
  }
  else
  {
    offsetTemp = pVirtMsmMsgTx->ulWR + offset;
  }
  
  pVirtMsmMsgTx->ulContent[offsetTemp / 4] = data;

  barrier();

  return 0;
}

/*****************************************************************************
 Prototype    : virt_msm_msg_send
 Description  : Send the virtual msm message process
 Input        : VIRT_MSM_MSG* pMsg  -- Message buffer
                UINT32 nMsgSize     -- Message size
 Output       : None
 Return Value : Error code
*****************************************************************************/
uint32_t virt_msm_msg_send(VIRT_MSM_MSG* pMsg, uint32_t unMsgSize)
{   
    uint32_t unRet = SUCCESS;   /* return value */
    uint32_t unPackageSize = unMsgSize + VIRT_MSM_MSG_PG_HEAD_LEN;

    if (NULL == pMsg)
    {
        return EINVAL;
    }

    do 
    {
        /* Set the package size */
        unRet = virtmsm_setmsgdword(VIRT_MSM_MSG_PG_LEN_POS, unPackageSize);

        /* Failed to set the package size */
        if (SUCCESS != unRet)
        {
            break;
        }

        /* Set the message type to package */
        unRet = virtmsm_setmsgdword(VIRT_MSM_MSG_PG_CMD_POS, 
            (uint32_t)(pMsg->msg_head.enCmd));

        /* Failed to set the message type */
        if (SUCCESS != unRet)
        {
           break;
        }

        /* The message have the message data */
    	if (unMsgSize > sizeof(VIRT_MSM_MSG_HD))
    	{
    	    /* Set the message data */
    	    unRet = virtmsm_setmsgbytes(VIRT_MSM_MSG_PG_DATA_POS, 
    		    (((char*)pMsg) + sizeof(VIRT_MSM_MSG_HD)), 
    			unMsgSize - sizeof(VIRT_MSM_MSG_HD));
    	}

        /* Failed to set the message data */
        if (SUCCESS != unRet)
        {
           break;
        }

        /* Sent the message */
        unRet = virtmsm_updatemsgWR(unPackageSize);
    }while(0);

    if (SUCCESS != unRet)
    {
        unRet = ENOBUFS;
    }

    return unRet;
}

