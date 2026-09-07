#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include <QThread>
#include "controlpanel.h"
#include "rs485handle.h"
#include "tcphandle.h"
#include "ProtocolHandle.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ControlWidget; }
QT_END_NAMESPACE

class ControlWidget : public QWidget
{
    Q_OBJECT

public:

    enum class ConnectObject
    {
        ObjectNone,
        ObjectTCP,
        Object485
    };
    Q_ENUM(ConnectObject)

    ControlWidget(QWidget *parent = nullptr);
    ~ControlWidget();

private slots:
    void on_pbUpdatePort_clicked();
    void on_leIP_textChanged(const QString &arg1);
    void OnMotionButtonClicked(const MotionIndex &eIndex);

    void on_pbTCPConnect_clicked();
    void OnConnectiveState(const bool &bState);

    void on_pbTCPDisconnect_clicked();

private:
    Ui::ControlWidget *ui;
    ConnectiveHandle *m_ConnectiveHandler;
    QThread *m_pConnectiveThread;
    ControlPanel *m_pControlWidget;
    ControlPanel *m_pModeWidget;
    ConnectObject m_eConnectObject;

    void SetConnectSlot(const bool bSet);
};
#endif // CONTROLWIDGET_H
