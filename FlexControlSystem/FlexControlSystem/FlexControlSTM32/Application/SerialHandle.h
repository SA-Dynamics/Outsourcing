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
	INFO_BIN_MOTION,
	INFO_STOP_BIN_MOTION,
	INFO_LIGHT_ON,
	INFO_LIGHT_OFF,
}InfoType;

typedef enum
{
	RESPOND_NONE = 0,
	RESPOND_HEART,
	RESPOND_MOTION_FINISH,
	RESPOND_MOTION_STOP,
	RESPOND_BIN_MOTION_FINISH,
	RESPOND_BIN_MOTION_STOP,
	RESPOND_LIGHT_ON,
	RESPOND_LIGHT_OFF,
}RespondType;



typedef struct
{
	InfoType eInfo;
	uint8_t u8MotionIndex;
	float fMotionVoltage;
	float fMotionFrequency;
	float fMotionTimeUse;
	int16_t s16AngleOffset;
	uint8_t u8SpeedCoeff;
	// 光源参数
	uint8_t u8LightIndex;			// 1=A光源, 2=B光源
	uint8_t u8LightBrightness;		// 0~100
	uint16_t u16LightKeepTime;		// 保持时间, 单位 100ms(协议中为秒数x10)
}SerialInfo;

extern void CbUartIdleHandler(UART_HandleTypeDef *huart);

void SerialHandleInit(void);
void SendSerialRespond(const RespondType eType, void *pParams);
bool GetSerialInfo(SerialInfo *pInfo);
void SerialDataHandle(void);

#endif
