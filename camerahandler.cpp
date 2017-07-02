#include "camerahandler.h"

#include <QVector2D>
#include <QtConcurrent/QtConcurrent>

#include "settings.h"

void playbackFinished(void* ptr)
{
    CameraHandler* p = (CameraHandler*)ptr;

    if(p->playbackFinishedCallback != NULL)
        p->playbackFinishedCallback(p->callbackParam);
    //p->disconnect();
}

CameraHandler::CameraHandler()
    :m_davisHandle(NULL),
     m_playbackHandle(NULL),
     m_isStreaming(false),
     m_isConnected(false),
     m_eventReciever(nullptr),
     m_frameReciever(nullptr)
{
    currTs = 0;
}
CameraHandler::~CameraHandler()
{
    if(m_isConnected)
        disconnect();
    QMutexLocker locker(&m_camLock);
}

void CameraHandler::disconnect()
{
    if(m_isStreaming)
        stopStreaming();

    m_isConnected = false;
    QMutexLocker locker(&m_camLock);
    if(m_davisHandle != NULL) {
        caerDeviceClose(&m_davisHandle);
        m_davisHandle = NULL;
    } else if(m_playbackHandle) {
        playbackClose(m_playbackHandle);
        m_playbackHandle = NULL;
        playbackFinishedCallback = NULL;
        callbackParam = NULL;
    }
}
bool CameraHandler::connect(QString file, void (*playbackFinishedCallback)(void*), void* param)
{
    if(m_isConnected)
        disconnect();

    m_playbackHandle = playbackOpen(file.toStdString().c_str(),playbackFinished,this);

    if(m_playbackHandle == NULL) {
        printf("Can't open file for playback!\n");
        return false;
    } else {
        m_isConnected = true;
        this->playbackFinishedCallback = playbackFinishedCallback;
        this->callbackParam = param;

        playbackInfo info = caerPlaybackInfoGet(m_playbackHandle);

        printf("DVS X: %d, DVS Y: %d.\n",  info->sx, info->sy);

        return true;
    }
}

bool CameraHandler::connect(int devId)
{
    if(m_isConnected)
        disconnect();
    m_davisHandle = caerDeviceOpen(devId, CAER_DEVICE_DAVIS, 0, 0, NULL);

    if(m_davisHandle == NULL) {
        printf("Can't connect to device!\n");
        return false;
    }
    m_isConnected = true;
    struct caer_davis_info davis_info = caerDavisInfoGet(m_davisHandle);

    printf("%s --- ID: %d, Master: %d, DVS X: %d, DVS Y: %d, Logic: %d.\n", davis_info.deviceString,
           davis_info.deviceID, davis_info.deviceIsMaster, davis_info.dvsSizeX, davis_info.dvsSizeY,
           davis_info.logicVersion);

    writeConfig();
    return true;
}

void CameraHandler::startStreaming()
{
    if(m_isStreaming)
        stopStreaming();
    m_isStreaming = true;
    m_future = QtConcurrent::run(this, &CameraHandler::run);
}

void CameraHandler::stopStreaming()
{
    m_isStreaming = false;
    m_future.waitForFinished();
}
QVector2D CameraHandler::getFrameSize()
{
    if(m_isConnected) {
        int sx = 0,sy = 0;
        if(m_davisHandle) {
            struct caer_davis_info info = caerDavisInfoGet(m_davisHandle);
            sx = info.apsSizeX;
            sy = info.apsSizeY;
        } else if(m_playbackHandle) {
            playbackInfo info = caerPlaybackInfoGet(m_playbackHandle);
            sx = info->sx;
            sy = info->sy;
        }
        return QVector2D(sx,sy);

    } else {
        return QVector2D();
    }
}

void CameraHandler::run()
{
    if(m_davisHandle != NULL) {
        bool success = caerDeviceDataStart(m_davisHandle, NULL, NULL, NULL, NULL, NULL);
        if(!success) {
            printf("Failed to start data transfer!\n");
            return;
        }

        // Let's turn on blocking data-get mode to avoid wasting resources.
        caerDeviceConfigSet(m_davisHandle, CAER_HOST_CONFIG_DATAEXCHANGE, CAER_HOST_CONFIG_DATAEXCHANGE_BLOCKING, true);
    } else if(m_playbackHandle != NULL) {
        playbackDataStart(m_playbackHandle);
    }
    printf("Streaming started.\n");
    QElapsedTimer timer;
    timer.start();
    while (m_isStreaming) {
        QMutexLocker locker(&m_camLock);
        caerEventPacketContainer packetContainer = NULL;
        if(m_davisHandle != NULL)
            packetContainer = caerDeviceDataGet(m_davisHandle);
        else if(m_playbackHandle != NULL)
            packetContainer = playbackDataGet(m_playbackHandle);

        if (packetContainer == NULL) {
            // Wait a bit!
            QThread::usleep(1);
            //QThread::yieldCurrentThread();
            continue; // Skip if nothing there.
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
                for(int i = 0; i < polarity->packetHeader.eventValid; i++) {
                    // Get full timestamp and addresses of first event.
                    caerPolarityEvent firstEvent = caerPolarityEventPacketGetEvent(polarity, i);

                    sDVSEventDepacked e;
                    e.ts = caerPolarityEventGetTimestamp(firstEvent);
                    e.x = caerPolarityEventGetX(firstEvent);
                    e.y = caerPolarityEventGetY(firstEvent);
                    e.pol = caerPolarityEventGetPolarity(firstEvent);

                    if(m_frameReciever != nullptr) {
                        m_eventReciever->newEvent(e);
                    }
                }

            } // Frames
            else if(i == FRAME_EVENT) {
                caerFrameEventPacket framePacket = (caerFrameEventPacket) packetHeader;
                for(int i = 0; i < framePacket->packetHeader.eventValid; i++) {
                    caerFrameEvent frame = caerFrameEventPacketGetEvent(framePacket,i);

                    if(m_frameReciever != nullptr) {
                        m_frameReciever->newFrame(frame);
                    }
                }
            }
        }
        caerEventPacketContainerFree(packetContainer);
    }

    if(m_davisHandle != NULL) {
        caerDeviceDataStop(m_davisHandle);
    } else if(m_playbackHandle != NULL) {
        playbackDataStop(m_playbackHandle);
    }

    printf("Streaming stopped.\n");
}
void CameraHandler::writeConfig()
{
    QMutexLocker locker(&m_camLock);
    if(!m_isConnected)
        return;

    // Send the default configuration before using the device.
    // No configuration is sent automatically!
    //caerDeviceSendDefaultConfig(m_davisHandle);

    // Enable autoexposure
    //caerDeviceConfigSet(m_davisHandle, DAVIS_CONFIG_APS, DAVIS_CONFIG_APS_AUTOEXPOSURE, true);

    // Tweak some biases, to increase bandwidth in this case.
//    struct caer_bias_coarsefine coarseFineBias;

//    coarseFineBias.coarseValue = 2;
//    coarseFineBias.fineValue = 116;
//    coarseFineBias.enabled = true;
//    coarseFineBias.sexN = false;
//    coarseFineBias.typeNormal = true;
//    coarseFineBias.currentLevelNormal = true;
//    caerDeviceConfigSet(m_davisHandle, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRBP,
//                        caerBiasCoarseFineGenerate(coarseFineBias));

//    coarseFineBias.coarseValue = 1;
//    coarseFineBias.fineValue = 33;
//    coarseFineBias.enabled = true;
//    coarseFineBias.sexN = false;
//    coarseFineBias.typeNormal = true;
//    coarseFineBias.currentLevelNormal = true;
//    caerDeviceConfigSet(m_davisHandle, DAVIS_CONFIG_BIAS, DAVIS240_CONFIG_BIAS_PRSFBP,
//                        caerBiasCoarseFineGenerate(coarseFineBias));
}
