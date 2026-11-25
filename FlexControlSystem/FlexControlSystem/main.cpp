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

    w.show();

    // 小屏防御: 屏幕可视区域小于窗口需求时最大化, 避免底部按钮溢出屏幕点不到
    // (窗口 min 尺寸由布局决定, frameGeometry().height() 在 show 后才有真实值)
    // 注意不能再用 showMaximized(): 窗口已去掉系统边框(见 ControlWidget 构造器),
    // 无边框窗口按 screenGeometry 铺满会盖掉任务栏, 交由标题栏按可用区域最大化
    const QRect sAvailable = QGuiApplication::primaryScreen()->availableGeometry();
    if (w.frameGeometry().height() > sAvailable.height() ||
        w.frameGeometry().width() > sAvailable.width())
    {
        w.MaximizeToScreen();
    }

    return a.exec();
}
