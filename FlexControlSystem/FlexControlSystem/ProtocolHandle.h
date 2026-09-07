#ifndef PROTOCOLHANDLE_H
#define PROTOCOLHANDLE_H

#include <QObject>

enum class MotionIndex
{
    MotionUp,
    MotionDown,
    MotionLeft,
    MotionRight,
};

class ProtocolHandle : public QObject
{
    Q_OBJECT

public:

    Q_ENUM(MotionIndex)

    virtual ~ProtocolHandle()
    {

    }

public:
    virtual void MotionControlProtocol(const MotionIndex eIndex,
                                       QByteArray &qbtSendCmd,
                                       QByteArray &qbtRespond) = 0;
};


#endif // PROTOCOLHANDLE_H
