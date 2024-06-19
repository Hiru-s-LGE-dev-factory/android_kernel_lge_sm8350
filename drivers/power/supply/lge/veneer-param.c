#define pr_fmt(fmt) "CHG: [VENEER] %s: " fmt, __func__
#define pr_veneer(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#define pr_dbg_veneer(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#include <linux/thermal.h>
#include "veneer-primitives.h"

struct veneer_param {
	enum veneer_feed_type type;
	int prop;
	const char* prop_str;
	int value;
	int rate;
};

veneer_backup_type veneer_backup = {0, };

#define __VP(_type)						{ .type = _type, .value = LGE_INITVAL, .rate = 1 }
#define __VP_PROP(_type, _prop)			{ .type = _type, .prop = _prop, .value = LGE_INITVAL, .rate = 1 }
#define __VP_RATE(_type, _prop, _rate)	{ .type = _type, .prop = _prop, .value = LGE_INITVAL, .rate = _rate }

struct veneer_param param_list[] = {
	[VENEER_FEED_ACTM_MODE]							= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_MODE),
	[VENEER_FEED_ACTM_MODE_NOW]						= __VP(VENEER),
	[VENEER_FEED_ACTM_LCDON_TEMP_OFFSET]			= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_LCDON_OFFSET),
	[VENEER_FEED_ACTM_SENSOR_WIRED]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_SENSOR_WIRED),
	[VENEER_FEED_ACTM_SENSOR_WIRELESS]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_SENSOR_WIRELESS),
	[VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRED_0]	= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_HOLDDEG_WIRED_0),
	[VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRED_1]	= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_HOLDDEG_WIRED_1),
	[VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRED_2]	= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_HOLDDEG_WIRED_2),
	[VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRELESS_0]	= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_0),
	[VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRELESS_1]	= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_1),
	[VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRELESS_2]	= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_2),
	[VENEER_FEED_ACTM_TEMPOFFS_WIRED_0]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_0),
	[VENEER_FEED_ACTM_TEMPOFFS_WIRED_1]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_1),
	[VENEER_FEED_ACTM_TEMPOFFS_WIRED_2]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_2),
	[VENEER_FEED_ACTM_TEMPOFFS_WIRELESS_0]			= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_0),
	[VENEER_FEED_ACTM_TEMPOFFS_WIRELESS_1]			= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_1),
	[VENEER_FEED_ACTM_TEMPOFFS_WIRELESS_2]			= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_2),
	[VENEER_FEED_ACTM_MAX_FCC_PPS]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_MAX_FCC_PPS),
	[VENEER_FEED_ACTM_MAX_FCC_QC3]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_MAX_FCC_QC3),
	[VENEER_FEED_ACTM_MAX_FCC_QC2]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_MAX_FCC_QC2),
	[VENEER_FEED_ACTM_CURRENT_WIRED_0]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_CURRENT_WIRED_0),
	[VENEER_FEED_ACTM_CURRENT_WIRED_1]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_CURRENT_WIRED_1),
	[VENEER_FEED_ACTM_CURRENT_WIRED_2]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_CURRENT_WIRED_2),
	[VENEER_FEED_ACTM_CURRENT_CP_PPS]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_CURR_CP_PPS),
	[VENEER_FEED_ACTM_CURRENT_CP_QC30]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_CURR_CP_QC30),
	[VENEER_FEED_ACTM_CURRENT_EPP_0]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_POWER_EPP_0),
	[VENEER_FEED_ACTM_CURRENT_EPP_1]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_POWER_EPP_1),
	[VENEER_FEED_ACTM_CURRENT_EPP_2]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_POWER_EPP_2),
	[VENEER_FEED_ACTM_CURRENT_BPP_0]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_POWER_BPP_0),
	[VENEER_FEED_ACTM_CURRENT_BPP_1]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_POWER_BPP_1),
	[VENEER_FEED_ACTM_CURRENT_BPP_2]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_ACTM_POWER_BPP_2),
	[VENEER_FEED_STATUS_RAW]						= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_STATUS_RAW),
	[VENEER_FEED_CAPACITY]							= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_CAPACITY),
	[VENEER_FEED_LG_UI_SOC_255]						= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_EXT_LG_UI_SOC_255),
	[VENEER_FEED_QBG_MSOC_X100]						= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_EXT_QBG_MSOC_X100),
	[VENEER_FEED_QBG_SYS_SOC_X100]					= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_EXT_QBG_SYS_SOC_X100),
	[VENEER_FEED_CHARGE_TYPE]						= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_CHARGE_TYPE),
	[VENEER_FEED_CHARGER_TYPE]						= __VP(OTHER),
	[VENEER_FEED_SENSOR_BATT]						= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_TEMP),
	[VENEER_FEED_SENSOR_VTS]						= __VP(OTHER),
	[VENEER_FEED_FCC]								= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT),
	[VENEER_FEED_IDC]								= __VP_PROP(PSY_WLS, POWER_SUPPLY_PROP_CURRENT_MAX),
	[VENEER_FEED_VDC]								= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_VOLTAGE_MAX),
	[VENEER_FEED_IRC_ENABLED]						= __VP_PROP(UNI_NODE, UNIFIED_NODE_IRC_ENABLED),
	[VENEER_FEED_IRC_RESISTANCE]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_IRC_RESISTANCE),
	[VENEER_FEED_LCDON_STATUS]						= __VP_PROP(UNI_NODE, UNIFIED_NODE_STATUS_LCD),
	[VENEER_FEED_POWER_SUPPLY_CHANGED]				= __VP(OTHER),
	[VENEER_FEED_BATT_PROFILE_FCC_VOTER]			= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX),
	[VENEER_FEED_BATT_PROFILE_FV_VOTER]				= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_VOLTAGE_MAX),
	[VENEER_FEED_POWER_NOW]							= __VP(OTHER),
	[VENEER_FEED_BSM_TTF]							= __VP_PROP(UNI_NODE, UNIFIED_NODE_BSM_TIMETOFULL),
	[VENEER_FEED_BSM_ENABLE]						= __VP_PROP(UNI_NODE, UNIFIED_NODE_BSM_ENABLE),
	[VENEER_FEED_TTF]								= __VP_PROP(UNI_NODE, UNIFIED_NODE_TIME_TO_FULL_NOW),
	[VENEER_FEED_BATT_PSY_IBAT_NOW]					= __VP_RATE(PSY_BATT, POWER_SUPPLY_PROP_CURRENT_NOW, 1000),
	[VENEER_FEED_USB_RESISTANCE_ID]					= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_EXT_RESISTANCE_ID),
	[VENEER_FEED_TYPEC_MODE]						= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_EXT_TYPEC_MODE),
	[VENEER_FEED_BAT_VOLTAGE_NOW]					= __VP_RATE(PSY_BATT, POWER_SUPPLY_PROP_VOLTAGE_NOW, 1000),
	[VENEER_FEED_USB_PRESENT]						= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_PRESENT),
	[VENEER_FEED_DEBUG_BATT]						= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_EXT_DEBUG_BATTERY),
	[VENEER_FEED_USB_TYPE]							= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_USB_TYPE),
	[VENEER_FEED_CHARGING]							= __VP(OTHER),
	[VENEER_FEED_FAKE_HVDCP]						= __VP_PROP(UNI_NODE, UNIFIED_NODE_FAKE_BATTERY),
	[VENEER_FEED_CHARGER_VERBOSE]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_CHARGER_VERBOSE),
	[VENEER_FEED_CHARGER_INCOMPATIBLE]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_CHARGER_INCOMPATIBLE),
	[VENEER_FEED_CHARGER_HIGH_SPEED]				= __VP_PROP(UNI_NODE, UNIFIED_NODE_CHARGER_HIGHSPEED),
	[VENEER_FEED_SUPPORT_FASTPL]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_SUPPORT_FASTPL),
	[VENEER_FEED_USB_REAL_TYPE]						= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_EXT_REAL_TYPE),
	[VENEER_FEED_HVDCP_TYPE]						= __VP(OTHER),
	[VENEER_FEED_FLOATED]							= __VP(OTHER),
	[VENEER_FEED_USB3P0]							= __VP(VENEER),
	[VENEER_FEED_DC_PRESENT]						= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_ONLINE),
	[VENEER_FEED_FORCE_UPDATE]						= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_EXT_FORCE_UPDATE),
	[VENEER_FEED_CP_ENABLED]						= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_EXT_CP_ENABLED),
	[VENEER_FEED_USB_POWER_NOW]						= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_EXT_INPUT_POWER_NOW),
	[VENEER_FEED_DC_POWER_NOW]						= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_POWER_NOW),
	[VENEER_FEED_PSEUDO_HEALTH]						= __VP(VENEER),
	[VENEER_FEED_MINCAP]							= __VP(VENEER),
	[VENEER_FEED_FULLRAW]							= __VP(VENEER),
	[VENEER_FEED_CHARING_SHOWCASE]					= __VP_PROP(UNI_NODE, UNIFIED_NODE_CHARGING_SHOWCASE),
	[VENEER_FEED_CHARGE_STATUS]						= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_EXT_CHARGE_STATUS),
	[VENEER_FEED_USB_AICL]							= __VP_PROP(PSY_USB, POWER_SUPPLY_PROP_EXT_INPUT_CURRENT_SETTLED),
	[VENEER_FEED_OCV]								= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_VOLTAGE_OCV),
	[VENEER_FEED_FAKE_ONLINE]						= __VP(OTHER),
	[VENEER_FEED_DC_VOLTAGE_MAX_DESIGN]				= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN),
	[VENEER_FEED_DC_VOLTAGE_NOW]					= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_VOLTAGE_NOW),
	[VENEER_FEED_DC_CURRENT_NOW]					= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_CURRENT_NOW),
	[VENEER_FEED_WLS_PRESENT]						= __VP_PROP(PSY_WLS, POWER_SUPPLY_PROP_PRESENT),
	[VENEER_FEED_WLS_CHARGE_PAUSE]					= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_EXT_CHARGE_PAUSE),
	[VENEER_FEED_HVT_SOC_RESCALE_COUNT]				= __VP(VENEER),
	[VENEER_FEED_VENEER_BACKUP_LOAD]				= __VP(OTHER),
	[VENEER_FEED_VENEER_BACKUP_ID]					= __VP(VENEER),
	[VENEER_FEED_VENEER_BACKUP_DATA]				= __VP(OTHER),
	[VENEER_FEED_VENEER_BACKUP_FLUSH]				= __VP(OTHER),
	[VENEER_FEED_SENSOR_BATT_RAW]					= __VP_PROP(PSY_BATT, POWER_SUPPLY_PROP_EXT_TEMP_RAW),
	[VENEER_FEED_DC_FORCE_UPDATE]					= __VP_PROP(PSY_DC, POWER_SUPPLY_PROP_EXT_FORCE_UPDATE),
};

struct power_supply* get_power_supply(struct veneer* veneer_me, enum veneer_feed_type ftype)
{
	struct power_supply* psy = NULL;

	if (!veneer_me)
		return psy;

	switch (ftype) {
	case PSY_BATT:
		psy = get_psy_battery(veneer_me);
		break;
	case PSY_USB:
		psy = get_psy_usb(veneer_me);
		break;
	case PSY_WLS:
		psy = get_psy_wireless(veneer_me);
		break;
	case PSY_CP:
		psy = get_psy_cp(veneer_me);
		break;
	case PSY_DC:
		psy = get_psy_dc(veneer_me);
		break;
	default :
		break;
	}

	return psy;
}

static int get_actm_current_cp(int type)
{
	int ret = -9999;
	int cp_enable, charger;
	enum unified_node_id uni = UNIFIED_NODE_ACTM_CURR_CP_PPS;

	if (type == CHARGING_SUPPLY_DCP_QC3)
		uni = UNIFIED_NODE_ACTM_CURR_CP_QC30;

	if (!get_veneer_param(VENEER_FEED_CP_ENABLED, &cp_enable)
			&& !get_veneer_param(VENEER_FEED_CHARGER_TYPE, &charger)) {
		if (cp_enable && charger == type)
			unified_nodes_read(uni, &ret);
	}

	return ret;
}

static int get_sensor_vts(void)
{
	struct thermal_zone_device*	tzd = thermal_zone_get_zone_by_name("vts-virt-therm");
	int ret = -9999;
	int temp, rc = 0;

	if(!tzd)
		return ret;

	rc = thermal_zone_get_temp(tzd, &temp);
	if (!rc)
		ret = temp / 100;

	return ret;
}

static int get_power_now(struct veneer* veneer_me)
{
	enum veneer_feed_id vfi = 0;
	int ret = 0, power = 0;

	if (veneer_me->presence_usb)
		vfi = VENEER_FEED_USB_POWER_NOW;
	else if (veneer_me->presence_wireless)
		vfi = VENEER_FEED_DC_POWER_NOW;
	else
		return ret;

	if (!get_veneer_param(vfi, &power))
		ret = power / 1000;

	return ret;
}

static int get_special_charger(int id)
{
	int val = -1, ret = 0;

	if (get_veneer_param(VENEER_FEED_USB_REAL_TYPE, &val))
		return ret;

	switch (val) {
		case CHARGER_TYPE_SNK_USB_QC_2P0 :
			if (id == VENEER_FEED_HVDCP_TYPE)
				ret = QC20;
			break;
		case CHARGER_TYPE_SNK_USB_QC_3P0 :
			if (id == VENEER_FEED_HVDCP_TYPE)
				ret = QC30;
			break;

		case CHARGER_TYPE_SNK_USB_FLOAT :
			if (id == VENEER_FEED_FLOATED)
				ret = true;
			break;

	}

	return ret;
}

static int load_veneer_backup(void)
{
	int rc = 0;

	rc = lge_get_veneer_backup((void *)&veneer_backup);
	if (rc) {
		pr_veneer("get veneer backup error.. rc=%d\n", rc);
		return -1;
	}
	if (!veneer_backup.is_good_cookie) {
		pr_veneer("get veneer backup is_good_cookie error\n");
		return -2;
	}

	return 0;
}

static int flush_veneer_backup(void)
{
	int rc = 0;
	unsigned long now = 0;

	veneer_backup.data[VENEER_BACKUP_ID_HVT_BACKUP_TIME] = 0;
	if (veneer_backup.data[VENEER_BACKUP_ID_HVT_COUNT] > 0) {
		get_rtc_time(&now);
		veneer_backup.data[VENEER_BACKUP_ID_HVT_BACKUP_TIME] = (unsigned int) now;
	}

	rc = lge_set_veneer_backup((void *)&veneer_backup);

	return rc;
}

static int get_veneer_backup(int id, int *value)
{
	*value = (int) veneer_backup.data[id];
	return 0;
}

static int set_veneer_backup(int id, int value)
{
	veneer_backup.data[id] = (uint32_t) value;
	return 0;
}

int get_other_param(struct veneer* veneer_me, int id, int *val)
{
	int rc = 0;

	*val = -9999;
	if (!veneer_me)
		return -1;

	switch (id) {
		case VENEER_FEED_ACTM_CURRENT_CP_PPS :
			*val = get_actm_current_cp(CHARGING_SUPPLY_USB_PD_PPS);
			break;

		case VENEER_FEED_ACTM_CURRENT_CP_QC30 :
			*val = get_actm_current_cp(CHARGING_SUPPLY_DCP_QC3);
			break;

		case VENEER_FEED_SENSOR_VTS :
			*val = get_sensor_vts();
			break;

		case VENEER_FEED_POWER_NOW :
			*val = get_power_now(veneer_me);
			break;

		case VENEER_FEED_CHARGER_TYPE :
			*val = (int) veneer_me->veneer_supplier;
			break;

		case VENEER_FEED_CHARGING :
			*val = veneer_me->presence_usb || veneer_me->presence_wireless;
			break;

		case VENEER_FEED_FAKE_ONLINE :
			if (veneer_voter_suspended(VOTER_TYPE_IUSB) == CHARGING_SUSPENDED_WITH_FAKE_OFFLINE)
				*val = true;
			else if (veneer_me->usb_typec_mode == USB_TYPEC_SINK
					|| veneer_me->usb_typec_mode == USB_TYPEC_SINK_POWERED_CABLE)
				*val = true;
			else
				*val = false;
			break;

		case VENEER_FEED_HVDCP_TYPE :
		case VENEER_FEED_FLOATED :
			*val = get_special_charger(id);
			break;

		case VENEER_FEED_VENEER_BACKUP_DATA:
			rc = get_veneer_backup(
					param_list[VENEER_FEED_VENEER_BACKUP_ID].value, val);
			break;

		default :
			break;
	}

	return rc;
}

int get_latest_veneer_param(int id, int *val)
{
	struct veneer_param *vp = &param_list[id];

	if (vp->value == LGE_INITVAL)
		*val = -9999;
	else
		*val = vp->value;

	return 0;
}

int get_veneer_param(int id, int *val)
{
	struct veneer* veneer_me = veneer_data_fromair();
	struct veneer_param *vp = &param_list[id];
	union power_supply_propval prop = { .intval = 0, };
	struct power_supply* psy = NULL;
	int rc = 0;

	*val = -9999;
	switch (vp->type) {
		case UNI_NODE:
			unified_nodes_read(vp->prop, val);
			break;

		case VENEER:
			*val = vp->value;
			break;

		case OTHER:
			rc = get_other_param(veneer_me, id, val);
			break;

		default :
			if (!veneer_me)
				return -1;

			psy = get_power_supply(veneer_me, vp->type);
			if (psy) {
				rc = power_supply_get_property(psy,
					vp->prop, &prop);
				if (!rc)
					*val = prop.intval / vp->rate;
			} else
				rc = -1;
			break;

	}

	if (*val != -9999)
		vp->value = *val;

	return rc;
}

int set_other_param(struct veneer* veneer_me, int id, int val)
{
	struct power_supply* psy = NULL;
	int rc = 0;

	if (!veneer_me)
		return -1;

	switch (id) {
		case VENEER_FEED_POWER_SUPPLY_CHANGED:
			psy = get_power_supply(veneer_me, PSY_BATT);
			if (psy)
				power_supply_changed(psy);
			break;

		case VENEER_FEED_VENEER_BACKUP_LOAD:
			rc = load_veneer_backup();
			break;

		case VENEER_FEED_VENEER_BACKUP_DATA:
			rc = set_veneer_backup(param_list[VENEER_FEED_VENEER_BACKUP_ID].value, val);
			break;

		case VENEER_FEED_VENEER_BACKUP_FLUSH:
			rc = flush_veneer_backup();
			break;

		default :
			break;
	}

	return rc;
}

int set_veneer_param(int id, int val)
{
	int rc = 0;
	union power_supply_propval prop = { .intval = 0, };
	struct veneer* veneer_me = veneer_data_fromair();
	struct veneer_param *vp = &param_list[id];
	struct power_supply* psy = NULL;

	switch (vp->type) {
		case UNI_NODE:
			rc = unified_nodes_write(vp->prop, val);
			break;

		case VENEER:
			vp->value = val;
			break;

		case OTHER:
			rc = set_other_param(veneer_me, id, val);
			break;

		default :
			if (!veneer_me)
				return -1;

			psy = get_power_supply(veneer_me, vp->type);
			if (psy) {
				prop.intval = val * vp->rate;
				rc = power_supply_set_property(psy,
					vp->prop, &prop);
			}
			break;

	}
	return rc;
}
