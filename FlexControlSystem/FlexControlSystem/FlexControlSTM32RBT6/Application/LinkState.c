#include "LinkState.h"
#include "Timer.h"
#include "MotorControl.h"
#include "BinControl.h"
#include "LightControl.h"

/*
 * 链路(上位机)状态机
 *
 *   WAIT_CONNECT --收到心跳--> CONNECTED --心跳超时--> 停执行器 + 复位单片机
 *
 * 设计要点:
 * 1. 只有"收到过心跳"才进入 CONNECTED。上电后上位机没开、线没插都属于
 *    WAIT_CONNECT, 不做超时处理 —— 避免了"刚上电就自己重启"的死循环。
 * 2. 复位前先把振动盘/料仓/光源全部停掉: 现场常是通信线被扯断, 复位瞬间
 *    执行器还在振动不安全, 且下次上电也希望从静止开始。
 * 3. 复位用的是 NVIC_SystemReset(), 不复位外设/不复位备份域, 只把内核与
 *    外设寄存器恢复到上电态, 与手动按复位键等效。
 */

typedef struct
{
	LinkStateType eState;			// 当前状态
	uint32_t u32HeartBeatTimer;		// 最近一次心跳的时刻(ms)
	uint32_t u32LastInterval;		// 最近一次心跳间隔(ms), 供调试观察
	uint32_t u32TimeOutCount;		// 累计判定超时的次数
}LinkStateData;

static LinkStateData g_sLinkState;


/* 超时处理: 先停执行器, 再按开关决定复位还是退回等待连接 */
static void LinkStateEnterTimeOut(void)
{
	g_sLinkState.u32TimeOutCount++;

	// 停掉全部执行器(内部会向上位机补发停止应答, 复位后这些帧自然丢弃)
	MotorControlStop();
	BinControlStop();
	LightControlStop(LIGHT_INDEX_ALL);

	g_sLinkState.u32LastInterval = 0;
	g_sLinkState.eState = LINK_STATE_WAIT_CONNECT;
	ResetTimerCount(&g_sLinkState.u32HeartBeatTimer);

#if (LINK_RESET_ON_TIMEOUT)
	// 复位单片机: 回到上电初始态, 重新开始等待连接
	NVIC_SystemReset();
#endif
}


void LinkStateInit(void)
{
	g_sLinkState.eState = LINK_STATE_WAIT_CONNECT;
	g_sLinkState.u32LastInterval = 0;
	g_sLinkState.u32TimeOutCount = 0;
	ResetTimerCount(&g_sLinkState.u32HeartBeatTimer);
}


void LinkStateOnHeartBeat(void)
{
	switch (g_sLinkState.eState)
	{
		case LINK_STATE_WAIT_CONNECT:
			// 第一次收到心跳: 连接建立, 计时从此刻开始
			g_sLinkState.eState = LINK_STATE_CONNECTED;
			g_sLinkState.u32LastInterval = 0;
			ResetTimerCount(&g_sLinkState.u32HeartBeatTimer);
			break;

		case LINK_STATE_CONNECTED:
			// 记录本拍间隔(仅调试用), 然后刷新计时
			g_sLinkState.u32LastInterval = GetTimerTickDelta(g_sLinkState.u32HeartBeatTimer, GetCurTimerCount());
			ResetTimerCount(&g_sLinkState.u32HeartBeatTimer);
			break;

		default:
			// 状态异常时退回等待连接, 等下一帧心跳重新建立
			g_sLinkState.eState = LINK_STATE_WAIT_CONNECT;
			break;
	}
}


void LinkStateHandle(void)
{
	switch (g_sLinkState.eState)
	{
		case LINK_STATE_WAIT_CONNECT:
			// 等待连接: 无连接即无超时, 不做任何判断
			break;

		case LINK_STATE_CONNECTED:
			if (GetTimerTickDelta(g_sLinkState.u32HeartBeatTimer, GetCurTimerCount()) >= LINK_HEARTBEAT_TIMEOUT_MS)
			{
				LinkStateEnterTimeOut();
			}
			break;

		default:
			g_sLinkState.eState = LINK_STATE_WAIT_CONNECT;
			break;
	}
}


LinkStateType LinkStateGet(void)
{
	return g_sLinkState.eState;
}


uint32_t LinkStateGetHeartBeatInterval(void)
{
	return g_sLinkState.u32LastInterval;
}


uint32_t LinkStateGetTimeOutCount(void)
{
	return g_sLinkState.u32TimeOutCount;
}
