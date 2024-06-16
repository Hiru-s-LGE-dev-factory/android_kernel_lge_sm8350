#define pr_fmt(fmt)	"[Display][nt37800a-ops:%s:%d] " fmt, __func__, __LINE__

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

enum expandable_enable_region {
	ENABLE_NONE = 0,
	ENABLE_FRONT,
	ENABLE_REAR,
	ENABLE_BOTH,
	ENABLE_MAX,
};

enum expandable_estv_region {
	ESTV_BOTH,
	ESTV_REAR,
	ESTV_FRONT,
	ESTV_DISABLED,
};

enum expandable_power_state {
	EXPANDABLE_STATE_OFF,
	EXPANDABLE_STATE_DOZE,
	EXPANDABLE_STATE_ON,
	EXPANDABLE_STATE_DOZE_SUSPEND,
	EXPANDABLE_STATE_ON_SUSPEND,
	EXPANDABLE_STATE_MAX,
};

const char expandable_power_state_string[5][12] = {
	"off", "doze", "on", "dozesuspend", "onsuspend",
};

const struct drs_res_info nt37800a_res[ENABLE_MAX] = {
	{"full", 0, 2428, 1876},
	{"max", 0, 2428, 1600},
	{"mid", 0, 2428, 1366},
	{"min", 0, 2428, 1080},
};

#define EXPANDABLE_FRONT 0
#define EXPANDABLE_REAR 1
#define EXPANDABLE_DEADZONE 108
#define EXPANDABLE_AREA_MAX 1
#define EXPANDABLE_AREA_MID 2
#define EXPANDABLE_AREA_MIN 3

struct expandable_private {
	enum expandable_power_state power_state;
	int disable_gate;
	int roi;
};

struct expandable_handle {
	struct mutex expandable_lock;
	bool initialized;
	int req_width_id;
	int cur_width_id;
	bool switch_dimming_param;
	int last_estv;
	struct expandable_private priv_info[2];
};

static struct expandable_handle *to_exp_handle(struct dsi_panel *panel)
{
	struct expandable_handle *h = NULL;

	h = (struct expandable_handle *)panel->lge.expandable_handle;
	if (!h) {
		pr_err("failed to get handle\n");
		return NULL;
	}

	return h;
}

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
	{0x38, 0x36, 0x37, 0x37, 0x36, 0x36, 0x36, 0x39, 0x33, 0x29, 0x2D, 0x29, 0x26, 0x31, 0x33, 0x35, 0x35, 0x36, 0x36, 0x36, 0x36, 0x34, 0x35, 0x35, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00},
	{0x3A, 0x3A, 0x3D, 0x3D, 0x3D, 0x3C, 0x3C, 0x3D, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00},
	{0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x00, 0x06, 0x0C, 0x03, 0x09, 0x0F, 0x68, 0x88, 0x00}
};

static char hue_ctrl_values[NUM_HUE_CTRL][OFFSET_HUE_CTRL] = {
	{0x77, 0x0C, 0xD0, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0xC8, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0x40, 0x96, 0x83, 0x93, 0x80, 0x7E, 0x63, 0x58, 0x40, 0x40, 0x40, 0x68, 0x7E, 0x78, 0x94, 0x94, 0x94, 0x8C, 0x7A, 0x7A, 0x7A, 0x7A, 0x7D, 0x86, 0x8A, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0xB8, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80},
	{0x77, 0x0C, 0xAF, 0x80, 0x90, 0x9A, 0x90, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80}
};

static char sha_ctrl_values[NUM_SHA_CTRL][OFFSET_SHA_CTRL] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
	{0x03, 0x00, 0x00, 0x01, 0x70, 0x00},
	{0x04, 0x00, 0x00, 0x01, 0x60, 0x00},
	{0x05, 0x00, 0x00, 0x01, 0x50, 0x00},
	{0x05, 0x00, 0x00, 0x01, 0x40, 0x00}
};


static int _get_dic_mode(int value, int type)
{
	int bitmask = 0x0;

	switch (type) {
	case EXPANDABLE_FRONT:
		bitmask |= BIT(7);
		break;
	case EXPANDABLE_REAR:
		bitmask |= BIT(3);
		break;
	default:
		pr_err("not defined\n");
		break;
	};

	return (!!(value & bitmask));
}

static void _del_dic_power_state(struct dsi_panel *panel, int type)
{
	int bitshift = ((type == EXPANDABLE_FRONT) ? 4 : 0);
	int bitmask = 0x0;
	bitmask |= (BIT(bitshift) | BIT(bitshift + 1) | BIT(bitshift + 2));

	panel->lge.expandable_mode &= ~bitmask;
	return;
}

static void _set_dic_power_state(struct dsi_panel *panel,
				enum expandable_power_state state,
				int type)
{
	int bitshift = ((type == EXPANDABLE_FRONT) ? 4 : 0);
	int new_state = state << bitshift;

	pr_info("set new power_state=%d\n", new_state);

	panel->lge.expandable_mode |= new_state;
	return;
}

static int _get_dic_power_state(int value, int type)
{
	int bitshift = ((type == EXPANDABLE_FRONT) ? 4 : 0);
	int bitmask = 0x0;
	bitmask |= (BIT(bitshift) | BIT(bitshift + 1) | BIT(bitshift + 2));

	return ((value & bitmask) >> bitshift);
}

static void _prepare_dic_power_state(struct dsi_panel *panel, int type)
{
	int bitshift = ((type == EXPANDABLE_FRONT) ? 9 : 8);
	int bitmask = 0x0;
	int value = panel->lge.expandable_mode;

	bitmask |= BIT(bitshift);
	panel->lge.expandable_mode &= ~bitmask;

	if ((value & bitmask) != 0) {
		if (_get_dic_power_state(value, type) == EXPANDABLE_STATE_ON) {
			pr_warn("do nothingi - skip for %d\n", type);
			return;
		}
		_del_dic_power_state(panel, type);
		_set_dic_power_state(panel, EXPANDABLE_STATE_ON, type);
	}
	return;
}

static bool update_expandable_power_state(struct expandable_handle *h, int value)
{
	int front = h->priv_info[EXPANDABLE_FRONT].power_state;
	int rear = h->priv_info[EXPANDABLE_REAR].power_state;
	int new = 0;
	bool updated = false;

	pr_info("++ front:%s, rear=%s\n",
			expandable_power_state_string[front],
			expandable_power_state_string[rear]);
	new = _get_dic_power_state(value, EXPANDABLE_FRONT);
	if (front != new) {
		h->priv_info[EXPANDABLE_FRONT].power_state = new;
		front = new;
		updated |= true;
	}

	new = _get_dic_power_state(value, EXPANDABLE_REAR);
	if (rear != new) {
		h->priv_info[EXPANDABLE_REAR].power_state = new;
		rear = new;
		updated |= true;
	}

	pr_info("-- front:%s, rear=%s, need_update=%d\n",
			expandable_power_state_string[front],
			expandable_power_state_string[rear],
			updated);

	return updated;
}

static bool update_expandable_disable_gate(struct expandable_handle *h, int value)
{
	int front = h->priv_info[EXPANDABLE_FRONT].disable_gate;
	int rear = h->priv_info[EXPANDABLE_REAR].disable_gate;
	int new = 0;
	bool updated = false;

	pr_info("++ front_disabled=%d, rear_disabled=%d\n", front, rear);
	new = _get_dic_mode(value, EXPANDABLE_FRONT);
	if (front != new) {
		h->priv_info[EXPANDABLE_FRONT].disable_gate = new;
		front = new;
		updated |= true;
	}

	new = _get_dic_mode(value, EXPANDABLE_REAR);
	if (rear != new) {
		h->priv_info[EXPANDABLE_REAR].disable_gate = new;
		rear = new;
		updated |= true;
	}

	pr_info("-- front_disabled=%d, rear_disabled=%d, need_update=%d\n", front, rear, updated);
	return updated;
}

static bool update_expandable_roi(struct expandable_handle *h)
{
	int front = h->priv_info[EXPANDABLE_FRONT].roi;
	int new = 0;
	bool updated = false;

	pr_info("++ front=%s\n", nt37800a_res[front].resolution);

	if (front == 0) {
		/* first bootup : update current pos */
		new = h->cur_width_id;
	} else if (h->cur_width_id == 0) {
		pr_info("current id = 0, do nothing\n");
		new = front;
	} else if ((h->req_width_id != 0) &&
			(h->req_width_id < front)) {
		pr_info("go wide screen mode\n");
		new = h->req_width_id;
	} else {
		new = h->cur_width_id;
	}

	if (front != new) {
		h->priv_info[EXPANDABLE_FRONT].roi = new;
		front = new;
		updated |= true;
	}
	pr_info("-- front=%s, updated=%d\n",
			nt37800a_res[front].resolution, updated);

	return updated;
}

static void update_expandable_gate(struct dsi_panel *panel, struct expandable_handle *h)
{
	char *payload = NULL;
	int front_disable = 0, rear_disable = 0, enable = ENABLE_BOTH, estv = ESTV_DISABLED;
	bool partial_mode = true;

	if (!panel || !h) {
		pr_err("invalid input\n");
		return;
	}

	front_disable = h->priv_info[EXPANDABLE_FRONT].disable_gate;
	rear_disable = h->priv_info[EXPANDABLE_REAR].disable_gate;

	if (front_disable & 0x01) {
		if (rear_disable & 0x01)
			enable = ENABLE_NONE;
		else {
			enable = ENABLE_REAR;
			estv = ESTV_REAR;
		}
	} else {
		if (rear_disable & 0x01) {
			enable = ENABLE_FRONT;
			estv = ESTV_FRONT;
		} else {
			enable = ENABLE_BOTH;
		}
	}

	partial_mode = ((h->priv_info[EXPANDABLE_FRONT].roi > 0) ? true : false);

	payload = get_payload_addr(panel, LGE_DDIC_DSI_EXPANDABLE_AREA_ENABLE, 0);
	payload++;
	*payload = enable;

	payload = get_payload_addr(panel, LGE_DDIC_DSI_EXPANDABLE_AREA_ENABLE, 1);
	if (partial_mode)
		*payload = 0x12;
	else
		*payload = 0x13;

	if (h->priv_info[EXPANDABLE_FRONT].roi != EXPANDABLE_AREA_MIN)
		estv = ESTV_DISABLED;

	h->last_estv = estv;

	payload = get_payload_addr(panel, LGE_DDIC_DSI_EXPANDABLE_AREA_ENABLE, 2);
	if (payload) {
		payload++;
		*payload = estv;
	}

	pr_info("partial=%d, enable=%d, estv=%d\n", partial_mode, enable, estv);

	return;
}


static void update_expandable_area(struct dsi_panel *panel, struct expandable_handle *h)
{
	char *payload = NULL;
	int roi = 0, front_s = 0, front_e = 0, rear_s = 0, rear_e = 0;
	int deadzone = EXPANDABLE_DEADZONE;

	if (!panel || !h) {
		pr_err("invalid input\n");
		return;
	}

	roi = h->priv_info[EXPANDABLE_FRONT].roi;
	front_e = nt37800a_res[0].height - 1;
	front_s = front_e - nt37800a_res[roi].height - deadzone - 1;
	rear_e = front_s - 1;
	rear_s = 0;

	pr_info("roi=%d, fs=%d, fe=%d, rs=%d, re=%d\n", roi, front_s, front_e, rear_s, rear_e);
	payload = get_payload_addr(panel, LGE_DDIC_DSI_EXPANDABLE_AREA, 0);
	payload++;
	WORDS_TO_BYTE_ARRAY(front_s, front_e, payload);

	payload = get_payload_addr(panel, LGE_DDIC_DSI_EXPANDABLE_AREA, 1);
	payload++;
	WORDS_TO_BYTE_ARRAY(rear_s, rear_e, payload);
	return;
}

static int update_hl_mode(struct expandable_handle *h)
{
	int hl_mode = 0;

	if (h->priv_info[EXPANDABLE_FRONT].roi != EXPANDABLE_AREA_MIN)
		return hl_mode;

	if ((h->priv_info[EXPANDABLE_REAR].power_state == EXPANDABLE_STATE_ON) &&
			(h->priv_info[EXPANDABLE_FRONT].power_state != EXPANDABLE_STATE_ON))
		hl_mode = 1;

	pr_info("hl_mode %s\n", ((hl_mode == 1) ? "enabled" : "disabled"));
	return hl_mode;
}

static void lge_hl_mode_reset_value(struct dsi_panel *panel, struct expandable_handle *h)
{
	panel->lge.hl_mode = 0;
	panel->lge.expandable_mode = 0;
	panel->lge.expandable_pending_mode = 0;
	panel->lge.expandable_pending = false;
	memset(&h->priv_info[EXPANDABLE_FRONT], 0x0, sizeof(struct expandable_private));
	memset(&h->priv_info[EXPANDABLE_REAR], 0x0, sizeof(struct expandable_private));

	update_expandable_gate(panel, h);

	pr_info("finished\n");
}

static void lge_init_hl_mode_nt37800a(struct dsi_panel *panel)
{
	struct expandable_handle *h = (struct expandable_handle *)panel->lge.expandable_handle;

	if (h && h->initialized) {
		lge_hl_mode_reset_value(panel, h);
		return;
	} else
		h = (struct expandable_handle*)kzalloc(sizeof(struct expandable_handle), GFP_KERNEL);

	if (!h) {
		pr_err("failed get resource\n");
		return;
	}

	lge_hl_mode_reset_value(panel, h);
	panel->lge.expandable_handle = (void *)h;
	mutex_init(&h->expandable_lock);
	h->initialized = true;

	return;
}

/************** AoD ********************/
/*
 * Demura / page 4, D0h bit(7)
 * PPA / page 7, B0h bit(7)
 * VDC or IRC / page 8, bit(0) --> remove
 */
static void prepare_power_optimize_cmds(struct dsi_panel *panel, struct dsi_cmd_desc *cmds, int cmds_count, bool optimize)
{
	char *payload = NULL;
	struct expandable_handle *h = NULL;

	h = to_exp_handle(panel);
	if (!h) {
		pr_err("handle is null\n");
		return;
	}

	payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_LP2, 2);
	if (payload) {
		payload++;
		if (!optimize)
			*payload |= BIT(7);
		else
			*payload &= ~BIT(7);
	}

	pr_err("last_estv = %d\n", h->last_estv);
	payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_LP2, 7);
	if (payload) {
		payload++;
		*payload = h->last_estv;
	}
}

#if 0
static void prepare_aod_area_nt37800a(struct dsi_panel *panel, struct dsi_cmd_desc *cmds, int cmds_count)
{
	int sr = 0, er = 0;

	if (panel == NULL || cmds == NULL || cmds_count == 0)
		return;

	adjust_roi(panel, &sr, &er);
	prepare_cmd(cmds, cmds_count, ADDR_PTLAR, sr, er);

	return;
}
#endif

static int prepare_aod_cmds_nt37800a(struct dsi_panel *panel, struct dsi_cmd_desc *cmds, int cmds_count)
{
	int rc = 0;

	if (panel == NULL || cmds == NULL || cmds_count == 0)
		return -EINVAL;

	if (panel->lge.aod_power_mode &&
			(panel->lge.aod_area.h != nt37800a_res[0].width)) {
		pr_info("set aod power optimization\n");
		/* FIXME : after verifying optimization feature */
		prepare_power_optimize_cmds(panel, cmds, cmds_count, false/*true*/);
	} else {
		pr_info("unset aod power optimization\n");
		prepare_power_optimize_cmds(panel, cmds, cmds_count, false);
	}

	return rc;
}

/************** Display Control ********************/
static void lge_display_control_store_nt37800a(struct dsi_panel *panel, bool send_cmd)
{
	char *payload1 = NULL;
	char *payload2 = NULL;
	char *payload3 = NULL;
	char *dispctrl1_payload = NULL;
	char *dispctrl2_payload = NULL;
	char *dispctrl3_payload = NULL;
	struct expandable_handle *h = NULL;

	if(!panel) {
		pr_err("panel not exist\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	dispctrl1_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1, 0);
	dispctrl2_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_2, 0);
	dispctrl3_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_2, 3);

	if (!dispctrl1_payload || !dispctrl2_payload || !dispctrl3_payload) {
		pr_err("LGE_DDIC_DSI_DISP_CTRL_COMMAND is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	/* BC DIM EN */
	h = to_exp_handle(panel);
	payload1 = dispctrl1_payload;
	payload1++;
	if (panel->lge.use_dim_ctrl && panel->lge.use_bc_dimming_work &&
			h && h->switch_dimming_param) {
		*payload1 |= BIT(3);
	} else {
		*payload1 &= ~BIT(3);
	}

	payload2 = dispctrl2_payload;
	payload2++;

	if (panel->lge.dgc_status == 1) {
		*payload2 |= BIT(7);
	} else {
		*payload2 &= ~BIT(7);
	}

	payload3 = dispctrl3_payload;
	payload3++;

	if (panel->lge.sharpness_status == 1) {
		*payload3 |= BIT(0);
	} else {
		*payload3 &= ~BIT(0);
	}

	pr_info("ctrl-command-1: 0x%02x\n", *(++dispctrl1_payload));
	pr_info("ctrl-command-2: 0x%02x 0x%02x\n", *(++dispctrl2_payload), *(++dispctrl3_payload));

	if (send_cmd) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_1);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_CTRL_COMMAND_2);
		if (panel->lge.use_exscreen_control) {
			lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_EXPANDABLE_AREA);
			lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_EXPANDABLE_AREA_ENABLE);
		}
	}

	mutex_unlock(&panel->panel_lock);
	return;
}

static void lge_set_custom_rgb_nt37800a(struct dsi_panel *panel, bool send_cmd)
{
/* To Do :: FIXME */
#if 0
	int i = 0;
	int red_index, green_index, blue_index = 0;
	char *dgctl1_payload = NULL;
	char *dgctl2_payload = NULL;

	if (panel == NULL) {
		pr_err("Invalid input\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	//cm_rgb_step : 0~11
	//rgb_index 0~11 + 0~12
	if (panel->lge.screen_mode == screen_mode_oled_natural) {
		pr_info("Set default rgb step for screen_mode: %d\n", panel->lge.screen_mode);
		red_index   = rgb_preset[panel->lge.cm_preset_step][RED];
		green_index = rgb_preset[panel->lge.cm_preset_step][GREEN];
		blue_index  = rgb_preset[panel->lge.cm_preset_step][BLUE];
	} else {
		red_index   = rgb_preset[panel->lge.cm_preset_step][RED] + panel->lge.cm_red_step;
		green_index = rgb_preset[panel->lge.cm_preset_step][GREEN] + panel->lge.cm_green_step;
		blue_index  = rgb_preset[panel->lge.cm_preset_step][BLUE] + panel->lge.cm_blue_step;
	}

	pr_info("red_index=(%d) green_index=(%d) blue_index=(%d)\n", red_index, green_index, blue_index);

	dgctl1_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_DG_COMMAND_DUMMY, IDX_DG_CTRL1);
	dgctl2_payload = get_payload_addr(panel, LGE_DDIC_DSI_DISP_DG_COMMAND_DUMMY, IDX_DG_CTRL2);

	if (!dgctl1_payload || !dgctl2_payload) {
		pr_err("LGE_DDIC_DSI_DISP_DG_COMMAND_DUMMY is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	/*
	*	RU: RED_UPPER, BU: BLUE_UPPER, GU: GREEN_UPPER
	*	RL: RED_LOWER, BL: BLUE_LOWER, GL: GREEN_LOWER
	*
	* CTRL1(B4h): RU#1~3 GU#1~3 BU#1~3
	* CTRL2(B5h): RL#4~12 GL#4~12 BL#4~12
	*/

	// For RGB UPPER CTRL1
	for (i = 0; i < OFFSET_DG_UPPER; i++) {
		dgctl1_payload[i+START_DG_CTRL1] = dg_ctrl1_values[red_index][i];  //payload_ctrl1[2][3][4]
		dgctl1_payload[i+START_DG_CTRL1+OFFSET_DG_UPPER] = dg_ctrl1_values[green_index][i]; //payload_ctrl1[5][6][7]
		dgctl1_payload[i+START_DG_CTRL1+OFFSET_DG_UPPER*BLUE] = dg_ctrl1_values[blue_index][i]; //payload_ctrl1[8][9][10]
	}

	// FOR RGB LOWER CTRL2
	for (i = 0; i < OFFSET_DG_LOWER; i++) {
		dgctl2_payload[i+START_DG_CTRL2] = dg_ctrl2_values[red_index][i]; //payload_ctrl2[1]~[9]
		dgctl2_payload[i+START_DG_CTRL2+OFFSET_DG_LOWER] = dg_ctrl2_values[green_index][i]; //payload_ctrl2[10]~[18]
		dgctl2_payload[i+START_DG_CTRL2+OFFSET_DG_LOWER*BLUE] = dg_ctrl2_values[blue_index][i]; //payload_ctrl2[19]~[27]
	}

	for (i = 0; i < NUM_DG_CTRL1; i++) {
		pr_debug("Reg:0x%02x [%d:0x%02x]\n", REG_DG_CTRL1, i, dgctl1_payload[i]);
	}

	for (i = 0; i < NUM_DG_CTRL2; i++) {
		pr_debug("Reg:0x%02x [%d:0x%02x]\n", REG_DG_CTRL2, i, dgctl2_payload[i]);
	}

	if (send_cmd) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DISP_DG_COMMAND_DUMMY);
	}

	mutex_unlock(&panel->panel_lock);
#endif
	return;
}

static void lge_set_screen_tune_nt37800a(struct dsi_panel *panel)
{
/* To Do :: FIXME */
	int i;
	int sat_index = 0;
	int hue_index = 0;
	int sha_index = 0;

	char *sat_payload = NULL;
	char *hue_payload = NULL;
	char *sha_payload = NULL;

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
	panel->lge.sharpness_status = !!panel->lge.sc_sha_step;

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

static void set_dgc_status(struct dsi_panel *panel, bool req_state)
{
	int new = (req_state ? 0x01 : 0x00);

	if (panel->lge.hdr_mode || panel->lge.irc_current_state)
		new = 0x00;

	panel->lge.dgc_status = new;
}

static void lge_set_screen_mode_nt37800a(struct dsi_panel *panel, bool send_cmd)
{
	mutex_lock(&panel->panel_lock);

	pr_info("screen_mode %d\n", panel->lge.screen_mode);

	switch (panel->lge.screen_mode) {
	case screen_mode_oled_natural:
		set_dgc_status(panel, true);
		panel->lge.sharpness_status = 0x00;
		pr_info("natural mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_NATURAL);

		mutex_unlock(&panel->panel_lock);
		//lge_set_custom_rgb_nt37800a(panel, send_cmd);
		mutex_lock(&panel->panel_lock);
		break;
	case screen_mode_oled_cinema:
		set_dgc_status(panel, true);
		panel->lge.sharpness_status = 0x00;
		pr_info("cinema mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_CINEMA);
		break;
	case screen_mode_oled_vivid:
		set_dgc_status(panel, true);
		panel->lge.sharpness_status = 0x00;
		pr_info("vivid mode\n");
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_CM_VIVID);
		break;
	case screen_mode_oled_custom:
		pr_info("preset: %d, red: %d, green: %d, blue: %d\n",
				panel->lge.cm_preset_step, panel->lge.cm_red_step,
				panel->lge.cm_green_step, panel->lge.cm_blue_step);
		pr_info("saturation: %d, hue: %d, sharpness: %d(%s)\n",
				panel->lge.sc_sat_step, panel->lge.sc_hue_step, panel->lge.sc_sha_step,
				((!!panel->lge.sc_sha_step) ? "on" : "off"));

		set_dgc_status(panel, true);
		panel->lge.sharpness_status = !!panel->lge.sc_sha_step;

		mutex_unlock(&panel->panel_lock);
		//lge_set_custom_rgb_nt37800a(panel, send_cmd);
		lge_set_screen_tune_nt37800a(panel);
		mutex_lock(&panel->panel_lock);
		break;
	default:
		set_dgc_status(panel, true);
		panel->lge.sharpness_status = 0x00;
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SATURATION_DEFAULT);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_HUE_DEFAULT);
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_WB_DEFAULT);
		break;
	}

	mutex_unlock(&panel->panel_lock);
	lge_display_control_store_nt37800a(panel, send_cmd);
	return;
}

static int lge_hdr_mode_set_nt37800a(struct dsi_panel *panel, int input)
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
		lge_display_control_store_nt37800a(panel, true);
	} else {
		lge_set_screen_mode_nt37800a(panel, true);
	}

	lge_backlight_device_update_status(panel->bl_config.raw_bd);

	return 0;
}

/************** Brightness ********************/
static void lge_update_irc_state(struct dsi_panel *panel, int value)
{
	panel->lge.irc_current_state = value;
	pr_info("current irc state is %d\n", panel->lge.irc_current_state);

	return;
}

int lge_set_irc_state_nt37800a(struct dsi_panel *panel, enum lge_irc_mode mode, enum lge_irc_ctrl enable)
{
	int prev_state = !!panel->lge.irc_current_state;
	int new_state;

	if (!panel) {
		pr_err("panel not exist\n");
		return -EINVAL;
	}
	pr_info("irc request=%s\n", ((enable == LGE_IRC_OFF) ? "off" : "on"));

	mutex_lock(&panel->panel_lock);
	lge_update_irc_state(panel, enable);
	new_state = !!panel->lge.irc_current_state;

	if (prev_state == new_state) {
		pr_info("same state, skip=(%d,%d)\n", prev_state, new_state);
	}

	panel->lge.irc_pending = false;

	if (new_state) {
		set_dgc_status(panel, false);
	} else {
		set_dgc_status(panel, true);
	}
	mutex_unlock(&panel->panel_lock);
	pr_info("irc=%s, dgc=%s\n", (new_state ? "set" : "unset"),
			((panel->lge.dgc_status == 1) ? "enabled" : "disabled"));

	if (new_state) {
		lge_display_control_store_nt37800a(panel, true);
	} else {
		lge_set_screen_mode_nt37800a(panel, true);
	}

	return 0;
}

int lge_get_irc_state_nt37800a(struct dsi_panel *panel)
{
	int ret;

	mutex_lock(&panel->panel_lock);
	pr_info("current_state=%d\n", panel->lge.irc_current_state);
	ret = !!panel->lge.irc_current_state;
	mutex_unlock(&panel->panel_lock);
	return ret;
}

int lge_set_irc_default_state_nt37800a(struct dsi_panel *panel)
{
	lge_update_irc_state(panel, LGE_IRC_ON);
	return 0;
}

static void lge_bc_dim_set_nt37800a(struct dsi_panel *panel, u8 bc_dim_en, u8 bc_dim_f_cnt)
{
	char *payload = NULL;
	struct expandable_handle *h = NULL;
	bool dim_updated = false;

	mutex_lock(&panel->panel_lock);
	payload = get_payload_addr(panel, LGE_DDIC_DSI_BC_DEFAULT_DIMMING, 2);
	if (!payload) {
		mutex_unlock(&panel->panel_lock);
		return;
	}

	payload[1] = payload[2] = bc_dim_f_cnt;

	h = to_exp_handle(panel);
	if (h && h->switch_dimming_param) {
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_BC_DEFAULT_DIMMING);
		panel->lge.bc_dim_en = bc_dim_en;
		dim_updated = true;
	}
	mutex_unlock(&panel->panel_lock);

	if (h && h->switch_dimming_param &&
			(bc_dim_f_cnt == BC_DIM_FRAMES_NORMAL)) {
		h->switch_dimming_param = false;
		dim_updated = true;
	}

	if (dim_updated) {
		lge_display_control_store_nt37800a(panel, true);
		mdelay(15);
		pr_info("updated : dimming %s\n", h->switch_dimming_param ? "On" : "Off");
	}
}

static int lge_set_therm_dim_nt37800a(struct dsi_panel *panel, int input)
{
	u8 bc_dim_f_cnt;

	if (panel->bl_config.raw_bd->props.brightness < BC_DIM_BRIGHTNESS_THERM) {
		pr_info("Normal Mode. Skip therm dim. Current Brightness: %d\n", panel->bl_config.raw_bd->props.brightness);
		return -EINVAL;
	}

	if (input)
		bc_dim_f_cnt = BC_DIM_FRAMES_THERM;
	else
		bc_dim_f_cnt = BC_DIM_FRAMES_NORMAL;

	if (panel->lge.use_bc_dimming_work)
		cancel_delayed_work_sync(&panel->lge.bc_dim_work);

	lge_bc_dim_set_nt37800a(panel, BC_DIM_ON, bc_dim_f_cnt);

	panel->bl_config.raw_bd->props.brightness = BC_DIM_BRIGHTNESS_THERM;
	lge_backlight_device_update_status(panel->bl_config.raw_bd);

	if (panel->lge.use_bc_dimming_work)
		schedule_delayed_work(&panel->lge.bc_dim_work, BC_DIM_TIME);

	return 0;
}

static int lge_get_brightness_dim_nt37800a(struct dsi_panel *panel)
{
	u8 bc_dim_f_cnt;
	char *payload = NULL;

	payload = get_payload_addr(panel, LGE_DDIC_DSI_BC_DEFAULT_DIMMING, 2);
	if (payload) {
		payload++;
		bc_dim_f_cnt = *payload;
	} else {
		pr_err("bc_dim_f_cnt payload is not available\n");
		return -EINVAL;
	}
	return bc_dim_f_cnt;
}

static void lge_set_brightness_dim_nt37800a(struct dsi_panel *panel, int input)
{
	u8 enable = 0;
	u8 dim_frame = 0;

	if (input == BC_DIM_OFF) {
		enable = BC_DIM_OFF;
		dim_frame = input;
	} else {
		enable = BC_DIM_ON;
		if (input > BC_DIM_MAX_FRAMES)
			dim_frame = BC_DIM_MAX_FRAMES;
		else
			dim_frame = input;
	}

	lge_bc_dim_set_nt37800a(panel, enable, dim_frame);
}

static void lge_vr_lp_mode_set_nt37800a(struct dsi_panel *panel, int input)
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

/************** Factory ********************/
void lge_check_vert_black_line_nt37800a(struct dsi_panel *panel)
{
	int rc = 0;

	mutex_lock(&panel->panel_lock);
	pr_info("send LGE_DDIC_DSI_DETECT_BLACK_VERT_LINE\n");
	rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DETECT_BLACK_VERT_LINE);
	mutex_unlock(&panel->panel_lock);

	if (rc)
		pr_err("failed to send DETECT_BLACK_VERT_LINE cmd, rc=%d\n", rc);
}

void lge_check_vert_white_line_nt37800a(struct dsi_panel *panel)
{
	int rc = 0;

	mutex_lock(&panel->panel_lock);
	pr_info("send LGE_DDIC_DSI_DETECT_WHITE_VERT_LINE\n");
	rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DETECT_WHITE_VERT_LINE);
	mutex_unlock(&panel->panel_lock);

	if (rc)
		pr_err("failed to send DETECT_WHITE_VERT_LINE cmd, rc=%d\n", rc);
}

void lge_check_vert_line_restore_nt37800a(struct dsi_panel *panel)
{
	int rc = 0;

	pr_info("%s\n", __func__);

	mutex_lock(&panel->panel_lock);
	pr_info("send LGE_DDIC_DSI_DETECT_VERT_LINE_RESTORE");
	rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_DETECT_VERT_LINE_RESTORE);
	mutex_unlock(&panel->panel_lock);

	if (rc)
		pr_err("failed to send DETECT_VERT_LINE_RESTORE cmd, rc=%d\n", rc);
}

static void lge_set_tc_perf_nt37800a(struct dsi_panel *panel, int input)
{
	bool tc_perf_mode = ((input > 0) ? true : false);

	mutex_lock(&panel->panel_lock);
	if (tc_perf_mode)
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_TC_PERF_ON_COMMAND);
	else
		lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_TC_PERF_OFF_COMMAND);
	mutex_unlock(&panel->panel_lock);

	pr_info("set tc perf %s\n", (tc_perf_mode) ? "enable" : "disable");
}

static void set_saturation_level(struct dsi_panel *panel, int input)
{
	bool enable = false;
	int idx = 0, i = 0;
	char *sat_payload = NULL;
	int target_idx = 4; /* received from IQTask */

	mutex_lock(&panel->panel_lock);
	sat_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_SATURATION, 1);
	if (!sat_payload) {
		pr_err("LGE_DDIC_DSI_SET_SATURATION is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	idx = ((input > 0) ? target_idx : panel->lge.sc_sat_step);
	enable = ((input > 0) ? true : false);

	if (enable) {
		set_dgc_status(panel, true);
		for (i = 0; i < OFFSET_SAT_CTRL; i++) {
			sat_payload[i+1] = sat_ctrl_values[idx][i];
		}
	}

	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SATURATION);
	mutex_unlock(&panel->panel_lock);

	if (!enable)
		lge_set_screen_mode_nt37800a(panel, true);

	pr_info("inc_saturation : %s , sat_lvl=%d\n",
			(enable ? "enabled" : "disabled"), idx);
}

static bool set_expandable_mode_sub(struct dsi_panel *panel, struct expandable_handle *h)
{
	bool updated = false, updated_roi = false, updated_gate = false;
	int mode = panel->lge.expandable_mode;

	mutex_lock(&h->expandable_lock);
	updated |= update_expandable_power_state(h, mode);
	updated_gate = update_expandable_disable_gate(h, mode);
	updated |= updated_gate;
	updated_roi = update_expandable_roi(h);
	updated |= updated_roi;
	mutex_unlock(&h->expandable_lock);

	if (updated) {
		int prev_hl_mode = panel->lge.hl_mode;
		panel->lge.hl_mode = update_hl_mode(h);
		if (panel->lge.hl_mode != prev_hl_mode) {
			if (panel->lge.hl_mode == 1)
				h->switch_dimming_param = true;

			if (h->switch_dimming_param) {
				if (panel->lge.use_bc_dimming_work)
					cancel_delayed_work_sync(&panel->lge.bc_dim_work);
				if (panel->lge.use_dim_ctrl)
					lge_bc_dim_set_nt37800a(panel, BC_DIM_ON, (BC_DIM_FRAMES_VE >> 1));
			}

			set_saturation_level(panel, panel->lge.hl_mode);
			lge_backlight_device_update_status(panel->bl_config.raw_bd);

			if (h->switch_dimming_param && panel->lge.use_bc_dimming_work)
				schedule_delayed_work(&panel->lge.bc_dim_work, BC_DIM_TIME);
		}

		if (updated_gate)
			update_expandable_gate(panel, h);

		if (updated_roi) {
			update_expandable_gate(panel, h);
			update_expandable_area(panel, h);
		}
	}

	return (updated_gate | updated_roi | h->switch_dimming_param);
}

int lge_get_hl_mode_nt37800a(struct dsi_panel *panel)
{
	return panel->lge.hl_mode;
}

void lge_set_hl_mode_pending_nt37800a(struct dsi_panel *panel, int mode)
{
	if (!panel) {
		pr_err("null ptr\n");
		return;
	}

	if ((mode == -1) && (panel->lge.expandable_pending)) {
		panel->lge.expandable_pending = false;
		panel->lge.expandable_pending_mode = 0;

		pr_info("clear pending\n");
		return;
	}

	panel->lge.expandable_pending = true;
	panel->lge.expandable_pending_mode = mode;

	pr_info("pending store = %d\n", mode);
	return;
}

int lge_get_hl_mode_pending_nt37800a(struct dsi_panel *panel)
{
	int bit_mask = 0xFF;

	if (!panel) {
		pr_err("null ptr\n");
		return 0;
	}

	bit_mask = (panel->lge.expandable_pending ? 0xFF : 0x00);

	return (panel->lge.expandable_pending_mode & bit_mask);
}

void lge_set_hl_mode_nt37800a(struct dsi_panel *panel, int mode)
{
	struct expandable_handle *h = to_exp_handle(panel);
	bool updated = false;

	if (!h) {
		pr_err("not initialized\n");
		return;
	};

	panel->lge.expandable_mode = mode;
	pr_err("get mode=%d\n", panel->lge.expandable_mode);

	_prepare_dic_power_state(panel, EXPANDABLE_FRONT);
	_prepare_dic_power_state(panel, EXPANDABLE_REAR);

	updated = set_expandable_mode_sub(panel, h);

	if (updated)
		lge_display_control_store_nt37800a(panel, true);
}

void lge_set_exscreen_width_nt37800a(struct dsi_panel *panel, int value, bool is_target)
{
	struct expandable_handle *h = to_exp_handle(panel);
	int i;

	if (!h) {
		pr_err("not initialized\n");
		return;
	};

	for (i = 0; i < ENABLE_MAX; i++) {
		pr_err("compare %d, %d\n", value, nt37800a_res[i].height);
		if (value == nt37800a_res[i].height) {
			break;
		}
	}

	if (i == ENABLE_MAX) {
		pr_err("abnormal value\n");
		return;
	}

	if (is_target)
		h->cur_width_id = i;
	else
		h->req_width_id = i;

	set_expandable_mode_sub(panel, h);

	lge_display_control_store_nt37800a(panel, true);
}

static void lge_set_video_enhancement_nt37800a(struct dsi_panel *panel, int input)
{
	bool enable = false;
	int idx = 0, i = 0;
	char *sat_payload = NULL;
	int target_idx = 3; /* received from IQTask */

	mutex_lock(&panel->panel_lock);
	sat_payload = get_payload_addr(panel, LGE_DDIC_DSI_SET_SATURATION, 1);
	if (!sat_payload) {
		pr_err("LGE_DDIC_DSI_SET_SATURATION is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	idx = ((input > 0) ? target_idx : panel->lge.sc_sat_step);
	enable = ((input > 0) ? true : false);

	if (enable) {
		set_dgc_status(panel, true);
		for (i = 0; i < OFFSET_SAT_CTRL; i++) {
			sat_payload[i+1] = sat_ctrl_values[idx][i];
		}
	}

	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_SATURATION);
	mutex_unlock(&panel->panel_lock);

	if (enable)
		lge_display_control_store_nt37800a(panel, true);
	else
		lge_set_screen_mode_nt37800a(panel, true);


	pr_info("video_enhancement : %s , sat_lvl=%d\n",
			(enable ? "enabled" : "disabled"), idx);

	/* lge_backlight_device_update_status(panel->bl_config.raw_bd); */ // SKIP
}

/* To Do :: FIXME */
#if 0
static void lge_irc_control_store_nt37800a(struct dsi_panel *panel, bool enable)
{
	char *payload = NULL;

	if (!panel) {
		pr_err("panel not exist\n");
		return;
	}

	mutex_lock(&panel->panel_lock);

	payload = get_payload_addr(panel, LGE_DDIC_DSI_IRC_CTRL, 1);
	if (!payload) {
		pr_err("LGE_DDIC_DSI_IRC_CTRL is NULL\n");
		mutex_unlock(&panel->panel_lock);
		return;
	}

	payload[1] &= 0x60;
	panel->lge.irc_current_state = enable;
	payload[1] |= panel->lge.irc_current_state;

	pr_info("irc-command: 0x%02x", payload[1]);

	lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_IRC_CTRL);

	mutex_unlock(&panel->panel_lock);
}

static void lge_set_brighter_nt37800a(struct dsi_panel *panel, int input)
{
	int rc;
	bool brighter_mode = ((input > 0) ? true : false);

	pr_info("brighter mode = %s\n", (brighter_mode? "set" : "unset"));

	if (brighter_mode){
		lge_irc_control_store_nt37800a(panel, false);
		mutex_lock(&panel->panel_lock);
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_BRIGHTER_MODE_ON);
		mutex_unlock(&panel->panel_lock);
	} else {
		lge_irc_control_store_nt37800a(panel, true);
		mutex_lock(&panel->panel_lock);
		rc = lge_ddic_dsi_panel_tx_cmd_set(panel, LGE_DDIC_DSI_SET_BRIGHTER_MODE_OFF);
		mutex_unlock(&panel->panel_lock);
		lge_set_screen_mode_nt37800a(panel, true);
	}
}
#endif

struct lge_ddic_ops nt37800a_ops = {
	/* AoD */
	.store_aod_area = store_aod_area,
	.prepare_aod_cmds = prepare_aod_cmds_nt37800a,
	.prepare_aod_area = NULL,
	.lge_check_vert_black_line = lge_check_vert_black_line_nt37800a,
	.lge_check_vert_white_line = lge_check_vert_white_line_nt37800a,
	.lge_check_vert_line_restore = lge_check_vert_line_restore_nt37800a,
	/* brightness */
	.init_hl_mode = lge_init_hl_mode_nt37800a,
	.set_hl_mode_pending = lge_set_hl_mode_pending_nt37800a,
	.get_hl_mode_pending = lge_get_hl_mode_pending_nt37800a,
	.set_hl_mode = lge_set_hl_mode_nt37800a,
	.get_hl_mode = lge_get_hl_mode_nt37800a,
	.set_exscreen_width = lge_set_exscreen_width_nt37800a,
	.lge_bc_dim_set = lge_bc_dim_set_nt37800a,
	.lge_set_therm_dim = lge_set_therm_dim_nt37800a,
	.lge_get_brightness_dim = lge_get_brightness_dim_nt37800a,
	.lge_set_brightness_dim = lge_set_brightness_dim_nt37800a,
	.daylight_mode_set = NULL,
	/* image quality */
	.hdr_mode_set = lge_hdr_mode_set_nt37800a,
	.lge_set_custom_rgb = lge_set_custom_rgb_nt37800a,
	.lge_display_control_store = lge_display_control_store_nt37800a,
	.lge_set_screen_tune = lge_set_screen_tune_nt37800a,
	.lge_set_screen_mode = lge_set_screen_mode_nt37800a,
	.sharpness_set = NULL,
	.lge_set_true_view_mode = NULL,
	.lge_set_video_enhancement = lge_set_video_enhancement_nt37800a,
	.lge_vr_lp_mode_set = lge_vr_lp_mode_set_nt37800a,
	.lge_set_tc_perf = lge_set_tc_perf_nt37800a,

	/* drs */
	.get_current_res = NULL,
	.get_support_res = NULL,
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
	.set_irc_default_state = lge_set_irc_default_state_nt37800a,
	.set_irc_state = lge_set_irc_state_nt37800a,
	.get_irc_state = lge_get_irc_state_nt37800a,
	/* lhbm */
	.lge_set_fp_lhbm = NULL,
};
