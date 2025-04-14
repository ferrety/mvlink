#include <QTest>
#include "test_mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication testApp(argc, argv);
    TestMainWindow testMainWindow;
    return QTest::qExec(&testMainWindow, argc, argv);   
}