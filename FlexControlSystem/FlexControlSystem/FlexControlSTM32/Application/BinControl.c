#include "BinControl.h"
#include "MotorControl.h"
#include "Timer.h"
#include <string.h>
#include "tim.h"


// 与 MotorControl 共用同一张占空比正弦表(1600 点/周期, 幅值 ±4499)
extern const int16_t g_s16DutyTable1Cycle[];

#define BIN_MAX_VOLTAGE_VALUE		24.0f
#define BIN_MAX_FREQUENCY			200.0f
#define BIN_MIN_FREQUENCY			1.0f
// 16kHz PWM(TIM3: PSC=0, ARR=4499, 72MHz/4500) 下 10Hz 振动频率一个周期包含的 PWM 数
#define BIN_PWM_COUNT_10HZ_CYCLE	1600
#define BIN_PWM_PULSE_CYCLE_S		0.0000625f

// 占空比上限 = TIM3 的 ARR
#define BIN_PWM_MAX_DUTY			4499

// 料仓驱动器使能(PA8): 与 MotorControl 里的同名宏保持一致
#define BIN_ENABLE(bEnable)			(bEnable ? HAL_GPIO_WritePin(EnableBin_GPIO_Port, EnableBin_Pin, GPIO_PIN_SET) \
											  : HAL_GPIO_WritePin(EnableBin_GPIO_Port, EnableBin_Pin, GPIO_PIN_RESET))

// 极性: CH1(PA6)=P, CH2(PA7)=N, 与振动盘单个执行器的接法一致
#define BIN_CH_P					1
#define BIN_CH_N					2


// 料仓指令环形队列
static struct
{
	BinMotionParams sParams[BIN_MOTION_INFO_BUFFER_SIZE];
	uint8_t u8WriteIndex;
	uint8_t u8ReadIndex;
	uint8_t u8BufferLen;
}g_sBinInfoBuffer;


// 料仓运行参数
static struct
{
	// 查表参数: 与 MotorControl::PrepareMotion 同一套算法
	uint32_t u32DutyZoom;		// 电压换算的占空比缩放系数(Q15)
	uint32_t u32Span;			// 每个PWM周期表索引增量(Q15)
	volatile uint32_t u32Index;	// 当前表索引(Q15)

	uint32_t u32TimeUseMs;		// 本次运动时长 ms, 0 表示立即完成(与振动盘一致)
	uint32_t u32TimerCount;

	bool (*pControlFunc)(void);	// 当前运动函数

	volatile uint8_t u8ActiveCh;	// 当前激活的通道: BIN_CH_P / BIN_CH_N
	volatile bool bPwmActive;		// PWM 查表中
} g_sBinControl;


// 状态机步骤: 必须放在模块作用域, 停止指令要能把它复位
// (若像原实现那样做成 BinControl() 内的 static, 停止后仍停在 BIN_HANDLE,
//  而 pControlFunc 已被置成 BinMoveNone 并立即返回 true —— 下一次进主循环
//  就会误发一帧"运动完成" 0x08)
static enum
{
	BIN_STEP_GET_INFO = 0,
	BIN_STEP_HANDLE,
}g_eBinStep = BIN_STEP_GET_INFO;


// 将料仓运动指令存入队列
void SendBinInfo(BinMotionParams *pParams)
{
	if (g_sBinInfoBuffer.u8BufferLen < BIN_MOTION_INFO_BUFFER_SIZE)
	{
		memcpy(&g_sBinInfoBuffer.sParams[g_sBinInfoBuffer.u8WriteIndex], pParams, sizeof(BinMotionParams));
		g_sBinInfoBuffer.u8WriteIndex = (g_sBinInfoBuffer.u8WriteIndex + 1) % BIN_MOTION_INFO_BUFFER_SIZE;
		g_sBinInfoBuffer.u8BufferLen += 1;
	}
}


// 从队列取出一条料仓运动指令
static bool GetBinInfo(BinMotionParams *pParams)
{
	bool bRet = false;

	if (g_sBinInfoBuffer.u8BufferLen)
	{
		memcpy(pParams, &g_sBinInfoBuffer.sParams[g_sBinInfoBuffer.u8ReadIndex], sizeof(BinMotionParams));
		g_sBinInfoBuffer.u8ReadIndex = (g_sBinInfoBuffer.u8ReadIndex + 1) % BIN_MOTION_INFO_BUFFER_SIZE;
		g_sBinInfoBuffer.u8BufferLen -= 1;

		bRet = true;
	}

	return bRet;
}


// 清空料仓指令队列
static void ClearBinInfo(void)
{
	g_sBinInfoBuffer.u8ReadIndex = 0;
	g_sBinInfoBuffer.u8WriteIndex = 0;
	g_sBinInfoBuffer.u8BufferLen = 0;
}


__weak void CbBinMotionFinish(void)
{

}


__weak void CbBinMotionStopFinish(void)
{

}


// 通道号 -> TIM 通道
static uint32_t BinTimChannel(const uint8_t u8Ch)
{
	return (u8Ch == BIN_CH_P) ? TIM_CHANNEL_1 : TIM_CHANNEL_2;
}


// 通道号 -> 比较中断
static uint32_t BinTimIT(const uint8_t u8Ch)
{
	return (u8Ch == BIN_CH_P) ? TIM_IT_CC1 : TIM_IT_CC2;
}


// 无运动: 空函数, 仅用于停止态占位
static bool BinMoveNone(void)
{
	return true;
}


/*
 * 停 PWM。
 *
 * 禁忌: 这里不能调 HAL_TIM_PWM_Stop()/HAL_TIM_PWM_Stop_IT()。
 * 它们的结尾都会执行 __HAL_TIM_DISABLE(htim) 关掉整个 TIM3 的计数器(CEN=0),
 * 而 TIM3 的 CH3/CH4 是两路光源在用的 —— 计数器一停, 光源的 PWM 输出会被冻结
 * 在当前电平(可能是常亮), 而且再也不会自动变化。
 * 所以只关"这一路"的 CC 中断并把两路比较值清 0(PWM1 + OCPolarity=HIGH => 恒低)。
 */
static void BinPwmStop(void)
{
	g_sBinControl.bPwmActive = false;

	__HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC1);
	__HAL_TIM_DISABLE_IT(&htim3, TIM_IT_CC2);

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);
}


// 启动 PWM: 从极性 P 起振, 首个脉冲的占空比由中断里填
//
// 这里刻意不用 HAL_TIM_PWM_Start_IT():
//   它会把通道状态记成 BUSY, 而我们又不能用 HAL_TIM_PWM_Stop_IT 复位状态
//   (会关掉整个 TIM3 计数器, 把光源拖死), 于是第二次启动时 HAL 会因
//   "channel state == BUSY" 直接返回 HAL_ERROR, CC1IE 根本不会打开 ——
//   现象就是第一次能振、之后怎么点都不动, 而且没有任何报错。
static void BinPwmStart(void)
{
	BIN_ENABLE(1);

	g_sBinControl.u32Index = 0;
	g_sBinControl.u8ActiveCh = BIN_CH_P;

	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, 0);

	// 清掉可能残留的比较标志, 再开通道和中断
	__HAL_TIM_CLEAR_IT(&htim3, TIM_IT_CC1);
	__HAL_TIM_CLEAR_IT(&htim3, TIM_IT_CC2);
	TIM_CCxChannelCmd(htim3.Instance, TIM_CHANNEL_1, TIM_CCx_ENABLE);
	__HAL_TIM_ENABLE_IT(&htim3, TIM_IT_CC1);

	g_sBinControl.bPwmActive = true;

	// 只置 CEN, 不影响其它通道; 计数器通常已被光源那两路启起来了
	__HAL_TIM_ENABLE(&htim3);
}


// 换向: 只切"比较通道", 不碰计数器和另一路的通道使能
static void BinSwitchChannel(const uint8_t u8Ch)
{
	if (u8Ch == g_sBinControl.u8ActiveCh)
	{
		return;
	}

	// 关旧通道的 CC 中断(不能停通道本身, 更不能动计数器), 占空比清 0
	__HAL_TIM_DISABLE_IT(&htim3, BinTimIT(g_sBinControl.u8ActiveCh));
	__HAL_TIM_SET_COMPARE(&htim3, BinTimChannel(g_sBinControl.u8ActiveCh), 0);

	g_sBinControl.u8ActiveCh = u8Ch;

	// 开新通道: 补上 CCxE 和 CCxIE; 清掉可能残留的 CC 标志, 避免立刻误触发
	__HAL_TIM_CLEAR_IT(&htim3, BinTimIT(u8Ch));
	TIM_CCxChannelCmd(htim3.Instance, BinTimChannel(u8Ch), TIM_CCx_ENABLE);
	__HAL_TIM_ENABLE_IT(&htim3, BinTimIT(u8Ch));
}


/*
 * PWM 中断服务: 每个 PWM 周期(TIM3 比较匹配)走一步表, 频率 16kHz。
 * 由 MotorControl.c 的 HAL_TIM_PWM_PulseFinishedCallback 统一转发进来
 * (HAL 的脉冲完成回调只能有一处定义, 那里是现有的总入口)。
 */
void BinPwmPulseCallback(TIM_HandleTypeDef *htim)
{
	int32_t s32TableValue;
	int32_t s32Duty;
	uint8_t u8Ch;

	if (htim->Instance != TIM3 || (!g_sBinControl.bPwmActive))
	{
		return;
	}

	// 一个振动周期走完, 回表头继续振(料仓要保持振动直到时长到)
	if ((g_sBinControl.u32Index >> BIN_TABLE_SHIFT) >= BIN_TABLE_SIZE)
	{
		g_sBinControl.u32Index = 0;
	}

	// 查表 + 按电压缩放(Q15)
	s32TableValue = ((int32_t)g_s16DutyTable1Cycle[g_sBinControl.u32Index >> BIN_TABLE_SHIFT]
					* (int32_t)g_sBinControl.u32DutyZoom) >> BIN_TABLE_SHIFT;

	// 正负半周分别走 P/N 两个通道, 幅值即占空比
	if (s32TableValue >= 0)
	{
		s32Duty = s32TableValue;
		u8Ch = BIN_CH_P;
	}
	else
	{
		s32Duty = -s32TableValue;
		u8Ch = BIN_CH_N;
	}

	if (s32Duty > BIN_PWM_MAX_DUTY)
	{
		s32Duty = BIN_PWM_MAX_DUTY;
	}

	BinSwitchChannel(u8Ch);

	__HAL_TIM_SET_COMPARE(&htim3, BinTimChannel(u8Ch), (uint32_t)s32Duty);

	g_sBinControl.u32Index += g_sBinControl.u32Span;
}


// 料仓振动: 查表由 PWM 中断推进, 这里只管时长
static bool BinRun(void)
{
	// 时长 0 视为立即完成(与振动盘语义一致, 上位机据此不会超时)
	if (g_sBinControl.u32TimeUseMs == 0)
	{
		BinPwmStop();
		return true;
	}

	if (GetTimerTickDelta(g_sBinControl.u32TimerCount, GetCurTimerCount()) >= g_sBinControl.u32TimeUseMs)
	{
		BinPwmStop();
		return true;
	}

	return false;
}


// 运动准备: 与 MotorControl::PrepareMotion 同一套算法计算查表参数
static void PrepareBinMotion(const BinMotionParams *pParams)
{
	uint32_t u32PWMCount;
	float fVoltage = pParams->fVoltage;
	float fFrequency = pParams->fFrequency;

	// 运动耗时 ms
	g_sBinControl.u32TimeUseMs = (uint32_t)(pParams->fTimeUse * 1000.0f);
	ResetTimerCount(&g_sBinControl.u32TimerCount);

	// 电压 -> 占空比缩放系数(Q15), 表幅值已经是 4499(=ARR), 满电压即满占空比
	if (fVoltage > BIN_MAX_VOLTAGE_VALUE)
	{
		fVoltage = BIN_MAX_VOLTAGE_VALUE;
	}
	if (fVoltage < 0.0f)
	{
		fVoltage = 0.0f;
	}
	g_sBinControl.u32DutyZoom = (uint32_t)(fVoltage / BIN_MAX_VOLTAGE_VALUE * 32768.0f);

	// 频率 -> 表索引跨度: 一个振动周期的 PWM 数越少, 每个 PWM 走表越多
	if (fFrequency > BIN_MAX_FREQUENCY)
	{
		fFrequency = BIN_MAX_FREQUENCY;
	}
	if (fFrequency < BIN_MIN_FREQUENCY)
	{
		fFrequency = BIN_MIN_FREQUENCY;		// 防止除零/表索引步长溢出
	}

	u32PWMCount = (uint32_t)(1.0f / fFrequency / BIN_PWM_PULSE_CYCLE_S);
	if (u32PWMCount == 0)
	{
		u32PWMCount = 1;
	}
	g_sBinControl.u32Span = (uint32_t)(BIN_PWM_COUNT_10HZ_CYCLE * 32768.0f / (float)u32PWMCount);

	g_sBinControl.u32Index = 0;
	g_sBinControl.pControlFunc = BinRun;

	// 时长 0: 不下发 PWM, 免得打出一个无意义的脉冲, 由 BinRun 立刻判完成
	if (g_sBinControl.u32TimeUseMs > 0)
	{
		BinPwmStart();
	}
}


void BinControlInit(void)
{
	g_sBinControl.pControlFunc = BinMoveNone;
	g_sBinControl.bPwmActive = false;
	g_eBinStep = BIN_STEP_GET_INFO;

	BinPwmStop();
	ClearBinInfo();
}


void BinControlStop(void)
{
	// 停止当前运动并停 PWM
	g_sBinControl.pControlFunc = BinMoveNone;
	g_sBinControl.bPwmActive = false;
	g_eBinStep = BIN_STEP_GET_INFO;		// 复位状态机, 否则下一轮会误发 0x08
	BinPwmStop();

	// 清空指令队列
	ClearBinInfo();

	// 通知上位机已停止(0x0A)
	CbBinMotionStopFinish();
}


void BinControl(void)
{
	static BinMotionParams sParams;

	switch (g_eBinStep)
	{
		case BIN_STEP_GET_INFO:
			if (GetBinInfo(&sParams))
			{
				// 准备查表参数并进入运动处理(参数齐了就起振)
				PrepareBinMotion(&sParams);
				g_eBinStep = BIN_STEP_HANDLE;
			}
			break;

		case BIN_STEP_HANDLE:
			if (g_sBinControl.pControlFunc())
			{
				// 运动到时/立即完成: PWM 已在 BinRun 里停掉
				g_sBinControl.pControlFunc = BinMoveNone;
				g_eBinStep = BIN_STEP_GET_INFO;

				// 通知上位机运动完成(0x08)
				CbBinMotionFinish();
			}
			break;

		default:
			g_eBinStep = BIN_STEP_GET_INFO;
			break;
	}
}
