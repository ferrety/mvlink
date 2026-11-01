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
#include <QFrame>
#include <QToolButton>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QStyleFactory>
#include <QApplication>
#include <QAbstractItemModel>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>

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
const char hardLinkKey[] = "hardLink";
constexpr int failureRole = Qt::UserRole + 1;

class FileDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const override
    {
        return QSize(120, qMax(64, option.fontMetrics.height() * 2 + 24));
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem panel(option);
        initStyleOption(&panel, index);
        panel.text.clear();
        panel.icon = QIcon();
        const QStyle *style = option.widget->style();
        style->drawControl(QStyle::CE_ItemViewItem, &panel, painter, option.widget);
        const bool failed = !index.data(failureRole).toString().isEmpty();
        const bool selected = option.state & QStyle::State_Selected;
        const QColor primary = selected ? option.palette.highlightedText().color() : QColor("#182230");
        const QColor secondary = selected ? primary : QColor("#526174");
        const QRect area = option.rect.adjusted(12, 8, -12, -8);
        const QRect iconRect(area.left(), area.center().y() - 12, 24, 24);
        if (failed) {
            // Draw the warning directly so stripped Windows packages need no icon plugin.
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(QPen(QColor("#B54708"), 1.5));
            painter->setBrush(QColor("#FEF0C7"));
            painter->drawPolygon(QPolygon{iconRect.topLeft() + QPoint(12, 2),
                iconRect.bottomLeft() + QPoint(1, -2), iconRect.bottomRight() + QPoint(-1, -2)});
            QFont warningFont(option.font);
            warningFont.setBold(true);
            painter->setFont(warningFont);
            painter->drawText(iconRect.adjusted(0, 4, 0, 0), Qt::AlignCenter, "!");
            painter->restore();
        } else {
            style->standardIcon(QStyle::SP_FileIcon).paint(painter, iconRect);
        }
        const int left = area.left() + 38;
        const int width = qMax(0, area.right() - left);
        painter->save();
        QFont font(option.font);
        font.setBold(true);
        painter->setFont(font);
        painter->setPen(failed && !selected ? QColor("#B42318") : primary);
        painter->drawText(QRect(left, area.top(), width, area.height() / 2), Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(index.data().toString(), Qt::ElideMiddle, width));
        painter->setFont(option.font);
        painter->setPen(secondary);
        const QString folder = QDir::toNativeSeparators(QFileInfo(index.data(Qt::UserRole).toString()).absolutePath());
        painter->drawText(QRect(left, area.center().y(), width, area.height() / 2), Qt::AlignVCenter,
                          option.fontMetrics.elidedText(folder, Qt::ElideMiddle, width));
        painter->restore();
    }
};

class UserSettings : public QSettings
{
public:
    UserSettings() : QSettings(QSettings::IniFormat, QSettings::UserScope, "mvlink", "mvlink")
    {
        setFallbacksEnabled(false);
        if (!contains(destinationHistoryKey)) {
            QSettings legacy(QSettings::NativeFormat, QSettings::UserScope, "mvlink", "mvlink");
            legacy.setFallbacksEnabled(false);
            if (legacy.contains(destinationHistoryKey)) {
                setValue(destinationHistoryKey, legacy.value(destinationHistoryKey));
                sync();
            }
        }
    }
};
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
    // The list model emits modelReset while its child widgets are destroyed.
    disconnect(fileListWidget->model(), nullptr, this, nullptr);
    fileListWidget->viewport()->removeEventFilter(this);
}

void MainWindow::setupUi()
{
    // QWidget::setStyle does not propagate to child controls.
    if (QApplication::style()->objectName() != "fusion")
        QApplication::setStyle(QStyleFactory::create("Fusion"));
    QStyle *fusion = QApplication::style();
    QPalette colors = fusion->standardPalette();
    colors.setColor(QPalette::Window, QColor("#F4F6FA"));
    colors.setColor(QPalette::Base, Qt::white);
    colors.setColor(QPalette::Text, QColor("#182230"));
    colors.setColor(QPalette::WindowText, QColor("#182230"));
    colors.setColor(QPalette::Highlight, QColor("#2563EB"));
    colors.setColor(QPalette::HighlightedText, Qt::white);
    setPalette(colors);
    QFile theme(":/styles/theme.qss");
    if (theme.open(QIODevice::ReadOnly))
        setStyleSheet(QString::fromUtf8(theme.readAll()));

    QWidget *central = new QWidget(this);
    central->setObjectName("central");
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(12);

    auto *header = new QHBoxLayout();
    auto *brand = new QLabel();
    brand->setPixmap(QIcon(":/icons/mvlink-256.png").pixmap(QSize(48, 48), devicePixelRatioF()));
    brand->setToolTip("mvlink — move files and create links");
    header->addWidget(brand);
    auto *titles = new QVBoxLayout();
    titles->setSpacing(2);
    auto *title = new QLabel("mvlink");
    title->setObjectName("title");
    title->setToolTip("Move files and create links");
    auto *subtitle = new QLabel("Move files and create links");
    subtitle->setObjectName("muted");
    subtitle->setToolTip("Move files to a destination or keep them in place and create links.");
    titles->addWidget(title);
    titles->addWidget(subtitle);
    header->addLayout(titles);
    header->addStretch();
    layout->addLayout(header);

    auto *pathLayout = new QHBoxLayout();
    pathLayout->setSpacing(8);
    destinationPathEdit = new QComboBox();
    destinationPathEdit->setAccessibleName("Destination Folder");
    destinationPathEdit->setToolTip("Type a destination folder or choose one from your saved history.");
    destinationPathEdit->setEditable(true);
    destinationPathEdit->setInsertPolicy(QComboBox::NoInsert);
    destinationPathEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    destinationPathEdit->setMinimumWidth(0);
    UserSettings settings;
    destinationPathEdit->addItems(settings.value(destinationHistoryKey).toStringList());
    destinationPathEdit->setCurrentText(QString());
    pathLayout->addWidget(destinationPathEdit, 1);
    browseButton = new QPushButton("Browse...");
    browseButton->setToolTip("Open the folder picker to choose a destination.");
    pathLayout->addWidget(browseButton);
    layout->addLayout(pathLayout);

    filesPanel = new QFrame();
    filesPanel->setObjectName("filesPanel");
    auto *filesLayout = new QVBoxLayout(filesPanel);
    filesLayout->setContentsMargins(12, 8, 12, 12);
    filesLayout->setSpacing(4);
    auto *filesHeader = new QHBoxLayout();
    auto *filesTitle = new QLabel("Files");
    filesTitle->setObjectName("sectionTitle");
    filesTitle->setToolTip("Files queued for moving or linking.");
    fileCountLabel = new QLabel();
    fileCountLabel->setObjectName("muted");
    fileCountLabel->setToolTip("Number of files remaining in the list.");
    clearButton = new QPushButton("Clear List");
    clearButton->setObjectName("quiet");
    clearButton->setToolTip("Remove all files from this list without changing files on disk.");
    filesHeader->addWidget(filesTitle);
    filesHeader->addWidget(fileCountLabel);
    filesHeader->addStretch();
    filesHeader->addWidget(clearButton);
    filesLayout->addLayout(filesHeader);
    fileListWidget = new QListWidget();
    fileListWidget->setAccessibleName("Files to move or link");
    fileListWidget->setToolTip("Drop files here. Failed or skipped files remain for retry.");
    fileListWidget->setMinimumHeight(128);
    fileListWidget->setDragEnabled(false);
    fileListWidget->setAcceptDrops(true);
    fileListWidget->viewport()->installEventFilter(this);
    fileListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    fileListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fileListWidget->setItemDelegate(new FileDelegate(fileListWidget));
    filesLayout->addWidget(fileListWidget);
    layout->addWidget(filesPanel, 1);

    emptyState = new QWidget(fileListWidget->viewport());
    emptyState->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *emptyLayout = new QVBoxLayout(emptyState);
    emptyLayout->addStretch();
    auto *emptyIcon = new QLabel();
    emptyIcon->setPixmap(QIcon(":/icons/mvlink-256.png").pixmap(QSize(64, 64), devicePixelRatioF()));
    emptyIcon->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyIcon);
    auto *emptyTitle = new QLabel("Drop files here");
    emptyTitle->setObjectName("emptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyTitle);
    auto *emptyHint = new QLabel("Files will appear here before you move or link them.");
    emptyHint->setObjectName("muted");
    emptyHint->setWordWrap(true);
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(emptyHint);
    emptyLayout->addStretch();

    hardLinkCheckBox = new QCheckBox("Create hard links instead of symbolic links");
    hardLinkCheckBox->setToolTip("Hard links require the same filesystem. Edits through either path affect the same file. This choice is saved.");
    hardLinkCheckBox->setChecked(settings.value(hardLinkKey, false).toBool());
    layout->addWidget(hardLinkCheckBox);
    linkExplanation = new QLabel();
    linkExplanation->setObjectName("muted");
    linkExplanation->setWordWrap(true);
    linkExplanation->setToolTip("How the selected link type behaves.");
    layout->addWidget(linkExplanation);
    connect(hardLinkCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        UserSettings settings;
        settings.setValue(hardLinkKey, checked);
        settings.sync();
        updateUiState();
    });

    auto *actions = new QHBoxLayout();
    statusLabel = new QLabel();
    statusLabel->setWordWrap(true);
    statusLabel->setToolTip("Current status or the result of the latest operation.");
    actions->addWidget(statusLabel, 1);
    linkButton = new QPushButton("Link");
    linkButton->setToolTip("Keep the original files and create links in the destination.");
    moveButton = new QPushButton("Move Files");
    moveButton->setObjectName("primary");
    moveButton->setToolTip("Place files in the destination and leave links at their original paths.");
    actions->addWidget(linkButton);
    actions->addWidget(moveButton);
    layout->addLayout(actions);

    activityButton = new QToolButton();
    activityButton->setText("Activity");
    activityButton->setCheckable(true);
    activityButton->setArrowType(Qt::RightArrow);
    activityButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    activityButton->setToolTip("Show or hide operation details and errors.");
    layout->addWidget(activityButton);
    logView = new QTextEdit();
    logView->setAccessibleName("Activity log");
    logView->setToolTip("Review operation details and errors. Select and copy text as needed.");
    logView->setReadOnly(true);
    logView->setFixedHeight(96);
    logView->hide();
    layout->addWidget(logView);
    connect(activityButton, &QToolButton::toggled, this, [this](bool expanded) {
        activityButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        logView->setVisible(expanded);
        setMinimumHeight(qMax(520, centralWidget()->minimumSizeHint().height()));
    });
    auto *model = fileListWidget->model();
    connect(model, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateUiState);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateUiState);
    connect(model, &QAbstractItemModel::modelReset, this, &MainWindow::updateUiState);
    updateUiState();
    setMinimumSize(580, 520);
    resize(760, 580);
}

void MainWindow::updateUiState()
{
    const int count = fileListWidget->count();
    fileCountLabel->setText(QString::number(count));
    emptyState->setVisible(count == 0);
    emptyState->setGeometry(fileListWidget->viewport()->rect());
    moveButton->setEnabled(count > 0);
    linkButton->setEnabled(count > 0);
    clearButton->setEnabled(count > 0);
    linkExplanation->setText(hardLinkCheckBox->isChecked()
        ? "Hard links share file contents and require the same filesystem."
        : "Symbolic links point to a file at another path.");
}

void MainWindow::setDropHighlight(bool active)
{
    filesPanel->setProperty("dragActive", active);
    filesPanel->style()->unpolish(filesPanel);
    filesPanel->style()->polish(filesPanel);
    filesPanel->update();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (fileListWidget && watched == fileListWidget->viewport()) {
        if (event->type() == QEvent::Resize && emptyState)
            emptyState->setGeometry(fileListWidget->viewport()->rect());
        else if (event->type() == QEvent::DragEnter) {
            dragEnterEvent(static_cast<QDragEnterEvent *>(event));
            return event->isAccepted();
        } else if (event->type() == QEvent::DragMove) {
            auto *drag = static_cast<QDragMoveEvent *>(event);
            if (drag->mimeData()->hasUrls()) {
                drag->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::DragLeave) {
            dragLeaveEvent(static_cast<QDragLeaveEvent *>(event));
            return true;
        } else if (event->type() == QEvent::Drop) {
            dropEvent(static_cast<QDropEvent *>(event));
            return event->isAccepted();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    setDropHighlight(false);
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        setDropHighlight(true);
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    setDropHighlight(false);
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        
        for (const QUrl &url : urlList) {
            QString filePath = url.toLocalFile();
            QFileInfo fileInfo(filePath);
            
            if (fileInfo.isFile()) {
                QListWidgetItem *item = new QListWidgetItem(fileInfo.fileName());
                item->setToolTip(filePath);
                item->setData(Qt::UserRole, filePath);
                fileListWidget->addItem(item);
            }
        }
        
        QString message = QString("%1 files ready to be moved").arg(fileListWidget->count());
        statusLabel->setText(message);
        logMessage(message);
        event->acceptProposedAction();
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
    const QString destDir = destinationPathEdit->currentText();
    if (destDir.isEmpty()) {
        QMessageBox::warning(this, "Warning", "Please select a destination folder first.");
        logMessage("Error: No destination folder selected");
        activityButton->setChecked(true);
        return;
    }
    QDir dir(destDir);
    if (!dir.exists()) {
        QMessageBox::critical(this, "Error", "Destination folder does not exist.");
        logMessage("Error: Destination folder does not exist");
        activityButton->setChecked(true);
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
    logView->append(message.toHtmlEscaped());
    
    // scroll to latest message
    QScrollBar *scrollBar = logView->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void MainWindow::markFileFailed(QListWidgetItem *item, const QString &reason)
{
    failCount++;
    logMessage(reason);
    item->setForeground(QColor("#B42318"));
    item->setData(failureRole, reason);
    item->setToolTip(item->data(Qt::UserRole).toString() + "\n" + reason);
    activityButton->setChecked(true);
}

void MainWindow::rememberDestination(const QString &path)
{
    const QString normalizedPath = QDir::cleanPath(path);
    UserSettings settings;
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
