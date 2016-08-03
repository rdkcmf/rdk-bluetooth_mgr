/*
 * ============================================================================
 * RDK MANAGEMENT, LLC CONFIDENTIAL AND PROPRIETARY
 * ============================================================================
 * This file (and its contents) are the intellectual property of RDK Management, LLC.
 * It may not be used, copied, distributed or otherwise  disclosed in whole or in
 * part without the express written permission of RDK Management, LLC.
 * ============================================================================
 * Copyright (c) 2016 RDK Management, LLC. All rights reserved.
 * ============================================================================
 */
/**
 * @file btrMgr_streamIn.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces to external BT devices
 *
 * Copyright (c) 2016  Comcast
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
