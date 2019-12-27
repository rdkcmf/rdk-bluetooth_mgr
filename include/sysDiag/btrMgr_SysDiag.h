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
 * @file btrMgr_SysDiag.h
 *
 * @defgroup Sys_DiagInfo SysDiagInfoInterface
 * This file defines 
 * @ingroup  BTR_MGR
 *
 */

#ifndef __BTR_MGR_SYSDIAG_H__
#define __BTR_MGR_SYSDIAG_H__

#define BTRMGR_STR_LEN_MAX        256


typedef enum _BTRMGR_SysDiagChar_t {
    BTRMGR_SYS_DIAG_BEGIN = 100,
    BTRMGR_SYS_DIAG_DEVICEMAC,
    BTRMGR_SYS_DIAG_BTRADDRESS,
    BTRMGR_SYS_DIAG_SYSTEMID,
    BTRMGR_SYS_DIAG_MODELNUMBER,
    BTRMGR_SYS_DIAG_SERIALNUMBER,
    BTRMGR_SYS_DIAG_FWREVISION,
    BTRMGR_SYS_DIAG_HWREVISION,
    BTRMGR_SYS_DIAG_SWREVISION,
    BTRMGR_SYS_DIAG_MFGRNAME,
    BTRMGR_SYS_DIAG_DEVICESTATUS,
    BTRMGR_SYS_DIAG_FWDOWNLOADSTATUS,
    BTRMGR_SYS_DIAG_WEBPASTATUS,
    BTRMGR_SYS_DIAG_WIFIRADIO1STATUS,
    BTRMGR_SYS_DIAG_WIFIRADIO2STATUS,
    BTRMGR_SYS_DIAG_RFSTATUS,
    BTRMGR_SYS_DIAG_WIFI_CONNECT,
    BTRMGR_SYS_DIAG_UNKNOWN,
    BTRMGR_SYS_DIAG_END
} BTRMGR_SysDiagChar_t;


/* Interfaces */
/**
 * @addtogroup  LE_DiagInfo
 * @{
 *
 */

eBTRMgrRet BTRMGR_SysDiag_GetData(BTRMGR_SysDiagChar_t aenSysDiagChar, char* aData);

eBTRMgrRet BTRMGR_Wifi_ConnectToWifi(char* aSSID, char* aPassword, int aSecurityMode);

/** @} */

#endif /* __BTR_MGR_SYSDIAG_H__ */

