/*
 * CAUTION! :
 * 	This file will be included at the end of "qpnp-smb5.c".
 * 	So "qpnp-smb5.c" should be touched before you start to build.
 * 	If not, your work will not be applied to the built image
 * 	because the build system may not care the update time of this file.
 */

#define pr_fmt(fmt)	"BATTERY_CHG: %s: " fmt, __func__

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/iio/consumer.h>
#include <linux/freezer.h>
#include <linux/thermal.h>
#include <linux/rtc.h>
#include <linux/spmi.h>
#include "veneer-primitives.h"

#define DEFAULT_VOTER "DEFAULT_VOTER"
#define USER_VOTER "USER_VOTER"

int get_rtc_time(unsigned long *rtc_time)
{
	struct rtc_time tm = {0, };
	struct rtc_device *rtc = NULL;
	int rc = 0;

	rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);
	if (rtc == NULL) {
		pr_err("Failed to open rtc device (%s)\n",
				CONFIG_RTC_HCTOSYS_DEVICE);
		return -EINVAL;
	}

	rc = rtc_read_time(rtc, &tm);
	if (rc) {
		pr_err("Failed to read rtc time (%s) : %d\n",
				CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}

	rc = rtc_valid_tm(&tm);
	if (rc) {
		pr_err("Invalid RTC time (%s): %d\n",
				CONFIG_RTC_HCTOSYS_DEVICE, rc);
		goto close_time;
	}
	rtc_tm_to_time(&tm, rtc_time);

close_time:
	rtc_class_close(rtc);
	return rc;
}

#define _BASE(_name, _bus_id, _sid, _base) \
[REGDUMP_##_name] = { .name = #_name, .bus_id = _bus_id, .sid = _sid, .base = _base, }
#define _PMI(_name, _base) _BASE(_name, 0, 0x3, _base)
#define _SMB(_name, _base) _BASE(_name, 1, 0xb, _base)
#define SPMI_GLINK_MAX_READ_BYTES	256
static const struct base {
	const char* name;
	u32 bus_id;
	u8 sid;
	u16 base;
} bases [] = {
	_BASE(POLL, -1, -1, -1),
	_PMI(CHGR,   0x2600),
	_PMI(DCDC,   0x2700),
	_PMI(BATIF,  0x2800),
	_PMI(USB,    0x2900),
	_PMI(WLS,    0x2A00),
	_PMI(TYPEC,  0x2B00),
	_PMI(MISC,   0x2C00),
	_PMI(PD,     0x2D00),
	_SMB(PERPH0, 0x2600),
	_SMB(PERPH1, 0x2700),
	_SMB(PERPH2, 0x2800),
};

static void debug_dump(struct battery_chg_dev *bcdev, int index)
{
	const struct base *dump_base = &bases[index];
	u8 buf[SPMI_GLINK_MAX_READ_BYTES];
	u16 reg;
	u8 *val;
	int rc;

	rc = spmi_glink_get_regdump(dump_base->bus_id, dump_base->sid, dump_base->base, buf);
	if (rc) {
		pr_err("REGDUMP: check error rc = %d\n",rc);
		return;
	}

	for (reg = 0; reg < 0x100; reg += 0x10) {
		val = &buf[reg];
		pr_err("[%s] 0x%X - %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
			dump_base->name, dump_base->base + reg, val[0], val[1], val[2], val[3], val[4], val[5], val[6], val[7], val[8],
			val[9], val[10], val[11], val[12], val[13], val[14], val[15]);
	}
}


#define MAX_ICM_STATE		11
#define MAX_ICM_SUB_STATE	6
static void debug_polling(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	static const char * const usb_type_text[] = {
		"None", "Unknown", "USB", "USB_OCP", "USB_CDP", "USB_DCP",
		"USB_FLOAT", "USBC_56K", "USBC_22K", "USBC_10K",
		"DEBUG_ACCESS", "USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP_3P5",
		"USB_PD", "USB_PPS", "POWERCABLE", "UNO_DEBUG_ACCESS", "AUDIO_ACCESS",
		"WLS_SRC_BPP", "WLS_SNK_BPP", "WLS_SNK_EPP", "WLS_SNK_PDDE", "INVALID",
	};

	static const char * const status_text[] = {
		"Unknown", "Charging", "Discharging", "Not charging", "Full"
	};

	static const char * const charge_type_text[] = {
		"Unknown", "N/A", "Trickle", "Fast", "Standard", "Adaptive", "Custom"
	};

	static const char * const bsm_text[] = {
		"ENTRY", "NO_CHG", "FAST", "TOP_OFF", "DONE", "RECHARGE", "TDONE",
		"NOT_CHARGING_THERMAL", "ERROR", "TEST", "MAX", "INVALID", "NA"
	};

	static const char * const icm_text[MAX_ICM_STATE][MAX_ICM_SUB_STATE] = {
		{"INITIAL"},
		{"PRE_CHG"},
		{"NCP_STBY"},
		{"SCP_SVS"},
		{"SCP_INOV_NORMAL", "SCP_INOV_THERMAL", "SCP_INOV_STATE_MAX"},
		{"SCP_CC_STARTUP", "SCP_CC_TAPER", "SCP_CC_THERMAL", "SCP_CC_BALANCED", "SCP_CC_STATE_MAX"},
		{"MCP_CV_STARTUP", "MCP_CV_IRQ", "MCP_CV_TAPER", "MCP_CV_THERMAL", "MCP_CV_BALANCED", "MCP_CV_STATE_MAX"},
		{"MCP_CC_STARTUP", "MCP_CC_TAPER", "MCP_CC_THERMAL", "MCP_CC_IRQ", "MCP_CC_BALANCED", "MCP_CC_STATE_MAX"},
		{"MCP_CC_2S_STARTUP", "MCP_CC_2S_TAPER", "MCP_CC_2S_THERMAL", "MCP_CC_2S_IRQ", "MCP_CC_2S_BALANCED", "MCP_CC_2S_STATE_MAX"},
		{"MCP_PT"},
		{"STATE_MAX"}
	};

	struct oem_logging_type	*log;
	struct thermal_zone_device* tzd = thermal_zone_get_zone_by_name("vts-virt-therm");
	char  chg_name [16] = { 0, };
	int rc, vwls_reg = 0, vidt = 0, iidt = 0, vts_temp = -1000;
	char *iwls_name = "";
	char *vwls_name = "";

	if (!ext_bcdev) {
		pr_info("PMINFO: Skip logging by ext_bcdev\n");
		return;
	}

	rc = read_oem_logging(bcdev);
	if (rc) {
		pr_info("PMINFO: Skip logging by glink\n");
		return;
	}
	log = &ext_bcdev->logging;

	if (ext_bcdev->dc_psy) {
		iwls_name = (char *)get_effective_client(ext_bcdev->dc_icl_votable);
		vwls_name = (char *)get_effective_client(ext_bcdev->vwls_votable);
		get_veneer_param(VENEER_FEED_DC_VOLTAGE_NOW, &vidt);
		get_veneer_param(VENEER_FEED_DC_CURRENT_NOW, &iidt);
		get_veneer_param(VENEER_FEED_DC_VOLTAGE_MAX_DESIGN, &vwls_reg);
	}

	if (tzd) {
		vts_temp = !thermal_zone_get_temp(tzd, &vts_temp) ? vts_temp / 100 : -1000;
	}

	printk("PMINFO: [CSV] "
	/* Capacity     */ "cUI:%d, cMNT:%d, cSYS:%d, cBAT:%d, cCHG:%d, "
	/* v/i ADCs     */ "iBAT:%d, vBAT:%d, vOCV:%d, vUSB:%d, iUSB:%d, AiCL:%d, iSMB:%d, fsmb:%d, "
	/* Temp         */ "tSYS:%d, tORI:%d, tVTS:%d, "
	/* Battery sts  */ "rESR:%d, cLRN:%d, CYCLE:%d, cRCH:%d, "
	/* Wireless     */ "vWLS:%d, iWLS:%d, vIDT:%d, iIDT:%d\n",
		ext_bcdev->rescale.result , log->soc_mono, log->soc_sys, log->soc_bat, log->charge_counter,
		(int) log->ibat, log->vbat, log->ocv_mv, log->vbus, log->iusb, log->aicl, log->ismb, log->fsmb,
		calculate_battery_temperature(bcdev), (int) log->bat_temp, vts_temp,
		log->esr, log->learned_capacity, log->cycle_count, log->recharge_soc,
		log->vwls, log->iwls, vidt, iidt);

	printk("PMINFO: [VOT] IUSB:%d(%s)-%d(%s)-%d(reg), FCC:%d(%s)-%d(%s)-%d(reg), FLOAT:%d(%s)-%d(%s)-%d(reg), "
			"IWLS:%d(%s)-%d(%s)-%d(reg), VWLS:%d(%s)-%d(reg)\n",
		get_effective_result(ext_bcdev->usb_icl_votable), get_effective_client(ext_bcdev->usb_icl_votable),
		log->vote_icl, log->icl_name, log->reg_icl,
		get_effective_result(ext_bcdev->fcc_votable), get_effective_client(ext_bcdev->fcc_votable),
		log->vote_fcc, log->fcc_name, log->reg_fcc,
		get_effective_result(ext_bcdev->fv_votable), get_effective_client(ext_bcdev->fv_votable),
		log->vote_fv, log->fv_name, log->reg_fv,
		get_effective_result(ext_bcdev->dc_icl_votable), iwls_name,
		log->vote_icl, log->icl_name, log->reg_wls,
		get_effective_result(ext_bcdev->vwls_votable), vwls_name, vwls_reg);

	unified_nodes_show("charger_name", chg_name);
	printk("PMINFO: [CHG] NAME:%s(%s), STAT:%s(ori)/%s(chg), PATH:%d, BSM:%s, ICM:%s\n",
		chg_name, usb_type_text[log->charger_type],
		status_text[bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_STATUS]],
		charge_type_text[bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_CHG_TYPE]],
		log->pwr_path, bsm_text[log->bsm_state], icm_text[log->icm_state][log->icm_sub_state]);

	printk("PMINFO: [SMB] ICL:%d ICHG:%d VOUT:%d perph0_int:0x%02x, perph1_int:0x%02x,perph0_mode:0x%02x\n",
		log->smb_icl,log->smb_ichg,log->smb_vout, log->smb_perph0_int, log->smb_perph1_int, log->smb_perph0_mode);

	printk("PMINFO: ---------------------------------------------"
			"-----------------------------------------%s-END.\n",
			unified_bootmode_marker());
	return;
}


static void debug_battery(struct battery_chg_dev *bcdev, int func)
{
	int i;

	if (func < 0) {
		for (i = 1; i < REGDUMP_MAX; ++i)
			debug_dump(bcdev, i);
	}
	else if (func == 0)
		debug_polling(bcdev);
	else if (func < REGDUMP_MAX)
		debug_dump(bcdev, func);
}

//static void support_parallel_checking(struct smb_charger *chg, int disable)

static int get_oem_property(struct battery_chg_dev *bcdev, enum oem_property_id oem_prop, int* val)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int rc = 0;

	if (!ext_bcdev || oem_prop >= OEM_PROP_MAX)
		return -1;

	rc = read_oem_property(bcdev, oem_prop);
	if (!rc)
		*val = ext_bcdev->prop_data[oem_prop];

	return rc;
}

static int get_force_update(struct battery_chg_dev *bcdev, int *val)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	*val = -1;

	if (!ext_bcdev)
		return 0;

	*val = ext_bcdev->force_update;
	return 0;
}

static int set_force_update(struct battery_chg_dev *bcdev, bool toggle)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int rc = 0;

	if (!ext_bcdev)
		return rc;

	if(toggle)
		ext_bcdev->force_update = !ext_bcdev->force_update;

	if(bcdev->psy_list[PSY_TYPE_BATTERY].psy) {
		power_supply_changed(bcdev->psy_list[PSY_TYPE_BATTERY].psy);
		rc = 1;
	}
	return rc;
}


/* Extension A. Battery temp tuning */
/* calculate_battery_temperature
 *     bias  : 1st compensation by predefined diffs
 *     icomp : 2nd compensation by (i^2 * k)
 */

static int get_charging_type(struct battery_chg_dev *bcdev)
{
	union power_supply_propval val = { 0, };
	int lcd_status = true;

	if (bcdev->psy_list[PSY_TYPE_BATTERY].psy) {
		if (!power_supply_get_property(bcdev->psy_list[PSY_TYPE_BATTERY].psy,
			POWER_SUPPLY_PROP_STATUS, &val))
			if (val.intval != POWER_SUPPLY_STATUS_CHARGING)
				return TCOMP_CHG_NONE;
	}

	if (bcdev->psy_list[PSY_TYPE_USB].psy) {
		if (!power_supply_get_property(bcdev->psy_list[PSY_TYPE_USB].psy,
			POWER_SUPPLY_PROP_PRESENT, &val))
			if (val.intval)
				return TCOMP_CHG_USB;
	}

	if (bcdev->psy_list[PSY_TYPE_WLS].psy) {
		if (!power_supply_get_property(bcdev->psy_list[PSY_TYPE_WLS].psy,
			POWER_SUPPLY_PROP_PRESENT, &val)) {
			if (val.intval) {
				if (unified_nodes_read(UNIFIED_NODE_STATUS_LCD, &lcd_status)) {
					if (!lcd_status)
						return TCOMP_CHG_WLC_LCDOFF;
					return TCOMP_CHG_WLC_LCDON;
				} else
					return TCOMP_CHG_WLC_LCDOFF;
			}
		}
	}

	return TCOMP_CHG_NONE;
}

#define TEMP_CATCHUP_SEC_MAX		150
#define TEMP_CATCHUP_SEC_PER_DEGREE	30
#define MAX_CATCHUP_TEMP (TEMP_CATCHUP_SEC_MAX/TEMP_CATCHUP_SEC_PER_DEGREE)
#define TEMP_UPDATE_TIME_SEC		5
#define DIVIDE_TEMP			10
#define DIVIDE_MS_TO_S			1000

static int catchup_batt_therm(int temp, int temp_backup, int ts_diff)
{
	int catchup_time_s = 0, catchup_temp = 0, change_therm = 0;

	change_therm = abs(temp - temp_backup);
	if ((change_therm / DIVIDE_TEMP) < MAX_CATCHUP_TEMP)
		catchup_time_s = change_therm * TEMP_CATCHUP_SEC_PER_DEGREE / DIVIDE_TEMP;
	else
		catchup_time_s = TEMP_CATCHUP_SEC_MAX;

	if (catchup_time_s <= 0)
		catchup_time_s = 1;

	ts_diff++;
	/* Scaling temp in case of time_diff < catchup_time */
	if (ts_diff <= catchup_time_s) {
		catchup_temp = ((catchup_time_s - ts_diff)*temp_backup + (ts_diff * temp))/catchup_time_s;

		pr_info("catchup_temp ts_diff:%d, catchup_time_s:%d, "
			"temp:%d, therm_backup:%d, result_temp:%d\n",
			ts_diff, catchup_time_s,
			temp, temp_backup, catchup_temp);
		return catchup_temp;
	}
	/* Out of target time, doesn't scaling */
	else {
		pr_debug("catchup_skip ts_diff:%d, catchup_time_s:%d, "
			"temp:%d, therm_backup:%d, result_temp:%d\n",
			ts_diff, catchup_time_s,
			temp, temp_backup, temp);
		return temp;
	}
}

#define DEFAULT_BATT_TEMP  200
#define BATTERY_TEMP_COMPENSATION_THRESHOLD_CURRENT 1000000 //unit: uA, 1000 mA
static int calculate_battery_temperature(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int battemp_bias, battemp_icomp = 0, battemp_cell = 0;
	int i, temp, ichg_comp = 0, tbl_pt = 0;
	union power_supply_propval val = { 0, };
	bool tbl_changed = false;
	static int pre_tbl_pt = -1;
	int ts_diff = 0;
	ktime_t now;
	struct smooth_param *smooth_therm;
	struct tcomp_param *tcomp;

	if (!bcdev->psy_list[PSY_TYPE_BATTERY].psy ||
			battery_psy_get_prop(bcdev->psy_list[PSY_TYPE_BATTERY].psy, POWER_SUPPLY_PROP_TEMP, &val)){
		pr_info("get real batt therm error\n");
		return DEFAULT_BATT_TEMP;
	}

	battemp_cell = val.intval;
	if (!ext_bcdev)
		return battemp_cell;

	smooth_therm = &ext_bcdev->smooth_therm;
	tcomp = &ext_bcdev->tcomp;
	if (smooth_therm->smooth_filter_enable) {
		/* Check now ktime */
		now = ktime_get();

		if ((smooth_therm->last_time == 0 && smooth_therm->therm_backup == 0)
					|| !smooth_therm->batt_therm_ready) {
			smooth_therm->last_time = now;
			pr_info("Initialize time and temp, ts=%lld, backup=%d, ready=%d\n",
					smooth_therm->last_time, smooth_therm->therm_backup,
					smooth_therm->batt_therm_ready);
			smooth_therm->enable_flag = false;
			goto skip_time;
		}

		ts_diff = ((unsigned long)ktime_ms_delta(now, smooth_therm->last_time)/DIVIDE_MS_TO_S);

		/* Update batt_therm every 5sec */
		if (ts_diff < TEMP_UPDATE_TIME_SEC) {
			pr_debug("ts_diff(%d) is lower than UPDATE_SEC. now(%lld), ts_bef(%lld)\n",
					ts_diff, now, smooth_therm->last_time);
			goto out_temp;
		}

		if (!smooth_therm->enable_flag)
			smooth_therm->enable_flag = true;
	}
skip_time:
	if (!tcomp->load_done) {
		pr_info("not ready tcomp table. rerun -> table=%d\n",
			tcomp->load_done);
		return battemp_cell;
	}

	if (!tcomp->icoeff_load_done) {
		pr_info("not ready icoeff. rerun -> icoeff=%d\n",
			tcomp->icoeff_load_done);
		return battemp_cell;
	}

	if (!smooth_therm->batt_therm_ready)
		smooth_therm->batt_therm_ready = true;

	if (tcomp->load_max > 1) {
		switch (get_charging_type(bcdev)) {
			case TCOMP_CHG_WLC_LCDOFF: tbl_pt = 1; break;
			case TCOMP_CHG_WLC_LCDON:  tbl_pt = 2; break;
			default: tbl_pt = 0; break;
		}
		if (pre_tbl_pt >= 0 )
			if (pre_tbl_pt != tbl_pt)
				tbl_changed = true;
		pre_tbl_pt = tbl_pt;
	}
	else
		tbl_pt = 0;


	/* Compensating battemp_bias */
	for (i = 0; i < TCOMP_COUNT; i++) {
		if (battemp_cell < tcomp->table[tbl_pt][i].temp_cell)
			break;
	}

	if (i == 0)
		battemp_bias = tcomp->table[tbl_pt][0].temp_bias;
	else if (i == TCOMP_COUNT)
		battemp_bias = tcomp->table[tbl_pt][TCOMP_COUNT-1].temp_bias;
	else
		battemp_bias =
		(	(tcomp->table[tbl_pt][i].temp_bias -
				tcomp->table[tbl_pt][i-1].temp_bias)
			* (battemp_cell - tcomp->table[tbl_pt][i-1].temp_cell)
			/ (tcomp->table[tbl_pt][i].temp_cell -
				tcomp->table[tbl_pt][i-1].temp_cell)
		) + tcomp->table[tbl_pt][i-1].temp_bias;

	/* Compensating battemp_icomp */
	if (bcdev->psy_list[PSY_TYPE_BATTERY].psy) {
		if (!power_supply_get_property(
			bcdev->psy_list[PSY_TYPE_BATTERY].psy, POWER_SUPPLY_PROP_STATUS, &val)
			&& val.intval == POWER_SUPPLY_STATUS_CHARGING
			&& !power_supply_get_property(
				bcdev->psy_list[PSY_TYPE_BATTERY].psy, POWER_SUPPLY_PROP_CURRENT_NOW, &val)
			&& val.intval > BATTERY_TEMP_COMPENSATION_THRESHOLD_CURRENT)
			ichg_comp = ((val.intval) / 1000);
	} else {
		pr_info("Battery is not available, %d(=%d+%d) as batt temp\n",
			battemp_cell + battemp_bias, battemp_cell, battemp_bias);
	}

	battemp_icomp = ichg_comp * ichg_comp * tcomp->icoeff;
	battemp_icomp = battemp_icomp / 10000000;
	temp = battemp_cell + battemp_bias - battemp_icomp;

	if (tcomp->logging)
		pr_info("Battery temperature : "
				"%d = ( %d )(cell) + ( %d )(bias) - %d (icomp), "
				"icoeff = %d, ichg_comp = %d \n",
			temp, battemp_cell, battemp_bias, battemp_icomp,
			tcomp->icoeff, ichg_comp);

	if (smooth_therm->smooth_filter_enable) {
		if (smooth_therm->enable_flag) {
			smooth_therm->therm_backup = catchup_batt_therm(temp, smooth_therm->therm_backup, ts_diff);
			smooth_therm->last_time = now;
		}
		/* In some cases, Using default batt_therm */
		else {
			smooth_therm->therm_backup = temp;
		}
	}
	else {
		return temp;
	}

out_temp:
	return smooth_therm->therm_backup;
}

static int extension_get_battery_temp(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int temp = 0;

	if (ext_bcdev && ext_bcdev->fake.temperature > -9999)
		temp = ext_bcdev->fake.temperature;
	else
		temp = calculate_battery_temperature(bcdev); // Use compensated temperature

	return temp;
}

//#define HVT_DEBUG
#define DEFAULT_BATTERY_SOC     51
#define FULL_CAPACITY           100
#define FULL_CAPACITY_X100      10000
#define MAX_BACKUP_INTERVAL		(1 * 60 * 60)   /* 1 hour */

static void set_veneer_backup_hvt_count(int value)
{
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_ID, VENEER_BACKUP_ID_HVT_COUNT);
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_DATA, value);
}

static void get_veneer_backup_hvt_count(int *value)
{
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_ID, VENEER_BACKUP_ID_HVT_COUNT);
	get_veneer_param(
		VENEER_FEED_VENEER_BACKUP_DATA, value);
}

static void set_veneer_backup_hvt_ui_soc(int value)
{
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_ID, VENEER_BACKUP_ID_HVT_UI_SOC);
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_DATA, value);
}

int set_veneer_backup_cbc_cycle(int value)
{
	int rc = 0;
	pr_info("set_veneer_backup_cbc_cycle %d \n", value);
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_ID, VENEER_BACKUP_ID_CBC_CYCLE);
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_DATA, value);
	rc = set_veneer_param(VENEER_FEED_VENEER_BACKUP_FLUSH, 0);
	return rc;
}

void get_veneer_backup_cbc_cycle(int *value)
{
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_ID, VENEER_BACKUP_ID_CBC_CYCLE);
	get_veneer_param(
		VENEER_FEED_VENEER_BACKUP_DATA, value);
	pr_info("get_veneer_backup_cbc_cycle %d \n", *value);
}

static void get_veneer_backup_hvt_ui_soc(int *value)
{
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_ID, VENEER_BACKUP_ID_HVT_UI_SOC);
	get_veneer_param(
		VENEER_FEED_VENEER_BACKUP_DATA, value);
}

static void get_veneer_backup_hvt_backup_time(int *value)
{
	set_veneer_param(
		VENEER_FEED_VENEER_BACKUP_ID, VENEER_BACKUP_ID_HVT_BACKUP_TIME);
	get_veneer_param(
		VENEER_FEED_VENEER_BACKUP_DATA, value);
}

static int get_hvt_count_from_veneer_backup(struct ext_battery_chg *ext_bcdev)
{
	int rc = 0, backup_time_temp = 0, result_temp = 0;
	unsigned long backup_time = 0, now = 0;

	if (!ext_bcdev) {
		pr_info("[HVT] error: ext_bcdev is null\n");
		return -1;
	}

	if (ext_bcdev->hvt_rescale.is_load_backup)
		return 0;

	rc = set_veneer_param(VENEER_FEED_VENEER_BACKUP_LOAD, 0);
	if (rc == -1) { /* error: no reponse */
		ext_bcdev->hvt_rescale.count = 0;
		pr_info("[HVT] error: veneer backup have no reponse error.\n");
		return -1;
	}
	else if (rc == -2) { /* error: it isn't good cookie */
		ext_bcdev->hvt_rescale.count = 0;
		pr_info("[HVT] veneer backup is not set... no good cookie.\n");
		goto exit_get_hvt_count_from_veneer_backup;
	}

	/* restore hvt params from veneer backup */
	get_veneer_backup_hvt_count(&ext_bcdev->hvt_rescale.count);
	get_veneer_backup_hvt_ui_soc(&result_temp);
	get_rtc_time(&now);
	get_veneer_backup_hvt_backup_time(&backup_time_temp);
	backup_time = (unsigned long) backup_time_temp;

	if ((ext_bcdev->hvt_rescale.count > 0) &&
		((result_temp > 50) && (result_temp <= 100)) &&
		((now - backup_time) < MAX_BACKUP_INTERVAL) && ((now - backup_time) > 0)) {
		ext_bcdev->hvt_rescale.en = true;
		ext_bcdev->hvt_rescale.state = HVT_RESCALE_STATE_ENABLED;
		ext_bcdev->hvt_rescale.hvt_scale =
			ext_bcdev->hvt_rescale.dt_scale[ext_bcdev->hvt_rescale.count - 1];
		ext_bcdev->rescale.result = result_temp;
		set_veneer_param(
			VENEER_FEED_HVT_SOC_RESCALE_COUNT, ext_bcdev->hvt_rescale.count);
		pr_info("[HVT] hvt rescale state is ENABLED.\n");
	}

exit_get_hvt_count_from_veneer_backup:

	ext_bcdev->hvt_rescale.is_load_backup = true;
	pr_info(
		"[HVT] veneer backup load is done...count=%d, soc=%d, "
		"diff_time=%dsec(now=%dsec, backup=%dsec)\n",
		ext_bcdev->hvt_rescale.count, ext_bcdev->rescale.result, (now - backup_time),  now, backup_time);

	return 0;
}

static int rescale_x100(struct ext_battery_chg *ext_bcdev, int raw_soc, int rescale_factor)
{
	ext_bcdev->hvt_rescale.scale_now = rescale_factor;
	return DIV_ROUND_CLOSEST(raw_soc * FULL_CAPACITY_X100, rescale_factor);
}

static int rescale_ui(struct ext_battery_chg *ext_bcdev, int ui_raw)
{
	int result = DIV_ROUND_CLOSEST(min(ui_raw, FULL_CAPACITY_X100), FULL_CAPACITY);
	if (ext_bcdev->hvt_rescale.scale_now == ext_bcdev->rescale.criteria)
		ext_bcdev->hvt_rescale.ui_raw = result;
	return result;
}

static int rescale_ui_255(struct ext_battery_chg *ext_bcdev, int ui_raw)
{
	return DIV_ROUND_CLOSEST(min(ui_raw, FULL_CAPACITY_X100) * 255,	FULL_CAPACITY_X100);
}

#if defined(HVT_DEBUG)
static void lge_rescale_log(struct ext_battery_chg *ext_bcdev)
{
	/* Debug Log
	   csv header:
	   ,en,ui_raw,ui,msoc,error,hvt_scale,scale_now,count,state
	*/
	pr_info("[HVT SOC],%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		ext_bcdev->hvt_rescale.en,
		ext_bcdev->hvt_rescale.ui_raw,
		ext_bcdev->rescale.result,
		ext_bcdev->rescale.rawsoc,
		ext_bcdev->hvt_rescale.soc_error,
		ext_bcdev->hvt_rescale.hvt_scale,
		ext_bcdev->hvt_rescale.scale_now,
		ext_bcdev->hvt_rescale.count,
		ext_bcdev->hvt_rescale.state
	);
}
#endif

static void hvt_soc_release(struct ext_battery_chg *ext_bcdev)
{
	ext_bcdev->hvt_rescale.en = false;
	ext_bcdev->hvt_rescale.state = HVT_RESCALE_STATE_NONE;
	ext_bcdev->hvt_rescale.count = 0;
	ext_bcdev->hvt_rescale.ui_raw = 0;
	ext_bcdev->hvt_rescale.hvt_scale = 0;
	ext_bcdev->hvt_rescale.scale_now = 0;
	ext_bcdev->hvt_rescale.soc_error = 0;
	ext_bcdev->hvt_rescale.last_time = 0;
	if (set_veneer_param(VENEER_FEED_HVT_SOC_RESCALE_COUNT, 0))
		pr_info("[HVT] set_veneer_param(hvt_soc_rescale_count) failure, error\n");
}

static void hvt_soc_rescale_process(struct ext_battery_chg *ext_bcdev, int status_raw)
{
	int ui_soc_raw = 0, ui_soc_new = 0, fast_dischg_offset = 0;
	int temp_result = 0, result_comp = 0;

	/* get original lge rescale soc */
	ext_bcdev->rescale.rawsoc = ext_bcdev->qbg_info.soc_mono;
	ui_soc_raw = rescale_x100(ext_bcdev,
			ext_bcdev->rescale.rawsoc, ext_bcdev->rescale.criteria);
	ext_bcdev->hvt_rescale.ui_raw = rescale_ui(ext_bcdev, ui_soc_raw);

	/* set to fast discharging soc for hvt */
	if ((status_raw != POWER_SUPPLY_STATUS_CHARGING &&
			ext_bcdev->rescale.result < FULL_CAPACITY) ||
		(status_raw != POWER_SUPPLY_STATUS_CHARGING &&
			ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_EN_DISCHARGE)) {

		/* find fast discharge offset */
		fast_dischg_offset =
			FULL_CAPACITY_X100 -
			(FULL_CAPACITY_X100 *
				ext_bcdev->hvt_rescale.hvt_scale /
				ext_bcdev->hvt_rescale.dt_fast_scale);

		/* get fast discharge rescaled soc */
		ui_soc_new = rescale_x100(ext_bcdev,
			ext_bcdev->rescale.rawsoc, ext_bcdev->hvt_rescale.dt_fast_scale);

		/* rescale compensated raw soc */
		result_comp = ui_soc_new + fast_dischg_offset;
		temp_result = rescale_ui(ext_bcdev, result_comp);

		/* find error of rescaled result */
		if (ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_ENABLED
			|| ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_EN_CHARGE) {
			ext_bcdev->hvt_rescale.state = HVT_RESCALE_STATE_EN_DISCHARGE;
			ext_bcdev->hvt_rescale.soc_error = ext_bcdev->rescale.result - temp_result;
			pr_info("[HVT] hvt rescale state is EN_DISCHARGE..."
					"fast_dischg_offset=%d, rescale_factor=%d, error=%d, old=%d, new=%d\n",
					fast_dischg_offset, ext_bcdev->hvt_rescale.dt_fast_scale,
					ext_bcdev->hvt_rescale.soc_error,
					ext_bcdev->rescale.result, temp_result);
		}

		ext_bcdev->rescale.result = temp_result;
		ext_bcdev->rescale.result += ext_bcdev->hvt_rescale.soc_error;
		ext_bcdev->rescale.result = min(ext_bcdev->rescale.result, FULL_CAPACITY);
		result_comp += (ext_bcdev->hvt_rescale.soc_error * FULL_CAPACITY);
		ext_bcdev->rescale.result_255 = rescale_ui_255(ext_bcdev, result_comp);

	} else if ((status_raw == POWER_SUPPLY_STATUS_CHARGING &&
				 ext_bcdev->rescale.result < FULL_CAPACITY ) ||
			   (status_raw == POWER_SUPPLY_STATUS_CHARGING &&
				 ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_EN_CHARGE)) {
		/* find rescaled error */
		if (ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_EN_DISCHARGE) {
			ext_bcdev->hvt_rescale.state = HVT_RESCALE_STATE_EN_CHARGE;
			ext_bcdev->hvt_rescale.soc_error =
				ext_bcdev->rescale.result - ext_bcdev->hvt_rescale.ui_raw;
			pr_info("[HVT] hvt rescale state is EN_CHARGE...error=%d, old=%d, new=%d\n",
				ext_bcdev->hvt_rescale.soc_error,
				ext_bcdev->rescale.result, ext_bcdev->hvt_rescale.ui_raw);
		}

		ext_bcdev->rescale.result =
			ext_bcdev->hvt_rescale.ui_raw + ext_bcdev->hvt_rescale.soc_error;
		ext_bcdev->rescale.result = min(ext_bcdev->rescale.result, FULL_CAPACITY);
		ui_soc_new = ui_soc_raw + (ext_bcdev->hvt_rescale.soc_error * FULL_CAPACITY);
		ext_bcdev->rescale.result_255 = rescale_ui_255(ext_bcdev, ui_soc_new);
	} else {
		ui_soc_new = rescale_x100(ext_bcdev,
			ext_bcdev->rescale.rawsoc, ext_bcdev->hvt_rescale.hvt_scale);
		ext_bcdev->rescale.result = rescale_ui(ext_bcdev, ui_soc_new);
		ext_bcdev->rescale.result_255 = rescale_ui_255(ext_bcdev, ui_soc_new);
	}
}

static int hvt_soc_process(struct ext_battery_chg *ext_bcdev)
{
	bool is_changed_hvt_status = false;
	int hvt_count = 0, status_raw = 0;
	int ui_soc_raw = 0, result_now = 0;
	int raw_soc = 0;

	if (!ext_bcdev)
		return -1;

	if (!ext_bcdev->hvt_rescale.dt_en)
		goto hvt_soc_no_process_out;

	get_hvt_count_from_veneer_backup(ext_bcdev);

	if (get_veneer_param(VENEER_FEED_STATUS_RAW, &status_raw)) {
        pr_info("[HVT] get_veneer_param(status_raw) failure, error\n");
		goto hvt_soc_no_process_out;
    }
	if (get_veneer_param(VENEER_FEED_HVT_SOC_RESCALE_COUNT, &hvt_count)) {
		hvt_soc_release(ext_bcdev);
		pr_info("[HVT] get_veneer_param(hvt_soc_rescale_count) failure, error\n");
    }

	/* if hvt is disabled in protection-battemp.c,
	   hvt_count is always zero. hvt is never triggered */
	if (ext_bcdev->hvt_rescale.count != hvt_count) {
		ext_bcdev->hvt_rescale.count = hvt_count;
		is_changed_hvt_status = true;

		set_veneer_backup_hvt_count(ext_bcdev->hvt_rescale.count);
		set_veneer_param(VENEER_FEED_VENEER_BACKUP_FLUSH, 0);

		pr_info("[HVT] soc rescale hvt_count is updated..count=%d\n", hvt_count);
	}

	/* here is for checking to enter hvt mode only. */
	if (!ext_bcdev->hvt_rescale.en && is_changed_hvt_status && hvt_count > 0) {
		ext_bcdev->hvt_rescale.en = true;
		ext_bcdev->hvt_rescale.state = HVT_RESCALE_STATE_ENABLED;
		pr_info("[HVT] hvt rescale state is ENABLED...count=%d\n", hvt_count);
	}

	if (!ext_bcdev->hvt_rescale.en)
		goto hvt_soc_no_process_out;

	/* here is for updating hvt count */
	if (ext_bcdev->hvt_rescale.en && is_changed_hvt_status && hvt_count > 0) {
		/* when hvt is enabled, ui_raw increase to hvt scale */
		ext_bcdev->hvt_rescale.hvt_scale =
			ext_bcdev->hvt_rescale.dt_scale[hvt_count - 1];

		pr_info("[HVT] soc scale factor is updated..count=%d, "
				"hvt_scale=%d, ui_soc=%d, ui_raw=%d, mono_soc=%d\n",
			hvt_count, ext_bcdev->hvt_rescale.hvt_scale, ext_bcdev->rescale.result,
			ext_bcdev->hvt_rescale.ui_raw, ext_bcdev->rescale.rawsoc);
	}

	/* check release condition of hvt mode */
	if (ext_bcdev->hvt_rescale.en) {

		hvt_soc_rescale_process(ext_bcdev, status_raw);

		raw_soc = ext_bcdev->qbg_info.soc_mono;
		if (ext_bcdev->qbg_info.soc_mono > (ext_bcdev->qbg_info.soc_sys + 100))
			raw_soc = ext_bcdev->qbg_info.soc_sys;

		// 1. change to charging state
		if ((status_raw == POWER_SUPPLY_STATUS_CHARGING)
			&& ((ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_EN_CHARGE) ||
			    (ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_ENABLED))
		    && (raw_soc >
					(ext_bcdev->hvt_rescale.dt_scale[0] +
						ext_bcdev->hvt_rescale.dt_rescale_margin + 100))
			&& (ext_bcdev->hvt_rescale.ui_raw >= 100)) {
			hvt_soc_release(ext_bcdev);
			pr_info("[HVT] rescale is disabled - charged to UI 100 percent.\n");
			goto hvt_soc_no_process_out;
		}
		// 2. if it reach to cross line of original soc, release hvt scale
		else if ((status_raw != POWER_SUPPLY_STATUS_CHARGING)
			&& (ext_bcdev->hvt_rescale.state == HVT_RESCALE_STATE_EN_DISCHARGE)
			&& (ext_bcdev->hvt_rescale.ui_raw < 100)
			&& (ext_bcdev->hvt_rescale.ui_raw >= ext_bcdev->rescale.result)) {
			hvt_soc_release(ext_bcdev);
			pr_info("[HVT] rescale is disabled - discharged to cross line.\n");
			goto hvt_soc_no_process_out;
		}

		get_veneer_backup_hvt_ui_soc(&result_now);
		set_veneer_backup_hvt_ui_soc(ext_bcdev->rescale.result);
		if (result_now != ext_bcdev->rescale.result) {
			if (set_veneer_param(VENEER_FEED_VENEER_BACKUP_FLUSH, 0)) {
				/* if write errir is occured, it restores old value to veneer backup disk. */
				set_veneer_backup_hvt_ui_soc(result_now);
			}
		}

	} else {
		goto hvt_soc_no_process_out;
	}

#if defined(HVT_DEBUG)
	lge_rescale_log(ext_bcdev);
#endif

	return 0;

hvt_soc_no_process_out:

	/* get original lge rescale soc */
	ext_bcdev->rescale.rawsoc = ext_bcdev->qbg_info.soc_mono;
	ui_soc_raw = rescale_x100(ext_bcdev,
			ext_bcdev->rescale.rawsoc, ext_bcdev->rescale.criteria);
	ext_bcdev->rescale.result = rescale_ui(ext_bcdev, ui_soc_raw);
	ext_bcdev->rescale.result_255 = rescale_ui_255(ext_bcdev, ui_soc_raw);

#if defined(HVT_DEBUG)
	lge_rescale_log(ext_bcdev);
#endif

	return 0;
}

static void calculate_battery_soc(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	union power_supply_propval val = { 0, };

	if (!bcdev || !ext_bcdev)
		return;

	if (bcdev->psy_list[PSY_TYPE_BATTERY].psy) {
		if (battery_psy_get_prop(
				bcdev->psy_list[PSY_TYPE_BATTERY].psy,
				POWER_SUPPLY_PROP_CAPACITY, &val)) {
			pr_info("Failed to get a real soc\n");
			return;
		}
	} else {
		pr_info("battery psy is null.\n");
		return;
	}

	if (!(ext_bcdev->rescale.enable)) {
		ext_bcdev->rescale.result = val.intval;
		return;
	}

	if (read_oem_qbg_info(bcdev)) {
		pr_info("PMINFO: Skip qgb_info by glink\n");
		ext_bcdev->rescale.rawsoc = DEFAULT_BATTERY_SOC * 100;
		ext_bcdev->rescale.result = DEFAULT_BATTERY_SOC;
		return;
	}

	hvt_soc_process(ext_bcdev);

	return;
}

struct update_node {
	enum veneer_feed_id veneer_id;
	int value;
};

#define _NODE(_type)	{ .veneer_id = _type, .value = -1}

struct update_node update_list[] = {
	_NODE(VENEER_FEED_CHARGER_VERBOSE),
	_NODE(VENEER_FEED_CHARGER_HIGH_SPEED),
	_NODE(VENEER_FEED_CHARGER_INCOMPATIBLE),
};

static int extension_get_update_uevent(struct battery_chg_dev *bcdev)
{
	static bool ret = false;
	int val = 0, i;
	bool update_uevent = false;

	for (i = 0; i < ARRAY_SIZE(update_list); i++) {
		get_veneer_param(update_list[i].veneer_id, &val);
		if (update_list[i].value != val) {
			update_list[i].value = val;
			update_uevent = true;
		}
	}

	if (update_uevent)
		ret = !ret;

	return ret;
}


///////////////////////////////////////////////////////////////////////////////

static enum power_supply_property extension_battery_appended [] = {
	POWER_SUPPLY_PROP_STATUS_RAW,
	POWER_SUPPLY_PROP_CAPACITY_RAW,
	POWER_SUPPLY_PROP_UPDATE_UEVENT,
};

enum power_supply_property* extension_battery_properties(void)
{
	static enum power_supply_property extended_properties[ARRAY_SIZE(battery_props) + ARRAY_SIZE(extension_battery_appended)];
	int size_original = ARRAY_SIZE(battery_props);
	int size_appended = ARRAY_SIZE(extension_battery_appended);

	memcpy(extended_properties, battery_props,
		size_original * sizeof(enum power_supply_property));
	memcpy(&extended_properties[size_original], extension_battery_appended,
		size_appended * sizeof(enum power_supply_property));

	veneer_extension_pspoverlap(battery_props, size_original,
		extension_battery_appended, size_appended);

	return extended_properties;
}

size_t extension_battery_num_properties(void)
{
	return ARRAY_SIZE(battery_props) + ARRAY_SIZE(extension_battery_appended);
}

int extension_battery_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct power_supply* veneer = power_supply_get_by_name("veneer");
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_BATTERY];
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	switch (psp) {
	case POWER_SUPPLY_PROP_CAPACITY :
		// Battery fake setting has top priority
		calculate_battery_soc(bcdev);
		if (ext_bcdev && ext_bcdev->fake.capacity >= 0) {
			val->intval = ext_bcdev->fake.capacity;
			return 0;
		}
		else if (ext_bcdev && ext_bcdev->no_batt_boot && !unified_bootmode_usermode()) {
			val->intval = 50;
			return 0;
		}
		else if (ext_bcdev && ext_bcdev->rescale.result != LGE_INITVAL) {
			val->intval = ext_bcdev->rescale.result;

			if (IS_ENABLED(CONFIG_QTI_PMIC_GLINK_CLIENT_DEBUG) &&
			   (bcdev->fake_soc >= 0 && bcdev->fake_soc <= 100))
				val->intval = bcdev->fake_soc;
			return 0;
		}
		break;

	case POWER_SUPPLY_PROP_CAPACITY_RAW :
		/* it is the original monotonic soc by 255 level. */
		battery_psy_get_prop(psy, POWER_SUPPLY_PROP_CAPACITY, val);
		val->intval = pst->prop[BATT_CAPACITY] * 255 / 10000;
		return 0;

	case POWER_SUPPLY_PROP_TEMP :
		val->intval = extension_get_battery_temp(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_EXT_TEMP_RAW :
		val->intval = calculate_battery_temperature(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW :
		if (ext_bcdev && ext_bcdev->fake.uvoltage >= 0) {
			val->intval = ext_bcdev->fake.uvoltage;
			return 0;
		}
		break;

	case POWER_SUPPLY_PROP_STATUS :
		if (!veneer || power_supply_get_property(veneer, POWER_SUPPLY_PROP_STATUS, val))
			break;
		return 0;

	case POWER_SUPPLY_PROP_HEALTH :
		if (!veneer || power_supply_get_property(veneer, POWER_SUPPLY_PROP_HEALTH, val))
			break;
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_OCV :
		return get_oem_property(bcdev, OEM_BATT_OCV, &val->intval);

	case POWER_SUPPLY_PROP_EXT_FORCE_UPDATE :
		get_force_update(bcdev, &val->intval);
		return 0;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG :
		if (!veneer || power_supply_get_property(veneer, POWER_SUPPLY_PROP_TIME_TO_FULL_NOW, val))
			break;
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT :
		if (!ext_bcdev || !ext_bcdev->fcc_votable)
			return -1;

		val->intval = get_effective_result(ext_bcdev->fcc_votable);
		return 0;

//	case POWER_SUPPLY_PROP_BATTERY_TYPE :
//	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN :

	case POWER_SUPPLY_PROP_STATUS_RAW :
		battery_psy_get_prop(psy, POWER_SUPPLY_PROP_STATUS, val);
		return 0;

	case POWER_SUPPLY_PROP_EXT_SAFETY_TIMER_ENABLE :
		return get_oem_property(bcdev, OEM_SAFETY_TIMER_ENABLE, &val->intval);

	case POWER_SUPPLY_PROP_EXT_QBG_MSOC_X100 :
		battery_psy_get_prop(psy, POWER_SUPPLY_PROP_CAPACITY, val);
		val->intval = pst->prop[BATT_CAPACITY];
		return 0;

	case POWER_SUPPLY_PROP_EXT_QBG_SYS_SOC_X100 :
		if (read_oem_qbg_info(bcdev)) {
			val->intval = pst->prop[BATT_CAPACITY];
		}
		val->intval = ext_bcdev->qbg_info.soc_sys;
		return 0;

	case POWER_SUPPLY_PROP_EXT_LG_UI_SOC_255 :
		if (!ext_bcdev)
			val->intval = pst->prop[BATT_CAPACITY] * 255 / 10000;
		else
			val->intval = ext_bcdev->rescale.result_255;
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX :
		val->intval = get_client_vote(ext_bcdev->fcc_votable, DEFAULT_VOTER);
		return 0;

	case POWER_SUPPLY_PROP_EXT_CHARGE_PAUSE :
		val->intval = read_oem_property(bcdev, OEM_CHG_PAUSE);
		return 0;

	case POWER_SUPPLY_PROP_EXT_QNI_DISCHARGE :
		val->intval = read_oem_property(bcdev, OEM_QNI_DISCHG);
		return 0;

	case POWER_SUPPLY_PROP_UPDATE_UEVENT :
		val->intval = extension_get_update_uevent(bcdev);
		return 0;

 	default:
		break;
	}

	return battery_psy_get_prop(psy, psp, val);
}

int extension_battery_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	pr_debug("Set %d to property(%d)", val->intval, psp);
	switch (psp) {
	case POWER_SUPPLY_PROP_TEMP :
		if (ext_bcdev) {
			ext_bcdev->fake.temperature = val->intval;
			power_supply_changed(psy);
		}
		return 0;

	case POWER_SUPPLY_PROP_CAPACITY :
		if (ext_bcdev) {
			ext_bcdev->fake.capacity = val->intval;
			power_supply_changed(psy);
		}
		return 0;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW :
		if (ext_bcdev) {
			ext_bcdev->fake.uvoltage = val->intval;
			power_supply_changed(psy);
		}
		return 0;

	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW :
		unified_nodes_write(UNIFIED_NODE_TIME_TO_FULL_NOW, val->intval);
		if(bcdev->psy_list[PSY_TYPE_BATTERY].psy)
			power_supply_changed(bcdev->psy_list[PSY_TYPE_BATTERY].psy);
		return 0;

	case POWER_SUPPLY_PROP_EXT_BATTERY_CHARGING_ENABLED :
		if (!ext_bcdev || !ext_bcdev->fcc_votable)
			return -1;

		return vote(ext_bcdev->fcc_votable, USER_VOTER, (bool)!val->intval, 0);

	case POWER_SUPPLY_PROP_EXT_SAFETY_TIMER_ENABLE :
		return write_oem_property(bcdev, OEM_SAFETY_TIMER_ENABLE, val->intval);

	case POWER_SUPPLY_PROP_EXT_FORCE_UPDATE :
		return set_force_update(bcdev, val->intval);

//	case POWER_SUPPLY_PROP_PARALLEL_DISABLE:
	case POWER_SUPPLY_PROP_EXT_DEBUG_BATTERY:
		debug_battery(bcdev, val->intval);
		return 0;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		vote(ext_bcdev->fcc_votable, DEFAULT_VOTER, true, val->intval);
		return 0;

	case POWER_SUPPLY_PROP_EXT_CHARGE_PAUSE :
		return write_oem_property(bcdev, OEM_CHG_PAUSE, val->intval);

	case POWER_SUPPLY_PROP_EXT_QNI_DISCHARGE :
		return write_oem_property(bcdev, OEM_QNI_DISCHG, val->intval);

	default:
		break;
	}

	return battery_psy_set_prop(psy, psp, val);
}

int extension_battery_property_is_writeable(struct power_supply *psy,
	enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW :
		rc = 1;
		break;
	default:
		rc = battery_psy_prop_is_writeable(psy, psp);
		break;
	}
	return rc;
}

/*************************************************************
 * simple extension for usb psy.
 */

static const char* adc_usbid_name(enum charger_usbid type)
{
	switch (type) {
	case CHARGER_USBID_UNKNOWN:	return "UNKNOWN";
	case CHARGER_USBID_56KOHM:	return "56K";
	case CHARGER_USBID_130KOHM:	return "130K";
	case CHARGER_USBID_910KOHM:	return "910K";
	case CHARGER_USBID_OPEN:	return "OPEN";
	default :
		break;
	}

	return "INVALID";
}

static int adc_usbid_range(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int i, usb_id[MAX_CHARGER_USBID];

	if (!ext_bcdev) {
		pr_info("USB-ID: Skip logging by ext_bcdev\n");
		return -1;
	}

	if (!ext_bcdev->pullup_mvol || !ext_bcdev->pullup_kohm || !ext_bcdev->usbid_range || !ext_bcdev->usbldo_range) {
		pr_err("USB-ID: Error on getting USBID ADC (pull up %dmvol, %dkohm, %dpct, %dpct)\n",
			ext_bcdev->pullup_mvol, ext_bcdev->pullup_kohm, ext_bcdev->usbid_range, ext_bcdev->usbldo_range);
		return -1;
	}

	ext_bcdev->usbid_table[0].type = CHARGER_USBID_56KOHM;
	ext_bcdev->usbid_table[1].type = CHARGER_USBID_130KOHM;
	ext_bcdev->usbid_table[2].type = CHARGER_USBID_910KOHM;
	ext_bcdev->usbid_table[3].type = CHARGER_USBID_OPEN;

	for (i = 0; i < MAX_CHARGER_USBID - 1; i++) {
		if (ext_bcdev->paral_kohm > 0)
			usb_id[i] = ext_bcdev->usbid_table[i].type/1000*ext_bcdev->paral_kohm/(ext_bcdev->paral_kohm+ext_bcdev->usbid_table[i].type/1000);
		else
			usb_id[i] = ext_bcdev->usbid_table[i].type/1000;

		ext_bcdev->usbid_table[i].min = ext_bcdev->pullup_mvol * (100 - ext_bcdev->usbldo_range)/100 * usb_id[i]
				* (100 - ext_bcdev->usbid_range)/100 / (usb_id[i] * (100 - ext_bcdev->usbid_range)/100 + ext_bcdev->pullup_kohm);
		ext_bcdev->usbid_table[i].max = ext_bcdev->pullup_mvol * (100 + ext_bcdev->usbldo_range)/100 * usb_id[i]
				* (100 + ext_bcdev->usbid_range)/100 / (usb_id[i] * (100 + ext_bcdev->usbid_range)/100 + ext_bcdev->pullup_kohm);
	}
	usb_id[i] = ext_bcdev->usbid_table[i].type/1000;
	ext_bcdev->usbid_table[i].min = ext_bcdev->usbid_table[i-1].max + 1;
	ext_bcdev->usbid_table[i].max = CHARGER_USBID_OPEN;

	for (i = 0; i < MAX_CHARGER_USBID; i++) {
		pr_info("USB-ID : %s min : %d, max %d\n",
				adc_usbid_name(ext_bcdev->usbid_table[i].type), ext_bcdev->usbid_table[i].min,  ext_bcdev->usbid_table[i].max);
	}

	return 0;
}

static int adc_usbid_uvoltage(struct battery_chg_dev *bcdev, struct iio_channel *channel)
{
	int val, rc = 0;

// Read ADC if possible
	rc |= wa_avoiding_mbg_fault_usbid(bcdev->dev, true);
	rc |= iio_read_channel_processed(channel, &val);
	rc |= wa_avoiding_mbg_fault_usbid(bcdev->dev, false);

	if (rc < 0)
		pr_info("USB-ID: Failed to read ADC\n");

	return rc >= 0 ? val : 0;
}

static enum charger_usbid adc_usbid_type(struct battery_chg_dev *bcdev, int mvoltage)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	enum charger_usbid 		usbid_ret = CHARGER_USBID_UNKNOWN;
	int i;

	if (!ext_bcdev) {
		pr_info("USB-ID: Skip logging by ext_bcdev\n");
		return usbid_ret;
	}

	if (!ext_bcdev->usbid_range) {
		pr_err("USB-ID: Error on getting USBID ADC (usbid_range=%d%%)\n",
			ext_bcdev->usbid_range);
		return usbid_ret;
	}

	for (i = 0; i < MAX_CHARGER_USBID; i++) {
		if (ext_bcdev->usbid_table[i].min <= mvoltage
				&& mvoltage <=ext_bcdev->usbid_table[i].max) {
			if (usbid_ret == CHARGER_USBID_UNKNOWN)
				usbid_ret = ext_bcdev->usbid_table[i].type;
			else
				pr_err("USB-ID: Overlap in usbid table!\n");
		}
	}

	return usbid_ret;
}

static bool psy_usbid_update(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev) {
		pr_info("USB-ID: Skip logging by ext_bcdev\n");
		return 0;
	}

	mutex_lock(&ext_bcdev->psy_usbid_mutex);
// Update all
	if (ext_bcdev->usb_id_chan) {
		ext_bcdev->cache_usbid_uvoltage = adc_usbid_uvoltage(bcdev, ext_bcdev->usb_id_chan);
		ext_bcdev->cache_usbid_type     = adc_usbid_type(bcdev, ext_bcdev->cache_usbid_uvoltage/1000);
		pr_info("USB-ID: Updated to %dmvol => %s\n",
			ext_bcdev->cache_usbid_uvoltage/1000, adc_usbid_name(ext_bcdev->cache_usbid_type));
	}
	else
		pr_err("USB-ID: Error on getting USBID ADC(mvol)\n");
	mutex_unlock(&ext_bcdev->psy_usbid_mutex);

// Check validation of result
	return ext_bcdev->cache_usbid_uvoltage > 0;
}

static enum charger_usbid psy_usbid_get(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	enum charger_usbid bootcable;

	if (!ext_bcdev) {
		pr_info("USB-ID: Skip logging by ext_bcdev\n");
		return unified_bootmode_usbid();
	}

	if (ext_bcdev->cache_usbid_type == CHARGER_USBID_INVALID) {
		mutex_lock(&ext_bcdev->psy_usbid_mutex);
		if (ext_bcdev->cache_usbid_type == CHARGER_USBID_INVALID) {
		       /* If cable detection is not initiated, refer to the cmdline */
			bootcable = unified_bootmode_usbid();
			pr_info("USB-ID: Not initiated yet, refer to boot USBID %s\n",
				adc_usbid_name(bootcable));
			ext_bcdev->cache_usbid_type = bootcable;
		}
		mutex_unlock(&ext_bcdev->psy_usbid_mutex);
	}

	return ext_bcdev->cache_usbid_type;
}

static int get_upper_pcb_adc(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val, rc = 0;

	if (!ext_bcdev) {
		pr_info("UPPER_PCB_ADC: Skip logging by ext_bcdev\n");
		return rc;
	}

	if (!ext_bcdev->upper_pcb_adc_chan) {
		pr_err("UPPER_PCB_ADC: Error on getting UPPER_PCB ADC(mvol)\n");
		return rc;
	}

	mutex_lock(&ext_bcdev->upper_pcb_adc_mutex);
	rc = iio_read_channel_processed(ext_bcdev->upper_pcb_adc_chan, &val);
	if (rc < 0)
		pr_err("UPPER_PCB_ADC: Failed to read ADC(%d)\n", rc);

	mutex_unlock(&ext_bcdev->upper_pcb_adc_mutex);
	pr_err("UPPER_PCB_ADC: UPPER_PCB ADC = %d(mvol)\n", val);
	return rc >= 0 ? val : 0;
}

static int get_low_pcb_adc(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val, rc = 0;

	if (!ext_bcdev) {
		pr_info("LOW_PCB_ADC: Skip logging by ext_bcdev\n");
		return rc;
	}

	if (!ext_bcdev->low_pcb_adc_chan) {
		pr_err("LOW_PCB_ADC: Error on getting LOW_PCB ADC(mvol)\n");
		return rc;
	}

	mutex_lock(&ext_bcdev->low_pcb_adc_mutex);
	rc = iio_read_channel_processed(ext_bcdev->low_pcb_adc_chan, &val);
	if (rc < 0)
		pr_err("LOW_PCB_ADC: Failed to read ADC(%d)\n", rc);

	mutex_unlock(&ext_bcdev->low_pcb_adc_mutex);
	pr_err("LOW_PCB_ADC: LOW_PCB ADC = %d(mvol)\n", val);
	return rc >= 0 ? val : 0;
}

static bool extension_usb_get_online(struct battery_chg_dev *bcdev)
{
	int online, fo = 0;
	bool ret = false;

	if (!get_oem_property(bcdev, OEM_USB_ONLINE, &online)
			&& !get_veneer_param(VENEER_FEED_FAKE_ONLINE, &fo)) {
		if (online && fo)
			ret = false;
		else
			ret = online;
	}

	return ret;
}

static bool extension_usb_get_parallel_enabled(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	static int count = 0;
	int capacity = bcdev->psy_list[PSY_TYPE_BATTERY].prop[BATT_CAPACITY] / 100;
	int status = 0, fastpl = 0;
	bool ret = false;

	if (!get_oem_property(bcdev, OEM_PARALLEL_MODE, &status)) {
		if (capacity > 70 && status != -1)
			ret = true;
		else if (status == 0xCB)
			ret = true;
	}

	if (unified_nodes_read(UNIFIED_NODE_SUPPORT_FASTPL, &fastpl) && fastpl) {
		pr_err("SMB: get charging enable %d (status 0x%x, soc : %d, count : %d)\n",
			ret, status, capacity, count);
		if (ext_bcdev && ext_bcdev->usbin_plugin && !ret) {
			count++;
			if (count > 6 && count % 2) {
				set_veneer_param(VENEER_FEED_DEBUG_BATT, REGDUMP_PERPH0);
				set_veneer_param(VENEER_FEED_DEBUG_BATT, REGDUMP_PERPH1);
				set_veneer_param(VENEER_FEED_DEBUG_BATT, REGDUMP_PERPH2);
			}
		} else {
			count = 0;
		}
	} else {
		count = 0;
	}

	return ret;
}

static bool extension_usb_set_input_current_limit(struct battery_chg_dev *bcdev, const union power_supply_propval* val)
{
	u32 usb_type = bcdev->psy_list[PSY_TYPE_USB].prop[USB_ADAP_TYPE];
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int skip = false;

	set_veneer_param(VENEER_FEED_USB3P0, val->intval == USB_ICL_900MA);
	if ((usb_type == POWER_SUPPLY_USB_TYPE_SDP || usb_type == POWER_SUPPLY_USB_TYPE_PD) &&
			(val->intval != 0 && val->intval != USB_ICL_900MA)) {
		vote(ext_bcdev->usb_icl_votable, USB_PSY_VOTER, true, val->intval/1000);
		skip = true;
	} else {
		vote(ext_bcdev->usb_icl_votable, USB_PSY_VOTER, false, val->intval/1000);
	}

	return skip;
}

//static bool get_fake_battery_status(void)
//static bool fake_hvdcp_property(struct smb_charger *chg)
//static bool fake_hvdcp_effected(struct smb_charger *chg)
//static bool fake_hvdcp_enable(struct smb_charger *chg, bool enable)

///////////////////////////////////////////////////////////////////////////////

static enum power_supply_property extension_usb_appended [] = {
	POWER_SUPPLY_PROP_PRESENT,
};

enum power_supply_property* extension_usb_properties(void)
{
	static enum power_supply_property extended_properties[ARRAY_SIZE(usb_props) + ARRAY_SIZE(extension_usb_appended)];
	int size_original = ARRAY_SIZE(usb_props);
	int size_appended = ARRAY_SIZE(extension_usb_appended);

	memcpy(extended_properties, usb_props,
		size_original * sizeof(enum power_supply_property));
	memcpy(&extended_properties[size_original], extension_usb_appended,
		size_appended * sizeof(enum power_supply_property));

	veneer_extension_pspoverlap(usb_props, size_original,
		extension_usb_appended, size_appended);

	return extended_properties;
}

size_t extension_usb_num_properties(void)
{
	return ARRAY_SIZE(usb_props) + ARRAY_SIZE(extension_usb_appended);
}

int extension_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE :
		val->intval = extension_usb_get_online(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_PRESENT :
		return get_oem_property(bcdev, OEM_USB_PRESENT, &val->intval);

	case POWER_SUPPLY_PROP_EXT_TYPEC_MODE :
		return get_oem_property(bcdev, OEM_TYPEC_MODE, &val->intval);

	case POWER_SUPPLY_PROP_EXT_CC_ORIENTATION :
		return get_oem_property(bcdev, OEM_CC_ORIENTATION, &val->intval);

	case POWER_SUPPLY_PROP_EXT_INPUT_POWER_NOW :
		return get_oem_property(bcdev, OEM_USB_POWER_NOW, &val->intval);

	case POWER_SUPPLY_PROP_EXT_REAL_TYPE :
		return get_oem_property(bcdev, OEM_CHG_TYPE, &val->intval);

	case POWER_SUPPLY_PROP_EXT_INPUT_CURRENT_SETTLED :
		return get_oem_property(bcdev, OEM_USB_AICL, &val->intval);

	case POWER_SUPPLY_PROP_EXT_RESISTANCE :	/* in uvol */
		if (!psy_usbid_update(bcdev))
			pr_err("USB-ID: Error on getting USBID\n");
		if (!ext_bcdev)
			return 1;

		val->intval = ext_bcdev->cache_usbid_uvoltage;
		return 0;

	case POWER_SUPPLY_PROP_EXT_RESISTANCE_ID :	/* in ohms */
		val->intval = psy_usbid_get(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_EXT_CHARGE_STATUS :
		return get_oem_property(bcdev, OEM_CHG_STATUS, &val->intval);

	case POWER_SUPPLY_PROP_EXT_UPPER_PCB :
		val->intval = get_upper_pcb_adc(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_EXT_LOW_PCB :
		val->intval = get_low_pcb_adc(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_EXT_CP_ENABLED :	//FIXED_ME
		val->intval = false;
		return 0;

	case POWER_SUPPLY_PROP_EXT_PARALLEL_ENABLED :
		val->intval = extension_usb_get_parallel_enabled(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_EXT_MOISTURE_EN :
		return get_oem_property(bcdev, OEM_MOISTURE_EN, &val->intval);

	default:
		break;
	}

	return usb_psy_get_prop(psy, psp, val);
}

int extension_usb_set_property(struct power_supply* psy,
		enum power_supply_property psp, const union power_supply_propval* val)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_EXT_RESISTANCE :
	case POWER_SUPPLY_PROP_EXT_RESISTANCE_ID :	/* in ohms */
		psy_usbid_update(bcdev);
		return 0;

	case POWER_SUPPLY_PROP_EXT_FAKE_HVDCP :
		return write_oem_property(bcdev, OEM_FAKE_HVDCP, val->intval);

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (extension_usb_set_input_current_limit(bcdev, val))
			return 0;
		break;

	case POWER_SUPPLY_PROP_EXT_MOISTURE_EN :
		return write_oem_property(bcdev, OEM_MOISTURE_EN, val->intval);

	default:
		break;
	}

	return usb_psy_set_prop(psy, psp, val);
}

int extension_usb_property_is_writeable(struct power_supply *psy,
	enum power_supply_property psp)
{
	int rc;

	switch (psp) {
	default:
		rc = usb_psy_prop_is_writeable(psy, psp);
		break;
	}

	return rc;
}

/*************************************************************
 * simple extension for dc psy. (for further purpose)
 */


static enum power_supply_property extension_wls_appended [] = {
};

enum power_supply_property* extension_wls_properties(void)
{
	static enum power_supply_property extended_properties[ARRAY_SIZE(wls_props) + ARRAY_SIZE(extension_wls_appended)];
	int size_original = ARRAY_SIZE(wls_props);
	int size_appended = ARRAY_SIZE(extension_wls_appended);

	memcpy(extended_properties, wls_props,
		size_original * sizeof(enum power_supply_property));
	memcpy(&extended_properties[size_original], extension_wls_appended,
		size_appended * sizeof(enum power_supply_property));

	veneer_extension_pspoverlap(wls_props, size_original,
		extension_wls_appended, size_appended);

	return extended_properties;
}

size_t extension_wls_num_properties(void)
{
	return ARRAY_SIZE(wls_props) + ARRAY_SIZE(extension_wls_appended);
}

int extension_wls_get_property(struct power_supply* psy,
	enum power_supply_property psp, union power_supply_propval* val)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int wls_en = 0;

	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT :
			return get_oem_property(bcdev, OEM_WLS_PRESENT, &val->intval);

		case POWER_SUPPLY_PROP_EXT_INPUT_POWER_NOW :
			return get_oem_property(bcdev, OEM_WLS_POWER_NOW, &val->intval);

		case POWER_SUPPLY_PROP_CURRENT_MAX :
			if (!ext_bcdev || !ext_bcdev->dc_icl_votable)
				return -1;

			val->intval = get_effective_result(ext_bcdev->dc_icl_votable);
			return 0;

		case POWER_SUPPLY_PROP_VOLTAGE_MAX :
			return get_oem_property(bcdev, OEM_WLS_VOLTAGE_MAX, &val->intval);

		case POWER_SUPPLY_PROP_EXT_INPUT_SUSPEND :
			get_oem_property(bcdev, OEM_WLS_EN, &wls_en);
			val->intval = !wls_en;
			return 0;

		default :
			break;
	}

	return wls_psy_get_prop(psy, psp, val);;
}

int extension_wls_set_property(struct power_supply* psy,
	enum power_supply_property psp, const union power_supply_propval* val)
{
	struct battery_chg_dev *bcdev = power_supply_get_drvdata(psy);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	switch (psp) {
	case POWER_SUPPLY_PROP_EXT_CHARGING_ENABLED :
		if (!ext_bcdev || !ext_bcdev->dc_icl_votable)
			return -1;

		vote(ext_bcdev->dc_icl_votable, USER_VOTER, !val->intval, 0);
		return 0;

	case POWER_SUPPLY_PROP_POWER_NOW :
		return write_oem_property(bcdev, OEM_WLS_POWER_NOW, val->intval);

	case POWER_SUPPLY_PROP_EXT_INPUT_SUSPEND :
		return write_oem_property(bcdev, OEM_WLS_EN, val->intval);

	default:
		break;
	}

	return wls_psy_set_prop(psy, psp, val);
}

int extension_wls_property_is_writeable(struct power_supply *psy,
	enum power_supply_property psp)
{
	switch (psp) {
	default:
		break;
	}

	return wls_psy_prop_is_writeable(psy, psp);;
}

/*************************************************************
 * work function
 */

void extenstion_battery_chg_update_usb_type_work(struct work_struct *work)
{
	struct battery_chg_dev *bcdev = container_of(work,
					struct battery_chg_dev, usb_type_work);
	struct psy_state *pst = &bcdev->psy_list[PSY_TYPE_USB];

	battery_chg_update_usb_type_work(work);

	switch (pst->prop[USB_ADAP_TYPE]) {
	case POWER_SUPPLY_USB_TYPE_SDP:
	case POWER_SUPPLY_USB_TYPE_CDP:
		usb_psy_desc_extension.type = POWER_SUPPLY_TYPE_USB;
		break;
	default:
		usb_psy_desc_extension.type = POWER_SUPPLY_TYPE_USB_PD;
		break;
	}

}

/*************************************************************
 * vote callback function
 */

static int fcc_vote_cb(struct votable *votable, void *data,
			int max_fcc_ua, const char *client)
{
	struct battery_chg_dev *bcdev = data;
	enum oem_property_id opid = OEM_FCC;

	if (!get_effective_client_locked(votable))
		return 0;

	if (get_effective_priority_locked(votable) >= PRIORITY_HIGH)
		opid = OEM_FCC_OVERRIDE;

#ifdef CONFIG_USE_WIRELESS_CHARGING
	set_veneer_param(VENEER_FEED_DC_POWER_NOW, max_fcc_ua);
#endif

	return write_oem_property(bcdev, opid, max_fcc_ua);
}

static int fv_vote_cb(struct votable *votable, void *data,
			int fv_uv, const char *client)
{
	struct battery_chg_dev *bcdev = data;
	enum oem_property_id opid = OEM_FOLAT_VOTAGE;

	if (!get_effective_client_locked(votable))
		return 0;

	if (get_effective_priority_locked(votable) >= PRIORITY_HIGH)
		opid = OEM_FOLAT_VOTAGE_OVERRIDE;

	return write_oem_property(bcdev, opid, fv_uv);
}

static int usb_icl_vote_cb(struct votable *votable, void *data,
			int icl_ua, const char *client)
{
	struct battery_chg_dev *bcdev = data;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	enum oem_property_id opid = OEM_USB_ICL;

	if (!get_effective_client_locked(votable))
		return 0;

	if (ext_bcdev->wlsin_plugin)
		return 0;

	if (get_effective_priority_locked(votable) >= PRIORITY_HIGH)
		opid = OEM_USB_ICL_OVERRIDE;

	if (icl_ua <= 50)
		icl_ua = 0;

	return write_oem_property(bcdev, opid, icl_ua);
}

static int dc_icl_vote_cb(struct votable *votable, void *data,
			int icl_ua, const char *client)
{
	struct battery_chg_dev *bcdev = data;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	enum oem_property_id opid = OEM_DC_ICL;

	if (!get_effective_client_locked(votable))
		return 0;

	if (ext_bcdev->usbin_plugin)
		return 0;

	if (get_effective_priority_locked(votable) >= PRIORITY_HIGH)
		opid = OEM_DC_ICL_OVERRIDE;

	return write_oem_property(bcdev, opid, icl_ua);
}

/*************************************************************
 * PMIC Glink of OEM.
 */
static int oem_log_mask = OEM_LOG_FULL;
static void set_oem_log_mask(struct battery_chg_dev *bcdev, u32 mask)
{
	enum oem_property_id opid = OEM_LOG_MASK;
	write_oem_property(bcdev, opid, mask);
	pr_info("It starts oem charging-log service from adsp to kernel.\n");
}

static void handle_oem_log(struct battery_chg_dev *bcdev, void *data, size_t len)
{
	struct oem_log_rsp_msg *rsp_msg = data;
	if (rsp_msg)
		printk("[AtoK] %s\n", rsp_msg->log);
}

static void handle_oem_rt_noti(struct battery_chg_dev *bcdev, void *data, size_t len)
{
	struct battman_oem_noti_msg *rsp_msg = data;
	if (rsp_msg)
	{
		printk("[rt_noti] id:%d,log:%s\n", rsp_msg->id, rsp_msg->log);
		switch(rsp_msg->id){
		case RT_NOTI_ID_WLS_SWICH_TO_USB :
			wa_wls_switch_to_usb();
			break;
		default:
			break;
		}
	}
}

static void handle_oem_qbg_info(struct battery_chg_dev *bcdev, void *data, size_t len)
{
	struct oem_qbg_info_rsp_msg *rsp_msg = data;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	memcpy(&ext_bcdev->qbg_info, &rsp_msg->qbg_info, sizeof(rsp_msg->qbg_info));
	complete(&bcdev->ack);
}

static void handle_oem_logging(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct chg_oem_logging_resp_msg *resp_msg = data;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	memcpy(&ext_bcdev->logging, &resp_msg->data, sizeof(resp_msg->data));
	complete(&bcdev->ack);
}

static int chg_oem_write(struct battery_chg_dev *bcdev, void *data,	int len)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int rc = 0;

	if (!ext_bcdev)
		return rc;
	/*
	 * When the subsystem goes down, it's better to return the last
	 * known values until it comes back up. Hence, return 0 so that
	 * pmic_glink_write() is not attempted until pmic glink is up.
	 */
	if (atomic_read(&bcdev->state) == PMIC_GLINK_STATE_DOWN) {
		pr_info("glink state is down\n");
		return 0;
	}

	if (bcdev->debug_battery_detected && bcdev->block_tx)
		return 0;

	mutex_lock(&bcdev->rw_lock);
	reinit_completion(&bcdev->ack);
	rc = pmic_glink_write(ext_bcdev->oem_client, data, len);
	if (!rc) {
		rc = wait_for_completion_timeout(&bcdev->ack,
					msecs_to_jiffies(BC_WAIT_TIME_MS));
		if (!rc) {
			pr_err("Error, timed out sending message\n");
			mutex_unlock(&bcdev->rw_lock);
			return -ETIMEDOUT;
		}

		rc = 0;
	}
	mutex_unlock(&bcdev->rw_lock);

	return rc;
}

int read_oem_qbg_info(struct battery_chg_dev *bcdev)
{
	struct oem_qbg_info_rsp_msg req_msg = { { 0 } };

	req_msg.hdr.owner = MSG_OWNER_OEM;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = OEM_OPCODE_QBG_INFO_GET;

	pr_debug("opcode = %d\n", req_msg.hdr.opcode);
	return chg_oem_write(bcdev, &req_msg, sizeof(req_msg));
}

int read_oem_logging(struct battery_chg_dev *bcdev)
{
	struct chg_oem_logging_resp_msg req_msg = { { 0 } };

	req_msg.hdr.owner = MSG_OWNER_OEM;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = OEM_OPCODE_LOGGING;

	pr_debug("opcode = %d\n", req_msg.hdr.opcode);
	return chg_oem_write(bcdev, &req_msg, sizeof(req_msg));
}

int read_oem_property(struct battery_chg_dev *bcdev,
			u32 prop_id)
{
	struct chg_oem_get_prop_req_msg req_msg = { { 0 } };

	req_msg.oem_property_id = prop_id;
	req_msg.hdr.owner = MSG_OWNER_OEM;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = OEM_OPCODE_GET_PROP;

	pr_debug("opcode = %d, prop id = %d\n", req_msg.hdr.opcode, req_msg.oem_property_id);
	return chg_oem_write(bcdev, &req_msg, sizeof(req_msg));
}

int write_oem_property(struct battery_chg_dev *bcdev,
			u32 prop_id, u32 data)
{
	struct chg_oem_set_prop_req_msg req_msg = { { 0 } };

	req_msg.oem_property_id = prop_id;
	req_msg.value = data;
	req_msg.hdr.owner = MSG_OWNER_OEM;
	req_msg.hdr.type = MSG_TYPE_REQ_RESP;
	req_msg.hdr.opcode = OEM_OPCODE_SET_PROP;

	pr_debug("opcode = %d, prop id = %d, val =%d \n",
		req_msg.hdr.opcode, req_msg.oem_property_id, req_msg.value);
	return chg_oem_write(bcdev, &req_msg, sizeof(req_msg));
}

static void handle_oem_message(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
	struct chg_oem_get_prop_resp_msg *resp_msg = data;
	u32 prop_id = resp_msg->oem_property_id;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	pr_debug("get prop of oem %d = %d\n", prop_id,resp_msg->value);
	ext_bcdev->prop_data[prop_id] = resp_msg->value;
	complete(&bcdev->ack);
}

static void handle_oem_notification(struct battery_chg_dev *bcdev, void *data,
				size_t len)
{
//	struct chg_oem_write_req_msg *notify_msg = data;
//
//	if (len != sizeof(*notify_msg)) {
//		pr_err("Incorrect response length %zu\n", len);
//		return;
//	}
//	switch (notify_msg->oem_property_id) {
//	case OEM_CHG_TYPE:
//		unified_nodes_write("charging_step", notify_msg->data_buffer[0]);
//		break;
//	default:
//		break;
//	}
}

static int chg_oem_callback(void *priv, void *data, size_t len)
{
	struct pmic_glink_hdr *hdr = data;
	struct battery_chg_dev *bcdev = priv;

	pr_debug("owner: %u type: %u opcode: %#x len: %zu\n",
		hdr->owner, hdr->type, hdr->opcode, len);
	if (hdr->type == MSG_TYPE_REQ_RESP) {
		if (hdr->opcode == OEM_OPCODE_GET_PROP)
			handle_oem_message(bcdev, data, len);
		else if (hdr->opcode == OEM_OPCODE_LOGGING)
			handle_oem_logging(bcdev, data, len);
		else if (hdr->opcode == OEM_OPCODE_QBG_INFO_GET)
			handle_oem_qbg_info(bcdev, data, len);
		else {
			pr_err("error oem gilnk owner: %u type: %u opcode: %#x len: %zu\n",
				hdr->owner, hdr->type, hdr->opcode, len);
			complete(&bcdev->ack);
		}
	} else if (hdr->type == MSG_TYPE_NOTIFY) {
		if (hdr->opcode == OEM_OPCODE_LOG)
			handle_oem_log(bcdev, data, len);
		else if (hdr->opcode == OEM_OPCODE_RT_NOTI)
			handle_oem_rt_noti(bcdev, data, len);
		else
			handle_oem_notification(bcdev, data, len);
	}

	return 0;
}


/*************************************************************
 * debugging attr of power.
 */

ssize_t get_regdump_addr_show(struct class *c, struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev)
		return 0;

	ext_bcdev->regdump_id++;
	if (ext_bcdev->regdump_id >= REGDUMP_MAX)
		ext_bcdev->regdump_id = REGDUMP_POLL;

	return scnprintf(buf, PAGE_SIZE, "[%d] = %s\n", ext_bcdev->regdump_id, bases[ext_bcdev->regdump_id].name);
}

ssize_t get_regdump_data_show(struct class *c, struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val = -1;

	if (!ext_bcdev)
		return 0;

	if (ext_bcdev->regdump_id < 0)
		return scnprintf(buf, PAGE_SIZE, "Error about init\n");

	if (ext_bcdev->regdump_id)
		val = ext_bcdev->regdump_id;

	set_veneer_param(VENEER_FEED_DEBUG_BATT, val);
	return scnprintf(buf, PAGE_SIZE, "Done\n");
}

ssize_t veneer_param_id_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	if (!ext_bcdev || val < 0 || val >= VENEER_FEED_MAX)
		return -EINVAL;

	ext_bcdev->veneer_param_id = val;

	return count;
}

ssize_t veneer_param_id_show(struct class *c, struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev)
		return 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", ext_bcdev->veneer_param_id);
}

ssize_t veneer_param_data_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val;

	if (kstrtoint(buf, 0, &val) || !ext_bcdev)
		return -EINVAL;

	set_veneer_param(ext_bcdev->veneer_param_id, val);

	return count;
}

ssize_t veneer_param_data_show(struct class *c, struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val;

	if (!ext_bcdev)
		return 0;

	get_veneer_param(ext_bcdev->veneer_param_id, &val);

	return scnprintf(buf, PAGE_SIZE, "%d : %d\n", ext_bcdev->veneer_param_id, val);
}

ssize_t oem_opcode_id_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val;

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	if (!ext_bcdev || val < 0 || val >= OEM_PROP_MAX)
		return -EINVAL;

	ext_bcdev->oem_opcode_id = val;

	return count;
}

ssize_t oem_opcode_id_show(struct class *c, struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (!ext_bcdev)
		return 0;

	return scnprintf(buf, PAGE_SIZE, "%d\n", ext_bcdev->oem_opcode_id);
}

ssize_t oem_opcode_data_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val;

	if (kstrtoint(buf, 0, &val) || !ext_bcdev)
		return -EINVAL;

	if (write_oem_property(bcdev, ext_bcdev->oem_opcode_id, val))
		return -EINVAL;

	return count;
}

ssize_t oem_opcode_data_show(struct class *c, struct class_attribute *attr, char *buf)
{
	struct battery_chg_dev *bcdev = container_of(c, struct battery_chg_dev,
						battery_class);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int val;

	if (!ext_bcdev)
		return 0;

	get_oem_property(bcdev, ext_bcdev->oem_opcode_id, &val);

	return scnprintf(buf, PAGE_SIZE, "%d : %d\n", ext_bcdev->oem_opcode_id, val);
}


/*************************************************************
 * extension for smb5 probe.
 */

void updata_battery_valid(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int battery_id = 0;
	int battery_valid = false;
	int rc = 0;
	union power_supply_propval val = { 0, };

	get_oem_property(bcdev, OEM_BATT_ID, &battery_id);
	get_oem_property(bcdev, OEM_BATT_ID_VALID, &battery_valid);
	rc = battery_psy_get_prop(bcdev->psy_list[PSY_TYPE_BATTERY].psy, POWER_SUPPLY_PROP_MODEL_NAME, &val);
	if (rc < 0)
		pr_info("Fail to get model name(Battery ID : %d, valid : %d)\n", battery_id, battery_valid);
	else
		pr_info("Battery ID : %d, model name = %s, valid : %d\n", battery_id, val.strval, battery_valid);

	battery_psy_get_prop(bcdev->psy_list[PSY_TYPE_BATTERY].psy, POWER_SUPPLY_PROP_TEMP, &val);
	if (ext_bcdev->pcb_batt_id >=0
			&& battery_id >= ext_bcdev->pcb_batt_id * 85/100
			&& battery_id <= ext_bcdev->pcb_batt_id * 115/100) {
		battery_valid = false;
		pr_info("Check battery temp for no battery boot temp = %d\n", val.intval);
		if (val.intval <= -250) {
			ext_bcdev->no_batt_boot = true;
		}
	}

	if (unified_bootmode_usermode())
		battery_valid = true;
	unified_nodes_write(UNIFIED_NODE_BATTERY_VALID , battery_valid);
}

void updata_battery_dcs(struct battery_chg_dev *bcdev)
{
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int battery_valid = false;

	unified_nodes_read(UNIFIED_NODE_BATTERY_VALID , &battery_valid);
	if (unified_bootmode_usermode() || battery_valid || !ext_bcdev->disable_dcs)
		write_oem_property(bcdev, OEM_DCS_ENABLE, true);
}

void extenstion_battery_chg_init_psy(struct battery_chg_dev *bcdev)
{
	batt_psy_desc_extension.name = batt_psy_desc.name;
	batt_psy_desc_extension.type = batt_psy_desc.type;
	batt_psy_desc_extension.properties = extension_battery_properties();
	batt_psy_desc_extension.num_properties = extension_battery_num_properties();
	batt_psy_desc_extension.get_property = extension_battery_get_property;
	batt_psy_desc_extension.set_property = extension_battery_set_property;
	batt_psy_desc_extension.property_is_writeable = extension_battery_property_is_writeable;

	usb_psy_desc_extension.name = usb_psy_desc.name;
	usb_psy_desc_extension.type = usb_psy_desc.type;
	usb_psy_desc_extension.properties = extension_usb_properties();
	usb_psy_desc_extension.num_properties = extension_usb_num_properties();
	usb_psy_desc_extension.get_property = extension_usb_get_property;
	usb_psy_desc_extension.set_property = extension_usb_set_property;
	usb_psy_desc_extension.usb_types = usb_psy_desc.usb_types;
	usb_psy_desc_extension.num_usb_types = usb_psy_desc.num_usb_types;
	usb_psy_desc_extension.property_is_writeable = extension_usb_property_is_writeable;

	wls_psy_desc_extension.name = wls_psy_desc.name;
	wls_psy_desc_extension.type = wls_psy_desc.type;
	wls_psy_desc_extension.properties = extension_wls_properties();
	wls_psy_desc_extension.num_properties = extension_wls_num_properties();
	wls_psy_desc_extension.get_property = extension_wls_get_property;
	wls_psy_desc_extension.set_property = extension_wls_set_property;
	wls_psy_desc_extension.property_is_writeable = extension_wls_property_is_writeable;
}

int extension_qg_load_icoeff_dt(struct battery_chg_dev *bcdev)
{
	struct device_node* tcomp_dtroot = NULL;
//	struct device_node* tcomp_override = NULL;
	int dt_icomp = 0;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (ext_bcdev->tcomp.icoeff_load_done) {
		pr_info("icoeff had already been loaded.\n");
		return 0;
	}

	tcomp_dtroot = of_find_node_by_name(NULL, "lge-battery-supplement");

	if (!tcomp_dtroot) {
		pr_info("failed to find lge-battery-supplement\n");
		return LGE_INITVAL;
	}

#if 0
	if (qg->bp.batt_type_str) {
		tcomp_override = of_find_node_by_name(
				tcomp_dtroot, qg->bp.batt_type_str);
		if (tcomp_override &&
				of_property_read_u32(
					tcomp_override, "tempcomp-icoeff", &dt_icomp) >= 0)
			pr_info("ICOEFF is overridden to %d for %s\n", dt_icomp, qg->bp.batt_type_str);
	}
#endif

	if (!dt_icomp) {
		if (of_property_read_u32(tcomp_dtroot, "tempcomp-icoeff", &dt_icomp) >= 0) {
			pr_info("ICOEFF is set to %d by default\n", dt_icomp);
		} else {
			pr_info("ICOEFF isn't set. error.\n");
			return -1;
		}
	}

	ext_bcdev->tcomp.icoeff = dt_icomp;
	ext_bcdev->tcomp.icoeff_load_done = true;
	return 0;
}

int extension_qg_load_dt(struct battery_chg_dev *bcdev)
{
	const char str_tempcomp[TCOMP_TABLE_MAX][30] = {
		"tempcomp-offset",
		"tempcomp-offset-wlc-lcdoff",
		"tempcomp-offset-wlc-lcdon"
	};

	struct device_node* tcomp_dtroot = NULL;
	int dtarray_count = TCOMP_COUNT * 2;
	u32 dtarray_data[TCOMP_COUNT * 2] = {0, };
	int i = 0, j = 0;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	int profile = 0;
	int hvt_trigger_soc[MAX_HVT_STEP_SIZE] = {0, };

	if (ext_bcdev->tcomp.load_done) {
		pr_info("tcomp table had already been loaded.\n");
		return 0;
	}

	tcomp_dtroot = of_find_node_by_name(NULL, "lge-battery-supplement");

	if (!tcomp_dtroot) {
		pr_info("failed to find lge-battery-supplement\n");
		return -1;
	}

	if (of_property_read_bool(tcomp_dtroot, "tempcomp-offset-wlc-enable"))
		ext_bcdev->tcomp.load_max = 3;
	else
		ext_bcdev->tcomp.load_max = 1;

	for (j = 0; j < ext_bcdev->tcomp.load_max; j++ ) {
		/* Finding tcomp_table and tcomp_icoeff */
		if (of_property_read_u32_array(tcomp_dtroot, str_tempcomp[j],
				dtarray_data, dtarray_count) >= 0) {
			for (i = 0; i < dtarray_count; i += 2) {
				ext_bcdev->tcomp.table[j][i/2].temp_cell = dtarray_data[i];
				ext_bcdev->tcomp.table[j][i/2].temp_bias = dtarray_data[i+1];
				pr_debug("Index = %02d : %4d - %4d\n",
					i/2,
					ext_bcdev->tcomp.table[j][i/2].temp_cell,
					ext_bcdev->tcomp.table[j][i/2].temp_bias);
			}
		} else {
			pr_info("%s is not found, error\n", str_tempcomp[j]);
			ext_bcdev->tcomp.table[j][0].temp_cell = INT_MAX;
			ext_bcdev->tcomp.table[j][i/2].temp_bias = 0;
			return -1;
		}
	}

	ext_bcdev->tcomp.logging = of_property_read_bool(tcomp_dtroot,
		"tempcomp-logging");
	ext_bcdev->smooth_therm.smooth_filter_enable = of_property_read_bool(tcomp_dtroot,
		"tempcomp-smooth-filter-enable");
	ext_bcdev->rescale.enable = of_property_read_bool(tcomp_dtroot,
		"capacity-scaling-enable");

	if (ext_bcdev->rescale.enable &&
		of_property_read_u32(tcomp_dtroot,
			"capacity-raw-full", &ext_bcdev->rescale.criteria)) {
		pr_err("Failed to read capacity-raw-full, setting 10000\n");
		ext_bcdev->rescale.criteria = 10000;
	}

	ext_bcdev->hvt_rescale.dt_en =
		of_property_read_bool(tcomp_dtroot, "lge,hvt-enable");

	ext_bcdev->hvt_rescale.dt_rescale_margin = 250;
	ext_bcdev->hvt_rescale.dt_fast_rescale_margin = 2500;

	if (ext_bcdev->rescale.enable && ext_bcdev->hvt_rescale.dt_en) {
		of_property_read_u32_array(tcomp_dtroot,
			"lge,hvt-trigger-soc-entry", hvt_trigger_soc, MAX_HVT_STEP_SIZE);
		of_property_read_u32(tcomp_dtroot,
			"lge,hvt-rescale-margin", &ext_bcdev->hvt_rescale.dt_rescale_margin);
		of_property_read_u32(tcomp_dtroot,
			"lge,hvt-fast-rescale-margin", &ext_bcdev->hvt_rescale.dt_fast_rescale_margin);

		for (i = 0; i < MAX_HVT_STEP_SIZE; i++)
			ext_bcdev->hvt_rescale.dt_scale[i] =
				hvt_trigger_soc[i] - ext_bcdev->hvt_rescale.dt_rescale_margin;

		/* set to minus 20% from hvt scale factor */
		ext_bcdev->hvt_rescale.dt_fast_scale =
			ext_bcdev->rescale.criteria - ext_bcdev->hvt_rescale.dt_fast_rescale_margin;

		hvt_soc_release(ext_bcdev);

		/* check veneer backup data */
		ext_bcdev->hvt_rescale.is_load_backup = false;
		get_hvt_count_from_veneer_backup(ext_bcdev);
	} else {
		for (i=0; i<MAX_HVT_STEP_SIZE; i++)
			ext_bcdev->hvt_rescale.dt_scale[i] = 9500;
	}

	pr_info("wsws: test: rescale:%d, criteria:%d\n",
			ext_bcdev->rescale.enable,
			ext_bcdev->rescale.criteria);
	if (j == ext_bcdev->tcomp.load_max) {
		ext_bcdev->tcomp.load_done = true;

		pr_info("[tempcomp config] table count: %s (%d/%d)\n",
			(j == ext_bcdev->tcomp.load_max) ? "done" : "error", j, ext_bcdev->tcomp.load_max);
	}

	if (of_property_read_u32(tcomp_dtroot, "max-voltage-uv", &profile) >= 0)
		vote(ext_bcdev->fv_votable, DEFAULT_VOTER, true, profile);

	if (of_property_read_u32(tcomp_dtroot, "input-current-ma", &profile) >= 0)
		vote(ext_bcdev->usb_icl_votable, DEFAULT_VOTER, true, profile);

	if (of_property_read_u32(tcomp_dtroot, "fastchg-current-ma", &profile) >= 0)
		vote(ext_bcdev->fcc_votable, DEFAULT_VOTER, true, profile);

	return 0;
}

int extension_parse_dt(struct battery_chg_dev *bcdev)
{
	int rc = 0;
	struct device_node *node = bcdev->dev->of_node;
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;
	char property[MAX_PROPERTY_LENGTH];

	rc = of_property_match_string(node, "io-channel-names", "usb_id");
	if (rc >= 0) {
		ext_bcdev->usb_id_chan = iio_channel_get(bcdev->dev, "usb_id");
		if (IS_ERR(ext_bcdev->usb_id_chan)) {
			rc = PTR_ERR(ext_bcdev->usb_id_chan);
			pr_err("USB_ID channel unavailable, %ld\n", rc);
			ext_bcdev->usb_id_chan = NULL;
		}
	}

	rc = of_property_match_string(node, "io-channel-names", "upper_pcb_adc");
	if (rc >= 0) {
		ext_bcdev->upper_pcb_adc_chan = iio_channel_get(bcdev->dev, "upper_pcb_adc");
		if (IS_ERR(ext_bcdev->upper_pcb_adc_chan)) {
			rc = PTR_ERR(ext_bcdev->upper_pcb_adc_chan);
			pr_err("UPPER_PCB_ADC channel unavailable, %ld\n", rc);
			ext_bcdev->upper_pcb_adc_chan = NULL;
		}
	}

	rc = of_property_match_string(node, "io-channel-names", "low_pcb_adc");
	if (rc >= 0) {
		ext_bcdev->low_pcb_adc_chan = iio_channel_get(bcdev->dev, "low_pcb_adc");
		if (IS_ERR(ext_bcdev->low_pcb_adc_chan)) {
			rc = PTR_ERR(ext_bcdev->low_pcb_adc_chan);
			pr_err("LOW_PCB_ADC channel unavailable, %ld\n", rc);
			ext_bcdev->low_pcb_adc_chan = NULL;
		}
	}

	rc = of_property_read_s32(of_find_node_by_name(node, "lge-extension-usb"),
		"lge,usbid-pullup-mvol", &ext_bcdev->pullup_mvol);
	if (rc < 0) {
		pr_err("Fail to get usbid-pullup-mvol rc = %d \n", rc);
		ext_bcdev->pullup_mvol = 0;
	}

	rc = of_property_read_s32(of_find_node_by_name(node, "lge-extension-usb"),
		"lge,usbid-pullup-kohm", &ext_bcdev->pullup_kohm);
	if (rc < 0) {
		pr_err("Fail to get usbid-pullup-kohm rc = %d \n", rc);
		ext_bcdev->pullup_kohm = 0;
	}

	rc = of_property_read_s32(of_find_node_by_name(node, "lge-extension-usb"),
		"lge,usbid-parallel-kohm", &ext_bcdev->paral_kohm);
	if (rc < 0) {
		pr_err("Fail to get usbid-parallel-kohm rc = %d \n", rc);
		ext_bcdev->paral_kohm = 0;
	}

	rc = of_property_read_s32(of_find_node_by_name(node, "lge-extension-usb"),
		"lge,usbid-adc-range", &ext_bcdev->usbid_range);
	if (rc < 0) {
		pr_err("Fail to get usbid-adc-range rc = %d \n", rc);
		ext_bcdev->usbid_range = 0;
	}

	rc = of_property_read_s32(of_find_node_by_name(node, "lge-extension-usb"),
		"lge,usbid-ldo-range", &ext_bcdev->usbldo_range);
	if (rc < 0) {
		pr_err("Fail to get usbid-adc-range rc = %d \n", rc);
		ext_bcdev->usbldo_range = 0;
	}

	pr_info("USB-ID: get pullup-mvol: %d, pullup-kohm: %d, "
			"parallel-kohm:%d, adc-range: %d, ldo-range: %d\n",
		ext_bcdev->pullup_mvol, ext_bcdev->pullup_kohm, ext_bcdev->paral_kohm,
		ext_bcdev->usbid_range, ext_bcdev->usbldo_range);

	// Build up USB-ID table
	adc_usbid_range(bcdev);

	rc = of_property_read_u32(node, "lge,pcb-battery-id", &ext_bcdev->pcb_batt_id);
	if (rc < 0) {
		pr_err("Fail to get pcb-battery-id rc = %d \n", rc);
		ext_bcdev->pcb_batt_id = -1;
	}

	ext_bcdev->disable_dcs = of_property_read_bool(node, "lge,disable-dcs");

	/* Disable HVDCP */
	unified_bootmode_get_property_region("lge,hvdcp-disable-", property);
	if (of_property_read_bool(node, property)) {
		pr_info("Disable HVDCP by %s\n", property);
		write_oem_property(bcdev, OEM_DISABLE_HVDCP, true);
	}

	extension_qg_load_dt(bcdev);
	extension_qg_load_icoeff_dt(bcdev);

	return rc;
}

#define MAX_HW_ICL_UA		3000000
int extension_chg_early_probe(struct battery_chg_dev *bcdev)
{
	int rc = 0;
	struct ext_battery_chg *ext_bcdev;
	struct pmic_glink_client_data client_data = { 0, };

	ext_bcdev = devm_kzalloc(bcdev->dev, sizeof(*ext_bcdev), GFP_KERNEL);
	if (!ext_bcdev) {
		pr_err("Couldn't alloc ext_smb_charger rc=%d\n", ext_bcdev);
		return rc;
	}
	bcdev->ext_bcdev = ext_bcdev;
	ext_bcdev->bcdev = bcdev;
	mutex_init(&ext_bcdev->psy_usbid_mutex);
	mutex_init(&ext_bcdev->upper_pcb_adc_mutex);
	mutex_init(&ext_bcdev->low_pcb_adc_mutex);

	ext_bcdev->fake.capacity = -9999;
	ext_bcdev->fake.temperature = -9999;
	ext_bcdev->fake.uvoltage = -9999;

	ext_bcdev->rescale.enable = false;
	ext_bcdev->rescale.rawsoc = LGE_INITVAL;
	ext_bcdev->rescale.result = LGE_INITVAL;
	ext_bcdev->no_batt_boot = false;
	ext_bcdev->regdump_id = -1;

	client_data.id = MSG_OWNER_OEM;
	client_data.name = "chg_oem";
	client_data.msg_cb = chg_oem_callback;
	client_data.priv = bcdev;
	client_data.state_cb = NULL;

	ext_bcdev->oem_client = pmic_glink_register_client(bcdev->dev, &client_data);

	ext_bcdev->fcc_votable = create_votable("FCC", VOTE_MIN,
					fcc_vote_cb, bcdev);
	if (IS_ERR(ext_bcdev->fcc_votable)) {
			rc = PTR_ERR(ext_bcdev->fcc_votable);
			ext_bcdev->fcc_votable = NULL;
			pr_err("Couldn't create vote of FCC rc=%d\n", rc);
	}

	ext_bcdev->fv_votable = create_votable("FV", VOTE_MIN,
					fv_vote_cb, bcdev);
	if (IS_ERR(ext_bcdev->fv_votable)) {
			rc = PTR_ERR(ext_bcdev->fv_votable);
			ext_bcdev->fv_votable = NULL;
			pr_err("Couldn't create vote of FV rc=%d\n", rc);
	}

	ext_bcdev->usb_icl_votable = create_votable("USB_ICL", VOTE_MIN,
					usb_icl_vote_cb, bcdev);
	if (IS_ERR(ext_bcdev->usb_icl_votable)) {
			rc = PTR_ERR(ext_bcdev->usb_icl_votable);
			ext_bcdev->usb_icl_votable = NULL;
			pr_err("Couldn't create vote of USB_ICL rc=%d\n", rc);
	}

	ext_bcdev->dc_icl_votable = create_votable("DC_ICL", VOTE_MIN,
					dc_icl_vote_cb, bcdev);
	if (IS_ERR(ext_bcdev->dc_icl_votable)) {
			rc = PTR_ERR(ext_bcdev->dc_icl_votable);
			ext_bcdev->dc_icl_votable = NULL;
			pr_err("Couldn't create vote of DC_ICL rc=%d\n", rc);
	}

	ext_bcdev->vwls_votable = find_votable("WLC_VOLTAGE");
	if (IS_ERR(ext_bcdev->vwls_votable)) {
			rc = PTR_ERR(ext_bcdev->vwls_votable);
			ext_bcdev->vwls_votable = NULL;
			pr_err("Couldn't get vote of WLC_VOLTAGE rc=%d\n", rc);
	}

	ext_bcdev->dc_psy = power_supply_get_by_name("dc");
	if (IS_ERR(ext_bcdev->dc_psy)) {
			rc = PTR_ERR(ext_bcdev->dc_psy);
			ext_bcdev->dc_psy = NULL;
			pr_err("Couldn't get dc_psy rc=%d\n", rc);
	}

	set_oem_log_mask(bcdev, oem_log_mask);
	extension_parse_dt(bcdev);

	pr_info("extension_chg early probe is successful.\n");
	return rc;
}

int extension_chg_probe(struct battery_chg_dev *bcdev)
{
	updata_battery_valid(bcdev);
	updata_battery_dcs(bcdev);
	wa_helper_init(bcdev);
	set_veneer_param(VENEER_FEED_USB3P0, false);
	set_veneer_param(VENEER_FEED_DC_FORCE_UPDATE, true);

	pr_info("extension_chg probe is successful.\n");
	return 0;
}

int battery_chg_suspend(struct device *dev)
{
	struct battery_chg_dev *bcdev = dev_get_drvdata(dev);
	struct ext_battery_chg *ext_bcdev = bcdev->ext_bcdev;

	if (ext_bcdev && ext_bcdev->smooth_therm.smooth_filter_enable)
		ext_bcdev->smooth_therm.batt_therm_ready = false;

	return 0;
}

void battery_chg_shutdown(struct platform_device *pdev)
{
	struct battery_chg_dev *bcdev = platform_get_drvdata(pdev);
	struct battery_charger_notify_msg msg = { { 0 } };
	int rc = 0;

	msg.hdr.owner = MSG_OWNER_BC;
	msg.hdr.type = MSG_TYPE_REQ_RESP;
	msg.hdr.opcode = BC_SET_SHDN_REQ;
	msg.notification = 0;

	rc = battery_chg_write(bcdev, &msg, sizeof(msg));
	if (rc < 0) {
		pr_err("Failed to notify shutdown rc=%d\n", rc);
	}
}
