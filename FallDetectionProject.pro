#-------------------------------------------------
#
# Project created by QtCreator 2017-05-12T08:49:32
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = FallDetectionProject
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

LIBS += -fopenmp
LIBS += `pkg-config --libs opencv`
LIBS += -lcaer

QMAKE_CXXFLAGS += -fopenmp

SOURCES += main.cpp\
        mainwindow.cpp \
    eventbuffer.cpp \
    camerahandlerdavis.cpp \
    processor.cpp \
    simpletimeplot.cpp \
    aspectratiopixmap.cpp

HEADERS  += mainwindow.h \
    eventbuffer.h \
    camerahandlerdavis.h \
    processor.h \
    datatypes.h \
    simpletimeplot.h \
    settings.h \
    aspectratiopixmap.h

FORMS    += mainwindow.ui
