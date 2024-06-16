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
 */

#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/ice40/ice40-core.h>
#include <linux/fpga/ice40-spi.h>

#include <soc/qcom/lge/board_lge.h>

//#define MOTOR_WAKE_LOCK
#if defined(MOTOR_WAKE_LOCK)
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#endif

#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/syscalls.h>

#define TUNING_FILE_PATH         "/data/vendor/motor/motor_self_test.txt"
#define FH_SAVE_FILE_PATH        "/data/vendor/motor/motor_fh_time_num.txt"

#define LATTTICE_MCU_CLK                  24000000

#define MOTOR_DEFAULT_PULSE                   4920

#define COMPENSATED_OPEN_VALUE                   0
#define COMPENSATED_CLOSE_VALUE                300
#define COMPENSATED_EXCEPTION_VALUE            300

#define COMPENSATED_OPEN_TIME                  200
#define COMPENSATED_CLOSE_TIME                 230
#define COMPENSATED_EXCEPTION_TIME             150

#define EXCEPTION_CASE_PULSE_RATIO            2500

#define TIME_COUNT_ARRAY_SIZE                    2
#define ACC_DEC_RATE_AND_CNT_ARRAY_SIZE         30

/*
 * Extern ICE40 Global Value
 */
extern struct ice40 *global_ice40;

struct motor_board_data {
	struct gpio_desc *status;
};

struct motor_drv {
	/* Platform_device */
	struct platform_device *pdev;
    struct motor_board_data bdata;

	/* Check Init Value*/
	bool motor_default_cal_dt;
	int motor_fh_pps_index;
	int motor_temperature_sect;
	int motor_operation_type;

	int motor_comp_cvalue;
	int motor_comp_evalue;

	int motor_max_pps;
	int motor_min_pps;

	/* Check Motor Enable Value */
	bool motor_status;
	unsigned int motor_status_irq;

	/* Check Motor Pulse Value */
	int motor_total_pulse;
	int motor_pulse_ratio;

	/* Check Motor Current Position */
	int motor_kposition;

#if defined(MOTOR_WAKE_LOCK)
	/* Wakeup irq */
	int wakeup_irq;
#endif
};
static struct motor_drv *mdrv = NULL;

/*
 * Motor Default / Current Register Table
 */
static unsigned char default_register[MOTOR_REG_DATA_MAX] = {0, };
static unsigned char current_register[MOTOR_REG_DATA_MAX] = {0, };

/*
 * Motor Close Acelation / Deceleration Table
 */
static unsigned char close_r_acc_dec_rate_and_count[ACC_DEC_RATE_AND_CNT_ARRAY_SIZE] = {0, };

/*
 * Motor Exception Case test Acelation / Deceleration Table
 */
static unsigned char exception_r_acc_dec_rate_and_count[ACC_DEC_RATE_AND_CNT_ARRAY_SIZE] = {0, };

/*
 * Motor Frequency Setting Value
 */
static struct reg_default low_frequency_time_cnt[][TIME_COUNT_ARRAY_SIZE] = {
//	{ {ICE40_R_FH_TIME_CNT_15_8, 0x27}, {ICE40_R_FH_TIME_CNT_7_0, 0x10}, },     // 0) 1.9 Sec (2400pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x28}, {ICE40_R_FH_TIME_CNT_7_0, 0xC2}, },     // 1) 2.1 Sec (2300pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x2A}, {ICE40_R_FH_TIME_CNT_7_0, 0x9D}, },     // 2) 2.2 Sec (2200pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x2E}, {ICE40_R_FH_TIME_CNT_7_0, 0xE0}, },     // 3) 2.4 Sec (2000pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x34}, {ICE40_R_FH_TIME_CNT_7_0, 0x15}, },     // 4) 2.7 Sec (1800pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x3A}, {ICE40_R_FH_TIME_CNT_7_0, 0x98}, },     // 5) 3.0 Sec (1600pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x42}, {ICE40_R_FH_TIME_CNT_7_0, 0xF6}, },     // 6) 3.5 Sec (1400pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x5D}, {ICE40_R_FH_TIME_CNT_7_0, 0xC0}, },     // 7) 4.8 Sec (1000pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0xBB}, {ICE40_R_FH_TIME_CNT_7_0, 0x80}, },     // 8) 9.6 Sec ( 500pps)
	{ {ICE40_R_FH_TIME_CNT_15_8, 0x55}, {ICE40_R_FH_TIME_CNT_7_0, 0x3A}, },     // Min Default(1100pps)
};
