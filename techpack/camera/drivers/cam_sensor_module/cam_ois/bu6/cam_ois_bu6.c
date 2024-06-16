#ifndef CAM_OIS_BU6_C
#define CAM_OIS_BU6_C
#endif

#include "cam_ois_bu6.h"

struct cam_ois_ctrl_t *bu6_ctrl;
extern uint8_t        zdrift_ready;

void write_z_value(struct i2c_settings_list *i2c_list)
{
	uint32_t z_value =
		0x9000 << 16 | i2c_list->i2c_settings.reg_setting[0].reg_data;

	if ((i2c_list->i2c_settings.reg_setting[0].reg_addr == 0x00) && (zdrift_ready == 3))
		ois_write24(bu6_ctrl, BU6_ZDT_DAC_ADDR, z_value);
}

int32_t cam_ois_bu6_init(struct cam_ois_ctrl_t *o_ctrl)
{
	ADJ_STS   OIS_MAIN_STS  = ADJ_ERR;
	_FACT_ADJ fadj;
	OIS_UWORD fw_ver = 0;
	OIS_UWORD sts = 0;
	bu6_ctrl = o_ctrl;

	e2p_read8(bu6_ctrl, BU6_E2P_SID, 0x279D, &fw_ver);
	CAM_ERR(CAM_OIS, "Enter %s m.build 0x%x", OIS_INFO, fw_ver);

	//------------------------------------------------------
	// Get Factory adjusted data from non volatile memory
	//------------------------------------------------------
	OIS_MAIN_STS = get_FADJ_MEM_from_non_volatile_memory(&fadj);
	if ( OIS_MAIN_STS <= ADJ_ERR ) return OIS_MAIN_STS;

	//------------------------------------------------------
	// PLL setting to use external CLK
	//------------------------------------------------------
	VCOSET0();

	//------------------------------------------------------
	// Download Program and Coefficient
	//------------------------------------------------------
	OIS_MAIN_STS = func_PROGRAM_DOWNLOAD( );                // Program Download
	if ( OIS_MAIN_STS <= ADJ_ERR ) return OIS_MAIN_STS;     // If success OIS_MAIN_STS is zero.

	func_COEF_DOWNLOAD( 0 );                                // Download Coefficient
	//------------------------------------------------------
	// Change Clock to external pin CLK_PS
	//------------------------------------------------------
	VCOSET1();

	//------------------------------------------------------
	// Set calibration data
	//------------------------------------------------------
	SET_FADJ_PARAM( &fadj );
	if (fw_ver > 0x09)
		SET_ZDRIFT_PARAM();

	//------------------------------------------------------
	// Issue DSP start command.
	//------------------------------------------------------
	I2C_OIS_spcl_cmnd( 1, _cmd_8C_EI );                     // DSP calculation START

	//------------------------------------------------------
	// Set scene parameter for OIS
	//------------------------------------------------------
	func_SET_SCENE_PARAM_for_NewGYRO_Fil( _SCENE_SPORT_1, 0, &fadj );
	                                                        // Set default SCENE ( Mode is example )
	ois_read16(bu6_ctrl, 0x84F7, &sts);
	if ((sts & 0x0004) != 0x0004) return OIS_FAIL;
	ois_read16(bu6_ctrl, 0x84F6, &fw_ver);
	if (fw_ver > 0x09) zdrift_ready |= 1;

	CAM_ERR(CAM_OIS, "%s init done. fw 0x%x zdt 0x%x", OIS_INFO, fw_ver, zdrift_ready);

	if (bu6_ctrl->is_ois_aat == 1) {
		CAM_ERR(CAM_OIS, "OIS AAT start");
		usleep_range(20000, 20010);
		OIS_MAIN_STS = cam_ois_bu6_selftest();
		if (OIS_MAIN_STS != OIS_SUCCESS) return OIS_STOP_TEST;
		OIS_MAIN_STS = cam_ois_bu6_calibration();
		if (OIS_MAIN_STS != OIS_SUCCESS) return OIS_STOP_TEST;
		OIS_MAIN_STS = cam_ois_bu6_selftest2();
		if (OIS_MAIN_STS != OIS_SUCCESS) return OIS_STOP_TEST;
		CAM_ERR(CAM_OIS, "OIS AAT end");
	}

	return ADJ_OK;
}

int16_t cam_ois_bu6_calibration(void)
{
	ADJ_STS   sts = ADJ_OK;
	OIS_UWORD u16_avrN = 16;
	OIS_UWORD u16_i;
	OIS_UWORD u16_tmp_read1, u16_tmp_read2;
	OIS_LONG  s32_dat1 = 0, s32_dat2 = 0;
	uint32_t  gyrOfs, chksum;
	uint32_t  result = cam_ois_selftest_get();
	uint8_t   gyrOfs_e2p[4] = {0,}, gyrOfs_cal[4] = {0,};
	uint8_t   ois_chksum[4] = {0,}, tot_chksum[4] = {0,};
	uint8_t   bkup_data[BU6_OIS_BKUP_SIZE] = {0,};
	bool      isFTime = TRUE;
	int       i, diff;

	sts = verify_e2p_chksum(bu6_ctrl, BU6_E2P_SID, BU6_E2P_ST_ADDR, BU6_E2P_SIZE, BU6_TOT_CHKSUM_ADDR);
	if (sts < 0) {
		CAM_ERR(CAM_OIS, "E2P checksum fail");
		return sts;
	}

	u16_tmp_read1 = I2C_OIS_mem__read( 0x06 );
	u16_tmp_read2 = I2C_OIS_mem__read( 0x86 );
	usleep_range(2000, 2010);

	e2p_cnt_rd(bu6_ctrl, BU6_E2P_SID, 0x27B0, gyrOfs_e2p, 4);
	usleep_range(2000, 2010);
	e2p_cnt_rd(bu6_ctrl, BU6_E2P_SID, BU6_OIS_CHKSUM_ADDR, ois_chksum, 4);
	usleep_range(2000, 2010);
	e2p_cnt_rd(bu6_ctrl, BU6_E2P_SID, BU6_TOT_CHKSUM_ADDR, tot_chksum, 4);
	usleep_range(2000, 2010);

	CAM_ERR(CAM_OIS, "Calibration start. Gyro offset mem_x: %5d mem_y: %5d e2p_x: %5d e2p_y: %5d OIS_CHKSUM: 0x%x TOT_CHKSUM: 0x%x",
		H2D(u16_tmp_read1),
		H2D(u16_tmp_read2),
		H2D(gyrOfs_e2p[0] | gyrOfs_e2p[1] << 8),
		H2D(gyrOfs_e2p[2] | gyrOfs_e2p[3] << 8),
		ois_chksum[0] | ois_chksum[1] << 8 | ois_chksum[2] << 16 | ois_chksum[3] << 24,
		tot_chksum[0] | tot_chksum[1] << 8 | tot_chksum[2] << 16 | tot_chksum[3] << 24);

	e2p_cnt_rd(bu6_ctrl, BU6_E2P_SID, BU6_OIS_BKUP_ADDR, bkup_data, BU6_OIS_BKUP_SIZE);
	usleep_range(2000, 2010);
	for (i = 0; i < BU6_OIS_BKUP_SIZE; i++) {
		if (bkup_data[i] != 0xFF) isFTime = FALSE;
	}

	if (isFTime) {
		CAM_ERR(CAM_OIS, "FTime Calibration. e2p bkup.");
		e2p_write32(bu6_ctrl, BU6_E2P_SID, BU6_OIS_BKUP_ADDR,
			gyrOfs_e2p[0] << 24 | gyrOfs_e2p[1] << 16 | gyrOfs_e2p[2] << 8 | gyrOfs_e2p[3]);
		usleep_range(7000, 7010);
		e2p_write32(bu6_ctrl, BU6_E2P_SID, BU6_OIS_BKUP_ADDR + 4,
			ois_chksum[0] << 24 | ois_chksum[1] << 16 | ois_chksum[2] << 8 | ois_chksum[3]);
		usleep_range(7000, 7010);
		e2p_write32(bu6_ctrl, BU6_E2P_SID, BU6_OIS_BKUP_ADDR + 8,
			tot_chksum[0] << 24 | tot_chksum[1] << 16 | tot_chksum[2] << 8 | tot_chksum[3]);
		usleep_range(7000, 7010);
	}

	for ( u16_i = 1; u16_i <= u16_avrN; u16_i += 1 ) {
		usleep_range(5000, 5010);
		u16_tmp_read1 = I2C_OIS_mem__read( 0x55 );
		u16_tmp_read2 = I2C_OIS_mem__read( 0x56 );
		s32_dat1 += H2D( u16_tmp_read1 );
		s32_dat2 += H2D( u16_tmp_read2 );
	}

	CAM_ERR(CAM_OIS, "AvGx:%5ld AvGy:%5ld", s32_dat1 / u16_avrN, s32_dat2 / u16_avrN);

	I2C_OIS_mem_write(0x06, D2H((OIS_WORD) div_N( s32_dat1, u16_avrN )));
	I2C_OIS_mem_write(0x86, D2H((OIS_WORD) div_N( s32_dat2, u16_avrN )));
	usleep_range(5000, 5010);

	gyrOfs = D2H((OIS_WORD) div_N( s32_dat1, u16_avrN )) << 16 | D2H((OIS_WORD) div_N( s32_dat2, u16_avrN ));

	sts = store_FADJ_MEM_to_non_volatile_memory(gyrOfs);
	if (sts < 0) {
		return sts;
	}

	u16_tmp_read1 = I2C_OIS_mem__read( 0x06 );
	u16_tmp_read2 = I2C_OIS_mem__read( 0x86 );
	usleep_range(5000, 5010);

	e2p_cnt_rd(bu6_ctrl, BU6_E2P_SID, 0x27B0, gyrOfs_cal, 4);
	CAM_ERR(CAM_OIS, "Calibration end. Gyro offset mem_x: %5d mem_y: %5d e2p_x: %5d e2p_y: %5d",
		H2D(u16_tmp_read1),
		H2D(u16_tmp_read2),
		H2D(gyrOfs_cal[0] | gyrOfs_cal[1] << 8),
		H2D((gyrOfs_cal[2] | gyrOfs_cal[3] << 8)));

	if ((u16_tmp_read1 != (gyrOfs_cal[0] | gyrOfs_cal[1] << 8)) ||
		(u16_tmp_read2 != (gyrOfs_cal[2] | gyrOfs_cal[3] << 8))) {
		CAM_ERR(CAM_OIS, "Calibration error.");
		return OIS_FAIL;
	}

	for (i = 0, diff = 0; i < 4; i++) diff += gyrOfs_cal[i] - gyrOfs_e2p[i];

	chksum = ((ois_chksum[0] | ois_chksum[1] << 8 |
		ois_chksum[2] << 16 | ois_chksum[3] << 24)) + diff;
	chksum = ((chksum & 0xFF) << 24) | (((chksum >> 8 ) & 0xFF) << 16) |
		(((chksum >> 16) & 0xFF) << 8) | (chksum >> 24);
	e2p_write32(bu6_ctrl, BU6_E2P_SID, BU6_OIS_CHKSUM_ADDR, chksum);
	usleep_range(7000, 7010);

	for (i = 0; i < 4; i++) diff += ((chksum >> (24 - (i * 8))) & 0xff) - ois_chksum[i];

	chksum = ((tot_chksum[0] | tot_chksum[1] << 8 |
		tot_chksum[2] << 16 | tot_chksum[3] << 24)) + diff;
	chksum = ((chksum & 0xFF) << 24) | (((chksum >> 8 ) & 0xFF) << 16) |
		(((chksum >> 16) & 0xFF) << 8) | (chksum >> 24);
	e2p_write32(bu6_ctrl, BU6_E2P_SID, BU6_TOT_CHKSUM_ADDR, chksum);
	usleep_range(7000, 7010);

	sts = verify_e2p_chksum(bu6_ctrl, BU6_E2P_SID, BU6_E2P_ST_ADDR, BU6_E2P_SIZE, BU6_TOT_CHKSUM_ADDR);
	if (sts < 0) {
		CAM_ERR(CAM_OIS, "E2P checksum fail.");
		return sts;
	} else {
		result |= 1;
	}

	// Clear buffers of gyro filter
	I2C_OIS_mem_write(0x3C, 0x0000);
	I2C_OIS_mem_write(0xBC, 0x0000);
	I2C_OIS_mem_write(0x18, 0x0000);
	I2C_OIS_mem_write(0x98, 0x0000);
	I2C_OIS_mem_write(0x19, 0x0000);
	I2C_OIS_mem_write(0x99, 0x0000);

	result |= (1 << 2);
	cam_ois_selftest_set(result);

	return OIS_SUCCESS;
}

int32_t cam_ois_bu6_move_lens(void *data)
{
	int16_t hallx = 0, hally = 0, rHallx = 0, rHally = 0;
	int16_t LCC_X = 0, LCC_Y = 0;
	int16_t offset[2];

	memcpy(offset, data, sizeof(offset));

	hallx =  offset[0];
	hally =  offset[1];

	ois_write16(bu6_ctrl, 0x847F, 0x0C2C);

	ois_read16(bu6_ctrl, 0x846A, &LCC_X);
	ois_read16(bu6_ctrl, 0x846B, &LCC_Y);

	I2C_OIS_mem_write(0x17, hallx);
	I2C_OIS_mem_write(0x97, hally);

	usleep_range(100000, 100010);

	ois_read16(bu6_ctrl, 0x843F, &rHallx);
	ois_read16(bu6_ctrl, 0x84BF, &rHally);

	if ((abs(rHallx - (rHallx * rHallx * LCC_X / LCC_DIV_VAL + hallx)) < HALL_LIMIT) &&
		(abs(rHally - (rHally * rHally * LCC_Y / LCC_DIV_VAL + hally)) < HALL_LIMIT)) {
			CAM_ERR(CAM_OIS, "Target x: %5d  y: %5d diff x: %5d  y: %5d",
				hallx, hally,
				abs(rHallx - (rHallx * rHallx * LCC_X / LCC_DIV_VAL + hallx)),
				abs(rHally - (rHally * rHally * LCC_Y / LCC_DIV_VAL + hally)));
			return	OIS_SUCCESS;
		}

		CAM_ERR(CAM_OIS, "Target x: %5d  y: %5d  diff x: %5d  y: %5d  <-- move fail!!!",
			hallx, hally,
			abs(rHallx - (rHallx * rHallx * LCC_X / LCC_DIV_VAL + hallx)),
			abs(rHally - (rHally * rHally * LCC_Y / LCC_DIV_VAL + hally)));

	return OIS_FAIL;
}

int32_t cam_ois_bu6_stat(sensor_ois_stat_t *data)
{
	sensor_ois_stat_t ois_stat;
	uint16_t          val_gyro_x, val_gyro_y;
	uint16_t          val_gyro_offset_x, val_gyro_offset_y;

	memset(&ois_stat, 0, sizeof(ois_stat));

	/* Gyro Read by reg */
	ois_read16(bu6_ctrl, 0x8455, &val_gyro_x);
	ois_read16(bu6_ctrl, 0x8456, &val_gyro_y);
	ois_read16(bu6_ctrl, 0x8407, &val_gyro_offset_x);
	ois_read16(bu6_ctrl, 0x8487, &val_gyro_offset_y);

	ois_stat.gyro[0]   = (int16_t)val_gyro_x;
	ois_stat.gyro[1]   = (int16_t)val_gyro_y;
	ois_stat.offset[0] = (int16_t)val_gyro_offset_x;
	ois_stat.offset[1] = (int16_t)val_gyro_offset_y;
	ois_stat.is_stable = 1;

	*data = ois_stat;

	CAM_ERR(CAM_OIS,"gyro x %d gyro y %d offset x %d offset y %d",
		ois_stat.gyro[0], ois_stat.gyro[1],
		ois_stat.offset[0], ois_stat.offset[1]);

	return OIS_SUCCESS;
}

/*===========================================================================
 * FUNCTION    - cam_ois_bu6_selftest -
 *
 * DESCRIPTION: ois self-test routine for all-auto-test
 *==========================================================================*/
#define GYRO_RAW_LIMIT    30   // 30[dps]*87.5
#define GYRO_CAL_LIMIT    10   // 10[dps]*87.5
#define GYRO_SCALE_FACTOR 87   // Gyro sensitivity LSB/dps

#define GYRO_RAW_X_DPS ois_stat.gyro[0] / GYRO_SCALE_FACTOR
#define GYRO_RAW_Y_DPS ois_stat.gyro[1] / GYRO_SCALE_FACTOR
#define GYRO_CAL_X_DPS ois_stat.offset[0] / GYRO_SCALE_FACTOR
#define GYRO_CAL_Y_DPS ois_stat.offset[1] / GYRO_SCALE_FACTOR

#define UNSTABLE_RATIO 40 //0.4 * 100

int8_t cam_ois_bu6_selftest(void)
{
	int32_t           rc = OIS_SUCCESS;
	uint32_t          result = 0;
	int               i = 0;
	sensor_ois_stat_t ois_stat;
	int16_t           hall_target[][2] = {
		{ 1449,     0}, { 1024,   862}, {    0,  1220},
		{-1024,   862}, {-1449,     0}, {-1024,  -862},
		{    0, -1220}, { 1024,  -862}, { 1449,     0}
	};

	memset(&ois_stat, 0, sizeof(ois_stat));

	CAM_ERR(CAM_OIS, " enter");

	//0. is ois init success ?
	result |= (1 << 5);

	//1. check lens movement range
	result |= (1 << 4);
	for (i = 0; i <= 8; i++) {
		if (cam_ois_bu6_move_lens(&hall_target[i]) < 0)
			result &= ~(1 << 4);
	}

	//2. reset lens position before ois turn-on.
	{
		uint16_t offset[2] = {0, 0};
		cam_ois_bu6_move_lens(&offset);
	}

	//3. get first stat value.
	cam_ois_bu6_stat(&ois_stat);

	//4. check ois module is alive.
	if (ois_stat.gyro[0] == 0 && ois_stat.gyro[1] == 0 &&
		ois_stat.offset[0] == 0 && ois_stat.offset[1] == 0) {
		CAM_ERR(CAM_OIS, "gyro stat fail");
		rc = OIS_FAIL;
		goto END;
	}

	//5. check gyro initial dps.
	result |= (1 << 3);
	if (abs(GYRO_RAW_X_DPS) <= GYRO_RAW_LIMIT) {
		CAM_ERR(CAM_OIS, "(GYRO_RAW_X_DPS) <= GYRO_RAW_LIMIT)");
	} else {
		CAM_ERR(CAM_OIS, "spec over! (GYRO_RAW_X_DPS %d) > GYRO_RAW_LIMIT)", GYRO_RAW_X_DPS);
		result &= ~(1 << 3);
		rc = OIS_FAIL;
	}
	if (abs(GYRO_RAW_Y_DPS) <= GYRO_RAW_LIMIT) {
		CAM_ERR(CAM_OIS, "(GYRO_RAW_Y_DPS) <= GYRO_RAW_LIMIT)");
	} else {
		CAM_ERR(CAM_OIS, "spec over! (GYRO_RAW_Y_DPS %d) > GYRO_RAW_LIMIT)", GYRO_RAW_Y_DPS);
		result &= ~(1 << 3);
		rc = OIS_FAIL;
	}

	END:
	cam_ois_selftest_set(result);
	CAM_ERR(CAM_OIS," exit");

	return rc;
}

/*===========================================================================
 * FUNCTION    - cam_ois_bu6_selftest2 -
 *
 * DESCRIPTION: ois self-test routine #2 for all-auto-test
 *==========================================================================*/
int8_t cam_ois_bu6_selftest2(void)
{
	int32_t           rc = OIS_SUCCESS;
	uint32_t          result = cam_ois_selftest_get();
	int               i;
	int               unstable = 0;
	sensor_ois_stat_t ois_stat;

	memset(&ois_stat, 0, sizeof(ois_stat));

	CAM_ERR(CAM_OIS," enter");

	//check gyro validity, servo stability at least 100 times
	result |= (1 << 1);
	for (i = 0; i < 100; i++) {
		rc = cam_ois_bu6_stat(&ois_stat);
		if (rc < 0) {
			CAM_ERR(CAM_OIS,"i:%d ois_get_stat error", i);
			result &= ~(1 << 5);
			result &= ~(1 << 1);
			return OIS_FAIL;
		}
		if (abs(GYRO_CAL_X_DPS) > GYRO_CAL_LIMIT) {
			CAM_ERR(CAM_OIS," %d spec over GYRO_CAL_X_DPS %d", i, GYRO_CAL_X_DPS);
			result &= ~(1 << 1);
			ois_stat.is_stable = 0;
		}
		if (abs(GYRO_CAL_Y_DPS) > GYRO_CAL_LIMIT) {
			CAM_ERR(CAM_OIS," %d spec over GYRO_CAL_Y_DPS %d", i, GYRO_CAL_Y_DPS);
			result &= ~(1 << 1);
			ois_stat.is_stable = 0;
		}
		if (!ois_stat.is_stable)
			unstable++;
	}

	if (unstable > UNSTABLE_RATIO)
		result &= ~(1 << 5);

	CAM_ERR(CAM_OIS," unstable -> %d", unstable);

	cam_ois_selftest_set(result);
	CAM_ERR(CAM_OIS," exit");

	return OIS_SUCCESS;
}

void VCOSET0( void )
{
	// !!!  Depend on user system   !!!
	OIS_UWORD   CLK_PS = 19200;                                 // Input Frequency [kHz] of CLK/PS terminal (Depend on your system)
	// !!!  Depend on user system   !!!

	OIS_UWORD   FVCO_1 = 36000;                                 // Target Frequency [kHz]
	OIS_UWORD   FREF   = 25;                                    // Reference Clock Frequency [kHz]

	OIS_UWORD   DIV_N  = CLK_PS / FREF - 1;                     // calc DIV_N
	OIS_UWORD   DIV_M  = FVCO_1 / FREF - 1;                     // calc DIV_M

	I2C_OIS_per_write( 0x62, DIV_N  );                          // Divider for internal reference clock
	I2C_OIS_per_write( 0x63, DIV_M  );                          // Divider for internal PLL clock
	I2C_OIS_per_write( 0x64, 0x4060 );                          // Loop Filter
	I2C_OIS_per_write( 0x60, 0x3011 );                          // PLL
	I2C_OIS_per_write( 0x65, 0x0080 );                          //
	I2C_OIS_per_write( 0x61, 0x8002 );                          // VCOON
	I2C_OIS_per_write( 0x61, 0x8003 );                          // Circuit ON
	I2C_OIS_per_write( 0x61, 0x8809 );                          // PLL ON
}


void VCOSET1( void )
{
	I2C_OIS_per_write( 0x05, 0x000C );                          // Prepare for PLL clock as master clock
	I2C_OIS_per_write( 0x05, 0x000D );                          // Change to PLL clock
}

int store_FADJ_MEM_to_non_volatile_memory(uint32_t u32_dat)
{
	int       rc = OIS_SUCCESS;
	OIS_UBYTE ofs_X[2], ofs_Y[2];

	ofs_X[0] = ( u32_dat >> 16 ) & 0xFF;
	ofs_X[1] = ( u32_dat >> 24 ) & 0xFF;
	ofs_Y[0] = ( u32_dat       ) & 0xFF;
	ofs_Y[1] = ( u32_dat >> 8  ) & 0xFF;

	rc = e2p_write16(bu6_ctrl, BU6_E2P_SID, 0x27B0, ofs_X[0] << 8 | ofs_X[1]);
	if (rc < 0) {
		CAM_ERR(CAM_OIS,"e2p write err");
		return rc;
	}
	usleep_range(10000, 10010);
	rc = e2p_write16(bu6_ctrl, BU6_E2P_SID, 0x27B2, ofs_Y[0] << 8 | ofs_Y[1]);
	if (rc < 0) {
		CAM_ERR(CAM_OIS,"e2p write err");
		return rc;
	}
	usleep_range(10000, 10010);

	return rc;
}

int SET_ZDRIFT_PARAM( void )
{
	int     rc = OIS_SUCCESS;
	uint8_t zdrift_data[BU6_OIS_ZDRIFT_SIZE] = {0,};
	int     i;

	rc = e2p_cnt_rd(bu6_ctrl, BU6_E2P_SID, BU6_ZDT_ST_ADDR, zdrift_data, BU6_OIS_ZDRIFT_SIZE);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "E2P read fail.");
		return OIS_INIT_EEPROM_ERROR;
	}

	for (i = 0; i < BU6_OIS_ZDRIFT_SIZE / 2; i++) {
		ois_write16(bu6_ctrl, 0x82B2 + i,
			zdrift_data[i * 2 + 1] << 8 | zdrift_data[i * 2]);
	}

	zdrift_ready |= (1 << 1);

	return rc;
}

int get_FADJ_MEM_from_non_volatile_memory(_FACT_ADJ *fadj)
{
	int     rc = 0;
	uint8_t cal_data[BU6_OIS_CAL_SIZE] = {0,};
	bool    isAvailable = FALSE;
	int     i;

	rc = e2p_cnt_rd(bu6_ctrl, BU6_E2P_SID, BU6_CAL_ST_ADDR, cal_data, BU6_OIS_CAL_SIZE);
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "E2P read fail.");
		return OIS_INIT_EEPROM_ERROR;
	} else {
		for (i = 0; i < BU6_OIS_CAL_SIZE + 1; i++) {
			if ((i != 0) && (i % 2 == 0)) {
				((OIS_UWORD *)fadj)[(i / 2) - 1] = cal_data[i - 2] | cal_data[i - 1] << 8;
				if (((OIS_UWORD *)fadj)[(i / 2) - 1] != 0xFFFF)
					isAvailable = TRUE;
			}
		}
			if (!isAvailable)
				CAM_ERR(CAM_OIS, "E2P data is not available.");
	}

	return OIS_SUCCESS;
}

