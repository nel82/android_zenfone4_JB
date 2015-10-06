/*
 *  drivers/switch/switch_mid.c
 *
 * Copyright (C) 2010 Intel.
 *
 * Based on drivers/switch/switch_gpio.c which from Google Android.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/slab.h>

struct mid_switch_data {
	struct switch_dev sdev;
};

static char *name_dock_in = "dock_in";
static char *name_dock_out = "dock_out";
static char *state_dock_in = "1";
static char *state_dock_out = "0";

static struct mid_switch_data *dock_switch_data;

void mid_dock_report(int state)
{
	pr_debug("%s : state %d\n", __func__, state);
	if (dock_switch_data)
		switch_set_state(&dock_switch_data->sdev, state);
}
EXPORT_SYMBOL_GPL(mid_dock_report);

static ssize_t dock_print_name(struct switch_dev *sdev, char *buf)
{
	const char *name;

	if (!buf)
		return -EINVAL;

	switch (switch_get_state(sdev)) {
	case 0:
		name = name_dock_out;
		break;
	case 1:
		name = name_dock_in;
		break;
	default:
		name = NULL;
		break;
	}

	if (name)
		return sprintf(buf, "%s\n", name);
	else
		return -EINVAL;
}

static ssize_t dock_print_state(struct switch_dev *sdev, char *buf)
{
	const char *state;

	if (!buf)
		return -EINVAL;

	switch (switch_get_state(sdev)) {
	case 0:
		state = state_dock_out;
		break;
	case 1:
		state = state_dock_in;
		break;
	default:
		state = NULL;
		break;
	}

	if (state)
		return sprintf(buf, "%s\n", state);
	else
		return -EINVAL;
}

static int mid_switch_dock_probe(struct platform_device *pdev)
{
	struct mid_switch_data *switch_data;
	int ret = 0;

	switch_data = kzalloc(sizeof(struct mid_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;
	dock_switch_data = switch_data;

	switch_data->sdev.name = "dock";
	switch_data->sdev.print_name = dock_print_name;
	switch_data->sdev.print_state = dock_print_state;

	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	platform_set_drvdata(pdev, switch_data);
	pr_debug("%s : probe success\n", __func__);
	return 0;

err_switch_dev_register:
	kfree(switch_data);
	return ret;
}

static int mid_switch_dock_remove(struct platform_device *pdev)
{
	struct mid_switch_data *switch_data = platform_get_drvdata(pdev);

	switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);
	dock_switch_data = NULL;

	return 0;
}

static struct platform_driver mid_switch_dock_driver = {
	.probe		= mid_switch_dock_probe,
	.remove		= mid_switch_dock_remove,
	.driver		= {
		.name	= "switch-dock",
		.owner	= THIS_MODULE,
	},
};

static int __init mid_switch_dock_init(void)
{
	pr_debug("%s\n", __func__);
	return platform_driver_register(&mid_switch_dock_driver);
}

static void __exit mid_switch_dock_exit(void)
{
	platform_driver_unregister(&mid_switch_dock_driver);
}

module_init(mid_switch_dock_init);
module_exit(mid_switch_dock_exit);

MODULE_AUTHOR("Deng, Bing (bingx.deng@intel.com)");
MODULE_DESCRIPTION("Dock Switch driver");
MODULE_LICENSE("GPL");
