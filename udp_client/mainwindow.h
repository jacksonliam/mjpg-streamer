#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUdpSocket>
#include <QRegExp>
#include <QRegExpValidator>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QUdpSocket *socket;

private slots:
    void on_pushButton_clicked();
    void socketConnected();
    void socketError(QAbstractSocket::SocketError);
    void socketStateChanged(QAbstractSocket::SocketState);
};

#endif // MAINWINDOW_H
