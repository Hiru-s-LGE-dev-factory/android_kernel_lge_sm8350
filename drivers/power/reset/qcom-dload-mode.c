// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2020 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>
#include <linux/qcom_scm.h>
#include <soc/qcom/minidump.h>

#ifdef CONFIG_LGE_HANDLE_PANIC
#include <soc/qcom/lge/lge_handle_panic.h>
#endif
#ifdef CONFIG_LGE_POWEROFF_TIMEOUT
#include <asm/system_misc.h>
#include <linux/input/qpnp-power-on.h>
#define LGE_REBOOT_CMD_SIZE 32
struct lge_poweroff_timeout_struct {
	enum reboot_mode reboot_mode;
	char reboot_cmd[LGE_REBOOT_CMD_SIZE];
	struct hrtimer poweroff_timer;
};
#endif

enum qcom_download_dest {
	QCOM_DOWNLOAD_DEST_UNKNOWN = -1,
	QCOM_DOWNLOAD_DEST_QPST = 0,
	QCOM_DOWNLOAD_DEST_EMMC = 2,
};

struct qcom_dload {
	struct notifier_block panic_nb;
	struct notifier_block reboot_nb;
	struct kobject kobj;

	bool in_panic;
	void __iomem *dload_dest_addr;
};

#define to_qcom_dload(o) container_of(o, struct qcom_dload, kobj)

#define QCOM_DOWNLOAD_BOTHDUMP (QCOM_DOWNLOAD_FULLDUMP | QCOM_DOWNLOAD_MINIDUMP)

#ifdef CONFIG_LGE_HANDLE_PANIC
static bool enable_dump = true;
static bool ftm_crash_handler = false;
#else
static bool enable_dump =
	IS_ENABLED(CONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE_DEFAULT);
#endif
static enum qcom_download_mode current_download_mode = QCOM_DOWNLOAD_NODUMP;
static enum qcom_download_mode dump_mode = QCOM_DOWNLOAD_FULLDUMP;

static int set_download_mode(enum qcom_download_mode mode)
{
	if ((mode & QCOM_DOWNLOAD_MINIDUMP) && !msm_minidump_enabled()) {
		mode &= ~QCOM_DOWNLOAD_MINIDUMP;
		pr_warn("Minidump not enabled.\n");
		if (!mode)
			return -ENODEV;
	}
	current_download_mode = mode;
	qcom_scm_set_download_mode(mode, 0);
	return 0;
}

static int set_dump_mode(enum qcom_download_mode mode)
{
	int ret = 0;

	if (enable_dump) {
		ret = set_download_mode(mode);
		if (likely(!ret))
			dump_mode = mode;
	} else
		dump_mode = mode;
	return ret;
}

static void msm_enable_dump_mode(bool enable)
{
	if (enable)
		set_download_mode(dump_mode);
	else
		set_download_mode(QCOM_DOWNLOAD_NODUMP);
}

static void set_download_dest(struct qcom_dload *poweroff,
			      enum qcom_download_dest dest)
{
	if (poweroff->dload_dest_addr)
		__raw_writel(dest, poweroff->dload_dest_addr);
}
static enum qcom_download_dest get_download_dest(struct qcom_dload *poweroff)
{
	if (poweroff->dload_dest_addr)
		return __raw_readl(poweroff->dload_dest_addr);
	else
		return QCOM_DOWNLOAD_DEST_UNKNOWN;
}

static int param_set_download_mode(const char *val,
		const struct kernel_param *kp)
{
	int ret;

	/* update enable_dump according to user input */
	ret = param_set_bool(val, kp);
	if (ret)
		return ret;

	msm_enable_dump_mode(true);

	return 0;
}
module_param_call(download_mode, param_set_download_mode, param_get_int,
			&enable_dump, 0644);

/* interface for exporting attributes */
struct reset_attribute {
	struct attribute        attr;
	ssize_t (*show)(struct kobject *kobj, struct attribute *attr,
			char *buf);
	ssize_t (*store)(struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t count);
};
#define to_reset_attr(_attr) \
	container_of(_attr, struct reset_attribute, attr)

static ssize_t attr_show(struct kobject *kobj, struct attribute *attr,
				char *buf)
{
	struct reset_attribute *reset_attr = to_reset_attr(attr);
	ssize_t ret = -EIO;

	if (reset_attr->show)
		ret = reset_attr->show(kobj, attr, buf);

	return ret;
}

static ssize_t attr_store(struct kobject *kobj, struct attribute *attr,
				const char *buf, size_t count)
{
	struct reset_attribute *reset_attr = to_reset_attr(attr);
	ssize_t ret = -EIO;

	if (reset_attr->store)
		ret = reset_attr->store(kobj, attr, buf, count);

	return ret;
}

static const struct sysfs_ops reset_sysfs_ops = {
	.show	= attr_show,
	.store	= attr_store,
};

static struct kobj_type qcom_dload_kobj_type = {
	.sysfs_ops	= &reset_sysfs_ops,
};

static ssize_t emmc_dload_show(struct kobject *kobj,
			       struct attribute *this,
			       char *buf)
{
	struct qcom_dload *poweroff = to_qcom_dload(kobj);

	if (!poweroff->dload_dest_addr)
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n",
			get_download_dest(poweroff) == QCOM_DOWNLOAD_DEST_EMMC);
}
static ssize_t emmc_dload_store(struct kobject *kobj,
				struct attribute *this,
				const char *buf, size_t count)
{
	int ret;
	bool enabled;
	struct qcom_dload *poweroff = to_qcom_dload(kobj);

	if (!poweroff->dload_dest_addr)
		return -ENODEV;

	ret = kstrtobool(buf, &enabled);

	if (ret < 0)
		return ret;

	if (enabled)
		set_download_dest(poweroff, QCOM_DOWNLOAD_DEST_EMMC);
	else
		set_download_dest(poweroff, QCOM_DOWNLOAD_DEST_QPST);

	return count;
}
static struct reset_attribute attr_emmc_dload = __ATTR_RW(emmc_dload);

static ssize_t dload_mode_show(struct kobject *kobj,
			       struct attribute *this,
			       char *buf)
{
	const char *mode;

	switch ((unsigned int)dump_mode) {
	case QCOM_DOWNLOAD_FULLDUMP:
		mode = "full";
		break;
	case QCOM_DOWNLOAD_MINIDUMP:
		mode = "mini";
		break;
	case QCOM_DOWNLOAD_BOTHDUMP:
		mode = "both";
		break;
	default:
		mode = "unknown";
		break;
	}
	return scnprintf(buf, PAGE_SIZE, "DLOAD dump type: %s\n", mode);
}
static ssize_t dload_mode_store(struct kobject *kobj,
				struct attribute *this,
				const char *buf, size_t count)
{
	enum qcom_download_mode mode;

	if (sysfs_streq(buf, "full"))
		mode = QCOM_DOWNLOAD_FULLDUMP;
	else if (sysfs_streq(buf, "mini"))
		mode = QCOM_DOWNLOAD_MINIDUMP;
	else if (sysfs_streq(buf, "both"))
		mode = QCOM_DOWNLOAD_BOTHDUMP;
	else {
		pr_err("Invalid dump mode request...\n");
		pr_err("Supported dumps: 'full', 'mini', or 'both'\n");
		return -EINVAL;
	}

	return set_dump_mode(mode) ? : count;
}
static struct reset_attribute attr_dload_mode = __ATTR_RW(dload_mode);

static struct attribute *qcom_dload_attrs[] = {
	&attr_emmc_dload.attr,
	&attr_dload_mode.attr,
	NULL
};
static struct attribute_group qcom_dload_attr_group = {
	.attrs = qcom_dload_attrs,
};

static int qcom_dload_panic(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	struct qcom_dload *poweroff = container_of(this, struct qcom_dload,
						     panic_nb);
	poweroff->in_panic = true;
	if (enable_dump)
		msm_enable_dump_mode(true);
#ifdef CONFIG_LGE_HANDLE_PANIC
	lge_set_panic_reason();
#endif
	return NOTIFY_OK;
}

static int qcom_dload_reboot(struct notifier_block *this, unsigned long event,
			      void *ptr)
{
	char *cmd = ptr;
	struct qcom_dload *poweroff = container_of(this, struct qcom_dload,
						     reboot_nb);

	/* Clean shutdown, disable dump mode to allow normal restart */
	if (!poweroff->in_panic)
		set_download_mode(QCOM_DOWNLOAD_NODUMP);

	if (cmd) {
#ifndef CONFIG_LGE_HANDLE_PANIC
		if (!strcmp(cmd, "edl"))
			set_download_mode(QCOM_DOWNLOAD_EDL);
		else if (!strcmp(cmd, "qcom_dload"))
			msm_enable_dump_mode(true);
#else
		if (!strcmp(cmd, "qcom_dload"))
			msm_enable_dump_mode(true);
#endif
	}

	if (current_download_mode != QCOM_DOWNLOAD_NODUMP)
		reboot_mode = REBOOT_WARM;

	return NOTIFY_OK;
}

#ifdef CONFIG_LGE_HANDLE_PANIC
int lge_get_ftm_crash_handler()
{
	return ftm_crash_handler ? 1 : 0;
}
EXPORT_SYMBOL(lge_get_ftm_crash_handler);

int lge_get_download_mode()
{
	return enable_dump ? 1 : 0;
}
EXPORT_SYMBOL(lge_get_download_mode);

int lge_set_download_mode(int val)
{
	if(val >> 1)
		return -EINVAL;

	enable_dump = val ? true : false;
	if(enable_dump)
		msm_enable_dump_mode(true);
	return 0;
}
EXPORT_SYMBOL(lge_set_download_mode);

extern int skip_free_rdump;
static int __init lge_crash_handler(char *status)
{
	if (!strcmp(status, "on"))
	{
		enable_dump = true;
		skip_free_rdump = 1;
		ftm_crash_handler = true;
	}
	return 1;
}
__setup("lge.crash_handler=", lge_crash_handler);
#endif
#ifdef CONFIG_LGE_POWEROFF_TIMEOUT
static enum hrtimer_restart lge_poweroff_timeout_handler(struct hrtimer *timer)
{
	/* Disable interrupts first */
	local_irq_disable();
	smp_send_stop();

	pr_emerg("poweroff timeout expired. forced power off.\n");

#ifdef CONFIG_LGE_HANDLE_PANIC
	if (lge_get_ftm_crash_handler() == true) {
		BUG();
	}
	else {
		//FIXME copyed from do_msm_poweroff
		pr_notice("Powering off the SoC (pid: %d, comm: %s)\n",
				current->pid, current->comm);

		set_download_mode(QCOM_DOWNLOAD_NODUMP);
		qpnp_pon_system_pwr_off(PON_POWER_OFF_SHUTDOWN);

		qcom_scm_deassert_ps_hold();

		msleep(10000);
		pr_err("Powering off has failed\n");

	}
#endif

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart lge_reboot_timeout_handler(struct hrtimer *timer)
{
	struct lge_poweroff_timeout_struct *reboot_timeout;

	/* Disable interrupts first */
	local_irq_disable();
	smp_send_stop();

	reboot_timeout = container_of(timer, struct lge_poweroff_timeout_struct, poweroff_timer);

	pr_emerg("reboot timeout expired. forced reboot with cmd %s.\n", reboot_timeout->reboot_cmd);

#ifdef CONFIG_LGE_HANDLE_PANIC
	if (lge_get_download_mode() == true) {
		BUG();
	}
	else {
		//FIXME
		qcom_dload_reboot(NULL, reboot_timeout->reboot_mode, reboot_timeout->reboot_cmd);
	}
#endif

	return HRTIMER_NORESTART;
}

static void do_msm_poweroff_timeout(void)
{
	static struct lge_poweroff_timeout_struct lge_poweroff_timeout;
	u64 timeout_ms = 60*1000;

#ifdef CONFIG_LGE_HANDLE_PANIC
	if (lge_get_ftm_crash_handler() == true) {
		timeout_ms = 120*1000; // forbid false positive, for debugging
	} else {
		timeout_ms = 60*1000; // smooth action for customer
	}
#endif

	if (lge_poweroff_timeout.poweroff_timer.function == NULL) {
		hrtimer_init(&lge_poweroff_timeout.poweroff_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		lge_poweroff_timeout.poweroff_timer.function = lge_poweroff_timeout_handler;
		hrtimer_start(&lge_poweroff_timeout.poweroff_timer, ms_to_ktime(timeout_ms), HRTIMER_MODE_REL);
		pr_info("%s : %dsec \n", __func__, (timeout_ms / 1000));
	}
	else {
		pr_info("duplicated %s, ignored\n", __func__);
	}
}

static void do_msm_restart_timeout(enum reboot_mode reboot_mode, const char *cmd)
{
	static struct lge_poweroff_timeout_struct lge_reboot_timeout;
	u64 timeout_ms = 15*1000;

#ifdef CONFIG_LGE_HANDLE_PANIC
	if (lge_get_ftm_crash_handler() == true) {
		timeout_ms = 120*1000; // forbid false positive & for debugging
	} else {
		timeout_ms = 60*1000; // smooth action for customer
	}
#endif

	if (lge_reboot_timeout.poweroff_timer.function == NULL) {
		hrtimer_init(&lge_reboot_timeout.poweroff_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		lge_reboot_timeout.poweroff_timer.function = lge_reboot_timeout_handler;

		if ((cmd != NULL && cmd[0] != '\0')) {
			pr_info("%s %d %s : %dsec\n", __func__, reboot_mode, cmd, (timeout_ms / 1000));
			strlcpy(lge_reboot_timeout.reboot_cmd, cmd, LGE_REBOOT_CMD_SIZE);
		}
		else {
			pr_info("%s %d : %dsec\n", __func__, reboot_mode, (timeout_ms / 1000));
		}
		hrtimer_start(&lge_reboot_timeout.poweroff_timer, ms_to_ktime(timeout_ms), HRTIMER_MODE_REL);
	}
	else {
		pr_info("duplicated %s, ignored\n", __func__);
	}
}
#endif

static void __iomem *map_prop_mem(const char *propname)
{
	struct device_node *np = of_find_compatible_node(NULL, NULL, propname);
	void __iomem *addr;

	if (!np) {
		pr_err("Unable to find DT property: %s\n", propname);
		return NULL;
	}

	addr = of_iomap(np, 0);
	if (!addr)
		pr_err("Unable to map memory for DT property: %s\n", propname);
	return addr;
}

#ifdef CONFIG_RANDOMIZE_BASE
#define KASLR_OFFSET_MASK	0x00000000FFFFFFFF
static void store_kaslr_offset(void)
{
	void __iomem *mem = map_prop_mem("qcom,msm-imem-kaslr_offset");

	if (!mem)
		return;

	__raw_writel(0xdead4ead, mem);
	__raw_writel((kimage_vaddr - KIMAGE_VADDR) & KASLR_OFFSET_MASK,
		     mem + 4);
	__raw_writel(((kimage_vaddr - KIMAGE_VADDR) >> 32) & KASLR_OFFSET_MASK,
		     mem + 8);

	iounmap(mem);
}
#else
static void store_kaslr_offset(void) {}
#endif /* CONFIG_RANDOMIZE_BASE */

static int qcom_dload_probe(struct platform_device *pdev)
{
	struct qcom_dload *poweroff;
	int ret;

	if (!qcom_scm_is_available())
		return -EPROBE_DEFER;

	poweroff = devm_kzalloc(&pdev->dev, sizeof(*poweroff), GFP_KERNEL);
	if (!poweroff)
		return -ENOMEM;

	ret = kobject_init_and_add(&poweroff->kobj, &qcom_dload_kobj_type,
				   kernel_kobj, "dload");
	if (ret) {
		pr_err("%s: Error in creation kobject_add\n", __func__);
		kobject_put(&poweroff->kobj);
		return ret;
	}

	ret = sysfs_create_group(&poweroff->kobj, &qcom_dload_attr_group);
	if (ret) {
		pr_err("%s: Error in creation sysfs_create_group\n", __func__);
		kobject_del(&poweroff->kobj);
		return ret;
	}

	poweroff->dload_dest_addr = map_prop_mem("qcom,msm-imem-dload-type");
	store_kaslr_offset();

	msm_enable_dump_mode(enable_dump);
#ifdef CONFIG_LGE_HANDLE_PANIC
	if (!enable_dump) {
		qcom_scm_disable_sdi();
		lge_panic_handler_fb_cleanup();
	}
#else
	if (!enable_dump)
		qcom_scm_disable_sdi();
#endif

	poweroff->panic_nb.notifier_call = qcom_dload_panic;
	poweroff->panic_nb.priority = INT_MAX;
	atomic_notifier_chain_register(&panic_notifier_list,
				       &poweroff->panic_nb);

	poweroff->reboot_nb.notifier_call = qcom_dload_reboot;
	poweroff->reboot_nb.priority = 255;
	register_reboot_notifier(&poweroff->reboot_nb);

#ifdef CONFIG_LGE_POWEROFF_TIMEOUT
	pm_power_off_timeout = do_msm_poweroff_timeout;
	arm_pm_restart_timeout = do_msm_restart_timeout;
#endif

	platform_set_drvdata(pdev, poweroff);

	return 0;
}

static int qcom_dload_remove(struct platform_device *pdev)
{
	struct qcom_dload *poweroff = platform_get_drvdata(pdev);

	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &poweroff->panic_nb);
	unregister_reboot_notifier(&poweroff->reboot_nb);

	if (poweroff->dload_dest_addr)
		iounmap(poweroff->dload_dest_addr);

	return 0;
}

static const struct of_device_id of_qcom_dload_match[] = {
	{ .compatible = "qcom,dload-mode", },
	{},
};
MODULE_DEVICE_TABLE(of, of_qcom_dload_match);

static struct platform_driver qcom_dload_driver = {
	.probe = qcom_dload_probe,
	.remove = qcom_dload_remove,
	.driver = {
		.name = "qcom-dload-mode",
		.of_match_table = of_match_ptr(of_qcom_dload_match),
	},
};

static int __init qcom_dload_driver_init(void)
{
	return platform_driver_register(&qcom_dload_driver);
}
#if IS_MODULE(CONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE)
module_init(qcom_dload_driver_init);
#else
pure_initcall(qcom_dload_driver_init);
#endif

static void __exit qcom_dload_driver_exit(void)
{
	return platform_driver_unregister(&qcom_dload_driver);
}
module_exit(qcom_dload_driver_exit);

MODULE_DESCRIPTION("MSM Download Mode Driver");
MODULE_LICENSE("GPL v2");
