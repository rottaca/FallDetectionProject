#include "eventbuffer.h"
#include <QMutexLocker>

EventBuffer::EventBuffer():m_timewindow(0)
{
}

void EventBuffer::setup(const uint32_t timewindow)
{
    QMutexLocker locker(&m_lock);
    m_timewindow = timewindow;
    m_buffer.clear();
}

void EventBuffer::addEvent(const sDVSEventDepacked &event)
{
    QMutexLocker locker(&m_lock);
    // Remove all old events
    while (m_buffer.size() > 0 &&
            event.ts - m_buffer.back().ts > m_timewindow) {

        m_buffer.pop_back();
    }

    // Add new event
    m_buffer.push_front(event);
}
