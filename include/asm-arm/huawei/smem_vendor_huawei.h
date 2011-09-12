/*<BU7D00272 yanzhijun 20100122 begin*/
/*
 * Copyright (C) 2008 The HUAWEI ,inc
 * All rights reserved.
 *
 */
#ifndef _SMEM_VENDOR_HUAWEI_H_
#define _SMEM_VENDOR_HUAWEI_H_

#define VENDOR_PROP_CMDLINE_ID  " androidboot.localproppath="

#define APP_USB_SERIAL_LEN   16

#define VENDOR_NAME_LEN      32

#define MAGIC_NUMBER_FACTORY 0xD0D79D41
#define MAGIC_NUMBER_NORMAL  0xA77011F2
#define NETWORK_UMTS         1
#define NETWORK_CDMA         2
unsigned char network_is_cdma(void);
unsigned char bootimage_is_recovery(void);
unsigned char runmode_is_factory(void);

typedef struct _app_usb_para_smem
{
  /* Stores usb serial number for apps */
  unsigned char usb_serial[APP_USB_SERIAL_LEN];
  unsigned usb_pid_index;
} app_usb_para_smem;

typedef struct _app_verder_name
{
  unsigned char vender_name[VENDOR_NAME_LEN];
  unsigned char country_name[VENDOR_NAME_LEN];
  /* del the update state */
}app_vender_name;

typedef struct
{
  app_usb_para_smem      usb_para;
  app_vender_name   vender_para;
  unsigned               network_type;
  unsigned               run_mode;
  unsigned char          reserved[32];
} smem_huawei_vender;

#define COUNTRY_JAPAN   "jp"
#define VENDOR_EMOBILE  "emobile"

#endif //_SMEM_VENDOR_HUAWEI_H_
/* BU7D00272 yanzhijun 20100122 end >*/

