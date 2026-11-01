QT       += core gui widgets

TARGET = mvlink
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/mainwindow.h

RESOURCES += assets/icons.qrc
win32:RC_ICONS = assets/mvlink.ico

unix:!macx {
    isEmpty(PREFIX): PREFIX = /usr/local
    target.path = $$PREFIX/bin
    desktop.files = assets/mvlink.desktop
    desktop.path = $$PREFIX/share/applications
    appicon.files = assets/mvlink.png
    appicon.path = $$PREFIX/share/icons/hicolor/256x256/apps
    INSTALLS += target desktop appicon
}
