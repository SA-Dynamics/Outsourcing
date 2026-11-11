/**
 ******************************************************************************
 * @file    W5500.c
 * @brief   W5500 硬件 TCP/IP 控制器驱动实现
 * @author  user
 * @date    2026-11-11
 *
 * @note    本文件不出现任何 SPI 外设名与引脚号:
 *          SPI 句柄、CS、RST 全部由 W5500_Config_t 在 W5500_Init 时注入,
 *          底层收发统一走 SPI_MasterFullDuplexDMA 的 SpiDev* 接口。
 *
 * @note    W5500 的 SPI 帧格式:
 *            [地址高][地址低][控制字节][数据 ...]
 *          控制字节 = BSB(块选择) | RWB(读写) | OM(长度模式)
 *          一次片选有效期间的所有字节属于同一帧,地址在帧内自动递增。
 ******************************************************************************
 */

#include "W5500.h"

/*----------------------------------------------------------------------------
 * 可调参数
 *----------------------------------------------------------------------------*/
#define W5500_CONNECT_TIMEOUT_MS    (3000u)     /**< TCP 连接建立超时              */
#define W5500_SEND_OK_TIMEOUT_MS    (1000u)     /**< 等 SEND_OK 中断的超时         */
#define W5500_RESET_DONE_TIMEOUT_MS (100u)      /**< 等软复位完成(MR.RST 自清)   */

/*----------------------------------------------------------------------------
 * 私有变量
 *----------------------------------------------------------------------------*/
static SpiDevice_t g_sSpiDev;                   /**< SPI 设备描述(运行期绑定)     */
static uint8_t     g_u8Inited = 0u;             /**< 初始化完成标志                */
static GPIO_TypeDef *g_pIntPort = NULL;         /**< INT 引脚端口,未接则为 NULL    */
static uint16_t    g_u16IntPin = 0u;            /**< INT 引脚                      */
static volatile uint8_t g_u8IntFlag = 0u;       /**< EXTI 置位的中断标志            */
static uint8_t     g_au8Event[W5500_SOCKET_COUNT]; /**< 各 Socket 累计事件位      */

/* 配置留空时使用的默认网络参数 */
static const uint8_t g_au8DefaultMac[6]  = {0x00u, 0x08u, 0xDCu, 0x12u, 0x34u, 0x56u};
static const uint8_t g_au8DefaultIp[4]   = {192u, 168u, 2u, 199u};
static const uint8_t g_au8DefaultGw[4]   = {192u, 168u, 2u, 1u};
static const uint8_t g_au8DefaultMask[4] = {255u, 255u, 255u, 0u};

/*----------------------------------------------------------------------------
 * 私有函数原型
 *----------------------------------------------------------------------------*/
static int32_t W5500_FrameWrite(uint8_t u8Block, uint16_t u16Addr,
                                const uint8_t *pData, uint16_t u16Len);
static int32_t W5500_FrameRead(uint8_t u8Block, uint16_t u16Addr,
                               uint8_t *pBuf, uint16_t u16Len);
static int32_t W5500_WaitCommandDone(uint8_t u8Sock, uint32_t u32TimeoutMs);
static int32_t W5500_WaitSocketStatus(uint8_t u8Sock, uint8_t u8Status,
                                      uint32_t u32TimeoutMs);
static int32_t W5500_WaitTxFree(uint8_t u8Sock, uint16_t u16Len,
                                uint32_t u32TimeoutMs);
static void    W5500_ReadSocketBuf(uint8_t u8Sock, uint16_t u16Offset,
                                   uint8_t *pBuf, uint16_t u16Len);
static void    W5500_WriteSocketBuf(uint8_t u8Sock, uint16_t u16Offset,
                                    const uint8_t *pBuf, uint16_t u16Len);
static uint8_t W5500_IsAllZero(const uint8_t *pData, uint8_t u8Len);

/*----------------------------------------------------------------------------
 * 帧级收发:一帧 = 片选有效 + 3 字节命令头 + 数据 + 片选释放
 *----------------------------------------------------------------------------*/

/**
 * @brief  写帧
 * @param  u8Block 块选择码(BSB),如 W5500_BSB_COMMON / W5500_BSB_SOCK_REG(n)
 * @param  u16Addr 块内偏移
 * @param  pData   数据,可为 NULL(仅发命令头)
 * @param  u16Len  数据字节数
 * @return W5500_OK / W5500_ERR_SPI
 */
static int32_t W5500_FrameWrite(uint8_t u8Block, uint16_t u16Addr,
                                const uint8_t *pData, uint16_t u16Len)
{
    uint8_t aHeader[3];
    int32_t s32Result;

    aHeader[0] = (uint8_t)(u16Addr >> 8);
    aHeader[1] = (uint8_t)(u16Addr & 0xFFu);
    aHeader[2] = (uint8_t)(u8Block | W5500_RWB_WRITE | W5500_OM_VDM);

    s32Result = SpiDevBegin(&g_sSpiDev);
    if (s32Result != SPI_DEV_OK)
    {
        return W5500_ERR_SPI;
    }

    s32Result = SpiDevWrite(&g_sSpiDev, aHeader, 3u, 0u);
    if ((s32Result == SPI_DEV_OK) && (pData != NULL) && (u16Len > 0u))
    {
        s32Result = SpiDevWrite(&g_sSpiDev, pData, u16Len, 0u);
    }

    if (SpiDevEnd(&g_sSpiDev) != SPI_DEV_OK)
    {
        s32Result = SPI_DEV_ERR_TIMEOUT;
    }

    return (s32Result == SPI_DEV_OK) ? W5500_OK : W5500_ERR_SPI;
}

/**
 * @brief  读帧
 * @param  u8Block 块选择码(BSB)
 * @param  u16Addr 块内偏移
 * @param  pBuf    接收缓冲区
 * @param  u16Len  读取字节数
 * @return W5500_OK / W5500_ERR_SPI
 * @note   命令头发出后 W5500 立即开始输出数据,因此不需要额外的哑元字节;
 *         读数据段由 SpiDevRead 通过 DMA 完成,CPU 只在等待结束。
 */
static int32_t W5500_FrameRead(uint8_t u8Block, uint16_t u16Addr,
                               uint8_t *pBuf, uint16_t u16Len)
{
    uint8_t aHeader[3];
    int32_t s32Result;

    aHeader[0] = (uint8_t)(u16Addr >> 8);
    aHeader[1] = (uint8_t)(u16Addr & 0xFFu);
    aHeader[2] = (uint8_t)(u8Block | W5500_RWB_READ | W5500_OM_VDM);

    s32Result = SpiDevBegin(&g_sSpiDev);
    if (s32Result != SPI_DEV_OK)
    {
        return W5500_ERR_SPI;
    }

    s32Result = SpiDevWrite(&g_sSpiDev, aHeader, 3u, 0u);
    if ((s32Result == SPI_DEV_OK) && (pBuf != NULL) && (u16Len > 0u))
    {
        s32Result = SpiDevRead(&g_sSpiDev, pBuf, u16Len, 0u);
    }

    if (SpiDevEnd(&g_sSpiDev) != SPI_DEV_OK)
    {
        s32Result = SPI_DEV_ERR_TIMEOUT;
    }

    return (s32Result == SPI_DEV_OK) ? W5500_OK : W5500_ERR_SPI;
}

/*----------------------------------------------------------------------------
 * 寄存器访问
 *----------------------------------------------------------------------------*/

uint8_t W5500_Read8(uint8_t u8Block, uint16_t u16Addr)
{
    uint8_t u8Value = 0u;

    if (W5500_FrameRead(u8Block, u16Addr, &u8Value, 1u) != W5500_OK)
    {
        return 0u;
    }
    return u8Value;
}

void W5500_Write8(uint8_t u8Block, uint16_t u16Addr, uint8_t u8Data)
{
    (void)W5500_FrameWrite(u8Block, u16Addr, &u8Data, 1u);
}

/**
 * @brief  读 16 位寄存器(大端:高字节在低地址)
 * @note   W5500 的 16 位寄存器支持变长模式连续读两个字节,无需切 FDM2。
 */
uint16_t W5500_Read16(uint8_t u8Block, uint16_t u16Addr)
{
    uint8_t aBuf[2] = {0u, 0u};

    if (W5500_FrameRead(u8Block, u16Addr, aBuf, 2u) != W5500_OK)
    {
        return 0u;
    }
    return (uint16_t)(((uint16_t)aBuf[0] << 8) | (uint16_t)aBuf[1]);
}

void W5500_Write16(uint8_t u8Block, uint16_t u16Addr, uint16_t u16Data)
{
    uint8_t aBuf[2];

    aBuf[0] = (uint8_t)(u16Data >> 8);
    aBuf[1] = (uint8_t)(u16Data & 0xFFu);
    (void)W5500_FrameWrite(u8Block, u16Addr, aBuf, 2u);
}

void W5500_ReadBuf(uint8_t u8Block, uint16_t u16Addr, uint8_t *pBuf, uint16_t u16Len)
{
    if ((pBuf == NULL) || (u16Len == 0u))
    {
        return;
    }
    (void)W5500_FrameRead(u8Block, u16Addr, pBuf, u16Len);
}

void W5500_WriteBuf(uint8_t u8Block, uint16_t u16Addr,
                    const uint8_t *pBuf, uint16_t u16Len)
{
    if ((pBuf == NULL) || (u16Len == 0u))
    {
        return;
    }
    (void)W5500_FrameWrite(u8Block, u16Addr, pBuf, u16Len);
}

/*----------------------------------------------------------------------------
 * 状态等待
 *----------------------------------------------------------------------------*/

/**
 * @brief  等 Socket 命令执行完毕(Sn_CR 由硬件自动清零)
 */
static int32_t W5500_WaitCommandDone(uint8_t u8Sock, uint32_t u32TimeoutMs)
{
    uint32_t u32TimeStart = HAL_GetTick();
    uint8_t  u8Block = W5500_BSB_SOCK_REG(u8Sock);

    while (W5500_Read8(u8Block, W5500_SN_CR) != 0u)
    {
        if ((HAL_GetTick() - u32TimeStart) > u32TimeoutMs)
        {
            return W5500_ERR_TIMEOUT;
        }
    }
    return W5500_OK;
}

/**
 * @brief  等 Socket 状态变为指定值
 */
static int32_t W5500_WaitSocketStatus(uint8_t u8Sock, uint8_t u8Status,
                                      uint32_t u32TimeoutMs)
{
    uint32_t u32TimeStart = HAL_GetTick();

    while (W5500_SocketStatus(u8Sock) != u8Status)
    {
        if ((HAL_GetTick() - u32TimeStart) > u32TimeoutMs)
        {
            return W5500_ERR_TIMEOUT;
        }
    }
    return W5500_OK;
}

/**
 * @brief  等发送缓冲区出现 u16Len 字节空闲
 */
static int32_t W5500_WaitTxFree(uint8_t u8Sock, uint16_t u16Len, uint32_t u32TimeoutMs)
{
    uint32_t u32TimeStart = HAL_GetTick();
    uint8_t  u8Block = W5500_BSB_SOCK_REG(u8Sock);

    while (W5500_Read16(u8Block, W5500_SN_TX_FSR) < u16Len)
    {
        if ((HAL_GetTick() - u32TimeStart) > u32TimeoutMs)
        {
            return W5500_ERR_TIMEOUT;
        }
    }
    return W5500_OK;
}

/*----------------------------------------------------------------------------
 * Socket 缓冲区读写(处理 2KB 环形回卷)
 *----------------------------------------------------------------------------*/

/**
 * @brief  从 Socket 接收缓冲区读数据,自动处理跨越缓冲区末尾的情况
 * @param  u16Offset 缓冲区内的起始偏移(已按缓冲大小取模)
 */
static void W5500_ReadSocketBuf(uint8_t u8Sock, uint16_t u16Offset,
                                uint8_t *pBuf, uint16_t u16Len)
{
    uint16_t u16First;
    uint8_t  u8Block = W5500_BSB_SOCK_RX(u8Sock);

    /* 距离缓冲区末尾还剩多少字节 */
    u16First = (uint16_t)(W5500_SOCKET_BUF_SIZE - u16Offset);

    if (u16First >= u16Len)
    {
        W5500_ReadBuf(u8Block, u16Offset, pBuf, u16Len);
        return;
    }

    /* 先读完尾部,再从 0 偏移继续读剩余部分 */
    W5500_ReadBuf(u8Block, u16Offset, pBuf, u16First);
    W5500_ReadBuf(u8Block, 0u, &pBuf[u16First], (uint16_t)(u16Len - u16First));
}

/**
 * @brief  向 Socket 发送缓冲区写数据,自动处理跨越缓冲区末尾的情况
 */
static void W5500_WriteSocketBuf(uint8_t u8Sock, uint16_t u16Offset,
                                 const uint8_t *pBuf, uint16_t u16Len)
{
    uint16_t u16First;
    uint8_t  u8Block = W5500_BSB_SOCK_TX(u8Sock);

    u16First = (uint16_t)(W5500_SOCKET_BUF_SIZE - u16Offset);

    if (u16First >= u16Len)
    {
        W5500_WriteBuf(u8Block, u16Offset, pBuf, u16Len);
        return;
    }

    W5500_WriteBuf(u8Block, u16Offset, pBuf, u16First);
    W5500_WriteBuf(u8Block, 0u, &pBuf[u16First], (uint16_t)(u16Len - u16First));
}

/**
 * @brief  判断一段数据是否全 0
 * @note   用于识别调用方没填网络参数的情况,此时改用默认值。
 */
static uint8_t W5500_IsAllZero(const uint8_t *pData, uint8_t u8Len)
{
    uint8_t i;

    for (i = 0u; i < u8Len; i++)
    {
        if (pData[i] != 0u)
        {
            return 0u;
        }
    }
    return 1u;
}

/*----------------------------------------------------------------------------
 * 初始化
 *----------------------------------------------------------------------------*/

uint8_t W5500_ReadVersion(void)
{
    return W5500_Read8(W5500_BSB_COMMON, W5500_VERSIONR);
}

uint8_t W5500_IsReady(void)
{
    return g_u8Inited;
}

int32_t W5500_SoftReset(void)
{
    uint32_t u32TimeStart;

    /* 写 1 触发软复位,完成后硬件自动清零 */
    W5500_Write8(W5500_BSB_COMMON, W5500_MR, W5500_MR_RST);

    u32TimeStart = HAL_GetTick();
    while ((W5500_Read8(W5500_BSB_COMMON, W5500_MR) & W5500_MR_RST) != 0u)
    {
        if ((HAL_GetTick() - u32TimeStart) > W5500_RESET_DONE_TIMEOUT_MS)
        {
            return W5500_ERR_TIMEOUT;
        }
    }
    return W5500_OK;
}

uint8_t W5500_IsLinked(void)
{
    if (W5500_Read8(W5500_BSB_COMMON, W5500_PHYCFGR) & W5500_PHY_LINK)
    {
        return 1u;
    }
    return 0u;
}

int32_t W5500_WaitLink(uint32_t u32TimeoutMs)
{
    uint32_t u32TimeStart;

    if (u32TimeoutMs == 0u)
    {
        u32TimeoutMs = W5500_LINK_TIMEOUT_MS;
    }

    u32TimeStart = HAL_GetTick();
    while (W5500_IsLinked() == 0u)
    {
        if ((HAL_GetTick() - u32TimeStart) > u32TimeoutMs)
        {
            return W5500_ERR_NOLINK;
        }
    }
    return W5500_OK;
}

/*----------------------------------------------------------------------------
 * INT 引脚中断
 *----------------------------------------------------------------------------*/

uint8_t W5500_IntIsAsserted(void)
{
    /* EXTI 置过的标志也要算:W5500 的 INT 是电平输出,漏掉的下降沿
     * 靠这里补上,否则事件会丢 */
    if (g_u8IntFlag != 0u)
    {
        return 1u;
    }

    if (g_pIntPort == NULL)
    {
        return 0u;
    }

    /* W5500 的 INT 为低电平有效 */
    if (HAL_GPIO_ReadPin(g_pIntPort, g_u16IntPin) == GPIO_PIN_RESET)
    {
        return 1u;
    }
    return 0u;
}

uint8_t W5500_ProcessInt(void)
{
    uint8_t u8Sir;
    uint8_t u8Ir;
    uint8_t u8Count = 0u;
    uint8_t i;

    if (g_u8Inited == 0u)
    {
        return 0u;
    }

    g_u8IntFlag = 0u;

    u8Sir = W5500_Read8(W5500_BSB_COMMON, W5500_SIR);
    if (u8Sir == 0u)
    {
        return 0u;
    }

    for (i = 0u; i < W5500_SOCKET_COUNT; i++)
    {
        if ((u8Sir & (uint8_t)(1u << i)) != 0u)
        {
            /* 读 Sn_IR 并写回清零,该 Socket 的 INT 源随之撤销 */
            u8Ir = W5500_SocketClearIr(i);
            g_au8Event[i] |= u8Ir;
            u8Count++;
        }
    }

    /* SIR 写 1 清零 */
    W5500_Write8(W5500_BSB_COMMON, W5500_SIR, u8Sir);

    return u8Count;
}

uint8_t W5500_GetEvent(uint8_t u8Sock)
{
    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return 0u;
    }
    return g_au8Event[u8Sock];
}

/**
 * @brief  EXTI 回调(HAL 弱符号重载):只置标志,绝不在中断上下文访问 SPI
 * @note   本工程无其它 EXTI 引脚时可直接使用;若应用层还有别的 EXTI 中断,
 *         请在应用层重写本回调并自行分发(W5500IntSetFlag 可供置位)。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if ((g_pIntPort != NULL) && (GPIO_Pin == g_u16IntPin))
    {
        g_u8IntFlag = 1u;
    }
}

void W5500_ClearEvent(uint8_t u8Sock, uint8_t u8Mask)
{
    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return;
    }
    g_au8Event[u8Sock] &= (uint8_t)(~u8Mask);
}

int32_t W5500_SocketIntEnable(uint8_t u8Sock, uint8_t u8Enable)
{
    uint8_t u8Simr;

    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return W5500_ERR_PARAM;
    }

    u8Simr = W5500_Read8(W5500_BSB_COMMON, W5500_SIMR);

    if (u8Enable != 0u)
    {
        u8Simr |= (uint8_t)(1u << u8Sock);
        /* 打开该 Socket 的全部事件源 */
        W5500_Write8(W5500_BSB_SOCK_REG(u8Sock), W5500_SN_IMR, W5500_SN_IMR_ALL);
    }
    else
    {
        u8Simr &= (uint8_t)(~(1u << u8Sock));
    }

    W5500_Write8(W5500_BSB_COMMON, W5500_SIMR, u8Simr);

    return W5500_OK;
}

int32_t W5500_Init(const W5500_Config_t *pConfig)
{
    const uint8_t *pMac;
    const uint8_t *pIp;
    const uint8_t *pGw;
    const uint8_t *pMask;
    uint8_t u8Try;
    uint8_t i;
    int32_t s32Result;

    if ((pConfig == NULL) || (pConfig->pSpi == NULL) || (pConfig->pCsPort == NULL))
    {
        return W5500_ERR_PARAM;
    }

    g_u8Inited = 0u;

    /* 1. 绑定 SPI 设备,先把片选释放(W5500 低电平为选中) */
    if (SpiDevAttach(&g_sSpiDev, pConfig->pSpi,
                     pConfig->pCsPort, pConfig->CsPin) != SPI_DEV_OK)
    {
        return W5500_ERR_PARAM;
    }
    SpiDevSetCsActiveLevel(&g_sSpiDev, 0u);
    SpiDevDeselect(&g_sSpiDev);

    /* 2. 硬件复位(接了 RST 引脚才执行) */
    if (pConfig->pRstPort != NULL)
    {
        HAL_GPIO_WritePin(pConfig->pRstPort, pConfig->RstPin, GPIO_PIN_RESET);
        HAL_Delay(W5500_RESET_LOW_MS);
        HAL_GPIO_WritePin(pConfig->pRstPort, pConfig->RstPin, GPIO_PIN_SET);
        HAL_Delay(W5500_RESET_HIGH_MS);
    }

    /* 3. 探测芯片:VERSIONR 必须等于 0x04,否则说明 SPI 或片选有问题 */
    s32Result = W5500_ERR_NO_DEVICE;
    for (u8Try = 0u; u8Try < 5u; u8Try++)
    {
        if (W5500_ReadVersion() == W5500_VERSION_REG_VALUE)
        {
            s32Result = W5500_OK;
            break;
        }
        HAL_Delay(2u);
    }
    if (s32Result != W5500_OK)
    {
        return s32Result;
    }

    /* 4. 软件复位,确保寄存器回到默认值 */
    if (W5500_SoftReset() != W5500_OK)
    {
        return W5500_ERR_TIMEOUT;
    }
    HAL_Delay(W5500_RESET_HIGH_MS);

    /* 5. 写入网络参数(未填则用默认值) */
    pMac  = (W5500_IsAllZero(pConfig->Mac, 6u) != 0u) ? g_au8DefaultMac : pConfig->Mac;
    pIp   = (W5500_IsAllZero(pConfig->Ip, 4u) != 0u) ? g_au8DefaultIp : pConfig->Ip;
    pGw   = (W5500_IsAllZero(pConfig->Gateway, 4u) != 0u) ? g_au8DefaultGw : pConfig->Gateway;
    pMask = (W5500_IsAllZero(pConfig->Subnet, 4u) != 0u) ? g_au8DefaultMask : pConfig->Subnet;

    W5500_WriteBuf(W5500_BSB_COMMON, W5500_SHAR, pMac, 6u);
    W5500_WriteBuf(W5500_BSB_COMMON, W5500_GAR, pGw, 4u);
    W5500_WriteBuf(W5500_BSB_COMMON, W5500_SUBR, pMask, 4u);
    W5500_WriteBuf(W5500_BSB_COMMON, W5500_SIPR, pIp, 4u);

    /* 6. 8 个 Socket 的收/发缓冲区各分配 2KB(芯片共 16KB + 16KB) */
    for (i = 0u; i < W5500_SOCKET_COUNT; i++)
    {
        W5500_Write8(W5500_BSB_SOCK_REG(i), W5500_SN_RXBUF_SIZE, 2u);
        W5500_Write8(W5500_BSB_SOCK_REG(i), W5500_SN_TXBUF_SIZE, 2u);
    }

    /* 7. 重传参数:200ms 超时 × 8 次 */
    W5500_Write16(W5500_BSB_COMMON, W5500_RTR, 2000u);
    W5500_Write8(W5500_BSB_COMMON, W5500_RCR, 8u);

    /* 8. INT 引脚:接了才放开中断,否则保持全屏蔽避免 INT 引脚悬空抖动 */
    g_pIntPort  = pConfig->pIntPort;
    g_u16IntPin = pConfig->IntPin;
    g_u8IntFlag = 0u;

    W5500_Write8(W5500_BSB_COMMON, W5500_IMR, 0x00u);
    W5500_Write8(W5500_BSB_COMMON, W5500_SIMR, 0x00u);
    W5500_Write8(W5500_BSB_COMMON, W5500_SIR, 0xFFu);
    W5500_Write8(W5500_BSB_COMMON, W5500_IR, 0xFFu);

    for (i = 0u; i < W5500_SOCKET_COUNT; i++)
    {
        g_au8Event[i] = 0u;
    }

    g_u8Inited = 1u;

    return W5500_OK;
}

/*----------------------------------------------------------------------------
 * Socket 操作
 *----------------------------------------------------------------------------*/

uint8_t W5500_SocketStatus(uint8_t u8Sock)
{
    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return W5500_SR_CLOSED;
    }
    return W5500_Read8(W5500_BSB_SOCK_REG(u8Sock), W5500_SN_SR);
}

uint8_t W5500_SocketClearIr(uint8_t u8Sock)
{
    uint8_t u8Ir;

    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return 0u;
    }

    u8Ir = W5500_Read8(W5500_BSB_SOCK_REG(u8Sock), W5500_SN_IR);
    if (u8Ir != 0u)
    {
        /* Sn_IR 写 1 清零 */
        W5500_Write8(W5500_BSB_SOCK_REG(u8Sock), W5500_SN_IR, u8Ir);
    }
    return u8Ir;
}

int32_t W5500_SocketOpen(uint8_t u8Sock, uint8_t u8Protocol, uint16_t u16Port)
{
    uint8_t u8Block;

    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return W5500_ERR_PARAM;
    }
    if ((u8Protocol != W5500_PROTO_TCP) &&
        (u8Protocol != W5500_PROTO_UDP) &&
        (u8Protocol != W5500_PROTO_MACRAW))
    {
        return W5500_ERR_PARAM;
    }

    u8Block = W5500_BSB_SOCK_REG(u8Sock);

    /* 先关掉可能残留的连接 */
    W5500_SocketClose(u8Sock);

    W5500_Write8(u8Block, W5500_SN_MR, u8Protocol);
    if (u16Port != 0u)
    {
        W5500_Write16(u8Block, W5500_SN_PORT, u16Port);
    }

    W5500_Write8(u8Block, W5500_SN_CR, W5500_CR_OPEN);
    if (W5500_WaitCommandDone(u8Sock, W5500_CMD_TIMEOUT_MS) != W5500_OK)
    {
        return W5500_ERR_TIMEOUT;
    }

    if (W5500_SocketStatus(u8Sock) == W5500_SR_CLOSED)
    {
        return W5500_ERR_STATE;
    }

    /* 接了 INT 引脚才打开该 Socket 的中断上报 */
    g_au8Event[u8Sock] = 0u;
    if (g_pIntPort != NULL)
    {
        (void)W5500_SocketIntEnable(u8Sock, 1u);
    }

    return W5500_OK;
}

int32_t W5500_SocketListen(uint8_t u8Sock)
{
    uint8_t u8Block;

    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return W5500_ERR_PARAM;
    }

    u8Block = W5500_BSB_SOCK_REG(u8Sock);

    if (W5500_SocketStatus(u8Sock) != W5500_SR_INIT)
    {
        return W5500_ERR_STATE;
    }

    W5500_Write8(u8Block, W5500_SN_CR, W5500_CR_LISTEN);
    if (W5500_WaitCommandDone(u8Sock, W5500_CMD_TIMEOUT_MS) != W5500_OK)
    {
        return W5500_ERR_TIMEOUT;
    }

    return W5500_WaitSocketStatus(u8Sock, W5500_SR_LISTEN, W5500_CMD_TIMEOUT_MS);
}

int32_t W5500_SocketConnect(uint8_t u8Sock, const uint8_t pDstIp[4], uint16_t u16DstPort)
{
    uint32_t u32TimeStart;
    uint8_t  u8Block;
    uint8_t  u8Ir;

    if ((u8Sock >= W5500_SOCKET_COUNT) || (pDstIp == NULL))
    {
        return W5500_ERR_PARAM;
    }

    u8Block = W5500_BSB_SOCK_REG(u8Sock);

    W5500_WriteBuf(u8Block, W5500_SN_DIPR, pDstIp, 4u);
    W5500_Write16(u8Block, W5500_SN_DPORT, u16DstPort);

    (void)W5500_SocketClearIr(u8Sock);

    W5500_Write8(u8Block, W5500_SN_CR, W5500_CR_CONNECT);
    if (W5500_WaitCommandDone(u8Sock, W5500_CMD_TIMEOUT_MS) != W5500_OK)
    {
        return W5500_ERR_TIMEOUT;
    }

    /* 握手由芯片完成,这里等状态迁移或超时中断 */
    u32TimeStart = HAL_GetTick();
    while (W5500_SocketStatus(u8Sock) != W5500_SR_ESTABLISHED)
    {
        u8Ir = W5500_SocketClearIr(u8Sock);
        if ((u8Ir & W5500_SN_IR_TIMEOUT) != 0u)
        {
            return W5500_ERR_TIMEOUT;
        }
        if (W5500_SocketStatus(u8Sock) == W5500_SR_CLOSED)
        {
            return W5500_ERR_STATE;
        }
        if ((HAL_GetTick() - u32TimeStart) > W5500_CONNECT_TIMEOUT_MS)
        {
            return W5500_ERR_TIMEOUT;
        }
    }

    return W5500_OK;
}

void W5500_SocketClose(uint8_t u8Sock)
{
    uint8_t u8Block;

    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return;
    }

    u8Block = W5500_BSB_SOCK_REG(u8Sock);

    /* 先关中断上报,避免关闭过程中 INT 反复拉低 */
    (void)W5500_SocketIntEnable(u8Sock, 0u);

    W5500_Write8(u8Block, W5500_SN_CR, W5500_CR_CLOSE);
    (void)W5500_WaitCommandDone(u8Sock, W5500_CMD_TIMEOUT_MS);
    (void)W5500_WaitSocketStatus(u8Sock, W5500_SR_CLOSED, W5500_CMD_TIMEOUT_MS);

    /* 清掉关闭过程中产生的标志,避免影响下一次连接 */
    W5500_Write8(u8Block, W5500_SN_IR, 0xFFu);
    g_au8Event[u8Sock] = 0u;
}

uint16_t W5500_SocketRecvSize(uint8_t u8Sock)
{
    if (u8Sock >= W5500_SOCKET_COUNT)
    {
        return 0u;
    }
    return W5500_Read16(W5500_BSB_SOCK_REG(u8Sock), W5500_SN_RX_RSR);
}

int32_t W5500_SocketSend(uint8_t u8Sock, const uint8_t *pData, uint16_t u16Len)
{
    uint32_t u32TimeStart;
    uint16_t u16Wr;
    uint16_t u16Offset;
    uint8_t  u8Block;
    uint8_t  u8Status;
    uint8_t  u8Ir;

    if ((u8Sock >= W5500_SOCKET_COUNT) || (pData == NULL) || (u16Len == 0u))
    {
        return W5500_ERR_PARAM;
    }
    if (u16Len > W5500_SOCKET_BUF_SIZE)
    {
        return W5500_ERR_PARAM;
    }

    u8Block  = W5500_BSB_SOCK_REG(u8Sock);
    u8Status = W5500_SocketStatus(u8Sock);
    if ((u8Status != W5500_SR_ESTABLISHED) &&
        (u8Status != W5500_SR_CLOSE_WAIT) &&
        (u8Status != W5500_SR_UDP))
    {
        return W5500_ERR_STATE;
    }

    /* 等发送缓冲区腾出足够空间 */
    if (W5500_WaitTxFree(u8Sock, u16Len, W5500_TXFREE_TIMEOUT_MS) != W5500_OK)
    {
        return W5500_ERR_NO_MEM;
    }

    /* 数据写入发送缓冲区,再推进写指针 */
    u16Wr     = W5500_Read16(u8Block, W5500_SN_TX_WR);
    u16Offset = (uint16_t)(u16Wr & W5500_SOCKET_BUF_MASK);
    W5500_WriteSocketBuf(u8Sock, u16Offset, pData, u16Len);
    W5500_Write16(u8Block, W5500_SN_TX_WR, (uint16_t)(u16Wr + u16Len));

    /* 清旧的 SEND_OK 后下发送命令 */
    W5500_Write8(u8Block, W5500_SN_IR, W5500_SN_IR_SENDOK);
    W5500_Write8(u8Block, W5500_SN_CR, W5500_CR_SEND);
    if (W5500_WaitCommandDone(u8Sock, W5500_CMD_TIMEOUT_MS) != W5500_OK)
    {
        return W5500_ERR_TIMEOUT;
    }

    /* UDP 发送完成快;TCP 要等对端 ACK,期间可能触发重传 */
    u32TimeStart = HAL_GetTick();
    while (1)
    {
        u8Ir = W5500_SocketClearIr(u8Sock);
        if ((u8Ir & W5500_SN_IR_SENDOK) != 0u)
        {
            return (int32_t)u16Len;
        }
        if ((u8Ir & W5500_SN_IR_TIMEOUT) != 0u)
        {
            return W5500_ERR_TIMEOUT;
        }
        if ((HAL_GetTick() - u32TimeStart) > W5500_SEND_OK_TIMEOUT_MS)
        {
            return W5500_ERR_TIMEOUT;
        }
    }
}

int32_t W5500_SocketSendTo(uint8_t u8Sock, const uint8_t pDstIp[4],
                           uint16_t u16DstPort, const uint8_t *pData, uint16_t u16Len)
{
    uint8_t u8Block;

    if ((u8Sock >= W5500_SOCKET_COUNT) || (pDstIp == NULL))
    {
        return W5500_ERR_PARAM;
    }

    u8Block = W5500_BSB_SOCK_REG(u8Sock);

    /* UDP 无连接:每次发送前指定目标地址 */
    W5500_WriteBuf(u8Block, W5500_SN_DIPR, pDstIp, 4u);
    W5500_Write16(u8Block, W5500_SN_DPORT, u16DstPort);

    return W5500_SocketSend(u8Sock, pData, u16Len);
}

int32_t W5500_SocketRecv(uint8_t u8Sock, uint8_t *pBuf, uint16_t u16MaxLen)
{
    uint16_t u16Size;
    uint16_t u16Len;
    uint16_t u16Rd;
    uint16_t u16Offset;
    uint8_t  u8Block;

    if ((u8Sock >= W5500_SOCKET_COUNT) || (pBuf == NULL) || (u16MaxLen == 0u))
    {
        return W5500_ERR_PARAM;
    }

    u8Block = W5500_BSB_SOCK_REG(u8Sock);

    u16Size = W5500_Read16(u8Block, W5500_SN_RX_RSR);
    if (u16Size == 0u)
    {
        return 0;
    }

    u16Len = (u16Size > u16MaxLen) ? u16MaxLen : u16Size;

    /* 读数据 + 推进读指针 + 通知芯片已取走(RECV) */
    u16Rd     = W5500_Read16(u8Block, W5500_SN_RX_RD);
    u16Offset = (uint16_t)(u16Rd & W5500_SOCKET_BUF_MASK);
    W5500_ReadSocketBuf(u8Sock, u16Offset, pBuf, u16Len);
    W5500_Write16(u8Block, W5500_SN_RX_RD, (uint16_t)(u16Rd + u16Len));

    W5500_Write8(u8Block, W5500_SN_CR, W5500_CR_RECV);
    if (W5500_WaitCommandDone(u8Sock, W5500_CMD_TIMEOUT_MS) != W5500_OK)
    {
        return W5500_ERR_TIMEOUT;
    }

    return (int32_t)u16Len;
}

/**
 * @brief  UDP 接收: 同时取出发送方 IP 与端口
 * @note   W5500 在 UDP 模式下,接收缓冲里每条数据报都是
 *         "8 字节报文头 + 负载",且 Sn_RX_RSR 把报文头算在内:
 *           [0..3] 源 IP  [4..5] 源端口(大端)  [6..7] 负载长度(大端)
 *         所以不能按 TCP 的数据流直接读,必须跳过报文头。
 *         官方 ioLibrary 的 recvfrom() 也是这个顺序。
 */
int32_t W5500_SocketRecvFrom(uint8_t u8Sock, uint8_t *pBuf, uint16_t u16MaxLen,
                             uint8_t pSrcIp[4], uint16_t *pSrcPort)
{
    uint8_t  au8Head[8];           /* 源 IP(4) + 源端口(2) + 负载长度(2) */
    uint16_t u16Size;
    uint16_t u16Dgram;             /* 整条数据报的负载长度(不含报文头)   */
    uint16_t u16Copy;              /* 本次拷给用户的长度                 */
    uint16_t u16Rd;
    uint16_t u16Offset;
    uint8_t  u8Block;

    if ((u8Sock >= W5500_SOCKET_COUNT) || (pBuf == NULL) || (u16MaxLen == 0u))
    {
        return W5500_ERR_PARAM;
    }

    u8Block = W5500_BSB_SOCK_REG(u8Sock);

    u16Size = W5500_Read16(u8Block, W5500_SN_RX_RSR);
    if (u16Size < 8u)              /* 连报文头都还没收全 */
    {
        return 0;
    }

    u16Rd     = W5500_Read16(u8Block, W5500_SN_RX_RD);
    u16Offset = (uint16_t)(u16Rd & W5500_SOCKET_BUF_MASK);

    /* 1. 先读 8 字节报文头,取出源地址与负载长度 */
    W5500_ReadSocketBuf(u8Sock, u16Offset, au8Head, 8u);

    u16Dgram = (uint16_t)(((uint16_t)au8Head[6] << 8) | (uint16_t)au8Head[7]);
    u16Copy  = (u16Dgram > u16MaxLen) ? u16MaxLen : u16Dgram;

    /* 2. 读负载(截断时剩余部分照样要消费,否则读指针与芯片不一致) */
    if (u16Copy > 0u)
    {
        W5500_ReadSocketBuf(u8Sock,
                            (uint16_t)((u16Offset + 8u) & W5500_SOCKET_BUF_MASK),
                            pBuf, u16Copy);
    }

    /* 3. 读指针跨越"报文头 + 整条负载",再发 RECV 通知芯片释放缓冲 */
    W5500_Write16(u8Block, W5500_SN_RX_RD, (uint16_t)(u16Rd + 8u + u16Dgram));
    W5500_Write8(u8Block, W5500_SN_CR, W5500_CR_RECV);
    if (W5500_WaitCommandDone(u8Sock, W5500_CMD_TIMEOUT_MS) != W5500_OK)
    {
        return W5500_ERR_TIMEOUT;
    }

    /* 4. 源地址取自报文头(比读 Sn_DIPR 可靠: 该寄存器会被后续数据报覆盖) */
    if (pSrcIp != NULL)
    {
        pSrcIp[0] = au8Head[0];
        pSrcIp[1] = au8Head[1];
        pSrcIp[2] = au8Head[2];
        pSrcIp[3] = au8Head[3];
    }
    if (pSrcPort != NULL)
    {
        *pSrcPort = (uint16_t)(((uint16_t)au8Head[4] << 8) | (uint16_t)au8Head[5]);
    }

    return (int32_t)u16Copy;
}
