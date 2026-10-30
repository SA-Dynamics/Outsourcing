#include "generalprotocol.h"
#include <QDebug>

using namespace GeneralProtocolItems;

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

    qbtSendCmd.append(0xB);
    qbtSendCmd.append(GetMotionIndexCmd(sParams.eIndex));

    uint16_t u16Temp = 0;
    u16Temp = static_cast<uint16_t>(sParams.fVoltage * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));


    u16Temp = static_cast<uint16_t>(sParams.fFrequency * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));


    u16Temp = static_cast<uint16_t>(sParams.fTimeUse * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));


    qbtSendCmd.append(static_cast<uint8_t>(sParams.s16AngleOffset >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(sParams.s16AngleOffset & 0xFF));

    u16Temp = static_cast<uint16_t>(sParams.u8SpeedCoeff * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));


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


// 料仓运动(0x07): 数据域 7 字节, 时间域偏移与振动盘帧一致([9][10], 单位 100ms)
void GeneralProtocol::BinMotionProtocol(const GeneralMotion::MotionParams &sParams,
                                        QByteArray &qbtSendCmd,
                                        QByteArray &qbtRespond)
{
    // 上位机发送
    qbtSendCmd.append(u8FRAME_HEAD1);
    qbtSendCmd.append(u8FRAME_HEAD2);

    qbtSendCmd.append(u8SW_BIN_MOTION_CMD);
    qbtSendCmd.append(0x7);
    // 料仓运动索引固定为 1(协议定义)
    qbtSendCmd.append(uint8_t(0x01));

    uint16_t u16Temp = 0;
    u16Temp = static_cast<uint16_t>(sParams.fVoltage * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));

    u16Temp = static_cast<uint16_t>(sParams.fFrequency * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));

    u16Temp = static_cast<uint16_t>(sParams.fTimeUse * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));

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
                                      const uint16_t u16KeepTime,
                                      QByteArray &qbtSendCmd, QByteArray &qbtRespond)
{
    // 上位机发送
    qbtSendCmd.append(u8FRAME_HEAD1);
    qbtSendCmd.append(u8FRAME_HEAD2);

    qbtSendCmd.append(u8SW_LIGHT_ON_CMD);
    qbtSendCmd.append(0x4);
    qbtSendCmd.append(u8Index);
    qbtSendCmd.append(u8Brightness);
    qbtSendCmd.append(static_cast<uint8_t>(u16KeepTime >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16KeepTime & 0xFF));

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


// 关闭光源(0x0D/0x0E): u8Index = 1/2 只关指定光源, 0 关闭全部
// 两路光源独立控制, 所以关 A 必须能带上索引, 否则会连带把 B 一起关掉
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

