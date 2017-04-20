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

#include "btmgr_priv.h"
#include "btmgr_iarm_interface.h"

static unsigned char gIsBTMGR_Internal_Inited = 0;

static IARM_Result_t
_GetNumberOfAdapters (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    unsigned char   numOfAdapters = 0;
    unsigned char*  pNumberOfAdapters = (unsigned char*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pNumberOfAdapters) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_GetNumberOfAdapters(&numOfAdapters);
    if (BTMGR_RESULT_SUCCESS == rc) {
        *pNumberOfAdapters = numOfAdapters;
        BTMGRLOG_INFO ("%s : Success; Number of Adapters = %d\n", __FUNCTION__, numOfAdapters);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_SetAdapterName (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t* pName = (BTMGR_IARMAdapterName_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pName) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_SetAdapterName(pName->m_adapterIndex, pName->m_name);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_GetAdapterName (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterName_t* pName = (BTMGR_IARMAdapterName_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pName) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_GetAdapterName(pName->m_adapterIndex, pName->m_name);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success ; Adapter name is %s\n", __FUNCTION__, pName->m_name);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_SetAdapterPowerStatus (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t* pPowerStatus = (BTMGR_IARMAdapterPower_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pPowerStatus) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_SetAdapterPowerStatus(pPowerStatus->m_adapterIndex, pPowerStatus->m_powerStatus);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_GetAdapterPowerStatus (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterPower_t* pPowerStatus = (BTMGR_IARMAdapterPower_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pPowerStatus) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_GetAdapterPowerStatus(pPowerStatus->m_adapterIndex, &pPowerStatus->m_powerStatus);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_SetAdapterDiscoverable (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t* pDiscoverable = (BTMGR_IARMAdapterDiscoverable_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pDiscoverable) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_SetAdapterDiscoverable(pDiscoverable->m_adapterIndex, pDiscoverable->m_isDiscoverable, pDiscoverable->m_timeout);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_IsAdapterDiscoverable (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscoverable_t* pDiscoverable = (BTMGR_IARMAdapterDiscoverable_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pDiscoverable) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_IsAdapterDiscoverable(pDiscoverable->m_adapterIndex, &pDiscoverable->m_isDiscoverable);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_ChangeDeviceDiscoveryStatus (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMAdapterDiscover_t* pAdapterIndex = (BTMGR_IARMAdapterDiscover_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pAdapterIndex) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    if (pAdapterIndex->m_setDiscovery)
        rc = BTMGR_StartDeviceDiscovery(pAdapterIndex->m_adapterIndex);
    else
        rc = BTMGR_StopDeviceDiscovery(pAdapterIndex->m_adapterIndex);

    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_GetDiscoveredDevices (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMDiscoveredDevices_t* pDiscoveredDevices = (BTMGR_IARMDiscoveredDevices_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pDiscoveredDevices) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_GetDiscoveredDevices(pDiscoveredDevices->m_adapterIndex, &pDiscoveredDevices->m_devices);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_PairDevice (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t* pPairDevice = (BTMGR_IARMPairDevice_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pPairDevice) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_PairDevice(pPairDevice->m_adapterIndex, pPairDevice->m_deviceHandle);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
#if 0
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
#endif
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_UnpairDevice (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMPairDevice_t* pUnPairDevice = (BTMGR_IARMPairDevice_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pUnPairDevice) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_UnpairDevice(pUnPairDevice->m_adapterIndex, pUnPairDevice->m_deviceHandle);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
#if 0
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
#endif
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_GetPairedDevices (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMPairedDevices_t* pPairedDevices = (BTMGR_IARMPairedDevices_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pPairedDevices) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_GetPairedDevices(pPairedDevices->m_adapterIndex, &pPairedDevices->m_devices);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t 
_ConnectToDevice (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t* pConnect = (BTMGR_IARMConnectDevice_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pConnect) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_ConnectToDevice(pConnect->m_adapterIndex, pConnect->m_deviceHandle, pConnect->m_connectAs);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_DisconnectFromDevice (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMConnectDevice_t* pConnect = (BTMGR_IARMConnectDevice_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pConnect) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_DisconnectFromDevice(pConnect->m_adapterIndex, pConnect->m_deviceHandle);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_GetConnectedDevices (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMConnectedDevices_t* pConnectedDevices = (BTMGR_IARMConnectedDevices_t*) arg;


    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pConnectedDevices) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_GetConnectedDevices(pConnectedDevices->m_adapterIndex, &pConnectedDevices->m_devices);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_GetDeviceProperties (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMDDeviceProperty_t* pDeviceProperty = (BTMGR_IARMDDeviceProperty_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pDeviceProperty) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_GetDeviceProperties(pDeviceProperty->m_adapterIndex, pDeviceProperty->m_deviceHandle, &pDeviceProperty->m_deviceProperty);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}


static IARM_Result_t
_StartAudioStreaming (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMStreaming_t*  pStartStream = (BTMGR_IARMStreaming_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pStartStream) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_StartAudioStreamingOut(pStartStream->m_adapterIndex, pStartStream->m_deviceHandle, pStartStream->m_audioPref);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_StopAudioStreaming (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMStreaming_t* pStopStream = (BTMGR_IARMStreaming_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pStopStream) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_StopAudioStreamingOut(pStopStream->m_adapterIndex, pStopStream->m_deviceHandle);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t _IsAudioStreaming(void *arg)
{
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMStreamingStatus_t* pStreamStatus = (BTMGR_IARMStreamingStatus_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pStreamStatus) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_IsAudioStreamingOut(pStreamStatus->m_adapterIndex, &pStreamStatus->m_streamingStatus);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_SetAudioStreamOutType (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    BTMGR_IARMStreamingType_t* pStartStream = (BTMGR_IARMStreamingType_t*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pStartStream) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_SetAudioStreamingOutType(pStartStream->m_adapterIndex, pStartStream->m_audioOutType);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, rc);
    }


    return retCode;
}

static IARM_Result_t
_ResetAdapter (
    void*   arg
) {
    IARM_Result_t   retCode = IARM_RESULT_SUCCESS;
    BTMGR_Result_t  rc = BTMGR_RESULT_SUCCESS;
    unsigned char*  pAdapterIndex =  (unsigned char*) arg;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        retCode = IARM_RESULT_INVALID_STATE;
        BTMGRLOG_ERROR ("%s : BTRMgr is not Inited\n", __FUNCTION__);
        return retCode;
    }

    if (!pAdapterIndex) {
        retCode = IARM_RESULT_INVALID_PARAM;
        BTMGRLOG_ERROR ("%s : Failed; RetCode = %d\n", __FUNCTION__, retCode);
        return retCode;
    }


    rc = BTMGR_ResetAdapter (*pAdapterIndex);
    if (BTMGR_RESULT_SUCCESS == rc) {
        BTMGRLOG_INFO ("%s : Success\n", __FUNCTION__);
    }
    else {
        retCode = IARM_RESULT_IPCCORE_FAIL; /* We do not have other IARM Error code to describe this. */
        BTMGRLOG_ERROR ("_ResetAdapter: Failed; RetCode = %d\n", rc);
    }


    return retCode;

}

static IARM_Result_t
_DeInit (
    void*   arg
) {
    if (gIsBTMGR_Internal_Inited)
        BTMGR_DeInit();

    return IARM_RESULT_SUCCESS;
}


void
_EventCallback (
    BTMGR_EventMessage_t events
) {
    BTMGR_EventMessage_t eventData;

    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    memcpy (&eventData, &events, sizeof(BTMGR_EventMessage_t));

    if (eventData.m_eventType == BTMGR_EVENT_DEVICE_OUT_OF_RANGE) {
        BTMGRLOG_WARN ("Post Device Out of Range event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_DISCOVERY_UPDATE) {
        BTMGRLOG_WARN ("Post Discovery Status update\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_DISCOVERY_COMPLETE) {
        BTMGRLOG_WARN ("Post Discovery Complete event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_PAIRING_COMPLETE) {
        BTMGRLOG_WARN ("Post Device Pairing Complete event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_UNPAIRING_COMPLETE) {
        BTMGRLOG_WARN ("Post Device Pairing Complete event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_CONNECTION_COMPLETE) {
        BTMGRLOG_WARN ("Post Device Connection Complete event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_DISCONNECT_COMPLETE) {
        BTMGRLOG_WARN ("Post Device Disconnected event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_PAIRING_FAILED) {
        BTMGRLOG_WARN ("Post Device Pairing Failed event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_PAIRING_FAILED, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_UNPAIRING_FAILED) {
        BTMGRLOG_WARN ("Post Device UnPairing Failed event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_CONNECTION_FAILED) {
        BTMGRLOG_WARN ("Post Device Connection Failed event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_DEVICE_DISCONNECT_FAILED) {
        BTMGRLOG_WARN ("Post Device Disconnect Failed event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST) {
        BTMGRLOG_WARN ("Post External Device Pair Request event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST, (void *)&eventData, sizeof(eventData));
    }
    else if (eventData.m_eventType == BTMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST) {
        BTMGRLOG_WARN ("Post External Device Connect Request event\n");
        IARM_Bus_BroadcastEvent(IARM_BUS_BTMGR_NAME, (IARM_EventId_t) BTMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST, (void *)&eventData, sizeof(eventData));
    }
    

    return;
}


void
btmgr_BeginIARMMode (
    void
) {
    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (!gIsBTMGR_Internal_Inited) {
        gIsBTMGR_Internal_Inited = 1;
        IARM_Bus_Init(IARM_BUS_BTMGR_NAME);
        IARM_Bus_Connect();

        BTMGRLOG_INFO ("IARM Interface Initializing\n");

        IARM_Bus_RegisterCall("GetNumberOfAdapters", _GetNumberOfAdapters);
        IARM_Bus_RegisterCall("SetAdapterName", _SetAdapterName);
        IARM_Bus_RegisterCall("GetAdapterName", _GetAdapterName);
        IARM_Bus_RegisterCall("SetAdapterPowerStatus", _SetAdapterPowerStatus);
        IARM_Bus_RegisterCall("GetAdapterPowerStatus", _GetAdapterPowerStatus);
        IARM_Bus_RegisterCall("SetAdapterDiscoverable", _SetAdapterDiscoverable);
        IARM_Bus_RegisterCall("IsAdapterDiscoverable", _IsAdapterDiscoverable);
        IARM_Bus_RegisterCall("SetDeviceDiscoveryStatus", _ChangeDeviceDiscoveryStatus);
        IARM_Bus_RegisterCall("GetDiscoveredDevices", _GetDiscoveredDevices);
        IARM_Bus_RegisterCall("PairDevice", _PairDevice);
        IARM_Bus_RegisterCall("UnpairDevice", _UnpairDevice);
        IARM_Bus_RegisterCall("GetPairedDevices", _GetPairedDevices);
        IARM_Bus_RegisterCall("ConnectToDevice", _ConnectToDevice);
        IARM_Bus_RegisterCall("DisconnectFromDevice", _DisconnectFromDevice);
        IARM_Bus_RegisterCall("GetConnectedDevices", _GetConnectedDevices);
        IARM_Bus_RegisterCall("GetDeviceProperties", _GetDeviceProperties);

        IARM_Bus_RegisterCall("StartAudioStreaming", _StartAudioStreaming);
        IARM_Bus_RegisterCall("StopAudioStreaming", _StopAudioStreaming);
        IARM_Bus_RegisterCall("IsAudioStreaming", _IsAudioStreaming);
        IARM_Bus_RegisterCall("SetAudioStreamOutType", _SetAudioStreamOutType);
        IARM_Bus_RegisterCall("ResetAdapter", _ResetAdapter);
        IARM_Bus_RegisterCall("DeInit", _DeInit);

        IARM_Bus_RegisterEvent(BTMGR_IARM_EVENT_MAX);

        /* Register a callback */
        BTMGR_RegisterEventCallback(_EventCallback);

        BTMGRLOG_INFO ("IARM Interface Inited Successfully\n");
    }
    else {
        BTMGRLOG_INFO ("IARM Interface Already Inited\n");
    }

    return;
}

void
btmgr_TermIARMMode (
    void
) {
    BTMGRLOG_INFO ("%s : Entering\n", __FUNCTION__);

    if (gIsBTMGR_Internal_Inited) {
        BTMGRLOG_INFO ("IARM Interface Being terminated\n");
        IARM_Bus_Disconnect();
        IARM_Bus_Term();
    }
    else {
        BTMGRLOG_INFO ("IARM Interface Not Inited\n");
    }
}
