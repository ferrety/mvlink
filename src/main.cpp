#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication mainApp(argc, argv);
    MainWindow mainWindow;
    mainWindow.show();
    return mainApp.exec();
}