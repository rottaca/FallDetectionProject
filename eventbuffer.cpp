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

QImage EventBuffer::toImage()
{
    QImage img(DAVIS_IMG_WIDHT,DAVIS_IMG_HEIGHT,QImage::Format_RGB888);
    int maxEventCntPerPx = 2;
    int colorOffset = 255/maxEventCntPerPx;

    img.fill(Qt::white);
    {
        QMutexLocker locker(&m_lock);
        for(sDVSEventDepacked e:m_buffer) {
            *(img.scanLine(e.y) + 3*e.x) = qMax(0,*(img.scanLine(e.y) + 3*e.x) - colorOffset);
            *(img.scanLine(e.y) + 3*e.x + 1) = qMax(0,*(img.scanLine(e.y) + 3*e.x + 1) - colorOffset);
            *(img.scanLine(e.y) + 3*e.x + 2) = qMax(0,*(img.scanLine(e.y) + 3*e.x + 2) - colorOffset);
        }
    }

    return img;
}
