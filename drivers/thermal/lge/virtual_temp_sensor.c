#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/thermal.h>
#include <linux/string.h>
#include <linux/errno.h>
#ifdef CONFIG_LGE_ONE_BINARY_SKU
#include <soc/qcom/lge/board_lge.h>
#endif

#define MAX_VIRTUAL_SENSORS    5
#define MAX_AGGREGATE_SENSORS 10

typedef struct virtual_temp_sensor {
	int                           num_virt_sensor;
	struct thermal_zone_device    *virt_tzd[MAX_VIRTUAL_SENSORS];
} VTS;

struct virt_sensor_data {
	int                           num_sensor;
	int                           coefficients[MAX_AGGREGATE_SENSORS];
	int                           coeffi_sign[MAX_AGGREGATE_SENSORS];
	int                           avg_denominator;
	int                           avg_offset;
	struct thermal_zone_device    *tzd[MAX_AGGREGATE_SENSORS];
};

static int vts_tz_get_temp(struct thermal_zone_device *thermal,
			    int *temp)
{
	struct virt_sensor_data *sens = thermal->devdata;
	int idx, agg_temp = 0, ret = 0;

	for (idx = 0; idx < sens->num_sensor; idx++) {
		int sens_temp = 0;
		ret = thermal_zone_get_temp(sens->tzd[idx], &sens_temp);
		if (ret) {
			pr_err("failed to read out thermal zone senror %s, error : %d\n",
					thermal->type, ret);
			return ret;
		}
		pr_debug("aggregate sensor name : %s, temperature : %d\n",
				sens->tzd[idx]->type, sens_temp);
		if (sens->coeffi_sign[idx]){
			agg_temp = agg_temp - sens_temp * sens->coefficients[idx];
		} else {
			agg_temp = agg_temp + sens_temp * sens->coefficients[idx];
		}

	}
	agg_temp = agg_temp / sens->avg_denominator;

	*temp = agg_temp + sens->avg_offset;
	pr_debug("virtual sensor name : %s, temperature : %d\n",
			 thermal->type, *temp);

	return 0;
}

static struct thermal_zone_device_ops vts_tzd_ops = {
	.get_temp = vts_tz_get_temp,
};

static int vts_probe(struct platform_device *pdev)
{
	struct device_node *np, *child;
	VTS *vts = kzalloc(sizeof(VTS), GFP_KERNEL);
	int ret = 0;
	int index = 0;
	u32 prop;
#ifdef CONFIG_LGE_ONE_BINARY_SKU
	enum lge_sku_carrier_type sku_carrier = HW_SKU_MAX;
	sku_carrier = lge_get_sku_carrier();
#endif

	if (!vts) {
		pr_err("Fail to get *vts.\n");
		return -ENOMEM;
	}

	np = of_find_node_by_name(NULL, "lge-vts");
	if (!np) {
		pr_err("unable to find lge-vts\n");
		kfree(vts);
		return 0;
	}

	for_each_available_child_of_node(np, child) {
		int idx, count = 0;
		struct thermal_zone_device *zone;
		struct virt_sensor_data *sens =
			   kzalloc(sizeof(struct virt_sensor_data), GFP_KERNEL);
		const char *sensor_names[MAX_AGGREGATE_SENSORS];

#ifdef CONFIG_LGE_ONE_BINARY_SKU
		if (!strncmp(child->name, "qtm-", strlen("qtm-"))) {
			if (sku_carrier != HW_SKU_NA_CDMA_VZW) {
				pr_info("skip %s virtual sensor register\n", child->name);
				continue;
			}
		}
#endif
		pr_info("virtual sensor name : %s\n", child->name);

		count = of_property_count_strings(child, "sensor_names");
		if (count < 0 || count >= MAX_AGGREGATE_SENSORS) {
			pr_err("missing or too many aggregate sensors\n");
			kfree(sens);
			continue;
		}

		sens->num_sensor = count;
		pr_info("number of aggregate sensor : %d\n", sens->num_sensor);

		count = of_property_read_string_array(child, "sensor_names", sensor_names, count);
		if (count < 0) {
			pr_err("couldn't read aggreagte sensor name\n");
			kfree(sens);
			continue;
		}

		for (idx = 0; idx < sens->num_sensor; idx++) {
			pr_info("sensor name : %s\n", sensor_names[idx]);
			sens->tzd[idx] = thermal_zone_get_zone_by_name(sensor_names[idx]);

			if (IS_ERR(sens->tzd[idx])) {
				break;
			};
		}

		if (idx != sens->num_sensor) {
			pr_err("sens:%s, couldn't get thermal zone by name\n", child->name);
			kfree(sens);
			continue;
		}

		ret = of_property_read_u32_array(child, "coeffi_sign", sens->coeffi_sign, sens->num_sensor);
		if (ret < 0) {
			pr_err("missing coefficients property\n");
			kfree(sens);
			continue;
		}
		ret = of_property_read_u32_array(child, "coefficients", sens->coefficients, sens->num_sensor);
		if (ret < 0) {
			pr_err("missing coefficients property\n");
			kfree(sens);
			continue;
		}

		ret = of_property_read_u32(child, "avg_denominator", &prop);
		if (ret < 0) {
			pr_err("missing avg_denominator property\n", np);
			kfree(sens);
			continue;
		}
		sens->avg_denominator = prop;

		ret = of_property_read_u32(child, "avg_offset", &prop);
		if (ret < 0) {
			pr_err("missing avg_offset property\n", np);
			kfree(sens);
			continue;
		}
		sens->avg_offset = prop;

		zone = thermal_zone_device_register(
			   child->name, 0, 0, sens,
			   &vts_tzd_ops, NULL, 0, 0
			   );

		if (IS_ERR(zone)) {
			ret = PTR_ERR(zone);
			pr_err("fail to register %s virtual sensor\n",
					child->name);
			kfree(sens);
			goto err;
		}

		vts->virt_tzd[index] = zone;
		vts->num_virt_sensor = ++index;

		if (vts->num_virt_sensor == MAX_VIRTUAL_SENSORS) {
			pr_info("Too many registered virtual sensors\n");
			break;
		}
	}

	pr_info("%d virtual sensors registered\n", vts->num_virt_sensor);
	platform_set_drvdata(pdev, vts);

	return 0;

err:
	while (--index >= 0)
		thermal_zone_device_unregister(vts->virt_tzd[index]);

	pr_err("failed virtual sensor register\n");
	platform_set_drvdata(pdev, NULL);
	kfree(vts);

	return ret;
}

static int vts_remove(struct platform_device *pdev)
{
	VTS *vts = platform_get_drvdata(pdev);
	int index;

	for (index = 0; index < vts->num_virt_sensor; index++)
		thermal_zone_device_unregister(vts->virt_tzd[index]);

	platform_set_drvdata(pdev, NULL);
	kfree(vts);

	return 0;
}

static const struct of_device_id vts_match[] = {
	{ .compatible = "lge,vts", },
	{}
};

static struct platform_driver vts_driver = {
	.probe = vts_probe,
	.remove = vts_remove,
	.driver = {
		.name = "vts",
		.owner = THIS_MODULE,
		.of_match_table = vts_match,
	},
};

static int __init vts_init_driver(void)
{
	return platform_driver_register(&vts_driver);
}
late_initcall(vts_init_driver);

static void __exit vts_exit_driver(void)
{
	return platform_driver_unregister(&vts_driver);
}
module_exit(vts_exit_driver);

MODULE_AUTHOR("LGE");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtual Temp Sensor driver");

