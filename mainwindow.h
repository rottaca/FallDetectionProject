#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QCloseEvent>
#include <QTimer>

#include "simpletimeplot.h"

#include "camerahandlerdavis.h"
#include "processor.h"

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

public slots:
    void updateUI(QString msg);
    void redrawUI();

private:
    Ui::MainWindow *ui;
    QTimer* timer;
    CameraHandlerDavis camHandler;
    Processor proc;

    SimpleTimePlot *plotEventsInWindow,*plotVerticalCentroid;
};

#endif // MAINWINDOW_H
