#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    p = new SimpleTimePlot(this);
    p->setRange(0,1,0,1);
    p->enableTimePlotMode(true,1);
    p2 = new SimpleTimePlot(this);
    p2->setRange(0,1,0,30000);
    p2->enableTimePlotMode(true,10000);

    prevX = 0;
    prevY = 0;
    prevX2 = 0;
    prevY2 = 0;

    ui->gridLayout->addWidget(p);
    ui->gridLayout->addWidget(p2);

    connect(&proc,SIGNAL(updateUI(QString)),this,SLOT(updateUI(QString)));

    camHandler.setDVSEventReciever(&proc);
    camHandler.setFrameReciever(&proc);
    camHandler.connect(1);
    proc.start();
    camHandler.startStreaming();

    timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()),this,SLOT(redrawUI()));
    timer->start(30);
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

}

void MainWindow::redrawUI()
{
    EventBuffer & buff = proc.getBuffer();

    float currX = prevX+0.01f;
    float currY = prevY*0.9+0.1*((float)qrand()/RAND_MAX);
    p->addPoint(currX,currY);
    prevX = currX;
    prevY = currY;
    float currY2 = buff.getSize();
    int time = buff.getCurrTime();
    p2->addPoint(time,currY2);

    p2->update();
    p->update();

    ui->label->setPixmap(QPixmap::fromImage(buff.toImage()));

}
