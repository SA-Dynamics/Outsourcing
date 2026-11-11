#include "controlwidget.h"
#include "ui_controlwidget.h"
#include <QDebug>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QStringList>
#include <QCloseEvent>
#include <QPushButton>
#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTimer>
#include "generalprotocol.h"


namespace
{
// 生成"整页深色底"的样式表, 只作用于 tabPanel 的某一页(按 objectName 限定):
// 底色铺到该页自身 + 内层 QTabWidget 的 pane/tab, 让深色真正露出来;
// QLabel/QGroupBox/CheckBox 的文字提亮保证可读;
// 按钮/输入框等仍由系统样式绘制, 不做改动。
QString MakePanelStyle(const QString &qstrPageId, const QString &qstrBase,
                       const QString &qstrBorder, const QString &qstrTab,
                       const QString &qstrText)
{
    return QStringLiteral(
               "QWidget#%1 { background-color: %2; }"
               "QWidget#%1 QTabWidget { background-color: %2; }"
               "QWidget#%1 QTabWidget::pane { background-color: %2; border: 1px solid %3; }"
               "QWidget#%1 QTabBar::tab { background: %4; color: %5; padding: 4px 12px; }"
               "QWidget#%1 QTabBar::tab:selected { background: %2; color: #FFFFFF; }"
               "QWidget#%1 QLabel, QWidget#%1 QGroupBox, QWidget#%1 QCheckBox { color: %5; }")
            .arg(qstrPageId, qstrBase, qstrBorder, qstrTab, qstrText);
}


// ============================ 配置文件(DefaultConfig.ini) ============================
// 只用"按行定位 [TCP] 段的 ip=" 的方式读写, 不用 QSettings:
// QSettings 保存时会把整个文件重写, 中文注释、分组顺序、空行全部丢失,
// 而这个 ini 同时还存着运动参数, 不能被顺手破坏。

QString ConfigFilePath(void)
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/DefaultConfig.ini");
}


// 读取 [TCP] 段的 ip; 没有则返回空串(调用方保留界面默认值)
QString ReadIniIp(void)
{
    QFile file(ConfigFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");

    bool bInTcp = false;
    while (!in.atEnd())
    {
        const QString qstrLine = in.readLine().trimmed();

        if (qstrLine.startsWith('[') && qstrLine.endsWith(']'))
        {
            bInTcp = (qstrLine.compare(QStringLiteral("[TCP]"), Qt::CaseInsensitive) == 0);
            continue;
        }

        if (bInTcp && qstrLine.startsWith(QStringLiteral("ip"), Qt::CaseInsensitive) &&
            qstrLine.contains('='))
        {
            return qstrLine.section('=', 1).trimmed();
        }
    }

    return QString();
}


// 只改 [TCP] 段的 ip 行, 其余内容原样保留; 用 QSaveFile 保证写失败不破坏原文件
bool WriteIniIp(const QString &qstrIp)
{
    const QString qstrPath = ConfigFilePath();
    QStringList qslLines;

    QFile file(qstrPath);
    if (file.exists())
    {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return false;
        }

        QTextStream in(&file);
        in.setCodec("UTF-8");
        QString qstrAll = in.readAll();
        file.close();

        // 统一成 '\n' 再拆行, 写回时不会出现混合换行
        qstrAll.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        qslLines = qstrAll.split('\n');
    }

    bool bInTcp = false;
    bool bDone = false;
    int iTcpHeader = -1;

    for (int i = 0; i < qslLines.size(); ++i)
    {
        const QString qstrTrim = qslLines.at(i).trimmed();

        if (qstrTrim.startsWith('[') && qstrTrim.endsWith(']'))
        {
            bInTcp = (qstrTrim.compare(QStringLiteral("[TCP]"), Qt::CaseInsensitive) == 0);
            if (bInTcp && (iTcpHeader < 0))
            {
                iTcpHeader = i;
            }
            continue;
        }

        if (bInTcp && qstrTrim.startsWith(QStringLiteral("ip"), Qt::CaseInsensitive) &&
            qstrTrim.contains('='))
        {
            qslLines[i] = QStringLiteral("ip=") + qstrIp;
            bDone = true;
            break;
        }
    }

    // 没有 ip 行: 有 [TCP] 段就插在段首, 连段都没有就追加一段
    if (!bDone)
    {
        if (iTcpHeader >= 0)
        {
            qslLines.insert(iTcpHeader + 1, QStringLiteral("ip=") + qstrIp);
        }
        else
        {
            if (!qslLines.isEmpty() && !qslLines.last().trimmed().isEmpty())
            {
                qslLines.append(QString());
            }
            qslLines.append(QStringLiteral("[TCP]"));
            qslLines.append(QStringLiteral("ip=") + qstrIp);
        }
    }

    QSaveFile out(qstrPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    QTextStream os(&out);
    os.setCodec("UTF-8");
    os << qslLines.join('\n');

    return out.commit();
}


// 解析 "a.b.c.d" (允许带 :port 后缀); 失败返回 false
bool ParseIpText(const QString &qstrText, QByteArray &qbtIp)
{
    QString qstrIp = qstrText.trimmed();

    const int iColon = qstrIp.indexOf(':');
    if (iColon >= 0)
    {
        qstrIp = qstrIp.left(iColon);
    }

    const QStringList qslParts = qstrIp.split('.');
    if (qslParts.size() != 4)
    {
        return false;
    }

    QByteArray qbtTemp;
    for (int i = 0; i < 4; ++i)
    {
        bool bOk = false;
        const int iValue = qslParts.at(i).toInt(&bOk);
        if (!bOk || (iValue < 0) || (iValue > 255))
        {
            return false;
        }
        qbtTemp.append(static_cast<char>(static_cast<uint8_t>(iValue)));
    }

    // 0.0.0.0 / 广播地址对设备没有意义, 挡在界面层比让下位机存进去更好
    const uint8_t u8First = static_cast<uint8_t>(qbtTemp.at(0));
    const uint8_t u8Last = static_cast<uint8_t>(qbtTemp.at(3));
    if ((u8First == 0) || (u8First == 255) || u8Last == 0 || u8Last == 255)
    {
        return false;
    }

    qbtIp = qbtTemp;

    return true;
}


// ============================ UDP 发现(未知 IP 时获取) ============================
// TCP 必须先知道对方 IP 才能建连, 所以完全不知道 IP 时经 UDP 广播发现:
// 广播 0x11 到 255.255.255.255:5000, 下位机按广播源地址单播回 0x12 + IP。
// 帧格式与校验规则和 TCP 通道完全一致。

// 下位机 UDP 发现服务监听的端口(与下位机 Network.c 的 NETWORK_DISCOVERY_PORT 一致)
constexpr quint16 u16UDP_DISCOVERY_PORT = 5000;
// 广播发现超时 ms
constexpr int iUDP_DISCOVERY_TIMEOUT_MS = 2000;

// 组发现请求帧: 55 AA 11 01 01 chkH chkL AA 55
QByteArray BuildUdpGetIpCmd(void)
{
    QByteArray qbtCmd;
    qbtCmd.append(static_cast<char>(0x55));
    qbtCmd.append(static_cast<char>(0xAA));
    qbtCmd.append(static_cast<char>(0x11));
    qbtCmd.append(static_cast<char>(0x01));
    qbtCmd.append(static_cast<char>(0x01));

    quint16 u16Sum = 0;
    for (int i = 2; i < 5; ++i)
    {
        u16Sum += static_cast<quint8>(qbtCmd.at(i));
    }
    u16Sum = static_cast<quint16>(~u16Sum + 1);
    qbtCmd.append(static_cast<char>(u16Sum >> 8));
    qbtCmd.append(static_cast<char>(u16Sum & 0xFF));

    qbtCmd.append(static_cast<char>(0xAA));
    qbtCmd.append(static_cast<char>(0x55));

    return qbtCmd;
}

// 校验发现应答帧并提取 IP: 55 AA 12 04 IP1~IP4 chkH chkL AA 55(共 12 字节)
bool ValidateUdpIpRespond(const QByteArray &qbtFrame, QString &qstrIp)
{
    if (qbtFrame.size() != 12)
    {
        return false;
    }
    if ((static_cast<quint8>(qbtFrame.at(0)) != 0x55) ||
        (static_cast<quint8>(qbtFrame.at(1)) != 0xAA) ||
        (static_cast<quint8>(qbtFrame.at(2)) != 0x12) ||
        (static_cast<quint8>(qbtFrame.at(3)) != 0x04) ||
        (static_cast<quint8>(qbtFrame.at(10)) != 0xAA) ||
        (static_cast<quint8>(qbtFrame.at(11)) != 0x55))
    {
        return false;
    }

    quint16 u16Sum = 0;
    for (int i = 2; i < 8; ++i)
    {
        u16Sum += static_cast<quint8>(qbtFrame.at(i));
    }
    u16Sum = static_cast<quint16>(~u16Sum + 1);
    const quint16 u16Recv = static_cast<quint16>(
                (static_cast<quint8>(qbtFrame.at(8)) << 8) |
                 static_cast<quint8>(qbtFrame.at(9)));
    if (u16Sum != u16Recv)
    {
        return false;
    }

    qstrIp = QStringLiteral("%1.%2.%3.%4")
            .arg(static_cast<quint8>(qbtFrame.at(4)))
            .arg(static_cast<quint8>(qbtFrame.at(5)))
            .arg(static_cast<quint8>(qbtFrame.at(6)))
            .arg(static_cast<quint8>(qbtFrame.at(7)));

    return true;
}
}

ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlWidget)
{
    ui->setupUi(this);

    m_eConnectObject = ConnectObject::ObjectNone;

    // 通信对象与工作线程指针必须显式初始化, 否则连接失败分支会使用野指针
    m_ConnectiveHandler = nullptr;
    m_pConnectiveThread = nullptr;

    // 保存IP / 获取IP 事务标记
    m_bSaveIpPending = false;
    m_bGetIpPending = false;

    // UDP 发现通道(未连接时获取 IP 用), 生命周期跟随本控件
    m_pGetIpUdpSocket = new QUdpSocket(this);
    m_pGetIpUdpTimer = new QTimer(this);
    m_pGetIpUdpTimer->setSingleShot(true);
    connect(m_pGetIpUdpSocket, &QUdpSocket::readyRead,
            this, &ControlWidget::OnUdpGetIpReadyRead);
    connect(m_pGetIpUdpTimer, &QTimer::timeout,
            this, &ControlWidget::OnUdpGetIpTimeout);

    // 控制面板实例化, 一个用于配置, 一个用于控制
    m_pControlWidget = new ControlPanel(ui->ControlTabWidget);
    m_pModeWidget = new ControlPanel(ui->ModeTabWidget);
    m_pModeWidget->SetReadOnly(true);

    // 控制板数据页: 只用"很淡的蓝"标记按钮可点(与模式数据页的只读深灰底区分)。
    // 规则用 :enabled 限定 —— 整页禁用(未连接 / 断开)时按钮不匹配, 仍是系统禁用灰,
    // 这样"能不能按"一眼就能看出来, 不必靠颜色猜。
    ui->tabControl->setStyleSheet(QStringLiteral(
        "QWidget#tabControl QPushButton:enabled {"
        "  background-color: #E7F1FC;"
        "  border: 1px solid #BCD6EF;"
        "  border-radius: 3px;"
        "  padding: 3px 8px;"
        "}"
        "QWidget#tabControl QPushButton:enabled:checked {"
        "  background-color: #C6DEF7;"
        "  border-color: #96BFE6;"
        "}"
        "QWidget#tabControl QPushButton:enabled:hover { background-color: #D9E9FA; }"
        "QWidget#tabControl QPushButton:enabled:pressed { background-color: #C0DAF4; }"));

    // 模式数据页: 整页深灰底(上一轮需求, 保留)
    ui->tabMode->setStyleSheet(MakePanelStyle("tabMode", "#33383D", "#4C5155",
                                              "#3E4347", "#E6E8EA"));

    ui->tabControl->setEnabled(false);

    ui->pbOpenMode->setEnabled(false);
    ui->pbSaveMode->setEnabled(false);
    ui->pbSaveModeAs->setEnabled(false);
    ui->pbNewMode->setEnabled(false);
    ui->pbMode2Control->setEnabled(false);


    // 允许 ip:port 格式(冒号用于手动指定端口, 缺省 5000)
    QRegularExpression Regex("^[0-9.:]+$");
    QRegularExpressionValidator *Validator = new QRegularExpressionValidator(Regex, this);
    ui->leIP->setValidator(Validator);

    // IP 栏用 ini 里保存过的地址预填(上一次"保存IP"的结果), 没有则保留 .ui 默认值
    const QString qstrSavedIp = ReadIniIp();
    if (!qstrSavedIp.isEmpty())
    {
        ui->leIP->setText(qstrSavedIp);
    }
}

ControlWidget::~ControlWidget()
{
    // 退出前停止通信线程并销毁通信对象, 避免退出时访问已释放的界面资源
    CloseConnection();

    delete ui;
}


// 退出确认: 点窗口 X / Alt+F4 / 任务栏关闭 都会走到这里
// 之所以要拦一道:
//  1) 连接中退出会静默断开下位机, 用户常常以为是"软件卡了"而不是自己关了;
//  2) 动作执行中退出, 下位机是"收一条指令跑一条"的, 上位机断了也照样跑完当前指令,
//     没有提示的话现场会看到"软件关了机械还在动";
//  3) 保存IP 正在等应答时退出, 无法确认下位机是否已把参数写进 Flash。
// 确认按钮语义: 默认焦点落在"取消"上, 误按回车不会直接退出。
void ControlWidget::closeEvent(QCloseEvent *event)
{
    const bool bConnected = (m_eConnectObject != ConnectObject::ObjectNone) &&
                            ((m_ConnectiveHandler != nullptr) || (m_pConnectiveThread != nullptr));
    const bool bMotionRunning = (m_pControlWidget != nullptr) && m_pControlWidget->IsMotionRunning();

    QMessageBox box(this);
    box.setIcon(bConnected ? QMessageBox::Warning : QMessageBox::Question);
    box.setWindowTitle("退出确认");
    box.setText(bConnected ? "当前仍与下位机保持连接，退出软件将断开通信。"
                           : "确定要退出 FlexControlSystem 吗？");

    QStringList qslInfo;
    if (bMotionRunning)
    {
        qslInfo << "● 有动作指令正在执行，退出后下位机仍会按原指令运行到时长结束。";
    }
    if (m_bSaveIpPending)
    {
        qslInfo << "● 正在等待“保存IP”的应答，退出后无法确认下位机是否已保存参数。";
    }
    if (!qslInfo.isEmpty())
    {
        box.setInformativeText(qslInfo.join("\n"));
    }

    QPushButton *pBtnQuit = box.addButton("退出", QMessageBox::AcceptRole);
    QPushButton *pBtnCancel = box.addButton("取消", QMessageBox::RejectRole);
    box.setDefaultButton(pBtnCancel);
    box.setEscapeButton(pBtnCancel);
    box.exec();

    if (box.clickedButton() != pBtnQuit)
    {
        // 取消: 窗口保持打开, 连接与动作都不受影响
        event->ignore();
        return;
    }

    // 确认退出: 先按正常流程停线程/关连接, 再放行关闭事件
    CloseConnection();
    event->accept();
}


void ControlWidget::on_pbUpdatePort_clicked()
{
    ui->cbCOM->clear();
    foreach (const QSerialPortInfo &Port, QSerialPortInfo::availablePorts())
    {
        ui->cbCOM->addItem(Port.portName());
    }
}

void ControlWidget::on_leIP_textChanged(const QString &arg1)
{

}



void ControlWidget::OnConnectiveState(const bool &bState)
{
    if (bState)
    {
        // 如果连接成功, 按钮使能状态翻转
        if (m_eConnectObject == ConnectObject::Object485)
        {
            ui->pbRS485Connect->setEnabled(false);
            ui->pbRS485Disconnect->setEnabled(true);
            ui->pbUpdatePort->setEnabled(false);
        }
        else if (m_eConnectObject == ConnectObject::ObjectTCP)
        {
            ui->pbTCPConnect->setEnabled(false);
            ui->pbTCPDisconnect->setEnabled(true);
        }

        ui->tabControl->setEnabled(true);
    }
    else
    {
        // 连接不成功, 弹出提示并清理对象与线程
        QMessageBox::critical(this, "错误", "目标连接失败");
        CloseConnection();

        if (m_eConnectObject == ConnectObject::Object485)
        {
            ui->pbRS485Connect->setEnabled(true);
            ui->pbUpdatePort->setEnabled(true);
        }
        else if (m_eConnectObject == ConnectObject::ObjectTCP)
        {
            ui->pbTCPConnect->setEnabled(true);
        }

        m_eConnectObject = ConnectObject::ObjectNone;
    }
}


// 关闭并销毁通信对象与工作线程
// 线程先 quit/wait 停止, 再直接 delete 通信对象(此时其所属线程已停, 无并发访问)
// 不用 deleteLater: 若线程事件循环先退出, deleteLater 事件将永远不会被处理, 造成泄漏
//
// 统一在此禁用 ui->tabControl: 心跳丢失 / 主动断开 TCP/485 / 连接失败 / 析构 全部走这里,
// 断连后控制面板不能再操作(按钮即使被 sigMotionTerminate 恢复为 enabled, 因父容器禁用也不可点;
// 下次连接成功时 OnConnectiveState(true) 会重新 setEnabled(true))。
void ControlWidget::CloseConnection(void)
{
    ui->tabControl->setEnabled(false);

    // 保存IP 期间断连(心跳丢失/用户断开): 事务作废并恢复按钮,
    // 否则按钮会一直停在禁用态, 后续再也点不了
    if (m_bSaveIpPending)
    {
        m_bSaveIpPending = false;
        ui->pbSaveIP->setEnabled(true);
    }

    // 获取IP 期间断连: 事务作废(迟到的应答/超时信号会被 pending 标志挡掉);
    // UDP 发现通道也一并停掉
    m_bGetIpPending = false;
    if (m_pGetIpUdpTimer != nullptr)
    {
        m_pGetIpUdpTimer->stop();
    }
    if (m_pGetIpUdpSocket != nullptr)
    {
        m_pGetIpUdpSocket->close();
    }

    if (m_ConnectiveHandler)
    {
        // 断开全部信号连接, 避免清理过程中的信号投递
        m_ConnectiveHandler->disconnect();
    }

    if (m_pConnectiveThread)
    {
        // finished 信号由工作线程自己发出, 与 handler 是同线程直连,
        // Shutdown 在工作线程内执行: 定时器/串口都在正确线程被关闭,
        // 避免 "Timers cannot be stopped from another thread" 警告
        if (m_ConnectiveHandler)
        {
            connect(m_pConnectiveThread, &QThread::finished,
                    m_ConnectiveHandler, &ConnectiveHandle::Shutdown);
        }

        m_pConnectiveThread->quit();
        m_pConnectiveThread->wait();
    }

    if (m_ConnectiveHandler)
    {
        delete m_ConnectiveHandler;
        m_ConnectiveHandler = nullptr;
    }

    if (m_pConnectiveThread)
    {
        delete m_pConnectiveThread;
        m_pConnectiveThread = nullptr;
    }
}


void ControlWidget::SetConnectSlot(const bool bSet)
{
    if (bSet)
    {
        connect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));
        connect(m_pControlWidget, SIGNAL(sigMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendMotionCommand(QByteArray, QByteArray)));
        connect(m_pControlWidget, SIGNAL(sigStopMotion()), m_ConnectiveHandler, SLOT(SendStopMotionCommand()));
        // 料仓
        connect(m_pControlWidget, SIGNAL(sigBinMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendBinMotionCommand(QByteArray, QByteArray)));
        connect(m_pControlWidget, SIGNAL(sigBinStopMotion()), m_ConnectiveHandler, SLOT(SendBinStopMotionCommand()));
        // 光源
        connect(m_pControlWidget, SIGNAL(sigLightOn(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendLightOnCommand(QByteArray, QByteArray)));
        connect(m_pControlWidget, SIGNAL(sigLightOff(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendLightOffCommand(QByteArray, QByteArray)));

        // 保存IP 的结果(成功/失败/超时)由工作线程回传
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigSaveIpResult,
                this, &ControlWidget::OnSaveIpResult);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigGetIpResult,
                this, &ControlWidget::OnGetIpResult);

        connect(m_ConnectiveHandler, &ConnectiveHandle::sigHeartBeatUnnormal, this, [this]() {
            OnConnectiveUnnormal("HeartBeat");
        });
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigMotionRespondUnnormal, this, [this]() {
            OnConnectiveUnnormal("MotionRespond");
        });
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigBinMotionRespondUnnormal, this, [this]() {
            OnConnectiveUnnormal("BinMotionRespond");
        });
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigLightRespondUnnormal, this, [this]() {
            OnConnectiveUnnormal("LightRespond");
        });

        connect(m_ConnectiveHandler, &ConnectiveHandle::sigMotionTerminate, this, [this]() {
            emit m_pControlWidget->sigMotionTerminate();
        });

        qDebug() << "set slot after connected";
    }
    else
    {
        disconnect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));
        disconnect(m_pControlWidget, SIGNAL(sigMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendMotionCommand(QByteArray, QByteArray)));
    }
}


void ControlWidget::OnConnectiveUnnormal(const QString &qstrName)
{
    if (qstrName == "HeartBeat")
    {
        QMessageBox::critical(this, "错误", "心跳检测丢失");
        CloseConnection();

        if (m_eConnectObject == ConnectObject::Object485)
        {
            ui->pbRS485Connect->setEnabled(true);
            ui->pbRS485Disconnect->setEnabled(false);
            ui->pbUpdatePort->setEnabled(true);
        }
        else if (m_eConnectObject == ConnectObject::ObjectTCP)
        {
            ui->pbTCPConnect->setEnabled(true);
            ui->pbTCPDisconnect->setEnabled(false);
        }

        m_eConnectObject = ConnectObject::ObjectNone;
    }
    else if (qstrName == "MotionRespond")
    {
        QMessageBox::critical(this, "错误", "动作反馈超时");
    }
    else if (qstrName == "BinMotionRespond")
    {
        QMessageBox::critical(this, "错误", "料仓反馈超时");
    }
    else if (qstrName == "LightRespond")
    {
        QMessageBox::critical(this, "错误", "光源反馈超时");
    }

    emit m_pControlWidget->sigMotionTerminate();
}



void ControlWidget::on_pbTCPConnect_clicked()
{
    // 已有连接时忽略重复点击
    if (m_ConnectiveHandler || m_pConnectiveThread)
    {
        return;
    }

    // 点击了连接，创建 TCP 通信对象
    m_ConnectiveHandler = new TCPHandle();
    m_pConnectiveThread = new QThread();
    m_ConnectiveHandler->moveToThread(m_pConnectiveThread);

    SetConnectSlot(true);

    m_eConnectObject = ConnectObject::ObjectTCP;

    m_pConnectiveThread->start();

    // 连接目标: "ip:port", 未填端口时由 TCPHandle 默认 5000
    QString qstrTarget = ui->leIP->text().trimmed();
    if (!qstrTarget.contains(':'))
    {
        qstrTarget += ":5000";
    }

    ui->pbTCPConnect->setEnabled(false);

    emit m_ConnectiveHandler->sigSetUpConnective(qstrTarget);
}




void ControlWidget::on_pbTCPDisconnect_clicked()
{
    if (m_ConnectiveHandler && (m_eConnectObject == ConnectObject::ObjectTCP))
    {
        CloseConnection();

        ui->pbTCPConnect->setEnabled(true);
        ui->pbTCPDisconnect->setEnabled(false);

        m_eConnectObject = ConnectObject::ObjectNone;
    }
}



void ControlWidget::on_pbRS485Connect_clicked()
{
    // 已有连接时忽略重复点击
    if (m_ConnectiveHandler || m_pConnectiveThread)
    {
        return;
    }

    // 点击了连接，创建485通信对象
    m_ConnectiveHandler = new RS485Handle();
    m_pConnectiveThread = new QThread();
    m_ConnectiveHandler->moveToThread(m_pConnectiveThread);

    SetConnectSlot(true);    

    m_eConnectObject = ConnectObject::Object485;

    m_pConnectiveThread->start();

    // 打开串口通信
    emit m_ConnectiveHandler->sigSetUpConnective(ui->cbCOM->currentText());
}


void ControlWidget::on_pbRS485Disconnect_clicked()
{
    // 断开485连接
    if (m_ConnectiveHandler && (m_eConnectObject == ConnectObject::Object485))
    {
        CloseConnection();

        ui->pbRS485Connect->setEnabled(true);
        ui->pbRS485Disconnect->setEnabled(false);
        ui->pbUpdatePort->setEnabled(true);

        m_eConnectObject = ConnectObject::ObjectNone;
    }
}

void ControlWidget::on_pbOpenMode_clicked()
{
    QString qstrFileName = QFileDialog::getOpenFileName(
        this,
        "打开文件",
        QDir::homePath(),                       // 默认目录
        "配置文件 (*.ini)"
    );


    // 载入路径到模式数据中
    m_pModeWidget->LoadSettings(qstrFileName);
}

void ControlWidget::on_pbSaveMode_clicked()
{
    auto ret = QMessageBox::question(
        this,
        "确认",
        "确定要保存参数吗？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret == QMessageBox::Yes)
    {
        m_pModeWidget->SaveSettings();
    }
    else
    {

    }
}

void ControlWidget::on_pbSaveModeAs_clicked()
{
    QString qstrFileName = QFileDialog::getSaveFileName(
        this,
        "另存为",
        QDir::homePath(),                       // 默认目录
        "配置文件 (*.ini)"
    );

    m_pModeWidget->SaveSettings(qstrFileName);
}

void ControlWidget::on_pbNewMode_clicked()
{
    QString qstrFileName = QFileDialog::getSaveFileName(
        this,
        "新建",
        QDir::homePath(),                       // 默认目录
        "配置文件 (*.ini)"
    );

    m_pModeWidget->NewSettings(qstrFileName);
}

void ControlWidget::on_tabPanel_currentChanged(int index)
{
    qDebug() << index;

    if (0 == index)
    {
        ui->pbOpenMode->setEnabled(false);
        ui->pbSaveMode->setEnabled(false);
        ui->pbSaveModeAs->setEnabled(false);
        ui->pbNewMode->setEnabled(false);
        ui->pbMode2Control->setEnabled(false);
        ui->pbControl2Mode->setEnabled(true);
    }
    else
    {
        ui->pbOpenMode->setEnabled(true);
        ui->pbSaveMode->setEnabled(true);
        ui->pbSaveModeAs->setEnabled(true);
        ui->pbNewMode->setEnabled(true);
        ui->pbMode2Control->setEnabled(true);
        ui->pbControl2Mode->setEnabled(false);
    }
}

void ControlWidget::on_pbControl2Mode_clicked()
{
    auto ret = QMessageBox::question(
        this,
        "确认",
        "确定要将控制参数覆盖到模式数据吗？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret == QMessageBox::Yes)
    {
        m_pModeWidget->CopyButtonParams(m_pControlWidget);
    }
    else
    {

    }
}

void ControlWidget::on_pbMode2Control_clicked()
{
    auto ret = QMessageBox::question(
        this,
        "确认",
        "确定要将模式数据覆盖到控制参数吗？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret == QMessageBox::Yes)
    {
        m_pControlWidget->CopyButtonParams(m_pModeWidget);
    }
    else
    {

    }
}

// 保存IP: 确认 -> 下发 0x0F -> 等 0x10 结果 -> 写 ini + 断开 + 提示重启
// 顺序说明: 必须"先拿到下位机应答, 再写 ini 并断开"。若先断开, 应答收不到,
// 也就无法区分"保存成功"和"写 Flash 失败"这两种结果。
void ControlWidget::on_pbSaveIP_clicked()
{
    QByteArray qbtIp;

    if (m_bSaveIpPending)
    {
        return;
    }

    if (!ParseIpText(ui->leIP->text(), qbtIp))
    {
        QMessageBox::warning(this, "提示",
                             "IP 格式不正确，请按 192.168.2.199 的格式填写。\n"
                             "（四段 0~255，不能是 0.0.0.0 或广播地址）");
        return;
    }

    // 保存指令必须经通信通道下发, 未连接时直接提示
    if ((m_ConnectiveHandler == nullptr) || (m_pConnectiveThread == nullptr) ||
        (m_eConnectObject == ConnectObject::ObjectNone))
    {
        QMessageBox::warning(this, "提示", "请先连接下位机，再保存 IP。");
        return;
    }

    const QString qstrIp = QStringLiteral("%1.%2.%3.%4")
            .arg(static_cast<int>(static_cast<uint8_t>(qbtIp.at(0))))
            .arg(static_cast<int>(static_cast<uint8_t>(qbtIp.at(1))))
            .arg(static_cast<int>(static_cast<uint8_t>(qbtIp.at(2))))
            .arg(static_cast<int>(static_cast<uint8_t>(qbtIp.at(3))));

    const auto eRet = QMessageBox::question(
                this,
                "确认",
                QString("确定要把 IP 保存到下位机吗？\n\nIP：%1\n\n"
                        "下位机会写入 Flash 并自动重启，重启期间连接会断开，"
                        "之后需要用新 IP 重新连接。").arg(qstrIp),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

    if (eRet != QMessageBox::Yes)
    {
        return;
    }

    QByteArray qbtCmd;
    GeneralProtocol::getInstance().SaveIpProtocol(qbtIp, qbtCmd);

    m_bSaveIpPending = true;
    m_qstrSaveIpPending = qstrIp;
    ui->pbSaveIP->setEnabled(false);

    // 投递到通信线程执行(该对象属于工作线程, 不能在这里直接改它的队列)
    QMetaObject::invokeMethod(m_ConnectiveHandler, "SendSaveIpCommand",
                              Qt::QueuedConnection, Q_ARG(QByteArray, qbtCmd));
}


// 下位机对保存IP 的应答: 1=写入成功, 0=写入失败, -1=3s 内无应答
void ControlWidget::OnSaveIpResult(const int &iResult)
{
    // 非本次事务的迟到应答直接丢弃
    if (!m_bSaveIpPending)
    {
        return;
    }

    m_bSaveIpPending = false;
    ui->pbSaveIP->setEnabled(true);

    if (iResult == 1)
    {
        const QString qstrIp = m_qstrSaveIpPending;

        // 下位机已经写好了, 本机 ini 写失败只影响下次启动的预填, 如实提示即可
        if (!WriteIniIp(qstrIp))
        {
            QMessageBox::warning(this, "提示",
                                 "下位机保存成功，但本机配置文件写入失败"
                                 "（程序目录可能没有写权限），下次启动需重新填写 IP。");
        }

        // 更新 IP 栏, 下次连接直接指向新地址
        ui->leIP->setText(qstrIp);

        /*
         * 主动断开: 下位机 0.5s 后复位重启, 若不断开, 这段时间会先触发
         * "心跳丢失"再弹错误框, 与本操作的结果提示叠在一起, 干扰判断。
         */
        CloseConnection();
        ui->pbTCPConnect->setEnabled(true);
        ui->pbTCPDisconnect->setEnabled(false);
        ui->pbRS485Connect->setEnabled(true);
        ui->pbRS485Disconnect->setEnabled(false);
        ui->pbUpdatePort->setEnabled(true);
        m_eConnectObject = ConnectObject::ObjectNone;

        QMessageBox::information(this, "保存成功",
                                 QString("IP 已保存到下位机：%1\n\n"
                                         "下位机正在重启（约 5 秒），"
                                         "重启完成后请用新 IP 重新点击“连接”。")
                                 .arg(qstrIp));
        return;
    }

    if (iResult == 0)
    {
        QMessageBox::critical(this, "保存失败",
                              "下位机写入 Flash 失败（回读校验未通过），IP 未更改。\n"
                              "请重试；若反复失败，检查该 Flash 页是否被写保护。");
        return;
    }

    QMessageBox::warning(this, "无应答",
                         "下位机 3 秒内没有应答保存指令，无法确认是否保存成功。\n"
                         "请检查连接状态；若下位机已按新 IP 重启，"
                         "改用新 IP 重新连接即可。");
}

void ControlWidget::on_pbGetIP_clicked()
{
    if (m_bGetIpPending)
    {
        return;
    }

    const bool bConnected = (m_ConnectiveHandler != nullptr) &&
            (m_pConnectiveThread != nullptr) &&
            (m_eConnectObject != ConnectObject::ObjectNone);

    m_bGetIpPending = true;

    if (bConnected)
    {
        // TCP 路径: 已连接, 投递到通信线程执行(该对象属于工作线程, 不能在这里直接改它的队列)
        QMetaObject::invokeMethod(m_ConnectiveHandler, "SendGetIpCommand",
                                  Qt::QueuedConnection);
        return;
    }

    // UDP 路径: 未连接(通常意味着不知道下位机 IP), 广播发现。
    // W5500 只接收"子网定向广播"(如 192.168.2.255), 收不到发往
    // 255.255.255.255 的有限广播, 所以按本机各网卡的网段算出定向广播
    // 地址逐个发送, 最后再补一个有限广播兜底(发给别的类型设备也无害)
    m_pGetIpUdpSocket->close();
    if (!m_pGetIpUdpSocket->bind(QHostAddress(QHostAddress::AnyIPv4),
                                 static_cast<quint16>(0)))
    {
        m_bGetIpPending = false;
        QMessageBox::warning(this, "提示",
                             "本机 UDP 端口绑定失败，无法发起发现。\n"
                             "请检查防火墙设置后重试。");
        return;
    }

    QList<QHostAddress> qlTargets;
    const QList<QNetworkInterface> qlIfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &sIface : qlIfaces)
    {
        const uint u32Flags = static_cast<uint>(sIface.flags());
        if ((u32Flags & static_cast<uint>(QNetworkInterface::CanBroadcast)) == 0u ||
            (u32Flags & static_cast<uint>(QNetworkInterface::IsLoopBack)) != 0u ||
            (u32Flags & static_cast<uint>(QNetworkInterface::IsPointToPoint)) != 0u ||
            (u32Flags & static_cast<uint>(QNetworkInterface::IsUp)) == 0u)
        {
            continue;
        }

        const QList<QNetworkAddressEntry> qlEntries = sIface.addressEntries();
        for (const QNetworkAddressEntry &sEntry : qlEntries)
        {
            if (sEntry.ip().protocol() != QAbstractSocket::IPv4Protocol)
            {
                continue;
            }

            const quint32 u32Ip = sEntry.ip().toIPv4Address();
            const quint32 u32Mask = sEntry.netmask().toIPv4Address();
            if ((u32Mask == 0u) || (u32Mask == 0xFFFFFFFFu))
            {
                continue;
            }

            // 定向广播 = 本机 IP | (~子网掩码)
            const QHostAddress qBroadcast(u32Ip | (~u32Mask));
            if (!qlTargets.contains(qBroadcast))
            {
                qlTargets.append(qBroadcast);
                qDebug() << "discovery target:" << qBroadcast.toString()
                         << "via" << sIface.name();
            }
        }
    }

    const QHostAddress qLimited(QStringLiteral("255.255.255.255"));
    if (!qlTargets.contains(qLimited))
    {
        qlTargets.append(qLimited);
    }

    const QByteArray qbtCmd = BuildUdpGetIpCmd();
    qint64 i64Sent = -1;
    for (const QHostAddress &qAddr : qlTargets)
    {
        const qint64 i64Ret = m_pGetIpUdpSocket->writeDatagram(
                    qbtCmd, qAddr, u16UDP_DISCOVERY_PORT);
        if (i64Ret >= 0)
        {
            i64Sent = i64Ret;
            qDebug() << "discovery sent to" << qAddr.toString();
        }
        else
        {
            qDebug() << "discovery send failed to" << qAddr.toString()
                     << m_pGetIpUdpSocket->errorString();
        }
    }

    if (i64Sent < 0)
    {
        m_pGetIpUdpSocket->close();
        m_bGetIpPending = false;
        QMessageBox::warning(this, "提示",
                             "广播发送失败，无法发起发现。\n"
                             "请检查网卡与防火墙设置后重试。");
        return;
    }

    m_pGetIpUdpTimer->start(iUDP_DISCOVERY_TIMEOUT_MS);
}


// 下位机对获取IP 的应答(TCP 路径): IP 字符串=成功, 空串=2s 内无应答
void ControlWidget::OnGetIpResult(const QString &qstrIp)
{
    // 非本次事务的迟到应答直接丢弃
    if (!m_bGetIpPending)
    {
        return;
    }

    m_bGetIpPending = false;

    if (qstrIp.isEmpty())
    {
        QMessageBox::warning(this, "无应答",
                             "下位机 2 秒内没有应答获取指令，无法获取 IP。\n"
                             "请检查连接状态后重试。");
        return;
    }

    ui->leReadIP->setText(qstrIp);
    QMessageBox::information(this, "获取成功",
                             QString("下位机当前 IP：%1").arg(qstrIp));
}


// UDP 发现: 读到下位机的单播应答, 校验通过即填 IP 栏并提示
void ControlWidget::OnUdpGetIpReadyRead()
{
    while (m_pGetIpUdpSocket->hasPendingDatagrams())
    {
        QByteArray qbtDatagram;
        qbtDatagram.resize(static_cast<int>(m_pGetIpUdpSocket->pendingDatagramSize()));
        const QHostAddress qFrom(m_pGetIpUdpSocket->readDatagram(
                                     qbtDatagram.data(), qbtDatagram.size()));
        qDebug() << "discovery reply from" << qFrom.toString()
                 << "len" << qbtDatagram.size() << qbtDatagram.toHex();

        QString qstrIp;
        if (!ValidateUdpIpRespond(qbtDatagram, qstrIp))
        {
            continue;   // 别的设备的广播/杂散包, 继续等
        }

        m_pGetIpUdpTimer->stop();
        m_pGetIpUdpSocket->close();
        m_bGetIpPending = false;

        ui->leReadIP->setText(qstrIp);
        QMessageBox::information(this, "获取成功",
                                 QString("通过 UDP 发现到下位机，当前 IP：%1\n\n"
                                         "可以直接把这个 IP 填到上方 IP 栏后点击“连接”。")
                                 .arg(qstrIp));
        return;
    }
}


// UDP 发现超时: 网段里没有应答的下位机
void ControlWidget::OnUdpGetIpTimeout()
{
    m_pGetIpUdpSocket->close();
    m_bGetIpPending = false;

    QMessageBox::warning(this, "未发现下位机",
                         "广播 2 秒内没有收到应答。\n"
                         "请确认：\n"
                         "1. 下位机已烧录包含 UDP 发现功能的固件并已上电；\n"
                         "2. 上位机与下位机在同一网段（跨网段的广播到不了）；\n"
                         "3. 防火墙已放行本程序（首次使用会弹授权框，\n"
                         "   被静默拦截时现象就是收不到应答）。");
}
