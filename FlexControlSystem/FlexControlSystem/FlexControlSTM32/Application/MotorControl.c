#include "MotorControl.h"
#include "Timer.h"
#include <string.h>
#include "SerialHandle.h"
#include "tim.h"


#define TRAY_MAX_VIBRATION_FREQ			200
#define TRAY_MIN_VIBRATION_FREQ			10
#define MAX_VOLTAGE_VALUE				24.0f

#define ENABLE_LEFT_TOP(bEnable)	(bEnable ? HAL_GPIO_WritePin(EnableLeftTop_GPIO_Port, EnableLeftTop_Pin, GPIO_PIN_SET) : HAL_GPIO_WritePin(EnableLeftTop_GPIO_Port, EnableLeftTop_Pin, GPIO_PIN_RESET))
#define ENABLE_RIGHT_TOP(bEnable)	(bEnable ? HAL_GPIO_WritePin(EnableRightTop_GPIO_Port, EnableRightTop_Pin, GPIO_PIN_SET) : HAL_GPIO_WritePin(EnableRightTop_GPIO_Port, EnableRightTop_Pin, GPIO_PIN_RESET))
#define ENABLE_LEFT_BOTTUM(bEnable)	(bEnable ? HAL_GPIO_WritePin(EnableLeftBottum_GPIO_Port, EnableLeftBottum_Pin, GPIO_PIN_SET) : HAL_GPIO_WritePin(EnableLeftBottum_GPIO_Port, EnableLeftBottum_Pin, GPIO_PIN_RESET))
#define ENABLE_RIGHT_BOTTUM(bEnable)	(bEnable ? HAL_GPIO_WritePin(EnableRightBottum_GPIO_Port, EnableRightBottum_Pin, GPIO_PIN_SET) : HAL_GPIO_WritePin(EnableRightBottum_GPIO_Port, EnableRightBottum_Pin, GPIO_PIN_RESET))

//#define USE_SIN_WAVE
#define USE_FIX_INC

#if defined(USE_FIX_INC)
const int16_t g_s16PositiveHalf[800] = 
{
0, 11, 23, 34, 45, 56, 68, 79, 90, 101, 113,
124, 135, 147, 158, 169, 180, 192, 203, 214, 226,
237, 248, 259, 271, 282, 293, 304, 316, 327, 338,
350, 361, 372, 383, 395, 406, 417, 428, 440, 451,
462, 474, 485, 496, 507, 519, 530, 541, 553, 564,
575, 586, 598, 609, 620, 631, 643, 654, 665, 677,
688, 699, 710, 722, 733, 744, 755, 767, 778, 789,
801, 812, 823, 834, 846, 857, 868, 880, 891, 902,
913, 925, 936, 947, 958, 970, 981, 992, 1004, 1015,
1026, 1037, 1049, 1060, 1071, 1082, 1094, 1105, 1116, 1128,
1139, 1150, 1161, 1173, 1184, 1195, 1206, 1218, 1229, 1240,
1252, 1263, 1274, 1285, 1297, 1308, 1319, 1331, 1342, 1353,
1364, 1376, 1387, 1398, 1409, 1421, 1432, 1443, 1455, 1466,
1477, 1488, 1500, 1511, 1522, 1533, 1545, 1556, 1567, 1579,
1590, 1601, 1612, 1624, 1635, 1646, 1658, 1669, 1680, 1691,
1703, 1714, 1725, 1736, 1748, 1759, 1770, 1782, 1793, 1804,
1815, 1827, 1838, 1849, 1860, 1872, 1883, 1894, 1906, 1917,
1928, 1939, 1951, 1962, 1973, 1985, 1996, 2007, 2018, 2030,
2041, 2052, 2063, 2075, 2086, 2097, 2109, 2120, 2131, 2142,
2154, 2165, 2176, 2187, 2199, 2210, 2221, 2233, 2244, 2255,
2266, 2278, 2289, 2300, 2312, 2323, 2334, 2345, 2357, 2368,
2379, 2390, 2402, 2413, 2424, 2436, 2447, 2458, 2469, 2481,
2492, 2503, 2514, 2526, 2537, 2548, 2560, 2571, 2582, 2593,
2605, 2616, 2627, 2639, 2650, 2661, 2672, 2684, 2695, 2706,
2717, 2729, 2740, 2751, 2763, 2774, 2785, 2796, 2808, 2819,
2830, 2841, 2853, 2864, 2875, 2887, 2898, 2909, 2920, 2932,
2943, 2954, 2966, 2977, 2988, 2999, 3011, 3022, 3033, 3044,
3056, 3067, 3078, 3090, 3101, 3112, 3123, 3135, 3146, 3157,
3168, 3180, 3191, 3202, 3214, 3225, 3236, 3247, 3259, 3270,
3281, 3293, 3304, 3315, 3326, 3338, 3349, 3360, 3371, 3383,
3394, 3405, 3417, 3428, 3439, 3450, 3462, 3473, 3484, 3495,
3507, 3518, 3529, 3541, 3552, 3563, 3574, 3586, 3597, 3608,
3619, 3631, 3642, 3653, 3665, 3676, 3687, 3698, 3710, 3721,
3732, 3744, 3755, 3766, 3777, 3789, 3800, 3811, 3822, 3834,
3845, 3856, 3868, 3879, 3890, 3901, 3913, 3924, 3935, 3946,
3958, 3969, 3980, 3992, 4003, 4014, 4025, 4037, 4048, 4059,
4071, 4082, 4093, 4104, 4116, 4127, 4138, 4149, 4161, 4172,
4183, 4195, 4206, 4217, 4228, 4240, 4251, 4262, 4273, 4285,
4296, 4307, 4319, 4330, 4341, 4352, 4364, 4375, 4386, 4398,
4409, 4420, 4431, 4443, 4454, 4465, 4476, 4488, 4499, 4499,
4488, 4476, 4465, 4454, 4443, 4431, 4420, 4409, 4398, 4386,
4375, 4364, 4352, 4341, 4330, 4319, 4307, 4296, 4285, 4273,
4262, 4251, 4240, 4228, 4217, 4206, 4195, 4183, 4172, 4161,
4149, 4138, 4127, 4116, 4104, 4093, 4082, 4071, 4059, 4048,
4037, 4025, 4014, 4003, 3992, 3980, 3969, 3958, 3946, 3935,
3924, 3913, 3901, 3890, 3879, 3868, 3856, 3845, 3834, 3822,
3811, 3800, 3789, 3777, 3766, 3755, 3744, 3732, 3721, 3710,
3698, 3687, 3676, 3665, 3653, 3642, 3631, 3619, 3608, 3597,
3586, 3574, 3563, 3552, 3541, 3529, 3518, 3507, 3495, 3484,
3473, 3462, 3450, 3439, 3428, 3417, 3405, 3394, 3383, 3371,
3360, 3349, 3338, 3326, 3315, 3304, 3293, 3281, 3270, 3259,
3247, 3236, 3225, 3214, 3202, 3191, 3180, 3168, 3157, 3146,
3135, 3123, 3112, 3101, 3090, 3078, 3067, 3056, 3044, 3033,
3022, 3011, 2999, 2988, 2977, 2966, 2954, 2943, 2932, 2920,
2909, 2898, 2887, 2875, 2864, 2853, 2841, 2830, 2819, 2808,
2796, 2785, 2774, 2763, 2751, 2740, 2729, 2717, 2706, 2695,
2684, 2672, 2661, 2650, 2639, 2627, 2616, 2605, 2593, 2582,
2571, 2560, 2548, 2537, 2526, 2514, 2503, 2492, 2481, 2469,
2458, 2447, 2436, 2424, 2413, 2402, 2390, 2379, 2368, 2357,
2345, 2334, 2323, 2312, 2300, 2289, 2278, 2266, 2255, 2244,
2233, 2221, 2210, 2199, 2187, 2176, 2165, 2154, 2142, 2131,
2120, 2109, 2097, 2086, 2075, 2063, 2052, 2041, 2030, 2018,
2007, 1996, 1985, 1973, 1962, 1951, 1939, 1928, 1917, 1906,
1894, 1883, 1872, 1860, 1849, 1838, 1827, 1815, 1804, 1793,
1782, 1770, 1759, 1748, 1736, 1725, 1714, 1703, 1691, 1680,
1669, 1658, 1646, 1635, 1624, 1612, 1601, 1590, 1579, 1567,
1556, 1545, 1533, 1522, 1511, 1500, 1488, 1477, 1466, 1455,
1443, 1432, 1421, 1409, 1398, 1387, 1376, 1364, 1353, 1342,
1331, 1319, 1308, 1297, 1285, 1274, 1263, 1252, 1240, 1229,
1218, 1206, 1195, 1184, 1173, 1161, 1150, 1139, 1128, 1116,
1105, 1094, 1082, 1071, 1060, 1049, 1037, 1026, 1015, 1004,
992, 981, 970, 958, 947, 936, 925, 913, 902, 891,
880, 868, 857, 846, 834, 823, 812, 801, 789, 778,
767, 755, 744, 733, 722, 710, 699, 688, 677, 665,
654, 643, 631, 620, 609, 598, 586, 575, 564, 553,
541, 530, 519, 507, 496, 485, 474, 462, 451, 440,
428, 417, 406, 395, 383, 372, 361, 350, 338, 327,
316, 304, 293, 282, 271, 259, 248, 237, 226, 214,
203, 192, 180, 169, 158, 147, 135, 124, 113, 101,
90, 79, 68, 56, 45, 34, 23, 11,
};

#elif defined(USE_SIN_WAVE)
const uint16_t g_u16PositiveHalf[800] =
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

#endif


static bool MoveNone(void);



typedef enum
{
	VIB_FORM_NONE = 0,
	VIB_FORM_SINE,
}VibrationWaveForm;


typedef struct
{
	TIM_HandleTypeDef *htim;
	uint32_t u32ActiveChannel;
	uint32_t u32CmpChannel;
}PolarityType;


typedef struct
{
	PolarityType sPolarityP;	// 正极
	PolarityType sPolarityN;	// 负极
	
	PolarityType *pActivePolarity;
	
	// 占空比输出参数
	uint32_t u32DutyZoom;
	
	// 索引跨度
	uint32_t u32Span;
	
	// 查表索引
	uint32_t u32Index;
	
	uint16_t u16Duty;
	
	bool bStartFlag;
	int16_t s16Offset;
	uint16_t u16DirChangeCount;
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
	uint8_t u8FSMStep;
}g_sMotionControl;


volatile uint8_t u8Test = 0;
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) 
{	
	for (uint8_t i = 0; i < 4; i++)
	{
		if (htim == g_sMotorControl.sMotor[i].pActivePolarity->htim &&
			htim->Channel == g_sMotorControl.sMotor[i].pActivePolarity->u32ActiveChannel)
		{
//			// 停掉当前激活的通道
//			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
//			g_sMotorControl.sMotor[i].bStartFlag = false;
			
			if ((g_sMotorControl.sMotor[i].u32Index >> 15) >= 800)
			{
				g_sMotorControl.sMotor[i].u32Index = 0;
				g_sMotorControl.sMotor[i].bStartFlag = false;
				
				// 停掉当前激活的通道
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 0);
			}
			else
			{
				// 查表, 缩放占空比
				g_sMotorControl.sMotor[i].u16Duty = (g_s16PositiveHalf[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				
				
				if (g_sMotorControl.sMotor[i].pActivePolarity == &g_sMotorControl.sMotor[i].sPolarityP)
				{
					if (g_sMotorControl.sMotor[i].s16Offset > 0)
					{
						g_sMotorControl.sMotor[i].u16Duty += g_sMotorControl.sMotor[i].s16Offset;
					}
					else if (g_sMotorControl.sMotor[i].s16Offset < 0)
					{
						int16_t s16OffsetDuty = (int16_t)g_sMotorControl.sMotor[i].u16Duty + g_sMotorControl.sMotor[i].s16Offset;
						if (s16OffsetDuty < 0)
						{
							g_sMotorControl.sMotor[i].u16Duty = 0;
						}
						else
						{
							g_sMotorControl.sMotor[i].u16Duty = s16OffsetDuty;
						}
					}
				}
				else
				{
					if (g_sMotorControl.sMotor[i].s16Offset > 0)
					{
						int16_t s16OffsetDuty = (int16_t)g_sMotorControl.sMotor[i].u16Duty - g_sMotorControl.sMotor[i].s16Offset;
						if (s16OffsetDuty < 0)
						{
							g_sMotorControl.sMotor[i].u16Duty = 0;
						}
						else
						{
							g_sMotorControl.sMotor[i].u16Duty = s16OffsetDuty;
						}
						
					}
					else if (g_sMotorControl.sMotor[i].s16Offset < 0)
					{
						g_sMotorControl.sMotor[i].u16Duty = (int16_t)g_sMotorControl.sMotor[i].u16Duty - g_sMotorControl.sMotor[i].s16Offset;
					}
				}
				
				

				
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
										g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
										g_sMotorControl.sMotor[i].u16Duty); 
				g_sMotorControl.sMotor[i].u32Index += g_sMotorControl.sMotor[i].u32Span;	
			}

			break;
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


__weak void CbMotionStopFinish(void)
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


// 将电机运动信息清空
void ClearMotorInfo(void)
{
	g_sMotionInfoBuffer.u8ReadIndex = 0;
	g_sMotionInfoBuffer.u8WriteIndex = 0;
	g_sMotionInfoBuffer.u8BufferLen = 0;
}


void MotionEndHandler(void)
{
	g_sMotionControl.u8FSMStep = 0;
	g_sMotionControl.pControlFunc = MoveNone;
	
	ENABLE_LEFT_TOP(1);
	ENABLE_RIGHT_TOP(1);
	ENABLE_LEFT_BOTTUM(1);
	ENABLE_RIGHT_BOTTUM(1);
	
	for (uint8_t i = 0; i < 4; i++)
	{
		g_sMotorControl.sMotor[i].u32Index = 0;
	}
}

uint16_t u16CmpValue = 0;
uint32_t u32TestTimer = 2;
static void MoveTest(void)
{
	enum
	{
		SET_MOTOR = 0,
		START_CMP,
		DELAY_TIME,
	};
	
	static uint32_t u32Timer = 0;
	static uint32_t u32Index = 0;
	
	static PolarityType *pActivePolarity = NULL;
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case SET_MOTOR:
			pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityP;
			g_sMotionControl.u8FSMStep = START_CMP;
			HAL_TIM_PWM_Start(pActivePolarity->htim, pActivePolarity->u32CmpChannel);
			u32Index = 0;
			__HAL_TIM_SET_COMPARE(pActivePolarity->htim, pActivePolarity->u32CmpChannel, 0);
			break;
		
		case START_CMP:
			u16CmpValue = g_s16PositiveHalf[u32Index];
			__HAL_TIM_SET_COMPARE(pActivePolarity->htim, pActivePolarity->u32CmpChannel, u16CmpValue);
			g_sMotionControl.u8FSMStep = DELAY_TIME;
			ResetTimerCount(&u32Timer);
			break;
		
		case DELAY_TIME:
			if (GetTimerTickDelta(u32Timer, GetCurTimerCount()) >= u32TestTimer)
			{
				u32Index = (u32Index + 1) % 800;
				g_sMotionControl.u8FSMStep = START_CMP;
			}
			break;
		
		default:
			break;
	}
}


uint32_t u32UpOffsetTest = 400;
// 向上运动
static bool MoveUpMode(void)
{		
	bool bRet = false;
	
	enum
	{
		START_TIME = 0,
		INIT_MOTION,
		PREPARE_MOTION,
		WAIT_MOTION1GROUP_STOP,
		WAIT_MOTION2GROUP_STOP,
		CHECK_DIR_COUNT,
	};
	
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
			g_sMotorControl.sMotor[1].u32Index = u32UpOffsetTest * 32768;
			g_sMotorControl.sMotor[2].u32Index = u32UpOffsetTest * 32768;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16PositiveHalf[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
				g_sMotorControl.sMotor[i].u32Index += g_sMotorControl.sMotor[i].u32Span;	
			}
			break;
		
		case PREPARE_MOTION:
			g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			for (uint8_t i = 0; i < 4; i++)
			{
				if (g_sMotorControl.sMotor[i].pActivePolarity)
				{
					g_sMotorControl.sMotor[i].bStartFlag = true;
					g_sMotorControl.sMotor[i].u16DirChangeCount = 0;
					HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);					
				}
			}
			break;
			
		case WAIT_MOTION1GROUP_STOP:
			if (!g_sMotorControl.sMotor[1].bStartFlag && !g_sMotorControl.sMotor[2].bStartFlag)
			{
				if (g_sMotorControl.sMotor[1].pActivePolarity == &g_sMotorControl.sMotor[1].sPolarityP)
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[2].pActivePolarity == &g_sMotorControl.sMotor[2].sPolarityP)
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityP;
				}
				
				g_sMotorControl.sMotor[1].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].pActivePolarity->htim, g_sMotorControl.sMotor[1].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[1].bStartFlag = true;
				
				g_sMotorControl.sMotor[2].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[2].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[2].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				if (g_sMotorControl.sMotor[0].pActivePolarity == &g_sMotorControl.sMotor[0].sPolarityP)
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[3].pActivePolarity == &g_sMotorControl.sMotor[3].sPolarityP)
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityP;
				}
				
				g_sMotorControl.sMotor[0].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].pActivePolarity->htim, g_sMotorControl.sMotor[0].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[0].bStartFlag = true;
				
				g_sMotorControl.sMotor[3].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[3].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[3].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				g_sMotionControl.u8FSMStep = INIT_MOTION;
			}
			else
			{
				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			}
			break;
			
		default:
			break;			
	}
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);

		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}

uint32_t u32DownOffsetTest = 400;
// 向下运动
static bool MoveDownMode(void)
{
	bool bRet = false;
	
	enum
	{
		START_TIME = 0,
		INIT_MOTION,
		PREPARE_MOTION,
		WAIT_MOTION1GROUP_STOP,
		WAIT_MOTION2GROUP_STOP,
		CHECK_DIR_COUNT,
	};
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			break;
		
		case INIT_MOTION:	
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
			g_sMotorControl.sMotor[0].u32Index = u32DownOffsetTest * 32768;
			g_sMotorControl.sMotor[3].u32Index = u32DownOffsetTest * 32768;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16PositiveHalf[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
				g_sMotorControl.sMotor[i].u32Index += g_sMotorControl.sMotor[i].u32Span;	
			}
			break;
		
		case PREPARE_MOTION:
			g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			for (uint8_t i = 0; i < 4; i++)
			{
				if (g_sMotorControl.sMotor[i].pActivePolarity)
				{
					g_sMotorControl.sMotor[i].bStartFlag = true;
					g_sMotorControl.sMotor[i].u16DirChangeCount = 0;
					HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);					
				}
			}
			break;
			
		case WAIT_MOTION1GROUP_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				if (g_sMotorControl.sMotor[0].pActivePolarity == &g_sMotorControl.sMotor[0].sPolarityP)
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[3].pActivePolarity == &g_sMotorControl.sMotor[3].sPolarityP)
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityP;
				}
				
				g_sMotorControl.sMotor[0].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].pActivePolarity->htim, g_sMotorControl.sMotor[0].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[0].bStartFlag = true;
				
				g_sMotorControl.sMotor[3].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[3].pActivePolarity->htim, g_sMotorControl.sMotor[3].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[3].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[1].bStartFlag && !g_sMotorControl.sMotor[2].bStartFlag)
			{
				if (g_sMotorControl.sMotor[1].pActivePolarity == &g_sMotorControl.sMotor[1].sPolarityP)
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[2].pActivePolarity == &g_sMotorControl.sMotor[2].sPolarityP)
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityP;
				}
				
				g_sMotorControl.sMotor[1].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].pActivePolarity->htim, g_sMotorControl.sMotor[1].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[1].bStartFlag = true;
				
				g_sMotorControl.sMotor[2].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[2].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[2].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				g_sMotionControl.u8FSMStep = INIT_MOTION;
			}
			else
			{
				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);

		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}


// 向左运动
static bool MoveLeftMode(void)
{
	bool bRet = false;
	
	enum
	{
		START_TIME = 0,
		INIT_MOTION,
		PREPARE_MOTION,
		WAIT_MOTION1GROUP_STOP,
		WAIT_MOTION2GROUP_STOP,
		CHECK_DIR_COUNT,
	};
	
	switch (g_sMotionControl.u8FSMStep)
	{	
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
			g_sMotorControl.sMotor[2].u32Index = 400 * 32768;
			g_sMotorControl.sMotor[3].u32Index = 400 * 32768;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16PositiveHalf[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
				g_sMotorControl.sMotor[i].u32Index += g_sMotorControl.sMotor[i].u32Span;	
			}
			break;
		
		case PREPARE_MOTION:
			g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].bStartFlag = true;
				g_sMotorControl.sMotor[i].u16DirChangeCount = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
			}
			break;
			
		case WAIT_MOTION1GROUP_STOP:
			if (!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				if (g_sMotorControl.sMotor[2].pActivePolarity == &g_sMotorControl.sMotor[2].sPolarityP)
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[3].pActivePolarity == &g_sMotorControl.sMotor[3].sPolarityP)
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityP;
				}
				
				g_sMotorControl.sMotor[2].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[2].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[2].bStartFlag = true;
				
				g_sMotorControl.sMotor[3].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[3].pActivePolarity->htim, g_sMotorControl.sMotor[3].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[3].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[1].bStartFlag && !g_sMotorControl.sMotor[0].bStartFlag)
			{
				if (g_sMotorControl.sMotor[1].pActivePolarity == &g_sMotorControl.sMotor[1].sPolarityP)
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[0].pActivePolarity == &g_sMotorControl.sMotor[0].sPolarityP)
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityP;
				}
				
				g_sMotorControl.sMotor[1].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].pActivePolarity->htim, g_sMotorControl.sMotor[1].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[1].bStartFlag = true;
				
				g_sMotorControl.sMotor[0].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].pActivePolarity->htim, g_sMotorControl.sMotor[0].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[0].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				g_sMotionControl.u8FSMStep = INIT_MOTION;
			}
			else
			{
				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);

		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}


// 向右运动
static bool MoveRightMode(void)
{
	bool bRet = false;
	
	enum
	{
		START_TIME = 0,
		INIT_MOTION,
		PREPARE_MOTION,
		WAIT_MOTION1GROUP_STOP,
		WAIT_MOTION2GROUP_STOP,
		CHECK_DIR_COUNT,
	};
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
			g_sMotorControl.sMotor[0].u32Index = 400 * 32768;
			g_sMotorControl.sMotor[1].u32Index = 400 * 32768;
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16PositiveHalf[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
				g_sMotorControl.sMotor[i].u32Index += g_sMotorControl.sMotor[i].u32Span;	
			}
			break;
		
		case PREPARE_MOTION:
			g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].bStartFlag = true;
				g_sMotorControl.sMotor[i].u16DirChangeCount = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
			}
			break;
			
		case WAIT_MOTION1GROUP_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[1].bStartFlag)
			{
				if (g_sMotorControl.sMotor[0].pActivePolarity == &g_sMotorControl.sMotor[0].sPolarityP)
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[1].pActivePolarity == &g_sMotorControl.sMotor[1].sPolarityP)
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityP;
				}
				
				g_sMotorControl.sMotor[0].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].pActivePolarity->htim, g_sMotorControl.sMotor[0].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[0].bStartFlag = true;
				
				g_sMotorControl.sMotor[1].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].pActivePolarity->htim, g_sMotorControl.sMotor[1].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[1].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				if (g_sMotorControl.sMotor[2].pActivePolarity == &g_sMotorControl.sMotor[2].sPolarityP)
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[3].pActivePolarity == &g_sMotorControl.sMotor[3].sPolarityP)
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityP;
				}
				
				g_sMotorControl.sMotor[2].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[2].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[2].bStartFlag = true;
				
				g_sMotorControl.sMotor[3].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[3].pActivePolarity->htim, g_sMotorControl.sMotor[3].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[3].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				g_sMotionControl.u8FSMStep = INIT_MOTION;
			}
			else
			{
				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);

		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}


// 居中
static bool MoveGatherMode(void)
{
	bool bRet = false;
	
	enum
	{
		START_TIME = 0,
		INIT_MOTION,
		PREPARE_MOTION,
		WAIT_MOTION1GROUP_STOP,
		WAIT_MOTION2GROUP_STOP,
		CHECK_DIR_COUNT,
	};
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16PositiveHalf[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
				g_sMotorControl.sMotor[i].u32Index += g_sMotorControl.sMotor[i].u32Span;	
			}
			break;
		
		case PREPARE_MOTION:
			g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].bStartFlag = true;
				g_sMotorControl.sMotor[i].u16DirChangeCount = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
			}		
			break;
		
		case WAIT_MOTION1GROUP_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[1].bStartFlag && 
				!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				if (g_sMotorControl.sMotor[0].pActivePolarity == &g_sMotorControl.sMotor[0].sPolarityP)
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[0].u16DirChangeCount++;
					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[1].pActivePolarity == &g_sMotorControl.sMotor[1].sPolarityP)
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[1].u16DirChangeCount++;
					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[2].pActivePolarity == &g_sMotorControl.sMotor[2].sPolarityP)
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[2].u16DirChangeCount++;
					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityP;
				}
				
				if (g_sMotorControl.sMotor[3].pActivePolarity == &g_sMotorControl.sMotor[3].sPolarityP)
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityN;
				}
				else
				{
					g_sMotorControl.sMotor[3].u16DirChangeCount++;
					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityP;
				}
				
				g_sMotorControl.sMotor[0].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].pActivePolarity->htim, g_sMotorControl.sMotor[0].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[0].bStartFlag = true;
				
				g_sMotorControl.sMotor[1].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].pActivePolarity->htim, g_sMotorControl.sMotor[1].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[1].bStartFlag = true;
				
				g_sMotorControl.sMotor[2].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[2].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[2].bStartFlag = true;
				
				g_sMotorControl.sMotor[3].u32Index = 0;
				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[3].pActivePolarity->htim, g_sMotorControl.sMotor[3].pActivePolarity->u32CmpChannel);
				g_sMotorControl.sMotor[3].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);

		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}

// 振散
static bool MoveDisperseMode(void)
{
	bool bRet = false;
	
	enum
	{
		START_TIME = 0,
		PREPARE_MOTION1,
		WAIT_MOTION1_STOP,
		PREPARE_MOTION2,
		WAIT_MOTION2_STOP,
	};
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = PREPARE_MOTION1;		
			break;
		
		case PREPARE_MOTION1:
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
			g_sMotorControl.sMotor[0].bStartFlag = true;
		
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
			g_sMotorControl.sMotor[1].bStartFlag = true;
		
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
			g_sMotorControl.sMotor[2].bStartFlag = true;
		
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
			g_sMotorControl.sMotor[3].bStartFlag = true;
				
			g_sMotionControl.u8FSMStep = WAIT_MOTION1_STOP;			
			break;
		
		case WAIT_MOTION1_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[1].bStartFlag && 
				!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
								
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
				
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);
								
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
				
				g_sMotionControl.u8FSMStep = PREPARE_MOTION2;	
			}
			break;
			
		case PREPARE_MOTION2:	
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
			g_sMotorControl.sMotor[0].bStartFlag = true;
		
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
			g_sMotorControl.sMotor[1].bStartFlag = true;
		
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
			g_sMotorControl.sMotor[2].bStartFlag = true;
				
			HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
			__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
			HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
			g_sMotorControl.sMotor[3].bStartFlag = true;
		
			g_sMotionControl.u8FSMStep = WAIT_MOTION2_STOP;		
			break;
		
		case WAIT_MOTION2_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[1].bStartFlag && 
				!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
				
				
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
				
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
				
				
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);
				
				g_sMotionControl.u8FSMStep = PREPARE_MOTION1;	
			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityP.htim, g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[0].sPolarityN.htim, g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityP.htim, g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[1].sPolarityN.htim, g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel, 0);
		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityP.htim, g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel, 0);
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[2].sPolarityN.htim, g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel, 0);

		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityP.htim, g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel, 0);		
		HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel);
		__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[3].sPolarityN.htim, g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel, 0);
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}



// 无运动
static bool MoveNone(void)
{
	return true;
}



void MotorControlStop(void)
{
	// 停止当前运动
	g_sMotionControl.pControlFunc = MoveNone;
	
	// 将运动序列中的所有运动清除
	ClearMotorInfo();
	
	CbMotionStopFinish();
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
		
		case MOTION_GATHER:
			g_sMotionControl.pControlFunc = MoveGatherMode;
			break;
		
		case MOTION_H_GATHER:
			//g_sMotionControl.pControlFunc = MoveGatherMode;
			break;
		
		case MOTION_V_GATHER:
			//g_sMotionControl.pControlFunc = MoveGatherMode;
			break;

		case MOTION_DISPERSE:
			g_sMotionControl.pControlFunc = MoveDisperseMode;
			break;
		
		case MOTION_FLIP:
			//g_sMotionControl.pControlFunc = MoveGatherMode;
			break;
		
		default:
			g_sMotionControl.pControlFunc = MoveNone;
			break;
	}
}

float fM0Coeff = 1.0f;
float fM3Coeff = 1.0f;
static void PrepareMotion(const MotorMotionParams *pParams)
{
	const float fPulseCycle = 0.0000625f;				// 1周期pwm用时
	const uint32_t fPWMCount10HzHalfCycle = 800;		// 半周期振动下PWM总数
	
	// 计算当前运动的耗时, 单位为ms
	g_sMotionControl.u32TimeUseExpect = pParams->fTimeUse * 1000;
	ResetTimerCount(&g_sMotionControl.u32TimerCount);
		
	// 计算电压转换到PWM输出占空比的系数
	float fVoltage = pParams->fVoltage;
	if (fVoltage > MAX_VOLTAGE_VALUE)
	{
		fVoltage = MAX_VOLTAGE_VALUE;
	}
	// 得到一个放大后的整形值, 便于计算
	for (uint8_t i = 0; i < 4; i++)
	{
		g_sMotorControl.sMotor[i].u32DutyZoom = (uint16_t)(fVoltage / MAX_VOLTAGE_VALUE * 32768.0f);
		g_sMotorControl.sMotor[i].u32Index = 0;
	}
	
	g_sMotorControl.sMotor[0].u32DutyZoom = g_sMotorControl.sMotor[0].u32DutyZoom * fM0Coeff;
	g_sMotorControl.sMotor[3].u32DutyZoom = g_sMotorControl.sMotor[3].u32DutyZoom * fM3Coeff;
	
	// 16kHz的PWM, 10Hz振动频率时, 一个周期包含1600个PWM周期, 以此为基准
	float fFrequency = pParams->fFrequency;
	if (fFrequency > 200.0f)
	{
		fFrequency = 200.0f;
	}
	
	// 计算查表跨度, 需要得到的是半周期的PWM总数, 进而得到索引每次需要增加多少
	uint16_t u16PWMCount = (uint16_t)(1.0f / fFrequency / fPulseCycle / 2.0f);
	
	for (uint8_t i = 0; i < 4; i++)
	{
		g_sMotorControl.sMotor[i].u32Span = (uint32_t)(fPWMCount10HzHalfCycle * 32768.0f / u16PWMCount);
	}
	
	ENABLE_LEFT_TOP(0);
	ENABLE_RIGHT_TOP(0);
	ENABLE_LEFT_BOTTUM(0);
	ENABLE_RIGHT_BOTTUM(0);
	
	g_sMotionControl.u8FSMStep = 0;
	GetMotionFunc(pParams->eMotionIndex);
	g_sMotionControl.u8Index = pParams->eMotionIndex;
}


void MotionControlInit(void)
{
	g_sMotionControl.pControlFunc = MoveNone;
	
	// 将Motor数据结构和硬件对应起来
	
	// 左上
	g_sMotorControl.sMotor[0].sPolarityP.htim = &htim2;
	g_sMotorControl.sMotor[0].sPolarityP.u32CmpChannel = TIM_CHANNEL_1;
	g_sMotorControl.sMotor[0].sPolarityP.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_1;
	g_sMotorControl.sMotor[0].sPolarityN.htim = &htim4;
	g_sMotorControl.sMotor[0].sPolarityN.u32CmpChannel = TIM_CHANNEL_1;
	g_sMotorControl.sMotor[0].sPolarityN.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_1;

	// 左下
	g_sMotorControl.sMotor[1].sPolarityP.htim = &htim2;
	g_sMotorControl.sMotor[1].sPolarityP.u32CmpChannel = TIM_CHANNEL_2;
	g_sMotorControl.sMotor[1].sPolarityP.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_2;
	g_sMotorControl.sMotor[1].sPolarityN.htim = &htim4;
	g_sMotorControl.sMotor[1].sPolarityN.u32CmpChannel = TIM_CHANNEL_2;
	g_sMotorControl.sMotor[1].sPolarityN.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_2;	
	
	// 右下
	g_sMotorControl.sMotor[2].sPolarityP.htim = &htim2;
	g_sMotorControl.sMotor[2].sPolarityP.u32CmpChannel = TIM_CHANNEL_4;
	g_sMotorControl.sMotor[2].sPolarityP.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_4;
	g_sMotorControl.sMotor[2].sPolarityN.htim = &htim4;
	g_sMotorControl.sMotor[2].sPolarityN.u32CmpChannel = TIM_CHANNEL_4;
	g_sMotorControl.sMotor[2].sPolarityN.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_4;	
	
	// 右上
	g_sMotorControl.sMotor[3].sPolarityP.htim = &htim2;
	g_sMotorControl.sMotor[3].sPolarityP.u32CmpChannel = TIM_CHANNEL_3;
	g_sMotorControl.sMotor[3].sPolarityP.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_3;
	g_sMotorControl.sMotor[3].sPolarityN.htim = &htim4;
	g_sMotorControl.sMotor[3].sPolarityN.u32CmpChannel = TIM_CHANNEL_3;
	g_sMotorControl.sMotor[3].sPolarityN.u32ActiveChannel = HAL_TIM_ACTIVE_CHANNEL_3;	
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


