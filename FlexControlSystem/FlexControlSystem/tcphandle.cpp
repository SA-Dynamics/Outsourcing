#include "tcphandle.h"

TCPHandle::TCPHandle()
{

}


TCPHandle::~TCPHandle()
{

}


//void TCPHandle::SetConnectiveInfo(const QString &qstrInfo)
//{
//    m_qstrTargetIP = qstrInfo;
//}


void TCPHandle::SetupConnective(const QString &qstrInfo)
{
    m_pSocket = new QTcpSocket(this);

//    connect(socket, &QTcpSocket::connected, this, &YourClass::onConnected);
//    connect(socket, &QTcpSocket::disconnected, this, &YourClass::onDisconnected);
//    connect(socket, &QTcpSocket::errorOccurred, this, &YourClass::onError);
}


void TCPHandle::SendMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
//    socket->write(data); // [reference:12]
}


void TCPHandle::ReceivedDataHandler(void)
{
//    m_qbtRecvBuffer.append(m_pSocket->readAll());

    // 假设协议规定每帧数据为 15 个字节
//    const int FRAME_SIZE = 15;
//    while (buffer.size() >= FRAME_SIZE)
//    {
//        QByteArray frame = buffer.left(FRAME_SIZE);
//        buffer.remove(0, FRAME_SIZE);
//        processFrame(frame); // 处理完整帧
//    }
}



void TCPHandle::SendStopMotionCommand()
{

}
