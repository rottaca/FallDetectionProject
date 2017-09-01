#include "processor.h"

#include <QtConcurrent/QtConcurrent>
#include <QImageReader>
#include <qimage.h>

#include <assert.h>

#include <sstream>


Processor::Processor():
    m_timewindow(TIME_WINDOW_US),
    m_updateStatsInterval(UPDATE_INTERVAL_COMP_US)
{
    m_newFrameAvailable = false;
    m_nextId = 0;

    m_currProcFPS = 0;
    m_currFrameFPS = 0;

#if FALL_DETECTOR_POSTCLASSIFY_HUMANS
    if(!m_cascadeClassifier.load("cascade.xml")) {
        std::cerr << "Failded to load classifier" << std::endl;
        exit(1);
    }
#endif

}
void Processor::start(uint16_t sx, uint16_t sy)
{
    if(m_isRunning)
        stop();

    // Clear event queue
    {
        QMutexLocker locker(&m_queueMutex);
        while (!m_eventQueue.empty())
            m_eventQueue.pop();
    }

    m_sx = sx;
    m_sy = sy;
    m_currFrame = QImage(sx,sy,QImage::Format_Grayscale8);
    m_currFrame.fill(0);

    m_eventBuffer.setup(m_timewindow,sx,sy);
    m_stats.clear();

    m_currFrameFPS = 0;
    m_currProcFPS = 0;
    m_nextId = 0;
    m_newFrameAvailable = false;
    m_isRunning = true;

    m_future = QtConcurrent::run(this, &Processor::run);
}

void Processor::stop()
{
    m_isRunning = false;
    m_future.waitForFinished();
}

void Processor::newEvent(const sDVSEventDepacked & event)
{
    {
        QMutexLocker locker(&m_queueMutex);
        m_eventQueue.push(event);
    }
}

void Processor::newFrame(const caerFrameEvent &frame)
{
    if(frame->lengthX != m_sx ||
            frame->lengthY != m_sy) {
        std::cerr << "Invalid frame size" <<std::endl;
        return;
    }
    {
        QMutexLocker locker(&m_frameMutex);
        m_newFrameAvailable = true;
        // Convert to qt image
        uchar* ptr = m_currFrame.bits();
        u_int16_t* inPtr = frame->pixels;
        for(int i = 0; i < m_sx*m_sy; i++) {
            ptr[i] = inPtr[i]>>8;
        }
    }
}

void Processor::run()
{
    m_updateStatsTimer.restart();
    while (m_isRunning) {

        // Check if anything has to be done
        // New events available ?
        // New frames avalibale ?
        // Only sleep if we don't have to process the data
        if(m_eventQueue.size() == 0 && !m_newFrameAvailable) {
            // Don't waist resources: Sleep until next update step
            QThread::usleep(qMax(0LL,m_updateStatsInterval-m_updateStatsTimer.nsecsElapsed()/1000));
        }

        // Process events and add them to the buffer
        // Remove old ones if necessary
        if(m_eventQueue.size()>0) {
            QMutexLocker locker(&m_queueMutex);
            m_eventBuffer.addEvents(m_eventQueue);
        }
        // Recompute buffer stats
        if(m_updateStatsTimer.nsecsElapsed()/1000 > m_updateStatsInterval) {

            m_currProcFPS = (1.0f-FPS_LOWPASS_FILTER_COEFF)*m_currProcFPS +
                            FPS_LOWPASS_FILTER_COEFF*1000.0f/m_updateStatsTimer.elapsed();
            uint64_t elapsedTime = m_updateStatsTimer.nsecsElapsed()/1000;
            m_updateStatsTimer.restart();
            updateStatistics(elapsedTime);
        }
        if(m_newFrameAvailable) {
            {
                QMutexLocker locker(&m_frameMutex);
                m_currFrameFPS = (1.0f-FPS_LOWPASS_FILTER_COEFF)*m_currFrameFPS+
                                 FPS_LOWPASS_FILTER_COEFF*1000.0f/m_frameTimer.elapsed();
                m_frameTimer.restart();
                m_newFrameAvailable = false;
            }
        }
    }

    printf("Processor stopped.\n");
}

bool compare_rect(const cv::Rect & a, const cv::Rect &b)
{
    return a.area() > b.area();
}
std::vector<cv::Rect> Processor::detect()
{
    // Compute image of current event buffer
    m_bufferImg = cv::Mat(cv::Size(m_sx,m_sy), CV_8UC1);
    m_bufferImg.setTo(cv::Scalar(0));
    uchar* p;
    auto & buff = m_eventBuffer.getLockedBuffer();
    for(sDVSEventDepacked & e:buff) {
        p = m_bufferImg.ptr<uchar>(e.y,e.x);
        *p = 255;
    }
    m_eventBuffer.releaseLockedBuffer();
    // Perform opening if requrested
#if TRACK_OPENING_KERNEL_SZ > 1
    cv::Mat element = cv::getStructuringElement( cv::MORPH_OPEN, cv::Size( TRACK_OPENING_KERNEL_SZ, TRACK_OPENING_KERNEL_SZ ));
    cv::morphologyEx( m_bufferImg, m_bufferImg, cv::MORPH_OPEN, element );
#endif
    // Blur image
    cv::GaussianBlur(m_bufferImg,m_bufferImg,
                     cv::Size(TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ,TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ),
                     TRACK_BOX_DETECTOR_GAUSS_SIGMA,TRACK_BOX_DETECTOR_GAUSS_SIGMA,cv::BORDER_REPLICATE);

    if(m_smoothBufferImg.empty()) {
        m_smoothBufferImg = m_bufferImg;
        m_smoothBufferImg.setTo(0);
    }

    // Overwrite smoothbuffer and current buffer in a single pass
    uchar* bPtr = m_bufferImg.ptr();
    uchar* sPtr = m_smoothBufferImg.ptr();

    for(int i = 0; i < m_bufferImg.cols*m_bufferImg.rows; i++) {
        *bPtr = TRACK_BOX_TEMPORAL_SMOOTHING*(*bPtr)+(1-TRACK_BOX_TEMPORAL_SMOOTHING)*(*sPtr);
        *sPtr = *bPtr;
        bPtr++;
        sPtr++;
    }

    // Treshold image
    cv::threshold(m_bufferImg,m_bufferImg,TRACK_BOX_DETECTOR_THRESHOLD,255,CV_THRESH_BINARY);

    if(m_thresholdImg.isNull())
        m_thresholdImg = QImage(m_bufferImg.cols,m_bufferImg.rows,QImage::Format_Grayscale8);
    memcpy((void*)m_thresholdImg.bits(),(void*)m_bufferImg.ptr(),m_bufferImg.cols*m_bufferImg.rows);

    // Find outline contours only
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(m_bufferImg,contours,CV_RETR_EXTERNAL,CV_CHAIN_APPROX_SIMPLE);

    // Convert contours to bounding boxes
    std::vector<cv::Rect> tmpBoxes,tmpBoxes2,bboxes;
    for(int i = 0; i < contours.size(); i++) {
        cv::Rect r=cv::boundingRect(contours.at(i));
        tmpBoxes.push_back(r);
    }

    // Remove BBox inside others
    size_t i, j;
    for (i=0; i<tmpBoxes.size(); i++) {
        cv::Rect r = tmpBoxes[i];
        for (j=0; j<tmpBoxes.size(); j++)
            if (j!=i && (r & tmpBoxes[j])==r)
                break;
        if (j==tmpBoxes.size()) {
            tmpBoxes2.push_back(r);
        }
    }

    // Sort by area
    sort( tmpBoxes2.begin(), tmpBoxes2.end(), compare_rect );
    // Check if the found bounding box is entirely located around the image border
    cv::Rect imgWithoutBorder(TRACK_IMG_BORDER_SIZE_HORIZONTAL,TRACK_IMG_BORDER_SIZE_VERTICAL,
                              m_bufferImg.cols-2*TRACK_IMG_BORDER_SIZE_HORIZONTAL,
                              m_bufferImg.rows-2*TRACK_IMG_BORDER_SIZE_VERTICAL);

    for(int i = 0; i < qMin((int)tmpBoxes2.size(), TRACK_BIGGEST_N_BOXES); i++) {
        cv::Rect r=tmpBoxes2.at(i);
        // Expand bounding box
        r.x = qMax(0.0,r.x-r.width*(TRACK_BOX_SCALE-1.0)/2.0);
        r.y = qMax(0.0,r.y-r.height*(TRACK_BOX_SCALE-1.0)/2.0);
        r.width = qMin(m_sx - r.x - 1.0,r.width*TRACK_BOX_SCALE);
        r.height = qMin(m_sy - r.y - 1.0,r.height*TRACK_BOX_SCALE);
        if(r.area() >= TRACK_MIN_AREA && ((r & imgWithoutBorder).area() > 0)) {

            auto & buff = m_eventBuffer.getLockedBuffer();
            size_t cnt = 0;
            for(sDVSEventDepacked & e:buff) {
                if(r.contains(cv::Point(e.x,e.y)))
                    cnt++;
            }
            m_eventBuffer.releaseLockedBuffer();
            if(cnt >= TRACK_MIN_EVENT_CNT)
                bboxes.push_back(r);
        }
    }
    return bboxes;
}

void Processor::tracking(std::vector<cv::Rect> &bboxes)
{
    QVector<sObjectStats> oldStats = m_stats;
    m_stats.clear();

    uint64_t currTime = m_eventBuffer.getCurrTime();

    std::vector<int> trackedRects;
    for(int i = 0; i < oldStats.size(); i++) {
        sObjectStats o = oldStats.at(i);
        cv::Rect oROI(o.bbox.x(),o.bbox.y(),o.bbox.width(),o.bbox.height());
        float sz = oROI.area();

        float currScore = 0;
        int idx = -1;

        // Search for matching new rectangle
        for(int j=0; j < bboxes.size(); j++) {
            bool alreadyTracked = false;
            for(int k: trackedRects)
                if(k==j) {
                    alreadyTracked=true;
                    break;
                }
            if(alreadyTracked)
                continue;
            const cv::Rect& r = bboxes.at(j);

            float score = (oROI & r).area()/(sz);

            if(score > currScore) {
                currScore = score;
                idx = j;
            }
        }
        for(int i = FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD-1; i > 0; i--) {
            o.trackingLostHistory[i] = o.trackingLostHistory[i-1];
        }

        // Close enough ?
        if(currScore > TRACK_MIN_OVERLAP_RATIO) {
            const cv::Rect& r = bboxes.at(idx);
            o.trackingLostHistory[0] = false;
            o.bbox = QRectF(r.x, r.y, r.width, r.height);
            o.lastTrackingUpdate = currTime;
            m_stats.push_back(o);
            trackedRects.push_back(idx);
        }
        // ROI not found but still really new ?
        // possible fall ?
        else if(o.fallState && currTime-o.lastTrackingUpdate < TRACK_DELAY_KEEP_ROI_FALL_US) {
            o.trackingLostHistory[0] = true;
            m_stats.push_back(o);
        }
        // No fall
        else if(!o.fallState && currTime-o.lastTrackingUpdate < TRACK_DELAY_KEEP_ROI_US) {
            o.trackingLostHistory[0] = true;
            m_stats.push_back(o);
        }
    }

    // Add all missing rois
    for(int i = 0; i < bboxes.size(); i++) {
        bool alreadyFound = false;
        for(int k: trackedRects)
            if(k==i) {
                alreadyFound=true;
                break;
            }
        if(alreadyFound)
            continue;

        const cv::Rect& r = bboxes.at(i);
        sObjectStats stats;
        stats.id = m_nextId++;
        stats.bbox = QRectF(r.x,r.y,r.width,r.height);
        stats.lastTrackingUpdate = currTime;

        m_stats.push_back(stats);
    }
}
void Processor::updateStatistics(uint32_t elapsedTimeUs)
{
    // Update objects with new bounding box
    std::vector<cv::Rect> bboxes = detect();

    QMutexLocker locker(&m_statsMutex);
    tracking(bboxes);

    for(sObjectStats &stats:m_stats)
        updateObjectStats(stats, elapsedTimeUs);

}

void Processor::updateObjectStats(sObjectStats &st, uint32_t elapsedTimeUs)
{
    QPointF tmp, newCenter, newStd, newVelocity;
    size_t evCnt = 0, usedEvCnt = 0;
    newCenter.setX(0);
    newCenter.setY(0);
    newStd.setX(0);
    newStd.setY(0);

    cv::Mat maskImg (cv::Size(m_sx,m_sy), CV_8UC1);
    maskImg.setTo(cv::Scalar(0));
    maskImg(cv::Rect(st.bbox.x(),st.bbox.y(),st.bbox.width(),st.bbox.height())).setTo(cv::Scalar(255));

    uint32_t currTime = m_eventBuffer.getCurrTime();
    auto & buff = m_eventBuffer.getLockedBuffer();

#if FALL_DETECTOR_COMP_STATS_ALL_EVENTS
    QPointF sum,sumSquared;
    for(sDVSEventDepacked & e:buff) {
        if(maskImg.at<uchar>(e.y,e.x) == 0)
            continue;
        evCnt++;
        tmp.setX(e.x);
        tmp.setY(e.y);
        sum+=tmp;
        sumSquared+=QPointF(tmp.x()*tmp.x(),tmp.y()*tmp.y());
    }
    if(evCnt > 0) {
        newCenter = sum/evCnt;
        newStd = sumSquared / evCnt - QPointF(newCenter.x()*newCenter.x(),newCenter.y()*newCenter.y());
        newStd = QPointF(qSqrt(newStd.x()),qSqrt(newStd.y()));
    }
#else
    // Mark already used pixels in a new image
    cv::Mat markImg (cv::Size(m_sx,m_sy), CV_8UC1);
    markImg.setTo(cv::Scalar(0));
    QPointF sum,sumSquared;
    for(sDVSEventDepacked & e:buff) {
        if(maskImg.at<uchar>(e.y,e.x) == 0)
            continue;
        evCnt++;
        tmp.setX(e.x);
        tmp.setY(e.y);

        if(markImg.at<uchar>(e.y,e.x) == 0) {
            markImg.at<uchar>(e.y,e.x) = 255;
            sum+=tmp;
            sumSquared+=QPointF(tmp.x()*tmp.x(),tmp.y()*tmp.y());
            usedEvCnt++;
        }
    }
    if(usedEvCnt > 0) {
        newCenter = sum/usedEvCnt;
        newStd = sumSquared / usedEvCnt - QPointF(newCenter.x()*newCenter.x(),newCenter.y()*newCenter.y());
        newStd = QPointF(qSqrt(newStd.x()),qSqrt(newStd.y()));
    }
#endif
    m_eventBuffer.releaseLockedBuffer();

    if(st.initialized) {

        // Compute velocity if last point was set
        newVelocity.setX(1000000*(newCenter.x()-st.center.x())/elapsedTimeUs);
        newVelocity.setY(1000000*(newCenter.y()-st.center.y())/elapsedTimeUs);

        st.velocity = (1-STATS_SPEED_SMOOTHING_COEFF)*st.velocity + STATS_SPEED_SMOOTHING_COEFF*newVelocity;
        st.velocityNorm = st.velocity/(2*newStd.y());

        // Insert into history
        for(int i = FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD-1; i > 0; i--) {
            st.velocityNormYHistory[i] = st.velocityNormYHistory[i-1];
            st.centerYHistory[i] = st.centerYHistory[i-1];
            st.timeHistory[i] = st.timeHistory[i-1];
        }
        st.velocityNormYHistory[0] = st.velocityNorm.y();
        st.centerYHistory[0] = newCenter.y();
        st.timeHistory[0] = currTime;

        // Was previous speed a local maximum?
        bool isLocalSpeedMaximum = true;
        float localMaxNormVelocity = 0;
        for(int i = 0; i < FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD; i++) {
            if(i == FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2)
                continue;
            if(st.velocityNormYHistory[i] >= st.velocityNormYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2]) {
                isLocalSpeedMaximum = false;
                break;
            }
        }
        if(isLocalSpeedMaximum) {
            localMaxNormVelocity = st.velocityNormYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2];
        }

        cv::Rect cvRoi(st.bbox.x(),st.bbox.y(),st.bbox.width(),st.bbox.height());
        if(st.fallState != NO_FALL) {
            if(newCenter.y() < settings.fall_detector_y_center_threshold_unfall) {
                st.fallState = NO_FALL;
            } else if(!st.fallState && m_newFrameAvailable) {
                if(findFallingPersonInROI(cvRoi)) {
                    printf("%04u, [Fall]: Delayed detected, Time: %u\n",st.id, currTime);
                    st.fallState = FALL_CONFIRMED;
                }
            }
        } else if(isLocalSpeedMaximum &&
                  !st.trackingLostHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2] &&
                  st.centerYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2] > settings.fall_detector_y_center_threshold_fall &&
                  localMaxNormVelocity >= settings.fall_detector_y_speed_min_threshold &&
                  localMaxNormVelocity <= settings.fall_detector_y_speed_max_threshold) {
            st.fallTime = st.timeHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2];
            if(findFallingPersonInROI(cvRoi)) {
                printf("%04u, [Fall]: Directly detected, Time: %u, Speed (norm): %f, YCenter: %f\n",st.id, currTime, localMaxNormVelocity,st.centerYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2]);
                st.fallState = FALL_CONFIRMED;
            } else {
                st.fallState = FALL_POSSIBLE;
                printf("%04u, [Fall]: Possibly detected but no human found, Time: %u, Speed (norm): %f, YCenter: %f\n",st.id, currTime,localMaxNormVelocity,st.centerYHistory[FALL_DETECTOR_LOCAL_SPEED_MAX_NEIGHBORHOOD/2]);
            }
        }
    } else {
        st.initialized = true;
    }

    st.center = newCenter;
    st.std = newStd;
    st.evCnt = evCnt;
}

bool Processor::findFallingPersonInROI(cv::Rect bbox)
{
#if FALL_DETECTOR_POSTCLASSIFY_HUMANS
    std::vector<cv::Rect> detectedObjects;
    QMutexLocker locker(&m_frameMutex);
    cv::Mat image(cv::Size(m_currFrame.width(), m_currFrame.height()),
                  CV_8UC1, m_currFrame.bits(), m_currFrame.bytesPerLine());

    m_cascadeClassifier.detectMultiScale( image(bbox), detectedObjects, 1.05, 2, 0|cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30) );
    return detectedObjects.size() > 0;
#else
    return true;
#endif
}
