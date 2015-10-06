/*
 * Support for Sony imx111_raw 8MP camera sensor.
 *
 * Copyright (c) 2012 Intel Corporation. All Rights Reserved.
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
#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-device.h>
#include "imx111_raw.h"
#include <linux/proc_fs.h>

//Add for VCM+++
static struct dw9714_device dw9714_dev;
static int dw9714_i2c_write(struct i2c_client *client, u16 data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	u16 val;

	val = cpu_to_be16(data);
	msg.addr = DW9714_VCM_ADDR;
	msg.flags = 0;
	msg.len = DW9714_16BIT;
	msg.buf = (u8 *)&val;

	ret = i2c_transfer(client->adapter, &msg, 1);

	return ret == num_msg ? 0 : -EIO;
}

int dw9714_t_focus_vcm(struct v4l2_subdev *sd, u16 val)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = -EINVAL;
	//u8 mclk = vcm_step_mclk(dw9714_dev.vcm_settings.step_setting);
	u8 mclk = vcm_step_s(dw9714_dev.vcm_settings.step_setting);
	u8 s = vcm_step_s(dw9714_dev.vcm_settings.step_setting);

	/*
	 * For different mode, VCM_PROTECTION_OFF/ON required by the
	 * control procedure. For DW9714_DIRECT/DLC mode, slew value is
	 * VCM_DEFAULT_S(0).
	 */
	 
	//printk("%s: ========== VCM DeRing Check:VM=%d,mclk=%x,st=%x \n", __func__,dw9714_dev.vcm_mode,mclk,s);
	
	switch (dw9714_dev.vcm_mode) {
	case DW9714_DIRECT:
		if (dw9714_dev.vcm_settings.update) {
			ret = dw9714_i2c_write(client, VCM_PROTECTION_OFF);
			if (ret)
				return ret;
			ret = dw9714_i2c_write(client, DIRECT_VCM);
			if (ret)
				return ret;
			ret = dw9714_i2c_write(client, VCM_PROTECTION_ON);
			if (ret)
				return ret;
			dw9714_dev.vcm_settings.update = false;
		}
		ret = dw9714_i2c_write(client,
					vcm_val(val, VCM_DEFAULT_S));
		break;
	case DW9714_LSC:
		if (dw9714_dev.vcm_settings.update) {
			ret = dw9714_i2c_write(client, VCM_PROTECTION_OFF);
			if (ret)
				return ret;
			ret = dw9714_i2c_write(client,
				vcm_dlc_mclk(DLC_DISABLE, mclk));
			if (ret)
				return ret;
			ret = dw9714_i2c_write(client,
				vcm_tsrc(dw9714_dev.vcm_settings.t_src));
			if (ret)
				return ret;
			//printk("%s: ==== VCM LSC mode:%x,%x \n",__func__,dw9714_dev.vcm_settings.t_src,dw9714_dev.vcm_settings.step_setting);
			ret = dw9714_i2c_write(client, VCM_PROTECTION_ON);
			if (ret)
				return ret;
			dw9714_dev.vcm_settings.update = false;
		}
		ret = dw9714_i2c_write(client, vcm_val(val, s));
		break;
	case DW9714_DLC:
		if (dw9714_dev.vcm_settings.update) {
			ret = dw9714_i2c_write(client, VCM_PROTECTION_OFF);
			if (ret)
				return ret;
			ret = dw9714_i2c_write(client,
					vcm_dlc_mclk(DLC_ENABLE, mclk));
			if (ret)
				return ret;
			ret = dw9714_i2c_write(client,
				vcm_tsrc(dw9714_dev.vcm_settings.t_src));
			if (ret)
				return ret;
			//printk("%s: ==== VCM DLC mode: %x,%x,%x \n",__func__,dw9714_dev.vcm_settings.t_src,mclk,dw9714_dev.vcm_settings.step_setting);
			ret = dw9714_i2c_write(client, VCM_PROTECTION_ON);
			if (ret)
				return ret;
			dw9714_dev.vcm_settings.update = false;
		}
		ret = dw9714_i2c_write(client,
					vcm_val(val, VCM_DEFAULT_S));
		break;
	}
	return ret;
}

int dw9714_t_focus_abs(struct v4l2_subdev *sd, s32 value)
{
	int ret;

	value = min(value, DW9714_MAX_FOCUS_POS);
	ret = dw9714_t_focus_vcm(sd, value);
	if (ret == 0) {
		dw9714_dev.number_of_steps = value - dw9714_dev.focus;
		dw9714_dev.focus = value;
		getnstimeofday(&(dw9714_dev.timestamp_t_focus_abs));
	}
	//printk("%s: ========== FOCUS_POS:%x,%x \n", __func__, dw9714_dev.number_of_steps,dw9714_dev.focus);
	return ret;
}

int dw9714_t_focus_rel(struct v4l2_subdev *sd, s32 value)
{

	return dw9714_t_focus_abs(sd, dw9714_dev.focus + value);
}

int dw9714_q_focus_status(struct v4l2_subdev *sd, s32 *value)
{
	u32 status = 0;
	struct timespec temptime;

	const struct timespec timedelay = {
		0,
		min_t(u32, abs(dw9714_dev.number_of_steps)*DELAY_PER_STEP_NS,
			DELAY_MAX_PER_STEP_NS),
	};

	ktime_get_ts(&temptime);

	temptime = timespec_sub(temptime, (dw9714_dev.timestamp_t_focus_abs));

	if (timespec_compare(&temptime, &timedelay) <= 0) {
		status |= ATOMISP_FOCUS_STATUS_MOVING;
		status |= ATOMISP_FOCUS_HP_IN_PROGRESS;
	} else {
		status |= ATOMISP_FOCUS_STATUS_ACCEPTS_NEW_MOVE;
		status |= ATOMISP_FOCUS_HP_COMPLETE;
	}
	*value = status;

	return 0;
}

int dw9714_q_focus_abs(struct v4l2_subdev *sd, s32 *value)
{
	//s32 val;

	//dw9714_q_focus_status(sd, &val);

	//if (val & ATOMISP_FOCUS_STATUS_MOVING)
	//	*value  = dw9714_dev.focus - dw9714_dev.number_of_steps;
	//else
		*value  = dw9714_dev.focus ;

	return 0;
}

int dw9714_t_vcm_slew(struct v4l2_subdev *sd, s32 value)
{
	dw9714_dev.vcm_settings.step_setting = value;
	dw9714_dev.vcm_settings.update = true;

	return 0;
}

int dw9714_t_vcm_timing(struct v4l2_subdev *sd, s32 value)
{
	dw9714_dev.vcm_settings.t_src = value;
	dw9714_dev.vcm_settings.update = true;

	return 0;
}

int dw9714_vcm_init(struct v4l2_subdev *sd)
{
	/* set VCM to home position and vcm mode to direct*/
	//dw9714_dev.vcm_mode = DW9714_DIRECT;
	
	dw9714_dev.vcm_mode = DW9714_DLC;
	dw9714_dev.vcm_settings.t_src = 0x11;
	dw9714_dev.vcm_settings.step_setting = 0x02;
	//printk("%s: ==== VCM initial setting:%x,%x \n",__func__,dw9714_dev.vcm_settings.t_src,dw9714_dev.vcm_settings.step_setting);
	dw9714_dev.vcm_settings.update = true;

	return 0;
}

//Add for VCM---

//Add for ATD command+++
extern int build_version; //Add build version -> user:3, userdebug:2, eng:1
struct v4l2_subdev *main_sd;

int ATD_imx111_raw_status = 0;
static char camera_module_otp[120];

static void *imx111_raw_otp_read(struct v4l2_subdev *sd);

static ssize_t imx111_raw_show_status(struct device *dev,struct device_attribute *attr,char *buf)
{
	printk("%s: get imx111_raw status (%d) !!\n", __func__, ATD_imx111_raw_status);
	//Check sensor connect status, just do it  in begining for ATD camera status

	return sprintf(buf,"%d\n", ATD_imx111_raw_status);
}

static ssize_t imx111_raw_read_otp(struct device *dev,struct device_attribute *attr,char *buf)
{
	printk("%s: get imx111 module OTP %s !!\n", __func__, camera_module_otp);
	//Check sensor OTP value, just do it in begining for ATD camera status
/*
	if(build_version != 1){ //not eng, need to read otp first
		imx111_raw_otp_read(main_sd);
	}
*/
	return sprintf(buf,"%s", camera_module_otp);
}

static DEVICE_ATTR(imx111_raw_status, S_IRUGO,imx111_raw_show_status,NULL);
static DEVICE_ATTR(imx111_raw_read_otp, S_IRUGO,imx111_raw_read_otp,NULL);

static struct attribute *imx111_raw_attributes[] = {
	&dev_attr_imx111_raw_status.attr,
	&dev_attr_imx111_raw_read_otp.attr,
	NULL
};
//Add for ATD command---

//For DIT VCM Debug Interface+++
#define	VCM_PROC_FILE	"driver/camera_vcm"
static struct proc_dir_entry *vcm_proc_file;

static ssize_t vcm_proc_read(char *page, char **start, off_t off, int count,
            int *eof, void *data)
{
    s32 vcm_steps;
    dw9714_q_focus_abs(main_sd, &vcm_steps);
    pr_info("vcm_proc_read vcm_steps %d\n", vcm_steps);
    return sprintf(page, "%d\n", vcm_steps);
}

static ssize_t vcm_proc_write(struct file *filp, const char __user *buff,
            unsigned long len, void *data)
{
    char buffer[256];
    s32 value;

    if (len > 256)
        len = 256;

    pr_info("vcm_proc_write %s\n", buff);
    if (copy_from_user(buffer, buff, len)) {
        printk(KERN_INFO "%s: proc write to buffer failed.\n", __func__);
        return -EFAULT;
    }
    if (!strncmp("setabs",buffer,6)) {
        sscanf(&buffer[7],"%d",&value);
        printk("set position to %d\n", value);
        dw9714_t_focus_abs(main_sd, value);
    } else if(!strncmp("setrel",buffer,6)){
        sscanf(&buffer[7],"%d",&value);
        printk("add position %d\n", value);
        dw9714_t_focus_rel(main_sd, value);
    } else {
        pr_info("command not support\n");
    }

    return len;
}

void create_vcm_proc_file(void)
{
    vcm_proc_file = create_proc_entry(VCM_PROC_FILE, 0666, NULL);
    if (vcm_proc_file) {
        vcm_proc_file->read_proc = vcm_proc_read;
        vcm_proc_file->write_proc = vcm_proc_write;
    } else{
        pr_err("proc file create failed!\n");
    }
}
//For DIT VCM Debug Interface---

static int
imx111_raw_read_reg(struct i2c_client *client, u16 len, u16 reg, void *val, bool otpck)
{
	struct i2c_msg msg[2];
	u16 data[IMX111_RAW_SHORT_MAX] = { 0 };
	int err, i;

	if (len > IMX111_RAW_BYTE_MAX) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	memset(msg, 0 , sizeof(msg));

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = I2C_MSG_LENGTH;
	msg[0].buf = (u8 *)data;
	/* high byte goes first */
	data[0] = cpu_to_be16(reg);

	msg[1].addr = client->addr;
	msg[1].len = len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = (u8 *)data;

	err = i2c_transfer(client->adapter, msg, 2);
	if (err != 2) {
		if (err >= 0)
			err = -EIO;
		goto error;
	}

	if (otpck == 1) {
		memcpy(val, data, len);
		otpck = 0;
	} else if (len == IMX111_RAW_8BIT) {
		*((u16 *)val) = (u8)data[0];
	} else {
		/* 16-bit access is default when len > 1 */
		for (i = 0; i < (len >> 1); i++)
			((u16 *)val)[i] = be16_to_cpu(data[i]);
	}

	return 0;

error:
	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}

static int imx111_raw_i2c_write(struct i2c_client *client, u16 len, u8 *data)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;

	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = data;

	ret = i2c_transfer(client->adapter, &msg, 1);

	return ret == num_msg ? 0 : -EIO;
}

static int
imx111_raw_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u16 val)
{
	int ret;
	unsigned char data[4] = {0};
	u16 *wreg = (u16 *)data;
	const u16 len = data_length + sizeof(u16); /* 16-bit address + data */

	if (data_length != IMX111_RAW_8BIT && data_length != IMX111_RAW_16BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	/* high byte goes out first */
	*wreg = cpu_to_be16(reg);

	if (data_length == IMX111_RAW_8BIT)
		data[2] = (u8)(val);
	else {
		/* IMX111_RAW_16BIT */
		u16 *wdata = (u16 *)&data[2];
		*wdata = cpu_to_be16(val);
	}

	ret = imx111_raw_i2c_write(client, len, data);
	if (ret)
		dev_err(&client->dev,
			"write error: wrote 0x%x to offset 0x%x error %d",
			val, reg, ret);

	return ret;
}

/*
 * imx111_raw_write_reg_array - Initializes a list of imx111_raw registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __imx111_raw_flush_reg_array, __imx111_raw_buf_reg_array() and
 * __imx111_raw_write_reg_is_consecutive() are internal functions to
 * imx111_raw_write_reg_array_fast() and should be not used anywhere else.
 *
 */

static int __imx111_raw_flush_reg_array(struct i2c_client *client,
				     struct imx111_raw_write_ctrl *ctrl)
{
	u16 size;

	if (ctrl->index == 0)
		return 0;

	size = sizeof(u16) + ctrl->index; /* 16-bit address + data */
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	ctrl->index = 0;

	return imx111_raw_i2c_write(client, size, (u8 *)&ctrl->buffer);
}

static int __imx111_raw_buf_reg_array(struct i2c_client *client,
				   struct imx111_raw_write_ctrl *ctrl,
				   const struct imx111_raw_reg *next)
{
	int size;
	u16 *data16;

	switch (next->type) {
	case IMX111_RAW_8BIT:
		size = 1;
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case IMX111_RAW_16BIT:
		size = 2;
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = (u16)next->val;
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->sreg;

	ctrl->index += size;

	/*
	 * Buffer cannot guarantee free space for u32? Better flush it to avoid
	 * possible lack of memory for next item.
	 */
	if (ctrl->index + sizeof(u16) >= IMX111_RAW_MAX_WRITE_BUF_SIZE)
		return __imx111_raw_flush_reg_array(client, ctrl);

	return 0;
}

static int
__imx111_raw_write_reg_is_consecutive(struct i2c_client *client,
				   struct imx111_raw_write_ctrl *ctrl,
				   const struct imx111_raw_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->sreg;
}

static int imx111_raw_write_reg_array(struct i2c_client *client,
				   const struct imx111_raw_reg *reglist)
{
	const struct imx111_raw_reg *next = reglist;
	struct imx111_raw_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->type != IMX111_RAW_TOK_TERM; next++) {
		switch (next->type & IMX111_RAW_TOK_MASK) {
		case IMX111_RAW_TOK_DELAY:
			err = __imx111_raw_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;

		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__imx111_raw_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __imx111_raw_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __imx111_raw_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __imx111_raw_flush_reg_array(client, &ctrl);
}

static int imx111_raw_read_otp_reg_array(struct i2c_client *client, u16 size, u16 addr,
				  u8 *buf)
{
	u16 index;
	int ret;

	for (index = 0; index + IMX111_RAW_OTP_READ_ONETIME <= size;
					index += IMX111_RAW_OTP_READ_ONETIME) {
		ret = imx111_raw_read_reg(client, IMX111_RAW_OTP_READ_ONETIME, addr + index,
					&buf[index], 1);
		if (ret)
			return ret;
	}
	return 0;
}

static int __imx111_raw_otp_read(struct v4l2_subdev *sd, struct imx111_raw_af_data *buf)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	int i=0, j=0, k=1; 
	u8 read_value[30];

	for(i=6; i>=0; i=i-3) {
		
		/*set page NO.*/
		ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			       IMX111_RAW_OTP_PAGE_REG, i & 0xff);
		if (ret) {
			dev_err(&client->dev, "failed to prepare OTP page\n");
			return ret;
		}

		/* Reading the OTP data array */
		ret = imx111_raw_read_otp_reg_array(client, IMX111_RAW_OTP_PAGE_SIZE,
			IMX111_RAW_OTP_START_ADDR + i * IMX111_RAW_OTP_PAGE_SIZE, read_value);
		if (ret) {
			dev_err(&client->dev, "failed to read OTP data\n");
			return ret;
		}
	    printk("%s Check bank %d 0x%X 0x%X\n",__func__, i, read_value[0], read_value[1]);
	    if(((read_value[0]==0)&&(read_value[1]==0))	||((read_value[0]==0xff)&&(read_value[1]==0xff))) {
			continue;
	    }
		break;
	}

	for (j=i+1; j < i+3; j++) {

		/*set page NO.*/
		ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			       IMX111_RAW_OTP_PAGE_REG, j & 0xff);
		if (ret) {
			dev_err(&client->dev, "failed to prepare OTP page\n");
			return ret;
		}

		/* Reading the OTP data array */
		ret = imx111_raw_read_otp_reg_array(client, IMX111_RAW_OTP_PAGE_SIZE,
			IMX111_RAW_OTP_START_ADDR + j * IMX111_RAW_OTP_PAGE_SIZE, read_value + k * IMX111_RAW_OTP_PAGE_SIZE);
		if (ret) {
			dev_err(&client->dev, "failed to read OTP data\n");
			return ret;
		}
		k++;
	}	
	
	//Return OTP value
	snprintf(camera_module_otp, sizeof(camera_module_otp)
		, "0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X\n"
		, read_value[0], read_value[1], read_value[2], read_value[3], read_value[4], read_value[5], read_value[6]
		, read_value[7], read_value[8], read_value[9], read_value[10], read_value[11], read_value[12], read_value[13]
		, read_value[14], read_value[15], read_value[16], read_value[17], read_value[18], read_value[19]);
		
	printk("%s OTP value: %s\n",__func__, camera_module_otp);

	buf->af_inf_pos = read_value[0]<<8 | read_value[1];
	buf->af_1m_pos = read_value[2]<<8 | read_value[3];
	buf->af_10cm_pos = read_value[4]<<8 | read_value[5];
	buf->af_start_curr = read_value[6]<<8 | read_value[7];
	buf->module_id = read_value[8];
	buf->vendor_id = read_value[9];
	buf->default_af_inf_pos = IMX111_DEFAULT_AF_INF;
	buf->default_af_10cm_pos = IMX111_DEFAULT_AF_10CM;
	buf->default_af_start = IMX111_DEFAULT_AF_START;
	buf->default_af_end = IMX111_DEFAULT_AF_END;
	
	return 0;
}



static void *imx111_raw_otp_read(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 *buf;
	int ret;

	buf = devm_kzalloc(&client->dev, IMX111_RAW_OTP_DATA_SIZE, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	ret = __imx111_raw_otp_read(sd, buf);

	/* Driver has failed to find valid data */
	if (ret) {
		dev_err(&client->dev, "sensor found no valid OTP data\n");
		return ERR_PTR(ret);
	}

	return buf;
}
static long __imx111_raw_set_exposure(struct v4l2_subdev *sd, unsigned int coarse_itg,
			       unsigned int gain, unsigned int digitgain)

{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	u16 vts = dev->lines_per_frame;

//	printk("%s exposure=%d gain=%d dgain=%d mode=0x%X\n", __func__, coarse_itg, gain, digitgain, dev->run_mode);

	/* We want to double exposure time for snapshot because preview uses binning sum mode */
	if (dev->run_mode == CI_MODE_STILL_CAPTURE) {
		coarse_itg *=2;
	}
	
	// Digital gain must be at least 0x100 for IMX111
	if (digitgain<0x100)
		digitgain = 0x100;

	/* enable group hold */
	ret = imx111_raw_write_reg_array(client, imx111_raw_param_hold);
	if (ret)
		goto out;

	/* imx111_raw max exposure is VTS-4 */
	if (coarse_itg > dev->lines_per_frame - IMX111_RAW_INTEGRATION_TIME_MARGIN)
		vts = coarse_itg + IMX111_RAW_INTEGRATION_TIME_MARGIN;

	ret = imx111_raw_write_reg(client, IMX111_RAW_16BIT, IMX111_RAW_FRAME_LENGTH_LINES, vts);
	if (ret)
		return ret;

	ret = imx111_raw_write_reg(client, IMX111_RAW_16BIT,
		IMX111_RAW_COARSE_INTEGRATION_TIME, coarse_itg);
	if (ret)
		goto out;

	/* set global gain */
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
		IMX111_RAW_GLOBAL_GAIN, gain);
	if (ret)
		goto out_disable;

	/* digital gain: GR */
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ+1, digitgain & 0xFF);
	if (ret)
		return ret;
	/* digital gain: R */
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ+2, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ+3, digitgain & 0xFF);
	if (ret)
		return ret;
	/*  digital gain: B */
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ+4, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ+5, digitgain & 0xFF);
	if (ret)
		return ret;
	/* digital gain: GB */
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ+6, (digitgain >> 8) & 0xFF);
	if (ret)
		return ret;
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_DGC_ADJ+7, digitgain & 0xFF);
	if (ret)
		return ret;

	dev->gain = gain;
	dev->digital_gain = digitgain;
	dev->coarse_itg = coarse_itg;

out_disable:
	/* disable group hold */
	imx111_raw_write_reg_array(client, imx111_raw_param_update);
out:
	return ret;
}

static int imx111_raw_set_exposure(struct v4l2_subdev *sd, int exposure,
	int gain, int digitgain)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	int ret;

	mutex_lock(&dev->input_lock);
	ret = __imx111_raw_set_exposure(sd, exposure, gain, digitgain);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long imx111_raw_s_exposure(struct v4l2_subdev *sd,
			       struct atomisp_exposure *exposure)
{
	unsigned int exp = exposure->integration_time[0];
	unsigned int gain = exposure->gain[0];
	unsigned int digitgain = exposure->gain[1];

	return imx111_raw_set_exposure(sd, exp, gain, digitgain);
}

static int imx111_raw_g_priv_int_data(struct v4l2_subdev *sd,
				   struct v4l2_private_int_data *priv)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	u8 __user *to = priv->data;
	u32 read_size = priv->size;
	int ret;

	/* No need to copy data if size is 0 */
	if (!read_size)
		goto out;

	if (IS_ERR(dev->otp_data)) {
		dev_err(&client->dev, "OTP data not available");
		return PTR_ERR(dev->otp_data);
	}
	/* Correct read_size value only if bigger than maximum */
	if (read_size > IMX111_RAW_OTP_DATA_SIZE)
		read_size = IMX111_RAW_OTP_DATA_SIZE;

	ret = copy_to_user(to, dev->otp_data, read_size);
	if (ret) {
		dev_err(&client->dev, "%s: failed to copy OTP data to user\n",
			 __func__);
		return -EFAULT;
	}
out:
	/* Return correct size */
	priv->size = IMX111_RAW_OTP_DATA_SIZE;

	return 0;
}

static int imx111_raw_init_registers(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = imx111_raw_write_reg_array(client, imx111_raw_SwReset);
	ret |= imx111_raw_write_reg_array(client, imx111_raw_init_settings);

	return ret;
}

static int __imx111_raw_init(struct v4l2_subdev *sd, u32 val)
{
	int ret;

	/* restore settings */
	imx111_raw_res = imx111_raw_res_preview;
	N_RES = N_RES_PREVIEW;

	ret = imx111_raw_init_registers(sd);

	return ret;
}

static int imx111_raw_init(struct v4l2_subdev *sd, u32 val)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	int ret = 0;

	mutex_lock(&dev->input_lock);
	ret = __imx111_raw_init(sd, val);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static long imx111_raw_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{

	switch (cmd) {
	case ATOMISP_IOC_S_EXPOSURE:
		return imx111_raw_s_exposure(sd, arg);
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
		return imx111_raw_g_priv_int_data(sd, arg);
	default:
		return -EINVAL;
	}
	return 0;
}

static int power_up(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	int ret;

       /* power control */
	ret = dev->platform_data->power_ctrl(sd, 1);
	if (ret)
		goto fail_power;

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 1);
	if (ret) {
		dev_err(&client->dev, "gpio failed\n");
		goto fail_gpio;
	}

	/* flis clock control */
	ret = dev->platform_data->flisclk_ctrl(sd, 1);
	if (ret)
		goto fail_clk;

	if (HW_ID == 0xFF){
		HW_ID = Read_HW_ID();
	}

	switch (HW_ID) {
		case HW_ID_SR1:
			msleep(600);
			break;
		case HW_ID_SR2:
		case HW_ID_ER:		
		case HW_ID_PR:
		case HW_ID_MP:
		default:
			msleep(20);
			break;
	}

	return 0;
fail_gpio:
	dev->platform_data->gpio_ctrl(sd, 0);
fail_clk:
	dev->platform_data->flisclk_ctrl(sd, 0);
fail_power:
	dev->platform_data->power_ctrl(sd, 0);
	dev_err(&client->dev, "sensor power-up failed\n");

	return ret;
}

static int power_down(struct v4l2_subdev *sd)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	ret = dev->platform_data->flisclk_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "flisclk failed\n");

	/* gpio ctrl */
	ret = dev->platform_data->gpio_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "gpio failed\n");

	/* power control */
	ret = dev->platform_data->power_ctrl(sd, 0);
	if (ret)
		dev_err(&client->dev, "vprog failed.\n");

	return ret;
}

static int __imx111_raw_s_power(struct v4l2_subdev *sd, int on)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	int ret, r;

	if (on == 0) {
		ret = power_down(sd);
		dev->power = 0;
	} else {
		ret = dw9714_vcm_init(&dev->sd);
		ret = power_up(sd);
		if (!ret) {
			dev->power = 1;
			return __imx111_raw_init(sd, 0);
		}
	}

	return ret;
}

static int imx111_raw_s_power(struct v4l2_subdev *sd, int on)
{
	int ret;
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	mutex_lock(&dev->input_lock);
	ret = __imx111_raw_s_power(sd, on);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int imx111_raw_g_chip_ident(struct v4l2_subdev *sd,
				struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!chip)
		return -EINVAL;

	v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_IMX111_RAW, 0);

	return 0;
}

static int imx111_raw_get_intg_factor(struct i2c_client *client,
				struct camera_mipi_info *info,
				const struct imx111_raw_reg *reglist)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	u32 pre_pll_clk_div;
	u32 pll_multiplier;
	u32 rpgltd;
	u32 post_pll_clk_div;
	u32 ccp2_data_format;

	const int ext_clk_freq_hz = 19200000;
	struct atomisp_sensor_mode_data buf;
	int vt_pix_clk_freq_mhz, ret = 0;
	u16 data[IMX111_RAW_INTG_BUF_COUNT];

	u32 coarse_integration_time_min;
	u32 coarse_integration_time_max_margin;
	u32 frame_length_lines;
	u32 line_length_pck;
	u32 read_mode;
	u32 div;

	if (info == NULL) {
		ret = -EINVAL;
		goto out;
	}

	memset(data, 0, IMX111_RAW_INTG_BUF_COUNT * sizeof(u16));
	ret = imx111_raw_read_reg(client, 1, IMX111_RAW_PRE_PLL_CLK_DIV, data, 0);
	if (ret)
		goto out;
	pre_pll_clk_div = data[0];
	ret = imx111_raw_read_reg(client, 1, IMX111_RAW_PLL_MULTIPLIER, data, 0);
	if (ret)
		goto out;
	pll_multiplier = data[0];
	ret = imx111_raw_read_reg(client, 1, IMX111_RAW_RGPLTD, data, 0);
	if (ret)
		goto out;
	rpgltd = data[0] & IMX111_RAW_MASK_2BIT;
	ret = imx111_raw_read_reg(client, 1, IMX111_RAW_CCP2_DATA_FORMAT, data, 0);
	if (ret)
		goto out;
	ccp2_data_format = data[0];

	printk("%s pre_pll_clk_div=%d pll_multiplier=%d rpgltd=%d ccp2_data_format=%d\n", __func__, pre_pll_clk_div, pll_multiplier, rpgltd, ccp2_data_format);


	memset(data, 0, IMX111_RAW_INTG_BUF_COUNT * sizeof(u16));
	ret = imx111_raw_read_reg(client, 4, IMX111_RAW_FRAME_LENGTH_LINES, data, 0);
	if (ret)
		return ret;
	frame_length_lines = data[0];
	line_length_pck = data[1];
	memset(data, 0, IMX111_RAW_INTG_BUF_COUNT * sizeof(u16));
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_COARSE_INTG_TIME_MIN, data, 0);
	if (ret)
		goto out;
	coarse_integration_time_min = data[0];
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_COARSE_INTG_TIME_MAX, data, 0);
	if (ret)
		goto out;
	coarse_integration_time_max_margin = data[0];
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_CROP_X_START, data, 0);
	if (ret)
		goto out;
	buf.crop_horizontal_start = data[0];
//	printk("%s crop_horizontal_start=%d\n", __func__, buf.crop_horizontal_start);
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_CROP_X_END, data, 0);
	if (ret)
		goto out;
	buf.crop_horizontal_end = data[0];
//	printk("%s crop_horizontal_end=%d\n", __func__, buf.crop_horizontal_end);
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_CROP_Y_START, data, 0);
	if (ret)
		goto out;
	buf.crop_vertical_start = data[0];
//	printk("%s crop_vertical_start=%d\n", __func__, buf.crop_vertical_start);
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_CROP_Y_END, data, 0);
	if (ret)
		goto out;
	buf.crop_vertical_end = data[0];
//	printk("%s crop_vertical_end=%d\n", __func__, buf.crop_vertical_end);
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_OUTPUT_WIDTH, data, 0);
	if (ret)
		goto out;
	buf.output_width = data[0];
//	printk("%s output_width=%d\n", __func__, buf.output_width);
	ret = imx111_raw_read_reg(client, 2, IMX111_RAW_OUTPUT_HEIGHT, data, 0);
	if (ret)
		goto out;
	buf.output_height = data[0];
//	printk("%s output_height=%d\n", __func__, buf.output_height);

	memset(data, 0, IMX111_RAW_INTG_BUF_COUNT * sizeof(u16));
	ret = imx111_raw_read_reg(client, 1, IMX111_RAW_READ_MODE, data, 0);
	if (ret)
		goto out;
	read_mode = data[0] & IMX111_RAW_MASK_2BIT;

	if (rpgltd == 0)
	{
		post_pll_clk_div = 2;
	}
	else if (rpgltd == 1)
	{
		post_pll_clk_div = 4;
	}
	else
	{
		post_pll_clk_div = 1;
	}

	div = pre_pll_clk_div*post_pll_clk_div*ccp2_data_format;
	if (div == 0) {
		ret = -EINVAL;
		goto out;
	}
	/*
	 * imx111_raw uses vt_pix_clk * 2 for calculations, i.e. integration
	 * time: coarse_int_time * line_length_pck/(2*vt_pix_clk).
	 * Provide correct value to user.
	 */
	vt_pix_clk_freq_mhz = ext_clk_freq_hz / div * pll_multiplier * 2;

	dev->vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
	buf.coarse_integration_time_min = coarse_integration_time_min;
	buf.coarse_integration_time_max_margin
		= coarse_integration_time_max_margin;
	buf.fine_integration_time_min = IMX111_RAW_FINE_INTG_TIME;
	buf.fine_integration_time_max_margin = IMX111_RAW_FINE_INTG_TIME;
	buf.fine_integration_time_def = IMX111_RAW_FINE_INTG_TIME;
	buf.vt_pix_clk_freq_mhz = vt_pix_clk_freq_mhz;
//	printk("%s vt_pix_clk_freq_mhz=%d\n", __func__, buf.vt_pix_clk_freq_mhz);
	buf.line_length_pck = line_length_pck;
//	printk("%s line_length_pck=%d\n", __func__, buf.line_length_pck);
	buf.frame_length_lines = frame_length_lines;
//	printk("%s frame_length_lines=%d\n", __func__, buf.frame_length_lines);
	buf.read_mode = read_mode;

	buf.binning_factor_x = imx111_raw_res[dev->fmt_idx].bin_factor_x;
	buf.binning_factor_y = imx111_raw_res[dev->fmt_idx].bin_factor_y;
//	printk("%s binning x=%d y=%d\n", __func__, buf.binning_factor_x, buf.binning_factor_y);

	memcpy(&info->data, &buf, sizeof(buf));

out:
	return ret;
}

/* This returns the exposure time being used. This should only be used
   for filling in EXIF data, not for actual image processing. */
static int imx111_raw_q_exposure(struct v4l2_subdev *sd, s32 *value)
{
//	printk("%s\n", __func__);

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 coarse;
	int ret;

	/* the fine integration time is currently not calculated */
	ret = imx111_raw_read_reg(client, IMX111_RAW_16BIT,
			       IMX111_RAW_COARSE_INTEGRATION_TIME, &coarse, 0);
	*value = coarse;

	return ret;
}

static int imx111_raw_test_pattern(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return imx111_raw_write_reg(client, IMX111_RAW_16BIT,
			IMX111_RAW_TEST_PATTERN_MODE, value);
}

static int imx111_raw_v_flip(struct v4l2_subdev *sd, s32 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	u16 val;

	ret = imx111_raw_write_reg_array(client, imx111_raw_param_hold);
	if (ret)
		return ret;
	ret = imx111_raw_read_reg(client, IMX111_RAW_8BIT, IMX111_RAW_IMG_ORIENTATION, &val, 0);
	if (ret)
		return ret;
	if (value)
		val |= IMX111_RAW_VFLIP_BIT;
	else
		val &= ~IMX111_RAW_VFLIP_BIT;
	ret = imx111_raw_write_reg(client, IMX111_RAW_8BIT,
			IMX111_RAW_IMG_ORIENTATION, val);
	if (ret)
		return ret;
	return imx111_raw_write_reg_array(client, imx111_raw_param_update);
}

static int imx111_raw_g_focal(struct v4l2_subdev *sd, s32 *val)
{
	*val = (IMX111_RAW_FOCAL_LENGTH_NUM << 16) | IMX111_RAW_FOCAL_LENGTH_DEM;
	return 0;
}

static int imx111_raw_g_fnumber(struct v4l2_subdev *sd, s32 *val)
{
	/*const f number for imx111_raw*/
	*val = (IMX111_RAW_F_NUMBER_DEFAULT_NUM << 16) | IMX111_RAW_F_NUMBER_DEM;
	return 0;
}

static int imx111_raw_g_fnumber_range(struct v4l2_subdev *sd, s32 *val)
{
	*val = (IMX111_RAW_F_NUMBER_DEFAULT_NUM << 24) |
		(IMX111_RAW_F_NUMBER_DEM << 16) |
		(IMX111_RAW_F_NUMBER_DEFAULT_NUM << 8) | IMX111_RAW_F_NUMBER_DEM;
	return 0;
}

static int imx111_raw_g_bin_factor_x(struct v4l2_subdev *sd, s32 *val)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	*val = imx111_raw_res[dev->fmt_idx].bin_factor_x >> 1;

	return 0;
}

static int imx111_raw_g_bin_factor_y(struct v4l2_subdev *sd, s32 *val)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	*val = imx111_raw_res[dev->fmt_idx].bin_factor_y >> 1;

	return 0;
}

struct imx111_raw_control imx111_raw_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_EXPOSURE_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "exposure",
			.minimum = 0x0,
			.maximum = 0xffff,
			.step = 0x01,
			.default_value = 0x00,
			.flags = 0,
		},
		.query = imx111_raw_q_exposure,
	},
	{
		.qc = {
			.id = V4L2_CID_TEST_PATTERN,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Test pattern",
			.minimum = 0,
			.maximum = 0xffff,
			.step = 1,
			.default_value = 0,
		},
		.tweak = imx111_raw_test_pattern,
	},
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = imx111_raw_v_flip,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move absolute",
			.minimum = 0,
			.maximum = IMX111_RAW_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = dw9714_t_focus_abs,
		.query = dw9714_q_focus_abs,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_RELATIVE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus move relative",
			.minimum = IMX111_RAW_MAX_FOCUS_NEG,
			.maximum = IMX111_RAW_MAX_FOCUS_POS,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.tweak = dw9714_t_focus_rel,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCUS_STATUS,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focus status",
			.minimum = 0,
			.maximum = 100, /* allow enum to grow in the future */
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = dw9714_q_focus_status,
	},
	{
		.qc = {
			.id = V4L2_CID_FOCAL_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "focal length",
			.minimum = IMX111_RAW_FOCAL_LENGTH_DEFAULT,
			.maximum = IMX111_RAW_FOCAL_LENGTH_DEFAULT,
			.step = 0x01,
			.default_value = IMX111_RAW_FOCAL_LENGTH_DEFAULT,
			.flags = 0,
		},
		.query = imx111_raw_g_focal,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_ABSOLUTE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number",
			.minimum = IMX111_RAW_F_NUMBER_DEFAULT,
			.maximum = IMX111_RAW_F_NUMBER_DEFAULT,
			.step = 0x01,
			.default_value = IMX111_RAW_F_NUMBER_DEFAULT,
			.flags = 0,
		},
		.query = imx111_raw_g_fnumber,
	},
	{
		.qc = {
			.id = V4L2_CID_FNUMBER_RANGE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "f-number range",
			.minimum = IMX111_RAW_F_NUMBER_RANGE,
			.maximum =  IMX111_RAW_F_NUMBER_RANGE,
			.step = 0x01,
			.default_value = IMX111_RAW_F_NUMBER_RANGE,
			.flags = 0,
		},
		.query = imx111_raw_g_fnumber_range,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_HORZ,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "horizontal binning factor",
			.minimum = 0,
			.maximum = IMX111_RAW_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = imx111_raw_g_bin_factor_x,
	},
	{
		.qc = {
			.id = V4L2_CID_BIN_FACTOR_VERT,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "vertical binning factor",
			.minimum = 0,
			.maximum = IMX111_RAW_BIN_FACTOR_MAX,
			.step = 1,
			.default_value = 0,
			.flags = 0,
		},
		.query = imx111_raw_g_bin_factor_y,
	},
};
#define N_CONTROLS (ARRAY_SIZE(imx111_raw_controls))

static struct imx111_raw_control *imx111_raw_find_control(u32 id)
{
	int i;

	for (i = 0; i < N_CONTROLS; i++)
		if (imx111_raw_controls[i].qc.id == id)
			return &imx111_raw_controls[i];
	return NULL;
}

static int imx111_raw_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	struct imx111_raw_control *ctrl = imx111_raw_find_control(qc->id);
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	if (ctrl == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*qc = ctrl->qc;
	mutex_unlock(&dev->input_lock);

	return 0;
}

/* imx111_raw control set/get */
static int imx111_raw_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct imx111_raw_control *s_ctrl;
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	int ret;

	if (!ctrl)
		return -EINVAL;

	s_ctrl = imx111_raw_find_control(ctrl->id);
	if ((s_ctrl == NULL) || (s_ctrl->query == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = s_ctrl->query(sd, &ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

static int imx111_raw_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct imx111_raw_control *octrl = imx111_raw_find_control(ctrl->id);
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	int ret;

	if ((octrl == NULL) || (octrl->tweak == NULL))
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	ret = octrl->tweak(sd, ctrl->value);
	mutex_unlock(&dev->input_lock);

	return ret;
}

/*
 * distance - calculate the distance
 * @res: resolution
 * @w: width
 * @h: height
 *
 * Get the gap between resolution and w/h.
 * res->width/height smaller than w/h wouldn't be considered.
 * Returns the value of gap or -1 if fail.
 */
#define LARGEST_ALLOWED_RATIO_MISMATCH  600
static int distance(struct imx111_raw_resolution *res, u32 w, u32 h)
{
	unsigned int w_ratio = ((res->width << 13)/w);
	unsigned int h_ratio;
	int match;

	if (h == 0)
		return -1;
	h_ratio = ((res->height << 13) / h);
	if (h_ratio == 0)
		return -1;
	match   = abs(((w_ratio << 13) / h_ratio) - ((int)8192));

	if ((w_ratio < (int)8192) || (h_ratio < (int)8192)  ||
		(match > LARGEST_ALLOWED_RATIO_MISMATCH))
		return -1;

	return w_ratio + h_ratio;
}

/* Return the nearest higher resolution index */
static int nearest_resolution_index(int w, int h)
{
	int i;
	int idx = -1;
	int dist;
	int min_dist = INT_MAX;
	struct imx111_raw_resolution *tmp_res = NULL;

	for (i = 0; i < N_RES; i++) {
		tmp_res = &imx111_raw_res[i];
		dist = distance(tmp_res, w, h);
		if (dist == -1)
			continue;
		if (dist < min_dist) {
			min_dist = dist;
			idx = i;
		}
	}

	return idx;
}

static int get_resolution_index(int w, int h)
{
	int i;

	for (i = 0; i < N_RES; i++) {
		if (w != imx111_raw_res[i].width)
			continue;
		if (h != imx111_raw_res[i].height)
			continue;
		/* Found it */
		return i;
	}
	return -1;
}

static int imx111_raw_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	int idx = 0;

	if ((fmt->width > IMX111_RAW_RES_WIDTH_MAX)
		|| (fmt->height > IMX111_RAW_RES_HEIGHT_MAX)) {
		fmt->width = IMX111_RAW_RES_WIDTH_MAX;
		fmt->height = IMX111_RAW_RES_HEIGHT_MAX;
	} else {
		idx = nearest_resolution_index(fmt->width, fmt->height);

		/*
		 * nearest_resolution_index() doesn't return smaller
		 *  resolutions. If it fails, it means the requested
		 *  resolution is higher than wecan support. Fallback
		 *  to highest possible resolution in this case.
		 */
		if (idx == -1)
			idx = N_RES - 1;

		fmt->width = imx111_raw_res[idx].width;
		fmt->height = imx111_raw_res[idx].height;
	}
	fmt->code = V4L2_MBUS_FMT_SRGGB10_1X10;

	return 0;
}

static int imx111_raw_s_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	const struct imx111_raw_reg *imx111_raw_def_reg;
	struct camera_mipi_info *imx111_raw_info = NULL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	imx111_raw_info = v4l2_get_subdev_hostdata(sd);
	if (imx111_raw_info == NULL)
		return -EINVAL;
	ret = imx111_raw_try_mbus_fmt(sd, fmt);
	if (ret) {
		v4l2_err(sd, "try fmt fail\n");
		return ret;
	}

	mutex_lock(&dev->input_lock);
	dev->fmt_idx = get_resolution_index(fmt->width, fmt->height);

	/* Sanity check */
	if (unlikely(dev->fmt_idx == -1)) {
		v4l2_err(sd, "get resolution fail\n");
		ret = -EINVAL;
		goto out;
	}

	imx111_raw_def_reg = imx111_raw_res[dev->fmt_idx].regs;
	printk("%s RES %s selected\n", __func__, imx111_raw_res[dev->fmt_idx].desc);

	ret = imx111_raw_write_reg_array(client, imx111_raw_def_reg);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}
	/* FIXME: workround for MERR Pre-alpha due to ISP perf - start */

	ret = imx111_raw_write_reg_array(client, imx111_raw_param_update);
	if (ret) {
		ret = -EINVAL;
		goto out;
	}

	dev->fps = imx111_raw_res[dev->fmt_idx].fps;
	dev->pixels_per_line = imx111_raw_res[dev->fmt_idx].pixels_per_line;
	dev->lines_per_frame = imx111_raw_res[dev->fmt_idx].lines_per_frame;
/*
	ret = __imx111_raw_set_exposure(sd, dev->coarse_itg, dev->gain,
				 dev->digital_gain);
	if (ret)
		goto out;
*/
	ret = imx111_raw_get_intg_factor(client, imx111_raw_info, imx111_raw_def_reg);
	if (ret) {
		v4l2_err(sd, "failed to get integration_factor\n");
		goto out;
	}
out:
	mutex_unlock(&dev->input_lock);
	return ret;
}


static int imx111_raw_g_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	if (!fmt)
		return -EINVAL;

	fmt->width = imx111_raw_res[dev->fmt_idx].width;
	fmt->height = imx111_raw_res[dev->fmt_idx].height;
	fmt->code = V4L2_MBUS_FMT_SRGGB10_1X10;

	return 0;
}

static int imx111_raw_detect(struct i2c_client *client, u16 *id, u8 *revision)
{
	struct i2c_adapter *adapter = client->adapter;
	u16 high, low;

	ATD_imx111_raw_status = 0;	//Add for ATD command+++

	/* i2c check */
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -ENODEV;

	/* check sensor chip ID	 */
	if (imx111_raw_read_reg(client, IMX111_RAW_8BIT, IMX111_RAW_SC_CMMN_CHIP_ID_H, &high, 0)) {
		v4l2_err(client, "sensor_id_high = 0x%x\n", high);
		return -ENODEV;
	}

	if (imx111_raw_read_reg(client, IMX111_RAW_8BIT, IMX111_RAW_SC_CMMN_CHIP_ID_L, &low, 0)) {
		v4l2_err(client, "sensor_id_low = 0x%x\n", low);
		return -ENODEV;
	}

	*id = (((u8) high) << 8) | (u8) low;
	v4l2_info(client, "sensor_id = 0x%x\n", *id);

	if (*id != IMX111_RAW_ID) {
		v4l2_err(client, "sensor ID error\n");
		return -ENODEV;
	}

	v4l2_info(client, "detect imx111_raw success\n");

	/* TODO - need to be updated */
	*revision = 0;

	ATD_imx111_raw_status = 1;	//Add for ATD command+++

	return 0;
}

/*
 * imx111_raw stream on/off
 */
static int imx111_raw_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	mutex_lock(&dev->input_lock);
	if (enable) {
		ret = imx111_raw_write_reg_array(client, imx111_raw_streaming);
		if (ret != 0) {
			v4l2_err(client, "write_reg_array err\n");
			mutex_unlock(&dev->input_lock);
			return ret;
		}
		dev->streaming = 1;
	} else {
		ret = imx111_raw_write_reg_array(client, imx111_raw_soft_standby);
		if (ret != 0) {
			v4l2_err(client, "write_reg_array err\n");
			mutex_unlock(&dev->input_lock);
			return ret;
		}
		dev->streaming = 0;
	}
	printk("%s stream = %d\n", __func__, enable);
	mutex_unlock(&dev->input_lock);

	return 0;
}

/*
 * imx111_raw enum frame size, frame intervals
 */
static int imx111_raw_enum_framesizes(struct v4l2_subdev *sd,
				   struct v4l2_frmsizeenum *fsize)
{
	unsigned int index = fsize->index;

	if (index >= N_RES)
		return -EINVAL;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = imx111_raw_res[index].width;
	fsize->discrete.height = imx111_raw_res[index].height;
	fsize->reserved[0] = imx111_raw_res[index].used;

	return 0;
}

static int imx111_raw_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	int i;

	/* since the isp will donwscale the resolution to the right size,
	  * find the nearest one that will allow the isp to do so
	  * important to ensure that the resolution requested is padded
	  * correctly by the requester, which is the atomisp driver in
	  * this case.
	  */
	i = nearest_resolution_index(fival->width, fival->height);

	if (i == -1)
		return -EINVAL;

	fival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	fival->width = imx111_raw_res[i].width;
	fival->height = imx111_raw_res[i].height;
	fival->discrete.numerator = 1;
	fival->discrete.denominator = imx111_raw_res[i].fps;

	return 0;
}

static int imx111_raw_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned int index,
				 enum v4l2_mbus_pixelcode *code)
{
	if (index >= MAX_FMTS)
		return -EINVAL;

	*code = V4L2_MBUS_FMT_SRGGB10_1X10;
	return 0;
}

static int imx111_raw_s_config(struct v4l2_subdev *sd,
			    int irq, void *pdata)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u8 sensor_revision;
	u16 sensor_id;
	int ret;
	if (pdata == NULL)
		return -ENODEV;

	dev->platform_data = pdata;

	mutex_lock(&dev->input_lock);

	if (dev->platform_data->platform_init) {
		ret = dev->platform_data->platform_init(client);
		if (ret) {
			mutex_unlock(&dev->input_lock);
			v4l2_err(client, "imx111_raw platform init err\n");
			return ret;
		}
	}
	ret = __imx111_raw_s_power(sd, 1);
	if (ret) {
		v4l2_err(client, "imx111_raw power-up err.\n");
		mutex_unlock(&dev->input_lock);
		goto fail_csi_cfg;
	}

	ret = dev->platform_data->csi_cfg(sd, 1);
	if (ret)
		goto fail_csi_cfg;

	/* config & detect sensor */
	ret = imx111_raw_detect(client, &sensor_id, &sensor_revision);
	if (ret) {
		v4l2_err(client, "imx111_raw_detect err s_config.\n");
		goto fail_detect;
	}

	dev->sensor_id = sensor_id;
	dev->sensor_revision = sensor_revision;

	/* Read sensor's OTP data */
	dev->otp_data = imx111_raw_otp_read(sd);

	/* power off sensor */
	ret = __imx111_raw_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	if (ret)
		v4l2_err(client, "imx111_raw power-down err.\n");

	return ret;

fail_detect:
	dev->platform_data->csi_cfg(sd, 0);
fail_csi_cfg:
	__imx111_raw_s_power(sd, 0);
	mutex_unlock(&dev->input_lock);
	dev_err(&client->dev, "sensor power-gating failed\n");
	return ret;
}

static int
imx111_raw_enum_mbus_code(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_mbus_code_enum *code)
{
	if (code->index >= MAX_FMTS)
		return -EINVAL;
	code->code = V4L2_MBUS_FMT_SRGGB10_1X10;

	return 0;
}

static int
imx111_raw_enum_frame_size(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_frame_size_enum *fse)
{
	int index = fse->index;

	if (index >= N_RES)
		return -EINVAL;

	fse->min_width = imx111_raw_res[index].width;
	fse->min_height = imx111_raw_res[index].height;
	fse->max_width = imx111_raw_res[index].width;
	fse->max_height = imx111_raw_res[index].height;

	return 0;
}

static struct v4l2_mbus_framefmt *
__imx111_raw_get_pad_format(struct imx111_raw_device *sensor,
			 struct v4l2_subdev_fh *fh, unsigned int pad,
			 enum v4l2_subdev_format_whence which)
{
	switch (which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		return v4l2_subdev_get_try_format(fh, pad);
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		return &sensor->format;
	default:
		return NULL;
	}
}

static int
imx111_raw_get_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	struct v4l2_mbus_framefmt *format =
			__imx111_raw_get_pad_format(dev, fh, fmt->pad, fmt->which);

	fmt->format = *format;

	return 0;
}

static int
imx111_raw_set_pad_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		       struct v4l2_subdev_format *fmt)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	if (fmt->which == V4L2_SUBDEV_FORMAT_ACTIVE)
		dev->format = fmt->format;

	return 0;
}

static int
imx111_raw_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	dev->run_mode = param->parm.capture.capturemode;
	mutex_lock(&dev->input_lock);
	switch (dev->run_mode) {
	case CI_MODE_VIDEO:
		imx111_raw_res = imx111_raw_res_video;
		N_RES = N_RES_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		imx111_raw_res = imx111_raw_res_still;
		N_RES = N_RES_STILL;
		break;
	default:
		imx111_raw_res = imx111_raw_res_preview;
		N_RES = N_RES_PREVIEW;
	}
	mutex_unlock(&dev->input_lock);
	return 0;
}

int
imx111_raw_g_frame_interval(struct v4l2_subdev *sd,
				struct v4l2_subdev_frame_interval *interval)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	u16 lines_per_frame;
	/*
	 * if no specific information to calculate the fps,
	 * just used the value in sensor settings
	 */

	if (!dev->pixels_per_line || !dev->lines_per_frame) {
		interval->interval.numerator = 1;
		interval->interval.denominator = dev->fps;
		return 0;
	}

	/*
	 * DS: if coarse_integration_time is set larger than
	 * lines_per_frame the frame_size will be expanded to
	 * coarse_integration_time+1
	 */
	if (dev->coarse_itg > dev->lines_per_frame) {
		if (dev->coarse_itg == 0xFFFF) {
			/*
			 * we can not add 4 according to ds, as this will
			 * cause over flow
			 */
			v4l2_warn(client, "%s: abnormal coarse_itg:0x%x\n",
				  __func__, dev->coarse_itg);
			lines_per_frame = dev->coarse_itg;
		} else {
			lines_per_frame = dev->coarse_itg + 4;
		}
	} else {
		lines_per_frame = dev->lines_per_frame;
	}
	interval->interval.numerator = dev->pixels_per_line *
					lines_per_frame;
	interval->interval.denominator = dev->vt_pix_clk_freq_mhz;

	return 0;
}

static int imx111_raw_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	if (frames == NULL)
		return -EINVAL;

	mutex_lock(&dev->input_lock);
	*frames = imx111_raw_res[dev->fmt_idx].skip_frames;
	mutex_unlock(&dev->input_lock);

	return 0;
}

static const struct v4l2_subdev_video_ops imx111_raw_video_ops = {
	.s_stream = imx111_raw_s_stream,
	.enum_framesizes = imx111_raw_enum_framesizes,
	.enum_frameintervals = imx111_raw_enum_frameintervals,
	.enum_mbus_fmt = imx111_raw_enum_mbus_fmt,
	.try_mbus_fmt = imx111_raw_try_mbus_fmt,
	.g_mbus_fmt = imx111_raw_g_mbus_fmt,
	.s_mbus_fmt = imx111_raw_s_mbus_fmt,
	.s_parm = imx111_raw_s_parm,
	.g_frame_interval = imx111_raw_g_frame_interval,
};

static struct v4l2_subdev_sensor_ops imx111_raw_sensor_ops = {
	.g_skip_frames	= imx111_raw_g_skip_frames,
};

static const struct v4l2_subdev_core_ops imx111_raw_core_ops = {
	.g_chip_ident = imx111_raw_g_chip_ident,
	.queryctrl = imx111_raw_queryctrl,
	.g_ctrl = imx111_raw_g_ctrl,
	.s_ctrl = imx111_raw_s_ctrl,
	.s_power = imx111_raw_s_power,
	.ioctl = imx111_raw_ioctl,
	.init = imx111_raw_init,
};

static const struct v4l2_subdev_pad_ops imx111_raw_pad_ops = {
	.enum_mbus_code = imx111_raw_enum_mbus_code,
	.enum_frame_size = imx111_raw_enum_frame_size,
	.get_fmt = imx111_raw_get_pad_format,
	.set_fmt = imx111_raw_set_pad_format,
};

static const struct v4l2_subdev_ops imx111_raw_ops = {
	.core = &imx111_raw_core_ops,
	.video = &imx111_raw_video_ops,
	.pad = &imx111_raw_pad_ops,
	.sensor = &imx111_raw_sensor_ops,
};

static const struct media_entity_operations imx111_raw_entity_ops = {
	.link_setup = NULL,
};

static int imx111_raw_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct imx111_raw_device *dev = to_imx111_raw_sensor(sd);

	if (dev->platform_data->platform_deinit)
		dev->platform_data->platform_deinit();

	media_entity_cleanup(&dev->sd.entity);
	dev->platform_data->csi_cfg(sd, 0);
	v4l2_device_unregister_subdev(sd);
	kfree(dev);

	return 0;
}

static int imx111_raw_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct imx111_raw_device *dev;
	int ret;

	/* allocate sensor device & init sub device */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		v4l2_err(client, "%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	create_vcm_proc_file(); //For DIT VCM Debug Interface+++

	//Add for ATD command+++
	dev->sensor_i2c_attribute.attrs = imx111_raw_attributes;

	// Register sysfs hooks
	ret = sysfs_create_group(&client->dev.kobj, &dev->sensor_i2c_attribute);
	if (ret) {
		dev_err(&client->dev, "Not able to create the sysfs\n");
		return ret;
	}
	//Add for ATD command---

	mutex_init(&dev->input_lock);

	dev->fmt_idx = 0;
	v4l2_i2c_subdev_init(&(dev->sd), client, &imx111_raw_ops);

	ret = dw9714_vcm_init(&dev->sd);
	if (ret < 0)
		goto out_free;

	if (client->dev.platform_data) {
		ret = imx111_raw_s_config(&dev->sd, client->irq,
				       client->dev.platform_data);
		if (ret)
			goto out_free;
	}

	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	dev->pad.flags = MEDIA_PAD_FL_SOURCE;
	dev->format.code = V4L2_MBUS_FMT_SRGGB10_1X10;
	dev->sd.entity.ops = &imx111_raw_entity_ops;
	dev->sd.entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;

	ret = media_entity_init(&dev->sd.entity, 1, &dev->pad, 0);
	if (ret)
		imx111_raw_remove(client);

	main_sd = &dev->sd;	//Add for ATD command+++

	return ret;
out_free:
	v4l2_device_unregister_subdev(&dev->sd);
	kfree(dev);
	return ret;
}

static const struct i2c_device_id imx111_raw_id[] = {
	{IMX111_RAW_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, imx111_raw_id);

static struct i2c_driver imx111_raw_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = IMX111_RAW_NAME,
	},
	.probe = imx111_raw_probe,
	.remove = imx111_raw_remove,
	.id_table = imx111_raw_id,
};


static __init int init_imx111_raw(void)
{
	return i2c_add_driver(&imx111_raw_driver);
}

static __exit void exit_imx111_raw(void)
{
	i2c_del_driver(&imx111_raw_driver);
}

module_init(init_imx111_raw);
module_exit(exit_imx111_raw);

MODULE_DESCRIPTION("A low-level driver for Sony IMX111_RAW sensors");
MODULE_AUTHOR("Shenbo Huang <shenbo.huang@intel.com>");
MODULE_LICENSE("GPL");

