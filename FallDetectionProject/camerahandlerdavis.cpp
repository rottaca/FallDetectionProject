#include "camerahandlerdavis.h"


#include <QtConcurrent/QtConcurrent>


CameraHandlerDavis::CameraHandlerDavis()
    :m_davisHandle(NULL),
     m_isStreaming(false),
     m_isConnected(false),
     m_eventReciever(nullptr),
     m_frameReciever(nullptr)
{

}
CameraHandlerDavis::~CameraHandlerDavis()
{
    QMutexLocker locker(&m_camLock);
    if(m_isStreaming)
        stopStreaming();
    if(m_isConnected)
        disconnect();
}

void CameraHandlerDavis::disconnect()
{
    QMutexLocker locker(&m_camLock);
    if(m_isStreaming)
        stopStreaming();

    m_isConnected = false;
    caerDeviceClose(&m_davisHandle);
}

bool CameraHandlerDavis::connect(int devId)
{
    if(m_isStreaming)
        stopStreaming();
    if(m_isConnected)
        disconnect();
    m_davisHandle = caerDeviceOpen(devId, CAER_DEVICE_DAVIS_FX2, 0, 0, NULL);

    if(m_davisHandle == NULL) {
        printf("Can't connect to device!\n");
        return false;
    }

    struct caer_davis_info davis_info = getInfo();

    printf("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n", davis_info.deviceString,
           davis_info.deviceID, davis_info.deviceIsMaster, davis_info.dvsSizeX, davis_info.dvsSizeY,
           davis_info.logicVersion);

    writeConfig();
    return true;
}

void CameraHandlerDavis::startStreaming()
{
    if(m_isStreaming)
        stopStreaming();
    m_isStreaming = true;
    m_future = QtConcurrent::run(this, &CameraHandlerDavis::run);
}

void CameraHandlerDavis::stopStreaming()
{
    m_isStreaming = false;
    m_future.waitForFinished();
}

struct caer_davis_info CameraHandlerDavis::getInfo()
{
    QMutexLocker locker(&m_camLock);
    return caerDavisInfoGet(m_davisHandle);
}

void CameraHandlerDavis::run()
{
    printf("Streaming started.\n");
    caerDeviceDataStart(m_davisHandle, NULL, NULL, NULL, NULL, NULL);

    while (m_isStreaming) {
        QMutexLocker locker(&m_camLock);
        caerEventPacketContainer packetContainer = caerDeviceDataGet(m_davisHandle);
        if (packetContainer == NULL) {
            QThread::usleep(1);
            continue; // Skip if nothing there.
            // printf("No Data for camera handler..\n");
        }
        int32_t packetNum = caerEventPacketContainerGetEventPacketsNumber(packetContainer);
        //printf("\nGot event container with %d packets (allocated).\n", packetNum);
        // Iterate over all recieved packets
        for (int32_t i = 0; i < packetNum; i++) {
            caerEventPacketHeader packetHeader = caerEventPacketContainerGetEventPacket(packetContainer, i);
            if (packetHeader == NULL) {
                //printf("Packet %d is empty (not present).\n", i);
                continue; // Skip if nothing there.
            }

            //printf("Packet %d of type %d -> size is %d.\n", i, caerEventPacketHeaderGetEventType(packetHeader),
            //        caerEventPacketHeaderGetEventNumber(packetHeader));

            // DVS-Events
            if (i == POLARITY_EVENT) {
                caerPolarityEventPacket polarity = (caerPolarityEventPacket) packetHeader;

                // Get full timestamp and addresses of first event.
                caerPolarityEvent firstEvent = caerPolarityEventPacketGetEvent(polarity, 0);

                sDVSEventDepacked e;
                e.ts = caerPolarityEventGetTimestamp(firstEvent);
                e.x = caerPolarityEventGetX(firstEvent);
                e.y = caerPolarityEventGetY(firstEvent);
                e.pol = caerPolarityEventGetPolarity(firstEvent);

                //printf("First polarity event - ts: %d, x: %d, y: %d, pol: %d.\n", e.ts, e.x, e.y, e.pol);

                if(m_frameReciever != nullptr) {
                    m_eventReciever->newEvent(e);
                }

            } // Frames
            else if(i == FRAME_EVENT) {
                caerFrameEventPacket framePacket = (caerFrameEventPacket) packetHeader;
                caerFrameEvent frame = caerFrameEventPacketGetEvent(framePacket,0);

                if(m_frameReciever != nullptr) {
                    m_frameReciever->newFrame(frame);
                }
            }
        }
        caerEventPacketContainerFree(packetContainer);
    }
    caerDeviceDataStop(m_davisHandle);
    printf("Streaming stopped.\n");
}
void CameraHandlerDavis::writeConfig()
{
    QMutexLocker locker(&m_camLock);
    if(!m_isConnected)
        return;

    // Send the default configuration before using the device.
    // No configuration is sent automatically!
    caerDeviceSendDefaultConfig(m_davisHandle);
}
