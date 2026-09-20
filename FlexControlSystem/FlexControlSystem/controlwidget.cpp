#include "controlwidget.h"
#include "ui_controlwidget.h"
#include <QDebug>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QFileDialog>

ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlWidget)
{
    ui->setupUi(this);

    m_eConnectObject = ConnectObject::ObjectNone;


    // 控制面板实例化, 一个用于配置, 一个用于控制
    m_pControlWidget = new ControlPanel(ui->ControlTabWidget);
    m_pModeWidget = new ControlPanel(ui->ModeTabWidget);
    m_pModeWidget->SetReadOnly(true);

    ui->tabControl->setEnabled(false);


    QRegularExpression Regex("^[0-9.]+$");
    QRegularExpressionValidator *Validator = new QRegularExpressionValidator(Regex, this);
    ui->leIP->setValidator(Validator);
}

ControlWidget::~ControlWidget()
{
    delete ui;
}


void ControlWidget::on_pbUpdatePort_clicked()
{
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

        ui->tabControl->setEnabled(true);
    }
    else
    {
        // 连接不成功, 弹出提示, 结束线程
        QMessageBox::critical(this, "错误", "目标连接失败");
        m_ConnectiveHandler->deleteLater();
        m_pConnectiveThread->quit();
        m_pConnectiveThread->wait();

        m_eConnectObject = ConnectObject::ObjectNone;
    }
}


void ControlWidget::SetConnectSlot(const bool bSet)
{
    if (bSet)
    {
        connect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));
        connect(m_pControlWidget, SIGNAL(sigMotion(QByteArray, QByteArray)), m_ConnectiveHandler, SLOT(SendMotionCommand(QByteArray, QByteArray)));
        connect(m_pControlWidget, SIGNAL(sigStopMotion()), m_ConnectiveHandler, SLOT(SendStopMotionCommand()));
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigHeartBeatUnnormal, this, [this]() {
            OnConnectiveUnnormal("HeartBeat");
        });
        connect(m_ConnectiveHandler, &ConnectiveHandle::sigMotionRespondUnnormal, this, [this]() {
            OnConnectiveUnnormal("MotionRespond");
        });
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
        if (m_ConnectiveHandler && (m_eConnectObject == ConnectObject::Object485))
        {
            m_ConnectiveHandler->deleteLater();
            m_pConnectiveThread->quit();
            m_pConnectiveThread->wait();

            ui->pbRS485Connect->setEnabled(true);
            ui->pbRS485Disconnect->setEnabled(false);
            ui->pbUpdatePort->setEnabled(true);
        }
    }
    else if (qstrName == "MotionRespond")
    {
        QMessageBox::critical(this, "错误", "动作反馈超时");
    }
}



void ControlWidget::on_pbTCPConnect_clicked()
{
    m_ConnectiveHandler = new TCPHandle();
    m_pConnectiveThread = new QThread();
    m_ConnectiveHandler->moveToThread(m_pConnectiveThread);

    connect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));
}




void ControlWidget::on_pbTCPDisconnect_clicked()
{
    if (m_ConnectiveHandler && (m_eConnectObject == ConnectObject::ObjectTCP))
    {
        m_ConnectiveHandler->deleteLater();
        m_pConnectiveThread->quit();
        m_pConnectiveThread->wait();
    }
}



void ControlWidget::on_pbRS485Connect_clicked()
{
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
        m_ConnectiveHandler->deleteLater();
        m_pConnectiveThread->quit();
        m_pConnectiveThread->wait();

        ui->pbRS485Connect->setEnabled(true);
        ui->pbRS485Disconnect->setEnabled(false);
        ui->pbUpdatePort->setEnabled(true);
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
    m_pModeWidget->SaveSettings();
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
