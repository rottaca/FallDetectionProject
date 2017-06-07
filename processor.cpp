#include "processor.h"

#include <QtConcurrent/QtConcurrent>
#include <QImageReader>
#include <qimage.h>

#include <assert.h>


Processor::Processor():
    m_timewindow(TIME_WINDOW_US),
    m_updateStatsInterval(UPDATE_INTERVAL_COMP_US)
{
    m_eventBuffer.setup(m_timewindow);
    m_currFrame = QImage(DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT,QImage::Format_Grayscale8);
    m_newFrameAvailable = false;
    m_nextId = 0;

#ifdef ASSUME_SINGLE_PERSON
    sObjectStats stats;
    stats.roi = QRectF(0,0,DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT);
    stats.id = m_nextId++;
    m_stats.push_back(stats);
#endif

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
        // TODO: Detect time jump
    }
    m_waitForData.wakeAll();
}

void Processor::newFrame(const caerFrameEvent &frame)
{

#ifdef SIMULATE_CAMERA_INPUT
    {
        QMutexLocker locker(&m_frameMutex);
        m_newFrameAvailable = true;
        QImageReader imReader("testScaled.jpeg");
        QImage img = imReader.read();
        m_currFrame = QImage(img.size(),QImage::Format_Grayscale8);
        // Convert to rgb888 image
        for(int y = 0; y < img.height(); y++)
        {
            uchar* ptr = m_currFrame.scanLine(y);
            uchar* inPtr = img.scanLine(y);
            for(int x = 0; x < img.width(); x++) {
                ptr[x] = (inPtr[4*x+2] + inPtr[4*x+1] + inPtr[4*x])/3;
            }
        }
    }
#else
    assert(frame->lengthX == DAVIS_IMG_WIDHT);
    assert(frame->lengthY == DAVIS_IMG_HEIGHT);
    {
        QMutexLocker locker(&m_frameMutex);
        m_newFrameAvailable = true;
        // Convert to qt image
        uchar* ptr = m_currFrame.bits();
        u_int16_t* inPtr = frame->pixels;
        for(int i = 0; i < DAVIS_IMG_WIDHT*DAVIS_IMG_HEIGHT; i++) {
            ptr[i] = inPtr[i]>>8;
        }
    }
#endif
    m_waitForData.wakeAll();
}

void Processor::run()
{
    printf("Processor started.\n");
    m_updateStatsTimer.start();
    //QElapsedTimer comTimer;
    while (m_isRunning) {
        //comTimer.start();
        // New events available ?
        if(m_eventQueue.size() == 0 && m_newFrameAvailable) {
            QMutexLocker locker(&m_waitMutex);
            m_waitForData.wait(&m_waitMutex);    // Avoid busy waiting
            continue;
        }
        // Process data
        else {
            // Process events and add them to the buffer
            // Remove old ones if necessary
            if(m_eventQueue.size()>0) {
                QMutexLocker locker(&m_queueMutex);
                m_eventBuffer.addEvents(m_eventQueue);
            }
            // Process frame
            if(m_newFrameAvailable && m_futureAnyncPedestrianDetector.isFinished()) {
                {
                    QMutexLocker locker(&m_frameMutex);
                    m_currFrameFPS = (1.0f-FPS_LOWPASS_FILTER_COEFF)*m_currFrameFPS+
                                     FPS_LOWPASS_FILTER_COEFF*(1000.0f/m_frameTimer.elapsed());
                    m_frameTimer.restart();
                    m_newFrameAvailable = false;
                }
                // Execute face detection in different thread
                //m_futureAnyncPedestrianDetector = QtConcurrent::run(this, &Processor::processImage);
            }

            // Recompute buffer stats
            if(m_updateStatsTimer.nsecsElapsed()/1000 > m_updateStatsInterval) {
                uint64_t elapsedTime = m_updateStatsTimer.nsecsElapsed()/1000;
                //printf("Update: %u",elapsedTime);
                m_updateStatsTimer.restart();
                updateStatistics(elapsedTime);
            }
        }
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

void Processor::tracking(std::vector<cv::Rect> &bboxes)
{
    QVector<sObjectStats> oldStats = m_stats;
    m_stats.clear();


    /*  if(bboxes.size() == 0) {
          cv::Rect r;
          r.x = 0;
          r.y = 0;
          r.width = DAVIS_IMG_WIDHT;
          r.height = DAVIS_IMG_HEIGHT;
          bboxes.push_back(r);
      }*/

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
        // Close enough ?
        if(currScore > TRACK_MIN_OVERLAP_RATIO) {
            const cv::Rect& r = bboxes.at(idx);
            o.trackingLost = false;
            /* o.roi = QRectF(o.roi.x()*0.8+0.2*r.x,
                            o.roi.y()*0.8+0.2*r.y,
                            o.roi.width()*0.8+0.2*r.width,
                            o.roi.height()*0.8+0.2*r.height);*/
            o.roi = QRectF(r.x,
                           r.y,
                           r.width,
                           r.height);
            o.lastROIUpdate = m_eventBuffer.getCurrTime();
            m_stats.push_back(o);
            trackedRects.push_back(idx);
        }
        // ROI not found but still really new ?
        else if(m_eventBuffer.getCurrTime()-o.lastROIUpdate < TRACK_DELAY_KEEP_ROI_US) {
            o.trackingLost = true;
            m_stats.push_back(o);
            trackedRects.push_back(idx);
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
        stats.lastROIUpdate = m_eventBuffer.getCurrTime();
        m_stats.push_back(stats);
    }
}
bool compare_rect(const cv::Rect & a, const cv::Rect &b)
{
    return a.area() > b.area();
}
std::vector<cv::Rect> Processor::detect()
{
    // Compute image of current event buffer
    cv::Mat bufferImg(cv::Size(DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT), CV_8UC1);
    bufferImg.setTo(cv::Scalar(0));
    uchar* p;
    auto & buff = m_eventBuffer.getLockedBuffer();
    for(sDVSEventDepacked & e:buff) {
        p = bufferImg.ptr<uchar>(e.y,e.x);
        *p = 255;
    }
    m_eventBuffer.releaseLockedBuffer();

    // Blur image
    //cv::imwrite( "Image.jpg", bufferImg );
    cv::GaussianBlur(bufferImg,bufferImg,
                     cv::Size(TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ,TRACK_BOX_DETECTOR_GAUSS_KERNEL_SZ),
                     TRACK_BOX_DETECTOR_GAUSS_SIGMA,TRACK_BOX_DETECTOR_GAUSS_SIGMA,cv::BORDER_REPLICATE);

    //cv::imwrite( "blured.jpg", bufferImg );
    // Treshold image
    cv::threshold(bufferImg,bufferImg,TRACK_BOX_DETECTOR_THRESHOLD,255,CV_THRESH_BINARY);
    //cv::imwrite( "threshold.jpg", bufferImg );
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

    for(int i = 0; i < qMin((int)tmpBoxes.size(), TRACK_BIGGEST_N_BOXES); i++) {
        cv::Rect r=tmpBoxes.at(i);
        // Expand bounding box
        r.x = qMax(0.0,r.x+r.width*(1-TRACK_BOX_SCALE)/2);
        r.y = qMax(0.0,r.y+r.height*(1-TRACK_BOX_SCALE)/2);
        r.width = qMin(DAVIS_IMG_WIDHT - r.x - 1.0,r.width*TRACK_BOX_SCALE);
        r.height = qMin(DAVIS_IMG_HEIGHT - r.y - 1.0,r.height*TRACK_BOX_SCALE);
        if(r.area() >= TRACK_MIN_AREA) {
            bboxes.push_back(r);
        }
    }
    return bboxes;
}

void Processor::updateStatistics(uint32_t elapsedTimeUs)
{
    // Update objects with new bounding box
    QMutexLocker locker(&m_statsMutex);
#ifndef ASSUME_SINGLE_PERSON
    std::vector<cv::Rect> bboxes = detect();
    tracking(bboxes);
#endif

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
    }
    m_eventBuffer.releaseLockedBuffer();

    // Recompute position and standard deviation
    // if there are enough events in the buffer
    //if(evCnt/(st.roi.width()*st.roi.height()) > MIN_EVENT_PER_BOX_SIZE_RATIO) {
    buff = m_eventBuffer.getLockedBuffer();
    for(sDVSEventDepacked & e:buff) {
        if(!isInROI(e,st.roi))
            continue;
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

    // Compute velocity if last point was set
    if(!st.centerInitiallyComputed) {
        newVelocity.setX(0);
        newVelocity.setY(0);
    } else {
        newVelocity.setX(1000000*(st.center.x()-newCenter.x())/st.deltaTimeLastDataUpdateUs);
        newVelocity.setY(1000000*(st.center.y()-newCenter.y())/st.deltaTimeLastDataUpdateUs);
    }
    st.deltaTimeLastDataUpdateUs = 0;

    st.center = newCenter;
    st.std = newStd;
    st.evCnt = evCnt;

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

    if(st.possibleFall) {
        if(st.center.y() < FALL_DETECTOR_Y_CENTER_THRESHOLD) {
            st.possibleFall = false;
        }
    } else if(st.center.y() > FALL_DETECTOR_Y_CENTER_THRESHOLD &&
              st.velocityNorm.y() < FALL_DETECTOR_Y_SPEED_THRESHOLD) {
        st.possibleFall = true;
    };
    st.centerInitiallyComputed = true;

    /* } else {
         // No velocity detected
         st.std = QPointF(0,0);
         st.evCnt = evCnt;
         st.velocity = QPointF(0,0);
         st.velocityNorm = QPointF(0,0);
         st.skippedLastUpdate = true;
         st.centerInitiallyComputed = false;
     }*/

    st.stdDevBox.setX(st.center.x()-st.std.x());
    st.stdDevBox.setY(st.center.y()-st.std.y());
    st.stdDevBox.setWidth(2*st.std.x()+1);
    st.stdDevBox.setHeight(2*st.std.y()+1);

}
