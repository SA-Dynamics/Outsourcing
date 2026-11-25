#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include <QThread>
#include <QDialog>
#include <QPixmap>
#include <QStringList>
#include "controlpanel.h"
#include "rs485handle.h"
#include "tcphandle.h"
#include "ProtocolHandle.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ControlWidget; }
class QUdpSocket;
class QTimer;
class QCloseEvent;
class QSizeGrip;
class QLabel;
QT_END_NAMESPACE

class ControlWidget : public QWidget
{
    Q_OBJECT

public:

    enum class ConnectObject
    {
        ObjectNone,
        ObjectTCP,
        Object485
    };
    Q_ENUM(ConnectObject)

    ControlWidget(QWidget *parent = nullptr);
    ~ControlWidget();

    // 铺满当前屏幕可用区(不含任务栏)。窗口去掉了系统边框后, showMaximized()
    // 会把任务栏一起盖住, 所以最大化统一走自绘标题栏的 availableGeometry 逻辑
    void MaximizeToScreen(void);

    // 刷「状态」卡片里的固件版本号。连接成功后由 0x19/0x1A 事务调一次(只读一次, 不轮询);
    // 传空串按 "--" 显示 —— 断连、应答超时、或下位机是没实现 0x19 的旧固件都走这条。
    void SetFirmwareVersion(const QString &qstrVersion);

protected:
    // 关闭窗口(点 X / Alt+F4 / 任务栏关闭)统一在这里做退出确认:
    // 连接中退出会断开下位机通信, 动作执行中退出则下位机仍按原指令跑完,
    // 都必须让用户明确知情后再退出
    void closeEvent(QCloseEvent *event) override;

    // 无边框窗口没有系统画的 1px 边框, 自己补一圈, 否则窗口和桌面背景糊在一起
    void paintEvent(QPaintEvent *event) override;
    // 右下角 QSizeGrip 的定位(无边框窗口失去系统缩放边框, 靠它保留缩放能力)
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void on_pbUpdatePort_clicked();
    void on_leIP_textChanged(const QString &arg1);
//    void OnMotionButtonClicked(const MotionIndex &eIndex);

    void on_pbTCPConnect_clicked();
    void OnConnectiveState(const bool &bState);

    void on_pbTCPDisconnect_clicked();

    void on_pbRS485Connect_clicked();

    void on_pbRS485Disconnect_clicked();

    void OnConnectiveUnnormal(const QString &qstrName);

    void on_pbOpenMode_clicked();

    void on_pbSaveMode_clicked();

    void on_pbSaveModeAs_clicked();

    void on_pbNewMode_clicked();

    void on_pbControl2Mode_clicked();

    void on_pbMode2Control_clicked();

    void on_pbSaveIP_clicked();

    // 保存IP 的应答结果: 1=成功 0=失败 -1=超时(来自通信工作线程)
    void OnSaveIpResult(const int &iResult);

    void on_pbGetIP_clicked();

    // 获取IP 的应答结果: 成功为点分十进制字符串, 空串=超时(来自通信工作线程)
    void OnGetIpResult(const QString &qstrIp);

    // UDP 发现路径: 收到下位机单播应答 / 广播超时
    void OnUdpGetIpReadyRead();
    void OnUdpGetIpTimeout();

    // 电压周期查询: 2s 一次, 连接后自动开始, 断开停止
    void OnVoltageQueryTick();
    // 电压应答: 电压x10, 换算为 V 显示到 leVoltage(系数由下位机换算, 这里除 10)
    void OnVoltageResult(const quint16 &u16VoltX10);

    // 固件版本应答(0x1A→"主.次.修订"): 空串 = 超时/旧固件不支持, 标签回到 "--"
    void OnFirmwareVersionResult(const QString &qstrVersion);

private:
    Ui::ControlWidget *ui;
    ConnectiveHandle *m_ConnectiveHandler;
    QThread *m_pConnectiveThread;
    ControlPanel *m_pControlWidget;
    ControlPanel *m_pModeWidget;
    // 两个面板共用的一份"运动序列"状态(计数/聚拢保持/节奏快照/owner),
    // 由上面两个面板各持一个指针(见 ControlPanel::UseSharedSequence)
    ControlPanel::MotionSequenceState m_motionSequence;
    ConnectObject m_eConnectObject;

    // 保存IP 事务: 指令发出后置位, 收到结果(或断连)后清零,
    // 用来忽略非本次操作产生的迟到应答
    bool m_bSaveIpPending;
    QString m_qstrSaveIpPending;

    // 获取IP 事务: 语义同上(TCP 路径与 UDP 广播路径共用, 防迟到应答)
    bool m_bGetIpPending;

    // UDP 发现(未连接时获取 IP): 套接字收应答, 单发定时器做超时
    QUdpSocket *m_pGetIpUdpSocket;
    QTimer *m_pGetIpUdpTimer;

    // 电压周期查询定时器(2s): 连接成功启动, 断开/心跳丢失停止
    QTimer *m_pVoltageTimer = nullptr;

    QDialog *m_pConfigDialog = nullptr;

    // 无边框窗口的右下角缩放握把(系统边框已去掉, 缩放能力靠它保留)
    QSizeGrip *m_pSizeGrip = nullptr;

    // ---- 右下角"状态"卡片: 一行 = 电压 + 三盏状态点 + 两个版本号(2026-11-25 十四次) ----
    // 圆点图由 build-check/make_status_dot.cpp 从 image/灰色圆.png、image/绿色圆.png
    // 生成(灰=空闲 / 绿=运行), 逻辑尺寸 16px, 与这一行的 12px 文字等高。
    // 卡片是 .ui 里 groupBox_3 的 gridLayout_18: 原来的「电压(V)」那一行(gridLayout_17)
    // 被整块搬进新建的 StatusLampRow, 后面依次挂 3 个状态点 + 2 个版本号,
    // 上下各一个 Expanding 弹簧让这一行在卡片里垂直居中(.ui 本身不动)。
    enum StatusLampIndex
    {
        StatusLampConnect = 0,   // 连接: TCP/串口 心跳正常=绿; 未连接 或 心跳丢失=灰
        StatusLampMotion,        // 振动盘运动: 任一运动(单条/G1/G2/交替序列)执行中=绿
        StatusLampBin,           // 料仓运动: 运动中=绿; 空闲=灰
        StatusLampCount
    };

    QList<QLabel *> m_lstStatusLamps;
    QPixmap m_pmStatusOff;
    QPixmap m_pmStatusOn;
    QLabel *m_pLabelFirmwareVersion = nullptr;          // 固件版本号(暂为 "--")
    void BuildStatusLamps(void);                        // 构造期一次性把这一行建出来
    void SetStatusLamp(const int iIndex, const bool bOn);   // 换图 + 刷新 tooltip

    void ApplyConfiguration(const bool bSave);
    void SetConnectSlot(const bool bSet);
    void CloseConnection(void);
};
#endif // CONTROLWIDGET_H
