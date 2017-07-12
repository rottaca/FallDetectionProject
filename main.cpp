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

    QStringList args = QCoreApplication::arguments();
    if(args.size() == 2) {
        w.showMinimized();
        w.playFile(args.at(1));
    } else {
        w.show();
    }

    return a.exec();
}
