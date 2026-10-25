#ifndef RS485HANDLE_H
#define RS485HANDLE_H

#include "ConnectiveHandle.h"
#include <QSerialPort>
#include <QSerialPortInfo>

// 485/串口传输层: 公共业务(队列/心跳/应答/超时/帧解析)全部在 ConnectiveHandle 基类
class RS485Handle : public ConnectiveHandle
{
    Q_OBJECT

public:
    RS485Handle();
    ~RS485Handle() override;

protected:
    bool OpenChannel(const QString &qstrInfo) override;
    void CloseChannel() override;
    bool WriteData(const QByteArray &qbtData) override;

private slots:
    void ReceivedDataHandler(void);

private:
    QSerialPort *m_pSerial;
};

#endif // RS485HANDLE_H
