#include "generalprotocol.h"
#include <QDebug>
#include <cstring>

using namespace GeneralProtocolItems;


// 把一个 float 按 IEEE754 大端追加到字节数组(协议 0x16/0x17 用的就是这种"网络字节序")
// 注意: 不能直接 append(reinterpret_cast<const char*>(&f), 4) —— 那是小端机器的内存布局,
// 下位机按大端解析会得到完全不同的数值
static void AppendFloatBigEndian(QByteArray &qbtData, const float fValue)
{
    uint32_t u32Bits = 0;
    memcpy(&u32Bits, &fValue, sizeof(u32Bits));

    qbtData.append(static_cast<char>(static_cast<uint8_t>((u32Bits >> 24) & 0xFFu)));
    qbtData.append(static_cast<char>(static_cast<uint8_t>((u32Bits >> 16) & 0xFFu)));
    qbtData.append(static_cast<char>(static_cast<uint8_t>((u32Bits >> 8) & 0xFFu)));
    qbtData.append(static_cast<char>(static_cast<uint8_t>(u32Bits & 0xFFu)));
}


// 追加一个 32 位无符号整数(大端)。
// 时间域 2026-11-25 从 2 字节扩到 4 字节: 上限 9999s, 按 ×10 下发是 99990,
// 已经装不进 uint16(65535), 必须走 32 位, 否则 6553.5s 以上会回绕成小值。
static void AppendUInt32BigEndian(QByteArray &qbtData, const uint32_t u32Value)
{
    qbtData.append(static_cast<char>(static_cast<uint8_t>((u32Value >> 24) & 0xFFu)));
    qbtData.append(static_cast<char>(static_cast<uint8_t>((u32Value >> 16) & 0xFFu)));
    qbtData.append(static_cast<char>(static_cast<uint8_t>((u32Value >> 8) & 0xFFu)));
    qbtData.append(static_cast<char>(static_cast<uint8_t>(u32Value & 0xFFu)));
}


// 秒 → 协议时间刻度(单位 100ms, 即秒数×10)。
// 用四舍五入而不是直接截断: 0.8f 在二进制里是 0.8000000119..., 乘 10 得 7.9999995,
// 截断会下发 7(0.7s), 一帧就差 100ms。上限 9999s → 99990, 由调用方保证不越界。
static uint32_t TimeToTicks(const float fSeconds)
{
    if (fSeconds <= 0.0f)
    {
        return 0;
    }

    return static_cast<uint32_t>(static_cast<double>(fSeconds) * 10.0 + 0.5);
}


// 从字节数组的 iOffset 处按大端读一个 float
static float ReadFloatBigEndian(const QByteArray &qbtData, const int iOffset)
{
    const uint32_t u32Bits =
            (static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset))) << 24) |
            (static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset + 1))) << 16) |
            (static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset + 2))) << 8) |
             static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset + 3)));

    float fValue = 0.0f;
    memcpy(&fValue, &u32Bits, sizeof(fValue));

    return fValue;
}

//GeneralProtocol::GeneralProtocol()
//{

//}

GeneralProtocol::~GeneralProtocol()
{

}


uint8_t GeneralProtocol::GetMotionIndexCmd(const GeneralMotion::MotionParams::MotionIndex eIndex)
{
    uint8_t u8Value = 0;

    switch (eIndex)
    {
        case GeneralMotion::MotionParams::MotionIndex::MotionUp:
            u8Value = 1;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionDown:
            u8Value = 2;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionLeft:
            u8Value = 3;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionRight:
            u8Value = 4;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionGather:
            u8Value = 5;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionHCentered:
            u8Value = 6;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionVCentered:
            u8Value = 7;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionDisperse:
            u8Value = 8;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionFlip:
            u8Value = 9;
            break;

        // 料仓: 走独立的 0x07 协议(BinMotionProtocol 内索引写死 0x01),
        // 这里补上分支, 避免将来复用本函数时 MotionBin 落进 default 变成 0
        case GeneralMotion::MotionParams::MotionIndex::MotionBin:
            u8Value = 10;
            break;

        default:
            break;
    }

    return u8Value;
}


void GeneralProtocol::AddCheckSum(QByteArray &qbtData)
{
    uint16_t u16CheckSum = 0;

    for (int i = 2; i < qbtData.size(); i++)
    {
        u16CheckSum += static_cast<uint8_t>(qbtData.at(i));
    }

    u16CheckSum = ~u16CheckSum + 1;

    qbtData.append(static_cast<uint8_t>(u16CheckSum >> 8));
    qbtData.append(static_cast<uint8_t>(u16CheckSum & 0xFF));

}


void GeneralProtocol::MotionControlProtocol(const GeneralMotion::MotionParams &sParams,
                                   QByteArray &qbtSendCmd,
                                   QByteArray &qbtRespond)
{
    // 将发送数据协议化
    qbtSendCmd.append(u8FRAME_HEAD1);
    qbtSendCmd.append(u8FRAME_HEAD2);

    qbtSendCmd.append(u8SW_MOTION_CMD);

    // 数据域 14 字节:
    //   1=索引 2~3=电压 4~5=频率 6~9=时间(4B) 10~11=角度 12~13=系数 14=无限时间标志
    // 2026-11-25 时间域 2→4 字节(上限 9999s); 2026-11-22 末尾追加"无限时间"标志
    qbtSendCmd.append(0xE);
    qbtSendCmd.append(GetMotionIndexCmd(sParams.eIndex));

    uint16_t u16Temp = 0;
    u16Temp = static_cast<uint16_t>(sParams.fVoltage * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));


    u16Temp = static_cast<uint16_t>(sParams.fFrequency * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));


    // 数据域第 6~9 byte: 时间(秒×10), 4 字节大端
    AppendUInt32BigEndian(qbtSendCmd, TimeToTicks(sParams.fTimeUse));


    qbtSendCmd.append(static_cast<uint8_t>(sParams.s16AngleOffset >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(sParams.s16AngleOffset & 0xFF));

    // 数据域第 10~11 byte: ModifyCoeff 位置修正系数(有符号)
    // 不再有 speed 概念, 这里直接下发整数系数, 不做 *10 缩放;
    // 负值按补码下发, 下位机以 int8 取低字节, 高字节(符号扩展位)被截断
    const int16_t s16ModifyCoeff = static_cast<int16_t>(sParams.s8ModifyCoeff);
    qbtSendCmd.append(static_cast<uint8_t>(s16ModifyCoeff >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(s16ModifyCoeff & 0xFF));

    // 数据域第 14 字节: 1=无限时间动作, 0=按上面的时间域计时
    qbtSendCmd.append(static_cast<uint8_t>(sParams.bInfiniteTime ? 1 : 0));


    AddCheckSum(qbtSendCmd);

    qbtSendCmd.append(u8FRAME_TAIL1);
    qbtSendCmd.append(u8FRAME_TAIL2);

    qDebug() << "motion data:" << qbtSendCmd.toHex();

    // 生成对应的应答协议
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_MOTION_RESPOND);
    qbtRespond.append(0x2);
    qbtRespond.append(GetMotionIndexCmd(sParams.eIndex));
    qbtRespond.append(0x1);

    AddCheckSum(qbtRespond);

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


void GeneralProtocol::SWHeartBeatProtocol(QByteArray &qbtHeart, QByteArray &qbtRespond)
{
    // 上位机发送
    qbtHeart.append(u8FRAME_HEAD1);
    qbtHeart.append(u8FRAME_HEAD2);

    qbtHeart.append(u8SW_HEART_BEAT_CMD);
    qbtHeart.append(uint8_t(0));
    qbtHeart.append(uint8_t(0xFF));
    qbtHeart.append(uint8_t(0xFF));

    qbtHeart.append(u8FRAME_TAIL1);
    qbtHeart.append(u8FRAME_TAIL2);

    // 下位机应答
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_HEART_BEAT_RESPOND);
    qbtRespond.append(uint8_t(0));
    qbtRespond.append(uint8_t(0xFF));
    qbtRespond.append(uint8_t(0xFE));

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


void GeneralProtocol::MotionStopProtocol(QByteArray &qbtCmd, QByteArray &qbtRespond)
{
    // 上位机发送
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_MOTION_STOP_CMD);
    qbtCmd.append(uint8_t(0));
    qbtCmd.append(uint8_t(0xFF));
    qbtCmd.append(uint8_t(0xFB));

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    // 下位机应答
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_MOTION_STOP_RESPOND);
    qbtRespond.append(uint8_t(0));
    qbtRespond.append(uint8_t(0xFF));
    qbtRespond.append(uint8_t(0xFA));

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


// 料仓运动(0x07): 数据域 10 字节, 时间域位置与振动盘帧一致([9]~[12], 单位 100ms)
void GeneralProtocol::BinMotionProtocol(const GeneralMotion::MotionParams &sParams,
                                        QByteArray &qbtSendCmd,
                                        QByteArray &qbtRespond)
{
    // 上位机发送
    qbtSendCmd.append(u8FRAME_HEAD1);
    qbtSendCmd.append(u8FRAME_HEAD2);

    qbtSendCmd.append(u8SW_BIN_MOTION_CMD);
    // 数据域 10 字节: 索引 1 + 电压 2 + 频率 2 + 时间 4 + 无限时间标志 1
    // 2026-11-25 时间域 2→4 字节(上限 9999s); 2026-11-22 末尾追加"无限时间"标志
    qbtSendCmd.append(0xA);
    // 料仓运动索引固定为 1(协议定义)
    qbtSendCmd.append(uint8_t(0x01));

    uint16_t u16Temp = 0;
    u16Temp = static_cast<uint16_t>(sParams.fVoltage * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));

    u16Temp = static_cast<uint16_t>(sParams.fFrequency * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));

    // 数据域第 6~9 byte: 时间(秒×10), 4 字节大端
    AppendUInt32BigEndian(qbtSendCmd, TimeToTicks(sParams.fTimeUse));

    // 数据域第 10 字节: 1=无限时间动作, 0=按时间域计时
    qbtSendCmd.append(static_cast<uint8_t>(sParams.bInfiniteTime ? 1 : 0));

    AddCheckSum(qbtSendCmd);

    qbtSendCmd.append(u8FRAME_TAIL1);
    qbtSendCmd.append(u8FRAME_TAIL2);

    qDebug() << "bin motion data:" << qbtSendCmd.toHex();

    // 下位机反馈: 运动完成后发送, 帧内带回运动索引
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_BIN_MOTION_RESPOND);
    qbtRespond.append(0x2);
    qbtRespond.append(uint8_t(0x01));
    qbtRespond.append(0x1);

    AddCheckSum(qbtRespond);

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


// 料仓停止(0x09/0x0A): 双方均无数据域, 校验按同一规则计算
void GeneralProtocol::BinMotionStopProtocol(QByteArray &qbtCmd, QByteArray &qbtRespond)
{
    // 上位机发送
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_BIN_MOTION_STOP_CMD);
    qbtCmd.append(uint8_t(0));

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "bin stop data:" << qbtCmd.toHex();

    // 下位机应答
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_BIN_MOTION_STOP_RESPOND);
    qbtRespond.append(uint8_t(0));

    AddCheckSum(qbtRespond);

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


// 打开光源(0x0B): 亮度 0~100, 保持时间单位 100ms; 应答 0x0C 带回光源索引
void GeneralProtocol::LightOnProtocol(const uint8_t u8Index, const uint8_t u8Brightness,
                                      const uint32_t u32KeepTime, const bool bInfiniteKeep,
                                      QByteArray &qbtSendCmd, QByteArray &qbtRespond)
{
    // 上位机发送
    qbtSendCmd.append(u8FRAME_HEAD1);
    qbtSendCmd.append(u8FRAME_HEAD2);

    qbtSendCmd.append(u8SW_LIGHT_ON_CMD);
    // 数据域 7 字节: 索引 1 + 亮度 1 + 保持时间 4 + 无限保持标志 1
    // 2026-11-25 保持时间 2→4 字节(上限 9999s); 2026-11-22 末尾追加"无限保持"标志
    qbtSendCmd.append(0x7);
    qbtSendCmd.append(u8Index);
    qbtSendCmd.append(u8Brightness);
    // 第 3~6 字节: 保持时间(秒×10), 4 字节大端
    AppendUInt32BigEndian(qbtSendCmd, u32KeepTime);
    // 第 7 字节: 1=无限保持(等价于下位机的"保持时间 0 = 常亮"), 0=按保持时间
    qbtSendCmd.append(static_cast<uint8_t>(bInfiniteKeep ? 1 : 0));

    AddCheckSum(qbtSendCmd);

    qbtSendCmd.append(u8FRAME_TAIL1);
    qbtSendCmd.append(u8FRAME_TAIL2);

    qDebug() << "light on data:" << qbtSendCmd.toHex();

    // 下位机应答: 光源索引, 上位机要求 1s 内收到
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_LIGHT_ON_RESPOND);
    qbtRespond.append(0x1);
    qbtRespond.append(u8Index);

    AddCheckSum(qbtRespond);

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


// 关闭光源(0x0D/0x0E): u8Index = 1/2/3/4 只关 A/B/C/D, 0 关闭全部
// 各路光源独立控制, 关闭指定路时必须带上索引
void GeneralProtocol::LightOffProtocol(const uint8_t u8Index, QByteArray &qbtCmd, QByteArray &qbtRespond)
{
    // 上位机发送
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_LIGHT_OFF_CMD);

    if (u8Index == u8SW_LIGHT_INDEX_ALL)
    {
        // 关闭全部: 保持协议原始格式(长度 0, 无数据域)
        qbtCmd.append(uint8_t(0));
    }
    else
    {
        // 关闭指定光源: 数据域 1 字节索引(协议扩展, 下位机两种都兼容)
        qbtCmd.append(uint8_t(1));
        qbtCmd.append(u8Index);
    }

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "light off data:" << qbtCmd.toHex();

    // 下位机应答
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_LIGHT_OFF_RESPOND);
    qbtRespond.append(uint8_t(0));

    AddCheckSum(qbtRespond);

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


// 光源关闭完成期望帧(0x0E): 55 AA 0E 01 idx chkH chkL AA 55
// 下位机保持时间到熄灯时自发该帧(带回索引), 由 ConnectiveHandle 用于匹配
void GeneralProtocol::LightOffFinishExpect(const uint8_t u8Index, QByteArray &qbtRespond)
{
    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);

    qbtRespond.append(u8FW_LIGHT_OFF_RESPOND);
    qbtRespond.append(uint8_t(1));
    qbtRespond.append(u8Index);

    AddCheckSum(qbtRespond);

    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}


// 获取电压(0x13): 数据域 1 字节 0x01; 下位机回 0x14 + 2 字节 电压x10(大端)
void GeneralProtocol::GetVoltageProtocol(QByteArray &qbtCmd)
{
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_GET_VOLTAGE_CMD);
    qbtCmd.append(uint8_t(1));
    qbtCmd.append(uint8_t(0x01));

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "get voltage cmd:" << qbtCmd.toHex();
}


// 保存IP(0x0F): 数据域 4 字节 IP; 下位机写完 Flash 回 0x10(1=成功 0=失败)
// 应答帧有两种, 不能预先生成期望帧做整帧比对, 由 ConnectiveHandle 按命令码匹配
void GeneralProtocol::SaveIpProtocol(const QByteArray &qbtIp, QByteArray &qbtCmd)
{
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_SAVE_IP_CMD);
    qbtCmd.append(uint8_t(4));

    // 防御: 不足 4 字节按 0 补齐, 多出的截断, 保证帧长与长度域一致
    QByteArray qbtIpData = qbtIp.left(4);
    while (qbtIpData.size() < 4)
    {
        qbtIpData.append(uint8_t(0));
    }
    qbtCmd.append(qbtIpData);

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "save ip data:" << qbtCmd.toHex();
}


// 获取IP(0x11): 数据域 1 字节 0x01; 下位机回 0x12 + 4 字节 IP
// 应答数据域为变长 IP, 由 ConnectiveHandle 按命令码匹配, 不走整帧比对
void GeneralProtocol::GetIpProtocol(QByteArray &qbtCmd)
{
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_GET_IP_CMD);
    qbtCmd.append(uint8_t(1));
    qbtCmd.append(uint8_t(0x01));

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "get ip cmd:" << qbtCmd.toHex();
}


// 12 个 float -> 48 字节大端(协议 0x16/0x17 数据域主体)
// 顺序必须与 GeneralKeyParams::KeyParams 的定义、下位机 KeyParamsPack() 完全一致
void GeneralProtocol::KeyParamsToBytes(const GeneralKeyParams::KeyParams &sParams, QByteArray &qbtData)
{
    AppendFloatBigEndian(qbtData, sParams.fUpStartPhaseLeading);
    AppendFloatBigEndian(qbtData, sParams.fUpPhaseOffset);
    AppendFloatBigEndian(qbtData, sParams.fDownStartPhaseLeading);
    AppendFloatBigEndian(qbtData, sParams.fDownPhaseOffset);
    AppendFloatBigEndian(qbtData, sParams.fLeftStartPhaseLeading);
    AppendFloatBigEndian(qbtData, sParams.fLeftPhaseOffset);
    AppendFloatBigEndian(qbtData, sParams.fRightStartPhaseLeading);
    AppendFloatBigEndian(qbtData, sParams.fRightPhaseOffset);
    AppendFloatBigEndian(qbtData, sParams.fHGatherStartPhaseLeading);
    AppendFloatBigEndian(qbtData, sParams.fHGatherPhaseOffset);
    AppendFloatBigEndian(qbtData, sParams.fVGatherStartPhaseLeading);
    AppendFloatBigEndian(qbtData, sParams.fVGatherPhaseOffset);
}


// 48 字节大端 -> 12 个 float; 长度不够直接判失败(调用方据此提示"数据不完整")
bool GeneralProtocol::KeyParamsFromBytes(const QByteArray &qbtData, GeneralKeyParams::KeyParams &sParams)
{
    const int iLen = static_cast<int>(iKEY_PARAMS_DATA_LEN);

    if (qbtData.size() < iLen)
    {
        return false;
    }

    sParams.fUpStartPhaseLeading      = ReadFloatBigEndian(qbtData, 0);
    sParams.fUpPhaseOffset            = ReadFloatBigEndian(qbtData, 4);
    sParams.fDownStartPhaseLeading    = ReadFloatBigEndian(qbtData, 8);
    sParams.fDownPhaseOffset          = ReadFloatBigEndian(qbtData, 12);
    sParams.fLeftStartPhaseLeading    = ReadFloatBigEndian(qbtData, 16);
    sParams.fLeftPhaseOffset          = ReadFloatBigEndian(qbtData, 20);
    sParams.fRightStartPhaseLeading   = ReadFloatBigEndian(qbtData, 24);
    sParams.fRightPhaseOffset         = ReadFloatBigEndian(qbtData, 28);
    sParams.fHGatherStartPhaseLeading = ReadFloatBigEndian(qbtData, 32);
    sParams.fHGatherPhaseOffset       = ReadFloatBigEndian(qbtData, 36);
    sParams.fVGatherStartPhaseLeading = ReadFloatBigEndian(qbtData, 40);
    sParams.fVGatherPhaseOffset       = ReadFloatBigEndian(qbtData, 44);

    return true;
}


// 获取固件参数(0x15): 数据域 1 字节 0x01; 下位机回 0x16 + 49 字节
// 应答内容随参数变化, 不生成期望帧, 由 ConnectiveHandle 按命令码匹配
void GeneralProtocol::GetKeyParamsProtocol(QByteArray &qbtCmd)
{
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_GET_KEY_PARAMS_CMD);
    qbtCmd.append(uint8_t(1));
    qbtCmd.append(uint8_t(0x01));

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "get key params cmd:" << qbtCmd.toHex();
}


// 保存固件参数(0x17): 数据域 49 字节 = 标志 1 + 12 个 float(48 字节大端)
// 下位机写完 Flash 回 0x18(0=成功 1=失败), 两种应答也不预构造期望帧
void GeneralProtocol::SaveKeyParamsProtocol(const GeneralKeyParams::KeyParams &sParams, QByteArray &qbtCmd)
{
    QByteArray qbtData;
    KeyParamsToBytes(sParams, qbtData);

    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_SAVE_KEY_PARAMS_CMD);
    qbtCmd.append(uint8_t(1 + iKEY_PARAMS_DATA_LEN));
    qbtCmd.append(u8SAVE_KEY_PARAMS_FLAG);
    qbtCmd.append(qbtData);

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "save key params data:" << qbtCmd.toHex();
}


// 获取固件版本(0x19): 数据域 1 字节 0x01; 下位机回 0x1A + 3 字节(主/次/修订)
// 与获取电压同构: 应答内容随固件变化, 不生成期望帧, 由 ConnectiveHandle 按命令码匹配
void GeneralProtocol::GetFirmwareVersionProtocol(QByteArray &qbtCmd)
{
    qbtCmd.append(u8FRAME_HEAD1);
    qbtCmd.append(u8FRAME_HEAD2);

    qbtCmd.append(u8SW_GET_FW_VERSION_CMD);
    qbtCmd.append(uint8_t(1));
    qbtCmd.append(uint8_t(0x01));

    AddCheckSum(qbtCmd);

    qbtCmd.append(u8FRAME_TAIL1);
    qbtCmd.append(u8FRAME_TAIL2);

    qDebug() << "get firmware version cmd:" << qbtCmd.toHex();
}

