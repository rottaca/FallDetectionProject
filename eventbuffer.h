#ifndef EVENTFIFO_H
#define EVENTFIFO_H

#include <QMutex>
#include <QImage>

#include <deque>

#include <libcaer/events/polarity.h>

#include "datatypes.h"

#include <queue>

class EventBuffer
{
public:
    EventBuffer();

    /**
    * @brief EventBuffer Creates an empty event buffer that
    *        holds all events in the specified timewindow.
    * @param timewindow
    */
    void setup(const uint32_t timewindow, const uint16_t sx, const uint16_t sy);

    /**
     * @brief addEvent Adds a new event to the
     *        buffer and removes all old events
     * @param event
     */
    void addEvent(const sDVSEventDepacked & event);
    void addEvents(std::queue<sDVSEventDepacked> &events);


    void clear();

    int getSize()
    {
        QMutexLocker locker(&m_lock);
        return m_buffer.size();
    }

    uint32_t getCurrTime()
    {
        QMutexLocker locker(&m_lock);
        if(m_buffer.size() > 0)
            return m_buffer.front().ts;
        else
            return 0;
    }

    std::deque<sDVSEventDepacked> &getLockedBuffer()
    {
        // Lock the buffer
        m_lock.lock();
        return m_buffer;
    }

    void releaseLockedBuffer()
    {
        m_lock.unlock();
    }

    QImage toImage();

protected:
    // Queue used as event buffer
    // deque provides fast insert and delete operataions.
    std::deque<sDVSEventDepacked> m_buffer;

    // TODO Add Kd-Tree for bounding box - search

    uint32_t m_timeWindow;
    uint16_t m_sx,m_sy;
    QMutex m_lock;
};

#endif // EVENTFIFO_H
