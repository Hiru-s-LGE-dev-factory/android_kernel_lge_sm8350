#define pr_fmt(fmt) "CHG: [BTP] %s: " fmt, __func__
#define pr_battemp(reason, fmt, ...)			\
do {							\
	if (pr_debugmask & (reason))			\
		pr_info(fmt, ##__VA_ARGS__);		\
	else						\
		pr_debug(fmt, ##__VA_ARGS__);		\
} while (0)

static int pr_debugmask;

//#define DEBUG_BTP  //need to enable fake_battery when testing DEBUG_BTP

#include <linux/of.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>

#include "veneer-primitives.h"

#define BATTEMP_NOTREADY	INT_MAX
#define BATTEMP_WAKELOCK	"lge-btp-scenario"

#define VOTER_NAME_ICHARGE  "BTP(ibatt)"
#define VOTER_NAME_VFLOAT   "BTP(vbatt)"
#define VOTER_NAME_IUSB     "BTP(iusb)"
#define VOTER_NAME_IDC      "BTP(idc)"
#define VOTER_NAME_VFLOAT_IBAT	"BTP(OV)"
#define VOTER_NAME_CHILLY	"BTP(CHILLY)"

enum {
	HVT_CHG_RELEASED = 0,
	HVT_CHG_NOTCHG = 1,
	HVT_CHG_NOTCHG_HOLD = 2,
	HVT_CHG_DISCHG = 3,

	HVT_CHG_MAX,
};

enum {
	HVT_TIMER_RELEASED = 0,
	HVT_TIMER_ON = 1,
	HVT_TIMER_DONE = 2,

	HVT_TIMER_MAX,
};

static struct protection_battemp {
	struct delayed_work	battemp_dwork;
	struct wakeup_source *battemp_wakelock;
#ifdef DEBUG_BTP
	struct delayed_work	debug_btp_dwork;
#endif

	// processed in external

	struct voter_entry voter_ichilly;
	struct voter_entry voter_icharge;
	struct voter_entry voter_vfloat;
	struct voter_entry voter_vfloat_ibat;
	struct voter_entry voter_iusb;

	bool health_chilly;
	int  health_jeita;

// below fields are set in device tree
	int threshold_degc_upto_cool;	//  30 by default
	int threshold_degc_upto_good;	// 120 by default
	int threshold_degc_upto_warm;	// 450 by default
	int threshold_degc_upto_hot;	// 550 by default
	int threshold_degc_downto_warm;	// 520 by default
	int threshold_degc_downto_good;	// 430 by default
	int threshold_degc_downto_cool;	// 100 by default
	int threshold_degc_downto_cold;	//   0 by default

	int period_ms_emergency;	// 10000 by default
	int period_ms_warning;		// 30000 by default
	int period_ms_normal;		// 60000 by default
	const int period_ms_unknown;

	int cool_mv_alert;
	int cool_ma_alert;
	int cool_ma_normal;

	int warm_ma_charge;
	int warm_mv_float;

	// below fileds are for battery protection in chilly
	bool chilly_is_supported;
	int  chilly_degc_lowerbound;
	int  chilly_degc_upperbound;
	int  chilly_mv_hyst;
	int  chilly_mv_bound;
	int  chilly_mv_alert;
	int  chilly_ma_alert;
	int  chilly_ma_normal;
	bool enable;

	// discharging algorithm when hi temp and hi vbatt
	struct hvt_discharging {
		bool enable;                  /* prime key */
		int chg_status;
		int timer_status;
		int count;

		struct hvt_discharging_dt {
			int timer;
			int max_count;
			int upper_temp[MAX_HVT_STEP_SIZE];
			int lower_temp;
			int trigger_soc[MAX_HVT_STEP_SIZE];
			int exit_volt;
			int enter_volt;
		} dt;

		struct delayed_work	dwork;
	} hvt;

} battemp_me = {
	.voter_ichilly = { .type = VOTER_TYPE_INVALID },
	.voter_icharge = { .type = VOTER_TYPE_INVALID },
	.voter_vfloat  = { .type = VOTER_TYPE_INVALID },
	.voter_vfloat_ibat  = { .type = VOTER_TYPE_INVALID },
	.voter_iusb  = { .type = VOTER_TYPE_INVALID },

	.health_jeita  = POWER_SUPPLY_HEALTH_UNKNOWN,
	.health_chilly = false,

	.threshold_degc_upto_cool  = BATTEMP_NOTREADY,
	.threshold_degc_upto_good  = BATTEMP_NOTREADY,
	.threshold_degc_upto_warm  = BATTEMP_NOTREADY,
	.threshold_degc_upto_hot   = BATTEMP_NOTREADY,
	.threshold_degc_downto_warm = BATTEMP_NOTREADY,
	.threshold_degc_downto_good = BATTEMP_NOTREADY,
	.threshold_degc_downto_cool = BATTEMP_NOTREADY,
	.threshold_degc_downto_cold = BATTEMP_NOTREADY,

	.period_ms_emergency = BATTEMP_NOTREADY,
	.period_ms_warning   = BATTEMP_NOTREADY,
	.period_ms_normal    = BATTEMP_NOTREADY,
	.period_ms_unknown   = 30000,

	.cool_mv_alert	= BATTEMP_NOTREADY,
	.cool_ma_alert	= BATTEMP_NOTREADY,
	.cool_ma_normal	= BATTEMP_NOTREADY,
	.warm_ma_charge	= BATTEMP_NOTREADY,
	.warm_mv_float	= BATTEMP_NOTREADY,

	.chilly_is_supported    = false,
	.chilly_degc_upperbound = BATTEMP_NOTREADY,
	.chilly_degc_lowerbound = BATTEMP_NOTREADY,
	.chilly_mv_hyst		= BATTEMP_NOTREADY,
	.chilly_mv_bound	= BATTEMP_NOTREADY,
	.chilly_mv_alert	= BATTEMP_NOTREADY,
	.chilly_ma_alert	= BATTEMP_NOTREADY,
	.chilly_ma_normal       = BATTEMP_NOTREADY,
	.enable = false,

	.hvt = {
		.enable = false,
		.chg_status = HVT_CHG_RELEASED,
		.timer_status = HVT_TIMER_RELEASED,
		.count = 0,
		.dt = {
			.timer = 450000,
			.max_count = 8,
			.upper_temp = {
				360, 370, 380, 390,
				400, 400, 400, 400,
				400, 400, 400, 400},
			.lower_temp = 350,
			.trigger_soc = {
				9750, 9600, 9450, 9300,
				9175, 9050, 8925, 8800,
				8675, 8550, 8425, 8300},
			.exit_volt = 3900,
			.enter_volt = 4200,
		},
	},
};

#ifdef DEBUG_BTP
#define BTP_POWER_OFF_TEMP_IDX 22
#define BTP_POWER_OFF_WARNING_TEMP_IDX (BTP_POWER_OFF_TEMP_IDX-3)
#define BTP_POWER_OFF_CNT   3
#define BTP_RECHECK_PERIOD 60000

struct debug_btp {
	int temp;
	int capacity;
	int voltage;
};

const struct debug_btp debug_btp_table[BTP_POWER_OFF_TEMP_IDX] = {
	{-150, 80, 4000},   // HEALTH_COLD
	{-100, 80, 4000},   // HEALTH_COLD
	{-50, 80, 4000},    // HEALTH_COLD
	{0, 80, 4000},      // HEALTH_COLD HEALTH_CHIILY    (Charging Stop)
	{30, 80, 4000},     // HEALTH_COOL HEALTH_CHIILY    (Decrease Charging 0.3C Under 4V, Decrease Charging 0.2C Over 4V)
	{50, 80, 4000},     // HEALTH_COOL HEALTH_CHILLY    (Decrease Charging 0.3C Under 4V, Decrease Charging 0.2C Over 4V)
	{100, 80, 4000},    // HEALTH_COOL HEALTH_CHILLY    (Decrease Charging 0.3C Under 4V, Decrease Charging 0.2C Over 4V)
	{120, 80, 4000},    // HEALTH_GOOD HEALTH_CHILLY    (Up to Normal)
	{150, 80, 4000},    // HEALTH_GOOD                  (HEALTH_CHILLY : Acordding to Battery Spec)
	{200, 80, 4000},    // HEALTH_GOOD                  (HEALTH_CHIILY : Acordding to Battery Spec)
	{250, 80, 4000},    // HEALTH_GOOD
	{300, 80, 4000},    // HEALTH_GOOD
	{350, 80, 4000},    // HEALTH_GOOD
	{400, 80, 4000},    // HEALTH_GOOD
	{430, 80, 4000},    // HEALTH_GOOD                  (Down to Normal)
	{450, 80, 4000},    // HEALTH_WARM                  (Decrease Charging Under 4V, Charging Stop Over 4V)
	{500, 80, 4000},    // HEALTH_WARM                  (Decrease Charging Under 4V, Charging Stop Over 4V)
	{520, 80, 4000},    // HEALTH_WARM                  (Down to Warm)
	{550, 80, 4000},    // HEALTH_HOT                   (Charging Stop)
	{590, 80, 4000},    // HEALTH_HOT                   (VZW Cool Down Noti & Power Off Message)
	{600, 80, 4000},    // HEALTH_HOT                   (Imediately Power Off)
	{650, 80, 4000},    // HEALTH_HOT
};

static void debug_btp_polling_status_work(struct work_struct* work)
{
	struct power_supply* psy;
	union power_supply_propval prp_temp, prp_capacity, prp_voltagenow;
	static int tempstep = 0;
	static bool voltagelevel = false;
	static bool upward = true;
	static int power_off_cnt = 0;
	int fake_batt = 0;

	unified_nodes_read(UNIFIED_NODE_FAKE_BATTERY, &fake_batt);
	if (unified_bootmode_chargerlogo() && !fake_batt) //enable debug_btp in chargerlogo
		unified_nodes_write(UNIFIED_NODE_FAKE_BATTERY, 1);

	if (fake_batt) {
		if (power_off_cnt < BTP_POWER_OFF_CNT) {
			if(tempstep >= BTP_POWER_OFF_WARNING_TEMP_IDX) {
				upward = false;
			} else if(tempstep <= 0) {
				upward = true;
				voltagelevel = !voltagelevel;
				power_off_cnt++;
			}
		} else {
			if(tempstep >= BTP_POWER_OFF_TEMP_IDX - 1) {
				upward = false;
			} else if(tempstep <= 0) {
				upward = true;
				voltagelevel = !voltagelevel;
			}
		}

		psy = power_supply_get_by_name("battery");
		if(!psy) {
			schedule_delayed_work(to_delayed_work(work), msecs_to_jiffies(BTP_RECHECK_PERIOD));
			return;
		}

		prp_temp.intval = debug_btp_table[tempstep].temp;
		prp_capacity.intval = debug_btp_table[tempstep].capacity;
		if (!voltagelevel)
			prp_voltagenow.intval = (debug_btp_table[tempstep].voltage - 100) * 1000;
		else
			prp_voltagenow.intval = (debug_btp_table[tempstep].voltage + 100) * 1000;

		pr_battemp(UPDATE, "temp = %d, capacity = %d, voltage = %d\n",
				prp_temp.intval, prp_capacity.intval, prp_voltagenow.intval);
		if (psy) {
			power_supply_set_property(psy, POWER_SUPPLY_PROP_TEMP, &prp_temp);
			power_supply_set_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prp_capacity);
			power_supply_set_property(psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &prp_voltagenow);
			power_supply_put(psy);
		}

		if(upward)
			tempstep++;
		else
			tempstep--;

	} else {
		; //Nothing To Do
	}
	schedule_delayed_work(to_delayed_work(work), msecs_to_jiffies(BTP_RECHECK_PERIOD));
}
#endif

static const char* health_to_string(bool chilly, int jhealth)
{
	if (!chilly) {
		switch (jhealth) {
		case POWER_SUPPLY_HEALTH_UNKNOWN :
			return "HEALTH_UNKNOWN";
		case POWER_SUPPLY_HEALTH_COLD :
			return "HEALTH_COLD";
		case POWER_SUPPLY_HEALTH_COOL :
			return "HEALTH_COOL";
		case POWER_SUPPLY_HEALTH_GOOD :;
			return "HEALTH_GOOD";
		case POWER_SUPPLY_HEALTH_WARM :
			return "HEALTH_WARM";
		case POWER_SUPPLY_HEALTH_HOT :
			return "HEALTH_HOT";
		default :
			return "Undefined health";
		}
	}
	else
		return "HEALTH_CHILLY";
}

static int health_to_index(bool chilly, int jhealth)
{
	if (!chilly) {
		switch (jhealth) {
		case POWER_SUPPLY_HEALTH_UNKNOWN :
			return 0;
		case POWER_SUPPLY_HEALTH_COLD :
			return 1;
		case POWER_SUPPLY_HEALTH_COOL :
			return 2;
		case POWER_SUPPLY_HEALTH_GOOD :;
			return 3 + battemp_me.chilly_is_supported;
		case POWER_SUPPLY_HEALTH_WARM :
			return 4 + battemp_me.chilly_is_supported;
		case POWER_SUPPLY_HEALTH_HOT :
			return 5 + battemp_me.chilly_is_supported;
		default :
			return -1;
		}
	}
	else
		return 3;
}

static long health_to_period(int hvt_period_level, bool chilly, int jhealth)
{
	int msecs = 0;

	if (hvt_period_level == 1)  /* not-charging */
		return msecs_to_jiffies(battemp_me.period_ms_warning);
	else if (hvt_period_level == 2) /* dis-charging. prevent from discharging to 99% */
		return msecs_to_jiffies(battemp_me.period_ms_emergency);

	if (!chilly) {
		switch (jhealth) {
		case POWER_SUPPLY_HEALTH_HOT :
		case POWER_SUPPLY_HEALTH_COLD :
			msecs = battemp_me.period_ms_emergency;
			break;
		case POWER_SUPPLY_HEALTH_WARM :
		case POWER_SUPPLY_HEALTH_COOL :
			msecs = battemp_me.period_ms_warning;
			break;
		case POWER_SUPPLY_HEALTH_GOOD :
			msecs = battemp_me.period_ms_normal;
			break;
		case POWER_SUPPLY_HEALTH_UNKNOWN :
			msecs = battemp_me.period_ms_unknown;
			break;
		default :
			pr_battemp(ERROR, "Check the 'health'\n");
			break;
		}
	}
	else
		msecs = battemp_me.period_ms_warning;

	return msecs_to_jiffies(msecs);
}

static int icharge_by_chilly(bool chilly, int batvol)
{
	if (chilly) {
		if(battemp_me.chilly_mv_hyst <= batvol) {
			battemp_me.chilly_mv_hyst = battemp_me.chilly_mv_alert - battemp_me.chilly_mv_bound;
			pr_battemp(ERROR, "make hysterisis voltage 4000 to 3800 mV\n");
			return battemp_me.chilly_ma_alert;
		} else {
			battemp_me.chilly_mv_hyst = battemp_me.chilly_mv_alert;
			pr_battemp(ERROR, "Not in hysterisis threshold 4000 mV\n");
			return battemp_me.chilly_ma_normal;
		}
	} else {
		battemp_me.chilly_mv_hyst = battemp_me.chilly_mv_alert;
		return VOTE_TOTALLY_RELEASED;
	}
}

static int icharge_by_jhealth(int jhealth, int batvol)
{
	switch (jhealth) {
	case POWER_SUPPLY_HEALTH_COOL :
		return (battemp_me.cool_mv_alert <= batvol)
			? battemp_me.cool_ma_alert : battemp_me.cool_ma_normal;
	case POWER_SUPPLY_HEALTH_WARM :
		return battemp_me.warm_ma_charge;

	case POWER_SUPPLY_HEALTH_COLD :
	case POWER_SUPPLY_HEALTH_HOT :
		return VOTE_TOTALLY_BLOCKED;

	case POWER_SUPPLY_HEALTH_GOOD :
	case POWER_SUPPLY_HEALTH_UNKNOWN :
		return VOTE_TOTALLY_RELEASED;

	default :
		return -EINVAL;
	}
}

static int vfloat_by_jhealth(int jhealth)
{
	switch (jhealth) {
	case POWER_SUPPLY_HEALTH_GOOD :
	case POWER_SUPPLY_HEALTH_COOL :
	case POWER_SUPPLY_HEALTH_COLD :
	case POWER_SUPPLY_HEALTH_UNKNOWN :
		return VOTE_TOTALLY_RELEASED;

	case POWER_SUPPLY_HEALTH_HOT :
	case POWER_SUPPLY_HEALTH_WARM :
		return battemp_me.warm_mv_float;

	default :
		return -EINVAL;
	}
}

#define STAT_NOW (health_now)
#define TEMP_NOW (battemp_now)

#define TEMP_UPTO_COOL (battemp_me.threshold_degc_upto_cool)		//	 30 by default
#define TEMP_UPTO_GOOD (battemp_me.threshold_degc_upto_good)		//	120 by default
#define TEMP_UPTO_WARM (battemp_me.threshold_degc_upto_warm)		//	450 by default
#define TEMP_UPTO_HOT (battemp_me.threshold_degc_upto_hot)		//	550 by default
#define TEMP_DOWNTO_WARM (battemp_me.threshold_degc_downto_warm)	//	520 by default
#define TEMP_DOWNTO_GOOD (battemp_me.threshold_degc_downto_good)	//	430 by default
#define TEMP_DOWNTO_COOL (battemp_me.threshold_degc_downto_cool)	//	100 by default
#define TEMP_DOWNTO_COLD (battemp_me.threshold_degc_downto_cold)	//	  0 by default
static int polling_status_jeita(int health_now, int battemp_now)
{
	int health_new;

	switch (STAT_NOW) {
	case POWER_SUPPLY_HEALTH_UNKNOWN :
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			health_new = POWER_SUPPLY_HEALTH_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_COOL)
			health_new = POWER_SUPPLY_HEALTH_COOL;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			health_new = POWER_SUPPLY_HEALTH_GOOD;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			health_new = POWER_SUPPLY_HEALTH_WARM;
		else
			health_new = POWER_SUPPLY_HEALTH_HOT;
		break;

	case POWER_SUPPLY_HEALTH_COLD : // on the cold
		if (TEMP_NOW < TEMP_UPTO_COOL)
			health_new = POWER_SUPPLY_HEALTH_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_COOL)
			health_new = POWER_SUPPLY_HEALTH_COOL;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			health_new = POWER_SUPPLY_HEALTH_GOOD;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			health_new = POWER_SUPPLY_HEALTH_WARM;
		else
			health_new = POWER_SUPPLY_HEALTH_HOT;
		break;

	case POWER_SUPPLY_HEALTH_COOL : // on the cool
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			health_new = POWER_SUPPLY_HEALTH_COLD;
		else if (TEMP_NOW < TEMP_UPTO_GOOD)
			health_new = POWER_SUPPLY_HEALTH_COOL;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			health_new = POWER_SUPPLY_HEALTH_GOOD;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			health_new = POWER_SUPPLY_HEALTH_WARM;
		else
			health_new = POWER_SUPPLY_HEALTH_HOT;
		break;

	case POWER_SUPPLY_HEALTH_GOOD : // on the normal
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			health_new = POWER_SUPPLY_HEALTH_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_COOL)
			health_new = POWER_SUPPLY_HEALTH_COOL;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			health_new = POWER_SUPPLY_HEALTH_GOOD;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			health_new = POWER_SUPPLY_HEALTH_WARM;
		else
			health_new = POWER_SUPPLY_HEALTH_HOT;
		break;

	case POWER_SUPPLY_HEALTH_WARM : // on the warm
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			health_new = POWER_SUPPLY_HEALTH_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_COOL)
			health_new = POWER_SUPPLY_HEALTH_COOL;
		else if (TEMP_NOW < TEMP_DOWNTO_GOOD )
			health_new = POWER_SUPPLY_HEALTH_GOOD;
		else if (TEMP_NOW < TEMP_UPTO_HOT)
			health_new = POWER_SUPPLY_HEALTH_WARM;
		else
			health_new = POWER_SUPPLY_HEALTH_HOT;
		break;

	case POWER_SUPPLY_HEALTH_HOT : // on the hot
		if (TEMP_NOW < TEMP_DOWNTO_COLD)
			health_new = POWER_SUPPLY_HEALTH_COLD;
		else if (TEMP_NOW < TEMP_DOWNTO_COOL)
			health_new = POWER_SUPPLY_HEALTH_COOL;
		else if (TEMP_NOW < TEMP_UPTO_WARM )
			health_new = POWER_SUPPLY_HEALTH_GOOD;
		else if (TEMP_NOW < TEMP_DOWNTO_WARM)
			health_new = POWER_SUPPLY_HEALTH_WARM;
		else
			health_new = POWER_SUPPLY_HEALTH_HOT;
		break;
	default :
		health_new = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	return health_new;
}

static void battemp_update_vfloat(int updated_vfloat, int mvoltage)
{
	if (updated_vfloat == VOTE_TOTALLY_RELEASED) {
		if (veneer_voter_enabled(&battemp_me.voter_vfloat)) {
			veneer_voter_set(&battemp_me.voter_vfloat, updated_vfloat);
			veneer_voter_set(&battemp_me.voter_vfloat_ibat, VOTE_TOTALLY_BLOCKED);
		} else {
			veneer_voter_set(&battemp_me.voter_vfloat, updated_vfloat);
		}
		veneer_voter_set(&battemp_me.voter_vfloat_ibat, VOTE_TOTALLY_RELEASED);
	} else if (updated_vfloat < mvoltage && !veneer_voter_enabled(&battemp_me.voter_vfloat)) {
		veneer_voter_set(&battemp_me.voter_vfloat_ibat, VOTE_TOTALLY_BLOCKED);
		veneer_voter_set(&battemp_me.voter_vfloat, VOTE_TOTALLY_RELEASED);
	} else {
		veneer_voter_set(&battemp_me.voter_vfloat, updated_vfloat);
		veneer_voter_set(&battemp_me.voter_vfloat_ibat, VOTE_TOTALLY_RELEASED);
	}
}

#define CHILLY_TEMP_LOWERBOUND (battemp_me.chilly_degc_lowerbound)
#define CHILLY_TEMP_UPPERBOUND (battemp_me.chilly_degc_upperbound)
static bool polling_status_chilly(int battemp_now)
{
	return battemp_me.chilly_is_supported
		&& CHILLY_TEMP_LOWERBOUND <= battemp_now && battemp_now <= CHILLY_TEMP_UPPERBOUND;
}

/*******************************************************/
/*    HVT - high voltage and temperature algorithm     */
/*******************************************************/
static void hvt_set_timer(bool en)
{
	if (en && battemp_me.hvt.timer_status == HVT_TIMER_RELEASED) {
		battemp_me.hvt.timer_status = HVT_TIMER_ON;
		schedule_delayed_work(
			&battemp_me.hvt.dwork, msecs_to_jiffies(battemp_me.hvt.dt.timer));
	}
	else if (!en) {
		battemp_me.hvt.timer_status = HVT_TIMER_RELEASED;
		cancel_delayed_work(&battemp_me.hvt.dwork);
	}
}

static void hvt_timer_work(struct work_struct* work)
{
	if (!battemp_me.hvt.enable || !unified_bootmode_usermode()) {
		pr_battemp(MONITOR, "[HVT] ERROR: hvt isn't enabled.\n");
		return;
	}

	battemp_me.hvt.timer_status = HVT_TIMER_DONE;
	pr_battemp(UPDATE, "[HVT] timer is done. count=%d\n", battemp_me.hvt.count);
}

static void hvt_set_chg_status(int status)
{
	if (battemp_me.hvt.chg_status == status)
		return;

	switch (status) {
		case HVT_CHG_DISCHG:
			veneer_voter_set(&battemp_me.voter_iusb, 0);
			break;
		case HVT_CHG_NOTCHG:
		case HVT_CHG_NOTCHG_HOLD:
		case HVT_CHG_RELEASED:
			veneer_voter_release(&battemp_me.voter_iusb);
			break;
	}
}

static int hvt_chg_policy(int mono_soc, int batt_temp)
{
	char str_chg[HVT_CHG_MAX][13] = {"released", "not-chg", "not-chg-hold", "dis-chg"};
	int status = battemp_me.hvt.chg_status;
	int release_soc = 0;
	int lower_temp = battemp_me.hvt.dt.lower_temp;
	int upper_temp = (battemp_me.hvt.count >= battemp_me.hvt.dt.max_count) ?
			battemp_me.hvt.dt.upper_temp[battemp_me.hvt.dt.max_count-1]
			: battemp_me.hvt.dt.upper_temp[battemp_me.hvt.count];
	int max_soc = (battemp_me.hvt.count >= battemp_me.hvt.dt.max_count) ?
			battemp_me.hvt.dt.trigger_soc[battemp_me.hvt.dt.max_count-1]
			: battemp_me.hvt.dt.trigger_soc[battemp_me.hvt.count];
	release_soc = max_soc - 200;

	switch (status) {
		case HVT_CHG_RELEASED:
			if (mono_soc >= max_soc                 /* 97.5% */
			    && mono_soc <= (max_soc + 50))      /* 98.0% */
				status = HVT_CHG_NOTCHG;
			else if (mono_soc > (max_soc + 50))     /* 98.0% */
				status = HVT_CHG_DISCHG;
			break;
		case HVT_CHG_NOTCHG:
		case HVT_CHG_NOTCHG_HOLD:
			if (batt_temp >= upper_temp)
				status = HVT_CHG_NOTCHG;

			if (mono_soc > (max_soc + 50))          /* 98.0% */
				status = HVT_CHG_DISCHG;
			else if (mono_soc <= release_soc)       /* 95.5% */
				status = HVT_CHG_RELEASED;
			break;
		case HVT_CHG_DISCHG:
			if (mono_soc <= (max_soc + 50))         /* 97.5% */
				status = HVT_CHG_NOTCHG;
			else if (mono_soc <= release_soc)       /* 95.5% */
				status = HVT_CHG_RELEASED;
			break;
	}

	if (status > HVT_CHG_RELEASED && batt_temp < upper_temp)
		status = HVT_CHG_NOTCHG_HOLD;
	if (batt_temp < lower_temp)
		status = HVT_CHG_RELEASED;

	if (battemp_me.hvt.chg_status != status) {
		pr_battemp(UPDATE,
			"[HVT] chg restriction is changed = %s -> %s, "
			"batt_temp=%d[>=%d, <%d], "
			"mono_soc=%d, max_soc=%d, release_soc=%d\n",
			str_chg[battemp_me.hvt.chg_status], str_chg[status],
			batt_temp, upper_temp, lower_temp,
			mono_soc, max_soc, release_soc);
	}

	return status;
}

static int hvt_get_trigger(bool present, int ui_soc, int mono_soc, int batt_temp)
{
	static bool pre_present = false;
	bool trigger = battemp_me.hvt.chg_status > HVT_CHG_RELEASED ? true : false;
	int chg_status = HVT_CHG_RELEASED, dc_present = 0, voltage_now = 0;
	int upper_temp = (battemp_me.hvt.count >= battemp_me.hvt.dt.max_count) ?
			battemp_me.hvt.dt.upper_temp[battemp_me.hvt.dt.max_count-1]
			: battemp_me.hvt.dt.upper_temp[battemp_me.hvt.count];

	/* charging is disable over 45degree during wireless charging */
	if (!get_veneer_param(VENEER_FEED_DC_PRESENT, &dc_present)) {
		if (dc_present) {
			pr_battemp(MONITOR, "[HVT] ERROR: hvt doesn't support wireless.\n");
			return chg_status;
		}
	}

	get_veneer_param(VENEER_FEED_BAT_VOLTAGE_NOW, &voltage_now);
	if (present && !trigger && ui_soc >= 100
		&& batt_temp >= upper_temp
		&& voltage_now > battemp_me.hvt.dt.enter_volt) {
		trigger = true;
	} else if (!present || ui_soc < 100
		|| (voltage_now < battemp_me.hvt.dt.exit_volt )
		|| (  batt_temp < battemp_me.hvt.dt.lower_temp)) {
		trigger = false;
	}

	if (trigger)
		chg_status = hvt_chg_policy(mono_soc, batt_temp);

	if ((chg_status > HVT_CHG_RELEASED ? 1 : 0) !=
		(battemp_me.hvt.chg_status > HVT_CHG_RELEASED ? 1 : 0))
		pr_battemp(UPDATE,
			"[HVT] hvt trigger is changed..trigger=%d, "
			"present=%d, ui_soc=%d, batt_temp=%d[>=%d, <%d], vbat=%d[>%d, <%d]\n",
			trigger, present, ui_soc,
			batt_temp, upper_temp, battemp_me.hvt.dt.lower_temp,
			voltage_now, battemp_me.hvt.dt.enter_volt, battemp_me.hvt.dt.exit_volt);

	pre_present = present;
	return chg_status;
}

static bool hvt_process(bool present)
{
	char str_timer[3][10] = {"released", "on-timer", "done"};
	char str_chg[HVT_CHG_MAX][13] = {"released", "not-chg", "not-chg-hold", "dis-chg"};
	int ui_soc = 0, mono_soc = 0, sys_soc = 0, batt_temp = 0, fg_hvt_count = 0;
	int chg_status = HVT_CHG_RELEASED;
	int upper_temp = 0, voltage_now = 0;

	if (!battemp_me.hvt.enable || !unified_bootmode_usermode()) {
		pr_battemp(MONITOR, "[HVT] ERROR: hvt isn't enabled.\n");
		goto out_process_hvt;
	}

	if (get_veneer_param(VENEER_FEED_CAPACITY, &ui_soc))
		goto out_process_hvt;
	if (get_veneer_param(VENEER_FEED_QBG_MSOC_X100, &mono_soc))
		goto out_process_hvt;
	if (get_veneer_param(VENEER_FEED_QBG_SYS_SOC_X100, &sys_soc))
		goto out_process_hvt;
	if (get_veneer_param(VENEER_FEED_SENSOR_BATT, &batt_temp))
		goto out_process_hvt;
	if (get_veneer_param(VENEER_FEED_BAT_VOLTAGE_NOW, &voltage_now))
		goto out_process_hvt;
	if (get_veneer_param(VENEER_FEED_HVT_SOC_RESCALE_COUNT, &fg_hvt_count))
		goto out_process_hvt;

	upper_temp = (battemp_me.hvt.count >= battemp_me.hvt.dt.max_count) ?
		battemp_me.hvt.dt.upper_temp[battemp_me.hvt.dt.max_count-1]
		: battemp_me.hvt.dt.upper_temp[battemp_me.hvt.count];

	if (mono_soc <= (sys_soc + 100))
		chg_status = hvt_get_trigger(present, ui_soc, mono_soc, batt_temp);
	else
		chg_status = hvt_get_trigger(present, ui_soc, sys_soc, batt_temp);

	hvt_set_chg_status(chg_status);
	switch (chg_status) {
		case HVT_CHG_RELEASED:
			battemp_me.hvt.count = 0;
			hvt_set_timer(false);
			break;

		case HVT_CHG_NOTCHG:
			/* if timer is completed */
			if (battemp_me.hvt.count < battemp_me.hvt.dt.max_count) {
				if (battemp_me.hvt.timer_status == HVT_TIMER_DONE) {
					battemp_me.hvt.count++;
					battemp_me.hvt.timer_status = HVT_TIMER_RELEASED;
					set_veneer_param(VENEER_FEED_HVT_SOC_RESCALE_COUNT, battemp_me.hvt.count);
					/* send the updated status to extension-qti */
					get_veneer_param(VENEER_FEED_CAPACITY, &ui_soc);

					pr_battemp(UPDATE,
						"[HVT] hvt count is updated..count=%d(fg=%d), ui=%d, "
						"msoc=%d, sys_soc=%d, volt=%d, timer is %s, present=%d, temp=%d[>=%d, <%d]\n",
						battemp_me.hvt.count, fg_hvt_count, ui_soc, mono_soc, sys_soc, voltage_now,
						str_timer[battemp_me.hvt.timer_status],
						present, batt_temp,
						upper_temp,
						battemp_me.hvt.dt.lower_temp);
				}
				hvt_set_timer(true);
			}
			break;

		case HVT_CHG_NOTCHG_HOLD:
			break;

		case HVT_CHG_DISCHG:
			if (battemp_me.hvt.count < battemp_me.hvt.dt.max_count)
				hvt_set_timer(true);
			break;
	}

	if (battemp_me.hvt.chg_status != chg_status) {
		battemp_me.hvt.chg_status = chg_status;
		pr_battemp(UPDATE,
			"[HVT] hvt is %s(%s)..count=%d(fg=%d), ui=%d, "
			"msoc=%d, sys_soc=%d, volt=%d, timer is %s, present=%d, temp=%d[>=%d, <%d]\n",
			(!!chg_status) ? "enabled" : "disabled",
			str_chg[battemp_me.hvt.chg_status],
			battemp_me.hvt.count, fg_hvt_count, ui_soc, mono_soc, sys_soc, voltage_now,
			str_timer[battemp_me.hvt.timer_status],
			present, batt_temp,
			upper_temp,
			battemp_me.hvt.dt.lower_temp);
	} else {
		pr_battemp(MONITOR,
			"[HVT] hvt is %s(%s)..count=%d, fg_count=%d, ui=%d, "
			"msoc=%d, sys_soc=%d, volt=%d, timer is %s, present=%d, temp=%d[>=%d, <%d]\n",
			(!!chg_status) ? "enabled" : "disabled",
			str_chg[battemp_me.hvt.chg_status],
			battemp_me.hvt.count, fg_hvt_count, ui_soc, mono_soc, sys_soc, voltage_now,
			str_timer[battemp_me.hvt.timer_status],
			present, batt_temp,
			upper_temp,
			battemp_me.hvt.dt.lower_temp);
	}

out_process_hvt:
	return chg_status;
}
/*******************************************************/

#define UPDATE_TEMP_THRESHOLD 430
static void polling_status_work(struct work_struct* work)
{
	static int btp_temp = INT_MAX;
	int charging = 0, temperature = 0, mvoltage = 0, val = 0;
	int health_jeita = 0, updated_icharge = 0, updated_vfloat = 0, updated_ichilly = 0;
	bool health_chilly = false, warning_at_charging = false, warning_wo_charging = false;
	int hvt_chg_status = HVT_CHG_RELEASED, hvt_period_level = 0;

	if (!get_veneer_param(VENEER_FEED_CHARGING, &charging)
			&& !get_veneer_param(VENEER_FEED_SENSOR_BATT, &temperature)
			&& !get_veneer_param(VENEER_FEED_BAT_VOLTAGE_NOW, &mvoltage)) {
		// Calculates icharge and vfloat from the jeita health
		health_jeita = polling_status_jeita(battemp_me.health_jeita, temperature);
		updated_icharge = charging ? icharge_by_jhealth(health_jeita, mvoltage) : VOTE_TOTALLY_RELEASED;
		updated_vfloat = charging ? vfloat_by_jhealth(health_jeita) : VOTE_TOTALLY_RELEASED;

		// And ichilly from the boolean 'chilly' status
		health_chilly = polling_status_chilly(temperature);
		updated_ichilly = charging ? icharge_by_chilly(health_chilly, mvoltage) : VOTE_TOTALLY_RELEASED;

		// configure wakelock
		warning_at_charging = charging && (health_jeita != POWER_SUPPLY_HEALTH_GOOD);
		warning_wo_charging = health_jeita == POWER_SUPPLY_HEALTH_HOT;

		hvt_chg_status = hvt_process(charging);
		if (hvt_chg_status == HVT_CHG_NOTCHG || hvt_chg_status == HVT_CHG_NOTCHG_HOLD) {
			hvt_period_level = 1;
			updated_vfloat = battemp_me.warm_mv_float;
		}
		else if (hvt_chg_status == HVT_CHG_DISCHG)
			hvt_period_level = 2;

		if (hvt_chg_status > HVT_CHG_RELEASED
			|| warning_at_charging || warning_wo_charging) {
			if (!battemp_me.battemp_wakelock->active) {
				pr_battemp(UPDATE, "Acquiring wake lock\n");
				__pm_stay_awake(battemp_me.battemp_wakelock);
			}
		}
		else {
			if (battemp_me.battemp_wakelock->active) {
				pr_battemp(UPDATE, "Releasing wake lock\n");
				__pm_relax(battemp_me.battemp_wakelock);
			}
		}

		// logging for changes
		if (battemp_me.health_chilly != health_chilly || battemp_me.health_jeita != health_jeita)
			pr_battemp(UPDATE, "%s(%d) -> %s(%d), temperature=%d, mvoltage=%d\n",
				health_to_string(battemp_me.health_chilly, battemp_me.health_jeita),
					health_to_index(battemp_me.health_chilly, battemp_me.health_jeita),
				health_to_string(health_chilly, health_jeita),
					health_to_index(health_chilly, health_jeita),
				temperature, mvoltage);

		// Voting for icharge and vfloat
		veneer_voter_set(&battemp_me.voter_ichilly, updated_ichilly);
		veneer_voter_set(&battemp_me.voter_icharge, updated_icharge);
		battemp_update_vfloat(updated_vfloat, mvoltage);

		// update member status in 'battemp_me'
		battemp_me.health_chilly = health_chilly;
		battemp_me.health_jeita = health_jeita;

		// finallly, notify the psy-type health to client
		set_veneer_param(VENEER_FEED_PSEUDO_HEALTH, battemp_me.health_jeita);
		if (btp_temp/10 != temperature/10) {
			btp_temp = temperature;
			if (temperature >= UPDATE_TEMP_THRESHOLD)
				set_veneer_param(VENEER_FEED_POWER_SUPPLY_CHANGED, val);
		}
	}
	else
		pr_battemp(UPDATE, "temperature is not valid.\n");

	schedule_delayed_work(to_delayed_work(work),
		health_to_period(hvt_period_level,
			battemp_me.health_chilly, battemp_me.health_jeita));
	return;
}

#define SCALE_UNIT_MA	50
static bool battemp_create_parsedt(struct device_node* dnode)
{
	int rc = 0;
	int cool_ma_pct = 0, warm_ma_pct = 0;
	int chilly_ma_pct = 0;
	int mincap = 0;
	struct device_node* tcomp_dtroot = NULL;

	get_veneer_param(VENEER_FEED_MINCAP, &mincap);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_upto_cool,
		"threshold-degc-upto-cool", rc);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_upto_good,
		"threshold-degc-upto-good", rc);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_upto_warm,
		"threshold-degc-upto-warm", rc);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_upto_hot,
		"threshold-degc-upto-hot", rc);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_downto_warm,
		"threshold-degc-downto-warm", rc);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_downto_good,
		"threshold-degc-downto-good", rc);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_downto_cool,
		"threshold-degc-downto-cool", rc);
	OF_PROP_READ_S32(dnode, battemp_me.threshold_degc_downto_cold,
		"threshold-degc-downto-cold", rc);

	OF_PROP_READ_S32(dnode, battemp_me.period_ms_emergency,
		"period-ms-emergency", rc);
	OF_PROP_READ_S32(dnode, battemp_me.period_ms_warning,
		"period-ms-warning", rc);
	OF_PROP_READ_S32(dnode, battemp_me.period_ms_normal,
		"period-ms-normal", rc);

	OF_PROP_READ_S32(dnode, battemp_me.cool_mv_alert,
		"cool-mv-alert", rc);
	OF_PROP_READ_S32(dnode, battemp_me.cool_ma_alert,
		"cool-ma-alert", rc);
	OF_PROP_READ_S32(dnode, cool_ma_pct, "cool-ma-pct", rc);
		battemp_me.cool_ma_normal = (mincap * cool_ma_pct / 100) / SCALE_UNIT_MA * SCALE_UNIT_MA;

	OF_PROP_READ_S32(dnode, battemp_me.warm_mv_float,
		"warm-mv-float", rc);
	OF_PROP_READ_S32(dnode, warm_ma_pct, "warm-ma-pct", rc);
		battemp_me.warm_ma_charge = (mincap * warm_ma_pct / 100) / SCALE_UNIT_MA * SCALE_UNIT_MA;

	battemp_me.chilly_is_supported = of_property_read_bool(dnode, "lge,chilly-status-support");
	if (battemp_me.chilly_is_supported) {
		OF_PROP_READ_S32(dnode, battemp_me.chilly_degc_lowerbound,
			"chilly-degc-lowerbound", rc);
		OF_PROP_READ_S32(dnode, battemp_me.chilly_degc_upperbound,
			"chilly-degc-upperbound", rc);
		OF_PROP_READ_S32(dnode, battemp_me.chilly_mv_hyst,
			"chilly-mv-alert", rc);
		OF_PROP_READ_S32(dnode, battemp_me.chilly_mv_bound,
			"chilly-mv-bound", rc);
		OF_PROP_READ_S32(dnode, battemp_me.chilly_mv_alert,
			"chilly-mv-alert", rc);
		OF_PROP_READ_S32(dnode, battemp_me.chilly_ma_alert,
			"chilly-ma-alert", rc);
		OF_PROP_READ_S32(dnode, chilly_ma_pct, "chilly-ma-pct", rc);
			battemp_me.chilly_ma_normal =
				(mincap * chilly_ma_pct / 100) / SCALE_UNIT_MA * SCALE_UNIT_MA;
	}

	tcomp_dtroot = of_find_node_by_name(NULL, "lge-battery-supplement");
	battemp_me.hvt.enable = of_property_read_bool(tcomp_dtroot, "lge,hvt-enable");
	if (battemp_me.hvt.enable) {
		rc = of_property_read_u32(tcomp_dtroot,
			"lge,hvt-timer", &battemp_me.hvt.dt.timer);
		rc = of_property_read_u32(tcomp_dtroot,
			"lge,hvt-max-count", &battemp_me.hvt.dt.max_count);
		rc = of_property_read_u32_array(tcomp_dtroot,
			"lge,hvt-upper-temp", battemp_me.hvt.dt.upper_temp, MAX_HVT_STEP_SIZE);
		rc = of_property_read_u32(tcomp_dtroot,
			"lge,hvt-lower-temp", &battemp_me.hvt.dt.lower_temp);
		rc = of_property_read_u32_array(tcomp_dtroot,
			"lge,hvt-trigger-soc-entry", battemp_me.hvt.dt.trigger_soc, MAX_HVT_STEP_SIZE);
		rc = of_property_read_u32(tcomp_dtroot,
			"lge,hvt-exit-voltage", &battemp_me.hvt.dt.exit_volt);
		rc = of_property_read_u32(tcomp_dtroot,
			"lge,hvt-enter-voltage", &battemp_me.hvt.dt.enter_volt);
	}
	pr_battemp(UPDATE,
		"[HVT] en=%d, timer=%d.%dmin, max_count=%d, exit_volt=%d, enter_volt=%d, "
		"upper_temp=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d, lower_temp=%d, "
		"max_soc=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		battemp_me.hvt.enable,
		battemp_me.hvt.dt.timer/1000/60, ((battemp_me.hvt.dt.timer/1000)%60)/6,
		battemp_me.hvt.dt.max_count,
		battemp_me.hvt.dt.exit_volt, battemp_me.hvt.dt.enter_volt,
		battemp_me.hvt.dt.upper_temp[0],
		battemp_me.hvt.dt.upper_temp[1],
		battemp_me.hvt.dt.upper_temp[2],
		battemp_me.hvt.dt.upper_temp[3],
		battemp_me.hvt.dt.upper_temp[4],
		battemp_me.hvt.dt.upper_temp[5],
		battemp_me.hvt.dt.upper_temp[6],
		battemp_me.hvt.dt.upper_temp[7],
		battemp_me.hvt.dt.upper_temp[8],
		battemp_me.hvt.dt.upper_temp[9],
		battemp_me.hvt.dt.upper_temp[10],
		battemp_me.hvt.dt.upper_temp[11],
		battemp_me.hvt.dt.lower_temp,
		battemp_me.hvt.dt.trigger_soc[0],
		battemp_me.hvt.dt.trigger_soc[1],
		battemp_me.hvt.dt.trigger_soc[2],
		battemp_me.hvt.dt.trigger_soc[3],
		battemp_me.hvt.dt.trigger_soc[4],
		battemp_me.hvt.dt.trigger_soc[5],
		battemp_me.hvt.dt.trigger_soc[6],
		battemp_me.hvt.dt.trigger_soc[7],
		battemp_me.hvt.dt.trigger_soc[8],
		battemp_me.hvt.dt.trigger_soc[9],
		battemp_me.hvt.dt.trigger_soc[10],
		battemp_me.hvt.dt.trigger_soc[11]
	);

	return !rc;
}

static bool battemp_create_voters(void)
{
	return veneer_voter_register(&battemp_me.voter_ichilly, VOTER_NAME_CHILLY, VOTER_TYPE_IBAT, false)
		&& veneer_voter_register(&battemp_me.voter_icharge, VOTER_NAME_ICHARGE, VOTER_TYPE_IBAT, false)
		&& veneer_voter_register(&battemp_me.voter_vfloat_ibat, VOTER_NAME_VFLOAT_IBAT, VOTER_TYPE_IBAT, false)
		&& veneer_voter_register(&battemp_me.voter_vfloat, VOTER_NAME_VFLOAT, VOTER_TYPE_VFLOAT, true)
		&& veneer_voter_register(&battemp_me.voter_iusb, VOTER_NAME_IUSB, VOTER_TYPE_IUSB, false);
}

static bool battemp_create_preset(void)
{
	battemp_me.battemp_wakelock = wakeup_source_register(NULL, BATTEMP_WAKELOCK);

	INIT_DELAYED_WORK(&battemp_me.battemp_dwork,
		polling_status_work);
	INIT_DELAYED_WORK(&battemp_me.hvt.dwork,
		hvt_timer_work);
#ifdef DEBUG_BTP
	INIT_DELAYED_WORK(&battemp_me.debug_btp_dwork,
		debug_btp_polling_status_work);
#endif

	return true;
}

void protection_battemp_monitor(void)
{
	if (!battemp_me.enable)
		return;

	if (delayed_work_pending(&battemp_me.battemp_dwork))
		cancel_delayed_work(&battemp_me.battemp_dwork);
	schedule_delayed_work(&battemp_me.battemp_dwork, msecs_to_jiffies(0));
}

#ifdef DEBUG_BTP
void debug_btp_monitor(void)
{
	if (delayed_work_pending(&battemp_me.debug_btp_dwork))
		cancel_delayed_work(&battemp_me.debug_btp_dwork);
	schedule_delayed_work(&battemp_me.debug_btp_dwork, msecs_to_jiffies(0));
}
#endif

bool protection_battemp_create(struct device_node* dnode)
{
	pr_debugmask = ERROR | UPDATE;
	if (!(dnode && of_device_is_available(dnode))) {
		pr_battemp(ERROR, "protection_battemp disabled.\n");
		return true;
	}
	battemp_me.enable = true;

	if (!battemp_create_preset()) {
		pr_battemp(ERROR, "error on battemp_create_preset");
		goto destroy;
	}

	if (!battemp_create_parsedt(dnode)) {
		pr_battemp(ERROR, "error on battemp_create_devicetree");
		goto destroy;
	}

	if (!battemp_create_voters()) {
		pr_battemp(ERROR, "error on battemp_create_voters");
		goto destroy;
	}

	protection_battemp_monitor();
#ifdef DEBUG_BTP
	debug_btp_monitor();
#endif
	pr_battemp(UPDATE, "Complete to create\n");
	return true;
destroy:
	protection_battemp_destroy();
	return false;
}

void protection_battemp_destroy(void)
{
	wakeup_source_unregister(battemp_me.battemp_wakelock);
	cancel_delayed_work_sync(&battemp_me.battemp_dwork);
	cancel_delayed_work_sync(&battemp_me.hvt.dwork);
#ifdef DEBUG_BTP
	cancel_delayed_work_sync(&battemp_me.debug_btp_dwork);
#endif

	veneer_voter_unregister(&battemp_me.voter_ichilly);
	veneer_voter_unregister(&battemp_me.voter_icharge);
	veneer_voter_unregister(&battemp_me.voter_vfloat);
	veneer_voter_unregister(&battemp_me.voter_vfloat_ibat);
	veneer_voter_unregister(&battemp_me.voter_iusb);

	battemp_me.health_chilly = false;
	battemp_me.health_jeita = POWER_SUPPLY_HEALTH_UNKNOWN;

	battemp_me.threshold_degc_upto_cool   = BATTEMP_NOTREADY;
	battemp_me.threshold_degc_upto_good   = BATTEMP_NOTREADY;
	battemp_me.threshold_degc_upto_warm   = BATTEMP_NOTREADY;
	battemp_me.threshold_degc_upto_hot    = BATTEMP_NOTREADY;
	battemp_me.threshold_degc_downto_warm = BATTEMP_NOTREADY;
	battemp_me.threshold_degc_downto_good = BATTEMP_NOTREADY;
	battemp_me.threshold_degc_downto_cool = BATTEMP_NOTREADY;
	battemp_me.threshold_degc_downto_cold = BATTEMP_NOTREADY;

	battemp_me.period_ms_emergency = BATTEMP_NOTREADY;
	battemp_me.period_ms_warning   = BATTEMP_NOTREADY;
	battemp_me.period_ms_normal    = BATTEMP_NOTREADY;

	battemp_me.cool_mv_alert  = BATTEMP_NOTREADY,
	battemp_me.cool_ma_alert  = BATTEMP_NOTREADY,
	battemp_me.cool_ma_normal = BATTEMP_NOTREADY,
	battemp_me.warm_ma_charge = BATTEMP_NOTREADY;
	battemp_me.warm_mv_float  = BATTEMP_NOTREADY;

	battemp_me.chilly_is_supported    = false;
	battemp_me.chilly_degc_upperbound = BATTEMP_NOTREADY;
	battemp_me.chilly_degc_lowerbound = BATTEMP_NOTREADY;
	battemp_me.chilly_mv_hyst	  = BATTEMP_NOTREADY;
	battemp_me.chilly_mv_bound	  = BATTEMP_NOTREADY;
	battemp_me.chilly_mv_alert	  = BATTEMP_NOTREADY;
	battemp_me.chilly_ma_alert	  = BATTEMP_NOTREADY;
	battemp_me.chilly_ma_normal       = BATTEMP_NOTREADY;
}
