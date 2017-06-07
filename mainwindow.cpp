#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTime>
#include <QPainter>

#include "settings.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_uiRedrawFPS(0)
{
    ui->setupUi(this);


    for(int i = 0; i < TRACK_BIGGEST_N_BOXES; i++) {
        imgFalls[i] = new AspectRatioPixmap(this);
        ui->lay_foundFallsBoxes->addWidget(imgFalls[i]);
    }

    plotEventsInWindow = new SimpleTimePlot(this);
    plotEventsInWindow->setYRange(0,20000);
    plotEventsInWindow->setXRange(PLOT_TIME_RANGE_US);
    plotEventsInWindow->setTitle("Event Count");

    plotVerticalCentroid = new SimpleTimePlot(this);
    plotVerticalCentroid->setYRange(0,DAVIS_IMG_HEIGHT);
    plotVerticalCentroid->setXRange(PLOT_TIME_RANGE_US);
    plotVerticalCentroid->setTitle("Vertical centroid");

    plotSpeed = new SimpleTimePlot(this);
    plotSpeed->setYRange(-5,5 );
    plotSpeed->setXRange(PLOT_TIME_RANGE_US);
    plotSpeed->setTitle("Centroid horizontal speed");

    ui->gridLayout->addWidget(plotEventsInWindow);
    ui->gridLayout->addWidget(plotVerticalCentroid);
    ui->gridLayout->addWidget(plotSpeed);

    camHandler.setDVSEventReciever(&proc);
    camHandler.setFrameReciever(&proc);
    camHandler.connect(1);
    //camHandler.connect("/tausch/FallDetectionProjectRecords/fall1.aedat");
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
    uint32_t elapsedTime = m_realRedrawTimer.nsecsElapsed()/1000;
    m_realRedrawTimer.restart();
    m_uiRedrawFPS = m_uiRedrawFPS*(1-FPS_LOWPASS_FILTER_COEFF) +
                    FPS_LOWPASS_FILTER_COEFF*1000000.0f/elapsedTime;

    EventBuffer & buff = proc.getBuffer();
    QVector<Processor::sObjectStats> statsList = proc.getStats();
    int time = buff.getCurrTime();
    if(statsList.size() > 0) {
        Processor::sObjectStats stats = statsList.at(0);
        plotEventsInWindow->addPoint(time,stats.evCnt);
        plotVerticalCentroid->addPoint(time,DAVIS_IMG_HEIGHT-1-stats.center.y());
        plotSpeed->addPoint(time,stats.velocityNorm.y());
        int evCnt = buff.getSize();
        ui->l_status->setText(QString("Events: %1 GUI FPS: %2").arg(evCnt).arg(m_uiRedrawFPS,0,'g',3));
    } else {
        plotEventsInWindow->addPoint(time,nan(""));
        plotVerticalCentroid->addPoint(time,nan(""));
        plotSpeed->addPoint(time,nan(""));

    }
    plotEventsInWindow->update();
    plotVerticalCentroid->update();
    plotSpeed->update();
    QPen penRed(Qt::red);
    QPen penBlue(Qt::blue);
    QPen penCyan(Qt::cyan);

    QPen penGreen(Qt::green,4);
    QPen penGreenSmall(Qt::green,1);

    QImage grayImg = proc.getImg();
    QImage bufferImg = buff.toImage();
    QPixmap pix = QPixmap::fromImage(bufferImg);
    QPainter painter(&pix);


    int idx = 0;
    for(Processor::sObjectStats stats: statsList) {
        if(stats.possibleFall) {
            painter.setPen(penGreenSmall);
            painter.drawRect(stats.roi);
            if(idx < TRACK_BIGGEST_N_BOXES) {
                imgFalls[idx++]->setPixmap(QPixmap::fromImage(grayImg.copy(stats.roi.x(),stats.roi.y(),stats.roi.width(),stats.roi.height())));
            }
        } else {
            if(stats.trackingLost) {
                if(ui->cb_showLostTracking->isChecked())
                    painter.setPen(penCyan);
                else
                    continue;
            } else
                painter.setPen(penBlue);
            painter.drawRect(stats.roi);
        }

        painter.setPen(penRed);
        painter.drawRect(stats.stdDevBox);

        painter.setPen(penGreen);
        painter.drawPoint(stats.center);

        painter.setPen(penRed);
        QString str = QString("%1").arg(stats.id);
        painter.drawText(stats.roi.x()+stats.roi.width()-painter.fontMetrics().width(str)-2,
                         stats.roi.y()+painter.fontMetrics().height(),
                         str);
    }
    for (int j = idx; j < TRACK_BIGGEST_N_BOXES; j++)
        imgFalls[idx++]->clear();

    painter.setPen(penRed);
    painter.drawText(5,painter.fontMetrics().height(),
                     QString("%1 FPS").arg((double)proc.getProcessingFPS(),0,'g',3));
    painter.end();
    ui->l_events->setPixmap(pix);

    pix = QPixmap::fromImage(grayImg);
    QPainter painter2(&pix);
    painter2.setPen(penRed);
    painter2.drawText(5,painter2.fontMetrics().height(),
                      QString("%1 FPS").arg((double)proc.getFrameFPS(),0,'g',3));

    for(Processor::sObjectStats stats: statsList) {
        if(!ui->cb_showLostTracking->isChecked())
            continue;
        painter2.setPen(penGreenSmall);
        painter2.drawRect(stats.roi);
    }
    painter2.end();
    ui->l_gray->setPixmap(pix);

    ui->l_threshold->setPixmap(QPixmap::fromImage(proc.getThresholdImg()));

}
