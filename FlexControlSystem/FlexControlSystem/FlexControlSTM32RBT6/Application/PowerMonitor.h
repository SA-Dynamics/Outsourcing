/**
 * @file    PowerMonitor.h
 * @brief   电源电压监测: ADC1_IN10(PC0) 非阻塞采样
 * @note    采样方式: 主循环状态机 启动转换 -> 轮询完成 -> 取值,
 *          全程不使用 HAL_Delay / 阻塞等待, 单次转换 + 多次平均,
 *          ADC 初始化(MX_ADC1_Init)由 CubeMX 在 main 里完成, 本模块只使用句柄。
 *          换通道/换分压比只改下面的宏。
 */
#ifndef POWERMONITOR_H
#define POWERMONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/** ADC 原始值均值滤波窗口(2 的幂, 移位实现) */
#define POWER_MONITOR_FILTER_DEPTH      8u

/** ADC 参考电压(V): F103 为 VDDA, 板上按 3.3V 设计 */
#define POWER_MONITOR_VREF_V            3.3f

/** 分压比: 电源 -> PC0 的分压系数(按实际电阻填写, 当前假设 1:11) */
#define POWER_MONITOR_DIVIDER_RATIO     9.47f

/** 两次启动转换的最小间隔 ms(避免主循环过快连续触发) */
#define POWER_MONITOR_INTERVAL_MS       20u

void PowerMonitorInit(void);
void PowerMonitorHandle(void);
bool PowerMonitorGetVoltage(float *pfVoltage);

#ifdef __cplusplus
}
#endif

#endif /* POWERMONITOR_H */
