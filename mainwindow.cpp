#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    p = new SimpleTimePlot(this);
    p->setRange(0,1,0,1);
    p2 = new SimpleTimePlot(this);
    p2->setRange(0,1,0,1);

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
    connect(timer,SIGNAL(timeout()),this,SLOT(insertPoint()));
    timer->start(1);
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

void MainWindow::insertPoint()
{
    float currX = prevX+0.01f;
    float currY = prevY*0.9+0.1*((float)qrand()/RAND_MAX);
    p->setRange(currX-1,currX,0,1);
    p->addPoint(currX,currY);
    prevX = currX;
    prevY = currY;
    float currX2 = prevX2+0.01f;
    float currY2 = prevY2*0.9+0.1*((float)qrand()/RAND_MAX);
    p2->setRange(currX2-1,currX2,0,1);
    p2->addPoint(currX2,currY2);
    prevX2 = currX2;
    prevY2 = currY2;

    p2->update();
    p->update();
}
