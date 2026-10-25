#include "Network.h"
#include "W5500.h"
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
	int16_t s16Len = W5500SocketRecv(NETWORK_SOCKET_INDEX, u8Temp, sizeof(u8Temp));

	// 将收到的数据放入环形缓冲
	for (int16_t i = 0; i < s16Len; i++)
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

	int16_t s16Ret = W5500SocketSend(NETWORK_SOCKET_INDEX,
		g_sNetData.sSend.u8Data[g_sNetData.sSend.u8ReadIndex],
		g_sNetData.sSend.u8Len[g_sNetData.sSend.u8ReadIndex]);

	// 发送成功才移出队列; 失败(发送缓冲不足超时)保留本帧, 下一周期重试
	if (s16Ret > 0)
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

				if (W5500GetLinkStatus())
				{
					g_sNetData.eFsmStep = NET_FSM_OPEN_SOCKET;
				}
			}
			break;

		case NET_FSM_OPEN_SOCKET:
			if (W5500SocketOpen(NETWORK_SOCKET_INDEX, W5500_MODE_TCP, NETWORK_TCP_PORT))
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
			if (W5500SocketListen(NETWORK_SOCKET_INDEX))
			{
				g_sNetData.eFsmStep = NET_FSM_WAIT_CONNECT;
			}
			else
			{
				W5500SocketClose(NETWORK_SOCKET_INDEX);
				ResetTimerCount(&g_sNetData.u32WaitTimer);
				g_sNetData.eFsmStep = NET_FSM_CHECK_LINK;
			}
			break;

		case NET_FSM_WAIT_CONNECT:
			if (W5500GetSocketStatus(NETWORK_SOCKET_INDEX) == W5500_SR_ESTABLISHED)
			{
				g_sNetData.bConnected = true;
				// 进入连接后清一次残留中断标志
				W5500ClearSocketIrq(NETWORK_SOCKET_INDEX, 0xFF);
				g_sNetData.eFsmStep = NET_FSM_ESTABLISHED;
			}
			else if (W5500GetSocketStatus(NETWORK_SOCKET_INDEX) == W5500_SR_CLOSED)
			{
				g_sNetData.eFsmStep = NET_FSM_OPEN_SOCKET;
			}
			break;

		case NET_FSM_ESTABLISHED:
		{
			uint8_t u8Status = W5500GetSocketStatus(NETWORK_SOCKET_INDEX);

			// 对端断开或链路断开, 重新走监听流程
			if (u8Status == W5500_SR_CLOSE_WAIT || u8Status == W5500_SR_CLOSED || !W5500GetLinkStatus())
			{
				g_sNetData.bConnected = false;
				// 丢弃半包数据
				g_sNetData.sRecv.u16ReadIndex = g_sNetData.sRecv.u16WriteIndex;
				W5500SocketClose(NETWORK_SOCKET_INDEX);
				g_sNetData.eFsmStep = NET_FSM_OPEN_SOCKET;
			}
			else
			{
				// 清理本周期已处理的中断标志, 释放 INTN 引脚
				uint8_t u8Irq = W5500_SIR_RECV | W5500_SIR_CON | W5500_SIR_DISCON |
								W5500_SIR_SEND_OK | W5500_SIR_TIMEOUT;
				W5500ClearSocketIrq(NETWORK_SOCKET_INDEX, u8Irq);
			}
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
	memset(&g_sNetData, 0, sizeof(g_sNetData));

	// 硬复位并校验 W5500
	if (!W5500HardwareReset())
	{
		return false;
	}

	// 配置网络信息
	const uint8_t u8Mac[6] = NETWORK_LOCAL_MAC;
	const uint8_t u8Ip[4] = NETWORK_LOCAL_IP;
	const uint8_t u8Subnet[4] = NETWORK_LOCAL_SUBNET;
	const uint8_t u8Gateway[4] = NETWORK_LOCAL_GATEWAY;
	W5500SetNetInfo(u8Mac, u8Ip, u8Subnet, u8Gateway);

	// 网线在位则直接进入打开流程, 否则等待链路
	g_sNetData.eFsmStep = NET_FSM_CHECK_LINK;
	ResetTimerCount(&g_sNetData.u32WaitTimer);

	return true;
}


void NetworkHandle(void)
{
	NetworkFsmHandler();

	if (g_sNetData.bConnected)
	{
		NetworkRecvHandler();
		NetworkSendHandler();
	}
}
