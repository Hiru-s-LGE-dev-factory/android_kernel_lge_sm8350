// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */
#define pr_fmt(fmt) "lguc-boost: " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/time.h>
#include <linux/sysfs.h>
#include <linux/pm_qos.h>

#include "sched.h"

static unsigned int cas_uclamp_value = 500;
static unsigned int input_uclamp_boost_ms = 1500;

#define cpu_boost_cas_attr_rw(_name)							\
static struct kobj_attribute _name##_attr =						\
__ATTR(_name, 0664, show_##_name, store_##_name)

#define show_one(file_name)										\
static ssize_t show_##file_name									\
(struct kobject *kobj, struct kobj_attribute *attr, char *buf)	\
{																\
	return scnprintf(buf, PAGE_SIZE, "%u\n", file_name);		\
}

#define store_one(file_name, min, max)							\
static ssize_t store_##file_name								\
(struct kobject *kobj, struct kobj_attribute *attr,				\
const char *buf, size_t count)									\
{																\
																\
	sscanf(buf, "%u", &file_name);								\
	if (file_name < min) file_name = min;						\
	else if (file_name > max) file_name = max;					\
	return count;												\
}

static u64 last_input_time;
#define MIN_INPUT_CAS_INTERVAL (150 * USEC_PER_MSEC)

static struct workqueue_struct *lguc_boost_wq;
static struct work_struct input_uclamp_boost;
static struct delayed_work input_uclamp_boost_rem;
static unsigned int cas_boost_status = 0;
static unsigned int cas_feature_enable = 1;

static void update_group_uclamp(unsigned int value)
{
	struct task_group *tg;

	rcu_read_lock();
	list_for_each_entry_rcu(tg, &task_groups, list) {
		if (tg->wtg.colocate) {
			set_uclamp_touch(tg, value);
			break;
		}
	}
	rcu_read_unlock();
}

#define store_one_cas(file_name)								\
static ssize_t store_##file_name								\
(struct kobject *kobj, struct kobj_attribute *attr,				\
const char *buf, size_t count)									\
{																\
	sscanf(buf, "%u", &file_name);								\
	if (cas_feature_enable) {									\
		if (cas_boost_status != 0) {						 	\
			update_group_uclamp(0);								\
		} else {												\
			update_group_uclamp(1);								\
		}														\
	}															\
	return count;												\
}

#define store_one_cas_enable(file_name)							\
static ssize_t store_##file_name								\
(struct kobject *kobj, struct kobj_attribute *attr,				\
const char *buf, size_t count)									\
{																\
	sscanf(buf, "%u", &file_name);								\
	if (cas_feature_enable == 0) {								\
		update_group_uclamp(0);									\
	} else {													\
		update_group_uclamp(1);									\
	}															\
	return count;												\
}

show_one(cas_uclamp_value);
store_one(cas_uclamp_value, 0, 1024);
cpu_boost_cas_attr_rw(cas_uclamp_value);

show_one(input_uclamp_boost_ms);
store_one(input_uclamp_boost_ms, 0, 5000);
cpu_boost_cas_attr_rw(input_uclamp_boost_ms);

show_one(cas_boost_status);
store_one_cas(cas_boost_status);
cpu_boost_cas_attr_rw(cas_boost_status);

show_one(cas_feature_enable);
store_one_cas_enable(cas_feature_enable);
cpu_boost_cas_attr_rw(cas_feature_enable);

static void do_input_uclamp_boost_rem(struct work_struct *work){
	if (cas_feature_enable) {
		if (cas_boost_status != 0) {
			update_group_uclamp(0);
		} else {
			update_group_uclamp(1);
		}
	}
}

static void do_input_uclamp_boost(struct work_struct *work){

	cancel_delayed_work_sync(&input_uclamp_boost_rem);

//	printk("%s, cas_boost_status:, %d", __func__, cas_boost_status);

	if (cas_feature_enable) {
		if (cas_boost_status == 0 || cas_boost_status == 2) {
			update_group_uclamp(cas_uclamp_value);
		} else if (cas_boost_status == 1) {
			update_group_uclamp(1);
		} else {
			update_group_uclamp(0);
		}
	}

	queue_delayed_work(lguc_boost_wq, &input_uclamp_boost_rem,
					msecs_to_jiffies(input_uclamp_boost_ms));
}

static void lgucboost_input_event(struct input_handle *handle,
		unsigned int type, unsigned int code, int value)
{
	u64 now;

	if (!cas_feature_enable)
		return;

	now = ktime_to_us(ktime_get());
	if (now - last_input_time < MIN_INPUT_CAS_INTERVAL)
		return;

	if (work_pending(&input_uclamp_boost))
		return;

	queue_work(lguc_boost_wq, &input_uclamp_boost);
	last_input_time = ktime_to_us(ktime_get());
}

static int lgucboost_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "lguc";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void lgucboost_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id lgucboost_ids[] = {
	/* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	/* touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};

static struct input_handler lgucboost_input_handler = {
	.event          = lgucboost_input_event,
	.connect        = lgucboost_input_connect,
	.disconnect     = lgucboost_input_disconnect,
	.name           = "lguc-boost",
	.id_table       = lgucboost_ids,
};

struct kobject *lguc_boost_kobj;
static int lguc_boost_init(void)
{
	int ret;

	lguc_boost_wq = alloc_workqueue("lgucboost_wq", WQ_HIGHPRI, 0);
	if (!lguc_boost_wq)
		return -EFAULT;

	INIT_WORK(&input_uclamp_boost, do_input_uclamp_boost);
	INIT_DELAYED_WORK(&input_uclamp_boost_rem, do_input_uclamp_boost_rem);

	lguc_boost_kobj = kobject_create_and_add("lguc_boost",
						&cpu_subsys.dev_root->kobj);
	if (!lguc_boost_kobj)
		pr_err("Failed to initialize sysfs node for cas_cpu_boost.\n");

    ret = sysfs_create_file(lguc_boost_kobj, &cas_boost_status_attr.attr);
    if (ret)
        pr_err("Failed to create cas_boost_status node: %d\n", ret);

    ret = sysfs_create_file(lguc_boost_kobj, &cas_feature_enable_attr.attr);
    if (ret)
        pr_err("Failed to create cas_feature_enable node: %d\n", ret);

	ret = sysfs_create_file(lguc_boost_kobj, &cas_uclamp_value_attr.attr);
	if (ret)
		pr_err("Failed to create cas_uclamp_value node: %d\n", ret);

	ret = sysfs_create_file(lguc_boost_kobj, &input_uclamp_boost_ms_attr.attr);
	if (ret)
		pr_err("Failed to create cas_uclamp_value node: %d\n", ret);

	ret = input_register_handler(&lgucboost_input_handler);
	if (ret)
		pr_err("Failed to register input handler: %d\n", ret);

	return 0;
}
late_initcall(lguc_boost_init);
