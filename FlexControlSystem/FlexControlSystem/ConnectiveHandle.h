#ifndef CONNECTIVEHANDLE_H
#define CONNECTIVEHANDLE_H

#include <QObject>

class ConnectiveHandle : public QObject
{
    Q_OBJECT

public:
//    explicit ConnectiveHandle(QObject *parent = nullptr);
    virtual ~ConnectiveHandle()
    {

    }

public slots:
    virtual void SetConnectiveInfo(const QString &qstrInfo) = 0;
    virtual void SetupConnective(void) = 0;
    virtual void SendCommand(const QByteArray &qbtData) = 0;

signals:
    void sigConnectiveState(const bool &bState);
};

#endif // CONNECTIVEHANDLE_H
