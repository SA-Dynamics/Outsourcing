/**
 * @file    PowerMonitor.c
 * @brief   电源电压监测: ADC1_IN10(PC0) 非阻塞采样实现
 * @note    状态机:
 *            IDLE   --到采样间隔--> START(启动转换, 立即返回)
 *            START  --EOC 置位--> READ(读值入滤波窗口, 回 IDLE)
 *          HAL_ADC_PollForConversion 带 0 超时等价于查标志, 但它内部有
 *          锁与状态迁移, 这里直接读 SR 的 EOC 位, 不动句柄状态, 主循环
 *          任何时刻调用都不会卡住。ADC 中断(ADC1_2_IRQHandler)已由
 *          CubeMX 使能但本模块不使用, 留给以后扩展(如看门狗/注入通道)。
 */
#include "PowerMonitor.h"
#include "adc.h"
#include "Timer.h"

// 采样状态
typedef enum
{
	PM_STEP_IDLE = 0,		// 等待下一次采样间隔
	PM_STEP_WAIT_EOC,		// 转换进行中, 轮询 EOC
}PowerMonitorStep;

static struct
{
	PowerMonitorStep eStep;
	uint32_t u32IntervalTimer;						// 采样间隔计时
	uint16_t au16FilterBuf[POWER_MONITOR_FILTER_DEPTH];	// 均值窗口
	uint8_t  u8FilterIndex;							// 窗口写入位置
	uint8_t  u8FilterCount;							// 已填充个数(填满前平均值偏小)
	bool bSampledOnce;								// 是否已有完整窗口
} g_sPowerMonitor;

// 均值滤波: 窗口求和右移(log2(DEPTH)), 填满前按实际个数平均
static uint16_t PowerMonitorFilterAverage(void)
{
	uint32_t u32Sum = 0;
	uint8_t i;

	for (i = 0; i < g_sPowerMonitor.u8FilterCount; i++)
	{
		u32Sum += g_sPowerMonitor.au16FilterBuf[i];
	}

	if (g_sPowerMonitor.u8FilterCount == 0u)
	{
		return 0u;
	}

	return (uint16_t)(u32Sum / g_sPowerMonitor.u8FilterCount);
}

volatile HAL_StatusTypeDef g_ePmCalibrationStatus = HAL_ERROR;	// 调试观察: HAL_OK=校准成功

void PowerMonitorInit(void)
{
	uint8_t i;

	g_sPowerMonitor.eStep = PM_STEP_IDLE;
	g_sPowerMonitor.u8FilterIndex = 0u;
	g_sPowerMonitor.u8FilterCount = 0u;
	g_sPowerMonitor.bSampledOnce = false;
	ResetTimerCount(&g_sPowerMonitor.u32IntervalTimer);

	for (i = 0; i < POWER_MONITOR_FILTER_DEPTH; i++)
	{
		g_sPowerMonitor.au16FilterBuf[i] = 0u;
	}

	// 校准提高精度; F1 的 HAL_ADCEx_Calibration_Start 内部会自行 Enable ADC,
	// 启动后立即返回, 校准期间的等待在函数内部完成(阻塞极短, 仅初始化时一次)
	g_ePmCalibrationStatus = HAL_ADCEx_Calibration_Start(&hadc1);
}

void PowerMonitorHandle(void)
{
	switch (g_sPowerMonitor.eStep)
	{
		case PM_STEP_IDLE:
			// 到间隔才启动转换: 单次模式软件启动, 启动后立即返回
			if (GetTimerTickDelta(g_sPowerMonitor.u32IntervalTimer, GetCurTimerCount())
					>= POWER_MONITOR_INTERVAL_MS)
			{
				ResetTimerCount(&g_sPowerMonitor.u32IntervalTimer);

				if (HAL_ADC_Start(&hadc1) == HAL_OK)
				{
					g_sPowerMonitor.eStep = PM_STEP_WAIT_EOC;
				}
				// 启动失败(句柄忙等): 停在 IDLE, 下个间隔重试, 不阻塞
			}
			break;

		case PM_STEP_WAIT_EOC:
			// 直接查 EOC 标志, 不用 HAL_ADC_PollForConversion(它操作句柄状态)
			if ((hadc1.Instance->SR & ADC_FLAG_EOC) != 0u)
			{
				const uint16_t u16Raw = (uint16_t)hadc1.Instance->DR;	// 读 DR 自动清 EOC

				g_sPowerMonitor.au16FilterBuf[g_sPowerMonitor.u8FilterIndex] = u16Raw;
				g_sPowerMonitor.u8FilterIndex = (uint8_t)((g_sPowerMonitor.u8FilterIndex + 1u)
												% POWER_MONITOR_FILTER_DEPTH);
				if (g_sPowerMonitor.u8FilterCount < POWER_MONITOR_FILTER_DEPTH)
				{
					g_sPowerMonitor.u8FilterCount++;
				}
				else
				{
					g_sPowerMonitor.bSampledOnce = true;
				}

				g_sPowerMonitor.eStep = PM_STEP_IDLE;
			}
			// EOC 未置位: 转换仍在进行(1.5 周期采样 + 12.5 周期转换,
			// 12MHz ADCCLK 下约 1.17us), 下轮主循环再查, 不等待
			break;

		default:
			g_sPowerMonitor.eStep = PM_STEP_IDLE;
			break;
	}
}

bool PowerMonitorGetVoltage(float *pfVoltage)
{
	if (pfVoltage == NULL)
	{
		return false;
	}

	// 窗口未填满时也可以读, 只是均值样本少; 调用方按需决定是否等待
	if (g_sPowerMonitor.u8FilterCount == 0u)
	{
		return false;
	}

	const uint16_t u16Adc = PowerMonitorFilterAverage();

	// 原始值 -> 电压: ADC = Vpin/VREF * 4095, 电源 = Vpin * 分压比
	*pfVoltage = (float)u16Adc / 4095.0f * POWER_MONITOR_VREF_V
				* POWER_MONITOR_DIVIDER_RATIO;

	return g_sPowerMonitor.bSampledOnce;
}
