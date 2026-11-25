#include "controlpanel.h"
#include "ui_controlpanel.h"
#include "groupmotion.h"
#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QAbstractSpinBox>
#include <QSlider>
#include <QDial>
#include <QGroupBox>
#include <QGridLayout>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QLabel>


namespace
{
// ---------------- 运动键图标 ----------------
// 图标本体是 style/icons/*.png(由 build-check/make_icons.cpp 从 icons/src/*.svg 光栅化),
// 颜色只在本文件定义: 想换主色改 kpszIconGreen 一处即可, 不用重新导出素材。
// 绿色 = 动作语义色, 和主色蓝(可操作/选中)、红(停止/断开)区分开。
const char *const kpszIconGreen = "#16A34A";
// 光源通道名(A~D): 状态灯的 tooltip 用它, 下标 0~3 与协议索引 1~4 对应
const char *const kpstrLightNames[4] = {"A", "B", "C", "D"};
const char *const kpszIconGray = "#9CA3AF";   // 与 QSS 里的禁用文字色一致

// 时间类参数(运动/料仓/光源, 单位秒)的上限。
// 2026-11-25 协议时间域由 2 字节扩到 4 字节, 秒×10 最大 99990, 界面统一给到 9999s。
// 改了这里要同步 doc/通信协议.docx 与下位机的解析宽度(下位机若还用 uint16 计时会回绕)。
const double kMaxTimeUse = 9999.0;

// 把单色图标整体换色: SourceIn 只保留新颜色的 alpha 形状, 抗锯齿边缘不受影响
QPixmap RecolorPixmap(const QPixmap &pmSrc, const QColor &clrTint)
{
    QPixmap pmOut(pmSrc.size());
    pmOut.setDevicePixelRatio(pmSrc.devicePixelRatio());
    pmOut.fill(Qt::transparent);

    QPainter pnt(&pmOut);
    pnt.drawPixmap(0, 0, pmSrc);
    pnt.setCompositionMode(QPainter::CompositionMode_SourceIn);
    pnt.fillRect(pmOut.rect(), clrTint);
    pnt.end();

    return pmOut;
}

// 4 种 mode × On/Off 全部给齐。少给一种, Qt 会按"最接近的一张"回退,
// 典型症状: 选中(实心蓝底)时图标还是绿的, 绿压蓝糊成一团; 或者禁用态图标仍是彩色的。
QIcon MakeMotionIcon(const QString &qstrName)
{
    const QColor clrGreen(kpszIconGreen);
    const QColor clrGray(kpszIconGray);
    const QColor clrWhite(Qt::white);

    struct IconVariant
    {
        QIcon::Mode eMode;
        QIcon::State eState;
        QColor clrTint;
    };
    const QList<IconVariant> lstVariants = {
        {QIcon::Normal,   QIcon::Off, clrGreen},   // 常态
        {QIcon::Active,   QIcon::Off, clrGreen},   // 悬停 / 按下
        {QIcon::Selected, QIcon::Off, clrGreen},
        {QIcon::Normal,   QIcon::On,  clrWhite},   // 选中: 蓝底, 必须转白
        {QIcon::Active,   QIcon::On,  clrWhite},
        {QIcon::Selected, QIcon::On,  clrWhite},
        {QIcon::Disabled, QIcon::Off, clrGray},    // 禁用: 灰底灰图标
        {QIcon::Disabled, QIcon::On,  clrGray}};

    QIcon icoOut;
    for (const int iSize : {48, 96})
    {
        const QPixmap pmSrc(QStringLiteral(":/icons/%1_%2.png").arg(qstrName).arg(iSize));
        if (pmSrc.isNull())
        {
            qWarning() << "motion icon missing:" << qstrName << iSize;
            continue;
        }
        for (const IconVariant &sVariant : lstVariants)
        {
            icoOut.addPixmap(RecolorPixmap(pmSrc, sVariant.clrTint),
                             sVariant.eMode, sVariant.eState);
        }
    }

    return icoOut;
}
}


ControlPanel::ControlPanel(QWidget *parent, const bool bFlatRun) :
    QWidget(parent),
    ui(new Ui::ControlPanel)
{
//    ui->setupUi(this);

    // ⚠️ 这里传的是 parent 而不是 this(历史写法, 为了把 UI 直接铺到 QTabWidget 上)。
    // 副作用: setupUi() 末尾的 QMetaObject::connectSlotsByName(o) 里的 o 就是这里的 parent,
    // 它只会拿 **parent 的元对象** 去匹配 on_<控件名>_<信号>, 而槽都定义在 ControlPanel 上,
    // 永远匹配不到 → **所有"转到槽"自动连接全部失效**。
    // 因此本类每个 on_xxx_clicked/valueChanged 都必须手动 connect, 见下面的"连接按钮事件"段。
    // 若哪天改成 setupUi(this), 必须同时把本控件正确 addTab 进 QTabWidget, 否则界面会错位。
    ui->setupUi(parent);

    if (bFlatRun)
    {
        // 复用 Designer 的内层布局，不能对已有布局的 parent 再安装布局。
        ui->gridLayout->removeWidget(ui->tabWidget);
        ui->tabWidget->hide();
        QHBoxLayout *pFlatLayout = new QHBoxLayout();
        ui->gridLayout->addLayout(pFlatLayout, 0, 0);
        const QStringList Titles = {QStringLiteral("运动"), QStringLiteral("光源"), QStringLiteral("料仓")};
        // 三张卡的归属先收集起来, 外层怎么摆放到循环之后统一决定(见下面的"外层排布")
        QGroupBox *pRunGroups[3] = {nullptr, nullptr, nullptr};
        for (int s32Index = 0; s32Index < 3; ++s32Index)
        {
            QWidget *pPage = ui->tabWidget->widget(0);
            ui->tabWidget->removeTab(0);
            // 标题只由下面的蓝色标题条承担：QGroupBox 自带标题会和它重复（框外一行灰字 + 框内一行蓝底粗体）。
            QGroupBox *pGroup = new QGroupBox(parent);
            pGroup->setObjectName(QStringLiteral("RunGroup%1").arg(s32Index));
            pGroup->setTitle(QString());
            QVBoxLayout *pLayout = new QVBoxLayout(pGroup);
            pLayout->setContentsMargins(4, 4, 4, 4);
            pLayout->setSpacing(4);
            QLabel *pTitle = new QLabel(Titles.at(s32Index), pGroup);
            pTitle->setObjectName(QStringLiteral("RunGroupTitle%1").arg(s32Index));
            // 标题条观感(字重/颜色/底部分隔线)统一由 style/app.qss 里
            // QLabel#RunGroupTitleN 提供, 这里不再写内联样式。
            // 但"定高 + 纵向 Fixed"必须留在代码里 —— 那是布局约束不是观感,
            // 交给样式表改不到, 漏了就会出现"标题下面一大片空白"(2026-11-21 踩过)。
            pTitle->setFixedHeight(24);
            pTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            pTitle->show();
            pLayout->addWidget(pTitle, 0);
            pLayout->addWidget(pPage, 0);
            // 富余高度统一留在底部（与光源/料仓页内 spacer 的观感一致），不再撑高标题条或拉开按钮间距。
            pLayout->addStretch(1);
            pRunGroups[s32Index] = pGroup;
            pPage->show();
            for (QWidget *pEditor : pPage->findChildren<QWidget *>())
            {
                if (qobject_cast<QAbstractSpinBox *>(pEditor) ||
                    qobject_cast<QAbstractSlider *>(pEditor) ||
                    qobject_cast<QLineEdit *>(pEditor) ||
                    // "保持"复选框也是参数编辑控件: 运行页只保留按钮, 值仍参与下发
                    qobject_cast<QCheckBox *>(pEditor))
                {
                    pEditor->hide();
                    pEditor->setEnabled(false);
                }
            }
            for (QLabel *pLabel : pPage->findChildren<QLabel *>())
            {
                pLabel->hide();
            }
        }
        // ---- 外层排布: 运动占左列, 光源在上、料仓在下占右列（2026-11-25 需求）----
        // 料仓只有一个"发送运动/停止", 单独占一列时整张卡几乎全空(实测 284×440 只放
        // 了 2 个小按钮), 太浪费。把料仓并到光源下面: 右列少一列, 让出来的宽度正好给
        // 光源那 8 个按钮铺开用掉, 空白面积比原来少了一大块。
        // 高度按内容比例 4:1 分(光源 4 行键 / 料仓 1 行键), 料仓不会被拉成一张大空卡。
        // 间距/边距跟外层横排保持一致, 否则两列之间和两张卡之间的缝对不齐。
        pFlatLayout->addWidget(pRunGroups[0], 1);
        QVBoxLayout *pRightCol = new QVBoxLayout();
        pRightCol->setContentsMargins(pFlatLayout->contentsMargins());
        pRightCol->setSpacing(pFlatLayout->spacing());
        pRightCol->addWidget(pRunGroups[1], 4);
        pRightCol->addWidget(pRunGroups[2], 1);
        pFlatLayout->addLayout(pRightCol, 1);

        // 隐藏的高级页及其 G1/G2 控件留在 tabWidget 中，仍参与运行取值。
        ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->horizontalSpacer_8->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        // horizontalSpacer_9 不再清零(2026-11-24): 它是"运动键区"和"发送/停止"
        // 之间唯一的弹性间隔。清零后富余宽度会被平均摊给键区那三列,
        // 方块之间出现 40+px 的大缝(实测 60px 方块被摊到 103px 列宽)。
        // 留着它 Expanding, 键区贴左、操作键贴右, 方块间距才是网格自身间距。

        // ---- 光源区: A/B/C/D 四组, 每组 = 状态灯 + 开启/关闭（2026-11-25 十二次）----
        // .ui 里光源是「4 组 × 2 行」的纵向单列（标签 + 亮度/时间编辑器 + 开/关键），
        // 运行页把标签和编辑器全隐藏后只剩 8 个按钮，谁开着只能看按钮文字，现场扫一眼看不出来。
        // 现在每组补一盏状态灯（灰=灭 / 绿=亮），按钮宽度收到 130（原来铺满 253 的一半）：
        //   每组 = [灯 36×36] + [开启/关闭 竖排, 各 130×36]
        //   4 组摆成 2×2：左列 A/B、右列 C/D（与原来的列归属一致）
        // 横向富余交给三处等宽伸缩 —— 边距 : 组间 : 边距 三个缝一样宽，整块居中，
        // 不再像以前那样右边甩出一大块白。
        //
        // 做法: 把 gridLayout_5 里剩下的东西整体摘掉，换成一个自建面板挂进同一个格子。
        // **控件只摘不删** —— 亮度/时间编辑器虽然隐藏，值仍要参与下发（构造时读它们算协议帧）。
        const int kLampSize = 36;        // 灯与按键同高, 一眼能看出"这盏灯属于这组键"
        const int kLightKeyWidth = 130;  // ≈ 253/2
        // 状态灯位图: 用 96 的源图并标上 DPR(96/36), 这样 36px 的逻辑尺寸是从 96 个
        // 像素采出来的; 直接把 96 缩成 36 的位图再贴, 150%/200% 缩放下会糊。
        m_pmLampOff = QPixmap(QStringLiteral(":/icons/lamp_off_96.png"));
        m_pmLampOn  = QPixmap(QStringLiteral(":/icons/lamp_on_96.png"));
        if (!m_pmLampOff.isNull() && m_pmLampOff.width() > kLampSize)
        {
            const qreal dpr = qreal(m_pmLampOff.width()) / qreal(kLampSize);
            m_pmLampOff.setDevicePixelRatio(dpr);
            m_pmLampOn.setDevicePixelRatio(dpr);
        }
        while (QLayoutItem *pItem = ui->gridLayout_5->takeAt(0))
        {
            if (pItem->widget() != nullptr)
            {
                pItem->widget()->hide();
            }
            delete pItem;   // 删的是布局项(含弹簧), 控件本身不删
        }

        QWidget *pLightHost = new QWidget(ui->gridLayout_5->parentWidget());
        pLightHost->setObjectName(QStringLiteral("LightLampPanel"));
        QVBoxLayout *pHostLayout = new QVBoxLayout(pLightHost);
        pHostLayout->setContentsMargins(0, 0, 0, 0);
        pHostLayout->setSpacing(34);   // 两排之间的纵向间隔(横向的缝由两侧等宽伸缩给出)
        ui->gridLayout_5->addWidget(pLightHost, 0, 0, 1, 1);

        // A~D 的开/关键, 下标与协议索引一致(1=A ... 4=D, 这里用 0~3)
        QPushButton *arrOpenKeys[4] = {ui->pbOpenLightA, ui->pbOpenLightB,
                                       ui->pbOpenLightC, ui->pbOpenLightD};
        QPushButton *arrCloseKeys[4] = {ui->pbCloseLightA, ui->pbCloseLihgtB,
                                        ui->pbCloseLightC, ui->pbCloseLightD};
        QList<QWidget *> lstLightGroups;
        for (int i32Lamp = 0; i32Lamp < 4; ++i32Lamp)
        {
            QWidget *pGroup = new QWidget(pLightHost);
            pGroup->setObjectName(QStringLiteral("LightGroup%1").arg(kpstrLightNames[i32Lamp]));
            QHBoxLayout *pGroupLayout = new QHBoxLayout(pGroup);
            pGroupLayout->setContentsMargins(0, 0, 0, 0);
            pGroupLayout->setSpacing(10);

            QLabel *pLamp = new QLabel(pGroup);
            pLamp->setObjectName(QStringLiteral("LightLamp%1").arg(i32Lamp));
            pLamp->setFixedSize(kLampSize, kLampSize);
            // 灯按**中线**和两个键对齐(键区高 78、灯 36), 顶对齐会显得灯"吊"在上面
            pLamp->setAlignment(Qt::AlignCenter);
            pGroupLayout->addWidget(pLamp, 0, Qt::AlignVCenter);
            m_lstLightLamps.append(pLamp);
            pLamp->setPixmap(m_pmLampOff);

            QVBoxLayout *pKeys = new QVBoxLayout();
            pKeys->setContentsMargins(0, 0, 0, 0);
            pKeys->setSpacing(6);
            const QList<QPushButton *> lstKeys{arrOpenKeys[i32Lamp], arrCloseKeys[i32Lamp]};
            for (QPushButton *pButton : lstKeys)
            {
                pButton->setFixedSize(kLightKeyWidth, 36);   // 宽度减半, 高度仍是 24×1.5
                pButton->show();
                pKeys->addWidget(pButton);
            }
            pGroupLayout->addLayout(pKeys);
            lstLightGroups.append(pGroup);
        }

        // 2×2 摆放: 第 0 行 A|C, 第 1 行 B|D
        for (int i32Row = 0; i32Row < 2; ++i32Row)
        {
            QHBoxLayout *pRow = new QHBoxLayout();
            pRow->setContentsMargins(0, 0, 0, 0);
            pRow->setSpacing(0);
            pRow->addStretch(1);
            pRow->addWidget(lstLightGroups.at(i32Row));
            pRow->addStretch(1);
            pRow->addWidget(lstLightGroups.at(i32Row + 2));
            pRow->addStretch(1);
            pHostLayout->addLayout(pRow);
        }

        // 全部摆好以后再统一刷一次状态: 建灯的时候布局还没装配完,
        // 那时调 SetLightLamp 会和 QLabel 的 sizeHint 抢同一个时机(实测会让其中一组宽度算成 0),
        // 放到这里既安全又只走一次。
        SetAllLightLampsOff();

        // ---- 料仓区: 两个键横着并排铺满卡片（2026-11-25 需求）----
        // .ui 里料仓是"标签 + 参数编辑器"占 row 0~2、按钮竖排在 row 3/4 的单列网格。
        // 运行页把标签和编辑器全隐藏后, 只剩"发送运动/停止"两个 90px 宽的小键贴左上角,
        // 卡片其余部分全空 —— 这就是"料仓那片太浪费"的根源。
        // 运行页改为: 两个键并排, 高度 ×1.5 到 36。
        // 弹簧 horizontalSpacer_3 也要摘掉, 否则它会在右边继续吸走富余宽度。
        ui->gridLayout_7->removeItem(ui->horizontalSpacer_3);
        ui->gridLayout_7->removeWidget(ui->pbBinStartMotion);
        ui->gridLayout_7->removeWidget(ui->pbBinStopMotion);
        QHBoxLayout *pBinRow = new QHBoxLayout();
        pBinRow->setContentsMargins(0, 0, 0, 0);
        pBinRow->setSpacing(8);
        // 长度再缩一半(2026-11-25 十六次): 原来是等分铺满整张卡, 各 304px,
        // 4 个字的键长到 300 多显得空荡、字还小。现在各 152px(304 的一半)居中摆,
        // 左右各留一段等量空白 —— 与上方光源四组的居中方式一致, 不会一边贴边一边空。
        // 宽度写死而不是按卡片宽度算: 卡片宽度要等 show() 之后才有值, 而这里在构造期,
        // 写死 152 在 1400/最大化两种窗口下都稳定。
        const int kBinButtonWidth = 152;
        const int kBinButtonHeight = 36;
        pBinRow->addStretch(1);
        pBinRow->addWidget(ui->pbBinStartMotion);
        pBinRow->addWidget(ui->pbBinStopMotion);
        pBinRow->addStretch(1);
        // 跨满原有三列放在 row 0：row 0~2 上只剩隐藏的标签/编辑器, 列宽会塌成 0,
        // 这一行实际就是整张卡的内容行（原 row 3/4 的占位随按钮移走而空掉）。
        ui->gridLayout_7->addLayout(pBinRow, 0, 0, 1, 3);
        const QList<QPushButton *> BinButtons = {ui->pbBinStartMotion, ui->pbBinStopMotion};
        for (QPushButton *pButton : BinButtons)
        {
            pButton->setFixedSize(kBinButtonWidth, kBinButtonHeight);
        }

        // 两张卡里各有一个纵向弹簧(.ui 里 sizeHint 40px)。它们的职责只是"把内容顶到
        // 上边", 但 40px 的建议高度会跟着算进控制卡的自然高度 —— 实测把卡片顶高了
        // 22px(507→529), 多出来的全是空白。这里把建议高度清零, 只留 Expanding。
        ui->verticalSpacer_7->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        ui->verticalSpacer->changeSize(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    }

    // ---- 运动页 11 个方向/动作键: 圆角正方形 + 绿色单色图标（2026-11-24 需求）----
    // "发送运动"/"停止" 是操作按钮，不在此列，保持矩形与文字。
    //
    // 尺寸只能写在代码里：.ui 里逐个设 minimumSize+maximumSize 要写 22 个属性块，
    // 而且光有尺寸还不够 —— 这三个网格列原本靠"缺省平均分配富余宽度"把按钮拉成
    // 宽矩形，必须同时把富余宽度让给第 3 列（horizontalSpacer_9 所在列），
    // 否则按钮固定成方块后会被居中留缝，看着反而更散。
    // 圆角与内边距在 style/app.qss 的 QPushButton#pbUp ... 规则里（样式归样式表）。
    {
        // .ui 里的中文标题保留着当"素材说明"(Designer 里还看得懂),
        // 运行时清空换成图标 + tooltip —— 改图标不必进 Designer 重新摆控件。
        const QList<QPair<QPushButton *, QString>> lstIconKeys = {
            {ui->pbUp,        QStringLiteral("up")},
            {ui->pbDown,      QStringLiteral("down")},
            {ui->pbLeft,      QStringLiteral("left")},
            {ui->pbRight,     QStringLiteral("right")},
            {ui->pbGather,    QStringLiteral("gather")},
            {ui->pbHCentered, QStringLiteral("hcentered")},
            {ui->pbVCentered, QStringLiteral("vcentered")},
            {ui->pbDisperse,  QStringLiteral("disperse")},
            {ui->pbG1,        QStringLiteral("g1")},
            {ui->pbG2,        QStringLiteral("g2")},
            {ui->pbFlip,      QStringLiteral("flip")}};

        for (const QPair<QPushButton *, QString> &sPair : lstIconKeys)
        {
            QPushButton *pKey = sPair.first;

            // 64×64 方块 + 32px 图标: 图标占一半, 车间里 1m 外也认得出;
            // 64 是"卡片不被顶高、整页不出滚动条"的上限(试过 68, 出滚动条)。
            pKey->setFixedSize(64, 64);
            pKey->setIcon(MakeMotionIcon(sPair.second));
            pKey->setIconSize(QSize(36, 36));

            // 文字转 tooltip: 必须先取 text 再清空
            pKey->setToolTip(pKey->text());
            pKey->setAccessibleName(pKey->text());
            pKey->setText(QString());
        }
        // "发送运动/停止" 不是方块, 但固定成 56 的方块后, 网格会把富余宽度
        // 全塞给它们所在列, 两个按钮会被拉成通栏长条(2026-11-24 实测)。
        // 定宽 + 第 3 列吃掉余量, 恢复"键区在左、操作键在右"的版式。
        //
        // 高度必须显式给 64: QPushButton 的垂直 sizePolicy 是 Fixed, 不设高度时
        // 取 sizeHint(25px), 在 64 高的行里居中成一条"小扁条", 和方块键一比高低不齐
        // (2026-11-24 用户反馈)。两键分别落在 row3/row4, 行高本来就是 64, 对齐后等高。
        ui->pbSendMotion->setFixedSize(104, 64);
        ui->pbSendMotionStop->setFixedSize(104, 64);
        ui->gridLayout_3->setColumnStretch(3, 1);
    }

    ui->dialAngleRegulation->setTracking(true);

    // 组合运动表单: 把 groupmotion 分别嵌进 G1Widget / G2Widget 两个容器
    // (容器是 .ui 里的空 QWidget, 这里给它套一层无边距布局)
    m_pGroupMotion1 = new GroupMotion(ui->G1Widget);
    m_pGroupMotion2 = new GroupMotion(ui->G2Widget);

    QVBoxLayout *pLayoutG1 = new QVBoxLayout(ui->G1Widget);
    pLayoutG1->setContentsMargins(0, 0, 0, 0);
    pLayoutG1->addWidget(m_pGroupMotion1);

    QVBoxLayout *pLayoutG2 = new QVBoxLayout(ui->G2Widget);
    pLayoutG2->setContentsMargins(0, 0, 0, 0);
    pLayoutG2->addWidget(m_pGroupMotion2);

//    ui->pbG1->hide();
//    ui->pbG2->hide();
//    ui->pbFlip->hide();
    ui->hsldSpeed->hide();
    ui->lbSpeed->hide();

    // 按钮分组
    m_pMotionGroup = new QButtonGroup();
    m_pMotionGroup->setExclusive(true);

    m_pProtocol = &GeneralProtocol::getInstance();


    // 按钮集合
    m_lstMotionButtons =
    {
        {ui->pbUp, "MotionUp", GeneralMotion::MotionParams::MotionIndex::MotionUp, {}},
        {ui->pbDown, "MotionDown", GeneralMotion::MotionParams::MotionIndex::MotionDown, {}},
        {ui->pbLeft, "MotionLeft", GeneralMotion::MotionParams::MotionIndex::MotionLeft, {}},
        {ui->pbRight, "MotionRight", GeneralMotion::MotionParams::MotionIndex::MotionRight, {}},
        {ui->pbGather, "MotionGather", GeneralMotion::MotionParams::MotionIndex::MotionGather, {}},
        {ui->pbHCentered, "MotionHCentered", GeneralMotion::MotionParams::MotionIndex::MotionHCentered, {}},
        {ui->pbVCentered, "MotionVCentered", GeneralMotion::MotionParams::MotionIndex::MotionVCentered, {}},
        {ui->pbDisperse, "MotionDisperse", GeneralMotion::MotionParams::MotionIndex::MotionDisperse, {}},
        {ui->pbFlip, "MotionFlip", GeneralMotion::MotionParams::MotionIndex::MotionFlip, {}},   
    };

    //ui->leAngle->setValidator(new QIntValidator(-90, 90, this));

    GetSettingParams("");

    // 将按钮加入到button group中
    for (ButtonParams sBtn : m_lstMotionButtons)
    {
        sBtn.pButton->setCheckable(true);
        m_pMotionGroup->addButton(sBtn.pButton);
    }

    // 连接按钮事件
    connect(m_pMotionGroup, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), this, &ControlPanel::OnMotionGroupButtonClicked);
    connect(ui->dspbVoltage, SIGNAL(valueChanged(double)), this, SLOT(on_dspbVoltage_valueChanged(double)));
    connect(ui->dspbFrequency, SIGNAL(valueChanged(double)), this, SLOT(on_dspbFrequency_valueChanged(double)));
    connect(ui->dspbTime, SIGNAL(valueChanged(double)), this, SLOT(on_dspbTime_valueChanged(double)));
    // "保持"复选框: 与电压/频率/时间一样, 改的是"当前选中按钮"的那份参数
    connect(ui->cbMotionKeep, SIGNAL(toggled(bool)), this, SLOT(on_cbMotionKeep_toggled(bool)));
    // 注意: 因上面 setupUi(parent) 的缘故, 这里的 connect 不能省 ——
    // "转到槽"生成的 on_xxx_clicked 不会自动生效, 新增按钮必须在此补一行
    connect(ui->pbSendMotion, SIGNAL(clicked()), this, SLOT(on_pbSendMotion_clicked()));
    connect(ui->pbSendMotionStop, SIGNAL(clicked()), this, SLOT(on_pbSendMotionStop_clicked()));

    // 组合运动 G1 / G2(同样必须手动 connect)
    connect(ui->pbG1, SIGNAL(clicked()), this, SLOT(on_pbG1_clicked()));
    connect(ui->pbG2, SIGNAL(clicked()), this, SLOT(on_pbG2_clicked()));

    connect(ui->dialAngleRegulation, &QDial::valueChanged, this, &ControlPanel::on_dialAngleRegulation_valueChanged);
    connect(ui->hsldSpeed, &QDial::valueChanged, this, &ControlPanel::on_hsldSpeed_valueChanged);

    // ModifyCoeff 两个滑块: 同样必须手动 connect(见 setupUi(parent) 的说明),
    // 值在下发时直接读取, 这里连接是为了将来加"实时调节/即时存盘"时有落点
    connect(ui->hsVModify, &QSlider::valueChanged, this, &ControlPanel::on_hsVModify_valueChanged);
    connect(ui->vsHModify, &QSlider::valueChanged, this, &ControlPanel::on_vsHModify_valueChanged);

    // 料仓
    connect(ui->pbBinStartMotion, SIGNAL(clicked()), this, SLOT(on_pbBinStartMotion_clicked()));
    connect(ui->pbBinStopMotion, SIGNAL(clicked()), this, SLOT(on_pbBinStopMotion_clicked()));

    // 光源(注意 pbCloseLihgtB 的 Lihgt 是 ui 文件里的历史拼写)
    connect(ui->pbOpenLightA, SIGNAL(clicked()), this, SLOT(on_pbOpenLightA_clicked()));
    connect(ui->pbCloseLightA, SIGNAL(clicked()), this, SLOT(on_pbCloseLightA_clicked()));
    connect(ui->pbOpenLightB, SIGNAL(clicked()), this, SLOT(on_pbOpenLightB_clicked()));
    connect(ui->pbCloseLihgtB, SIGNAL(clicked()), this, SLOT(on_pbCloseLihgtB_clicked()));
    connect(ui->pbOpenLightC, &QPushButton::clicked, this, &ControlPanel::on_pbOpenLightC_clicked);
    connect(ui->pbCloseLightC, &QPushButton::clicked, this, &ControlPanel::on_pbCloseLightC_clicked);
    connect(ui->pbOpenLightD, &QPushButton::clicked, this, &ControlPanel::on_pbOpenLightD_clicked);
    connect(ui->pbCloseLightD, &QPushButton::clicked, this, &ControlPanel::on_pbCloseLightD_clicked);

    // 交替居中序列的三个时间参数(模式数据面板, 单位: 秒):
    // 默认值与此前固化的 0.8s / 0.8s / 0.1s 行为一致
    ui->dspHMotionTime->setValue(0.8);
    ui->dspVMotionTime->setValue(0.8);
    ui->dspIntervalTime->setValue(0.1);

    // 时间上限统一 9999s(2026-11-25 协议时间域 2→4 字节, 秒×10 最大 99990)。
    // 范围写在代码里、不写 .ui: 用户的 Designer 重存 .ui 会把它们改回 5000 / 99.99。
    // 小数位同时收到 1 位 —— 协议只能表达 0.1s, 留 2 位小数会让用户以为 0.85 生效,
    // 实际组帧时按 0.1s 刻度四舍五入成 0.9。
    // dspbTime = 单个运动键的"动作时长"(存进按钮参数的 timeuse), 与料仓、光源同属时间参数
    for (QDoubleSpinBox *pTime : {ui->dspbTime, ui->dspHMotionTime, ui->dspVMotionTime,
                                  ui->dspIntervalTime, ui->dspbBinTime})
    {
        pTime->setDecimals(1);
        pTime->setRange(0.0, kMaxTimeUse);
        pTime->setSingleStep(0.1);
    }
    for (QSpinBox *pTime : {ui->spLightATime, ui->spLightBTime, ui->spLightCTime, ui->spLightDTime})
    {
        pTime->setRange(0, static_cast<int>(kMaxTimeUse));
    }

    // pbGather 交替序列的兜底: 若中途某段响应超时(引擎清空队列, 剩余段不会再有完成帧),
    // 计数会卡住, 界面按钮永远不恢复 —— 到点强制复位
    m_pMotionSeqWatchdog = new QTimer(this);
    m_pMotionSeqWatchdog->setSingleShot(true);
    connect(m_pMotionSeqWatchdog, &QTimer::timeout, this, [this]() {
        if (m_pSequence->u32Remaining > 0)
        {
            qWarning() << "motion sequence watchdog timeout, force reset";
            ResetMotionSequence();
            RestoreIdleButtonsBoth();
        }
    });

    // 光源 A~D 的"到点自动恢复": 每路一个单次定时器, 互不影响(四路时间可以不同)
    for (int i32Light = 0; i32Light < 4; ++i32Light)
    {
        m_apLightRecoverTimers[i32Light] = new QTimer(this);
        m_apLightRecoverTimers[i32Light]->setSingleShot(true);
        connect(m_apLightRecoverTimers[i32Light], &QTimer::timeout, this,
                [this, i32Light]() { RecoverLightChannel(i32Light); });
    }

    connect(this, &ControlPanel::sigMotionTerminate, this, [this]() {
        // 未连接(不允许下发)的面板: 按钮本来就该一直灰着, 不能被完成帧"恢复"成可点
        if (!m_bRunEnabled)
        {
            return;
        }

        // 序列在对面面板手上: 这次完成帧由它处理(递减/续队/收尾), 它收尾时会把
        // 两个面板一起恢复。这里必须直接返回 —— 否则同一次完成帧会被两边各减一次,
        // 计数提前归零, 电机还在跑的时候按钮就解锁了。
        if (m_pSequence->pOwner != nullptr && m_pSequence->pOwner != this)
        {
            return;
        }

        // pbGather 交替序列: 每条命令(运动段或间隔段)完成触发一次本信号,
        // 中间的命令不恢复按钮, 最后一条才恢复
        if (m_pSequence->u32Remaining > 0)
        {
            m_pSequence->u32Remaining--;

            // 「保持」: 队列见底(只剩 1 条在跑)就把同一个 H+V 周期再排一遍,
            // 计数因此永不归零 → 界面一直停在运动中(停止键全程可用), 直到
            // 用户点"结束运动"/断连/看门狗超时。续排放在这里而不是等归零:
            // 队列里始终有后续命令, 引擎下一个 tick 就能接上, 不会多出一次空档。
            if (m_pSequence->bRepeat && m_pSequence->u32Remaining < 2)
            {
                const quint32 u32Added = QueueGatherRound();

                if (u32Added > 0)
                {
                    // 续排的命令同样要计入"未完成数", 否则下一帧就会误判成"队列已空"
                    // 再续一轮 —— 计数与真实队列脱节, 按钮也会提前解锁。
                    m_pSequence->u32Remaining += u32Added;
                    StartMotionSeqWatchdog(m_pSequence->iRoundMs, u32Added);
                    return;
                }

                // 按钮参数丢了(理论上不会发生): 退出保持, 按下方的正常收尾恢复界面
                qWarning() << "gather keep: cannot queue next round, stop repeating";
                m_pSequence->bRepeat = false;
            }

            if (m_pSequence->u32Remaining > 0)
            {
                return;
            }

            ResetMotionSequence();   // 计数归零, 序列正常结束, 落入下方统一恢复
        }

        // 统一恢复: 振动盘(含全部运动类型按钮) / 料仓 / 光源 / 组合运动按钮回到空闲态,
        // 两个面板一起 —— 序列计数是共享的, 按钮态也必须同步
        RestoreIdleButtonsBoth();
    });

    ui->pbSendMotionStop->setEnabled(false);

    // 固件参数页(0x15/0x16/0x17/0x18): 12 个编辑器统一成 4 位小数。
    // 这些相位参数的实际取值是 1.375 / 0.4375 这种 1/16 的整数倍, 而 Qt 默认 2 位小数。
    // 2 位: 1.375 -> 1.38, 3 位: 0.4375 -> 0.438。显示时被四舍五入, 用户再点"保存配置"
    // 就把舍入后的值写回 Flash —— 参数被界面悄悄改了, 且还会污染下一次读取的结果。
    // 取 4 位: 0.4375 在十进制下是精确的, 同时在二进制里也是精确值(7/16), 读写都不会漂。
    for (QDoubleSpinBox *pEditor : KeyParamsEditors())
    {
        pEditor->setDecimals(4);
        pEditor->setRange(0.0, 100.0);
        pEditor->setSingleStep(0.05);
    }

    // 同上: setupUi(parent) 让自动连接失效, 必须手动 connect
    connect(ui->pbGetKeyParams, SIGNAL(clicked()), this, SLOT(on_pbGetKeyParams_clicked()));
    connect(ui->pbSaveKeyParams, SIGNAL(clicked()), this, SLOT(on_pbSaveKeyParams_clicked()));
    // 未连接时不可点(连接成功由 ControlWidget 调 SetKeyParamsAvailable(true) 放开)
    UpdateKeyParamsButtons();

    // 料仓与光源按钮初始为空闲态
    ui->pbBinStopMotion->setEnabled(false);
    ui->pbCloseLightA->setEnabled(false);
    ui->pbCloseLihgtB->setEnabled(false);
    ui->pbCloseLightC->setEnabled(false);
    ui->pbCloseLightD->setEnabled(false);
}

ControlPanel::~ControlPanel()
{
    delete ui;
}


// 是否有动作正在执行:
//  1) 运动进行中的界面锁(单条运动 / G1 / G2 / 交替居中序列都置位);
//  2) 序列(交替居中 / 组合运动)未跑完 —— 计数 > 0;
//  3) 料仓运动 —— 对应"停止"按钮处于可用态即说明正在动
//     (空闲时两个停止键都是禁用的)
bool ControlPanel::IsMotionRunning(void) const
{
    return m_bMotionUiLocked ||
           (m_pSequence->u32Remaining > 0) ||
           ui->pbSendMotionStop->isEnabled() ||
           ui->pbBinStopMotion->isEnabled();
}


bool ControlPanel::IsBinRunning(void) const
{
    return ui->pbBinStopMotion->isEnabled();
}


// 只读 = 不允许下发。2026-11-25 二十次起,"配置数据"窗口里的面板不再是永久只读:
// 它同样要能发运动/料仓/光源(用户要"配完直接发一条看效果"), 只是初始(未连接)不可发。
void ControlPanel::SetReadOnly(const bool bReadOnly)
{
    m_bMotionUiLocked = false;

    SetRunEnabled(!bReadOnly);
}


// 允许/禁止本面板下发运行指令。
//   bEnabled = true : 一律回到空闲态 —— 发送键可点、停止键灰、运动类型键全开、
//                     料仓"可开/不可停"、四路光源"可开/不可关"、状态灯全灰。
//   bEnabled = false: 14 个运行键全部禁用(未连接时点了也不会有应答, 必须点不动,
//                     否则按钮会停在"发送已灰/停止可点"的半截态里永远回不来)。
// 运动类型按钮不受影响: 配置页要随时能点选, 才能切换正在编辑的那份参数。
void ControlPanel::SetRunEnabled(const bool bEnabled)
{
    m_bRunEnabled = bEnabled;

    if (bEnabled)
    {
        // 进来时序列/看门狗可能是上一轮留下的(断连时没清干净), 先归零
        ResetMotionSequence();
        RestoreIdleButtons();
        return;
    }

    ResetMotionSequence();
    m_bMotionUiLocked = false;

    const QList<QPushButton *> lstRunKeys = {
        ui->pbSendMotion, ui->pbSendMotionStop, ui->pbG1, ui->pbG2,
        ui->pbBinStartMotion, ui->pbBinStopMotion,
        ui->pbOpenLightA, ui->pbCloseLightA,
        ui->pbOpenLightB, ui->pbCloseLihgtB,
        ui->pbOpenLightC, ui->pbCloseLightC,
        ui->pbOpenLightD, ui->pbCloseLightD};
    for (QPushButton *pButton : lstRunKeys)
    {
        pButton->setEnabled(false);
    }

    // 光源状态灯跟着回灰(运行页才有灯, 配置面板的列表是空的, SetLightLamp 内部判空)
    StopAllLightRecover();
    SetAllLightLampsOff();
    NotifyRunState();
}


// 两个面板之间抄"运行类"按钮态(见头文件说明)
void ControlPanel::SyncRunButtonStates(const ControlPanel *pSource)
{
    if (pSource == nullptr)
    {
        return;
    }

    const QList<QPair<QPushButton *, QPushButton *>> lstPairs = {
        {ui->pbSendMotion, pSource->ui->pbSendMotion},
        {ui->pbSendMotionStop, pSource->ui->pbSendMotionStop},
        {ui->pbBinStartMotion, pSource->ui->pbBinStartMotion},
        {ui->pbBinStopMotion, pSource->ui->pbBinStopMotion},
        {ui->pbOpenLightA, pSource->ui->pbOpenLightA},
        {ui->pbCloseLightA, pSource->ui->pbCloseLightA},
        {ui->pbOpenLightB, pSource->ui->pbOpenLightB},
        {ui->pbCloseLihgtB, pSource->ui->pbCloseLihgtB},
        {ui->pbOpenLightC, pSource->ui->pbOpenLightC},
        {ui->pbCloseLightC, pSource->ui->pbCloseLightC},
        {ui->pbOpenLightD, pSource->ui->pbOpenLightD},
        {ui->pbCloseLightD, pSource->ui->pbCloseLightD}};

    // 注意要与"本面板能不能下发"取与: 未连接时抄进来的只会是"可点",
    // 直接照抄会把一个未连接的面板抄成可点的(点了发不出去, 按钮还会卡在锁定态)。
    // 所以未连接时这里保持全灰, 连接成功后再抄一次才生效。
    for (const QPair<QPushButton *, QPushButton *> &sPair : lstPairs)
    {
        sPair.first->setEnabled(m_bRunEnabled && sPair.second->isEnabled());
    }

    // 序列状态不在这里碰: 二十五次起两个面板共享同一份(MotionSequenceState),
    // 抄按钮态时去清它等于把对方正在跑的序列掐掉 —— 而序列的收尾本来就由
    // owner 面板负责, 收尾时会用 RestoreIdleButtonsBoth 把两边一起恢复。
    // (单面板使用即未共享时, 这份状态就是自己的, 也不该在抄态时被清)

    // 光源"时间模式"的到点恢复跟着接力。只抄按钮态的话, 在"配置数据"窗口里
    // 点开灯、还没到点就关窗, 运行页会卡成"只能点关闭" —— 正是本轮要消灭的误导。
    // 源面板哪一路还在倒计时就把剩余时间接过来; 保持 / 时间 0(常亮) 那几路
    // 没有倒计时(返回 0), 本面板也保持"只能手动关"。
    QPushButton *arrCloseLights[4] = {ui->pbCloseLightA, ui->pbCloseLihgtB,
                                      ui->pbCloseLightC, ui->pbCloseLightD};
    for (int i32Light = 0; i32Light < 4; ++i32Light)
    {
        const int iRemainMs = pSource->LightRecoverRemainMs(i32Light);
        if (iRemainMs > 0 && arrCloseLights[i32Light]->isEnabled())
        {
            StartLightRecoverMs(i32Light, iRemainMs);
        }
        else
        {
            StopLightRecover(i32Light);
        }
    }
}


// 全部运动类型按钮(上/下/左/右/聚拢/水平居中/垂直居中/分散/翻面)统一开关。
// 不碰 QButtonGroup 的选中项: 禁用不会清掉 checked, 解锁后 m_qstrCurrentMotionButton
// 与界面选中态仍然一致(参数控件显示的还是那个按钮的参数)
void ControlPanel::SetMotionTypeButtonsEnabled(const bool bEnabled)
{
    for (const ButtonParams &Button : m_lstMotionButtons)
    {
        Button.pButton->setEnabled(bEnabled);
    }
}


// 下发运动后上锁: 发送键置灰 / 停止键可用 / G1、G2 与全部运动类型按钮置灰。
// 解锁只有三条路(都由 RestoreIdleButtons 完成):
//   1) 下位机完成帧 → sigMotionTerminate
//   2) 运动应答超时  → 引擎清队后仍发 sigMotionTerminate(ConnectiveHandle.cpp OnEngineTick)
//   3) 点了"停止运动" → 引擎清队时发 sigMotionTerminate
void ControlPanel::LockMotionUi(void)
{
    if (!m_bRunEnabled)
    {
        return;
    }

    m_bMotionUiLocked = true;

    ui->pbSendMotion->setEnabled(false);
    ui->pbSendMotionStop->setEnabled(true);
    // 组合运动与当前运动互斥: 序列计数共用一套, 同时进去会互相吞完成帧
    ui->pbG1->setEnabled(false);
    ui->pbG2->setEnabled(false);
    SetMotionTypeButtonsEnabled(false);

    // 状态灯: 按下发后的真实界面态刷新(此时停止键已可用 → 运动灯亮)
    NotifyRunState();
}


// 唯一的统一恢复点: 振动盘(含运动类型按钮) / 料仓 / 光源 / 组合运动按钮全部回到空闲态
void ControlPanel::RestoreIdleButtons(void)
{
    // 不允许下发的面板(未连接): 按钮保持全灰, 这里不能把它"恢复"成可点
    if (!m_bRunEnabled)
    {
        return;
    }

    m_bMotionUiLocked = false;
    SetMotionTypeButtonsEnabled(true);

    // 兜底: 极少数情况下(如 exclusive 组内的按钮在禁用期间被 setChecked 过)选中态可能丢失,
    // 解锁后按 m_qstrCurrentMotionButton 补一次, 否则"发送运动"会因没有当前按钮而无反应
    if (m_pMotionGroup != nullptr && m_pMotionGroup->checkedButton() == nullptr)
    {
        for (const ButtonParams &Button : m_lstMotionButtons)
        {
            if (Button.qstrName == m_qstrCurrentMotionButton)
            {
                Button.pButton->setChecked(true);
                break;
            }
        }
    }

    ui->pbSendMotion->setEnabled(true);
    ui->pbSendMotionStop->setEnabled(false);
    ui->pbG1->setEnabled(true);
    ui->pbG2->setEnabled(true);
    ui->pbBinStartMotion->setEnabled(true);
    ui->pbBinStopMotion->setEnabled(false);
    ui->pbOpenLightA->setEnabled(true);
    ui->pbCloseLightA->setEnabled(false);
    ui->pbOpenLightB->setEnabled(true);
    ui->pbCloseLihgtB->setEnabled(false);
    ui->pbOpenLightC->setEnabled(true);
    ui->pbCloseLightC->setEnabled(false);
    ui->pbOpenLightD->setEnabled(true);
    ui->pbCloseLightD->setEnabled(false);
    // 光源按钮回到"可开、不可关"就等于四路都灭了, 状态灯必须跟着回灰
    StopAllLightRecover();
    SetAllLightLampsOff();

    // 状态灯: 这是唯一的空闲恢复点, 运动/料仓两盏灯都在这里回灰
    NotifyRunState();
}


// 序列收尾: 两个面板一起回空闲。
// 非 owner 面板收到完成帧时是直接返回的(见 sigMotionTerminate 的处理), 它自己
// 永远走不到这里; 若不带着它一起恢复, 它会一直停在"运动中"(发送灰、停止可用),
// 用户切回主界面就点不动了 —— 聚拢"保持"下尤其明显。
void ControlPanel::RestoreIdleButtonsBoth(void)
{
    RestoreIdleButtons();

    if (m_pPeerPanel != nullptr && m_pPeerPanel != this)
    {
        m_pPeerPanel->RestoreIdleButtons();
    }
}


// 右下角"状态"卡片的两盏运行灯(运动 / 料仓)。
// 状态直接读按钮的使能态, 与界面显示同源 —— 不另存标志位:
// 标志位和按钮是同一件事的两份拷贝, 断连/超时/停止路径上漏掉一处就会出现
// "灯还绿着但按钮已经回到空闲"的假状态, 而按钮态本身就是用户看到的真相。
//   运动: 界面锁(单条/G1/G2/交替序列) 或 序列计数>0 或 "停止"键可用
//   料仓: 料仓"停止"键可用(空闲时料仓是"可开 / 不可停")
void ControlPanel::NotifyRunState(void)
{
    // 这里不再按"是否允许下发"早退: 断连的那一刻按钮已被全部禁用,
    // 算出来就是"两盏灯都灰", 正是需要发出去的那一帧状态。
    // 早退会让断连后灯停在绿色("灯还绿着, 但按钮已经点不动了")。
    const bool bMotionRunning = m_bMotionUiLocked ||
                                (m_pSequence->u32Remaining > 0) ||
                                ui->pbSendMotionStop->isEnabled();
    emit sigMotionRunningChanged(bMotionRunning);
    emit sigBinRunningChanged(ui->pbBinStopMotion->isEnabled());
}


// 断连/重连: 链路没了, 在飞的指令不会再有完成帧, 界面必须无条件回空闲态。
// 不清通信队列(队列随连接销毁, 由 ControlWidget::CloseConnection 负责),
// 只复位计数 + 解锁按钮
void ControlPanel::ResetMotionUiState(void)
{
    if (!m_bRunEnabled)
    {
        return;
    }

    ResetMotionSequence();
    RestoreIdleButtons();
}


// ============================ 固件参数页(0x15~0x18) ============================

// 12 个编辑器, 顺序必须与协议 0x16/0x17 的数据域以及下位机 KeyParams 结构体一致:
// 上 / 下 / 左 / 右 / 水平居中 / 垂直居中, 每组"起始相位超前"在前、"相位偏移"在后
QList<QDoubleSpinBox *> ControlPanel::KeyParamsEditors(void) const
{
    return
    {
        ui->dspUpStartPhaseLeading,      ui->dspUpPhaseOffset,
        ui->dspDownStartPhaseLeading,    ui->dspDownPhaseOffset,
        ui->dspLeftStartPhaseLeading,    ui->dspLeftPhaseOffset,
        ui->dspRightStartPhaseLeading,   ui->dspRightPhaseOffset,
        ui->dspHGatherStartPhaseLeading, ui->dspHGatherPhaseOffset,
        ui->dspVGatherStartPhaseLeading, ui->dspVGatherPhaseOffset,
    };
}


GeneralKeyParams::KeyParams ControlPanel::CollectKeyParams(void) const
{
    GeneralKeyParams::KeyParams sParams;

    sParams.fUpStartPhaseLeading      = static_cast<float>(ui->dspUpStartPhaseLeading->value());
    sParams.fUpPhaseOffset            = static_cast<float>(ui->dspUpPhaseOffset->value());
    sParams.fDownStartPhaseLeading    = static_cast<float>(ui->dspDownStartPhaseLeading->value());
    sParams.fDownPhaseOffset          = static_cast<float>(ui->dspDownPhaseOffset->value());
    sParams.fLeftStartPhaseLeading    = static_cast<float>(ui->dspLeftStartPhaseLeading->value());
    sParams.fLeftPhaseOffset          = static_cast<float>(ui->dspLeftPhaseOffset->value());
    sParams.fRightStartPhaseLeading   = static_cast<float>(ui->dspRightStartPhaseLeading->value());
    sParams.fRightPhaseOffset         = static_cast<float>(ui->dspRightPhaseOffset->value());
    sParams.fHGatherStartPhaseLeading = static_cast<float>(ui->dspHGatherStartPhaseLeading->value());
    sParams.fHGatherPhaseOffset       = static_cast<float>(ui->dspHGatherPhaseOffset->value());
    sParams.fVGatherStartPhaseLeading = static_cast<float>(ui->dspVGatherStartPhaseLeading->value());
    sParams.fVGatherPhaseOffset       = static_cast<float>(ui->dspVGatherPhaseOffset->value());

    return sParams;
}


void ControlPanel::ApplyKeyParams(const GeneralKeyParams::KeyParams &sParams)
{
    ui->dspUpStartPhaseLeading->setValue(static_cast<double>(sParams.fUpStartPhaseLeading));
    ui->dspUpPhaseOffset->setValue(static_cast<double>(sParams.fUpPhaseOffset));
    ui->dspDownStartPhaseLeading->setValue(static_cast<double>(sParams.fDownStartPhaseLeading));
    ui->dspDownPhaseOffset->setValue(static_cast<double>(sParams.fDownPhaseOffset));
    ui->dspLeftStartPhaseLeading->setValue(static_cast<double>(sParams.fLeftStartPhaseLeading));
    ui->dspLeftPhaseOffset->setValue(static_cast<double>(sParams.fLeftPhaseOffset));
    ui->dspRightStartPhaseLeading->setValue(static_cast<double>(sParams.fRightStartPhaseLeading));
    ui->dspRightPhaseOffset->setValue(static_cast<double>(sParams.fRightPhaseOffset));
    ui->dspHGatherStartPhaseLeading->setValue(static_cast<double>(sParams.fHGatherStartPhaseLeading));
    ui->dspHGatherPhaseOffset->setValue(static_cast<double>(sParams.fHGatherPhaseOffset));
    ui->dspVGatherStartPhaseLeading->setValue(static_cast<double>(sParams.fVGatherStartPhaseLeading));
    ui->dspVGatherPhaseOffset->setValue(static_cast<double>(sParams.fVGatherPhaseOffset));
}


// 状态行: 正常用深灰, 失败/超时用红色, 一眼能区分"读到了"和"没读到"
// 这两句是"随数据变化"的颜色, 不能交给 app.qss(样式表里静态颜色无法按状态切换),
// 也是本工程唯一允许保留 setStyleSheet 的地方。色值已对齐主题的语义色。
void ControlPanel::SetKeyParamsStatus(const QString &qstrText, const bool bError)
{
    ui->lbKeyParamsStatus->setText(qstrText);
    ui->lbKeyParamsStatus->setStyleSheet(bError ? QStringLiteral("color: #DC2626;")
                                                : QStringLiteral("color: #374151;"));
}


void ControlPanel::SetKeyParamsButtonsEnabled(const bool bEnabled)
{
    ui->pbGetKeyParams->setEnabled(bEnabled);
    ui->pbSaveKeyParams->setEnabled(bEnabled);
}


// 按钮最终使能 = 已连接 && 没有进行中的事务。
// 两个条件分开存是为了断连/重连时不丢状态(见 SetKeyParamsAvailable)
void ControlPanel::UpdateKeyParamsButtons(void)
{
    SetKeyParamsButtonsEnabled(m_bKeyParamsAvailable && !m_bKeyParamsPending);
}


void ControlPanel::SetKeyParamsAvailable(const bool bAvailable)
{
    m_bKeyParamsAvailable = bAvailable;

    if (!bAvailable)
    {
        // 断连: 等待中的事务作废(应答不会再来), 状态行如实说明
        m_bKeyParamsPending = false;
        SetKeyParamsStatus(QStringLiteral("未连接：连接成功后才能获取/保存固件参数。"), false);
    }

    UpdateKeyParamsButtons();
}


void ControlPanel::ResetKeyParamsUiState(void)
{
    m_bKeyParamsAvailable = false;
    SetKeyParamsAvailable(false);
}


// 获取配置: 发 0x15, 等下位机回 0x16。
// 注意这里不看 m_bRunEnabled —— 固件参数页只在"配置数据"窗口里出现,
// 它发送的是"读写下位机 Flash"的指令, 不是运行指令, 可用性只由"是否已连接"
// (m_bKeyParamsAvailable)决定。
void ControlPanel::on_pbGetKeyParams_clicked()
{
    if (m_bKeyParamsPending || !m_bKeyParamsAvailable)
    {
        return;
    }

    m_bKeyParamsPending = true;
    UpdateKeyParamsButtons();
    SetKeyParamsStatus(QStringLiteral("正在从下位机读取固件参数…"), false);

    emit sigGetKeyParams();
}


// 保存配置: 组 0x17(数据域 49 字节), 等下位机回 0x18(0=成功 1=失败)
void ControlPanel::on_pbSaveKeyParams_clicked()
{
    if (m_bKeyParamsPending || !m_bKeyParamsAvailable)
    {
        return;
    }

    QByteArray qbtData;
    m_pProtocol->SaveKeyParamsProtocol(CollectKeyParams(), qbtData);

    m_bKeyParamsPending = true;
    UpdateKeyParamsButtons();
    SetKeyParamsStatus(QStringLiteral("正在保存到下位机 Flash…"), false);

    emit sigSaveKeyParams(qbtData);
}


void ControlPanel::OnGetKeyParamsResult(const quint8 &u8Status, const QByteArray &qbtData)
{
    // 非本次事务的迟到应答(例如断连前那次)直接丢弃
    if (!m_bKeyParamsPending)
    {
        return;
    }

    m_bKeyParamsPending = false;
    UpdateKeyParamsButtons();

    if (u8Status == 0xFF)
    {
        SetKeyParamsStatus(QStringLiteral("获取超时：下位机 2 秒内没有应答，请检查连接后重试。"), true);
        return;
    }

    GeneralKeyParams::KeyParams sParams;
    if (!GeneralProtocol::KeyParamsFromBytes(qbtData, sParams))
    {
        SetKeyParamsStatus(QStringLiteral("获取失败：下位机应答的数据不足 48 字节，无法解析。"), true);
        return;
    }

    ApplyKeyParams(sParams);

    if (u8Status == GeneralProtocolItems::u8GET_KEY_PARAMS_FROM_FLASH)
    {
        SetKeyParamsStatus(QStringLiteral("获取成功：以上参数读取自下位机 Flash（已保存过的值）。"), false);
    }
    else
    {
        SetKeyParamsStatus(QStringLiteral("下位机 Flash 里没有有效参数：以上是固件默认值，"
                                          "点“保存配置”才会写入 Flash。"), true);
    }
}


void ControlPanel::OnSaveKeyParamsResult(const int &iResult)
{
    if (!m_bKeyParamsPending)
    {
        return;
    }

    m_bKeyParamsPending = false;
    UpdateKeyParamsButtons();

    if (iResult == 1)
    {
        SetKeyParamsStatus(QStringLiteral("保存成功：参数已写入下位机 Flash，回读校验通过。"), false);
    }
    else if (iResult == 0)
    {
        SetKeyParamsStatus(QStringLiteral("保存失败：下位机写 Flash 未通过校验，参数未改变，请重试。"), true);
    }
    else
    {
        SetKeyParamsStatus(QStringLiteral("保存超时：3 秒内没收到下位机的保存结果，无法确认是否写入成功。"), true);
    }
}

// 获取配置参数
void ControlPanel::GetSettingParams(const QString &qstrFileName)
{
    QString qstrSettingFile;
    if (qstrFileName.isEmpty())
    {
        qstrSettingFile = QCoreApplication::applicationDirPath() + "/DefaultConfig.ini";
    }
    else
    {
        qstrSettingFile = qstrFileName;
    }


    if (!QFile::exists(qstrSettingFile))
    {
        qWarning() << "配置文件不存在，使用默认值:" << qstrSettingFile;
        return;
    }

    m_qstrCurrentSettingPath = qstrSettingFile;

    QSettings Settings(qstrSettingFile, QSettings::IniFormat);
    Settings.setIniCodec("UTF-8");

    // 读取一个数值键: 键缺失、或值非法(例如 ini 里被写成空值 @Variant(...))时返回 dDefault。
    // bMustPositive=true 的项(电压/频率)还要求 >0 —— 0V/0Hz 下发后下位机不会动作,
    // 表现为"点了发送没反应", 必须在这一层挡住(否则 SaveSettings 还会把坏值原样写回, 永久失效)
    auto ReadNumber = [&Settings, &qstrSettingFile](const QString &qstrKey,
                                                    const double dDefault,
                                                    const bool bMustPositive) -> double {
        const QVariant vValue = Settings.value(qstrKey);
        bool bOk = false;
        const double dValue = vValue.toDouble(&bOk);

        if (!bOk || (bMustPositive && dValue <= 0.0))
        {
            qWarning() << "配置项无效, 使用默认值:" << qstrSettingFile << qstrKey << vValue << "->" << dDefault;
            return dDefault;
        }

        return dValue;
    };

    for (ButtonParams &Button : m_lstMotionButtons)
    {
        Settings.beginGroup(Button.qstrName);

        Button.qlstMotionParams =
        {
            {
                {"frequency", static_cast<float>(ReadNumber("frequency", 100.0, true))},
                {"timeuse", static_cast<float>(ReadNumber("timeuse", 0.0, false))},
                {"voltage", static_cast<float>(ReadNumber("voltage", 10.0, true))},
                {"angle_offset", static_cast<int>(ReadNumber("angle_offset", 0.0, false))},
                // keep: 1=无限时间动作(默认 0)。老配置没有这个键, 一律按"按时间动作"
                {"keep", (ReadNumber("keep", 0.0, false) > 0.5) ? 1 : 0},
            },
        };

        Settings.endGroup();
    }

    // 料仓参数
    Settings.beginGroup("BinMotion");
    ui->dspbBinVoltage->setValue(Settings.value("voltage", ui->dspbBinVoltage->value()).toDouble());
    ui->dspbBinFrequency->setValue(Settings.value("frequency", ui->dspbBinFrequency->value()).toDouble());
    ui->dspbBinTime->setValue(Settings.value("timeuse", ui->dspbBinTime->value()).toDouble());
    ui->cbBinKeep->setChecked(Settings.value("keep", ui->cbBinKeep->isChecked() ? 1 : 0).toInt() != 0);
    Settings.endGroup();

    // 光源参数
    Settings.beginGroup("LightA");
    ui->spLightABrightness->setValue(Settings.value("brightness", ui->spLightABrightness->value()).toInt());
    ui->spLightATime->setValue(Settings.value("timeuse", ui->spLightATime->value()).toInt());
    ui->cbLightAKeep->setChecked(Settings.value("keep", ui->cbLightAKeep->isChecked() ? 1 : 0).toInt() != 0);
    Settings.endGroup();

    Settings.beginGroup("LightB");
    ui->spLightBBrightness->setValue(Settings.value("brightness", ui->spLightBBrightness->value()).toInt());
    ui->spLightBTime->setValue(Settings.value("timeuse", ui->spLightBTime->value()).toInt());
    ui->cbLightBKeep->setChecked(Settings.value("keep", ui->cbLightBKeep->isChecked() ? 1 : 0).toInt() != 0);
    Settings.endGroup();

    // 老配置缺 C/D 时明确回落为 0, 避免切换模式后残留上一份配置的值。
    Settings.beginGroup("LightC");
    ui->spLightCBrightness->setValue(Settings.value("brightness", 0).toInt());
    ui->spLightCTime->setValue(Settings.value("timeuse", 0).toInt());
    ui->cbLightCKeep->setChecked(Settings.value("keep", 0).toInt() != 0);
    Settings.endGroup();

    Settings.beginGroup("LightD");
    ui->spLightDBrightness->setValue(Settings.value("brightness", 0).toInt());
    ui->spLightDTime->setValue(Settings.value("timeuse", 0).toInt());
    ui->cbLightDKeep->setChecked(Settings.value("keep", 0).toInt() != 0);
    Settings.endGroup();

    // 聚拢(交替居中)序列的三个时间参数: 水平居中段 / 垂直居中段 / 段间间隔(单位秒)
    // 老配置没有这段时保留界面当前值(构造函数里的 0.8 / 0.8 / 0.1), 不被清成 0
    Settings.beginGroup("GatherSequence");
    if (Settings.contains("hmotion_time"))
    {
        ui->dspHMotionTime->setValue(Settings.value("hmotion_time").toDouble());
    }
    if (Settings.contains("vmotion_time"))
    {
        ui->dspVMotionTime->setValue(Settings.value("vmotion_time").toDouble());
    }
    if (Settings.contains("interval_time"))
    {
        ui->dspIntervalTime->setValue(Settings.value("interval_time").toDouble());
    }
    Settings.endGroup();

    // ModifyCoeff 位置修正系数(两个全局滑块, 只有水平/垂直居中会用到)
    // 与 GatherSequence 一样: 老配置没这段就保留界面当前的 0, 不去清别的值
    Settings.beginGroup("ModifyCoeff");
    if (Settings.contains("hcentered"))
    {
        ui->vsHModify->setValue(Settings.value("hcentered").toInt());
    }
    if (Settings.contains("vcentered"))
    {
        ui->hsVModify->setValue(Settings.value("vcentered").toInt());
    }
    Settings.endGroup();

    // 组合运动 G1/G2: 8 步的动作类型 + 运行时间
    auto LoadGroupMotion = [&Settings](const QString &qstrGroup, GroupMotion *pGroup) {
        // 整个组都不存在 = 老配置, 保持界面默认(每步 0.5s), 不要清零
        if (pGroup == nullptr || !Settings.contains(qstrGroup + "/s1_motion"))
        {
            return;
        }

        for (int i = 0; i < 8; i++)
        {
            const QString qstrKey = qstrGroup + QString("/s%1_").arg(i + 1);

            // 缺键时用 127(不存在的动作类型): SetStep 反查失败会保留原选项。
            // 不能用 -1 —— 下拉框新增的"停止"项就用 -1 作 itemData, 会把该步当成终止标记
            const int iMotion = Settings.value(qstrKey + "motion", 127).toInt();
            const double dTime = Settings.value(qstrKey + "time", 0.0).toDouble();

            // 动作类型非法时 SetStep 内部保留原选项, 时间照常恢复
            pGroup->SetStep(i, iMotion, static_cast<float>(dTime));
        }
    };

    LoadGroupMotion("GroupMotion1", m_pGroupMotion1);
    LoadGroupMotion("GroupMotion2", m_pGroupMotion2);
}


void ControlPanel::SetDialDirection(const GeneralMotion::MotionParams::MotionIndex eIndex, const int16_t s16Angle)
{
    int16_t s16OriginDirection = 0;

    switch (eIndex)
    {
        case GeneralMotion::MotionParams::MotionIndex::MotionUp:
            s16OriginDirection = 180;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionDown:
            s16OriginDirection = 0;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionLeft:
            s16OriginDirection = 270;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionRight:
            s16OriginDirection = 90;
            break;

        default:
            break;
    }

    if ((s16OriginDirection + s16Angle) >= 0)
    {
        ui->dialAngleRegulation->setValue(s16OriginDirection + s16Angle);
    }
    else
    {
        ui->dialAngleRegulation->setValue(s16OriginDirection + s16Angle + 360);
    }
}


// 处理不同按钮click时的事务
void ControlPanel::OnMotionGroupButtonClicked(QAbstractButton *pButton)
{
    for (const ButtonParams &Button : m_lstMotionButtons)
    {
        if (pButton == Button.pButton)
        {
            // 参数表为空说明配置文件缺失(GetSettingParams 找不到 ini 会提前返回),
            // 此时 .first() 会直接崩 —— 宁可这一下不响应, 也不能点一下就闪退
            if (Button.qlstMotionParams.isEmpty())
            {
                continue;
            }

            m_qstrCurrentMotionButton = Button.qstrName;

            // 将当前按钮的运动参数更新到界面上
            ui->dspbVoltage->setValue(Button.qlstMotionParams.first().value("voltage").toFloat());
            ui->dspbFrequency->setValue(Button.qlstMotionParams.first().value("frequency").toFloat());
            ui->dspbTime->setValue(Button.qlstMotionParams.first().value("timeuse").toFloat());
            // "保持"也是按钮级参数(同一套共享控件编辑不同按钮), 切换/刷新时必须跟着换
            ui->cbMotionKeep->setChecked(Button.qlstMotionParams.first().value("keep", 0).toInt() != 0);

            ui->leAngle->setText(QString::number(Button.qlstMotionParams.first().value("angle_offset").toInt()));
            SetDialDirection(Button.eMotion, Button.qlstMotionParams.first().value("angle_offset").toInt());
        }
    }
}


void ControlPanel::OnMotionFinished(void)
{
    ui->pbSendMotion->setEnabled(true);

}


void ControlPanel::on_dspbVoltage_valueChanged(double arg1)
{
    // 更新当前的电压显示
    for (ButtonParams &Button : m_lstMotionButtons)
    {
        if (m_qstrCurrentMotionButton == Button.qstrName && !Button.qlstMotionParams.isEmpty())
        {
            Button.qlstMotionParams.first()["voltage"] = arg1;
        }
    }
}

void ControlPanel::on_dspbFrequency_valueChanged(double arg1)
{
    // 更新当前的频率显示
    for (ButtonParams &Button : m_lstMotionButtons)
    {
        if (m_qstrCurrentMotionButton == Button.qstrName && !Button.qlstMotionParams.isEmpty())
        {
            Button.qlstMotionParams.first()["frequency"] = arg1;
        }
    }
}

void ControlPanel::on_dspbTime_valueChanged(double arg1)
{
    // 更新当前的时间显示
    for (ButtonParams &Button : m_lstMotionButtons)
    {
        if (m_qstrCurrentMotionButton == Button.qstrName && !Button.qlstMotionParams.isEmpty())
        {
            Button.qlstMotionParams.first()["timeuse"] = arg1;
        }
    }
}


// "保持"复选框: 勾选 = 下发"无限时间动作"(数据域末尾标志 1), 一直动作到点停止。
// 与电压/频率/时间同一套机制: 写进"当前选中按钮"的那份参数里(不勾则不选中任何按钮时无效)。
void ControlPanel::on_cbMotionKeep_toggled(bool bChecked)
{
    for (ButtonParams &Button : m_lstMotionButtons)
    {
        if (m_qstrCurrentMotionButton == Button.qstrName && !Button.qlstMotionParams.isEmpty())
        {
            Button.qlstMotionParams.first()["keep"] = bChecked ? 1 : 0;
        }
    }
}


// 将某个运动按钮的参数组帧下发(fTimeUseOverride > 0 时覆盖该按钮自己的执行时长;
// fVoltageOverride >= 0 时覆盖该按钮自己的电压 —— 交替序列的间隔段用 0V 实现)
void ControlPanel::SendButtonMotion(const ButtonParams &Button, const float fTimeUseOverride, const float fVoltageOverride)
{
    if (Button.qlstMotionParams.isEmpty())
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;

    GeneralMotion::MotionParams sParams;
    sParams.eIndex = Button.eMotion;
    sParams.fVoltage = (fVoltageOverride >= 0) ? fVoltageOverride
                                               : Button.qlstMotionParams.first().value("voltage").toFloat();
    sParams.fFrequency = Button.qlstMotionParams.first().value("frequency").toFloat();
    sParams.fTimeUse = (fTimeUseOverride > 0) ? fTimeUseOverride
                                              : Button.qlstMotionParams.first().value("timeuse").toFloat();
    sParams.s16AngleOffset = Button.qlstMotionParams.first().value("angle_offset").toInt();
    // speed 已取消, 改为 ModifyCoeff: 只有水平/垂直居中用滑块的值, 其他运动发 0
    sParams.s8ModifyCoeff = GetModifyCoeff(Button.eMotion);

    // "保持" = 无限时间动作(协议数据域末尾标志 1)。
    // 但序列(聚拢交替 / G1 / G2)推进全靠"时间到"的完成帧: 某一段一旦变成无限动作,
    // 下位机永远不回完成帧, 整个序列会卡死在那里。所以带 fTimeUseOverride 的段
    // 一律强制按时间走, 不受按钮上的保持勾选影响。
    sParams.bInfiniteTime = (fTimeUseOverride > 0)
                            ? false
                            : (Button.qlstMotionParams.first().value("keep", 0).toInt() != 0);

    m_pProtocol->MotionControlProtocol(sParams, qbtData, qbtRespond);

    emit sigMotion(qbtData, qbtRespond);
}


// pbGather 交替序列: 交替下发 pbHCentered / pbVCentered 的参数。
// 每段时长取 ui->dspHMotionTime / dspVMotionTime(两者可不同), 段间静止间隔取 ui->dspIntervalTime
// (间隔 0 表示无缝衔接); 段数累计到 Gather 按钮自身 timeuse 为止(至少 1 段)。
// 所有段一次性入队, 引擎逐段下发(每段等下位机完成回帧后才发下一段)。
//
// 「保持」(2026-11-25 二十五次): 勾了保持就完全不看 timeuse —— 只排一个 H+V 周期,
// 之后每次队列见底(见构造函数里的 sigMotionTerminate)再排一个周期, 一直交替到
// 用户点"结束运动"为止。
// 为什么不干脆把每段发成"无限时间动作": 序列是靠下位机"时间到"的完成帧推进的,
// 某段一旦变成无限动作就永远不会有完成帧, 序列会卡死在第一段(见 SendButtonMotion 注释)。
// 所以"保持"在聚拢这里改由上位机续排实现, 段本身仍按时间走。
void ControlPanel::StartGatherSequence(void)
{
    const ButtonParams *pGather = nullptr;
    const ButtonParams *pHCentered = nullptr;
    const ButtonParams *pVCentered = nullptr;

    for (const ButtonParams &Button : m_lstMotionButtons)
    {
        if (Button.qstrName == "MotionGather")
        {
            pGather = &Button;
        }
        else if (Button.qstrName == "MotionHCentered")
        {
            pHCentered = &Button;
        }
        else if (Button.qstrName == "MotionVCentered")
        {
            pVCentered = &Button;
        }
    }

    if (!pGather || !pHCentered || !pVCentered)
    {
        qWarning() << "gather sequence: button params missing";
        return;
    }

    if (pGather->qlstMotionParams.isEmpty())
    {
        return;
    }

    // 交替序列的三个时间参数取自模式数据面板(单位: 秒 -> 毫秒)
    // 段时长下限 10ms, 防止 0 值生成大量空指令; 间隔为 0 表示段间不停。
    // 启动时快照成成员: "保持"续排必须沿用同一份节奏, 否则用户跑到一半改界面,
    // 下一轮就换节奏了(正在跑的这一轮却没变), 现场很难解释。
    m_pSequence->iSegHMs = qMax(10, qRound(ui->dspHMotionTime->value() * 1000.0f));
    m_pSequence->iSegVMs = qMax(10, qRound(ui->dspVMotionTime->value() * 1000.0f));
    m_pSequence->iGapMs  = qMax(0,  qRound(ui->dspIntervalTime->value() * 1000.0f));

    // Gather 按钮自己的 "keep" = 用户勾了「保持」: 一直交替, 不按时间收尾
    m_pSequence->bRepeat = (pGather->qlstMotionParams.first().value("keep", 0).toInt() != 0);

    m_pSequence->u32Remaining = 0;
    // 本面板是这条序列的 owner: 完成帧由它递减/续队(对面面板只负责显示)
    m_pSequence->pOwner = this;
    m_pSequence->u32RoundCmds = 0;
    m_pSequence->iRoundMs = 0;

    int iWallMs = 0;

    if (m_pSequence->bRepeat)
    {
        // 保持: 一轮 = 一个完整的 H/V 交替周期。timeuse 不参与 —— 它表示"总共动多久",
        // 而勾了保持的语义正是"不设结束时间", 两个语义冲突时以保持为准。
        m_pSequence->u32Remaining = QueueGatherRound();
        m_pSequence->u32RoundCmds = m_pSequence->u32Remaining;
        m_pSequence->iRoundMs = m_pSequence->iSegHMs + m_pSequence->iSegVMs + 2 * m_pSequence->iGapMs;
        iWallMs = m_pSequence->iRoundMs;

        qDebug() << "gather sequence (keep): repeat H" << m_pSequence->iSegHMs << "ms / V"
                 << m_pSequence->iSegVMs << "ms, gap" << m_pSequence->iGapMs << "ms,"
                 << m_pSequence->u32Remaining << "cmds per round, until user stops";
    }
    else
    {
        // 总执行时间取 pbGather 按钮自己的 timeuse
        const int iTotalMs = qRound(pGather->qlstMotionParams.first().value("timeuse").toFloat() * 1000.0f);

        // 先排段: 从水平居中开始 H / V / H / V 交替, 逐段累计运动时长,
        // 累计刚达到/超过总时长就停止(至少 1 段) —— H/V 段时长可以不同, 不能再用整除算段数
        struct SeqSeg
        {
            const ButtonParams *pButton;
            int iMs;
        };
        QVector<SeqSeg> lstSegs;
        int iMotionMs = 0;
        for (int i = 0; i < 1000; i++)     // 1000 段上限, 防参数异常导致死循环
        {
            const bool bIsH = (i % 2 == 0);
            lstSegs.append({ bIsH ? pHCentered : pVCentered, bIsH ? m_pSequence->iSegHMs : m_pSequence->iSegVMs });
            iMotionMs += lstSegs.last().iMs;

            if (iMotionMs >= iTotalMs)
            {
                break;
            }
        }

        // 逐段入队: H [间隔] V [间隔] H ...
        // 间隔段 = 一条 0V 的运动指令: 下位机占空比为 0, 音圈电机断电静止,
        // 按间隔时长计时后照常回完成帧 —— 引擎节拍与应答匹配逻辑不变, 间隔由下位机保证
        // (间隔为 0 时不插间隔段, 段与段无缝衔接)
        for (int i = 0; i < lstSegs.size(); i++)
        {
            const SeqSeg &Seg = lstSegs[i];
            SendButtonMotion(*Seg.pButton, Seg.iMs / 1000.0f);
            m_pSequence->u32Remaining++;
            iWallMs += Seg.iMs;

            if (i + 1 < lstSegs.size() && m_pSequence->iGapMs > 0)
            {
                SendButtonMotion(*Seg.pButton, m_pSequence->iGapMs / 1000.0f, 0.0f);
                m_pSequence->u32Remaining++;
                iWallMs += m_pSequence->iGapMs;
            }
        }

        qDebug() << "gather sequence:" << lstSegs.size() << "motions,"
                 << (m_pSequence->iGapMs > 0 ? lstSegs.size() - 1 : 0) << "gaps,"
                 << iMotionMs << "ms motion," << iWallMs << "ms wall, expect" << iTotalMs << "ms"
                 << "(H" << m_pSequence->iSegHMs << "ms V" << m_pSequence->iSegVMs << "ms gap" << m_pSequence->iGapMs << "ms)";
    }

    if (m_pSequence->u32Remaining == 0)
    {
        qWarning() << "gather sequence: nothing queued";
        m_pSequence->bRepeat = false;
        return;
    }

    // 序列运行期间锁住 G1/G2, 防止两种序列互相清计数(完成/停止时统一恢复)
    ui->pbG1->setEnabled(false);
    ui->pbG2->setEnabled(false);

    StartMotionSeqWatchdog(iWallMs, m_pSequence->u32Remaining);
}


// 按启动时的快照再排一轮 H [间隔] V [间隔](保持模式专用)。
// 返回实际入队的命令数; 0 = 按钮参数丢了, 调用方应退出保持。
quint32 ControlPanel::QueueGatherRound(void)
{
    if (!m_bRunEnabled)
    {
        return 0;
    }

    const ButtonParams *pHCentered = nullptr;
    const ButtonParams *pVCentered = nullptr;

    for (const ButtonParams &Button : m_lstMotionButtons)
    {
        if (Button.qstrName == "MotionHCentered")
        {
            pHCentered = &Button;
        }
        else if (Button.qstrName == "MotionVCentered")
        {
            pVCentered = &Button;
        }
    }

    if (pHCentered == nullptr || pVCentered == nullptr
        || pHCentered->qlstMotionParams.isEmpty() || pVCentered->qlstMotionParams.isEmpty())
    {
        return 0;
    }

    quint32 u32Count = 0;

    SendButtonMotion(*pHCentered, m_pSequence->iSegHMs / 1000.0f);
    u32Count++;
    if (m_pSequence->iGapMs > 0)
    {
        SendButtonMotion(*pHCentered, m_pSequence->iGapMs / 1000.0f, 0.0f);
        u32Count++;
    }

    SendButtonMotion(*pVCentered, m_pSequence->iSegVMs / 1000.0f);
    u32Count++;
    if (m_pSequence->iGapMs > 0)
    {
        SendButtonMotion(*pVCentered, m_pSequence->iGapMs / 1000.0f, 0.0f);
        u32Count++;
    }

    return u32Count;
}


// 序列兜底计时: 每条命令最坏 = 命令时长 + 引擎 2s 响应超时冗余, 再加 1s 余量。
// 保持模式每续一轮都重启一次(按一轮的墙钟算), 所以只有"某一轮中途彻底没响应"
// 才会触发 —— 正常情况下这个看门狗永远等不到超时。
void ControlPanel::StartMotionSeqWatchdog(const int iWallMs, const quint32 u32Cmds)
{
    if (m_pMotionSeqWatchdog)
    {
        m_pMotionSeqWatchdog->start(iWallMs + static_cast<int>(u32Cmds) * 2200 + 1000);
    }
}


// 复位序列状态(不清通信队列; 需要停运动由停止指令负责)
void ControlPanel::ResetMotionSequence(void)
{
    m_pSequence->u32Remaining = 0;

    // 保持标志必须一起清: 只剩计数归零的话, 下一次(甚至迟到的)完成帧会让
    // 序列"自己活过来" —— 用户已经点了结束运动, 电机却又动起来。
    m_pSequence->bRepeat = false;
    m_pSequence->u32RoundCmds = 0;

    // 序列没了 owner: 之后到来的收尾完成帧(停止/超时那一次)谁都可以处理
    m_pSequence->pOwner = nullptr;

    if (m_pMotionSeqWatchdog)
    {
        m_pMotionSeqWatchdog->stop();
    }
}


// 两个面板共享同一份运动序列状态(ControlWidget 构造时调用一次)
void ControlPanel::UseSharedSequence(MotionSequenceState *pState, ControlPanel *pPeerPanel)
{
    if (pState == nullptr)
    {
        return;
    }

    m_pSequence = pState;
    m_pPeerPanel = pPeerPanel;
}


// 按运动类型找到对应的按钮参数(电压/频率/角度/速度都从这里取)
const ControlPanel::ButtonParams *ControlPanel::FindButtonByMotion(const GeneralMotion::MotionParams::MotionIndex eMotion) const
{
    for (const ButtonParams &Button : m_lstMotionButtons)
    {
        if (Button.eMotion == eMotion && !Button.qlstMotionParams.isEmpty())
        {
            return &Button;
        }
    }

    return nullptr;
}


// 位置修正系数 ModifyCoeff(替代原 speed, 有符号 -50~50):
// 水平居中 -> vsHModify(竖直滑块, 调水平位置); 垂直居中 -> hsVModify(水平滑块, 调垂直位置)
// 其他运动(含聚拢/G1/G2 里的普通动作)一律发 0
int8_t ControlPanel::GetModifyCoeff(const GeneralMotion::MotionParams::MotionIndex eMotion) const
{
    switch (eMotion)
    {
        case GeneralMotion::MotionParams::MotionIndex::MotionHCentered:
            return static_cast<int8_t>(ui->vsHModify->value());

        case GeneralMotion::MotionParams::MotionIndex::MotionVCentered:
            return static_cast<int8_t>(ui->hsVModify->value());

        default:
            return 0;
    }
}


// 组合运动 G1 / G2: 按 S1..S8 的顺序逐条下发。
// 每条指令的电压/频率/角度/速度取该运动类型自己的按钮参数, 只有执行时间取表单里填的时间。
// 与交替居中序列一样: 所有命令一次性入队, 节拍由下位机完成回帧驱动, 无累计漂移;
// 计数归零才恢复按钮, 中途可用"停止"打断。
void ControlPanel::StartGroupMotionSequence(GroupMotion *pGroupMotion, const QString &qstrGroupName)
{
    if (!m_bRunEnabled || pGroupMotion == nullptr)
    {
        return;
    }

    // 已有序列在跑(按钮此时应已禁用, 这里兜底): 不能中途重置计数, 否则按钮会提前恢复
    if (m_pSequence->u32Remaining > 0 || m_bMotionUiLocked)
    {
        qWarning() << qstrGroupName << "sequence ignored: another sequence is running";
        return;
    }

    const QVector<GroupMotion::Step> lstSteps = pGroupMotion->GetSteps();
    if (lstSteps.isEmpty())
    {
        qWarning() << qstrGroupName << "sequence: no valid step"
                   << "(第一步就是\"停止\", 或所有步的时间都是 0)";
        return;
    }

    // 清掉可能残留的序列计数, 避免误吞本次序列的完成信号
    ResetMotionSequence();

    quint32 u32Count = 0;
    int iWallMs = 0;
    for (const GroupMotion::Step &Step : lstSteps)
    {
        const ButtonParams *pButton = FindButtonByMotion(Step.eMotion);
        if (pButton == nullptr)
        {
            qWarning() << qstrGroupName << "sequence: no button params for motion"
                       << static_cast<int>(Step.eMotion);
            continue;
        }

        SendButtonMotion(*pButton, Step.fTimeUse);
        u32Count++;
        iWallMs += qRound(Step.fTimeUse * 1000.0f);
    }

    if (u32Count == 0)
    {
        qWarning() << qstrGroupName << "sequence: nothing queued";
        return;
    }

    m_pSequence->u32Remaining = u32Count;
    m_pSequence->pOwner = this;      // 组合运动同样: 只有发起者处理完成帧

    qDebug() << qstrGroupName << "sequence:" << u32Count << "steps," << iWallMs << "ms";

    // 序列期间锁界面: 发送/停止互换 + G1/G2 与全部运动类型按钮置灰
    LockMotionUi();

    StartMotionSeqWatchdog(iWallMs, u32Count);
}


void ControlPanel::on_pbG1_clicked()
{
    StartGroupMotionSequence(m_pGroupMotion1, "G1");
}


void ControlPanel::on_pbG2_clicked()
{
    StartGroupMotionSequence(m_pGroupMotion2, "G2");
}


void ControlPanel::on_pbSendMotion_clicked()
{
    // 兜底: 运动进行中(按钮已置灰)不接受重复下发, 否则会往队列里再塞一条运动
    if (!m_bRunEnabled || m_bMotionUiLocked)
    {
        return;
    }

    for (ButtonParams &Button : m_lstMotionButtons)
    {
        if (m_qstrCurrentMotionButton == Button.qstrName && !Button.qlstMotionParams.isEmpty())
        {
            if (Button.qstrName == "MotionGather")
            {
                // pbGather: 不下发自己的内容, 交替下发 水平居中/垂直居中 的内容
                StartGatherSequence();
            }
            else
            {
                // 普通运动: 先清掉可能残留的序列计数, 避免误吞本次运动的完成信号
                ResetMotionSequence();
                SendButtonMotion(Button, -1.0f);
            }

            // 下发后立刻上锁: 全部运动类型按钮置灰, 直到
            // "完成帧 / 运动反馈超时 / 点了停止运动" 才由 RestoreIdleButtons 解锁
            LockMotionUi();
        }
    }
}


void ControlPanel::on_pbSendMotionStop_clicked()
{
    // 停止所有运动; 交替序列也要终止:
    // 停止指令会清空引擎里未完成的运动队列段, 这里同步复位序列计数,
    // 随后到来的 sigMotionTerminate 会按"普通完成"路径恢复按钮
    ResetMotionSequence();

    // 停止所有运动
    emit sigStopMotion();
}


void ControlPanel::SaveSettings(void)
{
    // 将配置写入到当前打开的配置里
    QSettings Settings(m_qstrCurrentSettingPath, QSettings::IniFormat);

    Settings.setIniCodec("UTF-8");

    for (ButtonParams &Button : m_lstMotionButtons)
    {
        Settings.beginGroup(Button.qstrName);

        Settings.setValue("frequency", Button.qlstMotionParams.first().value("frequency"));
        Settings.setValue("timeuse", Button.qlstMotionParams.first().value("timeuse"));
        Settings.setValue("voltage", Button.qlstMotionParams.first().value("voltage"));

        Settings.setValue("angle_offset", Button.qlstMotionParams.first().value("angle_offset"));
        // keep: 1=无限时间动作(勾了"保持")
        Settings.setValue("keep", Button.qlstMotionParams.first().value("keep", 0));

        Settings.endGroup();
    }

    // 料仓参数
    Settings.beginGroup("BinMotion");
    Settings.setValue("voltage", ui->dspbBinVoltage->value());
    Settings.setValue("frequency", ui->dspbBinFrequency->value());
    Settings.setValue("timeuse", ui->dspbBinTime->value());
    Settings.setValue("keep", ui->cbBinKeep->isChecked() ? 1 : 0);
    Settings.endGroup();

    // 光源参数
    Settings.beginGroup("LightA");
    Settings.setValue("brightness", ui->spLightABrightness->value());
    Settings.setValue("timeuse", ui->spLightATime->value());
    Settings.setValue("keep", ui->cbLightAKeep->isChecked() ? 1 : 0);
    Settings.endGroup();

    Settings.beginGroup("LightB");
    Settings.setValue("brightness", ui->spLightBBrightness->value());
    Settings.setValue("timeuse", ui->spLightBTime->value());
    Settings.setValue("keep", ui->cbLightBKeep->isChecked() ? 1 : 0);
    Settings.endGroup();

    Settings.beginGroup("LightC");
    Settings.setValue("brightness", ui->spLightCBrightness->value());
    Settings.setValue("timeuse", ui->spLightCTime->value());
    Settings.setValue("keep", ui->cbLightCKeep->isChecked() ? 1 : 0);
    Settings.endGroup();

    Settings.beginGroup("LightD");
    Settings.setValue("brightness", ui->spLightDBrightness->value());
    Settings.setValue("timeuse", ui->spLightDTime->value());
    Settings.setValue("keep", ui->cbLightDKeep->isChecked() ? 1 : 0);
    Settings.endGroup();

    // 聚拢(交替居中)序列的时间参数
    Settings.beginGroup("GatherSequence");
    Settings.setValue("hmotion_time", ui->dspHMotionTime->value());
    Settings.setValue("vmotion_time", ui->dspVMotionTime->value());
    Settings.setValue("interval_time", ui->dspIntervalTime->value());
    Settings.endGroup();

    // ModifyCoeff 位置修正系数(水平居中/垂直居中各一个)
    Settings.beginGroup("ModifyCoeff");
    Settings.setValue("hcentered", ui->vsHModify->value());
    Settings.setValue("vcentered", ui->hsVModify->value());
    Settings.endGroup();

    // 组合运动 G1/G2: 8 步的动作类型(枚举值) + 运行时间
    auto SaveGroupMotion = [&Settings](const QString &qstrGroup, const GroupMotion *pGroup) {
        if (pGroup == nullptr)
        {
            return;
        }

        const QVector<GroupMotion::Step> lstSteps = pGroup->GetAllSteps();

        Settings.beginGroup(qstrGroup);
        for (int i = 0; i < lstSteps.size(); i++)
        {
            Settings.setValue(QString("s%1_motion").arg(i + 1), static_cast<int>(lstSteps.at(i).eMotion));
            Settings.setValue(QString("s%1_time").arg(i + 1), lstSteps.at(i).fTimeUse);
        }
        Settings.endGroup();
    };

    SaveGroupMotion("GroupMotion1", m_pGroupMotion1);
    SaveGroupMotion("GroupMotion2", m_pGroupMotion2);
}


void ControlPanel::SaveSettings(const QString &qstrFile)
{
    if (qstrFile.isEmpty())
    {
        return;
    }

    // 如果目标已存在，先删除
    if (QFile::exists(qstrFile))
    {
        QFile::remove(qstrFile);
    }

    // 复制
    QFile::copy(m_qstrCurrentSettingPath, qstrFile);

    // 把修改过的值要写入到新的文件里
    m_qstrCurrentSettingPath = qstrFile;
    SaveSettings();
}


void ControlPanel::LoadSettings(const QString &qstrFile)
{
    if (qstrFile.isEmpty())
    {
        return;
    }
    GetSettingParams(qstrFile);
    const QSignalBlocker BlockV(ui->dspbVoltage);
    const QSignalBlocker BlockF(ui->dspbFrequency);
    const QSignalBlocker BlockT(ui->dspbTime);
    const QSignalBlocker BlockD(ui->dialAngleRegulation);
    for (const ButtonParams &Button : m_lstMotionButtons)
    {
        if (Button.qstrName == m_qstrCurrentMotionButton)
        {
            OnMotionGroupButtonClicked(Button.pButton);
            break;
        }
    }
}


void ControlPanel::NewSettings(const QString &qstrFile)
{
    if (qstrFile.isEmpty())
    {
        return;
    }

    // 如果目标已存在，先删除
    if (QFile::exists(qstrFile))
    {
        QFile::remove(qstrFile);
    }

    // 复制
    QFile::copy(QCoreApplication::applicationDirPath() + "/DefaultConfigRaw.ini", qstrFile);
}


int16_t ControlPanel::ChangeDialDirection(const GeneralMotion::MotionParams::MotionIndex eIndex, const int16_t s16DialValue)
{
    int16_t s16OriginDirection = 0;

    switch (eIndex)
    {
        case GeneralMotion::MotionParams::MotionIndex::MotionUp:
            s16OriginDirection = 180;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionDown:
            s16OriginDirection = 0;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionLeft:
            s16OriginDirection = 270;
            break;

        case GeneralMotion::MotionParams::MotionIndex::MotionRight:
            s16OriginDirection = 90;
            break;

        default:
            break;
    }

    return (s16DialValue - s16OriginDirection);
}

void ControlPanel::on_dialAngleRegulation_valueChanged(int value)
{
    qDebug() << value;
    for (ButtonParams &Button : m_lstMotionButtons)
    {
        if (m_qstrCurrentMotionButton == Button.qstrName)
        {
            int16_t s16DialValue = ChangeDialDirection(Button.eMotion, value);
            Button.qlstMotionParams.first()["angle_offset"] = s16DialValue;
            ui->leAngle->setText(QString::number(s16DialValue));
        }
    }
}


void ControlPanel::on_hsldSpeed_valueChanged(int value)
{
    // speed 参数已取消(换成 ModifyCoeff, 且不再按按钮存), 控件也已隐藏。
    // 保留此槽只是为了不破坏 .ui 的自动连接, 不写任何参数, 免得污染按钮参数表。
    Q_UNUSED(value);
}


// 水平居中位置调节(竖直滑块 vsHModify): 值在下发时直接读取, 槽里不需要缓存
void ControlPanel::on_vsHModify_valueChanged(int value)
{
    Q_UNUSED(value);
}


// 垂直居中位置调节(水平滑块 hsVModify): 同上
void ControlPanel::on_hsVModify_valueChanged(int value)
{
    Q_UNUSED(value);
}


void ControlPanel::CopyButtonParams(const ControlPanel *pOther)
{
    if (this == pOther || pOther == nullptr)
    {
        return;
    }

    // 按名字逐按钮拷贝参数, 保留本面板自己的 pButton/eMotion:
    // 整表赋值会把对方面板的按钮指针拷过来, 导致本面板运动按钮永远选不中
    for (ButtonParams &Button : m_lstMotionButtons)
    {
        for (const ButtonParams &Other : pOther->m_lstMotionButtons)
        {
            if (Button.qstrName == Other.qstrName)
            {
                Button.qlstMotionParams = Other.qlstMotionParams;
                break;
            }
        }
    }

    // 料仓与光源参数同步拷贝
    ui->dspbBinVoltage->setValue(pOther->ui->dspbBinVoltage->value());
    ui->dspbBinFrequency->setValue(pOther->ui->dspbBinFrequency->value());
    ui->dspbBinTime->setValue(pOther->ui->dspbBinTime->value());
    ui->cbBinKeep->setChecked(pOther->ui->cbBinKeep->isChecked());
    ui->spLightABrightness->setValue(pOther->ui->spLightABrightness->value());
    ui->spLightATime->setValue(pOther->ui->spLightATime->value());
    ui->cbLightAKeep->setChecked(pOther->ui->cbLightAKeep->isChecked());
    ui->spLightBBrightness->setValue(pOther->ui->spLightBBrightness->value());
    ui->spLightBTime->setValue(pOther->ui->spLightBTime->value());
    ui->cbLightBKeep->setChecked(pOther->ui->cbLightBKeep->isChecked());
    ui->spLightCBrightness->setValue(pOther->ui->spLightCBrightness->value());
    ui->spLightCTime->setValue(pOther->ui->spLightCTime->value());
    ui->cbLightCKeep->setChecked(pOther->ui->cbLightCKeep->isChecked());
    ui->spLightDBrightness->setValue(pOther->ui->spLightDBrightness->value());
    ui->spLightDTime->setValue(pOther->ui->spLightDTime->value());
    ui->cbLightDKeep->setChecked(pOther->ui->cbLightDKeep->isChecked());

    // 聚拢(交替居中)序列的三个时间参数也要互拷, 否则模式数据面板保存后会丢
    ui->dspHMotionTime->setValue(pOther->ui->dspHMotionTime->value());
    ui->dspVMotionTime->setValue(pOther->ui->dspVMotionTime->value());
    ui->dspIntervalTime->setValue(pOther->ui->dspIntervalTime->value());

    // 两个 ModifyCoeff 滑块同样要互拷(水平/垂直居中的位置修正)
    ui->vsHModify->setValue(pOther->ui->vsHModify->value());
    ui->hsVModify->setValue(pOther->ui->hsVModify->value());

    // 组合运动 G1/G2: 8 步的动作类型与运行时间同步拷贝
    if (m_pGroupMotion1 != nullptr && pOther->m_pGroupMotion1 != nullptr)
    {
        m_pGroupMotion1->CopyFrom(pOther->m_pGroupMotion1);
    }
    if (m_pGroupMotion2 != nullptr && pOther->m_pGroupMotion2 != nullptr)
    {
        m_pGroupMotion2->CopyFrom(pOther->m_pGroupMotion2);
    }

    // 只刷新当前选中按钮；未选中时明确选中第一项，避免显示与编辑目标错配。
    if (m_qstrCurrentMotionButton.isEmpty() && !m_lstMotionButtons.isEmpty())
    {
        m_qstrCurrentMotionButton = m_lstMotionButtons.first().qstrName;
        m_lstMotionButtons.first().pButton->setChecked(true);
    }
    {
        const QSignalBlocker BlockV(ui->dspbVoltage);
        const QSignalBlocker BlockF(ui->dspbFrequency);
        const QSignalBlocker BlockT(ui->dspbTime);
        const QSignalBlocker BlockD(ui->dialAngleRegulation);

        for (const ButtonParams &Button : m_lstMotionButtons)
        {
            if (Button.qstrName != m_qstrCurrentMotionButton || Button.qlstMotionParams.isEmpty())
            {
                continue;
            }

            ui->dspbVoltage->setValue(Button.qlstMotionParams.first().value("voltage").toFloat());
            ui->dspbFrequency->setValue(Button.qlstMotionParams.first().value("frequency").toFloat());
            ui->dspbTime->setValue(Button.qlstMotionParams.first().value("timeuse").toFloat());
            // "保持"也是按钮级参数(同一套共享控件编辑不同按钮), 切换/刷新时必须跟着换
            ui->cbMotionKeep->setChecked(Button.qlstMotionParams.first().value("keep", 0).toInt() != 0);

            ui->leAngle->setText(QString::number(Button.qlstMotionParams.first().value("angle_offset").toInt()));
            SetDialDirection(Button.eMotion, Button.qlstMotionParams.first().value("angle_offset").toInt());
        }
    }
}

void ControlPanel::on_pbBinStartMotion_clicked()
{
    qDebug() << "send bin motion, run enabled:" << m_bRunEnabled;

    // 兜底: 未连接时本面板的运行键已全灰, 正常点不到; 这里再挡一次,
    // 避免有人绕过界面直接调槽把按钮带进"已发送但等不到应答"的半截态
    if (!m_bRunEnabled)
    {
        return;
    }

    // 料仓运动: 与振动盘同语义, 下位机运动完成后才回帧(0x08)
    GeneralMotion::MotionParams sParams;
    sParams.eIndex = GeneralMotion::MotionParams::MotionIndex::MotionBin;
    sParams.fVoltage = static_cast<float>(ui->dspbBinVoltage->value());
    sParams.fFrequency = static_cast<float>(ui->dspbBinFrequency->value());
    sParams.fTimeUse = static_cast<float>(ui->dspbBinTime->value());
    sParams.s16AngleOffset = 0;
    // 料仓不使用位置修正系数(协议里也没这两个字节)
    sParams.s8ModifyCoeff = 0;
    // "保持"= 无限时间动作: 下位机一直振动, 直到收到料仓停止指令(0x09)
    sParams.bInfiniteTime = ui->cbBinKeep->isChecked();

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->BinMotionProtocol(sParams, qbtData, qbtRespond);


    emit sigBinMotion(qbtData, qbtRespond);

    ui->pbBinStartMotion->setEnabled(false);
    ui->pbBinStopMotion->setEnabled(true);
    // 状态灯: 料仓开始动 → 右下角"料仓"灯转绿(停/完成帧到达时由 RestoreIdleButtons 回灰)
    NotifyRunState();
}

void ControlPanel::on_pbBinStopMotion_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    // 料仓停止: 通信引擎会清空未完成的料仓运动指令并发 terminate 恢复按钮
    emit sigBinStopMotion();
}

void ControlPanel::on_pbOpenLightA_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    // 光源 A(索引 1): 亮度 0~100, 保持时间单位 s, 协议层 x10(4 字节, 上限 9999s)
    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_A,
                                 static_cast<uint8_t>(ui->spLightABrightness->value()),
                                 static_cast<uint32_t>(ui->spLightATime->value() * 10),
                                 ui->cbLightAKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightA->setEnabled(false);
    ui->pbCloseLightA->setEnabled(true);
    SetLightLamp(0, true);
    StartLightRecover(0, ui->spLightATime->value(), ui->cbLightAKeep->isChecked());
}

void ControlPanel::on_pbCloseLightA_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    // 只关 A: 带上索引, 不能把 B 一起关掉
    m_pProtocol->LightOffProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_A, qbtData, qbtRespond);

    emit sigLightOff(qbtData, qbtRespond);

    ui->pbOpenLightA->setEnabled(true);
    ui->pbCloseLightA->setEnabled(false);
    SetLightLamp(0, false);

    // 手动关: 该路不会再有"到点", 表必须停, 否则过一会儿会把按钮再刷一遍
    StopLightRecover(0);
}

void ControlPanel::on_pbOpenLightB_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    // 光源 B(索引 2)
    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_B,
                                 static_cast<uint8_t>(ui->spLightBBrightness->value()),
                                 static_cast<uint32_t>(ui->spLightBTime->value() * 10),
                                 ui->cbLightBKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightB->setEnabled(false);
    ui->pbCloseLihgtB->setEnabled(true);
    SetLightLamp(1, true);
    StartLightRecover(1, ui->spLightBTime->value(), ui->cbLightBKeep->isChecked());
}

void ControlPanel::on_pbCloseLihgtB_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    // 只关 B
    m_pProtocol->LightOffProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_B, qbtData, qbtRespond);

    emit sigLightOff(qbtData, qbtRespond);

    ui->pbOpenLightB->setEnabled(true);
    ui->pbCloseLihgtB->setEnabled(false);
    SetLightLamp(1, false);
    StopLightRecover(1);
}

void ControlPanel::on_pbOpenLightC_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_C,
                                 static_cast<uint8_t>(ui->spLightCBrightness->value()),
                                 static_cast<uint32_t>(ui->spLightCTime->value() * 10),
                                 ui->cbLightCKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightC->setEnabled(false);
    ui->pbCloseLightC->setEnabled(true);
    SetLightLamp(2, true);
    StartLightRecover(2, ui->spLightCTime->value(), ui->cbLightCKeep->isChecked());
}

void ControlPanel::on_pbCloseLightC_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOffProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_C, qbtData, qbtRespond);

    emit sigLightOff(qbtData, qbtRespond);

    ui->pbOpenLightC->setEnabled(true);
    ui->pbCloseLightC->setEnabled(false);
    SetLightLamp(2, false);
    StopLightRecover(2);
}

void ControlPanel::on_pbOpenLightD_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_D,
                                 static_cast<uint8_t>(ui->spLightDBrightness->value()),
                                 static_cast<uint32_t>(ui->spLightDTime->value() * 10),
                                 ui->cbLightDKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightD->setEnabled(false);
    ui->pbCloseLightD->setEnabled(true);
    SetLightLamp(3, true);
    StartLightRecover(3, ui->spLightDTime->value(), ui->cbLightDKeep->isChecked());
}

void ControlPanel::on_pbCloseLightD_clicked()
{
    if (!m_bRunEnabled)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOffProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_D, qbtData, qbtRespond);

    emit sigLightOff(qbtData, qbtRespond);

    ui->pbOpenLightD->setEnabled(true);
    ui->pbCloseLightD->setEnabled(false);
    SetLightLamp(3, false);
    StopLightRecover(3);
}

// 光源保持时间到: 下位机自发 0x0E 完成帧(带索引), 恢复对应路光源按钮。
// u8Index: 1~4 对应 A~D, 0 = 全关(四路全部恢复)
// 光源状态灯: 灰=灭 / 绿=亮。
// 灯只在运行页(bFlatRun)创建, 配置窗口那个实例的 m_lstLightLamps 是空的 ——
// 所以这里必须判空, 否则在配置窗口里点开/关灯会直接崩。
// iIndex: 0~3 = A~D(协议索引 1~4 差 1, 转换只在调用处做一次)
void ControlPanel::SetLightLamp(const int iIndex, const bool bOn)
{
    if (iIndex < 0 || iIndex >= m_lstLightLamps.size())
    {
        return;
    }

    if (QLabel *pLamp = m_lstLightLamps.at(iIndex))
    {
        pLamp->setPixmap(bOn ? m_pmLampOn : m_pmLampOff);
        // 现场不连上位机时也能鼠标悬停确认这一路是开还是关
        const QString qstrName = QString::fromLatin1(kpstrLightNames[qBound(0, iIndex, 3)]);
        pLamp->setAccessibleName(QStringLiteral("%1 光源状态").arg(qstrName));
        pLamp->setToolTip(QStringLiteral("%1 光源: %2")
                              .arg(qstrName, bOn ? QStringLiteral("已开启") : QStringLiteral("已关闭")));
    }
}


void ControlPanel::SetAllLightLampsOff(void)
{
    for (int i32Index = 0; i32Index < m_lstLightLamps.size(); ++i32Index)
    {
        SetLightLamp(i32Index, false);
    }
}


void ControlPanel::OnLightOffFinish(const uint8_t &u8Index)
{
    // 未连接时不会收到本信号, 这里兜底(不按"能不能下发"早退: 配置窗口的面板
    // 现在也会收这个信号, 它自己也发过光源指令, 按钮同样要能恢复)
    if (!m_bRunEnabled)
    {
        return;
    }

    // 四路写成表: 索引 ↔ 开/关键 ↔ 状态灯下标一一对应。
    // 原来是一条 if / else if 链, 加状态灯以后每个分支都要改两处, 漏一处就是
    // "灯灭了但按钮还能点"这种半截状态, 用表循环只有一处映射。
    struct LightChannel
    {
        uint8_t u8Index;
        QPushButton *pOpen;
        QPushButton *pClose;
        int iLamp;
    };
    const LightChannel arrChannels[4] = {
        {GeneralProtocolItems::u8SW_LIGHT_INDEX_A, ui->pbOpenLightA, ui->pbCloseLightA, 0},
        {GeneralProtocolItems::u8SW_LIGHT_INDEX_B, ui->pbOpenLightB, ui->pbCloseLihgtB, 1},
        {GeneralProtocolItems::u8SW_LIGHT_INDEX_C, ui->pbOpenLightC, ui->pbCloseLightC, 2},
        {GeneralProtocolItems::u8SW_LIGHT_INDEX_D, ui->pbOpenLightD, ui->pbCloseLightD, 3}};

    for (const LightChannel &sChannel : arrChannels)
    {
        // u8Index == ALL(0) 表示四路全关, 否则只恢复索引匹配的那一路
        if (u8Index != GeneralProtocolItems::u8SW_LIGHT_INDEX_ALL &&
            u8Index != sChannel.u8Index)
        {
            continue;
        }

        sChannel.pOpen->setEnabled(true);
        sChannel.pClose->setEnabled(false);
        SetLightLamp(sChannel.iLamp, false);
        // 下位机已经回了"到点熄灭", 本地表不必再走一遍(幂等, 但早停能少一次刷新)
        StopLightRecover(sChannel.iLamp);
    }
}


// ============================ 光源时间模式的本地恢复 ============================
// 背景: 时间模式(没勾「保持」)下, 下位机到点自己灭灯, 但现场固件不保证回 0x0E
// 完成帧(固件里 CbLightOffFinish 还是拿 NULL 发的, 连索引都没带)。只靠完成帧
// 恢复的话, "打开"键会一直灰着, 用户只能去点一次"关闭"才知道可以再开 —— 而那时
// 灯其实早就灭了。这里按用户界面上填的秒数在本地起表, 到点直接放开"打开"键。

void ControlPanel::StartLightRecover(const int iIndex, const int iSeconds, const bool bKeep)
{
    if (iIndex < 0 || iIndex >= 4 || m_apLightRecoverTimers[iIndex] == nullptr)
    {
        return;
    }

    // 同一路重新开灯: 先掐掉上一轮的表(可能上次还没到点就手动关掉又开了一次)
    m_apLightRecoverTimers[iIndex]->stop();

    // 两种"下位机不会自动灭灯"的情况, 一律不启动本地表:
    //   · 勾了「保持」            —— 常亮, 只能手动关;
    //   · 没勾但时间 = 0          —— 协议里"保持时间 0 = 常亮"(见固件 LightOnHandler
    //                                把 u32KeepTime 直接交给 LightControl), 0 秒不是立刻灭。
    // 这两种情况若也自动恢复, "打开"键会变可点而灯还亮着, 比现在更误导。
    if (bKeep || iSeconds <= 0)
    {
        return;
    }

    // +150ms 余量: 下位机从"收到帧"开始计时, 上位机从"下发"开始算, 起点差几十毫秒。
    // 不留余量时按钮会比灯早一点点变可点, 手快连点会发出"灯还亮着又开一次"的指令。
    m_apLightRecoverTimers[iIndex]->start(iSeconds * 1000 + 150);
}

void ControlPanel::StopLightRecover(const int iIndex)
{
    if (iIndex >= 0 && iIndex < 4 && m_apLightRecoverTimers[iIndex] != nullptr)
    {
        m_apLightRecoverTimers[iIndex]->stop();
    }
}

// 给另一个面板交接剩余时间用(见 SyncRunButtonStates)。0 = 没有倒计时。
int ControlPanel::LightRecoverRemainMs(const int iIndex) const
{
    if (iIndex < 0 || iIndex >= 4 || m_apLightRecoverTimers[iIndex] == nullptr)
    {
        return 0;
    }

    QTimer *pTimer = m_apLightRecoverTimers[iIndex];
    return pTimer->isActive() ? pTimer->remainingTime() : 0;
}

// 按"剩余毫秒"起表(不再加 150ms 余量: 这个值本来就是从另一块表上量下来的)
void ControlPanel::StartLightRecoverMs(const int iIndex, const int iMs)
{
    if (iIndex < 0 || iIndex >= 4 || m_apLightRecoverTimers[iIndex] == nullptr)
    {
        return;
    }

    m_apLightRecoverTimers[iIndex]->stop();
    if (iMs > 0)
    {
        m_apLightRecoverTimers[iIndex]->start(iMs);
    }
}

void ControlPanel::StopAllLightRecover(void)
{
    for (int i32Light = 0; i32Light < 4; ++i32Light)
    {
        StopLightRecover(i32Light);
    }
}

// 本地计时到点: 把该路恢复成"可开 / 不可关"并让状态灯回灰。
// 与 OnLightOffFinish 落到同一组操作, 两个入口(本地到点 / 下位机 0x0E)谁先到都一样。
void ControlPanel::RecoverLightChannel(const int iIndex)
{
    if (iIndex < 0 || iIndex >= 4)
    {
        return;
    }

    // 断连后按钮已被 SetRunEnabled(false) 全部禁用, 这里不能再把它点亮。
    // (定时器那时已经停了, 但队列里可能还压着一个已排上的 timeout 事件)
    if (!m_bRunEnabled)
    {
        return;
    }

    QPushButton *arrOpen[4] = {ui->pbOpenLightA, ui->pbOpenLightB,
                               ui->pbOpenLightC, ui->pbOpenLightD};
    QPushButton *arrClose[4] = {ui->pbCloseLightA, ui->pbCloseLihgtB,
                                ui->pbCloseLightC, ui->pbCloseLightD};

    arrOpen[iIndex]->setEnabled(true);
    arrClose[iIndex]->setEnabled(false);
    SetLightLamp(iIndex, false);
}
