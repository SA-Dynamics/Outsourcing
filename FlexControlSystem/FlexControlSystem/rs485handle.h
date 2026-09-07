#ifndef RS485HANDLE_H
#define RS485HANDLE_H

#include "ConnectiveHandle.h"
#include <QSerialPort>
#include <QSerialPortInfo>

class RS485Handle : public ConnectiveHandle
{
public:
    RS485Handle();
    ~RS485Handle();

public slots:
    void SetConnectiveInfo(const QString &qstrInfo) override;
    bool SetupConnective(void) override;
    bool SendCommand(const QByteArray &qbtData) override;


private slots:
    void ReceivedDataHandler(void);

private:
    QString m_qstrCurrentPort;
    QSerialPort *m_pSerial;

    QByteArray m_qbtRecvBuffer;
    bool m_bConnect;
};

#endif // RS485HANDLE_H
