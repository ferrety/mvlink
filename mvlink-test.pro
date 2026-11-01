QT += core gui widgets testlib
CONFIG += qt warn_on depend_includepath testcase

TARGET = mvlink-test

SOURCES += \
    tests/test_mainwindow.cpp \
    tests/test_main.cpp \
    src/mainwindow.cpp \
    src/windowssymlink.cpp

HEADERS += \
    tests/test_mainwindow.h \
    src/mainwindow.h \
    src/windowssymlink.h

win32:LIBS += -lshell32 -ladvapi32

RESOURCES += assets/icons.qrc
