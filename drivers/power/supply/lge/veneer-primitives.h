#ifndef _VENEER_COMMON_H_
#define _VENEER_COMMON_H_

#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/pmic-voter.h>
#include <linux/pm_qos.h>

#define OF_PROP_READ_U32(dnode, buf, prop, rc)                                \
do {                                                                          \
	if (rc)                                                                   \
		break;                                                                \
                                                                              \
	rc = of_property_read_u32(dnode, "lge," prop, &buf);                      \
                                                                              \
	if (rc)                                                                   \
		pr_info("VENEER: Error reading " #prop " property rc = %d\n", rc);    \
	else                                                                      \
		pr_debug("VENEER: %s : %d\n", prop, buf);                             \
} while (0)

#define OF_PROP_READ_S32(dnode, buf, prop, rc)						\
do {											\
	if (rc)										\
		break;									\
											\
	rc = of_property_read_s32(dnode, "lge," prop, &buf);				\
											\
	if (rc)										\
		pr_info("VENEER: Error reading " #prop " property rc = %d\n", rc);	\
	else										\
		pr_debug("VENEER: %s : %d\n", prop, buf);				\
} while (0)

#define OF_PROP_READ_STR(dnode, buf, prop, rc)						\
do {											\
	if (rc)										\
		break;									\
											\
	rc = of_property_read_string(dnode, "lge," prop, &buf);				\
											\
	if (rc)										\
		pr_info("VENEER: Error reading " #prop " property rc = %d\n", rc);	\
	else										\
		pr_debug("VENEER: %s : %s\n", prop, buf);				\
} while (0)

#define OF_PROP_READ_GPIO(dnode, buf, prop, rc)						\
do {											\
	if (rc) 									\
		break;									\
											\
	buf = of_get_named_gpio(dnode, "lge," prop, 0); 			\
											\
	if (buf < 0) { 									\
		pr_info("VENEER: Error reading " #prop " property rc = %d\n", buf);	\
		rc = buf;								\
	} else										\
		pr_debug("VENEER: %s : %d\n", prop, buf);				\
} while (0)

/* VOTE_VALUE_GRINDER is used to identify who (== one of IUSB/IBAT/IDC/VFLOAT) has voted.
 * So VOTE_TOTALLY_RELEASED and VOTE_TOTALLY_BLOCKED are designed to import the
 * GRINDER value
 */
#define VOTE_VALUE_GRINDER 	10
#define VOTE_TOTALLY_RELEASED	((INT_MAX/VOTE_VALUE_GRINDER)*VOTE_VALUE_GRINDER)
#define VOTE_TOTALLY_BLOCKED 	((      0/VOTE_VALUE_GRINDER)*VOTE_VALUE_GRINDER)
#define VOTER_NAME_LENGTH 30
#define LGE_INITVAL -1
#define MAX_PROPERTY_LENGTH 32

enum voter_type {
	VOTER_TYPE_INVALID = 0,
	VOTER_TYPE_IUSB,
	VOTER_TYPE_IBAT,
	VOTER_TYPE_IDC,
	VOTER_TYPE_VFLOAT,
	VOTER_TYPE_ACTM_LCDON_TEMP_OFFSET,    /* extra item */
	/* add 'veneer_voter_type's here */
};

#define MAX_HVT_STEP_SIZE    12
#define MAX_IRC_VOLTAGE      4430

enum veneer_feed_id {
	VENEER_FEED_ACTM_MODE,
	VENEER_FEED_ACTM_MODE_NOW,
	VENEER_FEED_ACTM_LCDON_TEMP_OFFSET,
	VENEER_FEED_ACTM_SENSOR_WIRED,
	VENEER_FEED_ACTM_SENSOR_WIRELESS,
	VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRED_0,
	VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRED_1,
	VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRED_2,
	VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRELESS_0,
	VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRELESS_1,
	VENEER_FEED_ACTM_MAX_HOLD_CRITERIA_WIRELESS_2,
	VENEER_FEED_ACTM_TEMPOFFS_WIRED_0,
	VENEER_FEED_ACTM_TEMPOFFS_WIRED_1,
	VENEER_FEED_ACTM_TEMPOFFS_WIRED_2,
	VENEER_FEED_ACTM_TEMPOFFS_WIRELESS_0,
	VENEER_FEED_ACTM_TEMPOFFS_WIRELESS_1,
	VENEER_FEED_ACTM_TEMPOFFS_WIRELESS_2,
	VENEER_FEED_ACTM_MAX_FCC_PPS,
	VENEER_FEED_ACTM_MAX_FCC_QC3,
	VENEER_FEED_ACTM_MAX_FCC_QC2,
	VENEER_FEED_ACTM_CURRENT_WIRED_0,
	VENEER_FEED_ACTM_CURRENT_WIRED_1,
	VENEER_FEED_ACTM_CURRENT_WIRED_2,
	VENEER_FEED_ACTM_CURRENT_CP_PPS,
	VENEER_FEED_ACTM_CURRENT_CP_QC30,
	VENEER_FEED_ACTM_CURRENT_EPP_0,
	VENEER_FEED_ACTM_CURRENT_EPP_1,
	VENEER_FEED_ACTM_CURRENT_EPP_2,
	VENEER_FEED_ACTM_CURRENT_BPP_0,
	VENEER_FEED_ACTM_CURRENT_BPP_1,
	VENEER_FEED_ACTM_CURRENT_BPP_2,
	VENEER_FEED_STATUS_RAW,
	VENEER_FEED_CAPACITY,	    /*protection-batvolt.c*/
	VENEER_FEED_LG_UI_SOC_255,
	VENEER_FEED_QBG_MSOC_X100,
	VENEER_FEED_QBG_SYS_SOC_X100,
	VENEER_FEED_CHARGE_TYPE,
	VENEER_FEED_CHARGER_TYPE,	/* venner supplier */
	VENEER_FEED_SENSOR_BATT,
	VENEER_FEED_SENSOR_VTS,			//FIXED_ME
	VENEER_FEED_BAT_VOLTAGE_NOW,
	VENEER_FEED_FCC,
	VENEER_FEED_IDC,
	VENEER_FEED_VDC,
	VENEER_FEED_IRC_ENABLED,
	VENEER_FEED_IRC_RESISTANCE,
	VENEER_FEED_LCDON_STATUS,
	VENEER_FEED_POWER_SUPPLY_CHANGED,
	VENEER_FEED_BATT_PROFILE_FCC_VOTER,   /* mA, adaptive-charging-thermal.c */
	VENEER_FEED_BATT_PROFILE_FV_VOTER,    /* uV, protection-batvolt.c */
	VENEER_FEED_POWER_NOW,
	VENEER_FEED_BSM_TTF,
	VENEER_FEED_BSM_ENABLE,
	VENEER_FEED_TTF,				//FIXED_ME
	VENEER_FEED_BATT_PSY_IBAT_NOW,
	VENEER_FEED_USB_RESISTANCE_ID,	//FIXED_ME
	VENEER_FEED_TYPEC_MODE,
	VENEER_FEED_USB_PRESENT,
	VENEER_FEED_DEBUG_BATT,
	VENEER_FEED_USB_TYPE,
	VENEER_FEED_CHARGING,
	VENEER_FEED_FAKE_HVDCP,
	VENEER_FEED_CHARGER_VERBOSE,
	VENEER_FEED_CHARGER_INCOMPATIBLE,
	VENEER_FEED_CHARGER_HIGH_SPEED,
	VENEER_FEED_SUPPORT_FASTPL,
	VENEER_FEED_USB_REAL_TYPE,
	VENEER_FEED_HVDCP_TYPE,
	VENEER_FEED_FLOATED,
	VENEER_FEED_USB3P0,
	VENEER_FEED_DC_PRESENT,
	VENEER_FEED_FORCE_UPDATE,
	VENEER_FEED_CP_ENABLED,
	VENEER_FEED_USB_POWER_NOW,
	VENEER_FEED_DC_POWER_NOW,
	VENEER_FEED_PSEUDO_HEALTH,
	VENEER_FEED_MINCAP,
	VENEER_FEED_FULLRAW,
	VENEER_FEED_CHARING_SHOWCASE,
	VENEER_FEED_CHARGE_STATUS,
	VENEER_FEED_USB_AICL,
	VENEER_FEED_OCV,
	VENEER_FEED_FAKE_ONLINE,
	VENEER_FEED_DC_VOLTAGE_MAX_DESIGN,
	VENEER_FEED_DC_VOLTAGE_NOW,
	VENEER_FEED_DC_CURRENT_NOW,
	VENEER_FEED_WLS_PRESENT,
	VENEER_FEED_WLS_CHARGE_PAUSE,
	VENEER_FEED_HVT_SOC_RESCALE_COUNT,

	VENEER_FEED_VENEER_BACKUP_LOAD,
	VENEER_FEED_VENEER_BACKUP_ID,
	VENEER_FEED_VENEER_BACKUP_DATA,
	VENEER_FEED_VENEER_BACKUP_FLUSH,
	VENEER_FEED_SENSOR_BATT_RAW,
	VENEER_FEED_DC_FORCE_UPDATE,
	VENEER_FEED_MAX,
};

enum unified_node_id {
	UNIFIED_NODE_ACTM_MODE = 0,
	UNIFIED_NODE_ACTM_LCDON_OFFSET,
	UNIFIED_NODE_ACTM_SENSOR_WIRED,
	UNIFIED_NODE_ACTM_SENSOR_WIRELESS,
	UNIFIED_NODE_ACTM_HOLDDEG_WIRED_0,
	UNIFIED_NODE_ACTM_HOLDDEG_WIRED_1,
	UNIFIED_NODE_ACTM_HOLDDEG_WIRED_2,
	UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_0,
	UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_1,
	UNIFIED_NODE_ACTM_HOLDDEG_WIRELESS_2,
	UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_0,
	UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_1,
	UNIFIED_NODE_ACTM_TEMPOFFS_WIRED_2,
	UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_0,
	UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_1,
	UNIFIED_NODE_ACTM_TEMPOFFS_WIRELESS_2,
	UNIFIED_NODE_ACTM_CURRENT_WIRED_0,
	UNIFIED_NODE_ACTM_CURRENT_WIRED_1,
	UNIFIED_NODE_ACTM_CURRENT_WIRED_2,
	UNIFIED_NODE_ACTM_POWER_EPP_0,
	UNIFIED_NODE_ACTM_POWER_EPP_1,
	UNIFIED_NODE_ACTM_POWER_EPP_2,
	UNIFIED_NODE_ACTM_POWER_BPP_0,
	UNIFIED_NODE_ACTM_POWER_BPP_1,
	UNIFIED_NODE_ACTM_POWER_BPP_2,
	UNIFIED_NODE_ACTM_CURR_CP_PPS,
	UNIFIED_NODE_ACTM_CURR_CP_QC30,
	UNIFIED_NODE_ACTM_MAX_FCC_PPS,
	UNIFIED_NODE_ACTM_MAX_FCC_QC3,
	UNIFIED_NODE_ACTM_MAX_FCC_QC2,
	MAX_UNIFIED_NODES_SIZE,

	UNIFIED_NODE_IRC_ENABLED = MAX_UNIFIED_NODES_SIZE,
	UNIFIED_NODE_IRC_RESISTANCE,
	UNIFIED_NODE_THERMALD_IUSB,
	UNIFIED_NODE_THERMALD_IBAT,
	UNIFIED_NODE_THERMALD_IDC,
	UNIFIED_NODE_THERMALD_VDC,
	UNIFIED_NODE_STATUS_BOOT,
	UNIFIED_NODE_STATUS_LCD,
	UNIFIED_NODE_CHARGING_RESTRICTION,
	UNIFIED_NODE_CHARGING_ENABLE,
	UNIFIED_NODE_CHARGING_STEP,
	UNIFIED_NODE_TIME_TO_FULL_NOW,
	UNIFIED_NODE_TTF_CAPACITY,
	UNIFIED_NODE_AICL_DONE,
	UNIFIED_NODE_CCD_ICL,
	UNIFIED_NODE_CCD_FCC,
	UNIFIED_NODE_CCD_VFLOAT,
	UNIFIED_NODE_CCD_INPUT_SUSPEND,
	UNIFIED_NODE_CCD_BATCHG_EN,
	UNIFIED_NODE_CHARGING_SHOWCASE,
	UNIFIED_NODE_CHARGING_COMPLETED,
	UNIFIED_NODE_FAKE_BATTERY,
	UNIFIED_NODE_FAKE_BATTERY_TEMP,
	UNIFIED_NODE_FAKE_BATTERY_SOC,
	UNIFIED_NODE_FAKE_BATTERY_VOLT,
	UNIFIED_NODE_FAKE_BATTERY_TEMP_EN,
	UNIFIED_NODE_FAKE_BATTERY_SOC_EN,
	UNIFIED_NODE_FAKE_BATTERY_VOLT_EN,
	UNIFIED_NODE_FAKE_SDPMAX,
	UNIFIED_NODE_FAKE_HVDCP,
	UNIFIED_NODE_BATTERY_AGE,
	UNIFIED_NODE_BATTERY_CONDITION,
	UNIFIED_NODE_BATTERY_CYCLE,
	UNIFIED_NODE_BATTERY_VALID,
	UNIFIED_NODE_CHARGER_NAME,
	UNIFIED_NODE_CHARGER_HIGHSPEED,
	UNIFIED_NODE_CHARGER_INCOMPATIBLE,
	UNIFIED_NODE_CHARGER_USBID,
	UNIFIED_NODE_CHARGER_VERBOSE,
	UNIFIED_NODE_SUPPORT_FASTPL,
	UNIFIED_NODE_SUPPORT_FASTCHG,
	UNIFIED_NODE_BSM_TIMETOFULL,
	UNIFIED_NODE_BSM_ENABLE,
	UNIFIED_NODE_FACTORY_XO_THERM,
	UNIFIED_NODE_FACTORY_BD_THERM,
	UNIFIED_NODE_USB_REAL_TYPE,
	UNIFIED_NODE_SAFETY_TIMER_ENABLED,
	UNIFIED_NODE_INPUT_SUSPEND,
	UNIFIED_NODE_INPUT_CURRENT_SETTLED,
	UNIFIED_NODE_RESISTANCE_ID,
	UNIFIED_NODE_BATTERY_CHARGING_ENABLED,
	UNIFIED_NODE_PARALLEL_ENABLED,
	UNIFIED_NODE_AC_ONLINE,
	UNIFIED_NODE_USB_ONLINE,
	UNIFIED_NODE_TYPEC_CC_ORIENTATION,
	UNIFIED_NODE_TYPEC_RAW_CAPACITY,
	UNIFIED_NODE_TYPEC_MOISTURE_EN,
	UNIFIED_NODE_UPPER_PCB,
	UNIFIED_NODE_LOW_PCB,

	UNIFIED_NODE_MAX,
};

enum veneer_changed_prop {
	CP_CHARGE_STATUS,
	CP_CAPACITY,
	CP_VOLTAGE_NOW,
	CP_BAT_TEMP,
	CP_USB_PRESENT,
	CP_USB_AICL,
	CP_USB_TYPE,
	CP_TYPEC_MODE,
	CP_USB_POWER,
	CP_WLS_PRESENT,
};

enum veneer_feed_type {
	PSY_BATT,
	PSY_USB,
	PSY_WLS,
	PSY_CP,
	PSY_DC,
	UNI_NODE,
	VENEER,
	OTHER,
};

#define VENEER_BACKUP_MAX_COUNT 50
typedef struct {
  uint32_t cookie[4];
  uint32_t is_good_cookie;
  uint32_t data[VENEER_BACKUP_MAX_COUNT];
} veneer_backup_type;

enum veneer_backup_id {
	VENEER_BACKUP_ID_HVT_COUNT = 0,
	VENEER_BACKUP_ID_HVT_UI_SOC,
	VENEER_BACKUP_ID_HVT_BACKUP_TIME,
	VENEER_BACKUP_ID_CBC_CYCLE,
	VENEER_BACKUP_ID_MAX = VENEER_BACKUP_MAX_COUNT,
};

int lge_get_veneer_backup(void *data);
int lge_set_veneer_backup(void *data);

struct voter_entry {
/* static values being set with registering */
	char name[VOTER_NAME_LENGTH];
	enum voter_type		type;
	bool fakeui;	// Interfering UI when it votes 'suspend' (assigning '0' to limit)

/* runtime value with voting */
	int	limit;		// in mA
	bool effecting;	// in the veneer domain, not overall system.

/* private members controlled by voter lib. */
	int	id;
	struct list_head node;
};


static inline
const char* vote_title(enum voter_type type)
{
	switch (type) {
		case VOTER_TYPE_IUSB:	return "iusb";
		case VOTER_TYPE_IBAT:	return "ibat";
		case VOTER_TYPE_IDC:	return "idc";
		case VOTER_TYPE_VFLOAT:	return "vfloat";
		case VOTER_TYPE_ACTM_LCDON_TEMP_OFFSET:	return "actm";

		default: return NULL;
	}
}

static inline
enum voter_type vote_fromdt(const char* string)
{
	if (!strcmp(string, "iusb"))
		return VOTER_TYPE_IUSB;
	if (!strcmp(string, "ibat"))
		return VOTER_TYPE_IBAT;
	if (!strcmp(string, "idc"))
		return VOTER_TYPE_IDC;
	if (!strcmp(string, "vfloat"))
		return VOTER_TYPE_VFLOAT;
	if (!strcmp(string, "actm"))
		return VOTER_TYPE_ACTM_LCDON_TEMP_OFFSET;

	return VOTER_TYPE_INVALID;
}

enum charging_supplier { // Exclusive charging types
	CHARGING_SUPPLY_TYPE_UNKNOWN = 0,
	CHARGING_SUPPLY_TYPE_FLOAT,
	CHARGING_SUPPLY_TYPE_NONE,

	CHARGING_SUPPLY_DCP_DEFAULT,
	CHARGING_SUPPLY_DCP_10K,
	CHARGING_SUPPLY_DCP_22K,
	CHARGING_SUPPLY_DCP_QC2,
	CHARGING_SUPPLY_DCP_QC3,

	CHARGING_SUPPLY_USB_2P0,
	CHARGING_SUPPLY_USB_3PX,
	CHARGING_SUPPLY_USB_CDP,
	CHARGING_SUPPLY_USB_PD,
	CHARGING_SUPPLY_USB_PD_PPS,

	CHARGING_SUPPLY_FACTORY_56K,
	CHARGING_SUPPLY_FACTORY_130K,
	CHARGING_SUPPLY_FACTORY_910K,

	CHARGING_SUPPLY_WIRELESS_5W,
	CHARGING_SUPPLY_WIRELESS_9W,
	CHARGING_SUPPLY_WIRELESS_15W,
	CHARGING_SUPPLY_MAX,
};

enum charging_step {
	CHARGING_STEP_TRICKLE,
	CHARGING_STEP_CC,
	CHARGING_STEP_CV,
	CHARGING_STEP_TERMINATED,
	CHARGING_STEP_NOTCHARGING,
	CHARGING_STEP_DISCHARGING,
};

enum charging_verbosity {
	VERBOSE_CHARGER_NONE,
	VERBOSE_CHARGER_NORMAL,
	VERBOSE_CHARGER_INCOMPATIBLE,
	VERBOSE_CHARGER_SLOW,
};

enum charging_suspended {
	CHARGING_NOT_SUSPENDED,
	CHARGING_SUSPENDED_WITH_ONLINE,
	CHARGING_SUSPENDED_WITH_FAKE_OFFLINE,
};

#define MAX_CHARGER_USBID 4
enum charger_usbid {
	CHARGER_USBID_INVALID	= INT_MIN,	// No init
	CHARGER_USBID_UNKNOWN	=   -1000,	// Out of range
	CHARGER_USBID_56KOHM	=   56000,
	CHARGER_USBID_130KOHM	=  130000,
	CHARGER_USBID_910KOHM	=  910000,
	CHARGER_USBID_OPEN	= INT_MAX,
};

enum typec_mode {
	USB_TYPEC_NONE,

	/* Acting as source */
	USB_TYPEC_SINK,					/* Rd only */
	USB_TYPEC_SINK_POWERED_CABLE,	/* Rd/Ra */
	USB_TYPEC_SINK_DEBUG_ACCESSORY,	/* Rd/Rd */
	USB_TYPEC_SINK_AUDIO_ADAPTER,	/* Ra/Ra */
	USB_TYPEC_POWERED_CABLE_ONLY,	/* Ra only */

	/* Acting as sink */
	USB_TYPEC_SOURCE_DEFAULT,	/* Rp default */
	USB_TYPEC_SOURCE_MEDIUM,	/* Rp 1.5A */
	USB_TYPEC_SOURCE_HIGH,		/* Rp 3A */

	USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT,			/* Rp default / Rp default */
	USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT_MEDIUM,	/* Rp default / Rp 1.5A */
	USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT_HIGH,		/* Rp default / Rp 3A */
	USB_TYPEC_SOURCE_DEBUG_ACCESSORY_MEDIUM,			/* Rp 1.5A / Rp 1.5A */
	USB_TYPEC_SOURCE_DEBUG_ACCESSORY_MEDIUM_HIGH,		/* Rp 1.5A / Rp 3A */
	USB_TYPEC_SOURCE_DEBUG_ACCESSORY_HIGH,				/* Rp 3A / Rp 3A */

	USB_TYPEC_NON_COMPLIANT,
};

enum charge_status {
	CHARGE_STATUS_INHIBIT,
	CHARGE_STATUS_TRICKLE,
	CHARGE_STATUS_PRECHARGE,
	CHARGE_STATUS_FULLON,
	CHARGE_STATUS_TAPER,
	CHARGE_STATUS_TERMINATION,
	CHARGE_STATUS_PAUSE,
	CHARGE_STATUS_CHARGING_DISABLED,
	CHARGE_STATUS_INVALID,
};

static inline
const char* charge_status_name(enum charge_status status)
{
	switch(status) {
	case CHARGE_STATUS_INHIBIT :	return "Inhibit";
	case CHARGE_STATUS_TRICKLE :	return "Trickle";
	case CHARGE_STATUS_PRECHARGE :	return "Precharge";
	case CHARGE_STATUS_FULLON :	return "Fullon";
	case CHARGE_STATUS_TAPER :	return "Taper";
	case CHARGE_STATUS_TERMINATION :	return "Termination";
	case CHARGE_STATUS_PAUSE :	return "Pause";
	case CHARGE_STATUS_CHARGING_DISABLED :	return "Disabled";
	case CHARGE_STATUS_INVALID :	return "Invalid";
	default :
		break;
	}

	return "Invalid";
}

static inline
const char* usb_type_name(enum power_supply_usb_type type)
{
	switch(type) {
	case POWER_SUPPLY_USB_TYPE_UNKNOWN :	return "UNKNOWN";
	case POWER_SUPPLY_USB_TYPE_SDP :	return "SDP";
	case POWER_SUPPLY_USB_TYPE_DCP :	return "DCP";
	case POWER_SUPPLY_USB_TYPE_CDP :	return "CDP";
	case POWER_SUPPLY_USB_TYPE_ACA :	return "ACA";
	case POWER_SUPPLY_USB_TYPE_C :	return "TYPE_C";
	case POWER_SUPPLY_USB_TYPE_PD :	return "PD";
	case POWER_SUPPLY_USB_TYPE_PD_DRP :	return "PD_DRP";
	case POWER_SUPPLY_USB_TYPE_PD_PPS :	return "PD_PPS";
	case POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID :	return "ABI";

	default :
		break;
	}

	return "Invalid";
}

static inline
const char* typec_mode_name(enum typec_mode status)
{
	switch(status) {
	case USB_TYPEC_NONE :	return "None";
	case USB_TYPEC_SINK :	return "Rd-Open";
	case USB_TYPEC_SINK_POWERED_CABLE :	return "Rd-Ra";
	case USB_TYPEC_SINK_DEBUG_ACCESSORY :	return "Rd-Rd";
	case USB_TYPEC_SINK_AUDIO_ADAPTER :	return "Ra-Ra";
	case USB_TYPEC_POWERED_CABLE_ONLY :	return "Ra-Only";
	case USB_TYPEC_SOURCE_DEFAULT :	return "Rp56k";
	case USB_TYPEC_SOURCE_MEDIUM :	return "Rp22k";
	case USB_TYPEC_SOURCE_HIGH :	return "Rp10k";
	case USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT :	return "Rp-Rp";
	case USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT_MEDIUM :	return "Rp-Rp22k";
	case USB_TYPEC_SOURCE_DEBUG_ACCESSORY_DEFAULT_HIGH :	return "Rp-Rp10k";
	case USB_TYPEC_SOURCE_DEBUG_ACCESSORY_MEDIUM :	return "Rp22k-Rp22k";
	case USB_TYPEC_SOURCE_DEBUG_ACCESSORY_MEDIUM_HIGH :	return "Rp22k-Rp10k";
	case USB_TYPEC_SOURCE_DEBUG_ACCESSORY_HIGH :	return "Rp10k-Rp10k";

	case USB_TYPEC_NON_COMPLIANT :	return "Non-compliant";
	default :
		break;
	}

	return "Invalid";
}

#define QC20 1
#define QC30 2
enum charger_type {
    CHARGER_TYPE_NONE,
    CHARGER_TYPE_UNKNOWN,
    CHARGER_TYPE_SNK_USB_SDP,
    CHARGER_TYPE_SNK_USB_OCP,
    CHARGER_TYPE_SNK_USB_CDP,
    CHARGER_TYPE_SNK_USB_DCP,
    CHARGER_TYPE_SNK_USB_FLOAT,
    CHARGER_TYPE_SNK_TYPEC_DEFAULT,
    CHARGER_TYPE_SNK_TYPEC_RP_MEDIUM_1P5A,
    CHARGER_TYPE_SNK_TYPEC_RP_HIGH_3A,
    CHARGER_TYPE_SNK_DEBUG_ACCESS,
    CHARGER_TYPE_SNK_USB_QC_2P0,
    CHARGER_TYPE_SNK_USB_QC_3P0,
    CHARGER_TYPE_SNK_USB_QC_3P5,
    CHARGER_TYPE_SNK_PD,
    CHARGER_TYPE_SNK_PPS,
    CHARGER_TYPE_SRC_TYPEC_POWERCABLE,                 //RD-RA
    CHARGER_TYPE_SRC_TYPEC_UNORIENTED_DEBUG_ACCESS,    //RD/RD
    CHARGER_TYPE_SRC_TYPEC_AUDIO_ACCESS,               //RA/RA
    CHARGER_TYPE_WLS_SRC_BPP,
    CHARGER_TYPE_WLS_SNK_BPP,
    CHARGER_TYPE_WLS_SNK_EPP,
    CHARGER_TYPE_WLS_SNK_PDDE,
    CHARGER_TYPE_INVALID,
};

static inline
bool charger_fabcable(enum charging_supplier charger)
{
	switch(charger) {
	case CHARGING_SUPPLY_FACTORY_56K :
	case CHARGING_SUPPLY_FACTORY_130K :
	case CHARGING_SUPPLY_FACTORY_910K :
		return true;
	default :
		break;
	}

	return false;
}

static inline
bool charger_dcptype(enum charging_supplier charger)
{
	switch(charger) {
	case CHARGING_SUPPLY_DCP_DEFAULT :
	case CHARGING_SUPPLY_DCP_10K :
	case CHARGING_SUPPLY_DCP_22K :
		return true;
	default :
		break;
	}

	return false;
}

static inline
const char* charger_name(enum charging_supplier charger)
{
	switch(charger) {

	case CHARGING_SUPPLY_TYPE_UNKNOWN :	return "UNKNOWN";
	case CHARGING_SUPPLY_TYPE_FLOAT :	return "FLOAT";
	case CHARGING_SUPPLY_TYPE_NONE :	return "NONE";

	case CHARGING_SUPPLY_DCP_DEFAULT :	return "DCP";
	case CHARGING_SUPPLY_DCP_10K :	return "10K";
	case CHARGING_SUPPLY_DCP_22K :	return "22K";
	case CHARGING_SUPPLY_DCP_QC2 :	return "QC2";
	case CHARGING_SUPPLY_DCP_QC3 :	return "QC3";

	case CHARGING_SUPPLY_USB_2P0 :	return "USB2";
	case CHARGING_SUPPLY_USB_3PX :	return "USB3";
	case CHARGING_SUPPLY_USB_CDP :	return "CDP";
	case CHARGING_SUPPLY_USB_PD :	return "PD";
	case CHARGING_SUPPLY_USB_PD_PPS :	return "PPS";

	case CHARGING_SUPPLY_FACTORY_56K :	return "56K";
	case CHARGING_SUPPLY_FACTORY_130K :	return "130K";
	case CHARGING_SUPPLY_FACTORY_910K :	return "910K";

	case CHARGING_SUPPLY_WIRELESS_5W :	return "W5W";
	case CHARGING_SUPPLY_WIRELESS_9W :	return "W9W";
	case CHARGING_SUPPLY_WIRELESS_15W :	return "W15W";

	default :
		break;
	}

	return "ERROR";
}

static inline
bool veneer_extension_pspoverlap(
	enum power_supply_property* ori_arr, int ori_cnt,
	enum power_supply_property* ext_arr, int ext_cnt
){
	int i, j;

	for (i=0; i<ori_cnt; ++i) {
		for (j=0; j<ext_cnt; ++j) {
			if (ori_arr[i] == ext_arr[j]) {
				pr_err("VENEER: %d is overlapped in %s\n",
					ext_arr[j], __func__);

				return true; // Duplicated declaration in extended psy
			}
		}
	}

	return false; // means OK (not overlapped)
}

enum veneer_exception { // Can be accumulated to the 'int' storage
	// for battery			0x000?
	EXCEPTION_BATTERY_VBATLOW	= BIT(1),
	EXCEPTION_BATTERY_VBATOVER	= BIT(1),
	EXCEPTION_BATTERY_TEMPOVER	= BIT(2),
	EXCEPTION_BATTERY_MISSING	= BIT(3),
	// for wired charger		0x00?0
	EXCEPTION_WIRED_INCOMPATIBALE	= BIT(4),
	EXCEPTION_WIRED_VBUSOVER	= BIT(5),
	EXCEPTION_WIRED_IBUSOVER	= BIT(6),
	EXCEPTION_WIRED_VCCOVER		= BIT(7),
	// for wireless charger		0x0?00
	EXCEPTION_WLC_OVERCURRENT	= BIT(8),
	EXCEPTION_WLC_OVERVOLTAGE	= BIT(9),
	EXCEPTION_WLC_OVERTEMPERATURE	= BIT(10),
	// on during charging		0x?000
	EXCEPTION_CHARGING_TIMEOUT	= BIT(12),
	EXCEPTION_CHARGING_AICLFAIL	= BIT(13),
};

enum veneer_bootmode {
	BOOTMODE_ANDROID_NORMAL,
	BOOTMODE_ANDROID_56K,
	BOOTMODE_ANDROID_130K,
	BOOTMODE_ANDROID_910K,
	BOOTMODE_MINIOS_AAT,
	BOOTMODE_MINIOS_56K,
	BOOTMODE_MINIOS_130K,
	BOOTMODE_MINIOS_910K,
	BOOTMODE_LAF_NORMAL,
	BOOTMODE_LAF_910K,
	BOOTMODE_ETC_CHARGERLOGO,
	BOOTMODE_ETC_RECOVERY,
	BOOTMODE_ETC_UNKNOWN,
};

enum veneer_logmask {
	ERROR		= BIT(0),
	RETURN		= BIT(1),
	UPDATE		= BIT(2),
	ASSERT		= BIT(3),
	MONITOR		= BIT(4),
	EVALUATE	= BIT(5),
	INTERRUPT	= BIT(6),

	VERBOSE		= BIT(7),
};

struct __veneer_dt {
	/* protection_batvolt */
	bool irc_enable;
	int irc_mohm;
	int irc_threshold_ma;
	int vbat_limit_one_c_rate;
};

struct veneer {
/* module descripters */
	struct device*          veneer_dev;
	struct power_supply*    veneer_psy;
	struct wakeup_source*    veneer_wakelock;
	enum charging_supplier  veneer_supplier;

	struct __veneer_dt dt;

	int veneer_exception;
	// delayed works
	struct delayed_work	dwork_logger;	// for logging
	struct delayed_work	dwork_slowchg;	// for charging-verbosity
	struct work_struct	work_exchagned;	// for external_changed

	// battery profiles
	int			profile_mincap;
	int			profile_fullraw;
	int			profile_mvfloat;
	// highspeed threshold
	int			threshold_wired;
	int			threshold_wireless;
	int			threshold_wireless_15w;

/* shadow states used for cache */
	// charger states
	enum power_supply_usb_type	usbin_realtype;
	bool			usbin_typefix;
	int			usbin_aicl;
	int 		usbin_power;
	int			usb_typec_mode;
	bool			presence_otg;
	int				presence_usb;
	int			presence_wireless;
	// battery states
	bool		battery_eoc;
	int			battery_capacity;
	int			battery_voltage;
	int			battery_temperature;
	int			battery_temp_raw;
	int			charge_status;
	// pseudo values
	int			pseudo_status;
	int			pseudo_health;

/* power supplies */
	struct power_supply*	psy_cp;
	struct power_supply*	psy_battery;
	struct power_supply*	psy_usb;
	struct power_supply*	psy_wireless;
	struct power_supply*	psy_dc;

	bool pm_qos_flag;
	struct pm_qos_request pm_qos;
	struct mutex veneer_lock;

	int	changed_prop;
};

struct veneer* veneer_data_fromair(void);
struct voter_entry* veneer_voter_search(enum voter_type type, const char* name);
bool veneer_voter_enabled_fake_ui(enum voter_type type);
enum charging_suspended veneer_voter_suspended(enum voter_type type);
bool veneer_voter_changed(enum voter_type type);
void veneer_voter_passover(enum voter_type type, int limit, bool enable);
bool veneer_voter_effecting(struct voter_entry* voter);
bool veneer_voter_enabled(struct voter_entry* voter);
void veneer_voter_set(struct voter_entry* voter, int limit);
void veneer_voter_rerun(struct voter_entry* voter);
void veneer_voter_release(struct voter_entry* voter);
void veneer_voter_unregister(struct voter_entry* entry);
bool veneer_voter_register(
	struct voter_entry* entry,
	const char* name, enum voter_type type, bool fakeui);
void veneer_voter_destroy(void);
bool veneer_voter_create(void);

bool charging_ceiling_vote(enum charging_supplier charger);
bool charging_ceiling_sdpmax(bool enable);
void charging_ceiling_destroy(void);
bool charging_ceiling_create(struct device_node* dnode);

int  charging_time_remains(void);
void charging_time_clear(void);
void charging_time_destroy(void);
bool charging_time_update(void);
bool charging_time_create(struct device_node* dnode);

void protection_battemp_monitor(void);
void protection_battemp_destroy(void);
bool protection_battemp_create(struct device_node* dnode);

void protection_batvolt_refresh(bool charging);
void protection_batvolt_destroy(void);
bool protection_batvolt_create(struct device_node* dnode);

void protection_showcase_update(void);
void protection_showcase_destroy(void);
bool protection_showcase_create(struct device_node* dnode);

void actm_trigger(void);
void actm_destroy(void);
bool actm_create(struct device_node* dnode);

enum veneer_bootmode unified_bootmode_type(void);
enum charger_usbid unified_bootmode_usbid(void);
const char* unified_bootmode_operator(void);
const char* unified_bootmode_region(void);
const char* unified_bootmode_marker(void);
bool unified_bootmode_chargerlogo(void);
bool unified_bootmode_chgverbose(void);
bool unified_bootmode_fabproc(void);
bool unified_bootmode_usermode(void);
void unified_bootmode_get_property_operator(const char* s, char* property);
void unified_bootmode_get_property_region(const char* s, char* property);

int unified_bootmode_batteryid(void);

bool unified_nodes_show(const char* key, char* value);
bool unified_nodes_store(const char* key, const char* value, size_t size);
bool unified_nodes_write(enum unified_node_id uni, int data);
bool unified_nodes_read(enum unified_node_id uni, int *data);
void unified_nodes_destroy(void);
bool unified_nodes_create(struct device_node* dnode);

void unified_sysfs_destroy(void);
bool unified_sysfs_create(struct device_node* dnode);

int get_veneer_param(int id, int *val);
int set_veneer_param(int id, int val);
int get_latest_veneer_param(int id, int *val);

int get_rtc_time(unsigned long *rtc_time);

extern int set_veneer_backup_cbc_cycle(int value);
extern void get_veneer_backup_cbc_cycle(int *value);

struct power_supply* get_psy_cp(struct veneer* veneer_me);
struct power_supply* get_psy_battery(struct veneer* veneer_me);
struct power_supply* get_psy_usb(struct veneer* veneer_me);
struct power_supply* get_psy_wireless(struct veneer* veneer_me);
struct power_supply* get_psy_dc(struct veneer* veneer_me);
#endif
