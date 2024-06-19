/*
 * touch_core_sysfs.c
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

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>
#if IS_ENABLED(CONFIG_SECURE_TOUCH)
#include <touch_i2c.h>
#endif
#define TOUCH_SHOW(ret, buf, fmt, args...) \
	(ret += snprintf(buf + ret, PAGE_SIZE - ret, fmt, ##args))

static char ime_str[3][8] = {"OFF", "ON", "SWYPE"};
static char incoming_call_str[7][15] = {"IDLE", "RINGING", "OFFHOOK", "CDMA_RINGING", "CDMA_OFFHOOK", "LTE_RINGING", "LTE_OFFHOOK"};
static char mfts_str[5][8] = {"NONE", "FOLDER", "FLAT", "CURVED", "DS_FLAT"};

int ignore_compared_event;

static ssize_t show_platform_data(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int i;
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_SHOW(ret, buf, "=== Platform Data ===\n");
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "reset_pin", ts->reset_pin);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "int_pin", ts->int_pin);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "maker_id_pin", ts->maker_id_pin);

	TOUCH_SHOW(ret, buf, "caps:\n");
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "max_x", ts->caps.max_x);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "max_y", ts->caps.max_y);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "max_pressure",
			ts->caps.max_pressure);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "max_width_major",
			ts->caps.max_width_major);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "max_width_minor",
			ts->caps.max_width_minor);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "max_orientation",
			ts->caps.max_orientation);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "max_id", ts->caps.max_id);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "hw_reset_delay",
			ts->caps.hw_reset_delay);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "sw_reset_delay",
			ts->caps.sw_reset_delay);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "flexible_display_info_update_frequency",
			ts->caps.flexible_display_info_update_frequency);

	TOUCH_SHOW(ret, buf, "role:\n");
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "use_lpwg", ts->role.use_lpwg);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "use_lpwg_test",
			ts->role.use_lpwg_test);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "hide_coordinate",
			ts->role.hide_coordinate);

	TOUCH_SHOW(ret, buf, "power:\n");
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "vcl-gpio", ts->vcl_pin);
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "vdd-gpio", ts->vdd_pin);

	TOUCH_SHOW(ret, buf, "firmware:\n");
	TOUCH_SHOW(ret, buf, "\t%25s = %d\n", "def_fwcnt", ts->def_fwcnt);
	for (i = 0; i < ts->def_fwcnt; i++)
		TOUCH_SHOW(ret, buf, "\t%25s [%d:%s]\n", "def_fwpath",
				i, ts->def_fwpath[i]);

	TOUCH_SHOW(ret, buf, "panel_spec:\n");
	TOUCH_SHOW(ret, buf, "\t%25s [%s]\n", "panel_spec", ts->panel_spec);
	TOUCH_SHOW(ret, buf, "\t%25s [%s]\n", "panel_spec_mfts_folder", ts->panel_spec_mfts);
	return ret;
}

static ssize_t store_upgrade(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.screen == SCREEN_OFF) {
		TOUCH_E("LCD OFF state. please turn on the display\n");
		return count;
	}

	if (sscanf(buf, "%255s", &ts->test_fwpath[0]) <= 0)
		return count;

	ts->force_fwup = 1;
	queue_delayed_work(ts->wq, &ts->upgrade_work, 0);

	return count;
}

static ssize_t show_upgrade(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.screen == SCREEN_OFF) {
		TOUCH_E("LCD OFF state. please turn on the display\n");
		ret = snprintf(buf, PAGE_SIZE, "LCD Off state. please turn on the display\n");
		return ret;
	}

	ts->test_fwpath[0] = '\0';
	ts->force_fwup = 1;

	queue_delayed_work(ts->wq, &ts->upgrade_work, 0);

	return 0;
}

static ssize_t show_lpwg_data(struct device *dev, char *buf)
{
	int i = 0, ret = 0;
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	if (!ts->driver->lpwg && !ts->driver->display_area_status)
		return ret;

	for (i = 0; i < MAX_TAP_CNT; i++) {
		if (ts->lpwg_coord.code[i].x == -1 && ts->lpwg_coord.code[i].y == -1)
			break;
		ret += touch_snprintf(buf + ret, PAGE_SIZE - ret, "%d %d\n",
				ts->lpwg_coord.code[i].x, ts->lpwg_coord.code[i].y);
	}
	ts->lpwg_coord.count = 0;
	memset(ts->lpwg_coord.code, 0, sizeof(struct point) * MAX_TAP_CNT);

	return ret;
}

static ssize_t store_lpwg_data(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int reply = 0;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &reply) < 0)
		return count;

	TOUCH_I("%s : reply = %d\n", __func__, reply);

	atomic_set(&ts->state.uevent, UEVENT_IDLE);
	complete(&ts->uevent_complete);
	pm_relax(ts->dev);

	return count;
}

static ssize_t store_lpwg_notify(struct device *dev,
		const char *buf, size_t count)
{

	struct touch_core_data *ts = to_touch_core(dev);
	struct module_data *md;
	int code = 0;
	int param[4] = {0, };
	int boot_mode = TOUCH_NORMAL_BOOT;
	int i = 0;

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
	case TOUCH_MINIOS_AAT:
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		if (!ts->mfts_lpwg)
			return count;
		else
			break;
	case TOUCH_CHARGER_MODE:
	case TOUCH_LAF_MODE:
	case TOUCH_RECOVERY_MODE:
		return count;
	default:
		TOUCH_E("invalid boot_mode(%d)\n", boot_mode);
		return count;
	}

	if (sscanf(buf, "%d %d %d %d %d",
			&code, &param[0], &param[1], &param[2], &param[3]) <= 0)
		return count;

	/* only below code notify
		3 active_area
		4 knockcode tap count
		8 knockcode double tap check
		9 update_all
	*/
	if (code == 1 || code == 2 || code == 5 ||
		code == 6 || code == 7)
		return count;

	if (ts->driver->lpwg) {
		mutex_lock(&ts->lock);
		ts->driver->lpwg(ts->dev, code, param);
		mutex_unlock(&ts->lock);

		if (plist != NULL) {
			for (i = 0; i < 10; i++) {
				if (plist->sub_dev[i] != NULL) {
					md = to_module(plist->sub_dev[i]);
					if (!md) {
						TOUCH_E("module data is not located\n");
						return count;
					}
					md->m_driver.lpwg(md->dev, code, param);
				}
			}
		}
	}

	return count;
}

static ssize_t store_display_area_status(struct device *dev, const char *buf, size_t count)
{

	struct touch_core_data *ts = to_touch_core(dev);
	struct module_data *md;
	int param[5] = {0, };
	int boot_mode = TOUCH_NORMAL_BOOT;
	int i = 0;

	TOUCH_TRACE();

	boot_mode = touch_check_boot_mode(dev);

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
	case TOUCH_MINIOS_AAT:
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		if (!ts->mfts_lpwg)
			return count;
		else
			break;
	case TOUCH_CHARGER_MODE:
	case TOUCH_LAF_MODE:
	case TOUCH_RECOVERY_MODE:
		return count;
	default:
		TOUCH_E("invalid boot_mode(%d)\n", boot_mode);
		return count;
	}

	/* Display Area ID, Screen status, Lpwg mode, Proximity status, Cover status */
	if (sscanf(buf, "%d %d %d %d %d",
			&param[0], &param[1], &param[2], &param[3], &param[4]) <= 0)
		return count;


	if (ts->driver->display_area_status) {
		mutex_lock(&ts->lock);
		ts->driver->display_area_status(ts->dev, param);
		mutex_unlock(&ts->lock);

		if (plist != NULL) {
			for (i = 0; i < 10; i++) {
				if (plist->sub_dev[i] != NULL) {
					md = to_module(plist->sub_dev[i]);
					if (!md) {
						TOUCH_E("module data is not located\n");
						return count;
					}
					md->m_driver.display_area_status(md->dev, param);
				}
			}
		}
	}

	return count;
}

static ssize_t store_lpwg_function_info(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct module_data *md;
	int i = 0, j = 0, size_of_lpwg_function_info = 0;
	char *buf_copy = NULL;
	char *sptr = NULL, *sep_result = NULL, *delimiters = "\n";
	int id = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0, x_offset = 0, y_offset = 0;
	int lpwg_function = 0, gesture = 0;
	bool enable = false;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	buf_copy = devm_kzalloc(dev, count + 1, GFP_KERNEL);
	if (!buf_copy) {
		TOUCH_E("Failed to allocate memory for buf_copy\n");
		goto out;
	}

	strncpy(buf_copy, buf, count);
	sptr = buf_copy;

	sep_result = strsep(&sptr, delimiters);
	if (sep_result == NULL) {
		TOUCH_E("%s: failed parsing", __func__);
		goto out;
	}
	TOUCH_D(TRACE, "sep_result : %s", sep_result);

	if (sscanf(sep_result, "%d", &size_of_lpwg_function_info) <= 0)
		goto out;

	if ((size_of_lpwg_function_info <= 0) ||
		(size_of_lpwg_function_info > (DISPLAY_AREA_ID_MAX * LPWG_FUNCTION_MAX * GESTURE_MAX))) {
		TOUCH_E("%s: wrong size_of_lpwg_function_info = %d\n", __func__, size_of_lpwg_function_info);
		goto out;
	}

	for (i = 0; i < size_of_lpwg_function_info ; i++) {
		sep_result = strsep(&sptr, delimiters);
		if (sep_result == NULL) {
			TOUCH_E("%s: failed parsing(%d)", __func__, i);
			goto out;
		}
		TOUCH_D(TRACE, "sep_result(%d) : %s", i, sep_result);

		/* displayAreaId, lpwgFunctionInfo, lpwgGesture, enable, x1, y1, x2, y2, xOffset, yOffset  */
		if (sscanf(sep_result, "%d %d %d %d %d %d %d %d %d %d",
			&id, &lpwg_function, &gesture, &enable, &x1, &y1, &x2, &y2, &x_offset, &y_offset) <= 0)
			goto out;

		if (id >= 0 && id < DISPLAY_AREA_ID_MAX) {
			ts->lpwg_function[id].display_area_id = id;
			ts->lpwg_function[id].function = lpwg_function;
			ts->lpwg_function[id].gesture = gesture;
			ts->lpwg_function[id].enable = enable;
			ts->lpwg_function[id].function_area.area.left_top.x =  x1;
			ts->lpwg_function[id].function_area.area.left_top.y =  y1;
			ts->lpwg_function[id].function_area.area.right_bottom.x =  x2;
			ts->lpwg_function[id].function_area.area.right_bottom.y =  y2;
			ts->lpwg_function[id].function_area.x_offset = x_offset;
			ts->lpwg_function[id].function_area.y_offset = y_offset;

			TOUCH_D(TRACE, "%s: display_area_id(%d), function(%d), gesture(%d), enable(%d), start(%d, %d), end(%d, %d), x_offset(%d), y_offset(%d)\n", __func__,
					 id, lpwg_function, gesture, enable, x1, y1, x2, y2, x_offset, y_offset);

			if (ts->driver->lpwg_function_info) {
				ts->driver->lpwg_function_info(ts->dev, id);

				if (plist != NULL) {
					for (j = 0; j < 10; j++) {
						if (plist->sub_dev[j] != NULL) {
							md = to_module(plist->sub_dev[j]);
							if (!md) {
								TOUCH_E("module data is not located\n");
								goto out;
							}
							md->m_driver.lpwg_function_info(md->dev, id);
						}
					}
				}
			}
		} else {
			TOUCH_E("%s: wrong display_area_id=%d\n", __func__, id);
		}
	}
out:
	devm_kfree(dev, buf_copy);
	mutex_unlock(&ts->lock);

	return count;
}
static ssize_t show_lockscreen_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = atomic_read(&ts->state.lockscreen);

	ret = snprintf(buf, PAGE_SIZE, "%s : %s(%d)\n", __func__,
		value ? "LOCK" : "UNLOCK", value);

	return ret;
}

static ssize_t store_lockscreen_state(struct device *dev,
		const char *buf, size_t count)
{
	int value = 0;
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	if (value == LOCKSCREEN_UNLOCK || value == LOCKSCREEN_LOCK) {
		atomic_set(&ts->state.lockscreen, value);
		TOUCH_I("%s : %s(%d)\n", __func__,
				value ? "LOCK" : "UNLOCK", value);
	} else {
		TOUCH_I("%s : Unknown %d\n", __func__, value);
	}

	return count;
}

static ssize_t show_ime_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = atomic_read(&ts->state.ime);

	ret = snprintf(buf, PAGE_SIZE, "%s : %s(%d)\n", __func__,
			ime_str[value], value);

	return ret;
}

static ssize_t store_ime_state(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	if (value >= IME_OFF && value <= IME_SWYPE) {
		if (atomic_read(&ts->state.ime) == value)
			return count;

		atomic_set(&ts->state.ime, value);
		ret = touch_blocking_notifier_call(NOTIFY_IME_STATE,
			&ts->state.ime);
		TOUCH_I("%s : %s(%d), ret = %d\n",
			__func__, ime_str[value], value, ret);
	} else {
		TOUCH_I("%s : Unknown %d\n", __func__, value);
	}

	return count;
}

static ssize_t show_film_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = atomic_read(&ts->state.film);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", value);

	return ret;
}

static ssize_t store_film_state(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	if (value != 0 && value != 1) {
		TOUCH_I("%s : Unknown %d\n", __func__, value);
		return count;
	}

	if (atomic_read(&ts->state.film) == value)
		return count;

	atomic_set(&ts->state.film, value);
	ret = touch_blocking_notifier_call(NOTIFY_FILM_STATE,
		&ts->state.film);
	TOUCH_I("%s : %d, ret = %d\n",
			__func__, value, ret);

	return count;
}


#if IS_ENABLED(CONFIG_LGE_TOUCH_PEN)
static ssize_t show_activepen_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = atomic_read(&ts->state.active_pen);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", value);

	return ret;
}

static ssize_t store_activepen_state(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	if (value != 0 && value != 1) {
		TOUCH_I("%s : Unknown %d\n", __func__, value);
		return count;
	}

	if (atomic_read(&ts->state.active_pen) == value)
		return count;

	atomic_set(&ts->state.active_pen, value);
	ret = touch_blocking_notifier_call(NOTIFY_ACTIVE_PEN_STATE,
		&ts->state.active_pen);

	TOUCH_I("%s : %d, ret = %d\n",
			__func__, value, ret);

	return count;
}
#endif
static ssize_t show_incoming_call_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = atomic_read(&ts->state.incoming_call);

	ret = snprintf(buf, PAGE_SIZE, "%s : %s(%d)\n", __func__,
		incoming_call_str[value], value);

	return ret;
}

static ssize_t store_incoming_call_state(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	if (value >= INCOMING_CALL_IDLE && value <= INCOMING_CALL_LTE_OFFHOOK) {
		if (atomic_read(&ts->state.incoming_call) == value)
			return count;

		atomic_set(&ts->state.incoming_call, value);

		ret = touch_blocking_notifier_call(NOTIFY_CALL_STATE,
					&ts->state.incoming_call);

		TOUCH_I("%s : %s(%d)\n", __func__,
				incoming_call_str[value], value);
	} else {
		TOUCH_I("%s : Unknown %d\n", __func__, value);
	}

	return count;
}

static ssize_t show_version_info(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);
	ret = ts->driver->get(dev, CMD_VERSION, NULL, buf);
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_atcmd_version_info(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);
	ret = ts->driver->get(dev, CMD_ATCMD_VERSION, NULL, buf);
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_mfts_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = atomic_read(&ts->state.mfts);

	ret = snprintf(buf, PAGE_SIZE, "%s : %s(%d)\n", __func__,
			mfts_str[value], value);

	return ret;
}

static ssize_t store_mfts_state(struct device *dev,
		const char *buf, size_t count)
{
	int value = 0;
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	if (value >= MFTS_NONE && value <= MFTS_DS_FLAT) {
		atomic_set(&ts->state.mfts, value);
		TOUCH_I("%s : %s(%d)\n", __func__, mfts_str[value], value);
	} else {
		TOUCH_I("%s : Unknown %d\n", __func__, value);
	}

	return count;
}

static ssize_t show_mfts_lpwg(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret = snprintf(buf, PAGE_SIZE, "%d\n", ts->role.use_lpwg_test);

	return ret;
}

static ssize_t store_mfts_lpwg(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	ts->mfts_lpwg = value;
	TOUCH_I("mfts_lpwg:%d\n", ts->mfts_lpwg);

	return count;
}


static ssize_t show_sp_link_touch_off(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "sp link touch status %d\n",
			atomic_read(&ts->state.sp_link));

	return ret;
}

static ssize_t store_sp_link_touch_off(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	if (kstrtos32(buf, 10, &value) < 0) {
		TOUCH_I("Invalid Value\n");
		return count;
	}

	atomic_set(&ts->state.sp_link, value);

	if (atomic_read(&ts->state.sp_link) == SP_CONNECT) {
		touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
		TOUCH_I("SP Mirroring Connected\n");
	} else if (atomic_read(&ts->state.sp_link) == SP_DISCONNECT) {
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
		mod_delayed_work(ts->wq, &ts->init_work, 0);
		TOUCH_I("SP Mirroring Disconnected\n");
	}

	return count;
}

static ssize_t show_debug_tool_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	value = atomic_read(&ts->state.debug_tool);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", value);

	return ret;
}

static ssize_t store_debug_tool_state(struct device *dev,
		const char *buf, size_t count)
{
	int data = 0;
	int ret = 0;
	struct touch_core_data *ts = to_touch_core(dev);

	if (kstrtos32(buf, 10, &data) < 0)
		return count;

	if (data >= DEBUG_TOOL_DISABLE && data <= DEBUG_TOOL_ENABLE) {
		atomic_set(&ts->state.debug_tool, data);

		ret = touch_blocking_notifier_call(NOTIFY_DEBUG_TOOL,
				&ts->state.debug_tool);

		TOUCH_I("%s : %s, ret = %d\n",
				__func__,
				(data == DEBUG_TOOL_ENABLE) ?
				"Debug Tool Enabled" : "Debug Tool Disabled",
				ret);
	} else {
		TOUCH_I("%s : Unknown debug tool set value %d\n",
			__func__, data);
	}

	return count;
}

static ssize_t show_debug_option_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	value = atomic_read(&ts->state.debug_option_mask);

	ret = snprintf(buf, PAGE_SIZE, "%d\n", value);

	return ret;
}

static ssize_t store_debug_option_state(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int new_mask = 0;
	int old_mask = 0;
	int data[2] = {0, 0};
	int ret = 0;

	old_mask = atomic_read(&ts->state.debug_option_mask);

	if (kstrtos32(buf, 10, &new_mask) < 0)
		return count;

	if (new_mask >= DEBUG_OPTION_DISABLE
		&& new_mask <= DEBUG_OPTION_ALL) {
		atomic_set(&ts->state.debug_option_mask, new_mask);
		TOUCH_I("%s : Input masking value = %d\n",
			__func__, new_mask);
	} else {
		TOUCH_I("%s : Unknown debug option set value %d\n",
			__func__, new_mask);
	}

	data[0] = new_mask ^ old_mask; //Changed mask
	data[1] = data[0] & new_mask; //Enable(!=0) or Disable(==0)

	ret = touch_blocking_notifier_call(NOTIFY_DEBUG_OPTION, (void *)&data);
	TOUCH_I("%s : chg_mask = 0x%08X, enable = 0x%08X, ret = %d\n",
			__func__, data[0], data[1], ret);

	return count;
}

static ssize_t show_swipe_enable(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret += snprintf(buf + ret, PAGE_SIZE, "%d %d\n",
			PAY_TYPE_SWIPE_U, ts->swipe[SWIPE_U].enable);
	ret += snprintf(buf + ret, PAGE_SIZE, "%d %d\n",
			PAY_TYPE_SWIPE_L, ts->swipe[SWIPE_L2].enable);
	ret += snprintf(buf + ret, PAGE_SIZE, "%d %d\n",
			PAY_TYPE_SWIPE_R, ts->swipe[SWIPE_R2].enable);

	TOUCH_I("%s: ts->swipe[SWIPE_U].enable = %d\n", __func__,
			ts->swipe[SWIPE_U].enable);
	TOUCH_I("%s: ts->swipe[SWIPE_L2].enable = %d\n", __func__,
			ts->swipe[SWIPE_L2].enable);
	TOUCH_I("%s: ts->swipe[SWIPE_R2].enable = %d\n", __func__,
			ts->swipe[SWIPE_R2].enable);

	return ret;
}

static ssize_t store_swipe_enable(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int type = PAY_TYPE_DISABLE;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &type) < 0)
		return count;

	TOUCH_I("%s: type = %d\n", __func__, type);

	switch (type) {
	case PAY_TYPE_DISABLE:
		ts->swipe[SWIPE_U].enable = false;
		ts->swipe[SWIPE_L2].enable = false;
		ts->swipe[SWIPE_R2].enable = false;
		break;
	case PAY_TYPE_SWIPE_U:
		ts->swipe[SWIPE_U].enable = true;
		ts->swipe[SWIPE_L2].enable = false;
		ts->swipe[SWIPE_R2].enable = false;
		break;
	case PAY_TYPE_SWIPE_L:
		ts->swipe[SWIPE_U].enable = false;
		ts->swipe[SWIPE_L2].enable = true;
		ts->swipe[SWIPE_R2].enable = false;
		break;
	case PAY_TYPE_SWIPE_R:
		ts->swipe[SWIPE_U].enable = false;
		ts->swipe[SWIPE_L2].enable = false;
		ts->swipe[SWIPE_R2].enable = true;
		break;
	default:
		TOUCH_E("%s : invalid type(%d)\n", __func__, type);
		return count;
	}

	return count;
}

static ssize_t store_swipe_pay_area(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int type = PAY_TYPE_DISABLE;
	int offset_y = 0;
	int start_x = 0;
	int start_y = 0;
	int width = 0;
	int height = 0;
	int end_x = 0;
	int end_y = 0;
	int mode = SWIPE_DEFAULT;

	if (sscanf(buf, "%d %d %d %d %d %d", &type, &offset_y, &start_x,
				&start_y, &width, &height) <= 0)
		return count;

	TOUCH_I("%s: type = %d, offset_y = %d, start_x = %d, start_y = %d, width = %d, height = %d\n",
			__func__, type, offset_y,
			start_x, start_y, width, height);

	switch (type) {
	case PAY_TYPE_SWIPE_L:
		mode = SWIPE_L2;
		break;
	case PAY_TYPE_SWIPE_R:
		mode = SWIPE_R2;
		break;
	default:
		TOUCH_E("%s : type is no pay_area(%d)\n", __func__, type);
		break;
	}

	if (type == PAY_TYPE_SWIPE_L || type == PAY_TYPE_SWIPE_R) {
		if (offset_y == 0 && start_x == 0 && start_y == 0
				&& width == 0 && height == 0) {
			TOUCH_I("%s : Values are not changed.\n", __func__);
		} else {
			end_x = start_x + width - 1;
			end_y = start_y + height - 1;

			ts->swipe[mode].start_area.x1 = start_x;
			ts->swipe[mode].start_area.y1 = start_y;
			ts->swipe[mode].start_area.x2 = end_x;
			ts->swipe[mode].start_area.y2 = end_y;
		}
	}

	mutex_lock(&ts->lock);
	ts->driver->swipe_enable(dev, true);
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_swipe_tool(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = (ts->swipe[SWIPE_R].enable) && (ts->swipe[SWIPE_L].enable);
	int ret = 0;

	TOUCH_TRACE();

	ret += snprintf(buf + ret, PAGE_SIZE, "%d\n", value);
	TOUCH_I("%s: value = %d\n", __func__, value);

	return ret;
}

static ssize_t store_swipe_tool(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int enable = 0;
	int offset_y = 0;
	int start_x = 0;
	int start_y = 0;
	int width = 0;
	int height = 0;
	int end_x = 0;
	int end_y = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%d %d %d %d %d %d", &enable, &offset_y, &start_x,
				&start_y, &width, &height) <= 0)
		return count;

	TOUCH_I("%s: enable = %d, offset_y = %d, start_x = %d, start_y = %d, width = %d, height = %d\n",
			__func__, enable, offset_y,
			start_x, start_y, width, height);

	if ((enable > 1) || (enable < 0)) {
		TOUCH_E("invalid enable(%d)\n", enable);
		return count;
	}

	if (enable) {
		if (offset_y == 0 && start_x == 0 && start_y == 0
				&& width == 0 && height == 0) {
			TOUCH_I("%s : Values are not changed.\n", __func__);
		} else {
			end_x = start_x + width - 1;
			end_y = start_y + height - 1;

			ts->swipe[SWIPE_R].area.x1 = start_x;
			ts->swipe[SWIPE_R].area.y1 = start_y;
			ts->swipe[SWIPE_R].area.x2 = end_x;
			ts->swipe[SWIPE_R].area.y2 = end_y;
			ts->swipe[SWIPE_R].start_area.x1 = start_x;
			ts->swipe[SWIPE_R].start_area.y1 = start_y;
			ts->swipe[SWIPE_R].start_area.x2 = end_x;
			ts->swipe[SWIPE_R].start_area.y2 = end_y;

			ts->swipe[SWIPE_L].area.x1 = start_x;
			ts->swipe[SWIPE_L].area.y1 = start_y;
			ts->swipe[SWIPE_L].area.x2 = end_x;
			ts->swipe[SWIPE_L].area.y2 = end_y;
			ts->swipe[SWIPE_L].start_area.x1 = start_x;
			ts->swipe[SWIPE_L].start_area.y1 = start_y;
			ts->swipe[SWIPE_L].start_area.x2 = end_x;
			ts->swipe[SWIPE_L].start_area.y2 = end_y;
		}
	}

	ts->swipe[SWIPE_R].enable = (bool)enable;
	ts->swipe[SWIPE_L].enable = (bool)enable;

	mutex_lock(&ts->lock);
	ts->driver->swipe_enable(dev, true);
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_app_data(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int i = 0;
	u32 test_jig_tmp = ts->perf_test.jig_size;

	TOUCH_TRACE();

	for (i = 0; i < 3; i++) {
		if (i == 2) {
			// Show JIG Size
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d %d %d %d\n",
					i, test_jig_tmp, test_jig_tmp, test_jig_tmp);
		} else {
			// Show APP Size (Home & Contacts)
			ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d %s %d %d\n",
					ts->app_data[i].app, ts->app_data[i].version,
					ts->app_data[i].icon_size, ts->app_data[i].touch_slop);
		}
		if (ts->app_data[i].icon_size != 0) {
			TOUCH_I("%s : Read %s App data (Icon_Size = %d, Touch_Slop = %d, Version = %s)\n",
					__func__, (ts->app_data[i].app == APP_HOME ? "Home" : ((ts->app_data[i].app == APP_CONTACTS) ? "Contacts" : "N/A")),
					ts->app_data[i].icon_size, ts->app_data[i].touch_slop, ts->app_data[i].version);
		}

	}

	return ret;
}

static ssize_t store_app_data(struct device *dev,
		const char *buf, size_t count)
{

	struct touch_core_data *ts = to_touch_core(dev);
	struct app_info app_data_buf;

	TOUCH_TRACE();

	if (sscanf(buf, "%d %10s %d %d",
			&app_data_buf.app, app_data_buf.version,
			&app_data_buf.icon_size, &app_data_buf.touch_slop) <= 0)
		return count;

	if (app_data_buf.app >= APP_HOME && app_data_buf.app <= APP_CONTACTS) {
		memcpy(&ts->app_data[app_data_buf.app], &app_data_buf, sizeof(app_data_buf));
		TOUCH_I("%s : Write %s App data (Icon_Size = %d, Touch_Slop = %d, Version = %s)\n",
				__func__, (app_data_buf.app == APP_HOME ? "Home" : ((app_data_buf.app == APP_CONTACTS) ? "Contacts" : "N/A")),
				app_data_buf.icon_size, app_data_buf.touch_slop, app_data_buf.version);
	}

	return count;
}

static ssize_t show_click_test(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	struct touch_data tdata;
	int cnt = 100 / ts->perf_test.delay;	/* click 100ms */
	int i = 0;

	TOUCH_TRACE();

	if (!ts->perf_test.enable)
		return ret;

	if (cnt < 2) {
		TOUCH_E("invalid cnt(%d)\n", cnt);
		return -EINVAL;
	}

	tdata.id = 0;
	tdata.x = ts->perf_test.click_x;
	tdata.y = ts->perf_test.click_y;
	tdata.pressure = ts->perf_test.pressure;
	tdata.width_major = ts->perf_test.width;
	tdata.width_minor = ts->perf_test.width;
	tdata.orientation = 0;

	TOUCH_I("%s: start (%4d, %4d)\n", __func__, tdata.x, tdata.y);
	mutex_lock(&ts->lock);
	touch_report_all_event(ts);

	for (i = 0; i < cnt; i++) {
		input_mt_slot(ts->input, tdata.id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
		input_report_key(ts->input, BTN_TOUCH, 1);
		input_report_key(ts->input, BTN_TOOL_FINGER, 1);
		input_report_abs(ts->input, ABS_MT_TRACKING_ID, tdata.id);
		input_report_abs(ts->input, ABS_MT_POSITION_X, tdata.x);
		input_report_abs(ts->input, ABS_MT_POSITION_Y, tdata.y);
		input_report_abs(ts->input, ABS_MT_PRESSURE, tdata.pressure);
		input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR,
				tdata.width_major);
		input_report_abs(ts->input, ABS_MT_WIDTH_MINOR,
				tdata.width_minor);
		input_report_abs(ts->input, ABS_MT_ORIENTATION,
				tdata.orientation);
		input_sync(ts->input);

		touch_msleep(ts->perf_test.delay);
	}

	input_mt_slot(ts->input, tdata.id);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	input_report_key(ts->input, BTN_TOUCH, 0);
	input_report_key(ts->input, BTN_TOOL_FINGER, 0);
	input_sync(ts->input);

	touch_report_all_event(ts);
	mutex_unlock(&ts->lock);
	TOUCH_I("%s: end\n", __func__);

	return ret;
}

static ssize_t show_v_drag_test(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	struct touch_data *tdata = NULL;
	int cnt = 800 / ts->perf_test.delay;	/* drag 800ms */
	u16 start_y = ts->perf_test.v_drag_start_y;
	u16 end_y = ts->perf_test.v_drag_end_y;
	u16 y_diff = start_y - end_y;
	int i = 0;

	TOUCH_TRACE();

	if (!ts->perf_test.enable)
		return ret;

	if (cnt < 2) {
		TOUCH_E("invalid cnt(%d)\n", cnt);
		return -EINVAL;
	}

	tdata = kcalloc(cnt, sizeof(*tdata), GFP_KERNEL);
	if (tdata == NULL) {
		TOUCH_E("failed to kcalloc tdata\n");
		return -ENOMEM;
	}

	for (i = 0; i < cnt; i++) {
		tdata[i].id = 0;
		tdata[i].x = ts->perf_test.v_drag_x;
		tdata[i].y = start_y - ((y_diff * i) / (cnt - 1));
		tdata[i].pressure = ts->perf_test.pressure;
		tdata[i].width_major = ts->perf_test.width;
		tdata[i].width_minor = ts->perf_test.width;
		tdata[i].orientation = 0;
	}

	TOUCH_I("%s: start (y: %4d -> %4d)\n", __func__, start_y, end_y);
	mutex_lock(&ts->lock);
	touch_report_all_event(ts);

	for (i = 0; i < cnt; i++) {
		input_mt_slot(ts->input, tdata[i].id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
		input_report_key(ts->input, BTN_TOUCH, 1);
		input_report_key(ts->input, BTN_TOOL_FINGER, 1);
		input_report_abs(ts->input, ABS_MT_TRACKING_ID, tdata[i].id);
		input_report_abs(ts->input, ABS_MT_POSITION_X, tdata[i].x);
		input_report_abs(ts->input, ABS_MT_POSITION_Y, tdata[i].y);
		input_report_abs(ts->input, ABS_MT_PRESSURE, tdata[i].pressure);
		input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR,
				tdata[i].width_major);
		input_report_abs(ts->input, ABS_MT_WIDTH_MINOR,
				tdata[i].width_minor);
		input_report_abs(ts->input, ABS_MT_ORIENTATION,
				tdata[i].orientation);
		input_sync(ts->input);

		touch_msleep(ts->perf_test.delay);
	}

	input_mt_slot(ts->input, tdata[i - 1].id);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	input_report_key(ts->input, BTN_TOUCH, 0);
	input_report_key(ts->input, BTN_TOOL_FINGER, 0);
	input_sync(ts->input);

	touch_report_all_event(ts);
	mutex_unlock(&ts->lock);
	kfree(tdata);
	TOUCH_I("%s: end\n", __func__);

	return ret;
}

static ssize_t show_h_drag_test(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	struct touch_data *tdata = NULL;
	int cnt = 800 / ts->perf_test.delay;	/* drag 800ms */
	u16 start_x = ts->perf_test.h_drag_start_x;
	u16 end_x = ts->perf_test.h_drag_end_x;
	u16 x_diff = start_x - end_x;
	int i = 0;

	TOUCH_TRACE();

	if (!ts->perf_test.enable)
		return ret;

	if (cnt < 2) {
		TOUCH_E("invalid cnt(%d)\n", cnt);
		return -EINVAL;
	}

	tdata = kcalloc(cnt, sizeof(*tdata), GFP_KERNEL);
	if (tdata == NULL) {
		TOUCH_E("failed to kcalloc tdata\n");
		return -ENOMEM;
	}

	for (i = 0; i < cnt; i++) {
		tdata[i].id = 0;
		tdata[i].x = start_x - ((x_diff * i) / (cnt - 1));
		tdata[i].y = ts->perf_test.h_drag_y;
		tdata[i].pressure = ts->perf_test.pressure;
		tdata[i].width_major = ts->perf_test.width;
		tdata[i].width_minor = ts->perf_test.width;
		tdata[i].orientation = 0;
	}

	TOUCH_I("%s: start (x: %4d -> %4d)\n", __func__, start_x, end_x);
	mutex_lock(&ts->lock);
	touch_report_all_event(ts);

	for (i = 0; i < cnt; i++) {
		input_mt_slot(ts->input, tdata[i].id);
		input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, true);
		input_report_key(ts->input, BTN_TOUCH, 1);
		input_report_key(ts->input, BTN_TOOL_FINGER, 1);
		input_report_abs(ts->input, ABS_MT_TRACKING_ID, tdata[i].id);
		input_report_abs(ts->input, ABS_MT_POSITION_X, tdata[i].x);
		input_report_abs(ts->input, ABS_MT_POSITION_Y, tdata[i].y);
		input_report_abs(ts->input, ABS_MT_PRESSURE, tdata[i].pressure);
		input_report_abs(ts->input, ABS_MT_WIDTH_MAJOR,
				tdata[i].width_major);
		input_report_abs(ts->input, ABS_MT_WIDTH_MINOR,
				tdata[i].width_minor);
		input_report_abs(ts->input, ABS_MT_ORIENTATION,
				tdata[i].orientation);
		input_sync(ts->input);

		touch_msleep(ts->perf_test.delay);
	}

	input_mt_slot(ts->input, tdata[i - 1].id);
	input_mt_report_slot_state(ts->input, MT_TOOL_FINGER, false);
	input_report_key(ts->input, BTN_TOUCH, 0);
	input_report_key(ts->input, BTN_TOOL_FINGER, 0);
	input_sync(ts->input);

	touch_report_all_event(ts);
	mutex_unlock(&ts->lock);
	kfree(tdata);
	TOUCH_I("%s: end\n", __func__);

	return ret;
}

#if IS_ENABLED(CONFIG_SECURE_TOUCH)
static ssize_t show_secure_touch_devinfo(struct device *dev,
				char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_I("devInfo: %s\n", ts->touch_ic_name);

	ret = snprintf(buf + ret, PAGE_SIZE, ts->touch_ic_name);
	ret += snprintf(buf + ret, PAGE_SIZE - ret, ".%d", ts->bus_type);

	return ret;
}

static ssize_t show_secure_touch_enable(struct device *dev,
				char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	return scnprintf(buf, PAGE_SIZE, "%d",
			atomic_read(&ts->st_enabled));
}
/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t store_secure_touch_enable(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	unsigned long value;
	int err = 0;
	int cnt = 0;
	char *token = NULL;
	char *devinfo, *devinfo_orig = NULL;
	char *sep = ",";

	if (count > 2)
		return -EINVAL;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!ts->st_initialized)
		return -EIO;

	err = count;

	TOUCH_I("%s : value %lu\n", __func__, value);

	switch (value) {
	case 0:
		if (atomic_read(&ts->st_enabled) == 0)
			break;

		touch_i2c_set(ts);
		atomic_set(&ts->st_enabled, 0);
		secure_touch_notify(ts);
		complete(&ts->st_irq_processed);

		devinfo_orig = kstrdup(ts->touch_ic_name, GFP_KERNEL);
		if (devinfo_orig != NULL) {
			devinfo = devinfo_orig;
			/* Due to various touch IC, each initialization path must be different */
			token = strsep(&devinfo, sep);
			TOUCH_I("Device info : %s\n", token);
			if (token != NULL) {
				/* Synaptics int pin is not auto cleared by Firmware */
				if (!strcmp(token, "synaptics")) {
					touch_report_all_event(ts);
					while (gpio_get_value(ts->int_pin) == 0 && cnt < 10) {
						touch_msleep(10);
						cnt++;
					}
					TOUCH_I("delay time : %d\n", cnt*10);
					if  (cnt >= 10) {
						TOUCH_I("need init\n");
						touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
						mod_delayed_work(ts->wq, &ts->init_work, 0);
					}
				} else if (!strcmp(token, "lge")) {
					touch_report_all_event(ts);
					touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
					mod_delayed_work(ts->wq, &ts->init_work, 0);
				}
			}
			kfree(devinfo_orig);
		}
		complete(&ts->st_powerdown);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
		complete(&ts->st_irq_received);
#endif

		break;
	case 1:
		/* TO DO */
		if (atomic_read(&ts->st_enabled)) {
			err = -EBUSY;
			break;
		}

		touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
		touch_msleep(50);
		synchronize_irq(ts->irq);
		mutex_lock(&ts->lock);

		if (touch_i2c_get(ts) < 0) {
			TOUCH_E("touch_i2c_get failed\n");
			err = -EIO;
			break;
		}

#if IS_ENABLED(CONFIG_LGE_TOUCH_PEN)
		ts->driver->stop_pen_sensing(dev, true);
#endif
		reinit_completion(&ts->st_powerdown);
		reinit_completion(&ts->st_irq_processed);
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
		reinit_completion(&ts->st_irq_received);
#endif
		atomic_set(&ts->st_enabled, 1);
		atomic_set(&ts->st_pending_irqs,  0);
		mutex_unlock(&ts->lock);
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
		break;
	default:
		TOUCH_I("%s : Unknown value %lu\n",
				__func__, value);
		err = -EINVAL;
		break;
	}
	return err;
}

/*
 * This function returns whether there are pending interrupts, or
 * other error conditions that need to be signaled to the userspace library,
 * according tot he following logic:
 * - st_enabled is 0 if secure touch is not enabled, returning -EBADF
 * - st_pending_irqs is -1 to signal that secure touch is in being stopped,
 *   returning -EINVAL
 * - st_pending_irqs is 1 to signal that there is a pending irq, returning
 *   the value "1" to the sysfs read operation
 * - st_pending_irqs is 0 (only remaining case left) if the pending interrupt
 *   has been processed, so the interrupt handler can be allowed to continue.
 */
static ssize_t show_secure_touch(struct device *dev,
		char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int val = 0;

	if (atomic_read(&ts->st_enabled) == 0)
		return -EBADF;

	if (atomic_cmpxchg(&ts->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;

	if (atomic_cmpxchg(&ts->st_pending_irqs, 1, 0) == 1) {
		val = 1;
	} else {
		TOUCH_I("complete\n");
		complete(&ts->st_irq_processed);
	}
	return scnprintf(buf, PAGE_SIZE, "%u", val);

}
#endif

static ssize_t show_ignore_event(struct device *dev, char *buf)
{
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = ignore_compared_event;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", value);

	return ret;
}

static ssize_t store_ignore_event(struct device *dev,
		const char *buf, size_t count)
{
	int ret = 0;

	TOUCH_TRACE();


	if (kstrtos32(buf, 10, &ret) < 0)
		return count;

	if (ret >= 0 && ret <= 1) {
		ignore_compared_event = ret;
		TOUCH_I("%s. (%d)\n",
				(ignore_compared_event == 0 ? "Skip same event data" : "Do not skip same event data"),
				ignore_compared_event);
	} else {
		TOUCH_I("%s : invalied value = %d\n", __func__, ret);
	}

	return count;
}

/*
static ssize_t store_module_touch_test(struct device *dev,
		const char *buf, size_t count)
{
	struct module_data *md;
	char result[100] = {'0',};
	unsigned long value;
	int i = 0;

	if(kstrtoul(buf, 10, &value) < 0)
		return count;

	if (plist != NULL) {
		for (i = 0; i < 10; i++) {
			if (plist->sub_dev[i] != NULL) {
				md = to_module(plist->sub_dev[i]);
				if (!md) {
					TOUCH_E("module data is not located\n");
					return count;
				}

				md->m_driver.func(dev, value, result);

				TOUCH_I("%s : value %lu\n", __func__, value);
				TOUCH_I("%s : result [%s]\n", __func__, result);
			} else {
				TOUCH_I("sub_dev[%d] is NULL\n", i);
			}
		}
	} else
		TOUCH_I("plist is NULL, do nothing\n");

	return count;
}
static ssize_t show_module_touch_test(struct device *dev,
		char *buf)
{
	int val = 0;
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}

static ssize_t store_md_test_from_md(struct device *dev,
		const char *buf, size_t count)
{

	unsigned long value;

	if(kstrtoul(buf, 10, &value) < 0)
		return count;

	TOUCH_I("store_md_test_from_mod! value : %d\n", value);

	return count;
}

static ssize_t show_md_test_from_md(struct device *dev,
		char *buf)
{
	int val = 0;
	TOUCH_I("show_md_test_from_mod! val : %d\n", val);
	return scnprintf(buf, PAGE_SIZE, "%u", val);
}
*/

#if IS_ENABLED(CONFIG_LGE_TOUCH_DEX)
static ssize_t show_touch_dex_mode(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret += snprintf(buf + ret, PAGE_SIZE, "%d\n",
			ts->touch_dex.enable);
	TOUCH_I("%s: enable = %d, active_area(%d,%d)(%d,%d)\n",
			__func__, ts->touch_dex.enable, ts->touch_dex.area.x1, ts->touch_dex.area.y1,
			ts->touch_dex.area.x2, ts->touch_dex.area.y2);
	return ret;
}

static ssize_t store_touch_dex_mode(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int enable = 0;
	int offset_y = 0;
	int start_x = 0;
	int start_y = 0;
	int width = 0;
	int height = 0;
	int end_x = 0;
	int end_y = 0;
	int fd = 0;
	char *fname = NULL;

	TOUCH_TRACE();
	if (sscanf(buf, "%d %d %d %d %d %d", &enable, &offset_y, &start_x,
				&start_y, &width, &height) <= 0)
		return count;

	TOUCH_I("%s: enable = %d, offset_y = %d, start_x = %d, start_y = %d, width = %d, height = %d\n",
			__func__, enable, offset_y, start_x, start_y, width, height);

	if (offset_y == 1) { // if offset_y = 0, we use dex mode for main panel, if offset_y = 1, we use dex mode for sub panel
		mm_segment_t old_fs = get_fs();

		set_fs(KERNEL_DS);

		/* redirect path to sub touch dex mode */
		fname = "/sys/devices/virtual/input/lge_sub_touch/touch_dex_mode";
		fd = ksys_open(fname, O_WRONLY, 0666);
		if (fd >= 0) {
			TOUCH_I("sub dex touch detected: redirect infos\n");
			ksys_write(fd, buf, strlen(buf));
			ksys_close(fd);
			set_fs(old_fs);
			return count;
		} else {
			TOUCH_E("%s : fname is NULL, can not open FILE\n", __func__);
			set_fs(old_fs);
		}
	}

	if ((enable > 1) || (enable < 0)) {
		TOUCH_E("invalid enable(%d)\n", enable);
		return count;
	}

	if (ts->touch_dex.enable != enable) {
		mutex_lock(&ts->lock);
		touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
		touch_report_all_event(ts);
		atomic_set(&ts->state.dex_mode, enable);
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
		mutex_unlock(&ts->lock);
	}

	if (enable) {
		ts->touch_dex.enable = enable;
		end_x = start_x + width - 1;
		end_y = start_y + height - 1;
		ts->touch_dex.area.x1 = start_x;
		ts->touch_dex.area.y1 = start_y;
		ts->touch_dex.area.x2 = end_x;
		ts->touch_dex.area.y2 = end_y;
	} else {
		ts->touch_dex.enable = false;
	}


	return count;
}
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
/* 1.CMD Mode
 *   return cmds list and datas(if set read cmd)
 *
 * 2.CTRL Mode
 *   return node datas(if set file node read cmd)
 */
static char *log_buf;
static ssize_t read_touch_common_control(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t off, size_t count)
{
	struct touch_core_data *ts = container_of(kobj, struct touch_core_data, kobj);
	static int ret = 0;
	int i = 0;
	int detect = RW_MODE_NA;
	TOUCH_TRACE();
	mutex_lock(&ts->lock);
	if (log_buf == NULL) {
		log_buf = vzalloc(sizeof(char) * (MAX_READ_BUF));
		if (log_buf == NULL) {
			TOUCH_E("log_buf virtual memory alloc error\n");
			count = 0;
			goto free;
		}
	}
	if (off == 0) {
		if (log_buf == NULL) {
			TOUCH_E("virtual memory alloc error\n");
			count = 0;
			goto free;
		}
		ret = 0;
		TOUCH_I("rw_mode:%s fname:%s\n", ts->cmd_data.rw_mode, ts->cmd_data.cmd_fname);
		if ((ts->cmd_data.cmd_fname != NULL)
				&& (ts->cmd_data.rw_mode != NULL)) {
			ret = file_controller(ts->dev, filp, log_buf, NULL);
		} else {
			for (i = 0; i < MAX_CMD_COUNTS; i++) {
				if (ts->cmd_data.cmd_lists[i][0] == NULL) {
					TOUCH_I("cmd lists end\n");
					break;
				}
				if (!strcmp(ts->cmd_data.cmd_lists[i][3], "read")) {
					if (detect == READ_MODE)
						ret += snprintf(log_buf + ret, MAX_READ_BUF - ret, "\n");
					ret += snprintf(log_buf + ret, MAX_READ_BUF - ret, "%s",
							ts->cmd_data.cmd_lists[i][2]);
					detect = READ_MODE;
				}
				/* debug log */
				TOUCH_I("%s:%s:%s:%s:%s\n", ts->cmd_data.cmd_lists[i][0],
						ts->cmd_data.cmd_lists[i][1],
						ts->cmd_data.cmd_lists[i][2],
						ts->cmd_data.cmd_lists[i][3],
						ts->cmd_data.cmd_lists[i][4]);
			}
		}
	}
free:
	if (ret < count)
		count = ret;
	ret = ret - count;

	if ((off < MAX_READ_BUF) && (off + count <= MAX_READ_BUF)) {
		memcpy(buf, &log_buf[off], count);
	} else {
		TOUCH_I("%s size error offset[%d] size[%d]\n", __func__, (int)off, (int)count);
		count = 0;
	}
	/* memory free */
	if ((int)count == 0) {
		if (ts->cmd_data.rw_mode != NULL) {
			kfree(ts->cmd_data.rw_mode);
			ts->cmd_data.rw_mode = NULL;
		}
		if (ts->cmd_data.cmd_fname != NULL) {
			kfree(ts->cmd_data.cmd_fname);
			ts->cmd_data.cmd_fname = NULL;
		}
		if (ts->cmd_data.line_buf != NULL) {
			kfree(ts->cmd_data.line_buf);
			ts->cmd_data.line_buf = NULL;
		}
		if (ts->cmd_data.read_point != NULL) {
			kfree(ts->cmd_data.read_point);
			ts->cmd_data.read_point = NULL;
		}
		if (log_buf != NULL) {
			vfree(log_buf);
			log_buf = NULL;
		}
	}
	mutex_unlock(&ts->lock);
	return count;
}

/* CMD Mode
 * Send Cmd and Data to Functions
 * 1. write
 * <Mode:Target:RW_MODE:Cmd,Datas>
 * echo core:write:SYSTRACE.1 > touch_common_control
 * 2. read
 * <Mode:Target:RW_MODE:Cmd>
 * echo core:read:SYSTRACE > touch_common_control
 * cat touch_common_control
 *
 * Control Mode
 * Redirect data to exist Node
 * 1. write
 * <Mode:Node:RW_MODE:Data>
 * echo /sys/devices/virtual/input/lge_touch/reg_ctrl:write:write.0xc03.0x4 > touch_common_control
 * 2. read
 * <Mode:Node:RW_MODE>
 * echo /sys/devices/virtual/input/lge_touch_version:read
 * cat touch_common_control
 * expand counts available
 */
static ssize_t write_touch_common_control(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t off, size_t count)
{
	struct touch_core_data *ts = container_of(kobj, struct touch_core_data, kobj);
	char *control_buf = NULL;
	char *cmd_tmp = NULL;
	char *path_target = NULL;
	char *path_tmp = NULL;
	char *tok = NULL;
	char *sep_node = ":";
	char *sep_cmd = ",";
	char *target = NULL;
	int i = 0;
	int offset = 0;
	int mode = NA_MODE;
	int rw_mode = RW_MODE_NA;
	TOUCH_TRACE();
	if (count > MAX_CMD_LENGTH) {
		TOUCH_I("invalid count:%d\n", count);
		return -EINVAL;
	}

	mutex_lock(&ts->lock);
	/* read buf */
	control_buf = devm_kzalloc(ts->dev, count + 1, GFP_KERNEL);
	if (!control_buf) {
		TOUCH_I("fail to allocate mem\n");
		mutex_unlock(&ts->lock);
		devm_kfree(ts->dev, control_buf);
		return -EINVAL;
	}
	if (sscanf(buf, "%s", control_buf) <= 0) {
		mutex_unlock(&ts->lock);
		return -EINVAL;
	}

	TOUCH_I("control_buf: %s\n", control_buf);
	/* parse data*/
	if (control_buf != NULL) {
		path_target = kstrdup(control_buf, GFP_KERNEL);
		path_tmp = path_target;
		tok = strsep(&path_tmp, sep_node);
		devm_kfree(ts->dev, control_buf);

		if (tok != NULL) {
			if (ts->cmd_data.rw_mode != NULL) {
				kfree(ts->cmd_data.rw_mode);
				ts->cmd_data.rw_mode = NULL;
			}
			ts->cmd_data.rw_mode = kstrdup(tok, GFP_KERNEL);
			TOUCH_I("rw_mode:%s\n", ts->cmd_data.rw_mode);

			if (!strcmp(ts->cmd_data.rw_mode, "write")) {
				rw_mode = WRITE_MODE;
			} else if (!strcmp(ts->cmd_data.rw_mode, "read")) {
				rw_mode = READ_MODE;
			} else if (!strcmp(ts->cmd_data.rw_mode, "clear")) {
				rw_mode = CLEAR_MODE;
			}
			if (rw_mode != RW_MODE_NA) {
				TOUCH_I("%s Mode\n", tok);
				while (1) {
					tok = strsep(&path_tmp, sep_node);
					if (tok != NULL) {
						/*
						TOUCH_I("%d : %s\n", i, tok);
						*/
						if (i == 0) {
							if (strstr(tok, "/sys/")) {
							   mode = CTRL_MODE;
							} else {
							   mode = CMD_MODE;
							}

							if (mode == CMD_MODE) {
								ts->cmd_data.cmd_name = tok;
							} else if (mode == CTRL_MODE
									&& rw_mode != CLEAR_MODE) {
								if (ts->cmd_data.cmd_fname != NULL) {
									kfree(ts->cmd_data.cmd_fname);
									ts->cmd_data.cmd_fname = NULL;
								}
								ts->cmd_data.cmd_fname = kstrdup(tok, GFP_KERNEL);
								if (strstr(ts->cmd_data.cmd_fname, "touch_common_control")) {
									TOUCH_I("Not Supported Node: %s\n", ts->cmd_data.cmd_fname);
									ts->cmd_data.cmd_fname = NULL;
									break;
								}
								TOUCH_I("fname:%s\n", ts->cmd_data.cmd_fname);
							}

						} else if (i == 1) {
							cmd_tmp = tok;
							if (rw_mode == WRITE_MODE || rw_mode == READ_MODE) {
								TOUCH_I("try to parse cmd %s\n", cmd_tmp);
								ts->cmd_data.cmd_buf = kzalloc(MAX_CMD_LENGTH, GFP_KERNEL);
								while (1) {
									tok = strsep(&cmd_tmp, sep_cmd);
									if (tok != NULL) {
											offset += snprintf(ts->cmd_data.cmd_buf + offset, MAX_CMD_LENGTH - offset, "%s ",  tok);
									} else {
										break;
									}
								}
							} else if (rw_mode == CLEAR_MODE) {
								TOUCH_I("Clear Mode not suport cmd buf\n");
								break;
							} else {
								TOUCH_I("Not suport mode:%s\n", ts->cmd_data.rw_mode);
								break;
							}
						} else {
							TOUCH_I("Not support: Count over\n");
							break;
						}
						i++;
					} else {
						TOUCH_I("Parse End\n");
						break;
					}
				}
			} else {
				TOUCH_I("%s Not support Mode\n", tok);
			}
		}
		if (mode != NA_MODE) {
			if (mode == CMD_MODE) {
				TOUCH_I("cmd mode\n");
				if (ts->cmd_data.cmd_name != NULL && rw_mode != RW_MODE_NA) {
					ts->cmd_data.owner = kzalloc(MAX_CMD_COUNTS, GFP_KERNEL);
					snprintf(ts->cmd_data.owner, MAX_CMD_COUNTS, "%s(%d).%s", current->comm, task_pid_nr(current), ts->cmd_data.rw_mode);
					target = cmd_get_value(ts->dev, "target", ts->cmd_data.cmd_name);
					TOUCH_I("target:%s cmd:%s data:%s\n", target, ts->cmd_data.cmd_name, ts->cmd_data.cmd_buf);
					if (target != NULL) {
						if (common_control(ts->dev, target) < 0) {
							TOUCH_I("cmd control failed\n");
						}
					}
				}
			} else if (mode == CTRL_MODE) {
				TOUCH_I("ctrl mode\n");
				if (rw_mode != RW_MODE_NA && ts->cmd_data.cmd_fname != NULL) {
					TOUCH_I("rw:%s fname:%s data:%s\n", ts->cmd_data.rw_mode, ts->cmd_data.cmd_fname,
							ts->cmd_data.cmd_buf);
					if (rw_mode == WRITE_MODE) {
						file_controller(ts->dev, NULL, NULL, ts->cmd_data.cmd_buf);
					} else if (rw_mode == READ_MODE) {
						/*
						TOUCH_I("rw:%s fname:%s data:%s\n", ts->cmd_data.rw_mode, ts->cmd_data.cmd_fname,
								ts->cmd_data.cmd_buf);
						*/
					} else {
						TOUCH_I("check rw_mode:%s\n", ts->cmd_data.rw_mode);
					}
					if (ts->cmd_data.cmd_buf != NULL) {
						kfree(ts->cmd_data.cmd_buf);
						ts->cmd_data.cmd_buf = NULL;
					}
				}
			} else {
				TOUCH_I("NA mode\n");
			}
		}

		kfree(path_target);
		path_target = NULL;
		path_tmp = NULL;
	}
	mutex_unlock(&ts->lock);
	return count;
}
#endif
static ssize_t show_rotation(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;
	int ret = 0;

	TOUCH_TRACE();

	value = ts->rotation;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", value);

	return ret;
}

static ssize_t store_rotation(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	TOUCH_TRACE();

	if (kstrtos32(buf, 10, &value) < 0)
		return count;

	ts->rotation = value * 90;

	TOUCH_I("%s : %d\n", __func__, ts->rotation);

	if (ts->driver->rotation) {
		mutex_lock(&ts->lock);
		ts->driver->rotation(ts->dev);
		mutex_unlock(&ts->lock);
	}

	return count;
}

char *grip_area_str[GRIP_AREA_NUM] = {
	"PORTRAIT_A",
	"PORTRAIT_B",
	"PORTRAIT_C",
	"LANDSCAPE_VERTICAL_A",
	"LANDSCAPE_VERTICAL_B",
	"LANDSCAPE_VERTICAL_C",
	"LANDSCAPE_HORIZONTAL_A",
	"LANDSCAPE_HORIZONTAL_B",
	"IGNORE_LG_PAY",
	"IGNORE_SIDE_PANEL",
	"IGNORE_QMEMO_PANEL",
	"IGNORE_DS_TOOL",
};
EXPORT_SYMBOL(grip_area_str);

int get_grip_area_order(char *str)
{
	int i = 0;

	for (i = 0; i < GRIP_AREA_NUM; i++) {
		if (!strcmp(grip_area_str[i], str))
			return i;
	}

	return -EINVAL;
}

static ssize_t store_grip_area(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int i = 0;
	char *sep_result = NULL;
	char *buf_copy = NULL, *buf_sep = NULL;
	int temp = 0;
	int order = 0;

	TOUCH_TRACE();

	buf_copy = devm_kzalloc(dev, count + 1, GFP_KERNEL);
	if (!buf_copy) {
		TOUCH_E("Failed to allocate memory for buf_copy\n");
		return count;
	}
	strncpy(buf_copy, buf, count);
	buf_sep = buf_copy;

	for (i = 0; i < GRIP_AREA_NUM * 5; i++) {
		sep_result = strsep(&buf_sep, " ");
		if (!sep_result)
			break;

		sscanf(sep_result, "%d", &temp);

		switch (i % 5) {
		case 0:
			if (!sep_result[0]) //find the end of buf. " " is the string generated by HAL.
				break;

			order = get_grip_area_order(sep_result);
			if (order < 0) {
				TOUCH_E("Failed to get grip_area_order:%s\n", sep_result);
				goto error;
			}
			break;
		case 1:
			ts->grip_area[order].left_top.x = temp;
			break;
		case 2:
			ts->grip_area[order].left_top.y = temp;
			break;
		case 3:
			ts->grip_area[order].right_bottom.x = temp;
			break;
		case 4:
			ts->grip_area[order].right_bottom.y = temp;
			TOUCH_I("%s : %s (%d, %d, %d, %d)\n",
					__func__,
					grip_area_str[order],
					ts->grip_area[order].left_top.x,
					ts->grip_area[order].left_top.y,
					ts->grip_area[order].right_bottom.x,
					ts->grip_area[order].right_bottom.y);
			break;
		default:
			break;
		}
	}

	ts->have_grip_area = true;

	if (ts->driver->grip_area) {
		mutex_lock(&ts->lock);
		ts->driver->grip_area(ts->dev);
		mutex_unlock(&ts->lock);
	}

error:
	devm_kfree(dev, buf_copy);

	return count;
}

#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
static ssize_t read_app_fw_upgrade(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t off, size_t count)
{
	struct touch_core_data *ts = container_of(kobj, struct touch_core_data, kobj);
	int ret = 0;
	char *upgrade_status = NULL;

	TOUCH_TRACE();

	TOUCH_I("%s: ts->app_fw_upgrade.status = %d\n",
			__func__, ts->app_fw_upgrade.status);

	switch (ts->app_fw_upgrade.status) {
		case APP_FW_UPGRADE_IDLE:
			upgrade_status = "IDLE";
			break;
		case APP_FW_UPGRADE_GET_DATA:
			ts->app_fw_upgrade.status = APP_FW_UPGRADE_FLASHING;
			ts->force_fwup = 1;
			queue_delayed_work(ts->wq, &ts->app_upgrade_work, 0);
		case APP_FW_UPGRADE_FLASHING:
			upgrade_status = "FLASHING";
			break;
		default:
			TOUCH_E("unknown status\n");
			upgrade_status = "UNKNOWN";
			break;
	}

	ret = snprintf(buf, count + 1, upgrade_status + off);

	if (ret != 0) {
		if (ret > count)
			ret = count;
		TOUCH_I("ret:%d count:%d %s\n", ret, count, buf);
	}

	return ret;
}

static ssize_t write_app_fw_upgrade(struct file *filp,
		struct kobject *kobj, struct bin_attribute *bin_attr,
		char *buf, loff_t off, size_t count)
{
	struct touch_core_data *ts = container_of(kobj, struct touch_core_data, kobj);
	int ret = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	TOUCH_I("%s: ts->app_fw_upgrade.status = %d\n",
			__func__, ts->app_fw_upgrade.status);

	switch (ts->app_fw_upgrade.status) {
	case APP_FW_UPGRADE_IDLE:
		ts->app_fw_upgrade.status = APP_FW_UPGRADE_GET_DATA;
		ts->app_fw_upgrade.max_data_size = 400 * 1024;
		ts->app_fw_upgrade.offset = 0;
		ts->app_fw_upgrade.data = kzalloc(ts->app_fw_upgrade.max_data_size, GFP_KERNEL);
		TOUCH_I("%s: ts->app_fw_upgrade.max_data_size = %d, ts->app_fw_upgrade.offset = %d, ts->app_fw_upgrade.data = %p\n",
				__func__,
				(int)ts->app_fw_upgrade.max_data_size,
				(int)ts->app_fw_upgrade.offset,
				ts->app_fw_upgrade.data);
	case APP_FW_UPGRADE_GET_DATA:
		if ((ts->app_fw_upgrade.offset + count) < ts->app_fw_upgrade.max_data_size) {
			TOUCH_I("%s: ts->app_fw_upgrade.offset(before) = %d\n",
					__func__, (int)ts->app_fw_upgrade.offset);
			memcpy(ts->app_fw_upgrade.data + ts->app_fw_upgrade.offset, buf, count);
			ts->app_fw_upgrade.offset += count;
			TOUCH_I("%s: ts->app_fw_upgrade.offset(after) = %d\n",
					__func__, (int)ts->app_fw_upgrade.offset);
			ret = count;
		} else {
			TOUCH_E("data is full.\n");
		}
		break;
	case APP_FW_UPGRADE_FLASHING:
		TOUCH_E("flashing...\n");
		break;
	default:
		TOUCH_E("unknown status\n");
		break;
	}

	mutex_unlock(&ts->lock);

	return ret;
}
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
static ssize_t show_hidden_coordi(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	ret += snprintf(buf + ret, PAGE_SIZE, "%s: hidden_coordi(%s)\n", __func__,
			ts->role.hidden_coordi ? "enable" : "disable", ts->role.hidden_coordi);

	TOUCH_I("%s: hidden_coordi(%s)\n", __func__, ts->role.hidden_coordi ? "enable" : "disable");

	return ret;
}
static ssize_t store_hidden_coordi(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	TOUCH_TRACE();
	if (sscanf(buf, "%d", &value) <= 0)
		return count;

	if (value == 0)
		ts->role.hidden_coordi = false;
	else if (value == 1)
		ts->role.hidden_coordi = true;

	TOUCH_I("%s: hidden_coordi(%s)\n", __func__, ts->role.hidden_coordi ?
			"enable" : "disable", ts->role.hidden_coordi);

	return count;
}
#endif
static TOUCH_ATTR(platform_data, show_platform_data, NULL);
static TOUCH_ATTR(fw_upgrade, show_upgrade, store_upgrade);
static TOUCH_ATTR(lpwg_data, show_lpwg_data, store_lpwg_data);
static TOUCH_ATTR(lpwg_notify, NULL, store_lpwg_notify);
static TOUCH_ATTR(display_area_status, NULL, store_display_area_status);
static TOUCH_ATTR(lpwg_function_info, NULL, store_lpwg_function_info);
static TOUCH_ATTR(keyguard, show_lockscreen_state, store_lockscreen_state);
static TOUCH_ATTR(ime_status, show_ime_state, store_ime_state);
static TOUCH_ATTR(film_status, show_film_state, store_film_state);
#if IS_ENABLED(CONFIG_LGE_TOUCH_PEN)
static TOUCH_ATTR(active_pen_status, show_activepen_state, store_activepen_state);
#endif
static TOUCH_ATTR(incoming_call, show_incoming_call_state, store_incoming_call_state);
static TOUCH_ATTR(firmware, show_version_info, NULL);
static TOUCH_ATTR(version, show_version_info, NULL);
static TOUCH_ATTR(testmode_ver, show_atcmd_version_info, NULL);
static TOUCH_ATTR(mfts, show_mfts_state, store_mfts_state);
static TOUCH_ATTR(mfts_lpwg, show_mfts_lpwg, store_mfts_lpwg);
static TOUCH_ATTR(sp_link_touch_off, show_sp_link_touch_off, store_sp_link_touch_off);
static TOUCH_ATTR(debug_tool, show_debug_tool_state, store_debug_tool_state);
static TOUCH_ATTR(debug_option, show_debug_option_state, store_debug_option_state);
static TOUCH_ATTR(swipe_enable, show_swipe_enable, store_swipe_enable);
static TOUCH_ATTR(swipe_pay_area, NULL, store_swipe_pay_area);
static TOUCH_ATTR(swipe_tool, show_swipe_tool, store_swipe_tool);
static TOUCH_ATTR(app_data, show_app_data, store_app_data);
static TOUCH_ATTR(click_test, show_click_test, NULL);
static TOUCH_ATTR(v_drag_test, show_v_drag_test, NULL);
static TOUCH_ATTR(h_drag_test, show_h_drag_test, NULL);
#if IS_ENABLED(CONFIG_SECURE_TOUCH)
static TOUCH_ATTR(secure_touch_enable, show_secure_touch_enable, store_secure_touch_enable);
static TOUCH_ATTR(secure_touch, show_secure_touch, NULL);
static TOUCH_ATTR(secure_touch_devinfo, show_secure_touch_devinfo, NULL);
#endif
static TOUCH_ATTR(ignore_event, show_ignore_event, store_ignore_event);
#if IS_ENABLED(CONFIG_LGE_TOUCH_DEX)
static TOUCH_ATTR(touch_dex_mode, show_touch_dex_mode, store_touch_dex_mode);
#endif
static TOUCH_ATTR(rotation, show_rotation, store_rotation);
static TOUCH_ATTR(grip_area, NULL, store_grip_area);
#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
static TOUCH_ATTR(hidden_coordi, show_hidden_coordi, store_hidden_coordi);
#endif


/*
static TOUCH_ATTR(module_test, show_module_touch_test, store_module_touch_test);
static TOUCH_MODULE_ATTR(module_test_from_md, show_md_test_from_md, store_md_test_from_md);
*/
#define TOUCH_BIN_ATTR(_name, _read, _write, _size)		\
		struct bin_attribute touch_bin_attr_##_name	\
		= __BIN_ATTR(_name, 0644, _read, _write, _size)
#define APP_FW_UPGRADE_BUF	(400 * 1024)
#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
static TOUCH_BIN_ATTR(app_fw_upgrade, read_app_fw_upgrade, write_app_fw_upgrade, APP_FW_UPGRADE_BUF);
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
static TOUCH_BIN_ATTR(touch_common_control, read_touch_common_control, write_touch_common_control, MAX_READ_BUF);
#endif

static struct attribute *touch_attribute_list[] = {
	&touch_attr_platform_data.attr,
	&touch_attr_fw_upgrade.attr,
	&touch_attr_lpwg_data.attr,
	&touch_attr_lpwg_notify.attr,
	&touch_attr_display_area_status.attr,
	&touch_attr_lpwg_function_info.attr,
	&touch_attr_keyguard.attr,
	&touch_attr_ime_status.attr,
	&touch_attr_film_status.attr,
#if IS_ENABLED(CONFIG_LGE_TOUCH_PEN)
	&touch_attr_active_pen_status.attr,
#endif
	&touch_attr_incoming_call.attr,
	&touch_attr_firmware.attr,
	&touch_attr_version.attr,
	&touch_attr_testmode_ver.attr,
	&touch_attr_mfts.attr,
	&touch_attr_mfts_lpwg.attr,
	&touch_attr_sp_link_touch_off.attr,
	&touch_attr_debug_tool.attr,
	&touch_attr_debug_option.attr,
	&touch_attr_swipe_enable.attr,
	&touch_attr_swipe_pay_area.attr,
	&touch_attr_swipe_tool.attr,
	&touch_attr_app_data.attr,
	&touch_attr_click_test.attr,
	&touch_attr_v_drag_test.attr,
	&touch_attr_h_drag_test.attr,
#if IS_ENABLED(CONFIG_SECURE_TOUCH)
	&touch_attr_secure_touch_enable.attr,
	&touch_attr_secure_touch.attr,
	&touch_attr_secure_touch_devinfo.attr,
#endif
	&touch_attr_ignore_event.attr,
#if IS_ENABLED(CONFIG_LGE_TOUCH_DEX)
	&touch_attr_touch_dex_mode.attr,
#endif
	&touch_attr_rotation.attr,
	&touch_attr_grip_area.attr,
#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
	&touch_attr_hidden_coordi.attr,
#endif
//	&touch_attr_module_test.attr,
	NULL,
};

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
ssize_t tui_sysfs_ctl(struct device *dev, const char *buf, size_t count) {
	ssize_t ret;
	ret = store_secure_touch_enable(dev, buf, count);
	return ret;
}
#endif

static struct attribute *touch_attribute_module_list[] = {
	NULL,
};

static struct bin_attribute *touch_bin_attribute_list[] = {
#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
	&touch_bin_attr_app_fw_upgrade,
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
	&touch_bin_attr_touch_common_control,
#endif
	NULL,
};

static const struct attribute_group touch_attribute_group = {
	.attrs = touch_attribute_list,
	.bin_attrs = touch_bin_attribute_list,
};

static const struct attribute_group touch_attribute_group_module = {
	.attrs = touch_attribute_module_list,
};

static ssize_t touch_attr_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct touch_core_data *ts =
		container_of(kobj, struct touch_core_data, kobj);
	struct touch_attribute *priv =
		container_of(attr, struct touch_attribute, attr);
	ssize_t ret = 0;

	if (priv->show)
		ret = priv->show(ts->dev, buf);

	return ret;
}

static ssize_t touch_attr_show_module(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct module_data *md =
		container_of(kobj, struct module_data, kobj);
	struct touch_attribute *priv =
		container_of(attr, struct touch_attribute, attr);
	ssize_t ret = 0;

	if (priv->show)
		ret = priv->show(md->dev, buf);

	return ret;
}

static ssize_t touch_attr_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct touch_core_data *ts =
		container_of(kobj, struct touch_core_data, kobj);
	struct touch_attribute *priv =
		container_of(attr, struct touch_attribute, attr);

	if (priv->store)
		count = priv->store(ts->dev, buf, count);

	return count;
}

static ssize_t touch_attr_store_module(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	struct module_data *md =
		container_of(kobj, struct module_data, kobj);
	struct touch_attribute *priv =
		container_of(attr, struct touch_attribute, attr);

	if (priv->store)
		count = priv->store(md->dev, buf, count);

	return count;
}

static const struct sysfs_ops touch_sysfs_ops = {
	.show	= touch_attr_show,
	.store	= touch_attr_store,
};

static const struct sysfs_ops touch_sysfs_ops_module = {
	.show	= touch_attr_show_module,
	.store	= touch_attr_store_module,
};

static struct kobj_type touch_kobj_type = {
	.sysfs_ops = &touch_sysfs_ops,
};

static struct kobj_type touch_kobj_type_module = {
	.sysfs_ops = &touch_sysfs_ops_module,
};

int touch_init_sysfs(struct touch_core_data *ts)
{
	struct device *dev = &ts->input->dev;
	int ret;

	ret = kobject_init_and_add(&ts->kobj, &touch_kobj_type,
			dev->kobj.parent, "%s", LGE_TOUCH_NAME);

	if (ret < 0) {
		TOUCH_E("failed to initialize kobject\n");
		goto error;
	}

	ret = sysfs_create_group(&ts->kobj, &touch_attribute_group);

	if (ret < 0) {
		TOUCH_E("failed to create sysfs\n");
		goto error;
	}

	if (ts->driver->register_sysfs) {
		ret = ts->driver->register_sysfs(dev);
		if (ret < 0) {
			TOUCH_E("failed to device create sysfs\n");
			goto error;
		}
	}

	if (!ts->role.use_film_status) {
		sysfs_remove_file_from_group(&ts->kobj,
				&touch_attr_film_status.attr, NULL);
	}
#if IS_ENABLED(CONFIG_LGE_TOUCH_PEN)
	ret = pen_register_sysfs(ts->dev);
		if (ret < 0) {
			TOUCH_E("failed to device create pen sysfs\n");
			goto error;
		}
	if (!ts->role.use_active_pen_status) {
		sysfs_remove_file_from_group(&ts->kobj,
				&touch_attr_active_pen_status.attr, NULL);
	}
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_DEX)
	if (!ts->role.use_dex_mode) {
		sysfs_remove_file_from_group(&ts->kobj,
				&touch_attr_touch_dex_mode.attr, NULL);
	}
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
	ret = read_sysfs_lists(dev, &touch_attribute_group);
	if (ret < 0)
		TOUCH_I("fail to read sysfs lists\n");
#endif
error:
	return ret;
}

int touch_init_sysfs_module(struct module_data *md, void *module_ts)
{
	struct touch_core_data *ts = (struct touch_core_data *)module_ts;
	int ret = 0;

	if (md->m_driver.register_sysfs != NULL) {
		ret = kobject_init_and_add(&md->kobj, &touch_kobj_type_module,
				&ts->kobj, "%s", "module");

		if (ret < 0) {
			TOUCH_E("failed to initialize kobject module\n");
			return 0;
		}

		ret = sysfs_create_group(&md->kobj, &touch_attribute_group_module);

		if (ret < 0) {
			TOUCH_E("failed to create sysfs module\n");
			return 0;
		}

		ret = md->m_driver.register_sysfs(md->dev);
		if (ret < 0) {
			TOUCH_E("failed to module device create sysfs\n");
			goto error;
		}
	}
error:
	return ret;
}
