#include "FlexControl.h"
#include "SerialHandle.h"
#include "MotorControl.h"



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
	
	SendMotorInfo(&sParams);
}


// 运动完成处理回调
void CbMotionFinish(const uint8_t u8Index)
{
	// 将运动完成消息发送给上位机
	SendSerialRespond(RESPOND_MOTION_FINISH, (void *)&u8Index);
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


void FlexControlInit(void)
{
	SerialHandleInit();
}


void FlexControlHandle(void)
{
	SerialRecvInfoHandle();
	SerialDataHandle();
	
	MotorControl();
}



