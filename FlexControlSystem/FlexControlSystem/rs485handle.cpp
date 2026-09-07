#include "rs485handle.h"

RS485Handle::RS485Handle()
{
    m_bConnect = false;
    m_pSerial = nullptr;

    m_pProtocolHandler = new GeneralProtocol();


    m_qbtRecvBuffer.clear();

    m_pSendDataTimer = new QTimer(this);
    connect(m_pSendDataTimer, SIGNAL(timeout()), this, SLOT(OnSendDataTimerTimeout()));
    m_pSendDataTimer->start(10);

    m_pHeartBeatTimer = new QTimer(this);
    connect(m_pHeartBeatTimer, SIGNAL(timeout()), this, SLOT(OnHeartBeatTimerTimeout()));
    m_pHeartBeatTimer->start(1000);
}


RS485Handle::~RS485Handle()
{
    if (m_bConnect)
    {
        m_pSerial->close();
    }
}


void RS485Handle::SetConnectiveInfo(const QString &qstrInfo)
{
    m_qstrCurrentPort = qstrInfo;
}


void RS485Handle::SetupConnective(void)
{
    m_pSerial = new QSerialPort();
    m_pSerial->setPortName(m_qstrCurrentPort);              // 设置端口名
    m_pSerial->setBaudRate(QSerialPort::Baud115200);        // 设置波特率
    m_pSerial->setDataBits(QSerialPort::Data8);             // 设置数据位
    m_pSerial->setParity(QSerialPort::NoParity);            // 设置校验位
    m_pSerial->setStopBits(QSerialPort::OneStop);           // 设置停止位

    if (m_pSerial->open(QIODevice::ReadWrite))
    {
        // 打开成功
        m_bConnect = true;
        connect(m_pSerial, SIGNAL(m_pSerial->readyRead()), this, SLOT(ReceivedDataHandler()));
    }

    emit sigConnectiveState(m_bConnect);

}


void RS485Handle::SendCommand(const QByteArray &qbtData)
{
    m_qbtRecvBuffer.clear();
    m_pSerial->write(qbtData);
}


void RS485Handle::ReceivedDataHandler(void)
{
    m_qbtRecvBuffer.append(m_pSerial->readAll());

    // 假设协议规定每帧数据为 15 个字节
//    const int FRAME_SIZE = 15;
//    while (buffer.size() >= FRAME_SIZE)
//    {
//        QByteArray frame = buffer.left(FRAME_SIZE);
//        buffer.remove(0, FRAME_SIZE);
//        processFrame(frame); // 处理完整帧
//    }
}


void RS485Handle::OnHeartBeatTimerTimeout(void)
{
    // 发送心跳
}

void RS485Handle::OnSendDataTimerTimeout(void)
{
    // 发送心跳
}
