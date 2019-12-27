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
 * @file btrMgr_LeOnboarding.h
 *
 * @defgroup Sys_DiagInfo LeOnboardingInfoInterface
 * This file defines 
 * @ingroup  BTR_MGR
 *
 */

#ifndef __BTR_MGR_LEONBOARDING_H__
#define __BTR_MGR_LEONBOARDING_H__

#define BTRMGR_LE_STR_LEN_MAX        (256*3)
#define MAX_LEN_PUBLIC_KEY           BTRMGR_LE_STR_LEN_MAX
#define MAX_PAYLOAD_LEN              2048
#define SSID_MAX_LEN            32
#define PASS_PHRASE_LEN         64
#define MAX_FREQ_STR_LEN        7  //2.4GHz or 5GHz

typedef enum _BTRMGR_LeOnboardingChar_t {
    BTRMGR_LE_ONBRDG_BEGIN = 300,
    BTRMGR_LE_ONBRDG_SYSTEMID,
    BTRMGR_LE_ONBRDG_MODELNUMBER,
    BTRMGR_LE_ONBRDG_SERIALNUMBER,
    BTRMGR_LE_ONBRDG_FWREVISION,
    BTRMGR_LE_ONBRDG_HWREVISION,
    BTRMGR_LE_ONBRDG_SWREVISION,
    BTRMGR_LE_ONBRDG_MFGRNAME,
    BTRMGR_LE_ONBRDG_UUID_QR_CODE,
    BTRMGR_LE_ONBRDG_UUID_PROVISION_STATUS,
    BTRMGR_LE_ONBRDG_UUID_PUBLIC_KEY,
    BTRMGR_LE_ONBRDG_UUID_WIFI_CONFIG,
    BTRMGR_LE_ONBRDG_UUID_SSID_LIST,
    BTRMGR_LE_ONBRDG_UNKNOWN,
    BTRMGR_LE_ONBRDG_END
} BTRMGR_LeOnboardingChar_t;

typedef enum _BTRMGR_LeOnbrdg_WifiPrvsnStatus_t {
    BTRMGR_LE_ONBRDG_AWAITING_WIFI_CONFIG      = 0x0101,   /* RDK device is waiting for WiFi configuration from Mobile Application */
    BTRMGR_LE_ONBRDG_PROCESSING_WIFI_CONFIG    = 0x0102,   /* RDK device is processing received WiFi configuration over Bluetooth LE */
    BTRMGR_LE_ONBRDG_CONNECTING_TO_WIFI        = 0x0103,   /* RDK device is trying to connect to WiFi */
    BTRMGR_LE_ONBRDG_WIFI_CONNECT_SUCCESS      = 0x0104,   /* RDK device is successfully associated to WiFi */
    BTRMGR_LE_ONBRDG_ACQUIRING_IP_ADDRESS      = 0x0105,   /* RDK device is trying to acquire an IP post WiFi association */
    BTRMGR_LE_ONBRDG_IP_ADDRESS_ACQUIRED       = 0x0106,   /* RDK device successfully acquired IP */
    BTRMGR_LE_ONBRDG_DOWNLOADING_VIDEO_CONFIG  = 0x0107,   /* RDK device(camera) is downloading video configuration */
    BTRMGR_LE_ONBRDG_CONFIGURING_LIVE_VIDEO    = 0x0108,   /* RDK device(camera) is configuring live video */
    BTRMGR_LE_ONBRDG_FIRMWARE_UPGRADE          = 0x09FE,   /* RDK device has a critical firmware upgrade.After this, there wont be any more notifications sent via BLE interface. */
    BTRMGR_LE_ONBRDG_COMPLETE_SUCCESS          = 0x09FF,   /* Provisioning / Onboarding of RDK device is completed successfully.This would be the last provisioning status from RDK device in case of successful completion. */
}BTRMGR_LeOnbrdg_WifiPrvsnStatus;

typedef enum _BTRMGR_LeOnbrdg_state_t {
    BTRMGR_LE_ONBRDG_UNDEFINED = 0,
    BTRMGR_LE_ONBRDG_ADVERTISE,
    BTRMGR_LE_ONBRDG_BT_PAIRING,
    BTRMGR_LE_ONBRDG_INPROGRESS,
    BTRMGR_LE_ONBRDG_GET_LFAT,
    BTRMGR_LE_ONBRDG_GET_WIFI_CREDS,
    BTRMGR_LE_ONBRDG_CONNECT_WIFI,
    BTRMGR_LE_ONBRDG_COMPLETE,
    BTRMGR_LE_ONBRDG_EXIT
}BTRMGR_LeOnbrdg_state;


/* wifi credntials structure */
struct wifi_credentials {
    char ssid[SSID_MAX_LEN + 1];
    char passphrase[PASS_PHRASE_LEN + 1];
    char frequency[MAX_FREQ_STR_LEN + 1];
    int securitymode;
};

/* Interfaces */
/**
 * @addtogroup  LE_DiagInfo
 * @{
 *
 */

eBTRMgrRet BTRMGR_LeOnboarding_GetData(BTRMGR_LeOnboardingChar_t aenLeOnboardingChar, char* aData);
eBTRMgrRet BTRMGR_LeOnboarding_SetData(BTRMGR_LeOnboardingChar_t aenLeOnboardingChar, char* payload);

eBTRMgrRet BTRMGR_Wifi_ConnectToWifi(char* aSSID, char* aPassword, int aSecurityMode);

/** @} */

#endif /* __BTR_MGR_LEONBOARDING_H__ */

