#ifndef __BTMGR_H__
#define __BTMGR_H__

#define BTMGR_NAME_LEN_MAX            64
#define BTMGR_DEVICE_COUNT_MAX        32
#define BTMGR_ADAPTER_COUNT_MAX       16
#define BTMGR_MAX_DEVICE_PROFILE      32

typedef unsigned long long int BTMgrDeviceHandle;

typedef enum _BTMGR_Result_t {
    BTMGR_RESULT_SUCCESS = 0,
    BTMGR_RESULT_GENERIC_FAILURE = -1,
    BTMGR_RESULT_INVALID_INPUT = -2,
    BTMGR_RESULT_INIT_FAILED = -3
} BTMGR_Result_t;

typedef enum _BTMGR_Events_t {
    BTMGR_EVENT_DEVICE_OUT_OF_RANGE = 100,
    BTMGR_EVENT_DEVICE_DISCOVERY_UPDATE,
    BTMGR_EVENT_DEVICE_DISCOVERY_COMPLETE,
    BTMGR_EVENT_DEVICE_PAIRING_COMPLETE,
    BTMGR_EVENT_DEVICE_UNPAIRING_COMPLETE,
    BTMGR_EVENT_DEVICE_CONNECTION_COMPLETE,
    BTMGR_EVENT_DEVICE_DISCONNECT_COMPLETE,
    BTMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST,
    BTMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST,
    BTMGR_EVENT_MAX
} BTMGR_Events_t;

typedef enum _BTMGR_DeviceType_t {
    BTMGR_DEVICE_TYPE_UNKNOWN,
    BTMGR_DEVICE_TYPE_WEARABLE_HEADSET,
    BTMGR_DEVICE_TYPE_HANDSFREE,
    BTMGR_DEVICE_TYPE_RESERVED,
    BTMGR_DEVICE_TYPE_MICROPHONE,
    BTMGR_DEVICE_TYPE_LOUDSPEAKER,
    BTMGR_DEVICE_TYPE_HEADPHONES,
    BTMGR_DEVICE_TYPE_PORTABLE_AUDIO,
    BTMGR_DEVICE_TYPE_CAR_AUDIO,
    BTMGR_DEVICE_TYPE_STB,
    BTMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE,
    BTMGR_DEVICE_TYPE_VCR,
    BTMGR_DEVICE_TYPE_VIDEO_CAMERA,
    BTMGR_DEVICE_TYPE_CAMCODER,
    BTMGR_DEVICE_TYPE_VIDEO_MONITOR,
    BTMGR_DEVICE_TYPE_TV,
    BTMGR_DEVICE_TYPE_VIDEO_CONFERENCE,
    BTMGR_DEVICE_TYPE_END
} BTMGR_DeviceType_t;

typedef enum _BTMGR_StreamOut_Type_t {
    BTMGR_STREAM_PRIMARY = 0,
    BTMGR_STREAM_SECONDARY,
} BTMGR_StreamOut_Type_t;

typedef enum _BTMGR_DeviceConnect_Type_t {
    BTMGR_DEVICE_TYPE_AUDIOSINK     = 1 << 0,
    BTMGR_DEVICE_TYPE_HEADSET       = 1 << 1,
    BTMGR_DEVICE_TYPE_OTHER         = 1 << 2,
} BTMGR_DeviceConnect_Type_t;

typedef enum _BTMGR_DevicePower_t {
    BTMGR_DEVICE_POWER_ACTIVE = 0,
    BTMGR_DEVICE_POWER_LOW,
    BTMGR_DEVICE_POWER_STANDBY
} BTMGR_DevicePower_t;

typedef enum _BTMGR_RSSIValue_type_t {
    BTMGR_RSSI_NONE = 0,      //!< No signal (0 bar)
    BTMGR_RSSI_POOR,          //!< Poor (1 bar)
    BTMGR_RSSI_FAIR,          //!< Fair (2 bars)
    BTMGR_RSSI_GOOD,          //!< Good (3 bars)
    BTMGR_RSSI_EXCELLENT      //!< Excellent (4 bars)
} BTMGR_RSSIValue_t;

typedef struct _BTMGR_DeviceService_t {
    unsigned short m_uuid;
    char m_profile[BTMGR_NAME_LEN_MAX];
} BTMGR_DeviceService_t;

typedef struct _BTMGR_DeviceServiceList_t {
    unsigned short m_numOfService;
    BTMGR_DeviceService_t m_profileInfo[BTMGR_MAX_DEVICE_PROFILE];
} BTMGR_DeviceServiceList_t;

typedef struct _BTMGR_DevicesProperty_t {
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DeviceType_t m_deviceType;
    char m_name [BTMGR_NAME_LEN_MAX];
    char m_deviceAddress [BTMGR_NAME_LEN_MAX];
    BTMGR_RSSIValue_t m_rssi;
    int m_signalLevel;
    unsigned short m_vendorID;
    unsigned char m_isPaired;
    unsigned char m_isConnected; /* This must be used only when m_isPaired is TRUE */
    unsigned char m_isLowEnergyDevice;
    BTMGR_DeviceServiceList_t m_serviceInfo;
} BTMGR_DevicesProperty_t;

typedef struct _BTMGR_ConnectedDevice_t {
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DeviceType_t m_deviceType;
    char m_name [BTMGR_NAME_LEN_MAX];
    char m_deviceAddress [BTMGR_NAME_LEN_MAX];
    BTMGR_DeviceServiceList_t m_serviceInfo;
    unsigned short m_vendorID;
    unsigned char m_isLowEnergyDevice;
    unsigned char m_isConnected; /* This must be used only when m_isPaired is TRUE */
    BTMGR_DevicePower_t m_powerStatus;
} BTMGR_ConnectedDevice_t;

typedef struct _BTMGR_PairedDevices_t {
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DeviceType_t m_deviceType;
    char m_name [BTMGR_NAME_LEN_MAX];
    char m_deviceAddress [BTMGR_NAME_LEN_MAX];
    BTMGR_DeviceServiceList_t m_serviceInfo;
    unsigned short m_vendorID;
    unsigned char m_isLowEnergyDevice;
    unsigned char m_isConnected; /* This must be used only when m_isPaired is TRUE */
} BTMGR_PairedDevices_t;

typedef struct _BTMGR_DiscoveredDevices_t {
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DeviceType_t m_deviceType;
    char m_name [BTMGR_NAME_LEN_MAX];
    char m_deviceAddress [BTMGR_NAME_LEN_MAX];
    unsigned short m_vendorID;
    unsigned char m_isPairedDevice;
    unsigned char m_isConnected; /* This must be used only when m_isPaired is TRUE */
    unsigned char m_isLowEnergyDevice;
    BTMGR_RSSIValue_t m_rssi;
    int m_signalLevel;
} BTMGR_DiscoveredDevices_t;

typedef struct _BTMGR_ConnectedDevicesList_t {
    unsigned short m_numOfDevices;
    BTMGR_ConnectedDevice_t m_deviceProperty[BTMGR_DEVICE_COUNT_MAX];
} BTMGR_ConnectedDevicesList_t;

typedef struct _BTMGR_PairedDevicesList_t {
    unsigned short m_numOfDevices;
    BTMGR_PairedDevices_t m_deviceProperty[BTMGR_DEVICE_COUNT_MAX];
} BTMGR_PairedDevicesList_t;

typedef struct _BTMGR_DiscoveredDevicesList_t {
    unsigned short m_numOfDevices;
    BTMGR_DiscoveredDevices_t m_deviceProperty[BTMGR_DEVICE_COUNT_MAX];
} BTMGR_DiscoveredDevicesList_t;

typedef struct _BTMGR_ExternalDevice_t {
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DeviceType_t m_deviceType;
    char m_name [BTMGR_NAME_LEN_MAX];
    char m_deviceAddress [BTMGR_NAME_LEN_MAX];
    BTMGR_DeviceServiceList_t m_serviceInfo;
    unsigned short m_vendorID;
    unsigned char m_isLowEnergyDevice;
    char m_externalDevicePIN[BTMGR_NAME_LEN_MAX];
} BTMGR_ExternalDevice_t;

typedef struct _BTMGR_EventMessage_t {
    unsigned char m_adapterIndex;
    BTMGR_Events_t m_eventType;
    union {
        BTMGR_DiscoveredDevices_t m_discoveredDevice;
        BTMGR_ExternalDevice_t m_externalDevice;
        unsigned short m_numOfDevices;
    };
} BTMGR_EventMessage_t;

typedef void (*BTMGR_EventCallback)(BTMGR_EventMessage_t);


BTMGR_Result_t BTMGR_Init(void);
BTMGR_Result_t BTMGR_GetNumberOfAdapters(unsigned char *pNumOfAdapters);

BTMGR_Result_t BTMGR_SetAdapterName(unsigned char index_of_adapter, const char* pNameOfAdapter);
BTMGR_Result_t BTMGR_GetAdapterName(unsigned char index_of_adapter, char* pNameOfAdapter);

BTMGR_Result_t BTMGR_SetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char power_status);
BTMGR_Result_t BTMGR_GetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char *pPowerStatus);

BTMGR_Result_t BTMGR_SetAdapterDiscoverable(unsigned char index_of_adapter, unsigned char discoverable, unsigned short timeout);
BTMGR_Result_t BTMGR_IsAdapterDiscoverable(unsigned char index_of_adapter, unsigned char *pDiscoverable);

BTMGR_Result_t BTMGR_StartDeviceDiscovery(unsigned char index_of_adapter);
BTMGR_Result_t BTMGR_StopDeviceDiscovery(unsigned char index_of_adapter);
BTMGR_Result_t BTMGR_GetDiscoveredDevices(unsigned char index_of_adapter, BTMGR_DiscoveredDevicesList_t *pDiscoveredDevices);

BTMGR_Result_t BTMGR_PairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle);
BTMGR_Result_t BTMGR_UnpairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle);
BTMGR_Result_t BTMGR_GetPairedDevices(unsigned char index_of_adapter, BTMGR_PairedDevicesList_t *pPairedDevices);

BTMGR_Result_t BTMGR_ConnectToDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DeviceConnect_Type_t connectAs);
BTMGR_Result_t BTMGR_DisconnectFromDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle);
BTMGR_Result_t BTMGR_GetConnectedDevices(unsigned char index_of_adapter, BTMGR_ConnectedDevicesList_t *pConnectedDevices);

BTMGR_Result_t BTMGR_GetDeviceProperties(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DevicesProperty_t *pDeviceProperty);

BTMGR_Result_t BTMGR_RegisterEventCallback(BTMGR_EventCallback eventCallback);

BTMGR_Result_t BTMGR_StartAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_StreamOut_Type_t streamOutPref);
BTMGR_Result_t BTMGR_StopAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle);
BTMGR_Result_t BTMGR_IsAudioStreamingOut(unsigned char index_of_adapter, unsigned char *pStreamingStatus);

BTMGR_Result_t BTMGR_ResetAdapter(unsigned char index_of_adapter);
BTMGR_Result_t BTMGR_DeInit(void);


const char* BTMGR_GetDeviceTypeAsString(BTMGR_DeviceType_t type);

#endif /* __BTMGR_H__ */
