#include "processor.h"

#include <QtConcurrent/QtConcurrent>

Processor::Processor():
    m_timewindow(30000),
    m_updateStatsInterval(100)
{
    m_eventBuffer.setup(m_timewindow);
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
                updateStatistics();
            }
        }
    }
    printf("Processor stopped.\n");
}

void Processor::updateStatistics()
{
    auto & buff = m_eventBuffer.getLockedBuffer();

    QPointF center(0,0);
    QPointF tmp;
    for(sDVSEventDepacked e:buff) {
        tmp.setX(e.x);
        tmp.setY(e.y);
        center += tmp;
    }
    center /= buff.size();
    printf("Center: %fx%f\n",center.x(),center.y());

    QPointF std(0,0);
    for(sDVSEventDepacked e:buff) {
        tmp.setX(e.x);
        tmp.setY(e.y);
        tmp -= center;
        tmp.setX(tmp.x()*tmp.x());
        tmp.setY(tmp.y()*tmp.y());
        std += tmp;
    }
    std.setX(sqrtf(std.x()));
    std.setY(sqrtf(std.y()));
    std /= buff.size();
    printf("Std: %fx%f\n",std.x(),std.y());

    m_eventBuffer.releaseLockedBuffer();
}
