#ifndef TCPHANDLE_H
#define TCPHANDLE_H


#include "ConnectiveHandle.h"
#include <QTcpSocket>

class TCPHandle : public ConnectiveHandle
{
public:
    TCPHandle();
    ~TCPHandle();

public slots:
    void SetConnectiveInfo(const QString &qstrInfo) override;
    void SetupConnective(void) override;
    void SendCommand(const QByteArray &qbtData) override;


private slots:
    void ReceivedDataHandler(void);


private:
    QString m_qstrTargetIP;
    QTcpSocket *m_pSocket;
};

#endif // TCPHANDLE_H
