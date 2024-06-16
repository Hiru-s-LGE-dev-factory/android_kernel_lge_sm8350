/* drivers/intput/misc/akm09970.c - akm09970 compass driver
 *
 * Copyright (c) 2014-2020, Linux Foundation. All rights reserved.
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

//#define DEBUG
//#define pr_fmt(fmt) "akm09970: %s: %d " fmt, __func__, __LINE__

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/freezer.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#include <linux/pwm.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/hrtimer.h>

#include <linux/lge_hall_sensor_notifier.h>

#include <akm09970.h>

#define AKM9970_LOG_I(fmt, args...)\
	pr_info("[AKM9970][%s %d] "\
		fmt, __func__, __LINE__, ##args)

#define AKM9970_LOG_E(fmt, args...)\
	pr_err("[AKM9970_E][%s %d] "\
		fmt, __func__, __LINE__, ##args)

#define AKM_DRDY_TIMEOUT_MS		100
#define AKM_DEFAULT_MEASURE_HZ	100
#define AKM_I2C_NAME			"akm09970"

#define MAKE_S16(U8H, U8L) \
	(int16_t)(((uint16_t)(U8H) << 8) | (uint16_t)(U8L))

/* POWER SUPPLY VOLTAGE RANGE */
#define AKM09970_VDD_MIN_UV 1800000
#define AKM09970_VDD_MAX_UV 1800000

#define PWM_PERIOD_DEFAULT_NS 1000000


struct akm09970_soc_ctrl {
	struct i2c_client *client;
	struct input_dev *input;
	struct miscdevice miscdev;

	struct device_node *of_node;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;

	int gpio_rstn;
	int gpio_irq;

	int power_enabled;

	struct work_struct report_work;
	struct workqueue_struct *work_queue;

	struct mutex soc_mutex;
	wait_queue_head_t drdy_wq;
	atomic_t drdy;

	int irq;

	uint8_t sense_info[AKM_SENSOR_INFO_SIZE];
	uint8_t sense_data[AKM_SENSOR_DATA_SIZE];
	int32_t magnet_data[3];
	uint8_t layout;

	uint32_t measure_freq_hz;
	uint8_t measure_mode;
	uint8_t measure_range;

	uint8_t  test_event;
	uint16_t mag_status;
	uint16_t mag_status_ext;

	uint8_t DRDENbit;
	uint8_t SWX1ENbit;
	uint8_t SWX2ENbit;
	uint8_t SWY1ENbit;
	uint8_t SWY2ENbit;
	uint8_t SWZ1ENbit;
	uint8_t SWZ2ENbit;
	uint8_t ERRXYENbit;

	uint8_t ERRADCENbit;
	uint8_t INTENbit;
	uint8_t ODINTENbit;

	uint8_t SDRbit;

	uint16_t BOP1Xbits;
	uint16_t BRP1Xbits;
	uint16_t BOP2Xbits;
	uint16_t BRP2Xbits;

	uint16_t BOP1Ybits;
	uint16_t BRP1Ybits;
	uint16_t BOP2Ybits;
	uint16_t BRP2Ybits;

	uint16_t BOP1Zbits;
	uint16_t BRP1Zbits;
	uint16_t BOP2Zbits;
	uint16_t BRP2Zbits;

	uint16_t hall_detected;
};

static int ak09970_setup(struct akm09970_soc_ctrl *pctrl);

struct _akm09970_pd_handler {
	int ref_count;
	struct mutex lock;
	struct akm09970_soc_ctrl *data;
} akm09970_pd_handler = {
	.ref_count = -1,
	.data = NULL,
};

static int akm_write_byte(struct i2c_client *client, u8 reg, u8 val)
{
	int rc;

	/* As per specification, disable irq in between register writes */
	if (client->irq)
		disable_irq_nosync(client->irq);

	rc = i2c_smbus_write_byte_data(client, reg, val);

	if (client->irq)
		enable_irq(client->irq);

	return rc;
}

static int ak09970_i2c_writes(struct i2c_client *client,
									const u8 *tx, size_t wlen)
{
	int ret;

	ret = i2c_master_send(client, tx, wlen);

	if(ret != wlen) {
		AKM9970_LOG_E("comm error, ret %d, wlen %d\n", ret, (int)wlen);
		pr_debug("%s return %d\n", __func__, ret);
	}

	return ret;
}

static int akm_write_block(struct i2c_client *client, u8 reg, u8 *val, uint8_t val_len)
{
	int i, n, rc;
	u8  tx[25];

	if ( val_len > 24 ) {
		return -EIO;
	}

	if (client->irq)
		disable_irq_nosync(client->irq);

	n = 0;
	tx[n++] = reg;
	for ( i = 0 ; i < val_len ; i++ ) {
		tx[n++] = val[i];
	}

	rc = ak09970_i2c_writes(client, tx, n);

	if (client->irq)
		enable_irq(client->irq);

	return rc;
}

static s32 ak09970_i2c_write16(struct i2c_client *client, 
				u8 address, int valueNum, u16 value1, u16 value2)
{
	u8  tx[5];
	s32 ret;
	int n;

	if ( ( valueNum != 1 ) &&  ( valueNum != 2 ) ) {
		AKM9970_LOG_E("valueNum error, valueNum= %d\n", valueNum);
		return -EINVAL;
	}

	n = 0;

	tx[n++] = address;
	tx[n++] = (u8)((0xFF00 & value1) >> 8);
	tx[n++] = (u8)(0xFF & value1);

	if ( valueNum == 2 ) {
		tx[n++] = (u8)((0xFF00 & value2) >> 8);
		tx[n++] = (u8)(0xFF & value2);
	}

	ret = ak09970_i2c_writes(client, tx, n);

	pr_debug("%s return %d\n", __func__, ret);

	return ret;
}

static int akm_set_reg_bits(struct i2c_client *client,
					int val, int shift, u8 mask, u8 reg)
{
	int data;

	data = i2c_smbus_read_byte_data(client, reg);
	if ( data < 0 ) return data;

	data = (data & ~mask) | ((val << shift) & mask);
	AKM9970_LOG_I("AK09970 reg: 0x%x, data: 0x%x\n", reg, data);

	return akm_write_byte(client, reg, data); 
}

static int akm_set_smr(
	struct akm09970_soc_ctrl *pctrl,
	uint8_t mode)
{
	int rc;

	rc = akm_set_reg_bits(pctrl->client, mode, AK09970_SMR_MODE_POS,
				AK09970_SMR_MODE_MSK, AK09970_SMR_MODE_REG);

	return rc;
}

static int akm_set_mode(
	struct akm09970_soc_ctrl *pctrl,
	uint8_t mode)
{
	int rc;

	rc = akm_set_reg_bits(pctrl->client, mode, AK09970_MODE_POS,
				AK09970_MODE_MSK, AK09970_MODE_REG);

	return rc;
}

static void akm_reset(
	struct akm09970_soc_ctrl *pctrl)
{
	gpio_set_value(pctrl->gpio_rstn, 0);
	udelay(20);
	gpio_set_value(pctrl->gpio_rstn, 1);
	udelay(100);

	atomic_set(&pctrl->drdy, 0);
	return;
}

static int akm_power_down(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;
	AKM9970_LOG_I("power down called\n");
	if (pctrl->power_enabled) {
		gpio_set_value(pctrl->gpio_rstn, 0);
		AKM9970_LOG_I("power down\n");
		atomic_set(&pctrl->drdy, 0);
	}

	return rc;
}

static int akm_power_up(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;
	AKM9970_LOG_I("power up called\n");
	if (!pctrl->power_enabled) {
		AKM9970_LOG_I("power up\n");
		akm_reset(pctrl);
		return rc;
	}

	return rc;
}

/* This function will block a process until the latest measurement
 * data is available.
 */
static int akm_get_data(
	struct akm09970_soc_ctrl *pctrl,
	int *rbuf,
	int size)
{

	long timeout, try_count;
	int i;
	int rc = 0;
	uint8_t buffer[AKM_SENSOR_DATA_SIZE];

	if (pctrl->measure_mode == AK09970_MODE_SINGLE_MEASUREMENT){
		if ( pctrl->power_enabled == false ) {
			rc = akm_set_mode(pctrl, AK09970_MODE_SINGLE_MEASUREMENT);
		}
		try_count = 0;
		/* Check ST bit */
		do
		{
			/* Read data */
			rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_ST_XYZ, AKM_SENSOR_DATA_SIZE, buffer);
			if (rc < 0) {
				AKM9970_LOG_E("read data failed!\n");
				for ( i = 0 ; i < AKM_SENSOR_DATA_SIZE ; i ++ ) {
					buffer[i] = 0;
				}
			}
			/* ERRADC is */
			if (AKM_ERRADC_IS_HIGH(buffer[0])) {
				AKM9970_LOG_E("ADC over run!\n");
			}
			/* ERRXY is */
			if (AKM_ERRXY_IS_HIGH(buffer[1])) {
				AKM9970_LOG_E("errxy over run!\n");
			}
			/* Check ST bit */
			if (!(AKM_DRDY_IS_HIGH(buffer[1]))) {
				AKM9970_LOG_I("DRDY is low. Use last value.\n");
				try_count++;
			} else {
				break;
			}
		}while(try_count < 10);
		memcpy(pctrl->sense_data, buffer, AKM_SENSOR_DATA_SIZE);
		pctrl->power_enabled = false;
	}
	else{
		/* Block! */
		timeout = wait_event_interruptible_timeout(
				pctrl->drdy_wq,
				atomic_read(&pctrl->drdy),
				msecs_to_jiffies(AKM_DRDY_TIMEOUT_MS));

		if (!timeout) {
			AKM9970_LOG_E("wait_event timeout\n");
			for ( i = 0 ; i < AKM_SENSOR_DATA_SIZE ; i ++ ) {
				pctrl->sense_data[i] = 0;
			}
			rc = -EIO;
		}
	}

	pctrl->mag_status = ((u16)(0x3 & pctrl->sense_data[0]) << 8) + (u16)pctrl->sense_data[1];
	pctrl->magnet_data[0] = MAKE_S16(pctrl->sense_data[6], pctrl->sense_data[7]);
	pctrl->magnet_data[1] = MAKE_S16(pctrl->sense_data[4], pctrl->sense_data[5]);
	pctrl->magnet_data[2] = MAKE_S16(pctrl->sense_data[2], pctrl->sense_data[3]);

	rbuf[0] = pctrl->mag_status;
	for ( i = 1 ; i < size ; i ++ ) {
		rbuf[i] = pctrl->magnet_data[i-1];
	}
	atomic_set(&pctrl->drdy, 0);

	return rc;
}


static int akm_get_threshold(struct akm09970_soc_ctrl *pctrl, struct akm09970_threshold *threshold)

{
	uint8_t buffer[AKM_SENSOR_THRESHOLD_SIZE];
	int rc;

	/* Read data */
	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_THRESHOLD, AKM_SENSOR_THRESHOLD_SIZE, buffer);

	threshold->bop1x = ((buffer[0] << 8) & 0xFF00) | (buffer[1] & 0x00FF);
	threshold->brp1x = ((buffer[2] << 8) & 0xFF00) | (buffer[3] & 0x00FF);
	threshold->bop2x = ((buffer[4] << 8) & 0xFF00) | (buffer[5] & 0x00FF);
	threshold->brp2x = ((buffer[6] << 8) & 0xFF00) | (buffer[7] & 0x00FF);
	threshold->bop1y = ((buffer[8] << 8) & 0xFF00) | (buffer[9] & 0x00FF);
	threshold->brp1y = ((buffer[10] << 8) & 0xFF00) | (buffer[11] & 0x00FF);
	threshold->bop2y = ((buffer[12] << 8) & 0xFF00) | (buffer[13] & 0x00FF);
	threshold->brp2y = ((buffer[14] << 8) & 0xFF00) | (buffer[15] & 0x00FF);
	threshold->bop1z = ((buffer[16] << 8) & 0xFF00) | (buffer[17] & 0x00FF);
	threshold->brp1z = ((buffer[18] << 8) & 0xFF00) | (buffer[19] & 0x00FF);
	threshold->bop2z = ((buffer[20] << 8) & 0xFF00) | (buffer[21] & 0x00FF);
	threshold->brp2z = ((buffer[22] << 8) & 0xFF00) | (buffer[23] & 0x00FF);
	return rc;
}
static int akm_set_threshold(struct akm09970_soc_ctrl *pctrl, struct akm09970_threshold *threshold)
{
	int rc = 0;
	u8 val[24];
	int i;
	for(i = 0; i < 24; i++)
	{
		val[i] = (u8)threshold->reg[i];
	}

	rc = akm_write_block(pctrl->client, AK09970_REG_THRESHOLD,
			val, sizeof(val));
	return rc;
}

static int akm_active(struct akm09970_soc_ctrl *pctrl, bool on)
{
	int rc = 0;
	uint8_t mode;

	AKM9970_LOG_I("akm sensor %s\n", on ? "on" : "off");

	if (!pctrl->power_enabled && on) {
		pctrl->power_enabled = true;
		rc = akm_power_up(pctrl);
		if (rc) {
			AKM9970_LOG_E("Sensor power up fail!\n");
			pctrl->power_enabled = false;
			return rc;
		}

		if ( pctrl->measure_mode != AK09970_MODE_SINGLE_MEASUREMENT ) {
			AKM9970_LOG_I("enable irq\n");
			enable_irq(pctrl->client->irq);
		}

		if (pctrl->measure_freq_hz >= 100)
			mode = AK09970_MODE_CONTINUOUS_100HZ;
		else if (pctrl->measure_freq_hz >= 50 && pctrl->measure_freq_hz < 100)
			mode = AK09970_MODE_CONTINUOUS_50HZ;
		else if (pctrl->measure_freq_hz >= 20 && pctrl->measure_freq_hz < 50)
			mode = AK09970_MODE_CONTINUOUS_20HZ;
		else if (pctrl->measure_freq_hz >= 10 && pctrl->measure_freq_hz < 20)
			mode = AK09970_MODE_CONTINUOUS_10HZ;
		else if (pctrl->measure_freq_hz >= 1 && pctrl->measure_freq_hz < 10)
			mode = AK09970_MODE_CONTINUOUS_1HZ;
		else
			mode = AK09970_MODE_SINGLE_MEASUREMENT;

		pctrl->measure_mode = mode;
		rc = akm_set_mode(pctrl, mode);
		if (rc < 0) {
			AKM9970_LOG_E("Failed to set to mode(%d)\n", mode);
			pctrl->power_enabled = false;
			return rc;
		}
		rc = akm_set_smr(pctrl, pctrl->measure_range);
		if (rc < 0) {
			AKM9970_LOG_E("Failed to set smr.\n");
			pctrl->power_enabled = false;
			return rc;
		}

		//setup becaused it might be cleared after reset
		rc = ak09970_setup(pctrl);
		if (rc) {
			AKM9970_LOG_E("%s ak09970_setup failed %d\n", __func__, rc);
			return rc;
		}

	} else if (pctrl->power_enabled && !on) {
		pctrl->power_enabled = false;

		if ( pctrl->measure_mode != AK09970_MODE_SINGLE_MEASUREMENT ) {
			disable_irq_nosync(pctrl->client->irq);
			cancel_work_sync(&pctrl->report_work);
			AKM9970_LOG_I("disable irq\n");
		}

		rc = akm_set_mode(pctrl, AK09970_MODE_POWERDOWN);
		if (rc)
			pr_warn("Failed to set to POWERDOWN mode.\n");

		akm_power_down(pctrl);
	} else {
		AKM9970_LOG_I("power state is the same, do nothing!\n");
	}

	return 0;
}

static long akm09970_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	uint8_t active = 0;
	uint8_t mode = AK09970_MODE_POWERDOWN;
	int dat_buf[4];/* for GET_DATA */
	int state = 1;
	struct akm09970_soc_ctrl *pctrl = NULL;
	struct akm09970_threshold threshold;

	pctrl = (struct akm09970_soc_ctrl*)filp->private_data;

	if (pctrl == NULL)
		return -EFAULT;

	switch (cmd) {
	case AKM_IOC_SET_ACTIVE:
		AKM9970_LOG_I("AKM_IOC_SET_ACTIVE.\n");
		if (copy_from_user(&active, (uint8_t *)arg, sizeof(uint8_t))) {
			AKM9970_LOG_E("Failed to active status from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		rc = akm_active(pctrl, !!active);
		break;
	case AKM_IOC_SET_MODE:
		AKM9970_LOG_I("AKM_IOC_SET_MODE.\n");
		if (copy_from_user(&mode, (uint8_t *)arg, sizeof(uint8_t))) {
			AKM9970_LOG_E("Failed to copy mode from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		pctrl->measure_freq_hz = mode;

		break;
	case AKM_IOC_SET_PRESS:
		AKM9970_LOG_I("AKM_IOC_SET_PRESS.\n");
		input_event(pctrl->input, EV_KEY, KEY_BACK, state);
		input_sync(pctrl->input);
		input_event(pctrl->input, EV_KEY, KEY_BACK, !state);
		input_sync(pctrl->input);
		break;
	case AKM_IOC_GET_SENSSMR:
		if (copy_to_user((void __user *)arg, &pctrl->measure_range, sizeof(uint8_t))) {
			AKM9970_LOG_E("copy_to_user failed.\n");
			return -EFAULT;
		}
		break;
	case AKM_IOC_GET_SENSEDATA:
		AKM9970_LOG_I("AKM_IOC_GET_SENSEDATA.\n");
		rc = akm_get_data(pctrl, dat_buf, 4);
		if (copy_to_user((void __user *)arg, dat_buf, sizeof(dat_buf))) {
			AKM9970_LOG_E("copy_to_user failed.\n");
			rc = -EFAULT;
		}
		break;
	case AKM_IOC_SET_THRESHOLD:
		AKM9970_LOG_I("AKM_IOC_SET_THRESHOLD.\n");
		if (copy_from_user(&threshold, (uint8_t *)arg, sizeof(threshold))) {
			AKM9970_LOG_E("Failed to threshold from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		rc = akm_set_threshold(pctrl, &threshold);
		break;
	case AKM_IOC_GET_THRESHOLD:
		AKM9970_LOG_I("AKM_IOC_GET_THRESHOLD.\n");
		rc = akm_get_threshold(pctrl, &threshold);
		if (rc < 0)
			return rc;
		if (copy_to_user((void __user *)arg, &threshold, sizeof(threshold))) {
			AKM9970_LOG_E("copy_to_user failed.\n");
			return -EFAULT;
		}
		break;
	default:
		pr_warn("unsupport cmd:0x%x\n", cmd);
		break;
	}

	return rc;
}

#ifdef CONFIG_COMPAT
static long akm09970_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return akm09970_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int init_akm09970_pd(struct akm09970_soc_ctrl *data)
{
	struct _akm09970_pd_handler *akm09970 = &akm09970_pd_handler;

	if (data == NULL)
		return -EFAULT;

	mutex_init(&akm09970->lock);

	mutex_lock(&akm09970->lock);
	akm09970->data = data;
	mutex_unlock(&akm09970->lock);

	return 0;
}

static struct akm09970_soc_ctrl* get_akm09970_pd(void)
{
	struct _akm09970_pd_handler *akm09970 = &akm09970_pd_handler;

	if (akm09970->data == NULL)
		return NULL;

	mutex_lock(&akm09970->lock);
	akm09970->ref_count++;
	mutex_unlock(&akm09970->lock);

	return akm09970->data;
}

static int rel_akm09970_pd(struct akm09970_soc_ctrl *data)
{
	struct _akm09970_pd_handler *akm09970 = &akm09970_pd_handler;

	if (akm09970->data == NULL)
		return -EFAULT;

	mutex_lock(&akm09970->lock);
	akm09970->ref_count--;
	mutex_unlock(&akm09970->lock);

	data = NULL;

	return 0;
}

/* AK7719 Misc driver interfaces */
static int akm09970_open(struct inode *inode, struct file *file)
{
	struct akm09970_soc_ctrl *pctrl = NULL;

	pctrl = get_akm09970_pd();
	file->private_data = pctrl;

	return 0;
}


static int akm09970_close(struct inode *inode, struct file *file)
{
	struct akm09970_soc_ctrl *akm09970 = (struct akm09970_soc_ctrl*)file->private_data;

	rel_akm09970_pd(akm09970);

	return 0;
}


static const struct file_operations akm09970_fops = {
	.owner   = THIS_MODULE,
	.open    = akm09970_open,
	.release = akm09970_close,
	.unlocked_ioctl = akm09970_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = akm09970_compat_ioctl,
#endif
};

/* akm get and report data functions */
static int akm_read_sense_data(
	struct akm09970_soc_ctrl *pctrl,
	uint8_t *rbuf,
	int size)
{
	uint8_t buffer[AKM_SENSOR_DATA_SIZE];
	int i;
	int rc;

	/* Read data */
	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_ST_XYZ, AKM_SENSOR_DATA_SIZE, buffer);
	if (rc < 0) {
		for ( i = 0 ; i < size ; i++ ){
			rbuf[i] = 0;
		}
		AKM9970_LOG_E("read data failed!\n");
		return rc;
	}

	memcpy(rbuf, buffer, size);

	/* ERRADC is */
	if (AKM_ERRADC_IS_HIGH(buffer[0])) {
		AKM9970_LOG_E("ADC over run!\n");
		rc = -EIO;
	}

	/* ERRXY is */
	if (AKM_ERRXY_IS_HIGH(buffer[1])) {
		AKM9970_LOG_E("errxy over run!\n");
		rc = -EIO;
	}


	/* Check ST bit */
	if (!(AKM_DRDY_IS_HIGH(buffer[1]))) {
		AKM9970_LOG_I("DRDY is low. Use last value.\n");
	} else {
		atomic_set(&pctrl->drdy, 1);
		wake_up(&pctrl->drdy_wq);
	}

	return 0;
}

static int akm_report_data(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;
	int tmp, mag_x, mag_y, mag_z;
	short brp1x_signed, bop1x_signed;
	short thresh_signed;

	rc = akm_read_sense_data(pctrl, pctrl->sense_data, AKM_SENSOR_DATA_SIZE);
	if (rc) {
		AKM9970_LOG_E("Get data failed.\n");
		return -EIO;
	}

	mag_x = MAKE_S16(pctrl->sense_data[6], pctrl->sense_data[7]);
	mag_y = MAKE_S16(pctrl->sense_data[4], pctrl->sense_data[5]);
	mag_z = MAKE_S16(pctrl->sense_data[2], pctrl->sense_data[3]);

	AKM9970_LOG_I("raw data: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			pctrl->sense_data[0], pctrl->sense_data[1], pctrl->sense_data[2], pctrl->sense_data[3],
			pctrl->sense_data[4], pctrl->sense_data[5], pctrl->sense_data[6], pctrl->sense_data[7]);

	AKM9970_LOG_I("x_axis: %d, y_axis: %d, z_axis: %d\n", mag_x, mag_y, mag_z);

	switch (pctrl->layout) {
	case 0:
	case 1:
		/* Fall into the default direction */
		break;
	case 2:
		tmp = mag_x;
		mag_x = mag_y;
		mag_y = -tmp;
		break;
	case 3:
		mag_x = -mag_x;
		mag_y = -mag_y;
		break;
	case 4:
		tmp = mag_x;
		mag_x = -mag_y;
		mag_y = tmp;
		break;
	case 5:
		mag_x = -mag_x;
		mag_z = -mag_z;
		break;
	case 6:
		tmp = mag_x;
		mag_x = mag_y;
		mag_y = tmp;
		mag_z = -mag_z;
		break;
	case 7:
		mag_y = -mag_y;
		mag_z = -mag_z;
		break;
	case 8:
		tmp = mag_x;
		mag_x = -mag_y;
		mag_y = -tmp;
		mag_z = -mag_z;
		break;
	}

	pctrl->mag_status = (0x3FF & (((u16)pctrl->sense_data[0] << 8) + (u16)pctrl->sense_data[1]));

	if(pctrl->BRP1Xbits < 32768)
		brp1x_signed = (short)pctrl->BRP1Xbits;
	else
		brp1x_signed = (short)((int)pctrl->BRP1Xbits-65536);

	if(pctrl->BOP1Xbits < 32768)
		bop1x_signed = (short)pctrl->BOP1Xbits;
	else
		bop1x_signed = (short)((int)pctrl->BOP1Xbits-65536);

	if(abs(brp1x_signed) - abs(bop1x_signed) > 0)
	{
		thresh_signed = brp1x_signed;
	}
	else
	{
		thresh_signed = bop1x_signed;
	}

	if(thresh_signed >= 0)
	{
		if ( mag_x >= thresh_signed)
		{
			pctrl->hall_detected = 1;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_CLOSE);
		}
		else
		{
			pctrl->hall_detected = 0;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_OPEN);
		}
	}
	else
	{
		if ( mag_x <= thresh_signed)
		{
			pctrl->hall_detected = 1;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_CLOSE);
		}
		else
		{
			pctrl->hall_detected = 0;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_OPEN);
		}
	}

	AKM9970_LOG_I("x_axis: %d, thresh_signed: %d, hall_detected: %d\n", mag_x, thresh_signed, pctrl->hall_detected);

	input_report_abs(pctrl->input, ABS_MISC, pctrl->hall_detected); // report hall state as input event
	//input_report_abs(pctrl->input, ABS_MISC, (pctrl->mag_status_ext | pctrl->mag_status));
	input_report_abs(pctrl->input, ABS_X, mag_x);
	input_report_abs(pctrl->input, ABS_Y, mag_y);
	input_report_abs(pctrl->input, ABS_Z, mag_z);

	input_sync(pctrl->input);

	if ( pctrl->mag_status_ext == 0 ) {
		pctrl->mag_status_ext = 0x400;
	}
	else {
		pctrl->mag_status_ext = 0x0;
	}

	pctrl->magnet_data[0] = mag_x;
	pctrl->magnet_data[1] = mag_y;
	pctrl->magnet_data[2] = mag_z;

	return 0;
}

static int akm_initial_hall_state(struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;
	int tmp, mag_x, mag_y, mag_z;
	short brp1x_signed, bop1x_signed;
	short thresh_signed;


	rc = akm_read_sense_data(pctrl, pctrl->sense_data, AKM_SENSOR_DATA_SIZE);
	if (rc) {
		AKM9970_LOG_E("Get data failed.\n");
		return -EIO;
	}

	mag_x = MAKE_S16(pctrl->sense_data[6], pctrl->sense_data[7]);
	mag_y = MAKE_S16(pctrl->sense_data[4], pctrl->sense_data[5]);
	mag_z = MAKE_S16(pctrl->sense_data[2], pctrl->sense_data[3]);

	AKM9970_LOG_I("raw data: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			pctrl->sense_data[0], pctrl->sense_data[1], pctrl->sense_data[2], pctrl->sense_data[3],
			pctrl->sense_data[4], pctrl->sense_data[5], pctrl->sense_data[6], pctrl->sense_data[7]);

	AKM9970_LOG_I("x_axis: %d, y_axis: %d, z_axis: %d\n", mag_x, mag_y, mag_z);

	switch (pctrl->layout) {
	case 0:
	case 1:
		/* Fall into the default direction */
		break;
	case 2:
		tmp = mag_x;
		mag_x = mag_y;
		mag_y = -tmp;
		break;
	case 3:
		mag_x = -mag_x;
		mag_y = -mag_y;
		break;
	case 4:
		tmp = mag_x;
		mag_x = -mag_y;
		mag_y = tmp;
		break;
	case 5:
		mag_x = -mag_x;
		mag_z = -mag_z;
		break;
	case 6:
		tmp = mag_x;
		mag_x = mag_y;
		mag_y = tmp;
		mag_z = -mag_z;
		break;
	case 7:
		mag_y = -mag_y;
		mag_z = -mag_z;
		break;
	case 8:
		tmp = mag_x;
		mag_x = -mag_y;
		mag_y = -tmp;
		mag_z = -mag_z;
		break;
	}

	pctrl->mag_status = (0x3FF & (((u16)pctrl->sense_data[0] << 8) + (u16)pctrl->sense_data[1]));

	if(pctrl->BRP1Xbits < 32768)
		brp1x_signed = (short)pctrl->BRP1Xbits;
	else
		brp1x_signed = (short)((int)pctrl->BRP1Xbits-65536);

	if(pctrl->BOP1Xbits < 32768)
		bop1x_signed = (short)pctrl->BOP1Xbits;
	else
		bop1x_signed = (short)((int)pctrl->BOP1Xbits-65536);

	if(abs(brp1x_signed) - abs(bop1x_signed) > 0)
	{
		thresh_signed = brp1x_signed;
	}
	else
	{
		thresh_signed = bop1x_signed;
	}

	if(thresh_signed >= 0)
	{
		if ( mag_x > thresh_signed)
		{
			pctrl->hall_detected = 1;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_CLOSE);
		}
		else
		{
			pctrl->hall_detected = 0;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_OPEN);
		}
	}
	else
	{
		if ( mag_x < thresh_signed)
		{
			pctrl->hall_detected = 1;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_CLOSE);
		}
		else
		{
			pctrl->hall_detected = 0;
			lge_hall_sensor_notifier_call_chain(LGE_HALL_SENSOR_STATE_CHANGED, LGE_HALL_SENSOR_OPEN);
		}
	}

	input_report_abs(pctrl->input, ABS_MISC, pctrl->hall_detected); // report hall state as input event
	//input_report_abs(pctrl->input, ABS_MISC, (pctrl->mag_status_ext | pctrl->mag_status));
	input_report_abs(pctrl->input, ABS_X, mag_x);
	input_report_abs(pctrl->input, ABS_Y, mag_y);
	input_report_abs(pctrl->input, ABS_Z, mag_z);

	input_sync(pctrl->input);

	if ( pctrl->mag_status_ext == 0 ) {
		pctrl->mag_status_ext = 0x400;
	}
	else {
		pctrl->mag_status_ext = 0x0;
	}

	pctrl->magnet_data[0] = mag_x;
	pctrl->magnet_data[1] = mag_y;
	pctrl->magnet_data[2] = mag_z;

	return 0;
}

static void akm_dev_input_work(struct work_struct *work)
{
	int rc = 0;
	struct akm09970_soc_ctrl *pctrl;

	pctrl = container_of(work, struct akm09970_soc_ctrl, report_work);

	rc = akm_report_data(pctrl);
	if (rc < 0)
		pr_warn("Failed to report data.");
}

static irqreturn_t akm_irq(int irq, void *handle)
{
	struct akm09970_soc_ctrl *pctrl = handle;

	queue_work(pctrl->work_queue, &pctrl->report_work);

	AKM9970_LOG_I("enter");
	return IRQ_HANDLED;
}

int akm_input_open(struct input_dev *dev)
{
	pr_info("enter!!!!!");
	return 0;
}

void akm_input_close(struct input_dev *dev)
{
	pr_info("enter!!!!!");
	return;
}

static int akm09970_input_register(
	struct input_dev **input,
	struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;

	/* Declare input device */
	*input = input_allocate_device();
	if (!*input)
		return -ENOMEM;

	/* Setup input device */
	set_bit(EV_ABS, (*input)->evbit);
	set_bit(EV_KEY, (*input)->evbit);

	input_set_abs_params(*input, ABS_MISC,
			     0, 0x3FF, 0, 0);
	input_set_abs_params(*input, ABS_X,
			-32767, 32767, 0, 0);
	input_set_abs_params(*input, ABS_Y,
			-32767, 32767, 0, 0);
	input_set_abs_params(*input, ABS_Z,
			-32767, 32767, 0, 0);

	input_set_capability(*input, EV_KEY, KEY_BACK);

	/* Set name */
	(*input)->name = AKM_INPUT_DEVICE_NAME;
	(*input)->dev.init_name = AKM_INPUT_DEVICE_NAME;
	(*input)->open = akm_input_open;
	(*input)->close = akm_input_close;

	input_set_drvdata(pctrl->input, pctrl);

	/* Register */
	rc = input_register_device(*input);
	if (rc) {
		input_free_device(*input);
		return rc;
	}

	return rc;
}

static int akm_i2c_check_device(
	struct i2c_client *client)
{
	int rc = 0;
	struct akm09970_soc_ctrl *pctrl = i2c_get_clientdata(client);

	rc = i2c_smbus_read_i2c_block_data(client, AK09970_REG_WIA, AKM_SENSOR_INFO_SIZE, pctrl->sense_info);
	if (rc < 0) {
		dev_err(&client->dev, "%s(), I2C access failed: %d", __func__, rc);
		return rc;
	}

	/* Check read data */
	if ((pctrl->sense_info[0] != AK09970_WIA1_VALUE) ||
			(pctrl->sense_info[1] != AK09970_WIA2_VALUE)) {
		AKM9970_LOG_E("The device is not AKM Compass(AK09970).\n");
		return -ENXIO;
	}

	return rc;
}

static int akm09970_gpio_config(struct akm09970_soc_ctrl *pctrl)
{
	int32_t rc = 0;

	rc = gpio_request_one(pctrl->gpio_rstn, GPIOF_OUT_INIT_LOW, "akm09970-rstn");
	if (rc < 0) {
		AKM9970_LOG_E("Failed to request power enable GPIO %d", pctrl->gpio_rstn);
		goto fail0;
	}
	gpio_direction_output(pctrl->gpio_rstn, 0);

	rc = gpio_request_one(pctrl->gpio_irq, GPIOF_IN, "akm09970-irq");
	if (rc < 0) {
		AKM9970_LOG_E("Failed to request power enable GPIO %d", pctrl->gpio_irq);
		goto fail1;
	}
	gpio_direction_input(pctrl->gpio_irq);

	return 0;

fail1:
	if (gpio_is_valid(pctrl->gpio_rstn))
		gpio_free(pctrl->gpio_rstn);
fail0:
	return rc;
}

static int ak09970_setup(struct akm09970_soc_ctrl *pctrl)
{
	s32 err;
	u8   value;
	u16  value1;

	value1 = (u16)(pctrl->ERRADCENbit + (pctrl->INTENbit << 1) + (pctrl->ODINTENbit << 2));
	value1 <<= 8;
	value1 += ((u16)(pctrl->DRDENbit + (pctrl->SWX1ENbit << 1) + (pctrl->SWX2ENbit << 2) 
              + (pctrl->SWY1ENbit << 3) + (pctrl->SWY2ENbit << 4) 
                 + (pctrl->SWZ1ENbit << 5) + (pctrl->SWZ2ENbit << 6) 
                    + (pctrl->ERRXYENbit << 7))); 

	err = ak09970_i2c_write16(pctrl->client, AK09970_REG_CNTL1, 1, value1, 0);

	if (pctrl->measure_freq_hz >= 100)
		value = AK09970_MODE_CONTINUOUS_100HZ;
	else if (pctrl->measure_freq_hz >= 50 && pctrl->measure_freq_hz < 100)
		value = AK09970_MODE_CONTINUOUS_50HZ;
	else if (pctrl->measure_freq_hz >= 20 && pctrl->measure_freq_hz < 50)
		value = AK09970_MODE_CONTINUOUS_20HZ;
	else if (pctrl->measure_freq_hz >= 10 && pctrl->measure_freq_hz < 20)
		value = AK09970_MODE_CONTINUOUS_10HZ;
	else if (pctrl->measure_freq_hz >= 1 && pctrl->measure_freq_hz < 10)
		value = AK09970_MODE_CONTINUOUS_1HZ;
	else
		value = AK09970_MODE_SINGLE_MEASUREMENT;

	pctrl->measure_mode = value;

	value += (pctrl->SDRbit << 4) + (pctrl->measure_range << 5);

	err = akm_write_byte(pctrl->client, AK09970_REG_CNTL2, value);

	err = ak09970_i2c_write16(pctrl->client, AK09970_REG_THRESHOLD, 2, pctrl->BOP1Xbits, pctrl->BRP1Xbits);
	err = ak09970_i2c_write16(pctrl->client, AK09970_REG_TH2X, 2, pctrl->BOP2Xbits, pctrl->BRP2Xbits);
	err = ak09970_i2c_write16(pctrl->client, AK09970_REG_TH1Y, 2, pctrl->BOP1Ybits, pctrl->BRP1Ybits);
	err = ak09970_i2c_write16(pctrl->client, AK09970_REG_TH2Y, 2, pctrl->BOP2Ybits, pctrl->BRP2Ybits);
	err = ak09970_i2c_write16(pctrl->client, AK09970_REG_TH1Z, 2, pctrl->BOP1Zbits, pctrl->BRP1Zbits);
	err = ak09970_i2c_write16(pctrl->client, AK09970_REG_TH2Z, 2, pctrl->BOP2Zbits, pctrl->BRP2Zbits);

	pctrl->test_event = 0;
	pctrl->mag_status = 0;
	pctrl->mag_status_ext = 0;
	pctrl->layout = 0;

	return 0;
}

int akm09970_parse_dt(struct device *dev,
				struct akm09970_soc_ctrl *pctrl)
{
	int rc = 0;
	u32 buf[8];
	struct device_node *np;

	np = dev->of_node;

	pctrl->gpio_rstn = of_get_named_gpio_flags(np, "akm,gpio_rstn", 0, NULL);
	if (!gpio_is_valid(pctrl->gpio_rstn)) {
		pr_err("gpio reset pin %d is invalid.",
			pctrl->gpio_rstn);
		pctrl->gpio_rstn = -1;
	}
	else {
		AKM9970_LOG_I("reset pin %d", pctrl->gpio_rstn);
	}

	pctrl->gpio_irq = of_get_named_gpio_flags(np, "akm,gpio_irq", 0, NULL);
	if (!gpio_is_valid(pctrl->gpio_irq)) {
		pr_err("gpio irq pin %d is invalid.",
			pctrl->gpio_irq);
		return -EINVAL;
	}
	else {
		AKM9970_LOG_I("irq pin %d", pctrl->gpio_irq);
	}

	pctrl->client->irq = gpio_to_irq(pctrl->gpio_irq);

	rc = of_property_read_u32(dev->of_node, "akm,measure-freq-hz", &pctrl->measure_freq_hz);
	if (rc < 0) {
		AKM9970_LOG_I("akm,measure-freq-hz not set, use default.\n");
		pctrl->measure_freq_hz = AKM_DEFAULT_MEASURE_HZ;
	}

	rc= of_property_read_u32_array(np, "akm,DRDY_event", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->DRDENbit = 1;
	}
	else {
		pctrl->DRDENbit = buf[0];
	}
	AKM9970_LOG_I("DRDY_event %d", pctrl->DRDENbit);

	rc = of_property_read_u32_array(np, "akm,ERRXY_event", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->ERRXYENbit = 1;
	}
	else {
		pctrl->ERRXYENbit = buf[0];
	}
	AKM9970_LOG_I("ERRXY_event %d", pctrl->ERRXYENbit);

	rc = of_property_read_u32_array(np, "akm,ERRADC_event", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->ERRADCENbit = 1;
	}
	else {
		pctrl->ERRADCENbit = buf[0];
	}
	AKM9970_LOG_I("ERRADC_event %d", pctrl->ERRADCENbit);

	rc = of_property_read_u32_array(np, "akm,INT_event", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->INTENbit = 1;
	}
	else {
		pctrl->INTENbit = buf[0];
	}
	AKM9970_LOG_I("INT_event %d", pctrl->INTENbit);

	rc = of_property_read_u32_array(np, "akm,ODINT_event", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->ODINTENbit = 1;
	}
	else {
		pctrl->ODINTENbit = buf[0];
	}
	AKM9970_LOG_I("ODINT_event %d", pctrl->ODINTENbit);

	rc = of_property_read_u32_array(np, "akm,drive_setting", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->SDRbit = 0;
	}
	else {
		pctrl->SDRbit = buf[0];
	}
	AKM9970_LOG_I("drive_setting %d", pctrl->SDRbit);

	rc = of_property_read_u32_array(np, "akm,measurement_range", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->measure_range = 0;
	}
	else {
		pctrl->measure_range = buf[0];
	}
	AKM9970_LOG_I("measurement_range %d", pctrl->measure_range);

	rc = of_property_read_u32_array(np, "akm,threshold_X", buf, 4);
	if (rc && (rc != -EINVAL)) {
		pctrl->BOP1Xbits = 0;
		pctrl->BRP1Xbits = 0;
		pctrl->BOP2Xbits = 0;
		pctrl->BRP2Xbits = 0;
	}
	else {
		pctrl->BOP1Xbits = buf[0];
		pctrl->BRP1Xbits = buf[1];
		pctrl->BOP2Xbits = buf[2];
		pctrl->BRP2Xbits = buf[3];
	}
	AKM9970_LOG_I("threshold_X %d %d %d %d", pctrl->BOP1Xbits, pctrl->BRP1Xbits, pctrl->BOP2Xbits, pctrl->BRP2Xbits);

	rc = of_property_read_u32_array(np, "akm,threshold_Y", buf, 4);
	if (rc && (rc != -EINVAL)) {
		pctrl->BOP1Ybits = 0;
		pctrl->BRP1Ybits = 0;
		pctrl->BOP2Ybits = 0;
		pctrl->BRP2Ybits = 0;
	}
	else {
		pctrl->BOP1Ybits = buf[0];
		pctrl->BRP1Ybits = buf[1];
		pctrl->BOP2Ybits = buf[2];
		pctrl->BRP2Ybits = buf[3];
	}
	AKM9970_LOG_I("threshold_Y %d %d %d %d", pctrl->BOP1Ybits, pctrl->BRP1Ybits, pctrl->BOP2Ybits, pctrl->BRP2Ybits);

	rc = of_property_read_u32_array(np, "akm,threshold_Z", buf, 4);
	if (rc && (rc != -EINVAL)) {
		pctrl->BOP1Zbits = 0;
		pctrl->BRP1Zbits = 0;
		pctrl->BOP2Zbits = 0;
		pctrl->BRP2Zbits = 0;
	}
	else {
		pctrl->BOP1Zbits = buf[0];
		pctrl->BRP1Zbits = buf[1];
		pctrl->BOP2Zbits = buf[2];
		pctrl->BRP2Zbits = buf[3];
	}
	AKM9970_LOG_I("threshold_Z %d %d %d %d", pctrl->BOP1Zbits, pctrl->BRP1Zbits, pctrl->BOP2Zbits, pctrl->BRP2Zbits);

	rc = of_property_read_u32_array(np, "akm,switch_event", buf, 6);
	if (rc && (rc != -EINVAL)) {
		pctrl->SWX1ENbit = ((pctrl->BOP1Xbits != 0 ) ? 1 : 0);
		pctrl->SWX2ENbit = ((pctrl->BOP2Xbits != 0 ) ? 1 : 0);
		pctrl->SWY1ENbit = ((pctrl->BOP1Ybits != 0 ) ? 1 : 0);
		pctrl->SWY2ENbit = ((pctrl->BOP2Ybits != 0 ) ? 1 : 0);
		pctrl->SWZ1ENbit = ((pctrl->BOP1Zbits != 0 ) ? 1 : 0);
		pctrl->SWZ2ENbit = ((pctrl->BOP2Zbits != 0 ) ? 1 : 0);
	}
	else {
		pctrl->SWX1ENbit = buf[0];
		pctrl->SWX2ENbit = buf[1];
		pctrl->SWY1ENbit = buf[2];
		pctrl->SWY2ENbit = buf[3];
		pctrl->SWZ1ENbit = buf[4];
		pctrl->SWZ2ENbit = buf[5];
	}
	AKM9970_LOG_I("threshold_Z %d %d %d %d %d %d", pctrl->SWX1ENbit, pctrl->SWX2ENbit, pctrl->SWY1ENbit, pctrl->SWY2ENbit, pctrl->SWZ1ENbit, pctrl->SWZ2ENbit);

	return 0;
}

static ssize_t akm_show_reg_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm09970_soc_ctrl *pctrl = i2c_get_clientdata(client);
	int rc = 0;
	int mag_x = 0;
	int mag_y = 0;
	int mag_z = 0;
	uint8_t buffer[8] = {0};
	char buf_line[256] = "";
	char buf_regdump[512] = "";
	int nlength = 0;

	memset(buf_line, 0, sizeof(buf_line));
	memset(buf_regdump, 0, sizeof(buf_regdump));

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_ST_XYZ, 8, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}
	sprintf(buf_line, "reg[0x17]: %02x %02x %02x %02x %02x %02x %02x %02x\n",
		buffer[0], buffer[1], buffer[2], buffer[3],
		buffer[4], buffer[5], buffer[6], buffer[7]);
	nlength = strlen(buf_regdump);
	strcpy(&buf_regdump[nlength], buf_line);

	mag_x = MAKE_S16(buffer[6], buffer[7]);
	mag_y = MAKE_S16(buffer[4], buffer[5]);
	mag_z = MAKE_S16(buffer[2], buffer[3]);

	memset(buf_line, 0, sizeof(buf_line));
	sprintf(buf_line, "x_axis: %d, y_axis: %d, z_axis:%d, hall_state:%d\n", mag_x, mag_y, mag_z, pctrl->hall_detected);
	nlength = strlen(buf_regdump);
	strcpy(&buf_regdump[nlength], buf_line);

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_CNTL1, 2, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}
	memset(buf_line, 0, sizeof(buf_line));
	sprintf(buf_line, "reg[0x20]: %02x %02x\n", buffer[0], buffer[1]);
	nlength = strlen(buf_regdump);
	strcpy(&buf_regdump[nlength], buf_line);

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09970_REG_CNTL2, 1, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}

	memset(buf_line, 0, sizeof(buf_line));
	sprintf(buf_line, "reg[0x21]: %02x\n", buffer[0]);
	nlength = strlen(buf_regdump);
	strcpy(&buf_regdump[nlength], buf_line);

	return sprintf(buf,"%s\n", buf_regdump);
}
static DEVICE_ATTR(reg_dump, 0664, akm_show_reg_dump, NULL);

static ssize_t akm_show_hall_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm09970_soc_ctrl *pctrl = i2c_get_clientdata(client);

	return sprintf(buf,"%d\n", pctrl->hall_detected);
}
static DEVICE_ATTR(hall_state, 0664, akm_show_hall_state, NULL);

static struct attribute *akm09970_attributes[] = {
	&dev_attr_reg_dump.attr,
	&dev_attr_hall_state.attr,
	NULL
};

static struct attribute_group akm09970_attr_group = {
	.attrs = akm09970_attributes,
};

int akm09970_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;
	struct akm09970_soc_ctrl *pctrl = NULL;
	unsigned long irq_frag;

	dev_info(&client->dev, "%s start probe. %d", __func__, __LINE__);
	dev_err(&client->dev, "%s start probe. %d", __func__, __LINE__);
	AKM9970_LOG_I("%s start probe. %d", __func__, __LINE__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("check_functionality failed.");
		return -ENODEV;
	}

	/* Allocate memory for driver data */
	pctrl = devm_kzalloc(&client->dev, sizeof(struct akm09970_soc_ctrl), GFP_KERNEL);
	if (!pctrl) {
		pr_err("memory allocation failed.");
		rc = -ENOMEM;
		goto exit1;
	}

	/* set client data */
	pctrl->client = client;
	i2c_set_clientdata(client, pctrl);

	/* parse dt */
	pr_debug("start parse dt.");
	if (client->dev.of_node) {
		rc = akm09970_parse_dt(&client->dev, pctrl);
		if (rc < 0) {
			pr_err("Unable to parse platfrom data rc=%d\n", rc);
			goto exit2;
		}
	}

	/* gpio config */
	rc = akm09970_gpio_config(pctrl);
	if (rc < 0) {
		pr_err("Failed to config gpio\n");
		goto exit2;
	}

	rc = akm_power_up(pctrl);
	if (rc < 0)
		goto exit3;

	rc = akm_i2c_check_device(client);
	if (rc < 0) {
		dev_err(&client->dev, "%s check device failed: %d", __func__, rc);
		goto exit4;
	}

	rc = akm09970_input_register(&pctrl->input, pctrl);
	if (rc) {
		pr_err("%s input_dev register failed %d", __func__, rc);
		goto exit4;
	}

	/* create sysfs grou under virtual input*/
	rc = sysfs_create_group(&pctrl->input->dev.kobj, &akm09970_attr_group);
	if (rc) {
		pr_err("%s could not create sysfs\n");
		goto exit5;
	}

	//setup will be done while active
	//rc = ak09970_setup(pctrl);
	//if (rc) {
	//	pr_err("%s ak09970_setup failed %d", __func__, rc);
	//	goto exit4;
	//}

	pctrl->irq = client->irq;
	pr_debug("IRQ is #%d.", pctrl->irq);

	/* init report work queue */
	pctrl->work_queue = alloc_workqueue("akm_poll_work",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_WORK(&pctrl->report_work, akm_dev_input_work);

	if (pctrl->irq) {
		if ( pctrl->ODINTENbit == 0 ) {
			irq_frag = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
		}
		else {
			irq_frag = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		}
		rc = request_threaded_irq(
				client->irq,
				NULL,
				akm_irq,
				irq_frag,
				client->name,
				pctrl);

		if (rc < 0) {
			pr_err("request irq failed.");
			goto exit6;
		}
	}
	disable_irq_nosync(pctrl->client->irq);

	init_akm09970_pd(pctrl);

	pctrl->miscdev.minor = MISC_DYNAMIC_MINOR;
	pctrl->miscdev.name = AKM_MISCDEV_NAME;
	pctrl->miscdev.fops = &akm09970_fops;
	pctrl->miscdev.parent = &pctrl->client->dev;
	rc = misc_register(&pctrl->miscdev);
	if (rc) {
		pr_err("misc register failed!");
		goto exit7;
	}

	akm_power_down(pctrl);

	mutex_init(&pctrl->soc_mutex);
	init_waitqueue_head(&pctrl->drdy_wq);
	atomic_set(&pctrl->drdy, 0);

	// enable and get initial state
	rc = akm_active(pctrl, 1);
	if (rc) {
		pr_err("akm_active failed! %d", rc);
		goto exit8;
	}
	msleep(2);
	// get hall state
	rc = akm_initial_hall_state(pctrl);
	if (rc) {
		pr_err("akm_report_data failed! %d", rc);
		goto exit8;
	}
	lge_hall_sensor_state = pctrl->hall_detected;

	dev_info(&client->dev, "%s successfully probed.", __func__);
	return 0;

exit8:
	misc_deregister(&pctrl->miscdev);
exit7:
	if (pctrl->irq)
		free_irq(pctrl->irq, pctrl);
	if (gpio_is_valid(pctrl->irq))
		gpio_free(pctrl->irq);
exit6:
	sysfs_remove_group(&pctrl->input->dev.kobj, &akm09970_attr_group);
exit5:
	input_unregister_device(pctrl->input);
exit4:
	akm_power_down(pctrl);
exit3:
exit2:
	devm_kfree(&client->dev, pctrl);
exit1:
	return rc;
}

static int akm09970_remove(struct i2c_client *client)
{
	struct akm09970_soc_ctrl *pctrl = i2c_get_clientdata(client);

	cancel_work_sync(&pctrl->report_work);
	destroy_workqueue(pctrl->work_queue);

	mutex_destroy(&pctrl->soc_mutex);

	if (akm_power_down(pctrl))
		pr_err("power set failed.");

	misc_deregister(&pctrl->miscdev);

	if (pctrl->irq)
		free_irq(pctrl->irq, pctrl);

	sysfs_remove_group(&pctrl->input->dev.kobj, &akm09970_attr_group);
	input_unregister_device(pctrl->input);

	pr_debug("successfully removed.");
	return 0;
}

static const struct i2c_device_id akm09970_id[] = {
	{AKM_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak09970_id);

static struct of_device_id akm09970_match_table[] = {
	{ .compatible = "ak,ak09970", },
	{ .compatible = "akm,akm09970", },
	{ },
};

static struct i2c_driver akm09970_driver = {
	.probe		= akm09970_probe,
	.remove		= akm09970_remove,
	.id_table	= akm09970_id,
	.driver = {
		.name	= AKM_I2C_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = akm09970_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
	},
};
module_i2c_driver(akm09970_driver);


MODULE_AUTHOR("AsakiKasei Microdeivces");
MODULE_DESCRIPTION("AKM compass driver");
MODULE_LICENSE("GPL");
