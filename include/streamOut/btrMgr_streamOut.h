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
 * @file btrMgr_streamOut.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces to external BT devices
 *
 */

#ifndef __BTR_MGR_STREAMOUT_H__
#define __BTR_MGR_STREAMOUT_H__

typedef void* tBTRMgrSoHdl;
#include "btrMgr_Types.h"

/* Interfaces */
eBTRMgrRet BTRMgr_SO_Init (tBTRMgrSoHdl* phBTRMgrSoHdl);
eBTRMgrRet BTRMgr_SO_DeInit (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrRet BTRMgr_SO_GetDefaultSettings (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrRet BTRMgr_SO_GetCurrentSettings (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrRet BTRMgr_SO_GetStatus (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrMediaStatus* apstBtrMgrSoStatus);
eBTRMgrRet BTRMgr_SO_GetEstimatedInABufSize (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrInASettings* apstBtrMgrSoInASettings, stBTRMgrOutASettings* apstBtrMgrSoOutASettings);
eBTRMgrRet BTRMgr_SO_Start (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrInASettings* apstBtrMgrSoInASettings, stBTRMgrOutASettings* apstBtrMgrSoOutASettings);
eBTRMgrRet BTRMgr_SO_Stop (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrRet BTRMgr_SO_Pause (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrRet BTRMgr_SO_Resume (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrRet BTRMgr_SO_SendBuffer (tBTRMgrSoHdl hBTRMgrSoHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrRet BTRMgr_SO_SendEOS (tBTRMgrSoHdl hBTRMgrSoHdl);

#endif /* __BTR_MGR_STREAMOUT_H__ */
