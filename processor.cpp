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

    if(!m_cascadeClassifier.load("cascade.xml")) {
        std::cerr << "Failded to load classifier" << std::endl;
        exit(1);
    }

}
void Processor::start(uint16_t sx, uint16_t sy)
{
    if(m_isRunning)
        stop();


    // Clear event queue
    {
        QMutexLocker locker(&m_queueMutex);
        while (!m_eventQueue.empty()) {
            m_eventQueue.pop();
        }
    }
    m_sx = sx;
    m_sy = sy;
    m_eventBuffer.setup(m_timewindow,sx,sy);
    m_currFrame = QImage(sx,sy,QImage::Format_Grayscale8);
    m_currFrame.fill(0);
    m_eventBuffer.clear();
    m_stats.clear();
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

    if(TRACK_OPENING_KERNEL_SZ > 1) {
        cv::Mat element = cv::getStructuringElement( cv::MORPH_OPEN, cv::Size( TRACK_OPENING_KERNEL_SZ, TRACK_OPENING_KERNEL_SZ ));

        cv::morphologyEx( m_bufferImg, m_bufferImg, cv::MORPH_OPEN, element );

    }

    cv::GaussianBlur(m_bufferImg,m_bufferImg,
                     cv::Size(TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ,TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ),
                     TRACK_BOX_DETECTOR_GAUSS_SIGMA,TRACK_BOX_DETECTOR_GAUSS_SIGMA,cv::BORDER_REPLICATE);
    if(m_smoothBufferImg.empty()) {
        m_smoothBufferImg = m_bufferImg;
        m_smoothBufferImg.setTo(0);
    }

    uchar* bPtr = m_bufferImg.ptr();
    uchar* sPtr = m_smoothBufferImg.ptr();

    for(int i = 0; i < m_bufferImg.cols*m_bufferImg.rows; i++) {
        *bPtr = 0.6*(*bPtr)+0.4*(*sPtr);
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
    cv::Rect imgWithoutBorder(TRACK_IMG_BORDER_SIZE,TRACK_IMG_BORDER_SIZE,
                              m_bufferImg.cols-2*TRACK_IMG_BORDER_SIZE,
                              m_bufferImg.rows-2*TRACK_IMG_BORDER_SIZE);

    for(int i = 0; i < qMin((int)tmpBoxes2.size(), TRACK_BIGGEST_N_BOXES); i++) {
        cv::Rect r=tmpBoxes2.at(i);
        // Expand bounding box
        r.x = qMax(0.0,r.x+r.width*(1-TRACK_BOX_SCALE)/2);
        r.y = qMax(0.0,r.y+r.height*(1-TRACK_BOX_SCALE)/2);
        r.width = qMin(m_sx - r.x - 1.0,r.width*TRACK_BOX_SCALE);
        r.height = qMin(m_sy - r.y - 1.0,r.height*TRACK_BOX_SCALE);
        if(r.area() >= TRACK_MIN_AREA && ((r & imgWithoutBorder).area() > 0)) {
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

        float oXR = o.roi.x()+o.roi.width();
        float oXL = o.roi.x();
        float oYT = o.roi.y();
        float oYB = o.roi.y()+o.roi.height();
        float sz = o.roi.width()*o.roi.height();
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

            float rXR = r.x+r.width;
            float rXL = r.x;
            float rYT = r.y;
            float rYB = r.y+r.height;

            float dx = qMin(rXR,oXR)-qMax(rXL,oXL);
            float dy = qMin(rYB,oYB)-qMax(rYT,oYT);

            float score = qMax(0.f,dx)*
                          qMax(0.f,dy)/(sz);

            if(score > currScore) {
                currScore = score;
                idx = j;
            }
        }
        o.trackingPreviouslyLost = o.trackingLost;

        // Close enough ?
        if(currScore > TRACK_MIN_OVERLAP_RATIO) {
            const cv::Rect& r = bboxes.at(idx);

            o.trackingLost = false;

            QRectF newROI((r.x + o.roi.x())/2,
                          (r.y + o.roi.y())/2,
                          (r.width + o.roi.width())/2,
                          (r.height + o.roi.height())/2);
            if(o.trackingLost) {
                o.prevRoi = newROI;
            } else {
                o.prevRoi = o.prevRoi;
            }

            o.roi = newROI;
            o.lastROIUpdate = currTime;
            m_stats.push_back(o);
            trackedRects.push_back(idx);
        }
        // ROI not found but still really new ?
        // possible fall ?
        else if(o.possibleFall && currTime-o.lastROIUpdate < TRACK_DELAY_KEEP_ROI_FALL_US) {
            o.trackingLost = true;
            o.prevRoi = o.roi;
            m_stats.push_back(o);
        }
        // No fall
        else if(!o.possibleFall && currTime-o.lastROIUpdate < TRACK_DELAY_KEEP_ROI_US) {
            o.trackingLost = true;
            o.prevRoi = o.roi;
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
        stats.roi = QRectF(r.x,r.y,r.width,r.height);
        stats.prevRoi = stats.roi;
        stats.lastROIUpdate = currTime;

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

inline bool Processor::isInROI(const sDVSEventDepacked& e, const QRectF &roi)
{
    return e.x >= roi.x() && e.x <= roi.x()+roi.width() &&
           e.y >= roi.y() && e.y <= roi.y()+roi.height();
}

void Processor::updateObjectStats(sObjectStats &st, uint32_t elapsedTimeUs)
{
    QPointF tmp, newCenter, newStd, newVelocity;
    size_t evCnt = 0;
    newCenter.setX(0);
    newCenter.setY(0);
    newStd.setX(0);
    newStd.setY(0);

    // accumulate time
    st.deltaTimeLastDataUpdateUs += elapsedTimeUs;
    uint32_t currTime = m_eventBuffer.getCurrTime();
    auto & buff = m_eventBuffer.getLockedBuffer();

    for(sDVSEventDepacked & e:buff) {
        if(!isInROI(e,st.roi))
            continue;
        evCnt++;
        tmp.setX(e.x);
        tmp.setY(e.y);
        newCenter += tmp;
    }
    newCenter /= evCnt;

    for(sDVSEventDepacked & e:buff) {
        if(!isInROI(e,st.roi))
            continue;
        tmp.setX(e.x);
        tmp.setY(e.y);
        tmp -= newCenter;
        tmp.setX(tmp.x()*tmp.x());
        tmp.setY(tmp.y()*tmp.y());
        tmp.setX(sqrtf(tmp.x()));
        tmp.setY(sqrtf(tmp.y()));
        newStd += tmp;
    }
    newStd /= evCnt;
    m_eventBuffer.releaseLockedBuffer();

    if(!st.trackingLost) {

        // Compute velocity if last point was set
        newVelocity.setX(1000000*(newCenter.x()-st.center.x())/st.deltaTimeLastDataUpdateUs);
        newVelocity.setY(1000000*(newCenter.y()-st.center.y())/st.deltaTimeLastDataUpdateUs);

        // Smooth velocity with linear smoothing of previous values
        st.velocityHistory.push_back(newVelocity);
        if(st.velocityHistory.size()>STATS_SPEED_SMOOTHING_WINDOW_SZ) {
            st.velocityHistory.erase(st.velocityHistory.begin());
        }
        QPointF smoothedVelocity(0,0) ;
        float weightSum = 0;
        for(int i= st.velocityHistory.size()-1; i >= 0; i--) {
            const QPointF& p = st.velocityHistory.at(i);
            smoothedVelocity += (i+1)*p;
            weightSum += (i+1);
        }
        smoothedVelocity /= weightSum;
        st.velocity = smoothedVelocity;

        st.velocityNorm = st.velocity/(2*newStd.y());
        bool isLocalSpeedMaximum = false;
        // Was previous speed a local maximum
        if(st.velocityNormYLastTwo[1] < st.velocityNormYLastTwo[0] &&
                st.velocityNormYLastTwo[0] > st.velocityNorm.y()) {
            isLocalSpeedMaximum = true;
        }

        if(st.possibleFall) {
            if(newCenter.y() < FALL_DETECTOR_Y_CENTER_THRESHOLD_UNFALL) {
                st.possibleFall = false;
                st.confirmendFall = false;
            } else if(!st.confirmendFall && m_newFrameAvailable) {
                cv::Rect cvRoi(st.roi.x(),st.roi.y(),st.roi.width(),st.roi.height());
                if(findFallingPersonInROI(cvRoi)) {
                    printf("%04u, [Fall]: Delayed detected, Speed (norm): %f, Time: %u\n",st.id, st.velocityNorm.y(),currTime);
                    st.confirmendFall = true;
                    st.fallTime = currTime;
                }
            }
        } else if(newCenter.y() > FALL_DETECTOR_Y_CENTER_THRESHOLD_FALL && isLocalSpeedMaximum &&
                  st.velocityNormYLastTwo[0] >= settings.fall_detector_y_speed_min_threshold &&
                  st.velocityNormYLastTwo[0] <= settings.fall_detector_y_speed_max_threshold) {
            st.possibleFall = true;
            st.fallTime = currTime;

            cv::Rect cvRoi(st.roi.x(),st.roi.y(),st.roi.width(),st.roi.height());
            if(findFallingPersonInROI(cvRoi)) {
                printf("%04u, [Fall]: Directly detected, Speed (norm): %f, Time: %u\n",st.id, st.velocityNorm.y(),currTime);
                st.confirmendFall = true;

            } else {
                printf("%04u, [Fall]: Possibly detected but no human found, Speed (norm): %f, Time: %u\n",st.id, st.velocityNorm.y(),currTime);
            }
        }
        st.velocityNormYLastTwo[1] = st.velocityNormYLastTwo[0];
        st.velocityNormYLastTwo[0] = st.velocityNorm.y();

    } else {
        st.velocity.setX(0);
        st.velocity.setY(0);
        st.velocityNorm.setX(0);
        st.velocityNorm.setY(0);
        st.velocityNormYLastTwo[1] = 0;
        st.velocityNormYLastTwo[0] = 0;
    }

    st.center = newCenter;
    st.std = newStd;
    st.evCnt = evCnt;
    st.deltaTimeLastDataUpdateUs = 0;

    st.stdDevBox.setX(st.center.x()-st.std.x());
    st.stdDevBox.setY(st.center.y()-st.std.y());
    st.stdDevBox.setWidth(2*st.std.x()+1);
    st.stdDevBox.setHeight(2*st.std.y()+1);

}

bool Processor::findFallingPersonInROI(cv::Rect roi)
{
    std::vector<cv::Rect> detectedObjects;
    QMutexLocker locker(&m_frameMutex);
    cv::Mat image(cv::Size(m_currFrame.width(), m_currFrame.height()),
                  CV_8UC1, m_currFrame.bits(), m_currFrame.bytesPerLine());

    m_cascadeClassifier.detectMultiScale( image(roi), detectedObjects, 1.1, 2, 0|cv::CASCADE_SCALE_IMAGE, cv::Size(30, 30) );
    return detectedObjects.size() > 0;
}
