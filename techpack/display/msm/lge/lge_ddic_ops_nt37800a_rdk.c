#define pr_fmt(fmt)	"[Display][nt37800a_rdk-ops:%s:%d] " fmt, __func__, __LINE__

#include <linux/slab.h>
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

static void set_fps_cmds_nt37800a_rdk(struct dsi_panel *panel, u32 fps_input, bool resync)
{
	char *fpsctrl1_payload = NULL;

	if (!panel) {
		pr_err("panel is null\n");
		return;
	}

	if (lge_dsi_panel_is_power_on_lp(panel)) {
		pr_info("received fps swithcing in lp mode. fps %d reserved\n", fps_input);
		panel->lge.last_frame_rate = fps_input;
		return;
	}

	if (!resync && panel->lge.last_frame_rate != LGE_FPS_UNSET && panel->lge.last_frame_rate == fps_input) {
		pr_info("requested same fps %d. skip it\n", fps_input);
		return;
	}

	fpsctrl1_payload = get_payload_addr(panel, LGE_DDIC_DSI_SWITCH_FPS_CTRL, 1);

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
	panel->lge.last_frame_rate = fps_input;
	pr_info("%s %d fps cmds\n", resync?"restore":"send", fps_input);
	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SWITCH_FPS_CTRL);
}

struct lge_ddic_ops nt37800a_rdk_ops = {
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
	.daylight_mode_set = NULL,
	/* image quality */
	.hdr_mode_set = NULL,
	.lge_set_custom_rgb = NULL,
	.lge_display_control_store = NULL,
	.lge_set_screen_tune = NULL,
	.lge_set_screen_mode = NULL,
	.sharpness_set = NULL,
	.lge_set_true_view_mode = NULL,
	.lge_set_video_enhancement = NULL,
	.lge_vr_lp_mode_set = NULL,
	.lge_set_tc_perf = NULL,

	/* drs */
	.get_current_res = NULL,
	.get_support_res = NULL,

	/* fps */
	.set_fps_cmds = set_fps_cmds_nt37800a_rdk,
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
