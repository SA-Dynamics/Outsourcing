/**
 ******************************************************************************
 * @file    SPI_MasterFullDuplexDMA.h
 * @brief   SPI 主机全双工收发驱动(带 DMA),与具体 SPI 外设/片选引脚解耦
 * @author  user
 * @date    2026-11-11
 *
 * @note    分层关系:
 *            +-----------------------------+
 *            |  应用 / 器件驱动(W5500.c)   |  只调用本文件的 SpiDev* 接口
 *            +--------------+--------------+
 *                           |
 *            +--------------v--------------+
 *            | SPI_MasterFullDuplexDMA.c   |  不出现任何 hspiX / GPIO_PIN_x 字面量
 *            +--------------+--------------+
 *                           |
 *            +--------------v--------------+
 *            |  CubeMX 生成层(spi.c/dma.c) |  负责实例、GPIO、DMA 通道、NVIC
 *            +-----------------------------+
 *
 *          本文件只依赖一个 SPI_HandleTypeDef 指针 + 一个片选描述,
 *          因此同一份代码可以同时驱动 SPI1/SPI2/SPI3,也可多器件并存。
 *
 * @note    使用前提(由 CubeMX 保证):
 *            1. SPI 配置为 Master + 2LINES(全双工) + 8bit + 软件 NSS;
 *            2. 该 SPI 的 TX/RX 两个 DMA 通道都已使能并 __HAL_LINKDMA 绑定;
 *            3. 片选引脚已配置为推挽输出,初始电平为"未选中"。
 ******************************************************************************
 */

#ifndef SPI_MASTERFULLDUPLEXDMA_H
#define SPI_MASTERFULLDUPLEXDMA_H

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------------------
 * 平台适配:换 MCU 家族时只改下面这一处(或由编译器 -D 覆盖)
 *----------------------------------------------------------------------------*/
#ifndef SPI_MASTER_HAL_HEADER
#define SPI_MASTER_HAL_HEADER   "stm32f1xx_hal.h"
#endif
#include SPI_MASTER_HAL_HEADER

/*----------------------------------------------------------------------------
 * 可配置项
 *----------------------------------------------------------------------------*/
/** 1: 传输长度 >= 阈值时走 DMA;0: 全部走阻塞轮询(调试对比用) */
#ifndef SPI_MASTER_USE_DMA
#define SPI_MASTER_USE_DMA              (1)
#endif

/** 长度达到该值才启用 DMA(短帧走 DMA 反而更慢) */
#ifndef SPI_MASTER_DMA_MIN_LEN
#define SPI_MASTER_DMA_MIN_LEN          (8u)
#endif

/** 等待 SPI 空闲(上一次传输结束)的超时,单位 ms */
#ifndef SPI_MASTER_BUSY_TIMEOUT_MS
#define SPI_MASTER_BUSY_TIMEOUT_MS      (10u)
#endif

/** 单次传输的默认超时,单位 ms。18MHz 下 2KB 约 0.9ms,50ms 足够宽裕 */
#ifndef SPI_MASTER_XFER_TIMEOUT_MS
#define SPI_MASTER_XFER_TIMEOUT_MS      (50u)
#endif

/*----------------------------------------------------------------------------
 * 返回码
 *----------------------------------------------------------------------------*/
#define SPI_DEV_OK                      (0)     /**< 成功                       */
#define SPI_DEV_ERR_PARAM               (-1)    /**< 参数非法                   */
#define SPI_DEV_ERR_TIMEOUT             (-2)    /**< 超时(已尝试 Abort 传输)    */
#define SPI_DEV_ERR_HAL                 (-3)    /**< HAL 报错(溢出/模式错等)     */
#define SPI_DEV_ERR_BUSY                (-4)    /**< 设备被上一次传输占用       */

/*----------------------------------------------------------------------------
 * 类型定义
 *----------------------------------------------------------------------------*/
/**
 * @brief 片选控制回调
 * @param u8Level 1: 拉高(未选中); 0: 拉低(选中)
 * @note  当片选不是普通 GPIO(如用 74HC138 译码、或片选与其它器件共用)时,
 *        注册本回调由应用自行实现;传 NULL 则内部用 HAL_GPIO_WritePin。
 */
typedef void (*SpiCsCtrl_t)(uint8_t u8Level);

/**
 * @brief SPI 从设备描述(一个实例对应一个片选)
 * @note  该结构体由调用方静态分配,生命周期需覆盖使用期。
 */
typedef struct
{
    SPI_HandleTypeDef *pHandle;         /**< 由 CubeMX 初始化的 SPI 句柄,如 &hspi1 */
    GPIO_TypeDef      *pCsPort;         /**< 片选端口,如 GPIOA;用回调时可为 NULL */
    uint16_t           CsPin;           /**< 片选引脚,如 GPIO_PIN_15            */
    SpiCsCtrl_t        pfnCsCtrl;       /**< 片选控制回调,可为 NULL             */
    uint8_t            u8CsActiveLevel; /**< 选中电平: 0=低有效, 1=高有效       */
} SpiDevice_t;

/*----------------------------------------------------------------------------
 * 绑定与片选
 *----------------------------------------------------------------------------*/
/**
 * @brief  绑定一个 SPI 外设与片选引脚
 * @param  pDev       设备描述
 * @param  pHandle    SPI 句柄
 * @param  pCsPort    片选端口(NULL 表示改用回调)
 * @param  u16CsPin   片选引脚
 * @return SPI_DEV_OK / SPI_DEV_ERR_PARAM
 * @note   默认按"低电平有效"处理,高有效器件请调用 SpiDevSetCsActiveLevel。
 */
int32_t SpiDevAttach(SpiDevice_t     *pDev,
                     SPI_HandleTypeDef *pHandle,
                     GPIO_TypeDef      *pCsPort,
                     uint16_t           u16CsPin);

/**
 * @brief  设置片选有效电平
 * @param  pDev            设备描述
 * @param  u8ActiveLevel   0=低有效(默认), 1=高有效
 */
void SpiDevSetCsActiveLevel(SpiDevice_t *pDev, uint8_t u8ActiveLevel);

/**
 * @brief  注册自定义片选回调(传 NULL 恢复为 GPIO 直控)
 */
void SpiDevSetCsCallback(SpiDevice_t *pDev, SpiCsCtrl_t pfnCsCtrl);

/**
 * @brief  片选有效(拉低/拉高)
 * @note   一般不需要单独调用,使用 SpiDevBegin/SpiDevEnd 成对包裹整帧即可。
 */
void SpiDevSelect(SpiDevice_t *pDev);

/**
 * @brief  片选无效
 */
void SpiDevDeselect(SpiDevice_t *pDev);

/*----------------------------------------------------------------------------
 * 帧式访问(供 W5500 这类"一帧 = 命令头 + 数据"的器件使用)
 *----------------------------------------------------------------------------*/
/**
 * @brief  开始一帧:等总线空闲 + 清残留接收 + 片选有效
 * @return SPI_DEV_OK / SPI_DEV_ERR_*
 * @note   之后可连续调用 SpiDevWrite / SpiDevRead 多段,片选始终保持有效,
 *         最后必须调用 SpiDevEnd 结束,否则器件会把后续帧错当成同一条命令。
 */
int32_t SpiDevBegin(SpiDevice_t *pDev);

/**
 * @brief  结束一帧:等最后一个字节移出(BSY=0)后释放片选
 * @return SPI_DEV_OK / SPI_DEV_ERR_*
 */
int32_t SpiDevEnd(SpiDevice_t *pDev);

/*----------------------------------------------------------------------------
 * 数据收发
 *----------------------------------------------------------------------------*/
/**
 * @brief  发送数据(忽略同时收到的内容)
 * @param  pDev       设备描述
 * @param  pTxBuf     发送缓冲区
 * @param  u16Len     字节数(8bit 帧模式),必须 > 0
 * @param  u32TimeoutMs 超时,0 表示用默认值
 * @return SPI_DEV_OK / SPI_DEV_ERR_*
 */
int32_t SpiDevWrite(SpiDevice_t  *pDev,
                    const uint8_t *pTxBuf,
                    uint16_t       u16Len,
                    uint32_t       u32TimeoutMs);

/**
 * @brief  接收数据(主机自动输出时钟,MOSI 上为无效数据)
 * @param  pDev       设备描述
 * @param  pRxBuf     接收缓冲区
 * @param  u16Len     字节数,必须 > 0
 * @param  u32TimeoutMs 超时,0 表示用默认值
 * @return SPI_DEV_OK / SPI_DEV_ERR_*
 */
int32_t SpiDevRead(SpiDevice_t *pDev,
                   uint8_t     *pRxBuf,
                   uint16_t     u16Len,
                   uint32_t     u32TimeoutMs);

/**
 * @brief  全双工收发(同时收与发,长度相同)
 * @param  pDev       设备描述
 * @param  pTxBuf     发送缓冲区,不能为 NULL
 * @param  pRxBuf     接收缓冲区,不能为 NULL
 * @param  u16Len     字节数,必须 > 0
 * @param  u32TimeoutMs 超时,0 表示用默认值
 * @return SPI_DEV_OK / SPI_DEV_ERR_*
 */
int32_t SpiDevWriteRead(SpiDevice_t   *pDev,
                        const uint8_t *pTxBuf,
                        uint8_t       *pRxBuf,
                        uint16_t       u16Len,
                        uint32_t       u32TimeoutMs);

/*----------------------------------------------------------------------------
 * 状态查询
 *----------------------------------------------------------------------------*/
/**
 * @brief  总线是否仍在忙
 * @return 1=忙, 0=空闲
 */
uint8_t SpiDevIsBusy(SpiDevice_t *pDev);

/**
 * @brief  清掉残留的接收数据与溢出标志(OVR)
 * @note   W5500 这类"只写不读"的帧会在 RX 侧累积 OVR,下次读取前必须清干净,
 *         否则读回的字节会错位。SpiDevBegin 已自动调用,一般无需手动调。
 */
void SpiDevFlush(SpiDevice_t *pDev);

#ifdef __cplusplus
}
#endif

#endif /* SPI_MASTERFULLDUPLEXDMA_H */
