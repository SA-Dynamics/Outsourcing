#include "MotorControl.h"
#include "Timer.h"

struct
{
	uint32_t u32ConnectiveDetectTimer;
}g_sMotorControl;


void SetNormal(const bool bNormal)
{
	// 正常就更新计数器
	if (bNormal)
	{
		ResetTimerCount(&g_sMotorControl.u32ConnectiveDetectTimer);
	}
}


