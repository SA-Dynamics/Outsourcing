#include "FlexControl.h"
#include "SerialHandle.h"
#include "MotorControl.h"
#include "BinControl.h"
#include "LightControl.h"
#include "LinkState.h"
#include "Network.h"
#include "SaveParams.h"
#include "Timer.h"
#include "PowerMonitor.h"
#include "spi.h"

// 网络初始化失败后的重试间隔 ms
// 注意: 每次重试内部硬复位 W5500 会阻塞约 60ms, 电机脉冲由定时器中断驱动
// 不受影响, 但若现场对主循环实时性敏感可加大此间隔
#define NETWORK_RETRY_INTERVAL_MS	5000

/*
 * W5500 硬件绑定: SPI 句柄/CS/RST/INT 引脚只在这里出现一次,
 * Network.c 本身不认识任何具体引脚(它是可搬到其它项目的通用内核)。
 * 换 SPI 口或换板子只改这个结构体。
 */
static const NetworkHwConfig_t g_sNetworkHwCfg =
{
	.pSpi     = &hspi1,
	.pCsPort  = SPI_CS_GPIO_Port,
	.CsPin    = SPI_CS_Pin,
	.pRstPort = RSTN_GPIO_Port,
	.RstPin   = RSTN_Pin,
	.pIntPort = INTN_GPIO_Port,
	.IntPin   = INTN_Pin,
};

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

// 三个指示灯均为灌电流驱动: RESET 点亮, SET 熄灭。
#define RUNNING_LED_HALF_PERIOD_MS   1000u
#define COMM_LED_ON_TIME_MS          80u
#define COMM_LED_OFF_TIME_MS         80u

static struct
{
	uint32_t u32RunningTimer;
	uint32_t u32CommTimer;
	bool bRunningOn;
	bool bCommOn;
	bool bCommPending;
} g_sStatusLed;

static void StatusLedInit(void)
{
	const uint32_t u32Now = GetCurTimerCount();

	g_sStatusLed.u32RunningTimer = u32Now;
	g_sStatusLed.u32CommTimer = u32Now - COMM_LED_OFF_TIME_MS;
	g_sStatusLed.bRunningOn = true;
	g_sStatusLed.bCommOn = false;
	g_sStatusLed.bCommPending = false;
	HAL_GPIO_WritePin(RunningLED_GPIO_Port, RunningLED_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(CommLED_GPIO_Port, CommLED_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(MotionLED_GPIO_Port, MotionLED_Pin, GPIO_PIN_SET);
}

// 两个接收回调都由主循环调用, 不在 ISR 内操作GPIO或累计计时。
void CbSerialReceiveActivity(void)
{
	g_sStatusLed.bCommPending = true;
}

void CbNetworkReceiveActivity(void)
{
	g_sStatusLed.bCommPending = true;
}

static void StatusLedHandle(void)
{
	const uint32_t u32Now = GetCurTimerCount();
	const uint32_t u32HalfPeriods = (u32Now - g_sStatusLed.u32RunningTimer) / RUNNING_LED_HALF_PERIOD_MS;
	const bool bExecuting = MotorControlIsBusy() || BinControlIsBusy() || LightControlIsBusy();

	// 按经过的半周期更新, 不用delay/新中断; 无符号差值可跨HAL tick回绕。
	if (u32HalfPeriods != 0u)
	{
		g_sStatusLed.u32RunningTimer += u32HalfPeriods * RUNNING_LED_HALF_PERIOD_MS;
		if ((u32HalfPeriods & 1u) != 0u)
		{
			g_sStatusLed.bRunningOn = !g_sStatusLed.bRunningOn;
		}
		HAL_GPIO_WritePin(RunningLED_GPIO_Port, RunningLED_Pin,
			g_sStatusLed.bRunningOn ? GPIO_PIN_RESET : GPIO_PIN_SET);
	}

	// 接收密集时最多保留一个待闪事件, 且两次亮脉冲之间保证80ms灭灯。
	// 不在每包到来时延长亮灯时间, 避免心跳/持续流量把通信灯锁成常亮。
	if (g_sStatusLed.bCommOn)
	{
		if ((u32Now - g_sStatusLed.u32CommTimer) >= COMM_LED_ON_TIME_MS)
		{
			g_sStatusLed.bCommOn = false;
			g_sStatusLed.u32CommTimer = u32Now;
			HAL_GPIO_WritePin(CommLED_GPIO_Port, CommLED_Pin, GPIO_PIN_SET);
		}
	}
	else if (g_sStatusLed.bCommPending &&
			 (u32Now - g_sStatusLed.u32CommTimer) >= COMM_LED_OFF_TIME_MS)
	{
		g_sStatusLed.bCommPending = false;
		g_sStatusLed.bCommOn = true;
		g_sStatusLed.u32CommTimer = u32Now;
		HAL_GPIO_WritePin(CommLED_GPIO_Port, CommLED_Pin, GPIO_PIN_RESET);
	}

	// 振动盘/料仓包含排队和运动内等待; 光源保持期间(含常亮)也是执行中。
	HAL_GPIO_WritePin(MotionLED_GPIO_Port, MotionLED_Pin,
		bExecuting ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

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
	sParams.u8InfiniteTime = pInfo->u8InfiniteTime;
	
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
	sParams.u8InfiniteTime = pInfo->u8InfiniteTime;

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
	// "保持"勾选 = 无限保持。下位机本来就用"保持时间 0"表示常亮,
	// 这里把标志折算成 0, 不需要给 LightControl 增加新的状态位。
	sParams.u16KeepTime = pInfo->u8InfiniteTime ? 0 : pInfo->u16LightKeepTime;

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


// ADC 原始值 -> 电压(V) 系数: 待实测标定, 当前为预设值
// 换算: 电压(V) = ADC均值 / 4095 * VREF(3.3) * 分压比(11)
// 上传格式为 电压x10(协议 0x14), 即 ADC * COEFF * 10; 标定后只改这个宏
#define POWER_VOLTAGE_COEFF_X10		(11.0f * 3.3f)		// = 电压(V)/ADC ×1 预设值

/**
 * @brief  获取电压, 回 0x14 + 2 字节 电压x10
 * @note   取 PowerMonitor 的均值滤波结果; 尚无有效样本时上传 0,
 *         上位机连接后 2s 才发首帧, 正常情况下窗口已填满。
 */
static void GetVoltageHandler(void)
{
	float fVoltage = 0.0f;

	(void)PowerMonitorGetVoltage(&fVoltage);

	uint16_t u16VoltX10 = (uint16_t)(fVoltage * 10.0f + 0.5f);
	SendSerialRespond(RESPOND_GET_VOLTAGE, &u16VoltX10);
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
				// u8LightIndex = 0 关闭全部; 1/2/3/4 只关 A/B/C/D
				LightControlStop(sSerialInfo.u8LightIndex);
				break;

			case INFO_SAVE_IP:
				SaveIpHandler(&sSerialInfo);
				break;

			case INFO_GET_IP:
				GetIpHandler();
				break;

			case INFO_GET_VOLTAGE:
				GetVoltageHandler();
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
	g_bNetworkInitSuccess = NetworkInit(&g_sNetworkHwCfg);
}


void FlexControlInit(void)
{
	StatusLedInit();

	// 先读 Flash 参数: NetworkInit 要用里面保存的 IP
	SaveParamsInit();

	SerialHandleInit();
	LinkStateInit();
	MotionControlInit();
	BinControlInit();
	LightControlInit();
	PowerMonitorInit();

	// 首次初始化网络, 失败不阻塞其余模块, 由 NetworkRetryHandle 定期重试
	g_bNetworkInitSuccess = NetworkInit(&g_sNetworkHwCfg);
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

	// 电源电压采样: 状态机式启动/轮询, 每轮至多推进一步, 不阻塞
	PowerMonitorHandle();

	// 运行/通信/执行灯均在主循环刷新, 不新增中断。
	StatusLedHandle();

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



