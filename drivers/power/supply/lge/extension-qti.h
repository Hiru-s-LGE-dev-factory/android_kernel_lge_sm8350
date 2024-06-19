
#ifndef __EXTENSION_LIB_H
#define __EXTENSION_LIB_H

#include <linux/soc/qcom/pmic_glink.h>
#include <linux/soc/qcom/battery_charger.h>
#include "veneer-primitives.h"

#define MSG_OWNER_OEM                  32782
#define OEM_OPCODE_READ_BUFFER         0x10000
#define OEM_OPCODE_WRITE_BUFFER        0x10001

//OEM specific opcodes
#define OEM_OPCODE_LOGGING             0x10
#define OEM_OPCODE_GET_PROP            0x11
#define OEM_OPCODE_SET_PROP            0x12
#define OEM_OPCODE_QBG_INFO_GET        0x13
#define OEM_OPCODE_RT_NOTI             0x14

#define OEM_OPCODE_LOG                 0x20
#define OEM_LOG_CRITICAL               0x0001
#define OEM_LOG_ERROR                  0x0002
#define OEM_LOG_INFO                   0x0004
#define OEM_LOG_WARN                   0x0008
#define OEM_LOG_DEBUG                  0x0010
#define OEM_LOG_VERBOSE                0x0020
#define OEM_LOG_TRACE_CRITICAL         0x0100
#define OEM_LOG_TRACE_ERROR            0x0200
#define OEM_LOG_TRACE_INFO             0x0400
#define OEM_LOG_TRACE_WARN             0x0800
#define OEM_LOG_TRACE_DEBUG            0x1000
#define OEM_LOG_TRACE_VERBOSE          0x2000
#define OEM_LOG_FULL                   0xffff

#define BC_SET_SHDN_REQ                0x0047

#define USER_VOTER 	"USER_VOTER"
#define USB_PSY_VOTER   "USB_PSY_VOTER"

#define USB_ICL_900MA		900000
#define USB_ICL_500MA		500000

enum oem_property_id {
	OEM_FCC,
	OEM_FCC_OVERRIDE,
	OEM_USB_ICL,
	OEM_USB_ICL_OVERRIDE,
	OEM_DC_ICL,
	OEM_DC_ICL_OVERRIDE,
	OEM_FOLAT_VOTAGE,
	OEM_FOLAT_VOTAGE_OVERRIDE,
	OEM_CHG_TYPE,
	OEM_CHG_STATUS,
	OEM_SAFETY_TIMER_ENABLE,
	OEM_USB_PRESENT,
	OEM_USB_ONLINE,
	OEM_USB_AICL,
	OEM_TYPEC_MODE,
	OEM_USB_POWER_NOW,
	OEM_WLS_PRESENT,
	OEM_WLS_POWER_NOW,
	OEM_WLS_VOLTAGE_MAX,
	OEM_FAKE_HVDCP,
	OEM_BATT_OCV,
	OEM_BATT_ID,
	OEM_BATT_ID_VALID,
	OEM_PARALLEL_MODE,
	OEM_CC_ORIENTATION,
	OEM_APSD_RERUN,
	OEM_LOG_MASK,
	OEM_WLS_EN,
	OEM_CHG_PAUSE,
	OEM_DCS_ENABLE,
	OEM_QNI_DISCHG,
	OEM_MOISTURE_EN,
	OEM_ENABLE_MCP_CV,
	OEM_DISABLE_HVDCP,
	OEM_COMPLIANCE_MODE,
	OEM_PROP_MAX,
};

typedef enum {
	RT_NOTI_ID_WLS_SWICH_TO_USB,
	RT_NOTI_ID_MAX,
} rt_noti_id;

struct usbid_entry {
	enum charger_usbid type;
	int 	   min;
	int 	   max;
};

struct oem_qbg_info_type {
	u32 soc_bat;
	u32 soc_sys;
	u32 soc_mono;
	u32 learned_capacity;
	u32 charge_counter;
	u32 cycle_count;
	u32 resistance;
	u32 recharge_soc;
};

#define BATTMNGRTECH_AGGREGATOR_NAME_MAX_LENGTH                 12
struct oem_logging_type {
	u32 soc_bat;
	u32 soc_sys;
	u32 soc_mono;
	u32 learned_capacity;
	u32 charge_counter;
	u32 cycle_count;
	u32 esr;
	u32 recharge_soc;

	u32 vbat;
	u32 ibat;
	u32 vbus;
	u32 iusb;
	u32 vwls;
	u32 iwls;
	u32 aicl;
	u32 aicl_done;
	u32 bat_temp;
	u32 fsmb;
	u32 ismb;
	u32 ocv_mv;
	u32 vote_icl;
	u32 vote_fcc;
	u32 vote_fv;
	u32 reg_icl;
	u32 reg_fcc;
	u32 reg_fv;
	u32 reg_wls;
	u32 smb_icl;
	u32 smb_ichg;
	u32 smb_vout;
	char icl_name[BATTMNGRTECH_AGGREGATOR_NAME_MAX_LENGTH +1];
	char fcc_name[BATTMNGRTECH_AGGREGATOR_NAME_MAX_LENGTH +1];
	char fv_name[BATTMNGRTECH_AGGREGATOR_NAME_MAX_LENGTH +1];
	u32 charger_type;
	u32 pwr_path;
	u32 bsm_state;
	u32 icm_state;
	u32 icm_sub_state;
	u8 smb_perph0_int;
	u8 smb_perph1_int;
	u8 smb_perph0_mode;
};

#define OEM_LOG_MAX_LENGTH    256
struct oem_log_rsp_msg {
	struct pmic_glink_hdr hdr;
	char log[OEM_LOG_MAX_LENGTH];
};

struct oem_qbg_info_rsp_msg {
	struct pmic_glink_hdr hdr;
	struct oem_qbg_info_type qbg_info;
};

struct chg_oem_logging_resp_msg {
	struct pmic_glink_hdr hdr;
	struct oem_logging_type data;
};

struct chg_oem_get_prop_req_msg {
	struct pmic_glink_hdr hdr;
	u32 oem_property_id;
};

struct chg_oem_get_prop_resp_msg {
	struct pmic_glink_hdr hdr;
	u32 oem_property_id;
	u32 value;
	u32 return_code;
};

struct chg_oem_set_prop_req_msg {
	struct pmic_glink_hdr hdr;
	u32 oem_property_id;
	u32 value;
};

struct chg_oem_set_prop_resp_msg {
	struct pmic_glink_hdr hdr;
	u32 oem_property_id;
	u32 return_code;
};

#define OEM_NOTI_LOG_MAX_LENGTH    64
struct battman_oem_noti_msg {
	struct pmic_glink_hdr hdr;
	u32 id;
	char log[OEM_NOTI_LOG_MAX_LENGTH];
};


struct _fake {
	int temperature;
	int capacity;
	int uvoltage;
};

struct _rescale {
	bool enable;
	int criteria;    // 0 ~ 10000
	int rawsoc;      // QBG MSOC, 0 ~ 10000
	int result;      // LG UI SOC, 0 ~ 100
	int result_255;  // LG UI SOC, 0 ~ 255 for charging time
};

/* high voltage-temperature compensation */
enum {
	HVT_RESCALE_STATE_NONE = 0,
	HVT_RESCALE_STATE_ENABLED,
	HVT_RESCALE_STATE_EN_DISCHARGE,
	HVT_RESCALE_STATE_EN_CHARGE,
	HVT_RESCALE_STATE_MAX,
};

struct _hvt_rescale {
	bool en;
	bool is_load_backup;
	int state;
	int count;
	int ui_raw;                         // original rescaled soc, 0 ~ 100
	int hvt_scale;                      // 1/10000, variable
	int scale_now;                      // 1/10000, variable
	int soc_error;
	unsigned long last_time;            // last updating time - rtc

	bool dt_en;
	int dt_scale[MAX_HVT_STEP_SIZE];    // 1/10000, lge,hvt-trigger-soc-entry - 2.5% (250 fixed)
	int dt_fast_scale;                  // 1/10000, fixed : hvt_scale + 20%
	int dt_rescale_margin;
	int dt_fast_rescale_margin;
};

#define TCOMP_TABLE_MAX 3
#define TCOMP_COUNT 25
struct tcomp_param {
	bool load_done;
	int load_max;
	bool icoeff_load_done;
	struct tcomp_entry {
		int temp_cell;
		int temp_bias;
	} table[TCOMP_TABLE_MAX][TCOMP_COUNT];
	int icoeff;

	bool logging;
};

struct smooth_param {
/*
 * To update batt_therm immediately when wakeup from suspend,
 * using below flag in suspend/resume callback
 */
	bool		smooth_filter_enable;
	bool		batt_therm_ready;
	bool		enable_flag;
	int		therm_backup;
	ktime_t		last_time;
};

enum tcomp_chg_type {
	TCOMP_CHG_NONE = 0,
	TCOMP_CHG_USB,
	TCOMP_CHG_WLC_LCDOFF,
	TCOMP_CHG_WLC_LCDON
};

enum regdump_list {
	REGDUMP_POLL,
	REGDUMP_CHGR,
	REGDUMP_DCDC,
	REGDUMP_BATIF,
	REGDUMP_USB,
	REGDUMP_WLS,
	REGDUMP_TYPEC,
	REGDUMP_MISC,
	REGDUMP_PD,
	REGDUMP_PERPH0,
	REGDUMP_PERPH1,
	REGDUMP_PERPH2,
	REGDUMP_MAX,
};

struct ext_battery_chg {
	struct battery_chg_dev *bcdev;

//	struct voter_entry cp_qc3_ibat;

	struct power_supply	*dc_psy;

	struct iio_channel	*usb_id_chan;
	struct iio_channel	*upper_pcb_adc_chan;
	struct iio_channel	*low_pcb_adc_chan;

	enum charger_usbid cache_usbid_type;
	int cache_usbid_uvoltage;
	int pullup_mvol;
	int pullup_kohm;
	int paral_kohm;
	int usbid_range; // pct unit
	int usbldo_range; // pct unit
	struct usbid_entry usbid_table[MAX_CHARGER_USBID];
	int pcb_batt_id;
	bool disable_dcs;
	bool no_batt_boot;

	struct pmic_glink_client	*oem_client;
	u32 prop_data[OEM_PROP_MAX];
	struct oem_qbg_info_type qbg_info;
	struct oem_logging_type	logging;

	int force_update;
	struct mutex psy_usbid_mutex;
	struct mutex upper_pcb_adc_mutex;
	struct mutex low_pcb_adc_mutex;
	struct smooth_param smooth_therm;
	struct tcomp_param tcomp;
	struct _rescale rescale;
	struct _hvt_rescale hvt_rescale;

	struct votable		*fcc_votable;
	struct votable		*usb_icl_votable;
	struct votable		*dc_icl_votable;
	struct votable		*fv_votable;
	struct votable		*vwls_votable;
	struct _fake fake;

	/* notifiers */
	struct notifier_block	nb;
	struct work_struct	psy_update_work;

	int usbin_plugin;
	int wlsin_plugin;
	int usb_type;
	int typec_mode;
	int regdump_id;

	bool is_usb_configured;
	bool is_usb_compliance_mode;
	bool is_hall_ic;
	int settled_ua;

	int veneer_param_id;
	int oem_opcode_id;

	// Rerun apsd for abnormal sdp
	bool enable_rerun_apsd_sdp;
	bool wa_rerun_apsd_done_with_sdp;
	struct delayed_work	wa_rerun_apsd_for_sdp_dwork;

	// Enable CP charging for 2nd charger test
	bool enable_faster_try;

	// Support for weak battery pack
	bool enable_support_weak_supply;
	int	wa_support_weak_supply_count;
	struct delayed_work	wa_support_weak_supply_dwork;

	// Avoid Inrush current for USB Compliance test
	bool enable_avoid_inrush_current;
	struct delayed_work	wa_avoid_inrush_current_dwork;
};

int battery_chg_suspend(struct device *dev);
void battery_chg_shutdown(struct platform_device *pdev);

int read_oem_qbg_info(struct battery_chg_dev *bcdev);
int read_oem_logging(struct battery_chg_dev *bcdev);
int read_oem_property(struct battery_chg_dev *bcdev, u32 prop_id);
int write_oem_property(struct battery_chg_dev *bcdev, u32 prop_id, u32 data);

ssize_t get_regdump_addr_show(struct class *c, struct class_attribute *attr, char *buf);
static CLASS_ATTR_RO(get_regdump_addr);
ssize_t get_regdump_data_show(struct class *c, struct class_attribute *attr, char *buf);
static CLASS_ATTR_RO(get_regdump_data);

ssize_t veneer_param_id_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count);
ssize_t veneer_param_id_show(struct class *c, struct class_attribute *attr, char *buf);
static CLASS_ATTR_RW(veneer_param_id);

ssize_t veneer_param_data_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count);
ssize_t veneer_param_data_show(struct class *c, struct class_attribute *attr, char *buf);
static CLASS_ATTR_RW(veneer_param_data);

ssize_t oem_opcode_id_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count);
ssize_t oem_opcode_id_show(struct class *c, struct class_attribute *attr, char *buf);
static CLASS_ATTR_RW(oem_opcode_id);

ssize_t oem_opcode_data_store(struct class *c, struct class_attribute *attr, const char *buf, size_t count);
ssize_t oem_opcode_data_show(struct class *c, struct class_attribute *attr, char *buf);
static CLASS_ATTR_RW(oem_opcode_data);

static struct power_supply_desc batt_psy_desc_extension;
static struct power_supply_desc usb_psy_desc_extension;
static struct power_supply_desc wls_psy_desc_extension;
static int calculate_battery_temperature(struct battery_chg_dev *bcdev);
static void calculate_battery_soc(struct battery_chg_dev *bcdev);
int extension_chg_early_probe(struct battery_chg_dev *bcdev);
int extension_chg_probe(struct battery_chg_dev *bcdev);
void extenstion_battery_chg_init_psy(struct battery_chg_dev *bcdev);
void extenstion_battery_chg_update_usb_type_work(struct work_struct *work);

void wa_helper_init(struct battery_chg_dev *bcdev);
bool wa_avoiding_mbg_fault_uart(struct device *dev, bool enable);
bool wa_avoiding_mbg_fault_usbid(struct device *dev, bool enable);
void wa_wls_switch_to_usb(void);


extern bool unified_bootmode_fabproc(void);
extern bool unified_bootmode_usermode(void);
extern bool unified_bootmode_chargerlogo(void);

#endif /* __EXTENSION_LIB_H */
