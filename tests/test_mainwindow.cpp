#include "test_mainwindow.h"
#include <QTest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QListWidgetItem>
#include <QTextDocument>
#include <QSettings>
#include <QFileDevice>


void TestMainWindow::initTestCase()
{
    
    QVERIFY(m_sourceDir.isValid());
    QVERIFY(m_destDir.isValid());
    QVERIFY(m_settingsDir.isValid());
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, m_settingsDir.path());
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, m_settingsDir.filePath("legacy"));
}

void TestMainWindow::cleanupTestCase()
{
    
}

void TestMainWindow::init()
{
    QSettings(QSettings::IniFormat, QSettings::UserScope, "mvlink", "mvlink").clear();
#ifndef Q_OS_WIN
    QSettings(QSettings::NativeFormat, QSettings::UserScope, "mvlink", "mvlink").clear();
#endif
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
    QStringList entries = sourceDir.entryList(QDir::Files | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        QString filePath = sourceDir.filePath(entry);
        if (QFileInfo(filePath).isSymLink() || QFile::exists(filePath)) {
            QFile::remove(filePath);
        }
    }
    
    QDir destDir(m_destDir.path());
    entries = destDir.entryList(QDir::Files | QDir::System | QDir::Hidden | QDir::NoDotAndDotDot);
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
    QCOMPARE(QSettings(QSettings::IniFormat, QSettings::UserScope, "mvlink", "mvlink").value("destinationHistory").toStringList(), QStringList{first});
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


void TestMainWindow::testActions_data()
{
    QTest::addColumn<bool>("linkOnly");
    QTest::addColumn<bool>("hardLink");
    QTest::newRow("move-symbolic") << false << false;
    QTest::newRow("move-hard") << false << true;
    QTest::newRow("link-symbolic") << true << false;
    QTest::newRow("link-hard") << true << true;
}

void TestMainWindow::testActions()
{
    QFETCH(bool, linkOnly);
    QFETCH(bool, hardLink);
    MainWindow window;
    QVERIFY(!window.hardLinkCheckBox->isChecked());
    QCOMPARE(window.linkButton->text(), QString("Link"));
    window.destinationPathEdit->setEditText(m_destDir.path());
    window.hardLinkCheckBox->setChecked(hardLink);
    auto *item = new QListWidgetItem("testfile.txt");
    item->setData(Qt::UserRole, m_testFilePath);
    window.fileListWidget->addItem(item);

    if (linkOnly)
        window.handleLink();
    else
        window.handleMove();

    const QString destination = m_destDir.filePath("testfile.txt");
    QVERIFY(QFileInfo(destination).exists());
    QVERIFY(QFileInfo(m_testFilePath).exists());
    QCOMPARE(window.fileListWidget->count(), 0);
    QCOMPARE(window.failCount, 0);
    QCOMPARE(QFileInfo(destination).isSymLink(), linkOnly && !hardLink);
    QCOMPARE(QFileInfo(m_testFilePath).isSymLink(), !linkOnly && !hardLink);
    if (linkOnly && !hardLink)
        QCOMPARE(QFileInfo(destination).symLinkTarget(), m_testFilePath);
    if (!linkOnly && !hardLink)
        QCOMPARE(QFileInfo(m_testFilePath).symLinkTarget(), destination);
    if (hardLink) {
        QFile destFile(destination);
        QVERIFY(destFile.open(QIODevice::Append));
        QCOMPARE(destFile.write(" changed"), qint64(8));
        destFile.close();
        QFile sourceFile(m_testFilePath);
        QVERIFY(sourceFile.open(QIODevice::ReadOnly));
        QCOMPARE(sourceFile.readAll(), QByteArray("Test content changed"));
    }
}

void TestMainWindow::testSameDestination()
{
    MainWindow window;
    window.destinationPathEdit->setEditText(m_sourceDir.path());
    auto *item = new QListWidgetItem("testfile.txt");
    item->setData(Qt::UserRole, m_testFilePath);
    window.fileListWidget->addItem(item);
    window.handleLink();
    QVERIFY(QFileInfo(m_testFilePath).isFile());
    QVERIFY(!QFileInfo(m_testFilePath).isSymLink());
    QCOMPARE(window.fileListWidget->count(), 1);
    QCOMPARE(window.failCount, 1);
}

void TestMainWindow::testHardLinkFailure()
{
    // A hard link cannot be created inside a missing parent directory.
    MainWindow window;
    QString error;
    QVERIFY(!window.createHardLink(m_testFilePath,
                                   m_destDir.filePath("missing/testfile.txt"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo(m_testFilePath).isFile());
}

void TestMainWindow::testLongFilenameMove()
{
    const QString name(250, QLatin1Char('x'));
    const QString sourcePath = m_sourceDir.filePath(name);
    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("preserved"), qint64(9));
    source.close();
    MainWindow window;
    window.destinationPathEdit->setEditText(m_destDir.path());
    auto *item = new QListWidgetItem(name);
    item->setData(Qt::UserRole, sourcePath);
    window.fileListWidget->addItem(item);
    window.handleMove();
    QCOMPARE(window.fileListWidget->count(), 0);
    QCOMPARE(window.failCount, 0);
    QVERIFY(QFileInfo(sourcePath).isSymLink());
    QVERIFY(QFileInfo(m_destDir.filePath(name)).isFile());
    QVERIFY(QFile::remove(sourcePath));
    QVERIFY(QFile::remove(m_destDir.filePath(name)));
}

void TestMainWindow::testHardLinkPreference()
{
    {
        MainWindow window;
        QVERIFY(!window.hardLinkCheckBox->isChecked());
        window.hardLinkCheckBox->setChecked(true);
        QSettings saved(QSettings::IniFormat, QSettings::UserScope, "mvlink", "mvlink");
        QVERIFY(saved.value("hardLink").toBool());
        QCOMPARE(saved.status(), QSettings::NoError);
        QVERIFY(QFileInfo(saved.fileName()).isFile());
        QVERIFY(saved.fileName().startsWith(m_settingsDir.path() + "/"));
    }
    {
        MainWindow reopened;
        QVERIFY(reopened.hardLinkCheckBox->isChecked());
        reopened.hardLinkCheckBox->setChecked(false);
    }
    MainWindow reopened;
    QVERIFY(!reopened.hardLinkCheckBox->isChecked());
}

void TestMainWindow::testLegacyHistoryMigration()
{
#ifdef Q_OS_WIN
    QSKIP("Native Windows settings use the registry and cannot be redirected with setPath");
#else
    QSettings legacy(QSettings::NativeFormat, QSettings::UserScope, "mvlink", "mvlink");
    const QStringList history{m_destDir.path()};
    legacy.setValue("destinationHistory", history);
    legacy.sync();
    {
        MainWindow window;
        QCOMPARE(window.destinationPathEdit->itemText(0), history.first());
    }
    QSettings saved(QSettings::IniFormat, QSettings::UserScope, "mvlink", "mvlink");
    QCOMPARE(saved.value("destinationHistory").toStringList(), history);
    QCOMPARE(legacy.value("destinationHistory").toStringList(), history);

    const QStringList replacement{m_sourceDir.path()};
    saved.setValue("destinationHistory", replacement);
    saved.sync();
    MainWindow reopened;
    QCOMPARE(reopened.destinationPathEdit->itemText(0), replacement.first());
#endif
}
