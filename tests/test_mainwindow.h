#ifndef TEST_MAINWINDOW_H
#define TEST_MAINWINDOW_H

#include <QObject>
#include <QTemporaryDir>
#include "../src/mainwindow.h"

class TestMainWindow : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_sourceDir;
    QTemporaryDir m_destDir;
    QString m_testFilePath;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testCreateSymbolicLink();
    void testHandleMove();

};

#endif // TEST_MAINWINDOW_H