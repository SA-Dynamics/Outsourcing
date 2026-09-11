#include "SerialHandle.h"
#include "usart.h"
#include <string.h>

typedef struct
{
	float fAngle;
	float fVelosity;
	uint8_t u8RecvBuffer[UART_DATA_BUFFER_SIZE];
	uint16_t u16WriteIndex;
	uint16_t u16ReadIndex;
	uint16_t u16DataIndex;
	bool bRecvFinish;
	uint8_t u8FSMStep;
	UART_HandleTypeDef *huart;
	DMA_HandleTypeDef *hdma;
	float fAngleOffset;
}SerialDataType;

SerialDataType g_sSerial1Data;


SerialInfo g_sSerialInfoBuffer[SERIAL_INFO_BUFFER_SIZE];


//IMU_Type g_sIMULink1, g_sIMULink2;
//IMU_Data g_sIMU1, g_sIMU2;

void CbUartIdleHandler(UART_HandleTypeDef *huart)
{
	if (huart == &huart1)
	{
		g_sSerial1Data.bRecvFinish = true;
	}	
//	else if (huart == &huart3)
//	{
//		g_sIMULink2.bRecvFinish = true;
//	}
}



void IMU_Init(void)
{
//	g_sIMULink1.huart = &huart2;
//	g_sIMULink1.hdma = &hdma_usart2_rx;
//	g_sIMULink2.huart = &huart3;
//	g_sIMULink2.hdma = &hdma_usart3_rx;
//	
//	g_sIMULink1.fAngleOffset = 1.77279632679f;
//	g_sIMULink2.fAngleOffset = -0.02f;
	
//	__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
//	HAL_UART_Receive_DMA(&huart2, g_sIMULink1.u8RecvBuffer, sizeof(g_sIMULink1.u8RecvBuffer));
//	__HAL_UART_ENABLE_IT(&huart3, UART_IT_IDLE);
//	HAL_UART_Receive_DMA(&huart3, g_sIMULink2.u8RecvBuffer, sizeof(g_sIMULink2.u8RecvBuffer));
}


void SendInfoToBuffer(const SerialInfo *pInfo)
{
	
}


static void ParseMotionData(SerialInfo *pInfoWrite, uint8_t *pDataStart)
{
	
}



void ParseUartData(SerialDataType *pSerial)
{
	SerialInfo sInfo;
	uint16_t u16CheckSum = 0;
	
	// 判断帧类型
	switch (pSerial->u8RecvBuffer[pSerial->u16DataIndex]) 
	{
		case 0x01:
			sInfo.eInfo = INFO_HEART;
			break;
		
		case 0x03:
			sInfo.eInfo = INFO_MOTION;
			ParseMotionData(&sInfo, &pSerial->u8RecvBuffer[(pSerial->u16DataIndex + 2) % UART_DATA_BUFFER_SIZE]);
			break;
		
		default:
			return;
	}
	
	uint8_t u8DataSize = pSerial->u8RecvBuffer[(pSerial->u16DataIndex + 1) % UART_DATA_BUFFER_SIZE];
	
	// 校验数据
	for (uint8_t i = 0; i < u8DataSize; i++)
	{
		u16CheckSum += pSerial->u8RecvBuffer[(pSerial->u16DataIndex + 2 + i) % UART_DATA_BUFFER_SIZE];
	}
	u16CheckSum = ~u16CheckSum + 1;
	
	if ((u16CheckSum >> 8) == pSerial->u8RecvBuffer[(pSerial->u16DataIndex + 2 + u8DataSize) % UART_DATA_BUFFER_SIZE] &&
		(u16CheckSum & 0xFF) == pSerial->u8RecvBuffer[(pSerial->u16DataIndex + 2 + u8DataSize + 1) % UART_DATA_BUFFER_SIZE])
	{
		SendInfoToBuffer(&sInfo);
	}	
}


void UartBufferHandler(SerialDataType *pSerial)
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

    switch (pSerial->u8FSMStep)
    {
		case CHECK_FRAME_RECV:
            // 首先判断接收完成标志
			if (pSerial->bRecvFinish)
			{
				pSerial->u8FSMStep = UPDATE_WRITE_INDEX;
				pSerial->bRecvFinish = false;
			}
			break;

        case UPDATE_WRITE_INDEX:
			// 计算写索引在buffer中的位置
            pSerial->u16WriteIndex = 
                UART_DATA_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(pSerial->hdma);
            
			// 计算接收了多少个数据
			uint16_t u16Diff = (pSerial->u16WriteIndex >= pSerial->u16ReadIndex) ? (pSerial->u16WriteIndex - pSerial->u16ReadIndex) : (0XFFFF - pSerial->u16ReadIndex + pSerial->u16WriteIndex + 1);
            
			// 判断是否需要查找帧头
			if (pSerial->u16WriteIndex != pSerial->u16ReadIndex && u16Diff >= 8)
            {
                pSerial->u8FSMStep = FIND_FRAME_HEAD;
            }
            else
            {
                pSerial->u8FSMStep = CHECK_FRAME_RECV;
            }
            break;
		
        case FIND_FRAME_HEAD:
            // 判断帧头
            if (0x55 == pSerial->u8RecvBuffer[pSerial->u16ReadIndex] && 
				0xAA == pSerial->u8RecvBuffer[(pSerial->u16ReadIndex + 1) % UART_DATA_BUFFER_SIZE])
            {                
                pSerial->u8FSMStep = FIND_FRAME_TAIL;
				
				// 获取数据所在位置
				pSerial->u16DataIndex = (pSerial->u16ReadIndex + 2) % UART_DATA_BUFFER_SIZE;
            }
            else
            {
                // 如果没有找到帧头, 就重新等待接收
                if (pSerial->u16WriteIndex == pSerial->u16ReadIndex)
                {
                    pSerial->u8FSMStep = CHECK_FRAME_RECV;
                    break;
                }
				
				// 没有找到帧头的情况下, 把读索引前移, 为下一轮查找帧头
				pSerial->u16ReadIndex = (pSerial->u16ReadIndex + 1) % UART_DATA_BUFFER_SIZE;
            }
            break;

        case FIND_FRAME_TAIL:
            if (0xAA == pSerial->u8RecvBuffer[pSerial->u16ReadIndex] && 
				0x55 == pSerial->u8RecvBuffer[(pSerial->u16ReadIndex + 1) % UART_DATA_BUFFER_SIZE])
            {
				// 找到了帧尾, 去校验数据
				pSerial->u8FSMStep = PARSE_DATA;
            }
            else
            {
				// 如果没有找到帧尾, 就重新等待接收
                if (pSerial->u16WriteIndex == pSerial->u16ReadIndex)
                {
                    pSerial->u8FSMStep = CHECK_FRAME_RECV;
                    break;
                }
				
				// 没有找到帧尾的情况下, 把读索引前移, 为下一轮查找帧尾
				pSerial->u16ReadIndex = (pSerial->u16ReadIndex + 1) % UART_DATA_BUFFER_SIZE;
            }
            break;

        case PARSE_DATA:
            // 解析数据
            ParseUartData(pSerial);			
		
			// 把读索引更新到下一帧的开头位置
            pSerial->u16ReadIndex = (pSerial->u16ReadIndex + 2) % UART_DATA_BUFFER_SIZE;

            if (pSerial->u16ReadIndex == pSerial->u16WriteIndex)
            {
                // 当前接收到的数据已经读完
                pSerial->u8FSMStep = CHECK_FRAME_RECV;
            }
            else
            {
                // 查找下一个帧头
                pSerial->u8FSMStep = FIND_FRAME_HEAD;
            }           
            break;
    }
}


bool GetSerialInfo(SerialInfo *pInfo)
{
	bool bRet = false;
	
	return bRet;
}


void SerialDataHandle(void)
{
	UartBufferHandler(&g_sSerial1Data);
	
//	UartBufferHandler(&g_sIMULink2);
//	UartBufferHandler(&g_sIMULink1);
//	
//	g_sIMU1.fAngle = g_sIMULink1.fAngle;
//	g_sIMU1.fVelosity = g_sIMULink1.fVelosity;
//	g_sIMU2.fAngle = g_sIMULink2.fAngle;
//	g_sIMU2.fVelosity = g_sIMULink2.fVelosity;
}
