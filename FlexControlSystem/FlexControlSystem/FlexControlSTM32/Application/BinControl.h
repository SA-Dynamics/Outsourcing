#ifndef BINCONTROL_H
#define BINCONTROL_H

#include "main.h"

// 料仓指令队列: 料仓动作少且单次时间长, 队列比振动盘(MOTION_INFO_BUFFER_SIZE=16)短
#define BIN_MOTION_INFO_BUFFER_SIZE		4

// 查表参数: 与 MotorControl 使用同一张占空比正弦表
#define BIN_TABLE_SIZE					1600
#define BIN_TABLE_SHIFT					15		// 定点: 索引以 Q15 存储, >>15 得到表内偏移
#define BIN_TABLE_FULL_INDEX			((uint32_t)BIN_TABLE_SIZE << BIN_TABLE_SHIFT)

typedef struct
{
	float fVoltage;			// 电压, 协议为 x10 发送, 解析层已还原
	float fFrequency;		// 频率
	float fTimeUse;			// 运动时长, 秒
}BinMotionParams;

// weak, 回调由 FlexControl.c 实现
void CbBinMotionFinish(void);
void CbBinMotionStopFinish(void);

void SendBinInfo(BinMotionParams *pParams);
void BinControlInit(void);
void BinControl(void);
void BinControlStop(void);

// PWM 脉冲完成回调(16kHz): 由 MotorControl.c 的 HAL_TIM_PWM_PulseFinishedCallback 转发
void BinPwmPulseCallback(TIM_HandleTypeDef *htim);

#endif
