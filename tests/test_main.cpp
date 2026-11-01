#include <QTest>
#include "test_mainwindow.h"
#include "../src/windowssymlink.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    const int helperResult = WindowsSymlink::runHelperFromCommandLine();
    if (helperResult != -1)
        return helperResult;
#endif
    QApplication testApp(argc, argv);
    TestMainWindow testMainWindow;
    return QTest::qExec(&testMainWindow, argc, argv);   
}
