#ifndef PROTOCOLHANDLE_H
#define PROTOCOLHANDLE_H

#include <QObject>

namespace GeneralMotion
{
    struct MotionParams
    {
        enum class MotionIndex
        {
            MotionUp,
            MotionDown,
            MotionLeft,
            MotionRight,
            MotionGather,
            MotionHCentered,
            MotionVCentered,
            MotionDisperse,
            MotionFlip,
        };

        MotionIndex eIndex;
        float fVoltage;
        float fFrequency;
        float fTimeUse;
        int16_t s16AngleOffset;
        uint8_t u8SpeedCoeff;
    };
}

class ProtocolHandle : public QObject
{
    Q_OBJECT

public:

//    Q_ENUM(GeneralMotion::MotionParams)

    // 单例模式
    virtual ~ProtocolHandle() = default;


    virtual void MotionControlProtocol(const GeneralMotion::MotionParams &sParams,
                                       QByteArray &qbtSendCmd,
                                       QByteArray &qbtRespond) = 0;

    virtual void SWHeartBeatProtocol(QByteArray &qbtHeart, QByteArray &qbtRespond) = 0;

    virtual void MotionStopProtocol(QByteArray &qbtCmd, QByteArray &qbtRespond) = 0;
};


#endif // PROTOCOLHANDLE_H
