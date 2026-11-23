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
            pTitle->setStyleSheet(QStringLiteral("font-weight: bold; color: #202020; background: #E7F1FC; padding: 2px 6px;"));
            // 关键：标题条必须是"定高、纵向不可拉伸"。
            // 原来只写了 setMinimumHeight(22)，竖直 SizePolicy 仍是 Preferred(=可放大)，
            // QVBoxLayout 于是把整列富余高度都塞给它 → 运动区顶部出现一大片空白蓝块，
            // 按钮被顶到下面（光源/料仓因为页内自带 verticalSpacer 吸收了富余高度，所以看不出问题）。
            pTitle->setFixedHeight(24);
            pTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            pTitle->show();
            pLayout->addWidget(pTitle, 0);
            pLayout->addWidget(pPage, 0);
            // 富余高度统一留在底部（与光源/料仓页内 spacer 的观感一致），不再撑高标题条或拉开按钮间距。
            pLayout->addStretch(1);
            pFlatLayout->addWidget(pGroup, s32Index == 0 ? 3 : 2);
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
        // 隐藏的高级页及其 G1/G2 控件留在 tabWidget 中，仍参与运行取值。
        ui->horizontalSpacer->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->horizontalSpacer_8->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
        ui->horizontalSpacer_9->changeSize(0, 0, QSizePolicy::Fixed, QSizePolicy::Fixed);
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

    // pbGather 交替序列的兜底: 若中途某段响应超时(引擎清空队列, 剩余段不会再有完成帧),
    // 计数会卡住, 界面按钮永远不恢复 —— 到点强制复位
    m_pMotionSeqWatchdog = new QTimer(this);
    m_pMotionSeqWatchdog->setSingleShot(true);
    connect(m_pMotionSeqWatchdog, &QTimer::timeout, this, [this]() {
        if (m_u32MotionSeqRemaining > 0)
        {
            qWarning() << "motion sequence watchdog timeout, force reset";
            ResetMotionSequence();
            RestoreIdleButtons();
        }
    });

    connect(this, &ControlPanel::sigMotionTerminate, this, [this]() {
        // 只读面板(模式数据)的按钮被 SetReadOnly 禁用, 不能被这里"恢复"回可点状态
        if (m_bReadOnly)
        {
            return;
        }

        // pbGather 交替序列: 每条命令(运动段或间隔段)完成触发一次本信号,
        // 中间的命令不恢复按钮, 最后一条才恢复
        if (m_u32MotionSeqRemaining > 0)
        {
            m_u32MotionSeqRemaining--;

            if (m_u32MotionSeqRemaining > 0)
            {
                return;
            }

            ResetMotionSequence();   // 计数归零, 序列正常结束, 落入下方统一恢复
        }

        // 统一恢复: 振动盘(含全部运动类型按钮) / 料仓 / 光源 / 组合运动按钮回到空闲态
        RestoreIdleButtons();
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
           (m_u32MotionSeqRemaining > 0) ||
           ui->pbSendMotionStop->isEnabled() ||
           ui->pbBinStopMotion->isEnabled();
}


void ControlPanel::SetReadOnly(const bool bReadOnly)
{
    m_bReadOnly = bReadOnly;

    if (bReadOnly)
    {
        // 只读面板(模式数据)不连接通信引擎: 这些按钮点了也不会下发, 必须禁用,
        // 否则点击后按钮会被置灰且永远收不到 sigMotionTerminate 恢复, 表现为"点了没反应"
        ui->pbSendMotion->setEnabled(false);
        ui->pbSendMotionStop->setEnabled(false);
        ui->pbG1->setEnabled(false);
        ui->pbG2->setEnabled(false);
        ui->pbBinStartMotion->setEnabled(false);
        ui->pbBinStopMotion->setEnabled(false);
        ui->pbOpenLightA->setEnabled(false);
        ui->pbCloseLightA->setEnabled(false);
        ui->pbOpenLightB->setEnabled(false);
        ui->pbCloseLihgtB->setEnabled(false);
        ui->pbOpenLightC->setEnabled(false);
        ui->pbCloseLightC->setEnabled(false);
        ui->pbOpenLightD->setEnabled(false);
        ui->pbCloseLightD->setEnabled(false);

        // 只读面板的运动类型按钮保持可点(编辑参数要先选按钮), 但不能顺手把锁解了
        m_bMotionUiLocked = false;
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
    if (m_bReadOnly)
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
}


// 唯一的统一恢复点: 振动盘(含运动类型按钮) / 料仓 / 光源 / 组合运动按钮全部回到空闲态
void ControlPanel::RestoreIdleButtons(void)
{
    // 只读面板的按钮由 SetReadOnly 管, 这里不能把它"恢复"成可点
    if (m_bReadOnly)
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
}


// 断连/重连: 链路没了, 在飞的指令不会再有完成帧, 界面必须无条件回空闲态。
// 不清通信队列(队列随连接销毁, 由 ControlWidget::CloseConnection 负责),
// 只复位计数 + 解锁按钮
void ControlPanel::ResetMotionUiState(void)
{
    if (m_bReadOnly)
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
void ControlPanel::SetKeyParamsStatus(const QString &qstrText, const bool bError)
{
    ui->lbKeyParamsStatus->setText(qstrText);
    ui->lbKeyParamsStatus->setStyleSheet(bError ? QStringLiteral("color: #C02626;")
                                                : QStringLiteral("color: #202020;"));
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
// 注意这里没有 m_bReadOnly 早退 —— 固件参数页只在"配置数据"窗口里出现,
// 而那个面板是 SetReadOnly(true) 的; 它发送的是"读写下位机 Flash"的指令,
// 不是改本地面板参数, 所以只读面板也必须能发。
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

    // 总执行时间取 pbGather 按钮自己的 timeuse
    const int iTotalMs = qRound(pGather->qlstMotionParams.first().value("timeuse").toFloat() * 1000.0f);

    // 交替序列的三个时间参数取自模式数据面板(单位: 秒 -> 毫秒)
    // 段时长下限 10ms, 防止 0 值生成大量空指令; 间隔为 0 表示段间不停
    const int iSegHMs = qMax(10, qRound(ui->dspHMotionTime->value() * 1000.0f));
    const int iSegVMs = qMax(10, qRound(ui->dspVMotionTime->value() * 1000.0f));
    const int iGapMs  = qMax(0,  qRound(ui->dspIntervalTime->value() * 1000.0f));

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
        lstSegs.append({ bIsH ? pHCentered : pVCentered, bIsH ? iSegHMs : iSegVMs });
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
    m_u32MotionSeqRemaining = 0;
    int iWallMs = 0;
    for (int i = 0; i < lstSegs.size(); i++)
    {
        const SeqSeg &Seg = lstSegs[i];
        SendButtonMotion(*Seg.pButton, Seg.iMs / 1000.0f);
        m_u32MotionSeqRemaining++;
        iWallMs += Seg.iMs;

        if (i + 1 < lstSegs.size() && iGapMs > 0)
        {
            SendButtonMotion(*Seg.pButton, iGapMs / 1000.0f, 0.0f);
            m_u32MotionSeqRemaining++;
            iWallMs += iGapMs;
        }
    }

    qDebug() << "gather sequence:" << lstSegs.size() << "motions,"
             << (iGapMs > 0 ? lstSegs.size() - 1 : 0) << "gaps,"
             << iMotionMs << "ms motion," << iWallMs << "ms wall, expect" << iTotalMs << "ms"
             << "(H" << iSegHMs << "ms V" << iSegVMs << "ms gap" << iGapMs << "ms)";

    // 序列运行期间锁住 G1/G2, 防止两种序列互相清计数(完成/停止时统一恢复)
    ui->pbG1->setEnabled(false);
    ui->pbG2->setEnabled(false);

    // 兜底: 每条命令最坏 = 命令时长 + 引擎 2s 响应超时冗余, 再加 1s 余量
    if (m_pMotionSeqWatchdog)
    {
        m_pMotionSeqWatchdog->start(iWallMs + static_cast<int>(m_u32MotionSeqRemaining) * 2200 + 1000);
    }
}


// 复位序列状态(不清通信队列; 需要停运动由停止指令负责)
void ControlPanel::ResetMotionSequence(void)
{
    m_u32MotionSeqRemaining = 0;

    if (m_pMotionSeqWatchdog)
    {
        m_pMotionSeqWatchdog->stop();
    }
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
    if (m_bReadOnly || pGroupMotion == nullptr)
    {
        return;
    }

    // 已有序列在跑(按钮此时应已禁用, 这里兜底): 不能中途重置计数, 否则按钮会提前恢复
    if (m_u32MotionSeqRemaining > 0 || m_bMotionUiLocked)
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

    m_u32MotionSeqRemaining = u32Count;

    qDebug() << qstrGroupName << "sequence:" << u32Count << "steps," << iWallMs << "ms";

    // 序列期间锁界面: 发送/停止互换 + G1/G2 与全部运动类型按钮置灰
    LockMotionUi();

    // 兜底: 每条命令最坏 = 命令时长 + 引擎 2s 响应超时冗余, 再加 1s 余量
    if (m_pMotionSeqWatchdog)
    {
        m_pMotionSeqWatchdog->start(iWallMs + static_cast<int>(u32Count) * 2200 + 1000);
    }
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
    if (m_bReadOnly || m_bMotionUiLocked)
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
    qDebug() << "send bin motion, send Enable:" << m_bReadOnly;

    // 只读面板不下发指令: 其信号未连接通信引擎, 发出去无人接收
    if (m_bReadOnly)
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
}

void ControlPanel::on_pbBinStopMotion_clicked()
{
    if (m_bReadOnly)
    {
        return;
    }

    // 料仓停止: 通信引擎会清空未完成的料仓运动指令并发 terminate 恢复按钮
    emit sigBinStopMotion();
}

void ControlPanel::on_pbOpenLightA_clicked()
{
    if (m_bReadOnly)
    {
        return;
    }

    // 光源 A(索引 1): 亮度 0~100, 保持时间单位 s, 协议层 x10
    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_A,
                                 static_cast<uint8_t>(ui->spLightABrightness->value()),
                                 static_cast<uint16_t>(qMin(ui->spLightATime->value() * 10, 65535)),
                                 ui->cbLightAKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightA->setEnabled(false);
    ui->pbCloseLightA->setEnabled(true);
}

void ControlPanel::on_pbCloseLightA_clicked()
{
    if (m_bReadOnly)
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
}

void ControlPanel::on_pbOpenLightB_clicked()
{
    if (m_bReadOnly)
    {
        return;
    }

    // 光源 B(索引 2)
    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_B,
                                 static_cast<uint8_t>(ui->spLightBBrightness->value()),
                                 static_cast<uint16_t>(qMin(ui->spLightBTime->value() * 10, 65535)),
                                 ui->cbLightBKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightB->setEnabled(false);
    ui->pbCloseLihgtB->setEnabled(true);
}

void ControlPanel::on_pbCloseLihgtB_clicked()
{
    if (m_bReadOnly)
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
}

void ControlPanel::on_pbOpenLightC_clicked()
{
    if (m_bReadOnly)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_C,
                                 static_cast<uint8_t>(ui->spLightCBrightness->value()),
                                 static_cast<uint16_t>(qMin(ui->spLightCTime->value() * 10, 65535)),
                                 ui->cbLightCKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightC->setEnabled(false);
    ui->pbCloseLightC->setEnabled(true);
}

void ControlPanel::on_pbCloseLightC_clicked()
{
    if (m_bReadOnly)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOffProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_C, qbtData, qbtRespond);

    emit sigLightOff(qbtData, qbtRespond);

    ui->pbOpenLightC->setEnabled(true);
    ui->pbCloseLightC->setEnabled(false);
}

void ControlPanel::on_pbOpenLightD_clicked()
{
    if (m_bReadOnly)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOnProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_D,
                                 static_cast<uint8_t>(ui->spLightDBrightness->value()),
                                 static_cast<uint16_t>(qMin(ui->spLightDTime->value() * 10, 65535)),
                                 ui->cbLightDKeep->isChecked(),
                                 qbtData, qbtRespond);

    emit sigLightOn(qbtData, qbtRespond);

    ui->pbOpenLightD->setEnabled(false);
    ui->pbCloseLightD->setEnabled(true);
}

void ControlPanel::on_pbCloseLightD_clicked()
{
    if (m_bReadOnly)
    {
        return;
    }

    QByteArray qbtData;
    QByteArray qbtRespond;
    m_pProtocol->LightOffProtocol(GeneralProtocolItems::u8SW_LIGHT_INDEX_D, qbtData, qbtRespond);

    emit sigLightOff(qbtData, qbtRespond);

    ui->pbOpenLightD->setEnabled(true);
    ui->pbCloseLightD->setEnabled(false);
}

// 光源保持时间到: 下位机自发 0x0E 完成帧(带索引), 恢复对应路光源按钮。
// u8Index: 1~4 对应 A~D, 0 = 全关(四路全部恢复)
void ControlPanel::OnLightOffFinish(const uint8_t &u8Index)
{
    // 只读面板(模式数据)未连接通信引擎, 不会收到本信号, 这里兜底
    if (m_bReadOnly)
    {
        return;
    }

    if (u8Index == GeneralProtocolItems::u8SW_LIGHT_INDEX_A)
    {
        ui->pbOpenLightA->setEnabled(true);
        ui->pbCloseLightA->setEnabled(false);
    }
    else if (u8Index == GeneralProtocolItems::u8SW_LIGHT_INDEX_B)
    {
        ui->pbOpenLightB->setEnabled(true);
        ui->pbCloseLihgtB->setEnabled(false);
    }
    else if (u8Index == GeneralProtocolItems::u8SW_LIGHT_INDEX_C)
    {
        ui->pbOpenLightC->setEnabled(true);
        ui->pbCloseLightC->setEnabled(false);
    }
    else if (u8Index == GeneralProtocolItems::u8SW_LIGHT_INDEX_D)
    {
        ui->pbOpenLightD->setEnabled(true);
        ui->pbCloseLightD->setEnabled(false);
    }
    else if (u8Index == GeneralProtocolItems::u8SW_LIGHT_INDEX_ALL)
    {
        ui->pbOpenLightA->setEnabled(true);
        ui->pbCloseLightA->setEnabled(false);
        ui->pbOpenLightB->setEnabled(true);
        ui->pbCloseLihgtB->setEnabled(false);
        ui->pbOpenLightC->setEnabled(true);
        ui->pbCloseLightC->setEnabled(false);
        ui->pbOpenLightD->setEnabled(true);
        ui->pbCloseLightD->setEnabled(false);
    }
}
