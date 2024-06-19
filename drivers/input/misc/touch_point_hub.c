/*
 * UltraSense Systems Touch Point Hub I2C Driver
 *
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/version.h>
//#include <linux/msm_drm_notify.h>
#ifdef CONFIG_MACH_LAHAINA_BLM
#include <soc/qcom/lge/board_lge.h>
#endif
#include "touch_point_hub.h"

/**********************************************************
 ******************** Common Constants ********************
 **********************************************************/
/* MAJOR.MINOR.PATCH */
#define TOUCH_POINT_HUB_LINUX_DRIVER_VERSION "4.2.9"

MODULE_VERSION(TOUCH_POINT_HUB_LINUX_DRIVER_VERSION);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("UltraSense Systems Touch Point Hub I2C Driver");
MODULE_AUTHOR("UltraSense Systems Inc");

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0))
#define KERNEL_BELOW_3_19
#endif

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(v) (void)(v)
#endif

#define MAX_SENSORS_NUM							(4)
#define MAX_KEY_NAME_LEN						(32)
#define MAX_HUB_VERSION_LEN						(32)
#define TOUCH_POINT_HUB_FIRMWARE_NAME			"touch_point_hub.fw"
#define TOUCH_POINT_HUB_FIRMWARE_NAME_BLM_NA	"touch_point_hub.blm-na.fw"
#define I2C_MAX_RETRY_NUM						(3)
/*
 * limit i2c r/w max length to 32bytes for FW downloading.
 */
#define FW_DL_I2C_CHUNK_SIZE					(32)

/* hub wakeup request rejection time duration */
#define TOUCH_POINT_HUB_WAKEUP_REJECTION_TIME_DURATION_MS	(500)

/* sns wakeup request rejection time duration */
#define TOUCH_POINT_SNS_WAKEUP_REJECTION_TIME_DURATION_MS	(500)

#define ENABLE_HUB_WAKEUP_BY_INT_PIN			(0)
#if ENABLE_HUB_WAKEUP_BY_INT_PIN
/**
 * If AP<->Hub's interrupt pin will be used to wakeup hub, unlock interrupt pin first before operating gpio.
 */
#define OPERATE_IRQ_GPIO_LINE					(1)
#else
#define OPERATE_IRQ_GPIO_LINE					(0)
#define ENABLE_HUB_WAKEUP_BY_I2C				(1)
#endif

#define ENABLE_SNS_FW_HOLD_VIA_FD_REG			(1)

#define ENABLE_SNS_DSRAM_SINGLE_BYTE_RW			(1)
/**
 * DRM: Direct Rendering Manager
 * If set to 1, get display ON/OFF notification from DRM
 * If set to 0, get display ON/OFF notification from framebuffer(N/A for now).
 */
#define ENABLE_MSM_DRM_NOTIFIER					(0)

/* enable/disable suspend & resume */
#define ENABLE_SUSPEND_RESUME					(1)

/* set hub recovery rejection time */
#define TOUCH_POINT_HUB_RECOVERY_REJECTION_TIME_MS			(10000)

/* wait time for hub init done */
#define TOUCH_POINT_HUB_INIT_DONE_WAIT_TIME_US				(3000000UL) //3s

/* periodic monitor hub and sensors status */
#define TOUCH_POINT_HUB_MONITOR_TIMER_PERIOD_MS					(120*1000UL)	//120s

/**********************************************************
 ******************** Hub Constants ***********************
 **********************************************************/
#define TOUCH_POINT_HUB_FW_VER_START_ADDR_IN_MSBL_FILE		(0x18)
#define TOUCH_POINT_HUB_BL_PAGE_CNT_ADDR_IN_MSBL_FILE		(0x44)
#define TOUCH_POINT_HUB_BL_PAGE_START_ADDR_IN_MSBL_FILE		(0x4C)
#define TOUCH_POINT_HUB_BL_MAX_TRANSMIT_CHUNK_SIZE_IN_BYTE	(128)
#define TOUCH_POINT_HUB_BL_I2C_ADDR				(0x55) //7-bit i2c addr
#define TOUCH_POINT_HUB_WHOAMI_VALUE			(0xAA)

/**********************************************************
 ******************** Hub Register Map ********************
 **********************************************************/
#define TOUCH_POINT_HUB_REG_0X27_SNS_INIT_STATUS	(0x08)
#define TOUCH_POINT_HUB_REG_0X2F_SNS_INIT_STATUS	(0x09)
#define TOUCH_POINT_HUB_REG_0X37_SNS_INIT_STATUS	(0x0A)
#define TOUCH_POINT_HUB_REG_0X3F_SNS_INIT_STATUS	(0x0B)
#define TOUCH_POINT_HUB_REG_RUNNING_INDICATOR	(0x0C)
#define TOUCH_POINT_HUB_REG_VER_MAJOR			(0x0D)
#define TOUCH_POINT_HUB_REG_VER_MINOR			(0x0E)
#define TOUCH_POINT_HUB_REG_VER_PATCH			(0x0F)
#define TOUCH_POINT_HUB_REG_WHOAMI				(0x10)
#define TOUCH_POINT_HUB_REG_INIT_STATUS			(0x11)
	#define TOUCH_POINT_HUB_INIT_STATUS_MASK			(0x01)
#define TOUCH_POINT_HUB_REG_SENSOR_BITMAP		(0x12)
#define TOUCH_POINT_HUB_REG_INTR_STATUS			(0x13)
#define TOUCH_POINT_HUB_REG_CONTROL				(0x14)
	#define TOUCH_POINT_HUB_BYPASS_HUB_ALGO_MASK		(0x01)
	#define TOUCH_POINT_HUB_ALWAYS_ON_MASK				(0x02)
	#define TOUCH_POINT_HUB_UPDATE_DATA_TO_ALGO_MASK	(0x04)	
	#define TOUCH_POINT_HUB_RESET_HUB_ALGO_MASK 		(0x08)
	#define TOUCH_POINT_HUB_ALGO_MODE_MASK				(0x30)
#define TOUCH_POINT_HUB_REG_SWIPE_UP_EVENT_CNT			(0x21)
#define TOUCH_POINT_HUB_REG_SWIPE_DOWN_EVENT_CNT		(0x22)
#define TOUCH_POINT_HUB_REG_CONTROL2			(0x24)
	#define TOUCH_POINT_HUB_AP_POWEROFF_MASK			(0x01)
#define TOUCH_POINT_HUB_REG_HUB_KEY_MAP			(0x29)
	#define TOUCH_POINT_HUB_0X27_KEY_CODE_MASK			(0x03)
	#define TOUCH_POINT_HUB_0X2F_KEY_CODE_MASK			(0x0C)
	#define TOUCH_POINT_HUB_0X37_KEY_CODE_MASK			(0x30)
	#define TOUCH_POINT_HUB_0X3F_KEY_CODE_MASK			(0xC0)
#define TOUCH_POINT_HUB_REG_BOARD_REV			(0x2A)
#define TOUCH_POINT_HUB_REG_RESET_INDICATOR		(0x2D)
#define TOUCH_POINT_HUB_REG_HUB_EN_HAPTIC		(0x33)
	#define TOUCH_POINT_HUB_EN_HAPTIC_MASK				(0x0F)

/**********************************************************
 ******************** Sensor Constants ********************
 **********************************************************/
#define TOUCH_POINT_SNS_A0_WHOAMI_VALUE			(0x62)
#define TOUCH_POINT_SNS_A2_WHOAMI_VALUE			(0x64)
/* sensor FW version memory size */
#define TOUCH_POINT_SNS_FW_VER_DATE_BYTES		(12)
#define TOUCH_POINT_SNS_FW_VER_TIME_BYTES		(9)
#define TOUCH_POINT_SNS_FW_VER_MAX_STR_LEN		(64)
#define TOUCH_POINT_SNS_USP_OFFSET_VALUE		(1500)
#define TOUCH_POINT_SNS_ZFORCE_OFFSET_VALUE		(1024)

/**********************************************************
 ******************** Sensor Register Map *****************
 **********************************************************/
/* hw regs */
#define TOUCH_POINT_SNS_FORCE_RUNNING_RST_REG	(0x18FA)
#define TOUCH_POINT_SNS_BD_WHOAMI_REG			(0x18FD)

/* dsram memory */
#define TOUCH_POINT_SNS_REG_PAGE_ADDR			(0x1100)
#define TOUCH_POINT_SNS_REG_ADC					(TOUCH_POINT_SNS_REG_PAGE_ADDR + 0x00)
#define TOUCH_POINT_SNS_SENSOR_CTRL				(TOUCH_POINT_SNS_REG_PAGE_ADDR + 0x11)
	#define TOUCH_POINT_SNS_REG_SENSOR_CTRL_HOLDING_MASK		(0x02)
	#define TOUCH_POINT_SNS_REG_SENSOR_CTRL_LPM_MASK			(0x08)
	//#define TOUCH_POINT_REG_SENSOR_CTRL_FW_FLAG_MASK	(0x80)
#define TOUCH_POINT_SNS_LPM_ODR_SEL             (TOUCH_POINT_SNS_REG_PAGE_ADDR + 0x7A)
#define TOUCH_POINT_SNS_HPM_ODR_SEL             (TOUCH_POINT_SNS_REG_PAGE_ADDR + 0x7C)
#define TOUCH_POINT_SNS_SWREG_VERBYTE1_REVC		(TOUCH_POINT_SNS_REG_PAGE_ADDR + 0x7E)

/* fd 1-byte regs */
#define TOUCH_POINT_SNS_REG_FD_SC				(0x51)
	#define TOUCH_POINT_SNS_FD_HOST_WAKEUP_CHIP_BIT_MASK		(0x02)//BIT1: RTL looks for negative transition and pulls mcu int0 low for ~5 cycles
#define TOUCH_POINT_SNS_REG_FD_HOLD_FW			(0x57)
	#define TOUCH_POINT_SNS_REG_FD_HOLD_FW_BIT_MASK					(0x02)
#define TOUCH_POINT_SNS_REG_ADDR_INDEX_H		(0x60)
#define TOUCH_POINT_SNS_REG_ADDR_INDEX_L		(0x61)
#define TOUCH_POINT_SNS_REG_WHOAMI				(0x6E)
#define TOUCH_POINT_SNS_REG_BURST_WRITE			(0x70)
#define TOUCH_POINT_SNS_REG_BURST_READ			(0xF0)

/* otp memory */
/* OTP Mem Dump */
#define TOUCH_POINT_SNS_OTP_MEM_START_ADDR		(0x1A00)
#define TOUCH_POINT_SNS_OTP_MEM_END_ADDR		(0x29FF)//(0x1AFF)
#define TOUCH_POINT_SNS_OTP_MEM_SIZE			(TOUCH_POINT_SNS_OTP_MEM_END_ADDR - TOUCH_POINT_SNS_OTP_MEM_START_ADDR + 1)
#define TOUCH_POINT_SNS_OTP_MEM_FW_START_ADDR	(0x1B00)

/* OTP ZForce */
#define TOUCH_POINT_SNS_OTP_ZFORCE_START_ADDR	(0x1A16)
#define TOUCH_POINT_SNS_OTP_ZFORCE_END_ADDR		(0x1A35)
#define TOUCH_POINT_SNS_OTP_ZFORCE_SIZE			(TOUCH_POINT_SNS_OTP_ZFORCE_END_ADDR - TOUCH_POINT_SNS_OTP_ZFORCE_START_ADDR + 1)
/* OTP USP Bank0 */
#define TOUCH_POINT_SNS_OTP_BANK0_START_ADDR	(0x1A66)
#define TOUCH_POINT_OTP_BANK0_END_ADDR			(0x1A79)
#define TOUCH_POINT_SNS_OTP_BANK0_SIZE			(TOUCH_POINT_OTP_BANK0_END_ADDR - TOUCH_POINT_SNS_OTP_BANK0_START_ADDR + 1)
/* OTP USP Bank1 */
#define TOUCH_POINT_SNS_OTP_BANK1_START_ADDR	(0x1AA6)
#define TOUCH_POINT_OTP_BANK1_END_ADDR			(0x1AB9)
#define TOUCH_POINT_SNS_OTP_BANK1_SIZE			(TOUCH_POINT_OTP_BANK1_END_ADDR - TOUCH_POINT_SNS_OTP_BANK1_START_ADDR + 1)
/* OTP IDs */
#define TOUCH_POINT_SNS_OTP_LOTID_H_ADDR		(0x1AEB)
#define TOUCH_POINT_SNS_OTP_LOTID_L_ADDR		(0x1AEC)
#define TOUCH_POINT_SNS_OTP_STACKID_MSB_ADDR	(0x1AED)
#define TOUCH_POINT_SNS_OTP_STACKID_LSB_ADDR	(0x1AEE)
#define TOUCH_POINT_SNS_OTP_OTP_VER_ADDR		(0x1AEF)
#define TOUCH_POINT_SNS_OTP_SIREV_OLD_ADDR		(0x1AF0)
#define TOUCH_POINT_SNS_OTP_FW_VER_H_ADDR		(0x1AF1)
#define TOUCH_POINT_SNS_OTP_FW_VER_L_ADDR		(0x1AF2)
#define TOUCH_POINT_SNS_OTP_TIMEOFF_H_ADDR		(0x1AF3)
#define TOUCH_POINT_SNS_OTP_TIMEOFF_L_ADDR		(0x1AF4)
#define TOUCH_POINT_SNS_OTP_CHIP_ADDR			(0x1AF5)
#define TOUCH_POINT_SNS_OTP_DIEID_R_H_ADDR		(0x1AF7)
#define TOUCH_POINT_SNS_OTP_DIEID_R_L_ADDR		(0x1AF8)
#define TOUCH_POINT_SNS_OTP_DIEID_C_H_ADDR		(0x1AF9)
#define TOUCH_POINT_SNS_OTP_DIEID_C_L_ADDR		(0x1AFA)
#define TOUCH_POINT_SNS_OTP_WAFERID_ADDR		(0x1AFB)
#define TOUCH_POINT_SNS_OTP_SIREV_NEW_ADDR		(0x1AFC)

/* Convert OTP addr to OTP buffer index */
#define TOUCH_POINT_SNS_OTPADDR2IDX(addr)		(addr - TOUCH_POINT_SNS_OTP_MEM_START_ADDR)

#if OPERATE_IRQ_GPIO_LINE
#include "../../../drivers/gpio/gpiolib.h"
#endif

extern uint32_t ussys_key_cnt;

typedef enum touch_point_hub_algo_mode {
	HUB_USP_ONLY_ALGO_MODE,
	HUB_USP_ZFORCE_ALGO_MODE,
	HUB_USP_ZFORCE_ALGO_ON_SENSOR_MODE
} touch_point_hub_algo_mode_t;

typedef enum touch_point_sns_hpm_odr {
	HPM_ODR_200HZ	= 0x0000,
	HPM_ODR_100HZ	= 0x208D,
	HPM_ODR_75HZ	= 0x3640,
	HPM_ODR_50HZ	= 0x61A8,
	HPM_ODR_25HZ	= 0xE3DD,
} touch_point_sns_hpm_odr_t;

typedef enum touch_point_sns_lpm_odr {
	LPM_ODR_200HZ	= 0x0000,
	LPM_ODR_100HZ	= 0x000D,
	LPM_ODR_75HZ 	= 0x0016,
	LPM_ODR_50HZ 	= 0x0028,
	LPM_ODR_25HZ	= 0x005D,
	LPM_ODR_5HZ		= 0x0208,
	LPM_ODR_01HZ	= 0x681D,// 1/10s for Sleep mode
} touch_point_sns_lpm_odr_t;

/* sensor structure */
typedef struct touch_point_sns {
	uint8_t idx;
	uint8_t i2c_addr;
	char name[MAX_KEY_NAME_LEN];
	unsigned int input_ev_code;
	bool input_ev_enabled;
	uint8_t whoami;
	uint8_t si_rev;/*A0,A1,A2*/
	bool key_pressed;
	/* to prevent toggling sensor's int0 too much */
	ktime_t toggle_ts;
	
	u8 *otp_mem;

	/* fw ver */
	uint8_t ver_date[TOUCH_POINT_SNS_FW_VER_DATE_BYTES];
	uint8_t ver_time[TOUCH_POINT_SNS_FW_VER_TIME_BYTES];
} touch_point_sns_t;

/* hub platform data structure */
struct touch_point_hub_data {
	struct i2c_client *client;
	struct hrtimer timer;
	struct delayed_work delaywork;
	struct workqueue_struct *touch_point_hub_wq;
	struct mutex lock;
	struct mutex i2c_lock;
	struct regulator *vdd;
	struct miscdevice misc_dev;	
	/* input device */
	struct input_dev *input_dev;
	struct gpio_desc *gpiod_irq;
	struct gpio_desc *gpiod_hub_rst;
	struct gpio_desc *gpiod_sns_rst;
	/*
	 * If true, AP reset gpio pin(LOW->HIGH->LOW) will reset hub.
	 * If false, AP reset pin(HIGH->LOW->HIGH) will reset hub.
	 */
	bool hub_rst_high_active;
	/*
	 * If true, AP reset gpio pin(LOW->HIGH->LOW) will reset sensor.
	 * If false, AP reset pin(HIGH->LOW->HIGH) will reset sensor.
	 */
	bool sns_rst_high_active;
	bool vdd_supply_configured_in_dts;
	u32 rst_convert_time_ms;
	bool wakeup;
	/* add++ for reg_read sysfs node */
	u8 target_to_read;
	u16 reg_to_read;
	u16 size_to_read;	
	/* add-- for reg_read sysfs node */
	s32 miscdev_count;
	unsigned int input_code_in_dts[MAX_SENSORS_NUM];
	u32 en_haptic[MAX_SENSORS_NUM];
	uint8_t whoami;
	char version[MAX_HUB_VERSION_LEN];

	/* record hub wakeup time */
	ktime_t wakeup_ts;
	/* record hub recovery time */
	ktime_t recovery_ts;
	/* send recovery data to daemon during hub recovery */
	uint8_t recovery_data;

#if ENABLE_MSM_DRM_NOTIFIER
	struct notifier_block drm_notif;/*Register DRM notifier*/
#else
#ifdef CONFIG_FB
	struct notifier_block fb_notif;/*Register FB notifier*/
#endif
#endif/*ENABLE_MSM_DRM_NOTIFIER*/

	bool is_cali_ongoing;
	struct fasync_struct *fasync;

	bool init_done;/* indicates if hub init is done */
	bool init_once;/* some init code block only can be executed one time */
	bool is_otp_dumped;
	bool is_i2c_rw_just_after_hub_reset;
	ktime_t timer_start_ts;

	touch_point_sns_t sns[MAX_SENSORS_NUM];
	uint8_t sns_cnt;
	uint8_t curr_sns_idx;

	/* add++ for bootloader access */
	char bl_version[MAX_HUB_VERSION_LEN];
	unsigned short bl_i2c_addr;
	/* add-- for bootloader access */

	/* add++ for periodic monitor timer */
	struct delayed_work	monitor_work;
	/* add-- for periodic monitor timer */

	/* add++ for swipe gesture */
	u32 swipe_up_cnt;
	u32 swipe_down_cnt;
	/* add-- for swipe gesture */	
	u32 chip_mode_in_dts;
	u32 board_rev;
	u32 known_num_of_keys;
	bool dev_get_resumed;
};

static int touch_point_hub_recovery(struct touch_point_hub_data *pdata);

/**
 * Get key name by key input code in dts.
 * This key mapping only affects touch point test app key name, should keep this key mapping same as touch_point.kl
 */
static const char* touch_point_hub_get_key_name(unsigned int code)
{
	switch (code) {
	case KEY_VOLUMEUP:
		return "Vol+";
	case KEY_VOLUMEDOWN:
		return "Vol-";
	case KEY_HOME:
		return "Home";
	case KEY_POWER:
		return "Power";
	default:
		return "Unknown";
	}
}

static const char* touch_point_hub_get_chip_mode_string(uint32_t chip_mode)
{
	switch (chip_mode) {
	case 0:
		return "USP+ZForce@Sensor";
	case 1:
		return "USP-Only";
	case 3:
		return "USP+ZForce@Hub";
	default:
		return "Unknown";
	}
}

static uint8_t touch_point_hub_get_hub_key_code(unsigned int input_code)
{
	switch (input_code) {
	case KEY_VOLUMEUP:/*115*/
		return 0;/*vol+*/
	case KEY_VOLUMEDOWN:/*114*/
		return 1;/*vol-*/
	case KEY_POWER:/*116*/
		return 3;/*power*/
	default:
		return 2;/*function*/
	}
}

/**********************************************************
 ******************** I2C functions ***********************
 **********************************************************/
/* add++ for bootloader access */
static int touch_point_hub_bl_send(struct touch_point_hub_data *pdata, u8 *buf, u16 len)
{
	struct i2c_client *client = pdata->client;
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = pdata->bl_i2c_addr,
		.flags = 0,
		.len = len,
		.buf = buf,
	};

	if (NULL == buf || len < 1)
	    return -EINVAL;

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "[hub bl] i2c write failed!");
		return -EIO;
    }

	return 0;
}

static int touch_point_hub_bl_receive(struct touch_point_hub_data *pdata, u8 *buf, u16 len)
{
	struct i2c_client *client = pdata->client;
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = pdata->bl_i2c_addr,
		.flags = I2C_M_RD,
		.len = len,
		.buf = buf,
	};

	if (NULL == buf || len < 1)
	    return -EINVAL;

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "[hub bl] i2c read failed!");
		return -EIO;
    }

	return 0;
}
/* add-- for bootloader access */

/* write hub reg */
static int touch_point_hub_reg_write(struct touch_point_hub_data *pdata, u8 reg_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 *buf = kzalloc(len + 4, GFP_KERNEL);
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = len + 4,
		.buf = buf,
	};

	if (NULL == buf)
	    return -ENOMEM;

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject hub i2c write(i2c_addr:%#X ,reg_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			client->addr,
			reg_addr);
		if (NULL != buf)
			kfree(buf);
		return -EPERM;
	}

	buf[0] = 0x62;		//target byte for hub. It represents that this function is intend to communicate with hub's regs instead of sensors' regs.
	buf[1] = 1 + len;	//wt_len;//1->reg_addr length, len->val buffer length
	buf[2] = 1;         //rd_len;//dummy value for write-only ops.
	buf[3] = reg_addr;	//reg start addr;
	memcpy(&buf[4], val, len);

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	kfree(buf);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "hub reg i2c write %#X failed!", reg_addr);
		return -EIO;
    }

	return 0;
}

/* read hub reg */
static int touch_point_hub_reg_read(struct touch_point_hub_data *pdata, u8 reg_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 wt_buf[4] = {0x62/*target byte for hub*/, 1/*wt_len*/, len/*rd_len*/, reg_addr};
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(wt_buf),
			.buf = wt_buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		}
	};

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject hub i2c read(i2c_addr:%#X ,reg_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			client->addr,
			reg_addr);
		return -EPERM;
	}

	do {
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "hub reg i2c read %#X failed!", reg_addr);
		return -EIO;
    }

	return 0;
}

/* write sensor reg & memory via bd mode */
static int touch_point_sns_bd_write(struct touch_point_hub_data *pdata, u8 i2c_addr, u16 reg_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 *buf = kzalloc(len + 8, GFP_KERNEL);
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = len + 8,
		.buf = buf,
	};

	if (NULL == buf)
	    return -ENOMEM;

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject sns i2c bd write(i2c_addr:%#X ,reg_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			i2c_addr,
			reg_addr);
		if (NULL != buf)
			kfree(buf);
		return -EPERM;
	}

	buf[0] = i2c_addr;	//target byte for sns. It represents that this function is intend to communicate with sns's regs instead of hub' regs.
	buf[1] = 5 + len;	//wt_len;//1->reg_addr length, len->val buffer length
	buf[2] = 1;         //rd_len;//dummy value for write-only ops.
	buf[3] = 0x02;		//bd write
	buf[4] = (u8)(reg_addr >> 8);
	buf[5] = (u8)reg_addr;
	buf[6] = (u8)(len >> 8);/*max one byte for now, should be 0*/
	buf[7] = (u8)len;
	memcpy(&buf[8], val, len);

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	kfree(buf);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "sns(%#X) bd i2c write %#X failed!", i2c_addr, reg_addr);
		return -EIO;
    }

	return 0;
}

/* read sensor reg & memory via bd mode */
static int touch_point_sns_bd_read(struct touch_point_hub_data *pdata, u8 i2c_addr, u16 reg_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 wt_buf[] = {i2c_addr/*target byte for sns*/,
					5/*wt_len*/,
					len/*rd_len*/,
					0x01,/*bd read*/
					(u8)(reg_addr >> 8),
					(u8)reg_addr,
					(u8)(len >> 8),/*max one byte for now, should be 0*/
					(u8)len};
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(wt_buf),
			.buf = wt_buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		}
	};

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject sns i2c bd read(i2c_addr:%#X ,reg_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			i2c_addr,
			reg_addr);
		return -EPERM;
	}

	do {
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "sns(%#X) bd i2c read %#X failed!", i2c_addr, reg_addr);
		return -EIO;
    }

	return 0;
}

/* write sensor fd 1-byte reg via fd mode */
static int touch_point_sns_fd_reg_write(struct touch_point_hub_data *pdata, u8 i2c_addr, u8 reg_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 *buf = kzalloc(len + 4, GFP_KERNEL);
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = len + 4,
		.buf = buf,
	};

	if (NULL == buf)
	    return -ENOMEM;

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject sns fd reg i2c write(i2c_addr:%#X ,reg_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			i2c_addr,
			reg_addr);
		if (NULL != buf)
			kfree(buf);
		return -EPERM;
	}

	buf[0] = i2c_addr;	//target byte for sns. It represents that this function is intend to communicate with sns's regs instead of hub' regs.
	buf[1] = 1 + len;	//wt_len;//1->reg_addr length, len->val buffer length
	buf[2] = 1;         //rd_len;//dummy value for write-only ops.
	buf[3] = reg_addr;	//reg start addr;
	memcpy(&buf[4], val, len);

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	kfree(buf);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "sns(%#X) fd reg i2c write %#X failed!", i2c_addr, reg_addr);
		return -EIO;
    }

	return 0;
}

/* read sensor fd 1-byte reg via fd mode */
static int touch_point_sns_fd_reg_read(struct touch_point_hub_data *pdata, u8 i2c_addr, u8 reg_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 wt_buf[4] = {i2c_addr/*target byte for sns*/, 1/*wt_len*/, len/*rd_len*/, reg_addr};
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(wt_buf),
			.buf = wt_buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		}
	};

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject sns fd reg i2c read(i2c_addr:%#X ,reg_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			i2c_addr,
			reg_addr);
		return -EPERM;
	}

	do {
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "sns(%#X) fd reg i2c read %#X failed!", i2c_addr, reg_addr);
		return -EIO;
    }

	return 0;
}

/* write sensor memory(e.g. dsram) via fd mode */
static int touch_point_sns_mem_write(struct touch_point_hub_data *pdata, u8 i2c_addr, u16 mem_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 *buf = kzalloc(len + 8, GFP_KERNEL);
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = len + 8,
		.buf = buf,
	};

	if (NULL == buf)
	    return -ENOMEM;

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject sns mem i2c write(i2c_addr:%#X ,mem_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			i2c_addr,
			mem_addr);
		if (NULL != buf)
			kfree(buf);
		return -EPERM;
	}

	buf[0] = i2c_addr;	//target byte for sns. It represents that this function is intend to communicate with sns's regs instead of hub' regs.
	buf[1] = 5 + len;	//wt_len;//1->reg_addr length, len->val buffer length
	buf[2] = 1;         //rd_len;//dummy value for write-only ops.
	buf[3] = TOUCH_POINT_SNS_REG_ADDR_INDEX_H;
	buf[4] = (u8)(mem_addr >> 8);
	buf[5] = TOUCH_POINT_SNS_REG_ADDR_INDEX_L;
	buf[6] = (u8)mem_addr;
	buf[7] = TOUCH_POINT_SNS_REG_BURST_WRITE;
	memcpy(&buf[8], val, len);

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	kfree(buf);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "sns(%#X) mem i2c write %#X failed!", i2c_addr, mem_addr);
		return -EIO;
    }

	return 0;
}

/* read sensor memory(e.g. dsram) via fd mode */
static int touch_point_sns_mem_read(struct touch_point_hub_data *pdata, u8 i2c_addr, u16 mem_addr, u8 *val, u8 len)
{
	struct i2c_client *client = pdata->client;
	u8 wt_buf[] = {i2c_addr/*target byte for sns*/,
					5/*wt_len*/,
					len/*rd_len*/,
					TOUCH_POINT_SNS_REG_ADDR_INDEX_H,
					(u8)(mem_addr >> 8),
					TOUCH_POINT_SNS_REG_ADDR_INDEX_L,
					(u8)mem_addr,
					TOUCH_POINT_SNS_REG_BURST_READ};
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = sizeof(wt_buf),
			.buf = wt_buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		}
	};

	if (!pdata->init_done && !pdata->is_i2c_rw_just_after_hub_reset) {
		dev_warn(&pdata->client->dev, "hub init is ongoing(%d %d), reject sns mem i2c read(i2c_addr:%#X ,mem_addr:%#X)!",
			pdata->init_done,
			pdata->is_i2c_rw_just_after_hub_reset,
			i2c_addr,
			mem_addr);
		return -EPERM;
	}

	do {
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
		    break;
		i2c_retry_cnt++;
		mdelay(2);
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "sns(%#X) mem i2c read %#X failed!", i2c_addr, mem_addr);
		return -EIO;
    }

	return 0;
}

static int touch_point_hub_reg_write_mask(struct touch_point_hub_data *pdata, u8 reg, u8 mask, u8 val)
{
	int rc = 0;
	u8 orig = 0, tmp;

	rc = touch_point_hub_reg_read(pdata, reg, &orig, 1);
	if (rc != 0)
		return rc;

	tmp = orig & ~mask;
	tmp |= val & mask;

	rc = touch_point_hub_reg_write(pdata, reg, &tmp, 1);
	return rc;
}

static int touch_point_sns_bd_write_reg_mask(struct touch_point_hub_data *pdata, u8 i2c_addr, u16 reg, u8 mask, u8 val)
{
	int rc = 0;
	u8 reg_val = 0;

	rc += touch_point_sns_bd_read(pdata, i2c_addr, reg, &reg_val, 1);
	reg_val &= (~mask);
	reg_val |= (val & mask);
	rc += touch_point_sns_bd_write(pdata, i2c_addr, reg, &reg_val, 1);

	return rc;
}

static int touch_point_sns_fd_reg_write_mask(struct touch_point_hub_data *pdata, u8 i2c_addr, u8 reg, u8 mask, u8 val)
{
	int rc = 0;
	u8 orig = 0, tmp;

	rc = touch_point_sns_fd_reg_read(pdata, i2c_addr, reg, &orig, 1);
	if (rc != 0)
		return rc;

	tmp = orig & ~mask;
	tmp |= val & mask;

	rc = touch_point_sns_fd_reg_write(pdata, i2c_addr, reg, &tmp, 1);
	return rc;
}

static int touch_point_sns_mem_write_mask(struct touch_point_hub_data *pdata, u8 i2c_addr, u16 mem_addr, u8 mask, u8 val)
{
	int rc = 0;
	u8 orig = 0, tmp;

	rc = touch_point_sns_mem_read(pdata, i2c_addr, mem_addr, &orig, 1);
	if (rc != 0)
		return rc;

	tmp = orig & ~mask;
	tmp |= val & mask;

	rc = touch_point_sns_mem_write(pdata, i2c_addr, mem_addr, &tmp, 1);
	return rc;
}

/**********************************************************
 ******************** Basic functions ***********************
 **********************************************************/
static int touch_point_sns_reset(struct touch_point_hub_data *pdata)
{
	int rc = 0;

	if (NULL != pdata->gpiod_sns_rst) {
		dev_info(&pdata->client->dev, "reset sns...");
		if (pdata->sns_rst_high_active) {
			rc += gpiod_direction_output(pdata->gpiod_sns_rst, 1);
			mdelay(5);//TBD
			rc += gpiod_direction_output(pdata->gpiod_sns_rst, 0);
			//mdelay(50);//remove delay out of this function. Please don't add any delay here.
		} else {
			rc += gpiod_direction_output(pdata->gpiod_sns_rst, 0);
			mdelay(5);//TBD
			rc += gpiod_direction_output(pdata->gpiod_sns_rst, 1);
			//mdelay(50);//remove delay out of this function. Please don't add any delay here.
		}
	} else {
		dev_err(&pdata->client->dev, "sns reset pin is not configured in dts");
	}
	return rc;
}

static int touch_point_hub_reset(struct touch_point_hub_data *pdata)
{
	int rc = 0;

	if (NULL != pdata->gpiod_hub_rst) {
		dev_info(&pdata->client->dev, "reset hub...");
		if (pdata->hub_rst_high_active) {
			rc += gpiod_direction_output(pdata->gpiod_hub_rst, 1);
			mdelay(10);//TBD
			/* reset sns during hub reset pin is low */
			touch_point_sns_reset(pdata);
			rc += gpiod_direction_output(pdata->gpiod_hub_rst, 0);
			//mdelay(50);//remove delay out of this function. Please don't add any delay here.
		} else {
			rc += gpiod_direction_output(pdata->gpiod_hub_rst, 0);
			mdelay(10);//TBD
			/* reset sns during hub reset pin is low */
			touch_point_sns_reset(pdata);
			rc += gpiod_direction_output(pdata->gpiod_hub_rst, 1);
			//mdelay(50);//remove delay out of this function. Please don't add any delay here.
		}
	} else {
		dev_err(&pdata->client->dev, "hub reset pin is not configured in dts");
	}
	return rc;
}

/*
 * toggle hub wakeup pin
 */
static int touch_point_hub_wakeup(struct touch_point_hub_data *pdata)
{
	int rc = 0;
	ktime_t wakeup_ts_diff = ktime_set(0,0);
	ktime_t current_ts = ktime_get_boottime();

	wakeup_ts_diff = ktime_sub(current_ts, pdata->wakeup_ts);

	if (ktime_after(ms_to_ktime(TOUCH_POINT_HUB_WAKEUP_REJECTION_TIME_DURATION_MS), wakeup_ts_diff)) {
		dev_warn(&pdata->client->dev, "reject hub wakeup request(%lld < %d ms)!", ktime_to_ms(wakeup_ts_diff), TOUCH_POINT_HUB_WAKEUP_REJECTION_TIME_DURATION_MS);
		return 0;
	}

#if ENABLE_HUB_WAKEUP_BY_INT_PIN
	{
		#if OPERATE_IRQ_GPIO_LINE		
		struct gpio_chip	*chip;
		unsigned		offset;
		bool gpio_locked_as_irq = false;

		chip = gpiod_to_chip(pdata->gpiod_irq);
		#ifdef KERNEL_BELOW_3_19
		offset = desc_to_gpio(pdata->gpiod_irq);
		gpio_locked_as_irq = test_bit(FLAG_USED_AS_IRQ, &chip->desc[offset].flags);
		#else
		offset = gpio_chip_hwgpio(pdata->gpiod_irq);
		gpio_locked_as_irq = gpiochip_line_is_irq(chip, offset);
		#endif

		dev_info(&pdata->client->dev, "wakeup hub via intr pin");
		dev_info(&pdata->client->dev, "gpio_locked_as_irq %d", gpio_locked_as_irq);

		#ifdef KERNEL_BELOW_3_19
		if (gpio_locked_as_irq)
			gpio_unlock_as_irq(chip, offset);
		#else
		if (gpio_locked_as_irq)
			gpiochip_unlock_as_irq(chip, offset);
		#endif
		#endif

		//rc += gpiod_direction_output(pdata->gpiod_irq, 1);
		//mdelay(5);//TBD
		rc += gpiod_direction_output(pdata->gpiod_irq, 0);
		mdelay(5);//TBD
		rc += gpiod_direction_output(pdata->gpiod_irq, 1);
		rc += gpiod_direction_input(pdata->gpiod_irq);

		#if OPERATE_IRQ_GPIO_LINE
		#ifdef KERNEL_BELOW_3_19
		if (gpio_locked_as_irq)
			gpio_lock_as_irq(chip, offset);
		#else
		if (gpio_locked_as_irq)
			gpiochip_lock_as_irq(chip, offset);
		#endif
		#endif

		if (rc) {
			dev_err(&pdata->client->dev, "failed to operate intr pin(rc:%d)!", rc);
		}	
		mdelay(10);//TBD
	}
#elif ENABLE_HUB_WAKEUP_BY_I2C
	{
		uint8_t whoami = 0;

		dev_info(&pdata->client->dev, "wakeup hub via i2c");
		rc = touch_point_hub_reg_read(pdata, TOUCH_POINT_HUB_REG_WHOAMI, &whoami, sizeof(whoami));
		mdelay(10);//TBD
	}
#endif

	pdata->wakeup_ts = ktime_get_boottime();
	return 0;
}

/*
 * keep hub awake and wouldn't go to sleep automatically
 */
static int touch_point_hub_keep_awake(struct touch_point_hub_data *pdata, bool keep_awake)
{
	uint8_t value = keep_awake ? TOUCH_POINT_HUB_ALWAYS_ON_MASK : 0;

	return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_CONTROL, TOUCH_POINT_HUB_ALWAYS_ON_MASK, value);
}

/*
 * bypass hub algo
 */
static int touch_point_hub_bypass_algo(struct touch_point_hub_data *pdata, bool bypass_algo)
{
	uint8_t value = bypass_algo ? TOUCH_POINT_HUB_BYPASS_HUB_ALGO_MASK : 0;

	touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_CONTROL, TOUCH_POINT_HUB_BYPASS_HUB_ALGO_MASK, value);
	dev_info(&pdata->client->dev, "bypass algo(%d)", (uint8_t)bypass_algo);
	mdelay(10);//TBD
	return 0;
}

/*
 * set hub algo mode
 */
static int touch_point_hub_set_algo_mode(struct touch_point_hub_data *pdata, touch_point_hub_algo_mode_t mode)
{
	uint8_t value = 0;

	if (HUB_USP_ONLY_ALGO_MODE == mode) {
		value = 0x00;
	} else if (HUB_USP_ZFORCE_ALGO_MODE == mode) {
		value = 0x10;//HUB_USP_ZFORCE_ALGO_MODE
	} else if (HUB_USP_ZFORCE_ALGO_ON_SENSOR_MODE == mode) {
		value = 0x20;
	}

	return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_CONTROL, TOUCH_POINT_HUB_ALGO_MODE_MASK, value);
}

/*
 * notify hub that AP will enter poweroff
 */
static int touch_point_hub_notify_ap_poweroff(struct touch_point_hub_data *pdata)
{
	return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_CONTROL2, TOUCH_POINT_HUB_AP_POWEROFF_MASK, TOUCH_POINT_HUB_AP_POWEROFF_MASK);
}

/*
 * send key mapping in dts to hub
 */
static int touch_point_hub_send_key_map(struct touch_point_hub_data *pdata, uint8_t sns_idx)
{
	uint8_t hub_key_code = touch_point_hub_get_hub_key_code(pdata->sns[sns_idx].input_ev_code);

	dev_info(&pdata->client->dev, "send key map to hub(sns%d:%#X -> %s(%d))",
			sns_idx,
			pdata->sns[sns_idx].i2c_addr,
			touch_point_hub_get_key_name(pdata->sns[sns_idx].input_ev_code),
			hub_key_code);

	switch (pdata->sns[sns_idx].i2c_addr) {
	case 0x27:
		return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_HUB_KEY_MAP, TOUCH_POINT_HUB_0X27_KEY_CODE_MASK, hub_key_code);
	case 0x2F:
		return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_HUB_KEY_MAP, TOUCH_POINT_HUB_0X2F_KEY_CODE_MASK, hub_key_code << 2);
	case 0x37:
		return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_HUB_KEY_MAP, TOUCH_POINT_HUB_0X37_KEY_CODE_MASK, hub_key_code << 4);
	case 0x3F:
		return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_HUB_KEY_MAP, TOUCH_POINT_HUB_0X3F_KEY_CODE_MASK, hub_key_code << 6);
	default:
		return -1;
	}
}

/*
 * send haptic ON/OFF setting in dts to hub
 */
static int touch_point_hub_send_haptic_setting(struct touch_point_hub_data *pdata)
{
	if (MAX_SENSORS_NUM >= 4) {
		uint8_t en_hapic = (((!!pdata->en_haptic[3]) << 3) | ((!!pdata->en_haptic[2]) << 2) | ((!!pdata->en_haptic[1]) << 1) | (!!pdata->en_haptic[0]));

		dev_info(&pdata->client->dev, "send haptic setting to hub(%#X)", en_hapic);

		return touch_point_hub_reg_write_mask(pdata, TOUCH_POINT_HUB_REG_HUB_EN_HAPTIC, TOUCH_POINT_HUB_EN_HAPTIC_MASK, en_hapic);
	}
	return -1;
}

/*
 * send board rev info to hub
 */
static int touch_point_hub_send_board_rev(struct touch_point_hub_data *pdata)
{
	uint8_t board_rev = (uint8_t)pdata->board_rev;

	dev_info(&pdata->client->dev, "send board rev to hub(%d)", board_rev);
	return touch_point_hub_reg_write(pdata, TOUCH_POINT_HUB_REG_BOARD_REV, &board_rev, sizeof(board_rev));
}

/*
 * Once hub get restarted, hub fw will set flag=0.
 * Once this kernel driver bootup, send flag=1 to differentiate from initial value(flag=0).
 * If monitor work detected flag!=1, it means some issue occurs on hub side, most likely MCU get restarted somehow,
 * AP trigger recovery process in this case.
 */
static bool touch_point_hub_is_hub_get_restarted(struct touch_point_hub_data *pdata)
{
	uint8_t hub_restart_indicator = 0;

	touch_point_hub_reg_read(pdata, TOUCH_POINT_HUB_REG_RESET_INDICATOR, &hub_restart_indicator, sizeof(hub_restart_indicator));
	dev_info(&pdata->client->dev, "get hub restart indicator(%d)", hub_restart_indicator);
	if (hub_restart_indicator != 1) {
		return true;
	} else {
		return false;
	}
}

static int touch_point_hub_set_hub_restart_indicator(struct touch_point_hub_data *pdata)
{
	uint8_t hub_restart_indicator = 1;

	dev_info(&pdata->client->dev, "set hub restart indicator(%d)", hub_restart_indicator);
	return touch_point_hub_reg_write(pdata, TOUCH_POINT_HUB_REG_RESET_INDICATOR, &hub_restart_indicator, sizeof(hub_restart_indicator));
}

/*
 * toggle sensor's internal int0 for hpm
 */
static int touch_point_sns_toggle_int0(struct touch_point_hub_data *pdata, uint8_t sns_idx)
{
	int rc = 0;
	ktime_t toggle_ts_diff = ktime_set(0,0);
	ktime_t current_ts = ktime_get_boottime();

	if (sns_idx >= pdata->sns_cnt) {
		dev_err(&pdata->client->dev, "sns_idx is invalid(%d >= %d)!", sns_idx, pdata->sns_cnt);
		return -1;
	}

	toggle_ts_diff = ktime_sub(current_ts, pdata->sns[sns_idx].toggle_ts);

	if (ktime_after(ms_to_ktime(TOUCH_POINT_SNS_WAKEUP_REJECTION_TIME_DURATION_MS), toggle_ts_diff)) {
		dev_warn(&pdata->client->dev, "reject sns%d[%s] wakeup request(%lld < %d ms)!",
			sns_idx,
			pdata->sns[sns_idx].name,
			ktime_to_ms(toggle_ts_diff),
			TOUCH_POINT_HUB_WAKEUP_REJECTION_TIME_DURATION_MS);
		return 0;
	}

	{
		//use reg 0x51 BIT1 to wakeup chip.
		uint8_t sns_i2c_addr = pdata->sns[sns_idx].i2c_addr;
		uint8_t sc_byte = 0;
		uint8_t sc_bit1_high = 0;
		uint8_t sc_bit1_low = 0;

		dev_info(&pdata->client->dev, "toggle sns%d[%s] internal int0", sns_idx, pdata->sns[sns_idx].name);

		rc += touch_point_sns_fd_reg_read(pdata, sns_i2c_addr, TOUCH_POINT_SNS_REG_FD_SC, &sc_byte, 1);
		sc_bit1_high = sc_byte | TOUCH_POINT_SNS_FD_HOST_WAKEUP_CHIP_BIT_MASK;
		sc_bit1_low = sc_byte & (~TOUCH_POINT_SNS_FD_HOST_WAKEUP_CHIP_BIT_MASK);

		//bit 1: RTL looks for negative transition and pulls sensor mcu int0 low for ~5 cycles
		//1st toggle: hight->low
		rc += touch_point_sns_fd_reg_write(pdata, sns_i2c_addr, TOUCH_POINT_SNS_REG_FD_SC, &sc_bit1_high, 1);
		udelay(200);//50
		rc += touch_point_sns_fd_reg_write(pdata, sns_i2c_addr, TOUCH_POINT_SNS_REG_FD_SC, &sc_bit1_low, 1);
		udelay(200);//50

		//wait some time in case fw is close to enter LPM and not able to check sensor control bit3
		mdelay(10);//3//TODO: could reduce it in fd-based fw.

		//2nd toggle: high->low
		rc += touch_point_sns_fd_reg_write(pdata, sns_i2c_addr, TOUCH_POINT_SNS_REG_FD_SC, &sc_bit1_high, 1);
		udelay(200);//50
		rc += touch_point_sns_fd_reg_write(pdata, sns_i2c_addr, TOUCH_POINT_SNS_REG_FD_SC, &sc_bit1_low, 1);
		udelay(200);//50

		//ensure fw go back to 20MHz
		mdelay(20);//10//3//TODO: could reduce it in fd-based fw.
	}

	pdata->sns[sns_idx].toggle_ts = ktime_get_boottime();
	return 0;
}

/* go through all sensors to read sensor whoami */
static int touch_point_sns_check_whoami(struct touch_point_hub_data *pdata)
{
	int i = 0, ret = 0;

	for (i = 0; i < pdata->sns_cnt; i++) {
		int rc = 0;
		u8 whoami = 0;
		u8 i2c_addr = pdata->sns[i].i2c_addr;

		rc = touch_point_sns_fd_reg_read(pdata, i2c_addr, TOUCH_POINT_SNS_REG_WHOAMI, &whoami, sizeof(whoami));
		if (TOUCH_POINT_SNS_A0_WHOAMI_VALUE == whoami || TOUCH_POINT_SNS_A2_WHOAMI_VALUE == whoami) {
			pdata->sns[i].whoami = whoami;
			dev_info(&pdata->client->dev, "sns%d[%s](%#X) read sns whoami successfully(whoami:%#X)", i, pdata->sns[i].name, i2c_addr, whoami);
		} else {
			ret = -1;//return -1 as long as one of sensors failed to get whoami.
			dev_err(&pdata->client->dev, "sns%d[%s](%#X) read sns whoami failed(rc:%d, whoami:%#X)", i, pdata->sns[i].name, i2c_addr, rc, whoami);
		}
	}

	return ret;
}

static int touch_point_sns_put_to_hold(struct touch_point_hub_data *pdata, uint8_t sns_idx, bool en)
{
	int rc = 0;
	#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
	rc = touch_point_sns_fd_reg_write_mask(pdata, pdata->sns[sns_idx].i2c_addr, TOUCH_POINT_SNS_REG_FD_HOLD_FW,
		TOUCH_POINT_SNS_REG_FD_HOLD_FW_BIT_MASK, en ? TOUCH_POINT_SNS_REG_FD_HOLD_FW_BIT_MASK : 0);
	#else
	rc = touch_point_sns_mem_write_mask(pdata, pdata->sns[sns_idx].i2c_addr, TOUCH_POINT_SNS_SENSOR_CTRL,
		TOUCH_POINT_SNS_REG_SENSOR_CTRL_HOLDING_MASK, en ? TOUCH_POINT_SNS_REG_SENSOR_CTRL_HOLDING_MASK : 0);
	#endif

	if (en)
		mdelay(80);
	return rc;
}

static int touch_point_sns_exit_bd_enter_fd(struct touch_point_hub_data *pdata, uint8_t sns_idx)
{
	return touch_point_sns_bd_write_reg_mask(pdata, pdata->sns[sns_idx].i2c_addr, TOUCH_POINT_SNS_FORCE_RUNNING_RST_REG, 0x22, 0x02);
}

static int touch_point_sns_exit_fd_enter_bd(struct touch_point_hub_data *pdata, uint8_t sns_idx)
{
	return touch_point_sns_mem_write_mask(pdata, pdata->sns[sns_idx].i2c_addr, TOUCH_POINT_SNS_FORCE_RUNNING_RST_REG, 0x22, 0x20);
}

/*
 * Dump OTP memory, only be called under bd i2c.
 */
static int touch_point_sns_bd_dump_otp_mem(struct touch_point_hub_data *pdata, uint8_t sns_idx)
{
	int rc = 0, offset = 0, size = TOUCH_POINT_SNS_OTP_MEM_SIZE;
	touch_point_sns_t *sns = &pdata->sns[sns_idx];

	if (NULL == sns->otp_mem)
	{
		dev_warn(&pdata->client->dev, "[sns%d[%s]]otp memory is NULL", sns_idx, sns->name);
		return 0;//Not return error code.
	}

	/* read otp mem */
	while (offset < size)
	{
		if ((size - offset) > FW_DL_I2C_CHUNK_SIZE)
		{
			rc = touch_point_sns_bd_read(pdata, sns->i2c_addr, TOUCH_POINT_SNS_OTP_MEM_START_ADDR+offset, (sns->otp_mem+offset), FW_DL_I2C_CHUNK_SIZE);
			if (rc < 0)
				dev_err(&pdata->client->dev, "otp read failed(addr:%#X)", TOUCH_POINT_SNS_OTP_MEM_START_ADDR+offset);
			offset += FW_DL_I2C_CHUNK_SIZE;
		}
		else
		{
			rc += touch_point_sns_bd_read(pdata, sns->i2c_addr, TOUCH_POINT_SNS_OTP_MEM_START_ADDR+offset, (sns->otp_mem+offset), (size-offset));
			if (rc < 0)
				dev_err(&pdata->client->dev, "otp read failed(addr:%#X)", TOUCH_POINT_SNS_OTP_MEM_START_ADDR+offset);
			offset = size;
		}
	}
	return rc;
}

static int touch_point_sns_hpm_odr_sel(struct touch_point_hub_data *pdata, uint8_t sns_idx, enum touch_point_sns_hpm_odr odr)
{
	touch_point_sns_t *sns = &pdata->sns[sns_idx];
	uint8_t data[2] = {(uint8_t)((odr & 0xff00) >> 8), (uint8_t)(odr & 0x00ff)};

#if ENABLE_SNS_DSRAM_SINGLE_BYTE_RW
	{
		int rc = 0, i = 0;

		for (i = 0; i < ARRAY_SIZE(data); i++) {
			rc += touch_point_sns_mem_write(pdata, sns->i2c_addr, TOUCH_POINT_SNS_HPM_ODR_SEL + i, &data[i], 1);
		}
		return rc;
	}
#else
	return touch_point_sns_mem_write(pdata, sns->i2c_addr, TOUCH_POINT_SNS_HPM_ODR_SEL, data, ARRAY_SIZE(data));
#endif
}

static int touch_point_sns_lpm_odr_sel(struct touch_point_hub_data *pdata, uint8_t sns_idx, enum touch_point_sns_lpm_odr odr)
{
	touch_point_sns_t *sns = &pdata->sns[sns_idx];
	uint8_t data[2] = {(uint8_t)((odr & 0xff00) >> 8), (uint8_t)(odr & 0x00ff)};

#if ENABLE_SNS_DSRAM_SINGLE_BYTE_RW
	{
		int rc = 0, i = 0;

		for (i = 0; i < ARRAY_SIZE(data); i++) {
			rc += touch_point_sns_mem_write(pdata, sns->i2c_addr, TOUCH_POINT_SNS_LPM_ODR_SEL + i, &data[i], 1);
		}
		return rc;
	}
#else
	return touch_point_sns_mem_write(pdata, sns->i2c_addr, TOUCH_POINT_SNS_LPM_ODR_SEL, data, ARRAY_SIZE(data));
#endif
}

static int touch_point_sns_enable_lpm(struct touch_point_hub_data *pdata, uint8_t sns_idx, bool en)
{
	touch_point_sns_t *sns = &pdata->sns[sns_idx];
	int rc = 0;

	rc += touch_point_sns_mem_write_mask(pdata, sns->i2c_addr, TOUCH_POINT_SNS_SENSOR_CTRL,
		TOUCH_POINT_SNS_REG_SENSOR_CTRL_LPM_MASK, en ? TOUCH_POINT_SNS_REG_SENSOR_CTRL_LPM_MASK : 0);//0x00:hpm, 0x08:lpm

	return rc;
}

static int touch_point_sns_get_fw_ver(struct touch_point_hub_data *pdata, uint8_t sns_idx)
{
	int rc = 0;
	uint16_t ver_addr;
	uint8_t buf[2];
	uint8_t ver_date[TOUCH_POINT_SNS_FW_VER_DATE_BYTES];
	uint8_t ver_time[TOUCH_POINT_SNS_FW_VER_TIME_BYTES];

	memset(buf, 0, sizeof(buf));
	memset(ver_date, 0, sizeof(ver_date));
	memset(ver_time, 0, sizeof(ver_time));

#if ENABLE_SNS_DSRAM_SINGLE_BYTE_RW
	{
		int i = 0;

		for (i = 0; i < ARRAY_SIZE(buf); i++) {
			rc += touch_point_sns_mem_read(pdata, pdata->sns[sns_idx].i2c_addr, TOUCH_POINT_SNS_SWREG_VERBYTE1_REVC + i, &buf[i], 1);
		}
	}
#else
	rc = touch_point_sns_mem_read(pdata, pdata->sns[sns_idx].i2c_addr, TOUCH_POINT_SNS_SWREG_VERBYTE1_REVC, buf, ARRAY_SIZE(buf));
#endif
	if (rc < 0) {
		dev_err(&pdata->client->dev, "fd read fw ver addr failed");
		return rc;
	}
	ver_addr = (buf[0] << 8 | buf[1]);

#if ENABLE_SNS_DSRAM_SINGLE_BYTE_RW
	{
		int i = 0;

		rc = 0;
		for (i = 0; i < ARRAY_SIZE(ver_date); i++) {
			rc += touch_point_sns_mem_read(pdata, pdata->sns[sns_idx].i2c_addr, ver_addr + i, &ver_date[i], 1);
		}
	}
#else
	rc = touch_point_sns_mem_read(pdata, pdata->sns[sns_idx].i2c_addr, ver_addr, ver_date, ARRAY_SIZE(ver_date));
#endif
	if (rc < 0) {
		dev_err(&pdata->client->dev, "read sns%d[%s] fw ver date failed", sns_idx, pdata->sns[sns_idx].name);
		return rc;
	}

#if ENABLE_SNS_DSRAM_SINGLE_BYTE_RW
	{
		int i = 0;

		rc = 0;
		for (i = 0; i < ARRAY_SIZE(ver_time); i++) {
			rc += touch_point_sns_mem_read(pdata, pdata->sns[sns_idx].i2c_addr, ver_addr + sizeof(ver_date) + i, &ver_time[i], 1);
		}
	}
#else
	rc = touch_point_sns_mem_read(pdata, pdata->sns[sns_idx].i2c_addr, ver_addr + sizeof(ver_date), ver_time, ARRAY_SIZE(ver_time));
#endif
	if (rc < 0) {
		dev_err(&pdata->client->dev, "read sns%d[%s] fw ver time failed", sns_idx, pdata->sns[sns_idx].name);
		return rc;
	}

	memcpy(pdata->sns[sns_idx].ver_date, ver_date, ARRAY_SIZE(ver_date));
	memcpy(pdata->sns[sns_idx].ver_time, ver_time, ARRAY_SIZE(ver_time));
	pdata->sns[sns_idx].ver_date[TOUCH_POINT_SNS_FW_VER_DATE_BYTES - 1] = '\0';
	pdata->sns[sns_idx].ver_time[TOUCH_POINT_SNS_FW_VER_TIME_BYTES - 1] = '\0';
	dev_info(&pdata->client->dev, "sns%d[%s] fw ver:%s %s", sns_idx, pdata->sns[sns_idx].name, pdata->sns[sns_idx].ver_date, pdata->sns[sns_idx].ver_time);
	return rc;
}

/**********************************************************
 ******************** Advanced functions ******************
 **********************************************************/
#ifdef KERNEL_BELOW_3_19
//DB820 kernel 3.18 doesn't implement this func, driver add it here by refering to SDM845 kernel 4.9 device.h
//Another way to fix build error on 3.18 is to use devm_add_action() to replace devm_add_action_or_reset()
static inline int devm_add_action_or_reset(struct device *dev,
					   void (*action)(void *), void *data)
{
	int ret;

	ret = devm_add_action(dev, action, data);
	if (ret)
		action(data);

	return ret;
}
#endif

/* add++ for bootloader access */
/* enter bootloader mode */
static int touch_point_hub_bl_enter_bl_mode(struct touch_point_hub_data *pdata)
{
	uint16_t delay_time_ms = 0;
	uint8_t cmd_buf[] = {0x01, 0x00, 0x08};
	uint8_t response[1] = {0};
	int rc = 0;

	do {
		touch_point_hub_reset(pdata);
		if (delay_time_ms > 0) {
			mdelay(delay_time_ms);
		}
		//mdelay(pdata->rst_convert_time_ms + 5);//5ms Bootloader start time, ~14ms for signature phone's LDO circuit conversion time.

		touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
		touch_point_hub_bl_receive(pdata, response, sizeof(response));

		if (response[0] == 0xAA) {
			rc = 0;
			dev_info(&pdata->client->dev, "[hub bl] enter bl mode successfully(delay:%d ms)", delay_time_ms);
		} else {
			rc = 0xFF;
			dev_err(&pdata->client->dev, "[hub bl] enter bl mode failed(%#X, delay:%d ms)", response[0], delay_time_ms);
		}
		delay_time_ms += 2;
	} while (rc != 0 && delay_time_ms < 20);

	return rc;
}

/* check current mode (bootloader mode or application mode) */
static int touch_point_hub_bl_check_current_mode(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0x02, 0x00};
	uint8_t response[2] = {0,0};
	int rc = 0xFF;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if ((response[0] == 0xAA) && (response[1] == 0x08/*bootloader mode*/)) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] current mode:%#X", response[1]);
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] read mode failed(%#X %#X)", response[0], response[1]);
	}
	return rc;
}

#if 0
/* enable crc in BL config */
static int touch_point_hub_bl_enable_crc(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0x82, 0x01, 0x08, 0x01};
	uint8_t response[1] = {0};
	int rc = 0xFF;

	/* enable crc */
	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if (response[0] == 0xAA) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] enable crc successfully");
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] enable crc failed(%#X)", response[0]);
		return rc;
	}

	/* save bl config */
	cmd_buf[0] = 0x82;
	cmd_buf[1] = 0x00;
	response[0] = 0;
	touch_point_hub_bl_send(pdata, cmd_buf, 2);
	/*
	 * Need to add a Maxim recommended 150ms delay after sending SaveBootConfig()
	 * command else other flash operations fail.
	 */
	msleep(150);
	touch_point_hub_bl_receive(pdata, response, sizeof(response));
	if (response[0] == 0xAA) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] save bl config successfully");
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] save bl config failed(%#X)", response[0]);
	}

	return rc;
}
#endif

/* check device id */
static int touch_point_hub_bl_check_device_id(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0xFF, 0x00};
	uint8_t response[2] = {0,0};
	int rc = 0xFF;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if ((response[0] == 0xAA) && (response[1] == 0x01)) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] hub device id:%#X", response[1]);
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] read hub device id failed(%#X %#X)", response[0], response[1]);
	}
	return rc;
}

/* read bootloader version */
static int touch_point_hub_bl_read_version(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0x81, 0x00};
	uint8_t response[4] = {0,0,0,0};
	int rc = 0xFF;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if (response[0] == 0xAA) {
		rc = 0;
		scnprintf(pdata->bl_version, MAX_HUB_VERSION_LEN, "%d.%d.%d",
			response[1], response[2], response[3]);
		pdata->bl_version[MAX_HUB_VERSION_LEN - 1] = '\0';
		dev_info(&pdata->client->dev, "[hub bl] current bl ver:%s", pdata->bl_version);
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] read bl ver failed(%#X %d %d %d)",
			response[0], response[1], response[2], response[3]);
	}
	return rc;
}

/* read page size */
static int touch_point_hub_bl_read_page_size(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0x81, 0x01};
	uint8_t response[3] = {0,0,0};
	int rc = 0xFF;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if (response[0] == 0xAA) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] read page size:%#X", ((response[1] << 8) | response[2]));
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] read page size failed(%#X %#X %#X)", response[0], response[1], response[2]);
	}
	return rc;
}

/* write page count */
static int touch_point_hub_bl_write_page_count(struct touch_point_hub_data *pdata, uint8_t page_cnt)
{
	uint8_t cmd_buf[] = {0x80, 0x02, 0x00, page_cnt};
	uint8_t response[1] = {0};
	int rc = 0xFF;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if (response[0] == 0xAA) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] write page cnt:%#X", page_cnt);
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] write page cnt failed(%#X)", response[0]);
	}
	return rc;
}

/* send cmd to erase app memory */
static int touch_point_hub_bl_erase_app_memory(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0x80, 0x03};//erase all application flash
	uint8_t response[1] = {0};
	int rc = 0;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	//Maxim recommends 700ms wait as it may take that long for erase operation
	msleep(700);
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if (response[0] == 0xAA) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] erase flash done");
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] erase flash failed(%#X)", response[0]);
	}
	return rc;
}

#if 0
/* send cmd to erase page */
static int touch_point_hub_bl_erase_page(struct touch_point_hub_data *pdata, uint8_t page_number)
{
	uint8_t cmd_buf[] = {0x80, 0x05, 0x00, page_number};//erase one page specified by page_number, app memory page starts from 2.
	int rc = 0;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	msleep(100);//TBD

	dev_info(&pdata->client->dev, "[hub bl] erase page%d done", page_number);
	return rc;
}
#endif

/* set chunk size */
static int touch_point_hub_bl_set_chunk_size(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0x80, 0x06, 0x00, TOUCH_POINT_HUB_BL_MAX_TRANSMIT_CHUNK_SIZE_IN_BYTE};
	uint8_t response[1] = {0};
	int rc = 0xFF;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if (response[0] == 0xAA) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] set partial chunk size:%d bytes", TOUCH_POINT_HUB_BL_MAX_TRANSMIT_CHUNK_SIZE_IN_BYTE);
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] set partial chunk size failed(%#X)", response[0]);
	}
	return rc;
}

/* exit bootloader mode */
static int touch_point_hub_bl_exit_bl_mode(struct touch_point_hub_data *pdata)
{
	uint8_t cmd_buf[] = {0x01, 0x00, 0x00};
	uint8_t response[1] = {0};
	int rc = 0xFF;

	touch_point_hub_bl_send(pdata, cmd_buf, sizeof(cmd_buf));
	touch_point_hub_bl_receive(pdata, response, sizeof(response));

	if (response[0] == 0xAA) {
		rc = 0;
		dev_info(&pdata->client->dev, "[hub bl] exit bl mode successfully");
	} else {
		rc = 0xFF;
		dev_err(&pdata->client->dev, "[hub bl] exit bl mode failed(%#X)", response[0]);
	}
	return rc;
}
/* add-- for bootloader access */

static void touch_point_hub_dereg_misc_dev(void *misc_dev)
{
	misc_deregister(misc_dev);
}

static irqreturn_t touch_point_hub_irq_handler(int irq, void *dev)
{
	struct touch_point_hub_data *pdata = dev;

	dev_info(&pdata->client->dev, "hub irq fired.");

	if (pdata->wakeup)
		pm_stay_awake(&pdata->client->dev);

	if(NULL != pdata->touch_point_hub_wq)
		queue_delayed_work(pdata->touch_point_hub_wq, &pdata->delaywork, pdata->wakeup ? msecs_to_jiffies(10) : 0);

	return IRQ_HANDLED;
}

static void touch_point_hub_monitor_work(struct work_struct *work)
{
	struct touch_point_hub_data *pdata =
		container_of(work, struct touch_point_hub_data, monitor_work.work);
	uint8_t hub_whoami_value = 0;

	/* wakeup hub */
	touch_point_hub_wakeup(pdata);
	touch_point_hub_reg_read(pdata, TOUCH_POINT_HUB_REG_WHOAMI, &hub_whoami_value, sizeof(hub_whoami_value));
	dev_info(&pdata->client->dev, "[monitor work] hub whoami %#X", hub_whoami_value);
	if ((hub_whoami_value != TOUCH_POINT_HUB_WHOAMI_VALUE) || (touch_point_sns_check_whoami(pdata) < 0) || touch_point_hub_is_hub_get_restarted(pdata)) {
		dev_info(&pdata->client->dev, "[monitor work] recover hub!");
		touch_point_hub_recovery(pdata);
	}

	schedule_delayed_work(&pdata->monitor_work, msecs_to_jiffies(TOUCH_POINT_HUB_MONITOR_TIMER_PERIOD_MS));

	if (pdata->dev_get_resumed && pdata->wakeup) {
		pm_relax(&pdata->client->dev);
		pdata->dev_get_resumed = false;
	}
}

static int touch_point_hub_init(struct touch_point_hub_data *pdata)
{
	int i = 0, rc = 0;

	/* wakeup hub */
	touch_point_hub_wakeup(pdata);
	/* keep hub awake */
	touch_point_hub_keep_awake(pdata, true);
	touch_point_hub_bypass_algo(pdata, true);
	#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
	for (i = 0; i < pdata->sns_cnt; i++) {
		touch_point_sns_put_to_hold(pdata, i, true);
	}
	#endif
	for (i = 0; i < pdata->sns_cnt; i++) {
		/* switch sns to hpm */
		touch_point_sns_toggle_int0(pdata, i);
		touch_point_sns_enable_lpm(pdata, i, false);
	}

	for (i = 0; i < pdata->sns_cnt; i++) {
		/* read sns fw ver */
		touch_point_sns_get_fw_ver(pdata, i);
		/* set sns lpm 100hz odr */
		touch_point_sns_lpm_odr_sel(pdata, i, LPM_ODR_75HZ);//LPM_ODR_100HZ
	}

	/* set hub algo mode */
	#if 0
	#if !ENABLE_SNS_FW_HOLD_VIA_FD_REG
	for (i = 0; i < pdata->sns_cnt; i++) {
		touch_point_sns_put_to_hold(pdata, i, true);
	}
	#endif
	switch (pdata->chip_mode_in_dts) {
	case 0:
		touch_point_hub_set_algo_mode(pdata, HUB_USP_ZFORCE_ALGO_ON_SENSOR_MODE);
		break;
	case 1:
		touch_point_hub_set_algo_mode(pdata, HUB_USP_ONLY_ALGO_MODE);
		break;
	case 3:
		touch_point_hub_set_algo_mode(pdata, HUB_USP_ZFORCE_ALGO_MODE);
		break;
	default:
		touch_point_hub_set_algo_mode(pdata, HUB_USP_ZFORCE_ALGO_MODE);
		break;
	}
	#if !ENABLE_SNS_FW_HOLD_VIA_FD_REG
	for (i = 0; i < pdata->sns_cnt; i++) {
		touch_point_sns_put_to_hold(pdata, i, false);
	}
	#endif
	dev_info(&pdata->client->dev, "set chip mode:%d[%s]", pdata->chip_mode_in_dts, touch_point_hub_get_chip_mode_string(pdata->chip_mode_in_dts));
	#endif

	if (!pdata->is_otp_dumped) {
		/* read otp mem */
		dev_info(&pdata->client->dev, "dump otp memory...");
		//rc = touch_point_hub_bypass_algo(pdata, true);
		for (i = 0; i < pdata->sns_cnt; i++) {
			touch_point_sns_t *sns = &pdata->sns[i];

			sns->otp_mem = devm_kzalloc(&pdata->client->dev, TOUCH_POINT_SNS_OTP_MEM_SIZE, GFP_KERNEL);
			if (NULL == sns->otp_mem) {
				dev_warn(&pdata->client->dev, "no memory for sns%d[%s] otp buffer!", i, sns->name);
				continue;
			}

			#if !ENABLE_SNS_FW_HOLD_VIA_FD_REG
			rc += touch_point_sns_put_to_hold(pdata, i, true);
			#endif
			rc += touch_point_sns_exit_fd_enter_bd(pdata, i);
			{
				uint8_t bd_whoami = 0;
				touch_point_sns_bd_read(pdata, sns->i2c_addr, TOUCH_POINT_SNS_BD_WHOAMI_REG, &bd_whoami, sizeof(bd_whoami));
				dev_info(&pdata->client->dev, "sns%d[%s] bd whoami:%#X", i, sns->name, bd_whoami);
			}
			rc += touch_point_sns_bd_dump_otp_mem(pdata, i);
			if (NULL != sns->otp_mem) {
				sns->si_rev = sns->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_SIREV_NEW_ADDR)];
				dev_info(&pdata->client->dev, "sns%d[%s] otp si rev(%#X=%#X)", i, sns->name, TOUCH_POINT_SNS_OTP_SIREV_NEW_ADDR, sns->si_rev);
			}
			rc += touch_point_sns_exit_bd_enter_fd(pdata, i);
			{
				uint8_t fd_whoami = 0;
				touch_point_sns_fd_reg_read(pdata, sns->i2c_addr, TOUCH_POINT_SNS_REG_WHOAMI, &fd_whoami, sizeof(fd_whoami));
				dev_info(&pdata->client->dev, "sns%d[%s] fd whoami:%#X", i, sns->name, fd_whoami);
			}
			#if !ENABLE_SNS_FW_HOLD_VIA_FD_REG
			rc += touch_point_sns_put_to_hold(pdata, i, false);
			#endif
		}
		//rc += touch_point_hub_bypass_algo(pdata, false);
		dev_info(&pdata->client->dev, "dump otp memory done, rc:%d", rc);
		pdata->is_otp_dumped = true;
	}

	/* switch sns to lpm */
	for (i = 0; i < pdata->sns_cnt; i++) {
		touch_point_sns_enable_lpm(pdata, i, true);
	}
	#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
	for (i = 0; i < pdata->sns_cnt; i++) {
		touch_point_sns_put_to_hold(pdata, i, false);
	}
	#endif
	touch_point_hub_bypass_algo(pdata, false);
	/* set hub to be able to auto enter sleep */
	touch_point_hub_keep_awake(pdata, false);

	if (!pdata->init_once) {
		/* set input capability */
		for (i = 0; i < pdata->sns_cnt; i++) {
			input_set_capability(pdata->input_dev, EV_KEY, pdata->sns[i].input_ev_code);
		}
		input_set_capability(pdata->input_dev, EV_KEY, KEY_SCROLLUP);
		input_set_capability(pdata->input_dev, EV_KEY, KEY_SCROLLDOWN);

		#if 1//moved from probe
		/*
		rc = misc_register(&pdata->misc_dev);
		if (rc) {
			dev_err(&pdata->client->dev, "register misc dev failed");
		}
		dev_info(&pdata->client->dev, "miscdev minor:%d", pdata->misc_dev.minor);
		
		rc = devm_add_action_or_reset(&pdata->client->dev, touch_point_hub_dereg_misc_dev, &pdata->misc_dev);
		if (rc) {
			dev_err(&pdata->client->dev, "failed to add action for dereg misc dev");
		}
		*/
		
		/* register irq */
		rc	= devm_request_irq(&pdata->client->dev, pdata->client->irq, touch_point_hub_irq_handler,
								0, pdata->client->name, pdata);
		if (rc < 0) {
			dev_err(&pdata->client->dev, "request irq failed");
		}
		#endif

		disable_irq(pdata->client->irq);

		/* start periodic monitor */
		schedule_delayed_work(&pdata->monitor_work, msecs_to_jiffies(TOUCH_POINT_HUB_MONITOR_TIMER_PERIOD_MS));

		/* if no error, set flag to ensure this code block is only executed once */
		pdata->init_once = true;
	}
	enable_irq(pdata->client->irq);
	return 0;
}

/* download hub fw via bootloader */
static int touch_point_hub_fw_download(struct touch_point_hub_data *pdata, const u8 *fw_buf, u32 fw_size)
{
#define MAX_TRANSMIT_CHUNK_SIZE TOUCH_POINT_HUB_BL_MAX_TRANSMIT_CHUNK_SIZE_IN_BYTE//128 bytes per fw buf transaction
#define BL_PAGE_SIZE	(8192 + 16) //8K page + 4-byte CRC32 + 12 bytes of 0x00

	uint8_t page_cnt = 0;
	uint32_t offset = TOUCH_POINT_HUB_BL_PAGE_START_ADDR_IN_MSBL_FILE;//send pages starting from 0x4C
	uint32_t sz = 0, bl_num_of_pages = fw_size / BL_PAGE_SIZE + 1;
	u8 *bl_page_buf = kzalloc(MAX_TRANSMIT_CHUNK_SIZE + 2, GFP_KERNEL);
	u8 ic_fw_ver[3] = {0, 0, 0};
	int i= 0, rc = 0;

	if (NULL == bl_page_buf) {
		rc = -1;		
		dev_err(&pdata->client->dev, "[hub bl] no mem for bl chunk buf(size:%d bytes)", MAX_TRANSMIT_CHUNK_SIZE + 2);
		return rc;
	}

	/* give permission to rw i2c */
	pdata->is_i2c_rw_just_after_hub_reset = true;
	/* wakeup hub */
	touch_point_hub_wakeup(pdata);
	touch_point_hub_reg_read(pdata, TOUCH_POINT_HUB_REG_VER_MAJOR, ic_fw_ver, sizeof(ic_fw_ver));
	pdata->is_i2c_rw_just_after_hub_reset = false;
	if ((ic_fw_ver[0] == fw_buf[TOUCH_POINT_HUB_FW_VER_START_ADDR_IN_MSBL_FILE]) &&
		(ic_fw_ver[1] == fw_buf[TOUCH_POINT_HUB_FW_VER_START_ADDR_IN_MSBL_FILE + 1]) &&
		(ic_fw_ver[2] == fw_buf[TOUCH_POINT_HUB_FW_VER_START_ADDR_IN_MSBL_FILE + 2])) {
		kfree(bl_page_buf);
		dev_info(&pdata->client->dev, "[hub bl] same fw ver(%d.%d.%d), skip fw dl!", ic_fw_ver[0], ic_fw_ver[1], ic_fw_ver[2]);
		return 0;
	}
	dev_info(&pdata->client->dev, "[hub bl] ic fw ver:%d.%d.%d, file fw ver:%d.%d.%d",
					ic_fw_ver[0],
					ic_fw_ver[1],
					ic_fw_ver[2],
					fw_buf[TOUCH_POINT_HUB_FW_VER_START_ADDR_IN_MSBL_FILE],
					fw_buf[TOUCH_POINT_HUB_FW_VER_START_ADDR_IN_MSBL_FILE + 1],
					fw_buf[TOUCH_POINT_HUB_FW_VER_START_ADDR_IN_MSBL_FILE + 2]);

#if 0
	/* add++ for enabling crc in bl config to fix BL corruption issue by flashing partial app fw */
	/* reset and enter bl mode */
	rc = touch_point_hub_bl_enter_bl_mode(pdata);
	if (rc)
		goto dl_exit;

	/* check if hub is in bl mode */
	rc = touch_point_hub_bl_check_current_mode(pdata);
	if (rc)
		goto dl_exit;

	/* enable crc */
	rc = touch_point_hub_bl_enable_crc(pdata);
	if (rc)
		goto dl_exit;

	/* exit bl mode, no need to check its return value */
	touch_point_hub_bl_exit_bl_mode(pdata);
	/* add-- for enabling crc in bl config to fix BL corruption issue by flashing partial app fw */
#endif

	/* reset and enter bl mode */
	rc = touch_point_hub_bl_enter_bl_mode(pdata);
	if (rc)
		goto dl_exit;

	/* check if hub is in bl mode */
	rc = touch_point_hub_bl_check_current_mode(pdata);
	if (rc)
		goto dl_exit;

	/* check hub device id */
	rc = touch_point_hub_bl_check_device_id(pdata);
	if (rc)
		goto dl_exit;

	/* read bl version */
	rc = touch_point_hub_bl_read_version(pdata);
	if (rc)
		goto dl_exit;

	/* read page size */
	rc = touch_point_hub_bl_read_page_size(pdata);
	if (rc)
		goto dl_exit;

	if (fw_size <= TOUCH_POINT_HUB_BL_PAGE_CNT_ADDR_IN_MSBL_FILE) {
		rc = -2;
		dev_err(&pdata->client->dev, "[hub bl] hub fw size(%d bytes) <= %#X bytes", fw_size, TOUCH_POINT_HUB_BL_PAGE_CNT_ADDR_IN_MSBL_FILE);
		goto dl_exit;
	}
	/*
	 * write page count
	 * get number of pages from MSBL file, addr=0x44
	 */
	page_cnt = fw_buf[TOUCH_POINT_HUB_BL_PAGE_CNT_ADDR_IN_MSBL_FILE];
	rc = touch_point_hub_bl_write_page_count(pdata, page_cnt);
	if (rc)
		goto dl_exit;

#if 1//
	/* erase app memory */
	rc = touch_point_hub_bl_erase_app_memory(pdata);
	if (rc)
		goto dl_exit;
#else
	/* erase app memory by page */
	for (i = 0; i < page_cnt; i++) {
		rc = touch_point_hub_bl_erase_page(pdata, i + 2/*first page num of app memeory*/);
		if (rc)
			goto dl_exit;
	}
#endif

	/* set chunk size for partial page download */
	rc = touch_point_hub_bl_set_chunk_size(pdata);
	if (rc)
		goto dl_exit;

	dev_info(&pdata->client->dev, "[hub bl] page cnt: %d, fw page num: %d", page_cnt, bl_num_of_pages);
	/* transmit application code to hub in chunks */
	if (page_cnt < bl_num_of_pages)
    {
		for (i = 0; ((i < page_cnt) && (!rc)); i++)
		{
			uint8_t block_cnt = 0;
			int curr_chunk_size = BL_PAGE_SIZE;

			while ((curr_chunk_size > 0) && (!rc))
			{
				uint8_t response[1] = {0};

				if (curr_chunk_size < MAX_TRANSMIT_CHUNK_SIZE)
					sz = curr_chunk_size;
				else
					sz = MAX_TRANSMIT_CHUNK_SIZE;
				curr_chunk_size -= MAX_TRANSMIT_CHUNK_SIZE;

				memcpy((uint8_t *)(bl_page_buf+2), (uint8_t *)(fw_buf+offset), sz);
				offset += sz;
				block_cnt++;

				bl_page_buf[0] = 0x80;
				bl_page_buf[1] = 0x04;

                touch_point_hub_bl_send(pdata, (uint8_t *)bl_page_buf, sz+2);
				if (sz < MAX_TRANSMIT_CHUNK_SIZE)
				{
					#if 1//
					if (i == (page_cnt - 1)) {
						msleep(170 + 340);
					} else {
						msleep(170);
					}
					#else
					//last chuck write of each page needs delay; Maxim recommends 170ms wait as it may take that long to finish write operation
					msleep(170);
					if (i == (page_cnt - 1)) //if last page, need additional delay for CRC check
						msleep(340-170); // total 340ms delay when finishing up last page write
					#endif
				}
				touch_point_hub_bl_receive(pdata, response, sizeof(response));
				if ((response[0] == 0xAA) || (response[0] == 0xAB)) {
					//dev_info(&pdata->client->dev, "[hub bl] wrote page %d-%d (size: %d), response:%#X", (i+1), block_cnt, sz, response[0]);
				} else {
					//dev_err(&pdata->client->dev, "[hub bl] page write %d-%d (size: %d) failed, response:%#X", (i+1), block_cnt, sz, response[0]);
					rc = 1;
				}
			}
		}
    } else {
        dev_info(&pdata->client->dev, "[hub bl] incorrect page cnt, possibly corrupt hub fw(page cnt:%d, fw page num:%d)", page_cnt, bl_num_of_pages);
    }

	dev_info(&pdata->client->dev, "[hub bl] flash pages finished(i:%d, actual written:%d bytes, addr[%#X, %#X], expected:%d bytes, addr[%#X, %#X]",
		i,
		offset - TOUCH_POINT_HUB_BL_PAGE_START_ADDR_IN_MSBL_FILE,
		TOUCH_POINT_HUB_BL_PAGE_START_ADDR_IN_MSBL_FILE,
		offset - 1,
		page_cnt * BL_PAGE_SIZE,
		TOUCH_POINT_HUB_BL_PAGE_START_ADDR_IN_MSBL_FILE,
		TOUCH_POINT_HUB_BL_PAGE_START_ADDR_IN_MSBL_FILE + page_cnt * BL_PAGE_SIZE - 1);

dl_exit:
	rc = touch_point_hub_bl_exit_bl_mode(pdata);

	kfree(bl_page_buf);
	return rc;
}

static void touch_point_hub_fw_dl_cb(const struct firmware *fw, void *ctx)
{
	struct touch_point_hub_data *pdata = ctx;
	int rc = 0;

	if (ussys_key_cnt > 0) {
		dev_warn(&pdata->client->dev, "detected ussys sensor(cnt:%d) on AP, skip hub driver!", ussys_key_cnt);
	} else {
		if (fw) {
			dev_info(&pdata->client->dev, "downloading hub fw(size:%zu)...", fw->size);
			rc = touch_point_hub_fw_download(pdata, fw->data, fw->size);
			dev_info(&pdata->client->dev, "hub fw download complete(rc:%d)", rc);

			hrtimer_start(&pdata->timer, ktime_set(0, TOUCH_POINT_HUB_INIT_DONE_WAIT_TIME_US * NSEC_PER_USEC), HRTIMER_MODE_REL);
			pdata->timer_start_ts = ktime_get_boottime();
		} else {
			dev_err(&pdata->client->dev, "get hub fw failed");
		}
	}

	release_firmware(fw);
}

static int touch_point_hub_recovery(struct touch_point_hub_data *pdata)
{
	ktime_t recovery_ts_diff = ktime_set(0,0);
	ktime_t current_ts = ktime_get_boottime();
	int rc = 0;

	/* reject request if this request is close to previous request */
	recovery_ts_diff = ktime_sub(current_ts, pdata->recovery_ts);
	if (ktime_after(ms_to_ktime(TOUCH_POINT_HUB_RECOVERY_REJECTION_TIME_MS), recovery_ts_diff)) {
		dev_warn(&pdata->client->dev, "reject hub recovery request(%lld < %d s)!", ktime_to_ms(recovery_ts_diff)/1000, TOUCH_POINT_HUB_RECOVERY_REJECTION_TIME_MS/1000);
		return 0;
	}

	/* reject request if previous recovery is ongoing */
	if (1 == pdata->recovery_data) {
		dev_warn(&pdata->client->dev, "hub recovery is ongoing, reject this recovery request!");
		return 0;
	}

	dev_info(&pdata->client->dev, "begin to recover hub...");

	disable_irq(pdata->client->irq);

	/* reset chip */
	touch_point_hub_reset(pdata);
	mdelay(50);

	pdata->init_done = false;
	pdata->recovery_data = 1;
	hrtimer_start(&pdata->timer, ktime_set(0, TOUCH_POINT_HUB_INIT_DONE_WAIT_TIME_US * NSEC_PER_USEC), HRTIMER_MODE_REL);
	pdata->timer_start_ts = ktime_get_boottime();

	pdata->recovery_ts = ktime_get_boottime();
	return rc;
}

static int touch_point_hub_check_report_key(struct touch_point_hub_data *pdata)
{
	int i = 0, rc = 0;
	u8 intr_status = 0;

	/* handle init done timer */
	if (!pdata->init_done) {
		u8 start_addr = TOUCH_POINT_HUB_REG_0X27_SNS_INIT_STATUS;
		u8 end_addr = TOUCH_POINT_HUB_REG_INTR_STATUS;
		u8 size = end_addr - start_addr + 1;
		u8 *buf = kzalloc(size, GFP_KERNEL);
		u8 init_status = 0;

		if (NULL == buf) {
			return -ENOMEM;
		}

		/* give permission to rw i2c */
		pdata->is_i2c_rw_just_after_hub_reset = true;

		/* wakeup hub */
		touch_point_hub_wakeup(pdata);
		rc = touch_point_hub_reg_read(pdata, start_addr, buf, size);
		if (rc < 0) {
			dev_err(&pdata->client->dev, "read init done failed!");
			kfree(buf);			
			pdata->is_i2c_rw_just_after_hub_reset = false;
			if (1 == pdata->recovery_data) {
				pdata->recovery_data = 0;//if failed, still mark hub recovery finished.
			}
			return rc;
		}

		intr_status = buf[TOUCH_POINT_HUB_REG_INTR_STATUS - start_addr];//for interrupt status checking
		init_status = buf[TOUCH_POINT_HUB_REG_INIT_STATUS - start_addr];//for init done checking
		dev_info(&pdata->client->dev,
			"hub init status: [0x3F:%#X ,0x37:%#X ,0x2F:%#X ,0x27:%#X] ,running indicator:%#X",
			buf[TOUCH_POINT_HUB_REG_0X3F_SNS_INIT_STATUS - start_addr],
			buf[TOUCH_POINT_HUB_REG_0X37_SNS_INIT_STATUS - start_addr],
			buf[TOUCH_POINT_HUB_REG_0X2F_SNS_INIT_STATUS - start_addr],
			buf[TOUCH_POINT_HUB_REG_0X27_SNS_INIT_STATUS - start_addr],
			buf[TOUCH_POINT_HUB_REG_RUNNING_INDICATOR - start_addr]);
		if ((init_status & TOUCH_POINT_HUB_INIT_STATUS_MASK) == TOUCH_POINT_HUB_INIT_STATUS_MASK) {
			u8 hub_whoami = buf[TOUCH_POINT_HUB_REG_WHOAMI - start_addr];
			u8 sensor_bit_map = buf[TOUCH_POINT_HUB_REG_SENSOR_BITMAP - start_addr];

			dev_info(&pdata->client->dev,
				"hub whoami: %#X ,init status: %d ,sensors bit map: %#X [0x3F:%d ,0x37:%d ,0x2F:%d ,0x27:%d] ,intr status:%#X",
				hub_whoami,
				init_status,
				sensor_bit_map,
				(sensor_bit_map & 0x08) >> 3,
				(sensor_bit_map & 0x04) >> 2,
				(sensor_bit_map & 0x02) >> 1,
				sensor_bit_map & 0x01,
				intr_status);

			/* initialize sensors structure */
			pdata->sns_cnt = 0;
			if (sensor_bit_map & 0x01) {
				pdata->sns[pdata->sns_cnt].idx = pdata->sns_cnt;
				pdata->sns[pdata->sns_cnt].i2c_addr = 0x27;
				pdata->sns[pdata->sns_cnt].input_ev_code = pdata->input_code_in_dts[0];
				strncpy(pdata->sns[pdata->sns_cnt].name, touch_point_hub_get_key_name(pdata->sns[pdata->sns_cnt].input_ev_code), MAX_KEY_NAME_LEN - 1);
				touch_point_hub_send_key_map(pdata, pdata->sns[pdata->sns_cnt].idx);
				pdata->sns_cnt++;
			}
			if (sensor_bit_map & 0x02) {
				pdata->sns[pdata->sns_cnt].idx = pdata->sns_cnt;
				pdata->sns[pdata->sns_cnt].i2c_addr = 0x2F;
				pdata->sns[pdata->sns_cnt].input_ev_code = pdata->input_code_in_dts[1];
				strncpy(pdata->sns[pdata->sns_cnt].name, touch_point_hub_get_key_name(pdata->sns[pdata->sns_cnt].input_ev_code), MAX_KEY_NAME_LEN - 1);
				touch_point_hub_send_key_map(pdata, pdata->sns[pdata->sns_cnt].idx);
				pdata->sns_cnt++;
			}
			if (sensor_bit_map & 0x04) {
				pdata->sns[pdata->sns_cnt].idx = pdata->sns_cnt;
				pdata->sns[pdata->sns_cnt].i2c_addr = 0x37;
				pdata->sns[pdata->sns_cnt].input_ev_code = pdata->input_code_in_dts[2];
				strncpy(pdata->sns[pdata->sns_cnt].name, touch_point_hub_get_key_name(pdata->sns[pdata->sns_cnt].input_ev_code), MAX_KEY_NAME_LEN - 1);
				touch_point_hub_send_key_map(pdata, pdata->sns[pdata->sns_cnt].idx);
				pdata->sns_cnt++;
			}
			if (sensor_bit_map & 0x08) {
				pdata->sns[pdata->sns_cnt].idx = pdata->sns_cnt;
				pdata->sns[pdata->sns_cnt].i2c_addr = 0x3F;
				pdata->sns[pdata->sns_cnt].input_ev_code = pdata->input_code_in_dts[3];
				strncpy(pdata->sns[pdata->sns_cnt].name, touch_point_hub_get_key_name(pdata->sns[pdata->sns_cnt].input_ev_code), MAX_KEY_NAME_LEN - 1);
				touch_point_hub_send_key_map(pdata, pdata->sns[pdata->sns_cnt].idx);
				pdata->sns_cnt++;
			}
			/* send board rev to hub */
			touch_point_hub_send_board_rev(pdata);
			/* send haptic setting to hub */
			touch_point_hub_send_haptic_setting(pdata);
			/* set hub restart indicator */
			touch_point_hub_set_hub_restart_indicator(pdata);

			/* print hub version */
			scnprintf(pdata->version, MAX_HUB_VERSION_LEN, "%d.%d.%d",
						buf[TOUCH_POINT_HUB_REG_VER_MAJOR - start_addr],
						buf[TOUCH_POINT_HUB_REG_VER_MINOR - start_addr],
						buf[TOUCH_POINT_HUB_REG_VER_PATCH - start_addr]);
			pdata->version[MAX_HUB_VERSION_LEN - 1] = '\0';
			dev_info(&pdata->client->dev, "hub ver:%s", pdata->version);

			/* check hub whoami */
			if (TOUCH_POINT_HUB_WHOAMI_VALUE == hub_whoami) {
				pdata->whoami = hub_whoami;
				dev_info(&pdata->client->dev, "read hub whoami successfully(hub whoami:%#X)", hub_whoami);
			} else {
				dev_info(&pdata->client->dev, "read hub whoami failed(hub whoami:%#X)", hub_whoami);
			}

			if (pdata->sns_cnt > 0) {
				/* check sensor whoami */
				touch_point_sns_check_whoami(pdata);
				touch_point_hub_init(pdata);

				/* notify daemon to finish hub recovery routine */
				if (1 == pdata->recovery_data) {
					kill_fasync(&pdata->fasync, SIGIO, POLL_IN);
				}
				pdata->init_done = true;
				dev_info(&pdata->client->dev, "hub init is done!");
			}
		} else {
			dev_err(&pdata->client->dev, "init done bit is NOT set!");
			if (1 == pdata->recovery_data) {
				pdata->recovery_data = 0;//if failed, still mark hub recovery finished.
			}
		}
		kfree(buf);
		pdata->is_i2c_rw_just_after_hub_reset = false;
	} else {
		rc = touch_point_hub_reg_read(pdata, TOUCH_POINT_HUB_REG_INTR_STATUS, &intr_status, sizeof(intr_status));
		if (rc < 0) {
			dev_err(&pdata->client->dev, "read intr status failed!");
			return rc;
		}
	}

	if (1 == pdata->recovery_data) {
		dev_warn(&pdata->client->dev, "recovery is ongoing, ignore key event!");
		return 0;
	}

	//TODO: handle sensor events!
	//In case of hub fw requests recovery
	//touch_point_hub_recovery(pdata);

	/* Ignore interrupt event while cali is ongoing */
	if (true == pdata->is_cali_ongoing) {
		dev_warn(&pdata->client->dev, "cali is ongoing, ignore key event!");
		return 0;
	}

	if (!pdata->init_done) {
		return 0;
	}

	dev_info(&pdata->client->dev, "intr status:%#X", intr_status);
	for (i = 0; i < pdata->sns_cnt; i++) {		
		uint8_t key_status = intr_status & (1 << i);
		touch_point_sns_t *sns = &pdata->sns[i];

		if (sns->input_ev_enabled) {
			if (key_status) {
				sns->key_pressed = true;
				input_report_key(pdata->input_dev, sns->input_ev_code, 1);
				input_sync(pdata->input_dev);
				dev_info(&pdata->client->dev, "==========key%d[%s][press]==========", i, sns->name);
			} else {
				if (sns->key_pressed) {
					sns->key_pressed = false;
					input_report_key(pdata->input_dev, sns->input_ev_code, 0);
					input_sync(pdata->input_dev);
					dev_info(&pdata->client->dev, "==========key%d[%s][release]==========", i, sns->name);
				}
			}
		
		}
	}

	//add++ for swipe gesture
	{
		u8 swipe_cnt[2] = {0, 0};

		rc = touch_point_hub_reg_read(pdata, TOUCH_POINT_HUB_REG_SWIPE_UP_EVENT_CNT, swipe_cnt, sizeof(swipe_cnt));
		if (rc < 0) {
			dev_err(&pdata->client->dev, "read swipe cnt failed(rc:%d)!", rc);
			return rc;
		}

		dev_info(&pdata->client->dev, "swipe up cnt[%d -> %d], swipe down cnt[%d -> %d]",
			pdata->swipe_up_cnt,
			swipe_cnt[0],
			pdata->swipe_down_cnt,
			swipe_cnt[1]);

		if (swipe_cnt[0] != pdata->swipe_up_cnt) {
			input_event(pdata->input_dev, EV_KEY, KEY_SCROLLUP, 1);
			input_event(pdata->input_dev, EV_KEY, KEY_SCROLLUP, 0);
			input_sync(pdata->input_dev);
			dev_info(&pdata->client->dev, "==========Swipe Up Detected!==========");
			pdata->swipe_up_cnt = swipe_cnt[0];
		}

		if (swipe_cnt[1] != pdata->swipe_down_cnt) {
			input_event(pdata->input_dev, EV_KEY, KEY_SCROLLDOWN, 1);
			input_event(pdata->input_dev, EV_KEY, KEY_SCROLLDOWN, 0);
			input_sync(pdata->input_dev);
			dev_info(&pdata->client->dev, "==========Swipe Down Detected!==========");
			pdata->swipe_down_cnt = swipe_cnt[1];
		}
	}
	//add-- for swipe gesture
	return 0;
}

static void touch_point_hub_worker(struct work_struct *work)
{
	struct touch_point_hub_data *pdata = container_of(work, struct touch_point_hub_data, delaywork.work);

	touch_point_hub_check_report_key(pdata);
	if (pdata->wakeup)
		pm_relax(&pdata->client->dev);
}

static enum hrtimer_restart touch_point_hub_timer_cb(struct hrtimer *timer)
{
	struct touch_point_hub_data *pdata = container_of(timer, struct touch_point_hub_data, timer);

	if(NULL != pdata->touch_point_hub_wq)
		queue_delayed_work(pdata->touch_point_hub_wq, &pdata->delaywork, 0);

	dev_info(&pdata->client->dev, "hub init timer cb");
	return HRTIMER_NORESTART;
}

/* add++ for sysfs nodes */
static ssize_t touch_point_curr_sns_idx_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "%d(%s)\n",
		pdata->curr_sns_idx,
		pdata->curr_sns_idx < MAX_SENSORS_NUM ? pdata->sns[pdata->curr_sns_idx].name : "out of range");
	return rc;
}

static ssize_t touch_point_curr_sns_idx_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	char *endp;

	pdata->curr_sns_idx = simple_strtoul(buf, &endp, 0);

	dev_info(&pdata->client->dev, "set curr_sns_idx=%d", pdata->curr_sns_idx);
	return size;
}

static ssize_t touch_point_reg_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	char *endp;

	/*
	 * a) if target_to_read is sns_idx (i.e. target_to_read value is 0, 1, ..., sns_cnt-1),
	 * this reg_read is going to read sensor's fd register or memory addr.
	 *
	 * b) if target_to_read value is >= sns_cnt, it will read hub's register.
	 */
	pdata->target_to_read = simple_strtoul(buf, &endp, 0);
	pdata->reg_to_read = simple_strtoul(endp+1, &endp, 0);
	pdata->size_to_read = simple_strtoul(endp+1, NULL, 0);

	dev_info(&pdata->client->dev, "[read] target:%d ,reg:0x%x ,size:%d", pdata->target_to_read, pdata->reg_to_read, pdata->size_to_read);
	return size;
}

static ssize_t touch_point_reg_read_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	u8 *reg = kzalloc(pdata->size_to_read, GFP_KERNEL);// 256 is total registers
	int rc = 0;
	u16 i = 0;

	if (reg == NULL) {
		dev_err(&pdata->client->dev, "no mem for read buf!");
		return -ENOMEM;
	}

	/* read target is sensor */
	if (pdata->target_to_read < pdata->sns_cnt) {
		if ((pdata->reg_to_read & 0xFF00) == 0) {
			/* if reg addr is 1-byte for sns, we assume it's fd reg addr. */
			touch_point_sns_fd_reg_read(pdata, pdata->sns[pdata->target_to_read].i2c_addr, pdata->reg_to_read, reg, pdata->size_to_read);
		} else {
			/* if reg addr is 2-bytes for sns, we assume it's memory addr like dsram addr, hw reg */
			touch_point_sns_mem_read(pdata, pdata->sns[pdata->target_to_read].i2c_addr, pdata->reg_to_read, reg, pdata->size_to_read);
		}
 	} else {
 		/* read target is hub */
		touch_point_hub_reg_read(pdata, pdata->reg_to_read, reg, pdata->size_to_read);
	}

	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=====target:%d ,reg:0x%x ,size:%d=====\n", pdata->target_to_read, pdata->reg_to_read, pdata->size_to_read);
	while (i < pdata->size_to_read) {
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x%x = 0x%02x\n", pdata->reg_to_read + i, reg[i]);
		i++;
	}

	kfree(reg);
	return rc;
}

static ssize_t touch_point_reg_write_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	char *wbuf = kzalloc(size, GFP_KERNEL);
	char *endp;
	u8 target = 0;
	u16 reg = 0;
	u16 len;
	u16 i = 0;

	if (wbuf == NULL) {
		dev_err(&pdata->client->dev, "no mem for write buf!");
		return -ENOMEM;
	}

	target = simple_strtoul(buf, &endp, 0);
	reg = simple_strtoul(endp+1, &endp, 0);
	len = simple_strtoul(endp+1, &endp, 0);

	while ((endp+1 < buf + size) && (i < size)) {
		wbuf[i] = simple_strtoul(endp+1, &endp, 0);
		i++;
	}

	dev_info(&pdata->client->dev, "[write] target:%d ,reg:0x%x ,len:%d ,len_of_buf:%d", target, reg, len, i);

	/* write target is sensor */
	if (target < pdata->sns_cnt) {
		if ((reg & 0xFF00) == 0) {
			/* if reg addr is 1-byte for sns, we assume it's fd reg addr. */
			touch_point_sns_fd_reg_write(pdata, pdata->sns[target].i2c_addr, reg, wbuf, len);
		} else {
			/* if reg addr is 2-bytes for sns, we assume it's memory addr like dsram addr, hw reg */
			touch_point_sns_mem_write(pdata, pdata->sns[target].i2c_addr, reg, wbuf, len);
		}
	} else {
		touch_point_hub_reg_write(pdata, reg, wbuf, len);
	}

	kfree(wbuf);
	return size;
}

static ssize_t touch_point_hub_debug_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	int i = 0, rc = 0;

	/* print linux kernel driver version */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "linux kernel drv ver:%s\n", TOUCH_POINT_HUB_LINUX_DRIVER_VERSION);

	/* print linux dts config */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=====dts config=====\n");
	if (MAX_SENSORS_NUM >= 4) {
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x27[A0:0, A1:0] input code(%d) --> %s\n", pdata->input_code_in_dts[0], touch_point_hub_get_key_name(pdata->input_code_in_dts[0]));
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x2F[A0:0, A1:1] input code(%d) --> %s\n", pdata->input_code_in_dts[1], touch_point_hub_get_key_name(pdata->input_code_in_dts[1]));
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x37[A0:1, A1:0] input code(%d) --> %s\n", pdata->input_code_in_dts[2], touch_point_hub_get_key_name(pdata->input_code_in_dts[2]));
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x3F[A0:1, A1:1] input code(%d) --> %s\n", pdata->input_code_in_dts[3], touch_point_hub_get_key_name(pdata->input_code_in_dts[3]));
	}
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "hub_rst_high_active(%d)\n", pdata->hub_rst_high_active);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "sns_rst_high_active(%d)\n", pdata->sns_rst_high_active);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "chip_mode(%d[%s])\n", pdata->chip_mode_in_dts, touch_point_hub_get_chip_mode_string(pdata->chip_mode_in_dts));
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "board_rev(%d)\n", pdata->board_rev);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "vdd_supply_configured_in_dts(%d)\n", pdata->vdd_supply_configured_in_dts);
	if (MAX_SENSORS_NUM >= 4) {
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "en_haptic([0x3F:%d ,0x37:%d ,0x2F:%d ,0x27:%d])\n",
												pdata->en_haptic[3],
												pdata->en_haptic[2],
												pdata->en_haptic[1],
												pdata->en_haptic[0]);
	}
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "known_num_of_keys(%d)\n", pdata->known_num_of_keys);

	/* print hub info */	
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=====hub info=====\n");
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "hub bl ver:%s\n", pdata->bl_version);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "hub fw ver:%s\n", pdata->version);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "hub whoami:%#X\n", pdata->whoami);

	/* print sensors info */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=====sensor info=====\n");
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "sensor count:%d\n", pdata->sns_cnt);
	for (i = 0; i < pdata->sns_cnt; i++) {
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "--------------------\n");
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "idx:%d\n", pdata->sns[i].idx);
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "i2c_addr:%#X\n", pdata->sns[i].i2c_addr);
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "name:%s\n", pdata->sns[i].name);
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "whoami:%#X\n", pdata->sns[i].whoami);
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "si_rev:%#X\n", pdata->sns[i].si_rev);
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "input_ev_code:%d\n", pdata->sns[i].input_ev_code);
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "fw ver:%s %s\n", pdata->sns[i].ver_date, pdata->sns[i].ver_time);
	}
	return rc;
}

static ssize_t touch_point_hub_keep_awake_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	char *endp;
	uint8_t keep_awake;

	keep_awake = simple_strtoul(buf, &endp, 0);

#if ENABLE_HUB_WAKEUP_BY_INT_PIN
	disable_irq(pdata->client->irq);
#endif

	/* wakeup hub */
	touch_point_hub_wakeup(pdata);
	touch_point_hub_keep_awake(pdata, !!keep_awake);

#if ENABLE_HUB_WAKEUP_BY_INT_PIN
	enable_irq(pdata->client->irq);
#endif

	return size;
}

static ssize_t touch_point_hub_recovery_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	char *endp;
	uint8_t recover_device = 0;

	recover_device = simple_strtoul(buf, &endp, 0);
	if (recover_device) {
		touch_point_hub_recovery(pdata);
	}
	return size;
}

static ssize_t touch_point_sns_fd_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
#define FD_REG_DEBUG_START_ADDR	0x54
#define FD_REG_DEBUG_END_ADDR		0x6F
#define FD_REG_DEBUG_READ_SIZE	(FD_REG_DEBUG_END_ADDR - FD_REG_DEBUG_START_ADDR + 1)
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	u8 fd_regs[FD_REG_DEBUG_READ_SIZE] = {0};
	int i = 0;
	int rc = 0;
	uint8_t idx = pdata->curr_sns_idx;
	uint8_t usp_contrast = 0;
	int8_t zforce_sum_contrast = 0, zforce_diff_contrast = 0;

	if (pdata->sns_cnt == 0 || pdata->sns_cnt > MAX_SENSORS_NUM || idx >= pdata->sns_cnt) {		
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "bad status(sns_cnt:%d(max:%d), curr_sns_idx:%d)\n",
					pdata->sns_cnt, MAX_SENSORS_NUM, pdata->curr_sns_idx);
		return rc;
	}

	touch_point_sns_fd_reg_read(pdata, pdata->sns[idx].i2c_addr, FD_REG_DEBUG_START_ADDR, fd_regs, FD_REG_DEBUG_READ_SIZE);

	usp_contrast = fd_regs[0x5B-FD_REG_DEBUG_START_ADDR];
	zforce_sum_contrast = (int8_t)fd_regs[0x5C-FD_REG_DEBUG_START_ADDR];
	zforce_diff_contrast = (int8_t)fd_regs[0x5D-FD_REG_DEBUG_START_ADDR];

	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "==========sns%d(%s)==========\n", idx, pdata->sns[idx].name);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "USP Contrast: %4d ,ADC: %4d\n", usp_contrast, TOUCH_POINT_SNS_USP_OFFSET_VALUE - usp_contrast);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "ZForce Sum Contrast: %4d ,ADC: %4d\n", zforce_sum_contrast, TOUCH_POINT_SNS_ZFORCE_OFFSET_VALUE + zforce_sum_contrast);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "ZForce Diff Contrast: %4d ,ADC: %4d\n", zforce_diff_contrast, TOUCH_POINT_SNS_ZFORCE_OFFSET_VALUE + zforce_diff_contrast);
	for (i = 0; i < FD_REG_DEBUG_READ_SIZE; i++) {
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "fd reg[%#X]=%#X\n", (FD_REG_DEBUG_START_ADDR + i), fd_regs[i]);
	}
	return rc;
}

static ssize_t touch_point_sns_mem_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	int rc = 0;
	u8 debug_data[20];
	uint8_t idx = pdata->curr_sns_idx;

	if (pdata->sns_cnt == 0 || pdata->sns_cnt > MAX_SENSORS_NUM || idx >= pdata->sns_cnt) {		
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "bad status(sns_cnt:%d(max:%d), curr_sns_idx:%d)\r\n",
					pdata->sns_cnt, MAX_SENSORS_NUM, pdata->curr_sns_idx);
		return rc;
	}

#if ENABLE_SNS_DSRAM_SINGLE_BYTE_RW
	{
		int i = 0;

		for (i = 0; i < ARRAY_SIZE(debug_data); i++) {
			touch_point_sns_mem_read(pdata, pdata->sns[idx].i2c_addr, TOUCH_POINT_SNS_REG_ADC + i, &debug_data[i], 1);
		}
	}
#else
	touch_point_sns_mem_read(pdata, pdata->sns[idx].i2c_addr, TOUCH_POINT_SNS_REG_ADC, debug_data, sizeof(debug_data));
#endif

	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "==========sns%d(%s)==========\n", idx, pdata->sns[idx].name);
	rc += snprintf(buf + rc, PAGE_SIZE - rc,
			"USPxX: %4d\n"
			"USP1x: %4d\n"
			"ZForceSum: %4d\n"
			"ZForceSumOffset: %4d\n"
			"ZForceDiff: %4d\n"
			"ZForceDiffOffset: %4d\n"
			"Temp: %4d\n"
			"AlgoState: %4d\n"
			"AlgoStateCNT: %4d\n"
			"SensorCtrl: %#X\n"
			"adcXAvgTimes_Debug: %4d\n"
			"adcXAvgTimes_LPM: %4d\n",
			(debug_data[0] << 8 | debug_data[1]) >> 5,	//USP xX
			(debug_data[2] << 8 | debug_data[3]) >> 5,	//USP 1x
			(uint16_t)debug_data[4] << 3,				//ZForce Sum
			(uint16_t)debug_data[5] << 3,				//ZForce Sum Offset
			(uint16_t)debug_data[6] << 3,				//ZForce Diff
			(uint16_t)debug_data[7] << 3,				//ZForce Diff Offset
			(debug_data[8] << 8 | debug_data[9]) >> 5,	//Temperature
			(int)debug_data[14],						//AlgoState
			(int)debug_data[15],						//AlgoStateCNT
			(int)debug_data[17],						//GPR1: Sensor Control
			(int)debug_data[18],						//adcXAvgTimes_Debug
			(int)debug_data[19]);						//adcXAvgTimes_LPM
	return rc;
}

static ssize_t touch_point_sns_en_hpm_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	char *endp;
	uint8_t en_hpm = 0;
	int i = 0;

	en_hpm = simple_strtoul(buf, &endp, 0);

#if ENABLE_HUB_WAKEUP_BY_INT_PIN
	disable_irq(pdata->client->irq);
#endif

	/* wakeup hub */
	touch_point_hub_wakeup(pdata);
	for (i = 0; i < pdata->sns_cnt; i++) {
		/* switch sns to hpm */
		touch_point_sns_toggle_int0(pdata, i);
		#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
		touch_point_sns_put_to_hold(pdata, i, true);
		#endif
		touch_point_sns_enable_lpm(pdata, i, !en_hpm);
		#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
		touch_point_sns_put_to_hold(pdata, i, false);
		#endif
	}

#if ENABLE_HUB_WAKEUP_BY_INT_PIN
	enable_irq(pdata->client->irq);
#endif

	return size;
}

static ssize_t touch_point_sns_otp_mem_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_hub_data *pdata = dev_get_drvdata(dev);
	int rc = 0, i = 0, line = 0;
	int first_line_addr = TOUCH_POINT_SNS_OTP_MEM_START_ADDR >> 4;
	int last_line_addr = (TOUCH_POINT_SNS_OTP_MEM_FW_START_ADDR - 1) >> 4;//TOUCH_POINT_SNS_OTP_MEM_END_ADDR >> 4;
	int idx = pdata->curr_sns_idx;
	touch_point_sns_t *sensor = NULL;

	if (pdata->sns_cnt == 0 || pdata->sns_cnt > MAX_SENSORS_NUM || idx >= pdata->sns_cnt) {		
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "bad status(sns_cnt:%d(max:%d), curr_sns_idx:%d)\r\n",
					pdata->sns_cnt, MAX_SENSORS_NUM, pdata->curr_sns_idx);
		return rc;
	}

	sensor = &pdata->sns[idx];

	if (NULL == sensor->otp_mem)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "otp memory is NULL\r\n");
		return rc;
	}

	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=====OTP Mem Dump(sns%d[%s])=====\r\n", idx, sensor->name);

	for (i = 0; i < 4; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X : %#X\r\n",
						TOUCH_POINT_SNS_OTP_MEM_START_ADDR + i,
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_MEM_START_ADDR + i)]);
	}

	/* OTP ZForce */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "//ZForce\r\n");
	for (i = 0; i < TOUCH_POINT_SNS_OTP_ZFORCE_SIZE; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X(Reg %#X) : %#X\r\n",
						TOUCH_POINT_SNS_OTP_ZFORCE_START_ADDR + i,
						0x1830 + i,
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_ZFORCE_START_ADDR + i)]);
	}

	/* OTP USP Bank0 */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "//USP Bank0\r\n");
	for (i = 0; i < TOUCH_POINT_SNS_OTP_BANK0_SIZE; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X(Reg %#X) : %#X\r\n",
						TOUCH_POINT_SNS_OTP_BANK0_START_ADDR + i,
						0x1880 + i,
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_BANK0_START_ADDR + i)]);
	}

	/* OTP USP Bank1 */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "//USP Bank1\r\n");
	for (i = 0; i < TOUCH_POINT_SNS_OTP_BANK1_SIZE; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X(Reg %#X) : %#X\r\n",
						TOUCH_POINT_SNS_OTP_BANK1_START_ADDR + i,
						0x18C0 + i,
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_BANK1_START_ADDR + i)]);
	}

	/* OTP IDs */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "LOTID_H : %#X ,LOTID_L : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_LOTID_H_ADDR)],
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_LOTID_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "WaferID : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_WAFERID_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "StackID(msb) : %#X ,StackID(lsb) : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_STACKID_MSB_ADDR)],
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_STACKID_LSB_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "OTP Ver : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_OTP_VER_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "SiRev(old) : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_SIREV_OLD_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "FW Ver_H : %#X ,FW Ver_L : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_FW_VER_H_ADDR)],
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_FW_VER_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "TempOff_H : %#X ,TempOff_L : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_TIMEOFF_H_ADDR)],
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_TIMEOFF_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "Chip : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_CHIP_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "DieID_R_H : %#X ,DieID_R_L : %#X ,DieID_C_H : %#X ,DieID_C_L : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_R_H_ADDR)],
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_R_L_ADDR)],
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_C_H_ADDR)],
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_C_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "32-bit DieID (R-H R-L C-H C-L): 0x%08X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_R_H_ADDR)]<<24 |
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_R_L_ADDR)]<<16 |
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_C_H_ADDR)]<< 8 |
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_DIEID_C_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "SiRev(new) : %#X\r\n",
						sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(TOUCH_POINT_SNS_OTP_SIREV_NEW_ADDR)]);

	/* OTP Memory */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "//OTP Memory:\r\n");
	//print column info
	for (i = 0; i < 16; i++)
	{
		if (i == 0)
			rc += scnprintf(buf + rc, PAGE_SIZE - rc, "      ");//6 spaces

		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x%02x ", i);

		if (i == 15)
			rc += scnprintf(buf + rc, PAGE_SIZE - rc, "\r\n");
	}

	for (line = last_line_addr; line >= first_line_addr; line--)
	{
		//print row info
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x%03x:", line);
		for (i = 0; i < 16; i++)
		{
			int otp_addr = (line << 4) + i;
			if (otp_addr >= TOUCH_POINT_SNS_OTP_MEM_START_ADDR && otp_addr <= TOUCH_POINT_SNS_OTP_MEM_END_ADDR)
				rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x%02x ", sensor->otp_mem[TOUCH_POINT_SNS_OTPADDR2IDX(otp_addr)]);
		}
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "\r\n");
	}

	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=========END==========\r\n");
	return rc;
}

static DEVICE_ATTR(curr_sns_idx, (S_IRUGO | S_IWUSR), touch_point_curr_sns_idx_show, touch_point_curr_sns_idx_store);
static DEVICE_ATTR(reg_read, (S_IRUGO | S_IWUSR), touch_point_reg_read_show, touch_point_reg_read_store);
static DEVICE_ATTR(reg_write, S_IWUSR, NULL, touch_point_reg_write_store);
static DEVICE_ATTR(hub_debug_info, S_IRUGO, touch_point_hub_debug_info_show, NULL);
static DEVICE_ATTR(hub_keep_awake, S_IWUSR, NULL, touch_point_hub_keep_awake_store);
static DEVICE_ATTR(hub_recovery, S_IWUSR, NULL, touch_point_hub_recovery_store);
static DEVICE_ATTR(sns_fd_info, S_IRUGO, touch_point_sns_fd_info_show, NULL);
static DEVICE_ATTR(sns_mem_info, S_IRUGO, touch_point_sns_mem_info_show, NULL);
static DEVICE_ATTR(sns_en_hpm, S_IWUSR, NULL, touch_point_sns_en_hpm_store);
static DEVICE_ATTR(sns_otp_mem, S_IRUGO, touch_point_sns_otp_mem_show, NULL);

static struct attribute *touch_point_hub_attrs[] = {
	&dev_attr_curr_sns_idx.attr,
	&dev_attr_reg_read.attr,
	&dev_attr_reg_write.attr,
	&dev_attr_hub_debug_info.attr,
	&dev_attr_hub_keep_awake.attr,
	&dev_attr_hub_recovery.attr,
	&dev_attr_sns_fd_info.attr,
	&dev_attr_sns_mem_info.attr,
	&dev_attr_sns_en_hpm.attr,
	&dev_attr_sns_otp_mem.attr,
	NULL
};

static const struct attribute_group touch_point_hub_attr_group = {
	.attrs = touch_point_hub_attrs,
};
/* add-- for sysfs nodes */

/* add++ for misc device */
static int touch_point_hub_set_param(struct touch_point_hub_data *pdata, uint8_t sns_idx, ussys_param_type param, uint8_t *buf, uint16_t len)
{
	int i = 0;

	if (NULL == buf || 0 == len)
		return -1;

	switch (param) {
	case USSYS_HUB_ENABLE_INPUT_KEY_EVENT:
		{
			if (sns_idx < pdata->sns_cnt) {
				pdata->sns[sns_idx].input_ev_enabled = buf[0];
			} else {
				for (i = 0; i < pdata->sns_cnt; i++) {
					pdata->sns[i].input_ev_enabled = buf[0];
				}
			}
			dev_info(&pdata->client->dev, "[set_param][enable input key event(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_HUB_WAKEUP:
		{
			bool wakeup_hub = !!buf[0];

			if (wakeup_hub) {
				/* wakeup hub */
				touch_point_hub_wakeup(pdata);
			}

			dev_info(&pdata->client->dev, "[set_param][wakeup hub(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_HUB_ENABLE_KEEP_AWAKE:
		{
			bool hub_keep_awake = !!buf[0];

			#if ENABLE_HUB_WAKEUP_BY_INT_PIN
			disable_irq(pdata->client->irq);
			#endif

			/* wakeup hub */
			touch_point_hub_wakeup(pdata);
			touch_point_hub_keep_awake(pdata, hub_keep_awake);

			#if ENABLE_HUB_WAKEUP_BY_INT_PIN
			enable_irq(pdata->client->irq);
			#endif
			dev_info(&pdata->client->dev, "[set_param][enable keep hub awake(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;		
	case USSYS_HUB_SET_CALI_FLAG:
		{
			bool cali_flag = !!buf[0];

			pdata->is_cali_ongoing = cali_flag;
			//bypass hub algo while calibration is ongoing.
			/*
			if (cali_flag) {
				touch_point_hub_bypass_algo(pdata, true);
			} else {
				touch_point_hub_bypass_algo(pdata, false);
			}
			*/
			dev_info(&pdata->client->dev, "[set_param][set cali flag(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_HUB_RECOVER_DEV:
		{
			bool recover_dev = !!buf[0];

			if (recover_dev) {
				touch_point_hub_recovery(pdata);
			}
			dev_info(&pdata->client->dev, "[set_param][recover dev(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_SNS_TOGGLE_INT0:
		{
			uint8_t toggle_int0 = buf[0];

			if (toggle_int0) {
				#if ENABLE_HUB_WAKEUP_BY_INT_PIN
				disable_irq(pdata->client->irq);
				#endif

				/* wakeup hub */
				touch_point_hub_wakeup(pdata);
				if (sns_idx < pdata->sns_cnt) {
					touch_point_sns_toggle_int0(pdata, sns_idx);
				} else {
					for (i = 0; i < pdata->sns_cnt; i++) {
						touch_point_sns_toggle_int0(pdata, i);
					}
				}

				#if ENABLE_HUB_WAKEUP_BY_INT_PIN
				enable_irq(pdata->client->irq);
				#endif
			}
			dev_info(&pdata->client->dev, "[set_param][toggle sns int0(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_SNS_ENABLE_HPM:
		{
			uint8_t en_hpm = buf[0];

			#if ENABLE_HUB_WAKEUP_BY_INT_PIN
			disable_irq(pdata->client->irq);
			#endif

			/* wakeup hub */
			touch_point_hub_wakeup(pdata);			
			if (sns_idx < pdata->sns_cnt) {
				/* switch sns to hpm */
				touch_point_sns_toggle_int0(pdata, sns_idx);
				#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
				touch_point_sns_put_to_hold(pdata, i, true);
				#endif
				touch_point_sns_enable_lpm(pdata, sns_idx, !en_hpm);
				#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
				touch_point_sns_put_to_hold(pdata, i, false);
				#endif
			} else {
				for (i = 0; i < pdata->sns_cnt; i++) {
					/* switch sns to hpm */
					touch_point_sns_toggle_int0(pdata, i);
					#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
					touch_point_sns_put_to_hold(pdata, i, true);
					#endif
					touch_point_sns_enable_lpm(pdata, i, !en_hpm);
					#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
					touch_point_sns_put_to_hold(pdata, i, false);
					#endif
				}
			}

			#if ENABLE_HUB_WAKEUP_BY_INT_PIN
			enable_irq(pdata->client->irq);
			#endif
			dev_info(&pdata->client->dev, "[set_param][en hpm(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	default:
		dev_err(&pdata->client->dev, "[set_param]Unsupported param %d", param);
		return -1;
	}
	return 0;
}

static int touch_point_hub_get_param(struct touch_point_hub_data *pdata, uint8_t sns_idx, ussys_param_type param, uint8_t *buf, uint16_t len)
{
	if (NULL == buf || 0 == len)
		return -1;

	switch (param) {
	case USSYS_HUB_ENABLE_INPUT_KEY_EVENT:
		{
			if (sns_idx < pdata->sns_cnt) {
				buf[0] = pdata->sns[sns_idx].input_ev_enabled;
			}
			dev_info(&pdata->client->dev, "[get_param]enable input key event(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_HUB_GET_INIT_DONE:
		{
			buf[0] = pdata->init_done;
			dev_info(&pdata->client->dev, "[get_param]get init done(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_HUB_GET_KEY_CNT:
		{
			buf[0] = pdata->sns_cnt;
			dev_info(&pdata->client->dev, "[get_param]get key cnt(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_HUB_GET_KEY_NAME:
		{
			if (len >= MAX_KEY_NAME_LEN && sns_idx < pdata->sns_cnt) {
				strncpy(&buf[0], pdata->sns[sns_idx].name, MAX_KEY_NAME_LEN);
				buf[len - 1] = '\0';
				dev_info(&pdata->client->dev, "[get_param]get key name(idx:%d param:%d buf:%s)]",
					sns_idx, param, buf);
			}
		}
		break;
	case USSYS_HUB_GET_VERSION:
		{
			if (sns_idx < pdata->sns_cnt) {
				if (len >= (TOUCH_POINT_SNS_FW_VER_DATE_BYTES + TOUCH_POINT_SNS_FW_VER_TIME_BYTES + 1)) {
					scnprintf(&buf[0], len - 1, "%s %s", pdata->sns[sns_idx].ver_date, pdata->sns[sns_idx].ver_time);
					buf[len - 1] = '\0';
					dev_info(&pdata->client->dev, "[get_param]get sns fw ver(idx:%d param:%d buf:%s)]",
						sns_idx, param, buf);
				}
			} else {
				if (len >= MAX_HUB_VERSION_LEN) {
					strncpy(&buf[0], pdata->version, MAX_HUB_VERSION_LEN);
					buf[len - 1] = '\0';
					dev_info(&pdata->client->dev, "[get_param]get hub ver(idx:%d param:%d buf:%s)]",
						sns_idx, param, buf);
				}
			}
		}
		break;
	case USSYS_HUB_GET_LINUX_DRV_VERSION:
		{
			if (len >= MAX_HUB_VERSION_LEN) {
				strncpy(&buf[0], TOUCH_POINT_HUB_LINUX_DRIVER_VERSION, MAX_HUB_VERSION_LEN);
				buf[len - 1] = '\0';
				dev_info(&pdata->client->dev, "[get_param]get linux drv ver(idx:%d param:%d buf:%s)]",
					sns_idx, param, buf);
			}
		}
		break;
	case USSYS_SNS_GET_OTP_MEM:
		{
			if (len >= TOUCH_POINT_SNS_OTP_MEM_SIZE && sns_idx < pdata->sns_cnt && NULL != pdata->sns[sns_idx].otp_mem) {
				memcpy(&buf[0], pdata->sns[sns_idx].otp_mem, TOUCH_POINT_SNS_OTP_MEM_SIZE);
			}
			dev_info(&pdata->client->dev, "[get_param]get otp mem(idx:%d param:%d)]",
				sns_idx, param);
		}
		break;
	case USSYS_HUB_GET_ALGO_MODE_IN_DTS_SETTING:
		{
			buf[0] = pdata->chip_mode_in_dts;
			dev_info(&pdata->client->dev, "[get_param]get algo mode in dts(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	case USSYS_HUB_GET_KNOWN_NUM_OF_KEYS:
		{
			buf[0] = pdata->known_num_of_keys;
			dev_info(&pdata->client->dev, "[get_param]get known_num_of_keys in dts(idx:%d param:%d buf:%d)]",
				sns_idx, param, buf[0]);
		}
		break;
	default:
		dev_err(&pdata->client->dev, "[get_param]Unsupported param %d", param);
		return -1;
	}

	return 0;
}

static int touch_point_hub_misc_dev_open(struct inode *inode, struct file *file)
{
	struct touch_point_hub_data *pdata = container_of(file->private_data,
					      struct touch_point_hub_data, misc_dev);

	if (!pdata) {
		return -EINVAL;
	}

	mutex_lock(&pdata->lock);
	if (pdata->miscdev_count) {
		dev_err(&pdata->client->dev, "hub misc dev is busy!");
		mutex_unlock(&pdata->lock);
		return -EBUSY;
	}
	pdata->miscdev_count++;
	mutex_unlock(&pdata->lock);

	return 0;
}

static int touch_point_hub_misc_dev_release(struct inode *inode, struct file *file)
{
	struct touch_point_hub_data *pdata = container_of(file->private_data,
						  struct touch_point_hub_data, misc_dev);

	mutex_lock(&pdata->lock);
	if (!pdata->miscdev_count) {
		dev_err(&pdata->client->dev, "haven't yet open hub misc dev!");
		mutex_unlock(&pdata->lock);
		return -ENOTTY;
	}
	pdata->miscdev_count--;
	mutex_unlock(&pdata->lock);

	return 0;
}

long touch_point_hub_misc_dev_ioctl(struct file *file,
	       unsigned int cmd, unsigned long arg)
{
	struct touch_point_hub_data *pdata = container_of(file->private_data,
							  struct touch_point_hub_data, misc_dev);
	ussys_rw_msg __user *user_rw_msg = (ussys_rw_msg __user *) arg;
	ussys_rw_msg *rw_msg;
	uint16_t rw_size;
	int rc = 0;

	switch (cmd) {
	case USSYSIO_READ_DEV:
		get_user(rw_size, &user_rw_msg->len);
		rw_msg = kzalloc(sizeof(ussys_rw_msg) + rw_size, GFP_KERNEL);
		if (rw_msg == NULL) {
			dev_err(&pdata->client->dev, "no mem for read buf.");
			return -EFAULT;
		}
		if (copy_from_user(rw_msg, user_rw_msg, sizeof(ussys_rw_msg))) {
			dev_err(&pdata->client->dev, "copy_from_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}

		/* read reg values to kernel space buf */
		if (rw_msg->target < pdata->sns_cnt) {
			if (rw_msg->addr_h == 0) {
				/* 1-byte reg addr, assume it's sns fd reg addr */
				rc = touch_point_sns_fd_reg_read(pdata, pdata->sns[rw_msg->target].i2c_addr, rw_msg->addr_l, rw_msg->buf, rw_msg->len);
			} else {
				/* 2-bytes reg addr, assume it's sns mem addr */
				rc = touch_point_sns_mem_read(pdata, pdata->sns[rw_msg->target].i2c_addr, rw_msg->addr_h << 8 | rw_msg->addr_l, rw_msg->buf, rw_msg->len);
			}
		} else {
			rc = touch_point_hub_reg_read(pdata, rw_msg->addr_l, rw_msg->buf, rw_msg->len);
		}

		if (rc <0) {
			dev_err(&pdata->client->dev, "ioctl read dev(target:%d) i2c failed(reg=%#X).", rw_msg->target, rw_msg->addr_h << 8 | rw_msg->addr_l);
			kfree(rw_msg);
			return -EFAULT;
		}

		/* copy kernel space buf to user space */
		if (copy_to_user(user_rw_msg->buf, rw_msg->buf, rw_msg->len)) {
			dev_err(&pdata->client->dev, "copy_to_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		kfree(rw_msg);
		break;
	case USSYSIO_WRITE_DEV:
		get_user(rw_size, &user_rw_msg->len);
		rw_msg = kzalloc(sizeof(ussys_rw_msg) + rw_size, GFP_KERNEL);
		if (rw_msg == NULL) {
			dev_err(&pdata->client->dev, "no mem for read buf.");
			return -EFAULT;
		}
		if (copy_from_user(rw_msg, user_rw_msg, sizeof(ussys_rw_msg) + rw_size)) {
			dev_err(&pdata->client->dev, "copy_from_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		/* write reg to device */
		if (rw_msg->target < pdata->sns_cnt) {
			if (rw_msg->addr_h == 0) {
				rc = touch_point_sns_fd_reg_write(pdata, pdata->sns[rw_msg->target].i2c_addr, rw_msg->addr_l, rw_msg->buf, rw_msg->len);
			} else {
				rc = touch_point_sns_mem_write(pdata, pdata->sns[rw_msg->target].i2c_addr, rw_msg->addr_h << 8 | rw_msg->addr_l, rw_msg->buf, rw_msg->len);
			}
		} else {
			rc = touch_point_hub_reg_write(pdata, rw_msg->addr_l, rw_msg->buf, rw_msg->len);
		}

		if (rc <0) {
			dev_err(&pdata->client->dev, "ioctl write dev(target:%d) i2c failed(reg=%#X).", rw_msg->target, rw_msg->addr_h << 8 | rw_msg->addr_l);
			kfree(rw_msg);
			return -EFAULT;
		}
		kfree(rw_msg);
		break;
	case USSYSIO_SET_PARAM:
	{
		get_user(rw_size, &user_rw_msg->len);
		rw_msg = kzalloc(sizeof(ussys_rw_msg) + rw_size, GFP_KERNEL);
		if (rw_msg == NULL) {
			dev_err(&pdata->client->dev, "no mem for read buf.");
			return -EFAULT;
		}
		if (copy_from_user(rw_msg, user_rw_msg, sizeof(ussys_rw_msg) + rw_size)) {
			dev_err(&pdata->client->dev, "copy_from_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		/* set param */
		touch_point_hub_set_param(pdata, rw_msg->target, rw_msg->addr_l, rw_msg->buf, rw_msg->len);
		kfree(rw_msg);
		break;
	}
	case USSYSIO_GET_PARAM:
	{
		int ret = -1;

		get_user(rw_size, &user_rw_msg->len);
		rw_msg = kzalloc(sizeof(ussys_rw_msg) + rw_size, GFP_KERNEL);
		if (rw_msg == NULL) {
			dev_err(&pdata->client->dev, "no mem for read buf.");
			return -EFAULT;
		}
		if (copy_from_user(rw_msg, user_rw_msg, sizeof(ussys_rw_msg))) {
			dev_err(&pdata->client->dev, "copy_from_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		/* read param to kernel space buf */
		ret = touch_point_hub_get_param(pdata, rw_msg->target, rw_msg->addr_l, rw_msg->buf, rw_msg->len);
		if (ret < 0)
		{
			dev_err(&pdata->client->dev, "get param %d failed.", rw_msg->addr_l);
			kfree(rw_msg);
			return -EFAULT;
		}
		/* copy kernel space buf to user space */
		if (copy_to_user(user_rw_msg->buf, rw_msg->buf, rw_msg->len)) {
			dev_err(&pdata->client->dev, "copy_to_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		kfree(rw_msg);
		break;
	}
	default:
		dev_err(&pdata->client->dev, "Unsupported ioctl command %u\n", cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int touch_point_hub_misc_dev_fasync(int fd, struct file *file, int on)
{
	struct touch_point_hub_data *pdata = container_of(file->private_data,
							  struct touch_point_hub_data, misc_dev);

	dev_info(&pdata->client->dev, "set fasync");
	return fasync_helper(fd, file, on, &pdata->fasync);
}

static unsigned int touch_point_hub_misc_dev_poll(struct file *file, poll_table *wait)
{
	//No need to implement.
	return 0;
}

static ssize_t touch_point_hub_misc_dev_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	struct touch_point_hub_data *pdata = container_of(file->private_data,
							  struct touch_point_hub_data, misc_dev);
	int rc = 0;

	if (count != 1) {
		dev_err(&pdata->client->dev, "bad param! (count:%zu)", count);
		return -EINVAL;
	}

	rc = copy_to_user(buffer, &pdata->recovery_data, 1);
	if (rc) {
		dev_err(&pdata->client->dev, "copy_to_user error! (recovery_data:%d)", pdata->recovery_data);
		return -EFAULT;
	}

	return count;
}

static ssize_t touch_point_hub_misc_dev_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	struct touch_point_hub_data *pdata = container_of(file->private_data,
							  struct touch_point_hub_data, misc_dev);
	uint8_t done_flag = 0;
	int rc = 0;

	if (count != 1) {
		dev_err(&pdata->client->dev, "bad param! (count:%zu)", count);
		return -EINVAL;
	}

	rc = copy_from_user(&done_flag, buffer, 1);
	if (rc) {
		dev_err(&pdata->client->dev, "copy_from_user error! (done_flag:%d)", done_flag);
		return -EFAULT;
	}

	if (done_flag) {
		/*
		 * If set to 0, it indicates daemon already re-load calibration files, user config files, etc for this chip.
		 */
		pdata->recovery_data = 0;
		dev_info(&pdata->client->dev, "hub recovery all done!");
	}
	return count;
}

static const struct file_operations touch_point_hub_misc_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= touch_point_hub_misc_dev_open,
	.release	= touch_point_hub_misc_dev_release,
	.unlocked_ioctl	= touch_point_hub_misc_dev_ioctl,
	.fasync		= touch_point_hub_misc_dev_fasync,
	.poll		= touch_point_hub_misc_dev_poll,
	.read		= touch_point_hub_misc_dev_read,
	.write		= touch_point_hub_misc_dev_write,
};
/* add-- for misc device */

/* add++ for input device */
static int touch_point_sns_input_dev_open(struct input_dev *dev)
{
	struct touch_point_hub_data *pdata __maybe_unused = input_get_drvdata(dev);

	//dev_info(&dev->dev, "touch_point_sns_input_dev_open()");
	return 0;
}

static void touch_point_sns_input_dev_close(struct input_dev *dev)
{
	struct touch_point_hub_data *pdata __maybe_unused = input_get_drvdata(dev);
	//dev_info(&dev->dev, "touch_point_sns_input_dev_close()");
}

static int touch_point_setup_input_dev(struct touch_point_hub_data *pdata)
{
	struct input_dev *input_dev;
	struct i2c_client *client = pdata->client;
	int rc = 0;

	input_dev = devm_input_allocate_device(&client->dev);
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device!");
		return -ENOMEM;
	}

	pdata->input_dev = input_dev;

	input_dev->name = client->name;
	input_dev->id.bustype = BUS_I2C;
	input_dev->open = touch_point_sns_input_dev_open;
	input_dev->close = touch_point_sns_input_dev_close;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_REP, input_dev->evbit);
//	input_set_capability(input_dev, EV_KEY, sns->input_ev_code);
	input_set_drvdata(input_dev, pdata);
	rc = input_register_device(input_dev);
	if (rc) {
		dev_err(&client->dev, "failed to register input device!");
		return rc;
	}

	return 0;
}
/* add-- for input device */

static void touch_point_hub_regulator_disable(void *regulator)
{
	regulator_disable(regulator);
}

static void touch_point_hub_remove_group(void *dev)
{
	sysfs_remove_group(&(((struct device *)dev)->kobj), &touch_point_hub_attr_group);
}

static void touch_point_hub_destroy_wq(void *workqueue)
{
	destroy_workqueue(workqueue);
}

#if ENABLE_MSM_DRM_NOTIFIER
/* This callback gets called when something important happens inside a
 * drm driver. We're looking if that important event is blanking,
 * and if it is and necessary, we're switching touch point power as well ...
 */
static int touch_point_hub_drm_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct touch_point_hub_data *pdata;
	struct msm_drm_notifier *evdata = data;
	int drm_blank = 0;

	pdata = container_of(self, struct touch_point_hub_data, drm_notif);
	//dev_info(&pdata->client->dev, "drm_notifier_callback(event:%ld)", event);

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != MSM_DRM_EARLY_EVENT_BLANK && event != MSM_DRM_EVENT_BLANK)
		return 0;

	if (MSM_DRM_PRIMARY_DISPLAY != evdata->id)
		return 0;

	if (pdata && evdata && evdata->data && (MSM_DRM_EVENT_BLANK/*MSM_DRM_EARLY_EVENT_BLANK*/ == event))
	{
		drm_blank = *(int *)evdata->data;
		dev_info(&pdata->client->dev, "drm blank:%d", drm_blank);
		if (MSM_DRM_BLANK_UNBLANK == drm_blank)//0
		{
			dev_info(&pdata->client->dev, "screen on.");
			//add something when phone screen is on.
		}
		else if (MSM_DRM_BLANK_POWERDOWN == drm_blank)//4
		{
			dev_info(&pdata->client->dev, "screen off.");
			//add something when phone screen is off.
		}
		else
		{
			dev_err(&pdata->client->dev, "unknown screen status.");
		}
	}

	return 0;
}

static int touch_point_hub_register_drm(struct touch_point_hub_data *pdata)
{
	memset(&pdata->drm_notif, 0, sizeof(pdata->drm_notif));
	pdata->drm_notif.notifier_call = touch_point_hub_drm_notifier_callback;

	dev_info(&pdata->client->dev, "reg drm notifier.");
	return msm_drm_register_client(&pdata->drm_notif);
}

static void touch_point_hub_unregister_drm(void *data)
{
	struct touch_point_hub_data *pdata = (struct touch_point_hub_data *)data;

	dev_info(&pdata->client->dev, "unreg drm notifier.");
	msm_drm_unregister_client(&pdata->drm_notif);
}
#else
#ifdef CONFIG_FB
/* This callback gets called when something important happens inside a
 * framebuffer driver. We're looking if that important event is blanking,
 * and if it is and necessary, we're switching touch point power as well ...
 */
static int touch_point_hub_fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct touch_point_hub_data *pdata;
	struct fb_event *evdata = data;
	int fb_blank = 0;

	pdata = container_of(self, struct touch_point_hub_data, fb_notif);
	//dev_info(&pdata->client->dev, "fb_notifier_callback(event:%ld)", event);

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK && event != FB_EVENT_CONBLANK)
		return 0;

	if (pdata && evdata && evdata->data)
	{
		fb_blank = *(int *)evdata->data;
		dev_info(&pdata->client->dev, "fb blank:%d", fb_blank);
		if (FB_BLANK_UNBLANK == fb_blank)//0
		{
			dev_info(&pdata->client->dev, "screen unblanked.");
			//add something when phone screen is on.
		}
		else if (FB_BLANK_POWERDOWN == fb_blank)//4
		{
			dev_info(&pdata->client->dev, "screen blanked.");
			//add something when phone screen is off.
		}
	}

	return 0;
}

static int touch_point_hub_register_fb(struct touch_point_hub_data *pdata)
{
	memset(&pdata->fb_notif, 0, sizeof(pdata->fb_notif));
	pdata->fb_notif.notifier_call = touch_point_hub_fb_notifier_callback;

	return fb_register_client(&pdata->fb_notif);
}

static void touch_point_hub_unregister_fb(void *data)
{
	struct touch_point_hub_data *pdata = (struct touch_point_hub_data *)data;

	fb_unregister_client(&pdata->fb_notif);
}
#endif/*CONFIG_FB*/
#endif/*ENABLE_MSM_DRM_NOTIFIER*/

#if ENABLE_SUSPEND_RESUME
static int touch_point_hub_suspend(struct device *dev)
{
	struct touch_point_hub_data *data = dev_get_drvdata(dev);

	hrtimer_cancel(&data->timer);
	cancel_delayed_work_sync(&data->delaywork);
	cancel_delayed_work_sync(&data->monitor_work);

	dev_info(&data->client->dev, "touch_point_hub_suspend\n");
	//dump_stack();
	return 0;
}

static int touch_point_hub_resume(struct device *dev)
{
	struct touch_point_hub_data *pdata/*__maybe_unused*/ = dev_get_drvdata(dev);

	if (pdata->wakeup)
		pm_stay_awake(&pdata->client->dev);

	pdata->dev_get_resumed = true;
	schedule_delayed_work(&pdata->monitor_work, 0);

	dev_info(&pdata->client->dev, "touch_point_hub_resume\n");
	//dump_stack();
	return 0;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id touch_point_hub_of_id_table[] = {
	{.compatible = "ussys,touch_point_hub",},
	{ },
};
MODULE_DEVICE_TABLE(of, touch_point_hub_of_id_table);
#else
#define touch_point_hub_of_id_table NULL
#endif

#ifdef CONFIG_OF
static struct touch_point_hub_data * touch_point_hub_parse_dt(struct device *dev)
{
	struct device_node *node = dev->of_node;
	const struct of_device_id *of_id;
	struct touch_point_hub_data *pdata;

	if (!node) {
		dev_err(dev, "hub dev node is NULL!");
		return ERR_PTR(-ENODEV);
	}

	of_id = of_match_node(touch_point_hub_of_id_table, node);
	if (!of_id) {
		dev_err(dev, "no hub of id");
		return ERR_PTR(-ENODEV);
	}

	pdata = devm_kzalloc(dev, sizeof(struct touch_point_hub_data), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENODEV);

	pdata->hub_rst_high_active = of_property_read_bool(node, "touch_point_hub,rst_high_active");
	pdata->sns_rst_high_active = of_property_read_bool(node, "touch_point_sns,rst_high_active");
	if (NULL != of_parse_phandle(node, "touch_point_hub-vdd-supply", 0)) {
		pdata->vdd_supply_configured_in_dts = true;
	}
	dev_info(dev, "vdd_supply_configured_in_dts(%d)", pdata->vdd_supply_configured_in_dts);

	if (of_property_read_u32(node, "touch_point_hub,rst_convert_time_ms", &pdata->rst_convert_time_ms)) {
		dev_warn(dev, "unable to read 'touch_point_hub,rst_convert_time_ms' from dts");
	}
	if (of_property_read_u32(node, "touch_point_hub,chip_mode", &pdata->chip_mode_in_dts)) {
		dev_warn(dev, "unable to read 'touch_point_hub,chip_mode' from dts");
	}
	if (of_property_read_u32(node, "touch_point_hub,board_rev", &pdata->board_rev)) {
		dev_warn(dev, "unable to read 'touch_point_hub,board_rev' from dts");
	}
	if (of_property_read_u32(node, "touch_point_hub,known_num_of_keys", &pdata->known_num_of_keys)) {
		dev_warn(dev, "unable to read 'touch_point_hub,known_num_of_keys' from dts");
	}
	pdata->wakeup = of_property_read_bool(node, "wakeup-source");

	if (MAX_SENSORS_NUM >= 4) {
		if (of_property_read_u32(node, "linux,input-code-a0_0_a1_0", &pdata->input_code_in_dts[0])) {
			dev_warn(dev, "unable to read 'linux,input-code-a0_0_a1_0' (0x27) from dts");
		}
		dev_info(dev, "dts config: a0_0_a1_0 -> %d [0x27 -> %s]", pdata->input_code_in_dts[0], touch_point_hub_get_key_name(pdata->input_code_in_dts[0]));

		if (of_property_read_u32(node, "linux,input-code-a0_0_a1_1", &pdata->input_code_in_dts[1])) {
			dev_warn(dev, "unable to read 'linux,input-code-a0_0_a1_1' (0x2F) from dts");
		}
		dev_info(dev, "dts config: a0_0_a1_1 -> %d [0x2F -> %s]", pdata->input_code_in_dts[1], touch_point_hub_get_key_name(pdata->input_code_in_dts[1]));

		if (of_property_read_u32(node, "linux,input-code-a0_1_a1_0", &pdata->input_code_in_dts[2])) {
			dev_warn(dev, "unable to read 'linux,input-code-a0_1_a1_0' (0x37) from dts");
		}
		dev_info(dev, "dts config: a0_1_a1_0 -> %d [0x37 -> %s]", pdata->input_code_in_dts[2], touch_point_hub_get_key_name(pdata->input_code_in_dts[2]));

		if (of_property_read_u32(node, "linux,input-code-a0_1_a1_1", &pdata->input_code_in_dts[3])) {
			dev_warn(dev, "unable to read 'linux,input-code-a0_1_a1_1' (0x3F) from dts");
		}
		dev_info(dev, "dts config: a0_1_a1_1 -> %d [0x3F -> %s]", pdata->input_code_in_dts[3], touch_point_hub_get_key_name(pdata->input_code_in_dts[3]));

#ifdef CONFIG_MACH_LAHAINA_BLM
        // Added by LGE for BLM (hyunwoo8.kim@lge.com)
        // If the revision of blm is not latest and MCU mode is set, just change the chip mode to sensor mode.
        dev_info(dev, "check revision to change chip mode: rev=%s, subrev=%s", lge_get_board_revision(), lge_get_board_subrevision());
        if (!(lge_get_board_rev_no() == HW_REV_1_0 && (lge_get_board_subrev_no() == HW_SUB_REV_1 || lge_get_board_subrev_no() == HW_SUB_REV_2) )) {
            if (pdata->chip_mode_in_dts == 3){
                pdata->chip_mode_in_dts = 0;
                dev_info(dev, "changed chip mode setting: MCU mode -> Sensor mode");
            }
        }
        if (lge_get_board_rev_no() == HW_REV_1_0 && lge_get_board_subrev_no() == HW_SUB_REV_0) {
            // If current device is Rev. 1.0, SubRev.0 and BLM,
            // swap the i2c address of the 2nd(0x3F) and 3rd(0x27) side key.
            unsigned int temp_addr = pdata->input_code_in_dts[0];
            pdata->input_code_in_dts[0] = pdata->input_code_in_dts[3];
            pdata->input_code_in_dts[3] = temp_addr;
            dev_info(dev, "Swap the i2c address of 2nd(0x3F) and 3rd(0x27) key.");
            dev_info(dev, "changed dts config: a0_0_a1_0 -> %d [0x27 -> %s]", pdata->input_code_in_dts[0], touch_point_hub_get_key_name(pdata->input_code_in_dts[0]));
            dev_info(dev, "chagned dts config: a0_1_a1_1 -> %d [0x3F -> %s]", pdata->input_code_in_dts[3], touch_point_hub_get_key_name(pdata->input_code_in_dts[3]));
        }
#endif //CONFIG_MACH_LAHAINA_BLM

		/* parse haptic ON/OFF setting */
		if (of_property_read_u32(node, "touch_point_hub,default-haptic-a0_0_a1_0", &pdata->en_haptic[0])) {
			dev_warn(dev, "unable to read 'touch_point_hub,default-haptic-a0_0_a1_0' (0x27) from dts");
		}

		if (of_property_read_u32(node, "touch_point_hub,default-haptic-a0_0_a1_1", &pdata->en_haptic[1])) {
			dev_warn(dev, "unable to read 'touch_point_hub,default-haptic-a0_0_a1_1' (0x2F) from dts");
		}

		if (of_property_read_u32(node, "touch_point_hub,default-haptic-a0_1_a1_0", &pdata->en_haptic[2])) {
			dev_warn(dev, "unable to read 'touch_point_hub,default-haptic-a0_1_a1_0' (0x37) from dts");
		}

		if (of_property_read_u32(node, "touch_point_hub,default-haptic-a0_1_a1_1", &pdata->en_haptic[3])) {
			dev_warn(dev, "unable to read 'touch_point_hub,default-haptic-a0_1_a1_1' (0x3F) from dts");
		}
		dev_info(dev, "en_haptic: [0x3F:%d ,0x37:%d ,0x2F:%d ,0x27:%d]",
				pdata->en_haptic[3],
				pdata->en_haptic[2],
				pdata->en_haptic[1],
				pdata->en_haptic[0]);
	}
	return pdata;
}
#else
static struct touch_point_hub_data * touch_point_hub_parse_dt(struct device *dev)
{
	return -ENODEV;
}
#endif

static int touch_point_hub_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct touch_point_hub_data *pdata;
	struct regulator *regulator;
	char firmware_file_name[128];
	int i = 0, rc = 0;

	dev_info(&client->dev, "ussys touch point hub probe");

	if (ussys_key_cnt > 0) {
		dev_warn(&client->dev, "detected ussys sensor(cnt:%d) on AP, skip hub driver!", ussys_key_cnt);
		return -ENODEV;
	}

	/* parse dts */
	pdata = touch_point_hub_parse_dt(&client->dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	if (pdata->vdd_supply_configured_in_dts) {
		/* request regulator */
		regulator = devm_regulator_get(&client->dev, "touch_point_hub-vdd");
		if (!IS_ERR(regulator)) {
			rc = regulator_enable(regulator);
			if (rc < 0) {
				dev_err(&client->dev, "Failed to enable regulator: %d", rc);
				return rc;
			}
			dev_info(&client->dev, "regulator enable");

			//TODO:remove it due to we couldn't disable regulator for keys which need keep active.
			rc = devm_add_action_or_reset(&client->dev, touch_point_hub_regulator_disable, regulator);
			if (rc)
				return rc;
			mdelay(50);//TODO: could be removed because hub reset will power cycle and wait 50ms again.
		} else {
			dev_info(&client->dev, "regulator error");
		}
	}

	pdata->client = client;
	i2c_set_clientdata(client, pdata);

	mutex_init(&pdata->lock);
	for(i = 0; i < MAX_SENSORS_NUM; i++) {
		strncpy(pdata->sns[i].name, "N/A", MAX_KEY_NAME_LEN - 1);
		pdata->sns[i].name[MAX_KEY_NAME_LEN - 1] = '\0';
	}

	/* init delayed work */
	INIT_DELAYED_WORK(&pdata->delaywork, touch_point_hub_worker);
	INIT_DELAYED_WORK(&pdata->monitor_work, touch_point_hub_monitor_work);

	/* init hrtimer */
	hrtimer_init(&pdata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->timer.function = touch_point_hub_timer_cb;

	/* register hub rst pin */
	if (pdata->hub_rst_high_active) {
		pdata->gpiod_hub_rst = devm_gpiod_get(&client->dev, "touch_point_hub,rst", GPIOD_OUT_LOW);
	} else {
		pdata->gpiod_hub_rst = devm_gpiod_get(&client->dev, "touch_point_hub,rst", GPIOD_OUT_HIGH);
	}
	if (IS_ERR(pdata->gpiod_hub_rst)) {
		dev_err(&client->dev, "get hub rst gpio failed");
		pdata->gpiod_hub_rst = NULL;//return PTR_ERR(data->gpiod_hub_rst);
	}

	/* register sns rst pin */
	if (pdata->sns_rst_high_active) {
		pdata->gpiod_sns_rst = devm_gpiod_get(&client->dev, "touch_point_sns,rst", GPIOD_OUT_LOW);
	} else {
		pdata->gpiod_sns_rst = devm_gpiod_get(&client->dev, "touch_point_sns,rst", GPIOD_OUT_HIGH);
	}
	if (IS_ERR(pdata->gpiod_sns_rst)) {
		dev_err(&client->dev, "get sns rst gpio failed");
		pdata->gpiod_sns_rst = NULL;//return PTR_ERR(data->gpiod_sns_rst);
	}

	/* reset chip */
	rc = touch_point_hub_reset(pdata);
	if (rc < 0)
	{
		dev_warn(&client->dev, "reset chip failed");
		//return rc;
	}
	//mdelay(50);//it should be safe to remove this delay for hub/sns reset.

	/* register irq gpio */
	pdata->gpiod_irq = devm_gpiod_get(&client->dev, "touch_point_hub,irq", GPIOD_IN);
	if (IS_ERR(pdata->gpiod_irq)) {
		dev_err(&client->dev, "get gpio failed");
		return PTR_ERR(pdata->gpiod_irq);
	}

	/* create sysfs group */
	rc = sysfs_create_group(&client->dev.kobj, &touch_point_hub_attr_group);
	if (rc < 0) {
		dev_err(&client->dev, "Failure %d creating sysfs group", rc);
		return rc;
	}
	rc = devm_add_action_or_reset(&client->dev, touch_point_hub_remove_group, &client->dev);
	if (rc < 0)
		return rc;

	rc = touch_point_setup_input_dev(pdata);
	if (rc < 0)
		return rc;

	/* register misc dev */
	pdata->misc_dev.fops = &touch_point_hub_misc_dev_fops;
	pdata->misc_dev.minor = MISC_DYNAMIC_MINOR;
	pdata->misc_dev.name = client->name;//misc_dev_name
#if 1//register misc dev after hub initialization.
	rc = misc_register(&pdata->misc_dev);
	if (rc) {
		dev_err(&client->dev, "register misc dev failed");
		return rc;
	}
	dev_info(&client->dev, "miscdev minor:%d", pdata->misc_dev.minor);

	rc = devm_add_action_or_reset(&client->dev, touch_point_hub_dereg_misc_dev, &pdata->misc_dev);
	if (rc)
		return rc;
#endif

	/* create workqueue */
	pdata->touch_point_hub_wq = create_singlethread_workqueue("touch_point_hub");
	if (!pdata->touch_point_hub_wq)
		return -ENOMEM;
	rc = devm_add_action_or_reset(&client->dev, touch_point_hub_destroy_wq, pdata->touch_point_hub_wq);
	if (rc)
		return rc;

	/* register drm or fb notification */
#if ENABLE_MSM_DRM_NOTIFIER
	rc = touch_point_hub_register_drm(pdata);
	if (rc) {
		dev_err(&client->dev, "register drm notifier failed");
		return rc;
	}
	rc = devm_add_action_or_reset(&client->dev, touch_point_hub_unregister_drm, pdata);
	if (rc)
		return rc;
#else
#ifdef CONFIG_FB
	rc = touch_point_hub_register_fb(pdata);
	if (rc) {
		dev_err(&client->dev, "register fb notifier failed");
		return rc;
	}
	rc = devm_add_action_or_reset(&client->dev, touch_point_hub_unregister_fb, pdata);
	if (rc)
		return rc;
#endif/*CONFIG_FB*/
#endif/*ENABLE_MSM_DRM_NOTIFIER*/

#if 0//register misc dev after hub initialization.
	/* register irq */
	rc  = devm_request_irq(&client->dev, client->irq, touch_point_hub_irq_handler,
							0, client->name, pdata);
	if (rc < 0) {
		dev_err(&client->dev, "request irq failed");
		return rc;
	}
	disable_irq(client->irq);
#endif

#if 0
	//TODO: move to the moment after hub fw is downloaded.
	hrtimer_start(&pdata->timer, ktime_set(0, TOUCH_POINT_HUB_INIT_DONE_WAIT_TIME_US * NSEC_PER_USEC), HRTIMER_MODE_REL);
	pdata->timer_start_ts = ktime_get_boottime();
#endif

	/* add++ for bootloader access */
	pdata->bl_i2c_addr = TOUCH_POINT_HUB_BL_I2C_ADDR;
	/* add-- for bootloader access */

	/* request firmware */
	dev_info(&client->dev, "req hub firmware");
	strcpy(firmware_file_name, TOUCH_POINT_HUB_FIRMWARE_NAME);
#ifdef CONFIG_MACH_LAHAINA_BLM
	if ( lge_get_sku_carrier() == HW_SKU_NA_CDMA_VZW ) {
		// If current device has the mixed button area for mmWave of VZW,
		// Change the FW file name for its algorithm.
		strcpy(firmware_file_name, TOUCH_POINT_HUB_FIRMWARE_NAME_BLM_NA);
		dev_info(&client->dev, "This device is BLM_NA, FW name=%s", firmware_file_name);
	}
#endif
	
	rc = request_firmware_nowait(THIS_MODULE, true, firmware_file_name,
					&client->dev, GFP_KERNEL, pdata, touch_point_hub_fw_dl_cb);
	if (rc) {
		dev_err(&client->dev, "req hub firmware failed");
		return rc;
	}

	dev_info(&client->dev, "probe done");
	return 0;
}

void touch_point_hub_shutdown(struct i2c_client *client)
{
	struct touch_point_hub_data *pdata = i2c_get_clientdata(client);
	enum touch_point_sns_lpm_odr poweroff_odr_value = LPM_ODR_5HZ;
	int i = 0;
	int rc = 0;

	rc = touch_point_hub_wakeup(pdata);
	if (rc < 0)
		return;
	#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
	for (i = 0; i < pdata->sns_cnt; i++) {
		rc = touch_point_sns_put_to_hold(pdata, i, true);
		if (rc < 0)
			return;
	}
	#endif
	rc = touch_point_hub_set_algo_mode(pdata, HUB_USP_ONLY_ALGO_MODE);
	if (rc < 0)
		return;
	for (i = 0; i < pdata->sns_cnt; i++) {
		rc = touch_point_sns_toggle_int0(pdata, i);
		if (rc < 0)
			return;
		rc = touch_point_sns_hpm_odr_sel(pdata, i, HPM_ODR_25HZ);
		if (rc < 0)
			return;
		rc = touch_point_sns_lpm_odr_sel(pdata, i, poweroff_odr_value);
		if (rc < 0)
			return;
		rc = touch_point_sns_enable_lpm(pdata, i, true);
		if (rc < 0)
			return;
	}
	#if ENABLE_SNS_FW_HOLD_VIA_FD_REG
	for (i = 0; i < pdata->sns_cnt; i++) {
		rc = touch_point_sns_put_to_hold(pdata, i, false);
		if (rc < 0)
			return;
	}
	#endif
	rc = touch_point_hub_notify_ap_poweroff(pdata);
	if (rc < 0)
		return;
	dev_info(&client->dev, "touch_point_hub_shutdown\n");
	//dump_stack();
}

#if ENABLE_SUSPEND_RESUME
static SIMPLE_DEV_PM_OPS(touch_point_hub_pm_ops, touch_point_hub_suspend, touch_point_hub_resume);
#endif

static const struct i2c_device_id touch_point_hub_id_table[] = {
	{"touch_point_hub", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, touch_point_hub_id_table);

static struct i2c_driver touch_point_hub_i2c_driver = {
	.driver = {
		.name = "touch_point_hub",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = touch_point_hub_of_id_table,
#endif
#if ENABLE_SUSPEND_RESUME
		.pm = &touch_point_hub_pm_ops,
#endif
	},
	.probe = touch_point_hub_probe,
	.shutdown = touch_point_hub_shutdown,
	.id_table = touch_point_hub_id_table,
};

module_i2c_driver(touch_point_hub_i2c_driver);
