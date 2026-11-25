#include "titlebar.h"

#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

namespace
{
// 三个窗口按钮的尺寸: 高 = 标题栏 46 - 上下各 7 的留白。
// 宽 48 是 Windows 11 的量级(46), 再宽会让右侧三条占掉太多金属面。
// 2026-11-25 二十一次: 标题栏 62→46 后按钮跟着压到 32, 否则 44 高的按钮
// 在 46 的条里只剩 1px 留白, 三个键会顶到上下边缘, 看着像"贴死的色块"。
const int kBtnWidth  = 48;
const int kBtnHeight = 32;
}

// ============================= TitleBarButton =============================

TitleBarButton::TitleBarButton(Role eRole, QWidget *parent)
    : QAbstractButton(parent)
    , m_eRole(eRole)
{
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(kBtnWidth, kBtnHeight);
    setToolTip(eRole == RoleMinimize ? QStringLiteral("最小化")
               : eRole == RoleClose  ? QStringLiteral("关闭")
                                     : QStringLiteral("最大化/还原"));
}

void TitleBarButton::SetRole(const Role eRole)
{
    if (m_eRole == eRole)
    {
        return;
    }
    m_eRole = eRole;
    update();
}

void TitleBarButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const bool bClose = (m_eRole == RoleClose);
    const bool bHover = underMouse();
    const bool bDown  = isDown();
    const QRect rcBody = rect().adjusted(0, 0, -1, -1);

    // 悬停底: 关闭键沿用 Windows 的"红底"肌肉记忆, 其余键用极淡墨色。
    // 一律半透明叠色 —— 不透明色块压在渐变金属上会一眼看出是贴上去的。
    if (bClose && (bHover || bDown))
    {
        p.setPen(Qt::NoPen);
        p.setBrush(bDown ? QColor(0xC5, 0x0F, 0x1F) : QColor(0xE8, 0x11, 0x23));
        p.drawRoundedRect(rcBody, 5, 5);
    }
    else if (bHover)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(16, 24, 40, bDown ? 38 : 20));
        p.drawRoundedRect(rcBody, 5, 5);
    }

    QColor clrIcon(0x3A, 0x42, 0x4C);
    if (bClose && (bHover || bDown))
    {
        clrIcon = QColor(0xFF, 0xFF, 0xFF);
    }
    else if (bHover)
    {
        clrIcon = QColor(0x14, 0x1A, 0x22);
    }

    QPen pen(clrIcon, 1.5);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::MiterJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const qreal dx = width() / 2.0;
    const qreal dy = height() / 2.0;

        switch (m_eRole)
        {
        case RoleMinimize:
        {
            // 横线只有长度没有面积, 按 1.5 画出来会比方框/叉视觉上轻一档,
            // 单独加粗到 1.8 才和另外两个图标等重
            QPen penMin = p.pen();
            penMin.setWidthF(1.8);
            p.setPen(penMin);
            p.drawLine(QPointF(dx - 5.5, dy + 0.5), QPointF(dx + 5.5, dy + 0.5));
            break;
        }

    case RoleMaximize:
        p.drawRect(QRectF(dx - 5.5, dy - 5.5, 11.0, 11.0));
        break;

    case RoleRestore:
        // "还原"图标: 后框只画上边 + 右边, 前框画完整矩形。
        // 两个框都画全, 交叠处会压出一个十字, 看着像加号而不是两个窗口。
        p.drawLine(QPointF(dx - 3.0, dy - 6.0), QPointF(dx + 6.0, dy - 6.0));
        p.drawLine(QPointF(dx + 6.0, dy - 6.0), QPointF(dx + 6.0, dy + 3.0));
        p.drawRect(QRectF(dx - 6.0, dy - 3.0, 9.0, 9.0));
        break;

    case RoleClose:
        p.drawLine(QPointF(dx - 5.0, dy - 5.0), QPointF(dx + 5.0, dy + 5.0));
        p.drawLine(QPointF(dx + 5.0, dy - 5.0), QPointF(dx - 5.0, dy + 5.0));
        break;
    }
}

// ================================ TitleBar ================================

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
    , m_pBtnMaximize(nullptr)
    , m_bMaximized(false)
    , m_bDragging(false)
{
    setObjectName(QStringLiteral("titleBar"));
    setFixedHeight(kBarHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // 品牌 logo: 96 高那档(高 DPI 下缩到 30 逻辑像素仍有余量)。
    // 图已由 build-check/make_logo.cpp 去过白底, 直接贴渐变底不会出现白方块。
    m_pmLogo = QPixmap(QStringLiteral(":/brand/logo_96.png"));

    TitleBarButton *pBtnMin   = new TitleBarButton(TitleBarButton::RoleMinimize, this);
    TitleBarButton *pBtnMax   = new TitleBarButton(TitleBarButton::RoleMaximize, this);
    TitleBarButton *pBtnClose = new TitleBarButton(TitleBarButton::RoleClose, this);
    m_pBtnMaximize = pBtnMax;

    QHBoxLayout *pLayout = new QHBoxLayout(this);
    pLayout->setContentsMargins(0, (kBarHeight - kBtnHeight) / 2, 6,
                                (kBarHeight - kBtnHeight) / 2);
    pLayout->setSpacing(0);
    pLayout->addStretch(1);
    pLayout->addWidget(pBtnMin);
    pLayout->addWidget(pBtnMax);
    pLayout->addWidget(pBtnClose);

    connect(pBtnMin, &QAbstractButton::clicked, this, [this]() {
        if (QWidget *pWin = window())
        {
            pWin->showMinimized();
        }
    });
    connect(pBtnMax, &QAbstractButton::clicked, this, [this]() {
        ToggleMaximized();
    });
    connect(pBtnClose, &QAbstractButton::clicked, this, [this]() {
        // 走 close() 而不是 qApp->quit(): 主窗口 closeEvent 里有退出确认
        // (连接中/动作执行中退出必须让用户知情), 直接退出会绕过它
        if (QWidget *pWin = window())
        {
            pWin->close();
        }
    });
}

TitleBar::~TitleBar()
{
}

void TitleBar::SetTitle(const QString &qstrTitle)
{
    if (m_qstrTitle == qstrTitle)
    {
        return;
    }
    m_qstrTitle = qstrTitle;
    update();
}

void TitleBar::SetMaximized(const bool bMaximized)
{
    QWidget *pWin = window();
    if (pWin == nullptr || pWin == this || m_bMaximized == bMaximized)
    {
        return;
    }

    if (bMaximized)
    {
        m_rectRestore = pWin->geometry();

        // 必须用 availableGeometry(): screenGeometry() 含任务栏, 无边框窗口
        // 按它铺满会把任务栏整个盖掉, 现场就点不到开始菜单/任务栏图标
        QScreen *pScreen = QGuiApplication::screenAt(pWin->frameGeometry().center());
        if (pScreen == nullptr)
        {
            pScreen = QGuiApplication::primaryScreen();
        }
        if (pScreen != nullptr)
        {
            pWin->setGeometry(pScreen->availableGeometry());
        }
        m_bMaximized = true;
    }
    else
    {
        if (m_rectRestore.isValid())
        {
            pWin->setGeometry(m_rectRestore);
        }
        m_bMaximized = false;
    }

    if (m_pBtnMaximize != nullptr)
    {
        m_pBtnMaximize->SetRole(m_bMaximized ? TitleBarButton::RoleRestore
                                             : TitleBarButton::RoleMaximize);
    }

    emit MaximizedChanged(m_bMaximized);
}

void TitleBar::ToggleMaximized(void)
{
    SetMaximized(!m_bMaximized);
}

QSize TitleBar::sizeHint(void) const
{
    return QSize(600, kBarHeight);
}

QSize TitleBar::minimumSizeHint(void) const
{
    return QSize(320, kBarHeight);
}

void TitleBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect rcAll = rect();

    // ---- 底色: 垂直渐变(顶部亮、底部略暗) ----
    // 档位按等间距铺开: 间距一密一疏, 视线会在疏密交界处看到一道横带,
    // 像两块板拼起来。(2026-11-25: 拉丝纹理已按用户要求去掉, 只留这层渐变。)
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0.00, QColor(0xFD, 0xFE, 0xFF));
    grad.setColorAt(0.18, QColor(0xEF, 0xF3, 0xF7));
    grad.setColorAt(0.36, QColor(0xE2, 0xE7, 0xEE));
    grad.setColorAt(0.54, QColor(0xD6, 0xDD, 0xE5));
    grad.setColorAt(0.72, QColor(0xC8, 0xD0, 0xDA));
    grad.setColorAt(0.88, QColor(0xBB, 0xC4, 0xCF));
    grad.setColorAt(1.00, QColor(0xAF, 0xB9, 0xC6));
    p.fillRect(rcAll, grad);

    // ---- 上沿迎光倒角 / 下沿收边 ----
    p.setPen(QColor(255, 255, 255, 255));
    p.drawLine(0, 0, width() - 1, 0);
    p.setPen(QColor(255, 255, 255, 120));
    p.drawLine(0, 1, width() - 1, 1);
    p.setPen(QColor(255, 255, 255, 105));
    p.drawLine(0, height() - 2, width() - 1, height() - 2);
    p.setPen(QColor(0x9E, 0xA8, 0xB5));
    p.drawLine(0, height() - 1, width() - 1, height() - 1);

    // 左右各压 1px 暗边, 与主窗口外框接得上, 不出现"标题栏比窗口宽"的错位感
    p.setPen(QColor(0xB4, 0xBC, 0xC7, 140));
    p.drawLine(0, 0, 0, height() - 1);
    p.drawLine(width() - 1, 0, width() - 1, height() - 1);

    // ---- 品牌 logo + 标题 ----
    // logo 高 26(文字字高约 15, 1.7 倍看着最平衡; 30 会显得压过标题),
    // 垂直居中, 文字接在 logo 右侧。96 高的源图缩到 26, 各种缩放下都够清晰。
    int iTextX = 18;
    if (!m_pmLogo.isNull())
    {
        const int iLogoH = 26;
        const int iLogoW = qMax(1, qRound(qreal(m_pmLogo.width()) * iLogoH
                                          / qreal(m_pmLogo.height())));
        const QRect rcLogo(18, (height() - iLogoH) / 2, iLogoW, iLogoH);

        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.drawPixmap(rcLogo, m_pmLogo);
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);

        iTextX = rcLogo.right() + 1 + 12;
    }

    QFont fTitle = font();
    fTitle.setPointSizeF(10.5);
    fTitle.setBold(true);
    fTitle.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
    p.setFont(fTitle);

    // 右侧留给三个窗口按钮(3×48 + 边距), 文字过长时省略而不是压到按钮底下
    const QRect rcText(iTextX, 0, qMax(0, width() - iTextX - 190), height());
    const QString qstrText = p.fontMetrics().elidedText(m_qstrTitle, Qt::ElideRight,
                                                        rcText.width());

    p.setPen(QColor(0x2B, 0x32, 0x3B));
    p.drawText(rcText, Qt::AlignLeft | Qt::AlignVCenter, qstrText);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_bDragging = true;
        m_ptDragOffset = event->globalPos() - window()->frameGeometry().topLeft();
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_bDragging || !(event->buttons() & Qt::LeftButton))
    {
        QWidget::mouseMoveEvent(event);
        return;
    }

    QWidget *pWin = window();
    if (pWin == nullptr || pWin == this)
    {
        return;
    }

    if (m_bMaximized)
    {
        // 最大化状态下拖动 = Windows 的"从最大化拖出来还原"。还原后必须重算
        // 抓握点, 否则鼠标和窗口脱节(表现为窗口突然跳到鼠标左边)。
        const QRect rcRestore = m_rectRestore.isValid() ? m_rectRestore : pWin->geometry();
        const qreal dRatio = (width() > 0) ? (qreal(event->pos().x()) / qreal(width())) : 0.5;
        SetMaximized(false);
        m_ptDragOffset = QPoint(int(rcRestore.width() * dRatio),
                                qBound(1, event->pos().y(), qMax(1, kBarHeight - 1)));
    }

    pWin->move(event->globalPos() - m_ptDragOffset);
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_bDragging = false;
    QWidget::mouseReleaseEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        ToggleMaximized();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}
