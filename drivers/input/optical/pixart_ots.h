#ifndef _PIXART_OTS_H_
#define _PIXART_OTS_H_

#include <linux/fs.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include  "pixart_platform.h"

/* export funtions */
void OTS_Reset_Variables(void);
uint8_t OTS_Detect_Rotation(int16_t dy16);
uint8_t OTS_Detect_Tilt(int16_t dx16);
void OTS_Sensor_Init(void);
void OTS_Sensor_ReadMotion(int16_t *dx, int16_t *dy);
void OTS_Sensor_Find_MarkingPosition(int *position, int *md, int *shutter, int shutter_th);

/* Global variables for Press Detection */
#define OTS_ROT_NO_CHANGE	0x00
#define OTS_ROT_UP			0x01
#define OTS_ROT_DOWN		0x02
#define OTS_ROT_STOP		0x03

#define RESET_FLAG        4
#define RESET_FACTOR      6

#define DISABLE_OTS_SENSOR    0
#define ENABLE_OTS_SENSOR     1
#define ENABLE_OTS_READY      2
#define ENABLE_MINIOS_MODE    3

#define DISABLE				0
#define ENABLE				1

#define TARGET_POSITION_UNKNOWN		0
#define TARGET_POSITION_COLLAPSE	1
#define TARGET_POSITION_DISABLE		2
#define TARGET_POSITION_EXPAND		3
#define TARGET_POSITION_HALF		4

#define PIXEL_MIN	0
#define PIXEL_HALF	286
#define PIXEL_MAX	520

#define SLIDE_COLLAPSE    1080
#define SLIDE_HALF        1366
#define SLIDE_EXPAND      1600

#define PIXEL_MARK_OFFSET	50
#define PIXEL_CAL_OFFSET	30
#define PIXEL_MIN_OFFSET	15

#define NON_DETECT			0
#define DETECT_COLLAPSE		1
#define DETECT_UNKNOWN		2
#define DETECT_EXPAND		3
#define DETECT_HALF			4

#define EXCEPTION_NON_DETECT	5
#define EXCEPTION_DETECT		6

#define MOT_STOP_NON_MARK	0
#define MOT_MOVE_NON_MARK	1
#define MOT_STOP_DET_MARK	2
#define MOT_MOVE_DET_MARK	3

#define MOTION_BIT              0x80
#define MKR_SHUTTER_TH_ECHO     4
#define MKR_SHUTTER_TH_MIN      6
#define MKR_SHUTTER_TH_MAX      7
#define MKR_SHUTTER_W_MIN       1
#define MKR_SHUTTER_TH_OFFSET   1
#define MKR_SHUTTER_ERROR       -1

#define MONITORING_MOVING_INTERVAL         100
#define MONITORING_COLLAPSE                150
#define MONITORING_EXPAND                  400
#define MONITORING_MOVING_FAST             3
#define MONITORING_MOVING_SLOW             1
#define MONITORING_MOVING_FORCE_UP         100
#define MONITORING_MOVING_FORCE_DOWN       100

#define MONITORING_INTERVAL_NOMAL          20
#define MONITORING_INTERVAL_START          90
#define MONITORING_INTERVAL_END            10
#define MONITORING_INTERVAL_FORCE          20

#define MARK_STOP_TIME_OFFSET_CLOSE    100
#define MARK_STOP_TIME_OFFSET_OPEN     100

#define REV_A_IRQ	428

#define MOTOR_MAX_PPS    1000

#define CAL_FACTOR_PATH    "/data/vendor/ots/cal_factor.txt"
#define SHUTTER_TH_PATH    "/data/vendor/ots/shutter_th.txt"

extern bool initFlag;

#endif
