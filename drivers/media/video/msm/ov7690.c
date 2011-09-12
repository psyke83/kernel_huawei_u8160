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
#include "ov7690.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "ov7690.c: " fmt, ## args)
//#define CDBG(fmt, args...)

/*=============================================================
    SENSOR REGISTER DEFINES
==============================================================*/
#define OV7690_REG_MODEL_ID 0x0a
#define OV7690_MODEL_ID 0x76
#define OV7690_REG_RESET_REGISTER 0x12


enum ov7690_test_mode_t
{
    TEST_OFF,
    TEST_1,
    TEST_2,
    TEST_3
};

enum ov7690_resolution_t
{
    QTR_SIZE,
    FULL_SIZE,
    INVALID_SIZE
};

enum ov7690_reg_update_t
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

enum ov7690_setting_t
{
    RES_PREVIEW,
    RES_CAPTURE
};

/* actuator's Slave Address */
#define OV7690_AF_I2C_ADDR 0x18

/*
 * AF Total steps parameters
 */
#define OV7690_TOTAL_STEPS_NEAR_TO_FAR 30 /* 28 */

/*
 * Time in milisecs for waiting for the sensor to reset.
 */
#define OV7690_RESET_DELAY_MSECS 66

/* for 30 fps preview */
#define OV7690_DEFAULT_CLOCK_RATE 24000000

/* FIXME: Changes from here */
struct ov7690_work_t
{
    struct work_struct work;
};

struct ov7690_ctrl_t
{
    const struct  msm_camera_sensor_info *sensordata;

    int sensormode;
    uint32_t           fps_divider; /* init to 1 * 0x00000400 */
    uint32_t           pict_fps_divider; /* init to 1 * 0x00000400 */

    uint16_t curr_lens_pos;
    uint16_t init_curr_lens_pos;
    uint16_t my_reg_gain;
    uint32_t my_reg_line_count;

    enum ov7690_resolution_t prev_res;
    enum ov7690_resolution_t pict_res;
    enum ov7690_resolution_t curr_res;
    enum ov7690_test_mode_t  set_test;

    unsigned short imgaddr;
};

struct ov7690_i2c_reg_conf
{
    unsigned char reg;
    unsigned char value;
};

static struct ov7690_i2c_reg_conf ov7690_init_reg_config[] =
{
    {0x0c, 0x56},
    {0x48, 0x42},
    {0x41, 0x43},
    {0x4c, 0x73},

    {0x81, 0xef},
    {0x21, 0x44},
    {0x16, 0x03},
    {0x39, 0x80},
    {0x1e, 0xb1},

    ////===Format===////

    {0x12, 0x00},
    {0x82, 0x03},
    {0xd0, 0x48},
    {0x80, 0x7e},
    {0x3e, 0x30},
    {0x22, 0x00},

    ////===Resolution===////

    {0x17, 0x69},
    {0x18, 0xa4},
    {0x19, 0x0c},
    {0x1a, 0xf6},

    {0xc8, 0x02},
    {0xc9, 0x80}, //ISP input hsize (640)
    {0xca, 0x01},
    {0xcb, 0xe0}, //ISP input vsize (480)

    {0xcc, 0x02},
    {0xcd, 0x80}, //ISP output hsize (640)
    {0xce, 0x01},
    {0xcf, 0xe0}, //ISP output vsize (480)

    ////===Lens Correction==////
    {0x85, 0x90},
    {0x86, 0x18},

    {0x87, 0x00},
    {0x88, 0x10},

    {0x89, 0x18},
    {0x8a, 0x10},
    {0x8b, 0x14},

    ////====Color Matrix====////

    {0xBB, 0x20},
    {0xBC, 0x40},
    {0xBD, 0x60},
    {0xBE, 0x58},
    {0xBF, 0x48},
    {0xC0, 0x10},
    {0xC1, 0x33},
    {0xc2, 0x02},

    ////===Edge + Denoise====////
    {0xb7, 0x02},
    {0xb8, 0x0b},
    {0xb9, 0x00},
    {0xba, 0x18},

    ////===UVAdjust====////
    {0x5A, 0x4A},
    {0x5B, 0x9F},
    {0x5C, 0x48},
    {0x5d, 0x32},

    ////====AEC/AGC target====////

    {0x24, 0x88},
    {0x25, 0x78},
    {0x26, 0xb3},

    ////====Gamma====////

    {0xa3, 0x0a},
    {0xa4, 0x13},
    {0xa5, 0x28},
    {0xa6, 0x50},
    {0xa7, 0x60},
    {0xa8, 0x72},
    {0xa9, 0x7e},
    {0xaa, 0x8a},
    {0xab, 0x94},
    {0xac, 0x9c},
    {0xad, 0xa8},
    {0xae, 0xb4},
    {0xaf, 0xc6},
    {0xb0, 0xd7},
    {0xb1, 0xe8},
    {0xb2, 0x20},

    ////===AWB===////

    ////==Simple==////
    {0x8e, 0x92}, // simple AWB
    {0x96, 0xff},
    {0x97, 0x00}, //unlimit AWB range.

    ////==Advance==////


    ////==General Control==////

    {0x50, 0x4d}, //4c
    {0x51, 0x3f},
    {0x21, 0x57}, //Steps
    {0x20, 0x00},

    {0x14, 0x39},
    {0x13, 0xf7},
    {0x11, 0x01},
    {0x68, 0xb0},
};
static struct  ov7690_work_t *ov7690sensorw = NULL;

static struct  i2c_client *ov7690_client = NULL;
static struct ov7690_ctrl_t *ov7690_ctrl = NULL;

static DECLARE_WAIT_QUEUE_HEAD(ov7690_wait_queue);
DECLARE_MUTEX(ov7690_sem);

static int ov7690_i2c_read(unsigned short saddr,
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
    
	if (i2c_transfer(ov7690_client->adapter, msgs, 2) < 0) {
		CDBG("ov7690_i2c_read failed!\n");
		return -EIO;
	}
    
    *value = buf;
    
	return 0;

}

static int ov7690_i2c_write(unsigned short saddr,
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
    
	if (i2c_transfer(ov7690_client->adapter, msg, 1) < 0) {
		CDBG("ov7690_i2c_write faild\n");
		return -EIO;
	}

	return 0;
}

int32_t ov7690_i2c_write_table(struct ov7690_i2c_reg_conf *reg_conf_tbl, int num_of_items_in_table)
{
    int i;
    int32_t rc = -EFAULT;

    for (i = 0; i < num_of_items_in_table; i++)
    {
        rc = ov7690_i2c_write(ov7690_client->addr,
                              reg_conf_tbl->reg, reg_conf_tbl->value);
        if (rc < 0)
        {
            break;
        }

        reg_conf_tbl++;
    }

    return rc;
}

int32_t ov7690_set_default_focus(uint8_t af_step)
{
    int32_t rc = 0;
    uint8_t code_val_msb, code_val_lsb;

    CDBG("ov7690_set_default_focus,af_step:%d\n", af_step);
    return 0;
    code_val_msb = 0x01;
    code_val_lsb = af_step;

    /* Write the digital code for current to the actuator */
    rc = ov7690_i2c_write(OV7690_AF_I2C_ADDR >> 1,
                          code_val_msb, code_val_lsb);

    ov7690_ctrl->curr_lens_pos = 0;
    ov7690_ctrl->init_curr_lens_pos = 0;
    return rc;
}

int32_t ov7690_set_fps(struct fps_cfg    *fps)
{
    /* input is new fps in Q8 format */
    int32_t rc = 0;

    CDBG("ov7690_set_fps\n");
    return rc;
}

int32_t ov7690_write_exp_gain(uint16_t gain, uint32_t line)
{
    CDBG("ov7690_write_exp_gain\n");
    return 0;
}

int32_t ov7690_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
    int32_t rc = 0;

    CDBG("ov7690_set_pict_exp_gain\n");

    mdelay(10);

    /* camera_timed_wait(snapshot_wait*exposure_ratio); */
    return rc;
}

int32_t ov7690_setting(enum ov7690_reg_update_t rupdate,
                       enum ov7690_setting_t    rt)
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

            rc = ov7690_i2c_write_table(ov7690_init_reg_config,
                                        sizeof(ov7690_init_reg_config) / sizeof(ov7690_init_reg_config[0]));
            mdelay(5);
            return rc;
            break;

        default:
            rc = -EFAULT;
            break;
    } /* switch (rupdate) */

    return rc;
}

int32_t ov7690_video_config(int mode, int res)
{
    int32_t rc;

    switch (res)
    {
        case QTR_SIZE:
            rc = ov7690_setting(UPDATE_PERIODIC, RES_PREVIEW);
            if (rc < 0)
            {
                return rc;
            }

            CDBG("sensor configuration done!\n");
            break;

        case FULL_SIZE:
            rc = ov7690_setting(UPDATE_PERIODIC, RES_CAPTURE);
            if (rc < 0)
            {
                return rc;
            }

            break;

        default:
            return 0;
    } /* switch */

    ov7690_ctrl->prev_res   = res;
    ov7690_ctrl->curr_res   = res;
    ov7690_ctrl->sensormode = mode;

    return rc;
}

int32_t ov7690_snapshot_config(int mode)
{
    int32_t rc = 0;

    rc = ov7690_setting(UPDATE_PERIODIC, RES_CAPTURE);
    mdelay(50);
    if (rc < 0)
    {
        return rc;
    }

    ov7690_ctrl->curr_res = ov7690_ctrl->pict_res;

    ov7690_ctrl->sensormode = mode;

    return rc;
}

int32_t ov7690_power_down(void)
{
    int32_t rc = 0;

    mdelay(5);

    return rc;
}

int32_t ov7690_move_focus(int direction, int32_t num_steps)
{
    int16_t step_direction;
    int16_t actual_step;
    int16_t next_position;
    int16_t break_steps[4];
    uint8_t code_val_msb, code_val_lsb;
    int16_t i;

    if (num_steps > OV7690_TOTAL_STEPS_NEAR_TO_FAR)
    {
        num_steps = OV7690_TOTAL_STEPS_NEAR_TO_FAR;
    }
    else if (num_steps == 0)
    {
        return -EINVAL;
    }

    if (direction == MOVE_NEAR)
    {
        step_direction = 4;
    }
    else if (direction == MOVE_FAR)
    {
        step_direction = -4;
    }
    else
    {
        return -EINVAL;
    }

    if (ov7690_ctrl->curr_lens_pos < ov7690_ctrl->init_curr_lens_pos)
    {
        ov7690_ctrl->curr_lens_pos = ov7690_ctrl->init_curr_lens_pos;
    }

    actual_step =
        (int16_t) (step_direction *
                   (int16_t) num_steps);

    for (i = 0; i < 4; i++)
    {
        break_steps[i] =
            actual_step / 4 * (i + 1) - actual_step / 4 * i;
    }

    for (i = 0; i < 4; i++)
    {
        next_position =
            (int16_t)
            (ov7690_ctrl->curr_lens_pos + break_steps[i]);

        if (next_position > 255)
        {
            next_position = 255;
        }
        else if (next_position < 0)
        {
            next_position = 0;
        }

        code_val_msb =
            ((next_position >> 4) << 2) |
            ((next_position << 4) >> 6);

        code_val_lsb =
            ((next_position & 0x03) << 6);

        /* Writing the digital code for current to the actuator */
        if (ov7690_i2c_write(OV7690_AF_I2C_ADDR >> 1,
                             code_val_msb, code_val_lsb) < 0)
        {
            return -EBUSY;
        }

        /* Storing the current lens Position */
        ov7690_ctrl->curr_lens_pos = next_position;

        if (i < 3)
        {
            mdelay(1);
        }
    } /* for */

    return 0;
}

static int ov7690_sensor_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
    if (data->vreg_disable_func)
    {
        data->vreg_disable_func(data->sensor_vreg, data->vreg_num);
    }
    
	return 0;
}

static int ov7690_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;
	unsigned char chipid;

	rc = gpio_request(data->sensor_pwd, "ov7690");
	if (!rc)
		gpio_direction_output(data->sensor_pwd, 0);
	else
		goto init_probe_fail;
	mdelay(OV7690_RESET_DELAY_MSECS);

    if (data->vreg_enable_func)
    {
        rc = data->vreg_enable_func(data->sensor_vreg, data->vreg_num);
        if (rc < 0)
        {
            goto init_probe_fail;
        }
    }
    mdelay(OV7690_RESET_DELAY_MSECS);

    //gpio_direction_output(data->sensor_pwd, 0);
    //mdelay(5);
    
	/* RESET the sensor image part via I2C command */
	rc = ov7690_i2c_write(ov7690_client->addr,
		OV7690_REG_RESET_REGISTER, 0x80);
	if (rc < 0)
		goto init_probe_fail;
    mdelay(5);
    
	/* 3. Read sensor Model ID: */
	rc = ov7690_i2c_read(ov7690_client->addr,
		OV7690_REG_MODEL_ID, &chipid);

	if (rc < 0)
		goto init_probe_fail;

	CDBG("ov7690 model_id = 0x%x\n", chipid);

	/* 4. Compare sensor ID to MT9T012VC ID: */
	if (chipid != OV7690_MODEL_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}

    rc = ov7690_setting(REG_INIT, RES_PREVIEW);
	if (rc < 0)
		goto init_probe_fail;  
   
    goto init_probe_done;

init_probe_fail:
    ov7690_sensor_init_done(data);
init_probe_done:
	return rc;
}

int ov7690_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	ov7690_ctrl = kzalloc(sizeof(struct ov7690_ctrl_t), GFP_KERNEL);
	if (!ov7690_ctrl) {
		CDBG("ov7690_sensor_open_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	ov7690_ctrl->fps_divider = 1 * 0x00000400;
	ov7690_ctrl->pict_fps_divider = 1 * 0x00000400;
	ov7690_ctrl->set_test = TEST_OFF;
	ov7690_ctrl->prev_res = QTR_SIZE;
	ov7690_ctrl->pict_res = FULL_SIZE;

	if (data)
		ov7690_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(OV7690_DEFAULT_CLOCK_RATE);
	mdelay(20);

	msm_camio_camif_pad_reg_reset();
	mdelay(20);

  rc = ov7690_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	if (ov7690_ctrl->prev_res == QTR_SIZE)
		rc = ov7690_setting(REG_INIT, RES_PREVIEW);
	else
		rc = ov7690_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0)
		goto init_fail;
	else
		goto init_done;

init_fail:
	kfree(ov7690_ctrl);
init_done:
	return rc;
}

int ov7690_init_client(struct i2c_client *client)
{
    /* Initialize the MSM_CAMI2C Chip */
    init_waitqueue_head(&ov7690_wait_queue);
    return 0;
}

int32_t ov7690_set_sensor_mode(int mode, int res)
{
    int32_t rc = 0;

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
            CDBG("SENSOR_PREVIEW_MODE\n");
            rc = ov7690_video_config(mode, res);
            break;

        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
            CDBG("SENSOR_SNAPSHOT_MODE\n");
            rc = ov7690_snapshot_config(mode);
            break;

        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}


int ov7690_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	down(&ov7690_sem);

  CDBG("ov7690_sensor_config: cfgtype = %d\n",
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
			rc = ov7690_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				ov7690_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				ov7690_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = ov7690_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = ov7690_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				ov7690_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				ov7690_set_default_focus(
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_EFFECT:
			rc = ov7690_set_default_focus(
						cdata.cfg.effect);
			break;

		default:
			rc = -EFAULT;
			break;
		}

	up(&ov7690_sem);

	return rc;
}

int ov7690_sensor_release(void)
{
	int rc = -EBADF;

	down(&ov7690_sem);

	ov7690_power_down();

    ov7690_sensor_init_done(ov7690_ctrl->sensordata);

	kfree(ov7690_ctrl);

	up(&ov7690_sem);
	CDBG("ov7690_release completed!\n");
	return rc;
}

static int ov7690_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	ov7690sensorw =
		kzalloc(sizeof(struct ov7690_work_t), GFP_KERNEL);

	if (!ov7690sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, ov7690sensorw);
	ov7690_init_client(client);
	ov7690_client = client;
	//ov7690_client->addr = ov7690_client->addr >> 1;
	mdelay(50);

	CDBG("i2c probe ok\n");
	return 0;

probe_failure:
	kfree(ov7690sensorw);
	ov7690sensorw = NULL;
	pr_err("i2c probe failure %d\n", rc);
	return rc;
}

static const struct i2c_device_id ov7690_i2c_id[] = {
	{ "ov7690", 0},
	{ }
};

static struct i2c_driver ov7690_i2c_driver = {
	.id_table = ov7690_i2c_id,
	.probe  = ov7690_i2c_probe,
	.remove = __exit_p(ov7690_i2c_remove),
	.driver = {
		.name = "ov7690",
	},
};

static int ov7690_sensor_probe(
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	/* We expect this driver to match with the i2c device registered
	 * in the board file immediately. */
	int rc = i2c_add_driver(&ov7690_i2c_driver);
	if (rc < 0 || ov7690_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(OV7690_DEFAULT_CLOCK_RATE);
	mdelay(20);

	rc = ov7690_probe_init_sensor(info);
	if (rc < 0) {
		i2c_del_driver(&ov7690_i2c_driver);
		goto probe_done;
	}

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CAMERA_SLAVE);
    #endif

	s->s_init = ov7690_sensor_open_init;
	s->s_release = ov7690_sensor_release;
	s->s_config  = ov7690_sensor_config;
	ov7690_sensor_init_done(info);

probe_done:
	return rc;
}

static int __ov7690_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov7690_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov7690_probe,
	.driver = {
		.name = "msm_camera_ov7690",
		.owner = THIS_MODULE,
	},
};

static int __init ov7690_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov7690_init);
