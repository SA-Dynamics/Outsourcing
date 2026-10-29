#include "LightControl.h"
#include "Timer.h"
#include <string.h>
#include "tim.h"

/*
 * 硬件映射(CubeMX 已配置 TIM3: PSC=0, ARR=4499, CH1~CH4 全为 PWM 输出)
 *   光源A(LIGHT1) -> TIM3_CH3 -> PB0
 *   光源B(LIGHT2) -> TIM3_CH4 -> PB1
 *   料仓(CH1/CH2, PA6/PA7)与光源共用 TIM3, 各自占独立比较通道, 互不影响。
 *
 * 亮度 0~100 线性折算为比较值: CCR = 亮度 x (ARR+1) / 100 = 亮度 x 45
 *   0   -> CCR=0    -> 恒低, 熄灭
 *   100 -> CCR=4500 -> 大于 ARR(4499), 恒高, 满占空比
 */

#define LIGHT_CHANNEL_NUM			2

// TIM3 一个 PWM 周期的计数值 = ARR + 1
#define LIGHT_PWM_PERIOD_TICKS		(4499U + 1U)

// 索引 1/2 -> 比较通道
static const uint32_t g_au32LightChannel[LIGHT_CHANNEL_NUM] =
{
	TIM_CHANNEL_3,		// 光源A
	TIM_CHANNEL_4,		// 光源B
};


// 每一路光源的独立状态: 独立开关, 独立计时
typedef struct
{
	bool bOn;					// 该路是否点亮中
	uint8_t u8Brightness;		// 当前亮度 0~100
	uint16_t u16KeepTime;		// 保持时间, 单位 100ms, 0 表示常亮
	uint32_t u32TimerCount;		// 保持时间计时起点
}LightChannelState;

static LightChannelState g_asLightChannel[LIGHT_CHANNEL_NUM];

static struct
{
	LightParams sParams[LIGHT_CMD_BUFFER_SIZE];
	uint8_t u8WriteIndex;
	uint8_t u8ReadIndex;
	uint8_t u8BufferLen;
} g_sLightInfoBuffer;


// 将光源开启指令存入队列
void SendLightInfo(LightParams *pParams)
{
	if (g_sLightInfoBuffer.u8BufferLen < LIGHT_CMD_BUFFER_SIZE)
	{
		memcpy(&g_sLightInfoBuffer.sParams[g_sLightInfoBuffer.u8WriteIndex], pParams, sizeof(LightParams));
		g_sLightInfoBuffer.u8WriteIndex = (g_sLightInfoBuffer.u8WriteIndex + 1) % LIGHT_CMD_BUFFER_SIZE;
		g_sLightInfoBuffer.u8BufferLen += 1;
	}
}


// 从队列取出一条光源开启指令
static bool GetLightInfo(LightParams *pParams)
{
	bool bRet = false;

	if (g_sLightInfoBuffer.u8BufferLen)
	{
		memcpy(pParams, &g_sLightInfoBuffer.sParams[g_sLightInfoBuffer.u8ReadIndex], sizeof(LightParams));
		g_sLightInfoBuffer.u8ReadIndex = (g_sLightInfoBuffer.u8ReadIndex + 1) % LIGHT_CMD_BUFFER_SIZE;
		g_sLightInfoBuffer.u8BufferLen -= 1;

		bRet = true;
	}

	return bRet;
}


// 清空光源开启指令队列
static void ClearLightInfo(void)
{
	g_sLightInfoBuffer.u8ReadIndex = 0;
	g_sLightInfoBuffer.u8WriteIndex = 0;
	g_sLightInfoBuffer.u8BufferLen = 0;
}


// 丢弃队列中指定光源的开启指令(关闭指令要即时生效, 不能让排队中的开启指令再把它点亮)
static void RemoveLightInfo(const uint8_t u8Index)
{
	LightParams sKeep[LIGHT_CMD_BUFFER_SIZE];
	uint8_t u8KeepLen = 0;
	uint8_t i;

	for (i = 0; i < g_sLightInfoBuffer.u8BufferLen; i++)
	{
		LightParams *pItem = &g_sLightInfoBuffer.sParams[(g_sLightInfoBuffer.u8ReadIndex + i) % LIGHT_CMD_BUFFER_SIZE];

		if (u8Index == LIGHT_INDEX_ALL || pItem->u8Index == u8Index)
		{
			continue;		// 丢弃(关闭全部时丢弃所有排队指令)
		}

		memcpy(&sKeep[u8KeepLen], pItem, sizeof(LightParams));
		u8KeepLen++;
	}

	memcpy(g_sLightInfoBuffer.sParams, sKeep, sizeof(LightParams) * u8KeepLen);
	g_sLightInfoBuffer.u8ReadIndex = 0;
	g_sLightInfoBuffer.u8WriteIndex = u8KeepLen % LIGHT_CMD_BUFFER_SIZE;
	g_sLightInfoBuffer.u8BufferLen = u8KeepLen;
}


// 索引(1/2) -> 通道下标(0/1), 非法索引返回 -1
static int8_t LightSlotOf(const uint8_t u8Index)
{
	if (u8Index == LIGHT_INDEX_A)
	{
		return 0;
	}

	if (u8Index == LIGHT_INDEX_B)
	{
		return 1;
	}

	return -1;
}


// 设置某路亮度: 只改比较值, 不动计数器
// 注意: 这里不能用 HAL_TIM_PWM_Stop —— 它会关掉整个 TIM3 的计数器,
//       而 TIM3 的 CH1/CH2 是料仓在用的, 会把料仓的 PWM 一起停掉。
static void LightSetDuty(const uint8_t u8Slot, const uint8_t u8Brightness)
{
	uint32_t u32Bright = u8Brightness;

	if (u32Bright > LIGHT_BRIGHTNESS_MAX)
	{
		u32Bright = LIGHT_BRIGHTNESS_MAX;
	}

	__HAL_TIM_SET_COMPARE(&htim3, g_au32LightChannel[u8Slot],
						  u32Bright * LIGHT_PWM_PERIOD_TICKS / LIGHT_BRIGHTNESS_MAX);
}


// 点亮某一路: 设置亮度, 并从此刻开始独立计时
static void LightTurnOn(const uint8_t u8Slot, const uint8_t u8Brightness, const uint16_t u16KeepTime)
{
	LightChannelState *pChannel = &g_asLightChannel[u8Slot];

	pChannel->u8Brightness = (u8Brightness > LIGHT_BRIGHTNESS_MAX) ? LIGHT_BRIGHTNESS_MAX : u8Brightness;
	pChannel->u16KeepTime = u16KeepTime;
	pChannel->bOn = true;

	LightSetDuty(u8Slot, pChannel->u8Brightness);

	// 每路独立的计时起点, 互不干扰
	ResetTimerCount(&pChannel->u32TimerCount);
}


// 熄灭某一路: 只把该路比较值清 0(输出恒低), 另一路不受任何影响
static void LightTurnOff(const uint8_t u8Slot)
{
	g_asLightChannel[u8Slot].bOn = false;

	LightSetDuty(u8Slot, 0);
}


__weak void CbLightOnFinish(const uint8_t u8Index)
{
	(void)u8Index;
}


__weak void CbLightOffFinish(const uint8_t u8Index)
{
	(void)u8Index;
}


void LightControlInit(void)
{
	uint8_t u8Slot;

	for (u8Slot = 0; u8Slot < LIGHT_CHANNEL_NUM; u8Slot++)
	{
		g_asLightChannel[u8Slot].bOn = false;
		g_asLightChannel[u8Slot].u8Brightness = 0;
		g_asLightChannel[u8Slot].u16KeepTime = 0;
		g_asLightChannel[u8Slot].u32TimerCount = 0;

		// 两路都从"熄灭"开始(CCR=0)
		LightSetDuty(u8Slot, 0);

		// 只启动一次通道, 之后开关灯全部靠改 CCR, 不反复启停
		(void)HAL_TIM_PWM_Start(&htim3, g_au32LightChannel[u8Slot]);
	}

	ClearLightInfo();
}


// 关闭光源: u8Index = 1/2 只关指定路; LIGHT_INDEX_ALL 关全部
void LightControlStop(const uint8_t u8Index)
{
	uint8_t u8Slot;

	if (u8Index == LIGHT_INDEX_ALL)
	{
		for (u8Slot = 0; u8Slot < LIGHT_CHANNEL_NUM; u8Slot++)
		{
			if (g_asLightChannel[u8Slot].bOn)
			{
				LightTurnOff(u8Slot);
			}
		}
	}
	else
	{
		int8_t s8Slot = LightSlotOf(u8Index);

		if (s8Slot >= 0)
		{
			if (g_asLightChannel[s8Slot].bOn)
			{
				LightTurnOff((uint8_t)s8Slot);
			}
		}
	}

	// 丢掉排队中的开启指令, 否则关灯后紧接着又被点亮
	RemoveLightInfo(u8Index);

	// 应答 0x0E 无数据域, 关哪一路都是同一帧
	CbLightOffFinish(u8Index);
}


// 每路独立检查保持时间, 到点自动熄灭
static void LightUpdateKeepTime(void)
{
	uint8_t u8Slot;

	for (u8Slot = 0; u8Slot < LIGHT_CHANNEL_NUM; u8Slot++)
	{
		LightChannelState *pChannel = &g_asLightChannel[u8Slot];

		if (!pChannel->bOn)
		{
			continue;
		}

		// 保持时间单位 100ms; 0 表示常亮, 不自动熄灭
		if (pChannel->u16KeepTime == 0)
		{
			continue;
		}

		if (GetTimerTickDelta(pChannel->u32TimerCount, GetCurTimerCount()) >=
			(uint32_t)pChannel->u16KeepTime * 100)
		{
			LightTurnOff(u8Slot);

			CbLightOffFinish((uint8_t)(u8Slot + 1));
		}
	}
}


void LightControl(void)
{
	LightParams sParams;

	// 排空队列: 一次主循环内处理完所有待开启指令
	while (GetLightInfo(&sParams))
	{
		int8_t s8Slot = LightSlotOf(sParams.u8Index);

		if (s8Slot >= 0)
		{
			LightTurnOn((uint8_t)s8Slot, sParams.u8Brightness, sParams.u16KeepTime);
		}
		// 索引非法时不动硬件, 但仍反馈, 避免上位机 1s 超时

		// 上位机要求开启后 1s 内收到反馈: 亮度已写入 CCR 再回, 反馈即代表灯已亮
		CbLightOnFinish(sParams.u8Index);
	}

	LightUpdateKeepTime();
}
