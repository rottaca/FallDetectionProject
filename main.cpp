#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QThreadPool>

#include "camerahandler.h"
#include "processor.h"

int main(int argc, char *argv[])
{
    setbuf(stdout, NULL);
    QApplication a(argc, argv);
    MainWindow w;


    QCommandLineParser parser;
    parser.addPositionalArgument("playback file",".aedat file to be played.");

    QCommandLineOption minimizeOption("min","Minimize application on start.");
    parser.addOption(minimizeOption);
    QCommandLineOption maximizeOption("max","Maximize application on start.");
    parser.addOption(maximizeOption);

    QCommandLineOption minYSpeedThresholdOpt("minSpeed","Minimal fall speed threshold for fall detection.", "minSpeed");
    parser.addOption(minYSpeedThresholdOpt);
    QCommandLineOption maxYSpeedThresholdOpt("maxSpeed","Maximal fall speed threshold for fall detection.", "maxSpeed");
    parser.addOption(maxYSpeedThresholdOpt);

    parser.process(a);

    const QStringList args = parser.positionalArguments();
    QString minYSpeed = parser.value(minYSpeedThresholdOpt);
    QString maxYSpeed = parser.value(maxYSpeedThresholdOpt);
    bool minimized = parser.isSet(minimizeOption);
    bool maximized = parser.isSet(maximizeOption);

    tSettings settings;
    if(!minYSpeed.isEmpty()) {
        settings.fall_detector_y_speed_min_threshold = minYSpeed.toDouble();
        qDebug("MinSpeed: %f", settings.fall_detector_y_speed_min_threshold);
    }
    if(!maxYSpeed.isEmpty()) {
        settings.fall_detector_y_speed_max_threshold = maxYSpeed.toDouble();
        qDebug("MaxSpeed: %f", settings.fall_detector_y_speed_max_threshold);
    }

    if(minimized)
        w.showMinimized();
    else if(maximized)
        w.showMaximized();
    else
        w.show();

    if(args.size() >= 1) {
        w.playFile(args.at(0));
    }

    w.setSettings(settings);

    return a.exec();
}
