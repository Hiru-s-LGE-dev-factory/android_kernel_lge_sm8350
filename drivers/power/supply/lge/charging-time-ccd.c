/*
 *   V2 : auto calculating algorithm
 *   V3 : V3 is based on V2. It supports ACTM.
 */
#define pr_fmt(fmt) "CHG: [CHGTIME] %s: " fmt, __func__
#define pr_chgtime(fmt, ...) pr_err(fmt, ##__VA_ARGS__)

#include <linux/of.h>
#include <linux/slab.h>
#include "veneer-primitives.h"

#define BCC_VOTER           "BCC"

static struct ccdttf {
	bool enable;
	struct voter_entry ibat_voter;
	int bcc_current;
	int bsm_ttf;
} ccdttf_me = {0, };

int charging_time_remains(void)
{
	int result = -1;
	if (!ccdttf_me.enable)
		return -1;

	get_veneer_param(VENEER_FEED_TTF, &result);

	return result;
}

/* ttf main function */
bool charging_time_update(void)
{
	int bsm_ttf = 0, val = 1;

	if(!ccdttf_me.enable)
		return true;

	if (!get_veneer_param(VENEER_FEED_BSM_TTF, &bsm_ttf))
		return false;

	if (ccdttf_me.bsm_ttf != bsm_ttf) {
		if (bsm_ttf)
			veneer_voter_set(&ccdttf_me.ibat_voter, ccdttf_me.bcc_current);
		else
			veneer_voter_release(&ccdttf_me.ibat_voter);
		set_veneer_param(VENEER_FEED_FORCE_UPDATE, val);
	}

	return true;
}

bool charging_time_create(struct device_node* dnode)
{
	if (!(dnode && of_device_is_available(dnode))) {
		pr_chgtime("charging_time disabled.\n");
		return true;
	}

	ccdttf_me.enable = true;
	veneer_voter_register(&ccdttf_me.ibat_voter, BCC_VOTER, VOTER_TYPE_IBAT, false);
	if (of_property_read_u32(dnode, "lge,bcc-charging-current", &ccdttf_me.bcc_current))
		ccdttf_me.bcc_current = 2000;

	return true;
}

void charging_time_clear(void) { return; }

void charging_time_destroy(void)
{
	veneer_voter_unregister(&ccdttf_me.ibat_voter);
}
