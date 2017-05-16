/*
 * If not stated otherwise in this file or this component's Licenses.txt file the
 * following copyright and licenses apply:
 *
 * Copyright 2017 RDK Management
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
 * @file btrMgr_persistIface.h
 *
 * @description This file defines bluetooth manager's Persistent storage interfaces
 *
 */
#ifndef __BTR_MGR_PERSIST_IFCE_H__
#define __BTR_MGR_PERSIST_IFCE_H__

#include "btrMgr_Types.h"

#define BTMGR_NAME_LEN_MAX            64
#define BTMGR_MAX_PERSISTENT_PROFILE_COUNT 5
#define BTMGR_MAX_PERSISTENT_DEVICE_COUNT	5
#define BTMGR_PERSISTENT_DATA_PATH "/opt/lib/bluetooth/btmgrPersist.json"
#define BTMGR_A2DP_SINK_PROFILE_ID "0x110b"


typedef void* tBTRMgrPIHdl;


typedef struct _stBTRMgrPersistDevice {
    unsigned long long deviceId;
    int isConnected;
} stBTRMgrPersistDevice;

typedef struct _stBTRMgrPersistProfile {
    unsigned char numOfDevices;
    char profileId[BTMGR_NAME_LEN_MAX];
    stBTRMgrPersistDevice deviceList[BTMGR_MAX_PERSISTENT_DEVICE_COUNT];
} stBTRMgrPersistProfile;


typedef struct _BTMGR_PersistentData_t {
    char adapterId[BTMGR_NAME_LEN_MAX];
    unsigned short numOfProfiles;
    stBTRMgrPersistProfile profileList[BTMGR_NAME_LEN_MAX];
} BTMGR_PersistentData_t;


typedef struct _BTMGR_Profile_t {
    char adapterId[BTMGR_NAME_LEN_MAX];
    char profileId[BTMGR_NAME_LEN_MAX];
    unsigned long long deviceId;
    int isConnect;
} BTMGR_Profile_t;

/* Interfaces */
eBTRMgrRet BTRMgr_PI_Init (tBTRMgrPIHdl* hBTRMgrPiHdl);
eBTRMgrRet BTRMgr_PI_DeInit (tBTRMgrPIHdl hBTRMgrPiHdl);

// Read all bluetooth profiles from json file and load to BTMGR_PersistentProfileList_t
eBTRMgrRet BTRMgr_PI_GetAllProfiles(tBTRMgrPIHdl hBTRMgrPiHdl,BTMGR_PersistentData_t* persistentData);

eBTRMgrRet BTRMgr_PI_SetAllProfiles (tBTRMgrPIHdl hBTRMgrPiHdl,BTMGR_PersistentData_t* persistentData);

//Add a single bluetooth profile to json file
eBTRMgrRet BTRMgr_PI_AddProfile (tBTRMgrPIHdl hBTRMgrPiHdl,BTMGR_Profile_t persistProfile);

//Remove a single bluetooth profile from json file
eBTRMgrRet BTRMgr_PI_RemoveProfile (tBTRMgrPIHdl hBTRMgrPiHdl,BTMGR_Profile_t persistProfile);

#endif /* __BTR_MGR_PERSIST_IFCE_H__ */
