/*
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "himax0356.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "himax0356.c: " fmt, ## args)
//#define CDBG(fmt, args...)

/*=============================================================
    SENSOR REGISTER DEFINES
==============================================================*/
#define HIMAX0356_REG_MODEL_ID_H 0x01
#define HIMAX0356_REG_MODEL_ID_L 0x02
#define HIMAX0356_MODEL_ID 0x0356

#define HIMAX0356_REG_RESET_REGISTER 0x21


enum himax0356_test_mode_t
{
    TEST_OFF,
    TEST_1,
    TEST_2,
    TEST_3
};

enum himax0356_resolution_t
{
    QTR_SIZE,
    FULL_SIZE,
    INVALID_SIZE
};

enum himax0356_reg_update_t
{
    /* Sensor egisters that need to be updated during initialization */
    REG_INIT,

    /* Sensor egisters that needs periodic I2C writes */
    UPDATE_PERIODIC,

    /* All the sensor Registers will be updated */
    UPDATE_ALL,

    /* Not valid update */
    UPDATE_INVALID
};

enum himax0356_setting_t
{
    RES_PREVIEW,
    RES_CAPTURE
};

/*
 * Time in milisecs for waiting for the sensor to reset.
 */
#define HIMAX0356_RESET_DELAY_MSECS 66

/* for 30 fps preview */
#define HIMAX0356_DEFAULT_CLOCK_RATE 24000000

/* FIXME: Changes from here */
struct himax0356_work_t
{
    struct work_struct work;
};

struct himax0356_ctrl_t
{
    const struct  msm_camera_sensor_info *sensordata;

    int sensormode;
    uint32_t           fps_divider; /* init to 1 * 0x00000400 */
    uint32_t           pict_fps_divider; /* init to 1 * 0x00000400 */

    uint16_t curr_lens_pos;
    uint16_t init_curr_lens_pos;
    uint16_t my_reg_gain;
    uint32_t my_reg_line_count;

    enum himax0356_resolution_t prev_res;
    enum himax0356_resolution_t pict_res;
    enum himax0356_resolution_t curr_res;
    enum himax0356_test_mode_t  set_test;

    unsigned short imgaddr;
};

struct himax0356_i2c_reg_conf
{
    unsigned char reg;
    unsigned char value;
};

static struct himax0356_i2c_reg_conf himax0356_init_reg_config[] =
{
    {0x21,0x00},
    {0x1E,0x01},
    {0x18,0x80},
    {0x1C,0x3B},
    {0x19,0x81},
    {0x26,0x00},
    {0x06,0x80},
    {0x22,0x07},
    {0x25,0x09},
    {0xAA,0x10},
    {0x5D,0x80},
    {0x5E,0x90},
    {0x5F,0x40},
    {0x60,0x40},
    {0x64,0x10},
    {0xF4,0x1D},
    {0xF5,0x89},
    {0xB8,0x0C},
    {0xB9,0x0C},
    {0xA9,0xF1},
    {0xAC,0x20},
    {0x33,0x06},

    {0x80,0x10},
    {0x81,0xF0},
    {0x82,0xC8},
    {0x83,0x88},
    {0x84,0x88},
    {0x85,0xA8},
    {0x86,0xC8},
    {0x87,0x88},
    {0x88,0xA8},
    {0x89,0xC8},
    {0x8A,0x88},
    {0x8B,0x98},
    {0x8C,0xA8},
    {0x8D,0x88},
    {0x8E,0x10},
    {0x8F,0x0A},
    {0x90,0x10},
    {0x91,0x0A},
    {0x92,0x10},
    {0x93,0x0A},
    {0x94,0x10},
    {0x95,0x0A},
    {0x96,0x01},
    {0x97,0x01},
    {0x98,0x01},
    {0x99,0x01},
    {0x9A,0x00},
    {0x9B,0x00},
    {0x9C,0x00},
    {0x9D,0x00},
    {0x9E,0x20},
    {0x9F,0x20},
    {0xA0,0x20},
    {0xA1,0x20},
    {0xA2,0xEC},
    {0xA3,0xEC},
    {0xA4,0xEC},
    {0xA5,0xEC},

    {0x50,0x00},
    {0x51,0x91},
    {0x52,0x8C},
    {0x53,0xA4},
    {0x54,0xFB},
    {0x55,0xCF},
    {0x56,0x2C},
    {0x57,0x51},
    {0x58,0xF8},
    {0x59,0x94},
    {0x5A,0x20},
    {0x5B,0xEC},
    {0x5C,0x1F},

    {0x6C,0x40},
    {0x6D,0xA5},
    {0x6E,0xAA},
    {0x6F,0x3F},
    {0x70,0x32},
    {0x71,0x64},
    {0x72,0xBF},
    {0x73,0x64},
    {0x74,0xA9},
    {0x75,0xDE},
    {0x76,0x0E},
    {0x77,0x37},
    {0x78,0x5A},
    {0x79,0x76},
    {0x7A,0xA4},
    {0x7B,0xCB},
    {0x7C,0x12},
    {0x7D,0x56},
    {0x7E,0x99},
    {0x7F,0x88},

    {0x31,0x03},
    {0x32,0x78},
    {0x34,0x02},
    {0x35,0xC0},
    {0x38,0x4A},
    {0x30,0x38},
    {0xDA,0x48},
    {0xDB,0x28},

    {0xC5,0x40},
    {0xC6,0x80},
    {0xC7,0x58},
    {0xC8,0x64},
    {0xC9,0x70},
    {0xCA,0x48},
    {0xCD,0x78},

    {0x21,0x01},
    {0x3A,0x00},
    {0x5D,0x30},
    {0x5E,0x90},
    {0x5F,0x20},
    {0x60,0x05},
    {0x64,0x04},
    {0xB6,0x28},
    {0xB7,0x3C},
    {0xB8,0x0C},
    {0xBB,0x10},
    {0xAD,0x00},

    {0x21,0x03},
    {0x68,0x20},
    {0x6C,0x28},
    {0x71,0x00},
    {0x73,0x70},
    {0x7C,0x0B},
    {0x80,0xCC},
    {0x81,0x4C},
    {0x83,0xCA},
    {0x91,0x16},
    {0x92,0x6B},
    {0x95,0x08},
    {0x96,0x01},
    {0xA0,0x00},
    {0xA1,0x00},
    {0xA2,0x00},
    {0xA3,0x00},
    {0xA4,0x00},
    {0xA5,0x00},
    {0xA6,0x01},
    {0xB8,0x10},
    {0xBB,0x10},
    {0xB7,0x10},
    {0x6C,0x2A},
    {0x6D,0xB0},
    {0x6E,0x38},
    {0x70,0x50},
    {0x71,0x03},
    {0x73,0x70},
    {0x7C,0x0B},
    {0x21,0x00},

    {0x05,0x01},
    {0x00,0x01},

};
static struct  himax0356_work_t *himax0356sensorw = NULL;

static struct  i2c_client *himax0356_client = NULL;
static struct himax0356_ctrl_t *himax0356_ctrl = NULL;

static DECLARE_WAIT_QUEUE_HEAD(himax0356_wait_queue);
DECLARE_MUTEX(himax0356_sem);

static int himax0356_i2c_read(unsigned short saddr,
                           unsigned char reg, unsigned char *value)
{
    unsigned char buf;

    struct i2c_msg msgs[] = 
    {
    	{
    		.addr   = saddr,
    		.flags = 0,
    		.len   = 1,
    		.buf   = &buf,
    	},
    	{
    		.addr  = saddr,
    		.flags = I2C_M_RD,
    		.len   = 1,
    		.buf   = &buf,
    	},
	};
    
    buf = reg;
    
	if (i2c_transfer(himax0356_client->adapter, msgs, 2) < 0) {
		CDBG("himax0356_i2c_read failed!\n");
		return -EIO;
	}
    
    *value = buf;
    
	return 0;

}

static int himax0356_i2c_write(unsigned short saddr,
                            unsigned char reg, unsigned char value )
{
	unsigned char buf[2];
	struct i2c_msg msg[] = 
    {
    	{
    		.addr = saddr,
    		.flags = 0,
    		.len = 2,
    		.buf = buf,
    	},
	};
    
	buf[0] = reg;
	buf[1] = value;
    
	if (i2c_transfer(himax0356_client->adapter, msg, 1) < 0) {
		CDBG("himax0356_i2c_write faild\n");
		return -EIO;
	}

	return 0;
}

int32_t himax0356_i2c_write_table(struct himax0356_i2c_reg_conf *reg_conf_tbl, int num_of_items_in_table)
{
    int i;
    int32_t rc = -EFAULT;

    for (i = 0; i < num_of_items_in_table; i++)
    {
        rc = himax0356_i2c_write(himax0356_client->addr,
                              reg_conf_tbl->reg, reg_conf_tbl->value);
        if (rc < 0)
        {
            break;
        }

        reg_conf_tbl++;
    }

    return rc;
}

int32_t himax0356_set_default_focus(uint8_t af_step)
{
    int32_t rc = 0;

    return rc;
}

int32_t himax0356_set_fps(struct fps_cfg    *fps)
{
    /* input is new fps in Q8 format */
    int32_t rc = 0;

    CDBG("himax0356_set_fps\n");
    return rc;
}

int32_t himax0356_write_exp_gain(uint16_t gain, uint32_t line)
{
    CDBG("himax0356_write_exp_gain\n");
    return 0;
}

int32_t himax0356_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
    int32_t rc = 0;

    CDBG("himax0356_set_pict_exp_gain\n");

    mdelay(10);

    /* camera_timed_wait(snapshot_wait*exposure_ratio); */
    return rc;
}

int32_t himax0356_setting(enum himax0356_reg_update_t rupdate,
                       enum himax0356_setting_t    rt)
{
    int32_t rc = 0;

    switch (rupdate)
    {
        case UPDATE_PERIODIC:
            if (rt == RES_PREVIEW)
            {
                return rc;
            }
            else
            {}

            break;

        case REG_INIT:

            rc = himax0356_i2c_write_table(himax0356_init_reg_config,
                                        sizeof(himax0356_init_reg_config) / sizeof(himax0356_init_reg_config[0]));
            mdelay(5);
            return rc;
            break;

        default:
            rc = -EFAULT;
            break;
    } /* switch (rupdate) */

    return rc;
}

int32_t himax0356_video_config(int mode, int res)
{
    int32_t rc;

    switch (res)
    {
        case QTR_SIZE:
            rc = himax0356_setting(UPDATE_PERIODIC, RES_PREVIEW);
            if (rc < 0)
            {
                return rc;
            }

            CDBG("sensor configuration done!\n");
            break;

        case FULL_SIZE:
            rc = himax0356_setting(UPDATE_PERIODIC, RES_CAPTURE);
            if (rc < 0)
            {
                return rc;
            }

            break;

        default:
            return 0;
    } /* switch */

    himax0356_ctrl->prev_res   = res;
    himax0356_ctrl->curr_res   = res;
    himax0356_ctrl->sensormode = mode;

    return rc;
}

int32_t himax0356_snapshot_config(int mode)
{
    int32_t rc = 0;

    rc = himax0356_setting(UPDATE_PERIODIC, RES_CAPTURE);
    mdelay(50);
    if (rc < 0)
    {
        return rc;
    }

    himax0356_ctrl->curr_res = himax0356_ctrl->pict_res;

    himax0356_ctrl->sensormode = mode;

    return rc;
}

int32_t himax0356_power_down(void)
{
    int32_t rc = 0;

    mdelay(5);

    return rc;
}

int32_t himax0356_move_focus(int direction, int32_t num_steps)
{
    return 0;
}

static int himax0356_sensor_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
    if (data->vreg_disable_func)
    {
        data->vreg_disable_func(data->sensor_vreg, data->vreg_num);
    }
    
	return 0;
}

static int himax0356_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;
    uint8_t sensor_id_8bit_h = 0;
    uint8_t sensor_id_8bit_l = 0;
    uint16_t sensor_id_16bit = 0;

	rc = gpio_request(data->sensor_pwd, "himax0356");
	if (!rc)
		gpio_direction_output(data->sensor_pwd, 0);
	else
		goto init_probe_fail;
	mdelay(HIMAX0356_RESET_DELAY_MSECS);

    if (data->vreg_enable_func)
    {
        rc = data->vreg_enable_func(data->sensor_vreg, data->vreg_num);
        if (rc < 0)
        {
            goto init_probe_fail;
        }
    }
    mdelay(HIMAX0356_RESET_DELAY_MSECS);

    //gpio_direction_output(data->sensor_pwd, 0);
    //mdelay(5);
    
	/* RESET the sensor image part via I2C command */
	rc = himax0356_i2c_write(himax0356_client->addr,
		HIMAX0356_REG_RESET_REGISTER, 0x00);
	if (rc < 0)
		goto init_probe_fail;
    mdelay(5);
    
	/* 3. Read sensor Model ID: */
	rc = himax0356_i2c_read(himax0356_client->addr,
		HIMAX0356_REG_MODEL_ID_H, &sensor_id_8bit_h);
	if (rc < 0)
		goto init_probe_fail;   
    
	rc = himax0356_i2c_read(himax0356_client->addr,
		HIMAX0356_REG_MODEL_ID_L, &sensor_id_8bit_l);
	if (rc < 0)
		goto init_probe_fail;

    sensor_id_16bit = ((sensor_id_8bit_h << 8) | sensor_id_8bit_l);
	CDBG("himax0356 model_id = 0x%x\n", sensor_id_16bit);

	/* 4. Compare sensor ID to MT9T012VC ID: */
	if (sensor_id_16bit != HIMAX0356_MODEL_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}

    goto init_probe_done;

init_probe_fail:
    himax0356_sensor_init_done(data);
init_probe_done:
	return rc;
}

int himax0356_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	himax0356_ctrl = kzalloc(sizeof(struct himax0356_ctrl_t), GFP_KERNEL);
	if (!himax0356_ctrl) {
		CDBG("himax0356_sensor_open_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	himax0356_ctrl->fps_divider = 1 * 0x00000400;
	himax0356_ctrl->pict_fps_divider = 1 * 0x00000400;
	himax0356_ctrl->set_test = TEST_OFF;
	himax0356_ctrl->prev_res = QTR_SIZE;
	himax0356_ctrl->pict_res = FULL_SIZE;

	if (data)
		himax0356_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(HIMAX0356_DEFAULT_CLOCK_RATE);
	mdelay(20);

	msm_camio_camif_pad_reg_reset();
	mdelay(20);

  rc = himax0356_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	if (himax0356_ctrl->prev_res == QTR_SIZE)
		rc = himax0356_setting(REG_INIT, RES_PREVIEW);
	else
		rc = himax0356_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0)
		goto init_fail;
	else
		goto init_done;

init_fail:
	kfree(himax0356_ctrl);
init_done:
	return rc;
}

int himax0356_init_client(struct i2c_client *client)
{
    /* Initialize the MSM_CAMI2C Chip */
    init_waitqueue_head(&himax0356_wait_queue);
    return 0;
}

int32_t himax0356_set_sensor_mode(int mode, int res)
{
    int32_t rc = 0;

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
            CDBG("SENSOR_PREVIEW_MODE\n");
            rc = himax0356_video_config(mode, res);
            break;

        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
            CDBG("SENSOR_SNAPSHOT_MODE\n");
            rc = himax0356_snapshot_config(mode);
            break;

        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}


int himax0356_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	down(&himax0356_sem);

  CDBG("himax0356_sensor_config: cfgtype = %d\n",
	  cdata.cfgtype);
		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
			break;

		case CFG_GET_PREV_L_PF:
			break;

		case CFG_GET_PREV_P_PL:
			break;

		case CFG_GET_PICT_L_PF:
			break;

		case CFG_GET_PICT_P_PL:
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = himax0356_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				himax0356_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				himax0356_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = himax0356_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = himax0356_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				himax0356_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				himax0356_set_default_focus(
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_EFFECT:
			rc = himax0356_set_default_focus(
						cdata.cfg.effect);
			break;

		default:
			rc = -EFAULT;
			break;
		}

	up(&himax0356_sem);

	return rc;
}

int himax0356_sensor_release(void)
{
	int rc = -EBADF;

	down(&himax0356_sem);

	himax0356_power_down();

    himax0356_sensor_init_done(himax0356_ctrl->sensordata);

	kfree(himax0356_ctrl);

	up(&himax0356_sem);
	CDBG("himax0356_release completed!\n");
	return rc;
}

int himax0356_i2c_probe(struct i2c_client *         client,
                 const struct i2c_device_id *id)
{
    int rc = 0;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        rc = -ENOTSUPP;
        goto probe_failure;
    }

    himax0356sensorw =
        kzalloc(sizeof(struct himax0356_work_t), GFP_KERNEL);

    if (!himax0356sensorw)
    {
        rc = -ENOMEM;
        goto probe_failure;
    }

    i2c_set_clientdata(client, himax0356sensorw);
    himax0356_init_client(client);
    himax0356_client = client;

    if (rc)
    {
        goto probe_failure;
    }

    mdelay(50);

    if (rc)
    {
        goto probe_failure;
    }

    return 0;

probe_failure:
    kfree(himax0356sensorw);
    himax0356sensorw = NULL;
    return rc;
}

static const struct i2c_device_id himax0356_i2c_id[] = {
	{ "himax0356", 0},
	{ }
};

static struct i2c_driver himax0356_i2c_driver = {
	.id_table = himax0356_i2c_id,
	.probe  = himax0356_i2c_probe,
	.remove = __exit_p(himax0356_i2c_remove),
	.driver = {
		.name = "himax0356",
	},
};

static int himax0356_sensor_probe(
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	/* We expect this driver to match with the i2c device registered
	 * in the board file immediately. */
	int rc = i2c_add_driver(&himax0356_i2c_driver);
	if (rc < 0 || himax0356_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(HIMAX0356_DEFAULT_CLOCK_RATE);
	mdelay(20);

	rc = himax0356_probe_init_sensor(info);
	if (rc < 0) {
		i2c_del_driver(&himax0356_i2c_driver);
		goto probe_done;
	}

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CAMERA_SLAVE);
    #endif

	s->s_init = himax0356_sensor_open_init;
	s->s_release = himax0356_sensor_release;
	s->s_config  = himax0356_sensor_config;
	himax0356_sensor_init_done(info);

probe_done:
	return rc;
}

static int __himax0356_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, himax0356_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __himax0356_probe,
	.driver = {
		.name = "msm_camera_himax0356",
		.owner = THIS_MODULE,
	},
};

static int __init himax0356_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(himax0356_init);
