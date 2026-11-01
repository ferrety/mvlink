QT       += core gui widgets

TARGET = mvlink
TEMPLATE = app
include(version.pri)

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/windowssymlink.cpp

HEADERS += \
    src/mainwindow.h \
    src/windowssymlink.h

RESOURCES += assets/icons.qrc
win32:RC_ICONS = assets/mvlink.ico
win32:LIBS += -lshell32 -ladvapi32

unix:!macx {
    isEmpty(PREFIX): PREFIX = /usr/local
    target.path = $$PREFIX/bin
    desktop.files = assets/mvlink.desktop
    desktop.path = $$PREFIX/share/applications
    appicon.files = assets/mvlink.png
    appicon.path = $$PREFIX/share/icons/hicolor/256x256/apps
    INSTALLS += target desktop appicon
}
