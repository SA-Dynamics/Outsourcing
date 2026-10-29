#include "ConnectiveHandle.h"
#include <QDebug>
#include <QDateTime>

// 引擎周期 ms
#define ENGINE_TICK_MS              10
// 心跳发送周期 ms
#define HEART_BEAT_PERIOD_MS        1000
// 心跳应答超时 ms
#define HEART_BEAT_TIMEOUT_MS       1000
// 停止指令应答超时 ms
#define STOP_MOTION_TIMEOUT_MS      1000
// 料仓停止指令应答超时 ms
#define BIN_STOP_TIMEOUT_MS         1000
// 光源开关应答超时 ms(要求光源开启后 1s 内必须收到完成反馈)
#define LIGHT_RESPOND_TIMEOUT_MS    1000
// 接收帧超时 ms(超时后解析状态复位)
#define RECV_PARSE_TIMEOUT_MS       2000


ConnectiveHandle::ConnectiveHandle(QObject *parent)
    : QObject(parent)
    , m_bChannelOpen(false)
{
    m_pProtocol = &GeneralProtocol::getInstance();

    m_qmapSendBuffer =
    {
        {SendCmdType::HeartBeat, {}},
        {SendCmdType::MotionCmd, {}},
        {SendCmdType::StopMotion, {}},
        {SendCmdType::BinMotion, {}},
        {SendCmdType::BinStopMotion, {}},
        {SendCmdType::LightOn, {}},
        {SendCmdType::LightOff, {}},
    };

    connect(this, SIGNAL(sigSetUpConnective(QString)), this, SLOT(SetupConnective(QString)));

    m_pHeartBeatTimer = new QTimer(this);
    connect(m_pHeartBeatTimer, &QTimer::timeout, this, &ConnectiveHandle::OnHeartBeatTick);

    m_pEngineTimer = new QTimer(this);
    m_pEngineTimer->setInterval(ENGINE_TICK_MS);
    connect(m_pEngineTimer, &QTimer::timeout, this, &ConnectiveHandle::OnEngineTick);

    m_pParseRecvTimer = new QTimer(this);
    m_pParseRecvTimer->setSingleShot(true);
    connect(m_pParseRecvTimer, &QTimer::timeout, this, &ConnectiveHandle::OnParseRecvTimeout);
}


ConnectiveHandle::~ConnectiveHandle()
{
    // 正常路径下 Shutdown() 已在工作线程内完成清理, 这里只是兜底
    // (若线程从未启动, 对象一直属于 GUI 线程, 直接停止也无警告)
    m_pHeartBeatTimer->stop();
    m_pEngineTimer->stop();
    m_pParseRecvTimer->stop();
}


// ============================ 连接管理 ============================

// 线程收尾: 必须在对象所属线程内执行, 否则定时器/串口会跨线程被关
void ConnectiveHandle::Shutdown(void)
{
    m_pHeartBeatTimer->stop();
    m_pEngineTimer->stop();
    m_pParseRecvTimer->stop();
    m_bChannelOpen = false;
    CloseChannel();
}

void ConnectiveHandle::SetupConnective(const QString &qstrInfo)
{
    m_bChannelOpen = OpenChannel(qstrInfo);

    if (m_bChannelOpen)
    {
        ResetQueues();
        m_eParseRecvState = WaitHeader1;
        m_pHeartBeatTimer->start(HEART_BEAT_PERIOD_MS);
        m_pEngineTimer->start();
    }

    // 无论成功与否都通知上层
    emit sigConnectiveState(m_bChannelOpen);
}


void ConnectiveHandle::ResetQueues(void)
{
    for (auto it = m_qmapSendBuffer.begin(); it != m_qmapSendBuffer.end(); ++it)
    {
        it.value().clear();
    }
    m_qvecRecvBuffer.clear();
}


// ============================ 指令发送(由子类槽转发而来) ============================

void ConnectiveHandle::SendMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;

    // 运动指令加 2s 的超时冗余: 帧内 [9][10] 为运动时长(单位 100ms)
    sSendStruct.u32ExpectedRespondTimeThresh =
            (static_cast<uint16_t>(static_cast<uint8_t>(qbtData[9]) << 8) | static_cast<uint8_t>(qbtData[10])) * 100 + 2000;

    qDebug() << "send motion";
    m_qmapSendBuffer[SendCmdType::MotionCmd].append(sSendStruct);
    // 队首的发送与超时统一由引擎处理
}


void ConnectiveHandle::SendStopMotionCommand()
{
    if (!m_bChannelOpen)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->MotionStopProtocol(qbtData, qbtRespond);

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = STOP_MOTION_TIMEOUT_MS;

    qDebug() << "send stop motion";

    // 停止指令优先: 丢弃尚未完成的运动指令, 并恢复界面按钮
    if (!m_qmapSendBuffer[SendCmdType::MotionCmd].isEmpty())
    {
        m_qmapSendBuffer[SendCmdType::MotionCmd].clear();
        emit sigMotionTerminate();
    }

    m_qmapSendBuffer[SendCmdType::StopMotion].append(sSendStruct);
}


// ============================ 料仓与光源指令 ============================

// 料仓运动: 与振动盘同语义, 下位机运动完成后才回帧, 因此超时 = 时长 + 冗余
void ConnectiveHandle::SendBinMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;

    // 料仓帧的数据域布局与振动盘一致: [9][10] 为运动时长(单位 100ms)
    sSendStruct.u32ExpectedRespondTimeThresh =
            (static_cast<uint16_t>(static_cast<uint8_t>(qbtData[9]) << 8) | static_cast<uint8_t>(qbtData[10])) * 100 + 2000;

    qDebug() << "send bin motion";
    m_qmapSendBuffer[SendCmdType::BinMotion].append(sSendStruct);
}


void ConnectiveHandle::SendBinStopMotionCommand()
{
    if (!m_bChannelOpen)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->BinMotionStopProtocol(qbtData, qbtRespond);

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = BIN_STOP_TIMEOUT_MS;

    qDebug() << "send bin stop motion";

    // 停止指令优先: 丢弃尚未完成的料仓运动指令, 并恢复界面按钮
    if (!m_qmapSendBuffer[SendCmdType::BinMotion].isEmpty())
    {
        m_qmapSendBuffer[SendCmdType::BinMotion].clear();
        emit sigMotionTerminate();
    }

    m_qmapSendBuffer[SendCmdType::BinStopMotion].append(sSendStruct);
}


void ConnectiveHandle::SendLightOnCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;
    // 光源开启后 1s 内必须收到下位机完成反馈
    sSendStruct.u32ExpectedRespondTimeThresh = LIGHT_RESPOND_TIMEOUT_MS;

    qDebug() << "send light on";
    m_qmapSendBuffer[SendCmdType::LightOn].append(sSendStruct);
}


void ConnectiveHandle::SendLightOffCommand(const QByteArray &qbtData, const QByteArray &qbtRespond)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond = qbtRespond;
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = LIGHT_RESPOND_TIMEOUT_MS;

    qDebug() << "send light off";
    m_qmapSendBuffer[SendCmdType::LightOff].append(sSendStruct);
}


// ============================ 引擎: 发送/应答/超时 ============================

void ConnectiveHandle::OnEngineTick(void)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    const qint64 i64Now = QDateTime::currentMSecsSinceEpoch();

    // ---- 心跳 ----
    QVector<SendCmdStruct> &sHeart = m_qmapSendBuffer[SendCmdType::HeartBeat];
    if (!sHeart.isEmpty())
    {
        SendCmdStruct &sItem = sHeart.first();

        if (!sItem.bSent)
        {
            if (WriteData(sItem.qbtSend))
            {
                sItem.bSent = true;
                sItem.i64StartTime = i64Now;
            }
        }
        else if (sItem.bRespond)
        {
            sHeart.removeFirst();
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 心跳超时: 连接视为异常, 清空所有队列并通知上层
            qDebug() << "heartbeat respond timeout";
            m_pHeartBeatTimer->stop();
            m_pEngineTimer->stop();
            ResetQueues();
            emit sigHeartBeatUnnormal();
            return;
        }
    }

    // ---- 运动指令 ----
    QVector<SendCmdStruct> &sMotion = m_qmapSendBuffer[SendCmdType::MotionCmd];
    if (!sMotion.isEmpty())
    {
        SendCmdStruct &sItem = sMotion.first();

        if (!sItem.bSent)
        {
            if (WriteData(sItem.qbtSend))
            {
                sItem.bSent = true;
                sItem.i64StartTime = i64Now;
            }
        }
        else if (sItem.bRespond)
        {
            sMotion.removeFirst();
            // 运动完成, 通知界面恢复按钮
            emit sigMotionTerminate();
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 运动应答超时
            qDebug() << "motion respond timeout";
            sMotion.clear();
            emit sigMotionRespondUnnormal();
            emit sigMotionTerminate();
        }
    }

    // ---- 停止指令 ----
    QVector<SendCmdStruct> &sStop = m_qmapSendBuffer[SendCmdType::StopMotion];
    if (!sStop.isEmpty())
    {
        SendCmdStruct &sItem = sStop.first();

        if (!sItem.bSent)
        {
            if (WriteData(sItem.qbtSend))
            {
                sItem.bSent = true;
                sItem.i64StartTime = i64Now;
            }
        }
        else if (sItem.bRespond)
        {
            sStop.removeFirst();
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 停止应答超时
            qDebug() << "stop respond timeout";
            sStop.clear();
            emit sigMotionRespondUnnormal();
        }
    }

    // ---- 料仓运动(应答在运动完成后才到, 超时含运动时长) ----
    QVector<SendCmdStruct> &sBinMotion = m_qmapSendBuffer[SendCmdType::BinMotion];
    if (!sBinMotion.isEmpty())
    {
        SendCmdStruct &sItem = sBinMotion.first();

        if (!sItem.bSent)
        {
            if (WriteData(sItem.qbtSend))
            {
                sItem.bSent = true;
                sItem.i64StartTime = i64Now;
            }
        }
        else if (sItem.bRespond)
        {
            sBinMotion.removeFirst();
            // 料仓运动完成, 通知界面恢复按钮
            emit sigMotionTerminate();
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 料仓运动应答超时
            qDebug() << "bin motion respond timeout";
            sBinMotion.clear();
            emit sigBinMotionRespondUnnormal();
            emit sigMotionTerminate();
        }
    }

    // ---- 料仓停止 ----
    QVector<SendCmdStruct> &sBinStop = m_qmapSendBuffer[SendCmdType::BinStopMotion];
    if (!sBinStop.isEmpty())
    {
        SendCmdStruct &sItem = sBinStop.first();

        if (!sItem.bSent)
        {
            if (WriteData(sItem.qbtSend))
            {
                sItem.bSent = true;
                sItem.i64StartTime = i64Now;
            }
        }
        else if (sItem.bRespond)
        {
            sBinStop.removeFirst();
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 料仓停止应答超时
            qDebug() << "bin stop respond timeout";
            sBinStop.clear();
            emit sigBinMotionRespondUnnormal();
        }
    }

    // ---- 光源开启(1s 内必须收到完成反馈) ----
    QVector<SendCmdStruct> &sLightOn = m_qmapSendBuffer[SendCmdType::LightOn];
    if (!sLightOn.isEmpty())
    {
        SendCmdStruct &sItem = sLightOn.first();

        if (!sItem.bSent)
        {
            if (WriteData(sItem.qbtSend))
            {
                sItem.bSent = true;
                sItem.i64StartTime = i64Now;
            }
        }
        else if (sItem.bRespond)
        {
            sLightOn.removeFirst();
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 光源开启反馈超时
            qDebug() << "light on respond timeout";
            sLightOn.clear();
            emit sigLightRespondUnnormal();
        }
    }

    // ---- 光源关闭 ----
    QVector<SendCmdStruct> &sLightOff = m_qmapSendBuffer[SendCmdType::LightOff];
    if (!sLightOff.isEmpty())
    {
        SendCmdStruct &sItem = sLightOff.first();

        if (!sItem.bSent)
        {
            if (WriteData(sItem.qbtSend))
            {
                sItem.bSent = true;
                sItem.i64StartTime = i64Now;
            }
        }
        else if (sItem.bRespond)
        {
            sLightOff.removeFirst();
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 光源关闭反馈超时
            qDebug() << "light off respond timeout";
            sLightOff.clear();
            emit sigLightRespondUnnormal();
        }
    }
}


void ConnectiveHandle::OnHeartBeatTick(void)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    // 队列为空才发新的心跳(上一拍尚未应答则跳过, 由引擎负责超时)
    if (m_qmapSendBuffer[SendCmdType::HeartBeat].isEmpty())
    {
        QByteArray qbtData;
        QByteArray qbtRespond;
        m_pProtocol->SWHeartBeatProtocol(qbtData, qbtRespond);

        SendCmdStruct sSendStruct;
        sSendStruct.bRespond = false;
        sSendStruct.bSent = false;
        sSendStruct.qbtSend = qbtData;
        sSendStruct.qbtRespond = qbtRespond;
        sSendStruct.i64StartTime = 0;
        sSendStruct.u32ExpectedRespondTimeThresh = HEART_BEAT_TIMEOUT_MS;

        m_qmapSendBuffer[SendCmdType::HeartBeat].append(sSendStruct);
    }
}


// ============================ 接收与应答匹配 ============================

// 收到完整帧后与各队列队首的期望应答比对
void ConnectiveHandle::RecvMessageHandle(void)
{
    if (m_qvecRecvBuffer.isEmpty())
    {
        return;
    }

    const QByteArray qbtFrame = m_qvecRecvBuffer.first();

    if (!m_qmapSendBuffer[SendCmdType::HeartBeat].isEmpty() &&
        qbtFrame == m_qmapSendBuffer[SendCmdType::HeartBeat].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::HeartBeat].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::MotionCmd].isEmpty() &&
             qbtFrame == m_qmapSendBuffer[SendCmdType::MotionCmd].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::MotionCmd].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::StopMotion].isEmpty() &&
             qbtFrame == m_qmapSendBuffer[SendCmdType::StopMotion].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::StopMotion].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::BinMotion].isEmpty() &&
             qbtFrame == m_qmapSendBuffer[SendCmdType::BinMotion].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::BinMotion].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::BinStopMotion].isEmpty() &&
             qbtFrame == m_qmapSendBuffer[SendCmdType::BinStopMotion].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::BinStopMotion].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::LightOn].isEmpty() &&
             qbtFrame == m_qmapSendBuffer[SendCmdType::LightOn].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::LightOn].first().bRespond = true;
    }
    else if (!m_qmapSendBuffer[SendCmdType::LightOff].isEmpty() &&
             qbtFrame == m_qmapSendBuffer[SendCmdType::LightOff].first().qbtRespond)
    {
        m_qmapSendBuffer[SendCmdType::LightOff].first().bRespond = true;
    }

    qDebug() << "remove recv info" << qbtFrame;
    m_qvecRecvBuffer.removeFirst();
}


// 接收帧状态机: 55 AA | 类型 | 长度 | 数据 | 校验高 | 校验低 | AA 55
void ConnectiveHandle::FeedReceivedBytes(const QByteArray &qbtRaw)
{
    for (uint8_t u8Byte : qbtRaw)
    {
        switch (m_eParseRecvState)
        {
            case WaitHeader1:
                if (u8Byte == GeneralProtocolItems::u8FRAME_HEAD1)
                {
                    m_pParseRecvTimer->start(RECV_PARSE_TIMEOUT_MS);
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
                if (m_qbtRecvData.size() - 4 >= m_qbtRecvData.at(3))
                {
                    m_eParseRecvState = WaitCheckSum1;
                }
                else
                {
                    m_qbtRecvData.append(u8Byte);
                    break;
                }
                // 故意穿透: 数据长度已满时当前字节属于校验高字节

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


void ConnectiveHandle::OnParseRecvTimeout(void)
{
    qDebug() << "recv timeout, restart";
    m_eParseRecvState = WaitHeader1;
    m_pParseRecvTimer->stop();
}
