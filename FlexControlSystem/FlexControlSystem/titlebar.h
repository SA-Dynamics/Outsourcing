#ifndef TITLEBAR_H
#define TITLEBAR_H

#include <QAbstractButton>
#include <QPixmap>
#include <QRect>
#include <QWidget>

// 标题栏右上角的窗口按钮(自绘: 最小化 / 最大化-还原 / 关闭)。
// 不用 QPushButton, 因为 app.qss 给 QPushButton 写了全局规则(白底/描边/圆角),
// 那几个规则放到金属条上会立刻毁掉质感; QAbstractButton 只提供点击与按下状态,
// 外观完全由 paintEvent 决定, 不受样式表干扰。
class TitleBarButton : public QAbstractButton
{
    Q_OBJECT

public:
    enum Role
    {
        RoleMinimize,
        RoleMaximize,
        RoleRestore,
        RoleClose
    };
    Q_ENUM(Role)

    explicit TitleBarButton(Role eRole, QWidget *parent = nullptr);
    void SetRole(Role eRole);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    Role m_eRole;
};

// 自绘标题栏(2026-11-24 六次引入, 七次去掉拉丝、加入品牌 logo)
// ----------------------------------------------------------------------------
// 为什么必须自绘:
//   Windows 原生标题栏的高度(NONCLIENTMETRICS 的 iCaptionHeight)、配色、按钮
//   尺寸都不允许进程内改。要"高度翻倍 + 定制外观"只有一条路 —— 主窗口去掉
//   系统边框(Qt::FramelessWindowHint), 由本控件把标题栏整条画出来。
//
// 自绘后必须自己补齐的系统行为(缺一个用户立刻能感觉到"不像正经窗口"):
//   ① 拖标题栏移动窗口;  ② 双击最大化/还原;  ③ 三个按钮的功能与悬停反馈;
//   ④ 最大化后不能盖住任务栏(用 availableGeometry 而不是 screenGeometry)。
//
// 外观(2026-11-25 用户确认): 纯渐变底 + 顶部高光/底部收边 + 左侧 logo + 文字。
// **不要加回拉丝纹理** —— 那版被判定"金属味太重", 已按用户要求去掉。
class TitleBar : public QWidget
{
    Q_OBJECT

public:
    // 标题栏高度。系统标题栏 @100% 缩放约 31px, 乘 2 = 62px; 2026-11-25 二十一次
    // 按用户要求收到原来的四分之三 → 62 * 0.75 ≈ 46px。**只压这条**, logo(26px)
    // 与标题字号(10.5pt)一律不动, 所以它们在新高度里的占比会变大, 这是预期效果。
    // 用逻辑像素, 高分屏由 Qt 按 devicePixelRatio 自动放大。
    static const int kBarHeight = 46;

    explicit TitleBar(QWidget *parent = nullptr);
    ~TitleBar();

    QString Title(void) const { return m_qstrTitle; }
    void SetTitle(const QString &qstrTitle);

    bool IsMaximized(void) const { return m_bMaximized; }
    // 最大化到当前屏幕可用区域(不含任务栏); 传 false 则还原到最大化前的尺寸
    void SetMaximized(bool bMaximized);

    QSize sizeHint(void) const override;
    QSize minimumSizeHint(void) const override;

signals:
    // 最大化状态变化(点击按钮、双击标题栏、从最大化拖出都会触发)
    void MaximizedChanged(bool bMaximized);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void ToggleMaximized(void);

    QString m_qstrTitle;
    QPixmap m_pmLogo;        // 品牌 logo(已去白底, 见 build-check/make_logo.cpp)

    TitleBarButton *m_pBtnMaximize;
    bool m_bMaximized;
    bool m_bDragging;
    QPoint m_ptDragOffset;   // 按下点相对窗口左上角的偏移
    QRect m_rectRestore;     // 最大化前的窗口几何, 用于还原
};

#endif // TITLEBAR_H
