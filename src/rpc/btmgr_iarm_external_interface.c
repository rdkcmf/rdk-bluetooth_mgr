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
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "btmgr.h"
#include "btmgr_priv.h"
#include "btmgr_iarm_interface.h"


static unsigned char isBTMGR_Inited = 0;
static BTMGR_EventCallback m_eventCallbackFunction = NULL;

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif

static void _btmgr_deviceCallback(const char *owner, IARM_EventId_t eventId, void *data, size_t len);

/*********************/
/* Public Interfaces */
/*********************/
BTMGR_Result_t BTMGR_Init()
{
    char processName[256] = "";

    if (!isBTMGR_Inited)
    {
        isBTMGR_Inited = 1;
#ifdef RDK_LOGGER_ENABLED
        const char* pDebugConfig = NULL;
        const char* BTMGR_IARM_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
        const char* BTMGR_IARM_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

        /* Init the logger */
        if( access(BTMGR_IARM_DEBUG_OVERRIDE_PATH, F_OK) != -1 )
            pDebugConfig = BTMGR_IARM_DEBUG_OVERRIDE_PATH;
        else
            pDebugConfig = BTMGR_IARM_DEBUG_ACTUAL_PATH;

        if( 0==rdk_logger_init(pDebugConfig))
	    b_rdk_logger_enabled = 1;
#endif        
        sprintf (processName, "BTMgr-User-%u", getpid());
        IARM_Bus_Init((const char*) &processName);
        IARM_Bus_Connect();

        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_PAIRING_FAILED, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST, _btmgr_deviceCallback);
        IARM_Bus_RegisterEventHandler(IARM_BUS_BTMGR_NAME, BTMGR_IARM_EVENT_DEVICE_FOUND, _btmgr_deviceCallback);
        BTMGRLOG_INFO ("IARM Interface Inited Successfully\n");
    }
    else
        BTMGRLOG_INFO ("IARM Interface Already Inited\n");

    return BTMGR_RESULT_SUCCESS;
}

BTMGR_Result_t BTMGR_DeInit()
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (isBTMGR_Inited)
    {
        /* This is leading to Crash the BTMgrBus which is listening; So lets not call this. */
        //retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "DeInit", 0, 0);
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    else
        BTMGRLOG_INFO ("IARM Interface for BTMgr is Not Inited Yet..\n");

    return rc;
}

BTMGR_Result_t BTMGR_GetNumberOfAdapters(unsigned char *pNumOfAdapters)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    unsigned char num_of_adapters = 0;
    
    if (!pNumOfAdapters)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetNumberOfAdapters", (void *)&num_of_adapters, sizeof(num_of_adapters));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pNumOfAdapters = num_of_adapters;
            BTMGRLOG_INFO ("Success; Number of Adapters = %d\n", num_of_adapters);
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterName(unsigned char index_of_adapter, const char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t adapterSetting;

    if((NULL == pNameOfAdapter) || (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        adapterSetting.m_adapterIndex = index_of_adapter;
        strncpy (adapterSetting.m_name, pNameOfAdapter, (BTMGR_NAME_LEN_MAX - 1));
        
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetAdapterName", (void *)&adapterSetting, sizeof(adapterSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterName(unsigned char index_of_adapter, char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t adapterSetting;

    if((NULL == pNameOfAdapter) || (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        adapterSetting.m_adapterIndex = index_of_adapter;
        memset (adapterSetting.m_name, '\0', sizeof (adapterSetting.m_name));

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetAdapterName", (void *)&adapterSetting, sizeof(adapterSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            strncpy (pNameOfAdapter, adapterSetting.m_name, (BTMGR_NAME_LEN_MAX - 1));
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char power_status)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t powerStatus;
    
    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (power_status > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        powerStatus.m_adapterIndex = index_of_adapter;
        powerStatus.m_powerStatus = power_status;

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetAdapterPowerStatus", (void *)&powerStatus, sizeof(powerStatus));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char *pPowerStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t powerStatus;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pPowerStatus))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        powerStatus.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetAdapterPowerStatus", (void *)&powerStatus, sizeof(powerStatus));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pPowerStatus = powerStatus.m_powerStatus;
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterDiscoverable(unsigned char index_of_adapter, unsigned char discoverable, unsigned short timeout)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t discoverableSetting;
    
    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (discoverable > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        discoverableSetting.m_adapterIndex = index_of_adapter;
        discoverableSetting.m_isDiscoverable = discoverable;
        discoverableSetting.m_timeout = timeout;

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetAdapterDiscoverable", (void *)&discoverableSetting, sizeof(discoverableSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_IsAdapterDiscoverable(unsigned char index_of_adapter, unsigned char *pDiscoverable)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t discoverableSetting;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pDiscoverable))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        discoverableSetting.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "IsAdapterDiscoverable", (void *)&discoverableSetting, sizeof(discoverableSetting));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pDiscoverable = discoverableSetting.m_isDiscoverable;
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StartDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscover_t deviceDiscovery;

    if (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        deviceDiscovery.m_adapterIndex = index_of_adapter;
        deviceDiscovery.m_setDiscovery = 1; /* TRUE */

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetDeviceDiscoveryStatus", (void *)&deviceDiscovery, sizeof(deviceDiscovery));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StopDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscover_t deviceDiscovery;

    if (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        deviceDiscovery.m_adapterIndex = index_of_adapter;
        deviceDiscovery.m_setDiscovery = 0; /* FALSE */

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetDeviceDiscoveryStatus", (void *)&deviceDiscovery, sizeof(deviceDiscovery));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetDiscoveredDevices(unsigned char index_of_adapter, BTMGR_DiscoveredDevicesList_t *pDiscoveredDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMDiscoveredDevices_t discoveredDevices;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pDiscoveredDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        memset (&discoveredDevices, 0, sizeof(discoveredDevices));
        discoveredDevices.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetDiscoveredDevices", (void *)&discoveredDevices, sizeof(discoveredDevices));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            memcpy (pDiscoveredDevices, &discoveredDevices.m_devices, sizeof(BTMGR_DiscoveredDevicesList_t));
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}


BTMGR_Result_t BTMGR_PairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t newDevice;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        memset (&newDevice, 0, sizeof(newDevice));
        newDevice.m_adapterIndex = index_of_adapter;
        newDevice.m_deviceHandle = handle;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "PairDevice", (void *)&newDevice, sizeof(newDevice));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_UnpairDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t removeDevice;
    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        memset (&removeDevice, 0, sizeof(removeDevice));
        removeDevice.m_adapterIndex = index_of_adapter;
        removeDevice.m_deviceHandle = handle;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "UnpairDevice", (void *)&removeDevice, sizeof(removeDevice));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetPairedDevices(unsigned char index_of_adapter, BTMGR_PairedDevicesList_t *pPairedDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMPairedDevices_t pairedDevices;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pPairedDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        memset (&pairedDevices, 0, sizeof(pairedDevices));
        pairedDevices.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetPairedDevices", (void *)&pairedDevices, sizeof(pairedDevices));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            memcpy (pPairedDevices, &pairedDevices.m_devices, sizeof(BTMGR_PairedDevicesList_t));
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_ConnectToDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DeviceConnect_Type_t connectAs)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t connectToDevice;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        connectToDevice.m_adapterIndex = index_of_adapter;
        connectToDevice.m_connectAs = connectAs;
        connectToDevice.m_deviceHandle = handle;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "ConnectToDevice", (void *)&connectToDevice, sizeof(connectToDevice));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_DisconnectFromDevice(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t disConnectToDevice;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        disConnectToDevice.m_adapterIndex = index_of_adapter;
        disConnectToDevice.m_deviceHandle = handle;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "DisconnectFromDevice", (void *)&disConnectToDevice, sizeof(disConnectToDevice));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetConnectedDevices(unsigned char index_of_adapter, BTMGR_ConnectedDevicesList_t *pConnectedDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMConnectedDevices_t connectedDevices;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pConnectedDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        memset (&connectedDevices, 0, sizeof(connectedDevices));
        connectedDevices.m_adapterIndex = index_of_adapter;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetConnectedDevices", (void *)&connectedDevices, sizeof(connectedDevices));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            memcpy (pConnectedDevices, &connectedDevices.m_devices, sizeof(BTMGR_ConnectedDevicesList_t));
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}


BTMGR_Result_t BTMGR_GetDeviceProperties(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DevicesProperty_t *pDeviceProperty)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMDDeviceProperty_t deviceProperty;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle) || (NULL == pDeviceProperty))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        deviceProperty.m_adapterIndex = index_of_adapter;
        deviceProperty.m_deviceHandle = handle;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "GetDeviceProperties", (void *)&deviceProperty, sizeof(deviceProperty));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            memcpy (pDeviceProperty, &deviceProperty.m_deviceProperty, sizeof(BTMGR_DevicesProperty_t));
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_RegisterEventCallback(BTMGR_EventCallback eventCallback)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    if (!eventCallback)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        m_eventCallbackFunction = eventCallback;
        BTMGRLOG_INFO ("Success\n");
    }
    return rc;
}

BTMGR_Result_t BTMGR_StartAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle, BTMGR_DeviceConnect_Type_t streamOutPref)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMStreaming_t streaming;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        streaming.m_adapterIndex = index_of_adapter;
        streaming.m_deviceHandle = handle;
        streaming.m_audioPref = streamOutPref;

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "StartAudioStreaming", (void *)&streaming, sizeof(streaming));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StopAudioStreamingOut(unsigned char index_of_adapter, BTMgrDeviceHandle handle)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMStreaming_t streaming;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        streaming.m_adapterIndex = index_of_adapter;
        streaming.m_deviceHandle = handle;

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "StopAudioStreaming", (void*) &streaming, sizeof(streaming));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_IsAudioStreamingOut(unsigned char index_of_adapter, unsigned char *pStreamingStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMStreamingStatus_t status;

    if ((BTMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pStreamingStatus))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        status.m_adapterIndex = index_of_adapter;
        status.m_streamingStatus = 0;
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "IsAudioStreaming", (void *)&status, sizeof(status));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            *pStreamingStatus = status.m_streamingStatus;
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAudioStreamingOutType(unsigned char index_of_adapter, BTMGR_StreamOut_Type_t type)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_IARMStreamingType_t streamingType;

    if (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        streamingType.m_adapterIndex = index_of_adapter;
        streamingType.m_audioOutType = type;

        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "SetAudioStreamOutType", (void *)&streamingType, sizeof(streamingType));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_ResetAdapter(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (BTMGR_ADAPTER_COUNT_MAX < index_of_adapter)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else
    {
        retCode = IARM_Bus_Call(IARM_BUS_BTMGR_NAME, "ResetAdapter", (void *)&index_of_adapter, sizeof(index_of_adapter));
        if (IARM_RESULT_SUCCESS == retCode)
        {
            BTMGRLOG_INFO ("Success\n");
        }
        else
        {
            rc = BTMGR_RESULT_GENERIC_FAILURE;
            BTMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
        }
    }
    return rc;
}

const char* BTMGR_GetDeviceTypeAsString(BTMGR_DeviceType_t type)
{
    if (type == BTMGR_DEVICE_TYPE_WEARABLE_HEADSET)
        return "WEARABLE HEADSET";
    else if (type == BTMGR_DEVICE_TYPE_HANDSFREE)
        return "HANDSFREE";
    else if (type == BTMGR_DEVICE_TYPE_MICROPHONE)
        return "MICROPHONE";
    else if (type == BTMGR_DEVICE_TYPE_LOUDSPEAKER)
        return "LOUDSPEAKER";
    else if (type == BTMGR_DEVICE_TYPE_HEADPHONES)
        return "HEADPHONES";
    else if (type == BTMGR_DEVICE_TYPE_PORTABLE_AUDIO)
        return "PORTABLE AUDIO DEVICE";
    else if (type == BTMGR_DEVICE_TYPE_CAR_AUDIO)
        return "CAR AUDIO";
    else if (type == BTMGR_DEVICE_TYPE_STB)
        return "STB";
    else if (type == BTMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)
        return "HIFI AUDIO DEVICE";
    else if (type == BTMGR_DEVICE_TYPE_VCR)
        return "VCR";
    else if (type == BTMGR_DEVICE_TYPE_VIDEO_CAMERA)
        return "VIDEO CAMERA";
    else if (type == BTMGR_DEVICE_TYPE_CAMCODER)
        return "CAMCODER";
    else if (type == BTMGR_DEVICE_TYPE_VIDEO_MONITOR)
        return "VIDEO MONITOR";
    else if (type == BTMGR_DEVICE_TYPE_TV)
        return "TV";
    else if (type == BTMGR_DEVICE_TYPE_VIDEO_CONFERENCE)
        return "VIDEO CONFERENCING";
    else
        return "UNKNOWN DEVICE";
}

/**********************/
/* Private Interfaces */
/**********************/
static void
_btmgr_deviceCallback (
    const char*     owner,
    IARM_EventId_t  eventId,
    void*           pData,
    size_t          len
) {
    if (NULL == pData) {
        BTMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        BTMGR_EventMessage_t *pReceivedEvent = (BTMGR_EventMessage_t*)pData;
        BTMGR_EventMessage_t newEvent;

        if ((BTMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE        == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE    == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE  == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE    == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE  == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_PAIRING_FAILED      == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED    == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED   == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED   == eventId) ||
            (BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST == eventId) ||
            (BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST    == eventId) ||
            (BTMGR_IARM_EVENT_DEVICE_FOUND               == eventId)) {

            memcpy (&newEvent, pReceivedEvent, sizeof(BTMGR_EventMessage_t));

            if (m_eventCallbackFunction)
                m_eventCallbackFunction (newEvent);
              
            BTMGRLOG_INFO ("posted event(%d) from the adapter(%d) to listener successfully\n", newEvent.m_eventType, newEvent.m_adapterIndex);

            if (BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE == eventId) {
                BTMGRLOG_INFO("Name = %s\n\n", newEvent.m_discoveredDevice.m_name);
            }
        }
        else {
            BTMGRLOG_ERROR ("Event is invalid\n");
        }
    }
    
    return;
}

