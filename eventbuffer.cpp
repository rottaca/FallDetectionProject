#include "eventbuffer.h"
#include <QMutexLocker>

#include "settings.h"

EventBuffer::EventBuffer():m_timeWindow(0)
{
}


void EventBuffer::clear()
{

    QMutexLocker locker(&m_lock);
    m_buffer.clear();
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
    QMutexLocker locker(&m_lock);

    // Remove all old events
    // Here, we have to lock for the whole period

    while (m_buffer.size() > 0 &&
            newTsStart - m_buffer.back().ts > m_timeWindow) {
        m_buffer.pop_back();
    }


    // Add events
    while(!events.empty()) {
        const sDVSEventDepacked &ev = events.front();
        // Don't block for the whole function or other threads are slowed down

        if(m_buffer.size() > 0 && m_buffer.front().ts > events.front().ts)
            printf("Time jump: %d to %d\n", m_buffer.front().ts,events.front().ts);

        {
            // BUGFIX: Skip if we recieve the same events
            //if(m_buffer.size() == 0 || m_buffer.front().x != ev.x || m_buffer.front().y != ev.y ||  ev.ts - m_buffer.front().ts > 10)
            m_buffer.push_front(ev);
        }

        events.pop();
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
