/*
 * LGE USB UART debugger driver
 *
 * Copyright (C) 2018 LG Electronics, Inc.
 * Author: Hansun Lee <hansun.lee@lge.com>
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
//#define DEBUG

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#ifdef CONFIG_LGE_PM_VENEER_PSY
#include <linux/power_supply.h>
#endif
#ifdef CONFIG_LGE_USB_SBU_SWITCH
#include <linux/usb/lge_sbu_switch.h>
#endif

struct lge_usb_debugger {
	struct device		*dev;
	struct delayed_work	work;
	bool			enable;

	struct gpio_desc	*uart_edp_sel_gpio;

#ifdef CONFIG_LGE_PM_VENEER_PSY
	struct power_supply	*usb_psy;
#endif

#ifdef CONFIG_LGE_USB_SBU_SWITCH
	struct lge_sbu_switch_desc      sbu_desc;
	struct lge_sbu_switch_instance  *sbu_inst;
#endif
};

int debug_accessory_status = 0;
int debug_accessory_detected = -1;
struct lge_usb_debugger *_dbg = NULL;

extern int msm_geni_serial_set_uart_console(int enable);
extern void msm_geni_serial_set_uart_console_status(int status);

static void lge_usb_debugger_work(struct work_struct *w)
{
	struct lge_usb_debugger *dbg = container_of(w,
			struct lge_usb_debugger, work.work);
#if defined(CONFIG_LGE_USB_FACTORY) && defined(CONFIG_LGE_PM_VENEER_PSY)
	union power_supply_propval val;
	int ret;
#endif

	dev_dbg(dbg->dev, "detected:%d, status:%d\n",
		debug_accessory_detected, debug_accessory_status);

	if (debug_accessory_detected) {
		if (dbg->enable && debug_accessory_status)
			return;

#if defined(CONFIG_LGE_USB_FACTORY) && defined(CONFIG_LGE_PM_VENEER_PSY)
		if (!dbg->usb_psy) {
			dbg->usb_psy = power_supply_get_by_name("usb");
			if (!dbg->usb_psy) {
				dev_err(dbg->dev, "Could not get usb psy\n");
				schedule_delayed_work(&dbg->work,
						msecs_to_jiffies(1000 * 5));
				return;
			}
		}

		ret = power_supply_set_property(dbg->usb_psy,
				POWER_SUPPLY_PROP_EXT_RESISTANCE, &val);
		if (ret) {
			dev_err(dbg->dev, "Could not set EXT_RESISTANCE %d\n",
				ret);
			goto skip_check_factory_cable;
		}

		ret = power_supply_get_property(dbg->usb_psy,
				POWER_SUPPLY_PROP_EXT_RESISTANCE_ID, &val);
		if (ret) {
			dev_err(dbg->dev, "Could not get EXT_RESISTANCE_ID %d\n",
				ret);
			goto skip_check_factory_cable;
		}

		switch (val.intval / 1000) {
		case 56:
		case 130:
		case 910:
			dev_info(dbg->dev,"Factory USB connected\n");
			if (debug_accessory_status)
				goto disable_usb_debugger;
			return;
		default:
			break;
		}

skip_check_factory_cable:
#endif

		if (dbg->uart_edp_sel_gpio)
			gpiod_direction_output(dbg->uart_edp_sel_gpio, 1);

#ifdef CONFIG_LGE_USB_SBU_SWITCH
		lge_sbu_switch_get(dbg->sbu_inst, LGE_SBU_SWITCH_FLAG_SBU_UART);
#endif

		debug_accessory_status = 1;
		msm_geni_serial_set_uart_console_status(1);
		msm_geni_serial_set_uart_console(1);

		dbg->enable = 1;
		dev_info(dbg->dev, "UART ON\n");

	} else {
		if (!dbg->enable && !debug_accessory_status)
			return;

#if defined(CONFIG_LGE_USB_FACTORY) && defined(CONFIG_LGE_PM_VENEER_PSY)
disable_usb_debugger:
#endif

		msm_geni_serial_set_uart_console(0);
		msm_geni_serial_set_uart_console_status(0);
		debug_accessory_status = 0;

#ifdef CONFIG_LGE_USB_SBU_SWITCH
		lge_sbu_switch_put(dbg->sbu_inst, LGE_SBU_SWITCH_FLAG_SBU_UART);
#endif

		if (dbg->uart_edp_sel_gpio)
			gpiod_direction_output(dbg->uart_edp_sel_gpio, 0);

		dbg->enable = 0;
		dev_info(dbg->dev, "UART OFF\n");
	}
}

int lge_usb_debugger_enable(bool enable)
{
	struct lge_usb_debugger *dbg = _dbg;

	pr_debug("%s: %d\n", __func__, enable);

	if (debug_accessory_detected == enable)
		return 0;

	debug_accessory_detected = enable;

	if (!dbg)
		return -EPROBE_DEFER;

	cancel_delayed_work(&dbg->work);

	if (enable)
		schedule_delayed_work(&dbg->work, 0);
	else
		lge_usb_debugger_work(&dbg->work.work);

	return 0;
}
EXPORT_SYMBOL(lge_usb_debugger_enable);

static int lge_usb_debugger_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lge_usb_debugger *dbg;
	int ret = 0;

	dev_info(&pdev->dev, "%s\n", __func__);

	dbg = kzalloc(sizeof(*dbg), GFP_KERNEL);
	if (!dbg) {
		dev_err(dev, "out of memory\n");
		return -ENOMEM;
	}

	dbg->dev = &pdev->dev;

	dbg->uart_edp_sel_gpio = devm_gpiod_get(dev,
			"lge,uart-sbu-sel", GPIOD_OUT_LOW);
	if (IS_ERR(dbg->uart_edp_sel_gpio)) {
		ret = PTR_ERR(dbg->uart_edp_sel_gpio);
		dev_err(dev, "Could not get uart-sbu-sel gpio_desc : %d\n", ret);
		dbg->uart_edp_sel_gpio = NULL;
	}

	INIT_DELAYED_WORK(&dbg->work, lge_usb_debugger_work);

#ifdef CONFIG_LGE_USB_SBU_SWITCH
	dbg->sbu_desc.flags = LGE_SBU_SWITCH_FLAG_SBU_UART;
	dbg->sbu_inst = devm_lge_sbu_switch_instance_register(dev,
							      &dbg->sbu_desc);
	if (!dbg->sbu_inst) {
		dev_err(dev, "Could not get LGE_SBU_SWITCH, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto err_lge_sbu_switch_register;
	}
#endif

	_dbg = dbg;

	if ((debug_accessory_detected == 0 && debug_accessory_status) ||
	    (debug_accessory_detected == 1 && !debug_accessory_status))
		lge_usb_debugger_work(&dbg->work.work);

	return 0;

#ifdef CONFIG_LGE_USB_SBU_SWITCH
err_lge_sbu_switch_register:
#endif
	if (dbg->uart_edp_sel_gpio)
		gpiod_put(dbg->uart_edp_sel_gpio);
	kfree(dbg);
	return ret;
}

static const struct of_device_id lge_usb_debugger_match_table[] = {
	{ .compatible = "lge,usb_debugger" },
	{ }
};
MODULE_DEVICE_TABLE(of, lge_usb_debugger_match_table);

static struct platform_driver lge_usb_debugger_driver = {
	.driver = {
		.name = "lge_usb_debugger",
		.of_match_table = lge_usb_debugger_match_table,
	},
	.probe = lge_usb_debugger_probe,
};
module_platform_driver(lge_usb_debugger_driver);

MODULE_DESCRIPTION("LGE USB UART debugger driver");
MODULE_LICENSE("GPL v2");
