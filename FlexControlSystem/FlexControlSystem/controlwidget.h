#ifndef CONTROLWIDGET_H
#define CONTROLWIDGET_H

#include <QWidget>
#include "controlpanel.h"
#include "rs485handle.h"

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

private:
    Ui::ControlWidget *ui;

    ControlPanel *m_pControlWidget;
    ControlPanel *m_pModeWidget;

    ConnectObject m_eObject;
};
#endif // CONTROLWIDGET_H
