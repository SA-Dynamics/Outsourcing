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
}


class GeneralProtocol : public ProtocolHandle
{
public:
    GeneralProtocol();

    void MotionControlProtocol(const MotionIndex eIndex,
                                       QByteArray &qbtSendCmd,
                                       QByteArray &qbtRespond) override;

private:
    void CalcCheckSum(QByteArray &qbtSendCmd);
};

#endif // GENERALPROTOCOL_H
