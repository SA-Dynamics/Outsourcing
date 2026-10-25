#include "tcphandle.h"
#include <QDebug>

// 未在地址中指定端口时的默认端口(与下位机 Network.h 中 NETWORK_TCP_PORT 一致)
#define TCP_DEFAULT_PORT    5000

TCPHandle::TCPHandle()
    : ConnectiveHandle()
    , m_pSocket(nullptr)
{
}


TCPHandle::~TCPHandle()
{
    CloseChannel();
}


bool TCPHandle::OpenChannel(const QString &qstrInfo)
{
    // 重复打开前先清理旧对象
    CloseChannel();

    // 解析 "ip:port", 缺省端口使用 TCP_DEFAULT_PORT
    QString qstrIp = qstrInfo.trimmed();
    quint16 u16Port = TCP_DEFAULT_PORT;

    int iColonPos = qstrIp.lastIndexOf(':');
    if (iColonPos > 0)
    {
        u16Port = qstrIp.mid(iColonPos + 1).toUShort();
        qstrIp = qstrIp.left(iColonPos);
    }

    if (qstrIp.isEmpty())
    {
        qWarning() << "tcp target is empty";
        return false;
    }

    m_pSocket = new QTcpSocket(this);

    // 本对象位于独立工作线程, 阻塞等待不影响界面; 与串口 open 语义保持一致
    m_pSocket->connectToHost(qstrIp, u16Port);

    if (!m_pSocket->waitForConnected(3000))
    {
        qWarning() << "tcp connect failed:" << qstrIp << u16Port << m_pSocket->errorString();
        delete m_pSocket;
        m_pSocket = nullptr;
        return false;
    }

    connect(m_pSocket, &QTcpSocket::readyRead, this, &TCPHandle::ReceivedDataHandler);

    // 连接被对端断开/网线拔出: 走心跳异常路径, 通知上层清理并提示
    connect(m_pSocket, &QTcpSocket::disconnected, this, [this]()
    {
        qDebug() << "tcp disconnected";
        emit sigHeartBeatUnnormal();
    });

    qDebug() << "tcp connected:" << qstrIp << u16Port;
    return true;
}


void TCPHandle::CloseChannel()
{
    if (m_pSocket)
    {
        m_pSocket->abort();
        delete m_pSocket;
        m_pSocket = nullptr;
    }
}


bool TCPHandle::WriteData(const QByteArray &qbtData)
{
    if (!m_pSocket || m_pSocket->state() != QAbstractSocket::ConnectedState)
    {
        return false;
    }

    return (m_pSocket->write(qbtData) != -1);
}


void TCPHandle::ReceivedDataHandler(void)
{
    // TCP 是字节流, 可能粘包/半包, 基类状态机按帧头帧尾逐字节还原, 无需按定长切分
    FeedReceivedBytes(m_pSocket->readAll());
}
