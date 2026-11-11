#include "Network.h"
#include "W5500.h"
#include "SaveParams.h"
#include "spi.h"
#include "Timer.h"
#include <string.h>


// 发送缓冲区单帧最大长度
#define NETWORK_SEND_DATA_LEN		64

// 链路检测间隔 ms
#define NETWORK_LINK_CHECK_MS		500

// 帧头帧尾
#define FRAME_HEAD_0				0x55
#define FRAME_HEAD_1				0xAA
#define FRAME_TAIL_0				0xAA
#define FRAME_TAIL_1				0x55

/*
 * UDP 发现服务: 上位机不知道本机 IP 时, 经 UDP 广播 0x11 发现帧,
 * 本机收到后按广播源地址单播回 0x12 + 当前 IP(帧格式与 TCP 通道完全一致)。
 * 用独立的 Socket1 监听, 与 Socket0 的 TCP 服务互不影响; UDP 与 TCP 的
 * 5000 端口是两个独立命名空间, 可以同号。
 */
#define NETWORK_DISCOVERY_SOCKET	1
#define NETWORK_DISCOVERY_PORT		5000
// 发现 Socket 状态异常后的重开间隔 ms(避免每周期空跑 SPI)
#define NETWORK_DISCOVERY_REOPEN_MS	500

// 当前运行的 IP, NetworkInit 成功后更新; 初值为默认宏,
// 供"获取IP"(0x11) 在 W5500 未初始化时也能给出合理应答
static uint8_t g_au8CurrentIp[4] = NETWORK_LOCAL_IP;

// UDP 发现服务(定义在文件尾部, NetworkHandle 会调用, 此处前置声明)
static void NetworkDiscoveryHandle(void);


// 连接维护状态机
typedef enum
{
	NET_FSM_CHECK_LINK = 0,			// 检测网线连接
	NET_FSM_OPEN_SOCKET,			// 打开 socket
	NET_FSM_LISTEN,					// 监听
	NET_FSM_WAIT_CONNECT,			// 等待上位机连接
	NET_FSM_ESTABLISHED,			// 已连接, 收发数据
}NetFsmStep;

typedef struct
{
	NetFsmStep eFsmStep;

	// 接收环形缓冲
	struct
	{
		uint8_t u8Buffer[NETWORK_RECV_BUFFER_SIZE];
		uint16_t u16WriteIndex;
		uint16_t u16ReadIndex;
	}sRecv;

	// 收到的完整帧队列
	struct
	{
		uint8_t u8Data[NETWORK_FRAME_QUEUE_SIZE][NETWORK_FRAME_DATA_LEN];
		uint8_t u8Len[NETWORK_FRAME_QUEUE_SIZE];
		uint8_t u8WriteIndex;
		uint8_t u8ReadIndex;
		uint8_t u8BufferLen;
	}sFrame;

	// 待发送帧队列
	struct
	{
		uint8_t u8Data[NETWORK_SEND_QUEUE_SIZE][NETWORK_SEND_DATA_LEN];
		uint8_t u8Len[NETWORK_SEND_QUEUE_SIZE];
		uint8_t u8WriteIndex;
		uint8_t u8ReadIndex;
		uint8_t u8BufferLen;
	}sSend;

	bool bConnected;				// TCP 连接状态
	uint32_t u32WaitTimer;			// 状态机等待计时
}NetDataType;

// 网络通信实例
static NetDataType g_sNetData;


// ============================ 接收帧处理 ============================

// 将环形缓冲一段数据拷出(带回卷)
static void CopyRecvData(uint16_t u16Index, uint8_t *pDst, const uint16_t u16Len)
{
	for (uint16_t i = 0; i < u16Len; i++)
	{
		pDst[i] = g_sNetData.sRecv.u8Buffer[(u16Index + i) % NETWORK_RECV_BUFFER_SIZE];
	}
}

// 将解析完成的帧入队, 队列满时丢弃新帧
static void SaveFrameToQueue(const uint8_t *pData, const uint8_t u8Len)
{
	if (g_sNetData.sFrame.u8BufferLen < NETWORK_FRAME_QUEUE_SIZE)
	{
		memcpy(g_sNetData.sFrame.u8Data[g_sNetData.sFrame.u8WriteIndex], pData, u8Len);
		g_sNetData.sFrame.u8Len[g_sNetData.sFrame.u8WriteIndex] = u8Len;
		g_sNetData.sFrame.u8WriteIndex = (g_sNetData.sFrame.u8WriteIndex + 1) % NETWORK_FRAME_QUEUE_SIZE;
		g_sNetData.sFrame.u8BufferLen++;
	}
}


// 从环形缓冲中提取一帧, 找到完整帧返回 true
// 帧格式: 55 AA | 类型 | 长度 | 数据 | 校验高 | 校验低 | AA 55
static bool ExtractFrameFromRecv(void)
{
	const uint16_t u16Size = NETWORK_RECV_BUFFER_SIZE;
	uint16_t u16Head = g_sNetData.sRecv.u16ReadIndex;
	uint16_t u16Write = g_sNetData.sRecv.u16WriteIndex;

	// 缓冲区内已填充的字节数
	uint16_t u16Filled = (u16Write >= u16Head) ? (u16Write - u16Head) : (u16Write + u16Size - u16Head);
	if (u16Filled == 0)
	{
		return false;
	}

	// 查找帧头 0x55 0xAA
	bool bFound = false;
	uint16_t u16Offset = 0;
	while (u16Offset + 1 < u16Filled)
	{
		if (g_sNetData.sRecv.u8Buffer[(u16Head + u16Offset) % u16Size] == FRAME_HEAD_0 &&
			g_sNetData.sRecv.u8Buffer[(u16Head + u16Offset + 1) % u16Size] == FRAME_HEAD_1)
		{
			bFound = true;
			break;
		}
		u16Offset++;
	}

	if (!bFound)
	{
		// 没有帧头, 丢弃全部数据, 但保留最后一个字节(可能是被截断的帧头前半)
		g_sNetData.sRecv.u16ReadIndex = (u16Head + u16Filled - 1) % u16Size;
		return false;
	}

	// 丢弃帧头前的杂散字节
	u16Head = (u16Head + u16Offset) % u16Size;
	u16Filled -= u16Offset;

	// 长度字段尚未收到
	if (u16Filled < 4)
	{
		return false;
	}

	uint8_t u8DataSize = g_sNetData.sRecv.u8Buffer[(u16Head + 3) % u16Size];

	// 长度字段异常(超过单帧上限), 认为帧头是误码, 跳过当前帧头重新搜索
	if (u8DataSize + 4 > NETWORK_FRAME_DATA_LEN)
	{
		g_sNetData.sRecv.u16ReadIndex = (u16Head + 1) % u16Size;
		return false;
	}

	// 完整帧尚未接收完, 等待后续数据(读指针停在帧头)
	const uint16_t u16FrameLen = u8DataSize + 8;
	if (u16Filled < u16FrameLen)
	{
		return false;
	}

	// 校验帧尾
	uint16_t u16Tail = (u16Head + u8DataSize + 6) % u16Size;
	if (g_sNetData.sRecv.u8Buffer[u16Tail] != FRAME_TAIL_0 ||
		g_sNetData.sRecv.u8Buffer[(u16Tail + 1) % u16Size] != FRAME_TAIL_1)
	{
		g_sNetData.sRecv.u16ReadIndex = (u16Head + 1) % u16Size;
		return false;
	}

	// 校验和: 从类型字节开始累计 u8DataSize + 2 个字节, 反码加一
	uint16_t u16CheckSum = 0;
	for (uint8_t i = 0; i < u8DataSize + 2; i++)
	{
		u16CheckSum += g_sNetData.sRecv.u8Buffer[(u16Head + 2 + i) % u16Size];
	}
	u16CheckSum = ~u16CheckSum + 1;

	if ((u16CheckSum >> 8) != g_sNetData.sRecv.u8Buffer[(u16Head + 4 + u8DataSize) % u16Size] ||
		(u16CheckSum & 0xFF) != g_sNetData.sRecv.u8Buffer[(u16Head + 5 + u8DataSize) % u16Size])
	{
		// 校验失败, 丢弃整帧
		g_sNetData.sRecv.u16ReadIndex = (u16Tail + 2) % u16Size;
		return false;
	}

	// 提取有效数据(类型字节到校验低字节)入队
	uint8_t u8FrameLen = u8DataSize + 4;
	uint8_t u8Frame[NETWORK_FRAME_DATA_LEN] = {0};
	CopyRecvData((u16Head + 2) % u16Size, u8Frame, u8FrameLen);
	SaveFrameToQueue(u8Frame, u8FrameLen);

	// 读指针移动到下一帧
	g_sNetData.sRecv.u16ReadIndex = (u16Tail + 2) % u16Size;

	return true;
}


// 从 W5500 拉取数据并提取帧
static void NetworkRecvHandler(void)
{
	uint8_t u8Temp[64] = {0};

	// 有中断标志或轮询到数据, 都拉取一次
	int32_t s32Len = W5500_SocketRecv(NETWORK_SOCKET_INDEX, u8Temp, sizeof(u8Temp));

	// 将收到的数据放入环形缓冲
	for (int32_t i = 0; i < s32Len; i++)
	{
		uint16_t u16Next = (g_sNetData.sRecv.u16WriteIndex + 1) % NETWORK_RECV_BUFFER_SIZE;

		// 缓冲区满, 丢弃旧数据, 读指针前移
		if (u16Next == g_sNetData.sRecv.u16ReadIndex)
		{
			g_sNetData.sRecv.u16ReadIndex = (g_sNetData.sRecv.u16ReadIndex + 1) % NETWORK_RECV_BUFFER_SIZE;
		}

		g_sNetData.sRecv.u8Buffer[g_sNetData.sRecv.u16WriteIndex] = u8Temp[i];
		g_sNetData.sRecv.u16WriteIndex = u16Next;
	}

	// 提取所有完整帧
	while (ExtractFrameFromRecv());
}


// ============================ 发送处理 ============================

// 从发送队列取出一帧发出
static void NetworkSendHandler(void)
{
	if (g_sNetData.sSend.u8BufferLen == 0)
	{
		return;
	}

	int32_t s32Ret = W5500_SocketSend(NETWORK_SOCKET_INDEX,
		g_sNetData.sSend.u8Data[g_sNetData.sSend.u8ReadIndex],
		g_sNetData.sSend.u8Len[g_sNetData.sSend.u8ReadIndex]);

	// 发送成功才移出队列; 失败(发送缓冲不足超时)保留本帧, 下一周期重试
	if (s32Ret > 0)
	{
		g_sNetData.sSend.u8ReadIndex = (g_sNetData.sSend.u8ReadIndex + 1) % NETWORK_SEND_QUEUE_SIZE;
		g_sNetData.sSend.u8BufferLen--;
	}
}


bool NetworkSendData(const uint8_t *pData, const uint8_t u8Len)
{
	// 未连接或超长直接丢弃
	if (!g_sNetData.bConnected || pData == NULL || u8Len == 0 || u8Len > NETWORK_SEND_DATA_LEN)
	{
		return false;
	}

	if (g_sNetData.sSend.u8BufferLen >= NETWORK_SEND_QUEUE_SIZE)
	{
		return false;
	}

	memcpy(g_sNetData.sSend.u8Data[g_sNetData.sSend.u8WriteIndex], pData, u8Len);
	g_sNetData.sSend.u8Len[g_sNetData.sSend.u8WriteIndex] = u8Len;
	g_sNetData.sSend.u8WriteIndex = (g_sNetData.sSend.u8WriteIndex + 1) % NETWORK_SEND_QUEUE_SIZE;
	g_sNetData.sSend.u8BufferLen++;

	return true;
}


bool NetworkGetFrame(uint8_t *pBuf, uint8_t *pLen)
{
	bool bRet = false;

	if (g_sNetData.sFrame.u8BufferLen)
	{
		uint8_t u8Index = g_sNetData.sFrame.u8ReadIndex;
		memcpy(pBuf, g_sNetData.sFrame.u8Data[u8Index], g_sNetData.sFrame.u8Len[u8Index]);
		*pLen = g_sNetData.sFrame.u8Len[u8Index];

		g_sNetData.sFrame.u8ReadIndex = (g_sNetData.sFrame.u8ReadIndex + 1) % NETWORK_FRAME_QUEUE_SIZE;
		g_sNetData.sFrame.u8BufferLen--;
		bRet = true;
	}

	return bRet;
}


bool NetworkIsConnected(void)
{
	return g_sNetData.bConnected;
}


// ============================ 连接维护状态机 ============================

static void NetworkFsmHandler(void)
{
	switch (g_sNetData.eFsmStep)
	{
		case NET_FSM_CHECK_LINK:
			// 每 500ms 检测一次网线连接
			if (GetTimerTickDelta(g_sNetData.u32WaitTimer, GetCurTimerCount()) >= NETWORK_LINK_CHECK_MS)
			{
				ResetTimerCount(&g_sNetData.u32WaitTimer);

				if (W5500_IsLinked() != 0u)
				{
					g_sNetData.eFsmStep = NET_FSM_OPEN_SOCKET;
				}
			}
			break;

		case NET_FSM_OPEN_SOCKET:
			if (W5500_SocketOpen(NETWORK_SOCKET_INDEX, W5500_PROTO_TCP, NETWORK_TCP_PORT) == W5500_OK)
			{
				g_sNetData.eFsmStep = NET_FSM_LISTEN;
			}
			else
			{
				// 打开失败, 间隔后重试
				ResetTimerCount(&g_sNetData.u32WaitTimer);
				g_sNetData.eFsmStep = NET_FSM_CHECK_LINK;
			}
			break;

		case NET_FSM_LISTEN:
			if (W5500_SocketListen(NETWORK_SOCKET_INDEX) == W5500_OK)
			{
				g_sNetData.eFsmStep = NET_FSM_WAIT_CONNECT;
			}
			else
			{
				W5500_SocketClose(NETWORK_SOCKET_INDEX);
				ResetTimerCount(&g_sNetData.u32WaitTimer);
				g_sNetData.eFsmStep = NET_FSM_CHECK_LINK;
			}
			break;

		case NET_FSM_WAIT_CONNECT:
			if (W5500_SocketStatus(NETWORK_SOCKET_INDEX) == W5500_SR_ESTABLISHED)
			{
				g_sNetData.bConnected = true;
				// 进入连接后清一次残留中断标志
				(void)W5500_SocketClearIr(NETWORK_SOCKET_INDEX);
				W5500_ClearEvent(NETWORK_SOCKET_INDEX, 0xFFu);
				g_sNetData.eFsmStep = NET_FSM_ESTABLISHED;
			}
			else if (W5500_SocketStatus(NETWORK_SOCKET_INDEX) == W5500_SR_CLOSED)
			{
				g_sNetData.eFsmStep = NET_FSM_OPEN_SOCKET;
			}
			break;

		case NET_FSM_ESTABLISHED:
		{
			uint8_t u8Status = W5500_SocketStatus(NETWORK_SOCKET_INDEX);

			// 对端断开或链路断开, 重新走监听流程
			if (u8Status == W5500_SR_CLOSE_WAIT || u8Status == W5500_SR_CLOSED || (W5500_IsLinked() == 0u))
			{
				g_sNetData.bConnected = false;
				// 丢弃半包数据
				g_sNetData.sRecv.u16ReadIndex = g_sNetData.sRecv.u16WriteIndex;
				W5500_SocketClose(NETWORK_SOCKET_INDEX);
				g_sNetData.eFsmStep = NET_FSM_OPEN_SOCKET;
			}
			// Sn_IR 的清理由 NetworkHandle 顶部的 W5500_ProcessInt 统一完成,
			// 这里不再逐周期写寄存器, 减少无效 SPI 事务
			break;
		}

		default:
			g_sNetData.eFsmStep = NET_FSM_CHECK_LINK;
			break;
	}
}


// ============================ 对外接口 ============================

bool NetworkInit(void)
{
	W5500_Config_t sW5500Cfg = {0};
	const uint8_t u8Mac[6] = NETWORK_LOCAL_MAC;
	const uint8_t u8DefIp[4] = NETWORK_LOCAL_IP;
	const uint8_t u8DefSubnet[4] = NETWORK_LOCAL_SUBNET;
	const uint8_t u8DefGateway[4] = NETWORK_LOCAL_GATEWAY;
	uint8_t u8Ip[4];
	uint8_t u8Subnet[4];
	uint8_t u8Gateway[4];

	memset(&g_sNetData, 0, sizeof(g_sNetData));

	// SPI 实例与引脚在此绑定: 换 SPI 口只改这几行(引脚名由 CubeMX 定义在 main.h)
	sW5500Cfg.pSpi     = &hspi1;
	sW5500Cfg.pCsPort  = SPI_CS_GPIO_Port;
	sW5500Cfg.CsPin    = SPI_CS_Pin;
	sW5500Cfg.pRstPort = RSTN_GPIO_Port;
	sW5500Cfg.RstPin   = RSTN_Pin;
	sW5500Cfg.pIntPort = INTN_GPIO_Port;
	sW5500Cfg.IntPin   = INTN_Pin;

	/*
	 * 网络参数优先级: Flash 里保存的值 > Network.h 的默认值。
	 * 上位机"保存IP"(0x0F) 下发后 IP 落在 Flash 里, 上电即按新 IP 启动;
	 * 从没保存过(擦除态/校验失败)时回退到宏, 保证首次烧录也能连上。
	 */
	if (SaveParamsGetIp(u8Ip) == false)
	{
		memcpy(u8Ip, u8DefIp, sizeof(u8Ip));
	}
	if (SaveParamsGetSubnet(u8Subnet) == false)
	{
		memcpy(u8Subnet, u8DefSubnet, sizeof(u8Subnet));
	}
	if (SaveParamsGetGateway(u8Gateway) == false)
	{
		memcpy(u8Gateway, u8DefGateway, sizeof(u8Gateway));
	}

	memcpy(sW5500Cfg.Mac, u8Mac, sizeof(u8Mac));
	memcpy(sW5500Cfg.Ip, u8Ip, sizeof(u8Ip));
	memcpy(sW5500Cfg.Subnet, u8Subnet, sizeof(u8Subnet));
	memcpy(sW5500Cfg.Gateway, u8Gateway, sizeof(u8Gateway));

	// 硬复位 + 校验版本号 + 写网络参数 + 分配 Socket 缓冲, 全部带超时
	// 注意: 内部硬件复位会阻塞约 60ms, 电机脉冲由定时器中断驱动不受影响
	if (W5500_Init(&sW5500Cfg) != W5500_OK)
	{
		return false;
	}

	// 记录实际运行的 IP, 供 NetworkGetIp 访问
	memcpy(g_au8CurrentIp, u8Ip, sizeof(g_au8CurrentIp));

	// 网线在位则直接进入打开流程, 否则等待链路
	g_sNetData.eFsmStep = NET_FSM_CHECK_LINK;
	ResetTimerCount(&g_sNetData.u32WaitTimer);

	return true;
}


void NetworkHandle(void)
{
	// 先处理 W5500 中断: 读 SIR -> 清各 Socket 的 Sn_IR -> 累加事件,
	// 同时释放 INTN 引脚(电平型 INT, Sn_IR 不清引脚就一直为低)
	// 未初始化/未接 INT 时内部自动跳过, 不占用 SPI 总线
	if (W5500_IntIsAsserted() != 0u)
	{
		(void)W5500_ProcessInt();
	}

	// UDP 发现服务与 TCP 状态机独立, 任何时候都轮询
	NetworkDiscoveryHandle();

	NetworkFsmHandler();

	if (g_sNetData.bConnected)
	{
		NetworkRecvHandler();
		NetworkSendHandler();
	}
}


// 获取当前运行的 IP(NetworkInit 实际写入 W5500 的值)
void NetworkGetIp(uint8_t *pIp)
{
	memcpy(pIp, g_au8CurrentIp, 4);
}


// 校验 UDP 发现请求帧: 55 AA 11 01 01 chkH chkL AA 55(共 9 字节)
static bool DiscoveryRequestValid(const uint8_t *pBuf, const uint8_t u8Len)
{
	uint16_t u16CheckSum = 0;
	uint16_t u16Sum;

	if (u8Len != 9)
	{
		return false;
	}
	if (pBuf[0] != FRAME_HEAD_0 || pBuf[1] != FRAME_HEAD_1)
	{
		return false;
	}
	if (pBuf[2] != 0x11 || pBuf[3] != 0x01 || pBuf[4] != 0x01)
	{
		return false;
	}
	if (pBuf[7] != FRAME_TAIL_0 || pBuf[8] != FRAME_TAIL_1)
	{
		return false;
	}

	// 校验 = cmd 起累加取反加一, 与 TCP 通道规则一致
	for (uint8_t i = 2; i < 5; i++)
	{
		u16CheckSum += pBuf[i];
	}
	u16CheckSum = (uint16_t)(~u16CheckSum + 1);
	u16Sum = (uint16_t)(((uint16_t)pBuf[5] << 8) | pBuf[6]);

	return u16Sum == u16CheckSum;
}


// 回发现应答帧: 55 AA 12 04 IP1~IP4 chkH chkL AA 55(共 12 字节)
static void DiscoverySendRespond(const uint8_t pDstIp[4], const uint16_t u16DstPort)
{
	uint8_t u8Resp[12];
	uint16_t u16CheckSum = 0;

	u8Resp[0] = FRAME_HEAD_0;
	u8Resp[1] = FRAME_HEAD_1;
	u8Resp[2] = 0x12;
	u8Resp[3] = 0x04;
	memcpy(&u8Resp[4], g_au8CurrentIp, 4);

	for (uint8_t i = 2; i < 8; i++)
	{
		u16CheckSum += u8Resp[i];
	}
	u16CheckSum = (uint16_t)(~u16CheckSum + 1);
	u8Resp[8] = u16CheckSum >> 8;
	u8Resp[9] = u16CheckSum & 0xFF;
	u8Resp[10] = FRAME_TAIL_0;
	u8Resp[11] = FRAME_TAIL_1;

	(void)W5500_SocketSendTo(NETWORK_DISCOVERY_SOCKET, pDstIp, u16DstPort, u8Resp, sizeof(u8Resp));
}


// UDP 发现服务: 监听 Socket1 的广播 0x11, 回 0x12 + 当前 IP。
// 与 TCP 状态机完全独立, 未连接/已连接/断开后都工作
static void NetworkDiscoveryHandle(void)
{
	static uint32_t s_u32OpenTimer = 0;		// Socket 重开的节流计时
	uint8_t u8Buf[16];
	uint8_t u8SrcIp[4];
	uint16_t u16SrcPort;
	int32_t s32Len;

	// Socket 不在 UDP 状态(首次/芯片复位/被外因关闭)时按间隔重开
	if (W5500_SocketStatus(NETWORK_DISCOVERY_SOCKET) != W5500_SR_UDP)
	{
		if (GetTimerTickDelta(s_u32OpenTimer, GetCurTimerCount()) < NETWORK_DISCOVERY_REOPEN_MS)
		{
			return;
		}
		ResetTimerCount(&s_u32OpenTimer);

		if (W5500_SocketOpen(NETWORK_DISCOVERY_SOCKET, W5500_PROTO_UDP, NETWORK_DISCOVERY_PORT) != W5500_OK)
		{
			return;
		}
	}

	if (W5500_SocketRecvSize(NETWORK_DISCOVERY_SOCKET) == 0)
	{
		return;
	}

	s32Len = W5500_SocketRecvFrom(NETWORK_DISCOVERY_SOCKET, u8Buf, sizeof(u8Buf), u8SrcIp, &u16SrcPort);
	if (s32Len <= 0)
	{
		return;
	}

	// 帧不对(杂散广播/别的设备)直接丢弃, 不应答
	if (!DiscoveryRequestValid(u8Buf, (uint8_t)s32Len))
	{
		return;
	}

	DiscoverySendRespond(u8SrcIp, u16SrcPort);
}
