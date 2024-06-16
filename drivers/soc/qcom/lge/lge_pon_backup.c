/*
 * driver/power/lge_pon_backup.c
 *
 * Copyright (C) 2019 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "CHG: [PON_BACKUP] %s: " fmt, __func__

#include <linux/of.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/fs_struct.h>
#include <linux/platform_device.h>
#include "../../../power/supply/lge/veneer-primitives.h"

#define MODULE_NAME "lge_pon_backup"
#define PON_DELAY_MS 15000
#define FTM_BLOCK_SIZE		4096
#define FTM_PON_BACKUP_OFFSET 227
#define FTM_PON_BACKUP_SIZE 728

#define PON_BACKUP_MAX_OCP 3
#define PON_BACKUP_MAX_PMIC 6
#define PON_BACKUP_MAX_COUNT 10

#define FTM_PATH "/dev/block/bootdevice/by-name/ftm"

typedef struct {
  uint64_t pon_reg;
  uint64_t fault_reg[PON_BACKUP_MAX_PMIC];
  uint32_t entry_soc;
  uint32_t entry_volt;
  uint32_t entry_rtc;
} backup_data_type;

typedef struct {
  uint32_t count;
  backup_data_type data[PON_BACKUP_MAX_COUNT];
} pon_backup_type;

struct lge_pon_backup_data {
	struct delayed_work lge_pon_backup_dwork;
	int retry_count;
	u32 regulator_num[PON_BACKUP_MAX_PMIC * 3];
	const char *pmic_name[PON_BACKUP_MAX_PMIC];
	pon_backup_type pon_backup_data;
};

static const char * const qpnp_pon_reason_groups[] = {
	" PON=", " POFF=", " OCP=", " OFF="
};

static const char * const qpnp_pon_reasons[] = {
	"SMPL:", "RTC:", "PON1:", "KPD:", "SYSOK:", "HR:", "NA:", "NA:",
	"SOFT:", "PS_HOLD:", "PMIC_WD:", "KPD&RESIN:", "RESIN_S2:", "RESIN_D:", "KPD_S2:", "WARM:",
	"PMK8350:", "PM8350:", "PM8350C:", "PM8350B:", "PMR735A:", "PMR735B:", "NA:", "NA:",
	"RAW_XVDD:", "RAW_DVDD:", "IM_XVDD:", "PBS_NACK:", "FAULT_WD:", "FAULT_N", "SHUTDOWN:", "NA:"
};

static const char * const qpnp_fault_reason_groups[PON_BACKUP_MAX_PMIC] = {
	" PMK8350=", " PM8350=", " PM8350C=", " PM8350B=", " PMR735A=", " PMR735B="
};

static const char * const qpnp_fault_reasons[] = {
	"GP_F0:", "GP_F1:", "GP_F2:", "GP_F3:", "MBG:", "OVLO:", "UVLO:", "AVDD:",
	"0:", "1:", "2:", "FAULT:", "PBS_WD:", "PBS_NACK:", "RESET:", "OTST3:",
	"FAULT4:", "FAULT5:", "FAULT6:", "FAULT7:", "FAULT8:", "FAULT9:", "FAULT10:", "FAULT11:"
};

static const char * const qpnp_ocp_reasons[PON_BACKUP_MAX_OCP] = {
	"S", "LDO", "BOB"
};

#define VENEER_BACKUP_COOKIE_0	0x0001  /* cookie verion */
#define VENEER_BACKUP_COOKIE_1	0x5678
#define VENEER_BACKUP_COOKIE_2	0x90AB
#define VENEER_BACKUP_COOKIE_3	0xCDEF

/* pm_backup version history
 *
 *  ver_100 : creation of pm_backup_type structure. it derived from previous pon_backup.
 */
#define PM_BACKUP_VER    100
typedef struct {
  pon_backup_type pon_backup;
  veneer_backup_type veneer_backup;
  uint32_t ver;
} pm_backup_type;

static uint64_t last_pon_reg;

int lge_is_forced_reset(void)
{
	uint64_t temp_reg = 0;

	temp_reg = ((last_pon_reg >> 11) & 0xF);

	if (temp_reg != 0)
		return 1;

	return 0;
}
EXPORT_SYMBOL(lge_is_forced_reset);

int lge_is_warm_reset(void)
{
	uint64_t temp_reg = 0;

	temp_reg = ((last_pon_reg >> 15) & 0x1);

	return (int)temp_reg;
}
EXPORT_SYMBOL(lge_is_warm_reset);

int lge_get_pon_reason(void)
{
	uint64_t temp_reg = 0;

	temp_reg = (last_pon_reg & 0xFF);
	return (int)temp_reg;
}
EXPORT_SYMBOL(lge_get_pon_reason);

static int lge_get_pon_backup(pon_backup_type *now_pon)
{
	struct file *fp;
	struct path root;
	int pon_count = 0, cnt = 0;
	pm_backup_type pm_backup = {0, };
	mm_segment_t old_fs=get_fs();
	pr_info("lge_pon_backup_func start\n");
	set_fs(KERNEL_DS);

	memset(&root, 0, sizeof(struct path));
	task_lock(&init_task);
	get_fs_root(init_task.fs, &root);
	task_unlock(&init_task);
	fp = file_open_root(root.dentry, root.mnt, FTM_PATH, O_RDWR, 0);
	path_put(&root);

	if(IS_ERR(fp)){
		pr_err("Unable to open FTM (%ld)\n", PTR_ERR(fp));
		pon_count = -1;
		goto err;
	}

	fp->f_pos = FTM_PON_BACKUP_OFFSET * FTM_BLOCK_SIZE; // LGFTM_PON_BACK_UP offset
	cnt = vfs_read(fp, (char*)&pm_backup, sizeof(pm_backup), &fp->f_pos);
	if(cnt != sizeof(pm_backup)){
		pr_err("Unable to read FTM (%d)\n", cnt);
		pon_count = -1;
		goto err;
	}

	memcpy(now_pon, &pm_backup.pon_backup, sizeof(pon_backup_type));
	if (now_pon->count >= PON_BACKUP_MAX_COUNT)
		pon_count = PON_BACKUP_MAX_COUNT;
	else
		pon_count = now_pon->count;

	pr_info("FTM Size %ld, pon count %d", sizeof(*now_pon), now_pon->count);
	now_pon->count = 0;
	pm_backup.pon_backup.count = now_pon->count;

	fp->f_pos = FTM_PON_BACKUP_OFFSET * FTM_BLOCK_SIZE; // LGFTM_PON_BACK_UP offset
	cnt = vfs_write(fp, (char*)&pm_backup, sizeof(pm_backup), &fp->f_pos);
	if(cnt != sizeof(pm_backup)) {
		pr_err("Unable to write FTM (%d)\n", cnt);
		goto err;
	}
err:
	if (!IS_ERR(fp))
		filp_close(fp, NULL);

	set_fs(old_fs);

	return pon_count;
}

int lge_get_veneer_backup(void *data)
{
	struct file *fp = NULL;
	struct path root;
	pm_backup_type pm_backup = {0, };
	veneer_backup_type *veneer_backup = (veneer_backup_type *) data;
	int cnt = 0, ret = 0;
	mm_segment_t old_fs = get_fs();

	if (!veneer_backup) {
		pr_err("parameter is null\n");
		return -1;
	}

	set_fs(KERNEL_DS);

	memset(&root, 0, sizeof(struct path));
	task_lock(&init_task);
	get_fs_root(init_task.fs, &root);
	task_unlock(&init_task);
	fp = file_open_root(root.dentry, root.mnt, FTM_PATH, O_RDWR, 0);
	path_put(&root);

	if (IS_ERR(fp)) {
		pr_err("Unable to open FTM (%ld)\n", PTR_ERR(fp));
		ret = -1;
		goto err;
	}

	fp->f_pos = FTM_PON_BACKUP_OFFSET * FTM_BLOCK_SIZE; // LGFTM_PON_BACK_UP offset
	cnt = vfs_read(fp, (char*)&pm_backup, sizeof(pm_backup), &fp->f_pos);
	if (cnt != sizeof(pm_backup)) {
		pr_err("Unable to read FTM (%d)\n", cnt);
		ret = -1;
		goto err;
	}

	if (!(pm_backup.veneer_backup.cookie[0] == VENEER_BACKUP_COOKIE_0 &&
	      pm_backup.veneer_backup.cookie[1] == VENEER_BACKUP_COOKIE_1 &&
	      pm_backup.veneer_backup.cookie[2] == VENEER_BACKUP_COOKIE_2 &&
	      pm_backup.veneer_backup.cookie[3] == VENEER_BACKUP_COOKIE_3)) {
		pm_backup.veneer_backup.is_good_cookie = 0;
		veneer_backup->is_good_cookie = 0;
	} else {
		pm_backup.veneer_backup.is_good_cookie = 1;
		memcpy(veneer_backup, &pm_backup.veneer_backup, sizeof(veneer_backup_type));
	}

	pr_info("read veneer backup. status=%s, cookie=%X,%X,%X,%X, "
	        "[0]=%d, [1]=%d, [2]=%d, [3]=%d, [4]=%d, "
			"[5]=%d, [6]=%d, [7]=%d, [8]=%d, [9]=%d\n",
		pm_backup.veneer_backup.is_good_cookie ? "GOOD" : "NOT_SET",
		pm_backup.veneer_backup.cookie[0],
		pm_backup.veneer_backup.cookie[1],
		pm_backup.veneer_backup.cookie[2],
		pm_backup.veneer_backup.cookie[3],
		pm_backup.veneer_backup.data[0],
		pm_backup.veneer_backup.data[1],
		pm_backup.veneer_backup.data[2],
		pm_backup.veneer_backup.data[3],
		pm_backup.veneer_backup.data[4],
		pm_backup.veneer_backup.data[5],
		pm_backup.veneer_backup.data[6],
		pm_backup.veneer_backup.data[7],
		pm_backup.veneer_backup.data[8],
		pm_backup.veneer_backup.data[9]);

err:
	if (!IS_ERR(fp))
		filp_close(fp, NULL);

	set_fs(old_fs);

	return ret;
}
EXPORT_SYMBOL(lge_get_veneer_backup);

int lge_set_veneer_backup(void *data)
{
	struct file *fp = NULL;
	struct path root;
	pm_backup_type pm_backup = {0, };
	veneer_backup_type *veneer_backup = (veneer_backup_type *) data;
	int cnt = 0, ret = 0;
	mm_segment_t old_fs=get_fs();

	if (!veneer_backup) {
		pr_err("parameter is null\n");
		return -1;
	}

	set_fs(KERNEL_DS);

	memset(&root, 0, sizeof(struct path));
	task_lock(&init_task);
	get_fs_root(init_task.fs, &root);
	task_unlock(&init_task);
	fp = file_open_root(root.dentry, root.mnt, FTM_PATH, O_RDWR, 0);
	path_put(&root);

	if (IS_ERR(fp)) {
		pr_err("Unable to open FTM (%ld)\n", PTR_ERR(fp));
		ret = -1;
		goto err;
	}

	fp->f_pos = FTM_PON_BACKUP_OFFSET * FTM_BLOCK_SIZE; // LGFTM_PON_BACK_UP offset
	cnt = vfs_read(fp, (char*)&pm_backup, sizeof(pm_backup), &fp->f_pos);
	if (cnt != sizeof(pm_backup)) {
		pr_err("Unable to read FTM (%d)\n", cnt);
		ret = -1;
		goto err;
	}

	if (!(pm_backup.veneer_backup.cookie[0] == VENEER_BACKUP_COOKIE_0 &&
	      pm_backup.veneer_backup.cookie[1] == VENEER_BACKUP_COOKIE_1 &&
	      pm_backup.veneer_backup.cookie[2] == VENEER_BACKUP_COOKIE_2 &&
	      pm_backup.veneer_backup.cookie[3] == VENEER_BACKUP_COOKIE_3))
	{
		memset(&pm_backup.veneer_backup, 0, sizeof(veneer_backup_type));
		veneer_backup->cookie[0] = VENEER_BACKUP_COOKIE_0;
		veneer_backup->cookie[1] = VENEER_BACKUP_COOKIE_1;
		veneer_backup->cookie[2] = VENEER_BACKUP_COOKIE_2;
		veneer_backup->cookie[3] = VENEER_BACKUP_COOKIE_3;
		veneer_backup->is_good_cookie = 1;
		pm_backup.ver = PM_BACKUP_VER;
	}
	memcpy(&pm_backup.veneer_backup, veneer_backup, sizeof(veneer_backup_type));

	fp->f_pos = FTM_PON_BACKUP_OFFSET * FTM_BLOCK_SIZE; // LGFTM_PON_BACK_UP offset
	cnt = vfs_write(fp, (char*)&pm_backup, sizeof(pm_backup), &fp->f_pos);
	if(cnt != sizeof(pm_backup)) {
		pr_err("Unable to write FTM (%d)\n", cnt);
		ret = -1;
		goto err;
	}

	pr_info("write veneer backup. status=%s, cookie=%X,%X,%X,%X, "
	        "[0]=%d, [1]=%d, [2]=%d, [3]=%d, [4]=%d, "
			"[5]=%d, [6]=%d, [7]=%d, [8]=%d, [9]=%d\n",
		pm_backup.veneer_backup.is_good_cookie ? "GOOD" : "NOT_SET",
		pm_backup.veneer_backup.cookie[0],
		pm_backup.veneer_backup.cookie[1],
		pm_backup.veneer_backup.cookie[2],
		pm_backup.veneer_backup.cookie[3],
		pm_backup.veneer_backup.data[0],
		pm_backup.veneer_backup.data[1],
		pm_backup.veneer_backup.data[2],
		pm_backup.veneer_backup.data[3],
		pm_backup.veneer_backup.data[4],
		pm_backup.veneer_backup.data[5],
		pm_backup.veneer_backup.data[6],
		pm_backup.veneer_backup.data[7],
		pm_backup.veneer_backup.data[8],
		pm_backup.veneer_backup.data[9]);

err:
	if (!IS_ERR(fp))
		filp_close(fp, NULL);

	set_fs(old_fs);

	return ret;
}
EXPORT_SYMBOL(lge_set_veneer_backup);

#define PM_STATUS_MSG_LEN 300
static void lge_pon_backup_func(struct work_struct *w)
{
	struct lge_pon_backup_data *pon_backup = container_of(to_delayed_work(w),
		struct lge_pon_backup_data, lge_pon_backup_dwork);
	pon_backup_type* now_pon = &pon_backup->pon_backup_data;
	backup_data_type* now_data = NULL;
	int i, j, count, index = 0, pre_index, reg_index, ocp_index;
	char buf[PM_STATUS_MSG_LEN] = "";
	char reg[PM_STATUS_MSG_LEN] = "";
	char address[20] = "";
	char name[16] = "";
	char num[10] = "";
	uint64_t pon_reg, fault_reg, ocp_reg;
	struct tm tm = {0,};
	struct timespec64 time = {0,};
	struct rtc_time rtc_tm = {0, };
	struct rtc_device *rtc;
	time64_t now_time;
	bool is_stack_log = false;
	bool is_stack_reg = false;
	const int sep = ARRAY_SIZE(qpnp_pon_reasons) / ARRAY_SIZE(qpnp_pon_reason_groups);

	count = lge_get_pon_backup(now_pon);
	if (count < 0) {
		if (pon_backup->retry_count < 5) {
			pon_backup->retry_count++;
			schedule_delayed_work(&pon_backup->lge_pon_backup_dwork,
				msecs_to_jiffies(PON_DELAY_MS));
		}
		return;
	}

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("Failed to open rtc device (%s)\n",
				CONFIG_RTC_HCTOSYS_DEVICE);
		return;
	}
	rtc_read_time(rtc, &rtc_tm);
	now_time = rtc_tm_to_time64(&rtc_tm);
	time = timespec_to_timespec64(__current_kernel_time());

	for (i = 0; i < count; i++) {
		now_data = &now_pon->data[i];
		pon_reg = now_data->pon_reg;

		index = 0;
		pre_index = -sep;

		while (pon_reg != 0 && index < ARRAY_SIZE(qpnp_pon_reasons)) {
			if (pon_reg & 1) {
				if ((index / sep) != (pre_index / sep))
					strlcat(buf, qpnp_pon_reason_groups[index / sep], PM_STATUS_MSG_LEN);
				strlcat(buf, qpnp_pon_reasons[index], PM_STATUS_MSG_LEN);
				pre_index = index;
				is_stack_log = true;
			}
			pon_reg = pon_reg >> 1;
			index++;
		}

		for (j = 0; j < PON_BACKUP_MAX_PMIC; j++) {
			fault_reg = (now_data->fault_reg[j] & 0xFFFFFF);
			ocp_reg = (now_data->fault_reg[j] >> 24);

			if (fault_reg == 0 && ocp_reg == 0)
				continue;

			index = 0;

			if (fault_reg != 0) {
				strlcat(buf, qpnp_fault_reason_groups[j], PM_STATUS_MSG_LEN);
				snprintf(address, sizeof(address), "f%d-0x%llx:", j, fault_reg);
				strlcat(reg, address, PM_STATUS_MSG_LEN);
				is_stack_reg = true;
			}

			while (fault_reg != 0 && index < ARRAY_SIZE(qpnp_fault_reasons)) {
				if (fault_reg & 1) {
					strlcat(buf, qpnp_fault_reasons[index], PM_STATUS_MSG_LEN);
					is_stack_log = true;
				}
				fault_reg = fault_reg >> 1;
				index++;
			}

			index = 0;
			pre_index = 0;

			if (ocp_reg != 0) {
				snprintf(address, sizeof(address), "o%d-0x%llx:", j, ocp_reg);
				strlcat(reg, address, PM_STATUS_MSG_LEN);
				is_stack_reg = true;
			}

			while (ocp_reg != 0) {
				if (ocp_reg & 1) {
					pre_index = 0;
					ocp_index = 0;
					snprintf(name, sizeof(name), "%s OCP=", pon_backup->pmic_name[j]);
					strlcat(buf, name, PM_STATUS_MSG_LEN);
					for (reg_index = 0; reg_index < PON_BACKUP_MAX_OCP; reg_index++) {
						ocp_index += pon_backup->regulator_num[j * PON_BACKUP_MAX_OCP + reg_index];
						if (index < ocp_index)
							break;
						pre_index = ocp_index;
					}

					if (reg_index < PON_BACKUP_MAX_OCP) {
						strlcat(buf, qpnp_ocp_reasons[reg_index], PM_STATUS_MSG_LEN);
						snprintf(num, sizeof(num), "%d", index - pre_index + 1);
						strlcat(buf, num, PM_STATUS_MSG_LEN);
						is_stack_log = true;
					}
				}
				ocp_reg = ocp_reg >> 1;
				index++;
			}
		}

		if (is_stack_reg)
			pr_info("Pon reg boot[%d] 0x%llx:%s\n", i, now_data->pon_reg, reg);
		else
			pr_info("Pon reg boot[%d] 0x%llx\n", i, now_data->pon_reg);

		if (is_stack_log)
			pr_info("power on/off reason boot[%d]%s\n", i, buf);

		if (now_data->entry_soc != 0 || now_data->entry_volt != 0 || now_data->entry_rtc != 0) {
			time64_to_tm(time.tv_sec - now_time + now_data->entry_rtc, sys_tz.tz_minuteswest * 60 * (-1), &tm);
			pr_info("power on/off reason boot[%d] vol = %d, rtc = %d-%02d-%02d %02d:%02d:%02d(%d)\n",
				i, now_data->entry_volt,
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec, now_data->entry_rtc);
		}

		snprintf(buf, sizeof(buf), "");
		snprintf(reg, sizeof(reg), "");
	}

	rtc_class_close(rtc);
	last_pon_reg = now_pon->data[i - 1].pon_reg;

	return;
}

static int lge_pon_backup_probe(struct platform_device *pdev)
{
	struct lge_pon_backup_data *pon_backup;
	const struct device_node *node = pdev->dev.of_node;
	int rc = 0;

	pr_info("start Pon BackUp\n");
	pon_backup = kzalloc(sizeof(struct lge_pon_backup_data),
								GFP_KERNEL);
	if (!pon_backup) {
		pr_err("Fail to get alloc of pon\n");
		return -EIO;
	}

	rc = of_property_read_u32_array(node, "lge,regulator-table", pon_backup->regulator_num,
			PON_BACKUP_MAX_PMIC * 3);
	if (rc) {
		pr_err("Fail to get dt of regulator table. rc = %d\n", rc);
		return -EIO;
	}

	rc = of_property_read_string_array(node, "lge,pmic-name", pon_backup->pmic_name,
			PON_BACKUP_MAX_PMIC);
	if (rc < 0) {
		pr_err("Fail to get dt of pmic name. rc = %d\n", rc);
		return -EIO;
	}

	pon_backup->retry_count = 0;
	INIT_DELAYED_WORK(&pon_backup->lge_pon_backup_dwork,
		lge_pon_backup_func);
	schedule_delayed_work(&pon_backup->lge_pon_backup_dwork,
		msecs_to_jiffies(PON_DELAY_MS));

	return 0;
}

static struct of_device_id lge_pon_backup_match[] = {
	{ .compatible = "lge,pon-backup"},
	{}
};

static int lge_pon_backup_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver lge_pon_backup_driver = {
	.probe = lge_pon_backup_probe,
	.remove = lge_pon_backup_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lge_pon_backup_match,
	},
};

static int __init lge_pon_backup_init(void)
{
	return platform_driver_register(&lge_pon_backup_driver);
}

static void __exit lge_pon_backup_exit(void)
{
	platform_driver_unregister(&lge_pon_backup_driver);
}

module_init(lge_pon_backup_init);
module_exit(lge_pon_backup_exit);

MODULE_DESCRIPTION("LGE pon backup");
MODULE_LICENSE("GPL");
