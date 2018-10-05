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
/**
 * @file btrMgr_persistIface.c
 *
 * @description This file defines bluetooth manager's Persistent storage interfaces
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Ext lib Headers */
#include "cJSON.h"

/* Interface lib Headers */
#include "btrMgr_logger.h"

/* Local Headers */
#include "btrMgr_Types.h"
#include "btrMgr_persistIface.h"


typedef struct _stBTRMgrPIHdl {
    BTRMGR_PersistentData_t piData;
} stBTRMgrPIHdl;

/* Static Function Prototypes */
static char* readPersistentFile (char*  fileContent);
static void writeToPersistentFile (char* fileName,cJSON* profileData);

/* Local Op Threads Prototypes*/

/* Incoming Callbacks Prototypes*/

/* Static Function Definition */
static char*
readPersistentFile (
    char*   fileName
) {
    FILE *fp = NULL;
    char *fileContent = NULL;
    BTRMGRLOG_DEBUG ("Reading file - %s\n", fileName);

    if( 0 == access(fileName, F_OK) )
    { 
       fp = fopen(fileName, "r");
       if (fp == NULL)
       {
           BTRMGRLOG_ERROR ("Could not open file - %s\n", fileName);
       }
       else
       {
           int ch_count = 0;
           fseek(fp, 0, SEEK_END);
           ch_count = ftell(fp);
           fseek(fp, 0, SEEK_SET);
           fileContent = (char *) malloc(sizeof(char) * (ch_count + 1));
           fread(fileContent, 1, ch_count,fp);
           fileContent[ch_count] ='\0';
           BTRMGRLOG_DEBUG ("Reading %s success, Content = %s \n", fileName,fileContent);
           fclose(fp);
       }
    }
    else
    { 
       BTRMGRLOG_WARN ("File %s does not exist!!!", fileName);
    }
    return fileContent;
}

static void
writeToPersistentFile (
    char*   fileName,
    cJSON*  profileData
) {
    FILE *fp = NULL;
    BTRMGRLOG_DEBUG ("Writing data to file %s\n" ,fileName);
    fp = fopen(fileName, "w");
    if (fp == NULL)
    {
        BTRMGRLOG_ERROR ("Could not open file to write, -  %s\n" ,fileName);
    }
    else
    {
        char* fileContent = cJSON_Print(profileData);
        BTRMGRLOG_DEBUG ("Writing data to file - %s, Content - %s\n" ,fileName,fileContent);
        fprintf(fp, "%s", fileContent);
        fclose(fp);
        BTRMGRLOG_DEBUG ("File write Success\n");
    }

}

/*  Local Op Threads */


 /*  Interfaces  */
eBTRMgrRet
BTRMgr_PI_Init (
    tBTRMgrPIHdl* hBTRMgrPiHdl
) {
    stBTRMgrPIHdl* piHandle = NULL;

    if ((piHandle = (stBTRMgrPIHdl*)malloc (sizeof(stBTRMgrPIHdl))) == NULL) {
        BTRMGRLOG_ERROR ("BTRMgr_PI_Init FAILED\n");
        return eBTRMgrFailure;
    }
    *hBTRMgrPiHdl = (tBTRMgrPIHdl) piHandle;
    return eBTRMgrSuccess;
}


eBTRMgrRet
BTRMgr_PI_DeInit (
    tBTRMgrPIHdl hBTRMgrPiHdl
) {
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;

    if( NULL != pstBtrMgrPiHdl)
    {
        free((void*)pstBtrMgrPiHdl);
        pstBtrMgrPiHdl = NULL;
        BTRMGRLOG_INFO ("BTRMgr_PI_DeInit SUCCESS\n");
        return eBTRMgrSuccess;
    }
    else
    {
        BTRMGRLOG_WARN ("BTRMgr PI handle is not Inited(NULL)\n");
        return eBTRMgrFailure;
    }
}

eBTRMgrRet
BTRMgr_PI_GetAllProfiles (
    tBTRMgrPIHdl                hBTRMgrPiHdl,
    BTRMGR_PersistentData_t*    persistentData
) {
    char *persistent_file_content = NULL;
    int profileCount = 0;
    int deviceCount = 0;
    int pcount = 0;
    int dcount = 0;

    // Validate Handle
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;

    if (pstBtrMgrPiHdl == NULL)
    {
        BTRMGRLOG_ERROR ("PI Handle not initialized\n");
        return eBTRMgrFailure;
    }

    // Read file and fill persistent_file_content
    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);

    // Seems like file is empty
    if(NULL == persistent_file_content)
    {
        return eBTRMgrFailure;
    }
    cJSON *btProfileData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if(NULL == btProfileData)
    {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }
    cJSON * adpaterIdptr = cJSON_GetObjectItem(btProfileData,"AdapterId");
    if(NULL == adpaterIdptr)
    {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not able to get AdapterId from JSON\n");
        return eBTRMgrFailure;
    }
    strcpy(persistentData->adapterId,adpaterIdptr->valuestring);
    cJSON *btProfiles = cJSON_GetObjectItem(btProfileData,"Profiles");
    if(NULL != btProfiles )
    {
        // Read Profile details
        profileCount = cJSON_GetArraySize(btProfiles);
        BTRMGRLOG_DEBUG ("Successfully Parsed Persistent profile, Profile count = %d\n",profileCount);
        persistentData->numOfProfiles = profileCount;
        for(pcount = 0; pcount < profileCount; pcount++ )
        {
            char* profileId = NULL;
            cJSON *profile = cJSON_GetArrayItem(btProfiles, pcount);
            if(cJSON_GetObjectItem(profile,"ProfileId"))
                profileId = cJSON_GetObjectItem(profile,"ProfileId")->valuestring;
            strcpy(persistentData->profileList[pcount].profileId,profileId);

            // Get Device Details
            cJSON *btDevices = cJSON_GetObjectItem(profile,"Devices");
            deviceCount = cJSON_GetArraySize(btDevices);
            persistentData->profileList[pcount].numOfDevices = deviceCount;
            BTRMGRLOG_DEBUG ("Parsing device details, %d devices found for profile %s\n",deviceCount,profileId);
            for(dcount = 0; dcount<deviceCount; dcount++)
            {
                char* deviceId = NULL;
                int isConnect  = 0;
                cJSON *device = cJSON_GetArrayItem(btDevices, dcount);
                if(cJSON_GetObjectItem(device,"DeviceId"))
                    deviceId = cJSON_GetObjectItem(device,"DeviceId")->valuestring;
                persistentData->profileList[pcount].deviceList[dcount].deviceId =  strtoll(deviceId,NULL,10);
                if(cJSON_GetObjectItem(device,"ConnectionStatus")->valueint)
                    isConnect =  cJSON_GetObjectItem(device,"ConnectionStatus")->valueint;
                persistentData->profileList[pcount].deviceList[dcount].isConnected = isConnect;
                if(deviceId && profileId)
		{
                   BTRMGRLOG_DEBUG ("Parsing device details, Device- %s, Status-%d, Profile-%s\n",deviceId,isConnect,profileId);
		}
            }
        }
    }
    else
    {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not able to get Profile Lists from JSON\n");
        return eBTRMgrFailure;
    }
    return eBTRMgrSuccess;
}


eBTRMgrRet
BTRMgr_PI_AddProfile (
    tBTRMgrPIHdl        hBTRMgrPiHdl,
    BTRMGR_Profile_t    persistProfile
) {
    // Get Current persistent data in order to append
    BTRMGR_PersistentData_t piData;
    int pcount = 0;
    int isObjectAdded = 0;

    // Validate Handle
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;

    if (pstBtrMgrPiHdl == NULL)
    {
        BTRMGRLOG_ERROR ("PI Handle not initialized\n");
        return eBTRMgrFailure;
    }

    BTRMgr_PI_GetAllProfiles(hBTRMgrPiHdl,&piData);
    int profileCount = piData.numOfProfiles;
    if( profileCount > 0) // Seems like some profile are already there, So append data
    {
        BTRMGRLOG_DEBUG ("Profile Count >0 need to append profile \n");
        for(pcount=0; pcount<profileCount ; pcount++)
        {
            if(strcmp(piData.profileList[pcount].profileId,persistProfile.profileId) == 0) // Profile already exists simply add device
            {
                BTRMGRLOG_DEBUG ("Profile entry already exists,need to add device alone  \n");
                int deviceCnt = piData.profileList[pcount].numOfDevices;
                // Check if it is a duplicate entry
                int dcount = 0;
                for(dcount = 0; dcount < deviceCnt; dcount++)
                {
                    if(piData.profileList[pcount].deviceList[dcount].deviceId == persistProfile.deviceId)
                    {
                        // Its a duplicate entry
                        BTRMGRLOG_ERROR ("Adding Failed Duplicate entry found - %lld\n",persistProfile.deviceId);
                        return eBTRMgrFailure;
                    }
                }
                // Not a duplicate add device to same profile
                BTRMGRLOG_DEBUG ("Not a duplicate device appending new device to deviceLits- %lld  \n",persistProfile.deviceId);
                piData.profileList[pcount].deviceList[dcount].deviceId = persistProfile.deviceId;
                piData.profileList[pcount].deviceList[dcount].isConnected = persistProfile.isConnect;
                piData.profileList[pcount].numOfDevices++;
                isObjectAdded = 1;
                break;
            }
        }
        if(0 == isObjectAdded) // Seems like its a new profile add it
        {
            BTRMGRLOG_DEBUG ("New Profile found, add to profile list -  %s \n",persistProfile.profileId);
            piData.profileList[pcount].numOfDevices = 1;
            strcpy(piData.profileList[pcount].profileId,persistProfile.profileId);
            piData.profileList[pcount].deviceList[0].deviceId = persistProfile.deviceId;
            piData.profileList[pcount].deviceList[0].isConnected = persistProfile.isConnect;
            piData.numOfProfiles++;
            isObjectAdded = 1;
            BTRMGRLOG_DEBUG ("New Profile added -  %s \n",persistProfile.profileId);
        }
    }
    else // Data is empty now, Lets create one and add
    {
        BTRMGRLOG_DEBUG ("Data is empty creating new entry -  %s \n",persistProfile.profileId);
        strcpy(piData.adapterId,persistProfile.adapterId);
        piData.numOfProfiles = 1;
        piData.profileList[0].numOfDevices =1;
        strcpy(piData.profileList[0].profileId,persistProfile.profileId );
        piData.profileList[0].deviceList[0].deviceId = persistProfile.deviceId;
        piData.profileList[0].deviceList[0].isConnected = 1;
        isObjectAdded = 1;
    }
    if(isObjectAdded)
    {
        BTRMGRLOG_DEBUG ("Writing changes to file -  %s \n",persistProfile.profileId);
        BTRMgr_PI_SetAllProfiles(hBTRMgrPiHdl,&piData);
    }
    return eBTRMgrSuccess;
}

eBTRMgrRet
BTRMgr_PI_SetAllProfiles (
    tBTRMgrPIHdl                hBTRMgrPiHdl,
    BTRMGR_PersistentData_t*    persistentData
) {
    int profileCount = 0;
    int pcount = 0;
    int dcount = 0;
    cJSON *Profiles = NULL;
    cJSON *devices = NULL;
    cJSON *Profile = NULL;
    cJSON *piData = NULL;

    // Validate Handle
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;
    if (pstBtrMgrPiHdl == NULL)
    {
        BTRMGRLOG_ERROR ("PI Handle not initialized\n");
        return eBTRMgrFailure;
    }

    profileCount = persistentData->numOfProfiles;
    piData = cJSON_CreateObject();
    Profiles = cJSON_CreateArray();
    BTRMGRLOG_DEBUG ("Writing object to JSON\n");
    for(pcount = 0; pcount <profileCount; pcount++)
    {
        // Get All Device details first
        int deviceCount = persistentData->profileList[pcount].numOfDevices;
        if(deviceCount > 0)
        {
            BTRMGRLOG_DEBUG ("Device count > 0 , Count = %d\n",deviceCount);
            devices = cJSON_CreateArray();
            for(dcount = 0; dcount<deviceCount; dcount++)
            {
                cJSON* device = cJSON_CreateObject();
                char deviceId[15];
                sprintf(deviceId, "%lld",persistentData->profileList[pcount].deviceList[dcount].deviceId );
                cJSON_AddStringToObject(device, "DeviceId",deviceId );
                cJSON_AddNumberToObject(device, "ConnectionStatus",persistentData->profileList[pcount].deviceList[dcount].isConnected);
                cJSON_AddItemToArray(devices, device);
                BTRMGRLOG_DEBUG ("Device Added :- %s, Status- %d,\n",deviceId,persistentData->profileList[pcount].deviceList[dcount].isConnected);
            }
            Profile = cJSON_CreateObject();
            cJSON_AddStringToObject(Profile,"ProfileId",persistentData->profileList[pcount].profileId);
            cJSON_AddItemToObject(Profile, "Devices", devices);
            cJSON_AddItemToArray(Profiles, Profile);
        }
        else
        {
            BTRMGRLOG_ERROR ("Empty device list could not set\n");
            //return eBTRMgrFailure;
        }
    }
    if(profileCount != 0)
    {
        cJSON_AddStringToObject(piData,"AdapterId",persistentData->adapterId);
        cJSON_AddItemToObject(piData, "Profiles", Profiles);
        BTRMGRLOG_DEBUG ("Writing Profile details - %s\n",persistentData->profileList[pcount].profileId);
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH,piData);
        cJSON_Delete(piData);
    }
    else // No profiles exists empty file
    {
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH,piData);
        BTRMGRLOG_DEBUG ("Writing empty data\n");
        cJSON_Delete(piData);
    }

    return eBTRMgrSuccess;
}

eBTRMgrRet
BTRMgr_PI_RemoveProfile (
    tBTRMgrPIHdl        hBTRMgrPiHdl,
    BTRMGR_Profile_t    persistProfile
) {
    // Get Current persistent data in order to append
    BTRMGR_PersistentData_t piData;
    int pcount = 0;
    int isObjectRemoved = 0;

    // Validate Handle
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;
    if (pstBtrMgrPiHdl == NULL)
    {
        BTRMGRLOG_ERROR ("PI Handle not initialized\n");
        return eBTRMgrFailure;
    }

    BTRMGRLOG_DEBUG ("Removing profile - %s\n",persistProfile.profileId);
    BTRMgr_PI_GetAllProfiles(hBTRMgrPiHdl,&piData);
    int profileCount = piData.numOfProfiles;
    if( profileCount > 0) // Seems like some profile are already
    {
        BTRMGRLOG_DEBUG ("Profiles not empty- %s\n",persistProfile.profileId);
        for(pcount=0; pcount<profileCount ; pcount++)
        {
            if(strcmp(piData.profileList[pcount].profileId,persistProfile.profileId) == 0) // Profile already exists simply remove device
            {
                BTRMGRLOG_DEBUG ("Profile match found - %s find device\n",persistProfile.profileId);
                int deviceCnt = piData.profileList[pcount].numOfDevices;
                // Check if it is a duplicate entry
                int dcount = 0;
                for(dcount = 0; dcount < deviceCnt; dcount++)
                {
                    if(piData.profileList[pcount].deviceList[dcount].deviceId == persistProfile.deviceId)
                    {
                        BTRMGRLOG_DEBUG ("Profile match found && Device Match Found - %s Deleting device %lld\n",
                                         persistProfile.profileId, persistProfile.deviceId);
                        piData.profileList[pcount].deviceList[dcount].deviceId = 0;
                        piData.profileList[pcount].deviceList[dcount].isConnected = 0;
                        piData.profileList[pcount].numOfDevices--;
                        isObjectRemoved = 1;
                        BTRMGRLOG_DEBUG ("Profile match found && Device Match Found - %s Delete Success %lld\n",
                                         persistProfile.profileId, persistProfile.deviceId);
                        break;
                    }
                }
                if(isObjectRemoved && piData.profileList[pcount].numOfDevices == 0) // There is no more device exists so no need to profile
                {
                    memset(piData.profileList[pcount].profileId, 0,BTRMGR_NAME_LEN_MAX );
                    piData.numOfProfiles--;
                }
            }
            else
            {
                // Unknown profile not able to delete
                BTRMGRLOG_ERROR ("Profile Not found, Could not delete %s\n",persistProfile.profileId);
                return eBTRMgrFailure;
            }
        }
        if(isObjectRemoved && piData.numOfProfiles == 0) // There is no more profiles exists
        {
            memset(piData.adapterId, 0,BTRMGR_NAME_LEN_MAX );
        }
        if(isObjectRemoved)
        {
            BTRMgr_PI_SetAllProfiles(hBTRMgrPiHdl,&piData);
        }
    }
    else
    {
        // Profile is empty cant delete
        BTRMGRLOG_ERROR ("Nothing to delete, Profile is empty \n");
        return eBTRMgrFailure;
    }
    return eBTRMgrSuccess;
}


eBTRMgrRet
BTRMgr_PI_GetProfile (
    stBTRMgrPersistProfile* persistProfile, 
    char*                   profileName,
    char*                   deviceId
) {
    return eBTRMgrSuccess;
}


// Outgoing callbacks Registration Interfaces


/*  Incoming Callbacks */


 /* End of File */
