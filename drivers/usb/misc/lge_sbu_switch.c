/*
 * LGE_SBU_SWITCH Port Protection Switch driver
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
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#ifdef CONFIG_TYPEC
#include <linux/usb/typec_mux.h>
#include <linux/usb/typec_altmode.h>
#endif

#include <linux/usb/lge_sbu_switch.h>

static unsigned long turn_on_delay_us = 100000;
module_param(turn_on_delay_us, ulong, 0644);

struct lge_sbu_switch {
	struct device			*dev;

	/* GPIOs */
	struct gpio_desc		*ldo_en_desc;
	bool				ldo_en;

	struct gpio_desc		*sbu_oe_desc;
	bool				sbu_oe;

	struct gpio_desc		*sbu_sel_desc;
	bool				sbu_sel;

	struct gpio_desc		*uart_oe_desc;
	bool				uart_oe;

	struct gpio_desc		*uart_sel_desc;
	bool				uart_sel;

	struct gpio_desc		*ovp_desc;
	int				ovp_irq;
	int				ovp;

	/* Flags */
	atomic_t			flags[LGE_SBU_SWITCH_MODE_MAX];
	unsigned long			cur_flag;

	struct mutex			lock;
	struct list_head		inst_list;

#ifdef CONFIG_TYPEC
	struct typec_mux		*mux;
	struct lge_sbu_switch_desc	mux_desc;
	struct lge_sbu_switch_instance	*mux_inst;
	int				mux_state;
#endif
};

static struct lge_sbu_switch *__lge_sbu_switch = NULL;

#ifdef CONFIG_LGE_USB_DEBUGGER
extern int debug_accessory_status;
extern int lge_usb_debugger_enable(bool enable);
#endif

static char *flag_to_string(unsigned long flag)
{
	switch (flag) {
	case LGE_SBU_SWITCH_FLAG_SBU_DISABLE:	return "SBU Disable";
	case LGE_SBU_SWITCH_FLAG_SBU_MD:	return "SBU Moisture Detected";
	case LGE_SBU_SWITCH_FLAG_SBU_FACTORY_ID: return "Factory Cable ID";
	case LGE_SBU_SWITCH_FLAG_SBU_MD_ING:	return "SBU Moisture Detecting";
	case LGE_SBU_SWITCH_FLAG_EDGE_MD:	return "Edge Moisture Detected";
	case LGE_SBU_SWITCH_FLAG_SBU_AUX:	return "AUX";
	case LGE_SBU_SWITCH_FLAG_SBU_UART:	return "UART";
	case LGE_SBU_SWITCH_FLAG_SBU_USBID:	return "USB ID";
	case LGE_SBU_SWITCH_FLAG_EDGE_MD_ING:	return "Edge Moisture Detecting";
	case LGE_SBU_SWITCH_FLAG_SBU_IDLE:      return "Idle";
	default:				return "Unknown";
	}
}

static unsigned long find_next_flag_bit(unsigned long flags,
					unsigned long offset)
{
	unsigned long i;

	if (offset >= LGE_SBU_SWITCH_MODE_MAX ||
	    (!(flags & GENMASK(LGE_SBU_SWITCH_MODE_MAX - 1, offset))))
		return LGE_SBU_SWITCH_MODE_MAX;

	for (i = offset; i < LGE_SBU_SWITCH_MODE_MAX; i++) {
		if (flags & BIT(i))
			break;
	}

	return i;
}

static unsigned long find_first_flag_bit(unsigned long flags)
{
	return find_next_flag_bit(flags, 0UL);
}

#define for_each_flag_bit(bit, flags)				\
	for ((bit) = find_first_flag_bit(flags);		\
	     (bit) < LGE_SBU_SWITCH_MODE_MAX;			\
	     (bit) = find_next_flag_bit(flags, (bit) + 1))

#define GPIOD_SET_VALUE(sw, _name)			\
	do { \
		if (!sw->_name##_desc || sw->_name == _name)	\
			break;					\
		gpiod_set_value(sw->_name##_desc, _name);	\
	} while (0)

static unsigned long update_state(struct lge_sbu_switch *sw)
{
	bool sbu_oe   = sw->sbu_oe;
	bool sbu_sel  = sw->sbu_sel;
	bool ldo_en   = sw->ldo_en;
	bool uart_oe  = sw->uart_oe;
	bool uart_sel = sw->uart_sel;
	unsigned i;

	for (i = 0; i < LGE_SBU_SWITCH_MODE_MAX; i++) {
		if (atomic_read(&sw->flags[i]))
			break;
	}

	if (sw->cur_flag == BIT(i))
		return sw->cur_flag;

	switch (i) {
	case LGE_SBU_SWITCH_MODE_SBU_DISABLE:
		sbu_oe   = 1;
		if (!sw->sbu_oe_desc) sbu_sel = 0;
		ldo_en   = 0;
		uart_oe  = 1;
		break;
	case LGE_SBU_SWITCH_MODE_SBU_FACTORY_ID:
		sbu_oe   = 0;
		sbu_sel  = 0;
		ldo_en   = 1;
		break;
	case LGE_SBU_SWITCH_MODE_SBU_MD:
	case LGE_SBU_SWITCH_MODE_SBU_MD_ING:
		sbu_oe   = 0;
		sbu_sel  = 0;
		ldo_en   = 0;
		uart_oe  = 1;
		break;
	case LGE_SBU_SWITCH_MODE_EDGE_MD:
	case LGE_SBU_SWITCH_MODE_EDGE_MD_ING:
		sbu_oe   = 0;
		sbu_sel  = 1;
		ldo_en   = 0;
		uart_oe  = 0;
		uart_sel = 0;
		break;
	case LGE_SBU_SWITCH_MODE_SBU_AUX:
		sbu_oe   = 0;
		sbu_sel  = 1;
		ldo_en   = 0;
		uart_oe  = 0;
		uart_sel = 0;
		break;
	case LGE_SBU_SWITCH_MODE_SBU_UART:
		sbu_oe   = 0;
		sbu_sel  = 1;
		ldo_en   = 0;
		uart_oe  = 0;
		uart_sel = 1;
		break;
	case LGE_SBU_SWITCH_MODE_SBU_USBID:
		sbu_oe   = 0;
		sbu_sel  = 0;
		ldo_en   = 1;
		break;
	case LGE_SBU_SWITCH_MODE_SBU_IDLE:
		sbu_oe   = 0;
		if (!sw->sbu_oe_desc) sbu_sel = 1;
		ldo_en   = 0;
		uart_oe  = 1;
		uart_sel = 0;
		break;
	default:
		sbu_oe   = 1;
		if (!sw->sbu_oe_desc) sbu_sel = 1;
		ldo_en   = 0;
		uart_oe  = 1;
		uart_sel = 0;
		break;
	}

	GPIOD_SET_VALUE(sw, ldo_en);

	if (sbu_oe) {
		GPIOD_SET_VALUE(sw, sbu_oe);
		GPIOD_SET_VALUE(sw, sbu_sel);

		if (uart_oe) {
			GPIOD_SET_VALUE(sw, uart_oe);
			GPIOD_SET_VALUE(sw, uart_sel);
		} else {
			GPIOD_SET_VALUE(sw, uart_sel);
			GPIOD_SET_VALUE(sw, uart_oe);
		}
	} else {
		if (uart_oe) {
			GPIOD_SET_VALUE(sw, uart_oe);
			GPIOD_SET_VALUE(sw, uart_sel);
		} else {
			GPIOD_SET_VALUE(sw, uart_sel);
			GPIOD_SET_VALUE(sw, uart_oe);
		}

		GPIOD_SET_VALUE(sw, sbu_sel);
		GPIOD_SET_VALUE(sw, sbu_oe);

		/*
		 * Turn-On Time, S, /OE to output.
		 *
		 * A delay is required to properly read the ADC.
		 */
		if (!sbu_sel &&
		    ((sw->sbu_oe != sbu_oe) || (sw->sbu_sel != sbu_sel))) {
			switch (i) {
			case LGE_SBU_SWITCH_MODE_SBU_FACTORY_ID:
			case LGE_SBU_SWITCH_MODE_SBU_MD:
			case LGE_SBU_SWITCH_MODE_SBU_MD_ING:
			case LGE_SBU_SWITCH_MODE_SBU_USBID:
					usleep_range(turn_on_delay_us,
					     turn_on_delay_us);
				break;
			default:
				break;
			}
		}
	}

	sw->sbu_oe   = sbu_oe;
	sw->sbu_sel  = sbu_sel;
	sw->ldo_en   = ldo_en;
	sw->uart_oe  = uart_oe;
	sw->uart_sel = uart_sel;
	sw->cur_flag = BIT(i);

	dev_info(sw->dev, "SBU(/OE:%d, SEL:%d), UART(/OE:%d, SEL:%d), LDO_EN:%d, current flag is \"%s\"\n",
		 sw->sbu_oe, sw->sbu_sel, sw->uart_oe, sw->uart_sel, sw->ldo_en,
		 flag_to_string(sw->cur_flag));

	return BIT(i);
}

int lge_sbu_switch_get(struct lge_sbu_switch_instance *inst, unsigned long flag)
{
	struct lge_sbu_switch *sw = __lge_sbu_switch;
	unsigned long flag_bit;

	if (!inst || flag >= LGE_SBU_SWITCH_FLAG_MAX || !(inst->desc->flags & flag))
		return -EINVAL;

	dev_dbg(sw->dev, "%s get \"%s\"\n",
		dev_driver_string(inst->dev), flag_to_string(flag));

	mutex_lock(&sw->lock);

	flag_bit = find_first_flag_bit(flag);
	if (test_and_set_bit(flag_bit, &inst->flags))
		goto out;

	atomic_inc(&sw->flags[flag_bit]);
	update_state(sw);
out:
	mutex_unlock(&sw->lock);

	return sw->cur_flag == flag ? 0 : -EBUSY;
}
EXPORT_SYMBOL(lge_sbu_switch_get);

int lge_sbu_switch_put(struct lge_sbu_switch_instance *inst, unsigned long flag)
{
	struct lge_sbu_switch *sw = __lge_sbu_switch;
	unsigned long flag_bit;

	if (!inst || flag >= LGE_SBU_SWITCH_FLAG_MAX || !(inst->desc->flags & flag))
		return -EINVAL;

	dev_dbg(sw->dev, "%s put \"%s\"\n",
		dev_driver_string(inst->dev), flag_to_string(flag));

	mutex_lock(&sw->lock);

	flag_bit = find_first_flag_bit(flag);
	if (!test_and_clear_bit(flag_bit, &inst->flags))
		goto out;

	atomic_dec(&sw->flags[flag_bit]);
	update_state(sw);
out:
	mutex_unlock(&sw->lock);

	return sw->cur_flag == flag ? 0 : -EBUSY;
}
EXPORT_SYMBOL(lge_sbu_switch_put);

unsigned long lge_sbu_switch_get_current_flag(struct lge_sbu_switch_instance *inst)
{
	struct lge_sbu_switch *sw = __lge_sbu_switch;
	unsigned long cur_flag;

	if (!sw || !inst)
		return LGE_SBU_SWITCH_FLAG_MAX;

	mutex_lock(&sw->lock);
	cur_flag = sw->cur_flag;
	mutex_unlock(&sw->lock);

	dev_dbg(sw->dev, "current flag is \"%s\"\n",
		flag_to_string(cur_flag));

	return cur_flag;
}
EXPORT_SYMBOL(lge_sbu_switch_get_current_flag);

static irqreturn_t lge_sbu_switch_ovp_irq_thread(int irq, void *data)
{
	struct lge_sbu_switch *sw = (struct lge_sbu_switch *)data;
	int ovp;
	struct list_head *pos;
	struct list_head *tmp;

	ovp = !gpiod_get_value(sw->ovp_desc);
	dev_dbg(sw->dev, "ovp old(%d), new(%d)\n", sw->ovp, ovp);

	if (ovp == sw->ovp)
		return IRQ_HANDLED;

	dev_info(sw->dev, "ovp (%d)\n", ovp);
	sw->ovp = ovp;

	list_for_each_safe(pos, tmp, &sw->inst_list) {
		struct lge_sbu_switch_instance *inst = container_of(pos,
			struct lge_sbu_switch_instance, list);

		if (inst->desc->ovp_callback)
			inst->desc->ovp_callback(inst, ovp);
	}

	return IRQ_HANDLED;
}

int lge_sbu_switch_get_ovp_state(struct lge_sbu_switch_instance *inst)
{
	struct lge_sbu_switch *sw = __lge_sbu_switch;

	if (!sw || !inst || !sw->ovp_desc)
		return -EINVAL;

	return sw->ovp;
}
EXPORT_SYMBOL(lge_sbu_switch_get_ovp_state);

void devm_lge_sbu_switch_instance_unregister(struct device *dev,
				      struct lge_sbu_switch_instance *inst)
{
	struct lge_sbu_switch *sw = __lge_sbu_switch;
	unsigned long flag_bit;

	if (!dev || !inst || dev != inst->dev)
		return;

	dev_dbg(sw->dev, "%s unregister\n", dev_driver_string(dev));

	for_each_flag_bit(flag_bit, inst->flags)
		lge_sbu_switch_put(inst, BIT(flag_bit));

	list_del(&inst->list);
	kfree(inst);
}
EXPORT_SYMBOL(devm_lge_sbu_switch_instance_unregister);

struct lge_sbu_switch_instance *__must_check
devm_lge_sbu_switch_instance_register(struct device *dev,
			       const struct lge_sbu_switch_desc *desc)
{
	struct lge_sbu_switch *sw = __lge_sbu_switch;
	struct lge_sbu_switch_instance *inst;
	unsigned long flag_bit;

	if (!sw)
		return ERR_PTR(-EPROBE_DEFER);

	if (!dev || desc->flags >= LGE_SBU_SWITCH_FLAG_MAX)
		return ERR_PTR(-EINVAL);

	dev_dbg(sw->dev, "%s register\n", dev_driver_string(dev));

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return ERR_PTR(-ENOMEM);

	inst->dev = dev;
	inst->desc = desc;

	INIT_LIST_HEAD(&inst->list);
	list_add(&inst->list, &sw->inst_list);

	for_each_flag_bit(flag_bit, desc->flags) {
		dev_info(sw->dev, "%s registered \"%s\"\n",
			 dev_driver_string(dev), flag_to_string(BIT(flag_bit)));
	}

	return inst;
}
EXPORT_SYMBOL(devm_lge_sbu_switch_instance_register);

#ifdef CONFIG_TYPEC
static int lge_sbu_switch_typec_mux_set(struct typec_mux *mux, int state)
{
	struct lge_sbu_switch *sw = typec_mux_get_drvdata(mux);

	dev_dbg(sw->dev, "%s: state:%d\n", __func__, state);

	if (sw->mux_state == state)
		return 0;

	switch (state) {
	case TYPEC_STATE_SAFE:
		switch (sw->mux_state) {
		case TYPEC_STATE_USB:
			lge_sbu_switch_put(sw->mux_inst,
					LGE_SBU_SWITCH_FLAG_SBU_AUX);
			break;
#ifdef CONFIG_LGE_USB_DEBUGGER
		case -1:
			if (!debug_accessory_status)
				break;

			mutex_lock(&sw->lock);
			sw->cur_flag = -1;
			update_state(sw);
			mutex_unlock(&sw->lock);
			/* fall through */
		case TYPEC_MODE_DEBUG:
			lge_usb_debugger_enable(0);
			break;
#endif
		default:
			break;
		}
		break;
	case TYPEC_STATE_USB:
		lge_sbu_switch_get(sw->mux_inst, LGE_SBU_SWITCH_FLAG_SBU_AUX);
		break;
#ifdef CONFIG_LGE_USB_DEBUGGER
	case TYPEC_MODE_DEBUG:
		lge_usb_debugger_enable(1);
		break;
#endif
	default:
		dev_err(sw->dev, "%s: unknown state %d\n", __func__, state);
		return -EINVAL;
	}

	sw->mux_state = state;

	return 0;
}
#endif

static int lge_sbu_switch_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
//	struct device_node *node = pdev->dev.of_node;
	struct lge_sbu_switch *sw;
	struct pinctrl* gpio_pinctrl;
	struct pinctrl_state* gpio_state;
	int ret;
#ifdef CONFIG_TYPEC
	struct typec_mux_desc mux_desc;
#endif

	dev_info(&pdev->dev, "%s\n", __func__);

	sw = devm_kzalloc(dev, sizeof(*sw), GFP_KERNEL);
	if (!sw) {
		dev_err(dev, "out of memory\n");
		return -ENOMEM;
	}

	sw->dev = &pdev->dev;

	/* gpio default values */
	sw->sbu_oe   = 1;
	sw->sbu_sel  = device_property_read_bool(dev, "lge,oe-gpio") ? 0 : 1;
	sw->ldo_en   = 0;
	sw->uart_oe  = 1;
	sw->uart_sel = 0;
	sw->cur_flag = LGE_SBU_SWITCH_FLAG_MAX;

	// Set SBU_IDLE to the default state
	sw->sbu_oe   = 0;
	sw->cur_flag = LGE_SBU_SWITCH_FLAG_SBU_IDLE;
	atomic_set(&sw->flags[LGE_SBU_SWITCH_MODE_SBU_IDLE], 1);

	dev_info(sw->dev, "SBU(/OE:%d, SEL:%d), UART(/OE:%d, SEL:%d), LDO_EN:%d, current flag is \"%s\"\n",
		 sw->sbu_oe, sw->sbu_sel, sw->uart_oe, sw->uart_sel, sw->ldo_en,
		 flag_to_string(sw->cur_flag));

#ifdef CONFIG_LGE_USB_DEBUGGER
	if (debug_accessory_status) {
		sw->sbu_oe   = 0;
		sw->sbu_sel  = 1;
		sw->uart_oe  = 0;
		sw->uart_sel = 1;
		dev_info(sw->dev, "SBU(/OE:%d, SEL:%d), UART(/OE:%d, SEL:%d), USB Debugger is connected\n",
			 sw->sbu_oe, sw->sbu_sel, sw->uart_oe, sw->uart_sel);
	}
#endif

	sw->ldo_en_desc = devm_gpiod_get(dev, "lge,ldo-en",
			sw->ldo_en ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
	if (IS_ERR(sw->ldo_en_desc)) {
		dev_err(dev, "couldn't get ldo en gpio_desc\n");
		sw->ldo_en_desc = NULL;
		sw->ldo_en = 0;
	}

	sw->uart_sel_desc = devm_gpiod_get(dev, "lge,uart-sbu-sel",
			sw->uart_sel ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
	if (IS_ERR(sw->uart_sel_desc)) {
		dev_err(dev, "Could not get uart sel gpio_desc\n");
		sw->uart_sel_desc = NULL;
		sw->uart_sel = 0;
	}

	sw->uart_oe_desc = devm_gpiod_get(dev, "lge,uart-edp-oe",
			sw->uart_oe ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
	if (IS_ERR(sw->uart_oe_desc)) {
		dev_err(dev, "Could not get uart oe gpio_desc\n");
		sw->uart_oe_desc = NULL;
		sw->uart_oe = 0;
	}

	sw->sbu_sel_desc = devm_gpiod_get(dev, "lge,sel",
			sw->sbu_sel ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
	if (IS_ERR(sw->sbu_sel_desc)) {
		dev_err(dev, "Could not get sel gpio_desc\n");
		sw->sbu_sel_desc = NULL;
		sw->sbu_sel = 0;
		ret = PTR_ERR(sw->sbu_sel_desc);
		goto err;
	}

	sw->sbu_oe_desc = devm_gpiod_get(dev, "lge,oe",
			sw->sbu_oe ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
	if (IS_ERR(sw->sbu_oe_desc)) {
		dev_err(dev, "Could not get oe gpio_desc\n");
		sw->sbu_oe_desc = NULL;
		sw->sbu_oe = 0;
	}

	sw->ovp_desc = devm_gpiod_get(dev, "lge,ovp", GPIOD_IN);
	if (IS_ERR(sw->ovp_desc)) {
		dev_info(dev, "Could not get ovp gpio_desc\n");
		sw->ovp_desc = NULL;
		goto skip_ovp;
	}
	sw->ovp_irq = gpiod_to_irq(sw->ovp_desc);

	ret = request_threaded_irq(sw->ovp_irq,
		NULL, lge_sbu_switch_ovp_irq_thread,
		IRQF_ONESHOT | IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
		"lge_sbu_switch-ovp", sw);
	if (ret) {
		dev_err(dev, "Cannot request ovp irq\n");
		goto err;
	}
	enable_irq_wake(sw->ovp_irq);
skip_ovp:

	gpio_pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(gpio_pinctrl)) {
		dev_err(dev, "Failed to get pinctrl (%ld)\n", PTR_ERR(gpio_pinctrl));
		ret = PTR_ERR(gpio_pinctrl);
		goto err;
	}

	gpio_state = pinctrl_lookup_state(gpio_pinctrl, "default");
	if (IS_ERR_OR_NULL(gpio_state)) {
		dev_err(dev, "pinstate not found, %ld\n", PTR_ERR(gpio_state));
		ret = PTR_ERR(gpio_state);
		goto err;
	}

	ret = pinctrl_select_state(gpio_pinctrl, gpio_state);
	if (ret < 0) {
		dev_err(dev, "cannot set pins %d\n", ret);
		goto err;
	}

	mutex_init(&sw->lock);
	INIT_LIST_HEAD(&sw->inst_list);

	__lge_sbu_switch = sw;

#ifdef CONFIG_TYPEC
	sw->mux_state = -1;

	sw->mux_desc.flags = LGE_SBU_SWITCH_FLAG_SBU_AUX;
	sw->mux_inst = devm_lge_sbu_switch_instance_register(dev,
			&sw->mux_desc);
	if (!sw->mux_inst) {
		dev_err(dev, "could not get instance of mux\n");
		goto err;
	}

	mux_desc.drvdata = sw;
	mux_desc.fwnode = dev->fwnode;
	mux_desc.set = lge_sbu_switch_typec_mux_set;

	sw->mux = typec_mux_register(dev, &mux_desc);
	if (IS_ERR(sw->mux)) {
		dev_err(dev, "error registering typec mux: %ld\n", PTR_ERR(sw->mux));
		ret = PTR_ERR(sw->mux);
		goto err;
	}
#endif

	return 0;
err:
	__lge_sbu_switch = NULL;
	return ret;
}

static const struct of_device_id lge_sbu_switch_match_table[] = {
	{ .compatible = "lge,lge_sbu_switch" },
	{ }
};
MODULE_DEVICE_TABLE(of, lge_sbu_switch_match_table);

static struct platform_driver lge_sbu_switch_driver = {
	.driver = {
		.name = "lge_sbu_switch",
		.of_match_table = lge_sbu_switch_match_table,
	},
	.probe = lge_sbu_switch_probe,
};
module_platform_driver(lge_sbu_switch_driver);

MODULE_DESCRIPTION("LGE CC/SBU Protection Switch driver");
MODULE_LICENSE("GPL v2");
