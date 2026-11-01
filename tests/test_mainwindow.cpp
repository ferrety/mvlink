#include "test_mainwindow.h"
#include <QTest>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QListWidgetItem>
#include <QTextDocument>
#include <QSettings>
#include <QFileDevice>
#include <QToolButton>
#include <QFrame>
#include <QMimeData>
#include <QUrl>
#include <QDragLeaveEvent>
#include <QProcess>
#include <QCoreApplication>
#include "../src/windowssymlink.h"

#ifdef Q_OS_WIN
namespace {
DWORD directResult;
DWORD elevatedResult;
int elevationCalls;
QString receivedTarget;
QString receivedLink;
int sessionLaunches;
HANDLE observedHelper = nullptr;
QStringList nativeTargets;
QStringList nativeLinks;
QList<DWORD> nativeFlags;
bool rejectUnprivilegedFlag = false;
BOOLEAN WINAPI captureNativeCreate(LPCWSTR link, LPCWSTR target, DWORD flags)
{
    nativeLinks.append(QString::fromWCharArray(link));
    nativeTargets.append(QString::fromWCharArray(target));
    nativeFlags.append(flags);
    if (rejectUnprivilegedFlag && (flags & 0x2)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return TRUE;
}
DWORD cancelSession(const QString &, HANDLE *)
{
    ++sessionLaunches;
    return ERROR_CANCELLED;
}
DWORD launchTestSession(const QString &parameters, HANDLE *process)
{
    ++sessionLaunches;
    QString command = "\"" + QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
        + "\" " + parameters;
    std::wstring native = command.toStdWString();
    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child = {};
    if (!CreateProcessW(nullptr, &native[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                         nullptr, nullptr, &startup, &child))
        return GetLastError();
    CloseHandle(child.hThread);
    *process = child.hProcess;
    DuplicateHandle(GetCurrentProcess(), child.hProcess, GetCurrentProcess(), &observedHelper,
                     SYNCHRONIZE, FALSE, 0);
    return ERROR_SUCCESS;
}
DWORD launchExitedSession(const QString &, HANDLE *process)
{
    return launchTestSession("--internal-create-symbolic-link", process);
}
DWORD fakeDirect(const QString &target, const QString &link)
{
    receivedTarget = target;
    receivedLink = link;
    return directResult;
}
DWORD fakeElevated(const QString &target, const QString &link)
{
    if (target != receivedTarget || link != receivedLink)
        return ERROR_INVALID_PARAMETER;
    ++elevationCalls;
    return elevatedResult;
}
}

void TestMainWindow::testWindowsNativePaths_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QString>("expected");
    QTest::newRow("dos") << QString("C:/folder/file.txt") << QStringLiteral(R"(\\?\C:\folder\file.txt)");
    QTest::newRow("unc") << QString("//server/share/folder/file.txt") << QStringLiteral(R"(\\?\UNC\server\share\folder\file.txt)");
    const QString name(250, QLatin1Char('x'));
    QTest::newRow("long-dos") << "C:/folder/another/" + name << QStringLiteral(R"(\\?\C:\folder\another\)") + name;
    QTest::newRow("long-unc") << "//server/share/folder/" + name << QStringLiteral(R"(\\?\UNC\server\share\folder\)") + name;
    QTest::newRow("native-unc") << QStringLiteral(R"(\\server\share\file.txt)") << QStringLiteral(R"(\\?\UNC\server\share\file.txt)");
    QTest::newRow("existing-dos-prefix") << QStringLiteral(R"(\\?\C:\folder\file.txt)") << QStringLiteral(R"(\\?\C:\folder\file.txt)");
    QTest::newRow("existing-unc-prefix") << QStringLiteral(R"(\\?\UNC\server\share\file.txt)") << QStringLiteral(R"(\\?\UNC\server\share\file.txt)");
    QTest::newRow("device-namespace") << QStringLiteral(R"(\\.\C:\folder\file.txt)") << QStringLiteral(R"(\\.\C:\folder\file.txt)");
    QTest::newRow("normalize-dos") << QString("C:/folder/./child/../file.txt") << QStringLiteral(R"(\\?\C:\folder\file.txt)");
    QTest::newRow("normalize-unc") << QString("//server/share/./child/../file.txt") << QStringLiteral(R"(\\?\UNC\server\share\file.txt)");
    QTest::newRow("unicode-and-spaces") << QString::fromUtf8("C:/folder with spaces/ä中.txt")
        << QString::fromUtf8("\\\\?\\C:\\folder with spaces\\ä中.txt");
    QTest::newRow("relative") << QString("folder/file.txt") << QStringLiteral(R"(folder\file.txt)");
    QTest::newRow("drive-relative") << QString("C:file.txt") << QString("C:file.txt");
}

void TestMainWindow::testWindowsNativePaths()
{
    QFETCH(QString, path);
    QFETCH(QString, expected);
    nativeTargets.clear();
    nativeLinks.clear();
    nativeFlags.clear();
    rejectUnprivilegedFlag = false;
    QCOMPARE(WindowsSymlink::createNative(path, path, 0, captureNativeCreate), DWORD(ERROR_SUCCESS));
    // Inspect the actual arguments at the Win32 API boundary, for both paths.
    QCOMPARE(nativeTargets, QStringList{expected});
    QCOMPARE(nativeLinks, QStringList{expected});
    QCOMPARE(nativeFlags, QList<DWORD>{0x2});
}

void TestMainWindow::testWindowsNativePathFallback()
{
    nativeTargets.clear();
    nativeLinks.clear();
    nativeFlags.clear();
    rejectUnprivilegedFlag = true;
    const DWORD code = WindowsSymlink::createNative("//server/share/target.txt", "C:/links/link.txt",
                                                   SYMBOLIC_LINK_FLAG_DIRECTORY, captureNativeCreate);
    rejectUnprivilegedFlag = false;
    QCOMPARE(code, DWORD(ERROR_SUCCESS));
    const QString target = QStringLiteral(R"(\\?\UNC\server\share\target.txt)");
    const QString link = QStringLiteral(R"(\\?\C:\links\link.txt)");
    QCOMPARE(nativeTargets, (QStringList{target, target}));
    QCOMPARE(nativeLinks, (QStringList{link, link}));
    QCOMPARE(nativeFlags, (QList<DWORD>{SYMBOLIC_LINK_FLAG_DIRECTORY | 0x2, SYMBOLIC_LINK_FLAG_DIRECTORY}));
}

void TestMainWindow::testWindowsLongPaths_data()
{
    QTest::addColumn<int>("route");
    QTest::addColumn<bool>("longTarget");
    QTest::addColumn<bool>("longLink");
    QTest::newRow("direct-long-target") << 0 << true << false;
    QTest::newRow("direct-long-link") << 0 << false << true;
    QTest::newRow("direct-both-long") << 0 << true << true;
    QTest::newRow("one-shot-helper") << 1 << true << true;
    QTest::newRow("session-helper") << 2 << true << true;
}

void TestMainWindow::testWindowsLongPaths()
{
    QFETCH(int, route);
    QFETCH(bool, longTarget);
    QFETCH(bool, longLink);
    // Probe without elevation so unattended test runs never open UAC prompts.
    const QString probe = m_destDir.filePath("privilege-probe.txt");
    const DWORD privilege = WindowsSymlink::createDirect(m_testFilePath, probe);
    if (privilege == ERROR_PRIVILEGE_NOT_HELD)
        QSKIP("Real symlink creation requires Developer Mode or symbolic-link privilege");
    QCOMPARE(privilege, DWORD(ERROR_SUCCESS));
    QVERIFY(QFile::remove(probe));
    const QString target = m_sourceDir.filePath(longTarget ? QString(250, 't') : "target.txt");
    const QString link = m_destDir.filePath(longLink ? QString(250, 'l') : "link.txt");
    if (longTarget)
        QVERIFY(target.size() >= MAX_PATH);
    if (longLink)
        QVERIFY(link.size() >= MAX_PATH);
    QVERIFY(QFile::copy(m_testFilePath, target));
    DWORD code = ERROR_GEN_FAILURE;
    if (route == 0) {
        code = WindowsSymlink::createDirect(target, link);
    } else if (route == 1) {
        QProcess helper;
        helper.start(QCoreApplication::applicationFilePath(), {"--internal-create-symbolic-link", target, link});
        QVERIFY(helper.waitForFinished());
        QCOMPARE(helper.exitStatus(), QProcess::NormalExit);
        code = DWORD(helper.exitCode());
    } else {
        observedHelper = nullptr;
        {
            WindowsSymlink::Session session(launchTestSession);
            code = session.create(target, link);
        }
        QVERIFY(observedHelper);
        QCOMPARE(WaitForSingleObject(observedHelper, 5000), DWORD(WAIT_OBJECT_0));
        CloseHandle(observedHelper);
    }
    QVERIFY2(code == ERROR_SUCCESS, qPrintable(WindowsSymlink::errorMessage(code)));
    QVERIFY(QFileInfo(link).isSymLink());
    QCOMPARE(QFileInfo(link).symLinkTarget(), target);
    QFile contents(link);
    QVERIFY(contents.open(QIODevice::ReadOnly));
    QCOMPARE(contents.readAll(), QByteArray("Test content"));
}

void TestMainWindow::testWindowsElevationPolicy_data()
{
    QTest::addColumn<quint32>("initialCode");
    QTest::addColumn<quint32>("helperCode");
    QTest::addColumn<bool>("shouldElevate");
    QTest::newRow("developer-mode") << quint32(ERROR_SUCCESS) << quint32(ERROR_GEN_FAILURE) << false;
    QTest::newRow("privilege-required") << quint32(ERROR_PRIVILEGE_NOT_HELD) << quint32(ERROR_SUCCESS) << true;
    QTest::newRow("uac-cancelled") << quint32(ERROR_PRIVILEGE_NOT_HELD) << quint32(ERROR_CANCELLED) << true;
    QTest::newRow("helper-failed") << quint32(ERROR_PRIVILEGE_NOT_HELD) << quint32(ERROR_PATH_NOT_FOUND) << true;
    QTest::newRow("access-denied") << quint32(ERROR_ACCESS_DENIED) << quint32(ERROR_SUCCESS) << false;
    QTest::newRow("already-exists") << quint32(ERROR_ALREADY_EXISTS) << quint32(ERROR_SUCCESS) << false;
    QTest::newRow("missing-parent") << quint32(ERROR_PATH_NOT_FOUND) << quint32(ERROR_SUCCESS) << false;
}

void TestMainWindow::testWindowsElevationPolicy()
{
    QFETCH(quint32, initialCode);
    QFETCH(quint32, helperCode);
    QFETCH(bool, shouldElevate);
    directResult = initialCode;
    elevatedResult = helperCode;
    elevationCalls = 0;
    const DWORD result = WindowsSymlink::create("relative target.txt", "relative link.txt", fakeDirect, fakeElevated);
    QCOMPARE(result, DWORD(shouldElevate ? helperCode : initialCode));
    QCOMPARE(elevationCalls, shouldElevate ? 1 : 0);
    QVERIFY(QDir::isAbsolutePath(receivedTarget));
    QVERIFY(QDir::isAbsolutePath(receivedLink));
    QVERIFY(WindowsSymlink::errorMessage(ERROR_CANCELLED).contains("cancelled"));
}

void TestMainWindow::testWindowsHelperValidation()
{
    const QString option = "--internal-create-symbolic-link";
    QCOMPARE(WindowsSymlink::runHelper({"mvlink"}), -1);
    QCOMPARE(WindowsSymlink::runHelper({"mvlink", option}), int(ERROR_INVALID_PARAMETER));
    QCOMPARE(WindowsSymlink::runHelper({"mvlink", option, "relative", "relative"}), int(ERROR_INVALID_PARAMETER));
    QCOMPARE(WindowsSymlink::runHelper({"mvlink", option, m_testFilePath, m_destDir.path(), "extra"}),
             int(ERROR_INVALID_PARAMETER));
    const QString session = "--internal-symbolic-link-session";
    QCOMPARE(WindowsSymlink::runHelper({"mvlink", session}), int(ERROR_INVALID_PARAMETER));
    QCOMPARE(WindowsSymlink::runHelper({"mvlink", session, "not-a-pipe", "123"}), int(ERROR_INVALID_PARAMETER));
}

void TestMainWindow::testWindowsSessionCancellation()
{
    sessionLaunches = 0;
    WindowsSymlink::Session session(cancelSession);
    QCOMPARE(session.create("relative", "relative"), DWORD(ERROR_INVALID_PARAMETER));
    QCOMPARE(sessionLaunches, 0);
    QCOMPARE(session.create(m_testFilePath, m_destDir.filePath("one.txt")), DWORD(ERROR_CANCELLED));
    QCOMPARE(session.create(m_testFilePath, m_destDir.filePath("two.txt")), DWORD(ERROR_CANCELLED));
    QCOMPARE(sessionLaunches, 1);
    QVERIFY(QFile::exists(m_testFilePath));
}

void TestMainWindow::testWindowsSessionReuse()
{
    sessionLaunches = 0;
    observedHelper = nullptr;
    {
        // Launch the real IPC helper without UAC so this test is unattended.
        WindowsSymlink::Session session(launchTestSession);
        for (const QString &name : {QString("first & %.txt"), QString::fromUtf8("second ä 中.txt")}) {
            const QString link = m_destDir.filePath(name);
            const DWORD code = session.create(m_testFilePath, link);
            QVERIFY2(code == ERROR_SUCCESS || code == ERROR_PRIVILEGE_NOT_HELD,
                     qPrintable(WindowsSymlink::errorMessage(code)));
            if (code == ERROR_SUCCESS) {
                QVERIFY(QFileInfo(link).isSymLink());
                QCOMPARE(QFileInfo(link).symLinkTarget(), m_testFilePath);
            }
        }
        QCOMPARE(sessionLaunches, 1);
        QVERIFY(observedHelper);
        QCOMPARE(WaitForSingleObject(observedHelper, 0), DWORD(WAIT_TIMEOUT));
    }
    // Closing the parent endpoint also cleans up the helper process.
    QCOMPARE(WaitForSingleObject(observedHelper, 5000), DWORD(WAIT_OBJECT_0));
    CloseHandle(observedHelper);
}

void TestMainWindow::testWindowsSessionHelperExit()
{
    sessionLaunches = 0;
    observedHelper = nullptr;
    WindowsSymlink::Session session(launchExitedSession);
    // A helper which exits before connecting must not hang the parent or
    // produce another elevation prompt for the next file.
    QCOMPARE(session.create(m_testFilePath, m_destDir.filePath("one.txt")), DWORD(ERROR_BROKEN_PIPE));
    QCOMPARE(session.create(m_testFilePath, m_destDir.filePath("two.txt")), DWORD(ERROR_BROKEN_PIPE));
    QCOMPARE(sessionLaunches, 1);
    CloseHandle(observedHelper);
}

void TestMainWindow::testWindowsHelperProcess()
{
    // Exercise the real Unicode command line parser and the headless helper.
    const QString target = m_sourceDir.filePath(QString::fromUtf8("target & %PATH% ^ ä 中.txt"));
    QVERIFY(QFile::copy(m_testFilePath, target));
    const QString link = m_destDir.filePath(QString::fromUtf8("link & %PATH% ^ ä 中.txt"));
    const QStringList arguments = {"--internal-create-symbolic-link", target, link};
    QProcess helper;
    helper.start(QCoreApplication::applicationFilePath(), arguments);
    QVERIFY(helper.waitForFinished());
    QCOMPARE(helper.exitStatus(), QProcess::NormalExit);
    if (helper.exitCode() == ERROR_PRIVILEGE_NOT_HELD)
        QSKIP("Helper process needs Developer Mode or symbolic-link privilege");
    QCOMPARE(helper.exitCode(), 0);
    QVERIFY(QFileInfo(link).isSymLink());
    QCOMPARE(QFileInfo(link).symLinkTarget(), target);
    helper.start(QCoreApplication::applicationFilePath(), arguments);
    QVERIFY(helper.waitForFinished());
    QVERIFY(helper.exitCode() != 0); // Never overwrite an existing link.
    QFile original(target);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), QByteArray("Test content"));
}
#endif


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

void TestMainWindow::testUiStates()
{
    MainWindow window;
    window.show();
    QCoreApplication::processEvents();
    QVERIFY(window.emptyState->isVisible());
    QVERIFY(!window.moveButton->isEnabled());
    QVERIFY(!window.linkButton->isEnabled());
    QVERIFY(!window.clearButton->isEnabled());
    QVERIFY(window.logView->isHidden());

    auto *item = new QListWidgetItem("testfile.txt");
    item->setData(Qt::UserRole, m_testFilePath);
    window.fileListWidget->addItem(item);
    QVERIFY(window.emptyState->isHidden());
    QCOMPARE(window.fileCountLabel->text(), QString("1"));
    QVERIFY(window.moveButton->isEnabled());
    QVERIFY(window.linkButton->isEnabled());
    QVERIFY(window.clearButton->isEnabled());
    window.markFileFailed(item, "Cannot link <testfile.txt>");
    QVERIFY(window.activityButton->isChecked());
    QVERIFY(window.logView->isVisible());
    QCoreApplication::processEvents();
    QVERIFY(window.fileListWidget->height() >= window.fileListWidget->minimumHeight());
    QVERIFY(item->toolTip().contains("Cannot link <testfile.txt>"));
    QVERIFY(window.logView->toPlainText().contains("Cannot link <testfile.txt>"));
    QCOMPARE(window.fileListWidget->count(), 1);
    window.activityButton->click();
    QVERIFY(window.logView->isHidden());
    window.hardLinkCheckBox->setChecked(true);
    QVERIFY(window.linkExplanation->text().contains("same filesystem"));
    window.hardLinkCheckBox->setChecked(false);
    QVERIFY(window.linkExplanation->text().contains("Symbolic"));
    window.clearButton->click();
    QVERIFY(window.emptyState->isVisible());
    QVERIFY(!window.moveButton->isEnabled());
    QVERIFY(!window.linkButton->isEnabled());
    QVERIFY(!window.clearButton->isEnabled());
    QVERIFY(QFile::exists(m_testFilePath));
}

void TestMainWindow::testViewportDrop()
{
    MainWindow window;
    window.show();
    QCoreApplication::processEvents();
    QMimeData mime;
    mime.setUrls({QUrl::fromLocalFile(m_testFilePath)});
    QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.fileListWidget->viewport(), &enter);
    QVERIFY(enter.isAccepted());
    QVERIFY(window.filesPanel->property("dragActive").toBool());
    QDragLeaveEvent leave;
    QApplication::sendEvent(window.fileListWidget->viewport(), &leave);
    QVERIFY(!window.filesPanel->property("dragActive").toBool());
    QDragEnterEvent reenter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.fileListWidget->viewport(), &reenter);
    QDropEvent drop(QPointF(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(window.fileListWidget->viewport(), &drop);
    QVERIFY(drop.isAccepted());
    QCOMPARE(window.fileListWidget->count(), 1);
    QCOMPARE(window.fileListWidget->item(0)->data(Qt::UserRole).toString(), m_testFilePath);
    QVERIFY(!window.filesPanel->property("dragActive").toBool());
    QVERIFY(window.emptyState->isHidden());
}
