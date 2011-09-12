

#include <linux/uaccess.h>
#include <mach/gpio.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h> 
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/crc-ccitt.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <mach/msm_rpcrouter.h>
#include <linux/hw_dev_dec.h>

#ifndef TYPEDEF_UINT8
typedef unsigned char	uint8;
#endif

#ifndef TYPEDEF_UINT16
typedef unsigned short	uint16;
#endif

#ifndef TYPEDEF_UINT32
typedef unsigned int	uint32;
#endif

#ifndef false
#define false 0
#endif
#ifndef true
#define true 1
#endif

#define BATTERY_RPC_PROG        0x30000089
#define BATTERY_RPC_VER_2_1	    0X00010003
#define HW_DEV_SET_FLAG_PROC   123
#define HW_DEV_RPC_TIMEOUT    5000	/* 5 sec */

/* all devices in this flag */
typedef struct{
    uint32  part1;
    uint32  part2;
}hw_dev_dct_flag_t; 
static hw_dev_dct_flag_t dev_flag = {0};

/* rpc client for set flag to modem side */
struct msm_rpc_client *hw_dev_rpc_client;

/* set device flag */
int set_hw_dev_flag( int dev_id )
{
    int dev_id_part2 = 0;

    /* out of max value */
    if( dev_id >= DEV_MAX )
    {
        return false;
    }

    /* move 0-31 */
    if( (dev_id >= 0) && ( dev_id <= 31 ) )
    {
        dev_flag.part1 = dev_flag.part1 | (1 << dev_id) ;
    }
    /* move 32-63 */
    else if( (dev_id >= 32) && ( dev_id <= 63 ) )
    {
        dev_id_part2 = dev_id - 32;
        dev_flag.part2 = dev_flag.part2 | (1 << dev_id_part2) ;
    }
    else 
    {
        return false;
    }

    return true;
}

/* get device flag */
hw_dev_dct_flag_t get_hw_dev_flag( void )
{
    return dev_flag;
}

static int hw_dev_set_flag_arg_func(struct msm_rpc_client *i2c_client,
				       void *buf, void *data)
{
	hw_dev_dct_flag_t *filter_delta_req =
		(hw_dev_dct_flag_t *)data;
	uint32 *req = (uint32 *)buf;
	int size = 0;

	*req = cpu_to_be32((filter_delta_req->part1));
	size += sizeof(uint32);
    req++;

	*req = cpu_to_be32((filter_delta_req->part2));
	size += sizeof(uint32);
	
	return size;
}

/* RPC call-back function for set flag */
static int hw_rpc_set_dev_flag( void )
{
	int rc;
	hw_dev_dct_flag_t req;

	req = get_hw_dev_flag();

    printk( " -------part1 is 0x%x\n part2 is 0x%x\n ", req.part1, req.part2 );

	rc = msm_rpc_client_req(hw_dev_rpc_client,
			HW_DEV_SET_FLAG_PROC,
			hw_dev_set_flag_arg_func, &req,
			NULL, NULL,
			msecs_to_jiffies(HW_DEV_RPC_TIMEOUT));

	if (rc < 0) 
    {
		pr_err("%s: FAIL: i2c dev set flag. rc=%d\n", __func__, rc);
		return rc;
	}
    
	return 0;
    
}


static int __devinit dev_dct_probe(struct platform_device *pdev)
{
    int ret = 0;

     printk( " -------dev_dct_probe\n " );

	hw_dev_rpc_client =
		msm_rpc_register_client("dev_dct", BATTERY_RPC_PROG,
					BATTERY_RPC_VER_2_1,
					1, NULL);

	if ( NULL == hw_dev_rpc_client ) 
    {
		printk("%s: FAIL: rpc_register_client. hw_dev_rpc_client=NULL\n",
		       __func__);
		return -ENODEV;
    }

    ret = hw_rpc_set_dev_flag();
    if( ret < 0 )
    {
        printk("%s: set hw dev detect flag failed.\n", __func__ );
    }

	return 0;
    
}


static struct platform_driver i2c_dev_dct_driver = {
	.driver	= {
		.name	= "hw-dev-detect",
	},
	.probe		= dev_dct_probe,
	.remove		= NULL,
};

static int __init hw_dev_dct_init(void)
{
	return platform_driver_register(&i2c_dev_dct_driver);
}

static void __exit hw_dev_dct_exit(void)
{
	platform_driver_unregister(&i2c_dev_dct_driver);
}

/* priority is 7s */
late_initcall_sync(hw_dev_dct_init);
//module_init(hw_dev_dct_init);
module_exit(hw_dev_dct_exit);

MODULE_AUTHOR("sjm");
MODULE_DESCRIPTION("I2C Device Detect Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:i2c_dev_dct");


