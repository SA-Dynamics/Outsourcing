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

    // 通信对象与工作线程指针必须显式初始化, 否则连接失败分支会使用野指针
    m_ConnectiveHandler = nullptr;
    m_pConnectiveThread = nullptr;


    // 控制面板实例化, 一个用于配置, 一个用于控制
    m_pControlWidget = new ControlPanel(ui->ControlTabWidget);
    m_pModeWidget = new ControlPanel(ui->ModeTabWidget);
    m_pModeWidget->SetReadOnly(true);

    ui->tabControl->setEnabled(false);

    ui->pbOpenMode->setEnabled(false);
    ui->pbSaveMode->setEnabled(false);
    ui->pbSaveModeAs->setEnabled(false);
    ui->pbNewMode->setEnabled(false);
    ui->pbMode2Control->setEnabled(false);


    // 允许 ip:port 格式(冒号用于手动指定端口, 缺省 5000)
    QRegularExpression Regex("^[0-9.:]+$");
    QRegularExpressionValidator *Validator = new QRegularExpressionValidator(Regex, this);
    ui->leIP->setValidator(Validator);
}

ControlWidget::~ControlWidget()
{
    // 退出前停止通信线程并销毁通信对象, 避免退出时访问已释放的界面资源
    CloseConnection();

    delete ui;
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

        ui->tabControl->setEnabled(true);
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
void ControlWidget::CloseConnection(void)
{
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

        connect(m_ConnectiveHandler, &ConnectiveHandle::sigMotionTerminate, this, [this]() {
            emit m_pControlWidget->sigMotionTerminate();
        });

        qDebug() << "set slot after connected";
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

    emit m_pControlWidget->sigMotionTerminate();
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

void ControlWidget::on_tabPanel_currentChanged(int index)
{
    qDebug() << index;

    if (0 == index)
    {
        ui->pbOpenMode->setEnabled(false);
        ui->pbSaveMode->setEnabled(false);
        ui->pbSaveModeAs->setEnabled(false);
        ui->pbNewMode->setEnabled(false);
        ui->pbMode2Control->setEnabled(false);
        ui->pbControl2Mode->setEnabled(true);
    }
    else
    {
        ui->pbOpenMode->setEnabled(true);
        ui->pbSaveMode->setEnabled(true);
        ui->pbSaveModeAs->setEnabled(true);
        ui->pbNewMode->setEnabled(true);
        ui->pbMode2Control->setEnabled(true);
        ui->pbControl2Mode->setEnabled(false);
    }
}

void ControlWidget::on_pbControl2Mode_clicked()
{
    auto ret = QMessageBox::question(
        this,
        "确认",
        "确定要将控制参数覆盖到模式数据吗？",
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (ret == QMessageBox::Yes)
    {
        m_pModeWidget->CopyButtonParams(m_pControlWidget);
    }
    else
    {

    }
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
        m_pControlWidget->CopyButtonParams(m_pModeWidget);
    }
    else
    {

    }
}
