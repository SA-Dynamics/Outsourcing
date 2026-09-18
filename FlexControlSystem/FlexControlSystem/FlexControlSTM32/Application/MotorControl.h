#ifndef MOTORCONTROL_H
#define MOTORCONTROL_H

#include "main.h"

#define DEFAULT_VOLTAGE_VALUE		10
#define MOTION_INFO_BUFFER_SIZE		16

typedef enum
{
	MOTION_NONE = 0,
	MOTION_UP,
	MOTION_DOWN,
	MOTION_LEFT,
	MOTION_RIGHT,
	MOTION_GATHER,
}MotionIndex;


typedef struct
{
	MotionIndex eMotionIndex;
	float fVoltage;
	float fFrequency;
	float fTimeUse;
}MotorMotionParams;

// weak, 外部实现
void CbMotionFinish(const uint8_t u8Index);


void SendMotorInfo(MotorMotionParams *pParams);
void SetNormal(const bool bNormal);


#endif
