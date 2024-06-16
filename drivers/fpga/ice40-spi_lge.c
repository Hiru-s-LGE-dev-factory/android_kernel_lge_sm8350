/*
 * FPGA Manager Driver for Lattice iCE40.
 *
 *  Copyright (c) 2016 Joel Holdsworth
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This driver adds support to the FPGA manager for configuring the SRAM of
 * Lattice iCE40 FPGAs through slave SPI.
 */

#include <linux/fpga/fpga-mgr.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/spi/spi.h>
#include <linux/stringify.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/lge/board_lge.h>
#include <linux/ice40/ice40-core.h>
#include <linux/fpga/ice40-spi.h>
#include <linux/interrupt.h>

#define TAG "[ice40] "
#define LOGI(fmt, args...) pr_info(TAG fmt, ##args)
#define LOGE(fmt, args...) pr_err(TAG "[%s %d] " fmt, __func__, __LINE__, ##args)

#define LATTICE_OFFSET		2
#define BUILDDATE_LENGTH	23

#define RESET_GPIO 74
#define LRESET_GPIO 75

#define TEST_PWM_FW_PATH "motion_bitmap_i2c_by_AP_test.img" //push it to /etc/firmware with same name.

#define ICE40_SPI_MAX_SPEED 25000000 /* Hz */
#define ICE40_SPI_MIN_SPEED 1000000 /* Hz */

#define ICE40_SPI_RESET_DELAY 1 /* us (>200ns) */
#define ICE40_SPI_HOUSEKEEPING_DELAY 1200 /* us */

#define ICE40_SPI_NUM_ACTIVATION_BYTES DIV_ROUND_UP(100, 8)

enum {
	POWER_OFF = 0,
	POWER_ON,
};

struct ice40_fpga_priv {
	struct spi_device *dev;
	struct gpio_desc *reset;

	struct gpio_desc *cdone;
	unsigned int cdone_irq;
	bool cdone_status;
	int cdone_gpio;
	bool cdone_use;

	struct gpio_desc *lreset;
	char builddate[BUILDDATE_LENGTH+1];
	struct regulator *vdda33;
	struct regulator *vdda12;
	struct regulator *vdda18;
	const char *fw;
};

static struct ice40_fpga_priv *g_ice40_fpga_priv = NULL;
extern struct ice40 *global_ice40;

static void ice40_power(struct device *dev, int ctrl)
{
	struct fpga_manager *mgr = dev_get_drvdata(dev);
	struct ice40_fpga_priv *priv = mgr->priv;
	int ret;

	switch(ctrl)
	{
		case POWER_OFF:
			if( priv->vdda12 ) {
				ret = regulator_disable(priv->vdda12);
				LOGE("%s : disable vdda12 supply\n", __func__);
				if(ret < 0)
					LOGE("%s : Failed to disable vdda12 supply %d\n", __func__, ret);
			}

			if( priv->vdda18 ) {
				ret = regulator_disable(priv->vdda18);
				LOGE("%s : disable vdda18 supply\n", __func__);
				if(ret < 0)
					LOGE("%s : Failed to disable vdda18 supply %d\n", __func__, ret);
			}

			if( priv->vdda33 ) {
				ret = regulator_disable(priv->vdda33);
				LOGE("%s : disable vdda33 supply \n", __func__);
				if(ret < 0)
					LOGE("%s : Failed to disable vdd33 supply %d\n", __func__, ret);
			}
			break;
		case POWER_ON:
			if( priv->vdda12 ) {
				ret = regulator_enable(priv->vdda12);
				LOGE("%s : enable vdda12 supply\n", __func__);
				if(ret < 0)
					LOGE("%s : Failed to enable vdda12 supply %d\n", __func__, ret);
			}

			if( priv->vdda18 ) {
				ret = regulator_enable(priv->vdda18);
				LOGE("%s : enable vdda18 supply\n", __func__);
				if(ret < 0)
					LOGE("%s : Failed to enable vdda18 supply %d\n", __func__, ret);
			}

			if( priv->vdda33 ) {
				ret = regulator_enable(priv->vdda33);
				LOGE("%s : enable vdda33 supply\n", __func__);
				if(ret < 0)
					LOGE("%s : Failed to enable vdda33 supply %d\n", __func__, ret);
			}
			break;
	}
}

static int ice40_write_firmware(struct device *dev, const char *img)
{
	int ret = 0;
	struct fpga_manager *mgr;
	struct fpga_image_info info;
	info.flags = 0;

	mgr = fpga_mgr_get(dev);

	ret = fpga_mgr_firmware_load(mgr, &info, img);
	if (ret)
		LOGE("Failed to load fpga image: %d\n", ret);
	fpga_mgr_put(mgr);

	LOGI("%s : fpga image write complete: %d (complete %d)\n", __func__, mgr->state, FPGA_MGR_STATE_WRITE_COMPLETE);

	return ret;
}

void ice40_set_lreset(int value)
{
    if(g_ice40_fpga_priv)
        gpiod_set_value(g_ice40_fpga_priv->lreset, value);
    else
        LOGE("g_ice40_fpga_priv is null\n");
}
EXPORT_SYMBOL(ice40_set_lreset);

int ice40_get_lreset(void)
{
    int ret = 0;

    if(g_ice40_fpga_priv)
        ret = gpiod_get_value(g_ice40_fpga_priv->lreset);
    else
        LOGE("g_ice40_fpga_priv is null\n");

    return ret;
}
EXPORT_SYMBOL(ice40_get_lreset);

int ice40_reset(void)
{
	int ret = 0;

	ice40_power(&g_ice40_fpga_priv->dev->dev, POWER_OFF);
	ice40_power(&g_ice40_fpga_priv->dev->dev, POWER_ON);
	ret = ice40_write_firmware(&g_ice40_fpga_priv->dev->dev, g_ice40_fpga_priv->fw);
	if (ret)
		LOGE("Failed to ice40_write_firmware: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(ice40_reset);

bool ice40_get_cdone(void)
{
    int ret = 0;

	if(g_ice40_fpga_priv)
		ret = gpiod_get_value(g_ice40_fpga_priv->cdone);
	else
		LOGE("g_ice40_fpga_priv is null\n");

    return ret;
}
EXPORT_SYMBOL(ice40_get_cdone);

static ssize_t show_fw(struct device *dev, struct device_attribute *attr,
		              char *buf)
{
	int ret = 0;
	struct fpga_manager *mgr = dev_get_drvdata(dev);
	struct ice40_fpga_priv *priv = mgr->priv;

	ret += snprintf(buf + ret, PAGE_SIZE, "%s\n", priv->builddate);

	return ret;
}

static ssize_t store_fw(struct device *dev, struct device_attribute *attr,
		             const char *buf, size_t count)
{
	struct fpga_manager *mgr = dev_get_drvdata(dev);
	struct ice40_fpga_priv *priv = mgr->priv;

	ice40_power(dev, POWER_OFF);
	ice40_power(dev, POWER_ON);
	ice40_write_firmware(dev, priv->fw);

	return count;
}

static ssize_t store_fwadb(struct device *dev, struct device_attribute *attr,
		             const char *buf, size_t count)
{
	ice40_write_firmware(dev, TEST_PWM_FW_PATH);

	return count;
}

static ssize_t show_gpio(struct device *dev, struct device_attribute *attr,
		              char *buf)
{
	int ret = 0;
	struct fpga_manager *mgr = dev_get_drvdata(dev);
	struct ice40_fpga_priv *priv = mgr->priv;

	ret += snprintf(buf + ret, PAGE_SIZE, "RESET = %d\n", gpiod_get_value(priv->reset));
	ret += snprintf(buf + ret, PAGE_SIZE, "LRESET = %d\n", gpiod_get_value(priv->lreset));
	if( priv->vdda33 )
		ret += snprintf(buf + ret, PAGE_SIZE, "vdd33 = %d\n", regulator_is_enabled(priv->vdda33));
	if( priv->vdda12 )
		ret += snprintf(buf + ret, PAGE_SIZE, "vdda12 = %d\n", regulator_is_enabled(priv->vdda12));
	if( priv->vdda18 )
		ret += snprintf(buf + ret, PAGE_SIZE, "vdda18 = %d\n", regulator_is_enabled(priv->vdda18));

	if (!IS_ERR(priv->cdone))
		ret += snprintf(buf + ret, PAGE_SIZE, "CDONE = %d\n", gpiod_get_value(priv->cdone));

	return ret;
}

static ssize_t store_gpio(struct device *dev, struct device_attribute *attr,
		             const char *buf, size_t count)
{
	struct fpga_manager *mgr = dev_get_drvdata(dev);
	struct ice40_fpga_priv *priv = mgr->priv;
	int gpio = 0;
	int value = 0;
	int ret = 0;

	ret = sscanf(buf, "%d %d", &gpio, &value);
	if(ret != 2)
		return -EINVAL;

	if ((value > 1) || (value < 0)) {
        LOGE("invalid value(%d)\n", value);
		return count;
	}

	switch(gpio)
	{
		case RESET_GPIO:
			gpiod_set_value(priv->reset, value);
			value = gpiod_get_value(priv->reset);
			LOGI("%s: RESET = %d\n", __func__, value);
			break;
		case LRESET_GPIO:
			gpiod_set_value(priv->lreset, value);
			value = gpiod_get_value(priv->lreset);
			LOGI("%s: LRESET = %d\n", __func__, value);
			break;
	}

	return count;
}

static DEVICE_ATTR(fw, S_IRUGO | S_IWUSR, show_fw, store_fw);
static DEVICE_ATTR(fwadb, S_IRUGO | S_IWUSR, NULL, store_fwadb);
static DEVICE_ATTR(gpio, S_IRUGO | S_IWUSR, show_gpio, store_gpio);

static struct attribute *dev_attrs[] = {
	&dev_attr_fw.attr,
	&dev_attr_fwadb.attr,
	&dev_attr_gpio.attr,
	NULL,
};

static struct attribute_group dev_attr_grp = {
	.attrs = dev_attrs,
};

static enum fpga_mgr_states ice40_fpga_ops_state(struct fpga_manager *mgr)
{
	struct ice40_fpga_priv *priv = mgr->priv;

	if (!IS_ERR(priv->cdone)) {
		return gpiod_get_value(priv->cdone) ? FPGA_MGR_STATE_OPERATING :
			FPGA_MGR_STATE_UNKNOWN;
	} else {
		return FPGA_MGR_STATE_OPERATING;
	}		
}

irqreturn_t cdone_irq_handler(int irq, void *__g_ice40_fpga_priv)
{
	struct irq_data *irq_data = irq_get_irq_data(irq);

	u32 flag = irqd_get_trigger_type(irq_data);
	bool high = gpiod_get_value(g_ice40_fpga_priv->cdone);

	pr_info("%s : vdda12 = %d, vdda18 = %d, vdda33 = %d\n", __func__, regulator_is_enabled(g_ice40_fpga_priv->vdda12), regulator_is_enabled(g_ice40_fpga_priv->vdda18), regulator_is_enabled(g_ice40_fpga_priv->vdda33));

	if (flag == IRQ_TYPE_EDGE_BOTH) {
		//        IRQ_TYPE =         EDGE_RISING : EDGE_FALLING
		g_ice40_fpga_priv->cdone_status = high ? true : false;
	} else {
		g_ice40_fpga_priv->cdone_status = false;
	}

	pr_info("%s : irq(%d), flag(%d). CDONE(%d)\n", __func__, irq, flag, g_ice40_fpga_priv->cdone_status);

	return IRQ_HANDLED;
}

static int ice40_fpga_ops_write_init(struct fpga_manager *mgr,
				     struct fpga_image_info *info,
				     const char *buf, size_t count)
{
	struct ice40_fpga_priv *priv = mgr->priv;
	struct spi_device *dev = priv->dev;
	struct spi_message *message;
	struct spi_transfer assert_cs_then_reset_delay = {
		.cs_change   = 1,
		.delay_usecs = ICE40_SPI_RESET_DELAY
	};
	struct spi_transfer housekeeping_delay_then_release_cs = {
		.delay_usecs = ICE40_SPI_HOUSEKEEPING_DELAY
	};
	int ret;

	LOGI("%s\n", __func__);
	if ((info->flags & FPGA_MGR_PARTIAL_RECONFIG)) {
		LOGE("Partial reconfiguration is not supported\n");
		return -ENOTSUPP;
	}

	message = kmalloc(sizeof(struct spi_message), GFP_KERNEL);
	if(!message) {
		LOGE("Failed to allocate message\n");
		return -ENOMEM;
	}

	gpiod_set_value(priv->lreset, 0);
	/* Lock the bus, assert CRESET_B and SS_B and delay >200ns */
	spi_bus_lock(dev->master);

	gpiod_set_value(priv->reset, 0);
	LOGE("%s : CReset 0\n", __func__);

	spi_message_init(message);
	spi_message_add_tail(&assert_cs_then_reset_delay, message);
	ret = spi_sync_locked(dev, message);

	/* Come out of reset */
	gpiod_set_value(priv->reset, 1);
	LOGE("%s : CReset 1\n", __func__);

	/* Abort if the chip-select failed */
	if (ret) {
		LOGE("Abort if the chip-select failed: %d\n", ret);
		goto fail;
	}

	/* Check CDONE is de-asserted i.e. the FPGA is reset */
	if (!IS_ERR(priv->cdone)) {
		if (gpiod_get_value(priv->cdone)) {
			dev_err(&dev->dev, "Device reset failed, CDONE is asserted\n");
			ret = -EIO;
			goto fail;
		}
	}

	/* Wait for the housekeeping to complete, and release SS_B */
	spi_message_init(message);
	spi_message_add_tail(&housekeeping_delay_then_release_cs, message);
	ret = spi_sync_locked(dev, message);

fail:
	spi_bus_unlock(dev->master);
	kfree(message);

	return ret;
}

//strstr seems to stop searching if needle meets a null character.
static char* my_strstr(const char *buf, const char *search)
{
	char c, sc;
	int len, cnt = 0;

	c = *search;
	len = strlen(search);
	do {
		do {
			sc = *buf++;
			cnt++;
		} while (sc != c && cnt < 0x100);
	} while (strncmp((buf-1), search, len) != 0 && cnt < 0x100);

	if(cnt >= 0x100) {
		LOGI("Fail to search %s\n", search);
		return NULL;
	}
	else
		return (char *)--buf;
}

static int ice40_fpga_ops_write(struct fpga_manager *mgr,
				const char *buf, size_t count)
{
	struct ice40_fpga_priv *priv = mgr->priv;
	char *p = my_strstr(buf, "Date");

	if(p)
		strncpy(priv->builddate, p, BUILDDATE_LENGTH);

	LOGI("%s %s %zu %s\n",(buf + LATTICE_OFFSET), priv->builddate, count, __func__);

	return spi_write(priv->dev, buf, count);
}

static int ice40_fpga_ops_write_complete(struct fpga_manager *mgr,
					 struct fpga_image_info *info)
{
	int ret;
	struct ice40_fpga_priv *priv = mgr->priv;
	struct spi_device *dev = priv->dev;
	u8 *padding;

	LOGI("%s\n", __func__);
	/* Check CDONE is asserted */

	if (!IS_ERR(priv->cdone)) {
		if (!gpiod_get_value(priv->cdone)) {
			dev_err(&dev->dev,
				"CDONE was not asserted after firmware transfer\n");
			return -EIO;
		}
	}

	padding = kzalloc(sizeof(u8)*ICE40_SPI_NUM_ACTIVATION_BYTES, GFP_KERNEL);
	if(!padding) {
		LOGE("Failed to allocate padding\n");
		return -ENOMEM;
	}

	/* Send of zero-padding to activate the firmware */
	ret = spi_write(dev, padding, sizeof(padding));

	if (!IS_ERR(priv->cdone)) {
		if (gpiod_get_value(priv->cdone))
			LOGI("CDONE High\n");
		else
			LOGI("CDONE Low\n");
	}

	gpiod_set_value(priv->lreset, 1);
	mdelay(1);
	gpiod_set_value(priv->lreset, 0);

	kfree(padding);

	return ret;
}

static const struct fpga_manager_ops ice40_fpga_ops = {
	.state = ice40_fpga_ops_state,
	.write_init = ice40_fpga_ops_write_init,
	.write = ice40_fpga_ops_write,
	.write_complete = ice40_fpga_ops_write_complete,
};

static int ice40_fpga_probe(struct spi_device *spi)
{
	struct device *dev = &spi->dev;
	struct ice40_fpga_priv *priv;
	int ret = 0;
	struct fpga_manager *mgr;
	struct device_node *np = dev->of_node;

	LOGI("%s : enter \n", __func__);
	spi_setup(spi);
	priv = devm_kzalloc(&spi->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		LOGE("priv devm_kzalloc failed\n");
		return -ENOMEM;
	}
	g_ice40_fpga_priv = priv;
	priv->dev = spi;

	/* Check board setup data. */
	if (spi->max_speed_hz > ICE40_SPI_MAX_SPEED) {
		LOGE("SPI speed is too high, maximum speed is "
			__stringify(ICE40_SPI_MAX_SPEED) "\n");
		goto board_setup_fail;
	}

	if (spi->max_speed_hz < ICE40_SPI_MIN_SPEED) {
		LOGE("SPI speed is too low, minimum speed is "
			__stringify(ICE40_SPI_MIN_SPEED) "\n");
		goto board_setup_fail;
	}

	if (spi->mode & SPI_CPHA) {
		LOGE("Bad SPI mode, CPHA not supported\n");
		goto board_setup_fail;
	}

	/* Set up the GPIOs */
	priv->cdone = devm_gpiod_get(dev, "cdone", GPIOD_IN);
	if (IS_ERR(priv->cdone)) { // HW Rev.A
		ret = PTR_ERR(priv->cdone);
		dev_err(dev, "Did not use CDONE GPIO: %d\n", ret);
	} else { // HW Rev.B
		priv->cdone_irq = gpiod_to_irq(priv->cdone);

		ret = devm_request_threaded_irq(dev, priv->cdone_irq, NULL, cdone_irq_handler,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,  "cdone_irq", priv);
		if (ret < 0) {
			LOGE("Status IRQ request thread failed, ret=%d\n", ret);
			goto pin_set_fail;
		}
	}

	priv->reset = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset)) {
		ret = PTR_ERR(priv->reset);
		LOGE("Failed to get CRESET_B GPIO: %d\n", ret);
		goto pin_set_fail;
	}

	priv->lreset = devm_gpiod_get(dev, "lreset", GPIOD_OUT_LOW);
	if (IS_ERR(priv->lreset)) {
		ret = PTR_ERR(priv->lreset);
		LOGE("Failed to get LATTICE_RESET GPIO: %d\n", ret);
		goto pin_set_fail;
	}

	// Though vdda33-supply is not exist, dev_regulator_get returns dummy regulator.
	// priv->vdda33 is not NULL, check vdda33-supply property.
	// kernel message : spi1.2 supply vdda33 not found, using dummy regulator
	if(of_get_property(np, "vdda33-supply", NULL)) { 
		priv->vdda33 = devm_regulator_get(dev, "vdda33");
		if(IS_ERR(priv->vdda33)) {
			ret = PTR_ERR(priv->vdda33);
			LOGI("Failed to get vdda33 supply\n", ret);
		}
	}

	if(of_get_property(np, "vdda12-supply", NULL)) { 
		priv->vdda12 = devm_regulator_get(dev, "vdda12");
		if(IS_ERR(priv->vdda12)) {
			ret = PTR_ERR(priv->vdda12);
			LOGI("Failed to get vdda12 supply\n", ret);
		}
	}
	
	if(of_get_property(np, "vdda18-supply", NULL)) { 
		priv->vdda18 = devm_regulator_get(dev, "vdda18");
		if(IS_ERR(priv->vdda18)) {
			ret = PTR_ERR(priv->vdda18);
			LOGI("Failed to get vdda18 supply\n", ret);
		}
	}

	ret = of_property_read_string(np, "fw", &priv->fw);
	if(ret < 0) {
		LOGE("Failed to get firmware image path: %d\n", ret);
		goto pin_set_fail;
	}
	LOGI("ice40 fw %s\n", priv->fw);

	/* Register with the FPGA manager */
	mgr = fpga_mgr_create(dev, "Lattice iCE40 FPGA Manager", &ice40_fpga_ops, priv);
	if (!mgr)
		goto fpga_mrg_fail;

	spi_set_drvdata(spi, mgr);

	ret = fpga_mgr_register(mgr);
	if (ret) {
		goto fpga_mrg_fail;
	}

	ret = sysfs_create_group(&spi->dev.kobj, &dev_attr_grp);
	if (ret) {
		 LOGE("failed to create dev. attrs : %d\n", ret);
		 goto sysfs_fail;
	}

	ice40_power(dev, POWER_ON);

	ret = ice40_write_firmware(dev, priv->fw);
	if (ret) {
		 LOGE("failed to write firmware. : %d\n", ret);
		 goto probe_fail;
	}

	LOGI("%s : leave \n", __func__);

	return ret;

probe_fail:
	ice40_power(&spi->dev, POWER_OFF);
sysfs_fail:
	sysfs_remove_group(&spi->dev.kobj, &dev_attr_grp);
fpga_mrg_fail:
	fpga_mgr_unregister(mgr);
pin_set_fail:
	devm_free_irq(&spi->dev, priv->cdone_irq, priv);
board_setup_fail:
	g_ice40_fpga_priv = NULL;

	return ret;
}

static int ice40_fpga_remove(struct spi_device *spi)
{
	struct fpga_manager *mgr = spi_get_drvdata(spi);

	ice40_power(&spi->dev, POWER_OFF);
	sysfs_remove_group(&spi->dev.kobj, &dev_attr_grp);
	fpga_mgr_unregister(mgr);
	devm_free_irq(&spi->dev, g_ice40_fpga_priv->cdone_irq, g_ice40_fpga_priv);

	g_ice40_fpga_priv = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int ice40_spi_runtime_suspend(struct device *dev)
{
	//struct fpga_manager *mgr = dev_get_drvdata(dev);
	//struct ice40_fpga_priv *priv = mgr->priv;

	//gpiod_set_value(priv->lreset, 0);

	return 0;
}

static int ice40_spi_runtime_resume(struct device *dev)
{
	//struct fpga_manager *mgr = dev_get_drvdata(dev);
	//struct ice40_fpga_priv *priv = mgr->priv;

	//gpiod_set_value(priv->lreset, 1);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ice40_spi_suspend(struct device *dev)
{
	//struct fpga_manager *mgr = dev_get_drvdata(dev);
	//struct ice40_fpga_priv *priv = mgr->priv;

	//gpiod_set_value(priv->lreset, 0);

	return 0;
}

static int ice40_spi_resume(struct device *dev)
{

	//struct fpga_manager *mgr = dev_get_drvdata(dev);
	//struct ice40_fpga_priv *priv = mgr->priv;

	//gpiod_set_value(priv->lreset, 1);

	return 0;
}
#endif


static const struct dev_pm_ops ice40_spi_pm_ops = {
    SET_SYSTEM_SLEEP_PM_OPS(ice40_spi_suspend, ice40_spi_resume)
    SET_RUNTIME_PM_OPS(ice40_spi_runtime_suspend, ice40_spi_runtime_resume, NULL)
};
#define ICE40_SPI_PM_OPS    (&ice40_spi_pm_ops)
#else
#define ICE40_SPI_PM_OPS    NULL
#endif

static const struct of_device_id ice40_fpga_of_match[] = {
	{ .compatible = "lattice,ice40-fpga-mgr", },
	{},
};
MODULE_DEVICE_TABLE(of, ice40_fpga_of_match);

static struct spi_driver ice40_fpga_driver = {
	.probe = ice40_fpga_probe,
	.remove = ice40_fpga_remove,
	.driver = {
		.name = "ice40spi",
		.pm = ICE40_SPI_PM_OPS,
		.of_match_table = of_match_ptr(ice40_fpga_of_match),
	},
};
module_spi_driver(ice40_fpga_driver);

MODULE_AUTHOR("Joel Holdsworth <joel@airwebreathe.org.uk>");
MODULE_DESCRIPTION("Lattice iCE40 FPGA Manager");
MODULE_LICENSE("GPL v2");
