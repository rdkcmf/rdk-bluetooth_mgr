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

typedef enum _eBTRMgrSORet {
    eBTRMgrSOFailure,
    eBTRMgrSOInitFailure,
    eBTRMgrSONotInitialized,
    eBTRMgrSOFailInArg,
    eBTRMgrSOSuccess
} eBTRMgrSORet;

typedef enum _eBTRMgrSOState {
    eBTRMgrSOStateInitialized,
    eBTRMgrSOStateDeInitialized,
    eBTRMgrSOStatePaused,
    eBTRMgrSOStatePlaying,
    eBTRMgrSOStateCompleted,
    eBTRMgrSOStateStopped,
    eBTRMgrSOStateUnknown
} eBTRMgrSOState;

typedef enum _eBTRMgrSOAType {
    eBTRMgrSOATypePCM,
    eBTRMgrSOATypeMPEG,
    eBTRMgrSOATypeSBC,
    eBTRMgrSOATypeAAC,
    eBTRMgrSOATypeUnknown
} eBTRMgrSOAType;

typedef enum _eBTRMgrSOSFreq {
    eBTRMgrSOSFreq16K,
    eBTRMgrSOSFreq32K,
    eBTRMgrSOSFreq44_1K,
    eBTRMgrSOSFreq48K,
    eBTRMgrSOSFreqUnknown
} eBTRMgrSOSFreq;

typedef enum _eBTRMgrSOSFmt {
    eBTRMgrSOSFmt16bit,
    eBTRMgrSOSFmt24bit,
    eBTRMgrSOSFmt32bit,
    eBTRMgrSOSFmtUnknown
} eBTRMgrSOSFmt;

typedef enum _eBTRMgrSOAChan {
    eBTRMgrSOAChanMono,
    eBTRMgrSOAChanStereo,
    eBTRMgrSOAChanJStereo,
    eBTRMgrSOAChan5_1,
    eBTRMgrSOAChanUnknown
} eBTRMgrSOAChan;

typedef struct _stBTRMgrSOInASettings {
    eBTRMgrSOAType  eBtrMgrSoInAType;
    eBTRMgrSOSFreq  eBtrMgrSoInSFreq;
    eBTRMgrSOSFmt   eBtrMgrSoInSFmt;
    eBTRMgrSOAChan  eBtrMgrSoInAChan;
    int             iBtrMgrSoInBufMaxSize;
} stBTRMgrSOInASettings;

typedef struct _stBTRMgrSOOutASettings {
    eBTRMgrSOAType  eBtrMgrSoOutAType;
    int             iBtrMgrSoDevFd;
    int             iBtrMgrSoDevMtu;
} stBTRMgrSOOutASettings;

typedef struct _stBTRMgrSOStatus {
    eBTRMgrSOState  eBtrMgrSoState;
    eBTRMgrSOSFreq  eBtrMgrSoSFreq;
    eBTRMgrSOSFmt   eBtrMgrSoSFmt;
    eBTRMgrSOAChan  eBtrMgrSoAChan;
    unsigned int    ui32OverFlowCnt;
    unsigned int    ui32UnderFlowCnt;
} stBTRMgrSOStatus;


/* Interfaces */
eBTRMgrSORet BTRMgr_SO_Init (tBTRMgrSoHdl* phBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_DeInit (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_GetDefaultSettings (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_GetCurrentSettings (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_GetStatus (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrSOStatus* apstBtrMgrSoStatus);
eBTRMgrSORet BTRMgr_SO_Start (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrSOInASettings* apstBtrMgrSoInASettings, stBTRMgrSOOutASettings* apstBtrMgrSoOutASettings);
eBTRMgrSORet BTRMgr_SO_Stop (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_Pause (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_Resume (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_SendBuffer (tBTRMgrSoHdl hBTRMgrSoHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrSORet BTRMgr_SO_SendEOS (tBTRMgrSoHdl hBTRMgrSoHdl);

#endif /* __BTR_MGR_STREAMOUT_H__ */
