#include "rs485handle.h"
#include <QDebug>
#include <QDateTime>
#include <QtConcurrent/QtConcurrentRun>

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
        {SendCmdType::StopMotion, {}},
    };

    connect(this, SIGNAL(sigSetUpConnective(QString)), this, SLOT(SetupConnective(QString)));

//    m_pSendHeartDataTimer = new QTimer(this);
//    connect(m_pSendHeartDataTimer, &QTimer::timeout, this, &RS485Handle::OnSendHeartDataTimerTimeout);

//    m_pSendMotionDataTimer = new QTimer(this);
//    connect(m_pSendMotionDataTimer, &QTimer::timeout, this, &RS485Handle::OnSendMotionDataTimerTimeout);

    m_pHeartBeatTimer = new QTimer(this);
    connect(m_pHeartBeatTimer, &QTimer::timeout, this, &RS485Handle::OnHeartBeatTimerTimeout);

    m_pParseRecvTimer = new QTimer(this);
    connect(m_pParseRecvTimer, &QTimer::timeout, this, &RS485Handle::OnParseRecvTimeout);
    m_pParseRecvTimer->setSingleShot(true);
}


RS485Handle::~RS485Handle()
{
    m_pSendHeartDataTimer->stop();
    m_pSendMotionDataTimer->stop();
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

        m_eParseRecvState = WaitHeader1;
        // 打卡成功才能开启定时发送
        //m_pSendHeartDataTimer->start(10);
        m_pHeartBeatTimer->start(1000);
        m_pSendMotionDataTimer->start(10);

    }

    // 发送连接状态信号
    emit sigConnectiveState(m_bConnect);
}


void RS485Handle::SendMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
    // 发送运动指令
    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;

    // 运动指令加2s的超时冗余
    sSendStruct.u32ExpectedRespondTimeThresh = (static_cast<uint16_t>(qbtData[9] << 8) | qbtData[10]) * 100 + 2000;
    sSendStruct.i64CurrentTime = 0;

    qDebug() << "send motion";
    m_qmapSendBuffer[SendCmdType::MotionCmd].append(sSendStruct);
    m_pSerial->write(m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtSend);


    QtConcurrent::run([this]()
    {
        for (int i = 0; i < 200; i++)
        {
            m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();

            if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime -
                    m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64StartTime <
                    m_qmapSendBuffer[SendCmdType::MotionCmd].first().u32ExpectedRespondTimeThresh)
            {
                QMutexLocker Locker(&m_Mutex);
                if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().bRespond)
                {
                    m_qmapSendBuffer[SendCmdType::MotionCmd].removeFirst();
                    break;
                }
                else
                {
                    QThread::msleep(10);
                }
            }
            else
            {
                // 超时
                m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
                emit sigMotionRespondUnnormal();
                break;
            }
        }
    });
}


void RS485Handle::SendStopMotionCommand()
{
    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->MotionStopProtocol(qbtData, qbtRespond);

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = 1000;
    sSendStruct.i64CurrentTime = 0;

    qDebug() << "send stop motion";
    m_qmapSendBuffer[SendCmdType::StopMotion].append(sSendStruct);

    m_pSerial->write(m_qmapSendBuffer[SendCmdType::StopMotion].first().qbtSend);


    QtConcurrent::run([this]()
    {
        for (int i = 0; i < 200; i++)
        {
            m_qmapSendBuffer[SendCmdType::StopMotion].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();

            if (m_qmapSendBuffer[SendCmdType::StopMotion].first().i64CurrentTime -
                    m_qmapSendBuffer[SendCmdType::StopMotion].first().i64StartTime <
                    m_qmapSendBuffer[SendCmdType::StopMotion].first().u32ExpectedRespondTimeThresh)
            {
                QMutexLocker Locker(&m_Mutex);
                if (m_qmapSendBuffer[SendCmdType::StopMotion].first().bRespond)
                {
                    m_qmapSendBuffer[SendCmdType::StopMotion].removeFirst();
                    break;
                }
                else
                {
                    QThread::msleep(10);
                }
            }
            else
            {
                // 超时
                m_qmapSendBuffer[SendCmdType::StopMotion].clear();
                emit sigMotionRespondUnnormal();
                break;
            }
        }
    });
}


void RS485Handle::RecvMessageHandle(void)
{
    QMutexLocker Locker(&m_Mutex);

    if (!m_qmapSendBuffer[SendCmdType::HeartBeat].isEmpty() &&
        m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::HeartBeat].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::MotionCmd].isEmpty() &&
             m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::MotionCmd].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::StopMotion].isEmpty() &&
             m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::StopMotion].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::StopMotion].first().bRespond = true;
    }
    else
    {
        qDebug() << "remove error recv info" << m_qvecRecvBuffer.first();
        m_qvecRecvBuffer.removeFirst();
    }
}


void RS485Handle::ReceivedDataHandler(void)
{
    QByteArray qbtRaw = m_pSerial->readAll();
    qDebug() << qbtRaw;
    for (uint8_t u8Byte : qbtRaw)
    {
        switch (m_eParseRecvState)
        {
            case WaitHeader1:
                if (u8Byte == GeneralProtocolItems::u8FRAME_HEAD1)
                {
                    qDebug() << "get fh1" << u8Byte;
                    m_pParseRecvTimer->start(2000);
                    m_qbtRecvData.clear();
                    m_qbtRecvData.append(u8Byte);
                    m_eParseRecvState = WaitHeader2;
                }
                break;

            case WaitHeader2:
                if (u8Byte == GeneralProtocolItems::u8FRAME_HEAD2)
                {
                    qDebug() << "get fh2" << u8Byte;
                    m_qbtRecvData.append(u8Byte);
                    m_eParseRecvState = WaitCmd;
                }
                break;

            case WaitCmd:
                qDebug() << "get cmd" << u8Byte;
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitLength;
                break;

            case WaitLength:
                qDebug() << "get length" << u8Byte;
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitData;
                break;

            case WaitData:                
                if (m_qbtRecvData.size() - 4 >= m_qbtRecvData.at(3))
                {
                    m_eParseRecvState = WaitCheckSum1;
                }
                else
                {
                    qDebug() << "get data" << u8Byte;
                    m_qbtRecvData.append(u8Byte);
                    break;
                }


            case WaitCheckSum1:
                qDebug() << "get cs1:" << u8Byte;
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitCheckSum2;
                break;

            case WaitCheckSum2:
                qDebug() << "get cs2:" << u8Byte;
                m_qbtRecvData.append(u8Byte);
                m_eParseRecvState = WaitTail1;
                break;

            case WaitTail1:
                if (u8Byte == GeneralProtocolItems::u8FRAME_TAIL1)
                {
                    qDebug() << "get t1" << u8Byte;
                    m_qbtRecvData.append(u8Byte);
                    m_eParseRecvState = WaitTail2;
                }
                break;

            case WaitTail2:
                if (u8Byte == GeneralProtocolItems::u8FRAME_TAIL2)
                {
                    qDebug() << "get t2" << u8Byte;
                    m_pParseRecvTimer->stop();
                    m_qbtRecvData.append(u8Byte);
                    m_qvecRecvBuffer.append(m_qbtRecvData);
                    RecvMessageHandle();
                    m_eParseRecvState = WaitHeader1;
                    qDebug() << "total data" << m_qvecRecvBuffer;
                }
                break;

            default:
                break;
        }
    }
}


void RS485Handle::OnHeartBeatTimerTimeout(void)
{
    if (m_qmapSendBuffer[SendCmdType::HeartBeat].isEmpty())
    {
        // 获取心跳数据协议，发送心跳
        QByteArray qbtData;
        QByteArray qbtRespond;
        m_pProtocol->SWHeartBeatProtocol(qbtData, qbtRespond);

        SendCmdStruct sSendStruct;
        sSendStruct.bRespond = false;
        sSendStruct.qbtSend = qbtData;
        sSendStruct.qbtRespond = qbtRespond;
        sSendStruct.i64StartTime = 0;
        sSendStruct.u32ExpectedRespondTimeThresh = 1000;
        sSendStruct.i64CurrentTime = 0;

        m_qmapSendBuffer[SendCmdType::HeartBeat].append(sSendStruct);
        m_pSerial->write(m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtSend);


        QtConcurrent::run([this]()
        {
            for (int i = 0; i < 200; i++)
            {
                m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();

                if (m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64CurrentTime -
                        m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64StartTime <
                        m_qmapSendBuffer[SendCmdType::HeartBeat].first().u32ExpectedRespondTimeThresh)
                {
                    QMutexLocker Locker(&m_Mutex);
                    if (m_qmapSendBuffer[SendCmdType::HeartBeat].first().bRespond)
                    {
                        m_qmapSendBuffer[SendCmdType::HeartBeat].removeFirst();
                        break;
                    }
                    else
                    {
                        QThread::msleep(10);
                    }
                }
                else
                {
                    // 超时
                    m_qmapSendBuffer[SendCmdType::HeartBeat].clear();
                    m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
                    emit sigHeartBeatUnnormal();
                    break;
                }
            }
        });
    }
}





//void RS485Handle::OnSendHeartDataTimerTimeout(void)
//{
//    if (!m_qmapSendBuffer[SendCmdType::HeartBeat].isEmpty())
//    {
//        if (m_qmapSendBuffer[SendCmdType::HeartBeat].first().bWait)
//        {
//            // 当前是在等待回应
//            m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();
//            if (m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64CurrentTime -
//                    m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64StartTime <
//                    m_qmapSendBuffer[SendCmdType::HeartBeat].first().u32ExpectedRespondTimeThresh)
//            {
//                m_bMotionCmdSendAllow = true;

//                if (!m_qvecRecvBuffer.isEmpty())
//                {
//                    // 如果收到回应, 就删除该组指令
//                    if (m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtRespond)
//                    {
////                        qDebug() << "mcu heart beat received";
//                        m_qvecRecvBuffer.removeFirst();
//                        m_qmapSendBuffer[SendCmdType::HeartBeat].removeFirst();
//                    }
//                }
//            }
//            else
//            {
//                // 超时处理
//                m_pSendHeartDataTimer->stop();
//                m_bMotionCmdSendAllow = false;
//                m_qmapSendBuffer[SendCmdType::HeartBeat].clear();
//                m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
//                emit sigHeartBeatUnnormal();
//            }
//        }
//        else
//        {
//            m_qmapSendBuffer[SendCmdType::HeartBeat].first().i64StartTime = QDateTime::currentMSecsSinceEpoch();
//            m_qmapSendBuffer[SendCmdType::HeartBeat].first().bWait = true;

//            // 发送buffer中的数据
//            m_pSerial->write(m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtSend);
//            m_bMotionCmdSendAllow = false;
//        }
//    }
//    else
//    {
//        m_bMotionCmdSendAllow = true;
//    }
//}


//void RS485Handle::OnSendMotionDataTimerTimeout(void)
//{
//    if (m_bMotionCmdSendAllow)
//    {
//        m_bMotionCmdSendAllow = false;

//        if (!m_qmapSendBuffer[SendCmdType::MotionCmd].isEmpty())
//        {
//            if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().bWait)
//            {
//                // 当前是在等待回应
//                m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();
//                if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime -
//                        m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64StartTime <
//                        m_qmapSendBuffer[SendCmdType::MotionCmd].first().u32ExpectedRespondTimeThresh)
//                {
//                    //qDebug() << "check motion respond in recv buffer" << m_qvecRecvBuffer;
//                    if (!m_qvecRecvBuffer.isEmpty())
//                    {

//                        // 如果收到回应, 就删除该组指令
//                        if (m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtRespond)
//                        {
//                            qDebug() << "mcu motion respond received";
//                            m_qvecRecvBuffer.removeFirst();
//                            m_qmapSendBuffer[SendCmdType::MotionCmd].removeFirst();
//                            emit sigMotionTerminate();
//                        }
//                    }
//                }
//                else
//                {
//                    // 超时处理
//                    m_qmapSendBuffer[SendCmdType::HeartBeat].clear();
//                    m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
//                    emit sigMotionRespondUnnormal();
//                }
//            }
//            else
//            {
//                m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64StartTime = QDateTime::currentMSecsSinceEpoch();
//                m_qmapSendBuffer[SendCmdType::MotionCmd].first().bWait = true;

//                // 发送buffer中的数据
//                qDebug() << "send a motion data frame";
//                m_pSerial->write(m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtSend);
//            }
//        }

//        if (!m_qmapSendBuffer[SendCmdType::StopMotion].isEmpty())
//        {
//            // 清空发送队列
//            m_qmapSendBuffer[SendCmdType::MotionCmd].clear();

//            if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().bWait)
//            {
//                // 当前是在等待回应
//                m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime = QDateTime::currentMSecsSinceEpoch();
//                if (m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64CurrentTime -
//                        m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64StartTime <
//                        m_qmapSendBuffer[SendCmdType::MotionCmd].first().u32ExpectedRespondTimeThresh)
//                {
//                    //qDebug() << "check motion respond in recv buffer" << m_qvecRecvBuffer;
//                    if (!m_qvecRecvBuffer.isEmpty())
//                    {

//                        // 如果收到回应, 就删除该组指令
//                        if (m_qvecRecvBuffer.first() == m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtRespond)
//                        {
//                            qDebug() << "mcu motion respond received";
//                            m_qvecRecvBuffer.removeFirst();
//                            m_qmapSendBuffer[SendCmdType::MotionCmd].removeFirst();
//                            emit sigMotionTerminate();
//                        }
//                    }
//                }
//                else
//                {
//                    // 超时处理
//                    m_qmapSendBuffer[SendCmdType::HeartBeat].clear();
//                    m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
//                    emit sigMotionRespondUnnormal();
//                }
//            }
//            else
//            {
//                m_qmapSendBuffer[SendCmdType::MotionCmd].first().i64StartTime = QDateTime::currentMSecsSinceEpoch();
//                m_qmapSendBuffer[SendCmdType::MotionCmd].first().bWait = true;

//                // 发送buffer中的数据
//                qDebug() << "send a motion data frame";
//                m_pSerial->write(m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtSend);
//            }
//        }
//    }
//}


void RS485Handle::OnParseRecvTimeout(void)
{
    qDebug() << "recv timeout, restart";
    m_eParseRecvState = WaitHeader1;
    m_pParseRecvTimer->stop();
}
