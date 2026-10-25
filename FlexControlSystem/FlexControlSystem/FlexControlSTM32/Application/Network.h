#ifndef NETWORK_H
#define NETWORK_H

#include "main.h"

// ============================ 网络配置(按现场实际修改) ============================

// 本机 MAC( locally administered 地址, 首字节 0x02 )
#define NETWORK_LOCAL_MAC			{0x02, 0x55, 0xAA, 0x55, 0xAA, 0x01}
// 本机 IP / 子网掩码 / 网关
#define NETWORK_LOCAL_IP			{192, 168, 1, 100}
#define NETWORK_LOCAL_SUBNET		{255, 255, 255, 0}
#define NETWORK_LOCAL_GATEWAY		{192, 168, 1, 1}

// TCP 服务端监听端口
#define NETWORK_TCP_PORT			5000

// 使用的 socket 编号
#define NETWORK_SOCKET_INDEX		0

// ============================ 缓冲区配置 ============================

// 接收环形缓冲区大小(需为 2 的幂)
#define NETWORK_RECV_BUFFER_SIZE	512

// 单帧数据最大长度(帧头帧尾之间的有效数据, 含校验字节)
#define NETWORK_FRAME_DATA_LEN		64

// 帧队列深度
#define NETWORK_FRAME_QUEUE_SIZE	8

// 发送队列深度
#define NETWORK_SEND_QUEUE_SIZE		8


// 网络初始化: 复位 W5500, 配置网络信息并打开 TCP 服务端
bool NetworkInit(void);

// 网络主循环处理: 链路检测/连接维护/收帧/发帧, 在 FlexControlHandle 中周期调用
void NetworkHandle(void);

// 取出一帧完整数据(不含帧头帧尾, 含校验字节), 校验失败的帧在内部被丢弃
bool NetworkGetFrame(uint8_t *pBuf, uint8_t *pLen);

// 将数据入发送队列, 由 NetworkHandle 在连接建立后发出, 队列满或未连接返回 false
bool NetworkSendData(const uint8_t *pData, const uint8_t u8Len);

// 当前是否有上位机 TCP 连接
bool NetworkIsConnected(void);

#endif
