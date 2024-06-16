#ifndef LGE_OIS_UTILS_H
#define LGE_OIS_UTILS_H

#define MAX_HALL_QUERY_SIZE 15
#define READ_OUT_TIME 10000000 /*5ms*/
#define MAX_FAIL_CNT 3

enum lge_ois_timer_state_t {
	OIS_TIME_INIT,
	OIS_TIME_ACTIVE,
	OIS_TIME_INACTIVE,
	OIS_TIME_ERROR,
};

struct lge_ois_timer_t {
	struct hrtimer hr_timer;
	struct workqueue_struct *ois_wq;
	struct work_struct g_work;
	enum lge_ois_timer_state_t ois_timer_state;
	int i2c_fail_count;
	void *o_ctrl;
};

#endif
