#include "rs485handle.h"
#include <QDebug>

RS485Handle::RS485Handle()
{
    m_bConnect = false;
    m_pSerial = nullptr;

    m_pProtocolHandler = new GeneralProtocol();


    m_qvecRecvBuffer.clear();

    connect(this, SIGNAL(sigSetUpConnective(QString)), this, SLOT(SetupConnective(QString)));

    m_pSendDataTimer = new QTimer(this);
    connect(m_pSendDataTimer, &QTimer::timeout, this, &RS485Handle::OnSendDataTimerTimeout);
    m_pSendDataTimer->start(10);

    m_pHeartBeatTimer = new QTimer(this);
    connect(m_pHeartBeatTimer, &QTimer::timeout, this, &RS485Handle::OnHeartBeatTimerTimeout);
    m_pHeartBeatTimer->start(1000);

    m_pParseRecvTimer = new QTimer(this);
    connect(m_pParseRecvTimer, &QTimer::timeout, this, &RS485Handle::OnParseRecvTimeout);
    m_pParseRecvTimer->setSingleShot(true);
}


RS485Handle::~RS485Handle()
{
    m_pSendDataTimer->stop();
    m_pHeartBeatTimer->stop();

    if (m_bConnect)
    {
        m_pSerial->close();
    }
}


//void RS485Handle::SetConnectiveInfo(const QString &qstrInfo)
//{
//    m_qstrCurrentPort = qstrInfo;
//}


void RS485Handle::SetupConnective(const QString &qstrInfo)
{
    m_pSerial = new QSerialPort();
    m_pSerial->setPortName(qstrInfo);              // 设置端口名
    m_pSerial->setBaudRate(QSerialPort::Baud115200);        // 设置波特率
    m_pSerial->setDataBits(QSerialPort::Data8);             // 设置数据位
    m_pSerial->setParity(QSerialPort::NoParity);            // 设置校验位
    m_pSerial->setStopBits(QSerialPort::OneStop);           // 设置停止位

    if (m_pSerial->open(QIODevice::ReadWrite))
    {
        // 打开成功
        m_bConnect = true;
        connect(m_pSerial, &QSerialPort::readyRead, this, &RS485Handle::ReceivedDataHandler);
    }

    emit sigConnectiveState(m_bConnect);
}


void RS485Handle::SendCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
    m_qvecRecvBuffer.clear();

}


void RS485Handle::ReceivedDataHandler(void)
{
    QByteArray qbtRaw = m_pSerial->readAll();

    for (uint8_t u8Byte : qbtRaw)
    {
        switch (m_eParseRecvState)
        {
            case WaitHeader1:
                if (u8Byte == GeneralProtocolItems::u8FRAME_HEAD1)
                {
                    m_pParseRecvTimer->start(1000);
                    m_qbtRecvData.clear();
                    m_qbtRecvData.append(u8Byte);
                    m_eParseRecvState = WaitHeader2;
                }
                break;

            case WaitHeader2:
                if (u8Byte == GeneralProtocolItems::u8FRAME_HEAD2)
                {
                    m_qbtRecvData.append(u8Byte);
                    m_eParseRecvState = WaitCmd;
                }
                break;

            case WaitCmd:
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitLength;
                break;

            case WaitLength:
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitData;
                break;

            case WaitData:
                m_qbtRecvData.append(u8Byte);
                if (m_qbtRecvData.size() - 4 >= m_qbtRecvData.at(3))
                {
                    m_eParseRecvState = WaitCheckSum1;
                }
                break;

            case WaitCheckSum1:
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitCheckSum2;
                break;

            case WaitCheckSum2:
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitTail1;
                break;

            case WaitTail1:
                if (u8Byte == GeneralProtocolItems::u8FRAME_TAIL1)
                {
                    m_qbtRecvData.append(u8Byte);
                    m_eParseRecvState = WaitTail2;
                }
                break;

            case WaitTail2:
                if (u8Byte == GeneralProtocolItems::u8FRAME_TAIL2)
                {
                    m_pParseRecvTimer->stop();
                    m_qbtRecvData.append(u8Byte);
                    m_qvecRecvBuffer.append(m_qbtRecvData);
                    m_eParseRecvState = WaitHeader1;
                    qDebug() << m_qvecRecvBuffer;
                }
                break;

            default:
                break;
        }
    }
}


void RS485Handle::OnHeartBeatTimerTimeout(void)
{
    // 获取心跳数据协议，发送心跳
    QByteArray qbtData;
    m_pProtocolHandler->SWHeartBeatProtocol(qbtData);

    m_qvecSendBuffer.append(qbtData);
}


void RS485Handle::OnSendDataTimerTimeout(void)
{
    // 发送buffer中的数据
    if (!m_qvecSendBuffer.empty())
    {
        QByteArray qbtData = m_qvecSendBuffer.first();
        m_pSerial->write(qbtData);
        m_qvecSendBuffer.removeFirst();
    }
}


void RS485Handle::OnParseRecvTimeout(void)
{
    qDebug() << "recv timeout, restart";
    m_eParseRecvState = WaitHeader1;
}
