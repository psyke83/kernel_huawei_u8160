/******************************************************************************

  Copyright (C), 2001-2011, Huawei Tech. Co., Ltd.

 ******************************************************************************
  File Name     : VirtSerialPort.h
  Version       : Initial Draft
  Author        : lihongxi
  Created       : 2009/8/10
  Last Modified :
  Description   : Virtual serial port interface define
  Function List :
  History       :
  1.Date        : 2009/8/10
    Author      : lihongxi
    Modification: Created file

******************************************************************************/

/*=============================================================================
                            !!! Notice !!!

    This file can only use in single thread. In other word, only one thread
can read or write the serial port device file. Otherwise you will make the 
date in serial port confusion!
=============================================================================*/

#ifndef _VIRT_SERIAL_PORT_H_
#define _VIRT_SERIAL_PORT_H_

#include <stdio.h>
#include <sys/epoll.h>
#include <VirtSerilaPortDataStructDef.h>

#ifndef min
#define min(a,b)    ( (a)<(b) ? (a):(b) )
#endif

/*****************************************************************************
 Prototype    : virt_read_bytes
 Description  : Read bytes from device file
 Input        : int fd        -- Device file id
                void* buf     -- Read buffer
                size_t count  -- Read 
 Output       : None
 Return Value : None
 Other        : It's not a open interface, do not use out of file
 *****************************************************************************/
__inline static ssize_t virt_read_bytes(int fd, void* buf, size_t count)
{
    char*   pReadStart = NULL;          /* Point to read buffer */
    ssize_t nReadOnce = 0;              /* Once read bytes */
    ssize_t nReadCount = 0;             /* Total read bytes */

    pReadStart = (char*)buf;

    /* Loop to read bytes from the device file */
    do
    {
        nReadOnce = read(fd, (void*)pReadStart, count);

        /*< BQ5D00129 Modified by lihongxi, 2009/8/22 BEGIN */
        if (nReadOnce < 0)
        {
            return nReadCount;
        }
        /* BQ5D00129 Modified by lihongxi, 2009/8/22 END>*/
        
        nReadCount += nReadOnce;
        pReadStart += nReadOnce;
        count -= nReadOnce;
    }while(count);

    return nReadCount;
}

/*****************************************************************************
 Prototype    : virt_discard_data
 Description  : Discard the serial port data
 Input        : int fd        -- Device file id
                size_t count  -- Discard data count
 Output       : None
 Return Value : None
 Other        : It's not a open interface, do not use out of file
*****************************************************************************/
__inline static void virt_discard_data(int fd, size_t count)
{
    size_t nLoop = 0;  /* Loop count */
    char tmp;       /* use as buffer */

    while(nLoop < count)
    {
        /* Once read one byte */
        (void)read(fd, (void*)(&tmp), 1);
        nLoop ++;
    }
}

/*****************************************************************************
 Prototype    : virt_read_msg_hd
 Description  : Read the message head
 Input        : int fd                       -- Device file id
                VirtPortPackageHd* pHeadBuf  -- Buffer to store package head 
 Output       : None
 Return Value : Read size
*****************************************************************************/
__inline static ssize_t virt_read_msg_hd(int fd, VirtPortPackageHd* pHeadBuf)
{   
    /* Check parameters */
    if (-1 == fd || NULL == pHeadBuf)
    {
        return -1;
    }

    return virt_read_bytes(fd, (void*)pHeadBuf, sizeof(VirtPortPackageHd));
}

/*****************************************************************************
 Prototype    : virt_read_msg_content
 Description  : Read message content
 Input        : int fd                       -- Device file id
                VirtPortPackageHd* pHeadBuf  -- Message head
                void* buf                    -- Message content out buffer
                size_t count                 -- Buffer size
 Output       : None
 Return Value : bytes to read
 Other        : The buffer must be enough, otherwise the data will be lost
*****************************************************************************/
__inline static ssize_t virt_read_msg_content(int fd, 
    const VirtPortPackageHd* pHeadBuf, void* buf, size_t count)
{
    ssize_t nRet = 0;              /* read bytes */
    
    /* Check parameters */
    if (-1 == fd || NULL == pHeadBuf
        || NULL == buf || 0 >= count)
    {
        return -1;
    }

    nRet = virt_read_bytes(fd, buf, min(count, pHeadBuf->unLen));

    /* The buffer is not big enough, discard other data */
    if (count < pHeadBuf->unLen)
    {
        virt_discard_data(fd, (pHeadBuf->unLen - count));
    }
    
    return nRet;
}

/*****************************************************************************
 Prototype    : virt_write_msg
 Description  : Write message to virtual serial port
 Input        : int fd        -- device file id
                void* buf     -- message buffer
                size_t count  -- message lenth to write
 Output       : None
 Return Value : The write lenth
*****************************************************************************/
__inline static ssize_t virt_write_msg(int fd, void* buf, size_t count)
{
    VirtPortPackageHd pgHead;   /* Package head */
    ssize_t nRet = 0;           /* Bytes number write to device file */

    /* check parameters */
    if (-1 == fd || 0 >= count)
    {
        return -1;
    }

    /* Set the package lenth */
    pgHead.unLen = count;

    /* Write the package head to device file */
    nRet = write(fd, (void*)(&pgHead), sizeof(VirtPortPackageHd));

    /* Fail to write the package head to device file */
    if (nRet != sizeof(VirtPortPackageHd))
    {
        return -1;
    }

    /* Write the package contern to device file */
    return write(fd, buf, count);
}

/*****************************************************************************
 Prototype    : virt_read_msg
 Description  : Read a message form virtual serial port
 Input        : int fd          -- device file id 
                void* buf       -- message buffer
                size_t count    -- buffer size
 Output       : None
 Return Value : read lenth
 Other        : The buffer must be enough, otherwise the data will be lost
*****************************************************************************/
__inline static ssize_t virt_read_msg(int fd, void* buf, size_t count)
{
    VirtPortPackageHd pgHead;           /* Package head */
    ssize_t nReadOnce = 0;              /* Once read bytes */

    /* Check parameters */
    if (-1 == fd || NULL == buf || 0 >= count)
    {
        return -1;
    }

    /* Read message head */
    nReadOnce = virt_read_msg_hd(fd, &pgHead);

    /* Read message head failed */
    if (nReadOnce != sizeof(VirtPortPackageHd))
    {
        return -1;
    }

    /* Read the message from device file */
    return virt_read_msg_content(fd, &pgHead, buf, count);
}

#endif /* END _VIRT_SERIAL_PORT_H_ */


