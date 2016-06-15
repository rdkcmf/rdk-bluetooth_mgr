#ifndef __BTMGR_H__
#define __BTMGR_H__

#define BTMGR_NAME_LEN_MAX            64
#define BTMGR_DEVICE_COUNT_MAX        16
#define BTMGR_ADAPTER_COUNT_MAX       16

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
    BTMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST,
    BTMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST,
    BTMGR_EVENT_MAX
} BTMGR_Events_t;

typedef enum _BTMGR_StreamOut_Type_t {
    BTMGR_STREAM_PRIMARY = 0,
    BTMGR_STREAM_SECONDARY,
} BTMGR_StreamOut_Type_t;

typedef enum _BTMGR_DeviceConnect_Type_t {
    BTMGR_DEVICE_TYPE_AUDIOSINK     = 1 << 0,
    BTMGR_DEVICE_TYPE_HEADSET       = 1 << 1,
    BTMGR_DEVICE_TYPE_OTHER         = 1 << 2,
} BTMGR_DeviceConnect_Type_t;

typedef struct _BTMGR_DevicesProperty_t {
    BTMgrDeviceHandle m_deviceHandle;
    char m_name [BTMGR_NAME_LEN_MAX];
    char m_deviceAddress [BTMGR_NAME_LEN_MAX];
    char m_supportedServices [BTMGR_NAME_LEN_MAX];
    unsigned char m_isPaired;
    unsigned char m_isConnected; /* This must be used only when m_isPaired is TRUE */
    int  m_rssi;
} BTMGR_DevicesProperty_t;

typedef struct _BTMGR_Devices_t {
    unsigned short m_numOfDevices;
    BTMGR_DevicesProperty_t m_deviceProperty[BTMGR_DEVICE_COUNT_MAX];
} BTMGR_Devices_t;

typedef struct _BTMGR_EventMessage_t {
    unsigned char m_adapterIndex;
    BTMGR_Events_t m_eventType;
    BTMGR_Devices_t m_eventData;
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
BTMGR_Result_t BTMGR_GetDiscoveredDevices(unsigned char index_of_adapter, BTMGR_Devices_t *pDiscoveredDevices);

BTMGR_Result_t BTMGR_PairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle);
BTMGR_Result_t BTMGR_UnpairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle);
BTMGR_Result_t BTMGR_GetPairedDevices(unsigned char index_of_adapter, BTMGR_Devices_t *pDiscoveredDevices);

BTMGR_Result_t BTMGR_ConnectToDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DeviceConnect_Type_t connectAs);
BTMGR_Result_t BTMGR_DisconnectFromDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle);

BTMGR_Result_t BTMGR_GetDeviceProperties(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DevicesProperty_t *pDeviceProperty);

BTMGR_Result_t BTMGR_RegisterEventCallback(BTMGR_EventCallback eventCallback);

BTMGR_Result_t BTMGR_StartAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_StreamOut_Type_t streamOutPref);
BTMGR_Result_t BTMGR_StopAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle);
BTMGR_Result_t BTMGR_IsAudioStreamingOut(unsigned char index_of_adapter, unsigned char *pStreamingStatus);

BTMGR_Result_t BTMGR_ResetAdapter(unsigned char index_of_adapter);
BTMGR_Result_t BTMGR_DeInit(void);

#endif /* __BTMGR_H__ */
