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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAcceptDrops(true);
    setupUi();

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseDirectory);
    connect(moveButton, &QPushButton::clicked, this, &MainWindow::handleMove);
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
    destinationPathEdit = new QLineEdit();
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
    mainLayout->addLayout(buttonLayout);
    
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
        destinationPathEdit->setText(dir);
        logMessage("Destination folder set to: " + dir);
    }
}

void MainWindow::handleMove()
{
    QString destDir = destinationPathEdit->text();
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
    
    int successCount = 0;
    failCount = 0;
    
    logMessage("Starting move operation to: " + destDir);
        
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        QString sourcePath = item->data(Qt::UserRole).toString();
        QFileInfo fileInfo(sourcePath);
        
        if (fileInfo.isSymLink()) {
            markFileFailed(item, "Skipped: " + fileInfo.fileName() + " (already a symbolic link)"); 
            continue;
        }

        QString destPath = dir.absoluteFilePath(fileInfo.fileName());

        if (QFile::exists(destPath)) {
            if (QMessageBox::question(this, "File exists", 
                QString("File %1 already exists in destination. Overwrite?").arg(fileInfo.fileName())) 
                != QMessageBox::Yes) {
                    markFileFailed(item, "file exists");
                    continue;
            }
            QFile::remove(destPath);
            logMessage("Removed existing file: " + fileInfo.fileName());
        }

        if (QFile::copy(sourcePath, destPath)) {
            if (QFile::remove(sourcePath)) {
                createSymbolicLink(destPath, sourcePath);
                successCount++;
                logMessage("Moved: " + fileInfo.fileName());
                delete fileListWidget->takeItem(i);
                i--;
            } else {
                QFile::remove(destPath);
                QString errorMsg = "Failed to remove source file: " + sourcePath;
                QMessageBox::warning(this, "Warning", errorMsg);
                markFileFailed(item, "Error:" + errorMsg);
            }
        } else {
            QString errorMsg = "Failed to copy file: " + sourcePath;
            QMessageBox::warning(this, "Warning", errorMsg);
            markFileFailed(item, "Error:" + errorMsg);        }
    }
    QString resultMessage = QString("Operation completed: %1 succeeded, %2 failed").arg(successCount).arg(failCount);
    statusLabel->setText(resultMessage);
    logMessage(resultMessage);
}

void MainWindow::clearList()
{
    fileListWidget->clear();
    statusLabel->setText("Drag and drop files here");
    logMessage("File list cleared");
}

void MainWindow::createSymbolicLink(const QString &sourcePath, const QString &destPath)
{
    #ifdef Q_OS_WIN
        // TODO: On Windows?
        QProcess process;
        process.start("cmd.exe", QStringList() << "/c" << "mklink" << QDir::toNativeSeparators(destPath) << QDir::toNativeSeparators(sourcePath));
        process.waitForFinished();
    #else
        // On Unix/Linux
        QFile::link(sourcePath, destPath);
    #endif
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