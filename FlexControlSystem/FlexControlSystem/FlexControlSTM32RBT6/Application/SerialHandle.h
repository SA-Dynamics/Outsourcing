#ifndef IMU_H
#define IMU_H


#include "main.h"
#include "KeyParameters.h"

#define UART_RECV_DATA_BUFFER_SIZE 		256
/*
 * 串口发送帧缓冲长度(单帧上限)。
 * 2026-11-23 由 32 放大到 64: "获取固件参数"的应答 0x16 数据域就有 49 字节,
 * 整帧 = 2(帧头) + 1(cmd) + 1(len) + 49 + 2(校验) + 2(帧尾) = 57 字节,
 * 32 字节的缓冲装不下 —— 会被 memcpy 截断/踩坏相邻队列项。
 * 以后再有任何长数据指令, 先回头看这里和 SaveRespondFrame 里的 RESPOND_FRAME_MAX_SIZE。
 */
#define UART_SEND_DATA_LEN 				64
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
	INFO_GET_VOLTAGE,		// 获取电压(无数据, 应答 0x14 带 2 字节 电压x10)
	INFO_GET_KEY_PARAMS,	// 获取固件参数(无数据, 应答 0x16 带 1 状态 + 48 字节 float)
	INFO_SAVE_KEY_PARAMS,	// 保存固件参数(数据 49 字节: 标志 + 48 字节 float, 应答 0x18)
	INFO_GET_FW_VERSION,	// 获取固件版本(无数据, 应答 0x1A 带 3 字节: 主/次/修订)
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
	RESPOND_GET_VOLTAGE,	// 参数为 uint16_t*: 电压x10(如 241 = 24.1V)
	RESPOND_GET_KEY_PARAMS,	// 参数为 uint8_t[49]: 首字节状态 + 48 字节 float 裸数据
	RESPOND_SAVE_KEY_PARAMS,// 参数为 uint8_t*: 0=保存成功, 1=失败(与保存IP 相反)
	RESPOND_GET_FW_VERSION,	// 参数为 uint8_t[3]: major / minor / patch
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
	// 数据域末尾追加的 1 字节(2026-11-22 协议扩展):
	// 1 = 无限时间动作(一直动作, 直到收到停止指令), 0 = 按 fMotionTimeUse 计时
	uint8_t u8InfiniteTime;
	// 光源参数
	uint8_t u8LightIndex;			// 1=A, 2=B, 3=C, 4=D; 0仅用于关闭全部
	uint8_t u8LightBrightness;		// 0~100
	uint32_t u32LightKeepTime;		// 保持时间, 单位 100ms(协议中为秒数x10), 4 字节/上限 9999s
	// 保存IP参数(0x0F): 4 字节新 IP
	uint8_t u8Ip[4];
	// 保存固件参数(0x17): 数据域除首字节标志外的 48 字节(12 个 float 大端),
	// 这里只做"搬运", 由 FlexControl 调 KeyParamsUnpack 解释, 通信层不依赖业务结构体
	uint8_t u8KeyParams[KEY_PARAMS_DATA_LEN];
}SerialInfo;

extern void CbUartIdleHandler(UART_HandleTypeDef *huart);

void SerialHandleInit(void);
void SendSerialRespond(const RespondType eType, void *pParams);
bool GetSerialInfo(SerialInfo *pInfo);
void SerialDataHandle(void);
// 主循环观察到 UART 新数据时调用, weak 实现由应用层覆盖, 不在中断内操作LED。
void CbSerialReceiveActivity(void);

// 网络帧解析入口: 帧布局与串口一致, 不含帧头帧尾: [cmd][len][data...][chkH][chkL]
// 校验已由网络层(Network.c)完成, 这里只做命令解析, 结果进与串口相同的
// SerialInfo 队列, 由 FlexControl 统一分发
void SerialHandleNetworkFrame(const uint8_t *pFrame, const uint8_t u8Len);

#endif
