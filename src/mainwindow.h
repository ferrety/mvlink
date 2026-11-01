#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QDragEnterEvent>
#include <QDropEvent>

class QFrame;
class QToolButton;
class TestMainWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void browseDirectory();
    void handleMove();
    void handleLink();
    void clearList();

private:
    friend class TestMainWindow;

    QListWidget *fileListWidget = nullptr;
    QComboBox *destinationPathEdit;
    QPushButton *browseButton;
    QPushButton *moveButton;
    QPushButton *clearButton;
    QPushButton *linkButton;
    QCheckBox *hardLinkCheckBox;
    QLabel *statusLabel;
    QTextEdit *logView;
    QFrame *filesPanel;
    QWidget *emptyState = nullptr;
    QLabel *fileCountLabel;
    QLabel *linkExplanation;
    QToolButton *activityButton;
    int failCount = 0;
    
    void setupUi();
    void updateUiState();
    void setDropHighlight(bool active);
    void processFiles(bool linkOnly);
    bool createSymbolicLink(const QString &sourcePath, const QString &destPath, QString *error = nullptr);
    bool createHardLink(const QString &sourcePath, const QString &destPath, QString *error = nullptr);
    void logMessage(const QString &message);
    void markFileFailed(QListWidgetItem *item, const QString &reason);
    void rememberDestination(const QString &path);
};

#endif // MAINWINDOW_H
