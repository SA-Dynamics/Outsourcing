#include "rs485handle.h"
#include <QDebug>

RS485Handle::RS485Handle()
    : ConnectiveHandle()
    , m_pSerial(nullptr)
{
}


RS485Handle::~RS485Handle()
{
    CloseChannel();
}


bool RS485Handle::OpenChannel(const QString &qstrInfo)
{
    // 重复打开前先清理旧对象
    CloseChannel();

    m_pSerial = new QSerialPort(this);
    m_pSerial->setPortName(qstrInfo);                       // 设置端口名
    m_pSerial->setBaudRate(QSerialPort::Baud115200);        // 设置波特率
    m_pSerial->setDataBits(QSerialPort::Data8);             // 设置数据位
    m_pSerial->setParity(QSerialPort::NoParity);            // 设置校验位
    m_pSerial->setStopBits(QSerialPort::OneStop);           // 设置停止位

    if (!m_pSerial->open(QIODevice::ReadWrite))
    {
        qWarning() << "open serial failed:" << qstrInfo << m_pSerial->errorString();
        delete m_pSerial;
        m_pSerial = nullptr;
        return false;
    }

    connect(m_pSerial, &QSerialPort::readyRead, this, &RS485Handle::ReceivedDataHandler);
    return true;
}


void RS485Handle::CloseChannel()
{
    if (m_pSerial)
    {
        m_pSerial->close();
        delete m_pSerial;
        m_pSerial = nullptr;
    }
}


bool RS485Handle::WriteData(const QByteArray &qbtData)
{
    if (!m_pSerial || !m_pSerial->isOpen())
    {
        return false;
    }

    return (m_pSerial->write(qbtData) != -1);
}


void RS485Handle::ReceivedDataHandler(void)
{
    // 原始字节流交给基类状态机处理
    FeedReceivedBytes(m_pSerial->readAll());
}
