#ifndef PROCESSOR_H
#define PROCESSOR_H
#include <QMutex>
#include <QRunnable>
#include <QString>
#include <QWaitCondition>
#include <QFuture>
#include <QElapsedTimer>
#include <QImage>

#include <atomic>
#include <queue>

#include <libcaer/events/polarity.h>
#include <libcaer/events/frame.h>

#include "camerahandlerdavis.h"
#include "eventbuffer.h"

class Processor:  public QObject,
    public CameraHandlerDavis::IDVSEventReciever,
    public CameraHandlerDavis::IFrameReciever
{
    Q_OBJECT
public:
    Processor();

    void start();
    void stop();

    void newEvent(const sDVSEventDepacked & event);
    void newFrame(const caerFrameEvent & frame);

    void run();

    EventBuffer &getBuffer()
    {
        return m_eventBuffer;
    }

    typedef struct sObjectStats {
        QPointF center;
        QPointF std;
        QRectF roi;
        int evCnt;
    } sObjectStats;

    sObjectStats getStats()
    {
        QMutexLocker locker(&m_statsMutex);
        return m_stats;
    }
signals:
    void updateUI(QString msg);

private:
    void updateStatistics();
    void updateObjectStats(sObjectStats &st);

private:
    std::atomic_bool m_isRunning;
    QFuture<void> m_future;

    QWaitCondition m_waitForEvents;
    EventBuffer m_eventBuffer;

    QMutex m_queueMutex;
    std::queue<sDVSEventDepacked> m_eventQueue;

    int m_timewindow;
    int m_updateStatsInterval;
    QElapsedTimer m_updateStatsTimer;

    QMutex m_statsMutex;
    sObjectStats m_stats;

};
#endif // PROCESSOR_H
