/* Protection bat volt
 * V2 : It's for direct battery cell voltage sensing */

#define pr_fmt(fmt) "CHG: [BVP-V2]: %s: " fmt, __func__
#define pr_batvolt(reason, fmt, ...)			\
do {							\
	if (pr_debugmask & (reason))			\
		pr_err(fmt, ##__VA_ARGS__);		\
	else						\
		pr_debug(fmt, ##__VA_ARGS__);		\
} while (0)

#include <linux/of.h>
#include <linux/of_platform.h>
//#include <linux/of_batterydata.h>
#include <linux/workqueue.h>

#include "veneer-primitives.h"

#define BVP_NOTREADY	INT_MAX
#define BVP_VOTER	"BVP"

static int pr_debugmask = ERROR | UPDATE;

static void protection_batvolt_work(struct work_struct *work);
static struct protection_batvolt_struct {
	struct voter_entry	bvp_voter;
	struct voter_entry	bvp_fv_voter;
	struct delayed_work	bvp_dwork;
	struct votable *fv_votable;

	/* thresholds */
	int			threshold_vbat_limit;
	int			threshold_ibat_rated;
	int			threshold_force_soc;
	int			threshold_cv_ibat_rated;

	/* dt contents */
	int			step_ibat_ma;
	int			step_poll_ms;
	bool enable;
	int poc_protect_volt;
	int poc_release_capacity;
	int poc_min_fcc;
	int poc_fv_step;
	int poc_fv_step_max_count;
	int poc_fv_step_count;
	int vadc_ir_comp_resist;
} bvp_me = {
	.bvp_voter	= { .type = VOTER_TYPE_INVALID },
	.bvp_fv_voter	= { .type = VOTER_TYPE_INVALID },
	.bvp_dwork	= __DELAYED_WORK_INITIALIZER(bvp_me.bvp_dwork,
		protection_batvolt_work, 0),
	.fv_votable = NULL,
	.threshold_vbat_limit		= BVP_NOTREADY,
	.threshold_ibat_rated		= BVP_NOTREADY,
	.threshold_cv_ibat_rated	= BVP_NOTREADY,

	.step_ibat_ma		= BVP_NOTREADY,
	.step_poll_ms		= BVP_NOTREADY,
	.enable = false,
	.poc_protect_volt = 4450,
	.poc_release_capacity = 90,
	.poc_min_fcc = 600,
	.poc_fv_step = 20,
	.poc_fv_step_max_count = 2,
	.poc_fv_step_count = 0,
	.vadc_ir_comp_resist = 0,
};

static void protection_batvolt_work(struct work_struct *work)
{
	// ibat_now: + is charging, - is discharging
	int icap_now = 0, chg_now = 0, batt_ocv = 0, vbat_now = 0;
	int status_raw = 0, ibat_now = 0, vbat_comp = 0;
	int step_ibat = bvp_me.step_ibat_ma;
	int capacity = 0, charger_type = 0;
	int default_fv_uV = 0, icap_new = 0, fv_new = 0;

	if (!bvp_me.fv_votable) {
		bvp_me.fv_votable = find_votable("FV");
	}

	if (bvp_me.fv_votable) {
		vbat_now = get_effective_result_locked(bvp_me.fv_votable);
		if (vbat_now < bvp_me.threshold_vbat_limit)
			goto done;
	}

	if (get_veneer_param(VENEER_FEED_FCC, &icap_now)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}
	if (get_veneer_param(VENEER_FEED_STATUS_RAW, &status_raw)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}
	if (get_latest_veneer_param(VENEER_FEED_CHARGE_STATUS, &chg_now)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}
	if (get_veneer_param(VENEER_FEED_OCV, &batt_ocv)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}
	if (get_veneer_param(VENEER_FEED_BAT_VOLTAGE_NOW, &vbat_now)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}
	if (get_veneer_param(VENEER_FEED_BATT_PSY_IBAT_NOW, &ibat_now)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}
	if (get_veneer_param(VENEER_FEED_CAPACITY, &capacity)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}
	if (get_veneer_param(VENEER_FEED_BATT_PROFILE_FV_VOTER, &default_fv_uV)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}

	vbat_comp = vbat_now;
	if (status_raw == POWER_SUPPLY_STATUS_CHARGING)
		vbat_comp = vbat_now + (ibat_now * bvp_me.vadc_ir_comp_resist) / 1000;

	pr_batvolt(MONITOR,
		"icap_now=%d, chg_now=%d, "
		"threshold_ibat_rated=%d, ocv=%dmV, vbat_now=%dmV, vbat_comp=%dmV, fv=%d\n",
		icap_now, chg_now, bvp_me.threshold_ibat_rated, batt_ocv, vbat_now, vbat_comp,
		veneer_voter_enabled(&bvp_me.bvp_fv_voter));

	if (get_veneer_param(VENEER_FEED_CHARGER_TYPE, &charger_type)) {
		pr_batvolt(UPDATE, "Host is not ready\n");
		goto done;
	}

	if ((charger_type == CHARGING_SUPPLY_FACTORY_56K)  ||
		(charger_type == CHARGING_SUPPLY_FACTORY_130K) ){
		pr_batvolt(UPDATE, "Factory cable type %d skip BVP\n", charger_type);
		goto done;
	}

	icap_new = max(0, icap_now - step_ibat);

	/* poc protection */
	if (vbat_comp >= bvp_me.poc_protect_volt) {
		if (icap_now > bvp_me.poc_min_fcc) {
			veneer_voter_set(&bvp_me.bvp_voter, icap_new);
			pr_batvolt(UPDATE,
				"reach to protection ic voltage..."
				"vbat_comp=%dmV, vbat_now=%dmV, ibat_now=%dmA, vadc_ir_resistance=%dmohm, "
				"ui soc=%d, Reduce current from %dmA to %dmA\n",
				vbat_comp, vbat_now, ibat_now, bvp_me.vadc_ir_comp_resist,
				capacity, icap_now, icap_new);
		}
		else {
			bvp_me.poc_fv_step_count =
				min(bvp_me.poc_fv_step_count+1, bvp_me.poc_fv_step_max_count);
			fv_new = (default_fv_uV/1000) - (bvp_me.poc_fv_step * bvp_me.poc_fv_step_count);
			veneer_voter_set(&bvp_me.bvp_fv_voter, fv_new);
			pr_batvolt(UPDATE,
				"reach to protection ic voltage... "
				"ui soc=%d, Reduce float voltage to %dmV(count=%d) at %dmA(min_fcc=%dmA)\n",
				capacity, fv_new, bvp_me.poc_fv_step_count, icap_now, bvp_me.poc_min_fcc);
		}

		goto done;
	}

	/* release poc protection */
	if (icap_now == 0 && capacity <= bvp_me.poc_release_capacity) {
		veneer_voter_release(&bvp_me.bvp_voter);
		pr_batvolt(UPDATE,
			"reach to safety poc area at soc=%d(<=%d), icap=%d(<=0mA)... "
			"release poc current limit\n",
			capacity, bvp_me.poc_release_capacity, icap_now);
		goto done;
	}

	/* set to 4.45V at force ui soc */
	if (capacity >= bvp_me.threshold_force_soc
			&& icap_now > bvp_me.threshold_ibat_rated) {
		veneer_voter_set(&bvp_me.bvp_voter, bvp_me.threshold_ibat_rated / 100 * 100);
		veneer_voter_release(&bvp_me.bvp_fv_voter);

		pr_batvolt(UPDATE,
			"reach to force 2nd fv area at soc=%d(>=%d), icap=%dmA(>%dmA)... "
			"set to default float voltage.\n",
			capacity, bvp_me.threshold_force_soc, icap_now, bvp_me.threshold_ibat_rated);

		goto done;
	}

	/* under 0.6C. set FV from 4.2V to 4.45V */
	if (bvp_me.poc_fv_step_count == 0 &&
		icap_now <= bvp_me.threshold_ibat_rated &&
		veneer_voter_enabled(&bvp_me.bvp_fv_voter)) {
		veneer_voter_release(&bvp_me.bvp_fv_voter);

		pr_batvolt(UPDATE,
			"reach to the end of 1st fv(=%dmV) area at fcc=%dmA(<=%dmA)... "
			"set to default float voltage.\n",
			bvp_me.threshold_vbat_limit, icap_now, bvp_me.threshold_ibat_rated);

		goto done;
	}

	/* over 0.6C. set FV to 4.2V */
	if (icap_now > bvp_me.threshold_ibat_rated
			&& !veneer_voter_enabled(&bvp_me.bvp_fv_voter)) {
		veneer_voter_set(&bvp_me.bvp_fv_voter, bvp_me.threshold_vbat_limit);

		pr_batvolt(UPDATE,
			"set 1st step float voltage: %dmV, icap=%dmA(>%dmA)\n",
			bvp_me.threshold_vbat_limit, icap_now, bvp_me.threshold_ibat_rated);

		goto done;
	}

	if (chg_now == CHARGE_STATUS_TAPER && capacity < 100) {
		/* check whether enter CV mode */
		if (icap_now <= bvp_me.threshold_cv_ibat_rated)
			goto done;

		veneer_voter_set(&bvp_me.bvp_voter, icap_new);

		pr_batvolt(UPDATE,
			"Taper.. ui soc=%d, Reduce current from %dmA to %dmA\n",
			capacity, icap_now, icap_new);
	}

done:
	schedule_delayed_work(
		to_delayed_work(work), msecs_to_jiffies(bvp_me.step_poll_ms));
	return;
}

void protection_batvolt_refresh(bool is_charging)
{
	static bool is_started = false;

	bool is_ready = bvp_me.threshold_vbat_limit != BVP_NOTREADY
		&& bvp_me.threshold_ibat_rated != BVP_NOTREADY
		&& is_charging;
	bool suspend = veneer_voter_suspended(VOTER_TYPE_IUSB) != CHARGING_NOT_SUSPENDED;
	int chg_now = 0;

	if (!bvp_me.enable)
		return;

	if (is_ready) {
		get_latest_veneer_param(VENEER_FEED_CHARGE_STATUS, &chg_now);
		if (!is_started && !suspend) {
			schedule_delayed_work(&bvp_me.bvp_dwork, 0);
			is_started = true;
		} else if (chg_now == CHARGE_STATUS_TAPER) {
			cancel_delayed_work_sync(&bvp_me.bvp_dwork);
			schedule_delayed_work(&bvp_me.bvp_dwork, 0);
		} else if (is_started && suspend) {
			veneer_voter_release(&bvp_me.bvp_voter);
			veneer_voter_release(&bvp_me.bvp_fv_voter);
			cancel_delayed_work_sync(&bvp_me.bvp_dwork);
			bvp_me.poc_fv_step_count = 0;
			is_started = false;
		}
	}
	else {
		veneer_voter_release(&bvp_me.bvp_voter);
		veneer_voter_release(&bvp_me.bvp_fv_voter);
		cancel_delayed_work_sync(&bvp_me.bvp_dwork);
		is_started = false;
		bvp_me.poc_fv_step_count = 0;
	}
}

bool protection_batvolt_create(struct device_node* dnode)
{
	int ret = 0, threshold_ibat_pct = 0, threshold_cv_ibat_pct = 0;
	int mincap = 0;

	if (!(dnode && of_device_is_available(dnode))) {
		pr_batvolt(ERROR, "protection_batvolt disabled.\n");
		return true;
	}
	bvp_me.enable = true;

	get_veneer_param(VENEER_FEED_MINCAP, &mincap);
	/* Parse device tree */
	ret = of_property_read_s32(dnode,
		"lge,threshold-ibat-rate", &threshold_ibat_pct);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,threshold-ibat-rate' ret=%d\n", ret);
		goto destroy;
	}
	else
		bvp_me.threshold_ibat_rated = mincap * threshold_ibat_pct / 100;

	ret = of_property_read_s32(dnode,
		"lge,threshold-force-soc", &bvp_me.threshold_force_soc);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,threshold-force-soc' ret=%d\n", ret);
		goto destroy;
	}

	ret = of_property_read_s32(dnode,
		"lge,threshold-ibat-cv-rate", &threshold_cv_ibat_pct);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,threshold-ibat-cv-rate' ret=%d\n", ret);
		goto destroy;
	}
	else
		bvp_me.threshold_cv_ibat_rated = mincap * threshold_cv_ibat_pct / 100;

	ret = of_property_read_s32(dnode,
		"lge,threshold-vbat-limit", &bvp_me.threshold_vbat_limit);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,threshold-vbat-limit' ret=%d\n", ret);
		goto destroy;
	}

	ret = of_property_read_s32(dnode,
		"lge,poc-protection-volt", &bvp_me.poc_protect_volt);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,poc-protection-volt' ret=%d\n", ret);
		goto destroy;
	}
	ret = of_property_read_s32(dnode,
		"lge,poc-protection-min_fcc", &bvp_me.poc_min_fcc);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,poc-protection-min_fcc' ret=%d\n", ret);
		goto destroy;
	}
	ret = of_property_read_s32(dnode,
		"lge,poc-protection-fv-step", &bvp_me.poc_fv_step);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,poc-protection-fv-step' ret=%d\n", ret);
		goto destroy;
	}
	ret = of_property_read_s32(dnode,
		"lge,poc-protection-fv-max-step", &bvp_me.poc_fv_step_max_count);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,poc-protection-fv-max-step' ret=%d\n", ret);
		goto destroy;
	}
	ret = of_property_read_s32(dnode,
		"lge,vadc-ir-comp-resistance", &bvp_me.vadc_ir_comp_resist);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,vadc-ir-comp-resistance' ret=%d\n", ret);
		bvp_me.vadc_ir_comp_resist = 0;
	}

	ret = of_property_read_s32(dnode,
		"lge,step-ibat-ma", &bvp_me.step_ibat_ma);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,step-ibat-ma' ret=%d\n", ret);
		goto destroy;
	}
	ret = of_property_read_s32(dnode,
		"lge,step-poll-ms", &bvp_me.step_poll_ms);
	if (ret < 0) {
		pr_batvolt(ERROR, "Failed to read 'lge,step-poll-ms' ret=%d\n", ret);
		goto destroy;
	}

	/* Register voter */
	if (!veneer_voter_register(
			&bvp_me.bvp_voter, BVP_VOTER, VOTER_TYPE_IBAT, false)) {
		pr_batvolt(ERROR, "Failed to register the BVP voter\n");
		goto destroy;
	}

	if (!veneer_voter_register(
			&bvp_me.bvp_fv_voter, BVP_VOTER, VOTER_TYPE_VFLOAT, true)) {
		pr_batvolt(ERROR, "Failed to register the BVP FV voter\n");
		goto destroy;
	}

	pr_batvolt(UPDATE, "Complete to create, "
		"threshold_vbat_limit=%dmV, "
		"threshold_ibat_rated=%dmA, threshold_cv_ibat_rated=%dmA, "
		"step_ibat_ma=%dmA, step_poll_ms=%dmsec, "
		"poc protection: volt=%dmV, fcc=%dmA, fv_step=%dmV, max_count=%d, "
		"vadc ir compensate resistance=%d\n",
		bvp_me.threshold_vbat_limit,
		bvp_me.threshold_ibat_rated, bvp_me.threshold_cv_ibat_rated,
		bvp_me.step_ibat_ma, bvp_me.step_poll_ms,
		bvp_me.poc_protect_volt, bvp_me.poc_min_fcc,
		bvp_me.poc_fv_step, bvp_me.poc_fv_step_max_count,
		bvp_me.vadc_ir_comp_resist);

	return true;

destroy:
	protection_batvolt_destroy();
	return false;
}

void protection_batvolt_destroy(void)
{
	cancel_delayed_work_sync(&bvp_me.bvp_dwork);
	veneer_voter_unregister(&bvp_me.bvp_voter);
	veneer_voter_unregister(&bvp_me.bvp_fv_voter);

	bvp_me.threshold_vbat_limit	= BVP_NOTREADY;
	bvp_me.threshold_ibat_rated	= BVP_NOTREADY;
	bvp_me.threshold_cv_ibat_rated	= BVP_NOTREADY;

	bvp_me.step_ibat_ma		= BVP_NOTREADY;
	bvp_me.step_poll_ms		= BVP_NOTREADY;
}
