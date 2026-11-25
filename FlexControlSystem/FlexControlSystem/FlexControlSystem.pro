QT       += core gui
QT += serialport
QT += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ConnectiveHandle.cpp \
    controlpanel.cpp \
    controlsettings.cpp \
    generalprotocol.cpp \
    groupmotion.cpp \
    main.cpp \
    controlwidget.cpp \
    rs485handle.cpp \
    tcphandle.cpp \
    titlebar.cpp

HEADERS += \
    ConnectiveHandle.h \
    ProtocolHandle.h \
    controlpanel.h \
    controlsettings.h \
    controlwidget.h \
    generalprotocol.h \
    groupmotion.h \
    rs485handle.h \
    tcphandle.h \
    titlebar.h

FORMS += \
    controlpanel.ui \
    controlwidget.ui \
    groupmotion.ui

# 全局视觉样式(主题「B 浅色卡片」), 唯一视觉出口, 见 style/app.qss 文件头说明
RESOURCES += \
    style/app.qrc

# exe 图标(资源管理器/任务栏/Alt+Tab 看到的那张脸)。app.ico 由
# build-check/make_logo.bat 从 image/logo.png 生成, 不要手改。
RC_FILE = app.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
