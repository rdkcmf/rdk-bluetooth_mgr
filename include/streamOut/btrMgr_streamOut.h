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
 * @file btrMgr_streamOut.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces to external BT devices
 *
 * Copyright (c) 2016  Comcast
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
} stBTRMgrSOInASettings;

typedef struct _stBTRMgrSOOutASettings {
    eBTRMgrSOAType  eBtrMgrSoOutAType;
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
eBTRMgrSORet BTRMgr_SO_Start (tBTRMgrSoHdl hBTRMgrSoHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU);
eBTRMgrSORet BTRMgr_SO_Stop (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_Pause (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_Resume (tBTRMgrSoHdl hBTRMgrSoHdl);
eBTRMgrSORet BTRMgr_SO_SendBuffer (tBTRMgrSoHdl hBTRMgrSoHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrSORet BTRMgr_SO_SendEOS (tBTRMgrSoHdl hBTRMgrSoHdl);

#endif /* __BTR_MGR_STREAMOUT_H__ */
