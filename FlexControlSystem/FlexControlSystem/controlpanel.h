#ifndef CONTROLPANEL_H
#define CONTROLPANEL_H

#include <QWidget>
#include <QButtonGroup>
#include <QPushButton>
#include <QTimer>
#include <QList>
#include <QPixmap>

#include "generalprotocol.h"
//#include "ConnectiveHandle.h"

class GroupMotion;
class QDoubleSpinBox;
class QLabel;

namespace Ui {
class ControlPanel;
}

class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget *parent = nullptr, const bool bFlatRun = false);
    ~ControlPanel();

    struct ButtonParams
    {
        QPushButton *pButton;
        QString qstrName;
        GeneralMotion::MotionParams::MotionIndex eMotion;
        QList<QMap<QString, QVariant>> qlstMotionParams;
    };

    // ---- 运动序列状态(2026-11-25 二十五次: 两个面板必须看见同一份) ----
    // 运行页与"配置数据"窗口里的面板共用同一个通信引擎。以前两边各记各的计数,
    // 切换窗口(抄按钮态)时必然清掉其中一方的计数: 第一个完成帧就把按钮解锁,
    // 而电机还在跑; 聚拢"保持"下计数永远不归零, 这个错位会一直持续下去
    // (表现为"停了却锁着"或"跑着却已解锁, 还能再点发送")。
    // 所以计数与节奏放在一份状态里, ControlWidget 让两个面板指向同一个对象;
    // 单面板使用(以及测试)时各指向自己的 m_ownSequence, 行为与以前完全一致。
    // 只有 pOwner(发起者)处理完成帧(递减/续队), 收尾时两个面板一起回空闲 ——
    // 否则同一次完成帧会被两个面板各减一次。
    struct MotionSequenceState
    {
        quint32 u32Remaining = 0;        // 尚未完成的命令数(运动段 + 间隔段)
        bool bRepeat = false;            // 聚拢"保持": 无限交替(见下)
        int iSegHMs = 0;                 // 水平居中段时长快照(ms)
        int iSegVMs = 0;                 // 垂直居中段时长快照(ms)
        int iGapMs = 0;                  // 段间间隔快照(ms)
        int iRoundMs = 0;                // 一轮墙钟时长(重启看门狗用)
        quint32 u32RoundCmds = 0;        // 一轮的命令条数(运动段 + 间隔段)
        ControlPanel *pOwner = nullptr;  // 发起者: 只有它处理完成帧(递减/续队)
    };

    // 旧接口: 只读 = 本面板不允许下发运行指令。现在等价于 SetRunEnabled(false),
    // 保留只为兼容既有调用点/测试("配置数据"面板打开初期就是不可发的)
    void SetReadOnly(const bool bReadOnly);
    // 本面板是否允许下发运行指令(运动/料仓/光源)。
    // 真正的判据只有"链路是否可用": 运行页与"配置数据"窗口里的面板都一样 ——
    // 2026-11-25 二十次 用户要求配置窗口里配完直接发一条看效果, 不再要求返回运行页。
    // 由 ControlWidget 在连接成功/断连时统一通知两个实例。
    void SetRunEnabled(const bool bEnabled);
    // 是否有动作序列正在执行(运动段/间隔段/组合运动都算);
    // 退出确认等场景需要据此提示"下位机会继续跑完当前指令"
    bool IsMotionRunning(void) const;
    // 料仓是否正在运动(空闲时料仓是"可开/不可停", 只有停止键可用才说明在动)
    bool IsBinRunning(void) const;
    // 把另一个面板的"运行类"按钮态抄过来: 发送/停止、料仓一对、光源四对。
    // 用途: "配置数据"窗口打开时让配置面板跟运行页对齐(哪盏灯亮着、什么在跑),
    // 关闭时再抄回去 —— 两个实例各自记自己的按钮态, 不同步就会出现
    // "运行页上灯已经关了, 配置窗口还认为它亮着, 关闭键点不动"这种错位。
    // 运动类型按钮不参与: 配置页要随时能点选切换编辑对象, 不受运行锁影响。
    void SyncRunButtonStates(const ControlPanel *pSource);
    // 两个面板共享同一份运动序列状态(ControlWidget 构造时调用一次)。
    // 不调用时的行为与以前完全一致(各记各的), 单面板使用与测试不受影响。
    void UseSharedSequence(MotionSequenceState *pState, ControlPanel *pPeerPanel);
    // 断连/重连时清理运动锁(下位机侧在飞指令随链路一起作废, 界面必须回到空闲态,
    // 否则运动类型按钮会一直停在禁用态, 重连后怎么点都没反应)
    void ResetMotionUiState(void);
    // 断连时清理"固件参数"页的等待态: 获取/保存的应答不会再来, 按钮必须放开,
    // 否则重连后"获取配置/保存配置"永远点不动
    void ResetKeyParamsUiState(void);
    // 固件参数页是否可用(仅连接成功后可用)。
    // 注意本类不知道连接状态: "配置数据"窗口里的只读面板与运行面板是两个实例,
    // 连接/断连由 ControlWidget 统一通知, 避免出现"点了没反应且按钮永久置灰"
    void SetKeyParamsAvailable(const bool bAvailable);
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

    // 只读面板(模式数据面板)的"不能下发"状态由 m_bRunEnabled 表达, 这里不再留副本 ——
    // 两份状态必然有一天对不上(断连时忘改一份, 就会出现"能点但发不出去")
    bool m_bRunEnabled = true;

    // ---- 光源状态灯(2026-11-25 十二次) ----
    // 光源卡里 A/B/C/D 各配一盏灯: 灰(lamp_off)=灭, 绿(lamp_on)=亮。
    // 灯只在运行页(bFlatRun)创建, 配置窗口那个实例的列表是空的,
    // 所以下面两个接口内部必须判空 —— 配置窗口里切光源页不能因为没灯而崩。
    // 下标 0~3 对应 A~D, 与协议里的索引 1~4 差 1(转换只在调用处做一次)。
    QList<QLabel *> m_lstLightLamps;
    QPixmap m_pmLampOff;
    QPixmap m_pmLampOn;
    void SetLightLamp(const int iIndex, const bool bOn);
    void SetAllLightLampsOff(void);

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
    // ---- 运动序列状态(定义在类首 public 区, 这里只放成员与内部接口) ----
    MotionSequenceState m_ownSequence;                  // 独立使用时的那一份
    MotionSequenceState *m_pSequence = &m_ownSequence;
    ControlPanel *m_pPeerPanel = nullptr;               // 对面的面板(ControlWidget 设置, 可空)
    QTimer *m_pMotionSeqWatchdog = nullptr;             // 每个面板一个: 只有 owner 启停
    void StartMotionSeqWatchdog(const int iWallMs, const quint32 u32Cmds);
    // 序列收尾: 两个面板一起回空闲(非 owner 面板自己不会恢复, 见 terminate 处理)
    void RestoreIdleButtonsBoth(void);

    // ---- 聚拢序列的"保持"(2026-11-25 二十五次) ----
    // 需求: 勾了「保持」的聚拢应该是"一直 H/V 交替", 直到用户点"结束运动",
    // 而不是按"运动时间"(Gather 按钮自己的 timeuse)到点就停。
    // 实现: 段本身仍按时间下发(每段靠下位机完成帧推进, 不能发无限动作, 见
    // SendButtonMotion 的注释), 由上位机在队列见底时把同一个 H+V 周期再排一遍 ——
    // 计数因此永远不归零, 界面全程停在"运动中"(停止键可用), 直到:
    //   点了"结束运动" / 断连 / 看门狗超时 / 参数丢失。
    // 一轮的长度与命令数在启动时快照, 续排沿用同一份(中途改界面不影响正在跑的节奏)。
    // 按快照再排一轮 H [+间隔] V [+间隔], 返回实际入队的命令数(0 = 参数丢失, 排不进去)
    quint32 QueueGatherRound(void);

    // ---- 光源"时间模式"到点自动恢复(2026-11-24 二十四次) ----
    // 需求: 时间模式(没勾「保持」)下点开光源, 到设定秒数就该把"打开"键放开,
    // 不能逼用户去点一次"关闭"才知道可以再开。原来只靠下位机的 0x0E 完成帧恢复,
    // 而现场固件到点灭灯后不一定回帧(或回得晚), 按钮就长期卡在"只能点关闭"。
    // 做法: A~D 各一个单次定时器, 下发时按界面上的秒数起表, 到点无条件恢复该路。
    // 两个例外不启动(它们下位机根本不会自动灭灯):
    //   · 勾了「保持」           —— 常亮, 只能手动关;
    //   · 没勾但是**时间 = 0**   —— 协议里"保持时间 0 = 常亮"(见固件 LightOnHandler),
    //                              这条最容易被忽略: 0 秒不是"立刻灭", 是"永远亮"。
    // 0x0E 完成帧仍然保留, 谁先到谁恢复(两个入口都幂等)。
    QTimer *m_apLightRecoverTimers[4] = {nullptr, nullptr, nullptr, nullptr};
    void StartLightRecover(const int iIndex, const int iSeconds, const bool bKeep);
    void StopLightRecover(const int iIndex);
    void StopAllLightRecover(void);
    void RecoverLightChannel(const int iIndex);
    // 两个面板之间交接"还剩多久到点": 配置窗口里点开灯、到点前就关窗时,
    // 运行页要把剩余时间接过去, 否则那边会卡在"只能点关闭"。
    // 返回 0 = 这一路没有本地倒计时(保持 / 时间 0 常亮 / 本来就没开)。
    int LightRecoverRemainMs(const int iIndex) const;
    void StartLightRecoverMs(const int iIndex, const int iMs);
    void SendButtonMotion(const ButtonParams &Button, const float fTimeUseOverride, const float fVoltageOverride = -1.0f);
    void StartGatherSequence(void);
    void ResetMotionSequence(void);

    // ---- 运动进行中的界面锁(2026-11-23) ----
    // 需求: 点了"发送运动"/G1/G2 之后, 全部运动类型按钮(上/下/左/右/聚拢/水平居中/
    // 垂直居中/分散/翻面)一起禁用, 直到"完成帧 / 运动反馈超时 / 点了停止运动"才恢复。
    // 原因不只是防止连点重复下发: 参数控件(dspbVoltage/Frequency/Time/cbMotionKeep)只有一套,
    // 跟着"当前选中按钮"走, 运动进行中允许改选会让界面显示与正在跑的那条指令不一致。
    // 锁只锁按钮的 enabled, 不动 QButtonGroup 的选中项(checkable 按钮禁用后仍保持 checked),
    // 所以解锁后"当前选中按钮"与界面选中态天然一致。
    bool m_bMotionUiLocked = false;
    void SetMotionTypeButtonsEnabled(const bool bEnabled);
    void LockMotionUi(void);                 // 下发后: 发送/停止互换 + G1/G2 与全部运动类型按钮置灰
    void RestoreIdleButtons(void);           // 唯一的统一恢复点(完成/超时/停止/兜底/断连 全走这里)
    // 把"运动/料仓是否正在执行"同步给右下角状态灯(见 sigMotionRunningChanged)。
    // 在锁定、恢复、料仓开/停这几处调用, 保证灯和按钮永远是同一个状态。
    void NotifyRunState(void);

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
    // 写"当前选中按钮"的 angle_offset 并同步隐藏圆盘(输入框 / 圆盘两条路最后都走这里)
    void ApplyAngleOffset(const int16_t s16Angle);

    // ---- 固件参数页(0x15/0x16 获取, 0x17/0x18 保存) ----
    // 12 个 QDoubleSpinBox 与协议 0x16 数据域一一对应(6 组动作 × 起始相位超前/相位偏移),
    // 全部 float, 走 48 字节大端裸数据。界面只负责取值/显示,
    // 组帧与解码在 GeneralProtocol::SaveKeyParamsProtocol / KeyParamsFromBytes。
    bool m_bKeyParamsPending = false;                       // 获取/保存进行中(按钮已禁用)
    bool m_bKeyParamsAvailable = false;                     // 已连接(ControlWidget 通知)
    QList<QDoubleSpinBox *> KeyParamsEditors(void) const;    // 12 个编辑器, 顺序同协议
    GeneralKeyParams::KeyParams CollectKeyParams(void) const;
    void ApplyKeyParams(const GeneralKeyParams::KeyParams &sParams);
    void SetKeyParamsStatus(const QString &qstrText, const bool bError);
    void SetKeyParamsButtonsEnabled(const bool bEnabled);
    void UpdateKeyParamsButtons(void);                      // 可用性 + 等待态 合成后的最终使能

public slots:
    void OnMotionGroupButtonClicked(QAbstractButton *pButton);
    void OnMotionFinished(void);

    // 固件参数: 获取结果(状态字节 + 48 字节裸数据) / 保存结果(1成功 0失败 -1超时),
    // 由 ControlWidget 从通信线程转发过来
    void OnGetKeyParamsResult(const quint8 &u8Status, const QByteArray &qbtData);
    void OnSaveKeyParamsResult(const int &iResult);

    // 光源保持时间到: 下位机自发 0x0E 完成帧, 恢复对应路(0=全关)的按钮状态
    void OnLightOffFinish(const uint8_t &u8Index);

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

    // ---- 运行状态灯(右下角"状态"卡片) ----
    // 运动(振动盘单条 / G1 / G2 / 交替序列)与料仓各自是否正在执行。
    // 值由 NotifyRunState() 从按钮的实际使能态算出(与界面显示严格一致),
    // 不另存一份标志位 —— 两份状态迟早会不一致。
    // 只有运行页(bFlatRun)实例会被 ControlWidget 接走; 只读实例没人接也安全。
    void sigMotionRunningChanged(const bool bRunning);
    void sigBinRunningChanged(const bool bRunning);

    // 固件参数: 获取(0x15)无参数, 由引擎组帧; 保存(0x17)帧在这里组好再发
    // (49 字节数据域, 由 GeneralProtocol::SaveKeyParamsProtocol 生成)
    void sigGetKeyParams();
    void sigSaveKeyParams(const QByteArray &qbtData);

private slots:
    void on_dspbVoltage_valueChanged(double arg1);
    void on_dspbFrequency_valueChanged(double arg1);
    void on_dspbTime_valueChanged(double arg1);
    void on_cbMotionKeep_toggled(bool bChecked);
    void on_pbSendMotion_clicked();
    void on_pbSendMotionStop_clicked();
    void on_pbG1_clicked();
    void on_pbG2_clicked();
    void on_dialAngleRegulation_valueChanged(int value);
    // 角度输入框(二十六次: 转盘隐藏, 改成手输整数, 可带负号)
    void on_leAngle_textEdited(const QString &qstrText);
    void on_leAngle_editingFinished();
    void on_hsldSpeed_valueChanged(int value);          // 已废弃: speed 参数取消, 控件也隐藏, 保留只为不破坏 .ui 连接
    void on_hsVModify_valueChanged(int value);          // 垂直居中位置调节(水平滑块)
    void on_vsHModify_valueChanged(int value);          // 水平居中位置调节(竖直滑块)
    void on_pbBinStartMotion_clicked();
    void on_pbBinStopMotion_clicked();
    void on_pbOpenLightA_clicked();
    void on_pbCloseLightA_clicked();
    void on_pbOpenLightB_clicked();
    void on_pbCloseLihgtB_clicked();
    void on_pbOpenLightC_clicked();
    void on_pbCloseLightC_clicked();
    void on_pbOpenLightD_clicked();
    void on_pbCloseLightD_clicked();
    void on_pbGetKeyParams_clicked();
    void on_pbSaveKeyParams_clicked();
};

#endif // CONTROLPANEL_H
