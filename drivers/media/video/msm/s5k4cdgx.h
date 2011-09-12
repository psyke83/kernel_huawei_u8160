

#ifndef S5K4CDGX_H
#define S5K4CDGX_H

#include <mach/board.h>

enum s5k4cdgx_test_mode_t
{
    TEST_OFF,
    TEST_1,
    TEST_2,
    TEST_3
};

enum s5k4cdgx_resolution_t
{
    QTR_SIZE,
    FULL_SIZE,
    INVALID_SIZE
};

enum s5k4cdgx_reg_update_t
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

enum s5k4cdgx_setting_t
{
    RES_PREVIEW,
    RES_CAPTURE
};


/* FIXME: Changes from here */
struct s5k4cdgx_work_t
{
    struct work_struct work;
};

struct s5k4cdgx_ctrl_t
{
    const struct  msm_camera_sensor_info *sensordata;

    int sensormode;
    uint32_t           fps_divider; /* init to 1 * 0x00000400 */
    uint32_t           pict_fps_divider; /* init to 1 * 0x00000400 */

    uint16_t curr_lens_pos;
    uint16_t init_curr_lens_pos;
    uint16_t my_reg_gain;
    uint32_t my_reg_line_count;

    enum s5k4cdgx_resolution_t prev_res;
    enum s5k4cdgx_resolution_t pict_res;
    enum s5k4cdgx_resolution_t curr_res;
    enum s5k4cdgx_test_mode_t  set_test;

    unsigned short imgaddr;
};

struct s5k4cdgx_i2c_reg_conf
{
	unsigned short waddr;
	unsigned short wdata;
};

typedef enum
{
  CAMERA_WB_MIN_MINUS_1,
  CAMERA_WB_AUTO = 1,  /* This list must match aeecamera.h */
  CAMERA_WB_CUSTOM,
  CAMERA_WB_INCANDESCENT,
  CAMERA_WB_FLUORESCENT,
  CAMERA_WB_DAYLIGHT,
  CAMERA_WB_CLOUDY_DAYLIGHT,
  CAMERA_WB_TWILIGHT,
  CAMERA_WB_SHADE,
  CAMERA_WB_MAX_PLUS_1
} config3a_wb_t;

typedef enum
{
  CAMERA_ANTIBANDING_OFF,
  CAMERA_ANTIBANDING_60HZ,
  CAMERA_ANTIBANDING_50HZ,
  CAMERA_ANTIBANDING_AUTO,
  CAMERA_MAX_ANTIBANDING,
} camera_antibanding_type;

#endif /* S5K4CDGX_H */

