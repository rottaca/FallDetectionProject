#include "eventbuffer.h"
#include <QMutexLocker>

#include "settings.h"

EventBuffer::EventBuffer():m_timeWindow(0)
{
}

void EventBuffer::setup(const uint32_t timewindow)
{
    QMutexLocker locker(&m_lock);
    m_timeWindow = timewindow;
    m_buffer.clear();
}

void EventBuffer::addEvent(const sDVSEventDepacked &event)
{
    QMutexLocker locker(&m_lock);
    // Remove all old events
    while (m_buffer.size() > 0 &&
            event.ts - m_buffer.back().ts > m_timeWindow) {
        m_buffer.pop_back();
    }

    // Add new event
    m_buffer.push_front(event);
}
void EventBuffer::addEvents(std::queue<sDVSEventDepacked> & events)
{
    if(events.size() == 0)
        return;

    uint32_t newTsStart = events.back().ts;

    // Add events
    //int lastTS = events.front().ts;
    while(!events.empty()) {
        const sDVSEventDepacked &ev = events.front();
        // Don't block for the whole function or other threads are slowed down
        {
            QMutexLocker locker(&m_lock);
            m_buffer.push_front(ev);
        }
        //if(lastTS > events.front().ts)
        // printf("Jump: %d to %d\n", lastTS,events.front().ts);
        //lastTS = events.front().ts;
        events.pop();
    }

    // Remove all old events
    // Here, we have to lock for the whole period
    QMutexLocker locker(&m_lock);
    while (m_buffer.size() > 0 &&
            newTsStart - m_buffer.back().ts > m_timeWindow) {
        m_buffer.pop_back();
    }
    //printf("Buff: %zu\n",m_buffer.size());
}

QImage EventBuffer::toImage()
{
    QImage img(DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT,QImage::Format_RGB888);

    img.fill(Qt::white);
    QMutexLocker locker(&m_lock);
    // Get current time and color according to temporal distance
    uint32_t currTime = m_buffer.front().ts;

    for(sDVSEventDepacked e:m_buffer) {
        uchar c = 255*(currTime-e.ts)/m_timeWindow;
        *(img.scanLine(e.y) + 3*e.x) = c;
        *(img.scanLine(e.y) + 3*e.x + 1) = c;
        *(img.scanLine(e.y) + 3*e.x + 2) = c;
    }
    return img;
}
