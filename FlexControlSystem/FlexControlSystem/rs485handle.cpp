#include "rs485handle.h"
#include <QDebug>
#include <QDateTime>

RS485Handle::RS485Handle()
{
    m_bConnect = false;
    m_pSerial = nullptr;

    // 获取协议实例
    m_pProtocol = &GeneralProtocol::getInstance();

    m_qvecRecvBuffer.clear();
    m_qmapSendBuffer =
    {
        {SendCmdType::HeartBeat, {}},
        {SendCmdType::MotionCmd, {}},
    };

    connect(this, SIGNAL(sigSetUpConnective(QString)), this, SLOT(SetupConnective(QString)));

    m_pSendDataTimer = new QTimer(this);
    connect(m_pSendDataTimer, &QTimer::timeout, this, &RS485Handle::OnSendDataTimerTimeout);

    m_pHeartBeatTimer = new QTimer(this);
    connect(m_pHeartBeatTimer, &QTimer::timeout, this, &RS485Handle::OnHeartBeatTimerTimeout);

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

        // 打卡成功才能开启定时发送
        m_pSendDataTimer->start(10);
        m_pHeartBeatTimer->start(1000);

    }

    // 发送连接状态信号
    emit sigConnectiveState(m_bConnect);
}


void RS485Handle::SendMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
    m_qvecRecvBuffer.clear();
//    qDebug() << qbtData;
//    qDebug() << qbtRespond;

    SendCmdStruct sSendStruct;
    sSendStruct.bWait = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = static_cast<uint16_t>(qbtData[9] << 8) | qbtData[10] + 1000;
    sSendStruct.i64CurrentTime = 0;

    m_qmapSendBuffer[SendCmdType::HeartBeat].append(sSendStruct);
}


void RS485Handle::RecvMessagePreHandle(void)
{
    if (!m_qmapSendBuffer[SendCmdType::HeartBeat].isEmpty())
    {
        if (m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtRespond)
        {
            return;
        }
    }

    if (!m_qmapSendBuffer[SendCmdType::MotionCmd].isEmpty())
    {
        if (m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtRespond)
        {
            return;
        }
    }

    m_qvecRecvBuffer.removeFirst();
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
                    RecvMessagePreHandle();
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
    QByteArray qbtRespond;
    m_pProtocol->SWHeartBeatProtocol(qbtData, qbtRespond);

    SendCmdStruct sSendStruct;
    sSendStruct.bWait = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = 1000;
    sSendStruct.i64CurrentTime = 0;

    m_qmapSendBuffer[SendCmdType::HeartBeat].append(sSendStruct);
}





void RS485Handle::OnSendDataTimerTimeout(void)
{
    if (!m_qmapSendBuffer[SendCmdType::HeartBeat].isEmpty())
    {
        if (m_qmapSendBuffer[SendCmdType::HeartBeat].first().bWait)
        {
            m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();
            if (m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64CurrentTime -
                    m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64StartTime <
                    m_qmapSendBuffer[SendCmdType::HeartBeat].first().u32ExpectedRespondTimeThresh)
            {
                m_bMotionCmdSendAllow = true;

                if (!m_qvecRecvBuffer.isEmpty())
                {
                    // 如果收到回应, 就删除该组指令
                    if (m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtRespond)
                    {
                        m_qvecRecvBuffer.removeFirst();
                        m_qmapSendBuffer[SendCmdType::HeartBeat].removeFirst();
                    }
                }
            }
            else
            {
                // 超时处理
                m_bMotionCmdSendAllow = false;
                m_qmapSendBuffer[SendCmdType::HeartBeat].clear();
                m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
                emit sigHeartBeatUnnormal();
            }
        }
        else
        {
            m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64StartTime = QDateTime::currentMSecsSinceEpoch();
            m_qmapSendBuffer[SendCmdType::HeartBeat].first().bWait = true;

            // 发送buffer中的数据
            m_pSerial->write(m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtSend);
            m_bMotionCmdSendAllow = false;
        }
    }


    if (m_bMotionCmdSendAllow)
    {
        m_bMotionCmdSendAllow = false;

        if (!m_qmapSendBuffer[SendCmdType::MotionCmd].isEmpty())
        {
            if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().bWait)
            {
                m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();
                if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime -
                        m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64StartTime <
                        m_qmapSendBuffer[SendCmdType::MotionCmd].first().u32ExpectedRespondTimeThresh)
                {
                    if (!m_qvecRecvBuffer.isEmpty())
                    {
                        // 如果收到回应, 就删除该组指令
                        if (m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtRespond)
                        {
                            m_qvecRecvBuffer.removeFirst();
                            m_qmapSendBuffer[SendCmdType::MotionCmd].removeFirst();
                        }
                    }
                }
                else
                {
                    // 超时处理
                    m_qmapSendBuffer[SendCmdType::HeartBeat].clear();
                    m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
                    emit sigMotionRespondUnnormal();
                }
            }
            else
            {
                m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64StartTime = QDateTime::currentMSecsSinceEpoch();
                m_qmapSendBuffer[SendCmdType::MotionCmd].first().bWait = true;

                // 发送buffer中的数据
                m_pSerial->write(m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtSend);
            }
        }
    }
}


void RS485Handle::OnParseRecvTimeout(void)
{
    qDebug() << "recv timeout, restart";
    m_eParseRecvState = WaitHeader1;
}
