#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QCloseEvent>
#include <QTimer>
#include <QElapsedTimer>

#include "simpletimeplot.h"

#include "camerahandler.h"
#include "processor.h"

#include "aspectratiopixmap.h"

namespace Ui
{
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void closeEvent (QCloseEvent *event);

    void playFile(QString fileName);

    void callbackProcessingStopped()
    {
        processingStopped = true;
    }
    void setSettings(tSettings &settings)
    {
        this->settings = settings;
        proc.setSettings(settings);
    }

public slots:
    void redrawUI();

    void onClickPlaybackConnect();
    void onClickOnlineConnect();
    void onClickBrowsePlaybackFile();
    void onPlayspeedChanged();

private:
    void setupUI();
private:
    Ui::MainWindow *ui;
    QTimer* timer;
    CameraHandler camHandler;
    Processor proc;
    float m_uiRedrawFPS;
    QElapsedTimer m_realRedrawTimer;
    SimpleTimePlot *plotEventsInWindow;
    SimpleTimePlot *plotVerticalCentroid;
    SimpleTimePlot *plotSpeed;
    int lastObjId;
    AspectRatioPixmap * imgFalls[TRACK_BIGGEST_N_BOXES];
    bool  exitAfterPlayback;
    bool processingStopped;
    tSettings settings;

};

#endif // MAINWINDOW_H
