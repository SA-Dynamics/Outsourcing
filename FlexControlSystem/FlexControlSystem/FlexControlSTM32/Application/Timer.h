#ifndef TIMER_H
#define TIMER_H

#include "stm32f1xx_hal.h"

uint32_t GetCurTimerCount(void);
void ResetTimerCount(uint32_t *pCount);
uint32_t GetTimerTickDelta(const uint32_t u32LastCount, const uint32_t u32CurCount);

void ResetTimerCountUs(uint32_t *pCountUs);
uint32_t GetCurTimerCountUs(void);
uint32_t GetTimerTickDeltaUs(const uint32_t u32LastCount, const uint32_t u32CurCount);

#endif
