/* ================================================================== */
/*  OIS firmware */
/* ================================================================== */
#include <linux/device.h>
#include <linux/spinlock.h>
#include <asm/arch_timer.h>
#include <cam_sensor_cmn_header.h>

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>

#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_debug_util.h"
#include "cam_sensor_util.h"
#include "lge_ois_utils.h"

#define OIS_HALL_SEQ_ADDR_0       (0x8B)
#define OIS_HALL_SEQ_ADDR_3       (0xE001)
#define OIS_GYRO_DEFAULT_GAIN     (15284)

extern struct class* get_camera_class(void);
extern void lge_ois_create_sysfs(struct cam_ois_ctrl_t *o_ctrl);
extern void lge_ois_destroy_sysfs(struct cam_ois_ctrl_t *o_ctrl);

static void lge_ois_read_work(struct work_struct *work);
static int lge_ois_start_read_thread(struct cam_ois_ctrl_t *o_ctrl);
extern int lge_ois_stop_read_thread( struct cam_ois_ctrl_t *o_ctrl);
extern int lge_ois_parse_userdata(struct cam_cmd_ois_userdata *ois_userdata,
        struct cam_ois_ctrl_t *o_ctrl);

static ssize_t show_ois_hall(struct device *dev, struct device_attribute *attr, char *buf)
{
    int count = 0;
    struct cam_ois_ctrl_t *o_ctrl = dev_get_drvdata(dev);

    if(o_ctrl->cam_ois_state == CAM_OIS_START) {
        struct msm_ois_readout hall_data[MAX_HALL_QUERY_SIZE];
        uint8_t query_size = MAX_HALL_QUERY_SIZE;
        uint8_t data_get_count = 0;
        uint16_t counter = 0;
        int32_t i;

        memset(hall_data, 0, sizeof(hall_data));

        spin_lock(&o_ctrl->hall_lock);

        counter = o_ctrl->buf.buffer_tail;
        for (i = query_size-1; i >= 0; i-- )
        {
            counter = (counter == 0) ? MSM_OIS_DATA_BUFFER_SIZE-1 : counter-1;
			hall_data[i].ois_x_shift = o_ctrl->buf.buffer[counter].ois_x_shift;
			hall_data[i].ois_y_shift = o_ctrl->buf.buffer[counter].ois_y_shift;
			hall_data[i].x_readout_time = o_ctrl->buf.buffer[counter].x_readout_time;
			hall_data[i].y_readout_time = o_ctrl->buf.buffer[counter].y_readout_time;
            data_get_count++;
        }
        spin_unlock(&o_ctrl->hall_lock);

        if (data_get_count != 0 && data_get_count < MAX_HALL_QUERY_SIZE + 1) {
            for(i = 0; i < data_get_count; i++) {
                CAM_DBG(CAM_OIS, "ois_x_shift is %d, ois_y_shift is %d, time %ld",
                             hall_data[i].ois_x_shift, hall_data[i].ois_y_shift, hall_data[i].x_readout_time);
				count += sprintf(buf + strlen(buf),"%d,%016llx,%d,%016llx,\n",
                             hall_data[i].ois_x_shift, hall_data[i].x_readout_time,
                             hall_data[i].ois_y_shift, hall_data[i].y_readout_time);
            }
        }

        CAM_DBG(CAM_OIS, "[OIS]:data_get_count = %d count : %d", data_get_count,count);
    }
    else {
        CAM_ERR(CAM_OIS, "[OIS] OIS Thread is not working");
    }

    return count;
}
static DEVICE_ATTR(ois_hall, S_IRUGO, show_ois_hall, NULL);

static DEVICE_ATTR(ois_tele_hall, S_IRUGO, show_ois_hall, NULL);

extern void lge_ois_create_sysfs(struct cam_ois_ctrl_t *o_ctrl) {
	struct device*  camera_ois_hall_dev;

    if(!o_ctrl->camera_class) {
        o_ctrl->camera_class = get_camera_class();

        if(o_ctrl->soc_info.index == 0 && o_ctrl->camera_class != NULL) {
            camera_ois_hall_dev   = device_create(o_ctrl->camera_class, NULL,
                    0, o_ctrl, "ois_hall");
            device_create_file(camera_ois_hall_dev, &dev_attr_ois_hall);

            o_ctrl->timer = kzalloc(sizeof(struct lge_ois_timer_t), GFP_KERNEL);
        }

        if(o_ctrl->soc_info.index == 3 && o_ctrl->camera_class != NULL) {
            camera_ois_hall_dev   = device_create(o_ctrl->camera_class, NULL,
                    0, o_ctrl, "ois_tele_hall");
            device_create_file(camera_ois_hall_dev, &dev_attr_ois_tele_hall);

            o_ctrl->timer = kzalloc(sizeof(struct lge_ois_timer_t), GFP_KERNEL);
        }
    }
}

extern void lge_ois_destroy_sysfs(struct cam_ois_ctrl_t *o_ctrl) {
    if(o_ctrl == NULL) {
        return;
    }

    if(o_ctrl->timer != NULL) {
        kfree(o_ctrl->timer);
        o_ctrl->timer = NULL;
    }

    if(o_ctrl->camera_class) {
        CAM_INFO(CAM_ACTUATOR, "[AFHALL] destory_sysfs id : %d", o_ctrl->soc_info.index);
        //class_destroy(o_ctrl->camera_class);
        o_ctrl->camera_class = NULL;
    }
}

static bool lge_ois_data_enqueue(uint64_t x_readout_time,
		int32_t x_shift,
		uint64_t y_readout_time,
		int32_t y_shift,
		struct cam_ois_ctrl_t *o_ctrl)
{
	bool rc;
    struct msm_ois_readout_buffer *o_buf = &o_ctrl->buf;

	spin_lock(&o_ctrl->hall_lock);

	if (o_buf->buffer_tail >= 0 && o_buf->buffer_tail <
			MSM_OIS_DATA_BUFFER_SIZE) {
		o_buf->buffer[o_buf->buffer_tail].ois_x_shift = x_shift;
		o_buf->buffer[o_buf->buffer_tail].ois_y_shift = y_shift;
		o_buf->buffer[o_buf->buffer_tail].x_readout_time = x_readout_time;
		o_buf->buffer[o_buf->buffer_tail].y_readout_time = y_readout_time;

		o_buf->buffer_tail++;
		if (o_buf->buffer_tail >= MSM_OIS_DATA_BUFFER_SIZE)
			o_buf->buffer_tail -= MSM_OIS_DATA_BUFFER_SIZE;

		rc = true;
	} else {
		rc = false;
	}

	spin_unlock(&o_ctrl->hall_lock);

	return rc;
}

static void lge_ois_read_work(struct work_struct *work)
{
	int32_t rc = 0;
	bool result;
	uint8_t buf[6] = {0,};
	uint64_t x_readout_time = 0;
	uint64_t y_readout_time = 0;
	int32_t x_shift = 0;
	int32_t y_shift = 0;
    struct lge_ois_timer_t *ois_timer_in_t = NULL;
    struct cam_ois_ctrl_t  *o_ctrl = NULL;

    ois_timer_in_t = container_of(work, struct lge_ois_timer_t, g_work);
    o_ctrl = (struct cam_ois_ctrl_t*)ois_timer_in_t->o_ctrl;

    if (o_ctrl->cam_ois_state < CAM_OIS_START)
    {
        CAM_DBG(CAM_OIS, "wait");
        return;
    }

    switch (o_ctrl->soc_info.index)
    {
        case 0:
        {
            rc = camera_io_dev_read_seq(
        		&(o_ctrl->io_master_info),
        		OIS_HALL_SEQ_ADDR_0,
        		buf,
        		CAMERA_SENSOR_I2C_TYPE_BYTE,
        		CAMERA_SENSOR_I2C_TYPE_BYTE,
        		6);

        	x_readout_time = __arch_counter_get_cntvct();
            y_readout_time = x_readout_time;
        	x_shift = (int32_t)((int16_t)((buf[2] << 8) | (buf[3])));
        	y_shift = (int32_t)((int16_t)((buf[4] << 8) | (buf[5])));
        } break;
        case 3 :
        {
            rc = camera_io_dev_read_seq(
        		&(o_ctrl->io_master_info),
        		OIS_HALL_SEQ_ADDR_3,
        		buf,
        		CAMERA_SENSOR_I2C_TYPE_WORD,
        		CAMERA_SENSOR_I2C_TYPE_BYTE,
        		6);

        	x_readout_time = __arch_counter_get_cntvct();
            y_readout_time = x_readout_time;
            x_shift = (int32_t)((int16_t)((buf[0] << 8) | (buf[1])));
            y_shift = (int32_t)((int16_t)((buf[2] << 8) | (buf[3])));
            x_shift = x_shift * 32767 /
                (o_ctrl->gyro_gain_x > 0 ? o_ctrl->gyro_gain_x : OIS_GYRO_DEFAULT_GAIN);
            y_shift = y_shift * 32767 /
                (o_ctrl->gyro_gain_y > 0 ? o_ctrl->gyro_gain_y : OIS_GYRO_DEFAULT_GAIN);

        } break;
        default:
        break;
    }

    CAM_DBG(CAM_OIS, "soc %d x_shift 0x%x, y_shift 0x%x, time %lu",
            o_ctrl->soc_info.index, x_shift, y_shift, x_readout_time);

	if (rc != 0) {
		ois_timer_in_t->i2c_fail_count++;
		CAM_ERR(CAM_OIS, "[OIS] %s : i2c_read_seq fail. cnt = %d",
				__func__, ois_timer_in_t->i2c_fail_count);
		if (ois_timer_in_t->i2c_fail_count == MAX_FAIL_CNT) {
			CAM_ERR(CAM_OIS, "[OIS] %s : Too many i2c failed. Stop timer.",
					__func__);
			ois_timer_in_t->ois_timer_state = OIS_TIME_ERROR;
		}
	} else {
		ois_timer_in_t->i2c_fail_count = 0;
		result = lge_ois_data_enqueue(x_readout_time, x_shift,
				y_readout_time, y_shift, o_ctrl);

		if (!result)
			CAM_ERR(CAM_OIS, "%s %d ois data enqueue ring buffer failed",
					__func__, __LINE__);
	}
}

static enum hrtimer_restart lge_ois_read_timer(struct hrtimer *timer)
{
    ktime_t currtime, interval;
    struct lge_ois_timer_t *ois_timer_in_t = NULL;
    struct cam_ois_ctrl_t  *o_ctrl = NULL;

    ois_timer_in_t = container_of(timer, struct lge_ois_timer_t, hr_timer);
    o_ctrl = (struct cam_ois_ctrl_t*)ois_timer_in_t->o_ctrl;

	if ((o_ctrl->cam_ois_state >= CAM_OIS_CONFIG)
			&& (ois_timer_in_t->ois_timer_state != OIS_TIME_ERROR)) {
        queue_work(ois_timer_in_t->ois_wq, &ois_timer_in_t->g_work);
        currtime  = ktime_get();
        interval = ktime_set(0, READ_OUT_TIME);
        hrtimer_forward(timer, currtime, interval);

        return HRTIMER_RESTART;
    } else {
		CAM_ERR(CAM_OIS, "[OIS] %s HRTIMER_NORESTART ois_state : %d timer_state : %d",
				__func__, o_ctrl->cam_ois_state, ois_timer_in_t->ois_timer_state);
        return HRTIMER_NORESTART;
    }
}

static int lge_ois_start_read_thread(struct cam_ois_ctrl_t * o_ctrl)
{
    ktime_t  ktime;
    struct lge_ois_timer_t *timer_t = NULL;

    CAM_INFO(CAM_OIS, "[OIS] %s:E", __func__);

    if (o_ctrl == NULL || o_ctrl->timer == NULL)
    {
        return 0;
    }

    timer_t = (struct lge_ois_timer_t *)(o_ctrl->timer);

    if (timer_t->ois_timer_state == OIS_TIME_ERROR) {
        CAM_ERR(CAM_OIS, "[OIS] %s:Timer error, close befoe create :%d.",
                __func__, timer_t->ois_timer_state);
        lge_ois_stop_read_thread(o_ctrl);
    }

    timer_t->i2c_fail_count = 0;

    if (timer_t->ois_timer_state != OIS_TIME_ACTIVE) {
        timer_t->o_ctrl = o_ctrl;

        INIT_WORK(&timer_t->g_work, lge_ois_read_work);
        timer_t->ois_wq = create_workqueue("ois_wq");

        if (!timer_t->ois_wq) {
            CAM_ERR(CAM_OIS, "[OIS]:%s ois_wq create failed.", __func__);
            return -EFAULT;
        }

        ktime = ktime_set(0, READ_OUT_TIME);
        hrtimer_init(&timer_t->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        timer_t->hr_timer.function = &lge_ois_read_timer;

        hrtimer_start(&timer_t->hr_timer, ktime, HRTIMER_MODE_REL);
        timer_t->ois_timer_state = OIS_TIME_ACTIVE;
    }
    else
    {
        CAM_ERR(CAM_OIS, "[OIS] invalid timer state = %d.",
            timer_t->ois_timer_state);
    }

    CAM_INFO(CAM_OIS, "[OIS] %s:X", __func__);

    return 0;
}


extern int lge_ois_stop_read_thread(struct cam_ois_ctrl_t *o_ctrl)
{
    struct lge_ois_timer_t *timer_t = NULL;

    CAM_INFO(CAM_OIS, "[OIS] %s:E", __func__);

    if (o_ctrl == NULL || o_ctrl->timer == NULL)
    {
        return 0;
    }

    timer_t = (struct lge_ois_timer_t *)(o_ctrl->timer);

    switch(timer_t->ois_timer_state)
    {
        case OIS_TIME_ACTIVE :
        case OIS_TIME_ERROR :
            CAM_INFO(CAM_OIS, "[OIS] cancel timer");
            hrtimer_cancel(&(timer_t->hr_timer));
            destroy_workqueue(timer_t->ois_wq);
            timer_t->ois_timer_state = OIS_TIME_INACTIVE;
            break;
        case OIS_TIME_INIT :
        case OIS_TIME_INACTIVE :
            break;
        default :
            timer_t->ois_timer_state = OIS_TIME_INACTIVE;
            CAM_ERR(CAM_OIS, "[OIS] invalid timer state = %d",
                    timer_t->ois_timer_state);
        break;
    }

    CAM_INFO(CAM_ACTUATOR, "[OIS] %s:X", __func__);

    return 0;
}

extern int lge_ois_parse_userdata(
        struct cam_cmd_ois_userdata *ois_userdata,
        struct cam_ois_ctrl_t *o_ctrl)
{
    int rc = -1;

    if (o_ctrl == NULL)
    {
        return rc;
    }

    CAM_INFO(CAM_OIS, "[OIS] %s: %d index = %d", __func__,
                        ois_userdata->select, o_ctrl->soc_info.index);

    if (o_ctrl->timer != NULL)
    {
        switch(ois_userdata->select)
        {
            case 1:
                if(ois_userdata->oisThreadNeeded == 1) {
                    rc = lge_ois_start_read_thread(o_ctrl);
                } else {
                    rc = lge_ois_stop_read_thread(o_ctrl);
                }
                break;
            default:
                    rc = 0;
                break;
        }
    }

    return rc;
}



