#ifndef LGE_ACTUATOR_UTILS_H
#define LGE_ACTUATOR_UTILS_H
/* ================================================================== */
/*  LGE AF HALL Utils  */
/* ================================================================== */

#define MAX_HALL_QUERY_SIZE 15
#define READ_OUT_TIME 5000000 /*5ms*/
#define MAX_FAIL_CNT 3

enum lge_actuator_timer_state {
    ACTUATOR_TIME_INIT,
    ACTUATOR_TIME_ACTIVE,
    ACTUATOR_TIME_INACTIVE,
    ACTUATOR_TIME_ERROR,
};

struct lge_actuator_timer_t {
    struct hrtimer hr_timer;
    struct workqueue_struct *actuator_wq;
    struct work_struct g_work;
    enum lge_actuator_timer_state actuator_timer_state;
    int i2c_fail_count;
    void* a_ctrl;
};

#endif /* LGE_ACTUATOR_UTILS_H */
