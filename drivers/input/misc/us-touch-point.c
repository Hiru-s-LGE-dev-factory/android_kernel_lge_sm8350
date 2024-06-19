/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#include <linux/firmware.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/input.h>
#include <linux/input/us-touch-point.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/fs.h>
#include <linux/version.h>
//#include <linux/msm_drm_notify.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0))
#define KERNEL_BELOW_3_19
#endif

#ifndef UNUSED_PARAM
#define UNUSED_PARAM(v) (void)(v)
#endif

#define ENABLE_DUMMY_DRIVER (0)

#define USSYS_TOUCH_POINT_A0_WHOAMI (0x62)
#define USSYS_TOUCH_POINT_A2_WHOAMI (0x64)

#define TOUCH_POINT_DSRAM_SI_REV				0x13F0
#define TOUCH_POINT_FORCE_RUNNING_RST_REG		0x18FA
	#define REG_FORCE_RST_BIT_SET_I2CSPI_CLOCK_20MHZ  0x01
	#define REG_FORCE_RST_BIT_FORCE_I2CBKDR_MODE      0x20
	#define REG_FORCE_RST_BIT_ENB_BCKEND_SPI          0x40
	#define REG_FORCE_RST_BIT_FORCE_BRAHM_RST         0x80

#define TOUCH_POINT_USP_ON_OFF_MODE_REG			0x18E7
	#define REG_USP_ON_OFF_MODE_REG_BIT_RELEASE_BRAHM_RST      0x01

#define TOUCH_POINT_INT_HW_RESET				0x18ED
#define TOUCH_POINT_BD_WHOAMI_REG     			0x18FD

#define TOUCH_POINT_FIRMWARE_NAME "touch_point.fw"

#define TOUCH_POINT_REG_PAGE_ADDR			0x1100
#define TOUCH_POINT_REG_ADC 				(TOUCH_POINT_REG_PAGE_ADDR + 0x00)
#define TOUCH_POINT_REG_ADC_32X				(TOUCH_POINT_REG_PAGE_ADDR + 0x02)
#define TOUCH_POINT_REG_ALGO_STATUS			(TOUCH_POINT_REG_PAGE_ADDR + 0x0E)
#define TOUCH_POINT_SENSOR_CTRL				(TOUCH_POINT_REG_PAGE_ADDR + 0x11)
	#define TOUCH_POINT_REG_SENSOR_CTRL_HOLDING_MASK	(0x02)
	#define TOUCH_POINT_REG_SENSOR_CTRL_LPM_MASK		(0x08)
	#define TOUCH_POINT_REG_SENSOR_CTRL_FW_FLAG_MASK	(0x80)
#define TOUCH_POINT_ADC_AVG_TIMES			(TOUCH_POINT_REG_PAGE_ADDR + 0x12)
#define TOUCH_POINT_LPM_ODR_SEL             (TOUCH_POINT_REG_PAGE_ADDR + 0x7A)
#define TOUCH_POINT_HPM_ODR_SEL             (TOUCH_POINT_REG_PAGE_ADDR + 0x7C)
#define TOUCH_POINT_ALGO_THS_START_ADDR		(TOUCH_POINT_REG_PAGE_ADDR + 0x50)	//Algo threshold start addr
#define TOUCH_POINT_SWREG_VERBYTE1_REVC		(TOUCH_POINT_REG_PAGE_ADDR + 0x7E)

#define TOUCH_POINT_INT_MASK				0x52

#define TOUCH_POINT_REG_ADDR_INDEX_H			0x60
#define TOUCH_POINT_REG_ADDR_INDEX_L			0x61
#define TOUCH_POINT_REG_LENGHT_H			0x62
#define TOUCH_POINT_REG_LENGHT_L			0x63
#define TOUCH_POINT_REG_WHOAMI				0x6E
#define TOUCH_POINT_REG_BURST_WRITE			0x70
#define TOUCH_POINT_REG_BURST_READ			0xF0

/* OTP Mem Dump */
#define TOUCH_POINT_OTP_MEM_START_ADDR		(0x1A00)
#define TOUCH_POINT_OTP_MEM_END_ADDR		(0x29FF)//(0x1AFF)
#define TOUCH_POINT_OTP_MEM_SIZE			(TOUCH_POINT_OTP_MEM_END_ADDR - TOUCH_POINT_OTP_MEM_START_ADDR + 1)
#define TOUCH_POINT_OTP_MEM_FW_START_ADDR	(0x1B00)

/* OTP ZForce */
#define TOUCH_POINT_OTP_ZForce_START_ADDR		(0x1A16)
#define TOUCH_POINT_OTP_ZForce_END_ADDR		(0x1A35)
#define TOUCH_POINT_OTP_ZForce_SIZE			(TOUCH_POINT_OTP_ZForce_END_ADDR - TOUCH_POINT_OTP_ZForce_START_ADDR + 1)
/* OTP USP Bank0 */
#define TOUCH_POINT_OTP_BANK0_START_ADDR	(0x1A66)
#define TOUCH_POINT_OTP_BANK0_END_ADDR		(0x1A79)
#define TOUCH_POINT_OTP_BANK0_SIZE			(TOUCH_POINT_OTP_BANK0_END_ADDR - TOUCH_POINT_OTP_BANK0_START_ADDR + 1)
/* OTP USP Bank1 */
#define TOUCH_POINT_OTP_BANK1_START_ADDR	(0x1AA6)
#define TOUCH_POINT_OTP_BANK1_END_ADDR		(0x1AB9)
#define TOUCH_POINT_OTP_BANK1_SIZE			(TOUCH_POINT_OTP_BANK1_END_ADDR - TOUCH_POINT_OTP_BANK1_START_ADDR + 1)
/* OTP IDs */
#define TOUCH_POINT_OTP_LOTID_H_ADDR		(0x1AE7)
#define TOUCH_POINT_OTP_LOTID_L_ADDR		(0x1AE8)
#define TOUCH_POINT_OTP_WAFERID_ADDR		(0x1AE9)
#define TOUCH_POINT_OTP_CPFT_VER_ADDR		(0x1AEA)
#define TOUCH_POINT_OTP_STACKID_MSB_ADDR	(0x1AED)
#define TOUCH_POINT_OTP_STACKID_LSB_ADDR	(0x1AEE)
#define TOUCH_POINT_OTP_OTP_VER_ADDR		(0x1AEF)
#define TOUCH_POINT_OTP_SIREV_OLD_ADDR		(0x1AF0)
#define TOUCH_POINT_OTP_FW_VER_H_ADDR		(0x1AF1)
#define TOUCH_POINT_OTP_FW_VER_L_ADDR		(0x1AF2)
#define TOUCH_POINT_OTP_TIMEOFF_H_ADDR		(0x1AF3)
#define TOUCH_POINT_OTP_TIMEOFF_L_ADDR		(0x1AF4)
#define TOUCH_POINT_OTP_CHIP_ADDR			(0x1AF5)
#define TOUCH_POINT_OTP_SIREV_NEW_ADDR		(0x1AF6)
#define TOUCH_POINT_OTP_DIEID_R_H_ADDR		(0x1AF7)
#define TOUCH_POINT_OTP_DIEID_R_L_ADDR		(0x1AF8)
#define TOUCH_POINT_OTP_DIEID_C_H_ADDR		(0x1AF9)
#define TOUCH_POINT_OTP_DIEID_C_L_ADDR		(0x1AFA)

/* Convert OTP addr to OTP buffer index */
#define TOUCH_POINT_OTPADDR2IDX(addr)		(addr - TOUCH_POINT_OTP_MEM_START_ADDR)

/* FW Ver */
#define TOUCH_POINT_FW_VER_DATE_BYTES		(12)
#define TOUCH_POINT_FW_VER_TIME_BYTES		(9)

/* use 1000 ms for toggle gap */
#define TOUCH_POINT_TOGGLE_INT_PIN_GAP		(1000UL)

/* set recovery rejection time */
#define TOUCH_POINT_RECOVERY_REJECTION_TIME_MS	(10000UL)

/* MAJOR.MINOR.PATCH */
#define TOUCH_POINT_VERSION "2.1.5"

MODULE_VERSION(TOUCH_POINT_VERSION);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("USSYS TouchPoint chip driver");
MODULE_AUTHOR("UltraSensor Corporation");

#define ENABLE_EXTERNAL_RESET_PIN	(1)
/**
 * DRM: Direct Rendering Manager
 * If set to 1, get display ON/OFF notification from DRM
 * If set to 0, get display ON/OFF notification from framebuffer(N/A for now).
 */
#define USING_MSM_DRM_NOTIFIER (0)

/**
 * By default, Chip control power mode by itself, if want to use suspend/resume to control this, pls set to 1
 */
#define USING_DRIVER_TO_CTL_POWER (0)

/**
 * sometimes, gpio line is locked to IRQ, if driver set this kind GPIO to output
 * Linux subsystem will refuse this, so we need to unlock first, then operate
 * GPIO values, and then back to lock it to IRQ.
 * This two function gpiochip_unlock_as_irq/gpiochip_lock_as_irq is desinged
 * for GPIO drivers, so this is not the best way, we may also need to consider to use
 * free_irq and request again.
 */
#define OPERATE_IRQ_GPIO_LINE (1)

#if OPERATE_IRQ_GPIO_LINE
#include "../../../drivers/gpio/gpiolib.h"
#endif

enum ussys_tp_key_idx {
	USSYS_KEY_0,
	USSYS_KEY_1,
	USSYS_KEY_2,
	USSYS_KEY_3,
	USSYS_KEY_MAX
};

enum ussys_tp_hpm_odr {
	HPM_ODR_200HZ = 0x0000,
	HPM_ODR_100HZ = 0x208D,
	HPM_ODR_75HZ = 0x3640,
	HPM_ODR_50HZ = 0x61A8,
	HPM_ODR_25HZ = 0xE3DD,
};

enum ussys_tp_lpm_odr {
	LPM_ODR_50HZ = 0x0000,
	LPM_ODR_25HZ = 0x0035,
	LPM_ODR_10HZ = 0x00D5,
	LPM_ODR_5HZ = 0x01E0,
	LPM_ODR_01HZ = 0x682A, // 1/10s for Sleep mode
};

enum ussys_parts_fac_status {
	FAC_NO_OTP = 0,
	FAC_W_OTP_WO_FW = 1,
	FAC_W_OTP_W_FW = 2,
};

typedef struct ZForceAlgo1stGlobalTHS {
	//Global THS
    
    //USP Part
	uint8_t		I500uspLPF32WeightHPM;  				//USPLPF32WEIGHTHPM
    uint8_t		I505uspLPF32WeightLPM;  				//USPLPF32WEIGHTLPM
	uint8_t     I508uspOffsetLPF32WEIGHTHPM;            //USPOffsetLPF32WEIGHTHPM
	uint8_t     I510uspOffsetLPF32WEIGHTLPM;            //USPOffsetLPF32WEIGHTLPM
	uint8_t		I520uspDcmtCntTHS;						//USPWINDecm
	uint8_t		I540uspMinDepthTHSOF256;	    		//USPMINDepthTHSOF256
    uint8_t     I560uspMaxContrast;                     //max contrast of this sensor   USPTYPContrast
	uint8_t		I570uspInputNoiseLPF256WEIGHT;			//USPInputNoiseLPF256WEIGHT
	uint8_t     I575uspInputNoiseAmpOver4AsStationary;	//USPInputNoiseAmpOver4AsStatic
    uint8_t     I580uspNoiseRMS;                        //noise RMS collected.          USPTYPNoiseRMS
	uint8_t		I585uspDepthTHSJitterOver4;				//USPDeptchTHSJitterOver4
	uint8_t		I590uspRisingEdgeTHSDiscOver32;			//USPRisingEdgeTHSDiscOver32
	uint8_t		I595uspFallingEdgeTHSDiscOver32;		//USPFallingEdgeTHSDiscOver32
        
    
    //ZForce Part 
	uint8_t     I700ZForceSumLPF32WEIGTH;					//ZForceLPF32WEIGTH
	uint8_t		I710ZForceSumOffsetTrack256WEIGTH;		//ZForceOFFSETTRACKLPF256WEIGHT
    uint8_t     I720ZForceOffsetAbsTolRange;              //ZForceOffsetAbsTolRange
	uint8_t		I770ZForceSumPlusTHSOF256;			    //ZForceSumPlusTHSOF256
	uint8_t		I780ZForceSumMinusTHSOF256;				//ZForceSumMinusTHSOF256
    uint8_t     I800ZForceSumP2PValueAtStdForcePtn;       //ZForceSumValue at standard force 3Hz 7.5N  ZForceSumP2PAtStdF
    uint8_t     I810ZForceSumNoiseRMS;                    //noise RMS for ZForce.	ZForceSumNoiseRMS
	uint8_t		I815ZForceSumMinuxPeakExpiredCNTTime;		//ZForceSumMinuxPeakExpiredCNTTime
	uint8_t		I820ZForceSumPeakTipJitterOver4;			//ZForceSumPeakTipJitterOver4
    
    //overAll
    uint8_t     I980AlgoSensitivityLevel;               //1~10 DEFSensLevel10
    uint8_t     I990HPMCntDownResetValueD64;            //HPMCntDownResetValueD64
	uint8_t     I995AlgoMode;							//0,1,2,3  ALGOMode
    
} __attribute__((packed)) ZForceAlgo1stGlobalTHSTyp;

struct touch_point_data {
	struct i2c_client *client;
	struct hrtimer timer;
	struct delayed_work delaywork;
	struct workqueue_struct *touch_point_wq;
	struct completion read_data_completion;
	struct mutex lock;
	struct mutex i2c_lock;
	struct mutex fd_lock;
	struct regulator *vdd;
	struct input_dev *input_dev;
	struct miscdevice misc_dev;
	struct gpio_desc *gpiod_irq;
	struct gpio_desc *gpiod_rst;
	enum ussys_tp_key_idx key_idx;
	enum ussys_parts_fac_status fac_status;
	int miscdev_minor;
	bool share_irq_pin;
	bool lpm_on;
	bool wakeup;
	u16 reg_to_read;
	u16 size_to_read;
	s32 miscdev_count;
	u8 miscdev_key_idx;
	unsigned int code;
	bool key_pressed;
	u8 event_cnt;

	/* to prevent toggling INT PIN too much */
	ktime_t toggle_ts;

	/*i2c FD cache read/write*/
	u8 host_index_addr_h;
	u8 host_index_addr_l;

#if USING_MSM_DRM_NOTIFIER
	struct notifier_block drm_notif;/*Register DRM notifier*/
#else
#ifdef CONFIG_FB
	struct notifier_block fb_notif;/*Register FB notifier*/
#endif
#endif/*USING_MSM_DRM_NOTIFIER*/

	u8 *otp_mem;
	bool is_cali_ongoing;
	struct fasync_struct *fasync;
	uint8_t recovery_data;

	/* fw ver */
	uint8_t ver_date[TOUCH_POINT_FW_VER_DATE_BYTES];
	uint8_t ver_time[TOUCH_POINT_FW_VER_TIME_BYTES];
};

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

/* The max supported key is 4 */
struct touch_point_data *tp_g_data[USSYS_KEY_MAX] = {NULL};
static bool is_shared_gpio_requested = false;
static struct gpio_desc *shared_gpiod_irq = NULL;
static uint8_t key_cnt = 0;
static ktime_t toggle_ts;
static DEFINE_MUTEX(recovery_lock);
/* For chip recovery */
static bool is_rst_pin_initialized = false;
static struct gpio_desc *shared_gpiod_rst = NULL;
static ktime_t recovery_ts;
uint32_t ussys_key_cnt = 0;
EXPORT_SYMBOL(ussys_key_cnt);

#define I2C_MAX_RETRY_NUM (3)
/*
 * Limit i2c r/w max length to 256bytes, currently only applied to FW download.
 * TODO: apply it to all i2c r/w interfaces.
 */
#define I2C_RW_MAX_LEN (32) //256
/*
 * Once chip i2c timeout feature enabled, we should set 1 here,
 * otherwise chip i2c timeout may be triggered due to platform clock stretch.
 */
#define I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION (0)

/**
 * Get key name by key input code in dts.
 * This key mapping only affects touch point test app key name, should keep this key mapping same as touch_point.kl
 */
static const char* touch_point_get_key_name(unsigned int code)
{
	if (KEY_VOLUMEUP == code)
		return "Vol+";//VOLUME_UP
	else if (KEY_VOLUMEDOWN == code)
		return "Vol-";//VOLUME_DOWN
	else if (KEY_HOME == code)
		return "Home";//HOME
	else if (KEY_POWER == code)
		return "Power";//POWER
	else
		return "Unknown";
}

#if ENABLE_DUMMY_DRIVER
static int touch_point_bd_write(struct touch_point_data *pdata, u16 reg, u8 *val, u16 size)
{
	UNUSED_PARAM(pdata);
	UNUSED_PARAM(reg);
	UNUSED_PARAM(val);
	UNUSED_PARAM(size);
	return 0;
}

static int touch_point_bd_read(struct touch_point_data *pdata, u16 reg, char *val, u16 size)
{
	int i = 0;

	UNUSED_PARAM(pdata);
	UNUSED_PARAM(reg);

	for (i = 0; i < size; i++)
	{
		val[i] = i+1;
	}
	return 0;
}

static int touch_point_fd_direct_write(struct touch_point_data *pdata, u8 reg, u8 *val, u16 len)
{
	UNUSED_PARAM(pdata);
	UNUSED_PARAM(reg);
	UNUSED_PARAM(val);
	UNUSED_PARAM(len);
	return 0;
}

static int touch_point_fd_direct_read(struct touch_point_data *pdata, u8 reg, u8 *val, u16 len)
{
	int i = 0;

	UNUSED_PARAM(pdata);
	UNUSED_PARAM(reg);

	for (i = 0; i < len; i++)
	{
		val[i] = i+1;
	}
	return 0;
}

static int touch_point_write_16bit_reg(struct touch_point_data *pdata, u16 reg, u8 *val, u16 len)
{
	UNUSED_PARAM(pdata);
	UNUSED_PARAM(reg);
	UNUSED_PARAM(val);
	UNUSED_PARAM(len);
	return 0;
}

static int touch_point_read_16bit_reg(struct touch_point_data *pdata, u16 reg, u8 *buf, u16 len)
{
	int i = 0;

	UNUSED_PARAM(pdata);
	UNUSED_PARAM(reg);

	for (i = 0; i < len; i++)
	{
		buf[i] = i+1;
	}
	return 0;
}
#else
static int touch_point_bd_write(struct touch_point_data *pdata, u16 reg, u8 *val, u16 size)
{
	struct i2c_client *client = pdata->client;
	u8 *buf = kzalloc(size + 5, GFP_KERNEL);
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = size + 5,
		.buf = buf,
	};

	if (NULL == buf)
		return -ENOMEM;

	buf[0] = 0x02;
	buf[1] = (u8)((reg & 0xff00) >> 8);
	buf[2] = (u8)(reg & 0x00ff);
	buf[3] = (u8)((size & 0xff00) >> 8);
	buf[4] = (u8)(size & 0x00ff);
	memcpy(&buf[5], val, size);

	do {
	    rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		i2c_retry_cnt++;
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	kfree(buf);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&client->dev, "i2c write 0x%x failed ", reg);
		return -EIO;
	}

	return 0;
}

static int touch_point_bd_read(struct touch_point_data *pdata, u16 reg, char *val, u16 size)
{
	struct i2c_client *client = pdata->client;
	u8 buf[5] = {0x01, (reg & 0xff00) >> 8, reg & 0x00ff, (size & 0xff00) >> 8, size & 0x00ff};
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 5,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = size,
			.buf = val,
		}
	};

	do {
#if I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION
		int i = 0;

		mutex_lock(&pdata->i2c_lock);
		for (i = 0; i < ARRAY_SIZE(msg); i++)
		{
			rc = i2c_transfer(client->adapter, &msg[i], 1);
			if (rc != 1)
				break;
		}
		mutex_unlock(&pdata->i2c_lock);

		if (i == ARRAY_SIZE(msg))
			break;
#else
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
			break;
#endif/*I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION*/
		i2c_retry_cnt++;
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
		dev_err(&client->dev, "i2c read 0x%x failed ", reg);
		return -EIO;
	}

	return 0;
}

static int touch_point_fd_direct_write(struct touch_point_data *pdata, u8 reg, u8 *val, u16 len)
{
	struct i2c_client *client = pdata->client;
	u8 *buf = kzalloc(len + 1, GFP_KERNEL);
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg = {
		.addr = client->addr,
		.flags = 0,
		.len = len + 1,
		.buf = buf,
	};

	if (NULL == buf)
	    return -ENOMEM;

	buf[0] = reg;
	memcpy(&buf[1], val, len);

	do {
		rc = i2c_transfer(client->adapter, &msg, 1);
		if (rc == 1)
		    break;
		mdelay(1);//in case i2c timeout also occur, wait until i2c recover from i2c timeout.
		i2c_retry_cnt++;
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	kfree(buf);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "i2c write 0x%02x failed ", reg);
		return -EIO;
    }

	return 0;
}

static int touch_point_fd_direct_read(struct touch_point_data *pdata, u8 reg, u8 *val, u16 len)
{
	struct i2c_client *client = pdata->client;
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = val,
		}
	};

	do {
#if I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION
		int i = 0;

		mutex_lock(&pdata->i2c_lock);
		for (i = 0; i < ARRAY_SIZE(msg); i++)
		{
			rc = i2c_transfer(client->adapter, &msg[i], 1);
			if (rc != 1)
			{
				mdelay(1);//in case i2c timeout also occur, wait until i2c recover from i2c timeout.
			    break;
			}
		}
		mutex_unlock(&pdata->i2c_lock);

		if (i == ARRAY_SIZE(msg))
			break;
#else
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
			break;
		mdelay(1);//in case i2c timeout also occur, wait until i2c recover from i2c timeout.
#endif/*I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION*/
		i2c_retry_cnt++;
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
		dev_err(&client->dev, "i2c read 0x%x failed ", reg);
		return -EIO;
	}

	return 0;
}

static int touch_point_write_16bit_reg(struct touch_point_data *pdata, u16 reg, u8 *val, u16 len)
{
	struct i2c_client *client = pdata->client;
	u8 addr_h[2] = {TOUCH_POINT_REG_ADDR_INDEX_H, (u8)(reg >> 8)};
	u8 addr_l[2] = {TOUCH_POINT_REG_ADDR_INDEX_L, (u8)reg};
	u8 *buf = kzalloc(len + 1, GFP_KERNEL);
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = addr_h,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = addr_l,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = len + 1,
			.buf = buf,
		},
	};
	buf[0] = TOUCH_POINT_REG_BURST_WRITE;
	memcpy(&buf[1], val, len);

	do {
#if I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION
		int i = 0;

		mutex_lock(&pdata->i2c_lock);
		for (i = 0; i < ARRAY_SIZE(msg); i++)
		{
			rc = i2c_transfer(client->adapter, &msg[i], 1);
			if (rc != 1)
				break;
		}
		mutex_unlock(&pdata->i2c_lock);

		if (i == ARRAY_SIZE(msg))
			break;
#else
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
			break;
#endif/*I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION*/
		i2c_retry_cnt++;
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	kfree(buf);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
		dev_err(&pdata->client->dev, "i2c write 0x%02x failed ", reg);
		return -EIO;
	}

	return 0;
}

static int touch_point_read_16bit_reg(struct touch_point_data *pdata, u16 reg, u8 *buf, u16 len)
{
	struct i2c_client *client = pdata->client;
	u8 addr_h[2] = {TOUCH_POINT_REG_ADDR_INDEX_H, (u8)(reg >> 8)};
	u8 addr_l[2] = {TOUCH_POINT_REG_ADDR_INDEX_L, (u8)reg};
	u8 data = TOUCH_POINT_REG_BURST_READ;
	int rc = 0, i2c_retry_cnt = 0;

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = addr_h,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = addr_l,
		},
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &data,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	do {
#if I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION
		int i = 0;

		mutex_lock(&pdata->i2c_lock);
		for (i = 0; i < ARRAY_SIZE(msg); i++)
		{
			rc = i2c_transfer(client->adapter, &msg[i], 1);
			if (rc != 1)
				break;
		}
		mutex_unlock(&pdata->i2c_lock);

		if (i == ARRAY_SIZE(msg))
			break;
#else
		rc = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
		if (rc == ARRAY_SIZE(msg))
			break;
#endif/*I2C_FORCE_SINGLE_MSG_IN_ONE_TRANSACTION*/
		i2c_retry_cnt++;
	} while (i2c_retry_cnt < I2C_MAX_RETRY_NUM);

	if (i2c_retry_cnt == I2C_MAX_RETRY_NUM) {
	    dev_err(&pdata->client->dev, "i2c read 0x%02x failed", reg);
		return -EIO;
	}

	return 0;
}
#endif/*ENABLE_DUMMY_DRIVER*/

/*
static int touch_point_bd_write_byte(struct touch_point_data *pdata, u8 reg, u8 data)
{
	return touch_point_bd_write(pdata, reg | 0x1800, &data, 1);
}
*/

static int touch_point_bd_write_reg_mask(struct touch_point_data *pdata, u16 reg, u8 mask, u8 val)
{
	int rc = 0;
	u8 reg_val = 0;

	rc += touch_point_bd_read(pdata, reg, &reg_val, 1);
	reg_val &= (~mask);
	reg_val |= (val & mask);
	rc += touch_point_bd_write(pdata, reg, &reg_val, 1);

	return rc;
}

static int touch_point_write_mask(struct touch_point_data *pdata, u16 reg, u8 mask, u8 val)
{
	int rc = 0;
	u8 orig = 0, tmp;

	rc = touch_point_read_16bit_reg(pdata, reg, &orig, 1);
	if (rc != 0)
		return rc;

	tmp = orig & ~mask;
	tmp |= val & mask;

	rc = touch_point_write_16bit_reg(pdata, reg, &tmp, 1);
	return rc;
}

static int touch_point_toggle_syncio_pin(struct touch_point_data *pdata)
{
	int rc = 0;
	ktime_t toggle_ts_diff = ktime_set(0,0);
	ktime_t current_ts = ktime_get_boottime();

#if OPERATE_IRQ_GPIO_LINE
	struct gpio_chip	*chip;
	unsigned		offset;
	bool gpio_locked_as_irq = false;
#endif

	dev_info(&pdata->client->dev, "toggle syncio PIN");

	if (pdata->share_irq_pin)
		toggle_ts_diff = ktime_sub(current_ts, toggle_ts);
	else
		toggle_ts_diff = ktime_sub(current_ts, pdata->toggle_ts);

	if (ktime_after(ms_to_ktime(TOUCH_POINT_TOGGLE_INT_PIN_GAP), toggle_ts_diff)) {
		dev_warn(&pdata->client->dev, "between toggling gap, ignore this one");
		return 0;//return rc;
	}

#if OPERATE_IRQ_GPIO_LINE
	chip = gpiod_to_chip(pdata->gpiod_irq);
	#ifdef KERNEL_BELOW_3_19
	offset = desc_to_gpio(pdata->gpiod_irq);
	gpio_locked_as_irq = test_bit(FLAG_USED_AS_IRQ, &chip->desc[offset].flags);
	#else
	offset = gpio_chip_hwgpio(pdata->gpiod_irq);
	gpio_locked_as_irq = gpiochip_line_is_irq(chip, offset);
	#endif

	dev_info(&pdata->client->dev, "gpio_locked_as_irq %d", gpio_locked_as_irq);

	#ifdef KERNEL_BELOW_3_19
	if (gpio_locked_as_irq)
		gpio_unlock_as_irq(chip, offset);
	#else
	if (gpio_locked_as_irq)
		gpiochip_unlock_as_irq(chip, offset);
	#endif
#endif

	//1st toggle
	rc += gpiod_direction_output(pdata->gpiod_irq, 1);
	udelay(40);
	rc += gpiod_direction_output(pdata->gpiod_irq, 0);
	udelay(40);

	//wait some time in case fw is close to enter LPM and not able to check sensor control bit3
	mdelay(10);//4 for B2//TODO:need measure it for C0.

	//2nd toggle in case 1st toggle doesn't take effect.
	rc += gpiod_direction_output(pdata->gpiod_irq, 1);
	udelay(40);
	rc += gpiod_direction_output(pdata->gpiod_irq, 0);
	udelay(40);

	rc += gpiod_direction_output(pdata->gpiod_irq, 1);
	//ensure fw go back to 20MHz
	mdelay(10);//4 for B2//TODO:need measure it for C0.
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

	if (rc)
		dev_err(&pdata->client->dev, "error occurs while toggling syncio pin(rc:%d)", rc);

	pdata->toggle_ts = toggle_ts = ktime_get_boottime();

	return 0;//return rc;
}

static int touch_point_bd_check_whoami(struct touch_point_data *data)
{
	struct i2c_client *client = data->client;
	u8 whoami;
	int rc = 0;

	dev_info(&client->dev, "bd check device whoami");
	rc = touch_point_bd_read(data, TOUCH_POINT_BD_WHOAMI_REG, &whoami, 1);
	if (rc < 0) {
		dev_err(&client->dev, "bd read whoami reg failed");
		return rc;
	}
	dev_info(&client->dev, "bd whoami is 0x%x", whoami);
#if !ENABLE_DUMMY_DRIVER
	if (USSYS_TOUCH_POINT_A0_WHOAMI != whoami && USSYS_TOUCH_POINT_A2_WHOAMI != whoami) {
		dev_err(&client->dev, "get incorrect whoami from bd");
		rc = -1;
	}
#endif

	return rc;
}

static int touch_point_fd_check_whoami(struct touch_point_data *data)
{
	int rc = 0;
	u8 whoami = 0;
	rc = touch_point_fd_direct_read(data, TOUCH_POINT_REG_WHOAMI, &whoami, sizeof(whoami));
	if (rc < 0) {
		dev_err(&data->client->dev, "fd read whoami reg failed");
		return rc;
	}
	dev_info(&data->client->dev, "fd read whoami:%#X", whoami);
#if !ENABLE_DUMMY_DRIVER
	if (USSYS_TOUCH_POINT_A0_WHOAMI != whoami && USSYS_TOUCH_POINT_A2_WHOAMI != whoami) {
		dev_err(&data->client->dev, "get incorrect whoami from fd");
		rc = -1;
	}
#endif

	return rc;
}

static int touch_point_put_to_hold(struct touch_point_data *pdata, bool en)
{
	int rc = 0;
	rc = touch_point_write_mask(pdata, TOUCH_POINT_SENSOR_CTRL,
		TOUCH_POINT_REG_SENSOR_CTRL_HOLDING_MASK, en ? TOUCH_POINT_REG_SENSOR_CTRL_HOLDING_MASK : 0);

	/* Same as SMT32 L4 */
	mdelay(20);

	return rc;
}

static int touch_point_exit_bd_enter_fd(struct touch_point_data *pdata)
{
	return touch_point_bd_write_reg_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, 0x22, 0x02);
}

static int touch_point_exit_fd_enter_bd(struct touch_point_data *pdata)
{
	return touch_point_write_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, 0x22, 0x20);
}

static int touch_point_bd_en_hs_i2c(struct touch_point_data *pdata)
{
    //  always set IIC to 20MHz , even when 20MHz is 0 for LPM , thus means LPM mode need to toggle to wake up first.
    return touch_point_bd_write_reg_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, REG_FORCE_RST_BIT_SET_I2CSPI_CLOCK_20MHZ, REG_FORCE_RST_BIT_SET_I2CSPI_CLOCK_20MHZ);
}

static int touch_point_fd_en_hs_i2c(struct touch_point_data *pdata)
{
    //  always set IIC to 20MHz , even when 20MHz is 0 for LPM , thus means LPM mode need to toggle to wake up first.
    return touch_point_write_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, REG_FORCE_RST_BIT_SET_I2CSPI_CLOCK_20MHZ, REG_FORCE_RST_BIT_SET_I2CSPI_CLOCK_20MHZ);
}

static int touch_point_bd_force_reset_hold(struct touch_point_data *pdata)
{
	int rc = 0;

	rc = touch_point_bd_write_reg_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, REG_FORCE_RST_BIT_FORCE_BRAHM_RST, REG_FORCE_RST_BIT_FORCE_BRAHM_RST);
	if (rc < 0)
		dev_err(&pdata->client->dev, "force rst failed ");

	rc = touch_point_bd_write_reg_mask(pdata, TOUCH_POINT_USP_ON_OFF_MODE_REG, REG_USP_ON_OFF_MODE_REG_BIT_RELEASE_BRAHM_RST, 0);
	if (rc < 0)
		dev_err(&pdata->client->dev, "hold rst failed ");

	return rc;
}

static int touch_point_fd_force_reset_hold(struct touch_point_data *pdata)
{
	int rc = 0;

	rc = touch_point_write_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, REG_FORCE_RST_BIT_FORCE_BRAHM_RST, REG_FORCE_RST_BIT_FORCE_BRAHM_RST);
	if (rc < 0)
		dev_err(&pdata->client->dev, "fd force rst failed ");

	rc = touch_point_write_mask(pdata, TOUCH_POINT_USP_ON_OFF_MODE_REG, REG_USP_ON_OFF_MODE_REG_BIT_RELEASE_BRAHM_RST, 0);
	if (rc < 0)
		dev_err(&pdata->client->dev, "fd hold rst failed ");

	return rc;
}

/*
//BD force release
static int touch_point_bd_force_reset_release(struct touch_point_data *pdata)
{
	int rc = 0;
	u8 data;

	data = 0x80;
	rc = touch_point_bd_write(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, &data, 1);
	if (rc < 0)
		dev_err(&pdata->client->dev, "bd force rst failed ");

	data = 0;
	rc = touch_point_bd_write(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, &data, 1);
	if (rc < 0)
		dev_err(&pdata->client->dev, "bd hold rst failed ");

	data = 0x01;
	rc = touch_point_bd_write(pdata, TOUCH_POINT_USP_ON_OFF_MODE_REG, &data, 1);
	if (rc < 0)
		dev_err(&pdata->client->dev, "bd release rst failed ");

	return rc;
}
*/

//FD force release
static int touch_point_fd_force_reset_release(struct touch_point_data *pdata)
{
	int rc = 0;

	rc = touch_point_write_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, REG_FORCE_RST_BIT_FORCE_BRAHM_RST, REG_FORCE_RST_BIT_FORCE_BRAHM_RST);
	if (rc < 0)
		dev_err(&pdata->client->dev, "fd force rst failed ");

	rc = touch_point_write_mask(pdata, TOUCH_POINT_FORCE_RUNNING_RST_REG, REG_FORCE_RST_BIT_FORCE_BRAHM_RST, 0);
	if (rc < 0)
		dev_err(&pdata->client->dev, "fd hold rst failed ");

	rc = touch_point_write_mask(pdata, TOUCH_POINT_USP_ON_OFF_MODE_REG, REG_USP_ON_OFF_MODE_REG_BIT_RELEASE_BRAHM_RST, REG_USP_ON_OFF_MODE_REG_BIT_RELEASE_BRAHM_RST);
	if (rc < 0)
		dev_err(&pdata->client->dev, "fd release rst failed ");

	return rc;
}

#if ENABLE_EXTERNAL_RESET_PIN
static int touch_point_reset_chip(struct touch_point_data *pdata)
{
	int rc = 0;

	if (NULL != pdata->gpiod_rst) {
		rc += gpiod_direction_output(pdata->gpiod_rst, 1);
		mdelay(10);//TBD
		rc += gpiod_direction_output(pdata->gpiod_rst, 0);
		mdelay(50);//ensure power is stable
	}

	dev_info(&pdata->client->dev, "set reset pin");
	return rc;
}
#else
static int touch_point_reset_chip(struct touch_point_data *pdata)
{
	int rc = 0;
	uint8_t int_rst = 0x05;

	/* keep INT pin >1ms and then pull down INT pin for >20ms */
#if OPERATE_IRQ_GPIO_LINE
	struct gpio_chip	*chip;
	unsigned		offset;
	bool gpio_locked_as_irq = false;
#endif

#if OPERATE_IRQ_GPIO_LINE
	chip = gpiod_to_chip(pdata->gpiod_irq);
	#ifdef KERNEL_BELOW_3_19
	offset = desc_to_gpio(pdata->gpiod_irq);
	gpio_locked_as_irq = test_bit(FLAG_USED_AS_IRQ, &chip->desc[offset].flags);
	#else
	offset = gpio_chip_hwgpio(pdata->gpiod_irq);
	gpio_locked_as_irq = gpiochip_line_is_irq(chip, offset);
	#endif

	dev_info(&pdata->client->dev, "(chip reset)gpio_locked_as_irq %d", gpio_locked_as_irq);

	#ifdef KERNEL_BELOW_3_19
	if (gpio_locked_as_irq)
		gpio_unlock_as_irq(chip, offset);
	#else
	if (gpio_locked_as_irq)
		gpiochip_unlock_as_irq(chip, offset);
	#endif
#endif

	rc += gpiod_direction_output(pdata->gpiod_irq, 1);
	/* OTP will load 0x0 to 0x18ED after HW reset. */
	if (pdata->fac_status == FAC_W_OTP_WO_FW || pdata->fac_status == FAC_W_OTP_W_FW) {
		rc += touch_point_write_16bit_reg(pdata, TOUCH_POINT_INT_HW_RESET, &int_rst, 1);
	} else {
		rc += touch_point_bd_write(pdata, TOUCH_POINT_INT_HW_RESET, &int_rst, 1);
	}
	mdelay(2);//>1ms
	rc += gpiod_direction_output(pdata->gpiod_irq, 0);
	mdelay(25);//>20ms
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
	dev_info(&pdata->client->dev, "operate int pin(rc:%d)", rc);
	return rc;
}
#endif

static int touch_point_set_algo_mode(struct touch_point_data *pdata, uint8_t mode)
{
	ZForceAlgo1stGlobalTHSTyp ths;
	int rc = 0;

	dev_info(&pdata->client->dev, "desired algo mode %d ", mode);

	rc += touch_point_read_16bit_reg(pdata, TOUCH_POINT_ALGO_THS_START_ADDR, (uint8_t *)&ths, sizeof(ZForceAlgo1stGlobalTHSTyp));
	ths.I995AlgoMode = mode;
	rc += touch_point_write_16bit_reg(pdata, TOUCH_POINT_ALGO_THS_START_ADDR, (uint8_t *)&ths, sizeof(ZForceAlgo1stGlobalTHSTyp));
	rc += touch_point_write_mask(pdata, TOUCH_POINT_SENSOR_CTRL, 0x01, 0x01);//Set bit0 to notify Brahms that algo param need to be updated.
	if (rc < 0)
		dev_err(&pdata->client->dev, "set algo mode failed");

	return rc;
}

static int touch_point_set_sensitivity(struct touch_point_data *pdata, uint8_t sstv_grade)
{
	ZForceAlgo1stGlobalTHSTyp ths;
	int rc = 0;

	dev_info(&pdata->client->dev, "desire sensitivity grade %d ", sstv_grade);

	/* Use same sensitivity grade as fw grade */
	if (sstv_grade > 10)
		sstv_grade = 10;
	if (sstv_grade < 1)
		sstv_grade = 1;

	rc += touch_point_read_16bit_reg(pdata, TOUCH_POINT_ALGO_THS_START_ADDR, (uint8_t *)&ths, sizeof(ZForceAlgo1stGlobalTHSTyp));
	ths.I980AlgoSensitivityLevel = sstv_grade;
	rc += touch_point_write_16bit_reg(pdata, TOUCH_POINT_ALGO_THS_START_ADDR, (uint8_t *)&ths, sizeof(ZForceAlgo1stGlobalTHSTyp));
	rc += touch_point_write_mask(pdata, TOUCH_POINT_SENSOR_CTRL, 0x01, 0x01);//Set bit0 to notify Brahms that algo param need to be updated.
	if (rc < 0)
		dev_err(&pdata->client->dev, "set sensitivity failed");

	return rc;
}

static int touch_point_get_fw_ver(struct touch_point_data *data)
{
	int rc;
	uint16_t ver_addr;
	uint8_t buf[2];
	uint8_t ver_date[TOUCH_POINT_FW_VER_DATE_BYTES];
	uint8_t ver_time[TOUCH_POINT_FW_VER_TIME_BYTES];

	memset(buf, 0, sizeof(buf));
	memset(ver_date, 0, sizeof(ver_date));
	memset(ver_time, 0, sizeof(ver_time));

	rc = touch_point_read_16bit_reg(data, TOUCH_POINT_SWREG_VERBYTE1_REVC, buf, ARRAY_SIZE(buf));
	if (rc < 0) {
		dev_err(&data->client->dev, "fd read fw ver addr failed");
		return rc;
	}
	ver_addr = (buf[0] << 8 | buf[1]);

	rc = touch_point_read_16bit_reg(data, ver_addr, ver_date, ARRAY_SIZE(ver_date));
	if (rc < 0) {
		dev_err(&data->client->dev, "fd read fw ver date failed");
		return rc;
	}
	rc = touch_point_read_16bit_reg(data, ver_addr + sizeof(ver_date), ver_time, ARRAY_SIZE(ver_time));
	if (rc < 0) {
		dev_err(&data->client->dev, "fd read fw ver time failed");
		return rc;
	}

	memcpy(data->ver_date, ver_date, ARRAY_SIZE(ver_date));
	memcpy(data->ver_time, ver_time, ARRAY_SIZE(ver_time));
	dev_info(&data->client->dev, "fw ver:%s %s", data->ver_date, data->ver_time);
	return rc;
}

/*
 * Dump OTP memory, only be called under bd i2c.
 */
static int touch_point_bd_dump_otp_mem(struct touch_point_data *pdata)
{
	int rc = 0, offset = 0, size = TOUCH_POINT_OTP_MEM_SIZE;

	if (NULL == pdata->otp_mem)
	{
		dev_warn(&pdata->client->dev, "otp memory is NULL");
		return 0;//Not return error code.
	}

	/* read otp mem */
	while (offset < size)
	{
		if ((size - offset) > I2C_RW_MAX_LEN)
		{
			rc = touch_point_bd_read(pdata, TOUCH_POINT_OTP_MEM_START_ADDR+offset, (pdata->otp_mem+offset), I2C_RW_MAX_LEN);
			if (rc < 0)
				dev_err(&pdata->client->dev, "otp read failed(addr:%#X)", TOUCH_POINT_OTP_MEM_START_ADDR+offset);
			offset += I2C_RW_MAX_LEN;
		}
		else
		{
			rc += touch_point_bd_read(pdata, TOUCH_POINT_OTP_MEM_START_ADDR+offset, (pdata->otp_mem+offset), (size-offset));
			if (rc < 0)
				dev_err(&pdata->client->dev, "otp read failed(addr:%#X)", TOUCH_POINT_OTP_MEM_START_ADDR+offset);
			offset = size;
		}
	}
	return rc;
}

int touch_point_hpm_odr_sel(struct touch_point_data *pdata, enum ussys_tp_hpm_odr odr)
{
	uint8_t data[2] = {(uint8_t)((odr & 0xff00) >> 8), (uint8_t)(odr & 0x00ff)};

	return touch_point_write_16bit_reg(pdata, TOUCH_POINT_HPM_ODR_SEL, data, ARRAY_SIZE(data));
}

static int touch_point_lpm_odr_sel(struct touch_point_data *pdata, enum ussys_tp_lpm_odr odr)
{
	uint8_t data[2] = {(uint8_t)((odr & 0xff00) >> 8), (uint8_t)(odr & 0x00ff)};

	return touch_point_write_16bit_reg(pdata, TOUCH_POINT_LPM_ODR_SEL, data, ARRAY_SIZE(data));
}

static int touch_point_enable_lpm(struct touch_point_data *pdata, bool en)
{
	int rc = 0;

	rc += touch_point_write_mask(pdata, TOUCH_POINT_SENSOR_CTRL,
		TOUCH_POINT_REG_SENSOR_CTRL_LPM_MASK, en ? TOUCH_POINT_REG_SENSOR_CTRL_LPM_MASK : 0);//0x00:high performance mode, 0x08:duty cycle mode
	pdata->lpm_on = en;

	return rc;
}

static int touch_point_fw_download(struct touch_point_data *pdata, const u8 *fw_buf, u16 size)
{
	int rc = 0, offset = 0;
	u8 *buf = kzalloc(size, GFP_KERNEL);

	dev_err(&pdata->client->dev, "fw size : %d bytes.", size);

	/* Firmware download */
	while (offset < size)
	{
		if ((size - offset) > I2C_RW_MAX_LEN)
		{
			rc = touch_point_write_16bit_reg(pdata, 0+offset, (u8 *)(fw_buf+offset), I2C_RW_MAX_LEN);
			if (rc < 0)
				dev_err(&pdata->client->dev, "fw download failed(addr:%#X)", offset);
			offset += I2C_RW_MAX_LEN;
		}
		else
		{
			rc = touch_point_write_16bit_reg(pdata, 0+offset, (u8 *)(fw_buf+offset), (size-offset));
			if (rc < 0)
				dev_err(&pdata->client->dev, "fw download failed(addr:%#X)", offset);
			offset = size;
		}
	}

	/* Firmware read */
	offset = 0;
	while (offset < size)
	{
		if ((size - offset) > I2C_RW_MAX_LEN)
		{
			rc = touch_point_read_16bit_reg(pdata, 0+offset, (buf+offset), I2C_RW_MAX_LEN);
			if (rc < 0)
				dev_err(&pdata->client->dev, "fw read failed(addr:%d)", offset);
			offset += I2C_RW_MAX_LEN;
		}
		else
		{
			rc = touch_point_read_16bit_reg(pdata, 0+offset, (buf+offset), (size-offset));
			if (rc < 0)
				dev_err(&pdata->client->dev, "fw read failed(addr:%d)", offset);
			offset = size;
		}
	}

	/* Firmware check */
	if (memcmp(buf, fw_buf, size))
		dev_err(&pdata->client->dev, "fw not match ");

	kfree(buf);

	return rc;
}

static int touch_point_wut_trim(struct touch_point_data *pdata) {
	uint8_t rA6, rDC, rDD, r94;
	uint8_t rA6Initial = 0x40;
	uint8_t val = 0;
	uint16_t countCurrent = 0;
	uint16_t countTarget = 0;
	uint16_t tmp;
	int rc = 0;

    //0:32KHz(ZForce:4KHz)
	r94 = 0x3A;//0x3F;(84KHz)//0x3A;(32KHz)
	countTarget = 625;//242;(84KHz)//625;(32KHz)
	rc += touch_point_write_16bit_reg(pdata, 0x18A6, &rA6Initial, 1);	// make sure we're at the expected starting conditions
	rc += touch_point_write_16bit_reg(pdata, 0x1894, &r94, 1);

	// Log initial values for debug purposes
	rc += touch_point_read_16bit_reg(pdata, 0x18A6, &rA6, 1);
	rc += touch_point_read_16bit_reg(pdata, 0x18DC, &rDC, 1);
	rc += touch_point_read_16bit_reg(pdata, 0x18DD, &rDD, 1);

	// Start counting number of 20MHz pulses in one 32kHz clock pulse
	val = rDD & 0xF7;
	rc += touch_point_write_16bit_reg(pdata, 0x18DD, &val, 1);
	val = rDD | 0x08;
	rc += touch_point_write_16bit_reg(pdata, 0x18DD, &val, 1);

	// Give 400us to finish counting (playing it safe)
	mdelay(1);//HAL_Delay(400);

	// Read final count
	rc += touch_point_read_16bit_reg(pdata, 0x18DC, &rDC, 1);
	rc += touch_point_read_16bit_reg(pdata, 0x18DD, &rDD, 1);
	countCurrent  = (rDD & 0x7) << 8 | rDC;

	// Calculate the required register setting for the target frequency
	tmp = rA6Initial * countCurrent / countTarget;
	rA6 = (uint8_t)(tmp);

	// write calculated value and do another count
	rDD &= 0xF8;	// zero out the bottom three bits
	rc += touch_point_write_16bit_reg(pdata, 0x18A6, &rA6, 1);

	dev_info(&pdata->client->dev, "WUT trim done (rc = %d, 0x18A6=%#X)", rc, rA6);
	return rc;
}

static int touch_point_trim(struct touch_point_data *pdata)
{
	//HW register
	//Acoustic Trimming registers:		0x80~0x93
	//TODO: check if 0x188E=0x19 is burned in OTP
	uint8_t Brahms_VIP_Registers_AcousticTrimming[]     = {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x89,0x88,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,0x90,0x91,0x92,0x93};
	//Set 0x5A to 0x188F for Chopin ibias time
	uint8_t Brahms_VIP_Registers_AcousticTrimming_Def[] = {0x08,0x05,0x7D,0x00,0x03,0xFF,0x00,0x00,0x01,0x05,0x13,0xAC,0x07,0xD4,0x19,0x5A,0x39,0x09,0x50,0x01}; //using golden trimming from sina for 4mm stack
	//CMOST trimming registers: 0x35, 0x36, 0x48, 0x49 for ZForce. 0xA7 is invalid in Chopin?
	uint8_t Brahms_VIP_Registers_CMOSTrimming[]         = {0x35,0x36,0x48,0x49,0xA1,0xA6,/*0xA7,*/0xAA,0xAC,0xAD};
	uint8_t Brahms_VIP_Registers_CMOSTrimming_Def[]     = {0x00,0x00,0x15,0x7C,0x00,0x44,/*0x7F,*/0x3E,0x0D,0x28}; //from Man Chia 190905 BGR register [0xAD]: 00 [0xAC]: 0D [0xA1]: 00 [0xA6]: 44 [0xA7]: 30 [0xAA]: 40
	//const uint8_t Brahms_VIP_Registers_CMOSTrimming_RTL_Def[15] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x10,0x00,0x00};
	//Other testing Registers://TODO: move 0xB8 ops to fw.
	//TODO:Need double confirm 0xFB? and Need enable i2c timeout feature and glitch filter(reg 0x18DD).
	//TODO:dynamically change reg 0xB8 in fw.
	//change 0xFB from 0x1F to 0x10 to save power.
	uint8_t Brahms_VIP_Registers_Others[]      = {0xAF, 0x94, /*0x95,*/ 0xFB, 0xDD, 0xDB, 0xE9, 0xF6, 0xF8, 0xBB, 0xB8};
	uint8_t Brahms_VIP_Registers_Others_Def[]  = {0x00, 0x3A, /*0x00,*/ 0x1F, 0x10, 0xFF, 0x40, 0x05, 0x01, 0x01, 0xE7};
	//For ZForce regs
	//TODO: move 0x32,0x33,0x4C's default setting to fw.
	//ZForce 4KHz:{0x1833=0x01, 0x184F=0xBE}. 8KHz:{0x1833=0x00, 0x184F=0x22}
	uint8_t Brahms_VIP_Registers_ZForce[] 		= {0x30,0x31,0x32,0x33,0x34,/*0x35,0x36,*/0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,/*0x48,0x49,*/0x4A,0x4B,0x4C,0x4D,0x4E,0x4F};
	uint8_t Brahms_VIP_Registers_ZForce_Def[] 	= {0x50,0x00,0x00,0x00,0x96,/*0x00,0x00,*/0x1F,0x0F,0x3E,0x00,0x00,0x00,0x00,0x00,0x00,0x3F,0x3C,0xFF,0x00,0x55,0x51,0x0F,0xD0,/*0x15,0x5F,*/0x00,0x15,0xF8,0x10,0x20,0x22};
	int rc = 0, i = 0;
	uint8_t si_rev = 0, bank0 = 0, bank1 = 0;

	/* Get chip reversion */
	rc = touch_point_read_16bit_reg(pdata, TOUCH_POINT_DSRAM_SI_REV, &si_rev, 1);
	if (rc < 0)
	{
		dev_err(&pdata->client->dev, "read si rev failed ");
		return rc;
	}
	dev_info(&pdata->client->dev, "read si rev (%#X=%#X) ", TOUCH_POINT_DSRAM_SI_REV, si_rev);

	/* Read OTP bank0 for reg 0x1880 */
	rc = touch_point_read_16bit_reg(pdata, TOUCH_POINT_OTP_BANK0_START_ADDR, &bank0, 1);
	if (rc < 0)
	{
		dev_err(&pdata->client->dev, "read otp bank0 failed ");
		return rc;
	}
	dev_info(&pdata->client->dev, "read otp bank0 (%#X=%#X) ", TOUCH_POINT_OTP_BANK0_START_ADDR, bank0);

	/* Read OTP bank1 for reg 0x18C0 */
	rc = touch_point_read_16bit_reg(pdata, TOUCH_POINT_OTP_BANK1_START_ADDR, &bank1, 1);
	if (rc < 0)
	{
		dev_err(&pdata->client->dev, "read otp bank1 failed ");
		return rc;
	}
	dev_info(&pdata->client->dev, "read otp bank1 (%#X=%#X) ", TOUCH_POINT_OTP_BANK1_START_ADDR, bank1);

	/*
	 * Only write accoustic trimming when bank0/1 is not burned in OTP.
	 */
	if (bank0 == 0xFF && bank1 == 0xFF)
	{
		for (i = 0; i < sizeof(Brahms_VIP_Registers_AcousticTrimming); i++)
		{
	        rc = touch_point_write_16bit_reg(pdata, 0x1800+Brahms_VIP_Registers_AcousticTrimming[i], &Brahms_VIP_Registers_AcousticTrimming_Def[i], 1);
			if (rc < 0)
			{
				dev_err(&pdata->client->dev, "set default acoustic regs failed ");
				return rc;
			}
		}
		dev_info(&pdata->client->dev, "accoustic regs setting completed.");
	}

	//If it's OTP-ed part with 0xC0, skip below CMOS trim regs setting. Trust factory already done OTP for CMOS trimming.
	if (0xC0 != si_rev)
	{
		for (i = 0; i < sizeof(Brahms_VIP_Registers_CMOSTrimming); i++)
		{
	        rc = touch_point_write_16bit_reg(pdata, 0x1800+Brahms_VIP_Registers_CMOSTrimming[i], &Brahms_VIP_Registers_CMOSTrimming_Def[i], 1);
			if (rc < 0)
			{
				dev_err(&pdata->client->dev, "set default CMOS regs failed ");
				return rc;
			}
		}
		dev_info(&pdata->client->dev, "CMOS regs setting completed.");
	}

	for (i = 0; i < sizeof(Brahms_VIP_Registers_Others); i++)
	{
        rc = touch_point_write_16bit_reg(pdata, 0x1800+Brahms_VIP_Registers_Others[i], &Brahms_VIP_Registers_Others_Def[i], 1);
		if (rc < 0)
		{
			dev_err(&pdata->client->dev, "set default others regs failed ");
			return rc;
		}
	}
	dev_info(&pdata->client->dev, "other regs setting completed.");

	for (i = 0; i < sizeof(Brahms_VIP_Registers_ZForce); i++)
	{
		//TODO: if USP+ZForce fw is OTP-ed into part, need skip 0x32, 0x33 reg setting here because fw already set both at the beginning.
		rc = touch_point_write_16bit_reg(pdata, 0x1800+Brahms_VIP_Registers_ZForce[i], &Brahms_VIP_Registers_ZForce_Def[i], 1);
		if (rc < 0)
		{
			dev_err(&pdata->client->dev, "set default ZForce regs failed ");
			return rc;
		}
	}
	dev_info(&pdata->client->dev, "ZForce regs setting completed.");

	//If it's OTP-ed part with 0xC0, skip below WUT trim regs setting. Trust factory already done OTP for WUT trimming.
	if (0xC0 != si_rev)
	{
		rc = touch_point_wut_trim(pdata);
		if (rc < 0)
		{
			dev_err(&pdata->client->dev, "WUT trim failed ");
			return rc;
		}
		dev_info(&pdata->client->dev, "WUT regs setting completed.");
	}

    return 0;
}

/* Assuming default i2c clk from WUT and no DSRAM access in this function */
static int touch_point_check_ussys_product(struct touch_point_data *data)
{
	int rc = 0;

	rc = touch_point_bd_check_whoami(data);
	if (rc >= 0) {
		/* currently, treat all OTP chip as no FW */
		data->fac_status = FAC_NO_OTP;
		return rc;
	}

	rc = touch_point_fd_check_whoami(data);
	if (rc >= 0) {
		/* currently, treat all OTP chip as no FW */
		data->fac_status = FAC_W_OTP_WO_FW;
		return rc;
	}

	return rc;
}

static int touch_point_apply_setting(struct touch_point_data *pdata)
{
	int rc = 0;
	uint8_t data = 0;

	dev_info(&pdata->client->dev, "apply setting");

	/* set HPM odr */
	rc += touch_point_hpm_odr_sel(pdata, HPM_ODR_75HZ);
	/* set default senitivity to 5 */
	rc += touch_point_set_sensitivity(pdata, 5);
	/* set algo mode "ZForce HPF Trigger and USP Depth Check mode" as default */
	rc += touch_point_set_algo_mode(pdata, 2);
	/* set flag to indicate that this fw is downloaded by linux driver */
	rc += touch_point_write_mask(pdata, TOUCH_POINT_SENSOR_CTRL, TOUCH_POINT_REG_SENSOR_CTRL_FW_FLAG_MASK, TOUCH_POINT_REG_SENSOR_CTRL_FW_FLAG_MASK);
	/* enable interrupt */
	data = 0x01;
	rc += touch_point_fd_direct_write(pdata, TOUCH_POINT_INT_MASK, &data, sizeof(data));
	/* FD check who am i*/
	rc += touch_point_fd_check_whoami(pdata);
	/* get driver version */
	rc += touch_point_get_fw_ver(pdata);
	/* set adc avg times for debugging */
	data = 0x02;
	rc += touch_point_write_16bit_reg(pdata, TOUCH_POINT_ADC_AVG_TIMES, &data, 1);
	//set statistical value as default for B prj phones.
	if (pdata->code == KEY_VOLUMEUP) {
		uint8_t value = 0x52;
		rc += touch_point_write_16bit_reg(pdata, 0x1844, &value, 1);
		value = 0x32;//400
		rc += touch_point_write_16bit_reg(pdata, 0x1162, &value, 1);
		value = 0x0A;//9.64
		rc += touch_point_write_16bit_reg(pdata, 0x1163, &value, 1);
		dev_info(&pdata->client->dev, "[Vol+] set statistical value (ZForce gain 0x1844=0x52,ZForce p2p 0x1162=0x32, ZForce noise 0x1163=0x0A)");
	} else if (pdata->code == KEY_VOLUMEDOWN) {
		uint8_t value = 0x53;
		rc += touch_point_write_16bit_reg(pdata, 0x1844, &value, 1);
		value = 0x32;//400
		rc += touch_point_write_16bit_reg(pdata, 0x1162, &value, 1);
		value = 0x0A;//9.02
		rc += touch_point_write_16bit_reg(pdata, 0x1163, &value, 1);
		dev_info(&pdata->client->dev, "[Vol-] set statistical value (ZForce gain 0x1844=0x53,ZForce p2p 0x1162=0x32, ZForce noise 0x1163=0x0A)");
	}
	/* set chip to LPM, chip will handle LPM and HPM automatically in this mode */
	rc += touch_point_enable_lpm(pdata, true);
	return rc;
}

static int touch_point_recovery(struct touch_point_data *pdata)
{
	const struct firmware *fw = NULL;
	int rc = 0;

	dev_info(&pdata->client->dev, "recovering chip %s(/dev/tp_key%d)", touch_point_get_key_name(pdata->code), pdata->miscdev_key_idx);

	rc = request_firmware(&fw, TOUCH_POINT_FIRMWARE_NAME, &pdata->client->dev);
	if (rc || NULL == fw || NULL == fw->data) {
		dev_err(&pdata->client->dev, "get fw failed \n");
		return -ENOENT;
	}

	rc += touch_point_toggle_syncio_pin(pdata);
	rc += touch_point_enable_lpm(pdata, false);
	/* download fw */
	/* assuming chip is already at fd mode. */
	rc += touch_point_fd_check_whoami(pdata);
	rc += touch_point_put_to_hold(pdata, true);
	rc += touch_point_fd_force_reset_hold(pdata);
	rc += touch_point_fd_en_hs_i2c(pdata);
	rc += touch_point_trim(pdata);
	rc += touch_point_fd_force_reset_hold(pdata);
	rc += touch_point_fw_download(pdata, fw->data, fw->size);
	rc += touch_point_fd_force_reset_release(pdata);

	mdelay(10);//ensure firmware is running

	rc += touch_point_apply_setting(pdata);

	/*
	 * If set to 1, it indicates kernel driver requests recovery for this chip and
	 * kernel driver already finish firmware downloading, chip initialization, etc.
	 * daemon will get this value and re-load calibration files, user config files, etc.
	 */
	pdata->recovery_data = 1;
	release_firmware(fw);
	dev_info(&pdata->client->dev, "%s(/dev/tp_key%d) recovery done(rc:%d)", touch_point_get_key_name(pdata->code), pdata->miscdev_key_idx, rc);
	return rc;
}

static int touch_point_recovery_all_chips(struct touch_point_data *pdata)
{
	ktime_t recovery_ts_diff = ktime_set(0,0);
	ktime_t current_ts = ktime_get_boottime();
	int i = 0;
	int rc = 0;

	/* reject request if this request is close to previous request */
	recovery_ts_diff = ktime_sub(current_ts, recovery_ts);
	if (ktime_after(ms_to_ktime(TOUCH_POINT_RECOVERY_REJECTION_TIME_MS), recovery_ts_diff)) {
		dev_warn(&pdata->client->dev, "too frequent(< %ld senconds), reject recovery request!", TOUCH_POINT_RECOVERY_REJECTION_TIME_MS/1000);
		return 0;
	}

	/* reject request if previous recovery is ongoing */
	for (i = 0; i < USSYS_KEY_MAX; i++) {
		if ((NULL != tp_g_data[i]) && (1 == tp_g_data[i]->recovery_data)) {
			dev_warn(&pdata->client->dev, "%s(/dev/tp_key%d) recovery is ongoing, reject this recovery request!", touch_point_get_key_name(tp_g_data[i]->code), tp_g_data[i]->miscdev_key_idx);
			return 0;
		}
	}

	dev_info(&pdata->client->dev, "begin to recover all chips");

	disable_irq(pdata->client->irq);

	/* reset chip */
	touch_point_reset_chip(pdata);

	for (i = 0; i < USSYS_KEY_MAX; i++) {
		if (NULL != tp_g_data[i]) {
			touch_point_recovery(tp_g_data[i]);
		}
	}

	/* notify daemon */
	kill_fasync(&pdata->fasync, SIGIO, POLL_IN);

	enable_irq(pdata->client->irq);
	recovery_ts = ktime_get_boottime();
	dev_info(&pdata->client->dev, "all chips recovery is done.");
	return rc;
}

static int touch_point_check_device_status(struct touch_point_data *pdata)
{
	uint8_t sensor_ctl = TOUCH_POINT_REG_SENSOR_CTRL_FW_FLAG_MASK;
	int i = 0;
	int rc = 0;

	/* recover device if i2c failed on all devices */
	if (pdata->share_irq_pin) {
		uint8_t cnt = 0;
		for (i = 0; i < USSYS_KEY_MAX; i++) {
			if (NULL != tp_g_data[i]) {
				rc = touch_point_fd_check_whoami(tp_g_data[i]);
				if (rc) {
					cnt++;
				}
			}
		}

		if (key_cnt == cnt) {
			dev_warn(&pdata->client->dev, "i2c failed on all devices, try to toggle int pin and check it again");
			/* toggle int pin and check it again */
			touch_point_toggle_syncio_pin(pdata);
			cnt = 0;
			for (i = 0; i < USSYS_KEY_MAX; i++) {
				if (NULL != tp_g_data[i]) {
					rc = touch_point_fd_check_whoami(tp_g_data[i]);
					if (rc) {
						cnt++;
					}
				}
			}

			/* still fail, do recovery */
			if (key_cnt == cnt) {
				dev_warn(&pdata->client->dev, "i2c failed on all devices, try to recover all chips");
				touch_point_recovery_all_chips(pdata);
				return 1;
			}
		}
	} else {
		rc = touch_point_fd_check_whoami(pdata);
		if (rc) {
			dev_warn(&pdata->client->dev, "i2c failed for %s(/dev/tp_key%d), try to toggle int pin and check it again", touch_point_get_key_name(pdata->code), pdata->miscdev_key_idx);
			touch_point_toggle_syncio_pin(pdata);
			rc = touch_point_fd_check_whoami(pdata);
			if (rc) {
				dev_warn(&pdata->client->dev, "i2c failed for %s(/dev/tp_key%d), try to recover all chips", touch_point_get_key_name(pdata->code), pdata->miscdev_key_idx);
				touch_point_recovery_all_chips(pdata);
				return 1;
			}
		}
	}

	/* recover device if chip fw was changed somehow */
	rc = touch_point_read_16bit_reg(pdata, TOUCH_POINT_SENSOR_CTRL, &sensor_ctl, 1);
	if (!rc && ((sensor_ctl & TOUCH_POINT_REG_SENSOR_CTRL_FW_FLAG_MASK) == 0)) {
		dev_warn(&pdata->client->dev, "chip fw was changed somehow for %s(/dev/tp_key%d), try to recover chip", touch_point_get_key_name(pdata->code), pdata->miscdev_key_idx);
		touch_point_recovery_all_chips(pdata);
		return 1;
	}
	return 0;
}

static ssize_t touch_point_fd_read_debug_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	int rc = 0;
	u8 debug_data[19];

	touch_point_read_16bit_reg(pdata, TOUCH_POINT_REG_ADC, debug_data, 19);

	rc = snprintf(buf, PAGE_SIZE,
			"USP2x: %4d\n"
			"USP32x:%4d\n"
			"SUM:   %4d\n"
			"Z0+Z1: %4d\n"
			"Z0+Z2: %4d\n"
			"Z2+Z3: %4d\n"
			"Temp:  %4d\n"
			"AlgoState:       %4d\n"
			"AlgoStateCNT:    %4d\n"
			"AlgoCurrentDepth:%4d\n"
			"SensorCtrl:      %#X\n"
			"adcXAvgTimes:    %4d\n",
			(debug_data[0] << 8 | debug_data[1]) >> 5,	//USP 2x
			(debug_data[2] << 8 | debug_data[3]) >> 5,	//USP 32x
			(uint16_t)debug_data[4] << 3,				//ZForce SUM
			(uint16_t)debug_data[5] << 3,				//ZForce Z0+Z1
			(uint16_t)debug_data[6] << 3,				//ZForce Z0+Z2
			(uint16_t)debug_data[7] << 3,				//ZForce Z2+Z3
			(debug_data[8] << 8 | debug_data[9]) >> 5,	//Temperature
			(int)debug_data[14],						//AlgoState
			(int)debug_data[15],						//AlgoStateCNT
			(int)debug_data[16],						//AlgoCurrentDepth
			(int)debug_data[17],						//GPR1: Sensor Control
			(int)debug_data[18]);						//adcXAvgTimes
	return rc;
}

static ssize_t touch_point_signal_strength_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	u8 signal_strength[3] = {0};

	touch_point_read_16bit_reg(pdata, TOUCH_POINT_REG_ADC_32X, signal_strength, 3);
	return scnprintf(buf, PAGE_SIZE,
				"USP %4d ZForce %4d\n",
				(signal_strength[0] << 8 | signal_strength[1]) >> 5,
				(uint16_t)signal_strength[2] << 3);
}

static ssize_t touch_point_drv_version_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	int rc = 0;

	rc = scnprintf(buf, PAGE_SIZE, "drv ver:%s\n", TOUCH_POINT_VERSION);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "fw ver:%s %s\n", pdata->ver_date, pdata->ver_time);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "misc_dev(/dev/tp_key%d),key input code:%d ,key name:%s\n",
			pdata->miscdev_key_idx,
			pdata->code,
			touch_point_get_key_name(pdata->code));

	return rc;
}

static ssize_t touch_point_fd_read_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	char *endp;

	dev_err(&pdata->client->dev, "this r cmd buf size %ld ", size);

	pdata->reg_to_read = simple_strtoul(buf, &endp, 0);
	pdata->size_to_read = simple_strtoul(endp+1, NULL, 0);

	dev_err(&pdata->client->dev, "reg 0x%x, size %d ", pdata->reg_to_read, pdata->size_to_read);

	return size;
}

static ssize_t touch_point_fd_read_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	u8 *reg = kzalloc(pdata->size_to_read, GFP_KERNEL); // 256 is total registers
	int rc = 0;
	u16 i = 0;

	if (reg == NULL) {
		dev_err(&pdata->client->dev, "no mem ");
		return -ENOMEM;
	}

	touch_point_read_16bit_reg(pdata, pdata->reg_to_read, reg, pdata->size_to_read);

	while (i < pdata->size_to_read) {
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x%04x = 0x%02x\n", pdata->reg_to_read + i, reg[i]);
		i++;
	}

	kfree(reg);

	return rc;
}

static ssize_t touch_point_fd_write_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	char *wbuf = kzalloc(size, GFP_KERNEL);
	char *endp;
	u16 reg = 0;
	u16 len;
	u16 i = 0;

	if (wbuf == NULL) {
		dev_err(&pdata->client->dev, "no mem ");
		return -ENOMEM;
	}

	reg = simple_strtoul(buf, &endp, 0);

	len = simple_strtoul(endp+1, &endp, 0);

	while ((endp+1 < buf + size) && (i < size)) {
		wbuf[i] = simple_strtoul(endp+1, &endp, 0);
		i++;
	}

	dev_info(&pdata->client->dev, "write reg 0x%x, len %d len_of_buf %d ", reg, len, i);

	touch_point_write_16bit_reg(pdata, reg, wbuf, len);

	kfree(wbuf);

	return size;
}

static ssize_t touch_point_sensitivity_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	char *endp;
	u8 sense_grade;

	sense_grade = simple_strtoul(buf, &endp, 0);
	touch_point_set_sensitivity(pdata, sense_grade);

	return size;
}

static ssize_t touch_point_sensitivity_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	ZForceAlgo1stGlobalTHSTyp ths;

	touch_point_read_16bit_reg(pdata, TOUCH_POINT_ALGO_THS_START_ADDR, (uint8_t *)&ths, sizeof(ZForceAlgo1stGlobalTHSTyp));

	dev_info(&pdata->client->dev, "sensitivity %d ", ths.I980AlgoSensitivityLevel);

	return scnprintf(buf, PAGE_SIZE, "sensitivity grade:%d\n", ths.I980AlgoSensitivityLevel);
}

static ssize_t touch_point_toggle_int_pin_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);

	disable_irq(pdata->client->irq);
	touch_point_toggle_syncio_pin(pdata);
	enable_irq(pdata->client->irq);

	return size;
}

static ssize_t touch_point_en_hpm_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	char *endp;
	uint8_t en_hpm = 0;

	en_hpm = simple_strtoul(buf, &endp, 0);

	disable_irq(pdata->client->irq);
	touch_point_toggle_syncio_pin(pdata);
	touch_point_enable_lpm(pdata, !en_hpm);
	enable_irq(pdata->client->irq);

	return size;
}

static ssize_t touch_point_recovery_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	char *endp;
	uint8_t recover_device = 0;

	recover_device = simple_strtoul(buf, &endp, 0);
	if (recover_device) {
		touch_point_recovery_all_chips(pdata);
	}
	return size;
}

static ssize_t touch_point_otp_mem_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct touch_point_data *pdata = dev_get_drvdata(dev);
	int rc = 0, i = 0, line = 0;
	int first_line_addr = TOUCH_POINT_OTP_MEM_START_ADDR >> 4;
	int last_line_addr = (TOUCH_POINT_OTP_MEM_FW_START_ADDR - 1) >> 4;//TOUCH_POINT_OTP_MEM_END_ADDR >> 4;

	if (NULL == pdata->otp_mem)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "otp memory is NULL\r\n");
		return rc;
	}

	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=====OTP Mem Dump(%s)=====\r\n", touch_point_get_key_name(pdata->code));

	for (i = 0; i < 4; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X : %#X\r\n",
						TOUCH_POINT_OTP_MEM_START_ADDR + i,
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_MEM_START_ADDR + i)]);
	}

	/* OTP ZForce */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "//ZForce\r\n");
	for (i = 0; i < TOUCH_POINT_OTP_ZForce_SIZE; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X(Reg %#X) : %#X\r\n",
						TOUCH_POINT_OTP_ZForce_START_ADDR + i,
						0x1830 + i,
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_ZForce_START_ADDR + i)]);
	}

	/* OTP USP Bank0 */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "//USP Bank0\r\n");
	for (i = 0; i < TOUCH_POINT_OTP_BANK0_SIZE; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X(Reg %#X) : %#X\r\n",
						TOUCH_POINT_OTP_BANK0_START_ADDR + i,
						0x1880 + i,
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_BANK0_START_ADDR + i)]);
	}

	/* OTP USP Bank1 */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "//USP Bank1\r\n");
	for (i = 0; i < TOUCH_POINT_OTP_BANK1_SIZE; i++)
	{
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "%#X(Reg %#X) : %#X\r\n",
						TOUCH_POINT_OTP_BANK1_START_ADDR + i,
						0x18C0 + i,
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_BANK1_START_ADDR + i)]);
	}

	/* OTP IDs */
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "LOTID_H : %#X ,LOTID_L : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_LOTID_H_ADDR)],
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_LOTID_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "WaferID : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_WAFERID_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "CP/FT Ver : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_CPFT_VER_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "StackID(msb) : %#X ,StackID(lsb) : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_STACKID_MSB_ADDR)],
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_STACKID_LSB_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "OTP Ver : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_OTP_VER_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "SiRev(old) : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_SIREV_OLD_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "FW Ver_H : %#X ,FW Ver_L : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_FW_VER_H_ADDR)],
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_FW_VER_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "TempOff_H : %#X ,TempOff_L : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_TIMEOFF_H_ADDR)],
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_TIMEOFF_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "Chip : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_CHIP_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "SiRev(new) : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_SIREV_NEW_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "DieID_R_H : %#X ,DieID_R_L : %#X ,DieID_C_H : %#X ,DieID_C_L : %#X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_R_H_ADDR)],
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_R_L_ADDR)],
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_C_H_ADDR)],
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_C_L_ADDR)]);
	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "32-bit DieID (R-H R-L C-H C-L): 0x%08X\r\n",
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_R_H_ADDR)]<<24 |
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_R_L_ADDR)]<<16 |
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_C_H_ADDR)]<< 8 |
						pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(TOUCH_POINT_OTP_DIEID_C_L_ADDR)]);

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
			if (otp_addr >= TOUCH_POINT_OTP_MEM_START_ADDR && otp_addr <= TOUCH_POINT_OTP_MEM_END_ADDR)
				rc += scnprintf(buf + rc, PAGE_SIZE - rc, "0x%02x ", pdata->otp_mem[TOUCH_POINT_OTPADDR2IDX(otp_addr)]);
		}
		rc += scnprintf(buf + rc, PAGE_SIZE - rc, "\r\n");
	}

	rc += scnprintf(buf + rc, PAGE_SIZE - rc, "=========END==========\r\n");
	return rc;
}

static DEVICE_ATTR(drv_version, S_IRUGO, touch_point_drv_version_show, NULL);
static DEVICE_ATTR(sensitivity, (S_IRUGO | S_IWUSR), touch_point_sensitivity_show, touch_point_sensitivity_store);
static DEVICE_ATTR(fd_write, S_IWUSR, NULL, touch_point_fd_write_store);
static DEVICE_ATTR(fd_read, (S_IRUGO | S_IWUSR), touch_point_fd_read_show, touch_point_fd_read_store);
static DEVICE_ATTR(fd_read_debug_info, S_IRUGO, touch_point_fd_read_debug_info_show, NULL);
//static DEVICE_ATTR(bd_read_debug_info, S_IRUGO, touch_point_bd_read_debug_info_show, NULL);
static DEVICE_ATTR(signal_strength, S_IRUGO, touch_point_signal_strength_show, NULL);
static DEVICE_ATTR(toggle_int_pin, S_IWUSR, NULL, touch_point_toggle_int_pin_store);
static DEVICE_ATTR(en_hpm, S_IWUSR, NULL, touch_point_en_hpm_store);
static DEVICE_ATTR(otp_mem, S_IRUGO, touch_point_otp_mem_show, NULL);
static DEVICE_ATTR(recovery, S_IWUSR, NULL, touch_point_recovery_store);

static struct attribute *touch_point_attrs[] = {
	&dev_attr_drv_version.attr,
	&dev_attr_signal_strength.attr,
	&dev_attr_sensitivity.attr,
	&dev_attr_toggle_int_pin.attr,
	&dev_attr_en_hpm.attr,
	&dev_attr_fd_write.attr,
	&dev_attr_fd_read.attr,
	&dev_attr_fd_read_debug_info.attr,
	//&dev_attr_bd_read_debug_info.attr,
	&dev_attr_otp_mem.attr,
	&dev_attr_recovery.attr,
	NULL
};

static const struct attribute_group touch_point_attr_group = {
	.attrs = touch_point_attrs,
};

static int touch_point_init(struct touch_point_data *pdata, const u8 *fw_data, size_t size)
{
	int rc = 0;
	bool otp_parts = (pdata->fac_status == FAC_W_OTP_WO_FW || pdata->fac_status == FAC_W_OTP_W_FW) ? true : false;

	dev_info(&pdata->client->dev, "fac status:%d", pdata->fac_status);

	if (otp_parts) {
		rc += touch_point_put_to_hold(pdata, true);
		rc += touch_point_fd_force_reset_hold(pdata);
		rc += touch_point_exit_fd_enter_bd(pdata);
		rc += touch_point_bd_check_whoami(pdata);
	}
	//Assuming chip is already at bd mode.
	rc += touch_point_bd_force_reset_hold(pdata);
	rc += touch_point_bd_en_hs_i2c(pdata);
	rc += touch_point_bd_dump_otp_mem(pdata);
	rc += touch_point_exit_bd_enter_fd(pdata);
	rc += touch_point_fd_check_whoami(pdata);
	rc += touch_point_trim(pdata);
	rc += touch_point_fd_force_reset_hold(pdata);
	rc += touch_point_fw_download(pdata, fw_data, size);
	rc += touch_point_fd_force_reset_release(pdata);

	mdelay(10);//ensure firmware is running

	rc += touch_point_apply_setting(pdata);

	dev_info(&pdata->client->dev, "init done(rc:%d)", rc);
	return rc;
}

static void touch_point_fw_dl_cb(const struct firmware *fw, void *ctx)
{
	struct touch_point_data *pdata = ctx;

	dev_info(&pdata->client->dev, "downloading fw...");

	if (fw) {
		touch_point_init(pdata, fw->data, fw->size);
		enable_irq(pdata->client->irq);
	} else {
		dev_info(&pdata->client->dev, "get fw failed");
	}

	release_firmware(fw);
}

static int touch_point_input_dev_open(struct input_dev *dev)
{
	struct touch_point_data *pdata __maybe_unused = input_get_drvdata(dev);

	dev_info(&dev->dev, "touch_point_input_dev_open");

	return 0;
}

static void touch_point_input_dev_close(struct input_dev *dev)
{
	struct touch_point_data *pdata = input_get_drvdata(dev);

	hrtimer_cancel(&pdata->timer);
	cancel_delayed_work_sync(&pdata->delaywork);

	dev_info(&dev->dev, "touch_point_input_dev_close");

}

static int touch_point_check_report_key(struct touch_point_data *pdata)
{
//	struct input_dev *input_dev = pdata->input_dev;
	int rc = 0;
	u8 buf[2];
	u8 state, event_cnt;

	/* Ignore interrupt event while cali is ongoing */
	if (true == pdata->is_cali_ongoing) {
		dev_warn(&pdata->client->dev, "cali is ongoing, ignore intr event!");
		return 0;
	}

	mutex_lock(&recovery_lock);
	rc = touch_point_check_device_status(pdata);
	if (rc == 1) {
		dev_warn(&pdata->client->dev, "device is just recoved");
		mutex_unlock(&recovery_lock);
		return rc;
	}
	mutex_unlock(&recovery_lock);

	rc = touch_point_read_16bit_reg(pdata, TOUCH_POINT_REG_ALGO_STATUS, buf, 2);
	if (rc < 0) {
		dev_info(&pdata->client->dev, "read state buf failed");
		return rc;
	}

	state = buf[0];
	event_cnt = buf[1];

	dev_info(&pdata->client->dev, "algo state: %d, event count %d %d key_pressed %d is_cali_ongoing %d", state, event_cnt, pdata->event_cnt, pdata->key_pressed, pdata->is_cali_ongoing);

	/* INT is not from this chip */
	if (pdata->event_cnt == event_cnt)
		return 0;

	if (state == 2) {
		pdata->key_pressed = true;
//		input_report_key(input_dev, pdata->code, 1);
	} else {
		/* in case key press is not detected, we still report press and release */
		if (!pdata->key_pressed)
//			input_report_key(input_dev, pdata->code, 1);
		pdata->key_pressed = false;
//		input_report_key(input_dev, pdata->code, 0);
	}

//	input_sync(input_dev);

	pdata->event_cnt = event_cnt;

	return 0;
}

static void touch_point_worker(struct work_struct *work)
{
	struct touch_point_data *pdata = container_of(work, struct touch_point_data, delaywork.work);

	touch_point_check_report_key(pdata);
	if (pdata->wakeup)
		pm_relax(&pdata->client->dev);
}

static irqreturn_t touch_point_irq_handler(int irq, void *dev)
{
	struct touch_point_data *pdata = dev;

	dev_err(&pdata->client->dev, "touch_point_irq_handler, %d", pdata->wakeup);

	if (pdata->wakeup)
		pm_stay_awake(&pdata->client->dev);

	if(NULL != pdata->touch_point_wq)
		queue_delayed_work(pdata->touch_point_wq, &pdata->delaywork, pdata->wakeup ? msecs_to_jiffies(10) : 0);

	return IRQ_HANDLED;
}

static enum hrtimer_restart touch_point_timer(struct hrtimer *timer)
{
	struct touch_point_data *pdata;
	//int time_ms;

	pdata = container_of(timer, struct touch_point_data, timer);

	//queue_work(pdata->touch_point_wq, &pdata->work);
	return HRTIMER_NORESTART;

	//hrtimer_forward_now(&data->timer, ktime_set(0, time_ms));
	//schedule_work(&data->work);
	//return HRTIMER_RESTART;
}

static void touch_point_regulator_disable(void *regulator)
{
	regulator_disable(regulator);
}

static void touch_point_remove_group(void *dev)
{
	sysfs_remove_group(&(((struct device *)dev)->kobj), &touch_point_attr_group);
}

static void touch_point_dereg_misc_dev(void *misc_dev)
{
	misc_deregister(misc_dev);
}

static void touch_point_destroy_wq(void *workqueue)
{
	destroy_workqueue(workqueue);
}

static struct touch_point_data * touch_point_find_key_data(struct inode *inode)
{
	struct touch_point_data *data = NULL;
	int minor_num = MINOR(inode->i_rdev);
	int i = 0;

	for (i = 0; i < USSYS_KEY_MAX; i++) {
		if (NULL != tp_g_data[i] && minor_num == tp_g_data[i]->miscdev_minor) {
			data = tp_g_data[i];
			break;
		}
	}

	return data;
}

static int touch_point_misc_dev_open(struct inode *inode, struct file *file)
{
	struct touch_point_data *data = touch_point_find_key_data(inode);

	if (!data) {
		return -EINVAL;
	}

	mutex_lock(&data->lock);
	if (data->miscdev_count) {
		dev_err(&data->client->dev, "device is busy ");
		mutex_unlock(&data->lock);
		return -EBUSY;
	}
	data->miscdev_count++;
	mutex_unlock(&data->lock);

	//dev_info(&data->client->dev, "open mis dev ");

	file->private_data = data;

	return 0;
}

static int touch_point_misc_dev_release(struct inode *inode, struct file *file)
{
	struct touch_point_data *data = file->private_data;

	mutex_lock(&data->lock);
	if (!data->miscdev_count) {
		dev_err(&data->client->dev, "file wasn't opened ");
		mutex_unlock(&data->lock);
		return -ENOTTY;
	}
	data->miscdev_count--;
	mutex_unlock(&data->lock);

	//dev_info(&data->client->dev, "close mis dev ");

	return 0;
}

long touch_point_misc_dev_ioctl(struct file *file,
	       unsigned int cmd, unsigned long arg)
{
	//void __user *argp = (void __user *)arg;
	struct touch_point_data *data = file->private_data;
	ussys_rw_msg __user *user_rw_msg = (ussys_rw_msg __user *) arg;
	ussys_rw_msg *rw_msg;
	uint16_t rw_size;
	uint8_t always_on;
	uint8_t cali_flag = 0;
	int rc = 0;

	switch (cmd) {
	case USSYSIO_ENABLE_ALWAYS_ON:
		rc = get_user(always_on, (uint8_t __user *) arg);
		dev_info(&data->client->dev, "set always on mode %d.", always_on);
		disable_irq(data->client->irq);
		rc += touch_point_toggle_syncio_pin(data);
		rc += touch_point_enable_lpm(data, !always_on);
		enable_irq(data->client->irq);
		break;
	case USSYSIO_TOGGLE_INT_PIN:
		disable_irq(data->client->irq);
		rc += touch_point_toggle_syncio_pin(data);
		enable_irq(data->client->irq);
		break;
	case USSYSIO_READ_DEV:
		get_user(rw_size, &user_rw_msg->len);
		rw_msg = kzalloc(sizeof(ussys_rw_msg) + rw_size, GFP_KERNEL);
		if (rw_msg == NULL) {
			dev_err(&data->client->dev, "no mem for read buf.");
			return -EFAULT;
		}
		if (copy_from_user(rw_msg, user_rw_msg, sizeof(ussys_rw_msg))) {
			dev_err(&data->client->dev, "copy_from_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		/* read reg to kernel space buf */
		touch_point_read_16bit_reg(data, rw_msg->channel << 8 | rw_msg->addr, rw_msg->buf, rw_msg->len);
		/* copy kernel space buf to user space */
		if (copy_to_user(user_rw_msg->buf, rw_msg->buf, rw_msg->len)) {
			dev_err(&data->client->dev, "copy_to_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		kfree(rw_msg);
		break;
	case USSYSIO_WRITE_DEV:
		get_user(rw_size, &user_rw_msg->len);
		rw_msg = kzalloc(sizeof(ussys_rw_msg) + rw_size, GFP_KERNEL);
		if (rw_msg == NULL) {
			dev_err(&data->client->dev, "no mem for read buf.");
			return -EFAULT;
		}
		if (copy_from_user(rw_msg, user_rw_msg, sizeof(ussys_rw_msg) + rw_size)) {
			dev_err(&data->client->dev, "copy_from_user failed.");
			kfree(rw_msg);
			return -EFAULT;
		}
		/* write reg to device */
		touch_point_write_16bit_reg(data, rw_msg->channel << 8 | rw_msg->addr, rw_msg->buf, rw_msg->len);
		kfree(rw_msg);
		break;
	case USSYSIO_SET_CALI_FLAG:
	{
		get_user(cali_flag, (uint8_t __user *) arg);
		data->is_cali_ongoing = !!cali_flag;
		dev_info(&data->client->dev, "set cali flag : %d", cali_flag);
		break;
	}
	case USSYSIO_GET_KEY_NAME:
	{
		const char *name = touch_point_get_key_name(data->code);
		char __user *user_buf = (char __user *) arg;
		if (copy_to_user(user_buf, (const void *)name, strlen(name) + 1)) {
			dev_err(&data->client->dev, "get key name failed.");
			return -EFAULT;
		}
		break;
	}
	case USSYSIO_DUMP_OTP_MEM:
	{
		char __user *user_buf = (char __user *) arg;
		if (NULL == data->otp_mem)
		{
			dev_warn(&data->client->dev, "otp memory is NULL.");
			return -EFAULT;
		}
		if (copy_to_user(user_buf, data->otp_mem, TOUCH_POINT_OTP_MEM_SIZE)) {
			dev_err(&data->client->dev, "dump otp mem failed.");
			return -EFAULT;
		}
		break;
	}
	case USSYSIO_GET_VERSION:
	{
		int rc = 0;
		char ver[128] = "";
		char __user *user_buf = (char __user *) arg;

		rc += scnprintf(ver, sizeof(ver) - rc, "%s,%s %s", TOUCH_POINT_VERSION, data->ver_date, data->ver_time);
		if (copy_to_user(user_buf, ver, strlen(ver) + 1)) {
			dev_err(&data->client->dev, "get version failed.");
			return -EFAULT;
		}
		break;
	}
	case USSYSIO_RECOVER_DEV:
	{
		dev_info(&data->client->dev, "user recover device %s(/dev/tp_key%d)", touch_point_get_key_name(data->code), data->miscdev_key_idx);
		touch_point_recovery_all_chips(data);
		break;
	}
	default:
		dev_err(&data->client->dev, "Unsupported ioctl command %u\n", cmd);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static int touch_point_misc_dev_fasync(int fd, struct file *file, int on)
{
	struct touch_point_data *data = file->private_data;

	dev_info(&data->client->dev, "set fasync");
	return fasync_helper(fd, file, on, &data->fasync);
}

static ssize_t touch_point_misc_dev_read(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	struct touch_point_data *data = file->private_data;
	int rc = 0;

	if (count != 1) {
		dev_err(&data->client->dev, "bad param! (count:%zu)", count);
		return -EINVAL;
	}

	rc = copy_to_user(buffer, &data->recovery_data, 1);
	if (rc) {
		dev_err(&data->client->dev, "copy_to_user error! (recovery_data:%d)", data->recovery_data);
		return -EFAULT;
	}

	return count;
}

static ssize_t touch_point_misc_dev_write(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	struct touch_point_data *data = file->private_data;
	uint8_t done_flag = 0;
	int rc = 0;

	if (count != 1) {
		dev_err(&data->client->dev, "bad param! (count:%zu)", count);
		return -EINVAL;
	}

	rc = copy_from_user(&done_flag, buffer, 1);
	if (rc) {
		dev_err(&data->client->dev, "copy_from_user error! (done_flag:%d)", done_flag);
		return -EFAULT;
	}

	if (done_flag) {
		/*
		 * If set to 0, it indicates daemon already re-load calibration files, user config files, etc for this chip.
		 */
		data->recovery_data = 0;
	}
	return count;
}

static const struct file_operations touch_point_misc_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= touch_point_misc_dev_open,
	.release	= touch_point_misc_dev_release,
	.unlocked_ioctl	= touch_point_misc_dev_ioctl,
	.fasync		= touch_point_misc_dev_fasync,
	.read		= touch_point_misc_dev_read,
	.write		= touch_point_misc_dev_write,
};

static int touch_point_setup_input_dev(struct touch_point_data *pdata)
{
	struct input_dev *input_dev;
	struct i2c_client *client = pdata->client;
	int rc = 0;

	input_dev = devm_input_allocate_device(&client->dev);
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	pdata->input_dev = input_dev;

	input_dev->name = client->name;
	input_dev->id.bustype = BUS_I2C;
	input_dev->open = touch_point_input_dev_open;
	input_dev->close = touch_point_input_dev_close;
	input_dev->dev.parent = &client->dev;

	__set_bit(EV_REP, input_dev->evbit);
	input_set_capability(input_dev, EV_KEY, pdata->code);
	input_set_drvdata(input_dev, pdata);
	rc = input_register_device(input_dev);
	if (rc) {
		dev_err(&client->dev, "failed to register input device\n");
		return rc;
	}

	return 0;
}

#if USING_MSM_DRM_NOTIFIER
/* This callback gets called when something important happens inside a
 * drm driver. We're looking if that important event is blanking,
 * and if it is and necessary, we're switching touch point power as well ...
 */
static int touch_point_drm_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct touch_point_data *pdata;
	struct msm_drm_notifier *evdata = data;
	int drm_blank = 0;

	pdata = container_of(self, struct touch_point_data, drm_notif);
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
			//TODO: Switch to touch point HPM
		}
		else if (MSM_DRM_BLANK_POWERDOWN == drm_blank)//4
		{
			dev_info(&pdata->client->dev, "screen off.");
			//TODO: Switch to touch point LPM
		}
		else
		{
			dev_err(&pdata->client->dev, "unknown screen status.");
		}
	}

	return 0;
}

static int touch_point_register_drm(struct touch_point_data *pdata)
{
	memset(&pdata->drm_notif, 0, sizeof(pdata->drm_notif));
	pdata->drm_notif.notifier_call = touch_point_drm_notifier_callback;

	dev_info(&pdata->client->dev, "reg drm notifier.");
	return msm_drm_register_client(&pdata->drm_notif);
}

static void touch_point_unregister_drm(void *data)
{
	struct touch_point_data *pdata = (struct touch_point_data *)data;

	dev_info(&pdata->client->dev, "unreg drm notifier.");
	msm_drm_unregister_client(&pdata->drm_notif);
}
#else
#ifdef CONFIG_FB
/* This callback gets called when something important happens inside a
 * framebuffer driver. We're looking if that important event is blanking,
 * and if it is and necessary, we're switching touch point power as well ...
 */
static int touch_point_fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct touch_point_data *pdata;
	struct fb_event *evdata = data;
	int fb_blank = 0;

	pdata = container_of(self, struct touch_point_data, fb_notif);
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
			//TODO: Switch to touch point HPM
		}
		else if (FB_BLANK_POWERDOWN == fb_blank)//4
		{
			dev_info(&pdata->client->dev, "screen blanked.");
			//TODO: Switch to touch point LPM
		}
	}

	return 0;
}

static int touch_point_register_fb(struct touch_point_data *pdata)
{
	memset(&pdata->fb_notif, 0, sizeof(pdata->fb_notif));
	pdata->fb_notif.notifier_call = touch_point_fb_notifier_callback;

	return fb_register_client(&pdata->fb_notif);
}

static void touch_point_unregister_fb(void *data)
{
	struct touch_point_data *pdata = (struct touch_point_data *)data;

	fb_unregister_client(&pdata->fb_notif);
}
#endif/*CONFIG_FB*/
#endif/*USING_MSM_DRM_NOTIFIER*/

#if USING_DRIVER_TO_CTL_POWER
static int touch_point_suspend(struct device *dev)
{
	struct touch_point_data *data = dev_get_drvdata(dev);
	enum ussys_tp_lpm_odr lpm_odr_value = data->code == KEY_POWER ? LPM_ODR_50HZ : LPM_ODR_50HZ;

	hrtimer_cancel(&data->timer);
	cancel_delayed_work_sync(&data->delaywork);

	touch_point_lpm_odr_sel(data, lpm_odr_value);
	touch_point_enable_lpm(data, true);
	dev_info(&data->client->dev, "touch_point_suspend\n");
	//dump_stack();
	return 0;
}

static int touch_point_resume(struct device *dev)
{
	struct touch_point_data *data __maybe_unused = dev_get_drvdata(dev);
	u8 buf[2];
	u8 state, event_cnt;
	int rc = 0;

	dev_info(&data->client->dev, "touch_point_resume\n");

	disable_irq(data->client->irq);
	touch_point_toggle_syncio_pin(data);
	//touch_point_enable_lpm(data, false);
	enable_irq(data->client->irq);

	//if (data->code == KEY_POWER) {
	if (data->code == KEY_POWER || data->code == KEY_VOLUMEUP || data->code == KEY_VOLUMEDOWN) {//if Power/Vol+/Vol- need keep functional in suspend
		return 0;
	}

	rc = touch_point_read_16bit_reg(data, TOUCH_POINT_REG_ALGO_STATUS, buf, 2);
	if (rc < 0) {
		dev_err(&data->client->dev, "resuming, read state buf failed");
		return rc;
	}

	state = buf[0];
	event_cnt = buf[1];

	if (data->event_cnt == event_cnt) {
		/* key is not pressed when system is sleep */
	} else if (state == 2) {
		/* key is still holding, report key status */
		queue_delayed_work(data->touch_point_wq, &data->delaywork, msecs_to_jiffies(15));
	} else {
		/* ignore the key status during sleep */
		data->event_cnt = event_cnt;
	}

	return 0;
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id touch_point_of_id_table[] = {
	{.compatible = "ussys,tp_key0", .data = (void *)USSYS_KEY_0},
	{.compatible = "ussys,tp_key1", .data = (void *)USSYS_KEY_1},
	{.compatible = "ussys,tp_key2", .data = (void *)USSYS_KEY_2},
	{.compatible = "ussys,tp_key3", .data = (void *)USSYS_KEY_3},
	{ },
};
MODULE_DEVICE_TABLE(of, touch_point_of_id_table);
#else
#define touch_point_of_id_table NULL
#endif

#ifdef CONFIG_OF
static struct touch_point_data * touch_point_parse_dt(struct device *dev)
{
	struct device_node *node = dev->of_node;
	const struct of_device_id *of_id;
	struct touch_point_data *pdata;

	if (!node) {
		dev_err(dev, "invalid pdata");
		return ERR_PTR(-ENODEV);
	}

	of_id = of_match_node(touch_point_of_id_table, node);
	if (!of_id) {
		dev_err(dev, "no of id");
		return ERR_PTR(-ENODEV);
	}

	pdata = devm_kzalloc(dev, sizeof(struct touch_point_data), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENODEV);

	pdata->otp_mem = devm_kzalloc(dev, TOUCH_POINT_OTP_MEM_SIZE, GFP_KERNEL);
	if (NULL == pdata->otp_mem) {
		dev_warn(dev, "no memory for otp");
		//return ERR_PTR(-ENODEV);//go ahead if otp mem is not available
	}

	pdata->share_irq_pin = of_property_read_bool(node, "touch_point,share-irq-pin");
	pdata->wakeup = of_property_read_bool(node, "wakeup-source");
	pdata->key_idx = (enum ussys_tp_key_idx)of_id->data;

	if (of_property_read_u32(node, "linux,input-code", &pdata->code)) {
		dev_err(dev, "failed get key code ");
		return ERR_PTR(-EINVAL);
	}

	return pdata;
}
#else
static struct touch_point_data * touch_point_parse_dt(struct device *dev)
{
	return -ENODEV;
}
#endif

static int touch_point_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct touch_point_data *data;
	struct regulator *regulator;
	char misc_dev_name[32];
	int rc;

	dev_info(&client->dev, "ussys tp key probe");

	data = touch_point_parse_dt(&client->dev);
	if (IS_ERR(data))
		return PTR_ERR(data);

	regulator = devm_regulator_get(&client->dev, "touch_point-vdd");
	if (!IS_ERR(regulator)) {
		rc = regulator_enable(regulator);
		if (rc < 0) {
			dev_err(&client->dev, "Failed to enable regulator: %d", rc);
			return rc;
		}
		dev_info(&client->dev, "regulator enable");

		//TODO:remove it due to we couldn't disable regulator for keys which need keep active.
		rc = devm_add_action_or_reset(&client->dev, touch_point_regulator_disable, regulator);
		if (rc)
			return rc;
		mdelay(50);
	} else {
		dev_info(&client->dev, "regulator error");
	}

	data->client = client;

	i2c_set_clientdata(client, data);

	mutex_init(&data->lock);
	mutex_init(&data->i2c_lock);
	mutex_init(&data->fd_lock);
	init_completion(&data->read_data_completion);
	INIT_DELAYED_WORK(&data->delaywork, touch_point_worker);
	hrtimer_init(&data->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	data->timer.function = touch_point_timer;
	toggle_ts = ktime_set(0,0);
	recovery_ts = ktime_set(0,0);

	/* assume rst pin is shared for all chips */
	if (!is_rst_pin_initialized) {
		data->gpiod_rst = devm_gpiod_get(&client->dev, "touch_point,rst", GPIOD_OUT_LOW);
		if (IS_ERR(data->gpiod_rst)) {
			dev_err(&client->dev, "get rst gpio failed");
			data->gpiod_rst = NULL;//return PTR_ERR(data->gpiod_rst);
		}
		/* reset chip */
		rc = touch_point_reset_chip(data);
		if (rc < 0)
		{
			dev_warn(&client->dev, "reset chip failed");
			//return rc;
		}
		shared_gpiod_rst = data->gpiod_rst;
		is_rst_pin_initialized = true;
	}
	data->gpiod_rst = shared_gpiod_rst;

	/* check function touch_point_init, so driver can only operation gpio after touch_point_init */
	//if (!data->share_irq_pin || (data->share_irq_pin && data->key_idx == USSYS_KEY_0)) {
	if (!data->share_irq_pin || (data->share_irq_pin && !is_shared_gpio_requested)) {
		data->gpiod_irq = devm_gpiod_get(&client->dev, "touch_point,irq", GPIOD_IN);
		if (IS_ERR(data->gpiod_irq)) {
			dev_err(&client->dev, "get gpio failed");
			return PTR_ERR(data->gpiod_irq);
		}

		/* Save the gpio desc for other keys */
		if (data->share_irq_pin)
			shared_gpiod_irq = data->gpiod_irq;

		is_shared_gpio_requested = true;

		/* client->irq = gpiod_to_irq(data->gpiod_irq);
		if (client->irq < 0) {
			dev_err(&client->dev, "get irq failed");
			return rc;
		} */
	}

	/* before this, driver must not operation the gpio */
	if (data->share_irq_pin)
		data->gpiod_irq = shared_gpiod_irq;

	/* In case OTP fw is running at LPM, wakeup first to ensure fw go back to 20MHz */
	touch_point_toggle_syncio_pin(data);

	rc = touch_point_check_ussys_product(data);
	if (rc < 0) {
		dev_err(&client->dev, "no ussys device");
		return rc;
	}
	ussys_key_cnt++;

	/* For otp parts, need set HPM via fd. For non-otp parts, couldn't read/write i2c in fd due to chip is in bd */
	if (data->fac_status == FAC_W_OTP_WO_FW || data->fac_status == FAC_W_OTP_W_FW) {
		uint8_t sensor_ctl = 0;

		touch_point_enable_lpm(data, false);
		touch_point_read_16bit_reg(data, TOUCH_POINT_SENSOR_CTRL, &sensor_ctl, 1);
		if (sensor_ctl & TOUCH_POINT_REG_SENSOR_CTRL_LPM_MASK) {
			dev_info(&data->client->dev, "(probe)wake up failed");
		} else {
			data->lpm_on = false;
			dev_info(&data->client->dev, "(probe)wake up successfully");
		}
	}

	rc = sysfs_create_group(&client->dev.kobj, &touch_point_attr_group);
	if (rc < 0) {
		dev_err(&client->dev, "Failure %d creating sysfs group", rc);
		return rc;
	}
	rc = devm_add_action_or_reset(&client->dev, touch_point_remove_group, &client->dev);
	if (rc < 0)
		return rc;

	rc = touch_point_setup_input_dev(data);
	if (rc < 0)
		return rc;

	dev_info(&client->dev, "req firmware");
	rc = request_firmware_nowait(THIS_MODULE, true, TOUCH_POINT_FIRMWARE_NAME,
					&client->dev, GFP_KERNEL, data, touch_point_fw_dl_cb);
	if (rc) {
		dev_err(&client->dev, "req firmware failed");
		return rc;
	}

	data->miscdev_key_idx = key_cnt;
	dev_info(&client->dev, "key cnt:%d", key_cnt);
	snprintf(misc_dev_name, sizeof(misc_dev_name), "tp_key%d", key_cnt++);

	data->misc_dev.fops = &touch_point_misc_dev_fops;
	data->misc_dev.minor = MISC_DYNAMIC_MINOR;
	data->misc_dev.name = misc_dev_name;//client->name;
	rc = misc_register(&data->misc_dev);
	if (rc) {
		dev_err(&client->dev, "reg misc dev failed");
		return rc;
	}
	data->miscdev_minor = data->misc_dev.minor;

	dev_err(&client->dev, "miscdev_minor %d", data->miscdev_minor);

	rc = devm_add_action_or_reset(&client->dev, touch_point_dereg_misc_dev, &data->misc_dev);
	if (rc)
		return rc;

	data->touch_point_wq = create_singlethread_workqueue("touch-point cal");
	if (!data->touch_point_wq)
		return -ENOMEM;
	rc = devm_add_action_or_reset(&client->dev, touch_point_destroy_wq, data->touch_point_wq);
	if (rc)
		return rc;

#if USING_MSM_DRM_NOTIFIER
	rc = touch_point_register_drm(data);
	if (rc) {
		dev_err(&client->dev, "reg drm notifier failed");
		return rc;
	}
	rc = devm_add_action_or_reset(&client->dev, touch_point_unregister_drm, data);
	if (rc)
		return rc;
#else
#ifdef CONFIG_FB
	rc = touch_point_register_fb(data);
	if (rc) {
		dev_err(&client->dev, "reg fb notifier failed");
		return rc;
	}
	rc = devm_add_action_or_reset(&client->dev, touch_point_unregister_fb, data);
	if (rc)
		return rc;
#endif/*CONFIG_FB*/
#endif/*USING_MSM_DRM_NOTIFIER*/

	rc  = devm_request_irq(&client->dev, client->irq, touch_point_irq_handler,
			data->share_irq_pin ? IRQF_SHARED : 0, client->name, data);
	if (rc < 0) {
		dev_err(&client->dev, "request irq failed");
		return rc;
	}
	disable_irq(client->irq);

	tp_g_data[data->key_idx] = data;

	return 0;
}

void touch_point_shutdown(struct i2c_client *client)
{
	struct touch_point_data *data = i2c_get_clientdata(client);
	enum ussys_tp_lpm_odr poweroff_odr_value = LPM_ODR_5HZ;

	disable_irq(data->client->irq);
	touch_point_toggle_syncio_pin(data);
	enable_irq(data->client->irq);
	touch_point_lpm_odr_sel(data, poweroff_odr_value);
	touch_point_enable_lpm(data, true);
	dev_info(&client->dev, "touch_point_shutdown\n");
	//dump_stack();
}

#if USING_DRIVER_TO_CTL_POWER
static SIMPLE_DEV_PM_OPS(touch_point_pm_ops, touch_point_suspend, touch_point_resume);
#endif

static const struct i2c_device_id touch_point_id_table[] = {
	{"touch_point", 0},
	{ },
};
MODULE_DEVICE_TABLE(i2c, touch_point_id_table);

static struct i2c_driver touch_point_i2c_driver = {
	.driver = {
		.name = "touch_point",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = touch_point_of_id_table,
#endif
#if USING_DRIVER_TO_CTL_POWER
		.pm = &touch_point_pm_ops,
#endif
	},
	.probe = touch_point_probe,
	.shutdown = touch_point_shutdown,
	.id_table = touch_point_id_table,
};

module_i2c_driver(touch_point_i2c_driver);
