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
    };


    GetSettingParams();

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


void ControlPanel::GetSettingParams(void)
{
    QString qstrSettingFile = QCoreApplication::applicationDirPath() + "/DefaultConfig.ini";

    if (!QFile::exists(qstrSettingFile))
    {
        qWarning() << "配置文件不存在，使用默认值:" << qstrSettingFile;
        return;
    }

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
            },
        };

        Settings.endGroup();
    }
}


void ControlPanel::OnMotionGroupButtonClicked(QAbstractButton *pButton)
{
    for (const ButtonParams &Button : m_lstMotionButtons)
    {
        if (pButton == Button.pButton)
        {
            m_qstrCurrentMotionButton = Button.qstrName;
            // 将当前按钮的运动参数更新到界面上
            ui->dspbVoltage->setValue(Button.qlstMotionParams.first().value("voltage"));
            ui->dspbFrequency->setValue(Button.qlstMotionParams.first().value("frequency"));
            ui->dspbTime->setValue(Button.qlstMotionParams.first().value("timeuse"));
        }
    }
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
            qDebug() << "send motion";
            // 发送运动信号
            QByteArray qbtData;
            QByteArray qbtRespond;

            GeneralMotion::MotionParams sParams;
            sParams.eIndex = Button.eMotion;
            sParams.fVoltage = Button.qlstMotionParams.first().value("voltage");
            sParams.fFrequency = Button.qlstMotionParams.first().value("frequency");
            sParams.fTimeUse = Button.qlstMotionParams.first().value("timeuse");

            m_pProtocol->MotionControlProtocol(sParams, qbtData, qbtRespond);

            emit sigMotion(qbtData, qbtRespond);
        }
    }
}

void ControlPanel::on_pbSendMotionStop_clicked()
{

}
