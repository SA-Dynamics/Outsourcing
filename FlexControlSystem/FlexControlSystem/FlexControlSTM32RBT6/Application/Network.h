/**
 * @file    Network.h
 * @brief   W5500 网络通信内核: TCP 服务端连接维护 + 帧收发队列 + UDP 发现服务
 * @note    本模块只做"把字节可靠地收进来、把帧可靠地发出去", 不含任何业务命令语义:
 *          收到的帧由应用层用 NetworkGetFrame() 取走自行解释, 要发的内容用
 *          NetworkSendData() 交给发送队列。这样同一份内核可以搬到任何项目。
 *
 *          硬件绑定(SPI 句柄/CS/RST/INT 引脚)全部由 NetworkInit() 的入参给出,
 *          本文件不 include spi.h, 也没有任何 hspi1 / GPIO_PIN_x 字面量 ——
 *          换板子只改应用层那一处结构体初始化。
 *
 *          帧格式(与上位机约定): 55 AA | cmd | len | data[len] | chkH chkL | AA 55
 *          校验 = 从 cmd 起累加 len+2 个字节, 取反加一(16 位)。
 *          NetworkGetFrame() 取出的是不含帧头帧尾、含 cmd..chk 的部分(长度 = len+4)。
 */
#ifndef NETWORK_H
#define NETWORK_H

#include "main.h"
#include <stdbool.h>

/* ============================ 硬件绑定 ====================================== */

/**
 * @brief W5500 的硬件连接描述(由应用层填写, 内核只保存不解释)
 * @note  引脚名来自 CubeMX 生成的 main.h, 各工程命名不同, 所以放在应用层绑定
 */
typedef struct
{
    SPI_HandleTypeDef *pSpi;        /**< SPI 句柄, 如 &hspi1                       */
    GPIO_TypeDef      *pCsPort;     /**< 片选端口                                  */
    uint16_t           CsPin;       /**< 片选引脚(W5500 低有效)                    */
    GPIO_TypeDef      *pRstPort;    /**< 复位端口, 没有硬复位时传 NULL             */
    uint16_t           RstPin;      /**< 复位引脚                                  */
    GPIO_TypeDef      *pIntPort;    /**< INT 端口, 不接中断时传 NULL               */
    uint16_t           IntPin;      /**< INT 引脚(W5500 低有效)                    */
} NetworkHwConfig_t;

/* ============================ 网络参数(按现场修改) ========================== */

/** 本机 MAC(locally administered 地址, 首字节 0x02) */
#define NETWORK_LOCAL_MAC           {0x02, 0x55, 0xAA, 0x55, 0xAA, 0x01}
/** 本机 IP / 子网掩码 / 网关: Flash 里没有有效参数时用这里的默认值 */
#define NETWORK_LOCAL_IP            {192, 168, 2, 199}
#define NETWORK_LOCAL_SUBNET        {255, 255, 255, 0}
#define NETWORK_LOCAL_GATEWAY       {192, 168, 2, 1}

/** TCP 服务端监听端口 */
#define NETWORK_TCP_PORT            5000

/** TCP 使用的 socket 编号 */
#define NETWORK_SOCKET_INDEX        0

/* ============================ UDP 发现服务 ================================== */

/**
 * @brief 设备发现: 上位机不知道本机 IP 时, 向广播地址发发现请求, 本机单播回自己的 IP
 * @note  用独立 socket 监听, 与 TCP 服务互不影响; UDP 与 TCP 的 5000 端口是两个
 *        独立命名空间, 可以同号。命令码是与上位机的约定, 两端必须一致。
 */
#define NETWORK_DISCOVERY_ENABLE    1
#define NETWORK_DISCOVERY_SOCKET    1
#define NETWORK_DISCOVERY_PORT      NETWORK_TCP_PORT
/** 发现请求命令码(cmd = 0x11, len = 1, data = 0x01) */
#define NETWORK_DISCOVERY_REQ_CMD   0x11
/** 发现应答命令码(cmd = 0x12, len = 4, data = 本机 IP 四字节) */
#define NETWORK_DISCOVERY_RESP_CMD  0x12
/** socket 状态异常后的重开间隔 ms(避免每个主循环都空跑 SPI) */
#define NETWORK_DISCOVERY_REOPEN_MS 500

/* ============================ 缓冲区配置 ==================================== */

/** 接收环形缓冲区大小(需为 2 的幂) */
#define NETWORK_RECV_BUFFER_SIZE    512
/** 单帧数据最大长度(帧头帧尾之间的有效数据, 含校验字节) */
#define NETWORK_FRAME_DATA_LEN      64
/** 帧队列深度 */
#define NETWORK_FRAME_QUEUE_SIZE    8
/** 发送队列深度 */
#define NETWORK_SEND_QUEUE_SIZE     8
/** 链路检测间隔 ms */
#define NETWORK_LINK_CHECK_MS       500

/* ============================ 对外接口 ====================================== */

/**
 * @brief  初始化 W5500 并启动 TCP 服务端
 * @param  pHw 硬件绑定(SPI/CS/RST/INT), 必须非空, 内核内部会复制一份
 * @retval true=初始化并打开 socket 成功, false=芯片无应答/参数非法
 * @note   内部会做硬复位, 阻塞约 60ms。可重复调用(内部先清空全部状态)。
 *         调用前应先执行 SaveParamsInit(), 里面保存的 IP 会覆盖 Network.h 的默认值。
 */
bool NetworkInit(const NetworkHwConfig_t *pHw);

/**
 * @brief  主循环处理: 处理中断 -> 发现服务 -> 连接状态机 -> 收发
 * @note   需在应用主循环里周期调用(建议 <= 10ms 一次), 内部不含等待
 */
void NetworkHandle(void);

/**
 * @brief  取出一帧完整数据(不含帧头帧尾, 含 cmd/len/data/chk)
 * @param  pBuf 输出缓冲, 不小于 NETWORK_FRAME_DATA_LEN
 * @param  pLen 输出帧长度(len + 4)
 * @retval true=取到一帧, false=队列为空
 */
bool NetworkGetFrame(uint8_t *pBuf, uint8_t *pLen);

/**
 * @brief  把一整帧交给发送队列, 由 NetworkHandle 在连接建立后发出
 * @param  pData 完整帧(含帧头帧尾), 长度不超过 64
 * @param  u8Len 帧长度
 * @retval true=已入队, false=未连接/队列满/参数非法
 * @note   发送失败(发送缓冲不足超时)的帧会保留在队列里下个周期重试
 */
bool NetworkSendData(const uint8_t *pData, const uint8_t u8Len);

/**
 * @brief  当前是否有上位机 TCP 连接
 */
bool NetworkIsConnected(void);

/**
 * @brief  取当前运行的 IP(NetworkInit 实际写入 W5500 的值)
 * @param  pIp 输出缓冲, 4 字节
 * @note   W5500 未初始化成功时返回 NETWORK_LOCAL_IP 的默认值
 */
void NetworkGetIp(uint8_t *pIp);

#endif /* NETWORK_H */
