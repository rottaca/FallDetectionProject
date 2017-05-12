#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QCloseEvent>


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

private:
    Ui::MainWindow *ui;

    CameraHandlerDavis camHandler;
    Processor proc;
};

#endif // MAINWINDOW_H
