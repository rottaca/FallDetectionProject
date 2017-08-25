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
    plotEventsInWindow = new SimpleTimePlot(this);
    plotEventsInWindow->setYRange(0,40000);
    plotEventsInWindow->setXRange(PLOT_TIME_RANGE_US);
    plotEventsInWindow->setTitle("Event Count");

    plotVerticalCentroid = new SimpleTimePlot(this);
    plotVerticalCentroid->setYRange(0,180);
    plotVerticalCentroid->setXRange(PLOT_TIME_RANGE_US);
    plotVerticalCentroid->setTitle("Vertical centroid");

    plotSpeed = new SimpleTimePlot(this);
    plotSpeed->setYRange(-settings.fall_detector_y_speed_max_threshold,settings.fall_detector_y_speed_max_threshold );
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

    QPen penRed(Qt::red);
    QPen penBlue(Qt::blue);
    QPen penCyan(Qt::cyan);
    QPen penYellow(Qt::yellow);
    QPen penGreenThick(Qt::green,4);
    QPen penGreen(Qt::green);
    QPen penOrange(QColor(255,69,0));


    plotEventsInWindow->setLineGroupActive(0,ui->cb_showFallsInGraph->isChecked());
    plotVerticalCentroid->setLineGroupActive(0,ui->cb_showFallsInGraph->isChecked());
    plotSpeed->setLineGroupActive(0,ui->cb_showFallsInGraph->isChecked());
    plotEventsInWindow->setLineGroupActive(1,ui->cb_showLostTrackingInGraph->isChecked());
    plotVerticalCentroid->setLineGroupActive(1,ui->cb_showLostTrackingInGraph->isChecked());
    plotSpeed->setLineGroupActive(1,ui->cb_showLostTrackingInGraph->isChecked());

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
            if((stats.confirmendFall || stats.possibleFall) && ui->cb_showFallsInGraph->isChecked()) {
                plotEventsInWindow->addLine(0,stats.fallTime,penRed);
                plotVerticalCentroid->addLine(0,stats.fallTime,penRed);
                plotSpeed->addLine(0,stats.fallTime,penRed);
            }
            if(stats.trackingLostHistory[0] && ui->cb_showLostTrackingInGraph->isChecked()) {
                plotEventsInWindow->addLine(1,time,penCyan);
                plotVerticalCentroid->addLine(1,time,penCyan);
                plotSpeed->addLine(1,time,penCyan);
            }
        } else {
            if(lastObjId != -1) {
                plotEventsInWindow->addPoint(time,INFINITY);
                plotVerticalCentroid->addPoint(time,INFINITY);
                plotSpeed->addPoint(time,INFINITY);
                plotEventsInWindow->addLine(2,time,penBlue);
                plotVerticalCentroid->addLine(2,time,penBlue);
                plotSpeed->addLine(2,time,penBlue);
            }
            lastObjId = -1;
        }
        plotEventsInWindow->update();
        plotVerticalCentroid->update();
        plotSpeed->update();

        QImage grayImg = proc.getImg();
        QImage bufferImg = buff.toImage();
        QPixmap pix = QPixmap::fromImage(bufferImg);
        QPainter painterEventImg(&pix);

        if(ui->cb_showHelpLines->isChecked()) {
            painterEventImg.setPen(penRed);
            painterEventImg.drawRect(TRACK_IMG_BORDER_SIZE_HORIZONTAL,TRACK_IMG_BORDER_SIZE_VERTICAL,
                                     grayImg.width()-2*TRACK_IMG_BORDER_SIZE_HORIZONTAL,
                                     grayImg.height()-2*TRACK_IMG_BORDER_SIZE_VERTICAL);
            painterEventImg.setPen(penRed);
            painterEventImg.drawLine(1,settings.fall_detector_y_center_threshold_fall,
                                     grayImg.width()-1,settings.fall_detector_y_center_threshold_fall);
            painterEventImg.setPen(penGreen);
            painterEventImg.drawLine(1,settings.fall_detector_y_center_threshold_unfall,
                                     grayImg.width()-1,settings.fall_detector_y_center_threshold_unfall);
        }

        for(Processor::sObjectStats stats: statsList) {

            if(stats.trackingLostHistory[0] && !ui->cb_showLostTrackingBBox->isChecked())
                continue;

            if(stats.confirmendFall) {
                if(stats.trackingLostHistory[0])
                    painterEventImg.setPen(penYellow);
                else
                    painterEventImg.setPen(penGreen);
            } else if(stats.possibleFall) {
                if(stats.trackingLostHistory[0])
                    painterEventImg.setPen(penYellow);
                else
                    painterEventImg.setPen(penOrange);
            } else {
                if(stats.trackingLostHistory[0])
                    painterEventImg.setPen(penCyan);
                else
                    painterEventImg.setPen(penBlue);
            }
            painterEventImg.drawRect(stats.roi);

            painterEventImg.setPen(penRed);
            painterEventImg.drawLine(stats.center - QPointF(stats.std.x(),0), stats.center + QPointF(stats.std.x(),0));
            painterEventImg.drawLine(stats.center - QPointF(0,stats.std.y()), stats.center + QPointF(0,stats.std.y()));

            painterEventImg.setPen(penGreenThick);
            painterEventImg.drawPoint(stats.center);

            painterEventImg.drawLine(stats.center,stats.center+stats.velocity);

            painterEventImg.setPen(penRed);
            QString str = QString("%1").arg(stats.id);
            painterEventImg.drawText(stats.roi.x()+stats.roi.width()-painterEventImg.fontMetrics().width(str)-2,
                                     stats.roi.y()+painterEventImg.fontMetrics().height(),
                                     str);
        }
        painterEventImg.setPen(penRed);
        painterEventImg.drawText(5,painterEventImg.fontMetrics().height(),
                                 QString("%1 FPS").arg((double)proc.getProcessingFPS(),0,'g',3));
        painterEventImg.end();
        ui->l_events->setPixmap(pix);

        pix = QPixmap::fromImage(grayImg);
        QPainter painterGrayImg(&pix);
        painterGrayImg.setPen(penRed);
        painterGrayImg.drawText(5,painterGrayImg.fontMetrics().height(),
                                QString("%1 FPS").arg((double)proc.getFrameFPS(),0,'g',3));

        for(Processor::sObjectStats stats: statsList) {
            if(stats.trackingLostHistory[0] && !ui->cb_showLostTrackingBBox->isChecked())
                continue;
            painterGrayImg.setPen(penGreen);
            painterGrayImg.drawRect(stats.roi);
        }
        painterGrayImg.end();
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
