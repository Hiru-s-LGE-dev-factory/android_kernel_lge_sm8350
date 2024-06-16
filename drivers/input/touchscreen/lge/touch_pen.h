#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/of_gpio.h>

enum {
	P_INACTIVE,
	P_RELEASED,
	P_PRESSED,
	DUMMY_2,
	P_HOVER,
	P_DETECT,
	P_RELEASE,
	P_HOVER_RELEASED,
};

struct button_data
{
	u16 key_code;
	u16 state;
};

enum {
	PEN_NONE = 0,
	HOVER_ENTER,
	HOVER_EXIT,
	PRESS,
	RELEASE,
	SWITCH_1_P,
	SWITCH_1_R,
	SWITCH_2_P,
	SWITCH_2_R,
	DEBUG_1,
	DEBUG_2,
};

/*******************************************************************
*  These values is to calculate the angle between two vectors.
*  998600   // 3(degree)
*  996200   // 5(degree)
*  992600   // 7(degree)
*  984800   // 10(degree)
*  974400   // 13(degree)
* *******************************************************************/
#define DELTA_DEGREE_HIGH_THRESHOLD     998600   // 3(degree)
#define DELTA_DEGREE_LOW_THRESHOLD      996200   // 5(degree)
#define VELOCITY_LOW_THRESHOLD          20
#define VELOCITY_HIGH_THRESHOLD         30
#define MOVE_CHK_CNT                    5
#define VSYNC_TIMING                    500      // 4.2*1000
#define INCHTOMILLI                     254
#define DPI                             392
struct pen_prediction_data {
	int predict_x;
	int predict_y;
	int prev_predict_x;
	int prev_predict_y;
	int prev_est_x;
	int prev_est_y;
	int prev_unit_x;
	int prev_unit_y;
	int prev_x;
	int prev_y;
	u16 move_cnt;
	u16 predict_ratio;
};

struct pen_data {
	u64 id;
	//
	u16 in_range;
	u16 contact;
	u16 swit1;
	u16 swit2;
	//
	u16 batt;
	u16 x;
	u16 y;
	u16 pressure;
	//
	u16 tilt;
	u16 azimuth;

	u16 prev_pressure;
	int state;
	int old_pen_flow_num;
	int old_btn_status;
	bool uevent_handled;
};

#define PEN_BIT_HOVER		0
#define PEN_BIT_TIP			1
#define PEN_BIT_SWIT1		4
#define PEN_BIT_SWIT2		5

#define PEN_MASK_HOVER		BIT(PEN_BIT_HOVER)
#define PEN_MASK_TIP		BIT(PEN_BIT_TIP)
#define PEN_MASK_SWIT1		BIT(PEN_BIT_SWIT1)
#define PEN_MASK_SWIT2		BIT(PEN_BIT_SWIT2)

extern void pen_input_handler(struct device *dev);
extern int pen_input_init(struct device *dev);
extern int pen_register_sysfs(struct device *dev);
