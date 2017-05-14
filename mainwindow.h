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
    void insertPoint();

private:
    Ui::MainWindow *ui;
    SimpleTimePlot *p,*p2;
    QTimer* timer;
    CameraHandlerDavis camHandler;
    Processor proc;
    float prevX;
    float prevY;
    float prevX2;
    float prevY2;
};

#endif // MAINWINDOW_H
