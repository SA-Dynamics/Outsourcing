#ifndef CONNECTIVEHANDLE_H
#define CONNECTIVEHANDLE_H

#include <QObject>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QByteArray>
#include "generalprotocol.h"

// 通信处理基类: 承载与传输方式无关的公共业务
//   - 指令队列(心跳/运动/停止)的发送、应答匹配、超时检测
//   - 协议帧接收状态机
// 线程模型: 本对象被 moveToThread 到独立工作线程, 所有队列/定时器操作都发生在
// 该线程内, 无任何跨线程数据访问(不使用 QtConcurrent, 从根源上避免数据竞争
// 与对象生命周期问题)
//
// 子类只需实现传输层三件套:
//   OpenChannel / CloseChannel / WriteData
// 并在收到原始字节流时调用 FeedReceivedBytes()
class ConnectiveHandle : public QObject
{
    Q_OBJECT

public:
    explicit ConnectiveHandle(QObject *parent = nullptr);
    ~ConnectiveHandle() override;

public slots:
    void SendMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond);
    void SendStopMotionCommand();

    // 料仓: 运动完成后下位机才回帧(与振动盘同语义), 超时按时长+冗余计算
    void SendBinMotionCommand(const QByteArray &qbtData, const QByteArray &qbtRespond);
    void SendBinStopMotionCommand();

    // 光源: 开启后 1s 内必须收到下位机完成反馈
    void SendLightOnCommand(const QByteArray &qbtData, const QByteArray &qbtRespond);
    void SendLightOffCommand(const QByteArray &qbtData, const QByteArray &qbtRespond);

    // 保存IP: 帧内已含目标 IP, 应答由引擎按命令码 0x10 单独匹配
    // 结果通过 sigSaveIpResult 回传(1=成功, 0=失败, -1=超时)
    void SendSaveIpCommand(const QByteArray &qbtData);

    // 获取IP: 无参数, 应答由引擎按命令码 0x12 单独匹配
    // 结果通过 sigGetIpResult 回传(成功为 "x.x.x.x", 空串=超时)
    void SendGetIpCommand(void);

    // 线程收尾: 在对象所属线程内停止全部定时器并关闭底层通道
    // 由 QThread::finished 直接触发(同线程直连执行), 避免跨线程 killTimer 警告
    void Shutdown(void);

signals:
    void sigSetUpConnective(const QString &qstrInfo);
    void sigConnectiveState(const bool &bState);
    void sigHeartBeatUnnormal();
    void sigMotionRespondUnnormal();
    void sigBinMotionRespondUnnormal();
    void sigLightRespondUnnormal();
    void sigMotionTerminate();

    // 保存IP结果: 1=下位机写入成功, 0=下位机写入失败, -1=应答超时
    // 下位机保存成功后会延时重启, 所以收到 0/1 之后连接随时可能断开
    void sigSaveIpResult(const int &iResult);

    // 获取IP结果: 成功为点分十进制字符串, 空串=应答超时
    void sigGetIpResult(const QString &qstrIp);

protected:
    // 打开连接(串口/TCP), 成功返回 true, 失败返回 false
    virtual bool OpenChannel(const QString &qstrInfo) = 0;

    // 关闭连接并释放传输层资源
    virtual void CloseChannel() = 0;

    // 底层写一段完整数据, 返回是否成功(失败时引擎会在下个周期重试)
    virtual bool WriteData(const QByteArray &qbtData) = 0;

    // 子类在收到原始数据时调用(QSerialPort::readyRead / QTcpSocket::readyRead)
    void FeedReceivedBytes(const QByteArray &qbtRaw);

    bool IsChannelOpen(void) const { return m_bChannelOpen; }

private slots:
    // 统一连接入口: sigSetUpConnective -> OpenChannel -> 启动引擎
    void SetupConnective(const QString &qstrInfo);

    // 10ms 引擎: 未发队首发出/已发队首等待应答或超时
    void OnEngineTick(void);

    // 1s 心跳发送
    void OnHeartBeatTick(void);

    // 接收帧超时, 解析状态复位
    void OnParseRecvTimeout(void);

private:
    void RecvMessageHandle(void);
    void ResetQueues(void);

private:
    enum ParseState
    {
        WaitHeader1, WaitHeader2, WaitCmd, WaitLength, WaitData,
        WaitCheckSum1, WaitCheckSum2, WaitTail1, WaitTail2
    };

    enum class SendCmdType { HeartBeat, MotionCmd, StopMotion,
                             BinMotion, BinStopMotion, LightOn, LightOff, SaveIp, GetIp };

    struct SendCmdStruct
    {
        bool bRespond;                          // 已收到匹配应答
        bool bSent;                             // 已完成底层发送
        uint32_t u32ExpectedRespondTimeThresh;  // 应答超时阈值 ms
        qint64 i64StartTime;                    // 发出时刻 ms
        QByteArray qbtSend;
        QByteArray qbtRespond;
    };

    ParseState m_eParseRecvState = WaitHeader1;
    QByteArray m_qbtRecvData;
    QVector<QByteArray> m_qvecRecvBuffer;

    QMap<SendCmdType, QVector<SendCmdStruct>> m_qmapSendBuffer;
    QTimer *m_pHeartBeatTimer;
    QTimer *m_pEngineTimer;
    QTimer *m_pParseRecvTimer;

    bool m_bChannelOpen;
    ProtocolHandle *m_pProtocol;

    // 保存IP 应答里解析出来的结果字节(1=成功, 0=失败), 由接收匹配写入、
    // 引擎在收到应答后随 sigSaveIpResult 一起发出
    bool m_bSaveIpSuccess = false;

    // 获取IP 应答里解析出来的 IP 字符串, 由接收匹配写入、
    // 引擎在收到应答后随 sigGetIpResult 一起发出
    QString m_qstrGetIpValue;
};

#endif // CONNECTIVEHANDLE_H
