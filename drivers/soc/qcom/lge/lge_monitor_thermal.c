/* Copyright (c) 2013-2014, LG Eletronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/cpu.h>
#include <linux/platform_device.h>

#include <soc/qcom/lge/board_lge.h>
#include <linux/thermal.h>

#include <../../../../kernel/sched/sched.h>
#include <linux/power_supply.h>

#define MODULE_NAME "monitor-thermal"
#define CLUSTER_CHECK(x) ((x==0 || x==4 || x==7) ? 1 : 0)

static struct workqueue_struct *monitor_wq;
extern struct list_head thermal_cdev_debug_list;

struct lge_monitor_thermal_data {
	struct device *dev;
	unsigned int polling_time;
	int last_xo_temp;
	int last_quiet_temp;
	int last_skin_temp;
	int last_vts_temp;

	int last_qtm_c_temp;
	int last_qtm_s_temp;
	int last_qtm_c_vts_temp;
	int last_qtm_s_vts_temp;

	int last_batt_temp;
	int last_batt_current;
	int last_batt_voltage;
	int last_batt_soc;

	struct thermal_zone_device *tz_xo;
	struct thermal_zone_device *tz_quiet;
	struct thermal_zone_device *tz_skin;
	struct thermal_zone_device *tz_vts;

	struct thermal_zone_device *tz_qtm_c;
	struct thermal_zone_device *tz_qtm_s;
	struct thermal_zone_device *tz_qtm_c_vts;
	struct thermal_zone_device *tz_qtm_s_vts;

	struct delayed_work init_monitor_work_struct;
	struct delayed_work monitor_work_struct;
	struct power_supply     *batt_psy;
};

/*
 * On the kernel command line specify
 * lge_monitor_thermal.enable=1 to enable monitoring thermal node.
 */
static int enable = 1;
module_param(enable, int, 0);

static int forced_vts = 0;
static int forced_qtm_c_vts = 0;
static int forced_qtm_s_vts = 0;

#ifdef CONFIG_LGE_ONE_BINARY_SKU
static enum lge_sku_carrier_type sku_carrier = HW_SKU_MAX;
#endif

static void poll_monitor_work(struct work_struct *work);
static void init_monitor_work(struct work_struct *work);

static int lge_monitor_thermal_suspend(struct device *dev)
{
return 0;
}

static int lge_monitor_thermal_resume(struct device *dev)
{
return 0;
}

static void get_tz_devs(struct lge_monitor_thermal_data *monitor_dd)
{
	if (!monitor_dd ||
	    (monitor_dd->tz_xo && monitor_dd->tz_quiet &&
	    monitor_dd->tz_skin && monitor_dd->tz_vts)) {
		return;
	}
	monitor_dd->tz_xo = thermal_zone_get_zone_by_name("xo-therm-usr");
	monitor_dd->tz_quiet = thermal_zone_get_zone_by_name("quiet-therm-usr");
	monitor_dd->tz_skin = thermal_zone_get_zone_by_name("skin-therm-usr");
#ifdef CONFIG_LGE_ONE_BINARY_SKU
	if (sku_carrier == HW_SKU_NA_CDMA_VZW) {
	    pr_info("[TM][VTS] changed from VTS to modem-vts beause of carrier supporting mmW \n");
	    monitor_dd->tz_vts = thermal_zone_get_zone_by_name("mmw-vts-therm");
	} else {
	    monitor_dd->tz_vts = thermal_zone_get_zone_by_name("vts-virt-therm");
	}
#else
	monitor_dd->tz_vts = thermal_zone_get_zone_by_name("vts-virt-therm");
#endif
}

static void get_qtm_tz_devs(struct lge_monitor_thermal_data *monitor_dd)
{
	if (!monitor_dd ||
	    (monitor_dd->tz_qtm_c && monitor_dd->tz_qtm_s &&
	    monitor_dd->tz_qtm_c_vts && monitor_dd->tz_qtm_s_vts)) {
		return;
	}

	monitor_dd->tz_qtm_c = thermal_zone_get_zone_by_name("qtm-c-therm-usr");
	monitor_dd->tz_qtm_s = thermal_zone_get_zone_by_name("qtm-s-therm-usr");
	monitor_dd->tz_qtm_c_vts = thermal_zone_get_zone_by_name("qtm-c-vts-therm");
	monitor_dd->tz_qtm_s_vts = thermal_zone_get_zone_by_name("qtm-s-vts-therm");
}

static ssize_t lge_vts_temp_get(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
	int ret;
	int vts_temp, init_temp = 250;

	struct lge_monitor_thermal_data *monitor_dd = dev_get_drvdata(dev);

	if (!monitor_dd || !monitor_dd->tz_vts) {
		return snprintf(buf, PAGE_SIZE, "%d\n", init_temp);
	}

	if (forced_vts)
		return snprintf(buf, PAGE_SIZE, "%d\n", forced_vts);

	ret = thermal_zone_get_temp(monitor_dd->tz_vts, &vts_temp);
	if (ret)
		snprintf(buf, PAGE_SIZE, "%d\n", init_temp);

	vts_temp = vts_temp / 100;
	return snprintf(buf, PAGE_SIZE, "%d\n", vts_temp);
}

static ssize_t lge_vts_temp_set(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	forced_vts = val;

	return count;
}

static DEVICE_ATTR(vts_temp, S_IRUGO|S_IWUSR,
                   lge_vts_temp_get, lge_vts_temp_set);

static ssize_t lge_qtm_c_vts_temp_get(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
	int ret;
	int temp, init_temp = 250;

	struct lge_monitor_thermal_data *monitor_dd = dev_get_drvdata(dev);

	if (!monitor_dd || !monitor_dd->tz_qtm_c_vts) {
		return snprintf(buf, PAGE_SIZE, "%d\n", init_temp);
	}

	if (forced_qtm_c_vts)
		return snprintf(buf, PAGE_SIZE, "%d\n", forced_qtm_c_vts);

	ret = thermal_zone_get_temp(monitor_dd->tz_qtm_c_vts, &temp);
	if (ret)
		snprintf(buf, PAGE_SIZE, "%d\n", init_temp);

	temp = temp / 100;
	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t lge_qtm_c_vts_temp_set(struct device *dev,
                                      struct device_attribute *attr,
                                      const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	forced_qtm_c_vts = val;

	return count;
}

static DEVICE_ATTR(qtm_c_vts_temp, S_IRUGO|S_IWUSR,
                   lge_qtm_c_vts_temp_get, lge_qtm_c_vts_temp_set);

static ssize_t lge_qtm_s_vts_temp_get(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
	int ret;
	int temp, init_temp = 250;

	struct lge_monitor_thermal_data *monitor_dd = dev_get_drvdata(dev);

	if (!monitor_dd || !monitor_dd->tz_qtm_s_vts) {
		return snprintf(buf, PAGE_SIZE, "%d\n", init_temp);
	}

	if (forced_qtm_s_vts)
		return snprintf(buf, PAGE_SIZE, "%d\n", forced_qtm_s_vts);

	ret = thermal_zone_get_temp(monitor_dd->tz_qtm_s_vts, &temp);
	if (ret)
		snprintf(buf, PAGE_SIZE, "%d\n", init_temp);

	temp = temp / 100;
	return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}

static ssize_t lge_qtm_s_vts_temp_set(struct device *dev,
                                      struct device_attribute *attr,
                                      const char *buf, size_t count)
{
	int val;
	if (kstrtoint(buf, 10, &val))
		return -EINVAL;

	forced_qtm_s_vts = val;

	return count;
}

static DEVICE_ATTR(qtm_s_vts_temp, S_IRUGO|S_IWUSR,
                   lge_qtm_s_vts_temp_get, lge_qtm_s_vts_temp_set);

static ssize_t lge_monitor_disable_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct lge_monitor_thermal_data *monitor_dd = dev_get_drvdata(dev);

	ret = snprintf(buf, PAGE_SIZE, "En:%d Poll-time:%d sec\n",
						enable == 0 ? 1 : 0,
						monitor_dd->polling_time);
	return ret;
}

static ssize_t lge_monitor_disable_set(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int ret;
	u8 disable;
	struct lge_monitor_thermal_data *monitor_dd = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 10, &disable);
	if (ret) {
		dev_err(monitor_dd->dev, "invalid user input\n");
		return ret;
	}

	if (enable && disable) {
		/* If have already delayed_work, cancel delayed_work. */
		cancel_delayed_work_sync(&monitor_dd->monitor_work_struct);
	}

	enable = (disable == 1) ? 0 : 1;

	return count;
}

static DEVICE_ATTR(disable, S_IWUSR | S_IRUSR, lge_monitor_disable_get,
						lge_monitor_disable_set);

#define NUMS_CDEV 12
static void _poll_monitor(struct lge_monitor_thermal_data *monitor_dd)
{
	int cpu, ret;
	union power_supply_propval prop = {0, };
	struct cpufreq_policy policy;
	struct thermal_cooling_device *cdev = NULL;
	unsigned long state;

	get_tz_devs(monitor_dd);

	if (monitor_dd->tz_xo && monitor_dd->tz_quiet &&
	    monitor_dd->tz_skin && monitor_dd->tz_vts) {
		thermal_zone_get_temp(monitor_dd->tz_xo,
				&monitor_dd->last_xo_temp);
		thermal_zone_get_temp(monitor_dd->tz_quiet,
				&monitor_dd->last_quiet_temp);
		thermal_zone_get_temp(monitor_dd->tz_skin,
				&monitor_dd->last_skin_temp);
		thermal_zone_get_temp(monitor_dd->tz_vts,
				&monitor_dd->last_vts_temp);
	}

#ifdef CONFIG_LGE_ONE_BINARY_SKU
	if (sku_carrier == HW_SKU_NA_CDMA_VZW) {
		get_qtm_tz_devs(monitor_dd);
		if (monitor_dd->tz_qtm_c && monitor_dd->tz_qtm_s &&
			monitor_dd->tz_qtm_c_vts && monitor_dd->tz_qtm_s_vts) {
			thermal_zone_get_temp(monitor_dd->tz_qtm_c,
					&monitor_dd->last_qtm_c_temp);
			thermal_zone_get_temp(monitor_dd->tz_qtm_s,
					&monitor_dd->last_qtm_s_temp);
			thermal_zone_get_temp(monitor_dd->tz_qtm_c_vts,
					&monitor_dd->last_qtm_c_vts_temp);
			thermal_zone_get_temp(monitor_dd->tz_qtm_s_vts,
					&monitor_dd->last_qtm_s_vts_temp);
		}
	}
#endif

	if (!monitor_dd->batt_psy) {
		monitor_dd->batt_psy = power_supply_get_by_name("battery");
		if (!monitor_dd->batt_psy) {
			pr_err("failed get batt_psy\n");
			return;
		}
	}

	ret = power_supply_get_property(monitor_dd->batt_psy,
			POWER_SUPPLY_PROP_TEMP, &prop);
	if (unlikely(ret < 0)) {
		pr_err("Error in getting battery temp, ret=%d\n", ret);
		monitor_dd->last_batt_temp = 0;
	} else
		monitor_dd->last_batt_temp = prop.intval;

	ret = power_supply_get_property(monitor_dd->batt_psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
	if (unlikely(ret < 0)) {
		pr_err("Error in getting battery current, ret=%d\n", ret);
		monitor_dd->last_batt_current = 0;
	} else
		monitor_dd->last_batt_current = prop.intval;

	ret = power_supply_get_property(monitor_dd->batt_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
	if (unlikely(ret < 0)) {
		pr_err("Error in getting battery voltage, ret=%d\n", ret);
		monitor_dd->last_batt_voltage = 4000;
	} else
		monitor_dd->last_batt_voltage = prop.intval;

	ret = power_supply_get_property(monitor_dd->batt_psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (unlikely(ret < 0)) {
		pr_err("Error in getting battery capacity, ret=%d\n", ret);
		monitor_dd->last_batt_soc = 50;
	} else
		monitor_dd->last_batt_soc = prop.intval;

	pr_info("[TM][I] XO:%3d, QUIET:%3d, MSM_SKIN:%3d, VTS:%3d\n",
			monitor_dd->last_xo_temp/1000,
			monitor_dd->last_quiet_temp/1000,
			monitor_dd->last_skin_temp/1000,
			monitor_dd->last_vts_temp/1000);

#ifdef CONFIG_LGE_ONE_BINARY_SKU
	if (sku_carrier == HW_SKU_NA_CDMA_VZW) {
		pr_info("[TM][M] VTS:%3d, QTM_C:%3d, QTM_S:%3d, QTM_C_VTS:%3d, QTM_S_VTS:%3d",
			 monitor_dd->last_vts_temp/1000,
			 monitor_dd->last_qtm_c_temp/1000,
			 monitor_dd->last_qtm_s_temp/1000,
			 monitor_dd->last_qtm_c_vts_temp/1000,
			 monitor_dd->last_qtm_s_vts_temp/1000);
	}
#endif

	pr_info("[TM][B] BAT_TEMP:%d, IBAT:%d, BAT_VOL:%d, SOC:%d\n",
			monitor_dd->last_batt_temp/10,
			monitor_dd->last_batt_current/1000,
			monitor_dd->last_batt_voltage/1000,
			monitor_dd->last_batt_soc);

	list_for_each_entry(cdev, &thermal_cdev_debug_list, node) {
		if (cdev->id <= NUMS_CDEV) {
			ret = cdev->ops->get_cur_state(cdev, &state);

			if (ret) {
				pr_info("failed to read cur_state of cdev[%d]\n",
					cdev->id);
			} else {
				if (state != 0)
					pr_info("[TM][C] %d:%s:%lu\n", cdev->id, cdev->type, state);
			}
		}

		if(cdev->id == 0)
			break;
	}

	get_online_cpus();
	for_each_online_cpu(cpu) {
		if (!CLUSTER_CHECK(cpu))
			continue;
		ret = cpufreq_get_policy(&policy, cpu);
		if (ret)
			continue;
		pr_info("[TM][C] CPU[%d], cur_freq:%u, min_freq:%u, max_freq:%u\n",
			cpu, policy.cur, policy.min, policy.max);
	}
	put_online_cpus();
}

static void poll_monitor_work(struct work_struct *work)
{
	unsigned long delay_time;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct lge_monitor_thermal_data *monitor_dd =
					container_of(delayed_work,
					struct lge_monitor_thermal_data,
					monitor_work_struct);

	if (enable)
		_poll_monitor(monitor_dd);

	delay_time = msecs_to_jiffies(monitor_dd->polling_time);
	/* Check again before scheduling */
	if (enable) {
		queue_delayed_work(monitor_wq,
			&monitor_dd->monitor_work_struct, delay_time);
	}
}

static int lge_monitor_thermal_remove(struct platform_device *pdev)
{
	struct lge_monitor_thermal_data *monitor_dd =
		(struct lge_monitor_thermal_data *)platform_get_drvdata(pdev);

	if (enable)
		cancel_delayed_work_sync(&monitor_dd->monitor_work_struct);

	device_remove_file(monitor_dd->dev, &dev_attr_disable);
	device_remove_file(monitor_dd->dev, &dev_attr_vts_temp);

#ifdef CONFIG_LGE_ONE_BINARY_SKU
	if (sku_carrier == HW_SKU_NA_CDMA_VZW) {
		device_remove_file(monitor_dd->dev, &dev_attr_qtm_c_vts_temp);
		device_remove_file(monitor_dd->dev, &dev_attr_qtm_s_vts_temp);
	}
#endif

	destroy_workqueue(monitor_wq);
	kfree(monitor_dd);
	return 0;
}

static void init_monitor_work(struct work_struct *work)
{
	struct lge_monitor_thermal_data *monitor_dd =
		container_of(to_delayed_work(work),
		struct lge_monitor_thermal_data,
		init_monitor_work_struct);
	unsigned long delay_time;
	int error;

	delay_time = msecs_to_jiffies(monitor_dd->polling_time);

	queue_delayed_work(monitor_wq,
		&monitor_dd->monitor_work_struct, delay_time);

	error = device_create_file(monitor_dd->dev, &dev_attr_disable);
	if (error)
		dev_err(monitor_dd->dev, "couldn't create sysfs attribute\n");

	error = device_create_file(monitor_dd->dev, &dev_attr_vts_temp);
	if (error)
		dev_err(monitor_dd->dev, "couldn't create vts_temp sysfs attribute\n");

#ifdef CONFIG_LGE_ONE_BINARY_SKU
	sku_carrier = lge_get_sku_carrier();

	if (sku_carrier == HW_SKU_NA_CDMA_VZW) {
		error = device_create_file(monitor_dd->dev, &dev_attr_qtm_c_vts_temp);
		if (error)
			dev_err(monitor_dd->dev, "couldn't create qtm_c_vts_temp sysfs attribute\n");

		error = device_create_file(monitor_dd->dev, &dev_attr_qtm_s_vts_temp);
		if (error)
			dev_err(monitor_dd->dev, "couldn't create qtm_s_vts_temp sysfs attribute\n");
	}
#endif

	dev_info(monitor_dd->dev, "LGE monitor thermal Initialized\n");

	return;
}

static struct of_device_id lge_monitor_thermal_match_table[] = {
	{ .compatible = "lge,monitor-thermal" },
	{}
};

static int lge_monitor_thermal_dt_to_pdata(struct platform_device *pdev,
					struct lge_monitor_thermal_data *pdata)
{
	struct device_node *node = (pdev) ? pdev->dev.of_node : NULL;
	struct device *dev_mt = (pdev) ? &pdev->dev : NULL;
	int ret;

	if (dev_mt == NULL) {
		dev_err(dev_mt, "failed to get platform_device data\n");
		return -EIO;
	}

	ret = of_property_read_u32(node,
				"lge,poll-time",
				&pdata->polling_time);
	if (ret) {
		dev_err(dev_mt, "reading poll time failed\n");
		return -ENXIO;
	}

	return 0;
}

static int lge_monitor_thermal_probe(struct platform_device *pdev)
{
	int ret;
	struct lge_monitor_thermal_data *monitor_dd;

	pr_info("thermal monitor probe start\n");

	monitor_wq = alloc_workqueue("monitor-thermal", WQ_FREEZABLE, 0);
	if (!monitor_wq) {
		pr_err("Failed to allocate monitor workqueue\n");
		return -EIO;
	}

	if (!pdev->dev.of_node || !enable)
		return -ENODEV;
	monitor_dd = kzalloc(sizeof(struct lge_monitor_thermal_data),
								GFP_KERNEL);
	if (!monitor_dd)
		return -EIO;

	ret = lge_monitor_thermal_dt_to_pdata(pdev, monitor_dd);
	if (ret)
		goto err;

	monitor_dd->dev = &pdev->dev;
	platform_set_drvdata(pdev, monitor_dd);
	INIT_DELAYED_WORK(&monitor_dd->init_monitor_work_struct,
		init_monitor_work);
	INIT_DELAYED_WORK(&monitor_dd->monitor_work_struct, poll_monitor_work);
	queue_delayed_work(monitor_wq,
		&monitor_dd->init_monitor_work_struct, 0);

	pr_info("thermal monitor probe done\n");

	return 0;
err:
	destroy_workqueue(monitor_wq);
	kzfree(monitor_dd);
	return ret;
}

static const struct dev_pm_ops lge_monitor_thermal_dev_pm_ops = {
	.suspend_noirq = lge_monitor_thermal_suspend,
	.resume_noirq = lge_monitor_thermal_resume,
};

static struct platform_driver lge_monitor_thermal_driver = {
	.probe = lge_monitor_thermal_probe,
	.remove = lge_monitor_thermal_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &lge_monitor_thermal_dev_pm_ops,
		.of_match_table = lge_monitor_thermal_match_table,
	},
};

static int init_lge_monitor_thermal(void)
{
	return platform_driver_register(&lge_monitor_thermal_driver);
}

late_initcall(init_lge_monitor_thermal);
MODULE_DESCRIPTION("LGE monitor thermal driver");
MODULE_LICENSE("GPL v2");
