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


#define BTRMGR_PI_DEVID_LEN     17
#define BTRMGR_PI_VOL_LEN       4

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
    BTRMGRLOG_INFO ("Reading file - %s\n", fileName);

    if (0 == access(fileName, F_OK)) {
        fp = fopen(fileName, "r");
        if (fp == NULL) {
            BTRMGRLOG_ERROR ("Could not open file - %s\n", fileName);
        }
        else {
            int ch_count = 0;
            fseek(fp, 0, SEEK_END);
            ch_count = ftell(fp);
            fseek(fp, 0, SEEK_SET);

            if(ch_count > 0) {
                fileContent = (char *) malloc(sizeof(char) * (ch_count + 1));
                if (fileContent != NULL) {
                    if (fread(fileContent, 1, ch_count, fp) != ch_count) {
                        BTRMGRLOG_ERROR ("fileContent not read - %s\n", fileName);
                    }  //CID:23373 - Checked return
                }
                fileContent[ch_count] ='\0';
           }  //CID:23376 - Negative retuns
           fclose(fp);
           BTRMGRLOG_DEBUG ("Reading %s success, Content = %s \n", fileName,fileContent);
       }
    }
    else {
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
    BTRMGRLOG_INFO("Writing data to file %s\n" ,fileName);

    fp = fopen(fileName, "w");
    if (fp == NULL) {
        BTRMGRLOG_ERROR ("Could not open file to write, -  %s\n" ,fileName);
    }
    else {
        char* fileContent = cJSON_Print(profileData);
        fprintf(fp, "%s", fileContent);
        fclose(fp);
        BTRMGRLOG_DEBUG ("Writing data to file - %s, Content - %s\n" ,fileName,fileContent);
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

    memset(piHandle->piData.limitBeaconDetection, '\0', BTRMGR_NAME_LEN_MAX);

    *hBTRMgrPiHdl = (tBTRMgrPIHdl) piHandle;
    return eBTRMgrSuccess;
}


eBTRMgrRet
BTRMgr_PI_DeInit (
    tBTRMgrPIHdl hBTRMgrPiHdl
) {
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;

    if ( NULL != pstBtrMgrPiHdl) {
        free((void*)pstBtrMgrPiHdl);
        pstBtrMgrPiHdl = NULL;
        BTRMGRLOG_INFO ("BTRMgr_PI_DeInit SUCCESS\n");
        return eBTRMgrSuccess;
    }
    else {
        BTRMGRLOG_WARN ("BTRMgr PI handle is not Inited(NULL)\n");
        return eBTRMgrFailure;
    }
}

/*Creating a persistent get/set api for limiting beacon detection */
eBTRMgrRet
BTRMgr_PI_GetLEBeaconLimitingStatus (
    BTRMGR_Beacon_PersistentData_t*    persistentData
) {
    char *persistent_file_content = NULL;

    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);
    if(persistent_file_content == NULL) {
        // Seems like file is empty
        return eBTRMgrFailure;
    }

    cJSON *btData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if(btData == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }

    cJSON * limitDetection = cJSON_GetObjectItem(btData,"LimitBeaconDetection");
    if(limitDetection == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not able to get limit value for beacon detection from JSON\n");
        return eBTRMgrFailure;
    }

    strcpy(persistentData->limitBeaconDetection ,limitDetection->valuestring);
    return eBTRMgrSuccess;
}

eBTRMgrRet
BTRMgr_PI_SetLEBeaconLimitingStatus (
    BTRMGR_Beacon_PersistentData_t*    persistentData
) {
    char *persistent_file_content = NULL;

    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);
    if(persistent_file_content == NULL) {
        // Seems like file is empty
        return eBTRMgrFailure;
    }

    cJSON *btData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if(btData == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }

    /*Adding limit for beacon detection*/
    BTRMGRLOG_DEBUG ("Appending object to JSON file\n");
    cJSON *limitDetection = cJSON_GetObjectItem(btData,"LimitBeaconDetection");
    if(limitDetection == NULL) {
        cJSON_AddStringToObject(btData, "LimitBeaconDetection", persistentData->limitBeaconDetection);
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH, btData);
    }
    else if (strcmp(limitDetection->valuestring, persistentData->limitBeaconDetection)) { // Only if Beacon change write persistent memory
        strcpy(limitDetection->valuestring, persistentData->limitBeaconDetection);
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH, btData);
    }

    return eBTRMgrSuccess;
}

#ifdef RDKTV_PERSIST_VOLUME_SKY
/*Creating a persistent get/set api for value of volume */
eBTRMgrRet
BTRMgr_PI_GetVolume (
    BTRMGR_Volume_PersistentData_t*    persistentData
) {
    char *persistent_file_content = NULL;

    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);
    if(persistent_file_content == NULL) {
        // Seems like file is empty
        return eBTRMgrFailure;
    }

    cJSON *btData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if(btData == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }

    cJSON * Volume = cJSON_GetObjectItem(btData,"Volume");
    if(Volume == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not able to get value of volume detection from JSON\n");
        return eBTRMgrFailure;
    }

    persistentData->Volume  = (unsigned char ) atoi (Volume->valuestring);
    return eBTRMgrSuccess;
}

eBTRMgrRet
BTRMgr_PI_SetVolume (
    BTRMGR_Volume_PersistentData_t*    persistentData
) {
    char *persistent_file_content = NULL;

    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);
    if(persistent_file_content == NULL) {
        // Seems like file is empty
        return eBTRMgrFailure;
    }

    cJSON *btData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if(btData == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }

    /*Adding value of volume */
    BTRMGRLOG_INFO ("Appending object to JSON file\n");
    char VolStr[BTRMGR_PI_VOL_LEN];
    memset(VolStr, '\0', BTRMGR_PI_VOL_LEN);
    sprintf(VolStr, "%d",persistentData->Volume);
    cJSON *Volume = cJSON_GetObjectItem(btData,"Volume");
    if (Volume == NULL) {
        cJSON_AddStringToObject(btData, "Volume", VolStr);
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH, btData);
    }
    else if (strcmp(Volume->valuestring, VolStr)) { // Only if Volume changed write to persistence
        strcpy(Volume->valuestring, VolStr);
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH, btData);
    }


    return eBTRMgrSuccess;
}

/*Creating a persistent get/set api for limiting beacon detection */
eBTRMgrRet
BTRMgr_PI_GetMute (
    BTRMGR_Mute_PersistentData_t*    persistentData
) {
    char *persistent_file_content = NULL;

    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);
    if(persistent_file_content == NULL) {
        // Seems like file is empty
        return eBTRMgrFailure;
    }

    cJSON *btData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if(btData == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }

    cJSON *Mute = cJSON_GetObjectItem(btData,"Mute");
    if(Mute == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not able to get limit value for beacon detection from JSON\n");
        return eBTRMgrFailure;
    }

    strcpy(persistentData->Mute ,Mute->valuestring);
    return eBTRMgrSuccess;
}

eBTRMgrRet
BTRMgr_PI_SetMute (
    BTRMGR_Mute_PersistentData_t*    persistentData
) {
    char *persistent_file_content = NULL;

    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);
    if(persistent_file_content == NULL) {
        // Seems like file is empty
        return eBTRMgrFailure;
    }

    cJSON *btData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if(btData == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }

    /*Adding limit for beacon detection*/
    BTRMGRLOG_INFO ("Appending object to JSON file\n");
    cJSON *Mute = cJSON_GetObjectItem(btData,"Mute");
    if (Mute == NULL) {
        cJSON_AddStringToObject(btData, "Mute", persistentData->Mute);
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH, btData);
    }
    else if (strcmp(Mute->valuestring, persistentData->Mute)) { // Only if Mute changed write to persistence
        strcpy(Mute->valuestring, persistentData->Mute);
        writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH, btData);
    }

    return eBTRMgrSuccess;
}
#endif

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

    if (pstBtrMgrPiHdl == NULL) {
        BTRMGRLOG_ERROR ("PI Handle not initialized\n");
        return eBTRMgrFailure;
    }

    // Read file and fill persistent_file_content
    persistent_file_content = readPersistentFile(BTRMGR_PERSISTENT_DATA_PATH);
    // Seems like file is empty
    if (persistent_file_content == NULL) {
        memset (persistentData->limitBeaconDetection, '\0', BTRMGR_NAME_LEN_MAX);
        strcpy(persistentData->limitBeaconDetection, "false");
        return eBTRMgrFailure;
    }

    cJSON *btProfileData = cJSON_Parse(persistent_file_content);
    free(persistent_file_content);
    if (btProfileData == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not parse JSON data file - Corrupted JSON\n");
        return eBTRMgrFailure;
    }

    cJSON * adpaterIdptr = cJSON_GetObjectItem(btProfileData,"AdapterId");
    if (adpaterIdptr == NULL) {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not able to get AdapterId from JSON\n");
        return eBTRMgrFailure;
    }
    strcpy(persistentData->adapterId,adpaterIdptr->valuestring);

    cJSON *limitDetection = cJSON_GetObjectItem(btProfileData,"LimitBeaconDetection");
    if(limitDetection != NULL) {
        strcpy(persistentData->limitBeaconDetection, limitDetection->valuestring);
    }
    else {
        memset (persistentData->limitBeaconDetection, '\0', BTRMGR_NAME_LEN_MAX);
        strcpy(persistentData->limitBeaconDetection, "false");
    }

    cJSON *btProfiles = cJSON_GetObjectItem(btProfileData,"Profiles");
    if (btProfiles != NULL) {
        // Read Profile details
        profileCount = cJSON_GetArraySize(btProfiles);
        BTRMGRLOG_DEBUG ("Successfully Parsed Persistent profile, Profile count = %d\n",profileCount);

        persistentData->numOfProfiles = profileCount;
        for (pcount = 0; pcount < profileCount; pcount++ ) {
            char* profileId = NULL;
            cJSON *profile = cJSON_GetArrayItem(btProfiles, pcount);
            if(cJSON_GetObjectItem(profile,"ProfileId")) {

                profileId = cJSON_GetObjectItem(profile,"ProfileId")->valuestring;
                if (profileId) {
                    strcpy(persistentData->profileList[pcount].profileId,profileId);

                    // Get Device Details
                    cJSON *btDevices = cJSON_GetObjectItem(profile,"Devices");
                    deviceCount = cJSON_GetArraySize(btDevices);
                    persistentData->profileList[pcount].numOfDevices = deviceCount;
                    BTRMGRLOG_DEBUG ("Parsing device details, %d devices found for profile %s\n",deviceCount,profileId);

                    for(dcount = 0; dcount<deviceCount; dcount++) {
                        char* deviceId = NULL;
                        int isConnect  = 0;
                        cJSON *device = cJSON_GetArrayItem(btDevices, dcount);
                        if(cJSON_GetObjectItem(device,"DeviceId")) {
                            deviceId = cJSON_GetObjectItem(device,"DeviceId")->valuestring;
                            if (deviceId) {  //CID:23328. 23379 - Reverse_inull and 23385,23443 - Forward null
                                persistentData->profileList[pcount].deviceList[dcount].deviceId =  strtoll(deviceId,NULL,10);

                                if(cJSON_GetObjectItem(device,"ConnectionStatus")->valueint)
                                   isConnect =  cJSON_GetObjectItem(device,"ConnectionStatus")->valueint;

                                persistentData->profileList[pcount].deviceList[dcount].isConnected = isConnect;
                                BTRMGRLOG_DEBUG ("Parsing device details, Device- %s, Status-%d, Profile-%s\n",deviceId,isConnect,profileId);
                            }
                        }
                    }
                }
            }
        }
    }
    else {
        // Corrupted JSON
        BTRMGRLOG_ERROR ("Could not able to get Profile Lists from JSON\n");
        return eBTRMgrFailure;
    }

    return eBTRMgrSuccess;
}


eBTRMgrRet
BTRMgr_PI_AddProfile (
    tBTRMgrPIHdl        hBTRMgrPiHdl,
    BTRMGR_Profile_t*    persistProfile
) {  //CID:23369 and 23437 - Pass by Value
    // Get Current persistent data in order to append
    BTRMGR_PersistentData_t piData;
    int pcount = 0;
    int isObjectAdded = 0;

    // Validate Handle
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;

    if ((pstBtrMgrPiHdl == NULL) && (persistProfile  == NULL)) {
        BTRMGRLOG_ERROR ("PI Handle and Persist profile not initialized\n");
        return eBTRMgrFailure;
    }

    BTRMgr_PI_GetAllProfiles(hBTRMgrPiHdl,&piData);
    int profileCount = piData.numOfProfiles;
    if( profileCount > 0) { // Seems like some profile are already there, So append data
        BTRMGRLOG_DEBUG ("Profile Count >0 need to append profile \n");

        for(pcount=0; pcount<profileCount ; pcount++) {
            if(strcmp(piData.profileList[pcount].profileId,persistProfile->profileId) == 0) { // Profile already exists simply add device
                BTRMGRLOG_DEBUG ("Profile entry already exists,need to add device alone  \n");
                int deviceCnt = piData.profileList[pcount].numOfDevices;
                // Check if it is a duplicate entry
                int dcount = 0;
                for(dcount = 0; dcount < deviceCnt; dcount++) {
                    if(piData.profileList[pcount].deviceList[dcount].deviceId == persistProfile->deviceId) {
                        // Its a duplicate entry
                        BTRMGRLOG_ERROR ("Adding Failed Duplicate entry found - %lld\n",persistProfile->deviceId);
                        return eBTRMgrFailure;
                    }
                }
                // Not a duplicate add device to same profile
                BTRMGRLOG_DEBUG ("Not a duplicate device appending new device to deviceLits- %lld  \n",persistProfile->deviceId);
                piData.profileList[pcount].deviceList[dcount].deviceId = persistProfile->deviceId;
                piData.profileList[pcount].deviceList[dcount].isConnected = persistProfile->isConnect;
                piData.profileList[pcount].numOfDevices++;
                isObjectAdded = 1;
                break;
            }
        }

        if(0 == isObjectAdded) { // Seems like its a new profile add it
            BTRMGRLOG_DEBUG ("New Profile found, add to profile list -  %s \n",persistProfile->profileId);
            piData.profileList[pcount].numOfDevices = 1;
            strcpy(piData.profileList[pcount].profileId,persistProfile->profileId);
            piData.profileList[pcount].deviceList[0].deviceId = persistProfile->deviceId;
            piData.profileList[pcount].deviceList[0].isConnected = persistProfile->isConnect;
            piData.numOfProfiles++;
            isObjectAdded = 1;
            BTRMGRLOG_DEBUG ("New Profile added -  %s \n",persistProfile->profileId);
        }
    }
    else { // Data is empty now, Lets create one and add
        BTRMGRLOG_DEBUG ("Data is empty creating new entry -  %s \n",persistProfile->profileId);
        strcpy(piData.adapterId,persistProfile->adapterId);
        piData.numOfProfiles = 1;
        piData.profileList[0].numOfDevices =1;
        strcpy(piData.profileList[0].profileId,persistProfile->profileId );
        piData.profileList[0].deviceList[0].deviceId = persistProfile->deviceId;
        piData.profileList[0].deviceList[0].isConnected = persistProfile->isConnect;
        isObjectAdded = 1;
    }

    if(isObjectAdded) {
        BTRMGRLOG_DEBUG ("Writing changes to file -  %s \n",persistProfile->profileId);
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
    if (pstBtrMgrPiHdl == NULL) {
        BTRMGRLOG_ERROR ("PI Handle not initialized\n");
        return eBTRMgrFailure;
    }

    profileCount = persistentData->numOfProfiles;
    piData = cJSON_CreateObject();
    Profiles = cJSON_CreateArray();
    BTRMGRLOG_DEBUG ("Writing object to JSON\n");
    for (pcount = 0; pcount <profileCount; pcount++) {
        // Get All Device details first
        int deviceCount = persistentData->profileList[pcount].numOfDevices;
        if(deviceCount > 0) {
            BTRMGRLOG_DEBUG ("Device count > 0 , Count = %d\n",deviceCount);
            devices = cJSON_CreateArray();
            for(dcount = 0; dcount<deviceCount; dcount++) {
                cJSON* device = cJSON_CreateObject();
                char deviceId[BTRMGR_PI_DEVID_LEN];
                memset(deviceId, '\0', BTRMGR_PI_DEVID_LEN);
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
        else {
            BTRMGRLOG_ERROR ("Empty device list could not set\n");
            //return eBTRMgrFailure;
        }
    }

    if (profileCount != 0) {
        cJSON_AddStringToObject(piData,"AdapterId",persistentData->adapterId);
        cJSON_AddItemToObject(piData, "Profiles", Profiles);
        BTRMGRLOG_DEBUG ("Writing Profile details - %s\n",persistentData->profileList[pcount].profileId);
    }
    else { // No profiles exists empty file
        BTRMGRLOG_DEBUG ("Writing empty data\n");
    }

    if (persistentData->limitBeaconDetection && *(persistentData->limitBeaconDetection) != '\0') {
        cJSON_AddStringToObject(piData, "LimitBeaconDetection", persistentData->limitBeaconDetection);
    }

    writeToPersistentFile(BTRMGR_PERSISTENT_DATA_PATH,piData);
    cJSON_Delete(piData);

    return eBTRMgrSuccess;
}

eBTRMgrRet
BTRMgr_PI_RemoveProfile (
    tBTRMgrPIHdl        hBTRMgrPiHdl,
    BTRMGR_Profile_t*    persistProfile
) {
    // Get Current persistent data in order to append
    BTRMGR_PersistentData_t piData;
    int pcount = 0;
    int isObjectRemoved = 0;

    // Validate Handle
    stBTRMgrPIHdl*  pstBtrMgrPiHdl = (stBTRMgrPIHdl*)hBTRMgrPiHdl;
    if ((pstBtrMgrPiHdl  == NULL) && (persistProfile  == NULL)) {
        BTRMGRLOG_ERROR ("PI Handle and Persist profile not initialized\n");
        return eBTRMgrFailure;
    }

    BTRMGRLOG_DEBUG ("Removing profile - %s\n",persistProfile->profileId);
    BTRMgr_PI_GetAllProfiles(hBTRMgrPiHdl,&piData);
    int profileCount = piData.numOfProfiles;
    if( profileCount > 0) { // Seems like some profile are already
        BTRMGRLOG_DEBUG ("Profiles not empty- %s\n",persistProfile->profileId);
        for(pcount = 0; pcount < profileCount; pcount++) {
            if(strcmp(piData.profileList[pcount].profileId,persistProfile->profileId) == 0) { // Profile already exists simply remove device
                BTRMGRLOG_DEBUG ("Profile match found - %s find device\n",persistProfile->profileId);
                int deviceCnt = piData.profileList[pcount].numOfDevices;
                // Check if it is a duplicate entry
                int dcount = 0;
                for(dcount = 0; dcount < deviceCnt; dcount++) {
                    if(piData.profileList[pcount].deviceList[dcount].deviceId == persistProfile->deviceId) {
                        BTRMGRLOG_DEBUG ("Profile match found && Device Match Found - %s Deleting device %lld\n",
                                         persistProfile->profileId, persistProfile->deviceId);
                        piData.profileList[pcount].deviceList[dcount].deviceId = 0;
                        piData.profileList[pcount].deviceList[dcount].isConnected = 0;
                        piData.profileList[pcount].numOfDevices--;
                        isObjectRemoved = 1;
                        BTRMGRLOG_DEBUG ("Profile match found && Device Match Found - %s Delete Success %lld\n",
                                         persistProfile->profileId, persistProfile->deviceId);
                        break;
                    }
                }

                if(isObjectRemoved && piData.profileList[pcount].numOfDevices == 0) { // There is no more device exists so no need to profile
                    memset(piData.profileList[pcount].profileId, 0,BTRMGR_NAME_LEN_MAX );
                    piData.numOfProfiles--;
                }
            }
            else if (!isObjectRemoved && (pcount == (profileCount - 1))) {
                // Unknown profile not able to delete
                BTRMGRLOG_ERROR ("Profile Not found, Could not delete %s\n",persistProfile->profileId);
                return eBTRMgrFailure;
            }
        }

        if(isObjectRemoved && piData.numOfProfiles == 0) { // There is no more profiles exists
            memset(piData.adapterId, 0,BTRMGR_NAME_LEN_MAX );
        }

        if(isObjectRemoved) {
            BTRMgr_PI_SetAllProfiles(hBTRMgrPiHdl,&piData);
        }
    }
    else {
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
