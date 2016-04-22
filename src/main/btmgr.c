#include "btmgr.h"
#include "btmgr_priv.h"
#include "btrCore.h"
#include <unistd.h>

tBTRCoreHandle gBTRCoreHandle = NULL;
stBTRCoreGetAdapter gDefaultAdapterContext;
stBTRCoreListAdapters gListOfAdapters;
unsigned char gIsDiscoveryInProgress = 0;
unsigned char gIsDeviceConnected = 0;
unsigned char gIsStreamoutInProgress = 0;
char gPreferredDeviceToStreamOut[BTMGR_NAME_LEN_MAX] = "";

BTMGR_StreamOut_Type_t gStreamoutType;

const char* BTMGR_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
const char* BTMGR_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

static BTMGR_EventCallback m_eventCallbackFunction = NULL;

/* Private function declarations */
void set_discovery_status (unsigned char status)
{
    gIsDiscoveryInProgress = status;
}

unsigned char isDiscoveryInProgress(void)
{
    return gIsDiscoveryInProgress;
}

void btmgr_DeviceDiscoveryCallback (stBTRCoreScannedDevicesCount devicefound)
{
    if (isDiscoveryInProgress())
    {
        int i = 0;
        BTMGR_EventMessage_t newEvent;
        memset (&newEvent, 0, sizeof(newEvent));
        
        newEvent.m_adapterIndex = 0;
        newEvent.m_eventType = BTMGR_EVENT_DEVICE_DISCOVERY_UPDATE;
        newEvent.m_eventData.m_numOfDevices = devicefound.numberOfDevices;
        for (i = 0; i < devicefound.numberOfDevices; i++)
        {
            strncpy (newEvent.m_eventData.m_deviceProperty[i].m_name, devicefound.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
            strncpy (newEvent.m_eventData.m_deviceProperty[i].m_deviceAddress, devicefound.devices[i].bd_address, (BTMGR_NAME_LEN_MAX - 1));
            newEvent.m_eventData.m_deviceProperty[i].m_rssi = devicefound.devices[i].RSSI;
        }

        /*  Post a callback */
        m_eventCallbackFunction (newEvent);
    }
    return;
}

int btmgr_DeviceStatusCallback (stBTRCoreDevStateCB* p_StatusCB)
{
    return 0;
}

unsigned char getAdaptherCount (void)
{
    unsigned char numbers = 0;

    numbers = gListOfAdapters.number_of_adapters;

    return numbers;
}

const char* getAdaptherPath (unsigned char index_of_adapter)
{
    const char* pReturn = NULL;

    if (gListOfAdapters.number_of_adapters)
    {
        if ((index_of_adapter < gListOfAdapters.number_of_adapters) &&
            (index_of_adapter < BTRCORE_MAX_NUM_BT_DEVICES))
        {
            pReturn = gListOfAdapters.adapter_path[index_of_adapter];
        }
    }

    return pReturn;
}

/* Public Functions */
BTMGR_Result_t BTMGR_Init()
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    const char* pDebugConfig = NULL;

    if (NULL == gBTRCoreHandle)
    {
        /* Init the logger */
        if( access(BTMGR_DEBUG_OVERRIDE_PATH, F_OK) != -1 )
            pDebugConfig = BTMGR_DEBUG_OVERRIDE_PATH;
        else
            pDebugConfig = BTMGR_DEBUG_ACTUAL_PATH;

        rdk_logger_init(pDebugConfig);

        /* Initialze all the database */
        memset (&gDefaultAdapterContext, 0, sizeof(gDefaultAdapterContext));
        memset (&gListOfAdapters, 0, sizeof(gListOfAdapters));
        gIsDiscoveryInProgress = 0;

        /* Init the mutex */

        /* Call the Core/HAL init */
        halrc = BTRCore_Init(&gBTRCoreHandle);
        if ((NULL == gBTRCoreHandle) || (enBTRCoreSuccess != halrc))
        {
            BTMGRLOG_ERROR ("BTMGR_Init : Could not initialize BTRCore/HAL module");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
        else
        {
            if (enBTRCoreSuccess != BTRCore_GetListOfAdapters (gBTRCoreHandle, &gListOfAdapters))
                BTMGRLOG_ERROR ("Failed to get the total number of Adapters present"); /* Not a Error case anyway */

            BTMGRLOG_INFO ("Number of Adapters found are = %u", gListOfAdapters.number_of_adapters);
            if (0 == gListOfAdapters.number_of_adapters)
            {
                BTMGRLOG_WARN("Bluetooth adapter NOT Found! Expected to be connected to make use of this module; Not considered as failure");
            }
            else
            {
                /* you have atlesat one BlueTooth adapter. Now get the Default Adapter path for future usages; */
                gDefaultAdapterContext.first_available = 1; /* This is unused by core now but lets fill it */
                if (enBTRCoreSuccess == BTRCore_GetAdapter(gBTRCoreHandle, &gDefaultAdapterContext))
                {
                    BTMGRLOG_INFO ("Aquired default Adapter; Path is %s", gDefaultAdapterContext.pcAdapterPath);
                }

                /* TODO: Handling multiple Adapters */
                if (gListOfAdapters.number_of_adapters > 1)
                    BTMGRLOG_WARN("There are more than 1 Bluetooth Adapters Found! Lets handle it properly");
            }

            /* Register for callback to get the status of connected Devices */
            BTRCore_RegisterStatusCallback(gBTRCoreHandle, btmgr_DeviceStatusCallback);

            /* Register for callback to get the Discovered Devices */
            BTRCore_RegisterDiscoveryCallback(gBTRCoreHandle, btmgr_DeviceDiscoveryCallback);
        }
    }
    else
    {
        BTMGRLOG_WARN("BTMGR_Init : Already Inited; Return Success");
    }

    return rc;
}

BTMGR_Result_t BTMGR_GetNumberOfAdapters(unsigned char *pNumOfAdapters)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCoreListAdapters listOfAdapters;

    memset (&listOfAdapters, 0, sizeof(listOfAdapters));
    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if (NULL == pNumOfAdapters)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        halrc = BTRCore_GetListOfAdapters (gBTRCoreHandle, &listOfAdapters);
        if (enBTRCoreSuccess == halrc)
        {
            *pNumOfAdapters = listOfAdapters.number_of_adapters;
            /* Copy to our backup */
            if (listOfAdapters.number_of_adapters != gListOfAdapters.number_of_adapters)
                memcpy (&gListOfAdapters, &listOfAdapters, sizeof (stBTRCoreListAdapters));

            BTMGRLOG_INFO ("BTMGR_GetNumberOfAdapters : Available Adapters = %d", listOfAdapters.number_of_adapters);
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetNumberOfAdapters : Could not find Adapters");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterName(unsigned char index_of_adapter, const char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    char name[BTMGR_NAME_LEN_MAX];
    memset(name, '\0', sizeof(name));

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((NULL == pNameOfAdapter) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            strncpy (name, pNameOfAdapter, (BTMGR_NAME_LEN_MAX - 1));
            halrc = BTRCore_SetAdapterName(gBTRCoreHandle, pAdapterPath, name);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_SetAdapterName : Failed to set Adapter Name");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_SetAdapterName : Set Successfully");
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_SetAdapterName : Failed to adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterName(unsigned char index_of_adapter, char* pNameOfAdapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    char name[BTMGR_NAME_LEN_MAX];
    memset(name, '\0', sizeof(name));

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((NULL == pNameOfAdapter) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);

        if (pAdapterPath)
        {
            halrc = BTRCore_GetAdapterName(gBTRCoreHandle, pAdapterPath, name);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to get Adapter Name");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_GetAdapterName : Fetched Successfully");

            /*  Copy regardless of success or failure. */
            strncpy (name, pNameOfAdapter, (BTMGR_NAME_LEN_MAX - 1));
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char power_status)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (power_status > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_SetAdapterPower(gBTRCoreHandle, pAdapterPath, power_status);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_SetAdapterPowerStatus : Failed to set Adapter Power Status");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_SetAdapterPowerStatus : Set Successfully");
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_SetAdapterPowerStatus : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_GetAdapterPowerStatus(unsigned char index_of_adapter, unsigned char *pPowerStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    unsigned char power_status = 0;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((NULL == pPowerStatus) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        
        if (pAdapterPath)
        {
            halrc = BTRCore_GetAdapterPower(gBTRCoreHandle, pAdapterPath, &power_status);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to get Adapter Power");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_GetAdapterName : Fetched Successfully");
                *pPowerStatus = power_status;
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetAdapterName : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_SetAdapterDiscoverable(unsigned char index_of_adapter, unsigned char discoverable, unsigned short timeout)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (discoverable > 1))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);

        if (pAdapterPath)
        {
            if (timeout)
            {
                halrc = BTRCore_SetAdapterDiscoverableTimeout(gBTRCoreHandle, pAdapterPath, timeout);
                if (enBTRCoreSuccess != halrc)
                    BTMGRLOG_ERROR ("BTMGR_SetAdapterDiscoverable : Failed to set Adapter discovery timeout");
                else
                    BTMGRLOG_INFO ("BTMGR_SetAdapterDiscoverable : Set timeout Successfully");
            }

            /* Set the  discoverable state */
            halrc = BTRCore_SetAdapterDiscoverable(gBTRCoreHandle, pAdapterPath, discoverable);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_SetAdapterDiscoverable : Failed to set Adapter discoverable status");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_SetAdapterDiscoverable : Set discoverable status Successfully");
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_SetAdapterDiscoverable : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_IsAdapterDiscoverable(unsigned char index_of_adapter, unsigned char *pDiscoverable)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    unsigned char discoverable = 0;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((NULL == pDiscoverable) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        
        if (pAdapterPath)
        {
            halrc = BTRCore_GetAdapterDiscoverableStatus(gBTRCoreHandle, pAdapterPath, &discoverable);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_IsAdapterDiscoverable : Failed to get Adapter Status");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_IsAdapterDiscoverable : Fetched Successfully");
                *pDiscoverable = discoverable;
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_IsAdapterDiscoverable : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }

    return rc;
}

BTMGR_Result_t BTMGR_StartDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    if (isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is already in progress");
    }
    else if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if(index_of_adapter > getAdaptherCount())
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_StartDeviceDiscovery(gBTRCoreHandle, pAdapterPath);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_StartDeviceDiscovery : Failed to start discovery");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_StartDeviceDiscovery : Discovery started Successfully");
                set_discovery_status(1);
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_StartDeviceDiscovery : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StopDeviceDiscovery(unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    if (!isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is not running now");
    }
    else if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if(index_of_adapter > getAdaptherCount())
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_StopDeviceDiscovery(gBTRCoreHandle, pAdapterPath);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_StopDeviceDiscovery : Failed to stop discovery");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_StopDeviceDiscovery : Discovery Stopped Successfully");
                set_discovery_status(0);
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_StopDeviceDiscovery : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_PairDevice(unsigned char index_of_adapter, const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    if (isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is still running now; Lets stop it");
        BTMGR_StopDeviceDiscovery(index_of_adapter);
    }

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (!pNameOfDevice))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_PairDeviceByName(gBTRCoreHandle, pAdapterPath, pNameOfDevice);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_PairDevice : Failed to pair a device");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
                BTMGRLOG_INFO ("BTMGR_PairDevice : Paired Successfully");
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_PairDevice : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetPairedDevices(unsigned char index_of_adapter, BTMGR_Devices_t *pDiscoveredDevices)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfDevices;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (!pDiscoveredDevices))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            memset (&listOfDevices, 0, sizeof(listOfDevices));
            halrc = BTRCore_GetListOfPairedDevices(gBTRCoreHandle, pAdapterPath, &listOfDevices);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_GetPairedDevices : Failed to get list of paired devices");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                /* Reset the values to 0 */
                memset (pDiscoveredDevices, 0, sizeof(BTMGR_Devices_t));
                if (listOfDevices.numberOfDevices)
                {
                    int i = 0;
                    BTMGR_DevicesProperty_t *ptr = NULL;
                    pDiscoveredDevices->m_numOfDevices = listOfDevices.numberOfDevices;
                    for (i = 0; i < listOfDevices.numberOfDevices; i++)
                    {
                        ptr = &pDiscoveredDevices->m_deviceProperty[i];
                        strncpy(ptr->m_name, listOfDevices.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
                        strncpy(ptr->m_deviceAddress, listOfDevices.devices[i].bd_path, (BTMGR_NAME_LEN_MAX - 1));
                    }
                }
                else
                    BTMGRLOG_WARN("No Device is paired yet");

                /*  Success */
                BTMGRLOG_INFO ("BTMGR_GetPairedDevices : Successful");
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetPairedDevices : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_UnpairDevice(unsigned char index_of_adapter, const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if ((!pNameOfDevice) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            halrc = BTRCore_UnPairDeviceByName(gBTRCoreHandle, pAdapterPath, pNameOfDevice);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_UnpairDevice : Failed to stop discovery");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_UnpairDevice : Discovery Stopped Successfully");
                set_discovery_status(0);
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_UnpairDevice : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_ConnectToDevice(unsigned char index_of_adapter, const char* pNameOfDevice, BTMGR_Device_Type_t connectAs)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (isDiscoveryInProgress())
    {
        BTMGRLOG_WARN("Scanning is still running now; Lets stop it");
        BTMGR_StopDeviceDiscovery(index_of_adapter);
    }

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if ((!pNameOfDevice) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            /* connectAs param is unused for now.. */
            halrc = BTRCore_ConnectDeviceByName(gBTRCoreHandle, pAdapterPath, pNameOfDevice, enBTRCoreSpeakers);
            if (enBTRCoreSuccess != halrc)
            {
                BTMGRLOG_ERROR ("BTMGR_ConnectToDevice : Failed to stop discovery");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
            else
            {
                BTMGRLOG_INFO ("BTMGR_ConnectToDevice : Discovery Stopped Successfully");
                gIsDeviceConnected = 1;
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_ConnectToDevice: Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_DisconnectFromDevice(unsigned char index_of_adapter, const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if ((!pNameOfDevice) || (index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else if (0 == gIsDeviceConnected)
    {
        BTMGRLOG_ERROR ("%s : No Device is connected at this time", __FUNCTION__);
        rc = BTMGR_RESULT_GENERIC_FAILURE;
    }
    else
    {
        if (gIsStreamoutInProgress)
        {
            /* The streaming is happening; stop it */
            rc = BTMGR_StopAudioStreamingOut();
            if (BTMGR_RESULT_SUCCESS != rc)
                BTMGRLOG_ERROR ("%s : Streamout is failed to stop", __FUNCTION__);
        }

        /* connectAs param is unused for now.. */
        halrc = BTRCore_DisconnectDeviceByName(gBTRCoreHandle, pNameOfDevice, enBTRCoreSpeakers);
        if (enBTRCoreSuccess != halrc)
        {
            BTMGRLOG_ERROR ("BTMGR_DisconnectFromDevice : Failed to stop discovery");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
        }
        else
        {
            BTMGRLOG_INFO ("BTMGR_DisconnectFromDevice : Discovery Stopped Successfully");
            gIsDeviceConnected = 0;
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_GetDeviceProperties(unsigned char index_of_adapter, const char* pNameOfDevice, BTMGR_DevicesProperty_t *pDeviceProperty)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    enBTRCoreRet halrc = enBTRCoreSuccess;
    stBTRCorePairedDevicesCount listOfPDevices;
    stBTRCoreScannedDevicesCount listOfSDevices;
    unsigned char isFound = 0;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()) || (!pDeviceProperty) || (!pNameOfDevice))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        const char* pAdapterPath = getAdaptherPath (index_of_adapter);
        if (pAdapterPath)
        {
            /* Reset the values to 0 */
            memset (&listOfPDevices, 0, sizeof(listOfPDevices));
            memset (&listOfSDevices, 0, sizeof(listOfSDevices));
            memset (pDeviceProperty, 0, sizeof(BTMGR_DevicesProperty_t));

            halrc = BTRCore_GetListOfScannedDevices (gBTRCoreHandle, pAdapterPath, &listOfSDevices);
            if (enBTRCoreSuccess == halrc)
            {
                if (listOfSDevices.numberOfDevices)
                {
                    int i = 0;
                    for (i = 0; i < listOfSDevices.numberOfDevices; i++)
                    {
                        if (0 == strcmp(pNameOfDevice, listOfSDevices.devices[i].device_name))
                        {
                            strncpy(pDeviceProperty->m_name, listOfSDevices.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
                            strncpy(pDeviceProperty->m_deviceAddress, listOfSDevices.devices[i].bd_address, (BTMGR_NAME_LEN_MAX - 1));
                            pDeviceProperty->m_rssi = listOfSDevices.devices[i].RSSI;
                            isFound = 1;
                            break;
                        }
                    }
                }
                else
                    BTMGRLOG_WARN("No Device in scan list");
            }

            if (!isFound)
            {
                halrc = BTRCore_GetListOfPairedDevices(gBTRCoreHandle, pAdapterPath, &listOfPDevices);
                if (enBTRCoreSuccess == halrc)
                {
                    if (listOfPDevices.numberOfDevices)
                    {
                        int i = 0;
                        for (i = 0; i < listOfPDevices.numberOfDevices; i++)
                        {
                            if (0 == strcmp(pNameOfDevice, listOfPDevices.devices[i].device_name))
                            {
                                strncpy(pDeviceProperty->m_name, listOfPDevices.devices[i].device_name, (BTMGR_NAME_LEN_MAX - 1));
                                strncpy(pDeviceProperty->m_deviceAddress, listOfPDevices.devices[i].bd_path, (BTMGR_NAME_LEN_MAX - 1));
                                pDeviceProperty->m_rssi = listOfPDevices.devices[i].RSSI; /* Will show the strength as low */
                                isFound = 1;
                                break;
                            }
                        }
                    }
                    else
                        BTMGRLOG_WARN("No Device is paired yet");
                }
            }
            if (!isFound)
            {
                BTMGRLOG_ERROR ("BTMGR_GetDeviceProperties : Could not retrive info for this device");
                rc = BTMGR_RESULT_GENERIC_FAILURE;
            }
        }
        else
        {
            BTMGRLOG_ERROR ("BTMGR_GetDeviceProperties : Failed to get adapter path");
            rc = BTMGR_RESULT_GENERIC_FAILURE;
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
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        m_eventCallbackFunction = eventCallback;
        BTMGRLOG_INFO ("BTMGR_RegisterEventCallback : Success");
    }
    return rc;
}

BTMGR_Result_t BTMGR_DeInit(void)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else
    {
        BTRCore_DeInit(gBTRCoreHandle);
        BTMGRLOG_ERROR ("%s : BTRCore DeInited; Now will we exit the app", __FUNCTION__);
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetDefaultDeviceToStreamOut(const char* pNameOfDevice)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if (pNameOfDevice)
    {
        /* The Expectation is, the name that is given must be one of the paired device */
        strncpy(gPreferredDeviceToStreamOut, pNameOfDevice, (BTMGR_NAME_LEN_MAX - 1));
        BTMGRLOG_INFO ("BTMGR_SetDefaultDeviceToStreamOut: Successfully saved preferred Device %s", gPreferredDeviceToStreamOut);
    }
    else
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    return rc;
}

BTMGR_Result_t BTMGR_SetPreferredAudioStreamOutType(BTMGR_StreamOut_Type_t stream_pref)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    if ((stream_pref == BTMGR_STREAM_PRIMARY) || (stream_pref == BTMGR_STREAM_SECONDARY))
    {
        if (gStreamoutType != stream_pref)
        {
            gStreamoutType = stream_pref;
            BTMGRLOG_INFO ("BTMGR_SetPreferredAudioStreamOutType: Successfully set to %d", gStreamoutType);
        }
        else
            BTMGRLOG_INFO ("BTMGR_SetPreferredAudioStreamOutType: Already in the given state");
    }
    else
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    return rc;
}

BTMGR_Result_t BTMGR_StartAudioStreamingOut (unsigned char index_of_adapter)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if (NULL == gBTRCoreHandle)
    {
        rc = BTMGR_RESULT_INIT_FAILED;
        BTMGRLOG_ERROR ("%s : BTRCore is not Inited", __FUNCTION__);
    }
    else if((index_of_adapter > getAdaptherCount()))
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else if (gPreferredDeviceToStreamOut[0] == '\0')
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Preferred Device is not set yet", __FUNCTION__);
    }
    else
    {
        if (0 == gIsDeviceConnected)
        {
            /* If the device is not connected, Connect to it */
            rc = BTMGR_ConnectToDevice(index_of_adapter, gPreferredDeviceToStreamOut, BTMGR_DEVICE_TYPE_AUDIOSINK);
        }

        if (BTMGR_RESULT_SUCCESS == rc)
        {
            /* TODO: Start doing one time init of StreamOut Libs and call it for playing out */
            gIsStreamoutInProgress = 1;
        }
        else
        {
            BTMGRLOG_ERROR ("%s : Failed to connect to device and not playing", __FUNCTION__);
        }
    }
    return rc;
}

BTMGR_Result_t BTMGR_StopAudioStreamingOut(void)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if (gIsStreamoutInProgress)
    {
        /* TODO: Stop StreamOut */
        gIsStreamoutInProgress = 1;
    }

    return rc;
}

BTMGR_Result_t BTMGR_IsAudioStreamingOut(unsigned char index_of_adapter, unsigned char *pStreamingStatus)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if(!pStreamingStatus)
    {
        rc = BTMGR_RESULT_INVALID_INPUT;
        BTMGRLOG_ERROR ("%s : Input is invalid", __FUNCTION__);
    }
    else
    {
        *pStreamingStatus = gIsStreamoutInProgress;
        BTMGRLOG_INFO ("BTMGR_IsAudioStreamingOut: Returned status Successfully");
    }

    return rc;
}

/* End of File */
