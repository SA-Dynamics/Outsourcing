#include "controlwidget.h"

#include <QApplication>
#include <QScreen>
#include <QFile>
#include <QFont>
#include <QFontInfo>
#include <QIcon>
#include <QPixmap>

namespace
{
// 加载全局样式表(主题「B 浅色卡片」)。样式表编译进 qrc, 不依赖外部文件,
// 拷 exe 到现场时不会漏带 qss 而退回 Windows 原生老界面。
void ApplyTheme(QApplication &app)
{
    QFile qss(QStringLiteral(":/style/app.qss"));
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
        qss.close();
    }
}

// 中文字体: Win8+ 优先 "Microsoft YaHei UI"(字重层次更清楚), Win7 只有 "Microsoft YaHei"。
// 用 QFontInfo 探测实际解析到的族名, 避免在 Win7 上静默回落到宋体(界面会立刻显旧)。
void ApplyFont(QApplication &app)
{
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSize(9);
    if (!QFontInfo(font).family().contains(QStringLiteral("YaHei")))
    {
        font.setFamily(QStringLiteral("Microsoft YaHei"));
    }
    app.setFont(font);
}

// 窗口尺寸(逻辑像素, 高 DPI 由 Qt 自动放大)。默认值是 100% 缩放下的理想尺寸;
// 现场机可能是 200% 缩放(逻辑屏只有 1280x800), 所以这两个数只是"上限",
// 真正用哪个尺寸由 main() 里按屏幕可用区裁剪。
const int kWinDefaultWidth  = 1400;
const int kWinDefaultHeight = 860;
const int kWinMinWidth      = 1200;
const int kWinMinHeight     = 700;
}

int main(int argc, char *argv[])
{
    // 高分屏 DPI 缩放感知: 2K/4K + 系统缩放(150%/200%)下窗口按逻辑像素渲染,
    // 不会因物理像素铺满屏幕导致底部按钮溢出; 必须在 QApplication 构造前设置
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);

    // 顺序不能换: 先定字体, 再由样式表覆盖颜色/字号。
    // 反过来时样式表里的 font-size 会被 setFont 冲掉一部分, 出现字号不统一。
    ApplyFont(a);
    ApplyTheme(a);

    // 窗口/任务栏图标。exe 里已经编了 app.ico(见 app.rc), 但 Qt 程序若不显式
    // setWindowIcon, Windows 会用 Qt 自带的默认图标画窗口和任务栏 —— 现场的
    // 第一眼就是任务栏上那个陌生的图标, 所以这里必须给。96 那档缩放最清晰。
    a.setWindowIcon(QIcon(QStringLiteral(":/brand/logo_96.png")));

    ControlWidget w;

    // ---- 按屏幕可用区定尺寸(2026-11-25 二十三次) --------------------------
    // .ui 里的 1400x860 / 最小 1200x700 是"开发机 100% 缩放"下的值。现场机常开
    // 200% 缩放, 逻辑可用区只有 1280x764: 写死 1400 会让右半边(含标题栏三个
    // 窗口按钮)整体推到屏幕外, 最小 1300 又把它锁死缩不回来。
    // 这里一律以 availableGeometry(不含任务栏)为上限裁剪, 屏小就最大化。
    const QRect rcAvail = QGuiApplication::primaryScreen()->availableGeometry();
    w.setMinimumSize(qMin(kWinMinWidth, rcAvail.width()),
                     qMin(kWinMinHeight, rcAvail.height()));
    w.resize(qMin(kWinDefaultWidth, rcAvail.width()),
             qMin(kWinDefaultHeight, rcAvail.height()));

    w.show();

    if (w.width() >= kWinDefaultWidth && w.height() >= kWinDefaultHeight)
    {
        // 屏装得下: 主动居中。Windows 给高 DPI 窗口的默认位置可能偏出右边界。
        w.move(rcAvail.x() + (rcAvail.width() - w.width()) / 2,
               rcAvail.y() + (rcAvail.height() - w.height()) / 2);
    }
    else
    {
        // 屏不够大: 铺满可用区。必须走自绘标题栏里的 availableGeometry 逻辑,
        // 不能 showMaximized —— 无边框窗口那样会连任务栏一起盖住。
        w.MaximizeToScreen();
    }

    return a.exec();
}
