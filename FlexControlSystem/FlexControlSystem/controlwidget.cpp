#include "controlwidget.h"
#include "ui_controlwidget.h"
#include <QDebug>
#include <QRegularExpressionValidator>


ControlWidget::ControlWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ControlWidget)
{
    ui->setupUi(this);

    m_eConnectObject = ConnectObject::ObjectNone;



    m_pControlWidget = new ControlPanel(ui->ControlTabWidget);
    m_pModeWidget = new ControlPanel(ui->ModeTabWidget);

//    m_pRS485Handler = new RS485Handle();
//    m_pTCPHandler = new TCPHandle();
//    m_pRS485Thread = new QThread(this);
//    m_pTCPThread = new QThread(this);
//    m_pRS485Handler->moveToThread(m_pRS485Thread);
//    m_pTCPHandler->moveToThread(m_pTCPThread);


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

void ControlWidget::OnMotionButtonClicked(const MotionIndex &eIndex)
{
    switch (m_eConnectObject)
    {
        case ConnectObject::ObjectTCP:

        break;

        case ConnectObject::Object485:

        break;

        default:
            break;
    }
}


void ControlWidget::SetConnectSlot(const bool bSet)
{
    if (bSet)
    {
        connect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));
    }
    else
    {

    }
}


void ControlWidget::on_pbTCPConnect_clicked()
{
    disconnect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));

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


void ControlWidget::OnConnectiveState(const bool &bState)
{
    if (bState)
    {

    }
}

void ControlWidget::on_pbRS485Connect_clicked()
{
//    disconnect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));

    m_ConnectiveHandler = new RS485Handle();
    m_pConnectiveThread = new QThread();
    m_ConnectiveHandler->moveToThread(m_pConnectiveThread);

    connect(m_ConnectiveHandler, SIGNAL(sigConnectiveState(bool)), this, SLOT(OnConnectiveState(bool)));

    m_eConnectObject = ConnectObject::Object485;

    m_pConnectiveThread->start();

    emit m_ConnectiveHandler->sigSetUpConnective(ui->cbCOM->currentText());

}


void ControlWidget::on_pbRS485Disconnect_clicked()
{
    if (m_ConnectiveHandler && (m_eConnectObject == ConnectObject::Object485))
    {
        m_ConnectiveHandler->deleteLater();
        m_pConnectiveThread->quit();
        m_pConnectiveThread->wait();
    }
}
