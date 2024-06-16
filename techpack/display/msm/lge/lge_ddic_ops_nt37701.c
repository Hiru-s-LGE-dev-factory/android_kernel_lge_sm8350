#define pr_fmt(fmt)	"[Display][nt37701-ops:%s:%d] " fmt, __func__, __LINE__

#include <linux/slab.h>
#include <linux/lge_panel_notify.h>
#include "dsi_panel.h"
#include "lge_ddic_ops_helper.h"
#include "cm/lge_color_manager.h"
#include "brightness/lge_brightness_def.h"
#include "lge_dsi_panel.h"

#define WORDS_TO_BYTE_ARRAY(w1, w2, b) do {\
		b[0] = WORD_UPPER_BYTE(w1);\
		b[1] = WORD_LOWER_BYTE(w1);\
		b[2] = WORD_UPPER_BYTE(w2);\
		b[3] = WORD_LOWER_BYTE(w2);\
} while(0)

extern int lge_ddic_dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum lge_ddic_dsi_cmd_set_type type);
extern int lge_mdss_dsi_panel_cmd_read(struct dsi_panel *panel,
					u8 cmd, int cnt, char* ret_buf);
extern char* get_payload_addr(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int get_payload_cnt(struct dsi_panel *panel, enum lge_ddic_dsi_cmd_set_type type, int position);
extern int lge_backlight_device_update_status(struct backlight_device *bd);

extern int dsi_panel_set_backlight(struct dsi_panel *panel, u32 bl_lvl);
extern int lge_panel_notifier_call_chain(unsigned long val, int display_id, int state);

#define NUM_SAT_CTRL 5
#define OFFSET_SAT_CTRL 46
#define REG_SAT_CTRL 0xD1

#define NUM_HUE_CTRL 5
#define OFFSET_HUE_CTRL 52
#define REG_HUE_CTRL 0xD2

#define NUM_SHA_CTRL 5
#define OFFSET_SHA_CTRL 7
#define REG_SHA_CTRL 0xD3

static char sat_ctrl_values[NUM_SAT_CTRL][OFFSET_SAT_CTRL] = {
	{0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00},
	{0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00},
	{0x32, 0x33, 0x32, 0x35, 0x32, 0x30, 0x30, 0x2F, 0x2D, 0x2D, 0x2D, 0x29, 0x29, 0x29, 0x32, 0x35, 0x35, 0x35, 0x35, 0x33, 0x32, 0x33, 0x35, 0x35, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x01, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00},
	{0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00},
	{0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00}
};

static char hue_ctrl_values[NUM_HUE_CTRL][OFFSET_HUE_CTRL] = {
	{0x77, 0x0C, 0xD0, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0xC8, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0x40, 0x80, 0x80, 0x7F, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xA8, 0xA8, 0xA0, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x40, 0x40, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0xB8, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0xAF, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
};

static char sha_ctrl_values[NUM_SHA_CTRL][OFFSET_SHA_CTRL] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x01, 0x00, 0x00, 0x01, 0x10, 0x00},
	{0x02, 0x00, 0x00, 0x01, 0x10, 0x00},
	{0x04, 0x00, 0x00, 0x01, 0x10, 0x00},
	{0x07, 0x00, 0x00, 0x01, 0x10, 0x00}
};

static void set_dgc_status(struct dsi_panel *panel, bool req_state)
{
	int new = (req_state ? 0x01 : 0x00);

	if (panel->lge.hdr_mode || panel->lge.irc_current_state)
		new = 0x00;

	panel->lge.dgc_status = new;
}

static void lge_display_control_store_nt37701(struct dsi_panel *panel, bool send_cmd)
{
	char *dispctrl1_payload = NULL;

	if(!panel) {
		pr_err("panel not exist\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	dispctrl1_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 0);

	if (!dispctrl1_payload) {
		pr_err("LGE_DDIC_DSI_DISP_CTRL_COMMAND is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	if (panel->lge.dgc_status == 1) {
		dispctrl1_payload[1] |= BIT(7);
	} else {
		dispctrl1_payload[1] &= ~BIT(7);
	}

	pr_info("ctrl-command-1: 0x%02x\n", dispctrl1_payload[1]);

	if (send_cmd) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1);
	}

	mutex_unlock(&panel->panel_lock);
	return;
}

static void lge_set_screen_tune_nt37701(struct dsi_panel *panel)
{
	int i = 0;
	int sat_index = 0;
	int hue_index = 0;
	int sha_index = 0;

	char *sat_payload = NULL;
	char *hue_payload = NULL;
	char *sha_payload = NULL;

	if(!panel) {
		pr_err("panel not exist\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	sat_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_SATURATION, 1);
	hue_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_HUE, 1);
	sha_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_SHARPNESS, 1);

	if (!sat_payload) {
		pr_err("LGE_DDIC_DSI_SET_SATURATION is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	if (!hue_payload) {
		pr_err("LGE_DDIC_DSI_SET_HUE is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	if (!sha_payload) {
		pr_err("LGE_DDIC_DSI_SET_SHARPNESS is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	sat_index = panel->lge.sc_sat_step;
	hue_index = panel->lge.sc_hue_step;
	sha_index = panel->lge.sc_sha_step;

	// SATURATION CTRL
	for (i = 0; i < OFFSET_SAT_CTRL; i++) {
		sat_payload[i+1] = sat_ctrl_values[sat_index][i];
	}

	// HUE CTRL
	for (i = 0; i < OFFSET_HUE_CTRL; i++) {
		hue_payload[i+1] = hue_ctrl_values[hue_index][i];
	}

	// SHARPNESS CTRL
	for (i = 0; i < OFFSET_SHA_CTRL; i++) {
		sha_payload[i+1] = sha_ctrl_values[sha_index][i];
	}

	for (i = 0; i < OFFSET_SAT_CTRL; i++) {
		pr_debug("Reg:0x%02x [%d:0x%02x]\n", REG_SAT_CTRL, i, sat_payload[i]);
	}
	for (i = 0; i < OFFSET_HUE_CTRL; i++) {
		pr_debug("Reg:0x%02x [%d:0x%02x]\n", REG_HUE_CTRL, i, hue_payload[i]);
	}
	for (i = 0; i < OFFSET_SHA_CTRL; i++) {
		pr_debug("Reg:0x%02x [%d:0x%02x]\n", REG_SHA_CTRL, i, sha_payload[i]);
	}

	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SATURATION);
	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_HUE);
	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SHARPNESS);

	mutex_unlock(&panel->panel_lock);
	return;
}

static void lge_set_screen_mode_nt37701(struct dsi_panel *panel, bool send_cmd)
{
	mutex_lock(&panel->panel_lock);

	pr_info("screen_mode %d\n", panel->lge.screen_mode);
	set_dgc_status(panel, true);

	switch (panel->lge.screen_mode) {
	case screen_mode_oled_cinema:
		panel->lge.sharpness_status = 0x00;
		pr_info("cinema mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_CINEMA);
		break;
	case screen_mode_oled_vivid:
		panel->lge.sharpness_status = 0x00;
		pr_info("vivid mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_VIVID);
		break;
	case screen_mode_oled_custom:
		panel->lge.sharpness_status = 0x01;
		pr_info("custom mode - saturation: %d, hue: %d, sharpness: %d\n",
				panel->lge.sc_sat_step, panel->lge.sc_hue_step, panel->lge.sc_sha_step);
		mutex_unlock(&panel->panel_lock);
		lge_set_screen_tune_nt37701(panel);
		mutex_lock(&panel->panel_lock);
		break;
	case screen_mode_oled_natural:
	default:
		panel->lge.sharpness_status = 0x00;
		pr_info("natural mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_NATURAL);
		break;
	}

	mutex_unlock(&panel->panel_lock);
	lge_display_control_store_nt37701(panel, send_cmd);
	return;
}

static int lge_hdr_mode_set_nt37701(struct dsi_panel *panel, int input)
{
	bool hdr_mode = ((input > 0) ? true : false);

	mutex_lock(&panel->panel_lock);
	if (hdr_mode) {
		set_dgc_status(panel, false);
	} else {
		set_dgc_status(panel, true);
	}
	mutex_unlock(&panel->panel_lock);
	pr_info("hdr=%s, dgc=%s\n", (hdr_mode ? "set" : "unset"),
			((panel->lge.dgc_status == 1) ? "enabled" : "disabled"));

	if (hdr_mode) {
		lge_display_control_store_nt37701(panel, true);
	} else {
		lge_set_screen_mode_nt37701(panel, true);
	}

	lge_backlight_device_update_status(panel->bl_config.raw_bd);

	return 0;
}

static void set_fps_cmds_nt37701(struct dsi_panel *panel, u32 fps_input, bool resync)
{
	char *fpsctrl1_payload = NULL;

	if (!panel) {
		pr_err("panel is null\n");
		return;
	}

	if (lge_dsi_panel_is_power_on_lp(panel) || !dsi_panel_initialized(panel)) {
		pr_info("received fps swithcing in active mode. fps %d reserved\n", fps_input);
		panel->lge.last_frame_rate = fps_input;
		return;
	}

	if (!resync && panel->lge.last_frame_rate != LGE_FPS_UNSET && panel->lge.last_frame_rate == fps_input) {
		pr_info("requested same fps %d. skip it\n", fps_input);
		return;
	}

	fpsctrl1_payload = get_payload_addr(panel, LGE_DDIC_DSI_SWITCH_FPS_CTRL, 0);

	if (!fpsctrl1_payload) {
		pr_err("LGE_DDIC_DSI_SWITCH_FPS_CTRL is NULL\n");
		return;
	}

	fpsctrl1_payload[1] &= 0x00;

	switch (fps_input) {
		case LGE_FPS_120HZ:
			fpsctrl1_payload[1] |= 0x02;
			break;
		case LGE_FPS_60HZ:
		case LGE_FPS_30HZ:
			fpsctrl1_payload[1] |= 0x01;
			break;
		default:
			pr_warn("WARNING: not supported fps\n");
			return;
	};
	mutex_lock(&panel->panel_lock);
	panel->lge.last_frame_rate = fps_input;
	pr_info("%s %d fps cmds\n", resync?"restore":"send", fps_input);
	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SWITCH_FPS_CTRL);
	mutex_unlock(&panel->panel_lock);
	pr_info("[Touch] call %d fps noti\n", fps_input);
	lge_panel_notifier_call_chain(LGE_PANEL_EVENT_FPS,
			panel->lge.display_id, fps_input);
}

static void lge_vr_lp_mode_set_nt37701(struct dsi_panel *panel, int input)
{
	int rc = 0;
	bool enable = false;

	mutex_lock(&panel->panel_lock);

	panel->lge.vr_lp_mode = input;
	enable = !!panel->lge.vr_lp_mode;

	if (enable) {
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_VR_MODE_ON);
	} else {
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_VR_MODE_OFF);
	}
	mutex_unlock(&panel->panel_lock);

	pr_info("send cmds to %s vr_lp_set \n",	(input == true) ? "enable" : "disable");
}

static int lge_daylight_mode_set_nt37701(struct dsi_panel *panel, int input)
{
	mutex_lock(&panel->panel_lock);
	pr_info("daylight_mode = %d\n", input);
/* TBD
	if(panel->lge.daylight_mode)
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DAYLIGHT_ON);
	else
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DAYLIGHT_OFF);
*/
	mutex_unlock(&panel->panel_lock);

	lge_backlight_device_update_status(panel->bl_config.raw_bd);

	return 0;
}

struct lge_ddic_ops nt37701_ops = {
	/* AoD */
	.store_aod_area = NULL,
	.prepare_aod_cmds = NULL,
	.prepare_aod_area = NULL,
	.lge_check_vert_black_line = NULL,
	.lge_check_vert_white_line = NULL,
	.lge_check_vert_line_restore = NULL,
	/* brightness */
	.init_hl_mode = NULL,
	.set_hl_mode_pending = NULL,
	.set_hl_mode = NULL,
	.get_hl_mode = NULL,
	.set_exscreen_width = NULL,
	.lge_bc_dim_set = NULL,
	.lge_set_therm_dim = NULL,
	.lge_get_brightness_dim = NULL,
	.lge_set_brightness_dim = NULL,
	.daylight_mode_set = lge_daylight_mode_set_nt37701,
	/* image quality */
	.hdr_mode_set = lge_hdr_mode_set_nt37701,
	.lge_set_custom_rgb = NULL,
	.lge_display_control_store = lge_display_control_store_nt37701,
	.lge_set_screen_tune = lge_set_screen_tune_nt37701,
	.lge_set_screen_mode = lge_set_screen_mode_nt37701,
	.sharpness_set = NULL,
	.lge_set_true_view_mode = NULL,
	.lge_set_video_enhancement = NULL,
	.lge_vr_lp_mode_set = lge_vr_lp_mode_set_nt37701,
	.lge_set_tc_perf = NULL,

	/* drs */
	.get_current_res = NULL,
	.get_support_res = NULL,

	/* fps */
	.set_fps_cmds = set_fps_cmds_nt37701,
	/* bist */
	.bist_ctrl = NULL,
	.release_bist = NULL,
	/* error detect */
	.err_detect_work = NULL,
	.err_detect_irq_handler = NULL,
	.set_err_detect_mask = NULL,
	/* pps */
	.set_pps_cmds = NULL,
	.unset_pps_cmds = NULL,
	/* irc */
	.set_irc_default_state = NULL,
	.set_irc_state = NULL,
	.get_irc_state = NULL,
	/* lhbm */
	.lge_set_fp_lhbm = NULL,
};
