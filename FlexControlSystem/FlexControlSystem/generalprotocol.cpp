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

    qbtSendCmd.append(0x7);
    qbtSendCmd.append(GetMotionIndexCmd(sParams.eIndex));

    uint16_t u16Temp = 0;
    u16Temp = static_cast<uint16_t>(sParams.fVoltage * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));
//    qDebug() << u16Temp;

    u16Temp = static_cast<uint16_t>(sParams.fFrequency * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));
//    qDebug() << u16Temp;

    u16Temp = static_cast<uint16_t>(sParams.fTimeUse * 10);
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp >> 8));
    qbtSendCmd.append(static_cast<uint8_t>(u16Temp & 0xFF));
//    qDebug() << u16Temp;

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
