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

    QCommandLineOption fallYCenterThresholdOpt("fallY","Lower bound for y coordinate to be a valid fall (Y axis points down!).", "fallY");
    parser.addOption(fallYCenterThresholdOpt);
    QCommandLineOption unfallYCenterThresholdOpt("unfallY","Lower bound for y coordinate to undo a fall (Y axis points down!)", "unfallY");
    parser.addOption(unfallYCenterThresholdOpt);

    parser.process(a);

    const QStringList args = parser.positionalArguments();
    QString minYSpeed = parser.value(minYSpeedThresholdOpt);
    QString maxYSpeed = parser.value(maxYSpeedThresholdOpt);
    QString fallYCenter = parser.value(fallYCenterThresholdOpt);
    QString unfallYCenter = parser.value(unfallYCenterThresholdOpt);
    bool minimized = parser.isSet(minimizeOption);
    bool maximized = parser.isSet(maximizeOption);

    tSettings settings;
    if(!minYSpeed.isEmpty()) {
        settings.fall_detector_y_speed_min_threshold = minYSpeed.toDouble();
    }
    if(!maxYSpeed.isEmpty()) {
        settings.fall_detector_y_speed_max_threshold = maxYSpeed.toDouble();
    }
    if(!fallYCenter.isEmpty()) {
        settings.fall_detector_y_center_threshold_fall = fallYCenter.toDouble();
    }
    if(!unfallYCenter.isEmpty()) {
        settings.fall_detector_y_center_threshold_unfall = unfallYCenter.toDouble();
    }
    qDebug("y_speed_max_threshold: %f", settings.fall_detector_y_speed_max_threshold);
    qDebug("y_speed_min_threshold: %f", settings.fall_detector_y_speed_min_threshold);
    qDebug("y_center_threshold_fall: %f", settings.fall_detector_y_center_threshold_fall);
    qDebug("y_center_threshold_unfall: %f", settings.fall_detector_y_center_threshold_unfall);

    MainWindow w(settings,nullptr);

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
