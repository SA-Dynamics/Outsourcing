#ifndef RS485HANDLE_H
#define RS485HANDLE_H

#include "ConnectiveHandle.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include "generalprotocol.h"

class RS485Handle : public ConnectiveHandle
{
public:
    RS485Handle();
    ~RS485Handle();

public slots:
    void SetConnectiveInfo(const QString &qstrInfo) override;
    void SetupConnective(void) override;
    void SendCommand(const QByteArray &qbtData) override;


private slots:
    void ReceivedDataHandler(void);
    void OnHeartBeatTimerTimeout(void);
    void OnSendDataTimerTimeout(void);

private:
    QString m_qstrCurrentPort;
    QSerialPort *m_pSerial;
    QTimer *m_pHeartBeatTimer;
    QTimer *m_pSendDataTimer;

    ProtocolHandle *m_pProtocolHandler;
    QVector<QByteArray> m_qvecSendBuffer;

    QByteArray m_qbtRecvBuffer;
    bool m_bConnect;
};

#endif // RS485HANDLE_H
