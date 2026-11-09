#include "groupmotion.h"
#include "ui_groupmotion.h"

#include <QComboBox>
#include <QDoubleSpinBox>

namespace
{
    // 下拉框里的动作类型顺序: 上下左右聚拢振散翻面水平居中垂直居中
    struct MotionItem
    {
        const char *pText;
        GeneralMotion::MotionParams::MotionIndex eMotion;
    };

    const MotionItem s_aMotionItems[] =
    {
        { "上",       GeneralMotion::MotionParams::MotionIndex::MotionUp },
        { "下",       GeneralMotion::MotionParams::MotionIndex::MotionDown },
        { "左",       GeneralMotion::MotionParams::MotionIndex::MotionLeft },
        { "右",       GeneralMotion::MotionParams::MotionIndex::MotionRight },
        { "聚拢",     GeneralMotion::MotionParams::MotionIndex::MotionGather },
        { "振散",     GeneralMotion::MotionParams::MotionIndex::MotionDisperse },
        { "翻面",     GeneralMotion::MotionParams::MotionIndex::MotionFlip },
        { "水平居中", GeneralMotion::MotionParams::MotionIndex::MotionHCentered },
        { "垂直居中", GeneralMotion::MotionParams::MotionIndex::MotionVCentered },
    };
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


void GroupMotion::AddMotionItem(QComboBox *pCombo, const QString &qstrText,
                                const GeneralMotion::MotionParams::MotionIndex eMotion)
{
    // 用 itemData 存枚举值, 取值时不用再按文本反查
    pCombo->addItem(qstrText, static_cast<int>(eMotion));
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
            AddMotionItem(pCombo, QString::fromUtf8(Item.pText), Item.eMotion);
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
}


QVector<GroupMotion::Step> GroupMotion::GetSteps(void) const
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

    for (int i = 0; i < 8; i++)
    {
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
