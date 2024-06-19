/* touch_sw82906.c
 *
 * Copyright (C) 2015 LGE.
 *
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
#define TS_MODULE "[sw82906]"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>

/*
 *  Include to touch core Header File
 */
#include <touch_core.h>
#include <touch_hwif.h>
#include <touch_common.h>

/*
 *  Include to Local Header File
 */
#include "touch_sw82906.h"
#include "touch_sw82906_prd.h"

#if defined(__SUPPORT_ABT)
#include "touch_sw82906_abt.h"
#endif
static void sw82906_lcd_mode(struct device *dev, u32 mode);
static int sw82906_power(struct device *dev, int ctrl);
static int sw82906_init(struct device *dev);
static int sw82906_set_display_area_status(struct device *dev, int display_area_id);
static int sw82906_set_display_area_split_info(struct device *dev, u8 type_display_area, u8 display_area_id);
static int __used sw82906_fw_flash_crc(struct device *dev, u32 *crc_val);
static void sw82906_set_lpwg_failreason(struct device *dev, int type, bool enable);

static void project_param_set(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	struct project_param *param = &d->p_param;

	TOUCH_I("%s\n", __func__);
	/*
	 *	wa : work around code
	 *	dfc : defence code
	 *	sys : select system mode
	 *	eg : engine control
	 */
	param->wa_trim_wr = FUNC_OFF;
	/* reset on suspend (U3->U2,U0) */
	param->touch_power_control_en = FUNC_ON;

	/* ic bus test */
	param->dfc_bus_test = FUNC_ON;

	/* flash_fw_size = F/W IMAGE SIZE - CFG SIZE */
	param->flash_fw_size = (128 * 1024);

	/* resume reset select (power reset or touch reset or nothing) */
	param->dfc_resume_power_ctl = RESUME_CTRL_RESET;

	param->sys_driving_ctrl = U3_MODE_1_2_SWING;
	param->sys_tc_stop_delay = 30;
	param->sys_tc_start_delay = 5;

	param->reg_debug = FUNC_OFF;
	param->dynamic_reg = FUNC_ON;

	/* Read flexible I2C data when N-finger come in.  */
	param->flex_report = FUNC_ON;

	/* siw swipe resister control (1:left, 2:right, 3:up, 4:down) */

	param->used_mode = (1<<LCD_MODE_U0)|(1<<LCD_MODE_U3)|
			(1<<LCD_MODE_U3_PARTIAL)|(1<<LCD_MODE_U2)|(1<<LCD_MODE_STOP);
}

#define TOUCH_NOOP()	\
	TOUCH_I("noop - %s\n", __func__)

#define LPWG_FAILREASON_KNOCK_ON_NUM 10
static const char * const __used lpwg_failreason_knock_on_str[LPWG_FAILREASON_KNOCK_ON_NUM] = {
	[0] = "SUCCESS",
	[1] = "DISTANCE_INTER_TAP",
	[2] = "DISTANCE_TOUCHSLOP",
	[3] = "MINTIMEOUT_INTER_TAP",
	[4] = "MAXTIMEOUT_INTER_TAP",
	[5] = "LONGPRESS_TIME_OUT",
	[6] = "MULTI_FINGER",
	[7] = "DELAY_TIME", /* Over Tap */
	[8] = "PALM_STATE",
	[9] = "OUTOF_AREA",
};

#define LPWG_FAILREASON_SWIPE_NUM 11
static const char * const __used lpwg_failreason_swipe_str[LPWG_FAILREASON_SWIPE_NUM] = {
	[0] = "ERROR",
	[1] = "FINGER_FAST_RELEASE",
	[2] = "MULTI_FINGER",
	[3] = "FAST_SWIPE",
	[4] = "SLOW_SWIPE",
	[5] = "WRONG_DIRECTION",
	[6] = "RATIO_FAIL",
	[7] = "OUT_OF_START_AREA",
	[8] = "OUT_OF_ACTIVE_AREA",
	[9] = "INITAIL_RATIO_FAIL",
	[10] = "PALM_STATE",
};

#define LPWG_FAILREASON_LONGPRESS_NUM 6
static const char * const __used lpwg_failreason_longpress_str[LPWG_FAILREASON_LONGPRESS_NUM] = {
	[0] = "SUCCESS",
	[1] = "MULTI_FINGER",
	[2] = "TOUCH_FAST_RELEASE",
	[3] = "TOUCH_SLOPE_FAIL",
	[4] = "OUT_OF_AREA",
	[5] = "PALM_STATE",
};

#define LPWG_FAILREASON_ONETAP_NUM 7
static const char * const __used lpwg_failreason_onetap_str[LPWG_FAILREASON_ONETAP_NUM] = {
	[0] = "SUCCESS",
	[1] = "MULTI_FINGER",
	[2] = "TOUCH_SLOPE_FAIL",
	[3] = "MAX_PRESS_TIME",
	[4] = "INTERRUPT_DELAY_TIME",
	[5] = "OUT_OF_AREA",
	[6] = "PALM_STATE",
};

#define IC_STATUS_INFO_NUM 5
static const int __used ic_status_info_idx[IC_STATUS_INFO_NUM] = {1, 2, 3, 4, 5};
static const char __used *ic_status_info_str[32] = {
	[1] = "Boot Up CRC Fail",
	[2] = "Not occur tch_attn interrupt from system",
	[3] = "WatchDog Time out",
	[4] = "Combined int",
	[5] = "CM3 fault Error",
};

#define TC_STATUS_INFO_NUM 9
static const int __used tc_status_info_idx[TC_STATUS_INFO_NUM] = {5, 6, 7, 9, 10, 15, 20, 22, 28};
static const char __used *tc_status_info_str[32] = {
	[5] = "Device Check Failed",
	[6] = "Code CRC Invalid",
	[9] = "Abnormal Status Detected",
	[10] = "ESD System Error Detected",
	[15] = "Low Active Interrupt Pin is High",
	[20] = "Touch Interrupt Disabled",
	[22] = "TC Driving Invalid",
};

static const char __used *lpwg_failreason_type_str[LPWG_DEBUG_NUM] = {
	[0] = "KNOCK_ON_FRONT",
	[1] = "KNOCK_ON_REAR",
	[2] = "SWIPE",
	[3] = "LONGPRESS",
	[4] = "ONETAP",
};

static const char __used *knock_on_cmd_str[] = {
	"ENABLE_CTRL",
	"TAP_COUNT_CTRL",
	"MIN_INTERTAP_CTRL",
	"MAX_INTERTAP_CTRL",
	"TOUCH_SLOP_CTRL",
	"TAP_DISTANCE_CTRL",
	"INTERRUPT_DELAY_CTRL",
	"ACTIVE_AREA_CTRL",
	"ACTIVE_AREA_RESET_CTRL",
};

#define DEBUG_INFO_NUM 32
static const char __used *debug_info_str[DEBUG_INFO_NUM] = {
	[0] = "NONE",
	[1] = "DBG_TG_FAULT",
	[2] = "DBG_ESD_FAULT",
	[3] = "DBG_WATDOG_TIMEOUT",
	[4] = "DBG_TC_DRV_MISMATCH",
	[5] = "DBG_TC_INVALID_TIME_DRV_REQ",
	[6] = "DBG_AFE_TUNE_FAIL",
	[7] = "DBG_DBG_MSG_FULL",
	[8] = "DBG_PRE_MA_OVF_ERR",
	[9] = "DBG_ADC_OVF_ERR",
	[10] = "DBG_CM3_FAULT",			// 0x0A
	[11] = "DBG_UNKNOWN_TEST_MSG [0x0B]",	// 0x0B
	[12] = "DBG_FLASH_EDTECT_ERR",		// 0x0C
	[13] = "DBG_MEM_ACCESS_ISR",		// 0x0D
	[14] = "DBG_DISPLAY_CHANGE_IRQ",	// 0x0E
	[15] = "DBG_PT_CHKSUM_ERR",		// 0x0F
	[16] = "DBG_UNKNOWN_CMD",		// 0x10
	[17] = "DBG_TE_FREQ_REPORT",		// 0x11
	[18] = "DBG_STACK_OVERFLOW_ERR",	// 0x12
	[19] = "DBG_ABNORMAL_ACCESS_ERR",	// 0x13
	[20] = "DBG_UNKNOWN_TEST_MSG [0x14]",	// 0x14
	[21] = "DBG_UNKNOWN_TEST_MSG [0x15]",	// 0x15
	[22] = "DBG_CG_CTL_INT",		// 0x16
	[23] = "DBG_DCS_IRQ",			// 0x17
	[24] = "DBG_DISPLAY_IRQ",		// 0x18
	[25] = "DBG_USER1_IRQ",			// 0x19
	[26] = "DBG_CMD_Q_FULL",		// 0x1A
	[27] = "DBG_TC_DRV_START_SKIP",		// 0x1B
	[28] = "DBG_TC_DRV_CMD_INVALID",	// 0x1C
	[29] = "DBG_UNKNOWN_TEST_MSG [0x1D]",	// 0x1D
	[30] = "DBG_CFG_S_IDX",			// 0x1E
	[31] = "DBG_UNKNOWN_TEST_MSG [0x1F]",	// 0x1F
};

static const char __used *lpwg_function_str[] = {
	"LPWG_LG_PAY",
	"LPWG_QUICK_TOOL",
	"LPWG_AI_PICK",
	"LPWG_UDF",
	"LPWG_SHOW_AOD",
	"LPWG_FAKE_OFF"
};

static const char __used *lpwg_gesture_str[] = {
	"ONE_TAP",
	"TWO_TAP",
	"THREE_TAP",
	"DOUBLE_TAP",
	"DOUBLE_TWO_TAP",
	"DOUBLE_THREE_TAP",
	"LONG_PRESS",
	"SWIPE",
	"SWIPE_UP",
	"SWIPE_RIGHT",
	"SWIPE_DOWN",
	"SWIPE_LEFT",
	"ABS_COORDINATE"
};

static const char __used *moving_side_str[] = {
	"UP_SIDE",
	"RIGHT_SIDE",
	"DOWN_SIDE",
	"LEFT_SIDE"
};

static const char __used *moving_state_str[] = {
	"STOP",
	"START",
	"MOVING"
};

static const char __used *moving_direction_str[] = {
	"INACTIVE",
	"ROLL",
	"UNROLL",
	"FOLD",
	"UNFOLD"
};

void sw82906_xfer_msg_ready(struct device *dev, u8 msg_cnt)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);

	mutex_lock(&d->io_lock);

	ts->xfer->msg_count = msg_cnt;
}

int sw82906_xfer_msg(struct device *dev, struct touch_xfer_msg *xfer)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	struct touch_xfer_data_t *tx = NULL;
	struct touch_xfer_data_t *rx = NULL;
	int addr_check = 0;
	int buf_cnt = 0;
	int r_hdr_size = R_HEADER_SIZE_I2C;
	int w_hdr_size = W_HEADER_SIZE_I2C;
	int ret = 0;
	int i = 0;

	for (i = 0; i < xfer->msg_count; i++) {
		buf_cnt = 0;
		tx = &xfer->data[i].tx;
		rx = &xfer->data[i].rx;
		if (rx->addr >= command_start_addr && rx->addr
				<= command_end_addr)
			addr_check = 1;
		else
			addr_check = 0;

		if (rx->size) {
			if (addr_check == 1)
				tx->data[0] = ((rx->size > 4) ? 0x30 : 0x10);
			else
				tx->data[0] = ((rx->size > 4) ? 0x20 : 0x00);

			tx->data[buf_cnt++] |= ((rx->addr >> 8) & 0x0f);
			tx->data[buf_cnt++] = (rx->addr & 0xff);
			tx->data[buf_cnt++] = 0;
			tx->data[buf_cnt++] = 0;
			tx->data[buf_cnt++] = 0;
			tx->data[buf_cnt++] = 0;

			if (addr_check == 1) {
				for (; buf_cnt < 18;)
					tx->data[buf_cnt++] = 0;
			}

			tx->size = buf_cnt;
			rx->size += r_hdr_size;
		} else {
			if (tx->size > (MAX_XFER_BUF_SIZE - w_hdr_size)) {
				TOUCH_E("buffer overflow\n");
				ret = -EOVERFLOW;
				goto error;

			}

			tx->data[0] = (tx->size > 4) ? 0x60 : 0x40;
			tx->data[0] |= ((tx->addr >> 8) & 0x0f);
			tx->data[1] = (tx->addr  & 0xff);
			memcpy(&tx->data[w_hdr_size], tx->buf, tx->size);
			tx->size += w_hdr_size;
		}
	}

	ret = touch_bus_xfer(dev, xfer);
	if (ret) {
		TOUCH_E("touch bus error : %d\n", ret);
		goto error;
	}

	for (i = 0; i < xfer->msg_count; i++) {
		rx = &xfer->data[i].rx;

		if (rx->size) {
			memcpy(rx->buf, rx->data + r_hdr_size,
					(rx->size - r_hdr_size));
		}
	}

error:
	for (i = 0; i < xfer->msg_count; i++) {
		rx = &xfer->data[i].rx;
		if (rx->size)
			rx->size = 0;
	}

	mutex_unlock(&d->io_lock);

	return ret;
}

int sw82906_reg_read(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct touch_bus_msg msg = {0, };
	int buf_cnt = 0;
	int r_hdr_size = R_HEADER_SIZE_I2C;
	int w_hdr_size = W_HEADER_SIZE_I2C;
	int ret = 0;

#if defined(CONFIG_SECURE_TOUCH)
	if (atomic_read(&ts->st_enabled)) {
		TOUCH_E("cannot use i2c, ownership changed!\n");
		return ret;
	}
#endif
	mutex_lock(&d->io_lock);

	if (d->p_param.reg_debug) {
		if (!(addr == 0x600) && (size >= 12))
			TOUCH_I(">> [R] %Xh\n", addr);
	}

	if (ts->bus_type == HWIF_SPI) {
		r_hdr_size = R_HEADER_SIZE_SPI;
		w_hdr_size = W_HEADER_SIZE_SPI;
	}

	ts->tx_buf[0] = ((size > 4) ? 0x20 : 0x00);
	ts->tx_buf[buf_cnt++] |= ((addr >> 8) & 0x0f);
	ts->tx_buf[buf_cnt++] = (addr & 0xff);
	ts->tx_buf[buf_cnt++] = 0;
	ts->tx_buf[buf_cnt++] = 0;
	ts->tx_buf[buf_cnt++] = 0;
	ts->tx_buf[buf_cnt++] = 0;

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = w_hdr_size;
	msg.rx_buf = ts->rx_buf;
	msg.rx_size = r_hdr_size + size;

	ret = touch_bus_read(dev, &msg);

	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
		mutex_unlock(&d->io_lock);
		return ret;
	}

	memcpy(data, &ts->rx_buf[r_hdr_size], size);
	mutex_unlock(&d->io_lock);
	return 0;
}

int sw82906_reg_write(struct device *dev, u16 addr, void *data, int size)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct touch_bus_msg msg = {0, };
	int w_hdr_size = W_HEADER_SIZE_I2C;	/* default for i2c & spi */
	int ret = 0;

#if defined(CONFIG_SECURE_TOUCH)
	if (atomic_read(&ts->st_enabled)) {
		TOUCH_E("cannot use i2c, ownership changed!\n");
		return ret;
	}
#endif
	mutex_lock(&d->io_lock);

	if (d->p_param.reg_debug) {
		TOUCH_I(">> [W] %Xh\n", addr);
	}

	ts->tx_buf[0] = ((size > 4) ? 0x60 : 0x40);

	ts->tx_buf[0] |= ((addr >> 8) & 0x0f);
	ts->tx_buf[1] = (addr  & 0xff);

	msg.tx_buf = ts->tx_buf;
	msg.tx_size = w_hdr_size + size;
	msg.rx_buf = NULL;
	msg.rx_size = 0;

	memcpy(&ts->tx_buf[w_hdr_size], data, size);

	ret = touch_bus_write(dev, &msg);
	mutex_unlock(&d->io_lock);

	if (ret < 0) {
		TOUCH_E("touch bus error : %d\n", ret);
		return ret;
	}

	return 0;
}

#if defined(CONFIG_DRM_MSM) && defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
static int sw82906_drm_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
#if 0
	struct msm_drm_notifier *ev = (struct msm_drm_notifier *)data;

	TOUCH_TRACE();

	if (ev && ev->data && event == MSM_DRM_EVENT_BLANK) {
		int *blank = (int *)ev->data;

		if (*blank == MSM_DRM_BLANK_UNBLANK)
			TOUCH_I("DRM_UNBLANK\n");
		else if (*blank == MSM_DRM_BLANK_POWERDOWN)
			TOUCH_I("DRM_POWERDOWN\n");
	}
#endif
	return 0;
}
#endif
#elif defined(CONFIG_FB)
static int sw82906_fb_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fb_event *ev = (struct fb_event *)data;

	TOUCH_TRACE();

	if (ev && ev->data && event == FB_EVENT_BLANK) {
		int *blank = (int *)ev->data;

		if (*blank == FB_BLANK_UNBLANK)
			TOUCH_I("FB_UNBLANK\n");
		else if (*blank == FB_BLANK_POWERDOWN)
			TOUCH_I("FB_BLANK\n");
	}

	return 0;
}
#endif

static int sw82906_sw_reset(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;

	TOUCH_I("%s : SW Reset(mode%d)\n", __func__, mode);

	if (mode == SW_RESET) {
		/* [CM3 reset] Touch F/W jump reset vector and do not code flash dump */
		sw82906_write_value(dev, SYS_RST_CTL, 2);
		touch_msleep(20);
		sw82906_write_value(dev, SYS_RST_CTL, 0);
	} else if (mode == SW_RESET_CODE_DUMP) {
		/* [system reset] Touch F/W resister reset and do code flash dump */
		sw82906_write_value(dev, SYS_RST_CTL, 1);
		touch_msleep(20);
		sw82906_write_value(dev, SYS_RST_CTL, 0);
	} else {
		TOUCH_E("%s Invalid SW reset mode!!\n", __func__);
	}

	atomic_set(&d->init, IC_INIT_NEED);
	d->driving_mode = LCD_MODE_UNKNOWN;

	queue_delayed_work(ts->wq, &ts->init_work, msecs_to_jiffies(ts->caps.sw_reset_delay));

	return ret;
}

int sw82906_hw_reset(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);

	TOUCH_I("%s : HW Reset(mode:%d)\n", __func__, mode);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	touch_gpio_direction_output(ts->reset_pin, 0);

	touch_msleep(1);

	touch_gpio_direction_output(ts->reset_pin, 1);
	atomic_set(&d->init, IC_INIT_NEED);
	atomic_set(&ts->state.sleep, IC_NORMAL);
	d->driving_mode = LCD_MODE_UNKNOWN;

	TOUCH_I("hw_reset_delay:%dms", ts->caps.hw_reset_delay);

	if (mode == HW_RESET_ASYNC) {
		queue_delayed_work(ts->wq, &ts->init_work, msecs_to_jiffies(ts->caps.hw_reset_delay));
	} else if (mode == HW_RESET_SYNC) {
		touch_msleep(ts->caps.hw_reset_delay);
		ts->driver->init(dev);
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	} else {
		TOUCH_E("%s Invalid HW reset mode!!\n", __func__);
	}

	return 0;
}

int sw82906_only_hw_reset(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);

	TOUCH_I("%s : ONLY HW Reset control\n", __func__);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	touch_gpio_direction_output(ts->reset_pin, 0);
	touch_msleep(1);

	touch_gpio_direction_output(ts->reset_pin, 1);
	touch_msleep(ts->caps.hw_reset_delay);

	atomic_set(&d->init, IC_INIT_NEED);
	atomic_set(&ts->state.sleep, IC_NORMAL);
	d->driving_mode = LCD_MODE_UNKNOWN;

	return 0;
}

int sw82906_power_reset(struct device *dev, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);

	TOUCH_I("%s : HW POWER Reset(mode:%d)\n", __func__, mode);
	touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
	sw82906_power(ts->dev, POWER_OFF);
	sw82906_power(ts->dev, POWER_ON);

	atomic_set(&d->init, IC_INIT_NEED);
	atomic_set(&ts->state.sleep, IC_NORMAL);
	d->driving_mode = LCD_MODE_UNKNOWN;

	TOUCH_I("hw_reset_delay:%dms", ts->caps.hw_reset_delay);

	touch_msleep(ts->caps.hw_reset_delay);
	ts->driver->init(dev);
	touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);

	return 0;
}

int sw82906_reset_ctrl(struct device *dev, int ctrl)
{
	int ret = 0;

	TOUCH_TRACE();

	switch (ctrl) {
	case SW_RESET:
	case SW_RESET_CODE_DUMP:
		ret = sw82906_sw_reset(dev, ctrl);
		break;

	case HW_RESET_ASYNC:
	case HW_RESET_SYNC:
		ret = sw82906_hw_reset(dev, ctrl);
		break;
	case HW_RESET_ONLY:
		ret = sw82906_only_hw_reset(dev);
		break;
	case HW_RESET_POWER:
		ret = sw82906_power_reset(dev, ctrl);
		break;
	default:
		TOUCH_E("UnKnown ctrl, %d\n", ctrl);
		break;
	}

	return ret;
}

static int sw82906_power(struct device *dev, int ctrl)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct project_param *param = &d->p_param;

	TOUCH_TRACE();

	switch (ctrl) {
	case POWER_OFF:
#if defined(CONFIG_SECURE_TOUCH)
		if (atomic_read(&ts->st_enabled))
			secure_touch_stop(ts, true);
#endif
		if (param->touch_power_control_en == FUNC_ON) {
			TOUCH_I("%s, off\n", __func__);
			atomic_set(&d->init, IC_INIT_NEED);
			atomic_set(&ts->state.sleep, IC_DEEP_SLEEP);
			d->driving_mode = LCD_MODE_UNKNOWN;
			touch_gpio_direction_output(ts->reset_pin, 0);
			touch_msleep(1);
			touch_power_1_8_vdd(dev, 0);
			touch_msleep(1);
			touch_power_3_3_vcl(dev, 0);
			touch_msleep(1);
		} else {
			TOUCH_I("%s, off Not Supported\n", __func__);
		}
		break;

	case POWER_ON:
		if (param->touch_power_control_en == FUNC_ON) {
			TOUCH_I("%s, on\n", __func__);
			touch_power_3_3_vcl(dev, 1);
			touch_msleep(1);
			touch_power_1_8_vdd(dev, 1);
			touch_msleep(10);
			touch_gpio_direction_output(ts->reset_pin, 1);
			atomic_set(&ts->state.sleep, IC_NORMAL);
		} else {
			TOUCH_I("%s, on Not Supported\n", __func__);
		}
		break;
	case POWER_HW_RESET_ASYNC:
		TOUCH_I("%s, HW reset\n", __func__);
		sw82906_reset_ctrl(dev, HW_RESET_ASYNC);
		break;

	case POWER_SW_RESET:
		sw82906_reset_ctrl(dev, SW_RESET);
		break;
	case POWER_SLEEP:
	case POWER_WAKE:
	default:
		TOUCH_I("%s, Not Supported. case: %d\n", __func__, ctrl);
		break;
	}

	return 0;
}

static int sw82906_ic_boot_check(struct device *dev, u32 *boot_st)
{
	u32 bootmode = 0;
	u32 rdata_crc = 0;
	u32 rdata_pass = ~0;
	u32 rdata_ptr = ~0;
	int err = 0;
	int ret;

	ret = sw82906_read_value(dev, GDMA_CRC_RESULT, &rdata_crc);
	err |= (rdata_crc != CRC_FIXED_VALUE);

	ret = sw82906_read_value(dev, GDMA_CRC_PASS, &rdata_pass);
	err |= (!rdata_pass)<<1;

	ret = sw82906_read_value(dev, INFO_PTR_ADDR, &rdata_ptr);
	err |= (!rdata_ptr)<<2;

	if (err) {
		TOUCH_E("boot status, %Xh(%Xh, %Xh, %Xh)\n",
				err, rdata_crc, rdata_pass, rdata_ptr);
	}

	bootmode = !err;

	if (boot_st)
		*boot_st = bootmode;

	return 0;
}

int sw82906_ic_info(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	u32 bootmode = 0;
	u32 crc_val = 0;
	int ret = 0;

	ret = sw82906_reg_read(dev, CHIP_INFO + tc_version,
			&d->ic_info.version, sizeof(d->ic_info.version));

	ret = sw82906_reg_read(dev, CHIP_INFO + tc_project_id,
			&d->ic_info.project_id, 8);

	ret = sw82906_reg_read(dev, PT_INFO,
			&d->ic_info.pt_info, sizeof(d->ic_info.pt_info));


	sw82906_ic_boot_check(dev, &bootmode);

	TOUCH_I("==================== Version Info ====================\n");
	TOUCH_I("version: v%d.%02d, build: %d, chip id: %d, protocol: %d\n",
			d->ic_info.version.major, d->ic_info.version.minor, d->ic_info.version.build,
			d->ic_info.version.chip_id, d->ic_info.version.protocol_ver);

	TOUCH_I("product id: %s, project id: %s",
			d->ic_info.pt_info.product_id, d->ic_info.project_id);

	TOUCH_I("chip_ver: %x, fpc_ver: %x, sensor_ver: %x machine_num: %x\n",
			d->ic_info.pt_info.chip_rev,
			d->ic_info.pt_info.fpc_ver,
			d->ic_info.pt_info.sensor_ver,
			d->ic_info.pt_info.machine_num);
	TOUCH_I("date: %04x.%02x.%02x, time: %02x:%02x:%02x\n",
			d->ic_info.pt_info.pt_date_year,
			d->ic_info.pt_info.pt_date_month,
			d->ic_info.pt_info.pt_date_day,
			d->ic_info.pt_info.pt_time_hour,
			d->ic_info.pt_info.pt_time_min,
			d->ic_info.pt_info.pt_time_sec);


	TOUCH_I("flash boot : %s\n", (bootmode) ? "BOOT pass" : "BOOT Fail");
	TOUCH_I("======================================================\n");

	if (ret < 0) {
		TOUCH_E("%s, IC REG read fail\n", __func__);
		ret = -EPERM;
		goto error;
	}

	if (!bootmode) {
		TOUCH_E("%s, FW Boot Fail, need to force FW upgrade\n", __func__);

		// Check flash crc value when FW BOOT FAIL
		ret = sw82906_fw_flash_crc(dev, &crc_val);
		if (ret < 0) {
			TOUCH_E("flash crc failed, %d\n", ret);
		}
		if (crc_val != CRC_FIXED_VALUE) {
			TOUCH_E("flash crc error %08Xh != %08Xh, %d\n",
					CRC_FIXED_VALUE, crc_val, ret);
		}
		TOUCH_I("flash crc check done\n");

		ts->force_fwup = 1;
		ret = -EPERM;
		goto error;
	}

	if ((d->ic_info.version.chip_id != 10) || (d->ic_info.version.protocol_ver != 4)) {
		TOUCH_E("%s, FW is in abnormal state because of ESD or something\n", __func__);
		ret = -EAGAIN;
		goto error;
	}

error:
	return ret;
}

static void sw82906_init_udf_longpress_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.area.x1 = 416;
	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.area.y1 = 2092;
	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.area.x2 = 665;
	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.area.y2 = 2340;
	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.slop = 16;
	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.press_time = 6;
	ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.contact_size = 1;

	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.area.x1 = 416;
	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.area.y1 = 2092;
	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.area.x2 = 665;
	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.area.y2 = 2340;
	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.slop = 16;
	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.press_time = 6;
	ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.contact_size = 1;


	TOUCH_I("%s, Long Press: display_area_id(%d), %s\n",
			__func__, DISPLAY_AREA_ID_0, ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.enable ? "Enable" : "Disable");
	TOUCH_I("%s, Long Press: display_area_id(%d), %s\n",
			__func__, DISPLAY_AREA_ID_1, ts->display_area_status[DISPLAY_AREA_ID_1].udf_longpress.enable ? "Enable" : "Disable");
}

static int sw82906_set_udf_longpress_active_area(struct device *dev, int display_area_id)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *area = &ts->display_area_status[display_area_id].udf_longpress.area;
	int ret = 0;
	u32 longpress_enable = 0;
	u32 active_area[2] = {0, };

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	area->x1 = ts->flexible_display.area[display_area_id].area.left_top.x;
	if (area->x1 < 0)
		area->x1 = 0;
	area->y1 = ts->flexible_display.area[display_area_id].area.left_top.y;
	if (area->y1 < 0)
		area->y1 = 0;
	area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
	if (area->x2 > ts->caps.max_x)
		area->x2 = ts->caps.max_x;
	area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;
	if (area->y2 > ts->caps.max_y)
		area->y2 = ts->caps.max_y;

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	longpress_enable = (display_area_id << 16) | ts->display_area_status[display_area_id].udf_longpress.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + LONG_PRESS_INDEX_AND_ENABLE, &longpress_enable, sizeof(longpress_enable));

	active_area[0] = (area->y1 << 16) | area->x1;
	active_area[1] = (area->y2 << 16) | area->x2;
	ret = sw82906_reg_write(dev, ABT_CMD + LONG_PRESS_ACT_AREA_START, active_area, sizeof(active_area));

	TOUCH_I("%s - display_area_id(%d): left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id, area->x1, area->y1, area->x2, area->y2);

	return ret;
}

static int sw82906_set_udf_longpress(struct device *dev, int display_area_id, bool enable)
{
	struct touch_core_data *ts = to_touch_core(dev);
	u32 longpress_enable = 0;
	u32 longpress_data[3] = {0, };
	int ret = 0;

	TOUCH_TRACE();

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	if (enable) {
		ts->display_area_status[display_area_id].udf_longpress.enable = 1;
	} else {
		ts->display_area_status[display_area_id].udf_longpress.enable = 0;
	}

	sw82906_set_udf_longpress_active_area(dev, display_area_id);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	longpress_enable = (display_area_id << 16) | ts->display_area_status[display_area_id].udf_longpress.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + LONG_PRESS_INDEX_AND_ENABLE, &longpress_enable, sizeof(longpress_enable));

	longpress_data[0] = ts->display_area_status[display_area_id].udf_longpress.slop;
	longpress_data[1] = ts->display_area_status[display_area_id].udf_longpress.press_time;
	longpress_data[2] = ts->display_area_status[display_area_id].udf_longpress.contact_size;
	ret = sw82906_reg_write(dev, ABT_CMD + LONG_PRESS_SLOPE, &longpress_data[0], sizeof(longpress_data));

	TOUCH_I("%s: DISPLAY_AREA_ID_%d: udf_longpress.enable=%d, longpress_enable=0x%x\n",
			__func__, display_area_id,
			ts->display_area_status[display_area_id].udf_longpress.enable,
			longpress_enable);

	return ret;
}

static void sw82906_init_fake_off_onetap_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.slop = 10;
	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.press_time = 50;
	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.delay_time = 10;
	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.area.x1 = 0 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.area.y1 = 0 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.area.x2 = 1080 - SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.area.y2 = ts->caps.max_y - SENSELESS_PIXEL;

	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.slop = 10;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.press_time = 50;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.delay_time = 10;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.area.x1 = 1189 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.area.y1 = 0 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.area.x2 = ts->caps.max_x - SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.area.y2 = ts->caps.max_y - SENSELESS_PIXEL;

	TOUCH_I("%s, LPWG_FAKE_OFF One Tap: display_area_id[%d] enable:%d, display_area_id[%d] enable:%d\n",
			__func__,
			DISPLAY_AREA_ID_0, ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.enable,
			DISPLAY_AREA_ID_1, ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.enable);
}

static void sw82906_init_show_aod_onetap_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.slop = 10;
	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.press_time = 50;
	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.delay_time = 10;
	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.area.x1 = 0 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.area.y1 = 0 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.area.x2 = 1080 - SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.area.y2 = ts->caps.max_y - SENSELESS_PIXEL;

	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.slop = 10;
	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.press_time = 50;
	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.delay_time = 10;
	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.area.x1 = 1189 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.area.y1 = 0 + SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.area.x2 = ts->caps.max_x - SENSELESS_PIXEL;
	ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.area.y2 = ts->caps.max_y - SENSELESS_PIXEL;

	TOUCH_I("%s, LPWG_SHOW_AOD One Tap: display_area_id[%d] enable:%d, display_area_id[%d] enable:%d\n",
			__func__,
			DISPLAY_AREA_ID_0, ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.enable,
			DISPLAY_AREA_ID_1, ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.enable);
}

static int sw82906_set_fake_off_onetap_active_area(
	struct device *dev, int display_area_id, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *fake_off_onetap_area = &ts->display_area_status[display_area_id].fake_off_onetap.area;
	int ret = 0;
	u32 onetap_enable = 0;
	u32 active_area[2] = {0, };

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	if (area_data != NULL) {
		fake_off_onetap_area->x1 = area_data->x1;
		fake_off_onetap_area->y1 = area_data->y1;
		fake_off_onetap_area->x2 = area_data->x2;
		fake_off_onetap_area->y2 = area_data->y2;
	}

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	onetap_enable = (display_area_id << 16) |
		(u32)(ts->display_area_status[display_area_id].fake_off_onetap.enable);
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_INDEX_AND_ENABLE, &onetap_enable, sizeof(onetap_enable));

	active_area[0] = (fake_off_onetap_area->y1 << 16) | fake_off_onetap_area->x1;
	active_area[1] = (fake_off_onetap_area->y2 << 16) | fake_off_onetap_area->x2;
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_ACTIVE_AREA_START, active_area, sizeof(active_area));

	TOUCH_I("%s - display_area_id(%d): left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id, fake_off_onetap_area->x1, fake_off_onetap_area->y1, fake_off_onetap_area->x2, fake_off_onetap_area->y2);

	return ret;
}

static int sw82906_set_fake_off_onetap(struct device *dev, int display_area_id, bool enable, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct onetap_ctrl *onetap = &ts->display_area_status[display_area_id].fake_off_onetap;
	u32 onetap_enable = 0;
	u32 onetap_data[3] = {0, };
	int ret = 0;

	TOUCH_TRACE();

	if (ts->display_area_status[display_area_id].show_aod_onetap.enable) {
		TOUCH_I("[ID:%d] Already show_aod_onetap enabled\n", display_area_id);
		return ret;
	}

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	onetap->enable = enable;

	sw82906_set_fake_off_onetap_active_area(dev, display_area_id, area_data);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	onetap_enable = (display_area_id << 16) | (u32)ts->display_area_status[display_area_id].fake_off_onetap.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_INDEX_AND_ENABLE, &onetap_enable, sizeof(onetap_enable));

	onetap_data[0] = onetap->slop;
	onetap_data[1] = onetap->press_time;
	onetap_data[2] = onetap->delay_time;
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_TOUCH_SLOPE, &onetap_data[0], sizeof(onetap_data));

	TOUCH_I("%s: DISPLAY_AREA_ID[%d], onetap->enable=%d, enable_data=0x%x\n",
			__func__, display_area_id,
			onetap->enable,
			onetap_enable);

	return ret;
}


static int sw82906_set_show_aod_onetap_active_area(
		struct device *dev, int display_area_id, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *show_aod_onetap_area = &ts->display_area_status[display_area_id].show_aod_onetap.area;
	int ret = 0;
	u32 onetap_enable = 0;
	u32 active_area[2] = {0, };

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	if (area_data != NULL) {
		show_aod_onetap_area->x1 = area_data->x1;
		show_aod_onetap_area->y1 = area_data->y1;
		show_aod_onetap_area->x2 = area_data->x2;
		show_aod_onetap_area->y2 = area_data->y2;
	}

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	onetap_enable = (display_area_id << 16) | (u32)ts->display_area_status[display_area_id].show_aod_onetap.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_INDEX_AND_ENABLE, &onetap_enable, sizeof(onetap_enable));

	active_area[0] = (show_aod_onetap_area->y1 << 16) | show_aod_onetap_area->x1;
	active_area[1] = (show_aod_onetap_area->y2 << 16) | show_aod_onetap_area->x2;
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_ACTIVE_AREA_START, active_area, sizeof(active_area));

	TOUCH_I("%s - display_area_id(%d): left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id, show_aod_onetap_area->x1, show_aod_onetap_area->y1, show_aod_onetap_area->x2, show_aod_onetap_area->y2);

	return ret;
}

static int sw82906_set_show_aod_onetap(struct device *dev, int display_area_id, bool enable)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct onetap_ctrl *onetap = &ts->display_area_status[display_area_id].show_aod_onetap;
	u32 onetap_enable = 0;
	u32 onetap_data[3] = {0, };
	int ret = 0;

	TOUCH_TRACE();

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	onetap->enable = enable;

	sw82906_set_show_aod_onetap_active_area(dev, display_area_id, &ts->display_area_status[display_area_id].show_aod_onetap.area);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	onetap_enable = (display_area_id << 16) | (u32)ts->display_area_status[display_area_id].show_aod_onetap.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_INDEX_AND_ENABLE, &onetap_enable, sizeof(onetap_enable));

	onetap_data[0] = onetap->slop;
	onetap_data[1] = onetap->press_time;
	onetap_data[2] = onetap->delay_time;
	ret = sw82906_reg_write(dev, ABT_CMD + ONE_TAP_TOUCH_SLOPE, &onetap_data[0], sizeof(onetap_data));

	TOUCH_I("%s: DISPLAY_AREA_ID[%d], onetap->enable=%d, enable_data=0x%x\n",
			__func__, display_area_id,
			onetap->enable,
			onetap_enable);

	return ret;
}

static void sw82906_init_knock_on_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->display_area_status[DISPLAY_AREA_ID_0].knock_on.tap_count = 2;
	ts->display_area_status[DISPLAY_AREA_ID_0].knock_on.min_intertap = 3;
	ts->display_area_status[DISPLAY_AREA_ID_0].knock_on.max_intertap = 70;
	ts->display_area_status[DISPLAY_AREA_ID_0].knock_on.touch_slop = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].knock_on.tap_distance = 10;
	ts->display_area_status[DISPLAY_AREA_ID_0].knock_on.intr_delay = 0;

	ts->display_area_status[DISPLAY_AREA_ID_1].knock_on.tap_count = 2;
	ts->display_area_status[DISPLAY_AREA_ID_1].knock_on.min_intertap = 3;
	ts->display_area_status[DISPLAY_AREA_ID_1].knock_on.max_intertap = 70;
	ts->display_area_status[DISPLAY_AREA_ID_1].knock_on.touch_slop = 100;
	ts->display_area_status[DISPLAY_AREA_ID_1].knock_on.tap_distance = 10;
	ts->display_area_status[DISPLAY_AREA_ID_1].knock_on.intr_delay = 0;
}

static int sw82906_get_coordinate(struct device *dev, int count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	u8 i = 0;
	u32 rdata[TAP_COUNT_KNOCK_ON];

	if (!count || count > TAP_COUNT_KNOCK_ON) {
		TOUCH_E("Invalid coordinate count : %d\n", count);
		return 0;
	}

	memcpy(&rdata, d->info.data, sizeof(u32) * count);

	ts->lpwg_coord.count = count;
	for (i = 0; i < count; i++) {
		ts->lpwg_coord.code[i].x = rdata[i] & 0xffff;
		ts->lpwg_coord.code[i].y = (rdata[i] >> 16) & 0xffff;

		TOUCH_I("LPWG data %d, %d\n", ts->lpwg_coord.code[i].x, ts->lpwg_coord.code[i].y);
	}
	ts->lpwg_coord.code[count].x = -1;
	ts->lpwg_coord.code[count].y = -1;

	return 0;
}

static int sw82906_set_knock_on_active_area(
		struct device *dev, int display_area_id, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *area = &ts->display_area_status[display_area_id].knock_on.area;
	int ret = 0;
	u32 knock_on_enable = 0;

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	if (area_data != NULL) {
		area->x1 = area_data->x1;
		area->y1 = area_data->y1;
		area->x2 = area_data->x2;
		area->y2 = area_data->y2;
	}

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	knock_on_enable = (display_area_id << 16) | ts->display_area_status[display_area_id].knock_on.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + KNOCK_ON_INDEX_AND_ENABLE, &knock_on_enable, sizeof(knock_on_enable));

	if (area->x1 < ts->flexible_display.area[display_area_id].area.left_top.x)
		area->x1 = ts->flexible_display.area[display_area_id].area.left_top.x;
	if (area->y1 < ts->flexible_display.area[display_area_id].area.left_top.y)
		area->y1 = ts->flexible_display.area[display_area_id].area.left_top.y;
	if (area->x2 > ts->flexible_display.area[display_area_id].area.right_bottom.x)
		area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
	if (area->y2 > ts->flexible_display.area[display_area_id].area.right_bottom.y)
		area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;
	ret = sw82906_reg_write(dev, ABT_CMD + KNOCK_ON_ACTIVE_AREA_X1, area, sizeof(struct active_area));

	TOUCH_I("%s - display_area_id(%d): left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id, area->x1, area->y1, area->x2, area->y2);

	return ret;
}

static int sw82906_set_knock_on(struct device *dev, int display_area_id, int mode)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct knock_on_ctrl *knock_on = &ts->display_area_status[display_area_id].knock_on;
	u32 knock_on_buf[7] = {0, };
	int ret = 0;

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	switch (mode) {
		case LPWG_NONE:
			knock_on->enable = 0;
			break;
		case LPWG_DOUBLE_TAP:
			knock_on->enable = 1;
			break;
		default:
			TOUCH_E("Unknown knock-on control case\n");
			return -EINVAL;
	}

	sw82906_set_knock_on_active_area(dev, display_area_id, &ts->display_area_status[display_area_id].knock_on.area);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn NORMAL\n", __func__);
		return -ENODEV;
	}

	knock_on_buf[0] = (display_area_id << 16) | knock_on->enable;
	knock_on_buf[1] = knock_on->tap_count;
	knock_on_buf[2] = knock_on->min_intertap;
	knock_on_buf[3] = knock_on->max_intertap;
	knock_on_buf[4] = knock_on->touch_slop;
	knock_on_buf[5] = knock_on->tap_distance;
	knock_on_buf[6] = knock_on->intr_delay;
	ret = sw82906_reg_write(dev, ABT_CMD + KNOCK_ON_INDEX_AND_ENABLE, &knock_on_buf[0], sizeof(knock_on_buf));

	TOUCH_I("%s: DISPLAY_AREA_ID_%d: knock_on->enable=%d, knock_on_buf[0]=0x%x\n",
			__func__, display_area_id, knock_on->enable, knock_on_buf[0]);

	sw82906_set_lpwg_failreason(dev, LPWG_DEBUG_KNOCK_ON_FRONT, true);
	sw82906_set_lpwg_failreason(dev, LPWG_DEBUG_KNOCK_ON_REAR, true);

	return ret;
}

static void sw82906_init_ai_pick_double_tap_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.area.x1 = 0;
	ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.area.y1 = 0;
	ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.area.x2 = 0;
	ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.area.y2 = 0;
}

static void sw82906_set_ai_pick_double_tap(struct device *dev, int display_area_id, bool enable)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	// 1. Saving received data
	if (ts->display_area_status[display_area_id].lpwg.sensor == PROX_NEAR) {
		TOUCH_I("%s : the function is skipped, because it is near\n", __func__);
		return;
	}

	ts->display_area_status[display_area_id].ai_pick_double_tap.enable = enable;
	ts->display_area_status[display_area_id].ai_pick_double_tap.area.x1
		= ts->lpwg_function[display_area_id].function_area.area.left_top.x;
	ts->display_area_status[display_area_id].ai_pick_double_tap.area.y1
		= ts->lpwg_function[display_area_id].function_area.area.left_top.y;
	ts->display_area_status[display_area_id].ai_pick_double_tap.area.x2
		= ts->lpwg_function[display_area_id].function_area.area.right_bottom.x;
	ts->display_area_status[display_area_id].ai_pick_double_tap.area.y2
		= ts->lpwg_function[display_area_id].function_area.area.right_bottom.y;

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return;
	}

	TOUCH_I("%s: %s\n", __func__,
			(enable ? "enable" : "disable"));

	if (enable) {
		if (ts->display_area_status[display_area_id].lpwg.mode == LPWG_NONE) {
			TOUCH_I("%s: enable ai_pick (display_area_id: %d, lpwg.mode : %d)\n",
					__func__,
					display_area_id,
					ts->display_area_status[display_area_id].lpwg.mode);

			sw82906_set_knock_on(dev, display_area_id, LPWG_DOUBLE_TAP);
			sw82906_set_knock_on_active_area(dev, display_area_id,
					&ts->display_area_status[display_area_id].ai_pick_double_tap.area);
		} else {
			TOUCH_I("%s: no need to enable lpwg setting (display_area_id: %d, lpwg_mode : %d)\n",
					__func__,
					display_area_id,
					ts->display_area_status[display_area_id].lpwg.mode);
		}
	} else {
		if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.mode == LPWG_NONE) {
			TOUCH_I("%s: disable ai_pick (display_area_id: %d, lpwg.mode : %d)\n",
					__func__,
					display_area_id,
					ts->display_area_status[display_area_id].lpwg.mode);

			sw82906_set_knock_on(dev, display_area_id, LPWG_NONE);
		} else {
			TOUCH_I("%s: no need to disable lpwg setting (display_area_id: %d, lpwg_mode : %d)\n",
					__func__,
					display_area_id,
					ts->display_area_status[display_area_id].lpwg.mode);
		}
	}
}

static bool sw82906_check_fake_off_onetap_event(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *area = &ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.area;
	bool result = false;

	TOUCH_TRACE();

	if ((ts->lpwg_coord.code[0].x >= area->x1)
			&& (ts->lpwg_coord.code[0].x <= area->x2)
			&& (ts->lpwg_coord.code[0].y >= area->y1)
			&& (ts->lpwg_coord.code[0].y <= area->y2)) {
		result = true;
	}

	return result;
}

static bool sw82906_check_ai_pick_event(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *area = &ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.area;
	int i = 0;
	bool result[2] = {false, false};

	TOUCH_TRACE();

	for (i = 0; i < 2; i++) {
		if ((ts->lpwg_coord.code[i].x >= area->x1)
				&& (ts->lpwg_coord.code[i].x <= area->x2)
				&& (ts->lpwg_coord.code[i].y >= area->y1)
				&& (ts->lpwg_coord.code[i].y <= area->y2)) {
			result[i] = true;
		}
	}

	return (result[0] & result[1]);
}

static int sw82906_get_swipe_data(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	u32 rdata[3];

	/* start (X, Y), end (X, Y), time = 2bytes * 5 = 10 bytes */
	memcpy(&rdata, d->info.data, sizeof(u32) * 3);

	ts->lpwg_coord.count = 2;
	ts->lpwg_coord.code[0].x = rdata[0] & 0xFFFF;
	ts->lpwg_coord.code[0].y = rdata[0] >> 16;
	ts->lpwg_coord.code[1].x = rdata[1] & 0xFFFF;
	ts->lpwg_coord.code[1].y = rdata[1] >> 16;

	TOUCH_I("Swipe Gesture: start(%4d,%4d) end(%4d,%4d) swipe_time(%dms)\n",
			rdata[0] & 0xffff, rdata[0] >> 16,
			rdata[1] & 0xffff, rdata[1] >> 16,
			rdata[2] & 0xffff);

	return 0;
}

static int sw82906_set_lg_pay_swipe_active_area(
		struct device *dev, int display_area_id, int gesture, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *swipe_active_area = NULL, *swipe_start_area = NULL;
	u16 addr_active_area = 0, addr_start_area = 0;
	u32 swipe_enable = 0;
	u32 swipe_buf_active_area[4] = {0, };
	u32 swipe_buf_start_area[4] = {0, };
	u32 clear_16bit = 0xFFFF, mask_16bit = 0xFFFF;
	int ret = 0, shift_bit = 0;

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	switch (gesture) {
		case SWIPE_UP:
			swipe_active_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_UP].area;
			swipe_start_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_UP].start_area;
			addr_active_area = ABT_CMD + SWIPE_ACTIVE_AREA_VERTICAL_X1;
			addr_start_area = ABT_CMD + SWIPE_START_AREA_VERTICAL_X1;
			shift_bit = 0;
			break;
		case SWIPE_DOWN:
			swipe_active_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_DOWN].area;
			swipe_start_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_DOWN].start_area;
			addr_active_area = ABT_CMD + SWIPE_ACTIVE_AREA_VERTICAL_X1;
			addr_start_area = ABT_CMD + SWIPE_START_AREA_VERTICAL_X1;
			shift_bit = 16;
			break;
		case SWIPE_RIGHT:
			swipe_active_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].area;
			swipe_start_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].start_area;
			addr_active_area = ABT_CMD + SWIPE_ACTIVE_AREA_HORIZONTAL_X1;
			addr_start_area = ABT_CMD + SWIPE_START_AREA_HORIZONTAL_X1;
			shift_bit = 16;
			break;
		case SWIPE_LEFT:
			swipe_active_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_LEFT].area;
			swipe_start_area = &ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_LEFT].start_area;
			addr_active_area = ABT_CMD + SWIPE_ACTIVE_AREA_HORIZONTAL_X1;
			addr_start_area = ABT_CMD + SWIPE_START_AREA_HORIZONTAL_X1;
			shift_bit = 0;
			break;
		default:
			TOUCH_E("Unknown lpwg gesture - display_area_id[%d], function[%d], gesture[%d]",
					display_area_id,
					ts->lpwg_function[display_area_id].function,
					ts->lpwg_function[display_area_id].gesture);
			return -ERANGE;
	};

	// Save last swipe active area
	swipe_active_area->x1 = ts->flexible_display.area[display_area_id].area.left_top.x;
	swipe_active_area->y1 = ts->flexible_display.area[display_area_id].area.left_top.y;
	swipe_active_area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
	swipe_active_area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;
	if (swipe_active_area->x1 < 0)
		swipe_active_area->x1 = 0;
	if (swipe_active_area->y1 < 0)
		swipe_active_area->y1 = 0;
	if (swipe_active_area->x2 > ts->flexible_display.area[display_area_id].area.right_bottom.x)
		swipe_active_area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
	if (swipe_active_area->y2 > ts->flexible_display.area[display_area_id].area.right_bottom.y)
		swipe_active_area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;

	// Save last swipe start area
	if (area_data != NULL) {
		swipe_start_area->x1 = area_data->x1;
		swipe_start_area->y1 = area_data->y1;
		swipe_start_area->x2 = area_data->x2;
		swipe_start_area->y2 = area_data->y2;
		if (swipe_start_area->x1 < 0)
			swipe_start_area->x1 = 0;
		if (swipe_start_area->y1 < 0)
			swipe_start_area->y1 = 0;
		if (swipe_start_area->x2 > area_data->x2)
			swipe_start_area->x2 = area_data->x2;
		if (swipe_start_area->y2 > area_data->y2)
			swipe_start_area->y2 = area_data->y2;
	}

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}
	ret = sw82906_reg_read(dev, ABT_CMD + SWIPE_INDEX_AND_ENABLE, &swipe_enable, sizeof(swipe_enable));
	swipe_enable = (swipe_enable & ~(clear_16bit << 16)) | (display_area_id << 16);
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_INDEX_AND_ENABLE, &swipe_enable, sizeof(swipe_enable));

	// Update swipe active area register
	ret = sw82906_reg_read(dev, addr_active_area, &swipe_buf_active_area[0], sizeof(swipe_buf_active_area));
	swipe_buf_active_area[0] = (swipe_buf_active_area[0] & ~(clear_16bit << shift_bit)) | (swipe_active_area->x1 & mask_16bit) << shift_bit;
	swipe_buf_active_area[1] = (swipe_buf_active_area[1] & ~(clear_16bit << shift_bit)) | (swipe_active_area->y1 & mask_16bit) << shift_bit;
	swipe_buf_active_area[2] = (swipe_buf_active_area[2] & ~(clear_16bit << shift_bit)) | (swipe_active_area->x2 & mask_16bit) << shift_bit;
	swipe_buf_active_area[3] = (swipe_buf_active_area[3] & ~(clear_16bit << shift_bit)) | (swipe_active_area->y2 & mask_16bit) << shift_bit;
	ret = sw82906_reg_write(dev, addr_active_area, &swipe_buf_active_area[0], sizeof(swipe_buf_active_area));

	// Update swipe start area register
	ret = sw82906_reg_read(dev, addr_start_area, &swipe_buf_start_area[0], sizeof(swipe_buf_start_area));
	swipe_buf_start_area[0] = (swipe_buf_start_area[0] & ~(clear_16bit << shift_bit)) | (swipe_start_area->x1 & mask_16bit) << shift_bit;
	swipe_buf_start_area[1] = (swipe_buf_start_area[1] & ~(clear_16bit << shift_bit)) | (swipe_start_area->y1 & mask_16bit) << shift_bit;
	swipe_buf_start_area[2] = (swipe_buf_start_area[2] & ~(clear_16bit << shift_bit)) | (swipe_start_area->x2 & mask_16bit) << shift_bit;
	swipe_buf_start_area[3] = (swipe_buf_start_area[3] & ~(clear_16bit << shift_bit)) | (swipe_start_area->y2 & mask_16bit) << shift_bit;
	ret = sw82906_reg_write(dev, addr_start_area, &swipe_buf_start_area[0], sizeof(swipe_buf_start_area));

	TOUCH_I("%s - display_area_id[%d] swipe gesture[%s]: left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id, lpwg_gesture_str[gesture],
			swipe_start_area->x1, swipe_start_area->y1, swipe_start_area->x2, swipe_start_area->y2);

	return ret;
}

static int sw82906_set_quick_tool_swipe_active_area(
		struct device *dev, int display_area_id, int gesture, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *swipe_active_area = NULL, *swipe_start_area = NULL;
	struct active_area *swipe_active_border_area = NULL, *swipe_start_border_area = NULL;
	u16 addr_active_area = 0, addr_start_area = 0;
	u32 swipe_enable = 0;
	u32 swipe_buf_active_area[4] = {0, };
	u32 swipe_buf_start_area[4] = {0, };
	u32 clear_16bit = 0xFFFF, mask_16bit = 0xFFFF;
	int ret = 0, shift_bit = 0;

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	switch (gesture) {
		case SWIPE_RIGHT:
			swipe_active_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].area;
			swipe_start_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].start_area;
			swipe_active_border_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].border_area;
			swipe_start_border_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].start_border_area;
			addr_active_area = ABT_CMD + SWIPE2_ACTIVE_AREA_HORIZONTAL_X1;
			addr_start_area = ABT_CMD + SWIPE2_START_AREA_HORIZONTAL_X1;
			shift_bit = 16;
			break;
		case SWIPE_LEFT:
			swipe_active_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].area;
			swipe_start_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].start_area;
			swipe_active_border_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].border_area;
			swipe_start_border_area = &ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].start_border_area;
			addr_active_area = ABT_CMD + SWIPE2_ACTIVE_AREA_HORIZONTAL_X1;
			addr_start_area = ABT_CMD + SWIPE2_START_AREA_HORIZONTAL_X1;
			shift_bit = 0;
			break;
		default:
			TOUCH_E("Unknown lpwg gesture - display_area_id[%d], function[%d], gesture[%d]",
					display_area_id,
					ts->lpwg_function[display_area_id].function,
					ts->lpwg_function[display_area_id].gesture);
			return -ERANGE;
	};

	// Save last swipe active area
	swipe_active_area->x1 = ts->flexible_display.area[display_area_id].area.left_top.x - swipe_active_border_area->x1;
	swipe_active_area->y1 = ts->flexible_display.area[display_area_id].area.left_top.y - swipe_active_border_area->y1;
	swipe_active_area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x + swipe_active_border_area->x2;
	swipe_active_area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y + swipe_active_border_area->y2;
	if (swipe_active_area->x1 < ts->flexible_display.area[display_area_id].area.left_top.x)
		swipe_active_area->x1 = ts->flexible_display.area[display_area_id].area.left_top.x;
	if (swipe_active_area->y1 < ts->flexible_display.area[display_area_id].area.left_top.y)
		swipe_active_area->y1 = ts->flexible_display.area[display_area_id].area.left_top.y;
	if (swipe_active_area->x2 > ts->flexible_display.area[display_area_id].area.right_bottom.x)
		swipe_active_area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
	if (swipe_active_area->y2 > ts->flexible_display.area[display_area_id].area.right_bottom.y)
		swipe_active_area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;

	// Save last swipe start area
	if (area_data != NULL) {
		swipe_start_area->x1 = area_data->x1 - swipe_start_border_area->x1;
		swipe_start_area->y1 = area_data->y1 - swipe_start_border_area->y1;
		swipe_start_area->x2 = area_data->x2 + swipe_start_border_area->x2;
		swipe_start_area->y2 = area_data->y2 + swipe_start_border_area->y2;
		if (swipe_start_area->x1 < swipe_active_area->x1)
			swipe_start_area->x1 = swipe_active_area->x1;
		if (swipe_start_area->y1 < swipe_active_area->y1)
			swipe_start_area->y1 = swipe_active_area->y1;
		if (swipe_start_area->x2 > swipe_active_area->x2)
			swipe_start_area->x2 = swipe_active_area->x2;
		if (swipe_start_area->y2 > swipe_active_area->y2)
			swipe_start_area->y2 = swipe_active_area->y2;
	}

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	ret = sw82906_reg_read(dev, ABT_CMD + SWIPE2_INDEX_AND_ENABLE, &swipe_enable, sizeof(swipe_enable));
	swipe_enable = (swipe_enable & ~(clear_16bit << 16)) | (display_area_id << 16);
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE2_INDEX_AND_ENABLE, &swipe_enable, sizeof(swipe_enable));

	// Update swipe active area register
	ret = sw82906_reg_read(dev, addr_active_area, &swipe_buf_active_area[0], sizeof(swipe_buf_active_area));
	swipe_buf_active_area[0] = (swipe_buf_active_area[0] & ~(clear_16bit << shift_bit)) | (swipe_active_area->x1 & mask_16bit) << shift_bit;
	swipe_buf_active_area[1] = (swipe_buf_active_area[1] & ~(clear_16bit << shift_bit)) | (swipe_active_area->y1 & mask_16bit) << shift_bit;
	swipe_buf_active_area[2] = (swipe_buf_active_area[2] & ~(clear_16bit << shift_bit)) | (swipe_active_area->x2 & mask_16bit) << shift_bit;
	swipe_buf_active_area[3] = (swipe_buf_active_area[3] & ~(clear_16bit << shift_bit)) | (swipe_active_area->y2 & mask_16bit) << shift_bit;
	ret = sw82906_reg_write(dev, addr_active_area, &swipe_buf_active_area[0], sizeof(swipe_buf_active_area));

	// Update swipe start area register
	ret = sw82906_reg_read(dev, addr_start_area, &swipe_buf_start_area[0], sizeof(swipe_buf_start_area));
	swipe_buf_start_area[0] = (swipe_buf_start_area[0] & ~(clear_16bit << shift_bit)) | (swipe_start_area->x1 & mask_16bit) << shift_bit;
	swipe_buf_start_area[1] = (swipe_buf_start_area[1] & ~(clear_16bit << shift_bit)) | (swipe_start_area->y1 & mask_16bit) << shift_bit;
	swipe_buf_start_area[2] = (swipe_buf_start_area[2] & ~(clear_16bit << shift_bit)) | (swipe_start_area->x2 & mask_16bit) << shift_bit;
	swipe_buf_start_area[3] = (swipe_buf_start_area[3] & ~(clear_16bit << shift_bit)) | (swipe_start_area->y2 & mask_16bit) << shift_bit;
	ret = sw82906_reg_write(dev, addr_start_area, &swipe_buf_start_area[0], sizeof(swipe_buf_start_area));

	TOUCH_I("%s - display_area_id[%d] swipe gesture[%s]: start_area left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id, lpwg_gesture_str[gesture],
			swipe_start_area->x1, swipe_start_area->y1, swipe_start_area->x2, swipe_start_area->y2);

	return ret;
}

static int sw82906_set_fake_off_swipe_active_area(
		struct device *dev, int display_area_id, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct swipe_ctrl *fake_off_swipe = &ts->display_area_status[display_area_id].fake_off_swipe[0];
	struct active_area *swipe_active_area = NULL, *swipe_start_area = NULL;
	u32 swipe_enable = 0;
	u32 swipe_buf_active_area[4] = {0, };
	u32 swipe_buf_start_area[4] = {0, };
	int ret = 0, i = 0;

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	for (i = 0; i < FAKE_OFF_SWIPE_MAX; i++) {
		swipe_active_area = &fake_off_swipe[i].area;
		swipe_start_area = &fake_off_swipe[i].start_area;

		// Save last swipe active area
		swipe_active_area->x1 = ts->flexible_display.area[display_area_id].area.left_top.x;
		swipe_active_area->y1 = ts->flexible_display.area[display_area_id].area.left_top.y;
		swipe_active_area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
		swipe_active_area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;
		if (swipe_active_area->x1 < 0)
			swipe_active_area->x1 = 0;
		if (swipe_active_area->y1 < 0)
			swipe_active_area->y1 = 0;
		if (swipe_active_area->x2 > ts->flexible_display.area[display_area_id].area.right_bottom.x)
			swipe_active_area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
		if (swipe_active_area->y2 > ts->flexible_display.area[display_area_id].area.right_bottom.y)
			swipe_active_area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;

		// Save last swipe start area
		if (area_data != NULL) {
			swipe_start_area->x1 = area_data->x1;
			swipe_start_area->y1 = area_data->y1;
			swipe_start_area->x2 = area_data->x2;
			swipe_start_area->y2 = area_data->y2;
			if (swipe_start_area->x1 < 0)
				swipe_start_area->x1 = 0;
			if (swipe_start_area->y1 < 0)
				swipe_start_area->y1 = 0;
			if (swipe_start_area->x2 > area_data->x2)
				swipe_start_area->x2 = area_data->x2;
			if (swipe_start_area->y2 > area_data->y2)
				swipe_start_area->y2 = area_data->y2;
		}
	}

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	swipe_enable = (display_area_id << 16) |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].enable << 12 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].enable << 8 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].enable << 4 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].enable;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_INDEX_AND_ENABLE, &swipe_enable, sizeof(swipe_enable));

	// Update swipe active area register
	swipe_buf_active_area[0] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].area.x1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].area.x1;
	swipe_buf_active_area[1] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].area.y1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].area.y1;
	swipe_buf_active_area[2] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].area.x2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].area.x2;
	swipe_buf_active_area[3] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].area.y2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].area.y2;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_ACTIVE_AREA_HORIZONTAL_X1, &swipe_buf_active_area[0], sizeof(swipe_buf_active_area));

	swipe_buf_active_area[0] = fake_off_swipe[FAKE_OFF_SWIPE_UP].area.x1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].area.x1;
	swipe_buf_active_area[1] = fake_off_swipe[FAKE_OFF_SWIPE_UP].area.y1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].area.y1;
	swipe_buf_active_area[2] = fake_off_swipe[FAKE_OFF_SWIPE_UP].area.x2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].area.x2;
	swipe_buf_active_area[3] = fake_off_swipe[FAKE_OFF_SWIPE_UP].area.y2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].area.y2;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_ACTIVE_AREA_VERTICAL_X1, &swipe_buf_active_area[0], sizeof(swipe_buf_active_area));

	// Update swipe start area register
	swipe_buf_start_area[0] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].start_area.x1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].start_area.x1;
	swipe_buf_start_area[1] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].start_area.y1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].start_area.y1;
	swipe_buf_start_area[2] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].start_area.x2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].start_area.x2;
	swipe_buf_start_area[3] = fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].start_area.y2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_LEFT].start_area.y2;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_START_AREA_HORIZONTAL_X1, &swipe_buf_start_area[0], sizeof(swipe_buf_start_area));

	swipe_buf_start_area[0] = fake_off_swipe[FAKE_OFF_SWIPE_UP].start_area.x1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].start_area.x1;
	swipe_buf_start_area[1] = fake_off_swipe[FAKE_OFF_SWIPE_UP].start_area.y1 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].start_area.y1;
	swipe_buf_start_area[2] = fake_off_swipe[FAKE_OFF_SWIPE_UP].start_area.x2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].start_area.x2;
	swipe_buf_start_area[3] = fake_off_swipe[FAKE_OFF_SWIPE_UP].start_area.y2 << 16 | fake_off_swipe[FAKE_OFF_SWIPE_DOWN].start_area.y2;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_START_AREA_VERTICAL_X1, &swipe_buf_start_area[0], sizeof(swipe_buf_start_area));

	TOUCH_I("%s - display_area_id[%d]: left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id,
			swipe_start_area->x1, swipe_start_area->y1, swipe_start_area->x2, swipe_start_area->y2);

	return ret;
}

static int sw82906_set_lg_pay_swipe(
		struct device *dev, int display_area_id, int gesture, bool enable, struct active_area *area_data) {
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	struct swipe_ctrl *lg_pay_swipe = &ts->display_area_status[display_area_id].lg_pay_swipe[0];
	u32 swipe_buf[10] = {0, };

	TOUCH_TRACE();

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	switch (gesture) {
		case SWIPE_UP:
			lg_pay_swipe[LG_PAY_SWIPE_UP].enable = enable;
			lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_DOWN].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable = 0;
			break;
		case SWIPE_DOWN:
			lg_pay_swipe[LG_PAY_SWIPE_UP].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_DOWN].enable = enable;
			lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable = 0;
			break;
		case SWIPE_RIGHT:
			lg_pay_swipe[LG_PAY_SWIPE_UP].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable = enable;
			lg_pay_swipe[LG_PAY_SWIPE_DOWN].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable = 0;
			break;
		case SWIPE_LEFT:
			lg_pay_swipe[LG_PAY_SWIPE_UP].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_DOWN].enable = 0;
			lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable = enable;
			break;
		default:
			TOUCH_E("Unknown lpwg gesture - display_area_id[%d], function[%d], gesture[%d]",
					display_area_id,
					ts->lpwg_function[display_area_id].function,
					ts->lpwg_function[display_area_id].gesture);
			return -1;
	};

	sw82906_set_lg_pay_swipe_active_area(dev, display_area_id, gesture, area_data);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	swipe_buf[0] = (display_area_id << 16) |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].enable << 12 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_UP].enable << 8 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable << 4 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable;
	swipe_buf[1] = (u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].distance << 24 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_UP].distance << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].distance << 8 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].distance;
	swipe_buf[2] = (u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].ratio_thres << 24 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_UP].ratio_thres << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].ratio_thres << 8 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].ratio_thres;
	swipe_buf[3] = (u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].min_time << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].min_time;
	swipe_buf[4] = (u32)lg_pay_swipe[LG_PAY_SWIPE_UP].min_time << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].min_time;
	swipe_buf[5] = (u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].max_time << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].max_time;
	swipe_buf[6] = (u32)lg_pay_swipe[LG_PAY_SWIPE_UP].max_time << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].max_time;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_INDEX_AND_ENABLE, &swipe_buf[0], sizeof(u32) * 7);

	swipe_buf[7] = (u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].wrong_dir_thres << 24 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_UP].wrong_dir_thres << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].wrong_dir_thres << 8 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].wrong_dir_thres;
	swipe_buf[8] = (u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].init_ratio_chk_dist << 24 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_UP].init_ratio_chk_dist << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].init_ratio_chk_dist << 8 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].init_ratio_chk_dist;
	swipe_buf[9] = (u32)lg_pay_swipe[LG_PAY_SWIPE_DOWN].init_ratio_thres << 24 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_UP].init_ratio_thres << 16 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_RIGHT].init_ratio_thres << 8 |
		(u32)lg_pay_swipe[LG_PAY_SWIPE_LEFT].init_ratio_thres;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_WRONG_DIRECTION_THD, &swipe_buf[7], sizeof(u32) * 3);

	TOUCH_I("%s: DISPLAY_AREA_ID_%d: swipe gesture[%s], enable=%d, swipe_buf[0]=0x%x\n",
			__func__, display_area_id, lpwg_gesture_str[gesture], enable, swipe_buf[0]);

	return 0;
}

static int sw82906_set_quick_tool_swipe(
		struct device *dev, int display_area_id, int gesture, bool enable, struct active_area *area_data) {
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	struct swipe_ctrl *quick_tool_swipe = &ts->display_area_status[display_area_id].quick_tool_swipe[0];
	u32 swipe_buf[8] = {0, };

	TOUCH_TRACE();

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	switch (gesture) {
		case SWIPE_RIGHT:
			quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].enable = enable;
			break;
		case SWIPE_LEFT:
			quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].enable = enable;
			break;
		default:
			TOUCH_E("Unknown lpwg gesture - display_area_id[%d], function[%d], gesture[%d]",
					display_area_id,
					ts->lpwg_function[display_area_id].function,
					ts->lpwg_function[display_area_id].gesture);
			return -1;
			break;
	};

	sw82906_set_quick_tool_swipe_active_area(dev, display_area_id, gesture, area_data);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	swipe_buf[0] = (display_area_id << 16) |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].enable << 8 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].enable;
	swipe_buf[1] = (u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].distance << 8 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].distance;
	swipe_buf[2] = (u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].ratio_thres << 8 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].ratio_thres;
	swipe_buf[3] = (u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].min_time << 16 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].min_time;
	swipe_buf[4] = (u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].max_time << 16 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].max_time;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE2_INDEX_AND_ENABLE, &swipe_buf[0], sizeof(u32) * 5);

	swipe_buf[5] = (u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].wrong_dir_thres << 8 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].wrong_dir_thres;
	swipe_buf[6] = (u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].init_ratio_chk_dist << 8 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].init_ratio_chk_dist;
	swipe_buf[7] = (u32)quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].init_ratio_thres << 8 |
		(u32)quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].init_ratio_thres;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE2_WRONG_DIRECTION_THD, &swipe_buf[5], sizeof(u32) * 3);

	TOUCH_I("%s: DISPLAY_AREA_ID_%d: swipe gesture[%s], enable=%d, swipe_buf[0]=0x%x\n",
			__func__, display_area_id, lpwg_gesture_str[gesture], enable, swipe_buf[0]);

	return 0;
}

static int sw82906_set_fake_off_swipe(
		struct device *dev, int display_area_id, bool enable, struct active_area *area_data) {
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	struct swipe_ctrl *fake_off_swipe = &ts->display_area_status[display_area_id].fake_off_swipe[0];
	u32 swipe_buf[10] = {0, };

	TOUCH_TRACE();

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	fake_off_swipe[FAKE_OFF_SWIPE_UP].enable = enable;
	fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].enable = enable;
	fake_off_swipe[FAKE_OFF_SWIPE_DOWN].enable = enable;
	fake_off_swipe[FAKE_OFF_SWIPE_LEFT].enable = enable;

	sw82906_set_fake_off_swipe_active_area(dev, display_area_id, area_data);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	swipe_buf[0] = (display_area_id << 16) |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].enable << 12 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].enable << 8 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].enable << 4 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].enable;
	swipe_buf[1] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].distance << 24 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].distance << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].distance << 8 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].distance;
	swipe_buf[2] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].ratio_thres << 24 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].ratio_thres << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].ratio_thres << 8 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].ratio_thres;
	swipe_buf[3] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].min_time << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].min_time;
	swipe_buf[4] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].min_time << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].min_time;
	swipe_buf[5] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].max_time << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].max_time;
	swipe_buf[6] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].max_time << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].max_time;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_INDEX_AND_ENABLE, &swipe_buf[0], sizeof(u32) * 7);

	swipe_buf[7] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].wrong_dir_thres << 24 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].wrong_dir_thres << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].wrong_dir_thres << 8 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].wrong_dir_thres;
	swipe_buf[8] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].init_ratio_chk_dist << 24 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].init_ratio_chk_dist << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].init_ratio_chk_dist << 8 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].init_ratio_chk_dist;
	swipe_buf[9] = (u32)fake_off_swipe[FAKE_OFF_SWIPE_DOWN].init_ratio_thres << 24 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_UP].init_ratio_thres << 16 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].init_ratio_thres << 8 |
		(u32)fake_off_swipe[FAKE_OFF_SWIPE_LEFT].init_ratio_thres;
	ret = sw82906_reg_write(dev, ABT_CMD + SWIPE_WRONG_DIRECTION_THD, &swipe_buf[7], sizeof(u32) * 3);

	TOUCH_I("%s: DISPLAY_AREA_ID_%d: enable=%d, swipe_buf[0]=0x%x\n",
			__func__, display_area_id, enable, swipe_buf[0]);

	return 0;
}

static void sw82906_init_swipe_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	/* Display Front (LG PAY) 'L, R, U, D' */
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].ratio_thres = 58;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].init_ratio_chk_dist = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].init_ratio_thres = 100;

	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].ratio_thres = 58;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].init_ratio_chk_dist = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].init_ratio_thres = 100;

	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].distance = 20;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].ratio_thres = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].init_ratio_chk_dist = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].init_ratio_thres = 100;

	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_DOWN].distance = 20;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_DOWN].ratio_thres = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_DOWN].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_DOWN].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_DOWN].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_DOWN].init_ratio_chk_dist = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_DOWN].init_ratio_thres = 100;

	/* Display Front (Quick Tool) 'L, R' */
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].ratio_thres = 58;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].init_ratio_chk_dist = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].init_ratio_thres = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].start_border_area.x1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].start_border_area.y1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].start_border_area.x2 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].start_border_area.y2 = 200;

	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].ratio_thres = 58;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].init_ratio_chk_dist = 4;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].init_ratio_thres = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].start_border_area.x1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].start_border_area.y1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].start_border_area.x2 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].start_border_area.y2 = 200;

	/* Display Front (FAKE OFF) 'L, R, U, D' */
	// Not Used

	/* Display Rear (LG PAY) 'L, R, U, D' */
	// Not Used

	/* Display Rear (Quick Tool) 'L, R' */
	// Not Used

	/* Display Rear (FAKE OFF) 'L, R, U, D' */
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].ratio_thres = 255;			// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].init_ratio_chk_dist = 255;	// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].init_ratio_thres = 255;		// No direction

	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].ratio_thres = 255;			// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].init_ratio_chk_dist = 255;	// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].init_ratio_thres = 255;		// No direction

	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].ratio_thres = 255;				// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].init_ratio_chk_dist = 255;		// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].init_ratio_thres = 255;		// No direction

	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].distance = 15;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].ratio_thres = 255;			// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].min_time = 4;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].max_time = 150;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].wrong_dir_thres = 5;
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].init_ratio_chk_dist = 255;	// No direction
	ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].init_ratio_thres = 255;		// No direction
}

static void sw82906_init_display_area_activation_info(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int boot_mode = TOUCH_NORMAL_BOOT;

	TOUCH_TRACE();

	d->display_area_split_info[DISPLAY_AREA_ID_0].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_ENABLE;
	d->display_area_split_info[DISPLAY_AREA_ID_0].pen_en = 0;
	d->display_area_split_info[DISPLAY_AREA_ID_0].finger_en = 1;
	d->display_area_split_info[DISPLAY_AREA_ID_0].magic = 0;
	d->display_area_split_info[DISPLAY_AREA_ID_0].display_mode = 1;

	d->display_area_split_info[DISPLAY_AREA_ID_1].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_DISABLE;
	d->display_area_split_info[DISPLAY_AREA_ID_1].pen_en = 0;
	d->display_area_split_info[DISPLAY_AREA_ID_1].finger_en = 1;
	d->display_area_split_info[DISPLAY_AREA_ID_1].magic = 0;
	d->display_area_split_info[DISPLAY_AREA_ID_1].display_mode = 1;

	d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_DISABLE;
	d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].pen_en = 0;
	d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].finger_en = 1;
	d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].magic = 0;
	d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].display_mode = 0;

	boot_mode = touch_check_boot_mode(dev);
	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
	case TOUCH_MINIOS_AAT:
		d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion = 1079;
		d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion = 1875;
		d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion = 1188;
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
	case TOUCH_MINIOS_MFTS_DS_FLAT:
		d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion = 1875;
		d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion = 1875;
		d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion = 1875;
		break;
	case TOUCH_CHARGER_MODE:
	case TOUCH_LAF_MODE:
	case TOUCH_RECOVERY_MODE:
		TOUCH_I("%s: Etc boot_mode(%d)!!!\n", __func__, boot_mode);
		return;
	default:
		TOUCH_E("%s: invalid boot_mode = %d\n", __func__, boot_mode);
		return;
	}

	TOUCH_I("%s: set End Position (%d, %d, %d)\n", __func__,
			d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion,
			d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion,
			d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion);
}

static void sw82906_init_quick_tool_abs_info(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();

	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.border.x1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.border.y1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.border.x2 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.border.y2 = 100;

	ts->display_area_status[DISPLAY_AREA_ID_1].quick_tool_abs.enable = false;
	ts->display_area_status[DISPLAY_AREA_ID_1].quick_tool_abs.border.x1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_1].quick_tool_abs.border.y1 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_1].quick_tool_abs.border.x2 = 100;
	ts->display_area_status[DISPLAY_AREA_ID_1].quick_tool_abs.border.y2 = 100;
}

static int sw82906_set_quick_tool_abs_active_area(struct device *dev, int display_area_id, struct active_area *area_data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct active_area *area = &ts->display_area_status[display_area_id].quick_tool_abs.area;
	struct active_area *border_area = &ts->display_area_status[display_area_id].quick_tool_abs.border;
	int ret = 0;
	u32 abs_enable = 0;
	u32 active_area[2] = {0, };

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	if (area_data != NULL) {
		area->x1 = area_data->x1 - border_area->x1;
		area->y1 = area_data->y1 - border_area->y1;
		area->x2 = area_data->x2 + border_area->x2;
		area->y2 = area_data->y2 + border_area->y2;
		if (area->x1 < ts->flexible_display.area[display_area_id].area.left_top.x)
			area->x1 = ts->flexible_display.area[display_area_id].area.left_top.x;
		if (area->y1 < ts->flexible_display.area[display_area_id].area.left_top.y)
			area->y1 = ts->flexible_display.area[display_area_id].area.left_top.y;
		if (area->x2 > ts->flexible_display.area[display_area_id].area.right_bottom.x)
			area->x2 = ts->flexible_display.area[display_area_id].area.right_bottom.x;
		if (area->y2 > ts->flexible_display.area[display_area_id].area.right_bottom.y)
			area->y2 = ts->flexible_display.area[display_area_id].area.right_bottom.y;
	}

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	abs_enable = (display_area_id << 16) | ts->display_area_status[display_area_id].quick_tool_abs.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + LPWG_ABS_INDEX_AND_ENABLE, &abs_enable, sizeof(abs_enable));

	active_area[0] = (area->y1 << 16) | area->x1;
	active_area[1] = (area->y2 << 16) | area->x2;
	ret = sw82906_reg_write(dev, ABT_CMD + LPWG_ABS_ACTIE_AREA_START, active_area, sizeof(active_area));

	TOUCH_I("%s - display_area_id(%d): left_top.x,y(%d, %d), right_bottom.x,y(%d, %d)\n", __func__,
			display_area_id, area->x1, area->y1, area->x2, area->y2);

	return ret;
}

static int sw82906_set_quick_tool_abs(
		struct device *dev, int display_area_id, bool enable, struct active_area *area_data) {
	struct touch_core_data *ts = to_touch_core(dev);
	u32 abs_enable = 0;
	int ret = 0;

	TOUCH_TRACE();

	// 1. Saving received data
	if (!(display_area_id >= DISPLAY_AREA_ID_0 && display_area_id < DISPLAY_AREA_ID_MAX)) {
		TOUCH_E("Not supported display_area_id : %d\n", display_area_id);
		return -ERANGE;
	}

	if (enable) {
		ts->display_area_status[display_area_id].quick_tool_abs.enable = 1;
	} else {
		ts->display_area_status[display_area_id].quick_tool_abs.enable = 0;
	}

	sw82906_set_quick_tool_abs_active_area(dev, display_area_id, area_data);

	// 2. Write data to IC register
	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	abs_enable = (display_area_id << 16) | ts->display_area_status[display_area_id].quick_tool_abs.enable;
	ret = sw82906_reg_write(dev, ABT_CMD + LPWG_ABS_INDEX_AND_ENABLE, &abs_enable, sizeof(abs_enable));

	TOUCH_I("%s: DISPLAY_AREA_ID_%d: quick_tool_abs.enable=%d, abs_enable=0x%x\n",
			__func__, display_area_id,
			ts->display_area_status[display_area_id].quick_tool_abs.enable,
			abs_enable);

	return ret;
}

static int sw82906_swipe_enable(struct device *dev, bool enable)
{
	return 0;
}

static void sw82906_print_lpwg_failreason(struct device *dev, int type, int count)
{
	u64 buf = 0;
	u8 data = 0;
	u8 fail_num = 0;
	int j,ret = 0;

	switch (type) {
	case LPWG_DEBUG_KNOCK_ON_FRONT:
		ret = sw82906_reg_read(dev, ABT_CMD + KNOCK_ON_FRONT_FAILREASON_BUF,
				&buf, sizeof(buf));
		fail_num = LPWG_FAILREASON_KNOCK_ON_NUM;
		break;
	case LPWG_DEBUG_KNOCK_ON_REAR:
		ret = sw82906_reg_read(dev, ABT_CMD + KNOCK_ON_REAR_FAILREASON_BUF,
				&buf, sizeof(buf));
		fail_num = LPWG_FAILREASON_KNOCK_ON_NUM;
		break;
	case LPWG_DEBUG_SWIPE:
		ret = sw82906_reg_read(dev, ABT_CMD + SWIPE_FAILREASON_BUF,
				&buf, sizeof(buf));
		fail_num = LPWG_FAILREASON_SWIPE_NUM;
		break;
	case LPWG_DEBUG_LONGPRESS:
		ret = sw82906_reg_read(dev, ABT_CMD + LONGPRESS_FAILREASON_BUF,
				&buf, sizeof(buf));
		fail_num = LPWG_FAILREASON_LONGPRESS_NUM;
		break;
	case LPWG_DEBUG_ONETAP:
		ret = sw82906_reg_read(dev, ABT_CMD + ONETAP_FAILREASON_BUF,
				&buf, sizeof(buf));
		fail_num = LPWG_FAILREASON_ONETAP_NUM;
		break;
	default:
		break;
	}

	if (ret < 0 || (buf == 0))
		return;

	if (count > FAIL_REASON_MAX_CNT) {
		count = FAIL_REASON_MAX_CNT;
	}

	for (j = 0; j < count; j++) {

		data = ((buf >> (8*j)) & 0xFF);

		if (data > 0 && data < fail_num) {
			if (type == LPWG_DEBUG_KNOCK_ON_FRONT) {
				TOUCH_I("[%s]-DBG[%d/%d] = %s\n",
						lpwg_failreason_type_str[type], j, count,
						lpwg_failreason_knock_on_str[data]);
			} else if (type == LPWG_DEBUG_KNOCK_ON_REAR) {
				TOUCH_I("[%s]-DBG[%d/%d] = %s\n",
						lpwg_failreason_type_str[type], j, count,
						lpwg_failreason_knock_on_str[data]);
			} else if (type == LPWG_DEBUG_SWIPE) {
				TOUCH_I("[%s]-DBG[%d/%d] = %s\n",
						lpwg_failreason_type_str[type], j, count,
						lpwg_failreason_swipe_str[data]);
			} else if (type == LPWG_DEBUG_LONGPRESS) {
				TOUCH_I("[%s]-DBG[%d/%d] = %s\n",
						lpwg_failreason_type_str[type], j, count,
						lpwg_failreason_longpress_str[data]);
			} else if (type == LPWG_DEBUG_ONETAP) {
				TOUCH_I("[%s]-DBG[%d/%d] = %s\n",
						lpwg_failreason_type_str[type], j, count,
						lpwg_failreason_onetap_str[data]);
			}
		} else {
			break;
		}
	}
}

static void sw82906_get_lpwg_failreason(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret, i = 0;
	u32 rdata[2] = {0, };
	u8 count[5] = {0, };

	if (!d->lpwg_failreason_ctrl)
		return;

	ret = sw82906_reg_read(dev, ABT_CMD + LPWG_FAILREASON_COUNT1,
			&rdata[0], sizeof(u32));
	ret = sw82906_reg_read(dev, ABT_CMD + LPWG_FAILREASON_COUNT2,
			&rdata[1], sizeof(u32));

	if (ret < 0)
		return;

	/*
	 * 0 : LPWG_DEBUG_KNOCK_ON_FRONT
	 * 1 : LPWG_DEBUG_KNOCK_ON_REAR
	 * 2 : LPWG_DEBUG_SWIPE
	 * 3 : LPWG_DEBUG_LONGPRESS
	 * 4 : LPWG_DEUBG_ONETAP
	 */
	for (i = 0; i < 4; i++) {
		count[i] = ((rdata[0] >> 8 * i) & 0xFF);
	}
	count[4] = rdata[1] & 0xFF;

	if (count[LPWG_DEBUG_KNOCK_ON_FRONT]) { /* Knock-on front */
		sw82906_print_lpwg_failreason(dev, LPWG_DEBUG_KNOCK_ON_FRONT,
				count[LPWG_DEBUG_KNOCK_ON_FRONT]);
	}

	if (count[LPWG_DEBUG_KNOCK_ON_REAR]) { /* Knock-on rear */
		sw82906_print_lpwg_failreason(dev, LPWG_DEBUG_KNOCK_ON_REAR,
				count[LPWG_DEBUG_KNOCK_ON_REAR]);
	}

	if (count[LPWG_DEBUG_SWIPE]) { /* Swipe */
		sw82906_print_lpwg_failreason(dev, LPWG_DEBUG_SWIPE,
				count[LPWG_DEBUG_SWIPE]);
	}

	if (count[LPWG_DEBUG_LONGPRESS]) { /* LongPress */
		sw82906_print_lpwg_failreason(dev, LPWG_DEBUG_LONGPRESS,
				count[LPWG_DEBUG_LONGPRESS]);
	}

	if (count[LPWG_DEBUG_ONETAP]) { /* LongPress */
		sw82906_print_lpwg_failreason(dev, LPWG_DEBUG_ONETAP,
				count[LPWG_DEBUG_ONETAP]);
	}

}

static void sw82906_set_lpwg_failreason(struct device *dev, int type, bool enable)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	u32 data = 0, addr = 0;

	if (!d->lpwg_failreason_ctrl) {
		TOUCH_I("%s failreason not set\n", __func__);
		return;
	}

	switch (type) {
	case LPWG_DEBUG_KNOCK_ON_FRONT:
		addr = ABT_CMD + LPWG_FAILREASON_REPORT_CTRL1;
		data |= enable;
		break;
	case LPWG_DEBUG_KNOCK_ON_REAR:
		addr = ABT_CMD + LPWG_FAILREASON_REPORT_CTRL1;
		data |= (enable << 8);
		break;
	case LPWG_DEBUG_SWIPE:
		addr = ABT_CMD + LPWG_FAILREASON_REPORT_CTRL1;
		data |= (enable << 16);
		break;
	case LPWG_DEBUG_LONGPRESS:
		addr = ABT_CMD + LPWG_FAILREASON_REPORT_CTRL1;
		data |= (enable << 24);
		break;
	case LPWG_DEBUG_ONETAP:
		addr = ABT_CMD + LPWG_FAILREASON_REPORT_CTRL2;
		data |= enable;
		break;
	default:
		break;
	}

	TOUCH_I("set lpwg_failreason type:%d, data[0x%x]\n", type, data);

	sw82906_reg_write(dev, addr, &data, sizeof(data));
}

static const char *driving_cmd_str[LCD_MODE_NUM] = {
	"LCD_MODE_U0",
	"LCD_MODE_U3_PARTIAL",
	"LCD_MODE_U2",
	"LCD_MODE_U3",
	"LCD_MODE_STOP",
	"LCD_MODE_UNKNOWN",
};

int sw82906_tc_driving(struct device *dev, int new_status)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct project_param *param = &d->p_param;
	int old_status = d->driving_mode;
	u32 ctrl = 0;
	u32 addr = TC_CMD + tc_driving_ctl;
	int i = 0;

	u32 rdata = 0;

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	if(!(param->used_mode & (1<<new_status))) {
		TOUCH_D(TRACE, "tc_driving canceled (new_status:%d)\n", new_status);
		return 0;
	}

	if (old_status == new_status && !atomic_read(&ts->state.mfts)) {
		TOUCH_I("%s: skip (old_status:%d, new_status:%d)\n",
				__func__, old_status, new_status);
		return 0;
	}

	if (new_status != LCD_MODE_STOP) {
		ctrl = 0x04;
		sw82906_reg_write(dev, addr, &ctrl, sizeof(ctrl));
		TOUCH_I("sw82906_tc_stop(0x%x) = LCD_MODE_STOP(%d), (0x%x)\n",
				addr, new_status, ctrl);
		touch_msleep(param->sys_tc_stop_delay);
		TOUCH_I("tc_stop delay %dms\n", param->sys_tc_stop_delay);
	}

	d->driving_mode = new_status;

	switch (new_status) {
		case LCD_MODE_U0:
			if (atomic_read(&ts->state.active_pen)) {
				ctrl = 0x0003;
			} else {
				ctrl = 0x0001;
			}
			break;
		case LCD_MODE_U3:
			if (atomic_read(&ts->state.active_pen)) {
				if (atomic_read(&ts->state.wireless)) {
					if(atomic_read(&ts->state.film_mode)) {
						ctrl = 0x130b;
					} else {
						ctrl = 0x1303;
					}
					TOUCH_I("%s: wireless charger (ctrl:0x%X)\n",
							__func__, ctrl);
				} else {
					if(atomic_read(&ts->state.film_mode)) {
						TOUCH_I("%s: protection film mode  = %d\n", __func__,
							atomic_read(&ts->state.film_mode));
						ctrl = 0x030b;
					} else {
						/*
						 * To Do
						if (ts->aes_mode == 2)
							ctrl = 0x0303;
						else if (ts->aes_mode == 0)
							ctrl = 0x1303;
						*/
					}
				}
			} else {
				if(atomic_read(&ts->state.film_mode)) {
					TOUCH_I("%s: protection film mode  = %d\n", __func__,
						atomic_read(&ts->state.film_mode));
					ctrl = 0x830b;
				} else  {
					//ctrl = 0x8303;
					if(atomic_read(&ts->state.mfts))
						ctrl = 0x0301;
					else
						ctrl = 0x03c1;
				}
			}
			break;

		case LCD_MODE_STOP:
			ctrl = 0x0004;
			break;
		default:
			ctrl = -1;
			break;
	}

	if (ctrl < 0) {
		TOUCH_I("invalid mode change, new_status : %d", new_status);
		return -EINVAL;
	}

	sw82906_reg_write(dev, addr, &ctrl, sizeof(ctrl));
	TOUCH_I("sw82906_tc_driving(0x%x) = %s(%d), (0x%x)\n",
			addr, driving_cmd_str[new_status], new_status, ctrl);

	if (new_status == LCD_MODE_STOP) {
		touch_msleep(param->sys_tc_stop_delay);
		TOUCH_I("tc_stop delay %dms\n", param->sys_tc_stop_delay);
	} else {
		touch_msleep(param->sys_tc_start_delay);
		TOUCH_I("tc_start delay %dms\n", param->sys_tc_start_delay);
	}

#if 0
	/* status check */
	sw82906_reg_read(dev, TC_IC_STATUS, (u8 *)&rdata, sizeof(u32));
	TOUCH_I("read ic_status(%x) = %x\n", TC_IC_STATUS, rdata);

	sw82906_reg_read(dev, TC_STS, (u8 *)&rdata, sizeof(u32));
	TOUCH_I("read tc_status(%x) = %x\n", TC_STS, rdata);

	sw82906_reg_read(dev, TC_CMD, (u8 *)&rdata, sizeof(rdata));
	TOUCH_I("TC_CMD status : 0x%02X\n", rdata);
#endif

	if (param->reg_debug) {
		if (new_status != LCD_MODE_STOP) {
			for (i = 0; i < 10; i++) {
				sw82906_reg_read(dev, TC_IC_STATUS, (u8 *)&rdata, sizeof(u32));
				TOUCH_I("read ic_status(%x) = %x\n", TC_IC_STATUS, rdata);

				sw82906_reg_read(dev, TC_STS, (u8 *)&rdata, sizeof(u32));
				TOUCH_I("read tc_status(%x) = %x\n", TC_STS, rdata);
				if ((rdata & 0x1F) == 0x7)
					return 0;
				touch_msleep(50);
			}
		}
	}

	return 0;
}

#ifdef CONFIG_LGE_TOUCH_PEN
static int sw82906_release_pen_event(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	TOUCH_TRACE();
	ts->pdata.in_range = 0;
	ts->pdata.contact = 0;
	pen_input_handler(dev);
	return 0;
}
#endif

static int sw82906_setup_q_sensitivity(struct device *dev, int enable)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;;

	d->q_sensitivity = enable; /* 1=enable touch, 0=disable touch */

	ret = sw82906_reg_write(dev, ABT_CMD + COVER_SENSITIVITY,
			&d->q_sensitivity, sizeof(u32));

	TOUCH_I("%s : %s(%d)\n", __func__,
			(d->q_sensitivity) ? "SENSITIVE" : "NORMAL", (d->q_sensitivity));

	return ret;
}

int sw82906_sleep_ctrl(struct device *dev, int new_status)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int old_status = atomic_read(&ts->state.sleep);
	int ret = 0;

	TOUCH_TRACE();

	if (old_status == new_status) {
		TOUCH_I("%s: skip (old_status:%d, new_status:%d)\n",
				__func__, old_status, new_status);
		return ret;
	}

	if (new_status == IC_NORMAL) {
		TOUCH_I("%s: IC NORMAL\n", __func__);
		sw82906_power(dev, POWER_ON);
		sw82906_reset_ctrl(dev, HW_RESET_SYNC);
		touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
	} else if (new_status == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC DEEP SLEEP\n", __func__);
		touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
		ret = sw82906_tc_driving(dev, LCD_MODE_STOP);
		if (!atomic_read(&ts->state.incoming_call)) {
			sw82906_power(dev, POWER_OFF);
		} else {
			TOUCH_I("%s: skip sleep ctrl in incoming_call\n", __func__);
		}
	} else {
		TOUCH_E("invalid new_status:%d\n", new_status);
		ret = -EINVAL;
	}

	return ret;
}

static int sw82906_set_display_area_moving(struct device *dev, bool moving) {
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;
	u32 data = 0;

	if (moving == d->split_display_area_moving) {
		return 0;
	}
	d->split_display_area_moving = moving; // renew  a moving state

	data = (u32)moving;

	TOUCH_I("%s: Now display moving %s(%d)", __func__, (moving) ? "START" : "STOP", moving);
	ret = sw82906_reg_write(dev, SPLIT_DISPLAY_AREA_MOVING, &data, sizeof(u32));
	if (ret < 0) {
		TOUCH_E("%s: write data failed, SPLIT_DISPLAY_AREA_MOVING data(0x%08x) ret(%d)\n", __func__, data, ret);
		return -ENODEV;
	}

	return ret;
}

static int sw82906_set_display_area_split_info(struct device *dev,
		u8 type_display_area, u8 display_area_id) {
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct sw82906_display_area_split_info *saved_display_area_split_info = NULL;
	struct project_param *param = &d->p_param;
	u16 addr = 0;
	u32 data = 0;
	int ret = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

	if (type_display_area == TYPE_DISPLAY_AREA) {
		saved_display_area_split_info = &(d->display_area_split_info[display_area_id]);

		switch (display_area_id) {
			case DISPLAY_AREA_ID_0:
				addr = SPLIT_DISPLAY_AREA_0;
				break;
			case DISPLAY_AREA_ID_1:
				addr = SPLIT_DISPLAY_AREA_1;
				break;
			default:
				TOUCH_E("%s: invalid setting - type : %d, area_id : %d)\n",
						__func__, type_display_area, display_area_id);
				return -EINVAL;
		}
	} else if (type_display_area == TYPE_DISPLAY_AREA_INTERSPACE) {
		saved_display_area_split_info = &(d->display_area_interspace_split_info[display_area_id]);

		switch (display_area_id) {
			case DISPLAY_AREA_INTERSPACE_0_1:
				addr = SPLIT_DISPLAY_AREA_INTERSPACE_0_1;
				break;
			default:
				TOUCH_E("%s: invalid setting - type : %d, area_id : %d)\n",
						__func__, type_display_area, display_area_id);
				return -EINVAL;
		}
	} else {
		TOUCH_E("Invalid type_display_area: %d\n", type_display_area);
		return -EINVAL;
	}

	data |= (saved_display_area_split_info->area_activation << SPLIT_DISPLAY_AREA_ENABLE_SHIFT_BIT) & SPLIT_DISPLAY_AREA_ENABLE_COMPARE_BIT;
	data |= (saved_display_area_split_info->pen_en<< SPLIT_DISPLAY_AREA_PEN_ENABLE_SHIFT_BIT) & SPLIT_DISPLAY_AREA_PEN_ENABLE_COMPARE_BIT;
	data |= (saved_display_area_split_info->finger_en << SPLIT_DISPLAY_AREA_FINGER_ENABLE_SHIFT_BIT) & SPLIT_DISPLAY_AREA_FINGER_ENABLE_COMPARE_BIT;
	data |= (saved_display_area_split_info->magic << SPLIT_DISPLAY_AREA_MAGIC_SHIFT_BIT) & SPLIT_DISPLAY_AREA_MAGIC_COMPARE_BIT;
	data |= (saved_display_area_split_info->display_mode << SPLIT_DISPLAY_AREA_DISPLAY_MODE_SHIFT_BIT) & SPLIT_DISPLAY_AREA_DISPLAY_MODE_COMPARE_BIT;
	data |= (saved_display_area_split_info->resolustion << SPLIT_DISPLAY_AREA_RESOLUTION_SHIFT_BIT) & SPLIT_DISPLAY_AREA_RESOLUTION_COMPARE_BIT;

	TOUCH_I("%s: write type(%d), area_id(%d), addr(0x%04x), data(0x%08x)", __func__, type_display_area, display_area_id, addr, data);

	ret = sw82906_reg_write(dev, addr, &data, sizeof(u32));
	if (ret < 0) {
		TOUCH_E("%s: write data failed, addr(0x%x) data(0x%08x) ret(%d)\n", __func__, addr, data, ret);
		return -ENODEV;
	}

	if (param->reg_debug) {
		ret = sw82906_reg_read(dev, addr, &data, sizeof(u32));
		if (ret < 0) {
			TOUCH_E("%s: read data failed, ret(%d)\n", __func__, ret);
			return -ENODEV;
		}
		TOUCH_I("%s: read  type(%d), area_id(%d), addr(0x%04x), data(0x%08x) ret(%d)", __func__, type_display_area, display_area_id, addr, data, ret);
	}

	return ret;
}

static int sw82906_set_display_area_status(struct device *dev, int display_area_id)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.fb) == FB_SUSPEND) {
		/* SUSPEND */
		if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.screen == SCREEN_OFF &&
				ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.screen == SCREEN_OFF) {
			if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.mode == LPWG_NONE &&
					ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.mode == LPWG_NONE &&
					ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.enable == false) {
				TOUCH_I("%s: [GLOBAL_SET] Both of display_area_id 0 & 1, SCREEN_OFF and EVERY LPWG FUNCTION DISABLE - IC_DEEP_SLEEP\n", __func__);
				sw82906_sleep_ctrl(dev, IC_DEEP_SLEEP);
				return ret;
			} else if ((ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.sensor == PROX_NEAR ||
					ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.qcover == HALL_NEAR) &&
					(ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.sensor == PROX_NEAR ||
					ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.qcover == HALL_NEAR)) {
				TOUCH_I("%s: [GLOBAL_SET] Both of display_area_id 0 & 1, PROX_NEAR or COVER_CLOSE - IC_DEEP_SLEEP\n", __func__);
				sw82906_sleep_ctrl(dev, IC_DEEP_SLEEP);
				return ret;
			} else {
				TOUCH_I("%s: [GLOBAL_SET] Both of display_area_id 0 & 1, SCREEN_OFF - TC_DRIVNG U0\n", __func__);
				if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
					sw82906_sleep_ctrl(dev, IC_NORMAL);
				}
				sw82906_tc_driving(dev, LCD_MODE_U0);
			}
		} else {
			/* SCREEN_ON / SCREEN_FAKE_OFF */
			TOUCH_I("%s: [GLOBAL_SET] Display_area_id 0 OR 1, PRE_RESUME\n", __func__);
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
				sw82906_sleep_ctrl(dev, IC_NORMAL);
			}
			sw82906_get_lpwg_failreason(dev);
			return ret;
		}
	} else {
		/* RESUME */
		if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.screen == SCREEN_OFF &&
				ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.screen == SCREEN_OFF) {
			if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.mode == LPWG_NONE &&
					ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.mode == LPWG_NONE &&
					ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_UP].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_UP].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].udf_longpress.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].show_aod_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].show_aod_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].fake_off_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.enable == false &&
					ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.enable == false) {
				TOUCH_I("%s: [GLOBAL_SET] Both of display_area_id 0 & 1, SCREEN_OFF and EVERY LPWG FUNCTION DISABLE - IC_DEEP_SLEEP\n", __func__);
				sw82906_sleep_ctrl(dev, IC_DEEP_SLEEP);
				return ret;
			} else if ((ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.sensor == PROX_NEAR ||
					ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.qcover == HALL_NEAR) &&
					(ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.sensor == PROX_NEAR ||
					ts->display_area_status[DISPLAY_AREA_ID_1].lpwg.qcover == HALL_NEAR)) {
				TOUCH_I("%s: [GLOBAL_SET] Both of display_area_id 0 & 1, PROX_NEAR or COVER_CLOSE - IC_DEEP_SLEEP\n", __func__);
				sw82906_sleep_ctrl(dev, IC_DEEP_SLEEP);
				return ret;
			} else {
				TOUCH_I("%s: [GLOBAL_SET] Both of display_area_id 0 & 1, PRE_SUSPEND\n", __func__);
				// No return : Individual settings for Knock-on when PRE_SUSPEND
			}
		} else {
			/* SCREEN_ON / SCREEN_FAKE_OFF */
			TOUCH_I("%s: [GLOBAL_SET] Display_area_id 0 OR 1, SCREEN_ON/FAKE_OFF - TC_DRIVING U3\n", __func__);
			if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
				sw82906_sleep_ctrl(dev, IC_NORMAL);
			}
			sw82906_tc_driving(dev, LCD_MODE_U3);
		}
	}

	if (ts->display_area_status[display_area_id].lpwg.screen == SCREEN_OFF ||
			ts->display_area_status[display_area_id].lpwg.screen == SCREEN_FAKE_OFF) {
		TOUCH_I("%s: [INDIVIDUAL_SET] DISPLAY_AREA_ID_%d, SCREEN_OFF/FAKE_OFF\n", __func__, display_area_id);
		touch_report_cancel_event_display_area(ts, display_area_id);
		if (ts->display_area_status[display_area_id].lpwg.mode == LPWG_NONE &&
					ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_UP].enable == false &&
					ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable == false &&
					ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].enable == false &&
					ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[display_area_id].fake_off_swipe[FAKE_OFF_SWIPE_UP].enable == false &&
					ts->display_area_status[display_area_id].fake_off_swipe[FAKE_OFF_SWIPE_RIGHT].enable == false &&
					ts->display_area_status[display_area_id].fake_off_swipe[FAKE_OFF_SWIPE_DOWN].enable == false &&
					ts->display_area_status[display_area_id].fake_off_swipe[FAKE_OFF_SWIPE_LEFT].enable == false &&
					ts->display_area_status[display_area_id].quick_tool_abs.enable == false &&
					ts->display_area_status[display_area_id].udf_longpress.enable == false &&
					ts->display_area_status[display_area_id].show_aod_onetap.enable == false &&
					ts->display_area_status[display_area_id].fake_off_onetap.enable == false &&
					ts->display_area_status[display_area_id].ai_pick_double_tap.enable == false) {
			TOUCH_I("%s: [INDIVIDUAL_SET] DISPLAY_AREA_ID_%d, SCREEN_OFF/FAKE_OFF, LPWG_NONE (ACTIVATION_DISABLE)\n", __func__, display_area_id);
			d->display_area_split_info[display_area_id].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_DISABLE;
		} else if (ts->display_area_status[display_area_id].lpwg.qcover == HALL_NEAR) {
			TOUCH_I("%s: [INDIVIDUAL_SET] DISPLAY_AREA_ID_%d, COVER_CLOSE - ACTIVATION_DISABLE\n", __func__, display_area_id);
			d->display_area_split_info[display_area_id].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_DISABLE;
		} else if (ts->display_area_status[display_area_id].lpwg.sensor == PROX_NEAR) {
			TOUCH_I("%s: [INDIVIDUAL_SET] DISPLAY_AREA_ID_%d, PROX_NEAR - ACTIVATION_DISABLE\n", __func__, display_area_id);
			d->display_area_split_info[display_area_id].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_DISABLE;
		} else {
			TOUCH_I("%s: [INDIVIDUAL_SET] DISPLAY_AREA_ID_%d, SCREEN_OFF/FAKE_OFF, MODE_LPWG (ACTIVATION_ENABLE)\n", __func__, display_area_id);
			d->display_area_split_info[display_area_id].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_ENABLE;
			d->display_area_split_info[display_area_id].display_mode = SPLIT_DISPLAY_AREA_MODE_LPWG;

			// Set again all lpwg function for reset
			sw82906_set_knock_on(dev, display_area_id, ts->display_area_status[display_area_id].lpwg.mode);

			if (ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_UP].enable) {
				sw82906_set_lg_pay_swipe(dev, display_area_id, SWIPE_UP, true, NULL);
			}
			if (ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_LEFT].enable) {
				sw82906_set_lg_pay_swipe(dev, display_area_id, SWIPE_LEFT, true, NULL);
			}
			if (ts->display_area_status[display_area_id].lg_pay_swipe[LG_PAY_SWIPE_RIGHT].enable) {
				sw82906_set_lg_pay_swipe(dev, display_area_id, SWIPE_RIGHT, true, NULL);
			}
			if (ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_LEFT].enable) {
				sw82906_set_quick_tool_swipe(dev, display_area_id, SWIPE_LEFT, true, NULL);
			}
			if (ts->display_area_status[display_area_id].quick_tool_swipe[QUICK_TOOL_SWIPE_RIGHT].enable) {
				sw82906_set_quick_tool_swipe(dev, display_area_id, SWIPE_RIGHT, true, NULL);
			}
			if (ts->display_area_status[display_area_id].fake_off_swipe[LG_PAY_SWIPE_UP].enable ||
					ts->display_area_status[display_area_id].fake_off_swipe[LG_PAY_SWIPE_RIGHT].enable ||
					ts->display_area_status[display_area_id].fake_off_swipe[LG_PAY_SWIPE_DOWN].enable ||
					ts->display_area_status[display_area_id].fake_off_swipe[LG_PAY_SWIPE_LEFT].enable) {
				sw82906_set_fake_off_swipe(dev, display_area_id, true, NULL);
			}
			if (ts->display_area_status[display_area_id].quick_tool_abs.enable) {
				sw82906_set_quick_tool_abs(dev, display_area_id, true, NULL);
			}
			if (ts->display_area_status[display_area_id].udf_longpress.enable) {
				sw82906_set_udf_longpress(dev, display_area_id, true);
			}
			if (ts->display_area_status[display_area_id].show_aod_onetap.enable) {
				sw82906_set_show_aod_onetap(dev, display_area_id, true);
			}
			if (ts->display_area_status[display_area_id].fake_off_onetap.enable) {
				sw82906_set_fake_off_onetap(dev, display_area_id, true, NULL);
			}
			if (ts->display_area_status[display_area_id].ai_pick_double_tap.enable) {
				sw82906_set_ai_pick_double_tap(dev, display_area_id, true);
			}
		}
	} else if (ts->display_area_status[display_area_id].lpwg.screen == SCREEN_ON) {
		TOUCH_I("%s: [INDIVIDUAL_SET] DISPLAY_AREA_ID_%d, SCREEN_ON - MODE_ABS (ACTIVATION_ENABLE)\n", __func__, display_area_id);
		d->display_area_split_info[display_area_id].area_activation = SPLIT_DISPLAY_AREA_ACTIVATION_ENABLE;
		d->display_area_split_info[display_area_id].display_mode = SPLIT_DISPLAY_AREA_MODE_ABS;
		sw82906_get_lpwg_failreason(dev);
	}
	sw82906_set_display_area_split_info(dev, TYPE_DISPLAY_AREA, display_area_id);

	return ret;
}

static int sw82906_display_area_status(struct device *dev, void *param)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int *value = (int *)param;
	int ret = 0;
	int display_area_id = 0;

	display_area_id = value[0];

	ts->display_area_status[display_area_id].display_area_id = value[0];
	ts->display_area_status[display_area_id].lpwg.screen = value[1];
	ts->display_area_status[display_area_id].lpwg.mode = value[2];
	ts->display_area_status[display_area_id].lpwg.sensor = value[3];
	ts->display_area_status[display_area_id].lpwg.qcover = value[4];

	TOUCH_I("DISPLAY_AREA_STATUS_UPDATE_ALL : display_area_id[%d], screen[%s], mode[%s], sensor[%s], qcover[%s]\n",
			display_area_id,
			ts->display_area_status[display_area_id].lpwg.screen == 1 ? "ON" :
			(ts->display_area_status[display_area_id].lpwg.screen == 2 ? "FAKE_OFF" :"OFF"),
			ts->display_area_status[display_area_id].lpwg.mode == 1 ? "KNOCK_ON" : "NONE",
			ts->display_area_status[display_area_id].lpwg.sensor == 1 ? "FAR" : "NEAR",
			ts->display_area_status[display_area_id].lpwg.qcover == 1 ? "CLOSE" : "OPEN");

	ret = sw82906_set_display_area_status(dev, display_area_id);
	if (ret)
		TOUCH_E("failed to set display area status, ret:%d", ret);

	return ret;
}

static int sw82906_set_lpwg_function_info(struct device *dev, int display_area_id)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int function = 0, gesture = 0, enable = 0;
	struct active_area area = {0, };

	TOUCH_TRACE();

	function = ts->lpwg_function[display_area_id].function;
	gesture = ts->lpwg_function[display_area_id].gesture;
	enable = ts->lpwg_function[display_area_id].enable;
	area.x1 = ts->lpwg_function[display_area_id].function_area.area.left_top.x;
	area.y1 = ts->lpwg_function[display_area_id].function_area.area.left_top.y;
	area.x2 = ts->lpwg_function[display_area_id].function_area.area.right_bottom.x;
	area.y2 = ts->lpwg_function[display_area_id].function_area.area.right_bottom.y;

	switch (function) {
		case LPWG_LG_PAY:
			switch (gesture) {
				case SWIPE_UP:
				case SWIPE_RIGHT:
				case SWIPE_LEFT:
					sw82906_set_lg_pay_swipe(dev, display_area_id, gesture, enable, &area);
					break;
				default:
					goto error;
			}
			break;
		case LPWG_QUICK_TOOL:
			switch (gesture) {
				case SWIPE_LEFT:
					sw82906_set_quick_tool_swipe(dev, display_area_id, gesture, enable, &area);
					break;
				case SWIPE_RIGHT:
					sw82906_set_quick_tool_swipe(dev, display_area_id, gesture, enable, &area);
					break;
				case ABS_COORDINATE:
					sw82906_set_quick_tool_abs(dev, display_area_id, enable, &area);
					break;
				default:
					goto error;
			}
			break;
		case LPWG_AI_PICK:
			switch (gesture) {
				case DOUBLE_TAP:
					sw82906_set_ai_pick_double_tap(dev, display_area_id, enable);
					break;
				default:
					goto error;
			}
			break;
		case LPWG_UDF:
			switch (gesture) {
				case LONG_PRESS:
					sw82906_set_udf_longpress(dev, display_area_id, enable);
					break;
				default:
					goto error;
			}
			break;
		case LPWG_SHOW_AOD:
			switch (gesture) {
				case ONE_TAP:
					sw82906_set_show_aod_onetap(dev, display_area_id, enable);
					break;
				default:
					goto error;
			}
			break;
		case LPWG_FAKE_OFF:
			switch (gesture) {
				case ONE_TAP:
					sw82906_set_fake_off_onetap(dev, display_area_id, enable, &area);
					break;
				case SWIPE:
					sw82906_set_fake_off_swipe(dev, display_area_id, enable, &area);
					break;
				default:
					goto error;
			}
			break;
		default:
			goto error;
	}

	return 0;
error:
	TOUCH_E("Unknown lpwg gesture - display_area_id[%d], function[%d], gesture[%d]",
			display_area_id,
			ts->lpwg_function[display_area_id].function,
			ts->lpwg_function[display_area_id].gesture);
	return -1;
}

static int sw82906_lpwg_function_info(struct device *dev, int display_area_id)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_I("LPWG_FUNCTION_INFO : display_area_id[%d], function[%s], gesture[%s], enable[%d]\n",
			ts->lpwg_function[display_area_id].display_area_id,
			lpwg_function_str[ts->lpwg_function[display_area_id].function],
			lpwg_gesture_str[ts->lpwg_function[display_area_id].gesture],
			ts->lpwg_function[display_area_id].enable);

	TOUCH_I("LPWG_FUNCTION_AREA : left_top[%d, %d], right_bottom[%d, %d], x_offset[%d], y_offset[%d]\n",
			ts->lpwg_function[display_area_id].function_area.area.left_top.x,
			ts->lpwg_function[display_area_id].function_area.area.left_top.y,
			ts->lpwg_function[display_area_id].function_area.area.right_bottom.x,
			ts->lpwg_function[display_area_id].function_area.area.right_bottom.y,
			ts->lpwg_function[display_area_id].function_area.x_offset,
			ts->lpwg_function[display_area_id].function_area.y_offset);

	ret = sw82906_set_lpwg_function_info(dev, display_area_id);
	if (ret)
		TOUCH_E("%s: failed to sw82906_set_lpwg_function_info, ret:%d",__func__, ret);

	return ret;
}

static void sw82906_connect(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int charger_state = atomic_read(&ts->state.connect);
	int wireless_state = atomic_read(&ts->state.wireless);

	TOUCH_TRACE();
	if (atomic_read(&ts->state.dualscreen)) {
		touch_send_uevent(ts, TOUCH_UEVENT_DS_UPDATE_STATE);
		atomic_set(&ts->state.uevent, UEVENT_IDLE);
	}
	d->charger = CONNECT_NONE;

	/* wire */
	if (charger_state)
		d->charger = CONNECT_USB;

	/* wireless */
	if (wireless_state)
		d->charger |= CONNECT_WIRELESS;

	TOUCH_I("%s: write charger_state = 0x%02X\n", __func__, d->charger);
	if (atomic_read(&ts->state.pm) > DEV_PM_RESUME) {
		TOUCH_I("DEV_PM_SUSPEND - Don't try\n");
		return;
	}

	TOUCH_I("CHARGER_STS addr=%x, val=%d",
			ABT_CMD + SPECIAL_CHARGER_INFO, d->charger);
	sw82906_reg_write(dev, ABT_CMD + SPECIAL_CHARGER_INFO,
			&d->charger, sizeof(u32));
}

static void sw82906_lcd_mode(struct device *dev, u32 mode)
{
	struct sw82906_data *d = to_sw82906_data(dev);

	d->prev_lcd_mode = d->lcd_mode;
	d->lcd_mode = mode;
	TOUCH_I("lcd_mode: %d (prev: %d)\n", d->lcd_mode, d->prev_lcd_mode);
}

static int sw82906_check_mode(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;

	if (d->lcd_mode != LCD_MODE_U3) {
		if (d->lcd_mode == LCD_MODE_U2) {
			if (d->prev_lcd_mode == LCD_MODE_U3_PARTIAL) {
				TOUCH_I("U3 Partial -> U2\n");
				ret = 1;
			} else {
				TOUCH_I("U2 mode change\n");
			}
		} else if (d->lcd_mode == LCD_MODE_U3_PARTIAL) {
			switch (d->prev_lcd_mode) {
			case LCD_MODE_U3:
				TOUCH_I("U3 -> U3 Partial\n");
				break;
			case LCD_MODE_U2:
				TOUCH_I("U2 -> U3 Partial\n");
				ret = 1;
				break;
			case LCD_MODE_U0:
				TOUCH_I("U0 -> U3 Partial mode change\n");
				break;
			default:
				TOUCH_E("%s - Not defined mode, lcd_mode:%d\n", __func__, d->lcd_mode);
				break;
			}
		} else if (d->lcd_mode == LCD_MODE_U0) {
			TOUCH_I("U0 mode change\n");
		} else {
			TOUCH_E("%s - Not defined mode, lcd_mode:%d\n", __func__, d->lcd_mode);
		}
	}

	return ret;
}

#if defined(__SUPPORT_NOTIFY_LCD_EVENT_REG)
static void sw82906_lcd_event_read_reg(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	struct touch_core_data *ts = to_touch_core(dev);
	u32 version_addr = CHIP_INFO + tc_version;
	u32 rdata[5] = {0};

	sw82906_read_value(dev, TC_IC_STATUS, &rdata[0]);

	sw82906_read_value(dev, TC_STS, &rdata[1]);

	sw82906_read_value(dev, version_addr, &rdata[2]);

	sw82906_read_value(dev, SPR_CHIP_ID, &rdata[3]);

	TOUCH_I(
			"reg[%x] = 0x%x reg[%x] = 0x%x reg[%x] = 0x%x reg[%x] = 0x%x\n",
			TC_IC_STATUS, rdata[0], TC_STS, rdata[1],
			version_addr, rdata[2],
			SPR_CHIP_ID, rdata[3]);
	TOUCH_I("v%d.%02d\n", (rdata[2] >> 8) & 0xF, rdata[2] & 0xFF);
}
#endif	/* __SUPPORT_NOTIFY_LCD_EVENT_REG */

static int get_batt_temperature(void) {

	struct power_supply *psy;
	union power_supply_propval val = { .intval = 0 };
	int batt_temp = 0;
	int ret = 0;
	int error_temp = 0xFFFF; //default set.

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		TOUCH_E("%s: usb psy doesn't prepared\n", __func__);
		return error_temp;
	}

	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_EXT_TEMP_RAW, &val);
	if (ret) {
		TOUCH_E("%s: Unable to read POWER_SUPPLY_PROP_EXT_TEMP_RAW: %d\n", __func__, ret);
		return error_temp;
	}

	batt_temp = val.intval;
	return batt_temp;
}

/* only B project */
static int get_cam_therm_usr_temperature(void) {

	struct thermal_zone_device *tzd;
	int error_temp = 0xFFFF; //default set.
	int temp, ret = 0;

	tzd = thermal_zone_get_zone_by_name("cam-therm-usr");

	if(!tzd) {
		TOUCH_E("%s: cam-therm-usr doesn't prepared\n", __func__);
		return error_temp;
	}

	ret = thermal_zone_get_temp(tzd, &temp);
	if (ret) {
		TOUCH_E("%s: Unable to read cam-therm-usr: %d\n", __func__, ret);
		return error_temp;
	}

	temp = temp / 100;
	return temp;
}

static int sw82906_temperature_status(struct device *dev, bool force_write_flag)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);

	int ret = 0;
	u32 wdata = 0;
	int batt_temp = 0;
	int cam_therm_usr = 0;
	bool batt_temp_change_flag = 0;

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC\n", __func__);
		return -ENODEV;
	}

	batt_temp = get_batt_temperature() / 10;
	cam_therm_usr = get_cam_therm_usr_temperature();

	if (batt_temp > 100) {
		TOUCH_E("invalid batt_temp");
		return -EINVAL;
	}

	batt_temp_change_flag = (d->thermal_wdata & (1 << 16)) || (batt_temp < 0);

	if (batt_temp_change_flag || force_write_flag) {
		if (batt_temp < 0) {
			wdata = (batt_temp * -1) & 0xFFFF;
			wdata |= 1 << 16;
		} else {
			wdata = batt_temp & 0xFFFF;
		}
		if (d->thermal_noti == 1)
			wdata |= 1 << 31;

		d->thermal_wdata = wdata;
		ret = sw82906_reg_write(dev, ABT_CMD + SPECIAL_TEMPERATURE_INFO,
							&wdata, sizeof(u32));
		TOUCH_I("origin noti:%d, batt:%d, thermal=%s, w_val:0x%x, cam_therm_usr:%d\n",
				atomic_read(&ts->state.temperature), batt_temp,
				d->thermal_noti? "enable":"disable", wdata, cam_therm_usr);
	} else {
		TOUCH_I("batt_temp setting skip. noti:%d, batt:%d, thermal=%s, cam_therm_usr:%d\n",
				atomic_read(&ts->state.temperature), batt_temp,
				d->thermal_noti? "enable":"disable", cam_therm_usr);
	}

	return ret;
}

static int sw82906_usb_status(struct device *dev, u32 mode)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();
	TOUCH_I("TA Type: %d\n", atomic_read(&ts->state.connect));
	sw82906_connect(dev);
	return 0;
}

static int sw82906_wireless_status(struct device *dev, u32 onoff)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();
	TOUCH_I("Wireless charger: 0x%02X\n", atomic_read(&ts->state.wireless));
	sw82906_connect(dev);

	/*if (atomic_read(&ts->state.active_pen)) {
		sw82906_lpwg_mode(dev, DISPLAY_AREA_ID_0);
		sw82906_lpwg_mode(dev, DISPLAY_AREA_ID_1);
	}*/

	return 0;
}

static int sw82906_earjack_status(struct device *dev, u32 onoff)
{
	struct touch_core_data *ts = to_touch_core(dev);

	TOUCH_TRACE();
	TOUCH_I("Earjack Type: 0x%02X\n", atomic_read(&ts->state.earjack));
	return 0;
}

#if defined(__SUPPORT_NOTIFY_DEBUG_OPTION)
static int sw82906_debug_tool(struct device *dev, u32 value)
{
	struct touch_core_data *ts = to_touch_core(dev);

	if (value == DEBUG_TOOL_ENABLE) {
	//	ts->driver->irq_handler = sw82906_sic_abt_irq_handler;
	} else {
		ts->driver->irq_handler = sw82906_irq_handler;
	}

	return 0;
}

static int sw82906_debug_option(struct device *dev, u32 *data)
{
	u32 chg_mask = data[0];
	u32 enable = data[1];

	switch (chg_mask) {
	case DEBUG_OPTION_0:
		TOUCH_I("Debug Option 0 %s\n", enable ? "Enable" : "Disable");
		break;
	case DEBUG_OPTION_1:
		break;
	case DEBUG_OPTION_2:
		TOUCH_I("Debug Info %s\n", enable ? "Enable" : "Disable");
		break;
	case DEBUG_OPTION_3:
		TOUCH_I("Debug Info Depth 10 %s\n",
				enable ? "Enable" : "Disable");
		break;
	case DEBUG_OPTION_4:
		TOUCH_I("TA Simulator mode %s\n",
				enable ? "Enable" : "Disable");
		sw82906_connect(dev);
		break;
	default:
		TOUCH_E("Not supported debug option\n");
		break;
	}

	return 0;
}
#endif

static void sw82906_fb_notify_work_func(struct work_struct *fb_notify_work)
{
	struct sw82906_data *d =
		container_of(to_delayed_work(fb_notify_work),
				struct sw82906_data, fb_notify_work);
	int ret = 0;

	TOUCH_TRACE();

	if (d->lcd_mode == LCD_MODE_U3)
		ret = FB_RESUME;
	else
		ret = FB_SUSPEND;

	touch_notifier_call_chain(NOTIFY_FB, &ret);
}

static int sw82906_rotation(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_I("%s : %d\n", __func__, ts->rotation);

	ret = sw82906_reg_write(dev, ABT_CMD + ROTATION_ANGLE,
							&ts->rotation, sizeof(int));
	if (ret < 0)
		TOUCH_E("failed to write rotation");

	return ret;
}

extern char *grip_area_str[GRIP_AREA_NUM];

static int sw82906_grip_area(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u32 buf = 0;
	int i = 0;

	TOUCH_TRACE();

	if (!ts->have_grip_area)
		return 0;

	for (i = 0; i < GRIP_AREA_NUM; i++) {
		TOUCH_I("%s: %s (%d %d %d %d)", __func__,
			grip_area_str[i],
			ts->grip_area[i].left_top.x,
			ts->grip_area[i].left_top.y,
			ts->grip_area[i].right_bottom.x,
			ts->grip_area[i].right_bottom.y);
	}

	/* Set grip Ignore Area */
	buf = 1;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_ENABLE,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_ENABLE");

	buf = ts->grip_area[IGNORE_LG_PAY].left_top.x;
	buf |= ts->grip_area[IGNORE_LG_PAY].left_top.y << 16;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_START,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_START");

	buf = ts->grip_area[IGNORE_LG_PAY].right_bottom.x;
	buf |= ts->grip_area[IGNORE_LG_PAY].right_bottom.y << 16;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_END,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_END");

	buf = 2;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_ENABLE,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_ENABLE");

	buf = ts->grip_area[IGNORE_SIDE_PANEL].left_top.x;
	buf |= ts->grip_area[IGNORE_SIDE_PANEL].left_top.y << 16;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_START,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_START");

	buf = ts->grip_area[IGNORE_SIDE_PANEL].right_bottom.x;
	buf |= ts->grip_area[IGNORE_SIDE_PANEL].right_bottom.y << 16;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_END,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_END");

	buf = 3;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_ENABLE,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_ENABLE");

	buf = ts->grip_area[IGNORE_QMEMO_PANEL].left_top.x;
	buf |= ts->grip_area[IGNORE_QMEMO_PANEL].left_top.y << 16;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_START,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_START");

	buf = ts->grip_area[IGNORE_QMEMO_PANEL].right_bottom.x;
	buf |= ts->grip_area[IGNORE_QMEMO_PANEL].right_bottom.y << 16;
	ret = sw82906_reg_write(dev, ABT_CMD + GRIP_EXCEPTION_END,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write GRIP_EXCEPTION_END");

	/* Set grip Area */
	if (ts->grip_area[PORTRAIT_A].left_top.x < 0) {
		TOUCH_I("Set default grip area (left_top.x:%d)",
				ts->grip_area[PORTRAIT_A].left_top.x);
		return 0;
	}

	buf = ts->grip_area[PORTRAIT_A].right_bottom.x;  //Portrait A width
	buf |= ts->grip_area[PORTRAIT_B].right_bottom.x << 16;  //Portrait B width

	ret = sw82906_reg_write(dev, ABT_CMD + PORTRAIT_VERTICAL_A_B_WIDTH,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write PORTRAIT_VERTICAL_A_B_WIDTH");

	buf = ts->grip_area[PORTRAIT_C].right_bottom.x;  //Portrait C width
	buf |= (ts->grip_area[PORTRAIT_C].right_bottom.y -
			ts->grip_area[PORTRAIT_C].left_top.y) << 16;  //Portrait C height

	ret = sw82906_reg_write(dev, ABT_CMD + PORTRAIT_VERTICAL_C_WIDTH_HEIGHT,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write PORTRAIT_VERTICAL_C_WIDTH_HEIGHT");

	buf = ts->grip_area[LANDSCAPE_VERTICAL_A].right_bottom.y;  //Landscape vertical A Width
	buf |= ts->grip_area[LANDSCAPE_VERTICAL_B].right_bottom.y << 16; //Landscape vertical B Width

	ret = sw82906_reg_write(dev, ABT_CMD + LANDSCAPE_VERTICAL_A_B_WIDTH,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write LANDSCAPE_VERTICAL_A_B_WIDTH");

	buf = ts->grip_area[LANDSCAPE_VERTICAL_C].right_bottom.y;  //Landscape vertical C width
	buf |= ts->grip_area[LANDSCAPE_VERTICAL_C].right_bottom.x << 16; //Landscape vertical C height

	ret = sw82906_reg_write(dev, ABT_CMD + LANDSCAPE_VERTICAL_C_WIDTH_HEIGH,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write LANDSCAPE_VERTICAL_C_WIDTH_HEIGH");

	buf = ts->grip_area[LANDSCAPE_HORIZONTAL_A].right_bottom.x;  //Landscape horizontal A height
	buf |= ts->grip_area[LANDSCAPE_HORIZONTAL_B].right_bottom.x << 16; //Landscape horizontal B height

	ret = sw82906_reg_write(dev, ABT_CMD + LANDSCAPE_HORIZONTAL_A_B_HEIGHT,
							&buf, sizeof(buf));
	if (ret < 0)
		TOUCH_E("failed to write LANDSCAPE_VERTICAL_A_B_HEIGHT");

	return ret;
}

static int sw82906_notify(struct device *dev, ulong event, void *data)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	/*
	struct project_param *param = &d->p_param;
	*/
	int ret = 0;
	u32 ctrl = 0;
	u32 addr = TC_CMD + tc_driving_ctl;
	u32 rdata = 0;

	TOUCH_TRACE();

	switch (event) {
	case NOTIFY_TOUCH_RESET:
		break;
	case LCD_EVENT_LCD_MODE:
		TOUCH_I("LCD_EVENT_LCD_MODE!\n");
		sw82906_lcd_mode(dev, *(u32 *)data);
		ret = sw82906_check_mode(dev);

		if (ret == 0) {
			queue_delayed_work(ts->wq, &d->fb_notify_work, 0);
		} else {
			ret = 0;
		}
		break;
	case LCD_EVENT_READ_REG:
		TOUCH_I("LCD_EVENT_READ_REG\n");
#if defined(__SUPPORT_NOTIFY_LCD_EVENT_REG)
		sw82906_lcd_event_read_reg(dev);
#endif
		break;
	case NOTIFY_CONNECTION:
		TOUCH_I("NOTIFY_CONNECTION!\n");
		ret = sw82906_usb_status(dev, *(u32 *)data);
		break;
	case NOTIFY_WIRELESS:
		TOUCH_I("NOTIFY_WIRELEES!\n");
		ret = sw82906_wireless_status(dev, *(u32 *)data);
		break;
	case NOTIFY_EARJACK:
		TOUCH_I("NOTIFY_EARJACK!\n");
		ret = sw82906_earjack_status(dev, *(u32 *)data);
		break;
	case NOTIFY_IME_STATE:
		TOUCH_I("NOTIFY_IME_STATE!\n");
		ret = sw82906_reg_write(dev, ABT_CMD +
				SPECIAL_IME_STATUS, (u32 *)data, sizeof(u32));
		break;
	case NOTIFY_CALL_STATE:
		TOUCH_I("NOTIFY_CALL_STATE!\n");
#if defined(__SUPPORT_NOTIFY_CALL)
		ret = sw82906_reg_write(dev, ABT_CMD +
				SPECIAL_CALL_INFO, (u32 *)data, sizeof(u32));
#endif
		break;
	case NOTIFY_DEBUG_TOOL:
		TOUCH_I("NOTIFY_DEBUG_TOOL!\n");
		break;
	case NOTIFY_DEBUG_OPTION:
		TOUCH_I("NOTIFY_DEBUG_OPTION!\n");
		break;
	case NOTIFY_ONHAND_STATE:
		TOUCH_I("NOTIFY_ONHAND_STATE!\n");
		break;
	case NOTIFY_FILM_STATE:
		TOUCH_I("NOTIFY_FILM_STATE!\n");
		ret = sw82906_reg_write(dev, ABT_CMD +
				SPECIAL_SENSITIVE_INFO, (u32 *)data, sizeof(u32));
		break;
	case NOTIFY_ACTIVE_PEN_STATE:
		TOUCH_I("NOTIFY_ACTIVE_PEN_STATE!\n");
		if (atomic_read(&ts->state.dualscreen)) {
			touch_send_uevent(ts, TOUCH_UEVENT_DS_UPDATE_STATE);
			atomic_set(&ts->state.uevent, UEVENT_IDLE);
		}
		/*
		 * delete tc stop for change driving ctrl in U3 status
		 *
		ctrl = 0x04;
		sw82906_reg_write(dev, addr, &ctrl, sizeof(ctrl));
		touch_msleep(param->sys_tc_stop_delay);
		TOUCH_I("TC STOP first\n");
		*/
		switch (d->driving_mode) {
			case LCD_MODE_U0:
				if (*(int *)data) {
					ctrl = 0x3;
				} else {
					ctrl = 0x1;
				}
				break;

			case LCD_MODE_U3:
				if (*(int *)data) {
					ctrl = 0x303;
				} else {
					ctrl = 0x8303;
				}
				break;
		}
		if (ctrl < 0) {
			TOUCH_I("invalid mode change, mode : %d", d->driving_mode);
			ret = -EINVAL;
		}
		sw82906_reg_write(dev, addr,
				&ctrl, sizeof(ctrl));
		TOUCH_I("sw82906_pen state write(0x%x) = (0x%x)\n",
				addr,  ctrl);
		/* tc_status check */
		sw82906_reg_read(dev, TC_IC_STATUS, (u8 *)&rdata, sizeof(u32));
		TOUCH_I("read ic_status(%x) = %x\n", TC_IC_STATUS, rdata);
		break;
	case NOTIFY_DUALSCREEN_STATE:
		TOUCH_I("NOTIFY_DUALSCREEN_STATE : %d\n", *(int *)data);
		atomic_set(&ts->state.dualscreen, *(int *)data);
		break;
	case NOTIFY_TEMPERATURE:
		TOUCH_I("NOTIFY_TEMPERATURE!\n");
		sw82906_temperature_status(dev, false);
		break;
	default:
		TOUCH_E("%lu is not supported\n", event);
		break;
	}

	return ret;
}

static int sw82906_init_pm(struct device *dev)
{
#if defined(CONFIG_DRM_MSM) && defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
	struct touch_core_data *ts = to_touch_core(dev);
	TOUCH_I("%s: drm_notif change\n", __func__);
	ts->drm_notif.notifier_call = sw82906_drm_notifier_callback;
#endif
#elif defined(CONFIG_FB)
	struct touch_core_data *ts = to_touch_core(dev);
	TOUCH_I("%s: fb_notif change\n", __func__);
	ts->fb_notif.notifier_call = sw82906_fb_notifier_callback;
#endif

	return 0;
}

static int sw82906_init_works(struct sw82906_data *d)
{
	d->wq_log = create_singlethread_workqueue("touch_wq_log");
	if (!d->wq_log) {
		TOUCH_E("failed to create workqueue log\n");
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&d->fb_notify_work, sw82906_fb_notify_work_func);

	return 0;
}

static void sw82906_init_locks(struct sw82906_data *d)
{
	mutex_init(&d->io_lock);
}

static int sw82906_probe(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = NULL;
	int boot_mode = TOUCH_NORMAL_BOOT;

	TOUCH_TRACE();

	TOUCH_I("sw82906 probe\n");

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d) {
		TOUCH_E("failed to allocate synaptics data\n");
		return -ENOMEM;
	}


	d->dev = dev;
	touch_set_device(ts, d);

	project_param_set(dev);

	touch_gpio_init(ts->reset_pin, "touch_reset");
	touch_gpio_direction_output(ts->reset_pin, 0);

	touch_gpio_init(ts->int_pin, "touch_int");
	touch_gpio_direction_input(ts->int_pin);

	touch_gpio_init(ts->maker_id_pin, "touch_make_id");
	touch_gpio_direction_input(ts->maker_id_pin);

	touch_power_init(dev);
	touch_bus_init(dev, MAX_BUF_SIZE);

	sw82906_init_works(d);
	sw82906_init_locks(d);

	boot_mode = touch_check_boot_mode(dev);
	if (boot_mode == TOUCH_CHARGER_MODE
			|| boot_mode == TOUCH_LAF_MODE
			|| boot_mode == TOUCH_RECOVERY_MODE) {
		TOUCH_I("%s: boot_mode = %d\n", __func__, boot_mode);
		touch_gpio_init(ts->reset_pin, "touch_reset");
		touch_gpio_direction_output(ts->reset_pin, 1);
		touch_msleep(ts->caps.hw_reset_delay);
		sw82906_init(dev);
		/* Deep Sleep */
		sw82906_sleep_ctrl(dev, IC_DEEP_SLEEP);
		return 0;
	}

	if ((boot_mode >= TOUCH_MINIOS_AAT)
			&& (boot_mode <= TOUCH_MINIOS_MFTS_DS_FLAT)) {
			atomic_set(&ts->state.film_mode, 0);
			TOUCH_I("%s: protection film mode  = %d\n", __func__,
				atomic_read(&ts->state.film_mode));
	}

	sw82906_init_knock_on_info(dev);
	sw82906_init_swipe_info(dev);
	sw82906_init_quick_tool_abs_info(dev);
	sw82906_init_ai_pick_double_tap_info(dev);
	sw82906_init_udf_longpress_info(dev);
	sw82906_init_fake_off_onetap_info(dev);
	sw82906_init_show_aod_onetap_info(dev);
	sw82906_init_display_area_activation_info(dev);

	pm_qos_add_request(&d->pm_qos_req, PM_QOS_CPU_DMA_LATENCY,
			PM_QOS_DEFAULT_VALUE);

#ifdef CONFIG_LGE_TOUCH_PEN
	ts->aes_mode = 2;
	ts->pdata.uevent_handled = true;
	ts->pred_filter = 0;
#endif
	d->lcd_mode = LCD_MODE_U3;
	d->lpwg_failreason_ctrl = LPWG_FAILREASON_ENABLE;
	d->split_display_area_moving = false;


#if defined(CONFIG_LGE_TOUCH_COMMON_CONTROL)
	cmds_adder(dev, (char *)ts->touch_ic_name, sw82906_cmd_lists);
#endif
	return 0;
}

static int sw82906_remove(struct device *dev)
{
	TOUCH_TRACE();

	return 0;
}

static int sw82906_shutdown(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);

	TOUCH_TRACE();

	pm_qos_remove_request(&d->pm_qos_req);

	return 0;
}

enum {
	BIN_CFG_OFFSET_POS = 0xE0,
	BIN_VER_OFFSET_POS = 0xE8,
	BIN_PID_OFFSET_POS = 0xF0,
};

static int sw82906_fw_compare(struct device *dev, const struct firmware *fw)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	u32 bin_ver_offset = *((u32 *)&fw->data[BIN_VER_OFFSET_POS]);
	u32 bin_pid_offset = *((u32 *)&fw->data[BIN_PID_OFFSET_POS]);
	struct sw82906_version *device = &d->ic_info.version;
	struct sw82906_version_bin *binary = NULL;
	char pid[12] = {0};
	int update = UPGRADE_NO_NEED;
	int flash_fw_size = d->p_param.flash_fw_size;
	u32 bootmode = 0;

	TOUCH_TRACE();

	if ((bin_ver_offset > flash_fw_size) ||
			(bin_pid_offset > flash_fw_size)) {
		TOUCH_I("%s : invalid offset\n", __func__);
		return -EINVAL;
	}

	binary = (struct sw82906_version_bin *)&fw->data[bin_ver_offset];

	memcpy(pid, &fw->data[bin_pid_offset], 8);
	memcpy(&d->ic_info.img_version, &fw->data[bin_ver_offset], sizeof(struct sw82906_version_bin));

	sw82906_ic_boot_check(dev, &bootmode);

	if (!bootmode) {
		TOUCH_E("%s, FW Boot Fail, need to force FW upgrade\n", __func__);
		ts->force_fwup = 1;
	}

	if (ts->force_fwup)
		update = UPGRADE_FORCE;
	else if ((binary->major != device->major) || (binary->minor != device->minor))
		update = UPGRADE_NEED;

	TOUCH_I("bin-ver: %d.%02d (%s), dev-ver: %d.%02d -> update: %d, force_fwup: %d\n",
			binary->major, binary->minor, pid, device->major, device->minor, update, ts->force_fwup);

	return update;
}

enum {
	EQ_COND = 0,
	NOT_COND,
};

static int sw82906_condition_wait(struct device *dev,
		u16 addr, u32 *value, u32 expect,
		u32 mask, u32 delay, u32 retry, int not_cond)
{
	u32 data = 0;
	int match = 0;
	int ret = 0;

	do {
		touch_msleep(delay);
		ret = sw82906_read_value(dev, addr, &data);
		if (ret >= 0) {
			match = (not_cond == NOT_COND) ? !!((data & mask) != expect) : !!((data & mask) == expect);

			if (match) {
				if (value)
					*value = data;

				TOUCH_I(
						"%d, addr[%04x] data[%08x], mask[%08x], expect[%s%08x]\n",
						retry, addr, data, mask,
						(not_cond == NOT_COND) ? "not " : "",
						expect);
				return 0;
			}
		}
	} while (--retry);

	if (value)
		*value = data;

	TOUCH_I("%s addr[%04Xh], expect[%s%08Xh], mask[%08Xh], data[%08Xh]\n",
			__func__, addr,
			(not_cond == NOT_COND) ? "not " : "",
			expect, mask, data);

	return -EPERM;
}

#if defined(__SW82906_SUPPORT_CFG)
static int sw82906_fw_verify_cfg(char *buf)
{
	struct cfg_head *head = (struct cfg_head *)buf;

	if (head->chip_id != CFG_CHIP_ID) {
		TOUCH_E("fw verify: invalid chip ID, %dh\n", head->chip_id);
		return -EFAULT;
	}

#if	(CFG_C_SIZE != 0)
	if (head->c_size.b.common_size != CFG_C_SIZE) {
		TOUCH_E("fw verify: invalid c_cfg size, %04Xh\n", head->c_size.b.common_size);
		return -EFAULT;
	}
#endif

	if (head->c_size.b.specific_size != CFG_S_SIZE) {
		TOUCH_E("fw verify: invalid s_cfg size, %04Xh\n", head->c_size.b.specific_size);
		return -EFAULT;
	}

	TOUCH_D(FW_UPGRADE, "fw verify: magic_code  : %08Xh\n", head->magic_code);
	TOUCH_D(FW_UPGRADE, "fw verify: chip ID     : %d\n", head->chip_id);
	TOUCH_D(FW_UPGRADE, "fw verify: c_cfg size  : %04X\n", head->c_size.b.common_size);
	TOUCH_D(FW_UPGRADE, "fw verify: s_cfg size  : %04X\n", head->c_size.b.specific_size);

	return 0;
}

static int sw82906_fw_verify_s_cfg(char *buf, int index)
{
	struct cfg_head *head = (struct cfg_head *)buf;
	struct s_cfg_head *s_head = &head->s_cfg_head;
	int chip_rev = s_head->info_1.b.chip_rev;

	if (chip_rev > 10) {
		TOUCH_E("fw verify: invalid s_cfg rev, %d\n", chip_rev);
		return -EFAULT;
	}

	TOUCH_D(FW_UPGRADE, "fw verify: s-chip_rev  : %d\n", s_head->info_1.b.chip_rev);
	TOUCH_D(FW_UPGRADE, "fw verify: s-model_id  : %d\n", s_head->info_1.b.model_id);
	TOUCH_D(FW_UPGRADE, "fw verify: s-lcm_id    : %d\n", s_head->info_1.b.lcm_id);
	TOUCH_D(FW_UPGRADE, "fw verify: s-fpc_id    : %d\n", s_head->info_1.b.fpc_id);
	TOUCH_D(FW_UPGRADE, "fw verify: s-lot_id    : %d\n", s_head->info_2.b.lot_id);

	return 0;
}
#endif	/* __SW82906_SUPPORT_CFG */

static int sw82906_fw_binary_verify(struct device *dev, u8 *fw_buf, int fw_size)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int flash_fw_size = d->p_param.flash_fw_size;
	u32 fw_code_crc = *(u32 *)&fw_buf[flash_fw_size - 4];
	u32 fw_code_size = *(u32 *)&fw_buf[flash_fw_size - 8];
#if defined(__SW82906_SUPPORT_CFG)
	struct cfg_head *head = NULL;
	char *cfg_base = NULL;
	char *s_cfg_base = NULL;
	u32 cfg_offset = 0;
	u32 cfg_pos = 0;
	int s_cfg_cnt = 0;
	int i;
	int ret = 0;
#endif	/* __SW82906_SUPPORT_CFG */

	TOUCH_TRACE();

	if (fw_size < flash_fw_size) {
		TOUCH_E("fw verify: too small img size(%Xh), must be >= flash_fw_size(%Xh)\n",
				fw_size, flash_fw_size);
		return E_FW_CODE_SIZE_ERR;
	}

	TOUCH_I("fw verify: code size %Xh, code crc %Xh\n",
			fw_code_size, fw_code_crc);

	if (fw_code_size > flash_fw_size) {
		TOUCH_E("fw verify: invalid code_size(%Xh), must be <= flash_fw_size(%Xh)\n",
				fw_code_size, flash_fw_size);
		return E_FW_CODE_SIZE_ERR;
	}

	if (fw_size == flash_fw_size) {
		return E_FW_CODE_ONLY_VALID;
	}

#if defined(__SW82906_SUPPORT_CFG)
	cfg_offset = *(u32 *)&fw_buf[BIN_CFG_OFFSET_POS];
	cfg_pos = *(u32 *)&fw_buf[cfg_offset];
	TOUCH_I("fw verify: cfg pos %Xh, cfg offset %xh\n", cfg_pos, cfg_offset);
	if (cfg_pos >= fw_size) {
		TOUCH_E("fw verify: invalid cfg_pos(%Xh), must be < img size(%Xh)\n",
				cfg_pos, fw_size);
		return E_FW_CODE_CFG_ERR;
	}
	if (cfg_pos >= FLASH_SIZE) {
		TOUCH_E("fw verify: invalid cfg_pos(%Xh), must be < SIZEOF_FLASH\n",
				cfg_pos);
		return E_FW_CODE_CFG_ERR;
	}

	cfg_base = (char *)&fw_buf[cfg_pos];

	head = (struct cfg_head *)cfg_base;
	if (head->magic_code != CFG_MAGIC_CODE) {
		TOUCH_E("fw verify: unknown data in cfg\n");
		return E_FW_CODE_ONLY_VALID;
	}

	TOUCH_I("fw verify: cfg detected\n");
	ret = sw82906_fw_verify_cfg((char *)head);
	if (ret < 0) {
		TOUCH_E("fw verify: invalid cfg\n");
		return E_FW_CODE_ONLY_VALID;
	}

	s_cfg_base = cfg_base + head->c_size.b.common_size;
	s_cfg_cnt = ((fw_size - flash_fw_size) - head->c_size.b.common_size)>>CHIP_POW_S_CONF;
	for (i = 0; i < s_cfg_cnt; i++) {
		ret = sw82906_fw_verify_s_cfg((char *)s_cfg_base, i);
		if (ret < 0) {
			TOUCH_E("fw verify: invalid s_cfg\n");
			return E_FW_CODE_CFG_ERR;
		}
		s_cfg_base += head->c_size.b.specific_size;
	}

	return E_FW_CODE_AND_CFG_VALID;
#else	/* !__SW82906_SUPPORT_CFG */
	return E_FW_CODE_SIZE_ERR;
#endif	/* __SW82906_SUPPORT_CFG */
}

/*
#define _reg_print(_reg, _element)	\
	TOUCH_I("# 0x%04X [%s]\n", _reg->_element, #_element)
static void sw82906_prt_reg_map(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	struct sw82906_reg_info *info = &d->reg_info;

	_reg_print(info, r_info_ptr_addr);
	_reg_print(info, r_tc_cmd_addr);
	_reg_print(info, r_test_cmd_addr);
	_reg_print(info, r_abt_cmd_addr);
	_reg_print(info, r_cfg_s_sram_oft);
	_reg_print(info, r_ic_status_addr);
	_reg_print(info, r_tc_status_addr);
	_reg_print(info, r_abt_report_addr);
	_reg_print(info, r_chip_info_addr);
	_reg_print(info, r_reg_info_addr);
	_reg_print(info, r_pt_info_addr);
	_reg_print(info, r_tc_sts_addr);
	_reg_print(info, r_abt_sts_addr);
	_reg_print(info, r_tune_code_addr);
	_reg_print(info, r_sys_dbg_buf1_sram_oft);
	_reg_print(info, r_sys_dbg_buf2_sram_oft);
	_reg_print(info, r_abt_buf_sram_oft);
	_reg_print(info, r_addr);
}
*/
/*
void sw82906_default_reg_map(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);

	d->reg_info.r_info_ptr_addr		= INFO_PTR_ADDR;
	d->reg_info.r_tc_cmd_addr		= TC_CMD;
	d->reg_info.r_rsvd1_addr		= RSVD1;
	d->reg_info.r_test_cmd_addr		= TEST_CMD;
	d->reg_info.r_abt_cmd_addr		= ABT_CMD;
	d->reg_info.r_rsvd2_addr		= RSVD2;
	d->reg_info.r_cfg_s_sram_oft		= CFG_S_SRAM_OFT;
	d->reg_info.r_ic_status_addr		= IC_STATUS;
	d->reg_info.r_tc_status_addr		= TC_STATUS;
	d->reg_info.r_abt_report_addr		= ABT_REPORT;
	d->reg_info.r_rsvd3_addr		= RSVD3;
	d->reg_info.r_chip_info_addr		= CHIP_INFO;
	d->reg_info.r_reg_info_addr		= REG_INFO;
	d->reg_info.r_pt_info_addr		= PT_INFO;
	d->reg_info.r_tc_sts_addr		= TC_STS;
	d->reg_info.r_abt_sts_addr		= ABT_STS;
	d->reg_info.r_tune_code_addr		= TUNE_CODE;
	d->reg_info.r_sys_dbg_buf1_sram_oft	= SYS_DBG_BUF1_SRAM_OFT;
	d->reg_info.r_sys_dbg_buf2_sram_oft	= SYS_DBG_BUF2_SRAM_OFT;
	d->reg_info.r_abt_buf_sram_oft		= ABT_BUF_SRAM_OFT;
	d->reg_info.r_addr			= R_ADDR;

	TOUCH_I("================================================\n");
	TOUCH_I("sw82906 default register map loaded!!\n");
	TOUCH_I("================================================\n");
	sw82906_prt_reg_map(dev);
}
*/

int sw82906_chip_info_load(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);

    u32 read_val = 0;

	if (sw82906_reg_read(dev, ENGINE_BUF_ADDR,
			(u8 *)&read_val, sizeof(u32)) < 0) {
		TOUCH_E("ENGINE_BUF_ADDR read error\n");
		goto error;
	}
	d->reg_info.engine_buf_addr = ((read_val >> 16) & 0xFFFF);
	TOUCH_I("engine buf addr read : %8.8X\n", d->reg_info.engine_buf_addr);

	if (sw82906_reg_read(dev, SYS_BUF_ADDR,
			(u8 *)&read_val, sizeof(u32)) < 0) {
		TOUCH_E("SYS_BUF_ADDR read error\n");
		goto error;
	}
	d->reg_info.sys_buf_addr = ((read_val >> 16) & 0xFFFF);
	TOUCH_I("sys buf addr read : %8.8X\n", d->reg_info.sys_buf_addr);
/*
	if (sw82906_reg_read(dev, INFO_PTR_ADDR,
				(u8 *)&info_ptr_val, sizeof(u32)) < 0) {
		TOUCH_E("Info ptr addr read error\n");
		goto error;
	}
	TOUCH_I("info ptr addr read : %8.8X\n", info_ptr_val);

	if (sw82906_reg_read(dev, TC_STS,
				(u32 *)&tc_status_val, sizeof(tc_status_val)) < 0) {
		TOUCH_E("tc status addr read error\n");
		goto error;
	}
	TOUCH_I("tc status read : %8.8X\n", tc_status_val);

	if (info_ptr_val == 0 || ((info_ptr_val >> 24) != 0)) {

		TOUCH_E("info_ptr_addr invalid!\n");
		goto error;
	}

	chip_info_addr = (info_ptr_val & 0xFFF);
	reg_info_addr  = ((info_ptr_val >> 12) & 0xFFF);

	TOUCH_I("========== Info ADDR ==========\n");
	TOUCH_I("chip_info_addr        : %4.4X\n", chip_info_addr);
	TOUCH_I("reg_info_addr         : %4.4X\n", reg_info_addr);
	TOUCH_I("========== Info ADDR ==========\n");

	if (!((tc_status_val >> 27) & 0x1))
		TOUCH_E("Model ID is not loaded : %x\n", tc_status_val);
	else
		TOUCH_I("Model ID is loaded\n");

	if ((tc_status_val >> 28) & 0x1)
		TOUCH_E("Production Test info checksum error : %x\n",
				tc_status_val);
	else
		TOUCH_I("Production Test info checksum is ok\n");

	if (sw82906_reg_read(dev, chip_info_addr, (u8 *)&d->chip_info,
				sizeof(struct sw82906_chip_info)) < 0) {
		TOUCH_E("Chip info Read Error\n");
		goto error;
	}

	if (sw82906_reg_read(dev, reg_info_addr, (u8 *)&d->reg_info,
				sizeof(struct sw82906_reg_info)) < 0) {
		TOUCH_E("Reg info Read Error\n");
		goto error;
	}
*/
	//	sw82906_prt_reg_map(dev);
	return 0;

error:
	return -1;
}

static int sw82906_flash_erase_check(struct device *dev)
{
	u32 erase_val = 0xFFFFFFFF;
	u32 start_offset = 0;
	u32 end_offset = (0x1FFFC)/4;
	u32 flash_read_val = 0;

	TOUCH_TRACE();

	sw82906_reg_write(dev, spr_flash_offset, &start_offset, sizeof(start_offset));
	TOUCH_I("flash read offset(start_offset) %x \n", start_offset);	//for debug

	sw82906_reg_read(dev, flash_access_addr, &flash_read_val, sizeof(flash_read_val));
	TOUCH_I("flash read val(start_offset) %x \n", flash_read_val);		//for debug

	if(erase_val != flash_read_val)
	{
		TOUCH_I("Start Area Erase Fail : %x  \n", flash_read_val);
		return -1;
	}

	sw82906_reg_write(dev, spr_flash_offset, &end_offset, sizeof(end_offset));
	TOUCH_I("flash read offset(end_offset) %x \n", end_offset);	//for debug

	sw82906_reg_read(dev, flash_access_addr, &flash_read_val, sizeof(flash_read_val));
	TOUCH_I("flash read val(end_offset) %x \n", flash_read_val);		//for debug

	if(erase_val != flash_read_val)
	{
		TOUCH_I("End Area Erase Fail : %x  \n", flash_read_val);
		return -1;
	}

	return 0;
}

static int __used sw82906_fw_flash_crc(struct device *dev, u32 *crc_val)
{
	u32 gdma_saddr = GDMA_SADDR;
	u32 gdma_ctl = GDMA_CTL;
	u32 gdma_start = GDMA_START;
	u32 gdma_crc_result = GDMA_CRC_RESULT;
	u32 gdma_crc_pass = GDMA_CRC_PASS;
	u32 data;
	u32 ctrl_data;
	int ret = 0;

	TOUCH_TRACE();

	ret = sw82906_write_value(dev, gdma_saddr, 0);
	if (ret < 0) {
		TOUCH_E("crc GDMA_SADDR(%04Xh) set zero failed, %d\n",
				gdma_saddr, ret);
		goto out;
	}

	ctrl_data = (FLASH_SIZE>>2) - 1;
	ctrl_data |= GDMA_CTL_GDMA_EN | GDMA_CTL_READONLY_EN;
	ret = sw82906_write_value(dev, gdma_ctl, ctrl_data);
	if (ret < 0) {
		TOUCH_E("crc GDMA_CTL(%04Xh) write %08Xh failed, %d\n",
				gdma_ctl, ctrl_data, ret);
		goto out;
	}
	touch_msleep(10);

	ret = sw82906_write_value(dev, gdma_start, 1);
	if (ret < 0) {
		TOUCH_E("crc GDMA_START(%04Xh) on failed, %d\n",
				gdma_start, ret);
		goto out;
	}
	touch_msleep(10);

	ret = sw82906_read_value(dev, gdma_crc_result, &data);
	if (ret < 0) {
		TOUCH_E("read crc_result(%04Xh) failed, %d\n",
				gdma_crc_result, ret);
		goto out;
	}

	TOUCH_I("crc_result(%04Xh) %08Xh\n", gdma_crc_result, data);

	if (crc_val != NULL)
		*crc_val = data;

	ret = sw82906_read_value(dev, gdma_crc_pass, &data);
	if (ret < 0) {
		TOUCH_E("read crc_pass(%04Xh) failed, %d\n",
				gdma_crc_pass, ret);
		goto out;
	}

	TOUCH_I("crc_pass(%04Xh) done, %08Xh\n", gdma_crc_pass, data);

out:
	touch_msleep(100);

	return ret;
}

static int __used sw82906_fw_flash_mass_erase(struct device *dev)
{
	u32 fc_addr = FC_ADDR;
	u32 fc_ctrl = FC_CTRL;
	u32 fc_stat = FC_STAT;
	u32 spi_flash_status = SPI_FLASH_STATUS;
	int fc_err = 0;
	int busy_time = FC_ERASE_WAIT_TIME;
	int busy_cnt = (FLASH_PAGE_SIZE<<1)/busy_time;
	u32 chk_resp, data;
	int ret = 0;
	u32 read_reg = 0;
	u32 erase_crc_val = 0;

	TOUCH_TRACE();
	TOUCH_I("flash mass erase start\n");

	ret = sw82906_write_value(dev, fc_addr, 0);
	if (ret < 0) {
		fc_err = 1;
		goto out;
	}

	ret = sw82906_write_value(dev, fc_ctrl, FC_CTRL_MASS_ERASE);
	if (ret < 0) {
		fc_err = 2;
		goto out;
	}

	sw82906_reg_read(dev, fc_ctrl, &read_reg, sizeof(read_reg));
	TOUCH_I("fc_ctrl erase command read_val : %x\n", read_reg);

	ret = sw82906_write_value(dev, fc_stat, 1);
	if (ret < 0) {
		fc_err = 3;
		goto out;
	}

	chk_resp = 1;
	ret = sw82906_condition_wait(dev, spi_flash_status, &data,
			chk_resp, ~0, busy_time, busy_cnt, NOT_COND);
	if (ret < 0) {
		fc_err = 4;
		TOUCH_E("flash erase wait(%Xh) failed, %Xh\n", chk_resp, data);
		goto out;
	}

	TOUCH_I("%s - crc check start!\n", __func__);

	sw82906_reg_read(dev, SYS_RST_CTL, &read_reg, sizeof(read_reg));
	TOUCH_I("1st cm3 status check read_val : %x\n", read_reg);

	//Flash Area check(first,last)
	ret = sw82906_flash_erase_check(dev);
	if (ret < 0) {
		fc_err = 5;
		TOUCH_E("sw82906_flash_erase_check(fc_err : %d) error!!!\n", fc_err);
		goto out;
	}

	sw82906_reg_read(dev, SYS_RST_CTL, &read_reg, sizeof(read_reg));
	TOUCH_I("2nd cm3 status check read_val : %x\n", read_reg);

	//Erase Flash CRC Check
	ret = sw82906_fw_flash_crc(dev, &erase_crc_val);
	if (ret < 0) {
		fc_err = 6;
		TOUCH_E("sw82906_flash_crc(fc_err : %d) error!!!\n", fc_err);
		goto out;
	}

	if (erase_crc_val != CRC_FIXED_VALUE) {
		fc_err = 7;
		TOUCH_E("erase flash crc error %08Xh != %08Xh, %d\n",
				CRC_FIXED_VALUE, erase_crc_val, ret);
		goto out;
	}

	TOUCH_I("%s - crc check end!\n", __func__);

out:
	touch_msleep(10);

	sw82906_write_value(dev, fc_ctrl, 0);

	if (fc_err) {
		TOUCH_E("flash mass erase error, %d, %d\n",
				fc_err, ret);
		return -fc_err;
	} else {
		TOUCH_I("flash mass erase done\n");
	}

	return ret;
}

static int __used sw82906_fw_flash_page_erase(struct device *dev)
{
	u32 fc_addr = FC_ADDR;
	u32 fc_ctrl = FC_CTRL;
	u32 fc_stat = FC_STAT;
	u32 spi_flash_status = SPI_FLASH_STATUS;
	int addr = 0;
	int fc_err = 0;
	int i;
	int busy_time = FC_ERASE_WAIT_TIME;
	int busy_cnt = FC_ERASE_WAIT_CNT;
	u32 chk_resp, data;
	int ret = 0;

	for (i = 0; i < (FLASH_SIZE/FLASH_PAGE_SIZE); i++) {
		ret = sw82906_write_value(dev, fc_addr, addr);
		if (ret < 0) {
			fc_err = 1;
			break;
		}

		ret = sw82906_write_value(dev, fc_ctrl, FC_CTRL_PAGE_ERASE);
		if (ret < 0) {
			fc_err = 2;
			break;
		}

		ret = sw82906_write_value(dev, fc_stat, 1);
		if (ret < 0) {
			fc_err = 3;
			break;
		}

		chk_resp = 1;
		ret = sw82906_condition_wait(dev, spi_flash_status, &data,
				chk_resp, ~0, busy_time, busy_cnt, NOT_COND);
		if (ret < 0) {
			fc_err = 4;
			TOUCH_E("flash page erase wait(%Xh) failed, %Xh\n",
					chk_resp, data);
			break;
		}

		addr += FLASH_PAGE_OFFSET;
	}

	touch_msleep(10);

	sw82906_write_value(dev, fc_addr, 0);
	sw82906_write_value(dev, fc_ctrl, 0);

	if (fc_err) {
		TOUCH_E("flash page erase error failed on %Xh, %d, %d\n",
				fc_addr, fc_err, ret);
	} else {
		TOUCH_I("flash page erase done\n");
	}

	return ret;
}

static int __used sw82906_fw_flash_write(struct device *dev, int addr, u8 *dn_buf, int dn_size)
{
	u32 fc_offset = serial_data_offset;
	u32 fc_code_access = data_access_addr;
	int ret = 0;

	if (!dn_size)
		return 0;

	ret = sw82906_write_value(dev, fc_offset, addr);
	if (ret < 0) {
		TOUCH_E("flash write addr failed, %Xh(%X), %d\n",
				addr, dn_size, ret);
		goto out;
	}

	ret = sw82906_reg_write(dev, fc_code_access, dn_buf, dn_size);
	if (ret < 0) {
		TOUCH_E("flash write data failed, %Xh(%X), %d\n",
				addr, dn_size, ret);
		goto out;
	}

out:
	touch_msleep(5);

	return ret;
}

#define LOG_SZ	64

static int sw82906_fw_rst_ctl(struct device *dev, int val, const char *str)
{
	u32 rst_ctl = spr_rst_ctl;
	char log[LOG_SZ] = { 0, };
	char *name = NULL;
	int ret = 0;

	TOUCH_TRACE();

	switch (val) {
		case 2:
			name = "system hold";
			break;
		case 1:
			name = "release cm3";
			break;
		default:
			name = "system release";
			break;
	}

	if (str == NULL)
		touch_snprintf(log, LOG_SZ, "%s", name);
	else
		touch_snprintf(log, LOG_SZ, "%s for %s", name, str);

	ret = sw82906_write_value(dev, rst_ctl, val);
	if (ret < 0) {
		TOUCH_E("spr_rst_ctl(%d) - %s failed, %d\n", val, log, ret);
		goto out;
	}

	TOUCH_D(TRACE, "spr_rst_ctl(%d) - %s done\n", val, log);

out:
	return ret;
}

static int sw82906_fw_upgrade_code(struct device *dev, u8 *dn_buf, int dn_size)
{
//	struct sw82906_data *d = to_sw82906_data(dev);
//	struct project_param *param = &d->p_param;
	u32 fc_ctrl = FC_CTRL;
	int ret = 0;
	u32 reg_buf = 0;
	u32 busycheck_cnt = 0;
	u32 write_cnt = 0;
	u32 remain_size = 0;
	u8 config = 0;
	int k = 0;

	TOUCH_TRACE();

#if 0	/* not used in SW82906 */
	if (param->wa_trim_wr) {
		sw82906_read_value(dev, SYS_OSC_CTL, &reg_buf);
		reg_buf &= ~0x7F;
		reg_buf |= 0x5A;
		sw82906_write_value(dev, SYS_OSC_CTL, reg_buf);
	}
#endif

//	sw82906_write_value(dev, FLASH_TPROG, 560);

	sw82906_write_value(dev, fc_ctrl, FC_CTRL_WR_EN);

	write_cnt = dn_size / BDMA_TRANS_SIZE;
	remain_size = dn_size % BDMA_TRANS_SIZE;

#if defined(__SW82906_SUPPORT_CFG)
	if(dn_size == FLASH_CONF_SIZE && dn_buf[0] == 0xCA && dn_buf[1] == 0xCA && dn_buf[2] == 0xCA && dn_buf[3] == 0xCA)
		config = 1;
#endif

	do {
		touch_msleep(10);
		sw82906_read_value(dev, BDMA_STS, &reg_buf);
		busycheck_cnt++;
		if(busycheck_cnt > 2000)
		{
			TOUCH_E("Busy count of BDMA is over.. Num:%d, Count:%d\n",
					k, busycheck_cnt);
			return 0;
		}
	} while ((reg_buf & BDMA_STS_TR_BUSY));


	busycheck_cnt = 0;
	for (k = 0; k <= write_cnt; k++)
	{
		busycheck_cnt = 0;

		if(k == write_cnt){
			if(remain_size == 0)
				break;

			sw82906_fw_flash_write(dev, 0, &dn_buf[k*BDMA_TRANS_SIZE], remain_size);
			reg_buf = remain_size / sizeof(u32);
		}else{
			sw82906_fw_flash_write(dev, 0, &dn_buf[k*BDMA_TRANS_SIZE], BDMA_TRANS_SIZE);
			reg_buf = BDMA_TRANS_SIZE / sizeof(u32);
		}
		reg_buf |= BDMA_CTL_BDMA_EN;

		sw82906_write_value(dev, BDMA_SADDR, DATASRAM_ADDR);
		sw82906_write_value(dev, BDMA_CTL, reg_buf);

		sw82906_write_value(dev, BDMA_CAL_OP, BDMA_CAL_OP_CTRL);

		if(config){
			sw82906_write_value(dev, BDMA_DADDR, 0x1FC00);
		} else {
			sw82906_write_value(dev, BDMA_DADDR, (BDMA_TRANS_SIZE * k));
		}
		sw82906_write_value(dev, FC_CTRL, 0x4);
		sw82906_write_value(dev, BDMA_START, 1);

		do {
			touch_msleep(10);
			sw82906_read_value(dev, BDMA_STS, &reg_buf);
			busycheck_cnt++;
			if(busycheck_cnt > 2000)
			{
				TOUCH_E("Busy count of BDMA is over.. Num:%d, Count:%d\n",
						k, busycheck_cnt);
				return 0;
			}
		} while ((reg_buf & BDMA_STS_TR_BUSY));
		TOUCH_I("BDMA is not busy.. Num:%d, Count:%d\n", k, busycheck_cnt);
	}

	sw82906_write_value(dev, fc_ctrl, 0);

	return ret;
}

#if defined(__SW82906_SUPPORT_CFG)
int specific_header_verify(unsigned char *header, int i)
{
	CFG_S_HEADER_CONTROL_TypeDef *head = (CFG_S_HEADER_CONTROL_TypeDef *)header;

	if (head->cfg_specific_info1.b.chip_rev <= 0
			&& head->cfg_specific_info1.b.chip_rev > 10) {
		TOUCH_I("Invalid Chip revision id. %8.8X\n", head->cfg_specific_info1.b.chip_rev);
		return -2;
	}

	TOUCH_I("====================== SPECIFIC #%d ===========================\n", i+1);
	TOUCH_I("chip_rev				: %d\n", head->cfg_specific_info1.b.chip_rev);
	TOUCH_I("fpcb_id				: %d\n", head->cfg_specific_info2.b.fpcb_id);
	TOUCH_I("lcm_id				: %d\n", head->cfg_specific_info2.b.lcm_id);
	TOUCH_I("model_id				: %d\n", head->cfg_specific_info1.b.model_id);
	TOUCH_I("lot_id				: %d\n", head->cfg_specific_info2.b.lot_id);
	TOUCH_I("===============================================================\n");

	return 1;
}

static int sw82906_fw_upgrade_conf(struct device *dev, u8 *pFirm, u32 nSize)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	CFG_S_HEADER_CONTROL_TypeDef *head;
	u32 cfg_size;
	int ret = 0;
	int flash_fw_size = d->p_param.flash_fw_size;

	head = (CFG_S_HEADER_CONTROL_TypeDef *)&pFirm[flash_fw_size];
	cfg_size = head->cfg_size.b.specific_cfg_size;

	TOUCH_TRACE();

	if (sw82906_reg_read(dev, CHIP_INFO, (u8 *)&d->chip_info,
				sizeof(struct sw82906_chip_info)) < 0) {
		TOUCH_E("Chip info Read Error\n");
	}

	if (d->chip_info.r_conf_dn_index== 0
			|| ((d->chip_info.r_conf_dn_index * cfg_size) > (nSize - flash_fw_size))) {
		TOUCH_I("Invalid Specific CFG Index => 0x%8.8X", d->chip_info.r_conf_dn_index);

	}
	else {
		TOUCH_I("Specific CFG Index => 0x%8.8X", d->chip_info.r_conf_dn_index);
	}

	if (d->chip_info.r_conf_dn_index > 1)
		memcpy((void*)&pFirm[flash_fw_size],
				(void*)&pFirm[flash_fw_size + (d->chip_info.r_conf_dn_index - 1)*cfg_size], cfg_size);

	sw82906_fw_upgrade_code(dev, &pFirm[flash_fw_size], cfg_size);
	touch_msleep(100);

	TOUCH_I("========== CFG Specific Header Info ========");
	specific_header_verify(&pFirm[flash_fw_size], d->chip_info.r_conf_dn_index - 1);
	TOUCH_I("CFG Specific CRC READ :%X(Hex)",
			*((u32*)&pFirm[flash_fw_size + (d->chip_info.r_conf_dn_index)*cfg_size - 4]));

	return ret;
}
#endif	/* __SW82906_SUPPORT_CFG */

static int sw82906_fw_upgrade_post(struct device *dev)
{
	u32 crc_val;
	int ret = 0;

	TOUCH_TRACE();

	sw82906_fw_rst_ctl(dev, 0, NULL);
	touch_msleep(100);

	sw82906_fw_rst_ctl(dev, 2, "crc");
	touch_msleep(100);

	ret = sw82906_fw_flash_crc(dev, &crc_val);
	if (ret < 0) {
		TOUCH_E("flash crc failed, %d\n", ret);
		goto out;
	}

	if (crc_val != CRC_FIXED_VALUE) {
		TOUCH_E("flash crc error %08Xh != %08Xh, %d\n",
				CRC_FIXED_VALUE, crc_val, ret);
		ret = -EFAULT;
		goto out;
	}
	TOUCH_I("flash crc check done\n");

	ret = sw82906_fw_rst_ctl(dev, 1, NULL);
	if (ret < 0)
		goto out;

	touch_msleep(100);

	ret = sw82906_fw_rst_ctl(dev, 0, NULL);
	if (ret < 0)
		goto out;

	touch_msleep(100);

	return 0;

out:
	sw82906_fw_rst_ctl(dev, 0, NULL);

	touch_msleep(100);

	return ret;
}

/*
 *	F/W upgrade Flash Direct Access for sw82906
 */
static int sw82906_fw_upgrade(struct device *dev,
		const struct firmware *fw)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	u8 *fwdata = (u8 *) fw->data;
	int ret = 0;
	int img_check_result;
	int flash_fw_size = d->p_param.flash_fw_size;
	int include_conf = !!(fw->size > flash_fw_size);

	TOUCH_I("%s - START\n", __func__);

	sw82906_tc_driving(dev, LCD_MODE_STOP);

	/* add code because of erase check routine(20200910) - start */
	sw82906_write_value(dev, SYS_RST_CTL, 1);

	touch_msleep(200);
	/* add code because of erase check routine(20200910) - end */

	if (fw->size > MAX_FLASH_SIZE) {
		TOUCH_E("Image Size invalid : %d. The Size must be less then 137KB. The Process of Firmware Download could not process!\n",
				(unsigned int)fw->size);
		return -EPERM;
	} else {
		TOUCH_I("Image Size : 0x%8.8X(%d)\n",
				(unsigned int)fw->size, (unsigned int)fw->size);
	}

	img_check_result = sw82906_fw_binary_verify(dev, (unsigned char *)fwdata, fw->size);
	switch (img_check_result) {
		case E_FW_CODE_ONLY_VALID:
			include_conf = 0;
			break;
		case E_FW_CODE_AND_CFG_VALID:
			break;
		case E_FW_CODE_CFG_ERR:
		case E_FW_CODE_SIZE_ERR:
		default:
			return -EPERM;
	}

	ret = sw82906_fw_rst_ctl(dev, 2, NULL);
	if (ret < 0)
		return ret;

	TOUCH_I("[fw flash mass erase - start]\n");
	ret = sw82906_fw_flash_mass_erase(dev);
	if (ret < 0)
		return ret;
	TOUCH_I("[fw flash mass erase - end]\n");

	TOUCH_I("[Code update]\n");
	sw82906_fw_upgrade_code(dev, fwdata, flash_fw_size);

#if 0	/* not fixed in SW82906 */
	sw82906_write_value(dev, fw_boot_code_addr, FW_BOOT_LOADER_INIT);

	touch_msleep(10);
	sw82906_fw_rst_ctl(dev, 0, NULL);
	touch_msleep(200);

	ret = sw82906_condition_wait(dev, fw_boot_code_addr, NULL,
			FW_BOOT_LOADER_CODE, ~0,
			10, 20, EQ_COND);
	if (ret < 0) {
		TOUCH_E("failed : \'boot check\'\n");
		return -EPERM;
	}

	TOUCH_I("success : boot check\n");
#else
//	sw82906_write_value(dev, SYS_RST_CTL, 0);
#endif

	sw82906_write_value(dev, SYS_RST_CTL, 1);

    touch_msleep(200);

	if (d->p_param.dynamic_reg) {
		if (sw82906_chip_info_load(dev) < 0) {
			TOUCH_E("IC_Register map load fail!\n");
			return -EPERM;
		}
	} else {
		d->reg_info.engine_buf_addr = ENGINE_BUF_OFT;
        d->reg_info.sys_buf_addr = SYS_BUF_OFT;
        TOUCH_I("Dynamic Not Used! sys_buf : 0x%08x, engine_buf : 0x%08x \n",
            d->reg_info.sys_buf_addr, d->reg_info.engine_buf_addr );
	}


#if defined(__SW82906_SUPPORT_CFG)
	sw82906_read_value(dev, CHIP_INFO + 4,
			&d->chip_info.r_conf_dn_index);
	TOUCH_I("conf_dn_index %d\n", d->chip_info.r_conf_dn_index);

	if (d->chip_info.r_conf_dn_index == 0 ||
			d->chip_info.r_conf_dn_index > 20) {
		TOUCH_E("CFG_S_INDEX(%d) invalid\n",
				d->chip_info.r_conf_dn_index);
		return -EPERM;
	}

	if (include_conf) {
		TOUCH_I("[CFG update]\n");
		ret = sw82906_fw_upgrade_conf(dev, fwdata, fw->size);
		if (ret < 0)
			return ret;
	}
#endif	/* __SW82906_SUPPORT_CFG */

	ret = sw82906_fw_upgrade_post(dev);
	if (ret < 0)
		return ret;

	TOUCH_I("===== Firmware download Okay =====\n");

	return 0;
}

static int sw82906_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
//	struct project_param *param = &d->p_param;
	const struct firmware *fw = NULL;
	char fwpath[256] = {0};
//	u32 data = 0;
	int ret = 0;
	int i = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		return -EPERM;
	}

	if (ts->test_fwpath[0]) {
		memcpy(fwpath, &ts->test_fwpath[0], sizeof(fwpath));
		TOUCH_I("get fwpath from test_fwpath:%s\n", &ts->test_fwpath[0]);
	} else if (ts->def_fwcnt) {
		memcpy(fwpath, ts->def_fwpath[0], sizeof(fwpath));
		TOUCH_I("get fwpath from def_fwpath : %s\n", fwpath);
	} else {
		TOUCH_E("no firmware file\n");
		return -EPERM;
	}

	fwpath[sizeof(fwpath) - 1] = '\0';

	if (strlen(fwpath) <= 0) {
		TOUCH_E("error get fw path\n");
		return -EPERM;
	}

	TOUCH_I("fwpath[%s]\n", fwpath);

	ret = request_firmware(&fw, fwpath, dev);

	if (ret < 0) {
		TOUCH_E("fail to request_firmware fwpath: %s (ret:%d)\n",
				fwpath, ret);

		return ret;
	}

	TOUCH_I("fw size:%zu, data: %p\n", fw->size, fw->data);

	d->check_fwup = sw82906_fw_compare(dev, fw);

	if (d->check_fwup == UPGRADE_NEED || d->check_fwup == UPGRADE_FORCE) {
		ret = -EINVAL;
		touch_msleep(200);
		for (i = 0; i < 3 && ret; i++) {
			TOUCH_I("%s - i = %d/ ret = %d\n", __func__, i, ret);
		#if 0	/* not used in SW82906 */
			if (param->wa_trim_wr) {
				if (i == 1) {
					ret = sw82906_read_value(dev, SYS_OSC_CTL, &data);
					if (ret < 0) {
						TOUCH_E("bus error in %s\n", __func__);
						goto error;
					}
					data = data & 0x7F;
					TOUCH_I("SYS_OSC_CTL : %x", data);
					if (data == 4) {
						TOUCH_I("Write AVERAGE_TRIM_VAL : %x", data);
						sw82906_write_value(dev, SYS_OSC_CTL, AVERAGE_TRIM_VAL);
					}
				}
			}
		#endif
			if ((atomic_read(&ts->state.mfts) == MFTS_DS_FLAT)
					&& d->check_fwup != UPGRADE_FORCE) {
				TOUCH_I("Do nothing for ds scinario, force update should be set for upgrade in mfts, state.mfts : %d, d->check_fwup : %d\n",
						atomic_read(&ts->state.mfts), d->check_fwup);
			} else {
				ret = sw82906_fw_upgrade(dev, fw);
				if (ret < 0) {
					TOUCH_E("%s: sw82906_fw_upgrade failed!(ret = %d)\n", __func__, ret);
					//d->check_fwup = UPGRADE_DONE;
					sw82906_reset_ctrl(dev, HW_RESET_ONLY);
					//goto error;
				} else {
					TOUCH_I("%s: sw82906_fw_upgrade is done!\n", __func__);
				}
			}
			TOUCH_I("for loop(i = %d / ret = %d) %s!!!!!\n", i, ret, __func__);
		}
		d->check_fwup = UPGRADE_DONE;
		TOUCH_I("out loop(i = %d) %s!!!!!\n", i, __func__);
	} else {
		release_firmware(fw);
		return -EPERM;
	}
//error:
	release_firmware(fw);
	return 0;
}

#if defined(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
static int sw82906_app_upgrade(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	const struct firmware *fw = NULL;
	struct firmware temp_fw;
	int ret = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.fb) >= FB_SUSPEND) {
		TOUCH_I("state.fb is not FB_RESUME\n");
		return -EPERM;
	}

	if ((ts->app_fw_upgrade.offset == 0) || (ts->app_fw_upgrade.data == NULL)) {
		TOUCH_I("app_fw_upgrade.offset = %d, app_fw_upgrade.data = %p\n",
				(int)ts->app_fw_upgrade.offset, ts->app_fw_upgrade.data);
		return -EPERM;
	}

	memset(&temp_fw, 0, sizeof(temp_fw));
	temp_fw.size = ts->app_fw_upgrade.offset;
	temp_fw.data = ts->app_fw_upgrade.data;

	fw = &temp_fw;

	TOUCH_I("fw size:%zu, data: %p\n", fw->size, fw->data);

	ret = sw82906_fw_upgrade(dev, fw);

	return ret;
}
#endif
static int sw82906_esd_recovery(struct device *dev)
{
	TOUCH_TRACE();

#if defined(CONFIG_LGE_TOUCH_CORE_QCT)
#if IS_ENABLED(CONFIG_LGE_DISPLAY_RECOVERY_ESD) || IS_ENABLED(CONFIG_LGE_TOUCH_PANEL_GLOBAL_RESET)
	lge_mdss_report_panel_dead();
#endif
#endif

#if defined(CONFIG_LGE_TOUCH_CORE_MTK)
	mtkfb_esd_recovery();
#endif

	TOUCH_I("sw82906_esd_recovery!!\n");

	return 0;
}

static int sw82906_suspend(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int boot_mode = 0, ret = 0, i = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Not Ready, Need to turn on IC_NORMAL\n", __func__);
		return -ENODEV;
	}

#if defined(CONFIG_DRM_MSM) && defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
#else
	d->lcd_mode = LCD_MODE_U0;
	TOUCH_I("Force LCD Mode setting : d->lcd_mode = %d\n", d->lcd_mode);
#endif

#elif defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
#else
	d->lcd_mode = LCD_MODE_U0;
	TOUCH_I("Force LCD Mode setting : d->lcd_mode = %d\n", d->lcd_mode);
#endif
#endif

	boot_mode = touch_check_boot_mode(dev);

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
	case TOUCH_MINIOS_AAT:
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
	case TOUCH_MINIOS_MFTS_DS_FLAT:
		if (!ts->mfts_lpwg) {
			TOUCH_I("%s : touch_suspend - MFTS\n", __func__);
			touch_interrupt_control(dev, INTERRUPT_DISABLE);
			sw82906_power(dev, POWER_OFF);
			return -EPERM;
		}
		break;
	case TOUCH_CHARGER_MODE:
	case TOUCH_LAF_MODE:
	case TOUCH_RECOVERY_MODE:
		TOUCH_I("%s: Etc boot_mode(%d)!!!\n", __func__, boot_mode);
		return -EPERM;
	default:
		TOUCH_E("%s: invalid boot_mode = %d\n", __func__, boot_mode);
		return -EPERM;
	}

	TOUCH_D(TRACE, "%s : touch_suspend start\n", __func__);

	if (atomic_read(&d->init) == IC_INIT_DONE) {
		for (i = 0; i < DISPLAY_AREA_ID_MAX; i++)
            sw82906_set_display_area_status(dev, i);
	} else /* need init */
		ret = 1;

	return ret;
}

static int sw82906_resume(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct project_param *param = &d->p_param;
	int boot_mode = TOUCH_NORMAL_BOOT;
	int ret = 0;

	TOUCH_TRACE();

#if defined(CONFIG_DRM_MSM) && defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
#else
	d->lcd_mode = LCD_MODE_U3;
	TOUCH_I("Force LCD Mode setting : d->lcd_mode = %d\n", d->lcd_mode);
#endif

#elif defined(CONFIG_FB)
#if defined(CONFIG_LGE_TOUCH_USE_PANEL_NOTIFY)
#else
	d->lcd_mode = LCD_MODE_U3;
	TOUCH_I("Force LCD Mode setting : d->lcd_mode = %d\n", d->lcd_mode);
#endif
#endif

	if (ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.enable) {
		TOUCH_I("%s: disable lpwg quick_tool_abs\n", __func__);
		sw82906_set_quick_tool_abs(dev, DISPLAY_AREA_ID_0, false, NULL);
	}

	boot_mode = touch_check_boot_mode(dev);

	switch (boot_mode) {
	case TOUCH_NORMAL_BOOT:
	case TOUCH_MINIOS_AAT:
		break;
	case TOUCH_MINIOS_MFTS_FOLDER:
	case TOUCH_MINIOS_MFTS_FLAT:
	case TOUCH_MINIOS_MFTS_CURVED:
		if (!ts->mfts_lpwg) {
			sw82906_power(dev, POWER_ON);
			touch_msleep(ts->caps.hw_reset_delay);
			touch_interrupt_control(dev, INTERRUPT_ENABLE);
			sw82906_ic_info(dev);
			/* TBD */
			/*
			if (sw82906_upgrade(dev) == 0) {
				sw82906_power(dev, POWER_OFF);
				sw82906_power(dev, POWER_ON);
				touch_msleep(ts->caps.hw_reset_delay);
			}
			*/
		}
		break;
	case TOUCH_MINIOS_MFTS_DS_FLAT:
		if (!ts->mfts_lpwg) {
			sw82906_power(dev, POWER_ON);
			touch_msleep(ts->caps.hw_reset_delay);
			touch_interrupt_control(dev, INTERRUPT_ENABLE);
			sw82906_init(dev);
			sw82906_upgrade(dev);
			TOUCH_I("doens't need to be initiated twice, so return it\n");
			ret = -EBUSY;
		}
		break;
	case TOUCH_CHARGER_MODE:
	case TOUCH_LAF_MODE:
	case TOUCH_RECOVERY_MODE:
		TOUCH_I("%s: Etc boot_mode(%d)!!!\n", __func__, boot_mode);
		sw82906_sleep_ctrl(dev, IC_DEEP_SLEEP);
		return -EPERM;
	default:
		TOUCH_E("%s: invalid boot_mode = %d\n", __func__, boot_mode);
		return -EPERM;
	}

	if (param->dfc_resume_power_ctl == RESUME_CTRL_POWER) {
		TOUCH_D(TRACE, "resume power reset start");
		sw82906_power(dev, POWER_OFF);
		sw82906_power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
	} else if (param->dfc_resume_power_ctl == RESUME_CTRL_RESET) {
		TOUCH_D(TRACE, "resume hw reset start");
		sw82906_reset_ctrl(dev, HW_RESET_ONLY);
	}

	return ret;
}

int sw82906_ic_test_unit(struct device *dev, u32 data)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	struct project_param *param = &d->p_param;
	u32 data_rd;
	int ret;

	if (!param->dfc_bus_test) {
		TOUCH_I("ic test addr not valid, skip\n");
		return IC_TEST_ADDR_NOT_VALID;
	}

	ret = sw82906_write_value(dev, BUS_TEST_REG, data);
	if (ret < 0) {
		TOUCH_E("ic test wr err, %08Xh, %d\n", data, ret);
		goto out;
	}

	ret = sw82906_read_value(dev, BUS_TEST_REG, &data_rd);
	if (ret < 0) {
		TOUCH_E("ic test rd err: %08Xh, %d\n", data, ret);
		goto out;
	}

	if (data != data_rd) {
		TOUCH_E("ic test cmp err, %08Xh, %08Xh\n", data, data_rd);
		ret = -EFAULT;
		goto out;
	}

out:
	return ret;
}

static int sw82906_ic_test(struct device *dev)
{
	u32 data[] = {
		0x5A5A5A5A,
		0xA5A5A5A5,
		0xF0F0F0F0,
		0x0F0F0F0F,
		0xFF00FF00,
		0x00FF00FF,
		0xFFFF0000,
		0x0000FFFF,
		0xFFFFFFFF,
		0x00000000,
	};
	int i;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(data); i++) {
		ret = sw82906_ic_test_unit(dev, data[i]);
		if ((ret == IC_TEST_ADDR_NOT_VALID) || (ret < 0)) {
			break;
		}
	}

	if (ret >= 0) {
		TOUCH_I("ic bus r/w test done\n");
	}

	return ret;
}

static int sw82906_init(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	u32 data = 1, rdata = 0;
	int ret = 0, i = 0;

	TOUCH_TRACE();

	if (atomic_read(&ts->state.sleep) == IC_DEEP_SLEEP) {
		TOUCH_I("%s: IC_DEEP_SLEEP. Turn it on before init\n", __func__);
		return -ENODEV;
	}

	TOUCH_I("%s: charger_state = 0x%02X\n", __func__, d->charger);

	ret = sw82906_ic_test(dev);

	if (ret < 0) {
		TOUCH_E("IC_TEST fail\n");
		touch_interrupt_control(dev, INTERRUPT_DISABLE);
		sw82906_power(dev, POWER_OFF);
		sw82906_power(dev, POWER_ON);
		touch_msleep(ts->caps.hw_reset_delay);
		return ret;
	}


	if (d->p_param.dynamic_reg) {
		ret = sw82906_chip_info_load(dev);
		if (ret < 0) {
			TOUCH_E("IC_Register map load fail\n");
			touch_interrupt_control(dev, INTERRUPT_DISABLE);
			sw82906_power(dev, POWER_OFF);
			sw82906_power(dev, POWER_ON);
			touch_msleep(ts->caps.hw_reset_delay);
			return ret;
		}
	} else {
		d->reg_info.engine_buf_addr = ENGINE_BUF_OFT;
		d->reg_info.sys_buf_addr = SYS_BUF_OFT;
		TOUCH_I("Dynamic Not Used! sys_buf : 0x%08x, engine_buf : 0x%08x \n",
		d->reg_info.sys_buf_addr, d->reg_info.engine_buf_addr );
	}

	ret = sw82906_ic_info(dev);
	if (ret < 0) {
		TOUCH_E("Failed to get ic info (ret: %d)\n", ret);
		if (ts->force_fwup == 1) {
			TOUCH_I("%s : Forcefully trigger f/w Upgrade\n", __func__);
			touch_interrupt_control(ts->dev, INTERRUPT_DISABLE);
			ret = sw82906_upgrade(dev);
			if (ret < 0) {
				TOUCH_E("Failed to f/w upgrade (ret: %d)\n", ret);
				return ret;
			}
			ts->force_fwup = 0;
			sw82906_power(dev, POWER_OFF);
			sw82906_power(dev, POWER_ON);
			touch_msleep(ts->caps.hw_reset_delay);
			touch_interrupt_control(ts->dev, INTERRUPT_ENABLE);
			ret = sw82906_ic_info(dev);
			if (ret < 0) {
				TOUCH_E("Failed to get ic info (ret: %d)\n", ret);
				return ret;
			}
		} else {
			atomic_set(&d->init, IC_INIT_NEED);
			d->driving_mode = LCD_MODE_UNKNOWN;
			return ret;
		}
	}

	/* sw82906 special register should be controlled */
	if (ts->bus_type == HWIF_I2C) {
		ret = sw82906_reg_write(dev, SERIAL_SPI_EN, &rdata, sizeof(rdata));
		if (ret)
			TOUCH_E("failed to write \'serial_spi_disable\', ret:%d\n", ret);
		else
			TOUCH_I("serial_spi_en : %d", rdata);
	} else if (ts->bus_type == HWIF_SPI) {
		ret = sw82906_reg_write(dev, SERIAL_I2C_EN, &rdata, sizeof(rdata));
		if (ret)
			TOUCH_E("failed to write \'serial_i2c_disable\', ret:%d\n", ret);
		else
			TOUCH_I("serial_i2c_en : %d", rdata);
	}

	ret = sw82906_reg_write(dev, TC_CMD + tc_device_ctl, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'tc_device_ctrl\', ret:%d\n", ret);

	ret = sw82906_reg_write(dev, TC_CMD + tc_interrupt_ctl, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'tc_interrupt_ctrl\', ret:%d\n", ret);

	ret = sw82906_reg_write(dev, ABT_CMD + SPECIAL_CHARGER_INFO, &d->charger, sizeof(u32));
	if (ret)
		TOUCH_E("failed to write \'spr_charger_sts\', ret:%d\n", ret);

	data = atomic_read(&ts->state.ime);
	ret = sw82906_reg_write(dev, ABT_CMD + SPECIAL_IME_STATUS, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'reg_ime_state\', ret:%d\n", ret);

	data = atomic_read(&ts->state.film);
	ret = sw82906_reg_write(dev, ABT_CMD + SPECIAL_SENSITIVE_INFO, &data, sizeof(data));
	if (ret)
		TOUCH_E("failed to write \'reg_film_state\', ret:%d\n", ret);

	ret = sw82906_temperature_status(dev, true);
	if (ret)
		TOUCH_E("failed to write \'reg_temperature_state\', ret:%d\n", ret);

	for (i = 0; i < DISPLAY_AREA_ID_MAX; i++) {
		ret = sw82906_set_display_area_split_info(dev, TYPE_DISPLAY_AREA, i);
		if (ret)
			TOUCH_E("failed to write \'display_area_split_info\', ret:%d, type:%d, display_area_id:%d\n", ret, TYPE_DISPLAY_AREA, i);
	}
	for (i = 0; i < DISPLAY_AREA_INTERSPACE_ID_MAX; i++) {
		ret = sw82906_set_display_area_split_info(dev, TYPE_DISPLAY_AREA_INTERSPACE, i);
		if (ret)
			TOUCH_E("failed to write \'display_area_split_info\', ret:%d, type:%d, display_area_id:%d\n", ret, TYPE_DISPLAY_AREA, i);
	}
	sw82906_setup_q_sensitivity(dev, d->q_sensitivity);
	sw82906_rotation(dev);
	sw82906_grip_area(dev);

	atomic_set(&d->init, IC_INIT_DONE);
	atomic_set(&ts->state.sleep, IC_NORMAL);

	for (i = 0; i < DISPLAY_AREA_ID_MAX; i++) {
		ret = sw82906_set_display_area_status(dev, i);
		if (ret < 0)
		    TOUCH_E("failed to set_display_area_status id:%d, ret:%d\n", i, ret);
	}

	TOUCH_I("[%s] display split area info :: Front [0~%d], Space[%d~%d], Rear[%d~%d]\n", __func__,
		d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion,
		d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion + 1,
		d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion,
		d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion + 1,
		d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion);

	return 0;
}

int sw82906_check_status(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;
	int i = 0, idx = 0;
	u64 status = (((u64)d->info.ic_status) << 32) | ((u64)d->info.tc_status);
	u64 status_mask = 0x0;

	TOUCH_D(ABS, "%s : status [0x%016llx] ic_status [0x%08x], tc_status [0x%08x]",
			__func__, (u64)status, d->info.ic_status, d->info.tc_status);

	/* Status Checking */
	status_mask = status ^ STATUS_NORMAL_MASK;

	if (status_mask & STATUS_GLOBAL_RESET_BIT) {
		TOUCH_I("%s : Need Global Reset", __func__);
		ret = -EGLOBALRESET;
	} else if (status_mask & STATUS_HW_RESET_BIT) {
		TOUCH_I("%s : Need Touch HW Reset", __func__);
		ret = -EHWRESET_ASYNC;
	} else if (status_mask & STATUS_SW_RESET_BIT) {
		TOUCH_I("%s : Need Touch SW Reset", __func__);
		ret = -ESWRESET;
	} else if (status_mask & STATUS_FW_UPGRADE_BIT) {
		TOUCH_I("%s : Need FW Upgrade, err_cnt = %d %s\n",
				__func__, d->err_cnt, d->err_cnt > 3 ? " skip upgrade":"");
		if (d->err_cnt > 3)
			ret = -ERANGE;
		else
			ret = -EUPGRADE;
		d->err_cnt++;
	} else if (status_mask & STATUS_LOGGING_BIT) {
		TOUCH_I("%s : Need Logging", __func__);
		ret = -ERANGE;

	}

	/* Status Logging */
	if (ret != 0) {
		for (i = 0, idx = 0; i < IC_STATUS_INFO_NUM; i++) {
			idx = ic_status_info_idx[i];
			if (((status_mask >> 32) & 0xFFFFFFFF) & (1 << idx)) {
				if (ic_status_info_str[idx] != NULL) {
					TOUCH_E("[IC_STATUS_INFO][%d]%s, status = %016llx, ic_status = 0x%08x\n",
							idx, ic_status_info_str[idx],
							(u64)status, d->info.ic_status);
				}
			}
		}
		for (i = 0, idx = 0; i < TC_STATUS_INFO_NUM; i++) {
			idx = tc_status_info_idx[i];
			if ((status_mask & 0xFFFFFFFF) & (1 << idx)) {
				if (tc_status_info_str[idx] != NULL) {
					TOUCH_E("[TC_STATUS_INFO][%d]%s, status = %016llx, tc_status = 0x%08x\n",
							idx, tc_status_info_str[idx],
							(u64)status, d->info.tc_status);
				}
			}
		}
	}

	return ret;
}

static void sw82906_lpwg_quick_quick_tool_abs_filter(struct device *dev, u8 touch_id)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	u16 old_y = ts->tdata[touch_id].y;
	u16 new_y = old_y - d->lpwg_abs.offset_y;
	u16 old_mask = ts->old_mask;
	u16 new_mask = ts->new_mask;
	u16 change_mask = old_mask ^ new_mask;
	u16 press_mask = new_mask & change_mask;
	bool hide_lockscreen_coord =
		((atomic_read(&ts->state.lockscreen) == LOCKSCREEN_LOCK) &&
		 (ts->role.hide_coordinate));

	TOUCH_TRACE();

	if ((new_y > ts->caps.max_y) || (new_y < 0)) {
		TOUCH_D(ABS, "%s: invalid new_y(%d)\n", __func__, new_y);
		new_y = 0;
	}

	if (press_mask & (1 << touch_id)) {
		if (hide_lockscreen_coord) {
			TOUCH_I("%s: <id:%d> shift Y value(xxxx->xxxx)\n",
					__func__, touch_id);
		} else {
			TOUCH_I("%s: <id:%d> shift Y value(%d->%d)\n",
					__func__, touch_id, old_y, new_y);
		}
	}

	ts->tdata[touch_id].y = new_y;
}

int sw82906_irq_abs_data(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct sw82906_touch_info *info = &d->info;
	struct sw82906_touch_data *data = info->data;
	struct touch_data *tdata = NULL;
	u32 touch_count = info->touch_cnt;
	u8 finger_index = 0;
	int i = 0;
	int ret = 0;

#ifdef CONFIG_LGE_TOUCH_PEN
	struct sw82906_pen_data *pen = &info->pen;
	u32 pen_count = info->pen_cnt;
//	ts->intr_status &= ~(TOUCH_IRQ_FINGER | TOUCH_IRQ_PEN);

//	ts->new_pen = 0;
	if (pen_count) {
		int in_range = pen->in_range;
		/*
		int hover = (in_range & !pen->contact);
		int tip = (in_range & pen->contact);
		int swit1 = (in_range & pen->swit1);
		int swit2 = (in_range & pen->swit2);
		ts->new_pen = (hover<<PEN_BIT_HOVER) | (tip<<PEN_BIT_TIP);
		ts->new_pen |= (swit1<<PEN_BIT_SWIT1) | (swit2<<PEN_BIT_SWIT2);
*/
		u32 aes_mode_bits = (d->info.tc_status & 0xe0000000) >> 29;
		int aes_mode_1 = 1;
		int aes_mode_2 = 2;
		int auto_detection = 7;

		if (ts->pdata.in_range == 0 && pen->in_range == 1) {
			if (aes_mode_bits == aes_mode_1) {
				TOUCH_I("%s: AES_MODE_1 (%d)\n", __func__, aes_mode_bits);
			} else if (aes_mode_bits == aes_mode_2) {
				TOUCH_I("%s: AES_MODE_2 (%d)\n", __func__, aes_mode_bits);
			} else if (aes_mode_bits == auto_detection) {
				TOUCH_I("%s: AUTO DETECTION! (%d)\n", __func__, aes_mode_bits);
			} else {
				TOUCH_I("%s: UNKNOWN (%d)\n", __func__, aes_mode_bits);
			}
		}

		ts->pdata.in_range = in_range;
		ts->pdata.contact = pen->contact;
		ts->pdata.swit1 = pen->swit1;
		ts->pdata.swit2 = pen->swit2;

		ts->pdata.id = pen->id_upper;
		ts->pdata.id <<= 28L;
		ts->pdata.id |= pen->id_lower;

		ts->pdata.batt = pen->batt;

		if (in_range) {
			ts->pdata.x = pen->x;
			ts->pdata.y = pen->y;
			ts->pdata.pressure = pen->pressure;
			ts->pdata.tilt = pen->tilt;
			ts->pdata.azimuth = pen->azimuth;

			TOUCH_D(ABS,
					"pen data [c %d, r %d, s1 %d, s2 %d, b %d, x %d, y %d, p %d, ti %d, az %d]\n",
					pen->contact,
					pen->in_range,
					pen->swit1,
					pen->swit2,
					pen->batt,
					pen->x,
					pen->y,
					pen->pressure,
					pen->tilt,
					pen->azimuth);
		}

		if (ts->old_mask)
			touch_report_all_event(ts);
		if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.screen) {
			pen_input_handler(ts->dev);
		}
//		ts->intr_status |= TOUCH_IRQ_PEN;
	}
#endif

	if (!touch_count) {
		goto out;
	}

	ts->new_mask = 0;

	/* check if palm detected */
	if (data[0].track_id == PALM_ID) {
		if (data[0].event == TOUCHSTS_DOWN) {
			ts->is_cancel = 1;
			TOUCH_I("Palm Detected\n");
		} else if (data[0].event == TOUCHSTS_UP) {
			ts->is_cancel = 0;
			TOUCH_I("Palm Released\n");
		}
		ts->tcount = 0;
		ts->intr_status |= TOUCH_IRQ_FINGER;
		goto out;
	}

	for (i = 0; i < touch_count; i++) {
		if (data[i].track_id >= MAX_FINGER)
			continue;

		if (data[i].event == TOUCHSTS_DOWN
				|| data[i].event == TOUCHSTS_MOVE) {
			ts->new_mask |= (1 << data[i].track_id);
			tdata = ts->tdata + data[i].track_id;

			tdata->id = data[i].track_id;
			tdata->type = data[i].tool_type;
			tdata->x = data[i].x;
			tdata->y = data[i].y;
			tdata->pressure = data[i].pressure;
			tdata->width_major = data[i].width_major;
			tdata->width_minor = data[i].width_minor;

			if (data[i].width_major == data[i].width_minor)
				tdata->orientation = 1;
			else
				tdata->orientation = (s8)(data[i].angle);

			finger_index++;

			TOUCH_D(ABS,
					"tdata [id:%d t:%d x:%d y:%d z:%d-%d,%d,%d]\n",
					tdata->id,
					tdata->type,
					tdata->x,
					tdata->y,
					tdata->pressure,
					tdata->width_major,
					tdata->width_minor,
					tdata->orientation);

			if (ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.enable) {
				sw82906_lpwg_quick_quick_tool_abs_filter(dev, tdata->id);
			}
		}
	}

	ts->tcount = finger_index;
	ts->intr_status |= TOUCH_IRQ_FINGER;

out:
	return ret;
}

int sw82906_irq_abs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	struct sw82906_touch_info *info = &d->info;
	u32 pen_count = info->pen_cnt;
	u32 touch_count = info->touch_cnt;

	/* check if touch cnt is valid */
	if (touch_count > ts->caps.max_id) {
		TOUCH_I("%s : touch cnt is invalid - %d\n",
				__func__, d->info.touch_cnt);
		return -ERANGE;
	}

	if (!pen_count && !touch_count) {
		return -ERANGE;
	}

	return sw82906_irq_abs_data(dev);
}

int sw82906_irq_etc(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	u32 type = d->info.wakeup_type;
	int ret = 0;

	switch (type) {
	case IRQ_TYPE_FRONT_PALM_DOWN:
		TOUCH_I("IRQ_TYPE_FRONT_PALM_DOWN\n");
		break;
	case IRQ_TYPE_FRONT_PALM_UP:
		TOUCH_I("IRQ_TYPE_FRONT_PALM_UP\n");
		break;
	case IRQ_TYPE_REAR_PALM_DOWN:
		TOUCH_I("IRQ_TYPE_REAR_PALM_DOWN\n");
		break;
	case IRQ_TYPE_REAR_PALM_UP:
		TOUCH_I("IRQ_TYPE_REAR_PALM_UP\n");
		break;
	default:
		TOUCH_I("Not supported irq_type![%d]\n", d->info.wakeup_type);
		break;
	}

	return ret;
}

int sw82906_irq_lpwg(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	u32 type = d->info.wakeup_type;
	int ret = 0;

	switch (type) {
	case IRQ_TYPE_KNOCK_ON:
		TOUCH_I("IRQ_TYPE_KNOCK_ON\n");
		sw82906_get_coordinate(dev, TAP_COUNT_KNOCK_ON);
		ts->intr_status = TOUCH_IRQ_KNOCK;

		if (ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.enable) {
			if (sw82906_check_ai_pick_event(dev)) {
				ts->intr_status = TOUCH_IRQ_AI_PICK;
				TOUCH_I("%s: send ai_pick event!\n", __func__);
			}
		}
		break;

	/* Pay Swipe */
	case IRQ_TYPE_LG_PAY_SWIPE_UP:
		TOUCH_I("IRQ_TYPE_LG_PAY_SWIPE_UP\n");
		sw82906_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_LG_PAY_SWIPE_UP;
		break;

	case IRQ_TYPE_LG_PAY_SWIPE_DOWN:
		TOUCH_I("SWIPE_DOWN\n");
		sw82906_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_SWIPE_DOWN;
		break;

	case IRQ_TYPE_LG_PAY_SWIPE_LEFT:
		TOUCH_I("IRQ_TYPE_LG_PAY_SWIPE_LEFT\n");
		sw82906_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_LG_PAY_SWIPE_LEFT;
		break;

	case IRQ_TYPE_LG_PAY_SWIPE_RIGHT:
		TOUCH_I("IRQ_TYPE_LG_PAY_SWIPE_RIGHT\n");
		sw82906_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_LG_PAY_SWIPE_RIGHT;
		break;

	/* Quick Tool Swipe */
	case IRQ_TYPE_QUICK_TOOL_SWIPE_LEFT:
		TOUCH_I("IRQ_TYPE_QUICK_TOOL_SWIPE_LEFT\n");
		sw82906_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_QUICK_TOOL_SWIPE_LEFT;
		if (ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.enable) {
			TOUCH_I("%s: lpwg quick_tool_abs is enabled - skip SWIPE_L gesture\n",
					__func__);
			ts->intr_status = TOUCH_IRQ_NONE;
		}
		break;

	case IRQ_TYPE_QUICK_TOOL_SWIPE_RIGHT:
		TOUCH_I("IRQ_TYPE_QUICK_TOOL_SWIPE_RIGHT\n");
		sw82906_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_QUICK_TOOL_SWIPE_RIGHT;
		if (ts->display_area_status[DISPLAY_AREA_ID_0].quick_tool_abs.enable) {
			TOUCH_I("%s: lpwg quick_tool_abs is enabled - skip SWIPE_R gesture\n",
					__func__);
			ts->intr_status = TOUCH_IRQ_NONE;
		}
		break;

	/* Fake Off Swipe */
	case IRQ_TYPE_FAKE_OFF_SWIPE_LEFT:
	case IRQ_TYPE_FAKE_OFF_SWIPE_RIGHT:
	case IRQ_TYPE_FAKE_OFF_SWIPE_UP:
	case IRQ_TYPE_FAKE_OFF_SWIPE_DOWN:
		TOUCH_I("IRQ_TYPE_FAKE_OFF_SWIPE (%d)\n", type);
		sw82906_get_swipe_data(dev);
		ts->intr_status = TOUCH_IRQ_FAKE_OFF_SWIPE;
		break;

	case IRQ_TYPE_LONG_PRESS_DOWN:
		TOUCH_I("IRQ_TYPE_LONG_PRESS_DOWN\n");
		ts->intr_status = TOUCH_IRQ_LONGPRESS_DOWN;
		break;

	case IRQ_TYPE_LONG_PRESS_UP:
		TOUCH_I("IRQ_TYPE_LONG_PRESS_UP\n");
		ts->intr_status = TOUCH_IRQ_LONGPRESS_UP;
		break;

	case IRQ_TYPE_SHOW_AOD_ONE_TAP:
		TOUCH_I("IRQ_TYPE_ONE_TAP\n");
		sw82906_get_coordinate(dev, TAP_COUNT_ONETAP);
		ts->intr_status = TOUCH_IRQ_SINGLE_WAKEUP;

		if (ts->display_area_status[DISPLAY_AREA_ID_1].fake_off_onetap.enable) {
			if (sw82906_check_fake_off_onetap_event(dev)) {
				ts->intr_status = TOUCH_IRQ_FAKE_OFF_SINGLE_WAKEUP;
				TOUCH_I("%s: send fake_off_onetap event!\n", __func__);
			}
		}
		break;

	case IRQ_TYPE_FAKE_OFF_ONE_TAP:
		TOUCH_I("IRQ_TYPE_FAKE_OFF_ONE_TAP\n");
		sw82906_get_coordinate(dev, TAP_COUNT_ONETAP);
		ts->intr_status = TOUCH_IRQ_FAKE_OFF_SINGLE_WAKEUP;
		break;

	case IRQ_TYPE_CUSTOM_DEBUG:
		TOUCH_I("IRQ_TYPE_CUSTOM_DEBUG\n");
		sw82906_get_lpwg_failreason(dev);
		break;

	case IRQ_TYPE_PEN_WAKEUP:
		TOUCH_I("IRQ_TYPE_PEN_WAKEUP\n");
		sw82906_get_coordinate(dev, TAP_COUNT_KNOCK_ON);
		ts->intr_status = TOUCH_IRQ_PEN_WAKEUP;

		if (ts->display_area_status[DISPLAY_AREA_ID_0].ai_pick_double_tap.enable) {
			if (sw82906_check_ai_pick_event(dev)) {
				ts->intr_status = TOUCH_IRQ_AI_PICK;
				TOUCH_I("%s: send ai_pick event!\n", __func__);
			}
		}
		break;

	case IRQ_TYPE_PEN_WAKEUP_BTN:
	case IRQ_TYPE_PEN_WAKEUP_BTN_B:
		TOUCH_I("IRQ_TYPE_PEN_WAKEUP_BTN:%d\n", type);
		sw82906_get_coordinate(dev, TAP_COUNT_KNOCK_ON);
		ts->intr_status = TOUCH_IRQ_PEN_WAKEUP_BTN;
		break;

	default:
		TOUCH_I("Not supported irq_type![%d]\n", d->info.wakeup_type);
		break;
	}

	return ret;
}

int sw82906_debug_info(struct device *dev) {
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0, i = 0;
	int count = 0;

	ret = sw82906_reg_read(dev, DEBUG_INFO, &d->debug_info, sizeof(d->debug_info));
	if (ret < 0)
		goto error;

	TOUCH_D(ABS, "%s : debug_info.type: %x, debug_info.length: %x\n", __func__, d->debug_info.type, d->debug_info.length);

	if (d->debug_info.type < DEBUG_INFO_NUM)
		TOUCH_E("[DEBUG_TYPE] [%d]%s \n", d->debug_info.type, debug_info_str[d->debug_info.type]);

	if (d->debug_info.length > 0 && d->debug_info.length <= 12) {
		count = d->debug_info.length / 4;
		for (i = 0; i < count; i++) {
			TOUCH_E("[DEBUG_INFO] Info[%d]: %x", 2 - i, d->debug_info.info[2 - i]);
		}
	}

error:
	return ret;

}

/* Flexible Report */
#define REPORT_BASE_HDR				(12)
#define REPORT_PACKET_SIZE			(sizeof(struct sw82906_touch_data))
#define REPORT_PACKET_BASE_COUNT	(1)

static int sw82906_irq_read_data(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	struct sw82906_touch_info *info = &d->info;
	int is_flex_report = d->p_param.flex_report;
	u32 addr = TC_IC_STATUS;
	char *buf = (char *)&d->info;
	int intr_type = 0;
	int size = 0;
	int pkt_unit = 0;
	int pkt_cnt = 0;
	int touch_cnt = 0;
	int ret = 0;

	pkt_unit = REPORT_PACKET_SIZE;
	pkt_cnt = REPORT_PACKET_BASE_COUNT;
	touch_cnt = MAX_FINGER - pkt_cnt;

	size = REPORT_BASE_HDR;
	size += sizeof(info->pen);
	size += (pkt_unit * pkt_cnt);

	if(is_flex_report) {
		ret = sw82906_reg_read(dev, addr, (void *)buf, size);
		if(ret < 0) {
			TOUCH_E("Register read fail\n");
			d->err_cnt++;
			ret = -EGLOBALRESET;
			goto error;
		}

		TOUCH_D(ABS, "Base Read - Addr: %x, ic: %x, tc:%x, Dst: %p, Touch_cnt: %d, Pen_cnt: %d, Size: %d\n",
				addr, info->ic_status, info->tc_status, info, info->touch_cnt, info->pen_cnt, size);

		if (info->wakeup_type != IRQ_TYPE_ABS_MODE) {
			/* no need to read more */
			return 0;
		}

		if ((info->touch_cnt <= pkt_cnt) ||
			(info->touch_cnt > MAX_FINGER)) {
			/* no need to read more */
			return 0;
		}

		addr += (size>>2);
		buf += size;
		size = 0;

		intr_type = sw82906_tc_sts_irq_type(info->tc_status);

		if (intr_type == INTR_TYPE_REPORT_PACKET) {
			touch_cnt = info->touch_cnt - pkt_cnt;
		} else {
			/* debugging */
			touch_cnt = MAX_FINGER - pkt_cnt;
		}
		if (!touch_cnt) {
			return 0;
		}
	}

	size += (pkt_unit * touch_cnt);

	ret = sw82906_reg_read(dev, addr, (void *)buf, size);
	if(ret < 0) {
		TOUCH_E("Register read fail\n");
		d->err_cnt++;
		ret = -EGLOBALRESET;
		goto error;
	}

	if(is_flex_report) {
		TOUCH_D(ABS, "Extra Read - Addr: %x, Dst: %p, Touch_cnt: %d(%d), Size: %d\n",
				addr, buf, touch_cnt, info->touch_cnt, size);
	}

	return ret;

error:
	if (d->err_cnt > 3) {
		TOUCH_I("%s : But skip err handling, err_cnt = %d\n", __func__, d->err_cnt);
		ret = -ERANGE;
	}

	return ret;
}


int sw82906_irq_handler(struct device *dev)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;

	pm_qos_update_request(&d->pm_qos_req, 10);
	ret = sw82906_irq_read_data(dev);
	pm_qos_update_request(&d->pm_qos_req, PM_QOS_DEFAULT_VALUE);
	if (ret < 0) {
		goto error;
	}

	ret = sw82906_check_status(dev);

	d->intr_type = sw82906_tc_sts_irq_type(d->info.tc_status);
	TOUCH_D(ABS, "%s : intr_type: %x\n", __func__, (int)d->intr_type);

	switch (d->intr_type) {
		case INTR_TYPE_REPORT_PACKET:
			switch (d->info.wakeup_type) {
				case IRQ_TYPE_ABS_MODE:
					sw82906_irq_abs(dev);
					break;
				case IRQ_TYPE_FRONT_PALM_DOWN:
				case IRQ_TYPE_FRONT_PALM_UP:
				case IRQ_TYPE_REAR_PALM_DOWN:
				case IRQ_TYPE_REAR_PALM_UP:
					sw82906_irq_etc(dev);
					break;
				default:
					sw82906_irq_lpwg(dev);
					break;
			}
			break;
		case INTR_TYPE_ABNORMAL_ERROR_REPORT:
		case INTR_TYPE_DEBUG_REPORT:
			sw82906_debug_info(dev);
			break;
		case INTR_TYPE_INIT_COMPLETE:
			TOUCH_I("Init Complete Interrupt!\n");
			break;
		case INTR_TYPE_BOOT_UP_DONE:
			TOUCH_I("Boot Up Done Interrupt!\n");
			break;
		default:
			TOUCH_E("Unknown Interrupt\n");
			break;
	}
error:
	return ret;
}

static ssize_t store_reg_ctrl(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	char command[6] = {0};
	u32 reg = 0;
	u32 value = 0;
	u32 data = 1;
	u16 reg_addr;

	mutex_lock(&ts->lock);

	if (sscanf(buf, "%5s %x %x", command, &reg, &value) <= 0)
		goto out;

	reg_addr = reg;
	if (!strcmp(command, "write")) {
		data = value;
		if (sw82906_reg_write(dev, reg_addr, &data, sizeof(u32)) < 0)
			TOUCH_E("reg addr 0x%x write fail\n", reg_addr);
		else
			TOUCH_I("reg[%x] = 0x%x\n", reg_addr, data);
	} else if (!strcmp(command, "read")) {
		if (sw82906_reg_read(dev, reg_addr, &data, sizeof(u32)) < 0)
			TOUCH_E("reg addr 0x%x read fail\n", reg_addr);
		else
			TOUCH_I("reg[%x] = 0x%x\n", reg_addr, data);
	} else {
		TOUCH_D(BASE_INFO, "Usage\n");
		TOUCH_D(BASE_INFO, "Write reg value\n");
		TOUCH_D(BASE_INFO, "Read reg\n");
	}
out:
	mutex_unlock(&ts->lock);
	return count;
}

static ssize_t show_lpwg_failreason(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;
	u32 rdata = -1;

	mutex_lock(&ts->lock);

	if (sw82906_reg_read(dev, ABT_CMD + LPWG_FAILREASON_REPORT_CTRL1,
				(u8 *)&rdata, sizeof(rdata)) < 0) {
		TOUCH_I("Fail to Read Failreason On Ctrl\n");
		goto out;
	}

	ret = touch_snprintf(buf + ret, PAGE_SIZE,
			"Failreason Ctrl[IC] = %s, data: %x\n", (rdata) ? "Enable" : "Disable", rdata);
	ret += touch_snprintf(buf + ret, PAGE_SIZE - ret,
			"Failreason Ctrl[Driver] = %s\n", d->lpwg_failreason_ctrl ? "Enable" : "Disable");
	TOUCH_I("Failreason Ctrl[IC] = %s, data: %x\n", (rdata) ? "Enable" : "Disable", rdata);
	TOUCH_I("Failreason Ctrl[Driver] = %s\n", d->lpwg_failreason_ctrl ? "Enable" : "Disable");

out:
	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t store_lpwg_failreason(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int value = 0;

	mutex_lock(&ts->lock);

	if (sscanf(buf, "%d", &value) <= 0)
		goto out;

	if (value > 1 || value < 0) {
		TOUCH_I("Set Lpwg Failreason Ctrl - 0(disable), 1(enable) only\n");
		goto out;
	}

	d->lpwg_failreason_ctrl = (u8)value;
	TOUCH_I("Set Lpwg Failreason Ctrl = %s\n", value ? "Enable" : "Disable");
out:
	mutex_unlock(&ts->lock);
	return count;
}

static ssize_t store_reset_ctrl(struct device *dev, const char *buf,
		size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	mutex_lock(&ts->lock);

	if (sscanf(buf, "%d", &value) <= 0)
		goto out;

	sw82906_reset_ctrl(dev, value);

out:
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t store_power_ctrl(struct device *dev, const char *buf,
		size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	mutex_lock(&ts->lock);

	if (sscanf(buf, "%d", &value) <= 0)
		goto out;

	touch_interrupt_control(dev, INTERRUPT_DISABLE);
	sw82906_power(dev, value);

	touch_interrupt_control(dev, INTERRUPT_ENABLE);

out:
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_grip_suppression(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u32 data = 0;

	mutex_lock(&ts->lock);

	ret = sw82906_reg_read(dev, ABT_CMD + GRAB_TOUCH_CTRL, &data, sizeof(data));
	if (ret < 0 ) {
		TOUCH_I("Fail to Read grip_suppression\n");
	}

	ret = touch_snprintf(buf, PAGE_SIZE, "%d\n", data & GRAB_TOUCH_CTRL_BIT);
	TOUCH_I("%s : grip_status[%d], grip_noti[%d]\n", __func__,
			data & GRAB_TOUCH_CTRL_BIT,
			(data & GRAB_NOTI_CTRL_BIT) > 0 ? 1 : 0);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t store_grip_suppression(struct device *dev, const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	u32 value = 0;
	u32 data = 0;

	mutex_lock(&ts->lock);

	if (kstrtos32(buf, 10, &value) < 0)
		goto out;

	ret = sw82906_reg_read(dev, ABT_CMD + GRAB_TOUCH_CTRL, &data, sizeof(data));
	if (ret < 0 ) {
		TOUCH_I("Fail to Read grip_suppression\n");
		goto out;
	}

	if (value == 0) {
		data &= (~GRAB_TOUCH_CTRL_BIT);
	} else if (value == 1) {
		data |= GRAB_TOUCH_CTRL_BIT;
	} else {
		TOUCH_I("Grip suppression value is invalid\n");
		goto out;
	}

	ret = sw82906_reg_write(dev, ABT_CMD + GRAB_TOUCH_CTRL, &data, sizeof(data));
	if (ret < 0 ) {
		TOUCH_I("Fail to Write grip_suppression\n");
		goto out;
	}

	TOUCH_I("%s : grip_status[%d], grip_noti[%d]\n", __func__,
			data & GRAB_TOUCH_CTRL_BIT,
			(data & GRAB_NOTI_CTRL_BIT) > 0 ? 1 : 0);
out:
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t store_q_sensitivity(struct device *dev, const char *buf,
		size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int value = 0;

	mutex_lock(&ts->lock);

	if (sscanf(buf, "%d", &value) <= 0)
		goto out;

	TOUCH_D(QUICKCOVER, "%s: change sensitivity %d -> %d", __func__, d->q_sensitivity, (value));
	d->q_sensitivity = (value); /* 1=enable touch, 0=disable touch */

	if (!(atomic_read(&ts->state.sleep) != IC_DEEP_SLEEP))
		goto out;

	TOUCH_I("%s : %s(%d)\n", __func__,
			(d->q_sensitivity) ? "SENSITIVE" : "NORMAL", (d->q_sensitivity));

	sw82906_setup_q_sensitivity(dev, value);
out:
	mutex_unlock(&ts->lock);
	return count;
}


static ssize_t show_fw_check(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	ret = touch_snprintf(buf, PAGE_SIZE, "%d\n", d->check_fwup);
	TOUCH_I("%s: %d\n", __func__, d->check_fwup);

	mutex_unlock(&ts->lock);

	return ret;
}

#ifdef CONFIG_LGE_TOUCH_PEN
static ssize_t show_predic_filter(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	ret += touch_snprintf(buf + ret, PAGE_SIZE, "%d\n",
			ts->pred_filter);

	TOUCH_I("%s : %s, ts->pred_filter :  %d\n", __func__,
		(ts->pred_filter) ? "Prediction filter On" : "prediction filter Off", ts->pred_filter);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t store_predic_filter(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int value = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	if (kstrtos32(buf, 10, &value) < 0)
		goto out;

	if (value !=0 && value !=1) {
		TOUCH_I("%s : Unknown %d\n", __func__, value);
		goto out;
	}

	ts->pred_filter = value;

	TOUCH_I("%s : %s, value :  %d\n", __func__,
		(value) ? "Prediction filter On" : "prediction filter Off", value);

out:
	mutex_unlock(&ts->lock);

	return count;
}
static ssize_t show_aes_mode(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	mutex_lock(&ts->lock);

	ret = touch_snprintf(buf + ret, PAGE_SIZE, "%d\n",
			ts->aes_mode);

	TOUCH_I("aes_mode : %d\n", ts->aes_mode);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t store_aes_mode(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int enable = 0;

	mutex_lock(&ts->lock);

	if (sscanf(buf, "%d", &enable) <= 0)
		goto out;

	if (enable < 0 || enable > 3) {
		TOUCH_I("%s : invalid value for enable : %d, should be 0 <= value < 4\n");
		goto out;
	}

	ts->aes_mode = enable;

	TOUCH_I("%s: aes_mode = %d\n", __func__, ts->aes_mode);

	/* this routine is for APP (switch individually by APP not thorough HAL) */
	if (ts->aes_mode == 1) {
		touch_send_uevent(ts, TOUCH_UEVENT_SWITCH_AES_TO_1);
		atomic_set(&ts->state.uevent, UEVENT_IDLE);
		ts->aes_mode -= 1;
	} else if (ts->aes_mode == 3) {
		touch_send_uevent(ts, TOUCH_UEVENT_SWITCH_AES_TO_2);
		atomic_set(&ts->state.uevent, UEVENT_IDLE);
		ts->aes_mode -= 1;
	} else if (ts->aes_mode == 2) {
		/* this routine is for HAL (switch BOTH by HAL) */
	} else if (ts->aes_mode == 0) {
		/* this routine is for HAL (switch BOTH by HAL) */
	}

	sw82906_tc_driving(dev, LCD_MODE_U3);

out:
	mutex_unlock(&ts->lock);

	return count;
}
#endif
static ssize_t store_flexible_display_info(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int i = 0, size_of_moving_info = 0, size_of_area_info = 0;
	char *buf_copy = NULL;
	char *sptr = NULL, *sep_result = NULL, *delimiters = "\n";
	int id = 0, x1 = 0, y1 =0, x2 =0, y2= 0;
	int moving_side = 0, moving_state = 0, moving_direction = 0;
	bool is_moving = false;
	struct active_area area = {0, };

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	buf_copy = devm_kzalloc(dev, count + 1, GFP_KERNEL);
	if (!buf_copy) {
		TOUCH_E("Failed to allocate memory for buf_copy\n");
		goto out;
	}

	strncpy(buf_copy, buf, count);
	sptr = buf_copy;

	sep_result = strsep(&sptr, delimiters);
	if (sep_result == NULL) {
		TOUCH_E("%s: failed parsing", __func__);
		goto out;
	}
	TOUCH_D(TRACE, "sep_result : %s", sep_result);

	if (sscanf(sep_result, "%d", &size_of_moving_info) <= 0)
		goto out;

	for (i = 0; i < size_of_moving_info && i < MOVING_INFO_MAX ; i++) {
		sep_result = strsep(&sptr, delimiters);
		if (sep_result == NULL) {
			TOUCH_E("%s: failed parsing(%d)", __func__, i);
			goto out;
		}
		TOUCH_D(TRACE, "sep_result(%d) : %s", i, sep_result);

		if (sscanf(sep_result, "%d %d %d", &moving_side, &moving_state, &moving_direction) <= 0)
			goto out;

		if (moving_side >= UP_SIDE && moving_side < MOVING_INFO_MAX) {
			 ts->flexible_display.moving[moving_side].side = moving_side;
			 ts->flexible_display.moving[moving_side].state = moving_state;
			 ts->flexible_display.moving[moving_side].direction = moving_direction;

			TOUCH_I("%s: moving_side(%s), moving_state(%s), moving_direction(%s)\n", __func__,
					 moving_side_str[moving_side], moving_state_str[moving_state], moving_direction_str[moving_direction]);
		} else {
			TOUCH_E("%s: wrong moving side =%d\n", __func__, moving_side);
		}
	}

	sep_result = strsep(&sptr, delimiters);
	if (sep_result == NULL) {
		TOUCH_E("%s: failed parsing", __func__);
		goto out;
	}
	TOUCH_D(TRACE, "sep_result : %s", sep_result);

	if (sscanf(sep_result, "%d", &size_of_area_info) <= 0)
		goto out;

	for (i = 0; i < size_of_area_info && i < DISPLAY_AREA_ID_MAX ; i++) {
		sep_result = strsep(&sptr, delimiters);
		if (sep_result == NULL) {
			TOUCH_E("%s: failed parsing(%d)", __func__, i);
			goto out;
		}
		TOUCH_D(TRACE, "sep_result(%d) : %s", i, sep_result);

		if (sscanf(sep_result, "%d %d %d %d %d", &id, &x1, &y1, &x2, &y2) <= 0)
			goto out;

		if (id >= 0 && id < DISPLAY_AREA_ID_MAX) {
			 ts->flexible_display.area[id].display_area_id = id;
			 ts->flexible_display.area[id].area.left_top.x = x1;
			 ts->flexible_display.area[id].area.left_top.y = y1;
			 ts->flexible_display.area[id].area.right_bottom.x = x2;
			 ts->flexible_display.area[id].area.right_bottom.y = y2;

			TOUCH_I("%s: display_area_id(%d), start(%d, %d), end(%d, %d)\n", __func__,
					 id, x1, y1, x2, y2);
		} else {
			TOUCH_E("%s: wrong display_area_id=%d\n", __func__, id);
		}
	}

	switch (ts->flexible_display.moving[RIGHT_SIDE].state) {
	case START:
	case MOVING:
		is_moving = true;
		break;
	case STOP:
		is_moving = false;
		break;
	default:
        TOUCH_E("%s, wrong moving state %d\n", __func__, ts->flexible_display.moving[RIGHT_SIDE].state);
		break;
	}

	d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion = ts->flexible_display.area[DISPLAY_AREA_ID_0].area.right_bottom.x;
	d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion = ts->flexible_display.area[DISPLAY_AREA_ID_1].area.right_bottom.x;
	d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion = ts->flexible_display.area[DISPLAY_AREA_ID_1].area.left_top.x - 1;

	// Maximum/minimum value check
	if (d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion < 0) {
		d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion = 0;
	}
	if (d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion < 0) {
		d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion = 0;
	}
	if (d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion < 0) {
		d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion = 0;
	}
	if (d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion > ts->caps.max_x) {
		d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion = ts->caps.max_x;
	}
	if (d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion > ts->caps.max_x) {
		d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion = ts->caps.max_x;
	}
	if (d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion > ts->caps.max_x) {
		d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion = ts->caps.max_x;
	}
	// Always meet the following conditions.
	// DISPLAY_AREA_ID_0 <= DISPLAY_AREA_INTERSPACE_0_1 <= DISPLAY_AREA_ID_1
	if (d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion
			> d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion) {
		d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion =
			d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion;
	}
	if (d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion
			> d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion) {
		d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion =
			d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion;
	}

	TOUCH_I("%s: set End Position (%d, %d, %d)\n", __func__,
			d->display_area_split_info[DISPLAY_AREA_ID_0].resolustion,
			d->display_area_interspace_split_info[DISPLAY_AREA_INTERSPACE_0_1].resolustion,
			d->display_area_split_info[DISPLAY_AREA_ID_1].resolustion);

	/* Moving information update */
	sw82906_set_display_area_moving(dev, is_moving);

	/* Display Area information update */
	for (i = 0; i < DISPLAY_AREA_ID_MAX; i++)
		sw82906_set_display_area_split_info(dev, TYPE_DISPLAY_AREA, i);
	for (i = 0; i < DISPLAY_AREA_INTERSPACE_ID_MAX; i++)
		sw82906_set_display_area_split_info(dev, TYPE_DISPLAY_AREA_INTERSPACE, i);

	/* Flexible Active Area information update (Swipe LG Pay & Knock On) */
	if (ts->flexible_display.moving[RIGHT_SIDE].state == STOP) {
		for (i = 0; i < DISPLAY_AREA_ID_MAX; i++) {
			area.x1 = ts->flexible_display.area[i].area.left_top.x + SENSELESS_PIXEL; // LEFT_SIDE
			area.y1 = ts->flexible_display.area[i].area.left_top.y; // UP_SIDE // Do not set senseless area on upside edge.
			area.x2 = ts->flexible_display.area[i].area.right_bottom.x - SENSELESS_PIXEL; // RIGHT_SIDE
			area.y2 = ts->flexible_display.area[i].area.right_bottom.y - SENSELESS_PIXEL; // DOWN_SIDE

			memcpy(&ts->display_area_status[i].knock_on.area, &area, sizeof(struct active_area));
			memcpy(&ts->display_area_status[i].show_aod_onetap.area, &area, sizeof(struct active_area));
		}
	}

out:
	devm_kfree(dev, buf_copy);
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_flexible_display_info_update_frequency(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	ret += snprintf(buf + ret, PAGE_SIZE - ret, "%d\n", ts->caps.flexible_display_info_update_frequency);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t store_flexible_display_info_update_frequency(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int reply = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	if (kstrtos32(buf, 10, &reply) < 0)
		goto out;

	TOUCH_I("%s : reply = %d\n", __func__, reply);

out:
	mutex_unlock(&ts->lock);

	return count;
}

static ssize_t show_ds_update_state(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int usb_type = atomic_read(&ts->state.connect);
	int wireless = atomic_read(&ts->state.wireless);
	int active_pen = atomic_read(&ts->state.active_pen);
	int ret = 0;
	int cmd = DS_UPDATE_CONNECT;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	ret += touch_snprintf(buf + ret, PAGE_SIZE - ret, "%d %d %d %d\n",
			cmd, usb_type, wireless, active_pen);

	TOUCH_I("ts_driver_state : CMD(%d), usb_type(%d), wireless(%d), active_pen(%d)\n",
			cmd, usb_type, wireless, active_pen);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_gpio_pin(struct device *dev, char *buf)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	ret = touch_snprintf(buf, PAGE_SIZE, "RST:%d, INT:%d\n",
			gpio_get_value(ts->reset_pin), gpio_get_value(ts->int_pin));

	TOUCH_I("%s :%s", __func__, buf);

	mutex_unlock(&ts->lock);

	return ret;
}

static ssize_t show_thermal_noti(struct device *dev, char *buf)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;
	int batt_temp = 0;
	int cam_therm_usr = 0;

	TOUCH_TRACE();

	batt_temp = get_batt_temperature() / 10;
	cam_therm_usr = get_cam_therm_usr_temperature();

	ret = touch_snprintf(buf, PAGE_SIZE, "thermal_noti : %s, batt : %d, cam : %d\n",
			d->thermal_noti? "enable":"disable", batt_temp, cam_therm_usr);

	TOUCH_I("%s :%s", __func__, buf);

	return ret;
}

static ssize_t store_thermal_noti(struct device *dev,
		const char *buf, size_t count)
{
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int noti = 0;

	TOUCH_TRACE();

	mutex_lock(&ts->lock);

	if (kstrtos32(buf, 10, &noti) < 0)
		goto out;

	d->thermal_noti = noti;
	sw82906_temperature_status(dev, true);

	TOUCH_I("%s : thermal_noti = %d\n", __func__, noti);

out:
	mutex_unlock(&ts->lock);

	return count;
}

static TOUCH_ATTR(reg_ctrl, NULL, store_reg_ctrl);
static TOUCH_ATTR(lpwg_failreason, show_lpwg_failreason, store_lpwg_failreason);
static TOUCH_ATTR(reset_ctrl, NULL, store_reset_ctrl);
static TOUCH_ATTR(power_ctrl, NULL, store_power_ctrl);
static TOUCH_ATTR(grip_suppression, show_grip_suppression, store_grip_suppression);
static TOUCH_ATTR(q_sensitivity, NULL, store_q_sensitivity);

#ifdef CONFIG_LGE_TOUCH_PEN
static TOUCH_ATTR(aes_mode, show_aes_mode, store_aes_mode);
static TOUCH_ATTR(prediction_filter, show_predic_filter, store_predic_filter);
#endif
static TOUCH_ATTR(fw_check, show_fw_check, NULL);

static TOUCH_ATTR(ds_update_state, show_ds_update_state, NULL);
static TOUCH_ATTR(flexible_display_info, NULL, store_flexible_display_info);
static TOUCH_ATTR(flexible_display_info_update_frequency, show_flexible_display_info_update_frequency, store_flexible_display_info_update_frequency);
static TOUCH_ATTR(gpio_pin, show_gpio_pin, NULL);
static TOUCH_ATTR(thermal_noti, show_thermal_noti, store_thermal_noti);

static struct attribute *sw82906_attribute_list[] = {
	&touch_attr_reg_ctrl.attr,
	&touch_attr_lpwg_failreason.attr,
	&touch_attr_reset_ctrl.attr,
	&touch_attr_power_ctrl.attr,
	&touch_attr_grip_suppression.attr,
	&touch_attr_q_sensitivity.attr,
#ifdef CONFIG_LGE_TOUCH_PEN
	&touch_attr_prediction_filter.attr,
	&touch_attr_aes_mode.attr,
#endif
	&touch_attr_fw_check.attr,
	&touch_attr_ds_update_state.attr,
	&touch_attr_flexible_display_info.attr,
	&touch_attr_flexible_display_info_update_frequency.attr,
	&touch_attr_gpio_pin.attr,
	&touch_attr_thermal_noti.attr,
	NULL,
};

static const struct attribute_group sw82906_attribute_group = {
	.attrs = sw82906_attribute_list,
};

static int sw82906_register_sysfs(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;

	TOUCH_TRACE();

	TOUCH_I("sw82906 sysfs reigster\n");

	ret = sysfs_create_group(&ts->kobj, &sw82906_attribute_group);
	if (ret < 0) {
		TOUCH_E("sw82906 sysfs register failed\n");
		goto error;
	}

	ret = sw82906_prd_register_sysfs(dev);
#if defined(__SUPPORT_ABT)
	ret = sw82906_abt_register_sysfs(dev);
#endif
	if (ret < 0) {
		TOUCH_E("sw82906 register failed\n");
		goto error;
	}

	return 0;

error:
	kobject_del(&ts->kobj);

	return ret;
}

static int sw82906_get_cmd_version(struct device *dev, char *buf)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int offset = 0;

	offset += touch_snprintf(buf + offset, PAGE_SIZE - offset, "=== ic_fw_version info ===\n");
	offset += touch_snprintf(buf + offset, PAGE_SIZE - offset, "version : v%d.%02d\n",
			d->ic_info.version.major, d->ic_info.version.minor);

	offset += touch_snprintf(buf + offset, PAGE_SIZE - offset,
			"chip_rev : %d, fpc_ver : %d, sensor_ver : %d, machine_num : %d\n",
			d->ic_info.pt_info.chip_rev, d->ic_info.pt_info.fpc_ver, d->ic_info.pt_info.sensor_ver, d->ic_info.pt_info.machine_num);
	offset += touch_snprintf(buf + offset, PAGE_SIZE - offset,
			"product-project id : [%s-%s]\n", d->ic_info.pt_info.product_id, d->ic_info.project_id);

	offset += touch_snprintf(buf + offset, PAGE_SIZE - offset,
			"date: %04x.%02x.%02x, time: %02x:%02x:%02x\n",
			d->ic_info.pt_info.pt_date_year, d->ic_info.pt_info.pt_date_month, d->ic_info.pt_info.pt_date_day,
			d->ic_info.pt_info.pt_time_hour, d->ic_info.pt_info.pt_time_min, d->ic_info.pt_info.pt_time_sec);

	offset += touch_snprintf(buf + offset, PAGE_SIZE - offset, "=== img version info ===\n");
	offset += touch_snprintf(buf + offset, PAGE_SIZE - offset, "version : v%d.%02d\n",
			d->ic_info.img_version.major, d->ic_info.img_version.minor);

	return offset;
}

static int sw82906_get_cmd_atcmd_version(struct device *dev, char *buf)
{
	struct sw82906_data *d = to_sw82906_data(dev);
	int offset = 0;

	offset = touch_snprintf(buf, PAGE_SIZE, "v%d.%02d\n", d->ic_info.version.major, d->ic_info.version.minor);

	return offset;
}

static int sw82906_set(struct device *dev, u32 cmd, void *input, void *output)
{
	TOUCH_TRACE();

	return 0;
}

static int sw82906_get(struct device *dev, u32 cmd, void *input, void *output)
{
	int ret = 0;

	TOUCH_D(BASE_INFO, "%s : cmd %d\n", __func__, cmd);

	switch (cmd) {
	case CMD_VERSION:
		ret = sw82906_get_cmd_version(dev, (char *)output);
		break;

	case CMD_ATCMD_VERSION:
		ret = sw82906_get_cmd_atcmd_version(dev, (char *)output);
		break;

	default:
		break;
	}

	return ret;
}
#ifdef CONFIG_LGE_TOUCH_PEN
static int sw82906_stop_pen_sensing(struct device *dev, bool enable)
{
#if defined(SUPPORT_STOP_BEFORE_DRIVING)
	struct sw82906_data *d = to_sw49202_data(dev);
	struct project_param *param = &d->p_param;
	u32 stop_driving = 0x04;
#endif
	static u32 prev_driving = 0;
	u32 curr_driving = 0;
	u32 addr = TC_CMD + tc_driving_ctl;

	if (enable) {
		sw82906_reg_read(dev, addr, (u8 *)&prev_driving, sizeof(u32));
		curr_driving = (prev_driving & 0xFFF8) | 0x01;
	} else {
		curr_driving = prev_driving;
	}

#if defined(SUPPORT_STOP_BEFORE_DRIVING)
	TOUCH_I("%s : TC Stop before switch no pen mode\n", __func__);
	sw82906_reg_write(dev, addr, &stop_driving, sizeof(stop_driving));
	touch_msleep(param->sys_tc_stop_delay);
#endif

	sw82906_reg_write(dev, addr, &curr_driving, sizeof(u32));

	TOUCH_I("%s[%s] : prev_driving : %X, curr_driving : %X\n", __func__,
		enable ? "T":"F", prev_driving, curr_driving);

	if (enable)
		sw82906_release_pen_event(dev);

	return 0;
}
#endif
#if defined(CONFIG_LGE_TOUCH_COMMON_CONTROL)
/* write function add here */
int sw82906_write_cmd(struct device *dev, int cmd) {
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;
	u32 ctrl = 0;
	u32 addr = 0;
	TOUCH_TRACE();
	/* WorkQueue recommend */
	switch (cmd) {
		case FLIP_INFO:
			TOUCH_I("FLIP_INFO write: %s\n", ts->cmd_data.cmd_buf);
			if (sscanf(ts->cmd_data.cmd_buf, "%d", &d->flip_info) <= 0) {
				ret = -1;
			}
			addr = FLIP_INFO_ADDR;
			if (d->flip_info == FLIP_FRONT) {
				ctrl = 1;
			} else if (d->flip_info == FLIP_BACK) {
				ctrl = 2;
			}
			if (!ret) {
				sw82906_reg_write(dev, addr, &ctrl, sizeof(ctrl));
				TOUCH_I("write flip info(%x) %x\n", addr, ctrl);
			}
			break;

		default:
			TOUCH_I("cmd not found\n");
			ret = -1;
			break;
	}

	if (ret >= 0) {
		TOUCH_I("%s cmd:%d (%s)\n", __func__, cmd, ts->cmd_data.cmd_buf);
		cmd_set(dev, (char *)ts->touch_ic_name, ts->cmd_data.cmd_buf,
				ts->cmd_data.rw_mode);
	}

	return ret;
}

/* read function add here */
int sw82906_read_cmd(struct device *dev, int cmd) {
	struct touch_core_data *ts = to_touch_core(dev);
	struct sw82906_data *d = to_sw82906_data(dev);
	int ret = 0;
	TOUCH_TRACE();
	/* WorkQueue recommend */
	switch (cmd) {
		case FLIP_INFO:
			TOUCH_I("FLIP_INFO read: %d\n", d->flip_info);
			ret = snprintf(ts->cmd_data.read_buf, MAX_CMD_LENGTH,
					"%d", d->flip_info);
			break;

		default:
			TOUCH_I("cmd not found\n");
			ret = -1;
			break;
	}

	if (ret >= 0) {
		TOUCH_I("%s cmd:%d (%s)\n", __func__, cmd, ts->cmd_data.read_buf);
		cmd_set(dev, (char *)ts->touch_ic_name, ts->cmd_data.read_buf,
				ts->cmd_data.rw_mode);
	}

	return ret;
}
static int sw82906_common_control(struct device *dev, int rw_mode, char *cmd)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	int cmd_r = -1;
	TOUCH_TRACE();
	cmd_r = cmd_num_search(dev, (char *)ts->touch_ic_name, cmd);
	TOUCH_I("cmd(%d)%s rw_mode:%d\n", cmd_r, cmd, rw_mode);

	if (rw_mode == WRITE_MODE) {
		ret = sw82906_write_cmd(dev, cmd_r);
	} else if (rw_mode == READ_MODE) {
		ret = sw82906_read_cmd(dev, cmd_r);
	} else {
	}

	return ret;

}

#endif
static struct touch_driver touch_driver = {
	.probe = sw82906_probe,
	.remove = sw82906_remove,
	.shutdown = sw82906_shutdown,
	.suspend = sw82906_suspend,
	.resume = sw82906_resume,
	.init = sw82906_init,
	.irq_handler = sw82906_irq_handler,
	.power = sw82906_power,
	.upgrade = sw82906_upgrade,
#if defined(CONFIG_LGE_TOUCH_APP_FW_UPGRADE)
	.app_upgrade = sw82906_app_upgrade,
#endif
	.esd_recovery = sw82906_esd_recovery,
	.swipe_enable = sw82906_swipe_enable,
	.lpwg = NULL,
	.display_area_status = sw82906_display_area_status,
	.lpwg_function_info = sw82906_lpwg_function_info,
	.notify = sw82906_notify,
	.init_pm = sw82906_init_pm,
	.register_sysfs = sw82906_register_sysfs,
	.set = sw82906_set,
	.get = sw82906_get,
	.rotation = sw82906_rotation,
	.grip_area = sw82906_grip_area,
#ifdef CONFIG_LGE_TOUCH_PEN
	.stop_pen_sensing = sw82906_stop_pen_sensing,
#endif
#if defined(CONFIG_LGE_TOUCH_COMMON_CONTROL)
	.common_control = sw82906_common_control,
#endif
};

#define MATCH_NAME			"lge,sw82906"

static struct of_device_id touch_match_ids[] = {
	{ .compatible = MATCH_NAME, },
	{},
};

static struct touch_hwif hwif = {
	.bus_type = HWIF_I2C,
	.name = LGE_TOUCH_NAME,
	.owner = THIS_MODULE,
	.of_match_table = of_match_ptr(touch_match_ids),
	/* for spi */
	/*
	.bits_per_word = 8,
	.spi_mode = SPI_MODE_0,
	.max_freq = (25 * 1000000),
	*/
};

static int __init touch_device_init(void)
{
	int maker = 0;
	TOUCH_TRACE();

	if (!is_ddic_name("nt37800a")) {
		TOUCH_I("%s, ddic nt37800a, touch ic sw82906 not found.\n", __func__);
		return 0;
	}

	TOUCH_I("%s, sw82906 detected: touch_id:%d\n", __func__, maker);

	return touch_bus_device_init(&hwif, &touch_driver);
}

static void __exit touch_device_exit(void)
{
	TOUCH_TRACE();
	touch_bus_device_exit(&hwif);

	TOUCH_I("%s, sw82906 ends\n", __func__);
}

late_initcall(touch_device_init);
module_exit(touch_device_exit);

MODULE_AUTHOR("hoyeon.jang@lge.com");
MODULE_DESCRIPTION("LGE touch driver v5");
MODULE_LICENSE("GPL");
