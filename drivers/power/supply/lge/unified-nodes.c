#define pr_fmt(fmt) "CHG: [UNINODE] %s: " fmt, __func__
#define pr_uninode(fmt, ...) pr_err(fmt, ##__VA_ARGS__)
#define pr_dbg_uninode(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)

#define UNIFIED_NODES_DEVICE	"lge-unified-nodes"
#define UNIFIED_NODES_DISABLED	-1000000

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#ifdef CONFIG_MACH_LITO_CAYMANLM_LAO_COM
#include <soc/qcom/lge/board_lge.h>
#endif

#include "veneer-primitives.h"

#define VOTER_NAME_THERMALD	"THERMALD"
#define VOTER_NAME_HIDDENM	"HIDDENM"
#define VOTER_NAME_CCD		"CCD"
#define VOTER_NAME_CCD_INPUT		"CCD_INPUT"
#define VOTER_NAME_CCD_WLC			"CCD_WLC"
#define VOTER_NAME_CCD_BATCHG		"CCD_BATCHG"
#define RESTRICTION_MAX_COUNT	8
#define RESTRICTION_MAX_NAME	32
#if defined(CONFIG_QPNP_SMB5) || defined(CONFIG_QPNP_QG)
#define CCD_SUSPEND	1
#else
#define CCD_SUSPEND	0
#endif

static struct device_attribute unified_nodes_dattrs[];
struct unified_nodes {
	struct device_node* devnode_restrictchg;

	struct voter_entry		thermald_iusb;
	struct voter_entry		thermald_ibat;
	struct voter_entry		thermald_idc;
	struct voter_entry		thermald_vdc;
	struct voter_entry		charging_restriction [RESTRICTION_MAX_COUNT];
	struct voter_entry		charging_enable [2]; // 0 for iusb, 1 for idc
	const char*			charging_step;
	int						time_to_full_now;
	int						ttf_capacity;
	int						aicl_done;
	struct voter_entry		ccd_icl;
	struct voter_entry		ccd_fcc;
	struct voter_entry		ccd_vfloat;
	struct voter_entry		ccd_input_suspend;
	struct voter_entry		ccd_wlc_suspend;
	struct voter_entry		ccd_batchg_en;
	bool				charging_showcase;
	void*				charging_completed;
	int				status_boot;
	bool				status_lcd;
	bool				fake_battery;
	bool				fake_battery_temp_en;
	bool				fake_battery_soc_en;
	bool				fake_battery_volt_en;
	int				fake_battery_temp;
	int				fake_battery_soc;
	int				fake_battery_volt;
	bool				fake_sdpmax;
	bool				fake_hvdcp;
	void*				battery_age;
	void*				battery_condition;
	void*				battery_cycle;
	bool				battery_valid;
	bool				battery_condition_tc_mode;
	int				battery_condition_tc_value;
	char				charger_name [64];
	bool				charger_highspeed;
	bool*				charger_incompatible;
	void*				charger_usbid;
	enum charging_verbosity*	charger_verbose;
	// Supporting features : used for verifying device
	int				support_fastpl;
	bool				support_fastchg;
	const char* fake_psy;
	const char* battage_psy;
	struct power_supply*		bat_psy;
	struct power_supply*		usb_psy;
	struct power_supply*		wls_psy;

	const char* ttf_psy;
	int				bsm_timetofull;
	int				bsm_enable;
	int				cbc_cycle;
	struct thermal_zone_device *xo_tz;
	struct thermal_zone_device *bd_tz;
	int actm[MAX_UNIFIED_NODES_SIZE];

	int irc_enabled;
	int irc_resistance;
};

static void voter_unregister(struct unified_nodes* uninodes)
{
	int i;
	struct voter_entry* restriction;

	// For Restriction
	for (i = 0; i < RESTRICTION_MAX_COUNT; i++) {
		restriction = &uninodes->charging_restriction[i];
		if (restriction->type != VOTER_TYPE_INVALID)
			veneer_voter_unregister(restriction);
	}
	// For Thermald
	veneer_voter_unregister(&uninodes->thermald_iusb);
	veneer_voter_unregister(&uninodes->thermald_ibat);
	veneer_voter_unregister(&uninodes->thermald_idc);
	// For Hiddenm
	veneer_voter_unregister(&uninodes->charging_enable[0]); // 0 for iusb,
	veneer_voter_unregister(&uninodes->charging_enable[1]); // 1 for idc,
	//For CCD
	veneer_voter_unregister(&uninodes->ccd_icl);
	veneer_voter_unregister(&uninodes->ccd_fcc);
	veneer_voter_unregister(&uninodes->ccd_vfloat);
	veneer_voter_unregister(&uninodes->ccd_input_suspend);
	veneer_voter_unregister(&uninodes->ccd_wlc_suspend);
	veneer_voter_unregister(&uninodes->ccd_batchg_en);
}

static bool voter_register(struct unified_nodes* uninodes)
{
	bool ret = true;

// For Sysnode
	struct device_node* child;
	int i = 0;
	char* restrict_name;
	char* restrict_type;
	struct voter_entry* restrict_voter;

	for_each_child_of_node(uninodes->devnode_restrictchg, child) {
		if (i < RESTRICTION_MAX_COUNT) {
			restrict_voter = &uninodes->charging_restriction[i];

			if (ret && !of_property_read_string(child, "restrict-name", (const char **) &restrict_name)
				&& !of_property_read_string(child, "restrict-type", (const char **) &restrict_type)
				&& strlen(restrict_name) < RESTRICTION_MAX_NAME) {
				if (!strncmp(restrict_type, "actm", 4)) {
					strlcpy(restrict_voter->name, restrict_name, VOTER_NAME_LENGTH);
					restrict_voter->type = vote_fromdt(restrict_type);
				} else {
					ret = ret && veneer_voter_register(restrict_voter, restrict_name, vote_fromdt(restrict_type), false);
				}
			}
			else {
				pr_uninode("Parsing error on restricted charging nodes\n");
				return false;
			}
			i++;
		}
		else {
			pr_uninode("Overflow! Check the count of restrictions\n");
			return false;
		}
	}

	ret = ret
// For Thermald
	&& veneer_voter_register(&uninodes->thermald_iusb, VOTER_NAME_THERMALD, VOTER_TYPE_IUSB, false)
	&& veneer_voter_register(&uninodes->thermald_ibat, VOTER_NAME_THERMALD, VOTER_TYPE_IBAT, false)
	&& veneer_voter_register(&uninodes->thermald_idc, VOTER_NAME_THERMALD, VOTER_TYPE_IDC, false)
// For Hiddenm
	&& veneer_voter_register(&uninodes->charging_enable[0], VOTER_NAME_HIDDENM, VOTER_TYPE_IUSB, true)
	&& veneer_voter_register(&uninodes->charging_enable[1], VOTER_NAME_HIDDENM, VOTER_TYPE_IDC, true);

	if (!ret) {
		pr_uninode("failed to register voters\n");
	}

// For CCD
	ret |= veneer_voter_register(&uninodes->ccd_icl, VOTER_NAME_CCD, VOTER_TYPE_IUSB, false);
	ret |= veneer_voter_register(&uninodes->ccd_fcc, VOTER_NAME_CCD, VOTER_TYPE_IBAT, false);
	ret |= veneer_voter_register(&uninodes->ccd_vfloat, VOTER_NAME_CCD, VOTER_TYPE_VFLOAT, false);
	ret |= veneer_voter_register(&uninodes->ccd_input_suspend, VOTER_NAME_CCD_INPUT, VOTER_TYPE_IUSB, false);
	ret |= veneer_voter_register(&uninodes->ccd_wlc_suspend, VOTER_NAME_CCD_WLC, VOTER_TYPE_IDC, false);
	ret |= veneer_voter_register(&uninodes->ccd_batchg_en, VOTER_NAME_CCD_BATCHG, VOTER_TYPE_IBAT, false);

	if (!ret) {
		pr_uninode("failed to register ccd voters\n");
	}

	return ret;
}

static ssize_t voter_store(struct voter_entry* voter, const char* buf, size_t size)
{
	long limit;
	pr_uninode("%s(%s) writes %s\n", voter->name, vote_title(voter->type), buf);

	if ( !(kstrtol(buf, 10, &limit)) && (0<=limit) ) {
		// uninodes->voter_thermald_iusb->limit will be updated to limit_ma
		// after veneer_voter_set
		veneer_voter_set(voter, limit);
	}
	else
		veneer_voter_release(voter);

	return size;
}

static ssize_t voter_show(struct voter_entry* voter, char* buf)
{
	return snprintf(buf, PAGE_SIZE, "%d", voter->limit);
}

static struct unified_nodes* get_unified_nodes(struct device* dev)
{
	struct unified_nodes* uninodes = NULL;

	if (dev && dev->platform_data)
		uninodes = (struct unified_nodes*) dev->platform_data;

	return uninodes;
}

static int bat_get_property(struct unified_nodes* uninodes, enum power_supply_property psp, int error)
{
	union power_supply_propval	prp = { .intval = 0, };

	if (!uninodes)
		return error;

	if (!uninodes->bat_psy)
		uninodes->bat_psy = power_supply_get_by_name("battery");

	if (!uninodes->bat_psy)
		return error;

	power_supply_get_property(uninodes->bat_psy, psp, &prp);

	return prp.intval;
}

static int bat_set_property(struct unified_nodes* uninodes, enum power_supply_property psp, const char* buf)
{
	union power_supply_propval	prp = { .intval = 0, };

	if (!uninodes)
		return -1;

	if (!uninodes->bat_psy)
		uninodes->bat_psy = power_supply_get_by_name("battery");

	if (!uninodes->bat_psy)
		return -1;

	sscanf(buf, "%d", &prp.intval);
	power_supply_set_property(uninodes->bat_psy, psp, &prp);

	return prp.intval;
}

static int usb_get_property(struct unified_nodes* uninodes, enum power_supply_property psp, int error)
{
	union power_supply_propval	prp = { .intval = 0, };

	if (!uninodes)
		return error;

	if (!uninodes->usb_psy)
		uninodes->usb_psy = power_supply_get_by_name("usb");

	if (!uninodes->usb_psy)
		return error;

	if (psp == POWER_SUPPLY_PROP_TYPE)
		return uninodes->usb_psy->desc->type;

	power_supply_get_property(uninodes->usb_psy, psp, &prp);

	return prp.intval;
}

static int usb_set_property(struct unified_nodes* uninodes, enum power_supply_property psp, const char* buf)
{
	union power_supply_propval	prp = { .intval = 0, };

	if (!uninodes)
		return -1;

	if (!uninodes->usb_psy)
		uninodes->usb_psy = power_supply_get_by_name("usb");

	if (!uninodes->usb_psy)
		return -1;

	sscanf(buf, "%d", &prp.intval);
	power_supply_set_property(uninodes->usb_psy, psp, &prp);

	return prp.intval;
}

static int wls_get_property(struct unified_nodes* uninodes, enum power_supply_property psp, int error)
{
	union power_supply_propval	prp = { .intval = 0, };

	if (!uninodes)
		return error;

	if (!uninodes->wls_psy)
		uninodes->wls_psy = power_supply_get_by_name("wireless");

	if (!uninodes->wls_psy)
		return error;

	power_supply_get_property(uninodes->wls_psy, psp, &prp);

	return prp.intval;
}

static int wls_set_property(struct unified_nodes* uninodes, enum power_supply_property psp, const char* buf)
{
	union power_supply_propval	prp = { .intval = 0, };

	if (!uninodes)
		return -1;

	if (!uninodes->wls_psy)
		uninodes->wls_psy = power_supply_get_by_name("wireless");

	if (!uninodes->wls_psy)
		return -1;

	sscanf(buf, "%d", &prp.intval);
	power_supply_set_property(uninodes->wls_psy, psp, &prp);

	return prp.intval;
}


void unified_node_read_s32(const struct device_node *np,
	const char *propname, u32 *out_values, int init)
{
	int ret = 0;

	ret = of_property_read_s32(np, propname, out_values);
	if (ret) {
		pr_uninode("Failed to read '%s'(%d)\n", propname, ret);
		*out_values = init;
	}
}

void unified_node_read_u32_array(const struct device_node *np,
	const char *propname, u32 *out_values, size_t sz, int init)
{
	int ret = 0;

	ret = of_property_read_u32_array(np, propname, out_values, sz);
	if (ret) {
		pr_uninode("Failed to read '%s'(%d)\n", propname, ret);
		memset(out_values, init, sizeof(int) * sz);
	}
}

static bool unified_nodes_actm_dt(
	struct device_node* devnode, struct unified_nodes* uninodes)
{
	struct device_node* dnode = devnode->parent;
	struct device_node* devnode_actm;
	struct device_node* devnode_bvp;

	bool actm_enable = false;
	int actm_array_size = 0;

	devnode_actm = of_find_node_by_name(dnode, "adaptive-charging-thermal");
	devnode_bvp = of_find_node_by_name(dnode, "protection-batvolt");

	uninodes->irc_enabled = of_property_read_bool(devnode_bvp, "lge,irc-enable");
	unified_node_read_s32(devnode_bvp, "lge,irc-resist-mohm", &uninodes->irc_resistance, 0);

	actm_enable = of_property_read_bool(devnode_actm, "lge,actm-enable");
	if (actm_enable) {
		unified_node_read_s32(devnode_actm, "lge,actm-default-mode",
			&uninodes->actm[UNIFIED_NODE_ACTM_MODE], -2);
		unified_node_read_s32(devnode_actm, "lge,actm-lcdon-temp-offset",
			&uninodes->actm[UNIFIED_NODE_ACTM_LCDON_OFFSET], 0);
		unified_node_read_u32_array(devnode_actm, "lge,wired-therm-sensor-type",
			&uninodes->actm[UNIFIED_NODE_ACTM_SENSOR_WIRED], 1, 3);
		unified_node_read_u32_array(devnode_actm, "lge,wired-max-hold-criteria",
			&uninodes->actm[UNIFIED_NODE_ACTM_HOLDDEG_WIRED_0], 3, 0);
		unified_node_read_u32_array(devnode_actm, "lge,wireless-max-hold-criteria",
			&uninodes->actm[UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_0], 3, 0);

		unified_node_read_u32_array(devnode_actm, "lge,wired-stage-size", &actm_array_size, 1, 3);
		unified_node_read_u32_array(devnode_actm, "lge,wired-temp-offset",
			&uninodes->actm[UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_0], actm_array_size, 0);
		unified_node_read_u32_array(devnode_actm, "lge,wired-current-limit-ma",
			&uninodes->actm[UNIFIED_NODE_ACTM_CURRENT_WIRED_0], actm_array_size, 0);
		unified_node_read_u32_array(devnode_actm, "lge,wired-curr-cp-limit-ma",
			&uninodes->actm[UNIFIED_NODE_ACTM_CURR_CP_PPS], 2, 0);
		unified_node_read_u32_array(devnode_actm, "lge,wired-max-fcc-ma",
			&uninodes->actm[UNIFIED_NODE_ACTM_MAX_FCC_PPS], 3, 0);

		unified_node_read_u32_array(devnode_actm, "lge,wireless-therm-sensor-type",
			&uninodes->actm[UNIFIED_NODE_ACTM_SENSOR_WIRELESS], 1, 3);

		unified_node_read_u32_array(devnode_actm, "lge,wireless-stage-size", &actm_array_size, 1, 3);
		unified_node_read_u32_array(devnode_actm, "lge,wireless-temp-offset",
			&uninodes->actm[UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_0], actm_array_size, 0);
		unified_node_read_u32_array(devnode_actm, "lge,wireless-epp-power-limit-mw",
			&uninodes->actm[UNIFIED_NODE_ACTM_POWER_EPP_0], actm_array_size, 0);
		unified_node_read_u32_array(devnode_actm, "lge,wireless-bpp-power-limit-mw",
			&uninodes->actm[UNIFIED_NODE_ACTM_POWER_BPP_0], actm_array_size, 0);
	}
	else {
		memset(uninodes->actm, 0, sizeof(int) * MAX_UNIFIED_NODES_SIZE);
		uninodes->actm[UNIFIED_NODE_ACTM_MODE] = -2;
	}

	return true;
}

static bool unified_nodes_devicetree(struct device_node* devnode, struct unified_nodes* uninodes)
{
	struct device_node* devnode_fakebatt = NULL;
	struct device_node* devnode_battage = NULL;
	struct device_node* devnode_ttf  = NULL;
	struct device_node* devnode_thermal = NULL;

	const char* xo_therm_name = NULL;
	const char* bd_therm_name = NULL;
	int buf = 0;
	int ret = 0;

	bool result = true;

	uninodes->support_fastpl = (!of_property_read_u32(devnode, "lge,feature-charging-parallel", &buf)
		&& buf == 1) ? 0 : -1; /* if parallel charging is not supported, set it to '-1' */
	uninodes->support_fastchg = !of_property_read_u32(devnode, "lge,feature-charging-highspeed", &buf)
		? !!buf : 0;

	uninodes->devnode_restrictchg = of_find_node_by_name(devnode, "lge,restrict-charging");
	devnode_fakebatt = of_find_node_by_name(devnode, "lge,fake-battery");
	devnode_battage = of_find_node_by_name(devnode, "lge,battery-age");
	devnode_ttf = of_find_node_by_name(devnode, "lge,ttf");
	devnode_thermal = of_find_node_by_name(devnode, "lge,thermal-zone");

	ret = of_property_read_string(devnode_fakebatt, "fakebatt-psy", &uninodes->fake_psy);
	if (ret != 0) {
		pr_uninode("Failed to read 'fakebatt-psy'(%d)\n", ret);
		result = false;
	}

	uninodes->fake_battery_temp_en = true;
	ret = of_property_read_u32(devnode_fakebatt, "fakebatt-temperature", &uninodes->fake_battery_temp);
	if (ret != 0) {
		pr_uninode("Failed to read 'fakebatt-temperature'(%d)\n", ret);
		result = false;
	}

	uninodes->fake_battery_soc_en = true;
	ret = of_property_read_u32(devnode_fakebatt, "fakebatt-capacity", &uninodes->fake_battery_soc);
	if (ret != 0) {
		pr_uninode("Failed to read 'fakebatt-capacity'(%d)\n", ret);
		result = false;
	}

	uninodes->fake_battery_volt_en = true;
	ret = of_property_read_u32(devnode_fakebatt, "fakebatt-mvoltage", &uninodes->fake_battery_volt);
	if (ret != 0) {
		pr_uninode("Failed to read 'fakebatt-mvoltage'(%d)\n", ret);
		result = false;
	}

	ret = of_property_read_string(devnode_battage, "battage-psy", &uninodes->battage_psy);
	if (ret != 0) {
		pr_uninode("Failed to read 'battage-psy'(%d)\n", ret);
		result = false;
	}

	ret = of_property_read_string(devnode_ttf, "ttf-psy", &uninodes->ttf_psy);
	if (ret != 0) {
		pr_uninode("Failed to read 'ttf-psy'(%d)\n", ret);
		//result = false;
	}

	ret = of_property_read_string(devnode_thermal, "xo-therm", &xo_therm_name);
	if (ret != 0) {
		pr_uninode("Failed to read 'xo-therm'(%d)\n", ret);
		//result = false;
	}

	ret = of_property_read_string(devnode_thermal, "bd-therm", &bd_therm_name);
	if (ret != 0) {
		pr_uninode("Failed to read 'bd-therm'(%d)\n", ret);
		//result = false;
	}

	uninodes->xo_tz= thermal_zone_get_zone_by_name(xo_therm_name);
	if (IS_ERR(uninodes->xo_tz)) {
		pr_uninode("Failed to read 'xo_tz' (%d)\n", PTR_ERR(uninodes->xo_tz));
		uninodes->xo_tz = NULL;
		//Temporary After thermal DTS complete modify
		//result = false;
	}

	uninodes->bd_tz= thermal_zone_get_zone_by_name(bd_therm_name);
	if (IS_ERR(uninodes->bd_tz)) {
		pr_uninode("Failed to read 'bd_tz' (%d)\n", PTR_ERR(uninodes->bd_tz));
		uninodes->bd_tz = NULL;
		//Temporary After thermal DTS complete modify
		//result = false;
	}

	//Initialize uninodes default value
	uninodes->battery_condition_tc_mode = false;

	return result;
}

static ssize_t status_lcd_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes*   ref;
	bool*           ori;
	int         new;

	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->status_lcd;

		sscanf(buf, "%d", &new);
		new = !!new;
		if (*ori != new)
			*ori = new;
	}

	return size;
}

static ssize_t status_lcd_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->status_lcd;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static int charging_restriction_limit(struct device_node* parent,
	const char* voter_name, const char* voter_type, const char* command)
{
	int limit = VOTE_TOTALLY_RELEASED;

	struct device_node*	child;
	for_each_child_of_node(parent, child) {
		const char	*name, *type;
		u32		val;
		int		ret;

		ret = of_property_read_string(child, "restrict-name", &name);
		if (ret != 0) {
			pr_uninode("Failed to read 'restrict-name'(%d)\n", ret);
			break;
		}

		ret = strcmp(voter_name, name);
		if (ret != 0) {
			pr_debug("Continue to search %s\n", name);
			continue;
		}

		ret = of_property_read_string(child, "restrict-type", &type);
		if (ret != 0) {
			pr_uninode("Failed to read 'restrict-type'(%d)\n", ret);
			break;
		}

		ret = strcmp(voter_type, type);
		if (ret != 0) {
			pr_debug("Continue to search %s\n", type);
			continue;
		}

		ret = of_property_match_string(child, "restrict-commands", command);
		if (ret < 0) {
			pr_uninode("Failed to read 'restrict-commands' %s, (%d)\n", command, ret);
			break;
		}

		ret = of_property_read_u32_index(child, "restrict-values", ret, &val);
		if (ret != 0) {
			pr_uninode("Failed to read 'restrict-values'[%s], (%d)\n", command, ret);
			break;
		}

		if (val != 0) {
			pr_uninode("Finally success to find limit value %d for '%s(%s) %s'\n",
				val, voter_name, voter_type, command);
			limit = (int)val;
			break;
		}
	}

	return limit;
}

static ssize_t charging_restriction_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes* uninodes;
	struct voter_entry* restrictions;
	const char* space = strchr(buf, ' ');
	struct voter_entry* iter;
	char cmd[16] = { 0, };
	int i, limit;

	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		uninodes = (struct unified_nodes*)dev->platform_data;
		restrictions = uninodes->charging_restriction;

		strncpy(cmd, space+1, 15);
		strreplace(cmd, '\n', '\0');

		for (i = 0; i < RESTRICTION_MAX_COUNT; i++) {
			iter = &restrictions[i];

			if (!strncasecmp(buf, iter->name, space-buf)) {
				limit = charging_restriction_limit(uninodes->devnode_restrictchg,
					iter->name, vote_title(iter->type), cmd);

				if (!strcmp(iter->name, "GAME") &&
					(iter->type == VOTER_TYPE_ACTM_LCDON_TEMP_OFFSET)) {
					set_veneer_param(VENEER_FEED_ACTM_LCDON_TEMP_OFFSET, limit);
				}
				else {
					if (!strcmp(iter->name, "LCD"))
						uninodes->status_lcd = !(limit == VOTE_TOTALLY_RELEASED);
					veneer_voter_set(iter, limit);
				}
				pr_uninode("%s votes %d to %s\n", iter->name, limit, vote_title(iter->type));
			}
		}
	}
	else
		pr_uninode("Error on setting restriction\n");

	return size;
}

static ssize_t charging_restriction_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	// Do print logs, but Don't return anything
	int i;
	struct voter_entry* restrictions;
	struct voter_entry* restriction;

	if (dev && dev->platform_data) {
		restrictions = ((struct unified_nodes*)dev->platform_data)->charging_restriction;

		for (i = 0; i < RESTRICTION_MAX_COUNT; i++) {
			restriction = &restrictions[i];
			if (restriction->type != VOTER_TYPE_INVALID) {
				pr_dbg_uninode("voting Name=%s, Value=%d\n", restriction->name,
					(restriction->limit == VOTE_TOTALLY_RELEASED ? -1 : restriction->limit));
			}
		}
	}

	return snprintf(buf, PAGE_SIZE, "%d", -1);
}

static ssize_t thermald_iusb_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* iusb = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->thermald_iusb.type == VOTER_TYPE_IUSB)
		iusb = &((struct unified_nodes*)dev->platform_data)->thermald_iusb;
	else
		pr_uninode("Error on getting voter\n");

	return iusb ? voter_store(iusb, buf, size) : size;
}

static ssize_t thermald_iusb_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* iusb = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->thermald_iusb.type == VOTER_TYPE_IUSB)
		iusb = &((struct unified_nodes*)dev->platform_data)->thermald_iusb;
	else
		pr_uninode("Error on getting voter\n");

	return iusb ? voter_show(iusb, buf) : 0;
}

static ssize_t thermald_ibat_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* ibat = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->thermald_ibat.type == VOTER_TYPE_IBAT)
		ibat = &((struct unified_nodes*)dev->platform_data)->thermald_ibat;
	else
		pr_uninode("Error on getting voter\n");

	return ibat ? voter_store(ibat, buf, size) : size;
}
static ssize_t thermald_ibat_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* ibat = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->thermald_ibat.type == VOTER_TYPE_IBAT)
		ibat = &((struct unified_nodes*)dev->platform_data)->thermald_ibat;
	else
		pr_uninode("Error on getting voter\n");

	return ibat ? voter_show(ibat, buf) : 0;
}

static ssize_t thermald_idc_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* idc = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->thermald_idc.type == VOTER_TYPE_IDC)
		idc = &((struct unified_nodes*)dev->platform_data)->thermald_idc;
	else
		pr_uninode("Error on getting voter\n");

	return idc ? voter_store(idc, buf, size) : size;
}

static ssize_t thermald_idc_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* idc = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->thermald_idc.type == VOTER_TYPE_IDC)
		idc = &((struct unified_nodes*)dev->platform_data)->thermald_idc;
	else
		pr_uninode("Error on getting voter\n");

	return idc ? voter_show(idc, buf) : 0;
}

static ssize_t thermald_vdc_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	wls_set_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_VOLTAGE_MAX, buf);

	return size;
}

static ssize_t thermald_vdc_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = wls_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_VOLTAGE_MAX, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t charging_enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes*	ref;
	struct voter_entry* iusb;
	struct voter_entry* idc;
	int enable = true;
	int limit;
	static int pre_enable = -1;

	sscanf(buf, "%d", &enable);
	if (pre_enable != enable) {
		pre_enable = enable;
		pr_uninode("Storing %s\n", buf);
	}

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		iusb = &ref->charging_enable[0];
		idc = &ref->charging_enable[1];

		if (iusb->type == VOTER_TYPE_IUSB && idc->type == VOTER_TYPE_IDC) {
			limit = !!enable ? VOTE_TOTALLY_RELEASED : VOTE_TOTALLY_BLOCKED;

			veneer_voter_set(iusb, limit);
			veneer_voter_set(idc, limit);
			set_veneer_param(VENEER_FEED_POWER_SUPPLY_CHANGED, 0);

			pr_uninode("Success to set charging_enabled\n");
		}
	}

	return size;
}

static ssize_t charging_enable_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 1;
	struct unified_nodes*	ref;
	struct voter_entry* iusb;
	struct voter_entry* idc;

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		iusb = &ref->charging_enable[0];
		idc = &ref->charging_enable[1];

		if (iusb->type == VOTER_TYPE_IUSB && idc->type == VOTER_TYPE_IDC
			&& iusb->limit == idc->limit) {
			ret = (iusb->limit == VOTE_TOTALLY_RELEASED);
			pr_dbg_uninode("Success to get charging_enabled (%d)\n", ret);
		}
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

enum charging_step_index {
	INHIBIT = 0,
	TRICKLE,
	PRECHARGING,
	FULLON,
	TAPER,
	EOC,
	PAUSE,
	CHARGING_DISABLED,
	INVALID,
};

static const char* const charging_step_string [] = {
	"0 INHIBIT",
	"1 TRICKLE",
	"2 PRECHARGING",
	"3 FULLON",
	"4 TAPER",
	"5 EOC",
	"6 PAUSE",
	"7 DISABLED",
	"8 INVALID",
};

static ssize_t charging_step_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	const char* ret = charging_step_string[INHIBIT];
	struct unified_nodes*	ref;
	int 		new;

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		sscanf(buf, "%d", &new);

		switch (new) {
		case CHARGE_STATUS_INHIBIT:	ret = charging_step_string[INHIBIT];
			break;
		case CHARGE_STATUS_TRICKLE:	ret = charging_step_string[TRICKLE];
			break;
		case CHARGE_STATUS_PRECHARGE:	ret = charging_step_string[PRECHARGING];
			break;
		case CHARGE_STATUS_FULLON:	ret = charging_step_string[FULLON];
			break;
		case CHARGE_STATUS_TAPER:		ret = charging_step_string[TAPER];
			break;
		case CHARGE_STATUS_TERMINATION:		ret = charging_step_string[EOC];
			break;
		case CHARGE_STATUS_PAUSE:	ret = charging_step_string[PAUSE];
			break;
		case CHARGE_STATUS_CHARGING_DISABLED:	ret = charging_step_string[CHARGING_DISABLED];
			break;
		case CHARGE_STATUS_INVALID:	ret = charging_step_string[INVALID];
			break;
		default:
			break;
		}

		ref->charging_step = ret;
	}

	return size;
}

static ssize_t charging_step_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	const char* ret = "0 DISCHG";
	struct unified_nodes*	ref;

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		if (ref->charging_step)
			ret = ref->charging_step;
	}

	return snprintf(buf, PAGE_SIZE, "%s", ret);
}

static ssize_t charging_showcase_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes*	ref;
	bool*			ori;
	int 		new;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->charging_showcase;

		sscanf(buf, "%d", &new);
		new = !!new;
		if (*ori != new) {
			*ori = new;
			protection_showcase_update();
		}
	}

	return size;
}

static ssize_t charging_showcase_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->charging_showcase;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t time_to_full_now_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	int new;
	struct unified_nodes* ref = NULL;
	struct power_supply* ttf_psy = NULL;
	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		if (ref->ttf_psy)
			ttf_psy = power_supply_get_by_name(ref->ttf_psy);

		sscanf(buf, "%d", &new);
		((struct unified_nodes*)dev->platform_data)->time_to_full_now = new;

		if (ttf_psy) {
			power_supply_changed(ttf_psy);
			power_supply_put(ttf_psy);
		}
	}

	return size;
}

static ssize_t time_to_full_now_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->time_to_full_now;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t ttf_capacity_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	int new;
	struct unified_nodes* ref = NULL;
	struct power_supply* ttf_psy = NULL;
	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		if (ref->ttf_psy)
			ttf_psy = power_supply_get_by_name(ref->ttf_psy);

		sscanf(buf, "%d", &new);
		((struct unified_nodes*)dev->platform_data)->ttf_capacity = new;

		if (ttf_psy) {
			power_supply_changed(ttf_psy);
			power_supply_put(ttf_psy);
		}
	}

	return size;
}

static ssize_t ttf_capacity_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->ttf_capacity;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t aicl_done_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	int new;
	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		sscanf(buf, "%d", &new);
		((struct unified_nodes*)dev->platform_data)->aicl_done = new;
	}

	return size;
}

static ssize_t aicl_done_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->aicl_done;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t ccd_icl_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* iusb = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_icl.type == VOTER_TYPE_IUSB)
		iusb = &((struct unified_nodes*)dev->platform_data)->ccd_icl;
	else
		pr_uninode("Error on getting voter\n");

	return iusb ? voter_store(iusb, buf, size) : size;
}

static ssize_t ccd_icl_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* iusb = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_icl.type == VOTER_TYPE_IUSB)
		iusb = &((struct unified_nodes*)dev->platform_data)->ccd_icl;
	else
		pr_uninode("Error on getting voter\n");

	return iusb ? voter_show(iusb, buf) : 0;
}

static ssize_t ccd_fcc_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* ibat = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_fcc.type == VOTER_TYPE_IBAT)
		ibat = &((struct unified_nodes*)dev->platform_data)->ccd_fcc;
	else
		pr_uninode("Error on getting voter\n");

	return ibat ? voter_store(ibat, buf, size) : size;
}

static ssize_t ccd_fcc_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* ibat = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_fcc.type == VOTER_TYPE_IBAT)
		ibat = &((struct unified_nodes*)dev->platform_data)->ccd_fcc;
	else
		pr_uninode("Error on getting voter\n");

	return ibat ? voter_show(ibat, buf) : 0;
}

static ssize_t ccd_vfloat_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* vfloat = NULL;
	int float_voltage = 0;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_vfloat.type == VOTER_TYPE_VFLOAT)
		vfloat = &((struct unified_nodes*)dev->platform_data)->ccd_vfloat;
	else
		pr_uninode("Error on getting voter\n");

	sscanf(buf, "%d", &float_voltage);
	veneer_voter_set(vfloat, float_voltage);

	return size;
}

static ssize_t ccd_vfloat_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* vfloat = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_vfloat.type == VOTER_TYPE_VFLOAT)
		vfloat = &((struct unified_nodes*)dev->platform_data)->ccd_vfloat;
	else
		pr_uninode("Error on getting voter\n");

	return vfloat ? voter_show(vfloat, buf) : 0;
}

static ssize_t ccd_input_suspend_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* input_suspend = NULL;
	struct voter_entry* wlc_suspend = NULL;
	int suspend = 0;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_input_suspend.type == VOTER_TYPE_IUSB)
		input_suspend = &((struct unified_nodes*)dev->platform_data)->ccd_input_suspend;
	else
		pr_uninode("Error on getting input_suspend voter\n");

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_wlc_suspend.type == VOTER_TYPE_IDC)
		wlc_suspend = &((struct unified_nodes*)dev->platform_data)->ccd_wlc_suspend;
	else
		pr_uninode("Error on getting wlc_suspend voter\n");

	sscanf(buf, "%d", &suspend);
	if (suspend == CCD_SUSPEND) {
		veneer_voter_set(input_suspend, VOTE_TOTALLY_BLOCKED);
		veneer_voter_set(wlc_suspend, VOTE_TOTALLY_BLOCKED);
		pr_uninode("Input Suspended by CCD voter\n");
	}else {
		veneer_voter_set(input_suspend, VOTE_TOTALLY_RELEASED);
		veneer_voter_set(wlc_suspend, VOTE_TOTALLY_RELEASED);
		pr_uninode("Input Released by CCD voter\n");
	}

	return size;
}

static ssize_t ccd_input_suspend_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* input_suspend = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_input_suspend.type == VOTER_TYPE_IUSB)
		input_suspend = &((struct unified_nodes*)dev->platform_data)->ccd_input_suspend;
	else
		pr_uninode("Error on getting voter\n");

	return input_suspend ? voter_show(input_suspend, buf) : 0;
}

static ssize_t ccd_batchg_en_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct voter_entry* batchg_en = NULL;
	int battery_charging_enabled;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_batchg_en.type == VOTER_TYPE_IBAT)
		batchg_en = &((struct unified_nodes*)dev->platform_data)->ccd_batchg_en;
	else
		pr_uninode("Error on getting voter\n");

	sscanf(buf, "%d", &battery_charging_enabled);
	if (battery_charging_enabled == 0){
		veneer_voter_set(batchg_en, VOTE_TOTALLY_BLOCKED);
		pr_uninode("Battery Charging blocked by CCD voter\n");
	}else {
		veneer_voter_set(batchg_en, VOTE_TOTALLY_RELEASED);
		pr_uninode("Battery Charging released by CCD voter\n");
	}

	return size;
}

static ssize_t ccd_batchg_en_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct voter_entry* batchg_en = NULL;

	if (dev && dev->platform_data &&
		((struct unified_nodes*)dev->platform_data)->ccd_batchg_en.type == VOTER_TYPE_IBAT)
		batchg_en = &((struct unified_nodes*)dev->platform_data)->ccd_batchg_en;
	else
		pr_uninode("Error on getting voter\n");

	return batchg_en ? voter_show(batchg_en, buf) : 0;
}

static ssize_t charging_completed_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 1;
	struct power_supply* battery = power_supply_get_by_name("battery");
	union power_supply_propval capacity = { .intval = 0, };

	if (dev && dev->platform_data) {
		if (battery) {
			if (!power_supply_get_property(battery, POWER_SUPPLY_PROP_CAPACITY, &capacity)
				&& capacity.intval == 100)
				ret = 0;
			power_supply_put(battery);
		}
	}
	pr_uninode("returning chcomp to %d", ret);
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_battery_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	bool *ori = NULL;
	int new = 0;
	struct power_supply* psy = NULL;
	union power_supply_propval prp_temp, prp_capacity, prp_voltagenow;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_battery;

		sscanf(buf, "%d", &new);
		new = !!new;
		if (*ori != new) {
			if (new && ref->fake_battery_temp_en)
				prp_temp.intval = ref->fake_battery_temp;
			else
				prp_temp.intval = -9999;

			if (new && ref->fake_battery_soc_en)
				prp_capacity.intval = ref->fake_battery_soc;
			else
				prp_capacity.intval = -9999;

			if (new && ref->fake_battery_volt_en)
				prp_voltagenow.intval = ref->fake_battery_volt * 1000;
			else
				prp_voltagenow.intval = -9999;

			psy = power_supply_get_by_name(ref->fake_psy);
			if (psy) {
				if (!power_supply_set_property(psy, POWER_SUPPLY_PROP_TEMP, &prp_temp)
					&& !power_supply_set_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prp_capacity)
					&& !power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &prp_voltagenow))
					*ori = new;
				power_supply_put(psy);
			}
		}
	}

	return size;
}

static ssize_t fake_battery_temp_en_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	bool *ori = NULL;
	int new = 0;
	struct power_supply* psy = NULL;
	union power_supply_propval temperature;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_battery_temp_en;

		sscanf(buf, "%d", &new);
		new = !!new;
		if (*ori != new) {
			if (ref->fake_battery && new)
				temperature.intval = ref->fake_battery_temp;
			else
				temperature.intval = -9999;

			psy = power_supply_get_by_name(ref->fake_psy);
			if (psy) {
				if (!power_supply_set_property(psy, POWER_SUPPLY_PROP_TEMP, &temperature))
					*ori = new;
				power_supply_put(psy);
			}
		}
	}

	return size;
}

static ssize_t fake_battery_soc_en_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	bool *ori = NULL;
	int new = 0;
	struct power_supply* psy = NULL;
	union power_supply_propval soc;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_battery_soc_en;

		sscanf(buf, "%d", &new);
		new = !!new;
		if (*ori != new) {
			if (ref->fake_battery && new)
				soc.intval = ref->fake_battery_soc;
			else
				soc.intval = -9999;

			psy = power_supply_get_by_name(ref->fake_psy);
			if (psy) {
				if (!power_supply_set_property(psy, POWER_SUPPLY_PROP_CAPACITY, &soc))
					*ori = new;
				power_supply_put(psy);
			}
		}
	}

	return size;
}

static ssize_t fake_battery_volt_en_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	bool *ori = NULL;
	int new = 0;
	struct power_supply* psy = NULL;
	union power_supply_propval volt;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_battery_volt_en;

		sscanf(buf, "%d", &new);
		new = !!new;
		if (*ori != new) {
			if (ref->fake_battery && new)
				volt.intval = ref->fake_battery_volt * 1000;
			else
				volt.intval = -9999;

			psy = power_supply_get_by_name(ref->fake_psy);
			if (psy) {
				if (!power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &volt))
					*ori = new;
				power_supply_put(psy);
			}
		}
	}

	return size;
}

static ssize_t fake_battery_temp_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	int *ori = NULL;
	int	new = 0;
	struct power_supply* psy = NULL;
	union power_supply_propval temperature;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_battery_temp;

		sscanf(buf, "%d", &new);
		if (*ori != new) {
			psy = power_supply_get_by_name(ref->fake_psy);
			if (ref->fake_battery && ref->fake_battery_temp_en)
				temperature.intval = new;
			else
				temperature.intval = -9999;

			if (psy) {
				if (!power_supply_set_property(psy, POWER_SUPPLY_PROP_TEMP, &temperature))
					*ori = new;
				power_supply_put(psy);
			}
		}
	}

	return size;
}

static ssize_t fake_battery_soc_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	int *ori = NULL;
	int	new = 0;
	struct power_supply* psy = NULL;
	union power_supply_propval soc;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_battery_soc;

		sscanf(buf, "%d", &new);
		if (*ori != new) {
			psy = power_supply_get_by_name(ref->fake_psy);
			if (ref->fake_battery && ref->fake_battery_soc_en)
				soc.intval = new;
			else
				soc.intval = -9999;

			if (psy) {
				if (!power_supply_set_property(psy, POWER_SUPPLY_PROP_CAPACITY, &soc))
					*ori = new;
				power_supply_put(psy);
			}
		}
	}

	return size;
}

static ssize_t fake_battery_volt_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	int *ori = NULL;
	int	new = 0;
	struct power_supply* psy = NULL;
	union power_supply_propval volt;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_battery_volt;

		sscanf(buf, "%d", &new);
		if (*ori != new) {
			psy = power_supply_get_by_name(ref->fake_psy);
			if (ref->fake_battery && ref->fake_battery_volt_en)
				volt.intval = new * 1000;
			else
				volt.intval = -9999;

			if (psy) {
				if (!power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &volt))
					*ori = new;
				power_supply_put(psy);
			}
		}
	}

	return size;
}

static ssize_t fake_battery_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_battery;
	}
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_battery_temp_en_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_battery_temp_en;
	}
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_battery_soc_en_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_battery_soc_en;
	}
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_battery_volt_en_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_battery_volt_en;
	}
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_battery_temp_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_battery_temp;
	}
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_battery_soc_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_battery_soc;
	}
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_battery_volt_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_battery_volt;
	}
	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_sdpmax_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes*	ref;
	bool*			ori;
	int 		new;

	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->fake_sdpmax;

		sscanf(buf, "%d", &new);
		new = !!new;
		if (*ori != new) {
			*ori = new;
			charging_ceiling_sdpmax(new);
		}
	}

	return size;
}

static ssize_t fake_sdpmax_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_sdpmax;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t fake_hvdcp_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes* uninodes = get_unified_nodes(dev);
	int val = 0;

	if (!uninodes)
		return size;

	sscanf(buf, "%d", &val);
	if (uninodes->fake_hvdcp != val) {
		pr_uninode("Storing %s\n", buf);
		uninodes->fake_hvdcp = val;
	}

	usb_set_property(uninodes, POWER_SUPPLY_PROP_EXT_FAKE_HVDCP, buf);

	return size;
}

static ssize_t fake_hvdcp_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->fake_hvdcp;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t battery_age_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	struct unified_nodes*	ref;
	struct power_supply* capacity_psy = NULL;
	union power_supply_propval capacity_designed = { .intval = 0, }, capacity_learned = { .intval = 0, };
	static int pre_designed, pre_learned = 0;

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		if (ref->battage_psy)
			capacity_psy = power_supply_get_by_name(ref->battage_psy);

		if (capacity_psy
			&& !power_supply_get_property(capacity_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &capacity_designed)
			&& !power_supply_get_property(capacity_psy,
				POWER_SUPPLY_PROP_CHARGE_FULL, &capacity_learned)
			&& capacity_designed.intval > 0) {

			ret = min((capacity_learned.intval * 100) / capacity_designed.intval,
				100);

			if (pre_designed != capacity_learned.intval ||pre_learned != capacity_designed.intval) {
				pre_designed = capacity_learned.intval;
				pre_learned = capacity_designed.intval;
				pr_uninode("battery age %d = %d/%d\n",
					ret, capacity_learned.intval, capacity_designed.intval);
			}
		}
		else
			pr_uninode("Calculating battery age is not ready\n");

		if (capacity_psy)
			power_supply_put(capacity_psy);
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t battery_condition_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int age;	// ratio type for battery age, i.e 95(%)
	int ret = 0;
	int age_thresold_best = 80;
	int age_thresold_good = 50;
	int age_thresold_bad = 0;
	int condition_ret_best = 1;
	int condition_ret_good = 2;
	int condition_ret_bad = 3;
	int condition_ret_abnormal = 0;

	int condition_msd = 10;

	bool tc_mode = 0;

	if (dev && dev->platform_data) {
		tc_mode = ((struct unified_nodes*)dev->platform_data)->battery_condition_tc_mode;
		if (tc_mode){
			ret = ((struct unified_nodes*)dev->platform_data)->battery_condition_tc_value;
			pr_uninode("Battery Condition tc_mode = %d tc_value = %d\n",tc_mode, ret);
			return snprintf(buf, PAGE_SIZE, "%d", ret);
		}
	}

	if (!!battery_age_show(dev, attr, buf) && !!sscanf(buf, "%d", &age) && age >= 0) {
		if (age >= 100)
			age = 100;
		else if (age < 0)
			age = 0;
		else
			condition_msd =  ( age /10 ) + 1;

		if (age >= age_thresold_best)
			ret = (condition_msd * 10) + condition_ret_best;
		else if (age >= age_thresold_good)
			ret = (condition_msd * 10) + condition_ret_good;
		else if (age >= age_thresold_bad)
			ret = (condition_msd * 10) + condition_ret_bad;
		else
			ret = (condition_msd * 10) + condition_ret_abnormal;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t battery_condition_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	int* ori = NULL;
	int new;
	bool* tc_mode = NULL;
	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		ori = &((struct unified_nodes*)dev->platform_data)->battery_condition_tc_value;
		sscanf(buf, "%d", &new);
		*ori = new;
		tc_mode = &((struct unified_nodes*)dev->platform_data)->battery_condition_tc_mode;
		*tc_mode = true;
	}

	pr_uninode("Battery Condition TC set mode = %d value = %d\n", *tc_mode, *ori);

	return size;
}

#define MAX_CBC_CYCLE 21474
static ssize_t battery_cycle_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	int rc = 0;
	int cycle_diff = 0;
	int backup_cbc_cycle = 0;
	static int org_cycle = 0;
	struct unified_nodes* ref;
	struct power_supply* battery_psy = NULL;
	union power_supply_propval cycle_count = { .intval = 0, };

	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		if (ref->battage_psy)
			battery_psy = power_supply_get_by_name(ref->battage_psy);

		if (battery_psy
			&& !power_supply_get_property(battery_psy,
				POWER_SUPPLY_PROP_CYCLE_COUNT, &cycle_count)) {
			ret = cycle_count.intval;
		}
		if (battery_psy)
			power_supply_put(battery_psy);

		if (ret != ref->cbc_cycle){
			if (ref->cbc_cycle == 0){
				get_veneer_backup_cbc_cycle(&backup_cbc_cycle);
				if ((backup_cbc_cycle != ref->cbc_cycle) && (backup_cbc_cycle > 0)
					&& (backup_cbc_cycle < MAX_CBC_CYCLE)){
					pr_uninode("read backup_cbc_cycle = %d, ref->cbc_cycle = %d \n", backup_cbc_cycle, ref->cbc_cycle );
					ref->cbc_cycle = backup_cbc_cycle;
				}
				else
					ref->cbc_cycle = ret;
			}
			pr_uninode("org_cycle = %d batt_psy cycle = %d  ref->cbc_cycle = %d \n", org_cycle, ret, ref->cbc_cycle);
			if (org_cycle == 0)
				org_cycle = ret;
			else {
				cycle_diff = ret - org_cycle;
				org_cycle = ret;
			}
			ret = ref->cbc_cycle + cycle_diff;
			if (ref->cbc_cycle != ret){
				rc = set_veneer_backup_cbc_cycle(ret);
				if ( rc < 0){
					org_cycle = cycle_count.intval - cycle_diff;
					pr_uninode("set_veneer_backup_cbc_cycle failed. retry again. rc = %d, restored org_cycle to %d\n", rc, org_cycle);
				}
			}
			if (rc == 0){
				ref->cbc_cycle = ret;
			}
		}
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t battery_cycle_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes*	ref;
	int*			ori;
	int 		new;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->cbc_cycle;

		sscanf(buf, "%d", &new);
		if (new <= 0){
			pr_uninode("Skip storing cycle %d.\n", new);
		} else if (*ori != new) {
			*ori = new;
			set_veneer_backup_cbc_cycle(new);
		}
	}
	return size;

}

static ssize_t battery_valid_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	bool*	ori;
	int new;
	pr_uninode("Storing %s\n", buf);

	if (dev && dev->platform_data) {
		ori = &((struct unified_nodes*)dev->platform_data)->battery_valid;
		sscanf(buf, "%d", &new);
		*ori = !!new;
	}

	return size;
}

static ssize_t battery_valid_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->battery_valid;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t charger_name_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	char* ori;

	if (dev && dev->platform_data) {
		ori = ((struct unified_nodes*)dev->platform_data)->charger_name;
		if (strcmp(ori, buf))
			pr_uninode("Storing %s\n", buf);

		strcpy(ori, buf);
		ori[strlen(buf)] = '\0';
	}

	return size;
}

static ssize_t charger_name_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	char* ori;

	if (dev && dev->platform_data) {
		ori = ((struct unified_nodes*)dev->platform_data)->charger_name;
		strncpy(buf, ori, strlen(ori));
		ret = strlen(ori);
	}

	return ret;
}

static ssize_t charger_highspeed_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	bool*	ori;
	int new;
	static int pre_new = -1;

	sscanf(buf, "%d", &new);
	if (new != pre_new) {
		pre_new = new;
		pr_uninode("Storing %s\n", buf);
	}

	if (dev && dev->platform_data) {
		ori = &((struct unified_nodes*)dev->platform_data)->charger_highspeed;

		*ori = !!new;
	}

	return size;
}

static ssize_t charger_highspeed_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->charger_highspeed;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t charger_incompatible_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	static bool container = false;
	bool**	ori;
	int new = -1;
	static int pre_new = -1;

	if (dev && dev->platform_data) {
		ori = &((struct unified_nodes*)dev->platform_data)->charger_incompatible;
		if (!(*ori))
			*ori = &container;

		sscanf(buf, "%d", &new);
		container = !!new;
	}

	if (new != pre_new) {
		pre_new = new;
		pr_uninode("Storing %s\n", buf);
	}
	return size;
}

static ssize_t charger_incompatible_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = UNIFIED_NODES_DISABLED;
	bool* val;

	if (dev && dev->platform_data) {
		val = ((struct unified_nodes*)dev->platform_data)->charger_incompatible;
		if (val)
			ret = !!(*val);
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t charger_usbid_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct power_supply* 		usb = power_supply_get_by_name("usb");
	union power_supply_propval	prp = { .intval = 0, };

	if (dev && dev->platform_data) {
		if (usb) {
			power_supply_get_property(usb, POWER_SUPPLY_PROP_EXT_RESISTANCE, &prp);
			power_supply_put(usb);
		}
	}

	pr_uninode("returning usbid(mv) is %d\n", prp.intval);
	return snprintf(buf, PAGE_SIZE, "Result:%d Raw:ffff\n", prp.intval);
}

static ssize_t charger_verbose_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	static enum charging_verbosity container = VERBOSE_CHARGER_NONE;
	enum charging_verbosity** ori;
	int 		  new;

	if (dev && dev->platform_data) {
		ori = &((struct unified_nodes*)dev->platform_data)->charger_verbose;

		if (!(*ori))
			*ori = &container;

		sscanf(buf, "%d", &new);
		container = new;
	}

	pr_uninode("Storing %s\n", buf);
	return size;
}

static ssize_t charger_verbose_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = UNIFIED_NODES_DISABLED;
	enum charging_verbosity* val;

	if (dev && dev->platform_data) {
		val = ((struct unified_nodes*)dev->platform_data)->charger_verbose;
		if (val)
			ret = *val;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t support_fastpl_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes*	ref;
	int*			ori;
	int 		new = 0;

	pr_uninode("FASTPL: Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->support_fastpl;

		sscanf(buf, "%d", &new);
		// If parallel charging is not supported (*ori == -1),
		// then do not update it via this command.
		if (*ori >= 0 && new >= 0 && *ori != new) {
			*ori = new;
		}
	}
	return size;
}

static ssize_t support_fastpl_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->support_fastpl;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t support_fastchg_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;
	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->support_fastchg;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t bsm_timetofull_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes*	ref;
	int*			ori;
	int 		new;

    pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->bsm_timetofull;

		sscanf(buf, "%d", &new);
		if (*ori != new) {
			*ori = new;
			charging_time_update();
		}
	}
	return size;
}

static ssize_t bsm_timetofull_show(struct device* dev, struct device_attribute* attr, char* buf) {
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->bsm_timetofull;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t bsm_enable_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size) {
	struct unified_nodes*   ref;
	int*                    ori;
	int             new;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->bsm_enable;

		sscanf(buf, "%d", &new);
		if (*ori != new) {
			*ori = new;
			charging_time_update();
		}
	}

	return size;
}
static ssize_t bsm_enable_show(struct device* dev, struct device_attribute* attr, char* buf) {
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->bsm_enable;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static int actm_node_get_error(enum unified_node_id uni)
{
	int ret = -9999;

	switch(uni) {
		case UNIFIED_NODE_ACTM_MODE :
		case UNIFIED_NODE_ACTM_LCDON_OFFSET :
		case UNIFIED_NODE_ACTM_SENSOR_WIRED :
		case UNIFIED_NODE_ACTM_SENSOR_WIRELESS :
			ret = -2;
			break;
		default :
			ret = -9999;
			break;
	}
	return ret;
}

static ssize_t actm_node_show(
	struct device* dev, struct device_attribute* attr, char* buf)
{
	struct unified_nodes* uninodes = get_unified_nodes(dev);
	enum unified_node_id uni = attr - unified_nodes_dattrs;
	int ret = -9999;

	//pr_uninode("check id = %d\n", uni);
	if (uni < 0 || uni >= MAX_UNIFIED_NODES_SIZE)
		goto out;

	if (!uninodes) {
		ret = actm_node_get_error(uni);
		goto out;
	}

	ret = uninodes->actm[uni];

out:
	return snprintf(buf, PAGE_SIZE, "%d", ret);

}

static bool actm_node_need_update(enum unified_node_id uni, int new)
{
	bool ret = true;

	switch(uni) {
		case UNIFIED_NODE_ACTM_MODE :
			if (!(new == -1 ||
					new ==  0 || new ==  1 || new ==  2 || new ==  3 ||
					new == 10 || new == 11 || new == 12 || new == 13 ||
					new == 20 || new == 21 || new == 22 || new == 23 ||
					new == 30 || new == 31 || new == 32 || new == 33 ))
				ret = false;
			break;

		case UNIFIED_NODE_ACTM_SENSOR_WIRED :
		case UNIFIED_NODE_ACTM_SENSOR_WIRELESS :
			if (!(new ==  1 || new ==  2 || new ==  3 ||
					new == 11 || new == 12 || new == 13 ||
					new == 21 || new == 22 || new == 23 ||
					new == 31 || new == 32 || new == 33 ))
				ret = false;
			break;

		case UNIFIED_NODE_ACTM_HOLDDEG_WIRED_0 ... UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_2 :
		case UNIFIED_NODE_ACTM_CURRENT_WIRED_0 ... UNIFIED_NODE_ACTM_MAX_FCC_QC2 :
			if (new <= 0)
				ret = false;
			break;

		default :
			break;
	}

	//pr_uninode("id = %d, new = %d, update %d\n", uni, new, ret);
	return ret;
}

static ssize_t actm_node_store(
	struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes* uninodes = get_unified_nodes(dev);
	enum unified_node_id uni = attr - unified_nodes_dattrs;
	int new = -9999;

	//pr_uninode("id = %d, Storing %s\n", uni, buf);
	if (uni < 0 || uni >= MAX_UNIFIED_NODES_SIZE)
		return size;

	if (!uninodes)
		return size;

	sscanf(buf, "%d", &new);
	if (actm_node_need_update(uni, new))
		uninodes->actm[uni] = new;

	return size;
}

static ssize_t irc_enabled_store(
	struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	int *ori = NULL;
	int new = -9999;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->irc_enabled;

		sscanf(buf, "%d", &new);
		if (*ori != new) {
			*ori = !!new;
		}
	}

	return size;
}

static ssize_t irc_enabled_show(
	struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = -2;

	if (dev && dev->platform_data)
		ret = ((struct unified_nodes*)dev->platform_data)->irc_enabled;

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t irc_resistance_store(
	struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes *ref = NULL;
	int *ori = NULL;
	int new = -9999;

	pr_uninode("Storing %s\n", buf);
	if (dev && dev->platform_data) {
		ref = (struct unified_nodes*)dev->platform_data;
		ori = &ref->irc_resistance;

		sscanf(buf, "%d", &new);
		if (*ori != new) {
			*ori = new;
		}
	}

	return size;
}

static ssize_t irc_resistance_show(
	struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = -2;

	if (dev && dev->platform_data)
		ret = ((struct unified_nodes*)dev->platform_data)->irc_resistance;

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t factory_xo_therm_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct thermal_zone_device *tz;
	int sens_temp = -1;
	int ret = 0;

	if (dev && dev->platform_data) {
		tz = ((struct unified_nodes*)dev->platform_data)->xo_tz;
		ret = thermal_zone_get_temp(tz, &sens_temp);
		if (ret) {
			pr_uninode("xo therm read error:%d\n", ret);
		}
	}

	return snprintf(buf, PAGE_SIZE, "Result:%d Raw:ffff\n", sens_temp);
}

static ssize_t factory_bd_therm_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct thermal_zone_device *tz;
	int sens_temp = -1;
	int ret = 0;

	if (dev && dev->platform_data) {
		tz = ((struct unified_nodes*)dev->platform_data)->bd_tz;
		ret = thermal_zone_get_temp(tz, &sens_temp);
		if (ret) {
			pr_uninode("bd therm read error:%d\n", ret);
		}
	}

	return snprintf(buf, PAGE_SIZE, "Result:%d Raw:ffff\n", sens_temp);
}

static ssize_t upper_pcb_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct power_supply* 		usb = power_supply_get_by_name("usb");
	union power_supply_propval	prp = { .intval = 0, };

	if (dev && dev->platform_data) {
		if (usb) {
			power_supply_get_property(usb, POWER_SUPPLY_PROP_EXT_UPPER_PCB, &prp);
			power_supply_put(usb);
		}
	}

	return snprintf(buf, PAGE_SIZE, "%d", prp.intval);
}

static ssize_t low_pcb_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct power_supply* 		usb = power_supply_get_by_name("usb");
	union power_supply_propval	prp = { .intval = 0, };

	if (dev && dev->platform_data) {
		if (usb) {
			power_supply_get_property(usb, POWER_SUPPLY_PROP_EXT_LOW_PCB, &prp);
			power_supply_put(usb);
		}
	}

	return snprintf(buf, PAGE_SIZE, "%d", prp.intval);
}

static const char * const usb_real_type_text[] = {
	"None", "Unknown", "USB", "USB_OCP", "USB_CDP", "USB_DCP",
	"USB_FLOAT", "USB_C", "USB_C", "USB_C",
	"DEBUG_ACCESS", "USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP_3",
	"USB_PD", "USB_PD", "POWERCABLE", "DEBUG_ACCESS", "AUDIO_ACCESS",
	"Wireless", "Wireless", "Wireless", "Wireless", "INVALID",
};
static ssize_t usb_real_type_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_REAL_TYPE, 0);

	return snprintf(buf, PAGE_SIZE, "%s\n", usb_real_type_text[ret]);
}

static ssize_t status_boot_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct unified_nodes* uninodes = get_unified_nodes(dev);

	pr_uninode("Storing %s\n", buf);
	if (!uninodes)
		return size;

	sscanf(buf, "%d", &uninodes->status_boot);
	if (uninodes->status_boot)
		wls_set_property(uninodes, POWER_SUPPLY_PROP_EXT_INPUT_SUSPEND, buf);

	return size;
}

static ssize_t status_boot_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = 0;

	if (dev && dev->platform_data) {
		ret = ((struct unified_nodes*)dev->platform_data)->status_boot;
	}

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t safety_timer_enabled_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = bat_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_SAFETY_TIMER_ENABLE, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t safety_timer_enabled_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	pr_uninode("Storing %s\n", buf);
	bat_set_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_SAFETY_TIMER_ENABLE, buf);

	return size;
}

#define INPUT_SUSPEND_VOTER "INPUT_SUSPEND_VOTER"
static ssize_t input_suspend_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct votable *usb_icl_votable = find_votable("USB_ICL");
	int ret = is_client_vote_enabled_locked(usb_icl_votable, INPUT_SUSPEND_VOTER);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t input_suspend_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct votable *usb_icl_votable = find_votable("USB_ICL");
	int ret = 0;

	sscanf(buf, "%d", &ret);
	vote_priority(usb_icl_votable, INPUT_SUSPEND_VOTER, ret, 0, PRIORITY_HIGH);

	return size;
}

static ssize_t input_current_settled_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_INPUT_CURRENT_SETTLED, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t resistance_id_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_RESISTANCE_ID, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

#define BATTERY_CHARGING_VOTER "BATTERY_CHARGING_VOTER"
static ssize_t battery_charging_enabled_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	struct votable *fcc_votable = find_votable("FCC");
	int ret = !is_client_vote_enabled_locked(fcc_votable, BATTERY_CHARGING_VOTER);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t battery_charging_enabled_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	struct votable *fcc_votable = find_votable("FCC");
	int ret = 0;

	sscanf(buf, "%d", &ret);
	vote(fcc_votable, BATTERY_CHARGING_VOTER, !ret, 0);

	return size;
}

static ssize_t parallel_enabled_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_PARALLEL_ENABLED, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t ac_online_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int online = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_ONLINE, 0);
	int type = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_TYPE, 0);

	if (online && type != POWER_SUPPLY_TYPE_USB_PD)
		online = 0;

	return snprintf(buf, PAGE_SIZE, "%d", online);
}

static ssize_t usb_online_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int online = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_ONLINE, 0);
	int type = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_TYPE, 0);

	if (online && type != POWER_SUPPLY_TYPE_USB)
		online = 0;

	return snprintf(buf, PAGE_SIZE, "%d", online);
}

static ssize_t typec_cc_orientation_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_CC_ORIENTATION, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t raw_capacity_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = bat_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_QBG_MSOC_X100, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t typec_moisture_en_show(struct device* dev, struct device_attribute* attr, char* buf)
{
	int ret = usb_get_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_MOISTURE_EN, 0);

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

static ssize_t typec_moisture_en_store(struct device* dev, struct device_attribute* attr, const char* buf, size_t size)
{
	pr_uninode("Storing %s\n", buf);
	usb_set_property(get_unified_nodes(dev), POWER_SUPPLY_PROP_EXT_MOISTURE_EN, buf);

	return size;
}

#define ACTM_ATTR(_name)					\
{									\
	.attr = { .name = #_name, .mode = 0664 },					\
	.show = actm_node_show,				\
	.store = actm_node_store,				\
}

static struct device_attribute unified_nodes_dattrs [] = {
	[UNIFIED_NODE_ACTM_MODE]					=  ACTM_ATTR(actm_mode),
	[UNIFIED_NODE_ACTM_LCDON_OFFSET]			=  ACTM_ATTR(actm_lcdon_offset),
	[UNIFIED_NODE_ACTM_SENSOR_WIRED]			=  ACTM_ATTR(actm_sensor_wired),
	[UNIFIED_NODE_ACTM_SENSOR_WIRELESS]			=  ACTM_ATTR(actm_sensor_wireless),
	[UNIFIED_NODE_ACTM_HOLDDEG_WIRED_0]			=  ACTM_ATTR(actm_holddeg_wired_0),
	[UNIFIED_NODE_ACTM_HOLDDEG_WIRED_1]			=  ACTM_ATTR(actm_holddeg_wired_1),
	[UNIFIED_NODE_ACTM_HOLDDEG_WIRED_2]			=  ACTM_ATTR(actm_holddeg_wired_2),
	[UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_0]		=  ACTM_ATTR(actm_holddeg_wireless_0),
	[UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_1]		=  ACTM_ATTR(actm_holddeg_wireless_1),
	[UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_2]		=  ACTM_ATTR(actm_holddeg_wireless_2),
	[UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_0]		=  ACTM_ATTR(actm_tempoffs_wired_0),
	[UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_1]		=  ACTM_ATTR(actm_tempoffs_wired_1),
	[UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_2]		=  ACTM_ATTR(actm_tempoffs_wired_2),
	[UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_0]		=  ACTM_ATTR(actm_tempoffs_wireless_0),
	[UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_1]		=  ACTM_ATTR(actm_tempoffs_wireless_1),
	[UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_2]		=  ACTM_ATTR(actm_tempoffs_wireless_2),
	[UNIFIED_NODE_ACTM_CURRENT_WIRED_0]			=  ACTM_ATTR(actm_current_wired_0),
	[UNIFIED_NODE_ACTM_CURRENT_WIRED_1]			=  ACTM_ATTR(actm_current_wired_1),
	[UNIFIED_NODE_ACTM_CURRENT_WIRED_2]			=  ACTM_ATTR(actm_current_wired_2),
	[UNIFIED_NODE_ACTM_POWER_EPP_0]				=  ACTM_ATTR(actm_power_epp_0),
	[UNIFIED_NODE_ACTM_POWER_EPP_1]				=  ACTM_ATTR(actm_power_epp_1),
	[UNIFIED_NODE_ACTM_POWER_EPP_2]				=  ACTM_ATTR(actm_power_epp_2),
	[UNIFIED_NODE_ACTM_POWER_BPP_0]				=  ACTM_ATTR(actm_power_bpp_0),
	[UNIFIED_NODE_ACTM_POWER_BPP_1]				=  ACTM_ATTR(actm_power_bpp_1),
	[UNIFIED_NODE_ACTM_POWER_BPP_2]				=  ACTM_ATTR(actm_power_bpp_2),
	[UNIFIED_NODE_ACTM_CURR_CP_PPS]				=  ACTM_ATTR(actm_curr_cp_pps),
	[UNIFIED_NODE_ACTM_CURR_CP_QC30]			=  ACTM_ATTR(actm_curr_cp_qc30),
	[UNIFIED_NODE_ACTM_MAX_FCC_PPS]				=  ACTM_ATTR(actm_max_fcc_pps),
	[UNIFIED_NODE_ACTM_MAX_FCC_QC3]				=  ACTM_ATTR(actm_max_fcc_qc3),
	[UNIFIED_NODE_ACTM_MAX_FCC_QC2]				=  ACTM_ATTR(actm_max_fcc_qc2),
	[UNIFIED_NODE_IRC_ENABLED]					=  __ATTR_RW(irc_enabled),
	[UNIFIED_NODE_IRC_RESISTANCE]				=  __ATTR_RW(irc_resistance),
	[UNIFIED_NODE_THERMALD_IUSB]				=  __ATTR_RW(thermald_iusb),
	[UNIFIED_NODE_THERMALD_IBAT]				=  __ATTR_RW(thermald_ibat),
	[UNIFIED_NODE_THERMALD_IDC]					=  __ATTR_RW(thermald_idc),
	[UNIFIED_NODE_THERMALD_VDC]					=  __ATTR_RW(thermald_vdc),
	[UNIFIED_NODE_STATUS_BOOT]					=  __ATTR_RW(status_boot),
	[UNIFIED_NODE_STATUS_LCD]					=  __ATTR_RW(status_lcd),
	[UNIFIED_NODE_CHARGING_RESTRICTION]			=  __ATTR_RW(charging_restriction),
	[UNIFIED_NODE_CHARGING_ENABLE]				=  __ATTR_RW(charging_enable),
	[UNIFIED_NODE_CHARGING_STEP]				=  __ATTR_RW(charging_step),
	[UNIFIED_NODE_TIME_TO_FULL_NOW]				=  __ATTR_RW(time_to_full_now),
	[UNIFIED_NODE_TTF_CAPACITY]					=  __ATTR_RW(ttf_capacity),
	[UNIFIED_NODE_AICL_DONE]					=  __ATTR_RW(aicl_done),
	[UNIFIED_NODE_CCD_ICL]						=  __ATTR_RW(ccd_icl),
	[UNIFIED_NODE_CCD_FCC]						=  __ATTR_RW(ccd_fcc),
	[UNIFIED_NODE_CCD_VFLOAT]					=  __ATTR_RW(ccd_vfloat),
	[UNIFIED_NODE_CCD_INPUT_SUSPEND]			=  __ATTR_RW(ccd_input_suspend),
	[UNIFIED_NODE_CCD_BATCHG_EN]				=  __ATTR_RW(ccd_batchg_en),
	[UNIFIED_NODE_CHARGING_SHOWCASE]			=  __ATTR_RW(charging_showcase),
	[UNIFIED_NODE_CHARGING_COMPLETED]			=  __ATTR_RO(charging_completed),
	[UNIFIED_NODE_FAKE_BATTERY]					=  __ATTR_RW(fake_battery),
	[UNIFIED_NODE_FAKE_BATTERY_TEMP]			=  __ATTR_RW(fake_battery_temp),
	[UNIFIED_NODE_FAKE_BATTERY_SOC]				=  __ATTR_RW(fake_battery_soc),
	[UNIFIED_NODE_FAKE_BATTERY_VOLT]			=  __ATTR_RW(fake_battery_volt),
	[UNIFIED_NODE_FAKE_BATTERY_TEMP_EN]			=  __ATTR_RW(fake_battery_temp_en),
	[UNIFIED_NODE_FAKE_BATTERY_SOC_EN]			=  __ATTR_RW(fake_battery_soc_en),
	[UNIFIED_NODE_FAKE_BATTERY_VOLT_EN]			=  __ATTR_RW(fake_battery_volt_en),
	[UNIFIED_NODE_FAKE_SDPMAX]					=  __ATTR_RW(fake_sdpmax),
	[UNIFIED_NODE_FAKE_HVDCP]					=  __ATTR_RW(fake_hvdcp),
	[UNIFIED_NODE_BATTERY_AGE]					=  __ATTR_RO(battery_age),
	[UNIFIED_NODE_BATTERY_CONDITION]			=  __ATTR_RW(battery_condition),
	[UNIFIED_NODE_BATTERY_CYCLE]				=  __ATTR_RW(battery_cycle),
	[UNIFIED_NODE_BATTERY_VALID]				=  __ATTR_RW(battery_valid),
	[UNIFIED_NODE_CHARGER_NAME]					=  __ATTR_RW(charger_name),
	[UNIFIED_NODE_CHARGER_HIGHSPEED]			=  __ATTR_RW(charger_highspeed),
	[UNIFIED_NODE_CHARGER_INCOMPATIBLE]			=  __ATTR_RW(charger_incompatible),
	[UNIFIED_NODE_CHARGER_USBID]				=  __ATTR_RO(charger_usbid),
	[UNIFIED_NODE_CHARGER_VERBOSE]				=  __ATTR_RW(charger_verbose),
	[UNIFIED_NODE_SUPPORT_FASTPL]				=  __ATTR_RW(support_fastpl),
	[UNIFIED_NODE_SUPPORT_FASTCHG]				=  __ATTR_RO(support_fastchg),
	[UNIFIED_NODE_BSM_TIMETOFULL]				=  __ATTR_RW(bsm_timetofull),
	[UNIFIED_NODE_BSM_ENABLE]					=  __ATTR_RW(bsm_enable),
	[UNIFIED_NODE_FACTORY_XO_THERM]				=  __ATTR_RO(factory_xo_therm),
	[UNIFIED_NODE_FACTORY_BD_THERM]				=  __ATTR_RO(factory_bd_therm),
	[UNIFIED_NODE_FACTORY_BD_THERM]				=  __ATTR_RO(factory_bd_therm),
	[UNIFIED_NODE_USB_REAL_TYPE]				=  __ATTR_RO(usb_real_type),
	[UNIFIED_NODE_SAFETY_TIMER_ENABLED]			=  __ATTR_RW(safety_timer_enabled),
	[UNIFIED_NODE_INPUT_SUSPEND]				=  __ATTR_RW(input_suspend),
	[UNIFIED_NODE_INPUT_CURRENT_SETTLED]		=  __ATTR_RO(input_current_settled),
	[UNIFIED_NODE_RESISTANCE_ID]				=  __ATTR_RO(resistance_id),
	[UNIFIED_NODE_BATTERY_CHARGING_ENABLED]		=  __ATTR_RW(battery_charging_enabled),
	[UNIFIED_NODE_PARALLEL_ENABLED]				=  __ATTR_RO(parallel_enabled),
	[UNIFIED_NODE_AC_ONLINE]					=  __ATTR_RO(ac_online),
	[UNIFIED_NODE_USB_ONLINE]					=  __ATTR_RO(usb_online),
	[UNIFIED_NODE_TYPEC_CC_ORIENTATION]			=  __ATTR_RO(typec_cc_orientation),
	[UNIFIED_NODE_TYPEC_RAW_CAPACITY]			=  __ATTR_RO(raw_capacity),
	[UNIFIED_NODE_TYPEC_MOISTURE_EN]			=  __ATTR_RW(typec_moisture_en),
	[UNIFIED_NODE_UPPER_PCB]					=  __ATTR_RO(upper_pcb),
	[UNIFIED_NODE_LOW_PCB]						=  __ATTR_RO(low_pcb),
};

static struct platform_device unified_nodes_device = {
	.name = UNIFIED_NODES_DEVICE,
	.id = -1,	// Set -1 explicitly to make device name simple
	.dev = {
		.platform_data = NULL,
	}
};

static void unified_nodes_clear(/*@Nonnull*/ struct unified_nodes* uninodes)
{
	memset(uninodes, 0, sizeof(struct unified_nodes));
	// It makes all the voter_entry.type to INVALID(0)
}

static struct device_attribute* unified_nodes_search(const char* key)
{
       /*	struct device_attribute {
	*		struct attribute attr;
	*		ssize_t (*show)(struct device* dev, struct device_attribute* attr, char* buf);
	*		ssize_t (*store)(struct device* dev, struct device_attribute* attr, const char* buf, size_t count);
	*	};
	*	struct attribute {
	*		const char* name;
	*		struct module* owner;
	*		mode_t mode;
	*	};
	*	#define __ATTR(_name,_mode,_show,_store) { .attr = {.name = __stringify(_name), .mode = _mode, .owner = THIS_MODULE }, .show = _show, .store = _store, }
	*/
	int i;
	for (i = 0; i < ARRAY_SIZE(unified_nodes_dattrs); i++) {
		if (!strcmp(unified_nodes_dattrs[i].attr.name, key)) {
			return &unified_nodes_dattrs[i];
		}
	}

	return NULL;
}

bool unified_nodes_write(enum unified_node_id uni, int data)
{
	struct device* device = &unified_nodes_device.dev;
	struct device_attribute* dattr = &unified_nodes_dattrs[uni];
	char buf[8] = { 0, };

	snprintf(buf, sizeof(buf), "%d", data);
	dattr->store(device, dattr, buf, sizeof(buf));
	return true;
}

bool unified_nodes_read(enum unified_node_id uni, int *data)
{
	struct device* device = &unified_nodes_device.dev;
	struct device_attribute* dattr = &unified_nodes_dattrs[uni];
	char buf[8] = { 0, };
	bool rc = true;

	rc &= dattr->show(device, dattr, buf);
	rc &= sscanf(buf, "%d", data);

	return rc;
}

bool unified_nodes_store(const char* key, const char* value, size_t size)
{
	struct device* device = &unified_nodes_device.dev;
	struct device_attribute* dattr = unified_nodes_search(key);
	if (dattr) {
		dattr->store(device, dattr, value, size);
		return true;
	}
	else
		return false;
}

bool unified_nodes_show(const char* key, char* value)
{
	struct device* device = &unified_nodes_device.dev;
	struct device_attribute* dattr = unified_nodes_search(key);
	if (dattr) {
		dattr->show(device, dattr, value);
		return true;
	}
	else
		return false;
}

static struct attribute_group* unified_nodes_attrgroup(void)
{
	static struct attribute*	s_attrs [ARRAY_SIZE(unified_nodes_dattrs) + 1];
	static struct attribute_group	s_files = { .attrs  = NULL, };
	int i;

	if (s_files.attrs == NULL) {
		s_files.attrs = s_attrs;

		for (i = 0; i < ARRAY_SIZE(unified_nodes_dattrs); i++)
			s_attrs[i] = &unified_nodes_dattrs[i].attr;
		s_attrs[i] = NULL;
	}

	return &s_files;
}

bool unified_nodes_create(struct device_node* devnode)
{
	struct unified_nodes* uninodes =
		kzalloc(sizeof(struct unified_nodes), GFP_KERNEL);

	pr_uninode("Creating unified nodes\n");

	if (!(devnode && of_device_is_available(devnode))) {
		pr_uninode("unified_nodes disabled.\n");
		return true;
	}

	if (!uninodes) {
		pr_uninode("Failed to alloc unified_nodes\n");
		goto failed;
	}
	else
		unified_nodes_device.dev.platform_data = uninodes;

	if (!unified_nodes_devicetree(devnode, uninodes)) {
		pr_uninode("Failed to parse unified_nodes_devicetree\n");
		goto failed;
	}

	if (!unified_nodes_actm_dt(devnode, uninodes)) {
		pr_uninode("Failed to parse unified_nodes_actm_dt\n");
		goto failed;
	}

	if (!voter_register(uninodes)) {
		pr_uninode("Failed to preset voters\n");
		goto failed;
	}
	if (sysfs_create_group(
			&unified_nodes_device.dev.kobj, unified_nodes_attrgroup())) {
		pr_uninode("Failed to create sysfs voters\n");
		goto failed;
	}

	pr_uninode("Success to create unified nodes\n");
	return true;

failed:	unified_nodes_destroy();
	return false;
}

void unified_nodes_destroy(void)
{
	struct unified_nodes* uninodes;

	pr_uninode("Destroying unified nodes\n");

	if (unified_nodes_device.dev.platform_data) {
		uninodes = unified_nodes_device.dev.platform_data;

		voter_unregister(uninodes);
		unified_nodes_clear(uninodes);
		kfree(uninodes);

		unified_nodes_device.dev.platform_data = NULL;
	}

	sysfs_remove_group(&unified_nodes_device.dev.kobj, unified_nodes_attrgroup());
}

static int __init unified_nodes_init(void)
{
	pr_uninode("platform_device_register : unified_nodes_device\n");
	return platform_device_register(&unified_nodes_device);
}

static void __exit unified_nodes_exit(void)
{
	pr_uninode("platform_device_unregister : unified_nodes_device\n");
	platform_device_unregister(&unified_nodes_device);
}

module_init(unified_nodes_init);
module_exit(unified_nodes_exit);
