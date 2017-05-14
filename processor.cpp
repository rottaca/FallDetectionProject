#include "processor.h"

#include <QtConcurrent/QtConcurrent>

#include "settings.h"

Processor::Processor():
    m_timewindow(30000),
    m_updateStatsInterval(100)
{
    m_eventBuffer.setup(m_timewindow);

    m_stats.roi = QRectF(0,0,DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT);
    m_stats.evCnt = 0;

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
    m_waitForEvents.wakeAll();
    m_future.waitForFinished();
}

void Processor::newEvent(const sDVSEventDepacked & event)
{
    QMutexLocker locker(&m_queueMutex);
    m_eventQueue.push(event);
    m_waitForEvents.wakeAll();
}

void Processor::newFrame(const caerFrameEvent &frame)
{
    printf("New frame\n");
}

void Processor::run()
{
    printf("Processor started.\n");
    m_updateStatsTimer.start();
    while (m_isRunning) {

        // New events available ?
        if(m_eventQueue.size() == 0) {
            QMutexLocker locker(&m_queueMutex);
            m_waitForEvents.wait(&m_queueMutex);    // Avoid busy waiting
        }
        // Process data
        else {
            {
                QMutexLocker locker(&m_queueMutex);
                const sDVSEventDepacked &ev = m_eventQueue.front();
                emit updateUI(QString("%1 %2 %3 %4").arg(ev.ts).arg(ev.x).arg(ev.y).arg(ev.pol));
                m_eventBuffer.addEvent(ev);
                m_eventQueue.pop();
            }
            // Recompute buffer stats ?
            if(m_updateStatsTimer.elapsed() > m_updateStatsInterval) {
                m_updateStatsTimer.restart();
                updateStatistics();
            }
        }
    }
    printf("Processor stopped.\n");
}

void Processor::updateStatistics()
{
    QMutexLocker locker(&m_statsMutex);
    updateObjectStats(m_stats);
}

void Processor::updateObjectStats(sObjectStats &st)
{
    auto & buff = m_eventBuffer.getLockedBuffer();

    QPointF tmp;
    st.center.setX(0);
    st.center.setY(0);
    st.evCnt = 0;
    for(sDVSEventDepacked e:buff) {
        if(e.x >= st.roi.x() && e.x <= st.roi.x()+st.roi.width() &&
                e.y >= st.roi.y() && e.y <= st.roi.y()+st.roi.height()) {
            tmp.setX(e.x);
            tmp.setY(e.y);
            st.center += tmp;
            st.evCnt++;
        }
    }
    st.center /= st.evCnt;
    //printf("Center: %fx%f\n",m_center.x(),m_center.y());

    st.std.setX(0);
    st.std.setY(0);
    for(sDVSEventDepacked e:buff) {
        if(e.x >= st.roi.x() && e.x <= st.roi.x()+st.roi.width() &&
                e.y >= st.roi.y() && e.y <= st.roi.y()+st.roi.height()) {
            tmp.setX(e.x);
            tmp.setY(e.y);
            tmp -= st.center;
            tmp.setX(tmp.x()*tmp.x());
            tmp.setY(tmp.y()*tmp.y());
            tmp.setX(sqrtf(tmp.x()));
            tmp.setY(sqrtf(tmp.y()));
            st.std += tmp;
        }
    }
    st.std /= st.evCnt;
    //printf("Std: %fx%f\n",m_std.x(),m_std.y());

    m_eventBuffer.releaseLockedBuffer();
}
