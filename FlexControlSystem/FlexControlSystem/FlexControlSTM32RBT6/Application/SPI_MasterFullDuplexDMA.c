/**
 ******************************************************************************
 * @file    SPI_MasterFullDuplexDMA.c
 * @brief   SPI 主机全双工收发驱动(带 DMA)实现
 * @author  user
 * @date    2026-11-11
 *
 * @note    本文件不含任何具体外设名(如 SPI1)、引脚号(如 GPIO_PIN_15),
 *          也不 include "main.h" / "spi.h",因此可被任意工程复用。
 *          所有实例信息由调用方通过 SpiDevice_t 描述符传入。
 *
 * @note    传输策略:
 *            - 短帧(< SPI_MASTER_DMA_MIN_LEN)走阻塞轮询,省去 DMA 启停开销;
 *            - 长帧走 DMA,CPU 在等待期间只做超时判断;
 *            - 两条路径对上层接口完全一致,可只改宏做 A/B 对比。
 ******************************************************************************
 */

#include "SPI_MasterFullDuplexDMA.h"

/*----------------------------------------------------------------------------
 * 私有函数原型
 *----------------------------------------------------------------------------*/
static uint32_t SpiDevTimeoutOrDefault(uint32_t u32TimeoutMs);
static void     SpiDevClearOverrun(SPI_HandleTypeDef *pHandle);
static int32_t  SpiDevWaitReady(SpiDevice_t *pDev, uint32_t u32TimeoutMs);
static int32_t  SpiDevCheckResult(SpiDevice_t *pDev);

/*----------------------------------------------------------------------------
 * 内部工具
 *----------------------------------------------------------------------------*/

/**
 * @brief  超时参数归一化,0 表示取默认值
 */
static uint32_t SpiDevTimeoutOrDefault(uint32_t u32TimeoutMs)
{
    if (u32TimeoutMs == 0u)
    {
        return (uint32_t)SPI_MASTER_XFER_TIMEOUT_MS;
    }
    return u32TimeoutMs;
}

/**
 * @brief  清除 RXNE 与 OVR(溢出)标志
 * @note   STM32 的 OVR 需"读 DR 再读 SR"才能清;连续两次确保彻底清干净。
 *         只写不读的帧(如 W5500 写寄存器)必然产生 OVR,不清会导致后续
 *         读回的字节整体错位一个。
 */
static void SpiDevClearOverrun(SPI_HandleTypeDef *pHandle)
{
    volatile uint32_t u32Tmp;

    u32Tmp = pHandle->Instance->DR;
    u32Tmp = pHandle->Instance->SR;
    u32Tmp = pHandle->Instance->DR;
    u32Tmp = pHandle->Instance->SR;
    (void)u32Tmp;
}

/**
 * @brief  等待 SPI 回到 READY(上一次传输结束)
 * @return SPI_DEV_OK / SPI_DEV_ERR_TIMEOUT
 * @note   超时后调用 HAL_SPI_Abort 收尾,避免 HAL 句柄卡在 BUSY 状态。
 */
static int32_t SpiDevWaitReady(SpiDevice_t *pDev, uint32_t u32TimeoutMs)
{
    uint32_t u32TimeStart = HAL_GetTick();

    while (pDev->pHandle->State != HAL_SPI_STATE_READY)
    {
        if ((HAL_GetTick() - u32TimeStart) > u32TimeoutMs)
        {
            (void)HAL_SPI_Abort(pDev->pHandle);
            return SPI_DEV_ERR_TIMEOUT;
        }
    }
    return SPI_DEV_OK;
}

/**
 * @brief  检查 HAL 错误码并清零
 */
static int32_t SpiDevCheckResult(SpiDevice_t *pDev)
{
    if (pDev->pHandle->ErrorCode != HAL_SPI_ERROR_NONE)
    {
        pDev->pHandle->ErrorCode = HAL_SPI_ERROR_NONE;
        return SPI_DEV_ERR_HAL;
    }
    return SPI_DEV_OK;
}

/*----------------------------------------------------------------------------
 * 绑定与片选
 *----------------------------------------------------------------------------*/

int32_t SpiDevAttach(SpiDevice_t      *pDev,
                     SPI_HandleTypeDef *pHandle,
                     GPIO_TypeDef      *pCsPort,
                     uint16_t           u16CsPin)
{
    if ((pDev == NULL) || (pHandle == NULL))
    {
        return SPI_DEV_ERR_PARAM;
    }

    pDev->pHandle         = pHandle;
    pDev->pCsPort         = pCsPort;
    pDev->CsPin           = u16CsPin;
    pDev->pfnCsCtrl       = NULL;
    pDev->u8CsActiveLevel = 0u;          /* 默认低电平有效 */

    return SPI_DEV_OK;
}

void SpiDevSetCsActiveLevel(SpiDevice_t *pDev, uint8_t u8ActiveLevel)
{
    if (pDev == NULL)
    {
        return;
    }
    pDev->u8CsActiveLevel = (u8ActiveLevel != 0u) ? 1u : 0u;
}

void SpiDevSetCsCallback(SpiDevice_t *pDev, SpiCsCtrl_t pfnCsCtrl)
{
    if (pDev == NULL)
    {
        return;
    }
    pDev->pfnCsCtrl = pfnCsCtrl;
}

void SpiDevSelect(SpiDevice_t *pDev)
{
    uint8_t u8Level;

    if (pDev == NULL)
    {
        return;
    }

    /* 选中电平取反即为引脚输出电平 */
    u8Level = (pDev->u8CsActiveLevel != 0u) ? 1u : 0u;

    if (pDev->pfnCsCtrl != NULL)
    {
        pDev->pfnCsCtrl(u8Level);
        return;
    }
    if (pDev->pCsPort == NULL)
    {
        return;
    }
    HAL_GPIO_WritePin(pDev->pCsPort, pDev->CsPin,
                      (u8Level != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void SpiDevDeselect(SpiDevice_t *pDev)
{
    uint8_t u8Level;

    if (pDev == NULL)
    {
        return;
    }

    /* 未选中电平 = 选中电平取反 */
    u8Level = (pDev->u8CsActiveLevel != 0u) ? 0u : 1u;

    if (pDev->pfnCsCtrl != NULL)
    {
        pDev->pfnCsCtrl(u8Level);
        return;
    }
    if (pDev->pCsPort == NULL)
    {
        return;
    }
    HAL_GPIO_WritePin(pDev->pCsPort, pDev->CsPin,
                      (u8Level != 0u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/*----------------------------------------------------------------------------
 * 帧式访问
 *----------------------------------------------------------------------------*/

int32_t SpiDevBegin(SpiDevice_t *pDev)
{
    if ((pDev == NULL) || (pDev->pHandle == NULL))
    {
        return SPI_DEV_ERR_PARAM;
    }

    /* 芯片手册要求:上一帧必须完全结束才能开始新帧 */
    if (SpiDevWaitReady(pDev, SPI_MASTER_BUSY_TIMEOUT_MS) != SPI_DEV_OK)
    {
        return SPI_DEV_ERR_BUSY;
    }

    SpiDevClearOverrun(pDev->pHandle);
    SpiDevSelect(pDev);

    return SPI_DEV_OK;
}

int32_t SpiDevEnd(SpiDevice_t *pDev)
{
    uint32_t u32TimeStart;

    if ((pDev == NULL) || (pDev->pHandle == NULL))
    {
        return SPI_DEV_ERR_PARAM;
    }

    if (SpiDevWaitReady(pDev, SPI_MASTER_XFER_TIMEOUT_MS) != SPI_DEV_OK)
    {
        SpiDevDeselect(pDev);
        return SPI_DEV_ERR_TIMEOUT;
    }

    /* 等发送缓冲区空 */
    u32TimeStart = HAL_GetTick();
    while (__HAL_SPI_GET_FLAG(pDev->pHandle, SPI_FLAG_TXE) == RESET)
    {
        if ((HAL_GetTick() - u32TimeStart) > SPI_MASTER_XFER_TIMEOUT_MS)
        {
            SpiDevDeselect(pDev);
            return SPI_DEV_ERR_TIMEOUT;
        }
    }

    /* 等最后一个 bit 移出(BSY=0),此时拉高片选器件才不会截断数据 */
    u32TimeStart = HAL_GetTick();
    while (__HAL_SPI_GET_FLAG(pDev->pHandle, SPI_FLAG_BSY) != RESET)
    {
        if ((HAL_GetTick() - u32TimeStart) > SPI_MASTER_XFER_TIMEOUT_MS)
        {
            SpiDevDeselect(pDev);
            return SPI_DEV_ERR_TIMEOUT;
        }
    }

    SpiDevDeselect(pDev);
    return SPI_DEV_OK;
}

/*----------------------------------------------------------------------------
 * 数据收发
 *----------------------------------------------------------------------------*/

int32_t SpiDevWrite(SpiDevice_t  *pDev,
                    const uint8_t *pTxBuf,
                    uint16_t       u16Len,
                    uint32_t       u32TimeoutMs)
{
    HAL_StatusTypeDef eStatus;
    uint32_t          u32Timeout;
    int32_t           s32Result;

    if ((pDev == NULL) || (pDev->pHandle == NULL) ||
        (pTxBuf == NULL) || (u16Len == 0u))
    {
        return SPI_DEV_ERR_PARAM;
    }

    u32Timeout = SpiDevTimeoutOrDefault(u32TimeoutMs);

#if (SPI_MASTER_USE_DMA != 0)
    if (u16Len >= SPI_MASTER_DMA_MIN_LEN)
    {
        eStatus = HAL_SPI_Transmit_DMA(pDev->pHandle, pTxBuf, u16Len);
    }
    else
#endif
    {
        eStatus = HAL_SPI_Transmit(pDev->pHandle, pTxBuf, u16Len, u32Timeout);
    }

    if (eStatus != HAL_OK)
    {
        return SPI_DEV_ERR_HAL;
    }

    s32Result = SpiDevWaitReady(pDev, u32Timeout);

    /* 只写不读的帧必然产生 OVR,这里统一清掉 */
    SpiDevClearOverrun(pDev->pHandle);

    if (s32Result != SPI_DEV_OK)
    {
        return s32Result;
    }
    return SpiDevCheckResult(pDev);
}

int32_t SpiDevRead(SpiDevice_t *pDev,
                   uint8_t     *pRxBuf,
                   uint16_t     u16Len,
                   uint32_t     u32TimeoutMs)
{
    HAL_StatusTypeDef eStatus;
    uint32_t          u32Timeout;
    int32_t           s32Result;

    if ((pDev == NULL) || (pDev->pHandle == NULL) ||
        (pRxBuf == NULL) || (u16Len == 0u))
    {
        return SPI_DEV_ERR_PARAM;
    }

    u32Timeout = SpiDevTimeoutOrDefault(u32TimeoutMs);

#if (SPI_MASTER_USE_DMA != 0)
    if (u16Len >= SPI_MASTER_DMA_MIN_LEN)
    {
        /*
         * 全双工主机模式下,接收必须同时发数据才能产生时钟。
         * HAL_SPI_Receive_DMA 内部会用同一个缓冲区做 dummy 发送,
         * MOSI 上的内容对 W5500 无效,因此不需要额外准备哑元缓冲区。
         */
        eStatus = HAL_SPI_Receive_DMA(pDev->pHandle, pRxBuf, u16Len);
    }
    else
#endif
    {
        eStatus = HAL_SPI_Receive(pDev->pHandle, pRxBuf, u16Len, u32Timeout);
    }

    if (eStatus != HAL_OK)
    {
        return SPI_DEV_ERR_HAL;
    }

    s32Result = SpiDevWaitReady(pDev, u32Timeout);
    if (s32Result != SPI_DEV_OK)
    {
        return s32Result;
    }
    return SpiDevCheckResult(pDev);
}

int32_t SpiDevWriteRead(SpiDevice_t   *pDev,
                        const uint8_t *pTxBuf,
                        uint8_t       *pRxBuf,
                        uint16_t       u16Len,
                        uint32_t       u32TimeoutMs)
{
    HAL_StatusTypeDef eStatus;
    uint32_t          u32Timeout;
    int32_t           s32Result;

    if ((pDev == NULL) || (pDev->pHandle == NULL) ||
        (pTxBuf == NULL) || (pRxBuf == NULL) || (u16Len == 0u))
    {
        return SPI_DEV_ERR_PARAM;
    }

    u32Timeout = SpiDevTimeoutOrDefault(u32TimeoutMs);

#if (SPI_MASTER_USE_DMA != 0)
    if (u16Len >= SPI_MASTER_DMA_MIN_LEN)
    {
        eStatus = HAL_SPI_TransmitReceive_DMA(pDev->pHandle, pTxBuf, pRxBuf, u16Len);
    }
    else
#endif
    {
        eStatus = HAL_SPI_TransmitReceive(pDev->pHandle, pTxBuf, pRxBuf,
                                          u16Len, u32Timeout);
    }

    if (eStatus != HAL_OK)
    {
        return SPI_DEV_ERR_HAL;
    }

    s32Result = SpiDevWaitReady(pDev, u32Timeout);
    if (s32Result != SPI_DEV_OK)
    {
        return s32Result;
    }
    return SpiDevCheckResult(pDev);
}

/*----------------------------------------------------------------------------
 * 状态查询
 *----------------------------------------------------------------------------*/

uint8_t SpiDevIsBusy(SpiDevice_t *pDev)
{
    if ((pDev == NULL) || (pDev->pHandle == NULL))
    {
        return 1u;
    }
    if (pDev->pHandle->State != HAL_SPI_STATE_READY)
    {
        return 1u;
    }
    if (__HAL_SPI_GET_FLAG(pDev->pHandle, SPI_FLAG_BSY) != RESET)
    {
        return 1u;
    }
    return 0u;
}

void SpiDevFlush(SpiDevice_t *pDev)
{
    if ((pDev == NULL) || (pDev->pHandle == NULL))
    {
        return;
    }
    SpiDevClearOverrun(pDev->pHandle);
}
