#include "controlpanel.h"
#include "ui_controlpanel.h"
#include "groupmotion.h"
#include <QDebug>
#include <QFile>
#include <QSettings>
#include <QSignalBlocker>
#include <QVBoxLayout>



ControlPanel::ControlPanel(QWidget *parent) :
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

            if (!m_bReadOnly)
            {
                ui->pbSendMotion->setEnabled(true);
                ui->pbSendMotionStop->setEnabled(false);
                ui->pbG1->setEnabled(true);
                ui->pbG2->setEnabled(true);
            }
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

        // 统一恢复: 振动盘 / 料仓 / 光源 / 组合运动按钮回到空闲态
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
    });

    ui->pbSendMotionStop->setEnabled(false);

    // 料仓与光源按钮初始为空闲态
    ui->pbBinStopMotion->setEnabled(false);
    ui->pbCloseLightA->setEnabled(false);
    ui->pbCloseLihgtB->setEnabled(false);
}

ControlPanel::~ControlPanel()
{
    delete ui;
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
            },
        };

        Settings.endGroup();
    }

    // 料仓参数
    Settings.beginGroup("BinMotion");
    ui->dspbBinVoltage->setValue(Settings.value("voltage", ui->dspbBinVoltage->value()).toDouble());
    ui->dspbBinFrequency->setValue(Settings.value("frequency", ui->dspbBinFrequency->value()).toDouble());
    ui->dspbBinTime->setValue(Settings.value("timeuse", ui->dspbBinTime->value()).toDouble());
    Settings.endGroup();

    // 光源参数
    Settings.beginGroup("LightA");
    ui->spLightABrightness->setValue(Settings.value("brightness", ui->spLightABrightness->value()).toInt());
    ui->spLightATime->setValue(Settings.value("timeuse", ui->spLightATime->value()).toInt());
    Settings.endGroup();

    Settings.beginGroup("LightB");
    ui->spLightBBrightness->setValue(Settings.value("brightness", ui->spLightBBrightness->value()).toInt());
    ui->spLightBTime->setValue(Settings.value("timeuse", ui->spLightBTime->value()).toInt());
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
    if (m_u32MotionSeqRemaining > 0)
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

    ui->pbSendMotion->setEnabled(false);
    ui->pbSendMotionStop->setEnabled(true);
    ui->pbG1->setEnabled(false);
    ui->pbG2->setEnabled(false);

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

            ui->pbSendMotion->setEnabled(false);
            ui->pbSendMotionStop->setEnabled(true);
            // 运动进行中锁住 G1/G2, 避免序列计数被当前运动的完成帧误吞
            ui->pbG1->setEnabled(false);
            ui->pbG2->setEnabled(false);
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

        Settings.endGroup();
    }

    // 料仓参数
    Settings.beginGroup("BinMotion");
    Settings.setValue("voltage", ui->dspbBinVoltage->value());
    Settings.setValue("frequency", ui->dspbBinFrequency->value());
    Settings.setValue("timeuse", ui->dspbBinTime->value());
    Settings.endGroup();

    // 光源参数
    Settings.beginGroup("LightA");
    Settings.setValue("brightness", ui->spLightABrightness->value());
    Settings.setValue("timeuse", ui->spLightATime->value());
    Settings.endGroup();

    Settings.beginGroup("LightB");
    Settings.setValue("brightness", ui->spLightBBrightness->value());
    Settings.setValue("timeuse", ui->spLightBTime->value());
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
    // 设置当前配置路径
    GetSettingParams(qstrFile);
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
    ui->spLightABrightness->setValue(pOther->ui->spLightABrightness->value());
    ui->spLightATime->setValue(pOther->ui->spLightATime->value());
    ui->spLightBBrightness->setValue(pOther->ui->spLightBBrightness->value());
    ui->spLightBTime->setValue(pOther->ui->spLightBTime->value());

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

    // 界面刷新所有按钮对应的参数(控件组只有一套, 依次刷新, 最终停留为最后
    // 一个按钮的值); 刷新期间必须屏蔽 valueChanged 信号, 否则槽会把遍历中的
    // 值写进"当前选中按钮"的存储参数, 造成数据污染
    {
        const QSignalBlocker BlockV(ui->dspbVoltage);
        const QSignalBlocker BlockF(ui->dspbFrequency);
        const QSignalBlocker BlockT(ui->dspbTime);
        const QSignalBlocker BlockD(ui->dialAngleRegulation);

        for (const ButtonParams &Button : m_lstMotionButtons)
        {
            if (Button.qlstMotionParams.isEmpty())
            {
                continue;
            }

            ui->dspbVoltage->setValue(Button.qlstMotionParams.first().value("voltage").toFloat());
            ui->dspbFrequency->setValue(Button.qlstMotionParams.first().value("frequency").toFloat());
            ui->dspbTime->setValue(Button.qlstMotionParams.first().value("timeuse").toFloat());

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
