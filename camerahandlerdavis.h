#ifndef CAMERAHANDLER_H
#define CAMERAHANDLER_H

#include <atomic>

#include <QMutexLocker>
#include <QMutex>
#include <QThread>
#include <QFuture>

#include <libcaer/devices/davis.h>

#include "datatypes.h"

class CameraHandlerDavis
{
public:
    CameraHandlerDavis();
    ~CameraHandlerDavis();
    bool connect(int devId = 1);
    void disconnect();
    void startStreaming();
    void stopStreaming();
    struct caer_davis_info getInfo();
    void writeConfig();

    void run();

    class IDVSEventReciever
    {
    public:
        virtual void newEvent(const sDVSEventDepacked & event)= 0;
    };
    class IFrameReciever
    {
    public:
        virtual void newFrame(const caerFrameEvent &)= 0;
    };

    void setDVSEventReciever(IDVSEventReciever* reciever)
    {
        m_eventReciever = reciever;
    }

    void setFrameReciever(IFrameReciever* reciever)
    {
        m_frameReciever = reciever;
    }

protected:
    caerDeviceHandle m_davisHandle;
    std::atomic_bool m_isStreaming;
    std::atomic_bool m_isConnected;
    QMutex m_camLock;
    QFuture<void> m_future;

    IDVSEventReciever* m_eventReciever;
    IFrameReciever* m_frameReciever;

    int32_t currTs;
};

#endif // CAMERAHANDLER_H
