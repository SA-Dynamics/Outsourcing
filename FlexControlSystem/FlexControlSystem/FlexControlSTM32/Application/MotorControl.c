#include "MotorControl.h"
#include "Timer.h"
#include <string.h>
#include "SerialHandle.h"


#define TRAY_MAX_VIBRATION_FREQ			200
#define TRAY_MIN_VIBRATION_FREQ			10
#define MAX_VOLTAGE_VALUE				24.0f

const uint16_t g_u16SinePositiveHalf[800] =
{
0, 18, 35, 53, 71, 88, 106, 124, 141, 159, 177,
195, 212, 230, 248, 265, 283, 301, 318, 336, 353,
371, 389, 406, 424, 442, 459, 477, 494, 512, 529,
547, 565, 582, 600, 617, 635, 652, 670, 687, 705,
722, 740, 757, 774, 792, 809, 827, 844, 861, 879,
896, 913, 931, 948, 965, 983, 1000, 1017, 1034, 1052,
1069, 1086, 1103, 1120, 1137, 1154, 1172, 1189, 1206, 1223,
1240, 1257, 1274, 1291, 1308, 1324, 1341, 1358, 1375, 1392,
1409, 1426, 1442, 1459, 1476, 1492, 1509, 1526, 1542, 1559,
1576, 1592, 1609, 1625, 1642, 1658, 1675, 1691, 1707, 1724,
1740, 1756, 1773, 1789, 1805, 1821, 1837, 1854, 1870, 1886,
1902, 1918, 1934, 1950, 1966, 1982, 1997, 2013, 2029, 2045,
2061, 2076, 2092, 2108, 2123, 2139, 2154, 2170, 2185, 2201,
2216, 2232, 2247, 2262, 2278, 2293, 2308, 2323, 2338, 2353,
2368, 2383, 2398, 2413, 2428, 2443, 2458, 2473, 2488, 2502,
2517, 2532, 2546, 2561, 2575, 2590, 2604, 2619, 2633, 2647,
2662, 2676, 2690, 2704, 2718, 2732, 2746, 2760, 2774, 2788,
2802, 2816, 2830, 2843, 2857, 2871, 2884, 2898, 2911, 2925,
2938, 2952, 2965, 2978, 2992, 3005, 3018, 3031, 3044, 3057,
3070, 3083, 3096, 3109, 3121, 3134, 3147, 3159, 3172, 3184,
3197, 3209, 3222, 3234, 3246, 3258, 3271, 3283, 3295, 3307,
3319, 3331, 3343, 3354, 3366, 3378, 3390, 3401, 3413, 3424,
3436, 3447, 3458, 3470, 3481, 3492, 3503, 3514, 3525, 3536,
3547, 3558, 3569, 3580, 3590, 3601, 3611, 3622, 3632, 3643,
3653, 3664, 3674, 3684, 3694, 3704, 3714, 3724, 3734, 3744,
3754, 3763, 3773, 3783, 3792, 3802, 3811, 3820, 3830, 3839,
3848, 3857, 3866, 3875, 3884, 3893, 3902, 3911, 3920, 3928,
3937, 3945, 3954, 3962, 3971, 3979, 3987, 3995, 4003, 4011,
4019, 4027, 4035, 4043, 4051, 4058, 4066, 4074, 4081, 4088,
4096, 4103, 4110, 4117, 4125, 4132, 4139, 4145, 4152, 4159,
4166, 4172, 4179, 4186, 4192, 4198, 4205, 4211, 4217, 4223,
4229, 4235, 4241, 4247, 4253, 4259, 4264, 4270, 4276, 4281,
4286, 4292, 4297, 4302, 4307, 4312, 4317, 4322, 4327, 4332,
4337, 4341, 4346, 4351, 4355, 4359, 4364, 4368, 4372, 4376,
4381, 4385, 4388, 4392, 4396, 4400, 4403, 4407, 4411, 4414,
4417, 4421, 4424, 4427, 4430, 4433, 4436, 4439, 4442, 4445,
4448, 4450, 4453, 4455, 4458, 4460, 4462, 4465, 4467, 4469,
4471, 4473, 4475, 4476, 4478, 4480, 4481, 4483, 4484, 4486,
4487, 4488, 4490, 4491, 4492, 4493, 4494, 4494, 4495, 4496,
4496, 4497, 4498, 4498, 4498, 4499, 4499, 4499, 4499, 4499,
4499, 4499, 4499, 4498, 4498, 4498, 4497, 4496, 4496, 4495,
4494, 4494, 4493, 4492, 4491, 4490, 4488, 4487, 4486, 4484,
4483, 4481, 4480, 4478, 4476, 4475, 4473, 4471, 4469, 4467,
4465, 4462, 4460, 4458, 4455, 4453, 4450, 4448, 4445, 4442,
4439, 4436, 4433, 4430, 4427, 4424, 4421, 4417, 4414, 4411,
4407, 4403, 4400, 4396, 4392, 4388, 4385, 4381, 4376, 4372,
4368, 4364, 4359, 4355, 4351, 4346, 4341, 4337, 4332, 4327,
4322, 4317, 4312, 4307, 4302, 4297, 4292, 4286, 4281, 4276,
4270, 4264, 4259, 4253, 4247, 4241, 4235, 4229, 4223, 4217,
4211, 4205, 4198, 4192, 4186, 4179, 4172, 4166, 4159, 4152,
4145, 4139, 4132, 4125, 4117, 4110, 4103, 4096, 4088, 4081,
4074, 4066, 4058, 4051, 4043, 4035, 4027, 4019, 4011, 4003,
3995, 3987, 3979, 3971, 3962, 3954, 3945, 3937, 3928, 3920,
3911, 3902, 3893, 3884, 3875, 3866, 3857, 3848, 3839, 3830,
3820, 3811, 3802, 3792, 3783, 3773, 3763, 3754, 3744, 3734,
3724, 3714, 3704, 3694, 3684, 3674, 3664, 3653, 3643, 3632,
3622, 3611, 3601, 3590, 3580, 3569, 3558, 3547, 3536, 3525,
3514, 3503, 3492, 3481, 3470, 3458, 3447, 3436, 3424, 3413,
3401, 3390, 3378, 3366, 3354, 3343, 3331, 3319, 3307, 3295,
3283, 3271, 3258, 3246, 3234, 3222, 3209, 3197, 3184, 3172,
3159, 3147, 3134, 3121, 3109, 3096, 3083, 3070, 3057, 3044,
3031, 3018, 3005, 2992, 2978, 2965, 2952, 2938, 2925, 2911,
2898, 2884, 2871, 2857, 2843, 2830, 2816, 2802, 2788, 2774,
2760, 2746, 2732, 2718, 2704, 2690, 2676, 2662, 2647, 2633,
2619, 2604, 2590, 2575, 2561, 2546, 2532, 2517, 2502, 2488,
2473, 2458, 2443, 2428, 2413, 2398, 2383, 2368, 2353, 2338,
2323, 2308, 2293, 2278, 2262, 2247, 2232, 2216, 2201, 2185,
2170, 2154, 2139, 2123, 2108, 2092, 2076, 2061, 2045, 2029,
2013, 1997, 1982, 1966, 1950, 1934, 1918, 1902, 1886, 1870,
1854, 1837, 1821, 1805, 1789, 1773, 1756, 1740, 1724, 1707,
1691, 1675, 1658, 1642, 1625, 1609, 1592, 1576, 1559, 1542,
1526, 1509, 1492, 1476, 1459, 1442, 1426, 1409, 1392, 1375,
1358, 1341, 1324, 1308, 1291, 1274, 1257, 1240, 1223, 1206,
1189, 1172, 1154, 1137, 1120, 1103, 1086, 1069, 1052, 1034,
1017, 1000, 983, 965, 948, 931, 913, 896, 879, 861,
844, 827, 809, 792, 774, 757, 740, 722, 705, 687,
670, 652, 635, 617, 600, 582, 565, 547, 529, 512,
494, 477, 459, 442, 424, 406, 389, 371, 353, 336,
318, 301, 283, 265, 248, 230, 212, 195, 177, 159,
141, 124, 106, 88, 71, 53, 35, 18, 0,
};






typedef enum
{
	VIB_FORM_NONE = 0,
	VIB_FORM_SINE,
}VibrationWaveForm;


typedef struct
{
	uint32_t u32TargetPulseCount;
	uint32_t u32PulseCount;
	uint16_t u16Duty;
}PolarityType;


typedef struct
{
	PolarityType sPolarityP;	// 正极
	PolarityType sPolarityN;	// 负极
	
	// 占空比输出参数, 100倍数
	uint8_t u8DutyZoom;
	uint16_t u16Span;
	uint32_t u32Index;
}MotorType;


struct
{
	MotorType sMotor[4];	// 0:左上, 1:左下, 2:右下, 3:右上
	uint32_t u32TotalPulseCount;
	uint32_t u32ConnectiveDetectTimer;
}g_sMotorControl;


struct
{
	MotorMotionParams sParams[MOTION_INFO_BUFFER_SIZE];
	uint8_t u8ReadIndex;
	uint8_t u8WriteIndex;
	uint8_t u8BufferLen;
}g_sMotionInfoBuffer;


struct
{
	uint8_t u8Index;
	uint32_t u32TimeUseExpect;
	uint32_t u32TimerCount;
	bool (*pControlFunc)(void);
}g_sMotionControl;


void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) 
{
	if (htim->Instance == TIM2) 
	{
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) 
		{
			// 查表, 缩放占空比
			__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, g_u16SinePositiveHalf[g_sMotorControl.sMotor[0].u32Index / 100] * 
								g_sMotorControl.sMotor[0].u8DutyZoom / 100); 
			g_sMotorControl.sMotor[0].u32Index += g_sMotorControl.sMotor[0].u16Span;
		} 
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
    }
	else if (htim->Instance == TIM3) 
	{
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, duty); 
		} 
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
    }	
	else if (htim->Instance == TIM4) 
	{
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, duty); 
		} 
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
		else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) 
		{
			//__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, duty); 
		}
    }	
}


void SetNormal(const bool bNormal)
{
	// 正常就更新计数器
	if (bNormal)
	{
		ResetTimerCount(&g_sMotorControl.u32ConnectiveDetectTimer);
	}
}


__weak void CbMotionFinish(const uint8_t u8Index)
{
	
}

// 将运动指令存到一个序列里
void SendMotorInfo(MotorMotionParams *pParams)
{
	if (g_sMotionInfoBuffer.u8BufferLen < MOTION_INFO_BUFFER_SIZE)
	{
		memcpy(&g_sMotionInfoBuffer.sParams[g_sMotionInfoBuffer.u8WriteIndex], pParams, sizeof(MotorMotionParams));
		g_sMotionInfoBuffer.u8WriteIndex = (g_sMotionInfoBuffer.u8WriteIndex + 1) % MOTION_INFO_BUFFER_SIZE;
		g_sMotionInfoBuffer.u8BufferLen += 1;
	}
}


// 从序列里拿出一个运动指令
bool GetMotorInfo(MotorMotionParams *pParams)
{
	bool bRet = false;
	
	if (g_sMotionInfoBuffer.u8BufferLen)
	{
		memcpy(pParams, &g_sMotionInfoBuffer.sParams[g_sMotionInfoBuffer.u8ReadIndex], sizeof(MotorMotionParams));
		g_sMotionInfoBuffer.u8ReadIndex = (g_sMotionInfoBuffer.u8ReadIndex + 1) % MOTION_INFO_BUFFER_SIZE;
		g_sMotionInfoBuffer.u8BufferLen -= 1;
		
		bRet = true;
	}

	return bRet;
}


void ClearMotorInfo(void)
{
	g_sMotionInfoBuffer.u8ReadIndex = 0;
	g_sMotionInfoBuffer.u8WriteIndex = 0;
	g_sMotionInfoBuffer.u8BufferLen = 0;
}


void MotorControlStop(void)
{
	// 停止当前运动
	
	// 将运动序列中的所有运动清除
	ClearMotorInfo();
}


static bool MoveNone(void)
{
	return true;
}


static bool MoveUpMode(void)
{
	bool bRet = false;
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		CbMotionFinish(g_sMotionControl.u8Index);
		bRet = true;
	}
	
	return bRet;
}


static bool MoveDownMode(void)
{
	bool bRet = false;
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		CbMotionFinish(g_sMotionControl.u8Index);
		bRet = true;
	}
	
	return bRet;
}



static bool MoveLeftMode(void)
{
	bool bRet = false;
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		CbMotionFinish(g_sMotionControl.u8Index);
		bRet = true;
	}
	
	return bRet;
}


static bool MoveRightMode(void)
{
	bool bRet = false;
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		CbMotionFinish(g_sMotionControl.u8Index);
		bRet = true;
	}
	
	return bRet;
}

	

static void GetMotionFunc(const MotionIndex eIndex)
{
	switch (eIndex)
	{
		case MOTION_UP:
			g_sMotionControl.pControlFunc = MoveUpMode;
			break;

		case MOTION_DOWN:
			g_sMotionControl.pControlFunc = MoveDownMode;
			break;

		case MOTION_LEFT:
			g_sMotionControl.pControlFunc = MoveLeftMode;
			break;

		case MOTION_RIGHT:
			g_sMotionControl.pControlFunc = MoveRightMode;
			break;
		
		default:
			g_sMotionControl.pControlFunc = MoveNone;
			break;
	}
}


static void PrepareMotion(const MotorMotionParams *pParams)
{
	const float fPulseCycle = 0.0000625f;				// 1周期pwm用时
	const uint16_t fPWMCount10HzHalfCycle = 800;		// 半周期振动下PWM总数
	
	
	g_sMotionControl.u32TimeUseExpect = pParams->fTimeUse * 1000;
	ResetTimerCount(&g_sMotionControl.u32TimerCount);
		
	// 计算PWM输出参数
	float fVoltage = pParams->fVoltage;
	if (fVoltage > MAX_VOLTAGE_VALUE)
	{
		fVoltage = MAX_VOLTAGE_VALUE;
	}
	g_sMotorControl.sMotor[0].u8DutyZoom = (uint16_t)(fVoltage / MAX_VOLTAGE_VALUE * 100.0f);
	
	
	// 16kHz的PWM, 10Hz振动频率时, 一个周期包含1600个PWM周期, 以此为基准
	float fFrequency = pParams->fFrequency;
	if (fFrequency > 200.0f)
	{
		fFrequency = 200.0f;
	}
	
	// 计算查表跨度
	uint16_t u16PWMCount = (uint16_t)(1.0f / fFrequency / fPulseCycle / 2.0f);
	g_sMotorControl.sMotor[0].u16Span = fPWMCount10HzHalfCycle * 100 / u16PWMCount;
	
	
	GetMotionFunc(pParams->eMotionIndex);
}


void MotionControlInit(void)
{
	g_sMotionControl.pControlFunc = MoveNone;
}


void MotorControl(void)
{
	static enum
	{
		GET_INFO = 0,
		MOTION_HANDLE,
	}eControlStep = GET_INFO;
	
	static MotorMotionParams sParams;
	
	switch (eControlStep)
	{
		case GET_INFO:
			if (GetMotorInfo(&sParams))
			{
				eControlStep = MOTION_HANDLE;
				
				// 准备好电机控制参数
				PrepareMotion(&sParams);
			}
			break;
		
		case MOTION_HANDLE:
			if (g_sMotionControl.pControlFunc())
			{
				eControlStep = GET_INFO;
			}
			break;
		
		default:
			break;
	}
}


