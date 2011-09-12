/* arch/arm/mach-msm/huawei_serial_server.c
 *
 * Copyright (C) 2009 HUAWEI Corporation.
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
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <mach/board.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <mach/msm_rpcrouter.h>
#include <asm/io.h>

#define SERIAL_MEM_ADDR 0xFFFC000
#define SERIAL_MEM_SIZE 0X2000


struct serial_mem_buf
{
    unsigned addr;
    unsigned size;
};

// extern void virtmsm_handlemsg(void);
extern void virtmsm_msg_init(void);

static int handle_serial_call(struct msm_rpc_server *server, struct rpc_request_hdr *req, unsigned len);

static struct msm_rpc_endpoint *endpoint;

static char *huawei_serial_msg_buf_msm_to_app;//4
static char *huawei_serial_msg_buf_app_to_msm;//4

char* get_msm_to_app_msg_buf(void)
{
    return huawei_serial_msg_buf_msm_to_app;
}

char* get_app_to_msm_msg_buf(void)
{
    return huawei_serial_msg_buf_app_to_msm;
}

#define HUAWEI_SERIAL_CLIENT_PROG 0x30000096
#define HUAWEI_SERIAL_CLIENT_VERS 0x00000011

#define HUAWEI_SERIAL_CLIENT_SVC_OK_PROC 0x01
#define HUAWEI_SERIAL_NOTIFY_MODEM_DONE_PROC 0x02

int huawei_serial_notify_modem_done(int data)
{
    int rc = 0;

	struct set_batt_delta_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	/*server is ok*/
	req.data = cpu_to_be32(data);
    printk(KERN_ERR"huawei_serial_notify_modem_done is called!!\n");

    /* init rpc */
	endpoint = msm_rpc_connect(HUAWEI_SERIAL_CLIENT_PROG, HUAWEI_SERIAL_CLIENT_VERS, 0);
	if (IS_ERR(endpoint)) {
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
		       __FUNCTION__, PTR_ERR(endpoint));
		return rc;
	}

    rc = msm_rpc_call(endpoint,HUAWEI_SERIAL_NOTIFY_MODEM_DONE_PROC,
		&req, sizeof(req), -1);

	if(rc < 0)
		printk(KERN_ERR "%s: rpc call failed! rc = %ld\n",
		       __FUNCTION__, PTR_ERR(endpoint));

    return rc;
}

static int huawei_serial_driver_probe(struct platform_device *pdev)
{
    int rc = 0;
    struct serial_mem_buf serial_buf;
    
	struct set_batt_delta_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;

	/*server is ok*/
	req.data = cpu_to_be32(1);
	
    /* init rpc */
	endpoint = msm_rpc_connect(HUAWEI_SERIAL_CLIENT_PROG, HUAWEI_SERIAL_CLIENT_VERS, 0);
	if (IS_ERR(endpoint)) {
		printk(KERN_ERR "%s: init rpc failed! rc = %ld\n",
		       __FUNCTION__, PTR_ERR(endpoint));
		return rc;
	}

    //mem map
    serial_buf.addr = (unsigned)ioremap(SERIAL_MEM_ADDR, SERIAL_MEM_SIZE);
    serial_buf.size = SERIAL_MEM_SIZE;
    if (!serial_buf.addr)
    {
        printk("ioremap failed\n"); 
        return -EBUSY;
    }

    huawei_serial_msg_buf_msm_to_app = (char*)serial_buf.addr;
    huawei_serial_msg_buf_app_to_msm = (char*)(serial_buf.addr + SERIAL_MEM_SIZE/2);
	virtmsm_msg_init();
	
    rc = msm_rpc_call(endpoint,HUAWEI_SERIAL_CLIENT_SVC_OK_PROC,
		&req, sizeof(req), 5 * HZ);

	if(rc < 0)
		printk(KERN_ERR "%s: rpc call failed! rc = %ld\n",
		       __FUNCTION__, PTR_ERR(endpoint));

    return 0;
}


static struct platform_driver huawei_serial_server_driver =
{
    .probe     = huawei_serial_driver_probe,
    .driver    = {
        .name  = "hw_ss_driver",
        .owner = THIS_MODULE,
    },
};

 
/* serial server definitions */
#define SERIAL_SERVER_PROG				0x30000095
#define SERIAL_SERVER_VERS				0x00000011
#define RPC_BATT_MTOA_NULL				0x00000000

static struct msm_rpc_server huawei_serial_server = {
	.prog = SERIAL_SERVER_PROG,
	.vers = SERIAL_SERVER_VERS,
	.rpc_call = handle_serial_call,
};

#define RPC_HW_SERIAL_NULL 0x00
#define RPC_HW_SERIAL_DATA 0x01

struct rpc_serial_data_args {
	u32 data;
};
static uint32_t procedure = 0;
#if 0
static void recv_data(u32 data)
{
	procedure = data;
}
#endif
static int handle_serial_call(struct msm_rpc_server *server,
			       struct rpc_request_hdr *req, unsigned len)
{	
    procedure = req->procedure;
	switch (req->procedure) {
	case RPC_HW_SERIAL_NULL:
		return 0;
    
	case RPC_HW_SERIAL_DATA:{
		struct rpc_serial_data_args *args;
		args = (struct rpc_serial_data_args *)(req + 1);
		args->data = be32_to_cpu(args->data);
		// virtmsm_handlemsg();
		printk("huawei serial server recv: %d\n", args->data);
		return 0;
	}
	default:
		printk(KERN_ERR "%s: program 0x%08x:%d: unknown procedure %d\n",
		       __FUNCTION__, req->prog, req->vers, req->procedure);
		return -ENODEV;
	}
}

static int __devinit hw_serial_server_init(void)
{
    msm_rpc_create_server(&huawei_serial_server);
    platform_driver_register(&huawei_serial_server_driver);
    return 0;
}

module_init(hw_serial_server_init);
MODULE_DESCRIPTION("Huawei Serial Server Driver");
MODULE_LICENSE("GPL");

