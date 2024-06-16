/*
 * Core Driver for ice40 Accessory Communication Interface
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
 *      PROJECT:   ice40 driver
 *      LANGUAGE:  ANSI C
 */

#ifndef ICE40_CORE_H
#define ICE40_CORE_H

#include <linux/ice40/ice40-reg.h>

/* ========================================================================== */
/* Includes and Defines */

/* ======================================================================== */
/* define for regmap range usage in older kernel versions */
#define regmap_reg_range(low, high) { .range_min = low, .range_max = high, }

#define REG_DATA_MIN               0x00
#define MOTOR_REG_DATA_MAX         ARRAY_SIZE(ice40_motor_regmap_defaults)
#define MOTOR_REG_WRITE_DATA_MAX   ICE40_R_DEC_DUTY_4

/*! global driver data structure */
struct ice40 {
    /*! device struct */
    struct device *dev;
    /*! register-map for master */
    struct regmap *regmap;
    /*! I2C client for master */
    struct i2c_client *client_master;
    /*! read length parameter for master register read through sysfs files */
    u8 reg_rd_len_master;
    /*! read address parameter for master register read through sysfs files */
    u8 reg_rd_addr_master;
    /*! global driver ice40 enable state */
    bool ice40_on;
    struct mutex ice40_i2c_lock;
};

static const struct regmap_range ice40_ranges[] = {
	regmap_reg_range(REG_DATA_MIN, MOTOR_REG_DATA_MAX),
};

static const struct regmap_access_table ice40_table = {
	.yes_ranges = ice40_ranges,
	.n_yes_ranges = ARRAY_SIZE(ice40_ranges),
};

static const struct regmap_config ice40_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_table = &ice40_table,
	.wr_table = &ice40_table,
	.rd_table = &ice40_table,
	.reg_defaults = ice40_motor_regmap_defaults,
	.num_reg_defaults = ARRAY_SIZE(ice40_motor_regmap_defaults),
	.max_register = MOTOR_REG_DATA_MAX-1,
	/* FIXME: added to suppress errors when reading test regs */
	.cache_type = REGCACHE_RBTREE,
	.can_multi_write = true,
};

#endif /* ICE40_CORE_H */
