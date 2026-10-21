#include "controlpanel.h"
#include "ui_controlpanel.h"
#include <QDebug>
#include <QFile>
#include <QSettings>



ControlPanel::ControlPanel(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ControlPanel)
{
//    ui->setupUi(this);

    ui->setupUi(parent);

    ui->dialAngleRegulation->setTracking(true);

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
    connect(ui->pbSendMotion, SIGNAL(clicked()), this, SLOT(on_pbSendMotion_clicked()));

    connect(ui->dialAngleRegulation, &QDial::valueChanged, this, &ControlPanel::on_dialAngleRegulation_valueChanged);
    connect(ui->hsldSpeed, &QDial::valueChanged, this, &ControlPanel::on_hsldSpeed_valueChanged);
}

ControlPanel::~ControlPanel()
{
    delete ui;
}


void ControlPanel::SetReadOnly(const bool bReadOnly)
{
    if (bReadOnly)
    {
        ui->pbSendMotion->setEnabled(false);
        ui->pbSendMotionStop->setEnabled(false);
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

    for (ButtonParams &Button : m_lstMotionButtons)
    {
        Settings.beginGroup(Button.qstrName);

        Button.qlstMotionParams =
        {
            {
                {"frequency", Settings.value("frequency").toFloat()},
                {"timeuse", Settings.value("timeuse").toFloat()},
                {"voltage", Settings.value("voltage").toFloat()},
                {"angle_offset", Settings.value("angle_offset").toInt()},
                {"speed", Settings.value("speed").toInt()},
            },
        };

        Settings.endGroup();
    }
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

            ui->hsldSpeed->setValue(Button.qlstMotionParams.first().value("speed").toInt());
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


void ControlPanel::on_pbSendMotion_clicked()
{    
    for (ButtonParams &Button : m_lstMotionButtons)
    {
        if (m_qstrCurrentMotionButton == Button.qstrName && !Button.qlstMotionParams.isEmpty())
        {        
            // 发送运动信号
            QByteArray qbtData;
            QByteArray qbtRespond;

            GeneralMotion::MotionParams sParams;
            sParams.eIndex = Button.eMotion;
            sParams.fVoltage = Button.qlstMotionParams.first().value("voltage").toFloat();
            sParams.fFrequency = Button.qlstMotionParams.first().value("frequency").toFloat();
            sParams.fTimeUse = Button.qlstMotionParams.first().value("timeuse").toFloat();

            m_pProtocol->MotionControlProtocol(sParams, qbtData, qbtRespond);

            emit sigMotion(qbtData, qbtRespond);
        }
    }
}


void ControlPanel::on_pbSendMotionStop_clicked()
{
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
        Settings.setValue("speed", Button.qlstMotionParams.first().value("speed"));

        Settings.endGroup();
    }
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
    qDebug() << value;
}
