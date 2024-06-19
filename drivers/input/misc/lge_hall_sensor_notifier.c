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
#include <linux/lge_hall_sensor_notifier.h>

static BLOCKING_NOTIFIER_HEAD(lge_hall_sensor_notifier_list);

int lge_hall_sensor_state = -1;
EXPORT_SYMBOL(lge_hall_sensor_state);

int lge_hall_sensor_notifier_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&lge_hall_sensor_notifier_list, nb);
}
EXPORT_SYMBOL(lge_hall_sensor_notifier_register_client);


int lge_hall_sensor_notifier_unregister_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&lge_hall_sensor_notifier_list, nb);
}
EXPORT_SYMBOL(lge_hall_sensor_notifier_unregister_client);


int lge_hall_sensor_notifier_call_chain(unsigned long action, int state)
{
	struct lge_hall_sensor_notifier notifier_data;
	notifier_data.state = state;

	if(action == LGE_HALL_SENSOR_STATE_CHANGED)
		lge_hall_sensor_state = state;

	return blocking_notifier_call_chain(&lge_hall_sensor_notifier_list, action, &notifier_data);
}
