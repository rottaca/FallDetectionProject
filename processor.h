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

/**
 * @brief The Processor class handles incoming event and framedata and detects falls.
 */
class Processor:  public QObject,
    public CameraHandler::IDVSEventReciever,
    public CameraHandler::IFrameReciever
{
    Q_OBJECT
public:
    Processor();
    /**
     * @brief setSettings Set the setting struct with adjustable parameters.
     * @param settings
     */
    void setSettings(tSettings &settings)
    {
        this->settings = settings;
    }
    /**
     * @brief start Starts the processing thread and sets the expected frame dimensions
     * @param sx
     * @param sy
     */
    void start(uint16_t sx, uint16_t sy);
    /**
     * @brief stop Stops the processing thread.
     */
    void stop();
    /**
     * @brief newEvent Implements callback function of the camera handler to receive events.
     * @param event
     */
    void newEvent(const sDVSEventDepacked & event);
    /**
     * @brief newFrame Implements callback function of the camera handler to receive frames.
     * @param frame
     */
    void newFrame(const caerFrameEvent & frame);
    /**
     * @brief run Memberfunctions that executes the detection, tracking and evaluation stages.
     * This is called by the launched thread.
     */
    void run();
    /**
     * @brief getBuffer Returns a reference to the event buffer object.
     * @return
     */
    EventBuffer &getBuffer()
    {
        return m_eventBuffer;
    }
    /**
     * @brief getImg Returns the current grayscale frame.
     * @return
     */
    QImage getImg()
    {
        QMutexLocker locker(&m_frameMutex);
        return m_currFrame;
    }

    /**
     * Possible object states.
     **/
    typedef enum FallState {
        NO_FALL = 0x00,
        FALL_POSSIBLE = 0x01,
        FALL_CONFIRMED = 0x02
    } FallState;

    /**
     * Struct contains all information of tracked / falling objects.
     **/
    typedef struct sObjectStats {
        // Current centroid position
        QPointF center;
        // Current normalized velocity
        QPointF velocityNorm;
        // Current unnormalized velocity (With exponential smoothing)
        QPointF velocity;
        // History of normalized vertical velocities,
        // were v[0] is the current normalized velocity
        float velocityNormYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        // History of vertical centroid positions
        float centerYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        // History of lost tracking
        bool trackingLostHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        // History of processing timestamps
        uint64_t timeHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD];
        // Current standard deviation
        QPointF std;
        // Current  bounding box for tracking
        QRectF bbox;
        // Number of events in bbox
        size_t evCnt;
        // Time of last tracking update
        uint64_t lastTrackingUpdate;
        // Inital fall time
        uint64_t fallTime;
        // Object ID
        uint32_t id;
        // Current fall state
        FallState fallState;
        // Initialization state: Don't compute speed in first run
        bool initialized;

        sObjectStats()
        {
            initialized = false;
            fallState = NO_FALL;
            id = -1;
            evCnt = 0;
            lastTrackingUpdate = 0;
            fallTime = 0;

            for(int i = 0; i < FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD; i++) {
                velocityNormYHistory[i] = 0;
                centerYHistory[i] = 0;
                trackingLostHistory[i] = false;
                timeHistory[i] = 0;
            }
        }

    } sObjectStats;
    /**
     * @brief getStats Returns a vector of all tracked / detected objects and their states.
     * @return
     */
    QVector<sObjectStats> getStats()
    {
        QMutexLocker locker(&m_statsMutex);
        return m_stats;
    }
    /**
     * @brief getThresholdImg Returns the current threshold image, used for detection.
     * @return
     */
    QImage getThresholdImg()
    {
        QMutexLocker locker(&m_statsMutex);
        return m_thresholdImg;
    }
    /**
     * @brief getProcessingFPS Returns the current processing operations per second.
     * @return
     */
    float getProcessingFPS()
    {
        QMutexLocker locker(&m_statsMutex);
        return m_currProcFPS;
    }
    /**
     * @brief getFrameFPS Returns the current number of grayscale frames per second.
     * @return
     */
    float getFrameFPS()
    {
        QMutexLocker locker(&m_frameMutex);
        return m_currFrameFPS;
    }

private:
    /**
     * @brief updateStatistics Detects, tracks and evaluates the current frame
     * @param elapsedTimeUs
     */
    void updateStatistics(uint32_t elapsedTimeUs);
    /**
     * @brief updateObjectStats Updates the statistics of a single object
     * with possibly updated bbox.
     * @param st
     * @param elapsedTimeUs
     */
    void updateObjectStats(sObjectStats &st, uint32_t elapsedTimeUs);
    /**
     * @brief detect Detects objects in the event buffer and returns a list of Bboxes.
     * @return
     */
    std::vector<cv::Rect> detect();
    /**
     * @brief tracking Tries to map the detected bboxes to the current objects
     * and inserts new objects if necessary.
     * @param bboxes
     */
    void tracking(std::vector<cv::Rect> &bboxes);
    /**
     * @brief findFallingPersonInROI Looks for a falling
     * person in the current grayscale image.
     * @param bbox
     * @return
     */
    bool findFallingPersonInROI(cv::Rect bbox);

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

#if FALL_DETECTOR_POSTCLASSIFY_HUMANS
    cv::CascadeClassifier m_cascadeClassifier;
#endif

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
