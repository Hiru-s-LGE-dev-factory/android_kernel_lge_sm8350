/* Driver for PEN Driver V2 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/async.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <touch_core.h>

u32 pen_debug_mask = 0; //0xFFF;
static const char __used *pen_flow[11] = {
	[0] = "<NONE>      ",
	[1] = "<HoverEnter>",
	[2] = "<HoverExit> ",
	[3] = "<Press>     ",
	[4] = "<Release>   ",
	[5] = "<Button1_P> ",
	[6] = "<Button1_R> ",
	[7] = "<Button2_P> ",
	[8] = "<Button2_R> ",
	[9] = "<Debug(1)>  ",
	[10] = "<Debug(2)> ",
};

void prediction_filter(struct touch_core_data *ts) {

	u32 DELTA_DEGREE_THRESHOLD = 0;
	u16 VELOCITY_THRESHOLD = 0;

	u16 K = 0;
	int x = ts->pdata.x;
	int y = ts->pdata.y;
	int dx = x - ts->pp.prev_x;
	int dy = y - ts->pp.prev_y;
	int distance = 0;
	int x_est = 0;
	int y_est = 0;
	int z_measured_x = 0;
	int z_measured_y =0;
	int currUnitX = 0;
	int currUnitY = 0;
	int deltaDegree = 0;
	int currVelocityX = dx*1000 / VSYNC_TIMING;
	int currVelocityY = dy*1000 / VSYNC_TIMING;

	if((ts->pdata.in_range == 0) || (ts->pdata.contact == 0)) {
		ts->pp.prev_predict_x = 0;
		ts->pp.prev_predict_y = 0;
		ts->pp.predict_x = 0;
		ts->pp.predict_y = 0;
		ts->pp.prev_est_x = 0;
		ts->pp.prev_est_y = 0;
		ts->pp.prev_unit_x = 0;
		ts->pp.prev_unit_y = 0;
		ts->pp.prev_x = 0;
		ts->pp.prev_y = 0;
		ts->pp.move_cnt = 0;
		ts->pp.predict_ratio = 0;
		return;
	}

	distance  = int_sqrt((dx*dx+dy*dy)*100); // *10
	if (distance != 0) {
		currUnitX = (dx*10000)/distance; // *1000
		currUnitY = (dy*10000)/distance; // *1000
	} else {
		currUnitX = 0;
		currUnitY = 0;
	}
	distance = distance*INCHTOMILLI/DPI;  // convert inch to mm / dpi => mm/s

	deltaDegree = (int)(currUnitX*ts->pp.prev_unit_x + currUnitY*ts->pp.prev_unit_y);

	//measure
	z_measured_x = currVelocityX;
	z_measured_y = currVelocityY;

	//correct
	K = 10;   // *100
	x_est = (ts->pp.prev_est_x*100 + K * (z_measured_x - ts->pp.prev_est_x))/100;
	y_est = (ts->pp.prev_est_y*100 + K * (z_measured_y - ts->pp.prev_est_y))/100;

	//prediction
	if ( MOVE_CHK_CNT < ts->pp.move_cnt ) {
		DELTA_DEGREE_THRESHOLD = DELTA_DEGREE_LOW_THRESHOLD;
		VELOCITY_THRESHOLD = VELOCITY_LOW_THRESHOLD;
	} else {
		DELTA_DEGREE_THRESHOLD = DELTA_DEGREE_HIGH_THRESHOLD;
		VELOCITY_THRESHOLD = VELOCITY_HIGH_THRESHOLD;
	}

	if ((deltaDegree > DELTA_DEGREE_THRESHOLD) && VELOCITY_THRESHOLD < distance) {
		ts->pp.move_cnt = ts->pp.move_cnt + 1;
		if ( MOVE_CHK_CNT < ts->pp.move_cnt ) {
			if (ts->pp.predict_ratio <= 100) {
				ts->pp.predict_ratio += 20;
			}
			ts->pp.predict_x = (int)(VSYNC_TIMING * x_est / 1000 * ts->pp.predict_ratio / 100);
			ts->pp.predict_y = (int)(VSYNC_TIMING * y_est / 1000 * ts->pp.predict_ratio / 100);
			ts->pp.predict_x = (ts->pp.prev_predict_x*100 + K * (ts->pp.predict_x - ts->pp.prev_predict_x))/100;
			ts->pp.predict_y = (ts->pp.prev_predict_y*100 + K * (ts->pp.predict_y - ts->pp.prev_predict_y))/100;
		} else if ((ts->pp.predict_x != 0 || ts->pp.predict_y != 0 ) && 2 <= ts->pp.predict_ratio) {
			ts->pp.predict_ratio -= 1;
			ts->pp.predict_x = ts->pp.predict_x * ts->pp.predict_ratio / 100;
			ts->pp.predict_y = ts->pp.predict_y * ts->pp.predict_ratio / 100;
			ts->pp.predict_x = (ts->pp.prev_predict_x*100 + K * (ts->pp.predict_x - ts->pp.prev_predict_x))/100;
			ts->pp.predict_y = (ts->pp.prev_predict_y*100 + K * (ts->pp.predict_y - ts->pp.prev_predict_y))/100;
			TOUCH_D(ABS, "Pen Predict blue1 [%4d,%4d] [%4d,%4d] [%4d,%4d] [%4d,%4d]\n", ts->pp.prev_x, ts->pp.prev_y,  x, y, ts->pp.prev_predict_x,  ts->pp.prev_predict_y, ts->pp.predict_x,  ts->pp.predict_y);
		} else {
			ts->pp.predict_x = 0;
			ts->pp.predict_y = 0;
			ts->pp.predict_ratio = 0;
		}
	}
	else {
		if ((ts->pp.predict_x != 0 || ts->pp.predict_y != 0) && MOVE_CHK_CNT < ts->pp.move_cnt) {
			ts->pp.predict_x = (ts->pp.prev_x + ts->pp.predict_x) - x;
			ts->pp.predict_y = (ts->pp.prev_y + ts->pp.predict_y) - y;
			ts->pp.predict_ratio = 100;
			TOUCH_D(ABS, "Pen Predict black [%4d,%4d] [%4d,%4d] [%4d,%4d] [%4d,%4d]\n", ts->pp.prev_x, ts->pp.prev_y,  x, y, ts->pp.prev_predict_x,  ts->pp.prev_predict_y, ts->pp.predict_x,  ts->pp.predict_y);
		} else if ((ts->pp.predict_x != 0 || ts->pp.predict_y != 0 ) && 2 <= ts->pp.predict_ratio) {
			TOUCH_D(ABS, "Pen Predict blue2 [%4d,%4d] [%4d,%4d] [%4d,%4d] [%4d,%4d]\n", ts->pp.prev_x, ts->pp.prev_y,  x, y, ts->pp.prev_predict_x,  ts->pp.prev_predict_y, ts->pp.predict_x,  ts->pp.predict_y);
			ts->pp.predict_ratio -= 1;
			ts->pp.predict_x = ts->pp.predict_x * ts->pp.predict_ratio / 100;
			ts->pp.predict_y = ts->pp.predict_y * ts->pp.predict_ratio / 100;
			ts->pp.predict_x = (ts->pp.prev_predict_x*100 + K * (ts->pp.predict_x - ts->pp.prev_predict_x))/100;
			ts->pp.predict_y = (ts->pp.prev_predict_y*100 + K * (ts->pp.predict_y - ts->pp.prev_predict_y))/100;
		} else {
			ts->pp.predict_x = 0;
			ts->pp.predict_y = 0;
			ts->pp.predict_ratio = 0;
		}
		x_est = 0;
		y_est = 0;
		ts->pp.move_cnt = 0;
	}
//	if (pen_debug_mask)
		TOUCH_D(ABS, "Pen Predict [%4d,%4d] [%4d,%4d], est [%4d,%4d], [%4d,%8d], move_cnt [%d]\n",
				ts->pdata.x,
				ts->pdata.y,
				ts->pp.predict_x,
				ts->pp.predict_y,
				x_est,
				y_est,
				distance,
				deltaDegree,
				ts->pp.move_cnt);
	x += ts->pp.predict_x;
	y += ts->pp.predict_y;

	if (x < 0)
		ts->pdata.x = 0;
	else if (x > ts->caps.max_x)
		ts->pdata.x = ts->caps.max_x;
	else
		ts->pdata.x = x;

	if (y < 0)
		ts->pdata.y = 0;
	else if (y > ts->caps.max_y)
		ts->pdata.y = ts->caps.max_y;
	else
		ts->pdata.y = y;

	//update
	ts->pp.prev_est_x = x_est;
	ts->pp.prev_est_y = y_est;
	ts->pp.prev_x = x - ts->pp.predict_x;
	ts->pp.prev_y = y - ts->pp.predict_y;
	ts->pp.prev_unit_x = currUnitX;
	ts->pp.prev_unit_y = currUnitY;
	ts->pp.prev_predict_x = ts->pp.predict_x;
	ts->pp.prev_predict_y = ts->pp.predict_y;
}

void pen_log(int pen_flow_num, u64 id, u16 in_range, u16 contact, u16 swit1, u16 swit2, u16 batt,
	u16 x, u16 y, u16 pressure, u16 tilt, u16 azimuth)
{
	int val = pen_debug_mask;

	if(!val)
		return;

	printk("[Touch] %s", pen_flow[pen_flow_num]);

	if(val & 0x1)
		printk("id:%llu", id);
	if((val>>1) & 0x1)
		printk("in_r:%2d cont:%d", in_range, contact);
	if((val>>2) & 0x1)
		printk("sw1:%2d sw2:%d", swit1, swit2);
	if((val>>3) & 0x1)
		printk("bat:%2d", batt);
	if((val>>4) & 0x1)
		printk("x:%4d y:%4d", x, y);
	if((val>>5) & 0x1)
		printk("z:%4d", pressure);
	if((val>>6) & 0x1)
		printk("tt:%2d", tilt);
	if((val>>7) & 0x1)
		printk("am:%2d", azimuth);
	printk("\n");
}
void pen_input_handler(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int pen_flow_num = PEN_NONE;
	int btn_status = PEN_NONE;
	int active_pen_state = 0;

	TOUCH_TRACE();

	active_pen_state = atomic_read(&ts->state.active_pen);
	if(ts->pred_filter)
		prediction_filter(ts);

	if ((ts->pdata.in_range == 0) && (ts->pdata.contact == 0)) {	//<release>
		pen_flow_num = HOVER_EXIT;
		input_report_key(ts->input_pen, BTN_TOUCH, 0);
		input_report_key(ts->input_pen, BTN_TOOL_PEN, 0);
		input_report_abs(ts->input_pen, ABS_X, ts->pdata.x);
		input_report_abs(ts->input_pen, ABS_Y, ts->pdata.y);
		input_report_abs(ts->input_pen, ABS_PRESSURE, ts->pdata.pressure);
		input_report_abs(ts->input_pen, ABS_DISTANCE, 0);
		input_report_abs(ts->input_pen, ABS_TILT_X, 0);
		input_report_abs(ts->input_pen, ABS_TILT_Y, 0);
		ts->pdata.state = 0;
		if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.screen) {
			if (ts->aes_mode == 0 && atomic_read(&ts->state.dualscreen)
					&& ts->pdata.uevent_handled) {
//				touch_send_uevent(ts, TOUCH_UEVENT_SWITCH_AES_BOTH);
				atomic_set(&ts->state.uevent, UEVENT_IDLE);
				ts->pdata.uevent_handled = false;
			}
		}
	} else	{
		/* <Detect> */
		if((ts->pdata.in_range == 1) && (ts->pdata.contact == 1)) { //<Tip>
			pen_flow_num = PRESS;
			input_report_key(ts->input_pen, BTN_TOUCH, 1);
			input_report_abs(ts->input_pen, ABS_TILT_X, ts->pdata.tilt);
			input_report_abs(ts->input_pen, ABS_TILT_Y, ts->pdata.azimuth);
			if (ts->display_area_status[DISPLAY_AREA_ID_0].lpwg.screen) {
				if (!atomic_read(&ts->state.active_pen) && ts->pdata.state == 0) {
					touch_send_uevent(ts, TOUCH_UEVENT_PEN_DETECTION);
					ts->pdata.state = 1;
				}
			}
		} else if((ts->pdata.in_range == 1) && (ts->pdata.contact == 0)) {
			if (ts->pdata.old_pen_flow_num == PRESS) {
				input_report_key(ts->input_pen, BTN_TOUCH, 0);
				pen_flow_num = RELEASE;
			} else {
				pen_flow_num = HOVER_ENTER;
			}
		}

		if((ts->pdata.in_range == 0) && (ts->pdata.contact == 1)) {
			pen_flow_num = DEBUG_1;
		} else {
			input_report_key(ts->input_pen, BTN_TOOL_PEN, 1);	//<Tip> & <Hover>
			input_report_abs(ts->input_pen, ABS_X, ts->pdata.x);
			input_report_abs(ts->input_pen, ABS_Y, ts->pdata.y);
			if ((ts->pdata.pressure == 0) && (ts->pdata.contact == 1)) {
				TOUCH_I("abnormal pressure %d", ts->pdata.pressure);
				if (ts->pdata.prev_pressure == 0) {
					input_report_abs(ts->input_pen, ABS_PRESSURE, 1);
				} else {
					input_report_abs(ts->input_pen,
							ABS_PRESSURE, ts->pdata.prev_pressure);
				}
			} else {
				input_report_abs(ts->input_pen, ABS_PRESSURE, ts->pdata.pressure);
			}
			input_report_abs(ts->input_pen, ABS_DISTANCE, 1);
		}
	}

	/* btn */
	if ((ts->pdata.in_range == 1) && ts->pdata.swit1) {
		input_report_key(ts->input_pen, BTN_STYLUS, 1);
		btn_status = SWITCH_1_P;
	} else {
		input_report_key(ts->input_pen, BTN_STYLUS, 0);
		if (ts->pdata.old_btn_status == SWITCH_1_P) {
			btn_status = SWITCH_1_R;
		}
	}
	if ((ts->pdata.in_range == 1) && ts->pdata.swit2) {
		input_report_key(ts->input_pen, BTN_STYLUS2, 1);
		btn_status = SWITCH_2_P;
	} else {
		input_report_key(ts->input_pen, BTN_STYLUS2, 0);
		if (ts->pdata.old_btn_status == SWITCH_2_P) {
			btn_status = SWITCH_2_R;
		}
	}
	if (ts->pdata.old_pen_flow_num != pen_flow_num && active_pen_state) {
		TOUCH_I("%s X: %4d Y: %4d Batt: %2d P: %4d T: %3d A: %3d\n",
				pen_flow[pen_flow_num],
				ts->pdata.x,
				ts->pdata.y,
				ts->pdata.batt,
				ts->pdata.pressure,
				ts->pdata.tilt,
				ts->pdata.azimuth);
	}
	if (ts->pdata.old_btn_status != btn_status && active_pen_state) {
		if (btn_status != PEN_NONE)
			TOUCH_I("%s\n", pen_flow[btn_status]);
	}
	ts->pdata.old_btn_status = btn_status;
	ts->pdata.old_pen_flow_num = pen_flow_num;
	if (pen_debug_mask) {
		TOUCH_I("%s id:%llu id_r:%d cont:%d sw1:%d sw2:%d bat:%d x:%d y:%d z:%d prevz:%d tt:%d am:%d\n",
				pen_flow[pen_flow_num],
				(unsigned long long)ts->pdata.id,
				ts->pdata.in_range,
				ts->pdata.contact,
				ts->pdata.swit1,
				ts->pdata.swit2,
				ts->pdata.batt,
				ts->pdata.x,
				ts->pdata.y,
				ts->pdata.pressure,
				ts->pdata.prev_pressure,
				ts->pdata.tilt,
				ts->pdata.azimuth);
	}
	ts->pdata.prev_pressure = ts->pdata.pressure;
	input_sync(ts->input_pen);
}

static ssize_t show_pen_log_ctl(struct device *dev, char *buf)
{
	int ret = 0;

	TOUCH_TRACE();

	ret += snprintf(buf + ret, PAGE_SIZE, "%d\n", pen_debug_mask);
	TOUCH_I("%s: pen_debug_mask = %d\n", __func__, pen_debug_mask);

	return ret;
}

static ssize_t store_pen_log_ctl(struct device *dev,
		const char *buf, size_t count)
{
	int id = 0;
	int contact_in_range = 0;
	int swit1_2 = 0;
	int batt = 0;
	int x_y = 0;
	int pressure = 0;
	int tilt = 0;
	int azimuth = 0;
	int value = 0;

	TOUCH_TRACE();

	if (sscanf(buf, "%x", &value) <= 0)
		return count;

	pen_debug_mask = value;

	id = value & 0x1;
	contact_in_range = (value>>1) & 0x1;

	swit1_2 = (value>>2) & 0x1;

	batt = (value>>3) & 0x1;
	x_y = (value>>4) & 0x1;

	pressure = (value>>5) & 0x1;
	tilt = (value>>6) & 0x1;
	azimuth = (value>>7) & 0x1;

	TOUCH_I("(value:%x)(id:%d)(contact,in_range:%d)(swit1,2:%d)(batt:%d)\
(x,y:%d)(pressure:%d)(tilt:%d)(azimuth:%d)\n",
		value, id, contact_in_range, swit1_2, batt, x_y , pressure, tilt, azimuth);

	return count;
}

static TOUCH_ATTR(pen_log_ctl, show_pen_log_ctl, store_pen_log_ctl);

static struct attribute *pen_attribute_list[] = {
	&touch_attr_pen_log_ctl.attr,
	NULL,
};

static const struct attribute_group pen_attribute_group = {
	.attrs = pen_attribute_list,
	NULL,
};

int pen_register_sysfs(struct device *dev)
{

	struct touch_core_data *ts = to_touch_core(dev);
	int ret = 0;
	TOUCH_TRACE();

	TOUCH_I("pen sysfs reigster\n");
	ret = sysfs_create_group(&ts->kobj, &pen_attribute_group);

	if (ret < 0) {
		TOUCH_E("failed to create sysfs\n");
	}

	return ret;
}

void pen_set_input_prop(struct touch_core_data *ts, struct input_dev *input)
{
	TOUCH_TRACE();
	input->phys = "devices/virtual/input";
	TOUCH_I("%s %d-%d\n", __func__,
			ts->caps.max_x,
			ts->caps.max_y);

	/* Common Set */
	set_bit(EV_SYN, input->evbit);
	set_bit(EV_ABS, input->evbit);
	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_TOUCH, input->keybit);
	set_bit(BTN_STYLUS, input->keybit);
	set_bit(BTN_STYLUS2, input->keybit);
	set_bit(BTN_TOOL_PEN, input->keybit);
	set_bit(INPUT_PROP_POINTER, input->propbit);
	input_set_abs_params(input, ABS_X, 0, ts->caps.max_x, 0, 0);
	input_set_abs_params(input, ABS_Y, 0, ts->caps.max_y, 0, 0);
	input_set_abs_params(input, ABS_DISTANCE, 0, 1, 0, 0);
	input_set_abs_params(input, ABS_PRESSURE, 0, 4096, 0, 0);
	input_set_abs_params(input, ABS_TILT_X, 0, 180, 0, 0);
	input_set_abs_params(input, ABS_TILT_Y, 0, 180, 0, 0);
	input_set_abs_params(input, ABS_MT_ORIENTATION, 0,
			90, 0, 0);
	input_set_drvdata(input, ts);
}

int pen_input_init(struct device *dev)
{
	struct touch_core_data *ts = to_touch_core(dev);
	int ret;

	TOUCH_TRACE();

	ts->input_pen = input_allocate_device();
	if (!ts->input_pen) {
		TOUCH_E("failed to allocate memory for input pen\n");
		return -ENOMEM;
	}

	ts->input_pen->name = "pen_dev";
	pen_set_input_prop(ts, ts->input_pen);
	ret = input_register_device(ts->input_pen);
	if (ret < 0) {
		TOUCH_E("failed to register input pen(ret:%d)\n", ret);
		goto error_register;
	}

	return 0;

error_register:
	input_mt_destroy_slots(ts->input_pen);
	input_free_device(ts->input_pen);

	if (ts->input_pen) {
		input_mt_destroy_slots(ts->input_pen);
		input_free_device(ts->input_pen);
	}
	return ret;
}

MODULE_AUTHOR("rangkast.jeong@lge.com");
MODULE_DESCRIPTION("LGE PEN driver v2");
MODULE_LICENSE("GPL");
