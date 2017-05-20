#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTime>
#include <QPainter>

#include "settings.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    plotEventsInWindow = new SimpleTimePlot(this);
    plotEventsInWindow->setYRange(0,20000);
    plotEventsInWindow->setXRange(PLOT_TIME_RANGE_US);
    plotEventsInWindow->setTitle("Event Count");

    plotVerticalCentroid = new SimpleTimePlot(this);
    plotVerticalCentroid->setYRange(0,DAVIS_IMG_HEIGHT);
    plotVerticalCentroid->setXRange(PLOT_TIME_RANGE_US);
    plotVerticalCentroid->setTitle("Vertical centroid");

    plotSpeed = new SimpleTimePlot(this);
    plotSpeed->setYRange(-10,10);
    plotSpeed->setXRange(PLOT_TIME_RANGE_US);
    plotSpeed->setTitle("Centroid horizontal speed");

    ui->gridLayout->addWidget(plotEventsInWindow);
    ui->gridLayout->addWidget(plotVerticalCentroid);
    ui->gridLayout->addWidget(plotSpeed);

    camHandler.setDVSEventReciever(&proc);
    camHandler.setFrameReciever(&proc);
    camHandler.connect(1);
    proc.start();
    camHandler.startStreaming();

    timer = new QTimer(this);
    connect(timer,SIGNAL(timeout()),this,SLOT(redrawUI()));
    timer->start(UPDATE_INTERVAL_UI_US/1000);
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

void MainWindow::redrawUI()
{
    EventBuffer & buff = proc.getBuffer();
    Processor::sObjectStats stats = proc.getStats().at(0);

    int time = buff.getCurrTime();
    int evCnt = stats.evCnt;
    plotEventsInWindow->addPoint(time,evCnt);
    plotVerticalCentroid->addPoint(time,DAVIS_IMG_HEIGHT-1-stats.center.y());
    plotSpeed->addPoint(time,stats.velocity.y()/stats.std.y());

    plotEventsInWindow->update();
    plotVerticalCentroid->update();
    plotSpeed->update();
    QPen penRed(Qt::red);
    QPen penBlue(Qt::blue);
    QPen penGreen(Qt::green,4);

    QImage img = buff.toImage();
    QPixmap pix = QPixmap::fromImage(img);
    QPainter painter(&pix);
    for(Processor::sObjectStats stats: proc.getStats()) {
        painter.setPen(penRed);
        painter.drawRect(stats.bbox);
        painter.setPen(penBlue);
        painter.drawRect(stats.roi);
        painter.setPen(penGreen);
        painter.drawPoint(stats.center);
    }
    painter.end();
    ui->label->setPixmap(pix);

    img = proc.getImg();
    pix = QPixmap::fromImage(img);
    QPainter painter2(&pix);
    painter2.setPen(penRed);
    painter2.drawText(5,painter2.fontMetrics().height(),
                      QString("%1 FPS").arg((double)proc.getFrameFPS(),0,'g',3));
    painter2.end();
    ui->label_3->setPixmap(pix);
    ui->label_2->setText(QString("Events: %1\nProc FPS: %2").arg(evCnt).arg(proc.getProcessingFPS()));

}
