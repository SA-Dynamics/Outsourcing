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
// 保存IP 应答超时 ms: 下位机要擦写一页 Flash(约 20~40ms), 再叠加网络往返,
// 3s 足够宽松, 同时不会让界面等太久
#define SAVE_IP_TIMEOUT_MS          3000
// 获取IP 应答超时 ms: 纯查询操作, 下位机组帧即回, 2s 足够
#define GET_IP_TIMEOUT_MS           2000
// 获取电压 应答超时 ms: 同为纯查询, 2s 足够; 周期轮询下超时静默清队列
#define GET_VOLTAGE_TIMEOUT_MS      2000
// 获取固件参数 应答超时 ms: 纯查询(下位机只是把 RAM 里的结构体打包), 2s 足够
#define GET_KEY_PARAMS_TIMEOUT_MS   2000
// 保存固件参数 应答超时 ms: 下位机擦一页 Flash + 逐半字编程 + 回读校验, 与保存IP 同量级
#define SAVE_KEY_PARAMS_TIMEOUT_MS  3000
// 获取固件版本 应答超时 ms: 纯查询(下位机只回 3 个字节), 2s 足够。
// 超时静默: 旧固件没有 0x19, 现场不该因为"读不到版本号"弹框,
// 界面显示 "--" 就够了。
#define GET_FW_VERSION_TIMEOUT_MS   2000
// 接收帧超时 ms(超时后解析状态复位)
#define RECV_PARSE_TIMEOUT_MS       2000
// 无限时间动作(0x03/0x07 数据域末尾标志=1)的响应超时 ms。
// 这类指令下位机只在收到停止指令后才结束, 不会自己回完成帧, 所以给一个
// "实际上永不超时"的值(24h); 真的超时了说明链路已死, 报错比静默等更合适。
// 用户点停止时 SendStopMotionCommand/SendBinStopMotionCommand 会清队列并
// emit sigMotionTerminate, 按钮照常恢复, 不会被这个超时卡住。
#define INFINITE_MOTION_TIMEOUT_MS  (24u * 60u * 60u * 1000u)


// 判断运动类帧是不是"无限时间动作"。帧布局:
//   55 AA | cmd | len | data(N) | chkH chkL | AA 55
// 数据域最后一个字节(无限时间标志)的下标 = size()-5 (0x03 是第 14 字节, 0x07 是第 10 字节)
static bool IsInfiniteTimeFrame(const QByteArray &qbtData)
{
    const int iFlagIndex = qbtData.size() - 5;

    return (iFlagIndex >= 4) && (static_cast<uint8_t>(qbtData.at(iFlagIndex)) == 1);
}


// 读帧内 4 字节大端无符号整数。
// 时间域 2026-11-25 从 2 字节扩到 4 字节, 0x03/0x07 两条帧的时间都落在整帧下标 9~12
// (数据域第 6~9 字节)。越界返回 0, 让超时退回"冗余 2s"而不是读脏数据。
static uint32_t ReadTimeTicks(const QByteArray &qbtData)
{
    const int iOffset = 9;

    if (iOffset + 3 >= qbtData.size())
    {
        return 0;
    }

    return (static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset))) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset + 1))) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset + 2))) << 8) |
            static_cast<uint32_t>(static_cast<uint8_t>(qbtData.at(iOffset + 3)));
}


// 运动/料仓动作的应答超时 = 动作时长(100ms/格 → ms) + 2s 冗余。
// 上限 9999s → 99990 格 → 9 999 000ms, 仍在 uint32 范围内(49 天), 不会溢出。
static uint32_t MotionTimeoutMs(const QByteArray &qbtData)
{
    return ReadTimeTicks(qbtData) * 100u + 2000u;
}


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
        {SendCmdType::SaveIp, {}},
        {SendCmdType::GetIp, {}},
        {SendCmdType::GetVoltage, {}},
        {SendCmdType::GetKeyParams, {}},
        {SendCmdType::SaveKeyParams, {}},
        {SendCmdType::GetFwVersion, {}},
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

    // 运动指令加 2s 的超时冗余: 帧内 [9]~[12] 为运动时长(4 字节大端, 单位 100ms)
    // 无限时间动作(数据域末字节=1)不会自己结束, 改用 24h 的"准无限"超时
    if (IsInfiniteTimeFrame(qbtData))
    {
        sSendStruct.u32ExpectedRespondTimeThresh = INFINITE_MOTION_TIMEOUT_MS;
    }
    else
    {
        sSendStruct.u32ExpectedRespondTimeThresh = MotionTimeoutMs(qbtData);
    }

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

    // 料仓帧的数据域布局与振动盘一致: [9]~[12] 为运动时长(4 字节大端, 单位 100ms)
    // 无限时间动作同样改用 24h 的"准无限"超时
    if (IsInfiniteTimeFrame(qbtData))
    {
        sSendStruct.u32ExpectedRespondTimeThresh = INFINITE_MOTION_TIMEOUT_MS;
    }
    else
    {
        sSendStruct.u32ExpectedRespondTimeThresh = MotionTimeoutMs(qbtData);
    }

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


// 保存IP: 应答有两种(成功/失败), 因此不入"期望应答整帧比对"的常规通道,
// 由 RecvMessageHandle 按命令码 0x10 识别并取结果字节
void ConnectiveHandle::SendSaveIpCommand(const QByteArray &qbtData)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond.clear();
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = SAVE_IP_TIMEOUT_MS;

    qDebug() << "send save ip";
    m_qmapSendBuffer[SendCmdType::SaveIp].append(sSendStruct);
}


// 获取IP: 应答 0x12 数据域为变长 IP, 与保存IP一样按命令码识别, 不走整帧比对
void ConnectiveHandle::SendGetIpCommand(void)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    QByteArray qbtCmd;
    m_pProtocol->GetIpProtocol(qbtCmd);

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtCmd;
    sSendStruct.qbtRespond.clear();
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = GET_IP_TIMEOUT_MS;

    qDebug() << "send get ip";
    m_qmapSendBuffer[SendCmdType::GetIp].append(sSendStruct);
}


// 获取电压: 应答 0x14 按命令码匹配, 与获取IP同构; 周期轮询下超时静默不上报
void ConnectiveHandle::SendGetVoltageCommand(void)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    // 上一拍尚未应答则跳过(2s 周期 > 2s 超时, 正常不会堆积)
    if (!m_qmapSendBuffer[SendCmdType::GetVoltage].isEmpty())
    {
        return;
    }

    QByteArray qbtCmd;
    m_pProtocol->GetVoltageProtocol(qbtCmd);

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtCmd;
    sSendStruct.qbtRespond.clear();
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = GET_VOLTAGE_TIMEOUT_MS;

    m_qmapSendBuffer[SendCmdType::GetVoltage].append(sSendStruct);
}


// 获取固件参数: 应答 0x16 是 49 字节变长内容, 按命令码匹配(与获取电压同构)
// 与电压不同: 这是用户手动点出来的, 超时必须回报(界面要解除按钮禁用并提示), 不能静默
void ConnectiveHandle::SendGetKeyParamsCommand(void)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    // 上一次还没回就再点: 不堆叠, 直接让上一次的超时逻辑收尾
    if (!m_qmapSendBuffer[SendCmdType::GetKeyParams].isEmpty())
    {
        return;
    }

    QByteArray qbtCmd;
    m_pProtocol->GetKeyParamsProtocol(qbtCmd);

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtCmd;
    sSendStruct.qbtRespond.clear();
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = GET_KEY_PARAMS_TIMEOUT_MS;

    m_u8GetKeyStatus = 0xFF;
    m_qbtGetKeyData.clear();

    qDebug() << "send get key params";
    m_qmapSendBuffer[SendCmdType::GetKeyParams].append(sSendStruct);
}


// 保存固件参数: 帧由界面组好(49 字节数据域), 应答 0x18 只有 1 字节, 按命令码匹配
void ConnectiveHandle::SendSaveKeyParamsCommand(const QByteArray &qbtData)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    if (!m_qmapSendBuffer[SendCmdType::SaveKeyParams].isEmpty())
    {
        return;
    }

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtData;
    sSendStruct.qbtRespond.clear();
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = SAVE_KEY_PARAMS_TIMEOUT_MS;

    m_bSaveKeyOk = false;

    qDebug() << "send save key params";
    m_qmapSendBuffer[SendCmdType::SaveKeyParams].append(sSendStruct);
}


// 获取固件版本: 连接成功后读一次, 应答 0x1A 数据域 3 字节, 按命令码匹配。
// 与"获取固件参数"的区别: 这里没有按钮, 也没有重试 —— 读不到就显示 "--",
// 下次重连会再读一次, 所以不需要界面侧的 pending 事务。
void ConnectiveHandle::SendGetFirmwareVersionCommand(void)
{
    if (!m_bChannelOpen)
    {
        return;
    }

    // 重连时可能上一轮还在队列里(极端情况), 不堆叠, 让上一次的超时逻辑收尾
    if (!m_qmapSendBuffer[SendCmdType::GetFwVersion].isEmpty())
    {
        return;
    }

    QByteArray qbtCmd;
    m_pProtocol->GetFirmwareVersionProtocol(qbtCmd);

    SendCmdStruct sSendStruct;
    sSendStruct.bRespond = false;
    sSendStruct.bSent = false;
    sSendStruct.qbtSend = qbtCmd;
    sSendStruct.qbtRespond.clear();
    sSendStruct.i64StartTime = 0;
    sSendStruct.u32ExpectedRespondTimeThresh = GET_FW_VERSION_TIMEOUT_MS;

    qDebug() << "send get firmware version";
    m_qmapSendBuffer[SendCmdType::GetFwVersion].append(sSendStruct);
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

    // ---- 保存IP(应答 0x10, 结果由接收匹配写入 m_bSaveIpSuccess) ----
    QVector<SendCmdStruct> &sSaveIp = m_qmapSendBuffer[SendCmdType::SaveIp];
    if (!sSaveIp.isEmpty())
    {
        SendCmdStruct &sItem = sSaveIp.first();

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
            sSaveIp.removeFirst();
            emit sigSaveIpResult(m_bSaveIpSuccess ? 1 : 0);
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 下位机未应答: 可能是连接已断(保存成功后它会重启), 或指令没送达
            qDebug() << "save ip respond timeout";
            sSaveIp.clear();
            emit sigSaveIpResult(-1);
        }
    }

    // ---- 获取IP(应答 0x12, IP 由接收匹配写入 m_qstrGetIpValue) ----
    QVector<SendCmdStruct> &sGetIp = m_qmapSendBuffer[SendCmdType::GetIp];
    if (!sGetIp.isEmpty())
    {
        SendCmdStruct &sItem = sGetIp.first();

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
            sGetIp.removeFirst();
            emit sigGetIpResult(m_qstrGetIpValue);
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            qDebug() << "get ip respond timeout";
            sGetIp.clear();
            emit sigGetIpResult(QString());
        }
    }

    // ---- 获取电压(应答 0x14, 电压x10 由接收匹配写入 m_u16VoltageX10) ----
    QVector<SendCmdStruct> &sGetVoltage = m_qmapSendBuffer[SendCmdType::GetVoltage];
    if (!sGetVoltage.isEmpty())
    {
        SendCmdStruct &sItem = sGetVoltage.first();

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
            sGetVoltage.removeFirst();
            emit sigGetVoltageResult(m_u16VoltageX10);
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 周期轮询场景: 超时静默清队列, 下一周期重发, 不打扰界面
            qDebug() << "get voltage respond timeout";
            sGetVoltage.clear();
        }
    }

    // ---- 获取固件参数(应答 0x16, 状态与 48 字节数据由接收匹配写入) ----
    QVector<SendCmdStruct> &sGetKey = m_qmapSendBuffer[SendCmdType::GetKeyParams];
    if (!sGetKey.isEmpty())
    {
        SendCmdStruct &sItem = sGetKey.first();

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
            sGetKey.removeFirst();
            emit sigGetKeyParamsResult(m_u8GetKeyStatus, m_qbtGetKeyData);
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 手动操作: 超时必须回报 0xFF, 界面才能解除按钮禁用
            qDebug() << "get key params respond timeout";
            sGetKey.clear();
            emit sigGetKeyParamsResult(0xFF, QByteArray());
        }
    }

    // ---- 保存固件参数(应答 0x18, 结果由接收匹配写入 m_bSaveKeyOk) ----
    QVector<SendCmdStruct> &sSaveKey = m_qmapSendBuffer[SendCmdType::SaveKeyParams];
    if (!sSaveKey.isEmpty())
    {
        SendCmdStruct &sItem = sSaveKey.first();

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
            sSaveKey.removeFirst();
            emit sigSaveKeyParamsResult(m_bSaveKeyOk ? 1 : 0);
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            qDebug() << "save key params respond timeout";
            sSaveKey.clear();
            emit sigSaveKeyParamsResult(-1);
        }
    }

    // ---- 获取固件版本(应答 0x1A, 三个字节由接收匹配写入) ----
    QVector<SendCmdStruct> &sGetFw = m_qmapSendBuffer[SendCmdType::GetFwVersion];
    if (!sGetFw.isEmpty())
    {
        SendCmdStruct &sItem = sGetFw.first();

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
            sGetFw.removeFirst();
            emit sigGetFirmwareVersionResult(QStringLiteral("%1.%2.%3")
                                             .arg(m_u8FwVersionMajor)
                                             .arg(m_u8FwVersionMinor)
                                             .arg(m_u8FwVersionPatch));
        }
        else if (i64Now - sItem.i64StartTime >= sItem.u32ExpectedRespondTimeThresh)
        {
            // 静默: 旧固件没实现 0x19, 不该弹框; 发空串让标签回到 "--"
            qDebug() << "get firmware version respond timeout";
            sGetFw.clear();
            emit sigGetFirmwareVersionResult(QString());
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
    // 光源关闭完成(0x0E): 保持时间到时下位机也会自发该帧(带索引, 0=全关),
    // 无论是否对应队列指令, 都先发 sigLightOffFinish 让界面复位对应路按钮;
    // 该帧不做入队期望帧的整帧比对, 而是按下方兼容规则匹配队列
    else if (qbtFrame.size() >= 8 &&
             static_cast<uint8_t>(qbtFrame.at(2)) == GeneralProtocolItems::u8FW_LIGHT_OFF_RESPOND)
    {
        const uint8_t u8Len = static_cast<uint8_t>(qbtFrame.at(3));
        // 无数据域(长度不为 1)按全关 0 处理
        const uint8_t u8Idx = (u8Len == 1) ? static_cast<uint8_t>(qbtFrame.at(4))
                                           : GeneralProtocolItems::u8SW_LIGHT_INDEX_ALL;

        emit sigLightOffFinish(u8Idx);

        QVector<SendCmdStruct> &sLightOff = m_qmapSendBuffer[SendCmdType::LightOff];
        if (!sLightOff.isEmpty())
        {
            const QByteArray &qbtExpect = sLightOff.first().qbtRespond;
            const uint8_t u8ExpectLen = static_cast<uint8_t>(qbtExpect.at(3));

            // 兼容匹配: 期望帧为全关(len 0)时任何 0x0E 完成帧均可收(到时自发的也算完成);
            // 期望帧带索引(len 1)时仅索引相同的完成帧可收
            bool bMatch = false;
            if (u8ExpectLen == 0)
            {
                bMatch = true;
            }
            else if (u8ExpectLen == 1)
            {
                QByteArray qbtExpectIdx;
                GeneralProtocol::getInstance().LightOffFinishExpect(
                            static_cast<uint8_t>(qbtExpect.at(4)), qbtExpectIdx);
                bMatch = (qbtFrame == qbtExpectIdx);
            }

            if (bMatch)
            {
                sLightOff.first().bRespond = true;
            }
        }
    }
    // 保存IP: 应答帧有两种(1=成功/0=失败), 无法整帧比对, 按命令码 + 数据域识别
    else if (!m_qmapSendBuffer[SendCmdType::SaveIp].isEmpty() &&
             qbtFrame.size() >= 5 &&
             static_cast<uint8_t>(qbtFrame.at(2)) == GeneralProtocolItems::u8FW_SAVE_IP_RESPOND &&
             static_cast<uint8_t>(qbtFrame.at(3)) == 1)
    {
        m_bSaveIpSuccess = (static_cast<uint8_t>(qbtFrame.at(4)) == GeneralProtocolItems::u8SAVE_IP_SUCCESS);
        m_qmapSendBuffer[SendCmdType::SaveIp].first().bRespond = true;
    }
    // 获取IP: 应答 0x12 数据域 4 字节 IP, 按命令码 + 长度识别
    else if (!m_qmapSendBuffer[SendCmdType::GetIp].isEmpty() &&
             qbtFrame.size() >= 8 &&
             static_cast<uint8_t>(qbtFrame.at(2)) == GeneralProtocolItems::u8FW_GET_IP_RESPOND &&
             static_cast<uint8_t>(qbtFrame.at(3)) == 4)
    {
        m_qstrGetIpValue = QStringLiteral("%1.%2.%3.%4")
                .arg(static_cast<uint8_t>(qbtFrame.at(4)))
                .arg(static_cast<uint8_t>(qbtFrame.at(5)))
                .arg(static_cast<uint8_t>(qbtFrame.at(6)))
                .arg(static_cast<uint8_t>(qbtFrame.at(7)));
        m_qmapSendBuffer[SendCmdType::GetIp].first().bRespond = true;
    }
    // 获取电压: 应答 0x14 数据域 2 字节 电压x10(大端), 按命令码+长度识别
    else if (!m_qmapSendBuffer[SendCmdType::GetVoltage].isEmpty() &&
             qbtFrame.size() >= 8 &&
             static_cast<uint8_t>(qbtFrame.at(2)) == GeneralProtocolItems::u8FW_GET_VOLTAGE_RESPOND &&
             static_cast<uint8_t>(qbtFrame.at(3)) == 2)
    {
        m_u16VoltageX10 = static_cast<quint16>(
                    (static_cast<uint8_t>(qbtFrame.at(4)) << 8) |
                     static_cast<uint8_t>(qbtFrame.at(5)));
        m_qmapSendBuffer[SendCmdType::GetVoltage].first().bRespond = true;
    }
    // 获取固件参数: 应答 0x16 数据域 49 字节 = 状态 1B + 12 个 float(48B),
    // 内容随参数变化不能整帧比对, 按命令码 + 长度识别
    else if (!m_qmapSendBuffer[SendCmdType::GetKeyParams].isEmpty() &&
             qbtFrame.size() >= 8 &&
             static_cast<uint8_t>(qbtFrame.at(2)) == GeneralProtocolItems::u8FW_GET_KEY_PARAMS_RESPOND &&
             static_cast<uint8_t>(qbtFrame.at(3)) == (1 + GeneralProtocolItems::iKEY_PARAMS_DATA_LEN))
    {
        m_u8GetKeyStatus = static_cast<uint8_t>(qbtFrame.at(4));
        m_qbtGetKeyData = qbtFrame.mid(5, GeneralProtocolItems::iKEY_PARAMS_DATA_LEN);
        m_qmapSendBuffer[SendCmdType::GetKeyParams].first().bRespond = true;
    }
    // 保存固件参数: 应答 0x18 数据域 1 字节(0=成功 1=失败), 按命令码 + 长度识别
    else if (!m_qmapSendBuffer[SendCmdType::SaveKeyParams].isEmpty() &&
             qbtFrame.size() >= 7 &&
             static_cast<uint8_t>(qbtFrame.at(2)) == GeneralProtocolItems::u8FW_SAVE_KEY_PARAMS_RESPOND &&
             static_cast<uint8_t>(qbtFrame.at(3)) == 1)
    {
        m_bSaveKeyOk = (static_cast<uint8_t>(qbtFrame.at(4)) == GeneralProtocolItems::u8SAVE_KEY_PARAMS_SUCCESS);
        m_qmapSendBuffer[SendCmdType::SaveKeyParams].first().bRespond = true;
    }
    // 获取固件版本: 应答 0x1A 数据域 3 字节(major/minor/patch), 按命令码 + 长度识别
    else if (!m_qmapSendBuffer[SendCmdType::GetFwVersion].isEmpty() &&
             qbtFrame.size() >= 9 &&
             static_cast<uint8_t>(qbtFrame.at(2)) == GeneralProtocolItems::u8FW_GET_FW_VERSION_RESPOND &&
             static_cast<uint8_t>(qbtFrame.at(3)) == GeneralProtocolItems::iFW_VERSION_DATA_LEN)
    {
        m_u8FwVersionMajor = static_cast<uint8_t>(qbtFrame.at(4));
        m_u8FwVersionMinor = static_cast<uint8_t>(qbtFrame.at(5));
        m_u8FwVersionPatch = static_cast<uint8_t>(qbtFrame.at(6));
        m_qmapSendBuffer[SendCmdType::GetFwVersion].first().bRespond = true;
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
