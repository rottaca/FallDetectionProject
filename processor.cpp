#include "processor.h"

#include <QtConcurrent/QtConcurrent>
#include <QImageReader>
#include <qimage.h>

#include <assert.h>

#include <opencv2/opencv.hpp>


Processor::Processor():
    m_timewindow(TIME_WINDOW_US),
    m_updateStatsInterval(UPDATE_INTERVAL_COMP_US)
{
    m_eventBuffer.setup(m_timewindow);
    m_currFrame = QImage(DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT,QImage::Format_RGB888);
    m_newFrameAvailable = false;
    m_nextId = 0;
    sObjectStats stats;

    stats.roi = QRectF(0,0,DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT);
    stats.id = m_nextId++;
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
            ptr[3*i] = inPtr[i]>>8;
            ptr[3*i + 1] = inPtr[i]>>8;
            ptr[3*i + 2] = inPtr[i]>>8;
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
                }
                // Execute face detection in different thread
                m_futureAnyncPedestrianDetector = QtConcurrent::run(this, &Processor::processImage);
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

        //cv::imwrite( "Image.jpg", gray_image );
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
//        roi_humans.clear();
    }

    {
        //printf("Humans: %zu\n",roi_humans.size());
        if(roi_humans.size() > 0) {
            QMutexLocker locker(&m_statsMutex);
            QVector<sObjectStats> oldStats = m_stats;
            m_stats.clear();

            std::vector<int> foundRects;
            for(int i = 0; i < oldStats.size(); i++) {
                sObjectStats o = oldStats.at(i);
                float oXR = o.roi.x()+o.roi.width();
                float oXL = o.roi.x();
                float oYT = o.roi.y();
                float oYB = o.roi.y()+o.roi.height();
                float sz = o.roi.width()*o.roi.height();

                //printf("O: %f %f %f %f\n",oXL,oYT,o.roi.width(),o.roi.height());

                float currScore = 0;
                int idx = -1;

                // Search for matching new rectangle
                for(int j=0; j < roi_humans.size(); j++) {
                    bool alreadyFound = false;
                    for(int k: foundRects)
                        if(k==j) {
                            alreadyFound=true;
                            break;
                        }
                    if(alreadyFound)
                        continue;
                    const cv::Rect& r = roi_humans.at(j);

                    float rXR = r.x+r.width;
                    float rXL = r.x;
                    float rYT = r.y;
                    float rYB = r.y+r.height;

                    //printf("ROI: %d %d %d %d\n",r.x,r.y,r.width,r.height);

                    float dx = qMin(rXR,oXR)-qMax(rXL,oXL);
                    float dy = qMin(rYB,oYB)-qMax(rYT,oYT);

                    float score = qMax(0.f,dx)*
                                  qMax(0.f,dy)/(sz);

                    //printf("Score: %f\n",score);
                    //printf("%f %f %f %f\n",qMin(rXR,oXR),qMax(rXL,oXL),
                    //       qMax(rYT,oYT),qMin(rYB,oYB));
                    //printf("%f %f\n",dx,dy);

                    if(score > currScore) {
                        currScore = score;
                        idx = j;
                    }
                }
                // Close enough ?
                float minScore = 0.8;
                if(currScore > minScore) {
                    const cv::Rect& r = roi_humans.at(idx);
                    o.roi = QRectF(r.x,r.y,r.width,r.height);
                    o.lastROIUpdate = m_eventBuffer.getCurrTime();
                    m_stats.push_back(o);
                    foundRects.push_back(idx);
                }
                // ROI not found but still really new ?
                else if(m_eventBuffer.getCurrTime()-o.lastROIUpdate < DELAY_KEEP_ROI_US) {
                    m_stats.push_back(o);
                    foundRects.push_back(idx);
                }
            }
            //printf("Tracked: %zu\n",foundRects.size());
            // Add all missing rois
            for(int i = 0; i < roi_humans.size(); i++) {
                bool alreadyFound = false;
                for(int k: foundRects)
                    if(k==i) {
                        alreadyFound=true;
                        break;
                    }
                if(alreadyFound)
                    continue;

                const cv::Rect& r = roi_humans.at(i);
                sObjectStats stats;
                stats.id = m_nextId++;
                stats.roi = QRectF(r.x,r.y,r.width,r.height);
                stats.lastROIUpdate = m_eventBuffer.getCurrTime();
                m_stats.push_back(stats);

            }

            //printf("Humans:");
            //for(sObjectStats o:m_stats)
            //    printf("%f %f %f %f\n",o.roi.x(),o.roi.y(),o.roi.width(),o.roi.height());
            //printf("New: %zu\n",roi_humans.size()-foundRects.size());
        }
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

void Processor::updateObjectStats(sObjectStats &st, uint32_t elapsedTimeUs)
{
    QPointF tmp, newCenter, newStd, newVelocity;
    size_t evCnt = 0;
    newCenter.setX(0);
    newCenter.setY(0);
    newStd.setX(0);
    newStd.setY(0);

    // accumulate time
    st.deltaTimeLastDataUpdate += elapsedTimeUs;


    auto & buff = m_eventBuffer.getLockedBuffer();
    for(sDVSEventDepacked & e:buff) {
        if(!isInROI(e,st.roi))
            continue;
        evCnt++;
    }
    m_eventBuffer.releaseLockedBuffer();

    // Recompute position and standard deviation
    // if there are enough events in the buffer
    if(evCnt > MIN_EVENT_TRESHOLD) {
        auto & buff = m_eventBuffer.getLockedBuffer();
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

        // Compute velocity
        newVelocity.setX(1000000*(st.center.x()-newCenter.x())/st.deltaTimeLastDataUpdate);
        newVelocity.setY(1000000*(st.center.y()-newCenter.y())/st.deltaTimeLastDataUpdate);
        st.deltaTimeLastDataUpdate = 0;
        newVelocity = 0.8*st.velocity + 0.2*newVelocity;

        st.center = newCenter;
        st.std = newStd;
        st.evCnt = evCnt;

        st.velocity = newVelocity;
        st.velocityNorm = newVelocity/(2*newStd.y());

    } else {
        // No velocity detected
        st.std = QPointF(0,0);
        st.evCnt = evCnt;
        st.velocity = QPointF(0,0);
        st.velocityNorm = QPointF(0,0);
    }

    st.bbox.setX(st.center.x()-st.std.x());
    st.bbox.setY(st.center.y()-st.std.y());
    st.bbox.setWidth(2*st.std.x()+1);
    st.bbox.setHeight(2*st.std.y()+1);
}
