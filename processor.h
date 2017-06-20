#ifndef PROCESSOR_H
#define PROCESSOR_H
#include <QMutex>
#include <QRunnable>
#include <QString>
#include <QWaitCondition>
#include <QFuture>
#include <QElapsedTimer>
#include <QImage>

#include <opencv2/opencv.hpp>

#include <atomic>
#include <queue>

#include <libcaer/events/polarity.h>
#include <libcaer/events/frame.h>

#include "camerahandler.h"
#include "eventbuffer.h"

#include "settings.h"

class Processor:  public QObject,
    public CameraHandler::IDVSEventReciever,
    public CameraHandler::IFrameReciever
{
    Q_OBJECT
public:
    Processor();

    void start(uint16_t sx, uint16_t sy);
    void stop();

    void newEvent(const sDVSEventDepacked & event);
    void newFrame(const caerFrameEvent & frame);

    void run();

    EventBuffer &getBuffer()
    {
        return m_eventBuffer;
    }

    QImage getImg()
    {
        QMutexLocker locker(&m_frameMutex);
        return m_currFrame;
    }

    typedef struct sObjectStats {
        QPointF center;
        QPointF velocity;
        std::vector<QPointF> velocityHistory;
        QPointF velocityNorm;
        QPointF std;
        QRectF roi,prevRoi;
        QRectF stdDevBox;
        size_t evCnt;
        uint64_t lastROIUpdate, deltaTimeLastDataUpdateUs;
        uint32_t id;
        bool trackingLost;
        bool possibleFall;
        bool trackingPreviouslyLost;
        sObjectStats()
        {
            possibleFall = false;
            trackingLost = true;
            trackingPreviouslyLost = true;
            id = -1;
            evCnt = 0;
            lastROIUpdate = 0;
            deltaTimeLastDataUpdateUs = 0;
        }
    } sObjectStats;

    QVector<sObjectStats> getStats()
    {
        QMutexLocker locker(&m_statsMutex);
        return m_stats;
    }
    QImage getThresholdImg()
    {
        QMutexLocker locker(&m_statsMutex);
        return m_thresholdImg;
    }

    float getProcessingFPS()
    {
        QMutexLocker locker(&m_statsMutex);
        return m_currProcFPS;
    }
    float getFrameFPS()
    {
        QMutexLocker locker(&m_frameMutex);
        return m_currFrameFPS;
    }



signals:
    void updateUI(QString msg);

private:
    void processImage();
    void updateStatistics(uint32_t elapsedTimeUs);
    void updateObjectStats(sObjectStats &st, uint32_t elapsedTimeUs);
    inline bool isInROI(const sDVSEventDepacked& e, const QRectF &roi);

    std::vector<cv::Rect> detect();
    void tracking(std::vector<cv::Rect> &bboxes);

private:
    std::atomic_bool m_isRunning;
    QFuture<void> m_future;

    EventBuffer m_eventBuffer;
    uint16_t m_sx,m_sy;

    QMutex m_queueMutex;
    std::queue<sDVSEventDepacked> m_eventQueue;

    QMutex m_frameMutex;
    float m_currFrameFPS;
    QElapsedTimer m_frameTimer;
    QImage m_currFrame;
    bool m_newFrameAvailable;
    QFuture<void> m_futureAnyncPedestrianDetector;
    u_int32_t m_nextId;

    // Time in us
    int m_timewindow;
    int m_updateStatsInterval;
    QElapsedTimer m_updateStatsTimer;

    QMutex m_statsMutex;
    QVector<sObjectStats> m_stats;
    float m_currProcFPS;
    QImage m_thresholdImg;
};
#endif // PROCESSOR_H
