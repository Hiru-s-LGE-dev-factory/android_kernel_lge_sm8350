#define DEBUG
#include "pixart_ots.h"

#define EVENT_COUNT_TH		1

/* Global Variables */
static int32_t x_sum;
static int32_t pre_dsCountX;
static int32_t ds_x_sum;

static int32_t y_sum;
static int32_t pre_dsCountY;
static int32_t ds_y_sum;

static void OTS_WriteRead(uint8_t address, uint8_t wdata)
{
	uint8_t read_value;
	do {
		/* Write data to specified address */
		WriteData(address, wdata);
		/* Read back previous written data */
		read_value = ReadData(address);
		/* Check if the data is correctly written */
	} while (read_value != wdata);
	return;
}

void OTS_Reset_Variables(void)
{
	/* reset variables */
	x_sum = 0;
	pre_dsCountX = 0;
	ds_x_sum = 0;
	y_sum = 0;
	pre_dsCountY = 0;
	ds_y_sum = 0;
}

static uint8_t Detect_Tilt(int32_t dsCountX)
{
	int32_t diff_count = 0;
	uint8_t OutTiltState = OTS_ROT_NO_CHANGE;

	diff_count = dsCountX - pre_dsCountX;
	if (diff_count >= EVENT_COUNT_TH)	{
		pre_dsCountX = dsCountX;
		OutTiltState = OTS_ROT_DOWN;
	}	else if (diff_count <= (-EVENT_COUNT_TH))	{
		pre_dsCountX = dsCountX;
		OutTiltState = OTS_ROT_UP;
	}
	return OutTiltState;
}

static uint8_t Detect_Rotation(int32_t dsCountY)
{
	int32_t diff_count = 0;
	uint8_t OutRotState = OTS_ROT_NO_CHANGE;

	diff_count = dsCountY - pre_dsCountY;
	if (diff_count >= EVENT_COUNT_TH)	{
		pre_dsCountY = dsCountY;
		OutRotState = OTS_ROT_DOWN;
	}	else if (diff_count <= (-EVENT_COUNT_TH))	{
		pre_dsCountY = dsCountY;
		OutRotState = OTS_ROT_UP;
	}
	return OutRotState;
}

uint8_t OTS_Detect_Rotation(int16_t dy16)
{
	ds_y_sum += dy16;
	return Detect_Rotation(ds_y_sum);
}

uint8_t OTS_Detect_Tilt(int16_t dx16)
{
	ds_x_sum += dx16;
	return Detect_Tilt(ds_x_sum);
}

bool initFlag = false;
void OTS_Sensor_Init()
{
	unsigned char sensor_pid = 0, read_id_ok = 0;
	/* Read sensor_pid in address 0x00 to check if the
	 serial link is valid, read value should be 0x31. */
	sensor_pid = ReadData(0x00);
	if (sensor_pid == 0x31) {
		read_id_ok = 1;

	    pr_debug("%s (%d) : sensor_pid ok \n",__func__, __LINE__);
		/* PAT9125 sensor recommended settings: */
		/* switch to bank0, not allowed to perform OTS_RegWriteRead */
		WriteData(0x7F, 0x00);
		/* software reset (i.e. set bit7 to 1).
		It will reset to 0 automatically */
		/* so perform OTS_RegWriteRead is not allowed. */
		WriteData(0x06, 0x97);

		delay_ms(3);				/* delay 1ms */
		WriteData(0x06, 0x17);

		/* disable write protect */
		OTS_WriteRead(0x09, 0x5A);
		/* set X-axis resolution (depends on application) */
		OTS_WriteRead(0x0D, 0x00);
		/* set Y-axis resolution (depends on application) */
		OTS_WriteRead(0x0E, 0x24);
		/* set 12-bit X/Y data format (depends on application) */
		OTS_WriteRead(0x19, 0x04);
		/* ONLY for VDD=VDDA=1.7~1.9V: for power saving */
		OTS_WriteRead(0x4B, 0x04);
		OTS_WriteRead(0x7C, 0x82);
		/* ONLY used ots counter*/
		OTS_WriteRead(0x2B, 0x6D);
		OTS_WriteRead(0x2D, 0x00);
		/* Shutter optimization routine */
		OTS_WriteRead(0x35, 0xC8);
		OTS_WriteRead(0x36, 0x46);
		OTS_WriteRead(0x37, 0x96);
		OTS_WriteRead(0x38, 0x91);
		OTS_WriteRead(0x39, 0x87);
		OTS_WriteRead(0x3A, 0x82);
		OTS_WriteRead(0x3B, 0x22);
		/*enable write protect */
		OTS_WriteRead(0x09, 0x00);
		delay_ms(5);
		initFlag = true;
		OTS_Reset_Variables();
	}else{
		initFlag = false;
		pr_debug("%s (%d) : pat9125 sensor_pid not ok \n",__func__, __LINE__);
	}
}

void OTS_Sensor_ReadMotion(int16_t *dx, int16_t *dy)
{
	int16_t deltaX_l=0, deltaY_l=0, deltaXY_h=0;
	int16_t deltaX_h=0, deltaY_h=0;
	int16_t condition_bit=0;

	condition_bit = ReadData(0x02);
	/* check motion bit in bit7 */
	if (condition_bit & MOTION_BIT) {
		deltaX_l = ReadData(0x03);
		deltaY_l = ReadData(0x04);
		deltaXY_h = ReadData(0x12);

		deltaX_h = (deltaXY_h << 4) & 0xF00;
		if (deltaX_h & 0x800)
			deltaX_h |= 0xf000;

		deltaY_h = (deltaXY_h << 8) & 0xF00;
		if (deltaY_h & 0x800)
			deltaY_h |= 0xf000;
	}

	/* inverse the data (depends on sensor's orientation and application) */
	*dx = -(deltaX_h | deltaX_l);
	/* inverse the data (depends on sensor's orientation and application) */
	*dy = -(deltaY_h | deltaY_l);
}


void OTS_Sensor_Find_MarkingPosition(int *position, int *md, int *shutter, int shutter_th)
{
	int16_t markD = NON_DETECT;
	int16_t count = *position;
	unsigned char Shutter = ReadData(0x14);
	//read hall ic status 

	if(Shutter >= shutter_th){
		if(count <= (PIXEL_MIN + PIXEL_MIN_OFFSET)){
			markD = DETECT_COLLAPSE;
			count = PIXEL_MIN;
		}
		else if((count <= (PIXEL_HALF + PIXEL_CAL_OFFSET)) && (count >= (PIXEL_HALF - PIXEL_CAL_OFFSET))){
			markD = DETECT_HALF;
			count = PIXEL_HALF;
		}
		else if(count >= (PIXEL_MAX - PIXEL_MARK_OFFSET)){
			markD = DETECT_EXPAND;
			count = PIXEL_MAX;
		}
		else{
			markD = DETECT_UNKNOWN;
		}
	}

//	if(pat9125data.hallicStatus == LGE_SLIDE_CLOSED && markD > NON_DETECT){
//		count = PIXEL_MIN;
//	}
//	*position = count;
	*shutter = Shutter;
	*md = markD;
}
