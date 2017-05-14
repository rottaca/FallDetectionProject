#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QPainter>

#include "settings.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    plotEventsInWindow = new SimpleTimePlot(this);
    plotEventsInWindow->setYRange(0,70000);
    plotEventsInWindow->setXRange(100000);
    plotEventsInWindow->setTitle("Event Count");

    plotVerticalCentroid = new SimpleTimePlot(this);
    plotVerticalCentroid->setYRange(0,DAVIS_IMG_HEIGHT);
    plotVerticalCentroid->setXRange(100000);
    plotVerticalCentroid->setTitle("Vertical centroid");

    ui->gridLayout->addWidget(plotEventsInWindow);
    ui->gridLayout->addWidget(plotVerticalCentroid);

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
    Processor::sObjectStats stats = proc.getStats();

    int time = buff.getCurrTime();
    int evCnt = buff.getSize();
    plotEventsInWindow->addPoint(time,evCnt);
    plotVerticalCentroid->addPoint(time,stats.center.y());
    plotEventsInWindow->update();
    plotVerticalCentroid->update();

    QImage img = buff.toImage();
    QPixmap pix = QPixmap::fromImage(img);
    QPainter painter(&pix);
    QPen pen(Qt::red,4);
    painter.setPen(pen);
    painter.drawRect(stats.center.x()-stats.std.x(),stats.center.y()-stats.std.y(),2*stats.std.x(),2*stats.std.y());
    QPen pen2(Qt::green,4);
    painter.setPen(pen2);
    painter.drawPoint(stats.center);
    painter.end();

    ui->label->setPixmap(pix);
    ui->label_2->setText(QString("Events: %1").arg(evCnt));
}
