#include <unistd.h>
#include "btmgr.h"
#include "btmgr_priv.h"
#include "btrCore.h"
#include "rmfAudioCapture.h"
#include "btrMgr_streamOut.h"

tBTRCoreHandle gBTRCoreHandle = NULL;
BTMgrDeviceHandle gCurStreamingDevHandle = 0;
stBTRCoreAdapter gDefaultAdapterContext;
stBTRCoreListAdapters gListOfAdapters;


BTMGR_PairedDevicesList_t gListOfPairedDevices;
unsigned char gIsDiscoveryInProgress = 0;
unsigned char gIsDeviceConnected = 0;

const char* BTMGR_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
const char* BTMGR_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

static BTMGR_EventCallback m_eventCallbackFunction = NULL;

typedef struct BTMGR_StreamingHandles_t {
    RMF_AudioCaptureHandle hAudCap;
    tBTRMgrSoHdl hBTRMgrSoHdl;
    unsigned long bytesWritten;
    unsigned samplerate;
    unsigned channels;
    unsigned bitsPerSample;
} BTMGR_StreamingHandles;

BTMGR_StreamingHandles gStreamCaptureSettings;

/* Private function declarations */
#define BTMGR_SIGNAL_POOR       (-90)
#define BTMGR_SIGNAL_FAIR       (-70)
#define BTMGR_SIGNAL_GOOD       (-60)
BTMGR_DeviceType_t btmgr_MapSignalStrengthToRSSI (int signalStrength);
BTMGR_DeviceType_t btmgr_MapDeviceTypeFromCore (enBTRCoreDeviceClass device_type);


rmf_Error cbBufferReady (void *pContext, void* pInDataBuf, unsigned int inBytesToEncode)
{
    BTMGR_StreamingHandles *data = (BTMGR_StreamingHandles*) pContext;
    BTRMgr_SO_SendBuffer(data->hBTRMgrSoHdl, pInDataBuf, inBytesToEncode);

    data->bytesWritten += inBytesToEncode;
    return RMF_SUCCESS;
}

BTMGR_Result_t btmgr_StartCastingAudio (int outFileFd, int outMTUSize)
{
    int inBytesToEncode = 2048;
    RMF_AudioCapture_Settings settings;

    if (0 == gCurStreamingDevHandle)
    {
        /* Reset the buffer */
        memset(&gStreamCaptureSettings, 0, sizeof(gStreamCaptureSettings));
        memset(&settings, 0, sizeof(settings));

        /* Init StreamOut module - Create Pipeline */
        BTRMgr_SO_Init(&gStreamCaptureSettings.hBTRMgrSoHdl);

        /* could get defaults from audio capture, but for the sample app we want to write a the wav header first*/
        gStreamCaptureSettings.bitsPerSample = 16;
        gStreamCaptureSettings.samplerate = 48000;
        gStreamCaptureSettings.channels = 2;

        if (RMF_AudioCapture_Open(&gStreamCaptureSettings.hAudCap))
        {
            return BTMGR_RESULT_GENERIC_FAILURE;
        }

        RMF_AudioCapture_GetDefaultSettings(&settings);
        settings.cbBufferReady      = cbBufferReady;
        settings.cbBufferReadyParm  = &gStreamCaptureSettings;
        settings.fifoSize           = 16 * inBytesToEncode;
        settings.threshold          = inBytesToEncode;

        BTRMgr_SO_Start(gStreamCaptureSettings.hBTRMgrSoHdl, inBytesToEncode, outFileFd, outMTUSize);

        if (RMF_AudioCapture_Start(gStreamCaptureSettings.hAudCap, &settings))
        {
            return BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return BTMGR_RESULT_SUCCESS;
}

void btmgr_StopCastingAudio ()
{
    if (gCurStreamingDevHandle)
    {
        RMF_AudioCapture_Stop(gStreamCaptureSettings.hAudCap);

        BTRMgr_SO_SendEOS(gStreamCaptureSettings.hBTRMgrSoHdl);
        BTRMgr_SO_Stop(gStreamCaptureSettings.hBTRMgrSoHdl);

        RMF_AudioCapture_Close(gStreamCaptureSettings.hAudCap);
        BTRMgr_SO_DeInit(gStreamCaptureSettings.hBTRMgrSoHdl);
    }

    return;
}

void set_discovery_status (unsigned char status)
{
    gIsDiscoveryInProgress = status;
}

unsigned char isDiscoveryInProgress(void)
{
    return gIsDiscoveryInProgress;
}

unsigned char btmgr_IsThisPairedDevice (BTMgrDeviceHandle handle)
{
    int j = 0;
    for (j = 0; j < gListOfPairedDevices.m_numOfDevices; j++)
    {
        if (handle == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle)
        {
            return 1;
        }
    }
    return 0;
}

BTMGR_DeviceType_t btmgr_MapDeviceTypeFromCore (enBTRCoreDeviceClass device_type)
{
    BTMGR_DeviceType_t type = BTMGR_DEVICE_TYPE_UNKNOWN;
    switch (device_type)
    {
        case enBTRCoreAV_WearableHeadset:
            type = BTMGR_DEVICE_TYPE_WEARABLE_HEADSET;
            break;
        case enBTRCoreAV_Handsfree:
            type = BTMGR_DEVICE_TYPE_HANDSFREE;
            break;
        case enBTRCoreAV_Microphone:
            type = BTMGR_DEVICE_TYPE_MICROPHONE;
            break;
        case enBTRCoreAV_Loudspeaker:
            type = BTMGR_DEVICE_TYPE_LOUDSPEAKER;
            break;
        case enBTRCoreAV_Headphones:
            type = BTMGR_DEVICE_TYPE_HEADPHONES;
            break;
        case enBTRCoreAV_PortableAudio:
            type = BTMGR_DEVICE_TYPE_PORTABLE_AUDIO;
            break;
        case enBTRCoreAV_CarAudio:
            type = BTMGR_DEVICE_TYPE_CAR_AUDIO;
            break;
        case enBTRCoreAV_STB:
            type = BTMGR_DEVICE_TYPE_STB;
            break;
        case enBTRCoreAV_HIFIAudioDevice:
            type = BTMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE;
            break;
        case enBTRCoreAV_VCR:
            type = BTMGR_DEVICE_TYPE_VCR;
            break;
        case enBTRCoreAV_VideoCamera:
            type = BTMGR_DEVICE_TYPE_VIDEO_CAMERA;
            break;
        case enBTRCoreAV_Camcoder:
            type = BTMGR_DEVICE_TYPE_CAMCODER;
            break;
        case enBTRCoreAV_VideoMonitor:
            type = BTMGR_DEVICE_TYPE_VIDEO_MONITOR;
            break;
        case enBTRCoreAV_TV:
            type = BTMGR_DEVICE_TYPE_TV;
            break;
        case enBTRCoreAV_VideoConference:
            type = BTMGR_DEVICE_TYPE_VIDEO_CONFERENCE;
            break;
        case enBTRCoreAV_Reserved:
        case enBTRCoreAV_Unknown:
            type = BTMGR_DEVICE_TYPE_UNKNOWN;
            break;
    }
    return type;
}

BTMGR_DeviceType_t btmgr_MapSignalStrengthToRSSI (int signalStrength)
{
    BTMGR_RSSIValue_t rssi = BTMGR_RSSI_NONE;

    if (signalStrength >= BTMGR_SIGNAL_GOOD)
        rssi = BTMGR_RSSI_EXCELLENT;
    else if (signalStrength >= BTMGR_SIGNAL_FAIR)
        rssi = BTMGR_RSSI_GOOD;
    else if (signalStrength >= BTMGR_SIGNAL_POOR)
        rssi = BTMGR_RSSI_FAIR;
    else
        rssi = BTMGR_RSSI_POOR;

    return rssi;
}

void btmgr_DeviceDiscoveryCallback (stBTRCoreScannedDevices devicefound)
{
    if (isDiscoveryInProgress() && (m_eventCallbackFunction))
    {
        BTMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));
        
        newEvent.m_adapterIndex = 0;
        newEvent.m_eventType = BTMGR_EVENT_DEVICE_DISCOVERY_UPDATE;

        /*  Post a callback */
        newEvent.m_discoveredDevice.m_deviceHandle = devicefound.deviceId;
        newEvent.m_discoveredDevice.m_deviceType = btmgr_MapDeviceTypeFromCore(devicefound.device_type);
        newEvent.m_discoveredDevice.m_signalLevel = devicefound.RSSI;
        newEvent.m_discoveredDevice.m_rssi = btmgr_MapSignalStrengthToRSSI(devicefound.RSSI);
        strncpy (newEvent.m_discoveredDevice.m_name, devicefound.device_name, (BTMGR_NAME_LEN_MAX - 1));
        strncpy (newEvent.m_discoveredDevice.m_deviceAddress, devicefound.device_address, (BTMGR_NAME_LEN_MAX - 1));
        newEvent.m_discoveredDevice.m_isPairedDevice = btmgr_IsThisPairedDevice (newEvent.m_discoveredDevice.m_deviceHandle);

        m_eventCallbackFunction (newEvent);
    }
    return;
}

void btmgr_DeviceStatusCallback (stBTRCoreDevStateCBInfo* p_StatusCB,  void* apvUserData)
{
    return;
}

unsigned char getAdaptherCount (void)
{
    unsigned char numbers = 0;

    numbers = gListOfAdapters.number_of_adapters;

    return numbers;
}

const char* getAdaptherPath (unsigned char index_of_adapter)
{
    const char* pReturn = NULL;

    if (gListOfAdapters.number_of_adapters)
    {
        if ((index_of_adapter < gListOfAdapters.number_of_adapters) &&
            (index_of_adapter < BTRCORE_MAX_NUM_BT_DEVICES))
        {
            pReturn = gListOfAdapters.adapter_path[index_of_adapter];
        }
    }

    return pReturn;
}

/* Public Functions */
BTMGR_Result_t BTMGR_Init()
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    const char* pDebugConfig = NULL;

    if (NULL == gBTRCoreHandle)
    {
        /* Init the logger */
        if( access(BTMGR_DEBUG_OVERRIDE_PATH, F_OK) != -1 )
            pDebugConfig = BTMGR_DEBUG_OVERRIDE_PATH;
        else
            pDebugConfig = BTMGR_DEBUG_ACTUAL_PATH;

        rdk_logger_init(pDebugConfig);

        /* Initialze all the database */
        memset (&gDefaultAdapterContext, 0, sizeof(gDefaultAdapterContext));
        memset (&gListOfAdapters, 0, sizeof(gListOfAdapters));
        memset(&gStreamCaptureSettings, 0, sizeof(gStreamCaptureSettings));
        memset (&gListOfPairedDevices, 0, sizeof(gListOfPairedDevices));
        gIsDiscoveryInProgress = 0;

        /* Init the mutex */

        /* Call the Core/HAL init */
        halrc = BTRCore_Init(&gBTRCoreHandle);
        if ((NULL == gBTRCoreHandle) || (enBTRCoreSuccess != halrc))
        {
            BTMGRLOG_ERROR ("BTMGR_Init : Could not initialize BTRCore/HAL module\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
        else
        {
            if (enBTRCoreSuccess != BTRCore_GetListOfAdapters (gBTRCoreHandle, &gListOfAdapters))
                BTMGRLOG_ERROR ("Failed to get the total number of Adapters present\n"); /* Not a Error case anyway */

            BTMGRLOG_INFO ("Number of Adapters found are = %u\n", gListOfAdapters.number_of_adapters);
            if (0 == gListOfAdapters.number_of_adapters)
            {
                BTMGRLOG_WARN("Bluetooth adapter NOT Found! Expected to be connected to make use of this module; Not considered as failure\n");
            }
            else
            {
                /* you have atlesat one BlueTooth adapter. Now get the Default Adapter path for future usages; */
                gDefaultAdapterContext.bFirstAvailable = 1; /* This is unused by core now but lets fill it */
                if (enBTRCoreSuccess == BTRCore_GetAdapter(gBTRCoreHandle, &gDefaultAdapterContext))
                {
                    BTMGRLOG_INFO ("Aquired default Adapter; Path is %s\n", gDefaultAdapterContext.pcAdapterPath);
                }

                /* TODO: Handling multiple Adapters */
                if (gListOfAdapters.number_of_adapters > 1)
                    BTMGRLOG_WARN("There are more than 1 Bluetooth Adapters Found! Lets handle it properly\n");
            }

            /* Register for callback to get the status of connected Devices */
            BTRCore_RegisterStatusCallback(gBTRCoreHandle, btmgr_DeviceStatusCallback, NULL);

            /* Register for callback to get the Discovered Devices */
            BTRCore_RegisterDiscoveryCallback(gBTRCoreHandle, btmgr_DeviceDiscoveryCallback, NULL);
        }
    }
    else
    {
        BTMGRLOG_WARN("BTMGR_Init : Already Inited; Return Success\n");
    }

    return rc;
}

BTMGR_Result_t BTMGR_GetNumberOfAdapters(unsigned char *pNumOfAdapters)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCoreListAdapters listOfAdapters;

    memset (&listOfAdapters, 0, sizeof(listOfAdapters));
    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if (NULL == pNumOfAdapters)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        halrc = BTRCore_GetListOfAdapters (gBTRCoreHandle, &listOfAdapters);
        if (enBTRCoreSuccess == halrc)
        {
            *pNumOfAdapters = listOfAdapters.number_of_adapters;
            /* Copy to our backup */
            if (listOfAdapters.number_of_adapters != gListOfAdapters.number_of_adapters)
                memcpy (&gListOfAdapters, &listOfAdapters, sizeof (stBTRCoreListAdapters));

            BTMGRLOG_INFO ("BTMGR_GetNumberOfAdapters : Available Adapters = %d\n", listOfAdapters.number_of_adapters);
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetNumberOfAdapters : Could not find Adapters\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterName(unsigned char index_of_adapter, const char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    char name[BTMGR_NAME_LEN_MAX];
    memset(name, '\0', sizeof(name));

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((NULL == pNameOfAdapter) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            strncpy (name, pNameOfAdapter, (BTMGR_NAME_LEN_MAX - 1));
            halrc = BTRCore_SetAdapterName(gBTRCoreHandle, pAdapterPath, name);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_SetAdapterName : Failed to set Adapter Name\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_SetAdapterName : Set Successfully\n");
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_SetAdapterName : Failed to adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterName(unsigned char index_of_adapter, char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    char name[BTMGR_NAME_LEN_MAX];
    memset(name, '\0', sizeof(name));

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((NULL == pNameOfAdapter) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);

        if (pAdapterPath)
        {
            halrc = BTRCore_GetAdapterName(gBTRCoreHandle, pAdapterPath, name);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to get Adapter Name\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_GetAdapterName : Fetched Successfully\n");

            /*  Copy regardless of success or failure. */
            strncpy (pNameOfAdapter, name, (BTMGR_NAME_LEN_MAX - 1));
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char power_status)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (power_status > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_SetAdapterPower(gBTRCoreHandle, pAdapterPath, power_status);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_SetAdapterPowerStatus : Failed to set Adapter Power Status\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_SetAdapterPowerStatus : Set Successfully\n");
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_SetAdapterPowerStatus : Failed to get adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char *pPowerStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    unsigned char power_status = 0;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((NULL == pPowerStatus) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        
        if (pAdapterPath)
        {
            halrc = BTRCore_GetAdapterPower(gBTRCoreHandle, pAdapterPath, &power_status);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to get Adapter Power\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_GetAdapterName : Fetched Successfully\n");
                *pPowerStatus = power_status;
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to get adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterDiscoverable(unsigned char index_of_adapter, unsigned char discoverable, unsigned short timeout)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (discoverable > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);

        if (pAdapterPath)
        {
            if (timeout)
            {
                halrc = BTRCore_SetAdapterDiscoverableTimeout(gBTRCoreHandle, pAdapterPath, timeout);
                if (enBTRCoreSuccess != halrc)
                    BTMGRLOG_ERROR ("BTMGR_SetAdapterDiscoverable : Failed to set Adapter discovery timeout\n");
                else
                    BTMGRLOG_INFO ("BTMGR_SetAdapterDiscoverable : Set timeout Successfully\n");
            }

            /* Set the  discoverable state */
            halrc = BTRCore_SetAdapterDiscoverable(gBTRCoreHandle, pAdapterPath, discoverable);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_SetAdapterDiscoverable : Failed to set Adapter discoverable status\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_SetAdapterDiscoverable : Set discoverable status Successfully\n");
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_SetAdapterDiscoverable : Failed to get adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_IsAdapterDiscoverable(unsigned char index_of_adapter, unsigned char *pDiscoverable)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    unsigned char discoverable = 0;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((NULL == pDiscoverable) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        
        if (pAdapterPath)
        {
            halrc = BTRCore_GetAdapterDiscoverableStatus(gBTRCoreHandle, pAdapterPath, &discoverable);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_IsAdapterDiscoverable : Failed to get Adapter Status\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_IsAdapterDiscoverable : Fetched Successfully\n");
                *pDiscoverable = discoverable;
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_IsAdapterDiscoverable : Failed to get adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_StartDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    if (isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is already in progress\n");
    }
    else if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if(index_of_adapter > getAdaptherCount())
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        /* Populate the currently Paired Devices. This will be used only for the callback DS update */
        BTMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);

        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_StartDeviceDiscovery(gBTRCoreHandle, pAdapterPath);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_StartDeviceDiscovery : Failed to start discovery\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_StartDeviceDiscovery : Discovery started Successfully\n");
                set_discovery_status(1);
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_StartDeviceDiscovery : Failed to get adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StopDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    if (!isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is not running now\n");
    }
    else if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if(index_of_adapter > getAdaptherCount())
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_StopDeviceDiscovery(gBTRCoreHandle, pAdapterPath);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_StopDeviceDiscovery : Failed to stop discovery\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_StopDeviceDiscovery : Discovery Stopped Successfully\n");
                set_discovery_status(0);
                if(m_eventCallbackFunction)
                {
                    BTMGR_EventMessage_t newEvent;
                    memset (&newEvent, 0, sizeof(newEvent));

                    newEvent.m_adapterIndex = index_of_adapter;
                    newEvent.m_eventType = BTMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
                    newEvent.m_numOfDevices = BTMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

                    /*  Post a callback */
                    m_eventCallbackFunction (newEvent);
                }
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_StopDeviceDiscovery : Failed to get adapter path\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetDiscoveredDevices(unsigned char index_of_adapter, BTMGR_DiscoveredDevicesList_t *pDiscoveredDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCoreScannedDevicesCount listOfDevices;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (!pDiscoveredDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        memset (&listOfDevices, 0, sizeof(listOfDevices));
        halrc = BTRCore_GetListOfScannedDevices(gBTRCoreHandle, &listOfDevices);
        if (enBTRCoreSuccess != halrc)
        {
            BTMGRLOG_ERROR ("BTMGR_GetDiscoveredDevices : Failed to get list of discovered devices\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
        else
        {
            /* Reset the values to 0 */
            memset (pDiscoveredDevices, 0, sizeof(BTMGR_DiscoveredDevicesList_t));
            if (listOfDevices.numberOfDevices)
            {
                int i = 0;
                BTMGR_DiscoveredDevices_t *ptr = NULL;
                pDiscoveredDevices->m_numOfDevices = listOfDevices.numberOfDevices;
                for (i = 0; i < listOfDevices.numberOfDevices; i++)
                {
                    ptr = &pDiscoveredDevices->m_deviceProperty[i];
                    strncpy(ptr->m_name, listOfDevices.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
                    strncpy(ptr->m_deviceAddress, listOfDevices.devices[i].device_address, (BTMGR_NAME_LEN_MAX - 1));
                    ptr->m_signalLevel = listOfDevices.devices[i].RSSI;
                    ptr->m_rssi = btmgr_MapSignalStrengthToRSSI (listOfDevices.devices[i].RSSI);
                    ptr->m_deviceHandle = listOfDevices.devices[i].deviceId;
                    ptr->m_deviceType = btmgr_MapDeviceTypeFromCore(listOfDevices.devices[i].device_type);
                    ptr->m_isPairedDevice = btmgr_IsThisPairedDevice (listOfDevices.devices[i].deviceId);

                    if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle == ptr->m_deviceHandle))
                        ptr->m_isConnected = 1;
                }
            }
            else
                BTMGRLOG_WARN("No Device is found yet\n");

            /*  Success */
            BTMGRLOG_INFO ("BTMGR_GetDiscoveredDevices : Successful\n");
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_PairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    if (isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
        BTMGR_StopDeviceDiscovery(index_of_adapter);
    }

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        /* Update the Paired Device List */
        BTMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);

        if (0 == btmgr_IsThisPairedDevice(handle))
        {
            halrc = BTRCore_PairDevice (gBTRCoreHandle, handle);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_PairDevice : Failed to pair a device\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_PairDevice : Paired Successfully\n");
                if(m_eventCallbackFunction)
                {
                    BTMGR_EventMessage_t newEvent;
                    memset (&newEvent, 0, sizeof(newEvent));

                    newEvent.m_adapterIndex = index_of_adapter;
                    newEvent.m_eventType = BTMGR_EVENT_DEVICE_PAIRING_COMPLETE;
                    newEvent.m_numOfDevices = BTMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

                    /*  Post a callback */
                    m_eventCallbackFunction (newEvent);
                }
            }
        }
        else
            BTMGRLOG_INFO ("BTMGR_PairDevice : Already a Paired Device; Nothing Done...\n");
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetPairedDevices(unsigned char index_of_adapter, BTMGR_PairedDevicesList_t *pPairedDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfDevices;
    stBTRCoreSupportedServiceList serviceList;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (!pPairedDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        memset (&listOfDevices, 0, sizeof(listOfDevices));

        halrc = BTRCore_GetListOfPairedDevices(gBTRCoreHandle, &listOfDevices);
        if (enBTRCoreSuccess != halrc)
        {
            BTMGRLOG_ERROR ("BTMGR_GetPairedDevices : Failed to get list of paired devices\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
        else
        {
            /* Reset the values to 0 */
            memset (pPairedDevices, 0, sizeof(BTMGR_PairedDevicesList_t));
            if (listOfDevices.numberOfDevices)
            {
                int i = 0;
                int j = 0;
                BTMGR_PairedDevices_t *ptr = NULL;
                pPairedDevices->m_numOfDevices = listOfDevices.numberOfDevices;
                for (i = 0; i < listOfDevices.numberOfDevices; i++)
                {
                    memset (&serviceList, 0, sizeof(stBTRCoreSupportedServiceList));
                    ptr = &pPairedDevices->m_deviceProperty[i];
                    strncpy(ptr->m_name, listOfDevices.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
                    strncpy(ptr->m_deviceAddress, listOfDevices.devices[i].device_address, (BTMGR_NAME_LEN_MAX - 1));
                    ptr->m_deviceHandle = listOfDevices.devices[i].deviceId;
                    ptr->m_deviceType = btmgr_MapDeviceTypeFromCore (listOfDevices.devices[i].device_type);

                    halrc = BTRCore_GetSupportedServices (gBTRCoreHandle, ptr->m_deviceHandle, &serviceList);
                    if (enBTRCoreSuccess == halrc)
                    {
                        ptr->m_serviceInfo.m_numOfService = serviceList.numberOfService;
                        for (j = 0; j < serviceList.numberOfService; j++)
                        {
                            BTMGRLOG_INFO ("%s : Profile ID = %u; Profile Name = %s \n", __FUNCTION__, serviceList.profile[j].uuid_value, serviceList.profile[j].profile_name);
                            ptr->m_serviceInfo.m_profileInfo[j].m_uuid = serviceList.profile[j].uuid_value;
                            strcpy (ptr->m_serviceInfo.m_profileInfo[j].m_profile, serviceList.profile[j].profile_name);
                        }
                    }
                    else
                    {
                        BTMGRLOG_ERROR ("%s : Failed to Get the Supported Services\n", __FUNCTION__);
                    }


                    if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle == ptr->m_deviceHandle))
                        ptr->m_isConnected = 1;
                }
            }
            else
                BTMGRLOG_WARN("No Device is paired yet\n");

            /*  Success */
            BTMGRLOG_INFO ("BTMGR_GetPairedDevices : Successful\n");
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_UnpairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
        BTMGR_StopDeviceDiscovery(index_of_adapter);
    }

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if ((0 == handle) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        /* Get the latest Paired Device List; This is added as the developer could add a device thro test application and try unpair thro' UI */
        BTMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);

        if (1 == btmgr_IsThisPairedDevice(handle))
        {
            halrc = BTRCore_UnPairDevice(gBTRCoreHandle, handle);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_UnpairDevice : Failed to unpair\n");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_UnpairDevice : Unpaired Successfully\n");
                if(m_eventCallbackFunction)
                {
                    BTMGR_EventMessage_t newEvent;
                    memset (&newEvent, 0, sizeof(newEvent));

                    newEvent.m_adapterIndex = index_of_adapter;
                    newEvent.m_eventType = BTMGR_EVENT_DEVICE_UNPAIRING_COMPLETE;
                    newEvent.m_numOfDevices = BTMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

                    /*  Post a callback */
                    m_eventCallbackFunction (newEvent);
                }

                /* Update the Paired Device List */
                BTMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_UnpairDevice : Not a Paired device...\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_ConnectToDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DeviceConnect_Type_t connectAs)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
        BTMGR_StopDeviceDiscovery(index_of_adapter);
    }

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if ((0 == handle) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        /* connectAs param is unused for now.. */
        halrc = BTRCore_ConnectDevice (gBTRCoreHandle, handle, enBTRCoreSpeakers);
        if (enBTRCoreSuccess != halrc)
        {
            BTMGRLOG_ERROR ("BTMGR_ConnectToDevice : Failed to Connect to this device\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
        else
        {
            BTMGRLOG_INFO ("BTMGR_ConnectToDevice : Connected Successfully\n");
            gIsDeviceConnected = 1;

            if(m_eventCallbackFunction)
            {
                BTMGR_EventMessage_t newEvent;
                memset (&newEvent, 0, sizeof(newEvent));

                newEvent.m_adapterIndex = index_of_adapter;
                newEvent.m_eventType = BTMGR_EVENT_DEVICE_CONNECTION_COMPLETE;
                newEvent.m_numOfDevices = BTMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

                /*  Post a callback */
                m_eventCallbackFunction (newEvent);
            }
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_DisconnectFromDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if ((0 == handle) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else if (0 == gIsDeviceConnected)
    {
        BTMGRLOG_ERROR ("%s : No Device is connected at this time\n", __FUNCTION__);
        rc = BTMGR_RESULT_GENERIC_FAILURE;
    }
    else
    {
        if (gCurStreamingDevHandle)
        {
            /* The streaming is happening; stop it */
            rc = BTMGR_StopAudioStreamingOut(index_of_adapter, gCurStreamingDevHandle);
            if (BTMGR_RESULT_SUCCESS != rc)
                BTMGRLOG_ERROR ("%s : Streamout is failed to stop\n", __FUNCTION__);
        }

        /* connectAs param is unused for now.. */
        halrc = BTRCore_DisconnectDevice (gBTRCoreHandle, handle, enBTRCoreSpeakers);
        if (enBTRCoreSuccess != halrc)
        {
            BTMGRLOG_ERROR ("BTMGR_DisconnectFromDevice : Failed to Disconnect\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
        else
        {
            BTMGRLOG_INFO ("BTMGR_DisconnectFromDevice : Disconnected  Successfully\n");
            gIsDeviceConnected = 0;

            if(m_eventCallbackFunction)
            {
                BTMGR_EventMessage_t newEvent;
                memset (&newEvent, 0, sizeof(newEvent));

                newEvent.m_adapterIndex = index_of_adapter;
                newEvent.m_eventType = BTMGR_EVENT_DEVICE_DISCONNECT_COMPLETE;
                newEvent.m_numOfDevices = BTMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

                /*  Post a callback */
                m_eventCallbackFunction (newEvent);
            }
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetConnectedDevices(unsigned char index_of_adapter, BTMGR_ConnectedDevicesList_t *pConnectedDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_DevicesProperty_t deviceProperty;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (!pConnectedDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        memset (pConnectedDevices, 0 , sizeof(BTMGR_ConnectedDevicesList_t));
        memset (&deviceProperty, 0 , sizeof(deviceProperty));
        if (gCurStreamingDevHandle)
        {
            if (BTMGR_RESULT_SUCCESS == BTMGR_GetDeviceProperties (index_of_adapter, gCurStreamingDevHandle, &deviceProperty))
            {
                pConnectedDevices->m_numOfDevices  = 1;
                pConnectedDevices->m_deviceProperty[0].m_deviceHandle  = deviceProperty.m_deviceHandle;
                pConnectedDevices->m_deviceProperty[0].m_deviceType = deviceProperty.m_deviceType;
                pConnectedDevices->m_deviceProperty[0].m_vendorID      = deviceProperty.m_vendorID;
                pConnectedDevices->m_deviceProperty[0].m_isConnected   = 1;
                memcpy (&pConnectedDevices->m_deviceProperty[0].m_serviceInfo, &deviceProperty.m_serviceInfo, sizeof (BTMGR_DeviceServiceList_t));
                strncpy (pConnectedDevices->m_deviceProperty[0].m_name, deviceProperty.m_name, (BTMGR_NAME_LEN_MAX - 1));
                strncpy (pConnectedDevices->m_deviceProperty[0].m_deviceAddress, deviceProperty.m_deviceAddress, (BTMGR_NAME_LEN_MAX - 1));
                BTMGRLOG_INFO ("%s : Successfully posted the connected device inforation\n", __FUNCTION__);
            }
            else
            {
                BTMGRLOG_ERROR ("%s : Failed to get connected device inforation\n", __FUNCTION__);
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
        }
        else if (gIsDeviceConnected != 0)
        {
            BTMGRLOG_ERROR ("%s : Seems like Device is connected but not streaming. Lost the connect info\n", __FUNCTION__);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetDeviceProperties(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DevicesProperty_t *pDeviceProperty)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfPDevices;
    stBTRCoreScannedDevicesCount listOfSDevices;
    stBTRCoreSupportedServiceList serviceList;
    unsigned char isFound = 0;
    int i = 0;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (!pDeviceProperty) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        /* Reset the values to 0 */
        memset (&serviceList, 0, sizeof(stBTRCoreSupportedServiceList));
        memset (&listOfPDevices, 0, sizeof(listOfPDevices));
        memset (&listOfSDevices, 0, sizeof(listOfSDevices));
        memset (pDeviceProperty, 0, sizeof(BTMGR_DevicesProperty_t));

        halrc = BTRCore_GetListOfPairedDevices(gBTRCoreHandle, &listOfPDevices);
        if (enBTRCoreSuccess == halrc)
        {
            if (listOfPDevices.numberOfDevices)
            {
                for (i = 0; i < listOfPDevices.numberOfDevices; i++)
                {
                    if (handle == listOfPDevices.devices[i].deviceId)
                    {
                        pDeviceProperty->m_deviceHandle = listOfPDevices.devices[i].deviceId;
                        pDeviceProperty->m_deviceType = btmgr_MapDeviceTypeFromCore(listOfPDevices.devices[i].device_type);
                        pDeviceProperty->m_vendorID = listOfPDevices.devices[i].vendor_id;
                        pDeviceProperty->m_isPaired = 1;
                        strncpy(pDeviceProperty->m_name, listOfPDevices.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
                        strncpy(pDeviceProperty->m_deviceAddress, listOfPDevices.devices[i].device_address, (BTMGR_NAME_LEN_MAX - 1));
                        if (gCurStreamingDevHandle == handle)
                            pDeviceProperty->m_isConnected = 1;

                        isFound = 1;
                        break;
                    }
                }
            }
            else
                BTMGRLOG_WARN("No Device is paired yet\n");
        }

        /* If and Only if the device found in paired device list, we can get the supported profile list */
        if (isFound)
        {
            halrc = BTRCore_GetSupportedServices (gBTRCoreHandle, handle, &serviceList);
            if (enBTRCoreSuccess == halrc)
            {
                pDeviceProperty->m_serviceInfo.m_numOfService = serviceList.numberOfService;
                for (i = 0; i < serviceList.numberOfService; i++)
                {
                    BTMGRLOG_INFO ("%s : Profile ID = %d; Profile Name = %s \n", __FUNCTION__, serviceList.profile[i].uuid_value, serviceList.profile[i].profile_name);
                    pDeviceProperty->m_serviceInfo.m_profileInfo[i].m_uuid = serviceList.profile[i].uuid_value;
                    strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[i].m_profile, serviceList.profile[i].profile_name, BTMGR_NAME_LEN_MAX);
                }
            }
            else
            {
                BTMGRLOG_ERROR ("%s : Failed to Get the Supported Services\n", __FUNCTION__);
            }
        }


        halrc = BTRCore_GetListOfScannedDevices (gBTRCoreHandle, &listOfSDevices);
        if (enBTRCoreSuccess == halrc)
        {
            if (listOfSDevices.numberOfDevices)
            {
                for (i = 0; i < listOfSDevices.numberOfDevices; i++)
                {
                    if (handle == listOfSDevices.devices[i].deviceId)
                    {
                        if (!isFound)
                        {
                            pDeviceProperty->m_deviceHandle = listOfSDevices.devices[i].deviceId;
                            pDeviceProperty->m_deviceType = btmgr_MapDeviceTypeFromCore(listOfSDevices.devices[i].device_type);
                            pDeviceProperty->m_vendorID = listOfSDevices.devices[i].vendor_id;
                            strncpy(pDeviceProperty->m_name, listOfSDevices.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
                            strncpy(pDeviceProperty->m_deviceAddress, listOfSDevices.devices[i].device_address, (BTMGR_NAME_LEN_MAX - 1));
                        }
                        pDeviceProperty->m_signalLevel = listOfSDevices.devices[i].RSSI;
                        isFound = 1;
                        break;
                    }
                }
            }
            else
                BTMGRLOG_WARN("No Device in scan list\n");
        }
        pDeviceProperty->m_rssi = btmgr_MapSignalStrengthToRSSI (pDeviceProperty->m_signalLevel);

        if (!isFound)
        {
            BTMGRLOG_ERROR ("BTMGR_GetDeviceProperties : Could not retrive info for this device\n");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_RegisterEventCallback(BTMGR_EventCallback eventCallback)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    if (!eventCallback)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        m_eventCallbackFunction = eventCallback;
        BTMGRLOG_INFO ("BTMGR_RegisterEventCallback : Success\n");
    }
    return rc;
}

BTMGR_Result_t BTMGR_DeInit(void)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else
    {
        BTRCore_DeInit(gBTRCoreHandle);
        BTMGRLOG_ERROR ("%s : BTRCore DeInited; Now will we exit the app\n", __FUNCTION__);
    }
    return rc;
}

BTMGR_Result_t BTMGR_StartAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DeviceConnect_Type_t streamOutPref)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfPDevices;
    unsigned char isFound = 0;
    int deviceFD = 0;
    int deviceReadMTU = 0;
    int deviceWriteMTU = 0;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else if (gCurStreamingDevHandle == handle)
    {
        BTMGRLOG_ERROR ("%s : Its already streaming out in this device.. Check the volume :)\n", __FUNCTION__);
    }
    else
    {
        if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle != handle))
        {
            BTMGRLOG_ERROR ("%s : Its already streaming out. lets stop this and start on other device \n", __FUNCTION__);
            rc = BTMGR_StopAudioStreamingOut(index_of_adapter, gCurStreamingDevHandle);
            if (rc != BTMGR_RESULT_SUCCESS)
            {
                BTMGRLOG_ERROR ("%s : Failed to stop streaming at the current device..\n", __FUNCTION__);
                return rc;
            }
        }

        /* Check whether the device is in the paired list */
        halrc = BTRCore_GetListOfPairedDevices(gBTRCoreHandle, &listOfPDevices);
        if (enBTRCoreSuccess == halrc)
        {
            if (listOfPDevices.numberOfDevices)
            {
                int i = 0;
                for (i = 0; i < listOfPDevices.numberOfDevices; i++)
                {
                    if (handle == listOfPDevices.devices[i].deviceId)
                    {
                        isFound = 1;
                        break;
                    }
                }
                if (!isFound)
                {
                    BTMGRLOG_ERROR ("%s : Failed to find this device in the paired devices list\n", __FUNCTION__);
                    rc = BTMGR_RESULT_GENERIC_FAILURE;
                }
                else
                {
                    /* Connect the device */
                    {
                        /* If the device is not connected, Connect to it */
                        BTMGR_ConnectToDevice(index_of_adapter, listOfPDevices.devices[i].deviceId, streamOutPref);

                        /* FIXME */
                        BTMGRLOG_WARN("%s : Sleep for few sec as alternate to async callback; temp change\n",__FUNCTION__);
                        sleep(5);
                        BTMGRLOG_WARN("%s : Slept for few sec as alternate to async callback; temp change\n",__FUNCTION__);
                    }

                    if (BTMGR_RESULT_SUCCESS == rc)
                    {
                        /* Aquire Device Data Path to start the audio casting */
                        halrc = BTRCore_AcquireDeviceDataPath (gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreSpeakers, &deviceFD, &deviceReadMTU, &deviceWriteMTU);

                        if (enBTRCoreSuccess == halrc)
                        {
                            /* Now that you got the FD & Read/Write MTU, start casting the audio */
                            if (BTMGR_RESULT_SUCCESS == btmgr_StartCastingAudio (deviceFD, deviceWriteMTU))
                            {
                                gCurStreamingDevHandle = listOfPDevices.devices[i].deviceId;
                                BTMGRLOG_WARN("%s : Streaming Started.. Enjoy the show..! :)\n", __FUNCTION__);
                            }
                            else
                            {
                                BTMGRLOG_ERROR ("%s : Failed to stream now\n", __FUNCTION__);
                                rc = BTMGR_RESULT_GENERIC_FAILURE;
                            }
                        }
                        else
                        {
                            BTMGRLOG_ERROR ("%s : Failed to get Device Data Path. So Will not be able to stream now\n", __FUNCTION__);
                            rc = BTMGR_RESULT_GENERIC_FAILURE;
                        }
                    }
                    else
                    {
                        BTMGRLOG_ERROR ("%s : Failed to connect to device and not playing\n", __FUNCTION__);
                    }
                }
            }
            else
            {
                BTMGRLOG_ERROR ("%s : No device is paired yet; Will not be able to play at this moment\n", __FUNCTION__);
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
        }
        else
        {
            BTMGRLOG_ERROR ("%s : Failed to get the paired devices list\n", __FUNCTION__);
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}


BTMGR_Result_t BTMGR_StopAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if (gCurStreamingDevHandle == handle)
    {
        btmgr_StopCastingAudio();

        BTRCore_ReleaseDeviceDataPath (gBTRCoreHandle, gCurStreamingDevHandle, enBTRCoreSpeakers);

        gCurStreamingDevHandle = 0;

        /* Just in case */
        /* FIXME */
        BTMGRLOG_WARN("%s : Sleep for few sec as alternate to async callback; temp change\n",__FUNCTION__);
        sleep(2);
        BTMGRLOG_WARN("%s : Slept for few sec as alternate to async callback; temp change\n",__FUNCTION__);

        /* We had Reset the gCurStreamingDevHandle to avoid recursion/looping; so no worries */
        rc = BTMGR_DisconnectFromDevice(index_of_adapter, handle);
    }

    return rc;
}

BTMGR_Result_t BTMGR_IsAudioStreamingOut(unsigned char index_of_adapter, unsigned char *pStreamingStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if(!pStreamingStatus)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        if (gCurStreamingDevHandle)
            *pStreamingStatus = 1;
        else
            *pStreamingStatus = 0;

        BTMGRLOG_INFO ("BTMGR_IsAudioStreamingOut: Returned status Successfully\n");
    }

    return rc;
}

BTMGR_Result_t BTMGR_SetAudioStreamingOutType(unsigned char index_of_adapter, BTMGR_StreamOut_Type_t type)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    BTMGRLOG_ERROR ("%s : Secondary audio support is not implemented yet. Always primary audio is played for now\n", __FUNCTION__);
    return rc;
}

BTMGR_Result_t BTMGR_ResetAdapter (unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited\n", __FUNCTION__);
    }
    else if (index_of_adapter > getAdaptherCount())
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid\n", __FUNCTION__);
    }
    else
    {
        BTMGRLOG_ERROR ("BTMGR_ResetAdapter: No Ops. As the Hal is not implemented yet\n");
    }

    return rc;
}

/* End of File */
