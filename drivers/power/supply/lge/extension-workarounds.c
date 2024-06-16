/*
 * LGE workaround code list
 * - JUDY-12620     : Avoiding MBG fault on SBU pin
 * - TIME-3624      : Rerun apsd for abnormal sdp
 * - JUDY-6149      : Support for weak battery pack
 * - TIME-4164      : Enable CP charging for 2nd charger test
 * - TIME-3963      : Avoid Inrush current for USB Compliance test
 */

#define pr_wa(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#define pr_dbg_wa(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/pmic-voter.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>

#include "veneer-primitives.h"
#ifdef CONFIG_LGE_USB_SBU_SWITCH
#include <linux/usb/lge_sbu_switch.h>
#endif
#include "extension-qti.h"

////////////////////////////////////////////////////////////////////////////
// LGE Workaround : Helper functions
////////////////////////////////////////////////////////////////////////////
#define APSD_RERUN_DELAY_MS 4000

static struct battery_chg_dev* wa_helper_chg(void)
{
	// getting smb_charger from air
	struct power_supply*	psy
		= power_supply_get_by_name("battery");
	struct battery_chg_dev*	bcdev
		= psy ? power_supply_get_drvdata(psy) : NULL;
	if (psy)
		power_supply_put(psy);

	return bcdev;
}

static bool wa_command_apsd(/*@Nonnull*/ struct battery_chg_dev* bcdev)
{
	return write_oem_property(bcdev, OEM_APSD_RERUN, 1);
}

////////////////////////////////////////////////////////////////////////////
// LGE Workaround : Avoiding MBG fault on SBU pin
////////////////////////////////////////////////////////////////////////////

#ifdef CONFIG_LGE_USB_SBU_SWITCH
// Rather than accessing pointer directly, Referring it as a singleton instance
static struct lge_sbu_switch_instance* wa_avoiding_mbg_fault_singleton(struct device *dev)
{
	static struct lge_sbu_switch_desc 	wa_amf_description = {
		.flags  = LGE_SBU_SWITCH_FLAG_SBU_AUX
			| LGE_SBU_SWITCH_FLAG_SBU_USBID
			| LGE_SBU_SWITCH_FLAG_SBU_FACTORY_ID,
	};
	static struct lge_sbu_switch_instance*	wa_amf_instance;
	static DEFINE_MUTEX(wa_amf_mutex);

	if (IS_ERR_OR_NULL(wa_amf_instance)) {
		mutex_lock(&wa_amf_mutex);
		if (IS_ERR_OR_NULL(wa_amf_instance)) {
			wa_amf_instance
				= devm_lge_sbu_switch_instance_register(dev, &wa_amf_description);
			if (IS_ERR_OR_NULL(wa_amf_instance))
				devm_lge_sbu_switch_instance_unregister(dev, wa_amf_instance);
		}
		mutex_unlock(&wa_amf_mutex);
	}

	return IS_ERR_OR_NULL(wa_amf_instance) ? NULL : wa_amf_instance;
}

bool wa_avoiding_mbg_fault_uart(struct device *dev, bool enable)
{
	// Preparing instance and checking validation of it.
	struct lge_sbu_switch_instance* instance
		= wa_avoiding_mbg_fault_singleton(dev);
	if (!instance)
		return -1;

	if (enable) {
		if (lge_sbu_switch_get_current_flag(instance) != LGE_SBU_SWITCH_FLAG_SBU_AUX)
			lge_sbu_switch_get(instance, LGE_SBU_SWITCH_FLAG_SBU_AUX);
	}
	else
		lge_sbu_switch_put(instance, LGE_SBU_SWITCH_FLAG_SBU_AUX);

	return 0;
}

bool wa_avoiding_mbg_fault_usbid(struct device *dev, bool enable)
{
	// Preparing instance and checking validation of it.
	struct lge_sbu_switch_instance* instance
		= wa_avoiding_mbg_fault_singleton(dev);
	if (!instance)
		return -1;

	if (enable)
		lge_sbu_switch_get(instance, LGE_SBU_SWITCH_FLAG_SBU_FACTORY_ID);
	else
		lge_sbu_switch_put(instance, LGE_SBU_SWITCH_FLAG_SBU_FACTORY_ID);

	return 0;
}
#else
bool wa_avoiding_mbg_fault_uart(struct device *dev, bool enable) { return 0; };
bool wa_avoiding_mbg_fault_usbid(struct device *dev, bool enable) { return 0; };
#endif


void wa_wls_switch_to_usb(void)
{
	union  power_supply_propval buffer;
	struct power_supply*	psy
		= power_supply_get_by_name("dc");

	if(!psy) {
		pr_wa("Error to get dc psy");
		return;
	}

	buffer.intval = 1;
	if (power_supply_set_property(psy,
				POWER_SUPPLY_PROP_EXT_CHARGE_PAUSE, &buffer)) {
		pr_wa("Error to pause wireless : %d\n", buffer.intval);
	}
	power_supply_put(psy);
	return;
}

////////////////////////////////////////////////////////////////////////////
// LGE Workaround : Rerun apsd for abnormal sdp
////////////////////////////////////////////////////////////////////////////

static void wa_rerun_apsd_for_sdp_main(struct work_struct *work)
{
	struct ext_battery_chg *ext_bcdev = container_of(work, struct ext_battery_chg,
						wa_rerun_apsd_for_sdp_dwork.work);
	struct battery_chg_dev *bcdev = ext_bcdev->bcdev;

	if (!ext_bcdev->wa_rerun_apsd_done_with_sdp || ext_bcdev->is_usb_configured) {
		pr_wa("stop apsd done. apsd(%d), configured(%d)\n",
				ext_bcdev->wa_rerun_apsd_done_with_sdp, ext_bcdev->is_usb_configured);
		return;
	}

	pr_wa("Rerun apsd\n");
	wa_command_apsd(bcdev);
}

void wa_rerun_apsd_for_sdp_triger(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	bool usb_type_sdp = ext_bcdev->usb_type == POWER_SUPPLY_USB_TYPE_SDP;
	bool usb_vbus_high = ext_bcdev->usbin_plugin;

	if (!ext_bcdev->enable_rerun_apsd_sdp)
		return;

	pr_dbg_wa("done(%d), SDP(%d), Vbus(%d)\n",
		ext_bcdev->wa_rerun_apsd_done_with_sdp, usb_type_sdp, usb_vbus_high);

	if (!ext_bcdev->wa_rerun_apsd_done_with_sdp
			&& usb_type_sdp && usb_vbus_high && !ext_bcdev->no_batt_boot) {
		ext_bcdev->wa_rerun_apsd_done_with_sdp = true;
		schedule_delayed_work(&ext_bcdev->wa_rerun_apsd_for_sdp_dwork,
			round_jiffies_relative(msecs_to_jiffies(APSD_RERUN_DELAY_MS)));
	} else if (ext_bcdev->wa_rerun_apsd_done_with_sdp
			&& is_client_vote_enabled(ext_bcdev->usb_icl_votable, USB_PSY_VOTER)
			&& !usb_type_sdp && usb_vbus_high) {
		vote(ext_bcdev->usb_icl_votable, USB_PSY_VOTER, false, 0);
	}
}

void wa_rerun_apsd_for_sdp_clear(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev->enable_rerun_apsd_sdp)
		return;

	ext_bcdev->wa_rerun_apsd_done_with_sdp = false;
}

void wa_rerun_apsd_for_sdp_init(struct battery_chg_dev* bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	struct device_node *node = bcdev->dev->of_node;
	struct device_node *dnode =
		of_find_node_by_name(node, "veneer-workaround");

	ext_bcdev->enable_rerun_apsd_sdp =
		of_property_read_bool(dnode, "lge,enable-rerun-apsd-sdp");

	if (!ext_bcdev->enable_rerun_apsd_sdp)
		return;

	ext_bcdev->wa_rerun_apsd_done_with_sdp = false;
	INIT_DELAYED_WORK(&ext_bcdev->wa_rerun_apsd_for_sdp_dwork,
			wa_rerun_apsd_for_sdp_main);
}


////////////////////////////////////////////////////////////////////////////
// LGE Workaround : Support for weak battery pack
////////////////////////////////////////////////////////////////////////////

#define WEAK_SUPPLY_VOTER "WEAK_SUPPLY_VOTER"
#define WEAK_DETACH_DELAY_MS		500
#define WEAK_ATTACH_DELAY_MS		6000
#define WEAK_DETECTION_COUNT		6
#define DEFAULT_WEAK_ICL_MA			1000

static void wa_support_weak_supply_func(struct work_struct *work)
{
	struct ext_battery_chg *ext_bcdev = container_of(work, struct ext_battery_chg,
						wa_support_weak_supply_dwork.work);

	if (ext_bcdev->wa_support_weak_supply_count != 1)
		pr_wa("Skip to check weak charger. count = %d \n", ext_bcdev->wa_support_weak_supply_count);
	ext_bcdev->wa_support_weak_supply_count = 0;
}

void wa_support_weak_supply_trigger(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int delay_ms = ext_bcdev->usbin_plugin ? WEAK_ATTACH_DELAY_MS : WEAK_DETACH_DELAY_MS;

	if (!ext_bcdev->enable_support_weak_supply)
		return;

	if (delayed_work_pending(&ext_bcdev->wa_support_weak_supply_dwork))
		cancel_delayed_work(&ext_bcdev->wa_support_weak_supply_dwork);

	if (ext_bcdev->usbin_plugin && ext_bcdev->wa_support_weak_supply_count >= WEAK_DETECTION_COUNT) {
		pr_wa("Weak battery is detected, set ICL to 1A\n");
		ext_bcdev->wa_support_weak_supply_count = 0;
		vote(ext_bcdev->usb_icl_votable, WEAK_SUPPLY_VOTER, true, DEFAULT_WEAK_ICL_MA);
	} else if (!ext_bcdev->usbin_plugin || ext_bcdev->wa_support_weak_supply_count != 0){
		vote(ext_bcdev->usb_icl_votable, WEAK_SUPPLY_VOTER, false, 0);
		ext_bcdev->wa_support_weak_supply_count++;
		schedule_delayed_work(&ext_bcdev->wa_support_weak_supply_dwork,
			msecs_to_jiffies(delay_ms));
	}
}

void wa_support_weak_supply_init(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	struct device_node *node = bcdev->dev->of_node;
	struct device_node *dnode =
		of_find_node_by_name(node, "veneer-workaround");

	ext_bcdev->enable_support_weak_supply =
		of_property_read_bool(dnode, "lge,enable-support-weak-supply");

	if (!ext_bcdev->enable_support_weak_supply)
		return;

	ext_bcdev->wa_support_weak_supply_count = 0;
	INIT_DELAYED_WORK(&ext_bcdev->wa_support_weak_supply_dwork,
			wa_support_weak_supply_func);
}


////////////////////////////////////////////////////////////////////////////
// LGE Workaround : Enable CP charging for 2nd charger test
////////////////////////////////////////////////////////////////////////////

#define FASTER_QC_VOTER        "FASTER_QC_VOTER"
#define MIN_PD_CURRENT_UA		2500
#define PD_INPUT_CURRENT_UA		2500
#define QC_INPUT_CURRENT_UA		1500

static void wa_faster_try_cp_qc30_trigger(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int data = 0;

	if (!ext_bcdev->enable_faster_try)
		return;

	if(unified_bootmode_usermode())
		return;

	if (unified_nodes_read(UNIFIED_NODE_SUPPORT_FASTPL, &data) && data != 1)
		return;

	if (ext_bcdev->usb_type == POWER_SUPPLY_USB_TYPE_PD_PPS
			|| ext_bcdev->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
		write_oem_property(bcdev, OEM_ENABLE_MCP_CV, true);
		vote_priority(ext_bcdev->fcc_votable, FASTER_QC_VOTER, true, MIN_PD_CURRENT_UA, PRIORITY_HIGH);
		if (ext_bcdev->usb_type == POWER_SUPPLY_USB_TYPE_PD_PPS){
			vote_priority(ext_bcdev->usb_icl_votable, FASTER_QC_VOTER, true, PD_INPUT_CURRENT_UA, PRIORITY_HIGH);
		}
		else {
			vote_priority(ext_bcdev->usb_icl_votable, FASTER_QC_VOTER, true, QC_INPUT_CURRENT_UA, PRIORITY_HIGH);
		}
		return;
	}
}

static void wa_faster_try_cp_qc30_clear(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev->enable_faster_try)
		return;

	if(unified_bootmode_usermode())
		return;

	write_oem_property(bcdev, OEM_ENABLE_MCP_CV, false);
	vote_priority(ext_bcdev->fcc_votable, FASTER_QC_VOTER, false, 0, PRIORITY_HIGH);
	vote_priority(ext_bcdev->usb_icl_votable, FASTER_QC_VOTER, false, 0, PRIORITY_HIGH);
}

static void wa_faster_try_cp_qc30_init(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	struct device_node *node = bcdev->dev->of_node;
	struct device_node *dnode =
		of_find_node_by_name(node, "veneer-workaround");

	ext_bcdev->enable_faster_try =
		of_property_read_bool(dnode, "lge,enable-faster-try");

	if (!ext_bcdev->enable_faster_try)
		return;
}

////////////////////////////////////////////////////////////////////////////
// LGE Workaround : LGE Workaround : Avoid Inrush current for USB Compliance test
////////////////////////////////////////////////////////////////////////////
#define AVOID_INRUSH_DELAY_MS 100
#define AVOID_INRUSH_VOTER		"AVOID_INRUSH_VOTER"

static void wa_avoid_inrush_current_func(struct work_struct *work)
{
	struct ext_battery_chg *ext_bcdev = container_of(work, struct ext_battery_chg,
						wa_avoid_inrush_current_dwork.work);

	pr_wa("Avoid inrush current input suspend timer expired\n");
	vote(ext_bcdev->usb_icl_votable, AVOID_INRUSH_VOTER, false, 0);
}

void wa_avoid_inrush_current_trigger(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev->enable_avoid_inrush_current)
		return;

	schedule_delayed_work(&ext_bcdev->wa_avoid_inrush_current_dwork,
			msecs_to_jiffies(AVOID_INRUSH_DELAY_MS));
	vote(ext_bcdev->usb_icl_votable, AVOID_INRUSH_VOTER, true, 0);
}

void wa_avoid_inrush_current_clear(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev->enable_avoid_inrush_current)
		return;

	vote(ext_bcdev->usb_icl_votable, AVOID_INRUSH_VOTER,
		true, 0);
		//ext_bcdev->is_usb_compliance_mode, 0);
}

void wa_avoid_inrush_current_skip(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev->enable_avoid_inrush_current)
		return;

	vote(ext_bcdev->usb_icl_votable, AVOID_INRUSH_VOTER, false, 0);
}

void wa_avoid_inrush_current_init(struct battery_chg_dev* bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	struct device_node *node = bcdev->dev->of_node;
	struct device_node *dnode =
		of_find_node_by_name(node, "veneer-workaround");

	ext_bcdev->enable_avoid_inrush_current =
		of_property_read_bool(dnode, "lge,enable-avoid-inrush-current");

	if (!ext_bcdev->enable_avoid_inrush_current)
		return;

	if (!unified_bootmode_usermode()) {
		ext_bcdev->enable_avoid_inrush_current = false;
		return;
	}

	INIT_DELAYED_WORK(&ext_bcdev->wa_avoid_inrush_current_dwork,
			wa_avoid_inrush_current_func);
}
////////////////////////////////////////////////////////////////////////////
// LGE Workaround : Helper functions init
////////////////////////////////////////////////////////////////////////////

void wa_update_usb_configured(bool configured)
{
	struct battery_chg_dev* bcdev = wa_helper_chg();
	struct ext_battery_chg *ext_bcdev;

	if (!bcdev) {
		pr_wa("'chg' is not ready\n");
		return;
	}
	ext_bcdev = bcdev->ext_bcdev;

	ext_bcdev->is_usb_configured = configured;
}

void wa_update_usb_compliance_mode(bool mode)
{
	struct battery_chg_dev* bcdev = wa_helper_chg();
	struct ext_battery_chg *ext_bcdev;

	if (!bcdev) {
		pr_wa("'chg' is not ready\n");
		return;
	}
	ext_bcdev = bcdev->ext_bcdev;

	ext_bcdev->is_usb_compliance_mode = mode;
	write_oem_property(bcdev, OEM_COMPLIANCE_MODE, mode);
}

/* global LGE workaround notification callback */
static void wa_usbin_plugin_cb(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (ext_bcdev->usbin_plugin) {
		wa_avoid_inrush_current_trigger(bcdev);
	} else {
		wa_rerun_apsd_for_sdp_clear(bcdev);
		wa_faster_try_cp_qc30_clear(bcdev);
		wa_avoid_inrush_current_clear(bcdev);
	}
	wa_support_weak_supply_trigger(bcdev);

	return;
}

static void wa_wlsin_plugin_cb(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev->wlsin_plugin) {
		if (!ext_bcdev->usbin_plugin) {
			wa_avoid_inrush_current_clear(bcdev);
		}
		rerun_election(ext_bcdev->usb_icl_votable);
	} else
		rerun_election(ext_bcdev->dc_icl_votable);

	return;
}

static void wa_source_change_cb(struct battery_chg_dev *bcdev)
{
	wa_rerun_apsd_for_sdp_triger(bcdev);
	wa_faster_try_cp_qc30_trigger(bcdev);

	return;
}

static void wa_typec_change_cb(struct battery_chg_dev *bcdev) { return; }
static void wa_psy_change_cb(struct battery_chg_dev *bcdev) { return; }

static const char * const POWER_SUPPLY_USB_TYPE_TEXT[] = {
	[POWER_SUPPLY_USB_TYPE_UNKNOWN]		= "Unknown",
	[POWER_SUPPLY_USB_TYPE_SDP]		= "SDP",
	[POWER_SUPPLY_USB_TYPE_DCP]		= "DCP",
	[POWER_SUPPLY_USB_TYPE_CDP]		= "CDP",
	[POWER_SUPPLY_USB_TYPE_ACA]		= "ACA",
	[POWER_SUPPLY_USB_TYPE_C]		= "C",
	[POWER_SUPPLY_USB_TYPE_PD]		= "PD",
	[POWER_SUPPLY_USB_TYPE_PD_DRP]		= "PD_DRP",
	[POWER_SUPPLY_USB_TYPE_PD_PPS]		= "PD_PPS",
	[POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID]	= "BrickID",
};

static const char * const POWER_SUPPLY_TYPEC_MODE_TEXT[] = {
	[USB_TYPEC_NONE]								= "None",
	[USB_TYPEC_SINK]								= "SINK (Rd only)",
	[USB_TYPEC_SINK_POWERED_CABLE]					= "SINK_POWERED_CABLE (Rd/Ra)",
	[USB_TYPEC_SINK_DEBUG_ACCESSORY]				= "SINK_DEBUG_ACCESSORY (Rd/Rd)",
	[USB_TYPEC_SINK_AUDIO_ADAPTER]					= "SINK_AUDIO_ADAPTER (Ra/Ra)",
	[USB_TYPEC_POWERED_CABLE_ONLY]					= "POWERED_CABLE_ONLY (Ra only)",
	[USB_TYPEC_SOURCE_DEFAULT]						= "SOURCE_DEFAULT (Rp 56k)",
	[USB_TYPEC_SOURCE_MEDIUM]						= "SOURCE_MEDIUM (Rp 22k)",
	[USB_TYPEC_SOURCE_HIGH]							= "SOURCE_HIGH (Rp 10k)",
	[USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT]		= "SOURCE_DEBUG_ACCESSORY_DEFAULT (56k/56k)",
	[USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT_MEDIUM]	= "SOURCE_DEBUG_ACCESSORY_DEFAULT_MEDIUM (56k/22k)",
	[USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT_HIGH]		= "SOURCE_DEBUG_ACCESSORY_DEFAULT_HIGH (56k/10k)",
	[USB_TYPEC_SOURCE_DEBUG_ACCESSORY_MEDIUM]		= "SOURCE_DEBUG_ACCESSORY_MEDIUM (22k/22k)",
	[USB_TYPEC_SOURCE_DEBUG_ACCESSORY_MEDIUM_HIGH]	= "SOURCE_DEBUG_ACCESSORY_MEDIUM_HIGH (22k/10k)",
	[USB_TYPEC_SOURCE_DEBUG_ACCESSORY_HIGH]			= "SOURCE_DEBUG_ACCESSORY_HIGH (10k/10k)",
	[USB_TYPEC_NON_COMPLIANT]						= "NON_COMPLIANT",
};

static void wa_psy_update_work(struct work_struct *work)
{
	struct ext_battery_chg *ext_bcdev = container_of(work, struct ext_battery_chg,
						psy_update_work);
	struct battery_chg_dev *bcdev = ext_bcdev->bcdev;
	int usbin_plugin, wlsin_plugin, usb_type, typec_mode, rc = 0;

	rc = get_veneer_param(VENEER_FEED_USB_PRESENT, &usbin_plugin);
	if (rc >= 0 && usbin_plugin != ext_bcdev->usbin_plugin) {
		ext_bcdev->usbin_plugin = usbin_plugin;
		pr_wa("usb_present changed. usbin-plugin %s\n", usbin_plugin ? "attached" : "detached");
		wa_usbin_plugin_cb(bcdev);
	}

	rc = get_veneer_param(VENEER_FEED_WLS_PRESENT, &wlsin_plugin);
	if (rc >= 0 && wlsin_plugin != ext_bcdev->wlsin_plugin) {
		if (wlsin_plugin)
			wa_avoid_inrush_current_skip(bcdev);
		ext_bcdev->wlsin_plugin = wlsin_plugin;
		pr_wa("wls_present changed. wlsin-plugin %s\n", wlsin_plugin ? "attached" : "detached");
		wa_wlsin_plugin_cb(bcdev);
	}

	rc = get_veneer_param(VENEER_FEED_USB_TYPE, &usb_type);
	if (rc >= 0 && usb_type != ext_bcdev->usb_type) {
		pr_wa("usb_type changed. usb type is %s\n", POWER_SUPPLY_USB_TYPE_TEXT[usb_type]);
		ext_bcdev->usb_type = usb_type;
		wa_source_change_cb(bcdev);
	}

	rc = get_veneer_param(VENEER_FEED_TYPEC_MODE, &typec_mode);
	if (rc >= 0 && typec_mode != ext_bcdev->typec_mode) {
		ext_bcdev->typec_mode = typec_mode;
		pr_wa("typec_mode changed. typec is %s\n", POWER_SUPPLY_TYPEC_MODE_TEXT[typec_mode]);
		wa_typec_change_cb(bcdev);
	}

	wa_psy_change_cb(bcdev);
}

static int wa_notifier_call(struct notifier_block *nb,
		unsigned long ev, void *v)
{
	struct ext_battery_chg *ext_bcdev = container_of(nb, struct ext_battery_chg, nb);

	schedule_work(&ext_bcdev->psy_update_work);

	return NOTIFY_OK;
}

void wa_helper_init(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int rc;

	INIT_WORK(&ext_bcdev->psy_update_work, wa_psy_update_work);

	ext_bcdev->usbin_plugin = -1;
	ext_bcdev->wlsin_plugin = -1;
	ext_bcdev->usb_type = -1;
	ext_bcdev->typec_mode = -1;

	wa_rerun_apsd_for_sdp_init(bcdev);
	wa_faster_try_cp_qc30_init(bcdev);
	wa_support_weak_supply_init(bcdev);
	wa_avoid_inrush_current_init(bcdev);

	ext_bcdev->nb.notifier_call = wa_notifier_call;
	rc = power_supply_reg_notifier(&ext_bcdev->nb);
	if (rc < 0)
		pr_wa("Couldn't register psy notifier rc = %d\n", rc);
}
