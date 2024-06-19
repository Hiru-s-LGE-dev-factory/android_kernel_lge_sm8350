/*
 * Veneer PSY
 * Copyright (C) 2017 LG Electronics, Inc
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 */

#define pr_fmt(fmt) "CHG: [VENEER] %s: " fmt, __func__
#define pr_veneer(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#define pr_dbg_veneer(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

#include "veneer-primitives.h"


#define VENEER_NAME		"veneer"
#define VENEER_COMPATIBLE	"lge,veneer-psy"
#define VENEER_DRIVER		"lge-veneer-psy"

#define VENEER_WAKELOCK 	VENEER_NAME": charging"
#define VENEER_NOTREADY		INT_MAX

#define SLOW_CHARGING_TIMEOUT_MS	5000
#define SLOW_CHARGING_CURRENT_MA	450

static int debug_polling_time = 30000;
module_param_named(
	debug_polling_time, debug_polling_time, int, 0600
);

struct power_supply* get_psy_cp(struct veneer* veneer_me)
{
	if (!veneer_me->psy_cp)
		veneer_me->psy_cp = power_supply_get_by_name("charge_pump_master");
	return veneer_me->psy_cp;
}

struct power_supply* get_psy_battery(struct veneer* veneer_me)
{
	if (!veneer_me->psy_battery)
		veneer_me->psy_battery = power_supply_get_by_name("battery");
	return veneer_me->psy_battery;
}

struct power_supply* get_psy_usb(struct veneer* veneer_me)
{
	if (!veneer_me->psy_usb)
		veneer_me->psy_usb = power_supply_get_by_name("usb");
	return veneer_me->psy_usb;
}

struct power_supply* get_psy_wireless(struct veneer* veneer_me)
{
	if (!veneer_me->psy_wireless)
		veneer_me->psy_wireless = power_supply_get_by_name("wireless");
	return veneer_me->psy_wireless;
}

struct power_supply* get_psy_dc(struct veneer* veneer_me)
{
	if (!veneer_me->psy_dc)
		veneer_me->psy_dc = power_supply_get_by_name("dc");
	return veneer_me->psy_dc;
}

static bool supplier_online(struct veneer* veneer_me)
{
	bool online_usb = veneer_me->presence_usb
		&& veneer_voter_suspended(VOTER_TYPE_IUSB)
			!= CHARGING_SUSPENDED_WITH_FAKE_OFFLINE;
	bool online_wireless = veneer_me->presence_wireless
		&& veneer_voter_suspended(VOTER_TYPE_IDC)
			!= CHARGING_SUSPENDED_WITH_FAKE_OFFLINE;

	return online_usb || online_wireless;
}

static bool supplier_connected(struct veneer* veneer_me)
{
	return veneer_me->presence_usb
		|| veneer_me->presence_wireless;
}

static void update_veneer_trigger(struct veneer* veneer_me)
{
	bool terminated = false, presence_otg = false;
	int val = 1;

	if (veneer_me->changed_prop & BIT(CP_CHARGE_STATUS)) {
		if ((veneer_me->charge_status == CHARGE_STATUS_TERMINATION
					|| veneer_me->charge_status == CHARGE_STATUS_CHARGING_DISABLED)
				&& !veneer_voter_enabled_fake_ui(VOTER_TYPE_VFLOAT))
			terminated = true;

		if (veneer_me->battery_eoc != terminated)
			veneer_me->battery_eoc = terminated;

		unified_nodes_write(UNIFIED_NODE_CHARGING_STEP, veneer_me->charge_status);
	}

	if (veneer_me->changed_prop & BIT(CP_USB_PRESENT)) {
		if (veneer_me->presence_usb) {
			schedule_delayed_work(&veneer_me->dwork_slowchg,
				round_jiffies_relative(msecs_to_jiffies(SLOW_CHARGING_TIMEOUT_MS)));
			pr_dbg_veneer("SLOWCHG: Start timer to check slow charger, PRESENT\n");
		} else {
			veneer_me->usbin_realtype = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			veneer_me->usbin_aicl = 0;
			set_veneer_param(VENEER_FEED_USB_RESISTANCE_ID, val);
			cancel_delayed_work(&veneer_me->dwork_slowchg);
		}
	}

	if (veneer_me->changed_prop & BIT(CP_USB_TYPE)) {
		if (veneer_me->usbin_realtype != POWER_SUPPLY_USB_TYPE_UNKNOWN)
			set_veneer_param(VENEER_FEED_USB_RESISTANCE_ID, val);
	}

	if (veneer_me->changed_prop & BIT(CP_TYPEC_MODE)) {
		if (veneer_me->usb_typec_mode == USB_TYPEC_SINK
				|| veneer_me->usb_typec_mode == USB_TYPEC_SINK_POWERED_CABLE)
			presence_otg = true;

		if (veneer_me->presence_otg != presence_otg)
			veneer_me->presence_otg = presence_otg;
	}
}

static void update_veneer_status(struct veneer* veneer_me)
{
	bool online = supplier_online(veneer_me);
	int capacity = veneer_me->battery_capacity;
	int status;

	if (capacity == 100)
		status = POWER_SUPPLY_STATUS_FULL;
	else if (online)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (veneer_me->pseudo_status != status) {
		pr_veneer("pseudo_status is updated to %d\n", status);
		veneer_me->pseudo_status = status;
	}
}

static void add_pm_qos_request(struct veneer* veneer_me)
{
	if (veneer_me && !veneer_me->pm_qos_flag) {
//FIXED_ME
//		veneer_me->pm_qos.irq = -1;
//		veneer_me->pm_qos.type = PM_QOS_REQ_AFFINE_CORES;
//
//		cpumask_copy(&veneer_me->pm_qos.cpus_affine, cpu_present_mask);
		if (!pm_qos_request_active(&veneer_me->pm_qos))
			pm_qos_add_request(&veneer_me->pm_qos, PM_QOS_CPU_DMA_LATENCY, 0);

		veneer_me->pm_qos_flag = true;
	}
}

static void remove_pm_qos_request(struct veneer* veneer_me)
{
	if (veneer_me && veneer_me->pm_qos_flag) {
		if (pm_qos_request_active(&veneer_me->pm_qos))
			pm_qos_remove_request(&veneer_me->pm_qos);

		veneer_me->pm_qos_flag = false;
	}
}

static bool charging_wakelock_acquire(struct veneer* veneer_me)
{
	if (veneer_me && !veneer_me->veneer_wakelock->active) {
		pr_veneer("%s\n", VENEER_WAKELOCK);
		__pm_stay_awake(veneer_me->veneer_wakelock);
		add_pm_qos_request(veneer_me);
		return true;
	}
	return false;
}

static bool charging_wakelock_release(struct veneer* veneer_me)
{
	if (veneer_me && veneer_me->veneer_wakelock->active) {
		pr_veneer("%s\n", VENEER_WAKELOCK);
		__pm_relax(veneer_me->veneer_wakelock);
		pm_wakeup_event(veneer_me->veneer_dev, 1000);
		remove_pm_qos_request(veneer_me);
		return true;
	}
	return false;
}

static void update_veneer_wakelock(struct veneer* veneer_me)
{
	bool connected = supplier_connected(veneer_me);
	bool eoc = veneer_me->battery_eoc;

	if (connected && !eoc)
		charging_wakelock_acquire(veneer_me);
	else
		charging_wakelock_release(veneer_me);
}

static enum charging_supplier supplier_sdp(void)
{
	enum charging_supplier ret = CHARGING_SUPPLY_USB_2P0;
	int usb_id, usb3p0, floated;

	if (get_veneer_param(VENEER_FEED_USB_RESISTANCE_ID, &usb_id)
			|| get_veneer_param(VENEER_FEED_USB3P0, &usb3p0)
			|| get_veneer_param(VENEER_FEED_FLOATED, &floated))
		return ret;

	switch (usb_id) {
	case CHARGER_USBID_56KOHM :
		ret = CHARGING_SUPPLY_FACTORY_56K;
		break;

	case CHARGER_USBID_130KOHM :
		ret = CHARGING_SUPPLY_FACTORY_130K;
		break;

	case CHARGER_USBID_910KOHM :
		ret = CHARGING_SUPPLY_FACTORY_910K;
		break;

	default :
		if (floated)
			ret = CHARGING_SUPPLY_TYPE_FLOAT;
		else if (usb3p0)
			ret = CHARGING_SUPPLY_USB_3PX;
		break;
	}

	return ret;
}

static enum charging_supplier supplier_typec(struct veneer* veneer_me)
{
	enum charging_supplier supplier = veneer_me->veneer_supplier;
	enum charging_supplier ret = CHARGING_SUPPLY_DCP_DEFAULT;
	int typec_mode, hvdcp_type, floated, fake_hvdcp;

	if (get_veneer_param(VENEER_FEED_TYPEC_MODE, &typec_mode)
			|| get_veneer_param(VENEER_FEED_HVDCP_TYPE, &hvdcp_type)
			|| get_veneer_param(VENEER_FEED_FAKE_HVDCP, &fake_hvdcp)
			|| get_veneer_param(VENEER_FEED_FLOATED, &floated))
		return ret;

	switch (typec_mode) {
	case USB_TYPEC_NONE :
	case USB_TYPEC_SOURCE_DEFAULT :
		if (floated)
			ret = CHARGING_SUPPLY_TYPE_FLOAT;
		else
			ret = CHARGING_SUPPLY_DCP_DEFAULT;
		break;

	case USB_TYPEC_SOURCE_MEDIUM :
		ret = CHARGING_SUPPLY_DCP_22K;
		break;

	case USB_TYPEC_SOURCE_HIGH :
		ret = CHARGING_SUPPLY_DCP_10K;
		break;

	default :
		break;
	}

	if (fake_hvdcp && supplier != CHARGING_SUPPLY_DCP_QC2) {
		ret = CHARGING_SUPPLY_DCP_QC2;
		set_veneer_param(VENEER_FEED_FAKE_HVDCP, fake_hvdcp);
		pr_veneer("Fake HVDCP is enabled, set supplier to QC2\n");
	} else if (hvdcp_type == QC20)
		ret = CHARGING_SUPPLY_DCP_QC2;
	else if (hvdcp_type == QC30)
		ret = CHARGING_SUPPLY_DCP_QC3;
	else if (supplier == CHARGING_SUPPLY_DCP_QC2 || supplier == CHARGING_SUPPLY_DCP_QC3)
		ret = supplier;

	return ret;
}

static enum charging_supplier supplier_wireless(struct veneer* veneer_me)
{
	enum charging_supplier ret = CHARGING_SUPPLY_TYPE_UNKNOWN;
	int wls_power;

	if (get_veneer_param(VENEER_FEED_DC_POWER_NOW, &wls_power))
		return ret;

	wls_power /= 1000;
	pr_veneer("wireless POWER_SUPPLY_PROP_POWER_NOW = %d\n", wls_power);
	if (wls_power >= veneer_me->threshold_wireless_15w)
		ret = CHARGING_SUPPLY_WIRELESS_15W;
	else if (wls_power >= veneer_me->threshold_wireless)
		ret = CHARGING_SUPPLY_WIRELESS_9W;
	else
		ret = CHARGING_SUPPLY_WIRELESS_5W;

	return ret;
}

static void update_veneer_supplier(struct veneer* veneer_me)
{
	enum charging_supplier supplier = CHARGING_SUPPLY_TYPE_UNKNOWN;

	if (veneer_me->presence_usb) {
		pr_dbg_veneer("POWER_SUPPLY_PROP_USB_TYPE = %d\n", veneer_me->usbin_realtype);
		switch (veneer_me->usbin_realtype) {
		case POWER_SUPPLY_USB_TYPE_DCP :
		case POWER_SUPPLY_USB_TYPE_C :
			supplier = supplier_typec(veneer_me);
			break;

		case POWER_SUPPLY_USB_TYPE_SDP :
			supplier = supplier_sdp();
			break;

		case POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID :
			if (!unified_bootmode_usermode())
				supplier = supplier_sdp();
			else
				supplier = supplier_typec(veneer_me);
			break;

		case POWER_SUPPLY_USB_TYPE_CDP :
			supplier = CHARGING_SUPPLY_USB_CDP;
			break;
		case POWER_SUPPLY_USB_TYPE_PD :
			supplier = CHARGING_SUPPLY_USB_PD;
			break;
		case POWER_SUPPLY_USB_TYPE_PD_PPS :
			supplier = CHARGING_SUPPLY_USB_PD_PPS;
			break;

		default :
			break;
		}
	} else if (veneer_me->presence_wireless) {
		supplier = supplier_wireless(veneer_me);
	} else {
		/* 'new' may be 'NONE' at the initial time and it will be updated soon */
		supplier = CHARGING_SUPPLY_TYPE_NONE;
	}

	if (veneer_me->veneer_supplier != supplier) {
		/* Updating member of charger type here */
		veneer_me->veneer_supplier = supplier;
		pr_veneer("Charger is changed to %s\n", charger_name(supplier));
	}
}

static void update_veneer_uninodes(struct veneer* veneer_me)
{
	// Should be called
	// - on every effective 'external_power_changed' and
	// - after 'update_veneer_supplier'
	enum charging_supplier	type = veneer_me->veneer_supplier;
	enum charging_verbosity verbose = VERBOSE_CHARGER_NORMAL;
	const char*		name = charger_name(type);
	int mw_now = 0, stored = 0, floated = 0, high_speed = 0;
	int mw_highspeed = INT_MAX;

    // 'bootcmd: lge.charger_verbose=(%bool)'
	//		is adopted to branch vzw/att or not.
	if (unified_bootmode_chgverbose()) {
		switch (type) {
		case CHARGING_SUPPLY_DCP_DEFAULT:
			// VERBOSE_CHARGER_SLOW
			//		will be set in set_property and a dedicated work
			if (get_veneer_param(VENEER_FEED_CHARGER_VERBOSE, &stored))
				break;

			if 	(stored == VERBOSE_CHARGER_SLOW)
				verbose = VERBOSE_CHARGER_SLOW;
			break;

		case CHARGING_SUPPLY_TYPE_NONE:
		case CHARGING_SUPPLY_TYPE_UNKNOWN:
			verbose = VERBOSE_CHARGER_NONE;
			break;

		case CHARGING_SUPPLY_TYPE_FLOAT:
			verbose = VERBOSE_CHARGER_INCOMPATIBLE;
			break;

		default :
			break;
		}

		/* Updating verbosity here for vzw models */
		set_veneer_param(VENEER_FEED_CHARGER_VERBOSE, verbose);
	} else {
		/* Updating incompatible charger status here for non-vzw models */
		if (type == CHARGING_SUPPLY_TYPE_FLOAT)
			floated = true;
		set_veneer_param(VENEER_FEED_CHARGER_INCOMPATIBLE, floated);
	}

	/* Updating fast charger status here */
	if (!get_veneer_param(VENEER_FEED_CHARGER_HIGH_SPEED, &high_speed)
			|| !high_speed
			|| type == CHARGING_SUPPLY_TYPE_NONE
			|| type == CHARGING_SUPPLY_TYPE_UNKNOWN
			|| type == CHARGING_SUPPLY_TYPE_FLOAT
			|| type == CHARGING_SUPPLY_WIRELESS_5W ) {
		get_veneer_param(VENEER_FEED_POWER_NOW, &mw_now);
		if (veneer_me->presence_usb)
			mw_highspeed = veneer_me->threshold_wired;
		else if (veneer_me->presence_wireless)
			mw_highspeed = veneer_me->threshold_wireless;

		high_speed = false;
		if (mw_now >= mw_highspeed && type != CHARGING_SUPPLY_DCP_10K)
			high_speed = true;
		set_veneer_param(VENEER_FEED_CHARGER_HIGH_SPEED, high_speed);
	} else
		pr_dbg_veneer("Skip to update highspeed "
			"if buff == 1 && !CHARGING_SUPPLY_TYPE_NONE\n");

	/* Finally, updating charger name here */
	unified_nodes_store("charger_name", name, sizeof(name));
}

static void update_veneer_charging(struct veneer* veneer_me)
{
	return;
}

static void notify_siblings(struct veneer* veneer_me)
{
	/* Capping IUSB/IBAT/IDC by charger */
	charging_ceiling_vote(veneer_me->veneer_supplier);
	/* Calculating remained charging time */
	charging_time_update();
	/* LGE OTP scenario */
	protection_battemp_monitor();
	/* To meet battery spec. */
	protection_batvolt_refresh(supplier_connected(veneer_me));
	/* Limiting SoC range in the store mode charging */
	protection_showcase_update();

	/* adaptive charging thermal mitigation(ACTM) */
	actm_trigger();
}

static void notify_fabproc(struct veneer* veneer_me)
{
	int fastparallel;

	if (!get_veneer_param(VENEER_FEED_SUPPORT_FASTPL, &fastparallel)
			&& fastparallel) {
		set_veneer_param(VENEER_FEED_SUPPORT_FASTPL, fastparallel);
		pr_veneer("psy should provide POWER_SUPPLY_PROP_PARALLEL_DISABLE"
			"for the factory fast parallel charging.\n");
	}
}

static void notify_sar_controller(struct veneer* veneer_me)
{
#ifdef CONFIG_LGE_SAR_CONTROLLER_USB_DETECT
    extern void sar_controller_notify_connect(u32 type, bool is_connected);
    static bool pre_usb_connected;


    enum charging_supplier  type = veneer_me->veneer_supplier;
    bool usb_connected = veneer_me->presence_usb;

    if( pre_usb_connected != usb_connected) {
        sar_controller_notify_connect((u32)type, usb_connected);
        pre_usb_connected = usb_connected;
        pr_veneer("call notify_sar_controller.\n");
    }
#endif
}

static void notify_touch(struct veneer* veneer_me)
{
#ifdef CONFIG_LGE_TOUCH_CORE
	extern void touch_notify_connect(u32 type);
	extern void touch_notify_wireless(u32 type);
	extern void touch_notify_temperature(int type);
	static bool charging_wired_prev, charging_wireless_prev;
	static int battery_temp_raw_prev;

	bool charging_wired_now = veneer_me->presence_usb;
	bool charging_wireless_now = veneer_me->presence_wireless;

	if (battery_temp_raw_prev != veneer_me->battery_temp_raw) {
		touch_notify_temperature(veneer_me->battery_temp_raw);
		battery_temp_raw_prev = veneer_me->battery_temp_raw;
	}

	if (charging_wired_now && charging_wireless_now) {
		// Assertion failed!
		pr_veneer("Wired and wireless charging should"
			" not be online at the same time");
		return;
	}
	else if (charging_wired_prev != charging_wired_now) {
		touch_notify_connect((u32)charging_wired_now);
		charging_wired_prev = charging_wired_now;
	}
	else if (charging_wireless_prev != charging_wireless_now) {
		touch_notify_wireless((u32)charging_wireless_now);
		charging_wireless_prev = charging_wireless_now;
	}
	else
		; // Do nothing yet
#endif
}

struct veneer* veneer_data_fromair(void)
{
	struct power_supply* veneer_psy = power_supply_get_by_name("veneer");
	struct veneer* veneer_me;

	if (veneer_psy) {
		veneer_me = power_supply_get_drvdata(veneer_psy);
		power_supply_put(veneer_psy);
		return veneer_me;
	}

	return NULL;
}

static void veneer_data_update(struct veneer* veneer_me)
{
	update_veneer_status(veneer_me);
	update_veneer_trigger(veneer_me);
	update_veneer_wakelock(veneer_me);
	update_veneer_supplier(veneer_me);
	update_veneer_uninodes(veneer_me);
	update_veneer_charging(veneer_me);

	// After update veneer structures,
	// do update sibling data, factory process and touch device finally.
	notify_siblings(veneer_me);
	notify_fabproc(veneer_me);
	notify_touch(veneer_me);
	notify_sar_controller(veneer_me);
}

static enum power_supply_property psy_property_list [] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
//	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
};

static bool detect_slowchg_required(/* @Nonnull */ struct veneer* veneer_me)
{
	int  verbose;

	return unified_bootmode_chgverbose()
		&& veneer_me->veneer_supplier == CHARGING_SUPPLY_DCP_DEFAULT
		&& veneer_me->usbin_aicl != 0
		&& veneer_me->usbin_aicl < SLOW_CHARGING_CURRENT_MA
		&& !get_veneer_param(VENEER_FEED_CHARGER_VERBOSE, &verbose)
		&& verbose != VERBOSE_CHARGER_SLOW;
}

static void detect_slowchg_timer(struct work_struct* work)
{
	struct veneer* veneer_me =
		container_of(work, struct veneer, dwork_slowchg.work);
	enum charging_verbosity verbose = VERBOSE_CHARGER_SLOW;
	int val = 1;

	if (!veneer_me)
		return;

	if (!detect_slowchg_required(veneer_me)) {
		pr_veneer("SLOWCHG: Charger %s, AICL %d, Threshold %d\n",
			charger_name(veneer_me->veneer_supplier),
			veneer_me->usbin_aicl, SLOW_CHARGING_CURRENT_MA);
		return;
	}

	/* Updating VERBOSE_CHARGER_SLOW here for vzw models */
	set_veneer_param(VENEER_FEED_CHARGER_VERBOSE, verbose);

	/* Toasting popup */
	set_veneer_param(VENEER_FEED_POWER_SUPPLY_CHANGED, val);

	pr_veneer("SLOWCHG: Slow charger detected (AICL %d < Threshold %d)\n",
		veneer_me->usbin_aicl, SLOW_CHARGING_CURRENT_MA);

	return;
}

static int psy_property_get(struct power_supply *psy_me,
	enum power_supply_property prop, union power_supply_propval *val)
{
	struct veneer* veneer_me = power_supply_get_drvdata(psy_me);
	int rc = 0;

	if (!veneer_me) {
		pr_veneer("veneer is not ready yet\n");
		return -EINVAL;
	}

	switch (prop) {
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW :
		val->intval = charging_time_remains();
		break;

	case POWER_SUPPLY_PROP_USB_TYPE :
		val->intval = veneer_me->usbin_realtype;
		break;

	case POWER_SUPPLY_PROP_STATUS :
		val->intval = veneer_me->pseudo_status;
		break;

	case POWER_SUPPLY_PROP_HEALTH :
		get_veneer_param(VENEER_FEED_PSEUDO_HEALTH, &val->intval);
		break;

	default:
		rc = -EINVAL;
	}

	return rc;
}

static int psy_property_set(struct power_supply *psy_me,
	enum power_supply_property prop, const union power_supply_propval *val)
{
	struct veneer* veneer_me = power_supply_get_drvdata(psy_me);
	int rc = 0;

	if (!veneer_me) {
		pr_veneer("veneer is not ready yet\n");
		return -EINVAL;
	}

	return rc;
}

static int psy_property_writeable(struct power_supply *psy,
	enum power_supply_property prop)
{
	int rc = 0;

	switch (prop) {
	default:
		rc = 0;
	}

	return rc;
}

/* VENEER delegates logging to an external psy who can access all the states of battery.
 * Consider that accessing PMIC regs in the VENEER, for example, would be impractical.
 * And the external psy should be able to process logging command,
 * by providing a dedicated logging PROP, POWER_SUPPLY_PROP_DEBUG_BATTERY
 */
static void psy_external_logging(struct work_struct* work)
{
	int val = 0;

	set_veneer_param(VENEER_FEED_DEBUG_BATT, val);

	schedule_delayed_work(
		to_delayed_work(work), msecs_to_jiffies(debug_polling_time));
}

static bool psy_update_param(int id, int* val)
{
	bool ret = false;
	int buf = 0;
	int rate = 1;

	if (id == VENEER_FEED_BAT_VOLTAGE_NOW)
		rate = 100;
	else if (id == VENEER_FEED_SENSOR_BATT)
		rate = 10;

	if (!get_veneer_param(id, &buf) && *val != buf) {
		if (*val/rate != buf/rate)
			ret = true;
		*val = buf;
	}

	return ret;
}

static void psy_external_changed(struct power_supply* psy_me)
{
	struct veneer* veneer_me = power_supply_get_drvdata(psy_me);

	pr_dbg_veneer("psy_external_changed\n");
	schedule_work(&veneer_me->work_exchagned);
}

static void external_changed_work(struct work_struct *work)
{
	struct veneer* veneer_me = container_of(work, struct veneer, work_exchagned);
	char hit [1024] = { 0, };
	char buf[36] = { 0, };
#ifdef CONFIG_USE_WIRELESS_CHARGING
	int val = 0;
#endif

	pr_dbg_veneer("external_changed_work S\n");
	mutex_lock(&veneer_me->veneer_lock);

	veneer_me->changed_prop = 0;

	/* Update eoc */
	if (psy_update_param(VENEER_FEED_CHARGE_STATUS, &veneer_me->charge_status)) {
		veneer_me->changed_prop |= BIT(CP_CHARGE_STATUS);
		sprintf(buf, "B:CHARGE_STATUS(%s) ", charge_status_name(veneer_me->charge_status));
		strcat(hit, buf);
	}

	/* Update capacity */
	if (psy_update_param(VENEER_FEED_CAPACITY, &veneer_me->battery_capacity)) {
		veneer_me->changed_prop |= BIT(CP_CAPACITY);
		sprintf(buf, "B:CAPACITY(%d) ", veneer_me->battery_capacity);
		strcat(hit, buf);
	}

	/* Update uvoltage : Just being used to trigger BTP */
	if (psy_update_param(VENEER_FEED_BAT_VOLTAGE_NOW, &veneer_me->battery_voltage)) {
		veneer_me->changed_prop |= BIT(CP_VOLTAGE_NOW);
		sprintf(buf, "B:VOLTAGE_NOW(%d) ", veneer_me->battery_voltage);
		strcat(hit, buf);
	}

	/* Update temperature : Just being used to trigger BTP */
	if (psy_update_param(VENEER_FEED_SENSOR_BATT, &veneer_me->battery_temperature)
		|| psy_update_param(VENEER_FEED_SENSOR_BATT_RAW, &veneer_me->battery_temp_raw)) {
		veneer_me->changed_prop |= BIT(CP_BAT_TEMP);
		sprintf(buf, "B:TEMP(%d) ", veneer_me->battery_temperature);
		strcat(hit, buf);
	}

	/* Update usb present */
	if (psy_update_param(VENEER_FEED_USB_PRESENT, &veneer_me->presence_usb)) {
		veneer_me->changed_prop |= BIT(CP_USB_PRESENT);
		sprintf(buf, "U:PRESENT(%d) ", veneer_me->presence_usb);
		strcat(hit, buf);
	}

	/* Update AICL */
	if (psy_update_param(VENEER_FEED_USB_AICL, &veneer_me->usbin_aicl)) {
		veneer_me->changed_prop |= BIT(CP_USB_AICL);
		sprintf(buf, "U:USB_AICL(%d) ", veneer_me->usbin_aicl);
		strcat(hit, buf);
	}

	/* Update usb type */
	if (psy_update_param(VENEER_FEED_USB_TYPE, (int*) &veneer_me->usbin_realtype)) {
		veneer_me->changed_prop |= BIT(CP_USB_TYPE);
		sprintf(buf, "U:USB_TYPE(%s) ", usb_type_name(veneer_me->usbin_realtype));
		strcat(hit, buf);
	}

	/* Update typec mode */
	if (psy_update_param(VENEER_FEED_TYPEC_MODE, &veneer_me->usb_typec_mode)) {
		veneer_me->changed_prop |= BIT(CP_TYPEC_MODE);
		sprintf(buf, "B:TYPEC_MODE(%s) ", typec_mode_name(veneer_me->usb_typec_mode));
		strcat(hit, buf);
	}

	/* Update Input power */
	if (psy_update_param(VENEER_FEED_POWER_NOW, &veneer_me->usbin_power)) {
		veneer_me->changed_prop |= BIT(CP_USB_POWER);
		sprintf(buf, "U:POWER(%d) ", veneer_me->usbin_power);
		strcat(hit, buf);
	}

	/* Then update wireless present */
	if (psy_update_param(VENEER_FEED_DC_PRESENT, &veneer_me->presence_wireless)) {
		veneer_me->changed_prop |= BIT(CP_WLS_PRESENT);
		sprintf(buf, "W:PRESENT(%d) ", veneer_me->presence_wireless);
		strcat(hit, buf);
	}

#ifdef CONFIG_USE_WIRELESS_CHARGING
	/* LGE scenario : Disable wireless charging on wired */
	/* veneer_me->enable_concurrency_otg_wlc */
	val = (veneer_me->presence_usb && !veneer_me->presence_otg);
	set_veneer_param(VENEER_FEED_WLS_CHARGE_PAUSE, val);
#endif

	if (veneer_me->changed_prop) {
		pr_veneer("externally changed : %s\n", hit);
		veneer_data_update(veneer_me);
	}
	veneer_me->changed_prop = 0;
	mutex_unlock(&veneer_me->veneer_lock);
	pr_dbg_veneer("external_changed_work E\n");

}

static bool probe_siblings(struct device* dev)
{
	struct device_node* dnode = dev->of_node;
	bool ret = true;

	ret &= veneer_voter_create();

	// veneer voter should be ready before builing siblings
	ret &= charging_ceiling_create(of_find_node_by_name(dnode, "charging-ceiling"));
	ret &= charging_time_create(of_find_node_by_name(dnode, "charging-time-v3"));
	ret &= protection_battemp_create(of_find_node_by_name(dnode, "protection-battemp"));
	ret &= protection_batvolt_create(of_find_node_by_name(dnode, "protection-batvolt-v2"));
	ret &= protection_showcase_create(of_find_node_by_name(dnode, "protection-showcase"));
	ret &= actm_create(of_find_node_by_name(dnode, "adaptive-charging-thermal"));
	ret &= unified_nodes_create(of_find_node_by_name(dnode, "unified-nodes"));
	ret &= unified_sysfs_create(of_find_node_by_name(dnode, "unified-sysfs"));

	return ret;
}

static bool probe_preset(struct veneer* veneer_me)
{
	veneer_me->profile_mvfloat = 4400;	// Fixed value at now
	veneer_me->veneer_supplier = CHARGING_SUPPLY_TYPE_UNKNOWN;

	/* Initialize veneer by default */
	// below shadows can be read before being initialized.
	veneer_me->usbin_realtype = POWER_SUPPLY_USB_TYPE_UNKNOWN;
	veneer_me->usbin_typefix = false;
	veneer_me->usbin_aicl = 0;
	veneer_me->presence_otg = false;
	veneer_me->presence_usb = false;
	veneer_me->presence_wireless = false;
	veneer_me->usb_typec_mode = USB_TYPEC_NONE;

	veneer_me->battery_eoc = false;
	veneer_me->battery_capacity = VENEER_NOTREADY;
	veneer_me->battery_voltage = VENEER_NOTREADY;
	veneer_me->battery_temperature = VENEER_NOTREADY;
	veneer_me->battery_temp_raw = VENEER_NOTREADY;

	veneer_me->charge_status = VENEER_NOTREADY;
	veneer_me->pseudo_status = POWER_SUPPLY_STATUS_UNKNOWN;

	veneer_me->changed_prop = 0;

	mutex_init(&veneer_me->veneer_lock);
	veneer_me->veneer_wakelock = wakeup_source_register(NULL, VENEER_WAKELOCK);

	set_veneer_param(VENEER_FEED_MINCAP, veneer_me->profile_mincap);
	set_veneer_param(VENEER_FEED_FULLRAW, veneer_me->profile_fullraw);

	INIT_DELAYED_WORK(&veneer_me->dwork_logger, psy_external_logging);
	INIT_DELAYED_WORK(&veneer_me->dwork_slowchg, detect_slowchg_timer);
	INIT_WORK(&veneer_me->work_exchagned, external_changed_work);

	return true;
}

static bool probe_dt(struct device* veneer_dev, struct veneer* veneer_me)
{
	struct device_node* veneer_supp =
		of_find_node_by_name(NULL, "lge-battery-supplement");

	if (!veneer_supp)
		return false;

	/* lge-battery-supplement */
	if (of_property_read_u32(veneer_supp, "capacity-mah-min",
			&veneer_me->profile_mincap) < 0
		|| of_property_read_u32(veneer_supp, "capacity-raw-full",
			&veneer_me->profile_fullraw) < 0) {
		pr_err("Failed to get battery profile, Check DT\n");
		return false;
	}

	/* lge-veneer-psy */
	if (of_property_read_u32(veneer_dev->of_node,
			"highspeed-threshold-mv-wired",
			&veneer_me->threshold_wired) < 0)
		veneer_me->threshold_wired = 15000;

	if (of_property_read_u32(veneer_dev->of_node,
			"highspeed-threshold-mv-wireless",
			&veneer_me->threshold_wireless) < 0)
		veneer_me->threshold_wireless = 7200;

	if (of_property_read_u32(veneer_dev->of_node,
			"highspeed-threshold-mv-wireless-15w",
			&veneer_me->threshold_wireless_15w) < 0)
		veneer_me->threshold_wireless_15w = 9000;

	return true;
}

static bool probe_psy(struct veneer* veneer)
{
	static struct power_supply_desc desc = {
		.name = VENEER_NAME,
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = psy_property_list,
		.num_properties = ARRAY_SIZE(psy_property_list),
		.get_property = psy_property_get,
		.set_property = psy_property_set,
		.property_is_writeable = psy_property_writeable,
		.external_power_changed = psy_external_changed,
	};
	struct power_supply_config cfg = {
		.drv_data = veneer,
		.of_node = veneer->veneer_dev->of_node,
	};

	veneer->veneer_psy =
		power_supply_register(veneer->veneer_dev, &desc, &cfg);

	if (!IS_ERR(veneer->veneer_psy)) {
		static char* from [] = { "battery", "usb", "wireless", "dc" };
		veneer->veneer_psy->supplied_from = from;
		veneer->veneer_psy->num_supplies = ARRAY_SIZE(from);
		return true;
	}
	else {
		pr_veneer("Couldn't register veneer power supply (%ld)\n",
			PTR_ERR(veneer->veneer_psy));
		return false;
	}
}

static void veneer_clear(struct veneer* veneer_me)
{
	pr_veneer("Clearing . . .\n");

	charging_ceiling_destroy();
	charging_time_destroy();
	protection_showcase_destroy();
	protection_battemp_destroy();
	protection_batvolt_destroy();
	actm_destroy();
	unified_nodes_destroy();
	unified_sysfs_destroy();

	veneer_voter_destroy();
	if (veneer_me) {
		wakeup_source_unregister(veneer_me->veneer_wakelock);
		cancel_delayed_work(&veneer_me->dwork_logger);
		cancel_delayed_work(&veneer_me->dwork_slowchg);
		if(veneer_me->veneer_psy)
			power_supply_unregister(veneer_me->veneer_psy);

		kfree(veneer_me);
	}
}

static int veneer_probe(struct platform_device *pdev)
{
	struct device* veneer_dev = &pdev->dev;
	struct veneer* veneer_me = kzalloc(sizeof(struct veneer), GFP_KERNEL);

	veneer_me->veneer_dev = veneer_dev;

	if (!veneer_dev || !veneer_me) {
		pr_veneer("%s: veneer resource isn't allocated.\n", __func__);
		goto fail;
	}

	/* Siblings are should be ready before receiving messages from other psy-s */
	if (!probe_dt(veneer_dev, veneer_me)) {
		pr_veneer("Failed on probe_dt\n");
		goto fail;
	}

	if (!probe_preset(veneer_me)) {
		pr_veneer("Failed on probe_preset\n");
		goto fail;
	}

	if (!probe_siblings(veneer_dev))
	{
		pr_veneer("Failed on probe_siblings\n");
		goto fail;
	}

	if (!probe_psy(veneer_me)) {
		pr_veneer("Failed on probe_psy\n");
		goto fail;
	}

	veneer_me->pm_qos_flag = false;
	platform_set_drvdata(pdev, veneer_me);
	device_init_wakeup(veneer_me->veneer_dev, true);
	psy_external_logging(&veneer_me->dwork_logger.work);

	pr_veneer("veneer probe is successful.\n");

	return 0;

fail:	//veneer_clear(veneer_me);
	kfree(veneer_me);
	pr_veneer("Retry to probe further\n");
	return -EPROBE_DEFER;
}

static int veneer_remove(struct platform_device *pdev)
{
	struct veneer *veneer_me = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	veneer_clear(veneer_me);
	return 0;
}

static int veneer_suspend(struct device *dev)
{
	struct veneer* veneer_me = dev_get_drvdata(dev);
	cancel_delayed_work(&veneer_me->dwork_logger);
	return 0;
}

static int veneer_resume(struct device *dev)
{
	struct veneer* veneer_me = dev_get_drvdata(dev);
	schedule_delayed_work(
		&veneer_me->dwork_logger, msecs_to_jiffies(debug_polling_time));
	return 0;
}

static const struct dev_pm_ops veneer_pm_ops = {
	.suspend	= veneer_suspend,
	.resume		= veneer_resume,
};

static const struct of_device_id veneer_match [] = {
	{ .compatible = VENEER_COMPATIBLE },
	{ },
};

static const struct platform_device_id veneer_id [] = {
	{ VENEER_DRIVER, 0 },
	{ },
};

static struct platform_driver veneer_driver = {
	.driver = {
		.name = VENEER_DRIVER,
		.owner = THIS_MODULE,
		.of_match_table = veneer_match,
		.pm 	= &veneer_pm_ops,
	},
	.probe = veneer_probe,
	.remove = veneer_remove,
	.id_table = veneer_id,
};

static int __init veneer_init(void)
{
	pr_veneer("platform_driver_register : veneer_driver\n");
	return platform_driver_register(&veneer_driver);
}

static void __exit veneer_exit(void)
{
	pr_veneer("platform_driver_unregister : veneer_driver\n");
	platform_driver_unregister(&veneer_driver);
}

#if !defined(CONFIG_LGE_PM_ACTM) && !defined(CONFIG_LGE_PM_ACTM_V2)
void actm_trigger(void) { return; };
void actm_destroy(void) { return; };
bool actm_create(struct device_node* dnode) { return true; };
#endif

module_init(veneer_init);
module_exit(veneer_exit);

MODULE_DESCRIPTION(VENEER_DRIVER);
MODULE_LICENSE("GPL v2");
