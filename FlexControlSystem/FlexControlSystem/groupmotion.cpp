#include "groupmotion.h"
#include "ui_groupmotion.h"

#include <QComboBox>
#include <QDoubleSpinBox>

namespace
{
    // 下拉框里的动作类型顺序: 上下左右聚拢振散翻面水平居中垂直居中, 最后是"停止"
    // iMotion 存 MotionIndex 的整数值; "停止"用 GroupMotion::MOTION_STOP(-1) 区分
    struct MotionItem
    {
        const char *pText;
        int iMotion;
    };

    const MotionItem s_aMotionItems[] =
    {
        { "上",       static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionUp) },
        { "下",       static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionDown) },
        { "左",       static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionLeft) },
        { "右",       static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionRight) },
        { "聚拢",     static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionGather) },
        { "振散",     static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionDisperse) },
        { "翻面",     static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionFlip) },
        { "水平居中", static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionHCentered) },
        { "垂直居中", static_cast<int>(GeneralMotion::MotionParams::MotionIndex::MotionVCentered) },
        { "停止",     GroupMotion::MOTION_STOP },
    };

    // S1~S8 的控件数组: 集中在一处取, 免得每个函数都重复一遍
    void GetStepWidgets(Ui::GroupMotion *pUi, QComboBox *apCombo[8], QDoubleSpinBox *apTime[8])
    {
        QComboBox *apC[8] =
        {
            pUi->cbS1, pUi->cbS2, pUi->cbS3, pUi->cbS4,
            pUi->cbS5, pUi->cbS6, pUi->cbS7, pUi->cbS8
        };

        QDoubleSpinBox *apT[8] =
        {
            pUi->dspS1Time, pUi->dspS2Time, pUi->dspS3Time, pUi->dspS4Time,
            pUi->dspS5Time, pUi->dspS6Time, pUi->dspS7Time, pUi->dspS8Time
        };

        for (int i = 0; i < 8; i++)
        {
            apCombo[i] = apC[i];
            apTime[i] = apT[i];
        }
    }
}


GroupMotion::GroupMotion(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GroupMotion)
{
    ui->setupUi(this);

    InitComboBoxes();
}

GroupMotion::~GroupMotion()
{
    delete ui;
}


void GroupMotion::AddMotionItem(QComboBox *pCombo, const QString &qstrText, const int iMotion)
{
    // 用 itemData 存枚举值(或 MOTION_STOP), 取值时不用再按文本反查
    pCombo->addItem(qstrText, iMotion);
}


void GroupMotion::InitComboBoxes(void)
{
    QComboBox *apCombo[8] =
    {
        ui->cbS1, ui->cbS2, ui->cbS3, ui->cbS4,
        ui->cbS5, ui->cbS6, ui->cbS7, ui->cbS8
    };

    for (QComboBox *pCombo : apCombo)
    {
        for (const MotionItem &Item : s_aMotionItems)
        {
            AddMotionItem(pCombo, QString::fromUtf8(Item.pText), Item.iMotion);
        }
    }

    QDoubleSpinBox *apTime[8] =
    {
        ui->dspS1Time, ui->dspS2Time, ui->dspS3Time, ui->dspS4Time,
        ui->dspS5Time, ui->dspS6Time, ui->dspS7Time, ui->dspS8Time
    };

    for (QDoubleSpinBox *pTime : apTime)
    {
        pTime->setDecimals(2);
        pTime->setRange(0.0, 60.0);
        pTime->setSingleStep(0.1);
        pTime->setSuffix(" s");
        pTime->setValue(0.5);       // 默认每步 0.5s, 0 表示该步不执行
    }

    // 选到"停止"后, 该步及之后的步都不会执行, 对应的时间输入要置灰
    // (填充完选项再连接, 避免 addItem 期间 currentIndexChanged 反复触发)
    for (QComboBox *pCombo : apCombo)
    {
        connect(pCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this]() { UpdateTimeEnabled(); });
    }

    UpdateTimeEnabled();
}


// 第一个"停止"步及它后面的时间输入置灰; 没有"停止"则全部可用
void GroupMotion::UpdateTimeEnabled(void)
{
    QComboBox *apCombo[8];
    QDoubleSpinBox *apTime[8];
    GetStepWidgets(ui, apCombo, apTime);

    int iStopAt = 8;
    for (int i = 0; i < 8; i++)
    {
        if (apCombo[i]->currentData().toInt() == MOTION_STOP)
        {
            iStopAt = i;
            break;
        }
    }

    for (int i = 0; i < 8; i++)
    {
        apTime[i]->setEnabled(i < iStopAt);
    }
}


QVector<GroupMotion::Step> GroupMotion::GetSteps(void) const
{
    QVector<Step> lstSteps;

    QComboBox *apCombo[8];
    QDoubleSpinBox *apTime[8];
    GetStepWidgets(ui, apCombo, apTime);

    for (int i = 0; i < 8; i++)
    {
        // "停止": 该步及它后面的所有步都取消运动, 序列到此为止
        if (apCombo[i]->currentData().toInt() == MOTION_STOP)
        {
            break;
        }

        const float fTime = static_cast<float>(apTime[i]->value());

        // 时间为 0(或未填)的步视为不使用, 跳过, 不下发空指令
        if (fTime <= 0.0f)
        {
            continue;
        }

        Step sStep;
        sStep.eMotion = static_cast<GeneralMotion::MotionParams::MotionIndex>(
                            apCombo[i]->currentData().toInt());
        sStep.fTimeUse = fTime;

        lstSteps.append(sStep);
    }

    return lstSteps;
}


QVector<GroupMotion::Step> GroupMotion::GetAllSteps(void) const
{
    QVector<Step> lstSteps;

    QComboBox *apCombo[8] =
    {
        ui->cbS1, ui->cbS2, ui->cbS3, ui->cbS4,
        ui->cbS5, ui->cbS6, ui->cbS7, ui->cbS8
    };

    QDoubleSpinBox *apTime[8] =
    {
        ui->dspS1Time, ui->dspS2Time, ui->dspS3Time, ui->dspS4Time,
        ui->dspS5Time, ui->dspS6Time, ui->dspS7Time, ui->dspS8Time
    };

    // 与 GetSteps 的区别: 这里不跳过时间为 0 的步, 保证 8 步能原样存回 ini
    for (int i = 0; i < 8; i++)
    {
        Step sStep;
        sStep.eMotion = static_cast<GeneralMotion::MotionParams::MotionIndex>(
                            apCombo[i]->currentData().toInt());
        sStep.fTimeUse = static_cast<float>(apTime[i]->value());

        lstSteps.append(sStep);
    }

    return lstSteps;
}


void GroupMotion::SetStep(const int iIndex, const int iMotion, const float fTimeUse)
{
    if (iIndex < 0 || iIndex >= 8)
    {
        return;
    }

    QComboBox *apCombo[8] =
    {
        ui->cbS1, ui->cbS2, ui->cbS3, ui->cbS4,
        ui->cbS5, ui->cbS6, ui->cbS7, ui->cbS8
    };

    QDoubleSpinBox *apTime[8] =
    {
        ui->dspS1Time, ui->dspS2Time, ui->dspS3Time, ui->dspS4Time,
        ui->dspS5Time, ui->dspS6Time, ui->dspS7Time, ui->dspS8Time
    };

    // 按枚举值反查下拉项: 找不到(例如 ini 里存的是已删除的动作类型)就保留原选项
    const int iItemIndex = apCombo[iIndex]->findData(iMotion);
    if (iItemIndex >= 0)
    {
        apCombo[iIndex]->setCurrentIndex(iItemIndex);
    }

    apTime[iIndex]->setValue(static_cast<double>(fTimeUse));
}


void GroupMotion::CopyFrom(const GroupMotion *pOther)
{
    if (pOther == nullptr || pOther == this)
    {
        return;
    }

    QComboBox *apCombo[8] =
    {
        ui->cbS1, ui->cbS2, ui->cbS3, ui->cbS4,
        ui->cbS5, ui->cbS6, ui->cbS7, ui->cbS8
    };

    QComboBox *apOtherCombo[8] =
    {
        pOther->ui->cbS1, pOther->ui->cbS2, pOther->ui->cbS3, pOther->ui->cbS4,
        pOther->ui->cbS5, pOther->ui->cbS6, pOther->ui->cbS7, pOther->ui->cbS8
    };

    QDoubleSpinBox *apTime[8] =
    {
        ui->dspS1Time, ui->dspS2Time, ui->dspS3Time, ui->dspS4Time,
        ui->dspS5Time, ui->dspS6Time, ui->dspS7Time, ui->dspS8Time
    };

    QDoubleSpinBox *apOtherTime[8] =
    {
        pOther->ui->dspS1Time, pOther->ui->dspS2Time, pOther->ui->dspS3Time, pOther->ui->dspS4Time,
        pOther->ui->dspS5Time, pOther->ui->dspS6Time, pOther->ui->dspS7Time, pOther->ui->dspS8Time
    };

    for (int i = 0; i < 8; i++)
    {
        apCombo[i]->setCurrentIndex(apOtherCombo[i]->currentIndex());
        apTime[i]->setValue(apOtherTime[i]->value());
    }
}
