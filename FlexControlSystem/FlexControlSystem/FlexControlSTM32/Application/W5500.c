#include "W5500.h"
#include "spi.h"
#include "Timer.h"

// INTN --- GPIOA4  (外部中断, 下降沿)
// RSTN --- GPIOA5  (推挽输出, 低有效)
// CS   --- GPIOA15 (推挽输出, 软件控制)
// SPI  --- SPI1, PB3=SCK PB4=MISO PB5=MOSI (重映射), 时钟 72/4 = 18MHz

// 引脚控制宏
#define W5500_CS_LOW()			HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET)
#define W5500_CS_HIGH()			HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_SET)
#define W5500_RST_LOW()			HAL_GPIO_WritePin(RSTN_GPIO_Port, RSTN_Pin, GPIO_PIN_RESET)
#define W5500_RST_HIGH()		HAL_GPIO_WritePin(RSTN_GPIO_Port, RSTN_Pin, GPIO_PIN_SET)

// 通用寄存器地址
#define W5500_REG_MR			0x0000
#define W5500_REG_GAR			0x0001		// 网关 4 字节
#define W5500_REG_SUBR			0x0005		// 子网掩码 4 字节
#define W5500_REG_SHAR			0x0009		// MAC 6 字节
#define W5500_REG_SIPR			0x000F		// 本机 IP 4 字节
#define W5500_REG_IR			0x0015		// 通用中断标志
#define W5500_REG_IMR			0x0016
#define W5500_REG_SIR			0x0017		// socket 中断状态
#define W5500_REG_SIMR			0x0018
#define W5500_REG_PHYCFGR		0x002E		// PHY 配置, bit0 = 链路状态
#define W5500_REG_VERSIONR		0x0039		// 版本号, 固定为 0x04

// socket 寄存器(地址为单个 socket 内的偏移)
#define W5500_SREG_MR			0x0000
#define W5500_SREG_CR			0x0001
#define W5500_SREG_IR			0x0002
#define W5500_SREG_SR			0x0003
#define W5500_SREG_PORT			0x0004		// 2 字节
#define W5500_SREG_TXBUF_SIZE	0x001F		// 单位 KB
#define W5500_SREG_RXBUF_SIZE	0x001E		// 单位 KB
#define W5500_SREG_TX_FSR		0x0020		// 发送空闲缓冲区大小 2 字节
#define W5500_SREG_TX_WR		0x0024		// 发送写指针 2 字节
#define W5500_SREG_RX_RSR		0x0026		// 接收已用缓冲区大小 2 字节
#define W5500_SREG_RX_RD		0x0028		// 接收读指针 2 字节
#define W5500_SREG_IMR			0x002C

// socket 命令
#define W5500_CR_OPEN			0x01
#define W5500_CR_LISTEN			0x02
#define W5500_CR_CLOSE			0x10
#define W5500_CR_SEND			0x20
#define W5500_CR_RECV			0x40

// BSB 段选择: 通用寄存器=0x00, socket n 的寄存器/发送缓冲/接收缓冲 = (1/2/3) + 4*n
// 注意: 控制字节 = BSB[4:0]<<3 | RWB(bit2) | OM[1:0], BSB 必须左移 3 位
#define W5500_BSB_COMMON		0x00		// 通用寄存器段
#define W5500_BSB_REG(socket)	(uint8_t)(0x01 + 4 * (socket))
#define W5500_BSB_TX(socket)	(uint8_t)(0x02 + 4 * (socket))
#define W5500_BSB_RX(socket)	(uint8_t)(0x03 + 4 * (socket))
#define W5500_CTRL_WRITE		0x04		// 控制字节 bit2: 1=写 0=读

// socket0 缓冲区大小(KB), 其余 socket 各 1KB, 总和不超过 16KB
#define W5500_SOCKET0_BUF_KB	4

// 版本号与复位时序参数
#define W5500_CHIP_VERSION		0x04
#define W5500_RST_HOLD_US		600			// 复位低电平保持时间 > 500us
#define W5500_RST_RECOVER_MS	60			// 复位后等待 PLL 稳定时间
#define W5500_WAIT_TIMEOUT_US	100000		// 寄存器等待超时 100ms


// 中断标志, 由 INTN 中断置位, 主循环消费
static volatile bool g_bW5500IrqFlag = false;

// 等待超时用的计时器
static uint32_t g_u32W5500RstTimer = 0;
static uint32_t g_u32W5500WaitTimer = 0;

// 各 socket 发送/接收缓冲区掩码(大小 - 1), 用于环形缓冲地址回卷
static uint16_t g_u16TxMask[W5500_SOCKET_NUM] = {0};
static uint16_t g_u16RxMask[W5500_SOCKET_NUM] = {0};

// 前置声明
static bool WaitSocketStatus(const uint8_t u8Socket, const uint8_t u8Status);
uint8_t W5500GetSocketStatus(const uint8_t u8Socket);
uint16_t W5500SocketGetRecvSize(const uint8_t u8Socket);


// ============================ SPI 底层 ============================

// SPI 收发一个字节
static uint8_t SpiTransferByte(const uint8_t u8Data)
{
	uint8_t u8Recv = 0;
	HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)&u8Data, &u8Recv, 1, 10);
	return u8Recv;
}

// 变长读写: W5500 帧格式 = 16 位地址 + 控制字节(BSB<<3 | RWB | OM=00) + 连续数据
static void W5500ReadBuf(const uint8_t u8Bsb, const uint16_t u16Addr, uint8_t *pData, const uint16_t u16Len)
{
	const uint8_t u8Ctrl = (uint8_t)(u8Bsb << 3);		// RWB=0 读, OM=00 变长模式

	W5500_CS_LOW();
	SpiTransferByte((uint8_t)(u16Addr >> 8));
	SpiTransferByte((uint8_t)(u16Addr));
	SpiTransferByte(u8Ctrl);
	for (uint16_t i = 0; i < u16Len; i++)
	{
		pData[i] = SpiTransferByte(0x00);
	}
	W5500_CS_HIGH();
}

static void W5500WriteBuf(const uint8_t u8Bsb, const uint16_t u16Addr, const uint8_t *pData, const uint16_t u16Len)
{
	const uint8_t u8Ctrl = (uint8_t)((u8Bsb << 3) | W5500_CTRL_WRITE);

	W5500_CS_LOW();
	SpiTransferByte((uint8_t)(u16Addr >> 8));
	SpiTransferByte((uint8_t)(u16Addr));
	SpiTransferByte(u8Ctrl);
	for (uint16_t i = 0; i < u16Len; i++)
	{
		SpiTransferByte(pData[i]);
	}
	W5500_CS_HIGH();
}

// 单字节寄存器读写
static uint8_t W5500ReadReg(const uint8_t u8Bsb, const uint16_t u16Addr)
{
	uint8_t u8Data = 0;
	W5500ReadBuf(u8Bsb, u16Addr, &u8Data, 1);
	return u8Data;
}

static void W5500WriteReg(const uint8_t u8Bsb, const uint16_t u16Addr, const uint8_t u8Data)
{
	W5500WriteBuf(u8Bsb, u16Addr, &u8Data, 1);
}

// 16 位寄存器读取(高位在前)
static uint16_t W5500ReadReg16(const uint8_t u8Bsb, const uint16_t u16Addr)
{
	uint8_t u8Data[2] = {0};
	W5500ReadBuf(u8Bsb, u16Addr, u8Data, 2);
	return (uint16_t)((uint16_t)u8Data[0] << 8) | u8Data[1];
}

static void W5500WriteReg16(const uint8_t u8Bsb, const uint16_t u16Addr, const uint16_t u16Data)
{
	uint8_t u8Buf[2] = {(uint8_t)(u16Data >> 8), (uint8_t)(u16Data)};
	W5500WriteBuf(u8Bsb, u16Addr, u8Buf, 2);
}

// 读 socket n 的变长缓冲, 考虑环形地址回卷
static void W5500ReadSocketBuf(const uint8_t u8Socket, const uint16_t u16Ptr, uint8_t *pData, uint16_t u16Len)
{
	const uint16_t u16Mask = g_u16RxMask[u8Socket];
	uint16_t u16Offset = u16Ptr & u16Mask;
	uint16_t u16BlockSize = u16Mask + 1;

	if (u16Offset + u16Len > u16BlockSize)
	{
		uint16_t u16First = u16BlockSize - u16Offset;
		W5500ReadBuf(W5500_BSB_RX(u8Socket), u16Offset, pData, u16First);
		W5500ReadBuf(W5500_BSB_RX(u8Socket), 0, pData + u16First, u16Len - u16First);
	}
	else
	{
		W5500ReadBuf(W5500_BSB_RX(u8Socket), u16Offset, pData, u16Len);
	}
}

// 写 socket n 的发送缓冲, 考虑环形地址回卷
static void W5500WriteSocketBuf(const uint8_t u8Socket, const uint16_t u16Ptr, const uint8_t *pData, uint16_t u16Len)
{
	const uint16_t u16Mask = g_u16TxMask[u8Socket];
	uint16_t u16Offset = u16Ptr & u16Mask;
	uint16_t u16BlockSize = u16Mask + 1;

	if (u16Offset + u16Len > u16BlockSize)
	{
		uint16_t u16First = u16BlockSize - u16Offset;
		W5500WriteBuf(W5500_BSB_TX(u8Socket), u16Offset, pData, u16First);
		W5500WriteBuf(W5500_BSB_TX(u8Socket), 0, pData + u16First, u16Len - u16First);
	}
	else
	{
		W5500WriteBuf(W5500_BSB_TX(u8Socket), u16Offset, pData, u16Len);
	}
}


// ============================ 调试辅助 ============================

// SPI 总线自检: 先确保已调用过 W5500HardwareReset(否则芯片在复位态必然失败)
// 写 MR 寄存器后回读比对, 用于隔离"读恒为 0xFF/0x00"是协议问题还是总线/接线问题
// 返回 true = 总线正常; false = MISO 未响应, 按顺序查:
//   1) RSTN 引脚是否为高(芯片不在复位态)  2) VDD 3.3V/GND
//   3) MISO/MOSI 是否接反  4) CS 是否接到 SCSn  5) 降低 SPI 速率再试
bool W5500SpiSelfTest(void)
{
	uint8_t u8Read;

	// MR bit4 = PB(禁止 ping 响应), 写入无害且随后恢复默认
	W5500WriteReg(W5500_BSB_COMMON, W5500_REG_MR, 0x10);
	u8Read = W5500ReadReg(W5500_BSB_COMMON, W5500_REG_MR);

	// 恢复默认值
	W5500WriteReg(W5500_BSB_COMMON, W5500_REG_MR, 0x00);

	return (u8Read == 0x10);
}


// ============================ 初始化 ============================
volatile uint8_t u8Read = 0;
bool W5500HardwareReset(void)
{
	// CubeMX 将 CS 初始化为低电平, 先释放片选
	W5500_CS_HIGH();
	W5500_RST_HIGH();
	HAL_Delay(10);

	// 复位时序: RSTN 拉低 > 500us 后释放, 等待 PLL 稳定
	W5500_RST_LOW();
	ResetTimerCountUs(&g_u32W5500RstTimer);
	while (GetTimerTickDeltaUs(g_u32W5500RstTimer, GetCurTimerCountUs()) < W5500_RST_HOLD_US);
	W5500_RST_HIGH();
	HAL_Delay(W5500_RST_RECOVER_MS);

	// 版本校验
	u8Read = W5500ReadReg(W5500_BSB_COMMON, W5500_REG_VERSIONR);
	if (W5500ReadReg(W5500_BSB_COMMON, W5500_REG_VERSIONR) != W5500_CHIP_VERSION)
	{
		return false;
	}

	// 配置各 socket 缓冲区大小: socket0 4KB, 其余 1KB, 并计算回卷掩码
	for (uint8_t i = 0; i < W5500_SOCKET_NUM; i++)
	{
		uint8_t u8Size = (i == 0) ? W5500_SOCKET0_BUF_KB : 1;
		W5500WriteReg(W5500_BSB_REG(i), W5500_SREG_TXBUF_SIZE, u8Size);
		W5500WriteReg(W5500_BSB_REG(i), W5500_SREG_RXBUF_SIZE, u8Size);

		g_u16TxMask[i] = (uint16_t)(u8Size * 1024) - 1;
		g_u16RxMask[i] = g_u16TxMask[i];

		// 允许所有 socket 事件触发中断
		W5500WriteReg(W5500_BSB_REG(i), W5500_SREG_IMR, 0xFF);
	}

	// 打开通用中断并清掉残留标志
	W5500WriteReg(W5500_BSB_COMMON, W5500_REG_IMR, 0xFF);
	W5500WriteReg(W5500_BSB_COMMON, W5500_REG_IR, 0xFF);
	W5500WriteReg(W5500_BSB_COMMON, W5500_REG_SIR, 0xFF);
	g_bW5500IrqFlag = false;

	return true;
}


void W5500SetNetInfo(const uint8_t *pMac, const uint8_t *pIp, const uint8_t *pSubnet, const uint8_t *pGw)
{
	W5500WriteBuf(W5500_BSB_COMMON, W5500_REG_SHAR, pMac, 6);
	W5500WriteBuf(W5500_BSB_COMMON, W5500_REG_SIPR, pIp, 4);
	W5500WriteBuf(W5500_BSB_COMMON, W5500_REG_SUBR, pSubnet, 4);
	W5500WriteBuf(W5500_BSB_COMMON, W5500_REG_GAR, pGw, 4);
}


bool W5500GetLinkStatus(void)
{
	// PHYCFGR bit0 = LNK, 1 表示链路已连接
	return (W5500ReadReg(W5500_BSB_COMMON, W5500_REG_PHYCFGR) & 0x01) ? true : false;
}


// ============================ socket 控制 ============================

// 等待 socket 进入指定状态, 超时返回 false
static bool WaitSocketStatus(const uint8_t u8Socket, const uint8_t u8Status)
{
	ResetTimerCountUs(&g_u32W5500WaitTimer);
	while (GetTimerTickDeltaUs(g_u32W5500WaitTimer, GetCurTimerCountUs()) < W5500_WAIT_TIMEOUT_US)
	{
		if (W5500GetSocketStatus(u8Socket) == u8Status)
		{
			return true;
		}
	}
	return false;
}


bool W5500SocketOpen(const uint8_t u8Socket, const uint8_t u8Mode, const uint16_t u16Port)
{
	if (u8Socket >= W5500_SOCKET_NUM)
	{
		return false;
	}

	// 先关闭再配置, 避免残留状态
	W5500WriteReg(W5500_BSB_REG(u8Socket), W5500_SREG_CR, W5500_CR_CLOSE);

	W5500WriteReg16(W5500_BSB_REG(u8Socket), W5500_SREG_PORT, u16Port);
	W5500WriteReg(W5500_BSB_REG(u8Socket), W5500_SREG_MR, u8Mode);
	W5500WriteReg(W5500_BSB_REG(u8Socket), W5500_SREG_CR, W5500_CR_OPEN);

	// TCP 等待进入 INIT, UDP 等待进入 UDP 状态
	return WaitSocketStatus(u8Socket, (u8Mode == W5500_MODE_UDP) ? W5500_SR_UDP : W5500_SR_INIT);
}


bool W5500SocketListen(const uint8_t u8Socket)
{
	W5500WriteReg(W5500_BSB_REG(u8Socket), W5500_SREG_CR, W5500_CR_LISTEN);
	return WaitSocketStatus(u8Socket, W5500_SR_LISTEN);
}


void W5500SocketClose(const uint8_t u8Socket)
{
	W5500WriteReg(W5500_BSB_REG(u8Socket), W5500_SREG_CR, W5500_CR_CLOSE);
}


uint8_t W5500GetSocketStatus(const uint8_t u8Socket)
{
	return W5500ReadReg(W5500_BSB_REG(u8Socket), W5500_SREG_SR);
}


// ============================ 收发 ============================

// 读取 16 位计数值, 连续两次相同才认为有效(芯片内部异步更新)
static uint16_t ReadReg16Stable(const uint8_t u8Bsb, const uint16_t u16Addr)
{
	uint16_t u16Last = W5500ReadReg16(u8Bsb, u16Addr);
	while (true)
	{
		uint16_t u16Cur = W5500ReadReg16(u8Bsb, u16Addr);
		if (u16Cur == u16Last)
		{
			return u16Cur;
		}
		u16Last = u16Cur;
	}
}


uint16_t W5500SocketGetRecvSize(const uint8_t u8Socket)
{
	return ReadReg16Stable(W5500_BSB_RX(u8Socket), W5500_SREG_RX_RSR);
}


int16_t W5500SocketSend(const uint8_t u8Socket, const uint8_t *pData, const uint16_t u16Len)
{
	if (u8Socket >= W5500_SOCKET_NUM || pData == NULL || u16Len == 0)
	{
		return -1;
	}

	// 等待发送空闲缓冲区足够, 超时返回失败
	ResetTimerCountUs(&g_u32W5500WaitTimer);
	while (true)
	{
		if (GetTimerTickDeltaUs(g_u32W5500WaitTimer, GetCurTimerCountUs()) >= W5500_WAIT_TIMEOUT_US)
		{
			return -1;
		}
		if (ReadReg16Stable(W5500_BSB_TX(u8Socket), W5500_SREG_TX_FSR) >= u16Len)
		{
			break;
		}
	}

	// 从写指针位置写入数据(带环形回卷), 然后更新写指针并启动发送
	uint16_t u16Ptr = W5500ReadReg16(W5500_BSB_TX(u8Socket), W5500_SREG_TX_WR);
	W5500WriteSocketBuf(u8Socket, u16Ptr, pData, u16Len);
	W5500WriteReg16(W5500_BSB_TX(u8Socket), W5500_SREG_TX_WR, u16Ptr + u16Len);
	W5500WriteReg(W5500_BSB_TX(u8Socket), W5500_SREG_CR, W5500_CR_SEND);

	return (int16_t)u16Len;
}


int16_t W5500SocketRecv(const uint8_t u8Socket, uint8_t *pData, const uint16_t u16MaxLen)
{
	uint16_t u16Size = W5500SocketGetRecvSize(u8Socket);
	if (u16Size == 0 || pData == NULL)
	{
		return 0;
	}

	// 单次读取不超过接收缓冲区大小
	uint16_t u16Len = (u16Size > u16MaxLen) ? u16MaxLen : u16Size;
	uint16_t u16Ptr = W5500ReadReg16(W5500_BSB_RX(u8Socket), W5500_SREG_RX_RD);
	W5500ReadSocketBuf(u8Socket, u16Ptr, pData, u16Len);

	// 更新读指针并通知芯片数据已取走
	W5500WriteReg16(W5500_BSB_RX(u8Socket), W5500_SREG_RX_RD, u16Ptr + u16Len);
	W5500WriteReg(W5500_BSB_RX(u8Socket), W5500_SREG_CR, W5500_CR_RECV);

	return (int16_t)u16Len;
}


void W5500ClearSocketIrq(const uint8_t u8Socket, const uint8_t u8Mask)
{
	W5500WriteReg(W5500_BSB_REG(u8Socket), W5500_SREG_IR, u8Mask);
}


// ============================ 中断 ============================

void W5500IrqHandler(void)
{
	g_bW5500IrqFlag = true;
}


bool W5500PopIrqFlag(void)
{
	if (g_bW5500IrqFlag)
	{
		g_bW5500IrqFlag = false;
		return true;
	}
	return false;
}


// INTN 引脚下降沿中断回调, 本工程内只有 W5500 使用外部中断
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == INTN_Pin)
	{
		W5500IrqHandler();
	}
}
