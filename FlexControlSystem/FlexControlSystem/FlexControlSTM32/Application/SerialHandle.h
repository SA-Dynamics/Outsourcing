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
	INFO_SAVE_IP,			// 保存 IP 到 Flash(上位机下发新 IP, 下位机重启后生效)
	INFO_GET_IP,			// 获取当前 IP(无数据, 应答 0x12 带 4 字节 IP)
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
	RESPOND_SAVE_IP,		// 参数为 uint8_t*: 1=保存成功, 0=失败
	RESPOND_GET_IP,			// 参数为 uint8_t[4]: 当前运行的 IP
}RespondType;



typedef struct
{
	InfoType eInfo;
	uint8_t u8MotionIndex;
	float fMotionVoltage;
	float fMotionFrequency;
	float fMotionTimeUse;
	int16_t s16AngleOffset;
	int8_t s8ModifyCoeff;
	// 光源参数
	uint8_t u8LightIndex;			// 1=A光源, 2=B光源
	uint8_t u8LightBrightness;		// 0~100
	uint16_t u16LightKeepTime;		// 保持时间, 单位 100ms(协议中为秒数x10)
	// 保存IP参数(0x0F): 4 字节新 IP
	uint8_t u8Ip[4];
}SerialInfo;

extern void CbUartIdleHandler(UART_HandleTypeDef *huart);

void SerialHandleInit(void);
void SendSerialRespond(const RespondType eType, void *pParams);
bool GetSerialInfo(SerialInfo *pInfo);
void SerialDataHandle(void);

// 网络帧解析入口: 帧布局与串口一致, 不含帧头帧尾: [cmd][len][data...][chkH][chkL]
// 校验已由网络层(Network.c)完成, 这里只做命令解析, 结果进与串口相同的
// SerialInfo 队列, 由 FlexControl 统一分发
void SerialHandleNetworkFrame(const uint8_t *pFrame, const uint8_t u8Len);

#endif
