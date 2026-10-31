#include "test_mainwindow.h"
#include <QTest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QListWidgetItem>
#include <QTextDocument>
#include <QSettings>

void TestMainWindow::initTestCase()
{
    
    QVERIFY(m_sourceDir.isValid());
    QVERIFY(m_destDir.isValid());
    QVERIFY(m_settingsDir.isValid());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDir.path());
}

void TestMainWindow::cleanupTestCase()
{
    
}

void TestMainWindow::init()
{
    QSettings("mvlink", "mvlink").clear();
    m_testFilePath = m_sourceDir.filePath("testfile.txt");
    QFile testFile(m_testFilePath);
    QVERIFY(testFile.open(QIODevice::WriteOnly | QIODevice::Text));
    testFile.write("Test content");
    testFile.close();
    QVERIFY(QFile::exists(m_testFilePath));
}

void TestMainWindow::cleanup()
{
    if (QFile::exists(m_testFilePath)) {
        QFile::remove(m_testFilePath);
    }
    

    QDir sourceDir(m_sourceDir.path());
    QStringList entries = sourceDir.entryList(QDir::Files);
    for (const QString &entry : entries) {
        QString filePath = sourceDir.filePath(entry);
        if (QFileInfo(filePath).isSymLink() || QFile::exists(filePath)) {
            QFile::remove(filePath);
        }
    }
    
    QDir destDir(m_destDir.path());
    entries = destDir.entryList(QDir::Files);
    for (const QString &entry : entries) {
        QFile::remove(destDir.filePath(entry));
    }
}

void TestMainWindow::testCreateSymbolicLink()
{
    // Initialize test data
    QString sourcePath = m_destDir.filePath("source.txt");
    QString destPath = m_sourceDir.filePath("link.txt");
    
    // Create source file
    QFile sourceFile(sourcePath);
    QVERIFY(sourceFile.open(QIODevice::WriteOnly | QIODevice::Text));
    sourceFile.write("Source file content");
    sourceFile.close();
    QVERIFY(QFile::exists(sourcePath));
    
    // Create MainWindow instance
    MainWindow mainWindow;
    
    mainWindow.createSymbolicLink(sourcePath, destPath);
    
    // Verify symlink was created
    QVERIFY(QFile::exists(destPath));
    QFileInfo linkInfo(destPath);
    QVERIFY(linkInfo.isSymLink());
    QCOMPARE(linkInfo.symLinkTarget(), sourcePath);
}

void TestMainWindow::testHandleMove()
{
    // Create MainWindow instance
    MainWindow mainWindow;
    
    // Add test file to the file list
    QListWidgetItem *item = new QListWidgetItem("testfile.txt");
    item->setData(Qt::UserRole, m_testFilePath);
    mainWindow.fileListWidget->addItem(item);
    
    // Set destination path
    mainWindow.destinationPathEdit->setCurrentText(m_destDir.path());
    
    // Call handleMove
    mainWindow.handleMove();
    
    // Verify file was moved and symlink created
    QString destFilePath = m_destDir.filePath("testfile.txt");
    QVERIFY(QFile::exists(destFilePath));

    QFileInfo linkInfo(m_testFilePath);
    QVERIFY(linkInfo.exists());
    QVERIFY(linkInfo.isSymLink());
    QCOMPARE(linkInfo.symLinkTarget(), destFilePath);
    
    // Verify that the file list is empty 
    QCOMPARE(mainWindow.fileListWidget->count(), 0);

    MainWindow reopenedWindow;
    QCOMPARE(reopenedWindow.destinationPathEdit->itemText(0), QDir(m_destDir.path()).absolutePath());
    QCOMPARE(reopenedWindow.destinationPathEdit->currentText(), QString());

    // TODO: Test for failure cases
}

void TestMainWindow::testDestinationHistory()
{
    MainWindow mainWindow;
    mainWindow.destinationPathEdit->setCurrentText(m_destDir.path());
    mainWindow.handleMove();
    QCOMPARE(mainWindow.destinationPathEdit->count(), 0);

    const QString first = QDir(m_destDir.path()).absolutePath();
    const QString second = QDir(m_sourceDir.path()).absolutePath();
    mainWindow.rememberDestination(first);
    QCOMPARE(QSettings("mvlink", "mvlink").value("destinationHistory").toStringList(), QStringList{first});
    mainWindow.rememberDestination(second);
    mainWindow.rememberDestination(first);

    MainWindow reopenedWindow;
    QCOMPARE(reopenedWindow.destinationPathEdit->count(), 2);
    QCOMPARE(reopenedWindow.destinationPathEdit->itemText(0), first);
    QCOMPARE(reopenedWindow.destinationPathEdit->itemText(1), second);
    reopenedWindow.destinationPathEdit->setCurrentIndex(1);
    QCOMPARE(reopenedWindow.destinationPathEdit->currentText(), second);

    for (int i = 0; i < 11; ++i) {
        mainWindow.rememberDestination(m_destDir.filePath(QString::number(i)));
    }
    MainWindow limitedWindow;
    QCOMPARE(limitedWindow.destinationPathEdit->count(), 10);
    QCOMPARE(limitedWindow.destinationPathEdit->itemText(0), m_destDir.filePath("10"));
    QCOMPARE(limitedWindow.destinationPathEdit->itemText(9), m_destDir.filePath("1"));
}

