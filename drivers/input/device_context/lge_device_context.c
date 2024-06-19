/*
 * LGE DEVICE CONTEXT MONITOR DRIVER
 *
 *
 * Copyright (c) 2020 LGE
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License Version 2
 * as published by the Free Software Foundation.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#define CONFIG_LGE_FB_NOTIFIER
#define CONFIG_LGE_CUSTOM_FB_NOTIFIER // for AOD

#ifdef CONFIG_LGE_FB_NOTIFIER
#include <linux/fb.h>
#include <linux/notifier.h>
#ifdef CONFIG_LGE_CUSTOM_FB_NOTIFIER
#include <linux/lge_panel_notify.h>
#endif
#endif // CONFIG_LGE_FB_NOTIFIER

static struct class *device_context_class;
static struct device *device_context_sysfs_dev;

struct device_context_data {
	struct device *dev;

	/* struct wake_lock ttw_wl; */
	struct mutex lock; /* To set/get exported values in sysfs */
	bool prepared;
	int context;
	atomic_t wakeup_enabled; /* Used both in ISR and non-ISR */

#ifdef CONFIG_LGE_FB_NOTIFIER
	struct notifier_block fb_notifier;
#endif // CONFIG_LGE_FB_NOTIFIER
};

#ifdef CONFIG_LGE_FB_NOTIFIER
#ifdef CONFIG_LGE_CUSTOM_FB_NOTIFIER
static int (*fb_notifier_register)(struct notifier_block *nb) = lge_panel_notifier_register_client;
static int (*fb_notifier_unregister)(struct notifier_block *nb) = lge_panel_notifier_unregister_client;
#else
static int (*fb_notifier_register)(struct notifier_block *nb) = fb_register_client;
static int (*fb_notifier_unregister)(struct notifier_block *nb) = fb_unregister_client;
#endif

static int fb_notifier_callback(struct notifier_block *nb, unsigned long action, void *data)
{
//	struct device_context_data *device_context = container_of(nb, struct device_context_data, fb_notifier);
#ifdef CONFIG_LGE_CUSTOM_FB_NOTIFIER
	static const int BLANK = LGE_PANEL_EVENT_BLANK;
	struct lge_panel_notifier *event = data;
	int *event_data = &event->state;
#else
	static const int BLANK = FB_EVENT_BLANK;
	struct fb_event *event = data;
	int *event_data = event->data;
#endif
	char *envp[2];

        // printk(KERN_INFO "device_context " "%s: action %lu event->data %d\n", __func__, action, *event_data);

	/* If we aren't interested in this event, skip it immediately ... */
	if (action != BLANK && action != LGE_PANEL_EVENT_FPS) {
		return NOTIFY_DONE;
	}
	if (action == BLANK) {
		switch (*event_data) {
#ifdef CONFIG_LGE_CUSTOM_FB_NOTIFIER
		case LGE_PANEL_STATE_UNBLANK: // U3, UNBLANK
#else
		case FB_BLANK_UNBLANK:
#endif
			envp[0] = "SCREEN=1";
			break;

#ifdef CONFIG_LGE_CUSTOM_FB_NOTIFIER
		case LGE_PANEL_STATE_BLANK: // U0, BLANK
		case LGE_PANEL_STATE_LP1: // U2_UNBLANK @ AOD
		case LGE_PANEL_STATE_LP2: // U2_BLANK @ AOD
#else
		case FB_BLANK_POWERDOWN:
#endif
			envp[0] = "SCREEN=0";
			break;
		default:
			printk(KERN_WARNING "device_context " "%s: skip event data %d\n", __func__, *event_data);
			return NOTIFY_DONE;
		}
	}
	else if (action == LGE_PANEL_EVENT_FPS ) {
		envp[0] = "SCREEN=2"; // FPS CHANGE
	}

	envp[1] = NULL;
	printk(KERN_INFO "device_context " "%s: envp[0] %s\n", __func__, envp[0]);
	kobject_uevent_env(&device_context_sysfs_dev->kobj, KOBJ_CHANGE, envp);

	return NOTIFY_OK;
}
#endif // CONFIG_LGE_FB_NOTIFIER

static int device_context_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;
	struct device_node *np = dev->of_node;
	struct device_context_data *device_context = devm_kzalloc(dev, sizeof(*device_context), GFP_KERNEL);

	if (!device_context) {
		dev_err(dev,
			"failed to allocate memory for struct device_context_data\n");
		rc = -ENOMEM;
		goto exit;
	}

	device_context->dev = dev;
	platform_set_drvdata(pdev, device_context);

	if (!np) {
		dev_err(dev, "no of node found\n");
		rc = -EINVAL;
		goto exit;
	}

	dev->init_name = "device_context_sysfs_dev";

	device_context_class = class_create(THIS_MODULE, "lge_sns");
	if (IS_ERR(device_context_class)){
		pr_err("[DEV_CONT] %s: class_create() failed ENOMEM\n", __func__);
		rc = -ENOMEM;
		goto exit;
	}

	/* mutex is not used yet*/
	mutex_init(&device_context->lock);

	device_context_sysfs_dev = device_create(device_context_class, NULL, 0, device_context, "device_context");

#ifdef CONFIG_LGE_FB_NOTIFIER
	/* Register screen on/off callback. */
	device_context->fb_notifier.notifier_call = fb_notifier_callback;
	rc = fb_notifier_register(&device_context->fb_notifier);
	if (rc) {
		printk(KERN_ERR "device_context " "%s: could not register fb notifier", __func__);
		goto exit;
	}
#endif // CONFIG_LGE_FB_NOTIFIER

	dev_info(dev, "%s: ok\n", __func__);

exit:
	return rc;
}

static int device_context_remove(struct platform_device *pdev)
{
	struct device_context_data *device_context = platform_get_drvdata(pdev);

#ifdef CONFIG_LGE_FB_NOTIFIER
	fb_notifier_unregister(&device_context->fb_notifier);
#endif // CONFIG_LGE_FB_NOTIFIER

	mutex_destroy(&device_context->lock);
	/*wake_lock_destroy(&device_context->ttw_wl);*/
	dev_info(&pdev->dev, "%s\n", __func__);

	return 0;
}

static const struct of_device_id device_context_of_match[] = {
	{ .compatible = "lge,device_context", },
	{}
};
MODULE_DEVICE_TABLE(of, device_context_of_match);

static struct platform_driver device_context_driver = {
	.driver = {
		.name	= "device_context",
		.owner	= THIS_MODULE,
		.of_match_table = device_context_of_match,
	},
	.probe	= device_context_probe,
	.remove	= device_context_remove,
};

static int __init device_context_init(void)
{
	int rc = platform_driver_register(&device_context_driver);

	if (!rc)
		pr_info("%s OK\n", __func__);
	else
		pr_err("%s %d\n", __func__, rc);

	return rc;
}

static void __exit device_context_exit(void)
{
	pr_info("%s\n", __func__);
	platform_driver_unregister(&device_context_driver);
}

module_init(device_context_init);
module_exit(device_context_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Henry Seo <henry.seo@lge.com>");
MODULE_DESCRIPTION("LGE Device context driver for SNS CONTEXT.");
