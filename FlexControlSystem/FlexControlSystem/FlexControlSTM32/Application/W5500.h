/**
 ******************************************************************************
 * @file    W5500.h
 * @brief   W5500 硬件 TCP/IP 以太网控制器驱动(SPI 接口)
 * @author  user
 * @date    2026-11-11
 *
 * @note    分层关系:
 *            W5500.c 只调用 SPI_MasterFullDuplexDMA.h 暴露的 SpiDev* 接口,
 *            不直接操作 SPI1/SPI2、不直接操作 CS/RST 引脚,也不 include HAL 的
 *            "spi.h"。SPI 实例与片选通过 W5500_Config_t 在初始化时绑定,
 *            因此换 SPI 口或换 MCU 时只改应用层的一处配置。
 *
 * @note    使用步骤:
 *            1. CubeMX 配好 SPI(8bit / Master / 全双工 / 软件 NSS)+ DMA;
 *            2. CS 引脚配为推挽输出,上电默认高电平(W5500 低电平选中);
 *            3. 填一个 W5500_Config_t(SPI 句柄 + CS + MAC/IP + 网关/掩码),
 *               调用 W5500_Init();
 *            4. 用 W5500_SocketOpen/Listen/Connect/Send/Recv 收发。
 ******************************************************************************
 */

#ifndef W5500_H
#define W5500_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "SPI_MasterFullDuplexDMA.h"

/*----------------------------------------------------------------------------
 * 版本与常量
 *----------------------------------------------------------------------------*/
#define W5500_VERSION_REG_VALUE     (0x04u)     /**< VERSIONR 固定读值,可用于探测芯片 */
#define W5500_SOCKET_COUNT          (8u)        /**< 芯片内置 8 个独立 Socket            */
#define W5500_SOCKET_BUF_SIZE       (2048u)     /**< 每 Socket 默认收/发缓冲各 2KB        */
#define W5500_SOCKET_BUF_MASK       (W5500_SOCKET_BUF_SIZE - 1u)

#define W5500_RESET_LOW_MS          (10u)       /**< RST 拉低保持时间,手册要求 >500us   */
#define W5500_RESET_HIGH_MS         (50u)       /**< 复位释放后等内部 PLL 稳定           */
#define W5500_CMD_TIMEOUT_MS        (100u)      /**< Socket 命令(Sn_CR)完成等待超时      */
#define W5500_LINK_TIMEOUT_MS       (2000u)     /**< 等待网线 LINK UP 超时               */
#define W5500_TXFREE_TIMEOUT_MS     (200u)      /**< 等发送缓冲区有空位超时              */

/*----------------------------------------------------------------------------
 * 返回码
 *----------------------------------------------------------------------------*/
#define W5500_OK                    (0)         /**< 成功                          */
#define W5500_ERR_PARAM             (-1)        /**< 参数非法(端口/长度/编号越界)  */
#define W5500_ERR_SPI               (-2)        /**< SPI 传输失败                  */
#define W5500_ERR_NO_DEVICE         (-3)        /**< 读不到正确版本号,芯片不在线   */
#define W5500_ERR_TIMEOUT           (-4)        /**< 命令/状态等待超时             */
#define W5500_ERR_STATE             (-5)        /**< Socket 状态不符合操作要求     */
#define W5500_ERR_NOLINK            (-6)        /**< 网线未连接                    */
#define W5500_ERR_NO_MEM            (-7)        /**< 发送缓冲区长期无空间          */

/*----------------------------------------------------------------------------
 * 通用寄存器(Common Register),地址为 16 位偏移
 *----------------------------------------------------------------------------*/
#define W5500_MR                    (0x0000u)   /**< 模式寄存器                    */
#define W5500_GAR                   (0x0001u)   /**< 网关 IP(4 字节)             */
#define W5500_SUBR                  (0x0005u)   /**< 子网掩码(4 字节)            */
#define W5500_SHAR                  (0x0009u)   /**< 本机 MAC(6 字节)            */
#define W5500_SIPR                  (0x000Fu)   /**< 本机 IP(4 字节)             */
#define W5500_INTLEVEL              (0x0013u)   /**< 中断引脚低电平持续时间        */
#define W5500_IR                    (0x0015u)   /**< 通用中断标志                  */
#define W5500_IMR                   (0x0016u)   /**< 通用中断屏蔽                  */
#define W5500_SIR                   (0x0017u)   /**< Socket 中断标志               */
#define W5500_SIMR                  (0x0018u)   /**< Socket 中断屏蔽               */
#define W5500_RTR                   (0x0019u)   /**< 重传超时(100us 单位)         */
#define W5500_RCR                   (0x001Bu)   /**< 重传次数                      */
#define W5500_PTIMER                (0x001Cu)   /**< PPP 链路保持周期              */
#define W5500_PMAGIC                (0x001Du)   /**< PPP 魔术字                    */
#define W5500_PHAR                  (0x001Eu)   /**< 对端 MAC(PPP)               */
#define W5500_PSID                  (0x0024u)   /**< PPP 会话 ID                   */
#define W5500_PMRU                  (0x0026u)   /**< PPP 最大接收单元              */
#define W5500_UIPR                  (0x0028u)   /**< 不可达 IP                     */
#define W5500_UPORTR                (0x002Cu)   /**< 不可达端口                    */
#define W5500_PHYCFGR               (0x002Eu)   /**< PHY 配置/状态                 */
#define W5500_VERSIONR              (0x0039u)   /**< 芯片版本,固定 0x04           */

/* MR 位 */
#define W5500_MR_RST                (0x80u)     /**< 软件复位,写 1 有效,自动清零  */
#define W5500_MR_WOL                (0x20u)
#define W5500_MR_PB                 (0x10u)
#define W5500_MR_PPP                (0x08u)
#define W5500_MR_FARP               (0x02u)

/* PHYCFGR 位 */
#define W5500_PHY_RST               (0x80u)     /**< PHY 复位                      */
#define W5500_PHY_OPMODE            (0x40u)     /**< 0=自动协商 1=强制             */
#define W5500_PHY_DPX               (0x04u)     /**< 1=全双工                      */
#define W5500_PHY_SPD               (0x02u)     /**< 1=100Mbps                     */
#define W5500_PHY_LINK              (0x01u)     /**< 1=链路已建立                  */

/*----------------------------------------------------------------------------
 * Socket 寄存器(n = 0..7),地址相对于该 Socket 的寄存器块
 *----------------------------------------------------------------------------*/
#define W5500_SN_MR                 (0x0000u)   /**< Socket 模式                   */
#define W5500_SN_CR                 (0x0001u)   /**< Socket 命令                   */
#define W5500_SN_IR                 (0x0002u)   /**< Socket 中断标志               */
#define W5500_SN_SR                 (0x0003u)   /**< Socket 状态                   */
#define W5500_SN_PORT               (0x0004u)   /**< 本地端口(2 字节)            */
#define W5500_SN_DHAR               (0x0006u)   /**< 目的 MAC(6 字节)            */
#define W5500_SN_DIPR               (0x000Cu)   /**< 目的 IP(4 字节)             */
#define W5500_SN_DPORT              (0x0010u)   /**< 目的端口(2 字节)            */
#define W5500_SN_MSSR               (0x0012u)   /**< TCP MSS                       */
#define W5500_SN_TOS                (0x0015u)   /**< IP TOS                        */
#define W5500_SN_TTL                (0x0016u)   /**< IP TTL                        */
#define W5500_SN_RXBUF_SIZE         (0x001Eu)   /**< 接收缓冲大小(单位 KB)       */
#define W5500_SN_TXBUF_SIZE         (0x001Fu)   /**< 发送缓冲大小(单位 KB)       */
#define W5500_SN_TX_FSR             (0x0020u)   /**< 发送空闲字节数(只读)        */
#define W5500_SN_TX_RD              (0x0022u)   /**< 发送读指针                    */
#define W5500_SN_TX_WR              (0x0024u)   /**< 发送写指针                    */
#define W5500_SN_RX_RSR             (0x0026u)   /**< 已接收字节数(只读)          */
#define W5500_SN_RX_RD              (0x0028u)   /**< 接收读指针                    */
#define W5500_SN_RX_WR              (0x002Au)   /**< 接收写指针(只读)            */
#define W5500_SN_IMR                (0x002Cu)   /**< Socket 中断屏蔽               */
#define W5500_SN_FRAG               (0x002Du)   /**< IP 分片长度                   */
#define W5500_SN_KPALVTR            (0x002Fu)   /**< Keep-alive 自动发送周期       */

/* Sn_MR 协议字段(低 4 位) */
#define W5500_PROTO_CLOSED          (0x00u)
#define W5500_PROTO_TCP             (0x01u)
#define W5500_PROTO_UDP             (0x02u)
#define W5500_PROTO_MACRAW          (0x04u)

/* Sn_CR 命令 */
#define W5500_CR_OPEN               (0x01u)
#define W5500_CR_LISTEN             (0x02u)
#define W5500_CR_CONNECT            (0x04u)
#define W5500_CR_DISCON             (0x08u)
#define W5500_CR_CLOSE              (0x10u)
#define W5500_CR_SEND               (0x20u)
#define W5500_CR_SEND_MAC           (0x21u)
#define W5500_CR_SEND_KEEP          (0x22u)
#define W5500_CR_RECV               (0x40u)

/* Sn_IR 中断位 */
#define W5500_SN_IR_SENDOK          (0x10u)
#define W5500_SN_IR_TIMEOUT         (0x08u)
#define W5500_SN_IR_RECV            (0x04u)
#define W5500_SN_IR_DISCON          (0x02u)
#define W5500_SN_IR_CON             (0x01u)

/* Sn_IMR 使能位:默认全部打开(事件位与 Sn_IR 同值) */
#define W5500_SN_IMR_ALL            (0x1Fu)

/* Sn_SR 状态机 */
#define W5500_SR_CLOSED             (0x00u)
#define W5500_SR_INIT               (0x13u)
#define W5500_SR_LISTEN             (0x14u)
#define W5500_SR_SYNSENT            (0x15u)
#define W5500_SR_SYNRECV            (0x16u)
#define W5500_SR_ESTABLISHED        (0x17u)
#define W5500_SR_FIN_WAIT           (0x18u)
#define W5500_SR_CLOSING            (0x1Au)
#define W5500_SR_TIME_WAIT          (0x1Bu)
#define W5500_SR_CLOSE_WAIT         (0x1Cu)
#define W5500_SR_LAST_ACK           (0x1Du)
#define W5500_SR_UDP                (0x22u)
#define W5500_SR_MACRAW             (0x02u)

/*----------------------------------------------------------------------------
 * SPI 控制字节:BSB(块选择) | RWB(读写) | OM(长度模式)
 *----------------------------------------------------------------------------*/
#define W5500_BSB_COMMON            (0x00u)
#define W5500_BSB_SOCK_REG(n)       ((uint8_t)((((uint8_t)(n)) * 0x20u) + 0x08u))
#define W5500_BSB_SOCK_TX(n)        ((uint8_t)((((uint8_t)(n)) * 0x20u) + 0x10u))
#define W5500_BSB_SOCK_RX(n)        ((uint8_t)((((uint8_t)(n)) * 0x20u) + 0x18u))

#define W5500_OM_VDM                (0x00u)     /**< 变长模式,帧内字节数由 CS 界定 */
#define W5500_OM_FDM1               (0x01u)
#define W5500_OM_FDM2               (0x02u)
#define W5500_OM_FDM4               (0x03u)

#define W5500_RWB_READ              (0x00u)
#define W5500_RWB_WRITE             (0x04u)

/*----------------------------------------------------------------------------
 * 配置结构
 *----------------------------------------------------------------------------*/
/**
 * @brief W5500 初始化参数
 * @note  除 SPI/CS 外,网络参数可留空(传 0)由驱动填默认值,
 *        但 MAC 必须唯一,否则同网段会冲突。
 */
typedef struct
{
    SPI_HandleTypeDef *pSpi;        /**< CubeMX 生成的 SPI 句柄,如 &hspi1       */
    GPIO_TypeDef      *pCsPort;     /**< 片选端口,如 GPIOA                      */
    uint16_t           CsPin;       /**< 片选引脚,如 GPIO_PIN_15                */
    GPIO_TypeDef      *pRstPort;    /**< 复位端口,无需硬件复位时传 NULL        */
    uint16_t           RstPin;      /**< 复位引脚                               */
    GPIO_TypeDef      *pIntPort;    /**< INT 端口,不接中断时传 NULL             */
    uint16_t           IntPin;      /**< INT 引脚(W5500 低电平有效)           */
    uint8_t            Mac[6];      /**< 本机 MAC                               */
    uint8_t            Ip[4];       /**< 本机 IP                                */
    uint8_t            Gateway[4];  /**< 网关 IP                                */
    uint8_t            Subnet[4];   /**< 子网掩码                               */
} W5500_Config_t;

/*----------------------------------------------------------------------------
 * 初始化与状态
 *----------------------------------------------------------------------------*/
/**
 * @brief  初始化 W5500:绑定 SPI、复位、校验版本、写入网络参数与 Socket 缓冲
 * @param  pConfig 配置参数
 * @return W5500_OK / W5500_ERR_*
 * @note   本函数不等待网线接入,需要判断链路请调用 W5500_WaitLink。
 */
int32_t W5500_Init(const W5500_Config_t *pConfig);

/**
 * @brief  读取芯片版本寄存器(VERSIONR),正常应等于 0x04
 */
uint8_t W5500_ReadVersion(void);

/**
 * @brief  驱动是否已完成初始化
 * @return 1=已初始化, 0=未初始化(此时不应调用 Socket 操作)
 */
uint8_t W5500_IsReady(void);

/**
 * @brief  软件复位(写 MR.RST),不涉及硬件引脚
 */
int32_t W5500_SoftReset(void);

/**
 * @brief  链路是否已建立
 * @return 1=已连接, 0=未连接
 */
uint8_t W5500_IsLinked(void);

/**
 * @brief  等待网线接入
 * @param  u32TimeoutMs 超时,0 表示用默认 W5500_LINK_TIMEOUT_MS
 * @return W5500_OK / W5500_ERR_NOLINK
 */
int32_t W5500_WaitLink(uint32_t u32TimeoutMs);

/*----------------------------------------------------------------------------
 * INT 引脚(可选):电平中断 + 事件缓存
 *
 *   使用方式(无 RTOS 的轮询主循环):
 *     1. EXTI 回调已由驱动内置(W5500.c 重载 HAL_GPIO_EXTI_Callback,
 *        只置标志,不在中断里读 SPI);应用层如有其它 EXTI 引脚,
 *        需在应用层重写回调并分发;
 *     2. 主循环里 W5500_IntIsAsserted() 为真时调用 W5500_ProcessInt();
 *     3. 用 W5500_GetEvent() 取本 Socket 的事件,处理完用 W5500_ClearEvent() 清。
 *
 *   注意:W5500 的 INT 是"电平"输出,只要还有未清的 Sn_IR 就一直拉低。
 *   W5500_IntIsAsserted() 同时包含"EXTI 置过的标志"与"引脚电平"两个条件,
 *   漏掉的边沿会被电平复查兜底,因此不会卡死。
 *----------------------------------------------------------------------------*/
/**
 * @brief  是否有待处理的 W5500 中断(EXTI 标志或 INT 引脚电平有效)
 * @return 1=有中断, 0=无中断(或未接 INT 引脚)
 */
uint8_t W5500_IntIsAsserted(void);

/**
 * @brief  处理一次中断:读 SIR、逐 Socket 读并清 Sn_IR、累加到事件缓存
 * @return 本次处理的 Socket 数量
 * @note   必须在主循环/任务上下文调用,不要放在 EXTI 服务函数里(内部有 SPI 阻塞访问)。
 */
uint8_t W5500_ProcessInt(void);

/**
 * @brief  读取并累加过的事件位(位定义与 W5500_SN_IR_* 相同)
 * @param  u8Sock Socket 编号 0..7
 */
uint8_t W5500_GetEvent(uint8_t u8Sock);

/**
 * @brief  清除指定 Socket 的事件位
 * @param  u8Mask 要清的位组合,0xFF 表示全清
 */
void W5500_ClearEvent(uint8_t u8Sock, uint8_t u8Mask);

/**
 * @brief  使能/关闭某个 Socket 的中断上报(SIMR 对应位)
 * @note   W5500_SocketOpen 会自动打开,SocketClose 会自动关闭,一般无需手动调用。
 */
int32_t W5500_SocketIntEnable(uint8_t u8Sock, uint8_t u8Enable);

/*----------------------------------------------------------------------------
 * 寄存器访问(底层,一般只在驱动内部或调试时使用)
 *----------------------------------------------------------------------------*/
uint8_t  W5500_Read8(uint8_t u8Block, uint16_t u16Addr);
void     W5500_Write8(uint8_t u8Block, uint16_t u16Addr, uint8_t u8Data);
uint16_t W5500_Read16(uint8_t u8Block, uint16_t u16Addr);
void     W5500_Write16(uint8_t u8Block, uint16_t u16Addr, uint16_t u16Data);
void     W5500_ReadBuf(uint8_t u8Block, uint16_t u16Addr, uint8_t *pBuf, uint16_t u16Len);
void     W5500_WriteBuf(uint8_t u8Block, uint16_t u16Addr, const uint8_t *pBuf, uint16_t u16Len);

/*----------------------------------------------------------------------------
 * Socket 操作
 *----------------------------------------------------------------------------*/
/**
 * @brief  打开 Socket
 * @param  u8Sock     Socket 编号 0..7
 * @param  u8Protocol W5500_PROTO_TCP / W5500_PROTO_UDP / W5500_PROTO_MACRAW
 * @param  u16Port    本地端口(0 表示由驱动不设置,保持原值)
 * @return W5500_OK / W5500_ERR_*
 */
int32_t W5500_SocketOpen(uint8_t u8Sock, uint8_t u8Protocol, uint16_t u16Port);

/**
 * @brief  TCP 服务端:进入监听状态,等待远端连接
 */
int32_t W5500_SocketListen(uint8_t u8Sock);

/**
 * @brief  TCP 客户端:连接远端服务器
 * @param  pDstIp    目标 IP,4 字节
 * @param  u16DstPort 目标端口
 * @return W5500_OK / W5500_ERR_* (超时表示对端无响应或不可达)
 */
int32_t W5500_SocketConnect(uint8_t u8Sock, const uint8_t pDstIp[4], uint16_t u16DstPort);

/**
 * @brief  主动断开并关闭 Socket(执行 CLOSE 命令,状态回到 CLOSED)
 */
void W5500_SocketClose(uint8_t u8Sock);

/**
 * @brief  发送数据(TCP 已连接 / UDP 已设置目标后)
 * @param  pData  待发数据
 * @param  u16Len 长度,不能超过该 Socket 的发送缓冲区剩余空间
 * @return >=0 实际发送字节数; <0 W5500_ERR_*
 */
int32_t W5500_SocketSend(uint8_t u8Sock, const uint8_t *pData, uint16_t u16Len);

/**
 * @brief  UDP 发送:先写目标地址再发送
 */
int32_t W5500_SocketSendTo(uint8_t u8Sock, const uint8_t pDstIp[4],
                           uint16_t u16DstPort, const uint8_t *pData, uint16_t u16Len);

/**
 * @brief  查询接收缓冲区中待读字节数
 */
uint16_t W5500_SocketRecvSize(uint8_t u8Sock);

/**
 * @brief  读取已接收数据并推进读指针(会执行 RECV 命令)
 * @param  pBuf       接收缓冲区
 * @param  u16MaxLen  缓冲区容量
 * @return >=0 实际读出字节数(0 表示暂无数据); <0 W5500_ERR_*
 */
int32_t W5500_SocketRecv(uint8_t u8Sock, uint8_t *pBuf, uint16_t u16MaxLen);

/**
 * @brief  UDP 接收:同时取出发送方 IP 与端口
 * @param  pSrcIp   输出:来源 IP,4 字节
 * @param  pSrcPort 输出:来源端口
 */
int32_t W5500_SocketRecvFrom(uint8_t u8Sock, uint8_t *pBuf, uint16_t u16MaxLen,
                             uint8_t pSrcIp[4], uint16_t *pSrcPort);

/**
 * @brief  读取 Socket 状态(Sn_SR)
 */
uint8_t W5500_SocketStatus(uint8_t u8Sock);

/**
 * @brief  读取并清除 Socket 中断标志(Sn_IR)
 * @return 清除前的中断标志位组合
 */
uint8_t W5500_SocketClearIr(uint8_t u8Sock);

#ifdef __cplusplus
}
#endif

#endif /* W5500_H */
