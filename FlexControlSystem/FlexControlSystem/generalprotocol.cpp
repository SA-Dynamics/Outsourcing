#include "generalprotocol.h"

using namespace GeneralProtocolItems;

GeneralProtocol::GeneralProtocol()
{

}



void GeneralProtocol::MotionControlProtocol(const MotionIndex eIndex,
                                   QByteArray &qbtSendCmd,
                                   QByteArray &qbtRespond)
{
    qbtSendCmd.append(u8FRAME_HEAD1);
    qbtSendCmd.append(u8FRAME_HEAD2);
    qbtSendCmd.append(u8FRAME_TAIL1);
    qbtSendCmd.append(u8FRAME_TAIL2);


    qbtRespond.append(u8FRAME_HEAD1);
    qbtRespond.append(u8FRAME_HEAD2);
    qbtRespond.append(u8FRAME_TAIL1);
    qbtRespond.append(u8FRAME_TAIL2);
}
