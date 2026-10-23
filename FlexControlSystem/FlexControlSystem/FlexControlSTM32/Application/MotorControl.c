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

#define USE_SIN_WAVE
//#define USE_FIX_INC

#if defined(USE_FIX_INC)
const int16_t g_s16DutyTable1Cycle[1600] = 
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
90, 79, 68, 56, 45, 34, 23, 11, 0,
-11, -23, -34, -45, -56, -68, -79, -90, -101, -113,
-124, -135, -147, -158, -169, -180, -192, -203, -214, -226,
-237, -248, -259, -271, -282, -293, -304, -316, -327, -338,
-350, -361, -372, -383, -395, -406, -417, -428, -440, -451,
-462, -474, -485, -496, -507, -519, -530, -541, -553, -564,
-575, -586, -598, -609, -620, -631, -643, -654, -665, -677,
-688, -699, -710, -722, -733, -744, -755, -767, -778, -789,
-801, -812, -823, -834, -846, -857, -868, -880, -891, -902,
-913, -925, -936, -947, -958, -970, -981, -992, -1004, -1015,
-1026, -1037, -1049, -1060, -1071, -1082, -1094, -1105, -1116, -1128,
-1139, -1150, -1161, -1173, -1184, -1195, -1206, -1218, -1229, -1240,
-1252, -1263, -1274, -1285, -1297, -1308, -1319, -1331, -1342, -1353,
-1364, -1376, -1387, -1398, -1409, -1421, -1432, -1443, -1455, -1466,
-1477, -1488, -1500, -1511, -1522, -1533, -1545, -1556, -1567, -1579,
-1590, -1601, -1612, -1624, -1635, -1646, -1658, -1669, -1680, -1691,
-1703, -1714, -1725, -1736, -1748, -1759, -1770, -1782, -1793, -1804,
-1815, -1827, -1838, -1849, -1860, -1872, -1883, -1894, -1906, -1917,
-1928, -1939, -1951, -1962, -1973, -1985, -1996, -2007, -2018, -2030,
-2041, -2052, -2063, -2075, -2086, -2097, -2109, -2120, -2131, -2142,
-2154, -2165, -2176, -2187, -2199, -2210, -2221, -2233, -2244, -2255,
-2266, -2278, -2289, -2300, -2312, -2323, -2334, -2345, -2357, -2368,
-2379, -2390, -2402, -2413, -2424, -2436, -2447, -2458, -2469, -2481,
-2492, -2503, -2514, -2526, -2537, -2548, -2560, -2571, -2582, -2593,
-2605, -2616, -2627, -2639, -2650, -2661, -2672, -2684, -2695, -2706,
-2717, -2729, -2740, -2751, -2763, -2774, -2785, -2796, -2808, -2819,
-2830, -2841, -2853, -2864, -2875, -2887, -2898, -2909, -2920, -2932,
-2943, -2954, -2966, -2977, -2988, -2999, -3011, -3022, -3033, -3044,
-3056, -3067, -3078, -3090, -3101, -3112, -3123, -3135, -3146, -3157,
-3168, -3180, -3191, -3202, -3214, -3225, -3236, -3247, -3259, -3270,
-3281, -3293, -3304, -3315, -3326, -3338, -3349, -3360, -3371, -3383,
-3394, -3405, -3417, -3428, -3439, -3450, -3462, -3473, -3484, -3495,
-3507, -3518, -3529, -3541, -3552, -3563, -3574, -3586, -3597, -3608,
-3619, -3631, -3642, -3653, -3665, -3676, -3687, -3698, -3710, -3721,
-3732, -3744, -3755, -3766, -3777, -3789, -3800, -3811, -3822, -3834,
-3845, -3856, -3868, -3879, -3890, -3901, -3913, -3924, -3935, -3946,
-3958, -3969, -3980, -3992, -4003, -4014, -4025, -4037, -4048, -4059,
-4071, -4082, -4093, -4104, -4116, -4127, -4138, -4149, -4161, -4172,
-4183, -4195, -4206, -4217, -4228, -4240, -4251, -4262, -4273, -4285,
-4296, -4307, -4319, -4330, -4341, -4352, -4364, -4375, -4386, -4398,
-4409, -4420, -4431, -4443, -4454, -4465, -4476, -4488, -4499,
-4488, -4476, -4465, -4454, -4443, -4431, -4420, -4409, -4398, -4386,
-4375, -4364, -4352, -4341, -4330, -4319, -4307, -4296, -4285, -4273,
-4262, -4251, -4240, -4228, -4217, -4206, -4195, -4183, -4172, -4161,
-4149, -4138, -4127, -4116, -4104, -4093, -4082, -4071, -4059, -4048,
-4037, -4025, -4014, -4003, -3992, -3980, -3969, -3958, -3946, -3935,
-3924, -3913, -3901, -3890, -3879, -3868, -3856, -3845, -3834, -3822,
-3811, -3800, -3789, -3777, -3766, -3755, -3744, -3732, -3721, -3710,
-3698, -3687, -3676, -3665, -3653, -3642, -3631, -3619, -3608, -3597,
-3586, -3574, -3563, -3552, -3541, -3529, -3518, -3507, -3495, -3484,
-3473, -3462, -3450, -3439, -3428, -3417, -3405, -3394, -3383, -3371,
-3360, -3349, -3338, -3326, -3315, -3304, -3293, -3281, -3270, -3259,
-3247, -3236, -3225, -3214, -3202, -3191, -3180, -3168, -3157, -3146,
-3135, -3123, -3112, -3101, -3090, -3078, -3067, -3056, -3044, -3033,
-3022, -3011, -2999, -2988, -2977, -2966, -2954, -2943, -2932, -2920,
-2909, -2898, -2887, -2875, -2864, -2853, -2841, -2830, -2819, -2808,
-2796, -2785, -2774, -2763, -2751, -2740, -2729, -2717, -2706, -2695,
-2684, -2672, -2661, -2650, -2639, -2627, -2616, -2605, -2593, -2582,
-2571, -2560, -2548, -2537, -2526, -2514, -2503, -2492, -2481, -2469,
-2458, -2447, -2436, -2424, -2413, -2402, -2390, -2379, -2368, -2357,
-2345, -2334, -2323, -2312, -2300, -2289, -2278, -2266, -2255, -2244,
-2233, -2221, -2210, -2199, -2187, -2176, -2165, -2154, -2142, -2131,
-2120, -2109, -2097, -2086, -2075, -2063, -2052, -2041, -2030, -2018,
-2007, -1996, -1985, -1973, -1962, -1951, -1939, -1928, -1917, -1906,
-1894, -1883, -1872, -1860, -1849, -1838, -1827, -1815, -1804, -1793,
-1782, -1770, -1759, -1748, -1736, -1725, -1714, -1703, -1691, -1680,
-1669, -1658, -1646, -1635, -1624, -1612, -1601, -1590, -1579, -1567,
-1556, -1545, -1533, -1522, -1511, -1500, -1488, -1477, -1466, -1455,
-1443, -1432, -1421, -1409, -1398, -1387, -1376, -1364, -1353, -1342,
-1331, -1319, -1308, -1297, -1285, -1274, -1263, -1252, -1240, -1229,
-1218, -1206, -1195, -1184, -1173, -1161, -1150, -1139, -1128, -1116,
-1105, -1094, -1082, -1071, -1060, -1049, -1037, -1026, -1015, -1004,
-992, -981, -970, -958, -947, -936, -925, -913, -902, -891,
-880, -868, -857, -846, -834, -823, -812, -801, -789, -778,
-767, -755, -744, -733, -722, -710, -699, -688, -677, -665,
-654, -643, -631, -620, -609, -598, -586, -575, -564, -553,
-541, -530, -519, -507, -496, -485, -474, -462, -451, -440,
-428, -417, -406, -395, -383, -372, -361, -350, -338, -327,
-316, -304, -293, -282, -271, -259, -248, -237, -226, -214,
-203, -192, -180, -169, -158, -147, -135, -124, -113, -101,
-90, -79, -68, -56, -45, -34, -23, -11,
};

#elif defined(USE_SIN_WAVE)
const int16_t g_s16DutyTable1Cycle[1600] =
{
0, 18, 35, 53, 71, 88, 106, 124, 141, 159, 177,
194, 212, 230, 247, 265, 283, 300, 318, 336, 353,
371, 388, 406, 424, 441, 459, 476, 494, 512, 529,
547, 564, 582, 599, 617, 634, 652, 669, 687, 704,
722, 739, 757, 774, 791, 809, 826, 844, 861, 878,
896, 913, 930, 947, 965, 982, 999, 1017, 1034, 1051,
1068, 1085, 1102, 1120, 1137, 1154, 1171, 1188, 1205, 1222,
1239, 1256, 1273, 1290, 1307, 1324, 1341, 1357, 1374, 1391,
1408, 1425, 1441, 1458, 1475, 1492, 1508, 1525, 1542, 1558,
1575, 1591, 1608, 1624, 1641, 1657, 1674, 1690, 1706, 1723,
1739, 1755, 1772, 1788, 1804, 1820, 1836, 1852, 1869, 1885,
1901, 1917, 1933, 1949, 1965, 1980, 1996, 2012, 2028, 2044,
2059, 2075, 2091, 2106, 2122, 2138, 2153, 2169, 2184, 2200,
2215, 2230, 2246, 2261, 2276, 2291, 2307, 2322, 2337, 2352,
2367, 2382, 2397, 2412, 2427, 2442, 2457, 2471, 2486, 2501,
2516, 2530, 2545, 2559, 2574, 2588, 2603, 2617, 2632, 2646,
2660, 2674, 2689, 2703, 2717, 2731, 2745, 2759, 2773, 2787,
2801, 2814, 2828, 2842, 2856, 2869, 2883, 2896, 2910, 2923,
2937, 2950, 2963, 2977, 2990, 3003, 3016, 3029, 3042, 3055,
3068, 3081, 3094, 3107, 3120, 3132, 3145, 3158, 3170, 3183,
3195, 3208, 3220, 3232, 3245, 3257, 3269, 3281, 3293, 3305,
3317, 3329, 3341, 3353, 3365, 3376, 3388, 3400, 3411, 3423,
3434, 3445, 3457, 3468, 3479, 3491, 3502, 3513, 3524, 3535,
3546, 3556, 3567, 3578, 3589, 3599, 3610, 3620, 3631, 3641,
3652, 3662, 3672, 3682, 3693, 3703, 3713, 3723, 3732, 3742,
3752, 3762, 3771, 3781, 3791, 3800, 3810, 3819, 3828, 3838,
3847, 3856, 3865, 3874, 3883, 3892, 3901, 3909, 3918, 3927,
3935, 3944, 3952, 3961, 3969, 3977, 3986, 3994, 4002, 4010,
4018, 4026, 4034, 4042, 4049, 4057, 4065, 4072, 4080, 4087,
4094, 4102, 4109, 4116, 4123, 4130, 4137, 4144, 4151, 4158,
4165, 4171, 4178, 4184, 4191, 4197, 4203, 4210, 4216, 4222,
4228, 4234, 4240, 4246, 4252, 4258, 4263, 4269, 4274, 4280,
4285, 4291, 4296, 4301, 4306, 4311, 4316, 4321, 4326, 4331,
4336, 4341, 4345, 4350, 4354, 4359, 4363, 4367, 4371, 4376,
4380, 4384, 4388, 4391, 4395, 4399, 4403, 4406, 4410, 4413,
4417, 4420, 4423, 4426, 4430, 4433, 4436, 4439, 4441, 4444,
4447, 4450, 4452, 4455, 4457, 4459, 4462, 4464, 4466, 4468,
4470, 4472, 4474, 4476, 4478, 4479, 4481, 4483, 4484, 4485,
4487, 4488, 4489, 4490, 4491, 4492, 4493, 4494, 4495, 4496,
4496, 4497, 4497, 4498, 4498, 4499, 4499, 4499, 4499, 4499,
4499, 4499, 4499, 4498, 4498, 4498, 4497, 4497, 4496, 4495,
4495, 4494, 4493, 4492, 4491, 4490, 4489, 4487, 4486, 4485,
4483, 4482, 4480, 4479, 4477, 4475, 4473, 4471, 4469, 4467,
4465, 4463, 4461, 4458, 4456, 4453, 4451, 4448, 4446, 4443,
4440, 4437, 4434, 4431, 4428, 4425, 4422, 4418, 4415, 4412,
4408, 4405, 4401, 4397, 4393, 4390, 4386, 4382, 4378, 4374,
4369, 4365, 4361, 4356, 4352, 4347, 4343, 4338, 4333, 4329,
4324, 4319, 4314, 4309, 4304, 4299, 4293, 4288, 4283, 4277,
4272, 4266, 4260, 4255, 4249, 4243, 4237, 4231, 4225, 4219,
4213, 4207, 4200, 4194, 4188, 4181, 4174, 4168, 4161, 4154,
4148, 4141, 4134, 4127, 4120, 4113, 4105, 4098, 4091, 4083,
4076, 4068, 4061, 4053, 4045, 4038, 4030, 4022, 4014, 4006,
3998, 3990, 3982, 3973, 3965, 3957, 3948, 3940, 3931, 3922,
3914, 3905, 3896, 3887, 3878, 3869, 3860, 3851, 3842, 3833,
3824, 3814, 3805, 3795, 3786, 3776, 3767, 3757, 3747, 3737,
3728, 3718, 3708, 3698, 3687, 3677, 3667, 3657, 3647, 3636,
3626, 3615, 3605, 3594, 3583, 3573, 3562, 3551, 3540, 3529,
3518, 3507, 3496, 3485, 3474, 3462, 3451, 3440, 3428, 3417,
3405, 3394, 3382, 3370, 3359, 3347, 3335, 3323, 3311, 3299,
3287, 3275, 3263, 3251, 3239, 3226, 3214, 3202, 3189, 3177,
3164, 3151, 3139, 3126, 3113, 3101, 3088, 3075, 3062, 3049,
3036, 3023, 3010, 2997, 2983, 2970, 2957, 2943, 2930, 2917,
2903, 2890, 2876, 2862, 2849, 2835, 2821, 2808, 2794, 2780,
2766, 2752, 2738, 2724, 2710, 2696, 2681, 2667, 2653, 2639,
2624, 2610, 2596, 2581, 2567, 2552, 2538, 2523, 2508, 2494,
2479, 2464, 2449, 2434, 2419, 2405, 2390, 2375, 2360, 2345,
2329, 2314, 2299, 2284, 2269, 2253, 2238, 2223, 2207, 2192,
2176, 2161, 2145, 2130, 2114, 2099, 2083, 2067, 2052, 2036,
2020, 2004, 1988, 1972, 1957, 1941, 1925, 1909, 1893, 1877,
1861, 1844, 1828, 1812, 1796, 1780, 1763, 1747, 1731, 1715,
1698, 1682, 1665, 1649, 1632, 1616, 1599, 1583, 1566, 1550,
1533, 1517, 1500, 1483, 1467, 1450, 1433, 1416, 1400, 1383,
1366, 1349, 1332, 1315, 1298, 1281, 1264, 1247, 1230, 1213,
1196, 1179, 1162, 1145, 1128, 1111, 1094, 1077, 1060, 1042,
1025, 1008, 991, 973, 956, 939, 922, 904, 887, 870,
852, 835, 817, 800, 783, 765, 748, 730, 713, 696,
678, 661, 643, 626, 608, 591, 573, 555, 538, 520,
503, 485, 468, 450, 432, 415, 397, 380, 362, 344,
327, 309, 291, 274, 256, 239, 221, 203, 186, 168,
150, 133, 115, 97, 80, 62, 44, 27, 9, -9,
-27, -44, -62, -80, -97, -115, -133, -150, -168, -186,
-203, -221, -239, -256, -274, -291, -309, -327, -344, -362,
-380, -397, -415, -432, -450, -468, -485, -503, -520, -538,
-555, -573, -591, -608, -626, -643, -661, -678, -696, -713,
-730, -748, -765, -783, -800, -817, -835, -852, -870, -887,
-904, -922, -939, -956, -973, -991, -1008, -1025, -1042, -1060,
-1077, -1094, -1111, -1128, -1145, -1162, -1179, -1196, -1213, -1230,
-1247, -1264, -1281, -1298, -1315, -1332, -1349, -1366, -1383, -1400,
-1416, -1433, -1450, -1467, -1483, -1500, -1517, -1533, -1550, -1566,
-1583, -1599, -1616, -1632, -1649, -1665, -1682, -1698, -1715, -1731,
-1747, -1763, -1780, -1796, -1812, -1828, -1844, -1861, -1877, -1893,
-1909, -1925, -1941, -1957, -1972, -1988, -2004, -2020, -2036, -2052,
-2067, -2083, -2099, -2114, -2130, -2145, -2161, -2176, -2192, -2207,
-2223, -2238, -2253, -2269, -2284, -2299, -2314, -2329, -2344, -2360,
-2375, -2390, -2405, -2419, -2434, -2449, -2464, -2479, -2494, -2508,
-2523, -2538, -2552, -2567, -2581, -2596, -2610, -2624, -2639, -2653,
-2667, -2681, -2696, -2710, -2724, -2738, -2752, -2766, -2780, -2794,
-2808, -2821, -2835, -2849, -2862, -2876, -2890, -2903, -2917, -2930,
-2943, -2957, -2970, -2983, -2997, -3010, -3023, -3036, -3049, -3062,
-3075, -3088, -3101, -3113, -3126, -3139, -3151, -3164, -3177, -3189,
-3202, -3214, -3226, -3239, -3251, -3263, -3275, -3287, -3299, -3311,
-3323, -3335, -3347, -3359, -3370, -3382, -3394, -3405, -3417, -3428,
-3440, -3451, -3462, -3474, -3485, -3496, -3507, -3518, -3529, -3540,
-3551, -3562, -3573, -3583, -3594, -3605, -3615, -3626, -3636, -3647,
-3657, -3667, -3677, -3687, -3698, -3708, -3718, -3728, -3737, -3747,
-3757, -3767, -3776, -3786, -3795, -3805, -3814, -3824, -3833, -3842,
-3851, -3860, -3869, -3878, -3887, -3896, -3905, -3914, -3922, -3931,
-3940, -3948, -3957, -3965, -3973, -3982, -3990, -3998, -4006, -4014,
-4022, -4030, -4038, -4045, -4053, -4061, -4068, -4076, -4083, -4091,
-4098, -4105, -4113, -4120, -4127, -4134, -4141, -4148, -4154, -4161,
-4168, -4174, -4181, -4188, -4194, -4200, -4207, -4213, -4219, -4225,
-4231, -4237, -4243, -4249, -4255, -4260, -4266, -4272, -4277, -4283,
-4288, -4293, -4299, -4304, -4309, -4314, -4319, -4324, -4329, -4333,
-4338, -4343, -4347, -4352, -4356, -4361, -4365, -4369, -4374, -4378,
-4382, -4386, -4390, -4393, -4397, -4401, -4405, -4408, -4412, -4415,
-4418, -4422, -4425, -4428, -4431, -4434, -4437, -4440, -4443, -4446,
-4448, -4451, -4453, -4456, -4458, -4461, -4463, -4465, -4467, -4469,
-4471, -4473, -4475, -4477, -4479, -4480, -4482, -4483, -4485, -4486,
-4487, -4489, -4490, -4491, -4492, -4493, -4494, -4495, -4495, -4496,
-4497, -4497, -4498, -4498, -4498, -4499, -4499, -4499, -4499, -4499,
-4499, -4499, -4499, -4498, -4498, -4497, -4497, -4496, -4496, -4495,
-4494, -4493, -4492, -4491, -4490, -4489, -4488, -4487, -4485, -4484,
-4483, -4481, -4479, -4478, -4476, -4474, -4472, -4470, -4468, -4466,
-4464, -4462, -4459, -4457, -4455, -4452, -4450, -4447, -4444, -4441,
-4439, -4436, -4433, -4430, -4426, -4423, -4420, -4417, -4413, -4410,
-4406, -4403, -4399, -4395, -4391, -4388, -4384, -4380, -4376, -4371,
-4367, -4363, -4359, -4354, -4350, -4345, -4341, -4336, -4331, -4326,
-4321, -4316, -4311, -4306, -4301, -4296, -4291, -4285, -4280, -4274,
-4269, -4263, -4258, -4252, -4246, -4240, -4234, -4228, -4222, -4216,
-4210, -4203, -4197, -4191, -4184, -4178, -4171, -4165, -4158, -4151,
-4144, -4137, -4130, -4123, -4116, -4109, -4102, -4094, -4087, -4080,
-4072, -4065, -4057, -4049, -4042, -4034, -4026, -4018, -4010, -4002,
-3994, -3986, -3977, -3969, -3961, -3952, -3944, -3935, -3927, -3918,
-3909, -3901, -3892, -3883, -3874, -3865, -3856, -3847, -3838, -3828,
-3819, -3810, -3800, -3791, -3781, -3771, -3762, -3752, -3742, -3732,
-3723, -3713, -3703, -3693, -3682, -3672, -3662, -3652, -3641, -3631,
-3620, -3610, -3599, -3589, -3578, -3567, -3556, -3546, -3535, -3524,
-3513, -3502, -3491, -3479, -3468, -3457, -3445, -3434, -3423, -3411,
-3400, -3388, -3376, -3365, -3353, -3341, -3329, -3317, -3305, -3293,
-3281, -3269, -3257, -3245, -3232, -3220, -3208, -3195, -3183, -3170,
-3158, -3145, -3132, -3120, -3107, -3094, -3081, -3068, -3055, -3042,
-3029, -3016, -3003, -2990, -2977, -2963, -2950, -2937, -2923, -2910,
-2896, -2883, -2869, -2856, -2842, -2828, -2814, -2801, -2787, -2773,
-2759, -2745, -2731, -2717, -2703, -2689, -2674, -2660, -2646, -2632,
-2617, -2603, -2588, -2574, -2559, -2545, -2530, -2516, -2501, -2486,
-2471, -2457, -2442, -2427, -2412, -2397, -2382, -2367, -2352, -2337,
-2322, -2307, -2291, -2276, -2261, -2246, -2230, -2215, -2200, -2184,
-2169, -2153, -2138, -2122, -2106, -2091, -2075, -2059, -2044, -2028,
-2012, -1996, -1980, -1965, -1949, -1933, -1917, -1901, -1885, -1869,
-1852, -1836, -1820, -1804, -1788, -1772, -1755, -1739, -1723, -1706,
-1690, -1674, -1657, -1641, -1624, -1608, -1591, -1575, -1558, -1542,
-1525, -1508, -1492, -1475, -1458, -1441, -1425, -1408, -1391, -1374,
-1357, -1341, -1324, -1307, -1290, -1273, -1256, -1239, -1222, -1205,
-1188, -1171, -1154, -1137, -1120, -1102, -1085, -1068, -1051, -1034,
-1017, -999, -982, -965, -947, -930, -913, -896, -878, -861,
-844, -826, -809, -791, -774, -757, -739, -722, -704, -687,
-669, -652, -634, -617, -599, -582, -564, -547, -529, -512,
-494, -476, -459, -441, -424, -406, -388, -371, -353, -336,
-318, -300, -283, -265, -247, -230, -212, -194, -177, -159,
-141, -124, -106, -88, -71, -53, -35, -18, 0,
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
	
	int16_t s16AngleOffset;
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
	int16_t s16AngleOffset;
	uint32_t u32DutyZoomTemp;
}g_sMotionControl;


volatile uint32_t u32Test = 0;
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) 
{	
	for (uint8_t i = 0; i < 4; i++)
	{
		if (htim == g_sMotorControl.sMotor[i].pActivePolarity->htim &&
			htim->Channel == g_sMotorControl.sMotor[i].pActivePolarity->u32ActiveChannel)
		{
			u32Test = g_sMotorControl.sMotor[1].u32Index >> 15;
//			// 停掉当前激活的通道			
			if ((g_sMotorControl.sMotor[i].u32Index >> 15) >= 1600)
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
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom / g_sMotionControl.u32DutyZoomTemp) >> 15;
				
				int16_t s16DutyHandler = (int16_t)g_sMotorControl.sMotor[i].u16Duty + g_sMotorControl.sMotor[i].s16Offset;
				
				bool bChangeDir = false;
				
				if (s16DutyHandler >= 0)
				{
					g_sMotorControl.sMotor[i].u16Duty = s16DutyHandler;
					if (g_sMotorControl.sMotor[i].u16Duty > 4499)
					{
						g_sMotorControl.sMotor[i].u16Duty = 4499;
					}
					if (g_sMotorControl.sMotor[i].pActivePolarity != &g_sMotorControl.sMotor[i].sPolarityP)
					{
						HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
						__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 0);
						
						g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
						bChangeDir = true;
					}
				}
				else
				{
					g_sMotorControl.sMotor[i].u16Duty = -s16DutyHandler;
					if (g_sMotorControl.sMotor[i].u16Duty > 4499)
					{
						g_sMotorControl.sMotor[i].u16Duty = 4499;
					}
					if (g_sMotorControl.sMotor[i].pActivePolarity != &g_sMotorControl.sMotor[i].sPolarityN)
					{
						HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
						__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 0);
						
						g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityN;
						bChangeDir = true;
					}					
				}
				
//				if (g_sMotorControl.sMotor[i].pActivePolarity == &g_sMotorControl.sMotor[i].sPolarityP)
//				{
//					if (g_sMotorControl.sMotor[i].s16Offset > 0)
//					{
//						g_sMotorControl.sMotor[i].u16Duty += g_sMotorControl.sMotor[i].s16Offset;
//					}
//					else if (g_sMotorControl.sMotor[i].s16Offset < 0)
//					{
//						int16_t s16OffsetDuty = (int16_t)g_sMotorControl.sMotor[i].u16Duty + g_sMotorControl.sMotor[i].s16Offset;
//						if (s16OffsetDuty < 0)
//						{
//							g_sMotorControl.sMotor[i].u16Duty = 0;
//						}
//						else
//						{
//							g_sMotorControl.sMotor[i].u16Duty = s16OffsetDuty;
//						}
//					}
//				}
//				else
//				{
//					if (g_sMotorControl.sMotor[i].s16Offset > 0)
//					{
//						int16_t s16OffsetDuty = (int16_t)g_sMotorControl.sMotor[i].u16Duty - g_sMotorControl.sMotor[i].s16Offset;
//						if (s16OffsetDuty < 0)
//						{
//							g_sMotorControl.sMotor[i].u16Duty = 0;
//						}
//						else
//						{
//							g_sMotorControl.sMotor[i].u16Duty = s16OffsetDuty;
//						}
//						
//					}
//					else if (g_sMotorControl.sMotor[i].s16Offset < 0)
//					{
//						g_sMotorControl.sMotor[i].u16Duty = (int16_t)g_sMotorControl.sMotor[i].u16Duty - g_sMotorControl.sMotor[i].s16Offset;
//					}
//				}
				
				

				
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
										g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
										g_sMotorControl.sMotor[i].u16Duty); 
										
				if (bChangeDir)
				{
					HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
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
			u16CmpValue = g_s16DutyTable1Cycle[u32Index];
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

uint32_t u32MoveOffset0 = 0;
uint32_t u32MoveOffset1 = 0;
uint32_t u32MoveOffset2 = 0;
uint32_t u32MoveOffset3 = 0;


void StopPWM_IT(void)
{
	g_sMotionControl.u32DutyZoomTemp = 1;
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
}


uint32_t u32UpOffsetTest = 600;
// 向上运动
static bool MoveDownMode(void)
//static bool MoveUpMode(void)
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
			u32MoveOffset0 = 0;
			u32MoveOffset3 = 0;
//			u32MoveOffset1 = 500;
//			u32MoveOffset2 = 150;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
			g_sMotorControl.sMotor[0].u32Index = u32MoveOffset0 * 32768;
			g_sMotorControl.sMotor[1].u32Index = u32MoveOffset1 * 32768;
			g_sMotorControl.sMotor[2].u32Index = u32MoveOffset2 * 32768;
			g_sMotorControl.sMotor[3].u32Index = u32MoveOffset3 * 32768;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
//				g_sMotorControl.sMotor[i].u32Index += g_sMotorControl.sMotor[i].u32Span;	
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
//				if (g_sMotorControl.sMotor[1].pActivePolarity == &g_sMotorControl.sMotor[1].sPolarityP)
//				{
//					g_sMotorControl.sMotor[1].u16DirChangeCount++;
//					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityN;
//				}
//				else
//				{
//					g_sMotorControl.sMotor[1].u16DirChangeCount++;
//					g_sMotorControl.sMotor[1].pActivePolarity = &g_sMotorControl.sMotor[1].sPolarityP;
//				}
//				
//				if (g_sMotorControl.sMotor[2].pActivePolarity == &g_sMotorControl.sMotor[2].sPolarityP)
//				{
//					g_sMotorControl.sMotor[2].u16DirChangeCount++;
//					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityN;
//				}
//				else
//				{
//					g_sMotorControl.sMotor[2].u16DirChangeCount++;
//					g_sMotorControl.sMotor[2].pActivePolarity = &g_sMotorControl.sMotor[2].sPolarityP;
//				}
//				
//				g_sMotorControl.sMotor[1].u32Index = 0;
//				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[1].pActivePolarity->htim, g_sMotorControl.sMotor[1].pActivePolarity->u32CmpChannel);
//				g_sMotorControl.sMotor[1].bStartFlag = true;
//				
//				g_sMotorControl.sMotor[2].u32Index = 0;
//				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[2].pActivePolarity->u32CmpChannel);
//				g_sMotorControl.sMotor[2].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
//				if (g_sMotorControl.sMotor[0].pActivePolarity == &g_sMotorControl.sMotor[0].sPolarityP)
//				{
//					g_sMotorControl.sMotor[0].u16DirChangeCount++;
//					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityN;
//				}
//				else
//				{
//					g_sMotorControl.sMotor[0].u16DirChangeCount++;
//					g_sMotorControl.sMotor[0].pActivePolarity = &g_sMotorControl.sMotor[0].sPolarityP;
//				}
//				
//				if (g_sMotorControl.sMotor[3].pActivePolarity == &g_sMotorControl.sMotor[3].sPolarityP)
//				{
//					g_sMotorControl.sMotor[3].u16DirChangeCount++;
//					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityN;
//				}
//				else
//				{
//					g_sMotorControl.sMotor[3].u16DirChangeCount++;
//					g_sMotorControl.sMotor[3].pActivePolarity = &g_sMotorControl.sMotor[3].sPolarityP;
//				}
//				
//				g_sMotorControl.sMotor[0].u32Index = 0;
//				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[0].pActivePolarity->htim, g_sMotorControl.sMotor[0].pActivePolarity->u32CmpChannel);
//				g_sMotorControl.sMotor[0].bStartFlag = true;
//				
//				g_sMotorControl.sMotor[3].u32Index = 0;
//				HAL_TIM_PWM_Start_IT(g_sMotorControl.sMotor[2].pActivePolarity->htim, g_sMotorControl.sMotor[3].pActivePolarity->u32CmpChannel);
//				g_sMotorControl.sMotor[3].bStartFlag = true;
				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
//			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
//				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				g_sMotionControl.u8FSMStep = INIT_MOTION;
			}
//			else
//			{
//				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
//			}
			break;
			
		default:
			break;			
	}
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		
		StopPWM_IT();
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}

uint32_t u32DownOffsetTest = 600;


int32_t s32Phase = 0;
// 向下运动
static bool MoveUpMode(void)
//static bool MoveDownMode(void)
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
			u32MoveOffset0 = 400 - (g_sMotionControl.s16AngleOffset / 90.0f) * 400.0f + s32Phase;
			u32MoveOffset3 = 400 + (g_sMotionControl.s16AngleOffset / 90.0f) * 400.0f + s32Phase;
			u32MoveOffset1 = 0;
			u32MoveOffset2 = 0;
			break;
		
		case INIT_MOTION:	
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
			g_sMotorControl.sMotor[0].u32Index = u32MoveOffset0 * 32768;
			g_sMotorControl.sMotor[1].u32Index = u32MoveOffset1 * 32768;
			g_sMotorControl.sMotor[2].u32Index = u32MoveOffset2 * 32768;
			g_sMotorControl.sMotor[3].u32Index = u32MoveOffset3 * 32768;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityN;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
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
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[1].bStartFlag && !g_sMotorControl.sMotor[2].bStartFlag)
			{				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
//			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
//				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				
				g_sMotionControl.u8FSMStep = INIT_MOTION;
			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{		
		StopPWM_IT();
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}

uint32_t u32LeftOffsetTest = 400;
// 向左运动
//static bool MoveRightMode(void)
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
			g_sMotorControl.sMotor[2].u32Index = u32LeftOffsetTest * 32768;
			g_sMotorControl.sMotor[3].u32Index = u32LeftOffsetTest * 32768;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 	
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
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[1].bStartFlag && !g_sMotorControl.sMotor[0].bStartFlag)
			{
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
//			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
//				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				g_sMotionControl.u8FSMStep = INIT_MOTION;
			}
//			else
//			{
//				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
//			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		StopPWM_IT();
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}

uint32_t u32RightOffsetTest = 400;
uint32_t u32DelayTest = 0;
uint32_t u32DivTemp = 4;
// 向右运动
//static bool MoveLeftMode(void)
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
	
	
	static uint8_t u8Group10 = 0;
	static uint8_t u8Group11 = 1;
	static uint8_t u8Group20 = 2;
	static uint8_t u8Group21 = 3;
	
	static uint32_t u32GroupSelect = 0;
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			u32GroupSelect = 0;
			u32MoveOffset0 = 0;
			u32MoveOffset3 = 400 + (g_sMotionControl.s16AngleOffset / 90.0f) * 400.0f + s32Phase;
			u32MoveOffset1 = 0;
			u32MoveOffset2 = 400 - (g_sMotionControl.s16AngleOffset / 90.0f) * 400.0f + s32Phase;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
			// 超前相位
			if (u32GroupSelect % 2 == 0)
			{
				u8Group10 = 2;
				u8Group11 = 3;
				u8Group20 = 0;
				u8Group21 = 1;
				
				g_sMotionControl.u32DutyZoomTemp = 1;
			}
			else
			{
				u8Group10 = 0;
				u8Group11 = 1;
				u8Group20 = 2;
				u8Group21 = 3;
				
				g_sMotionControl.u32DutyZoomTemp = u32DivTemp;
			}
			
			g_sMotorControl.sMotor[u8Group10].u32Index = u32MoveOffset2 * 32768;
			g_sMotorControl.sMotor[u8Group11].u32Index = u32MoveOffset3 * 32768;
			g_sMotorControl.sMotor[u8Group20].u32Index = 0;
			g_sMotorControl.sMotor[u8Group21].u32Index = 0;
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom / g_sMotionControl.u32DutyZoomTemp) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
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
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[1].bStartFlag)
			{
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
			for (uint8_t i = 0; i < 4; i++)
			{
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
			}
			//u32GroupSelect++;
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		StopPWM_IT();
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}


// 聚拢
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
	
	static uint32_t u32TimeWait = 0;
	
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
				g_sMotorControl.sMotor[i].u32Index = 800 * 32768;
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityN;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 	
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
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
				ResetTimerCount(&u32TimeWait);
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
				
			}
			break;
			
		case CHECK_DIR_COUNT:
//			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
//				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				
				if (GetTimerTickDelta(u32TimeWait, GetCurTimerCount()) >= 2)
				{
					g_sMotionControl.u8FSMStep = INIT_MOTION;
				}
				
			}
//			else
//			{
//				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
//			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		StopPWM_IT();
		
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
		INIT_MOTION,
		PREPARE_MOTION,
		WAIT_MOTION1GROUP_STOP,
		WAIT_MOTION2GROUP_STOP,
		CHECK_DIR_COUNT,
	};
	
	static uint32_t u32TimeWait = 0;
	
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
				g_sMotorControl.sMotor[i].u32Index = 0;
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 	
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
//			if (!g_sMotorControl.sMotor[0].bStartFlag && !g_sMotorControl.sMotor[1].bStartFlag &&
//				!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
		
			if ((g_sMotorControl.sMotor[0].u32Index >> 15) >= 800 && (g_sMotorControl.sMotor[1].u32Index >> 15) >= 800 &&
				(g_sMotorControl.sMotor[2].u32Index >> 15) >= 800 && (g_sMotorControl.sMotor[3].u32Index >> 15) >= 800)
			{				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
				ResetTimerCount(&u32TimeWait);
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[2].bStartFlag && !g_sMotorControl.sMotor[3].bStartFlag)
			{
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
				
			}
			break;
			
		case CHECK_DIR_COUNT:
//			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
//				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				
				if (GetTimerTickDelta(u32TimeWait, GetCurTimerCount()) >= 2)
				{
					g_sMotionControl.u8FSMStep = INIT_MOTION;
				}
				
			}
//			else
//			{
//				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
//			}
			break;
			
		default:
			break;			
	}
	
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		StopPWM_IT();
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}


uint32_t u32HGatherOffsetTest = 400;
// 水平居中
static bool MoveHGatherMode(void)
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
	
	static uint8_t u8Group10 = 0;
	static uint8_t u8Group11 = 1;
	static uint8_t u8Group20 = 2;
	static uint8_t u8Group21 = 3;
	
	static uint32_t u32GroupSelect = 0;
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			u32GroupSelect = 0;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
		
			// 超前相位
			if (u32GroupSelect % 2 == 0)
			{
				u8Group10 = 0;
				u8Group11 = 3;
				u8Group20 = 1;
				u8Group21 = 2;
			}
			else
			{
				u8Group10 = 1;
				u8Group11 = 2;
				u8Group20 = 0;
				u8Group21 = 3;
			}
			
			g_sMotorControl.sMotor[u8Group10].u32Index = u32HGatherOffsetTest * 32768;
			g_sMotorControl.sMotor[u8Group11].u32Index = u32HGatherOffsetTest * 32768;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityN;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
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
			if (!g_sMotorControl.sMotor[u8Group10].bStartFlag && !g_sMotorControl.sMotor[u8Group11].bStartFlag)
			{				
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[u8Group20].bStartFlag && !g_sMotorControl.sMotor[u8Group21].bStartFlag)
			{				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
//			if (g_sMotorControl.sMotor[0].u16DirChangeCount == 2 && g_sMotorControl.sMotor[1].u16DirChangeCount == 2 &&
//				g_sMotorControl.sMotor[2].u16DirChangeCount == 2 && g_sMotorControl.sMotor[3].u16DirChangeCount == 2)
			{
				for (uint8_t i = 0; i < 4; i++)
				{
					HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
				}
				g_sMotionControl.u8FSMStep = INIT_MOTION;
				u32GroupSelect++;
			}
//			else
//			{
//				g_sMotionControl.u8FSMStep = WAIT_MOTION1GROUP_STOP;
//			}
			break;
			
		default:
			break;			
	}
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		
		StopPWM_IT();
		
		CbMotionFinish(g_sMotionControl.u8Index);
		MotionEndHandler();
		bRet = true;
	}
	
		
	return bRet;
}


uint32_t u32VGatherOffsetTest = 400;
// 垂直居中
static bool MoveVGatherMode(void)
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
	
	static uint8_t u8Group10 = 0;
	static uint8_t u8Group11 = 1;
	static uint8_t u8Group20 = 2;
	static uint8_t u8Group21 = 3;
	
	static uint32_t u32GroupSelect = 0;
	
	switch (g_sMotionControl.u8FSMStep)
	{
		case START_TIME:
			ResetTimerCount(&g_sMotionControl.u32TimerCount);
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			u32GroupSelect = 0;
			break;
		
		case INIT_MOTION:
			g_sMotionControl.u8FSMStep = PREPARE_MOTION;	
		
			// 超前相位
			if (u32GroupSelect % 2 == 0)
			{
				u8Group10 = 2;
				u8Group11 = 3;
				u8Group20 = 0;
				u8Group21 = 1;
//				u32VGatherOffsetTest = 400;
//				u32VGatherOffsetTest = 400;
			}
			else
			{
				u8Group10 = 0;
				u8Group11 = 1;
				u8Group20 = 2;
				u8Group21 = 3;
//				u32VGatherOffsetTest = 400;
//				u32VGatherOffsetTest = 400;
			}
			
			g_sMotorControl.sMotor[u8Group10].u32Index = u32VGatherOffsetTest * 32768;
			g_sMotorControl.sMotor[u8Group11].u32Index = u32VGatherOffsetTest * 32768;
			g_sMotorControl.sMotor[u8Group20].u32Index = 0;
			g_sMotorControl.sMotor[u8Group21].u32Index = 0;
		
			for (uint8_t i = 0; i < 4; i++)
			{
				g_sMotorControl.sMotor[i].pActivePolarity = &g_sMotorControl.sMotor[i].sPolarityP;
			
				g_sMotorControl.sMotor[i].u16Duty = (g_s16DutyTable1Cycle[g_sMotorControl.sMotor[i].u32Index >> 15] * 
					g_sMotorControl.sMotor[i].u32DutyZoom) >> 15;
				__HAL_TIM_SET_COMPARE(g_sMotorControl.sMotor[i].pActivePolarity->htim, 
											g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel, 
											g_sMotorControl.sMotor[i].u16Duty); 
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
			if (!g_sMotorControl.sMotor[u8Group10].bStartFlag && !g_sMotorControl.sMotor[u8Group11].bStartFlag)
			{				
				g_sMotionControl.u8FSMStep = WAIT_MOTION2GROUP_STOP;
			}
			break;
			
		case WAIT_MOTION2GROUP_STOP:
			if (!g_sMotorControl.sMotor[u8Group20].bStartFlag && !g_sMotorControl.sMotor[u8Group21].bStartFlag)
			{				
				g_sMotionControl.u8FSMStep = CHECK_DIR_COUNT;
			}
			break;
			
		case CHECK_DIR_COUNT:
			for (uint8_t i = 0; i < 4; i++)
			{
				HAL_TIM_PWM_Stop_IT(g_sMotorControl.sMotor[i].pActivePolarity->htim, g_sMotorControl.sMotor[i].pActivePolarity->u32CmpChannel);
			}
			g_sMotionControl.u8FSMStep = INIT_MOTION;
			u32GroupSelect++;
			break;
			
		default:
			break;			
	}
	
	if (GetTimerTickDelta(g_sMotionControl.u32TimerCount, GetCurTimerCount()) >= g_sMotionControl.u32TimeUseExpect)
	{
		StopPWM_IT();
		
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
			g_sMotionControl.pControlFunc = MoveHGatherMode;
			break;
		
		case MOTION_V_GATHER:
			g_sMotionControl.pControlFunc = MoveVGatherMode;
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
float fM1Coeff = 1.0f;
float fM2Coeff = 1.0f;
float fM3Coeff = 1.0f;
static void PrepareMotion(const MotorMotionParams *pParams)
{
	const float fPulseCycle = 0.0000625f;				// 1周期pwm用时
	const uint32_t fPWMCount10HzCycle = 1600;		// 1周期振动下PWM总数
	
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
	g_sMotorControl.sMotor[1].u32DutyZoom = g_sMotorControl.sMotor[1].u32DutyZoom * fM1Coeff;
	g_sMotorControl.sMotor[2].u32DutyZoom = g_sMotorControl.sMotor[2].u32DutyZoom * fM2Coeff;
	g_sMotorControl.sMotor[3].u32DutyZoom = g_sMotorControl.sMotor[3].u32DutyZoom * fM3Coeff;
	
	g_sMotionControl.s16AngleOffset = pParams->s16AngleOffset;
	
	// 16kHz的PWM, 10Hz振动频率时, 一个周期包含1600个PWM周期, 以此为基准
	float fFrequency = pParams->fFrequency;
	if (fFrequency > 200.0f)
	{
		fFrequency = 200.0f;
	}
	
	// 计算查表跨度, 需要得到的是1周期的PWM总数, 进而得到索引每次需要增加多少
	uint16_t u16PWMCount = (uint16_t)(1.0f / fFrequency / fPulseCycle);
	
	for (uint8_t i = 0; i < 4; i++)
	{
		g_sMotorControl.sMotor[i].u32Span = (uint32_t)(fPWMCount10HzCycle * 32768.0f / u16PWMCount);
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


