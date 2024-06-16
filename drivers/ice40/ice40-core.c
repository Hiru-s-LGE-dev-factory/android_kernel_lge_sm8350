/*
 * Base Driver for ice40 Accessory Communication Interface
 *
 * Copyright (c) 2018 LG Electronics, Inc
 *
 * All rights are reserved.
 *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING
 * THE SOFTWARE.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *      PROJECT:   ice40 driver
 *      LANGUAGE:  ANSI C
 */

/* ========================================================================== */
/* Includes and Defines */
#ifndef DEBUG
#define DEBUG
#endif

#include <linux/device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/ice40/ice40-core.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <soc/qcom/lge/board_lge.h>

struct ice40 *global_ice40;

static int ice40_initialize(struct ice40 *ice40)
{
	int ret = 0;

	printk("ice40_initialize_regmap is called\n");
	if (ice40->regmap == NULL) {
		ice40->regmap = devm_regmap_init_i2c(ice40->client_master, &ice40_regmap_config);
		if (IS_ERR(ice40->regmap)) {
			ret = PTR_ERR(ice40->regmap);
			dev_err(ice40->dev, "%s: regmap initialization failed: %d\n", __func__, ret);

			return ret;
		}
	} else {
		printk("ice40_initialize_regmap not null\n");
	}

    printk("ice40_initialize is OK!\n");

	return ret;
}

static int ice40_deinitialize(struct ice40 *ice40)
{
	regmap_exit(ice40->regmap);

	return 0;
}

/*!
 *****************************************************************************
 *  \brief Called after suspend event
 *
 *  \return xx: returns 0 on success
 *
 *****************************************************************************
 */
static int ice40_suspend(struct device *dev)
{
    return 0;
}

/*!
 *****************************************************************************
 *  \brief Called after resume event
 *
 *  \return xx: returns 0 on success
 *
 *****************************************************************************
 */
static int ice40_resume(struct device *dev)
{
    return 0;
}

/*!
 *****************************************************************************
 *  \brief Probe function
 *
 *  Allocates memory for ice40 device, configures GPIO pins,
 *  configures IRQ routine, configures input events for buttons
 *
 *  \return xx: returns 0 on success
 *
 *****************************************************************************
 */
static int ice40_i2c_probe(struct i2c_client *client,
    const struct i2c_device_id *id)
{
    int ret = 0;
    struct ice40 *ice40;

    dev_info(&client->dev, "%s: i2c slave address 0x%x\n", __func__, client->addr);

    /* info: memory allocated with devm_kzalloc is automatically freed
     * upon driver detach, difference between kzalloc() and devm_kzalloc()
     */
    ice40 = devm_kzalloc(&client->dev, sizeof(struct ice40), GFP_KERNEL);
    if (!ice40)
        return -ENOMEM;
    global_ice40 = ice40;

    ice40->client_master = client;
    ice40->dev = &client->dev;
    i2c_set_clientdata(client, ice40);

	ret = ice40_initialize(ice40);
	if (ret < 0) {
        dev_info(ice40->dev, "%s: ice40_initialize fail\n", __func__);

        goto dev_attr_failed;
	}

	ice40->ice40_on = true;

    mutex_init(&ice40->ice40_i2c_lock);

    return 0;

dev_attr_failed:
	ice40_deinitialize(ice40);
    return ret;
}
EXPORT_SYMBOL(global_ice40);

/*!
 *****************************************************************************
 *  \brief Driver remove function
 *
 *  Removes sysfs files, frees interrupt, deallocates memory
 *
 *  \return xx: returns 0
 *
 *****************************************************************************
 */
static int ice40_i2c_remove(struct i2c_client *client)
{
    struct ice40 *ice40 = i2c_get_clientdata(client);

    dev_dbg(&client->dev, "%s()\n", __func__);

    if (ice40) {
        i2c_set_clientdata(client, NULL);
        devm_kfree(&client->dev, ice40);
    }

    ice40->ice40_on = false;

    ice40_deinitialize(ice40);

    return 0;
}

/* ========================================================================== */
/* Driver Matching */

#ifdef CONFIG_OF
/* find a match from full device tree entries (including vendor part) */
static const struct of_device_id ice40_of_match[] = {
    {.compatible = "lattice,ice40-i2c",},
    {}
};
MODULE_DEVICE_TABLE(of, ice40_of_match);
#endif

/* find a match in device tree entries */
static const struct i2c_device_id ice40_i2c_id[] = {
    {.name = "ice40_i2c", .driver_data = 0},
    {}
};
MODULE_DEVICE_TABLE(i2c, ice40_i2c_id);

/* ========================================================================== */
/* Driver Definition */

static const struct dev_pm_ops ice40_pm_ops = {
    .suspend = ice40_suspend,
    .resume  = ice40_resume,
};

static struct i2c_driver ice40_i2c_driver = {
    .probe = ice40_i2c_probe,
    .remove = ice40_i2c_remove,
    .id_table = ice40_i2c_id,
    .driver = {
       .name = "ice40_i2c",
       .owner = THIS_MODULE,
       .of_match_table = ice40_of_match,
       .pm = &ice40_pm_ops,
    },
};
module_i2c_driver(ice40_i2c_driver);

MODULE_AUTHOR("LG Electronics, Inc");
MODULE_DESCRIPTION("ICE40 core driver");
MODULE_LICENSE("GPL v2");
