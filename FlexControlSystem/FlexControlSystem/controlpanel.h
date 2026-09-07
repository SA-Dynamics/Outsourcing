#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QButtonGroup>
#include <QPushButton>

#include "ProtocolHandle.h"
#include "ConnectiveHandle.h"


namespace Ui {
class ControlPanel;
}

class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr);
    ~ControlPanel();

private:
    Ui::ControlPanel *ui;

    QButtonGroup *m_pMotionGroup;
    QList<QPushButton *> m_lstMotionButtons;

public slots:
    void OnMotionGroupButtonClicked(QAbstractButton *pButton);

signals:
    void sigMotion(const MotionIndex &eIndex);
};

#endif // CONTROLPANEL_H
