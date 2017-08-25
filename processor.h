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

    void setSettings(tSettings &settings)
    {
        this->settings = settings;
    }
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
        QPointF velocityNorm;
        float velocityNormYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        float centerYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        bool trackingLostHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        uint64_t timeHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        QPointF std;
        QRectF roi,prevRoi;
        QRectF stdDevBox;
        size_t evCnt;
        uint64_t lastTrackingUpdate, deltaTimeLastDataUpdateUs, fallTime;
        uint32_t id;
        bool possibleFall, confirmendFall;
        bool initialized;
        cv::Mat roiHist;

        sObjectStats()
        {
            initialized = false;
            confirmendFall = false;
            possibleFall = false;
            id = -1;
            evCnt = 0;
            lastTrackingUpdate = 0;
            deltaTimeLastDataUpdateUs = 0;
            for(int i = 0; i < FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD; i++) {
                velocityNormYHistory[i] = 0;
                centerYHistory[i] = 0;
                trackingLostHistory[i] = false;
                timeHistory[i] = 0;
            }
            fallTime = 0;
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

    bool findFallingPersonInROI(cv::Rect roi);

private:
    tSettings settings;
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
    u_int32_t m_nextId;
    cv::CascadeClassifier m_cascadeClassifier;

    // Time in us
    const int m_timewindow;
    const int m_updateStatsInterval;
    QElapsedTimer m_updateStatsTimer;

    QMutex m_statsMutex;
    QVector<sObjectStats> m_stats;
    float m_currProcFPS;
    QImage m_thresholdImg;
    cv::Mat m_bufferImg, m_smoothBufferImg;
};
#endif // PROCESSOR_H
