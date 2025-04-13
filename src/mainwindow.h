#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>

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
    void browseDestinationFolder();
    void moveAndCreateSymlinks();
    void clearList();

private:
    QListWidget *fileListWidget;
    QLineEdit *destinationPathEdit;
    QPushButton *browseButton;
    QPushButton *moveButton;
    QPushButton *clearButton;
    QLabel *statusLabel;
    
    void setupUi();
    void createSymbolicLink(const QString &sourcePath, const QString &destPath);
};

#endif // MAINWINDOW_H