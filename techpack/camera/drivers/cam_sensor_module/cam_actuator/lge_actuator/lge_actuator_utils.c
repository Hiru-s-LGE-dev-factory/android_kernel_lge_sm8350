/* ================================================================== */
/*  LGE AF HALL Utils  */
/* ================================================================== */
#include <linux/device.h>
#include <asm/arch_timer.h>
#include <cam_sensor_cmn_header.h>
#include "cam_actuator_core.h"
#include "cam_debug_util.h"
#include "lge_actuator_utils.h"

#define REG_ADDR_SOC_0         (0x84)
#define REG_ADDR_SOC_1         (0x3)
#define REG_ADDR_SOC_3         (0xF01A)

extern struct class* get_camera_class(void);
extern void lge_actuator_create_sysfs(struct cam_actuator_ctrl_t *a_ctrl);
extern void lge_actuator_destroy_sysfs(struct cam_actuator_ctrl_t *a_ctrl);
extern bool lge_actuator_data_enqueue(uint32_t reg_addr, uint32_t reg_data,
        struct cam_actuator_ctrl_t *a_ctrl);
static void lge_actuator_read_work(struct work_struct *work);
static int lge_actuator_start_read_thread(struct cam_actuator_ctrl_t *a_ctrl);
extern int lge_actuator_stop_read_thread( struct cam_actuator_ctrl_t *a_ctrl);
extern int lge_actuator_parse_userdata(struct cam_cmd_actuator_userdata *af_userdata,
        struct cam_actuator_ctrl_t *a_ctrl);

extern bool lge_actuator_data_enqueue(uint32_t reg_addr, uint32_t reg_data, struct cam_actuator_ctrl_t *a_ctrl)
{
    bool rc;
    struct msm_act_readout_buffer *o_buf = NULL;
    uint64_t hall_readout_time = 0;
    int16_t  act_hall = 0;

    if (a_ctrl == NULL) {
        return true;
    }

    CAM_DBG(CAM_ACTUATOR, "i2c data %x %x", reg_addr, reg_data);

    switch(a_ctrl->soc_info.index)
    {
        case 0:
        {
            if (0x0 == reg_addr){
                reg_data = reg_data >> 4;
            }
            else
            if (REG_ADDR_SOC_0 != reg_addr) { return true; }
            act_hall = 0xFFF & (reg_data);
        } break;
        case 1:
        {
            if (REG_ADDR_SOC_1 != reg_addr) { return true; }
            act_hall = 0xFFF & (reg_data);
        } break;
        case 3:
        {
            if (REG_ADDR_SOC_3 != reg_addr) { return true; }
            act_hall = 0xFFF & (reg_data);
        } break;
        default :
        {
            return true;
        }
    }

    hall_readout_time = __arch_counter_get_cntvct();

    CAM_DBG(CAM_ACTUATOR, "act 0x%x, %d, time %lu", act_hall, act_hall, hall_readout_time);

    spin_lock(&a_ctrl->hall_lock);

    o_buf = &(a_ctrl->buf);
    if ((o_buf->buffer_tail >= 0) &&
            (o_buf->buffer_tail < MSM_ACT_DATA_BUFFER_SIZE)) {
        o_buf->buffer[o_buf->buffer_tail].act_hall = act_hall;
        o_buf->buffer[o_buf->buffer_tail].hall_readout_time = hall_readout_time;

        o_buf->buffer_tail++;
        if (o_buf->buffer_tail >= MSM_ACT_DATA_BUFFER_SIZE)
            o_buf->buffer_tail -= MSM_ACT_DATA_BUFFER_SIZE;

        rc = true;
    } else {
        rc = false;
    }

    spin_unlock(&a_ctrl->hall_lock);

    return rc;
}

static ssize_t show_actuator_hall(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int count = 0;
    struct cam_actuator_ctrl_t *a_ctrl = dev_get_drvdata(dev);

    if(a_ctrl->cam_act_state == CAM_ACTUATOR_START) {
        struct msm_act_readout hall_data[MAX_HALL_QUERY_SIZE];
        uint8_t query_size = MAX_HALL_QUERY_SIZE;
        uint8_t data_get_count = 0;
        uint16_t counter = 0;
        int32_t i;

        memset(hall_data, 0, sizeof(hall_data));

        spin_lock(&a_ctrl->hall_lock);

        counter = a_ctrl->buf.buffer_tail;
        for (i = query_size-1; i >= 0; i-- )
        {
            counter = (counter == 0) ? MSM_ACT_DATA_BUFFER_SIZE-1 : counter-1;
            hall_data[i].act_hall = a_ctrl->buf.buffer[counter].act_hall;
            hall_data[i].hall_readout_time = a_ctrl->buf.buffer[counter].hall_readout_time;
            data_get_count++;
        }
        spin_unlock(&a_ctrl->hall_lock);

        if (data_get_count != 0 && data_get_count < MAX_HALL_QUERY_SIZE + 1) {
            for(i = 0; i < data_get_count; i++) {
                count += sprintf(buf + strlen(buf),"%d,%016llx,\n",hall_data[i].act_hall, hall_data[i].hall_readout_time);
            }
        }

        CAM_DBG(CAM_ACTUATOR,"[ACTUATOR] data_get_count = %d count : %d", data_get_count,count);
    }
    else {
        CAM_ERR(CAM_ACTUATOR,"[ACTUATOR] AF Thread is not working");
    }

    return count;
}

static DEVICE_ATTR(act_normal_hall, S_IRUGO, show_actuator_hall, NULL);
static DEVICE_ATTR(act_vt_hall, S_IRUGO, show_actuator_hall, NULL);
static DEVICE_ATTR(act_tele_hall, S_IRUGO, show_actuator_hall, NULL);

extern void lge_actuator_create_sysfs(struct cam_actuator_ctrl_t *a_ctrl)
{
    struct device*  camera_act_hall_dev;

    if(!a_ctrl->camera_class) {
        a_ctrl->camera_class = get_camera_class();

        if(a_ctrl->soc_info.index == 0 && a_ctrl->camera_class != NULL) {
            camera_act_hall_dev   = device_create(a_ctrl->camera_class, NULL,
                    0, a_ctrl, "actuator_normal");
            device_create_file(camera_act_hall_dev, &dev_attr_act_normal_hall);

#if 0 //defined(CONFIG_MACH_LAHAINA_BLM) || defined(CONFIG_MACH_LAHAINA_RAINBOWLM)
            a_ctrl->timer = kzalloc(sizeof(struct lge_actuator_timer_t), GFP_KERNEL);
#else
            a_ctrl->timer = NULL;
#endif
        }

        if(a_ctrl->soc_info.index == 1 && a_ctrl->camera_class != NULL) {
            camera_act_hall_dev   = device_create(a_ctrl->camera_class, NULL,
                    0, a_ctrl, "actuator_vt");
            device_create_file(camera_act_hall_dev, &dev_attr_act_vt_hall);

            a_ctrl->timer = NULL;
        }

        if(a_ctrl->soc_info.index == 3 && a_ctrl->camera_class != NULL) {
            camera_act_hall_dev   = device_create(a_ctrl->camera_class, NULL,
                    0, a_ctrl, "actuator_tele");
            device_create_file(camera_act_hall_dev, &dev_attr_act_tele_hall);

            a_ctrl->timer = NULL;
        }
    }
}

extern void lge_actuator_destroy_sysfs(struct cam_actuator_ctrl_t *a_ctrl)
{
    if(a_ctrl == NULL) {
        return;
    }

    if(a_ctrl->timer != NULL) {
        kfree(a_ctrl->timer);
        a_ctrl->timer = NULL;
    }

    if(a_ctrl->camera_class) {
        CAM_INFO(CAM_ACTUATOR, "[ACTUATOR] destory_sysfs id : %d", a_ctrl->soc_info.index);
        //class_destroy(a_ctrl->camera_class);
        a_ctrl->camera_class = NULL;
    }
}

static void lge_actuator_read_work(struct work_struct *work)
{
    int32_t rc = 0;
    bool result;
    uint32_t buf = 0;

    int32_t shift = 0;
    struct lge_actuator_timer_t *af_timer_in_t = NULL;
    struct cam_actuator_ctrl_t  *a_ctrl = NULL;

    af_timer_in_t = container_of(work, struct lge_actuator_timer_t, g_work);
    a_ctrl = (struct cam_actuator_ctrl_t*)af_timer_in_t->a_ctrl;

    if (a_ctrl->cam_act_state < CAM_ACTUATOR_START)
    {
        CAM_DBG(CAM_ACTUATOR, "wait");
        return;
    }

    rc = camera_io_dev_read(
        &(a_ctrl->io_master_info),
        REG_ADDR_SOC_0,
        &buf,
        CAMERA_SENSOR_I2C_TYPE_BYTE,
        CAMERA_SENSOR_I2C_TYPE_WORD);

    shift = buf >> 4;

    if (rc != 0) {
        af_timer_in_t->i2c_fail_count++;
        CAM_ERR(CAM_ACTUATOR, "[ACTUATOR] %s : i2c_read_seq fail. cnt = %d",
                __func__, af_timer_in_t->i2c_fail_count);
        if (af_timer_in_t->i2c_fail_count == MAX_FAIL_CNT) {
            CAM_ERR(CAM_ACTUATOR, "[ACTUATOR] %s : Too many i2c failed. Stop timer.",
                    __func__);
            af_timer_in_t->actuator_timer_state = ACTUATOR_TIME_ERROR;
        }
    } else {
        af_timer_in_t->i2c_fail_count = 0;
        result = lge_actuator_data_enqueue(REG_ADDR_SOC_0, shift, a_ctrl);

        if (!result)
            CAM_ERR(CAM_ACTUATOR, "[ACTUATOR] %s %d af data enqueue ring buffer failed",
                    __func__, __LINE__);
    }
}

static enum hrtimer_restart lge_read_timer(struct hrtimer *timer)
{
    ktime_t currtime, interval;
    struct lge_actuator_timer_t *af_timer_in_t = NULL;
    struct cam_actuator_ctrl_t  *a_ctrl = NULL;

    af_timer_in_t = container_of(timer, struct lge_actuator_timer_t, hr_timer);
    a_ctrl = (struct cam_actuator_ctrl_t*)af_timer_in_t->a_ctrl;

    if ((a_ctrl->cam_act_state >= CAM_ACTUATOR_CONFIG)
            && (af_timer_in_t->actuator_timer_state != ACTUATOR_TIME_ERROR)) {

        queue_work(af_timer_in_t->actuator_wq, &af_timer_in_t->g_work);
        currtime  = ktime_get();
        interval = ktime_set(0, READ_OUT_TIME);
        hrtimer_forward(timer, currtime, interval);

        return HRTIMER_RESTART;
    } else {
        CAM_ERR(CAM_ACTUATOR, "[ACTUATOR] %s HRTIMER_NORESTART af_state : %d timer_state : %d",
                __func__, a_ctrl->cam_act_state, af_timer_in_t->actuator_timer_state);
        return HRTIMER_NORESTART;
    }
}

static int lge_actuator_start_read_thread(struct cam_actuator_ctrl_t * a_ctrl)
{
    ktime_t  ktime;
    struct lge_actuator_timer_t *timer_t = NULL;

    CAM_INFO(CAM_ACTUATOR, "[ACTUATOR] %s:E", __func__);

    if (a_ctrl == NULL || a_ctrl->timer == NULL)
    {
        return 0;
    }

    timer_t = (struct lge_actuator_timer_t *)(a_ctrl->timer);

    if (timer_t->actuator_timer_state == ACTUATOR_TIME_ERROR) {
        CAM_ERR(CAM_ACTUATOR, "[ACTUATOR] %s:Timer error, close befoe create :%d.",
                __func__, timer_t->actuator_timer_state);
        lge_actuator_stop_read_thread(a_ctrl);
    }

    timer_t->i2c_fail_count = 0;

    if (timer_t->actuator_timer_state != ACTUATOR_TIME_ACTIVE) {
        timer_t->a_ctrl = a_ctrl;

        INIT_WORK(&timer_t->g_work, lge_actuator_read_work);
        timer_t->actuator_wq = create_workqueue("actuator_wq");

        if (!timer_t->actuator_wq) {
            CAM_ERR(CAM_ACTUATOR, "[ACTUATOR]:%s actuator_wq create failed.", __func__);
            return -EFAULT;
        }

        ktime = ktime_set(0, READ_OUT_TIME);
        hrtimer_init(&timer_t->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        timer_t->hr_timer.function = &lge_read_timer;

        hrtimer_start(&timer_t->hr_timer, ktime, HRTIMER_MODE_REL);
        timer_t->actuator_timer_state = ACTUATOR_TIME_ACTIVE;
    }
    else
    {
        CAM_ERR(CAM_ACTUATOR, "[ACTUATOR] invalid timer state = %d.",
            timer_t->actuator_timer_state);
    }

    CAM_INFO(CAM_ACTUATOR, "[ACTUATOR] %s:X", __func__);

    return 0;
}

extern int lge_actuator_stop_read_thread(struct cam_actuator_ctrl_t *a_ctrl)
{
    struct lge_actuator_timer_t *timer_t = NULL;

    CAM_INFO(CAM_ACTUATOR, "[ACTUATOR] %s:E", __func__);

    if (a_ctrl == NULL || a_ctrl->timer == NULL)
    {
        return 0;
    }

    timer_t = (struct lge_actuator_timer_t *)(a_ctrl->timer);

    switch(timer_t->actuator_timer_state)
    {
        case ACTUATOR_TIME_ACTIVE :
        case ACTUATOR_TIME_ERROR :
            CAM_INFO(CAM_ACTUATOR, "[ACTUATOR] %s:timer cancel.", __func__);
            hrtimer_cancel(&(timer_t->hr_timer));
            destroy_workqueue(timer_t->actuator_wq);
            timer_t->actuator_timer_state = ACTUATOR_TIME_INACTIVE;
            break;
        case ACTUATOR_TIME_INIT :
        case ACTUATOR_TIME_INACTIVE :
            break;
        default :
            timer_t->actuator_timer_state = ACTUATOR_TIME_INACTIVE;
            CAM_ERR(CAM_ACTUATOR, "[ACTUATOR] invalid timer state = %d",
                    timer_t->actuator_timer_state);
        break;
    }

    CAM_INFO(CAM_ACTUATOR, "[ACTUATOR] %s:X", __func__);

    return 0;
}

extern int lge_actuator_parse_userdata(
        struct cam_cmd_actuator_userdata *af_userdata,
        struct cam_actuator_ctrl_t *a_ctrl)
{
    int rc = -1;

    if (a_ctrl == NULL)
    {
        return rc;
    }

    CAM_INFO(CAM_ACTUATOR, "[ACTUATOR] %s: %d index = %d", __func__,
                        af_userdata->select, a_ctrl->soc_info.index);

    if (a_ctrl->timer != NULL)
    {
        switch(af_userdata->select)
        {
            case 1:
                if(af_userdata->actuatorThreadNeeded == 1) {
                    rc = lge_actuator_start_read_thread(a_ctrl);
                } else {
                    rc = lge_actuator_stop_read_thread(a_ctrl);
                }
                break;
            default:
                    rc = 0;
                break;
        }
    }

    return rc;
}


