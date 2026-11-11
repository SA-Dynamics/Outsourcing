#include "FlexControl.h"
#include "SerialHandle.h"
#include "MotorControl.h"
#include "BinControl.h"
#include "LightControl.h"
#include "LinkState.h"
#include "Network.h"
#include "SaveParams.h"
#include "Timer.h"

// 网络初始化失败后的重试间隔 ms
// 注意: 每次重试内部硬复位 W5500 会阻塞约 60ms, 电机脉冲由定时器中断驱动
// 不受影响, 但若现场对主循环实时性敏感可加大此间隔
#define NETWORK_RETRY_INTERVAL_MS	5000

// 保存 IP 成功后延时复位 ms
// 留出时间让 0x10 应答帧从发送队列真正发到网上(主循环 1 个周期约 10ms),
// 复位太早会把应答截断, 上位机只能看到超时
#define SAVE_IP_REBOOT_DELAY_MS		500

// 网络初始化结果标志: false = W5500 未就绪(缺芯片/SPI 异常/版本号不符)
// 供其他模块查询(如上位机应答中上报网络状态)
bool g_bNetworkInitSuccess = false;

// 保存 IP 后的复位倒计时: 1 = 等待复位中
static uint8_t g_u8RebootPending = 0;
static uint32_t g_u32RebootTimer = 0;

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
	sParams.s8ModifyCoeff = pInfo->s8ModifyCoeff;
	
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


// 料仓运动入队处理
void BinMotionHandler(const SerialInfo *pInfo)
{
	BinMotionParams sParams;
	sParams.fVoltage = pInfo->fMotionVoltage;
	sParams.fFrequency = pInfo->fMotionFrequency;
	sParams.fTimeUse = pInfo->fMotionTimeUse;

	SendBinInfo(&sParams);
}


// 料仓运动完成回调
void CbBinMotionFinish(void)
{
	// 0x08: 索引固定为 1
	uint8_t u8Index = 0x01;
	SendSerialRespond(RESPOND_BIN_MOTION_FINISH, (void *)&u8Index);
}


// 料仓停止完成回调
void CbBinMotionStopFinish(void)
{
	SendSerialRespond(RESPOND_BIN_MOTION_STOP, NULL);
}


// 光源开启入队处理
void LightOnHandler(const SerialInfo *pInfo)
{
	LightParams sParams;
	sParams.u8Index = pInfo->u8LightIndex;
	sParams.u8Brightness = pInfo->u8LightBrightness;
	sParams.u16KeepTime = pInfo->u16LightKeepTime;

	SendLightInfo(&sParams);
}


// 光源开启完成回调: 必须在开启后 1s 内发出
void CbLightOnFinish(const uint8_t u8Index)
{
	SendSerialRespond(RESPOND_LIGHT_ON, (void *)&u8Index);
}


// 光源关闭完成回调
void CbLightOffFinish(const uint8_t u8Index)
{
	(void)u8Index;
	SendSerialRespond(RESPOND_LIGHT_OFF, NULL);
}


/**
 * @brief  保存上位机下发的新 IP 到 Flash, 并按结果回 0x10
 * @param  pInfo 解析结果, u8Ip[4] 为新 IP
 * @note   擦除 Flash 一页要 20~40ms, 期间 CPU 从 Flash 取指会停顿, 控制环
 *         被打断。所以先停掉振动盘/料仓/光源, 保证"停顿"发生时没有执行器
 *         在动; 保存 IP 属于配置操作, 现场本来就应该在停机状态做。
 *         成功后置复位倒计时, 由 FlexControlHandle 在应答帧发出后重启。
 */
static void SaveIpHandler(const SerialInfo *pInfo)
{
	uint8_t u8Result = 0;

	MotorControlStop();
	BinControlStop();
	LightControlStop(LIGHT_INDEX_ALL);

	if (SaveParamsSaveIp(pInfo->u8Ip) != false)
	{
		u8Result = 1;
		// 应答先入发送队列, 复位由主循环延时执行, 否则上位机收不到结果
		g_u8RebootPending = 1;
		ResetTimerCount(&g_u32RebootTimer);
	}

	SendSerialRespond(RESPOND_SAVE_IP, &u8Result);
}


/**
 * @brief  获取当前 IP, 回 0x12 + 4 字节
 * @note   返回 NetworkInit 实际写入 W5500 的值(Flash 有效时为 Flash 值,
 *         无效时为 Network.h 默认宏), 与上位机"保存IP"看到的行为一致。
 */
static void GetIpHandler(void)
{
	uint8_t u8Ip[4];

	NetworkGetIp(u8Ip);
	SendSerialRespond(RESPOND_GET_IP, u8Ip);
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
				// 心跳喂狗: 仅在收到心跳时才认为"连接正常"
				LinkStateOnHeartBeat();
				// 发送心跳给上位机
				HeartBeatHandler();
				break;
			
			case INFO_MOTION:
				MotorMotionHandler(&sSerialInfo);
				break;
			
			case INFO_STOP_MOTION:
				MotorControlStop();
				break;

			case INFO_BIN_MOTION:
				BinMotionHandler(&sSerialInfo);
				break;

			case INFO_STOP_BIN_MOTION:
				BinControlStop();
				break;

			case INFO_LIGHT_ON:
				LightOnHandler(&sSerialInfo);
				break;

			case INFO_LIGHT_OFF:
				// u8LightIndex = 0 关闭全部; 1/2 只关指定光源
				LightControlStop(sSerialInfo.u8LightIndex);
				break;

			case INFO_SAVE_IP:
				SaveIpHandler(&sSerialInfo);
				break;

			case INFO_GET_IP:
				GetIpHandler();
				break;

			default:
				break;
		}
	}
}


// 网络帧泵: 把 TCP 收到的完整帧送进与串口相同的 SerialInfo 解析队列,
// 后续分发完全复用 SerialRecvInfoHandle, 网络与串口指令行为一致
static void NetworkRecvInfoHandle(void)
{
	uint8_t u8Frame[NETWORK_FRAME_DATA_LEN];
	uint8_t u8Len;

	// 帧校验(帧头/帧尾/校验和)已在 Network.c 提取时完成, 这里只管搬运
	while (NetworkGetFrame(u8Frame, &u8Len))
	{
		SerialHandleNetworkFrame(u8Frame, u8Len);
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
	// 先读 Flash 参数: NetworkInit 要用里面保存的 IP
	SaveParamsInit();

	SerialHandleInit();
	LinkStateInit();
	MotionControlInit();
	BinControlInit();
	LightControlInit();

	// 首次初始化网络, 失败不阻塞其余模块, 由 NetworkRetryHandle 定期重试
	g_bNetworkInitSuccess = NetworkInit();
}


void FlexControlHandle(void)
{
	// 网络初始化重试: 成功后本周期即开始收发
	NetworkRetryHandle();

	// 网络帧先入解析队列, 与串口帧汇合后统一分发
	// 未初始化成功时内部只做计时, 不访问 W5500, 不占用 SPI 总线
	if (g_bNetworkInitSuccess)
	{
		NetworkHandle();
		NetworkRecvInfoHandle();
	}

	SerialRecvInfoHandle();
	SerialDataHandle();

	// 链路状态机: 等待连接 -> 已连接 -> 心跳超时(停执行器并复位)
	// 放在接收处理之后, 保证本轮收到的心跳已经喂过计时器
	LinkStateHandle();

	MotorControl();
	BinControl();
	LightControl();

	// 保存 IP 成功后的延时复位: 等应答帧真的发出去再重启,
	// 否则上位机只会看到"保存无应答"。复位后 W5500 重新初始化, 新 IP 生效。
	if (g_u8RebootPending != 0u)
	{
		if (GetTimerTickDelta(g_u32RebootTimer, GetCurTimerCount()) >= SAVE_IP_REBOOT_DELAY_MS)
		{
			NVIC_SystemReset();
		}
	}
}



