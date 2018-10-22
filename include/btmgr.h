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
/**
 * @file btmgr.h
 *
 * @defgroup BTR_MGR Bluetooth Manager
 *
 * Bluetooth Manager (An RDK component) interfaces with BlueZ through the D-Bus API,
 * so there is no direct linking of the BlueZ library with Bluetooth Manager.
 * Bluetooth manager provides an interface to port any Bluetooth stack on RDK
 * The Bluetooth manager daemon manages Bluetooth services in RDK.
 * It uses IARM Bus to facilitate communication between the application and Bluetooth driver
 * through Bluetooth Manager component.
 * @ingroup  Bluetooth
 *
 * @defgroup BTR_MGR_API Bluetooth Manager Data Types and API(s)
 * This file provides the data types and API(s) used by the bluetooth manager.
 * @ingroup  BTR_MGR
 *
 */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @addtogroup BTR_MGR_API
 * @{
 */

#define BTRMGR_MAX_STR_LEN             256
#define BTRMGR_NAME_LEN_MAX            64
#define BTRMGR_STR_LEN                 32
#define BTRMGR_DEVICE_COUNT_MAX        32
#define BTRMGR_ADAPTER_COUNT_MAX       16
#define BTRMGR_MAX_DEVICE_PROFILE      32
#define BTRMGR_LE_FLAG_LIST_SIZE       10

typedef unsigned long long int BTRMgrDeviceHandle;

/**
 * @brief Represents the status of the operation.
 */
typedef enum _BTRMGR_Result_t {
    BTRMGR_RESULT_SUCCESS = 0,
    BTRMGR_RESULT_GENERIC_FAILURE = -1,
    BTRMGR_RESULT_INVALID_INPUT = -2,
    BTRMGR_RESULT_INIT_FAILED = -3
} BTRMGR_Result_t;

/**
 * @brief Represents the event status.
 */
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
    BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED,
    BTRMGR_EVENT_DEVICE_OP_READY,
    BTRMGR_EVENT_DEVICE_OP_INFORMATION,
    BTRMGR_EVENT_MAX
} BTRMGR_Events_t;

/**
 * @brief Represents the bluetooth device types.
 */
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
    BTRMGR_DEVICE_TYPE_TABLET,
    // LE
    BTRMGR_DEVICE_TYPE_TILE,
    BTRMGR_DEVICE_TYPE_HID,
    BTRMGR_DEVICE_TYPE_END
} BTRMGR_DeviceType_t;

/**
 * @brief Represents the stream output types.
 */
typedef enum _BTRMGR_StreamOut_Type_t {
    BTRMGR_STREAM_PRIMARY = 0,
    BTRMGR_STREAM_SECONDARY,
} BTRMGR_StreamOut_Type_t;

/**
 * @brief Represents the operation type for bluetooth device.
 */
typedef enum _BTRMGR_DeviceOperationType_t {
    BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT  = 1 << 0,
    BTRMGR_DEVICE_OP_TYPE_AUDIO_INPUT   = 1 << 1,
    BTRMGR_DEVICE_OP_TYPE_LE            = 1 << 2,
    BTRMGR_DEVICE_OP_TYPE_HID           = 1 << 3,
    BTRMGR_DEVICE_OP_TYPE_UNKNOWN       = 1 << 4
} BTRMGR_DeviceOperationType_t;

/**
 * @brief Represents the bluetooth power states.
 */
typedef enum _BTRMGR_DevicePower_t {
    BTRMGR_DEVICE_POWER_ACTIVE = 0,
    BTRMGR_DEVICE_POWER_LOW,
    BTRMGR_DEVICE_POWER_STANDBY
} BTRMGR_DevicePower_t;

/**
 * @brief Represents the bluetooth signal strength
 */
typedef enum _BTRMGR_RSSIValue_type_t {
    BTRMGR_RSSI_NONE = 0,      //!< No signal (0 bar)
    BTRMGR_RSSI_POOR,          //!< Poor (1 bar)
    BTRMGR_RSSI_FAIR,          //!< Fair (2 bars)
    BTRMGR_RSSI_GOOD,          //!< Good (3 bars)
    BTRMGR_RSSI_EXCELLENT      //!< Excellent (4 bars)
} BTRMGR_RSSIValue_t;

/**
 * @brief Represents the bluetooth Discovery Status
 */
typedef enum _BTRMGR_DiscoveryStatus_t {
    BTRMGR_DISCOVERY_STATUS_OFF,
    BTRMGR_DISCOVERY_STATUS_IN_PROGRESS,
} BTRMGR_DiscoveryStatus_t;


/**
 * @brief Represents the commands to control the media files.
 */
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

typedef enum _BTRMGR_LeProperty_t {  // looking for a better enum name
    BTRMGR_LE_PROP_UUID,
    BTRMGR_LE_PROP_PRIMARY,
    BTRMGR_LE_PROP_DEVICE,
    BTRMGR_LE_PROP_SERVICE,
    BTRMGR_LE_PROP_VALUE,
    BTRMGR_LE_PROP_NOTIFY,
    BTRMGR_LE_PROP_FLAGS,
    BTRMGR_LE_PROP_CHAR
} BTRMGR_LeProperty_t;

/**
 * @brief Represents the Low energy operations.
 */
typedef enum _BTRMGR_LeOp_t {
    BTRMGR_LE_OP_READY,
    BTRMGR_LE_OP_READ_VALUE,
    BTRMGR_LE_OP_WRITE_VALUE,
    BTRMGR_LE_OP_START_NOTIFY,
    BTRMGR_LE_OP_STOP_NOTIFY,
    BTRMGR_LE_OP_UNKNOWN
} BTRMGR_LeOp_t;

typedef enum _BTRMGR_GattCharFlags_t {
    BTRMGR_GATT_CHAR_FLAG_READ                         = 1 << 0,
    BTRMGR_GATT_CHAR_FLAG_WRITE                        = 1 << 1,
    BTRMGR_GATT_CHAR_FLAG_ENCRYPT_READ                 = 1 << 2,
    BTRMGR_GATT_CHAR_FLAG_ENCRYPT_WRITE                = 1 << 3,
    BTRMGR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_READ   = 1 << 4,
    BTRMGR_GATT_CHAR_FLAG_ENCRYPT_AUTHENTICATED_WRITE  = 1 << 5,
    BTRMGR_GATT_CHAR_FLAG_SECURE_READ                  = 1 << 6,
    BTRMGR_GATT_CHAR_FLAG_SECURE_WRITE                 = 1 << 7,
    BTRMGR_GATT_CHAR_FLAG_NOTIFY                       = 1 << 8,
    BTRMGR_GATT_CHAR_FLAG_INDICATE                     = 1 << 9,
    BTRMGR_GATT_CHAR_FLAG_BROADCAST                    = 1 << 10,
    BTRMGR_GATT_CHAR_FLAG_WRITE_WITHOUT_RESPONSE       = 1 << 11,
    BTRMGR_GATT_CHAR_FLAG_AUTHENTICATED_SIGNED_WRITES  = 1 << 12,
    BTRMGR_GATT_CHAR_FLAG_RELIABLE_WRITE               = 1 << 13,
    BTRMGR_GATT_CHAR_FLAG_WRITABLE_AUXILIARIES         = 1 << 14
} BTRMGR_GattCharFlags_t;


typedef enum _BTRMGR_ScanFilter_t {
    BTRMGR_DISCOVERY_FILTER_UUID,
    BTRMGR_DISCOVERY_FILTER_RSSI,
    BTRMGR_DISCOVERY_FILTER_PATH_LOSS,
    BTRMGR_DISCOVERY_FILTER_SCAN_TYPE
} BTRMGR_ScanFilter_t;

/**
 * @brief Represents the media track info.
 */
typedef struct _BTRMGR_MediaTrackInfo_t {
    char            pcAlbum[BTRMGR_MAX_STR_LEN];
    char            pcGenre[BTRMGR_MAX_STR_LEN];
    char            pcTitle[BTRMGR_MAX_STR_LEN];
    char            pcArtist[BTRMGR_MAX_STR_LEN];
    unsigned int    ui32TrackNumber;
    unsigned int    ui32Duration;
    unsigned int    ui32NumberOfTracks;
} BTRMGR_MediaTrackInfo_t;

/**
 * @brief Represents the media position info.
 */
typedef struct _BTRMGR_MediaPositionInfo_t {
    unsigned int          m_mediaDuration;
    unsigned int          m_mediaPosition;
} BTRMGR_MediaPositionInfo_t;

typedef struct _BTRMGR_LeUUID_t {
    unsigned short  flags;
    char            m_uuid[BTRMGR_NAME_LEN_MAX];
} BTRMGR_LeUUID_t;

/**
 * @brief Represents the supported service of the device.
 */
typedef struct _BTRMGR_DeviceService_t {
    unsigned short  m_uuid;
    char            m_profile[BTRMGR_NAME_LEN_MAX];
} BTRMGR_DeviceService_t;

/**
 * @brief Represents device services list.
 */
typedef struct _BTRMGR_DeviceServiceList_t {
    unsigned short          m_numOfService;

    union { /* have introduced BTRMGR_LeUUID_t inorder that the usage of BTRMGR_DeviceService_t shouldn't be confused
               if BTRMGR_DeviceService_t  alone is sufficient, then lets change in the next commit */
        BTRMGR_DeviceService_t  m_profileInfo[BTRMGR_MAX_DEVICE_PROFILE];
        BTRMGR_LeUUID_t         m_uuidInfo[BTRMGR_MAX_DEVICE_PROFILE];
    };
} BTRMGR_DeviceServiceList_t;

/**
 * @brief Represents the property of the device.
 */
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

/**
 * @brief Represents the details of device connected.
 */
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

/**
 * @brief Represents the paired devices information.
 */
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
    unsigned int                m_ui32DevClassBtSpec;
} BTRMGR_PairedDevices_t;

/**
 * @brief Represents the discovered device's details.
 */
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
    unsigned char       m_isDiscovered;
    unsigned char       m_isLastConnectedDevice;
    unsigned int        m_ui32DevClassBtSpec;
} BTRMGR_DiscoveredDevices_t;

/**
 * @brief Represents the connected devices list.
 */
typedef struct _BTRMGR_ConnectedDevicesList_t {
    unsigned short              m_numOfDevices;
    BTRMGR_ConnectedDevice_t    m_deviceProperty[BTRMGR_DEVICE_COUNT_MAX];
} BTRMGR_ConnectedDevicesList_t;

/**
 * @brief Represents the list of paired devices.
 */
typedef struct _BTRMGR_PairedDevicesList_t {
    unsigned short          m_numOfDevices;
    BTRMGR_PairedDevices_t  m_deviceProperty[BTRMGR_DEVICE_COUNT_MAX];
} BTRMGR_PairedDevicesList_t;

/**
 * @brief Represents the list of scanned devices.
 */
typedef struct _BTRMGR_DiscoveredDevicesList_t {
    unsigned short              m_numOfDevices;
    BTRMGR_DiscoveredDevices_t  m_deviceProperty[BTRMGR_DEVICE_COUNT_MAX];
} BTRMGR_DiscoveredDevicesList_t;

/**
 * @brief Represents the details of external devices connected.
 */
typedef struct _BTRMGR_ExternalDevice_t {
    BTRMgrDeviceHandle          m_deviceHandle;
    BTRMGR_DeviceType_t         m_deviceType;
    char                        m_name [BTRMGR_NAME_LEN_MAX];
    char                        m_deviceAddress [BTRMGR_NAME_LEN_MAX];
    BTRMGR_DeviceServiceList_t  m_serviceInfo;
    unsigned short              m_vendorID;
    unsigned char               m_isLowEnergyDevice;
    unsigned int                m_externalDevicePIN;
    unsigned char               m_requestConfirmation;
} BTRMGR_ExternalDevice_t;

/**
 * @brief Represents the media info.
 */
typedef struct _BTRMGR_MediaInfo_t {
    BTRMgrDeviceHandle     m_deviceHandle;
    BTRMGR_DeviceType_t    m_deviceType;
    char                   m_name [BTRMGR_NAME_LEN_MAX];
    union {
       BTRMGR_MediaTrackInfo_t     m_mediaTrackInfo;
       BTRMGR_MediaPositionInfo_t  m_mediaPositionInfo;
    };
} BTRMGR_MediaInfo_t;

/**
 * @brief Represents the notification data
 */
typedef struct _BTRMGR_DeviceOpInfo_t {
    BTRMgrDeviceHandle     m_deviceHandle;
    BTRMGR_DeviceType_t    m_deviceType;
    char                   m_name [BTRMGR_NAME_LEN_MAX];
    BTRMGR_LeOp_t          m_leOpType;

    union {
        char               m_readData[BTRMGR_MAX_STR_LEN];
        char               m_writeData[BTRMGR_MAX_STR_LEN];
        char               m_notifyData[BTRMGR_MAX_STR_LEN];
    };
} BTRMGR_DeviceOpInfo_t;

/**
 * @brief Represents the event message info.
 */
typedef struct _BTRMGR_EventMessage_t {
    unsigned char   m_adapterIndex;
    BTRMGR_Events_t m_eventType;
    union {
        BTRMGR_DiscoveredDevices_t  m_discoveredDevice;
        BTRMGR_ExternalDevice_t     m_externalDevice;
        BTRMGR_PairedDevices_t      m_pairedDevice;
        BTRMGR_MediaInfo_t          m_mediaInfo;
        BTRMGR_DeviceOpInfo_t       m_deviceOpInfo;
    };
} BTRMGR_EventMessage_t;

/**
 * @brief Represents the event response.
 */
typedef struct _BTRMGR_EventResponse_t {
    BTRMGR_Events_t     m_eventType;
    BTRMgrDeviceHandle  m_deviceHandle;
    union {
        unsigned char   m_eventResp;
    };
} BTRMGR_EventResponse_t;

typedef struct _BTRMGR_UUID_t {
    char**  m_uuid;
    short   m_uuidCount;
} BTRMGR_UUID_t;

typedef struct _BTRMGR_DiscoveryFilterHandle_t {
    
    BTRMGR_UUID_t               m_btuuid;
    int                         m_rssi;
    int                         m_pathloss;
    //BTRMGR_DeviceScanType_t     m_scanType;
} BTRMGR_DiscoveryFilterHandle_t;

/* Fptr Callbacks types */
typedef BTRMGR_Result_t (*BTRMGR_EventCallback)(BTRMGR_EventMessage_t astEventMessage);


/* Interfaces */

/**
 * @brief  This API initializes the bluetooth manager.
 *
 * This API performs the following operations:
 *
 * - Initializes the bluetooth core layer.
 * - Initialize the Paired Device List for Default adapter.
 * - Register for callback to get the status of connected Devices.
 * - Register for callback to get the Discovered Devices.
 * - Register for callback to process incoming pairing requests.
 * - Register for callback to process incoming connection requests.
 * - Register for callback to process incoming media events.
 * - Activates the default agent.
 * - Initializes the persistant interface and saves all bluetooth profiles to the database.
 *
 * @return Returns the status of the operation.
 * @retval BTRMGR_RESULT_SUCCESS on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_Init(void);

/**
 * @brief  This API invokes the deinit function of bluetooth core and persistant interface module.
 *
 * @return Returns the status of the operation.
 * @retval BTRMGR_RESULT_SUCCESS on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_DeInit(void);

/**
 * @brief  This API returns the number of bluetooth adapters available.
 *
 * @param[out] pNumOfAdapters    Indicates the number of adapters available.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetNumberOfAdapters(unsigned char *pNumOfAdapters);


/**
 * @brief  This API is designed to reset the bluetooth adapter.
 *
 * As of now, HAL implementation is not available for this API.
 *
 * @param[in] aui8AdapterIdx     Index of bluetooth adapter.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_ResetAdapter(unsigned char aui8AdapterIdx);

/**
 * @brief  This API is used to set the new name to the bluetooth adapter
 *
 * @param[in] aui8AdapterIdx     Index of bluetooth adapter.
 * @param[in] pNameOfAdapter     The name to set.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_SetAdapterName(unsigned char aui8AdapterIdx, const char* pNameOfAdapter);


/**
 * @brief  This API fetches the bluetooth adapter name.
 *
 * @param[in]  aui8AdapterIdx     Index of bluetooth adapter.
 * @param[out] pNameOfAdapter     Bluetooth adapter name.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetAdapterName(unsigned char aui8AdapterIdx, char* pNameOfAdapter);

/**
 * @brief  This API sets the bluetooth adapter power to ON/OFF.
 *
 * @param[in] aui8AdapterIdx     Index of bluetooth adapter.
 * @param[in] power_status        Value to set the  power. 0 to OFF & 1 to ON.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_SetAdapterPowerStatus(unsigned char aui8AdapterIdx, unsigned char power_status);

/**
 * @brief  This API fetches the power status, either 0 or 1.
 *
 * @param[in]  aui8AdapterIdx  Index of bluetooth adapter.
 * @param[out] pPowerStatus    Indicates the power status.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetAdapterPowerStatus(unsigned char aui8AdapterIdx, unsigned char *pPowerStatus);

/**
 * @brief  This API is to make the adapter discoverable until the given timeout.
 *
 * @param[in]  aui8AdapterIdx  Index of bluetooth adapter.
 * @param[in]  discoverable    Value to turn on or off the discovery.
 * @param[in]  timeout         Timeout to turn on discovery.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_SetAdapterDiscoverable(unsigned char aui8AdapterIdx, unsigned char discoverable, int timeout);

/**
 * @brief  This API checks the adapter is discoverable or not.
 *
 * @param[in]   aui8AdapterIdx  Index of bluetooth adapter.
 * @param[out]  pDiscoverable   Indicates discoverable or not.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_IsAdapterDiscoverable(unsigned char aui8AdapterIdx, unsigned char *pDiscoverable);

/**
 * @brief  This API initiates the scanning process.
 *
 * @param[in]   aui8AdapterIdx  Index of bluetooth adapter.
 * @param[in]   aenBTRMgrDevOpT Device operation type.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_StartDeviceDiscovery(unsigned char aui8AdapterIdx, BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT);

/**
 * @brief  This API terminates the scanning process.
 *
 * @param[in]   aui8AdapterIdx  Index of bluetooth adapter.
 * @param[in]   aenBTRMgrDevOpT Device operation type.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_StopDeviceDiscovery(unsigned char aui8AdapterIdx, BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT);

/**
 * @brief  This API gives the discovery status.
 *
 * @param[in]   aui8AdapterIdx  Index of bluetooth adapter.
 * @param[out]  aeDiscoveryStatus Device discovery status.
 * @param[out]  aenBTRMgrDevOpT  Device operation type.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetDiscoveryStatus (unsigned char aui8AdapterIdx, BTRMGR_DiscoveryStatus_t *aeDiscoveryStatus, BTRMGR_DeviceOperationType_t *aenBTRMgrDevOpT);
/**
 * @brief  This API fetches the list of devices scanned.
 *
 * @param[in]   aui8AdapterIdx        Index of bluetooth adapter.
 * @param[out]  pDiscoveredDevices    Structure which holds the details of device scanned.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetDiscoveredDevices(unsigned char aui8AdapterIdx, BTRMGR_DiscoveredDevicesList_t *pDiscoveredDevices);

/**
 * @brief  This API is used to pair the device that you wish to pair.
 *
 * @param[in] aui8AdapterIdx  Index of bluetooth adapter.
 * @param[in] ahBTRMgrDevHdl  Indicates the device handle.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_PairDevice(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl);

/**
 * @brief  This API is used to remove the pairing information of the device selected.
 *
 * @param[in]   aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]   ahBTRMgrDevHdl       Device handle.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_UnpairDevice(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl);

/**
 * @brief  This API returns the list of devices paired.
 *
 * @param[in] aui8AdapterIdx  Index of bluetooth adapter.
 * @param[in] pPairedDevices  Structure which holds the paired devices information.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetPairedDevices(unsigned char aui8AdapterIdx, BTRMGR_PairedDevicesList_t *pPairedDevices);

/**
 * @brief  This API connects the device as audio sink/headset/audio src based on the device type specified.
 *
 * @param[in] aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in] ahBTRMgrDevHdl        Indicates device handle.
 * @param[in] connectAs            Device operation type.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_ConnectToDevice(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t connectAs);

/**
 * @brief  This API terminates the current connection.
 *
 * @param[in] aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in] ahBTRMgrDevHdl        Indicates device handle.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_DisconnectFromDevice(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl);

/**
 * @brief  This API returns the list of devices connected.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[out] pConnectedDevices    List of connected devices.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetConnectedDevices(unsigned char aui8AdapterIdx, BTRMGR_ConnectedDevicesList_t *pConnectedDevices);

/**
 * @brief  This API returns the device information that includes the device name, mac address, RSSI value etc.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Indicates device handle.
 * @param[out] pDeviceProperty      Device property information.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */

BTRMGR_Result_t BTRMGR_GetDeviceProperties(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DevicesProperty_t *pDeviceProperty);
/**
 * @brief  This API initates the streaming from the device with default operation type.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  aenBTRMgrDevConT     Device opeartion type.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */

BTRMGR_Result_t BTRMGR_StartAudioStreamingOut_StartUp(unsigned char aui8AdapterIdx, BTRMGR_DeviceOperationType_t aenBTRMgrDevConT);
/**
 * @brief  This API initates the streaming from the device with the selected operation type.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Indicates device Handle.
 * @param[in]  connectAs            Device operation type.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */

BTRMGR_Result_t BTRMGR_StartAudioStreamingOut(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t connectAs);

/**
 * @brief  This API terminates the streaming from the device.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Indicates device Handle.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_StopAudioStreamingOut(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl);

/**
 * @brief  This API returns the stream out status.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[out] pStreamingStatus     Streaming status.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
//TODO: Return deviceHandle if we are streaming out
BTRMGR_Result_t BTRMGR_IsAudioStreamingOut(unsigned char aui8AdapterIdx, unsigned char *pStreamingStatus);

/**
 * @brief  This API is to set the audio type as primary or secondary.
 *
 * Secondary audio support is not implemented yet. Always primary audio is played for now.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  type                 Streaming type primary/secondary
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_SetAudioStreamingOutType(unsigned char aui8AdapterIdx, BTRMGR_StreamOut_Type_t type);

/**
 * @brief  This API starts the audio streaming.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Device handle.
 * @param[in]  connectAs            Device opeartion type.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_StartAudioStreamingIn(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_DeviceOperationType_t connectAs);

/**
 * @brief  This API termines the audio streaming.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Device handle.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_StopAudioStreamingIn(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl);

/**
 * @brief  This API returns the audio streaming status.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[out] pStreamingStatus     Streaming status.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
//TODO: Return  deviceHandle if we are streaming in
BTRMGR_Result_t BTRMGR_IsAudioStreamingIn(unsigned char aui8AdapterIdx, unsigned char *pStreamingStatus);

/**
 * @brief  This API handles the events received.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  apstBTRMgrEvtRsp     Structure which holds the event response.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_SetEventResponse(unsigned char aui8AdapterIdx, BTRMGR_EventResponse_t* apstBTRMgrEvtRsp);

/**
 * @brief  This API is used to perform the media control operations.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Device handle.
 * @param[in]  mediaCtrlCmd         Indicates the play, pause, resume etc.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_MediaControl(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_MediaControlCommand_t mediaCtrlCmd);

/**
 * @brief  This API fetches the media track info like title, genre, duration, number of tracks, current track number.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Device handle.
 * @param[out]  mediaTrackInfo       Track info like title, genre, duration etc.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetMediaTrackInfo(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_MediaTrackInfo_t *mediaTrackInfo);

/**
 * @brief  This API fetches the current position and total duration of the media.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Device handle.
 * @param[out] mediaPositionInfo    Media position info.
 *
 * @return Returns the status of the operation.
 * @retval BTRMGR_RESULT_SUCCESS on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetMediaCurrentPosition(unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, BTRMGR_MediaPositionInfo_t*  mediaPositionInfo);

/**
 * @brief  This API fetches the Device name of the media.
 *
 * @param[in]  type		Device type.
 *
 * @return Returns the device name.
 */
const char* BTRMGR_GetDeviceTypeAsString(BTRMGR_DeviceType_t type);

BTRMGR_Result_t BTRMGR_GetLeProperty (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, const char* apBtrPropUuid, BTRMGR_LeProperty_t aenLeProperty, void* vpPropValue);

/**
 * @brief  This API fetches the characteristic uuid of Le device.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Device handle.
 * @param[in]  apBtrServiceUuid     service UUID.
 * @param[out] apBtrCharUuidList    uuid list.
 *
 * @return Returns the status of the operation.
 * @retval BTRMGR_RESULT_SUCCESS on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_GetLeCharacteristicUUID (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, const char* apBtrServiceUuid, char* apBtrCharUuidList);

/**
 * @brief  This API performs LE operations on the specified bluetooth adapter.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  ahBTRMgrDevHdl       Device handle.
 * @param[in]  aBtrLeUuid           LE device uuid.
 * @param[in]  aLeOpType            LE device operation type.
 * @param[out] rOpResult            LE device operation result.
 *
 * @return Returns the status of the operation.
 * @retval BTRMGR_RESULT_SUCCESS on success, appropriate error code otherwise.
 */
BTRMGR_Result_t BTRMGR_PerformLeOp (unsigned char aui8AdapterIdx, BTRMgrDeviceHandle ahBTRMgrDevHdl, const char* aBtrLeUuid, BTRMGR_LeOp_t aLeOpType, char* aLeOpArg, char* rOpResult);

/**
 * @brief  This API performs LE operations on the specified bluetooth adapter.
 *
 * @param[in]  aui8AdapterIdx       Index of bluetooth adapter.
 * @param[in]  aui8State            0/1- Enable or Disable AudioIn service.
 *
 * @return Returns the status of the operation.
 * @retval BTRMGR_RESULT_SUCCESS on success.
 */
BTRMGR_Result_t BTRMGR_SetAudioInServiceState (unsigned char aui8AdapterIdx, unsigned char aui8State);

// Outgoing callbacks Registration Interfaces
BTRMGR_Result_t BTRMGR_RegisterEventCallback(BTRMGR_EventCallback afpcBBTRMgrEventOut);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* __BTR_MGR_H__ */
