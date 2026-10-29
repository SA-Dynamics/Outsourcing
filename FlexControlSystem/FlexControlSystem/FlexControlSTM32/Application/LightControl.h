#ifndef LIGHTCONTROL_H
#define LIGHTCONTROL_H

#include "main.h"

// 光源指令队列: 开启指令入队, 关闭指令即时执行
// 两路光源各有独立状态, 队列只需能容纳"排队中的开启指令", 取小值
#define LIGHT_CMD_BUFFER_SIZE			4

// 光源索引(与协议一致): 1=光源A, 2=光源B, 0=全部(仅关闭指令使用)
#define LIGHT_INDEX_A					1
#define LIGHT_INDEX_B					2
#define LIGHT_INDEX_ALL					0

// 亮度上限(协议: 0~100)
#define LIGHT_BRIGHTNESS_MAX			100

typedef struct
{
	uint8_t u8Index;			// 光源索引: 1=A光源, 2=B光源
	uint8_t u8Brightness;		// 亮度 0~100
	uint16_t u16KeepTime;		// 保持时间, 单位 100ms(协议中为秒数x10), 0 表示常亮
}LightParams;

// weak, 回调由 FlexControl.c 实现
void CbLightOnFinish(const uint8_t u8Index);
void CbLightOffFinish(const uint8_t u8Index);

void SendLightInfo(LightParams *pParams);
void LightControlInit(void);
void LightControl(void);

// 关闭光源: u8Index = 1/2 关闭指定路, LIGHT_INDEX_ALL 关闭全部
void LightControlStop(const uint8_t u8Index);

#endif
