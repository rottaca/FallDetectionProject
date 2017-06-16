#ifndef CAMERAHANDLER_H
#define CAMERAHANDLER_H

#include <atomic>

#include <QMutexLocker>
#include <QMutex>
#include <QThread>
#include <QFuture>

#include <libcaer/devices/davis.h>
#include <libcaer/devices/playback.h>

#include "datatypes.h"

class CameraHandler
{
public:
    CameraHandler();
    ~CameraHandler();

    bool connect(QString file, void (*playbackFinishedCallback) (void*), void* param);
    bool connect(int devId = 1);
    void disconnect();
    void startStreaming();
    void stopStreaming();

    QVector2D getFrameSize();

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

    bool isStreaming()
    {
        return m_isStreaming;
    }
    bool isConnected()
    {
        return m_isConnected;
    }

    void changePlaybackSpeed(float speed)
    {
        //QMutexLocker locker(&m_camLock);
        if(m_playbackHandle != NULL)
            playbackChangeSpeed(m_playbackHandle,speed);

    }

    void (*playbackFinishedCallback) (void*);
    void* callbackParam;
protected:
    caerDeviceHandle m_davisHandle;
    playbackHandle m_playbackHandle;
    std::atomic_bool m_isStreaming;
    std::atomic_bool m_isConnected;
    QMutex m_camLock;
    QFuture<void> m_future;

    IDVSEventReciever* m_eventReciever;
    IFrameReciever* m_frameReciever;

    int32_t currTs;

};

#endif // CAMERAHANDLER_H
