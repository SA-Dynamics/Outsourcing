#ifndef GROUPMOTION_H
#define GROUPMOTION_H

#include <QWidget>
#include <QVector>

#include "ProtocolHandle.h"     // GeneralMotion::MotionParams::MotionIndex

namespace Ui {
class GroupMotion;
}

class QComboBox;

// 组合运动表单: S1~S8 共 8 步, 每步"动作类型 + 运行时间"
// ControlPanel 把它嵌进 G1Widget / G2Widget, 点 G1/G2 时按 S1..S8 顺序下发
class GroupMotion : public QWidget
{
    Q_OBJECT

public:
    struct Step
    {
        GeneralMotion::MotionParams::MotionIndex eMotion;   // 动作类型
        float fTimeUse;                                     // 该步运行时间(秒)
    };

    explicit GroupMotion(QWidget *parent = nullptr);
    ~GroupMotion();

    // 按 S1..S8 顺序取出序列; 运行时间 <= 0 的步视为"不使用", 直接跳过
    QVector<Step> GetSteps(void) const;

    // 取出全部 8 步(S1..S8), 包含时间为 0 的"不使用"步 —— 用于保存到 ini
    QVector<Step> GetAllSteps(void) const;

    // 设置第 iIndex 步(S1..S8, 0 基)。iMotion 在列表里找不到时保持原选项不变,
    // 时间照常写入(ini 里被删掉的旧动作类型不会把界面搞乱)
    void SetStep(const int iIndex, const int iMotion, const float fTimeUse);

    // 从另一个表单拷贝 8 步的动作类型与时间(用于模式数据面板互拷)
    void CopyFrom(const GroupMotion *pOther);

private:
    void InitComboBoxes(void);
    void AddMotionItem(QComboBox *pCombo, const QString &qstrText,
                       const GeneralMotion::MotionParams::MotionIndex eMotion);

    Ui::GroupMotion *ui;
};

#endif // GROUPMOTION_H
