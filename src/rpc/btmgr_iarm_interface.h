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


#define IARM_BUS_BTMGR_NAME        "BTMgrBus"

typedef enum _BTMGR_IARMEvents_t {
    BTMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE,
    BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE,
    BTMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE,
    BTMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE,
    BTMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE,
    BTMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE,
    BTMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE,
    BTMGR_IARM_EVENT_DEVICE_PAIRING_FAILED,
    BTMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED,
    BTMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED,
    BTMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED,
    BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST,
    BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST,
    BTMGR_IARM_EVENT_DEVICE_FOUND,
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

typedef struct _BTMGR_IARMDiscoveredDevices_t{
    unsigned char m_adapterIndex;
    BTMGR_DiscoveredDevicesList_t m_devices;
} BTMGR_IARMDiscoveredDevices_t;

typedef struct _BTMGR_IARMPairedDevices_t{
    unsigned char m_adapterIndex;
    BTMGR_PairedDevicesList_t m_devices;
} BTMGR_IARMPairedDevices_t;

typedef struct _BTMGR_IARMConnectedDevices_t{
    unsigned char m_adapterIndex;
    BTMGR_ConnectedDevicesList_t m_devices;
} BTMGR_IARMConnectedDevices_t;

typedef struct _BTMGR_IARMStreaming_t {
    unsigned char m_adapterIndex;
    BTMgrDeviceHandle m_deviceHandle;
    BTMGR_DeviceConnect_Type_t m_audioPref;
} BTMGR_IARMStreaming_t;

typedef struct _BTMGR_IARMStreamingStatus_t {
    unsigned char m_adapterIndex;
    unsigned char m_streamingStatus;
} BTMGR_IARMStreamingStatus_t;

typedef struct _BTMGR_IARMStreamingType_t {
    unsigned char m_adapterIndex;
    BTMGR_StreamOut_Type_t m_audioOutType;
} BTMGR_IARMStreamingType_t;

void btmgr_BeginIARMMode();
void btmgr_TermIARMMode();

#endif /* __BT_MGR_IARM_INTERFACE_H__ */
