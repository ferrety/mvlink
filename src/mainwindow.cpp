#include "mainwindow.h"
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMimeData>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

namespace {
constexpr int destinationHistoryLimit = 10;
const char destinationHistoryKey[] = "destinationHistory";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAcceptDrops(true);
    setupUi();

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseDirectory);
    connect(moveButton, &QPushButton::clicked, this, &MainWindow::handleMove);
    connect(linkButton, &QPushButton::clicked, this, &MainWindow::handleLink);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearList);
    
    setWindowTitle("mvlink - Drag & Drop Files");
    statusLabel->setText("Drag and drop files here");
    }

MainWindow::~MainWindow()
{
}

void MainWindow::setupUi()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Destination path selection
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel("Destination Folder:"));
    destinationPathEdit = new QComboBox();
    destinationPathEdit->setEditable(true);
    destinationPathEdit->setInsertPolicy(QComboBox::NoInsert);
    QSettings settings("mvlink", "mvlink");
    destinationPathEdit->addItems(settings.value(destinationHistoryKey).toStringList());
    destinationPathEdit->setCurrentText(QString());
    pathLayout->addWidget(destinationPathEdit);
    browseButton = new QPushButton("Browse...");
    pathLayout->addWidget(browseButton);
    mainLayout->addLayout(pathLayout);
    
    // File list
    fileListWidget = new QListWidget();
    fileListWidget->setDragEnabled(false);
    fileListWidget->setAcceptDrops(true);
    fileListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainLayout->addWidget(fileListWidget);
    
    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    moveButton = new QPushButton("Move Files");
    clearButton = new QPushButton("Clear List");
    buttonLayout->addWidget(moveButton);
    buttonLayout->addWidget(clearButton);
    linkButton = new QPushButton("Link");
    buttonLayout->addWidget(linkButton);
    mainLayout->addLayout(buttonLayout);
    
    hardLinkCheckBox = new QCheckBox("Create hard links instead of symbolic links");
    hardLinkCheckBox->setToolTip("Hard links require both folders to be on the same filesystem. "
                                "Edits through either path affect the same file.");
    mainLayout->addWidget(hardLinkCheckBox);

    // Status label
    statusLabel = new QLabel();
    mainLayout->addWidget(statusLabel);
    
    // Log view
    QLabel *logLabel = new QLabel("Log:");
    mainLayout->addWidget(logLabel);

    logView = new QTextEdit();
    logView->setReadOnly(true);
    logView->setLineWrapMode(QTextEdit::WidgetWidth);
    logView->setMaximumHeight(80); // Show approximately 2-3 lines by default
    mainLayout->addWidget(logView);

    resize(600, 450);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        
        for (const QUrl &url : urlList) {
            QString filePath = url.toLocalFile();
            QFileInfo fileInfo(filePath);
            
            if (fileInfo.isFile()) {
                QListWidgetItem *item = new QListWidgetItem(fileInfo.fileName());
                item->setData(Qt::UserRole, filePath);
                fileListWidget->addItem(item);
            }
        }
        
        QString message = QString("%1 files ready to be moved").arg(fileListWidget->count());
        statusLabel->setText(message);
        logMessage(message);
    }
}

void MainWindow::browseDirectory()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Directory",
                                                   QDir::homePath(),
                                                   QFileDialog::ShowDirsOnly);
                                                   
    if (!dir.isEmpty()) {
        destinationPathEdit->setCurrentText(dir);
        logMessage("Destination folder set to: " + dir);
    }
}

void MainWindow::handleMove()
{
    QString destDir = destinationPathEdit->currentText();
    processFiles(false);
}

void MainWindow::handleLink()
{
    processFiles(true);
}

void MainWindow::processFiles(bool linkOnly)
{
    const QString destDir = destinationPathEdit->text();
    if (destDir.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a destination folder first.");
        logMessage("Error: No destination folder selected");
        return;
    }
    QDir dir(destDir);
    if (!dir.exists()) {
        QMessageBox::critical(this, "Error", "Destination folder does not exist.");
        logMessage("Error: Destination folder does not exist");
        return;
    }

    const bool hardLink = hardLinkCheckBox->isChecked();
    const QString action = linkOnly ? "Link" : "Move";
    const QString linkType = hardLink ? "hard link" : "symbolic link";
    int successCount = 0;
    failCount = 0;
    logMessage(QString("Starting %1 operation (%2) to: %3").arg(action, linkType, destDir));

    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        const QString sourcePath = QFileInfo(item->data(Qt::UserRole).toString()).absoluteFilePath();
        const QFileInfo fileInfo(sourcePath);
        if (fileInfo.isSymLink()) {
            markFileFailed(item, "Skipped: " + fileInfo.fileName() + " (already a symbolic link)");
            continue;
        }
        if (!fileInfo.isFile()) {
            markFileFailed(item, "Source is not a regular file: " + sourcePath);
            continue;
        }
        const QString destPath = dir.absoluteFilePath(fileInfo.fileName());
        const QFileInfo destInfo(destPath);
        if (QDir::cleanPath(sourcePath) == QDir::cleanPath(destPath) ||
            (destInfo.exists() && destInfo.canonicalFilePath() == fileInfo.canonicalFilePath())) {
            markFileFailed(item, "Destination resolves to source: " + sourcePath);
            continue;
        }
        if (destInfo.exists() || destInfo.isSymLink()) {
            if (QMessageBox::question(this, "File exists",
                QString("File %1 already exists in destination. Overwrite?").arg(fileInfo.fileName()))
                != QMessageBox::Yes) {
                markFileFailed(item, "Skipped: destination file exists");
                continue;
            }
            QFile destination(destPath);
            if (!destination.remove()) {
                markFileFailed(item, "Failed to remove destination: " + destination.errorString());
                continue;
            }
        }

        QString error;
        bool succeeded = false;
        if (hardLink) {
            succeeded = createHardLink(sourcePath, destPath, &error);
        } else if (linkOnly) {
            succeeded = createSymbolicLink(sourcePath, destPath, &error);
        } else {
            QFile source(sourcePath);
            if (!source.copy(destPath)) {
                error = "Failed to copy file: " + source.errorString();
            } else {
                // Keep the original recoverable until its replacement link exists.
                const QString backupPath = fileInfo.dir().absoluteFilePath(
                    ".mvlink-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
                if (!source.rename(backupPath)) {
                    error = "Failed to back up source: " + source.errorString();
                    QFile::remove(destPath);
                } else {
                    succeeded = createSymbolicLink(destPath, sourcePath, &error);
                    if (succeeded) {
                        if (!QFile::remove(backupPath))
                            logMessage("Could not remove source backup: " + backupPath);
                    } else if (QFile::rename(backupPath, sourcePath)) {
                        QFile::remove(destPath);
                    } else {
                        error += "; original preserved at " + backupPath;
                    }
                }
            }
        }
        if (!succeeded) {
            markFileFailed(item, QString("%1 failed (%2) for %3: %4")
                           .arg(action, linkType, sourcePath, error));
            continue;
        }
        ++successCount;
        logMessage(QString("%1 succeeded (%2): %3").arg(action, linkType, fileInfo.fileName()));
        delete fileListWidget->takeItem(i--);
    }
    const QString result = QString("%1 completed: %2 succeeded, %3 failed")
                           .arg(action).arg(successCount).arg(failCount);
    if (successCount > 0) {
        rememberDestination(dir.absolutePath());
    }
    statusLabel->setText(result);
    logMessage(result);
}

void MainWindow::clearList()
{
    fileListWidget->clear();
    statusLabel->setText("Drag and drop files here");
    logMessage("File list cleared");
}

bool MainWindow::createSymbolicLink(const QString &sourcePath, const QString &destPath, QString *error)
{
#ifdef Q_OS_WIN
    QProcess process;
    process.start("cmd.exe", QStringList() << "/c" << "mklink"
                  << QDir::toNativeSeparators(destPath) << QDir::toNativeSeparators(sourcePath));
    const bool finished = process.waitForFinished();
    const bool ok = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    if (!ok && error)
        *error = finished ? QString::fromLocal8Bit(process.readAllStandardError()) : process.errorString();
#else
    QFile source(sourcePath);
    const bool ok = source.link(destPath);
    if (!ok && error)
        *error = source.errorString();
#endif
    return ok;
}

bool MainWindow::createHardLink(const QString &sourcePath, const QString &destPath, QString *error)
{
#ifdef Q_OS_WIN
    const bool ok = CreateHardLinkW(reinterpret_cast<LPCWSTR>(destPath.utf16()),
                                    reinterpret_cast<LPCWSTR>(sourcePath.utf16()), nullptr);
    if (!ok && error)
        *error = QString("Windows error %1 (hard links require the same filesystem)").arg(GetLastError());
#else
    const bool ok = ::link(QFile::encodeName(sourcePath).constData(),
                           QFile::encodeName(destPath).constData()) == 0;
    if (!ok && error)
        *error = QString::fromLocal8Bit(std::strerror(errno));
#endif
    return ok;
}

void MainWindow::logMessage(const QString &message)
{
    logView->append(message);
    
    // scroll to latest message
    QScrollBar *scrollBar = logView->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void MainWindow::markFileFailed(QListWidgetItem *item, const QString &reason)
{
    failCount++;
    logMessage(reason);
    item->setForeground(Qt::red);
}

void MainWindow::rememberDestination(const QString &path)
{
    const QString normalizedPath = QDir::cleanPath(path);
    QSettings settings("mvlink", "mvlink");
    QStringList history = settings.value(destinationHistoryKey).toStringList();
    history.removeAll(normalizedPath);
    history.prepend(normalizedPath);
    while (history.size() > destinationHistoryLimit) {
        history.removeLast();
    }
    settings.setValue(destinationHistoryKey, history);

    destinationPathEdit->clear();
    destinationPathEdit->addItems(history);
    destinationPathEdit->setCurrentText(normalizedPath);
}
