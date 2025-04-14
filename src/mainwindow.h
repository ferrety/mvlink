#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QDragEnterEvent>
#include <QDropEvent>

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

private slots:
    void browseDirectory();
    void handleMove();
    void clearList();

private:
    friend class TestMainWindow;

    QListWidget *fileListWidget;
    QLineEdit *destinationPathEdit;
    QPushButton *browseButton;
    QPushButton *moveButton;
    QPushButton *clearButton;
    QLabel *statusLabel;
    QTextEdit *logView;
    int failCount = 0;
    
    void setupUi();
    void createSymbolicLink(const QString &sourcePath, const QString &destPath);
    void logMessage(const QString &message);
    void markFileFailed(QListWidgetItem *item, const QString &reason);
};

#endif // MAINWINDOW_H