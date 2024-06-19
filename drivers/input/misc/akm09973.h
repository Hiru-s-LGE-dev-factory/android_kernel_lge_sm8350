/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2020, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _UAPI_MISC_AKM09973_H
#define _UAPI_MISC_AKM09973_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define AKM_INPUT_DEVICE_NAME	"hall_sensor"
#define AKM_MISCDEV_NAME		"akm09973_dev"

/* Device specific constant values */
#define AK09973_REG_WIA                 0x00


#define AK09973_REG_WORD_ST             0x10
#define AK09973_REG_ST_XYZ              0x17
#define AK09973_REG_ST_V                0x18

#define AK09973_REG_CNTL1               0x20
#define AK09973_REG_CNTL2               0x21
#define AK09973_REG_CNTL1               0x20
#define AK09973_REG_CNTL2               0x21
#define AK09973_REG_THX                 0x22
#define AK09973_REG_THY                 0x23
#define AK09973_REG_THZ                 0x24
#define AK09973_REG_THV                 0x25
#define AK09973_REG_SRST                0x30

#define AK09973_REG_THRESHOLD           AK09973_REG_THX

#define AK09973_RESET_DATA              0x01

#define AK09973_WIA1_VALUE              0x48
#define AK09973_WIA2_VALUE              0xC1

#define AK09973_MODE_POWERDOWN          0x00
#define AK09973_MODE_SINGLE_MEASUREMENT 0x01 /* Single Mode */


#define AK09973_MODE_CONTINUOUS_5HZ     0x02 /* 5Hz */
#define AK09973_MODE_CONTINUOUS_10HZ    0x04 /* 10Hz */
#define AK09973_MODE_CONTINUOUS_20HZ    0x06 /* 20Hz */
#define AK09973_MODE_CONTINUOUS_50HZ    0x08 /* 50Hz */
#define AK09973_MODE_CONTINUOUS_100HZ   0x0A /* 100Hz */
#define AK09973_MODE_CONTINUOUS_500HZ   0x0C /* 500Hz */
#define AK09973_MODE_CONTINUOUS_1000HZ  0x0E /* 1000Hz */
#define AK09973_MODE_CONTINUOUS_2000HZ  0x10 /* 200Hz */

#define AKM_SENSOR_INFO_SIZE        2
#define AKM_SENSOR_CONF_SIZE        3
#define AKM_SENSOR_DATA_SIZE        7
#define AKM_SENSOR_DATA_SIZE_V      5


#define AKM_SENSOR_CONTROL_SIZE      2
#define AKM_SENSOR_THRESHOLD_SIZE   16

#define AK09973_MODE_POS        0
#define AK09973_MODE_MSK        0x1F
#define AK09973_MODE_REG        AK09973_REG_CNTL2

#define AK09973_SDR_MODE_POS    5
#define AK09973_SDR_MODE_MSK    0x20
#define AK09973_SDR_MODE_REG    AK09973_REG_CNTL2

#define AK09973_SMR_MODE_POS    6
#define AK09973_SMR_MODE_MSK    0x40
#define AK09973_SMR_MODE_REG    AK09973_REG_CNTL2

#define AKM_DRDY_IS_HIGH(x)         ((x) & 0x01)
#define AKM_ERR_IS_HIGH(x)          ((x) & 0x20)
#define AKM_DOR_IS_HIGH(x)          ((x) & 0x40)

#define AK09973_SENS_Q16    ((int32_t)(72090))  /* 1.1uT in Q16 format */

struct akm09973_threshold {
	union {
		struct {
			uint16_t bopx, brpx;
			uint16_t bopy, brpy;
			uint16_t bopz, brpz;
			uint16_t bopv, brpv;
		};
		uint8_t reg[AKM_SENSOR_THRESHOLD_SIZE];
	};
};

struct akm09973_control {
	uint8_t	drdyen;
	uint8_t	swxen;
	uint8_t	swyen;
	uint8_t	swzen;
	uint8_t	swven;
	uint8_t	erren;
	uint8_t	polx;
	uint8_t	poly;
	uint8_t	polz;
	uint8_t	polv;
};

/* ioctl numbers */
#define AKM_SENSOR 0xD4
/* AFU devices */
#define AKM_IOC_SET_ACTIVE			_IOW(AKM_SENSOR, 0x00, uint8_t)
#define AKM_IOC_SET_MODE			_IOW(AKM_SENSOR, 0x01, uint16_t)
#define AKM_IOC_SET_PRESS			_IOW(AKM_SENSOR, 0x02, uint8_t)
#define AKM_IOC_GET_SENSEDATA		_IOR(AKM_SENSOR, 0x10, int32_t[4])
#define AKM_IOC_GET_SENSSMR			_IOR(AKM_SENSOR, 0x11, uint8_t)
#define AKM_IOC_SET_THRESHOLD    	_IOW(AKM_SENSOR, 0x12, struct akm09973_threshold)
#define AKM_IOC_SET_CONTROL_REG    	_IOW(AKM_SENSOR, 0x13, struct akm09973_control)
#define AKM_IOC_GET_THRESHOLD    	_IOW(AKM_SENSOR, 0x15, struct akm09973_threshold)
#define AKM_IOC_GET_CONTROL_REG    	_IOW(AKM_SENSOR, 0x16, struct akm09973_control)

#endif /* _UAPI_MISC_AKM09973_H */
