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
//    virtual void SetConnectiveInfo(const QString &qstrInfo) = 0;
//    virtual void SetupConnective(void) = 0;
    virtual void SendCommand(const QByteArray &qbtData, const QByteArray &qbtRespond) = 0;

private slots:
    virtual void SetupConnective(const QString &qstrInfo) = 0;

signals:
    void sigSetUpConnective(const QString &qstrInfo);
    void sigConnectiveState(const bool &bState);
};

#endif // CONNECTIVEHANDLE_H
