#include "MotorControl.h"
#include "Timer.h"
#include <string.h>
#include "SerialHandle.h"


#define TRAY_MAX_VIBRATION_FREQ			200
#define TRAY_MIN_VIBRATION_FREQ			10
#define MAX_VOLTAGE_VALUE				24

const uint16_t g_u16SinePositiveHalf[801] =
{
0, 18, 35, 53, 71, 88, 106, 124, 141, 159, 177,
194, 212, 230, 247, 265, 282, 300, 318, 335, 353,
371, 388, 406, 423, 441, 459, 476, 494, 511, 529,
546, 564, 581, 599, 616, 634, 651, 669, 686, 704,
721, 739, 756, 774, 791, 808, 826, 843, 860, 878,
895, 912, 930, 947, 964, 981, 999, 1016, 1033, 1050,
1067, 1085, 1102, 1119, 1136, 1153, 1170, 1187, 1204, 1221,
1238, 1255, 1272, 1289, 1306, 1323, 1340, 1357, 1373, 1390,
1407, 1424, 1441, 1457, 1474, 1491, 1507, 1524, 1541, 1557,
1574, 1590, 1607, 1623, 1640, 1656, 1673, 1689, 1705, 1722,
1738, 1754, 1771, 1787, 1803, 1819, 1835, 1851, 1867, 1884,
1900, 1916, 1932, 1947, 1963, 1979, 1995, 2011, 2027, 2043,
2058, 2074, 2090, 2105, 2121, 2136, 2152, 2167, 2183, 2198,
2214, 2229, 2244, 2260, 2275, 2290, 2305, 2321, 2336, 2351,
2366, 2381, 2396, 2411, 2426, 2440, 2455, 2470, 2485, 2500,
2514, 2529, 2543, 2558, 2572, 2587, 2601, 2616, 2630, 2644,
2659, 2673, 2687, 2701, 2715, 2729, 2743, 2757, 2771, 2785,
2799, 2813, 2827, 2840, 2854, 2868, 2881, 2895, 2908, 2922,
2935, 2949, 2962, 2975, 2988, 3002, 3015, 3028, 3041, 3054,
3067, 3080, 3093, 3105, 3118, 3131, 3144, 3156, 3169, 3181,
3194, 3206, 3219, 3231, 3243, 3255, 3268, 3280, 3292, 3304,
3316, 3328, 3339, 3351, 3363, 3375, 3386, 3398, 3410, 3421,
3433, 3444, 3455, 3467, 3478, 3489, 3500, 3511, 3522, 3533,
3544, 3555, 3566, 3576, 3587, 3598, 3608, 3619, 3629, 3640,
3650, 3660, 3671, 3681, 3691, 3701, 3711, 3721, 3731, 3741,
3751, 3760, 3770, 3780, 3789, 3799, 3808, 3817, 3827, 3836,
3845, 3854, 3863, 3872, 3881, 3890, 3899, 3908, 3917, 3925,
3934, 3943, 3951, 3959, 3968, 3976, 3984, 3992, 4001, 4009,
4017, 4025, 4032, 4040, 4048, 4056, 4063, 4071, 4078, 4086,
4093, 4100, 4108, 4115, 4122, 4129, 4136, 4143, 4150, 4157,
4163, 4170, 4177, 4183, 4190, 4196, 4202, 4209, 4215, 4221,
4227, 4233, 4239, 4245, 4251, 4256, 4262, 4268, 4273, 4279,
4284, 4290, 4295, 4300, 4305, 4310, 4315, 4320, 4325, 4330,
4335, 4340, 4344, 4349, 4353, 4358, 4362, 4366, 4371, 4375,
4379, 4383, 4387, 4391, 4394, 4398, 4402, 4406, 4409, 4413,
4416, 4419, 4423, 4426, 4429, 4432, 4435, 4438, 4441, 4444,
4446, 4449, 4452, 4454, 4457, 4459, 4461, 4464, 4466, 4468,
4470, 4472, 4474, 4476, 4477, 4479, 4481, 4482, 4484, 4485,
4486, 4488, 4489, 4490, 4491, 4492, 4493, 4494, 4495, 4496,
4496, 4497, 4497, 4498, 4498, 4498, 4499, 4499, 4499, 4499,
4499, 4499, 4499, 4498, 4498, 4498, 4497, 4497, 4496, 4496,
4495, 4494, 4493, 4492, 4491, 4490, 4489, 4488, 4486, 4485,
4484, 4482, 4481, 4479, 4477, 4476, 4474, 4472, 4470, 4468,
4466, 4464, 4461, 4459, 4457, 4454, 4452, 4449, 4446, 4444,
4441, 4438, 4435, 4432, 4429, 4426, 4423, 4419, 4416, 4413,
4409, 4406, 4402, 4398, 4394, 4391, 4387, 4383, 4379, 4375,
4371, 4366, 4362, 4358, 4353, 4349, 4344, 4340, 4335, 4330,
4325, 4320, 4315, 4310, 4305, 4300, 4295, 4290, 4284, 4279,
4273, 4268, 4262, 4256, 4251, 4245, 4239, 4233, 4227, 4221,
4215, 4209, 4202, 4196, 4190, 4183, 4177, 4170, 4163, 4157,
4150, 4143, 4136, 4129, 4122, 4115, 4108, 4100, 4093, 4086,
4078, 4071, 4063, 4056, 4048, 4040, 4032, 4025, 4017, 4009,
4001, 3992, 3984, 3976, 3968, 3959, 3951, 3943, 3934, 3925,
3917, 3908, 3899, 3890, 3881, 3872, 3863, 3854, 3845, 3836,
3827, 3817, 3808, 3799, 3789, 3780, 3770, 3760, 3751, 3741,
3731, 3721, 3711, 3701, 3691, 3681, 3671, 3660, 3650, 3640,
3629, 3619, 3608, 3598, 3587, 3576, 3566, 3555, 3544, 3533,
3522, 3511, 3500, 3489, 3478, 3467, 3455, 3444, 3433, 3421,
3410, 3398, 3386, 3375, 3363, 3351, 3339, 3328, 3316, 3304,
3292, 3280, 3268, 3255, 3243, 3231, 3219, 3206, 3194, 3181,
3169, 3156, 3144, 3131, 3118, 3105, 3093, 3080, 3067, 3054,
3041, 3028, 3015, 3002, 2988, 2975, 2962, 2949, 2935, 2922,
2908, 2895, 2881, 2868, 2854, 2840, 2827, 2813, 2799, 2785,
2771, 2757, 2743, 2729, 2715, 2701, 2687, 2673, 2659, 2644,
2630, 2616, 2601, 2587, 2572, 2558, 2543, 2529, 2514, 2500,
2485, 2470, 2455, 2440, 2426, 2411, 2396, 2381, 2366, 2351,
2336, 2321, 2305, 2290, 2275, 2260, 2244, 2229, 2214, 2198,
2183, 2167, 2152, 2136, 2121, 2105, 2090, 2074, 2058, 2043,
2027, 2011, 1995, 1979, 1963, 1947, 1932, 1916, 1900, 1884,
1867, 1851, 1835, 1819, 1803, 1787, 1771, 1754, 1738, 1722,
1705, 1689, 1673, 1656, 1640, 1623, 1607, 1590, 1574, 1557,
1541, 1524, 1507, 1491, 1474, 1457, 1441, 1424, 1407, 1390,
1373, 1357, 1340, 1323, 1306, 1289, 1272, 1255, 1238, 1221,
1204, 1187, 1170, 1153, 1136, 1119, 1102, 1085, 1067, 1050,
1033, 1016, 999, 981, 964, 947, 930, 912, 895, 878,
860, 843, 826, 808, 791, 774, 756, 739, 721, 704,
686, 669, 651, 634, 616, 599, 581, 564, 546, 529,
511, 494, 476, 459, 441, 423, 406, 388, 371, 353,
335, 318, 300, 282, 265, 247, 230, 212, 194, 177,
159, 141, 124, 106, 88, 71, 53, 35, 18, 0,
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
}g_sMotionControl;


void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) 
{
	if (htim->Instance == TIM2) 
	{
		if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) 
		{
			__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, g_sMotorControl.sMotor[0].sPolarityP.u16Duty); 
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
				g_sMotionControl.u8Index = sParams.eMotionIndex;
				g_sMotionControl.u32TimeUseExpect = sParams.fTimeUse * 1000;
				ResetTimerCount(&g_sMotionControl.u32TimerCount);
			}
			break;
		
		case MOTION_HANDLE:
			if (MoveUpMode())
			{
				eControlStep = GET_INFO;
			}
			break;
		
		default:
			break;
	}
}


