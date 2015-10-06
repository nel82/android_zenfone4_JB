/*
 * leds-intel-kpd.c - Intel Keypad LED driver
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/rpmsg.h>

#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_pwm.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/HWVersion.h>
#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/mutex.h>
#include <linux/microp_notify.h>
#include <linux/microp_api.h>

#define CHRLEDPWM 0x194
#define CHRLEDCTRL 0x195
#define PWM1_DUT_CYC_ER 0x68
#define PWM2_DUT_CYC_ER 0x69

#define LED_RED_GPIO 38
#define LED_GREEN_GPIO 39

extern int Read_HW_ID(void);
extern int Read_PROJ_ID(void);
#ifdef  CONFIG_EEPROM_PADSTATION
extern int register_microp_notifier(struct notifier_block *nb);
extern int unregister_microp_notifier(struct notifier_block *nb);
#endif
static int HW_ID;
static int PROJ_ID;
static bool led_update = false; //allow led update
static DEFINE_MUTEX(led_mutex);

static bool LED_for_old_HW = false;
static bool LED_for_new_HW = false;

static void intel_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	int ret;
	int ret2;
        uint8_t data_led;

	if(!led_update){
	    return;
	}
	if(!LED_for_old_HW && !LED_for_new_HW)
	    return;

	mutex_lock(&led_mutex);	
	switch (value) {
	case 0: // off
		if(LED_for_old_HW){
			ret = intel_scu_ipc_iowrite8(CHRLEDPWM, 0x00); //duty_cycle = 0
			ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x36);	// led disable
		}else if (LED_for_new_HW){
			ret = intel_scu_ipc_iowrite8(CHRLEDPWM, 0xff); //duty_cycle = 0
			ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x34); // led disable
		}
		gpio_set_value(LED_RED_GPIO, 0);
		gpio_set_value(LED_GREEN_GPIO, 0);
		break;
	case 1: //red led
	case 2: //green led
	case 3: //orange led
                if(LED_for_old_HW){
			if((led_cdev -> blink_delay_on + led_cdev -> blink_delay_off) == 4000){//4s
                                data_led = (uint8_t)((255 * led_cdev -> blink_delay_on)/4000);
                                ret = intel_scu_ipc_iowrite8(CHRLEDPWM, data_led);
                                ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x27);		
			}
			else if((led_cdev -> blink_delay_on + led_cdev -> blink_delay_off) == 2000){//2s
                                data_led = (uint8_t)((255 * led_cdev -> blink_delay_on)/2000);
                                ret = intel_scu_ipc_iowrite8(CHRLEDPWM, data_led);
                                ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x2f);
			}
			else{
                                ret = intel_scu_ipc_iowrite8(CHRLEDPWM, 0xff); //duty_cycle = 100
                                ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x37);
			}
			
                }else if (LED_for_new_HW){
                        if((led_cdev -> blink_delay_on + led_cdev -> blink_delay_off) == 4000){//4s
                                data_led = (uint8_t)((255 * led_cdev -> blink_delay_off)/4000);
                                ret = intel_scu_ipc_iowrite8(CHRLEDPWM, data_led);
                                ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x25);
                        }
                        else if((led_cdev -> blink_delay_on + led_cdev -> blink_delay_off) == 2000){//2s
                                data_led = (uint8_t)((255 * led_cdev -> blink_delay_off)/2000);
                                ret = intel_scu_ipc_iowrite8(CHRLEDPWM, data_led);
                                ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x2d);
                        }
                        else{
                                ret = intel_scu_ipc_iowrite8(CHRLEDPWM, 0x00); //duty_cycle = 100
                                ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x35);
                        }
                }
		
                if(value == 1){
			gpio_set_value(LED_RED_GPIO,1);
			gpio_set_value(LED_GREEN_GPIO, 0);
		}else if(value == 2){
			gpio_set_value(LED_RED_GPIO,0);
			gpio_set_value(LED_GREEN_GPIO, 1);
		}else if (value ==3){
			gpio_set_value(LED_RED_GPIO,1);
			gpio_set_value(LED_GREEN_GPIO, 1);
		}
	       
		break;
	default :
		break;
	}
	
	//debug information
	if(1){
		printk("[LED SET] brightness = %d  blink_delay_on = %d  blink_delay_off = %d \n",value,led_cdev -> blink_delay_on, 
				led_cdev -> blink_delay_off);
		ret2 = intel_scu_ipc_ioread8(CHRLEDPWM, &data_led);
		printk("[LED]: Read CHRLEDPWM = %02X ret = %d\n",data_led,ret2);
		ret2 = intel_scu_ipc_ioread8(CHRLEDCTRL, &data_led);
		printk("[LED]: Read CHRLEDCTRL = %02X ret = %d\n",data_led,ret2);
	}
	mutex_unlock(&led_mutex);
}

static struct led_classdev intel_led = {
	.name			= "intel_led",
	.brightness_set		= intel_led_set,
	.brightness		= LED_OFF,
	.max_brightness		= 15,
};

static void red_led_set(struct led_classdev *led_cdev, enum led_brightness value){}
static struct led_classdev red_led = {
	.name                   = "red",
	.brightness_set         = red_led_set,
};

void led_set_for_driver(int brightness,unsigned long blink_delay_on, unsigned long blink_delay_off)
{
	mutex_lock(&led_mutex);
	intel_led.brightness = brightness;
	intel_led.blink_delay_on = blink_delay_on;
	intel_led.blink_delay_off = blink_delay_off;
	mutex_unlock(&led_mutex);
	intel_led_set(&intel_led, intel_led.brightness);

}
EXPORT_SYMBOL(led_set_for_driver);

#ifdef  CONFIG_EEPROM_PADSTATION
static int led_report(struct notifier_block *nb, unsigned long event, void *ptr)
{
	int ret;
	switch (event) {
		case P01_ADD:
		     printk("[LED] PAD add - close LED \n");
		     led_update = false;
		     mutex_lock(&led_mutex);
		     if(HW_ID == 2 || HW_ID == 6 || HW_ID == 1){
	             	if(HW_ID == 2){
		           ret = intel_scu_ipc_iowrite8(CHRLEDPWM, 0x00); //duty_cycle = 0
			   ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x36); // led disable
			}else if (HW_ID == 6 || HW_ID == 1){
		           ret = intel_scu_ipc_iowrite8(CHRLEDPWM, 0xff); //duty_cycle = 0
			   ret = intel_scu_ipc_iowrite8(CHRLEDCTRL, 0x34); // led disable
			}
                        gpio_set_value(LED_RED_GPIO, 0);
                        gpio_set_value(LED_GREEN_GPIO, 0);
                     }
		     mutex_unlock(&led_mutex);
		     break;
		case P01_REMOVE:
		     printk("[LED] PAD remove - restore LED \n");
		     led_update = true;
		     intel_led_set(&intel_led, intel_led.brightness);
		     break;
		default:
		     break;
	}

	return 0;
}


static struct notifier_block led_notifier = {
	.notifier_call = led_report,
};
#endif
/*static void intel_kpd_led_early_suspend(struct early_suspend *h)
{
	led_classdev_suspend(&intel_kpd_led);
}

static void intel_kpd_led_late_resume(struct early_suspend *h)
{
	led_classdev_resume(&intel_kpd_led);
}

static struct early_suspend intel_kpd_led_suspend_desc = {
	.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.suspend = intel_kpd_led_early_suspend,
	.resume  = intel_kpd_led_late_resume,
};*/

static int intel_kpd_led_rpmsg_probe(struct rpmsg_channel *rpdev)
{
	int ret;
	int ret2;
	uint8_t data_led;

	dev_info(&rpdev->dev, "Probed kpd_led rpmsg device\n");
        HW_ID = Read_HW_ID();
	PROJ_ID = Read_PROJ_ID();
	LED_for_old_HW =    (PROJ_ID == PROJ_ID_PF400CG && HW_ID == 2) ||
                            (PROJ_ID == PROJ_ID_A400CG  && (HW_ID == 0 || HW_ID == 4));
	LED_for_new_HW =    (PROJ_ID == PROJ_ID_PF400CG && (HW_ID == 6 || HW_ID == 1)) ||
                            (PROJ_ID == PROJ_ID_A400CG  && (HW_ID != 0 && HW_ID != 4 ));

	ret = led_classdev_register(&rpdev->dev, &intel_led);
	if (ret) {
		dev_err(&rpdev->dev, "register led dev failed");
		return ret;
	}

        ret = led_classdev_register(&rpdev->dev, &red_led);
        if (ret) {
                dev_err(&rpdev->dev, "register led dev failed");
                return ret;
        }

	ret = intel_scu_ipc_iowrite8(PWM1_DUT_CYC_ER, 0x00);
	ret = intel_scu_ipc_iowrite8(PWM2_DUT_CYC_ER, 0x00);

	if(LED_for_old_HW || LED_for_new_HW){//PF400CG ER2:2, PR:6, MP:1 
		printk("[LED]: The HW_ID = %d \n",HW_ID);
		ret = gpio_request_one(LED_RED_GPIO, GPIOF_OUT_INIT_LOW, "RED_LED");
		ret = gpio_request_one(LED_GREEN_GPIO, GPIOF_OUT_INIT_LOW, "GREEN_LED");
	}
	
	led_update = true;
	intel_led_set(&intel_led, intel_led.brightness);
#ifdef 	CONFIG_EEPROM_PADSTATION
	register_microp_notifier(&led_notifier);
#endif	
	//register_early_suspend(&intel_kpd_led_suspend_desc);

	return 0;
}

static void intel_kpd_led_rpmsg_remove(struct rpmsg_channel *rpdev)
{
	intel_led_set(&intel_led, LED_OFF);
	//unregister_early_suspend(&intel_kpd_led_suspend_desc);
#ifdef  CONFIG_EEPROM_PADSTATION
	unregister_microp_notifier(&led_notifier);
#endif
	led_classdev_unregister(&intel_led);
}

static void intel_kpd_led_rpmsg_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}
static struct rpmsg_device_id intel_kpd_led_id_table[] = {
	{ .name	= "rpmsg_kpd_led" },
	{ },
};

static struct rpmsg_driver intel_kpd_led_rpmsg = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= intel_kpd_led_id_table,
	.probe		= intel_kpd_led_rpmsg_probe,
	.callback	= intel_kpd_led_rpmsg_cb,
	.remove		= intel_kpd_led_rpmsg_remove,
};

static int __init intel_kpd_led_rpmsg_init(void)
{
	return register_rpmsg_driver(&intel_kpd_led_rpmsg);
}

module_init(intel_kpd_led_rpmsg_init);

static void __exit intel_kpd_led_rpmsg_exit(void)
{
	return unregister_rpmsg_driver(&intel_kpd_led_rpmsg);
}
module_exit(intel_kpd_led_rpmsg_exit);

MODULE_DESCRIPTION("Intel Keypad LED Driver");
MODULE_LICENSE("GPL v2");
