#include "controlwidget.h"
#include "ui_controlwidget.h"
#include "titlebar.h"
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
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QTimer>
#include <QPainter>
#include <QResizeEvent>
#include <QSizeGrip>
#include "generalprotocol.h"


namespace
{
// 2026-11-24 主题改造: 原来这里有个 MakePanelStyle(), 用来把整个"模式数据"页
// 反色成深灰底(#33383D)表示只读。换成「B 浅色卡片」主题后, 深灰整页与浅色主界面
// 放在一起比改造前更割裂, 且"反色"本身不是现代做法。
// 现在只读语义靠两条表达: ①该页所有下发按钮本就是 disabled → 全局 QSS 的
// :disabled 规则(灰底灰字)已足够显眼; ②#tabMode 在 app.qss 里给了略深一层的
// 底色(#FAFBFC), 视觉上"退到后面"。样式全部收到 style/app.qss, 本文件不再拼样式表。


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


// ==================== 右下角"状态"卡片: 三盏状态点 + 两个版本号 ====================
// 名字与两种状态的说明文字, 顺序必须与 ControlWidget::StatusLampIndex 一致
// (连接 / 振动盘运动 / 料仓运动)。BuildStatusLamps 建灯、SetStatusLamp 换 tooltip
// 都读这一份, 免得"标签写一处、提示写另一处"改漏。中文经 QString::fromUtf8 转换。
const char *const kpstrStatusNames[3] = {"连接", "振动盘运动", "料仓运动"};
const char *const kpstrStatusTipOn[3] = {"连接正常（心跳正常）", "振动盘运动中", "料仓运动中"};
const char *const kpstrStatusTipOff[3] = {"未连接或心跳丢失", "振动盘空闲", "料仓空闲"};

// 软件版本号: 界面「状态」卡片里显示。发布时与安装包一起改, 目前这里是唯一来源
// (FlexControlSystem.pro 里没有 VERSION 字段, 别指望 qmake 自动带版本)。
const char *const kpstrAppVersion = "0.1.0";
}

ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlWidget)
{
    ui->setupUi(this);

    // ---- 自绘标题栏(金属拉丝铝) --------------------------------------------------
    // 原生标题栏高度/配色/按钮尺寸都改不了, 所以整窗去边框, 由 ui->titleBar 自绘
    // 一条 62px(系统栏 31px 的两倍) 的金属条。必须在这里而非 show() 之后设置:
    // setWindowFlags 会让窗口隐藏并重建原生窗口, 构造期调用才能无闪动。
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    ui->titleBar->SetTitle(windowTitle());

    // 去掉系统边框就没了右下角的缩放热区, 用 QSizeGrip 补回来(定位见 resizeEvent)
    m_pSizeGrip = new QSizeGrip(this);
    m_pSizeGrip->setObjectName(QStringLiteral("sizeGrip"));
    m_pSizeGrip->resize(16, 16);
    m_pSizeGrip->raise();

    // 最大化时窗口尺寸由屏幕决定, 不该再露出缩放手柄(QSizeGrip 只认真正的
    // Qt 最大化状态, 而这里是用 setGeometry 模拟的, 得自己跟着状态收放)
    connect(ui->titleBar, &TitleBar::MaximizedChanged, this, [this](const bool bMaximized) {
        if (m_pSizeGrip != nullptr)
        {
            m_pSizeGrip->setVisible(!bMaximized);
        }
    });

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

    // 电压周期查询: 2s 一次, 连接成功后启动; 初始显示占位
    m_pVoltageTimer = new QTimer(this);
    m_pVoltageTimer->setInterval(2000);
    connect(m_pVoltageTimer, &QTimer::timeout,
            this, &ControlWidget::OnVoltageQueryTick);
    ui->leVoltage->setText("--");

    // 右下角"状态"卡片: 在"电压(V)"下面补一行三盏运行状态点(连接/运动/料仓)
    BuildStatusLamps();

    // 控制面板实例化, 一个用于配置, 一个用于控制
    m_pControlWidget = new ControlPanel(ui->ControlTabWidget, true);
    m_pModeWidget = new ControlPanel(ui->ModeTabWidget);
    // 两个面板共享同一份"运动序列"状态(二十五次): 谁发的序列谁当 owner,
    // 只有 owner 处理完成帧(递减/续队), 收尾时两个面板一起回空闲。
    // 不共享的话, 切换窗口抄按钮态就会清掉一方的计数 —— 聚拢"保持"下计数
    // 永不归零, 会出现"跑着却已解锁, 还能再点发送"/"停了却一直锁着"。
    m_pControlWidget->UseSharedSequence(&m_motionSequence, m_pModeWidget);
    m_pModeWidget->UseSharedSequence(&m_motionSequence, m_pControlWidget);
    // 配置面板初始不可下发(还没连上), 连接成功后由 OnConnectiveState 放开 ——
    // 2026-11-25 二十次: 配置窗口里的运动/料仓/光源按钮不再永久禁用,
    // 用户要"配完就地发一条看效果", 不必退回运行页再点一遍
    m_pModeWidget->SetReadOnly(true);

    // 运行状态灯接控制面板: 面板只报"运动/料仓是否在执行", 点灯由这里做。
    // 注意要在这里(构造期)接线, 不能放进 SetConnectSlot —— 那个函数只在连接成功
    // 前后被调, 断连期间灯就收不到"回空闲"的通知, 会一直停在绿灯。
    // 两个面板都接: 在配置窗口里试跑时, 右下角的两盏灯也应该跟着亮/灭。
    connect(m_pControlWidget, &ControlPanel::sigMotionRunningChanged,
            this, [this](const bool bRunning) { SetStatusLamp(StatusLampMotion, bRunning); });
    connect(m_pControlWidget, &ControlPanel::sigBinRunningChanged,
            this, [this](const bool bRunning) { SetStatusLamp(StatusLampBin, bRunning); });
    connect(m_pModeWidget, &ControlPanel::sigMotionRunningChanged,
            this, [this](const bool bRunning) { SetStatusLamp(StatusLampMotion, bRunning); });
    connect(m_pModeWidget, &ControlPanel::sigBinRunningChanged,
            this, [this](const bool bRunning) { SetStatusLamp(StatusLampBin, bRunning); });

    // 「运行」页签层已去掉(2026-11-25 用户反馈: 控制卡片里再套一层页签是多余的)。
    // .ui 里已经没有任何 QTabWidget 包着控制内容, 所以这里没有 removeTab / setCurrentWidget。
    //
    // ⚠️ 别再去引用 ui->tabControl / ui->tabMode —— 这两个"空容器" QWidget 已经被
    // Qt Designer 在保存 .ui 时自动合并掉了(它会把"无属性、只装一个布局"的 QWidget 删掉,
    // 把布局提上一层)。uic 因此不再生成这两个成员, 写了就是 no member named 编译错误。
    // 现在的等价锚点是:
    //   · 控制内容整块禁用 → ui->ControlTabWidget(运动/光源/料仓三组都挂在它下面)
    //   · 模式面板藏/取     → ui->ModeTabWidget + ui->gridLayout_14(它的原位置)
    ui->ModeTabWidget->hide();
    ui->pbControl2Mode->setText(QStringLiteral("配置数据"));
    ui->pbControl2Mode->setEnabled(true);
    ui->pbControl2Mode->setVisible(true);
    ui->pbMode2Control->setVisible(false);
    ui->pbOpenMode->setVisible(false);
    ui->pbSaveMode->setVisible(false);
    ui->pbSaveModeAs->setVisible(false);
    ui->pbNewMode->setVisible(false);

    // 「控制」卡片高度交给内容决定，不吸收窗口富余高度（2026-11-24 用户反馈"没必要这么高"）。
    // 原来 .ui 上挂了两道硬编码下限：groupBox_2 minimumHeight=600、tabPanel minimumHeight=500，
    // 卡片于是永远 600 起跳；窗口再大也只会被 gridLayout 继续撑高，内容下方留一片空。
    // 现在：① 两道 minimumHeight 已从 .ui 删除；② 这里在第 3 行补一个纵向 Expanding 占位，
    // 并把行拉伸全给它 —— 富余高度落到页面最底部（一片干净留白），而不是塞进任何一张卡片。
    ui->gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum,
                                           QSizePolicy::Expanding), 2, 0, 1, 3);
    ui->gridLayout->setRowStretch(2, 1);

    // 控制板数据页 / 模式数据页的底色与按钮观感全部由 style/app.qss 统一提供
    // (2026-11-24 主题改造)。这里原来有两段内联样式表:
    //   ① tabControl 的淡蓝按钮 —— 已被 QSS 的 QPushButton 全局规则取代;
    //   ② tabMode 的整页深灰底   —— 换成浅色只读观感, 理由见文件顶部注释。
    // 切勿在此再补 setStyleSheet: 内联样式优先级高于应用级样式表,
    // 会重新出现"局部和全局打架"的旧问题。

    // 初始禁用控制区: 原来禁的是包在外面的 tabControl, 该容器已被 Designer 合并,
    // 改用 ControlTabWidget —— 运动/光源/料仓三组按钮都是它的后代, 禁用照样传播。
    ui->ControlTabWidget->setEnabled(false);

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


// ==================== 右下角"状态"卡片: 一行状态 + 两个版本号 ====================
// 整张卡片只有一行:
//   电压(V) [12.3]  ● 连接  ● 振动盘运动  ● 料仓运动  软件版本 0.1.0  固件版本 --
//
// 为什么建在 cpp 而不是 .ui:
//   用户的 .ui 会被 Qt Designer 重存(它还会顺手合并掉"空容器"), 在 .ui 里加这些
//   自定义小面板, 下次存盘位置/层级都可能被改掉; 而这里只是把 .ui 里既有的
//   「电压(V)+输入框」那一行整块搬进新容器, 控件一个不删不建, .ui 也一个不动。
//
// 宽度(16px 加粗, 2026-11-25 十五次): 一行约 696px —— 电压 56+44、连接 24+32、
// 振动盘运动 24+80、料仓运动 24+64、软件版本 64+56、固件版本 64+56, 5 道缝 + 版本组前
// 一段固定 16px 的"隔开"间距。卡片的富余宽度由下面第 5 步全给 col2, 组与组之间插等量
// 弹性: 宽了均分、窄了归零。
void ControlWidget::BuildStatusLamps(void)
{
    const int kDotSize = 24;              // 跟着放大后的文字配比(原 16); 96/24=4 的整数 DPR, 边缘最清
    const int kGap = 10;                  // 组与组之间的最小缝
    const int kVersionGap = 16;           // 版本组前额外多留的间距: 让它离状态"稍远但不远"

    // 状态点用 96 的源图并标上 DPR(96/24), 24px 的逻辑尺寸是从 96 个像素采出来的;
    // 直接把 96 缩成 24 的位图再贴, 150%/200% 缩放下边缘会糊(与光源灯同一套做法)
    m_pmStatusOff = QPixmap(QStringLiteral(":/icons/status_off_96.png"));
    m_pmStatusOn  = QPixmap(QStringLiteral(":/icons/status_on_96.png"));
    if (!m_pmStatusOff.isNull() && m_pmStatusOff.width() > kDotSize)
    {
        const qreal dpr = qreal(m_pmStatusOff.width()) / qreal(kDotSize);
        m_pmStatusOff.setDevicePixelRatio(dpr);
        m_pmStatusOn.setDevicePixelRatio(dpr);
    }

    // 名字与 tooltip 的文案在文件头的 kpstrStatus* 三张表里(与 StatusLampIndex 同序)
    QWidget *pRow = new QWidget(ui->groupBox_3);
    pRow->setObjectName(QStringLiteral("StatusLampRow"));
    QHBoxLayout *pRowLayout = new QHBoxLayout(pRow);
    pRowLayout->setContentsMargins(0, 0, 0, 0);
    pRowLayout->setSpacing(kGap);

    // 往一行里塞一组: 每组前面垫一道等量弹性, 于是卡片被拉宽时 6 组是**均匀铺开**的,
    // 而不是全挤在左边、右边留一大片(上一版末尾一道大弹簧就是这样)。
    // 末尾不再加弹性, 否则最后一组会被推离右边缘一大截。
    // i32ExtraGap > 0 时再多插一段固定间距 —— 版本组用它跟运行状态"稍隔开一点"。
    bool bFirstCell = true;
    auto AddCell = [pRowLayout, &bFirstCell](QWidget *pCell, const int i32ExtraGap = 0) {
        if (!bFirstCell)
        {
            pRowLayout->addStretch(1);
            if (i32ExtraGap > 0)
            {
                pRowLayout->addSpacerItem(new QSpacerItem(i32ExtraGap, 0, QSizePolicy::Fixed,
                                                          QSizePolicy::Minimum));
            }
        }
        bFirstCell = false;
        pRowLayout->addWidget(pCell, 0, Qt::AlignVCenter);
    };

    // ---- 1) 电压(V) + 输入框: 把 Designer 里的 gridLayout_17 整块搬进这一行 ----
    // 只换容器、不重建控件: label_4 / leVoltage 还是 .ui 里那两个, 样式和电压刷新
    // 逻辑(OnVoltageResult 直接写 ui->leVoltage)都不用改。
    if (QLayoutItem *pVoltItem = ui->gridLayout_18->itemAtPosition(0, 0))
    {
        if (QLayout *pVoltRow = pVoltItem->layout())
        {
            ui->gridLayout_18->removeItem(pVoltItem);
            pRowLayout->addLayout(pVoltRow);
            bFirstCell = false;   // 电压这一段算第一组, 后面的组都要垫弹性
        }
    }
    // label_4 是 RightToLeft(文字贴右, 正好紧挨输入框), 让它按文字宽固定就行 ——
    // 原来它独占 191px 的整列空白, 一行排布时这些宽度全要省给后面的状态点。
    ui->label_4->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    ui->leVoltage->setFixedWidth(36);     // 16px 加粗下 "12.3" 约 34 宽; 再宽的话 "--" 后面的空白
                                          // 会和"版本组前那道间距"一样大, 反而看不出分组
    ui->leVoltage->setAlignment(Qt::AlignCenter);   // "--" 居中, 两侧各留一半, 不至于一边空一边贴

    // ---- 2) 三盏状态点: [圆点 24px] + [名字] ----
    for (int i32Index = 0; i32Index < int(StatusLampCount); ++i32Index)
    {
        QWidget *pCell = new QWidget(pRow);
        pCell->setObjectName(QStringLiteral("StatusLampCell%1").arg(i32Index));
        QHBoxLayout *pCellLayout = new QHBoxLayout(pCell);
        pCellLayout->setContentsMargins(0, 0, 0, 0);
        pCellLayout->setSpacing(8);                // 点与名字之间的缝

        QLabel *pLamp = new QLabel(pCell);
        pLamp->setObjectName(QStringLiteral("StatusLamp%1").arg(i32Index));
        pLamp->setFixedSize(kDotSize, kDotSize);
        pLamp->setAlignment(Qt::AlignCenter);
        pLamp->setPixmap(m_pmStatusOff);
        m_lstStatusLamps.append(pLamp);
        pCellLayout->addWidget(pLamp, 0, Qt::AlignVCenter);

        QLabel *pName = new QLabel(QString::fromUtf8(kpstrStatusNames[i32Index]), pCell);
        pName->setObjectName(QStringLiteral("StatusLampName%1").arg(i32Index));
        pCellLayout->addWidget(pName, 0, Qt::AlignVCenter);

        AddCell(pCell);
    }

    // ---- 3) 两个版本号: 静态信息, 没有"亮/灭"语义, 所以不带圆点 ----
    // 固件版本要等协议定了才有值, 先占 "--"; 将来收到固件版本帧调
    // SetFirmwareVersion() 一行就能刷, 不用动布局。值列按 "00.00.00" 的宽度留位置,
    // 所以这里用固定最小宽, 版本号从 "--" 变成真值时长短不会让整行左右乱跳。
    {
        QWidget *pCell = new QWidget(pRow);
        pCell->setObjectName(QStringLiteral("StatusVersionCellSoft"));
        QHBoxLayout *pCellLayout = new QHBoxLayout(pCell);
        pCellLayout->setContentsMargins(0, 0, 0, 0);
        pCellLayout->setSpacing(6);                // 名字与版本号之间的缝

        QLabel *pName = new QLabel(QStringLiteral("软件版本"), pCell);
        pName->setObjectName(QStringLiteral("StatusSoftVersionName"));
        pCellLayout->addWidget(pName, 0, Qt::AlignVCenter);
        QLabel *pValue = new QLabel(QString::fromLatin1(kpstrAppVersion), pCell);
        pValue->setObjectName(QStringLiteral("StatusSoftVersion"));
        pValue->setMinimumWidth(56);               // "0.1.0" / 将来的 "1.10.20"
        pValue->setToolTip(QStringLiteral("上位机软件版本"));
        pCellLayout->addWidget(pValue, 0, Qt::AlignVCenter);

        AddCell(pCell, kVersionGap);               // 和运行状态稍微隔开一点
    }
    {
        QWidget *pCell = new QWidget(pRow);
        pCell->setObjectName(QStringLiteral("StatusVersionCellFirm"));
        QHBoxLayout *pCellLayout = new QHBoxLayout(pCell);
        pCellLayout->setContentsMargins(0, 0, 0, 0);
        pCellLayout->setSpacing(6);

        QLabel *pName = new QLabel(QStringLiteral("固件版本"), pCell);
        pName->setObjectName(QStringLiteral("StatusFirmVersionName"));
        pCellLayout->addWidget(pName, 0, Qt::AlignVCenter);
        m_pLabelFirmwareVersion = new QLabel(QStringLiteral("--"), pCell);
        m_pLabelFirmwareVersion->setObjectName(QStringLiteral("StatusFirmVersion"));
        m_pLabelFirmwareVersion->setMinimumWidth(56);   // 给 "xx.xx.xx" 留好位置
        m_pLabelFirmwareVersion->setToolTip(QStringLiteral("固件版本：连接后自动读取"));
        pCellLayout->addWidget(m_pLabelFirmwareVersion, 0, Qt::AlignVCenter);

        AddCell(pCell);
    }

    // ---- 4) 纵向: [弹簧] [这一行] [弹簧] ----
    // 同一行里的「连接方式」卡有 174 高, 这张卡会被拉着等高; 上下弹簧让内容垂直居中,
    // 不然内容会贴着卡片顶上(默认 Alignment 是顶对齐)。
    ui->gridLayout_18->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum,
                                               QSizePolicy::Expanding), 0, 0);
    ui->gridLayout_18->addWidget(pRow, 1, 0);
    ui->gridLayout_18->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum,
                                               QSizePolicy::Expanding), 2, 0);
    ui->gridLayout_18->setRowStretch(0, 1);
    ui->gridLayout_18->setRowStretch(2, 1);

    // ---- 5) 横向: 让这张卡吃下右侧全部富余宽度 ----
    // 原来 gridLayout 的 col1 是一道 Expanding 空占位(实测 291px), col2 的状态卡
    // 只按 278 的 sizeHint 取宽度 —— 一行内容排不下。这里把占位清成 0 宽、列拉伸
    // 全给 col2。窗口变窄时 Qt 会先压 col0 的连接方式卡(最小 331), 状态卡反而更宽。
    if (ui->horizontalSpacer_5 != nullptr)
    {
        ui->horizontalSpacer_5->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Minimum);
    }
    ui->gridLayout->setColumnStretch(0, 0);
    ui->gridLayout->setColumnStretch(1, 0);
    ui->gridLayout->setColumnStretch(2, 1);
    ui->gridLayout->invalidate();

    // 初始一律灰点(tooltip 由 SetStatusLamp 一并写成"灰"的文案)
    for (int i32Index = 0; i32Index < int(StatusLampCount); ++i32Index)
    {
        SetStatusLamp(i32Index, false);
    }
}


// 刷固件版本号(协议定了以后由固件版本应答帧调用)。空串按 "--" 显示, 免得卡片上
// 突然少一截文字让整行宽度跳动。
void ControlWidget::SetFirmwareVersion(const QString &qstrVersion)
{
    if (m_pLabelFirmwareVersion != nullptr)
    {
        m_pLabelFirmwareVersion->setText(qstrVersion.isEmpty() ? QStringLiteral("--")
                                                               : qstrVersion);
        m_pLabelFirmwareVersion->setToolTip(
                    qstrVersion.isEmpty() ? QStringLiteral("固件版本：未读取到（连接后自动读取）")
                                          : QStringLiteral("固件版本：%1").arg(qstrVersion));
    }
}


// 点亮/熄灭一盏状态点。状态文案写进 tooltip: 现场不看颜色也能读到"连接正常/运动中"。
void ControlWidget::SetStatusLamp(const int iIndex, const bool bOn)
{
    if (iIndex < 0 || iIndex >= m_lstStatusLamps.size())
    {
        return;
    }

    if (QLabel *pLamp = m_lstStatusLamps.at(iIndex))
    {
        pLamp->setPixmap(bOn ? m_pmStatusOn : m_pmStatusOff);

        const int iClamped = qBound(0, iIndex, int(StatusLampCount) - 1);
        pLamp->setToolTip(QStringLiteral("%1：%2").arg(
                              QString::fromUtf8(kpstrStatusNames[iClamped]),
                              QString::fromUtf8(bOn ? kpstrStatusTipOn[iClamped]
                                                    : kpstrStatusTipOff[iClamped])));
    }
}


// 铺满屏幕可用区(不含任务栏)。原来 main.cpp 直接调 showMaximized(), 但窗口
// 去边框后 Qt 会按 screenGeometry 铺满, 把任务栏一起盖住; 这里改走自绘标题栏
// 里的 availableGeometry 逻辑, 最大化/还原都只有一份实现。
void ControlWidget::MaximizeToScreen(void)
{
    if (ui->titleBar != nullptr)
    {
        ui->titleBar->SetMaximized(true);
    }
}


// 无边框窗口没有系统画的边框, 自己补一圈 1px 描边, 否则窗口边界和桌面糊在一起
void ControlWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setPen(QColor(0xB4, 0xBC, 0xC7));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}


// 右下角缩放握把贴住窗口右下角(去掉系统边框后, 这里是唯一能拖拽缩放的入口)
void ControlWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_pSizeGrip != nullptr)
    {
        m_pSizeGrip->move(width() - m_pSizeGrip->width(), height() - m_pSizeGrip->height());
        m_pSizeGrip->raise();
    }
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
    // 配置窗口也能发指令, 所以"有动作在跑"要看两个面板(在窗口里试跑的那条同样要提示)
    const bool bMotionRunning = ((m_pControlWidget != nullptr) && m_pControlWidget->IsMotionRunning()) ||
                                ((m_pModeWidget != nullptr) && m_pModeWidget->IsMotionRunning());

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

        // 连接成功: 放开控制区。禁用/放开都作用在 ControlTabWidget 上(见构造函数注释),
        // 别再写 ui->tabControl —— 那个容器已被 Designer 合并, uic 不再生成成员。
        ui->ControlTabWidget->setEnabled(true);

        // 连接成功: 放开固件参数页的"获取/保存配置"按钮(断连时会被收回)
        if (m_pControlWidget != nullptr)
        {
            m_pControlWidget->SetKeyParamsAvailable(true);
        }
        if (m_pModeWidget != nullptr)
        {
            m_pModeWidget->SetKeyParamsAvailable(true);
            // 配置窗口里的运动/料仓/光源键跟着链路放开(内部会先把面板刷成空闲态)
            m_pModeWidget->SetRunEnabled(true);
            m_pModeWidget->SyncRunButtonStates(m_pControlWidget);
        }

        // 连接成功: 启动电压周期查询(2s 一次), 立即先发一帧
        if (m_pVoltageTimer != nullptr)
        {
            m_pVoltageTimer->start();
            OnVoltageQueryTick();
        }

        // 连接成功: 读一次固件版本(0x19), 只读一次 —— 固件不换, 版本不会变。
        // 走 QueuedConnection 投递到通信线程, 与电压查询同一条路径。
        if (m_ConnectiveHandler != nullptr)
        {
            QMetaObject::invokeMethod(m_ConnectiveHandler, "SendGetFirmwareVersionCommand",
                                      Qt::QueuedConnection);
        }

        // 连接状态灯: 链路已建立且心跳在跑 → 绿点(TCP / 串口 走的是同一条路径)
        SetStatusLamp(StatusLampConnect, true);
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
// 统一在此禁用 ui->ControlTabWidget: 心跳丢失 / 主动断开 TCP/485 / 连接失败 / 析构 全部走这里,
// 断连后控制面板不能再操作(按钮即使被 sigMotionTerminate 恢复为 enabled, 因父容器禁用也不可点;
// 下次连接成功时 OnConnectiveState(true) 会重新 setEnabled(true))。
void ControlWidget::CloseConnection(void)
{
    ui->ControlTabWidget->setEnabled(false);

    // 连接状态灯: 主动断开 / 连接失败 / 心跳丢失 / 退出 全部汇到这里 → 灰点。
    // 放在最前面是因为下面紧接着就要销毁通信对象, 越早把"已断开"显出来越不会误读。
    SetStatusLamp(StatusLampConnect, false);

    // 断连时在飞的指令不会再有完成帧(下面 disconnect() 还会把信号全掐掉),
    // 控制面板的运动锁必须在这里复位, 否则重连后运动类型按钮/开始按钮一直停在禁用态
    if (m_pControlWidget != nullptr)
    {
        m_pControlWidget->ResetMotionUiState();
        m_pControlWidget->ResetKeyParamsUiState();
    }

    // "配置数据"窗口的面板: 固件参数事务要复位; 试跑中的运动/料仓也要无条件回空闲态
    // (顺序不能反: 先 ResetMotionUiState 再 SetRunEnabled(false) —— 禁用后前者的
    //  "回空闲"会被自身判据挡掉, 灯就停在绿色了)
    if (m_pModeWidget != nullptr)
    {
        m_pModeWidget->ResetMotionUiState();
        m_pModeWidget->SetRunEnabled(false);
        m_pModeWidget->ResetKeyParamsUiState();
    }

    // 断连后停止电压周期查询并恢复占位, 避免残留旧读数误导
    if (m_pVoltageTimer != nullptr)
    {
        m_pVoltageTimer->stop();
    }
    ui->leVoltage->setText("--");

    // 固件版本同样复位: 断连后停在旧的版本号会让人以为"还连着"
    SetFirmwareVersion(QString());

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

        // 光源保持时间到: 下位机自发关闭完成帧, 复位对应路光源按钮
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigLightOffFinish,
                m_pControlWidget, &ControlPanel::OnLightOffFinish);

        // "配置数据"窗口里的面板同样要能下发(2026-11-25 二十次): 它自己组帧、自己发,
        // 参数就是用户刚在窗口里改的那一份 —— 不必先"应用"再回运行页点一遍。
        // 两个面板共用同一个通信引擎, 引擎的队列/应答匹配按帧走, 与来自哪个面板无关。
        connect(m_pModeWidget, SIGNAL(sigMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendMotionCommand(QByteArray, QByteArray)));
        connect(m_pModeWidget, SIGNAL(sigStopMotion()), m_ConnectiveHandler, SLOT(SendStopMotionCommand()));
        connect(m_pModeWidget, SIGNAL(sigBinMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendBinMotionCommand(QByteArray, QByteArray)));
        connect(m_pModeWidget, SIGNAL(sigBinStopMotion()), m_ConnectiveHandler, SLOT(SendBinStopMotionCommand()));
        connect(m_pModeWidget, SIGNAL(sigLightOn(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendLightOnCommand(QByteArray, QByteArray)));
        connect(m_pModeWidget, SIGNAL(sigLightOff(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendLightOffCommand(QByteArray, QByteArray)));
        // 光源到时帧两个面板都要收: 谁开的那一路, 谁的按钮要恢复
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigLightOffFinish,
                m_pModeWidget, &ControlPanel::OnLightOffFinish);

        // 固件参数(0x15~0x18): 固件参数页只在"配置数据"窗口里可见, 而那个面板是
        // m_pModeWidget, 所以两个面板都要接线 ——
        // 谁在用都不会漏, 不在用的那个靠内部的 pending 标志丢弃非本次事务的应答
        connect(m_pControlWidget, &ControlPanel::sigGetKeyParams,
                m_ConnectiveHandler, &ConnectiveHandle::SendGetKeyParamsCommand);
        connect(m_pControlWidget, &ControlPanel::sigSaveKeyParams,
                m_ConnectiveHandler, &ConnectiveHandle::SendSaveKeyParamsCommand);
        connect(m_pModeWidget, &ControlPanel::sigGetKeyParams,
                m_ConnectiveHandler, &ConnectiveHandle::SendGetKeyParamsCommand);
        connect(m_pModeWidget, &ControlPanel::sigSaveKeyParams,
                m_ConnectiveHandler, &ConnectiveHandle::SendSaveKeyParamsCommand);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigGetKeyParamsResult,
                m_pControlWidget, &ControlPanel::OnGetKeyParamsResult);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigGetKeyParamsResult,
                m_pModeWidget, &ControlPanel::OnGetKeyParamsResult);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigSaveKeyParamsResult,
                m_pControlWidget, &ControlPanel::OnSaveKeyParamsResult);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigSaveKeyParamsResult,
                m_pModeWidget, &ControlPanel::OnSaveKeyParamsResult);

        // 保存IP 的结果(成功/失败/超时)由工作线程回传
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigSaveIpResult,
                this, &ControlWidget::OnSaveIpResult);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigGetIpResult,
                this, &ControlWidget::OnGetIpResult);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigGetVoltageResult,
                this, &ControlWidget::OnVoltageResult);
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigGetFirmwareVersionResult,
                this, &ControlWidget::OnFirmwareVersionResult);

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

        // 完成帧/超时/停止 三路终止都汇到这里: 两个面板都要解锁,
        // 否则在配置窗口里发的那一条跑完后, 那个面板的"发送"会一直是灰的
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigMotionTerminate, this, [this]() {
            emit m_pControlWidget->sigMotionTerminate();
            emit m_pModeWidget->sigMotionTerminate();
        });

        qDebug() << "set slot after connected";
    }
    else
    {
        disconnect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));
        disconnect(m_pControlWidget, SIGNAL(sigMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendMotionCommand(QByteArray, QByteArray)));
        disconnect(m_ConnectiveHandler, &ConnectiveHandle::sigLightOffFinish,
                   m_pControlWidget, &ControlPanel::OnLightOffFinish);
        disconnect(m_pModeWidget, SIGNAL(sigMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendMotionCommand(QByteArray, QByteArray)));
        disconnect(m_ConnectiveHandler, &ConnectiveHandle::sigLightOffFinish,
                   m_pModeWidget, &ControlPanel::OnLightOffFinish);
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

    // 应答异常(含运动/料仓反馈超时): 引擎已清队, 两个面板都要回空闲态,
    // 否则在配置窗口里发的那一条会让那个面板的按钮一直停在运动锁里
    emit m_pControlWidget->sigMotionTerminate();
    emit m_pModeWidget->sigMotionTerminate();
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

void ControlWidget::on_pbControl2Mode_clicked()
{
    if (m_pConfigDialog != nullptr)
    {
        m_pConfigDialog->show();
        m_pConfigDialog->raise();
        m_pConfigDialog->activateWindow();
        return;
    }

    // 参数先对齐(运行页 → 配置页); 再对齐"运行类"按钮态: 哪几路光源亮着、
    // 有没有指令在跑, 两个面板必须是同一套, 否则关闭键会点不动/点错
    m_pModeWidget->CopyButtonParams(m_pControlWidget);
    m_pModeWidget->SyncRunButtonStates(m_pControlWidget);
    m_pConfigDialog = new QDialog(this);
    m_pConfigDialog->setObjectName(QStringLiteral("ConfigDialog"));
    m_pConfigDialog->setWindowTitle(QStringLiteral("配置数据"));
    m_pConfigDialog->setModal(false);
    // 默认尺寸不写死（原来 1050×720）：组装完按内容 sizeHint 取"最小但不挤"的尺寸，
    // 见本函数末尾的 adjustSize()。之后可以再放大，但缩不小到内容放不下。
    QVBoxLayout *pLayout = new QVBoxLayout(m_pConfigDialog);
    QHBoxLayout *pFiles = new QHBoxLayout();
    const QList<QPushButton *> Buttons = {ui->pbOpenMode, ui->pbSaveMode, ui->pbSaveModeAs, ui->pbNewMode};

    // ---- 四个模式按钮：定长、四等宽、宽度只装得下文字（2026-11-25 十八次）----
    // 坑：QPushButton 的水平 sizePolicy 是 Minimum —— sizeHint 只是**下限**，布局里
    // 有余量时它照样会被拉长。原来直接 addWidget，四个键被窗口宽度等分成四条长条。
    // 这里按**最长的一句**（「模式另存为」5 个字）量一次 sizeHint，四个统一 setFixedSize
    // （min=max 锁死，谁也拉不动），行尾补一道弹性把富余宽度甩到右边、按钮靠左。
    int i32ButtonWidth = 0;
    int i32ButtonHeight = 0;
    for (QPushButton *pButton : Buttons)
    {
        i32ButtonWidth = qMax(i32ButtonWidth, pButton->sizeHint().width());
        i32ButtonHeight = qMax(i32ButtonHeight, pButton->sizeHint().height());
    }
    for (QPushButton *pButton : Buttons)
    {
        pButton->setFixedSize(i32ButtonWidth, i32ButtonHeight);
        pFiles->addWidget(pButton);
        pButton->setEnabled(true);
        pButton->show();
    }
    pFiles->addStretch(1);
    pLayout->addLayout(pFiles);
    pLayout->addWidget(ui->ModeTabWidget, 1);
    ui->ModeTabWidget->show();
    // 提示文字(2026-11-25 二十次): 三个页签里的发送键现在直接以窗口内当前的编辑值下发,
    // 便于"配完立刻看效果"; 但它不写运行页参数、也不存盘 —— 要留下必须另点"应用"。
    QLabel *pNotice = new QLabel(QStringLiteral("发送键按窗口内当前值直接下发（不改变运行页参数、不存盘）；运动中不可应用。文件保存仅存盘，不自动应用。"), m_pConfigDialog);
    // 必须允许折行: 这句比原来长一倍, 不折行会把窗口宽度顶宽一大截,
    // 而窗口默认尺寸是 adjustSize() 按内容算的("最小但不挤", 十八次定的规矩)。
    // 再把最大宽度钉在页签区的自然宽度上: 折行后的 sizeHint 仍会把窗口顶宽 14px,
    // 定了上限以后窗口宽度只由页签区决定, 这句只负责换行(实测 809 → 795)。
    pNotice->setWordWrap(true);
    pNotice->setMaximumWidth(ui->ModeTabWidget->sizeHint().width());
    pLayout->addWidget(pNotice);
    QDialogButtonBox *pBox = new QDialogButtonBox(m_pConfigDialog);
    QPushButton *pApply = pBox->addButton(QStringLiteral("应用（不存盘）"), QDialogButtonBox::ActionRole);
    pApply->setObjectName(QStringLiteral("ConfigApply"));
    QPushButton *pSave = pBox->addButton(QStringLiteral("保存并应用"), QDialogButtonBox::ActionRole);
    pSave->setObjectName(QStringLiteral("ConfigSaveApply"));
    pBox->addButton(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消关闭"));
    pLayout->addWidget(pBox);
    connect(pApply, &QPushButton::clicked, this, [this]() { ApplyConfiguration(false); });
    connect(pSave, &QPushButton::clicked, this, [this]() { ApplyConfiguration(true); });
    connect(pBox, &QDialogButtonBox::rejected, m_pConfigDialog, &QDialog::reject);
    connect(m_pConfigDialog, &QDialog::finished, this, [this, Buttons]() {
        QDialog *pDialog = m_pConfigDialog;
        // 关闭前把"运行类"按钮态抄回运行页: 刚才在窗口里试开的那几路光源、
        // 正在跑的那条运动, 运行页上的键必须与之一致(否则会误以为灯是灭的)
        m_pControlWidget->SyncRunButtonStates(m_pModeWidget);
        // 归还模式面板: 原来的位置是 tabMode → gridLayout_14 → gridLayout_13,
        // 前两个容器已被 Designer 合并, 只剩 gridLayout_13 还在, 直接放回它里面。
        ui->gridLayout_13->addWidget(ui->ModeTabWidget);
        ui->ModeTabWidget->hide();
        for (QPushButton *pButton : Buttons)
        {
            pButton->setParent(this);
            pButton->hide();
        }
        m_pConfigDialog = nullptr;
        pDialog->deleteLater();
    });
    // 默认尺寸 = 内容的自然尺寸（"最小但不影响视觉和操作"）。旧版写死 1050×720，
    // 1400 宽的主窗下打开就是一大片空白。adjustSize() 取 layout 的 sizeHint：
    // 宽由"按钮行 / ModeTabWidget / 提示文字 / 按钮盒"里最宽的那个决定，高同理。
    // 再把 sizeHint 锁成最小值 —— 之后能放大，但缩不到挤坏布局。
    m_pConfigDialog->adjustSize();
    m_pConfigDialog->setMinimumSize(m_pConfigDialog->sizeHint());
    m_pConfigDialog->show();
}

void ControlWidget::ApplyConfiguration(const bool bSave)
{
    // 配置窗口自己也能下发指令了, 所以"运动中不可应用"要同时看两个面板:
    // 只看运行页会漏掉"在配置窗口里刚试跑了一条"的情况, 参数会在运动半途被换掉
    if (m_pControlWidget->IsMotionRunning() || m_pModeWidget->IsMotionRunning())
    {
        QMessageBox::warning(m_pConfigDialog, QStringLiteral("不可应用"),
                             QStringLiteral("运动正在执行，请先停止（运行页或本窗口的“停止”都可以），或等待执行结束后再应用。"));
        return;
    }
    if (bSave)
    {
        // SaveSettings 为 void，不能把调用完成当作存盘成功。
        m_pModeWidget->SaveSettings();
    }
    m_pControlWidget->CopyButtonParams(m_pModeWidget);
    if (bSave)
    {
        QMessageBox::information(m_pConfigDialog, QStringLiteral("已应用"),
                                 QStringLiteral("参数已应用；已请求写入当前模式文件，存盘结果未确认。"));
    }
    m_pConfigDialog->accept();
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
        ApplyConfiguration(false);
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


// 电压周期查询: 每 2s 投递一帧 0x13 到通信线程; 断连后定时器已停, 双重保护
void ControlWidget::OnVoltageQueryTick()
{
    if (m_ConnectiveHandler == nullptr || m_pConnectiveThread == nullptr ||
            m_eConnectObject == ConnectObject::ObjectNone)
    {
        return;
    }

    QMetaObject::invokeMethod(m_ConnectiveHandler, "SendGetVoltageCommand",
                              Qt::QueuedConnection);
}


// 电压应答: 下位机换算系数已含在值里, 这里只除 10 显示一位小数
void ControlWidget::OnVoltageResult(const quint16 &u16VoltX10)
{
    ui->leVoltage->setText(QString::number(u16VoltX10 / 10.0, 'f', 1));
}


// 固件版本应答: 引擎已把三个字节拼成 "主.次.修订"; 空串(超时/旧固件)按 "--" 显示
void ControlWidget::OnFirmwareVersionResult(const QString &qstrVersion)
{
    SetFirmwareVersion(qstrVersion);
}
