#include "FlexControl.h"
#include "SerialHandle.h"
#include "MotorControl.h"
#include "Network.h"
#include "Timer.h"

// 网络初始化失败后的重试间隔 ms
// 注意: 每次重试内部硬复位 W5500 会阻塞约 60ms, 电机脉冲由定时器中断驱动
// 不受影响, 但若现场对主循环实时性敏感可加大此间隔
#define NETWORK_RETRY_INTERVAL_MS	5000

// 网络初始化结果标志: false = W5500 未就绪(缺芯片/SPI 异常/版本号不符)
// 供其他模块查询(如上位机应答中上报网络状态)
bool g_bNetworkInitSuccess = false;

void HeartBeatHandler(void)
{
	// 将心跳信息反馈给上位机
	SendSerialRespond(RESPOND_HEART, NULL);
}


void MotorMotionHandler(const SerialInfo *pInfo)
{
	// 将运动信息加入到运动序列
	MotorMotionParams sParams;
	sParams.eMotionIndex = (MotionIndex)pInfo->u8MotionIndex;
	sParams.fVoltage = pInfo->fMotionVoltage;
	sParams.fFrequency = pInfo->fMotionFrequency;
	sParams.fTimeUse = pInfo->fMotionTimeUse;
	sParams.s16AngleOffset = pInfo->s16AngleOffset;
	sParams.u8SpeedCoeff = pInfo->u8SpeedCoeff;
	
	SendMotorInfo(&sParams);
}


// 运动完成处理回调
void CbMotionFinish(const uint8_t u8Index)
{
	// 将运动完成消息发送给上位机
	SendSerialRespond(RESPOND_MOTION_FINISH, (void *)&u8Index);
}


void CbMotionStopFinish(void)
{
	SendSerialRespond(RESPOND_MOTION_STOP, NULL);
}


void SerialRecvInfoHandle(void)
{
	static SerialInfo sSerialInfo;
	
	// 获取串口消息
	if (GetSerialInfo(&sSerialInfo))
	{
		switch (sSerialInfo.eInfo)
		{
			case INFO_HEART:
				SetNormal(true);
				// 发送心跳给上位机
				HeartBeatHandler();
				break;
			
			case INFO_MOTION:
				MotorMotionHandler(&sSerialInfo);
				break;
			
			case INFO_STOP_MOTION:
				MotorControlStop();
				break;
			
			default:
				break;
		}
	}
}


// 网络初始化重试: 初始化失败时按固定间隔重试, 成功后不再进入
// 场景: 上电时 W5500 未上电完成/网线未插/晶振未起振等导致首次复位校验失败
static void NetworkRetryHandle(void)
{
	static uint32_t s_u32RetryTimer;

	// 已初始化成功, 无需重试
	if (g_bNetworkInitSuccess)
	{
		return;
	}

	if (GetTimerTickDelta(s_u32RetryTimer, GetCurTimerCount()) < NETWORK_RETRY_INTERVAL_MS)
	{
		return;
	}
	ResetTimerCount(&s_u32RetryTimer);

	// NetworkInit 内部会清空全部状态, 重复调用安全
	g_bNetworkInitSuccess = NetworkInit();
}


void FlexControlInit(void)
{
	SerialHandleInit();
	MotionControlInit();

	// 首次初始化网络, 失败不阻塞其余模块, 由 NetworkRetryHandle 定期重试
	g_bNetworkInitSuccess = NetworkInit();
}


void FlexControlHandle(void)
{
	SerialRecvInfoHandle();
	SerialDataHandle();

	// 未初始化成功时内部只做计时, 不访问 W5500, 不占用 SPI 总线
	NetworkRetryHandle();

	// 初始化成功后才运行网络状态机, 避免 W5500 不在位时每周期空跑 SPI 读
	if (g_bNetworkInitSuccess)
	{
		NetworkHandle();
	}

	MotorControl();
}



