#ifndef LINKSTATE_H
#define LINKSTATE_H

#include "main.h"

/* ---- 心跳相关参数 ---------------------------------------------------------- */
/* 上位机(ConnectiveHandle)心跳发送周期为 1s, 这里取 3 个周期作为掉线判据:
   偶发丢 1~2 帧不会误复位, 真断线最多 3s 内动作 */
#define LINK_HEARTBEAT_TIMEOUT_MS		3000

/* 1 = 心跳超时后复位单片机(现场用法)
   0 = 超时只停执行器并回到"等待连接"状态(调试用, 方便挂仿真器观察) */
#define LINK_RESET_ON_TIMEOUT			1

/* ---- 状态 ------------------------------------------------------------------ */
typedef enum
{
	/* 等待连接: 上电后到收到第一帧心跳之前。此时没有"连接", 不做超时判断,
	   所以上位机没开/线没插时单片机只是安静等待, 不会反复复位 */
	LINK_STATE_WAIT_CONNECT = 0,

	/* 已连接: 收到过心跳, 每帧心跳刷新计时; 超过阈值没收到则判为掉线 */
	LINK_STATE_CONNECTED,
}LinkStateType;


void LinkStateInit(void);
void LinkStateHandle(void);

/* 收到一帧心跳时调用(解析层已通过校验) */
void LinkStateOnHeartBeat(void);

/* 查询当前状态 / 最近一次心跳间隔(ms, 0 表示还没测到) / 累计超时次数 */
LinkStateType LinkStateGet(void);
uint32_t LinkStateGetHeartBeatInterval(void);
uint32_t LinkStateGetTimeOutCount(void);

#endif
