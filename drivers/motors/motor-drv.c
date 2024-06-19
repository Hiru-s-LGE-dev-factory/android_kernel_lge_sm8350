/*
 * Base Driver for ice40 Accessory Communication Interface
 *
 * Copyright (c) 2020 LG Electronics, Inc
 *
 * All rights are reserved.
 *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING
 * THE SOFTWARE.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	< Step Motor Driver Test Code >
 *  Motor Type            : Stepper Motor
 *  Motor PWM Generator   : Lattice(FPGA)
 *  Motor Driver IC Maker : ST Micro
 *  Motor Driver IC Part  : STspin220
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/completion.h>
#include <linux/i2c.h>

#include "motor-types.h"
#include "motor-drv.h"

/*
 * < Frequency Time File Read/Write/Setting >
 * Frequency Time File : data/vendor/motor/motor_fh_time_num.txt
 */
static void set_motor_fh_time(int num)
{
	switch(num) {
		case MOTOR_F_TIME_0 :
			current_register[ICE40_R_FH_TIME_CNT_15_8] = default_register[ICE40_R_FH_TIME_CNT_15_8];
			current_register[ICE40_R_FH_TIME_CNT_7_0]  = default_register[ICE40_R_FH_TIME_CNT_7_0];
			break;
		case MOTOR_F_TIME_1 :
		case MOTOR_F_TIME_2 :
		case MOTOR_F_TIME_3 :
		case MOTOR_F_TIME_4 :
		case MOTOR_F_TIME_5 :
		case MOTOR_F_TIME_6 :
		case MOTOR_F_TIME_7 :
		case MOTOR_F_TIME_8 :
		default:
			current_register[ICE40_R_FH_TIME_CNT_15_8] = low_frequency_time_cnt[num-1][0].def;
			current_register[ICE40_R_FH_TIME_CNT_7_0]  = low_frequency_time_cnt[num-1][1].def;
			break;
	}

	mdrv->motor_max_pps = LATTTICE_MCU_CLK / (int)(current_register[ICE40_R_FH_TIME_CNT_15_8]<<8 | current_register[ICE40_R_FH_TIME_CNT_7_0]);

	pr_info("%s : fh time = 0x%x%x(%d PPS)", __func__, current_register[ICE40_R_FH_TIME_CNT_15_8], current_register[ICE40_R_FH_TIME_CNT_7_0], mdrv->motor_max_pps);
}

static void set_motor_fl_time(int num)
{
	switch(num) {
		case MOTOR_F_TIME_0 :
			current_register[ICE40_R_FL_TIME_CNT_15_8] = default_register[ICE40_R_FL_TIME_CNT_15_8];
			current_register[ICE40_R_FL_TIME_CNT_7_0]  = default_register[ICE40_R_FL_TIME_CNT_7_0];
			break;
		case MOTOR_F_TIME_1 :
		case MOTOR_F_TIME_2 :
		case MOTOR_F_TIME_3 :
		case MOTOR_F_TIME_4 :
		case MOTOR_F_TIME_5 :
		case MOTOR_F_TIME_6 :
			current_register[ICE40_R_FL_TIME_CNT_15_8] = low_frequency_time_cnt[MOTOR_F_MIN_D-1][0].def;
			current_register[ICE40_R_FL_TIME_CNT_7_0]  = low_frequency_time_cnt[MOTOR_F_MIN_D-1][1].def;
			break;
		case MOTOR_F_TIME_7 :
		case MOTOR_F_TIME_8 :
		default:
			current_register[ICE40_R_FL_TIME_CNT_15_8] = low_frequency_time_cnt[num-1][0].def;
			current_register[ICE40_R_FL_TIME_CNT_7_0]  = low_frequency_time_cnt[num-1][1].def;
			break;
	}

	mdrv->motor_min_pps = LATTTICE_MCU_CLK / (int)(current_register[ICE40_R_FL_TIME_CNT_15_8]<<8 | current_register[ICE40_R_FL_TIME_CNT_7_0]);

	pr_info("%s : fl time = 0x%x%x(%d PPS)", __func__, current_register[ICE40_R_FL_TIME_CNT_15_8], current_register[ICE40_R_FL_TIME_CNT_7_0], mdrv->motor_min_pps);
}

static void motor_fh_time_write()
{
	int fd = 0;
	unsigned char buf[2] = {0, };
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	fd = ksys_open (FH_SAVE_FILE_PATH, O_WRONLY|O_TRUNC|O_CREAT, 0666);
	ksys_chmod(FH_SAVE_FILE_PATH, 0666);

	if (fd >= 0){
		sprintf(buf,"%d", mdrv->motor_fh_pps_index);
		ksys_write (fd, buf, strlen(buf));
		pr_debug("%s : File open Success.", __func__);
		ksys_close(fd);
	} else {
		pr_debug("%s : File open Fail. %s\n", __func__, FH_SAVE_FILE_PATH);
	}

	set_fs (old_fs);
	pr_debug("%s : Leave. %d\n", __func__, fd);
}

static bool check_operation_type()
{
	int type = lge_get_laop_operator();
	bool val = false;

	switch (type) {
		case OP_ATT_US :
		case OP_VZW_POSTPAID :
		case OP_VZW_PREPAID :
			val = true;
			break;
        	default :
			break;
	}

	return val;
}

static void motor_fh_time_read()
{
	int fd = 0;
	unsigned char buf[2] = {0, };

	mm_segment_t old_fs = get_fs();

	/* Check Motor FH Time Init */
	set_fs(KERNEL_DS);
	fd = ksys_open(FH_SAVE_FILE_PATH, O_RDONLY, 0);
	if (fd >= 0) {
		pr_info("%s : File open Success. Set Tuning Register\n", __func__);

		ksys_read(fd, buf, sizeof(buf));
		sscanf(buf, "%d", &mdrv->motor_fh_pps_index);
		pr_info("%s : fh data = %d", __func__, mdrv->motor_fh_pps_index);

		ksys_close(fd);
	} else { 
		pr_info("%s : File isn't Exist. Set Default\n", __func__);

		if (mdrv->motor_fh_pps_index == 0 && lge_get_board_rev_no() == HW_REV_E && check_operation_type() == true && lge_get_boot_cable() != LT_CABLE_56K_DCP) {
			pr_info("%s : HW Rev = %d(RevE = %d), Exception Operator(AT&T, VZW) = %d\n", __func__, lge_get_board_rev_no(), HW_REV_E, lge_get_laop_operator());
			mdrv->motor_fh_pps_index = 6;

			motor_fh_time_write();
		} else {
			pr_info("%s : HW Rev = %d(RevE = %d), Exception Operator(ETC) = %d\n", __func__, lge_get_board_rev_no(), HW_REV_E, lge_get_laop_operator());
		}
	}
	set_motor_fh_time(mdrv->motor_fh_pps_index);
	set_motor_fl_time(mdrv->motor_fh_pps_index);

	set_fs(old_fs);
}

static int check_lattice_driver(bool disable)
{
	int ret = 0;
	unsigned int reg = 0;

	/* Check CDONE Pin */
	if (ice40_get_cdone() == false) {
		pr_err("%s: lattice cdone pin is low. ice40 reset\n", __func__);
		ret = ice40_reset();
		if (ret) {
			return -MOTOR_EIO;
		}
	} else {
		pr_info("%s: lattice cdone pin is high.\n", __func__);
	}

	/* Check Lattice RESET Pin */
	if (!ice40_get_lreset()) {
		pr_info("%s : Lattice Reset Enable\n", __func__);

		ice40_set_lreset(MOTOR_LRESET_ENABLE);
	} else {
		pr_info("%s : Lattice Reset Already Enable\n", __func__);
	}

	if (ice40_get_lreset()) {
		ret = regmap_read(global_ice40->regmap, ICE40_R_CTRL, &reg);
		if (ret < 0) {
			pr_err("%s : regmap read error %d\n", __func__, ret);
			ret = -MOTOR_ENODEV;
		}

		if (disable == true) {
			pr_info("%s: Lattice Reset Pin Low.\n", __func__);
			ice40_set_lreset(MOTOR_LRESET_DISABLE);
		} else {
			pr_info("%s: Keep Lattice Reset Pin High.\n", __func__);
		}

	} else {
		pr_err("%s : Did not set lreset %d\n", __func__, ret);
		return -MOTOR_ENODEV;
	}

	return ret;
}

/*
 * < Checking Motor Moving Pulse >
 * From Firmware, PWM Pulse Count
 */
static int get_motor_moving_pulse()
{
	int ret = 0;
	unsigned char moving_pulse[2] = {0, };

	ret = regmap_raw_read(global_ice40->regmap, ICE40_R_CURRENT_PPS_15_8, moving_pulse, 2);
	if (ret < 0) {
		pr_err("%s : regmap read error %d\n", __func__, ret);

		return -MOTOR_EIO;
	}

	return (int)(moving_pulse[0]<<8 | moving_pulse[1]);
}

/*
 * < Tuning File Read and Default Register Setting >
 * Tuning File : data/vendor/motor/motor_self_test.txt
 * Default Register : Rev.A - Header File, ==> Device Tree
 *                    Rev.C - Device Tree,
 *                    ETC   - Header File,
 */
static void set_motor_init_register()
{
	int fd = 0;
	int i = 0;
	unsigned char buf_str[2] = {0, };

	mm_segment_t old_fs = get_fs();

	/* Check Motor Init Register */
	set_fs(KERNEL_DS);
	fd = ksys_open(TUNING_FILE_PATH, O_RDONLY, 0);
	if (fd >= 0) {
		pr_info("%s : File open Success. Set Tuning Register\n", __func__);

		for (i=0 ; i < MOTOR_REG_DATA_MAX ; i++){
			ksys_read(fd, buf_str, sizeof(buf_str)*2);
			sscanf(buf_str, "%02x", &default_register[i]);
			//pr_info("%s : data %d = 0x%x", __func__, i, default_register[i]);
		}
		ksys_close(fd);
	} else if (mdrv->motor_default_cal_dt == true) {
		pr_info("%s : File isn't Exist. Set Default DT Register\n", __func__);
	} else { // DT Cal Error
		pr_info("%s : File isn't Exist. Set Default Register\n", __func__);

		for (i = 0 ; i < MOTOR_REG_DATA_MAX ; i++) {
			default_register[i] = ice40_motor_regmap_defaults[i].def;
			//pr_info("%s : data %d = 0x%x", __func__, i, default_register[i]);
		}

		for (i = 0 ; i < ACC_DEC_RATE_AND_CNT_ARRAY_SIZE ; i++) {
			close_r_acc_dec_rate_and_count[i] = ice40_motor_regmap_defaults[i+ICE40_R_ACC_RATE0].def;
		}

		for (i = 0 ; i < ACC_DEC_RATE_AND_CNT_ARRAY_SIZE ; i++) {
			exception_r_acc_dec_rate_and_count[i] = ice40_motor_regmap_defaults[i+ICE40_R_ACC_RATE0].def;
		}
	}
	set_fs(old_fs);

	/* Set Total Pulse */
	mdrv->motor_total_pulse = (int)(default_register[ICE40_R_TOTAL_PPS_15_8]<<8 | default_register[ICE40_R_TOTAL_PPS_7_0]);

	/* Copy Current Register */
	memcpy(current_register, default_register, MOTOR_REG_DATA_MAX);
}

/*
 * < Init Setting >
 * Read Init Registers and Default Value Setting
 */
static void set_motor_initialize()
{
	pr_info("%s : Motor initialize\n", __func__);

	/* Check Motor Register Init */
	set_motor_init_register();

	/* Set FH Time */
	motor_fh_time_read();
}

static int set_motor_control_value(int position)
{
	int ret = 0;

	pr_info("%s : Enter. Motor position from (%d) to (%d)\n", __func__, mdrv->motor_kposition, position);

	if (position == MOTOR_POSITION_DISABLE)
		return MOTOR_STATE_FORCE_DISABLE;

	pr_info("%s : Ratio = %d\n", __func__, mdrv->motor_pulse_ratio);
	if (mdrv->motor_pulse_ratio == 0) {
		return MOTOR_STATE_FORCE_DISABLE;
	} else if (mdrv->motor_pulse_ratio > 0) {
		ret = MOTOR_STATE_FORWARD_ENABLE;
	} else {
		ret = MOTOR_STATE_BACKWARD_ENABLE;
		mdrv->motor_pulse_ratio = -mdrv->motor_pulse_ratio;
	}

	mdrv->motor_kposition = position;

	return ret;
}

/*
 * < Motor Control >
 */
int set_motor_control(int position)
{
	int ret = 0;
	int control_value = 0;
	int pwm_pulse = 0;
	int updated_pulse = 0;
	int compensated_pulse = 0;

	pr_info("%s : Enter. Motor %s\n", __func__, (position==MOTOR_POSITION_DISABLE) ? "disable" : "enable");

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	/* Check Lattice Driver */
	if (check_lattice_driver(false) < 0) {
		pr_err("%s: Motor Lattice Driver Check Fail\n", __func__);

		goto motor_fail;
	}

	/* Init Register */
	if (current_register[0] == 0) {
		set_motor_initialize();
	}

	/* Check Position */
	control_value = set_motor_control_value(position);
	if (control_value == MOTOR_STATE_FORCE_DISABLE) {
		control_value = -control_value;

		goto motor_contorl;
	}

#if defined(MOTOR_WAKE_LOCK)
	enable_irq_wake(mdrv->wakeup_irq);
#endif

	/*
	 *  0. Fast-Stop Mode
	 *  : During Enabled PWM, Re-Control Motor Action
	 */
	if (mdrv->motor_status == MOTOR_PWM_ENABLE) {
		pr_info("%s : During Motor Operation, Re-Control Motor\n", __func__);

		/* Disable control_value */
		control_value &= ~MOTOR_ENABLE_MASK;

		/* Set Pause & Direction */
		ret = regmap_write(global_ice40->regmap, ICE40_R_CTRL, (u8)control_value);
		if (ret) {
			goto motor_fail;
		}

		/* Re-Enable control_value */
		control_value |= MOTOR_ENABLE_MASK;
	}

	/*
	 *  1. Accelation/Decelation Count
	 *  : Added a accelation and deceleration graph to prevent pinching hands
	 *  : Set Rate and Count
	 *  0) Exception Case - (Short pulses < EXCEPTION_CASE_PULSE) Blocked by something.
	 *  1) Open      Case
	 *  2) Close     Case
	 */
	if (mdrv->motor_operation_type != MOTOR_NORMAL) {
		pr_info("%s : Motor Exception Case HIDL\n", __func__);

		compensated_pulse = mdrv->motor_comp_evalue;
	}
#if 0
	else if ((mdrv->motor_pulse_ratio < EXCEPTION_CASE_PULSE_RATIO) && (control_value == MOTOR_STATE_BACKWARD_ENABLE)) {
		pr_info("%s : Motor Exception Case\n", __func__);

		control_value |= MOTOR_INSTANT_MOVE_MASK;
		compensated_pulse = mdrv->motor_comp_evalue;
	}
#endif
	else if (control_value & MOTOR_DIRECTION_MASK) {
		pr_info("%s : Motor Open Velocity Variation Interval\n", __func__);

		compensated_pulse = COMPENSATED_OPEN_VALUE;
		memcpy(current_register+ICE40_R_ACC_RATE0, default_register+ICE40_R_ACC_RATE0, ACC_DEC_RATE_AND_CNT_ARRAY_SIZE);
	} else {
		pr_info("%s : Motor Close Velocity Variation Interval\n", __func__);

		compensated_pulse = mdrv->motor_comp_cvalue;
		memcpy(current_register+ICE40_R_ACC_RATE0, close_r_acc_dec_rate_and_count, ACC_DEC_RATE_AND_CNT_ARRAY_SIZE);
	}

	/*
	 * 2. Pulse
	 * 1) Total Pulse : 4920(0x1338)
	 * 2) Half  Pulse : 2714(0x0A9A), 2206(0x089E)
	 *     1080    (286)     1366    (234)    1600
	 *        COLLAPSE_PARTIAL    EXPAND_PARTIAL
	 *           2714(0xA9A)      2206(0x89E)
	 *      |------------------|----------------|
	 * Ratio - 1 : 10000
	 */
	/* Last moving Pulse */
	pwm_pulse = get_motor_moving_pulse();
	updated_pulse = (mdrv->motor_total_pulse * mdrv->motor_pulse_ratio) / 10000 + compensated_pulse;
	if ((pwm_pulse > updated_pulse) && !(control_value & MOTOR_DIRECTION_MASK) && (position != MOTOR_POSITION_16_9)) {
		pr_info("%s : Apply PWM Count. Setting Pulse = %d/%d(0x%x)\n", __func__, pwm_pulse, mdrv->motor_total_pulse, pwm_pulse);
		current_register[ICE40_R_TOTAL_PPS_7_0]   = pwm_pulse;
		current_register[ICE40_R_TOTAL_PPS_15_8]  = pwm_pulse>>8;
	} else {
		pr_info("%s : Apply OTS Count. Setting Pulse = %d/%d(0x%x), Compensated Value = %d\n", __func__, updated_pulse, mdrv->motor_total_pulse, updated_pulse, compensated_pulse);
		current_register[ICE40_R_TOTAL_PPS_7_0]   = updated_pulse;
		current_register[ICE40_R_TOTAL_PPS_15_8]  = updated_pulse>>8;
	}

	/* Write Motor Current Reigster */
	ret = regmap_raw_write(global_ice40->regmap, ICE40_R_FH_TIME_CNT_15_8, current_register+ICE40_R_FH_TIME_CNT_15_8, MOTOR_REG_WRITE_DATA_MAX);
	if (ret) {
		pr_err("%s : Failed to write. ice40 Reset and Re-write.\n", __func__, ret);

		ret = ice40_reset();
		if (ret)
			goto motor_fail;

		ice40_set_lreset(MOTOR_LRESET_ENABLE);
		ret = regmap_raw_write(global_ice40->regmap, ICE40_R_FH_TIME_CNT_15_8, current_register+ICE40_R_FH_TIME_CNT_15_8, MOTOR_REG_WRITE_DATA_MAX);
	}

motor_contorl:
	ret = regmap_write(global_ice40->regmap, ICE40_R_CTRL, (u8)control_value);
	if (ret) {
		goto motor_fail;
	}

#if defined(MOTOR_WAKE_LOCK)
	disable_irq_wake(mdrv->wakeup_irq);
#endif

    return MOTOR_SUCCESS;

motor_fail:
	pr_err("%s : Motor Control Fail\n", __func__);

	if (ice40_get_lreset())
		ice40_set_lreset(MOTOR_LRESET_DISABLE);

#if defined(MOTOR_WAKE_LOCK)
	disable_irq_wake(mdrv->wakeup_irq);
#endif
	return -MOTOR_EIO;
}
EXPORT_SYMBOL(set_motor_control);

/*
 * < Motor Enable/Disable IRQ Handler >
 * EDGE_RISING  - Enable Motor and Genrate PWM
 * EDGE_FALLING - Disable Motor
 */
extern int pat9125_set_motor_status(int pwm_info, int pps_info);
irqreturn_t motor_status_irq_handler(int irq, void *__mot_drv)
{
	struct irq_data *irq_data = irq_get_irq_data(irq);

	u32 flag = irqd_get_trigger_type(irq_data);
	bool high = gpiod_get_value(mdrv->bdata.status);

	if (flag == IRQ_TYPE_EDGE_BOTH) {
		mdrv->motor_status = high ? MOTOR_PWM_ENABLE : MOTOR_PWM_DISABLE;
		pat9125_set_motor_status(mdrv->motor_status, mdrv->motor_max_pps);
	} else {
		mdrv->motor_status = MOTOR_PWM_DISABLE;
	}
	pr_info("%s : motor_status(%d)\n", __func__, mdrv->motor_status);

	return IRQ_HANDLED;
}

/*
 * < HIDL sysnode >
 * sys/devices/platform/soc/soc:lge,motor-drv/
 * mvalue_position                  : motor position          (get, set)
 * mvalue_motor_move_pulse_ratio    : motor pulse for moving  (   , set)
 * mvalue_total_pulse               : Total Pulse             (get, set)
 * mvalue_change_pulse              : Change Pulse            (get,    )
 * mvalue_motor_fh_time             : Set FH Time number      (get, set)
 * mvalue_motor_temperature_sect    : Set Max PPS according T (get, set)
 * mvalue_motor_max_freq_cnt        : Set Max Frequency value (get, set)
 * mvalue_motor_min_freq_cnt        : Set Min Frequency value (get, set)
 * mvalue_motor_open_acc_dec_store  : Set Open  Rate/Count    (   , set)
 * mvalue_motor_close_acc_dec_store : Set Close Rate/Count    (   , set)
 * mvalue_motor_one_register        : One Register For Tuning (   , set)
 */
static ssize_t motor_position_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	return sprintf(buf, "%d\n", mdrv->motor_kposition);
}

static ssize_t motor_position_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	ssize_t ret = 0;
	int value = 0;

	pr_info("%s : Set Position = %s\n", __func__, buf);

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf , 10, &value);

	ret = set_motor_control(value);
	if (ret) {
		pr_err("%s : Could not control motor %d\n", __func__, ret);

		return ret;
	}

	pr_info("%s : Leave.\n", __func__);

	return size;
}

static ssize_t motor_pulse_ratio_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_pulse_ratio);

	return size;
}

static ssize_t motor_fh_time_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	return sprintf(buf, "%d\n", mdrv->motor_fh_pps_index);
}

static ssize_t motor_fh_time_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_fh_pps_index);

	set_motor_fh_time(mdrv->motor_fh_pps_index);
	set_motor_fl_time(mdrv->motor_fh_pps_index);

	motor_fh_time_write();

	pr_info("%s : 8bit High %x, 8bit Low %x.\n", __func__,
		current_register[ICE40_R_FH_TIME_CNT_15_8],
		current_register[ICE40_R_FH_TIME_CNT_7_0]);

	return size;
}

static ssize_t motor_temperature_sect_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int pps_index = 0;

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_temperature_sect);

	/*
	 *  Max Frequency According to Temperature
	 *  : Changes depending on hardware characteristics according to temperature
	 *                                                Normal     ETC
	 *     Temperature  Index     PPS        Time      Value    Value
	 *  1)      ~ -5'C    1      500PPS     9.60Sec      8        8
	 *  2) -5'C ~  5'C    2     1400PPS     3.88Sec      6        7
	 *  3)  5'C ~ 10'C    3     1800PPS     3.30Sec      4        6
	 *  4) 10'C ~ 15'C    4     2200PPS     2.85Sec      2        5
	 *  5) 15'C ~         5     2400PPS     2.70Sec      0        4
	 *
	 *  If set Hidden Menu,
	 *                  Normal       ETC
	 *       Index       Value      Value
	 *  1)    0,1        0,1          4
	 *  2)    2,3        2,3          5
	 *  3)    4,5        4,5          6
	 *  4)    6,7        6,7          7
	 *  5)     8          8           8
	 */
	if (mdrv->motor_fh_pps_index != 0) {
		if (mdrv->motor_operation_type == MOTOR_NORMAL)
			pps_index = mdrv->motor_fh_pps_index;
		else
			pps_index = (mdrv->motor_fh_pps_index / 2) + 4;
	} else {
		if (mdrv->motor_operation_type == MOTOR_NORMAL)
			pps_index = 10 - 2 * mdrv->motor_temperature_sect;
		else
			pps_index = 9 - mdrv->motor_temperature_sect;
	}
	pr_info("%s : Max Frequency Temperature section = %d. PPS Value = %d\n", __func__, mdrv->motor_temperature_sect, pps_index);
	set_motor_fh_time(pps_index);
	set_motor_fl_time(pps_index);

	return size;
}

static ssize_t motor_temperature_sect_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int time = -1;
	int total_time = 0;
	int temp_ratio = 0;
	int motor_pps = 1;
	int compensated_time = COMPENSATED_OPEN_TIME;

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	temp_ratio = (mdrv->motor_pulse_ratio > 0) ? mdrv->motor_pulse_ratio : -mdrv->motor_pulse_ratio;
#if 0
	if (temp_ratio < EXCEPTION_CASE_PULSE_RATIO && mdrv->motor_pulse_ratio < 0) {
		motor_max_pps = mdrv->motor_min_pps;
		compensated_time = COMPENSATED_EXCEPTION_TIME;
	} else {
		motor_max_pps = mdrv->motor_max_pps;
		if (mdrv->motor_pulse_ratio < 0) {
			compensated_time = (COMPENSATED_CLOSE_VALUE * 1000) / motor_max_pps + COMPENSATED_CLOSE_TIME;
		}
	}
#else
	motor_pps = mdrv->motor_max_pps;
	if (mdrv->motor_pulse_ratio < 0) {
		compensated_time = (COMPENSATED_CLOSE_VALUE * 1000) / motor_pps + COMPENSATED_CLOSE_TIME;
	}
#endif

	total_time = (mdrv->motor_total_pulse * 1000) / motor_pps;
	time = (total_time * temp_ratio) / 10000 + compensated_time;

	pr_info("%s : motor total time %d, compensated value = %d, ratio time %d.\n", __func__, total_time, compensated_time, time);

	return sprintf(buf, "%d\n", time);
}

static ssize_t motor_change_pulse_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int changed_pulse = 0;

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	changed_pulse = get_motor_moving_pulse();
	if (changed_pulse < 0) {
		pr_err("%s : Fail to get current pulse%d\n", __func__, changed_pulse);

		return -MOTOR_EIO;
	}

	return sprintf(buf, "%d\n", changed_pulse);
}

static ssize_t motor_total_pulse_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	return sprintf(buf, "%d\n", mdrv->motor_total_pulse);
}

static ssize_t motor_total_pulse_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_total_pulse);

	pr_info("%s : 8bit High 0x%x, 8bit Low 0x%x.\n", __func__, mdrv->motor_total_pulse>>8, mdrv->motor_total_pulse);

	return size;
}

static ssize_t motor_driver_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	ret = check_lattice_driver(true);
	if (ret < 0) {
		pr_err("%s : Checking Lattice Driver Fail\n", __func__);
	}

	return sprintf(buf, "%d\n", ret);
}

static ssize_t motor_max_freq_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	return sprintf(buf, "%d\n", mdrv->motor_max_pps);
}

static ssize_t motor_max_freq_cnt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int tmp = 0;

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_max_pps);

	if (mdrv->motor_max_pps > 0)
		tmp = LATTTICE_MCU_CLK / mdrv->motor_max_pps;

	default_register[ICE40_R_FH_TIME_CNT_15_8] = tmp>>8;
	default_register[ICE40_R_FH_TIME_CNT_7_0]  = tmp;

	pr_info("%s : %d PPS, 8bit High 0x%x, 8bit Low 0x%x.\n", __func__, mdrv->motor_max_pps,
		default_register[ICE40_R_FH_TIME_CNT_15_8],
		default_register[ICE40_R_FH_TIME_CNT_7_0]);

	return size;
}

static ssize_t motor_min_freq_cnt_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	return sprintf(buf, "%d\n", mdrv->motor_min_pps);
}

static ssize_t motor_min_freq_cnt_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int tmp = 0;

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_min_pps);

	if (mdrv->motor_max_pps > 0)
		tmp = LATTTICE_MCU_CLK / mdrv->motor_min_pps;

	default_register[ICE40_R_FL_TIME_CNT_19_16] = tmp>>16;
	default_register[ICE40_R_FL_TIME_CNT_15_8]  = tmp>>8;
	default_register[ICE40_R_FL_TIME_CNT_7_0]   = tmp;

	pr_info("%s : %d PPS, 8bit High 0x%x, 8bit Mid 0x%x, 8bit Ligh 0x%x.\n", __func__, mdrv->motor_max_pps,
		default_register[ICE40_R_FL_TIME_CNT_19_16],
		default_register[ICE40_R_FL_TIME_CNT_15_8],
		default_register[ICE40_R_FL_TIME_CNT_7_0]);

	return size;
}

static ssize_t motor_one_register_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u16 temp = 0;

	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtou16(buf, 16, &temp);

	default_register[temp/256] = temp%256;
	current_register[temp/256] = temp%256;

	return size;
}

static ssize_t motor_open_acc_dec_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u16 temp = 0;

	if (mdrv == NULL) {
		pr_err("%s: motor did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtou16(buf, 16, &temp);

	default_register[temp/256] = temp%256;

	return size;
}

static ssize_t motor_close_acc_dec_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	u16 temp = 0;

	if (mdrv == NULL) {
		pr_err("%s: motor did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtou16(buf, 16, &temp);

	close_r_acc_dec_rate_and_count[(temp/256)-ICE40_R_ACC_RATE0] = temp%256;

	return size;
}

static ssize_t motor_compensated_close_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	return sprintf(buf, "%d\n", mdrv->motor_comp_cvalue);
}

static ssize_t motor_compensated_close_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_comp_cvalue);

	pr_info("%s : motor_comp_cvalue %d.\n", __func__, mdrv->motor_comp_cvalue);

	return size;
}

static ssize_t motor_compensated_exception_value_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	return sprintf(buf, "%d\n", mdrv->motor_comp_evalue);
}

static ssize_t motor_compensated_exception_value_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_comp_evalue);

	pr_info("%s : motor_comp_evalue %d.\n", __func__, mdrv->motor_comp_evalue);

	return size;
}

static ssize_t motor_operation_type_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	if (mdrv == NULL) {
		pr_err("%s: Motor Driver did not probe\n", __func__);

		return -MOTOR_ENODEV;
	}

	kstrtoint(buf, 10, &mdrv->motor_operation_type);

	pr_info("%s : motor_operation_type %d.\n", __func__, mdrv->motor_operation_type);

	return size;
}

static DEVICE_ATTR(mvalue_motor_position,          S_IWUSR | S_IRUGO, motor_position_show, motor_position_store);
static DEVICE_ATTR(mvalue_motor_pulse_ratio,       S_IWUSR | S_IRUGO, NULL, motor_pulse_ratio_store);
static DEVICE_ATTR(mvalue_motor_fh_time,           S_IWUSR | S_IRUGO, motor_fh_time_show, motor_fh_time_store);
static DEVICE_ATTR(mvalue_motor_temperature_sect,  S_IWUSR | S_IRUGO, motor_temperature_sect_show, motor_temperature_sect_store);
static DEVICE_ATTR(mvalue_motor_total_pulse,       S_IWUSR | S_IRUGO, motor_total_pulse_show, motor_total_pulse_store);
static DEVICE_ATTR(mvalue_motor_change_pulse,      S_IWUSR | S_IRUGO, motor_change_pulse_show, NULL);
static DEVICE_ATTR(mvalue_motor_driver_check,      S_IWUSR | S_IRUGO, motor_driver_check_show, NULL);
static DEVICE_ATTR(mvalue_motor_max_freq_cnt,      S_IWUSR | S_IRUGO, motor_max_freq_cnt_show, motor_max_freq_cnt_store);
static DEVICE_ATTR(mvalue_motor_min_freq_cnt,      S_IWUSR | S_IRUGO, motor_min_freq_cnt_show, motor_min_freq_cnt_store);
static DEVICE_ATTR(mvalue_motor_one_register,      S_IWUSR | S_IRUGO, NULL, motor_one_register_store);
static DEVICE_ATTR(mvalue_motor_open_acc_dec,      S_IWUSR | S_IRUGO, NULL, motor_open_acc_dec_store);
static DEVICE_ATTR(mvalue_motor_close_acc_dec,     S_IWUSR | S_IRUGO, NULL, motor_close_acc_dec_store);
static DEVICE_ATTR(mvalue_motor_comp_cvalue,       S_IWUSR | S_IRUGO, motor_compensated_close_value_show, motor_compensated_close_value_store);
static DEVICE_ATTR(mvalue_motor_comp_evalue,       S_IWUSR | S_IRUGO, motor_compensated_exception_value_show, motor_compensated_exception_value_store);
static DEVICE_ATTR(mvalue_motor_operation_type,    S_IWUSR | S_IRUGO, NULL, motor_operation_type_store);

static struct attribute *motor_sysfs_attrs[] = {
	&dev_attr_mvalue_motor_position.attr,
	&dev_attr_mvalue_motor_pulse_ratio.attr,
	&dev_attr_mvalue_motor_fh_time.attr,
	&dev_attr_mvalue_motor_temperature_sect.attr,
	&dev_attr_mvalue_motor_total_pulse.attr,
	&dev_attr_mvalue_motor_change_pulse.attr,
	&dev_attr_mvalue_motor_driver_check.attr,
	&dev_attr_mvalue_motor_max_freq_cnt.attr,
	&dev_attr_mvalue_motor_min_freq_cnt.attr,
	&dev_attr_mvalue_motor_one_register.attr,
	&dev_attr_mvalue_motor_open_acc_dec.attr,
	&dev_attr_mvalue_motor_close_acc_dec.attr,
	&dev_attr_mvalue_motor_comp_cvalue.attr,
	&dev_attr_mvalue_motor_comp_evalue.attr,
	&dev_attr_mvalue_motor_operation_type.attr,
	NULL,
};
ATTRIBUTE_GROUPS(motor_sysfs);

/*
 * < Device Tree Parsing >
 * 1) Motor Status - GPIO 115
 * 2) Motor Init Resister
 */
static int motor_driver_parse_dt(struct motor_drv *drv)
{
	struct device_node *node = drv->pdev->dev.of_node;
	struct device *dev = &drv->pdev->dev;
	int ret = 0;

	pr_info( "%s : Enter parse dt\n", __func__);

	/* Motor Status Pin GPIO 115 */
	mdrv->bdata.status = devm_gpiod_get(dev, "lge,status", GPIOD_IN);
	if (IS_ERR(mdrv->bdata.status)) { // HW Rev.A
		ret = PTR_ERR(mdrv->bdata.status);
		pr_info("%s : Can't find motor status gpio : %d\n", __func__, ret);

		return ret;
	} else {
		mdrv->motor_status_irq = gpiod_to_irq(mdrv->bdata.status);

		ret = devm_request_threaded_irq(dev, mdrv->motor_status_irq, NULL, motor_status_irq_handler,
			IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,  "motor_status_irq", mdrv);
		if (ret < 0) {
			pr_info("%s : Status IRQ request thread failed, ret=%d\n", __func__, ret);
			return ret;
		}
	}

	/* Motor Register */
	ret = of_property_read_variable_u8_array(node, "lge,motor-reg", default_register, 1, MOTOR_REG_DATA_MAX);
	if (ret >= 0) {
		pr_info("%s : Read Motor DT Cal\n", __func__);
		mdrv->motor_default_cal_dt = true;

		/* Init Default Setting */
		mdrv->motor_total_pulse = (int)(default_register[ICE40_R_TOTAL_PPS_15_8]<<8 | default_register[ICE40_R_TOTAL_PPS_7_0]);
		mdrv->motor_max_pps = LATTTICE_MCU_CLK / (int)(current_register[ICE40_R_FH_TIME_CNT_15_8]<<8 | current_register[ICE40_R_FH_TIME_CNT_7_0]);
		mdrv->motor_min_pps = LATTTICE_MCU_CLK / (int)(current_register[ICE40_R_FL_TIME_CNT_15_8]<<8 | current_register[ICE40_R_FL_TIME_CNT_7_0]);
	} else {
		pr_info("%s : Can't find the Motor DT Registers. Set Default Registers\n", __func__);
	}

	/* Motor Close ACC/DEC Register */
	ret = of_property_read_variable_u8_array(node, "lge,motor-close-reg", close_r_acc_dec_rate_and_count, 1, ACC_DEC_RATE_AND_CNT_ARRAY_SIZE);
	if (ret >= 0) {
		pr_info("%s : Read Motor Close DT Cal\n", __func__);
	} else {
		pr_info("%s : Can't find the Motor Close DT Registers. Set Default Registers\n", __func__);
	}

	/* Motor Exception ACC/DEC Register */
	ret = of_property_read_variable_u8_array(node, "lge,motor-exception-reg", exception_r_acc_dec_rate_and_count, 1, ACC_DEC_RATE_AND_CNT_ARRAY_SIZE);
	if (ret >= 0) {
		pr_info("%s : Read Motor Exception DT Cal\n", __func__);
	} else {
		pr_info("%s : Can't find the Motor Close DT Registers. Set Default Registers\n", __func__);
	}

	pr_info( "%s : Leave parse dt\n", __func__);

	return 0;
}

/*
 * < Motor Driver Probe >
 */
static int motor_driver_probe(struct platform_device *pdev)
{
	struct motor_drv *drv = NULL;
	int ret = 0;

	pr_info("%s : Enter\n", __func__);

	/* Check CDONE Pin */
	if (ice40_get_cdone() == false) {
		pr_err("%s: lattice cdone pin is low or FPGA probe Fail.\n", __func__);

		return -MOTOR_EIO;
	}

	drv = devm_kzalloc(&pdev->dev, sizeof(*drv), GFP_KERNEL);
	if (!drv) {
		pr_err("%s : NULL %d\n", __func__, __LINE__);

		return -MOTOR_ENOMEM;
	}

	drv->pdev = pdev;
	mdrv = drv;

	mdrv->motor_total_pulse = MOTOR_DEFAULT_PULSE;
	mdrv->motor_comp_cvalue = COMPENSATED_CLOSE_VALUE;
	mdrv->motor_comp_evalue = COMPENSATED_EXCEPTION_VALUE;

	ret = motor_driver_parse_dt(drv);
	if (ret < 0) {
		pr_err("%s : Device Tree Parsing Fail\n", __func__);
		goto dt_fail;
	}

	dev_set_drvdata(&pdev->dev, drv);

#if defined(MOTOR_WAKE_LOCK)
	device_init_wakeup(&pdev->dev, true);
	dev_pm_set_wake_irq(&pdev->dev, mdrv->wakeup_irq);
#endif

	ret = check_lattice_driver(true);
	if (ret < 0) {
		pr_err("%s : Checking Lattice Driver Fail\n", __func__);
		goto probe_fail;
	}

	/*
	 * Factory Assembly process
	 * Cable Type = 56K
	 * USB = DCP
	 * QEM = 1 
	 */
	if (lge_get_boot_cable() == LT_CABLE_56K_DCP)  {
		pr_info("%s : lge_get_boot_cable LT_56K_DCP(%d) \n", __func__, LT_CABLE_56K_DCP);

		mdrv->motor_fh_pps_index = 5;
		set_motor_fh_time(mdrv->motor_fh_pps_index);
		set_motor_fl_time(mdrv->motor_fh_pps_index);

		mdrv->motor_pulse_ratio = 10000;
		ret = set_motor_control(MOTOR_POSITION_EXPAND);
		if (ret)
			pr_err("%s : Could not control motor %d\n", __func__, ret);
	}

	pr_info("%s : Leave\n", __func__);

	return MOTOR_SUCCESS;

probe_fail:
#if defined(MOTOR_WAKE_LOCK)
	device_init_wakeup(&pdev->dev, false);
#endif
	dev_set_drvdata(&pdev->dev, NULL);
	ice40_set_lreset(MOTOR_LRESET_DISABLE);

dt_fail:
	devm_free_irq(&pdev->dev, mdrv->motor_status_irq, mdrv);

	mdrv = NULL;

	pr_err("%s : Error\n", __func__);

	return -MOTOR_ENODEV;
}

/*
 * < Motor Driver Remove >
 */
static int motor_driver_remove(struct platform_device *pdev)
{
    pr_info("%s : Enter\n", __func__);

#if defined(MOTOR_WAKE_LOCK)
	device_init_wakeup(&pdev->dev, false);
#endif
	dev_set_drvdata(&pdev->dev, NULL);
	ice40_set_lreset(MOTOR_LRESET_DISABLE);

	devm_free_irq(&pdev->dev, mdrv->motor_status_irq, mdrv);

    pr_info("%s : Leave\n", __func__);

	return 0;
}

/* 
 * < Motor Driver OPS >
 */
static int motor_suspend(struct device *dev)
{
	pr_info("%s : Enter.\n", __func__);

	if (ice40_get_lreset()) {
		ice40_set_lreset(MOTOR_LRESET_DISABLE);
	}

	return 0;
}

static int motor_resume(struct device *dev)
{
	pr_info("%s : Enter.\n", __func__);

	return 0;
}

static const struct dev_pm_ops motor_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(motor_suspend, motor_resume)
};

/*
 * < Motor Platform Driver Setting and Module Init >
 */
static const struct of_device_id motor_driver_dt_match[] = {
	{.compatible = "lge,motor-drv", },
	{},
};
MODULE_DEVICE_TABLE(of, motor_driver_dt_match);

static struct platform_driver motor_driver = {
	.driver             = {
		.name           = "lge,motor-drv",
		.owner          = THIS_MODULE,
		.pm             = &motor_pm_ops,
		.of_match_table = motor_driver_dt_match,
		.dev_groups     = motor_sysfs_groups,
	},
	.probe              = motor_driver_probe,
	.remove             = motor_driver_remove,
};
//EXPORT_SYMBOL(motor_driver);
//module_platform_driver(motor_driver);

static int __init motor_late_init(void)
{
	pr_info("%s : Enter\n", __func__);
	platform_driver_register(&motor_driver);

	return 0;
}
late_initcall(motor_late_init);
static void __exit motor_exit(void)
{
	pr_info("%s : Enter\n", __func__);
	platform_driver_unregister(&motor_driver);
}
module_exit(motor_exit);

MODULE_DESCRIPTION("Stepper Motor Driver");
MODULE_VERSION("0.9");
