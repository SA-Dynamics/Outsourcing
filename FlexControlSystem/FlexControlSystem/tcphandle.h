#ifndef TCPHANDLE_H
#define TCPHANDLE_H

#include "ConnectiveHandle.h"
#include <QTcpSocket>

// TCP 传输层: 公共业务(队列/心跳/应答/超时/帧解析)全部在 ConnectiveHandle 基类
class TCPHandle : public ConnectiveHandle
{
    Q_OBJECT

public:
    TCPHandle();
    ~TCPHandle() override;

protected:
    // qstrInfo 格式: "ip:port"(如 "192.168.1.100:5000"), 不带端口时默认 5000
    bool OpenChannel(const QString &qstrInfo) override;
    void CloseChannel() override;
    bool WriteData(const QByteArray &qbtData) override;

private slots:
    void ReceivedDataHandler(void);

private:
    QTcpSocket *m_pSocket;
};

#endif // TCPHANDLE_H
