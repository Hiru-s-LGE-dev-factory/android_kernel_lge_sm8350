/*
 ** =============================================================================
 **
 ** File: ImmVibeSPI.c
 **
 ** Description:
 **     Device-dependent functions called by Immersion TSP API
 **     to control PWM duty cycle, amp enable/disable, save IVT file, etc...
 **
 **
 ** Copyright (c) 2018 Immersion Corporation. All Rights Reserved.
 **
 ** This file contains Original Code and/or Modifications of Original Code
 ** as defined in and that are subject to the GNU Public License v2 -
 ** (the 'License'). You may not use this file except in compliance with the
 ** License. You should have received a copy of the GNU General Public License
 ** along with this program; if not, write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA or contact
 ** TouchSenseSales@immersion.com.
 **
 ** The Original Code and all software distributed under the License are
 ** distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 ** EXPRESS OR IMPLIED, AND IMMERSION HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 ** INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 ** FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see
 ** the License for the specific language governing rights and limitations
 ** under the License.
 **
 ** =============================================================================
 */

#ifdef IMMVIBESPIAPI
#undef IMMVIBESPIAPI
#endif
#define IMMVIBESPIAPI static

#define CONFIG_LGE_VIB
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#if defined(CONFIG_LGE_VIB)
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>

#include <linux/uaccess.h>
#include <linux/pm_wakeup.h>
#include <linux/of.h>
#include <tspdrv.h>

#if 1
#define gprintk(fmt, x... ) printk( "%s: " fmt, __FUNCTION__ , ## x)
#else
#define gprintk(x...) do { } while (0)
#endif

#define IMMVIBESPI_DEVICE_GETSTATUS_SUPPORT

#define DW7914_INFO             0x00
#define DW7914_VERSION          0x00
#define DW7914_STATUS           0x01
#define DW7914_FIFOFULL         0x01
#define DW7914_MODE				0x0B
#define DW7914_DATA             0x0A
#define DW7914_SWRESET          0x5F
#define DW7914_LDO              0x0A
#define SW_RESET_COMMAND        0x01 /* Reset */
#define HW_RESET_ENABLE_COMMAND 0x00 /* Enable hardware */
#define RTP_MODE				0x00
#define MEM_MODE				0x01
#endif

/*
 ** i2c bus
 */
#if defined(CONFIG_LGE_VIB)
#define DEVICE_BUS  4
#else
#define DEVICE_BUS  3
#endif

/*
 ** i2c device address
 */
#if defined(CONFIG_LGE_VIB)
#define DEVICE_ADDR 0x59
#else
#define DEVICE_ADDR 0x5A
#endif

/*
 ** Number of actuators this SPI supports
 */
#define NUM_ACTUATORS 1

/*
 ** Name displayed in TouchSense API and tools
 */
#define DEVICE_NAME "LG Alpha"

/*
 ** Manage amplifier buffer using ImmVibeSPI_ForceOut_BufferFull
 */
#define IMMVIBESPI_USE_BUFFERFULL

/*
 ** Number of sample periods to prebuffer before sending
 */
#define PREBUFFER_SAMPLE_PERIODS 3

/*
 ** Higher values buffer more future samples.
 */
#define DW7914_FIFOFULL_TARGET (PREBUFFER_SAMPLE_PERIODS * VIBE_OUTPUT_SAMPLE_SIZE)

#define DW7914_FIFO_STATUS 0x4D //TEST

/*
 ** Upscale immvibed data because it supports up to 12kHz (and 24kHz is less audible)
 */
#define UPSCALE_12_TO_24khz

#define NAK_RESEND_ATTEMPT 3

VibeStatus I2CWrite(unsigned char address, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer);
static void i2c_commands(struct i2c_client *i2c, struct i2c_msg *msgs, u8 *command, int size);
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer);

struct dw7914_dev {
#if defined(CONFIG_LGE_VIB)
	int use_en_pin; /* en gpio enable/disable */
	int enable_pin; /* if use_en_pin is 1, this pin used */
	int trigger_index;
	int vd_clamp;
	int trigger_num;
	bool trigger_enable;
#endif
	unsigned char chipid;
	unsigned char i2c_addr;
	struct i2c_client *i2c;
	ktime_t time;
	int buffering;
	s64 position;
#ifdef UPSCALE_12_TO_24khz
	VibeInt8 buffer[1 + DW7914_FIFOFULL_TARGET*2];
#else
	VibeInt8 buffer[1 + DW7914_FIFOFULL_TARGET];
#endif
};

static struct dw7914_dev *dw7914;

#if defined(CONFIG_LGE_VIB)
static int index_read_pattern;
#endif


/*
 ** I2C Transfer Functions
 */

static int i2c_recv_buf(struct i2c_client *i2c, unsigned char reg, unsigned char *buf, int count)
{
	struct i2c_msg msg[2];

	msg[0].addr = i2c->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = count;
	msg[1].buf = buf;

	return i2c_transfer(i2c->adapter, msg, 2);
}

static int i2c_send_buf(struct i2c_client *i2c, unsigned char *buf, int count)
{
	struct i2c_msg msg;

	msg.addr = i2c->addr;
	msg.flags = 0;
	msg.len = count;
	msg.buf = buf;

	return i2c_transfer(i2c->adapter, &msg, 1);
}

#define DW7914_CHIPID                0x00
#define DW7914_STATUS                0x01
#define DW7914_INT_EN                0x02

#define DW7914_MODE                  0x0B
#define DW7914_MODE_RTP              0x00
#define DW7914_MODE_MEMORY           0x01

//Trigger Settings
#define DW7914_TRIG_CTRL             0x03
#define DW7914_TRIG_CTRL_DEFAULT     0x04
#define DW7914_TRIG_CTRL_TRIG1_MODE  0x20
#define DW7914_TRIG_CTRL_PRIORITY    0x01

#define DW7914_TRIG1_R_WAVE          0x1F
#define DW7914_BRAKE0_R1_WAVE        0x20
#define DW7914_BRAKE1_R1_WAVE        0x21
#define DW7914_TRIG1_F_WAVE          0x22
#define DW7914_BRAKE0_F1_WAVE        0x23
#define DW7914_BRAKE1_F1_WAVE        0x24
#define DW7914_BRAKE_T1_CTRL         0x25

#define DW7914_TRIG2_R_WAVE          0x26
#define DW7914_BRAKE0_R2_WAVE        0x27
#define DW7914_BRAKE1_R2_WAVE        0x28
#define DW7914_TRIG2_F_WAVE          0x29
#define DW7914_BRAKE0_F2_WAVE        0x2A
#define DW7914_BRAKE1_F2_WAVE        0x2B
#define DW7914_BRAKE_T2_CTRL         0x2C

#define DW7914_TRIG3_R_WAVE          0x2D
#define DW7914_BRAKE0_R3_WAVE        0x2E
#define DW7914_BRAKE1_R3_WAVE        0x2F
#define DW7914_TRIG3_F_WAVE          0x30
#define DW7914_BRAKE0_F3_WAVE        0x31
#define DW7914_BRAKE1_F3_WAVE        0x32
#define DW7914_BRAKE_T3_CTRL         0x33

#define DW7914_TRIG_DET_EN           0x45
#define DW7914_TRIG_DET_EN_TRIG3_F_EN 0x80
#define DW7914_TRIG_DET_EN_TRIG3_R_EN 0x40
#define DW7914_TRIG_DET_EN_TRIG2_F_EN 0x20
#define DW7914_TRIG_DET_EN_TRIG2_R_EN 0x10
#define DW7914_TRIG_DET_EN_TRIG1_F_EN 0x08
#define DW7914_TRIG_DET_EN_TRIG1_R_EN 0x04
#define DW7914_TRIG_DET_EN_RESTART_EN 0x01

#define DW7914_PWM_FREQ              0x04
#define DW7914_PWM_FREQ_24kHz        0x00
#define DW7914_PWM_FREQ_48kHz        0x01
#define DW7914_PWM_FREQ_96kHz        0x02
#define DW7914_PWM_FREQ_12kHz        0x03

#define DW7914_BOOST_OUTPUT          0x05
#define DW7914_BOOST_OUTPUT_4_68     0x00
#define DW7914_BOOST_OUTPUT_6_36     0x4F
#define DW7914_BOOST_OUTPUT_7_00     0x57
#define DW7914_BOOST_OUTPUT_8_60     0x6B
#define DW7914_BOOST_OUTPUT_10_2     0x7F

#define DW7914_BOOST_MODE            0x07
#define DW7914_BOOST_MODE_VBST       0x00
#define DW7914_BOOST_MODE_BST_EN     0x01
#define DW7914_BOOST_MODE_BST_BYPASS 0x02
#define DW7914_BOOST_MODE_BST_LUMP   0x04
#define DW7914_BOOST_MODE_BST_ADAPT  0x08

#define DW7914_BOOST_OPTION                0x06
#define DW7914_BOOST_OPTION_BST_OFFSET_mV(x) ((x)/80)   /* 80 mV increments */
#define DW7914_BOOST_OPTION_BST_ILIMIT_30  0x00
#define DW7914_BOOST_OPTION_BST_ILIMIT_25  0x20
#define DW7914_BOOST_OPTION_BST_ILIMIT_20  0x40
#define DW7914_BOOST_OPTION_BST_ILIMIT_15  0x60
#define DW7914_BOOST_OPTION_BST_ILIMIT_35  0x10

#define DW7914_VMH                   0x09

#define DW7914_VD_CLAMP              0x0A
#define DW7914_VD_CLAMP_mV(x)        ((x)/40)

#define DW7914_PLAYBACK              0x0C
#define DW7914_PLAYBACK_GO           0x01
#define DW7914_PLAYBACK_STOP         0x00

#define DW7914_RTP_INPUT             0x0D
#define DW7914_MEM_GAIN              0x0B /* ignored in RTP mode */

#define DW7914_WAVE_SEQ0             0x0F
#define DW7914_WAVE_SEQ1             0x10
#define DW7914_WAVE_SEQ2             0x11
#define DW7914_WAVE_SEQ3             0x12
#define DW7914_WAVE_SEQ4             0x13
#define DW7914_WAVE_SEQ5             0x14
#define DW7914_WAVE_SEQ6             0x15
#define DW7914_WAVE_SEQ7             0x16
#define DW7914_WAVE_SEQ_LOOP0        0x17
#define DW7914_WAVE_SEQ_LOOP1        0x18
#define DW7914_WAVE_SEQ_LOOP2        0x19
#define DW7914_WAVE_SEQ_LOOP3        0x1A

#define DW7914_SWRST                 0x5F
#define DW7914_SWRST_SW_RESET        0x01

#define DW7914_DELAY_SAMPLES_US(us) ((us) * 60 / 5000)

#ifdef UPSCALE_12_TO_24khz
#define CHOSEN_SAMPLE_FREQUENCY DW7914_PWM_FREQ_24kHz
#else
#define CHOSEN_SAMPLE_FREQUENCY DW7914_PWM_FREQ_12kHz
#endif

#if !defined(CONFIG_LGE_VIB)
static void dw7914_dump_registers(struct dw7914_dev *dev)
{
	int i = 0;
	char buf = 0;
	char regs[] = {
		DW7914_CHIPID,
		DW7914_STATUS,
		DW7914_INT_EN,
		DW7914_MODE,
		DW7914_PWM_FREQ,
		DW7914_BOOST_OUTPUT,
		DW7914_BOOST_MODE,
		DW7914_VMH,
		DW7914_VD_CLAMP,
		DW7914_BOOST_OPTION
	};

	if (!dev || !dev->i2c) return;

	for (i = 0; i < sizeof(regs); i++) {
		i2c_recv_buf(dev->i2c, regs[i], &buf, sizeof(buf));
		DbgOut((DBL_INFO, "dw7914: reg[0x%02x]=0x%02x\n", i, buf));
	}
}
#endif

/*
 ** I2C Driver
 */

#if defined(CONFIG_LGE_VIB)

static void setupWaveformHeader() {
	{
		u8 command[] = {
			DW7914_MODE, DW7914_MODE_MEMORY,
			0x0f, 0x01,
		};
		struct i2c_msg msgs[sizeof(command)/2];
		i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	}
	{
		int i;
		struct i2c_msg msg1;
		for (i = 0; i < ARRAY_SIZE(trigHeader__); i++) {
			msg1.flags = 0;
			msg1.addr = dw7914->i2c->addr;
			msg1.buf = trigHeader__[i];
			msg1.len = sizeof(trigHeader__[i]);

			if (1 != i2c_transfer(dw7914->i2c->adapter, &msg1, 1)) {
				DbgOut((DBL_ERROR, "dw7914 Memory Test Header Write Fail\n"));
				//return VIBE_E_FAIL;
			}
		}
	}
	{
		u8 command[] = {
			DW7914_MODE, DW7914_MODE_RTP,
		};
		struct i2c_msg msgs[sizeof(command)/2];
		i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	}
}

static void setupWaveformData() {
	{
		u8 command[] = {
			DW7914_MODE, DW7914_MODE_MEMORY,
			0x0f, 0x01,
		};
		struct i2c_msg msgs[sizeof(command)/2];
		i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	}
	{
		int i;
		struct i2c_msg msg;
		for (i = 0; i < ARRAY_SIZE(trigData__); i++) {
			msg.flags = 0;
			msg.addr = dw7914->i2c->addr;
			msg.buf = trigData__[i];
			msg.len = sizeof(trigData__[i]);
			if (1 != i2c_transfer(dw7914->i2c->adapter, &msg, 1)) {
				DbgOut((DBL_ERROR, "dw7914 Memory Test Data Write Fail\n"));
				//return VIBE_E_FAIL;
			}
		}
	}
	{
		u8 command[] = {
			DW7914_MODE, DW7914_MODE_RTP,
		};
		struct i2c_msg msgs[sizeof(command)/2];
		i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	}
}

static void setupTrigger_1(int index) {

	int r_wave = index*2-1;
	int f_wave = index*2;
	u8 command[] = {
		DW7914_TRIG_CTRL, DW7914_TRIG_CTRL_DEFAULT
				| DW7914_TRIG_CTRL_TRIG1_MODE
				| DW7914_TRIG_CTRL_PRIORITY,
        DW7914_TRIG1_R_WAVE, r_wave,
        DW7914_TRIG1_F_WAVE, f_wave,

		DW7914_TRIG_DET_EN,(
				DW7914_TRIG_DET_EN_TRIG1_F_EN|
				DW7914_TRIG_DET_EN_TRIG1_R_EN|
				DW7914_TRIG_DET_EN_RESTART_EN
				),
	};
	struct i2c_msg msgs[sizeof(command)/2];
	i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	pr_info("%s r_wave:%d, f_wave:%d\n",__func__, r_wave, f_wave);
}

static void setupTrigger_2(int index) {

	int f_wave = index*2-1;
	int r_wave = index*2;
	u8 command[] = {
		DW7914_TRIG_CTRL, DW7914_TRIG_CTRL_DEFAULT
				| DW7914_TRIG_CTRL_TRIG1_MODE
				| DW7914_TRIG_CTRL_PRIORITY,
		DW7914_TRIG2_R_WAVE, f_wave, // release
		DW7914_TRIG2_F_WAVE, r_wave, // press
		DW7914_TRIG3_R_WAVE, f_wave, // release
		DW7914_TRIG3_F_WAVE, r_wave, // press

		DW7914_TRIG_DET_EN,(
				DW7914_TRIG_DET_EN_TRIG3_F_EN|
				DW7914_TRIG_DET_EN_TRIG3_R_EN|
				DW7914_TRIG_DET_EN_TRIG2_F_EN|
				DW7914_TRIG_DET_EN_TRIG2_R_EN|
				DW7914_TRIG_DET_EN_RESTART_EN
				),
	};
	struct i2c_msg msgs[sizeof(command)/2];
	i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	pr_info("%s r_wave:%d, f_wave:%d\n",__func__, r_wave, f_wave);
}

static void setupTrigger_3(int index) {

	int f_wave = index*2-1;
	int r_wave = index*2;
	u8 command[] = {
		DW7914_TRIG_CTRL, DW7914_TRIG_CTRL_DEFAULT
				| DW7914_TRIG_CTRL_TRIG1_MODE
				| DW7914_TRIG_CTRL_PRIORITY,
        DW7914_TRIG1_R_WAVE, r_wave,
        DW7914_TRIG1_F_WAVE, f_wave,
		DW7914_TRIG2_R_WAVE, f_wave, // release
		DW7914_TRIG2_F_WAVE, r_wave, // press
		DW7914_TRIG3_R_WAVE, f_wave, // release
		DW7914_TRIG3_F_WAVE, r_wave, // press

		DW7914_TRIG_DET_EN,(
				DW7914_TRIG_DET_EN_TRIG3_F_EN|
				DW7914_TRIG_DET_EN_TRIG3_R_EN|
				DW7914_TRIG_DET_EN_TRIG2_F_EN|
				DW7914_TRIG_DET_EN_TRIG2_R_EN|
				DW7914_TRIG_DET_EN_TRIG1_F_EN|
				DW7914_TRIG_DET_EN_TRIG1_R_EN|
				DW7914_TRIG_DET_EN_RESTART_EN
				),
	};
	struct i2c_msg msgs[sizeof(command)/2];
	i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	pr_info("%s r_wave:%d, f_wave:%d\n",__func__, r_wave, f_wave);
}

static void disableTrigger() {

	u8 command[] = {
	DW7914_TRIG_CTRL, DW7914_TRIG_CTRL_DEFAULT
			| DW7914_TRIG_CTRL_TRIG1_MODE
			| DW7914_TRIG_CTRL_PRIORITY,
		DW7914_TRIG1_R_WAVE, 0x00,
		DW7914_TRIG1_F_WAVE, 0x00,
		DW7914_TRIG2_R_WAVE, 0x00, // release
		DW7914_TRIG2_F_WAVE, 0x00, // press
		DW7914_TRIG3_R_WAVE, 0x00, // release
		DW7914_TRIG3_F_WAVE, 0x00, // press

	DW7914_TRIG_DET_EN,(
             0
			),
	};
	struct i2c_msg msgs[sizeof(command)/2];
	i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
}

static void setTrigger(int ntrig, int dir, int nwave) {
	u8 reg = 0;
	if (ntrig == 1 && dir == 0) {
		reg = DW7914_TRIG1_R_WAVE;
	} else if (ntrig == 1 && dir == 1) {
		reg = DW7914_TRIG1_F_WAVE;
	}
	if (ntrig == 2 && dir == 0) {
		reg = DW7914_TRIG2_R_WAVE;
	} else if (ntrig == 2 && dir == 1) {
		reg = DW7914_TRIG2_F_WAVE;
	} else if (ntrig == 3 && dir == 0) {
		reg = DW7914_TRIG3_R_WAVE;
	} else if (ntrig == 3 && dir == 1) {
		reg = DW7914_TRIG3_F_WAVE;
	}
	{
		u8 command[] = {
			reg, nwave,
		};
		struct i2c_msg msgs[sizeof(command)/2];
		i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	}
	pr_info("%s: set dw7914 trigger %d, %s to wave %d\n", __func__,
			ntrig, dir ? "Falling" : "Rising", nwave);
}

static void dw7914_poweron(int mode)
{
	unsigned char data;
	msleep(5);

	if(!mode) {
		// Software reset
		data = 0x01;
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_SWRESET, 1, &data))
		{
			//DbgOut((DBL_ERROR, "I2CWrite failed to send SW_RESET_COMMAND\n"));
		}
		msleep(1);
#if !defined(CONFIG_LGE_VIB)
		// batch dw7914 configuration sequence
		{
			u8 command[] = {
				DW7914_MODE, DW7914_MODE_RTP,
				DW7914_VD_CLAMP, DW7914_VD_CLAMP_mV(8000),
				DW7914_BOOST_OPTION, DW7914_BOOST_OPTION_BST_OFFSET_mV(400) | DW7914_BOOST_OPTION_BST_ILIMIT_35,
				DW7914_PWM_FREQ, CHOSEN_SAMPLE_FREQUENCY,
			};
			struct i2c_msg msgs[sizeof(command)/2];
			i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
		}
#else
		data = DW7914_MODE_RTP;
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_MODE, 1, &data))
		{
			//DbgOut((DBL_ERROR, "I2CWrite failed to send RTP_COMMAND\n"));
		}
		msleep(1);
		data = DW7914_VD_CLAMP_mV(dw7914->vd_clamp);
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_VD_CLAMP, 1, &data))
		{
			//DbgOut((DBL_ERROR, "I2CWrite failed to send RTP_COMMAND\n"));
		}
		msleep(1);

		data = DW7914_BOOST_OPTION_BST_OFFSET_mV(400) | DW7914_BOOST_OPTION_BST_ILIMIT_35;
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_BOOST_OPTION, 1, &data))
		{
			//DbgOut((DBL_ERROR, "I2CWrite failed to send RTP_COMMAND\n"));
		}
		msleep(1);

		data = CHOSEN_SAMPLE_FREQUENCY;
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_PWM_FREQ, 1, &data))
		{
			//DbgOut((DBL_ERROR, "I2CWrite failed to send RTP_COMMAND\n"));
		}
		msleep(1);

#endif
	}

	else {
		data = 0x01;
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_SWRESET, 1, &data))
		{
			//DbgOut((DBL_ERROR, "I2CWrite failed to send SW_RESET_COMMAND\n"));
		}
		msleep(1);

		data = MEM_MODE; // meomory mode set
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_MODE, 1, &data))
		{
			//DbgOut((DBL_ERROR, "I2CWrite failed to send RTP_COMMAND\n"));
		}
		msleep(1);
	}
	//dw7914_dump_registers(&dw7914);
	setupWaveformHeader();
	setupWaveformData();
	if(dw7914->trigger_num == 2)
		setupTrigger_2(dw7914->trigger_index);
	else if(dw7914->trigger_num == 3)
		setupTrigger_3(dw7914->trigger_index);
	else
		setupTrigger_1(dw7914->trigger_index);
}

static struct workqueue_struct *test_workqueue;
static struct delayed_work *test_work;

static void dw7914_trigger_test_clean(struct work_struct *work) {
	u8 command[] = {
		DW7914_MODE, DW7914_MODE_RTP,
	};
	struct i2c_msg msgs[sizeof(command)/2];
	i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
}

static void dw7914_trigger_test() {
	u8 command[] = {
		DW7914_MODE, DW7914_MODE_MEMORY,
		DW7914_WAVE_SEQ0, 0x01,
		DW7914_WAVE_SEQ_LOOP0, 0x00,
		DW7914_PLAYBACK, DW7914_PLAYBACK_GO
	};
	struct i2c_msg msgs[sizeof(command)/2];
	i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
	queue_delayed_work(test_workqueue, test_work, msecs_to_jiffies(500));
}

static void dw7914_trigger_test_init(struct device *dev) {
	test_workqueue = create_workqueue("trigger_test_workqueue");
	test_work = devm_kzalloc(dev, sizeof(struct delayed_work), GFP_KERNEL);
	if (!test_work) {
		pr_err("%s: devm_kzalloc failed\n", __func__);
		return;
	}
	INIT_DELAYED_WORK(test_work, dw7914_trigger_test_clean);
}

/*
 * sysfs device functions
 * ---
 */
static ssize_t dw7914_show_regs(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	int i, cnt = 0;

	for (i = 0; i < 0x5f; i++) {
		if (i % 0x10 == 0 && i != 0) {
			cnt += sprintf(buf + cnt, "\n");
		}
		cnt += sprintf(buf + cnt, "%02x: %02x  ", i, i2c_smbus_read_byte_data(c, i));
	}
	cnt += sprintf(buf + cnt, "\n");

	return cnt;
}


static ssize_t dw7914_store_regs(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	struct i2c_client *c = dw7914->i2c;
	unsigned int reg = 0, val = 0;
	char tmp[5];
	int i;

	memset(tmp,0x00,5);
	for(i = 0; i < cnt; i++) {
		if ( buf[i] != ' ' )
			tmp[i] = buf[i];
		else {
			buf += i+1;
			reg = simple_strtoul(tmp, NULL, 16);
			val = simple_strtoul(buf, NULL, 16);
			break;
		}
	}
	printk(KERN_INFO "writing: 0x%x: 0x%02X --> 0x%02X\n",
			reg, i2c_smbus_read_byte_data(c, reg), val);
	i2c_smbus_write_byte_data(c, reg, val);

	return cnt;
}

static DEVICE_ATTR(index_reg, 0660,
		dw7914_show_regs, dw7914_store_regs);


#if defined(CONFIG_LGE_VIB) // #ifdef PATTERN_TUNE
static ssize_t dw7914_p_h_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	int j, k = 0; char tmp[5];
	int index_row = 0;

	memcpy(tmp, buf, 2);
	index_row = simple_strtoul(tmp , NULL, 10);
	buf += 3;

	memset(tmp,0x00,5);

	for (j = 0; j < sizeof(trigHeader__[6]) ; j++) { 
	
		memcpy(tmp, buf, 4);
		trigHeader__[index_row][k] = simple_strtoul(tmp , NULL, 16);
		
		k++;
		memset(tmp,0x00,5);

		buf += 5;
	}

	//setupWaveformHeader();
	//setupWaveformData();
	//setupTrigger();

	return cnt;
}
static ssize_t dw7914_p_h_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j, cnt = 0;

	for (i = 0; i <  ARRAY_SIZE(trigHeader__); i++) {
		for (j = 0; j <  sizeof(trigHeader__[i]); j++) {
			cnt += sprintf(buf + cnt, "%02x ,",trigHeader__[i][j]);
		}
		cnt += sprintf(buf + cnt, "\n");
	}
	cnt += sprintf(buf + cnt, "\n");
	
	return cnt;
}

static DEVICE_ATTR(patten_hdr, 0660,
		dw7914_p_h_show, dw7914_p_h_store);

static int patten_data_position(int pattern_position)
{
	int i, patten_p, temp_p;

	patten_p = 0;
	temp_p = 0;

	if(pattern_position==0)
		return patten_p;
		
	for (i = 0; i <  pattern_position; i++) {
		
		temp_p = trigHeader__[i][6] & 0x0F;
		
		if(trigHeader__[i][5] == 1) {
			patten_p += trigHeader__[i][6]>>4; 
			patten_p += 16;
			if(temp_p != 0x00)
				patten_p += 1; 			

		} else{
			patten_p += trigHeader__[i][6]>>4; 
			if(temp_p != 0x00)
				patten_p += 1;
		}
		printk(KERN_INFO "patten[%d] position = %d \n", i+1,patten_p);
	}	
	return patten_p;
}
static int patten_data_size(int pattern_position)
{
	int p_size = 0;
	int temp_size = 0;
	
	temp_size = trigHeader__[pattern_position][6] & 0x0F;
		
	if(trigHeader__[pattern_position][5] == 1) {
		p_size += trigHeader__[pattern_position][6]>>4; 
		p_size += 16;
		if(temp_size != 0x00)
			p_size += 1; 			

	} else{
		p_size += trigHeader__[pattern_position][6]>>4; 
		if(temp_size != 0x00)
			p_size += 1;
	}

	return p_size;
}

static ssize_t dw7914_p_d_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	int i = 0; int index_row = 0;  char tmp[5];

	memset(tmp,0x00,5); 

	memcpy(tmp, buf, 3);
	index_row = simple_strtoul(tmp , NULL, 10);
	buf += 4;
	
	printk(KERN_INFO "dw7914_p_d_store index_row = %d \n", index_row);

	for (i = 0; i < 19 ; i++) { 
		memcpy(tmp, buf, 4);
		trigData__[index_row][i] = simple_strtoul(tmp , NULL, 16);
		memset(tmp,0x00,5);
		buf += 6;
	}

	//setupWaveformHeader();
	//setupWaveformData();
	//setupTrigger();

	return cnt;
}

static ssize_t dw7914_p_d_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j, cnt = 0;
	int position = 0;
	int size = 0;
	
	position = patten_data_position(index_read_pattern);
	size = patten_data_size(index_read_pattern);

	printk(KERN_INFO "position = %d \n", position);
	printk(KERN_INFO "size = %d \n", size);

	for (i = position; i <  position + size; i++) {
		for (j = 0; j <  sizeof(trigData__[i]); j++) {
			cnt += sprintf(buf + cnt, "%02x ,",trigData__[i][j]);
		}
		cnt += sprintf(buf + cnt, "\n");
	}
	cnt += sprintf(buf + cnt, "\n");
	
	return cnt;
}

static DEVICE_ATTR(patten_data, 0660,
		dw7914_p_d_show, dw7914_p_d_store);

static ssize_t dw7914_p_d_index_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	char tmp[5];

	memcpy(tmp, buf, 2);
	index_read_pattern = simple_strtoul(tmp , NULL, 10);
	printk(KERN_INFO "dw7914_p_d_index = %d \n", index_read_pattern);

	return cnt;
}

static ssize_t dw7914_p_d_index_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	cnt += sprintf(buf + cnt, "index: %d", index_read_pattern);
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static DEVICE_ATTR(p_data_read_index, 0660,
		dw7914_p_d_index_show, dw7914_p_d_index_store);

static ssize_t dw7914_key_patten_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	char tmp[5]; int i;

	memcpy(tmp, buf, 2);
	i = simple_strtoul(tmp , NULL, 10);
	printk(KERN_INFO "dw7914_patten_update_store = %d \n",i);

	if(i != 0)
	{
		printk(KERN_INFO "dw7914_patten_update_store enter \n");
		setupWaveformHeader();
		setupWaveformData();
		if(dw7914->trigger_num == 2)
			setupTrigger_2(dw7914->trigger_index);
		else if(dw7914->trigger_num == 3)
			setupTrigger_3(dw7914->trigger_index);
		else
			setupTrigger_1(dw7914->trigger_index);
	}
	else
	{
		printk(KERN_INFO "dw7914_patten_update_store enter 2 \n");
		setupWaveformHeader();
		setupWaveformData();
		if(dw7914->trigger_num == 2)
			setupTrigger_2(dw7914->trigger_index);
		else if(dw7914->trigger_num == 3)
			setupTrigger_3(dw7914->trigger_index);
		else
			setupTrigger_1(dw7914->trigger_index);

	}
		

	return cnt;
}

static DEVICE_ATTR(key_irq_p_update, 0220,
		NULL, dw7914_key_patten_update_store);

#endif

static ssize_t dw7914_store_trigger_test(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	dw7914_trigger_test();
	return cnt;
}

static DEVICE_ATTR(trigger_test, 0220,
		NULL, dw7914_store_trigger_test);

static ssize_t dw7914_show_trig1_r(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	int cnt = 0;
	cnt += sprintf(buf + cnt, "Wave: %d", i2c_smbus_read_byte_data(c, DW7914_TRIG1_R_WAVE));
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig1_r(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);
	setTrigger(1, 0, val);
	return cnt;
}

static DEVICE_ATTR(trig1_r, 0660,
		dw7914_show_trig1_r, dw7914_store_trig1_r);

static ssize_t dw7914_show_trig1_f(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	int cnt = 0;
	cnt += sprintf(buf + cnt, "Wave: %d", i2c_smbus_read_byte_data(c, DW7914_TRIG1_F_WAVE));
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig1_f(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);
	setTrigger(1, 1, val);
	return cnt;
}

static DEVICE_ATTR(trig1_f, 0660,
		dw7914_show_trig1_f, dw7914_store_trig1_f);

static ssize_t dw7914_show_trig2_r(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	int cnt = 0;
	cnt += sprintf(buf + cnt, "Wave: %d", i2c_smbus_read_byte_data(c, DW7914_TRIG2_R_WAVE));
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig2_r(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);
	setTrigger(2, 0, val);
	return cnt;
}

static DEVICE_ATTR(trig2_r, 0660,
		dw7914_show_trig2_r, dw7914_store_trig2_r);

static ssize_t dw7914_show_trig2_f(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	int cnt = 0;
	cnt += sprintf(buf + cnt, "Wave: %d", i2c_smbus_read_byte_data(c, DW7914_TRIG2_F_WAVE));
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig2_f(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);
	setTrigger(2, 1, val);
	return cnt;
}

static DEVICE_ATTR(trig2_f, 0660,
		dw7914_show_trig2_f, dw7914_store_trig2_f);

static ssize_t dw7914_show_trig3_r(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	int cnt = 0;
	cnt += sprintf(buf + cnt, "Wave: %d", i2c_smbus_read_byte_data(c, DW7914_TRIG3_R_WAVE));
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig3_r(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);
	setTrigger(3, 0, val);
	return cnt;
}

static DEVICE_ATTR(trig3_r, 0660,
		dw7914_show_trig3_r, dw7914_store_trig3_r);

static ssize_t dw7914_show_trig3_f(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	int cnt = 0;
	cnt += sprintf(buf + cnt, "Wave: %d", i2c_smbus_read_byte_data(c, DW7914_TRIG3_F_WAVE));
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig3_f(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);
	setTrigger(3, 1, val);
	return cnt;
}

static DEVICE_ATTR(trig3_f, 0660,
		dw7914_show_trig3_f, dw7914_store_trig3_f);

static ssize_t dw7914_show_trig_ctrl(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	cnt += sprintf(buf + cnt, "trigger_enable: %d", dw7914->trigger_enable);
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig_ctrl(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);

	pr_info("%s val:%d\n", __func__, val);

	dw7914->trigger_enable = val;

	if(dw7914->trigger_enable) {
		if(dw7914->trigger_num == 2)
			setupTrigger_2(dw7914->trigger_index);
		else if(dw7914->trigger_num == 3)
			setupTrigger_3(dw7914->trigger_index);
		else
			setupTrigger_1(dw7914->trigger_index);
	}
	else
		disableTrigger();

	return cnt;
}

static DEVICE_ATTR(trig_ctrl, 0660,
		dw7914_show_trig_ctrl, dw7914_store_trig_ctrl);

static ssize_t dw7914_show_trig_index(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	cnt += sprintf(buf + cnt, "trigger_index: %d", dw7914->trigger_index);
	cnt += sprintf(buf + cnt, "\n");
	return cnt;
}

static ssize_t dw7914_store_trig_index(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	unsigned int val = 0;
	val = simple_strtoul(buf, NULL, 10);

	dw7914->trigger_index = val;

	pr_info("%s val:%d trigger_index:%d\n", __func__, val, dw7914->trigger_index);
	if(dw7914->trigger_enable){
		if(dw7914->trigger_num == 2)
			setupTrigger_2(dw7914->trigger_index);
		else if(dw7914->trigger_num == 3)
			setupTrigger_3(dw7914->trigger_index);
		else
			setupTrigger_1(dw7914->trigger_index);
	}
	return cnt;
}

static DEVICE_ATTR(trig_index, 0660,
		dw7914_show_trig_index, dw7914_store_trig_index);

static int ram_highaddr;
static int ram_lowaddr;
static int ram_len;

static ssize_t dw7914_show_highaddr(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	cnt += sprintf(buf + cnt, "0x%02X\n", ram_highaddr);
	return cnt;
}
static ssize_t dw7914_show_lowaddr(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	cnt += sprintf(buf + cnt, "0x%02X\n", ram_lowaddr);
	return cnt;
}
static ssize_t dw7914_show_len(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	cnt += sprintf(buf + cnt, "%d\n", ram_len);
	return cnt;
}
static ssize_t dw7914_show_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt = 0;
	int i = 0;
	const u8 RAM_HIGH = 0x46;

	u8 buff[100];

	{
		struct i2c_msg msg[2];
		u8 command[] = {
			RAM_HIGH, ram_highaddr, ram_lowaddr,
		};
		msg[0].flags = 0;
		msg[0].addr = dw7914->i2c->addr;
		msg[0].buf = command;
		msg[0].len = sizeof(command);

		msg[1].addr = dw7914->i2c->addr;
		msg[1].flags = I2C_M_RD;
		msg[1].len = ram_len;
		msg[1].buf = buff;
		i2c_transfer(dw7914->i2c->adapter, msg, 2);
	}

	for (i = 0; i < ram_len; i++) {
		cnt += sprintf(buf + cnt, "%03X: 0x%02X\n", ((ram_highaddr<<8) + ram_lowaddr + i), buff[i]);
	}

	return cnt;
}

static ssize_t dw7914_store_highaddr(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	ram_highaddr = simple_strtoul(buf, NULL, 16);
	return cnt;
}

static ssize_t dw7914_store_lowaddr(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	ram_lowaddr = simple_strtoul(buf, NULL, 16);
	return cnt;
}

static ssize_t dw7914_store_len(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	ram_len = simple_strtoul(buf, NULL, 10);
	if (ram_len >= 100)
		ram_len = 99;
	return cnt;
}

static ssize_t dw7914_store_data(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t cnt)
{
	return cnt;
}

static DEVICE_ATTR(ram_highaddr, 0660,
		dw7914_show_highaddr, dw7914_store_highaddr);
static DEVICE_ATTR(ram_lowaddr, 0660,
		dw7914_show_lowaddr, dw7914_store_lowaddr);
static DEVICE_ATTR(ram_len, 0660,
		dw7914_show_len, dw7914_store_len);
static DEVICE_ATTR(ram_data, 0660,
		dw7914_show_data, dw7914_store_data);
/*
#define PATTERN_FILEPATH "/vendor/etc/pattern"
static ssize_t dw7914_show_pattern_update(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *c = dw7914->i2c;
	struct file *fp = NULL;
	const int buf_len = 50;
	const u8 RAM_ADDR = 0x46;
	int i = 0;
	int cnt = 0;
	int ret = 0;
	int pattern_num = 1;
	int pattern_start = 0;
	int pattern_length = 0;
	char file_name_buf[buf_len];
	char read_buf[3];
	u8 pattern_header[8];
	u8 pattern_data[23];
	mm_segment_t fs;
	loff_t pos = 0;

	fs = get_fs();
	set_fs(KERNEL_DS);

	while (pattern_num < 10) {
		snprintf(file_name_buf, buf_len, "%s%d", PATTERN_FILEPATH, pattern_num);
		cnt += sprintf(buf + cnt, "Trying to open %s\n", file_name_buf);

		fp = filp_open(file_name_buf, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			// File open error
			goto pattern_update_exit;
		}
		cnt += sprintf(buf + cnt, "Reading %s\n", file_name_buf);
		memset(read_buf, 0x00, 3);
		pos = 0;
		pattern_header[0] = RAM_ADDR;
		for (i = 1; i < 8; i++) {
			vfs_read(fp, read_buf, 2, &pos);
			pattern_header[i] = (u8)simple_strtoul(read_buf, NULL, 16);
		}
		cnt += sprintf(buf + cnt, "Writing Header 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02x 0x%02X\n",
				pattern_header[0], pattern_header[1],
				pattern_header[2], pattern_header[3],
				pattern_header[4], pattern_header[5],
				pattern_header[6], pattern_header[7]
				);
		ret = i2c_send_buf(c, pattern_header, 8);
		cnt += sprintf(buf + cnt, "Send Buffer Return %d\n", ret);
		pattern_start = ((pattern_header[3] & 0x0F) << 8) + pattern_header[4];
		pattern_length = ((pattern_header[5] & 0x0F) << 8) + pattern_header[6];
		cnt += sprintf(buf + cnt, "Pattern..\n");
		for (i = 0; i < pattern_length; i++) {
			if (i % 20 == 0) {
				memset(pattern_data, 0x00, 23);
				pattern_data[0] = RAM_ADDR;
				pattern_data[1] = (u8)((pattern_start & 0xF00) >> 8);
				pattern_data[2] = (u8)(pattern_start & 0xFF);
				cnt += sprintf(buf + cnt, "0x%02X 0x%02X 0x%02X", pattern_data[0], pattern_data[1], pattern_data[2]);
			}

			vfs_read(fp, read_buf, 2, &pos);
			pattern_data[(i % 20) + 3] = (u8)simple_strtoul(read_buf, NULL, 16);
			cnt += sprintf(buf + cnt, "0x%02X ", pattern_data[(i%20) + 3]);
			if (i % 20 == 19) {
				cnt += sprintf(buf + cnt, " Writing to 0x%02X%02X ", pattern_data[1], pattern_data[2]);
				ret = i2c_send_buf(c, pattern_data, 23);
				pattern_start += 20;
				cnt += sprintf(buf + cnt, "\n");
		cnt += sprintf(buf + cnt, "Send Buffer Return %d\n", ret);
			} else if (i == pattern_length - 1) {
				cnt += sprintf(buf + cnt, " Writing to 0x%02X%02X ", pattern_data[1], pattern_data[2]);
				ret = i2c_send_buf(c, pattern_data, (i % 20) + 4);
				cnt += sprintf(buf + cnt, "\n");
		cnt += sprintf(buf + cnt, "Send Buffer Return %d\n", ret);
			}
		}

		filp_close(fp, NULL);
		pattern_num++;
	}

pattern_update_exit:
	set_fs(fs);
	cnt += sprintf(buf + cnt, "%d Patterns are read.\n", pattern_num - 1);
	return cnt;
}

static DEVICE_ATTR(pattern_update, 0440,
		dw7914_show_pattern_update, NULL);
*/
#ifdef CONFIG_OF
static int dw7914_i2c_parse_dt(struct i2c_client *i2c, struct dw7914_dev *p)
{
	struct device *dev = &i2c->dev;
	struct device_node *np = dev->of_node;
	int ret = -1;
	u32 value;

	if (!np)
		return -1;

	ret = of_property_read_u32(np, "use_en_pin", &value);
	if (ret < 0) {
		dev_err(&i2c->dev, "use_en_pin read error\n");
		p->use_en_pin = -1;
	}
	p->use_en_pin = value;

	/* If you want to use en gpio pin */
	if (p->use_en_pin ) {
		p->enable_pin = of_get_named_gpio(np, "dw7914,en-gpio", 0);
		if (p->enable_pin < 0) {
			printk(KERN_ERR "Looking up %s property in node %s failed %d\n",
					"dw7914,en-gpio", dev->of_node->full_name,
					p->enable_pin);
			p->enable_pin = -1;

			goto error;
		}

		if( !gpio_is_valid(p->enable_pin) ) {
			printk(KERN_ERR "dw7914 enable_pin pin(%u) is invalid\n", p->enable_pin);
			goto error;
		}
	}

	gprintk("p->use_en_pin = %d\n", p->use_en_pin);
	gprintk("p->enable_pin = %d\n", p->enable_pin);

	ret = of_property_read_u32(np, "vd_clamp", &value);
	if (ret < 0) {
		dev_err(&i2c->dev, "vd_clamp read error\n");
		p->vd_clamp = 8000;
	}
	p->vd_clamp = value;
	pr_info("%s vd_clamp:%d\n", __func__, p->vd_clamp);

	ret = of_property_read_u32(np, "trigger_num", &value);
	if (ret < 0) {
		dev_err(&i2c->dev, "trigger_num read error\n");
		p->trigger_num = -1;
	}
	p->trigger_num = value;
	p->trigger_index = 3;
	p->trigger_enable = true;
	pr_info("%s trigger_num:%d trigger_index:%d trigger_enable:%d\n", __func__, p->trigger_num, p->trigger_index, p->trigger_enable);
	return 0;

error:
	gprintk("p->use_en_pin = %d\n", p->use_en_pin);
	gprintk("p->enable_pin = %d\n", p->enable_pin);
	return -1;
}
#else
static int dw7914_i2c_parse_dt(struct i2c_client *i2c, struct dw7914_dev *p)
{
	return NULL;
}
#endif

#endif

static int dw7914_probe(struct i2c_client* client, const struct i2c_device_id* id);
static void dw7914_shutdown(struct i2c_client* client);
static int dw7914_remove(struct i2c_client* client);

#if defined(CONFIG_LGE_VIB)

#ifdef CONFIG_PM
static int dw7914_suspend(struct device *dev)
{
	return 0;
}

static int dw7914_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(dw7914_pm_ops,
		dw7914_suspend, dw7914_resume);

#define DW7914_VIBRATOR_PM_OPS (&dw7914_pm_ops)
#else
#define DW7914_VIBRATOR_PM_OPS NULL
#endif

#ifdef CONFIG_OF
static struct of_device_id dw7914_i2c_dt_ids[] = {
	{ .compatible = "dwanatech,dw7914"},
	{ }
};
#endif
#endif

static const struct i2c_device_id dw7914_id[] = {
	{ "dw7914", 0 },
	{ }
};
#if !defined(CONFIG_LGE_VIB)
static struct i2c_board_info info = {
	I2C_BOARD_INFO("dw7914", DEVICE_ADDR),
};
#endif

static struct i2c_driver dw7914_driver = {
	.probe = dw7914_probe,
	.remove = dw7914_remove,
	.id_table = dw7914_id,
	.shutdown = dw7914_shutdown,
	.driver = {
		.name = "dw7914",
#if defined(CONFIG_LGE_VIB)
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(dw7914_i2c_dt_ids),
#endif
#ifdef CONFIG_PM
		.pm	= DW7914_VIBRATOR_PM_OPS,
#endif
#endif
	},
};

static int dw7914_probe(struct i2c_client* client, const struct i2c_device_id* id)
{
	uint8_t status = 0;
	uint8_t result = 0;
#if defined(CONFIG_LGE_VIB)
	int ret = -1;
#endif

	pr_info("%s : dw7914 Probe\n");
	dw7914 = kzalloc(sizeof(struct dw7914_dev), GFP_KERNEL);
	if (!dw7914)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		DbgOut((DBL_ERROR, "dw7914_probe: i2c_check_functionality failed.\n"));
		return -ENODEV;
	}
#if defined(CONFIG_LGE_VIB)
	if (client->dev.of_node) {
		gprintk("Read enable pin from device tree\n");
		ret = dw7914_i2c_parse_dt(client, dw7914);
		if( ret < 0 ) {
			dev_err(&client->dev, "%s: dt parse error, use_en_pin = %d,"\
					"enable_pin = %d\n", __func__,
					dw7914->use_en_pin, dw7914->enable_pin);
		}
	} else {
		dw7914->use_en_pin = -1;
	}

	if (dw7914->use_en_pin == -1) {
		dev_err(&client->dev, "%s: use_en_pin error\n", __func__);
		ret = -EINVAL;
		goto error1;
	}


	if (dw7914->use_en_pin > 0) {
		ret = gpio_request(dw7914->enable_pin, "dw7914_en");
		if (ret < 0) {
			dev_err(&client->dev, "%s: dw7914_en gpio pin request error\n", __func__);
			ret = -EINVAL;
			goto error1;
		}

		ret = gpio_direction_output(dw7914->enable_pin, 1);
		if (ret < 0) {
			dev_err(&client->dev, "enable pin level set failed");
			ret = -EIO;
			goto error2;
		}
		gpio_set_value(dw7914->enable_pin,1);
	}
	i2c_set_clientdata(client, dw7914);
#endif
	dw7914->i2c = client;
#if defined(CONFIG_LGE_VIB)
	// code here!!
	dw7914_poweron(RTP_MODE);	// rtp mode setting

	ret = device_create_file(&client->dev, &dev_attr_index_reg);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs index_reg file --> failed");
		ret = -EIO;
		goto error2;
	}
	ret = device_create_file(&client->dev, &dev_attr_trigger_test);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs trigger_test file --> failed");
		ret = -EIO;
		goto error2;
	}
	dw7914_trigger_test_init(&client->dev);
/*
	ret = device_create_file(&client->dev, &dev_attr_trig1_r);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs trig1_r file --> failed");
		ret = -EIO;
		goto error2;
	}
	ret = device_create_file(&client->dev, &dev_attr_trig1_f);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs trig1_f file --> failed");
		ret = -EIO;
		goto error2;
	}
*/
	ret = device_create_file(&client->dev, &dev_attr_trig2_r);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs trig2_r file --> failed");
		ret = -EIO;
		goto error2;
	}
	ret = device_create_file(&client->dev, &dev_attr_trig2_f);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs trig2_f file --> failed");
		ret = -EIO;
		goto error2;
	}
/*	ret = device_create_file(&client->dev, &dev_attr_pattern_update);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs pattern_update file --> failed");
		ret = -EIO;
		goto error2;
	}
*/
	ret = device_create_file(&client->dev, &dev_attr_ram_highaddr);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs pattern_update file --> failed");
		ret = -EIO;
		goto error2;
	}
	ret = device_create_file(&client->dev, &dev_attr_ram_lowaddr);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs pattern_update file --> failed");
		ret = -EIO;
		goto error2;
	}
	ret = device_create_file(&client->dev, &dev_attr_ram_len);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs pattern_update file --> failed");
		ret = -EIO;
		goto error2;
	}
	ret = device_create_file(&client->dev, &dev_attr_ram_data);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs pattern_update file --> failed");
		ret = -EIO;
		goto error2;
	}
	
	ret = device_create_file(&client->dev, &dev_attr_trig3_r);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs trig3_r file --> failed");
		ret = -EIO;
		goto error2;
	}

	ret = device_create_file(&client->dev, &dev_attr_trig3_f);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs trig3_f file --> failed");
		ret = -EIO;
		goto error2;
	}

	ret = device_create_file(&client->dev, &dev_attr_trig_ctrl);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs ctrl file --> failed");
		ret = -EIO;
		goto error2;
	}

	ret = device_create_file(&client->dev, &dev_attr_trig_index);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs index file --> failed");
		ret = -EIO;
		goto error2;
	}

	ret = device_create_file(&client->dev, &dev_attr_patten_hdr);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs patten_hdr file --> failed");
		ret = -EIO;
		goto error2;
	}

	ret = device_create_file(&client->dev, &dev_attr_patten_data);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs patten_data file --> failed");
		ret = -EIO;
		goto error2;
	}

	ret = device_create_file(&client->dev, &dev_attr_p_data_read_index);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs p_data_read_index file --> failed");
		ret = -EIO;
		goto error2;
	}

	ret = device_create_file(&client->dev, &dev_attr_key_irq_p_update);
	if (ret < 0) {
		dev_err(&client->dev, "create sysfs key_irq_p_update file --> failed");
		ret = -EIO;
		goto error2;
	}
#endif

	if (dw7914->i2c) {

		/* read any pending error codes */
		result = i2c_recv_buf(dw7914->i2c, DW7914_STATUS, &status, sizeof(status));
		if (result != 2) {
			DbgOut((DBL_ERROR, "dw7914_probe: i2c read status failure. result=%d\n", result));
		}

		/* get device id */
		if (2 != i2c_recv_buf(client, DW7914_CHIPID, &dw7914->chipid, 1)) {
			DbgOut((DBL_ERROR, "dw7914_probe: failed to read chipid.\n"));
			/*return -ENODEV;*/
		}

		/* diagnostic */
#if !defined(CONFIG_LGE_VIB)
		dw7914_dump_registers(dw7914);
#endif
	}

	dw7914->buffer[0] = DW7914_RTP_INPUT;

	DbgOut((DBL_VERBOSE, "dw7914_probe.\n"));

	return 0;

#if defined(CONFIG_LGE_VIB)
error1:
	kfree(dw7914);
	return ret;

error2:
	if (dw7914->use_en_pin > 0)
		gpio_free(dw7914->enable_pin);
	kfree(dw7914);
	return ret;
#endif
}

static void dw7914_shutdown(struct i2c_client* client)
{
	DbgOut((DBL_VERBOSE, "dw7914_shutdown.\n"));

	return;
}

static int dw7914_remove(struct i2c_client* client)
{
#if defined(CONFIG_LGE_VIB)
	struct dw7914_dev *p = i2c_get_clientdata(client);
#endif

	DbgOut((DBL_VERBOSE, "dw7914_remove.\n"));

#if defined(CONFIG_LGE_VIB)
	if (dw7914->use_en_pin > 0) {
		gpio_set_value(p->enable_pin,0);
		gpio_free(p->enable_pin);
	}
	device_remove_file(&client->dev, &dev_attr_index_reg);
	device_remove_file(&client->dev, &dev_attr_trigger_test);
	device_remove_file(&client->dev, &dev_attr_trig1_r);
	device_remove_file(&client->dev, &dev_attr_trig1_f);
	device_remove_file(&client->dev, &dev_attr_trig2_r);
	device_remove_file(&client->dev, &dev_attr_trig2_f);
//	device_remove_file(&client->dev, &dev_attr_pattern_update);
	device_remove_file(&client->dev, &dev_attr_ram_highaddr);
	device_remove_file(&client->dev, &dev_attr_ram_lowaddr);
	device_remove_file(&client->dev, &dev_attr_ram_len);
	device_remove_file(&client->dev, &dev_attr_ram_data);
	device_remove_file(&client->dev, &dev_attr_trig3_r);
	device_remove_file(&client->dev, &dev_attr_trig3_f);
	device_remove_file(&client->dev, &dev_attr_trig_ctrl);
	device_remove_file(&client->dev, &dev_attr_trig_index);
	device_remove_file(&client->dev, &dev_attr_patten_hdr);
	device_remove_file(&client->dev, &dev_attr_patten_data);
	device_remove_file(&client->dev, &dev_attr_p_data_read_index);
	device_remove_file(&client->dev, &dev_attr_key_irq_p_update);

	i2c_set_clientdata(client, NULL);
	kfree(p);
#endif

	return 0;
}
/*
 ** TouchSense SPI Functions
 */

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpDisable(VibeUInt8 nActuatorIndex)
{
	DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_AmpDisable.\n"));

	if (dw7914->buffering) {
		dw7914->buffering = -1;
		ImmVibeSPI_ForceOut_SetSamples(nActuatorIndex, 8, VIBE_OUTPUT_SAMPLE_SIZE, 0);
	}
	return VIBE_S_SUCCESS;
}

static void i2c_commands(struct i2c_client *i2c, struct i2c_msg *msgs, u8 *command, int size)
{
	int i, c, count = size/2;

	for (i = 0, c = 0; c < size; c+=2, i++) {
		msgs[i].flags = 0;
		msgs[i].addr = i2c->addr;
		msgs[i].buf = command + c;
		msgs[i].len = 2;
	}
	if (count != i2c_transfer(i2c->adapter, msgs, count)) {
		DbgOut((DBL_ERROR, "failed to send dw7914 command sequence\n"));
	}
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_AmpEnable(VibeUInt8 nActuatorIndex)
{
	int result;
	uint8_t status;
#if defined(CONFIG_LGE_VIB)
	unsigned char data;
#endif

	DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_AmpEnable.\n"));

	dw7914->buffering = 1;
	dw7914->position = 1;

	if (dw7914->i2c) {

		/* read any pending error codes */
		result = i2c_recv_buf(dw7914->i2c, DW7914_STATUS, &status, sizeof(status));
		if (result != 2) {
			DbgOut((DBL_ERROR, "dw7914: i2c read status failure. result=%d\n", result));
			//return VIBE_E_FAIL;
		}
		if (status & 0x0F) {
			DbgOut((DBL_ERROR, "dw7914: status error! (0x%02x)\n", status));
			//return VIBE_E_FAIL;
		}
#if !defined(CONFIG_LGE_VIB)// move to dw7914_poweron function for default setting
		// batch dw7914 configuration sequence
		{
			u8 command[] = {
				DW7914_MODE, DW7914_MODE_RTP,
				DW7914_VD_CLAMP, DW7914_VD_CLAMP_mV(8000),
				DW7914_BOOST_OPTION, DW7914_BOOST_OPTION_BST_OFFSET_mV(400) | DW7914_BOOST_OPTION_BST_ILIMIT_35,
				DW7914_PWM_FREQ, CHOSEN_SAMPLE_FREQUENCY,
				DW7914_PLAYBACK, DW7914_PLAYBACK_GO
			};
			struct i2c_msg msgs[sizeof(command)/2];
			i2c_commands(dw7914->i2c, msgs, command, sizeof(command));
		}
#else
		data = DW7914_PLAYBACK_GO;
		if (VIBE_S_SUCCESS != I2CWrite(DW7914_PLAYBACK, 1, &data))
		{
			DbgOut((DBL_ERROR, "I2CWrite failed to send DW7914_PLAYBACK\n"));
		}
#endif
	}
	//DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_AmpEnable. End\n"));
	return VIBE_S_SUCCESS;
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Initialize(void)
{

	int retVal = 0;
#if defined(CONFIG_LGE_VIB)
	DbgOut((DBL_ERROR, "ImmVibeSPI_ForceOut_Initialize enter.\n"));
	retVal = i2c_add_driver(&dw7914_driver);
	if (retVal) {
		DbgOut((DBL_ERROR, "ImmVibeSPI_ForceOut_Initialize: Cannot add driver.\n"));
		return VIBE_E_FAIL;
	}
	return VIBE_S_SUCCESS;
#else
	struct i2c_adapter* adapter = 0;
	struct i2c_client* client = 0;

	adapter = i2c_get_adapter(DEVICE_BUS);
	if (!adapter) {
		DbgOut((DBL_ERROR, "ImmVibeSPI_ForceOut_Initialize: I2C Adapter not found.\n"));
		return VIBE_E_FAIL;
	}

	client = i2c_new_device(adapter, &info);
	if (!client) {
		DbgOut((DBL_ERROR, "ImmVibeSPI_ForceOut_Initialize: Cannot create new device.\n"));
		return VIBE_E_FAIL;
	}

	return VIBE_S_SUCCESS;
#endif
}

IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_Terminate(void)
{
	DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_Terminate.\n"));
	i2c_del_driver(&dw7914_driver);
	return VIBE_S_SUCCESS;
}

/*
 ** Called by the real-time loop to set the force
 */
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetSamples(VibeUInt8 nActuatorIndex, VibeUInt16 nOutputSignalBitDepth, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
#if 0
	if (pForceOutputBuffer) {
		char buffer[VIBE_OUTPUT_SAMPLE_SIZE*3];
		int i, count = nBufferSizeInBytes / (nOutputSignalBitDepth / 8);
		for (i = 0; i < count; i++) {
			sprintf(buffer + (i * 3), " %02hhx", pForceOutputBuffer[i]);
		}
		DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_SetSamples:%s\n", buffer));
	}
#endif

#ifdef IMMVIBESPI_USE_BUFFERFULL
	/* write zeros to keep hardware buffer full and in sync with driver */
	if (nBufferSizeInBytes != VIBE_OUTPUT_SAMPLE_SIZE) {
		static VibeInt8 buffer[VIBE_OUTPUT_SAMPLE_SIZE] = {0,};
		pForceOutputBuffer = buffer;
		nBufferSizeInBytes = sizeof(buffer);
		nOutputSignalBitDepth = 8;
	}
#endif

	/* buffer initial samples from daemon to compensate for system delay */
	if (dw7914->buffering >= 0 && pForceOutputBuffer && nBufferSizeInBytes > 0 /*&& dw7914->position + nBufferSizeInBytes + 1 <= sizeof(dw7914->buffer)*/) {
		DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_SetSamples: buffering position=%d buffer=%d sizeof=%d\n", dw7914->position, nBufferSizeInBytes, sizeof(dw7914->buffer)));
#ifdef UPSCALE_12_TO_24khz
		{   s8 sample1;
			s8 *src = pForceOutputBuffer;
			s8 *dest = dw7914->buffer + dw7914->position;
			s8 *last = pForceOutputBuffer + (nBufferSizeInBytes - 1);
			while (src < last) {
				*dest = sample1 = *src; src++; dest++;
				*dest = sample1 + ((*src - sample1) >> 1); dest++;
			}
			/* copy last element, since there is no element after to average */
			dest[0] = src[0];
			dest[1] = src[0];
		}
		dw7914->position += nBufferSizeInBytes << 1;
#else
		memcpy(dw7914->buffer + dw7914->position, pForceOutputBuffer, nBufferSizeInBytes);
		dw7914->position += nBufferSizeInBytes;
#endif
		/* filling buffer */
		if (dw7914->buffering == 1 && dw7914->position < sizeof(dw7914->buffer))
		{
			return VIBE_S_SUCCESS;
		}
		if (dw7914->buffering > 1 && dw7914->position < 240)
		{
			return VIBE_S_SUCCESS;
		}
	}

	/* estimate dw7914 sample time */
	if (dw7914->buffering) {
		dw7914->buffering = 2;
		dw7914->time = ktime_get();
	} else {
		dw7914->time = ktime_add_ms(dw7914->time, g_nTimerPeriodMs);
	}

	if (dw7914->i2c) {

		/* batch multiple i2c transactions for faster transmission */
		if (dw7914->position > 1)
		{
			struct i2c_msg msgs[2];
			// send 'go' to wake up dw7914 fifo logic
			u8 command[2] = {DW7914_PLAYBACK, DW7914_PLAYBACK_GO};
			msgs[0].flags = 0;
			msgs[0].addr = dw7914->i2c->addr;
			msgs[0].buf = dw7914->buffer;
			msgs[0].len = dw7914->position;
			msgs[1].flags = 0;
			msgs[1].addr = dw7914->i2c->addr;
			msgs[1].buf = command;
			msgs[1].len = sizeof(command);
			if (2 != i2c_transfer(dw7914->i2c->adapter, msgs, 2)) {
				DbgOut((DBL_ERROR, "ImmVibeSPI_ForceOut_SetSamples: i2c write failed\n"));
				//return VIBE_E_FAIL;
			}
			//pr_info("ImmVibeSPI_ForceOut_SetSamples\n");
		}
	}

	/* buffer is empty */
	dw7914->position = 1;

	//DbgOut((DBL_INFO, "ImmVibeSPI_ForceOut_SetSamples: done\n"));

	return VIBE_S_SUCCESS;
}

VibeStatus I2CWrite(unsigned char address, VibeUInt16 nBufferSizeInBytes, VibeInt8* pForceOutputBuffer)
{
	unsigned char buf[VIBE_OUTPUT_SAMPLE_SIZE+1];

	if (!dw7914->i2c) {
		return VIBE_E_FAIL;
	}

	if (nBufferSizeInBytes > VIBE_OUTPUT_SAMPLE_SIZE) {
		return VIBE_E_FAIL;
	}

	buf[0] = address;
	memcpy(buf + 1, pForceOutputBuffer, nBufferSizeInBytes);
	if (1 != i2c_send_buf(dw7914->i2c, buf, nBufferSizeInBytes+1)) {
		return VIBE_E_FAIL;
	}

	return VIBE_S_SUCCESS;
}

/*
 ** Called to set force output frequency parameters
 */
IMMVIBESPIAPI VibeStatus ImmVibeSPI_ForceOut_SetFrequency(VibeUInt8 nActuatorIndex, VibeUInt16 nFrequencyParameterID, VibeUInt32 nFrequencyParameterValue)
{
	return VIBE_S_SUCCESS;
}

/*
 ** Called to get the device name (device name must be returned as ANSI char)
 */
IMMVIBESPIAPI VibeStatus ImmVibeSPI_Device_GetName(VibeUInt8 nActuatorIndex, char *szDevName, int nSize)
{
#if defined(CONFIG_LGE_VIB)
	return VIBE_S_SUCCESS;
#else
	char szRevision[24];
	if ((!szDevName) || (nSize < 1)) {
		return VIBE_E_FAIL;
	}

	/* Append revision number to the device name */
	sprintf(szRevision, DEVICE_NAME "-DW7914-%02x", dw7914->chipid);
	if (strlen(szRevision) + strlen(szDevName) < nSize - 1) {
		strcat(szDevName, szRevision);
	}

	/* Make sure the string is NULL terminated */
	szDevName[nSize - 1] = '\0';

	return VIBE_S_SUCCESS;
#endif
}

#ifdef IMMVIBESPI_DEVICE_GETSTATUS_SUPPORT
/*
 ** Called by the TouchSense Player Service/Daemon when an application calls
 ** ImmVibeGetDeviceCapabilityInt32 with VIBE_DEVCAPTYPE_DRIVER_STATUS.
 ** The TouchSense Player does not interpret the returned status.
 */
static VibeStatus ImmVibeSPI_Device_GetStatus(VibeUInt8 nActuatorIndex)
{
	return VIBE_S_SUCCESS;
}
EXPORT_SYMBOL(ImmVibeSPI_Device_GetStatus);
#endif

#ifdef IMMVIBESPI_USE_BUFFERFULL
/*
 ** Check if the amplifier sample buffer is full (not ready for more data).
 */


IMMVIBESPIAPI int ImmVibeSPI_ForceOut_BufferFull(void)
{
#if 0 
	return !dw7914->buffering && ktime_before(ktime_get(), dw7914->time);
#else
   uint16_t fifo_status;
   uint8_t bytes[2] = {0,};
   // read two bytes
   i2c_recv_buf(dw7914->i2c, DW7914_FIFO_STATUS, bytes, sizeof(bytes));
   // flip order of bytes
   fifo_status = ((bytes[0] & 0x3F) << 8) | bytes[1];
   // true: fifo has enough data, tspdrv timer can sleep
   return fifo_status > DW7914_FIFOFULL_TARGET;
#endif
}
#endif

