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
#include "libIBus.h"
#include "libIARM.h"

#ifndef __BT_MGR_IARM_INTERFACE_H__
#define __BT_MGR_IARM_INTERFACE_H__


#define IARM_BUS_BTRMGR_NAME        "BTRMgrBus"


#define BTRMGR_IARM_METHOD_GET_NUMBER_OF_ADAPTERS           "GetNumberOfAdapters"
#define BTRMGR_IARM_METHOD_SET_ADAPTER_NAME                 "SetAdapterName"
#define BTRMGR_IARM_METHOD_GET_ADAPTER_NAME                 "GetAdapterName"
#define BTRMGR_IARM_METHOD_SET_ADAPTER_POWERSTATUS          "SetAdapterPowerStatus"
#define BTRMGR_IARM_METHOD_GET_ADAPTER_POWERSTATUS          "GetAdapterPowerStatus"
#define BTRMGR_IARM_METHOD_SET_ADAPTER_DISCOVERABLE         "SetAdapterDiscoverable"
#define BTRMGR_IARM_METHOD_IS_ADAPTER_DISCOVERABLE          "IsAdapterDiscoverable"
#define BTRMGR_IARM_METHOD_CHANGE_DEVICE_DISCOVERY_STATUS   "SetDeviceDiscoveryStatus"
#define BTRMGR_IARM_METHOD_GET_DISCOVERED_DEVICES           "GetDiscoveredDevices"
#define BTRMGR_IARM_METHOD_PAIR_DEVICE                      "PairDevice"
#define BTRMGR_IARM_METHOD_UNPAIR_DEVICE                    "UnpairDevice"
#define BTRMGR_IARM_METHOD_GET_PAIRED_DEVICES               "GetPairedDevices"
#define BTRMGR_IARM_METHOD_CONNECT_TO_DEVICE                "ConnectToDevice"
#define BTRMGR_IARM_METHOD_DISCONNECT_FROM_DEVICE           "DisconnectFromDevice"
#define BTRMGR_IARM_METHOD_GET_CONNECTED_DEVICES            "GetConnectedDevices"
#define BTRMGR_IARM_METHOD_GET_DEVICE_PROPERTIES            "GetDeviceProperties"
#define BTRMGR_IARM_METHOD_START_AUDIO_STREAMING_OUT        "StartAudioStreamingOut"
#define BTRMGR_IARM_METHOD_STOP_AUDIO_STREAMING_OUT         "StopAudioStreamingOut"
#define BTRMGR_IARM_METHOD_IS_AUDIO_STREAMING_OUT           "IsAudioStreamingOut"
#define BTRMGR_IARM_METHOD_SET_AUDIO_STREAM_OUT_TYPE        "SetAudioStreamOutType"
#define BTRMGR_IARM_METHOD_START_AUDIO_STREAMING_IN         "StartAudioStreamingIn"
#define BTRMGR_IARM_METHOD_STOP_AUDIO_STREAMING_IN          "StopAudioStreamingIn"
#define BTRMGR_IARM_METHOD_IS_AUDIO_STREAMING_IN            "IsAudioStreamingIn"
#define BTRMGR_IARM_METHOD_SET_EVENT_RESPONSE               "SetEventResponse"
#define BTRMGR_IARM_METHOD_RESET_ADAPTER                    "ResetAdapter"
#define BTRMGR_IARM_METHOD_DEINIT                           "DeInit"


typedef enum _BTRMGR_IARMEvents_t {
    BTRMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE,
    BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE,
    BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE,
    BTRMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE,
    BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE,
    BTRMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE,
    BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE,
    BTRMGR_IARM_EVENT_DEVICE_PAIRING_FAILED,
    BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED,
    BTRMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED,
    BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED,
    BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST,
    BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST,
    BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST,
    BTRMGR_IARM_EVENT_DEVICE_FOUND,
    BTRMGR_IARM_EVENT_MAX
} BTRMGR_IARM_Events_t;

typedef struct _BTRMGR_IARMAdapterName_t {
    unsigned char m_adapterIndex;
    char m_name[BTRMGR_NAME_LEN_MAX];
} BTRMGR_IARMAdapterName_t;

typedef struct _BTRMGR_IARMPairDevice_t {
    unsigned char m_adapterIndex;
    BTRMgrDeviceHandle m_deviceHandle;
} BTRMGR_IARMPairDevice_t;

typedef struct _BTRMGR_IARMConnectDevice_t {
    unsigned char m_adapterIndex;
    BTRMgrDeviceHandle m_deviceHandle;
    BTRMGR_DeviceConnect_Type_t m_connectAs;
} BTRMGR_IARMConnectDevice_t;

typedef struct _BTRMGR_IARMAdapterPower_t {
    unsigned char m_adapterIndex;
    unsigned char m_powerStatus;
} BTRMGR_IARMAdapterPower_t;

typedef struct _BTRMGR_IARMAdapterDiscoverable_t {
    unsigned char m_adapterIndex;
    unsigned char m_isDiscoverable;
    unsigned short m_timeout;
} BTRMGR_IARMAdapterDiscoverable_t;

typedef struct _BTRMGR_IARMAdapterDiscover_t {
    unsigned char m_adapterIndex;
    unsigned char m_setDiscovery;
} BTRMGR_IARMAdapterDiscover_t;

typedef struct _BTRMGR_IARMDDeviceProperty_t {
    unsigned char m_adapterIndex;
    BTRMgrDeviceHandle m_deviceHandle;
    BTRMGR_DevicesProperty_t m_deviceProperty;
} BTRMGR_IARMDDeviceProperty_t;

typedef struct _BTRMGR_IARMDiscoveredDevices_t{
    unsigned char m_adapterIndex;
    BTRMGR_DiscoveredDevicesList_t m_devices;
} BTRMGR_IARMDiscoveredDevices_t;

typedef struct _BTRMGR_IARMPairedDevices_t{
    unsigned char m_adapterIndex;
    BTRMGR_PairedDevicesList_t m_devices;
} BTRMGR_IARMPairedDevices_t;

typedef struct _BTRMGR_IARMConnectedDevices_t{
    unsigned char m_adapterIndex;
    BTRMGR_ConnectedDevicesList_t m_devices;
} BTRMGR_IARMConnectedDevices_t;

typedef struct _BTRMGR_IARMStreaming_t {
    unsigned char m_adapterIndex;
    BTRMgrDeviceHandle m_deviceHandle;
    BTRMGR_DeviceConnect_Type_t m_audioPref;
} BTRMGR_IARMStreaming_t;

typedef struct _BTRMGR_IARMStreamingStatus_t {
    unsigned char m_adapterIndex;
    unsigned char m_streamingStatus;
} BTRMGR_IARMStreamingStatus_t;

typedef struct _BTRMGR_IARMStreamingType_t {
    unsigned char m_adapterIndex;
    BTRMGR_StreamOut_Type_t m_audioOutType;
} BTRMGR_IARMStreamingType_t;

typedef struct _BTRMGR_IARMEventResp_t {
    unsigned char   m_adapterIndex;
    BTRMGR_EventResponse_t m_stBTRMgrEvtRsp;
} BTRMGR_IARMEventResp_t;


#endif /* __BT_MGR_IARM_INTERFACE_H__ */