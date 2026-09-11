#include "FlexControl.h"
#include "SerialHandle.h"


void SerialInfoHandle(void)
{
	SerialInfo eSerialInfo;
	
	if (GetSerialInfo(&eSerialInfo))
	{
		switch (eSerialInfo.eInfo)
		{
			case INFO_HEART:
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



