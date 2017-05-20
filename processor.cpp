#include "processor.h"

#include <QtConcurrent/QtConcurrent>

#include <assert.h>

#include <opencv2/opencv.hpp>

Processor::Processor():
    m_timewindow(TIME_WINDOW_US),
    m_updateStatsInterval(UPDATE_INTERVAL_COMP_US)
{
    m_eventBuffer.setup(m_timewindow);
    m_currFrame = QImage(DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT,QImage::Format_RGB888);
    m_newFrameAvailable = false;

    sObjectStats stats;
    stats.roi = QRectF(0,0,DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT);
    m_stats.push_back(stats);

    m_currProcFPS = 0;
    m_currFrameFPS = 0;
}
void Processor::start()
{
    if(m_isRunning)
        stop();
    m_isRunning = true;
    m_future = QtConcurrent::run(this, &Processor::run);
}

void Processor::stop()
{
    m_isRunning = false;
    m_waitForData.wakeAll();
    m_future.waitForFinished();
}

void Processor::newEvent(const sDVSEventDepacked & event)
{
    {
        QMutexLocker locker(&m_queueMutex);
        m_eventQueue.push(event);
    }
    m_waitForData.wakeAll();
}

void Processor::newFrame(const caerFrameEvent &frame)
{
    assert(frame->lengthX == DAVIS_IMG_WIDHT);
    assert(frame->lengthY == DAVIS_IMG_HEIGHT);

    if(m_frameTimer.isValid()) {
        m_currFrameFPS = (1.0f-FPS_LOWPASS_FILTER_COEFF)*m_currFrameFPS+
                         FPS_LOWPASS_FILTER_COEFF*(1000.0f/m_frameTimer.elapsed());
    }
    m_frameTimer.restart();

    {
        QMutexLocker locker(&m_frameMutex);
        m_newFrameAvailable = true;
        // Convert to qt image
        uchar* ptr = m_currFrame.bits();
        u_int16_t* inPtr = frame->pixels;
        for(int i = 0; i < DAVIS_IMG_WIDHT*DAVIS_IMG_HEIGHT; i++) {
            ptr[3*i] = inPtr[i]>>8;
            ptr[3*i + 1] = inPtr[i]>>8;
            ptr[3*i + 2] = inPtr[i]>>8;
        }
    }
    m_waitForData.wakeAll();
}

void Processor::run()
{
    printf("Processor started.\n");
    m_updateStatsTimer.start();
    QElapsedTimer comTimer;
    while (m_isRunning) {
        comTimer.restart();
        // New events available ?
        if(m_eventQueue.size() == 0 && m_newFrameAvailable) {
            QMutexLocker locker(&m_waitMutex);
            m_waitForData.wait(&m_waitMutex);    // Avoid busy waiting
            continue;
        }
        // Process data
        else {
            // Process events
            if(m_eventQueue.size()>0) {
                QMutexLocker locker(&m_queueMutex);
                //printf("%lu\n",m_eventQueue.size());
                m_eventBuffer.addEvents(m_eventQueue);
                while(!m_eventQueue.empty()) {
                    const sDVSEventDepacked &ev = m_eventQueue.front();
                    m_eventBuffer.addEvent(ev);
                    m_eventQueue.pop();
                }
            }
            // Process frame
            if(m_newFrameAvailable) {
                processImage();
            }

            // Recompute buffer stats
            if(m_updateStatsTimer.nsecsElapsed()/1000 > m_updateStatsInterval) {
                uint32_t elapsedTime = m_updateStatsTimer.nsecsElapsed()/1000;
                //printf("Update: %u",elapsedTime);
                m_updateStatsTimer.restart();
                updateStatistics(elapsedTime);
            }
        }
        //printf("CompTimer: %lld\n",comTimer.elapsed());
    }
    printf("Processor stopped.\n");
}

void Processor::processImage()
{
    {
        QMutexLocker locker(&m_frameMutex);
        m_newFrameAvailable = false;
        // Source: http://www.magicandlove.com/blog/2011/08/26/people-detection-in-opencv-again/
        cv::HOGDescriptor hog;
        hog.setSVMDetector(cv::HOGDescriptor::getDefaultPeopleDetector());

        cv::Mat image(cv::Size(DAVIS_IMG_WIDHT, DAVIS_IMG_HEIGHT), CV_8UC3, m_currFrame.bits(), cv::Mat::AUTO_STEP);

        std::vector<cv::Rect> found, found_filtered;
        hog.detectMultiScale(image, found, 0, cv::Size(8,8), cv::Size(32,32), 1.05, 2);

        size_t i, j;
        for (i=0; i<found.size(); i++) {
            cv::Rect r = found[i];
            for (j=0; j<found.size(); j++)
                if (j!=i && (r & found[j])==r)
                    break;
            if (j==found.size())
                found_filtered.push_back(r);
        }
    }

    {
        // TODO Map rects to statistics roi
        QMutexLocker locker(&m_statsMutex);
    }
}

void Processor::updateStatistics(uint32_t elapsedTimeUs)
{
    QMutexLocker locker(&m_statsMutex);
    for(sObjectStats &stats:m_stats)
        updateObjectStats(stats, elapsedTimeUs);

    m_currProcFPS = (1-FPS_LOWPASS_FILTER_COEFF)*m_currProcFPS +
                    FPS_LOWPASS_FILTER_COEFF*1000000.0f/elapsedTimeUs;
}

inline bool Processor::isInROI(const sDVSEventDepacked& e, const QRectF &roi)
{
    return e.x >= roi.x() && e.x <= roi.x()+roi.width() &&
           e.y >= roi.y() && e.y <= roi.y()+roi.height();
}

void Processor::updateObjectStats(sObjectStats &st,uint32_t elapsedTimeUs)
{
    auto & buff = m_eventBuffer.getLockedBuffer();
    // Extract event subset
    /*std::vector<size_t> subsetIndices;
    for(size_t i = 0; i < buff.size(); i++) {
        sDVSEventDepacked & e = buff.at(i);
        if(isInROI(e,st.roi)) {
            subsetIndices.push_back(i);
        }
    }*/
    //printf("SubsetSize: %zu\n",subsetIndices.size());

    QPointF tmp, newCenter, newStd, newVelocity;
    size_t evCnt = 0;
    newCenter.setX(0);
    newCenter.setY(0);
    for(sDVSEventDepacked & e:buff) { //size_t &i:subsetIndices) {
        //sDVSEventDepacked & e = buff.at(i);
        if(!isInROI(e,st.roi))
            continue;
        tmp.setX(e.x);
        tmp.setY(e.y);
        newCenter += tmp;
        evCnt++;
    }
    newCenter /= evCnt;
    //printf("Center: %fx%f\n",m_center.x(),m_center.y());

    newStd.setX(0);
    newStd.setY(0);
    for(sDVSEventDepacked & e:buff) { //size_t &i:subsetIndices) {
        //sDVSEventDepacked & e = buff.at(i);
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
    //printf("Std: %fx%f\n",m_std.x(),m_std.y());

    // Compute velocity
    newVelocity.setX(1000000*(st.center.x()-newCenter.x())/elapsedTimeUs);
    newVelocity.setY(1000000*(st.center.y()-newCenter.y())/elapsedTimeUs);
    newVelocity = 0.8*st.velocity + 0.2*newVelocity;

    m_eventBuffer.releaseLockedBuffer();

    st.center = newCenter;
    st.std = newStd;
    st.evCnt = evCnt;
    st.bbox.setX(newCenter.x()-newStd.x());
    st.bbox.setY(newCenter.y()-newStd.y());
    st.bbox.setWidth(2*newStd.x()-1);
    st.bbox.setHeight(2*newStd.y()-1);
    st.velocity = newVelocity;
    st.velocityNorm = newVelocity/(2*newStd.y());
}
