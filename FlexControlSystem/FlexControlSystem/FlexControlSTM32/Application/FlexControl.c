#include "FlexControl.h"
#include "SerialHandle.h"
#include "MotorControl.h"



void HeartBeatHandler(void)
{
	
}


void MotorMotionHandler(void)
{
	
}


void CbMotionFinish(const bool bSuccess)
{
	
}


void SerialInfoHandle(void)
{
	SerialInfo eSerialInfo;
	
	// 获取串口消息
	if (GetSerialInfo(&eSerialInfo))
	{
		switch (eSerialInfo.eInfo)
		{
			case INFO_HEART:
				SetNormal(true);
				// 发送心跳给上位机
				HeartBeatHandler();
				break;
			
			case INFO_MOTION:
				MotorMotionHandler();
				break;
			
			default:
				break;
		}
	}
}


void FlexControlHandle(void)
{
	SerialInfoHandle();
}



