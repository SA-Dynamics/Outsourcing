#ifndef IMU_H
#define IMU_H


#include "main.h"

#define UART_RECV_DATA_BUFFER_SIZE 		256
#define UART_SEND_DATA_LEN 				32
#define UART_SEND_BUFFER_SIZE			16
#define SERIAL_INFO_BUFFER_SIZE			16


typedef enum
{
	INFO_NONE = 0,
	INFO_HEART,
	INFO_MOTION,
	INFO_STOP_MOTION,
}InfoType;

typedef enum
{
	RESPOND_NONE = 0,
	RESPOND_HEART,
	RESPOND_MOTION_FINISH,
}RespondType;



typedef struct
{
	InfoType eInfo;
	uint8_t u8MotionIndex;
	float fMotionVoltage;
	float fMotionFrequency;
	float fMotionTimeUse;
}SerialInfo;

extern void CbUartIdleHandler(UART_HandleTypeDef *huart);

void SerialHandleInit(void);
void SendSerialRespond(const RespondType eType, void *pParams);
bool GetSerialInfo(SerialInfo *pInfo);
void SerialDataHandle(void);

#endif
