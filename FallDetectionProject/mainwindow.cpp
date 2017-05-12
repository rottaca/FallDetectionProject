#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&proc,SIGNAL(updateUI(QString)),this,SLOT(updateUI(QString)));

    camHandler.setDVSEventReciever(&proc);
    camHandler.setFrameReciever(&proc);
    camHandler.connect(1);
    proc.start();
    camHandler.startStreaming();
}

MainWindow::~MainWindow()
{
    delete ui;
}
void MainWindow::closeEvent (QCloseEvent *event)
{
    camHandler.disconnect();
    proc.stop();
}
void MainWindow::updateUI(QString msg)
{
    ui->label->setText(msg);
}
