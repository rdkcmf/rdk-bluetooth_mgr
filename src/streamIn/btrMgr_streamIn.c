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
 * @file btrMgr_streamIn.c
 *
 * @description This file implements bluetooth manager's Generic streaming interface to external BT devices
 *
 * Copyright (c) 2016  Comcast
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */
#include <stdlib.h>

/* Ext lib Headers */
#include <glib.h>

/* Interface lib Headers */
#include "btrMgr_streamIn.h"

/* Local Headers */
#ifdef USE_GST1
#include "btrMgr_streamInGst.h"
#endif

/* Local types */
typedef struct _stBTRMgrSIHdl {
    stBTRMgrSIStatus    lstBtrMgrSiStatus;
#ifdef USE_GST1
    tBTRMgrSiGstHdl     hBTRMgrSiGstHdl;
#endif
} stBTRMgrSIHdl;



eBTRMgrSIRet
BTRMgr_SI_Init (
    tBTRMgrSiHdl*   phBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = NULL;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if ((pstBtrMgrSiHdl = (stBTRMgrSIHdl*)g_malloc0 (sizeof(stBTRMgrSIHdl))) == NULL) {
        g_print ("%s:%d:%s - Unable to allocate memory\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSIInitFailure;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstInit(&(pstBtrMgrSiHdl->hBTRMgrSiGstHdl))) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIInitFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    if (leBtrMgrSiRet != eBTRMgrSISuccess) {
        BTRMgr_SI_DeInit((tBTRMgrSiHdl)pstBtrMgrSiHdl);
        return leBtrMgrSiRet;
    }

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiState = eBTRMgrSIStateInitialized;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiSFreq = eBTRMgrSISFreq48K;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiSFmt  = eBTRMgrSISFmt16bit;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiAChan = eBTRMgrSIAChanJStereo;

    *phBTRMgrSiHdl = (tBTRMgrSiHdl)pstBtrMgrSiHdl;

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_DeInit (
    tBTRMgrSiHdl    hBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstDeInit(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIFailure;
    }
    pstBtrMgrSiHdl->hBTRMgrSiGstHdl = NULL;
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiState  = eBTRMgrSIStateDeInitialized;

    g_free((void*)pstBtrMgrSiHdl);
    pstBtrMgrSiHdl = NULL;

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_GetDefaultSettings (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
    (void)leBtrMgrSiGstRet;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_GetCurrentSettings (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
    (void)leBtrMgrSiGstRet;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_GetStatus (
    tBTRMgrSiHdl      hBTRMgrSiHdl,
    stBTRMgrSIStatus* apstBtrMgrSiStatus
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
    (void)leBtrMgrSiGstRet;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_Start (
    tBTRMgrSiHdl    hBTRMgrSiHdl,
    int             aiInBufMaxSize,
    int             aiBTDevFd,
    int             aiBTDevMTU
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstStart(pstBtrMgrSiHdl->hBTRMgrSiGstHdl, aiInBufMaxSize, aiBTDevFd, aiBTDevMTU)) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiState  = eBTRMgrSIStatePlaying;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_Stop (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstStop(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiState  = eBTRMgrSIStateStopped;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_Pause (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstPause(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiState  = eBTRMgrSIStatePaused;

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_Resume (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet  = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstResume(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiState  = eBTRMgrSIStatePlaying;

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_SendBuffer (
    tBTRMgrSiHdl    hBTRMgrSiHdl,
    char*           pcInBuf,
    int             aiInBufSize
) {
    eBTRMgrSIRet    leBtrMgrSiRet = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

    //TODO: Implement ping-pong/triple/circular buffering if needed
#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstSendBuffer(pstBtrMgrSiHdl->hBTRMgrSiGstHdl, pcInBuf, aiInBufSize)) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    return leBtrMgrSiRet;
}


eBTRMgrSIRet
BTRMgr_SI_SendEOS (
    tBTRMgrSiHdl    hBTRMgrSiHdl
) {
    eBTRMgrSIRet    leBtrMgrSiRet = eBTRMgrSISuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrSINotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstSendEOS(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrSIFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSiState  = eBTRMgrSIStateCompleted;

    return leBtrMgrSiRet;
}
