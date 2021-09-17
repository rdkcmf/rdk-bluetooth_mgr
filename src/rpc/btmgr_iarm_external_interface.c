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
#include "btrMgr_logger.h"
#include "btmgr_iarm_interface.h"


static unsigned char isBTRMGR_Inited = 0;
static unsigned char isBTRMGR_Iarm_Inited = 0;
static unsigned char isBTRMGR_Iarm_Connected = 0;
static BTRMGR_EventCallback m_eventCallbackFunction = NULL;

#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif

static void btrMgrdeviceCallback(const char *owner, IARM_EventId_t eventId, void *data, size_t len);
static void btrMgrMediaCallback(const char *owner, IARM_EventId_t eventId, void *data, size_t len);


/**********************/
/* Private Interfaces */
/**********************/


/*********************/
/* Public Interfaces */
/*********************/
BTRMGR_Result_t
BTRMGR_Init (
    void
) {
    char processName[256] = "";
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (!isBTRMGR_Inited) {
        isBTRMGR_Inited = 1;
#ifdef RDK_LOGGER_ENABLED
        const char* pDebugConfig = NULL;
        const char* BTRMGR_IARM_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
        const char* BTRMGR_IARM_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

        /* Init the logger */
        if (access(BTRMGR_IARM_DEBUG_OVERRIDE_PATH, F_OK) != -1)
            pDebugConfig = BTRMGR_IARM_DEBUG_OVERRIDE_PATH;
        else
            pDebugConfig = BTRMGR_IARM_DEBUG_ACTUAL_PATH;

        if (0 == rdk_logger_init(pDebugConfig))
            b_rdk_logger_enabled = 1;
#endif        
        sprintf (processName, "BTRMgr-User-%u", getpid());
        if (IARM_RESULT_SUCCESS == (retCode = IARM_Bus_Init((const char*) &processName))) {
            isBTRMGR_Iarm_Inited = 1;
            BTRMGRLOG_INFO ("IARM Interface Inited Successfully\n");
        }
        else {
            BTRMGRLOG_INFO ("IARM Interface Inited Externally\n");
        }

        if (IARM_RESULT_SUCCESS == (retCode = IARM_Bus_Connect())) {
            isBTRMGR_Iarm_Connected = 1;
            BTRMGRLOG_INFO ("IARM Interface Inited Internally-BTRMGR\n");
        }
        else {
            BTRMGRLOG_INFO ("IARM Interface Connected Externally\n");
        }

        BTRMGR_RegisterForCallbacks(NULL);
    }
    else
        BTRMGRLOG_INFO ("IARM Interface Already Inited\n");

    return BTRMGR_RESULT_SUCCESS;
}


BTRMGR_Result_t
BTRMGR_RegisterForCallbacks (
    const char *apcProcessName
)  {
    int isRegistered = 0;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

#ifdef RDK_LOGGER_ENABLED
    if (!b_rdk_logger_enabled) {
        const char* pDebugConfig = NULL;
        const char* BTRMGR_IARM_DEBUG_ACTUAL_PATH    = "/etc/debug.ini";
        const char* BTRMGR_IARM_DEBUG_OVERRIDE_PATH  = "/opt/debug.ini";

        /* Init the logger */
        if (access(BTRMGR_IARM_DEBUG_OVERRIDE_PATH, F_OK) != -1)
            pDebugConfig = BTRMGR_IARM_DEBUG_OVERRIDE_PATH;
        else
            pDebugConfig = BTRMGR_IARM_DEBUG_ACTUAL_PATH;

        if (0 == rdk_logger_init(pDebugConfig))
            b_rdk_logger_enabled = 1;
    }
#endif

    if (!apcProcessName) {
        BTRMGRLOG_INFO ("BTRMGR_INIT has called\n");
    }
    else {
        BTRMGRLOG_INFO ("apcProcessName = %s\n", apcProcessName);
        retCode = IARM_Bus_IsConnected(apcProcessName, &isRegistered);
        if ((retCode != IARM_RESULT_SUCCESS) ||  (isRegistered == 0)) {
            BTRMGRLOG_ERROR ("IARM_Bus_IsConnected Failure (%s); RetCode = %d\n",apcProcessName, retCode);
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_STARTED, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_PAIRING_FAILED, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_FOUND, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_STARTED, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_PLAYING, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_PAUSED, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_STOPPED, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_POSITION, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_CHANGED, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYBACK_ENDED, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_OP_READY, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_OP_INFORMATION, btrMgrdeviceCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_NAME, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_VOLUME, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_EQUALIZER_OFF, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_EQUALIZER_ON, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_OFF, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_ALLTRACKS, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_GROUP, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_OFF, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_SINGLETRACK, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_ALLTRACKS, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_GROUP, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_ALBUM_INFO, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_ARTIST_INFO, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_GENRE_INFO, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_COMPILATION_INFO, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYLIST_INFO, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACKLIST_INFO, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_MUTE, btrMgrMediaCallback);
    IARM_Bus_RegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_UNMUTE, btrMgrMediaCallback);

    BTRMGRLOG_INFO ("IARM Interface Inited Register Event Successfully\n");

    return BTRMGR_RESULT_SUCCESS;
}


BTRMGR_Result_t
BTRMGR_DeInit (
    void
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (m_eventCallbackFunction)
        m_eventCallbackFunction = NULL;


    if (isBTRMGR_Inited) {
        BTRMGR_UnRegisterFromCallbacks(NULL);

        if (isBTRMGR_Iarm_Connected)  {
            if (IARM_RESULT_SUCCESS == (retCode = IARM_Bus_Disconnect())) {
                isBTRMGR_Iarm_Connected = 0;
            }
            else {
                BTRMGRLOG_ERROR ("IARM_Bus_Disconnect Failed; RetCode = %d\n", retCode);
            }
        }

        if (isBTRMGR_Iarm_Inited) {
            if (IARM_RESULT_SUCCESS == (retCode = IARM_Bus_Term())) {
                isBTRMGR_Iarm_Inited = 0;
            }
            else {
                BTRMGRLOG_ERROR ("IARM_Bus_Term Failed; RetCode = %d\n", retCode);
            }
        }

        isBTRMGR_Inited = 0;
        BTRMGRLOG_INFO ("IARM Interface termination Successfully \n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_INFO ("IARM Interface for BTRMgr is Not Inited Yet..\n");
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_UnRegisterFromCallbacks (
    const char *apcProcessName
) {
    int isRegistered = 0;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (!apcProcessName) {
        BTRMGRLOG_ERROR ("apcProcessName is NULL\n");
    }
    else {
        retCode = IARM_Bus_IsConnected(apcProcessName, &isRegistered);
        if (retCode != IARM_RESULT_SUCCESS || isRegistered == 0 ) {
            BTRMGRLOG_ERROR ("IARM_Bus_IsConnected Failure; RetCode = %d\n", retCode);
            return BTRMGR_RESULT_GENERIC_FAILURE;
        }
    }

    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACKLIST_INFO);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYLIST_INFO);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_COMPILATION_INFO);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_GENRE_INFO);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_ARTIST_INFO);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_ALBUM_INFO);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_GROUP);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_ALLTRACKS);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_SINGLETRACK);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_OFF);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_GROUP);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_ALLTRACKS);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_OFF);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_EQUALIZER_ON);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_EQUALIZER_OFF);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_VOLUME);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_NAME);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_OP_INFORMATION);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_OP_READY);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYBACK_ENDED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_CHANGED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_POSITION);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_STOPPED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_PAUSED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_PLAYING);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_TRACK_STARTED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_FOUND);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_PAIRING_FAILED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_STARTED);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_MUTE);
    IARM_Bus_UnRegisterEventHandler(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_EVENT_MEDIA_PLAYER_UNMUTE);

    BTRMGRLOG_INFO ("IARM Interface UnRegister Event Succesful\n");

    return BTRMGR_RESULT_SUCCESS;
}

BTRMGR_Result_t
BTRMGR_GetNumberOfAdapters (
    unsigned char*  pNumOfAdapters
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    unsigned char num_of_adapters = 0;
    
    if (!pNumOfAdapters) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_NUMBER_OF_ADAPTERS, (void *)&num_of_adapters, sizeof(num_of_adapters), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        *pNumOfAdapters = num_of_adapters;
        BTRMGRLOG_INFO ("Success; Number of Adapters = %d\n", num_of_adapters);
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t BTRMGR_LE_StartAdvertisement(unsigned char aui8AdapterIdx, BTRMGR_LeCustomAdvertisement_t *pstBTMGR_LeCustomAdvt)
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdvtInfo_t lstAdvtInfo;

    lstAdvtInfo.m_adapterIndex = aui8AdapterIdx;
    memcpy(&lstAdvtInfo.m_CustAdvt, pstBTMGR_LeCustomAdvt, sizeof(BTRMGR_LeCustomAdvertisement_t));

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_LE_START_ADVERTISEMENT, (void *)&lstAdvtInfo, sizeof(BTRMGR_IARMAdvtInfo_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) 
    {
        BTRMGRLOG_INFO("Success; Device is now advertising\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t BTRMGR_LE_StopAdvertisement(unsigned char aui8AdapterIdx)
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_LE_STOP_ADVERTISEMENT, (void*)&aui8AdapterIdx, sizeof(unsigned char), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode)
    {
        BTRMGRLOG_INFO("Success; Device has stopped advertising\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t BTRMGR_LE_GetPropertyValue(unsigned char aui8AdapterIdx, char *aUUID, char *aValue, BTRMGR_LeProperty_t aElement)
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMGATTValue_t lGattValue;

    strncpy(lGattValue.m_UUID, aUUID, (BTRMGR_MAX_STR_LEN - 1));
    lGattValue.aElement = aElement;
    lGattValue.m_adapterIndex = aui8AdapterIdx;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_LE_GET_PROP_VALUE, (void *)&lGattValue, sizeof(lGattValue), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        strncpy(aValue, lGattValue.m_Value, (BTRMGR_MAX_STR_LEN - 1));
        BTRMGRLOG_INFO("Success; Property value is = %s\n", lGattValue.m_Value);
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t BTRMGR_LE_SetServiceInfo(unsigned char aui8AdapterIdx, char *aUUID, unsigned char aServiceType)
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMGATTServiceInfo_t lGattServiceInfo = {};

    lGattServiceInfo.m_adapterIndex = aui8AdapterIdx;
    strncpy(lGattServiceInfo.m_UUID, aUUID, (BTRMGR_MAX_STR_LEN - 1));
    lGattServiceInfo.m_ServiceType = aServiceType;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_LE_SET_GATT_SERVICE_INFO, (void *)&lGattServiceInfo, sizeof(lGattServiceInfo), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) 
    {
        BTRMGRLOG_INFO("Success; \n");
    }
    else 
    {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t BTRMGR_LE_SetGattInfo(unsigned char aui8AdapterIdx, char *aParentUUID, char *aUUID, unsigned short aFlags, char *aValue, BTRMGR_LeProperty_t aElement)
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_INVALID_PARAM;
    BTRMGR_IARMGATTInfo_t lGattCharInfo = {};

    if ((NULL != aParentUUID) && (NULL != aUUID))
    {
        lGattCharInfo.m_adapterIndex = aui8AdapterIdx;
        strncpy(lGattCharInfo.m_ParentUUID, aParentUUID, (BTRMGR_MAX_STR_LEN - 1));
        strncpy(lGattCharInfo.m_UUID, aUUID, (BTRMGR_MAX_STR_LEN - 1));
        lGattCharInfo.m_Flags = aFlags;
        if (NULL != aValue)
        {
            strncpy(lGattCharInfo.m_Value, aValue, (BTRMGR_MAX_STR_LEN - 1));
        }
        lGattCharInfo.m_Element = aElement;

        retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_LE_SET_GATT_CHAR_INFO, (void *)&lGattCharInfo, sizeof(lGattCharInfo), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    }
    
    if (IARM_RESULT_SUCCESS == retCode)
    {
        BTRMGRLOG_INFO("Success; \n");
    }
    else
    {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t BTRMGR_LE_SetGattPropertyValue(unsigned char aui8AdapterIdx, char *aUUID, char *aValue, BTRMGR_LeProperty_t aElement) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMGATTValue_t lGattValue;

    strncpy(lGattValue.m_UUID, aUUID, (BTRMGR_MAX_STR_LEN - 1));
    lGattValue.aElement = aElement;
    lGattValue.m_adapterIndex = aui8AdapterIdx;

    strncpy(lGattValue.m_Value, aValue, (BTRMGR_MAX_STR_LEN - 1));
    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_LE_SET_GATT_PROPERTY_VALUE, (void *)&lGattValue, sizeof(lGattValue), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        
        BTRMGRLOG_INFO("Success; Property value is = %s\n", lGattValue.m_Value);
    }

    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_ResetAdapter (
    unsigned char index_of_adapter
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_RESET_ADAPTER, (void *)&index_of_adapter, sizeof(index_of_adapter), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetLimitBeaconDetection (
    unsigned char   index_of_adapter,
    unsigned char   *pLimited
) {
    BTRMGR_Result_t rc    = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMBeaconDetection_t beaconDetection;

    if((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pLimited)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    beaconDetection.m_adapterIndex = index_of_adapter;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_LIMIT_BEACON_DETECTION, (void *)&beaconDetection, sizeof(BTRMGR_IARMBeaconDetection_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        *pLimited = beaconDetection.m_limitBeaconDetection;
        BTRMGRLOG_INFO ("Success; Beacon Detection Limited ?  %s\n", (*pLimited ? "true" : "false"));
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetLimitBeaconDetection (
    unsigned char   index_of_adapter,
    unsigned char   limited
) {

    BTRMGR_Result_t rc    = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMBeaconDetection_t beaconDetection;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&beaconDetection, 0, sizeof(BTRMGR_IARMBeaconDetection_t));
    beaconDetection.m_adapterIndex = index_of_adapter;
    beaconDetection.m_limitBeaconDetection = limited;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_LIMIT_BEACON_DETECTION, (void*)&beaconDetection, sizeof(BTRMGR_IARMBeaconDetection_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAdapterName (
    unsigned char   index_of_adapter,
    const char*     pNameOfAdapter
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterName_t adapterSetting;

    if((NULL == pNameOfAdapter) || (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    adapterSetting.m_adapterIndex = index_of_adapter;
    strncpy (adapterSetting.m_name, pNameOfAdapter, (BTRMGR_NAME_LEN_MAX - 1));
    
    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_ADAPTER_NAME, (void *)&adapterSetting, sizeof(adapterSetting), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetAdapterName (
    unsigned char   index_of_adapter,
    char*           pNameOfAdapter
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterName_t adapterSetting;

    if((NULL == pNameOfAdapter) || (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    adapterSetting.m_adapterIndex = index_of_adapter;
    memset (adapterSetting.m_name, '\0', sizeof (adapterSetting.m_name));

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_ADAPTER_NAME, (void *)&adapterSetting, sizeof(adapterSetting), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        strncpy (pNameOfAdapter, adapterSetting.m_name, (BTRMGR_NAME_LEN_MAX - 1));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAdapterPowerStatus (
    unsigned char   index_of_adapter,
    unsigned char   power_status
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterPower_t powerStatus;
    
    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (power_status > 1)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    powerStatus.m_adapterIndex = index_of_adapter;
    powerStatus.m_powerStatus = power_status;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_ADAPTER_POWERSTATUS, (void *)&powerStatus, sizeof(powerStatus), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetAdapterPowerStatus (
    unsigned char   index_of_adapter,
    unsigned char*  pPowerStatus
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterPower_t powerStatus;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pPowerStatus)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    powerStatus.m_adapterIndex = index_of_adapter;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_ADAPTER_POWERSTATUS, (void *)&powerStatus, sizeof(powerStatus), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        *pPowerStatus = powerStatus.m_powerStatus;
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAdapterDiscoverable (
    unsigned char   index_of_adapter,
    unsigned char   discoverable,
    int  timeout
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterDiscoverable_t discoverableSetting;
    
    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (discoverable > 1)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    discoverableSetting.m_adapterIndex = index_of_adapter;
    discoverableSetting.m_isDiscoverable = discoverable;
    discoverableSetting.m_timeout = timeout;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_ADAPTER_DISCOVERABLE, (void *)&discoverableSetting, sizeof(discoverableSetting), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_IsAdapterDiscoverable (
    unsigned char   index_of_adapter,
    unsigned char*  pDiscoverable
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterDiscoverable_t discoverableSetting;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pDiscoverable)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    discoverableSetting.m_adapterIndex = index_of_adapter;
    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_IS_ADAPTER_DISCOVERABLE, (void *)&discoverableSetting, sizeof(discoverableSetting), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        *pDiscoverable = discoverableSetting.m_isDiscoverable;
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_StartDeviceDiscovery (
    unsigned char                index_of_adapter,
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterDiscover_t deviceDiscovery;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    deviceDiscovery.m_adapterIndex  = index_of_adapter;
    deviceDiscovery.m_setDiscovery  = 1; /* TRUE */
    deviceDiscovery.m_enBTRMgrDevOpT= aenBTRMgrDevOpT;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_CHANGE_DEVICE_DISCOVERY_STATUS, (void *)&deviceDiscovery, sizeof(deviceDiscovery), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_StopDeviceDiscovery (
    unsigned char                index_of_adapter,
    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpT
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAdapterDiscover_t deviceDiscovery;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    deviceDiscovery.m_adapterIndex = index_of_adapter;
    deviceDiscovery.m_setDiscovery = 0; /* FALSE */
    deviceDiscovery.m_enBTRMgrDevOpT= aenBTRMgrDevOpT;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_CHANGE_DEVICE_DISCOVERY_STATUS, (void *)&deviceDiscovery, sizeof(deviceDiscovery), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetDiscoveryStatus (
        unsigned char                aui8AdapterIdx,
        BTRMGR_DiscoveryStatus_t     *isDiscoveryInProgress,
        BTRMGR_DeviceOperationType_t *aenBTRMgrDevOpT
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMDiscoveryStatus_t discoveryStatus;

    memset (&discoveryStatus, 0, sizeof(BTRMGR_IARMDiscoveryStatus_t));
    discoveryStatus.m_adapterIndex = aui8AdapterIdx;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_DISCOVERY_STATUS, (void *)&discoveryStatus, sizeof(discoveryStatus), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
    	*isDiscoveryInProgress = discoveryStatus.m_discoveryInProgress;
        *aenBTRMgrDevOpT       = discoveryStatus.m_discoveryType;
        BTRMGRLOG_INFO ("Success; Discovery State = %d\n", discoveryStatus.m_discoveryInProgress);
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetDiscoveredDevices (
    unsigned char                   index_of_adapter,
    BTRMGR_DiscoveredDevicesList_t*  pDiscoveredDevices
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMDiscoveredDevices_t discoveredDevices;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pDiscoveredDevices)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    memset (&discoveredDevices, 0, sizeof(discoveredDevices));
    discoveredDevices.m_adapterIndex = index_of_adapter;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_DISCOVERED_DEVICES, (void *)&discoveredDevices, sizeof(discoveredDevices), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        memcpy (pDiscoveredDevices, &discoveredDevices.m_devices, sizeof(BTRMGR_DiscoveredDevicesList_t));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_PairDevice (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle   handle
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMPairDevice_t newDevice;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    memset (&newDevice, 0, sizeof(newDevice));
    newDevice.m_adapterIndex = index_of_adapter;
    newDevice.m_deviceHandle = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_PAIR_DEVICE, (void *)&newDevice, sizeof(newDevice), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_UnpairDevice (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle   handle
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMPairDevice_t removeDevice;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    memset (&removeDevice, 0, sizeof(removeDevice));
    removeDevice.m_adapterIndex = index_of_adapter;
    removeDevice.m_deviceHandle = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_UNPAIR_DEVICE, (void *)&removeDevice, sizeof(removeDevice), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetPairedDevices (
    unsigned char               index_of_adapter,
    BTRMGR_PairedDevicesList_t*  pPairedDevices
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMPairedDevices_t pairedDevices;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pPairedDevices)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    memset (&pairedDevices, 0, sizeof(pairedDevices));
    pairedDevices.m_adapterIndex = index_of_adapter;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_PAIRED_DEVICES, (void *)&pairedDevices, sizeof(pairedDevices), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        memcpy (pPairedDevices, &pairedDevices.m_devices, sizeof(BTRMGR_PairedDevicesList_t));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_ConnectToDevice (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_DeviceOperationType_t connectAs
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMConnectDevice_t connectToDevice;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    connectToDevice.m_adapterIndex = index_of_adapter;
    connectToDevice.m_connectAs = connectAs;
    connectToDevice.m_deviceHandle = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_CONNECT_TO_DEVICE, (void *)&connectToDevice, sizeof(connectToDevice), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_DisconnectFromDevice (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle   handle
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMConnectDevice_t disConnectToDevice;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    disConnectToDevice.m_adapterIndex = index_of_adapter;
    disConnectToDevice.m_deviceHandle = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_DISCONNECT_FROM_DEVICE, (void *)&disConnectToDevice, sizeof(disConnectToDevice), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_GetConnectedDevices (
    unsigned char                   index_of_adapter,
    BTRMGR_ConnectedDevicesList_t*   pConnectedDevices
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMConnectedDevices_t connectedDevices;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pConnectedDevices)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    memset (&connectedDevices, 0, sizeof(connectedDevices));
    connectedDevices.m_adapterIndex = index_of_adapter;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_CONNECTED_DEVICES, (void *)&connectedDevices, sizeof(connectedDevices), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        memcpy (pConnectedDevices, &connectedDevices.m_devices, sizeof(BTRMGR_ConnectedDevicesList_t));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetDeviceProperties (
    unsigned char               index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_DevicesProperty_t*    pDeviceProperty
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMDDeviceProperty_t deviceProperty;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle) || (NULL == pDeviceProperty)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    deviceProperty.m_adapterIndex = index_of_adapter;
    deviceProperty.m_deviceHandle = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_DEVICE_PROPERTIES, (void *)&deviceProperty, sizeof(deviceProperty), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        memcpy (pDeviceProperty, &deviceProperty.m_deviceProperty, sizeof(BTRMGR_DevicesProperty_t));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_StartAudioStreamingOut (
    unsigned char                   index_of_adapter,
    BTRMgrDeviceHandle              handle,
    BTRMGR_DeviceOperationType_t    streamOutPref
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMStreaming_t streaming;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    streaming.m_adapterIndex = index_of_adapter;
    streaming.m_deviceHandle = handle;
    streaming.m_audioPref = streamOutPref;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_START_AUDIO_STREAMING_OUT, (void *)&streaming, sizeof(streaming), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_StopAudioStreamingOut (
    unsigned char       index_of_adapter,
    BTRMgrDeviceHandle  handle
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMStreaming_t streaming;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    streaming.m_adapterIndex = index_of_adapter;
    streaming.m_deviceHandle = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_STOP_AUDIO_STREAMING_OUT, (void*) &streaming, sizeof(streaming), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_IsAudioStreamingOut (
    unsigned char   index_of_adapter,
    unsigned char*  pStreamingStatus
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMStreamingStatus_t status;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (NULL == pStreamingStatus)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    status.m_adapterIndex = index_of_adapter;
    status.m_streamingStatus = 0;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_IS_AUDIO_STREAMING_OUT, (void *)&status, sizeof(status), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        *pStreamingStatus = status.m_streamingStatus;
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetAudioStreamingOutType (
    unsigned char           index_of_adapter,
    BTRMGR_StreamOut_Type_t type
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMStreamingType_t streamingType;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    streamingType.m_adapterIndex = index_of_adapter;
    streamingType.m_audioOutType = type;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_AUDIO_STREAM_OUT_TYPE, (void *)&streamingType, sizeof(streamingType), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_StartAudioStreamingIn (
    unsigned char                   ui8AdapterIdx,
    BTRMgrDeviceHandle              handle,
    BTRMGR_DeviceOperationType_t    streamOutPref
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMStreaming_t streaming;

    if ((BTRMGR_ADAPTER_COUNT_MAX < ui8AdapterIdx) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    streaming.m_adapterIndex = ui8AdapterIdx;
    streaming.m_deviceHandle = handle;
    streaming.m_audioPref = streamOutPref;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_START_AUDIO_STREAMING_IN, (void *)&streaming, sizeof(streaming), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_StopAudioStreamingIn (
    unsigned char       ui8AdapterIdx,
    BTRMgrDeviceHandle  handle
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMStreaming_t streaming;

    if ((BTRMGR_ADAPTER_COUNT_MAX < ui8AdapterIdx) || (0 == handle)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    streaming.m_adapterIndex = ui8AdapterIdx;
    streaming.m_deviceHandle = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_STOP_AUDIO_STREAMING_IN, (void*) &streaming, sizeof(streaming), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_IsAudioStreamingIn (
    unsigned char   ui8AdapterIdx,
    unsigned char*  pStreamingStatus
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMStreamingStatus_t status;

    if ((BTRMGR_ADAPTER_COUNT_MAX < ui8AdapterIdx) || (NULL == pStreamingStatus)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }


    status.m_adapterIndex = ui8AdapterIdx;
    status.m_streamingStatus = 0;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_IS_AUDIO_STREAMING_IN, (void *)&status, sizeof(status), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        *pStreamingStatus = status.m_streamingStatus;
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetEventResponse (
    unsigned char           index_of_adapter,
    BTRMGR_EventResponse_t* apstBTRMgrEvtRsp
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMEventResp_t lstBtrMgrIArmEvtResp;

    if ((BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) || (apstBTRMgrEvtRsp == NULL)) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    lstBtrMgrIArmEvtResp.m_adapterIndex = index_of_adapter;
    memcpy(&lstBtrMgrIArmEvtResp.m_stBTRMgrEvtRsp, apstBTRMgrEvtRsp, sizeof(BTRMGR_EventResponse_t));

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_EVENT_RESPONSE, (void *)&lstBtrMgrIArmEvtResp, sizeof(lstBtrMgrIArmEvtResp), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_MediaControl (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_MediaControlCommand_t mediaCtrlCmd
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMMediaProperty_t  mediaProperty;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&mediaProperty, 0, sizeof(BTRMGR_IARMMediaProperty_t));
    mediaProperty.m_adapterIndex    = index_of_adapter;
    mediaProperty.m_deviceHandle    = handle;
    mediaProperty.m_mediaControlCmd = mediaCtrlCmd;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_MEDIA_CONTROL, (void*)&mediaProperty, sizeof(BTRMGR_IARMMediaProperty_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_GetDeviceVolumeMute (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t deviceOpType,
    unsigned char *pui8Volume,
    unsigned char *pui8Mute
) {

    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMDeviceVolumeMute_t devvolmut;

    if (BTRMGR_ADAPTER_COUNT_MAX < aui8AdapterIdx || NULL == pui8Volume || NULL == pui8Mute) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&devvolmut, 0, sizeof(BTRMGR_IARMDeviceVolumeMute_t));
    devvolmut.m_adapterIndex    = aui8AdapterIdx;
    devvolmut.m_deviceHandle    = ahBTRMgrDevHdl;
    devvolmut.m_deviceOpType    = deviceOpType;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_DEVICE_VOLUME_MUTE_INFO, (void*)&devvolmut, sizeof(BTRMGR_IARMMediaProperty_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        *pui8Volume =  devvolmut.m_volume;
        *pui8Mute   =  devvolmut.m_mute;
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_SetDeviceVolumeMute (
    unsigned char                aui8AdapterIdx,
    BTRMgrDeviceHandle           ahBTRMgrDevHdl,
    BTRMGR_DeviceOperationType_t deviceOpType,
    unsigned char ui8Volume,
    unsigned char ui8Mute
) {

    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMDeviceVolumeMute_t devvolmut;

    if (BTRMGR_ADAPTER_COUNT_MAX < aui8AdapterIdx || ui8Mute > 1) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&devvolmut, 0, sizeof(BTRMGR_IARMDeviceVolumeMute_t));
    devvolmut.m_adapterIndex    = aui8AdapterIdx;
    devvolmut.m_deviceHandle    = ahBTRMgrDevHdl;
    devvolmut.m_deviceOpType    = deviceOpType;
    devvolmut.m_volume          = ui8Volume;
    devvolmut.m_mute            = ui8Mute;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_DEVICE_VOLUME_MUTE_INFO, (void*)&devvolmut, sizeof(BTRMGR_IARMMediaProperty_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetMediaTrackInfo (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_MediaTrackInfo_t*     mediaTrackInfo
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMMediaProperty_t  mediaProperty;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter || NULL == mediaTrackInfo) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&mediaProperty, 0, sizeof(BTRMGR_IARMMediaProperty_t));
    mediaProperty.m_adapterIndex    = index_of_adapter;
    mediaProperty.m_deviceHandle    = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_MEDIA_TRACK_INFO, (void*)&mediaProperty, sizeof(BTRMGR_IARMMediaProperty_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        memcpy (mediaTrackInfo, &mediaProperty.m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetMediaElementTrackInfo (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMgrMediaElementHandle     mediaElementHandle,
    BTRMGR_MediaTrackInfo_t*     mediaTrackInfo
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMMediaProperty_t  mediaProperty;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter || NULL == mediaTrackInfo) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&mediaProperty, 0, sizeof(BTRMGR_IARMMediaProperty_t));
    mediaProperty.m_adapterIndex    = index_of_adapter;
    mediaProperty.m_deviceHandle    = handle;
    mediaProperty.m_mediaElementHandle   = mediaElementHandle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_MEDIA_ELEMENT_TRACK_INFO, (void*)&mediaProperty, sizeof(BTRMGR_IARMMediaProperty_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        memcpy (mediaTrackInfo, &mediaProperty.m_mediaTrackInfo, sizeof(BTRMGR_MediaTrackInfo_t));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_GetMediaCurrentPosition (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    BTRMGR_MediaPositionInfo_t*  mediaPositionInfo
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMMediaProperty_t  mediaProperty;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter || NULL == mediaPositionInfo) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&mediaProperty, 0, sizeof(BTRMGR_IARMMediaProperty_t));
    mediaProperty.m_adapterIndex         = index_of_adapter;
    mediaProperty.m_deviceHandle         = handle;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_MEDIA_CURRENT_POSITION, (void*)&mediaProperty, sizeof(BTRMGR_IARMMediaProperty_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        memcpy (mediaPositionInfo, &mediaProperty.m_mediaPositionInfo, sizeof(BTRMGR_MediaPositionInfo_t));
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_SetMediaElementActive (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           deviceHandle,
    BTRMgrMediaElementHandle     mediaElementHandle,
    BTRMGR_MediaElementType_t    mediaElementType
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMMediaElementListInfo_t   mediaElementList;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&mediaElementList, 0, sizeof(BTRMGR_IARMMediaElementListInfo_t));
    mediaElementList.m_adapterIndex         = index_of_adapter;
    mediaElementList.m_deviceHandle         = deviceHandle;
    mediaElementList.m_mediaElementHandle   = mediaElementHandle;
    mediaElementList.m_mediaElementType     = mediaElementType;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_MEDIA_ELEMENT_ACTIVE, (void*)&mediaElementList, sizeof(BTRMGR_IARMMediaElementListInfo_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_GetMediaElementList (
    unsigned char                  index_of_adapter,
    BTRMgrDeviceHandle             deviceHandle,
    BTRMgrMediaElementHandle       mediaElementHandle,
    unsigned short                 mediaElementStartIdx,
    unsigned short                 mediaElementEndIdx, 
    unsigned char                  mediaElementListDepth,
    BTRMGR_MediaElementType_t      mediaElementType,
    BTRMGR_MediaElementListInfo_t* mediaElementListInfo
) { 
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMMediaElementListInfo_t   mediaElementList;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter || !mediaElementListInfo) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&mediaElementList, 0, sizeof(BTRMGR_IARMMediaElementListInfo_t));
    mediaElementList.m_adapterIndex         = index_of_adapter;
    mediaElementList.m_deviceHandle         = deviceHandle;
    mediaElementList.m_mediaElementHandle   = mediaElementHandle;
    mediaElementList.m_mediaElementStartIdx = mediaElementStartIdx;
    mediaElementList.m_mediaElementEndIdx   = mediaElementEndIdx;
    mediaElementList.m_mediaElementListDepth= mediaElementListDepth;
    mediaElementList.m_mediaElementType     = mediaElementType;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_MEDIA_ELEMENT_LIST, (void*)&mediaElementList, sizeof(BTRMGR_IARMMediaElementListInfo_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
        memcpy (mediaElementListInfo, &mediaElementList.m_mediaTrackListInfo, sizeof(BTRMGR_MediaElementListInfo_t));
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_SelectMediaElement (
    unsigned char                  index_of_adapter,
    BTRMgrDeviceHandle             deviceHandle,
    BTRMgrMediaElementHandle       mediaElementHandle,
    BTRMGR_MediaElementType_t      mediaElementType

) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMMediaElementListInfo_t   mediaElementList;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&mediaElementList, 0, sizeof(BTRMGR_IARMMediaElementListInfo_t));
    mediaElementList.m_adapterIndex         = index_of_adapter;
    mediaElementList.m_deviceHandle         = deviceHandle;
    mediaElementList.m_mediaElementHandle   = mediaElementHandle;
    mediaElementList.m_mediaElementType     = mediaElementType;
    


    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SELECT_MEDIA_ELEMENT, (void*)&mediaElementList, sizeof(BTRMGR_IARMMediaElementListInfo_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_GetLeProperty (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    const char*                  apBtrUuid,
    BTRMGR_LeProperty_t          aenLeProperty,
    void*                        apBtrPropValue
) {

    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMLeProperty_t leProperty;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter || !apBtrUuid) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }
    
    memset (&leProperty, 0, sizeof(BTRMGR_IARMLeProperty_t));
    leProperty.m_adapterIndex = index_of_adapter;
    leProperty.m_deviceHandle = handle;
    leProperty.m_enLeProperty = aenLeProperty;
    strncpy(leProperty.m_propUuid, apBtrUuid, BTRMGR_MAX_STR_LEN-1);

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_LE_PROPERTY, (void*)&leProperty, sizeof(BTRMGR_IARMLeProperty_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        switch (aenLeProperty) {
        case BTRMGR_LE_PROP_UUID:
            memcpy (apBtrPropValue, &leProperty.m_uuidList, sizeof(leProperty.m_uuidList));
            break;
        case BTRMGR_LE_PROP_DEVICE:
            memcpy (apBtrPropValue, &leProperty.m_devicePath, sizeof(leProperty.m_devicePath)-1);
            break;
        case BTRMGR_LE_PROP_SERVICE:
            memcpy (apBtrPropValue, &leProperty.m_servicePath, sizeof(leProperty.m_servicePath)-1);
            break;
        case BTRMGR_LE_PROP_CHAR:
            memcpy (apBtrPropValue, &leProperty.m_characteristicPath, sizeof(leProperty.m_characteristicPath)-1);
            break;
        case BTRMGR_LE_PROP_VALUE:
            memcpy (apBtrPropValue, &leProperty.m_value,  sizeof(leProperty.m_value)-1);
            break;
        case BTRMGR_LE_PROP_PRIMARY:
            *(unsigned char*)apBtrPropValue = leProperty.m_primary;
            break;
        case BTRMGR_LE_PROP_NOTIFY:
            *(unsigned char*)apBtrPropValue = leProperty.m_notifying;
            break;
        case BTRMGR_LE_PROP_FLAGS:
            memcpy (apBtrPropValue, &leProperty.m_flags, sizeof(leProperty.m_flags));
        default:
            break;
        }
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_PerformLeOp (
    unsigned char                index_of_adapter,
    BTRMgrDeviceHandle           handle,
    const char*                  aBtrLeUuid,
    BTRMGR_LeOp_t                aLeOpType,
    char*                        aLeOpArg,
    char*                        rOpResult
) {
    BTRMGR_Result_t rc    = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMLeOp_t leOp;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter || NULL == aBtrLeUuid) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&leOp, 0, sizeof(BTRMGR_IARMLeOp_t));
    leOp.m_adapterIndex = index_of_adapter;
    leOp.m_deviceHandle = handle;
    leOp.m_leOpType     = aLeOpType;
    strncpy(leOp.m_opArg, aLeOpArg,   BTRMGR_MAX_STR_LEN-1);
    strncpy(leOp.m_uuid,  aBtrLeUuid, BTRMGR_MAX_STR_LEN-1);

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_PERFORM_LE_OP, (void*)&leOp, sizeof(BTRMGR_IARMLeOp_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);
    
    if (IARM_RESULT_SUCCESS == retCode) {
        if (BTRMGR_LE_OP_READ_VALUE == aLeOpType) {
            memcpy(rOpResult, leOp.m_opRes, BTRMGR_MAX_STR_LEN - 1);
        }
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}


BTRMGR_Result_t
BTRMGR_SetAudioInServiceState (
    unsigned char                index_of_adapter,
    unsigned char                aui8ServiceState
) {
    BTRMGR_Result_t rc    = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMAudioInServiceState_t audioInServiceState;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&audioInServiceState, 0, sizeof(BTRMGR_IARMAudioInServiceState_t));
    audioInServiceState.m_adapterIndex = index_of_adapter;
    audioInServiceState.m_serviceState = aui8ServiceState;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_AUDIO_IN_SERVICE_STATE, (void*)&audioInServiceState, sizeof(BTRMGR_IARMAudioInServiceState_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SetHidGamePadServiceState (
    unsigned char                index_of_adapter,
    unsigned char                aui8ServiceState
) {
    BTRMGR_Result_t rc    = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMHidGamePadServiceState_t hidGamePadServiceState;

    if (BTRMGR_ADAPTER_COUNT_MAX < index_of_adapter) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
        return rc;
    }

    memset (&hidGamePadServiceState, 0, sizeof(BTRMGR_IARMHidGamePadServiceState_t));
    hidGamePadServiceState.m_adapterIndex = index_of_adapter;
    hidGamePadServiceState.m_serviceState = aui8ServiceState;

    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_SET_HID_GAMEPAD_SERVICE_STATE, (void*)&hidGamePadServiceState, sizeof(BTRMGR_IARMHidGamePadServiceState_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO ("Success\n");
    }
    else {
        rc = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR ("Failed; RetCode = %d\n", retCode);
    }

    return rc;
}

BTRMGR_Result_t
BTRMGR_SysDiagInfo(
    unsigned char aui8AdapterIdx,
    char *apDiagElement,
    char *apValue,
    BTRMGR_LeOp_t aOpType
) {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMDiagInfo_t lDiagInfo;

    memset(&lDiagInfo, 0, sizeof(BTRMGR_IARMDiagInfo_t));

    if (BTRMGR_ADAPTER_COUNT_MAX < aui8AdapterIdx) {
        lenBtrMgrResult = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR("Input is invalid\n");
        return lenBtrMgrResult;
    }

    lDiagInfo.m_adapterIndex = aui8AdapterIdx;
    strncpy(lDiagInfo.m_UUID, apDiagElement, BTRMGR_MAX_STR_LEN - 1);
    lDiagInfo.m_OpType = aOpType;
    if (BTRMGR_LE_OP_WRITE_VALUE == aOpType)
    {
        strncpy(lDiagInfo.m_DiagInfo, apValue, BTRMGR_MAX_STR_LEN - 1);
    }
    BTRMGRLOG_INFO("calling BTRMGR_IARM_METHOD_GET_SYS_DIAG_INFO\n");
    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_GET_SYS_DIAG_INFO, (void*)&lDiagInfo, sizeof(BTRMGR_IARMDiagInfo_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        if (BTRMGR_LE_OP_READ_VALUE == aOpType)
        {
            strncpy(apValue, lDiagInfo.m_DiagInfo, BTRMGR_MAX_STR_LEN - 1);
        }
        BTRMGRLOG_INFO("Success\n");
    }
    else {
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return lenBtrMgrResult;
}

BTRMGR_Result_t
BTRMGR_ConnectToWifi(
    unsigned char aui8AdapterIdx,
    char *apSSID,
    char *apPassword,
    int aSecMode
)     {
    BTRMGR_Result_t lenBtrMgrResult = BTRMGR_RESULT_SUCCESS;
    IARM_Result_t retCode = IARM_RESULT_SUCCESS;
    BTRMGR_IARMWifiConnectInfo_t lWifiInfo;

    memset(&lWifiInfo, 0, sizeof(BTRMGR_IARMWifiConnectInfo_t));

    if (BTRMGR_ADAPTER_COUNT_MAX < aui8AdapterIdx) {
        lenBtrMgrResult = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR("Input is invalid\n");
        return lenBtrMgrResult;
    }

    lWifiInfo.m_adapterIndex = aui8AdapterIdx;
    strncpy(lWifiInfo.m_SSID, apSSID, BTRMGR_MAX_STR_LEN - 1);
    strncpy(lWifiInfo.m_Password, apPassword, BTRMGR_MAX_STR_LEN - 1);
    lWifiInfo.m_SecMode = aSecMode;
    retCode = IARM_Bus_Call_with_IPCTimeout(IARM_BUS_BTRMGR_NAME, BTRMGR_IARM_METHOD_WIFI_CONNECT_INFO, (void*)&lWifiInfo, sizeof(BTRMGR_IARMWifiConnectInfo_t), BTRMGR_IARM_METHOD_CALL_TIMEOUT_DEFAULT_MS);

    if (IARM_RESULT_SUCCESS == retCode) {
        BTRMGRLOG_INFO("Success\n");
    }
    else {
        lenBtrMgrResult = BTRMGR_RESULT_GENERIC_FAILURE;
        BTRMGRLOG_ERROR("Failed; RetCode = %d\n", retCode);
    }

    return lenBtrMgrResult;
}

const char*
BTRMGR_GetDeviceTypeAsString (
    BTRMGR_DeviceType_t  type
) {
    if (type == BTRMGR_DEVICE_TYPE_WEARABLE_HEADSET)
        return "WEARABLE HEADSET";
    else if (type == BTRMGR_DEVICE_TYPE_HANDSFREE)
        return "HANDSFREE";
    else if (type == BTRMGR_DEVICE_TYPE_MICROPHONE)
        return "MICROPHONE";
    else if (type == BTRMGR_DEVICE_TYPE_LOUDSPEAKER)
        return "LOUDSPEAKER";
    else if (type == BTRMGR_DEVICE_TYPE_HEADPHONES)
        return "HEADPHONES";
    else if (type == BTRMGR_DEVICE_TYPE_PORTABLE_AUDIO)
        return "PORTABLE AUDIO DEVICE";
    else if (type == BTRMGR_DEVICE_TYPE_CAR_AUDIO)
        return "CAR AUDIO";
    else if (type == BTRMGR_DEVICE_TYPE_STB)
        return "STB";
    else if (type == BTRMGR_DEVICE_TYPE_HIFI_AUDIO_DEVICE)
        return "HIFI AUDIO DEVICE";
    else if (type == BTRMGR_DEVICE_TYPE_VCR)
        return "VCR";
    else if (type == BTRMGR_DEVICE_TYPE_VIDEO_CAMERA)
        return "VIDEO CAMERA";
    else if (type == BTRMGR_DEVICE_TYPE_CAMCODER)
        return "CAMCODER";
    else if (type == BTRMGR_DEVICE_TYPE_VIDEO_MONITOR)
        return "VIDEO MONITOR";
    else if (type == BTRMGR_DEVICE_TYPE_TV)
        return "TV";
    else if (type == BTRMGR_DEVICE_TYPE_VIDEO_CONFERENCE)
        return "VIDEO CONFERENCING";
    else if (type == BTRMGR_DEVICE_TYPE_SMARTPHONE)
        return "SMARTPHONE";
    else if (type == BTRMGR_DEVICE_TYPE_TABLET)
        return "TABLET";
    else if (type == BTRMGR_DEVICE_TYPE_TILE)
        return "LE TILE";
    else if ((type == BTRMGR_DEVICE_TYPE_HID) || (type == BTRMGR_DEVICE_TYPE_HID_GAMEPAD))
        return "HUMAN INTERFACE DEVICE";
    else
        return "UNKNOWN DEVICE";
}


BTRMGR_Result_t
BTRMGR_RegisterEventCallback (
    BTRMGR_EventCallback eventCallback
) {
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    if (!eventCallback) {
        rc = BTRMGR_RESULT_INVALID_INPUT;
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        m_eventCallbackFunction = eventCallback;
        BTRMGRLOG_INFO ("Success\n");
    }

    return rc;
}


/**********************/
/* Incoming Callbacks */
/**********************/
static void
btrMgrdeviceCallback (
    const char*     owner,
    IARM_EventId_t  eventId,
    void*           pData,
    size_t          len
) {

    if (NULL == pData) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        BTRMGR_EventMessage_t *pReceivedEvent = (BTRMGR_EventMessage_t*)pData;
        BTRMGR_EventMessage_t newEvent;

        if ((BTRMGR_IARM_EVENT_DEVICE_OUT_OF_RANGE                  == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_STARTED             == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE              == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_COMPLETE            == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_PAIRING_COMPLETE              == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_COMPLETE            == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_CONNECTION_COMPLETE           == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_COMPLETE           == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_PAIRING_FAILED                == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_UNPAIRING_FAILED              == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_CONNECTION_FAILED             == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_DISCONNECT_FAILED             == eventId) ||
            (BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST       == eventId) ||
            (BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST    == eventId) ||
            (BTRMGR_IARM_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST   == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_OP_READY                      == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_OP_INFORMATION                == eventId) ||
            (BTRMGR_IARM_EVENT_DEVICE_FOUND                         == eventId) ){

            memcpy (&newEvent, pReceivedEvent, sizeof(BTRMGR_EventMessage_t));


            if (BTRMGR_IARM_EVENT_DEVICE_FOUND == eventId) {
                //TODO: Delay posting device found event to XRE by 3 secs
                //TODO: Should be part of btrMgr not the ext-interface
                sleep(3); 
            }

            if (m_eventCallbackFunction)
                m_eventCallbackFunction (newEvent);
              
            BTRMGRLOG_TRACE ("posted event(%d) from the adapter(%d) to listener successfully\n", newEvent.m_eventType, newEvent.m_adapterIndex);

            if (BTRMGR_IARM_EVENT_DEVICE_DISCOVERY_UPDATE == eventId) {
                BTRMGRLOG_TRACE("Name = %s\n\n", newEvent.m_discoveredDevice.m_name);
            }
        }
        else {
            BTRMGRLOG_ERROR ("Event is invalid\n");
        }
    }
    
    return;
}


static void
btrMgrMediaCallback (
    const char*     owner,
    IARM_EventId_t  eventId,
    void*           pData,
    size_t          len
) {

    if (NULL == pData) {
        BTRMGRLOG_ERROR ("Input is invalid\n");
    }
    else {
        BTRMGR_EventMessage_t *pReceivedEvent = (BTRMGR_EventMessage_t*)pData;
        BTRMGR_EventMessage_t newEvent;

        if ((BTRMGR_IARM_EVENT_MEDIA_TRACK_STARTED              == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_TRACK_PLAYING              == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_TRACK_PAUSED               == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_TRACK_STOPPED              == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_TRACK_POSITION             == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_TRACK_CHANGED              == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYBACK_ENDED             == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_NAME                == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_VOLUME              == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_EQUALIZER_OFF       == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_EQUALIZER_ON        == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_OFF         == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_ALLTRACKS   == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_SHUFFLE_GROUP       == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_OFF          == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_SINGLETRACK  == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_ALLTRACKS    == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_REPEAT_GROUP        == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_ALBUM_INFO                 == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_ARTIST_INFO                == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_GENRE_INFO                 == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_COMPILATION_INFO           == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYLIST_INFO              == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_TRACKLIST_INFO             == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_MUTE                == eventId) ||
            (BTRMGR_IARM_EVENT_MEDIA_PLAYER_UNMUTE              == eventId) ){
            memcpy (&newEvent, pReceivedEvent, sizeof(BTRMGR_EventMessage_t));

            if (m_eventCallbackFunction)
                m_eventCallbackFunction (newEvent);

            BTRMGRLOG_INFO ("posted media event(%d) from the adapter(%d) to listener successfully\n", newEvent.m_eventType, newEvent.m_adapterIndex);
        }
        else {
            BTRMGRLOG_ERROR ("Event is invalid\n");
        }
    }
    return;
}

