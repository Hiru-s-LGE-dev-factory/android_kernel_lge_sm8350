/*
 * Base Driver for pat9126
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
 *  - Revision Record
 *   V1.1. 2020.06.29 Modified ABS_Z value
*/
#define DEBUG
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/lge_panel_notify.h>
#include <linux/lge_hall_sensor_notifier.h>
#include <soc/qcom/lge/board_lge.h>
#include "pixart_ots.h"
#include "pixart_platform.h"

#define pat9125_name 			"pat9125"
#define pat9125_DEV_NAME    	 pat9125_name

#define XCNT(X) ((X) < 0 ? -(X) : (X))
#define XWAY(X) ((X) == 0 ? TARGET_POSITION_UNKNOWN : (X) < 0 ? TARGET_POSITION_COLLAPSE : TARGET_POSITION_EXPAND)
#define TWAY(X) ((X) == TARGET_POSITION_HALF ? TARGET_POSITION_EXPAND : (X))

struct pat9125_linux_data_t {
	struct i2c_client	*client;
	struct input_dev	*pat9125_input_dev;
	struct device		*i2c_dev;
	struct regulator	*vdd;
	int		irq_gpio;
	int		irq;
	u32		irq_flags;
	u64		last_jiffies;
	int		preCount;
	int		rawCount;
	int		otsCount;
	int		syncCount;
	int		startCount;
	int		targetCount;
	int		markPosition;
	int		completeStatus;
	int		movingStatus;
	int		forceMoveStatus;
	int		hallicStatus;
	int		hallicEn;
	int		hallsensorStatus;
	int		hallsensorEn;
	struct notifier_block hallic_notifier;
	struct notifier_block hallsensor_notifier;
};
static struct wakeup_source *g_otsWakelock;

static int pat9125_init_input_data(void);
static int pat9125_i2c_write(u8 reg, u8 *data, int len);
static int pat9125_i2c_read(u8 reg, u8 *data);
static void pat9125_set_position_work(struct work_struct *work);
static void pat9125_interval_check(struct work_struct *work);
static void pat9125_mark_stop_time_offset(struct work_struct *work);
static void pat9125_exception_handling(int16_t exception);
static void pat9125_get_marking_status(void);

extern int set_motor_control(int status);

static struct pat9125_linux_data_t pat9125data;
static int set_position = TARGET_POSITION_UNKNOWN;
static int pre_position = TARGET_POSITION_UNKNOWN;
static int set_dummy_enable = DISABLE;
static int set_ots_count = PIXEL_MIN;
static int set_offset = PIXEL_CAL_OFFSET;
static int set_stop_offset_close = MARK_STOP_TIME_OFFSET_CLOSE;
static int set_stop_offset_open = MARK_STOP_TIME_OFFSET_OPEN;

static int set_interval = DISABLE;
static int set_motor_request = ENABLE;
static int set_mark_stop = ENABLE;
static int set_motor_back = ENABLE;
static int set_motor_force_move = ENABLE;
static int set_try_again = DISABLE;

static int set_nomal_interval = MONITORING_INTERVAL_NOMAL;
static int set_start_interval = MONITORING_INTERVAL_START;
static int set_end_interval = MONITORING_INTERVAL_END;
static int set_force_interval = MONITORING_INTERVAL_FORCE;

static int set_motor_stop_th = MONITORING_MOVING_FAST;
static int set_force_move_up_th = MONITORING_MOVING_FORCE_UP;
static int set_force_move_down_th = MONITORING_MOVING_FORCE_DOWN;

static int set_cal_factor = DISABLE;
static int calibration_factor = PIXEL_MAX;
static int calibration_factor_changed = PIXEL_MAX;
static int shutter = NON_DETECT;
static int wshutter = MKR_SHUTTER_W_MIN;
static int shutter_th = MKR_SHUTTER_TH_MIN;

static int black_shutter = MKR_SHUTTER_ERROR;
static int white_shutter = MKR_SHUTTER_TH_MIN;

static int slide_state = TARGET_POSITION_UNKNOWN;
static int recovery_mode = DISABLE;

static int minios_mode = DISABLE;
static int enable_mode = DISABLE;
static int start_collapse = DISABLE;
static int exception_moving = DISABLE;
static int check_stop_work = DISABLE;
static int ots_breakdown = DISABLE;

static int logging_count_en = DISABLE;

static DECLARE_DELAYED_WORK(work, pat9125_set_position_work);
static DECLARE_DELAYED_WORK(work_param, pat9125_interval_check);
static DECLARE_DELAYED_WORK(work_stop, pat9125_mark_stop_time_offset);

/**************************************/
extern unsigned char ReadData(unsigned char addr)
{
	u8 data = 0xff;
	int rc = 0;
	if(ots_breakdown) {
		pr_debug("pat9125_%s (%d) : OTS Breakdown\n", __func__, __LINE__);
		return -1;
	}

	rc = pat9125_i2c_read(addr, &data);
	if(rc < 0) {
		ots_breakdown = ENABLE;
		set_motor_request = DISABLE;
		return -1;
	}
	return data;
}
extern void WriteData(unsigned char addr, unsigned char data)
{
	pat9125_i2c_write(addr, &data, 1);
}
extern void delay_ms(int ms)
{
	msleep(ms);
}
/**************************************/
static int pat9125_i2c_write(u8 reg, u8 *data, int len)
{
	u8  buf[20];
	int rc;
	int ret = 0;
	int i;

	buf[0] = reg;
	if (len >= 20) {
		pr_debug(
			"%s (%d) : FAILED: buffer size is limitted(20) %d\n",
			__func__, __LINE__, len);
		dev_err(&pat9125data.client->dev, "pat9125_i2c_write FAILED: buffer size is limitted(20)\n");
		return -ENODEV;
	}

	for (i = 0 ; i < len; i++)
		buf[i+1] = data[i];

	/* Returns negative errno, or else the number of bytes written. */
	rc = i2c_master_send(pat9125data.client, buf, len+1);

	if (rc != len+1) {
		pr_debug(
			"%s (%d) : FAILED: writing to reg 0x%x\n",
			__func__, __LINE__, reg);

		ret = -ENODEV;
	}else{
		pr_debug(
			"%s (%d) : SUCCESS: writing to reg 0x%x\n",
			__func__, __LINE__, reg);
	}

	return ret;
}

static int pat9125_i2c_read(u8 reg, u8 *data)
{
	u8  buf[20];
	int rc;

	buf[0] = reg;

	/* If everything went ok (i.e. 1 msg transmitted),
	return #bytes  transmitted, else error code.
	thus if transmit is ok  return value 1 */
	rc = i2c_master_send(pat9125data.client, buf, 1);
	if (rc != 1) {
		pr_debug(
			"%s (%d) : FAILED: writing to address 0x%x\n",
			__func__, __LINE__, reg);
		return -ENODEV;
	}

	/* returns negative errno, or else the number of bytes read */
	rc = i2c_master_recv(pat9125data.client, buf, 1);
	if (rc != 1) {
		 pr_debug(
			"%s (%d) : FAILED: reading data\n",
			__func__, __LINE__);
		return -ENODEV;
	}else{
		//ots_breakdown = DISABLE;
	}

	*data = buf[0];
	return 0;
}
/*********************************************************************************/

static int pat9125_read_file(char* path)
{
	int fd;
	char buf[8];
	int value = 0;

	mm_segment_t old_fs = get_fs();

	pr_debug("%s (%d) : Enter\n", __func__, __LINE__);

	set_fs(KERNEL_DS);
	fd = ksys_open(path, O_RDONLY, 0);

	if (fd >= 0) {
		ksys_read(fd, buf, sizeof(buf));
		sscanf(buf, "%d", &value);
		pr_debug("%s : File open Success. Result = %d  ", __func__, value);

		ksys_close(fd);
	} else {
		pr_debug("%s : File open Fail. Set Default Register\n", __func__);
	}

	set_fs(old_fs);
	pr_debug("%s : Leave. %d\n", __func__, fd);
	return value;
}

static void pat9125_write_file(char* path, int value)
{
	int fd;
	char buf[10];
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	fd = ksys_open (path, O_WRONLY|O_TRUNC|O_CREAT, 0666);
	ksys_chmod(path, 0666);

	if (fd >= 0){
		sprintf(buf,"%d", value);
		ksys_write (fd, buf, sizeof(buf));
		pr_debug("%s : File open Success.", __func__);
		ksys_close(fd);
	} else {
		pr_debug("%s : File open Fail. %s\n", __func__, path);
	}

	set_fs (old_fs);
	pr_debug("%s : Leave. %d\n", __func__, fd);
}

/************************************************************************/

static int pat9125_position_to_count(int position)
{
	int count = 0;
	switch(position){
		case TARGET_POSITION_COLLAPSE :
			count = PIXEL_MIN;
			break;
		case TARGET_POSITION_EXPAND :
			count = PIXEL_MAX;
			break;
		case TARGET_POSITION_HALF:
			count = PIXEL_HALF;
			break;
		default :
			count = PIXEL_MIN;
			break;
	}
	return count;
}

static int pat9125_count_to_position(int count)
{
	int position = 0;
	switch(count){
		case PIXEL_MIN :
			position = TARGET_POSITION_COLLAPSE;
			break;
		case PIXEL_MAX :
			position = TARGET_POSITION_EXPAND;
			break;
		case PIXEL_HALF:
			position = TARGET_POSITION_HALF;
			break;
		default :
			position = set_position;
			break;
	}
	return position;
}
/****************report ots event data***********************************/
static void pat9125_report(int count, int status, int target, unsigned char OutOtsState)
{
	switch(OutOtsState){
		case OTS_ROT_NO_CHANGE:
			pr_debug("%s : OTS_ROT_NO_CHANGE count = %d, state = %d \n", __func__, count, status);
			break;
		case OTS_ROT_UP:
			if(count <= pat9125data.syncCount)	return;
			if(pat9125data.forceMoveStatus)	break;
			if(set_position == TARGET_POSITION_HALF && count > PIXEL_HALF)
				return;
			break;
		case OTS_ROT_DOWN:
			if(count >= pat9125data.syncCount)	return;
			if(pat9125data.forceMoveStatus)	break;
			if(set_position == TARGET_POSITION_HALF && count < PIXEL_HALF)
				return;
			break;
		case OTS_ROT_STOP:
			if(set_position == TARGET_POSITION_COLLAPSE){
				pat9125data.preCount = count;
				pat9125data.rawCount = count;
				pat9125data.otsCount = count;
			}
			if(set_position == TARGET_POSITION_EXPAND){
				pat9125data.preCount = calibration_factor_changed;
				pat9125data.rawCount = calibration_factor_changed;
				pat9125data.otsCount = calibration_factor_changed;
			}
			target = count;
			pr_debug("%s : OTS_ROT_STOP count = %d, state = %d \n", __func__, count, status);
			break;
		default:
			break;
	}

	if(status == RESET_FLAG){
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_X, count);
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Y, status);
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Z, target);
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_RZ, status);
		return;
	}
	if(count >= PIXEL_MIN && count <= PIXEL_MAX){
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_X, count);
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Y, status);
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Z, target);
		input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_RZ, pat9125data.hallsensorStatus);
		input_sync(pat9125data.pat9125_input_dev);
		pat9125data.syncCount = count;
	}
}

/****************position initialize***********************************/

#ifdef CONFIG_LGE_SAR_CONTROLLER_OTS_DETECT
static void pat9125_notify_sar_controller(int targetCount)
{
    extern void sar_controller_ots_notify_connect(u32 targetCount);
    static int pre_targetCount = 0;

    if( pre_targetCount != targetCount) {
        sar_controller_ots_notify_connect((u32)targetCount);
        pre_targetCount = targetCount;
        pr_info("pat9125 call notify_sar_controller.\n");
    }
}
#endif

static void update_slide_state(int count)
{
	pre_position = pat9125_count_to_position(count);
	start_collapse = DISABLE;
	set_try_again = DISABLE;
	exception_moving = DISABLE;
	slide_state = SLIDE_COLLAPSE + count;
#ifdef CONFIG_LGE_SAR_CONTROLLER_OTS_DETECT
	pat9125_notify_sar_controller(count);
#endif
}

static void pat9125_access_read_position(int mode)
{
	if(mode == ENABLE_OTS_SENSOR && !minios_mode && !enable_mode){
		int slide_ftm_status = 0;
		enable_mode = ENABLE;
		kstrtoint(get_slide_ftm_status(), 10, &slide_ftm_status);
		pat9125data.otsCount = (slide_ftm_status - SLIDE_COLLAPSE);

		if(!pat9125data.hallsensorEn){
			pat9125data.otsCount = PIXEL_MIN;
			pat9125data.completeStatus = MOT_STOP_NON_MARK;
			goto report;
		}
		pat9125_get_marking_status();
		pat9125data.hallsensorStatus = lge_hall_sensor_state;
		if(pat9125data.hallsensorStatus == LGE_HALL_SENSOR_CLOSE){
			pat9125data.otsCount = PIXEL_MIN;
		} else if(pat9125data.hallsensorStatus == LGE_HALL_SENSOR_OPEN && pat9125data.otsCount != PIXEL_MAX) {
			pat9125data.completeStatus = MOT_STOP_NON_MARK;
		}
		pr_debug("%s (%d) : slide_ftm_status = %d, otsCount = %d\n", __func__,__LINE__, slide_ftm_status, pat9125data.otsCount);
	} else if(mode == ENABLE_OTS_SENSOR && enable_mode) {
		pat9125_report(RESET_FLAG, RESET_FLAG , 0, OTS_ROT_STOP);
		pat9125_report(pat9125data.syncCount, pat9125data.completeStatus, 0, OTS_ROT_STOP);
		pr_debug("%s : init otsCount = %d , mark_detect(%d) , hall(%d)\n"
		, __func__, pat9125data.otsCount, pat9125data.completeStatus, pat9125data.hallsensorStatus);
		return;
	} else if(mode == ENABLE_MINIOS_MODE && !minios_mode){
		minios_mode = ENABLE;
		pat9125data.otsCount = PIXEL_MIN;
		pat9125data.preCount = PIXEL_MIN;
	} else {
		return;
	}

report:
	update_slide_state(pat9125data.otsCount);
	pat9125data.preCount = pat9125data.otsCount;
	pat9125data.rawCount = pat9125data.otsCount;
	pat9125_report(RESET_FLAG, RESET_FLAG , 0, OTS_ROT_STOP);
	pat9125_report(pat9125data.otsCount, pat9125data.completeStatus, 0, OTS_ROT_STOP);
	pr_debug("%s : init otsCount = %d , mark_detect(%d) , hall(%d)\n"
		, __func__, pat9125data.otsCount, pat9125data.completeStatus, pat9125data.hallsensorStatus);
}

/*******************motor control*************************************/
static void pat9125_get_marking_status()
{
	if(ots_breakdown) {
		pat9125data.completeStatus = pat9125data.movingStatus + 2;
		black_shutter = -1;
		return;
	}
	OTS_Sensor_Find_MarkingPosition(&pat9125data.otsCount, &pat9125data.markPosition, &shutter, shutter_th);
	black_shutter = shutter;

	if(pat9125data.markPosition){
		pat9125data.completeStatus = pat9125data.movingStatus + 2;
	}else{
		pat9125data.completeStatus = pat9125data.movingStatus;
	}
}

int pat9125_set_motor_status(int pwm_info, int pps_info)
{
	int pre_position = set_position;

	if(pwm_info == DISABLE){
		pat9125data.movingStatus = DISABLE;
		check_stop_work = DISABLE;
	}else{
		set_motor_stop_th = (pps_info >= MOTOR_MAX_PPS) ? MONITORING_MOVING_FAST : MONITORING_MOVING_SLOW;
		pr_debug("%s : motor_max_pps = %d pps, stop count treshold = %d count\n", __func__, pps_info, set_motor_stop_th);

		pat9125data.movingStatus = ENABLE;
		pat9125data.forceMoveStatus = DISABLE;
		cancel_delayed_work_sync(&work_param);
		schedule_delayed_work(&work_param, set_start_interval);
	}

	pat9125_get_marking_status();
	if(set_motor_request == DISABLE && pwm_info == DISABLE)
		pat9125_report(pat9125data.targetCount, pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_STOP);
	else
		pat9125_report(pat9125data.syncCount, pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_NO_CHANGE);
	pr_debug("%s : otsCount = %d , motor_en(%d), marked (%d)\n", __func__, pat9125data.otsCount, pwm_info, pat9125data.completeStatus);

	if(pwm_info == DISABLE && pre_position == set_position) {
		pat9125_exception_handling(pre_position);
	}
	return 0;
}
EXPORT_SYMBOL(pat9125_set_motor_status);

static void pat9125_motor_request(int position, int force, int enable)
{
	char *uevent_message[3];
	int offset = PIXEL_CAL_OFFSET + (exception_moving * RESET_FLAG);

	if(!enable || !set_motor_request)	return;

	switch(position) {
		case TARGET_POSITION_DISABLE :
			if(pat9125data.movingStatus)
				set_motor_control(TARGET_POSITION_DISABLE);
			return;
		case TARGET_POSITION_COLLAPSE :
			if(pat9125data.otsCount > (PIXEL_MAX - offset))
				pat9125_report(PIXEL_MAX, pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_NO_CHANGE);
			else
				pat9125_report(pat9125data.otsCount + offset , pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_NO_CHANGE);
			break;
		case TARGET_POSITION_EXPAND :
			if(pat9125data.otsCount < (PIXEL_MIN + offset))
				pat9125_report(PIXEL_MIN, pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_NO_CHANGE);
			else
				pat9125_report(pat9125data.otsCount - offset , pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_NO_CHANGE);
			break;
		case TARGET_POSITION_HALF :
			break;
		default:
			return;
	}
	schedule_timeout_interruptible(20);

	uevent_message[0] = kasprintf(GFP_KERNEL, (force == ENABLE) ? "FORCEMOVING" : (position == set_position) ? "TRYAGAIN" : "MOTORBACK");
	uevent_message[1] = kasprintf(GFP_KERNEL, "POSITION=%d", position);
	uevent_message[2] = NULL;
	kobject_uevent_env(&pat9125data.pat9125_input_dev->dev.kobj, KOBJ_CHANGE, uevent_message);
	pr_debug("%s : %s , %s\n", __func__,uevent_message[0],uevent_message[1]);
	kfree(uevent_message[0]);
	kfree(uevent_message[1]);
}

static void pat9125_reverse_motor_position()
{
	pr_debug("%s \n", __func__);
	pat9125_motor_request(pre_position, DISABLE, set_motor_back);
}

static void pat9125_try_again_motor_position(int position)
{
	if(position != set_position)	return;

	pr_debug("%s \n", __func__);
	if((pat9125data.otsCount < MONITORING_COLLAPSE && set_position == TARGET_POSITION_COLLAPSE) || set_try_again == ENABLE){
		set_try_again = DISABLE;
		pat9125_reverse_motor_position();
		pre_position = set_position;
		return;
	}
	set_try_again = ENABLE;
	pat9125_motor_request(set_position, DISABLE, set_motor_back);
}

static void pat9125_auto_moving(int direction, int interval)
{
	if(pat9125data.markPosition && direction == TARGET_POSITION_COLLAPSE){
		if(pat9125data.hallsensorStatus == LGE_HALL_SENSOR_CLOSE){
			pat9125data.otsCount = PIXEL_MIN;
			pat9125_report(pat9125data.otsCount, pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_DOWN);
		}
		if(pat9125data.otsCount < PIXEL_MIN) {
			pat9125data.otsCount = PIXEL_MIN;
			pat9125_report(pat9125data.otsCount, pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_DOWN);
		}
	} else if(direction == TARGET_POSITION_EXPAND){
		if(pat9125data.otsCount > PIXEL_MAX){
			pat9125data.otsCount = PIXEL_MAX;
			pat9125_report(pat9125data.otsCount, pat9125data.completeStatus, pat9125data.targetCount, OTS_ROT_UP);
		}
	}
	pat9125data.preCount = pat9125data.otsCount;
	pat9125data.rawCount = pat9125data.otsCount;

	if(pat9125data.otsCount == PIXEL_MIN || pat9125data.otsCount == PIXEL_MAX)	return;

	if(interval >= MONITORING_MOVING_FORCE_DOWN){
		pr_debug("%s : force moving count = %d\n", __func__, interval);
		pat9125_motor_request(direction, ENABLE, set_motor_force_move);
	}

}

static void pat9125_mark_stop_time_offset(struct work_struct *work)
{
	pr_debug("%s : TARGET_POSITION %d mark detecting \n", __func__, pat9125data.markPosition);
	pat9125_motor_request(TARGET_POSITION_DISABLE, ENABLE, ENABLE);
}

static void pat9125_interval_check(struct work_struct *work)
{
	int interval_cnt = XCNT(pat9125data.otsCount - pat9125data.preCount);
	int interval_direc = XWAY(pat9125data.otsCount - pat9125data.preCount);
	int target_direc = XWAY(pat9125data.targetCount - pat9125data.startCount);
	int moving_cnt = XCNT(pat9125data.targetCount - pat9125data.otsCount);
	pat9125data.preCount = pat9125data.otsCount;

	pr_debug("%s : %d, %d, target_direc = %d, interval_direc = %d, otsCount = %d, interval_cnt == %d\n",
		__func__, shutter, shutter_th, target_direc, interval_direc, pat9125data.otsCount, interval_cnt);

	if(pat9125data.forceMoveStatus){
		pat9125_auto_moving(interval_direc, moving_cnt);
		return;
	}

	if(interval_cnt < set_motor_stop_th || (target_direc && interval_direc != target_direc)){
		pr_debug("%s : detected stop \n", __func__);

		pat9125data.forceMoveStatus = ENABLE;
		if(pat9125data.movingStatus){
			pat9125_motor_request(TARGET_POSITION_DISABLE, ENABLE, ENABLE);
		}
		return;
	}

	if(pat9125data.otsCount <= MONITORING_COLLAPSE && set_position == TARGET_POSITION_COLLAPSE){
		schedule_delayed_work(&work_param, set_end_interval);
	}else{
		schedule_delayed_work(&work_param, set_nomal_interval);
	}
}

static bool pat9125_is_exception_zone(int position)
{
	bool returnValue = false;
	int cal_count = pat9125data.otsCount;

	if(position == TARGET_POSITION_COLLAPSE){
		if(cal_count > (PIXEL_MIN + set_offset)){
			returnValue = true;
		}
	} else if(position == TARGET_POSITION_EXPAND){
		if(cal_count < (PIXEL_MAX - set_offset)){
			returnValue = true;
		}
	} else if(position == TARGET_POSITION_HALF){
		if((cal_count >= (PIXEL_HALF + set_offset))
			|| (cal_count <= (PIXEL_HALF - set_offset))){
			returnValue = true;
		}
	}

	if(position == pat9125data.markPosition){
		returnValue = false;
		if(calibration_factor_changed != calibration_factor && position == TARGET_POSITION_COLLAPSE){
			if(pat9125data.hallsensorStatus == LGE_HALL_SENSOR_CLOSE) {
				pr_debug("%s : apply calibration factor = %d\n", __func__, calibration_factor_changed);
				input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_RY, calibration_factor_changed);
				set_cal_factor = ENABLE;
			}
		}

		if(start_collapse == ENABLE && position == TARGET_POSITION_EXPAND && set_cal_factor == DISABLE) {
			if(pat9125data.otsCount < (PIXEL_MAX - RESET_FACTOR) || pat9125data.otsCount > (PIXEL_MAX + RESET_FACTOR)) {
				pr_debug("%s : changing the calibration factor = %d\n", __func__, calibration_factor_changed);
				calibration_factor_changed = pat9125data.rawCount;
			} else {
				set_cal_factor = ENABLE;
				pr_debug("%s : Calibration already completed. \n", __func__);
			}
		}
	}

	if(returnValue && pat9125data.markPosition){
		if(pat9125data.hallsensorStatus == LGE_HALL_SENSOR_CLOSE) {
			if(position == TARGET_POSITION_COLLAPSE){
				returnValue = false;
				pr_debug("%s : detect - collapse marking\n", __func__);
			}
		} else if(pat9125data.hallsensorStatus == LGE_HALL_SENSOR_OPEN) {
			int interval_cnt = XCNT(pat9125data.otsCount- pat9125data.startCount);
			if(start_collapse && interval_cnt < (pat9125data.targetCount - MONITORING_MOVING_INTERVAL)){
				return returnValue;
			}
			if(position == TARGET_POSITION_EXPAND){
				returnValue = false;
				pr_debug("%s : detect - unknown marking , target expand\n", __func__);
			} else if(position == TARGET_POSITION_HALF){
				returnValue = false;
				pr_debug("%s : detect - unknown marking , target half\n", __func__);
			}
		} else {
			returnValue = false;
			pr_debug("%s : detect - mark(no exist - hall sensor)\n", __func__);
		}
	}

	return returnValue;
}

static void pat9125_exception_handling(int16_t position)
{
	if(!set_motor_request)	return ;

	if(pat9125_is_exception_zone(position)){
		pr_debug("%s : is_exception_zone\n", __func__);
		pat9125_try_again_motor_position(position);
	} else {
		if(position != set_position)	return;
		pr_debug("%s : is_success_zone\n", __func__);

		pat9125_get_marking_status();
		if(pat9125data.markPosition || (pat9125data.hallsensorStatus == LGE_HALL_SENSOR_CLOSE && set_position == TARGET_POSITION_COLLAPSE)){
			pat9125_report(pat9125data.targetCount, pat9125data.markPosition ? MOT_STOP_DET_MARK : MOT_STOP_NON_MARK, 0, OTS_ROT_STOP);
		} else {
			pat9125_report(pat9125data.targetCount, MOT_STOP_NON_MARK, pat9125data.targetCount, OTS_ROT_NO_CHANGE);
		}
		pr_debug("%s : success count = %d , marked(%d), shutter(%d)\n", __func__, pat9125data.otsCount, pat9125data.completeStatus, shutter);
		update_slide_state(pat9125data.targetCount);
	}
}

/*********************************************************************/

void pixart_pat9125_ist(void)
{
	int16_t deltaX, deltaY;
	unsigned char OutTiltState;
	unsigned char OutOtsState;

	OTS_Sensor_ReadMotion(&deltaX, &deltaY);

	if(ots_breakdown) {
		set_motor_request = DISABLE;
		pr_debug(">>> %s (%d) :: free irq \n", __func__, __LINE__);
		free_irq(pat9125data.irq, &pat9125data);
		return;
	}

	if(deltaY == 0 || abs(deltaY) > 10){
		pr_debug(">>> %s (%d) :: delta = %d \n", __func__, __LINE__,deltaY);
		return;
	}

	OutOtsState = OTS_Detect_Rotation(deltaY);
	OutTiltState = OTS_Detect_Tilt(deltaX);

	if(pat9125data.irq == REV_A_IRQ){
		pat9125data.rawCount += (2 * deltaY);
	} else {
		pat9125data.rawCount -= (2 * deltaY);
	}
	pat9125data.otsCount  = (pat9125data.rawCount * PIXEL_MAX) / calibration_factor;
	//OTS_Sensor_Find_MarkingPosition(&pat9125data.otsCount, &pat9125data.markPosition, &shutter, shutter_th);
	pat9125_get_marking_status();

	if(logging_count_en || pat9125data.forceMoveStatus)
		pr_debug("%s : rawCount = %d, shutter = %d\n", __func__, pat9125data.rawCount, shutter);

	if(pat9125data.forceMoveStatus){
		//pat9125_get_marking_status();
		pat9125_report(pat9125data.otsCount, pat9125data.completeStatus, pat9125data.targetCount, OutOtsState);
		if(XCNT(pat9125data.targetCount - pat9125data.otsCount) >= MONITORING_MOVING_FORCE_DOWN ) {
			//cancel_delayed_work_sync(&work_param);
			schedule_delayed_work(&work_param, MONITORING_INTERVAL_FORCE);
		}
	}else{//mark stop
		pat9125_report(pat9125data.otsCount, pat9125data.movingStatus, pat9125data.targetCount, OutOtsState);
		if(set_mark_stop && set_position == pat9125data.markPosition && !check_stop_work){
			if(set_position == TARGET_POSITION_HALF){
				schedule_delayed_work(&work_stop, 0);
			}
			check_stop_work = ENABLE;
		}
		//white/black shutter monitoring
		if(!start_collapse) return;
		if((pat9125data.otsCount > (PIXEL_MIN + PIXEL_MARK_OFFSET) && pat9125data.otsCount < (PIXEL_HALF - PIXEL_MARK_OFFSET)) || 
			(pat9125data.otsCount > (PIXEL_HALF + PIXEL_MARK_OFFSET) && pat9125data.otsCount < (PIXEL_MAX - PIXEL_MARK_OFFSET))){
			if(wshutter < shutter){
				wshutter = shutter;
				white_shutter = wshutter;
				pr_debug("%s : white max-shutter mornitoring = %d\n", __func__, white_shutter);
			}
			if(set_cal_factor && shutter_th <= shutter && shutter < MKR_SHUTTER_TH_MAX){
				pr_debug("%s : update shutter threshold = %d\n", __func__, (shutter + MKR_SHUTTER_TH_OFFSET));
				input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_RX, (shutter + MKR_SHUTTER_TH_OFFSET));
			}
		}
	}
}

static irqreturn_t pixart_pat9125_irq(int irq, void *handle)
{
/* "cat /proc/kmsg" to see kernel message */
	__pm_stay_awake(g_otsWakelock);
	pixart_pat9125_ist();
	__pm_relax(g_otsWakelock);
	return IRQ_HANDLED;
}

static void pat9125_start()
{
	int err = (-1);

	OTS_Sensor_Init();
	if(!initFlag){
		pr_err("%s (%d) :initFlag = %d\n", __func__, __LINE__, initFlag);
		return;
	}

	err = request_threaded_irq(pat9125data.irq, NULL, pixart_pat9125_irq,
				   pat9125data.irq_flags | IRQF_ONESHOT | IRQF_TRIGGER_LOW | IRQF_NO_SUSPEND,
				   "pat9125_irq",
				   &pat9125data);
	if (err){
		pr_err("%s (%d) :irq %d busy? %d\n", __func__, __LINE__, pat9125data.irq, err);
	}else{
		pr_debug("%s (%d) :irq %d success\n", __func__, __LINE__, pat9125data.irq);
	}
	pat9125data.last_jiffies = jiffies_64;
}

static void pat9125_stop(void)
{
	pr_debug(">>> %s (%d)\n", __func__, __LINE__);
	free_irq(pat9125data.irq, &pat9125data);
}

//------------------------------------------------------------------------------------//
// dummy event -
//------------------------------------------------------------------------------------//
static void pat9125_set_position_work(struct work_struct *work)
{
	pr_debug("%s : set_position = %d\n", __func__, set_position);
	input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Y, MOT_MOVE_NON_MARK);
	input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Z, pat9125data.targetCount);
	input_sync(pat9125data.pat9125_input_dev);

	schedule_timeout_interruptible(300);
	input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_X, pat9125data.targetCount);
	input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Y, MOT_STOP_DET_MARK);
	input_sync(pat9125data.pat9125_input_dev);

	pr_info("%s : Leave \n", __func__);
}

//------------------------------------------------------------------------------------//
//sys/module/pat9125_linux_driver/
//------------------------------------------------------------------------------------//
static int pat9125_set_position_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	cancel_delayed_work_sync(&work_param);

	pat9125data.startCount = pat9125data.otsCount;
	if(pat9125data.hallsensorStatus == LGE_HALL_SENSOR_CLOSE && pat9125data.syncCount == PIXEL_MIN && pat9125data.markPosition > NON_DETECT){
		start_collapse = ENABLE;
		pr_debug("%s : start with COLLAPSE \n", __func__);
	}

	/*control by user*/
	if(set_position == TARGET_POSITION_DISABLE){
		exception_moving += ENABLE;
		pr_debug("%s : Motor Stop Request :: ots Count = %d\n", __func__, pat9125data.otsCount);
	} else {
		pat9125data.targetCount = pat9125_position_to_count(set_position);
		pr_debug("%s : start Count = %d, target Count = %d\n", __func__, pat9125data.startCount, pat9125data.targetCount);
	}

	/*for dummy event*/
	if(!initFlag || set_dummy_enable){
		ots_breakdown = ENABLE;
		set_motor_request = DISABLE;
		//schedule_delayed_work(&work, 0);
	}

	return ret;
}
static int pat9125_set_offset_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_offset %d\n", __func__, set_offset);
	return ret;
}
static int pat9125_set_interval_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_interval %d\n", __func__, set_interval);
	return ret;
}
static int pat9125_set_nomal_interval_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_nomal_interval %d\n", __func__, set_nomal_interval);
	return ret;
}
static int pat9125_set_start_interval_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_start_interval %d\n", __func__, set_start_interval);
	return ret;
}
static int pat9125_set_end_interval_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_end_interval %d\n", __func__, set_end_interval);
	return ret;
}
static int pat9125_set_force_interval_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_force_interval %d\n", __func__, set_force_interval);
	return ret;
}
static int pat9125_set_motor_request_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_motor_request %d\n", __func__, set_motor_request);
	return ret;
}
static int pat9125_set_mark_stop_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_mark_stop %d\n", __func__, set_mark_stop);
	return ret;
}
static int pat9125_set_motor_back_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_motor_back %d\n", __func__, set_motor_back);
	return ret;
}
static int pat9125_set_motor_force_move_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_motor_force_move %d\n", __func__, set_motor_force_move);
	return ret;
}
static int pat9125_set_motor_stop_th_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_motor_stop_th %d\n", __func__, set_motor_stop_th);
	return ret;
}
static int pat9125_set_force_move_up_th_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_force_move_up_th %d\n", __func__, set_force_move_up_th);
	return ret;
}
static int pat9125_set_force_move_down_th_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_force_move_down_th %d\n", __func__, set_force_move_down_th);
	return ret;
}
static int pat9125_set_dummy_enable_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	if(set_dummy_enable != ENABLE)	return ret;
	pat9125data.otsCount = PIXEL_MIN;
	pat9125data.preCount = PIXEL_MIN;
	pat9125data.rawCount = PIXEL_MIN;
	pat9125_report(pat9125data.otsCount, MOT_STOP_DET_MARK, 0, OTS_ROT_STOP);
	pr_debug("%s : set_dummy_enable = %d, dumy otsCount = %d \n", __func__, set_dummy_enable, pat9125data.otsCount);
	return ret;
}
static int pat9125_set_ots_count_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pat9125data.otsCount = (set_ots_count - SLIDE_COLLAPSE);
	pat9125data.rawCount = (set_ots_count - SLIDE_COLLAPSE);
	pat9125data.preCount = (set_ots_count - SLIDE_COLLAPSE);
	pat9125_get_marking_status();
	input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_X, pat9125data.otsCount);
	input_event(pat9125data.pat9125_input_dev, EV_ABS, ABS_Y, pat9125data.completeStatus);
	input_sync(pat9125data.pat9125_input_dev);
	pr_debug("%s : set_ots_count %d\n", __func__, set_ots_count);
	return ret;
}
static int pat9125_set_stop_offset_close_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_stop_offset_close %d\n", __func__, set_stop_offset_close);
	return ret;
}
static int pat9125_set_stop_offset_open_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : set_stop_offset_open %d\n", __func__, set_stop_offset_open);
	return ret;
}
static int pat9125_black_shutter_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	if(!ots_breakdown)
		black_shutter = ReadData(0x14);
	else
		black_shutter = -1;
	pr_debug("%s : black_shutter %d\n", __func__, black_shutter);
	return ret;
}
static int pat9125_white_shutter_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	white_shutter = wshutter;
	pr_debug("%s : white_shutter %d\n", __func__, white_shutter);
	return ret;
}
static int pat9125_slide_state_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : slide_state %d\n", __func__, slide_state);
	return ret;
}
static int pat9125_recovery_mode_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : recovery_mode %d\n", __func__, recovery_mode);
	return ret;
}
static int pat9125_logging_count_en_cb(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val,kp);
	pr_debug("%s : logging_count_en %d\n", __func__, logging_count_en);
	return ret;
}
module_param_call(set_position, pat9125_set_position_cb, param_get_int, &set_position, 0664);
module_param_call(set_offset, pat9125_set_offset_cb, param_get_int, &set_offset, 0664);
module_param_call(set_interval, pat9125_set_interval_cb, param_get_int, &set_interval, 0664);
module_param_call(set_nomal_interval, pat9125_set_nomal_interval_cb, param_get_int, &set_nomal_interval, 0664);
module_param_call(set_start_interval, pat9125_set_start_interval_cb, param_get_int, &set_start_interval, 0664);
module_param_call(set_end_interval, pat9125_set_end_interval_cb, param_get_int, &set_end_interval, 0664);
module_param_call(set_force_interval, pat9125_set_force_interval_cb, param_get_int, &set_force_interval, 0664);
module_param_call(set_motor_request, pat9125_set_motor_request_cb, param_get_int, &set_motor_request, 0664);
module_param_call(set_mark_stop, pat9125_set_mark_stop_cb, param_get_int, &set_mark_stop, 0664);
module_param_call(set_motor_back, pat9125_set_motor_back_cb, param_get_int, &set_motor_back, 0664);
module_param_call(set_motor_force_move, pat9125_set_motor_force_move_cb, param_get_int, &set_motor_force_move, 0664);
module_param_call(set_motor_stop_th, pat9125_set_motor_stop_th_cb, param_get_int, &set_motor_stop_th, 0664);
module_param_call(set_force_move_up_th, pat9125_set_force_move_up_th_cb, param_get_int, &set_force_move_up_th, 0664);
module_param_call(set_force_move_down_th, pat9125_set_force_move_down_th_cb, param_get_int, &set_force_move_down_th, 0664);
module_param_call(set_dummy_enable, pat9125_set_dummy_enable_cb, param_get_int, &set_dummy_enable, 0664);
module_param_call(set_ots_count, pat9125_set_ots_count_cb, param_get_int, &set_ots_count, 0664);
module_param_call(set_stop_offset_close, pat9125_set_stop_offset_close_cb, param_get_int, &set_stop_offset_close, 0664);
module_param_call(set_stop_offset_open, pat9125_set_stop_offset_open_cb, param_get_int, &set_stop_offset_open, 0664);
module_param_call(black_shutter, pat9125_black_shutter_cb, param_get_int, &black_shutter, 0664);
module_param_call(white_shutter, pat9125_white_shutter_cb, param_get_int, &white_shutter, 0664);
module_param_call(slide_state, pat9125_slide_state_cb, param_get_int, &slide_state, 0664);
module_param_call(recovery_mode, pat9125_recovery_mode_cb, param_get_int, &recovery_mode, 0664);
module_param_call(logging_count_en, pat9125_logging_count_en_cb, param_get_int, &logging_count_en, 0664);
//------------------------------------------------------------------------------------//
//sys/devices/virtual/input/pat9125/onoff
//------------------------------------------------------------------------------------//
static ssize_t pat9125_store_otsonoff(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	pr_debug("%s (%d) : %s\n", __func__,__LINE__,buf);
	kstrtoint(buf, 10, &value);

	if(value == ENABLE_OTS_SENSOR){
		if(shutter_th != MKR_SHUTTER_TH_ECHO)
			shutter_th = pat9125_read_file(SHUTTER_TH_PATH);
		if(shutter_th == 0)
			shutter_th = MKR_SHUTTER_TH_MIN;

		calibration_factor = pat9125_read_file(CAL_FACTOR_PATH);
		if(calibration_factor == 0)
			calibration_factor = PIXEL_MAX;
		else
			set_offset = PIXEL_MIN_OFFSET;
		calibration_factor_changed = calibration_factor;
	}
	pat9125_access_read_position(value);
	return count;
}

static ssize_t pat9125_store_resol(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char s[256];
	char *p = s;

	pr_debug("%s (%d)\n", __func__, __LINE__);

	memcpy(s, buf, sizeof(s));

	*(s+1) = '\0';
	*(s+4) = '\0';
	*(s+7) = '\0';
	/* example(in console): echo w oe 48> resol */
	if (*p == 'w') {
		long write_addr, write_data;
		p += 2;
		if (!kstrtol(p, 16, &write_addr)) {
			p += 3;
			if (!kstrtol(p, 16, &write_data)) {
				pr_debug(
					"w 0x%x 0x%x\n",
					(u8)write_addr, (u8)write_data);
				WriteData((u8)write_addr, (u8)write_data);
			}
		}
		/* example(in console): echo r 14 > resol */
	}	else if (*p == 'r')	{
		long read_addr;
		p += 2;

		if (!kstrtol(p, 16, &read_addr)) {
			int data = 0;
			data = ReadData((u8)read_addr);
			pr_debug(
				"%s (%d) r 0x%x 0x%x\n", __func__, __LINE__,
				(unsigned int)read_addr, data);
		}
	}
	return count;
}

static ssize_t pat9125_store_shutter(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	pr_debug("%s (%d) : %s\n", __func__,__LINE__,buf);
	kstrtoint(buf, 10, &value);
	shutter_th = value;
	pat9125_write_file(SHUTTER_TH_PATH, value);

	return count;
}

static ssize_t pat9125_store_calfactor(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	pr_debug("%s (%d) : %s\n", __func__,__LINE__,buf);
	kstrtoint(buf, 10, &value);
	calibration_factor = value;
	pat9125_write_file(CAL_FACTOR_PATH, value);

	return count;
}

static ssize_t pat9125_show_shutter(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int value = 0;
	value = black_shutter;
	pr_debug("%s (%d) : %d\n", __func__, __LINE__, value);

	return snprintf(buf, PAGE_SIZE, "%d\n", value);
}

static ssize_t pat9125_show_wshutter(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int value = 0;
	value = wshutter;
	pr_debug("%s (%d) : %d\n", __func__, __LINE__, value);

	return snprintf(buf, PAGE_SIZE, "%d\n", value);
}

static DEVICE_ATTR(onoff, 0664 , NULL, pat9125_store_otsonoff);
static DEVICE_ATTR(resol, 0664 , NULL, pat9125_store_resol);
static DEVICE_ATTR(shutter, 0664 , NULL, pat9125_store_shutter);
static DEVICE_ATTR(calfactor, 0664 , NULL, pat9125_store_calfactor);
static DEVICE_ATTR(mshutter, 0664 , pat9125_show_shutter, NULL);
static DEVICE_ATTR(wshutter, 0664 , pat9125_show_wshutter, NULL);

static struct attribute *pat9125_attr_list[] = {
	&dev_attr_onoff.attr,
	&dev_attr_resol.attr,
	&dev_attr_shutter.attr,
	&dev_attr_calfactor.attr,
	&dev_attr_mshutter.attr,
	&dev_attr_wshutter.attr,
	NULL,
};

static struct attribute_group pat9125_attr_group = {
	.attrs = pat9125_attr_list,
};

static int pat9125_create_attr(struct input_dev *dev)
{
	int err = 0;
	pr_debug("%s (%d) :\n", __func__, __LINE__);
	if (!dev){
		pr_debug("fail find device file %s (%d) :\n", __func__, __LINE__);
		return -EINVAL;
	}

	err = sysfs_create_group(&pat9125data.pat9125_input_dev->dev.kobj, &pat9125_attr_group);
	if (err)
		pr_debug("%s (%d) : sysfs creat fail!!\n", __func__, __LINE__);

	return err;
}

static int pat9125_delete_attr(struct input_dev *dev)
{
	pr_debug("%s (%d) :\n", __func__, __LINE__);
	if (!dev){
		pr_debug("fail find device file %s (%d) :\n", __func__, __LINE__);
		return -EINVAL;
	}

	sysfs_remove_group(&pat9125data.pat9125_input_dev->dev.kobj, &pat9125_attr_group);

	return 0;
}

/*----------------------------------------------------------------------------*/

static int pat9125_hallic_notifier_callback(struct notifier_block *this, unsigned long action, void *data)
{
	struct pat9125_linux_data_t *ots_data = container_of(this, struct pat9125_linux_data_t, hallic_notifier);
	static const int SLIDE = LGE_SLIDE_DETECT;
	struct lge_panel_notifier *event = data;
	int *event_data = &event->state;
	
	if (action != SLIDE) {
		return 0;
	}

	switch (*event_data) {
		case LGE_SLIDE_CLOSED:
			ots_data->hallicStatus = LGE_SLIDE_CLOSED;
			break;
		case LGE_SLIDE_OPEN:
			ots_data->hallicStatus = LGE_SLIDE_OPEN;
			break;
		default:
			pr_debug("%s : unknown event (%lu)\n", __func__, event);
			return 0;
	}

	pr_debug("%s : hallic_status(%d)\n", __func__, pat9125data.hallicStatus);
	return 0;
}

static int pat9125_hallsensor_notifier_callback(struct notifier_block *this, unsigned long action, void *data)
{
	struct pat9125_linux_data_t *ots_data = container_of(this, struct pat9125_linux_data_t, hallsensor_notifier);
	static const int SLIDE = LGE_HALL_SENSOR_STATE_CHANGED;
	struct lge_hall_sensor_notifier *event = data;
	int *event_data = &event->state;

	if (action != SLIDE) {
		return 0;
	}

	switch (*event_data) {
		case LGE_HALL_SENSOR_CLOSE:
			ots_data->hallsensorStatus = LGE_HALL_SENSOR_CLOSE;
			break;
		case LGE_HALL_SENSOR_OPEN:
			ots_data->hallsensorStatus = LGE_HALL_SENSOR_OPEN;
			break;
		default:
			pr_debug("%s : unknown event (%lu)\n", __func__, event);
			break;
	}

	pr_debug("%s : hallsensor_status(%d)\n", __func__, pat9125data.hallsensorStatus);
	return 0;
}

static int pat9125_init_notify(void)
{
	int ret = 0;
	pr_debug("%s\n", __func__);

	pat9125data.hallic_notifier.notifier_call = pat9125_hallic_notifier_callback;
	ret = lge_panel_notifier_register_client(&pat9125data.hallic_notifier);
	if (ret < 0)
		pr_debug("%s (%d) failed to regiseter lge_panel_notify callback\n", __func__, __LINE__);

	return ret;
}

static int pat9125_init_hall_sensor_notify(void)
{
	int ret = 0;
	pr_debug("%s\n", __func__);

	pat9125data.hallsensor_notifier.notifier_call = pat9125_hallsensor_notifier_callback;
	ret = lge_hall_sensor_notifier_register_client(&pat9125data.hallsensor_notifier);
	if (ret < 0)
		pr_debug("%s (%d) failed to regiseter lge_panel_notify callback\n", __func__, __LINE__);

	return ret;
}
/*----------------------------------------------------------------------------*/

static int pat9125_i2c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int err = 0;
	struct device_node *np;

	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	pr_debug("%s (%d) : probe module....\n", __func__, __LINE__);

	memset(&pat9125data, 0, sizeof(pat9125data));
	err = i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE);
	if (err < 0)
		goto error_return;

	pat9125data.client = client;

	/*register input driver*/
	if (pat9125_init_input_data() < 0)
		goto error_return;

	/*create sysfs*/
	err = pat9125_create_attr(pat9125data.pat9125_input_dev);
	if (err) {
		pr_debug("pat9125 create attribute err = %d\n", err);
		goto error_return;
	}

	pat9125data.i2c_dev = &client->dev;
	np = pat9125data.i2c_dev->of_node;

	/* 3.3v supply */
	if(of_get_property(np, "vdd-supply", NULL)){
		pat9125data.vdd = devm_regulator_get(pat9125data.i2c_dev, "vdd");
		if(IS_ERR(pat9125data.vdd)){
			pr_debug("Failed to get pat9125 vdd supply\n");
			goto error_return;
		}else{
			regulator_enable(pat9125data.vdd);
		}
	}

	/* enable hall ic */
	if(of_get_property(np, "hallic", NULL)){
		pat9125data.hallicEn = ENABLE;
		pr_debug("%s (%d) : enable hall ic\n", __func__, __LINE__);
		pat9125_init_notify();
	}else{
		pat9125data.hallicEn = DISABLE;
		pr_debug("%s (%d) : disable hall ic\n", __func__, __LINE__);
	}

	/* enable hall sensor */
	if(of_get_property(np, "hallsensor", NULL)){
		pat9125data.hallsensorEn = ENABLE;
		pr_debug("%s (%d) : enable hall sensor\n", __func__, __LINE__);
		pat9125_init_hall_sensor_notify();
	}else{
		pat9125data.hallsensorEn = DISABLE;
		pr_debug("%s (%d) : disable hall sensor\n", __func__, __LINE__);
	}

	/* set mark detect threshold */
	if(of_get_property(np, "markthreshold", NULL)){
		shutter_th = MKR_SHUTTER_TH_ECHO;
		pr_debug("%s (%d) : detect threshold = %d \n", __func__, __LINE__, MKR_SHUTTER_TH_ECHO);
	}else{
		shutter_th = MKR_SHUTTER_TH_MIN;
	}

	/* interrupt initialization */
	pat9125data.irq_gpio = of_get_named_gpio_flags(np,
			"pixart,irq_gpio", 0, &pat9125data.irq_flags);

	pr_debug(
		"irq_gpio: %d, irq_flags: 0x%x\n",
		pat9125data.irq_gpio, pat9125data.irq_flags);

	if (!gpio_is_valid(pat9125data.irq_gpio)) {
		err = (-1);
		pr_debug(
			"invalid irq_gpio: %d\n",
			pat9125data.irq_gpio);
		goto error_return;
	}

	err = gpio_request(pat9125data.irq_gpio, "pat9125_irq_gpio");
	if (err) {
		pr_debug(
			"unable to request gpio [%d], [%d]\n",
			pat9125data.irq_gpio, err);
		goto error_return;
	}

	err = gpio_direction_input(pat9125data.irq_gpio);
	if (err){
		pr_debug("unable to set dir for gpio[%d], [%d]\n",
		pat9125data.irq_gpio, err);
		goto error_return;
	}

	pat9125data.irq = gpio_to_irq(pat9125data.irq_gpio);

	pat9125_start();
	pat9125data.preCount = PIXEL_MIN;
	pat9125data.otsCount = PIXEL_MIN;
	pat9125data.syncCount = PIXEL_MIN;
	pat9125data.startCount = PIXEL_MIN;
	pat9125data.targetCount = PIXEL_MIN;
	pat9125data.markPosition = NON_DETECT;
	pat9125data.completeStatus = MOT_STOP_NON_MARK;
	pat9125data.movingStatus = DISABLE;
	pat9125data.forceMoveStatus = ENABLE;
	pat9125data.hallicStatus = LGE_SLIDE_CLOSED;
	pat9125data.hallsensorStatus = lge_hall_sensor_state;

	return 0;

error_return:
	return err;
}

static int pat9125_i2c_remove(struct i2c_client *client)
{
	 pr_debug("%s (%d) \n", __func__, __LINE__);
	return 0;
}

static int pat9125_suspend(struct device *dev)
{
	pr_debug("%s (%d) \n", __func__, __LINE__);
	enable_irq_wake(pat9125data.irq);
	return 0;
}

static int pat9125_resume(struct device *dev)
{
	pr_debug("%s (%d) \n", __func__, __LINE__);
	disable_irq_wake(pat9125data.irq);
	return 0;
}

static const struct i2c_device_id pat9125_device_id[] = {
	{pat9125_DEV_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, pat9125_device_id);

static const struct dev_pm_ops pat9125_pm_ops = {
	.suspend = pat9125_suspend,
	.resume = pat9125_resume
};

static struct of_device_id pixart_pat9125_match_table[] = {
	{ .compatible = "pixart,pat9125",},
	{ },
};

static struct i2c_driver pat9125_i2c_driver = {
	.driver = {
		   .name = pat9125_DEV_NAME,
		   .owner = THIS_MODULE,
		   .pm = &pat9125_pm_ops,
		   .of_match_table = pixart_pat9125_match_table,
		   },
	.probe = pat9125_i2c_probe,
	.remove = pat9125_i2c_remove,
	.id_table = pat9125_device_id,
};

static int pat9125_open(struct input_dev *dev)
{
	pr_debug("%s (%d)\n", __func__, __LINE__);
	return 0;
}

static void pat9125_close(struct input_dev *dev)
{
	 pr_debug("%s (%d)\n", __func__, __LINE__);
}

static int pat9125_init_input_data(void)
{
	int ret = 0;

	 pr_debug("%s (%d) : initialize data\n", __func__, __LINE__);

	pat9125data.pat9125_input_dev = input_allocate_device();

	if (!pat9125data.pat9125_input_dev) {
			pr_debug(
			"%s (%d) : could not allocate input device\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	__set_bit(EV_ABS, pat9125data.pat9125_input_dev->evbit);
	__set_bit(ABS_X, pat9125data.pat9125_input_dev->absbit);
	__set_bit(ABS_Y, pat9125data.pat9125_input_dev->absbit);
	__set_bit(ABS_Z, pat9125data.pat9125_input_dev->absbit);
	__set_bit(ABS_RX, pat9125data.pat9125_input_dev->absbit);
	__set_bit(ABS_RY, pat9125data.pat9125_input_dev->absbit);
	__set_bit(ABS_RZ, pat9125data.pat9125_input_dev->absbit);

	input_set_abs_params(pat9125data.pat9125_input_dev,ABS_X,0,1000,0,0);
	input_set_abs_params(pat9125data.pat9125_input_dev,ABS_Y,0,1000,0,0);
	input_set_abs_params(pat9125data.pat9125_input_dev,ABS_Z,0,1000,0,0);
	input_set_abs_params(pat9125data.pat9125_input_dev,ABS_RX,0,1000,0,0);
	input_set_abs_params(pat9125data.pat9125_input_dev,ABS_RY,0,1000,0,0);
	input_set_abs_params(pat9125data.pat9125_input_dev,ABS_RZ,0,1000,0,0);

	pat9125data.pat9125_input_dev->name = pat9125_DEV_NAME;
	pat9125data.pat9125_input_dev->dev.init_name = pat9125_DEV_NAME;
	pat9125data.pat9125_input_dev->id.bustype = BUS_I2C;
	pat9125data.pat9125_input_dev->open = pat9125_open;
	pat9125data.pat9125_input_dev->close = pat9125_close;

	input_set_drvdata(pat9125data.pat9125_input_dev, &pat9125data);

	ret = input_register_device(pat9125data.pat9125_input_dev);
	if (ret < 0) {
		input_free_device(pat9125data.pat9125_input_dev);
		 pr_debug(
			"%s (%d) : could not register input device\n",
			__func__, __LINE__);
		return ret;
	}

	return 0;
}

static int __init pat9125_linux_init(void)
{
	pr_debug("%s (%d) :init module\n", __func__, __LINE__);
	i2c_add_driver(&pat9125_i2c_driver);
	g_otsWakelock = wakeup_source_register(NULL,"otsdrv");
	return 0;
}

static void __exit pat9125_linux_exit(void)
{
	pr_debug("%s (%d) : exit module\n", __func__, __LINE__);
	pat9125_stop();
	pat9125_delete_attr(pat9125data.pat9125_input_dev);
	i2c_del_driver(&pat9125_i2c_driver);
	cancel_delayed_work_sync(&work);
	cancel_delayed_work_sync(&work_param);
	cancel_delayed_work_sync(&work_stop);
	wakeup_source_unregister(g_otsWakelock);
}

module_init(pat9125_linux_init);
module_exit(pat9125_linux_exit);
MODULE_AUTHOR("pixart");
MODULE_DESCRIPTION("pixart pat9125 driver");
MODULE_LICENSE("GPL");
