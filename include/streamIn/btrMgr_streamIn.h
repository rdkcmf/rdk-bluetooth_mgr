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
 * @file btrMgr_streamIn.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces to external BT devices
 *
 */

#ifndef __BTR_MGR_STREAMIN_H__
#define __BTR_MGR_STREAMIN_H__

typedef void* tBTRMgrSiHdl;

typedef enum _eBTRMgrSIRet {
    eBTRMgrSIFailure,
    eBTRMgrSIInitFailure,
    eBTRMgrSINotInitialized,
    eBTRMgrSIFailInArg,
    eBTRMgrSISuccess
} eBTRMgrSIRet;

typedef enum _eBTRMgrSIState {
    eBTRMgrSIStateInitialized,
    eBTRMgrSIStateDeInitialized,
    eBTRMgrSIStatePaused,
    eBTRMgrSIStatePlaying,
    eBTRMgrSIStateCompleted,
    eBTRMgrSIStateStopped,
    eBTRMgrSIStateUnknown
} eBTRMgrSIState;

typedef enum _eBTRMgrSIAType {
    eBTRMgrSIATypePCM,
    eBTRMgrSIATypeMPEG,
    eBTRMgrSIATypeSBC,
    eBTRMgrSIATypeAAC,
    eBTRMgrSIATypeUnknown
} eBTRMgrSIAType;

typedef enum _eBTRMgrSISFreq {
    eBTRMgrSISFreq16K,
    eBTRMgrSISFreq32K,
    eBTRMgrSISFreq44_1K,
    eBTRMgrSISFreq48K,
    eBTRMgrSISFreqUnknown
} eBTRMgrSISFreq;

typedef enum _eBTRMgrSISFmt {
    eBTRMgrSISFmt16bit,
    eBTRMgrSISFmt24bit,
    eBTRMgrSISFmt32bit,
    eBTRMgrSISFmtUnknown
} eBTRMgrSISFmt;

typedef enum _eBTRMgrSIAChan {
    eBTRMgrSIAChanMono,
    eBTRMgrSIAChanStereo,
    eBTRMgrSIAChanJStereo,
    eBTRMgrSIAChan5_1,
    eBTRMgrSIAChanUnknown
} eBTRMgrSIAChan;

typedef struct _stBTRMgrSIInASettings {
    eBTRMgrSIAType  eBtrMgrSiInAType;
    eBTRMgrSISFreq  eBtrMgrSiInSFreq;
    eBTRMgrSISFmt   eBtrMgrSiInSFmt;
    eBTRMgrSIAChan  eBtrMgrSiInAChan;
} stBTRMgrSIInASettings;

typedef struct _stBTRMgrSIOutASettings {
    eBTRMgrSIAType  eBtrMgrSiOutAType;
} stBTRMgrSIOutASettings;

typedef struct _stBTRMgrSIStatus {
    eBTRMgrSIState  eBtrMgrSiState;
    eBTRMgrSISFreq  eBtrMgrSiSFreq;
    eBTRMgrSISFmt   eBtrMgrSiSFmt;
    eBTRMgrSIAChan  eBtrMgrSiAChan;
    unsigned int    ui32OverFlowCnt;
    unsigned int    ui32UnderFlowCnt;
} stBTRMgrSIStatus;


/* Interfaces */
eBTRMgrSIRet BTRMgr_SI_Init (tBTRMgrSiHdl* phBTRMgrSiHdl);
eBTRMgrSIRet BTRMgr_SI_DeInit (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrSIRet BTRMgr_SI_GetDefaultSettings (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrSIRet BTRMgr_SI_GetCurrentSettings (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrSIRet BTRMgr_SI_GetStatus (tBTRMgrSiHdl hBTRMgrSiHdl, stBTRMgrSIStatus* apstBtrMgrSiStatus);
eBTRMgrSIRet BTRMgr_SI_Start (tBTRMgrSiHdl hBTRMgrSiHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU);
eBTRMgrSIRet BTRMgr_SI_Stop (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrSIRet BTRMgr_SI_Pause (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrSIRet BTRMgr_SI_Resume (tBTRMgrSiHdl hBTRMgrSiHdl);
eBTRMgrSIRet BTRMgr_SI_SendBuffer (tBTRMgrSiHdl hBTRMgrSiHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrSIRet BTRMgr_SI_SendEOS (tBTRMgrSiHdl hBTRMgrSiHdl);

#endif /* __BTR_MGR_STREAMOUT_H__ */
