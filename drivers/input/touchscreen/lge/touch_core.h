/*
 * touch_core.h
 *
 * Copyright (c) 2015 LGE.
 *
 * author : hoyeon.jang@lge.com
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef LGE_TOUCH_CORE_H
#define LGE_TOUCH_CORE_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#if IS_ENABLED(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#elif IS_ENABLED(CONFIG_DRM_MSM) && IS_ENABLED(CONFIG_FB)
#include <linux/msm_drm_notify.h>
#elif IS_ENABLED(CONFIG_FB)
#include <linux/fb.h>
#endif
#include <linux/notifier.h>
#include <linux/atomic.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include "touch_hwif.h"
#include <linux/input/lge_touch_notify.h>
#include <linux/input/lge_touch_module.h>
#if IS_ENABLED(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
#include <linux/lge_panel_notify.h>
#endif

#if IS_ENABLED(CONFIG_SECURE_TOUCH)
#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_DEX) || IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
#include "touch_dex.h"
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/current.h>
#endif
#include "touch_common.h"
#ifdef CONFIG_LGE_TOUCH_PEN
#include <touch_pen.h>
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
#include "touch_hidden.h"
#endif

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
#include <linux/input/tui_hal_ts.h>
#endif
#define LGE_TOUCH_NAME			"lge_touch"
#define LGE_TOUCH_DRIVER_NAME	"lge_touch_driver"

#define MAX_FINGER				10
#define TAP_COUNT_ONETAP		1
#define TAP_COUNT_KNOCK_ON		2
#define MAX_TAP_CNT 			(TAP_COUNT_KNOCK_ON + 1)
#define EUPGRADE				140
#define EHWRESET_ASYNC			141
#define EHWRESET_SYNC			142
#define ESWRESET				143
#define EGLOBALRESET			144

#define UEVENT_TIMEOUT			100
#define UEVENT_MSG_LEN			2048

enum TOUCH_DEBUG {
	_NONE                      = 0,
	BASE_INFO                 = (1U << 0),    /* 1 */
	TRACE                     = (1U << 1),    /* 2 */
	GET_DATA                  = (1U << 2),    /* 4 */
	ABS                       = (1U << 3),    /* 8 */
	BUTTON                    = (1U << 4),    /* 16*/
	FW_UPGRADE                = (1U << 5),    /* 32 */
	GHOST                     = (1U << 6),    /* 64 */
	IRQ_HANDLE                = (1U << 7),    /* 128 */
	POWER                     = (1U << 8),    /* 256 */
	JITTER                    = (1U << 9),    /* 512 */
	ACCURACY                  = (1U << 10),   /* 1024 */
	BOUNCING                  = (1U << 11),   /* 2048 */
	GRIP                      = (1U << 12),   /* 4096 */
	FILTER_RESULT             = (1U << 13),   /* 8192 */
	QUICKCOVER                = (1U << 12),   /* 4096 */
	LPWG                      = (1U << 14),   /* 16384 */
	NOISE                     = (1U << 15),   /* 32768 */
	LPWG_COORDINATES          = (1U << 16),   /* 65536 */
};

#define TOUCH_I(fmt, args...)					\
	pr_info("[Touch] "					\
			fmt, ##args)

#define TOUCH_E(fmt, args...)					\
	pr_err("[Touch E] [%s %d] "				\
			fmt, __func__, __LINE__, ##args)

#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
#define TOUCH_HIDDEN(fmt, args...)					\
	pr_info("[HIDDEN_COORDI] "					\
			fmt, ##args)
#endif

#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
#include <linux/trace_events.h>
#define touch_trace_begin(fmt, args...) do { \
	preempt_disable(); \
	event_trace_printk(get_tracing_mark(), \
			"B|%d|"fmt"\n", current->tgid, ##args); \
	preempt_enable();\
} while (0)

#define touch_trace_end(fmt, args...) do { \
	preempt_disable(); \
	event_trace_printk(get_tracing_mark(), \
			"E|%d|"fmt"\n", current->tgid, ##args); \
	preempt_enable();\
} while (0)

unsigned long get_tracing_mark(void);
#endif
extern u32 touch_debug_mask;
#define TOUCH_D(condition, fmt, args...)			\
	do {							\
		if (unlikely(touch_debug_mask & (condition)))	\
			pr_info("[Touch] " fmt, ##args);	\
	} while (0)

#define TOUCH_DEBUG_SHOW_FILE
#ifdef TOUCH_DEBUG_SHOW_FILE
#define __SHORT_FILE__ (strrchr(__FILE__, '/') + 1)
#define TOUCH_TRACE()	TOUCH_D(TRACE, "- %s(%s) %d\n",		\
		__func__, __SHORT_FILE__, __LINE__)
#else
#define TOUCH_TRACE()	TOUCH_D(TRACE, "- %s %d", __func__, __LINE__)
#endif

#define TOUCH_IRQ_NONE						0
#define TOUCH_IRQ_FINGER					(1 << 0)
#define TOUCH_IRQ_KNOCK						(1 << 1)
#define TOUCH_IRQ_PASSWD					(1 << 2)
#define TOUCH_IRQ_SWIPE_DOWN				(1 << 3)
#define TOUCH_IRQ_LG_PAY_SWIPE_UP			(1 << 4)
#define TOUCH_IRQ_LG_PAY_SWIPE_LEFT			(1 << 5)
#define TOUCH_IRQ_LG_PAY_SWIPE_RIGHT		(1 << 6)
#define TOUCH_IRQ_WATER_MODE_ON				(1 << 7)
#define TOUCH_IRQ_WATER_MODE_OFF			(1 << 8)
#define TOUCH_IRQ_AI_BUTTON					(1 << 9)
#define TOUCH_IRQ_AI_PICK					(1 << 10)
#define TOUCH_IRQ_QUICK_TOOL_SWIPE_LEFT		(1 << 12)
#define TOUCH_IRQ_QUICK_TOOL_SWIPE_RIGHT	(1 << 13)
#define TOUCH_IRQ_ERROR						(1 << 15)
#define TOUCH_IRQ_LONGPRESS_DOWN			(1 << 16)
#define TOUCH_IRQ_LONGPRESS_UP				(1 << 17)
#define TOUCH_IRQ_SINGLE_WAKEUP				(1 << 18)
#define TOUCH_IRQ_PEN_WAKEUP				(1 << 19)
#define TOUCH_IRQ_PEN_WAKEUP_BTN			(1 << 20)
#define TOUCH_IRQ_PEN_DETECTION				(1 << 21)
#define TOUCH_IRQ_FAKE_OFF_SWIPE			(1 << 22)
#define TOUCH_IRQ_FAKE_OFF_SINGLE_WAKEUP	(1 << 23)


enum {
	PORTRAIT_A = 0,
	PORTRAIT_B,
	PORTRAIT_C,
	LANDSCAPE_VERTICAL_A,
	LANDSCAPE_VERTICAL_B,
	LANDSCAPE_VERTICAL_C,
	LANDSCAPE_HORIZONTAL_A,
	LANDSCAPE_HORIZONTAL_B,
	IGNORE_LG_PAY,
	IGNORE_SIDE_PANEL,
	IGNORE_QMEMO_PANEL,
	IGNORE_DS_TOOL,
	GRIP_AREA_NUM,
};

enum {
	POWER_OFF = 0,
	POWER_SLEEP,
	POWER_WAKE,
	POWER_ON,
	POWER_HW_RESET_ASYNC,
	POWER_HW_RESET_SYNC,
	POWER_SW_RESET,
	POWER_DSV_TOGGLE,
	POWER_DSV_ALWAYS_ON,
};

enum {
	UEVENT_IDLE = 0,
	UEVENT_BUSY,
};

enum {
	SCREEN_OFF = 0,
	SCREEN_ON,
	SCREEN_FAKE_OFF,
};

enum {
	LPWG_NONE = 0,
	LPWG_DOUBLE_TAP,
	LPWG_PASSWORD,
	LPWG_PASSWORD_ONLY,
};

enum {
	PROX_NEAR = 0,
	PROX_FAR,
};

enum {
	HALL_FAR = 0,
	HALL_NEAR,
};

enum {
	DEV_PM_RESUME = 0,
	DEV_PM_SUSPEND,
	DEV_PM_SUSPEND_IRQ,
};

enum {
	FB_RESUME = 0,
	FB_CHANGED,
	FB_SUSPEND,
};

enum {
	SP_DISCONNECT = 0,
	SP_CONNECT,
};

/* Deep Sleep or not */
enum {
	IC_NORMAL = 0,
	IC_DEEP_SLEEP,
};

/* TCI */
enum {
	ENABLE_CTRL = 0,
	TAP_COUNT_CTRL,
	MIN_INTERTAP_CTRL,
	MAX_INTERTAP_CTRL,
	TOUCH_SLOP_CTRL,
	TAP_DISTANCE_CTRL,
	INTERRUPT_DELAY_CTRL,
	ACTIVE_AREA_CTRL,
	ACTIVE_AREA_RESET_CTRL,
};

enum {
	TCI_1 = 0,
	TCI_2,
};

enum {
	SWIPE_D = 0,
	SWIPE_U,
	SWIPE_R,
	SWIPE_L,
	SWIPE_R2,
	SWIPE_L2,
	SWIPE_DEFAULT,
};

enum {
	PAY_TYPE_DISABLE = 0,
	PAY_TYPE_SWIPE_U = 1,
	PAY_TYPE_SWIPE_L = 2,
	PAY_TYPE_SWIPE_R = 3,
};

enum {
	LG_PAY_SWIPE_UP = 0,
	LG_PAY_SWIPE_RIGHT,
	LG_PAY_SWIPE_LEFT,
	LG_PAY_SWIPE_DOWN,
	LG_PAY_SWIPE_MAX,
};

enum {
	QUICK_TOOL_SWIPE_RIGHT = 0,
	QUICK_TOOL_SWIPE_LEFT = 0,
	QUICK_TOOL_SWIPE_MAX,
};

enum {
	FAKE_OFF_SWIPE_UP = 0,
	FAKE_OFF_SWIPE_RIGHT,
	FAKE_OFF_SWIPE_LEFT,
	FAKE_OFF_SWIPE_DOWN,
	FAKE_OFF_SWIPE_MAX,
};

enum {
	LPWG_ENABLE = 1,
	LPWG_LCD,
	LPWG_ACTIVE_AREA,
	LPWG_TAP_COUNT,
	LPWG_LENGTH_BETWEEN_TAP,
	LPWG_EARLY_SUSPEND,
	LPWG_SENSOR_STATUS,
	LPWG_DOUBLE_TAP_CHECK,
	LPWG_UPDATE_ALL,
	LPWG_READ,
	LPWG_REPLY,
};

enum {
	LOCKSCREEN_UNLOCK = 0,
	LOCKSCREEN_LOCK,
};

enum {
	IME_OFF = 0,
	IME_ON,
	IME_SWYPE,
};

enum {
	QUICKCOVER_OPEN = 0,
	QUICKCOVER_CLOSE,
};

enum {
	INCOMING_CALL_IDLE,
	INCOMING_CALL_RINGING,
	INCOMING_CALL_OFFHOOK,
	INCOMING_CALL_CDMA_RINGING,
	INCOMING_CALL_CDMA_OFFHOOK,
	INCOMING_CALL_LTE_RINGING,
	INCOMING_CALL_LTE_OFFHOOK,
};

enum {
	MFTS_NONE = 0,
	MFTS_FOLDER,
	MFTS_FLAT,
	MFTS_CURVED,
	MFTS_DS_FLAT,
};

enum { /* Command lists */
	CMD_VERSION,
	CMD_ATCMD_VERSION,
};

enum {
	INTERRUPT_DISABLE = 0,
	INTERRUPT_ENABLE,
};

enum {
	CORE_NONE = 0,
	CORE_PROBE,
	CORE_UPGRADE,
	CORE_NORMAL,
	CORE_SHUTDOWN,
	CORE_REMOVE,
};

enum {
	EARJACK_NONE = 0,
	EARJACK_NORMAL,
	EARJACK_DEBUG,
};

enum {
	DEBUG_TOOL_DISABLE = 0,
	DEBUG_TOOL_ENABLE,
};

enum {
	DEBUG_OPTION_DISABLE = 0,
	DEBUG_OPTION_0 = (1 << 0),
	DEBUG_OPTION_1 = (1 << 1),
	DEBUG_OPTION_2 = (1 << 2),
	DEBUG_OPTION_3 = (1 << 3),
	DEBUG_OPTION_4 = (1 << 4),
	DEBUG_OPTION_5 = (1 << 5),
	DEBUG_OPTION_6 = (1 << 6),
	DEBUG_OPTION_7 = (1 << 7),
	DEBUG_OPTION_8 = (1 << 8),
	DEBUG_OPTION_9 = (1 << 9),
	DEBUG_OPTION_ALL = DEBUG_OPTION_0|DEBUG_OPTION_1|DEBUG_OPTION_2|
		DEBUG_OPTION_3|DEBUG_OPTION_4|DEBUG_OPTION_5|
		DEBUG_OPTION_6|DEBUG_OPTION_7|DEBUG_OPTION_8|DEBUG_OPTION_9,
};

enum {
	TOUCH_NORMAL_BOOT = 0,
	TOUCH_MINIOS_AAT,
	TOUCH_MINIOS_MFTS_FOLDER,
	TOUCH_MINIOS_MFTS_FLAT,
	TOUCH_MINIOS_MFTS_CURVED,
	TOUCH_MINIOS_MFTS_DS_FLAT,
	TOUCH_CHARGER_MODE,
	TOUCH_LAF_MODE,
	TOUCH_RECOVERY_MODE,
};

enum {
	TOUCH_UEVENT_KNOCK = 0,
	TOUCH_UEVENT_PASSWD,
	TOUCH_UEVENT_SWIPE_DOWN,
	TOUCH_UEVENT_SWIPE_UP,
	TOUCH_UEVENT_SWIPE_LEFT,
	TOUCH_UEVENT_SWIPE_RIGHT,
	TOUCH_UEVENT_WATER_MODE_ON,
	TOUCH_UEVENT_WATER_MODE_OFF,
	TOUCH_UEVENT_AI_BUTTON,
	TOUCH_UEVENT_AI_PICK,
	TOUCH_UEVENT_LONGPRESS_DOWN,
	TOUCH_UEVENT_LONGPRESS_UP,
	TOUCH_UEVENT_SINGLE_WAKEUP,
	TOUCH_UEVENT_PEN_WAKEUP,
	TOUCH_UEVENT_PEN_WAKEUP_BTN,
	TOUCH_UEVENT_PEN_DETECTION,
	TOUCH_UEVENT_SWITCH_AES_BOTH,
	TOUCH_UEVENT_SWITCH_AES_TO_1,
	TOUCH_UEVENT_SWITCH_AES_TO_2,
	TOUCH_UEVENT_DS_UPDATE_STATE,
	TOUCH_UEVENT_SWIPE,
	TOUCH_UEVENT_FAKE_OFF_SINGLE_WAKEUP,
	TOUCH_UEVENT_SIZE,
};

#define TOUCH_UEVENT_LG_PAY_SWIPE_UP			TOUCH_UEVENT_SWIPE_UP
#define TOUCH_UEVENT_LG_PAY_SWIPE_LEFT			TOUCH_UEVENT_SWIPE_UP
#define TOUCH_UEVENT_LG_PAY_SWIPE_RIGHT			TOUCH_UEVENT_SWIPE_UP
#define TOUCH_UEVENT_QUICK_TOOL_SWIPE_LEFT		TOUCH_UEVENT_SWIPE_LEFT
#define TOUCH_UEVENT_QUICK_TOOL_SWIPE_RIGHT		TOUCH_UEVENT_SWIPE_RIGHT
#define TOUCH_UEVENT_FAKE_OFF_SWIPE				TOUCH_UEVENT_SWIPE

enum {
	DS_UPDATE_NONE		= 0,
	DS_UPDATE_CONNECT	= 1,
	DS_UPDATE_CUSTOM	= 2,
};

enum {
	APP_HOME = 0,
	APP_CONTACTS,
	APP_MENU,
};

/* Support multiple display area */
enum {
	TYPE_DISPLAY_AREA = 0,
	TYPE_DISPLAY_AREA_INTERSPACE,
};

/* Real Display Area ID, matching with framework protocol */
enum {
	DISPLAY_AREA_ID_0 = 0,
	DISPLAY_AREA_ID_1,
	DISPLAY_AREA_ID_MAX,
};

/* Display space of between N and M area */
enum {
	DISPLAY_AREA_INTERSPACE_0_1 = 0,
	DISPLAY_AREA_INTERSPACE_ID_MAX,
};

/* LPWG Function list */
enum {
	LPWG_LG_PAY = 0,
	LPWG_QUICK_TOOL,
	LPWG_AI_PICK,
	LPWG_UDF,
	LPWG_SHOW_AOD,
	LPWG_FAKE_OFF,
	LPWG_FUNCTION_MAX,
};

/* LPWG Gesture list */
enum {
	ONE_TAP = 0,
	TWO_TAP,
	THREE_TAP,
	DOUBLE_TAP,
	DOUBLE_TWO_TAP,
	DOUBLE_THREE_TAP,
	LONG_PRESS,
	SWIPE,
	SWIPE_UP,
	SWIPE_RIGHT,
	SWIPE_DOWN,
	SWIPE_LEFT,
	ABS_COORDINATE,
	GESTURE_MAX,
};

/* Moving side of flexible display  */
enum {
	UP_SIDE	= 0,
	RIGHT_SIDE,
	DOWN_SIDE,
	LEFT_SIDE,
	MOVING_INFO_MAX,
};

/* Moving state of flexible display */
enum {
	STOP = 0,
	START,
	MOVING,
};

/* Moving direction of flexible display */
enum {
	INACTIVE = 0,
	ROLL,
	UNROLL,
	FOLD,
	UNFOLD,
};

#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
enum {
	APP_FW_UPGRADE_IDLE = 0,
	APP_FW_UPGRADE_GET_DATA,
	APP_FW_UPGRADE_FLASHING,
};
#endif

struct state_info {
	atomic_t core;
	atomic_t pm;
	atomic_t fb;
	atomic_t sleep;
	atomic_t uevent;
	atomic_t irq_enable;
	atomic_t connect; /* connection using USB port */
	atomic_t wireless; /* connection using wirelees_charger */
	atomic_t earjack; /* connection using earjack */
	atomic_t fm_radio; /* connection using fm radio */
	atomic_t temperature;
	atomic_t lockscreen;
	atomic_t ime;
	atomic_t film;
	atomic_t incoming_call;
	atomic_t mfts;
	atomic_t sp_link;
	atomic_t debug_tool;
	atomic_t debug_option_mask;
	atomic_t onhand;
	atomic_t active_pen;
	atomic_t film_mode; /* protection film in manufcturing */
	atomic_t dualscreen;
	atomic_t dex_mode;
};

struct touch_driver {
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
	int (*shutdown)(struct device *dev);
	int (*suspend)(struct device *dev);
	int (*resume)(struct device *dev);
	int (*init)(struct device *dev);
	int (*irq_handler)(struct device *dev);
	int (*power)(struct device *dev, int power_mode);
	int (*upgrade)(struct device *dev);
#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
	int (*app_upgrade)(struct device *dev);
#endif
	int (*esd_recovery)(struct device *dev);
	int (*lpwg)(struct device *dev,	u32 code, void *param);
	int (*display_area_status)(struct device *dev, void *param);
	int (*lpwg_function_info)(struct device *dev, int display_area_id);
	int (*swipe_enable)(struct device *dev,	bool enable);
	int (*notify)(struct device *dev, ulong event, void *data);
	int (*init_pm)(struct device *dev);
	int (*register_sysfs)(struct device *dev);
	int (*set)(struct device *dev, u32 cmd, void *input, void *output);
	int (*get)(struct device *dev, u32 cmd, void *input, void *output);
	int (*rotation)(struct device *dev);
	int (*grip_area)(struct device *dev);
#ifdef CONFIG_LGE_TOUCH_PEN
	int (*stop_pen_sensing)(struct device *dev, bool enable);
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
	int (*common_control)(struct device *dev, int rw_mode, char *cmd);
#endif
};

struct touch_device_caps {
	u32 max_x;
	u32 max_y;
	u32 max_pressure;
	u32 max_width_major;
	u32 max_width_minor;
	u32 max_orientation;
	u32 max_id;
	u32 hw_reset_delay;
	u32 sw_reset_delay;
	u32 flexible_display_info_update_frequency;
};

struct touch_operation_role {
	bool use_lpwg;
	bool use_lpwg_test;
	bool hide_coordinate;
	bool use_film_status;
	bool use_active_pen_status;
	bool hidden_coordi;
	bool use_dex_mode;
};

struct active_area {
	int x1;
	int y1;
	int x2;
	int y2;
};

struct knock_on_ctrl {
	bool enable;
	u16 tap_count;
	u16 min_intertap;
	u16 max_intertap;
	u16 touch_slop;
	u16 tap_distance;
	u16 intr_delay;
	struct active_area area;
};

struct swipe_ctrl {
	bool enable;
	bool debug_enable;
	u8 distance;
	u8 ratio_thres;
	u16 min_time;
	u16 max_time;
	u8 wrong_dir_thres;
	u8 init_ratio_chk_dist;
	u8 init_ratio_thres;
	struct active_area area;
	struct active_area start_area;
	struct active_area border_area;
	struct active_area start_border_area;
};

struct longpress_ctrl {
	bool enable;
	int slop;
	int press_time;
	int contact_size;
	struct active_area area;
};

struct onetap_ctrl {
	bool enable;
	u32 slop;
	u32 press_time;
	u32 delay_time;
	struct active_area area;
};

struct abs_ctrl {
	bool enable;
	struct active_area area;
	struct active_area border;
};

struct ai_pick_ctrl {
	bool enable;
	struct active_area area;
};

struct touch_pinctrl {
	struct pinctrl *ctrl;
	struct pinctrl_state *active;
	struct pinctrl_state *suspend;
};

struct touch_data {
	u16 id;
	u16 x;
	u16 y;
	u16 width_major;
	u16 width_minor;
	s16 orientation;
	u16 pressure;
	/* finger, palm, pen, glove, hover */
	u16 type;
#if IS_ENABLED(CONFIG_LGE_TOUCH_DEX)
	struct touch_dex_data dex_data;
#endif
};

struct point {
	int x;
	int y;
};

struct area_info {
	struct point left_top;
	struct point right_bottom;
};

struct lpwg_info {
	u8 screen;	// SCREEN_OFF, SCREEN_ON
	u8 mode;	// LPWG_NONE, LPWG_DOUBLE_TAP
	u8 sensor;	// PROX_NEAR, PROX_FAR
	u8 qcover;	// HALL_FAR, HALL_NEAR
};

struct lpwg_coordinate {
	u32 count;
	struct point code[MAX_TAP_CNT];
};

struct app_info {
	int app;
	char version[10];
	int icon_size;
	int touch_slop;
};

struct perf_test_info {
	bool enable;
	u32 jig_size;
	u32 delay;
	u16 pressure;
	u16 width;
	u16 click_x;
	u16 click_y;
	u16 v_drag_x;
	u16 v_drag_start_y;
	u16 v_drag_end_y;
	u16 h_drag_start_x;
	u16 h_drag_end_x;
	u16 h_drag_y;
};

struct display_area_status_info {
	int display_area_id;
	struct lpwg_info lpwg;
	struct knock_on_ctrl knock_on;
	struct swipe_ctrl lg_pay_swipe[LG_PAY_SWIPE_MAX];
	struct swipe_ctrl quick_tool_swipe[QUICK_TOOL_SWIPE_MAX];
	struct swipe_ctrl fake_off_swipe[FAKE_OFF_SWIPE_MAX];
	struct abs_ctrl quick_tool_abs;
	struct longpress_ctrl udf_longpress;
	struct onetap_ctrl show_aod_onetap;
	struct onetap_ctrl fake_off_onetap;
	struct ai_pick_ctrl ai_pick_double_tap;
};

struct flexible_display_moving_info {
	int side;
	int state;
	int direction;
};

struct flexible_display_area_info {
	int display_area_id;
	struct area_info area;
};

struct flexible_display_info {
	struct flexible_display_moving_info moving[MOVING_INFO_MAX];
	struct flexible_display_area_info area[DISPLAY_AREA_ID_MAX];
};

struct lpwg_function_area {
    struct area_info area;
    int32_t x_offset;
    int32_t y_offset;
};

struct lpwg_function_info {
    int32_t display_area_id;
    int32_t function;
    int32_t gesture;
    bool enable;
    struct lpwg_function_area function_area;
};
#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
struct app_fw_upgrade_info {
	int status;
	size_t max_data_size;
	size_t offset;
	u8 *data;
};
#endif

struct touch_core_data {
	struct platform_device *pdev;

	u8 bus_type;
	int irq;
	unsigned long irqflags;

	struct device *dev;			/* client device : i2c or spi */
	struct input_dev *input;
	struct touch_driver *driver;
	struct kobject kobj;

	int reset_pin;
	int int_pin;
	int maker_id_pin;
	int vcl_pin;
	int vdd_pin;

	void *vcl;
	void *vdd;

	struct touch_device_caps caps;
	struct touch_operation_role role;
	struct state_info state;
	struct touch_pinctrl pinctrl;

	u32 intr_status;
	u16 new_mask;
	u16 old_mask;
	int tcount;
	struct touch_data tdata[MAX_FINGER];
	int is_cancel;

	/* for support multi display area */
	struct display_area_status_info display_area_status[DISPLAY_AREA_ID_MAX];
	/* for support lpwg function set */
	struct lpwg_function_info lpwg_function[DISPLAY_AREA_ID_MAX];
	/* for support flexible display moving */
	struct flexible_display_info flexible_display;

	struct lpwg_coordinate lpwg_coord;
	struct swipe_ctrl swipe[6];	/* down, up, right, left, right2, left2 */

	u8 def_fwcnt;
	const char *touch_ic_name;
	const char *def_fwpath[4];
	char test_fwpath[256];
	const char *panel_spec;
	const char *panel_spec_mfts;
	const char *dual_panel_spec[4];
	const char *dual_panel_spec_mfts[4];
	const char *panel_spec_mfts_flat;
	const char *panel_spec_mfts_curved;
	u8 force_fwup;

	int mfts_lpwg;

	u8 *tx_buf;
	u8 *rx_buf;
	struct touch_xfer_msg *xfer;

	struct mutex lock;
	struct workqueue_struct *wq;
	struct delayed_work init_work;
	struct delayed_work upgrade_work;
#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
	struct delayed_work hidden_work;
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
	struct delayed_work app_upgrade_work;
#endif
	struct delayed_work notify_work;
	struct delayed_work fb_work;
	struct delayed_work panel_reset_work;

	struct notifier_block blocking_notif;
	struct notifier_block atomic_notif;
	struct notifier_block display_notif;
	struct notifier_block notif;
	unsigned long notify_event;
	int notify_data;
	struct atomic_notify_event notify_event_arr[ATOMIC_NOTIFY_EVENT_SIZE];
#if IS_ENABLED(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#elif IS_ENABLED(CONFIG_DRM_MSM) & IS_ENABLED(CONFIG_FB)
	struct notifier_block drm_notif;
#elif IS_ENABLED(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	void *touch_device_data;

#if IS_ENABLED(CONFIG_SECURE_TOUCH)
	atomic_t st_enabled;
	atomic_t st_pending_irqs;
	struct completion st_powerdown;
	struct completion st_irq_processed;
	bool st_initialized;
	struct clk *core_clk;
	struct clk *iface_clk;
	struct mutex touch_io_ctrl_mutex;
#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
    struct completion st_irq_received;
#endif

#endif

	/* for MTK */
	int vcl_id;
	int vcl_vol;
	int vdd_id;
	int vdd_vol;

	dma_addr_t tx_pa;
	dma_addr_t rx_pa;

	struct app_info app_data[3];
	struct perf_test_info perf_test;

#ifdef CONFIG_LGE_TOUCH_PEN
	struct input_dev *input_pen;
	struct pen_data pdata;

	/* prediction filter off : 0, on : 1 */
	struct pen_prediction_data pp;
	int pred_filter;
	u8 aes_mode;
#endif

	struct completion uevent_complete;

	int rotation;
#if IS_ENABLED(CONFIG_LGE_TOUCH_DEX)
	int dex_tcount;
	struct touch_dex_ctrl touch_dex;
	struct input_dev *input_dex;
#endif
	struct area_info grip_area[GRIP_AREA_NUM];
	bool have_grip_area;
#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
	long hidden_matrix[HIDDEN_COEFF_MAT_SIZE][HIDDEN_COEFF_MAT_SIZE];
	char str_hidden_RSA[BASE64_ENCODING_SIZE];
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
	struct app_fw_upgrade_info app_fw_upgrade;
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_COMMON_CONTROL)
	struct common_cmd_data cmd_data;
	int systrace_mode;
#endif
};

#define PROPERTY_GPIO(np, string, target)				\
	(target = of_get_named_gpio_flags(np, string, 0, NULL))

#define PROPERTY_BOOL(np, string, target)				\
	do {								\
		u32 tmp_val = 0;					\
		if (of_property_read_u32(np, string, &tmp_val) < 0)	\
			target = 0;					\
		else							\
			target = (u8)tmp_val;				\
	} while (0)

#define PROPERTY_U32(np, string, target)				\
	do {								\
		u32 tmp_val = 0;					\
		if (of_property_read_u32(np, string, &tmp_val) < 0)	\
			target = -1;					\
		else							\
			target = tmp_val;				\
	} while (0)

#define PROPERTY_STRING_ARRAY(np, string, target, cnt)			\
	do {								\
		int i;						\
		cnt = of_property_count_strings(np, string);		\
		for (i = 0; i < cnt; i++) {				\
			of_property_read_string_index(np, string, i,	\
						      &target[i]);	\
		}							\
	} while (0)

#define PROPERTY_STRING(np, string, target)				\
	do {								\
		of_property_read_string(np, string, &target);		\
	} while (0)

struct touch_attribute {
	struct attribute attr;
	ssize_t (*show)(struct device *dev, char *buf);
	ssize_t (*store)(struct device *idev, const char *buf, size_t count);
};

#define TOUCH_ATTR(_name, _show, _store)		\
			struct touch_attribute touch_attr_##_name	\
			= __ATTR(_name, 0644, _show, _store)

static inline void touch_set_device(struct touch_core_data *ts, void *data)
{
	TOUCH_I("%s, data:%p\n", __func__, data);
	ts->touch_device_data = data;
}

static inline void *touch_get_device(struct touch_core_data *ts)
{
	return ts->touch_device_data;
}

static inline struct touch_core_data *to_touch_core(struct device *dev)
{
	return dev ? (struct touch_core_data *)dev_get_drvdata(dev) : NULL;
}

extern void module_write_file(struct device *dev, char *data, int write_time);
extern void module_log_file_size_check(struct device *dev);
extern irqreturn_t touch_irq_handler(int irq, void *dev_id);
extern irqreturn_t touch_irq_thread(int irq, void *dev_id);
extern void touch_msleep(unsigned int msecs);
extern int touch_snprintf(char *buf, int size, const char *fmt, ...);
extern int touch_get_dts(struct touch_core_data *ts);
extern int touch_get_platform_data(struct touch_core_data *ts);
extern int touch_init_sysfs(struct touch_core_data *ts);
extern int touch_init_sysfs_module(struct module_data *md_kobj, void *module_ts);
extern void touch_interrupt_control(struct device *dev, int on_off);
extern void touch_report_all_event(struct touch_core_data *ts);
extern void touch_restore_state(struct touch_core_data *ts);
extern void touch_send_uevent(struct touch_core_data *ts, int type);
extern void touch_report_cancel_event(struct touch_core_data *ts);
extern void touch_report_cancel_event_display_area(struct touch_core_data *ts, int display_area_id);

#if IS_ENABLED(CONFIG_SECURE_TOUCH)
extern void secure_touch_notify(struct touch_core_data *ts);
extern void secure_touch_init(struct touch_core_data *ts);
extern void secure_touch_stop(struct touch_core_data *ts, bool blocking);
#endif
#if IS_ENABLED(CONFIG_LGE_TOUCH_HIDDEN)
extern void generate_coeff_matrix(void *ts);
extern void hidden_coordinate(void *ts, int x, int y);
#endif

#if defined(CONFIG_TRUSTONIC_TRUSTED_UI)
ssize_t tui_sysfs_ctl(struct device *dev, const char *buf, size_t count);
#endif

#endif /* LGE_TOUCH_CORE_H */
