#ifndef GENERALPROTOCOL_H
#define GENERALPROTOCOL_H

#include "ProtocolHandle.h"

class GeneralProtocol : public ProtocolHandle
{
public:
    GeneralProtocol();

    QByteArray MotionControlProtocol(const uint8_t &u8Index) override;
};

#endif // GENERALPROTOCOL_H
