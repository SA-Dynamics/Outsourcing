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
        QList<QMap<QString, QVariant>> qlstMotionParams;
    };

    void SetReadOnly(const bool bReadOnly);
    void SaveSettings(void);
    void SaveSettings(const QString &qstrFile);
    void LoadSettings(const QString &qstrFile);
    void NewSettings(const QString &qstrFile);
    void CopyButtonParams(const ControlPanel *pOther);

private:
    Ui::ControlPanel *ui;

    QButtonGroup *m_pMotionGroup;
    QList<ButtonParams> m_lstMotionButtons;
    QString m_qstrCurrentMotionButton;
    void GetSettingParams(const QString &qstrFileName);

    ProtocolHandle *m_pProtocol;
    QString m_qstrCurrentSettingPath;

    void SetDialDirection(const GeneralMotion::MotionParams::MotionIndex eIndex, const int16_t s16Angle);
    int16_t ChangeDialDirection(const GeneralMotion::MotionParams::MotionIndex eIndex, const int16_t s16DialValue);

public slots:
    void OnMotionGroupButtonClicked(QAbstractButton *pButton);
    void OnMotionFinished(void);

signals:
    void sigMotion(const QByteArray &qbtData, const QByteArray &qbtRespond);
    void sigStopMotion();
    void sigMotionTerminate();
    // 料仓: 下位机运动完成后回帧, 超时按时长+冗余计算
    void sigBinMotion(const QByteArray &qbtData, const QByteArray &qbtRespond);
    void sigBinStopMotion();
    // 光源: 开启后 1s 内必须收到下位机完成反馈
    void sigLightOn(const QByteArray &qbtData, const QByteArray &qbtRespond);
    void sigLightOff(const QByteArray &qbtData, const QByteArray &qbtRespond);

private slots:
    void on_dspbVoltage_valueChanged(double arg1);
    void on_dspbFrequency_valueChanged(double arg1);
    void on_dspbTime_valueChanged(double arg1);
    void on_pbSendMotion_clicked();
    void on_pbSendMotionStop_clicked();
    void on_dialAngleRegulation_valueChanged(int value);
    void on_hsldSpeed_valueChanged(int value);
    void on_pbBinStartMotion_clicked();
    void on_pbBinStopMotion_clicked();
    void on_pbOpenLightA_clicked();
    void on_pbCloseLightA_clicked();
    void on_pbOpenLightB_clicked();
    void on_pbCloseLihgtB_clicked();
};

#endif // CONTROLPANEL_H
