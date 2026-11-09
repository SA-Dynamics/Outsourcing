#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QButtonGroup>
#include <QPushButton>
#include <QTimer>

#include "generalprotocol.h"
//#include "ConnectiveHandle.h"

class GroupMotion;

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

    // 只读面板(模式数据面板): 其信号未连接通信引擎, 只能编辑/保存参数, 不允许下发指令
    bool m_bReadOnly = false;

    // ---- pbGather 交替序列 ----
    // 点击"发送"且当前选中 Gather 时, 不下发 Gather 自身参数,
    // 而是交替下发 pbHCentered / pbVCentered 的参数, 逐段累计运动时长到
    // Gather 按钮自身 timeuse 为止(至少 1 段)。
    // 段时长与间隔取模式数据面板的三个控件(单位秒, 构造函数里设默认值):
    //   ui->dspHMotionTime 水平居中段时长 / ui->dspVMotionTime 垂直居中段时长
    //   / ui->dspIntervalTime 段间静止间隔(0 = 无缝衔接)。
    // 序列逐段入队(引擎串行下发, 每段等下位机完成回帧后才发下一段, 节拍无累计漂移);
    // 间隔段实现为一条 0V 的运动指令: 下位机占空比为 0、电机断电,
    // 按间隔时长计时后照常回完成帧, 引擎的节拍与应答匹配逻辑完全不变;
    // 每个命令段(运动或间隔)完成都会触发一次 sigMotionTerminate,
    // 只有最后一个命令(计数归零)才恢复界面按钮。
    quint32 m_u32MotionSeqRemaining = 0;                   // 序列中尚未完成的命令数(运动段+间隔段)
    QTimer *m_pMotionSeqWatchdog = nullptr;                // 异常兜底(响应超时清队等导致计数卡住时强制恢复)
    void SendButtonMotion(const ButtonParams &Button, const float fTimeUseOverride, const float fVoltageOverride = -1.0f);
    void StartGatherSequence(void);
    void ResetMotionSequence(void);

    // ---- 组合运动 G1 / G2 ----
    // G1Widget / G2Widget 里各嵌入一个 GroupMotion 表单(S1~S8: 动作类型 + 运行时间)。
    // 点 pbG1/pbG2 时按 S1..S8 的顺序逐条下发: 电压/频率/角度/ModifyCoeff 取该运动类型
    // 自己的参数, 只有执行时间取表单里填的时间。
    // 与交替序列共用同一套"一次性入队 + 完成帧驱动 + 计数恢复按钮"机制。
    GroupMotion *m_pGroupMotion1 = nullptr;
    GroupMotion *m_pGroupMotion2 = nullptr;
    void StartGroupMotionSequence(GroupMotion *pGroupMotion, const QString &qstrGroupName);
    const ButtonParams *FindButtonByMotion(const GeneralMotion::MotionParams::MotionIndex eMotion) const;

    // ---- ModifyCoeff(位置修正系数) ----
    // 运动参数里的 speed 已取消, 换成 ModifyCoeff(有符号 int8, -50~50)。
    // 它不再是"每个按钮各存一份"的参数, 而是两个全局滑块:
    //   水平居中(MotionHCentered) 取 ui->vsHModify(竖直滑块, 调水平位置)
    //   垂直居中(MotionVCentered) 取 ui->hsVModify(水平滑块, 调垂直位置)
    // 其余运动一律发 0。
    int8_t GetModifyCoeff(const GeneralMotion::MotionParams::MotionIndex eMotion) const;

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
    void on_pbG1_clicked();
    void on_pbG2_clicked();
    void on_dialAngleRegulation_valueChanged(int value);
    void on_hsldSpeed_valueChanged(int value);          // 已废弃: speed 参数取消, 控件也隐藏, 保留只为不破坏 .ui 连接
    void on_hsVModify_valueChanged(int value);          // 垂直居中位置调节(水平滑块)
    void on_vsHModify_valueChanged(int value);          // 水平居中位置调节(竖直滑块)
    void on_pbBinStartMotion_clicked();
    void on_pbBinStopMotion_clicked();
    void on_pbOpenLightA_clicked();
    void on_pbCloseLightA_clicked();
    void on_pbOpenLightB_clicked();
    void on_pbCloseLihgtB_clicked();
};

#endif // CONTROLPANEL_H
