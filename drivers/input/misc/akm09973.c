/* drivers/intput/misc/akm09973.c - akm09973 compass driver
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
//#define pr_fmt(fmt) "akm09973: %s: %d " fmt, __func__, __LINE__

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

#include <akm09973.h>

#define AKM9973_LOG_I(fmt, args...)\
	pr_info("[AKM9973][%s %d] "\
		fmt, __func__, __LINE__, ##args)

#define AKM9973_LOG_E(fmt, args...)\
	pr_err("[AKM9973_E][%s %d] "\
		fmt, __func__, __LINE__, ##args)

#define AKM_DRDY_TIMEOUT_MS		100
#define AKM_DEFAULT_MEASURE_HZ	100
#define AKM_I2C_NAME			"akm09973"

#define MAKE_S16(U8H, U8L) \
	(int16_t)(((uint16_t)(U8H) << 8) | (uint16_t)(U8L))

/* POWER SUPPLY VOLTAGE RANGE */
#define AKM09973_VDD_MIN_UV 1800000
#define AKM09973_VDD_MAX_UV 1800000
#define PWM_PERIOD_DEFAULT_NS 1000000

struct akm09973_soc_ctrl {
	struct i2c_client *client;
	struct input_dev *input;
	struct miscdevice miscdev;
	struct device_node *of_node;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_default;
//	struct pinctrl_state *pinctrl_sleep;

	int gpio_irq;

	int measureDataAddr;
	int measureDataSize;
	int power_enabled;

	struct work_struct report_work;
	struct workqueue_struct *work_queue;

	struct mutex soc_mutex;
	wait_queue_head_t drdy_wq;
	atomic_t drdy;

	int irq;

	uint8_t sense_info[AKM_SENSOR_INFO_SIZE];
//	uint8_t sense_conf[AKM_SENSOR_CONF_SIZE];
	uint8_t sense_data[AKM_SENSOR_DATA_SIZE];
	int32_t magnet_data[3];
	uint8_t layout;

	uint32_t measure_freq_hz;
	uint8_t measure_mode;
	
	uint8_t  test_event;
	uint16_t mag_status;
	uint16_t mag_status_ext;

	struct akm09973_control ctrlReg;

	uint8_t SDRbit;
	uint8_t SMRbit;

	uint16_t BOPXbits;
	uint16_t BRPXbits;
	uint16_t BOPYbits;
	uint16_t BRPYbits;
	uint16_t BOPZbits;
	uint16_t BRPZbits;
	uint16_t BOPVbits;
	uint16_t BRPVbits;

	uint16_t hall_detected;
};

struct _akm09973_pd_handler {
	int ref_count;
	struct mutex lock;
	struct akm09973_soc_ctrl *data;
} akm09973_pd_handler = {
	.ref_count = -1,
	.data = NULL,
};

static int akm09973_write_byte(struct i2c_client *client, u8 reg, u8 val)
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

static int akm09973_i2c_writes(struct i2c_client *client,
									const u8 *tx, size_t wlen)
{
	int ret;

	ret = i2c_master_send(client, tx, wlen);

	if(ret != wlen) {
		pr_err("%s: comm error, ret %d, wlen %d\n", __func__, ret, (int)wlen);
		pr_debug("%s return %d\n", __func__, ret);
	}

	return ret;
}

static int akm09973_write_block(struct i2c_client *client, u8 reg, u8 *val, uint8_t val_len)
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

	rc = akm09973_i2c_writes(client, tx, n);

	if (client->irq)
		enable_irq(client->irq);

	return rc;
}

static s32 akm09973_i2c_write16(struct i2c_client *client, 
				u8 address, int valueNum, u16 value1, u16 value2)
{
	u8  tx[5];
	s32 ret;
	int n;

	if ( ( valueNum != 1 ) &&  ( valueNum != 2 ) ) {
		pr_err("%s: valueNum error, valueNum= %d\n", __func__, valueNum);
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

	ret = akm09973_i2c_writes(client, tx, n);

	pr_debug("%s return %d\n", __func__, ret);

	return ret;
}

static int akm09973_set_reg_bits(struct i2c_client *client,
					int val, int shift, u8 mask, u8 reg)
{
	int data;

	data = i2c_smbus_read_byte_data(client, reg);
	if ( data < 0 ) return data;

	data = (data & ~mask) | ((val << shift) & mask);
	pr_info("AK09973 reg: 0x%x, data: 0x%x\n", reg, data);

	return akm09973_write_byte(client, reg, data); 
}

static int akm09973_set_smr(
	struct akm09973_soc_ctrl *pctrl,
	uint8_t mode)
{
	int rc;

	rc = akm09973_set_reg_bits(pctrl->client, mode, AK09973_SMR_MODE_POS,
				AK09973_SMR_MODE_MSK, AK09973_SMR_MODE_REG);

	return rc;
}

static int akm09973_set_mode(
	struct akm09973_soc_ctrl *pctrl,
	uint8_t mode)
{
	int rc;

	rc = akm09973_set_reg_bits(pctrl->client, mode, AK09973_MODE_POS,
				AK09973_MODE_MSK, AK09973_MODE_REG);

	return rc;
}

static void akm09973_reset(
	struct akm09973_soc_ctrl *pctrl)
{
	atomic_set(&pctrl->drdy, 0);
	return;
}

static int akm09973_power_down(struct akm09973_soc_ctrl *pctrl)
{
	int rc = 0;

	if (pctrl->power_enabled) {
		akm09973_set_mode(pctrl, AK09973_MODE_POWERDOWN);
		pr_debug("power down");
		atomic_set(&pctrl->drdy, 0);
	}

	return rc;
}

static int akm09973_power_up(struct akm09973_soc_ctrl *pctrl)
{
	int rc = 0;

	if (!pctrl->power_enabled) {
		pr_info("power up");
		akm09973_reset(pctrl);
		return rc;
	}

	return rc;
}

static int32_t akm09973_make_L32(uint8_t data0, uint8_t data1,
							uint8_t data2, uint8_t data3)
{
	uint32_t value;
	int32_t  data32;

	value = ((uint32_t)data0 << 24) + ((uint32_t)data1 << 16)
						+ ((uint32_t)data2 << 8) + (uint32_t)data3;
	if ( value <= 0x7FFFFFFF ) {
		data32 = (int32_t)value;
	}
	else {
		value = ~value;
		data32 = -(int32_t)value - 1;
	}

	return data32;

}

/* This function will block a process until the latest measurement
 * data is available.
 */
static int akm09973_get_data(
	struct akm09973_soc_ctrl *pctrl,
	int32_t *rbuf,
	int size)
{

	long timeout, try_count;
	int i;
	int rc = 0;
	uint8_t buffer[AKM_SENSOR_DATA_SIZE];

	if (pctrl->measure_mode == AK09973_MODE_SINGLE_MEASUREMENT){
		if ( pctrl->power_enabled == false ) {
			rc = akm09973_set_mode(pctrl, AK09973_MODE_SINGLE_MEASUREMENT);
		}
		try_count = 0;
		/* Check ST bit */		
		do
		{
			/* Read data */
			rc = i2c_smbus_read_i2c_block_data(pctrl->client,
								pctrl->measureDataAddr, pctrl->measureDataSize, buffer);
			if (rc < 0) {
				pr_err("read data failed!");
				for ( i = 0 ; i < AKM_SENSOR_DATA_SIZE ; i ++ ) {
					buffer[i] = 0;
				}
			}
			/* ERR is */
			if (AKM_ERR_IS_HIGH(buffer[0])) {
				pr_err("Sensor overflow!\n");
			}
			/* Check ST bit */
			if (!(AKM_DRDY_IS_HIGH(buffer[0]))) {
				pr_info("DRDY is low. Use last value.");
				try_count++;
			} else {
				break;
			}
		}while(try_count < 10);
		memcpy(pctrl->sense_data, buffer, pctrl->measureDataSize);
		pctrl->power_enabled = false;
	}
	else{
		/* Block! */
		timeout = wait_event_interruptible_timeout(
				pctrl->drdy_wq,
				atomic_read(&pctrl->drdy),
				msecs_to_jiffies(AKM_DRDY_TIMEOUT_MS));
		
		if (!timeout) {
			pr_err("wait_event timeout");
			for ( i = 0 ; i < AKM_SENSOR_DATA_SIZE ; i ++ ) {
				pctrl->sense_data[i] = 0;
			}
			rc = -EIO;
		}
	}

	pctrl->mag_status = (u16)pctrl->sense_data[0];
	if ( pctrl->measureDataAddr == AK09973_REG_ST_XYZ ) {
		pctrl->magnet_data[0] = MAKE_S16(pctrl->sense_data[1], pctrl->sense_data[2]);
		pctrl->magnet_data[1] = MAKE_S16(pctrl->sense_data[3], pctrl->sense_data[4]);
		pctrl->magnet_data[2] = MAKE_S16(pctrl->sense_data[5], pctrl->sense_data[6]);
	}
	else {
		pctrl->magnet_data[0] = akm09973_make_L32(pctrl->sense_data[1], pctrl->sense_data[2],
											pctrl->sense_data[2], pctrl->sense_data[4]);
		pctrl->magnet_data[1] = 0;
		pctrl->magnet_data[2] = 0;
	}


	rbuf[0] = pctrl->mag_status;
	for ( i = 1 ; i < size ; i ++ ) {
		rbuf[i] = pctrl->magnet_data[i-1];
	}
	atomic_set(&pctrl->drdy, 0);

	return rc;
}

static int akm09973_get_threshold(struct akm09973_soc_ctrl *pctrl, 
									struct akm09973_threshold *threshold)
{
	uint8_t buffer[AKM_SENSOR_THRESHOLD_SIZE];
	int rc;

	/* Read data */
	rc = i2c_smbus_read_i2c_block_data(pctrl->client,
				AK09973_REG_THRESHOLD, AKM_SENSOR_THRESHOLD_SIZE, buffer);
	threshold->bopx = ((buffer[0] << 8) & 0xFF00) | (buffer[1] & 0x00FF);
	threshold->brpx = ((buffer[2] << 8) & 0xFF00) | (buffer[3] & 0x00FF);
	threshold->bopy = ((buffer[4] << 8) & 0xFF00) | (buffer[5] & 0x00FF);
	threshold->brpy = ((buffer[6] << 8) & 0xFF00) | (buffer[7] & 0x00FF);
	threshold->bopz = ((buffer[8] << 8) & 0xFF00) | (buffer[9] & 0x00FF);
	threshold->brpz = ((buffer[10] << 8) & 0xFF00) | (buffer[11] & 0x00FF);
	threshold->bopv = ((buffer[12] << 8) & 0xFF00) | (buffer[13] & 0x00FF);
	threshold->brpv = ((buffer[14] << 8) & 0xFF00) | (buffer[15] & 0x00FF);
	return rc;
}

static int akm09973_set_threshold(struct akm09973_soc_ctrl *pctrl,
								struct akm09973_threshold *threshold)
{
	int rc = 0;
	u8 val[AKM_SENSOR_THRESHOLD_SIZE];
	int i;
	for(i = 0; i < AKM_SENSOR_THRESHOLD_SIZE; i++)
	{
		val[i] = (u8)threshold->reg[i];
	}

	rc = akm09973_write_block(pctrl->client, AK09973_REG_THRESHOLD,
			val, sizeof(val));
	return rc;
}

static int akm09973_get_control_register(struct akm09973_soc_ctrl *pctrl, 
								struct akm09973_control *controlReg)
{
	uint8_t buffer[AKM_SENSOR_CONTROL_SIZE];
	int rc;

	/* Read data */
	rc = i2c_smbus_read_i2c_block_data(pctrl->client,
				AK09973_REG_CNTL1, AKM_SENSOR_CONTROL_SIZE, buffer);

	controlReg->polx = ( 0x01 & buffer[0] ) ? 1 : 0;
	controlReg->poly = ( 0x02 & buffer[0] ) ? 1 : 0;
	controlReg->polz = ( 0x04 & buffer[0] ) ? 1 : 0;
	controlReg->polv = ( 0x08 & buffer[0] ) ? 1 : 0;

	controlReg->drdyen = ( 0x01 & buffer[1] ) ? 1 : 0;
	controlReg->swxen = ( 0x02 & buffer[1] ) ? 1 : 0;
	controlReg->swyen = ( 0x04 & buffer[1] ) ? 1 : 0;
	controlReg->swzen = ( 0x08 & buffer[1] ) ? 1 : 0;
	controlReg->swven = ( 0x10 & buffer[1] ) ? 1 : 0;
	controlReg->erren = ( 0x20 & buffer[1] ) ? 1 : 0;

	return rc;
}


static int akm09973_set_control_register(struct akm09973_soc_ctrl *pctrl)
{
	int	ret;
	u16 value1;

	value1 = (u16)(pctrl->ctrlReg.polx + (pctrl->ctrlReg.poly << 1) 
					+ (pctrl->ctrlReg.polz << 2) + (pctrl->ctrlReg.polv << 3));
	value1 <<= 8;
	value1 += ((u16)(pctrl->ctrlReg.drdyen + (pctrl->ctrlReg.swxen << 1) 
			   + (pctrl->ctrlReg.swyen << 2) + (pctrl->ctrlReg.swzen << 3) 
                 + (pctrl->ctrlReg.swven << 4) + (pctrl->ctrlReg.erren << 5))); 

	ret = akm09973_i2c_write16(pctrl->client, AK09973_REG_CNTL1, 1, value1, 0);

	return ret;

}

static uint8_t akm09973_get_measure_mode(struct akm09973_soc_ctrl *pctrl)
{
	uint8_t mode;

	if (pctrl->measure_freq_hz >= 2000)
		mode = AK09973_MODE_CONTINUOUS_2000HZ;
	else if (pctrl->measure_freq_hz >= 1000)
		mode = AK09973_MODE_CONTINUOUS_1000HZ;
	else if (pctrl->measure_freq_hz >= 500)
		mode = AK09973_MODE_CONTINUOUS_500HZ;
	else if (pctrl->measure_freq_hz >= 100)
		mode = AK09973_MODE_CONTINUOUS_100HZ;
	else if (pctrl->measure_freq_hz >= 50)
		mode = AK09973_MODE_CONTINUOUS_50HZ;
	else if (pctrl->measure_freq_hz >= 20)
		mode = AK09973_MODE_CONTINUOUS_20HZ;
	else if (pctrl->measure_freq_hz >= 10)
		mode = AK09973_MODE_CONTINUOUS_10HZ;
	else if (pctrl->measure_freq_hz >= 5)
		mode = AK09973_MODE_CONTINUOUS_5HZ;
	else
		mode = AK09973_MODE_SINGLE_MEASUREMENT;

	return mode;

}

static int akm09973_active(struct akm09973_soc_ctrl *pctrl, int activeMode)
{
	int rc = 0;
	uint8_t mode;

	pr_info("akm sensor active=%d\n", activeMode);

	if (!pctrl->power_enabled && activeMode) {
		if ( activeMode == 1 ) {
			pctrl->measureDataAddr = AK09973_REG_ST_XYZ;
			pctrl->measureDataSize = AKM_SENSOR_DATA_SIZE;
			input_set_abs_params(pctrl->input, ABS_X,
								-32767, 32767, 0, 0);
		}
		else {
			pctrl->measureDataAddr = AK09973_REG_ST_V;
			pctrl->measureDataSize = AKM_SENSOR_DATA_SIZE_V;
			input_set_abs_params(pctrl->input, ABS_X,
								-0x7FFFFFFF, 0x7FFFFFFF, 0, 0);
		}

		pctrl->power_enabled = true;
		rc = akm09973_power_up(pctrl);
		if (rc) {
			pr_err("Sensor power up fail!\n");
			pctrl->power_enabled = false;
			return rc;
		}

		mode = akm09973_get_measure_mode(pctrl);
		pctrl->measure_mode = mode;

		rc = akm09973_set_smr(pctrl, pctrl->SMRbit);
		if (rc < 0) {
			pr_err("Failed to set smr.");
			pctrl->power_enabled = false;
			return rc;
		}

		usleep_range(100,100);

		rc = akm09973_set_mode(pctrl, mode);
		if (rc < 0) {
			pr_err("Failed to set to mode(%d)", mode);
			pctrl->power_enabled = false;
			return rc;
		}

		if ( pctrl->measure_mode != AK09973_MODE_SINGLE_MEASUREMENT ) {
			pr_debug("enable irq");
			enable_irq(pctrl->client->irq);
		}
	} else if (pctrl->power_enabled && (activeMode == 0)) {
		pctrl->power_enabled = false;

		if ( pctrl->measure_mode != AK09973_MODE_SINGLE_MEASUREMENT ) {
			disable_irq_nosync(pctrl->client->irq);
			cancel_work_sync(&pctrl->report_work);
			pr_debug("disable irq");
		}

		rc = akm09973_set_mode(pctrl, AK09973_MODE_POWERDOWN);
		if (rc)
			pr_warn("Failed to set to POWERDOWN mode.\n");

		akm09973_power_down(pctrl);
	} else {
		pr_info("power state is the same, do nothing!");
	}

	return 0;
}

static long akm09973_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	uint8_t activeMode = 0;
	uint16_t mode = AK09973_MODE_POWERDOWN;
	int32_t dat_buf[4];/* for GET_DATA */
	int state = 1;
	struct akm09973_soc_ctrl *pctrl = NULL;
	struct akm09973_threshold threshold;
	struct akm09973_control   controlReg;

	pctrl = (struct akm09973_soc_ctrl*)filp->private_data;

	if (pctrl == NULL)
		return -EFAULT;

//	mutex_lock(&pctrl->soc_mutex);

	switch (cmd) {
	case AKM_IOC_SET_ACTIVE:
		pr_debug("AKM_IOC_SET_ACTIVE.");
		if (copy_from_user(&activeMode, (uint8_t *)arg, sizeof(uint8_t))) {
			pr_err("Failed to active status from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		rc = akm09973_active(pctrl, activeMode);
		break;
	case AKM_IOC_SET_MODE:
		pr_debug("AKM_IOC_SET_MODE.");
		if (copy_from_user(&mode, (uint8_t *)arg, sizeof(uint16_t))) {
			pr_err("Failed to copy mode from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		pctrl->measure_freq_hz = mode;

		break;
	case AKM_IOC_SET_PRESS:
		pr_debug("AKM_IOC_SET_PRESS.");
		input_event(pctrl->input, EV_KEY, KEY_BACK, state);
		input_sync(pctrl->input);
		input_event(pctrl->input, EV_KEY, KEY_BACK, !state);
		input_sync(pctrl->input);
		break;
	case AKM_IOC_GET_SENSSMR:
		if (copy_to_user((void __user *)arg, &pctrl->SMRbit, sizeof(uint8_t))) {
			pr_err("copy_to_user failed.");
			return -EFAULT;
		}
		break;
	case AKM_IOC_GET_SENSEDATA:
		pr_info("AKM_IOC_GET_SENSEDATA.");
		rc = akm09973_get_data(pctrl, dat_buf, 4);
		if (copy_to_user((void __user *)arg, dat_buf, sizeof(dat_buf))) {
			pr_err("copy_to_user failed.");
			rc = -EFAULT;
		}
		break;
	case AKM_IOC_SET_THRESHOLD:
		if (copy_from_user(&threshold, (uint8_t *)arg, sizeof(threshold))) {
			pr_err("Failed to threshold from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		rc = akm09973_set_threshold(pctrl, &threshold);
		break;
	case AKM_IOC_GET_THRESHOLD:
		pr_debug("AKM_IOC_GET_THRESHOLD.");
		rc = akm09973_get_threshold(pctrl, &threshold);
		if (rc < 0)
			return rc;
		if (copy_to_user((void __user *)arg, &threshold, sizeof(threshold))) {
			pr_err("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case AKM_IOC_GET_CONTROL_REG:
		rc = akm09973_get_control_register(pctrl, &controlReg);
		if (rc < 0)
			return rc;
		if (copy_to_user((void __user *)arg, &controlReg, sizeof(controlReg))) {
			pr_err("copy_to_user failed.");
			return -EFAULT;
		}
		break;

	case AKM_IOC_SET_CONTROL_REG:
		if (copy_from_user(&controlReg, (uint8_t *)arg, sizeof(controlReg))) {
			pr_err("Failed to control register from user to kernel\n");
			rc = -EFAULT;
			break;
		}
		pctrl->ctrlReg = controlReg;
		rc = akm09973_set_control_register(pctrl);
		break;

	default:
		pr_warn("unsupport cmd:0x%x\n", cmd);
		break;
	}

//	mutex_unlock(&pctrl->soc_mutex);
	return rc;
}

#ifdef CONFIG_COMPAT
static long akm09973_compat_ioctl(struct file *filp,
										unsigned int cmd, unsigned long arg)
{
	return akm09973_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static int init_akm09973_pd(struct akm09973_soc_ctrl *data)
{
	struct _akm09973_pd_handler *akm09973 = &akm09973_pd_handler;

	if (data == NULL)
		return -EFAULT;

	mutex_init(&akm09973->lock);

	mutex_lock(&akm09973->lock);
	akm09973->data = data;
	mutex_unlock(&akm09973->lock);

	return 0;
}

static struct akm09973_soc_ctrl* get_akm09973_pd(void)
{
	struct _akm09973_pd_handler *akm09973 = &akm09973_pd_handler;

	if (akm09973->data == NULL)
		return NULL;

	mutex_lock(&akm09973->lock);
	akm09973->ref_count++;
	mutex_unlock(&akm09973->lock);

	return akm09973->data;
}

static int rel_akm09973_pd(struct akm09973_soc_ctrl *data)
{
	struct _akm09973_pd_handler *akm09973 = &akm09973_pd_handler;

	if (akm09973->data == NULL)
		return -EFAULT;

	mutex_lock(&akm09973->lock);
	akm09973->ref_count--;
	mutex_unlock(&akm09973->lock);

	data = NULL;

	return 0;
}

/* AK7719 Misc driver interfaces */
static int akm09973_open(struct inode *inode, struct file *file)
{
	struct akm09973_soc_ctrl *pctrl = NULL;

	pctrl = get_akm09973_pd();
	file->private_data = pctrl;

	return 0;
}


static int akm09973_close(struct inode *inode, struct file *file)
{
	struct akm09973_soc_ctrl *akm09973 = (struct akm09973_soc_ctrl*)file->private_data;

	rel_akm09973_pd(akm09973);

	return 0;
}


static const struct file_operations akm09973_fops = {
	.owner   = THIS_MODULE,
	.open    = akm09973_open,
	.release = akm09973_close,
	.unlocked_ioctl = akm09973_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = akm09973_compat_ioctl,
#endif
};

/* akm get and report data functions */
static int akm09973_read_sense_data(
	struct akm09973_soc_ctrl *pctrl,
	uint8_t *rbuf,
	int size)
{
	uint8_t buffer[AKM_SENSOR_DATA_SIZE];
	int i;
	int rc;

	/* Read data */
	rc = i2c_smbus_read_i2c_block_data(pctrl->client,
							pctrl->measureDataAddr, pctrl->measureDataSize, buffer);
	if (rc < 0) {
		for ( i = 0 ; i < size ; i++ ){
			rbuf[i] = 0;
		}
		pr_err("read data failed!");
		return rc;
	}

	memcpy(rbuf, buffer, size);

	/* ERR is */
	if (AKM_ERR_IS_HIGH(buffer[0])) {
		pr_err("Sensor overflow!\n");
		rc = -EIO;
	}

	/* Check ST bit */
	if (!(AKM_DRDY_IS_HIGH(buffer[0]))) {
		pr_info("DRDY is low. Use last value.");
	} else {
		atomic_set(&pctrl->drdy, 1);
		wake_up(&pctrl->drdy_wq);
	}

	return 0;
}

static int akm09973_report_data(struct akm09973_soc_ctrl *pctrl)
{
	int rc = 0;
	int tmp, mag_x, mag_y, mag_z;
	short brpx_signed, bopx_signed;
	short thresh_signed;

	rc = akm09973_read_sense_data(pctrl, pctrl->sense_data, AKM_SENSOR_DATA_SIZE);
	if (rc) {
		pr_err("Get data failed.");
		return -EIO;
	}

	if ( pctrl->measureDataAddr == AK09973_REG_ST_XYZ ) {
		mag_x = MAKE_S16(pctrl->sense_data[5], pctrl->sense_data[6]);
		mag_y = MAKE_S16(pctrl->sense_data[3], pctrl->sense_data[4]);
		mag_z = MAKE_S16(pctrl->sense_data[1], pctrl->sense_data[2]);
	}
	else {
		mag_x = akm09973_make_L32(pctrl->sense_data[1], pctrl->sense_data[2],
							pctrl->sense_data[2], pctrl->sense_data[4]);
		mag_y = 0;
		mag_z = 0;
	}

	AKM9973_LOG_I("raw data: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
			pctrl->sense_data[0],
			pctrl->sense_data[1], pctrl->sense_data[2],
			pctrl->sense_data[3], pctrl->sense_data[4],
			pctrl->sense_data[5], pctrl->sense_data[6]);
	AKM9973_LOG_I("x_axis: %d, y_axis: %d, z_axis: %d\n", mag_x, mag_y, mag_z);

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

	pctrl->mag_status = pctrl->sense_data[0];

	if(pctrl->BRPXbits < 32768)
		brpx_signed = (short)pctrl->BRPXbits;
	else
		brpx_signed = (short)((int)pctrl->BRPXbits-65536);

	if(pctrl->BOPXbits < 32768)
		bopx_signed = (short)pctrl->BOPXbits;
	else
		bopx_signed = (short)((int)pctrl->BOPXbits-65536);

	if(abs(brpx_signed) - abs(bopx_signed) > 0)
	{
		thresh_signed = brpx_signed;
	}
	else
	{
		thresh_signed = bopx_signed;
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

	AKM9973_LOG_I("x_axis: %d, thresh_signed: %d, hall_detected: %d\n", mag_x, thresh_signed, pctrl->hall_detected);

	input_report_abs(pctrl->input, ABS_MISC, pctrl->hall_detected); // report hall state as input event
//	input_report_abs(pctrl->input, ABS_MISC, (pctrl->mag_status_ext | pctrl->mag_status));
	input_report_abs(pctrl->input, ABS_X, mag_x);
	if ( pctrl->measureDataAddr == AK09973_REG_ST_XYZ ) {
		input_report_abs(pctrl->input, ABS_Y, mag_y);
		input_report_abs(pctrl->input, ABS_Z, mag_z);
	}
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

//	enable_irq(pctrl->client->irq);

	return 0;
}

static int akm09973_initial_hall_state(struct akm09973_soc_ctrl *pctrl)
{
	int rc = 0;
	int tmp, mag_x, mag_y, mag_z;
	short brpx_signed, bopx_signed;
	short thresh_signed;

	rc = akm09973_read_sense_data(pctrl, pctrl->sense_data, AKM_SENSOR_DATA_SIZE);
	if (rc) {
		pr_err("Get data failed.");
		return -EIO;
	}

	if ( pctrl->measureDataAddr == AK09973_REG_ST_XYZ ) {
		mag_x = MAKE_S16(pctrl->sense_data[5], pctrl->sense_data[6]);
		mag_y = MAKE_S16(pctrl->sense_data[3], pctrl->sense_data[4]);
		mag_z = MAKE_S16(pctrl->sense_data[1], pctrl->sense_data[2]);
	}
	else {
		mag_x = akm09973_make_L32(pctrl->sense_data[1], pctrl->sense_data[2],
							pctrl->sense_data[2], pctrl->sense_data[4]);
		mag_y = 0;
		mag_z = 0;
	}

	AKM9973_LOG_I("raw data: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x",
			pctrl->sense_data[0],
			pctrl->sense_data[1], pctrl->sense_data[2],
			pctrl->sense_data[3], pctrl->sense_data[4],
			pctrl->sense_data[5], pctrl->sense_data[6]);
	AKM9973_LOG_I("x_axis: %d, y_axis: %d, z_axis: %d\n", mag_x, mag_y, mag_z);

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

	pctrl->mag_status = pctrl->sense_data[0];

	if(pctrl->BRPXbits < 32768)
		brpx_signed = (short)pctrl->BRPXbits;
	else
		brpx_signed = (short)((int)pctrl->BRPXbits-65536);

	if(pctrl->BOPXbits < 32768)
		bopx_signed = (short)pctrl->BOPXbits;
	else
		bopx_signed = (short)((int)pctrl->BOPXbits-65536);

	if(abs(brpx_signed) - abs(bopx_signed) > 0)
	{
		thresh_signed = brpx_signed;
	}
	else
	{
		thresh_signed = bopx_signed;
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

	AKM9973_LOG_I("x_axis: %d, thresh_signed: %d, hall_detected: %d\n", mag_x, thresh_signed, pctrl->hall_detected);

	input_report_abs(pctrl->input, ABS_MISC, pctrl->hall_detected); // report hall state as input event
//	input_report_abs(pctrl->input, ABS_MISC, (pctrl->mag_status_ext | pctrl->mag_status));
	input_report_abs(pctrl->input, ABS_X, mag_x);
	if ( pctrl->measureDataAddr == AK09973_REG_ST_XYZ ) {
		input_report_abs(pctrl->input, ABS_Y, mag_y);
		input_report_abs(pctrl->input, ABS_Z, mag_z);
	}
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

//	enable_irq(pctrl->client->irq);

	return 0;
}

static void akm09973_dev_input_work(struct work_struct *work)
{
	int rc = 0;
	struct akm09973_soc_ctrl *pctrl;

	pctrl = container_of(work, struct akm09973_soc_ctrl, report_work);

	rc = akm09973_report_data(pctrl);
	if (rc < 0)
		pr_warn("Failed to report data.");
}

static irqreturn_t akm09973_irq(int irq, void *handle)
{
	struct akm09973_soc_ctrl *pctrl = handle;

//	disable_irq_nosync(pctrl->client->irq);
	queue_work(pctrl->work_queue, &pctrl->report_work);

	pr_debug("enter");
	return IRQ_HANDLED;
}

int akm09973_input_open(struct input_dev *dev)
{
	pr_info("enter!!!!!");
	return 0;
}

void akm09973_input_close(struct input_dev *dev)
{
	pr_info("enter!!!!!");
	return;
}

static int akm09973_input_register(
	struct input_dev **input,
	struct akm09973_soc_ctrl *pctrl)
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
	(*input)->open = akm09973_input_open;
	(*input)->close = akm09973_input_close;

	input_set_drvdata(pctrl->input, pctrl);

	/* Register */
	rc = input_register_device(*input);
	if (rc) {
		input_free_device(*input);
		return rc;
	}

	return rc;
}

static int akm09973_i2c_check_device(
	struct i2c_client *client)
{
	int rc = 0;
	struct akm09973_soc_ctrl *pctrl = i2c_get_clientdata(client);

	rc = i2c_smbus_read_i2c_block_data(client,
					AK09973_REG_WIA, AKM_SENSOR_INFO_SIZE, pctrl->sense_info);
	if (rc < 0) {
		dev_err(&client->dev, "%s(), I2C access failed: %d", __func__, rc);
		return rc;
	}

	/* Check read data */
	if ((pctrl->sense_info[0] != AK09973_WIA1_VALUE) ||
			(pctrl->sense_info[1] != AK09973_WIA2_VALUE)) {
		pr_err("The device is not AKM Compass(AK09973).");
		return -ENXIO;
	}

	return rc;
}
 
static int akm09973_gpio_config(struct akm09973_soc_ctrl *pctrl)
{
	int32_t rc = 0;

	rc = gpio_request_one(pctrl->gpio_irq, GPIOF_IN, "akm09973-irq");
	if (rc < 0) {
		pr_err("Failed to request power enable GPIO %d", pctrl->gpio_irq);
		goto fail0;
	}
	gpio_direction_input(pctrl->gpio_irq);

	return 0;

fail0:
	return rc;
}

static int akm09973_setup(struct akm09973_soc_ctrl *pctrl)
{
	s32 err;
	u8   value;

	akm09973_set_control_register(pctrl);

	pctrl->measure_mode = akm09973_get_measure_mode(pctrl);

	value = pctrl->measure_mode + (pctrl->SDRbit << 5) + (pctrl->SMRbit << 6);

	err = akm09973_write_byte(pctrl->client, AK09973_REG_CNTL2, value);

	err = akm09973_i2c_write16(pctrl->client, AK09973_REG_THX, 2,
								pctrl->BOPXbits, pctrl->BRPXbits);
	err = akm09973_i2c_write16(pctrl->client, AK09973_REG_THY, 2,
								pctrl->BOPYbits, pctrl->BRPYbits);
	err = akm09973_i2c_write16(pctrl->client, AK09973_REG_THZ, 2,
								pctrl->BOPZbits, pctrl->BRPZbits);
	err = akm09973_i2c_write16(pctrl->client, AK09973_REG_THV, 2,
								pctrl->BOPVbits, pctrl->BRPVbits);

	pctrl->test_event = 0;
	pctrl->mag_status = 0;
	pctrl->mag_status_ext = 0;
	pctrl->layout = 0;

	pctrl->layout = 0;
	pctrl->measureDataAddr = AK09973_REG_ST_XYZ;
	pctrl->measureDataSize = AKM_SENSOR_DATA_SIZE;

	return 0;
}

int akm09973_parse_dt(struct device *dev,
				struct akm09973_soc_ctrl *pctrl)
{
	int rc = 0;
	u32 buf[8];
	struct device_node *np;

	np = dev->of_node;

	pctrl->gpio_irq = of_get_named_gpio_flags(np, "akm,gpio_irq", 0, NULL);
	if (!gpio_is_valid(pctrl->gpio_irq)) {
		pr_err("gpio irq pin %d is invalid.",
			pctrl->gpio_irq);
		return -EINVAL;
	}
	else {
		AKM9973_LOG_I("irq pin %d", pctrl->gpio_irq);
	}

	pctrl->client->irq = gpio_to_irq(pctrl->gpio_irq);

	rc = of_property_read_u32(dev->of_node,
					"akm,measure-freq-hz", &pctrl->measure_freq_hz);
	if (rc < 0) {
		pr_info("akm,measure-freq-hz not set, use default.");
		pctrl->measure_freq_hz = AKM_DEFAULT_MEASURE_HZ;
	}
	else {
		AKM9973_LOG_I("measure-freq-hz %d", pctrl->measure_freq_hz);
	}

	rc= of_property_read_u32_array(np, "akm,DRDY_event", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->ctrlReg.drdyen = 1;
	}
	else {
		pctrl->ctrlReg.drdyen = buf[0];
	}
	AKM9973_LOG_I("DRDY_event %d", pctrl->ctrlReg.drdyen);

	rc = of_property_read_u32_array(np, "akm,ERR_event", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->ctrlReg.erren = 1;
	}
	else {
		pctrl->ctrlReg.erren = buf[0];
	}
	AKM9973_LOG_I("ERR_event %d", pctrl->ctrlReg.erren);

	rc = of_property_read_u32_array(np, "akm,POL_setting", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->ctrlReg.polx = 0;
		pctrl->ctrlReg.poly = 0;
		pctrl->ctrlReg.polz = 0;
		pctrl->ctrlReg.polv = 0;
	}
	else {
		pctrl->ctrlReg.polx = buf[0];
		pctrl->ctrlReg.poly = buf[0];
		pctrl->ctrlReg.polz = buf[0];
		pctrl->ctrlReg.polv = buf[0];
	}
	AKM9973_LOG_I("POL_setting %d %d %d %d", pctrl->ctrlReg.polx, pctrl->ctrlReg.poly, pctrl->ctrlReg.polz, pctrl->ctrlReg.polv);

	rc = of_property_read_u32_array(np, "akm,drive_setting", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->SDRbit = 0;
	}
	else {
		pctrl->SDRbit = buf[0];
	}
	AKM9973_LOG_I("drive_setting %d", pctrl->SDRbit);

	rc = of_property_read_u32_array(np, "akm,measurement_range", buf, 1);
	if (rc && (rc != -EINVAL)) {
		pctrl->SMRbit = 0;
	}
	else {
		pctrl->SMRbit = buf[0];
	}
	AKM9973_LOG_I("measurement_range %d", pctrl->SMRbit);

	rc = of_property_read_u32_array(np, "akm,threshold_X", buf, 2);
	if (rc && (rc != -EINVAL)) {
		pctrl->BOPXbits = 0;
		pctrl->BRPXbits = 0;
	}
	else {
		pctrl->BOPXbits = buf[0];
		pctrl->BRPXbits = buf[1];
	}
	AKM9973_LOG_I("threshold_X 0x%x 0x%x", pctrl->BOPXbits, pctrl->BRPXbits);

	rc = of_property_read_u32_array(np, "akm,threshold_Y", buf, 2);
	if (rc && (rc != -EINVAL)) {
		pctrl->BOPYbits = 0;
		pctrl->BRPYbits = 0;
	}
	else {
		pctrl->BOPYbits = buf[0];
		pctrl->BRPYbits = buf[1];
	}
	AKM9973_LOG_I("threshold_Y 0x%x 0x%x", pctrl->BOPYbits, pctrl->BRPYbits);

	rc = of_property_read_u32_array(np, "akm,threshold_Z", buf, 2);
	if (rc && (rc != -EINVAL)) {
		pctrl->BOPZbits = 0;
		pctrl->BRPZbits = 0;
	}
	else {
		pctrl->BOPZbits = buf[0];
		pctrl->BRPZbits = buf[1];
	}
	AKM9973_LOG_I("threshold_Z 0x%x 0x%x", pctrl->BOPZbits, pctrl->BRPZbits);

	rc = of_property_read_u32_array(np, "akm,threshold_V", buf, 2);
	if (rc && (rc != -EINVAL)) {
		pctrl->BOPVbits = 0;
		pctrl->BRPVbits = 0;
	}
	else {
		pctrl->BOPVbits = buf[0];
		pctrl->BRPVbits = buf[1];
	}
	AKM9973_LOG_I("threshold_V 0x%x 0x%x", pctrl->BOPVbits, pctrl->BRPVbits);

	rc = of_property_read_u32_array(np, "akm,switch_event", buf, 4);
	if (rc && (rc != -EINVAL)) {
		pctrl->ctrlReg.swxen = ((pctrl->BOPXbits != 0 ) ? 1 : 0);
		pctrl->ctrlReg.swyen = ((pctrl->BOPYbits != 0 ) ? 1 : 0);
		pctrl->ctrlReg.swzen = ((pctrl->BOPZbits != 0 ) ? 1 : 0);
		pctrl->ctrlReg.swven = ((pctrl->BOPVbits != 0 ) ? 1 : 0);
	}
	else {
		pctrl->ctrlReg.swxen = buf[0];
		pctrl->ctrlReg.swyen = buf[1];
		pctrl->ctrlReg.swzen = buf[2];
		pctrl->ctrlReg.swven = buf[3];
	}
	AKM9973_LOG_I("switch_event %d %d %d %d", pctrl->ctrlReg.swxen, pctrl->ctrlReg.swyen, pctrl->ctrlReg.swzen, pctrl->ctrlReg.swven);

	return 0;
}

static ssize_t akm9973_show_reg_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm09973_soc_ctrl *pctrl = i2c_get_clientdata(client);
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

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09973_REG_ST_XYZ, AKM_SENSOR_DATA_SIZE, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}
	sprintf(buf_line, "reg[0x17]: %02x %02x %02x %02x %02x %02x %02x\n",
		buffer[0], buffer[1], buffer[2], buffer[3],
		buffer[4], buffer[5], buffer[6]);
	nlength = strlen(buf_regdump);
	strcpy(&buf_regdump[nlength], buf_line);

	mag_x = MAKE_S16(buffer[5], buffer[6]);
	mag_y = MAKE_S16(buffer[3], buffer[4]);
	mag_z = MAKE_S16(buffer[1], buffer[2]);

	memset(buf_line, 0, sizeof(buf_line));
	sprintf(buf_line, "x_axis: %d, y_axis: %d, z_axis:%d, hall_state:%d\n", mag_x, mag_y, mag_z, pctrl->hall_detected);
	nlength = strlen(buf_regdump);
	strcpy(&buf_regdump[nlength], buf_line);

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09973_REG_CNTL1, 2, buffer);
	if (rc < 0) {
		pr_err("read data failed!");
		return 0;
	}
	memset(buf_line, 0, sizeof(buf_line));
	sprintf(buf_line, "reg[0x20]: %02x %02x\n", buffer[0], buffer[1]);
	nlength = strlen(buf_regdump);
	strcpy(&buf_regdump[nlength], buf_line);

	rc = i2c_smbus_read_i2c_block_data(pctrl->client, AK09973_REG_CNTL2, 1, buffer);
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
static DEVICE_ATTR(reg_dump, 0664, akm9973_show_reg_dump, NULL);

static ssize_t akm9973_show_hall_state(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct akm09973_soc_ctrl *pctrl = i2c_get_clientdata(client);

	return sprintf(buf,"%d\n", pctrl->hall_detected);
}
static DEVICE_ATTR(hall_state, 0664, akm9973_show_hall_state, NULL);

static struct attribute *akm09973_attributes[] = {
	&dev_attr_reg_dump.attr,
	&dev_attr_hall_state.attr,
	NULL
};

static struct attribute_group akm09973_attr_group = {
	.attrs = akm09973_attributes,
};

int akm09973_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int rc = 0;
	struct akm09973_soc_ctrl *pctrl = NULL;
	unsigned long irq_frag;

	AKM9973_LOG_I("start probe");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		AKM9973_LOG_E("check_functionality failed.");
		return -ENODEV;
	}

	/* Allocate memory for driver data */
	pctrl = devm_kzalloc(&client->dev, 
				sizeof(struct akm09973_soc_ctrl), GFP_KERNEL);
	if (!pctrl) {
		AKM9973_LOG_E("memory allocation failed.");
		rc = -ENOMEM;
		goto exit1;
	}

	/* set client data */
	pctrl->client = client;
	i2c_set_clientdata(client, pctrl);

	/* parse dt */
	AKM9973_LOG_I("start parse dt.");
	if (client->dev.of_node) {
		rc = akm09973_parse_dt(&client->dev, pctrl);
		if (rc < 0) {
			AKM9973_LOG_E("Unable to parse platform data rc=%d\n", rc);
			goto exit2;
		}
	}

	/* gpio config */
	rc = akm09973_gpio_config(pctrl);
	if (rc < 0) {
		AKM9973_LOG_E("Failed to config gpio\n");
		goto exit2;
	}

	rc = akm09973_power_up(pctrl);
	if (rc < 0)
		goto exit3;

	rc = akm09973_i2c_check_device(client);
	if (rc < 0) {
		AKM9973_LOG_E("check device failed: %d", rc);
		goto exit4;
	}

	rc = akm09973_input_register(&pctrl->input, pctrl);
	if (rc) {
		AKM9973_LOG_E("input_dev register failed %d", rc);
		goto exit4;
	}

	/* create sysfs grou under virtual input*/
	rc = sysfs_create_group(&pctrl->input->dev.kobj, &akm09973_attr_group);
	if (rc) {
		AKM9973_LOG_E("could not create sysfs\n");
		goto exit5;
	}

	rc = akm09973_setup(pctrl);
	if (rc) {
		AKM9973_LOG_E("akm09973_setup failed %d", rc);
		goto exit6;
	}

	pctrl->irq = client->irq;
	AKM9973_LOG_I("IRQ is #%d.", pctrl->irq);

	/* init report work queue */
	pctrl->work_queue = alloc_workqueue("akm09973_poll_work",
		WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_WORK(&pctrl->report_work, akm09973_dev_input_work);

	if (pctrl->irq) {
		if(pctrl->ctrlReg.drdyen == 0) {
			irq_frag = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_ONESHOT;
		}
		else{
			irq_frag = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
		}
		rc = request_threaded_irq(
				client->irq,
				NULL,
				akm09973_irq,
				irq_frag,
				client->name,
				pctrl);

		if (rc < 0) {
			AKM9973_LOG_E("request irq failed.");
			goto exit6;
		}
	}
	disable_irq_nosync(pctrl->client->irq);

	init_akm09973_pd(pctrl);

	pctrl->miscdev.minor = MISC_DYNAMIC_MINOR;
	pctrl->miscdev.name = AKM_MISCDEV_NAME;
	pctrl->miscdev.fops = &akm09973_fops;
	pctrl->miscdev.parent = &pctrl->client->dev;
	rc = misc_register(&pctrl->miscdev);
	if (rc) {
		AKM9973_LOG_E("misc register failed!");
		goto exit7;
	}

	akm09973_power_down(pctrl);
	
	rc = akm09973_active(pctrl, 1); // default enable
	if (rc) {
		AKM9973_LOG_E("akm09973_active failed! %d", rc);
		goto exit8;
	}

	// get hall state
	rc = akm09973_initial_hall_state(pctrl);
	if (rc) {
		pr_err("akm_report_data failed! %d", rc);
		goto exit8;
	}
	lge_hall_sensor_state = pctrl->hall_detected;

	mutex_init(&pctrl->soc_mutex);
	init_waitqueue_head(&pctrl->drdy_wq);
	atomic_set(&pctrl->drdy, 0);

	AKM9973_LOG_I("successfully probed");
	return 0;

exit8:
	misc_deregister(&pctrl->miscdev);
exit7:
	if (pctrl->irq)
		free_irq(pctrl->irq, pctrl);
	if (gpio_is_valid(pctrl->irq))
		gpio_free(pctrl->irq);
exit6:
	sysfs_remove_group(&pctrl->input->dev.kobj, &akm09973_attr_group);
exit5:
	input_unregister_device(pctrl->input);
exit4:
	akm09973_power_down(pctrl);
exit3:
exit2:
	devm_kfree(&client->dev, pctrl);
exit1:
	return rc;
}

static int akm09973_remove(struct i2c_client *client)
{
	struct akm09973_soc_ctrl *pctrl = i2c_get_clientdata(client);

	cancel_work_sync(&pctrl->report_work);
	destroy_workqueue(pctrl->work_queue);

	mutex_destroy(&pctrl->soc_mutex);

	if (akm09973_power_down(pctrl))
		pr_err("power set failed.");

	misc_deregister(&pctrl->miscdev);

	if (pctrl->irq)
		free_irq(pctrl->irq, pctrl);

	sysfs_remove_group(&pctrl->input->dev.kobj, &akm09973_attr_group);
	input_unregister_device(pctrl->input);

	pr_debug("successfully removed.");
	return 0;
}

static const struct i2c_device_id akm09973_id[] = {
	{AKM_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ak09973_id);

static struct of_device_id akm09973_match_table[] = {
	{ .compatible = "ak,ak09973", },
	{ .compatible = "akm,akm09973", },
	{ },
};

static struct i2c_driver akm09973_driver = {
	.probe		= akm09973_probe,
	.remove		= akm09973_remove,
	.id_table	= akm09973_id,
	.driver = {
		.name	= AKM_I2C_NAME,
		.owner  = THIS_MODULE,
		.of_match_table = akm09973_match_table,
	},
};
module_i2c_driver(akm09973_driver);

MODULE_AUTHOR("AsakiKasei Microdeivces");
MODULE_DESCRIPTION("AKM compass driver");
MODULE_LICENSE("GPL");
