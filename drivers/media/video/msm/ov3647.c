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
#include "ov3647.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#undef CDBG
//#define CDBG(fmt, args...) printk(KERN_INFO "ov3647.c: " fmt, ## args)
#define CDBG(fmt, args...) 

/*=============================================================
    SENSOR REGISTER DEFINES
==============================================================*/
#define OV3647_REG_MODEL_ID 0x0a
#define OV3647_MODEL_ID 0x36
#define OV3647_REG_RESET_REGISTER 0x12

struct ov3647_reg_struct
{
    uint16_t pll_multiplier;          /*  0x0306 */

    uint16_t x_output_size;           /*  0x034C */
    uint16_t y_output_size;           /*  0x034E */
    uint16_t line_length_pck;         /*  0x300C */
    uint16_t frame_length_lines;      /*  0x300A */
};

/*Micron settings from Applications for lower power consumption.*/
struct ov3647_reg_struct ov3647_reg_pat[2] =
{
    {
        /* pll_multiplier  REG=0x0306 60 for 30fps preview, 40
         * for 20fps preview */
        10,

        /* x_output_size REG=0x034C */
        1024,

        /* y_output_size REG=0x034E */
        768,

        /* line_length_pck    REG=0x300C */
        1540,

        /* frame_length_lines REG=0x300A */
        788,
    },
    { /*Snapshot */

        /* pll_multiplier REG=0x0306 50 for 15fps snapshot,
         * 40 for 10fps snapshot */
        16,

        /* x_output_size REG=0x034C */
        2064,

        /* y_output_size REG=0x034E */
        1544,

    	/* line_length_pck REG=0x300C */
    	2364,

    	/* frame_length_lines    REG=0x300A */
    	1574,
    }
};

enum ov3647_test_mode_t
{
    TEST_OFF,
    TEST_1,
    TEST_2,
    TEST_3
};

enum ov3647_resolution_t
{
    QTR_SIZE,
    FULL_SIZE,
    INVALID_SIZE
};

enum ov3647_reg_update_t
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

enum ov3647_setting_t
{
    RES_PREVIEW,
    RES_CAPTURE
};

/* actuator's Slave Address */
#define OV3647_AF_I2C_ADDR 0x18

/*
 * AF Total steps parameters
 */
#define OV3647_TOTAL_STEPS_NEAR_TO_FAR 28 /* 28 */
#define OV3647_POSITION_MAX 1023
#define OV3647_ONE_STEP_POSITION ((OV3647_POSITION_MAX+1)/OV3647_TOTAL_STEPS_NEAR_TO_FAR)

/*
 * Time in milisecs for waiting for the sensor to reset.
 */
#define OV3647_RESET_DELAY_MSECS 66

/* for 30 fps preview */
#define OV3647_DEFAULT_CLOCK_RATE 24000000
#define OV3647_DEFAULT_MAX_FPS 26

/* FIXME: Changes from here */
struct ov3647_work_t
{
    struct work_struct work;
};

struct ov3647_ctrl_t
{
    const struct  msm_camera_sensor_info *sensordata;

    int sensormode;
    uint32_t           fps_divider; /* init to 1 * 0x00000400 */
    uint32_t           pict_fps_divider; /* init to 1 * 0x00000400 */

    uint16_t curr_lens_pos;
    uint16_t init_curr_lens_pos;
    uint16_t my_reg_gain;
    uint32_t my_reg_line_count;

    enum ov3647_resolution_t prev_res;
    enum ov3647_resolution_t pict_res;
    enum ov3647_resolution_t curr_res;
    enum ov3647_test_mode_t  set_test;

    unsigned short imgaddr;
};

struct ov3647_i2c_reg_conf
{
    unsigned char reg;
    unsigned char value;
};
static struct ov3647_i2c_reg_conf ov3647_init_reg_config[] =
{
    {0x05, 0xfe},
    {0x06, 0x00},

    {0x77, 0x00},
    {0x78, 0x00},
    {0x01, 0x3f},
    {0x0c, 0x0f},
    {0x0e, 0x36},
    {0x0f, 0x0c},
    {0x13, 0x00},
    {0x15, 0x82},
    {0x16, 0x83},
    {0x21, 0x34},
    {0x25, 0x08},
    {0x72, 0x02},
    {0x75, 0x06},
    {0x7a, 0x08},
    {0x7b, 0x12},
    {0x91, 0x4a},
    {0x99, 0x4c},
    {0x9b, 0xf0},
    {0xA8, 0x20},
    {0x9B, 0x00},
    {0xA9, 0xB3},
    {0x9c, 0x18},
    {0xd8, 0x0d},
    {0xd9, 0x80},
    {0x78, 0x60},
    {0xa6, 0x83},
    {0xd8, 0x0d},
    {0xd9, 0x8d},
    {0x67, 0xcc},
    {0x66, 0x02},
    {0x21, 0x30},
    {0x23, 0x06},
    {0xd9, 0x8e},
   
    {0x92, 0x80},
    {0x11, 0x00},
    {0x7b, 0x02},

    {0xb0, 0x02},
    {0xb1, 0xcf},
    {0xB7,0x43},
    {0xB8,0x31},
    {0xB9,0x17},
    {0xBA,0x3e},
    {0xBB,0x28},
    {0xBC,0x87},

    {0xBD,0x42},
    {0xBE,0x30},
    {0xBF,0x6c},
    {0xC0,0x3a},
    {0xC1,0xA4},
    {0xC2,0x87},

    {0xC3,0x43},
    {0xC4,0x31},
    {0xC5,0x0d},
    {0xC6,0x37},
    {0xC7,0x82},
    {0xC8,0x87},
        
    {0xc9,0x75},
};
static struct ov3647_i2c_reg_conf ov3647_snapshot_reg_config[] =
{
    {0x0e, 0x30},       
    {0x21, 0x35},
    {0x23, 0x0c},
    {0xd9, 0x8d},
    {0x92, 0x00},     
    {0xc9, 0x70},
};

static struct ov3647_i2c_reg_conf ov3647_preview_reg_config[] =
{
    {0x0e, 0x36},
    {0x21, 0x30},
    {0x23, 0x06},
    {0xd9, 0x8e},
    {0x92, 0x80},
    {0xc9, 0x75},
};
 
static enum ov3647_reg_update_t last_rupdate = -1;
static enum ov3647_setting_t last_rt = -1;

static struct  ov3647_work_t *ov3647_sensorw = NULL;

static struct  i2c_client *ov3647_client = NULL;
static struct ov3647_ctrl_t *ov3647_ctrl = NULL;
static DECLARE_WAIT_QUEUE_HEAD(ov3647_wait_queue);
DECLARE_MUTEX(ov3647_sem);

static int ov3647_i2c_read(unsigned short saddr,
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
    
	if (i2c_transfer(ov3647_client->adapter, msgs, 2) < 0) {
		CDBG("ov3647_i2c_read failed!\n");
		return -EIO;
	}
    
    *value = buf;
    
	return 0;

}

static int ov3647_i2c_write(unsigned short saddr,
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
    
	if (i2c_transfer(ov3647_client->adapter, msg, 1) < 0) {
		CDBG("ov3647_i2c_write faild\n");
		return -EIO;
	}

	return 0;

}

static int32_t ov3647_i2c_write_table(struct ov3647_i2c_reg_conf *reg_conf_tbl, int num_of_items_in_table)
{
    int i;
    int32_t rc = -EFAULT;

    for (i = 0; i < num_of_items_in_table; i++)
    {
        rc = ov3647_i2c_write(ov3647_client->addr,
                              reg_conf_tbl->reg, reg_conf_tbl->value);
        if (rc < 0)
        {
            break;
        }

        reg_conf_tbl++;
    }

    return rc;
}

int32_t ov3647_set_default_focus(uint8_t af_step)
{
    int32_t rc = 0;
    uint8_t code_val_msb, code_val_lsb;
    int16_t next_position;
        
    CDBG("ov3647_set_default_focus,af_step:%d\n", af_step);
    next_position = OV3647_ONE_STEP_POSITION * af_step;

    code_val_msb =
	((next_position >> 4) & 0x3f);

	code_val_lsb =
	((next_position & 0x0f) << 4);


    /* Write the digital code for current to the actuator */

    rc = ov3647_i2c_write(OV3647_AF_I2C_ADDR >> 1,
    code_val_msb, code_val_lsb);

    ov3647_ctrl->curr_lens_pos = next_position;
    ov3647_ctrl->init_curr_lens_pos = next_position;
    return rc;
}

void ov3647_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
    /* input fps is preview fps in Q8 format */
    uint32_t divider;   /*Q10 */
    uint32_t pclk_mult; /*Q10 */

    if (ov3647_ctrl->prev_res == QTR_SIZE)
    {
        divider =
            (uint32_t)(
            ((ov3647_reg_pat[RES_PREVIEW].frame_length_lines *
              ov3647_reg_pat[RES_PREVIEW].line_length_pck) *
             0x00000400) /
            (ov3647_reg_pat[RES_CAPTURE].frame_length_lines *
             ov3647_reg_pat[RES_CAPTURE].line_length_pck));

        pclk_mult =
            (uint32_t) ((ov3647_reg_pat[RES_CAPTURE].pll_multiplier *
                         0x00000400) /
                        (ov3647_reg_pat[RES_PREVIEW].pll_multiplier));
    }
    else
    {
        /* full size resolution used for preview. */
        divider   = 0x00000400;  /*1.0 */
        pclk_mult = 0x00000400;  /*1.0 */
    }

    /* Verify PCLK settings and frame sizes. */
    *pfps = (uint16_t) (fps * divider * pclk_mult /
                        0x00000400 / 0x00000400);
}

static uint16_t ov3647_get_prev_lines_pf(void)
{
    if (ov3647_ctrl->prev_res == QTR_SIZE)
    {
        return ov3647_reg_pat[RES_PREVIEW].frame_length_lines;
    }
    else
    {
        return ov3647_reg_pat[RES_CAPTURE].frame_length_lines;
    }
}

static uint16_t ov3647_get_prev_pixels_pl(void)
{
    if (ov3647_ctrl->prev_res == QTR_SIZE)
    {
        return ov3647_reg_pat[RES_PREVIEW].line_length_pck;
    }
    else
    {
        return ov3647_reg_pat[RES_CAPTURE].line_length_pck;
    }
}

static uint16_t ov3647_get_pict_lines_pf(void)
{
    return ov3647_reg_pat[RES_CAPTURE].frame_length_lines;
}

static uint16_t ov3647_get_pict_pixels_pl(void)
{
    return ov3647_reg_pat[RES_CAPTURE].line_length_pck;
}

static uint32_t ov3647_get_pict_max_exp_lc(void)
{
    uint16_t snapshot_lines_per_frame;

    if (ov3647_ctrl->pict_res == QTR_SIZE)
    {
        snapshot_lines_per_frame =
            ov3647_reg_pat[RES_PREVIEW].frame_length_lines - 1;
    }
    else
    {
        snapshot_lines_per_frame =
            ov3647_reg_pat[RES_CAPTURE].frame_length_lines - 1;
    }

    return snapshot_lines_per_frame * 24;
}

int32_t ov3647_set_fps(struct fps_cfg    *fps)
{
    /* input is new fps in Q8 format */
    int32_t rc = 0;

    CDBG("ov3647_set_fps\n");
    return rc;
}

int32_t ov3647_write_exp_gain(uint16_t gain, uint32_t line)
{
    uint32_t elc;
    uint32_t dummy = 0;
    uint32_t aec_msb;
    uint32_t aec_lsb;
    int32_t rc = 0;

    //CDBG("ov3647_write_exp_gain\n");
	if (ov3647_ctrl->sensormode ==
			SENSOR_PREVIEW_MODE) {
		ov3647_ctrl->my_reg_gain = gain;
		ov3647_ctrl->my_reg_line_count = line;
	}

    elc = (uint16_t)line;
#define OV3647_PREVIEW_MAX_LINE 784
    if (elc > OV3647_PREVIEW_MAX_LINE)
    {
        dummy = elc - OV3647_PREVIEW_MAX_LINE;
        elc = OV3647_PREVIEW_MAX_LINE;
    }

    aec_msb = (elc & 0xFF00) >> 8;
    aec_lsb = elc & 0x00FF;

    rc = ov3647_i2c_write(ov3647_client->addr, 0x2, aec_msb);

    if (rc == 0)
    {
        rc = ov3647_i2c_write(ov3647_client->addr, 0x3, aec_lsb);
    }

    if (rc == 0)
    {
        CDBG("ov3647_write_exp_gain,aec_msb=%d,aec_lsb=%d\n", aec_msb, aec_lsb);
    }

    aec_msb = (dummy & 0xFF00) >> 8;
    aec_lsb = dummy & 0x00FF;

    rc = ov3647_i2c_write(ov3647_client->addr, 0x2d, aec_msb);

    if (rc == 0)
    {
        rc = ov3647_i2c_write(ov3647_client->addr, 0x2e, aec_lsb);
    }

    if (rc == 0)
    {
        CDBG("ov3647_write_exp_gain,dummy_msb=%d,dummy_lsb=%d\n", aec_msb, aec_lsb);
    }
    
    rc = ov3647_i2c_write(ov3647_client->addr, 0x1, gain);

    if (rc == 0)
    {
        CDBG("ov3647_write_exp_gain,gain=%d\n", gain);
    }

    return rc;
}

int32_t ov3647_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
    uint32_t elc;
    uint32_t dummy = 0;
    uint32_t aec_msb;
    uint32_t aec_lsb;
    int32_t rc = 0;

    CDBG("ov3647_set_pict_exp_gain,gain=%d,line=%d\n",gain,line);
    elc = line*3 + line/2;
#define OV3647_SNAPSHOT_MAX_LINE 1544
    if (elc > OV3647_SNAPSHOT_MAX_LINE)
    {
        dummy = elc - OV3647_SNAPSHOT_MAX_LINE;
        
        elc = OV3647_SNAPSHOT_MAX_LINE;
    }

    aec_msb = (elc & 0xFF00) >> 8;
    aec_lsb = elc & 0x00FF;

    rc = ov3647_i2c_write(ov3647_client->addr, 0x2, aec_msb);

    if (rc == 0)
    {
        rc = ov3647_i2c_write(ov3647_client->addr, 0x3, aec_lsb);
    }

    if (rc == 0)
    {
        CDBG("ov3647_set_pict_exp_gain,aec_msb=%d,aec_lsb=%d\n", aec_msb, aec_lsb);
    }

    aec_msb = (dummy & 0xFF00) >> 8;
    aec_lsb = dummy & 0x00FF;

    rc = ov3647_i2c_write(ov3647_client->addr, 0x2d, aec_msb);

    if (rc == 0)
    {
        rc = ov3647_i2c_write(ov3647_client->addr, 0x2e, aec_lsb);
    }

    if (rc == 0)
    {
        CDBG("ov3647_set_pict_exp_gain,dummy_msb=%d,dummy_lsb=%d\n", aec_msb, aec_lsb);
    }

    rc = ov3647_i2c_write(ov3647_client->addr, 0x1, gain);

    if (rc == 0)
    {
        CDBG("ov3647_set_pict_exp_gain,gain=%d\n", gain);
    }

    mdelay(10);

    /* camera_timed_wait(snapshot_wait*exposure_ratio); */
    return rc;
}

int32_t ov3647_setting(enum ov3647_reg_update_t rupdate,
                       enum ov3647_setting_t    rt)
{
    int32_t rc = 0;
    if(rupdate == last_rupdate && rt == last_rt)
    {
        CDBG("ov3647_setting exit\n");
        return rc;
    }
    switch (rupdate)
    {
        case UPDATE_PERIODIC:
            rc = ov3647_i2c_write(ov3647_client->addr, 0x0c, 0x2f);
            if (rc < 0)
            {
                return rc;
            }
            if (rt == RES_PREVIEW)
            {
                rc = ov3647_i2c_write_table(ov3647_preview_reg_config,
                                            sizeof(ov3647_preview_reg_config) / sizeof(ov3647_preview_reg_config[0]));              
            }
            else
            {
                rc = ov3647_i2c_write_table(ov3647_snapshot_reg_config,
                                            sizeof(ov3647_snapshot_reg_config) / sizeof(ov3647_snapshot_reg_config[0]));
            }
            break;

        case REG_INIT:

            rc = ov3647_i2c_write_table(ov3647_init_reg_config,
                                        sizeof(ov3647_init_reg_config) / sizeof(ov3647_init_reg_config[0]));
            break;

        default:
            rc = -EFAULT;
            break;
    } /* switch (rupdate) */
    
	if (rc < 0)
		return rc;
   
    rc = ov3647_i2c_write(ov3647_client->addr, 0x24, ov3647_reg_pat[rt].x_output_size>>4);
	if (rc < 0)
		return rc;
    rc = ov3647_i2c_write(ov3647_client->addr, 0x25, ov3647_reg_pat[rt].x_output_size<<4);
	if (rc < 0)
		return rc;

    rc = ov3647_i2c_write(ov3647_client->addr, 0x26, ov3647_reg_pat[rt].y_output_size>>4);
	if (rc < 0)
		return rc;
    rc = ov3647_i2c_write(ov3647_client->addr, 0x27, ov3647_reg_pat[rt].y_output_size<<4);
	if (rc < 0)
		return rc;  
    
    rc = ov3647_i2c_write(ov3647_client->addr, 0x28, ov3647_reg_pat[rt].line_length_pck>>8);
	if (rc < 0)
		return rc;
    rc = ov3647_i2c_write(ov3647_client->addr, 0x29, ov3647_reg_pat[rt].line_length_pck);
	if (rc < 0)
		return rc;  

    rc = ov3647_i2c_write(ov3647_client->addr, 0x2a, ov3647_reg_pat[rt].frame_length_lines>>8);
	if (rc < 0)
		return rc;
    rc = ov3647_i2c_write(ov3647_client->addr, 0x2b, ov3647_reg_pat[rt].frame_length_lines);
	if (rc < 0)
		return rc;    

    rc = ov3647_i2c_write(ov3647_client->addr, 0x12, rt == RES_PREVIEW ? 0x10 : 00);
    if (rc < 0)
        return rc;	
    if(rupdate == UPDATE_PERIODIC)
    {
        rc = ov3647_i2c_write(ov3647_client->addr, 0x0c, 0x0f);
        if (rc < 0)
        {
            return rc;
        }
    }
    mdelay(5);
    if(rc == 0)
    {
        last_rupdate = rupdate;
        last_rt = rt; 
    }
    return rc;
}

int32_t ov3647_video_config(int mode, int res)
{
    int32_t rc;

    switch (res)
    {
        case QTR_SIZE:
            rc = ov3647_setting(UPDATE_PERIODIC, RES_PREVIEW);
            if (rc < 0)
            {
                return rc;
            }

            CDBG("sensor configuration done!\n");
            break;

        case FULL_SIZE:
            rc = ov3647_setting(UPDATE_PERIODIC, RES_CAPTURE);
            if (rc < 0)
            {
                return rc;
            }

            break;

        default:
            return 0;
    } /* switch */

    ov3647_ctrl->prev_res   = res;
    ov3647_ctrl->curr_res   = res;
    ov3647_ctrl->sensormode = mode;
	rc =
		ov3647_write_exp_gain(ov3647_ctrl->my_reg_gain,
			ov3647_ctrl->my_reg_line_count);

    return rc;
}

int32_t ov3647_snapshot_config(int mode)
{
    int32_t rc = 0;

    rc = ov3647_setting(UPDATE_PERIODIC, RES_CAPTURE);
    if (rc < 0)
    {
        return rc;
    }

    ov3647_ctrl->curr_res = ov3647_ctrl->pict_res;

    ov3647_ctrl->sensormode = mode;

    return rc;
}

int32_t ov3647_power_down(void)
{
    int32_t rc = 0;

    mdelay(5);

    return rc;
}

int32_t ov3647_move_focus(int direction, int32_t num_steps)
{
    int16_t step_direction;
    int16_t actual_step;
    int16_t next_position;
    uint8_t code_val_msb, code_val_lsb;

    if (num_steps > OV3647_TOTAL_STEPS_NEAR_TO_FAR)
    {
        num_steps = OV3647_TOTAL_STEPS_NEAR_TO_FAR;
    }
    else if (num_steps == 0)
    {
        return -EINVAL;
    }
    if (direction == MOVE_NEAR)
    {
        step_direction = OV3647_ONE_STEP_POSITION;
    }
    else if (direction == MOVE_FAR)
    {
        step_direction = -1 * OV3647_ONE_STEP_POSITION;
    }
    else
    {
        return -EINVAL;
    }

    if (ov3647_ctrl->curr_lens_pos < ov3647_ctrl->init_curr_lens_pos)
    {
        ov3647_ctrl->curr_lens_pos = ov3647_ctrl->init_curr_lens_pos;
    }

    actual_step = (int16_t) (step_direction * (int16_t) num_steps);



    next_position =
        (int16_t)
        (ov3647_ctrl->curr_lens_pos + actual_step);

    if (next_position > OV3647_POSITION_MAX)
    {
        next_position = OV3647_POSITION_MAX;
    }
    else if (next_position < 0)
    {
        next_position = 0;
    }
#if 0
        code_val_msb =
            ((next_position >> 4) << 2) |
            ((next_position << 4) >> 6);

        code_val_lsb =
            ((next_position & 0x03) << 6);
#else
    code_val_msb =
    ((next_position >> 4) & 0x3f);

    code_val_lsb =
    ((next_position & 0x0f) << 4);

#endif
        /* Writing the digital code for current to the actuator */
    if (ov3647_i2c_write(OV3647_AF_I2C_ADDR >> 1,
                             code_val_msb, code_val_lsb) < 0)
    {
        CDBG("ov3647_i2c_write is failed in ov3647_move_focus\n");

        return -EBUSY;
    }
    /* Storing the current lens Position */
    ov3647_ctrl->curr_lens_pos = next_position;

    mdelay(1);

    return 0;
}

static int ov3647_sensor_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);

    gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
    gpio_direction_output(data->vcm_pwd, 0);
    gpio_free(data->vcm_pwd);
   
    if (data->vreg_disable_func)
    {
        data->vreg_disable_func(data->sensor_vreg, data->vreg_num);
    }
	last_rupdate = -1;
	last_rt = -1;
	return 0;
}

static int ov3647_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;
	unsigned char chipid;

	/* pull down power down */
	rc = gpio_request(data->sensor_pwd, "ov3647");
	if (!rc || rc == -EBUSY)
		gpio_direction_output(data->sensor_pwd, 1);
	else 
        goto init_probe_fail;
    
	rc = gpio_request(data->sensor_reset, "ov3647");
	if (!rc) {
		rc = gpio_direction_output(data->sensor_reset, 0);
	}
	else
		goto init_probe_fail;

    mdelay(5);
    
    if (data->vreg_enable_func)
    {
        rc = data->vreg_enable_func(data->sensor_vreg, data->vreg_num);
        if (rc < 0)
        {
            goto init_probe_fail;
        }
    }
    
    mdelay(20);
    
    if(data->master_init_control_slave == NULL 
        || data->master_init_control_slave(data) != 0
        )
    {

        rc = gpio_direction_output(data->sensor_pwd, 0);
         if (rc < 0)
            goto init_probe_fail;

        mdelay(20);
        /*hardware reset*/
        rc = gpio_direction_output(data->sensor_reset, 1);
        if (rc < 0)
            goto init_probe_fail;

        mdelay(20);
    }
    
	/* pull down power down */
	rc = gpio_request(data->vcm_pwd, "ov3647");
    
	if (!rc || rc == -EBUSY)
		gpio_direction_output(data->vcm_pwd, 1);
	else 
        goto init_probe_fail;
    
    mdelay(2);

	/* RESET the sensor image part via I2C command */
	rc = ov3647_i2c_write(ov3647_client->addr,
		OV3647_REG_RESET_REGISTER, 0x80);
	if (rc < 0)
		goto init_probe_fail;
    mdelay(5);

	/* 3. Read sensor Model ID: */
	rc = ov3647_i2c_read(ov3647_client->addr,
		OV3647_REG_MODEL_ID, &chipid);

	if (rc < 0)
		goto init_probe_fail;

	CDBG("ov3647 model_id = 0x%x\n", chipid);

	/* 4. Compare sensor ID to OV3647 ID: */
	if (chipid != OV3647_MODEL_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}

    goto init_probe_done;

init_probe_fail:
    ov3647_sensor_init_done(data);
init_probe_done:
	return rc;
}

int ov3647_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	ov3647_ctrl = kzalloc(sizeof(struct ov3647_ctrl_t), GFP_KERNEL);
	if (!ov3647_ctrl) {
		CDBG("ov3647_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	ov3647_ctrl->fps_divider = 1 * 0x00000400;
	ov3647_ctrl->pict_fps_divider = 1 * 0x00000400;
	ov3647_ctrl->set_test = TEST_OFF;
	ov3647_ctrl->prev_res = QTR_SIZE;
	ov3647_ctrl->pict_res = FULL_SIZE;

	if (data)
		ov3647_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(OV3647_DEFAULT_CLOCK_RATE);
	mdelay(20);

	msm_camio_camif_pad_reg_reset();
	mdelay(20);

  rc = ov3647_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	if (ov3647_ctrl->prev_res == QTR_SIZE)
		rc = ov3647_setting(REG_INIT, RES_PREVIEW);
	else
		rc = ov3647_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0)
		goto init_fail;
	else
		goto init_done;
    rc = ov3647_set_default_focus(1);
    
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;    
    
init_fail:
	kfree(ov3647_ctrl);
init_done:
	return rc;
}

int ov3647_init_client(struct i2c_client *client)
{
    /* Initialize the MSM_CAMI2C Chip */
    init_waitqueue_head(&ov3647_wait_queue);
    return 0;
}

int32_t ov3647_set_sensor_mode(int mode, int res)
{
    int32_t rc = 0;

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
            CDBG("SENSOR_PREVIEW_MODE\n");
            rc = ov3647_video_config(mode, res);
            break;

        case SENSOR_SNAPSHOT_MODE:
        case SENSOR_RAW_SNAPSHOT_MODE:
            CDBG("SENSOR_SNAPSHOT_MODE\n");
            rc = ov3647_snapshot_config(mode);
            break;

        default:
            rc = -EINVAL;
            break;
    }

    return rc;
}

int ov3647_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	down(&ov3647_sem);

  CDBG("ov3647_sensor_config: cfgtype = %d\n",
	  cdata.cfgtype);
		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
				ov3647_get_pict_fps(
				cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_L_PF:
			cdata.cfg.prevl_pf =
			ov3647_get_prev_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_P_PL:
			cdata.cfg.prevp_pl =
				ov3647_get_prev_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_L_PF:
			cdata.cfg.pictl_pf =
				ov3647_get_pict_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_P_PL:
			cdata.cfg.pictp_pl =
				ov3647_get_pict_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			cdata.cfg.pict_max_exp_lc =
				ov3647_get_pict_max_exp_lc();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = ov3647_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				ov3647_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				ov3647_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = ov3647_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = ov3647_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				ov3647_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				ov3647_set_default_focus(
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_EFFECT:
			rc = ov3647_set_default_focus(
						cdata.cfg.effect);
			break;

		default:
			rc = -EFAULT;
			break;
		}

	up(&ov3647_sem);

	return rc;
}

int ov3647_sensor_release(void)
{
	int rc = -EBADF;

	down(&ov3647_sem);

	ov3647_power_down();

    ov3647_sensor_init_done(ov3647_ctrl->sensordata);

	kfree(ov3647_ctrl);

	up(&ov3647_sem);
	CDBG("ov3647_release completed!\n");
	return rc;
}

static int ov3647_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	ov3647_sensorw =
		kzalloc(sizeof(struct ov3647_work_t), GFP_KERNEL);

	if (!ov3647_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, ov3647_sensorw);
	ov3647_init_client(client);
	ov3647_client = client;
	//ov3647_client->addr = ov3647_client->addr >> 1;
	mdelay(50);

	CDBG("i2c probe ok\n");
	return 0;

probe_failure:
	kfree(ov3647_sensorw);
	ov3647_sensorw = NULL;
	pr_err("i2c probe failure %d\n", rc);
	return rc;
}

static const struct i2c_device_id ov3647_i2c_id[] = {
	{ "ov3647", 0},
	{ }
};

static struct i2c_driver ov3647_i2c_driver = {
	.id_table = ov3647_i2c_id,
	.probe  = ov3647_i2c_probe,
	.remove = __exit_p(ov3647_i2c_remove),
	.driver = {
		.name = "ov3647",
	},
};

static int ov3647_sensor_probe(
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	/* We expect this driver to match with the i2c device registered
	 * in the board file immediately. */
	int rc = i2c_add_driver(&ov3647_i2c_driver);
	if (rc < 0 || ov3647_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(OV3647_DEFAULT_CLOCK_RATE);
	mdelay(20);

	rc = ov3647_probe_init_sensor(info);
	if (rc < 0) {
		i2c_del_driver(&ov3647_i2c_driver);
		goto probe_done;
	}

    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CAMERA_MAIN);
    #endif

	s->s_init = ov3647_sensor_open_init;
	s->s_release = ov3647_sensor_release;
	s->s_config  = ov3647_sensor_config;
	ov3647_sensor_init_done(info);

probe_done:
	return rc;
}

static int __ov3647_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov3647_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov3647_probe,
	.driver = {
		.name = "msm_camera_ov3647",
		.owner = THIS_MODULE,
	},
};

static int __init ov3647_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov3647_init);
