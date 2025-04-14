QT += core gui widgets testlib
CONFIG += qt warn_on depend_includepath testcase

TARGET = mvlink-test

SOURCES += \
    tests/test_mainwindow.cpp \
    tests/test_main.cpp \
    src/mainwindow.cpp

HEADERS += \
    tests/test_mainwindow.h \
    src/mainwindow.h
