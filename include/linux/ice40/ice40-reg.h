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
 * Revision Record
 *   V0.1. 2020.02.24 Create Reg map File
*/

#include <linux/regmap.h>

#define ICE40_R_CTRL                    0x00
#define ICE40_R_FH_TIME_CNT_15_8        0x01
#define ICE40_R_FH_TIME_CNT_7_0         0x02
#define ICE40_R_FL_TIME_CNT_19_16       0x03
#define ICE40_R_FL_TIME_CNT_15_8        0x04
#define ICE40_R_FL_TIME_CNT_7_0         0x05
#define ICE40_R_ACC_RATE0               0x06
#define ICE40_R_ACC_RATE1               0x07
#define ICE40_R_ACC_RATE2               0x08
#define ICE40_R_ACC_RATE3               0x09
#define ICE40_R_ACC_RATE4               0x0A
#define ICE40_R_DEC_RATE0               0x0B
#define ICE40_R_DEC_RATE1               0x0C
#define ICE40_R_DEC_RATE2               0x0D
#define ICE40_R_DEC_RATE3               0x0E
#define ICE40_R_DEC_RATE4               0x0F
#define ICE40_R_ACC_CNT0_15_8           0x10
#define ICE40_R_ACC_CNT0_7_0            0x11
#define ICE40_R_ACC_CNT1_15_8           0x12
#define ICE40_R_ACC_CNT1_7_0            0x13
#define ICE40_R_ACC_CNT2_15_8           0x14
#define ICE40_R_ACC_CNT2_7_0            0x15
#define ICE40_R_ACC_CNT3_15_8           0x16
#define ICE40_R_ACC_CNT3_7_0            0x17
#define ICE40_R_ACC_CNT4_15_8           0x18
#define ICE40_R_ACC_CNT4_7_0            0x19
#define ICE40_R_DEC_CNT0_15_8           0x1A
#define ICE40_R_DEC_CNT0_7_0            0x1B
#define ICE40_R_DEC_CNT1_15_8           0x1C
#define ICE40_R_DEC_CNT1_7_0            0x1D
#define ICE40_R_DEC_CNT2_15_8           0x1E
#define ICE40_R_DEC_CNT2_7_0            0x1F
#define ICE40_R_DEC_CNT3_15_8           0x20
#define ICE40_R_DEC_CNT3_7_0            0x21
#define ICE40_R_DEC_CNT4_15_8           0x22
#define ICE40_R_DEC_CNT4_7_0            0x23
#define ICE40_R_TOTAL_PPS_15_8          0x24
#define ICE40_R_TOTAL_PPS_7_0           0x25
#define ICE40_R_DEC_DUTY_1_0            0x26
#define ICE40_R_DEC_DUTY_3_2            0x27
#define ICE40_R_DEC_DUTY_4              0x28
#define ICE40_R_AUTO_CNT_W_23_16        0x29
#define ICE40_R_AUTO_CNT_W_15_8         0x2A
#define ICE40_R_AUTO_CNT_W_7_0          0x2B
#define ICE40_R_AUTO_CNT_REC_23_16      0x2C
#define ICE40_R_AUTO_CNT_REC_15_8       0x2D
#define ICE40_R_AUTO_CNT_REC_7_0        0x2E
#define ICE40_R_CURRENT_PPS_15_8        0x2F
#define ICE40_R_CURRENT_PPS_7_0         0x30

static struct reg_default ice40_motor_regmap_defaults[] = {
	{ ICE40_R_CTRL,                     0x02 }, //  1: 0x00 - 0x0000 0010
	{ ICE40_R_FH_TIME_CNT_15_8,         0x56 }, //  2: 0x01 - 0x0101 0110
	{ ICE40_R_FH_TIME_CNT_7_0,          0x3A }, //  3: 0x02 - 0x0011 1010
	{ ICE40_R_FL_TIME_CNT_19_16,        0x00 }, //  4: 0x03 - 0x0000 0000
	{ ICE40_R_FL_TIME_CNT_15_8,         0x68 }, //  5: 0x04 - 0x0110 1000
	{ ICE40_R_FL_TIME_CNT_7_0,          0x2A }, //  6: 0x05 - 0x0010 1010
	{ ICE40_R_ACC_RATE0,                0x59 }, //  7: 0x06 - 0x0101 1001
	{ ICE40_R_ACC_RATE1,                0x39 }, //  8: 0x07 - 0x0011 1001
	{ ICE40_R_ACC_RATE2,                0x2A }, //  9: 0x08 - 0x0010 1010
	{ ICE40_R_ACC_RATE3,                0x1F }, // 10: 0x09 - 0x0001 1111
	{ ICE40_R_ACC_RATE4,                0x17 }, // 11: 0x0A - 0x0001 0111
	{ ICE40_R_DEC_RATE0,                0x17 }, // 12: 0x0B - 0x0001 0111
	{ ICE40_R_DEC_RATE1,                0x1F }, // 13: 0x0C - 0x0001 1111
	{ ICE40_R_DEC_RATE2,                0x2A }, // 14: 0x0D - 0x0010 1010
	{ ICE40_R_DEC_RATE3,                0x39 }, // 15: 0x0E - 0x0011 1001
	{ ICE40_R_DEC_RATE4,                0x59 }, // 16: 0x0F - 0x0101 1001
	{ ICE40_R_ACC_CNT0_15_8,            0x00 }, // 17: 0x10 - 0x0000 0000
	{ ICE40_R_ACC_CNT0_7_0,             0x05 }, // 18: 0x11 - 0x0000 0101
	{ ICE40_R_ACC_CNT1_15_8,            0x00 }, // 19: 0x12 - 0x0000 0000
	{ ICE40_R_ACC_CNT1_7_0,             0x1E }, // 20: 0x13 - 0x0001 1110
	{ ICE40_R_ACC_CNT2_15_8,            0x00 }, // 21: 0x14 - 0x0000 0000
	{ ICE40_R_ACC_CNT2_7_0,             0x1E }, // 22: 0x15 - 0x0001 1110
	{ ICE40_R_ACC_CNT3_15_8,            0x00 }, // 23: 0x16 - 0x0000 0000
	{ ICE40_R_ACC_CNT3_7_0,             0x1E }, // 24: 0x17 - 0x0001 1110
	{ ICE40_R_ACC_CNT4_15_8,            0x00 }, // 25: 0x18 - 0x0000 0000
	{ ICE40_R_ACC_CNT0_7_0,             0x1E }, // 26: 0x19 - 0x0001 1110
	{ ICE40_R_DEC_CNT0_15_8,            0x00 }, // 27: 0x1A - 0x0000 0000
	{ ICE40_R_DEC_CNT0_7_0,             0x1E }, // 28: 0x1B - 0x0001 1110
	{ ICE40_R_DEC_CNT1_15_8,            0x00 }, // 29: 0x1C - 0x0000 0000
	{ ICE40_R_DEC_CNT1_7_0,             0x1E }, // 30: 0x1D - 0x0001 1110
	{ ICE40_R_DEC_CNT2_15_8,            0x00 }, // 31: 0x1E - 0x0000 0000
	{ ICE40_R_DEC_CNT2_7_0,             0x1E }, // 32: 0x1F - 0x0001 1110
	{ ICE40_R_DEC_CNT3_15_8,            0x00 }, // 33: 0x20 - 0x0000 0000
	{ ICE40_R_DEC_CNT3_7_0,             0x1E }, // 34: 0x21 - 0x0001 1110
	{ ICE40_R_DEC_CNT4_15_8,            0x00 }, // 35: 0x22 - 0x0000 0000
	{ ICE40_R_DEC_CNT4_7_0,             0x05 }, // 36: 0x23 - 0x0000 0101
	{ ICE40_R_TOTAL_PPS_15_8,           0x13 }, // 37: 0x24 - 0x0001 0011
	{ ICE40_R_TOTAL_PPS_7_0,            0x38 }, // 38: 0x25 - 0x0011 1000
	{ ICE40_R_DEC_DUTY_1_0,             0x88 }, // 39: 0x26 - 0x1000 1000
	{ ICE40_R_DEC_DUTY_3_2,             0x88 }, // 40: 0x27 - 0x1000 1000
	{ ICE40_R_DEC_DUTY_4,               0x08 }, // 41: 0x28 - 0x1000 1000
	{ ICE40_R_AUTO_CNT_W_23_16,         0x00 }, // 42: 0x29 - 0x0000 0000
	{ ICE40_R_AUTO_CNT_W_15_8,          0x00 }, // 43: 0x2A - 0x0000 0000
	{ ICE40_R_AUTO_CNT_W_7_0,           0x64 }, // 44: 0x2B - 0x0110 0100
	{ ICE40_R_AUTO_CNT_REC_23_16,       0x00 }, // 45: 0x2C - 0x0000 0000
	{ ICE40_R_AUTO_CNT_REC_15_8,        0x00 }, // 46: 0x2D - 0x0000 0000
	{ ICE40_R_AUTO_CNT_REC_7_0,         0x00 }, // 47: 0x2E - 0x0000 0000
	{ ICE40_R_CURRENT_PPS_15_8,         0x00 }, // 48: 0x2F - 0x0000 0000
	{ ICE40_R_CURRENT_PPS_7_0,          0x00 }, // 49: 0x30 - 0x0000 0000
};
