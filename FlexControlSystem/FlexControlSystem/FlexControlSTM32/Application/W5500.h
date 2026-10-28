#ifndef W5500_H
#define W5500_H

#include "main.h"

// W5500 socket 数量
#define W5500_SOCKET_NUM		8

// socket 工作模式
#define W5500_MODE_TCP			0x01
#define W5500_MODE_UDP			0x02
#define W5500_MODE_MACRAW		0x04

// socket 状态寄存器值
#define W5500_SR_CLOSED			0x00
#define W5500_SR_INIT			0x13
#define W5500_SR_LISTEN			0x14
#define W5500_SR_ESTABLISHED	0x17
#define W5500_SR_FIN_WAIT		0x15
#define W5500_SR_CLOSING		0x1A
#define W5500_SR_CLOSE_WAIT		0x1C
#define W5500_SR_UDP			0x22

// socket 中断标志位
#define W5500_SIR_CON			0x01
#define W5500_SIR_DISCON		0x02
#define W5500_SIR_RECV			0x04
#define W5500_SIR_TIMEOUT		0x08
#define W5500_SIR_SEND_OK		0x10


// 硬复位 W5500 并校验版本号, 上电后调用一次, 成功返回 true
bool W5500HardwareReset(void);

// SPI 总线自检: 写 MR 回读比对, 需在 W5500HardwareReset 之后调用, 用于调试总线/接线问题
bool W5500SpiSelfTest(void);

// SPI 回环测试: 断开 W5500 MISO, 杜邦线直连 PB4(MISO)-PB5(MOSI) 后调用
// 返回 true = MCU 侧 SPI 正常, 问题在接线/供电/芯片; false = 查 MCU 侧 SPI 配置
bool W5500SpiLoopbackTest(void);

// 配置网络信息(MAC/IP/子网掩码/网关), 需在 W5500HardwareReset 成功后调用
void W5500SetNetInfo(const uint8_t *pMac, const uint8_t *pIp, const uint8_t *pSubnet, const uint8_t *pGw);

// 获取 PHY 链路状态, true = 网线已连接
bool W5500GetLinkStatus(void);

// 打开 socket(TCP/UDP), 内部等待进入 INIT/UDP 状态, 成功返回 true
bool W5500SocketOpen(const uint8_t u8Socket, const uint8_t u8Mode, const uint16_t u16Port);

// 监听(TCP 服务端), 内部等待进入 LISTEN 状态, 成功返回 true
bool W5500SocketListen(const uint8_t u8Socket);

// 关闭 socket
void W5500SocketClose(const uint8_t u8Socket);

// 获取 socket 当前状态值(见 W5500_SR_xxx 宏)
uint8_t W5500GetSocketStatus(const uint8_t u8Socket);

// 获取接收缓冲区可读字节数
uint16_t W5500SocketGetRecvSize(const uint8_t u8Socket);

// 发送数据, 内部等待空闲发送缓冲区(带超时), 返回实际发出的长度, 失败返回 -1
int16_t W5500SocketSend(const uint8_t u8Socket, const uint8_t *pData, const uint16_t u16Len);

// 接收数据, 返回实际读到的长度, 无数据返回 0
int16_t W5500SocketRecv(const uint8_t u8Socket, uint8_t *pData, const uint16_t u16MaxLen);

// 清除 socket 中断标志位(写 1 清除), 见 W5500_SIR_xxx 宏
void W5500ClearSocketIrq(const uint8_t u8Socket, const uint8_t u8Mask);

// INTN 引脚中断服务, 由 HAL_GPIO_EXTI_Callback 调用, 仅置标志不访问 SPI
void W5500IrqHandler(void);

// 获取并清除中断标志(由主循环调用), 有中断返回 true
bool W5500PopIrqFlag(void);

#endif
