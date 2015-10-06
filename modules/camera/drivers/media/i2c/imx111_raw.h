/*
 * Support for Sony IMX111_RAW camera sensor.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __IMX111_RAW_H__
#define __IMX111_RAW_H__
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <linux/HWVersion.h>
extern int Read_HW_ID(void);
extern int Read_PROJ_ID(void);
static unsigned int HW_ID = 0xFF;
static unsigned int PROJECT_ID = 0xFF;

//Add for VCM+++
#define DW9714_VCM_ADDR	0x0c

enum dw9714_tok_type {
	DW9714_8BIT  = 0x0001,
	DW9714_16BIT = 0x0002,
};

struct dw9714_vcm_settings {
	u16 code;	/* bit[9:0]: Data[9:0] */
	u8 t_src;	/* bit[4:0]: T_SRC[4:0] */
	u8 step_setting;	/* bit[3:0]: S[3:0]/bit[5:4]: MCLK[1:0] */
	bool update;
};

enum dw9714_vcm_mode {
	DW9714_DIRECT = 0x1,	/* direct control */
	DW9714_LSC = 0x2,	/* linear slope control */
	DW9714_DLC = 0x3,	/* dual level control */
};

/* dw9714 device structure */
struct dw9714_device {
	struct dw9714_vcm_settings vcm_settings;
	struct timespec timestamp_t_focus_abs;
	enum dw9714_vcm_mode vcm_mode;
	s16 number_of_steps;
	bool initialized;		/* true if dw9714 is detected */
	s32 focus;			/* Current focus value */
	struct timespec focus_time;	/* Time when focus was last time set */
	__u8 buffer[4];			/* Used for i2c transactions */
	const struct camera_af_platform_data *platform_data;
};

#define DW9714_INVALID_CONFIG	0xffffffff
#define DW9714_MAX_FOCUS_POS	1023


/* MCLK[1:0] = 01 T_SRC[4:0] = 00001 S[3:0] = 0111 */
#define DELAY_PER_STEP_NS	1000000
#define DELAY_MAX_PER_STEP_NS	(1000000 * 1023)

#define DLC_ENABLE 1
#define DLC_DISABLE 0
#define VCM_PROTECTION_OFF	0xeca3
#define VCM_PROTECTION_ON	0xdc51
#define VCM_DEFAULT_S 0x0

#define vcm_step_s(a) (u8)(a & 0xf)
#define vcm_step_mclk(a) (u8)((a >> 4) & 0x3)
#define vcm_dlc_mclk(dlc, mclk) (u16)((dlc << 3) | mclk | 0xa104)
#define vcm_tsrc(tsrc) (u16)(tsrc << 3 | 0xf200)
#define vcm_val(data, s) (u16)(data << 4 | s)
#define DIRECT_VCM vcm_dlc_mclk(0, 0)
//Add for VCM---



#define IMX111_RAW_NAME	"imx111_raw"
#define IMX111_RAW_ID	0x0111

#define IMX111_RAW_SC_CMMN_CHIP_ID_H	0x0000
#define IMX111_RAW_SC_CMMN_CHIP_ID_L	0x0001
#define IMX111_RAW_FOCAL_LENGTH_NUM	369	/*3.69mm*/
#define IMX111_RAW_FOCAL_LENGTH_DEM	100
#define IMX111_RAW_F_NUMBER_DEFAULT_NUM	22
#define IMX111_RAW_F_NUMBER_DEM	10

#define IMX111_RAW_RES_WIDTH_MAX	4208
#define IMX111_RAW_RES_HEIGHT_MAX	3120
#define IMX111_RAW_BIN_FACTOR_MAX			8
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define IMX111_RAW_FOCAL_LENGTH_DEFAULT 0x1710064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define IMX111_RAW_F_NUMBER_DEFAULT 0x16000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define IMX111_RAW_F_NUMBER_RANGE 0x160a160a

enum imx111_raw_tok_type {
	IMX111_RAW_8BIT  = 0x0001,
	IMX111_RAW_16BIT = 0x0002,
	IMX111_RAW_TOK_TERM   = 0xf000,	/* terminating token for reg list */
	IMX111_RAW_TOK_DELAY  = 0xfe00	/* delay token for reg list */
};

/**
 * struct imx111_raw_reg - MI sensor  register format
 * @type: type of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 *
 * Define a structure for sensor register initialization values
 */
struct imx111_raw_reg {
	enum imx111_raw_tok_type type;
	u16 sreg;
	u32 val;	/* @set value for read/mod/write, @mask */
};

struct imx111_raw_resolution {
	u8 *desc;
	const struct imx111_raw_reg *regs;
	int res;
	int width;
	int height;
	int fps;
	unsigned short pixels_per_line;
	unsigned short lines_per_frame;
	u8 bin_factor_x;
	u8 bin_factor_y;
	bool used;
	unsigned short skip_frames;
};
#define GROUPED_PARAMETER_HOLD_ENABLE  {IMX111_RAW_8BIT, 0x0104, 0x1}
#define GROUPED_PARAMETER_HOLD_DISABLE  {IMX111_RAW_8BIT, 0x0104, 0x0}






#define I2C_MSG_LENGTH		0x2

#define IMX111_RAW_MCLK		192

/* TODO - This should be added into include/linux/videodev2.h */
#ifndef V4L2_IDENT_IMX111_RAW
#define V4L2_IDENT_IMX111_RAW	8245
#endif

/*
 * imx111_raw System control registers
 */
#define IMX111_RAW_MASK_5BIT	0x1F
#define IMX111_RAW_MASK_4BIT	0xF
#define IMX111_RAW_MASK_2BIT	0x3
#define IMX111_RAW_MASK_11BIT	0x7FF
#define IMX111_RAW_INTG_BUF_COUNT		2

#define IMX111_RAW_INTEGRATION_TIME_MARGIN	5

#define IMX111_RAW_FINE_INTG_TIME		0x1E8

#define IMX111_RAW_PRE_PLL_CLK_DIV			0x0305
#define IMX111_RAW_PLL_MULTIPLIER			0x0307
#define IMX111_RAW_RGPLTD			0x30A4
#define IMX111_RAW_CCP2_DATA_FORMAT			0x0113

#define IMX111_RAW_FRAME_LENGTH_LINES		0x0340
#define IMX111_RAW_LINE_LENGTH_PIXELS		0x0342
#define IMX111_RAW_COARSE_INTG_TIME_MIN	0x1004
#define IMX111_RAW_COARSE_INTG_TIME_MAX	0x1006
#define IMX111_RAW_BINNING_ENABLE		0x0390
#define IMX111_RAW_BINNING_TYPE		0x0391

#define IMX111_RAW_CROP_X_START		0x0344
#define IMX111_RAW_CROP_Y_START		0x0346
#define IMX111_RAW_CROP_X_END			0x0348
#define IMX111_RAW_CROP_Y_END			0x034A
#define IMX111_RAW_OUTPUT_WIDTH		0x034C
#define IMX111_RAW_OUTPUT_HEIGHT		0x034E

#define IMX111_RAW_READ_MODE			0x0390

#define IMX111_RAW_COARSE_INTEGRATION_TIME		0x0202
#define IMX111_RAW_TEST_PATTERN_MODE			0x0600
#define IMX111_RAW_IMG_ORIENTATION			0x0101
#define IMX111_RAW_VFLIP_BIT			1
#define IMX111_RAW_GLOBAL_GAIN			0x0205
#define IMX111_RAW_DGC_ADJ			0x020E

/* Defines for register writes and register array processing */
#define IMX111_RAW_BYTE_MAX	32 /* change to 32 as needed by otpdata */
#define IMX111_RAW_SHORT_MAX	16
#define I2C_RETRY_COUNT		5
#define IMX111_RAW_TOK_MASK	0xfff0

#define MAX_FMTS 1

/* Defines for OTP Data Registers */
#define IMX111_RAW_OTP_START_ADDR		0x3500
#define IMX111_RAW_OTP_DATA_SIZE		60
#define IMX111_RAW_OTP_PAGE_SIZE		0x08
#define IMX111_RAW_OTP_PAGE_REG		0x34C9
#define IMX111_RAW_OTP_PAGE_MAX		0x09
#define IMX111_RAW_OTP_READ_ONETIME		8

#define IMX111_DEFAULT_AF_10CM	370
#define IMX111_DEFAULT_AF_INF	210
#define IMX111_DEFAULT_AF_START	156
#define	IMX111_DEFAULT_AF_END	560

struct imx111_raw_af_data {
	u16 af_inf_pos;
	u16 af_1m_pos;
	u16 af_10cm_pos;
	u16 af_start_curr;
	u8 module_id;
	u8 vendor_id;
	u16 default_af_inf_pos;
	u16 default_af_10cm_pos;
	u16 default_af_start;
	u16 default_af_end;
};


#define	v4l2_format_capture_type_entry(_width, _height, \
		_pixelformat, _bytesperline, _colorspace) \
	{\
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,\
		.fmt.pix.width = (_width),\
		.fmt.pix.height = (_height),\
		.fmt.pix.pixelformat = (_pixelformat),\
		.fmt.pix.bytesperline = (_bytesperline),\
		.fmt.pix.colorspace = (_colorspace),\
		.fmt.pix.sizeimage = (_height)*(_bytesperline),\
	}

#define	s_output_format_entry(_width, _height, _pixelformat, \
		_bytesperline, _colorspace, _fps) \
	{\
		.v4l2_fmt = v4l2_format_capture_type_entry(_width, \
			_height, _pixelformat, _bytesperline, \
				_colorspace),\
		.fps = (_fps),\
	}

#define	s_output_format_reg_entry(_width, _height, _pixelformat, \
		_bytesperline, _colorspace, _fps, _reg_setting) \
	{\
		.s_fmt = s_output_format_entry(_width, _height,\
				_pixelformat, _bytesperline, \
				_colorspace, _fps),\
		.reg_setting = (_reg_setting),\
	}

struct s_ctrl_id {
	struct v4l2_queryctrl qc;
	int (*s_ctrl)(struct v4l2_subdev *sd, u32 val);
	int (*g_ctrl)(struct v4l2_subdev *sd, u32 *val);
};

#define	v4l2_queryctrl_entry_integer(_id, _name,\
		_minimum, _maximum, _step, \
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_INTEGER, \
		.name = _name, \
		.minimum = (_minimum), \
		.maximum = (_maximum), \
		.step = (_step), \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}
#define	v4l2_queryctrl_entry_boolean(_id, _name,\
		_default_value, _flags)	\
	{\
		.id = (_id), \
		.type = V4L2_CTRL_TYPE_BOOLEAN, \
		.name = _name, \
		.minimum = 0, \
		.maximum = 1, \
		.step = 1, \
		.default_value = (_default_value),\
		.flags = (_flags),\
	}

#define	s_ctrl_id_entry_integer(_id, _name, \
		_minimum, _maximum, _step, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_integer(_id, _name,\
				_minimum, _maximum, _step,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}

#define	s_ctrl_id_entry_boolean(_id, _name, \
		_default_value, _flags, \
		_s_ctrl, _g_ctrl)	\
	{\
		.qc = v4l2_queryctrl_entry_boolean(_id, _name,\
				_default_value, _flags), \
		.s_ctrl = _s_ctrl, \
		.g_ctrl = _g_ctrl, \
	}


struct imx111_raw_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, s32 value);
};

/* imx111_raw device structure */
struct imx111_raw_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct camera_sensor_platform_data *platform_data;
	struct mutex input_lock; /* serialize sensor's ioctl */
	int fmt_idx;
	int status;
	int streaming;
	int power;
	int run_mode;
	int vt_pix_clk_freq_mhz;
	u32 focus;
	u16 sensor_id;
	u16 coarse_itg;
	u16 fine_itg;
	u16 gain;
	u16 digital_gain;
	u16 pixels_per_line;
	u16 lines_per_frame;
	u8 fps;
	u8 res;
	u8 type;
	u8 sensor_revision;
	u8 *otp_data;
	struct attribute_group sensor_i2c_attribute; //Add for ATD command+++
};

#define to_imx111_raw_sensor(x) container_of(x, struct imx111_raw_device, sd)

#define IMX111_RAW_MAX_WRITE_BUF_SIZE	32
struct imx111_raw_write_buffer {
	u16 addr;
	u8 data[IMX111_RAW_MAX_WRITE_BUF_SIZE];
};

struct imx111_raw_write_ctrl {
	int index;
	struct imx111_raw_write_buffer buffer;
};

struct sensor_mode_data {
	u32 coarse_integration_time_min;
	u32 coarse_integration_time_max_margin;
	u32 fine_integration_time_min;
	u32 fine_integration_time_max_margin;
	u32 fine_integration_time_def;
	u32 frame_length_lines;
	u32 line_length_pck;
	u32 read_mode;
	int vt_pix_clk_freq_mhz;
};

static const struct imx111_raw_reg imx111_raw_soft_standby[] = {
	{IMX111_RAW_8BIT, 0x0100, 0x00},
	{IMX111_RAW_TOK_TERM, 0, 0}
};

static const struct imx111_raw_reg imx111_raw_streaming[] = {
	{IMX111_RAW_8BIT, 0x0100, 0x01},
	{IMX111_RAW_TOK_TERM, 0, 0}
};

static const struct imx111_raw_reg imx111_raw_param_hold[] = {
	{IMX111_RAW_8BIT, 0x0104, 0x01},	/* GROUPED_PARAMETER_HOLD */
	{IMX111_RAW_TOK_TERM, 0, 0}
};

static const struct imx111_raw_reg imx111_raw_param_update[] = {
	{IMX111_RAW_8BIT, 0x0104, 0x00},	/* GROUPED_PARAMETER_HOLD */
	{IMX111_RAW_TOK_TERM, 0, 0}
};

#define IMX111_RAW_INVALID_CONFIG	0xffffffff
#define IMX111_RAW_MAX_FOCUS_POS	1023
#define IMX111_RAW_MAX_FOCUS_NEG	(-1023)
#define IMX111_RAW_VCM_SLEW_STEP_MAX	0x3f
#define IMX111_RAW_VCM_SLEW_TIME_MAX	0x1f

/********************** settings for imx111_raw from vendor*********************/

static struct imx111_raw_reg const imx111_raw_SwReset[] = {
	{ IMX111_RAW_8BIT,  0x0100 , 0x00 },
	{ IMX111_RAW_8BIT,  0x0103 , 0x01 },
	{ IMX111_RAW_TOK_DELAY, 0, 5},
	{ IMX111_RAW_8BIT,  0x0103 , 0x00 },
	{ IMX111_RAW_TOK_TERM, 0, 0},
};

/********************** settings for imx111_raw - reference *********************/
static struct imx111_raw_reg const imx111_raw_init_settings[] = {
	/* basic settings */
	GROUPED_PARAMETER_HOLD_ENABLE,
	//Global Setting
	{IMX111_RAW_8BIT, 0x3080, 0x50},
	{IMX111_RAW_8BIT, 0x3087, 0x53},
	{IMX111_RAW_8BIT, 0x309D, 0x94},
	{IMX111_RAW_8BIT, 0x30B1, 0x00},
	{IMX111_RAW_8BIT, 0x30C6, 0x00},
	{IMX111_RAW_8BIT, 0x30C7, 0x00},
	{IMX111_RAW_8BIT, 0x3115, 0x0B},
	{IMX111_RAW_8BIT, 0x3118, 0x30},
	{IMX111_RAW_8BIT, 0x311D, 0x25},
	{IMX111_RAW_8BIT, 0x3121, 0x0A},
	{IMX111_RAW_8BIT, 0x3212, 0xF2},
	{IMX111_RAW_8BIT, 0x3213, 0x0F},
	{IMX111_RAW_8BIT, 0x3215, 0x0F},
	{IMX111_RAW_8BIT, 0x3217, 0x0B},
	{IMX111_RAW_8BIT, 0x3219, 0x0B},
	{IMX111_RAW_8BIT, 0x321B, 0x0D},
	{IMX111_RAW_8BIT, 0x321D, 0x0D},
	{IMX111_RAW_8BIT, 0x32AA, 0x11},
	
	//Black Level Setting
	{IMX111_RAW_8BIT, 0x3032, 0x40},

	//2D noise reduction
	{IMX111_RAW_8BIT, 0x30D0, 0x40},
	{IMX111_RAW_8BIT, 0x30CF, 0x0E},
	{IMX111_RAW_8BIT, 0x3640, 0x0F},

	//=== End of Initial Setting ===
	GROUPED_PARAMETER_HOLD_DISABLE,
	{IMX111_RAW_TOK_TERM, 0, 0}	
};

/*****************************STILL********************************/
static struct imx111_raw_reg const imx111_raw_8M_17fps_still[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,
	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x0A},
		{IMX111_RAW_8BIT, 0x0341,0xA5},
		{IMX111_RAW_8BIT, 0x0342,0x0E},
		{IMX111_RAW_8BIT, 0x0343,0xE0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0x08},
		{IMX111_RAW_8BIT, 0x0346,0x00},
		{IMX111_RAW_8BIT, 0x0347,0x30},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0xD7},
		{IMX111_RAW_8BIT, 0x034A,0x09},
		{IMX111_RAW_8BIT, 0x034B,0xCF},
		{IMX111_RAW_8BIT, 0x034C,0x0C},
		{IMX111_RAW_8BIT, 0x034D,0xD0},
		{IMX111_RAW_8BIT, 0x034E,0x09},
		{IMX111_RAW_8BIT, 0x034F,0xA0},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x01},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x01},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x00},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x08},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x07},
		{IMX111_RAW_8BIT, 0x30D5,0x00},
		{IMX111_RAW_8BIT, 0x30D6,0x85},
		{IMX111_RAW_8BIT, 0x30D7,0x2A},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x00},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x60},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x0A},
		{IMX111_RAW_8BIT, 0x0203,0xA0},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

static struct imx111_raw_reg const imx111_raw_8M_16_9_24fps_still[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,
	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x07},
		{IMX111_RAW_8BIT, 0x0341,0xAA},
		{IMX111_RAW_8BIT, 0x0342,0x0E},
		{IMX111_RAW_8BIT, 0x0343,0xE0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0x08},
		{IMX111_RAW_8BIT, 0x0346,0x01},
		{IMX111_RAW_8BIT, 0x0347,0x62},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0xD7},
		{IMX111_RAW_8BIT, 0x034A,0x08},
		{IMX111_RAW_8BIT, 0x034B,0x9D},
		{IMX111_RAW_8BIT, 0x034C,0x0C},
		{IMX111_RAW_8BIT, 0x034D,0xD0},
		{IMX111_RAW_8BIT, 0x034E,0x07},
		{IMX111_RAW_8BIT, 0x034F,0x3C},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x01},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x01},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x00},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x08},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x07},
		{IMX111_RAW_8BIT, 0x30D5,0x00},
		{IMX111_RAW_8BIT, 0x30D6,0x85},
		{IMX111_RAW_8BIT, 0x30D7,0x2A},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x00},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x60},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x07},
		{IMX111_RAW_8BIT, 0x0203,0xA5},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

/*****************************IMX111_RAW PREVIEW********************************/
static struct imx111_raw_reg const imx111_raw_8M_18fps_pre[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,
	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x0A},
		{IMX111_RAW_8BIT, 0x0341,0xA5},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0x08},
		{IMX111_RAW_8BIT, 0x0346,0x00},
		{IMX111_RAW_8BIT, 0x0347,0x30},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0xD7},
		{IMX111_RAW_8BIT, 0x034A,0x09},
		{IMX111_RAW_8BIT, 0x034B,0xCF},
		{IMX111_RAW_8BIT, 0x034C,0x0C},
		{IMX111_RAW_8BIT, 0x034D,0xD0},
		{IMX111_RAW_8BIT, 0x034E,0x09},
		{IMX111_RAW_8BIT, 0x034F,0xA0},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x01},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x01},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x00},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x08},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x07},
		{IMX111_RAW_8BIT, 0x30D5,0x00},
		{IMX111_RAW_8BIT, 0x30D6,0x85},
		{IMX111_RAW_8BIT, 0x30D7,0x2A},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x00},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x60},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x0A},
		{IMX111_RAW_8BIT, 0x0203,0xA0},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

static struct imx111_raw_reg const imx111_raw_8M_16_9_26fps_pre[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,
	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x07},
		{IMX111_RAW_8BIT, 0x0341,0xAA},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0x08},
		{IMX111_RAW_8BIT, 0x0346,0x01},
		{IMX111_RAW_8BIT, 0x0347,0x62},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0xD7},
		{IMX111_RAW_8BIT, 0x034A,0x08},
		{IMX111_RAW_8BIT, 0x034B,0x9D},
		{IMX111_RAW_8BIT, 0x034C,0x0C},
		{IMX111_RAW_8BIT, 0x034D,0xD0},
		{IMX111_RAW_8BIT, 0x034E,0x07},
		{IMX111_RAW_8BIT, 0x034F,0x3C},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x01},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x01},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x00},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x08},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x07},
		{IMX111_RAW_8BIT, 0x30D5,0x00},
		{IMX111_RAW_8BIT, 0x30D6,0x85},
		{IMX111_RAW_8BIT, 0x30D7,0x2A},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x00},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x60},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x07},
		{IMX111_RAW_8BIT, 0x0203,0xA5},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};


static struct imx111_raw_reg const imx111_raw_2M_26fps[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,
	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x07},
		{IMX111_RAW_8BIT, 0x0341,0x75},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0x08},
		{IMX111_RAW_8BIT, 0x0346,0x00},
		{IMX111_RAW_8BIT, 0x0347,0x30},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0xD7},
		{IMX111_RAW_8BIT, 0x034A,0x09},
		{IMX111_RAW_8BIT, 0x034B,0xCF},
		{IMX111_RAW_8BIT, 0x034C,0x06},
		{IMX111_RAW_8BIT, 0x034D,0x68},
		{IMX111_RAW_8BIT, 0x034E,0x04},
		{IMX111_RAW_8BIT, 0x034F,0xD0},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x03},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x03},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x01},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x09},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x05},
		{IMX111_RAW_8BIT, 0x30D5,0x09},
		{IMX111_RAW_8BIT, 0x30D6,0x01},
		{IMX111_RAW_8BIT, 0x30D7,0x01},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x02},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x70},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x07},
		{IMX111_RAW_8BIT, 0x0203,0x70},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

static struct imx111_raw_reg const imx111_raw_2M_16_9_44fps[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,
	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x04},
		{IMX111_RAW_8BIT, 0x0307,0xE2},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x05},
		{IMX111_RAW_8BIT, 0x0341,0x55},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0x08},
		{IMX111_RAW_8BIT, 0x0346,0x01},
		{IMX111_RAW_8BIT, 0x0347,0x66},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0xD7},
		{IMX111_RAW_8BIT, 0x034A,0x08},
		{IMX111_RAW_8BIT, 0x034B,0x99},
		{IMX111_RAW_8BIT, 0x034C,0x06},
		{IMX111_RAW_8BIT, 0x034D,0x68},
		{IMX111_RAW_8BIT, 0x034E,0x03},
		{IMX111_RAW_8BIT, 0x034F,0x9A},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x03},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x03},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x01},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x09},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x05},
		{IMX111_RAW_8BIT, 0x30D5,0x09},
		{IMX111_RAW_8BIT, 0x30D6,0x01},
		{IMX111_RAW_8BIT, 0x30D7,0x01},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x02},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x70},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x05},
		{IMX111_RAW_8BIT, 0x0203,0x50},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

static struct imx111_raw_reg const imx111_raw_0_5M_80fps[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,
	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x02},
		{IMX111_RAW_8BIT, 0x0341,0x74},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0x08},
		{IMX111_RAW_8BIT, 0x0346,0x00},
		{IMX111_RAW_8BIT, 0x0347,0x34},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0xD7},
		{IMX111_RAW_8BIT, 0x034A,0x09},
		{IMX111_RAW_8BIT, 0x034B,0xCB},
		{IMX111_RAW_8BIT, 0x034C,0x03},
		{IMX111_RAW_8BIT, 0x034D,0x34},
		{IMX111_RAW_8BIT, 0x034E,0x02},
		{IMX111_RAW_8BIT, 0x034F,0x66},
		{IMX111_RAW_8BIT, 0x0381,0x05},
		{IMX111_RAW_8BIT, 0x0383,0x03},
		{IMX111_RAW_8BIT, 0x0385,0x05},
		{IMX111_RAW_8BIT, 0x0387,0x03},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x01},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x09},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x03},
		{IMX111_RAW_8BIT, 0x30D5,0x09},
		{IMX111_RAW_8BIT, 0x30D6,0x00},
		{IMX111_RAW_8BIT, 0x30D7,0x00},
		{IMX111_RAW_8BIT, 0x30D8,0x00},
		{IMX111_RAW_8BIT, 0x30D9,0x00},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x04},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x79},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x02},
		{IMX111_RAW_8BIT, 0x0203,0x6F},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};


/***************** IMX111_RAW VIDEO ***************************************/
static struct imx111_raw_reg const imx111_raw_1080p_28fps[] = {
	/* basic settings */
	GROUPED_PARAMETER_HOLD_ENABLE,

	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x07},
		{IMX111_RAW_8BIT, 0x0341,0x05},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x02},
		{IMX111_RAW_8BIT, 0x0345,0xA8},
		{IMX111_RAW_8BIT, 0x0346,0x02},
		{IMX111_RAW_8BIT, 0x0347,0xDC},
		{IMX111_RAW_8BIT, 0x0348,0x0A},
		{IMX111_RAW_8BIT, 0x0349,0x37},
		{IMX111_RAW_8BIT, 0x034A,0x07},
		{IMX111_RAW_8BIT, 0x034B,0x23},
		{IMX111_RAW_8BIT, 0x034C,0x07},
		{IMX111_RAW_8BIT, 0x034D,0x90},
		{IMX111_RAW_8BIT, 0x034E,0x04},
		{IMX111_RAW_8BIT, 0x034F,0x48},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x01},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x01},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x00},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x08},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x07},
		{IMX111_RAW_8BIT, 0x30D5,0x00},
		{IMX111_RAW_8BIT, 0x30D6,0x85},
		{IMX111_RAW_8BIT, 0x30D7,0x2A},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x00},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x60},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x07},
		{IMX111_RAW_8BIT, 0x0203,0x00},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

static struct imx111_raw_reg const imx111_raw_720p_37fps[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,

	//	{IMX111_RAW_8BIT, 0x0101,0x03},
			
		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},
		
		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x05},
		{IMX111_RAW_8BIT, 0x0341,0x55},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x01},
		{IMX111_RAW_8BIT, 0x0345,0x60},
		{IMX111_RAW_8BIT, 0x0346,0x02},
		{IMX111_RAW_8BIT, 0x0347,0x20},
		{IMX111_RAW_8BIT, 0x0348,0x0B},
		{IMX111_RAW_8BIT, 0x0349,0x7F},
		{IMX111_RAW_8BIT, 0x034A,0x07},
		{IMX111_RAW_8BIT, 0x034B,0xDF},
		{IMX111_RAW_8BIT, 0x034C,0x05},
		{IMX111_RAW_8BIT, 0x034D,0x10},
		{IMX111_RAW_8BIT, 0x034E,0x02},
		{IMX111_RAW_8BIT, 0x034F,0xE0},
		{IMX111_RAW_8BIT, 0x0381,0x01},
		{IMX111_RAW_8BIT, 0x0383,0x03},
		{IMX111_RAW_8BIT, 0x0385,0x01},
		{IMX111_RAW_8BIT, 0x0387,0x03},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x01},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x09},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x05},
		{IMX111_RAW_8BIT, 0x30D5,0x09},
		{IMX111_RAW_8BIT, 0x30D6,0x01},
		{IMX111_RAW_8BIT, 0x30D7,0x01},
		{IMX111_RAW_8BIT, 0x30D8,0x64},
		{IMX111_RAW_8BIT, 0x30D9,0x89},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x02},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x70},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},
		
		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x05},
		{IMX111_RAW_8BIT, 0x0203,0x50},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

static struct imx111_raw_reg const imx111_raw_vga_62fps[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,

	//	{IMX111_RAW_8BIT, 0x0101,0x03},

		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},

		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x03},
		{IMX111_RAW_8BIT, 0x0341,0x35},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x01},
		{IMX111_RAW_8BIT, 0x0345,0x50},
		{IMX111_RAW_8BIT, 0x0346,0x01},
		{IMX111_RAW_8BIT, 0x0347,0x20},
		{IMX111_RAW_8BIT, 0x0348,0x0B},
		{IMX111_RAW_8BIT, 0x0349,0x8F},
		{IMX111_RAW_8BIT, 0x034A,0x08},
		{IMX111_RAW_8BIT, 0x034B,0xDF},
		{IMX111_RAW_8BIT, 0x034C,0x02},
		{IMX111_RAW_8BIT, 0x034D,0x90},
		{IMX111_RAW_8BIT, 0x034E,0x01},
		{IMX111_RAW_8BIT, 0x034F,0xF0},
		{IMX111_RAW_8BIT, 0x0381,0x05},
		{IMX111_RAW_8BIT, 0x0383,0x03},
		{IMX111_RAW_8BIT, 0x0385,0x05},
		{IMX111_RAW_8BIT, 0x0387,0x03},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x01},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x09},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x03},
		{IMX111_RAW_8BIT, 0x30D5,0x09},
		{IMX111_RAW_8BIT, 0x30D6,0x00},
		{IMX111_RAW_8BIT, 0x30D7,0x00},
		{IMX111_RAW_8BIT, 0x30D8,0x00},
		{IMX111_RAW_8BIT, 0x30D9,0x00},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x04},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x79},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},

		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x03},
		{IMX111_RAW_8BIT, 0x0203,0x30},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

static struct imx111_raw_reg const imx111_raw_cif_80fps[] = {
		/* basic settings */
		GROUPED_PARAMETER_HOLD_ENABLE,

	//	{IMX111_RAW_8BIT, 0x0101,0x03},

		//PLL Setting
		{IMX111_RAW_8BIT, 0x0305,0x02},
		{IMX111_RAW_8BIT, 0x0307,0x5E},
		{IMX111_RAW_8BIT, 0x30A4,0x02},
		{IMX111_RAW_8BIT, 0x303C,0x3C},

		//Mode Setting
		{IMX111_RAW_8BIT, 0x0340,0x02},
		{IMX111_RAW_8BIT, 0x0341,0x7E},
		{IMX111_RAW_8BIT, 0x0342,0x0D},
		{IMX111_RAW_8BIT, 0x0343,0xD0},
		{IMX111_RAW_8BIT, 0x0344,0x00},
		{IMX111_RAW_8BIT, 0x0345,0xB0},
		{IMX111_RAW_8BIT, 0x0346,0x00},
		{IMX111_RAW_8BIT, 0x0347,0x40},
		{IMX111_RAW_8BIT, 0x0348,0x0C},
		{IMX111_RAW_8BIT, 0x0349,0x2F},
		{IMX111_RAW_8BIT, 0x034A,0x09},
		{IMX111_RAW_8BIT, 0x034B,0xBF},
		{IMX111_RAW_8BIT, 0x034C,0x01},
		{IMX111_RAW_8BIT, 0x034D,0x70},
		{IMX111_RAW_8BIT, 0x034E,0x01},
		{IMX111_RAW_8BIT, 0x034F,0x30},
		{IMX111_RAW_8BIT, 0x0381,0x09},
		{IMX111_RAW_8BIT, 0x0383,0x07},
		{IMX111_RAW_8BIT, 0x0385,0x09},
		{IMX111_RAW_8BIT, 0x0387,0x07},
		{IMX111_RAW_8BIT, 0x3033,0x00},
		{IMX111_RAW_8BIT, 0x303D,0x10},
		{IMX111_RAW_8BIT, 0x303E,0x41},
		{IMX111_RAW_8BIT, 0x3040,0x08},
		{IMX111_RAW_8BIT, 0x3041,0x97},
		{IMX111_RAW_8BIT, 0x3048,0x01},
		{IMX111_RAW_8BIT, 0x304C,0x6F},
		{IMX111_RAW_8BIT, 0x304D,0x03},
		{IMX111_RAW_8BIT, 0x3064,0x12},
		{IMX111_RAW_8BIT, 0x3073,0x00},
		{IMX111_RAW_8BIT, 0x3074,0x11},
		{IMX111_RAW_8BIT, 0x3075,0x11},
		{IMX111_RAW_8BIT, 0x3076,0x11},
		{IMX111_RAW_8BIT, 0x3077,0x11},
		{IMX111_RAW_8BIT, 0x3079,0x00},
		{IMX111_RAW_8BIT, 0x307A,0x00},
		{IMX111_RAW_8BIT, 0x309B,0x20},
		{IMX111_RAW_8BIT, 0x309C,0x13},
		{IMX111_RAW_8BIT, 0x309E,0x00},
		{IMX111_RAW_8BIT, 0x30A0,0x14},
		{IMX111_RAW_8BIT, 0x30A1,0x09},
		{IMX111_RAW_8BIT, 0x30AA,0x02},
		{IMX111_RAW_8BIT, 0x30B2,0x01},
		{IMX111_RAW_8BIT, 0x30D5,0x09},
		{IMX111_RAW_8BIT, 0x30D6,0x00},
		{IMX111_RAW_8BIT, 0x30D7,0x00},
		{IMX111_RAW_8BIT, 0x30D8,0x00},
		{IMX111_RAW_8BIT, 0x30D9,0x00},
		{IMX111_RAW_8BIT, 0x30DA,0x00},
		{IMX111_RAW_8BIT, 0x30DB,0x00},
		{IMX111_RAW_8BIT, 0x30DC,0x00},
		{IMX111_RAW_8BIT, 0x30DD,0x00},
		{IMX111_RAW_8BIT, 0x30DE,0x08},
		{IMX111_RAW_8BIT, 0x30DF,0x20},
		{IMX111_RAW_8BIT, 0x3102,0x10},
		{IMX111_RAW_8BIT, 0x3103,0x44},
		{IMX111_RAW_8BIT, 0x3104,0x40},
		{IMX111_RAW_8BIT, 0x3105,0x00},
		{IMX111_RAW_8BIT, 0x3106,0x0D},
		{IMX111_RAW_8BIT, 0x3107,0x01},
		{IMX111_RAW_8BIT, 0x3108,0x09},
		{IMX111_RAW_8BIT, 0x3109,0x08},
		{IMX111_RAW_8BIT, 0x310A,0x0F},
		{IMX111_RAW_8BIT, 0x315C,0x5D},
		{IMX111_RAW_8BIT, 0x315D,0x5C},
		{IMX111_RAW_8BIT, 0x316E,0x5E},
		{IMX111_RAW_8BIT, 0x316F,0x5D},
		{IMX111_RAW_8BIT, 0x3301,0x00},
		{IMX111_RAW_8BIT, 0x3304,0x05},
		{IMX111_RAW_8BIT, 0x3305,0x05},
		{IMX111_RAW_8BIT, 0x3306,0x15},
		{IMX111_RAW_8BIT, 0x3307,0x02},
		{IMX111_RAW_8BIT, 0x3308,0x0D},
		{IMX111_RAW_8BIT, 0x3309,0x07},
		{IMX111_RAW_8BIT, 0x330A,0x09},
		{IMX111_RAW_8BIT, 0x330B,0x05},
		{IMX111_RAW_8BIT, 0x330C,0x08},
		{IMX111_RAW_8BIT, 0x330D,0x06},
		{IMX111_RAW_8BIT, 0x330E,0x03},
		{IMX111_RAW_8BIT, 0x3318,0x7B},
		{IMX111_RAW_8BIT, 0x3322,0x03},
		{IMX111_RAW_8BIT, 0x3342,0x00},
		{IMX111_RAW_8BIT, 0x3348,0xE0},

		//Shutter Gain Setting
		{IMX111_RAW_8BIT, 0x0202,0x02},
		{IMX111_RAW_8BIT, 0x0203,0x79},

		GROUPED_PARAMETER_HOLD_DISABLE,
		{IMX111_RAW_TOK_TERM,  0, 0}
};

struct imx111_raw_resolution imx111_raw_res_preview[] = {
	{
		.desc = "imx111_raw_8M_16_9_24fps_still",
		.regs = imx111_raw_8M_16_9_24fps_still,
		.width = 3280,
		.height = 1852,
		.fps = 24,
		.pixels_per_line = 0x0EE0, /* consistent with regs arrays */
		.lines_per_frame = 0x07AA, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_8M_17fps_still",
		.regs = imx111_raw_8M_17fps_still,
		.width = 3280,
		.height = 2464,
		.fps = 17,
		.pixels_per_line = 0x0EE0, /* consistent with regs arrays */
		.lines_per_frame = 0x0AA5, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_1080p_28fps",
		.regs = imx111_raw_1080p_28fps,
		.width = 1936,
		.height = 1096,
		.fps = 28,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0705, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_2M_16_9_44fps",
		.regs = imx111_raw_2M_16_9_44fps,
		.width = 1640,
		.height = 922,
		.fps = 44,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0555, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_2M_26fps",
		.regs = imx111_raw_2M_26fps,
		.width = 1640,
		.height = 1232,
		.fps = 26,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0775, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
		.skip_frames = 1,
	},
};

struct imx111_raw_resolution imx111_raw_res_still[] = {
	{
		.desc = "imx111_raw_8M_16_9_24fps_still",
		.regs = imx111_raw_8M_16_9_24fps_still,
		.width = 3280,
		.height = 1852,
		.fps = 24,
		.pixels_per_line = 0x0EE0, /* consistent with regs arrays */
		.lines_per_frame = 0x07AA, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_8M_17fps_still",
		.regs = imx111_raw_8M_17fps_still,
		.width = 3280,
		.height = 2464,
		.fps = 17,
		.pixels_per_line = 0x0EE0, /* consistent with regs arrays */
		.lines_per_frame = 0x0AA5, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_2M_16_9_44fps",
		.regs = imx111_raw_2M_16_9_44fps,
		.width = 1640,
		.height = 922,
		.fps = 44,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0555, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_2M_26fps",
		.regs = imx111_raw_2M_26fps,
		.width = 1640,
		.height = 1232,
		.fps = 26,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0775, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
		.skip_frames = 1,
	},
	{
		.desc = "imx111_raw_1080p_28fps",
		.regs = imx111_raw_1080p_28fps,
		.width = 1936,
		.height = 1096,
		.fps = 28,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0705, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 1,
	},
};

struct imx111_raw_resolution imx111_raw_res_video[] = {
	{
		.desc = "imx111_raw_cif_80fps",
		.regs = imx111_raw_cif_80fps,
		.width = 368,
		.height = 304,
		.fps = 80,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x027E, /* consistent with regs arrays */
		.bin_factor_x = 4,
		.bin_factor_y = 4,
		.used = 0,
		.skip_frames = 2,
	},		
	{
		.desc = "imx111_raw_vga_62fps",
		.regs = imx111_raw_vga_62fps,
		.width = 656,
		.height = 496,
		.fps = 62,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0335, /* consistent with regs arrays */
		.bin_factor_x = 4,
		.bin_factor_y = 4,
		.used = 0,
		.skip_frames = 2,
	},
	{
		.desc = "imx111_raw_720p_37fps",
		.regs = imx111_raw_720p_37fps,
		.width = 1296,
		.height = 736,
		.fps = 37,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0555, /* consistent with regs arrays */
		.bin_factor_x = 2,
		.bin_factor_y = 2,
		.used = 0,
		.skip_frames = 2,
	},
	{
		.desc = "imx111_raw_1080p_28fps",
		.regs = imx111_raw_1080p_28fps,
		.width = 1936,
		.height = 1096,
		.fps = 28,
		.pixels_per_line = 0x0DD0, /* consistent with regs arrays */
		.lines_per_frame = 0x0705, /* consistent with regs arrays */
		.bin_factor_x = 1,
		.bin_factor_y = 1,
		.used = 0,
		.skip_frames = 2,
	},
};

#define N_RES_PREVIEW (ARRAY_SIZE(imx111_raw_res_preview))
#define N_RES_STILL (ARRAY_SIZE(imx111_raw_res_still))
#define N_RES_VIDEO (ARRAY_SIZE(imx111_raw_res_video))

struct imx111_raw_resolution *imx111_raw_res = imx111_raw_res_preview;
static int N_RES = N_RES_PREVIEW;


#endif

