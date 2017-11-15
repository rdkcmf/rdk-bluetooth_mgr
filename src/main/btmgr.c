/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2016 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "btrCore.h"

#include "btmgr.h"
#include "btmgr_priv.h"

#include "btrMgr_Types.h"
#include "btrMgr_mediaTypes.h"
#include "btrMgr_streamOut.h"
#include "btrMgr_audioCap.h"
#include "btrMgr_streamIn.h"
#include "btrMgr_persistIface.h"


//TODO: Move to a local handle. Mutex protect all
static tBTRCoreHandle             gBTRCoreHandle             = NULL;
static BTRMgrDeviceHandle         gCurStreamingDevHandle     = 0;
static stBTRCoreAdapter           gDefaultAdapterContext;
static stBTRCoreListAdapters      gListOfAdapters;
static stBTRCoreDevMediaInfo      gstBtrCoreDevMediaInfo;
static tBTRMgrPIHdl  		  piHandle                   = NULL;
static BTRMgrDeviceHandle         gLastConnectedDevHandle    = 0;
static BTRMGR_PairedDevicesList_t gListOfPairedDevices;
static unsigned char              gIsDiscoveryInProgress     = 0;
static unsigned char              gIsDeviceConnected         = 0;
static unsigned char              gIsAgentActivated          = 0;
static unsigned char              gEventRespReceived         = 0;
static unsigned char              gAcceptConnection          = 0;
static unsigned char              gIsUserInitiated           = 0;


#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif


static BTRMGR_EventCallback m_eventCallbackFunction = NULL;

typedef struct _stBTRMgrStreamingInfo {
    tBTRMgrAcHdl    hBTRMgrAcHdl;
    tBTRMgrSoHdl    hBTRMgrSoHdl;
    tBTRMgrSoHdl    hBTRMgrSiHdl;
    unsigned long   bytesWritten;
    unsigned        samplerate;
    unsigned        channels;
    unsigned        bitsPerSample;
} stBTRMgrStreamingInfo;

static stBTRMgrStreamingInfo gstBTRMgrStreamingInfo;

/* Private Macro definitions */
#define BTRMGR_SIGNAL_POOR       (-90)
#define BTRMGR_SIGNAL_FAIR       (-70)
#define BTRMGR_SIGNAL_GOOD       (-60)

#define BTRMGR_CONNECT_RETRY_ATTEMPTS       2
#define BTRMGR_DEVCONN_CHECK_RETRY_ATTEMPTS 3


/* Static Function Prototypes */
static unsigned char btrMgr_GetAdapterCnt (void);
static const char* btrMgr_GetAdapterPath (unsigned char index_of_adapter);
static void btrMgr_SetAgentActivated (unsigned char aui8AgentActivated);
static unsigned char btrMgr_GetAgentActivated (void);
static void btrMgr_SetDiscoveryInProgress (unsigned char status);
static unsigned char btrMgr_GetDiscoveryInProgress (void);
static unsigned char btrMgr_GetDevPaired (BTRMgrDeviceHandle handle); 
static BTRMGR_DeviceType_t btrMgr_MapDeviceTypeFromCore (enBTRCoreDeviceClass device_type);
static BTRMGR_DeviceType_t btrMgr_MapSignalStrengthToRSSI (int signalStrength);
static eBTRMgrRet btrMgr_MapDevstatusInfoToEventInfo (void* p_StatusCB, BTRMGR_EventMessage_t*  newEvent, BTRMGR_Events_t type);
static BTRMGR_Result_t btrMgr_StartCastingAudio (int outFileFd, int outMTUSize);
static BTRMGR_Result_t btrMgr_StopCastingAudio (void); 
static BTRMGR_Result_t btrMgr_StartReceivingAudio (int inFileFd, int inMTUSize, unsigned int ui32InSampFreq);
static BTRMGR_Result_t btrMgr_StopReceivingAudio (void); 
static BTRMGR_Result_t btrMgr_ConnectToDevice (unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_DeviceConnect_Type_t connectAs, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx);
static BTRMGR_Result_t btrMgr_StartAudioStreamingOut (unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_DeviceConnect_Type_t streamOutPref, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx, unsigned int aui32SleepIdx);
static eBTRMgrRet btrMgr_AddPersistentEntry(unsigned char index_of_adapter, BTRMgrDeviceHandle  handle);
static eBTRMgrRet btrMgr_RemovePersistentEntry(unsigned char index_of_adapter, BTRMgrDeviceHandle  handle);

/* Callbacks Prototypes */
static eBTRMgrRet btrMgr_ACDataReadyCallback (void* apvAcDataBuf, unsigned int aui32AcDataLen, void* apvUserData);
static void btrMgr_DeviceDiscoveryCallback (stBTRCoreScannedDevice devicefound);
static int btrMgr_ConnectionInIntimationCallback (stBTRCoreConnCBInfo* apstConnCbInfo);
static int btrMgr_ConnectionInAuthenticationCallback (stBTRCoreConnCBInfo* apstConnCbInfo);
static void btrMgr_DeviceStatusCallback (stBTRCoreDevStatusCBInfo* p_StatusCB, void* apvUserData);
static void btrMgr_MediaStatusCallback (stBTRCoreMediaStatusCBInfo* mediaStatusCB, void* apvUserData);


/* Static Function Definitions */
static unsigned char
btrMgr_GetAdapterCnt (
    void
) {
    unsigned char numbers = 0;

    numbers = gListOfAdapters.number_of_adapters;

    return numbers;
}

static const char* 
btrMgr_GetAdapterPath (
    unsigned char   index_of_adapter
) {
    const char* pReturn = NULL;

    if (gListOfAdapters.number_of_adapters) {
        if ((index_of_adapter < gListOfAdapters.number_of_adapters) &&
            (index_of_adapter < BTRCORE_MAX_NUM_BT_ADAPTERS)) {
            pReturn = gListOfAdapters.adapter_path[index_of_adapter];
        }
    }

    return pReturn;
}

static void
btrMgr_SetAgentActivated (
    unsigned char aui8AgentActivated
) {
    gIsAgentActivated = aui8AgentActivated;
}

static unsigned char
btrMgr_GetAgentActivated (
    void
) {
    return gIsAgentActivated;
}

static void
btrMgr_SetDiscoveryInProgress (
    unsigned char   status
) {
    gIsDiscoveryInProgress = status;
}

static unsigned char
btrMgr_GetDiscoveryInProgress (
    void
) {
    return gIsDiscoveryInProgress;
}

static unsigned char
btrMgr_GetDevPaired (
    BTRMgrDeviceHandle   handle
) {
    int j = 0;
    for (j = 0; j < gListOfPairedDevices.m_numOfDevices; j++) {
        if (handle == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
            return 1;
        }
    }
    return 0;
}

static BTRMGR_DeviceType_t
btrMgr_MapDeviceTypeFromCore (
    enBTRCoreDeviceClass    device_type
) {
    BTRMGR_DeviceType_t type = BTRMGR_DEVICE_TYPE_UNKNOWN;

    switch (device_type) {
    case enBTRCore_DC_SmartPhone:
        type = BTRMGR_DEVICE_TYPE_SMARTPHONE;
        break;
    case enBTRCore_DC_WearableHeadset:
        type = BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET;
        break;
    case enBTRCore_DC_Handsfree:
        type = BTRMGR_DEVICE_TYPE_HANDSFREE;
        break;
    case enBTRCore_DC_Microphone:
        type = BTRMGR_DEVICE_TYPE_MICROPHONE;
        break;
    case enBTRCore_DC_Loudspeaker:
        type = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
        break;
    case enBTRCore_DC_Headphones:
        type = BTRMGR_DEVICE_TYPE_HEADPHONES;
        break;
    case enBTRCore_DC_PortableAudio:
        type = BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO;
        break;
    case enBTRCore_DC_CarAudio:
        type = BTRMGR_DEVICE_TYPE_CAR_AUDIO;
        break;
    case enBTRCore_DC_STB:
        type = BTRMGR_DEVICE_TYPE_STB;
        break;
    case enBTRCore_DC_HIFIAudioDevice:
        type = BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE;
        break;
    case enBTRCore_DC_VCR:
        type = BTRMGR_DEVICE_TYPE_VCR;
        break;
    case enBTRCore_DC_VideoCamera:
        type = BTRMGR_DEVICE_TYPE_VIDEO_CAMERA;
        break;
    case enBTRCore_DC_Camcoder:
        type = BTRMGR_DEVICE_TYPE_CAMCODER;
        break;
    case enBTRCore_DC_VideoMonitor:
        type = BTRMGR_DEVICE_TYPE_VIDEO_MONITOR;
        break;
    case enBTRCore_DC_TV:
        type = BTRMGR_DEVICE_TYPE_TV;
        break;
    case enBTRCore_DC_VideoConference:
        type = BTRMGR_DEVICE_TYPE_VIDEO_CONFERENCE;
        break;
    case enBTRCore_DC_Reserved:
    case enBTRCore_DC_Unknown:
        type = BTRMGR_DEVICE_TYPE_UNKNOWN;
        break;
    }

    return type;
}

static BTRMGR_DeviceType_t
btrMgr_MapSignalStrengthToRSSI (
    int signalStrength
) {
    BTRMGR_RSSIValue_t rssi = BTRMGR_RSSI_NONE;

    if (signalStrength >= BTRMGR_SIGNAL_GOOD)
        rssi = BTRMGR_RSSI_EXCELLENT;
    else if (signalStrength >= BTRMGR_SIGNAL_FAIR)
        rssi = BTRMGR_RSSI_GOOD;
    else if (signalStrength >= BTRMGR_SIGNAL_POOR)
        rssi = BTRMGR_RSSI_FAIR;
    else
        rssi = BTRMGR_RSSI_POOR;

    return rssi;
}

static eBTRMgrRet 
btrMgr_MapDevstatusInfoToEventInfo (
    void*                           p_StatusCB,               /* device status info                   */
    BTRMGR_EventMessage_t*          newEvent,                 /* event message                        */
    BTRMGR_Events_t                 type                      /* event type                           */
) {
    eBTRMgrRet  retResult    = eBTRMgrSuccess;
    newEvent->m_adapterIndex = 0;
    newEvent->m_eventType    = type;
    newEvent->m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;/* Application will have to get the list explicitly for list;Lets return the max value */

    if (!p_StatusCB)
        return eBTRMgrFailure;

    
    if (type == BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE) {
        newEvent->m_discoveredDevice.m_deviceHandle        = ((stBTRCoreScannedDevice*)p_StatusCB)->deviceId;
        newEvent->m_discoveredDevice.m_signalLevel         = ((stBTRCoreScannedDevice*)p_StatusCB)->RSSI;
        newEvent->m_discoveredDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreScannedDevice*)p_StatusCB)->device_type);
        newEvent->m_discoveredDevice.m_rssi                = btrMgr_MapSignalStrengthToRSSI(((stBTRCoreScannedDevice*)p_StatusCB)->RSSI);
        newEvent->m_discoveredDevice.m_isPairedDevice      = btrMgr_GetDevPaired(newEvent->m_discoveredDevice.m_deviceHandle);
        strncpy(newEvent->m_discoveredDevice.m_name, ((stBTRCoreScannedDevice*)p_StatusCB)->device_name, (BTRMGR_NAME_LEN_MAX-1));
        strncpy(newEvent->m_discoveredDevice.m_deviceAddress, ((stBTRCoreScannedDevice*)p_StatusCB)->device_address, (BTRMGR_NAME_LEN_MAX-1));
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST) {
        newEvent->m_externalDevice.m_deviceHandle        = ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.deviceId;
        newEvent->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.device_type);
        newEvent->m_externalDevice.m_vendorID            = ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.vendor_id;
        newEvent->m_externalDevice.m_isLowEnergyDevice   = 0;
        newEvent->m_externalDevice.m_externalDevicePIN   = ((stBTRCoreConnCBInfo*)p_StatusCB)->ui32devPassKey;
        strncpy(newEvent->m_externalDevice.m_name, ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.device_name, (BTRMGR_NAME_LEN_MAX - 1));
        strncpy(newEvent->m_externalDevice.m_deviceAddress, ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.device_address, (BTRMGR_NAME_LEN_MAX - 1));
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST) {
        newEvent->m_externalDevice.m_deviceHandle        = ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.deviceId;
        newEvent->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.device_type);
        newEvent->m_externalDevice.m_vendorID            = ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.vendor_id;
        newEvent->m_externalDevice.m_isLowEnergyDevice   = 0;
        strncpy(newEvent->m_externalDevice.m_name, ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.device_name, (BTRMGR_NAME_LEN_MAX - 1));
        strncpy(newEvent->m_externalDevice.m_deviceAddress, ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.device_address, (BTRMGR_NAME_LEN_MAX - 1));
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST) {
        newEvent->m_externalDevice.m_deviceHandle        = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
        newEvent->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
        newEvent->m_externalDevice.m_vendorID            = 0;
        newEvent->m_externalDevice.m_isLowEnergyDevice   = 0;
        strncpy(newEvent->m_externalDevice.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName, (BTRMGR_NAME_LEN_MAX - 1));
        strncpy(newEvent->m_externalDevice.m_deviceAddress,  "TO BE FILLED", (BTRMGR_NAME_LEN_MAX - 1));
    }
    else {
       newEvent->m_pairedDevice.m_deviceHandle            = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
       newEvent->m_pairedDevice.m_deviceType              = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
       newEvent->m_pairedDevice.m_isLastConnectedDevice   = (gLastConnectedDevHandle == newEvent->m_pairedDevice.m_deviceHandle)?1:0;
       strncpy(newEvent->m_pairedDevice.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName, (BTRMGR_NAME_LEN_MAX-1));
    }


    return retResult; 
}

static BTRMGR_Result_t
btrMgr_StartCastingAudio (
    int     outFileFd, 
    int     outMTUSize
) {
    stBTRMgrOutASettings        lstBtrMgrAcOutASettings;
    stBTRMgrInASettings         lstBtrMgrSoInASettings;
    stBTRMgrOutASettings        lstBtrMgrSoOutASettings;
    BTRMGR_Result_t              eBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    int inBytesToEncode = 3072; // Corresponds to MTU size of 895

    if (0 == gCurStreamingDevHandle) {
        /* Reset the buffer */
        memset(&gstBTRMgrStreamingInfo, 0, sizeof(gstBTRMgrStreamingInfo));

        memset(&lstBtrMgrAcOutASettings, 0, sizeof(lstBtrMgrAcOutASettings));
        memset(&lstBtrMgrSoInASettings,  0, sizeof(lstBtrMgrSoInASettings));
        memset(&lstBtrMgrSoOutASettings, 0, sizeof(lstBtrMgrSoOutASettings));

        /* Init StreamOut module - Create Pipeline */
        if (eBTRMgrSuccess != BTRMgr_SO_Init(&gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SO_Init FAILED\n");
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_AC_Init(&gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_AC_Init FAILED\n");
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }

        /* could get defaults from audio capture, but for the sample app we want to write a the wav header first*/
        gstBTRMgrStreamingInfo.bitsPerSample = 16;
        gstBTRMgrStreamingInfo.samplerate = 48000;
        gstBTRMgrStreamingInfo.channels = 2;


        lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ?
                                                                        (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));
        lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo   = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ?
                                                                        (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));
        lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) > sizeof(stBTRCoreDevMediaMpegInfo) ?
                                                                        (sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) : sizeof(stBTRCoreDevMediaMpegInfo));


        if (!(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo) || !(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo) || !(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo)) {
            BTRMGRLOG_ERROR ("MEMORY ALLOC FAILED\n");
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }


        if (eBTRMgrSuccess != BTRMgr_AC_GetDefaultSettings(gstBTRMgrStreamingInfo.hBTRMgrAcHdl, &lstBtrMgrAcOutASettings)) {
            BTRMGRLOG_ERROR("BTRMgr_AC_GetDefaultSettings FAILED\n");
        }


        lstBtrMgrSoInASettings.eBtrMgrInAType     = lstBtrMgrAcOutASettings.eBtrMgrOutAType;

        if (lstBtrMgrSoInASettings.eBtrMgrInAType == eBTRMgrATypePCM) {
            stBTRMgrPCMInfo* pstBtrMgrSoInPcmInfo   = (stBTRMgrPCMInfo*)(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo);
            stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo  = (stBTRMgrPCMInfo*)(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo);

            memcpy(pstBtrMgrSoInPcmInfo, pstBtrMgrAcOutPcmInfo, sizeof(stBTRMgrPCMInfo));
        }


        if (gstBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
            stBTRMgrSBCInfo*            pstBtrMgrSoOutSbcInfo       = ((stBTRMgrSBCInfo*)(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo));
            stBTRCoreDevMediaSbcInfo*   pstBtrCoreDevMediaSbcInfo   = ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo));

            lstBtrMgrSoOutASettings.eBtrMgrOutAType   = eBTRMgrATypeSBC;
            if (pstBtrMgrSoOutSbcInfo && pstBtrCoreDevMediaSbcInfo) {

                if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 8000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq8K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 16000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq16K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 32000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq32K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 44100) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq44_1K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 48000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq48K;
                }
                else {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreqUnknown;
                }


                switch (pstBtrCoreDevMediaSbcInfo->eDevMAChan) {
                case eBTRCoreDevMediaAChanMono:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanMono;
                    break;
                case eBTRCoreDevMediaAChanDualChannel:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanDualChannel;
                    break;
                case eBTRCoreDevMediaAChanStereo:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanStereo;
                    break;
                case eBTRCoreDevMediaAChanJointStereo:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanJStereo;
                    break;
                case eBTRCoreDevMediaAChan5_1:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan5_1;
                    break;
                case eBTRCoreDevMediaAChan7_1:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan7_1;
                    break;
                case eBTRCoreDevMediaAChanUnknown:
                default:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanUnknown;
                    break;
                }

                pstBtrMgrSoOutSbcInfo->ui8SbcAllocMethod  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcAllocMethod;
                pstBtrMgrSoOutSbcInfo->ui8SbcSubbands     = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcSubbands;
                pstBtrMgrSoOutSbcInfo->ui8SbcBlockLength  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcBlockLength;
                pstBtrMgrSoOutSbcInfo->ui8SbcMinBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMinBitpool;
                pstBtrMgrSoOutSbcInfo->ui8SbcMaxBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMaxBitpool;
                pstBtrMgrSoOutSbcInfo->ui16SbcFrameLen    = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcFrameLen;
                pstBtrMgrSoOutSbcInfo->ui16SbcBitrate     = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcBitrate;
            }
        }

        lstBtrMgrSoOutASettings.i32BtrMgrDevFd      = outFileFd;
        lstBtrMgrSoOutASettings.i32BtrMgrDevMtu     = outMTUSize;


        if (eBTRMgrSuccess != BTRMgr_SO_GetEstimatedInABufSize(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) {
            BTRMGRLOG_ERROR ("BTRMgr_SO_GetEstimatedInABufSize FAILED\n");
            lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize = inBytesToEncode;
        }
        else {
            inBytesToEncode = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;
        }


        if (eBTRMgrSuccess != BTRMgr_SO_Start(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) {
            BTRMGRLOG_ERROR ("BTRMgr_SO_Start FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBtrMgrResult == BTRMGR_RESULT_SUCCESS) {

            lstBtrMgrAcOutASettings.i32BtrMgrOutBufMaxSize = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;

            if (eBTRMgrSuccess != BTRMgr_AC_Start(gstBTRMgrStreamingInfo.hBTRMgrAcHdl,
                                                  &lstBtrMgrAcOutASettings,
                                                  btrMgr_ACDataReadyCallback,
                                                  &gstBTRMgrStreamingInfo
                                                 )) {
                BTRMGRLOG_ERROR ("BTRMgr_AC_Stop FAILED\n");
                eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
            }
        }

        if (lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo)
            free(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo);

        if (lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo)
            free(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo);

        if (lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo)
            free(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo);

    }

    return eBtrMgrResult;
}

static BTRMGR_Result_t
btrMgr_StopCastingAudio (
    void
) {
    BTRMGR_Result_t  eBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (gCurStreamingDevHandle) {

        if (eBTRMgrSuccess != BTRMgr_AC_Stop(gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_AC_Stop FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_SO_SendEOS(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SO_SendEOS FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_SO_Stop(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SO_Stop FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_AC_DeInit(gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_AC_DeInit FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_SO_DeInit(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SO_DeInit FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        gstBTRMgrStreamingInfo.hBTRMgrAcHdl = NULL;
        gstBTRMgrStreamingInfo.hBTRMgrSoHdl = NULL;
    }
    return eBtrMgrResult;
}

static BTRMGR_Result_t
btrMgr_StartReceivingAudio (
    int             inFileFd,
    int             inMTUSize, 
    unsigned int    ui32InSampFreq
) {
    BTRMGR_Result_t eBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    int             inBytesToEncode = 3072;

    if (gCurStreamingDevHandle == 0) {

        /* Init StreamIn module - Create Pipeline */
        if (eBTRMgrSuccess != BTRMgr_SI_Init(&gstBTRMgrStreamingInfo.hBTRMgrSiHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SI_Init FAILED\n");
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_SI_Start(gstBTRMgrStreamingInfo.hBTRMgrSiHdl, inBytesToEncode, inFileFd, inMTUSize, ui32InSampFreq)) {
            BTRMGRLOG_ERROR ("BTRMgr_SI_Start FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return eBtrMgrResult;
}

static BTRMGR_Result_t
btrMgr_StopReceivingAudio (
    void
) {
    BTRMGR_Result_t  eBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (gCurStreamingDevHandle) {

        if (eBTRMgrSuccess != BTRMgr_SI_SendEOS(gstBTRMgrStreamingInfo.hBTRMgrSiHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SI_SendEOS FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_SI_Stop(gstBTRMgrStreamingInfo.hBTRMgrSiHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SI_Stop FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        if (eBTRMgrSuccess != BTRMgr_SI_DeInit(gstBTRMgrStreamingInfo.hBTRMgrSiHdl)) {
            BTRMGRLOG_ERROR ("BTRMgr_SI_DeInit FAILED\n");
            eBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        gstBTRMgrStreamingInfo.hBTRMgrSiHdl = NULL;
    }

    return eBtrMgrResult;
}

static BTRMGR_Result_t
btrMgr_ConnectToDevice (
    unsigned char               index_of_adapter,
    BTRMgrDeviceHandle          handle,
    BTRMGR_DeviceConnect_Type_t connectAs,
    unsigned int                aui32ConnectRetryIdx,
    unsigned int                aui32ConfirmIdx
) {
    BTRMGR_Result_t  rc                 = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc               = enBTRCoreSuccess;
    unsigned char   ui8reActivateAgent  = 0;
    unsigned int    ui32retryIdx        = aui32ConnectRetryIdx + 1;


    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;

    }

    if ((!handle) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (btrMgr_GetDiscoveryInProgress()) {
        BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
        BTRMGR_StopDeviceDiscovery(index_of_adapter);
    }

    if (btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("De-Activate agent\n");
        if ((halrc = BTRCore_UnregisterAgent(gBTRCoreHandle)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Deactivate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(0);
            ui8reActivateAgent = 1;
        }
    }


    do {
        /* connectAs param is unused for now.. */
        halrc = BTRCore_ConnectDevice (gBTRCoreHandle, handle, enBTRCoreSpeakers);
        if (enBTRCoreSuccess != halrc) {
            BTRMGRLOG_ERROR ("Failed to Connect to this device\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            BTRMGRLOG_INFO ("Connected Successfully\n");
            rc = BTRMGR_RESULT_SUCCESS;
        }


        if (rc != BTRMGR_RESULT_GENERIC_FAILURE) {
            /* Max 20 sec timeout - Polled at 1 second interval: Confirmed 4 times */
            unsigned int ui32sleepTimeOut = 1;
            unsigned int ui32confirmIdx = aui32ConfirmIdx + 1;
            
            do {
                unsigned int ui32sleepIdx = 5;

                do {
                    sleep(ui32sleepTimeOut); 
                    halrc = BTRCore_GetDeviceConnected(gBTRCoreHandle, handle, enBTRCoreSpeakers);
                } while ((halrc != enBTRCoreSuccess) && (--ui32sleepIdx));
            } while (--ui32confirmIdx);

            if (halrc != enBTRCoreSuccess) {
                BTRMGRLOG_ERROR ("Failed to Connect to this device - Confirmed\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                BTRMGRLOG_DEBUG ("Succes Connect to this device - Confirmed\n");

                if (gLastConnectedDevHandle && gLastConnectedDevHandle != handle) {
                   BTRMGRLOG_DEBUG ("Remove persistent entry for previously connected device(%llu)\n", gLastConnectedDevHandle);
                   btrMgr_RemovePersistentEntry(index_of_adapter, gLastConnectedDevHandle);
                }

                btrMgr_AddPersistentEntry (index_of_adapter, handle);
                gIsDeviceConnected = 1;
                gLastConnectedDevHandle = handle;
            }
        }
    } while ((rc == BTRMGR_RESULT_GENERIC_FAILURE) && (--ui32retryIdx));


    if (ui8reActivateAgent) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((halrc = BTRCore_RegisterAgent(gBTRCoreHandle, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }

    return rc;
}

static BTRMGR_Result_t
btrMgr_StartAudioStreamingOut (
    unsigned char               index_of_adapter,
    BTRMgrDeviceHandle          handle,
    BTRMGR_DeviceConnect_Type_t streamOutPref,
    unsigned int                aui32ConnectRetryIdx,
    unsigned int                aui32ConfirmIdx,
    unsigned int                aui32SleepIdx
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc = enBTRCoreSuccess;
    unsigned char   isFound = 0;
    int             i = 0;
    int             deviceFD = 0;
    int             deviceReadMTU = 0;
    int             deviceWriteMTU = 0;
    unsigned int    ui32retryIdx = aui32ConnectRetryIdx + 1;
    stBTRCorePairedDevicesCount listOfPDevices;

    if (NULL == gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if ((index_of_adapter > btrMgr_GetAdapterCnt()) || (0 == handle)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }
    else if (gCurStreamingDevHandle == handle) {
        BTRMGRLOG_WARN ("Its already streaming out in this device.. Check the volume :)\n");
        return BTRMGR_RESULT_SUCCESS;
    }


    if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle != handle)) {
        BTRMGRLOG_ERROR ("Its already streaming out. lets stop this and start on other device \n");

        rc = BTRMGR_StopAudioStreamingOut(index_of_adapter, gCurStreamingDevHandle);
        if (rc != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
            return rc;
        }
    }

    /* Check whether the device is in the paired list */
    memset(&listOfPDevices, 0, sizeof(listOfPDevices));
    if (BTRCore_GetListOfPairedDevices(gBTRCoreHandle, &listOfPDevices) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get the paired devices list\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (!listOfPDevices.numberOfDevices) {
        BTRMGRLOG_ERROR ("No device is paired yet; Will not be able to play at this moment\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    for (i = 0; i < listOfPDevices.numberOfDevices; i++) {
        if (handle == listOfPDevices.devices[i].deviceId) {
            isFound = 1;
            break;
        }
    }

    if (!isFound) {
        BTRMGRLOG_ERROR ("Failed to find this device in the paired devices list\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (aui32ConnectRetryIdx) {
        unsigned int ui32sleepTimeOut = 2;
        unsigned int ui32confirmIdx = aui32ConfirmIdx + 1;

        do {
            unsigned int ui32sleepIdx = aui32SleepIdx + 1;
            do {
                sleep(ui32sleepTimeOut);
                halrc = BTRCore_IsDeviceConnectable(gBTRCoreHandle, listOfPDevices.devices[i].deviceId);
            } while ((halrc != enBTRCoreSuccess) && (--ui32sleepIdx));
        } while ((halrc != enBTRCoreSuccess) && (--ui32confirmIdx));

        if (halrc != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Device Not Connectable\n");
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }


    do {
        /* Connect the device  - If the device is not connected, Connect to it */
        if (aui32ConnectRetryIdx) {
            rc = btrMgr_ConnectToDevice(index_of_adapter, listOfPDevices.devices[i].deviceId, streamOutPref, BTRMGR_CONNECT_RETRY_ATTEMPTS, BTRMGR_DEVCONN_CHECK_RETRY_ATTEMPTS);
        }
        else {
            rc = btrMgr_ConnectToDevice(index_of_adapter, listOfPDevices.devices[i].deviceId, streamOutPref, 0, 1);
        }

        if (BTRMGR_RESULT_SUCCESS == rc) {
            if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
                free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
                gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
            }

            gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaSbcInfo) > sizeof(stBTRCoreDevMediaMpegInfo)) ? sizeof(stBTRCoreDevMediaSbcInfo) : sizeof(stBTRCoreDevMediaMpegInfo));

            halrc = BTRCore_GetDeviceMediaInfo(gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreSpeakers, &gstBtrCoreDevMediaInfo);
            if (enBTRCoreSuccess == halrc) {
                if (gstBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
                    BTRMGRLOG_WARN("\n"
                    "Device Media Info SFreq         = %d\n"
                    "Device Media Info AChan         = %d\n"
                    "Device Media Info SbcAllocMethod= %d\n"
                    "Device Media Info SbcSubbands   = %d\n"
                    "Device Media Info SbcBlockLength= %d\n"
                    "Device Media Info SbcMinBitpool = %d\n"
                    "Device Media Info SbcMaxBitpool = %d\n" 
                    "Device Media Info SbcFrameLen   = %d\n" 
                    "Device Media Info SbcBitrate    = %d\n",
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen,
                    ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
                }
            }
            
            /* Aquire Device Data Path to start the audio casting */
            halrc = BTRCore_AcquireDeviceDataPath (gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreSpeakers, &deviceFD, &deviceReadMTU, &deviceWriteMTU);

            if (enBTRCoreSuccess == halrc) {
                /* Now that you got the FD & Read/Write MTU, start casting the audio */
                if (BTRMGR_RESULT_SUCCESS == btrMgr_StartCastingAudio(deviceFD, deviceWriteMTU)) {
                    gCurStreamingDevHandle = listOfPDevices.devices[i].deviceId;
                    BTRMGRLOG_INFO("Streaming Started.. Enjoy the show..! :)\n");
                }
                else {
                    BTRMGRLOG_ERROR ("Failed to stream now\n");
                    rc = BTRMGR_RESULT_GENERIC_FAILURE;
                }
            }
            else {
                BTRMGRLOG_ERROR ("Failed to get Device Data Path. So Will not be able to stream now\n");
                halrc = BTRCore_DisconnectDevice (gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreSpeakers);
                if (enBTRCoreSuccess == halrc) {
                    /* Max 4 sec timeout - Polled at 1 second interval: Confirmed 2 times */
                    unsigned int ui32sleepTimeOut = 1;
                    unsigned int ui32confirmIdx = aui32ConfirmIdx + 1;
                    
                    do {
                        unsigned int ui32sleepIdx = aui32SleepIdx + 1;

                        do {
                            sleep(ui32sleepTimeOut);
                            halrc = BTRCore_GetDeviceDisconnected(gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreSpeakers);
                        } while ((halrc != enBTRCoreSuccess) && (--ui32sleepIdx));
                    } while (--ui32confirmIdx);

                    if (halrc != enBTRCoreSuccess) {
                        BTRMGRLOG_ERROR ("Failed to Disconnect from this device - Confirmed\n");
                        rc = BTRMGR_RESULT_GENERIC_FAILURE;
                    }
                    else
                        BTRMGRLOG_DEBUG ("Success Disconnect from this device - Confirmed\n");
                }

                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
        }
        else {
            BTRMGRLOG_ERROR ("Failed to connect to device and not playing\n");
        }

    } while ((rc == BTRMGR_RESULT_GENERIC_FAILURE) && (--ui32retryIdx));


    if ((rc == BTRMGR_RESULT_GENERIC_FAILURE) && m_eventCallbackFunction) {
        BTRMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));

        newEvent.m_adapterIndex = index_of_adapter;
        strncpy(newEvent.m_pairedDevice.m_name, listOfPDevices.devices[i].device_name, BTRMGR_NAME_LEN_MAX);
        newEvent.m_eventType    = BTRMGR_EVENT_DEVICE_CONNECTION_FAILED;
        newEvent.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */
        /*  Post a callback */
        m_eventCallbackFunction (newEvent);
    }

    return rc;
}

static eBTRMgrRet
btrMgr_AddPersistentEntry (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle  handle
) {
   char lui8adapterAddr[BD_NAME_LEN] = {'\0'};
   eBTRMgrRet retStatus  = eBTRMgrFailure;
 
   BTRCore_GetAdapterAddr(gBTRCoreHandle, index_of_adapter, lui8adapterAddr);

   // Device connected add data from json file
   BTRMGR_Profile_t btPtofile;
   strncpy(btPtofile.adapterId,lui8adapterAddr,BTRMGR_NAME_LEN_MAX);
   strncpy(btPtofile.profileId,BTRMGR_A2DP_SINK_PROFILE_ID,BTRMGR_NAME_LEN_MAX);
   btPtofile.deviceId = handle;
   btPtofile.isConnect = 1;

   retStatus = BTRMgr_PI_AddProfile(piHandle,btPtofile);

   if(eBTRMgrSuccess == retStatus) {
      BTRMGRLOG_INFO ("Persistent File updated successfully\n");
   }
   else {
      BTRMGRLOG_ERROR ("Persistent File update failed \n");
   }

  return retStatus;
}

static eBTRMgrRet
btrMgr_RemovePersistentEntry (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle  handle
) {
   char lui8adapterAddr[BD_NAME_LEN] = {'\0'};
   eBTRMgrRet retStatus = eBTRMgrFailure;

   BTRCore_GetAdapterAddr(gBTRCoreHandle, index_of_adapter, lui8adapterAddr);

   // Device disconnected remove data from json file
   BTRMGR_Profile_t btPtofile;
   strncpy(btPtofile.adapterId,lui8adapterAddr,BTRMGR_NAME_LEN_MAX);
   strncpy(btPtofile.profileId,BTRMGR_A2DP_SINK_PROFILE_ID,BTRMGR_NAME_LEN_MAX);
   btPtofile.deviceId = handle;
   btPtofile.isConnect = 1;

   retStatus = BTRMgr_PI_RemoveProfile(piHandle,btPtofile);

   if(eBTRMgrSuccess == retStatus) {
       BTRMGRLOG_INFO ("Persistent File updated successfully\n");
   }
   else {
       BTRMGRLOG_ERROR ("Persistent File update failed \n");
   }

   return retStatus;
}


/* Public Functions */
BTRMGR_Result_t
BTRMGR_Init (
    void
) {
    BTRMGR_Result_t rc          = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet 	halrc       = enBTRCoreSuccess;
    eBTRMgrRet      piInitRet   = eBTRMgrFailure;
    char            lpcBtVersion[BTRCORE_STRINGS_MAX_LEN] = {'\0'};


    if (NULL != gBTRCoreHandle) {
        BTRMGRLOG_WARN("Already Inited; Return Success\n");
        return rc;
    }

#ifdef RDK_LOGGER_ENABLED
    const char* pDebugConfig = NULL;
    const char* BTRMGR_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
    const char* BTRMGR_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

    /* Init the logger */
    if (access(BTRMGR_DEBUG_OVERRIDE_PATH, F_OK) != -1) {
        pDebugConfig = BTRMGR_DEBUG_OVERRIDE_PATH;
    }
    else {
        pDebugConfig = BTRMGR_DEBUG_ACTUAL_PATH;
    }

    if (0 == rdk_logger_init(pDebugConfig)) {
        b_rdk_logger_enabled = 1;
    }
#endif

    /* Initialze all the database */
    memset (&gDefaultAdapterContext, 0, sizeof(gDefaultAdapterContext));
    memset (&gListOfAdapters, 0, sizeof(gListOfAdapters));
    memset(&gstBTRMgrStreamingInfo, 0, sizeof(gstBTRMgrStreamingInfo));
    memset (&gListOfPairedDevices, 0, sizeof(gListOfPairedDevices));
    memset(&gstBtrCoreDevMediaInfo, 0, sizeof(gstBtrCoreDevMediaInfo));
    gIsDiscoveryInProgress = 0;


    /* Init the mutex */

    /* Call the Core/HAL init */
    halrc = BTRCore_Init(&gBTRCoreHandle);
    if ((NULL == gBTRCoreHandle) || (enBTRCoreSuccess != halrc)) {
        BTRMGRLOG_ERROR ("Could not initialize BTRCore/HAL module\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }



    if (enBTRCoreSuccess != BTRCore_GetVersionInfo(gBTRCoreHandle, lpcBtVersion)) {
        BTRMGRLOG_ERROR ("BTR Bluetooth Version: FAILED\n");
    }
    BTRMGRLOG_INFO("BTR Bluetooth Version: %s\n", lpcBtVersion);

    if (enBTRCoreSuccess != BTRCore_GetListOfAdapters (gBTRCoreHandle, &gListOfAdapters)) {
        BTRMGRLOG_ERROR ("Failed to get the total number of Adapters present\n"); /* Not a Error case anyway */
    }
    BTRMGRLOG_INFO ("Number of Adapters found are = %u\n", gListOfAdapters.number_of_adapters);

    if (0 == gListOfAdapters.number_of_adapters) {
        BTRMGRLOG_WARN("Bluetooth adapter NOT Found! Expected to be connected to make use of this module; Not considered as failure\n");
    }
    else {
        /* you have atlesat one Bluetooth adapter. Now get the Default Adapter path for future usages; */
        gDefaultAdapterContext.bFirstAvailable = 1; /* This is unused by core now but lets fill it */
        if (enBTRCoreSuccess == BTRCore_GetAdapter(gBTRCoreHandle, &gDefaultAdapterContext)) {
            BTRMGRLOG_DEBUG ("Aquired default Adapter; Path is %s\n", gDefaultAdapterContext.pcAdapterPath);
        }

        /* TODO: Handling multiple Adapters */
        if (gListOfAdapters.number_of_adapters > 1) {
            BTRMGRLOG_WARN("Number of Bluetooth Adapters Found : %u !! Lets handle it properly\n", gListOfAdapters.number_of_adapters);
        }
    }

    /* Register for callback to get the status of connected Devices */
    BTRCore_RegisterStatusCallback(gBTRCoreHandle, btrMgr_DeviceStatusCallback, NULL);

    /* Register for callback to process incoming media events */
    BTRCore_RegisterMediaStatusCallback(gBTRCoreHandle, btrMgr_MediaStatusCallback, NULL);

    /* Register for callback to get the Discovered Devices */
    BTRCore_RegisterDiscoveryCallback(gBTRCoreHandle, btrMgr_DeviceDiscoveryCallback, NULL);

    /* Register for callback to process incoming pairing requests */
    BTRCore_RegisterConnectionIntimationCallback(gBTRCoreHandle, btrMgr_ConnectionInIntimationCallback, NULL);

    /* Register for callback to process incoming connection requests */
    BTRCore_RegisterConnectionAuthenticationCallback(gBTRCoreHandle, btrMgr_ConnectionInAuthenticationCallback, NULL);
    // Enabling Agent at Init() leads DELIA-24185. 
    // TODO:To have a better logic to keep Agent alive always.
    /* Activate Agent on Init */
#if 0
    if (!btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((halrc = BTRCore_RegisterAgent(gBTRCoreHandle, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }
#endif

    // Init Persistent handles
    piInitRet = BTRMgr_PI_Init(&piHandle);
    if(piInitRet == eBTRMgrSuccess) {
        char lui8adapterAddr[BD_NAME_LEN] = {'\0'};
        int  pindex = 0;
        int  dindex = 0;
        int profileRetStatus;
        int numOfProfiles = 0;
        int deviceCount = 0;
        int isConnected = 0;

        BTRMGR_PersistentData_t persistentData;
        BTRMgrDeviceHandle lDeviceHandle;

        profileRetStatus = BTRMgr_PI_GetAllProfiles(piHandle,&persistentData);
        BTRCore_GetAdapterAddr(gBTRCoreHandle, 0, lui8adapterAddr);

        if (profileRetStatus != eBTRMgrFailure) {
            BTRMGRLOG_INFO ("Successfully get all profiles\n");

            if(strcmp(persistentData.adapterId,lui8adapterAddr) == 0) {
                BTRMGRLOG_DEBUG ("Adapter matches = %s\n",lui8adapterAddr);
                numOfProfiles = persistentData.numOfProfiles;
                BTRMGRLOG_DEBUG ("Number of Profiles = %d\n",numOfProfiles);

                for(pindex = 0; pindex < numOfProfiles; pindex++) {
                    deviceCount = persistentData.profileList[pindex].numOfDevices;

                    for(dindex = 0; dindex < deviceCount ; dindex++) {
                        lDeviceHandle = persistentData.profileList[pindex].deviceList[dindex].deviceId;
                        isConnected = persistentData.profileList[pindex].deviceList[dindex].isConnected;

                        if(isConnected) {
                            if(strcmp(persistentData.profileList[pindex].profileId,BTRMGR_A2DP_SINK_PROFILE_ID) == 0) {
                                BTRMGRLOG_INFO ("Streaming to Device  = %lld\n",lDeviceHandle);
                                btrMgr_StartAudioStreamingOut(0, lDeviceHandle, BTRMGR_DEVICE_TYPE_AUDIOSINK, 1, 1, 1);
                            }
                        }
                    }
                }
            }
        }
    }
    else {
        BTRMGRLOG_ERROR ("Could not initialize PI module\n");
    }
    
    return rc;
}

BTRMGR_Result_t
BTRMGR_GetNumberOfAdapters (
    unsigned char*  pNumOfAdapters
) {
    BTRMGR_Result_t          rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet            halrc = enBTRCoreSuccess;
    stBTRCoreListAdapters   listOfAdapters;

    memset (&listOfAdapters, 0, sizeof(listOfAdapters));
    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if (NULL == pNumOfAdapters) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        halrc = BTRCore_GetListOfAdapters (gBTRCoreHandle, &listOfAdapters);
        if (enBTRCoreSuccess == halrc) {
            *pNumOfAdapters = listOfAdapters.number_of_adapters;
            /* Copy to our backup */
            if (listOfAdapters.number_of_adapters != gListOfAdapters.number_of_adapters)
                memcpy (&gListOfAdapters, &listOfAdapters, sizeof (stBTRCoreListAdapters));

            BTRMGRLOG_DEBUG ("Available Adapters = %d\n", listOfAdapters.number_of_adapters);
        }
        else {
            BTRMGRLOG_ERROR ("Could not find Adapters\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAdapterName (
    unsigned char   index_of_adapter,
    const char*     pNameOfAdapter
) {
    BTRMGR_Result_t  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc = enBTRCoreSuccess;
    char            name[BTRMGR_NAME_LEN_MAX];

    memset(name, '\0', sizeof(name));

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((NULL == pNameOfAdapter) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        const char* pAdapterPath = btrMgr_GetAdapterPath (index_of_adapter);
        if (pAdapterPath) {
            strncpy (name, pNameOfAdapter, (BTRMGR_NAME_LEN_MAX - 1));
            halrc = BTRCore_SetAdapterName(gBTRCoreHandle, pAdapterPath, name);
            if (enBTRCoreSuccess != halrc) {
                BTRMGRLOG_ERROR ("Failed to set Adapter Name\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                BTRMGRLOG_INFO ("Set Successfully\n");
            }
        }
        else {
            BTRMGRLOG_ERROR ("Failed to adapter path\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetAdapterName (
    unsigned char   index_of_adapter,
    char*           pNameOfAdapter
) {
    BTRMGR_Result_t  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc = enBTRCoreSuccess;
    char            name[BTRMGR_NAME_LEN_MAX];

    memset(name, '\0', sizeof(name));

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((NULL == pNameOfAdapter) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        const char* pAdapterPath = btrMgr_GetAdapterPath (index_of_adapter);

        if (pAdapterPath) {
            halrc = BTRCore_GetAdapterName(gBTRCoreHandle, pAdapterPath, name);
            if (enBTRCoreSuccess != halrc) {
                BTRMGRLOG_ERROR ("Failed to get Adapter Name\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                BTRMGRLOG_INFO ("Fetched Successfully\n");
            }

            /*  Copy regardless of success or failure. */
            strncpy (pNameOfAdapter, name, (BTRMGR_NAME_LEN_MAX - 1));
        }
        else {
            BTRMGRLOG_ERROR ("Failed to adapter path\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAdapterPowerStatus (
    unsigned char   index_of_adapter,
    unsigned char   power_status
) {
    BTRMGR_Result_t  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((index_of_adapter > btrMgr_GetAdapterCnt()) || (power_status > 1)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        /* Check whether the requested device is connected n playing. */
        if ((gCurStreamingDevHandle) && (power_status == 0)) {
            /* This will internall stops the playback as well as disconnects. */
            rc = BTRMGR_StopAudioStreamingOut(index_of_adapter, gCurStreamingDevHandle);
            if (BTRMGR_RESULT_SUCCESS != rc) {
                BTRMGRLOG_ERROR ("BTRMGR_SetAdapterPowerStatus : This device is being Connected n Playing. Failed to stop Playback. Going Ahead to power off Adapter.\n");
            }
        }

        const char* pAdapterPath = btrMgr_GetAdapterPath (index_of_adapter);
        if (pAdapterPath) {
            halrc = BTRCore_SetAdapterPower(gBTRCoreHandle, pAdapterPath, power_status);
            if (enBTRCoreSuccess != halrc) {
                BTRMGRLOG_ERROR ("BTRMGR_SetAdapterPowerStatus : Failed to set Adapter Power Status\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                BTRMGRLOG_INFO ("BTRMGR_SetAdapterPowerStatus : Set Successfully\n");
            }
        }
        else {
            BTRMGRLOG_ERROR ("BTRMGR_SetAdapterPowerStatus : Failed to get adapter path\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetAdapterPowerStatus (
    unsigned char   index_of_adapter,
    unsigned char*  pPowerStatus
) {
    BTRMGR_Result_t  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc = enBTRCoreSuccess;
    unsigned char   power_status = 0;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((NULL == pPowerStatus) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        const char* pAdapterPath = btrMgr_GetAdapterPath (index_of_adapter);
        
        if (pAdapterPath) {
            halrc = BTRCore_GetAdapterPower(gBTRCoreHandle, pAdapterPath, &power_status);
            if (enBTRCoreSuccess != halrc) {
                BTRMGRLOG_ERROR ("Failed to get Adapter Power\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                BTRMGRLOG_INFO ("Fetched Successfully\n");
                *pPowerStatus = power_status;
            }
        }
        else {
            BTRMGRLOG_ERROR ("Failed to get adapter path\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAdapterDiscoverable (
    unsigned char   index_of_adapter,
    unsigned char   discoverable,
    unsigned short  timeout
) {
    BTRMGR_Result_t rc          = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc       = enBTRCoreSuccess;
    const char*     pAdapterPath= NULL;

    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;

    }

    if ((index_of_adapter > btrMgr_GetAdapterCnt()) || (discoverable > 1)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    pAdapterPath = btrMgr_GetAdapterPath(index_of_adapter);
    if (!pAdapterPath) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (timeout) {
        halrc = BTRCore_SetAdapterDiscoverableTimeout(gBTRCoreHandle, pAdapterPath, timeout);
        if (halrc != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to set Adapter discovery timeout\n");
        }
        else {
            BTRMGRLOG_INFO ("Set timeout Successfully\n");
        }
    }

    /* Set the  discoverable state */
    if ((halrc = BTRCore_SetAdapterDiscoverable(gBTRCoreHandle, pAdapterPath, discoverable)) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to set Adapter discoverable status\n");
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Set discoverable status Successfully\n");
        if (discoverable) {
            if (!btrMgr_GetAgentActivated()) {
                BTRMGRLOG_INFO ("Activate agent\n");
                if ((halrc = BTRCore_RegisterAgent(gBTRCoreHandle, 1)) != enBTRCoreSuccess) {
                    BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
                    rc = BTRMGR_RESULT_GENERIC_FAILURE;
                }
                else {
                    btrMgr_SetAgentActivated(1);
                }
            }
        }
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_IsAdapterDiscoverable (
    unsigned char   index_of_adapter,
    unsigned char*  pDiscoverable
) {
    BTRMGR_Result_t  rc          = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc       = enBTRCoreSuccess;
    unsigned char   discoverable= 0;
    const char*     pAdapterPath= NULL;

    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((NULL == pDiscoverable) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    pAdapterPath = btrMgr_GetAdapterPath(index_of_adapter);
    if (!pAdapterPath) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }
    

    if ((halrc = BTRCore_GetAdapterDiscoverableStatus(gBTRCoreHandle, pAdapterPath, &discoverable)) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get Adapter Status\n");
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Fetched Successfully\n");
        *pDiscoverable = discoverable;
        if (discoverable) {
            if (!btrMgr_GetAgentActivated()) {
                BTRMGRLOG_INFO ("Activate agent\n");
                if ((halrc = BTRCore_RegisterAgent(gBTRCoreHandle, 1)) != enBTRCoreSuccess) {
                    BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
                    rc = BTRMGR_RESULT_GENERIC_FAILURE;
                }
                else {
                    btrMgr_SetAgentActivated(1);
                }
            }
        }
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_StartDeviceDiscovery (
    unsigned char   index_of_adapter
) {
    BTRMGR_Result_t  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc = enBTRCoreSuccess;

    if (btrMgr_GetDiscoveryInProgress()) {
        BTRMGRLOG_WARN("Scanning is already in progress\n");
    }
    else if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if (index_of_adapter > btrMgr_GetAdapterCnt()) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        /* Populate the currently Paired Devices. This will be used only for the callback DS update */
        BTRMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);

        const char* pAdapterPath = btrMgr_GetAdapterPath (index_of_adapter);
        if (pAdapterPath) {
            halrc = BTRCore_StartDiscovery(gBTRCoreHandle, pAdapterPath, enBTRCoreUnknown, 0);
            if (enBTRCoreSuccess != halrc) {
                BTRMGRLOG_ERROR ("Failed to start discovery\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                BTRMGRLOG_INFO ("Discovery started Successfully\n");
                btrMgr_SetDiscoveryInProgress(1);
            }
        }
        else {
            BTRMGRLOG_ERROR ("Failed to get adapter path\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_StopDeviceDiscovery (
    unsigned char   index_of_adapter
) {
    BTRMGR_Result_t  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc = enBTRCoreSuccess;

    if (!btrMgr_GetDiscoveryInProgress()) {
        BTRMGRLOG_WARN("Scanning is not running now\n");
    }
    else if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if (index_of_adapter > btrMgr_GetAdapterCnt()) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        const char* pAdapterPath = btrMgr_GetAdapterPath (index_of_adapter);
        if (pAdapterPath) {
            halrc = BTRCore_StopDiscovery(gBTRCoreHandle, pAdapterPath, enBTRCoreUnknown);
            if (enBTRCoreSuccess != halrc) {
                BTRMGRLOG_ERROR ("Failed to stop discovery\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                BTRMGRLOG_INFO ("Discovery Stopped Successfully\n");
                btrMgr_SetDiscoveryInProgress(0);

                if (m_eventCallbackFunction) {
                    BTRMGR_EventMessage_t newEvent;
                    memset (&newEvent, 0, sizeof(newEvent));

                    newEvent.m_adapterIndex = index_of_adapter;
                    newEvent.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
                    newEvent.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

                    /*  Post a callback */
                    m_eventCallbackFunction (newEvent);
                }
            }
        }
        else {
            BTRMGRLOG_ERROR ("Failed to get adapter path\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetDiscoveredDevices (
    unsigned char                   index_of_adapter,
    BTRMGR_DiscoveredDevicesList_t*  pDiscoveredDevices
) {
    BTRMGR_Result_t                  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                    halrc = enBTRCoreSuccess;
    stBTRCoreScannedDevicesCount    listOfDevices;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((index_of_adapter > btrMgr_GetAdapterCnt()) || (!pDiscoveredDevices)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        memset (&listOfDevices, 0, sizeof(listOfDevices));
        halrc = BTRCore_GetListOfScannedDevices(gBTRCoreHandle, &listOfDevices);
        if (enBTRCoreSuccess != halrc) {
            BTRMGRLOG_ERROR ("Failed to get list of discovered devices\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            /* Reset the values to 0 */
            memset (pDiscoveredDevices, 0, sizeof(BTRMGR_DiscoveredDevicesList_t));
            if (listOfDevices.numberOfDevices) {
                int i = 0;
                BTRMGR_DiscoveredDevices_t *ptr = NULL;
                pDiscoveredDevices->m_numOfDevices = listOfDevices.numberOfDevices;

                for (i = 0; i < listOfDevices.numberOfDevices; i++) {
                    ptr = &pDiscoveredDevices->m_deviceProperty[i];
                    strncpy(ptr->m_name, listOfDevices.devices[i].device_name, (BTRMGR_NAME_LEN_MAX - 1));
                    strncpy(ptr->m_deviceAddress, listOfDevices.devices[i].device_address, (BTRMGR_NAME_LEN_MAX - 1));
                    ptr->m_signalLevel = listOfDevices.devices[i].RSSI;
                    ptr->m_rssi = btrMgr_MapSignalStrengthToRSSI (listOfDevices.devices[i].RSSI);
                    ptr->m_deviceHandle = listOfDevices.devices[i].deviceId;
                    ptr->m_deviceType = btrMgr_MapDeviceTypeFromCore(listOfDevices.devices[i].device_type);
                    ptr->m_isPairedDevice = btrMgr_GetDevPaired(listOfDevices.devices[i].deviceId);

                    if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle == ptr->m_deviceHandle))
                        ptr->m_isConnected = 1;
                }
                /*  Success */
                BTRMGRLOG_INFO ("Successful\n");
            }
            else {
                BTRMGRLOG_WARN("No Device is found yet\n");
            }
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_PairDevice (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle   handle
) {
    BTRMGR_Result_t  rc  = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc   = enBTRCoreSuccess;
    BTRMGR_Events_t  lBtMgrOutEvent = -1;
    unsigned char   ui8reActivateAgent = 0;


    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((!handle) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (btrMgr_GetDiscoveryInProgress()) {
        BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
        BTRMGR_StopDeviceDiscovery(index_of_adapter);
    }

    /* Update the Paired Device List */
    BTRMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);
    if (1 == btrMgr_GetDevPaired(handle)) {
        BTRMGRLOG_INFO ("Already a Paired Device; Nothing Done...\n");
        return BTRMGR_RESULT_SUCCESS;
    }

    if (btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("De-Activate agent\n");
        if ((halrc = BTRCore_UnregisterAgent(gBTRCoreHandle)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Deactivate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(0);
            ui8reActivateAgent = 1;
        }
    }


    if (enBTRCoreSuccess != BTRCore_PairDevice(gBTRCoreHandle, handle)) {
        BTRMGRLOG_ERROR ("Failed to pair a device\n");
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        lBtMgrOutEvent = BTRMGR_EVENT_DEVICE_PAIRING_FAILED;
    }
    else {
        BTRMGRLOG_INFO ("Paired Successfully\n");
        rc = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
    }


    if (m_eventCallbackFunction) {
        BTRMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));

        newEvent.m_adapterIndex = index_of_adapter;
        newEvent.m_eventType    = lBtMgrOutEvent;
        newEvent.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

        /*  Post a callback */
        m_eventCallbackFunction (newEvent);
    }

    /* Update the Paired Device List */
    BTRMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);


    if (ui8reActivateAgent) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((halrc = BTRCore_RegisterAgent(gBTRCoreHandle, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_UnpairDevice (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle   handle
) {
    BTRMGR_Result_t  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc   = enBTRCoreSuccess;
    BTRMGR_Events_t  lBtMgrOutEvent = -1;
    unsigned char   ui8reActivateAgent = 0;


    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((!handle) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (btrMgr_GetDiscoveryInProgress()) {
        BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
        BTRMGR_StopDeviceDiscovery(index_of_adapter);
    }

    /* Get the latest Paired Device List; This is added as the developer could add a device thro test application and try unpair thro' UI */
    BTRMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);
    if (0 == btrMgr_GetDevPaired(handle)) {
        BTRMGRLOG_ERROR ("Not a Paired device...\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }
    
    if (btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("De-Activate agent\n");
        if ((halrc = BTRCore_UnregisterAgent(gBTRCoreHandle)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Deactivate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(0);
            ui8reActivateAgent = 1;
        }
    }


    /* Check whether the requested device is connected n playing. */
    if (gCurStreamingDevHandle == handle) {
        /* This will internall stops the playback as well as disconnects. */
        rc = BTRMGR_StopAudioStreamingOut(index_of_adapter, gCurStreamingDevHandle);
        if (BTRMGR_RESULT_SUCCESS != rc) {
            BTRMGRLOG_ERROR ("BTRMGR_UnpairDevice :This device is being Connected n Playing. Failed to stop Playback. Going Ahead to unpair.\n");
        }
    }


    if (enBTRCoreSuccess != BTRCore_UnPairDevice(gBTRCoreHandle, handle)) {
        BTRMGRLOG_ERROR ("Failed to unpair\n");
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        lBtMgrOutEvent = BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED;
    }
    else {
        BTRMGRLOG_INFO ("Unpaired Successfully\n");
        rc = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent = BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE;
    }


    if (m_eventCallbackFunction) {
        BTRMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));

        newEvent.m_adapterIndex = index_of_adapter;
        newEvent.m_eventType    = lBtMgrOutEvent;
        newEvent.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX; /* Application will have to get the list explicitly for list; Lets return the max value */

        /*  Post a callback */
        m_eventCallbackFunction (newEvent);
    }

    /* Update the Paired Device List */
    BTRMGR_GetPairedDevices (index_of_adapter, &gListOfPairedDevices);


    if (ui8reActivateAgent) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((halrc = BTRCore_RegisterAgent(gBTRCoreHandle, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetPairedDevices (
    unsigned char               index_of_adapter,
    BTRMGR_PairedDevicesList_t*  pPairedDevices
) {
    BTRMGR_Result_t              rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                halrc = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfDevices;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((index_of_adapter > btrMgr_GetAdapterCnt()) || (!pPairedDevices)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        memset (&listOfDevices, 0, sizeof(listOfDevices));

        halrc = BTRCore_GetListOfPairedDevices(gBTRCoreHandle, &listOfDevices);
        if (enBTRCoreSuccess != halrc) {
            BTRMGRLOG_ERROR ("Failed to get list of paired devices\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            /* Reset the values to 0 */
            memset (pPairedDevices, 0, sizeof(BTRMGR_PairedDevicesList_t));
            if (listOfDevices.numberOfDevices) {
                int i = 0;
                int j = 0;
                BTRMGR_PairedDevices_t*  ptr = NULL;

                pPairedDevices->m_numOfDevices = listOfDevices.numberOfDevices;

                for (i = 0; i < listOfDevices.numberOfDevices; i++) {
                    ptr = &pPairedDevices->m_deviceProperty[i];
                    strncpy(ptr->m_name, listOfDevices.devices[i].device_name, (BTRMGR_NAME_LEN_MAX - 1));
                    strncpy(ptr->m_deviceAddress, listOfDevices.devices[i].device_address, (BTRMGR_NAME_LEN_MAX - 1));
                    ptr->m_deviceHandle = listOfDevices.devices[i].deviceId;
                    ptr->m_deviceType = btrMgr_MapDeviceTypeFromCore (listOfDevices.devices[i].device_type);
                    ptr->m_serviceInfo.m_numOfService = listOfDevices.devices[i].device_profile.numberOfService;
                    for (j = 0; j < listOfDevices.devices[i].device_profile.numberOfService; j++) {
                        BTRMGRLOG_INFO ("Profile ID = %u; Profile Name = %s \n", listOfDevices.devices[i].device_profile.profile[j].uuid_value,
                                                                                                   listOfDevices.devices[i].device_profile.profile[j].profile_name);
                        ptr->m_serviceInfo.m_profileInfo[j].m_uuid = listOfDevices.devices[i].device_profile.profile[j].uuid_value;
                        strcpy (ptr->m_serviceInfo.m_profileInfo[j].m_profile, listOfDevices.devices[i].device_profile.profile[j].profile_name);
                    }

                    if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle == ptr->m_deviceHandle))
                        ptr->m_isConnected = 1;
                }
                /*  Success */
                BTRMGRLOG_INFO ("Successful\n");
            }
            else {
                BTRMGRLOG_WARN("No Device is paired yet\n");
            }
        }
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_ConnectToDevice (
    unsigned char               index_of_adapter,
    BTRMgrDeviceHandle          handle,
    BTRMGR_DeviceConnect_Type_t connectAs
) {
    return  btrMgr_ConnectToDevice(index_of_adapter, handle, connectAs, 0, 1);
}


BTRMGR_Result_t
BTRMGR_DisconnectFromDevice (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle   handle
) {
    BTRMGR_Result_t  rc      = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    halrc   = enBTRCoreSuccess;
    unsigned char   ui8reActivateAgent = 0;

    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((!handle) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;

    }

    if (!gIsDeviceConnected) {
        BTRMGRLOG_ERROR ("No Device is connected at this time\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (btrMgr_GetDiscoveryInProgress()) {
        BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
        BTRMGR_StopDeviceDiscovery(index_of_adapter);
    }

    if (btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("De-Activate agent\n");
        if ((halrc = BTRCore_UnregisterAgent(gBTRCoreHandle)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Deactivate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(0);
            ui8reActivateAgent = 1;
        }
    }

    if (gCurStreamingDevHandle) {
        /* The streaming is happening; stop it */
        rc = BTRMGR_StopAudioStreamingOut(index_of_adapter, gCurStreamingDevHandle);
        if (BTRMGR_RESULT_SUCCESS != rc) {
            BTRMGRLOG_ERROR ("Streamout is failed to stop\n");
        }
    }

    gIsUserInitiated = 1;
    /* connectAs param is unused for now.. */
    halrc = BTRCore_DisconnectDevice (gBTRCoreHandle, handle, enBTRCoreSpeakers);
    if (enBTRCoreSuccess != halrc) {
        BTRMGRLOG_ERROR ("Failed to Disconnect\n");
        rc = BTRMGR_RESULT_GENERIC_FAILURE;

        if (m_eventCallbackFunction) {
            BTRMGR_EventMessage_t newEvent;
            memset (&newEvent, 0, sizeof(newEvent));

            newEvent.m_adapterIndex = index_of_adapter;
            newEvent.m_eventType    = BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED;
            newEvent.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

            /*  Post a callback */
            m_eventCallbackFunction (newEvent);
        }
    }
    else {
        BTRMGRLOG_INFO ("Disconnected  Successfully\n");
    }


    if (rc != BTRMGR_RESULT_GENERIC_FAILURE) {
        /* Max 4 sec timeout - Polled at 1 second interval: Confirmed 2 times */
        unsigned int ui32sleepTimeOut = 1;
        unsigned int ui32confirmIdx = 2;
        
        do {
            unsigned int ui32sleepIdx = 2;

            do {
                sleep(ui32sleepTimeOut);
                halrc = BTRCore_GetDeviceDisconnected(gBTRCoreHandle, handle, enBTRCoreSpeakers);
            } while ((halrc != enBTRCoreSuccess) && (--ui32sleepIdx));
        } while (--ui32confirmIdx);

        if (halrc != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Disconnect from this device - Confirmed\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            BTRMGRLOG_DEBUG ("Success Disconnect from this device - Confirmed\n");
            btrMgr_RemovePersistentEntry(index_of_adapter, handle);
            gIsDeviceConnected = 0;
            gLastConnectedDevHandle = 0;
        }
    }

    if (ui8reActivateAgent) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((halrc = BTRCore_RegisterAgent(gBTRCoreHandle, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_GetConnectedDevices (
    unsigned char                   index_of_adapter,
    BTRMGR_ConnectedDevicesList_t*   pConnectedDevices
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    BTRMGR_DevicesProperty_t deviceProperty;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((index_of_adapter > btrMgr_GetAdapterCnt()) || (!pConnectedDevices)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        memset (pConnectedDevices, 0 , sizeof(BTRMGR_ConnectedDevicesList_t));
        memset (&deviceProperty, 0 , sizeof(deviceProperty));

        if (gCurStreamingDevHandle) {
            if (BTRMGR_RESULT_SUCCESS == BTRMGR_GetDeviceProperties (index_of_adapter, gCurStreamingDevHandle, &deviceProperty)) {
                pConnectedDevices->m_numOfDevices  = 1;
                pConnectedDevices->m_deviceProperty[0].m_deviceHandle  = deviceProperty.m_deviceHandle;
                pConnectedDevices->m_deviceProperty[0].m_deviceType = deviceProperty.m_deviceType;
                pConnectedDevices->m_deviceProperty[0].m_vendorID      = deviceProperty.m_vendorID;
                pConnectedDevices->m_deviceProperty[0].m_isConnected   = 1;
                memcpy (&pConnectedDevices->m_deviceProperty[0].m_serviceInfo, &deviceProperty.m_serviceInfo, sizeof (BTRMGR_DeviceServiceList_t));
                strncpy (pConnectedDevices->m_deviceProperty[0].m_name, deviceProperty.m_name, (BTRMGR_NAME_LEN_MAX - 1));
                strncpy (pConnectedDevices->m_deviceProperty[0].m_deviceAddress, deviceProperty.m_deviceAddress, (BTRMGR_NAME_LEN_MAX - 1));
                BTRMGRLOG_INFO ("Successfully posted the connected device inforation\n");
            }
            else {
                BTRMGRLOG_ERROR ("Failed to get connected device inforation\n");
                rc = BTRMGR_RESULT_GENERIC_FAILURE;
            }
        }
        else if (gIsDeviceConnected != 0) {
            BTRMGRLOG_ERROR ("Seems like Device is connected but not streaming. Lost the connect info\n");
        }
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetDeviceProperties (
    unsigned char               index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_DevicesProperty_t*    pDeviceProperty
) {
    BTRMGR_Result_t                  rc = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                    halrc = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount     listOfPDevices;
    stBTRCoreScannedDevicesCount    listOfSDevices;
    unsigned char                   isFound = 0;
    int                             i = 0;
    int                             j = 0;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if ((index_of_adapter > btrMgr_GetAdapterCnt()) || (!pDeviceProperty) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        /* Reset the values to 0 */
        memset (&listOfPDevices, 0, sizeof(listOfPDevices));
        memset (&listOfSDevices, 0, sizeof(listOfSDevices));
        memset (pDeviceProperty, 0, sizeof(BTRMGR_DevicesProperty_t));

        halrc = BTRCore_GetListOfPairedDevices(gBTRCoreHandle, &listOfPDevices);
        if (enBTRCoreSuccess == halrc) {
            if (listOfPDevices.numberOfDevices) {
                for (i = 0; i < listOfPDevices.numberOfDevices; i++) {
                    if (handle == listOfPDevices.devices[i].deviceId) {
                        pDeviceProperty->m_deviceHandle = listOfPDevices.devices[i].deviceId;
                        pDeviceProperty->m_deviceType = btrMgr_MapDeviceTypeFromCore(listOfPDevices.devices[i].device_type);
                        pDeviceProperty->m_vendorID = listOfPDevices.devices[i].vendor_id;
                        pDeviceProperty->m_isPaired = 1;
                        strncpy(pDeviceProperty->m_name, listOfPDevices.devices[i].device_name, (BTRMGR_NAME_LEN_MAX - 1));
                        strncpy(pDeviceProperty->m_deviceAddress, listOfPDevices.devices[i].device_address, (BTRMGR_NAME_LEN_MAX - 1));

                        pDeviceProperty->m_serviceInfo.m_numOfService = listOfPDevices.devices[i].device_profile.numberOfService;
                        for (j = 0; j < listOfPDevices.devices[i].device_profile.numberOfService; j++) {
                            BTRMGRLOG_INFO ("Profile ID = %d; Profile Name = %s \n", listOfPDevices.devices[i].device_profile.profile[j].uuid_value,
                                                                                                       listOfPDevices.devices[i].device_profile.profile[j].profile_name);
                            pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_uuid = listOfPDevices.devices[i].device_profile.profile[j].uuid_value;
                            strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, listOfPDevices.devices[i].device_profile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
                        }

                      if (listOfPDevices.devices[i].device_connected) {
                        pDeviceProperty->m_isConnected = 1;
                      }
                        isFound = 1;
                        break;
                    }
                }
            }
            else {
                BTRMGRLOG_WARN("No Device is paired yet\n");
            }
        }

        halrc = BTRCore_GetListOfScannedDevices (gBTRCoreHandle, &listOfSDevices);
        if (enBTRCoreSuccess == halrc) {
            if (listOfSDevices.numberOfDevices) {
                for (i = 0; i < listOfSDevices.numberOfDevices; i++) {
                    if (handle == listOfSDevices.devices[i].deviceId) {
                        if (!isFound) {
                            pDeviceProperty->m_deviceHandle = listOfSDevices.devices[i].deviceId;
                            pDeviceProperty->m_deviceType = btrMgr_MapDeviceTypeFromCore(listOfSDevices.devices[i].device_type);
                            pDeviceProperty->m_vendorID = listOfSDevices.devices[i].vendor_id;
                            strncpy(pDeviceProperty->m_name, listOfSDevices.devices[i].device_name, (BTRMGR_NAME_LEN_MAX - 1));
                            strncpy(pDeviceProperty->m_deviceAddress, listOfSDevices.devices[i].device_address, (BTRMGR_NAME_LEN_MAX - 1));

                            pDeviceProperty->m_serviceInfo.m_numOfService = listOfSDevices.devices[i].device_profile.numberOfService;
                            for (j = 0; j < listOfSDevices.devices[i].device_profile.numberOfService; j++) {
                                BTRMGRLOG_INFO ("Profile ID = %d; Profile Name = %s \n", listOfSDevices.devices[i].device_profile.profile[j].uuid_value,
                                                                                                           listOfSDevices.devices[i].device_profile.profile[j].profile_name);
                                pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_uuid = listOfSDevices.devices[i].device_profile.profile[j].uuid_value;
                                strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, listOfSDevices.devices[i].device_profile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
                            }
                        }
                        pDeviceProperty->m_signalLevel = listOfSDevices.devices[i].RSSI;
                        isFound = 1;
                        break;
                    }
                }
            }
            else {
                BTRMGRLOG_WARN("No Device in scan list\n");
            }
        }
        pDeviceProperty->m_rssi = btrMgr_MapSignalStrengthToRSSI (pDeviceProperty->m_signalLevel);

        if (!isFound) {
            BTRMGRLOG_ERROR ("Could not retrive info for this device\n");
            rc = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_StartAudioStreamingOut (
    unsigned char               index_of_adapter,
    BTRMgrDeviceHandle          handle,
    BTRMGR_DeviceConnect_Type_t streamOutPref
) { 
    return btrMgr_StartAudioStreamingOut(index_of_adapter, handle, streamOutPref, 0, 0, 0);
}

BTRMGR_Result_t
BTRMGR_StopAudioStreamingOut (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle   handle
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (gCurStreamingDevHandle == handle) {

        btrMgr_StopCastingAudio();

        if (gIsDeviceConnected) { 
           BTRCore_ReleaseDeviceDataPath (gBTRCoreHandle, gCurStreamingDevHandle, enBTRCoreSpeakers);
        }

        gCurStreamingDevHandle = 0;

        if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
            free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
            gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
        }

        /* We had Reset the gCurStreamingDevHandle to avoid recursion/looping; so no worries */
        rc = BTRMGR_DisconnectFromDevice(index_of_adapter, handle);
    }
    return rc;
}

BTRMGR_Result_t
BTRMGR_IsAudioStreamingOut (
    unsigned char   index_of_adapter,
    unsigned char*  pStreamingStatus
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if(!pStreamingStatus) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        if (gCurStreamingDevHandle)
            *pStreamingStatus = 1;
        else
            *pStreamingStatus = 0;

        BTRMGRLOG_INFO ("BTRMGR_IsAudioStreamingOut: Returned status Successfully\n");
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAudioStreamingOutType (
    unsigned char           index_of_adapter,
    BTRMGR_StreamOut_Type_t  type
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    BTRMGRLOG_ERROR ("Secondary audio support is not implemented yet. Always primary audio is played for now\n");
    return rc;
}

BTRMGR_Result_t
BTRMGR_StartAudioStreamingIn (
    unsigned char               ui8AdapterIdx,
    BTRMgrDeviceHandle          handle,
    BTRMGR_DeviceConnect_Type_t connectAs
) {
    BTRMGR_Result_t lenBtrMgrRet    = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfPDevices;
    int i = 0;
    int i32IsFound = 0;
    int i32DeviceFD = 0;
    int i32DeviceReadMTU = 0;
    int i32DeviceWriteMTU = 0;


    if (gBTRCoreHandle == NULL) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;

    }
    else if ((ui8AdapterIdx > btrMgr_GetAdapterCnt()) || (0 == handle)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }
    else if (gCurStreamingDevHandle == handle) {
        BTRMGRLOG_WARN ("Its already streaming-in in this device.. Check the volume :)\n");
        return BTRMGR_RESULT_SUCCESS;
    }

    if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle != handle)) {
        BTRMGRLOG_ERROR ("Its already streaming in. lets stop this and start on other device \n");

        lenBtrMgrRet = BTRMGR_StopAudioStreamingIn(ui8AdapterIdx, gCurStreamingDevHandle);
        if (lenBtrMgrRet != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
            return lenBtrMgrRet;
        }
    }

    /* Check whether the device is in the paired list */
    memset(&listOfPDevices, 0, sizeof(listOfPDevices));
    if (BTRCore_GetListOfPairedDevices(gBTRCoreHandle, &listOfPDevices) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get the paired devices list\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (!listOfPDevices.numberOfDevices) {
        BTRMGRLOG_ERROR ("No device is paired yet; Will not be able to play at this moment\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    for (i = 0; i < listOfPDevices.numberOfDevices; i++) {
        if (handle == listOfPDevices.devices[i].deviceId) {
            i32IsFound = 1;
            break;
        }
    }

    if (!i32IsFound) {
        BTRMGRLOG_ERROR ("Failed to find this device in the paired devices list\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
        free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
        gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
    }

    gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaSbcInfo) > sizeof(stBTRCoreDevMediaMpegInfo)) ? sizeof(stBTRCoreDevMediaSbcInfo) : sizeof(stBTRCoreDevMediaMpegInfo));

    lenBtrCoreRet = BTRCore_GetDeviceMediaInfo(gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreMobileAudioIn, &gstBtrCoreDevMediaInfo);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (gstBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
            BTRMGRLOG_WARN("\n"
            "Device Media Info SFreq         = %d\n"
            "Device Media Info AChan         = %d\n"
            "Device Media Info SbcAllocMethod= %d\n"
            "Device Media Info SbcSubbands   = %d\n"
            "Device Media Info SbcBlockLength= %d\n"
            "Device Media Info SbcMinBitpool = %d\n"
            "Device Media Info SbcMaxBitpool = %d\n" 
            "Device Media Info SbcFrameLen   = %d\n" 
            "Device Media Info SbcBitrate    = %d\n",
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen,
            ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
        }
    }

    /* Aquire Device Data Path to start audio reception */
    lenBtrCoreRet = BTRCore_AcquireDeviceDataPath (gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreMobileAudioIn, &i32DeviceFD, &i32DeviceReadMTU, &i32DeviceWriteMTU);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (BTRMGR_RESULT_SUCCESS == btrMgr_StartReceivingAudio(i32DeviceFD, i32DeviceReadMTU, ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq)) {
            gCurStreamingDevHandle = listOfPDevices.devices[i].deviceId;
            BTRMGRLOG_INFO("Audio Reception Started.. Enjoy the show..! :)\n");
        }
        else {
            BTRMGRLOG_ERROR ("Failed to read audio now\n");
            lenBtrMgrRet = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }
    else {
        BTRMGRLOG_ERROR ("Failed to get Device Data Path. So Will not be able to stream now\n");
        lenBtrMgrRet = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (BTRMGR_RESULT_SUCCESS == lenBtrMgrRet && enBTRCoreSuccess != BTRCore_ReportMediaPosition (gBTRCoreHandle, listOfPDevices.devices[i].deviceId, enBTRCoreMobileAudioIn)) {
        BTRMGRLOG_ERROR ("Failed to set BTRCore report media position info!!!");
        lenBtrMgrRet = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    
    return lenBtrMgrRet;
}

BTRMGR_Result_t
BTRMGR_StopAudioStreamingIn (
    unsigned char       ui8AdapterIdx,
    BTRMgrDeviceHandle  handle
) {
    BTRMGR_Result_t lenBtrMgrRet = BTRMGR_RESULT_SUCCESS;

    if (gCurStreamingDevHandle == handle) {
        btrMgr_StopReceivingAudio();

        BTRCore_ReleaseDeviceDataPath (gBTRCoreHandle, gCurStreamingDevHandle, enBTRCoreMobileAudioIn);

        gCurStreamingDevHandle = 0;

        if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
            free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
            gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
        }
    }

    return lenBtrMgrRet;
}

BTRMGR_Result_t
BTRMGR_IsAudioStreamingIn (
    unsigned char   ui8AdapterIdx,
    unsigned char*  pStreamingStatus
) {
    BTRMGR_Result_t lenBtrMgrRet = BTRMGR_RESULT_SUCCESS;

    if(!pStreamingStatus) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;

    }


    if (gCurStreamingDevHandle)
        *pStreamingStatus = 1;
    else
        *pStreamingStatus = 0;

    BTRMGRLOG_INFO ("BTRMGR_IsAudioStreamingIn: Returned status Successfully\n");


    return lenBtrMgrRet;
}

BTRMGR_Result_t
BTRMGR_SetEventResponse (
    unsigned char           index_of_adapter,
    BTRMGR_EventResponse_t* apstBTRMgrEvtRsp
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((!apstBTRMgrEvtRsp) || (index_of_adapter > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;

    }


    switch (apstBTRMgrEvtRsp->m_eventType) {
    case BTRMGR_EVENT_DEVICE_OUT_OF_RANGE:
        break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE:
        break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE:
        break;
    case BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE:
        break;
    case BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE:
        break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE:
        break;
    case BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE:
        break;
    case BTRMGR_EVENT_DEVICE_PAIRING_FAILED:
        break;
    case BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED:
        break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_FAILED:
        break;
    case BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED:
        break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST:
        gEventRespReceived = 1;
        if (apstBTRMgrEvtRsp->m_eventResp) {
            gAcceptConnection = 1;
        }
        break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST:
        gEventRespReceived = 1;
        if (apstBTRMgrEvtRsp->m_eventResp) {
            gAcceptConnection = 1;
        }
        break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST:
        if (apstBTRMgrEvtRsp->m_eventResp && apstBTRMgrEvtRsp->m_deviceHandle) {
            BTRMGR_DeviceConnect_Type_t stream_pref = BTRMGR_DEVICE_TYPE_AUDIOSRC;
            rc = BTRMGR_StartAudioStreamingIn(index_of_adapter, apstBTRMgrEvtRsp->m_deviceHandle, stream_pref);   
        }
        break;
    case BTRMGR_EVENT_DEVICE_FOUND:
        break;
    case BTRMGR_EVENT_MAX:
    default:
        break;
    }


    return rc;
}



BTRMGR_Result_t
BTRMGR_ResetAdapter (
    unsigned char index_of_adapter
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else if (index_of_adapter > btrMgr_GetAdapterCnt()) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        BTRMGRLOG_ERROR ("No Ops. As the Hal is not implemented yet\n");
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_DeInit (
    void
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (NULL == gBTRCoreHandle) {
        rc = BTRMGR_RESULT_INIT_FAILED;
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
    }
    else {
        BTRMgr_PI_DeInit(&piHandle);
        BTRMGRLOG_ERROR ("PI Module DeInited; Now will we exit the app\n");
        BTRCore_DeInit(gBTRCoreHandle);
        BTRMGRLOG_ERROR ("BTRCore DeInited; Now will we exit the app\n");
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_RegisterEventCallback (
    BTRMGR_EventCallback eventCallback
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (!eventCallback) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        m_eventCallbackFunction = eventCallback;
        BTRMGRLOG_INFO ("BTRMGR_RegisterEventCallback : Success\n");
    }

    return rc;
}


/*  Incoming Callbacks */
static eBTRMgrRet
btrMgr_ACDataReadyCallback (
    void*           apvAcDataBuf,
    unsigned int    aui32AcDataLen,
    void*           apvUserData
) {
    eBTRMgrRet              leBtrMgrSoRet = eBTRMgrSuccess;
    stBTRMgrStreamingInfo*  data = (stBTRMgrStreamingInfo*)apvUserData; 

    if ((leBtrMgrSoRet = BTRMgr_SO_SendBuffer(data->hBTRMgrSoHdl, apvAcDataBuf, aui32AcDataLen)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("cbBufferReady: BTRMgr_SO_SendBuffer FAILED\n");
    }

    data->bytesWritten += aui32AcDataLen;

    return leBtrMgrSoRet;
}


static void
btrMgr_DeviceDiscoveryCallback (
    stBTRCoreScannedDevice  devicefound
) {
    if (btrMgr_GetDiscoveryInProgress() && (m_eventCallbackFunction)) {
        BTRMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));

        btrMgr_MapDevstatusInfoToEventInfo ((void*)&devicefound, &newEvent, BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE);
        m_eventCallbackFunction (newEvent); /*  Post a callback */
    }

    return;
}


static int
btrMgr_ConnectionInIntimationCallback (
    stBTRCoreConnCBInfo*    apstConnCbInfo
) {
    if (apstConnCbInfo->ui32devPassKey) {
        BTRMGRLOG_ERROR ("Incoming Connection passkey = %6d\n", apstConnCbInfo->ui32devPassKey);
    }

    if (m_eventCallbackFunction) {
        BTRMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));

        btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &newEvent, BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST);  
        m_eventCallbackFunction (newEvent); /* Post a callback */
    }
    

    /* Max 15 sec timeout - Polled at 500ms second interval */
    {
        unsigned int ui32sleepIdx = 30;

        do {
            usleep(500000);
        } while ((gEventRespReceived == 0) && (--ui32sleepIdx));

        gEventRespReceived = 0;
    }

    BTRMGRLOG_ERROR ("you picked %d\n", gAcceptConnection);
    if (gAcceptConnection == 1) {
        BTRMGRLOG_ERROR ("Pin-Passkey accepted\n");
        gAcceptConnection = 0;  //reset variabhle for the next connection
        return 1;
    }
    else {
        BTRMGRLOG_ERROR ("Pin-Passkey Rejected\n");
        gAcceptConnection = 0;  //reset variabhle for the next connection
        return 0;
    }

}


static int
btrMgr_ConnectionInAuthenticationCallback (
    stBTRCoreConnCBInfo*    apstConnCbInfo
) {
    if (m_eventCallbackFunction) {
        BTRMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));

        btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &newEvent, BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);  
        m_eventCallbackFunction (newEvent);     /* Post a callback */
    }

    /* Max 15 sec timeout - Polled at 500ms second interval */
    {
        unsigned int ui32sleepIdx = 30;

        do {
            usleep(500000);
        } while ((gEventRespReceived == 0) && (--ui32sleepIdx));

        gEventRespReceived = 0;
    }

    BTRMGRLOG_ERROR ("you picked %d\n", gAcceptConnection);
    if (gAcceptConnection == 1) {
        BTRMGRLOG_ERROR ("Connection accepted\n");
        gAcceptConnection = 0;    //reset variabhle for the next connection
        return 1;
    }   
    else {
        BTRMGRLOG_ERROR ("Connection denied\n");
        gAcceptConnection = 0;    //reset variabhle for the next connection
        return 0;
    } 
}


static void
btrMgr_DeviceStatusCallback (
    stBTRCoreDevStatusCBInfo*   p_StatusCB,
    void*                       apvUserData
) {
    BTRMGR_EventMessage_t newEvent;
    memset (&newEvent, 0, sizeof(newEvent));

    BTRMGRLOG_INFO ("Received status callback\n");

    if ((p_StatusCB) && (m_eventCallbackFunction)) {

        switch (p_StatusCB->eDeviceCurrState) {

        case enBTRCoreDevStInitialized:
            break;
        case enBTRCoreDevStConnecting:
            break;
        case enBTRCoreDevStConnected:               /*  notify user device back   */
            if (enBTRCoreDevStLost == p_StatusCB->eDevicePrevState || enBTRCoreDevStPaired == p_StatusCB->eDevicePrevState) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &newEvent, BTRMGR_EVENT_DEVICE_FOUND);  
                m_eventCallbackFunction(newEvent);  /* Post a callback */
            }
            else 
            if (enBTRCoreDevStInitialized != p_StatusCB->eDevicePrevState) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &newEvent, BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE);  
                m_eventCallbackFunction(newEvent);  /* Post a callback */
            }
         break;


        case enBTRCoreDevStDisconnected:
            btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &newEvent, BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE);
            m_eventCallbackFunction (newEvent);    /* Post a callback */

            if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle == p_StatusCB->deviceId)) {
                /* update the flags as the device is NOT Connected */
                gIsDeviceConnected = 0;

                BTRMGRLOG_INFO ("newEvent.m_pairedDevice.m_deviceType = %d\n", newEvent.m_pairedDevice.m_deviceType);
                if (newEvent.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE) {
                    /* Stop the playback which already stopped internally but to free up the memory */
                    BTRMGR_StopAudioStreamingIn(0, gCurStreamingDevHandle);
                }
                else {
                    /* Stop the playback which already stopped internally but to free up the memory */
                    BTRMGR_StopAudioStreamingOut(0, gCurStreamingDevHandle);
                }
            }
            break;

        case enBTRCoreDevStLost:
            if( !gIsUserInitiated ) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &newEvent, BTRMGR_EVENT_DEVICE_OUT_OF_RANGE);
                m_eventCallbackFunction (newEvent);    /* Post a callback */

                if ((gCurStreamingDevHandle != 0) && (gCurStreamingDevHandle == p_StatusCB->deviceId)) {
                    /* update the flags as the device is NOT Connected */
                    gIsDeviceConnected = 0;

                    BTRMGRLOG_INFO ("newEvent.m_pairedDevice.m_deviceType = %d\n", newEvent.m_pairedDevice.m_deviceType);
                    if (newEvent.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE) {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        BTRMGR_StopAudioStreamingIn(0, gCurStreamingDevHandle);
                    }
                    else {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        BTRMGR_StopAudioStreamingOut (0, gCurStreamingDevHandle);
                    }
                }
            }
            gIsUserInitiated = 0;
            break;

        case enBTRCoreDevStPlaying:
            if (btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_SMARTPHONE) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &newEvent, BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST);
                m_eventCallbackFunction (newEvent);    /* Post a callback */
            }
         break;
        default:
         break;
        }
    }
    return;
}


static void
btrMgr_MediaStatusCallback (
    stBTRCoreMediaStatusCBInfo*  mediaStatusCB,
    void*                        apvUserData
) {
    BTRMGR_EventMessage_t newEvent;
    memset (&newEvent, 0, sizeof(newEvent));

    BTRMGRLOG_INFO ("Received media status callback\n");

    if ((mediaStatusCB) && (m_eventCallbackFunction)) {
       stBTRCoreMediaStreamStatus* mediaStreamStatus = mediaStatusCB->m_mediaStreamStatus;

       newEvent.m_mediaInfo.m_deviceHandle = mediaStatusCB->deviceId;
       newEvent.m_mediaInfo.m_deviceType   = mediaStatusCB->eDeviceClass;
       strncpy (newEvent.m_mediaInfo.m_name, mediaStatusCB->deviceName, BTRMGR_NAME_LEN_MAX);

       switch (mediaStreamStatus->eStreamstate) {
         
       case eBTRCoreStreamStarted:
             newEvent.m_eventType = BTRMGR_EVENT_MEDIA_STARTED;
             //memcpy(&newEvent.m_mediaInfo.m_mediaTrackInfo, &mediaStreamStatus->m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
             break;
       case eBTRCoreStreamPaused:
             newEvent.m_eventType = BTRMGR_EVENT_MEDIA_PAUSED;
             //memcpy(&newEvent.m_mediaInfo.m_mediaTrackInfo, &mediaStreamStatus->m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
             break;
       case eBTRCoreStreamStopped:
             newEvent.m_eventType = BTRMGR_EVENT_MEDIA_STOPPED;
             break;
       case eBTRCoreStreamEnded:
             newEvent.m_eventType = BTRMGR_EVENT_MEDIA_ENDED;
             break;
       case eBTRCoreStreamPosition:
             newEvent.m_eventType = BTRMGR_EVENT_MEDIA_POSITION_UPDATE;
             memcpy(&newEvent.m_mediaInfo.m_mediaPositionInfo, &mediaStreamStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
             break;
       case eBTRCoreStreamChanged:
             newEvent.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_CHANGED;
             memcpy(&newEvent.m_mediaInfo.m_mediaTrackInfo, &mediaStreamStatus->m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
             break;
       default:
             break;
       }

       m_eventCallbackFunction (newEvent);    /* Post a callback */
    }
}


BTRMGR_Result_t
BTRMGR_MediaControl (
    unsigned char                 index_of_adapter,
    BTRMgrDeviceHandle            handle,
    BTRMGR_MediaControlCommand_t  mediaCtrlCmd
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        rc = BTRMGR_RESULT_INIT_FAILED;
    }

    if (index_of_adapter > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        rc = BTRMGR_RESULT_INVALID_INPUT;
    }

    if (BTRMGR_RESULT_SUCCESS == rc && enBTRCoreSuccess != BTRCore_MediaControl(gBTRCoreHandle,
                                                                                handle,
                                                                                enBTRCoreMobileAudioIn,
                                                                                mediaCtrlCmd)) {
        BTRMGRLOG_ERROR ("Media Control Command for %llu Failed!!!\n", handle);
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    return rc;
}


BTRMGR_Result_t
BTRMGR_GetMediaTrackInfo (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_MediaTrackInfo_t      *mediaTrackInfo
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        rc = BTRMGR_RESULT_INIT_FAILED;
    }

    if (index_of_adapter > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        rc = BTRMGR_RESULT_INVALID_INPUT;
    }

    if (BTRMGR_RESULT_SUCCESS == rc && enBTRCoreSuccess != BTRCore_GetMediaTrackInfo(gBTRCoreHandle,
                                                                                     handle,
                                                                                     enBTRCoreMobileAudioIn,
                                                                                     (void*)mediaTrackInfo)) {
       BTRMGRLOG_ERROR ("Get Media Track Information for %llu Failed!!!\n", handle);
       rc = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    return rc;
}


BTRMGR_Result_t
BTRMGR_GetMediaCurrentPosition (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_MediaPositionInfo_t  *mediaPositionInfo
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (!gBTRCoreHandle) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        rc = BTRMGR_RESULT_INIT_FAILED;
    }

    if (index_of_adapter > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        rc = BTRMGR_RESULT_INVALID_INPUT;
    }

    if (BTRMGR_RESULT_SUCCESS == rc && enBTRCoreSuccess != BTRCore_GetMediaPositionInfo(gBTRCoreHandle,
                                                                                        handle,
                                                                                        enBTRCoreMobileAudioIn,
                                                                                        (void*)mediaPositionInfo)) {
       BTRMGRLOG_ERROR ("Get Media Current Position for %llu Failed!!!\n", handle);
       rc = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    return rc;
}

/* End of File */
