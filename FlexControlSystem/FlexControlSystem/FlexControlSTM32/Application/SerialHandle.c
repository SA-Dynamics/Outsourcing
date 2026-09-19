#include "SerialHandle.h"
#include "usart.h"
#include <string.h>
#include "Timer.h"


#define HEART_BEAT_RESPOND_SIZE			8
#define MOTION_FINISH_RESPOND_SIZE		10

typedef struct
{
	struct
	{
		uint8_t u8RecvBuffer[UART_RECV_DATA_BUFFER_SIZE];
		uint16_t u16WriteIndex;
		uint16_t u16ReadIndex;
		uint16_t u16DataIndex;
		bool bRecvFinish;
		uint8_t u8FSMStep;
		
		DMA_HandleTypeDef *hdma;
	}sRecv;	
	
	struct
	{
		struct
		{
			uint8_t u8SendData[UART_SEND_DATA_LEN];
			uint8_t u8Len;
		}sBuffer[UART_SEND_BUFFER_SIZE];
		uint16_t u16WriteIndex;
		uint16_t u16ReadIndex;
		uint16_t u16BufferLen;
		bool bSendFinish;
		uint8_t u8FSMStep;
		
		DMA_HandleTypeDef *hdma;
	}sSend;	

	UART_HandleTypeDef *huart;	
}SerialDataType;

// 串口通信实例
SerialDataType g_sSerial1Data;

// 存放处理过后的数据, 所有通信方式共享
struct
{
	uint8_t u8WriteIndex;
	uint8_t u8ReadIndex;
	uint8_t u8BufferLen;
	SerialInfo sSerialInfoBuffer[SERIAL_INFO_BUFFER_SIZE];
}g_sParseData;


// 检测空闲中断
void CbUartIdleHandler(UART_HandleTypeDef *huart)
{
	if (huart == &huart1)
	{
		g_sSerial1Data.sRecv.bRecvFinish = true;
	}	
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	if (huart == &huart1)
	{
		g_sSerial1Data.sSend.bSendFinish = true;
	}
}


void SerialHandleInit(void)
{	
	g_sSerial1Data.huart = &huart1;
	g_sSerial1Data.sRecv.hdma = &hdma_usart1_rx;
	g_sSerial1Data.sSend.hdma = &hdma_usart1_tx;
	__HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
	HAL_UART_Receive_DMA(&huart1, g_sSerial1Data.sRecv.u8RecvBuffer, sizeof(g_sSerial1Data.sRecv.u8RecvBuffer));
}


// 获取解析后的数据, 供外部调用
bool GetSerialInfo(SerialInfo *pInfo)
{
	bool bRet = false;
	
	if (g_sParseData.u8BufferLen)
	{
		memcpy(pInfo, &g_sParseData.sSerialInfoBuffer[g_sParseData.u8ReadIndex], sizeof(SerialInfo));
		g_sParseData.u8ReadIndex = (g_sParseData.u8ReadIndex + 1) % SERIAL_INFO_BUFFER_SIZE;
		
		g_sParseData.u8BufferLen--;
		bRet = true;
	}
	
	return bRet;
}


// 将解析数据存放在buffer中
static void SendRecvParseInfoToBuffer(const SerialInfo *pInfo)
{
	if (g_sParseData.u8BufferLen < SERIAL_INFO_BUFFER_SIZE)
	{
		memcpy(&g_sParseData.sSerialInfoBuffer[g_sParseData.u8WriteIndex], pInfo, sizeof(SerialInfo));
		g_sParseData.u8WriteIndex = (g_sParseData.u8WriteIndex + 1) % SERIAL_INFO_BUFFER_SIZE;
		g_sParseData.u8BufferLen++;
	}
}


// 解析运动指令
static void ParseMotionData(SerialInfo *pInfoWrite, SerialDataType *pSerial)
{
	pInfoWrite->u8MotionIndex = pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 2) % UART_RECV_DATA_BUFFER_SIZE];
	
	pInfoWrite->fMotionVoltage = (float)((uint16_t)(pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 3) % UART_RECV_DATA_BUFFER_SIZE] << 8) | 
								pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 4) % UART_RECV_DATA_BUFFER_SIZE]) / 10.0f;
	pInfoWrite->fMotionFrequency = (float)((uint16_t)(pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 5) % UART_RECV_DATA_BUFFER_SIZE] << 8) | 
								pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 6) % UART_RECV_DATA_BUFFER_SIZE]) / 10.0f;
	pInfoWrite->fMotionTimeUse = (float)((uint16_t)(pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 7) % UART_RECV_DATA_BUFFER_SIZE] << 8) | 
								pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 8) % UART_RECV_DATA_BUFFER_SIZE]) / 10.0f;
}



void ParseUartData(SerialDataType *pSerial)
{
	SerialInfo sInfo;
	uint16_t u16CheckSum = 0;
	
	// 判断帧类型
	switch (pSerial->sRecv.u8RecvBuffer[pSerial->sRecv.u16DataIndex]) 
	{
		case 0x01:
			sInfo.eInfo = INFO_HEART;
			break;
		
		case 0x03:
			sInfo.eInfo = INFO_MOTION;
			ParseMotionData(&sInfo, pSerial);
			break;
		
		default:
			return;
	}
	
	uint8_t u8DataSize = pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 1) % UART_RECV_DATA_BUFFER_SIZE];
	
	// 校验数据
	for (uint8_t i = 0; i < u8DataSize + 2; i++)
	{
		u16CheckSum += pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + i) % UART_RECV_DATA_BUFFER_SIZE];
	}
	u16CheckSum = ~u16CheckSum + 1;
	
	if ((u16CheckSum >> 8) == pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 2 + u8DataSize) % UART_RECV_DATA_BUFFER_SIZE] &&
		(u16CheckSum & 0xFF) == pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16DataIndex + 2 + u8DataSize + 1) % UART_RECV_DATA_BUFFER_SIZE])
	{
		SendRecvParseInfoToBuffer(&sInfo);
	}	
}


static void UartRecvBufferHandler(SerialDataType *pSerial)
{
    enum
    {
		CHECK_FRAME_RECV = 0,
        UPDATE_WRITE_INDEX,
        FIND_FRAME_HEAD,
        FIND_FRAME_TAIL,
        PARSE_DATA,
    };
    static uint8_t u8DataLen = 0;

    switch (pSerial->sRecv.u8FSMStep)
    {
		case CHECK_FRAME_RECV:
            // 首先判断接收完成标志
			if (pSerial->sRecv.bRecvFinish)
			{
				pSerial->sRecv.u8FSMStep = UPDATE_WRITE_INDEX;
				pSerial->sRecv.bRecvFinish = false;
			}
			break;

        case UPDATE_WRITE_INDEX:
			// 计算写索引在buffer中的位置
            pSerial->sRecv.u16WriteIndex = 
                UART_RECV_DATA_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(pSerial->sRecv.hdma);
            
			// 计算接收了多少个数据
			uint16_t u16Diff = (pSerial->sRecv.u16WriteIndex >= pSerial->sRecv.u16ReadIndex) ? 
				(pSerial->sRecv.u16WriteIndex - pSerial->sRecv.u16ReadIndex) : 
				(0XFFFF - pSerial->sRecv.u16ReadIndex + pSerial->sRecv.u16WriteIndex + 1);
            
			// 判断是否需要查找帧头
			if (pSerial->sRecv.u16WriteIndex != pSerial->sRecv.u16ReadIndex && u16Diff >= 8)
            {
                pSerial->sRecv.u8FSMStep = FIND_FRAME_HEAD;
            }
            else
            {
                pSerial->sRecv.u8FSMStep = CHECK_FRAME_RECV;
            }
            break;
		
        case FIND_FRAME_HEAD:
            // 判断帧头
            if (0x55 == pSerial->sRecv.u8RecvBuffer[pSerial->sRecv.u16ReadIndex] && 
				0xAA == pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16ReadIndex + 1) % UART_RECV_DATA_BUFFER_SIZE])
            {                
                pSerial->sRecv.u8FSMStep = FIND_FRAME_TAIL;
				
				// 获取数据所在位置
				pSerial->sRecv.u16DataIndex = (pSerial->sRecv.u16ReadIndex + 2) % UART_RECV_DATA_BUFFER_SIZE;
            }
            else
            {
                // 如果没有找到帧头, 就重新等待接收
                if (pSerial->sRecv.u16WriteIndex == pSerial->sRecv.u16ReadIndex)
                {
                    pSerial->sRecv.u8FSMStep = CHECK_FRAME_RECV;
                    break;
                }
				
				// 没有找到帧头的情况下, 把读索引前移, 为下一轮查找帧头
				pSerial->sRecv.u16ReadIndex = (pSerial->sRecv.u16ReadIndex + 1) % UART_RECV_DATA_BUFFER_SIZE;
            }
            break;

        case FIND_FRAME_TAIL:
            if (0xAA == pSerial->sRecv.u8RecvBuffer[pSerial->sRecv.u16ReadIndex] && 
				0x55 == pSerial->sRecv.u8RecvBuffer[(pSerial->sRecv.u16ReadIndex + 1) % UART_RECV_DATA_BUFFER_SIZE])
            {
				// 找到了帧尾, 去校验数据
				pSerial->sRecv.u8FSMStep = PARSE_DATA;
            }
            else
            {
				// 如果没有找到帧尾, 就重新等待接收
                if (pSerial->sRecv.u16WriteIndex == pSerial->sRecv.u16ReadIndex)
                {
                    pSerial->sRecv.u8FSMStep = CHECK_FRAME_RECV;
                    break;
                }
				
				// 没有找到帧尾的情况下, 把读索引前移, 为下一轮查找帧尾
				pSerial->sRecv.u16ReadIndex = (pSerial->sRecv.u16ReadIndex + 1) % UART_RECV_DATA_BUFFER_SIZE;
            }
            break;

        case PARSE_DATA:
            // 解析数据
            ParseUartData(pSerial);			
		
			// 把读索引更新到下一帧的开头位置
            pSerial->sRecv.u16ReadIndex = (pSerial->sRecv.u16ReadIndex + 2) % UART_RECV_DATA_BUFFER_SIZE;

            if (pSerial->sRecv.u16ReadIndex == pSerial->sRecv.u16WriteIndex)
            {
                // 当前接收到的数据已经读完
                pSerial->sRecv.u8FSMStep = CHECK_FRAME_RECV;
            }
            else
            {
                // 查找下一个帧头
                pSerial->sRecv.u8FSMStep = FIND_FRAME_HEAD;
            }           
            break;
    }
}


// 获取要发送的数据
static bool GetSendByteArray(uint8_t *pData, uint8_t *pLen)
{
	bool bRet = false;
	
	if (g_sSerial1Data.sSend.u16BufferLen)
	{
		memcpy(pData, g_sSerial1Data.sSend.sBuffer[g_sSerial1Data.sSend.u16ReadIndex].u8SendData, 
			g_sSerial1Data.sSend.sBuffer[g_sSerial1Data.sSend.u16ReadIndex].u8Len);
		
		*pLen = g_sSerial1Data.sSend.sBuffer[g_sSerial1Data.sSend.u16ReadIndex].u8Len;
		g_sSerial1Data.sSend.u16ReadIndex = (g_sSerial1Data.sSend.u16ReadIndex + 1) % UART_SEND_BUFFER_SIZE;
		
		g_sSerial1Data.sSend.u16BufferLen--;
		bRet = true;
	}
	
	return bRet;
}


// 将发送数据存放在buffer中
static void SaveSendByteArray(const uint8_t *pData, const uint8_t u8Len)
{
	if (g_sSerial1Data.sSend.u16BufferLen < UART_SEND_BUFFER_SIZE)
	{
		memcpy(g_sSerial1Data.sSend.sBuffer[g_sSerial1Data.sSend.u16WriteIndex].u8SendData, pData, u8Len);
		g_sSerial1Data.sSend.sBuffer[g_sSerial1Data.sSend.u16WriteIndex].u8Len = u8Len;
		g_sSerial1Data.sSend.u16WriteIndex = (g_sSerial1Data.sSend.u16WriteIndex + 1) % UART_SEND_BUFFER_SIZE;
		g_sSerial1Data.sSend.u16BufferLen++;
	}
}


static void SendHeartBeat(void)
{
	uint8_t u8Buffer[HEART_BEAT_RESPOND_SIZE] = {0};
	
	u8Buffer[0] = 0x55;
	u8Buffer[1] = 0xAA;
	u8Buffer[2] = 0x02;
	u8Buffer[3] = 0x00;
	u8Buffer[4] = 0xFF;
	u8Buffer[5] = 0xFE;
	u8Buffer[6] = 0xAA;
	u8Buffer[7] = 0x55;
	
	SaveSendByteArray(u8Buffer, HEART_BEAT_RESPOND_SIZE);
}


static void SendMotionFinish(uint8_t u8Index)
{
	uint8_t u8Buffer[MOTION_FINISH_RESPOND_SIZE] = {0};
	
	u8Buffer[0] = 0x55;
	u8Buffer[1] = 0xAA;
	u8Buffer[2] = 0x04;
	u8Buffer[3] = 0x02;
	u8Buffer[4] = u8Index;
	u8Buffer[5] = 0x01;
	
	uint16_t u16CheckSum = 0;
	// 校验数据
	for (uint8_t i = 0; i < u8Buffer[3] + 2; i++)
	{
		u16CheckSum += u8Buffer[2 + i];
	}
	u16CheckSum = ~u16CheckSum + 1;
	
	u8Buffer[6] = u16CheckSum >> 8;
	u8Buffer[7] = u16CheckSum & 0xFF;
	
	u8Buffer[8] = 0xAA;
	u8Buffer[9] = 0x55;
	
	SaveSendByteArray(u8Buffer, MOTION_FINISH_RESPOND_SIZE);
}


// 发送上位机数据的响应
void SendSerialRespond(const RespondType eType, void *pParams)
{
	switch (eType)
	{
		case RESPOND_HEART:
			SendHeartBeat();
			break;
		
		case RESPOND_MOTION_FINISH:
			SendMotionFinish(*(uint8_t *)pParams);
			break;
		
		default:
			break;
	}
}


void SendSerialByteArray(SerialDataType *pSerial)
{
    enum
    {
        WRITE_SERIAL_FUNC = 0,
		WAIT_TRANSMIT_SUCCESS,
		WAIT_TIME,
    };
    
    static uint32_t u32WaitTimer = 0;
    static uint8_t u8SendData[UART_SEND_DATA_LEN] = {0};
	static uint8_t u8Len = 0;

    switch (pSerial->sSend.u8FSMStep)
    {
        case WRITE_SERIAL_FUNC:
			if (GetSendByteArray(u8SendData, &u8Len))
			{
				// 发送数据
				HAL_UART_Transmit_DMA(pSerial->huart, u8SendData, u8Len);
				ResetTimerCount(&u32WaitTimer);
				pSerial->sSend.u8FSMStep = WAIT_TRANSMIT_SUCCESS;
			}
            break;
		
		case WAIT_TRANSMIT_SUCCESS:
            // 等待发送完成
			if (pSerial->sSend.bSendFinish)
			{
				ResetTimerCount(&u32WaitTimer);
				pSerial->sSend.bSendFinish = false;
				pSerial->sSend.u8FSMStep = WAIT_TIME;
			}
			else
			{
				if (GetTimerTickDelta(u32WaitTimer, GetCurTimerCount()) >= 2000)
				{
					pSerial->sSend.u8FSMStep = WRITE_SERIAL_FUNC;
				}
			}
			break;

        case WAIT_TIME:
            // 等待串口屏响应
            if (GetTimerTickDelta(u32WaitTimer, GetCurTimerCount()) >= 100)
            {
				memset(u8SendData, 0, sizeof(u8SendData));
                pSerial->sSend.u8FSMStep = WRITE_SERIAL_FUNC;
            }
            break;
    }	
}



void SerialDataHandle(void)
{
	UartRecvBufferHandler(&g_sSerial1Data);
	SendSerialByteArray(&g_sSerial1Data);
}
