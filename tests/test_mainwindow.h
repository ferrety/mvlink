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
#ifdef Q_OS_WIN
    void testWindowsElevationPolicy_data();
    void testWindowsElevationPolicy();
    void testWindowsHelperValidation();
    void testWindowsHelperProcess();
    void testWindowsSessionCancellation();
    void testWindowsSessionReuse();
    void testWindowsSessionHelperExit();
    void testWindowsNativePaths_data();
    void testWindowsNativePaths();
    void testWindowsNativePathFallback();
    void testWindowsLongPaths_data();
    void testWindowsLongPaths();
#endif

};

#endif // TEST_MAINWINDOW_H
