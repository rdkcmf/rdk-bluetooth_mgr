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
 * @file btrMgr_streamOut.c
 *
 * @description This file implements bluetooth manager's Generic streaming interface to external BT devices
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */
#include <stdlib.h>

/* Ext lib Headers */
#include <glib.h>

/* Interface lib Headers */
#include "btrMgr_streamOut.h"

/* Local Headers */
#ifdef USE_GST1
#include "btrMgr_streamOutGst.h"
#endif

/* Local types */
typedef struct _stBTRMgrSOHdl {
    stBTRMgrSOStatus    lstBtrMgrSoStatus;
#ifdef USE_GST1
    tBTRMgrSoGstHdl     hBTRMgrSoGstHdl;
#endif
} stBTRMgrSOHdl;



eBTRMgrSORet
BTRMgr_SO_Init (
    tBTRMgrSoHdl*   phBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = NULL;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if ((pstBtrMgrSoHdl = (stBTRMgrSOHdl*)g_malloc0 (sizeof(stBTRMgrSOHdl))) == NULL) {
        g_print ("%s:%d:%s - Unable to allocate memory\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOInitFailure;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstInit(&(pstBtrMgrSoHdl->hBTRMgrSoGstHdl))) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOInitFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    if (leBtrMgrSoRet != eBTRMgrSOSuccess) {
        BTRMgr_SO_DeInit((tBTRMgrSoHdl)pstBtrMgrSoHdl);
        return leBtrMgrSoRet;
    }

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoState = eBTRMgrSOStateInitialized;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoSFreq = eBTRMgrSOSFreq48K;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoSFmt  = eBTRMgrSOSFmt16bit;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoAChan = eBTRMgrSOAChanJStereo;

    *phBTRMgrSoHdl = (tBTRMgrSoHdl)pstBtrMgrSoHdl;

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_DeInit (
    tBTRMgrSoHdl    hBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstDeInit(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOFailure;
    }
    pstBtrMgrSoHdl->hBTRMgrSoGstHdl = NULL;
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoState  = eBTRMgrSOStateDeInitialized;

    g_free((void*)pstBtrMgrSoHdl);
    pstBtrMgrSoHdl = NULL;

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_GetDefaultSettings (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
    (void)leBtrMgrSoGstRet;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_GetCurrentSettings (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
    (void)leBtrMgrSoGstRet;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_GetStatus (
    tBTRMgrSoHdl      hBTRMgrSoHdl,
    stBTRMgrSOStatus* apstBtrMgrSoStatus
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
    (void)leBtrMgrSoGstRet;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_Start (
    tBTRMgrSoHdl    hBTRMgrSoHdl,
    int             aiInBufMaxSize,
    int             aiBTDevFd,
    int             aiBTDevMTU
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstStart(pstBtrMgrSoHdl->hBTRMgrSoGstHdl, aiInBufMaxSize, aiBTDevFd, aiBTDevMTU)) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoState  = eBTRMgrSOStatePlaying;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_Stop (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstStop(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoState  = eBTRMgrSOStateStopped;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_Pause (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstPause(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoState  = eBTRMgrSOStatePaused;

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_Resume (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstResume(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoState  = eBTRMgrSOStatePlaying;

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_SendBuffer (
    tBTRMgrSoHdl    hBTRMgrSoHdl,
    char*           pcInBuf,
    int             aiInBufSize
) {
    eBTRMgrSORet    leBtrMgrSoRet = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

    //TODO: Implement ping-pong/triple/circular buffering if needed
#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstSendBuffer(pstBtrMgrSoHdl->hBTRMgrSoGstHdl, pcInBuf, aiInBufSize)) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    return leBtrMgrSoRet;
}


eBTRMgrSORet
BTRMgr_SO_SendEOS (
    tBTRMgrSoHdl    hBTRMgrSoHdl
) {
    eBTRMgrSORet    leBtrMgrSoRet = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstSendEOS(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrSOFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSoState  = eBTRMgrSOStateCompleted;

    return leBtrMgrSoRet;
}
