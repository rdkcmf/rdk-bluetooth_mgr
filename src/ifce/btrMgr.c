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
#include <glib.h>

#include "btrCore.h"

#include "btmgr.h"
#include "btrMgr_logger.h"
#include "rfcapi.h"

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

#define BTRMGR_DISCOVERY_HOLD_OFF_TIME      120

// Move to private header ?
typedef struct _stBTRMgrStreamingInfo {
    tBTRMgrAcHdl    hBTRMgrAcHdl;
    tBTRMgrSoHdl    hBTRMgrSoHdl;
    tBTRMgrSoHdl    hBTRMgrSiHdl;
    unsigned long   bytesWritten;
    unsigned        samplerate;
    unsigned        channels;
    unsigned        bitsPerSample;
} stBTRMgrStreamingInfo;

typedef enum _BTRMGR_DiscoveryState_t {
    BTRMGR_DISCOVERY_ST_UNKNOWN,
    BTRMGR_DISCOVERY_ST_STARTED,
    BTRMGR_DISCOVERY_ST_PAUSED,
    BTRMGR_DISCOVERY_ST_RESUMED,
    BTRMGR_DISCOVERY_ST_STOPPED,
} BTRMGR_DiscoveryState_t;

typedef struct _BTRMGR_DiscoveryHandle_t {
    BTRMGR_DeviceOperationType_t    m_devOpType;
    BTRMGR_DiscoveryState_t         m_disStatus;
    BTRMGR_DiscoveryFilterHandle_t  m_disFilter;
} BTRMGR_DiscoveryHandle_t;

//TODO: Move to a local handle. Mutex protect all
static tBTRCoreHandle               ghBTRCoreHdl                = NULL;
static tBTRMgrPIHdl  		        ghBTRMgrPiHdl               = NULL;
static BTRMgrDeviceHandle           ghBTRMgrDevHdlLastConnected = 0;
static BTRMgrDeviceHandle           ghBTRMgrDevHdlCurStreaming  = 0;
static BTRMGR_DiscoveryHandle_t     ghBTRMgrDiscoveryHdl;
static BTRMGR_DiscoveryHandle_t     ghBTRMgrBgDiscoveryHdl;

static stBTRCoreAdapter                 gDefaultAdapterContext;
static stBTRCoreListAdapters            gListOfAdapters;
static stBTRCoreDevMediaInfo            gstBtrCoreDevMediaInfo;
static BTRMGR_DiscoveredDevicesList_t   gListOfDiscoveredDevices;
static BTRMGR_PairedDevicesList_t       gListOfPairedDevices;
static stBTRMgrStreamingInfo            gstBTRMgrStreamingInfo;

static unsigned char                gIsLeDeviceConnected        = 0;
static unsigned char                gIsAgentActivated           = 0;
static unsigned char                gEventRespReceived          = 0;
static unsigned char                gAcceptConnection           = 0;
static unsigned char                gIsUserInitiated            = 0;
static unsigned char                gIsAudOutStartupInProgress  = 0;
static unsigned char                gDiscHoldOffTimeOutCbData   = 0;
static unsigned char                gIsAudioInEnabled           = 0;
static volatile guint               gTimeOutRef                 = 0;
static volatile unsigned int        gIsAdapterDiscovering       = 0;

static BTRMGR_DeviceOperationType_t gBgDiscoveryType            = BTRMGR_DEVICE_OP_TYPE_UNKNOWN;
static BTRMGR_Events_t              gMediaPlaybackStPrev        = BTRMGR_EVENT_MAX;

static void*                        gpvMainLoop                 = NULL;
static void*                        gpvMainLoopThread           = NULL;

static BTRMGR_EventCallback         gfpcBBTRMgrEventOut         = NULL;

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif



/* Static Function Prototypes */
static inline unsigned char btrMgr_GetAdapterCnt (void);
static const char* btrMgr_GetAdapterPath (unsigned char aui8AdapterIdx);

static inline void btrMgr_SetAgentActivated (unsigned char aui8AgentActivated);
static inline unsigned char btrMgr_GetAgentActivated (void);
static void btrMgr_CheckAudioInServiceAvailability (void);

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

static BTRMGR_DeviceType_t btrMgr_MapDeviceTypeFromCore (enBTRCoreDeviceClass device_type);
static BTRMGR_DeviceOperationType_t btrMgr_MapDeviceOpFromDeviceType (BTRMGR_DeviceType_t device_type);
static BTRMGR_RSSIValue_t btrMgr_MapSignalStrengthToRSSI (int signalStrength);
static eBTRMgrRet btrMgr_MapDevstatusInfoToEventInfo (void* p_StatusCB, BTRMGR_EventMessage_t* apstEventMessage, BTRMGR_Events_t type);

static eBTRMgrRet btrMgr_StartCastingAudio (int outFileFd, int outMTUSize, eBTRCoreDevMediaType aenBtrCoreDevOutMType, void* apstBtrCoreDevOutMCodecInfo);
static eBTRMgrRet btrMgr_StopCastingAudio (void);
static eBTRMgrRet btrMgr_StartReceivingAudio (int inFileFd, int inMTUSize, eBTRCoreDevMediaType aenBtrCoreDevInMType, void* apstBtrCoreDevInMCodecInfo);
static eBTRMgrRet btrMgr_StopReceivingAudio (void);

static eBTRMgrRet btrMgr_ConnectToDevice (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t connectAs, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx);

static eBTRMgrRet btrMgr_StartAudioStreamingOut (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t streamOutPref, unsigned int aui32ConnectRetryIdx, unsigned int aui32ConfirmIdx, unsigned int aui32SleepIdx);

static eBTRMgrRet btrMgr_AddPersistentEntry(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, const char* apui8ProfileStr, int ai32DevConnected);
static eBTRMgrRet btrMgr_RemovePersistentEntry(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, const char* apui8ProfileStr);


/*  Local Op Threads Prototypes */
static gpointer btrMgr_g_main_loop_Task (gpointer appvMainLoop);


/* Incoming Callbacks Prototypes */
static gboolean btrMgr_DiscoveryHoldOffTimerCb (gpointer gptr);

static eBTRMgrRet btrMgr_ACDataReadyCb (void* apvAcDataBuf, unsigned int aui32AcDataLen, void* apvUserData);
static eBTRMgrRet btrMgr_SOStatusCb (stBTRMgrMediaStatus* apstBtrMgrSoStatus, void* apvUserData);
static eBTRMgrRet btrMgr_SIStatusCb (stBTRMgrMediaStatus* apstBtrMgrSiStatus, void* apvUserData);

static enBTRCoreRet btrMgr_DeviceStatusCb (stBTRCoreDevStatusCBInfo* p_StatusCB, void* apvUserData);
static enBTRCoreRet btrMgr_DeviceDiscoveryCb (stBTRCoreDiscoveryCBInfo* astBTRCoreDiscoveryCbInfo, void* apvUserData);
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

        if (BTRMGR_RESULT_SUCCESS == BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl))) {

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
        if (BTRMGR_RESULT_SUCCESS == BTRMGR_StartDeviceDiscovery (aui8AdapterIdx, btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl))) {

			//TODO: Move before you make the call to StartDeviceDiscovery, store the previous state and restore the previous state in case of Failure
            btrMgr_SetDiscoveryState (ahdiscoveryHdl, BTRMGR_DISCOVERY_ST_RESUMED);
            BTRMGRLOG_DEBUG ("[%s] Successfully Resumed Scan\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
        } else {
            BTRMGRLOG_ERROR ("[%s] Failed Resume Scan!!!\n"
                            , btrMgr_GetDiscoveryDeviceTypeAsString (btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl)));
            lenBtrMgrRet =  eBTRMgrFailure;
        }
    //}

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

        if (BTRMGR_RESULT_SUCCESS == BTRMGR_StopDeviceDiscovery (aui8AdapterIdx, btrMgr_GetDiscoveryDeviceType(ahdiscoveryHdl))) {

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
    else {
        apstEventMessage->m_pairedDevice.m_deviceHandle          = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceId;
        apstEventMessage->m_pairedDevice.m_deviceType            = btrMgr_MapDeviceTypeFromCore(((stBTRCoreDevStatusCBInfo*)p_StatusCB)->eDeviceClass);
        apstEventMessage->m_pairedDevice.m_isLastConnectedDevice = (ghBTRMgrDevHdlLastConnected == apstEventMessage->m_pairedDevice.m_deviceHandle) ? 1 : 0;
        apstEventMessage->m_pairedDevice.m_isLowEnergyDevice     = (apstEventMessage->m_pairedDevice.m_deviceType==BTRMGR_DEVICE_TYPE_TILE)?1:0;//We shall make it generic later
        apstEventMessage->m_pairedDevice.m_ui32DevClassBtSpec    = ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->ui32DevClassBtSpec;
        strncpy(apstEventMessage->m_pairedDevice.m_name, ((stBTRCoreDevStatusCBInfo*)p_StatusCB)->deviceName, BTRMGR_NAME_LEN_MAX - 1);
        strncpy(apstEventMessage->m_pairedDevice.m_deviceAddress, "TO BE FILLED", BTRMGR_NAME_LEN_MAX - 1);
        // Need to add devAddress in stBTRCoreDevStatusCBInfo ?
    }


    return lenBtrMgrRet;
}

static eBTRMgrRet
btrMgr_StartCastingAudio (
    int                     outFileFd, 
    int                     outMTUSize,
    eBTRCoreDevMediaType    aenBtrCoreDevOutMType,
    void*                   apstBtrCoreDevOutMCodecInfo
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
    int                  inFileFd,
    int                  inMTUSize,
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
    eBTRMgrRet      lenBtrMgrRet    = eBTRMgrSuccess;
    enBTRCoreRet    lenBtrCoreRet   = enBTRCoreSuccess;
    unsigned int    ui32retryIdx    = aui32ConnectRetryIdx + 1;
    enBTRCoreDeviceType lenBTRCoreDeviceType  = enBTRCoreUnknown;

    lenBtrMgrRet = btrMgr_PreCheckDiscoveryStatus(aui8AdapterIdx, connectAs);

    if (eBTRMgrSuccess != lenBtrMgrRet) {
        BTRMGRLOG_ERROR ("Pre Check Discovery State Rejected !!!\n");
        return lenBtrMgrRet;
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
            BTRMGRLOG_INFO ("Connected Successfully - %llu\n", ahBTRMgrDevHdl);
            lenBtrMgrRet = eBTRMgrSuccess;
        }

        if (lenBtrMgrRet != eBTRMgrFailure) {
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
                BTRMGRLOG_ERROR ("Failed to Connect to this device - Confirmed - %llu\n", ahBTRMgrDevHdl);
                lenBtrMgrRet = eBTRMgrFailure;

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
                    if (ghBTRMgrDevHdlLastConnected && ghBTRMgrDevHdlLastConnected != ahBTRMgrDevHdl) {
                       BTRMGRLOG_DEBUG ("Remove persistent entry for previously connected device(%llu)\n", ghBTRMgrDevHdlLastConnected);

                        if ((lenBTRCoreDeviceType == enBTRCoreSpeakers) || (lenBTRCoreDeviceType == enBTRCoreHeadSet)) {
                            btrMgr_RemovePersistentEntry(aui8AdapterIdx, ghBTRMgrDevHdlLastConnected, BTRMGR_A2DP_SINK_PROFILE_ID);
                        }
                        else if ((lenBTRCoreDeviceType == enBTRCoreMobileAudioIn) || (lenBTRCoreDeviceType == enBTRCorePCAudioIn)) {
                            btrMgr_RemovePersistentEntry(aui8AdapterIdx, ghBTRMgrDevHdlLastConnected, BTRMGR_A2DP_SRC_PROFILE_ID);
                        }
                    }

                    if ((lenBTRCoreDeviceType == enBTRCoreSpeakers) || (lenBTRCoreDeviceType == enBTRCoreHeadSet)) {
                        btrMgr_AddPersistentEntry (aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SINK_PROFILE_ID, 1);
                    }
                    else if ((lenBTRCoreDeviceType == enBTRCoreMobileAudioIn) || (lenBTRCoreDeviceType == enBTRCorePCAudioIn)) {
                        btrMgr_AddPersistentEntry (aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SRC_PROFILE_ID, 1);
                    }

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

    *pLimited =  (unsigned char)((!strncasecmp(BeaconPersistentData.limitBeaconDetection, "true", 4)) ? 1 : 0 );
    if (pLimited != NULL) {
        BTRMGRLOG_INFO ("the beacon detection detection : %s\n", *pLimited? "true":"false");
    }
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
                lenBtrCoreRet = BTRCore_AcquireDeviceDataPath(ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, enBTRCoreSpeakers, &deviceFD, &deviceReadMTU, &deviceWriteMTU);
                if ((lenBtrCoreRet == enBTRCoreSuccess) && deviceWriteMTU) {
                    /* Now that you got the FD & Read/Write MTU, start casting the audio */
                    if ((lenBtrMgrRet = btrMgr_StartCastingAudio(deviceFD, deviceWriteMTU, lenBtrCoreDevOutMType, lpstBtrCoreDevOutMCodecInfo)) == eBTRMgrSuccess) {
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
    strncpy(btPtofile.adapterId, lui8adapterAddr, BTRMGR_NAME_LEN_MAX);
    strncpy(btPtofile.profileId, apui8ProfileStr, BTRMGR_NAME_LEN_MAX);
    btPtofile.deviceId  = ahBTRMgrDevHdl;
    btPtofile.isConnect = ai32DevConnected;

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
    BTRMgrDeviceHandle  ahBTRMgrDevHdl,
    const char*         apui8ProfileStr
) {
    char         lui8adapterAddr[BD_NAME_LEN] = {'\0'};
    eBTRMgrRet   lenBtrMgrPiRet = eBTRMgrFailure;

    BTRCore_GetAdapterAddr(ghBTRCoreHdl, aui8AdapterIdx, lui8adapterAddr);

    // Device disconnected remove data from json file
    BTRMGR_Profile_t btPtofile;
    strncpy(btPtofile.adapterId, lui8adapterAddr, BTRMGR_NAME_LEN_MAX);
    strncpy(btPtofile.profileId, apui8ProfileStr, BTRMGR_NAME_LEN_MAX);
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
    BTRMGR_Result_t lenBtrMgrResult= BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet 	lenBtrCoreRet  = enBTRCoreSuccess;
    eBTRMgrRet      lenBtrMgrPiRet = eBTRMgrFailure;
    GMainLoop*      pMainLoop      = NULL;
    GThread*        pMainLoopThread= NULL;

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

    btrMgr_SetBgDiscoveryType (BTRMGR_DEVICE_OP_TYPE_LE);

    /* Initialize the Paired Device List for Default adapter */
    BTRMGR_GetPairedDevices (gDefaultAdapterContext.adapter_number, &gListOfPairedDevices);


    // Init Persistent handles
    lenBtrMgrPiRet = BTRMgr_PI_Init(&ghBTRMgrPiHdl);
    if(lenBtrMgrPiRet != eBTRMgrSuccess) {
        BTRMGRLOG_ERROR ("Could not initialize PI module\n");
    }
    btrMgr_CheckAudioInServiceAvailability();
      
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
    eBTRMgrRet      lenBtrMgrPiResult = eBTRMgrSuccess;
    enBTRCoreRet 	lenBtrCoreRet     = enBTRCoreSuccess;
    BTRMGR_Result_t lenBtrMgrResult   = BTRMGR_RESULT_SUCCESS;


    if (btrMgr_isTimeOutSet()) {
        btrMgr_ClearDiscoveryHoldOffTimer();
        gDiscHoldOffTimeOutCbData = 0;
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
    strncpy (pNameOfAdapter, name, (BTRMGR_NAME_LEN_MAX - 1));


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

BTRMGR_Result_t
BTRMGR_StartDeviceDiscovery (
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
            BTRMGRLOG_WARN ("[%s] Scan already in Progress..."
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


BTRMGR_Result_t
BTRMGR_StopDeviceDiscovery (
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
BTRMGR_GetDiscoveryStatus (
        unsigned char                aui8AdapterIdx, 
        BTRMGR_DiscoveryStatus_t     *isDiscoveryInProgress,
        BTRMGR_DeviceOperationType_t *aenBTRMgrDevOpT
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
        BTRMGRLOG_WARN ("[%s] Scan already in Progress. "
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
    BTRMGR_Result_t                 lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                    lenBtrCoreRet   = enBTRCoreSuccess;
    stBTRCoreScannedDevicesCount    lstBtrCoreListOfSDevices;
    int i = 0;

    if (!ghBTRCoreHdl) {
        BTRMGRLOG_ERROR ("BTRCore is not Inited\n");
        return BTRMGR_RESULT_INIT_FAILED;
    }

    if ((aui8AdapterIdx > btrMgr_GetAdapterCnt()) || (!pDiscoveredDevices)) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return BTRMGR_RESULT_INVALID_INPUT;
    }


    memset (&lstBtrCoreListOfSDevices, 0, sizeof(lstBtrCoreListOfSDevices));

    lenBtrCoreRet = BTRCore_GetListOfScannedDevices(ghBTRCoreHdl, &lstBtrCoreListOfSDevices);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get list of discovered devices\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        memset (pDiscoveredDevices, 0, sizeof(BTRMGR_DiscoveredDevicesList_t));
        memset (&gListOfDiscoveredDevices, 0, sizeof(gListOfDiscoveredDevices));
        if (lstBtrCoreListOfSDevices.numberOfDevices) {
            BTRMGR_DiscoveredDevices_t *lpstBtrMgrSDevice = NULL;

            pDiscoveredDevices->m_numOfDevices = (lstBtrCoreListOfSDevices.numberOfDevices < BTRMGR_DEVICE_COUNT_MAX) ? 
                                                    lstBtrCoreListOfSDevices.numberOfDevices : BTRMGR_DEVICE_COUNT_MAX;

            for (i = 0; i < pDiscoveredDevices->m_numOfDevices; i++) {
                lpstBtrMgrSDevice = &pDiscoveredDevices->m_deviceProperty[i];

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
            }
            /*  Success */
            BTRMGRLOG_INFO ("Successful\n");
            /* Update BTRMgr DiscoveredDev List cache */
            memcpy (&gListOfDiscoveredDevices, pDiscoveredDevices, sizeof(BTRMGR_DiscoveredDevicesList_t));
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
        BTRMGRLOG_INFO ("Paired Successfully\n");
        lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
        lBtMgrOutEvent  = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
    }

    /* The Paring is not a blocking call for HID. So we can sent the pairing complete msg when paring actually completed and gets a notification.
     * But the pairing failed, we must send it right here. */
    if ((lenBtrMgrResult != BTRMGR_RESULT_SUCCESS) || ((lenBtrMgrResult == BTRMGR_RESULT_SUCCESS) && (lenBTRCoreDevTy != enBTRCoreHID))) {
        BTRMGR_EventMessage_t  lstEventMessage;
        memset (&lstEventMessage, 0, sizeof(lstEventMessage));
        btrMgr_GetDiscoveredDevInfo (ahBTRMgrDevHdl, &lstEventMessage.m_discoveredDevice);
        lstEventMessage.m_adapterIndex = aui8AdapterIdx;
        lstEventMessage.m_eventType    = lBtMgrOutEvent;
        
        if (gfpcBBTRMgrEventOut) {
            gfpcBBTRMgrEventOut(lstEventMessage);
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
        BTRMGRLOG_INFO ("Unpaired Successfully\n");
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


    memset (&lstBtrCoreListOfPDevices, 0, sizeof(lstBtrCoreListOfPDevices));

    lenBtrCoreRet = BTRCore_GetListOfPairedDevices(ghBTRCoreHdl, &lstBtrCoreListOfPDevices);
    if (lenBtrCoreRet != enBTRCoreSuccess) {
        BTRMGRLOG_ERROR ("Failed to get list of paired devices\n");
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else {
        memset (pPairedDevices, 0, sizeof(BTRMGR_PairedDevicesList_t));     /* Reset the values to 0 */
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
                lenBtrCoreRet = BTRCore_GetDeviceDisconnected(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBTRCoreDevTy);
            } while ((lenBtrCoreRet != enBTRCoreSuccess) && (--ui32sleepIdx));
        } while (--ui32confirmIdx);

        if (lenBtrCoreRet != enBTRCoreSuccess) {
            BTRMGRLOG_ERROR ("Failed to Disconnect from this device - Confirmed\n");
            lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        }
        else {
            BTRMGRLOG_DEBUG ("Success Disconnect from this device - Confirmed\n");

            if (lenBTRCoreDevTy == enBTRCoreLE) {
                gIsLeDeviceConnected = 0;
            }
            else if ((lenBTRCoreDevTy == enBTRCoreSpeakers) || (lenBTRCoreDevTy == enBTRCoreHeadSet)) {
                btrMgr_RemovePersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SINK_PROFILE_ID);
                btrMgr_SetDevConnected(ahBTRMgrDevHdl, 0);
                ghBTRMgrDevHdlLastConnected = 0;
            }
            else if ((lenBTRCoreDevTy == enBTRCoreMobileAudioIn) || (lenBTRCoreDevTy == enBTRCorePCAudioIn)) {
                btrMgr_RemovePersistentEntry(aui8AdapterIdx, ahBTRMgrDevHdl, BTRMGR_A2DP_SRC_PROFILE_ID);
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

                   lpstBtrMgrPDevice->m_isConnected  = 1;
                   lpstBtrMgrPDevice->m_deviceHandle = lstBtrCoreListOfPDevices.devices[i].tDeviceId;
                   lpstBtrMgrPDevice->m_vendorID     = lstBtrCoreListOfPDevices.devices[i].ui32VendorId;
                   lpstBtrMgrPDevice->m_deviceType   = btrMgr_MapDeviceTypeFromCore(lstBtrCoreListOfPDevices.devices[i].enDeviceType);

                   strncpy (lpstBtrMgrPDevice->m_name,          lstBtrCoreListOfPDevices.devices[i].pcDeviceName,   (BTRMGR_NAME_LEN_MAX - 1));
                   strncpy (lpstBtrMgrPDevice->m_deviceAddress, lstBtrCoreListOfPDevices.devices[i].pcDeviceAddress,(BTRMGR_NAME_LEN_MAX - 1));

                   lpstBtrMgrPDevice->m_serviceInfo.m_numOfService = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService;
                   for (j = 0; j < lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.numberOfService; j++) {
                       lpstBtrMgrPDevice->m_serviceInfo.m_profileInfo[j].m_uuid = lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].uuid_value;
                       strncpy (lpstBtrMgrPDevice->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
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
                        strncpy (lpstBtrMgrSDevice->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
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
                        strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfPDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
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
                            strncpy (pDeviceProperty->m_serviceInfo.m_profileInfo[j].m_profile, lstBtrCoreListOfSDevices.devices[i].stDeviceProfile.profile[j].profile_name, BTRMGR_NAME_LEN_MAX);
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


    if (BTRMgr_PI_GetAllProfiles(ghBTRMgrPiHdl, &lstPersistentData) == eBTRMgrFailure) {
        btrMgr_AddPersistentEntry (aui8AdapterIdx, 0, BTRMGR_A2DP_SINK_PROFILE_ID, isConnected);
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

                if (isConnected && lDeviceHandle) {
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


    if (!listOfPDevices.devices[i].bDeviceConnected) {
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
    lenBtrCoreRet = BTRCore_AcquireDeviceDataPath (ghBTRCoreHdl, listOfPDevices.devices[i].tDeviceId, lenBtrCoreDevType, &i32DeviceFD, &i32DeviceReadMTU, &i32DeviceWriteMTU);
    if (lenBtrCoreRet == enBTRCoreSuccess) {
        if ((lenBtrMgrRet = btrMgr_StartReceivingAudio(i32DeviceFD, i32DeviceReadMTU, lenBtrCoreDevInMType, lpstBtrCoreDevInMCodecInfo)) == eBTRMgrSuccess) {
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
    BTRMGR_Result_t                 lenBtrMgrResult     = BTRMGR_RESULT_SUCCESS;
    enBTRCoreRet                    lenBtrCoreRet       = enBTRCoreSuccess;
    enBTRCoreDeviceType             lenBtrCoreDevTy     = enBTRCoreUnknown;
    enBTRCoreDeviceClass            lenBtrCoreDevCl     = enBTRCore_DC_Unknown;
    enBTRCoreMediaCtrl              aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlUnknown;


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
    case BTRMGR_MEDIA_CTRL_EQUALIZER_OFF:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlEqlzrOff;
        break;
    case BTRMGR_MEDIA_CTRL_EQUALIZER_ON:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlEqlzrOn;
        break;
    case BTRMGR_MEDIA_CTRL_SHUFFLE_OFF:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlShflOff;
        break;
    case BTRMGR_MEDIA_CTRL_SHUFFLE_ALLTRACKS:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlShflAllTracks;
        break;
    case BTRMGR_MEDIA_CTRL_SHUFFLE_GROUP:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlShflGroup;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_OFF:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptOff;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_SINGLETRACK:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptSingleTrack;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_ALLTRACKS:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptAllTracks;
        break;
    case BTRMGR_MEDIA_CTRL_REPEAT_GROUP:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlRptGroup;
        break;
    default:
        aenBTRCoreMediaCtrl = enBTRCoreMediaCtrlUnknown;
    }

    if (aenBTRCoreMediaCtrl == enBTRCoreMediaCtrlUnknown) {
        BTRMGRLOG_ERROR ("Media Control Command for %llu Unknown!!!\n", ahBTRMgrDevHdl);
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
    }
    else
    if (enBTRCoreSuccess != BTRCore_MediaControl(ghBTRCoreHdl, ahBTRMgrDevHdl, lenBtrCoreDevTy, aenBTRCoreMediaCtrl)) {
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
    if (enBTRCoreSuccess != BTRCore_GetMediaElementList (ghBTRCoreHdl,
                                                         ahBTRMgrDevHdl,
                                                         ahBTRMgrMedElementHdl,
                                                         aui16MediaElementStartIdx,
                                                         aui16MediaElementEndIdx,
                                                         lenBtrCoreDevTy,
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
    BTRMGR_GetConnectedDevices (aui8AdapterIdx, &listOfCDevices);
    for ( ;ui16LoopIdx < listOfCDevices.m_numOfDevices; ui16LoopIdx++) {
        if (listOfCDevices.m_deviceProperty[ui16LoopIdx].m_deviceHandle == ahBTRMgrDevHdl) {
            isConnected = listOfCDevices.m_deviceProperty[ui16LoopIdx].m_isConnected;
            break;
        }
    }

    if (!isConnected) {
       BTRMGRLOG_ERROR ("LE Device %lld is not connected to perform LE Op!!!\n", ahBTRMgrDevHdl);
       return BTRMGR_RESULT_GENERIC_FAILURE;
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
    else if (type == BTRMGR_DEVICE_TYPE_HID)
        return "HUMAN INTERFACE DEVICE";
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
        BTRMGRLOG_WARN ("CB context received NULL arg!");
    }

    gTimeOutRef = 0;

    if (btrMgr_PostCheckDiscoveryStatus (lui8AdapterIdx, BTRMGR_DEVICE_OP_TYPE_UNKNOWN) != eBTRMgrSuccess) {
	BTRMGRLOG_ERROR ("Post Check Disc Status Failed!\n");
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

    if (!gIsAudioInEnabled &&
        (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
        lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET)) {
        BTRMGRLOG_DEBUG ("Rejecting status callback - BTR AudioIn is currently Disabled!\n");
        return lenBtrCoreRet;
    }

    BTRMGRLOG_INFO ("Received status callback\n");

    if (p_StatusCB) {

        switch (p_StatusCB->eDeviceCurrState) {
        case enBTRCoreDevStPaired:
            /* Post this event only for HID Devices */
            if (p_StatusCB->eDeviceType == enBTRCoreHID) {
                btrMgr_GetDiscoveredDevInfo (p_StatusCB->deviceId, &lstEventMessage.m_discoveredDevice);
                lstEventMessage.m_eventType = BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE;
                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);  /* Post a callback */
                }
            }
            break;
        case enBTRCoreDevStInitialized:
            break;
        case enBTRCoreDevStConnecting:
            break;
        case enBTRCoreDevStConnected:               /*  notify user device back   */
            if (enBTRCoreDevStLost == p_StatusCB->eDevicePrevState || enBTRCoreDevStPaired == p_StatusCB->eDevicePrevState) {
                if (!gIsAudOutStartupInProgress) {
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
                    else if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID) {
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

                if ((lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HANDSFREE) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_LOUDSPEAKER) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HEADPHONES) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_CAR_AUDIO) &&
                    (lstEventMessage.m_pairedDevice.m_deviceType != BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)) {

                    /* Update the flag as the Device is Connected */
                    if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID) {
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

                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
                }

                BTRMGRLOG_INFO ("lstEventMessage.m_pairedDevice.m_deviceType = %d\n", lstEventMessage.m_pairedDevice.m_deviceType);
                if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TILE) {
                    /* update the flags as the LE device is NOT Connected */
                    gIsLeDeviceConnected = 0;
                }
                else if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_HID) {
                    btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 0);
                }
                else if ((ghBTRMgrDevHdlCurStreaming != 0) && (ghBTRMgrDevHdlCurStreaming == p_StatusCB->deviceId)) {
                    /* update the flags as the device is NOT Connected */
                    btrMgr_SetDevConnected(lstEventMessage.m_pairedDevice.m_deviceHandle, 0);

                    if (lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_SMARTPHONE ||
                        lstEventMessage.m_pairedDevice.m_deviceType == BTRMGR_DEVICE_TYPE_TABLET) {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        BTRMGR_StopAudioStreamingIn(0, ghBTRMgrDevHdlCurStreaming);
                        ghBTRMgrDevHdlLastConnected = 0;
                    }
                    else {
                        /* Stop the playback which already stopped internally but to free up the memory */
                        BTRMGR_StopAudioStreamingOut(0, ghBTRMgrDevHdlCurStreaming);
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
                            BTRMGR_StopAudioStreamingOut (0, ghBTRMgrDevHdlCurStreaming);
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
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OP_READY);
                //Not a big fan of devOpResponse. We should think of a better way to do this
                strncpy(lstEventMessage.m_deviceOpInfo.m_notifyData, p_StatusCB->devOpResponse, BTRMGR_MAX_STR_LEN-1);

                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
                }
            }
            break;
        case enBTRCoreDevStOpInfo:
            if (btrMgr_MapDeviceTypeFromCore(p_StatusCB->eDeviceClass) == BTRMGR_DEVICE_TYPE_TILE) {
                btrMgr_MapDevstatusInfoToEventInfo ((void*)p_StatusCB, &lstEventMessage, BTRMGR_EVENT_DEVICE_OP_INFORMATION);
                //Not a big fan of devOpResponse. We should think of a better way to do this
                strncpy(lstEventMessage.m_deviceOpInfo.m_notifyData, p_StatusCB->devOpResponse, BTRMGR_MAX_STR_LEN-1);

                if (gfpcBBTRMgrEventOut) {
                    gfpcBBTRMgrEventOut(lstEventMessage);    /* Post a callback */
                }
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
        if (btrMgr_GetDiscoveryInProgress() || (astBTRCoreDiscoveryCbInfo->device.bFound == FALSE)) { /* Not a big fan of this */
            BTRMGR_EventMessage_t lstEventMessage;
            memset (&lstEventMessage, 0, sizeof(lstEventMessage));

            btrMgr_MapDevstatusInfoToEventInfo ((void*)&astBTRCoreDiscoveryCbInfo->device, &lstEventMessage, BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE);

            if (gfpcBBTRMgrEventOut) {
                gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
            }
        }
    }
    else if (astBTRCoreDiscoveryCbInfo->type == enBTRCoreOpTypeAdapter)
    {
        BTRMGRLOG_INFO ("adapter number = %d, discoverable = %d, discovering = %d\n",
                astBTRCoreDiscoveryCbInfo->adapter.adapter_number,
                astBTRCoreDiscoveryCbInfo->adapter.discoverable,
                astBTRCoreDiscoveryCbInfo->adapter.bDiscovering);

        gIsAdapterDiscovering = astBTRCoreDiscoveryCbInfo->adapter.bDiscovering;

        if (gfpcBBTRMgrEventOut) {
            BTRMGR_EventMessage_t lstEventMessage;
            memset (&lstEventMessage, 0, sizeof(lstEventMessage));

            lstEventMessage.m_adapterIndex = astBTRCoreDiscoveryCbInfo->adapter.adapter_number;
            if (astBTRCoreDiscoveryCbInfo->adapter.bDiscovering)
            {
                lstEventMessage.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED;
            }
            else
            {
                lstEventMessage.m_eventType    = BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE;
            }

            gfpcBBTRMgrEventOut(lstEventMessage); /*  Post a callback */
        }
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

    if (BTRMGR_DEVICE_TYPE_HID != (lBtrMgrDevType = btrMgr_MapDeviceTypeFromCore(apstConnCbInfo->stFoundDevice.enDeviceType))) {
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
    if (BTRMGR_DEVICE_TYPE_HID == lBtrMgrDevType) {
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
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HIFIAudioDevice)   ||
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_Headphones)) {

        BTRMGRLOG_WARN ("Incoming Connection from BT Speaker/Headset\n");
        if (btrMgr_GetDevPaired(apstConnCbInfo->stKnownDevice.tDeviceId) && (apstConnCbInfo->stKnownDevice.tDeviceId == ghBTRMgrDevHdlLastConnected)) {
            BTRMGR_EventMessage_t lstEventMessage;

            BTRMGRLOG_DEBUG ("Paired - Last Connected device...\n");

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
             (apstConnCbInfo->stKnownDevice.enDeviceType == enBTRCore_DC_HID_Joystick)      ){

        BTRMGRLOG_WARN ("Incoming Connection from BT Keyboard\n");
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
        strncpy (lstEventMessage.m_mediaInfo.m_name, mediaStatusCB->deviceName, BTRMGR_NAME_LEN_MAX);

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
            memcpy(&lstEventMessage.m_mediaInfo.m_mediaPositionInfo, &mediaStatus->m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
            break;
        case eBTRCoreMediaTrkStChanged:
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_TRACK_CHANGED;
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
            lstEventMessage.m_eventType = BTRMGR_EVENT_MEDIA_PLAYER_VOLUME;
            lstEventMessage.m_mediaInfo.m_mediaPlayerVolumeInPercentage = mediaStatus->m_mediaPlayerVolumePercentage;
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
                lstBtrMgrSiStatus.ui8Volume     = mediaStatus->m_mediaPlayerVolumePercentage;

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
