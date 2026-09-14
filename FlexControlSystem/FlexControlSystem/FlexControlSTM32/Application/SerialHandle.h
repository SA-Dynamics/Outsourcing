#ifndef IMU_H
#define IMU_H


#include "main.h"

#define UART_DATA_BUFFER_SIZE 		256
#define SERIAL_INFO_BUFFER_SIZE		16


typedef enum
{
	INFO_NONE = 0,
	INFO_HEART,
	INFO_MOTION,
}InfoTypeype;





typedef struct
{
	InfoTypeype eInfo;
	uint8_t u8MotionIndex;
}SerialInfo;

extern void CbUartIdleHandler(UART_HandleTypeDef *huart);

//void IMU_Init(void);
bool GetSerialInfo(SerialInfo *pInfo);
void SerialDataHandle(void);

#endif
