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
    QTemporaryDir m_settingsDir;
    QString m_testFilePath;

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void testCreateSymbolicLink();
    void testHandleMove();
    void testDestinationHistory();
    void testUiStates();
    void testViewportDrop();
    void testHardLinkPreference();
    void testLegacyHistoryMigration();
    void testActions_data();
    void testActions();
    void testSameDestination();
    void testHardLinkFailure();
    void testLongFilenameMove();

};

#endif // TEST_MAINWINDOW_H
