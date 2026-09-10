#ifndef GENERALPROTOCOL_H
#define GENERALPROTOCOL_H

#include "ProtocolHandle.h"


namespace GeneralProtocolItems
{
    constexpr uint8_t u8FRAME_HEAD1 = 0x55;
    constexpr uint8_t u8FRAME_HEAD2 = 0xAA;
    constexpr uint8_t u8FRAME_TAIL1 = 0xAA;
    constexpr uint8_t u8FRAME_TAIL2 = 0x55;

    constexpr uint8_t u8SW_HEART_BEAT_CMD = 0x01;
    constexpr uint8_t u8FW_HEART_BEAT_RESPOND = 0x02;

    constexpr uint8_t u8SW_MOTION_CMD = 0x03;
    constexpr uint8_t u8FW_MOTION_RESPOND = 0x04;

    constexpr uint8_t u8SW_MOTION_UP = 0x01;
    constexpr uint8_t u8SW_MOTION_DOWN = 0x02;
    constexpr uint8_t u8SW_MOTION_LEFT = 0x03;
    constexpr uint8_t u8SW_MOTION_RIGHT = 0x04;
    constexpr uint8_t u8SW_MOTION_GATHER = 0x05;
    constexpr uint8_t u8SW_MOTION_HCENTER = 0x06;
    constexpr uint8_t u8SW_MOTION_VCENTER = 0x07;
    constexpr uint8_t u8SW_MOTION_DISPERSE = 0x08;
}


class GeneralProtocol : public ProtocolHandle
{
public:
    GeneralProtocol();

    void MotionControlProtocol(const MotionIndex eIndex,
                                       QByteArray &qbtSendCmd,
                                       QByteArray &qbtRespond) override;

    void SWHeartBeatProtocol(QByteArray &qbtHeart) override;

private:
    void CalcCheckSum(QByteArray &qbtSendCmd);
};

#endif // GENERALPROTOCOL_H
