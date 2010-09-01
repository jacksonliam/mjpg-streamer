#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QRegExpValidator *v = new QRegExpValidator(this);
    QRegExp rx("((1{0,1}[0-9]{0,2}|2[0-4]{1,1}[0-9]{1,1}|25[0-5]{1,1})\\.){3,3}(1{0,1}[0-9]{0,2}|2[0-4]{1,1}[0-9]{1,1}|25[0-5]{1,1})");
    v->setRegExp(rx);
    ui->lineEditServer->setValidator(v);

    socket = new QUdpSocket(this);
    connect(socket, SIGNAL(connected()), this, SLOT(socketConnected()));
    connect(socket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(socketError(QAbstractSocket::SocketError)));
    connect(socket, SIGNAL(stateChanged(QAbstractSocket::SocketState)), this, SLOT(socketStateChanged(QAbstractSocket::SocketState)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    socket->connectToHost(QHostAddress(ui->lineEditServer->text()), ui->spinBoxPort->value(), QUdpSocket::ReadWrite);
}

void MainWindow::socketConnected()
{
    ui->labelStatus->setText("Connected");
    ui->pushButton->setEnabled(false);
    socket->write(ui->lineEditFileName->text().toAscii());
    socket->close();
}

void MainWindow::socketStateChanged(QAbstractSocket::SocketState state)
{
    if (state == QAbstractSocket::UnconnectedState) {
        ui->labelStatus->setText("File saved");
        ui->pushButton->setEnabled(true);
    }
}

void MainWindow::socketError(QAbstractSocket::SocketError err)
{
    ui->labelStatus->setText(QString("Error: %1").arg(err));
    ui->pushButton->setEnabled(true);
}
