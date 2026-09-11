#include "Timer.h"
//#include "tim.h"


/**
  * @brief  从systick中获取当前ms数
  * @param  None
  * @retval 当前已运行的ms数
  */
uint32_t GetCurTimerCount(void)
{
	return HAL_GetTick();
}


/**
  * @brief  重置用户的Timer计数
  * @param  None
  * @retval None
  */
void ResetTimerCount(uint32_t *pCount)
{
	*pCount = HAL_GetTick();
}


/**
  * @brief  获取两次Timer计数的差值
  * @param  None
  * @retval Timer计数差值, 单位ms
  */
uint32_t GetTimerTickDelta(const uint32_t u32LastCount, const uint32_t u32CurCount)
{
    uint32_t u32Delta = 0;
    u32Delta = (u32CurCount >= u32LastCount) ? (u32CurCount - u32LastCount) : (0XFFFFFFFF - u32LastCount + u32CurCount + 1);
    
    return u32Delta;
}


/**
  * @brief  重置用户的Timer计数
  * @param  None
  * @retval None
  */
void ResetTimerCountUs(uint32_t *pCountUs)
{
//	*pCountUs = htim5.Instance->CNT;
}


/**
  * @brief  从用户定义的tim中获取当前us数
  * @param  None
  * @retval 当前已运行的us计数
  */
uint32_t GetCurTimerCountUs(void)
{
//  return htim5.Instance->CNT;
}


/**
  * @brief  获取两次Timer计数的差值
  * @param  None
  * @retval Timer计数差值, 单位us
  */
uint32_t GetTimerTickDeltaUs(const uint32_t u32LastCount, const uint32_t u32CurCount)
{
//    uint32_t u32Delta = 0;
//    u32Delta = (u32CurCount >= u32LastCount) ? (u32CurCount - u32LastCount) : (0xFFFFFFFF - u32LastCount + u32CurCount + 1);
//    
//    return u32Delta;
}

