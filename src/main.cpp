#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication mainApp(argc, argv);
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
