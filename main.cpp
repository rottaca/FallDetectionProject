#include "mainwindow.h"

#include <QApplication>
#include <QThreadPool>

#include "camerahandler.h"
#include "processor.h"

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();

    return 0;
}
