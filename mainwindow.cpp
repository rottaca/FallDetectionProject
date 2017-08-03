#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTime>
#include <QPainter>
#include <QMessageBox>
#include <QFileDialog>

#include "settings.h"


void callbackPlaybackStopped(void * p)
{
    MainWindow *w = (MainWindow*)p;
    w->callbackProcessingStopped();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_uiRedrawFPS(0)
{
    ui->setupUi(this);


    setupUI();

    connect(ui->b_online_connect,SIGNAL(clicked()),this,SLOT(onClickOnlineConnect()));
    connect(ui->b_playback_connect,SIGNAL(clicked()),this,SLOT(onClickPlaybackConnect()));
    connect(ui->b_playback_browse,SIGNAL(clicked()),this,SLOT(onClickBrowsePlaybackFile()));
    connect(ui->dsb_playspeed,SIGNAL(editingFinished()),this,SLOT(onPlayspeedChanged()));

    processingStopped = false;
    exitAfterPlayback = false;

    camHandler.setDVSEventReciever(&proc);
    camHandler.setFrameReciever(&proc);

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

void MainWindow::setupUI()
{
    for(int i = 0; i < TRACK_BIGGEST_N_BOXES; i++) {
        imgFalls[i] = new AspectRatioPixmap(this);
        ui->lay_foundFallsBoxes->addWidget(imgFalls[i]);
    }

    plotEventsInWindow = new SimpleTimePlot(this);
    plotEventsInWindow->setYRange(0,40000);
    plotEventsInWindow->setXRange(PLOT_TIME_RANGE_US);
    plotEventsInWindow->setTitle("Event Count");

    plotVerticalCentroid = new SimpleTimePlot(this);
    plotVerticalCentroid->setYRange(0,180);
    plotVerticalCentroid->setXRange(PLOT_TIME_RANGE_US);
    plotVerticalCentroid->setTitle("Vertical centroid");

    plotSpeed = new SimpleTimePlot(this);
    plotSpeed->setYRange(-5,5 );
    plotSpeed->setXRange(PLOT_TIME_RANGE_US);
    plotSpeed->setTitle("Centroid horizontal speed");

    ui->gridLayout->addWidget(plotEventsInWindow);
    ui->gridLayout->addWidget(plotVerticalCentroid);
    ui->gridLayout->addWidget(plotSpeed);
}

void MainWindow::onPlayspeedChanged()
{
    camHandler.changePlaybackSpeed(ui->dsb_playspeed->value());
}

void MainWindow::onClickPlaybackConnect()
{
    if(camHandler.isConnected()) {
        camHandler.disconnect();
        proc.stop();
        ui->b_playback_connect->setEnabled(true);
        ui->b_online_connect->setEnabled(true);
        ui->b_playback_connect->setText("playback");
        ui->b_online_connect->setText("online");
    } else {
        if(!camHandler.connect(ui->l_playback_file->text(),callbackPlaybackStopped,this)) {
            QMessageBox::critical(this,"Error","Can't open file!");
            return;
        }
        onPlayspeedChanged();
        ui->b_online_connect->setEnabled(false);
        ui->b_playback_connect->setText("stop");
        plotEventsInWindow->clear();
        plotSpeed->clear();
        plotVerticalCentroid->clear();

        QVector2D sz = camHandler.getFrameSize();
        proc.start(sz.x(),sz.y());
        camHandler.startStreaming();
    }

}
void MainWindow::onClickOnlineConnect()
{
    if(camHandler.isConnected()) {
        camHandler.disconnect();
        proc.stop();
        ui->b_playback_connect->setEnabled(true);
        ui->b_online_connect->setEnabled(true);
        ui->b_playback_connect->setText("playback");
        ui->b_online_connect->setText("online");
    } else {
        if(!camHandler.connect(1)) {
            QMessageBox::critical(this,"Error","Can't connect to camera!");
            return;
        }
        ui->b_playback_connect->setEnabled(false);
        ui->b_online_connect->setText("stop");
        plotEventsInWindow->clear();
        plotSpeed->clear();
        plotVerticalCentroid->clear();

        QVector2D sz = camHandler.getFrameSize();
        proc.start(sz.x(),sz.y());
        camHandler.startStreaming();
    }
}

void MainWindow::onClickBrowsePlaybackFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                       tr("Open Playback file"), "/tausch/FallDetectionProjectRecords", tr("AEDAT files (*.aedat)"));
    if(!fileName.isEmpty())
        ui->l_playback_file->setText(fileName);
}

void MainWindow::redrawUI()
{
    if(processingStopped) {
        processingStopped = false;
        camHandler.disconnect();
        proc.stop();
        ui->b_playback_connect->setEnabled(true);
        ui->b_online_connect->setEnabled(true);
        ui->b_playback_connect->setText("playback");
        ui->b_online_connect->setText("online");
        if(exitAfterPlayback)
            QApplication::quit();
    }

    uint32_t elapsedTime = m_realRedrawTimer.nsecsElapsed()/1000;
    m_realRedrawTimer.restart();
    m_uiRedrawFPS = m_uiRedrawFPS*(1-FPS_LOWPASS_FILTER_COEFF) +
                    FPS_LOWPASS_FILTER_COEFF*1000000.0f/elapsedTime;


    if(camHandler.isStreaming()) {

        EventBuffer & buff = proc.getBuffer();
        QVector<Processor::sObjectStats> statsList = proc.getStats();
        int time = buff.getCurrTime();

        int evCnt = buff.getSize();
        ui->l_status->setText(QString("Events: %1 GUI FPS: %2").arg(evCnt).arg(m_uiRedrawFPS,0,'g',3));

        if(statsList.size() > 0) {
            Processor::sObjectStats stats = statsList.at(0);
            lastObjId = stats.id;
            plotEventsInWindow->addPoint(time,stats.evCnt);
            plotVerticalCentroid->addPoint(time,stats.center.y());
            plotSpeed->addPoint(time,stats.velocityNorm.y());
        } else {
            if(lastObjId != -1) {
                plotEventsInWindow->addPoint(time,INFINITY);
                plotVerticalCentroid->addPoint(time,INFINITY);
                plotSpeed->addPoint(time,INFINITY);
            }
            lastObjId = -1;
        }
        plotEventsInWindow->update();
        plotVerticalCentroid->update();
        plotSpeed->update();
        QPen penRed(Qt::red);
        QPen penBlue(Qt::blue);
        QPen penCyan(Qt::cyan);

        QPen penGreen(Qt::green,4);
        QPen penGreenSmall(Qt::green,1);
        QPen penOrangeSmall(QColor(255,69,0),1);

        QImage grayImg = proc.getImg();
        QImage bufferImg = buff.toImage();
        QPixmap pix = QPixmap::fromImage(bufferImg);
        QPainter painter(&pix);


        int idx = 0;
        for(Processor::sObjectStats stats: statsList) {
            if(stats.confirmendFall) {
                painter.setPen(penGreenSmall);
                painter.drawRect(stats.roi);
                if(idx < TRACK_BIGGEST_N_BOXES) {
                    imgFalls[idx++]->setPixmap(QPixmap::fromImage(
                                                   grayImg.copy(stats.roi.x(),
                                                           stats.roi.y(),
                                                           stats.roi.width(),
                                                           stats.roi.height())));
                }
            } else if(stats.possibleFall) {
                painter.setPen(penOrangeSmall);
                painter.drawRect(stats.roi);
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
            painter.drawLine(stats.center - QPointF(stats.std.x(),0), stats.center + QPointF(stats.std.x(),0));
            painter.drawLine(stats.center - QPointF(0,stats.std.y()), stats.center + QPointF(0,stats.std.y()));

            painter.setPen(penGreen);
            painter.drawPoint(stats.center);

            painter.drawLine(stats.center,stats.center+stats.velocity);

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
            if(stats.trackingLost && !ui->cb_showLostTracking->isChecked())
                continue;
            painter2.setPen(penGreenSmall);
            painter2.drawRect(stats.roi);
        }
        painter2.end();
        ui->l_gray->setPixmap(pix);

        ui->l_threshold->setPixmap(QPixmap::fromImage(proc.getThresholdImg()));

    } else {
        ui->l_status->setText(QString("GUI FPS: %2").arg(m_uiRedrawFPS,0,'g',3));
    }
}
void MainWindow::playFile(QString fileName)
{
    if(!fileName.isEmpty())
        ui->l_playback_file->setText(fileName);
    exitAfterPlayback = true;

    onClickPlaybackConnect();
}
