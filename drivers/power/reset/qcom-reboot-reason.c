// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/of_address.h>
#include <linux/nvmem-consumer.h>

#ifdef CONFIG_LGE_HANDLE_PANIC
#include <soc/qcom/lge/lge_handle_panic.h>
#endif
#ifdef CONFIG_LGE_PON_RESTART_REASON
#include <linux/input/qpnp-power-on.h>
#include <soc/qcom/lge/board_lge.h>
#endif

#ifdef CONFIG_LGE_USB_GADGET
/* dload specific suppot */
#define PID_MAGIC_ID		0x71432909
#define SERIAL_NUM_MAGIC_ID	0x61945374
#define SERIAL_NUMBER_LENGTH	128

struct dload_struct {
	u32	pid;
	char	serial_number[SERIAL_NUMBER_LENGTH];
	u32	pid_magic;
	u32	serial_magic;
};

static struct dload_struct __iomem *diag_dload;
#endif

struct qcom_reboot_reason {
	struct device *dev;
	struct notifier_block reboot_nb;
	struct nvmem_cell *nvmem_cell;
};

struct poweroff_reason {
	const char *cmd;
	unsigned char pon_reason;
};

static struct poweroff_reason reasons[] = {
	{ "recovery",			0x01 },
	{ "bootloader",			0x02 },
	{ "rtc",			0x03 },
	{ "dm-verity device corrupted",	0x04 },
	{ "dm-verity enforcing",	0x05 },
	{ "keys clear",			0x06 },
#ifdef CONFIG_LGE_PON_RESTART_REASON
	{ "dload", PON_RESTART_REASON_LAF_DLOAD_MODE },
	{ "apdp_sdcard", PON_RESTART_REASON_APDP_SDCARD },
	{ "apdp_update", PON_RESTART_REASON_APDP_UPDATE },
#endif
#if defined(CONFIG_LGE_DISPLAY_DIMMING_BOOT_SUPPORT)
	{ "FOTA LCD off", 0x23},
	{ "FOTA OUT LCD off", 0x24},
	{ "LCD off", 0x25},
#endif
#ifdef CONFIG_LGE_PM
	{ "oem-11", PON_RESTART_REASON_SHIP_MODE},
#endif
	{}
};

static int qcom_reboot_reason_reboot(struct notifier_block *this,
				     unsigned long event, void *ptr)
{
	char *cmd = ptr;
	struct qcom_reboot_reason *reboot = container_of(this,
		struct qcom_reboot_reason, reboot_nb);
	struct poweroff_reason *reason;

#ifdef CONFIG_LGE_HANDLE_PANIC
	struct task_struct *task = current;
	pr_notice("Going down for restart now (pid: %d, comm: %s)\n",
			                        task->pid, task->comm);
#endif
	if (!cmd)
		return NOTIFY_OK;
	for (reason = reasons; reason->cmd; reason++) {
		if (!strcmp(cmd, reason->cmd)) {
#ifdef CONFIG_LGE_PON_RESTART_REASON
			if (!strncmp(cmd, "apdp_update", 13)) {
				switch(lge_get_bootreason()) {
					/* some restart reason should keep original reason*/
					case PON_RESTART_REASON_FOTA_LCD_OFF:
						reason->pon_reason = PON_RESTART_REASON_FOTA_LCD_OFF;
						break;
					case PON_RESTART_REASON_FOTA_OUT_LCD_OFF:
						reason->pon_reason = PON_RESTART_REASON_FOTA_OUT_LCD_OFF;
						break;
					case PON_RESTART_REASON_LCD_OFF:
						reason->pon_reason = PON_RESTART_REASON_LCD_OFF;
						break;
					default:
						break;
				}
			} else
#endif
#ifdef CONFIG_LGE_USB_GADGET
			if (!strncmp(cmd, "dload", 6)) {
				if (diag_dload && diag_dload->pid_magic == PID_MAGIC_ID) {
					switch (diag_dload->pid) {
						case 0x633A: // common
						case 0x631B: // common_fusion_chip (temporary)
						case 0x634A: // common_fusion_chip
							reason->pon_reason = PON_RESTART_REASON_LAF_DLOAD_BOOT;
							break;
						case 0x633F: // common_fusion_chip
						case 0x633E: // common
						case 0x62CE: // vzw
						case 0x62CF: // vzw_fusion_chip
							reason->pon_reason = PON_RESTART_REASON_LAF_DLOAD_MTP;
							break;
						case 0x6343: // common_fusion_chip
						case 0x6344: // common
						case 0x62C4: // vzw
						case 0x62C3: // vzw_fusion_chip
							reason->pon_reason = PON_RESTART_REASON_LAF_DLOAD_TETHER;
							break;
						case 0x6000: // factory
							reason->pon_reason = PON_RESTART_REASON_LAF_DLOAD_FACTORY;
							break;
						default:
							reason->pon_reason = PON_RESTART_REASON_LAF_DLOAD_MODE;
							break;
					}
				}
			}
#endif
			nvmem_cell_write(reboot->nvmem_cell,
					 &reason->pon_reason,
					 sizeof(reason->pon_reason));
			break;
		}
	}

	return NOTIFY_OK;
}

static int qcom_reboot_reason_probe(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot;
#ifdef CONFIG_LGE_USB_GADGET
	struct device_node *np;
#endif

	reboot = devm_kzalloc(&pdev->dev, sizeof(*reboot), GFP_KERNEL);
	if (!reboot)
		return -ENOMEM;

	reboot->dev = &pdev->dev;

	reboot->nvmem_cell = nvmem_cell_get(reboot->dev, "restart_reason");

	if (IS_ERR(reboot->nvmem_cell))
		return PTR_ERR(reboot->nvmem_cell);

	reboot->reboot_nb.notifier_call = qcom_reboot_reason_reboot;
	reboot->reboot_nb.priority = 255;
	register_reboot_notifier(&reboot->reboot_nb);

	platform_set_drvdata(pdev, reboot);

#ifdef CONFIG_LGE_USB_GADGET
	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-diag-dload");
	if (!np)
		np = of_find_compatible_node(NULL, NULL, "qcom,android-usb");

	if (!np)
		pr_warn("diag: failed to find diag_dload imem node\n");

	diag_dload  = np ? of_iomap(np, 0) : NULL;
#endif

	return 0;
}

static int qcom_reboot_reason_remove(struct platform_device *pdev)
{
	struct qcom_reboot_reason *reboot = platform_get_drvdata(pdev);

#ifdef CONFIG_LGE_USB_GADGET
	iounmap(diag_dload);
#endif
	unregister_reboot_notifier(&reboot->reboot_nb);

	return 0;
}

static const struct of_device_id of_qcom_reboot_reason_match[] = {
	{ .compatible = "qcom,reboot-reason", },
	{},
};
MODULE_DEVICE_TABLE(of, of_qcom_reboot_reason_match);

static struct platform_driver qcom_reboot_reason_driver = {
	.probe = qcom_reboot_reason_probe,
	.remove = qcom_reboot_reason_remove,
	.driver = {
		.name = "qcom-reboot-reason",
		.of_match_table = of_match_ptr(of_qcom_reboot_reason_match),
	},
};

module_platform_driver(qcom_reboot_reason_driver);

MODULE_DESCRIPTION("MSM Reboot Reason Driver");
MODULE_LICENSE("GPL v2");
