/*
 *   copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_LGE_HALL_SENSOR_NOTIFIER_H
#define __LINUX_LGE_HALL_SENSOR_NOTIFIER_H

#include <linux/notifier.h>

struct lge_hall_sensor_notifier {
	int state;
};

enum {
	LGE_HALL_SENSOR_STATE_CHANGED,
}; /* LGE_HALL_SENSOR_ACTION */

enum {
	LGE_HALL_SENSOR_OPEN,
	LGE_HALL_SENSOR_CLOSE,
}; /* LGE_HALL_SENSOR_STATE */

extern int lge_hall_sensor_state;

int lge_hall_sensor_notifier_register_client(struct notifier_block *nb);

int lge_hall_sensor_notifier_unregister_client(struct notifier_block *nb);

int lge_hall_sensor_notifier_call_chain(unsigned long val, int state);
#endif
