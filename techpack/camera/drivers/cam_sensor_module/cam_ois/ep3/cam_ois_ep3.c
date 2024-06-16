/* ================================================================== */
/*	OIS firmware */
/* ================================================================== */
#include "cam_ois_ep3.h"
#include "onsemi_ois.h"
#include "LC898124EP3_Code_9_2_1_0.h"
#include "LC898124EP3_Code_9_2_1_1.h"

#define OIS_INFO "EP3"

#define LIMIT_STATUS_POLLING    (15)
#define LIMIT_OIS_ON_RETRY      (5)
#define EP3_READ_STATUS_ADDR    (0xF100)
#define OIS_STATUS_OK           (0x00)
#define EP3_E2P_SID             (0xA6)
#define MODULE_FW_VER_ADDR      (0xBB8)
#define HALL_LIMIT              (2601)
#define OIS_GYRO_DEFAULT_GAIN   (15284)

stAdjPar    StAdjPar; // temporary buffer for caribration dataz
struct cam_ois_ctrl_t *ep3_ctrl;

int cam_ois_ep3_poll_ready(void)
{
	uint32_t ois_status = OIS_INIT_TIMEOUT;
	int      read_byte = 0;
	int      rc = OIS_SUCCESS;

	while ((ois_status != OIS_STATUS_OK) && (read_byte < LIMIT_STATUS_POLLING)) {
		rc = ois_read32(ep3_ctrl, EP3_READ_STATUS_ADDR, &ois_status); /* polling status ready */
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS_I2C_ERROR");
			return OIS_INIT_I2C_ERROR;
		}
		usleep_range(5* 1000, 5* 1000 + 10); /* wait 5ms */
		read_byte++;
	}

	if (ois_status == OIS_STATUS_OK) {
		return read_byte;
	}
	else {
		CAM_ERR(CAM_OIS, "OIS_TIMEOUT_ERROR");
		return OIS_INIT_TIMEOUT;
	}

	return OIS_SUCCESS;
}

//****************************************************
//	LOCAL RAM LIST
//****************************************************
#define BURST_LENGTH_PM (12*5)
#define BURST_LENGTH_DM (10*6)
#define BURST_LENGTH BURST_LENGTH_PM

void MemoryClear(uint16_t UsSourceAddress, uint16_t UsClearSize)
{
	uint16_t UsLoopIndex;

	for (UsLoopIndex = 0; UsLoopIndex < UsClearSize; UsLoopIndex += 4) {
		ois_write32(ep3_ctrl, UsSourceAddress + UsLoopIndex, 0x00000000);          // 4Byte
		//TRACE("MEM CLR ADR = %04xh \n",UsSourceAddress + UsLoopIndex);
	}
}

void SetTransDataAdr(uint16_t UsLowAddress , uint32_t UlLowAdrBeforeTrans)
{
	UnDwdVal StTrsVal;

	if (UlLowAdrBeforeTrans < 0x00009000) {
		StTrsVal.UlDwdVal = UlLowAdrBeforeTrans;
	} else {
		StTrsVal.StDwdVal.UsHigVal = (uint16_t)((UlLowAdrBeforeTrans & 0x0000F000) >> 8);
		StTrsVal.StDwdVal.UsLowVal = (uint16_t)(UlLowAdrBeforeTrans & 0x00000FFF);
	}
//TRACE(" TRANS  ADR = %04xh , DAT = %08xh \n",UsLowAddress , StTrsVal.UlDwdVal);
	ois_write32(ep3_ctrl, UsLowAddress, StTrsVal.UlDwdVal);
}

void ClrMesFil(void)
{
	ois_write32(ep3_ctrl, MeasureFilterA_Delay_z11, 0);
	ois_write32(ep3_ctrl, MeasureFilterA_Delay_z12, 0);

	ois_write32(ep3_ctrl, MeasureFilterA_Delay_z21, 0);
	ois_write32(ep3_ctrl, MeasureFilterA_Delay_z22, 0);

	ois_write32(ep3_ctrl, MeasureFilterB_Delay_z11, 0);
	ois_write32(ep3_ctrl, MeasureFilterB_Delay_z12, 0);

	ois_write32(ep3_ctrl, MeasureFilterB_Delay_z21, 0);
	ois_write32(ep3_ctrl, MeasureFilterB_Delay_z22, 0);
}

void SetWaitTime(uint16_t UsWaitTime)
{
	ois_write32(ep3_ctrl, WaitTimerData_UiWaitCounter, 0);
	ois_write32(ep3_ctrl, WaitTimerData_UiTargetCount, (uint32_t)(ONE_MSEC_COUNT * UsWaitTime));
}

void MeasureStart(int32_t SlMeasureParameterNum , uint32_t SlMeasureParameterA , uint32_t SlMeasureParameterB)
{
	MemoryClear(StMeasFunc_SiSampleNum , sizeof(MeasureFunction_Type));
	ois_write32(ep3_ctrl, StMeasFunc_MFA_SiMax1, 0x80000000);              // Set Min
	ois_write32(ep3_ctrl, StMeasFunc_MFB_SiMax2, 0x80000000);              // Set Min
	ois_write32(ep3_ctrl, StMeasFunc_MFA_SiMin1, 0x7FFFFFFF);              // Set Max
	ois_write32(ep3_ctrl, StMeasFunc_MFB_SiMin2, 0x7FFFFFFF);              // Set Max

	SetTransDataAdr(StMeasFunc_MFA_PiMeasureRam1, SlMeasureParameterA);    // Set Measure Filter A Ram Address
	SetTransDataAdr(StMeasFunc_MFB_PiMeasureRam2, SlMeasureParameterB);    // Set Measure Filter B Ram Address
	ois_write32(ep3_ctrl, StMeasFunc_SiSampleNum, 0);                      // Clear Measure Counter
	ClrMesFil();                                                           // Clear Delay Ram
	SetWaitTime(1);
	ois_write32(ep3_ctrl, StMeasFunc_SiSampleMax, SlMeasureParameterNum);  // Set Measure Max Number
}

void MeasureWait(void)
{
	uint32_t SlWaitTimerSt;

	SlWaitTimerSt = 1;
	while(SlWaitTimerSt){
		ois_read32(ep3_ctrl, StMeasFunc_SiSampleMax , &SlWaitTimerSt);
	}
}

void MesFil(void)	             // 20.019kHz
{
	uint32_t UlMeasFilaA, UlMeasFilaB, UlMeasFilaC;
	uint32_t UlMeasFilbA, UlMeasFilbB, UlMeasFilbC;

	UlMeasFilaA	= 0x7FFFFFFF;    // Through
	UlMeasFilaB	= 0x00000000;
	UlMeasFilaC	= 0x00000000;
	UlMeasFilbA	= 0x7FFFFFFF;    // Through
	UlMeasFilbB	= 0x00000000;
	UlMeasFilbC	= 0x00000000;

	ois_write32(ep3_ctrl, MeasureFilterA_Coeff_a1, UlMeasFilaA);
	ois_write32(ep3_ctrl, MeasureFilterA_Coeff_b1, UlMeasFilaB);
	ois_write32(ep3_ctrl, MeasureFilterA_Coeff_c1, UlMeasFilaC);

	ois_write32(ep3_ctrl, MeasureFilterA_Coeff_a2, UlMeasFilbA);
	ois_write32(ep3_ctrl, MeasureFilterA_Coeff_b2, UlMeasFilbB);
	ois_write32(ep3_ctrl, MeasureFilterA_Coeff_c2, UlMeasFilbC);

	ois_write32(ep3_ctrl, MeasureFilterB_Coeff_a1, UlMeasFilaA);
	ois_write32(ep3_ctrl, MeasureFilterB_Coeff_b1, UlMeasFilaB);
	ois_write32(ep3_ctrl, MeasureFilterB_Coeff_c1, UlMeasFilaC);

	ois_write32(ep3_ctrl, MeasureFilterB_Coeff_a2, UlMeasFilbA);
	ois_write32(ep3_ctrl, MeasureFilterB_Coeff_b2, UlMeasFilbB);
	ois_write32(ep3_ctrl, MeasureFilterB_Coeff_c2, UlMeasFilbC);
}

void DMIOWrite32(uint32_t IOadrs, uint32_t IOdata)
{
	ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS, IOadrs);
	ois_write32(ep3_ctrl, CMD_IO_DAT_ACCESS, IOdata);
};

//********************************************************************************
// Function Name	: DownloadToEP3
// Retun Value		: NON
// Argment Value	: PMlength: 5byte unit, DMlength : 1Byte unit
// Explanation		: <Pmem Memory> Write Data
// History			: First edition
//********************************************************************************
unsigned char DownloadToEP3( const uint8_t* DataPM, uint32_t LengthPM, uint32_t Parity, const uint8_t* DataDM, uint32_t LengthDMA, uint32_t LengthDMB )
{
	uint32_t i, j;
	uint8_t  data[64];          // work fifo buffer max size 64 byte
	uint8_t  Remainder;
	uint32_t UlReadVal, UlCnt;
	uint32_t ReadVerifyPM, ReadVerifyDMA, ReadVerifyDMB;    // Checksum

//*******************************************************************************//
//*   pre-check ROM code version                                                *//
//*******************************************************************************//
	ois_read32(ep3_ctrl,  CMD_ROMVER , &UlReadVal);

	if ( UlReadVal == OLD_VER ) return( 3 );           /* ROM code version error */

//--------------------------------------------------------------------------------
// 0. Start up to boot exection
//--------------------------------------------------------------------------------
	ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS, ROMINFO);
	ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);
	switch ( (uint8_t)UlReadVal ) {
	case 0x0A:  /* Normal Rom program execution */
		break;

	case 0x01:  /* Normal Ram program execution */
		DMIOWrite32( SYSDSP_REMAP, 0x00001000 ); // CORE_RST
		WitTim( 6 );
		break;

	default:
		return( 1 );
	}
//--------------------------------------------------------------------------------
// 1. Download Program
//--------------------------------------------------------------------------------
	data[0] = 0x30;     // Pmem address set
	data[1] = 0x00;     // Command High
	data[2] = 0x10;     // Command High
	data[3] = 0x00;     // Command High
	data[4] = 0x00;     // Command High
	ois_cnt_wrt(ep3_ctrl, data, 5); // I2C 1Byte address.
	// program start
	data[0] = 0x40;         // Pmem address set
	Remainder = ( (LengthPM*5) / BURST_LENGTH_PM );
	for (i=0 ; i< Remainder ; i++) {
		UlCnt = 1;
		for (j=0 ; j < BURST_LENGTH_PM; j++) data[UlCnt++] = *DataPM++;

		ois_cnt_wrt(ep3_ctrl, data, BURST_LENGTH_PM+1); // I2Caddresss 1Byte.
	}
	Remainder = ( (LengthPM*5) % BURST_LENGTH_PM);
	if (Remainder != 0) {
		UlCnt = 1;
		for (j=0 ; j < Remainder; j++) data[UlCnt++] = *DataPM++;
		ois_cnt_wrt(ep3_ctrl,  data, UlCnt); // I2C 1Byte address.
	}
	// Chercksum start
	data[0] = 0xF0;                                         // Pmem address set
	data[1] = 0x0A;                                         // Command High
	data[2] = (unsigned char)((LengthPM & 0xFF00) >> 8);    // Size High
	data[3] = (unsigned char)((LengthPM & 0x00FF) >> 0);    // Size Low
	ois_cnt_wrt(ep3_ctrl, data, 4); // I2C 2Byte addresss.

//--------------------------------------------------------------------------------
// 2. Download Table Data
//--------------------------------------------------------------------------------
	ois_write32(ep3_ctrl, DmCheck_CheckSumDMA, 0);         // DMA Parity Clear
	ois_write32(ep3_ctrl, DmCheck_CheckSumDMB, 0);         // DMB Parity Clear

	/***** DMA Data Send *****/
	Remainder = ( (LengthDMA*6/4) / BURST_LENGTH_DM );
	for (i=0 ; i< Remainder ; i++) {
		ois_cnt_wrt(ep3_ctrl, (uint8_t*)DataDM, BURST_LENGTH_DM);     // I2Caddresss 1Byte.
		DataDM += BURST_LENGTH_DM;
	}
	Remainder = ( (LengthDMA*6/4) % BURST_LENGTH_DM );
	if (Remainder != 0) {
		ois_cnt_wrt(ep3_ctrl, (uint8_t*)DataDM, (uint8_t)Remainder);  // I2Caddresss 1Byte.
	}
	DataDM += Remainder;

	/***** DMB Data Send *****/
	Remainder = ( (LengthDMB*6/4) / BURST_LENGTH_DM );
	for (i=0 ; i< Remainder ; i++) {
		ois_cnt_wrt(ep3_ctrl, (uint8_t*)DataDM, BURST_LENGTH_DM);     // I2Caddresss 1Byte.
		DataDM += BURST_LENGTH_DM;
	}
	Remainder = ( (LengthDMB*6/4) % BURST_LENGTH_DM );
	if (Remainder != 0) {
		ois_cnt_wrt(ep3_ctrl, (uint8_t*)DataDM, (uint8_t)Remainder);  // I2Caddresss 1Byte.
	}

	//--------------------------------------------------------------------------------
	// 3. Verify
	//--------------------------------------------------------------------------------
	ois_read32(ep3_ctrl, PmCheck_CheckSum, &ReadVerifyPM);
	ois_read32(ep3_ctrl, DmCheck_CheckSumDMA, &ReadVerifyDMA);
	ois_read32(ep3_ctrl, DmCheck_CheckSumDMB, &ReadVerifyDMB);

	if ( (ReadVerifyPM + ReadVerifyDMA + ReadVerifyDMB) != Parity ) {
		TRACE("error! %08x %08x %08x \n", (unsigned int)ReadVerifyPM, (unsigned int)ReadVerifyDMA, (unsigned int)ReadVerifyDMB);
		return( 2 );
	}
	return(0);
}

//********************************************************************************
// Function Name	: ReMapMain
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Pmem Memory> Write Data
// History			: First edition
//********************************************************************************
void RemapMain( void )
{
	ois_write32(ep3_ctrl, 0xF000, 0x00000000) ;
}


//********************************************************************************
// Function Name	: GetInfomationAfterDownload
// Retun Value		: NON
// Argment Value	: NON
// Explanation		: <Pmem Memory> Write Data
// History			: First edition
//********************************************************************************
uint8_t GetInfomationAfterDownload( DSPVER* Info )
{
	uint32_t Data;
	uint32_t UlReadVal;

	ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS , ROMINFO);
	ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);
	if ( (uint8_t)UlReadVal != 0x01 ) return( 1 );

	ois_read32(ep3_ctrl, (SiVerNum + 0), &Data);
	Info->Vendor   = (uint8_t)(Data >> 24);
	Info->User     = (uint8_t)(Data >> 16);
	Info->Model    = (uint8_t)(Data >>  8);
	Info->Version  = (uint8_t)(Data >>  0);
	ois_read32(ep3_ctrl, (SiVerNum + 4), &Data);
	Info->SpiMode  = (uint8_t)(Data >> 24);
	Info->ActType  = (uint8_t)(Data >>  8);
	Info->GyroType = (uint8_t)(Data >>  0);

	return( 0 );
}

//********************************************************************************
// Function Name	: GetInfomationBeforeDownload
// Retun Value		: True(0) / Fail(1)
// Argment Value	: NON
// Explanation		: <Pmem Memory> Write Data
// History			: First edition
//********************************************************************************
uint8_t GetInfomationBeforeDownload( DSPVER* Info, const uint8_t* DataDM, uint32_t LengthDM )
{
	uint32_t i;
	Info->ActType = 0;
	Info->GyroType = 0;

	for ( i=0; i < LengthDM; i+=6 ) {
		if ( (DataDM[0+i] == 0xA0) && (DataDM[1+i] == 0x00) ) {
			Info->Vendor = DataDM[2+i];
			Info->User = DataDM[3+i];
			Info->Model = DataDM[4+i];
			Info->Version = DataDM[5+i];
			if ( (DataDM[6+i] == 0xA0) && (DataDM[7+i] == 0x04) ) {
				Info->SpiMode = DataDM[8+i];
				Info->ActType = DataDM[10+i];
				Info->GyroType = DataDM[11+i];
			}
			return (0);
		}
	}
	return(1);
}


const DOWNLOAD_TBL DTbl[] = {
	{0x0201, LC898124EP3_PM_9_2_1_0, LC898124EP3_PMSize_9_2_1_0, (uint32_t)((uint32_t)LC898124EP3_PMCheckSum_9_2_1_0 + (uint32_t)LC898124EP3_DMA_CheckSum_9_2_1_0 + (uint32_t)LC898124EP3_DMB_CheckSum_9_2_1_0), LC898124EP3_DM_9_2_1_0, LC898124EP3_DMA_ByteSize_9_2_1_0 , LC898124EP3_DMB_ByteSize_9_2_1_0 },
	{0x8201, LC898124EP3_PM_9_2_1_1, LC898124EP3_PMSize_9_2_1_1, (uint32_t)((uint32_t)LC898124EP3_PMCheckSum_9_2_1_1 + (uint32_t)LC898124EP3_DMA_CheckSum_9_2_1_1 + (uint32_t)LC898124EP3_DMB_CheckSum_9_2_1_1), LC898124EP3_DM_9_2_1_1, LC898124EP3_DMA_ByteSize_9_2_1_1 , LC898124EP3_DMB_ByteSize_9_2_1_1 },
	{0xFFFF, (void*)0, 0, 0, (void*)0 ,0 ,0 }
};

int32_t cam_ois_ep3_init(struct cam_ois_ctrl_t *o_ctrl)
{
	int32_t       rc = OIS_INIT;
	int32_t       poll_cnt;
	uint16_t      Model = SEL_MODEL;
	uint8_t       ActSelect = SELECT_ACT;
	DSPVER        Dspcode;
	DOWNLOAD_TBL* ptr;
	uint16_t      m_fw_ver = 0;
	ep3_ctrl = o_ctrl;

	memset(&Dspcode, 0, sizeof(Dspcode));

	e2p_read8(ep3_ctrl, EP3_E2P_SID, MODULE_FW_VER_ADDR, &m_fw_ver);
	CAM_ERR(CAM_OIS, "Enter %s 0x%x", OIS_INFO, m_fw_ver);

	ptr = ( DOWNLOAD_TBL * )DTbl;
	while (ptr->Cmd != 0xFFFF) {
		if ( ptr->Cmd == ( ((uint16_t)Model << 8) + ActSelect) ) break;
		ptr++ ;
	}

	if (ptr->Cmd == 0xFFFF) return -(0xF0);
	if ( GetInfomationBeforeDownload( &Dspcode, ptr->DataDM, ( ptr->LengthDMA +	ptr->LengthDMB ) ) != 0 ) return -(0xF1);

	if ( (ActSelect != Dspcode.ActType) || (Model != Dspcode.Model) ) return -(0xF2);

	rc = DownloadToEP3( ptr->DataPM, ptr->LengthPM, ptr->Parity, ptr->DataDM, ptr->LengthDMA , ptr->LengthDMB );
	if (rc) {
		CAM_ERR(CAM_OIS, "init fail rc = %d", rc);
		return -rc;
	}
	RemapMain();

	poll_cnt = cam_ois_ep3_poll_ready();

	// Gyro Initial
	ois_write32(ep3_ctrl, 0xF01E, 0x6B800000);
	WitTim(70);
	ois_write32(ep3_ctrl, 0xF01E, 0x6B010000);
	WitTim(50);
	ois_write32(ep3_ctrl, 0xF01E, 0x1B100000);
	WitTim(5);
	ois_write32(ep3_ctrl, 0xF01F, 0x00000001);

	CAM_ERR(CAM_OIS, "%s init done. %d", OIS_INFO, poll_cnt);

	if (ep3_ctrl->is_ois_aat == 1) {
		CAM_ERR(CAM_OIS, "OIS AAT start");
		usleep_range(70 * 1000, 70 * 1000 + 10); //70msec
		rc = cam_ois_ep3_selftest();
		if (rc != OIS_SUCCESS) return OIS_STOP_TEST;
		rc = cam_ois_ep3_calibration();
		if (rc != OIS_SUCCESS) return OIS_STOP_TEST;
		rc = cam_ois_ep3_selftest2();
		if (rc != OIS_SUCCESS) return OIS_STOP_TEST;
		CAM_ERR(CAM_OIS, "OIS AAT end");
	}

    // Read Gyro Gains
	o_ctrl->gyro_gain_x = 0;
	ois_read16(o_ctrl, 0x82B8, &(o_ctrl->gyro_gain_x));

	if(o_ctrl->gyro_gain_x == 0 || o_ctrl->gyro_gain_x == 0xFFFF) {
		o_ctrl->gyro_gain_x = OIS_GYRO_DEFAULT_GAIN;
	}

	o_ctrl->gyro_gain_y = 0;
    ois_read16(o_ctrl, 0x8318, &(o_ctrl->gyro_gain_y));

	if(o_ctrl->gyro_gain_y == 0 || o_ctrl->gyro_gain_y == 0xFFFF) {
		o_ctrl->gyro_gain_y = OIS_GYRO_DEFAULT_GAIN;
	}

	CAM_INFO(CAM_OIS, "OIS gyro_gain = %x, %x", o_ctrl->gyro_gain_x, o_ctrl->gyro_gain_y);

	return rc;
}
uint32_t cam_ois_ep3_calibration(void)
{
	uint32_t  UlRsltSts;
	int32_t   SlMeasureParameterA , SlMeasureParameterB;
	int32_t   SlMeasureParameterNum;
	UnllnVal  StMeasValueA , StMeasValueB;
	int32_t   SlMeasureAveValueA , SlMeasureAveValueB;
	uint32_t  val_gyro_offset_x, val_gyro_offset_y;
	uint8_t   e2p_gyro_offset_x[2], e2p_gyro_offset_y[2];
	uint32_t  result = cam_ois_selftest_get();

	UlRsltSts = ChecksumVerification();
	if (UlRsltSts < 0) {
		CAM_ERR(CAM_OIS, "E2P checksum fail");
		return UlRsltSts;
	}

	ois_read32(ep3_ctrl, 0x338, &val_gyro_offset_x);
	ois_read32(ep3_ctrl, 0x33C, &val_gyro_offset_y);

	CAM_ERR(CAM_OIS, "EP3 calibration start. val_gyro_offset_x: %d, val_gyro_offset_y: %d",
		(int16_t)(val_gyro_offset_x >> 16),
		(int16_t)(val_gyro_offset_y >> 16));

	MesFil();//(THROUGH);                               // Set Measure Filter

	SlMeasureParameterNum   =   GYROF_NUM;              // Measurement times
	SlMeasureParameterA     =   GYRO_RAM_GX_ADIDAT;     // Set Measure RAM Address
	SlMeasureParameterB     =   GYRO_RAM_GY_ADIDAT;     // Set Measure RAM Address

	MeasureStart(SlMeasureParameterNum , SlMeasureParameterA , SlMeasureParameterB);            // Start measure

	MeasureWait();              // Wait complete of measurement

	ois_read32(ep3_ctrl, StMeasFunc_MFA_LLiIntegral1    , &StMeasValueA.StUllnVal.UlLowVal);    // X axis
	ois_read32(ep3_ctrl, StMeasFunc_MFA_LLiIntegral1 + 4, &StMeasValueA.StUllnVal.UlHigVal);
	ois_read32(ep3_ctrl, StMeasFunc_MFB_LLiIntegral2    , &StMeasValueB.StUllnVal.UlLowVal);    // Y axis
	ois_read32(ep3_ctrl, StMeasFunc_MFB_LLiIntegral2 + 4, &StMeasValueB.StUllnVal.UlHigVal);

	SlMeasureAveValueA = (int32_t)((INT64)StMeasValueA.UllnValue / SlMeasureParameterNum);
	SlMeasureAveValueB = (int32_t)((INT64)StMeasValueB.UllnValue / SlMeasureParameterNum);


	SlMeasureAveValueA = (SlMeasureAveValueA >> 16) & 0x0000FFFF;
	SlMeasureAveValueB = (SlMeasureAveValueB >> 16) & 0x0000FFFF;

	UlRsltSts = EXE_END;
	StAdjPar.StGvcOff.UsGxoVal = (uint16_t)(SlMeasureAveValueA & 0x0000FFFF);             // Measure Result Store
	if (((INT16)StAdjPar.StGvcOff.UsGxoVal > (INT16)GYROF_UPPER) || ((INT16)StAdjPar.StGvcOff.UsGxoVal < (INT16)GYROF_LOWER)) {
		UlRsltSts |= EXE_GXADJ;
	}
	ois_write32(ep3_ctrl, GYRO_RAM_GXOFFZ, ((SlMeasureAveValueA << 16) & 0xFFFF0000));   // X axis Gyro offset

	StAdjPar.StGvcOff.UsGyoVal = (uint16_t)(SlMeasureAveValueB & 0x0000FFFF);   //Measure Result Store
	if (((INT16)StAdjPar.StGvcOff.UsGyoVal > (INT16)GYROF_UPPER) || ((INT16)StAdjPar.StGvcOff.UsGyoVal < (INT16)GYROF_LOWER)) {
		UlRsltSts |= EXE_GYADJ;
	}
	ois_write32(ep3_ctrl, GYRO_RAM_GYOFFZ, ((SlMeasureAveValueB << 16) & 0xFFFF0000));   // Y axis Gyro offset
	ois_write32(ep3_ctrl, GYRO_RAM_GYROX_OFFSET, 0x00000000);                             // X axis Drift Gyro offset
	ois_write32(ep3_ctrl, GYRO_RAM_GYROY_OFFSET, 0x00000000);                             // Y axis Drift Gyro offset
	ois_write32(ep3_ctrl, GyroFilterDelayX_GXH1Z2, 0x00000000);                           // X axis H1Z2 Clear
	ois_write32(ep3_ctrl, GyroFilterDelayY_GYH1Z2, 0x00000000);                           // Y axis H1Z2 Clear

	UlRsltSts = WrGyroOffsetData();
	if (UlRsltSts != OIS_SUCCESS) {
		CAM_ERR(CAM_OIS, "E2P checksum fail");
		return UlRsltSts;
	}
	result |= 1;

	ois_read32(ep3_ctrl, 0x338, &val_gyro_offset_x);
	ois_read32(ep3_ctrl, 0x33C, &val_gyro_offset_y);

	BurstReadE2Prom(0x62, e2p_gyro_offset_x, 2);
	BurstReadE2Prom(0x64, e2p_gyro_offset_y, 2);

	CAM_ERR(CAM_OIS, "EP3 calibration end. Gyro offset mem_x: %5d mem_y: %5d e2p_x: %5d e2p_y: %5d",
		(int16_t)(val_gyro_offset_x >> 16),
		(int16_t)(val_gyro_offset_y >> 16),
		(int16_t)((e2p_gyro_offset_x[1]<<8) + e2p_gyro_offset_x[0]),
		(int16_t)((e2p_gyro_offset_y[1]<<8) + e2p_gyro_offset_y[0]));

	if (((int16_t)(val_gyro_offset_x >> 16) != (int16_t)((e2p_gyro_offset_x[1]<<8) + e2p_gyro_offset_x[0])) ||
		(int16_t)(val_gyro_offset_y >> 16) != (int16_t)((e2p_gyro_offset_y[1]<<8) + e2p_gyro_offset_y[0])) {
		CAM_ERR(CAM_OIS, "Calibration error.");
		return OIS_FAIL;
	}

	result |= (1 << 2);
	cam_ois_selftest_set(result);

	return OIS_SUCCESS;
}

int32_t cam_ois_ep3_stat(sensor_ois_stat_t *data)
{
	sensor_ois_stat_t ois_stat;
	uint32_t val_gyro_x, val_gyro_y;
	uint32_t val_gyro_offset_x, val_gyro_offset_y;

	memset(&ois_stat, 0, sizeof(ois_stat));

	/* Gyro Read by reg */
	ois_read32(ep3_ctrl, 0x310, &val_gyro_x);
	ois_read32(ep3_ctrl, 0x314, &val_gyro_y);
	ois_read32(ep3_ctrl, 0x338, &val_gyro_offset_x);
	ois_read32(ep3_ctrl, 0x33C, &val_gyro_offset_y);

	ois_stat.gyro[0] = (int16_t)(val_gyro_x >> 16);
	ois_stat.gyro[1] = (int16_t)(val_gyro_y >> 16);
	ois_stat.offset[0] = (int16_t)(val_gyro_offset_x >> 16);
	ois_stat.offset[1] = (int16_t)(val_gyro_offset_y >> 16);
	ois_stat.is_stable = 1;

	*data = ois_stat;

	CAM_ERR(CAM_OIS,"gyro x %d gyro y %d offset x %d offset y %d",
		ois_stat.gyro[0], ois_stat.gyro[1], ois_stat.offset[0], ois_stat.offset[1]);

	return OIS_SUCCESS;
}

int32_t cam_ois_ep3_move_lens(void *data)
{
	int32_t hallx = 0, hally = 0, rHallx = 0, rHally = 0;
	int16_t offset[2];

	memcpy(offset, data, sizeof(offset));

	hallx = offset[0] << 16;
	hally = offset[1] << 16;

	ois_write32(ep3_ctrl, 0x1C4, hallx);
	ois_write32(ep3_ctrl, 0x218, hally);
	usleep_range(100000, 100010);

	ois_read32(ep3_ctrl, 0x1C8, (uint32_t *)&rHallx);
	ois_read32(ep3_ctrl, 0x21C, (uint32_t *)&rHally);

	if ((abs((rHallx >> 16) - offset[0]) < HALL_LIMIT) && (abs((rHally >> 16) - offset[1]) < HALL_LIMIT)) {
		CAM_ERR(CAM_OIS, "Target x: %5d  y: %5d diff x: %5d  y: %5d",
			offset[0], offset[1],
			rHallx >> 16, rHally >> 16);

		return	OIS_SUCCESS;
	}

	CAM_ERR(CAM_OIS, "Target x: %5d  y: %5d diff x: %5d  y: %5d  <-- move fail!!!",
			offset[0], offset[1],
			rHallx >> 16, rHally >> 16);

	return OIS_FAIL;
}

/*===========================================================================
 * FUNCTION    - cam_ois_ep3_selftest -
 *
 * DESCRIPTION: ois self-test routine for all-auto-test
 *==========================================================================*/
#define GYRO_RAW_LIMIT     30  // 30[dps]*175
#define GYRO_CAL_LIMIT     10  // 10[dps]*175
#define GYRO_SCALE_FACTOR  175 //Gyro sensitivity LSB/dps

#define GYRO_RAW_X_DPS ois_stat.gyro[0] / GYRO_SCALE_FACTOR
#define GYRO_RAW_Y_DPS ois_stat.gyro[1] / GYRO_SCALE_FACTOR
#define GYRO_CAL_X_DPS (ois_stat.gyro[0] - ois_stat.offset[0]) / GYRO_SCALE_FACTOR
#define GYRO_CAL_Y_DPS (ois_stat.gyro[1] - ois_stat.offset[1]) / GYRO_SCALE_FACTOR

#define  UNSTABLE_RATIO 40 //0.4 * 100

static int8_t cam_ois_ep3_selftest(void)
{
	int32_t	rc = OIS_SUCCESS;
	uint32_t result = 0;
	int i = 0;
	sensor_ois_stat_t ois_stat;
	int16_t hall_target[9][2] = {
		{ 10010,     0}, {  7078,  6866}, {     0,  9710},
		{ -7078,  6866}, {-10010,     0}, { -7078, -6866},
		{     0, -9710}, {  7078, -6866}, { 10010,     0}
	};

	memset(&ois_stat, 0, sizeof(ois_stat));

	CAM_ERR(CAM_OIS, " enter");
	//0. is ois init success ?
	result |= (1 << 5);

	//1. check lens movement range
	result |= (1 << 4);
	for (i = 0; i <= 8; i++) {
		if (cam_ois_ep3_move_lens(&hall_target[i]) < 0)
			result &= ~(1 << 4);
	}

	//2. reset lens position before ois turn-on.
	{
		uint16_t offset[2] = {0, 0};
		cam_ois_ep3_move_lens(&offset);
	}

	//3. get first stat value.
	cam_ois_ep3_stat(&ois_stat);

	//4. check ois module is alive.
	if (ois_stat.gyro[0] == 0 && ois_stat.gyro[1] == 0 && ois_stat.offset[0] == 0 && ois_stat.offset[1] == 0) {
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
 * FUNCTION    - cam_ois_ep3_selftest2 -
 *
 * DESCRIPTION: ois self-test routine #2 for all-auto-test
 *==========================================================================*/
static int8_t cam_ois_ep3_selftest2(void)
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
		rc = cam_ois_ep3_stat(&ois_stat);
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

void BurstReadE2Prom(unsigned char address, unsigned char * val, unsigned char cnt)
{
	uint32_t UlReadVal;
	unsigned char i;

	DMIOWrite32(E2P_ADR, address);       // Start Address
	DMIOWrite32(E2P_ASCNT, (cnt -1));    // Count Number
	DMIOWrite32(E2P_CMD, 1);             // Re-Program
	// Read Exe
	ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS, E2P_RDAT);
	for (i=0; i<cnt; i++) {
		ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);    // Read Access
		val[i] = (unsigned char)UlReadVal;
	}
}

void ReadE2Prom(unsigned char address, unsigned char * val)
{
	uint32_t UlReadVal;

	DMIOWrite32(E2P_ADR, address);  // Start Address
	DMIOWrite32(E2P_ASCNT, 0);      // Count Number
	DMIOWrite32(E2P_CMD, 1);        // Re-Program
	// Read Exe
	ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS, E2P_RDAT);
	ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);         // Read Access

	*val = (unsigned char)UlReadVal;
}

unsigned char UnlockCodeClear(void)
{
	uint32_t UlReadVal;

	ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS, E2P_WPB);            // UNLOCK_CLR(E0_7014h[4])=1
	ois_write32(ep3_ctrl, CMD_IO_DAT_ACCESS, 0x00000010);
	ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);
	if ((UlReadVal & 0x00000080) != 0) return(3);

	return(0);
}

unsigned char UnlockCodeSet(void)
{
	uint32_t UlReadVal;

	DMIOWrite32(E2P_UNLK_CODE1, 0xAAAAAAAA);    // UNLK_CODE1(E0_7554h) = AAAA_AAAAh
	DMIOWrite32(E2P_UNLK_CODE2, 0x55555555);    // UNLK_CODE2(E0_7AA8h) = 5555_5555h
	DMIOWrite32(E2P_RSTB,        0x00000001);   // RSTB_FLA_WR(E0_74CCh[0])=1
	DMIOWrite32(E2P_CLKON,   0x00000010);       // FLA_WR_ON(E0_7664h[4])=1
	DMIOWrite32(E2P_UNLK_CODE3, 0x0000ACD5);    // Additional Unllock Code Set

	ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS, E2P_WPB);
	ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);
	if ((UlReadVal & 0x00000002) != 2) return(1);

	return(0);
}

uint8_t ChecksumVerification( void )
{
	uint8_t data[CHECK_SUM_NUM], cnt;
	uint32_t ReadVerify, Parity;

//------------------------------------------------------------------------------------------------
// Checksum Verification
//------------------------------------------------------------------------------------------------
	BurstReadE2Prom( EEPROM_ONSEMI_IDSEL, data, CHECK_SUM_NUM );
	ReadVerify = 0;
	for ( cnt=0; cnt < CHECK_SUM_NUM; cnt++ ) {
		ReadVerify += data[cnt];
	}
	ReadE2Prom( CHECK_SUM_ADR, &cnt );
	Parity = cnt;
	if ((uint8_t)ReadVerify != (uint8_t)Parity) return OIS_INIT_CHECKSUM_ERROR;

	return OIS_SUCCESS;

}

//********************************************************************************
// Function Name    : WrGyroOffsetData
// Retun Value      : error
// Argment Value    : NON
// Explanation      : Write data to E2Prom
// History          : First edition
//********************************************************************************
uint8_t WrGyroOffsetData( void )
{
	uint32_t UlReadVal, UlCnt;
	uint8_t  ans, data[CHECK_SUM_NUM], cnt;
	uint32_t ReadVerify, Parity;

    // Flash write
	ans = UnlockCodeSet();
	if ( ans != 0 ) return ( 1 );    // Unlock Code Set
//------------------------------------------------------------------------------------------------
// Page 1 (0x10-0x1F)
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Page 2 (0x20-0x2F)
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Page 3 (0x30-0x32)
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Page 5 (0x50-0x5F)
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Page 6 (0x60-0x6F)
//------------------------------------------------------------------------------------------------
	DMIOWrite32( E2P_ADR, 0x60 );   // Start Address
	DMIOWrite32( E2P_DFG, 0 );      // FLG CLR

	DMIOWrite32( E2P_WDAT02, (uint8_t)(StAdjPar.StGvcOff.UsGxoVal>>LSB) );
	DMIOWrite32( E2P_WDAT03, (uint8_t)(StAdjPar.StGvcOff.UsGxoVal>>MSB) );
	DMIOWrite32( E2P_WDAT04, (uint8_t)(StAdjPar.StGvcOff.UsGyoVal>>LSB) );
	DMIOWrite32( E2P_WDAT05, (uint8_t)(StAdjPar.StGvcOff.UsGyoVal>>MSB) );
#if SEL_SHIFT_COR == 1
	DMIOWrite32( E2P_WDAT06, (uint8_t)(StAdjPar.StGvcOff.UsGzoVal>>LSB) );
	DMIOWrite32( E2P_WDAT07, (uint8_t)(StAdjPar.StGvcOff.UsGzoVal>>MSB) );
#endif  //SEL_SHIFT_COR

	DMIOWrite32( E2P_CMD, 2 );          // Re-Program
	WitTim( 20 ) ;
	UlCnt=0;
	do {
		if ( UlCnt++ > 10 ) {
			UnlockCodeClear();                          // Unlock Code Clear
			return( 4 );
		}
		ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS ,E2P_INT);
		ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);
		} while ( (UlReadVal & 0x00000080) != 0 );

//------------------------------------------------------------------------------------------------
// CheckSum Creating
//------------------------------------------------------------------------------------------------
	BurstReadE2Prom( EEPROM_ONSEMI_IDSEL, data, CHECK_SUM_NUM );
	Parity = 0;
	for ( cnt=0; cnt < CHECK_SUM_NUM; cnt++ ) {
		Parity += data[cnt];
	}

//------------------------------------------------------------------------------------------------
// Page 7 (0x70-0x7F)
//------------------------------------------------------------------------------------------------
	DMIOWrite32( E2P_ADR, 0x70 );                 // Start Address
	DMIOWrite32( E2P_DFG, 0 );                    // FLG CLR

	DMIOWrite32( E2P_WDAT13, (uint8_t)(Parity) ); // CheckSum:0x7D

	DMIOWrite32( E2P_CMD, 2 );                    // Re-Program
	WitTim( 20 ) ;
	UlCnt=0;
	do {
		if ( UlCnt++ > 10 ) {
			UnlockCodeClear();                    // Unlock Code Clear
			return( 5 );
		}
		ois_write32(ep3_ctrl, CMD_IO_ADR_ACCESS, E2P_INT);
		ois_read32(ep3_ctrl, CMD_IO_DAT_ACCESS, &UlReadVal);
	} while ( (UlReadVal & 0x00000080) != 0 );

	UnlockCodeClear();                            // Unlock Code Clear
//------------------------------------------------------------------------------------------------
// Checksum Verification
//------------------------------------------------------------------------------------------------
	BurstReadE2Prom( EEPROM_ONSEMI_IDSEL, data, CHECK_SUM_NUM );
	ReadVerify = 0;
	for ( cnt=0; cnt < CHECK_SUM_NUM; cnt++ ) {
		ReadVerify += data[cnt];
	}
	ReadE2Prom( CHECK_SUM_ADR, &cnt );
	Parity = cnt;
	if ( (uint8_t)ReadVerify != (uint8_t)Parity) return( 6 );

	return( OIS_SUCCESS );

}

void WitTim(unsigned short UsWitTim)
{
	usleep_range(1000 * UsWitTim, 1000 * UsWitTim + 10); /* wait [UsWitTim]ms */
}
