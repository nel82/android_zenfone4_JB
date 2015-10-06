/*
 * Copyright (c)  2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicensen
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "displays/sn65dsi86_vid.h"
#include "mdfld_dsi_dpi.h"
#include "mdfld_dsi_pkg_sender.h"
#include <linux/gpio.h>
#include "psb_drv.h"
#include <linux/lnw_gpio.h>
#include <asm/intel_scu_pmic.h>
#include <linux/mutex.h>
#include <linux/HWVersion.h>
#include <linux/ite8566.h>
#include "asus_panel_id.h"

#define SN65DSI86_BRIDGE_NAME	"SN65DSI86-BRIDGE"
#define TI_EDP_BRIDGE_I2C_ADAPTER	2
#define TI_EDP_BRIDGE_I2C_ADDR	0x2C

#define DP_BRDG_EN 63        //GP_AON_063
#define LCD_LOGIC_PWR_EN 108 //GP_CORE_012 = 12+96 (SOC gpio, panel logic power)
#define EN_VDD_BL 0x7E       //BACKLIGHT_EN(PMIC, panel led driver)
#define PCB_ID3 112          //GP_CORE_016 = 16+96

#define PWM_ENABLE_GPIO 49
#define PWM_BASE_UNIT 0x32 //228.4Hz

//#define BRIDGE_IRQ
struct mutex switch_mutex;

static int hw_id;
extern int Read_HW_ID(void);
static int pcb_id;
extern int Read_PCB_ID(void);

static struct i2c_client *sn65dsi86_client;
static int panel_id; // 0:Innolux(1920*1080), 1:BOE(1366*768)
static int pad_power;
static int pad_prev_level = 0;
static int prev_owner;
static int owner_id; // 0:Android, 1:Windows8
static int request_gpio49 = 0;
static bool aux_fail = false;

#ifdef CONFIG_HID_ASUS_PAD_EC
//switch from Android to Win8
extern int register_dock_atow_early_notifier(struct notifier_block *nb);
extern int unregister_dock_atow_early_notifier(struct notifier_block *nb);
//switch from Win8 to Android
extern int register_dock_wtoa_late_notifier(struct notifier_block *nb);
extern int unregister_dock_wtoa_late_notifier(struct notifier_block *nb);
//detach
extern int register_dock_detach_notifier(struct notifier_block *nb);
extern int unregister_dock_detach_notifier(struct notifier_block *nb);
//MUX owner
extern int ite_panel_owner_notify(void);

static int mux_owner_notify(struct notifier_block *this, unsigned long code, void *data);

static struct notifier_block display_atow_early_notifier = {
        .notifier_call =    mux_owner_notify,
};
static struct notifier_block display_wtoa_late_notifier = {
        .notifier_call =    mux_owner_notify,
};
static struct notifier_block display_detach_notifier = {
        .notifier_call =    mux_owner_notify,
};
#endif

static u8 panel_assr;
static u8 sn65dsi86_rev;
static ssize_t sn65dsi86_bridge_rev_show(struct device *class, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%x\n", sn65dsi86_rev);
}
DEVICE_ATTR(bridge_rev, (S_IWUSR | S_IRUGO), sn65dsi86_bridge_rev_show, NULL);

static struct attribute *sn65dsi86_attr[] = {
	&dev_attr_bridge_rev.attr,
	NULL
};

static struct attribute_group sn65dsi86_attr_group = {
        .attrs = sn65dsi86_attr,
        .name = "sn65dsi86",
};

union sst_pwmctrl_reg {
	struct {
		u32 pwmtd:8;
		u32 pwmbu:22;
		u32 pwmswupdate:1;
		u32 pwmenable:1;
	} part;
	u32 full;
};

static void __iomem *pwmctrl_mmio;

static int pwm_configure(int duty)
{
	union sst_pwmctrl_reg pwmctrl;

	/*Read the PWM register to make sure there is no pending
	*update.
	*/
	pwmctrl.full = readl(pwmctrl_mmio);

	/*check pwnswupdate bit */
	if (pwmctrl.part.pwmswupdate)
		return -EBUSY;
	pwmctrl.part.pwmswupdate = 0x1;
	pwmctrl.part.pwmbu = PWM_BASE_UNIT;
	pwmctrl.part.pwmtd = duty;
	writel(pwmctrl.full, pwmctrl_mmio);

	return 0;
}

static void pwm_enable(void) {
	union sst_pwmctrl_reg pwmctrl;

	lnw_gpio_set_alt(PWM_ENABLE_GPIO, LNW_ALT_2);
	/*Enable the PWM by setting PWM enable bit to 1 */
	pwmctrl.full = readl(pwmctrl_mmio);
	pwmctrl.part.pwmenable = 1;
	writel(pwmctrl.full, pwmctrl_mmio);
}

static void pwm_disable(void) {
	union sst_pwmctrl_reg pwmctrl;
	/*setting PWM enable bit to 0 */
	pwmctrl.full = readl(pwmctrl_mmio);
	pwmctrl.part.pwmenable = 0;
	writel(pwmctrl.full, pwmctrl_mmio);

	gpio_set_value(PWM_ENABLE_GPIO, 0);
	lnw_gpio_set_alt(PWM_ENABLE_GPIO, 0);
}

struct work_struct irq_workq;

static int sn65dsi86_reg_write(struct i2c_client *client, u8 reg, u8 value)
{
	int r;
	u8 tx_data[] = {
		reg & 0xff,
		value & 0xff,
	};
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = tx_data,
			.len = ARRAY_SIZE(tx_data),
		},
	};

	r = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (r < 0) {
		dev_err(&client->dev, "%s: reg 0x%04x val 0x%08x error %d\n",
			__func__, reg, value, r);
		return r;
	}

	if (r < ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "%s: reg 0x%04x val 0x%08x msgs %d\n",
			__func__, reg, value, r);
		return -EAGAIN;
	}

	return 0;
}

/**
 * sn65dsi86_reg_read - Read DSI-eDP bridge register using I2C
 * @client: struct i2c_client to use
 * @reg: register address
 * @value: pointer for storing the value
 *
 * Returns 0 on success, or a negative error value.
 */
static int sn65dsi86_reg_read(struct i2c_client *client, u8 reg, u8 *value)
{
	int r;
	u8 tx_data[] = {
		reg & 0xff,
	};
	u8 rx_data[1];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.buf = tx_data,
			.len = ARRAY_SIZE(tx_data),
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.buf = rx_data,
			.len = ARRAY_SIZE(rx_data),
		 },
	};

	r = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (r < 0) {
		dev_err(&client->dev, "%s: reg 0x%04x error %d\n", __func__,
			reg, r);
		return r;
	}

	if (r < ARRAY_SIZE(msgs)) {
		dev_err(&client->dev, "%s: reg 0x%04x msgs %d\n", __func__,
			reg, r);
		return -EAGAIN;
	}

	*value = rx_data[0];

	dev_dbg(&client->dev, "%s: reg 0x%04x value 0x%08x\n", __func__,
		reg, *value);

	return 0;
}

// This function will read from the DPCD register of sink.
// auxaddr: 20bit DPCD address.
// Returns:  byte from sink.
static int sn65dsi86_dpcd_read(int auxaddr)
{
	struct i2c_client *i2c = sn65dsi86_client;
	u8 r = 0;
	int rty = 0;
	int timeout = 0;

dpcd_rd_err:

	/* Clear Status Registers */
	sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, 0xFF);

	/* Send AUX READ REQUEST */
	sn65dsi86_reg_write(i2c, DSI86_AUXADDR_HI, (auxaddr & 0xF0000) >> 16);
	sn65dsi86_reg_write(i2c, DSI86_AUXADDR_MID, (auxaddr & 0xFF00) >> 8);
	sn65dsi86_reg_write(i2c, DSI86_AUXADDR_LO, auxaddr & 0xFF);
	sn65dsi86_reg_write(i2c, DSI86_AUXLEN, 0x1);
	sn65dsi86_reg_write(i2c, DSI86_AUX_CMD_SEND, 0x91);

	usleep_range(100, 100);

	/* POLL SEND_INT */
	do {
		sn65dsi86_reg_read(i2c, DSI86_AUX_STATUS, &r);
		//printk(KERN_INFO "[DISPLAY] AUX_STATUS = %x \n", r);
		timeout++;
	} while ((r == 0) && (timeout < 3));

	if ((r & 0x59) != 0x01)
	{
		rty++;
		if (rty <= 7)
		{
			printk(KERN_INFO "DSI86: AUX DPCD READ FAILURE. 0x%x  TRY AGAIN\n", r);
			//sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, r);
			goto dpcd_rd_err;
		}
		else
		{
			printk(KERN_INFO "DSI86: AUX DPCD READ FAILURE. 0x%x  RETRY more than 7 times\n", r);
			//sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, r);
			return 0;
		}
	}
	else
	{
		//printk(KERN_INFO "DSI86: AUX DPCD READ success!\n", r);
		//sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, r);
	}

    sn65dsi86_reg_read(i2c, DSI86_AUX_RDATA0, &r);
	return r;
}

// This function will write to the DPCD register of sink.
// auxaddr: 20bit DPCD address.
// writedata:  byte to be written.
static int sn65dsi86_dpcd_write(int auxaddr, int writedata)
{
	struct i2c_client *i2c = sn65dsi86_client;
	u8 r = 0;
	int rty = 0;
	int timeout = 0;

dpcd_wr_err:
	/* Clear Status Registers */
	sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, 0xFF);

	/* Send AUX WRITE REQUEST */
	sn65dsi86_reg_write(i2c, DSI86_AUX_WDATA0, writedata);
	sn65dsi86_reg_write(i2c, DSI86_AUXADDR_HI, (auxaddr & 0xF0000) >> 16);
	sn65dsi86_reg_write(i2c, DSI86_AUXADDR_MID, (auxaddr & 0xFF00) >> 8);
	sn65dsi86_reg_write(i2c, DSI86_AUXADDR_LO, auxaddr & 0xFF);
	sn65dsi86_reg_write(i2c, DSI86_AUXLEN, 0x1);
	sn65dsi86_reg_write(i2c, DSI86_AUX_CMD_SEND, 0x81);

	usleep_range(100, 100);

	/* POLL SEND_INT */
	do {
		sn65dsi86_reg_read(i2c, DSI86_AUX_STATUS, &r);
		//printk(KERN_INFO "[DISPLAY] AUX_STATUS = %x \n", r);
		timeout++;
	} while ((r == 0) && (timeout < 3));

	if ((r & 0x59) != 0x01)
	{
		rty++;
		if (rty <= 7)
		{
			printk(KERN_INFO "DSI86: AUX DPCD WRITE FAILURE. 0x%x  TRY AGAIN\n", r);
			//sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, r);
			goto dpcd_wr_err;
		}
		else
		{
			printk(KERN_INFO "DSI86: AUX DPCD WRITE FAILURE. 0x%x  RETRY more than 7 times\n", r);
			//sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, r);
			return -EINVAL;
		}
	}
	else
	{
		//printk(KERN_INFO "DSI86: AUX DPCD WRITE success!\n", r);
		//sn65dsi86_reg_write(i2c, DSI86_AUX_STATUS, r);
	}

	return 0;
}

#ifdef BRIDGE_IRQ
void sn65dsi86_csr_clear_all_status_bits()
{
	struct i2c_client *i2c = sn65dsi86_client;

	//Clear Status
	sn65dsi86_reg_write(i2c, 0xF0, 0xff);
	sn65dsi86_reg_write(i2c, 0xF1, 0xff);
	sn65dsi86_reg_write(i2c, 0xF2, 0xff);
	sn65dsi86_reg_write(i2c, 0xF3, 0xff);
	sn65dsi86_reg_write(i2c, 0xF4, 0xff);
	sn65dsi86_reg_write(i2c, 0xF5, 0xff);
	sn65dsi86_reg_write(i2c, 0xF6, 0xff);
	sn65dsi86_reg_write(i2c, 0xF7, 0xff);
	sn65dsi86_reg_write(i2c, 0xF8, 0xff);

}

void sn65dsi86_csr_read_irq()
{
	struct i2c_client *i2c = sn65dsi86_client;
	u8 irq_value_f0 = 0;
	u8 irq_value_f1 = 0;
//	u8 irq_value_f2 = 0;
//	u8 irq_value_f3 = 0;
	u8 irq_value_f4 = 0;
	u8 irq_value_f5 = 0;
	u8 irq_value_f6 = 0;
	u8 irq_value_f7 = 0;
	u8 irq_value_f8 = 0;

	//Read Status
	sn65dsi86_reg_read(i2c, 0xF0, &irq_value_f0);
	sn65dsi86_reg_read(i2c, 0xF1, &irq_value_f1);
//	sn65dsi86_reg_read(i2c, 0xF2, &irq_value_f2);
//	sn65dsi86_reg_read(i2c, 0xF3, &irq_value_f3);
	sn65dsi86_reg_read(i2c, 0xF4, &irq_value_f4);
	sn65dsi86_reg_read(i2c, 0xF5, &irq_value_f5);
	sn65dsi86_reg_read(i2c, 0xF6, &irq_value_f6);
	sn65dsi86_reg_read(i2c, 0xF7, &irq_value_f7);
	sn65dsi86_reg_read(i2c, 0xF8, &irq_value_f8);

	printk(KERN_INFO "IRQ [F0, F1, F4, F5, F6, F7, F8] = %x, %x, %x, %x, %x, %x, %x\n",
			irq_value_f0, irq_value_f1, irq_value_f4, irq_value_f5,
			irq_value_f6, irq_value_f7, irq_value_f8);

	//Clear Status
	sn65dsi86_reg_write(i2c, 0xF0, 0xff);
	sn65dsi86_reg_write(i2c, 0xF1, 0xff);
	sn65dsi86_reg_write(i2c, 0xF2, 0xff);
	sn65dsi86_reg_write(i2c, 0xF3, 0xff);
	sn65dsi86_reg_write(i2c, 0xF4, 0xff);
	sn65dsi86_reg_write(i2c, 0xF5, 0xff);
	sn65dsi86_reg_write(i2c, 0xF6, 0xff);
	sn65dsi86_reg_write(i2c, 0xF7, 0xff);
	sn65dsi86_reg_write(i2c, 0xF8, 0xff);
}

static void sn65dsi86_csr_read_irq_cycle(struct work_struct *work)
{
	struct i2c_client *i2c = sn65dsi86_client;
	u8 irq_value_f0 = 0;
	u8 irq_value_f1 = 0;
	u8 irq_value_f4 = 0;
	u8 irq_value_f5 = 0;
	u8 irq_value_f6 = 0;
	u8 irq_value_f7 = 0;
	u8 irq_value_f8 = 0;

	do {
		if (!pad_power)
			goto skip_i2c_command;

		//Read Status
		sn65dsi86_reg_read(i2c, 0xF0, &irq_value_f0);
		sn65dsi86_reg_read(i2c, 0xF1, &irq_value_f1);
		sn65dsi86_reg_read(i2c, 0xF4, &irq_value_f4);
		sn65dsi86_reg_read(i2c, 0xF5, &irq_value_f5);
		sn65dsi86_reg_read(i2c, 0xF6, &irq_value_f6);
		sn65dsi86_reg_read(i2c, 0xF7, &irq_value_f7);
		sn65dsi86_reg_read(i2c, 0xF8, &irq_value_f8);

		if (irq_value_f0 || irq_value_f1 || irq_value_f4 || irq_value_f5 ||
			irq_value_f6 || irq_value_f7 || irq_value_f8)
		{
			printk(KERN_INFO "[DISPLAY] IRQ [F0, F1, F4, F5, F6, F7, F8] = %x, %x, %x, %x, %x, %x, %x\n",
					irq_value_f0, irq_value_f1, irq_value_f4, irq_value_f5,
					irq_value_f6, irq_value_f7, irq_value_f8);

			//Clear Status
			sn65dsi86_reg_write(i2c, 0xF0, 0xff);
			sn65dsi86_reg_write(i2c, 0xF1, 0xff);
			sn65dsi86_reg_write(i2c, 0xF2, 0xff);
			sn65dsi86_reg_write(i2c, 0xF3, 0xff);
			sn65dsi86_reg_write(i2c, 0xF4, 0xff);
			sn65dsi86_reg_write(i2c, 0xF5, 0xff);
			sn65dsi86_reg_write(i2c, 0xF6, 0xff);
			sn65dsi86_reg_write(i2c, 0xF7, 0xff);
			sn65dsi86_reg_write(i2c, 0xF8, 0xff);
		}
skip_i2c_command:
		msleep(1000);
	} while (1);
}
#endif

void sn65dsi86_csr_vstream_enable(void)
{
	struct i2c_client *i2c = sn65dsi86_client;

	//VSTREAM_ENABLE
	if (sn65dsi86_rev == 0x1) {
		sn65dsi86_reg_write(i2c, 0x5A, 0x0c);
		printk("[DISPLAY] VSTREAM_ENABLE(ES1)\n");
	} else if ((sn65dsi86_rev == 0x2) && (!aux_fail)) {
		sn65dsi86_reg_write(i2c, 0x5A, 0x0d);
		printk("[DISPLAY] VSTREAM_ENABLE(MP)\n");
	} else {
		printk("[DISPLAY] DO NOT enable VSTREAM_ENABLE\n");
	}

}

void sn65dsi86_csr_configure_bridge(void)
{
	struct i2c_client *i2c = sn65dsi86_client;
	u8 pll_lock_value = 0;
	int timeout = 0;
	int fast_training = 0;
	int ret = 0;

	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	if (panel_id) {
		sn65dsi86_reg_write(i2c, 0x10, 0x2e);  //3 lanes
		sn65dsi86_reg_write(i2c, 0x93, 0x14);  //1DP No SSC
		if (sn65dsi86_rev == 0x01) {
			panel_assr = 0;
			printk("[DISPLAY] ASSR = %x \n", panel_assr);
		}
		else if (sn65dsi86_rev == 0x02) {
			//MP bridge
		}
		else
			printk("[DISPLAY] FAIL DEVICE_REV = 0x%x \n", sn65dsi86_rev);
	} else {
		sn65dsi86_reg_write(i2c, 0x10, 0x26);  //4 lanes
		sn65dsi86_reg_write(i2c, 0x93, 0x24);  //2DP No SSC
		if (sn65dsi86_rev == 0x01) {
			panel_assr = 0;
			fast_training = 1;
			printk("[DISPLAY] ASSR = %x \n", panel_assr);
		}
		else if (sn65dsi86_rev == 0x02) {
			sn65dsi86_reg_write(i2c, 0xFF, 0x07);
			sn65dsi86_reg_write(i2c, 0x12, 0x06);  //AUX_VODAUX  900mV
			sn65dsi86_reg_write(i2c, 0xFF, 0x00);
		}
		else
			printk("[DISPLAY] FAIL DEVICE_REV = 0x%x \n", sn65dsi86_rev);
	}

	//HPD Disable
	sn65dsi86_reg_write(i2c, 0x5C, 0x01);

	//HBR 2.7Gbps
	sn65dsi86_reg_write(i2c, 0x94, 0x80);

//	------------------------------------------------------------------------------

	sn65dsi86_reg_write(i2c, 0x0D, 0x01);
	timeout = 0;
	do {
		//read 0x0A until DP PLL locked
		sn65dsi86_reg_read(i2c, 0x0A, &pll_lock_value);
		pr_debug("[DISPLAY] DP_PLL_LOCK = %x \n", pll_lock_value);
		usleep_range(1000, 1500);
		timeout++;

	} while (!(pll_lock_value & 0x80) && (timeout < 5)); //0x01
	if (timeout >= 5)
		printk("[DISPLAY] enable DisplayPort PLL failed! (DP_PLL_LOCK = %x)\n", pll_lock_value);

	if (sn65dsi86_rev == 0x2) {
		//Enable ASSR
		ret = sn65dsi86_dpcd_write(0x10A, 0x1);
		if (ret)
			panel_assr = 0;
		else {
			aux_fail = false;
			panel_assr = sn65dsi86_dpcd_read(0x10A);
		}
		printk("[DISPLAY] ASSR = %x \n", panel_assr);
		if (panel_assr == 0) {
			printk("[DISPLAY] enable ASSR failed!\n");
			sn65dsi86_reg_write(i2c, 0x5A, 0x04);
			aux_fail = true;
		}
	}

auto_train_fail:
	if (fast_training) {
		//TPS1 and TPS2
		sn65dsi86_reg_write(i2c, 0x95, 0xc0);
		//ML_TX_MODE = Fast Link Training
		sn65dsi86_reg_write(i2c, 0x96, 0x09);
		timeout = 0;
		do {
			//read 0x96 until ML_TX_MODE transition to Normal Mode (0x01)
			sn65dsi86_reg_read(i2c, 0x96, &pll_lock_value);
			pr_debug("[DISPLAY] ML_TX_MODE = %x\n", pll_lock_value);
			usleep_range(1000, 1500);
			timeout++;

		} while ((pll_lock_value & 0xfe) && (timeout < 7)); //0x80
		if (pll_lock_value == 1)
			printk("[DISPLAY] Fast Link Training ... Successful.\n");
		else if (pll_lock_value == 0)
			printk("[DISPLAY] Fast Link Training ... Failed!!!\n");
		else if (timeout >= 7)
			printk("[DISPLAY] Fast Link Training ... Timeout. (ML_TX_MODE = %x)\n", pll_lock_value);
	} else {
		//Post-Cursor2 0dB
		sn65dsi86_reg_write(i2c, 0x95, 0x00);
		//Start LInk Training.
		sn65dsi86_reg_write(i2c, 0x96, 0x0A);
		timeout = 0;
		do {
			//read 0x96 until ML_TX_MODE transition to Normal Mode (0x01)
			sn65dsi86_reg_read(i2c, 0x96, &pll_lock_value);
			pr_debug("[DISPLAY] ML_TX_MODE = %x \n", pll_lock_value);
			usleep_range(1000, 1500);
			timeout++;

		} while ((pll_lock_value & 0xfe) && (timeout < 7)); //0x80
		if (pll_lock_value == 1)
			printk("[DISPLAY] Semi-Auto Link Training ... Successful.\n");
		else if (pll_lock_value == 0)
			printk("[DISPLAY] Semi-Auto Link Training ... Failed!!!\n");
		else if (timeout >= 7)
			printk("[DISPLAY] Semi-Auto Link Training ... Timeout. (ML_TX_MODE = %x)\n", pll_lock_value);

		//when Semi-Auto Link Training failed, try to use Fast Link Training.
		if (pll_lock_value != 1) {
			printk(KERN_INFO "[DISPLAY] Semi-Auto Training failed, try to use Fast Training!\n");
			fast_training = 1;
			goto auto_train_fail;
		}
	}

	if (panel_id) {
		//Hact=1366
		sn65dsi86_reg_write(i2c, 0x20, 0x56);
		sn65dsi86_reg_write(i2c, 0x21, 0x05);
		//Vact=768
		sn65dsi86_reg_write(i2c, 0x24, 0x00);
		sn65dsi86_reg_write(i2c, 0x25, 0x03);
		//VSYNC=6
		sn65dsi86_reg_write(i2c, 0x30, 0x06);
		//VBP=13
		sn65dsi86_reg_write(i2c, 0x36, 0x0d);
	} else {
		//Hact=1920
		sn65dsi86_reg_write(i2c, 0x20, 0x80);
		sn65dsi86_reg_write(i2c, 0x21, 0x07);
		//Vact=1080
		sn65dsi86_reg_write(i2c, 0x24, 0x38);
		sn65dsi86_reg_write(i2c, 0x25, 0x04);
		//VSYNC=5
		sn65dsi86_reg_write(i2c, 0x30, 0x05);
		//VBP=24
		sn65dsi86_reg_write(i2c, 0x36, 0x18);
	}
	//HSYNC=32 Negative
	sn65dsi86_reg_write(i2c, 0x2C, 0x20);
	sn65dsi86_reg_write(i2c, 0x2D, 0x80);
	//VSYNC Negative
	sn65dsi86_reg_write(i2c, 0x31, 0x80);
	//HBP=80
	sn65dsi86_reg_write(i2c, 0x34, 0x50);
	//HFP=48
	sn65dsi86_reg_write(i2c, 0x38, 0x30);
	//VFP=3
	sn65dsi86_reg_write(i2c, 0x3A, 0x03);

	//Color_Bar Disabled
	//sn65dsi86_reg_write(i2c, 0x3C, 0x00);

	if (aux_fail) {
		printk("[DISPLAY] try to enable ASSR again!\n");
		ret = sn65dsi86_dpcd_write(0x10A, 0x1);
		if (ret)
			panel_assr = 0;
		else {
			aux_fail = false;
			panel_assr = sn65dsi86_dpcd_read(0x10A);
		}
		printk("[DISPLAY] ASSR = %x \n", panel_assr);
		if (panel_assr == 0) {
			printk("[DISPLAY] enable ASSR failed!\n");
			sn65dsi86_reg_write(i2c, 0x5A, 0x04);
			aux_fail = true;
		}
	}

	//printk(KERN_INFO "[DISPLAY] %s: Exit\n", __func__);
}

int panel_switch_to_android(void)
{
	printk(KERN_INFO "[DISPLAY] %s: Enter\n", __func__);

	//when pad running switch from win8 to android
	if ((pad_power) && (prev_owner))
	{
		if (sn65dsi86_rev == 0x02)
		{
			//sn65dsi86_reg_write(i2c, 0x09, 0x01);
			int bridge_power = gpio_get_value(DP_BRDG_EN);
			if (bridge_power == 0) {
				/*DP_BRDG_EN*/
				if (gpio_direction_output(DP_BRDG_EN, 1))
					gpio_set_value_cansleep(DP_BRDG_EN, 1);
				printk("[DISPLAY] BRIDGE power +++++\n");
				usleep_range(1000, 2000);
			}
			sn65dsi86_csr_configure_bridge();
			sn65dsi86_csr_vstream_enable();
			//if AUX channel fail, set owner_id= 1(Win8).
			if (aux_fail)
				owner_id = 1;
		}

		pwm_enable();
		pwm_configure(pad_prev_level);
		printk("[DISPLAY] %s: set backlight to %d\n", __func__, pad_prev_level);
		usleep_range(10000, 10000);

		/*EN_VDD_BL*/
		intel_scu_ipc_iowrite8(EN_VDD_BL, 0x1);
	}
	else if (prev_owner == 0)
	{
		printk("[DISPLAY] %s: prev_owner = %d\n", __func__, prev_owner);
	}
	else
	{
		printk("[DISPLAY] %s: Pad is sleep\n", __func__);
	}

	return 0;
}

#ifdef CONFIG_HID_ASUS_PAD_EC
static int mux_owner_notify(struct notifier_block *this, unsigned long owner, void *data)
{
	struct i2c_client *i2c = sn65dsi86_client;
	int ret = 0;
	//printk(KERN_INFO "[DISPLAY] %s: Enter\n", __func__);

	mutex_lock(&switch_mutex);
	if (owner == SYSTEM_NONE)
	{
		prev_owner = owner_id;
		owner_id = 0;
		printk("[DISPLAY] %s: MUX owner is Android.\n", __func__);
		if (prev_owner) {
			ret = panel_switch_to_android();
		} else {
			printk("[DISPLAY] %s: prev_owner = %d\n", __func__, prev_owner);
		}
	}
	else if (owner == SYSTEM_ANDROID)
	{
		prev_owner = owner_id;
		owner_id = 0;
		printk("[DISPLAY] %s: MUX owner switch to Android.\n", __func__);
		if (prev_owner) {
			ret = panel_switch_to_android();
		} else {
			printk("[DISPLAY] %s: prev_owner = %d\n", __func__, prev_owner);
		}
	}
	else if (owner == SYSTEM_UPDATE)
	{
		prev_owner = owner_id;
		owner_id = 0;
		printk("[DISPLAY] %s: MUX owner switch to Android for EC FW update.\n", __func__);
		if (prev_owner) {
			ret = panel_switch_to_android();
		} else {
			printk("[DISPLAY] %s: prev_owner = %d\n", __func__, prev_owner);
		}
	}
	else if (owner == SYSTEM_WINDOWS)
	{
		if (!owner_id)
		{
			//turn off backlight and pwm
			intel_scu_ipc_iowrite8(EN_VDD_BL, 0x0);

			if (panel_assr & 0x1) {
				printk("[DISPLAY] %s: disable ASSR\n", __func__);
				//Disable video stream
				sn65dsi86_reg_write(i2c, 0x5A, 0x04);
				ret = 0;
				//Disable ASSR
				ret = sn65dsi86_dpcd_write(0x10A, 0x0);
				if (ret) {
					aux_fail = true;
					printk("[DISPLAY] %s: disable ASSR failed!\n", __func__);
				} else {
					aux_fail = false;
					panel_assr = 0;
				}
				/*DP_BRDG_EN*/
				if (gpio_direction_output(DP_BRDG_EN, 0))
					gpio_set_value_cansleep(DP_BRDG_EN, 0);
				printk("[DISPLAY] BRIDGE power ---\n");
			} else {
				printk("[DISPLAY] %s: do nothing\n", __func__);
			}
			pwm_configure(0);
			pwm_disable();
			printk("[DISPLAY] %s: turn off backlight and pwm\n", __func__);
		}

		prev_owner = owner_id;
		owner_id = 1;
		printk("[DISPLAY] %s: MUX owner switch to Windows8, prev_owner= %d\n", __func__, prev_owner);
	}
	else
	{
		printk("[DISPLAY] %s: dock notifier failed.\n", __func__);
	}
	mutex_unlock(&switch_mutex);

	return NOTIFY_OK;
}
#endif

static void sn65dsi86_vid_dsi_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx = &dsi_config->dsi_hw_context;
	struct drm_device *dev = dsi_config->dev;

	struct csc_setting csc = {	.pipe = 0,
								.type = CSC_REG_SETTING,
								.enable_state = true,
								.data_len = CSC_REG_COUNT,
								.data.csc_reg_data = {
									0x400, 0x0, 0x4000000, 0x0, 0x0, 0x400}
							 };

	struct gamma_setting boe_gamma = {	.pipe = 0,
									.type = GAMMA_REG_SETTING,
									.enable_state = true,
									.data_len = GAMMA_10_BIT_TABLE_COUNT,
									.gamma_tableX100 = {
										0x000000, 0x020202, 0x040404, 0x060606,
										0x080808, 0x0A0A0A, 0x0C0C0C, 0x0E0E0E,
										0x101010, 0x121212, 0x141414, 0x161616,
										0x181818, 0x1A1A1A, 0x1C1C1C, 0x1E1E1E,
										0x202020, 0x222222, 0x242424, 0x262626,
										0x282828, 0x2A2A2A, 0x2C2C2C, 0x2E2E2E,
										0x303030, 0x323232, 0x343434, 0x363636,
										0x383838, 0x3A3A3A, 0x3C3C3C, 0x3E3E3E,
										0x404040, 0x424242, 0x444444, 0x464646,
										0x484848, 0x4A4A4A, 0x4C4C4C, 0x4E4E4E,
										0x505050, 0x525252, 0x545454, 0x565656,
										0x585858, 0x5A5A5A, 0x5C5C5C, 0x5E5E5E,
										0x606060, 0x626262, 0x646464, 0x666666,
										0x686868, 0x6A6A6A, 0x6C6C6C, 0x6E6E6E,
										0x707070, 0x727272, 0x747474, 0x767676,
										0x787878, 0x7A7A7A, 0x7C7C7C, 0x7E7E7E,
										0x808080, 0x828282, 0x848484, 0x868686,
										0x888888, 0x8A8A8A, 0x8C8C8C, 0x8E8E8E,
										0x909090, 0x929292, 0x949494, 0x969696,
										0x989898, 0x9A9A9A, 0x9C9C9C, 0x9E9E9E,
										0xA0A0A0, 0xA2A2A2, 0xA4A4A4, 0xA6A6A6,
										0xA8A8A8, 0xAAAAAA, 0xACACAC, 0xAEAEAE,
										0xB0B0B0, 0xB2B2B2, 0xB4B4B4, 0xB6B6B6,
										0xB8B8B8, 0xBABABA, 0xBCBCBC, 0xBEBEBE,
										0xC0C0C0, 0xC2C2C2, 0xC4C4C4, 0xC6C6C6,
										0xC8C8C8, 0xCACACA, 0xCCCCCC, 0xCECECE,
										0xD0D0D0, 0xD2D2D2, 0xD4D4D4, 0xD6D6D6,
										0xD8D8D8, 0xDADADA, 0xDCDCDC, 0xDEDEDE,
										0xE0E0E0, 0xE2E2E2, 0xE4E4E4, 0xE6E6E6,
										0xE8E8E8, 0xEAEAEA, 0xECECEC, 0xEEEEEE,
										0xF0F0F0, 0xF2F2F2, 0xF4F4F4, 0xF6F6F6,
										0xF8F8F8, 0xFAFAFA, 0xFCFCFC, 0xFEFEFE,
										0x010000, 0x010000, 0x010000}
								 };

	struct gamma_setting innolux_gamma = {	.pipe = 0,
									.type = GAMMA_REG_SETTING,
									.enable_state = true,
									.data_len = GAMMA_10_BIT_TABLE_COUNT,
									.gamma_tableX100 = {
										0x000000, 0x020202, 0x040404, 0x060606,
										0x080808, 0x0A0A0A, 0x0C0C0C, 0x0E0E0E,
										0x101010, 0x121212, 0x141414, 0x161616,
										0x181818, 0x1A1A1A, 0x1C1C1C, 0x1E1E1E,
										0x202020, 0x222222, 0x242424, 0x262626,
										0x282828, 0x2A2A2A, 0x2C2C2C, 0x2E2E2E,
										0x303030, 0x323232, 0x343434, 0x363636,
										0x383838, 0x3A3A3A, 0x3C3C3C, 0x3E3E3E,
										0x404040, 0x424242, 0x444444, 0x464646,
										0x484848, 0x4A4A4A, 0x4C4C4C, 0x4E4E4E,
										0x505050, 0x525252, 0x545454, 0x565656,
										0x585858, 0x5A5A5A, 0x5C5C5C, 0x5E5E5E,
										0x606060, 0x626262, 0x646464, 0x666666,
										0x686868, 0x6A6A6A, 0x6C6C6C, 0x6E6E6E,
										0x707070, 0x727272, 0x747474, 0x767676,
										0x787878, 0x7A7A7A, 0x7C7C7C, 0x7E7E7E,
										0x808080, 0x828282, 0x848484, 0x868686,
										0x888888, 0x8A8A8A, 0x8C8C8C, 0x8E8E8E,
										0x909090, 0x929292, 0x949494, 0x969696,
										0x989898, 0x9A9A9A, 0x9C9C9C, 0x9E9E9E,
										0xA0A0A0, 0xA2A2A2, 0xA4A4A4, 0xA6A6A6,
										0xA8A8A8, 0xAAAAAA, 0xACACAC, 0xAEAEAE,
										0xB0B0B0, 0xB2B2B2, 0xB4B4B4, 0xB6B6B6,
										0xB8B8B8, 0xBABABA, 0xBCBCBC, 0xBEBEBE,
										0xC0C0C0, 0xC2C2C2, 0xC4C4C4, 0xC6C6C6,
										0xC8C8C8, 0xCACACA, 0xCCCCCC, 0xCECECE,
										0xD0D0D0, 0xD2D2D2, 0xD4D4D4, 0xD6D6D6,
										0xD8D8D8, 0xDADADA, 0xDCDCDC, 0xDEDEDE,
										0xE0E0E0, 0xE2E2E2, 0xE4E4E4, 0xE6E6E6,
										0xE8E8E8, 0xEAEAEA, 0xECECEC, 0xEEEEEE,
										0xF0F0F0, 0xF2F2F2, 0xF4F4F4, 0xF6F6F6,
										0xF8F8F8, 0xFAFAFA, 0xFCFCFC, 0xFEFEFE,
										0x010000, 0x010000, 0x010000}
								 };
	PSB_DEBUG_ENTRY("\n");

	if (panel_id) {
		// BOE 1366*768
		dsi_config->lane_count = 3;
		dsi_config->bpp = 24;
		dsi_config->lane_config = MDFLD_DSI_DATA_LANE_3_1;
		dsi_config->enable_gamma_csc = ENABLE_GAMMA | ENABLE_CSC;

		hw_ctx->cck_div = 1;
		hw_ctx->pll_bypass_mode = 0;

		hw_ctx->mipi_control = 0x18;
		hw_ctx->intr_en = 0xffffffff;
		hw_ctx->hs_tx_timeout = 0xffffff;
		hw_ctx->lp_rx_timeout = 0xffffff;
		hw_ctx->turn_around_timeout = 0x1f;
		hw_ctx->device_reset_timer = 0xffff;
		hw_ctx->high_low_switch_count = 0x1d;
		hw_ctx->init_count = 0xf0;
		hw_ctx->eot_disable = 0x2;
		hw_ctx->lp_byteclk = 0x4;
		hw_ctx->clk_lane_switch_time_cnt = 0x1d000d;
		hw_ctx->dphy_param = 0x1d114715;

	} else {
		// Innolux 1920*1080
		dsi_config->lane_count = 4;
		dsi_config->bpp = 24;
		dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;
		dsi_config->enable_gamma_csc = ENABLE_GAMMA | ENABLE_CSC;

		hw_ctx->cck_div = 0;
		hw_ctx->pll_bypass_mode = 0;

		hw_ctx->mipi_control = 0x18;
		hw_ctx->intr_en = 0xffffffff;
		hw_ctx->hs_tx_timeout = 0xffffff;
		hw_ctx->lp_rx_timeout = 0xffffff;
		hw_ctx->turn_around_timeout = 0x1f;
		hw_ctx->device_reset_timer = 0xffff;
		hw_ctx->high_low_switch_count = 0x2a;
		hw_ctx->init_count = 0xf0;
		hw_ctx->eot_disable = 0x2;
		hw_ctx->lp_byteclk = 0x6;
		hw_ctx->clk_lane_switch_time_cnt = 0x2a0013;
		hw_ctx->dphy_param = 0x2e1a711f;
	}

	/* Setup video mode format */
	hw_ctx->video_mode_format = 0xf;

	/* Set up func_prg, RGB888(0x200) MIPIA_DSI_FUNC_PRG_REG 0xb00c*/
	hw_ctx->dsi_func_prg = (0x200 | dsi_config->lane_count);

	/* Setup mipi port configuration */
	hw_ctx->mipi = PASS_FROM_SPHY_TO_AFE | dsi_config->lane_config;

	if (dsi_config->enable_gamma_csc & ENABLE_CSC) {
		/* setting the tuned csc setting */
		drm_psb_enable_color_conversion = 1;
		mdfld_intel_crtc_set_color_conversion(dev, &csc);
	}

	if (dsi_config->enable_gamma_csc & ENABLE_GAMMA) {
		/* setting the tuned gamma setting */
		drm_psb_enable_gamma = 1;
		if (panel_id)
			mdfld_intel_crtc_set_gamma(dev, &boe_gamma);
		else
			mdfld_intel_crtc_set_gamma(dev, &innolux_gamma);
	}

}

static int sn65dsi86_vid_detect(struct mdfld_dsi_config *dsi_config)
{
	int status;
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	u32 dpll_val, device_ready_val;
	int pipe = dsi_config->pipe;
	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	if (pipe == 0) {
		/*
		 * FIXME: WA to detect the panel connection status, and need to
		 * implement detection feature with get_power_mode DSI command.
		 */
		if (!ospm_power_using_hw_begin(OSPM_DISPLAY_ISLAND,
			OSPM_UHB_FORCE_POWER_ON)) {
			DRM_ERROR("hw begin failed\n");
			return -EAGAIN;
		}

		dpll_val = REG_READ(regs->dpll_reg);
		device_ready_val = REG_READ(regs->device_ready_reg);
		if ((device_ready_val & DSI_DEVICE_READY) &&
		    (dpll_val & DPLL_VCO_ENABLE)) {
			dsi_config->dsi_hw_context.panel_on = true;
			psb_enable_vblank(dev, pipe);
		} else {
			dsi_config->dsi_hw_context.panel_on = false;
			DRM_INFO("%s: panel is not detected!\n", __func__);
		}

		status = MDFLD_DSI_PANEL_CONNECTED;
		ospm_power_using_hw_end(OSPM_DISPLAY_ISLAND);
	} else {
		DRM_INFO("%s: do NOT support dual panel\n", __func__);
		status = MDFLD_DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static int sn65dsi86_vid_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	DRM_INFO("[DISPLAY] %s: Enter\n", __func__);

	usleep_range(200000, 200000);
	sn65dsi86_csr_configure_bridge();

	usleep_range(1500, 1500);

	/* Send TURN_ON packet */
	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
					    MDFLD_DSI_DPI_SPK_TURN_ON);
	if (err) {
		DRM_ERROR("Failed to send turn on packet\n");
		return err;
	}

	sn65dsi86_csr_vstream_enable();

#ifdef BRIDGE_IRQ
	sn65dsi86_csr_clear_all_status_bits();

	// ti bridge interrupt enable
	schedule_work(&irq_workq);
#endif

	if (panel_id)
		usleep_range(140000, 150000);

	return 0;
}

static int sn65dsi86_vid_power_off(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	DRM_INFO("[DISPLAY] %s: Enter\n", __func__);

	/* Send SHUT_DOWN packet */
	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
					    MDFLD_DSI_DPI_SPK_SHUT_DOWN);
	if (err) {
		DRM_ERROR("Failed to send turn off packet\n");
		return err;
	}
	usleep_range(10000, 15000);

	/*DP_BRDG_EN*/
	if (gpio_direction_output(DP_BRDG_EN, 0))
		gpio_set_value_cansleep(DP_BRDG_EN, 0);
	usleep_range(1000, 2000);

	/*LCD_LOGIC_PWR_EN*/
	if (hw_id == HW_ID_SR1)
	{
		if (owner_id == 0) {
			// MUX owner: Android
			if (gpio_direction_output(LCD_LOGIC_PWR_EN, 0))
				gpio_set_value_cansleep(LCD_LOGIC_PWR_EN, 0);
			printk("[DISPLAY] MUX owner: Android, turn off LCD_LOGIC_PWR_EN pin.\n");
			usleep_range(1000, 2000);
			panel_assr = 0;
		} else {
			// MUX owner: Windows8
			printk("[DISPLAY] MUX owner: Windows8, DO NOT turn off LCD_LOGIC_PWR_EN pin.\n");
		}
	}
	else
	{
		if (gpio_direction_output(LCD_LOGIC_PWR_EN, 0))
			gpio_set_value_cansleep(LCD_LOGIC_PWR_EN, 0);
		usleep_range(1000, 2000);
		panel_assr = 0;
	}

	return 0;
}

static int sn65dsi86_vid_reset(struct mdfld_dsi_config *dsi_config)
{
	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	/*LCD_LOGIC_PWR_EN*/
	if (gpio_direction_output(LCD_LOGIC_PWR_EN, 1))
		gpio_set_value_cansleep(LCD_LOGIC_PWR_EN, 1);
	usleep_range(1000, 2000);

	/*DP_BRDG_EN*/
	if (gpio_direction_output(DP_BRDG_EN, 1))
		gpio_set_value_cansleep(DP_BRDG_EN, 1);  /*Pull MIPI Bridge EN pin to High */
	usleep_range(1000, 2000);

	if (request_gpio49 == 0) {
		gpio_request(PWM_ENABLE_GPIO, "PWM_ENABLE_GPIO");
		request_gpio49 = 1;
	}

	pr_debug("[DISPLAY] Panel: %s\n", panel_id ? "BOE(1366x768)" : "Innolux(1920x1080)");

	return 0;
}

static int bl_prev_level = 0;
static int sn65dsi86_vid_set_brightness(struct mdfld_dsi_config *dsi_config,
					 int level)
{
	int duty_val = 0;

	if (level == 1)
		level = 0;
	duty_val = level;

	//add for debug ++
	if(!!bl_prev_level^!!level) {
#ifdef CONFIG_HID_ASUS_PAD_EC
		//read MUX owner
		int mux = ite_panel_owner_notify();
		if (aux_fail || (mux == SYSTEM_WINDOWS))
			owner_id = 1;
		else if ((mux == SYSTEM_ANDROID) || (mux == SYSTEM_NONE))
			owner_id = 0;
#endif
		if (bl_prev_level == 0)
		{	//power on
			if (owner_id == 0) {
				pwm_enable();
				pwm_configure(duty_val);
				pad_power = 1;
				usleep_range(10000, 15000);
				/*EN_VDD_BL*/
				intel_scu_ipc_iowrite8(EN_VDD_BL, 0x1);
				DRM_INFO("[DISPLAY] brightness level : %d > %d\n", bl_prev_level, level);
			} else {
				pad_power = 1;
				printk("[DISPLAY] MUX owner: Windows8, DO NOT control BL_EN and BL_PWM (keep low).\n");
			}
		}
		else if (level == 0)
		{	//power off
			intel_scu_ipc_iowrite8(EN_VDD_BL, 0x0);
			usleep_range(10000, 15000);
			pwm_configure(duty_val);
			pad_power = 0;
			pwm_disable();
			DRM_INFO("[DISPLAY] brightness level : %d > %d\n", bl_prev_level, level);
		}
	} else {
		pwm_configure(duty_val);
	}

	bl_prev_level = level;
	//add for debug --

	if (level)
		pad_prev_level = level;

	pr_debug("[DISPLAY] brightness level : %d (duty = 0x%x)\n", level, duty_val);

	return 0;
}

struct drm_display_mode *sn65dsi86_vid_get_config_mode(void)
{
	struct drm_display_mode *mode;

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;
	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	if (panel_id) {
		// BOE 1366*768
		mode->hdisplay = 1366;
		mode->hsync_start = (mode->hdisplay + 2) + 48;	//sync offset
		mode->hsync_end = mode->hsync_start + 32;	//pulse width
		mode->htotal = (mode->hdisplay + 2) + 160;	//blank

		mode->vdisplay = 768;
		mode->vsync_start = mode->vdisplay + 3;
		mode->vsync_end = mode->vsync_start + 6;
		mode->vtotal = mode->vdisplay + 22;

	} else {
		// Innolux 1920*1080
		mode->hdisplay = 1920;
		mode->hsync_start = mode->hdisplay + 64;	//sync offset
		mode->hsync_end = mode->hsync_start + 43;	//pulse width
		mode->htotal = mode->hdisplay + 214;		//blank

		mode->vdisplay = 1080;
		mode->vsync_start = mode->vdisplay + 4;
		mode->vsync_end = mode->vsync_start + 7;
		mode->vtotal = mode->vdisplay + 43;
	}

	mode->vrefresh = 60;
	mode->clock = mode->vrefresh * mode->vtotal * mode->htotal / 1000;
	mode->type |= DRM_MODE_TYPE_PREFERRED;
	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	return mode;
}

static void sn65dsi86_vid_get_panel_info(int pipe, struct panel_info *pi)
{
	pi->width_mm = 256;
	pi->height_mm = 144;
}

static int sn65dsi86_vid_gpio_init(void)
{
	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	gpio_request(LCD_LOGIC_PWR_EN, "LCD_LOGIC_PWR_EN");

	hw_id = Read_HW_ID();
	pcb_id = Read_PCB_ID();
	panel_id = 0; //default setting
	pad_power = 1;

	if (hw_id == HW_ID_SR1)
	{
		gpio_request(PCB_ID3, "PCB_ID3");
		panel_id = gpio_get_value(PCB_ID3);
	}
	else if ((hw_id == HW_ID_ER) || (hw_id == HW_ID_ER2))
	{
		if ((pcb_id & 0x8) && (pcb_id & 0x10)) //BOE(1366*768)
			panel_id = 1;
		else if (!(pcb_id & 0x8) && !(pcb_id & 0x10)) //Innolux(1920*1080)
			panel_id = 0;
		else
			DRM_INFO("[DISPLAY] Panel detect failed! (pcb_id=0x%x)\n", pcb_id);
	}
	else
	{
		panel_id = 0; //Innolux(1920*1080)
	}
	asus_panel_id = TX201LA_PANEL | (panel_id ? 0x0 : 0x1);
	DRM_INFO("[DISPLAY] Panel ID = %d\n", panel_id);

	owner_id = 0;

	return 0;
}

#define PWMCTRL_REG 0xffae9000
#define PWMCTRL_SIZE 0x80

static int sn65dsi86_vid_brightness_init(void)
{
	int ret = 0;
	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	pwmctrl_mmio = ioremap_nocache(PWMCTRL_REG, PWMCTRL_SIZE);

	lnw_gpio_set_alt(PWM_ENABLE_GPIO, LNW_ALT_2);

	return ret;
}

static int sn65dsi86_bridge_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int bridge_power;
	PSB_DEBUG_ENTRY("\n");
	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		DRM_ERROR("i2c_check_functionality() failed\n");
		return -ENODEV;
	}

	gpio_request(DP_BRDG_EN, "DP_BRDG_EN");

	bridge_power = gpio_get_value(DP_BRDG_EN);
	if (bridge_power == 0) {
		/*DP_BRDG_EN*/
		if (gpio_direction_output(DP_BRDG_EN, 1))
			gpio_set_value_cansleep(DP_BRDG_EN, 1);  /*Pull MIPI Bridge EN pin to High */
		usleep_range(1000, 2000);
	}

	sn65dsi86_client = client;

	sn65dsi86_reg_read(sn65dsi86_client, 0x08, &sn65dsi86_rev);
	DRM_INFO("[DISPLAY] SN65DSI86 DEVICE_REV: 0x%x\n", sn65dsi86_rev);

	if ((panel_id == 0) && (sn65dsi86_rev == 0x1)) {
		panel_assr = 0;
	} else {
		panel_assr = sn65dsi86_dpcd_read(0x10A);
		//sn65dsi86_reg_read(sn65dsi86_client, 0x79, &panel_assr);
	}
	DRM_INFO("[DISPLAY] ASSR = %x\n", panel_assr);

	mutex_init(&switch_mutex);

#ifdef BRIDGE_IRQ
	INIT_WORK(&irq_workq, sn65dsi86_csr_read_irq_cycle);
#endif

#ifdef CONFIG_HID_ASUS_PAD_EC
	register_dock_atow_early_notifier(&display_atow_early_notifier);
	register_dock_wtoa_late_notifier(&display_wtoa_late_notifier);
	register_dock_detach_notifier(&display_detach_notifier);
#endif

	return 0;
}

static const struct i2c_device_id sn65dsi86_bridge_id[] = {
	{"i2c_disp_brig", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sn65dsi86_bridge_id);

static int sn65dsi86_bridge_remove(struct i2c_client *client)
{
	PSB_DEBUG_ENTRY("\n");
	DRM_INFO("[DISPLAY] %s: Enter\n", __func__);

#ifdef CONFIG_HID_ASUS_PAD_EC
	unregister_dock_atow_early_notifier(&display_atow_early_notifier);
	unregister_dock_wtoa_late_notifier(&display_wtoa_late_notifier);
	unregister_dock_detach_notifier(&display_detach_notifier);
#endif

	gpio_free(DP_BRDG_EN);

	sn65dsi86_client = NULL;

	return 0;
}

static struct i2c_driver sn65dsi86_bridge_i2c_driver = {
	.driver = {
		.name = "i2c_disp_brig",
	},
	.id_table = sn65dsi86_bridge_id,
	.probe = sn65dsi86_bridge_probe,
	.remove = __devexit_p(sn65dsi86_bridge_remove),
};

static int sn65dsi86_hack_create_device(void)
{
	struct i2c_adapter *adapter;
	struct i2c_client *client;
	struct i2c_board_info info = {
		.type = "i2c_disp_brig",
		.addr = TI_EDP_BRIDGE_I2C_ADDR,
	};

	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	adapter = i2c_get_adapter(TI_EDP_BRIDGE_I2C_ADAPTER);
	if (!adapter) {
		pr_err("%s: i2c_get_adapter(%d) failed\n", __func__,
			TI_EDP_BRIDGE_I2C_ADAPTER);
		return -EINVAL;
	}

	client = i2c_new_device(adapter, &info);
	if (!client) {
		pr_err("%s: i2c_new_device() failed\n", __func__);
		i2c_put_adapter(adapter);
		return -EINVAL;
	}

	return 0;
}

void sn65dsi86_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	int ret = 0;
	pr_debug("[DISPLAY] %s: Enter\n", __func__);

	p_funcs->get_config_mode = sn65dsi86_vid_get_config_mode;
	p_funcs->get_panel_info = sn65dsi86_vid_get_panel_info;
	p_funcs->dsi_controller_init = sn65dsi86_vid_dsi_controller_init;
	p_funcs->detect = sn65dsi86_vid_detect;
	p_funcs->power_on = sn65dsi86_vid_power_on;
	p_funcs->power_off = sn65dsi86_vid_power_off;
	p_funcs->reset = sn65dsi86_vid_reset;
	p_funcs->set_brightness = sn65dsi86_vid_set_brightness;
	ret = sn65dsi86_vid_gpio_init();
	if (ret)
		DRM_ERROR("Failed to request GPIO for SN65DSI86 bridge\n");

	ret = sn65dsi86_vid_brightness_init();
	if (ret)
		DRM_ERROR("Failed to initilize PWM of MSCI\n");

	ret = sn65dsi86_hack_create_device();
	if (ret)
		DRM_ERROR("sn65dsi86_hack_create_device() failed\n");

	ret = i2c_add_driver(&sn65dsi86_bridge_i2c_driver);
	if (ret)
		DRM_ERROR("add bridge I2C driver failed\n");

	ret = sysfs_create_group(&dev->dev->kobj, &sn65dsi86_attr_group);
	if (ret)
		DRM_ERROR("Not able to create the sysfs\n");

}

static int sn65dsi86_vid_lcd_probe(struct platform_device *pdev)
{
	DRM_INFO("%s: SN65DSI86 bridge detected\n", __func__);
	intel_mid_panel_register(sn65dsi86_vid_init);

	return 0;
}

struct platform_driver sn65dsi86_vid_lcd_driver = {
	.probe	= sn65dsi86_vid_lcd_probe,
	.driver	= {
		.name	= SN65DSI86_BRIDGE_NAME,
		.owner	= THIS_MODULE,
	},
};
