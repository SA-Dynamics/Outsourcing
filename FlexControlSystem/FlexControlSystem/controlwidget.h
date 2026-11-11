#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include <QThread>
#include "controlpanel.h"
#include "rs485handle.h"
#include "tcphandle.h"
#include "ProtocolHandle.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ControlWidget; }
class QUdpSocket;
class QTimer;
class QCloseEvent;
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

protected:
    // 关闭窗口(点 X / Alt+F4 / 任务栏关闭)统一在这里做退出确认:
    // 连接中退出会断开下位机通信, 动作执行中退出则下位机仍按原指令跑完,
    // 都必须让用户明确知情后再退出
    void closeEvent(QCloseEvent *event) override;

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

    void on_tabPanel_currentChanged(int index);

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

private:
    Ui::ControlWidget *ui;
    ConnectiveHandle *m_ConnectiveHandler;
    QThread *m_pConnectiveThread;
    ControlPanel *m_pControlWidget;
    ControlPanel *m_pModeWidget;
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

    void SetConnectSlot(const bool bSet);
    void CloseConnection(void);
};
#endif // CONTROLWIDGET_H
