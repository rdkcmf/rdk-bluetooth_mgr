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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include "btrCore.h"

#include "btmgr.h"
#include "btrMgr_logger.h"

#ifndef BUILD_FOR_PI
#include "rfcapi.h"
#endif

#include "btrMgr_Types.h"
#include "btrMgr_mediaTypes.h"
#include "btrMgr_streamOut.h"
#include "btrMgr_audioCap.h"
#include "btrMgr_streamIn.h"
#include "btrMgr_persistIface.h"
#include "btrMgr_SysDiag.h"
#include "btrMgr_Columbo.h"
#include "btrMgr_LEOnboarding.h"

/* Private Macro definitions */
#define BTRMGR_SIGNAL_POOR       (-90)
#define BTRMGR_SIGNAL_FAIR       (-70)
#define BTRMGR_SIGNAL_GOOD       (-60)

#define BTRMGR_CONNECT_RETRY_ATTEMPTS       2
#define BTRMGR_DEVCONN_CHECK_RETRY_ATTEMPTS 3
#define BTRMGR_DEVCONN_PWRST_CHANGE_TIME    3

#define BTRMGR_DISCOVERY_HOLD_OFF_TIME      120
#define BTRTEST_LE_ONBRDG_ENABLE 1


// Move to private header ?
typedef struct _stBTRMgrStreamingInfo {
    tBTRMgrAcHdl            hBTRMgrAcHdl;
    tBTRMgrSoHdl            hBTRMgrSoHdl;
    tBTRMgrSoHdl            hBTRMgrSiHdl;
    BTRMGR_StreamOut_Type_t tBTRMgrSoType;
    unsigned long           bytesWritten;
    unsigned                samplerate;
    unsigned                channels;
    unsigned                bitsPerSample;
    int                     i32BytesToEncode;
} stBTRMgrStreamingInfo;

typedef enum _BTRMGR_DiscoveryState_t {
    BTRMGR_DISCOVERY_ST_UNKNOWN,
    BTRMGR_DISCOVERY_ST_STARTED,
    BTRMGR_DISCOVERY_ST_PAUSED,
    BTRMGR_DISCOVERY_ST_RESUMED,
    BTRMGR_DISCOVERY_ST_STOPPED,
} BTRMGR_DiscoveryState_t;

typedef enum _enBTRMGRStartupAudio {
    BTRMGR_STARTUP_AUD_INPROGRESS,
    BTRMGR_STARTUP_AUD_SKIPPED,
    BTRMGR_STARTUP_AUD_COMPLETED,
    BTRMGR_STARTUP_AUD_UNKNOWN,
} enBTRMGRStartupAudio;

typedef struct _BTRMGR_DiscoveryHandle_t {
    BTRMGR_DeviceOperationType_t    m_devOpType;
    BTRMGR_DiscoveryState_t         m_disStatus;
    BTRMGR_DiscoveryFilterHandle_t  m_disFilter;
} BTRMGR_DiscoveryHandle_t;

//TODO: Move to a local handle. Mutex protect all
static tBTRCoreHandle                   ghBTRCoreHdl                = NULL;
static tBTRMgrPIHdl                     ghBTRMgrPiHdl               = NULL;
static tBTRMgrSDHdl                     ghBTRMgrSdHdl               = NULL;
static BTRMgrDeviceHandle               ghBTRMgrDevHdlLastConnected = 0;
static BTRMgrDeviceHandle               ghBTRMgrDevHdlCurStreaming  = 0;
static BTRMGR_DiscoveryHandle_t         ghBTRMgrDiscoveryHdl;
static BTRMGR_DiscoveryHandle_t         ghBTRMgrBgDiscoveryHdl;

static stBTRCoreAdapter                 gDefaultAdapterContext;
static stBTRCoreListAdapters            gListOfAdapters;
static stBTRCoreDevMediaInfo            gstBtrCoreDevMediaInfo;
static BTRMGR_DiscoveredDevicesList_t   gListOfDiscoveredDevices;
static BTRMGR_PairedDevicesList_t       gListOfPairedDevices;
static stBTRMgrStreamingInfo            gstBTRMgrStreamingInfo;

static unsigned char                    gui8IsSoDevAvrcpSupported   = 0;
static unsigned char                    gIsLeDeviceConnected        = 0;
static unsigned char                    gIsAgentActivated           = 0;
static unsigned char                    gEventRespReceived          = 0;
static unsigned char                    gAcceptConnection           = 0;
static unsigned char                    gIsUserInitiated            = 0;
static unsigned char                    gDiscHoldOffTimeOutCbData   = 0;
static unsigned char                    gConnPwrStChTimeOutCbData   = 0;
static unsigned char                    gIsAudioInEnabled           = 0;
static unsigned char                    gIsHidGamePadEnabled        = 0;
static volatile guint                   gTimeOutRef                 = 0;
static volatile guint                   gConnPwrStChangeTimeOutRef  = 0;
static volatile unsigned int            gIsAdapterDiscovering       = 0;

static BTRMGR_DeviceOperationType_t     gBgDiscoveryType            = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;
static BTRMGR_Events_t                  gMediaPlaybackStPrev        = BTRMGR_EVENT_MAX;
static enBTRMGRStartupAudio             gIsAudOutStartupInProgress  = BTRMGR_STARTUP_AUD_UNKNOWN;

static void*                            gpvMainLoop                 = NULL;
static void*                            gpvMainLoopThread           = NULL;

static BTRMGR_EventCallback             gfpcBBTRMgrEventOut         = NULL;
static char                             gLeReadOpResponse[BTRMGR_MAX_DEV_OP_DATA_LEN] = "\0";
static BOOLEAN                          gIsAdvertisementSet         = FALSE;
static BOOLEAN                          gIsDeviceAdvertising        = FALSE;
static BOOLEAN                          gIsDiscoveryOpInternal      = FALSE;

BTRMGR_LeCustomAdvertisement_t stCoreCustomAdv =
{
    0x02 ,
    0x01 ,
    0x06 ,
    0x05 ,
    0x03 ,
    0x0A ,
    0x18 ,
    0xB9 ,
    0xFD ,
    0x0B ,
    0xFF ,
    0xA3 ,
    0x07 ,
    0x0101,
    {
        0xC8,
        0xB3,
        0x73,
        0x32,
        0xEA,
        0x3D
    }
};

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif



/* Static Function Prototypes */
static inline unsigned char btrMgr_GetAdapterCnt (void);
static const char* btrMgr_GetAdapterPath (unsigned char aui8AdapterIdx);

static inline void btrMgr_SetAgentActivated (unsigned char aui8AgentActivated);
static inline unsigned char btrMgr_GetAgentActivated (void);
static void btrMgr_CheckAudioInServiceAvailability (void);
static void btrMgr_CheckHidGamePadServiceAvailability (void);

static const char* btrMgr_GetDiscoveryDeviceTypeAsString (BTRMGR_DeviceOperationType_t adevOpType);
//static const char* btrMgr_GetDiscoveryFilterAsString (BTRMGR_ScanFilter_t ascanFlt);
static const char* btrMgr_GetDiscoveryStateAsString (BTRMGR_DiscoveryState_t  aScanStatus);

static inline void btrMgr_SetDiscoveryHandle (BTRMGR_DeviceOperationType_t adevOpType, BTRMGR_DiscoveryState_t aScanStatus);
static inline void btrMgr_SetBgDiscoveryType (BTRMGR_DeviceOperationType_t adevOpType);
static inline void btrMgr_SetDiscoveryState (BTRMGR_DiscoveryHandle_t* ahdiscoveryHdl, BTRMGR_DiscoveryState_t aScanStatus);
static inline void btrMgr_SetDiscoveryDeviceType (BTRMGR_DiscoveryHandle_t*  ahdiscoveryHdl, BTRMGR_DeviceOperationType_t aeDevOpType);

static inline BTRMGR_DeviceOperationType_t btrMgr_GetBgDiscoveryType (void);
static inline BTRMGR_DiscoveryState_t btrMgr_GetDiscoveryState (BTRMGR_DiscoveryHandle_t* ahdiscoveryHdl);
static inline BTRMGR_DeviceOperationType_t btrMgr_GetDiscoveryDeviceType (BTRMGR_DiscoveryHandle_t*  ahdiscoveryHdl);
//static inline BTRMGR_DiscoveryFilterHandle_t* btrMgr_GetDiscoveryFilter (BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl);

static inline gboolean btrMgr_isTimeOutSet (void);
static inline void btrMgr_ClearDiscoveryHoldOffTimer(void);
static inline void btrMgr_SetDiscoveryHoldOffTimer(unsigned char aui8AdapterIdx);

//static eBTRMgrRet btrMgr_SetDiscoveryFilter (BTRMGR_DiscoveryHandle_t* ahdiscoveryHdl, BTRMGR_ScanFilter_t aeScanFilterType, void* aFilterValue);
//static eBTRMgrRet btrMgr_ClearDiscoveryFilter (BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl);

static BTRMGR_DiscoveryHandle_t* btrMgr_GetDiscoveryInProgress (void);

static BTRMGR_Result_t BTRMGR_StartDeviceDiscovery_Internal (unsigned char aui8AdapterIdx, BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT);
static BTRMGR_Result_t BTRMGR_StopDeviceDiscovery_Internal (unsigned char aui8AdapterIdx, BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT);
static BTRMGR_Result_t BTRMGR_GetDiscoveredDevices_Internal (unsigned char aui8AdapterIdx, BTRMGR_DiscoveredDevicesList_t* pDiscoveredDevices);

static eBTRMgrRet btrMgr_PauseDeviceDiscovery (unsigned char aui8AdapterIdx, BTRMGR_DiscoveryHandle_t* ahdiscoveryHdl);
static eBTRMgrRet btrMgr_ResumeDeviceDiscovery (unsigned char aui8AdapterIdx, BTRMGR_DiscoveryHandle_t* ahdiscoveryHdl);
static eBTRMgrRet btrMgr_StopDeviceDiscovery (unsigned char aui8AdapterIdx, BTRMGR_DiscoveryHandle_t* ahdiscoveryHdl);

static eBTRMgrRet btrMgr_PreCheckDiscoveryStatus (unsigned char aui8AdapterIdx, BTRMGR_DeviceOperationType_t aDevOpType);
static eBTRMgrRet btrMgr_PostCheckDiscoveryStatus (unsigned char aui8AdapterIdx, BTRMGR_DeviceOperationType_t aDevOpType);

static void btrMgr_GetPairedDevInfo (BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_PairedDevices_t* apBtMgrPairedDevInfo);
static void btrMgr_GetDiscoveredDevInfo (BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DiscoveredDevices_t* apBtMgrDiscoveredDevInfo);

static unsigned char btrMgr_GetDevPaired (BTRMgrDeviceHandle ahBTRMgrDevHdl);

static void btrMgr_SetDevConnected (BTRMgrDeviceHandle ahBTRMgrDevHdl, unsigned char aui8isDeviceConnected);
static unsigned char btrMgr_IsDevConnected (BTRMgrDeviceHandle ahBTRMgrDevHdl);

static unsigned char btrMgr_IsDevNameSameAsAddress (char* apcDeviceName, char* apcDeviceAddress, unsigned int ui32StrLen);
static unsigned char btrMgr_CheckIfDevicePrevDetected (BTRMgrDeviceHandle ahBTRMgrDevHdl);

static BTRMGR_DeviceType_t btrMgr_MapDeviceTypeFromCore (enBTRCoreDeviceClass device_type);
static BTRMGR_DeviceOperationType_t btrMgr_MapDeviceOpFromDeviceType (BTRMGR_DeviceType_t device_type);
static BTRMGR_RSSIValue_t btrMgr_MapSignalStrengthToRSSI (int signalStrength);
static eBTRMgrRet btrMgr_MapDevstatusInfoToEventInfo (void* p_StatusCB, BTRMGR_EventMessage_t* apstEventMessage, BTRMGR_Events_t type);

static eBTRMgrRet btrMgr_StartCastingAudio (int outFileFd, int outMTUSize, unsigned int outDevDelay, eBTRCoreDevMediaType aenBtrCoreDevOutMType, void* apstBtrCoreDevOutMCodecInfo);
static eBTRMgrRet btrMgr_StopCastingAudio (void);
static eBTRMgrRet btrMgr_SwitchCastingAudio_AC (BTRMGR_StreamOut_Type_t aenCurrentSoType);
static eBTRMgrRet btrMgr_StartReceivingAudio (int inFileFd, int inMTUSize, unsigned int inDevDelay, eBTRCoreDevMediaType aenBtrCoreDevInMType, void* apstBtrCoreDevInMCodecInfo);
static eBTRMgrRet btrMgr_StopReceivingAudio (void);

static eBTRMgrRet btrMgr_ConnectToDevice (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t connectAs, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx);

static eBTRMgrRet btrMgr_StartAudioStreamingOut (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t streamOutPref, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx, unsigned int aui32SleepIdx);

static eBTRMgrRet btrMgr_AddPersistentEntry(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, const char* apui8ProfileStr, int ai32DevConnected);
static eBTRMgrRet btrMgr_RemovePersistentEntry(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, const char* apui8ProfileStr);
static eBTRMgrRet btrMgr_MediaControl(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_MediaDeviceStatus_t* apstMediaDeviceStatus, enBTRCoreDeviceType aenBtrCoreDevTy, enBTRCoreDeviceClass aenBtrCoreDevCl, stBTRCoreMediaCtData *apstBtrCoreMediaCData);

#if 0
static void btrMgr_AddStandardAdvGattInfo(void);

static void btrMgr_AddColumboGATTInfo(void);
#endif

static BTRMGR_SysDiagChar_t btrMgr_MapUUIDtoDiagElement(char *aUUID);

/*  Local Op Threads Prototypes */
static gpointer btrMgr_g_main_loop_Task (gpointer appvMainLoop);


/* Incoming Callbacks Prototypes */
static gboolean btrMgr_DiscoveryHoldOffTimerCb (gpointer gptr);
static gboolean btrMgr_ConnPwrStChangeTimerCb (gpointer gptr);

static eBTRMgrRet btrMgr_ACDataReadyCb (void* apvAcDataBuf, unsigned int aui32AcDataLen, void* apvUserData);
static eBTRMgrRet btrMgr_ACStatusCb (stBTRMgrMediaStatus* apstBtrMgrAcStatus, void* apvUserData);
static eBTRMgrRet btrMgr_SOStatusCb (stBTRMgrMediaStatus* apstBtrMgrSoStatus, void* apvUserData);
static eBTRMgrRet btrMgr_SIStatusCb (stBTRMgrMediaStatus* apstBtrMgrSiStatus, void* apvUserData);
static eBTRMgrRet btrMgr_SDStatusCb (stBTRMgrSysDiagStatus* apstBtrMgrSdStatus, void* apvUserData);

static enBTRCoreRet btrMgr_DeviceStatusCb (stBTRCoreDevStatusCBInfo* p_StatusCB, void* apvUserData);
static enBTRCoreRet btrMgr_DeviceDiscoveryCb (stBTRCoreDiscoveryCBInfo* astBTRCoreDiscoveryCbInfo, void* apvUserData);
static enBTRCoreRet btrMgr_ConnectionInIntimationCb (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInIntimResp, void* apvUserData);
static enBTRCoreRet btrMgr_ConnectionInAuthenticationCb (stBTRCoreConnCBInfo* apstConnCbInfo, int* api32ConnInAuthResp, void* apvUserData);
static enBTRCoreRet btrMgr_MediaStatusCb (stBTRCoreMediaStatusCBInfo* mediaStatusCB, void* apvUserData);

#ifdef RDKTV_PERSIST_VOLUME_SKY
static eBTRMgrRet btrMgr_SetLastVolume(unsigned char aui8AdapterIdx, unsigned char ui8Volume);
static eBTRMgrRet btrMgr_GetLastVolume(unsigned char aui8AdapterIdx, unsigned char *pVolume);
static eBTRMgrRet btrMgr_SetLastMuteState(unsigned char aui8AdapterIdx, gboolean Mute);
static eBTRMgrRet btrMgr_GetLastMuteState(unsigned char aui8AdapterIdx, gboolean *pMute);
#endif

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

static inline void
btrMgr_SetAgentActivated (
    unsigned char aui8AgentActivated
) {
    gIsAgentActivated = aui8AgentActivated;
}

static inline unsigned char
btrMgr_GetAgentActivated (
    void
) {
    return gIsAgentActivated;
}


static void
btrMgr_CheckAudioInServiceAvailability (
    void
) {
#ifdef BUILD_FOR_PI
   //Since RFC is not enabled for PI devices, enabling by default
    gIsAudioInEnabled = 1;
    BTRMGRLOG_INFO ("Enabling BTR AudioIn Service for raspberry pi devices.\n");
#else
    RFC_ParamData_t param = {0};
    /* We shall make this api generic and macro defined tr181 params as we start to enable diff services based on RFC */
    WDMP_STATUS status = getRFCParameter("BTRMGR", "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BTR.AudioIn.Enable", &param);

    if (status == WDMP_SUCCESS) {
        BTRMGRLOG_DEBUG ("name = %s, type = %d, value = %s\n", param.name, param.type, param.value);

        if (!strncmp(param.value, "true", strlen("true"))) {
            gIsAudioInEnabled = 1;
            BTRMGRLOG_INFO ("BTR AudioIn Serivce will be available.\n");
        }
        else {
            BTRMGRLOG_INFO ("BTR AudioIn Serivce will not be available.\n");
        }
    }
    else {
        BTRMGRLOG_ERROR ("getRFCParameter Failed : %s\n", getRFCErrorString(status));
    }
#endif
}

static void
btrMgr_CheckHidGamePadServiceAvailability (
    void
) {
#ifdef BUILD_FOR_PI
   //Since RFC is not enabled for PI devices, enabling by default
    gIsHidGamePadEnabled = 1;
    BTRMGRLOG_INFO ("Enabling BTR HidGamePad Service for raspberry pi devices.\n");
#else
    RFC_ParamData_t param = {0};
    /* We shall make this api generic and macro defined tr181 params as we start to enable diff services based on RFC */
    WDMP_STATUS status = getRFCParameter("BTRMGR", "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BTR.GamePad.Enable", &param);

    if (status == WDMP_SUCCESS) {
        BTRMGRLOG_DEBUG ("name = %s, type = %d, value = %s\n", param.name, param.type, param.value);

        if (!strncmp(param.value, "true", strlen("true"))) {
            gIsHidGamePadEnabled = 1;
            BTRMGRLOG_INFO ("BTR HidGamePad Serivce will be available.\n");
        }
        else {
            BTRMGRLOG_INFO ("BTR HidGamePad Serivce will not be available.\n");
        }
    }
    else {
        BTRMGRLOG_ERROR ("getRFCParameter Failed : %s\n", getRFCErrorString(status));
    }
#endif
}

static const char*
btrMgr_GetDiscoveryDeviceTypeAsString (
    BTRMGR_DeviceOperationType_t    adevOpType
) {
    char* opType = NULL;

    switch (adevOpType) {
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT:
        opType = "AUDIO_OUT";
        break;
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT:
        opType = "AUDIO_IN";
        break;
    case BTRMGR_DEVICE_OP_TYPE_LE:
        opType = "LE";
        break;
    case BTRMGR_DEVICE_OP_TYPE_HID:
        opType = "HID";
        break;
    case BTRMGR_DEVICE_OP_TYPE_UNKNOWN:
        opType = "UNKNOWN";
    }

    return opType;
}

#if 0
static const char*
btrMgr_GetDiscoveryFilterAsString (
    BTRMGR_ScanFilter_t ascanFlt
) {
    char* filter = NULL;

    switch (ascanFlt) {
    case BTRMGR_DISCOVERY_FILTER_UUID:
        filter = "UUID";
        break;
    case BTRMGR_DISCOVERY_FILTER_RSSI:
        filter = "RSSI";
        break;
    case BTRMGR_DISCOVERY_FILTER_PATH_LOSS:
        filter = "PATH_LOSS";
        break;
    case BTRMGR_DISCOVERY_FILTER_SCAN_TYPE:
        filter = "SCAN_TYPE";
    }

    return filter;
}
#endif

static const char*
btrMgr_GetDiscoveryStateAsString (
    BTRMGR_DiscoveryState_t         aScanStatus
) {
    char* state = NULL;

    switch (aScanStatus) { 
    case BTRMGR_DISCOVERY_ST_STARTED:
        state = "ST_STARTED";
        break;
    case BTRMGR_DISCOVERY_ST_PAUSED:
        state = "ST_PAUSED";
        break;
    case BTRMGR_DISCOVERY_ST_RESUMED:
        state = "ST_RESUMED";
        break;
    case BTRMGR_DISCOVERY_ST_STOPPED:
        state = "ST_STOPPED";
        break;
    case BTRMGR_DISCOVERY_ST_UNKNOWN:
    default:
        state = "ST_UNKNOWN";
    }

    return state;
}

static inline void
btrMgr_SetBgDiscoveryType (
    BTRMGR_DeviceOperationType_t    adevOpType
) {
    gBgDiscoveryType = adevOpType;
}

static inline void
btrMgr_SetDiscoveryState (
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl,
    BTRMGR_DiscoveryState_t     aScanStatus
) {
    ahdiscoveryHdl->m_disStatus = aScanStatus;
}

static inline void
btrMgr_SetDiscoveryDeviceType (
    BTRMGR_DiscoveryHandle_t*       ahdiscoveryHdl,
    BTRMGR_DeviceOperationType_t    aeDevOpType
) {
    ahdiscoveryHdl->m_devOpType = aeDevOpType;
}

static inline BTRMGR_DeviceOperationType_t
btrMgr_GetBgDiscoveryType (
    void
) {
    return gBgDiscoveryType;
}

static inline BTRMGR_DiscoveryState_t
btrMgr_GetDiscoveryState (
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl
) {
    return ahdiscoveryHdl->m_disStatus;
}

static inline BTRMGR_DeviceOperationType_t
btrMgr_GetDiscoveryDeviceType (
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl
) {
    return ahdiscoveryHdl->m_devOpType;
}

static inline void
btrMgr_SetDiscoveryHandle (
    BTRMGR_DeviceOperationType_t    aDevOpType,
    BTRMGR_DiscoveryState_t         aScanStatus
) {
    BTRMGR_DiscoveryHandle_t*   ldiscoveryHdl = NULL;

    if (aDevOpType == btrMgr_GetBgDiscoveryType()) {
        ldiscoveryHdl = &ghBTRMgrBgDiscoveryHdl;
    }
    else {
        ldiscoveryHdl = &ghBTRMgrDiscoveryHdl;
    }


    if (btrMgr_GetDiscoveryState(ldiscoveryHdl) != BTRMGR_DISCOVERY_ST_PAUSED) {
        btrMgr_SetDiscoveryDeviceType (ldiscoveryHdl, aDevOpType);
        btrMgr_SetDiscoveryState (ldiscoveryHdl, aScanStatus);
        // set Filter in the handle from the global Filter
    }
}

#if 0
static inline BTRMGR_DiscoveryFilterHandle_t*
btrMgr_GetDiscoveryFilter (
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl
) {
    return &ahdiscoveryHdl->m_disFilter;
}
#endif

static inline gboolean
btrMgr_isTimeOutSet (
    void
) {
    return (gTimeOutRef > 0) ? TRUE : FALSE;
}

static inline void
btrMgr_ClearDiscoveryHoldOffTimer (
    void
) {
    if (gTimeOutRef) {
        BTRMGRLOG_DEBUG ("Cancelling previous Discovery hold off TimeOut Session : %u\n", gTimeOutRef);
        g_source_remove (gTimeOutRef);
        gTimeOutRef = 0;
    }
}

static inline void
btrMgr_SetDiscoveryHoldOffTimer (
    unsigned char   aui8AdapterIdx
) {
    gDiscHoldOffTimeOutCbData = aui8AdapterIdx;
    gTimeOutRef =  g_timeout_add_seconds (BTRMGR_DISCOVERY_HOLD_OFF_TIME, btrMgr_DiscoveryHoldOffTimerCb, (gpointer)&gDiscHoldOffTimeOutCbData);
    BTRMGRLOG_ERROR ("DiscoveryHoldOffTimeOut set to  +%u  seconds || TimeOutReference : %u\n", BTRMGR_DISCOVERY_HOLD_OFF_TIME, gTimeOutRef);
}

#if 0
static eBTRMgrRet
btrMgr_SetDiscoveryFilter (
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl,
    BTRMGR_ScanFilter_t         aeScanFilterType,
    void*                       aFilterValue
) {
    BTRMGR_DiscoveryFilterHandle_t* ldisFilter = btrMgr_GetDiscoveryFilter(ahdiscoveryHdl);

    if (btrMgr_GetDiscoveryState(ahdiscoveryHdl) != BTRMGR_DISCOVERY_ST_INITIALIZING){
        BTRMGRLOG_ERROR ("Not in Initializing state !!!. Current state is %s\n"
                        , btrMgr_GetDiscoveryStateAsString (btrMgr_GetDiscoveryState(ahdiscoveryHdl)));
        return eBTRMgrFailure;
    }

    switch (aeScanFilterType) {
    case BTRMGR_DISCOVERY_FILTER_UUID:
        ldisFilter->m_btuuid.m_uuid     = (char**) realloc (ldisFilter->m_btuuid.m_uuid, (sizeof(char*) * (++ldisFilter->m_btuuid.m_uuidCount)));
        ldisFilter->m_btuuid.m_uuid[ldisFilter->m_btuuid.m_uuidCount]   = (char*)  malloc  (BTRMGR_NAME_LEN_MAX);
        strncpy (ldisFilter->m_btuuid.m_uuid[ldisFilter->m_btuuid.m_uuidCount-1], (char*)aFilterValue, BTRMGR_NAME_LEN_MAX-1);
        break;
    case BTRMGR_DISCOVERY_FILTER_RSSI:
        ldisFilter->m_rssi      = *(short*)aFilterValue;
        break;
    case BTRMGR_DISCOVERY_FILTER_PATH_LOSS:
        ldisFilter->m_pathloss  = *(short*)aFilterValue;
        break;
    case BTRMGR_DISCOVERY_FILTER_SCAN_TYPE:
        ldisFilter->m_scanType  = *(BTRMGR_DeviceScanType_t*)aFilterValue;
    }

    BTRMGRLOG_DEBUG ("Discovery Filter is set successfully with the given %s...\n"
                    , btrMgr_GetDiscoveryFilterAsString(aeScanFilterType));

    return eBTRMgrSuccess;
}

static eBTRMgrRet
btrMgr_ClearDiscoveryFilter (
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl
) {
    BTRMGR_DiscoveryFilterHandle_t* ldisFilter = btrMgr_GetDiscoveryFilter(ahdiscoveryHdl);

    if (btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_INITIALIZED ||
        btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_STARTED     ||
        btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_RESUMED     ||
        btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_PAUSED      ){
        BTRMGRLOG_WARN ("Cannot clear Discovery Filter when Discovery is in %s\n"
                        , btrMgr_GetDiscoveryStateAsString(btrMgr_GetDiscoveryState(ahdiscoveryHdl)));
        return eBTRMgrFailure;
     }

    if (ldisFilter->m_btuuid.m_uuidCount) {
        while (ldisFilter->m_btuuid.m_uuidCount) {
            free (ldisFilter->m_btuuid.m_uuid[ldisFilter->m_btuuid.m_uuidCount-1]);
            ldisFilter->m_btuuid.m_uuid[ldisFilter->m_btuuid.m_uuidCount-1] = NULL;
            ldisFilter->m_btuuid.m_uuidCount--;
        }
        free (ldisFilter->m_btuuid.m_uuid);
    }

    ldisFilter->m_btuuid.m_uuid = NULL;
    ldisFilter->m_rssi          = 0;
    ldisFilter->m_pathloss      = 0;
    ldisFilter->m_scanType      = BTRMGR_DEVICE_SCAN_TYPE_AUTO;

    return eBTRMgrSuccess;
}
#endif

static BTRMGR_DiscoveryHandle_t*
btrMgr_GetDiscoveryInProgress (
    void
) {
    BTRMGR_DiscoveryHandle_t*   ldiscoveryHdl = NULL;

    if (btrMgr_GetDiscoveryState(&ghBTRMgrDiscoveryHdl) == BTRMGR_DISCOVERY_ST_STARTED ||
        btrMgr_GetDiscoveryState(&ghBTRMgrDiscoveryHdl) == BTRMGR_DISCOVERY_ST_RESUMED ){
        ldiscoveryHdl = &ghBTRMgrDiscoveryHdl;
    }
    else if (btrMgr_GetDiscoveryState(&ghBTRMgrBgDiscoveryHdl) == BTRMGR_DISCOVERY_ST_STARTED ||
             btrMgr_GetDiscoveryState(&ghBTRMgrBgDiscoveryHdl) == BTRMGR_DISCOVERY_ST_RESUMED ){
        ldiscoveryHdl = &ghBTRMgrBgDiscoveryHdl;
    }

    if (ldiscoveryHdl) {
        BTRMGRLOG_DEBUG ("[%s] Scan in Progress...\n"
                        , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl)));
    }

    return ldiscoveryHdl;
}

static eBTRMgrRet
btrMgr_PauseDeviceDiscovery (
    unsigned char               aui8AdapterIdx,
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl
) {
    eBTRMgrRet  lenBtrMgrRet   = eBTRMgrSuccess;

    if (btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_STARTED ||
        btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_RESUMED ){

        if (BTRMGR_RESULT_SUCCESS == BTRMGR_StopDeviceDiscovery_Internal (aui8AdapterIdx, btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl))) {

            btrMgr_SetDiscoveryState (ahdiscoveryHdl, BTRMGR_DISCOVERY_ST_PAUSED);
            BTRMGRLOG_DEBUG ("[%s] Successfully Paused Scan\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
        }
        else {
            BTRMGRLOG_ERROR ("[%s] Failed to Pause Scan\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
            lenBtrMgrRet =  eBTRMgrFailure;
        }
    }
    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_ResumeDeviceDiscovery (
    unsigned char               aui8AdapterIdx,
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl
) {
    eBTRMgrRet  lenBtrMgrRet   = eBTRMgrSuccess;

    if (btrMgr_GetDiscoveryState(ahdiscoveryHdl) != BTRMGR_DISCOVERY_ST_PAUSED) {
        BTRMGRLOG_WARN ("\n[%s] Device Discovery Resume is requested, but current state is %s !!!\n"
                        , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl))
                        , btrMgr_GetDiscoveryStateAsString (btrMgr_GetDiscoveryState(ahdiscoveryHdl)));
        BTRMGRLOG_WARN ("\n Still continuing to Resume Discovery\n");
    }
#if 0
    if (enBTRCoreSuccess != BTRCore_ApplyDiscoveryFilter (btrMgr_GetDiscoveryFilter(ahdiscoveryHdl))) {
        BTRMGRLOG_ERROR ("[%s] Failed to set Discovery Filter!!!"
                        , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
        lenBtrMgrRet = eBTRMgrFailure;
    }
    else {
#endif
        if (BTRMGR_RESULT_SUCCESS == BTRMGR_StartDeviceDiscovery_Internal (aui8AdapterIdx, btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl))) {

            //TODO: Move before you make the call to StartDeviceDiscovery, store the previous state and restore the previous state in case of Failure
            btrMgr_SetDiscoveryState (ahdiscoveryHdl, BTRMGR_DISCOVERY_ST_RESUMED);
            BTRMGRLOG_DEBUG ("[%s] Successfully Resumed Scan\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
        } else {
            BTRMGRLOG_ERROR ("[%s] Failed Resume Scan!!!\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
            lenBtrMgrRet =  eBTRMgrFailure;
        }
#if 0
    }
#endif

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StopDeviceDiscovery (
    unsigned char               aui8AdapterIdx,
    BTRMGR_DiscoveryHandle_t*   ahdiscoveryHdl
) {
    eBTRMgrRet  lenBtrMgrRet   = eBTRMgrSuccess;

    if (btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_STARTED ||
        btrMgr_GetDiscoveryState(ahdiscoveryHdl) == BTRMGR_DISCOVERY_ST_RESUMED ){

        if (BTRMGR_RESULT_SUCCESS == BTRMGR_StopDeviceDiscovery_Internal (aui8AdapterIdx, btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl))) {

            BTRMGRLOG_DEBUG ("[%s] Successfully Stopped scan\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
        }
        else {
            BTRMGRLOG_ERROR ("[%s] Failed to Stop scan\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
            lenBtrMgrRet =  eBTRMgrFailure;
        }
    }
    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_PreCheckDiscoveryStatus (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_DeviceOperationType_t    aDevOpType
) {
    eBTRMgrRet                lenBtrMgrRet  = eBTRMgrSuccess;
    BTRMGR_DiscoveryHandle_t* ldiscoveryHdl = NULL;

    if ((ldiscoveryHdl = btrMgr_GetDiscoveryInProgress())) {

        if ( btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl) == btrMgr_GetBgDiscoveryType()) {
            BTRMGRLOG_WARN ("Calling btrMgr_PauseDeviceDiscovery\n");
            lenBtrMgrRet = btrMgr_PauseDeviceDiscovery (aui8AdapterIdx, ldiscoveryHdl);
        }
        else if (aDevOpType != btrMgr_GetBgDiscoveryType()) {
            BTRMGRLOG_WARN ("Calling btrMgr_StopDeviceDiscovery\n");
            lenBtrMgrRet = btrMgr_StopDeviceDiscovery (aui8AdapterIdx, ldiscoveryHdl);
        }
        else {
            BTRMGRLOG_WARN ("[%s] Scan in Progress.. Request for %s operation is rejected...\n"
                           , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl))
                           , btrMgr_GetDiscoveryDeviceTypeAsString (aDevOpType));
            lenBtrMgrRet = eBTRMgrFailure;
        }
    }
    else if (btrMgr_isTimeOutSet()) {
        if (aDevOpType == btrMgr_GetBgDiscoveryType()) {
            BTRMGRLOG_WARN ("[NON-%s] Operation in Progress.. Request for %s operation is rejected...\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (aDevOpType)
                            , btrMgr_GetDiscoveryDeviceTypeAsString (aDevOpType));
            lenBtrMgrRet = eBTRMgrFailure;
        }
    }

    if (aDevOpType != btrMgr_GetBgDiscoveryType()) {
        btrMgr_ClearDiscoveryHoldOffTimer();
    }

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_PostCheckDiscoveryStatus (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_DeviceOperationType_t    aDevOpType
) {
    eBTRMgrRet  lenBtrMgrRet  = eBTRMgrSuccess;

    if (!btrMgr_isTimeOutSet()) {
        if (aDevOpType == BTRMGR_DEVICE_OP_TYPE_UNKNOWN) {
            if (btrMgr_GetDiscoveryState(&ghBTRMgrBgDiscoveryHdl) == BTRMGR_DISCOVERY_ST_PAUSED) {
                BTRMGRLOG_WARN ("Calling btrMgr_ResumeDeviceDiscovery\n");
                lenBtrMgrRet = btrMgr_ResumeDeviceDiscovery (aui8AdapterIdx, &ghBTRMgrBgDiscoveryHdl);
            }
        }
        else
        if (aDevOpType != btrMgr_GetBgDiscoveryType()) {
            if (btrMgr_GetDiscoveryState(&ghBTRMgrBgDiscoveryHdl) == BTRMGR_DISCOVERY_ST_PAUSED) {
                btrMgr_SetDiscoveryHoldOffTimer(aui8AdapterIdx);
            }
        }
    }

    return lenBtrMgrRet;
}


static void
btrMgr_GetDiscoveredDevInfo (
    BTRMgrDeviceHandle          ahBTRMgrDevHdl,
    BTRMGR_DiscoveredDevices_t* apBtMgrDiscDevInfo
) {
    int j = 0;

    for (j = 0; j < gListOfDiscoveredDevices.m_numOfDevices; j++) {
        if (ahBTRMgrDevHdl == gListOfDiscoveredDevices.m_deviceProperty[j].m_deviceHandle) {
            memcpy(apBtMgrDiscDevInfo, &gListOfDiscoveredDevices.m_deviceProperty[j], sizeof(BTRMGR_DiscoveredDevices_t));;
        }
    }
}


static void
btrMgr_GetPairedDevInfo (
    BTRMgrDeviceHandle      ahBTRMgrDevHdl,
    BTRMGR_PairedDevices_t* apBtMgrPairedDevInfo
) {
    int j = 0;

    for (j = 0; j < gListOfPairedDevices.m_numOfDevices; j++) {
        if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
            memcpy(apBtMgrPairedDevInfo, &gListOfPairedDevices.m_deviceProperty[j], sizeof(BTRMGR_PairedDevices_t));
            break;
        }
    }
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


static void
btrMgr_SetDevConnected (
    BTRMgrDeviceHandle  ahBTRMgrDevHdl,
    unsigned char       aui8isDeviceConnected
) {
    int i = 0;

    for (i = 0; i < gListOfPairedDevices.m_numOfDevices; i++) {
        if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[i].m_deviceHandle) {
            gListOfPairedDevices.m_deviceProperty[i].m_isConnected = aui8isDeviceConnected; 
            BTRMGRLOG_WARN ("Setting = %lld - \tConnected = %d\n", ahBTRMgrDevHdl, aui8isDeviceConnected);
            break;
        }
    }
}


static unsigned char
btrMgr_IsDevConnected (
    BTRMgrDeviceHandle ahBTRMgrDevHdl
) {
    unsigned char lui8isDeviceConnected = 0;
    int           i = 0;

    for (i = 0; i < gListOfPairedDevices.m_numOfDevices; i++) {
        if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[i].m_deviceHandle) {
            lui8isDeviceConnected = gListOfPairedDevices.m_deviceProperty[i].m_isConnected;
            BTRMGRLOG_WARN ("Getting = %lld - \tConnected = %d\n", ahBTRMgrDevHdl, lui8isDeviceConnected);
        }
    }
    
    return lui8isDeviceConnected;
}


static unsigned char
btrMgr_IsDevNameSameAsAddress (
    char*           apcDeviceName,
    char*           apcDeviceAddress,
    unsigned int    ui32StrLen
) {
    if (ui32StrLen > 17)
        return 0;


    if ((apcDeviceName[0] == apcDeviceAddress[0]) &&
        (apcDeviceName[1] == apcDeviceAddress[1]) &&
        (apcDeviceName[3] == apcDeviceAddress[3]) &&
        (apcDeviceName[4] == apcDeviceAddress[4]) &&
        (apcDeviceName[6] == apcDeviceAddress[6]) &&
        (apcDeviceName[7] == apcDeviceAddress[7]) &&
        (apcDeviceName[9] == apcDeviceAddress[9]) &&
        (apcDeviceName[10] == apcDeviceAddress[10]) &&
        (apcDeviceName[12] == apcDeviceAddress[12]) &&
        (apcDeviceName[13] == apcDeviceAddress[13]) &&
        (apcDeviceName[15] == apcDeviceAddress[15]) &&
        (apcDeviceName[16] == apcDeviceAddress[16]) ) {
        return 1;
    }
    else {
        return 0;
    }
}


static unsigned char
btrMgr_CheckIfDevicePrevDetected (
    BTRMgrDeviceHandle          ahBTRMgrDevHdl
) {
    int j = 0;

    for (j = 0; j < gListOfDiscoveredDevices.m_numOfDevices; j++) {
        if (ahBTRMgrDevHdl == gListOfDiscoveredDevices.m_deviceProperty[j].m_deviceHandle) {
            BTRMGRLOG_WARN ("DevicePrevDetected = %lld - %s\n", gListOfDiscoveredDevices.m_deviceProperty[j].m_deviceHandle, gListOfDiscoveredDevices.m_deviceProperty[j].m_name);
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
        // type = BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO;
        type = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
        break;
    case enBTRCore_DC_CarAudio:
        // type = BTRMGR_DEVICE_TYPE_CAR_AUDIO;
        type = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
        break;
    case enBTRCore_DC_STB:
        type = BTRMGR_DEVICE_TYPE_STB;
        break;
    case enBTRCore_DC_HIFIAudioDevice:
        // type = BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE;
        type = BTRMGR_DEVICE_TYPE_LOUDSPEAKER;
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
    case enBTRCore_DC_Tile:
        type = BTRMGR_DEVICE_TYPE_TILE;
        break;
    case enBTRCore_DC_HID_Keyboard:
        type = BTRMGR_DEVICE_TYPE_HID;
        break;
    case enBTRCore_DC_HID_Mouse:
        type = BTRMGR_DEVICE_TYPE_HID;
        break;
    case enBTRCore_DC_HID_MouseKeyBoard:
        type = BTRMGR_DEVICE_TYPE_HID;
        break;
    case enBTRCore_DC_HID_Joystick:
        type = BTRMGR_DEVICE_TYPE_HID;
        break;
    case enBTRCore_DC_HID_GamePad:
        type = BTRMGR_DEVICE_TYPE_HID_GAMEPAD;
        break;
    case enBTRCore_DC_HID_AudioRemote:
        type = BTRMGR_DEVICE_TYPE_HID;
        break;
    case enBTRCore_DC_Reserved:
    case enBTRCore_DC_Unknown:
        type = BTRMGR_DEVICE_TYPE_UNKNOWN;
        break;
    }

    return type;
}

static BTRMGR_DeviceOperationType_t
btrMgr_MapDeviceOpFromDeviceType (
    BTRMGR_DeviceType_t device_type
) {
    BTRMGR_DeviceOperationType_t devOpType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;

    switch (device_type) {
    case BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET:
    case BTRMGR_DEVICE_TYPE_HANDSFREE:
    case BTRMGR_DEVICE_TYPE_LOUDSPEAKER:
    case BTRMGR_DEVICE_TYPE_HEADPHONES:
    case BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO:
    case BTRMGR_DEVICE_TYPE_CAR_AUDIO:
    case BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE:
        devOpType = BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT;
        break;
    case BTRMGR_DEVICE_TYPE_SMARTPHONE:
    case BTRMGR_DEVICE_TYPE_TABLET:
        devOpType = BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT;
        break;
    case BTRMGR_DEVICE_TYPE_TILE:
        devOpType = BTRMGR_DEVICE_OP_TYPE_LE;
        break;
    case BTRMGR_DEVICE_TYPE_HID:
    case BTRMGR_DEVICE_TYPE_HID_GAMEPAD:
        devOpType = BTRMGR_DEVICE_OP_TYPE_HID;
        break;
    case BTRMGR_DEVICE_TYPE_STB:
    case BTRMGR_DEVICE_TYPE_MICROPHONE:
    case BTRMGR_DEVICE_TYPE_VCR:
    case BTRMGR_DEVICE_TYPE_VIDEO_CAMERA:
    case BTRMGR_DEVICE_TYPE_CAMCODER:
    case BTRMGR_DEVICE_TYPE_VIDEO_MONITOR:
    case BTRMGR_DEVICE_TYPE_TV:
    case BTRMGR_DEVICE_TYPE_VIDEO_CONFERENCE:
    case BTRMGR_DEVICE_TYPE_RESERVED:
    case BTRMGR_DEVICE_TYPE_UNKNOWN:
    default:
        devOpType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;
    }

    return devOpType;
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

    if (!p_StatusCB)
        return eBTRMgrFailure;


    if (type == BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE) {
        apstEventMessage->m_discoveredDevice.m_deviceHandle      = ((stBTRCoreBTDevice*)p_StatusCB)->tDeviceId;
        apstEventMessage->m_discoveredDevice.m_signalLevel       = ((stBTRCoreBTDevice*)p_StatusCB)->i32RSSI;
        apstEventMessage->m_discoveredDevice.m_deviceType        = btrMgr_MapDeviceTypeFromCore(((stBTRCoreBTDevice*)p_StatusCB)->enDeviceType);
        apstEventMessage->m_discoveredDevice.m_rssi              = btrMgr_MapSignalStrengthToRSSI(((stBTRCoreBTDevice*)p_StatusCB)->i32RSSI);
        apstEventMessage->m_discoveredDevice.m_isPairedDevice    = btrMgr_GetDevPaired(apstEventMessage->m_discoveredDevice.m_deviceHandle);
        apstEventMessage->m_discoveredDevice.m_isLowEnergyDevice = (apstEventMessage->m_discoveredDevice.m_deviceType==BTRMGR_DEVICE_TYPE_TILE)?1:0;//We shall make it generic later

        apstEventMessage->m_discoveredDevice.m_isDiscovered         = ((stBTRCoreBTDevice*)p_StatusCB)->bFound;
        apstEventMessage->m_discoveredDevice.m_isLastConnectedDevice= (ghBTRMgrDevHdlLastConnected == apstEventMessage->m_discoveredDevice.m_deviceHandle) ? 1 : 0;
        apstEventMessage->m_discoveredDevice.m_ui32DevClassBtSpec   = ((stBTRCoreBTDevice*)p_StatusCB)->ui32DevClassBtSpec;

        strncpy(apstEventMessage->m_discoveredDevice.m_name, ((stBTRCoreBTDevice*)p_StatusCB)->pcDeviceName, BTRMGR_NAME_LEN_MAX - 1);
        strncpy(apstEventMessage->m_discoveredDevice.m_deviceAddress, ((stBTRCoreBTDevice*)p_StatusCB)->pcDeviceAddress, BTRMGR_NAME_LEN_MAX - 1);
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST) {
        apstEventMessage->m_externalDevice.m_deviceHandle        = ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.tDeviceId;
        apstEventMessage->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.enDeviceType);
        apstEventMessage->m_externalDevice.m_vendorID            = ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.ui32VendorId;
        apstEventMessage->m_externalDevice.m_isLowEnergyDevice   = 0;
        apstEventMessage->m_externalDevice.m_externalDevicePIN   = ((stBTRCoreConnCBInfo*)p_StatusCB)->ui32devPassKey;
        apstEventMessage->m_externalDevice.m_requestConfirmation = ((stBTRCoreConnCBInfo*)p_StatusCB)->ucIsReqConfirmation;
        strncpy(apstEventMessage->m_externalDevice.m_name, ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.pcDeviceName, BTRMGR_NAME_LEN_MAX - 1);
        strncpy(apstEventMessage->m_externalDevice.m_deviceAddress, ((stBTRCoreConnCBInfo*)p_StatusCB)->stFoundDevice.pcDeviceAddress, BTRMGR_NAME_LEN_MAX - 1);
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST) {
        apstEventMessage->m_externalDevice.m_deviceHandle        = ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.tDeviceId;
        apstEventMessage->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.enDeviceType);
        apstEventMessage->m_externalDevice.m_vendorID            = ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.ui32VendorId;
        apstEventMessage->m_externalDevice.m_isLowEnergyDevice   = 0;
        strncpy(apstEventMessage->m_externalDevice.m_name, ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.pcDeviceName, BTRMGR_NAME_LEN_MAX - 1);
        strncpy(apstEventMessage->m_externalDevice.m_deviceAddress, ((stBTRCoreConnCBInfo*)p_StatusCB)->stKnownDevice.pcDeviceAddress, BTRMGR_NAME_LEN_MAX - 1);
    }
    else if (type == BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST) {
        apstEventMessage->m_externalDevice.m_deviceHandle        = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
        apstEventMessage->m_externalDevice.m_deviceType          = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
        apstEventMessage->m_externalDevice.m_vendorID            = 0;
        apstEventMessage->m_externalDevice.m_isLowEnergyDevice   = 0;
        strncpy(apstEventMessage->m_externalDevice.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName, BTRMGR_NAME_LEN_MAX - 1);
        strncpy(apstEventMessage->m_externalDevice.m_deviceAddress, "TO BE FILLED", BTRMGR_NAME_LEN_MAX - 1);
        // Need to check for devAddress, if possible ?
    }
    else if (type == BTRMGR_EVENT_DEVICE_OP_INFORMATION) {
        if (enBTRCoreLE == ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceType) {
            apstEventMessage->m_deviceOpInfo.m_deviceHandle      = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
            apstEventMessage->m_deviceOpInfo.m_deviceType        = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
            apstEventMessage->m_deviceOpInfo.m_leOpType          = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eCoreLeOper;

            strncpy(apstEventMessage->m_deviceOpInfo.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName,
                    strlen(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName) < BTRMGR_NAME_LEN_MAX ? strlen (((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName) : BTRMGR_NAME_LEN_MAX - 1);
            strncpy(apstEventMessage->m_deviceOpInfo.m_deviceAddress, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceAddress,
                    strlen(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceAddress) < BTRMGR_NAME_LEN_MAX ? strlen (((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceAddress) : BTRMGR_NAME_LEN_MAX - 1);
            strncpy(apstEventMessage->m_deviceOpInfo.m_uuid, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->uuid,
                    strlen(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->uuid) < BTRMGR_MAX_STR_LEN ? strlen(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->uuid) : BTRMGR_MAX_STR_LEN - 1);
        }
    }
    else {
        apstEventMessage->m_pairedDevice.m_deviceHandle          = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
        apstEventMessage->m_pairedDevice.m_deviceType            = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
        apstEventMessage->m_pairedDevice.m_isLastConnectedDevice = (ghBTRMgrDevHdlLastConnected == apstEventMessage->m_pairedDevice.m_deviceHandle) ? 1 : 0;

        if (apstEventMessage->m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TILE) {
            apstEventMessage->m_pairedDevice.m_isLowEnergyDevice     = 1;//We shall make it generic later
        }
        else {
            apstEventMessage->m_pairedDevice.m_isLowEnergyDevice     = (((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceType == enBTRCoreLE)?1:0;//We shall make it generic later
        }

        apstEventMessage->m_pairedDevice.m_ui32DevClassBtSpec    = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->ui32DevClassBtSpec;
        strncpy(apstEventMessage->m_pairedDevice.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName,
                    strlen(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName) < BTRMGR_NAME_LEN_MAX ? strlen (((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName) : BTRMGR_NAME_LEN_MAX - 1);
        strncpy(apstEventMessage->m_pairedDevice.m_deviceAddress, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceAddress,
                    strlen(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceAddress) < BTRMGR_NAME_LEN_MAX ? strlen (((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceAddress) : BTRMGR_NAME_LEN_MAX - 1);
    }

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StartCastingAudio (
    int                     outFileFd, 
    int                     outMTUSize,
    unsigned int            outDevDelay,
    eBTRCoreDevMediaType    aenBtrCoreDevOutMType,
    void*                   apstBtrCoreDevOutMCodecInfo
) {
    stBTRMgrOutASettings    lstBtrMgrAcOutASettings;
    stBTRMgrInASettings     lstBtrMgrSoInASettings;
    stBTRMgrOutASettings    lstBtrMgrSoOutASettings;
    eBTRMgrRet              lenBtrMgrRet = eBTRMgrSuccess;

    int                     inBytesToEncode = 3072; // Corresponds to MTU size of 895

    BTRMGR_StreamOut_Type_t lenCurrentSoType  = gstBTRMgrStreamingInfo.tBTRMgrSoType;
    tBTRMgrAcType           lpi8BTRMgrAcType= BTRMGR_AC_TYPE_PRIMARY;
#ifdef RDKTV_PERSIST_VOLUME_SKY
    unsigned char           ui8Volume = BTRMGR_SO_MAX_VOLUME;
    gboolean                lbMute = FALSE;
#endif

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

    if (lenCurrentSoType == BTRMGR_STREAM_PRIMARY) {
        lpi8BTRMgrAcType = BTRMGR_AC_TYPE_PRIMARY;
        gstBTRMgrStreamingInfo.tBTRMgrSoType = lenCurrentSoType;
    }
    else if (lenCurrentSoType == BTRMGR_STREAM_AUXILIARY) {
        lpi8BTRMgrAcType = BTRMGR_AC_TYPE_AUXILIARY;
        gstBTRMgrStreamingInfo.tBTRMgrSoType = lenCurrentSoType;
    }
    else {
        lpi8BTRMgrAcType = BTRMGR_AC_TYPE_PRIMARY;
        gstBTRMgrStreamingInfo.tBTRMgrSoType = BTRMGR_STREAM_PRIMARY;
    }

    if ((lenBtrMgrRet = BTRMgr_AC_Init(&gstBTRMgrStreamingInfo.hBTRMgrAcHdl, lpi8BTRMgrAcType)) != eBTRMgrSuccess) {
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
    lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ?
                                                                    (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));


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


    if (aenBtrCoreDevOutMType == eBTRCoreDevMediaTypeSBC) {
        stBTRMgrSBCInfo*            pstBtrMgrSoOutSbcInfo       = ((stBTRMgrSBCInfo*)(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo));
        stBTRCoreDevMediaSbcInfo*   pstBtrCoreDevMediaSbcInfo   = (stBTRCoreDevMediaSbcInfo*)apstBtrCoreDevOutMCodecInfo;

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
        gstBTRMgrStreamingInfo.i32BytesToEncode = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;
    }

#ifdef RDKTV_PERSIST_VOLUME_SKY
    if (btrMgr_GetLastVolume(0, &ui8Volume) == eBTRMgrSuccess) {
        if (!gui8IsSoDevAvrcpSupported && (BTRMgr_SO_SetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, ui8Volume) != eBTRMgrSuccess)) {
            BTRMGRLOG_ERROR (" BTRMgr_SO_SetVolume FAILED \n");
        }
    }

    if (btrMgr_GetLastMuteState(0, &lbMute) == eBTRMgrSuccess) {
        if (BTRMgr_SO_SetMute(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, lbMute) != eBTRMgrSuccess) {
            BTRMGRLOG_ERROR (" BTRMgr_SO_SetMute FAILED \n");
        }
    }
#endif

    if ((lenBtrMgrRet = BTRMgr_SO_Start(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_Start FAILED\n");
    }

    if (lenBtrMgrRet == eBTRMgrSuccess) {
        lstBtrMgrAcOutASettings.i32BtrMgrOutBufMaxSize = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;
        lstBtrMgrAcOutASettings.ui32BtrMgrDevDelay = outDevDelay;

        if ((lenBtrMgrRet = BTRMgr_AC_Start(gstBTRMgrStreamingInfo.hBTRMgrAcHdl,
                                            &lstBtrMgrAcOutASettings,
                                            btrMgr_ACDataReadyCb,
                                            btrMgr_ACStatusCb,
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
btrMgr_SwitchCastingAudio_AC (
    BTRMGR_StreamOut_Type_t aenCurrentSoType
) {
    eBTRMgrRet              lenBtrMgrRet            = eBTRMgrSuccess;
    tBTRMgrAcType           lpi8BTRMgrAcType        = BTRMGR_AC_TYPE_PRIMARY;
    stBTRMgrOutASettings    lstBtrMgrAcOutASettings;


    if (ghBTRMgrDevHdlCurStreaming == 0) {
        return eBTRMgrFailInArg;
    }


    if ((lenBtrMgrRet = BTRMgr_AC_Stop(gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_AC_Stop FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_SO_Pause(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_Pause FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_AC_DeInit(gstBTRMgrStreamingInfo.hBTRMgrAcHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_AC_DeInit FAILED\n");
    }

    gstBTRMgrStreamingInfo.hBTRMgrAcHdl = NULL;


    if (aenCurrentSoType == BTRMGR_STREAM_PRIMARY) {
        lpi8BTRMgrAcType = BTRMGR_AC_TYPE_PRIMARY;
    }
    else if (aenCurrentSoType == BTRMGR_STREAM_AUXILIARY) {
        lpi8BTRMgrAcType = BTRMGR_AC_TYPE_AUXILIARY;
    }
    else {
        lpi8BTRMgrAcType = BTRMGR_AC_TYPE_PRIMARY;
    }


    if ((lenBtrMgrRet = BTRMgr_AC_Init(&gstBTRMgrStreamingInfo.hBTRMgrAcHdl, lpi8BTRMgrAcType)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_AC_Init FAILED\n");
        return eBTRMgrInitFailure;
    }

    /* Reset the buffer */
    memset(&lstBtrMgrAcOutASettings, 0, sizeof(lstBtrMgrAcOutASettings));
    lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ?
                                                                    (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));
    if (!lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo) {
        BTRMGRLOG_ERROR ("MEMORY ALLOC FAILED\n");
        return eBTRMgrFailure;
    }


    if ((lenBtrMgrRet = BTRMgr_AC_GetDefaultSettings(gstBTRMgrStreamingInfo.hBTRMgrAcHdl, &lstBtrMgrAcOutASettings)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR("BTRMgr_AC_GetDefaultSettings FAILED\n");
    }

    if ((lenBtrMgrRet = BTRMgr_SO_Resume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SO_Resume FAILED\n");
    }

    lstBtrMgrAcOutASettings.i32BtrMgrOutBufMaxSize = gstBTRMgrStreamingInfo.i32BytesToEncode;
    if ((lenBtrMgrRet = BTRMgr_AC_Start(gstBTRMgrStreamingInfo.hBTRMgrAcHdl,
                                        &lstBtrMgrAcOutASettings,
                                        btrMgr_ACDataReadyCb,
                                        btrMgr_ACStatusCb,
                                        &gstBTRMgrStreamingInfo)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_AC_Start FAILED\n");
    }

    if (lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo)
        free(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo);

    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StartReceivingAudio (
    int                  inFileFd,
    int                  inMTUSize,
    unsigned int         inDevDelay,
    eBTRCoreDevMediaType aenBtrCoreDevInMType,
    void*                apstBtrCoreDevInMCodecInfo
) {
    eBTRMgrRet          lenBtrMgrRet = eBTRMgrSuccess;
    int                 inBytesToEncode = 3072;
    stBTRMgrInASettings lstBtrMgrSiInASettings;


    if ((ghBTRMgrDevHdlCurStreaming != 0) || (inMTUSize == 0)) {
        return eBTRMgrFailInArg;
    }


    /* Reset the buffer */
    memset(&lstBtrMgrSiInASettings, 0, sizeof(lstBtrMgrSiInASettings));


    /* Init StreamIn module - Create Pipeline */
    if ((lenBtrMgrRet = BTRMgr_SI_Init(&gstBTRMgrStreamingInfo.hBTRMgrSiHdl, btrMgr_SIStatusCb, &gstBTRMgrStreamingInfo)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SI_Init FAILED\n");
        return eBTRMgrInitFailure;
    }


    lstBtrMgrSiInASettings.pstBtrMgrInCodecInfo = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ?
                                                                (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));

    if (aenBtrCoreDevInMType == eBTRCoreDevMediaTypeSBC) {
        stBTRMgrSBCInfo*            pstBtrMgrSiInSbcInfo       = ((stBTRMgrSBCInfo*)(lstBtrMgrSiInASettings.pstBtrMgrInCodecInfo));
        stBTRCoreDevMediaSbcInfo*   pstBtrCoreDevMediaSbcInfo  = (stBTRCoreDevMediaSbcInfo*)apstBtrCoreDevInMCodecInfo;

        lstBtrMgrSiInASettings.eBtrMgrInAType   = eBTRMgrATypeSBC;
        if (pstBtrMgrSiInSbcInfo && pstBtrCoreDevMediaSbcInfo) {

            if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 8000) {
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq8K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 16000) {
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq16K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 32000) {
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq32K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 44100) {
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq44_1K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 48000) {
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq48K;
            }
            else {
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreqUnknown;
            }


            switch (pstBtrCoreDevMediaSbcInfo->eDevMAChan) {
            case eBTRCoreDevMediaAChanMono:
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanMono;
                break;
            case eBTRCoreDevMediaAChanDualChannel:
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanDualChannel;
                break;
            case eBTRCoreDevMediaAChanStereo:
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanStereo;
                break;
            case eBTRCoreDevMediaAChanJointStereo:
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanJStereo;
                break;
            case eBTRCoreDevMediaAChan5_1:
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan5_1;
                break;
            case eBTRCoreDevMediaAChan7_1:
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan7_1;
                break;
            case eBTRCoreDevMediaAChanUnknown:
            default:
                pstBtrMgrSiInSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanUnknown;
                break;
            }

            pstBtrMgrSiInSbcInfo->ui8SbcAllocMethod  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcAllocMethod;
            pstBtrMgrSiInSbcInfo->ui8SbcSubbands     = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcSubbands;
            pstBtrMgrSiInSbcInfo->ui8SbcBlockLength  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcBlockLength;
            pstBtrMgrSiInSbcInfo->ui8SbcMinBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMinBitpool;
            pstBtrMgrSiInSbcInfo->ui8SbcMaxBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMaxBitpool;
            pstBtrMgrSiInSbcInfo->ui16SbcFrameLen    = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcFrameLen;
            pstBtrMgrSiInSbcInfo->ui16SbcBitrate     = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcBitrate;
        }
    }
    else if (aenBtrCoreDevInMType == eBTRCoreDevMediaTypeAAC) {
        stBTRMgrMPEGInfo*           pstBtrMgrSiInAacInfo       = ((stBTRMgrMPEGInfo*)(lstBtrMgrSiInASettings.pstBtrMgrInCodecInfo));
        stBTRCoreDevMediaMpegInfo*  pstBtrCoreDevMediaAacInfo  = (stBTRCoreDevMediaMpegInfo*)apstBtrCoreDevInMCodecInfo;

        lstBtrMgrSiInASettings.eBtrMgrInAType   = eBTRMgrATypeAAC;
        if (pstBtrMgrSiInAacInfo && pstBtrCoreDevMediaAacInfo) {

            if (pstBtrCoreDevMediaAacInfo->ui32DevMSFreq == 8000) {
                pstBtrMgrSiInAacInfo->eBtrMgrMpegSFreq  = eBTRMgrSFreq8K;
            }
            else if (pstBtrCoreDevMediaAacInfo->ui32DevMSFreq == 16000) {
                pstBtrMgrSiInAacInfo->eBtrMgrMpegSFreq  = eBTRMgrSFreq16K;
            }
            else if (pstBtrCoreDevMediaAacInfo->ui32DevMSFreq == 32000) {
                pstBtrMgrSiInAacInfo->eBtrMgrMpegSFreq  = eBTRMgrSFreq32K;
            }
            else if (pstBtrCoreDevMediaAacInfo->ui32DevMSFreq == 44100) {
                pstBtrMgrSiInAacInfo->eBtrMgrMpegSFreq  = eBTRMgrSFreq44_1K;
            }
            else if (pstBtrCoreDevMediaAacInfo->ui32DevMSFreq == 48000) {
                pstBtrMgrSiInAacInfo->eBtrMgrMpegSFreq  = eBTRMgrSFreq48K;
            }
            else {
                pstBtrMgrSiInAacInfo->eBtrMgrMpegSFreq  = eBTRMgrSFreqUnknown;
            }


            switch (pstBtrCoreDevMediaAacInfo->eDevMAChan) {
            case eBTRCoreDevMediaAChanMono:
                pstBtrMgrSiInAacInfo->eBtrMgrMpegAChan  = eBTRMgrAChanMono;
                break;
            case eBTRCoreDevMediaAChanDualChannel:
                pstBtrMgrSiInAacInfo->eBtrMgrMpegAChan  = eBTRMgrAChanDualChannel;
                break;
            case eBTRCoreDevMediaAChanStereo:
                pstBtrMgrSiInAacInfo->eBtrMgrMpegAChan  = eBTRMgrAChanStereo;
                break;
            case eBTRCoreDevMediaAChanJointStereo:
                pstBtrMgrSiInAacInfo->eBtrMgrMpegAChan  = eBTRMgrAChanJStereo;
                break;
            case eBTRCoreDevMediaAChan5_1:
                pstBtrMgrSiInAacInfo->eBtrMgrMpegAChan  = eBTRMgrAChan5_1;
                break;
            case eBTRCoreDevMediaAChan7_1:
                pstBtrMgrSiInAacInfo->eBtrMgrMpegAChan  = eBTRMgrAChan7_1;
                break;
            case eBTRCoreDevMediaAChanUnknown:
            default:
                pstBtrMgrSiInAacInfo->eBtrMgrMpegAChan  = eBTRMgrAChanUnknown;
                break;
            }
            
            pstBtrMgrSiInAacInfo->ui8MpegCrc       = pstBtrCoreDevMediaAacInfo->ui8DevMMpegCrc;
            pstBtrMgrSiInAacInfo->ui8MpegLayer     = pstBtrCoreDevMediaAacInfo->ui8DevMMpegLayer;
            pstBtrMgrSiInAacInfo->ui8MpegMpf       = pstBtrCoreDevMediaAacInfo->ui8DevMMpegMpf;
            pstBtrMgrSiInAacInfo->ui8MpegRfa       = pstBtrCoreDevMediaAacInfo->ui8DevMMpegRfa;
            pstBtrMgrSiInAacInfo->ui16MpegBitrate  = pstBtrCoreDevMediaAacInfo->ui16DevMMpegBitrate;
        }
    }
    


    lstBtrMgrSiInASettings.i32BtrMgrDevFd   = inFileFd;
    lstBtrMgrSiInASettings.i32BtrMgrDevMtu  = inMTUSize;


    if ((lenBtrMgrRet = BTRMgr_SI_Start(gstBTRMgrStreamingInfo.hBTRMgrSiHdl, inBytesToEncode, &lstBtrMgrSiInASettings)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("BTRMgr_SI_Start FAILED\n");
    }


    if (lstBtrMgrSiInASettings.pstBtrMgrInCodecInfo)
        free(lstBtrMgrSiInASettings.pstBtrMgrInCodecInfo);

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
    eBTRMgrRet          lenBtrMgrRet        = eBTRMgrSuccess;
    enBTRCoreRet        lenBtrCoreRet       = enBTRCoreSuccess;
    int                 i32PairDevIdx       = 0;
    unsigned int        ui32retryIdx        = aui32ConnectRetryIdx + 1;
    enBTRCoreDeviceType lenBTRCoreDeviceType= enBTRCoreUnknown;
    BTRMGR_DeviceOperationType_t lenMgrLConDevOpType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;

    lenBtrMgrRet = btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, connectAs);

    if (eBTRMgrSuccess != lenBtrMgrRet) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Rejected !!!\n");
        return lenBtrMgrRet;
    }

    switch (connectAs) {
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT:
        gui8IsSoDevAvrcpSupported = 0;      // TODO: Find a better way to do this
        lenBTRCoreDeviceType = enBTRCoreSpeakers;
        break;
    case BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT:
        lenBTRCoreDeviceType = enBTRCoreMobileAudioIn;
        if (!gIsAudioInEnabled) {
            BTRMGRLOG_WARN ("Connection Rejected - BTR AudioIn is currently Disabled!\n");
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }
        break;
    case BTRMGR_DEVICE_OP_TYPE_LE:
        lenBTRCoreDeviceType = enBTRCoreLE;
        break;
    case BTRMGR_DEVICE_OP_TYPE_HID:
        lenBTRCoreDeviceType = enBTRCoreHID;
        break;
    case BTRMGR_DEVICE_OP_TYPE_UNKNOWN:
    default:
        lenBTRCoreDeviceType = enBTRCoreUnknown;
        break;
    } 


    do {
        /* connectAs param is unused for now.. */
        lenBtrCoreRet = BTRCore_ConnectDevice (ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDeviceType);
        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Connect to this device - %llu\n", ahBTRMgrDevHdl);
            lenBtrMgrRet = eBTRMgrFailure;
        }
        else {
            int j;
            for (j = 0; j <= gListOfPairedDevices.m_numOfDevices; j++) {
                 if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
                     BTRMGRLOG_INFO ("Connected Successfully -  %llu - Name -  %s\n",ahBTRMgrDevHdl,gListOfPairedDevices.m_deviceProperty[j].m_name);
                     break;
                 }
            }
            lenBtrMgrRet = eBTRMgrSuccess;
            if ((lenBTRCoreDeviceType == enBTRCoreSpeakers) || (lenBTRCoreDeviceType == enBTRCoreHeadSet) ||
                (lenBTRCoreDeviceType == enBTRCoreMobileAudioIn) || (lenBTRCoreDeviceType == enBTRCorePCAudioIn)) {
                if (ghBTRMgrDevHdlLastConnected && ghBTRMgrDevHdlLastConnected != ahBTRMgrDevHdl) {
                    BTRMGRLOG_DEBUG ("Remove persistent entry for previously connected device(%llu)\n", ghBTRMgrDevHdlLastConnected);

                    for (i32PairDevIdx = 0; i32PairDevIdx < gListOfPairedDevices.m_numOfDevices; i32PairDevIdx++) {
                        if (ghBTRMgrDevHdlLastConnected == gListOfPairedDevices.m_deviceProperty[i32PairDevIdx].m_deviceHandle) {
                            lenMgrLConDevOpType = btrMgr_MapDeviceOpFromDeviceType(gListOfPairedDevices.m_deviceProperty[i32PairDevIdx].m_deviceType);
                        }
                    }

                    if (lenMgrLConDevOpType == BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) {
                        btrMgr_RemovePersistentEntry(aui8AdapterIdx, ghBTRMgrDevHdlLastConnected, BTRMGR_A2DP_SINK_PROFILE_ID);
                    }
                    else if (lenMgrLConDevOpType == BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT) {
                        btrMgr_RemovePersistentEntry(aui8AdapterIdx, ghBTRMgrDevHdlLastConnected, BTRMGR_A2DP_SRC_PROFILE_ID);
                    }
                }

                if ((lenBTRCoreDeviceType == enBTRCoreSpeakers) || (lenBTRCoreDeviceType == enBTRCoreHeadSet)) {
                    btrMgr_AddPersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SINK_PROFILE_ID, 1);
                }
                else if ((lenBTRCoreDeviceType == enBTRCoreMobileAudioIn) || (lenBTRCoreDeviceType == enBTRCorePCAudioIn)) {
                    btrMgr_AddPersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SRC_PROFILE_ID, 1);
                }
            }
        }

        if (lenBtrMgrRet != eBTRMgrFailure) {
            /* Max 15 sec timeout - Polled at 1 second interval: Confirmed 5 times */
            unsigned int ui32sleepTimeOut = 1;
            unsigned int ui32confirmIdx = aui32ConfirmIdx + 1;
            unsigned int ui32AdjSleepIdx = (aui32ConfirmIdx > BTRMGR_DEVCONN_CHECK_RETRY_ATTEMPTS) ? 2 : 5; //Interval in attempts

            do {
                unsigned int ui32sleepIdx = ui32AdjSleepIdx;

                do {
                    sleep(ui32sleepTimeOut); 
                    lenBtrCoreRet = BTRCore_GetDeviceConnected(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDeviceType);
                } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
            } while (--ui32confirmIdx);

            if (lenBtrCoreRet != enBTRCoreSuccess) {
                BTRMGRLOG_ERROR ("Failed to Connect to this device - Confirmed - %llu\n", ahBTRMgrDevHdl);
                lenBtrMgrRet = eBTRMgrFailure;

                if ((lenBTRCoreDeviceType == enBTRCoreSpeakers) || (lenBTRCoreDeviceType == enBTRCoreHeadSet)) {
                    btrMgr_RemovePersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SINK_PROFILE_ID);
                }
                else if ((lenBTRCoreDeviceType == enBTRCoreMobileAudioIn) || (lenBTRCoreDeviceType == enBTRCorePCAudioIn)) {
                    btrMgr_RemovePersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SRC_PROFILE_ID);
                }

                if (ghBTRMgrDevHdlLastConnected && ghBTRMgrDevHdlLastConnected != ahBTRMgrDevHdl) {
                    BTRMGRLOG_DEBUG ("Add back persistent entry for previously connected device(%llu)\n", ghBTRMgrDevHdlLastConnected);

                    if (lenMgrLConDevOpType == BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) {
                        btrMgr_AddPersistentEntry(aui8AdapterIdx, ghBTRMgrDevHdlLastConnected, BTRMGR_A2DP_SINK_PROFILE_ID, 1);
                    }
                    else if (lenMgrLConDevOpType == BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT) {
                        btrMgr_AddPersistentEntry(aui8AdapterIdx, ghBTRMgrDevHdlLastConnected, BTRMGR_A2DP_SRC_PROFILE_ID, 1);
                    }
                }

                if (BTRCore_DisconnectDevice (ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDeviceType) != enBTRCoreSuccess) {
                    BTRMGRLOG_ERROR ("Failed to Disconnect - %llu\n", ahBTRMgrDevHdl);
                }
            }
            else {
                BTRMGRLOG_DEBUG ("Succes Connect to this device - Confirmed - %llu\n", ahBTRMgrDevHdl);

                if (lenBTRCoreDeviceType == enBTRCoreLE) {
                    gIsLeDeviceConnected = 1;
                }
                else if ((lenBTRCoreDeviceType == enBTRCoreSpeakers) || (lenBTRCoreDeviceType == enBTRCoreHeadSet) ||
                         (lenBTRCoreDeviceType == enBTRCoreMobileAudioIn) || (lenBTRCoreDeviceType == enBTRCorePCAudioIn)) {
                    btrMgr_SetDevConnected(ahBTRMgrDevHdl, 1);
                    gIsUserInitiated = 0;
                    ghBTRMgrDevHdlLastConnected = ahBTRMgrDevHdl;
                }
                else if (lenBTRCoreDeviceType == enBTRCoreHID) {
                    btrMgr_SetDevConnected(ahBTRMgrDevHdl, 1);
                }
                else {
                    btrMgr_SetDevConnected(ahBTRMgrDevHdl, 1);
                }
            }
        }
    } while ((lenBtrMgrRet == eBTRMgrFailure) && (--ui32retryIdx));

    if (lenBtrMgrRet == eBTRMgrFailure && lenBTRCoreDeviceType == enBTRCoreLE) {
        connectAs = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;
    }

    btrMgr_PostCheckDiscoveryStatus(aui8AdapterIdx, connectAs);

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
    BTRMGR_Result_t             lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    eBTRMgrRet                  lenBtrMgrRet    = eBTRMgrSuccess;
    enBTRCoreRet                lenBtrCoreRet   = enBTRCoreSuccess;
    unsigned char               isFound = 0;
    int                         i = 0;
    int                         deviceFD = 0;
    int                         deviceReadMTU = 0;
    int                         deviceWriteMTU = 0;
    unsigned int                deviceDelay = 0xFFFFu;
    unsigned int                ui32retryIdx = aui32ConnectRetryIdx + 1;
    stBTRCorePairedDevicesCount listOfPDevices;
    eBTRCoreDevMediaType        lenBtrCoreDevOutMType = eBTRCoreDevMediaTypeUnknown;
    void*                       lpstBtrCoreDevOutMCodecInfo = NULL;


    if (ghBTRMgrDevHdlCurStreaming == ahBTRMgrDevHdl) {
        BTRMGRLOG_WARN ("Its already streaming out in this device.. Check the volume :)\n");
        return eBTRMgrSuccess;
    }


    if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming != ahBTRMgrDevHdl)) {
        enBTRCoreDeviceType     lenBTRCoreDevTy = enBTRCoreUnknown;
        enBTRCoreDeviceClass    lenBTRCoreDevCl = enBTRCore_DC_Unknown;

        BTRMGRLOG_WARN ("Its already streaming out. lets stop this and start on other device \n");

        lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ghBTRMgrDevHdlCurStreaming, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
        BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBTRCoreDevTy, lenBTRCoreDevCl);

        if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
            /* Streaming-Out is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to stop Playback.-Out\n");
                BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
                return lenBtrMgrResult;
            }
        }
        else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
            /* Streaming-In is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingIn(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to stop Playback.-In\n");
                BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
                return lenBtrMgrResult;
            }
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
        else if (strstr(listOfPDevices.devices[i].pcDeviceName, "Denon") || strstr(listOfPDevices.devices[i].pcDeviceName, "AVR") ||
                 strstr(listOfPDevices.devices[i].pcDeviceName, "DENON") || strstr(listOfPDevices.devices[i].pcDeviceName, "Avr")) {
            lenBtrMgrRet = btrMgr_ConnectToDevice(aui8AdapterIdx, listOfPDevices.devices[i].tDeviceId, streamOutPref, 0, BTRMGR_DEVCONN_CHECK_RETRY_ATTEMPTS + 2);
        }
        else {
            lenBtrMgrRet = btrMgr_ConnectToDevice(aui8AdapterIdx, listOfPDevices.devices[i].tDeviceId, streamOutPref, 0, 1);
        }

        if (lenBtrMgrRet == eBTRMgrSuccess) {
            if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
                free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
                gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
            }


            gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) > sizeof(stBTRCoreDevMediaMpegInfo) ?
                                                                           (sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) : sizeof(stBTRCoreDevMediaMpegInfo));

            lenBtrCoreRet = BTRCore_GetDeviceMediaInfo(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, enBTRCoreSpeakers, &gstBtrCoreDevMediaInfo);
            if (lenBtrCoreRet == enBTRCoreSuccess) {
                lenBtrCoreDevOutMType      = gstBtrCoreDevMediaInfo.eBtrCoreDevMType;
                lpstBtrCoreDevOutMCodecInfo= gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo;

                if (lenBtrCoreDevOutMType == eBTRCoreDevMediaTypeSBC) {
                    stBTRCoreDevMediaSbcInfo*   lpstBtrCoreDevMSbcInfo = (stBTRCoreDevMediaSbcInfo*)lpstBtrCoreDevOutMCodecInfo;

                    ui16DevMediaBitrate = lpstBtrCoreDevMSbcInfo->ui16DevMSbcBitrate;

                    BTRMGRLOG_INFO ("DevMedInfo SFreq         = %d\n", lpstBtrCoreDevMSbcInfo->ui32DevMSFreq);
                    BTRMGRLOG_INFO ("DevMedInfo AChan         = %d\n", lpstBtrCoreDevMSbcInfo->eDevMAChan);
                    BTRMGRLOG_INFO ("DevMedInfo SbcAllocMethod= %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcAllocMethod);
                    BTRMGRLOG_INFO ("DevMedInfo SbcSubbands   = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcSubbands);
                    BTRMGRLOG_INFO ("DevMedInfo SbcBlockLength= %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcBlockLength);
                    BTRMGRLOG_INFO ("DevMedInfo SbcMinBitpool = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcMinBitpool);
                    BTRMGRLOG_INFO ("DevMedInfo SbcMaxBitpool = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcMaxBitpool);
                    BTRMGRLOG_INFO ("DevMedInfo SbcFrameLen   = %d\n", lpstBtrCoreDevMSbcInfo->ui16DevMSbcFrameLen);
                    BTRMGRLOG_DEBUG("DevMedInfo SbcBitrate    = %d\n", lpstBtrCoreDevMSbcInfo->ui16DevMSbcBitrate);
                }
            }

            if (ui16DevMediaBitrate) {
                /* Aquire Device Data Path to start the audio casting */
                lenBtrCoreRet = BTRCore_AcquireDeviceDataPath(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, enBTRCoreSpeakers, &deviceFD, &deviceReadMTU, &deviceWriteMTU, &deviceDelay);
                if ((lenBtrCoreRet == enBTRCoreSuccess) && deviceWriteMTU) {
                    /* Now that you got the FD & Read/Write MTU, start casting the audio */
                    if ((lenBtrMgrRet = btrMgr_StartCastingAudio(deviceFD, deviceWriteMTU, deviceDelay, lenBtrCoreDevOutMType, lpstBtrCoreDevOutMCodecInfo)) == eBTRMgrSuccess) {
                        ghBTRMgrDevHdlCurStreaming = listOfPDevices.devices[i].tDeviceId;
                        BTRMGRLOG_INFO("Streaming Started.. Enjoy the show..! :)\n");

                        if (BTRCore_SetDeviceDataAckTimeout(ghBTRCoreHdl, 100) != enBTRCoreSuccess) {
                            BTRMGRLOG_WARN ("Failed to set timeout for Audio drop. EXPECT AV SYNC ISSUES!\n");
                        }
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
                    BTRMGRLOG_ERROR ("Failed to Disconnect from this device - Confirmed - %llu\n", listOfPDevices.devices[i].tDeviceId);
                    lenBtrMgrRet = eBTRMgrFailure; 
                }
                else
                    BTRMGRLOG_DEBUG ("Success Disconnect from this device - Confirmed - %llu\n", listOfPDevices.devices[i].tDeviceId);
            }

            lenBtrMgrRet = eBTRMgrFailure; 
        }


    } while ((lenBtrMgrRet == eBTRMgrFailure) && (--ui32retryIdx));


    {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        lstEventMessage.m_adapterIndex                 = aui8AdapterIdx;
        lstEventMessage.m_pairedDevice.m_deviceHandle  = listOfPDevices.devices[i].tDeviceId;
        lstEventMessage.m_pairedDevice.m_deviceType    = btrMgr_MapDeviceTypeFromCore(listOfPDevices.devices[i].enDeviceType);
        lstEventMessage.m_pairedDevice.m_isConnected   = (btrMgr_IsDevConnected(listOfPDevices.devices[i].tDeviceId)) ? 1 : 0;
        lstEventMessage.m_pairedDevice.m_isLowEnergyDevice = (lstEventMessage.m_pairedDevice.m_deviceType==BTRMGR_DEVICE_TYPE_TILE)?1:0;//will make it generic later
        strncpy(lstEventMessage.m_pairedDevice.m_name, listOfPDevices.devices[i].pcDeviceName, BTRMGR_NAME_LEN_MAX);
        strncpy(lstEventMessage.m_pairedDevice.m_deviceAddress, listOfPDevices.devices[i].pcDeviceAddress, BTRMGR_NAME_LEN_MAX);

        if (lenBtrMgrRet == eBTRMgrSuccess) {
            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE;

            if (gfpcBBTRMgrEventOut) {
                gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
            }
        }
        else if (lenBtrMgrRet == eBTRMgrFailure) {
            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_CONNECTION_FAILED;

            if (gfpcBBTRMgrEventOut) {
                gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
            }
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
    BTRMgrDeviceHandle  ahBTRMgrDevHdl,
    const char*         apui8ProfileStr,
    int                 ai32DevConnected
) {
    char        lui8adapterAddr[BD_NAME_LEN] = {'\0'};
    eBTRMgrRet  lenBtrMgrPiRet = eBTRMgrFailure;

    BTRCore_GetAdapterAddr(ghBTRCoreHdl, aui8AdapterIdx, lui8adapterAddr);

    // Device connected add data from json file
    BTRMGR_Profile_t btPtofile;
    strncpy(btPtofile.adapterId, lui8adapterAddr, BTRMGR_NAME_LEN_MAX -1);
    btPtofile.adapterId[BTRMGR_NAME_LEN_MAX -1] = '\0';
    strncpy(btPtofile.profileId, apui8ProfileStr, BTRMGR_NAME_LEN_MAX -1);
    btPtofile.profileId[BTRMGR_NAME_LEN_MAX -1] = '\0'; //CID:136398 - Bufefr size warning
    btPtofile.deviceId  = ahBTRMgrDevHdl;
    btPtofile.isConnect = ai32DevConnected;

    lenBtrMgrPiRet = BTRMgr_PI_AddProfile(ghBTRMgrPiHdl, &btPtofile);
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
    BTRMgrDeviceHandle  ahBTRMgrDevHdl,
    const char*         apui8ProfileStr
) {
    char         lui8adapterAddr[BD_NAME_LEN] = {'\0'};
    eBTRMgrRet   lenBtrMgrPiRet = eBTRMgrFailure;

    BTRCore_GetAdapterAddr(ghBTRCoreHdl, aui8AdapterIdx, lui8adapterAddr);

    // Device disconnected remove data from json file
    BTRMGR_Profile_t btPtofile;
    strncpy(btPtofile.adapterId, lui8adapterAddr, BTRMGR_NAME_LEN_MAX -1);
    btPtofile.adapterId[BTRMGR_NAME_LEN_MAX -1] = '\0';
    strncpy(btPtofile.profileId, apui8ProfileStr, BTRMGR_NAME_LEN_MAX -1);
    btPtofile.profileId[BTRMGR_NAME_LEN_MAX -1] = '\0';  //CID:136475 - Buffer size warning
    btPtofile.deviceId = ahBTRMgrDevHdl;
    btPtofile.isConnect = 1;

    lenBtrMgrPiRet = BTRMgr_PI_RemoveProfile(ghBTRMgrPiHdl, &btPtofile);
    if(lenBtrMgrPiRet == eBTRMgrSuccess) {
       BTRMGRLOG_INFO ("Persistent File updated successfully\n");
    }
    else {
       BTRMGRLOG_ERROR ("Persistent File update failed \n");
    }

    return lenBtrMgrPiRet;
}

static BTRMGR_SysDiagChar_t
btrMgr_MapUUIDtoDiagElement(
    char *aUUID
) {
    BTRMGR_SysDiagChar_t lDiagChar = BTRMGR_SYS_DIAG_UNKNOWN;
#ifndef BTRTEST_LE_ONBRDG_ENABLE
    if (!strcmp(aUUID, BTRMGR_SYSTEM_ID_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_SYSTEMID; }
    else if (!strcmp(aUUID, BTRMGR_MODEL_NUMBER_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_MODELNUMBER; }
    else if (!strcmp(aUUID, BTRMGR_SERIAL_NUMBER_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_SERIALNUMBER; }
    else if (!strcmp(aUUID, BTRMGR_FIRMWARE_REVISION_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_FWREVISION; }
    else if (!strcmp(aUUID, BTRMGR_HARDWARE_REVISION_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_HWREVISION; }
    else if (!strcmp(aUUID, BTRMGR_SOFTWARE_REVISION_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_SWREVISION; }
    else if (!strcmp(aUUID, BTRMGR_MANUFACTURER_NAME_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_MFGRNAME; }
#else
    if (!strcmp(aUUID, BTRMGR_SYSTEM_ID_UUID)) { lDiagChar = BTRMGR_LE_ONBRDG_SYSTEMID; }
    else if (!strcmp(aUUID, BTRMGR_MODEL_NUMBER_UUID)) { lDiagChar = BTRMGR_LE_ONBRDG_MODELNUMBER; }
    else if (!strcmp(aUUID, BTRMGR_SERIAL_NUMBER_UUID)) { lDiagChar = BTRMGR_LE_ONBRDG_SERIALNUMBER; }
    else if (!strcmp(aUUID, BTRMGR_FIRMWARE_REVISION_UUID)) { lDiagChar = BTRMGR_LE_ONBRDG_FWREVISION; }
    else if (!strcmp(aUUID, BTRMGR_HARDWARE_REVISION_UUID)) { lDiagChar = BTRMGR_LE_ONBRDG_HWREVISION; }
    else if (!strcmp(aUUID, BTRMGR_SOFTWARE_REVISION_UUID)) { lDiagChar = BTRMGR_LE_ONBRDG_SWREVISION; }
    else if (!strcmp(aUUID, BTRMGR_MANUFACTURER_NAME_UUID)) { lDiagChar = BTRMGR_LE_ONBRDG_MFGRNAME; }
#endif
    else if (!strcmp(aUUID, BTRMGR_LEONBRDG_UUID_QR_CODE)) { lDiagChar = BTRMGR_LE_ONBRDG_UUID_QR_CODE; }
    else if (!strcmp(aUUID, BTRMGR_LEONBRDG_UUID_PROVISION_STATUS)) { lDiagChar = BTRMGR_LE_ONBRDG_UUID_PROVISION_STATUS; }
    else if (!strcmp(aUUID, BTRMGR_LEONBRDG_UUID_PUBLIC_KEY)) { lDiagChar = BTRMGR_LE_ONBRDG_UUID_PUBLIC_KEY; }
    else if (!strcmp(aUUID, BTRMGR_LEONBRDG_UUID_WIFI_CONFIG)) { lDiagChar = BTRMGR_LE_ONBRDG_UUID_WIFI_CONFIG; }
    else if (!strcmp(aUUID, BTRMGR_DEVICE_STATUS_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_DEVICESTATUS; }
    else if (!strcmp(aUUID, BTRMGR_FWDOWNLOAD_STATUS_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_FWDOWNLOADSTATUS; }
    else if (!strcmp(aUUID, BTRMGR_WEBPA_STATUS_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_WEBPASTATUS; }
    else if (!strcmp(aUUID, BTRMGR_WIFIRADIO1_STATUS_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_WIFIRADIO1STATUS; }
    else if (!strcmp(aUUID, BTRMGR_WIFIRADIO2_STATUS_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_WIFIRADIO2STATUS; }
    else if (!strcmp(aUUID, BTRMGR_RF_STATUS_UUID)) { lDiagChar = BTRMGR_SYS_DIAG_RFSTATUS; }
    else if (!strcmp(aUUID, BTRMGR_DEVICE_MAC)) { lDiagChar = BTRMGR_SYS_DIAG_DEVICEMAC; }
    else if (!strcmp(aUUID, BTRMGR_COLUMBO_START)) { lDiagChar = BTRMGR_SYSDIAG_COLUMBO_START; }
    else if (!strcmp(aUUID, BTRMGR_COLUMBO_STOP)) { lDiagChar = BTRMGR_SYSDIAG_COLUMBO_STOP; }
    else { lDiagChar = BTRMGR_SYS_DIAG_UNKNOWN; }

    return lDiagChar;
}

#if 0
static void btrMgr_AddColumboGATTInfo(
){
    char l16BitUUID[4] = "";
    char l128BitUUID[BTRMGR_NAME_LEN_MAX] = "";

    strncpy(l128BitUUID, BTRMGR_COLUMBO_UUID, BTRMGR_NAME_LEN_MAX - 1);
    snprintf(l16BitUUID, 5, "%s", l128BitUUID);

    /* add to GAP advertisement */
    BTRCore_SetServiceUUIDs(ghBTRCoreHdl, l16BitUUID);
    /* Add to GATT Info */
    BTRCore_SetServiceInfo(ghBTRCoreHdl, BTRMGR_COLUMBO_UUID, TRUE);
    BTRCore_SetGattInfo(ghBTRCoreHdl, BTRMGR_COLUMBO_UUID, BTRMGR_COLUMBO_START, 0x2, " ", enBTRCoreLePropGChar);           /* uuid_columbo_service_char_start */
    BTRCore_SetGattInfo(ghBTRCoreHdl, BTRMGR_COLUMBO_UUID, BTRMGR_COLUMBO_STOP, 0x2, " ", enBTRCoreLePropGChar);           /* uuid_columbo_service_char_stop */
    BTRCore_SetGattInfo(ghBTRCoreHdl, BTRMGR_COLUMBO_UUID, BTRMGR_COLUMBO_STATUS, 0x1, " ", enBTRCoreLePropGChar);           /* uuid_columbo_service_char_status */
    BTRCore_SetGattInfo(ghBTRCoreHdl, BTRMGR_COLUMBO_UUID, BTRMGR_COLUMBO_REPORT, 0x1, " ", enBTRCoreLePropGChar);           /* uuid_columbo_service_char_report */
}

static void btrMgr_AddStandardAdvGattInfo(
){
    char lPropertyValue[BTRMGR_MAX_STR_LEN] = "";

    BTRMGRLOG_INFO("Adding default local services : DEVICE_INFORMATION_UUID - 0x180a, RDKDIAGNOSTICS_UUID - 0xFDB9\n");
    BTRMGR_LE_SetServiceInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, 1);
    BTRMGR_LE_SetServiceInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, 1);

    /* Get model number */
    BTRMGR_SysDiagInfo(0, BTRMGR_SYSTEM_ID_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGRLOG_INFO("Adding char for the default local services : 0x180a, 0xFDB9\n");
    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SYSTEM_ID_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                                /* system ID            */
    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_MODEL_NUMBER_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                             /* model number         */
    /*Get HW revision*/
    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_HARDWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Hardware revision    */
    /* Get serial number */
    BTRMGR_SysDiagInfo(0, BTRMGR_SERIAL_NUMBER_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SERIAL_NUMBER_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                            /* serial number        */
    /* Get firmware revision */
    BTRMGR_SysDiagInfo(0, BTRMGR_FIRMWARE_REVISION_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_FIRMWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Firmware revision    */
    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SOFTWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Software revision    */
    /* Get manufacturer name */
    BTRMGR_SysDiagInfo(0, BTRMGR_MANUFACTURER_NAME_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_MANUFACTURER_NAME_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Manufacturer name    */

    /* 0xFDB9 */
    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_DEVICE_STATUS_UUID, 0x1, "READY", BTRMGR_LE_PROP_CHAR);                                       /* DeviceStatus         */
    BTRMGR_SysDiagInfo(0, BTRMGR_FWDOWNLOAD_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_FWDOWNLOAD_STATUS_UUID, 0x103, lPropertyValue, BTRMGR_LE_PROP_CHAR);                          /* FWDownloadStatus     */
    BTRMGR_SysDiagInfo(0, BTRMGR_WEBPA_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_WEBPA_STATUS_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                                 /* WebPAStatus          */
    BTRMGR_SysDiagInfo(0, BTRMGR_WIFIRADIO1_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_WIFIRADIO1_STATUS_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                            /* WiFiRadio1Status     */
    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_WIFIRADIO2_STATUS_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                            /* WiFiRadio2Status     */
    BTRMGR_SysDiagInfo(0, BTRMGR_RF_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_RF_STATUS_UUID, 0x1, "NOT CONNECTED", BTRMGR_LE_PROP_CHAR);                                   /* RFStatus             */
}
#endif
/*  Local Op Threads */


/* Interfaces - Public Functions */
BTRMGR_Result_t
BTRMGR_Init (
    void
) {
    BTRMGR_Result_t lenBtrMgrResult= BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet    lenBtrCoreRet  = enBTRCoreSuccess;
    eBTRMgrRet      lenBtrMgrPiRet = eBTRMgrFailure;
    eBTRMgrRet      lenBtrMgrSdRet = eBTRMgrFailure;
    GMainLoop*      pMainLoop      = NULL;
    GThread*        pMainLoopThread= NULL;

    char            lpcBtVersion[BTRCORE_STR_LEN] = {'\0'};

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
    //gIsDiscoveryInProgress = 0;


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
    if (enBTRCoreSuccess != BTRCore_GetAdapter(ghBTRCoreHdl, &gDefaultAdapterContext)) {
        BTRMGRLOG_WARN("Bluetooth adapter NOT received..!!!!\n");
        return  BTRMGR_RESULT_GENERIC_FAILURE;
    }

    BTRMGRLOG_DEBUG ("Aquired default Adapter; Path is %s\n", gDefaultAdapterContext.pcAdapterPath);
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

    btrMgr_SetBgDiscoveryType (BTRMGR_DEVICE_OP_TYPE_LE);

    /* Initialize the Paired Device List for Default adapter */
    BTRMGR_GetPairedDevices (gDefaultAdapterContext.adapter_number, &gListOfPairedDevices);


    // Init Persistent handles
    if ((lenBtrMgrPiRet = BTRMgr_PI_Init(&ghBTRMgrPiHdl)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Could not initialize PI module\n");
    }

    // Init SysDiag handles
    if ((lenBtrMgrSdRet = BTRMgr_SD_Init(&ghBTRMgrSdHdl, btrMgr_SDStatusCb, NULL)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Could not initialize SD - SysDiag module\n");
    }

    btrMgr_CheckAudioInServiceAvailability();
    btrMgr_CheckHidGamePadServiceAvailability();
#if 0
    // Set the beacon and start the advertisement

    char lDeviceMAC[BTRMGR_MAX_STR_LEN] = "";

    BTRMGR_SysDiagInfo(0, BTRMGR_DEVICE_MAC, lDeviceMAC, BTRMGR_LE_OP_READ_VALUE);
    strncpy((char*)stCoreCustomAdv.device_mac, lDeviceMAC, strlen(lDeviceMAC));
    /* Add Columbo Gatt info */
    btrMgr_AddColumboGATTInfo();

    BTRMGR_LE_StartAdvertisement(0, &stCoreCustomAdv);
#endif
    pMainLoop   = g_main_loop_new (NULL, FALSE);
    gpvMainLoop = (void*)pMainLoop;


    pMainLoopThread   = g_thread_new("btrMgr_g_main_loop_Task", btrMgr_g_main_loop_Task, gpvMainLoop);
    gpvMainLoopThread = (void*)pMainLoopThread;
    if ((pMainLoop == NULL) || (pMainLoopThread == NULL)) {
        BTRMGRLOG_ERROR ("Could not initialize g_main module\n");
        BTRMGR_DeInit();
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_DeInit (
    void
) {
    eBTRMgrRet                      lenBtrMgrRet      = eBTRMgrSuccess;
    enBTRCoreRet                    lenBtrCoreRet     = enBTRCoreSuccess;
    BTRMGR_Result_t                 lenBtrMgrResult   = BTRMGR_RESULT_SUCCESS;
    BTRMGR_DiscoveryHandle_t*       ldiscoveryHdl     = NULL;
    unsigned short                  ui16LoopIdx       = 0;
    BTRMGR_ConnectedDevicesList_t   lstConnectedDevices;
    unsigned int                    ui32sleepTimeOut = 1;


    if (btrMgr_isTimeOutSet()) {
        btrMgr_ClearDiscoveryHoldOffTimer();
        gDiscHoldOffTimeOutCbData = 0;
    }

    if ((ldiscoveryHdl = btrMgr_GetDiscoveryInProgress())) {
        lenBtrMgrRet = btrMgr_StopDeviceDiscovery (0, ldiscoveryHdl);
        BTRMGRLOG_DEBUG ("Exit Discovery Status = %d\n", lenBtrMgrRet);
    }

    if ((lenBtrMgrResult = BTRMGR_GetConnectedDevices(0, &lstConnectedDevices)) == BTRMGR_RESULT_SUCCESS) {
        BTRMGRLOG_DEBUG ("Connected Devices = %d\n", lstConnectedDevices.m_numOfDevices);

        for (ui16LoopIdx = 0; ui16LoopIdx < lstConnectedDevices.m_numOfDevices; ui16LoopIdx++) {
            unsigned int            ui32confirmIdx  = 2;
            enBTRCoreDeviceType     lenBtrCoreDevTy = enBTRCoreUnknown;
            enBTRCoreDeviceClass    lenBtrCoreDevCl = enBTRCore_DC_Unknown;

            BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, lstConnectedDevices.m_deviceProperty[ui16LoopIdx].m_deviceHandle, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
            if (BTRCore_DisconnectDevice(ghBTRCoreHdl, lstConnectedDevices.m_deviceProperty[ui16LoopIdx].m_deviceHandle, lenBtrCoreDevTy) != enBTRCoreSuccess) {
                BTRMGRLOG_ERROR ("Failed to Disconnect - %llu\n", lstConnectedDevices.m_deviceProperty[ui16LoopIdx].m_deviceHandle);
            }

            do {
                unsigned int ui32sleepIdx = 2;

                do {
                    sleep(ui32sleepTimeOut);
                    lenBtrCoreRet = BTRCore_GetDeviceDisconnected(ghBTRCoreHdl, lstConnectedDevices.m_deviceProperty[ui16LoopIdx].m_deviceHandle, lenBtrCoreDevTy);
                } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
            } while (--ui32confirmIdx);
        }
    }

    if (gpvMainLoop) {
        g_main_loop_quit(gpvMainLoop);
    }

    if (gpvMainLoopThread) {
        g_thread_join(gpvMainLoopThread);
        gpvMainLoopThread = NULL;
    }

    if (gpvMainLoop) {
        g_main_loop_unref(gpvMainLoop);
        gpvMainLoop = NULL;
    }


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (ghBTRMgrSdHdl) {
        lenBtrMgrRet = BTRMgr_SD_DeInit(ghBTRMgrSdHdl);
        ghBTRMgrSdHdl = NULL;
        BTRMGRLOG_ERROR ("SD Module DeInited = %d\n", lenBtrMgrRet);
    }

    if (ghBTRMgrPiHdl) {
        lenBtrMgrRet = BTRMgr_PI_DeInit(ghBTRMgrPiHdl);
        ghBTRMgrPiHdl = NULL;
        BTRMGRLOG_ERROR ("PI Module DeInited = %d\n", lenBtrMgrRet);
    }


    if (ghBTRCoreHdl) {
        lenBtrCoreRet = BTRCore_DeInit(ghBTRCoreHdl);
        ghBTRCoreHdl = NULL;
        BTRMGRLOG_ERROR ("BTRCore DeInited; Now will we exit the app = %d\n", lenBtrCoreRet);
    }

    lenBtrMgrResult =  ((lenBtrMgrRet == eBTRMgrSuccess) &&
                        (lenBtrCoreRet == enBTRCoreSuccess)) ? BTRMGR_RESULT_SUCCESS : BTRMGR_RESULT_GENERIC_FAILURE;
    BTRMGRLOG_DEBUG ("Exit Status = %d\n", lenBtrMgrResult);


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
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    const char*     pAdapterPath    = NULL;
    char            name[BTRMGR_NAME_LEN_MAX] = {'\0'};

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


    lenBtrCoreRet = BTRCore_DisableAdapter(ghBTRCoreHdl,&gDefaultAdapterContext);

    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to disable Adapter - %s\n", pAdapterPath);
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_INFO ("Bluetooth Adapter Disabled Successfully - %s\n", pAdapterPath);
        sleep(1);

        lenBtrCoreRet = BTRCore_SetAdapterName(ghBTRCoreHdl, pAdapterPath, name);
        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to set Adapter Name\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }

        
        lenBtrCoreRet = BTRCore_EnableAdapter(ghBTRCoreHdl,&gDefaultAdapterContext);
        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to enable Adapter\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
         }
        else {
            BTRMGRLOG_INFO ("Bluetooth Adapter Enabled Successfully - %s\n", pAdapterPath);
        }
    }
    
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
    strncpy (pNameOfAdapter, name, (strlen(pNameOfAdapter) > BTRMGR_NAME_LEN_MAX -1) ? BTRMGR_NAME_LEN_MAX -1 : strlen(name));
    pNameOfAdapter[(strlen(pNameOfAdapter) > BTRMGR_NAME_LEN_MAX -1) ? BTRMGR_NAME_LEN_MAX -1 : strlen(name)] = '\0';


    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_SetAdapterPowerStatus (
    unsigned char   aui8AdapterIdx,
    unsigned char   power_status
) {
    BTRMGR_Result_t         lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
    enBTRCoreDeviceType     lenBTRCoreDevTy = enBTRCoreSpeakers;
    enBTRCoreDeviceClass    lenBTRCoreDevCl = enBTRCore_DC_Unknown;
    const char*             pAdapterPath    = NULL;

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
        lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ghBTRMgrDevHdlCurStreaming, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
        BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBTRCoreDevTy, lenBTRCoreDevCl);

        if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
            /* Streaming-Out is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to stop Playback. Going Ahead to power off Adapter.-Out\n");
            }
        }
        else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
            /* Streaming-In is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingIn(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to stop Playback. Going Ahead to  power off Adapter.-In\n");
            }
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

static BTRMGR_Result_t
BTRMGR_StartDeviceDiscovery_Internal (
    unsigned char                aui8AdapterIdx, 
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    BTRMGR_Result_t     lenBtrMgrResult      = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet        lenBtrCoreRet        = enBTRCoreSuccess;
    enBTRCoreDeviceType lenBTRCoreDeviceType = enBTRCoreUnknown;
    const char*         pAdapterPath         = NULL;
    BTRMGR_DiscoveryHandle_t* ldiscoveryHdl  = NULL;

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
    // TODO Try to move this check logically into btrMgr_PreCheckDiscoveryStatus
    if ((ldiscoveryHdl = btrMgr_GetDiscoveryInProgress())) {
        if (aenBTRMgrDevOpT == btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl)) {
            BTRMGRLOG_WARN ("[%s] Scan already in Progress...\n"
                           , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl)));
            return BTRMGR_RESULT_SUCCESS;
        }
    }

    if (eBTRMgrSuccess != btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, aenBTRMgrDevOpT)) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Rejected !!!\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
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
    case BTRMGR_DEVICE_OP_TYPE_HID:
        lenBTRCoreDeviceType = enBTRCoreHID;
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
        {   /* Max 3 sec timeout - Polled at 5ms second interval */
            unsigned int ui32sleepIdx = 600;

            do {
                usleep(5000);
            } while ((!gIsAdapterDiscovering) && (--ui32sleepIdx));
        }

        if (!gIsAdapterDiscovering) {
            BTRMGRLOG_WARN ("Discovery is not yet Started !!!\n");
        }
        else {
            BTRMGRLOG_INFO ("Discovery started Successfully\n");
        }

        btrMgr_SetDiscoveryHandle (aenBTRMgrDevOpT, BTRMGR_DISCOVERY_ST_STARTED);
    }

    return lenBtrMgrResult;
}


static BTRMGR_Result_t
BTRMGR_StopDeviceDiscovery_Internal (
    unsigned char                aui8AdapterIdx,
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    BTRMGR_Result_t     lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet        lenBtrCoreRet   = enBTRCoreSuccess;
    enBTRCoreDeviceType lenBTRCoreDeviceType = enBTRCoreUnknown;
    const char*         pAdapterPath    = NULL;
    BTRMGR_DiscoveryHandle_t* ahdiscoveryHdl = NULL;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!(pAdapterPath = btrMgr_GetAdapterPath(aui8AdapterIdx))) {
        BTRMGRLOG_ERROR ("Failed to get adapter path\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (!(ahdiscoveryHdl = btrMgr_GetDiscoveryInProgress())) {
        BTRMGRLOG_WARN("Scanning is not running now\n");
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
    case BTRMGR_DEVICE_OP_TYPE_HID:
        lenBTRCoreDeviceType = enBTRCoreHID;
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

#if 0
        Let we not make the OutTask Context of BTRCore Layer wait here
        TODO: Confirm Discovery Stop

        {   /* Max 3 sec timeout - Polled at 500ms second interval */
            unsigned int ui32sleepIdx = 6;

            while ((gIsAdapterDiscovering) && (ui32sleepIdx--)) {
                usleep(500000);
            }
        }

        if (gIsAdapterDiscovering) {
            BTRMGRLOG_WARN ("Discovery is not yet Stopped !!!\n");
        }
        else {
            BTRMGRLOG_INFO ("Discovery Stopped Successfully\n");
        }
#endif 

        if (ahdiscoveryHdl) {
            if ((btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl) == BTRMGR_DEVICE_OP_TYPE_LE && aenBTRMgrDevOpT == BTRMGR_DEVICE_OP_TYPE_LE)
                || btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl) != BTRMGR_DEVICE_OP_TYPE_LE) {

                btrMgr_SetDiscoveryState (ahdiscoveryHdl, BTRMGR_DISCOVERY_ST_STOPPED);
                if (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl) != BTRMGR_DEVICE_OP_TYPE_LE &&
                    btrMgr_GetDiscoveryState(&ghBTRMgrBgDiscoveryHdl) == BTRMGR_DISCOVERY_ST_PAUSED) {
                    btrMgr_ClearDiscoveryHoldOffTimer();
                    btrMgr_SetDiscoveryHoldOffTimer(aui8AdapterIdx);
                }
            }
            else {
                btrMgr_SetDiscoveryState (ahdiscoveryHdl, BTRMGR_DISCOVERY_ST_PAUSED);
                btrMgr_SetDiscoveryHoldOffTimer(aui8AdapterIdx);
            }
        }
        else if (aenBTRMgrDevOpT == BTRMGR_DEVICE_OP_TYPE_LE) {
            if (btrMgr_GetDiscoveryState(&ghBTRMgrBgDiscoveryHdl) == BTRMGR_DISCOVERY_ST_PAUSED) {
                btrMgr_ClearDiscoveryHoldOffTimer();
                btrMgr_SetDiscoveryState (&ghBTRMgrBgDiscoveryHdl, BTRMGR_DISCOVERY_ST_STOPPED);
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
    BTRMGR_Result_t result = BTRMGR_RESULT_GENERIC_FAILURE;

    gIsDiscoveryOpInternal = FALSE;

    memset (&gListOfDiscoveredDevices, 0, sizeof(gListOfDiscoveredDevices));
    result = BTRMGR_StartDeviceDiscovery_Internal(aui8AdapterIdx, aenBTRMgrDevOpT);

    if ((result == BTRMGR_RESULT_SUCCESS) && gfpcBBTRMgrEventOut) {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        lstEventMessage.m_adapterIndex = aui8AdapterIdx;
        if (gIsAdapterDiscovering) {
            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED;
        }

        gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
    }

    return result;
}


BTRMGR_Result_t
BTRMGR_StopDeviceDiscovery (
    unsigned char                aui8AdapterIdx,
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    BTRMGR_Result_t result = BTRMGR_RESULT_GENERIC_FAILURE;

    gIsDiscoveryOpInternal = FALSE;
    result = BTRMGR_StopDeviceDiscovery_Internal(aui8AdapterIdx, aenBTRMgrDevOpT);

    if ((result == BTRMGR_RESULT_SUCCESS) && gfpcBBTRMgrEventOut) {
        BTRMGR_EventMessage_t lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));

        lstEventMessage.m_adapterIndex = aui8AdapterIdx;
        if (!gIsAdapterDiscovering) {
            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
        }

        gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
    }

    return result;
}


BTRMGR_Result_t
BTRMGR_GetDiscoveryStatus (
    unsigned char                   aui8AdapterIdx, 
    BTRMGR_DiscoveryStatus_t*       isDiscoveryInProgress,
    BTRMGR_DeviceOperationType_t*   aenBTRMgrDevOpT
) {
    BTRMGR_Result_t result = BTRMGR_RESULT_SUCCESS;
    BTRMGR_DiscoveryHandle_t* ldiscoveryHdl  = NULL;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }
    else if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!isDiscoveryInProgress) || (!aenBTRMgrDevOpT)) {
            BTRMGRLOG_ERROR ("Input is invalid\n");
            return BTRMGR_RESULT_INVALID_INPUT;
    }

    if ((ldiscoveryHdl = btrMgr_GetDiscoveryInProgress())) {
        *isDiscoveryInProgress = BTRMGR_DISCOVERY_STATUS_IN_PROGRESS;
        *aenBTRMgrDevOpT = btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl);
        BTRMGRLOG_WARN ("[%s] Scan already in Progress\n"
                               , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl)));
    }
    else {
         *isDiscoveryInProgress = BTRMGR_DISCOVERY_STATUS_OFF;
         *aenBTRMgrDevOpT = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;
    }
    return result;
}

BTRMGR_Result_t
BTRMGR_GetDiscoveredDevices (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_DiscoveredDevicesList_t* pDiscoveredDevices
) {
    memset (&gListOfDiscoveredDevices, 0, sizeof(gListOfDiscoveredDevices));
    return BTRMGR_GetDiscoveredDevices_Internal(aui8AdapterIdx, pDiscoveredDevices);
}


static BTRMGR_Result_t
BTRMGR_GetDiscoveredDevices_Internal (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_DiscoveredDevicesList_t* pDiscoveredDevices
) {
    BTRMGR_Result_t                 lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                    lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCoreScannedDevicesCount    lstBtrCoreListOfSDevices;
    BTRMGR_DeviceType_t         lenBtrMgrDevType  = BTRMGR_DEVICE_TYPE_UNKNOWN;
    int i = 0;
    int j = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pDiscoveredDevices)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    lenBtrCoreRet = BTRCore_GetListOfScannedDevices(ghBTRCoreHdl, &lstBtrCoreListOfSDevices);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get list of discovered devices\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        memset (pDiscoveredDevices, 0, sizeof(BTRMGR_DiscoveredDevicesList_t));

        if (lstBtrCoreListOfSDevices.numberOfDevices) {
            BTRMGR_DiscoveredDevices_t *lpstBtrMgrSDevice = NULL;

            pDiscoveredDevices->m_numOfDevices = (lstBtrCoreListOfSDevices.numberOfDevices < BTRMGR_DEVICE_COUNT_MAX) ? 
                                                    lstBtrCoreListOfSDevices.numberOfDevices : BTRMGR_DEVICE_COUNT_MAX;

            for (i = 0,j = 0; j < pDiscoveredDevices->m_numOfDevices; i++,j++) {
                lenBtrMgrDevType =  btrMgr_MapDeviceTypeFromCore(lstBtrCoreListOfSDevices.devices[i].enDeviceType);
                if (!gIsHidGamePadEnabled && (lenBtrMgrDevType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD)) {
                    j--;
                    pDiscoveredDevices->m_numOfDevices--;
                    BTRMGRLOG_WARN ("HID GamePad RFC not enable, HID GamePad devices are not listing \n");
                    continue;
                }

                lpstBtrMgrSDevice = &pDiscoveredDevices->m_deviceProperty[j];

                lpstBtrMgrSDevice->m_deviceHandle   = lstBtrCoreListOfSDevices.devices[i].tDeviceId;
                lpstBtrMgrSDevice->m_signalLevel    = lstBtrCoreListOfSDevices.devices[i].i32RSSI;
                lpstBtrMgrSDevice->m_rssi           = btrMgr_MapSignalStrengthToRSSI (lstBtrCoreListOfSDevices.devices[i].i32RSSI);
                lpstBtrMgrSDevice->m_deviceType     = btrMgr_MapDeviceTypeFromCore(lstBtrCoreListOfSDevices.devices[i].enDeviceType);
                lpstBtrMgrSDevice->m_isPairedDevice = btrMgr_GetDevPaired(lstBtrCoreListOfSDevices.devices[i].tDeviceId);

                strncpy(lpstBtrMgrSDevice->m_name,          lstBtrCoreListOfSDevices.devices[i].pcDeviceName,   (BTRMGR_NAME_LEN_MAX - 1));
                strncpy(lpstBtrMgrSDevice->m_deviceAddress, lstBtrCoreListOfSDevices.devices[i].pcDeviceAddress,(BTRMGR_NAME_LEN_MAX - 1));

                if (lstBtrCoreListOfSDevices.devices[i].bDeviceConnected) {
                    lpstBtrMgrSDevice->m_isConnected = 1;
                }

                if (!btrMgr_CheckIfDevicePrevDetected(lpstBtrMgrSDevice->m_deviceHandle) && (gListOfDiscoveredDevices.m_numOfDevices < BTRMGR_DEVICE_COUNT_MAX)) {
                    /* Update BTRMgr DiscoveredDev List cache */
                    memcpy(&gListOfDiscoveredDevices.m_deviceProperty[gListOfDiscoveredDevices.m_numOfDevices], lpstBtrMgrSDevice, sizeof(BTRMGR_DiscoveredDevices_t));
                    gListOfDiscoveredDevices.m_numOfDevices++;
                }
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
    BTRMGR_Result_t         lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet            lenBtrCoreRet       = enBTRCoreSuccess;
    BTRMGR_Events_t         lBtMgrOutEvent      = -1;
    unsigned char           ui8reActivateAgent  = 0;
    enBTRCoreDeviceType     lenBTRCoreDevTy     = enBTRCoreSpeakers;
    enBTRCoreDeviceClass    lenBTRCoreDevCl     = enBTRCore_DC_Unknown;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);

    /* Update the Paired Device List */
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);
    if (1 == btrMgr_GetDevPaired(ahBTRMgrDevHdl)) {
        BTRMGRLOG_INFO ("Already a Paired Device; Nothing Done...\n");
        return BTRMGR_RESULT_SUCCESS;
    }

    BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBTRCoreDevTy, lenBTRCoreDevCl);

    if (!gIsHidGamePadEnabled && (lenBTRCoreDevCl == enBTRCore_DC_HID_GamePad)) {
        BTRMGRLOG_WARN ("BTR HID GamePad is currently Disabled\n");
       return BTRMGR_RESULT_GENERIC_FAILURE;
    }
    if (!gIsAudioInEnabled && ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn))) {
        BTRMGRLOG_WARN ("Pairing Rejected - BTR AudioIn is currently Disabled!\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }
    /* We always need a agent to get the pairing done.. if no agent registered, default agent will be used.
     * But we will not able able to get the PIN and other stuff received at our level. We have to have agent
     * for pairing process..
     *
     * We are using the default-agent for the audio devices and we have it deployed to field already.
     *
     * Since HID is new device and to not to break the Audio devices, BTMGR specific Agent is registered only for HID devices.
     */
    if (lenBTRCoreDevTy != enBTRCoreHID) {
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
    }
    else
        BTRMGRLOG_DEBUG ("For HID Devices, let us use the agent\n");

    if (enBTRCoreSuccess != BTRCore_PairDevice(ghBTRCoreHdl, ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Failed to pair a device\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_PAIRING_FAILED;
    }
    else {
        lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
    }

    /* The Paring is not a blocking call for HID. So we can sent the pairing complete msg when paring actually completed and gets a notification.
     * But the pairing failed, we must send it right here. */
    if ((lenBtrMgrResult != BTRMGR_RESULT_SUCCESS) ||
        ((lenBtrMgrResult == BTRMGR_RESULT_SUCCESS) &&
         ((lenBTRCoreDevTy != enBTRCoreHID) && (lenBTRCoreDevTy != enBTRCoreMobileAudioIn) && (lenBTRCoreDevTy != enBTRCorePCAudioIn)))) {
        BTRMGR_EventMessage_t  lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));
        btrMgr_GetDiscoveredDevInfo (ahBTRMgrDevHdl, &lstEventMessage.m_discoveredDevice);

        if (lstEventMessage.m_discoveredDevice.m_deviceHandle != ahBTRMgrDevHdl) {
            BTRMGRLOG_WARN ("Attempted to pair Undiscovered device - %lld\n", ahBTRMgrDevHdl);
            lstEventMessage.m_discoveredDevice.m_deviceHandle = ahBTRMgrDevHdl;
        }

        lstEventMessage.m_adapterIndex = aui8AdapterIdx;
        lstEventMessage.m_eventType    = lBtMgrOutEvent;
        
        if (gfpcBBTRMgrEventOut) {
            gfpcBBTRMgrEventOut(lstEventMessage);
        }
    }

    /* Update the Paired Device List */
    if(lenBTRCoreDevTy == enBTRCoreHID){
       sleep(10);
    }
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);
    int j ;
    for (j = 0; j <= gListOfPairedDevices.m_numOfDevices; j++) {
         if ((ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) && ( BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE == lBtMgrOutEvent )) {
             BTRMGRLOG_INFO ("Paired Successfully -  %llu - Name - %s\n",ahBTRMgrDevHdl,gListOfPairedDevices.m_deviceProperty[j].m_name);
             break;
         }
    }
   
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

    btrMgr_PostCheckDiscoveryStatus(aui8AdapterIdx, btrMgr_MapDeviceOpFromDeviceType( btrMgr_MapDeviceTypeFromCore(lenBTRCoreDevCl)));

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_UnpairDevice (
    unsigned char       aui8AdapterIdx,
    BTRMgrDeviceHandle  ahBTRMgrDevHdl
) {
    BTRMGR_Result_t         lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet            lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType     lenBTRCoreDevTy     = enBTRCoreSpeakers;
    enBTRCoreDeviceClass    lenBTRCoreDevCl     = enBTRCore_DC_Unknown;
    BTRMGR_Events_t         lBtMgrOutEvent      = -1;
    unsigned char           ui8reActivateAgent  = 0;
    BTRMGR_EventMessage_t   lstEventMessage;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);

    /* Get the latest Paired Device List; This is added as the developer could add a device thro test application and try unpair thro' UI */
    BTRMGR_GetPairedDevices (aui8AdapterIdx, &gListOfPairedDevices);
    memset (&lstEventMessage, 0, sizeof(lstEventMessage));
    btrMgr_GetPairedDevInfo (ahBTRMgrDevHdl, &lstEventMessage.m_pairedDevice);

    if (!lstEventMessage.m_pairedDevice.m_deviceHandle) {
        BTRMGRLOG_ERROR ("Not a Paired device...\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBTRCoreDevTy, lenBTRCoreDevCl);

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
        if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
            /* Streaming-Out is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("BTRMGR_UnpairDevice :This device is being Connected n Playing. Failed to stop Playback. Going Ahead to unpair.-Out\n");
            }
        }
        else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
            /* Streaming-In is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingIn(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("BTRMGR_UnpairDevice :This device is being Connected n Playing. Failed to stop Playback. Going Ahead to unpair.-In\n");
            }
        }
    }


    if (enBTRCoreSuccess != BTRCore_UnPairDevice(ghBTRCoreHdl, ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Failed to unpair\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED;
    }
    else {
        int j;
        for (j = 0; j <= gListOfPairedDevices.m_numOfDevices; j++) {
             if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
                 BTRMGRLOG_INFO ("Unpaired Successfully -  %llu - Name -  %s\n",ahBTRMgrDevHdl,gListOfPairedDevices.m_deviceProperty[j].m_name);
                 break;
             }
        }   
        lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE;
    }


    {
        lstEventMessage.m_adapterIndex = aui8AdapterIdx;
        lstEventMessage.m_eventType    = lBtMgrOutEvent;

        if (gfpcBBTRMgrEventOut) {
            gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
        }
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

    btrMgr_PostCheckDiscoveryStatus(aui8AdapterIdx, btrMgr_MapDeviceOpFromDeviceType( btrMgr_MapDeviceTypeFromCore(lenBTRCoreDevCl)));

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetPairedDevices (
    unsigned char                aui8AdapterIdx,
    BTRMGR_PairedDevicesList_t*  pPairedDevices
) {
    BTRMGR_Result_t             lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount lstBtrCoreListOfPDevices;
    int i = 0;
    int j = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pPairedDevices)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    lstBtrCoreListOfPDevices.numberOfDevices = 0;
    for (i = 0; i < BTRCORE_MAX_NUM_BT_DEVICES; i++) {
        memset (&lstBtrCoreListOfPDevices.devices[i], 0, sizeof(stBTRCoreBTDevice));
    }


    lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &lstBtrCoreListOfPDevices);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get list of paired devices\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        pPairedDevices->m_numOfDevices = 0;
        for (i = 0; i < BTRMGR_DEVICE_COUNT_MAX; i++) {
            memset (&pPairedDevices->m_deviceProperty[i], 0, sizeof(BTRMGR_PairedDevices_t));     /* Reset the values to 0 */
        }

        if (lstBtrCoreListOfPDevices.numberOfDevices) {
            BTRMGR_PairedDevices_t* lpstBtrMgrPDevice = NULL;

            pPairedDevices->m_numOfDevices = (lstBtrCoreListOfPDevices.numberOfDevices < BTRMGR_DEVICE_COUNT_MAX) ? 
                                                lstBtrCoreListOfPDevices.numberOfDevices : BTRMGR_DEVICE_COUNT_MAX;

            for (i = 0; i < pPairedDevices->m_numOfDevices; i++) {
                lpstBtrMgrPDevice = &pPairedDevices->m_deviceProperty[i];

                lpstBtrMgrPDevice->m_deviceHandle = lstBtrCoreListOfPDevices.devices[i].tDeviceId;
                lpstBtrMgrPDevice->m_deviceType   = btrMgr_MapDeviceTypeFromCore (lstBtrCoreListOfPDevices.devices[i].enDeviceType);

                strncpy(lpstBtrMgrPDevice->m_name,          lstBtrCoreListOfPDevices.devices[i].pcDeviceName,   (BTRMGR_NAME_LEN_MAX - 1));
                strncpy(lpstBtrMgrPDevice->m_deviceAddress, lstBtrCoreListOfPDevices.devices[i].pcDeviceAddress,(BTRMGR_NAME_LEN_MAX - 1));

                BTRMGRLOG_INFO ("Paired Device ID = %lld Connected = %d\n", lstBtrCoreListOfPDevices.devices[i].tDeviceId, lstBtrCoreListOfPDevices.devices[i].bDeviceConnected);

                lpstBtrMgrPDevice->m_serviceInfo.m_numOfService = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService;
                for (j = 0; j < lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                    BTRMGRLOG_TRACE ("Profile ID = %u; Profile Name = %s\n", lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value,
                                                                            lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                    lpstBtrMgrPDevice->m_serviceInfo.m_profileInfo[j].m_uuid = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                    strcpy (lpstBtrMgrPDevice->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                }

                if (lstBtrCoreListOfPDevices.devices[i].bDeviceConnected) {
                    lpstBtrMgrPDevice->m_isConnected = 1;
                }
            }
            /*  Success */
            BTRMGRLOG_TRACE ("Successful\n");
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
    enBTRCoreDeviceType     lenBTRCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass    lenBTRCoreDevCl     = enBTRCore_DC_Unknown;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;

    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
    BTRMGRLOG_DEBUG (" Device Type = %d\t Device Class = %x\n", lenBTRCoreDevTy, lenBTRCoreDevCl);

    if (!gIsHidGamePadEnabled && (lenBTRCoreDevCl == enBTRCore_DC_HID_GamePad)) {
        BTRMGRLOG_WARN ("BTR HID GamePad is currently Disabled\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
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
    BTRMGR_Result_t         lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
    enBTRCoreDeviceType     lenBTRCoreDevTy = enBTRCoreSpeakers;
    enBTRCoreDeviceClass    lenBTRCoreDevCl = enBTRCore_DC_Unknown;
    BTRMGR_DeviceOperationType_t lenBtrMgrDevOpType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBTRCoreDevTy, lenBTRCoreDevCl);

    if (((lenBtrMgrDevOpType = btrMgr_MapDeviceOpFromDeviceType( btrMgr_MapDeviceTypeFromCore(lenBTRCoreDevCl))) == BTRMGR_DEVICE_OP_TYPE_UNKNOWN) &&
        (lenBtrCoreRet   == enBTRCoreFailure) &&
        (lenBTRCoreDevTy == enBTRCoreUnknown)) {
        //TODO: Bad Bad Bad way to treat a Unknown device as LE device, when we are unable to determine from BTRCore
        //      BTRCore_GetDeviceTypeClass. Means device was removed from the lower layer Scanned/Paired devices
        //      After Discovered/Paired lists by operation of another client of BTRMgr
        lenBTRCoreDevTy     = enBTRCoreLE;
        lenBtrMgrDevOpType  = BTRMGR_DEVICE_OP_TYPE_LE;
    }

    if (eBTRMgrSuccess != btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, lenBtrMgrDevOpType)) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Failed !!!\n");
        if (lenBTRCoreDevTy == enBTRCoreLE) {
            btrMgr_PostCheckDiscoveryStatus(aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
        }
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if ((lenBTRCoreDevTy != enBTRCoreLE) && !btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("No Device is connected at this time\n");
        btrMgr_PostCheckDiscoveryStatus(aui8AdapterIdx, lenBtrMgrDevOpType);
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (lenBTRCoreDevTy == enBTRCoreLE && !gIsLeDeviceConnected) {
        BTRMGRLOG_ERROR ("No LE Device is connected at this time\n");
        btrMgr_PostCheckDiscoveryStatus(aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN);
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (lenBTRCoreDevTy != enBTRCoreLE) {

        if (ghBTRMgrDevHdlCurStreaming) {
            if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
                /* Streaming-Out is happening; stop it */
                if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                    BTRMGRLOG_ERROR ("Streamout failed to stop\n");
                }
            }
            else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
                /* Streaming-In is happening; stop it */
                if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingIn(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                    BTRMGRLOG_ERROR ("Streamin failed to stop\n");
                }
            }
        }

        gIsUserInitiated = 1;
    }


    /* connectAs param is unused for now.. */
    lenBtrCoreRet = BTRCore_DisconnectDevice (ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDevTy);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to Disconnect - %llu\n", ahBTRMgrDevHdl);
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;

        {
            BTRMGR_EventMessage_t lstEventMessage;
            memset (&lstEventMessage, 0, sizeof(lstEventMessage));

            btrMgr_GetPairedDevInfo(ahBTRMgrDevHdl, &lstEventMessage.m_pairedDevice);

            lstEventMessage.m_adapterIndex = aui8AdapterIdx;
            lstEventMessage.m_eventType    = BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED;
            lstEventMessage.m_pairedDevice.m_isLowEnergyDevice = (lenBTRCoreDevCl==enBTRCore_DC_Tile)?1:0;//Will make it generic later

            if (gfpcBBTRMgrEventOut) {
                gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
            }
        }
    }
    else {
        int j;
        for (j = 0; j <= gListOfPairedDevices.m_numOfDevices; j++) {
             if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
                 BTRMGRLOG_INFO ("Disconnected Successfully -  %llu - Name -  %s\n",ahBTRMgrDevHdl,gListOfPairedDevices.m_deviceProperty[j].m_name);
                 break;
             }
        }
        if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
            btrMgr_RemovePersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SINK_PROFILE_ID);
        }
        else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
            btrMgr_RemovePersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SRC_PROFILE_ID);
        }
    }


    if (lenBtrMgrResult != BTRMGR_RESULT_GENERIC_FAILURE) {
        /* Max 4 sec timeout - Polled at 1 second interval: Confirmed 2 times */
        unsigned int ui32sleepTimeOut = 1;
        unsigned int ui32confirmIdx = 2;
        
        do {
            unsigned int ui32sleepIdx = 2;

            do {
                sleep(ui32sleepTimeOut);
                lenBtrCoreRet = BTRCore_GetDeviceDisconnected(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDevTy);
            } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
        } while (--ui32confirmIdx);

        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Disconnect from this device - Confirmed\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
            if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
                btrMgr_AddPersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SINK_PROFILE_ID, 1);
            }
            else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
                btrMgr_AddPersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SRC_PROFILE_ID, 1);
            }
        }
        else {
            BTRMGRLOG_DEBUG ("Success Disconnect from this device - Confirmed\n");

            if (lenBTRCoreDevTy == enBTRCoreLE) {
                gIsLeDeviceConnected = 0;
            }
            else if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
                btrMgr_SetDevConnected(ahBTRMgrDevHdl, 0);
                ghBTRMgrDevHdlLastConnected = 0;
            }
            else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
                btrMgr_SetDevConnected(ahBTRMgrDevHdl, 0);
                ghBTRMgrDevHdlLastConnected = 0;
            }
            else if (lenBTRCoreDevTy == enBTRCoreHID) {
                btrMgr_SetDevConnected(ahBTRMgrDevHdl, 0);
            }
            else {
                btrMgr_SetDevConnected(ahBTRMgrDevHdl, 0);
            }
        }
    }

    if (lenBtrMgrDevOpType == BTRMGR_DEVICE_OP_TYPE_LE) {
        lenBtrMgrDevOpType = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;
    }

    btrMgr_PostCheckDiscoveryStatus(aui8AdapterIdx, lenBtrMgrDevOpType);

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_GetConnectedDevices (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_ConnectedDevicesList_t*  pConnectedDevices
) {
    BTRMGR_Result_t              lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                 lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount  lstBtrCoreListOfPDevices;
    stBTRCoreScannedDevicesCount lstBtrCoreListOfSDevices;
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
    memset (&lstBtrCoreListOfPDevices,   0, sizeof(lstBtrCoreListOfPDevices));
    memset (&lstBtrCoreListOfSDevices,   0, sizeof(lstBtrCoreListOfSDevices));

    lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &lstBtrCoreListOfPDevices);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (lstBtrCoreListOfPDevices.numberOfDevices) {
            BTRMGR_ConnectedDevice_t* lpstBtrMgrPDevice = NULL;

            for (i = 0; i < lstBtrCoreListOfPDevices.numberOfDevices; i++) {
                if ((lstBtrCoreListOfPDevices.devices[i].bDeviceConnected) && (pConnectedDevices->m_numOfDevices < BTRMGR_DEVICE_COUNT_MAX)) {
                   lpstBtrMgrPDevice = &pConnectedDevices->m_deviceProperty[pConnectedDevices->m_numOfDevices];

                   lpstBtrMgrPDevice->m_deviceType   = btrMgr_MapDeviceTypeFromCore(lstBtrCoreListOfPDevices.devices[i].enDeviceType);
                   lpstBtrMgrPDevice->m_deviceHandle = lstBtrCoreListOfPDevices.devices[i].tDeviceId;

                    if ((lpstBtrMgrPDevice->m_deviceType == BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET)  ||
                        (lpstBtrMgrPDevice->m_deviceType == BTRMGR_DEVICE_TYPE_HANDSFREE)         ||
                        (lpstBtrMgrPDevice->m_deviceType == BTRMGR_DEVICE_TYPE_LOUDSPEAKER)       ||
                        (lpstBtrMgrPDevice->m_deviceType == BTRMGR_DEVICE_TYPE_HEADPHONES)        ||
                        (lpstBtrMgrPDevice->m_deviceType == BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO)    ||
                        (lpstBtrMgrPDevice->m_deviceType == BTRMGR_DEVICE_TYPE_CAR_AUDIO)         ||
                        (lpstBtrMgrPDevice->m_deviceType == BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE) ){

                        if (lpstBtrMgrPDevice->m_deviceHandle != ghBTRMgrDevHdlCurStreaming)
                            continue;
                    }

                   lpstBtrMgrPDevice->m_vendorID     = lstBtrCoreListOfPDevices.devices[i].ui32VendorId;
                   lpstBtrMgrPDevice->m_isConnected  = 1;

                   strncpy (lpstBtrMgrPDevice->m_name,          lstBtrCoreListOfPDevices.devices[i].pcDeviceName,   (BTRMGR_NAME_LEN_MAX - 1));
                   strncpy (lpstBtrMgrPDevice->m_deviceAddress, lstBtrCoreListOfPDevices.devices[i].pcDeviceAddress,(BTRMGR_NAME_LEN_MAX - 1));

                   lpstBtrMgrPDevice->m_serviceInfo.m_numOfService = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService;
                   for (j = 0; j < lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                       lpstBtrMgrPDevice->m_serviceInfo.m_profileInfo[j].m_uuid = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                       strncpy (lpstBtrMgrPDevice->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX -1);
                       lpstBtrMgrPDevice->m_serviceInfo.m_profileInfo[j].m_profile[BTRMGR_NAME_LEN_MAX -1] = '\0';   ///CID:136654 - Buffer size warning
                   }

                   pConnectedDevices->m_numOfDevices++;
                   BTRMGRLOG_TRACE ("Successfully obtained the connected device information from paried list\n");
                }
            }
        }
        else {
            BTRMGRLOG_WARN("No Device in paired list\n");
        }
    }

    lenBtrCoreRet  = BTRCore_GetListOfScannedDevices (ghBTRCoreHdl, &lstBtrCoreListOfSDevices);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (lstBtrCoreListOfSDevices.numberOfDevices) {
            BTRMGR_ConnectedDevice_t* lpstBtrMgrSDevice =  NULL;

            for (i = 0; i < lstBtrCoreListOfSDevices.numberOfDevices; i++) {
                if ((lstBtrCoreListOfSDevices.devices[i].bDeviceConnected) && (pConnectedDevices->m_numOfDevices < BTRMGR_DEVICE_COUNT_MAX)) {
                    lpstBtrMgrSDevice = &pConnectedDevices->m_deviceProperty[pConnectedDevices->m_numOfDevices];

                    lpstBtrMgrSDevice->m_isConnected  = 1;
                    lpstBtrMgrSDevice->m_deviceHandle = lstBtrCoreListOfSDevices.devices[i].tDeviceId;
                    lpstBtrMgrSDevice->m_vendorID     = lstBtrCoreListOfSDevices.devices[i].ui32VendorId;
                    lpstBtrMgrSDevice->m_deviceType   = btrMgr_MapDeviceTypeFromCore(lstBtrCoreListOfSDevices.devices[i].enDeviceType);

                    strncpy (lpstBtrMgrSDevice->m_name,          lstBtrCoreListOfSDevices.devices[i].pcDeviceName,   (BTRMGR_NAME_LEN_MAX - 1));
                    strncpy (lpstBtrMgrSDevice->m_deviceAddress, lstBtrCoreListOfSDevices.devices[i].pcDeviceAddress,(BTRMGR_NAME_LEN_MAX - 1));

                    lpstBtrMgrSDevice->m_serviceInfo.m_numOfService = lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.numberOfService;
                    for (j = 0; j < lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                        lpstBtrMgrSDevice->m_serviceInfo.m_profileInfo[j].m_uuid = lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                        strncpy (lpstBtrMgrSDevice->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX -1);
                        lpstBtrMgrSDevice->m_serviceInfo.m_profileInfo[j].m_profile[BTRMGR_NAME_LEN_MAX -1] = '\0';
                    }

                    pConnectedDevices->m_numOfDevices++;
                    BTRMGRLOG_TRACE ("Successfully obtained the connected device information from scanned list\n");
                }
            }
        }
        else {
            BTRMGRLOG_WARN("No Device in scan list\n");
        }
    }

    if (enBTRCoreSuccess != lenBtrCoreRet) {
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
    stBTRCorePairedDevicesCount     lstBtrCoreListOfPDevices;
    stBTRCoreScannedDevicesCount    lstBtrCoreListOfSDevices;
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
    memset (&lstBtrCoreListOfPDevices, 0, sizeof(lstBtrCoreListOfPDevices));
    memset (&lstBtrCoreListOfSDevices, 0, sizeof(lstBtrCoreListOfSDevices));
    memset (pDeviceProperty, 0, sizeof(BTRMGR_DevicesProperty_t));

    lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &lstBtrCoreListOfPDevices);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (lstBtrCoreListOfPDevices.numberOfDevices) {

            for (i = 0; i < lstBtrCoreListOfPDevices.numberOfDevices; i++) {
                if (ahBTRMgrDevHdl == lstBtrCoreListOfPDevices.devices[i].tDeviceId) {
                    pDeviceProperty->m_deviceHandle      = lstBtrCoreListOfPDevices.devices[i].tDeviceId;
                    pDeviceProperty->m_deviceType        = btrMgr_MapDeviceTypeFromCore(lstBtrCoreListOfPDevices.devices[i].enDeviceType);
                    pDeviceProperty->m_vendorID          = lstBtrCoreListOfPDevices.devices[i].ui32VendorId;
                    pDeviceProperty->m_isLowEnergyDevice = (pDeviceProperty->m_deviceType==BTRMGR_DEVICE_TYPE_TILE)?1:0; //We shall make it generic later
                    pDeviceProperty->m_isPaired          = 1;

                    strncpy(pDeviceProperty->m_name,          lstBtrCoreListOfPDevices.devices[i].pcDeviceName,    (BTRMGR_NAME_LEN_MAX - 1));
                    strncpy(pDeviceProperty->m_deviceAddress, lstBtrCoreListOfPDevices.devices[i].pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));

                    pDeviceProperty->m_serviceInfo.m_numOfService = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService;
                    for (j = 0; j < lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                        BTRMGRLOG_TRACE ("Profile ID = %d; Profile Name = %s \n", lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value,
                                                                                                   lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                        pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_uuid = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                        strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX -1);
                        pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile[BTRMGR_NAME_LEN_MAX -1] = '\0';  //CID:136651 - Buffer size warning
                    }

                  if (lstBtrCoreListOfPDevices.devices[i].bDeviceConnected) {
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


    if (isFound) {
        BTRMGRLOG_DEBUG("GetDeviceProperties - Paired Device\n");
        return lenBtrMgrResult;
    }


    lenBtrCoreRet  = BTRCore_GetListOfScannedDevices (ghBTRCoreHdl, &lstBtrCoreListOfSDevices);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if (lstBtrCoreListOfSDevices.numberOfDevices) {

            for (i = 0; i < lstBtrCoreListOfSDevices.numberOfDevices; i++) {
                if (ahBTRMgrDevHdl == lstBtrCoreListOfSDevices.devices[i].tDeviceId) {
                    if (!isFound) {
                        pDeviceProperty->m_deviceHandle      = lstBtrCoreListOfSDevices.devices[i].tDeviceId;
                        pDeviceProperty->m_deviceType        = btrMgr_MapDeviceTypeFromCore(lstBtrCoreListOfSDevices.devices[i].enDeviceType);
                        pDeviceProperty->m_vendorID          = lstBtrCoreListOfSDevices.devices[i].ui32VendorId;
                        pDeviceProperty->m_isLowEnergyDevice = (pDeviceProperty->m_deviceType==BTRMGR_DEVICE_TYPE_TILE)?1:0; //We shall make it generic later

                        strncpy(pDeviceProperty->m_name,          lstBtrCoreListOfSDevices.devices[i].pcDeviceName,    (BTRMGR_NAME_LEN_MAX - 1));
                        strncpy(pDeviceProperty->m_deviceAddress, lstBtrCoreListOfSDevices.devices[i].pcDeviceAddress, (BTRMGR_NAME_LEN_MAX - 1));

                        pDeviceProperty->m_serviceInfo.m_numOfService = lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.numberOfService;
                        for (j = 0; j < lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                            BTRMGRLOG_TRACE ("Profile ID = %d; Profile Name = %s \n", lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].uuid_value,
                                                                                                       lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].profile_name);
                            pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_uuid = lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                            strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX -1);
                            pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile[BTRMGR_NAME_LEN_MAX -1] = '\0';

                            if(0 != lstBtrCoreListOfSDevices.devices[i].stAdServiceData[j].len)
                            {
                                fprintf(stderr, "%d\t: %s - ServiceData for UUID : %s \n", __LINE__, __FUNCTION__, lstBtrCoreListOfSDevices.devices[i].stAdServiceData[j].pcUUIDs);
                                strncpy (pDeviceProperty->m_adServiceData[j].m_UUIDs, lstBtrCoreListOfSDevices.devices[i].stAdServiceData[j].pcUUIDs, (BTRMGR_UUID_STR_LEN_MAX - 1));
                                memcpy(pDeviceProperty->m_adServiceData[j].m_ServiceData, lstBtrCoreListOfSDevices.devices[i].stAdServiceData[j].pcData, lstBtrCoreListOfSDevices.devices[i].stAdServiceData[j].len);
                                pDeviceProperty->m_adServiceData[j].m_len = lstBtrCoreListOfSDevices.devices[i].stAdServiceData[j].len;

                                for (int k=0; k < pDeviceProperty->m_adServiceData[j].m_len; k++){
                                    fprintf(stderr, "%d\t: %s - ServiceData[%d] = [%x]\n ", __LINE__, __FUNCTION__, k, pDeviceProperty->m_adServiceData[j].m_ServiceData[k]);
                                }
                            }
                        }
                    }

                    pDeviceProperty->m_signalLevel = lstBtrCoreListOfSDevices.devices[i].i32RSSI;

                    if (lstBtrCoreListOfSDevices.devices[i].bDeviceConnected) {
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

    BTRMGRLOG_DEBUG("GetDeviceProperties - Scanned Device\n");
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

    gIsAudOutStartupInProgress = BTRMGR_STARTUP_AUD_UNKNOWN;
    if (BTRMgr_PI_GetAllProfiles(ghBTRMgrPiHdl, &lstPersistentData) == eBTRMgrFailure) {
        btrMgr_AddPersistentEntry (aui8AdapterIdx, 0, BTRMGR_A2DP_SINK_PROFILE_ID, isConnected);
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    BTRMGRLOG_INFO ("Successfully get all profiles\n");
    BTRCore_GetAdapterAddr(ghBTRCoreHdl, aui8AdapterIdx, lui8adapterAddr);

    if (strcmp(lstPersistentData.adapterId, lui8adapterAddr) == 0) {
        gIsAudOutStartupInProgress = BTRMGR_STARTUP_AUD_INPROGRESS;
        numOfProfiles = lstPersistentData.numOfProfiles;

        BTRMGRLOG_DEBUG ("Adapter matches = %s\n", lui8adapterAddr);
        BTRMGRLOG_DEBUG ("Number of Profiles = %d\n", numOfProfiles);

        for (i32ProfileIdx = 0; i32ProfileIdx < numOfProfiles; i32ProfileIdx++) {
            deviceCount = lstPersistentData.profileList[i32ProfileIdx].numOfDevices;

            for (i32DeviceIdx = 0; i32DeviceIdx < deviceCount ; i32DeviceIdx++) {
                lDeviceHandle   = lstPersistentData.profileList[i32ProfileIdx].deviceList[i32DeviceIdx].deviceId;
                isConnected     = lstPersistentData.profileList[i32ProfileIdx].deviceList[i32DeviceIdx].isConnected;

                if (isConnected && lDeviceHandle) {
                    ghBTRMgrDevHdlLastConnected = lDeviceHandle;
                    if(strcmp(lstPersistentData.profileList[i32ProfileIdx].profileId, BTRMGR_A2DP_SINK_PROFILE_ID) == 0) {
                        char                    lPropValue[BTRMGR_LE_STR_LEN_MAX] = {'\0'};
                         BTRMGR_SysDiagChar_t   lenDiagElement = BTRMGR_SYS_DIAG_POWERSTATE;

                        if (eBTRMgrSuccess != BTRMGR_SysDiag_GetData(ghBTRMgrSdHdl, lenDiagElement, lPropValue)) {
                            gIsAudOutStartupInProgress = BTRMGR_STARTUP_AUD_UNKNOWN;
                            BTRMGRLOG_ERROR("Could not get diagnostic data\n");
                            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
                        }

                        if ((lenBtrMgrResult == BTRMGR_RESULT_GENERIC_FAILURE) ||
                             !strncmp(lPropValue, BTRMGR_SYS_DIAG_PWRST_ON, strlen(BTRMGR_SYS_DIAG_PWRST_ON))) {
                            BTRMGRLOG_INFO ("Streaming to Device  = %lld\n", lDeviceHandle);
                            if (btrMgr_StartAudioStreamingOut(0, lDeviceHandle, aenBTRMgrDevConT, 1, 1, 1) != eBTRMgrSuccess) {
                                BTRMGRLOG_ERROR ("btrMgr_StartAudioStreamingOut - Failure\n");
                                lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
                            }

                            gIsAudOutStartupInProgress = BTRMGR_STARTUP_AUD_COMPLETED;
                        }
                        else {
                            gIsAudOutStartupInProgress = BTRMGR_STARTUP_AUD_SKIPPED;
                        }
                    }
                }
            }
        }

        if (gIsAudOutStartupInProgress == BTRMGR_STARTUP_AUD_INPROGRESS)
            gIsAudOutStartupInProgress = BTRMGR_STARTUP_AUD_UNKNOWN;
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
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    if ((lenBtrMgrRet = btrMgr_StopCastingAudio()) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("btrMgr_StopCastingAudio = %d\n", lenBtrMgrRet);
    }

    if (btrMgr_IsDevConnected(ahBTRMgrDevHdl) == 1) {
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
    BTRMGR_StreamOut_Type_t aenCurrentSoType
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

    BTRMGRLOG_INFO ("Audio output - Stored %d - Switching to %d\n", gstBTRMgrStreamingInfo.tBTRMgrSoType, aenCurrentSoType);
    if (gstBTRMgrStreamingInfo.tBTRMgrSoType != aenCurrentSoType) {
        unsigned char ui8StreamingStatus = 0;
        BTRMGR_StreamOut_Type_t lenCurrentSoType = gstBTRMgrStreamingInfo.tBTRMgrSoType;

        gstBTRMgrStreamingInfo.tBTRMgrSoType = aenCurrentSoType;
        if ((BTRMGR_RESULT_SUCCESS == BTRMGR_IsAudioStreamingOut(aui8AdapterIdx, &ui8StreamingStatus)) && ui8StreamingStatus) {
            enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
            enBTRCoreDeviceType     lenBTRCoreDevTy = enBTRCoreUnknown;
            enBTRCoreDeviceClass    lenBTRCoreDevCl = enBTRCore_DC_Unknown;

            BTRMGRLOG_WARN ("Its already streaming. lets Switch\n");

            lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ghBTRMgrDevHdlCurStreaming, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
            BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBTRCoreDevTy, lenBTRCoreDevCl);

            if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
                /* Streaming-Out is happening; Lets switch it */
                if (btrMgr_SwitchCastingAudio_AC(aenCurrentSoType) != eBTRMgrSuccess) {
                    gstBTRMgrStreamingInfo.tBTRMgrSoType = lenCurrentSoType;
                    BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to switch to %d\n", aenCurrentSoType);
                    BTRMGRLOG_ERROR ("Failed to switch streaming on the current device. Streaming %d\n", gstBTRMgrStreamingInfo.tBTRMgrSoType);
                    if (btrMgr_SwitchCastingAudio_AC(gstBTRMgrStreamingInfo.tBTRMgrSoType) == eBTRMgrSuccess) {
                        BTRMGRLOG_WARN ("Streaming on the current device. Streaming %d\n", gstBTRMgrStreamingInfo.tBTRMgrSoType);
                    }

                    return BTRMGR_RESULT_GENERIC_FAILURE;
                }
            }
        }
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_StartAudioStreamingIn (
    unsigned char                   aui8AdapterIdx,
    BTRMgrDeviceHandle              ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t    connectAs
) {
    BTRMGR_Result_t             lenBtrMgrResult   = BTRMGR_RESULT_SUCCESS;
    BTRMGR_DeviceType_t         lenBtrMgrDevType  = BTRMGR_DEVICE_TYPE_UNKNOWN;
    eBTRMgrRet                  lenBtrMgrRet      = eBTRMgrSuccess;
    enBTRCoreRet                lenBtrCoreRet     = enBTRCoreSuccess;
    enBTRCoreDeviceType         lenBtrCoreDevType = enBTRCoreUnknown;
    stBTRCorePairedDevicesCount listOfPDevices;
    int                         i = 0;
    int                         i32IsFound = 0;
    int                         i32DeviceFD = 0;
    int                         i32DeviceReadMTU = 0;
    int                         i32DeviceWriteMTU = 0;
    unsigned int                ui32deviceDelay = 0xFFFFu;
    eBTRCoreDevMediaType        lenBtrCoreDevInMType = eBTRCoreDevMediaTypeUnknown;
    void*                       lpstBtrCoreDevInMCodecInfo = NULL;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!ahBTRMgrDevHdl)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (ghBTRMgrDevHdlCurStreaming == ahBTRMgrDevHdl) {

        if (gMediaPlaybackStPrev == BTRMGR_EVENT_MEDIA_TRACK_STOPPED) {
            BTRMGRLOG_DEBUG ("Starting Media Playback.\n");
            lenBtrMgrResult = BTRMGR_MediaControl (aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_MEDIA_CTRL_PLAY);
        }
        else if (gMediaPlaybackStPrev == BTRMGR_EVENT_MEDIA_TRACK_PAUSED) {
            BTRMGRLOG_DEBUG ("Resuming Media Playback.\n");
            lenBtrMgrResult = BTRMGR_MediaControl (aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_MEDIA_CTRL_PLAY);
        }
        else {
            BTRMGRLOG_WARN ("Its already streaming-in in this device.. Check the volume :)\n");
        }

        if (lenBtrMgrResult != BTRMGR_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR ("Failed to perform Media Control!\n");
        }

        return lenBtrMgrResult;
    }


    if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming != ahBTRMgrDevHdl)) {
        enBTRCoreDeviceType     lenBTRCoreDevTy = enBTRCoreUnknown;
        enBTRCoreDeviceClass    lenBTRCoreDevCl = enBTRCore_DC_Unknown;

        BTRMGRLOG_WARN ("Its already streaming in. lets stop this and start on other device \n");

        lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ghBTRMgrDevHdlCurStreaming, &lenBTRCoreDevTy, &lenBTRCoreDevCl);
        BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBTRCoreDevTy, lenBTRCoreDevCl);

        if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
            /* Streaming-Out is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingOut(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to stop Playback.-Out\n");
                BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
                return lenBtrMgrResult;
            }
        }
        else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
            /* Streaming-In is happening; stop it */
            if ((lenBtrMgrResult = BTRMGR_StopAudioStreamingIn(aui8AdapterIdx, ghBTRMgrDevHdlCurStreaming)) != BTRMGR_RESULT_SUCCESS) {
                BTRMGRLOG_ERROR ("This device is being Connected n Playing. Failed to stop Playback.-In\n");
                BTRMGRLOG_ERROR ("Failed to stop streaming at the current device..\n");
                return lenBtrMgrResult;
            }
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
    if (!gIsAudioInEnabled && ((lenBtrCoreDevType == enBTRCoreMobileAudioIn) || (lenBtrCoreDevType == enBTRCorePCAudioIn))) {
        BTRMGRLOG_WARN ("StreamingIn Rejected - BTR AudioIn is currently Disabled!\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }


    if (!listOfPDevices.devices[i].bDeviceConnected || (ghBTRMgrDevHdlCurStreaming != listOfPDevices.devices[i].tDeviceId)) {
        if ((lenBtrMgrRet = btrMgr_ConnectToDevice(aui8AdapterIdx, listOfPDevices.devices[i].tDeviceId, connectAs, 0, 1)) == eBTRMgrSuccess) {
            gMediaPlaybackStPrev = BTRMGR_EVENT_MEDIA_TRACK_STOPPED;    //TODO: Bad Bad Bad way of doing this
        }
        else {
            BTRMGRLOG_ERROR ("Failure\n");
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }


    if (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
        free (gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);
        gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = NULL;
    }


    gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) > sizeof(stBTRCoreDevMediaMpegInfo) ?
                                                                   (sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) : sizeof(stBTRCoreDevMediaMpegInfo));

    lenBtrCoreRet = BTRCore_GetDeviceMediaInfo(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, lenBtrCoreDevType, &gstBtrCoreDevMediaInfo);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        lenBtrCoreDevInMType      = gstBtrCoreDevMediaInfo.eBtrCoreDevMType;
        lpstBtrCoreDevInMCodecInfo= gstBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo;

        if (lenBtrCoreDevInMType == eBTRCoreDevMediaTypeSBC) {
            stBTRCoreDevMediaSbcInfo*   lpstBtrCoreDevMSbcInfo = (stBTRCoreDevMediaSbcInfo*)lpstBtrCoreDevInMCodecInfo;

            BTRMGRLOG_INFO ("DevMedInfo SFreq           = %d\n", lpstBtrCoreDevMSbcInfo->ui32DevMSFreq);
            BTRMGRLOG_INFO ("DevMedInfo AChan           = %d\n", lpstBtrCoreDevMSbcInfo->eDevMAChan);
            BTRMGRLOG_INFO ("DevMedInfo SbcAllocMethod  = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcAllocMethod);
            BTRMGRLOG_INFO ("DevMedInfo SbcSubbands     = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcSubbands);
            BTRMGRLOG_INFO ("DevMedInfo SbcBlockLength  = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcBlockLength);
            BTRMGRLOG_INFO ("DevMedInfo SbcMinBitpool   = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcMinBitpool);
            BTRMGRLOG_INFO ("DevMedInfo SbcMaxBitpool   = %d\n", lpstBtrCoreDevMSbcInfo->ui8DevMSbcMaxBitpool);
            BTRMGRLOG_INFO ("DevMedInfo SbcFrameLen     = %d\n", lpstBtrCoreDevMSbcInfo->ui16DevMSbcFrameLen);
            BTRMGRLOG_DEBUG("DevMedInfo SbcBitrate      = %d\n", lpstBtrCoreDevMSbcInfo->ui16DevMSbcBitrate);
        }
        else if (lenBtrCoreDevInMType == eBTRCoreDevMediaTypeAAC) {
            stBTRCoreDevMediaMpegInfo*   lpstBtrCoreDevMAacInfo = (stBTRCoreDevMediaMpegInfo*)lpstBtrCoreDevInMCodecInfo;

            BTRMGRLOG_INFO ("DevMedInfo SFreq           = %d\n", lpstBtrCoreDevMAacInfo->ui32DevMSFreq);
            BTRMGRLOG_INFO ("DevMedInfo AChan           = %d\n", lpstBtrCoreDevMAacInfo->eDevMAChan);
            BTRMGRLOG_INFO ("DevMedInfo AacMpegCrc      = %d\n", lpstBtrCoreDevMAacInfo->ui8DevMMpegCrc);
            BTRMGRLOG_INFO ("DevMedInfo AacMpegLayer    = %d\n", lpstBtrCoreDevMAacInfo->ui8DevMMpegLayer);
            BTRMGRLOG_INFO ("DevMedInfo AacMpegMpf      = %d\n", lpstBtrCoreDevMAacInfo->ui8DevMMpegMpf);
            BTRMGRLOG_INFO ("DevMedInfo AacMpegRfa      = %d\n", lpstBtrCoreDevMAacInfo->ui8DevMMpegRfa);
            BTRMGRLOG_INFO ("DevMedInfo AacMpegFrmLen   = %d\n", lpstBtrCoreDevMAacInfo->ui16DevMMpegFrameLen);
            BTRMGRLOG_INFO ("DevMedInfo AacMpegBitrate  = %d\n", lpstBtrCoreDevMAacInfo->ui16DevMMpegBitrate);
        }
    }

    /* Aquire Device Data Path to start audio reception */
    lenBtrCoreRet = BTRCore_AcquireDeviceDataPath (ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, lenBtrCoreDevType, &i32DeviceFD, &i32DeviceReadMTU, &i32DeviceWriteMTU, &ui32deviceDelay);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if ((lenBtrMgrRet = btrMgr_StartReceivingAudio(i32DeviceFD, i32DeviceReadMTU, ui32deviceDelay, lenBtrCoreDevInMType, lpstBtrCoreDevInMCodecInfo)) == eBTRMgrSuccess) {
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


    if ((ghBTRMgrDevHdlCurStreaming != ahBTRMgrDevHdl) && (ghBTRMgrDevHdlLastConnected != ahBTRMgrDevHdl)) {
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

    /* We had Reset the ghBTRMgrDevHdlCurStreaming to avoid recursion/looping; so no worries */
    lenBtrMgrResult = BTRMGR_DisconnectFromDevice(aui8AdapterIdx, ahBTRMgrDevHdl);

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
        gAcceptConnection  = 0;
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
    case BTRMGR_EVENT_DEVICE_OP_INFORMATION:
        if (apstBTRMgrEvtRsp->m_eventResp) {
            strncpy(gLeReadOpResponse, apstBTRMgrEvtRsp->m_writeData, BTRMGR_MAX_DEV_OP_DATA_LEN - 1);
            gEventRespReceived = 1;
        }
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
    BTRMGR_Result_t             lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType         lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass        lenBtrCoreDevCl     = enBTRCore_DC_Unknown;
    BTRMGR_MediaDeviceStatus_t  lstMediaDeviceStatus;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);


    lstMediaDeviceStatus.m_ui8mediaDevVolume= BTRMGR_SO_MAX_VOLUME;
    lstMediaDeviceStatus.m_enmediaCtrlCmd   = mediaCtrlCmd;
    if (mediaCtrlCmd == BTRMGR_MEDIA_CTRL_MUTE)
        lstMediaDeviceStatus.m_ui8mediaDevMute  =  1;
    else if (mediaCtrlCmd == BTRMGR_MEDIA_CTRL_UNMUTE)
        lstMediaDeviceStatus.m_ui8mediaDevMute  =  0;
    else
        lstMediaDeviceStatus.m_ui8mediaDevMute  =  BTRMGR_SO_MAX_VOLUME;     // To handle alternate options


    if (btrMgr_MediaControl(aui8AdapterIdx, ahBTRMgrDevHdl, &lstMediaDeviceStatus, lenBtrCoreDevTy, lenBtrCoreDevCl, NULL) != eBTRMgrSuccess)
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;

    return lenBtrMgrResult;
}

eBTRMgrRet
btrMgr_MediaControl (
    unsigned char               aui8AdapterIdx,
    BTRMgrDeviceHandle          ahBTRMgrDevHdl,
    BTRMGR_MediaDeviceStatus_t* apstMediaDeviceStatus,
    enBTRCoreDeviceType         aenBtrCoreDevTy,
    enBTRCoreDeviceClass        aenBtrCoreDevCl,
    stBTRCoreMediaCtData*       apstBtrCoreMediaCData
) {
    enBTRCoreMediaCtrl  lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlUnknown;
    eBTRMgrRet          lenBtrMgrRet        = eBTRMgrFailure;

    switch (apstMediaDeviceStatus->m_enmediaCtrlCmd) {
    case BTRMGR_MEDIA_CTRL_PLAY:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlPlay;
        break;
    case BTRMGR_MEDIA_CTRL_PAUSE:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlPause;
        break;
    case BTRMGR_MEDIA_CTRL_STOP:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlStop;
        break;
    case BTRMGR_MEDIA_CTRL_NEXT:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlNext;
        break;
    case BTRMGR_MEDIA_CTRL_PREVIOUS:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlPrevious;
        break;
    case BTRMGR_MEDIA_CTRL_FASTFORWARD:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlFastForward;
        break;
    case BTRMGR_MEDIA_CTRL_REWIND:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRewind;
        break;
    case BTRMGR_MEDIA_CTRL_VOLUMEUP:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlVolumeUp;
        break;
    case BTRMGR_MEDIA_CTRL_VOLUMEDOWN:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlVolumeDown;
        break;
    case BTRMGR_MEDIA_CTRL_EQUALIZER_OFF:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlEqlzrOff;
        break;
    case BTRMGR_MEDIA_CTRL_EQUALIZER_ON:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlEqlzrOn;
        break;
    case BTRMGR_MEDIA_CTRL_SHUFFLE_OFF:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlShflOff;
        break;
    case BTRMGR_MEDIA_CTRL_SHUFFLE_ALLTRACKS:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlShflAllTracks;
        break;
    case BTRMGR_MEDIA_CTRL_SHUFFLE_GROUP:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlShflGroup;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_OFF:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptOff;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_SINGLETRACK:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptSingleTrack;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_ALLTRACKS:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptAllTracks;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_GROUP:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptGroup;
        break;
    case BTRMGR_MEDIA_CTRL_MUTE:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlMute;
        break;
    case BTRMGR_MEDIA_CTRL_UNMUTE:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlUnMute;
        break;
    case BTRMGR_MEDIA_CTRL_UNKNOWN:
    default:
        lenBTRCoreMediaCtrl = enBTRCoreMediaCtrlUnknown;
    }


    if (lenBTRCoreMediaCtrl != enBTRCoreMediaCtrlUnknown) {

        if ((aenBtrCoreDevTy == enBTRCoreSpeakers) || (aenBtrCoreDevTy == enBTRCoreHeadSet)) {
            if ((ghBTRMgrDevHdlCurStreaming == ahBTRMgrDevHdl) ) {
                BTRMGR_EventMessage_t lstEventMessage;

                memset (&lstEventMessage, 0, sizeof(lstEventMessage));

                switch (apstMediaDeviceStatus->m_enmediaCtrlCmd) {
                case BTRMGR_MEDIA_CTRL_VOLUMEUP:
                    if (enBTRCoreSuccess == BTRCore_MediaControl(ghBTRCoreHdl, ahBTRMgrDevHdl, aenBtrCoreDevTy, lenBTRCoreMediaCtrl, apstBtrCoreMediaCData)) {
                        BTRMGRLOG_INFO ("Media Control Command BTRMGR_MEDIA_CTRL_VOLUMEUP for %llu Success for streamout!!!\n", ahBTRMgrDevHdl);
                        lenBtrMgrRet = eBTRMgrSuccess;
                    }
                    else if (apstMediaDeviceStatus->m_ui8mediaDevMute == BTRMGR_SO_MAX_VOLUME) {
                        unsigned char         ui8CurVolume = 0;

                        BTRMgr_SO_GetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl,&ui8CurVolume);
                        BTRMGRLOG_DEBUG ("ui8CurVolume %d \n ",ui8CurVolume);

                        if (apstBtrCoreMediaCData != NULL) {
                            ui8CurVolume = apstBtrCoreMediaCData->m_mediaAbsoluteVolume;
                        }
                        else {
                            if(ui8CurVolume < 5)
                                ui8CurVolume = 5;
                            else if (ui8CurVolume <= 245 && ui8CurVolume >= 5)
                                ui8CurVolume = ui8CurVolume + 10; // Increment steps in 10
                            else
                                ui8CurVolume = BTRMGR_SO_MAX_VOLUME;
                        }

                        if ((lenBtrMgrRet = BTRMgr_SO_SetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, ui8CurVolume)) == eBTRMgrSuccess) {
                            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_MEDIA_STATUS;
                            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevVolume  = ui8CurVolume;
                            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevMute    = (ui8CurVolume) ? FALSE : TRUE;
                            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd     = BTRMGR_MEDIA_CTRL_VOLUMEUP;
                            BTRMGRLOG_DEBUG (" Volume Up %d \n", ui8CurVolume);
#ifdef RDKTV_PERSIST_VOLUME_SKY
                            btrMgr_SetLastVolume(0, ui8CurVolume);
#endif
                        }
                    }
                break;

                case BTRMGR_MEDIA_CTRL_VOLUMEDOWN:
                    if (enBTRCoreSuccess == BTRCore_MediaControl(ghBTRCoreHdl, ahBTRMgrDevHdl, aenBtrCoreDevTy, lenBTRCoreMediaCtrl, apstBtrCoreMediaCData)) {
                        BTRMGRLOG_INFO ("Media Control Command BTRMGR_MEDIA_CTRL_VOLUMEDOWN for %llu Success for streamout!!!\n", ahBTRMgrDevHdl);
                        lenBtrMgrRet = eBTRMgrSuccess;
                    }
                    else if (apstMediaDeviceStatus->m_ui8mediaDevMute == BTRMGR_SO_MAX_VOLUME) {
                        unsigned char         ui8CurVolume = 0;

                        BTRMgr_SO_GetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl,&ui8CurVolume);
                        BTRMGRLOG_DEBUG ("ui8CurVolume %d \n ",ui8CurVolume);

                        if (apstBtrCoreMediaCData != NULL) {
                            ui8CurVolume = apstBtrCoreMediaCData->m_mediaAbsoluteVolume;
                        }
                        else {
                            if (ui8CurVolume > 250)
                                ui8CurVolume = 250;
                            else if (ui8CurVolume <= 250 && ui8CurVolume >= 10)
                                ui8CurVolume = ui8CurVolume - 10;   // Decrement steps in 10
                            else
                                ui8CurVolume = 0;
                        }

                        lenBtrMgrRet = BTRMgr_SO_SetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, ui8CurVolume);

                        if (lenBtrMgrRet == eBTRMgrSuccess) {
                            lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_MEDIA_STATUS;
                            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevVolume= ui8CurVolume;
                            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevMute  = (ui8CurVolume) ? FALSE : TRUE;
                            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd   = BTRMGR_MEDIA_CTRL_VOLUMEDOWN;
                            BTRMGRLOG_DEBUG (" Volume Down %d \n", ui8CurVolume);
#ifdef RDKTV_PERSIST_VOLUME_SKY
                            btrMgr_SetLastVolume(0, ui8CurVolume);
#endif
                        }
                    }
                break;

                case BTRMGR_MEDIA_CTRL_MUTE:
                    if ((lenBtrMgrRet = BTRMgr_SO_SetMute(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, TRUE)) == eBTRMgrSuccess) {
                        unsigned char         ui8CurVolume = 0;
                        BTRMgr_SO_GetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl,&ui8CurVolume);

                        lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_MEDIA_STATUS;
                        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevVolume= ui8CurVolume;
                        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevMute  = TRUE;
                        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd   = BTRMGR_MEDIA_CTRL_MUTE;
                        BTRMGRLOG_DEBUG (" Mute set success \n");
#ifdef RDKTV_PERSIST_VOLUME_SKY
                        btrMgr_SetLastMuteState(0, TRUE);
#endif
                    }
                break;

                case BTRMGR_MEDIA_CTRL_UNMUTE:
                    if ((lenBtrMgrRet = BTRMgr_SO_SetMute(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, FALSE)) == eBTRMgrSuccess) {
                        unsigned char         ui8CurVolume = 0;
                        BTRMgr_SO_GetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl,&ui8CurVolume);

                        lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_MEDIA_STATUS;
                        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevVolume= ui8CurVolume;
                        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevMute  = FALSE;
                        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd   = BTRMGR_MEDIA_CTRL_UNMUTE;
                        BTRMGRLOG_DEBUG (" UnMute set success \n");
#ifdef RDKTV_PERSIST_VOLUME_SKY
                        btrMgr_SetLastMuteState(0, FALSE);
#endif
                    }
                break;

                default:
                    if (enBTRCoreSuccess == BTRCore_MediaControl(ghBTRCoreHdl, ahBTRMgrDevHdl, aenBtrCoreDevTy, lenBTRCoreMediaCtrl, apstBtrCoreMediaCData)) {
                        lenBtrMgrRet = eBTRMgrSuccess;
                    }
                    else  {
                        BTRMGRLOG_ERROR ("Media Control Command for %llu Failed for streamout!!!\n", ahBTRMgrDevHdl);
                        lenBtrMgrRet = eBTRMgrFailure;
                    }
                }

                if ((lenBtrMgrRet == eBTRMgrSuccess) && (gfpcBBTRMgrEventOut)) {
                    lstEventMessage.m_mediaInfo.m_deviceHandle  = ahBTRMgrDevHdl;
                    lstEventMessage.m_mediaInfo.m_deviceType    = btrMgr_MapDeviceTypeFromCore(aenBtrCoreDevCl);
                    for (int j = 0; j <= gListOfPairedDevices.m_numOfDevices; j++) {
                        if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
                            strncpy(lstEventMessage.m_mediaInfo.m_name, gListOfPairedDevices.m_deviceProperty[j].m_name, BTRMGR_NAME_LEN_MAX -1);
                            break;
                        }
                    }

                    gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
                }

            } else {
                BTRMGRLOG_ERROR ("pstBtrMgrSoHdl Null  or streaming out not started \n");
                lenBtrMgrRet = eBTRMgrFailure;
            }
        }
        else {
            if (enBTRCoreSuccess != BTRCore_MediaControl(ghBTRCoreHdl, ahBTRMgrDevHdl, aenBtrCoreDevTy, lenBTRCoreMediaCtrl, apstBtrCoreMediaCData)) {
                BTRMGRLOG_ERROR ("Media Control Command for %llu Failed!!!\n", ahBTRMgrDevHdl);
                lenBtrMgrRet = eBTRMgrFailure;
            }
        }
    }
    else {
        BTRMGRLOG_ERROR ("Media Control Command for %llu Unknown!!!\n", ahBTRMgrDevHdl);
        lenBtrMgrRet = eBTRMgrFailure;
    }

    return lenBtrMgrRet;
}


BTRMGR_Result_t
BTRMGR_GetDeviceVolumeMute (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t deviceOpType,
    unsigned char*               pui8Volume,
    unsigned char*               pui8Mute
) {
    BTRMGR_Result_t       lenBtrMgrResult       = BTRMGR_RESULT_SUCCESS;
    eBTRMgrRet            lenBtrMgrRet          = eBTRMgrSuccess;
    eBTRMgrRet            lenBtrMgrPersistRet   = eBTRMgrFailure;
    enBTRCoreRet          lenBtrCoreRet         = enBTRCoreSuccess;
    enBTRCoreDeviceType   lenBtrCoreDevTy       = enBTRCoreUnknown;
    enBTRCoreDeviceClass  lenBtrCoreDevCl       = enBTRCore_DC_Unknown;
    unsigned char         lui8CurVolume         = 0;
    gboolean              lbCurMute             = FALSE;

#ifdef RDKTV_PERSIST_VOLUME_SKY
    unsigned char         lui8PersistedVolume = 0;
    gboolean              lbPersistedMute = FALSE;
#endif

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl) || (gstBTRMgrStreamingInfo.hBTRMgrSoHdl == NULL)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected/streaming\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    // Currently implemented for audio out only
    if (deviceOpType != BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not audio out\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);

#ifdef RDKTV_PERSIST_VOLUME_SKY
    if ((lenBtrMgrPersistRet = btrMgr_GetLastVolume(0, &lui8PersistedVolume)) == eBTRMgrSuccess) {
        lui8CurVolume = lui8PersistedVolume;
    }
    else {
       BTRMGRLOG_WARN ("Device Handle(%lld) Persist audio out volume get fail\n", ahBTRMgrDevHdl);
    }
#endif

    if ((lenBtrMgrPersistRet == eBTRMgrFailure) &&
        ((lenBtrMgrRet = BTRMgr_SO_GetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl,&lui8CurVolume)) != eBTRMgrSuccess)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) audio out volume get fail\n", ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

#ifdef RDKTV_PERSIST_VOLUME_SKY
    if ((lenBtrMgrPersistRet = btrMgr_GetLastMuteState(0, &lbPersistedMute)) == eBTRMgrSuccess) {
        lbCurMute = lbPersistedMute;
    }
    else {
       BTRMGRLOG_ERROR ("Device Handle(%lld) Persist audio out mute get fail\n", ahBTRMgrDevHdl);
    }
#endif

    if ((lenBtrMgrPersistRet == eBTRMgrFailure) &&
        ((lenBtrMgrRet = BTRMgr_SO_GetMute(gstBTRMgrStreamingInfo.hBTRMgrSoHdl,&lbCurMute)) != eBTRMgrSuccess)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) audio out mute get fail\n", ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (lenBtrMgrRet == eBTRMgrSuccess)  {
        *pui8Volume = lui8CurVolume;
        *pui8Mute   = (unsigned char)lbCurMute;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_SetDeviceVolumeMute (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t deviceOpType,
    unsigned char                aui8Volume,
    unsigned char                aui8Mute
) {
    BTRMGR_Result_t             lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    eBTRMgrRet                  lenBtrMgrRet        = eBTRMgrSuccess;
    enBTRCoreRet                lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType         lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass        lenBtrCoreDevCl     = enBTRCore_DC_Unknown;
    unsigned char               lui8Volume          = BTRMGR_SO_MAX_VOLUME - 1;
    gboolean                    lbMuted             = FALSE;
    gboolean                    abMuted             = FALSE;
    BTRMGR_MediaDeviceStatus_t  lstMediaDeviceStatus;
    stBTRCoreMediaCtData        lstBTRCoreMediaCData;
    BTRMGR_EventMessage_t       lstEventMessage;


    memset (&lstEventMessage, 0, sizeof(lstEventMessage));
    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt() || aui8Mute > 1) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl) || (gstBTRMgrStreamingInfo.hBTRMgrSoHdl == NULL)) {
        BTRMGRLOG_ERROR ("Device Handle(%lld) not connected/streaming\n", ahBTRMgrDevHdl);
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    // Currently implemented for audio out only
    if (deviceOpType != BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) {
        BTRMGRLOG_ERROR ("Device Handle(%lld) not audio out\n", ahBTRMgrDevHdl);
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);

#ifdef RDKTV_PERSIST_VOLUME_SKY
    btrMgr_GetLastVolume(0, &lui8Volume);
    btrMgr_GetLastMuteState(0, &lbMuted);
#endif

    lstMediaDeviceStatus.m_enmediaCtrlCmd   = BTRMGR_MEDIA_CTRL_UNKNOWN;
    lstMediaDeviceStatus.m_ui8mediaDevVolume= aui8Volume;
    lstMediaDeviceStatus.m_ui8mediaDevMute  = aui8Mute;

    lstBTRCoreMediaCData.m_mediaAbsoluteVolume  = aui8Volume;

    if (aui8Volume > lui8Volume)
        lstMediaDeviceStatus.m_enmediaCtrlCmd   = BTRMGR_MEDIA_CTRL_VOLUMEUP;
    else if (aui8Volume < lui8Volume)
        lstMediaDeviceStatus.m_enmediaCtrlCmd   = BTRMGR_MEDIA_CTRL_VOLUMEDOWN;
    

    if ((lenBtrMgrRet = btrMgr_MediaControl(aui8AdapterIdx, ahBTRMgrDevHdl, &lstMediaDeviceStatus, lenBtrCoreDevTy, lenBtrCoreDevCl, &lstBTRCoreMediaCData)) == eBTRMgrSuccess) {
        BTRMGRLOG_INFO ("Device Handle(%lld) AVRCP audio out volume Set Success\n", ahBTRMgrDevHdl);
        if (BTRMgr_SO_SetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, BTRMGR_SO_MAX_VOLUME) != eBTRMgrSuccess) {
            BTRMGRLOG_ERROR ("Device Handle(%lld) AVRCP audio-out SO volume Set fail\n", ahBTRMgrDevHdl);
        }
    }
    else if ((aui8Volume != lui8Volume) && ((lenBtrMgrRet = BTRMgr_SO_SetVolume(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, aui8Volume)) != eBTRMgrSuccess)) {
        BTRMGRLOG_ERROR ("Device Handle(%lld) audio out volume Set fail\n", ahBTRMgrDevHdl);
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

#ifdef RDKTV_PERSIST_VOLUME_SKY
    if (lenBtrMgrRet == eBTRMgrSuccess)
        btrMgr_SetLastVolume(0, aui8Volume);
#endif

    abMuted = aui8Mute ? TRUE : FALSE;
    if ((lenBtrMgrRet = BTRMgr_SO_SetMute(gstBTRMgrStreamingInfo.hBTRMgrSoHdl, abMuted)) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Device Handle(%lld) not audio out mute set fail\n", ahBTRMgrDevHdl);
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
#ifdef RDKTV_PERSIST_VOLUME_SKY
        btrMgr_SetLastMuteState(0, abMuted);
#endif
    }

    if ((lenBtrMgrResult == BTRMGR_RESULT_SUCCESS) && gfpcBBTRMgrEventOut) {
        lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_MEDIA_STATUS;
        lstEventMessage.m_mediaInfo.m_deviceType = btrMgr_MapDeviceTypeFromCore(lenBtrCoreDevCl);
        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevVolume = aui8Volume;
        lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevMute   = aui8Mute;

        if ((abMuted != lbMuted) && abMuted)
            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_MUTE;
        else if ((abMuted != lbMuted) && !abMuted)
            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_UNMUTE;
        else if (aui8Volume > lui8Volume)
            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_VOLUMEUP;
        else if (aui8Volume < lui8Volume)
            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_VOLUMEDOWN;
        else
            lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_UNKNOWN;

        for (int j = 0; j <= gListOfPairedDevices.m_numOfDevices; j++) {
            if (ahBTRMgrDevHdl == gListOfPairedDevices.m_deviceProperty[j].m_deviceHandle) {
                lstEventMessage.m_mediaInfo.m_deviceHandle = ahBTRMgrDevHdl;
                strncpy(lstEventMessage.m_mediaInfo.m_name, gListOfPairedDevices.m_deviceProperty[j].m_name, BTRMGR_NAME_LEN_MAX -1);
                break;
            }
        }

        gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetMediaTrackInfo (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMGR_MediaTrackInfo_t      *mediaTrackInfo
) {
    BTRMGR_Result_t              lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                 lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType          lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass         lenBtrCoreDevCl     = enBTRCore_DC_Unknown;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);

    if (enBTRCoreSuccess != BTRCore_GetMediaTrackInfo(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBtrCoreDevTy, (stBTRCoreMediaTrackInfo*)mediaTrackInfo)) {
       BTRMGRLOG_ERROR ("Get Media Track Information for %llu Failed!!!\n", ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_GetMediaElementTrackInfo (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMgrMediaElementHandle     ahBTRMgrMedElementHdl,
    BTRMGR_MediaTrackInfo_t      *mediaTrackInfo
) {
    BTRMGR_Result_t              lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                 lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType          lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass         lenBtrCoreDevCl     = enBTRCore_DC_Unknown;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);

    if (enBTRCoreSuccess != BTRCore_GetMediaElementTrackInfo(ghBTRCoreHdl, ahBTRMgrDevHdl,
                            lenBtrCoreDevTy, ahBTRMgrMedElementHdl, (stBTRCoreMediaTrackInfo*)mediaTrackInfo)) {
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
    BTRMGR_Result_t             lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType         lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass        lenBtrCoreDevCl     = enBTRCore_DC_Unknown;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);

    if (enBTRCoreSuccess != BTRCore_GetMediaPositionInfo(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBtrCoreDevTy, (stBTRCoreMediaPositionInfo*)mediaPositionInfo)) {
       BTRMGRLOG_ERROR ("Get Media Current Position for %llu Failed!!!\n", ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_SetMediaElementActive (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMgrMediaElementHandle     ahBTRMgrMedElementHdl,
    BTRMGR_MediaElementType_t    aMediaElementType
) {
    BTRMGR_Result_t              lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                 lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType          lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass         lenBtrCoreDevCl     = enBTRCore_DC_Unknown;
    eBTRCoreMedElementType       lenMediaElementType = enBTRCoreMedETypeUnknown;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);

    switch (aMediaElementType) {
    case BTRMGR_MEDIA_ELEMENT_TYPE_ALBUM:
        lenMediaElementType    = enBTRCoreMedETypeAlbum;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_ARTIST:
        lenMediaElementType    = enBTRCoreMedETypeArtist;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_GENRE:
        lenMediaElementType    = enBTRCoreMedETypeGenre;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_COMPILATIONS:
        lenMediaElementType    = enBTRCoreMedETypeCompilation;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_PLAYLIST:
        lenMediaElementType    = enBTRCoreMedETypePlayList;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_TRACKLIST:
        lenMediaElementType    = enBTRCoreMedETypeTrackList;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_TRACK:
        lenMediaElementType    = enBTRCoreMedETypeTrack;
        break;
    default:
        break;
    }

    /* Add intelligence to prefetch MediaElementInfo based on users' interest/navigation */

    if (enBTRCoreSuccess != BTRCore_SetMediaElementActive (ghBTRCoreHdl,
                                                           ahBTRMgrDevHdl,
                                                           ahBTRMgrMedElementHdl,
                                                           lenBtrCoreDevTy,
                                                           lenMediaElementType)) {
       BTRMGRLOG_ERROR ("Set Active Media Element(%llu) List for Dev %llu Failed!!!\n", ahBTRMgrMedElementHdl, ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_GetMediaElementList (
    unsigned char                   aui8AdapterIdx,
    BTRMgrDeviceHandle              ahBTRMgrDevHdl,
    BTRMgrMediaElementHandle        ahBTRMgrMedElementHdl,
    unsigned short                  aui16MediaElementStartIdx,
    unsigned short                  aui16MediaElementEndIdx,
    unsigned char                   abMediaElementListDepth,
    BTRMGR_MediaElementType_t       aMediaElementType,
    BTRMGR_MediaElementListInfo_t*  aMediaElementListInfo
) {
    BTRMGR_Result_t               lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                  lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType           lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass          lenBtrCoreDevCl     = enBTRCore_DC_Unknown;
    stBTRCoreMediaElementInfoList lpstBTRCoreMediaElementInfoList;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt() || !aMediaElementListInfo ||
        aui16MediaElementStartIdx > aui16MediaElementEndIdx) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (aui16MediaElementEndIdx - aui16MediaElementStartIdx > BTRMGR_MEDIA_ELEMENT_COUNT_MAX -1) {
        aui16MediaElementEndIdx = aui16MediaElementStartIdx + BTRMGR_MEDIA_ELEMENT_COUNT_MAX -1;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);

    memset (&lpstBTRCoreMediaElementInfoList, 0, sizeof(stBTRCoreMediaElementInfoList));

    /* TODO Retrive required List from the populated DataBase */
    /* In below passing argument aMediaElementType is not transitioning now due to
     * BTRMGR_MediaElementType_t and eBTRCoreMedElementType both enums are same.
     * In future if change the any enums it will impact */
    if (enBTRCoreSuccess != BTRCore_GetMediaElementList (ghBTRCoreHdl,
                                                         ahBTRMgrDevHdl,
                                                         ahBTRMgrMedElementHdl,
                                                         aui16MediaElementStartIdx,
                                                         aui16MediaElementEndIdx,
                                                         lenBtrCoreDevTy,
                                                         aMediaElementType,
                                                         &lpstBTRCoreMediaElementInfoList)) {
        BTRMGRLOG_ERROR ("Get Media Element(%llu) List for Dev %llu Failed!!!\n", ahBTRMgrMedElementHdl, ahBTRMgrDevHdl);
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        unsigned short               ui16LoopIdx = 0;
        stBTRCoreMediaElementInfo*   srcElement  = 0;
        BTRMGR_MediaElementInfo_t*   desElement  = 0;

        if (lpstBTRCoreMediaElementInfoList.m_numOfElements > BTRMGR_MEDIA_ELEMENT_COUNT_MAX) {
            lpstBTRCoreMediaElementInfoList.m_numOfElements = BTRMGR_MEDIA_ELEMENT_COUNT_MAX;
        }

        while (ui16LoopIdx < lpstBTRCoreMediaElementInfoList.m_numOfElements) {
            srcElement = &lpstBTRCoreMediaElementInfoList.m_mediaElementInfo[ui16LoopIdx];
            desElement = &aMediaElementListInfo->m_mediaElementInfo[ui16LoopIdx++];

            desElement->m_mediaElementHdl  = srcElement->ui32MediaElementId;
            desElement->m_IsPlayable       = srcElement->bIsPlayable;
            strncpy (desElement->m_mediaElementName, srcElement->m_mediaElementName, BTRMGR_MAX_STR_LEN -1);
            memcpy  (&desElement->m_mediaTrackInfo,  &srcElement->m_mediaTrackInfo,  sizeof(BTRMGR_MediaTrackInfo_t));
        }
        aMediaElementListInfo->m_numberOfElements   = lpstBTRCoreMediaElementInfoList.m_numOfElements;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_SelectMediaElement (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMgrMediaElementHandle     ahBTRMgrMedElementHdl,
    BTRMGR_MediaElementType_t    aMediaElementType
) {
    BTRMGR_Result_t              lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                 lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType          lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass         lenBtrCoreDevCl     = enBTRCore_DC_Unknown;
    eBTRCoreMedElementType       lenMediaElementType = enBTRCoreMedETypeUnknown;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (!btrMgr_IsDevConnected(ahBTRMgrDevHdl)) {
       BTRMGRLOG_ERROR ("Device Handle(%lld) not connected\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrCoreRet = BTRCore_GetDeviceTypeClass(ghBTRCoreHdl, ahBTRMgrDevHdl, &lenBtrCoreDevTy, &lenBtrCoreDevCl);
    BTRMGRLOG_DEBUG ("Status = %d\t Device Type = %d\t Device Class = %x\n", lenBtrCoreRet, lenBtrCoreDevTy, lenBtrCoreDevCl);
   

    switch (aMediaElementType) {
    case BTRMGR_MEDIA_ELEMENT_TYPE_ALBUM:
        lenMediaElementType    = enBTRCoreMedETypeAlbum;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_ARTIST:
        lenMediaElementType    = enBTRCoreMedETypeArtist;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_GENRE:
        lenMediaElementType    = enBTRCoreMedETypeGenre;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_COMPILATIONS:
        lenMediaElementType    = enBTRCoreMedETypeCompilation;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_PLAYLIST:
        lenMediaElementType    = enBTRCoreMedETypePlayList;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_TRACKLIST:
        lenMediaElementType    = enBTRCoreMedETypeTrackList;
        break;
    case BTRMGR_MEDIA_ELEMENT_TYPE_TRACK:
        lenMediaElementType    = enBTRCoreMedETypeTrack;
        break;
    default:
        break;
    }

    if (enBTRCoreSuccess != BTRCore_SelectMediaElement (ghBTRCoreHdl,
                                                        ahBTRMgrDevHdl,
                                                        ahBTRMgrMedElementHdl,
                                                        lenBtrCoreDevTy,
                                                        lenMediaElementType)) {
       BTRMGRLOG_ERROR ("Select Media Element(%llu) on Dev %llu Failed!!!\n", ahBTRMgrMedElementHdl, ahBTRMgrDevHdl);
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
    else if (type == BTRMGR_DEVICE_TYPE_TILE)
        return "LE TILE";
    else if ((type == BTRMGR_DEVICE_TYPE_HID) || (type == BTRMGR_DEVICE_TYPE_HID_GAMEPAD))
        return "HUMAN INTERFACE DEVICE";
    else
        return "UNKNOWN DEVICE";
}


BTRMGR_Result_t
BTRMGR_SetAudioInServiceState (
    unsigned char   aui8AdapterIdx,
    unsigned char   aui8State
) {
    if ((gIsAudioInEnabled = aui8State)) {
        BTRMGRLOG_INFO ("AudioIn Service is Enabled.\n");
    }
    else {
        BTRMGRLOG_INFO ("AudioIn Service is Disabled.\n");
    }
    return BTRMGR_RESULT_SUCCESS;
}

BTRMGR_Result_t
BTRMGR_SetHidGamePadServiceState (
    unsigned char   aui8AdapterIdx,
    unsigned char   aui8State
) {
    if ((gIsHidGamePadEnabled = aui8State)) {
        BTRMGRLOG_INFO ("HID GamePad Service is Enabled.\n");
    }
    else {
        BTRMGRLOG_INFO ("HID GamePad Service is Disabled.\n");
    }
    return BTRMGR_RESULT_SUCCESS;
}

BTRMGR_Result_t
BTRMGR_GetLimitBeaconDetection (
    unsigned char   aui8AdapterIdx,
    unsigned char   *pLimited
) {

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    BTRMGR_Beacon_PersistentData_t BeaconPersistentData;
    if (BTRMgr_PI_GetLEBeaconLimitingStatus(&BeaconPersistentData) == eBTRMgrFailure) {
        BTRMGRLOG_INFO ("Failed to get limit for beacon detection from json.\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (pLimited != NULL) {
        *pLimited =  (unsigned char)((!strncasecmp(BeaconPersistentData.limitBeaconDetection, "true", 4)) ? 1 : 0 );
        BTRMGRLOG_INFO ("the beacon detection detection : %s\n", *pLimited? "true":"false");
    }  //CID:45164 - Reverse_inull
    else {
        BTRMGRLOG_INFO ("Failed to get limit for beacon detection.\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return BTRMGR_RESULT_SUCCESS;
}

BTRMGR_Result_t
BTRMGR_SetLimitBeaconDetection (
    unsigned char   aui8AdapterIdx,
    unsigned char   limited
) {

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    BTRMGR_Beacon_PersistentData_t BeaconPersistentData;
    sprintf(BeaconPersistentData.limitBeaconDetection, "%s", limited ? "true" : "false");
    if (BTRMgr_PI_SetLEBeaconLimitingStatus(&BeaconPersistentData) == eBTRMgrFailure) {
        BTRMGRLOG_ERROR ("Failed to set limit for beacon detection from json.\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    if (limited) {
        BTRMGRLOG_INFO ("Limiting the beacon detection.\n");
     }
     else {
        BTRMGRLOG_INFO ("Removing the limit for beacon detection\n");
     }
    return BTRMGR_RESULT_SUCCESS;
}

#ifdef RDKTV_PERSIST_VOLUME_SKY
static eBTRMgrRet
btrMgr_SetLastVolume (
    unsigned char   aui8AdapterIdx,
    unsigned char   volume
) {

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return eBTRMgrFailInArg;
    }

    BTRMGR_Volume_PersistentData_t VolumePersistentData;
    VolumePersistentData.Volume =  volume;
    if (BTRMgr_PI_SetVolume(&VolumePersistentData) == eBTRMgrFailure) {
        BTRMGRLOG_ERROR ("Failed to set volume from json.\n");
        return eBTRMgrFailure;
    }

    BTRMGRLOG_INFO ("set volume %d.\n",volume);
    return eBTRMgrSuccess;
}

static eBTRMgrRet
btrMgr_GetLastVolume (
    unsigned char   aui8AdapterIdx,
    unsigned char   *pVolume
) {

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || !(pVolume)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return eBTRMgrFailInArg;
    }

    BTRMGR_Volume_PersistentData_t VolumePersistentData;
    if (BTRMgr_PI_GetVolume(&VolumePersistentData) == eBTRMgrFailure) {
        BTRMGRLOG_INFO ("Failed to get volume detection from json.\n");
        return eBTRMgrFailure;
    }

    *pVolume = VolumePersistentData.Volume;
    BTRMGRLOG_INFO ("get volume %d \n",*pVolume);
    return eBTRMgrSuccess;
}

static eBTRMgrRet
btrMgr_SetLastMuteState (
    unsigned char   aui8AdapterIdx,
    gboolean        muted
) {

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt())) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return eBTRMgrFailInArg;
    }

    BTRMGR_Mute_PersistentData_t MutePersistentData;
    sprintf(MutePersistentData.Mute, "%s", muted ? "true" : "false");
    if (BTRMgr_PI_SetMute(&MutePersistentData) == eBTRMgrFailure) {
        BTRMGRLOG_ERROR ("Failed to set mute from json.\n");
        return eBTRMgrFailure;
    }

    BTRMGRLOG_INFO (" set mute %s.\n", muted ? "true":"false");
    return eBTRMgrSuccess;
}

static eBTRMgrRet
btrMgr_GetLastMuteState (
    unsigned char   aui8AdapterIdx,
    gboolean        *pMute
) {

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || !(pMute)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return eBTRMgrFailInArg;
    }

    BTRMGR_Mute_PersistentData_t MutePersistentData;
    if (BTRMgr_PI_GetMute(&MutePersistentData) == eBTRMgrFailure) {
        BTRMGRLOG_INFO ("Failed to get mute detection from json.\n");
        return eBTRMgrFailure;
    }

    *pMute =  (unsigned char)((!strncasecmp(MutePersistentData.Mute, "true", 4)) ? 1 : 0 );
    BTRMGRLOG_INFO (" get mute %s.\n", *pMute ? "true":"false");
    return eBTRMgrSuccess;
}
#endif

BTRMGR_Result_t
BTRMGR_GetLeProperty (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    const char*                  apBtrPropUuid,
    BTRMGR_LeProperty_t          aenLeProperty,
    void*                        vpPropValue
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt() || !apBtrPropUuid) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (eBTRMgrSuccess != btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_LE)) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Rejected !!!\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    enBTRCoreLeProp lenBTRCoreLeProp = enBTRCoreLePropUnknown;

    switch(aenLeProperty) {
    case BTRMGR_LE_PROP_UUID:
        lenBTRCoreLeProp = enBTRCoreLePropGUUID;
        break;
    case BTRMGR_LE_PROP_PRIMARY:
        lenBTRCoreLeProp = enBTRCoreLePropGPrimary;
        break;
    case BTRMGR_LE_PROP_DEVICE:
        lenBTRCoreLeProp = enBTRCoreLePropGDevice;
        break;
    case BTRMGR_LE_PROP_SERVICE:
        lenBTRCoreLeProp = enBTRCoreLePropGService;
        break;
    case BTRMGR_LE_PROP_VALUE:
        lenBTRCoreLeProp = enBTRCoreLePropGValue;
        break;
    case BTRMGR_LE_PROP_NOTIFY:
        lenBTRCoreLeProp = enBTRCoreLePropGNotifying;
        break;
    case BTRMGR_LE_PROP_FLAGS:
        lenBTRCoreLeProp = enBTRCoreLePropGFlags;
        break;
    case BTRMGR_LE_PROP_CHAR:
        lenBTRCoreLeProp = enBTRCoreLePropGChar;
        break;
    default:
        break;
    }

    if (enBTRCoreSuccess != BTRCore_GetLEProperty(ghBTRCoreHdl, ahBTRMgrDevHdl, apBtrPropUuid, lenBTRCoreLeProp, vpPropValue)) {
       BTRMGRLOG_ERROR ("Get LE Property %d for Device/UUID  %llu/%s Failed!!!\n", lenBTRCoreLeProp, ahBTRMgrDevHdl, apBtrPropUuid);
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
    char*                        aLeOpArg,
    char*                        rOpResult
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    BTRMGR_ConnectedDevicesList_t listOfCDevices;
    unsigned char isConnected  = 0;
    unsigned short ui16LoopIdx = 0;
    enBTRCoreLeOp aenBTRCoreLeOp =  enBTRCoreLeOpUnknown;


    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt() || !aBtrLeUuid) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    if (eBTRMgrSuccess != btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_LE)) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Rejected !!!\n");
        return BTRMGR_RESULT_GENERIC_FAILURE;
    }

    /* Check whether the device is in the Connected list */ 
    if(!BTRMGR_GetConnectedDevices (aui8AdapterIdx, &listOfCDevices)) {
        BTRMGRLOG_TRACE ("BTRMGR_GetConnectedDevices is error !!!\n");
    }  //CID:95101 - Checked return

    for ( ;ui16LoopIdx < listOfCDevices.m_numOfDevices; ui16LoopIdx++) {
        if (listOfCDevices.m_deviceProperty[ui16LoopIdx].m_deviceHandle == ahBTRMgrDevHdl) {
            isConnected = listOfCDevices.m_deviceProperty[ui16LoopIdx].m_isConnected;
            break;
        }
    }

    if (!isConnected) {
#if 0
        //TODO: Check if ahBTRMgrDevHdl corresponds to the current adapter
        BTRMGRLOG_ERROR ("LE Device %lld is not connected to perform LE Op!!!\n", ahBTRMgrDevHdl);
        return BTRMGR_RESULT_GENERIC_FAILURE;
#endif
    }


    switch (aLeOpType) {
    case BTRMGR_LE_OP_READY:
        aenBTRCoreLeOp = enBTRCoreLeOpGReady;
        break;
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
    case BTRMGR_LE_OP_UNKNOWN:
    default:
        aenBTRCoreLeOp = enBTRCoreLeOpGReady;
        break;
    }

    if (enBTRCoreSuccess != BTRCore_PerformLEOp (ghBTRCoreHdl, ahBTRMgrDevHdl, aBtrLeUuid, aenBTRCoreLeOp, aLeOpArg, rOpResult)) {
       BTRMGRLOG_ERROR ("Perform LE Op %d for device  %llu Failed!!!\n", aLeOpType, ahBTRMgrDevHdl);
       lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_LE_StartAdvertisement (
    unsigned char                   aui8AdapterIdx,
    BTRMGR_LeCustomAdvertisement_t* pstBTMGR_LeCustomAdvt
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    char *lAdvtType = "peripheral";
    int lLenServiceUUID = 0;
    int lComcastFlagType = 0;
    int lLenManfData = 0;
    unsigned short lManfId = 0;
    unsigned char laDeviceDetails[BTRMGR_DEVICE_COUNT_MAX] = {'\0'};
    unsigned char lManfType;

    /* Parse the adv data  */

    /* Set advertisement type */
    BTRCore_SetAdvertisementType(ghBTRCoreHdl, lAdvtType);

    /* Parse and set the services supported by the device */
    lLenServiceUUID = (int)pstBTMGR_LeCustomAdvt->len_comcastflags;

    lComcastFlagType = (int)pstBTMGR_LeCustomAdvt->type_comcastflags;
    lLenServiceUUID -= 1;

    if ((2 == lComcastFlagType) || (3 == lComcastFlagType)) { /* TODO use macro/enum */
        char lUUID[10];
        unsigned short lu16UUID = ((unsigned short)pstBTMGR_LeCustomAdvt->deviceInfo_UUID_HI << 8) | (unsigned short)pstBTMGR_LeCustomAdvt->deviceInfo_UUID_LO;
        snprintf(lUUID, sizeof(lUUID), "%x", lu16UUID);
        if (enBTRCoreSuccess != BTRCore_SetServiceUUIDs(ghBTRCoreHdl, lUUID)) {
            lenBtrMgrResult = BTRMGR_RESULT_INVALID_INPUT;
        }

        lu16UUID = ((unsigned short)pstBTMGR_LeCustomAdvt->rdk_diag_UUID_HI << 8) | (unsigned short)pstBTMGR_LeCustomAdvt->rdk_diag_UUID_LO;
        snprintf(lUUID, sizeof(lUUID), "%x", lu16UUID);
        if (enBTRCoreSuccess != BTRCore_SetServiceUUIDs(ghBTRCoreHdl, lUUID)) {
            lenBtrMgrResult = BTRMGR_RESULT_INVALID_INPUT;
        }
    }

    /* Parse and set the manufacturer specific data for the device */
    lLenManfData = (int)pstBTMGR_LeCustomAdvt->len_manuf;
    BTRMGRLOG_INFO("Length of manf data is %d \n", lLenManfData);

    lManfType = pstBTMGR_LeCustomAdvt->type_manuf;
    lLenManfData -= sizeof(lManfType);

    lManfId = ((unsigned short)pstBTMGR_LeCustomAdvt->company_HI << 8) | ((unsigned short)pstBTMGR_LeCustomAdvt->company_LO);
    lLenManfData -= sizeof(lManfId);
    BTRMGRLOG_INFO("manf id is %d \n", lManfId);

    int index = 0;
    laDeviceDetails[index + 1] = pstBTMGR_LeCustomAdvt->device_model & 0xF;
    laDeviceDetails[index] = (pstBTMGR_LeCustomAdvt->device_model >> 8) & 0xF;
    index += sizeof(pstBTMGR_LeCustomAdvt->device_model);

    for (int count = 0; count < BTRMGR_DEVICE_MAC_LEN; count++) {
        laDeviceDetails[index] = pstBTMGR_LeCustomAdvt->device_mac[count];
        index += 1;
    }

    BTRCore_SetManufacturerData(ghBTRCoreHdl, lManfId, laDeviceDetails, lLenManfData);

    /* Set the Tx Power */
    BTRCore_SetEnableTxPower(ghBTRCoreHdl, TRUE);

    gIsAdvertisementSet = TRUE;

    /* Add standard advertisement Gatt Info */
    //btrMgr_AddStandardAdvGattInfo();

    /* Start advertising */
    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR("BTRCore is not Inited\n");
        lenBtrMgrResult = BTRMGR_RESULT_INIT_FAILED;
    }
    else if (FALSE == gIsAdvertisementSet) {
        BTRMGRLOG_ERROR("Advertisement data has not been set\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else if (TRUE == gIsDeviceAdvertising) {
        BTRMGRLOG_ERROR("Device is already advertising\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        /* start advertisement */
        if (enBTRCoreSuccess != BTRCore_StartAdvertisement(ghBTRCoreHdl)) {
            BTRMGRLOG_ERROR("Starting advertisement has failed\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            BTRMGRLOG_INFO("Device is advertising\n");
            gIsDeviceAdvertising = TRUE;
        }
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_LE_StopAdvertisement (
    unsigned char aui8AdapterIdx
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR("BTRCore is not Inited\n");
        lenBtrMgrResult = BTRMGR_RESULT_INIT_FAILED;
    }
    else if (FALSE == gIsDeviceAdvertising) {
        BTRMGRLOG_ERROR("Device is not advertising yet\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        /* stop advertisement */
        BTRMGRLOG_INFO("Stopping advertisement\n");
        if (enBTRCoreSuccess != BTRCore_StopAdvertisement(ghBTRCoreHdl)) {
            BTRMGRLOG_ERROR("Stopping advertisement has failed\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        gIsDeviceAdvertising = FALSE;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_LE_GetPropertyValue (
    unsigned char       aui8AdapterIdx,
    char*               aUUID,
    char*               aValue,
    BTRMGR_LeProperty_t aElement
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    char lPropValue[BTRMGR_MAX_STR_LEN] = "";

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR("BTRCore is not Inited\n");
        lenBtrMgrResult = BTRMGR_RESULT_INIT_FAILED;
    }
    else if (FALSE == gIsDeviceAdvertising) {
        BTRMGRLOG_ERROR("Device is not advertising yet\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        if (enBTRCoreSuccess != BTRCore_GetPropertyValue(ghBTRCoreHdl, aUUID, lPropValue, aElement)) {
            BTRMGRLOG_ERROR("Getting property value has failed\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            strncpy(aValue, lPropValue, (BTRMGR_MAX_STR_LEN - 1));
        }
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_LE_SetServiceUUIDs (
    unsigned char   aui8AdapterIdx,
    char*           aUUID
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;


    if (enBTRCoreSuccess != BTRCore_SetServiceUUIDs(ghBTRCoreHdl, aUUID)) {
        lenBtrMgrResult = BTRMGR_RESULT_INVALID_INPUT;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_LE_SetServiceInfo (
    unsigned char   aui8AdapterIdx,
    char*           aUUID,
    unsigned char   aServiceType
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (enBTRCoreSuccess != BTRCore_SetServiceInfo(ghBTRCoreHdl, aUUID, (BOOLEAN)aServiceType)) {
        BTRMGRLOG_ERROR("Could not add service info\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_LE_SetGattInfo (
    unsigned char       aui8AdapterIdx,
    char*               aParentUUID,
    char*               aCharUUID,
    unsigned short      aFlags,
    char*               aValue,
    BTRMGR_LeProperty_t aElement
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (enBTRCoreSuccess != BTRCore_SetGattInfo(ghBTRCoreHdl, aParentUUID, aCharUUID, aFlags, aValue, aElement)) {
        BTRMGRLOG_ERROR("Could not add gatt info\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_LE_SetGattPropertyValue (
    unsigned char       aui8AdapterIdx,
    char*               aUUID,
    char*               aValue,
    BTRMGR_LeProperty_t aElement
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR("BTRCore is not Inited\n");
        lenBtrMgrResult = BTRMGR_RESULT_INIT_FAILED;
    }
    else if (FALSE == gIsDeviceAdvertising) {
        BTRMGRLOG_ERROR("Device is not advertising yet\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        BTRMGRLOG_DEBUG("Value is %s\n", aValue);

#if 1
        if (enBTRCoreSuccess != BTRCore_SetPropertyValue(ghBTRCoreHdl, aUUID, aValue, aElement)) {
            BTRMGRLOG_ERROR("Setting property value has failed\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
#else
        BTRCore_PerformLEOp
#endif
    }

    return lenBtrMgrResult;
}


BTRMGR_Result_t
BTRMGR_SysDiagInfo(
    unsigned char aui8AdapterIdx,
    char *apDiagElement,
    char *apValue,
    BTRMGR_LeOp_t aOpType
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    int lenDiagElement = 0;
    char lPropValue[BTRMGR_LE_STR_LEN_MAX] = {'\0'};   //CID:135225 - Overrurn

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt() || !apDiagElement) {
        BTRMGRLOG_ERROR("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenDiagElement = btrMgr_MapUUIDtoDiagElement(apDiagElement);

    if ((BTRMGR_SYS_DIAG_BEGIN < (BTRMGR_SysDiagChar_t)lenDiagElement) && (BTRMGR_SYS_DIAG_END > (BTRMGR_SysDiagChar_t)lenDiagElement)) {
        if (BTRMGR_LE_OP_READ_VALUE == aOpType) {
            if (eBTRMgrSuccess != BTRMGR_SysDiag_GetData(ghBTRMgrSdHdl, lenDiagElement, lPropValue)) {
                BTRMGRLOG_ERROR("Could not get diagnostic data\n");
                lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                strncpy(apValue, lPropValue, BTRMGR_MAX_STR_LEN - 1);
            }
        }
    }
    else if ((BTRMGR_SYSDIAG_COLUMBO_BEGIN < (BTRMGR_ColumboChar_t)lenDiagElement) && (BTRMGR_SYSDIAG_COLUMBO_END > (BTRMGR_ColumboChar_t)lenDiagElement)) {
        if (BTRMGR_LE_OP_READ_VALUE == aOpType) {
            if (eBTRMgrSuccess != BTRMGR_Columbo_GetData(lenDiagElement, lPropValue)) {
                BTRMGRLOG_ERROR("Could not get diagnostic data\n");
                lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                strncpy(apValue, lPropValue, BTRMGR_MAX_STR_LEN - 1);
            }
        }
        else if (BTRMGR_LE_OP_WRITE_VALUE == aOpType) {
            if (eBTRMgrSuccess != BTRMGR_Columbo_SetData(lenDiagElement, lPropValue)) {
                BTRMGRLOG_ERROR("Could not get diagnostic data\n");
                lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
            }
        }
    }
    else if ((BTRMGR_LE_ONBRDG_BEGIN < (BTRMGR_LeOnboardingChar_t)lenDiagElement) && (BTRMGR_LE_ONBRDG_END > (BTRMGR_LeOnboardingChar_t)lenDiagElement)) {
        if (BTRMGR_LE_OP_READ_VALUE == aOpType) {
            if (eBTRMgrSuccess != BTRMGR_LeOnboarding_GetData(lenDiagElement, lPropValue)) {
                BTRMGRLOG_ERROR("Could not get diagnostic data\n");
                lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                strncpy(apValue, lPropValue, BTRMGR_MAX_STR_LEN - 1);
            }
        }
        else if (BTRMGR_LE_OP_WRITE_VALUE == aOpType) {
            if (eBTRMgrSuccess != BTRMGR_LeOnboarding_SetData(lenDiagElement, apValue)) {
                BTRMGRLOG_ERROR("Could not set diagnostic data\n");
                lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
            }
            else {
                
            }
        }
    }
    else {
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    
    
    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_ConnectToWifi(
    unsigned char   aui8AdapterIdx,
    char*           apSSID,
    char*           apPassword,
    int             aSecMode
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;

    if (!ghBTRCoreHdl || !ghBTRMgrSdHdl) {
        BTRMGRLOG_ERROR("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if (aui8AdapterIdx > btrMgr_GetAdapterCnt()) {
        BTRMGRLOG_ERROR("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }

    lenBtrMgrResult = BTRMGR_SysDiag_ConnectToWifi(ghBTRMgrSdHdl, apSSID, apPassword, aSecMode);

    return lenBtrMgrResult;
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


/*  Local Op Threads Prototypes */
static gpointer
btrMgr_g_main_loop_Task (
    gpointer appvMainLoop
) {
    GMainLoop* pMainLoop = (GMainLoop*)appvMainLoop;
    if (!pMainLoop) {
        BTRMGRLOG_INFO ("GMainLoop Error - In arguments Exiting\n");
        return NULL;
    }

    BTRMGRLOG_INFO ("GMainLoop Running - %p - %p\n", pMainLoop, appvMainLoop);
    g_main_loop_run (pMainLoop);

    return appvMainLoop;
}


/*  Incoming Callbacks */
static gboolean
btrMgr_DiscoveryHoldOffTimerCb (
    gpointer    gptr
) {
    unsigned char lui8AdapterIdx = 0;

    BTRMGRLOG_DEBUG ("CB context Invoked for btrMgr_DiscoveryHoldOffTimerCb || TimeOutReference - %u\n", gTimeOutRef);

    if (gptr) {
        lui8AdapterIdx = *(unsigned char*)gptr;
    } else {
        BTRMGRLOG_WARN ("CB context received NULL arg!\n");
    }

    gTimeOutRef = 0;

    if (btrMgr_PostCheckDiscoveryStatus (lui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN) != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Post Check Disc Status Failed!\n");
    }

    return FALSE;
}


static gboolean
btrMgr_ConnPwrStChangeTimerCb (
    gpointer gptr
) {
    unsigned char lui8AdapterIdx = 0;

    BTRMGRLOG_DEBUG ("CB context Invoked for btrMgr_ConnPwrStChangeTimerCb || TimeOutReference - %u\n", gConnPwrStChangeTimeOutRef);

    if (gptr) {
        lui8AdapterIdx = *(unsigned char*)gptr;
    } else {
        BTRMGRLOG_WARN ("CB context received NULL arg!\n");
    }

    gConnPwrStChangeTimeOutRef = 0;

    if (BTRMGR_StartAudioStreamingOut_StartUp(lui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT) != BTRMGR_RESULT_SUCCESS) {
        BTRMGRLOG_ERROR ("ConnPwrStChange - BTRMGR_StartAudioStreamingOut_StartUp Failed!\n");
    }

    return FALSE;
}


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
        lstBTRMgrStrmInfo->bytesWritten += aui32AcDataLen;
    }   //CID:23337 - Forward null

    return leBtrMgrAcRet;
}


static eBTRMgrRet
btrMgr_ACStatusCb (
    stBTRMgrMediaStatus*    apstBtrMgrAcStatus,
    void*                   apvUserData
) {
    eBTRMgrRet              leBtrMgrAcRet = eBTRMgrSuccess;
    stBTRMgrStreamingInfo*  lpstBTRMgrStrmInfo  = (stBTRMgrStreamingInfo*)apvUserData; 

    if (lpstBTRMgrStrmInfo && apstBtrMgrAcStatus) {
        stBTRMgrMediaStatus lstBtrMgrSoStatus;

        //TODO; Dont just memcpy from AC Status to SO Status, map it correctly in the future.
        BTRMGRLOG_WARN ("STATUS CHANGED\n");
        memcpy(&lstBtrMgrSoStatus, apstBtrMgrAcStatus, sizeof(stBTRMgrMediaStatus));
        if ((leBtrMgrAcRet = BTRMgr_SO_SetStatus(lpstBTRMgrStrmInfo->hBTRMgrSoHdl, &lstBtrMgrSoStatus)) != eBTRMgrSuccess) {
            BTRMGRLOG_ERROR ("BTRMgr_SO_SetStatus FAILED = %d\n", leBtrMgrAcRet);
        }
    }

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


static eBTRMgrRet
btrMgr_SDStatusCb (
    stBTRMgrSysDiagStatus*  apstBtrMgrSdStatus,
    void*                   apvUserData
) {
    eBTRMgrRet           leBtrMgrSdRet = eBTRMgrFailure;

    if ((apstBtrMgrSdStatus != NULL) &&
        (gIsAudOutStartupInProgress != BTRMGR_STARTUP_AUD_INPROGRESS) &&
        (apstBtrMgrSdStatus->enSysDiagChar == BTRMGR_SYS_DIAG_POWERSTATE) &&
        !strncmp(apstBtrMgrSdStatus->pcSysDiagRes, BTRMGR_SYS_DIAG_PWRST_ON, strlen(BTRMGR_SYS_DIAG_PWRST_ON))) {

        gConnPwrStChTimeOutCbData = 0; // TODO: Change when this entire file is made re-entrant
        if ((gConnPwrStChangeTimeOutRef =  g_timeout_add_seconds (BTRMGR_DEVCONN_PWRST_CHANGE_TIME, btrMgr_ConnPwrStChangeTimerCb, (gpointer)&gConnPwrStChTimeOutCbData)) != 0)
            leBtrMgrSdRet = eBTRMgrSuccess;
    }

    return leBtrMgrSdRet;
}


static enBTRCoreRet
btrMgr_DeviceStatusCb (
    stBTRCoreDevStatusCBInfo*   p_StatusCB,
    void*                       apvUserData
) {
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
    BTRMGR_EventMessage_t   lstEventMessage;
    BTRMGR_DeviceType_t     lBtrMgrDevType  = BTRMGR_DEVICE_TYPE_UNKNOWN;


    memset (&lstEventMessage, 0, sizeof(lstEventMessage));

    BTRMGRLOG_INFO ("Received status callback\n");

    if (p_StatusCB) {

        lBtrMgrDevType = btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass);
        BTRMGRLOG_INFO (" Received status callback device type %d PrevState %d State %d\n",lBtrMgrDevType, p_StatusCB->eDevicePrevState, p_StatusCB->eDeviceCurrState);

        if (!gIsHidGamePadEnabled &&
            (lBtrMgrDevType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD)) {
             BTRMGRLOG_WARN ("Rejecting status callback - BTR HID Gamepad is currently Disabled!\n");
             return lenBtrCoreRet;
        }

        switch (p_StatusCB->eDeviceCurrState) {
        case enBTRCoreDevStPaired:
            /* Post this event only for HID Devices and Audio-In Devices */
            if ((p_StatusCB->eDeviceType == enBTRCoreHID) ||
                (p_StatusCB->eDeviceType == enBTRCoreMobileAudioIn) ||
                (p_StatusCB->eDeviceType == enBTRCorePCAudioIn)) {

                btrMgr_GetDiscoveredDevInfo (p_StatusCB->deviceId, &lstEventMessage.m_discoveredDevice);
                lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);  /* Post a callback */
                }

                if (p_StatusCB->eDeviceType == enBTRCoreHID) {
                    BTRMGR_GetPairedDevices (gDefaultAdapterContext.adapter_number, &gListOfPairedDevices);
                }
            }
            break;
        case enBTRCoreDevStInitialized:
            break;
        case enBTRCoreDevStConnecting:
            break;
        case enBTRCoreDevStConnected:               /*  notify user device back   */
            if (enBTRCoreDevStLost == p_StatusCB->eDevicePrevState || enBTRCoreDevStPaired == p_StatusCB->eDevicePrevState) {
                if (gIsAudOutStartupInProgress != BTRMGR_STARTUP_AUD_INPROGRESS) {
                    unsigned char doPostCheck = 0;
                    btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_FOUND);

                    if ((lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET)  ||
                        (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HANDSFREE)         ||
                        (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_LOUDSPEAKER)       ||
                        (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HEADPHONES)        ||
                        (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO)    ||
                        (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_CAR_AUDIO)         ||
                        (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE) ){

                        if (lstEventMessage.m_pairedDevice.m_isLastConnectedDevice) {
                            btrMgr_PreCheckDiscoveryStatus (0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
                            doPostCheck = 1;
                        }
                    }
                    else if ((lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID)
                            || (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD)) {
                        lstEventMessage.m_pairedDevice.m_deviceType = BTRMGR_DEVICE_TYPE_HID ;
                        btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 1);
                    }

                    if (gfpcBBTRMgrEventOut) {
                        gfpcBBTRMgrEventOut(lstEventMessage);  /* Post a callback */
                    }

                    if (doPostCheck) {
                        btrMgr_PostCheckDiscoveryStatus (0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
                    }
                }
            }
            else if (enBTRCoreDevStInitialized != p_StatusCB->eDevicePrevState) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE);
                lstEventMessage.m_pairedDevice.m_isConnected = 1;

                if ((lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HANDSFREE) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_LOUDSPEAKER) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HEADPHONES) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_CAR_AUDIO) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)) {

                    /* Update the flag as the Device is Connected */
                    if ((lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID)
                        || (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD)) {
                        lstEventMessage.m_pairedDevice.m_deviceType = BTRMGR_DEVICE_TYPE_HID;
                        BTRMGRLOG_WARN ("\n Sending Event for HID ");
                        btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 1);
                    }
                    else if (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_TILE) {
                        btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 1);
                        ghBTRMgrDevHdlLastConnected = lstEventMessage.m_pairedDevice.m_deviceHandle;
                    }


                    if (gfpcBBTRMgrEventOut) {
                        gfpcBBTRMgrEventOut(lstEventMessage);  /* Post a callback */
                    }
                }
            }
            break;
        case enBTRCoreDevStDisconnected:
            if (enBTRCoreDevStConnecting != p_StatusCB->eDevicePrevState) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE);
                /* external modules like thunder,servicemanager yet to define
                 * HID sub types hence type is sending as BTRMGR_DEVICE_TYPE_HID in events
                 */
                if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD)
                    lstEventMessage.m_pairedDevice.m_deviceType = BTRMGR_DEVICE_TYPE_HID;

                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
                }

                BTRMGRLOG_INFO ("lstEventMessage.m_pairedDevice.m_deviceType = %d\n", lstEventMessage.m_pairedDevice.m_deviceType);
                if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TILE) {
                    /* update the flags as the LE device is NOT Connected */
                    gIsLeDeviceConnected = 0;
                }
                else if ((lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID)
                        || (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID_GAMEPAD)) {
                    btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 0);
                }
                else if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming == p_StatusCB->deviceId)) {
                    /* update the flags as the device is NOT Connected */
                    btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 0);

                    if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                        lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        if(!BTRMGR_StopAudioStreamingIn(0, ghBTRMgrDevHdlCurStreaming))
                        {
                            BTRMGRLOG_ERROR ("BTRMGR_StopAudioStreamingIn error \n ");  //CID:41286 - Checked return
                        }
                        ghBTRMgrDevHdlLastConnected = 0;
                    }
                    else {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        if (BTRMGR_RESULT_SUCCESS != BTRMGR_StopAudioStreamingOut(0, ghBTRMgrDevHdlCurStreaming)) {
                            BTRMGRLOG_ERROR ("BTRMGR_StopAudioStreamingOut error \n ");  //CID:41354 - Checked return
                        }
                    }
                }
                else if ((btrMgr_IsDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle) == 1) &&
                         (ghBTRMgrDevHdlLastConnected == lstEventMessage.m_pairedDevice.m_deviceHandle)) {

                    if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                        lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
                        ghBTRMgrDevHdlLastConnected = 0;
                    }
                    else {
                        //TODO: Add what to do for other device types
                    }
                }
            }
            break;
        case enBTRCoreDevStLost:
            if( !gIsUserInitiated ) {

                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OUT_OF_RANGE);
                if ((lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET)   ||
                    (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HANDSFREE)          ||
                    (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_LOUDSPEAKER)        ||
                    (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HEADPHONES)         ||
                    (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO)     ||
                    (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_CAR_AUDIO)          ||
                    (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)) {

                    btrMgr_PreCheckDiscoveryStatus (0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);

                    if (gfpcBBTRMgrEventOut) {
                        gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
                    }

                    if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming == p_StatusCB->deviceId)) {
                        /* update the flags as the device is NOT Connected */
                        btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 0);

                        BTRMGRLOG_INFO ("lstEventMessage.m_pairedDevice.m_deviceType = %d\n", lstEventMessage.m_pairedDevice.m_deviceType);
                        if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                            lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
                            /* Stop the playback which already stopped internally but to free up the memory */
                            BTRMGR_StopAudioStreamingIn(0, ghBTRMgrDevHdlCurStreaming);
                            ghBTRMgrDevHdlLastConnected = 0;
                        }
                        else {
                            /* Stop the playback which already stopped internally but to free up the memory */
                            if (BTRMGR_RESULT_SUCCESS != BTRMGR_StopAudioStreamingOut (0, ghBTRMgrDevHdlCurStreaming)) {
                                BTRMGRLOG_ERROR ("BTRMGR_StopAudioStreamingOut error \n ");  //CID:41354 - Checked return
                            }
                        }
                    }
                    btrMgr_PostCheckDiscoveryStatus (0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
                }
            }
            gIsUserInitiated = 0;
            break;
        case enBTRCoreDevStPlaying:
            if (btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_TABLET) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST);

                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
                }
            }
            break;
        case enBTRCoreDevStOpReady:
            if (btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_TILE) {
                memset(lstEventMessage.m_deviceOpInfo.m_deviceAddress, '\0', BTRMGR_NAME_LEN_MAX);
                memset(lstEventMessage.m_deviceOpInfo.m_name, '\0', BTRMGR_NAME_LEN_MAX);
                memset(lstEventMessage.m_deviceOpInfo.m_uuid, '\0', BTRMGR_MAX_STR_LEN);
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OP_READY);
                //Not a big fan of devOpResponse. We should think of a better way to do this
                memset(lstEventMessage.m_deviceOpInfo.m_notifyData, '\0', BTRMGR_MAX_DEV_OP_DATA_LEN);
                strncpy(lstEventMessage.m_deviceOpInfo.m_notifyData, p_StatusCB->devOpResponse,
                            strlen(p_StatusCB->devOpResponse) < BTRMGR_MAX_DEV_OP_DATA_LEN ? strlen(p_StatusCB->devOpResponse) : BTRMGR_MAX_DEV_OP_DATA_LEN - 1);

                /* Post a callback */
                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);
                }
            }
            break;
        case enBTRCoreDevStOpInfo:
            if (btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_TILE) {
                memset(lstEventMessage.m_deviceOpInfo.m_deviceAddress, '\0', BTRMGR_NAME_LEN_MAX);
                memset(lstEventMessage.m_deviceOpInfo.m_name, '\0', BTRMGR_NAME_LEN_MAX);
                memset(lstEventMessage.m_deviceOpInfo.m_uuid, '\0', BTRMGR_MAX_STR_LEN);
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OP_INFORMATION);
                //Not a big fan of devOpResponse. We should think of a better way to do this
                memset(lstEventMessage.m_deviceOpInfo.m_notifyData, '\0', BTRMGR_MAX_DEV_OP_DATA_LEN);
                strncpy(lstEventMessage.m_deviceOpInfo.m_notifyData, p_StatusCB->devOpResponse,
                            strlen(p_StatusCB->devOpResponse) < BTRMGR_MAX_DEV_OP_DATA_LEN ? strlen(p_StatusCB->devOpResponse) : BTRMGR_MAX_DEV_OP_DATA_LEN - 1);

                /* Post a callback */
                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);
                }
            }
            else if (BTRMGR_LE_OP_READ_VALUE == (BTRMGR_LeOp_t)p_StatusCB->eCoreLeOper) {
                btrMgr_MapDevstatusInfoToEventInfo((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OP_INFORMATION);
                /* Post a callback */
                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);
                }

                /* Max 10 sec timeout - Polled at 100ms second interval */
                {
                    unsigned int ui32sleepIdx = 100;

                    do {
                        usleep(100000);
                    } while ((gEventRespReceived == 0) && (--ui32sleepIdx));

                }

                strncpy(p_StatusCB->devOpResponse, gLeReadOpResponse, BTRMGR_MAX_STR_LEN - 1);
            }
            else if(BTRMGR_LE_OP_WRITE_VALUE == (BTRMGR_LeOp_t)p_StatusCB->eCoreLeOper) {
                btrMgr_MapDevstatusInfoToEventInfo((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OP_INFORMATION);
                memset(lstEventMessage.m_deviceOpInfo.m_writeData, '\0', BTRMGR_MAX_DEV_OP_DATA_LEN);
                strncpy(lstEventMessage.m_deviceOpInfo.m_writeData, p_StatusCB->devOpResponse,
                            strlen(p_StatusCB->devOpResponse) < BTRMGR_MAX_DEV_OP_DATA_LEN ? strlen(p_StatusCB->devOpResponse) : BTRMGR_MAX_DEV_OP_DATA_LEN - 1);
                
                /* Post a callback */
                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);
                }
            }
            else {
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
    stBTRCoreDiscoveryCBInfo*   astBTRCoreDiscoveryCbInfo,
    void*                       apvUserData
) {
    enBTRCoreRet        lenBtrCoreRet   = enBTRCoreSuccess;

    BTRMGRLOG_TRACE ("callback type = %d\n", astBTRCoreDiscoveryCbInfo->type);

    if (astBTRCoreDiscoveryCbInfo->type == enBTRCoreOpTypeDevice) {
        BTRMGR_DiscoveryHandle_t* ldiscoveryHdl  = NULL;

        if ((ldiscoveryHdl = btrMgr_GetDiscoveryInProgress()) || (astBTRCoreDiscoveryCbInfo->device.bFound == FALSE)) { /* Not a big fan of this */
            if (ldiscoveryHdl &&
                (btrMgr_GetDiscoveryDeviceType(ldiscoveryHdl) == BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT)) {
                // Acceptable hack for DELIA-39526
                // We would have already reported a list of discovered devices to XRE/Client of BTRMgr if
                // they have requested for the same. But this list will be invalidated by the Resume and if
                // a Pairing Op is requested based on this stale information it will fail, but we will not end up
                // in a loop repeating the same sequence of events as we understand that on the first resume
                // Bluez/BTRCore will correctly give us the device Name. But if we get a new Audio-Out device which exhibits the
                // same behavior then we will repeat this again. As we do the above only for Audio-Out devices
                // non-Audio-Out devices cannot trigger this logic. So we will perform a focussed Audio-Out internal rescan
                if (((astBTRCoreDiscoveryCbInfo->device.enDeviceType == enBTRCore_DC_WearableHeadset)   ||
                     (astBTRCoreDiscoveryCbInfo->device.enDeviceType == enBTRCore_DC_Loudspeaker)       ||
                     (astBTRCoreDiscoveryCbInfo->device.enDeviceType == enBTRCore_DC_PortableAudio)     ||
                     (astBTRCoreDiscoveryCbInfo->device.enDeviceType == enBTRCore_DC_CarAudio)          ||
                     (astBTRCoreDiscoveryCbInfo->device.enDeviceType == enBTRCore_DC_HIFIAudioDevice)   ||
                     (astBTRCoreDiscoveryCbInfo->device.enDeviceType == enBTRCore_DC_Headphones)) &&
                    (strlen(astBTRCoreDiscoveryCbInfo->device.pcDeviceName) == strlen(astBTRCoreDiscoveryCbInfo->device.pcDeviceAddress)) &&
                    btrMgr_IsDevNameSameAsAddress(astBTRCoreDiscoveryCbInfo->device.pcDeviceName, astBTRCoreDiscoveryCbInfo->device.pcDeviceAddress, strlen(astBTRCoreDiscoveryCbInfo->device.pcDeviceName)) &&
                    !btrMgr_CheckIfDevicePrevDetected(astBTRCoreDiscoveryCbInfo->device.tDeviceId)) {

                    BTRMGR_DiscoveredDevicesList_t  lstDiscDevices;
                    eBTRMgrRet                      lenBtrMgrRet    = eBTRMgrFailure;
                    BTRMGR_Result_t                 lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;

                    gIsDiscoveryOpInternal = TRUE;
                    lenBtrMgrRet = btrMgr_PauseDeviceDiscovery(0, ldiscoveryHdl);
                    BTRMGRLOG_WARN ("Called btrMgr_PauseDeviceDiscovery = %d\n", lenBtrMgrRet);

                    lenBtrMgrResult = BTRMGR_GetDiscoveredDevices_Internal(0, &lstDiscDevices);
                    BTRMGRLOG_WARN ("Stored BTRMGR_GetDiscoveredDevices_Internal = %d\n", lenBtrMgrResult);

                    lenBtrMgrRet = btrMgr_ResumeDeviceDiscovery(0, ldiscoveryHdl);
                    BTRMGRLOG_WARN ("Called btrMgr_ResumeDeviceDiscovery = %d\n", lenBtrMgrRet);
                }
                else {
                    BTRMGR_EventMessage_t lstEventMessage;
                    memset (&lstEventMessage, 0, sizeof(lstEventMessage));

                    gIsDiscoveryOpInternal = FALSE;
                    btrMgr_MapDevstatusInfoToEventInfo ((void*)&astBTRCoreDiscoveryCbInfo->device, &lstEventMessage, BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE);

                    if (gfpcBBTRMgrEventOut) {
                        gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
                    }
                }
            }
            else {
                BTRMGR_EventMessage_t lstEventMessage;
                memset (&lstEventMessage, 0, sizeof(lstEventMessage));

                if (!gIsHidGamePadEnabled &&
                     (astBTRCoreDiscoveryCbInfo->device.enDeviceType == enBTRCore_DC_HID_GamePad)) {
                     BTRMGRLOG_WARN ("BTR HID Gamepad is currently Disabled!\n");
                     return lenBtrCoreRet;
                }

                btrMgr_MapDevstatusInfoToEventInfo ((void*)&astBTRCoreDiscoveryCbInfo->device, &lstEventMessage, BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE);

                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
                }
            }
        }
    }
    else if (astBTRCoreDiscoveryCbInfo->type == enBTRCoreOpTypeAdapter) {
        BTRMGRLOG_INFO ("adapter number = %d, discoverable = %d, discovering = %d\n",
                astBTRCoreDiscoveryCbInfo->adapter.adapter_number,
                astBTRCoreDiscoveryCbInfo->adapter.discoverable,
                astBTRCoreDiscoveryCbInfo->adapter.bDiscovering);

        gIsAdapterDiscovering = astBTRCoreDiscoveryCbInfo->adapter.bDiscovering;

#if 0
        if (gfpcBBTRMgrEventOut && (gIsDiscoveryOpInternal == FALSE)) {
            BTRMGR_EventMessage_t lstEventMessage;
            memset (&lstEventMessage, 0, sizeof(lstEventMessage));

            lstEventMessage.m_adapterIndex = astBTRCoreDiscoveryCbInfo->adapter.adapter_number;
            if (astBTRCoreDiscoveryCbInfo->adapter.bDiscovering) {
                lstEventMessage.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED;
            }
            else {
                lstEventMessage.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
            }

            gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
        }
#endif

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
    BTRMGR_Result_t         lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    eBTRMgrRet              lenBtrMgrRet    = eBTRMgrSuccess;
    BTRMGR_DeviceType_t     lBtrMgrDevType  = BTRMGR_DEVICE_TYPE_UNKNOWN;
    BTRMGR_Events_t         lBtMgrOutEvent  = -1;
    unsigned char           lui8AdapterIdx  = 0;
    BTRMGR_EventMessage_t   lstEventMessage;

     if (!apstConnCbInfo) {
        BTRMGRLOG_ERROR ("Invaliid argument\n");
        return enBTRCoreInvalidArg;
    }

    lBtrMgrDevType = btrMgr_MapDeviceTypeFromCore(apstConnCbInfo->stFoundDevice.enDeviceType);
    if(! ((BTRMGR_DEVICE_TYPE_HID == lBtrMgrDevType) || (BTRMGR_DEVICE_TYPE_HID_GAMEPAD == lBtrMgrDevType ))) {
        if (!gIsAudioInEnabled) {
            BTRMGRLOG_WARN ("Incoming Connection Rejected - BTR AudioIn is currently Disabled!\n");
            *api32ConnInIntimResp = 0;
            return lenBtrCoreRet;
        }
    }

    lenBtrMgrRet = btrMgr_PreCheckDiscoveryStatus(lui8AdapterIdx, lBtrMgrDevType);

    if (eBTRMgrSuccess != lenBtrMgrRet) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Rejected !!!\n");
        return enBTRCoreFailure;
    }

    if (apstConnCbInfo->ui32devPassKey) {
        BTRMGRLOG_WARN ("Incoming Connection passkey = %06d\n", apstConnCbInfo->ui32devPassKey);
    }

    memset (&lstEventMessage, 0, sizeof(lstEventMessage));
    btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST);

    /* We mustn't need this conditional check; We must always reset the globals before invoking the Callbacks. But as per code review comments, resetting it only for HID */
    if ((BTRMGR_DEVICE_TYPE_HID == lBtrMgrDevType)||(BTRMGR_DEVICE_TYPE_HID_GAMEPAD == lBtrMgrDevType)) {
        gEventRespReceived = 0;
        gAcceptConnection  = 0;
    }

    if (gfpcBBTRMgrEventOut) {
        gfpcBBTRMgrEventOut(lstEventMessage); /* Post a callback */
    }

    /* Used for PIN DISPLAY request */
    if (!apstConnCbInfo->ucIsReqConfirmation) {
        BTRMGRLOG_WARN ("This paring request does not require a confirmation BUT it might need you to enter the PIN at the specified device\n");
        /* Set the return to true; just in case */
        *api32ConnInIntimResp = 1;
        btrMgr_PostCheckDiscoveryStatus (lui8AdapterIdx, lBtrMgrDevType);
        return lenBtrCoreRet;
    }

    /* Max 25 sec timeout - Polled at 500ms second interval */
    {
        unsigned int ui32sleepIdx = 50;

        do {
            usleep(500000);
        } while ((gEventRespReceived == 0) && (--ui32sleepIdx));

    }

    BTRMGRLOG_ERROR ("you picked %d\n", gAcceptConnection);
    if (gEventRespReceived && gAcceptConnection == 1) {
        BTRMGRLOG_ERROR ("Pin-Passkey accepted\n");
        gAcceptConnection = 0;  //reset variabhle for the next connection
        *api32ConnInIntimResp = 1;
    }
    else {
        BTRMGRLOG_ERROR ("Pin-Passkey Rejected\n");
        gAcceptConnection = 0;  //reset variabhle for the next connection
        *api32ConnInIntimResp = 0;
    }

    gEventRespReceived = 0;

    if (*api32ConnInIntimResp == 1) {
        BTRMGRLOG_INFO ("Paired Successfully\n");
        lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
    }
    else {
        BTRMGRLOG_ERROR ("Failed to pair a device\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_PAIRING_FAILED;
    }


    lstEventMessage.m_adapterIndex = lui8AdapterIdx;
    lstEventMessage.m_eventType    = lBtMgrOutEvent;

    if (gfpcBBTRMgrEventOut) {
        gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
    }

    btrMgr_PostCheckDiscoveryStatus (lui8AdapterIdx, lBtrMgrDevType);

    (void)lenBtrMgrResult;

    return lenBtrCoreRet;
}


static enBTRCoreRet
btrMgr_ConnectionInAuthenticationCb (
    stBTRCoreConnCBInfo*    apstConnCbInfo,
    int*                    api32ConnInAuthResp,
    void*                   apvUserData
) {
    enBTRCoreRet            lenBtrCoreRet   = enBTRCoreSuccess;
    eBTRMgrRet              lenBtrMgrRet    = eBTRMgrSuccess;
    BTRMGR_DeviceType_t     lBtrMgrDevType  = BTRMGR_DEVICE_TYPE_UNKNOWN;
    unsigned char           lui8AdapterIdx  = 0;


    if (!apstConnCbInfo) {
        BTRMGRLOG_ERROR ("Invaliid argument\n");
        return enBTRCoreInvalidArg;
    }

    lBtrMgrDevType = btrMgr_MapDeviceTypeFromCore(apstConnCbInfo->stKnownDevice.enDeviceType);
    /* Move this check into DeviceClass scope? */
    lenBtrMgrRet = btrMgr_PreCheckDiscoveryStatus(lui8AdapterIdx, lBtrMgrDevType);

    if (eBTRMgrSuccess != lenBtrMgrRet) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Rejected !!!\n");
        return enBTRCoreFailure;
    }


    if (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_SmartPhone ||
        apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_Tablet) {

        if (!gIsAudioInEnabled) {
            BTRMGRLOG_WARN ("Incoming Connection Rejected - BTR AudioIn is currently Disabled!\n");
            *api32ConnInAuthResp = 0;
            btrMgr_PostCheckDiscoveryStatus (lui8AdapterIdx, lBtrMgrDevType);
            return lenBtrCoreRet;
        }

        BTRMGRLOG_WARN ("Incoming Connection from BT SmartPhone/Tablet\n");

        if (apstConnCbInfo->stKnownDevice.tDeviceId != ghBTRMgrDevHdlLastConnected) {
            BTRMGR_EventMessage_t lstEventMessage;

            memset (&lstEventMessage, 0, sizeof(lstEventMessage));
            btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);

            if (gfpcBBTRMgrEventOut) {
                gfpcBBTRMgrEventOut(lstEventMessage);     /* Post a callback */
            }

            /* PairedList updation is necessary for Connect event than Disconnect event */
            BTRMGR_GetPairedDevices (lstEventMessage.m_adapterIndex, &gListOfPairedDevices);

            
            {   /* Max 25 sec timeout - Polled at 500ms second interval */
                unsigned int ui32sleepIdx = 50;

                do {
                    usleep(500000);
                } while ((gEventRespReceived == 0) && (--ui32sleepIdx));
            }


            if (gEventRespReceived == 1) {
                BTRMGRLOG_ERROR ("you picked %d\n", gAcceptConnection);
                if (gAcceptConnection == 1) {
                    BTRMGRLOG_WARN ("Incoming Connection accepted\n");

                    if (ghBTRMgrDevHdlLastConnected) {
                        BTRMGRLOG_DEBUG ("Disconnecting from previous AudioIn connection(%llu)!\n", ghBTRMgrDevHdlLastConnected);

                        if (BTRMGR_RESULT_SUCCESS != BTRMGR_DisconnectFromDevice(0, ghBTRMgrDevHdlLastConnected)) {
                            BTRMGRLOG_ERROR ("Failed to Disconnect from previous AudioIn connection(%llu)!\n", ghBTRMgrDevHdlLastConnected);
                            //return lenBtrCoreRet;
                        }
                    }

                    ghBTRMgrDevHdlLastConnected = lstEventMessage.m_externalDevice.m_deviceHandle;
                }
                else {
                    BTRMGRLOG_ERROR ("Incoming Connection denied\n");
                }

                *api32ConnInAuthResp = gAcceptConnection;
            }
            else {
                BTRMGRLOG_ERROR ("Incoming Connection Rejected\n");
                *api32ConnInAuthResp = 0;
            }

            gEventRespReceived = 0;
        }
        else {
            BTRMGRLOG_ERROR ("Incoming Connection From Dev = %lld Status %d LastConnectedDev = %lld\n", apstConnCbInfo->stKnownDevice.tDeviceId, gAcceptConnection, ghBTRMgrDevHdlLastConnected);
            *api32ConnInAuthResp = gAcceptConnection;
        }
    }
    else if ((apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_WearableHeadset)   ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_Loudspeaker)       ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_PortableAudio)     ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_CarAudio)          ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HIFIAudioDevice)   ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_Headphones)) {

        BTRMGRLOG_WARN ("Incoming Connection from BT Speaker/Headset\n");
        if (btrMgr_GetDevPaired(apstConnCbInfo->stKnownDevice.tDeviceId) && (apstConnCbInfo->stKnownDevice.tDeviceId == ghBTRMgrDevHdlLastConnected)) {

            BTRMGRLOG_DEBUG ("Paired - Last Connected device...\n");

            if (gIsAudOutStartupInProgress != BTRMGR_STARTUP_AUD_INPROGRESS) {
                BTRMGR_EventMessage_t lstEventMessage;

                memset (&lstEventMessage, 0, sizeof(lstEventMessage));
                btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);

                //TODO: Check if XRE wants to bring up a Pop-up or Respond
                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);     /* Post a callback */
                }


                {   /* Max 200msec timeout - Polled at 50ms second interval */
                    unsigned int ui32sleepIdx = 4;

                    do {
                        usleep(50000);
                    } while ((gEventRespReceived == 0) && (--ui32sleepIdx));

                    gEventRespReceived = 0;
                }
            }

            BTRMGRLOG_WARN ("Incoming Connection accepted\n");
            *api32ConnInAuthResp = 1;
        }
        else {
            BTRMGRLOG_ERROR ("Incoming Connection denied\n");
            *api32ConnInAuthResp = 0;
        }
    }
    else if ((apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_Keyboard)      ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_Mouse)         ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_MouseKeyBoard) ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_AudioRemote)   ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_Joystick)      ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_GamePad)) {


        if ((!gIsHidGamePadEnabled) && (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_GamePad)) {
            BTRMGRLOG_WARN ("Incoming Connection Rejected - BTR GamePad is currently Disabled!\n");
            *api32ConnInAuthResp = 0;
            btrMgr_PostCheckDiscoveryStatus (lui8AdapterIdx, lBtrMgrDevType);
            return lenBtrCoreRet;
        }

        BTRMGRLOG_WARN ("Incoming Connection from BT HID device\n");
        /* PairedList updation is necessary for Connect event than Disconnect event */
        BTRMGR_GetPairedDevices (gDefaultAdapterContext.adapter_number, &gListOfPairedDevices);

        if (btrMgr_GetDevPaired(apstConnCbInfo->stKnownDevice.tDeviceId)) {
            BTRMGR_EventMessage_t lstEventMessage;

            memset (&lstEventMessage, 0, sizeof(lstEventMessage));
            btrMgr_MapDevstatusInfoToEventInfo ((void*)apstConnCbInfo, &lstEventMessage, BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);

            //TODO: Check if XRE wants to bring up a Pop-up or Respond
            if (gfpcBBTRMgrEventOut) {
                gfpcBBTRMgrEventOut(lstEventMessage);     /* Post a callback */
            }


            {   /* Max 2 sec timeout - Polled at 50ms second interval */
                unsigned int ui32sleepIdx = 40;

                do {
                    usleep(50000);
                } while ((gEventRespReceived == 0) && (--ui32sleepIdx));


                if (gEventRespReceived == 0)
                    *api32ConnInAuthResp = 1;
                else
                    *api32ConnInAuthResp = gAcceptConnection;


                gEventRespReceived = 0;
            }

            BTRMGRLOG_WARN ("Incoming Connection accepted - %d\n", *api32ConnInAuthResp);
        }
        else {
            BTRMGRLOG_ERROR ("Incoming Connection denied\n");
            *api32ConnInAuthResp = 0;
        }
    }
    else
    {
        BTRMGRLOG_ERROR ("Incoming Connection Auth Callback\n");
    }

    btrMgr_PostCheckDiscoveryStatus (lui8AdapterIdx, lBtrMgrDevType);

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
        strncpy (lstEventMessage.m_mediaInfo.m_name, mediaStatusCB->deviceName, BTRMGR_NAME_LEN_MAX -1);
        lstEventMessage.m_mediaInfo.m_name[BTRMGR_NAME_LEN_MAX -1] = '\0';  //CID:136544 - Buffer size warning

        switch (mediaStatus->eBTMediaStUpdate) {
        case eBTRCoreMediaTrkStStarted:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_STARTED;
            gMediaPlaybackStPrev = lstEventMessage.m_eventType;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStPlaying: 
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_PLAYING;
            gMediaPlaybackStPrev = lstEventMessage.m_eventType;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStPaused:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_PAUSED;
            gMediaPlaybackStPrev = lstEventMessage.m_eventType;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStStopped:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_STOPPED;
            gMediaPlaybackStPrev = lstEventMessage.m_eventType;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkPosition:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_POSITION;
            gMediaPlaybackStPrev = lstEventMessage.m_eventType;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStChanged:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_CHANGED;
            gMediaPlaybackStPrev = lstEventMessage.m_eventType;
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaTrackInfo, &mediaStatus->m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
            break;
        case eBTRCoreMediaPlaybackEnded:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED;
            gMediaPlaybackStPrev = lstEventMessage.m_eventType;
            break;
        case eBTRCoreMediaPlyrName:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_NAME;
            strncpy (lstEventMessage.m_mediaInfo.m_mediaPlayerName, mediaStatus->m_mediaPlayerName, BTRMGR_MAX_STR_LEN -1);
            break;
        case eBTRCoreMediaPlyrEqlzrStOff:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_EQUALIZER_OFF;
            break;
        case eBTRCoreMediaPlyrEqlzrStOn:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_EQUALIZER_ON;
            break;
        case eBTRCoreMediaPlyrShflStOff:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_OFF;
            break;
        case eBTRCoreMediaPlyrShflStAllTracks:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_ALLTRACKS;
            break;
        case eBTRCoreMediaPlyrShflStGroup:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_GROUP;
            break;
        case eBTRCoreMediaPlyrRptStOff:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_OFF;
            break;
        case eBTRCoreMediaPlyrRptStSingleTrack:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_SINGLETRACK;
            break;
        case eBTRCoreMediaPlyrRptStAllTracks:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_ALLTRACKS;
            break;
        case eBTRCoreMediaPlyrRptStGroup:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_GROUP;
            break;
        case eBTRCoreMediaPlyrVolume:
            if ((lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET)  ||
                (lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_HANDSFREE)         ||
                (lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_LOUDSPEAKER)       ||
                (lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_HEADPHONES)        ||
                (lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO)    ||
                (lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_CAR_AUDIO)         ||
                (lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)) {

                gboolean        lbMuted = FALSE;
                unsigned char   lui8Volume = BTRMGR_SO_MAX_VOLUME - 1;

#ifdef RDKTV_PERSIST_VOLUME_SKY
                if (btrMgr_GetLastVolume(0, &lui8Volume) == eBTRMgrSuccess) {
                }

                if (btrMgr_GetLastMuteState(0, &lbMuted) == eBTRMgrSuccess) {
                }

                if (btrMgr_SetLastVolume(0, mediaStatus->m_mediaPlayerVolume) == eBTRMgrSuccess) {
                }
#endif
                lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_MEDIA_STATUS;
                lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevVolume = mediaStatus->m_mediaPlayerVolume;
                lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_ui8mediaDevMute = (unsigned char)lbMuted;
                gui8IsSoDevAvrcpSupported = 1;      // TODO: Find a better way to do this

                if (mediaStatus->m_mediaPlayerVolume > lui8Volume)
                    lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_VOLUMEUP;
                else if (mediaStatus->m_mediaPlayerVolume < lui8Volume)
                    lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_VOLUMEDOWN;
                else
                    lstEventMessage.m_mediaInfo.m_mediaDevStatus.m_enmediaCtrlCmd = BTRMGR_MEDIA_CTRL_UNKNOWN;
            
            }
            else {
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_VOLUME;
                lstEventMessage.m_mediaInfo.m_mediaPlayerVolume = mediaStatus->m_mediaPlayerVolume;
            }
            break;

        case eBTRCoreMediaElementInScope:
            switch (mediaStatus->m_mediaElementInfo.eAVMedElementType) {
            case enBTRCoreMedETypeAlbum:
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_ALBUM_INFO;
                break;
            case enBTRCoreMedETypeArtist:
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_ARTIST_INFO;
                break;
            case enBTRCoreMedETypeGenre:
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_GENRE_INFO;
                break;
            case enBTRCoreMedETypeCompilation:
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_COMPILATION_INFO;
                break;
            case enBTRCoreMedETypePlayList:
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYLIST_INFO;
                break;
            case enBTRCoreMedETypeTrackList:
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACKLIST_INFO;
                break;
            case enBTRCoreMedETypeTrack:
                lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_INFO;
                break;
            default:
                break;
            }

            lstEventMessage.m_mediaInfo.m_mediaTrackListInfo.m_numberOfElements = 1;
            lstEventMessage.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_mediaElementHdl  = mediaStatus->m_mediaElementInfo.ui32MediaElementId;
            
            if ((lstEventMessage.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_IsPlayable = mediaStatus->m_mediaElementInfo.bIsPlayable)) {
                memcpy (&lstEventMessage.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_mediaTrackInfo, &mediaStatus->m_mediaElementInfo.m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
            }
            else {
                strncpy (lstEventMessage.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_mediaElementName, mediaStatus->m_mediaElementInfo.m_mediaElementName, BTRMGR_MAX_STR_LEN -1);
            }
            break;
        case eBTRCoreMediaElementOofScope:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MAX;
            break;

        default:
            break;
        }


        if (gfpcBBTRMgrEventOut) {
            gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
        }


        switch (mediaStatus->eBTMediaStUpdate) {
        case eBTRCoreMediaTrkStStarted:
            break;
        case eBTRCoreMediaTrkStPlaying: 
            break;
        case eBTRCoreMediaTrkStPaused:
            break;
        case eBTRCoreMediaTrkStStopped:
            break;
        case eBTRCoreMediaTrkPosition:
            break;
        case eBTRCoreMediaTrkStChanged:
            break;
        case eBTRCoreMediaPlaybackEnded:
            break;
        case eBTRCoreMediaPlyrName:
            break;
        case eBTRCoreMediaPlyrEqlzrStOff:
            break;
        case eBTRCoreMediaPlyrEqlzrStOn:
            break;
        case eBTRCoreMediaPlyrShflStOff:
            break;
        case eBTRCoreMediaPlyrShflStAllTracks:
            break;
        case eBTRCoreMediaPlyrShflStGroup:
            break;
        case eBTRCoreMediaPlyrRptStOff:
            break;
        case eBTRCoreMediaPlyrRptStSingleTrack:
            break;
        case eBTRCoreMediaPlyrRptStAllTracks:
            break;
        case eBTRCoreMediaPlyrRptStGroup:
            break;
        case eBTRCoreMediaPlyrVolume:
            {
                stBTRMgrMediaStatus lstBtrMgrSiStatus;

                lstBtrMgrSiStatus.eBtrMgrState  = eBTRMgrStateUnknown;
                lstBtrMgrSiStatus.eBtrMgrSFreq  = eBTRMgrSFreqUnknown;
                lstBtrMgrSiStatus.eBtrMgrSFmt   = eBTRMgrSFmtUnknown;
                lstBtrMgrSiStatus.eBtrMgrAChan  = eBTRMgrAChanUnknown;
                lstBtrMgrSiStatus.ui8Volume     = mediaStatus->m_mediaPlayerVolume;

                if (gstBTRMgrStreamingInfo.hBTRMgrSiHdl &&
                    ((lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE) ||
                     (lstEventMessage.m_mediaInfo.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET))) {
                    if (BTRMgr_SI_SetStatus(gstBTRMgrStreamingInfo.hBTRMgrSiHdl, &lstBtrMgrSiStatus) != eBTRMgrSuccess) {
                        BTRMGRLOG_WARN ("BTRMgr_SI_SetStatus FAILED - eBTRCoreMediaPlyrVolume\n");
                    }
                }
            }
            break;
        case eBTRCoreMediaElementInScope:
            break;
        case eBTRCoreMediaElementOofScope:
            break;
        default:
            break;
        }

    }

    return lenBtrCoreRet;
}

/* End of File */
