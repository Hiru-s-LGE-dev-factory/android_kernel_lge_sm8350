/*
 * touch_common.h
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

#ifndef LGE_TOUCH_COMMON_H
#define LGE_TOUCH_COMMON_H

void touch_msleep(unsigned int msecs);
void touch_interrupt_control(struct device *dev, int on_off);
int touch_snprintf(char *buf, int size, const char *fmt, ...);
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
int sysfs_adder(struct device *dev, const char *name);
int read_sysfs_lists(struct device *dev, const struct attribute_group *list);
int cmds_adder(struct device *dev, char *target, char *(*list)[2]);
int cmd_set(struct device *dev, char *target, char *buf, char *rw_mode);
int cmd_num_search(struct device *dev, char *target, char *cmd);
int common_control(struct device *dev, char *target);
char *cmd_get_value(struct device *dev, char *target, char *cmd);
int file_controller(struct device *dev, struct file *filp, char *read_buf, char *write_buf);
#define MAX_CMD_COUNTS 50
#define MAX_CMD_LENGTH 2048
#define MAX_SYSFS_LENGTH 100
#define MAX_READ_BUF 10000

enum {
	DISABLE,
	SYS_PR,
	SYS_IV,
};

enum {
	CMD_MODE,
	CTRL_MODE,
	NA_MODE,
};

enum {
	WRITE_MODE,
	READ_MODE,
	CLEAR_MODE,
	RW_MODE_NA,
};

enum {
	CMD_LIST,
	SYSFS_LIST,
	SYSTRACE,
	TOUCH_DEX_MODE,
};

/* cmd lists for core driver */
/* cmd name, typeDef */
static char *core_cmd_lists[MAX_CMD_COUNTS][2] = {
	[0] = {"cmd_list", "[n/a]"},
	[1] = {"sysfs_list", "[n/a]"},
	[2] = {"systrace", "[enable:1/0]"},
	{NULL},
};

struct common_cmd_data {
	/* target
	 * cmd name
	 * return value
	 * rw_status
	 * owner
	 * typedef
	 */
	char *cmd_lists[MAX_CMD_COUNTS][6];
	char *sysfs_lists[MAX_SYSFS_LENGTH];
	char *owner;
	char *rw_mode;
	char *cmd_fname;
	char *cmd_name;
	char *cmd_buf;
	char *read_buf;
	char *line_buf;
	char *read_point;
};
#endif

#endif /* LGE_TOUCH_CORE_H */
