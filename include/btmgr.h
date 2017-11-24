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
#ifndef __BTR_MGR_H__
#define __BTR_MGR_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define BTRMGR_MAX_STR_LEN             256
#define BTRMGR_NAME_LEN_MAX            64
#define BTRMGR_STR_LEN                 32
#define BTRMGR_DEVICE_COUNT_MAX        32
#define BTRMGR_ADAPTER_COUNT_MAX       16
#define BTRMGR_MAX_DEVICE_PROFILE      32

typedef unsigned long long int BTRMgrDeviceHandle;

typedef enum _BTRMGR_Result_t {
    BTRMGR_RESULT_SUCCESS = 0,
    BTRMGR_RESULT_GENERIC_FAILURE = -1,
    BTRMGR_RESULT_INVALID_INPUT = -2,
    BTRMGR_RESULT_INIT_FAILED = -3
} BTRMGR_Result_t;

typedef enum _BTRMGR_Events_t {
    BTRMGR_EVENT_DEVICE_OUT_OF_RANGE = 100,
    BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE,
    BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE,
    BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE,
    BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE,
    BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE,
    BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE,
    BTRMGR_EVENT_DEVICE_PAIRING_FAILED,
    BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED,
    BTRMGR_EVENT_DEVICE_CONNECTION_FAILED,
    BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED,
    BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST,
    BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST,
    BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST,
    BTRMGR_EVENT_DEVICE_FOUND,
    BTRMGR_EVENT_MEDIA_TRACK_STARTED,
    BTRMGR_EVENT_MEDIA_TRACK_PLAYING,
    BTRMGR_EVENT_MEDIA_TRACK_PAUSED,
    BTRMGR_EVENT_MEDIA_TRACK_STOPPED,
    BTRMGR_EVENT_MEDIA_TRACK_POSITION,
    BTRMGR_EVENT_MEDIA_TRACK_CHANGED,
    BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED,
    BTRMGR_EVENT_MAX
} BTRMGR_Events_t;

typedef enum _BTRMGR_DeviceType_t {
    BTRMGR_DEVICE_TYPE_UNKNOWN,
    BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET,
    BTRMGR_DEVICE_TYPE_HANDSFREE,
    BTRMGR_DEVICE_TYPE_RESERVED,
    BTRMGR_DEVICE_TYPE_MICROPHONE,
    BTRMGR_DEVICE_TYPE_LOUDSPEAKER,
    BTRMGR_DEVICE_TYPE_HEADPHONES,
    BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO,
    BTRMGR_DEVICE_TYPE_CAR_AUDIO,
    BTRMGR_DEVICE_TYPE_STB,
    BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE,
    BTRMGR_DEVICE_TYPE_VCR,
    BTRMGR_DEVICE_TYPE_VIDEO_CAMERA,
    BTRMGR_DEVICE_TYPE_CAMCODER,
    BTRMGR_DEVICE_TYPE_VIDEO_MONITOR,
    BTRMGR_DEVICE_TYPE_TV,
    BTRMGR_DEVICE_TYPE_VIDEO_CONFERENCE,
    BTRMGR_DEVICE_TYPE_SMARTPHONE,
    BTRMGR_DEVICE_TYPE_END
} BTRMGR_DeviceType_t;

typedef enum _BTRMGR_StreamOut_Type_t {
    BTRMGR_STREAM_PRIMARY = 0,
    BTRMGR_STREAM_SECONDARY,
} BTRMGR_StreamOut_Type_t;

typedef enum _BTRMGR_DeviceConnect_Type_t {
    BTRMGR_DEVICE_TYPE_AUDIOSINK     = 1 << 0,
    BTRMGR_DEVICE_TYPE_HEADSET       = 1 << 1,
    BTRMGR_DEVICE_TYPE_AUDIOSRC      = 1 << 2,
    BTRMGR_DEVICE_TYPE_OTHER         = 1 << 3,
} BTRMGR_DeviceConnect_Type_t;

typedef enum _BTRMGR_DevicePower_t {
    BTRMGR_DEVICE_POWER_ACTIVE = 0,
    BTRMGR_DEVICE_POWER_LOW,
    BTRMGR_DEVICE_POWER_STANDBY
} BTRMGR_DevicePower_t;

typedef enum _BTRMGR_RSSIValue_type_t {
    BTRMGR_RSSI_NONE = 0,      //!< No signal (0 bar)
    BTRMGR_RSSI_POOR,          //!< Poor (1 bar)
    BTRMGR_RSSI_FAIR,          //!< Fair (2 bars)
    BTRMGR_RSSI_GOOD,          //!< Good (3 bars)
    BTRMGR_RSSI_EXCELLENT      //!< Excellent (4 bars)
} BTRMGR_RSSIValue_t;

typedef enum _BTRMGR_MediaControlCommand_t {
    BTRMGR_MEDIA_CTRL_PLAY,
    BTRMGR_MEDIA_CTRL_PAUSE,
    BTRMGR_MEDIA_CTRL_STOP,
    BTRMGR_MEDIA_CTRL_NEXT,
    BTRMGR_MEDIA_CTRL_PREVIOUS,
    BTRMGR_MEDIA_CTRL_FASTFORWARD,
    BTRMGR_MEDIA_CTRL_REWIND,
    BTRMGR_MEDIA_CTRL_VOLUMEUP,
    BTRMGR_MEDIA_CTRL_VOLUMEDOWN
} BTRMGR_MediaControlCommand_t;

typedef struct _BTRMGR_MediaTrackInfo_t {
    char            pcAlbum[BTRMGR_MAX_STR_LEN];
    char            pcGenre[BTRMGR_MAX_STR_LEN];
    char            pcTitle[BTRMGR_MAX_STR_LEN];
    char            pcArtist[BTRMGR_MAX_STR_LEN];
    unsigned int    ui32TrackNumber;
    unsigned int    ui32Duration;
    unsigned int    ui32NumberOfTracks;
} BTRMGR_MediaTrackInfo_t;

typedef struct _BTRMGR_MediaPositionInfo_t {
    unsigned int          m_mediaDuration;
    unsigned int          m_mediaPosition;
} BTRMGR_MediaPositionInfo_t;

typedef struct _BTRMGR_DeviceService_t {
    unsigned short  m_uuid;
    char            m_profile[BTRMGR_NAME_LEN_MAX];
} BTRMGR_DeviceService_t;

typedef struct _BTRMGR_DeviceServiceList_t {
    unsigned short          m_numOfService;
    BTRMGR_DeviceService_t  m_profileInfo[BTRMGR_MAX_DEVICE_PROFILE];
} BTRMGR_DeviceServiceList_t;

typedef struct _BTRMGR_DevicesProperty_t {
    BTRMgrDeviceHandle          m_deviceHandle;
    BTRMGR_DeviceType_t         m_deviceType;
    char                        m_name [BTRMGR_NAME_LEN_MAX];
    char                        m_deviceAddress [BTRMGR_NAME_LEN_MAX];
    BTRMGR_RSSIValue_t          m_rssi;
    int                         m_signalLevel;
    unsigned short              m_vendorID;
    unsigned char               m_isPaired;
    unsigned char               m_isConnected; /* This must be used only when m_isPaired is TRUE */
    unsigned char               m_isLowEnergyDevice;
    BTRMGR_DeviceServiceList_t  m_serviceInfo;
} BTRMGR_DevicesProperty_t;

typedef struct _BTRMGR_ConnectedDevice_t {
    BTRMgrDeviceHandle          m_deviceHandle;
    BTRMGR_DeviceType_t         m_deviceType;
    char                        m_name [BTRMGR_NAME_LEN_MAX];
    char                        m_deviceAddress [BTRMGR_NAME_LEN_MAX];
    BTRMGR_DeviceServiceList_t  m_serviceInfo;
    unsigned short              m_vendorID;
    unsigned char               m_isLowEnergyDevice;
    unsigned char               m_isConnected; /* This must be used only when m_isPaired is TRUE */
    BTRMGR_DevicePower_t        m_powerStatus;
} BTRMGR_ConnectedDevice_t;

typedef struct _BTRMGR_PairedDevices_t {
    BTRMgrDeviceHandle          m_deviceHandle;
    BTRMGR_DeviceType_t         m_deviceType;
    char                        m_name [BTRMGR_NAME_LEN_MAX];
    char                        m_deviceAddress [BTRMGR_NAME_LEN_MAX];
    BTRMGR_DeviceServiceList_t  m_serviceInfo;
    unsigned short              m_vendorID;
    unsigned char               m_isLowEnergyDevice;
    unsigned char               m_isConnected; /* This must be used only when m_isPaired is TRUE */
    unsigned char               m_isLastConnectedDevice;
} BTRMGR_PairedDevices_t;

typedef struct _BTRMGR_DiscoveredDevices_t {
    BTRMgrDeviceHandle  m_deviceHandle;
    BTRMGR_DeviceType_t m_deviceType;
    char                m_name [BTRMGR_NAME_LEN_MAX];
    char                m_deviceAddress [BTRMGR_NAME_LEN_MAX];
    unsigned short      m_vendorID;
    unsigned char       m_isPairedDevice;
    unsigned char       m_isConnected; /* This must be used only when m_isPaired is TRUE */
    unsigned char       m_isLowEnergyDevice;
    BTRMGR_RSSIValue_t  m_rssi;
    int                 m_signalLevel;
} BTRMGR_DiscoveredDevices_t;

typedef struct _BTRMGR_ConnectedDevicesList_t {
    unsigned short              m_numOfDevices;
    BTRMGR_ConnectedDevice_t    m_deviceProperty[BTRMGR_DEVICE_COUNT_MAX];
} BTRMGR_ConnectedDevicesList_t;

typedef struct _BTRMGR_PairedDevicesList_t {
    unsigned short          m_numOfDevices;
    BTRMGR_PairedDevices_t  m_deviceProperty[BTRMGR_DEVICE_COUNT_MAX];
} BTRMGR_PairedDevicesList_t;

typedef struct _BTRMGR_DiscoveredDevicesList_t {
    unsigned short              m_numOfDevices;
    BTRMGR_DiscoveredDevices_t  m_deviceProperty[BTRMGR_DEVICE_COUNT_MAX];
} BTRMGR_DiscoveredDevicesList_t;

typedef struct _BTRMGR_ExternalDevice_t {
    BTRMgrDeviceHandle          m_deviceHandle;
    BTRMGR_DeviceType_t         m_deviceType;
    char                        m_name [BTRMGR_NAME_LEN_MAX];
    char                        m_deviceAddress [BTRMGR_NAME_LEN_MAX];
    BTRMGR_DeviceServiceList_t  m_serviceInfo;
    unsigned short              m_vendorID;
    unsigned char               m_isLowEnergyDevice;
    unsigned int                m_externalDevicePIN;
} BTRMGR_ExternalDevice_t;

typedef struct _BTRMGR_MediaInfo_t {
    BTRMgrDeviceHandle     m_deviceHandle;
    BTRMGR_DeviceType_t    m_deviceType;
    char                   m_name [BTRMGR_NAME_LEN_MAX];
    union {
       BTRMGR_MediaTrackInfo_t     m_mediaTrackInfo;
       BTRMGR_MediaPositionInfo_t  m_mediaPositionInfo;
    };
} BTRMGR_MediaInfo_t;

typedef struct _BTRMGR_EventMessage_t {
    unsigned char   m_adapterIndex;
    BTRMGR_Events_t m_eventType;
    union {
        BTRMGR_DiscoveredDevices_t  m_discoveredDevice;
        BTRMGR_ExternalDevice_t     m_externalDevice;
        BTRMGR_PairedDevices_t      m_pairedDevice;
        BTRMGR_MediaInfo_t          m_mediaInfo;
        unsigned short              m_numOfDevices;
    };
} BTRMGR_EventMessage_t;

typedef struct _BTRMGR_EventResponse_t {
    BTRMGR_Events_t     m_eventType;
    BTRMgrDeviceHandle  m_deviceHandle;
    union {
        unsigned char   m_eventResp;
    };
} BTRMGR_EventResponse_t;


typedef void (*BTRMGR_EventCallback)(BTRMGR_EventMessage_t);


BTRMGR_Result_t BTRMGR_Init(void);
BTRMGR_Result_t BTRMGR_GetNumberOfAdapters(unsigned char *pNumOfAdapters);

BTRMGR_Result_t BTRMGR_SetAdapterName(unsigned char index_of_adapter, const char* pNameOfAdapter);
BTRMGR_Result_t BTRMGR_GetAdapterName(unsigned char index_of_adapter, char* pNameOfAdapter);

BTRMGR_Result_t BTRMGR_SetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char power_status);
BTRMGR_Result_t BTRMGR_GetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char *pPowerStatus);

BTRMGR_Result_t BTRMGR_SetAdapterDiscoverable(unsigned char index_of_adapter, unsigned char discoverable, int timeout);
BTRMGR_Result_t BTRMGR_IsAdapterDiscoverable(unsigned char index_of_adapter, unsigned char *pDiscoverable);

BTRMGR_Result_t BTRMGR_StartDeviceDiscovery(unsigned char index_of_adapter);
BTRMGR_Result_t BTRMGR_StopDeviceDiscovery(unsigned char index_of_adapter);
BTRMGR_Result_t BTRMGR_GetDiscoveredDevices(unsigned char index_of_adapter, BTRMGR_DiscoveredDevicesList_t *pDiscoveredDevices);

BTRMGR_Result_t BTRMGR_PairDevice(unsigned char index_of_adapter, BTRMgrDeviceHandle handle);
BTRMGR_Result_t BTRMGR_UnpairDevice(unsigned char index_of_adapter, BTRMgrDeviceHandle handle);
BTRMGR_Result_t BTRMGR_GetPairedDevices(unsigned char index_of_adapter, BTRMGR_PairedDevicesList_t *pPairedDevices);

BTRMGR_Result_t BTRMGR_ConnectToDevice(unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_DeviceConnect_Type_t connectAs);
BTRMGR_Result_t BTRMGR_DisconnectFromDevice(unsigned char index_of_adapter, BTRMgrDeviceHandle handle);
BTRMGR_Result_t BTRMGR_GetConnectedDevices(unsigned char index_of_adapter, BTRMGR_ConnectedDevicesList_t *pConnectedDevices);

BTRMGR_Result_t BTRMGR_GetDeviceProperties(unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_DevicesProperty_t *pDeviceProperty);

BTRMGR_Result_t BTRMGR_StartAudioStreamingOut(unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_DeviceConnect_Type_t connectAs);
BTRMGR_Result_t BTRMGR_StopAudioStreamingOut(unsigned char index_of_adapter, BTRMgrDeviceHandle handle);
BTRMGR_Result_t BTRMGR_IsAudioStreamingOut(unsigned char index_of_adapter, unsigned char *pStreamingStatus);
BTRMGR_Result_t BTRMGR_SetAudioStreamingOutType(unsigned char index_of_adapter, BTRMGR_StreamOut_Type_t type);

BTRMGR_Result_t BTRMGR_StartAudioStreamingIn(unsigned char ui8AdapterIdx, BTRMgrDeviceHandle handle, BTRMGR_DeviceConnect_Type_t connectAs);
BTRMGR_Result_t BTRMGR_StopAudioStreamingIn(unsigned char ui8AdapterIdx, BTRMgrDeviceHandle handle);
BTRMGR_Result_t BTRMGR_IsAudioStreamingIn(unsigned char ui8AdapterIdx, unsigned char *pStreamingStatus);

BTRMGR_Result_t BTRMGR_SetEventResponse(unsigned char index_of_adapter, BTRMGR_EventResponse_t* apstBTRMgrEvtRsp);

BTRMGR_Result_t BTRMGR_ResetAdapter(unsigned char index_of_adapter);

BTRMGR_Result_t BTRMGR_DeInit(void);

BTRMGR_Result_t BTRMGR_RegisterEventCallback(BTRMGR_EventCallback eventCallback);

BTRMGR_Result_t BTRMGR_MediaControl (unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_MediaControlCommand_t mediaCtrlCmd);

BTRMGR_Result_t BTRMGR_GetMediaTrackInfo (unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_MediaTrackInfo_t *mediaTrackInfo);

BTRMGR_Result_t BTRMGR_GetMediaCurrentPosition (unsigned char index_of_adapter, BTRMgrDeviceHandle handle, BTRMGR_MediaPositionInfo_t*  mediaPositionInfo);

const char* BTRMGR_GetDeviceTypeAsString(BTRMGR_DeviceType_t type);

#ifdef __cplusplus
}
#endif

#endif /* __BTR_MGR_H__ */
