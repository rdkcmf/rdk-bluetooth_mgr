#ifdef BTMGR_ENABLE_IARM_INTERFACE
#include "libIBus.h"
#include "libIARM.h"

#define IARM_BUS_BTMGR_NAME        "BTMgrBus"

typedef enum _BTMGR_IARMEvents_t {
    BTMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE,
    BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE,
    BTMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE,
    BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST,
    BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST,
    BTMGR_IARM_EVENT_MAX
} BTMGR_IARM_Events_t;

typedef struct _BTMGR_IARMAdapterName_t {
    unsigned char m_adapterIndex;
    char m_name[BTMGR_NAME_LEN_MAX];
} BTMGR_IARMAdapterName_t;

typedef struct _BTMGR_IARMPairDevice_t {
    unsigned char m_adapterIndex;
    BTMgrDeviceHandle m_deviceHandle;
} BTMGR_IARMPairDevice_t;

typedef struct _BTMGR_IARMConnectDevice_t {
    unsigned char m_adapterIndex;
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DeviceConnect_Type_t m_connectAs;
} BTMGR_IARMConnectDevice_t;

typedef struct _BTMGR_IARMAdapterPower_t {
    unsigned char m_adapterIndex;
    unsigned char m_powerStatus;
} BTMGR_IARMAdapterPower_t;

typedef struct _BTMGR_IARMAdapterDiscoverable_t {
    unsigned char m_adapterIndex;
    unsigned char m_isDiscoverable;
    unsigned short m_timeout;
} BTMGR_IARMAdapterDiscoverable_t;

typedef struct _BTMGR_IARMAdapterDiscover_t {
    unsigned char m_adapterIndex;
    unsigned char m_setDiscovery;
} BTMGR_IARMAdapterDiscover_t;

typedef struct _BTMGR_IARMDDeviceProperty_t {
    unsigned char m_adapterIndex;
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DevicesProperty_t m_deviceProperty;
} BTMGR_IARMDDeviceProperty_t;

typedef struct _BTMGR_IARMDevices_t {
    unsigned char m_adapterIndex;
    BTMGR_Devices_t m_devices;
} BTMGR_IARMDevices_t;

typedef struct _BTMGR_IARMStreaming_t {
    unsigned char m_adapterIndex;
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_StreamOut_Type_t m_audioPref;
} BTMGR_IARMStreaming_t;

typedef struct _BTMGR_IARMStreamingStatus_t {
    unsigned char m_adapterIndex;
    unsigned char m_streamingStatus;
} BTMGR_IARMStreamingStatus_t;


void btmgr_BeginIARMMode();
void btmgr_TermIARMMode();

#endif /* BTMGR_ENABLE_IARM_INTERFACE */
