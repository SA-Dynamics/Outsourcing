#ifndef RS485HANDLE_H
#define RS485HANDLE_H

#include "ConnectiveHandle.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include "generalprotocol.h"
#include <QMap>

class RS485Handle : public ConnectiveHandle
{
public:
    RS485Handle();
    ~RS485Handle();


public slots:
//    void SetConnectiveInfo(const QString &qstrInfo) override;
//    void SetupConnective(void) override;
    void SendMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond) ;


private slots:
    void ReceivedDataHandler(void);
    void OnHeartBeatTimerTimeout(void);
    void OnSendDataTimerTimeout(void);
    void OnParseRecvTimeout(void);
//    void SetConnectiveInfo(const QString &qstrInfo);
    void SetupConnective(const QString &qstrInfo) override;

private:
    QString m_qstrCurrentPort;
    QSerialPort *m_pSerial;
    QTimer *m_pHeartBeatTimer;
    QTimer *m_pSendDataTimer;
    QTimer *m_pParseRecvTimer;

    enum ParseState { WaitHeader1, WaitHeader2, WaitCmd, WaitLength, WaitData, WaitCheckSum1, WaitCheckSum2, WaitTail1, WaitTail2};
    enum class SendCmdType {HeartBeat, MotionCmd, UnknownCmd};

    struct SendCmdStruct
    {
        bool bWait;
        uint32_t u32ExpectedRespondTimeThresh;
        qint64 i64StartTime;
        qint64 i64CurrentTime;
        QByteArray qbtSend;
        QByteArray qbtRespond;
    };

    ParseState m_eParseRecvState = WaitHeader1;

    ProtocolHandle *m_pProtocol;
    QMap<SendCmdType, QVector<SendCmdStruct>> m_qmapSendBuffer;

    QByteArray m_qbtRecvData;
    QVector<QByteArray> m_qvecRecvBuffer;
    bool m_bConnect;

    bool m_bMotionCmdSendAllow;

    void RecvMessagePreHandle(void);
};

#endif // RS485HANDLE_H
