#define pr_fmt(fmt) "CHG: [SHOWCASE] %s: " fmt, __func__
#define pr_showcase(fmt, ...) pr_err(fmt, ##__VA_ARGS__)

#include <linux/of.h>
#include <linux/ctype.h>
#include <linux/power_supply.h>

#include "veneer-primitives.h"

enum showcase_status {
	SHOWCASE_CHARGING_NONE,
	SHOWCASE_CHARGING_RELEASE,
	SHOWCASE_CHARGING_BLOCK,
	SHOWCASE_CHARGING_HOLD,
};

static struct showcase_charging {
	enum showcase_status charging_status;

	struct voter_entry voter_ibat;
	struct voter_entry voter_iusb;
	struct voter_entry voter_idc;

	int soc_min;
	int soc_max;
	bool enable;
} showcase = { 0, };

static const char* status_to_string(enum showcase_status status)
{
	switch (status) {
	case SHOWCASE_CHARGING_NONE :
		return "SHOWCASE_CHARGING_NONE";
	case SHOWCASE_CHARGING_RELEASE :
		return "SHOWCASE_CHARGING_RELEASE";
	case SHOWCASE_CHARGING_BLOCK :
		return "SHOWCASE_CHARGING_BLOCK";
	case SHOWCASE_CHARGING_HOLD :
		return "SHOWCASE_CHARGING_HOLD";
	default :
		return "SHOWCASE_CHARGING_INVALID";
	}
}

static void status_to_vote(enum showcase_status status)
{
	switch (status) {
	case SHOWCASE_CHARGING_NONE :
	case SHOWCASE_CHARGING_RELEASE :
		veneer_voter_release(&showcase.voter_ibat);
		veneer_voter_release(&showcase.voter_iusb);
		veneer_voter_release(&showcase.voter_idc);
		break;
	case SHOWCASE_CHARGING_BLOCK :
		veneer_voter_set(&showcase.voter_ibat, VOTE_TOTALLY_BLOCKED);
		veneer_voter_set(&showcase.voter_iusb, VOTE_TOTALLY_BLOCKED);
		veneer_voter_set(&showcase.voter_idc, VOTE_TOTALLY_BLOCKED);
		break;
	case SHOWCASE_CHARGING_HOLD :
		veneer_voter_set(&showcase.voter_ibat, VOTE_TOTALLY_BLOCKED);
		veneer_voter_release(&showcase.voter_iusb);
		veneer_voter_release(&showcase.voter_idc);
		break;
	default :
		pr_showcase("Invalid parameter %d\n", status);
		break;
	}
}

#define SOC_MAX showcase.soc_max
#define SOC_MIN showcase.soc_min
static enum showcase_status showcase_charging_transit(bool enabled, bool charging, int capacity, enum showcase_status now)
{
	/* When the capacity is 35% or more, charging is disabled until 29% */
	if (enabled && charging) {
		/* Check capacity < SOC_MIN or capacity > SOC_MAX */
		if (capacity < SOC_MIN)
			return SHOWCASE_CHARGING_RELEASE;
		else if (SOC_MAX < capacity)
			return SHOWCASE_CHARGING_BLOCK;

		/* Check SOC_MIN <= capacity <= SOC_MAX */
		if (capacity == SOC_MAX) {
			if (now == SHOWCASE_CHARGING_NONE)
				return SHOWCASE_CHARGING_BLOCK;
			else if (now == SHOWCASE_CHARGING_RELEASE)
				return SHOWCASE_CHARGING_HOLD;
		}

		if (now == SHOWCASE_CHARGING_NONE)
			return SHOWCASE_CHARGING_RELEASE;
		else
			return now; // not changed
	}
	else
		return SHOWCASE_CHARGING_NONE;
}

static bool showcase_create_parsedt(struct device_node* dnode)
{
	char property[MAX_PROPERTY_LENGTH];
	struct { u32 min; u32 max; } range;

	unified_bootmode_get_property_operator("lge,soc-range-", property);
	pr_showcase("property %s\n", property);
	if (!of_find_property(dnode, property, NULL))
		strncpy(property, "lge,soc-range-default", 32);

	// Finding soc range
	if (!of_property_read_u32_array(dnode, property, (u32*)(&range), 2)) {
		showcase.soc_min = range.min;
		showcase.soc_max = range.max;
		pr_showcase("soc-min : %d, soc-max : %d of %s\n", showcase.soc_min, showcase.soc_max, property);
		return true;
	}
	else {
		pr_showcase("Couldn't find %s\n", property);
		return false;
	}
}

#define SHOWCASE_VOTER "SHOWCASE"
static bool showcase_create_voters(void)
{
	return veneer_voter_register(&showcase.voter_ibat, SHOWCASE_VOTER, VOTER_TYPE_IBAT, false)
		&& veneer_voter_register(&showcase.voter_iusb, SHOWCASE_VOTER, VOTER_TYPE_IUSB, true)
		&& veneer_voter_register(&showcase.voter_idc, SHOWCASE_VOTER, VOTER_TYPE_IDC, true);
}

void protection_showcase_update(void)
{
	enum showcase_status status_now = showcase.charging_status;
	enum showcase_status status_new;
	int enabled, charging, capacity, val = 1;

	if (!showcase.enable)
		return;

	if (!get_veneer_param(VENEER_FEED_CHARING_SHOWCASE, &enabled)
			&& !get_veneer_param(VENEER_FEED_CHARGING, &charging)
			&& !get_veneer_param(VENEER_FEED_CAPACITY, &capacity)) {
		status_new = showcase_charging_transit(enabled, charging, capacity, status_now);

		if (status_now != status_new) {
			status_to_vote(status_new);
			showcase.charging_status = status_new;
			pr_showcase("charging is in progress : %s\n", status_to_string(status_new));
			set_veneer_param(VENEER_FEED_POWER_SUPPLY_CHANGED, val);
		}
	}
	else {
		pr_showcase("Updating showcase charging is not ready\n");
	}
}

bool protection_showcase_create(struct device_node* dnode)
{
	if (!(dnode && of_device_is_available(dnode))) {
		pr_showcase("protection_batvolt disabled.\n");
		return true;
	}
	showcase.enable = true;

	if (!showcase_create_parsedt(dnode)) {
		pr_showcase("error on showcase_create_devicetree\n");
		goto destroy;
	}

	if (!showcase_create_voters()) {
		pr_showcase("error on showcase_create_voters\n");
		goto destroy;
	}

	pr_showcase("Complete to create\n");
	return true;

destroy:
	protection_showcase_destroy();
	return false;
}

void protection_showcase_destroy(void)
{
	veneer_voter_unregister(&showcase.voter_ibat);
	veneer_voter_unregister(&showcase.voter_iusb);
	veneer_voter_unregister(&showcase.voter_idc);

	memset(&showcase, 0 ,sizeof(showcase));
}
