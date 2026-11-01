#include "mainwindow.h"
#include "windowssymlink.h"
#include <QApplication>
#include <QIcon>
#include <cstdio>
#include <cstring>

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    const int helperResult = WindowsSymlink::runHelperFromCommandLine();
    if (helperResult != -1)
        return helperResult;
#endif
    // QApplication needs a display; version queries must also work headlessly.
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-v") == 0) {
            std::printf("mvlink %s\n", MVLINK_VERSION);
            return 0;
        }
    }
    QApplication mainApp(argc, argv);
    mainApp.setApplicationVersion(QStringLiteral(MVLINK_VERSION));
    mainApp.setDesktopFileName("mvlink");
    QIcon appIcon;
    for (int size : {16, 24, 32, 48, 64, 128, 256, 512}) {
        appIcon.addFile(QString(":/icons/mvlink-%1.png").arg(size), QSize(size, size));
    }
    mainApp.setWindowIcon(appIcon);
  
    MainWindow mainWindow;
    mainWindow.show();
    return mainApp.exec();
}
