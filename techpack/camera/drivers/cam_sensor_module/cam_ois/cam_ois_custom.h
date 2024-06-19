#ifndef CAM_OIS_CUSTOM_H
#define CAM_OIS_CUSTOM_H

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/file.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>

#include "cam_debug_util.h"
#include <cam_sensor_cmn_header.h>
#include "cam_ois_dev.h"

#define PROPERTY_VALUE_MAX        (92)

//OIS ERROR CODE
#define OIS_STOP_TEST                0
#define OIS_SUCCESS                  0
#define OIS_FAIL                    -1
#define OIS_INIT                    -2
#define OIS_INIT_CHECKSUM_ERROR     -3
#define OIS_INIT_EEPROM_ERROR       -4
#define OIS_INIT_I2C_ERROR          -5
#define OIS_INIT_TIMEOUT            -6
#define OIS_INIT_DOWNLOAD_ERROR     -7

uint32_t cam_ois_selftest_get(void);
void cam_ois_selftest_set(uint32_t result);
int cam_ois_poll_ready(uint32_t addr, uint32_t sts);

int ois_read16(struct cam_ois_ctrl_t *o_ctrl, uint16_t addr, uint16_t *ReadData);
int32_t ois_read32(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t *ReadData);
int ois_write16(struct cam_ois_ctrl_t *o_ctrl, uint16_t addr, uint16_t data_wr);
int ois_write24(struct cam_ois_ctrl_t *o_ctrl, uint8_t addr, uint32_t data_wr);
int32_t ois_write32(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data_wr);
int ois_spcl_wrt(struct cam_ois_ctrl_t *o_ctrl, uint8_t addr, uint8_t  data_wr);
int32_t ois_cnt_wrt(struct cam_ois_ctrl_t *o_ctrl, uint8_t *data, uint16_t num_byte);
int32_t e2p_read8(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t e2p_addr, uint16_t *e2p_data);
int e2p_cnt_rd(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t e2p_addr, uint8_t *e2p_data, uint32_t num_byte);
int e2p_write16(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint16_t e2p_addr, uint16_t  data_wr);
int32_t e2p_write32(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t e2p_addr, uint32_t data_wr);
int verify_e2p_chksum(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t st_addr, uint32_t e2p_size, uint32_t chk_addr);

typedef struct {
	int16_t gyro[2];
	int16_t target[2];
	int16_t offset[2];
	uint8_t is_stable;
} sensor_ois_stat_t;

#endif
