/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora Forum nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * Alternatively, provided that this notice is retained in full, this software
 * may be relicensed by the recipient under the terms of the GNU General Public
 * License version 2 ("GPL") and only version 2, in which case the provisions of
 * the GPL apply INSTEAD OF those given above.  If the recipient relicenses the
 * software under the GPL, then the identification text in the MODULE_LICENSE
 * macro must be changed to reflect "GPLv2" instead of "Dual BSD/GPL".  Once a
 * recipient changes the license terms to the GPL, subsequent recipients shall
 * not relicense under alternate licensing terms, including the BSD or dual
 * BSD/GPL terms.  In addition, the following license statement immediately
 * below and between the words START and END shall also then apply when this
 * software is relicensed under the GPL:
 *
 * START
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 and only version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * END
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include <mach/camera.h>
#include "mt9t013.h"

#ifdef CONFIG_HUAWEI_HW_DEV_DCT
#include <linux/hw_dev_dec.h>
#endif

#ifdef CONFIG_HUAWEI_CAMERA
#undef CDBG
#define CDBG(fmt, args...) printk(KERN_INFO "mt9t013_byd.c: " fmt, ## args)
#endif

/*Micron settings from Applications for lower power consumption.*/
static struct reg_struct const mt9t013_byd_reg_pat[2] = {
	{ /* Preview 2x2 binning 20fps, pclk MHz, MCLK 24MHz */
	/* vt_pix_clk_div:REG=0x0300 update get_snapshot_fps
	* if this change */
	8,

	/* vt_sys_clk_div: REG=0x0302  update get_snapshot_fps
	* if this change */
	1,

	/* pre_pll_clk_div REG=0x0304  update get_snapshot_fps
	* if this change */
	2,

	/* pll_multiplier  REG=0x0306 60 for 30fps preview, 40
	 * for 20fps preview
	 * 46 for 30fps preview, try 47/48 to increase further */
	32,

	/* op_pix_clk_div        REG=0x0308 */
	8,

	/* op_sys_clk_div        REG=0x030A */
	1,

	/* scale_m       REG=0x0404 */
	16,

	/* row_speed     REG=0x3016 */
	0x0111,

	/* x_addr_start  REG=0x3004 */
	8,

	/* x_addr_end    REG=0x3008 */
	2053,

	/* y_addr_start  REG=0x3002 */
	8,

	/* y_addr_end    REG=0x3006 */
	1541,

	/* read_mode     REG=0x3040 */
#ifndef CONFIG_HUAWEI_CAMERA    
	0x046C,
#else
	0x046F,
#endif

	/* x_output_size REG=0x034C */
	1024,

	/* y_output_size REG=0x034E */
	768,

	/* line_length_pck    REG=0x300C */
	2616,

	/* frame_length_lines REG=0x300A */
	916,

	/* coarse_int_time REG=0x3012 */
	16,

	/* fine_int_time   REG=0x3014 */
	1461
	},
	{ /*Snapshot */
	/* vt_pix_clk_div  REG=0x0300 update get_snapshot_fps
	* if this change */
	8,

	/* vt_sys_clk_div  REG=0x0302 update get_snapshot_fps
	* if this change */
	1,

	/* pre_pll_clk_div REG=0x0304 update get_snapshot_fps
	 * if this change */
	2,

	/* pll_multiplier REG=0x0306 50 for 15fps snapshot,
	 * 40 for 10fps snapshot
	 * 46 for 30fps snapshot, try 47/48 to increase further */
	32,

	/* op_pix_clk_div        REG=0x0308 */
	8,

	/* op_sys_clk_div        REG=0x030A */
	1,

	/* scale_m       REG=0x0404 */
	16,

	/* row_speed     REG=0x3016 */
	0x0111,

	/* x_addr_start  REG=0x3004 */
	8,

	/* x_addr_end    REG=0x3008 */
	2071,

	/* y_addr_start  REG=0x3002 */
	8,

	/* y_addr_end    REG=0x3006 */
	1551,

	/* read_mode     REG=0x3040 */
#ifndef CONFIG_HUAWEI_CAMERA    
	0x0024,
#else
	0x0027,
#endif

	/* x_output_size REG=0x034C */
	2064,

	/* y_output_size REG=0x034E */
	1544,

	/* line_length_pck REG=0x300C */
	2954,

	/* frame_length_lines    REG=0x300A */
	1629,

	/* coarse_int_time REG=0x3012 */
	16,

	/* fine_int_time REG=0x3014   */
	733
	}
};

static struct mt9t013_i2c_reg_conf const mt9t013_byd_test_tbl[] = {
	{ 0x3044, 0x0544 & 0xFBFF },
	{ 0x30CA, 0x0004 | 0x0001 },
	{ 0x30D4, 0x9020 & 0x7FFF },
	{ 0x31E0, 0x0003 & 0xFFFE },
	{ 0x3180, 0x91FF & 0x7FFF },
	{ 0x301A, (0x10CC | 0x8000) & 0xFFF7 },
	{ 0x301E, 0x0000 },
	{ 0x3780, 0x0000 },
};

/* [Lens shading 85 Percent TL84] */
static struct mt9t013_i2c_reg_conf const mt9t013_byd_lc_tbl[] = {
	{ 0x360A, 0x00F0 },
	{ 0x360C, 0x9A4D },
	{ 0x360E, 0x6891 },
	{ 0x3610, 0x5F4B },
	{ 0x3612, 0x8670 },
	{ 0x364A, 0x154B },
	{ 0x364C, 0xCC2B },
	{ 0x364E, 0x030F },
	{ 0x3650, 0xC40F },
	{ 0x3652, 0xD6EF },
	{ 0x368A, 0x3331 },
	{ 0x368C, 0x8830 },
	{ 0x368E, 0x2453 },
	{ 0x3690, 0x4252 },
	{ 0x3692, 0x8E56 },
	{ 0x36CA, 0x4AEE },
	{ 0x36CC, 0xD36F },
	{ 0x36CE, 0x9A31 },
	{ 0x36D0, 0x2632 },
	{ 0x36D2, 0x0732 },
	{ 0x370A, 0x6EF0 },
	{ 0x370C, 0x4531 },
	{ 0x370E, 0xE276 },
	{ 0x3710, 0x9C34 },
	{ 0x3712, 0x1C79 },
	{ 0x3600, 0x0190 },
	{ 0x3602, 0x138E },
	{ 0x3604, 0x4E91 },
	{ 0x3606, 0x298F },
	{ 0x3608, 0x8D6E },
	{ 0x3640, 0xA748 },
	{ 0x3642, 0x07AC },
	{ 0x3644, 0xBE6E },
	{ 0x3646, 0xBE50 },
	{ 0x3648, 0x14CD },
	{ 0x3680, 0x1691 },
	{ 0x3682, 0x088F },
	{ 0x3684, 0x04D3 },
	{ 0x3686, 0xA0CF },
	{ 0x3688, 0xE515 },
	{ 0x36C0, 0x964D },
	{ 0x36C2, 0x9B10 },
	{ 0x36C4, 0x8D92 },
	{ 0x36C6, 0x4532 },
	{ 0x36C8, 0x46B4 },
	{ 0x3700, 0x8D0D },
	{ 0x3702, 0xDE6B },
	{ 0x3704, 0xAB96 },
	{ 0x3706, 0xBDD3 },
	{ 0x3708, 0x7338 },
	{ 0x3614, 0x0190 },
	{ 0x3616, 0x7C0D },
	{ 0x3618, 0x3391 },
	{ 0x361A, 0x2BCE },
	{ 0x361C, 0xF24E },
	{ 0x3654, 0x844D },
	{ 0x3656, 0xC56D },
	{ 0x3658, 0x140E },
	{ 0x365A, 0x9DED },
	{ 0x365C, 0x104E },
	{ 0x3694, 0x0831 },
	{ 0x3696, 0x6A8E },
	{ 0x3698, 0x4991 },
	{ 0x369A, 0xBE6F },
	{ 0x369C, 0x98B5 },
	{ 0x36D4, 0x2E8D },
	{ 0x36D6, 0x474B },
	{ 0x36D8, 0x8571 },
	{ 0x36DA, 0x1FD0 },
	{ 0x36DC, 0x16B2 },
	{ 0x3714, 0x476E },
	{ 0x3716, 0xA7F1 },
	{ 0x3718, 0xE855 },
	{ 0x371A, 0x7412 },
	{ 0x371C, 0x5658 },
	{ 0x361E, 0x0170 },
	{ 0x3620, 0xFAAD },
	{ 0x3622, 0x3F31 },
	{ 0x3624, 0x0ACD },
	{ 0x3626, 0xE4EF },
	{ 0x365E, 0xB3EC },
	{ 0x3660, 0x3D0D },
	{ 0x3662, 0x4A4C },
	{ 0x3664, 0xFEAF },
	{ 0x3666, 0x0AEE },
	{ 0x369E, 0x1AD1 },
	{ 0x36A0, 0x8CD0 },
	{ 0x36A2, 0x20F2 },
	{ 0x36A4, 0x3A12 },
	{ 0x36A6, 0xACF5 },
	{ 0x36DE, 0xDD4C },
	{ 0x36E0, 0x9C2F },
	{ 0x36E2, 0xBEF1 },
	{ 0x36E4, 0x0C11 },
	{ 0x36E6, 0x7313 },
	{ 0x371E, 0xB86F },
	{ 0x3720, 0x7531 },
	{ 0x3722, 0x9076 },
	{ 0x3724, 0x9DF4 },
	{ 0x3726, 0x60D8 },
	{ 0x3782, 0x0400 },
	{ 0x3784, 0x0300 },
	{ 0x3780, 0x8000 },
};

static struct mt9t013_reg mt9t013_byd_regs = {
	.reg_pat = &mt9t013_byd_reg_pat[0],
	.reg_pat_size = ARRAY_SIZE(mt9t013_byd_reg_pat),
	.ttbl = &mt9t013_byd_test_tbl[0],
	.ttbl_size = ARRAY_SIZE(mt9t013_byd_test_tbl),
	.lctbl = &mt9t013_byd_lc_tbl[0],
	.lctbl_size = ARRAY_SIZE(mt9t013_byd_lc_tbl),
	.rftbl = &mt9t013_byd_lc_tbl[0],	/* &mt9t013_byd_rolloff_tbl[0], */
	.rftbl_size = ARRAY_SIZE(mt9t013_byd_lc_tbl)
};

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define MT9T013_REG_MODEL_ID 		 0x0000
#define MT9T013_MODEL_ID     		 0x2600
#define REG_GROUPED_PARAMETER_HOLD   0x0104
#define GROUPED_PARAMETER_HOLD       0x0100
#define GROUPED_PARAMETER_UPDATE     0x0000
#define REG_COARSE_INT_TIME          0x3012
#define REG_VT_PIX_CLK_DIV           0x0300
#define REG_VT_SYS_CLK_DIV           0x0302
#define REG_PRE_PLL_CLK_DIV          0x0304
#define REG_PLL_MULTIPLIER           0x0306
#define REG_OP_PIX_CLK_DIV           0x0308
#define REG_OP_SYS_CLK_DIV           0x030A
#define REG_SCALE_M                  0x0404
#define REG_FRAME_LENGTH_LINES       0x300A
#define REG_LINE_LENGTH_PCK          0x300C
#define REG_X_ADDR_START             0x3004
#define REG_Y_ADDR_START             0x3002
#define REG_X_ADDR_END               0x3008
#define REG_Y_ADDR_END               0x3006
#define REG_X_OUTPUT_SIZE            0x034C
#define REG_Y_OUTPUT_SIZE            0x034E
#define REG_FINE_INT_TIME            0x3014
#define REG_ROW_SPEED                0x3016
#define MT9T013_REG_RESET_REGISTER   0x301A
#define MT9T013_RESET_REGISTER_PWON  0x10CC
#define MT9T013_RESET_REGISTER_PWOFF 0x1008 /* 0x10C8 stop streaming*/
#define REG_READ_MODE                0x3040
#define REG_GLOBAL_GAIN              0x305E
#define REG_TEST_PATTERN_MODE        0x3070

#define BYD_3M_MODE_ID   0xF2

enum mt9t013_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum mt9t013_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};

enum mt9t013_reg_update_t {
  /* Sensor egisters that need to be updated during initialization */
  REG_INIT,
  /* Sensor egisters that needs periodic I2C writes */
  UPDATE_PERIODIC,
  /* All the sensor Registers will be updated */
  UPDATE_ALL,
  /* Not valid update */
  UPDATE_INVALID
};

enum mt9t013_setting_t {
	RES_PREVIEW,
	RES_CAPTURE
};

/* actuator's Slave Address */
#define MT9T013_AF_I2C_ADDR   0x18

/*
* AF Total steps parameters
*/
#define MT9T013_TOTAL_STEPS_NEAR_TO_FAR    28 /* 28 */
#define MT9T013_POSITION_MAX 1023
#define MT9T013_ONE_STEP_POSITION ((MT9T013_POSITION_MAX+1)/MT9T013_TOTAL_STEPS_NEAR_TO_FAR)

/*
 * Time in milisecs for waiting for the sensor to reset.
 */
#define MT9T013_RESET_DELAY_MSECS   66

/* for 30 fps preview */
#define MT9T013_DEFAULT_CLOCK_RATE  24000000
#define MT9T013_DEFAULT_MAX_FPS     26


/* FIXME: Changes from here */
struct mt9t013_work_t {
	struct work_struct work;
};

static struct  mt9t013_work_t *mt9t013_sensorw;
static struct  i2c_client *mt9t013_client;

struct mt9t013_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;

	int sensormode;
	uint32_t fps_divider; 		/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider; 	/* init to 1 * 0x00000400 */

	uint16_t curr_lens_pos;
	uint16_t init_curr_lens_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;

	enum mt9t013_resolution_t prev_res;
	enum mt9t013_resolution_t pict_res;
	enum mt9t013_resolution_t curr_res;
	enum mt9t013_test_mode_t  set_test;

	unsigned short imgaddr;
};


static struct mt9t013_ctrl_t *mt9t013_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(mt9t013_wait_queue);
DECLARE_MUTEX(mt9t013_byd_sem);

/*=============================================================*/

static int mt9t013_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr   = saddr,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr  = saddr,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(mt9t013_client->adapter, msgs, 2) < 0) {
		CDBG("mt9t013_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9t013_i2c_read_w(unsigned short saddr,
	unsigned short raddr, unsigned short *rdata)
{
	int32_t rc = 0;
	unsigned char buf[4];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00)>>8;
	buf[1] = (raddr & 0x00FF);

	rc = mt9t013_i2c_rxdata(saddr, buf, 2);
	if (rc < 0)
		return rc;

	*rdata = buf[0] << 8 | buf[1];

	if (rc < 0)
		CDBG("mt9t013_i2c_read failed!\n");

	return rc;
}

static int32_t mt9t013_i2c_txdata(unsigned short saddr,
	unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
	{
		.addr = saddr,
		.flags = 0,
		.len = length,
		.buf = txdata,
	},
	};

	if (i2c_transfer(mt9t013_client->adapter, msg, 1) < 0) {
		CDBG("mt9t013_i2c_txdata faild\n");
		return -EIO;
	}

	return 0;
}

static int32_t mt9t013_i2c_write_b(unsigned short saddr,
	unsigned short waddr, unsigned short wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[2];

	memset(buf, 0, sizeof(buf));
	buf[0] = waddr;
	buf[1] = wdata;
	rc = mt9t013_i2c_txdata(saddr, buf, 2);

	if (rc < 0)
		CDBG("i2c_write failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t mt9t013_i2c_write_w(unsigned short saddr,
	unsigned short waddr, unsigned short wdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[4];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00)>>8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = (wdata & 0xFF00)>>8;
	buf[3] = (wdata & 0x00FF);

	rc = mt9t013_i2c_txdata(saddr, buf, 4);

	if (rc < 0)
		CDBG("i2c_write_w failed, addr = 0x%x, val = 0x%x!\n",
		waddr, wdata);

	return rc;
}

static int32_t mt9t013_i2c_write_w_table(
	struct mt9t013_i2c_reg_conf const *reg_conf_tbl,
	int num_of_items_in_table)
{
	int i;
	int32_t rc = -EFAULT;

	for (i = 0; i < num_of_items_in_table; i++) {
		rc = mt9t013_i2c_write_w(mt9t013_client->addr,
			reg_conf_tbl->waddr, reg_conf_tbl->wdata);
		if (rc < 0)
			break;
		reg_conf_tbl++;
	}

	return rc;
}

static int32_t mt9t013_test(enum mt9t013_test_mode_t mo)
{
	int32_t rc = 0;

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_HOLD);
	if (rc < 0)
		return rc;

	if (mo == TEST_OFF)
		return 0;
	else {
		rc = mt9t013_i2c_write_w_table(mt9t013_regs.ttbl,
			mt9t013_regs.ttbl_size);
		if (rc < 0)
			return rc;

		rc = mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_TEST_PATTERN_MODE, (uint16_t)mo);
		if (rc < 0)
			return rc;
	}

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_UPDATE);
	if (rc < 0)
		return rc;

	return rc;
}

static int32_t mt9t013_set_lc(void)
{
	int32_t rc;

	/*	rc = mt9t013_i2c_write_w_table(mt9t013_regs.lctbl,
	 *	ARRAY_SIZE(mt9t013_regs.lctbl)); */
	rc = mt9t013_i2c_write_w_table(mt9t013_regs.lctbl,
		mt9t013_regs.lctbl_size);
	if (rc < 0)
		return rc;

	return rc;
}

static int32_t mt9t013_set_default_focus(uint8_t af_step)
{
#ifndef CONFIG_HUAWEI_CAMERA    
	int32_t rc = 0;
	uint8_t code_val_msb, code_val_lsb;
	code_val_msb = 0x01;
	code_val_lsb = af_step;

	/* Write the digital code for current to the actuator */
	rc =
		mt9t013_i2c_write_b(MT9T013_AF_I2C_ADDR>>1,
			code_val_msb, code_val_lsb);

	mt9t013_ctrl->curr_lens_pos = 0;
	mt9t013_ctrl->init_curr_lens_pos = 0;
	return rc;
#else

    int32_t rc = 0;
    uint8_t code_val_msb, code_val_lsb;
	int16_t next_position;

    CDBG("mt9t013_set_default_focus,af_step:%d\n", af_step);
    next_position = MT9T013_ONE_STEP_POSITION * af_step;
    code_val_msb =
	((next_position >> 4) & 0x3f);

	code_val_lsb =
	((next_position & 0x0f) << 4);


    /* Write the digital code for current to the actuator */

    rc = mt9t013_i2c_write_b(MT9T013_AF_I2C_ADDR >> 1,
    code_val_msb, code_val_lsb);

    mt9t013_ctrl->curr_lens_pos = next_position;
    mt9t013_ctrl->init_curr_lens_pos = next_position;
    return rc;

#endif
}

static void mt9t013_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
	/* input fps is preview fps in Q8 format */
	uint32_t divider;   /*Q10 */
	uint32_t pclk_mult; /*Q10 */

	if (mt9t013_ctrl->prev_res == QTR_SIZE) {
		divider =
			(uint32_t)(
		((mt9t013_regs.reg_pat[RES_PREVIEW].frame_length_lines *
		mt9t013_regs.reg_pat[RES_PREVIEW].line_length_pck) *
		0x00000400) /
		(mt9t013_regs.reg_pat[RES_CAPTURE].frame_length_lines *
		mt9t013_regs.reg_pat[RES_CAPTURE].line_length_pck));

		pclk_mult =
		(uint32_t) ((mt9t013_regs.reg_pat[RES_CAPTURE].pll_multiplier *
		0x00000400) /
		(mt9t013_regs.reg_pat[RES_PREVIEW].pll_multiplier));

	} else {
		/* full size resolution used for preview. */
		divider   = 0x00000400;  /*1.0 */
		pclk_mult = 0x00000400;  /*1.0 */
	}

	/* Verify PCLK settings and frame sizes. */
	*pfps =
		(uint16_t) (fps * divider * pclk_mult /
		0x00000400 / 0x00000400);
}

static uint16_t mt9t013_get_prev_lines_pf(void)
{
	if (mt9t013_ctrl->prev_res == QTR_SIZE)
		return mt9t013_regs.reg_pat[RES_PREVIEW].frame_length_lines;
	else
		return mt9t013_regs.reg_pat[RES_CAPTURE].frame_length_lines;
}

static uint16_t mt9t013_get_prev_pixels_pl(void)
{
	if (mt9t013_ctrl->prev_res == QTR_SIZE)
		return mt9t013_regs.reg_pat[RES_PREVIEW].line_length_pck;
	else
		return mt9t013_regs.reg_pat[RES_CAPTURE].line_length_pck;
}

static uint16_t mt9t013_get_pict_lines_pf(void)
{
	return mt9t013_regs.reg_pat[RES_CAPTURE].frame_length_lines;
}

static uint16_t mt9t013_get_pict_pixels_pl(void)
{
	return mt9t013_regs.reg_pat[RES_CAPTURE].line_length_pck;
}

static uint32_t mt9t013_get_pict_max_exp_lc(void)
{
	uint16_t snapshot_lines_per_frame;

	if (mt9t013_ctrl->pict_res == QTR_SIZE) {
		snapshot_lines_per_frame =
		mt9t013_regs.reg_pat[RES_PREVIEW].frame_length_lines - 1;
	} else  {
		snapshot_lines_per_frame =
		mt9t013_regs.reg_pat[RES_CAPTURE].frame_length_lines - 1;
	}

	return snapshot_lines_per_frame * 24;
}

static int32_t mt9t013_set_fps(struct fps_cfg	*fps)
{
	/* input is new fps in Q8 format */
	int32_t rc = 0;

	mt9t013_ctrl->fps_divider = fps->fps_div;
	mt9t013_ctrl->pict_fps_divider = fps->pict_fps_div;

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_HOLD);
	if (rc < 0)
		return -EBUSY;

	CDBG("mt9t013_set_fps: fps_div is %d, frame_rate is %d\n",
		fps->fps_div,
		(uint16_t) (
		mt9t013_regs.reg_pat[RES_PREVIEW].frame_length_lines *
		fps->fps_div/0x00000400));

	CDBG("mt9t013_set_fps: fps_mult is %d, frame_rate is %d\n",
		fps->f_mult,
		(uint16_t) (
		mt9t013_regs.reg_pat[RES_PREVIEW].line_length_pck *
		fps->f_mult / 0x00000400));

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_LINE_LENGTH_PCK,
			(uint16_t) (
			mt9t013_regs.reg_pat[RES_PREVIEW].line_length_pck *
			fps->f_mult / 0x00000400));
	if (rc < 0)
		return rc;

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_UPDATE);
	if (rc < 0)
		return rc;

	return rc;
}

static int32_t mt9t013_write_exp_gain(uint16_t gain, uint32_t line)
{
	uint16_t max_legal_gain = 0x01FF;
	enum mt9t013_setting_t setting;
	int32_t rc = 0;

	if (mt9t013_ctrl->sensormode ==
			SENSOR_PREVIEW_MODE) {
		mt9t013_ctrl->my_reg_gain = gain;
		mt9t013_ctrl->my_reg_line_count = (uint16_t) line;
	}

	if (gain > 0x00000400)
		gain = max_legal_gain;

	/* Verify no overflow */
	if (mt9t013_ctrl->sensormode != SENSOR_SNAPSHOT_MODE) {
		line =
			(uint32_t) (line * mt9t013_ctrl->fps_divider /
			0x00000400);

		setting = RES_PREVIEW;

	} else {
		line =
			(uint32_t) (line * mt9t013_ctrl->pict_fps_divider /
			0x00000400);

		setting = RES_CAPTURE;
	}

	/*Set digital gain to 1 */
	gain |= 0x0200;


	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GLOBAL_GAIN, gain);
	if (rc < 0)
		return rc;

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_COARSE_INT_TIME,
			line );
	if (rc < 0)
		return rc;

	return rc;
}

static int32_t mt9t013_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;

	rc =
		mt9t013_write_exp_gain(gain, line);
	if (rc < 0)
		return rc;

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			MT9T013_REG_RESET_REGISTER,
			0x10CC | 0x0002);

	mdelay(5);

	/* camera_timed_wait(snapshot_wait*exposure_ratio); */
	return rc;
}

static int32_t mt9t013_setting(enum mt9t013_reg_update_t rupdate,
	enum mt9t013_setting_t rt)
{
	int32_t rc = 0;

	switch (rupdate) {
	case UPDATE_PERIODIC: {

	if (rt == RES_PREVIEW ||
			rt == RES_CAPTURE) {
#if 0
		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				MT9T013_REG_RESET_REGISTER,
				MT9T013_RESET_REGISTER_PWOFF);
		if (rc < 0)
			return rc;
#endif

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_VT_PIX_CLK_DIV,
				mt9t013_regs.reg_pat[rt].vt_pix_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_VT_SYS_CLK_DIV,
				mt9t013_regs.reg_pat[rt].vt_sys_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_PRE_PLL_CLK_DIV,
				mt9t013_regs.reg_pat[rt].pre_pll_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_PLL_MULTIPLIER,
				mt9t013_regs.reg_pat[rt].pll_multiplier);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_OP_PIX_CLK_DIV,
				mt9t013_regs.reg_pat[rt].op_pix_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_OP_SYS_CLK_DIV,
				mt9t013_regs.reg_pat[rt].op_sys_clk_div);
		if (rc < 0)
			return rc;

		mdelay(5);

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_GROUPED_PARAMETER_HOLD,
				GROUPED_PARAMETER_HOLD);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_ROW_SPEED,
				mt9t013_regs.reg_pat[rt].row_speed);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_X_ADDR_START,
				mt9t013_regs.reg_pat[rt].x_addr_start);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_X_ADDR_END,
				mt9t013_regs.reg_pat[rt].x_addr_end);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_Y_ADDR_START,
				mt9t013_regs.reg_pat[rt].y_addr_start);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_Y_ADDR_END,
				mt9t013_regs.reg_pat[rt].y_addr_end);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_READ_MODE,
				mt9t013_regs.reg_pat[rt].read_mode);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_SCALE_M,
				mt9t013_regs.reg_pat[rt].scale_m);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_X_OUTPUT_SIZE,
				mt9t013_regs.reg_pat[rt].x_output_size);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_Y_OUTPUT_SIZE,
				mt9t013_regs.reg_pat[rt].y_output_size);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_LINE_LENGTH_PCK,
				mt9t013_regs.reg_pat[rt].line_length_pck);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_FRAME_LENGTH_LINES,
			(mt9t013_regs.reg_pat[rt].frame_length_lines *
			mt9t013_ctrl->fps_divider / 0x00000400));
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_COARSE_INT_TIME,
			mt9t013_regs.reg_pat[rt].coarse_int_time);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_FINE_INT_TIME,
			mt9t013_regs.reg_pat[rt].fine_int_time);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_UPDATE);
		if (rc < 0)
			return rc;

		rc = mt9t013_test(mt9t013_ctrl->set_test);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
			MT9T013_REG_RESET_REGISTER,
			MT9T013_RESET_REGISTER_PWON);
		if (rc < 0)
			return rc;

		mdelay(5);

		return rc;
	}
	}
		break;

	/*CAMSENSOR_REG_UPDATE_PERIODIC */
	case REG_INIT: {
	if (rt == RES_PREVIEW ||
			rt == RES_CAPTURE) {

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				MT9T013_REG_RESET_REGISTER,
				MT9T013_RESET_REGISTER_PWOFF);
		if (rc < 0)
			/* MODE_SELECT, stop streaming */
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_VT_PIX_CLK_DIV,
				mt9t013_regs.reg_pat[rt].vt_pix_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_VT_SYS_CLK_DIV,
				mt9t013_regs.reg_pat[rt].vt_sys_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_PRE_PLL_CLK_DIV,
				mt9t013_regs.reg_pat[rt].pre_pll_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_PLL_MULTIPLIER,
				mt9t013_regs.reg_pat[rt].pll_multiplier);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_OP_PIX_CLK_DIV,
				mt9t013_regs.reg_pat[rt].op_pix_clk_div);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_OP_SYS_CLK_DIV,
				mt9t013_regs.reg_pat[rt].op_sys_clk_div);
		if (rc < 0)
			return rc;

		mdelay(5);

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_GROUPED_PARAMETER_HOLD,
				GROUPED_PARAMETER_HOLD);
		if (rc < 0)
			return rc;

		/* additional power saving mode ok around 38.2MHz */
		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				0x3084, 0x2409);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				0x3092, 0x0A49);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				0x3094, 0x4949);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				0x3096, 0x4949);
		if (rc < 0)
			return rc;

		/* Set preview or snapshot mode */
		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_ROW_SPEED,
				mt9t013_regs.reg_pat[rt].row_speed);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_X_ADDR_START,
				mt9t013_regs.reg_pat[rt].x_addr_start);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_X_ADDR_END,
				mt9t013_regs.reg_pat[rt].x_addr_end);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_Y_ADDR_START,
				mt9t013_regs.reg_pat[rt].y_addr_start);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_Y_ADDR_END,
				mt9t013_regs.reg_pat[rt].y_addr_end);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_READ_MODE,
				mt9t013_regs.reg_pat[rt].read_mode);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_SCALE_M,
				mt9t013_regs.reg_pat[rt].scale_m);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_X_OUTPUT_SIZE,
				mt9t013_regs.reg_pat[rt].x_output_size);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_Y_OUTPUT_SIZE,
				mt9t013_regs.reg_pat[rt].y_output_size);
		if (rc < 0)
			return 0;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_LINE_LENGTH_PCK,
				mt9t013_regs.reg_pat[rt].line_length_pck);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_FRAME_LENGTH_LINES,
				mt9t013_regs.reg_pat[rt].frame_length_lines);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_COARSE_INT_TIME,
				mt9t013_regs.reg_pat[rt].coarse_int_time);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_FINE_INT_TIME,
				mt9t013_regs.reg_pat[rt].fine_int_time);
		if (rc < 0)
			return rc;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_GROUPED_PARAMETER_HOLD,
				GROUPED_PARAMETER_UPDATE);
			if (rc < 0)
				return rc;

		/* load lens shading */
		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_GROUPED_PARAMETER_HOLD,
				GROUPED_PARAMETER_HOLD);
		if (rc < 0)
			return rc;

		/* most likely needs to be written only once. */
		rc = mt9t013_set_lc();
		if (rc < 0)
			return -EBUSY;

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				REG_GROUPED_PARAMETER_HOLD,
				GROUPED_PARAMETER_UPDATE);
		if (rc < 0)
			return rc;

		rc = mt9t013_test(mt9t013_ctrl->set_test);
		if (rc < 0)
			return rc;

		mdelay(5);

		rc =
			mt9t013_i2c_write_w(mt9t013_client->addr,
				MT9T013_REG_RESET_REGISTER,
				MT9T013_RESET_REGISTER_PWON);
		if (rc < 0)
			/* MODE_SELECT, stop streaming */
			return rc;

		CDBG("!!! mt9t013 !!! PowerOn is done!\n");
		mdelay(5);
		return rc;
		}
		} /* case CAMSENSOR_REG_INIT: */
		break;

	/*CAMSENSOR_REG_INIT */
	default:
		rc = -EFAULT;
		break;
	} /* switch (rupdate) */

	return rc;
}

static int32_t mt9t013_video_config(int mode, int res)
{
	int32_t rc;

	switch (res) {
	case QTR_SIZE:
		rc = mt9t013_setting(UPDATE_PERIODIC, RES_PREVIEW);
		if (rc < 0)
			return rc;

			CDBG("sensor configuration done!\n");
		break;

	case FULL_SIZE:
		rc = mt9t013_setting(UPDATE_PERIODIC, RES_CAPTURE);
		if (rc < 0)
			return rc;
		break;

	default:
		return 0;
	} /* switch */

	mt9t013_ctrl->prev_res = res;
	mt9t013_ctrl->curr_res = res;
	mt9t013_ctrl->sensormode = mode;

	rc =
		mt9t013_write_exp_gain(mt9t013_ctrl->my_reg_gain,
			mt9t013_ctrl->my_reg_line_count);

	return rc;
}

static int32_t mt9t013_snapshot_config(int mode)
{
	int32_t rc = 0;

	rc =
		mt9t013_setting(UPDATE_PERIODIC, RES_CAPTURE);
	if (rc < 0)
		return rc;

	mt9t013_ctrl->curr_res = mt9t013_ctrl->pict_res;

	mt9t013_ctrl->sensormode = mode;

	return rc;
}

static int32_t mt9t013_raw_snapshot_config(int mode)
{
	int32_t rc = 0;

	rc = mt9t013_setting(UPDATE_PERIODIC, RES_CAPTURE);
	if (rc < 0)
		return rc;

	mt9t013_ctrl->curr_res = mt9t013_ctrl->pict_res;

	mt9t013_ctrl->sensormode = mode;

	return rc;
}

static int32_t mt9t013_power_down(void)
{
	int32_t rc = 0;

	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			MT9T013_REG_RESET_REGISTER,
			MT9T013_RESET_REGISTER_PWOFF);

	mdelay(5);

	return rc;
}

static int32_t mt9t013_move_focus(int direction,
	int32_t num_steps)
{
#ifndef CONFIG_HUAWEI_CAMERA
	int16_t step_direction;
	int16_t actual_step;
	int16_t next_position;
	int16_t break_steps[4];
	uint8_t code_val_msb, code_val_lsb;
	int16_t i;

	if (num_steps > MT9T013_TOTAL_STEPS_NEAR_TO_FAR)
		num_steps = MT9T013_TOTAL_STEPS_NEAR_TO_FAR;
	else if (num_steps == 0)
		return -EINVAL;

	if (direction == MOVE_NEAR)
		step_direction = 4;
	else if (direction == MOVE_FAR)
		step_direction = -4;
	else
		return -EINVAL;

	if (mt9t013_ctrl->curr_lens_pos < mt9t013_ctrl->init_curr_lens_pos)
		mt9t013_ctrl->curr_lens_pos = mt9t013_ctrl->init_curr_lens_pos;

	actual_step =
		(int16_t) (step_direction *
		(int16_t) num_steps);

	for (i = 0; i < 4; i++)
		break_steps[i] =
			actual_step / 4 * (i + 1) - actual_step / 4 * i;

	for (i = 0; i < 4; i++) {
		next_position =
		(int16_t)
		(mt9t013_ctrl->curr_lens_pos + break_steps[i]);

		if (next_position > 255)
			next_position = 255;
		else if (next_position < 0)
			next_position = 0;

		code_val_msb =
		((next_position >> 4) << 2) |
		((next_position << 4) >> 6);

		code_val_lsb =
		((next_position & 0x03) << 6);

		/* Writing the digital code for current to the actuator */
		if (mt9t013_i2c_write_b(MT9T013_AF_I2C_ADDR>>1,
				code_val_msb, code_val_lsb) < 0)
			return -EBUSY;

		/* Storing the current lens Position */
		mt9t013_ctrl->curr_lens_pos = next_position;

		if (i < 3)
			mdelay(1);
	} /* for */

	return 0;
#else
	int16_t step_direction;
	int16_t actual_step;
	int16_t next_position;
	uint8_t code_val_msb, code_val_lsb;

        
	if (num_steps > MT9T013_TOTAL_STEPS_NEAR_TO_FAR)
		num_steps = MT9T013_TOTAL_STEPS_NEAR_TO_FAR;
	else if (num_steps == 0)
		return -EINVAL;
	if (direction == MOVE_NEAR)
		step_direction = MT9T013_ONE_STEP_POSITION;
	else if (direction == MOVE_FAR)
		step_direction = -1 * MT9T013_ONE_STEP_POSITION;
	else
		return -EINVAL;

	if (mt9t013_ctrl->curr_lens_pos < mt9t013_ctrl->init_curr_lens_pos)
		mt9t013_ctrl->curr_lens_pos = mt9t013_ctrl->init_curr_lens_pos;
    
	actual_step = (int16_t)(step_direction * (int16_t)num_steps);
	next_position = (int16_t)(mt9t013_ctrl->curr_lens_pos + actual_step);

        if (next_position > MT9T013_POSITION_MAX)
        {
            next_position = MT9T013_POSITION_MAX;
        }
        else if (next_position < 0)
        {
            next_position = 0;
        }

        code_val_msb =
		((next_position >> 4) & 0x3f);

		code_val_lsb =
		((next_position & 0x0f) << 4);

		/* Writing the digital code for current to the actuator */
		if (mt9t013_i2c_write_b(MT9T013_AF_I2C_ADDR>>1,
				code_val_msb, code_val_lsb) < 0)
		{
            CDBG("mt9t013_i2c_write_b is failed in mt9t013_move_focus\n");
			return -EBUSY;
		}

		/* Storing the current lens Position */
		mt9t013_ctrl->curr_lens_pos = next_position;

			mdelay(1);

	return 0;
#endif	
}

static int mt9t013_sensor_init_done(const struct msm_camera_sensor_info *data)
{
	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
#ifdef CONFIG_HUAWEI_CAMERA    
    gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
    gpio_direction_output(data->vcm_pwd, 0);
    gpio_free(data->vcm_pwd);
   
    if (data->vreg_disable_func)
    {
        data->vreg_disable_func(data->sensor_vreg, data->vreg_num);
    }
#endif
	return 0;
}

static int mt9t013_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int rc;
	uint16_t chipid;
        uint16_t mode_id;
#ifndef CONFIG_HUAWEI_CAMERA    

	rc = gpio_request(data->sensor_reset, "mt9t013");
	if (!rc)
		gpio_direction_output(data->sensor_reset, 1);
	else
		goto init_probe_done;

	msleep(20);
#else

	/* pull down power down */
	rc = gpio_request(data->sensor_pwd, "mt9t013");
	if (!rc || rc == -EBUSY)
		gpio_direction_output(data->sensor_pwd, 1);
	else 
        goto init_probe_fail;
    
	rc = gpio_request(data->sensor_reset, "mt9t013");
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
	rc = gpio_request(data->vcm_pwd, "mt9t013");
	if (!rc || rc == -EBUSY)
		gpio_direction_output(data->vcm_pwd, 1);
	else 
        goto init_probe_fail;
    
    mdelay(2);
    
#endif
	/* RESET the sensor image part via I2C command */
	rc = mt9t013_i2c_write_w(mt9t013_client->addr,
		MT9T013_REG_RESET_REGISTER, 0x1009);
	if (rc < 0)
		goto init_probe_fail;

	msleep(10);

	/* 3. Read sensor Model ID: */
	rc = mt9t013_i2c_read_w(mt9t013_client->addr,
		MT9T013_REG_MODEL_ID, &chipid);

	if (rc < 0)
		goto init_probe_fail;

	CDBG("mt9t013_byd chipid = 0x%x\n", chipid);

	/* 4. Compare sensor ID to MT9T012VC ID: */
	if (chipid != MT9T013_MODEL_ID) {
		rc = -ENODEV;
		goto init_probe_fail;
	}

	rc = mt9t013_i2c_write_w(mt9t013_client->addr,
		0x3064, 0x0805);
	if (rc < 0)
		goto init_probe_fail;

	mdelay(MT9T013_RESET_DELAY_MSECS);

  /*在读取GPIO状态之前，0x301A寄存器的bit8必须先写1，然后才能正确读取*/
	rc = mt9t013_i2c_write_w(mt9t013_client->addr,
		MT9T013_REG_RESET_REGISTER, MT9T013_RESET_REGISTER_PWON | 0x0100);
	if (rc < 0)
		goto init_probe_fail;
  /*读取GPIO状态寄存器，低四位标示GPIO状态，高12位默认值为0xFFF，
    为了便于和SOC2030区别，3M摄像头提取低八位来识别不同的模组*/  
	rc = mt9t013_i2c_read_w(mt9t013_client->addr,
		0x3026, &mode_id);
    CDBG("mt9t013_byd model_id = 0x%x\n", mode_id);
	if (rc < 0)
		goto init_probe_fail;
    /*提取标识位*/
    mode_id = mode_id & 0xff;

    if(BYD_3M_MODE_ID != mode_id)
    {
        rc = -ENODEV;
		goto init_probe_fail;
    }
    
  goto init_probe_done;

	/* sensor: output enable */
#if 0
	rc = mt9t013_i2c_write_w(mt9t013_client->addr,
		MT9T013_REG_RESET_REGISTER,
		MT9T013_RESET_REGISTER_PWON);

	/* if this fails, the sensor is not the MT9T013 */
	rc = mt9t013_set_default_focus(0);
#endif

init_probe_fail:
#ifndef CONFIG_HUAWEI_CAMERA
	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
#else
    mt9t013_sensor_init_done(data);
#endif
init_probe_done:
	return rc;
}

static int mt9t013_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int32_t  rc;

	mt9t013_ctrl = kzalloc(sizeof(struct mt9t013_ctrl_t), GFP_KERNEL);
	if (!mt9t013_ctrl) {
		CDBG("mt9t013_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}

	mt9t013_ctrl->fps_divider = 1 * 0x00000400;
	mt9t013_ctrl->pict_fps_divider = 1 * 0x00000400;
	mt9t013_ctrl->set_test = TEST_OFF;
	mt9t013_ctrl->prev_res = QTR_SIZE;
	mt9t013_ctrl->pict_res = FULL_SIZE;

	if (data)
		mt9t013_ctrl->sensordata = data;

	/* enable mclk first */
	msm_camio_clk_rate_set(MT9T013_DEFAULT_CLOCK_RATE);
	mdelay(20);

	msm_camio_camif_pad_reg_reset();
	mdelay(20);

  rc = mt9t013_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;

	if (mt9t013_ctrl->prev_res == QTR_SIZE)
		rc = mt9t013_setting(REG_INIT, RES_PREVIEW);
	else
		rc = mt9t013_setting(REG_INIT, RES_CAPTURE);

	if (rc < 0)
		goto init_fail;
	else
		goto init_done;
#ifdef CONFIG_HUAWEI_CAMERA
    rc = mt9t013_set_default_focus(1);
    
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;  
#endif		
init_fail:
	kfree(mt9t013_ctrl);
init_done:
	return rc;
}

static int mt9t013_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&mt9t013_wait_queue);
	return 0;
}


static int32_t mt9t013_set_sensor_mode(int mode, int res)
{
	int32_t rc = 0;
	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_HOLD);
	if (rc < 0)
		return rc;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = mt9t013_video_config(mode, res);
		break;

	case SENSOR_SNAPSHOT_MODE:
		rc = mt9t013_snapshot_config(mode);
		break;

	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = mt9t013_raw_snapshot_config(mode);
		break;
	default:
		rc = -EINVAL;
		break;
	}
	rc =
		mt9t013_i2c_write_w(mt9t013_client->addr,
			REG_GROUPED_PARAMETER_HOLD,
			GROUPED_PARAMETER_UPDATE);
	return rc;
}

static int mt9t013_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

	down(&mt9t013_byd_sem);

		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
				mt9t013_get_pict_fps(
				cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_L_PF:
			cdata.cfg.prevl_pf =
			mt9t013_get_prev_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PREV_P_PL:
			cdata.cfg.prevp_pl =
				mt9t013_get_prev_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_L_PF:
			cdata.cfg.pictl_pf =
				mt9t013_get_pict_lines_pf();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_P_PL:
			cdata.cfg.pictp_pl =
				mt9t013_get_pict_pixels_pl();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_GET_PICT_MAX_EXP_LC:
			cdata.cfg.pict_max_exp_lc =
				mt9t013_get_pict_max_exp_lc();

			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;

		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = mt9t013_set_fps(&(cdata.cfg.fps));
			break;

		case CFG_SET_EXP_GAIN:
			rc =
				mt9t013_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_PICT_EXP_GAIN:
			rc =
				mt9t013_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;

		case CFG_SET_MODE:
			rc = mt9t013_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;

		case CFG_PWR_DOWN:
			rc = mt9t013_power_down();
			break;

		case CFG_MOVE_FOCUS:
			rc =
				mt9t013_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_DEFAULT_FOCUS:
			rc =
				mt9t013_set_default_focus(
					cdata.cfg.focus.steps);
			break;

		case CFG_SET_EFFECT:
			rc = mt9t013_set_default_focus(
						cdata.cfg.effect);
			break;

		default:
			rc = -EFAULT;
			break;
		}

	up(&mt9t013_byd_sem);

	return rc;
}
int mt9t013_byd_sensor_release(void)
{
	int rc = -EBADF;

	down(&mt9t013_byd_sem);

	mt9t013_power_down();
#ifndef CONFIG_HUAWEI_CAMERA
	gpio_direction_output(mt9t013_ctrl->sensordata->sensor_reset,
			0);
	gpio_free(mt9t013_ctrl->sensordata->sensor_reset);
#else
    mt9t013_sensor_init_done(mt9t013_ctrl->sensordata);
#endif
	kfree(mt9t013_ctrl);

	up(&mt9t013_byd_sem);
	CDBG("mt9t013_release completed!\n");
	return rc;
}

static int mt9t013_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	mt9t013_sensorw =
		kzalloc(sizeof(struct mt9t013_work_t), GFP_KERNEL);

	if (!mt9t013_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, mt9t013_sensorw);
	mt9t013_init_client(client);
	mt9t013_client = client;
	mt9t013_client->addr = mt9t013_client->addr >> 1;
	mdelay(50);

	CDBG("i2c probe ok\n");
	return 0;

probe_failure:
	kfree(mt9t013_sensorw);
	mt9t013_sensorw = NULL;
	pr_err("i2c probe failure %d\n", rc);
	return rc;
}

static const struct i2c_device_id mt9t013_i2c_id[] = {
	{ "mt9t013_byd", 0},
	{ }
};

static struct i2c_driver mt9t013_i2c_driver = {
	.id_table = mt9t013_i2c_id,
	.probe  = mt9t013_i2c_probe,
	.remove = __exit_p(mt9t013_i2c_remove),
	.driver = {
		.name = "mt9t013_byd",
	},
};

static int mt9t013_sensor_probe(
		const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	/* We expect this driver to match with the i2c device registered
	 * in the board file immediately. */
	int rc = i2c_add_driver(&mt9t013_i2c_driver);
	if (rc < 0 || mt9t013_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

	/* enable mclk first */
	msm_camio_clk_rate_set(MT9T013_DEFAULT_CLOCK_RATE);
	mdelay(20);

	rc = mt9t013_probe_init_sensor(info);
	if (rc < 0) {
		i2c_del_driver(&mt9t013_i2c_driver);
		goto probe_done;
	}
	
    #ifdef CONFIG_HUAWEI_HW_DEV_DCT
    /* detect current device successful, set the flag as present */
    set_hw_dev_flag(DEV_I2C_CAMERA_MAIN);
    #endif
        
        mt9t013_regs = mt9t013_byd_regs;
        
	s->s_init = mt9t013_sensor_open_init;
	s->s_release = mt9t013_byd_sensor_release;
	s->s_config  = mt9t013_sensor_config;
	mt9t013_sensor_init_done(info);

probe_done:
	return rc;
}

static int __mt9t013_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, mt9t013_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __mt9t013_probe,
	.driver = {
		.name = "msm_camera_byd3m",
		.owner = THIS_MODULE,
	},
};

static int __init mt9t013_byd_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(mt9t013_byd_init);
