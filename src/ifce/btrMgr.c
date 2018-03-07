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

#include "btrCore.h"

#include "btmgr.h"
#include "btrMgr_logger.h"

#include "btrMgr_Types.h"
#include "btrMgr_mediaTypes.h"
#include "btrMgr_streamOut.h"
#include "btrMgr_audioCap.h"
#include "btrMgr_streamIn.h"
#include "btrMgr_persistIface.h"


/* Private Macro definitions */
#define BTRMGR_SIGNAL_POOR       (-90)
#define BTRMGR_SIGNAL_FAIR       (-70)
#define BTRMGR_SIGNAL_GOOD       (-60)

#define BTRMGR_CONNECT_RETRY_ATTEMPTS       2
#define BTRMGR_DEVCONN_CHECK_RETRY_ATTEMPTS 3


typedef struct _stBTRMgrStreamingInfo {
    tBTRMgrAcHdl    hBTRMgrAcHdl;
    tBTRMgrSoHdl    hBTRMgrSoHdl;
    tBTRMgrSoHdl    hBTRMgrSiHdl;
    unsigned long   bytesWritten;
    unsigned        samplerate;
    unsigned        channels;
    unsigned        bitsPerSample;
} stBTRMgrStreamingInfo;


//TODO: Move to a local handle. Mutex protect all
static tBTRCoreHandle               ghBTRCoreHdl                = NULL;
static tBTRMgrPIHdl  		        ghBTRMgrPiHdl               = NULL;
static BTRMgrDeviceHandle           ghBTRMgrDevHdlLastConnected = 0;
static BTRMgrDeviceHandle           ghBTRMgrDevHdlCurStreaming  = 0;

static stBTRCoreAdapter             gDefaultAdapterContext;
static stBTRCoreListAdapters        gListOfAdapters;
static stBTRCoreDevMediaInfo        gstBtrCoreDevMediaInfo;
static BTRMGR_PairedDevicesList_t   gListOfPairedDevices;
static stBTRMgrStreamingInfo        gstBTRMgrStreamingInfo;

static unsigned char                gIsDiscoveryInProgress      = 0;
static unsigned char                gIsDeviceConnected          = 0;
static unsigned char                gIsAgentActivated           = 0;
static unsigned char                gEventRespReceived          = 0;
static unsigned char                gAcceptConnection           = 0;
static unsigned char                gIsUserInitiated            = 0;
static unsigned char                gIsAudOutStartupInProgress  = 0;

static BTRMGR_DeviceOperationType_t gDiscoveryType          = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;

static BTRMGR_EventCallback         gfpcBBTRMgrEventOut     = NULL;

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif



/* Static Function Prototypes */
static inline unsigned char btrMgr_GetAdapterCnt (void);
static const char* btrMgr_GetAdapterPath (unsigned char aui8AdapterIdx);
static void btrMgr_SetAgentActivated (unsigned char aui8AgentActivated);
static unsigned char btrMgr_GetAgentActivated (void);
static void btrMgr_SetDiscoveryInProgress (unsigned char status);
static unsigned char btrMgr_GetDiscoveryInProgress (void);
static void btrMgr_SetCurrentDiscoveryDeviceType (BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT);
static BTRMGR_DeviceOperationType_t btrMgr_GetCurrentDiscoveryDeviceType (void);
static unsigned char btrMgr_GetDevPaired (BTRMgrDeviceHandle ahBTRMgrDevHdl); 
static BTRMGR_DeviceType_t btrMgr_MapDeviceTypeFromCore (enBTRCoreDeviceClass device_type);
static BTRMGR_RSSIValue_t btrMgr_MapSignalStrengthToRSSI (int signalStrength);
static eBTRMgrRet btrMgr_MapDevstatusInfoToEventInfo (void* p_StatusCB, BTRMGR_EventMessage_t* apstEventMessage, BTRMGR_Events_t type);
static eBTRMgrRet btrMgr_StartCastingAudio (int outFileFd, int outMTUSize);
static eBTRMgrRet btrMgr_StopCastingAudio (void); 
static eBTRMgrRet btrMgr_StartReceivingAudio (int inFileFd, int inMTUSize, unsigned int ui32InSampFreq);
static eBTRMgrRet btrMgr_StopReceivingAudio (void); 
static eBTRMgrRet btrMgr_ConnectToDevice (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t connectAs, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx);
static eBTRMgrRet btrMgr_StartAudioStreamingOut (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t streamOutPref, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx, unsigned int aui32SleepIdx);
static eBTRMgrRet btrMgr_AddPersistentEntry(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl);
static eBTRMgrRet btrMgr_RemovePersistentEntry(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl);

/*  Local Op Threads Prototypes */

/* Incoming Callbacks Prototypes */
static eBTRMgrRet btrMgr_ACDataReadyCb (void* apvAcDataBuf, unsigned int aui32AcDataLen, void* apvUserData);
static eBTRMgrRet btrMgr_SOStatusCb (stBTRMgrMediaStatus* apstBtrMgrSoStatus, void* apvUserData);
static eBTRMgrRet btrMgr_SIStatusCb (stBTRMgrMediaStatus* apstBtrMgrSiStatus, void* apvUserData);

static enBTRCoreRet btrMgr_DeviceStatusCb (stBTRCoreDevStatusCBInfo* p_StatusCB, void* apvUserData);
static enBTRCoreRet btrMgr_DeviceDiscoveryCb (stBTRCoreBTDevice devicefound, void* apvUserData);
static enBTRCoreRet btrMgr_ConnectionInIntimationCb (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInIntimResp, void* apvUserData);
static enBTRCoreRet btrMgr_ConnectionInAuthenticationCb (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInAuthResp, void* apvUserData);
static enBTRCoreRet btrMgr_MediaStatusCb (stBTRCoreMediaStatusCBInfo* mediaStatusCB, void* apvUserData);


/* Static Function Definitions */
static inline unsigned char
btrMgr_GetAdapterCnt (
    void
) {
    return gListOfAdapters.number_of_adapters;
}

static const char* 
btrMgr_GetAdapterPath (
    unsigned char   aui8AdapterIdx
) {
    const char* pReturn = NULL;

    if (gListOfAdapters.number_of_adapters) {
        if ((aui8AdapterIdx < gListOfAdapters.number_of_adapters) && (aui8AdapterIdx < BTRCORE_MAX_NUM_BT_ADAPTERS)) {
            pReturn = gListOfAdapters.adapter_path[aui8AdapterIdx];
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

static void
btrMgr_SetCurrentDiscoveryDeviceType (
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    gDiscoveryType = aenBTRMgrDevOpT;
}

static BTRMGR_DeviceOperationType_t
btrMgr_GetCurrentDiscoveryDeviceType (
    void
) {
    return gDiscoveryType;
}

static unsigned char
btrMgr_GetDevPaired (
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    int j = 0;

    for (j = 0; j < gListOfPairedDevices.m_numOfDevices; j++) {
        if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
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
    case enBTRCore_DC_Tablet: 
        type = BTRMGR_DEVICE_TYPE_TABLET;
        break; 
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

static BTRMGR_RSSIValue_t
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
    void*                   p_StatusCB,         /* device status info */
    BTRMGR_EventMessage_t*  apstEventMessage,   /* event message      */
    BTRMGR_Events_t         type                /* event type         */
) {
    eBTRMgrRet  lenBtrMgrRet = eBTRMgrSuccess;

    apstEventMessage->m_adapterIndex = 0;
    apstEventMessage->m_eventType    = type;
    apstEventMessage->m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;/* Application will have to get the list explicitly for list;Lets return the max value */

    if (!p_StatusCB)
        return eBTRMgrFailure;

    
    if (type == BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE) {
        apstEventMessage->m_discoveredDevice.m_deviceHandle      = ((stBTRCoreBTDevice*)p_StatusCB)->tDeviceId;
        apstEventMessage->m_discoveredDevice.m_signalLevel       = ((stBTRCoreBTDevice*)p_StatusCB)->i32RSSI;
        apstEventMessage->m_discoveredDevice.m_deviceType        = btrMgr_MapDeviceTypeFromCore(((stBTRCoreBTDevice*)p_StatusCB)->enDeviceType);
        apstEventMessage->m_discoveredDevice.m_rssi              = btrMgr_MapSignalStrengthToRSSI(((stBTRCoreBTDevice*)p_StatusCB)->i32RSSI);
        apstEventMessage->m_discoveredDevice.m_isPairedDevice    = btrMgr_GetDevPaired(apstEventMessage->m_discoveredDevice.m_deviceHandle);
        strncpy(apstEventMessage->m_discoveredDevice.m_name, ((stBTRCoreBTDevice*)p_StatusCB)->pcDeviceName, (BTRMGR_NAME_LEN_MAX-1));
        strncpy(apstEventMessage->m_discoveredDevice.m_deviceAddress, ((stBTRCoreBTDevice*)p_StatusCB)->pcDeviceAddress, (BTRMGR_NAME_LEN_MAX-1));
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST) {
        apstEventMessage->m_externalDevice.m_deviceHandle        = ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.tDeviceId;
        apstEventMessage->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.enDeviceType);
        apstEventMessage->m_externalDevice.m_vendorID            = ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.ui32VendorId;
        apstEventMessage->m_externalDevice.m_isLowEnergyDevice   = 0;
        apstEventMessage->m_externalDevice.m_externalDevicePIN   = ((stBTRCoreConnCBInfo*)p_StatusCB)->ui32devPassKey;
        strncpy(apstEventMessage->m_externalDevice.m_name, ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.pcDeviceName, (BTRMGR_NAME_LEN_MAX - 1));
        strncpy(apstEventMessage->m_externalDevice.m_deviceAddress, ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST) {
        apstEventMessage->m_externalDevice.m_deviceHandle        = ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.tDeviceId;
        apstEventMessage->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.enDeviceType);
        apstEventMessage->m_externalDevice.m_vendorID            = ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.ui32VendorId;
        apstEventMessage->m_externalDevice.m_isLowEnergyDevice   = 0;
        strncpy(apstEventMessage->m_externalDevice.m_name, ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.pcDeviceName, (BTRMGR_NAME_LEN_MAX - 1));
        strncpy(apstEventMessage->m_externalDevice.m_deviceAddress, ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST) {
        apstEventMessage->m_externalDevice.m_deviceHandle        = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
        apstEventMessage->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
        apstEventMessage->m_externalDevice.m_vendorID            = 0;
        apstEventMessage->m_externalDevice.m_isLowEnergyDevice   = 0;
        strncpy(apstEventMessage->m_externalDevice.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName, (BTRMGR_NAME_LEN_MAX - 1));
        strncpy(apstEventMessage->m_externalDevice.m_deviceAddress, "TO BE FILLED", (BTRMGR_NAME_LEN_MAX - 1));
    }
    else {
       apstEventMessage->m_pairedDevice.m_deviceHandle           = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
       apstEventMessage->m_pairedDevice.m_deviceType             = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
       apstEventMessage->m_pairedDevice.m_isLastConnectedDevice  = (ghBTRMgrDevHdlLastConnected == apstEventMessage->m_pairedDevice.m_deviceHandle) ? 1 : 0;
       strncpy(apstEventMessage->m_pairedDevice.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName, (BTRMGR_NAME_LEN_MAX-1));
    }


    return lenBtrMgrRet; 
}

static eBTRMgrRet
btrMgr_StartCastingAudio (
    int     outFileFd, 
    int     outMTUSize
) {
    stBTRMgrOutASettings    lstBtrMgrAcOutASettings;
    stBTRMgrInASettings     lstBtrMgrSoInASettings;
    stBTRMgrOutASettings    lstBtrMgrSoOutASettings;
    eBTRMgrRet              lenBtrMgrRet = eBTRMgrSuccess;

    int                     inBytesToEncode = 3072; // Corresponds to MTU size of 895


    if ((ghBTRMgrDevHdlCurStreaming != 0) || (outMTUSize == 0)) {
        return eBTRMgrFailInArg;
    }


    /* Reset the buffer */
    memset(&gstBTRMgrStreamingInfo, 0, sizeof(gstBTRMgrStreamingInfo));

    memset(&lstBtrMgrAcOutASettings, 0, sizeof(lstBtrMgrAcOutASettings));
    memset(&lstBtrMgrSoInASettings,  0, sizeof(lstBtrMgrSoInASettings));
    memset(&lstBtrMgrSoOutASettings, 0, sizeof(lstBtrMgrSoOutASettings));

    /* Init StreamOut module - Create Pipeline */
    if ((lenBtrMgrRet = BTRMgr_SO_Init(&gstBTRMgrStreamingInfo.hBTRMgrSoHdl, btrMgr_SOStatusCb, &gstBTRMgrStreamingInfo)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_Init FAILED\n");
        return eBTRMgrInitFailure;
    }

    if ((lenBtrMgrRet = BTRMgr_AC_Init(&gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_AC_Init FAILED\n");
        return eBTRMgrInitFailure;
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
        return eBTRMgrFailure;
    }


    if ((lenBtrMgrRet = BTRMgr_AC_GetDefaultSettings(gstBTRMgrStreamingInfo.hBTRMgrAcHdl, &lstBtrMgrAcOutASettings)) != eBTRMgrSuccess) {
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


    if ((lenBtrMgrRet = BTRMgr_SO_GetEstimatedInABufSize(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_GetEstimatedInABufSize FAILED\n");
        lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize = inBytesToEncode;
    }
    else {
        inBytesToEncode = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;
    }


    if ((lenBtrMgrRet = BTRMgr_SO_Start(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_Start FAILED\n");
    }

    if (lenBtrMgrRet == eBTRMgrSuccess) {
        lstBtrMgrAcOutASettings.i32BtrMgrOutBufMaxSize = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;

        if ((lenBtrMgrRet = BTRMgr_AC_Start(gstBTRMgrStreamingInfo.hBTRMgrAcHdl,
                                            &lstBtrMgrAcOutASettings,
                                            btrMgr_ACDataReadyCb,
                                            &gstBTRMgrStreamingInfo)) != eBTRMgrSuccess) {
            BTRMGRLOG_ERROR ("BTRMgr_AC_Start FAILED\n");
        }
    }

    if (lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo)
        free(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo);

    if (lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo)
        free(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo);

    if (lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo)
        free(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo);

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StopCastingAudio (
    void
) {
    eBTRMgrRet  lenBtrMgrRet = eBTRMgrSuccess;

    if (ghBTRMgrDevHdlCurStreaming == 0) {
        return eBTRMgrFailInArg;
    }


    if ((lenBtrMgrRet = BTRMgr_AC_Stop(gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_AC_Stop FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_SO_SendEOS(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_SendEOS FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_SO_Stop(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_Stop FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_AC_DeInit(gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_AC_DeInit FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_SO_DeInit(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_DeInit FAILED\n");
    }

    gstBTRMgrStreamingInfo.hBTRMgrAcHdl = NULL;
    gstBTRMgrStreamingInfo.hBTRMgrSoHdl = NULL;

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StartReceivingAudio (
    int             inFileFd,
    int             inMTUSize, 
    unsigned int    ui32InSampFreq
) {
    eBTRMgrRet      lenBtrMgrRet = eBTRMgrSuccess;
    int             inBytesToEncode = 3072;

    if ((ghBTRMgrDevHdlCurStreaming != 0) || (inMTUSize == 0)) {
        return eBTRMgrFailInArg;
    }


    /* Init StreamIn module - Create Pipeline */
    if ((lenBtrMgrRet = BTRMgr_SI_Init(&gstBTRMgrStreamingInfo.hBTRMgrSiHdl, btrMgr_SIStatusCb, &gstBTRMgrStreamingInfo)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SI_Init FAILED\n");
        return eBTRMgrInitFailure;
    }

    if ((lenBtrMgrRet = BTRMgr_SI_Start(gstBTRMgrStreamingInfo.hBTRMgrSiHdl, inBytesToEncode, inFileFd, inMTUSize, ui32InSampFreq)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SI_Start FAILED\n");
    }

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StopReceivingAudio (
    void
) {
    eBTRMgrRet  lenBtrMgrRet = eBTRMgrSuccess;

    if (ghBTRMgrDevHdlCurStreaming == 0) {
        return eBTRMgrFailInArg;
    }


    if ((lenBtrMgrRet = BTRMgr_SI_SendEOS(gstBTRMgrStreamingInfo.hBTRMgrSiHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SI_SendEOS FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_SI_Stop(gstBTRMgrStreamingInfo.hBTRMgrSiHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SI_Stop FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_SI_DeInit(gstBTRMgrStreamingInfo.hBTRMgrSiHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SI_DeInit FAILED\n");
    }

    gstBTRMgrStreamingInfo.hBTRMgrSiHdl = NULL;

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_ConnectToDevice (
    unsigned char                   aui8AdapterIdx,
    BTRMgrDeviceHandle              ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t    connectAs,
    unsigned int                    aui32ConnectRetryIdx,
    unsigned int                    aui32ConfirmIdx
) {
    eBTRMgrRet      lenBtrMgrRet    = eBTRMgrSuccess;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    unsigned int    ui32retryIdx    = aui32ConnectRetryIdx + 1;
    enBTRCoreDeviceType lenBTRCoreDeviceType = enBTRCoreUnknown;


    if (btrMgr_GetDiscoveryInProgress()) {
        if (btrMgr_GetCurrentDiscoveryDeviceType() == BTRMGR_DEVICE_OP_TYPE_LE) {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_LE);
        }
        else {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
        }
    }

    switch (connectAs) {
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT:
        lenBTRCoreDeviceType = enBTRCoreSpeakers;
        break;
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT:
        lenBTRCoreDeviceType = enBTRCoreMobileAudioIn;
        break;
    case BTRMGR_DEVICE_OP_TYPE_LE:
        lenBTRCoreDeviceType = enBTRCoreLE;
        break;
    case BTRMGR_DEVICE_OP_TYPE_UNKNOWN:
    default:
        break;
    } 

    if (lenBTRCoreDeviceType == enBTRCoreUnknown) {
       BTRMGRLOG_ERROR ("Unknown BTRMGR_DeviceConnect_Type_t : %d", lenBTRCoreDeviceType);
       return eBTRMgrFailure;
    }

    do {
        /* connectAs param is unused for now.. */
        lenBtrCoreRet = BTRCore_ConnectDevice (ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDeviceType);
        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Connect to this device\n");
            lenBtrMgrRet = eBTRMgrFailure;
        }
        else {
            BTRMGRLOG_INFO ("Connected Successfully\n");
            lenBtrMgrRet = eBTRMgrSuccess;
        }

        if (lenBTRCoreDeviceType != enBTRCoreLE && lenBtrMgrRet != eBTRMgrFailure) {
            /* Max 20 sec timeout - Polled at 1 second interval: Confirmed 4 times */
            unsigned int ui32sleepTimeOut = 1;
            unsigned int ui32confirmIdx = aui32ConfirmIdx + 1;
            
            do {
                unsigned int ui32sleepIdx = 5;

                do {
                    sleep(ui32sleepTimeOut); 
                    lenBtrCoreRet = BTRCore_GetDeviceConnected(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDeviceType);
                } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
            } while (--ui32confirmIdx);

            if (lenBtrCoreRet != enBTRCoreSuccess) {
                BTRMGRLOG_ERROR ("Failed to Connect to this device - Confirmed\n");
                lenBtrMgrRet = eBTRMgrFailure;
            }
            else {
                BTRMGRLOG_DEBUG ("Succes Connect to this device - Confirmed\n");

                if (ghBTRMgrDevHdlLastConnected && ghBTRMgrDevHdlLastConnected != ahBTRMgrDevHdl) {
                   BTRMGRLOG_DEBUG ("Remove persistent entry for previously connected device(%llu)\n", ghBTRMgrDevHdlLastConnected);
                   btrMgr_RemovePersistentEntry(aui8AdapterIdx, ghBTRMgrDevHdlLastConnected);
                }

                btrMgr_AddPersistentEntry (aui8AdapterIdx, ahBTRMgrDevHdl);
                gIsDeviceConnected = 1;
                gIsUserInitiated = 0;
                ghBTRMgrDevHdlLastConnected = ahBTRMgrDevHdl;
            }
        }
    } while ((lenBtrMgrRet == eBTRMgrFailure) && (--ui32retryIdx));


    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StartAudioStreamingOut (
    unsigned char                   aui8AdapterIdx,
    BTRMgrDeviceHandle              ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t    streamOutPref,
    unsigned int                    aui32ConnectRetryIdx,
    unsigned int                    aui32ConfirmIdx,
    unsigned int                    aui32SleepIdx
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    eBTRMgrRet      lenBtrMgrRet    = eBTRMgrSuccess;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    unsigned char   isFound = 0;
    int             i = 0;
    int             deviceFD = 0;
    int             deviceReadMTU = 0;
    int             deviceWriteMTU = 0;
    unsigned int    ui32retryIdx = aui32ConnectRetryIdx + 1;
    stBTRCorePairedDevicesCount listOfPDevices;


    if (ghBTRMgrDevHdlCurStreaming == ahBTRMgrDevHdl) {
        BTRMGRLOG_WARN ("Its already streaming out in this device.. Check the volume :)\n");
        return eBTRMgrSuccess;
    }


    if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming != ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Its already streaming out. lets stop this and start on other device \n");
        if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
            return eBTRMgrFailure;
        }
    }

    /* Check whether the device is in the paired list */
    memset(&listOfPDevices, 0, sizeof(listOfPDevices));
    if ((lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &listOfPDevices)) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get the paired devices list\n");
        return eBTRMgrFailure;
    }


    if (!listOfPDevices.numberOfDevices) {
        BTRMGRLOG_ERROR ("No device is paired yet; Will not be able to play at this moment\n");
        return eBTRMgrFailure;
    }


    for (i = 0; i < listOfPDevices.numberOfDevices; i++) {
        if (ahBTRMgrDevHdl == listOfPDevices.devices[i].tDeviceId) {
            isFound = 1;
            break;
        }
    }

    if (!isFound) {
        BTRMGRLOG_ERROR ("Failed to find this device in the paired devices list\n");
        return eBTRMgrFailure;
    }


    if (aui32ConnectRetryIdx) {
        unsigned int ui32sleepTimeOut = 2;
        unsigned int ui32confirmIdx = aui32ConfirmIdx + 1;

        do {
            unsigned int ui32sleepIdx = aui32SleepIdx + 1;
            do {
                sleep(ui32sleepTimeOut);
                lenBtrCoreRet = BTRCore_IsDeviceConnectable(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId);
            } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
        } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32confirmIdx));

        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Device Not Connectable\n");
            return eBTRMgrFailure;
        }
    }


    do {
        unsigned short  ui16DevMediaBitrate = 0;

        /* Connect the device  - If the device is not connected, Connect to it */
        if (aui32ConnectRetryIdx) {
            lenBtrMgrRet = btrMgr_ConnectToDevice(aui8AdapterIdx, listOfPDevices.devices[i].tDeviceId, streamOutPref, BTRMGR_CONNECT_RETRY_ATTEMPTS, BTRMGR_DEVCONN_CHECK_RETRY_ATTEMPTS);
        }
        else {
            lenBtrMgrRet = btrMgr_ConnectToDevice(aui8AdapterIdx, listOfPDevices.devices[i].tDeviceId, streamOutPref, 0, 1);
        }

        if (lenBtrMgrRet == eBTRMgrSuccess) {
            if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
                free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
                gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
            }

            gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaSbcInfo) > sizeof(stBTRCoreDevMediaMpegInfo)) ? sizeof(stBTRCoreDevMediaSbcInfo) : sizeof(stBTRCoreDevMediaMpegInfo));

            lenBtrCoreRet = BTRCore_GetDeviceMediaInfo(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, enBTRCoreSpeakers, &gstBtrCoreDevMediaInfo);
            if (lenBtrCoreRet == enBTRCoreSuccess) {
                if (gstBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
                    ui16DevMediaBitrate = ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate;

                    BTRMGRLOG_INFO ("DevMedInfo SFreq         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
                    BTRMGRLOG_INFO ("DevMedInfo AChan         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
                    BTRMGRLOG_INFO ("DevMedInfo SbcAllocMethod= %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
                    BTRMGRLOG_INFO ("DevMedInfo SbcSubbands   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
                    BTRMGRLOG_INFO ("DevMedInfo SbcBlockLength= %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
                    BTRMGRLOG_INFO ("DevMedInfo SbcMinBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
                    BTRMGRLOG_INFO ("DevMedInfo SbcMaxBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
                    BTRMGRLOG_INFO ("DevMedInfo SbcFrameLen   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
                    BTRMGRLOG_DEBUG("DevMedInfo SbcBitrate    = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
                }
            }

            if (ui16DevMediaBitrate) {
                /* Aquire Device Data Path to start the audio casting */
                lenBtrCoreRet = BTRCore_AcquireDeviceDataPath (ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, enBTRCoreSpeakers, &deviceFD, &deviceReadMTU, &deviceWriteMTU);
                if ((lenBtrCoreRet == enBTRCoreSuccess) && deviceWriteMTU) {
                    /* Now that you got the FD & Read/Write MTU, start casting the audio */
                    if ((lenBtrMgrRet = btrMgr_StartCastingAudio(deviceFD, deviceWriteMTU)) == eBTRMgrSuccess) {
                        ghBTRMgrDevHdlCurStreaming = listOfPDevices.devices[i].tDeviceId;
                        BTRMGRLOG_INFO("Streaming Started.. Enjoy the show..! :)\n");
                    }
                    else {
                        BTRMGRLOG_ERROR ("Failed to stream now\n");
                    }
                }
            }

        }


        if (!ui16DevMediaBitrate || (lenBtrCoreRet != enBTRCoreSuccess) || (lenBtrMgrRet != eBTRMgrSuccess)) {
            BTRMGRLOG_ERROR ("Failed to get Device Data Path. So Will not be able to stream now\n");
            BTRMGRLOG_ERROR ("Failed to get Valid MediaBitrate. So Will not be able to stream now\n");
            BTRMGRLOG_ERROR ("Failed to StartCastingAudio. So Will not be able to stream now\n");
            BTRMGRLOG_ERROR ("Failed to connect to device and not playing\n");
            lenBtrCoreRet = BTRCore_DisconnectDevice (ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, enBTRCoreSpeakers);
            if (lenBtrCoreRet == enBTRCoreSuccess) {
                /* Max 4 sec timeout - Polled at 1 second interval: Confirmed 2 times */
                unsigned int ui32sleepTimeOut = 1;
                unsigned int ui32confirmIdx = aui32ConfirmIdx + 1;
                
                do {
                    unsigned int ui32sleepIdx = aui32SleepIdx + 1;

                    do {
                        sleep(ui32sleepTimeOut);
                        lenBtrCoreRet = BTRCore_GetDeviceDisconnected(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, enBTRCoreSpeakers);
                    } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
                } while (--ui32confirmIdx);

                if (lenBtrCoreRet != enBTRCoreSuccess) {
                    BTRMGRLOG_ERROR ("Failed to Disconnect from this device - Confirmed\n");
                    lenBtrMgrRet = eBTRMgrFailure; 
                }
                else
                    BTRMGRLOG_DEBUG ("Success Disconnect from this device - Confirmed\n");
            }

            lenBtrMgrRet = eBTRMgrFailure; 
        }


    } while ((lenBtrMgrRet == eBTRMgrFailure) && (--ui32retryIdx));


    if (gfpcBBTRMgrEventOut) {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        lstEventMessage.m_adapterIndex                 = aui8AdapterIdx;
        lstEventMessage.m_pairedDevice.m_deviceHandle  = listOfPDevices.devices[i].tDeviceId;
        lstEventMessage.m_pairedDevice.m_deviceType    = btrMgr_MapDeviceTypeFromCore(listOfPDevices.devices[i].enDeviceType);
        strncpy(lstEventMessage.m_pairedDevice.m_name, listOfPDevices.devices[i].pcDeviceName, BTRMGR_NAME_LEN_MAX);
        lstEventMessage.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

        if (lenBtrMgrRet == eBTRMgrSuccess) {
            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE;
            gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
        }
        else if (lenBtrMgrRet == eBTRMgrFailure) {
            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_CONNECTION_FAILED;
            gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
        }
        else {
            //TODO: Some error specific event to XRE
        }
    }

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_AddPersistentEntry (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    char        lui8adapterAddr[BD_NAME_LEN] = {'\0'};
    eBTRMgrRet  lenBtrMgrPiRet = eBTRMgrFailure;

    BTRCore_GetAdapterAddr(ghBTRCoreHdl, aui8AdapterIdx, lui8adapterAddr);

    // Device connected add data from json file
    BTRMGR_Profile_t btPtofile;
    strncpy(btPtofile.adapterId, lui8adapterAddr, BTRMGR_NAME_LEN_MAX);
    strncpy(btPtofile.profileId, BTRMGR_A2DP_SINK_PROFILE_ID, BTRMGR_NAME_LEN_MAX);
    btPtofile.deviceId = ahBTRMgrDevHdl;
    btPtofile.isConnect = 1;

    lenBtrMgrPiRet = BTRMgr_PI_AddProfile(ghBTRMgrPiHdl, btPtofile);
    if(lenBtrMgrPiRet == eBTRMgrSuccess) {
        BTRMGRLOG_INFO ("Persistent File updated successfully\n");
    }
    else {
        BTRMGRLOG_ERROR ("Persistent File update failed \n");
    }

    return lenBtrMgrPiRet;
}

static eBTRMgrRet
btrMgr_RemovePersistentEntry (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    char         lui8adapterAddr[BD_NAME_LEN] = {'\0'};
    eBTRMgrRet   lenBtrMgrPiRet = eBTRMgrFailure;

    BTRCore_GetAdapterAddr(ghBTRCoreHdl, aui8AdapterIdx, lui8adapterAddr);

    // Device disconnected remove data from json file
    BTRMGR_Profile_t btPtofile;
    strncpy(btPtofile.adapterId, lui8adapterAddr, BTRMGR_NAME_LEN_MAX);
    strncpy(btPtofile.profileId, BTRMGR_A2DP_SINK_PROFILE_ID, BTRMGR_NAME_LEN_MAX);
    btPtofile.deviceId = ahBTRMgrDevHdl;
    btPtofile.isConnect = 1;

    lenBtrMgrPiRet = BTRMgr_PI_RemoveProfile(ghBTRMgrPiHdl, btPtofile);
    if(lenBtrMgrPiRet == eBTRMgrSuccess) {
       BTRMGRLOG_INFO ("Persistent File updated successfully\n");
    }
    else {
       BTRMGRLOG_ERROR ("Persistent File update failed \n");
    }

    return lenBtrMgrPiRet;
}


/*  Local Op Threads */


/* Interfaces - Public Functions */
BTRMGR_Result_t
BTRMGR_Init (
    void
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet 	lenBtrCoreRet   = enBTRCoreSuccess;
    eBTRMgrRet      lenBtrMgrPiRet  = eBTRMgrFailure;
    char            lpcBtVersion[BTRCORE_STRINGS_MAX_LEN] = {'\0'};


    if (ghBTRCoreHdl) {
        BTRMGRLOG_WARN("Already Inited; Return Success\n");
        return lenBtrMgrResult;
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
    memset(&gDefaultAdapterContext, 0, sizeof(gDefaultAdapterContext));
    memset(&gListOfAdapters, 0, sizeof(gListOfAdapters));
    memset(&gstBTRMgrStreamingInfo, 0, sizeof(gstBTRMgrStreamingInfo));
    memset(&gListOfPairedDevices, 0, sizeof(gListOfPairedDevices));
    memset(&gstBtrCoreDevMediaInfo, 0, sizeof(gstBtrCoreDevMediaInfo));
    gIsDiscoveryInProgress = 0;


    /* Init the mutex */

    /* Call the Core/HAL init */
    lenBtrCoreRet = BTRCore_Init(&ghBTRCoreHdl);
    if ((!ghBTRCoreHdl) || (lenBtrCoreRet != enBTRCoreSuccess)) {
        BTRMGRLOG_ERROR ("Could not initialize BTRCore/HAL module\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (enBTRCoreSuccess != BTRCore_GetVersionInfo(ghBTRCoreHdl, lpcBtVersion)) {
        BTRMGRLOG_ERROR ("BTR Bluetooth Version: FAILED\n");
    }
    BTRMGRLOG_INFO("BTR Bluetooth Version: %s\n", lpcBtVersion);

    if (enBTRCoreSuccess != BTRCore_GetListOfAdapters (ghBTRCoreHdl, &gListOfAdapters)) {
        BTRMGRLOG_ERROR ("Failed to get the total number of Adapters present\n"); /* Not a Error case anyway */
    }
    BTRMGRLOG_INFO ("Number of Adapters found are = %u\n", gListOfAdapters.number_of_adapters);

    if (0 == gListOfAdapters.number_of_adapters) {
        BTRMGRLOG_WARN("Bluetooth adapter NOT Found..!!!!\n");
        return  BTRMGR_RESULT_GENERIC_FAILURE;
    }


    /* you have atlesat one Bluetooth adapter. Now get the Default Adapter path for future usages; */
    gDefaultAdapterContext.bFirstAvailable = 1; /* This is unused by core now but lets fill it */
    if (enBTRCoreSuccess == BTRCore_GetAdapter(ghBTRCoreHdl, &gDefaultAdapterContext)) {
        BTRMGRLOG_DEBUG ("Aquired default Adapter; Path is %s\n", gDefaultAdapterContext.pcAdapterPath);
    }

    /* TODO: Handling multiple Adapters */
    if (gListOfAdapters.number_of_adapters > 1) {
        BTRMGRLOG_WARN("Number of Bluetooth Adapters Found : %u !! Lets handle it properly\n", gListOfAdapters.number_of_adapters);
    }


    /* Register for callback to get the status of connected Devices */
    BTRCore_RegisterStatusCb(ghBTRCoreHdl, btrMgr_DeviceStatusCb, NULL);

    /* Register for callback to get the Discovered Devices */
    BTRCore_RegisterDiscoveryCb(ghBTRCoreHdl, btrMgr_DeviceDiscoveryCb, NULL);

    /* Register for callback to process incoming pairing requests */
    BTRCore_RegisterConnectionIntimationCb(ghBTRCoreHdl, btrMgr_ConnectionInIntimationCb, NULL);

    /* Register for callback to process incoming connection requests */
    BTRCore_RegisterConnectionAuthenticationCb(ghBTRCoreHdl, btrMgr_ConnectionInAuthenticationCb, NULL);

    /* Register for callback to process incoming media events */
    BTRCore_RegisterMediaStatusCb(ghBTRCoreHdl, btrMgr_MediaStatusCb, NULL);


    /* Activate Agent on Init */
    if (!btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((lenBtrCoreRet = BTRCore_RegisterAgent(ghBTRCoreHdl, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }

    /* Initialize the Paired Device List for Default adapter */
    BTRMGR_GetPairedDevices (gDefaultAdapterContext.adapter_number, &gListOfPairedDevices);


    // Init Persistent handles
    lenBtrMgrPiRet = BTRMgr_PI_Init(&ghBTRMgrPiHdl);
    if(lenBtrMgrPiRet != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Could not initialize PI module\n");
    }
    
    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_DeInit (
    void
) {
    eBTRMgrRet      lenBtrMgrPiResult = eBTRMgrSuccess;
    enBTRCoreRet 	lenBtrCoreRet     = enBTRCoreSuccess;
    BTRMGR_Result_t lenBtrMgrResult   = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (ghBTRMgrPiHdl) {
        lenBtrMgrPiResult = BTRMgr_PI_DeInit(ghBTRMgrPiHdl);
        ghBTRMgrPiHdl = NULL;
        BTRMGRLOG_ERROR ("PI Module DeInited; Now will we exit the app = %d\n", lenBtrMgrPiResult);
    }

    if (ghBTRCoreHdl) {
        lenBtrCoreRet = BTRCore_DeInit(ghBTRCoreHdl);
        ghBTRCoreHdl = NULL;
        BTRMGRLOG_ERROR ("BTRCore DeInited; Now will we exit the app = %d\n", lenBtrCoreRet);
    }

    lenBtrMgrResult =  ((lenBtrMgrPiResult == eBTRMgrSuccess) && 
                        (lenBtrCoreRet == enBTRCoreSuccess)) ? BTRMGR_RESULT_SUCCESS : BTRMGR_RESULT_GENERIC_FAILURE;
    BTRMGRLOG_DEBUG ("Exit Status = %d\n", lenBtrMgrResult)


    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_GetNumberOfAdapters (
    unsigned char*  pNumOfAdapters
) {
    BTRMGR_Result_t         lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCoreListAdapters   listOfAdapters;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if (!pNumOfAdapters) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    memset(&listOfAdapters, 0, sizeof(listOfAdapters));

    lenBtrCoreRet = BTRCore_GetListOfAdapters(ghBTRCoreHdl, &listOfAdapters);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        *pNumOfAdapters = listOfAdapters.number_of_adapters;
        /* Copy to our backup */
        if (listOfAdapters.number_of_adapters != gListOfAdapters.number_of_adapters)
            memcpy (&gListOfAdapters, &listOfAdapters, sizeof (stBTRCoreListAdapters));

        BTRMGRLOG_DEBUG ("Available Adapters = %d\n", listOfAdapters.number_of_adapters);
    }
    else {
        BTRMGRLOG_ERROR ("Could not find Adapters\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }


    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_ResetAdapter (
    unsigned char aui8AdapterIdx
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    BTRMGRLOG_ERROR ("No Ops. As the Hal is not implemented yet\n");

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_SetAdapterName (
    unsigned char   aui8AdapterIdx,
    const char*     pNameOfAdapter
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    const char*     pAdapterPath    = NULL;
    char            name[BTRMGR_NAME_LEN_MAX] = {'\0'};

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pNameOfAdapter)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    strncpy (name, pNameOfAdapter, (BTRMGR_NAME_LEN_MAX - 1));
    lenBtrCoreRet = BTRCore_SetAdapterName(ghBTRCoreHdl, pAdapterPath, name);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to set Adapter Name\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Set Successfully\n");
    }


    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetAdapterName (
    unsigned char   aui8AdapterIdx,
    char*           pNameOfAdapter
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    const char*     pAdapterPath    = NULL;
    char            name[BTRMGR_NAME_LEN_MAX] = {'\0'};

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pNameOfAdapter)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    lenBtrCoreRet = BTRCore_GetAdapterName(ghBTRCoreHdl, pAdapterPath, name);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get Adapter Name\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Fetched Successfully\n");
    }

    /*  Copy regardless of success or failure. */
    strncpy (pNameOfAdapter, name, (BTRMGR_NAME_LEN_MAX - 1));


    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_SetAdapterPowerStatus (
    unsigned char   aui8AdapterIdx,
    unsigned char   power_status
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    const char*     pAdapterPath    = NULL;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (power_status > 1)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    /* Check whether the requested device is connected n playing. */
    if ((ghBTRMgrDevHdlCurStreaming) && (power_status == 0)) {
        /* This will internall stops the playback as well as disconnects. */
        lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming);
        if (lenBtrMgrResult != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to stop Playback. Going Ahead to power off Adapter.\n");
        }
    }


    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }



    lenBtrCoreRet = BTRCore_SetAdapterPower(ghBTRCoreHdl, pAdapterPath, power_status);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to set Adapter Power Status\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Set Successfully\n");
    }


    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetAdapterPowerStatus (
    unsigned char   aui8AdapterIdx,
    unsigned char*  pPowerStatus
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    const char*     pAdapterPath    = NULL;
    unsigned char   power_status    = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pPowerStatus)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    lenBtrCoreRet = BTRCore_GetAdapterPower(ghBTRCoreHdl, pAdapterPath, &power_status);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get Adapter Power\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Fetched Successfully\n");
        *pPowerStatus = power_status;
    }


    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_SetAdapterDiscoverable (
    unsigned char   aui8AdapterIdx,
    unsigned char   discoverable,
    int  timeout
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    const char*     pAdapterPath    = NULL;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;

    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (discoverable > 1)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    //timeout=0 -> no timeout, -1 -> default timeout (180 secs), x -> x seconds
    if (timeout >= 0) {
        lenBtrCoreRet = BTRCore_SetAdapterDiscoverableTimeout(ghBTRCoreHdl, pAdapterPath, timeout);
        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to set Adapter discovery timeout\n");
        }
        else {
            BTRMGRLOG_INFO ("Set timeout Successfully\n");
        }
    }

    /* Set the  discoverable state */
    if ((lenBtrCoreRet = BTRCore_SetAdapterDiscoverable(ghBTRCoreHdl, pAdapterPath, discoverable)) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to set Adapter discoverable status\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Set discoverable status Successfully\n");
        if (discoverable) {
            if (!btrMgr_GetAgentActivated()) {
                BTRMGRLOG_INFO ("Activate agent\n");
                if ((lenBtrCoreRet = BTRCore_RegisterAgent(ghBTRCoreHdl, 1)) != enBTRCoreSuccess) {
                    BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
                    lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
                }
                else {
                    btrMgr_SetAgentActivated(1);
                }
            }
        }
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_IsAdapterDiscoverable (
    unsigned char   aui8AdapterIdx,
    unsigned char*  pDiscoverable
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    const char*     pAdapterPath    = NULL;
    unsigned char   discoverable    = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pDiscoverable)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    
    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }
    

    if ((lenBtrCoreRet = BTRCore_GetAdapterDiscoverableStatus(ghBTRCoreHdl, pAdapterPath, &discoverable)) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get Adapter Status\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Fetched Successfully\n");
        *pDiscoverable = discoverable;
        if (discoverable) {
            if (!btrMgr_GetAgentActivated()) {
                BTRMGRLOG_INFO ("Activate agent\n");
                if ((lenBtrCoreRet = BTRCore_RegisterAgent(ghBTRCoreHdl, 1)) != enBTRCoreSuccess) {
                    BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
                    lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
                }
                else {
                    btrMgr_SetAgentActivated(1);
                }
            }
        }
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_StartDeviceDiscovery (
    unsigned char                aui8AdapterIdx, 
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    BTRMGR_Result_t     lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet        lenBtrCoreRet   = enBTRCoreSuccess;
    enBTRCoreDeviceType lenBTRCoreDeviceType = enBTRCoreUnknown;
    const char*         pAdapterPath    = NULL;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (btrMgr_GetDiscoveryInProgress()) {
        if (btrMgr_GetCurrentDiscoveryDeviceType() == aenBTRMgrDevOpT) {
            BTRMGRLOG_WARN("Scanning is already in progress\n");
            return BTRMGR_RESULT_SUCCESS;
        }
        else {
            if (btrMgr_GetCurrentDiscoveryDeviceType() == BTRMGR_DEVICE_OP_TYPE_LE) {
                BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
                BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_LE);
            }
            else {
                BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
                BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
            }
        }
    }


    /* Populate the currently Paired Devices. This will be used only for the callback DS update */
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);

    
    switch (aenBTRMgrDevOpT) {
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT:
        lenBTRCoreDeviceType = enBTRCoreSpeakers;
        break;
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT:
        lenBTRCoreDeviceType = enBTRCoreMobileAudioIn;
        break;
    case BTRMGR_DEVICE_OP_TYPE_LE:
        lenBTRCoreDeviceType = enBTRCoreLE;
        break;
    case BTRMGR_DEVICE_OP_TYPE_UNKNOWN:
    default:
        lenBTRCoreDeviceType = enBTRCoreUnknown;
        break;
    } 


    lenBtrCoreRet = BTRCore_StartDiscovery(ghBTRCoreHdl, pAdapterPath, lenBTRCoreDeviceType, 0);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to start discovery\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Discovery started Successfully\n");
        btrMgr_SetCurrentDiscoveryDeviceType(aenBTRMgrDevOpT);
        btrMgr_SetDiscoveryInProgress(1);
    }

    
    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_StopDeviceDiscovery (
    unsigned char                aui8AdapterIdx,
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    BTRMGR_Result_t     lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet        lenBtrCoreRet   = enBTRCoreSuccess;
    enBTRCoreDeviceType lenBTRCoreDeviceType = enBTRCoreUnknown;
    const char*         pAdapterPath    = NULL;

    if (!btrMgr_GetDiscoveryInProgress()) {
        BTRMGRLOG_WARN("Scanning is not running now\n");
    }
    else if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    switch (aenBTRMgrDevOpT) {
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT:
        lenBTRCoreDeviceType = enBTRCoreSpeakers;
        break;
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT:
        lenBTRCoreDeviceType = enBTRCoreMobileAudioIn;
        break;
    case BTRMGR_DEVICE_OP_TYPE_LE:
        lenBTRCoreDeviceType = enBTRCoreLE;
        break;
    case BTRMGR_DEVICE_OP_TYPE_UNKNOWN:
    default:
        lenBTRCoreDeviceType = enBTRCoreUnknown;
        break;
    }


    lenBtrCoreRet = BTRCore_StopDiscovery(ghBTRCoreHdl, pAdapterPath, lenBTRCoreDeviceType);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to stop discovery\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Discovery Stopped Successfully\n");
        btrMgr_SetDiscoveryInProgress(0);
        btrMgr_SetCurrentDiscoveryDeviceType(BTRMGR_DEVICE_OP_TYPE_UNKNOWN);

        if (gfpcBBTRMgrEventOut) {
            BTRMGR_EventMessage_t lstEventMessage;
            memset (&lstEventMessage, 0, sizeof(lstEventMessage));

            lstEventMessage.m_adapterIndex = aui8AdapterIdx;
            lstEventMessage.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
            lstEventMessage.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

            /*  Post a callback */
            gfpcBBTRMgrEventOut(lstEventMessage);
        }
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetDiscoveredDevices (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_DiscoveredDevicesList_t* pDiscoveredDevices
) {
    BTRMGR_Result_t                 lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                    lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCoreScannedDevicesCount    listOfDevices;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pDiscoveredDevices)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    memset (&listOfDevices, 0, sizeof(listOfDevices));
    lenBtrCoreRet = BTRCore_GetListOfScannedDevices(ghBTRCoreHdl, &listOfDevices);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get list of discovered devices\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
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
                strncpy(ptr->m_name, listOfDevices.devices[i].pcDeviceName, (BTRMGR_NAME_LEN_MAX - 1));
                strncpy(ptr->m_deviceAddress, listOfDevices.devices[i].pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));
                ptr->m_signalLevel = listOfDevices.devices[i].i32RSSI;
                ptr->m_rssi = btrMgr_MapSignalStrengthToRSSI (listOfDevices.devices[i].i32RSSI);
                ptr->m_deviceHandle = listOfDevices.devices[i].tDeviceId;
                ptr->m_deviceType = btrMgr_MapDeviceTypeFromCore(listOfDevices.devices[i].enDeviceType);
                ptr->m_isPairedDevice = btrMgr_GetDevPaired(listOfDevices.devices[i].tDeviceId);

                if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming == ptr->m_deviceHandle))
                    ptr->m_isConnected = 1;
            }
            /*  Success */
            BTRMGRLOG_INFO ("Successful\n");
        }
        else {
            BTRMGRLOG_WARN("No Device is found yet\n");
        }
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_PairDevice (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    BTRMGR_Result_t     lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet        lenBtrCoreRet   = enBTRCoreSuccess;
    BTRMGR_Events_t     lBtMgrOutEvent  = -1;
    unsigned char       ui8reActivateAgent = 0;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (btrMgr_GetDiscoveryInProgress()) {
        if (btrMgr_GetCurrentDiscoveryDeviceType() == BTRMGR_DEVICE_OP_TYPE_LE) {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_LE);
        }
        else {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
        }
    }

    /* Update the Paired Device List */
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);
    if (1 == btrMgr_GetDevPaired(ahBTRMgrDevHdl)) {
        BTRMGRLOG_INFO ("Already a Paired Device; Nothing Done...\n");
        return BTRMGR_RESULT_SUCCESS;
    }

    if (btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("De-Activate agent\n");
        if ((lenBtrCoreRet = BTRCore_UnregisterAgent(ghBTRCoreHdl)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Deactivate Agent\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(0);
            ui8reActivateAgent = 1;
        }
    }


    if (enBTRCoreSuccess != BTRCore_PairDevice(ghBTRCoreHdl, ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Failed to pair a device\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_PAIRING_FAILED;
    }
    else {
        BTRMGRLOG_INFO ("Paired Successfully\n");
        lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
    }


    if (gfpcBBTRMgrEventOut) {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        lstEventMessage.m_adapterIndex = aui8AdapterIdx;
        lstEventMessage.m_eventType    = lBtMgrOutEvent;
        lstEventMessage.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

        /*  Post a callback */
        gfpcBBTRMgrEventOut(lstEventMessage);
    }

    /* Update the Paired Device List */
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);


    if (ui8reActivateAgent) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((lenBtrCoreRet = BTRCore_RegisterAgent(ghBTRCoreHdl, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_UnpairDevice (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    BTRMGR_Result_t     lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet        lenBtrCoreRet   = enBTRCoreSuccess;
    BTRMGR_Events_t     lBtMgrOutEvent  = -1;
    unsigned char       ui8reActivateAgent = 0;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (btrMgr_GetDiscoveryInProgress()) {
        if (btrMgr_GetCurrentDiscoveryDeviceType() == BTRMGR_DEVICE_OP_TYPE_LE) {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_LE);
        }
        else {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
        }
    }

    /* Get the latest Paired Device List; This is added as the developer could add a device thro test application and try unpair thro' UI */
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);
    if (0 == btrMgr_GetDevPaired(ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Not a Paired device...\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }
    
    if (btrMgr_GetAgentActivated()) {
        BTRMGRLOG_INFO ("De-Activate agent\n");
        if ((lenBtrCoreRet = BTRCore_UnregisterAgent(ghBTRCoreHdl)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Deactivate Agent\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(0);
            ui8reActivateAgent = 1;
        }
    }


    /* Check whether the requested device is connected n playing. */
    if (ghBTRMgrDevHdlCurStreaming == ahBTRMgrDevHdl) {
        /* This will internall stops the playback as well as disconnects. */
        lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming);
        if (lenBtrMgrResult != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("BTRMGR_UnpairDevice :This device is being Connected n Playing. Failed to stop Playback. Going Ahead to unpair.\n");
        }
    }


    if (enBTRCoreSuccess != BTRCore_UnPairDevice(ghBTRCoreHdl, ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Failed to unpair\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED;
    }
    else {
        BTRMGRLOG_INFO ("Unpaired Successfully\n");
        lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE;
    }


    if (gfpcBBTRMgrEventOut) {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        lstEventMessage.m_adapterIndex = aui8AdapterIdx;
        lstEventMessage.m_eventType    = lBtMgrOutEvent;
        lstEventMessage.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX; /* Application will have to get the list explicitly for list; Lets return the max value */

        /*  Post a callback */
        gfpcBBTRMgrEventOut(lstEventMessage);
    }

    /* Update the Paired Device List */
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);


    if (ui8reActivateAgent) {
        BTRMGRLOG_INFO ("Activate agent\n");
        if ((lenBtrCoreRet = BTRCore_RegisterAgent(ghBTRCoreHdl, 1)) != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Activate Agent\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            btrMgr_SetAgentActivated(1);
        }
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetPairedDevices (
    unsigned char                aui8AdapterIdx,
    BTRMGR_PairedDevicesList_t*  pPairedDevices
) {
    BTRMGR_Result_t             lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfDevices;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pPairedDevices)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    memset (&listOfDevices, 0, sizeof(listOfDevices));

    lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &listOfDevices);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get list of paired devices\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        memset (pPairedDevices, 0, sizeof(BTRMGR_PairedDevicesList_t));     /* Reset the values to 0 */
        if (listOfDevices.numberOfDevices) {
            int i = 0;
            int j = 0;
            BTRMGR_PairedDevices_t*  ptr = NULL;

            pPairedDevices->m_numOfDevices = listOfDevices.numberOfDevices;

            for (i = 0; i < listOfDevices.numberOfDevices; i++) {
                ptr = &pPairedDevices->m_deviceProperty[i];
                strncpy(ptr->m_name, listOfDevices.devices[i].pcDeviceName, (BTRMGR_NAME_LEN_MAX - 1));
                strncpy(ptr->m_deviceAddress, listOfDevices.devices[i].pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));
                ptr->m_deviceHandle = listOfDevices.devices[i].tDeviceId;
                ptr->m_deviceType = btrMgr_MapDeviceTypeFromCore (listOfDevices.devices[i].enDeviceType);
                ptr->m_serviceInfo.m_numOfService = listOfDevices.devices[i].stDeviceProfile.numberOfService;
                BTRMGRLOG_INFO ("Paired Device ID = %lld\n", listOfDevices.devices[i].tDeviceId);
                for (j = 0; j < listOfDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                    BTRMGRLOG_INFO ("Profile ID = %u; Profile Name = %s\n", listOfDevices.devices[i].stDeviceProfile.profile[j].uuid_value,
                                                                            listOfDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                    ptr->m_serviceInfo.m_profileInfo[j].m_uuid = listOfDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                    strcpy (ptr->m_serviceInfo.m_profileInfo[j].m_profile, listOfDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                }

                if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming == ptr->m_deviceHandle))
                    ptr->m_isConnected = 1;
            }
            /*  Success */
            BTRMGRLOG_INFO ("Successful\n");
        }
        else {
            BTRMGRLOG_WARN("No Device is paired yet\n");
        }
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_ConnectToDevice (
    unsigned char                   aui8AdapterIdx,
    BTRMgrDeviceHandle              ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t    connectAs
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;

    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (btrMgr_ConnectToDevice(aui8AdapterIdx, ahBTRMgrDevHdl, connectAs, 0, 1) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Failure\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return  lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_DisconnectFromDevice (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!gIsDeviceConnected) {
        BTRMGRLOG_ERROR ("No Device is connected at this time\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (btrMgr_GetDiscoveryInProgress()) {
        if (btrMgr_GetCurrentDiscoveryDeviceType() == BTRMGR_DEVICE_OP_TYPE_LE) {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_LE);
        }
        else {
            BTRMGRLOG_WARN("Scanning is still running now; Lets stop it\n");
            BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
        }
    }

    if (ghBTRMgrDevHdlCurStreaming) {
        /* The streaming is happening; stop it */
        lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming);
        if (lenBtrMgrResult != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("Streamout is failed to stop\n");
        }
    }

    gIsUserInitiated = 1;
    /* connectAs param is unused for now.. */
    lenBtrCoreRet = BTRCore_DisconnectDevice (ghBTRCoreHdl, ahBTRMgrDevHdl, enBTRCoreSpeakers);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to Disconnect\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;

        if (gfpcBBTRMgrEventOut) {
            BTRMGR_EventMessage_t lstEventMessage;
            memset (&lstEventMessage, 0, sizeof(lstEventMessage));

            lstEventMessage.m_adapterIndex = aui8AdapterIdx;
            lstEventMessage.m_eventType    = BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED;
            lstEventMessage.m_numOfDevices = BTRMGR_DEVICE_COUNT_MAX;  /* Application will have to get the list explicitly for list; Lets return the max value */

            /*  Post a callback */
            gfpcBBTRMgrEventOut(lstEventMessage);
        }
    }
    else {
        BTRMGRLOG_INFO ("Disconnected  Successfully\n");
    }


    if (lenBtrMgrResult != BTRMGR_RESULT_GENERIC_FAILURE) {
        /* Max 4 sec timeout - Polled at 1 second interval: Confirmed 2 times */
        unsigned int ui32sleepTimeOut = 1;
        unsigned int ui32confirmIdx = 2;
        
        do {
            unsigned int ui32sleepIdx = 2;

            do {
                sleep(ui32sleepTimeOut);
                lenBtrCoreRet = BTRCore_GetDeviceDisconnected(ghBTRCoreHdl, ahBTRMgrDevHdl, enBTRCoreSpeakers);
            } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
        } while (--ui32confirmIdx);

        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Disconnect from this device - Confirmed\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            BTRMGRLOG_DEBUG ("Success Disconnect from this device - Confirmed\n");
            btrMgr_RemovePersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl);
            gIsDeviceConnected = 0;
            ghBTRMgrDevHdlLastConnected = 0;
        }
    }


    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_GetConnectedDevices (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_ConnectedDevicesList_t*  pConnectedDevices
) {
    BTRMGR_Result_t             lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfPDevices;
    unsigned char i = 0;
    unsigned char j = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pConnectedDevices)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    memset (pConnectedDevices, 0, sizeof(BTRMGR_ConnectedDevicesList_t));
    memset (&listOfPDevices, 0, sizeof(listOfPDevices));

    lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &listOfPDevices);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (listOfPDevices.numberOfDevices) {
            for (i = 0; i < listOfPDevices.numberOfDevices; i++) {
                if (listOfPDevices.devices[i].bDeviceConnected) {
                   pConnectedDevices->m_numOfDevices  = 1; /* as we don't support multiple device connection for now */
                   pConnectedDevices->m_deviceProperty[0].m_deviceHandle = listOfPDevices.devices[i].tDeviceId;
                   pConnectedDevices->m_deviceProperty[0].m_deviceType   = btrMgr_MapDeviceTypeFromCore(listOfPDevices.devices[i].enDeviceType);
                   pConnectedDevices->m_deviceProperty[0].m_vendorID     = listOfPDevices.devices[i].ui32VendorId;
                   pConnectedDevices->m_deviceProperty[0].m_isConnected  = 1;
                   strncpy (pConnectedDevices->m_deviceProperty[0].m_name, listOfPDevices.devices[i].pcDeviceName, (BTRMGR_NAME_LEN_MAX - 1));
                   strncpy (pConnectedDevices->m_deviceProperty[0].m_deviceAddress, listOfPDevices.devices[i].pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));

                   pConnectedDevices->m_deviceProperty[0].m_serviceInfo.m_numOfService = listOfPDevices.devices[i].stDeviceProfile.numberOfService;
                   for (j = 0; j < listOfPDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                       pConnectedDevices->m_deviceProperty[0].m_serviceInfo.m_profileInfo[j].m_uuid = listOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                       strncpy (pConnectedDevices->m_deviceProperty[0].m_serviceInfo.m_profileInfo[j].m_profile, listOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
                   }
                   BTRMGRLOG_INFO ("Successfully posted the connected device information\n");
                   break;
                }
            }
        }
    }
    else {
        BTRMGRLOG_ERROR ("Failed to get connected device information\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetDeviceProperties (
    unsigned char               aui8AdapterIdx,
    BTRMgrDeviceHandle          ahBTRMgrDevHdl,
    BTRMGR_DevicesProperty_t*   pDeviceProperty
) {
    BTRMGR_Result_t                 lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                    lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount     listOfPDevices;
    stBTRCoreScannedDevicesCount    listOfSDevices;
    unsigned char                   isFound = 0;
    int                             i = 0;
    int                             j = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pDeviceProperty) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    /* Reset the values to 0 */
    memset (&listOfPDevices, 0, sizeof(listOfPDevices));
    memset (&listOfSDevices, 0, sizeof(listOfSDevices));
    memset (pDeviceProperty, 0, sizeof(BTRMGR_DevicesProperty_t));

    lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &listOfPDevices);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (listOfPDevices.numberOfDevices) {
            for (i = 0; i < listOfPDevices.numberOfDevices; i++) {
                if (ahBTRMgrDevHdl == listOfPDevices.devices[i].tDeviceId) {
                    pDeviceProperty->m_deviceHandle = listOfPDevices.devices[i].tDeviceId;
                    pDeviceProperty->m_deviceType = btrMgr_MapDeviceTypeFromCore(listOfPDevices.devices[i].enDeviceType);
                    pDeviceProperty->m_vendorID = listOfPDevices.devices[i].ui32VendorId;
                    pDeviceProperty->m_isPaired = 1;
                    strncpy(pDeviceProperty->m_name, listOfPDevices.devices[i].pcDeviceName, (BTRMGR_NAME_LEN_MAX - 1));
                    strncpy(pDeviceProperty->m_deviceAddress, listOfPDevices.devices[i].pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));

                    pDeviceProperty->m_serviceInfo.m_numOfService = listOfPDevices.devices[i].stDeviceProfile.numberOfService;
                    for (j = 0; j < listOfPDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                        BTRMGRLOG_INFO ("Profile ID = %d; Profile Name = %s \n", listOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value,
                                                                                                   listOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                        pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_uuid = listOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                        strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, listOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
                    }

                  if (listOfPDevices.devices[i].bDeviceConnected) {
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

    lenBtrCoreRet  = BTRCore_GetListOfScannedDevices (ghBTRCoreHdl, &listOfSDevices);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (listOfSDevices.numberOfDevices) {
            for (i = 0; i < listOfSDevices.numberOfDevices; i++) {
                if (ahBTRMgrDevHdl == listOfSDevices.devices[i].tDeviceId) {
                    if (!isFound) {
                        pDeviceProperty->m_deviceHandle = listOfSDevices.devices[i].tDeviceId;
                        pDeviceProperty->m_deviceType = btrMgr_MapDeviceTypeFromCore(listOfSDevices.devices[i].enDeviceType);
                        pDeviceProperty->m_vendorID = listOfSDevices.devices[i].ui32VendorId;
                        strncpy(pDeviceProperty->m_name, listOfSDevices.devices[i].pcDeviceName, (BTRMGR_NAME_LEN_MAX - 1));
                        strncpy(pDeviceProperty->m_deviceAddress, listOfSDevices.devices[i].pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));

                        pDeviceProperty->m_serviceInfo.m_numOfService = listOfSDevices.devices[i].stDeviceProfile.numberOfService;
                        for (j = 0; j < listOfSDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                            BTRMGRLOG_INFO ("Profile ID = %d; Profile Name = %s \n", listOfSDevices.devices[i].stDeviceProfile.profile[j].uuid_value,
                                                                                                       listOfSDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                            pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_uuid = listOfSDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                            strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, listOfSDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
                        }
                    }
                    pDeviceProperty->m_signalLevel = listOfSDevices.devices[i].i32RSSI;

                    if (listOfPDevices.devices[i].bDeviceConnected) {
                       pDeviceProperty->m_isConnected = 1;
                    }

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
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_StartAudioStreamingOut_StartUp (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_DeviceOperationType_t    aenBTRMgrDevConT
) {
    char                    lui8adapterAddr[BD_NAME_LEN] = {'\0'};
    int                     i32ProfileIdx = 0;
    int                     i32DeviceIdx = 0;
    int                     numOfProfiles = 0;
    int                     deviceCount = 0;
    int                     isConnected = 0;

    BTRMGR_PersistentData_t lstPersistentData;
    BTRMgrDeviceHandle      lDeviceHandle;

    BTRMGR_Result_t         lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;


    if (BTRMgr_PI_GetAllProfiles(ghBTRMgrPiHdl, &lstPersistentData) == eBTRMgrFailure) {
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    BTRMGRLOG_INFO ("Successfully get all profiles\n");
    BTRCore_GetAdapterAddr(ghBTRCoreHdl, aui8AdapterIdx, lui8adapterAddr);

    if (strcmp(lstPersistentData.adapterId, lui8adapterAddr) == 0) {
        gIsAudOutStartupInProgress = 1;
        numOfProfiles = lstPersistentData.numOfProfiles;

        BTRMGRLOG_DEBUG ("Adapter matches = %s\n", lui8adapterAddr);
        BTRMGRLOG_DEBUG ("Number of Profiles = %d\n", numOfProfiles);

        for (i32ProfileIdx = 0; i32ProfileIdx < numOfProfiles; i32ProfileIdx++) {
            deviceCount = lstPersistentData.profileList[i32ProfileIdx].numOfDevices;

            for (i32DeviceIdx = 0; i32DeviceIdx < deviceCount ; i32DeviceIdx++) {
                lDeviceHandle   = lstPersistentData.profileList[i32ProfileIdx].deviceList[i32DeviceIdx].deviceId;
                isConnected     = lstPersistentData.profileList[i32ProfileIdx].deviceList[i32DeviceIdx].isConnected;

                if (isConnected) {
                    ghBTRMgrDevHdlLastConnected = lDeviceHandle;
                    if(strcmp(lstPersistentData.profileList[i32ProfileIdx].profileId, BTRMGR_A2DP_SINK_PROFILE_ID) == 0) {
                        BTRMGRLOG_INFO ("Streaming to Device  = %lld\n", lDeviceHandle);
                        if (btrMgr_StartAudioStreamingOut(0, lDeviceHandle, aenBTRMgrDevConT, 1, 1, 1) != eBTRMgrSuccess) {
                            BTRMGRLOG_ERROR ("btrMgr_StartAudioStreamingOut - Failure\n");
                            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
                        }
                    }
                }
            }
        }

        gIsAudOutStartupInProgress = 0;
    }


    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_StartAudioStreamingOut (
    unsigned char                   aui8AdapterIdx,
    BTRMgrDeviceHandle              ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t    streamOutPref
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (btrMgr_StartAudioStreamingOut(aui8AdapterIdx, ahBTRMgrDevHdl, streamOutPref, 0, 0, 0) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Failure\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_StopAudioStreamingOut (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    eBTRMgrRet      lenBtrMgrRet    = eBTRMgrSuccess;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (ghBTRMgrDevHdlCurStreaming != ahBTRMgrDevHdl) {
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if ((lenBtrMgrRet = btrMgr_StopCastingAudio()) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("btrMgr_StopCastingAudio = %d\n", lenBtrMgrRet);
    }

    if (gIsDeviceConnected) { 
       BTRCore_ReleaseDeviceDataPath (ghBTRCoreHdl, ghBTRMgrDevHdlCurStreaming, enBTRCoreSpeakers);
    }

    ghBTRMgrDevHdlCurStreaming = 0;

    if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
        free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
        gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
    }

    /* We had Reset the ghBTRMgrDevHdlCurStreaming to avoid recursion/looping; so no worries */
    lenBtrMgrResult = BTRMGR_DisconnectFromDevice(aui8AdapterIdx, ahBTRMgrDevHdl);

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_IsAudioStreamingOut (
    unsigned char   aui8AdapterIdx,
    unsigned char*  pStreamingStatus
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if(!pStreamingStatus) {
        lenBtrMgrResult = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        if (ghBTRMgrDevHdlCurStreaming)
            *pStreamingStatus = 1;
        else
            *pStreamingStatus = 0;

        BTRMGRLOG_INFO ("BTRMGR_IsAudioStreamingOut: Returned status Successfully\n");
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_SetAudioStreamingOutType (
    unsigned char           aui8AdapterIdx,
    BTRMGR_StreamOut_Type_t type
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    BTRMGRLOG_ERROR ("Secondary audio support is not implemented yet. Always primary audio is played for now\n");
    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_StartAudioStreamingIn (
    unsigned char                   aui8AdapterIdx,
    BTRMgrDeviceHandle              ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t    connectAs
) {
    BTRMGR_Result_t     lenBtrMgrResult   = BTRMGR_RESULT_SUCCESS;
    BTRMGR_DeviceType_t lenBtrMgrDevType  = BTRMGR_DEVICE_TYPE_UNKNOWN;
    eBTRMgrRet          lenBtrMgrRet      = eBTRMgrSuccess;
    enBTRCoreRet        lenBtrCoreRet     = enBTRCoreSuccess;
    enBTRCoreDeviceType lenBtrCoreDevType = enBTRCoreUnknown;
    stBTRCorePairedDevicesCount listOfPDevices;
    int i = 0;
    int i32IsFound = 0;
    int i32DeviceFD = 0;
    int i32DeviceReadMTU = 0;
    int i32DeviceWriteMTU = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (ghBTRMgrDevHdlCurStreaming == ahBTRMgrDevHdl) {
        BTRMGRLOG_WARN ("Its already streaming-in in this device.. Check the volume :)\n");
        return BTRMGR_RESULT_SUCCESS;
    }


    if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming != ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Its already streaming in. lets stop this and start on other device \n");

        lenBtrMgrResult = BTRMGR_StopAudioStreamingIn(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming);
        if (lenBtrMgrResult != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
            return lenBtrMgrResult;
        }
    }

    /* Check whether the device is in the paired list */
    memset(&listOfPDevices, 0, sizeof(listOfPDevices));
    if (BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &listOfPDevices) != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get the paired devices list\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (!listOfPDevices.numberOfDevices) {
        BTRMGRLOG_ERROR ("No device is paired yet; Will not be able to play at this moment\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    for (i = 0; i < listOfPDevices.numberOfDevices; i++) {
        if (ahBTRMgrDevHdl == listOfPDevices.devices[i].tDeviceId) {
            i32IsFound = 1;
            break;
        }
    }

    if (!i32IsFound) {
        BTRMGRLOG_ERROR ("Failed to find this device in the paired devices list\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    lenBtrMgrDevType = btrMgr_MapDeviceTypeFromCore(listOfPDevices.devices[i].enDeviceType);
    if (lenBtrMgrDevType == BTRMGR_DEVICE_TYPE_SMARTPHONE) {
       lenBtrCoreDevType = enBTRCoreMobileAudioIn;
    }
    else if (lenBtrMgrDevType == BTRMGR_DEVICE_TYPE_TABLET) {
       lenBtrCoreDevType = enBTRCorePCAudioIn; 
    }

    if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
        free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
        gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
    }

    gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaSbcInfo) > sizeof(stBTRCoreDevMediaMpegInfo)) ? sizeof(stBTRCoreDevMediaSbcInfo) : sizeof(stBTRCoreDevMediaMpegInfo));

    lenBtrCoreRet = BTRCore_GetDeviceMediaInfo(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, lenBtrCoreDevType, &gstBtrCoreDevMediaInfo);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (gstBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
            BTRMGRLOG_INFO ("DevMedInfo SFreq         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
            BTRMGRLOG_INFO ("DevMedInfo AChan         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
            BTRMGRLOG_INFO ("DevMedInfo SbcAllocMethod= %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
            BTRMGRLOG_INFO ("DevMedInfo SbcSubbands   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
            BTRMGRLOG_INFO ("DevMedInfo SbcBlockLength= %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
            BTRMGRLOG_INFO ("DevMedInfo SbcMinBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
            BTRMGRLOG_INFO ("DevMedInfo SbcMaxBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
            BTRMGRLOG_INFO ("DevMedInfo SbcFrameLen   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
            BTRMGRLOG_DEBUG("DevMedInfo SbcBitrate    = %d\n", ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
        }
    }

    /* Aquire Device Data Path to start audio reception */
    lenBtrCoreRet = BTRCore_AcquireDeviceDataPath (ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, lenBtrCoreDevType, &i32DeviceFD, &i32DeviceReadMTU, &i32DeviceWriteMTU);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if ((lenBtrMgrRet = btrMgr_StartReceivingAudio(i32DeviceFD, i32DeviceReadMTU, ((stBTRCoreDevMediaSbcInfo*)(gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq)) == eBTRMgrSuccess) {
            ghBTRMgrDevHdlCurStreaming = listOfPDevices.devices[i].tDeviceId;
            BTRMGRLOG_INFO("Audio Reception Started.. Enjoy the show..! :)\n");
        }
        else {
            BTRMGRLOG_ERROR ("Failed to read audio now\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }
    else {
        BTRMGRLOG_ERROR ("Failed to get Device Data Path. So Will not be able to stream now\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (BTRMGR_RESULT_SUCCESS == lenBtrMgrResult && enBTRCoreSuccess != BTRCore_ReportMediaPosition (ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, lenBtrCoreDevType)) {
        BTRMGRLOG_ERROR ("Failed to set BTRCore report media position info!!!");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    
    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_StopAudioStreamingIn (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    eBTRMgrRet      lenBtrMgrRet    = eBTRMgrSuccess;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (ghBTRMgrDevHdlCurStreaming != ahBTRMgrDevHdl) {
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if ((lenBtrMgrRet = btrMgr_StopReceivingAudio()) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("btrMgr_StopReceivingAudio = %d\n", lenBtrMgrRet);
    }

    // TODO : determine enBTRCoreDeviceType from get Paired dev list
    BTRCore_ReleaseDeviceDataPath (ghBTRCoreHdl, ghBTRMgrDevHdlCurStreaming, enBTRCoreMobileAudioIn);

    ghBTRMgrDevHdlCurStreaming = 0;

    if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
        free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
        gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_IsAudioStreamingIn (
    unsigned char   aui8AdapterIdx,
    unsigned char*  pStreamingStatus
) {
    BTRMGR_Result_t lenBtrMgrRet = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pStreamingStatus)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if (ghBTRMgrDevHdlCurStreaming)
        *pStreamingStatus = 1;
    else
        *pStreamingStatus = 0;

    BTRMGRLOG_INFO ("BTRMGR_IsAudioStreamingIn: Returned status Successfully\n");

    return lenBtrMgrRet;
}

BTRMGR_Result_t
BTRMGR_SetEventResponse (
    unsigned char           aui8AdapterIdx,
    BTRMGR_EventResponse_t* apstBTRMgrEvtRsp
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!apstBTRMgrEvtRsp)) {
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
            BTRMGR_DeviceOperationType_t    stream_pref = BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT;
            lenBtrMgrResult = BTRMGR_StartAudioStreamingIn(aui8AdapterIdx, apstBTRMgrEvtRsp->m_deviceHandle, stream_pref);   
        }
        break;
    case BTRMGR_EVENT_DEVICE_FOUND:
        break;
    case BTRMGR_EVENT_MAX:
    default:
        break;
    }


    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_MediaControl (
    unsigned char                 aui8AdapterIdx,
    BTRMgrDeviceHandle            ahBTRMgrDevHdl,
    BTRMGR_MediaControlCommand_t  mediaCtrlCmd
) {
    BTRMGR_Result_t                 lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    BTRMGR_ConnectedDevicesList_t   listOfCDevices;
    enBTRCoreMediaCtrl              aenBTRCoreMediaCtrl = 0;
    enBTRCoreDeviceType             lenBtrCoreDevType = enBTRCoreUnknown;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    /* Check whether the device is in the Connected list */
    BTRMGR_GetConnectedDevices (aui8AdapterIdx, &listOfCDevices);
    /* As the stack allows only single dev conn for now  */
    if (listOfCDevices.m_deviceProperty[0].m_deviceHandle != ahBTRMgrDevHdl) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }
    /* Can implement a reverse mapping when our dev class usecases grow later point of time */
    if (listOfCDevices.m_deviceProperty[0].m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE) {
       lenBtrCoreDevType = enBTRCoreMobileAudioIn;
    }
    else if (listOfCDevices.m_deviceProperty[0].m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
       lenBtrCoreDevType = enBTRCorePCAudioIn;
    }

    switch (mediaCtrlCmd) {
    case BTRMGR_MEDIA_CTRL_PLAY:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlPlay;
        break;
    case BTRMGR_MEDIA_CTRL_PAUSE:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlPause;
        break;
    case BTRMGR_MEDIA_CTRL_STOP:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlStop;
        break;
    case BTRMGR_MEDIA_CTRL_NEXT:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlNext;
        break;
    case BTRMGR_MEDIA_CTRL_PREVIOUS:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlPrevious;
        break;
    case BTRMGR_MEDIA_CTRL_FASTFORWARD:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlFastForward;
        break;
    case BTRMGR_MEDIA_CTRL_REWIND:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRewind;
        break;
    case BTRMGR_MEDIA_CTRL_VOLUMEUP:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlVolumeUp;
        break;
    case BTRMGR_MEDIA_CTRL_VOLUMEDOWN:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlVolumeDown;
        break;
    }

    if (enBTRCoreSuccess != BTRCore_MediaControl(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBtrCoreDevType, aenBTRCoreMediaCtrl)) {
        BTRMGRLOG_ERROR ("Media Control Command for %llu Failed!!!\n", ahBTRMgrDevHdl);
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_GetMediaTrackInfo (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMGR_MediaTrackInfo_t      *mediaTrackInfo
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    BTRMGR_ConnectedDevicesList_t listOfCDevices;
    enBTRCoreDeviceType lenBtrCoreDevType = enBTRCoreUnknown;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    /* Check whether the device is in the Connected list */
    BTRMGR_GetConnectedDevices (aui8AdapterIdx, &listOfCDevices);
    /* As the stack allows only single dev conn for now  */
    if (listOfCDevices.m_deviceProperty[0].m_deviceHandle != ahBTRMgrDevHdl) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }
    /* Can implement a reverse mapping when our dev class usecases grow later point of time */
    if (listOfCDevices.m_deviceProperty[0].m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE) {
       lenBtrCoreDevType = enBTRCoreMobileAudioIn;
    }
    else if (listOfCDevices.m_deviceProperty[0].m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
       lenBtrCoreDevType = enBTRCorePCAudioIn;
    }


    if (enBTRCoreSuccess != BTRCore_GetMediaTrackInfo(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBtrCoreDevType, (stBTRCoreMediaTrackInfo*)mediaTrackInfo)) {
       BTRMGRLOG_ERROR ("Get Media Track Information for %llu Failed!!!\n", ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_GetMediaCurrentPosition (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMGR_MediaPositionInfo_t  *mediaPositionInfo
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    BTRMGR_ConnectedDevicesList_t listOfCDevices;
    enBTRCoreDeviceType lenBtrCoreDevType = enBTRCoreUnknown;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    /* Check whether the device is in the Connected list */ 
    BTRMGR_GetConnectedDevices (aui8AdapterIdx, &listOfCDevices);
    /* As the stack allows only single dev conn for now  */
    if (listOfCDevices.m_deviceProperty[0].m_deviceHandle != ahBTRMgrDevHdl) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }
    /* Can implement a reverse mapping when our dev class usecases grow later point of time */
    if (listOfCDevices.m_deviceProperty[0].m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE) {
       lenBtrCoreDevType = enBTRCoreMobileAudioIn;
    }
    else if (listOfCDevices.m_deviceProperty[0].m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
       lenBtrCoreDevType = enBTRCorePCAudioIn;
    }


    if (enBTRCoreSuccess != BTRCore_GetMediaPositionInfo(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBtrCoreDevType, (stBTRCoreMediaPositionInfo*)mediaPositionInfo)) {
       BTRMGRLOG_ERROR ("Get Media Current Position for %llu Failed!!!\n", ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_GetLeCharacteristicUUID (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    const char*                  apBtrServiceUuid,
    char*                        apBtrCharUuidList // should check on having enum which indicate the Charr UUID mode (read/write/notify/..
                                                   // to obtain the specific UUID
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    //BTRMGR_DevicesProperty_t   lBtrLeDevProp;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }
    // Check device
    //BTRMGR_GetDeviceProperties(aui8AdapterIdx, ahBTRMgrDevHdl, lBtrLeDevProp)

    if (enBTRCoreSuccess != BTRCore_GetLEProperty(ghBTRCoreHdl, ahBTRMgrDevHdl, apBtrServiceUuid, enBTRCoreLePropGCharList, (void*)apBtrCharUuidList)) {
       BTRMGRLOG_ERROR ("Get LE Char UUID for device  %llu Failed!!!\n", ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_PerformLeOp (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    const char*                  aBtrLeUuid,
    BTRMGR_LeOp_t                aLeOpType,
    void*                        rOpResult
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    //BTRMGR_DevicesProperty_t   lBtrLeDevProp;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    // Check if device is connected
    /*BTRMGR_GetDeviceProperties(aui8AdapterIdx, ahBTRMgrDevHdl, lBtrLeDevProp)

    if (lBtrLeDevProp.m_isConnected) {
       BTRMGRLOG_ERROR ("Device %lld not connected to perform LE Op!!!\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_GENERIC_FAILURE;
    }*/

    enBTRCoreLeOp aenBTRCoreLeOp =  enBTRCoreLeOpUnknown;

    switch (aLeOpType) {
    case BTRMGR_LE_OP_READ_VALUE:
        aenBTRCoreLeOp = enBTRCoreLeOpGReadValue;
        break;
    case BTRMGR_LE_OP_WRITE_VALUE:
        aenBTRCoreLeOp = enBTRCoreLeOpGWriteValue;
        break;
    case BTRMGR_LE_OP_START_NOTIFY:
        aenBTRCoreLeOp = enBTRCoreLeOpGStartNotify;
        break;
    case BTRMGR_LE_OP_STOP_NOTIFY:
        aenBTRCoreLeOp = enBTRCoreLeOpGStopNotify;
        break;
    }

    if (enBTRCoreSuccess != BTRCore_PerformLEOp (ghBTRCoreHdl, ahBTRMgrDevHdl, aBtrLeUuid, aenBTRCoreLeOp, NULL, rOpResult)) {
       BTRMGRLOG_ERROR ("Perform LE Op %d for device  %llu Failed!!!\n", aLeOpType, ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}



const char*
BTRMGR_GetDeviceTypeAsString (
    BTRMGR_DeviceType_t  type
) {
    if (type == BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET)
        return "WEARABLE HEADSET";
    else if (type == BTRMGR_DEVICE_TYPE_HANDSFREE)
        return "HANDSFREE";
    else if (type == BTRMGR_DEVICE_TYPE_MICROPHONE)
        return "MICROPHONE";
    else if (type == BTRMGR_DEVICE_TYPE_LOUDSPEAKER)
        return "LOUDSPEAKER";
    else if (type == BTRMGR_DEVICE_TYPE_HEADPHONES)
        return "HEADPHONES";
    else if (type == BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO)
        return "PORTABLE AUDIO DEVICE";
    else if (type == BTRMGR_DEVICE_TYPE_CAR_AUDIO)
        return "CAR AUDIO";
    else if (type == BTRMGR_DEVICE_TYPE_STB)
        return "STB";
    else if (type == BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)
        return "HIFI AUDIO DEVICE";
    else if (type == BTRMGR_DEVICE_TYPE_VCR)
        return "VCR";
    else if (type == BTRMGR_DEVICE_TYPE_VIDEO_CAMERA)
        return "VIDEO CAMERA";
    else if (type == BTRMGR_DEVICE_TYPE_CAMCODER)
        return "CAMCODER";
    else if (type == BTRMGR_DEVICE_TYPE_VIDEO_MONITOR)
        return "VIDEO MONITOR";
    else if (type == BTRMGR_DEVICE_TYPE_TV)
        return "TV";
    else if (type == BTRMGR_DEVICE_TYPE_VIDEO_CONFERENCE)
        return "VIDEO CONFERENCING";
    else if (type == BTRMGR_DEVICE_TYPE_SMARTPHONE)
        return "SMARTPHONE";
    else if (type == BTRMGR_DEVICE_TYPE_TABLET)
        return "TABLET";
    else
        return "UNKNOWN DEVICE";
}


// Outgoing callbacks Registration Interfaces
BTRMGR_Result_t
BTRMGR_RegisterEventCallback (
    BTRMGR_EventCallback    afpcBBTRMgrEventOut
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!afpcBBTRMgrEventOut) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    gfpcBBTRMgrEventOut = afpcBBTRMgrEventOut;
    BTRMGRLOG_INFO ("BTRMGR_RegisterEventCallback : Success\n");

    return lenBtrMgrResult;
}


/*  Incoming Callbacks */
static eBTRMgrRet
btrMgr_ACDataReadyCb (
    void*           apvAcDataBuf,
    unsigned int    aui32AcDataLen,
    void*           apvUserData
) {
    eBTRMgrRet              leBtrMgrAcRet       = eBTRMgrSuccess;
    stBTRMgrStreamingInfo*  lstBTRMgrStrmInfo   = (stBTRMgrStreamingInfo*)apvUserData; 

    if (lstBTRMgrStrmInfo) {
        if ((leBtrMgrAcRet = BTRMgr_SO_SendBuffer(lstBTRMgrStrmInfo->hBTRMgrSoHdl, apvAcDataBuf, aui32AcDataLen)) != eBTRMgrSuccess) {
            BTRMGRLOG_ERROR ("cbBufferReady: BTRMgr_SO_SendBuffer FAILED\n");
        }
    }

    lstBTRMgrStrmInfo->bytesWritten += aui32AcDataLen;

    return leBtrMgrAcRet;
}


static eBTRMgrRet
btrMgr_SOStatusCb (
    stBTRMgrMediaStatus*    apstBtrMgrSoStatus,
    void*                   apvUserData
) {
    eBTRMgrRet              leBtrMgrSoRet = eBTRMgrSuccess;
    stBTRMgrStreamingInfo*  lpstBTRMgrStrmInfo   = (stBTRMgrStreamingInfo*)apvUserData; 

    if (lpstBTRMgrStrmInfo && apstBtrMgrSoStatus) {
        BTRMGRLOG_DEBUG ("Entering\n");

        //TODO: Not happy that we are doing in the context of the callback.
        //      If possible move to a task thread
        //TODO: Rather than giving up on Streaming Out altogether, think about a retry implementation
        if (apstBtrMgrSoStatus->eBtrMgrState == eBTRMgrStateError) {
            if (ghBTRMgrDevHdlCurStreaming && lpstBTRMgrStrmInfo->hBTRMgrSoHdl) { /* Connected device. AC extablished; Release and Disconnect it */
                BTRMGRLOG_ERROR ("Error - ghBTRMgrDevHdlCurStreaming = %lld\n", ghBTRMgrDevHdlCurStreaming);
                if (ghBTRMgrDevHdlCurStreaming) { /* The streaming is happening; stop it */
#if 0
                    //TODO: DONT ENABLE Just a Reference of what we are trying to acheive
                    if (BTRMGR_StopAudioStreamingOut(0, ghBTRMgrDevHdlCurStreaming) != BTRMGR_RESULT_SUCCESS) {
                        BTRMGRLOG_ERROR ("Streamout is failed to stop\n");
                        leBtrMgrSoRet = eBTRMgrFailure;
                    }
#endif
                }
            }
        }
    }

    return leBtrMgrSoRet;
}


static eBTRMgrRet
btrMgr_SIStatusCb (
    stBTRMgrMediaStatus*    apstBtrMgrSiStatus,
    void*                   apvUserData
) {
    eBTRMgrRet              leBtrMgrSiRet = eBTRMgrSuccess;
    stBTRMgrStreamingInfo*  lpstBTRMgrStrmInfo   = (stBTRMgrStreamingInfo*)apvUserData; 

    if (lpstBTRMgrStrmInfo && apstBtrMgrSiStatus) {
        BTRMGRLOG_DEBUG ("Entering\n");

        //TODO: Not happy that we are doing in the context of the callback.
        //      If possible move to a task thread
        //TODO: Rather than giving up on Streaming In altogether, think about a retry implementation
        if (apstBtrMgrSiStatus->eBtrMgrState == eBTRMgrStateError) {
            if (ghBTRMgrDevHdlCurStreaming && lpstBTRMgrStrmInfo->hBTRMgrSiHdl) { /* Connected device. AC extablished; Release and Disconnect it */
                BTRMGRLOG_ERROR ("Error - ghBTRMgrDevHdlCurStreaming = %lld\n", ghBTRMgrDevHdlCurStreaming);
                if (ghBTRMgrDevHdlCurStreaming) { /* The streaming is happening; stop it */
#if 0
                    //TODO: DONT ENABLE Just a Reference of what we are trying to acheive
                    if (BTRMGR_StopAudioStreamingIn(0, ghBTRMgrDevHdlCurStreaming) != BTRMGR_RESULT_SUCCESS) {
                        BTRMGRLOG_ERROR ("Streamin is failed to stop\n");
                        leBtrMgrSiRet = eBTRMgrFailure;
                    }
#endif
                }
            }
        }
    }

    return leBtrMgrSiRet;
}


static enBTRCoreRet
btrMgr_DeviceStatusCb (
    stBTRCoreDevStatusCBInfo*   p_StatusCB,
    void*                       apvUserData
) {
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
    BTRMGR_EventMessage_t   lstEventMessage;

    memset (&lstEventMessage, 0, sizeof(lstEventMessage));

    BTRMGRLOG_INFO ("Received status callback\n");

    if ((p_StatusCB) && (gfpcBBTRMgrEventOut)) {

        switch (p_StatusCB->eDeviceCurrState) {
        case enBTRCoreDevStInitialized:
            break;
        case enBTRCoreDevStConnecting:
            break;
        case enBTRCoreDevStConnected:               /*  notify user device back   */
            if (enBTRCoreDevStLost == p_StatusCB->eDevicePrevState || enBTRCoreDevStPaired == p_StatusCB->eDevicePrevState) {
                if (!gIsAudOutStartupInProgress) {
                    btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_FOUND);  
                    gfpcBBTRMgrEventOut(lstEventMessage);  /* Post a callback */
                }
            }
            else if (enBTRCoreDevStInitialized != p_StatusCB->eDevicePrevState) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE);  

                if ((lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HANDSFREE) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_LOUDSPEAKER) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HEADPHONES) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_CAR_AUDIO) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)) {
                    gfpcBBTRMgrEventOut(lstEventMessage);  /* Post a callback */
                }
            }
            break;
        case enBTRCoreDevStDisconnected:
            btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE);
            gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */

            if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming == p_StatusCB->deviceId)) {
                /* update the flags as the device is NOT Connected */
                gIsDeviceConnected = 0;

                BTRMGRLOG_INFO ("lstEventMessage.m_pairedDevice.m_deviceType = %d\n", lstEventMessage.m_pairedDevice.m_deviceType);
                if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                    lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
                    /* Stop the playback which already stopped internally but to free up the memory */
                    BTRMGR_StopAudioStreamingIn(0, ghBTRMgrDevHdlCurStreaming);
                }
                else {
                    /* Stop the playback which already stopped internally but to free up the memory */
                    BTRMGR_StopAudioStreamingOut(0, ghBTRMgrDevHdlCurStreaming);
                }
            }
            break;
        case enBTRCoreDevStLost:
            if( !gIsUserInitiated ) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OUT_OF_RANGE);
                gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */

                if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming == p_StatusCB->deviceId)) {
                    /* update the flags as the device is NOT Connected */
                    gIsDeviceConnected = 0;

                    BTRMGRLOG_INFO ("lstEventMessage.m_pairedDevice.m_deviceType = %d\n", lstEventMessage.m_pairedDevice.m_deviceType);
                    if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                        lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        BTRMGR_StopAudioStreamingIn(0, ghBTRMgrDevHdlCurStreaming);
                    }
                    else {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        BTRMGR_StopAudioStreamingOut (0, ghBTRMgrDevHdlCurStreaming);
                    }
                }
            }
            gIsUserInitiated = 0;
            break;
        case enBTRCoreDevStPlaying:
            if (btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_TABLET) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST);
                gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
            }
            break;
        default:
            break;
        }
    }

    return lenBtrCoreRet;
}


static enBTRCoreRet
btrMgr_DeviceDiscoveryCb (
    stBTRCoreBTDevice   devicefound,
    void*               apvUserData
) {
    enBTRCoreRet        lenBtrCoreRet   = enBTRCoreSuccess;

    if (btrMgr_GetDiscoveryInProgress() && (gfpcBBTRMgrEventOut)) {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        btrMgr_MapDevstatusInfoToEventInfo ((void*)&devicefound, &lstEventMessage, BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE);
        gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
    }

    return lenBtrCoreRet;
}


static enBTRCoreRet
btrMgr_ConnectionInIntimationCb (
    stBTRCoreConnCBInfo*    apstConnCbInfo,
    int*                    api32ConnInIntimResp,
    void*                   apvUserData
) {
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;

     if (!apstConnCbInfo) {
        BTRMGRLOG_ERROR ("Invaliid argument\n");
        return enBTRCoreInvalidArg;
    }


    if (apstConnCbInfo->ui32devPassKey) {
        BTRMGRLOG_ERROR ("Incoming Connection passkey = %6d\n", apstConnCbInfo->ui32devPassKey);
    }

    if (gfpcBBTRMgrEventOut) {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST);  
        gfpcBBTRMgrEventOut(lstEventMessage); /* Post a callback */
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
        *api32ConnInIntimResp = 1;
    }
    else {
        BTRMGRLOG_ERROR ("Pin-Passkey Rejected\n");
        gAcceptConnection = 0;  //reset variabhle for the next connection
        *api32ConnInIntimResp = 0;
    }

    return lenBtrCoreRet;
}


static enBTRCoreRet
btrMgr_ConnectionInAuthenticationCb (
    stBTRCoreConnCBInfo*    apstConnCbInfo,
    int*                    api32ConnInAuthResp,
    void*                   apvUserData
) {
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;

    if (!apstConnCbInfo) {
        BTRMGRLOG_ERROR ("Invaliid argument\n");
        return enBTRCoreInvalidArg;
    }
    

    if (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_SmartPhone ||
        apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_Tablet) {
        if (gfpcBBTRMgrEventOut) {
            BTRMGR_EventMessage_t lstEventMessage;
            memset (&lstEventMessage, 0, sizeof(lstEventMessage));

            btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);  
            gfpcBBTRMgrEventOut(lstEventMessage);     /* Post a callback */
        }

        
        {   /* Max 15 sec timeout - Polled at 500ms second interval */
            unsigned int ui32sleepIdx = 30;

            do {
                usleep(500000);
            } while ((gEventRespReceived == 0) && (--ui32sleepIdx));

            gEventRespReceived = 0;
        }

        BTRMGRLOG_ERROR ("you picked %d\n", gAcceptConnection);
        if (gAcceptConnection == 1) {
            BTRMGRLOG_WARN ("Incoming Connection accepted\n");
            gAcceptConnection = 0;    //reset variabhle for the next connection
            *api32ConnInAuthResp = 1;
        }   
        else {
            BTRMGRLOG_ERROR ("Incoming Connection denied\n");
            gAcceptConnection = 0;    //reset variabhle for the next connection
            *api32ConnInAuthResp = 0;
        } 
    }
    else if ((apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_WearableHeadset)   ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_Loudspeaker)       ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HIFIAudioDevice)   ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_Headphones)) {

        BTRMGRLOG_WARN ("Incoming Connection from BT Speaker/Headset\n");
        if (btrMgr_GetDevPaired(apstConnCbInfo->stKnownDevice.tDeviceId) && (apstConnCbInfo->stKnownDevice.tDeviceId == ghBTRMgrDevHdlLastConnected)) {
            BTRMGRLOG_DEBUG ("Paired - Last Connected device...\n");

            //TODO: Check if XRE wants to bring up a Pop-up or Respond
            if (gfpcBBTRMgrEventOut) {
                BTRMGR_EventMessage_t lstEventMessage;
                memset (&lstEventMessage, 0, sizeof(lstEventMessage));

                btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);  
                gfpcBBTRMgrEventOut(lstEventMessage);     /* Post a callback */
            }

            {   /* Max 200msec timeout - Polled at 50ms second interval */
                unsigned int ui32sleepIdx = 4;

                do {
                    usleep(50000);
                } while ((gEventRespReceived == 0) && (--ui32sleepIdx));

                gEventRespReceived = 0;
            }

            BTRMGRLOG_WARN ("Incoming Connection accepted\n");
            *api32ConnInAuthResp = 1;
        }
        else {
            BTRMGRLOG_ERROR ("Incoming Connection denied\n");
            *api32ConnInAuthResp = 0;
        }
    }

    return lenBtrCoreRet;
}


static enBTRCoreRet
btrMgr_MediaStatusCb (
    stBTRCoreMediaStatusCBInfo*  mediaStatusCB,
    void*                        apvUserData
) {
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
    BTRMGR_EventMessage_t   lstEventMessage;

    memset (&lstEventMessage, 0, sizeof(lstEventMessage));

    BTRMGRLOG_INFO ("Received media status callback\n");

    if (mediaStatusCB) {
        stBTRCoreMediaStatusUpdate* mediaStatus = &mediaStatusCB->m_mediaStatusUpdate;

        lstEventMessage.m_mediaInfo.m_deviceHandle = mediaStatusCB->deviceId;
        lstEventMessage.m_mediaInfo.m_deviceType   = btrMgr_MapDeviceTypeFromCore(mediaStatusCB->eDeviceClass);
        strncpy (lstEventMessage.m_mediaInfo.m_name, mediaStatusCB->deviceName, BTRMGR_NAME_LEN_MAX);

        switch (mediaStatus->eBTMediaStUpdate) {
        case eBTRCoreMediaTrkStStarted:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_STARTED;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStPlaying:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_PLAYING;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStPaused:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_PAUSED;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStStopped:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_STOPPED;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkPosition:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_POSITION;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStChanged:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_CHANGED;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaTrackInfo, &mediaStatus->m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
            break;
        case eBTRCoreMediaPlaybackEnded:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED;
            //memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        default:
            break;
        }

        if (gfpcBBTRMgrEventOut) {
            gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
        }
    }

    return lenBtrCoreRet;
}

/* End of File */
