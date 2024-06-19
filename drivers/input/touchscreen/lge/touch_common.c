/*
 * touch_common.c
 *
 * Copyright (c) 2015 LGE.
 *
 * author : hoyeon.jang@lge.com
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/input/lge_touch_notify.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>
#include <touch_common.h>
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
/* write function add here */
int core_write_cmd(struct device *dev, int cmd) {
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	TOUCH_TRACE();
	/* WorkQueue recommend */
	switch (cmd) {
		case SYSTRACE:
			if (sscanf(ts->cmd_data.cmd_buf, "%d", &ts->systrace_mode) <= 0) {
				ret = -1;
			}
			break;
		case CMD_LIST:
			break;
		case SYSFS_LIST:
			break;
		default:
			TOUCH_I("cmd not found\n");
			ret = -1;
			break;
	}

	if (ret >= 0) {
		TOUCH_I("%s cmd:%d (%s)\n", __func__, cmd, ts->cmd_data.cmd_buf);
		cmd_set(dev, "core", ts->cmd_data.cmd_buf,
				ts->cmd_data.rw_mode);
	}

	return ret;
}

/* read function add here */
int core_read_cmd(struct device *dev, int cmd) {
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int i = 0;
	TOUCH_TRACE();
	/* WorkQueue recommend */
	switch (cmd) {
		case SYSTRACE:
			ret = snprintf(ts->cmd_data.read_buf, MAX_CMD_LENGTH,
					"%d", ts->systrace_mode);
			break;
		case CMD_LIST:
			for (i = 0; i < MAX_CMD_COUNTS; i++) {
				if (ts->cmd_data.cmd_lists[i][0] == NULL) {
					break;
				}
				ret += snprintf(ts->cmd_data.read_buf + ret, MAX_CMD_LENGTH - ret, "%s %s\n",
						ts->cmd_data.cmd_lists[i][1], ts->cmd_data.cmd_lists[i][5]);
			}
			break;
		case SYSFS_LIST:
			for (i = 0; i < MAX_SYSFS_LENGTH; i++) {
				if (ts->cmd_data.sysfs_lists[i] == NULL)
					break;
				ret += snprintf(ts->cmd_data.read_buf + ret,
						MAX_CMD_LENGTH - ret, "%s\n",
						ts->cmd_data.sysfs_lists[i]);
			}
			break;
		default:
			TOUCH_I("cmd not found\n");
			ret = -1;
			break;
	}

	if (ret >= 0) {
		TOUCH_I("%s cmd:%d (%s)\n", __func__, cmd, ts->cmd_data.read_buf);
		cmd_set(dev, "core", ts->cmd_data.read_buf,
				ts->cmd_data.rw_mode);
	}

	return ret;
}

int core_common_control_handler(struct device *dev, int rw_mode, int cmd)
{
	int ret = 0;
	TOUCH_TRACE();
	if (rw_mode == WRITE_MODE) {
		ret = core_write_cmd(dev, cmd);
	} else if (rw_mode == READ_MODE) {
		ret = core_read_cmd(dev, cmd);
	} else {
	}
	return ret;
}

char *cmd_get_value(struct device *dev, char *target, char *cmd) {
	struct touch_core_data *ts = to_touch_core(dev);
	char *return_value = NULL;
	int i = 0;

	for (i = 0; i < MAX_CMD_COUNTS; i++) {
		if (ts->cmd_data.cmd_lists[i][0] == NULL ||
				i == (MAX_CMD_COUNTS - 1)) {
			break;
		} else {
			if (!strcmp(cmd, ts->cmd_data.cmd_lists[i][1])) {
				if (!strcmp(target, "target")) {
					return_value = ts->cmd_data.cmd_lists[i][0];
				} else if (!strcmp(target, "typedef")) {
					return_value = ts->cmd_data.cmd_lists[i][5];
				}
			}
		}
	}
	TOUCH_I("return:%s\n", return_value);
	return return_value;
}

/* cmd parse and distribute data buffer */
int common_control(struct device *dev, char *target)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int rw_mode = RW_MODE_NA;
	int cmd_r = -1;
	char *type = NULL;
	TOUCH_TRACE();
	if (ts->cmd_data.rw_mode != NULL && ts->cmd_data.owner != NULL) {
		if (!strcmp(ts->cmd_data.rw_mode, "write")) {
			rw_mode = WRITE_MODE;
			if (ts->cmd_data.cmd_buf == NULL) {
				TOUCH_I("wbuf is null\n");
				ret = -EINVAL;
				goto end;
			}
		} else if (!strcmp(ts->cmd_data.rw_mode, "read")) {
			rw_mode = READ_MODE;
			if(ts->cmd_data.read_buf == NULL) {
				ts->cmd_data.read_buf = kzalloc(MAX_CMD_LENGTH, GFP_KERNEL);
				if (ts->cmd_data.read_buf == NULL) {
					TOUCH_E("failed to kzalloc read buf\n");
					ts->cmd_data.read_buf = NULL;
					ret = -EINVAL;
					goto end;
				}
			}
			if (ts->cmd_data.cmd_buf != NULL &&
					strstr(ts->cmd_data.cmd_buf, "typedef")) {
					type = cmd_get_value(ts->dev, "typedef", ts->cmd_data.cmd_name);
					TOUCH_I("%s:%s\n",ts->cmd_data.cmd_buf, type);
					if (type != NULL) {
						cmd_set(dev, target, type,
								ts->cmd_data.rw_mode);
						goto end;
					}
			}
		} else if (!strcmp(ts->cmd_data.rw_mode, "clear")) {
			rw_mode = CLEAR_MODE;
			cmd_set(dev, target, "", "");
			goto end;
		} else {
			ret = -EINVAL;
			goto end;
		}
	} else {
		TOUCH_I("rw is null\n");
		ret = -EINVAL;
		goto end;
	}

	if (!strcmp(target, "core")) {
		/* common control for core driver */
		TOUCH_I("core conctrol detect\n");
		cmd_r = cmd_num_search(dev, "core", ts->cmd_data.cmd_name);
		ret = core_common_control_handler(dev, rw_mode, cmd_r);
	} else {
		if (ts->touch_ic_name != NULL) {
			if (!strcmp(target, ts->touch_ic_name)) {
				/* control for vendor IC */
				TOUCH_I("vendor conctrol detect\n");
				if (ts->driver->common_control != NULL) {
					ret = ts->driver->common_control(dev, rw_mode, ts->cmd_data.cmd_name);
				} else {
					TOUCH_I("vendor control driver not detect\n");
					ret = -1;
				}
				/* Can add other target
				 * module, pen, dex etc ..
				 * */
			} else {
				TOUCH_I("not support\n");
				ret = -1;
			}
		} else {
			TOUCH_I("touch_ic_name is null\n");
			ret = -1;
		}
	}
end:
	if (ts->cmd_data.cmd_buf != NULL) {
		kfree(ts->cmd_data.cmd_buf);
		ts->cmd_data.cmd_buf = NULL;
	}
	if (ts->cmd_data.read_buf != NULL) {
		kfree(ts->cmd_data.read_buf);
		ts->cmd_data.read_buf = NULL;
	}
	if (ts->cmd_data.owner != NULL) {
		kfree(ts->cmd_data.owner);
		ts->cmd_data.owner = NULL;
	}
	return ret;
}

int read_sysfs_lists(struct device *dev, const struct attribute_group *list)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct kernfs_node *kn;
	struct attribute *const *attr;
	struct bin_attribute *const *bin_attr;
	int i = 0;
	int ret = 0;

	kn = ts->kobj.sd;
	TOUCH_I("kn name:%s\n", kn->name);
	if (list->attrs) {
		for (i = 0, attr = list->attrs; *attr; i++, attr++) {
			if ((*attr)->name == NULL) {
				break;
			}
			sysfs_adder(dev, (*attr)->name);
		}
	}
	if (list->bin_attrs) {
		for (i = 0, bin_attr = list->bin_attrs; *bin_attr; i++, bin_attr++) {
			if ((*bin_attr)->attr.name == NULL) {
				break;
			}
			sysfs_adder(dev, (*bin_attr)->attr.name);
		}
	}
	return ret;
}

int cmds_adder(struct device *dev, char *target, char *(*list)[2])
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int i = 0;
	int eof = 0;
	TOUCH_TRACE();

	mutex_lock(&ts->lock);
	/* search eof */
	for (i = 0; i < MAX_CMD_COUNTS; i++) {
		if (ts->cmd_data.cmd_lists[i][0] == NULL ||
				i == (MAX_CMD_COUNTS - 1)) {
			eof = i;
			break;
		} else {
			TOUCH_I("%s:%d:%s\n", ts->cmd_data.cmd_lists[i][0], i, ts->cmd_data.cmd_lists[i][1]);
		}
	}

	for (i = 0; i < MAX_CMD_COUNTS; i++) {
		if (list[i][0] == NULL) {
			break;
		} else {
			if ((eof + i) >= MAX_CMD_COUNTS) {
				TOUCH_I("cmd counts over\n");
				break;
			}
			/* target */
			ts->cmd_data.cmd_lists[eof + i][0] = target;
			/* cmd name */
			ts->cmd_data.cmd_lists[eof + i][1] = list[i][0];
			/* return value*/
			ts->cmd_data.cmd_lists[eof + i][2] = "";
			/* rw status*/
			ts->cmd_data.cmd_lists[eof + i][3] = "";
			/* owner */
			ts->cmd_data.cmd_lists[eof + i][4] = "";
			/* typedef */
			ts->cmd_data.cmd_lists[eof + i][5] = list[i][1];
		}
	}
	mutex_unlock(&ts->lock);
	return ret;
}

int cmd_set(struct device *dev, char *target, char *buf, char *rw_mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int i = 0;
	TOUCH_TRACE();

	for (i = 0; i < MAX_CMD_COUNTS; i++) {
		if (ts->cmd_data.cmd_lists[i][0] == NULL) {
			break;
		}
		if (!strcmp(target, ts->cmd_data.cmd_lists[i][0])) {
			if (!strcmp(ts->cmd_data.cmd_name, ts->cmd_data.cmd_lists[i][1])) {
				ts->cmd_data.cmd_lists[i][2] = kstrdup(buf, GFP_KERNEL);
				ts->cmd_data.cmd_lists[i][3] = kstrdup(rw_mode, GFP_KERNEL);
				ts->cmd_data.cmd_lists[i][4] = kstrdup(ts->cmd_data.owner, GFP_KERNEL);
				TOUCH_I("%s:%s:%s:%s:%s set\n",
						ts->cmd_data.cmd_lists[i][0],
						ts->cmd_data.cmd_lists[i][1],
						ts->cmd_data.cmd_lists[i][2],
						ts->cmd_data.cmd_lists[i][3],
						ts->cmd_data.cmd_lists[i][4]);
				break;
			}
		}
	}
	return ret;
}

int cmd_num_search(struct device *dev, char *target, char *cmd)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int detect = -1;
	int count = 0;
	int i = 0;

	for (i = 0; i < MAX_CMD_COUNTS; i++) {
		if (ts->cmd_data.cmd_lists[i][0] == NULL) {
			break;
		}
		if (!strcmp(target, ts->cmd_data.cmd_lists[i][0])) {
			if (!strcmp(cmd, ts->cmd_data.cmd_lists[i][1])) {
				detect = count;
				/*
				TOUCH_I("cmd:%s detected:%d\n", cmd, detect);
				*/
				break;
			}
			count++;
		}
	}
	return detect;
}

int sysfs_adder(struct device *dev, const char *name)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int i = 0;
	int ret = 0;
	if(ts->cmd_data.read_buf == NULL) {
		ts->cmd_data.read_buf = kzalloc(MAX_CMD_LENGTH, GFP_KERNEL);
		if (ts->cmd_data.read_buf == NULL) {
			TOUCH_E("failed to kzalloc read buf\n");
			ts->cmd_data.read_buf = NULL;
			return -EINVAL;
		}
	}
	for (i = 0; i < MAX_SYSFS_LENGTH; i++) {
		if (ts->cmd_data.sysfs_lists[i] == NULL) {
			snprintf(ts->cmd_data.read_buf,
					strlen(name) + 1,
					"%s", name);
			ts->cmd_data.sysfs_lists[i] = kstrdup(ts->cmd_data.read_buf, GFP_KERNEL);
			break;
		}
	}
	return ret;
}



int file_controller(struct device *dev, struct file *filp, char *read_buf, char *write_buf)
{
	int ret = 0;
	int read_ret = 0;
	int ret_status = 0;
	char *tok = NULL;
	char *sep_tok = "\n";
	struct file *filp_s;
	int fd = 0;
	struct touch_core_data *ts = to_touch_core(dev);
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
	mutex_unlock(&ts->lock);
	if (!strcmp(ts->cmd_data.rw_mode, "read")) {
		if (strstr(ts->cmd_data.cmd_fname, "touch_common_control")) {
			ret += snprintf(read_buf + ret, MAX_READ_BUF - ret, "%s\n", "Not support Node");
			TOUCH_I("Not Support Node\n");
			ret = -EINVAL;
			goto error;
		}

		if (ts->cmd_data.line_buf == NULL) {
			ts->cmd_data.line_buf = kzalloc(MAX_READ_BUF, GFP_KERNEL);
			if (ts->cmd_data.line_buf == NULL) {
				TOUCH_E("failed to kzalloc line buf\n");
				ret += snprintf(read_buf + ret, MAX_READ_BUF - ret, "%s\n", "alloc line buf fail");
				ret = -EINVAL;
				goto error;
			}
		}

		filp_s = filp_open(ts->cmd_data.cmd_fname, O_RDONLY, S_IRUSR|S_IWUSR);
		if (IS_ERR(filp_s)) {
			TOUCH_I("file open fail\n");
			ret += snprintf(read_buf + ret, MAX_READ_BUF - ret, "%s\n", "file open fail");
			goto error;
		} else {
			TOUCH_I("file open success\n");
		}

		do {
			ret_status = vfs_read(filp_s, ts->cmd_data.line_buf + read_ret, MAX_CMD_LENGTH, &filp->f_pos);
			if (ret_status < 0) {
				TOUCH_I("error: %d\n", ret_status);
				ret += snprintf(read_buf + ret, MAX_READ_BUF - ret, "read error : %d\n", ret_status);
				break;
			}
			read_ret += ret_status;
		} while (ret_status != 0);

		filp_close(filp_s, NULL);
		if (ts->cmd_data.line_buf != NULL) {
			ts->cmd_data.read_point = kstrdup(ts->cmd_data.line_buf, GFP_KERNEL);
			while (1) {
				tok = strsep(&ts->cmd_data.read_point, sep_tok);
				if (tok != NULL) {
					if (MAX_READ_BUF - ret < 0)  {
						break;
					} else {
						ret += snprintf(read_buf + ret, MAX_READ_BUF - ret, "%s\n",  tok);
					}
				} else {
					TOUCH_I("break\n");
					break;
				}
			}
		}
	} else 	if (!strcmp(ts->cmd_data.rw_mode, "write")) {
		fd = ksys_open(ts->cmd_data.cmd_fname, O_WRONLY, 0666);
		if (fd >= 0) {
			TOUCH_I("sysfs node find\n");
			if (ts->cmd_data.cmd_buf != NULL) {
				ksys_write(fd, write_buf, strlen(write_buf) - 1);
			}
			ksys_close(fd);
		} else {
			TOUCH_E("%s : cmd_fname is NULL, can not open FILE\n", __func__);
		}
	} else {
		TOUCH_I("Not support contorl mode\n");
	}

error:
	mutex_lock(&ts->lock);
	set_fs(old_fs);
	return ret;
}

unsigned long get_tracing_mark(void)
{
	static unsigned long __read_mostly tracing_mark_write_addr;

	if (unlikely(tracing_mark_write_addr == 0))
		tracing_mark_write_addr =
			kallsyms_lookup_name("tracing_mark_write");

	return tracing_mark_write_addr;
}
#endif

void touch_msleep(unsigned int msecs)
{
	if (msecs >= 20)
		msleep(msecs);
	else
		usleep_range(msecs * 1000, msecs * 1000);
}
EXPORT_SYMBOL(touch_msleep);

void touch_interrupt_control(struct device *dev, int on_off)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	if (on_off) {
		if (atomic_cmpxchg(&ts->state.irq_enable, 0, 1) == 0) {
			touch_enable_irq(ts->irq);

			if (ts->role.use_lpwg)
				touch_enable_irq_wake(ts->irq);
		}
	} else {
		if (atomic_cmpxchg(&ts->state.irq_enable, 1, 0) == 1) {
			if (ts->role.use_lpwg)
				touch_disable_irq_wake(ts->irq);

			touch_disable_irq(ts->irq);
		}
	}
}
EXPORT_SYMBOL(touch_interrupt_control);

int touch_snprintf(char *buf, int size, const char *fmt, ...)
{

	va_list args;
	int ret = 0;

	if(size <= 0) {
		TOUCH_E("size is negative!\n");
		return 0;
	}

	va_start(args, fmt);
	ret = vsnprintf(buf, (size_t)size, fmt, args);
	va_end(args);

	if (ret < 0) {
		TOUCH_E("snprintf error. change ret value(%d -> 0)\n", ret);
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(touch_snprintf);
