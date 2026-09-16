#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QButtonGroup>
#include <QPushButton>

#include "generalprotocol.h"
//#include "ConnectiveHandle.h"


namespace Ui {
class ControlPanel;
}

class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr);
    ~ControlPanel();

    struct ButtonParams
    {
        QPushButton *pButton;
        QString qstrName;
        GeneralMotion::MotionParams::MotionIndex eMotion;
        QList<QMap<QString, float>> qlstMotionParams;
    };

    void SetReadOnly(const bool bReadOnly);

private:
    Ui::ControlPanel *ui;

    QButtonGroup *m_pMotionGroup;
    QList<ButtonParams> m_lstMotionButtons;
    QString m_qstrCurrentMotionButton;
    void GetSettingParams(void);

    ProtocolHandle *m_pProtocol;

public slots:
    void OnMotionGroupButtonClicked(QAbstractButton *pButton);

signals:
    void sigMotion(const QByteArray &qbtData, const QByteArray &qbtRespond);

private slots:
    void on_dspbVoltage_valueChanged(double arg1);
    void on_dspbFrequency_valueChanged(double arg1);
    void on_dspbTime_valueChanged(double arg1);
    void on_pbSendMotion_clicked();
    void on_pbSendMotionStop_clicked();
};

#endif // CONTROLPANEL_H
