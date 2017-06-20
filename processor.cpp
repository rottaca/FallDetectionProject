#include "processor.h"

#include <QtConcurrent/QtConcurrent>
#include <QImageReader>
#include <qimage.h>

#include <assert.h>


Processor::Processor():
    m_timewindow(TIME_WINDOW_US),
    m_updateStatsInterval(UPDATE_INTERVAL_COMP_US)
{
    m_newFrameAvailable = false;
    m_nextId = 0;

    m_currProcFPS = 0;
    m_currFrameFPS = 0;
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
        //printf("%d %d %d\n",event.x,event.y,m_eventQueue.size());
    }
}

void Processor::newFrame(const caerFrameEvent &frame)
{

    assert(frame->lengthX == m_sx);
    assert(frame->lengthY == m_sy);
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
    printf("Processor started.\n");
#ifndef NDEBUG
    QString prevEv,prevFr,suff,numStr;
    suff = ".png";
    int i = 0;
    prevEv = "fallEv";
    prevFr = "fallFr";
    numStr = QString("%1").arg(i++);
    while(QFile(prevEv+numStr+suff).exists()) {
        QFile(prevEv+numStr+suff).remove();
        QFile(prevFr+numStr+suff).remove();

        numStr = QString("%1").arg(i++);
    }
#endif

    m_updateStatsTimer.restart();
    //QElapsedTimer comTimer;
    //QElapsedTimer t,t2;
    //t.start();
    while (m_isRunning) {

        // Check if anything has to be done
        // New events available ?
        // New frames avalibale ?
        // Only sleep if we don't have to process the data
        if(m_eventQueue.size() == 0 && !m_newFrameAvailable) {
            // Don't waist resources: Sleep until next update step
            QThread::usleep(qMax(0LL,m_updateStatsInterval-m_updateStatsTimer.nsecsElapsed()/1000));
        }

        // Process data
        //printf("Tasks: %lu %d %lld\n",m_eventQueue.size(), m_newFrameAvailable,m_updateStatsTimer.nsecsElapsed()/1000);

        // Process events and add them to the buffer
        // Remove old ones if necessary
        // t2.start();
        if(m_eventQueue.size()>0) {
            QMutexLocker locker(&m_queueMutex);
            m_eventBuffer.addEvents(m_eventQueue);
        }
        //printf("Event Update: %lld\n",t2.nsecsElapsed()/1000);
        // Process frame
        if(m_newFrameAvailable && m_futureAnyncPedestrianDetector.isFinished()) {
            {
                QMutexLocker locker(&m_frameMutex);
                m_currFrameFPS = (1.0f-FPS_LOWPASS_FILTER_COEFF)*m_currFrameFPS+
                                 FPS_LOWPASS_FILTER_COEFF*1000.0f/m_frameTimer.elapsed();
                m_frameTimer.restart();
                m_newFrameAvailable = false;
            }
            // Execute face detection in different thread
            //m_futureAnyncPedestrianDetector = QtConcurrent::run(this, &Processor::processImage);
        }
        //t2.restart();
        // Recompute buffer stats
        if(m_updateStatsTimer.nsecsElapsed()/1000 > m_updateStatsInterval) {
            //printf("Update: %u",elapsedTime);

            m_currProcFPS = (1.0f-FPS_LOWPASS_FILTER_COEFF)*m_currProcFPS +
                            FPS_LOWPASS_FILTER_COEFF*1000.0f/m_updateStatsTimer.elapsed();

            //printf("FPS: %f %f\n",m_currProcFPS, 1000000000.0f/m_updateStatsTimer.nsecsElapsed());
            //printf("Elapsed: %lld instead of %d\n",m_updateStatsTimer.nsecsElapsed()/1000, m_updateStatsInterval);
            uint64_t elapsedTime = m_updateStatsTimer.nsecsElapsed()/1000;
            m_updateStatsTimer.restart();
            updateStatistics(elapsedTime);
            //printf("Update done.\n");
        }
        //printf("Stats Update: %lld\n",t2.nsecsElapsed()/1000);

        //printf("Update: %lld\n",t.nsecsElapsed()/1000);
        //t.restart();
        //printf("%lld\n",comTimer.nsecsElapsed());
    }

    printf("Processor stopped.\n");
}

void Processor::processImage()
{
    QThread::currentThread()->setPriority(QThread::LowestPriority);
    std::vector<cv::Rect> roi_humans;
    {
        QMutexLocker locker(&m_frameMutex);
        m_newFrameAvailable = false;
        // Source: http://www.magicandlove.com/blog/2011/08/26/people-detection-in-opencv-again/
        cv::HOGDescriptor hog;
        hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());

        cv::Mat image(cv::Size(m_currFrame.width(), m_currFrame.height()),
                      CV_8UC1, m_currFrame.bits(), m_currFrame.bytesPerLine());

        std::vector<cv::Rect> found;
        hog.detectMultiScale(image, found,0,cv::Size(6,6),cv::Size(32,64),1.2);

        size_t i, j;
        for (i=0; i<found.size(); i++) {
            cv::Rect r = found[i];
            for (j=0; j<found.size(); j++)
                if (j!=i && (r & found[j])==r)
                    break;
            if (j==found.size()) {
                roi_humans.push_back(r);
            }
        }
    }
}

bool compare_rect(const cv::Rect & a, const cv::Rect &b)
{
    return a.area() > b.area();
}
std::vector<cv::Rect> Processor::detect()
{
    // Compute image of current event buffer
    cv::Mat bufferImg(cv::Size(m_sx,m_sy), CV_8UC1);
    bufferImg.setTo(cv::Scalar(0));
    uchar* p;
    auto & buff = m_eventBuffer.getLockedBuffer();
    for(sDVSEventDepacked & e:buff) {
        p = bufferImg.ptr<uchar>(e.y,e.x);
        *p = 255;
    }
    m_eventBuffer.releaseLockedBuffer();

#ifndef NDEBUG
    cv::imwrite( "0image.jpg", bufferImg );
#endif

    cv::Mat element = cv::getStructuringElement( cv::MORPH_OPEN, cv::Size( 3, 3 ));

    cv::morphologyEx( bufferImg, bufferImg, cv::MORPH_OPEN, element );

#ifndef NDEBUG
    cv::imwrite( "1closed.jpg", bufferImg );
#endif

    cv::GaussianBlur(bufferImg,bufferImg,
                     cv::Size(TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ,TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ),
                     TRACK_BOX_DETECTOR_GAUSS_SIGMA,TRACK_BOX_DETECTOR_GAUSS_SIGMA,cv::BORDER_REPLICATE);

#ifndef NDEBUG
    cv::imwrite( "2blurred.jpg", bufferImg );
#endif
    // Treshold image
    cv::threshold(bufferImg,bufferImg,TRACK_BOX_DETECTOR_THRESHOLD,255,CV_THRESH_BINARY);
#ifndef NDEBUG
    cv::imwrite( "3threshold.jpg", bufferImg );
#endif
    m_thresholdImg = QImage(bufferImg.cols,bufferImg.rows,QImage::Format_Grayscale8);
    memcpy((void*)m_thresholdImg.bits(),(void*)bufferImg.ptr(),bufferImg.cols*bufferImg.rows);

    // Find outline contours only
    std::vector<std::vector<cv::Point> > contours;
    cv::findContours(bufferImg,contours,CV_RETR_EXTERNAL,CV_CHAIN_APPROX_SIMPLE);

    // Convert contours to bounding boxes
    std::vector<cv::Rect> tmpBoxes,bboxes;
    for(int i = 0; i < contours.size(); i++) {
        cv::Rect r=cv::boundingRect(contours.at(i));
        tmpBoxes.push_back(r);
    }

    // Sort by area
    sort( tmpBoxes.begin(), tmpBoxes.end(), compare_rect );

    cv::Rect imgWithoutBorder(TRACK_IMG_BORDER_SIZE,TRACK_IMG_BORDER_SIZE,
                              bufferImg.cols-2*TRACK_IMG_BORDER_SIZE, bufferImg.rows-2*TRACK_IMG_BORDER_SIZE);

    for(int i = 0; i < qMin((int)tmpBoxes.size(), TRACK_BIGGEST_N_BOXES); i++) {
        cv::Rect r=tmpBoxes.at(i);
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

            QRectF newROI(r.x,
                          r.y,
                          r.width,
                          r.height);
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
    //printf("Detect: %lld\n",t.nsecsElapsed()/1000);
    //t.restart();

    QMutexLocker locker(&m_statsMutex);
    tracking(bboxes);
    //printf("Track: %lld\n",t.nsecsElapsed()/1000);

    //t.restart();
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

        st.velocityHistory.push_back(newVelocity);
        if(st.velocityHistory.size()>STATS_SPEED_SMOOTHING_WINDOW_SZ) {
            st.velocityHistory.erase(st.velocityHistory.begin());
        }

        st.velocity.setX(0);
        st.velocity.setY(0);
        float weightSum = 0;
        for(int i= st.velocityHistory.size()-1; i >= 0; i--) {
            const QPointF& p = st.velocityHistory.at(i);
            st.velocity += (i+1)*p;
            weightSum += (i+1);
        }
        st.velocity /= weightSum;

        st.velocityNorm = st.velocity/(2*newStd.y());

        /*if(std::abs(st.velocityNorm.y()) > 2.5) {
            printf("High speed: PrevCenter: %f, Center: %f, Speed: %f, Time: %lu, %d, %d\n",
                   st.center.y(),newCenter.y(),st.velocityNorm.y(),
                   st.deltaTimeLastDataUpdateUs,st.trackingLost,st.trackingPreviouslyLost);
        }*/

        if(st.possibleFall) {
            //printf("Falling: PrevCenter: %f, Center: %f, Speed: %f, Time: %lu\n",
            //       st.center.y(),newCenter.y(),st.velocityNorm.y(),st.deltaTimeLastDataUpdateUs);
            if(newCenter.y() < FALL_DETECTOR_Y_CENTER_THRESHOLD_UNFALL) {
                st.possibleFall = false;
            }
        } else if(newCenter.y() > FALL_DETECTOR_Y_CENTER_THRESHOLD_FALL &&
                  st.velocityNorm.y() >= FALL_DETECTOR_Y_SPEED_MIN_THRESHOLD &&
                  st.velocityNorm.y() <= FALL_DETECTOR_Y_SPEED_MAX_THRESHOLD) {
            st.possibleFall = true;
            printf("Fall detected: PrevCenter: %f, Center: %f, Speed (norm): %f, Time: %lu\n",
                   st.center.y(),newCenter.y(),st.velocityNorm.y(),st.deltaTimeLastDataUpdateUs);

#ifndef NDEBUG
            QImage img = m_eventBuffer.toImage();
            QRect roi = QRect(st.roi.x(),st.roi.y(),st.roi.width(),st.roi.height());
            QString prevEv,prevFr,suff,numStr;
            suff = ".png";
            int i = 0;
            prevEv = "fallEv";
            prevFr = "fallFr";

            do {
                numStr = QString("%1").arg(i++);
            } while(QFile(prevEv+numStr+suff).exists());
            img.copy(roi).save(prevEv+numStr+suff);
            {
                QMutexLocker locker(&m_frameMutex);
                m_currFrame.copy(roi).save(prevFr+numStr+suff);
            }
#endif
        }
    } else {
        st.velocity.setX(0);
        st.velocity.setY(0);
        st.velocityNorm.setX(0);
        st.velocityNorm.setY(0);
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
