#ifndef FLEXCONTROL_H
#define FLEXCONTROL_H

#include "main.h"

// 网络初始化结果: true = W5500 就绪, false = 初始化失败(内部定期重试)
extern bool g_bNetworkInitSuccess;

void FlexControlInit(void);
void FlexControlHandle(void);


#endif
