#include "controlwidget.h"

#include <QApplication>
#include <QScreen>

int main(int argc, char *argv[])
{
    // 高分屏 DPI 缩放感知: 2K/4K + 系统缩放(150%/200%)下窗口按逻辑像素渲染,
    // 不会因物理像素铺满屏幕导致底部按钮溢出; 必须在 QApplication 构造前设置
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);
    ControlWidget w;

    w.show();

    // 小屏防御: 屏幕可视区域小于窗口需求时最大化, 避免底部按钮溢出屏幕点不到
    // (窗口 min 尺寸由布局决定, frameGeometry().height() 在 show 后才有真实值)
    const QRect sAvailable = QGuiApplication::primaryScreen()->availableGeometry();
    if (w.frameGeometry().height() > sAvailable.height() ||
        w.frameGeometry().width() > sAvailable.width())
    {
        w.showMaximized();
    }

    return a.exec();
}
