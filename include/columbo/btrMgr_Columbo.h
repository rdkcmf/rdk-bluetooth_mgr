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

#ifndef __BTR_MGR_COLUMBO_H__
#define __BTR_MGR_COLUMBO_H__

#define BTRMGR_COL_STR_LEN_MAX        256


typedef enum _BTRMGR_ColumboChar_t {
    BTRMGR_SYSDIAG_COLUMBO_BEGIN = 200,
    BTRMGR_SYSDIAG_COLUMBO_START,
    BTRMGR_SYSDIAG_COLUMBO_STOP,
    BTRMGR_SYSDIAG_COLUMBO_STATUS,
    BTRMGR_SYSDIAG_COLUMBO_REPORT,
    BTRMGR_SYSDIAG_COLUMBO_UNKNOWN,
    BTRMGR_SYSDIAG_COLUMBO_END
} BTRMGR_ColumboChar_t;


/* Interfaces */
/**
 * @addtogroup  LE_DiagInfo
 * @{
 *
 */

eBTRMgrRet BTRMGR_Columbo_GetData(BTRMGR_ColumboChar_t aenColumboChar, char* aData);

eBTRMgrRet BTRMGR_Columbo_SetData(BTRMGR_ColumboChar_t aenColumboChar, char* aData);

/** @} */

#endif /* __BTR_MGR_COLUMBO_H__ */

