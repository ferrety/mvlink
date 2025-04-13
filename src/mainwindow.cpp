#include "mainwindow.h"
#include <QFileInfo>
#include <QDir>
#include <QProcess>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setAcceptDrops(true);
    setupUi();

    connect(browseButton, &QPushButton::clicked, this, &MainWindow::browseDestinationFolder);
    connect(moveButton, &QPushButton::clicked, this, &MainWindow::moveAndCreateSymlinks);
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
    
    // Action buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    moveButton = new QPushButton("Move Files");
    clearButton = new QPushButton("Clear List");
    buttonLayout->addWidget(moveButton);
    buttonLayout->addWidget(clearButton);
    mainLayout->addLayout(buttonLayout);
    
    // Status label
    statusLabel = new QLabel();
    mainLayout->addWidget(statusLabel);
    
    // Set minimum size
    resize(600, 400);
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
        
        statusLabel->setText(QString("%1 files ready to be moved").arg(fileListWidget->count()));
    }
}

void MainWindow::browseDestinationFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Destination Directory",
                                                   QDir::homePath(),
                                                   QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
                                                   
    if (!dir.isEmpty()) {
        destinationPathEdit->setText(dir);
    }
}

void MainWindow::moveAndCreateSymlinks()
{
    QString destDir = destinationPathEdit->text();
    if (destDir.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a destination folder first.");
        return;
    }
    
    QDir dir(destDir);
    if (!dir.exists()) {
        QMessageBox::critical(this, "Error", "Destination folder does not exist.");
        return;
    }
    
    int successCount = 0;
    int failCount = 0;
    
    for (int i = 0; i < fileListWidget->count(); ++i) {
        QListWidgetItem *item = fileListWidget->item(i);
        QString sourcePath = item->data(Qt::UserRole).toString();
        QFileInfo fileInfo(sourcePath);
        QString destPath = dir.absoluteFilePath(fileInfo.fileName());
        
        // Check if destination file already exists
        if (QFile::exists(destPath)) {
            if (QMessageBox::question(this, "File exists", 
                QString("File %1 already exists in destination. Overwrite?").arg(fileInfo.fileName())) 
                != QMessageBox::Yes) {
                failCount++;
                continue;
            }
            QFile::remove(destPath);
        }
        
        // Move the file to destination
        if (QFile::copy(sourcePath, destPath)) {
            // If move successful, create symbolic link
            if (QFile::remove(sourcePath)) {
                createSymbolicLink(destPath, sourcePath);
                successCount++;
            } else {
                // If unable to remove source file, remove the copied file
                QFile::remove(destPath);
                failCount++;
                QMessageBox::warning(this, "Warning", 
                    QString("Failed to remove source file: %1").arg(sourcePath));
            }
        } else {
            failCount++;
            QMessageBox::warning(this, "Warning", 
                QString("Failed to copy file: %1").arg(sourcePath));
        }
    }
    
    // Clear the list if successful
    if (successCount > 0 && failCount == 0) {
        fileListWidget->clear();
    }
    
    statusLabel->setText(QString("Operation completed: %1 succeeded, %2 failed").arg(successCount).arg(failCount));
}

void MainWindow::clearList()
{
    fileListWidget->clear();
    statusLabel->setText("Drag and drop files here");
}

void MainWindow::createSymbolicLink(const QString &sourcePath, const QString &destPath)
{
    #ifdef Q_OS_WIN
        // On Windows
        QProcess process;
        process.start("cmd.exe", QStringList() << "/c" << "mklink" << QDir::toNativeSeparators(destPath) << QDir::toNativeSeparators(sourcePath));
        process.waitForFinished();
    #else
        // On Unix/Linux
        QFile::link(sourcePath, destPath);
    #endif
}