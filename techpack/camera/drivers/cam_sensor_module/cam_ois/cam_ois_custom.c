#ifndef CAM_OIS_CUSTOM_C
#define CAM_OIS_CUSTOM_C
#endif

#include "cam_ois_custom.h"

extern int32_t cam_ois_bu6_init(struct cam_ois_ctrl_t *o_ctrl);
#ifdef CONFIG_RAINBOWLM_CAMERA
extern int32_t cam_ois_ep3_init(struct cam_ois_ctrl_t *o_ctrl);
#endif

struct device* cam_ois_aat_result;
static struct class *cam_ois_aat_result_class = NULL;
char   aat_selftest_result[PROPERTY_VALUE_MAX] = "000000";

static ssize_t show_ois_aat_selftest_result(struct device *dev, struct device_attribute *attr, char *buf)
{
	CAM_ERR(CAM_OIS, "show_ois_aat_selftest_result: [%s] \n", aat_selftest_result);
	return sprintf(buf, "%s\n", aat_selftest_result);
}
static DEVICE_ATTR(ois_aat_selftest_result, S_IRUGO, show_ois_aat_selftest_result, NULL);

void cam_ois_create_sysfs(void)
{
	if (!cam_ois_aat_result_class) {
		CAM_ERR(CAM_OIS, "create cam_ois_aat_result_class!!");
		cam_ois_aat_result_class = class_create(THIS_MODULE, "ois");
		cam_ois_aat_result = device_create(cam_ois_aat_result_class, NULL,0, NULL, "ois_aat_selftest_result");
		device_create_file(cam_ois_aat_result, &dev_attr_ois_aat_selftest_result);
	}
}

void cam_ois_destroy_sysfs(void)
{
	if (cam_ois_aat_result_class) {
		device_remove_file(cam_ois_aat_result, &dev_attr_ois_aat_selftest_result);
		device_destroy(cam_ois_aat_result_class, 0);
		class_destroy(cam_ois_aat_result_class);
		cam_ois_aat_result_class = NULL;
		CAM_ERR(CAM_OIS, "del cam_ois_aat_result_class!!");
	}
}

uint32_t cam_ois_selftest_get(void)
{
	uint32_t out = 0;
	int      i;

	for (i = 0; i < 6; i++)
		if (aat_selftest_result[5 - i] == '1')
			out |= (1 << i);

	CAM_ERR(CAM_OIS," result = %s", aat_selftest_result);

	return out;
}

void cam_ois_selftest_set(uint32_t result)
{
	uint8_t rc[6];
	int     i;

	for (i = 0; i < 6; i++)
		rc[i] = (result & (1 << i)) >> i;

	sprintf(aat_selftest_result, "%d%d%d%d%d%d", rc[5], rc[4], rc[3], rc[2], rc[1], rc[0]);

	CAM_ERR(CAM_OIS," result = %s", aat_selftest_result);
}

int32_t cam_ois_init(struct cam_ois_ctrl_t *o_ctrl)
{
	int16_t rc = OIS_FAIL;

#ifdef CONFIG_BLM_CAMERA
	rc = cam_ois_bu6_init(o_ctrl);
#elif CONFIG_RAINBOWLM_CAMERA
	if (o_ctrl->soc_info.index == 0) {
		rc = cam_ois_bu6_init(o_ctrl);
	} else {
		rc = cam_ois_ep3_init(o_ctrl);
	}
#endif

	return rc;
}

int ois_read16(struct cam_ois_ctrl_t *o_ctrl, uint16_t addr, uint16_t *ReadData)
{
	int     rc = 0;
	uint8_t buf[2];

	rc = camera_io_dev_read_seq(
		&(o_ctrl->io_master_info),
		addr,
		buf,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_BYTE,
		2);
	*ReadData = (buf[0] << 8) | buf[1];

	return rc;
}

int32_t ois_read32(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t *ReadData)
{
	int32_t rc = 0;
	uint8_t buf[4];

	rc = camera_io_dev_read_seq(
		&(o_ctrl->io_master_info),
		addr,
		buf,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_BYTE,
		4);
	*ReadData = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	return rc;
}

int ois_write16(struct cam_ois_ctrl_t *o_ctrl, uint16_t addr, uint16_t data_wr)
{
	int                                rc = 0;
	struct  cam_sensor_i2c_reg_setting i2c_reg_setting;

	memset(&i2c_reg_setting, 0, sizeof(i2c_reg_setting));

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * 1, GFP_KERNEL);
	if (i2c_reg_setting.reg_setting == NULL) {
		CAM_ERR(CAM_OIS,"kzalloc failed");
	}
	i2c_reg_setting.reg_setting->reg_addr = addr;
	i2c_reg_setting.reg_setting->reg_data = data_wr;
	i2c_reg_setting.reg_setting->delay = 0;
	i2c_reg_setting.reg_setting->data_mask = 0;

	rc = camera_io_dev_write(
		&(o_ctrl->io_master_info),
		&i2c_reg_setting);
	kfree(i2c_reg_setting.reg_setting);

	return rc;
}

int ois_write24(struct cam_ois_ctrl_t *o_ctrl, uint8_t addr, uint32_t data_wr)
{
	int                                rc = 0;
	struct  cam_sensor_i2c_reg_setting i2c_reg_setting;

	memset(&i2c_reg_setting, 0, sizeof(i2c_reg_setting));

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * 1, GFP_KERNEL);
	if (i2c_reg_setting.reg_setting == NULL) {
		CAM_ERR(CAM_OIS,"kzalloc failed");
	}
	i2c_reg_setting.reg_setting->reg_addr = addr;
	i2c_reg_setting.reg_setting->reg_data = data_wr;
	i2c_reg_setting.reg_setting->delay = 0;
	i2c_reg_setting.reg_setting->data_mask = 0;

	rc = camera_io_dev_write(
		&(o_ctrl->io_master_info),
		&i2c_reg_setting);
	kfree(i2c_reg_setting.reg_setting);

	return rc;
}

int32_t ois_write32(struct cam_ois_ctrl_t *o_ctrl, uint32_t addr, uint32_t data_wr)
{
	int32_t                           rc = 0;
	struct cam_sensor_i2c_reg_setting i2c_reg_setting;

	memset(&i2c_reg_setting, 0, sizeof(i2c_reg_setting));

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * 1, GFP_KERNEL);
	if (i2c_reg_setting.reg_setting == NULL) {
		CAM_ERR(CAM_OIS,"kzalloc failed");
		rc = OIS_FAIL;
		return rc;
	}
	i2c_reg_setting.reg_setting->reg_addr = addr;
	i2c_reg_setting.reg_setting->reg_data = data_wr;
	i2c_reg_setting.reg_setting->delay = 0;
	i2c_reg_setting.reg_setting->data_mask = 0;

	rc = camera_io_dev_write(
		&(o_ctrl->io_master_info),
		&i2c_reg_setting);
	kfree(i2c_reg_setting.reg_setting);

	return rc;
}

int ois_spcl_wrt(struct cam_ois_ctrl_t *o_ctrl, uint8_t addr, uint8_t data_wr)
{
	int                               rc = 0;
	struct cam_sensor_i2c_reg_setting i2c_reg_setting;

	memset(&i2c_reg_setting, 0, sizeof(i2c_reg_setting));

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * 1, GFP_KERNEL);
	if (i2c_reg_setting.reg_setting == NULL) {
		CAM_ERR(CAM_OIS,"kzalloc failed");
	}
	i2c_reg_setting.reg_setting->reg_addr = addr;
	i2c_reg_setting.reg_setting->reg_data = data_wr;
	i2c_reg_setting.reg_setting->delay = 0;
	i2c_reg_setting.reg_setting->data_mask = 0;

	rc = camera_io_dev_write(
		&(o_ctrl->io_master_info),
		&i2c_reg_setting);
	kfree(i2c_reg_setting.reg_setting);

	return rc;
}

int32_t ois_cnt_wrt(struct cam_ois_ctrl_t *o_ctrl, uint8_t *data, uint16_t num_byte)
{
	int32_t                           rc = 0;
	uint16_t                          cnt;
	uint8_t                           *ptr = NULL;
	struct cam_sensor_i2c_reg_setting i2c_reg_setting;

	memset(&i2c_reg_setting, 0, sizeof(i2c_reg_setting));

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = num_byte - 1;
	i2c_reg_setting.delay = 0;
	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * num_byte - 1, GFP_KERNEL);
	if (i2c_reg_setting.reg_setting == NULL) {
		CAM_ERR(CAM_OIS, "kzalloc failed");
		return -EINVAL;
	}

	for (cnt = 0, ptr = (uint8_t *)data + 1; cnt < num_byte - 1; cnt++, ptr++) {
		i2c_reg_setting.reg_setting[cnt].reg_addr = data[0];
		i2c_reg_setting.reg_setting[cnt].reg_data = *ptr;
		i2c_reg_setting.reg_setting[cnt].delay = 0;
		i2c_reg_setting.reg_setting[cnt].data_mask = 0;
	}

	rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
		&i2c_reg_setting, 1);

	if (rc < 0) {
		CAM_ERR(CAM_OIS, "OIS_CntWrt fail %d", rc);
	}

	kfree(i2c_reg_setting.reg_setting);
	return rc;
}

int32_t e2p_read8(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t e2p_addr, uint16_t *e2p_data)
{
	int32_t  rc = 0;
	uint32_t data = 0;
	uint16_t temp_sid = 0;

	temp_sid = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = slv_addr >> 1;

	rc = camera_io_dev_read(
		&(o_ctrl->io_master_info),
		e2p_addr,
		&data,
		CAMERA_SENSOR_I2C_TYPE_WORD,
		CAMERA_SENSOR_I2C_TYPE_BYTE);

	o_ctrl->io_master_info.cci_client->sid = temp_sid;
	*e2p_data = (uint16_t)data;

	return rc;
}

int e2p_cnt_rd(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t e2p_addr, uint8_t *e2p_data, uint32_t num_byte)
{
	int      rc = 0;
	uint16_t read_cnt, remaining_byte;
	uint16_t temp_sid = 0;
	int      i;

	temp_sid = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = slv_addr >> 1;

	if (num_byte <= I2C_REG_DATA_MAX) {
		rc = camera_io_dev_read_seq(
			&(o_ctrl->io_master_info),
			e2p_addr,
			e2p_data,
			CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_BYTE,
			num_byte);
	} else {
		read_cnt = num_byte / I2C_REG_DATA_MAX;
		remaining_byte = num_byte % I2C_REG_DATA_MAX;
		for (i = 0; i < read_cnt; i++, e2p_addr = e2p_addr + I2C_REG_DATA_MAX) {
			rc = camera_io_dev_read_seq(
				&(o_ctrl->io_master_info),
				e2p_addr,
				&e2p_data[i * I2C_REG_DATA_MAX],
				CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,
				I2C_REG_DATA_MAX);
		}
		if (remaining_byte > 0) {
			rc = camera_io_dev_read_seq(
				&(o_ctrl->io_master_info),
				e2p_addr,
				&e2p_data[i * I2C_REG_DATA_MAX],
				CAMERA_SENSOR_I2C_TYPE_WORD,
				CAMERA_SENSOR_I2C_TYPE_BYTE,
				remaining_byte);
		}
	}

	o_ctrl->io_master_info.cci_client->sid = temp_sid;

	return rc;
}

int e2p_write16(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint16_t e2p_addr, uint16_t data_wr)
{
	int                               rc = 0;
	uint16_t                          temp_sid = 0;
	struct cam_sensor_i2c_reg_setting i2c_reg_setting;

	temp_sid = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = slv_addr >> 1;

	memset(&i2c_reg_setting, 0, sizeof(i2c_reg_setting));

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * 1, GFP_KERNEL);
	if (i2c_reg_setting.reg_setting == NULL) {
		CAM_ERR(CAM_OIS,"kzalloc failed");
	}
	i2c_reg_setting.reg_setting->reg_addr = e2p_addr;
	i2c_reg_setting.reg_setting->reg_data = data_wr;
	i2c_reg_setting.reg_setting->delay = 0;
	i2c_reg_setting.reg_setting->data_mask = 0;

	rc = camera_io_dev_write(
		&(o_ctrl->io_master_info),
		&i2c_reg_setting);
	kfree(i2c_reg_setting.reg_setting);

	o_ctrl->io_master_info.cci_client->sid = temp_sid;

	return rc;
}

int32_t e2p_write32(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t e2p_addr, uint32_t data_wr)
{
	int32_t                           rc = 0;
	uint16_t                          temp_sid = 0;
	struct cam_sensor_i2c_reg_setting i2c_reg_setting;

	temp_sid = o_ctrl->io_master_info.cci_client->sid;
	o_ctrl->io_master_info.cci_client->sid = slv_addr >> 1;

	memset(&i2c_reg_setting, 0, sizeof(i2c_reg_setting));

	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_DWORD;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *)
		kzalloc(sizeof(struct cam_sensor_i2c_reg_array) * 1, GFP_KERNEL);
	if (i2c_reg_setting.reg_setting == NULL) {
		CAM_ERR(CAM_OIS,"kzalloc failed");
		rc = OIS_FAIL;
		return rc;
	}
	i2c_reg_setting.reg_setting->reg_addr = e2p_addr;
	i2c_reg_setting.reg_setting->reg_data = data_wr;
	i2c_reg_setting.reg_setting->delay = 0;
	i2c_reg_setting.reg_setting->data_mask = 0;

	rc = camera_io_dev_write(
		&(o_ctrl->io_master_info),
		&i2c_reg_setting);
	kfree(i2c_reg_setting.reg_setting);

	o_ctrl->io_master_info.cci_client->sid = temp_sid;

	return rc;
}

int verify_e2p_chksum(struct cam_ois_ctrl_t *o_ctrl, uint16_t slv_addr, uint32_t st_addr, uint32_t e2p_size, uint32_t chk_addr)
{
	int      rc = 0;
	uint32_t partial_sum = 0;
	uint8_t  chksum[4] = {0,};
	uint8_t  *e2p_data;
	int      i;

	e2p_data = (uint8_t *)kzalloc(sizeof(uint8_t) * e2p_size, GFP_KERNEL);

	rc = e2p_cnt_rd(o_ctrl, slv_addr, st_addr, e2p_data, e2p_size);
	if (rc < 0) {
		CAM_ERR(CAM_OIS,"Verify e2p i2c failed");
		goto END;
	}

	for (i = 0; i < e2p_size; i++) partial_sum += e2p_data[i];

	rc = e2p_cnt_rd(o_ctrl, slv_addr, chk_addr, chksum, 4);
	if (rc < 0) {
		CAM_ERR(CAM_OIS,"Verify e2p i2c failed");
		goto END;
	}

	if (partial_sum != (chksum[0] | chksum[1] << 8 | chksum[2] << 16 | chksum[3] << 24)) {
		CAM_ERR(CAM_OIS,"Verify e2p checksum failed partial sum: 0x%x checksum: 0x%x",
			partial_sum, (chksum[0] | chksum[1] << 8 | chksum[2] << 16 | chksum[3] << 24));
		for (i = 0; i < e2p_size; i++) pr_err("0x%x \n", e2p_data[i]);

		rc = OIS_INIT_CHECKSUM_ERROR;
		goto END;
	}

	END:
	kfree(e2p_data);
	return rc;
}

