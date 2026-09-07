#ifndef PROTOCOLHANDLE_H
#define PROTOCOLHANDLE_H

#include <QObject>

class ProtocolHandle
{
public:
    virtual ~ProtocolHandle()
    {

    }

public:
    virtual QByteArray MotionControlProtocol(const uint8_t &u8Index) = 0;
};


#endif // PROTOCOLHANDLE_H
