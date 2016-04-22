#include "btmgr_priv.h"
#include "btmgr_iarm_interface.h"

#ifdef BTMGR_ENABLE_IARM_INTERFACE
static unsigned char gIsBTMGR_Internal_Inited = 0;

static IARM_Result_t _GetNumberOfAdapters(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    unsigned char numOfAdapters = 0;
    unsigned char *pNumberOfAdapters = (unsigned char*) arg;

    BTMGRLOG_INFO ("_GetNumberOfAdapters : Entering %s", __FUNCTION__);
    if (gIsBTMGR_Internal_Inited)
    {
        if (pNumberOfAdapters)
        {
            rc = BTMGR_GetNumberOfAdapters(&numOfAdapters);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                *pNumberOfAdapters = numOfAdapters;
                BTMGRLOG_INFO ("_GetNumberOfAdapters : Success; Number of Adapters = %d", numOfAdapters);
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_GetNumberOfAdapters : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_GetNumberOfAdapters : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _SetAdapterName(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t *pName = (BTMGR_IARMAdapterName_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pName)
        {
            rc = BTMGR_SetAdapterName(pName->m_adapterIndex, pName->m_name);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_SetAdapterName : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_SetAdapterName : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_SetAdapterName : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _GetAdapterName(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t *pName = (BTMGR_IARMAdapterName_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pName)
        {
            rc = BTMGR_GetAdapterName(pName->m_adapterIndex, pName->m_name);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_GetAdapterName : Success ; Adapter name is %s", pName->m_name);
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_GetAdapterName : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_GetAdapterName : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _SetAdapterPowerStatus(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t *pPowerStatus = (BTMGR_IARMAdapterPower_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pPowerStatus)
        {
            rc = BTMGR_SetAdapterPowerStatus(pPowerStatus->m_adapterIndex, pPowerStatus->m_powerStatus);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_SetAdapterPowerStatus : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_SetAdapterPowerStatus : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_SetAdapterPowerStatus : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _GetAdapterPowerStatus(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t *pPowerStatus = (BTMGR_IARMAdapterPower_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pPowerStatus)
        {
            rc = BTMGR_GetAdapterPowerStatus(pPowerStatus->m_adapterIndex, &pPowerStatus->m_powerStatus);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_GetAdapterPowerStatus : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_GetAdapterPowerStatus : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_GetAdapterPowerStatus : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _SetAdapterDiscoverable(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t *pDiscoverable = (BTMGR_IARMAdapterDiscoverable_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pDiscoverable)
        {
            rc = BTMGR_SetAdapterDiscoverable(pDiscoverable->m_adapterIndex, pDiscoverable->m_isDiscoverable, pDiscoverable->m_timeout);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_SetAdapterDiscoverable : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_SetAdapterDiscoverable : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_SetAdapterDiscoverable : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _IsAdapterDiscoverable(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t *pDiscoverable = (BTMGR_IARMAdapterDiscoverable_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pDiscoverable)
        {
            rc = BTMGR_IsAdapterDiscoverable(pDiscoverable->m_adapterIndex, &pDiscoverable->m_isDiscoverable);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_IsAdapterDiscoverable : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_IsAdapterDiscoverable : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_IsAdapterDiscoverable : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _ChangeDeviceDiscoveryStatus(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscover_t *pAdapterIndex = (BTMGR_IARMAdapterDiscover_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pAdapterIndex)
        {
            if (pAdapterIndex->m_setDiscovery)
                rc = BTMGR_StartDeviceDiscovery(pAdapterIndex->m_adapterIndex);
            else
                rc = BTMGR_StopDeviceDiscovery(pAdapterIndex->m_adapterIndex);

            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_ChangeDeviceDiscoveryStatus : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_ChangeDeviceDiscoveryStatus : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_ChangeDeviceDiscoveryStatus : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _PairDevice(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t *pPairDevice = (BTMGR_IARMPairDevice_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pPairDevice)
        {
            rc = BTMGR_PairDevice(pPairDevice->m_adapterIndex, pPairDevice->m_name);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_PairDevice : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_PairDevice : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_PairDevice : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _UnpairDevice(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t *pUnPairDevice = (BTMGR_IARMPairDevice_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pUnPairDevice)
        {
            rc = BTMGR_UnpairDevice(pUnPairDevice->m_adapterIndex, pUnPairDevice->m_name);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_UnpairDevice : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_UnpairDevice : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_UnpairDevice : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _GetPairedDevices(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMDevices_t *pPairedDevices = (BTMGR_IARMDevices_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pPairedDevices)
        {
            rc = BTMGR_GetPairedDevices(pPairedDevices->m_adapterIndex, &pPairedDevices->m_devices);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_GetPairedDevices : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_GetPairedDevices : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_GetPairedDevices : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _ConnectToDevice(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t *pConnect = (BTMGR_IARMConnectDevice_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pConnect)
        {
            rc = BTMGR_ConnectToDevice(pConnect->m_adapterIndex, pConnect->m_name, pConnect->m_connectAs);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_ConnectToDevice : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_ConnectToDevice : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_ConnectToDevice : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _DisconnectFromDevice(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t *pConnect = (BTMGR_IARMConnectDevice_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pConnect)
        {
            rc = BTMGR_DisconnectFromDevice(pConnect->m_adapterIndex, pConnect->m_name);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_DisconnectFromDevice : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_DisconnectFromDevice : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_DisconnectFromDevice : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _GetDeviceProperties(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMDDeviceProperty_t *pDeviceProperty = (BTMGR_IARMDDeviceProperty_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pDeviceProperty)
        {
            rc = BTMGR_GetDeviceProperties(pDeviceProperty->m_adapterIndex, pDeviceProperty->m_name, &pDeviceProperty->m_deviceProperty);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_GetDeviceProperties : Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_GetDeviceProperties : Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_GetDeviceProperties : Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _DeInit(void *arg)
{
    if (gIsBTMGR_Internal_Inited)
        BTMGR_DeInit();

    return IARM_RESULT_SUCCESS;
}

static IARM_Result_t _DeviceToStream(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    char *pName = (char*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pName)
        {
            rc = BTMGR_SetDefaultDeviceToStreamOut(pName);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_DeviceToStream: Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_DeviceToStream: Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_DeviceToStream: Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _SetPreferredAudio(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMPrefAudioType_t *pStreamType =  (BTMGR_IARMPrefAudioType_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pStreamType)
        {
            rc = BTMGR_SetPreferredAudioStreamOutType(pStreamType->m_audioPref);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_SetPreferredAudio: Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_SetPreferredAudio: Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_SetPreferredAudio: Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _StartAudioStreaming(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMStartStreaming_t *pStartStream =  (BTMGR_IARMStartStreaming_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pStartStream)
        {
            rc = BTMGR_StartAudioStreamingOut(pStartStream->m_adapterIndex);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_StartAudioStreaming: Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_StartAudioStreaming: Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_StartAudioStreaming: Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _StopAudioStreaming(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    if (gIsBTMGR_Internal_Inited)
    {
        rc = BTMGR_StopAudioStreamingOut();
        if (BTMGR_RESULT_SUCCESS == rc)
        {
            BTMGRLOG_INFO ("_StopAudioStreaming: Success");
        }
        else
        {
            retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
            BTMGRLOG_ERROR ("_StopAudioStreaming: Failed; RetCode = %d", rc);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

static IARM_Result_t _IsAudioStreaming(void *arg)
{
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMStreamingStatus_t *pStreamStatus =  (BTMGR_IARMStreamingStatus_t*) arg;

    if (gIsBTMGR_Internal_Inited)
    {
        if (pStreamStatus)
        {
            rc = BTMGR_IsAudioStreamingOut(pStreamStatus->m_adapterIndex, &pStreamStatus->m_streamingStatus);
            if (BTMGR_RESULT_SUCCESS == rc)
            {
                BTMGRLOG_INFO ("_IsAudioStreaming: Success");
            }
            else
            {
                retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
                BTMGRLOG_ERROR ("_IsAudioStreaming: Failed; RetCode = %d", rc);
            }
        }
        else
        {
            retCode = IARM_RESULT_INVALID_PARAM;
            BTMGRLOG_ERROR ("_IsAudioStreaming: Failed; RetCode = %d", retCode);
        }
    }
    else
    {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited", __FUNCTION__);
    }

    return retCode;
}

void _EventCallback(BTMGR_EventMessage_t events)
{
    BTMGR_EventMessage_t eventData;
    memcpy (&eventData, &events, sizeof(BTMGR_EventMessage_t));
    
    if (eventData.m_eventType == BTMGR_EVENT_DEVICE_DISCOVERY_UPDATE)
    {
        BTMGRLOG_WARN ("Post Discovery Status update");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE, (void *)&eventData, sizeof(eventData));
    }
    return;
}

void btmgr_BeginIARMMode()
{
    if (!gIsBTMGR_Internal_Inited)
    {
        gIsBTMGR_Internal_Inited = 1;
        IARM_Bus_Init(IARM_BUS_BTMGR_NAME);
        IARM_Bus_Connect();

        BTMGRLOG_INFO ("_GetNumberOfAdapters : Entering %s", __FUNCTION__);

        IARM_Bus_RegisterCall("GetNumberOfAdapters", _GetNumberOfAdapters);
        IARM_Bus_RegisterCall("SetAdapterName", _SetAdapterName);
        IARM_Bus_RegisterCall("GetAdapterName", _GetAdapterName);
        IARM_Bus_RegisterCall("SetAdapterPowerStatus", _SetAdapterPowerStatus);
        IARM_Bus_RegisterCall("GetAdapterPowerStatus", _GetAdapterPowerStatus);
        IARM_Bus_RegisterCall("SetAdapterDiscoverable", _SetAdapterDiscoverable);
        IARM_Bus_RegisterCall("IsAdapterDiscoverable", _IsAdapterDiscoverable);
        IARM_Bus_RegisterCall("SetDeviceDiscoveryStatus", _ChangeDeviceDiscoveryStatus);
        IARM_Bus_RegisterCall("PairDevice", _PairDevice);
        IARM_Bus_RegisterCall("UnpairDevice", _UnpairDevice);
        IARM_Bus_RegisterCall("GetPairedDevices", _GetPairedDevices);
        IARM_Bus_RegisterCall("ConnectToDevice", _ConnectToDevice);
        IARM_Bus_RegisterCall("DisconnectFromDevice", _DisconnectFromDevice);
        IARM_Bus_RegisterCall("GetDeviceProperties", _GetDeviceProperties);
        IARM_Bus_RegisterCall("DeInit", _DeInit);

        IARM_Bus_RegisterCall("DeviceToStreamOut", _DeviceToStream);
        IARM_Bus_RegisterCall("SetPreferredAudio", _SetPreferredAudio);
        IARM_Bus_RegisterCall("StartAudioStreaming", _StartAudioStreaming);
        IARM_Bus_RegisterCall("StopAudioStreaming", _StopAudioStreaming);
        IARM_Bus_RegisterCall("IsAudioStreaming", _IsAudioStreaming);

        IARM_Bus_RegisterEvent(BTMGR_IARM_EVENT_MAX);

        /* Register a callback */
        BTMGR_RegisterEventCallback(_EventCallback);

        BTMGRLOG_INFO ("IARM Interface Inited Successfully\n");
    }
    else
        BTMGRLOG_INFO ("IARM Interface Already Inited\n");

    return;
}

void btmgr_TermIARMMode()
{
    if (gIsBTMGR_Internal_Inited)
    {
        BTMGRLOG_INFO ("IARM Interface Being terminated\n");
        IARM_Bus_Disconnect();
        IARM_Bus_Term();
    }
    else
        BTMGRLOG_INFO ("IARM Interface Not Inited\n");
}
#endif /* BTMGR_ENABLE_IARM_INTERFACE */
