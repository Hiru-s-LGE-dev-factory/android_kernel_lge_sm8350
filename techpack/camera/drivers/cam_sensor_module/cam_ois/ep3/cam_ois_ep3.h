#ifndef CAM_OIS_EP3_H
#define CAM_OIS_EP3_H

#include "cam_ois_custom.h"

//****************************************************
//	CUSTOMER NECESSARY CREATING LIST
//****************************************************
/* for I2C Multi Translation : Burst Mode*/
void BurstReadE2Prom( unsigned char address, unsigned char * val, unsigned char cnt );

/* for Wait timer [Need to adjust for your system] */
void WitTim( unsigned short	UsWitTim );

uint8_t       ChecksumVerification( void );
uint8_t       WrGyroOffsetData( void );
uint32_t      cam_ois_ep3_calibration(void);
static int8_t cam_ois_ep3_selftest(void);
static int8_t cam_ois_ep3_selftest2(void);

#endif
