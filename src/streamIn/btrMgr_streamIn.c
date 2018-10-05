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
 * @file btrMgr_streamIn.c
 *
 * @description This file implements bluetooth manager's Generic streaming interface to external BT devices
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */
#include <stdlib.h>
#include <string.h>

/* Ext lib Headers */
#include <glib.h>

/* Interface lib Headers */
#include "btrMgr_logger.h"         //for rdklogger

/* Local Headers */
#include "btrMgr_Types.h"
#include "btrMgr_mediaTypes.h"
#include "btrMgr_streamIn.h"
#ifdef USE_GST1
#include "btrMgr_streamInGst.h"
#endif

/* Local types */
typedef struct _stBTRMgrSIHdl {
    stBTRMgrMediaStatus     lstBtrMgrSiStatus;
    fPtr_BTRMgr_SI_StatusCb fpcBSiStatus;
    void*                   pvcBUserData;
#ifdef USE_GST1
    tBTRMgrSiGstHdl         hBTRMgrSiGstHdl;
#endif
} stBTRMgrSIHdl;


/* Static Function Prototypes */

/* Local Op Threads Prototypes */

/* Incoming Callbacks Prototypes */
#ifdef USE_GST1
static eBTRMgrSIGstRet btrMgr_SI_GstStatusCb (eBTRMgrSIGstStatus aeBtrMgrSiGstStatus, void* apvUserData);
#endif


/* Static Function Definition */

/* Local Op Threads */

/*  Interfaces  */
eBTRMgrRet
BTRMgr_SI_Init (
    tBTRMgrSiHdl*           phBTRMgrSiHdl,
    fPtr_BTRMgr_SI_StatusCb afpcBSiStatus,
    void*                   apvUserData
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = NULL;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if ((pstBtrMgrSiHdl = (stBTRMgrSIHdl*)g_malloc0 (sizeof(stBTRMgrSIHdl))) == NULL) {
        BTRMGRLOG_ERROR ("Unable to allocate memory\n");
        return eBTRMgrInitFailure;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstInit(&(pstBtrMgrSiHdl->hBTRMgrSiGstHdl), btrMgr_SI_GstStatusCb, pstBtrMgrSiHdl)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrInitFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    if (leBtrMgrSiRet != eBTRMgrSuccess) {
        BTRMgr_SI_DeInit((tBTRMgrSiHdl)pstBtrMgrSiHdl);
        return leBtrMgrSiRet;
    }

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState = eBTRMgrStateInitialized;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSFreq = eBTRMgrSFreq44_1K;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrSFmt  = eBTRMgrSFmt16bit;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrAChan = eBTRMgrAChanStereo;
    pstBtrMgrSiHdl->fpcBSiStatus                   = afpcBSiStatus;
    pstBtrMgrSiHdl->pvcBUserData                   = apvUserData;


    *phBTRMgrSiHdl = (tBTRMgrSiHdl)pstBtrMgrSiHdl;

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_DeInit (
    tBTRMgrSiHdl    hBTRMgrSiHdl
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstDeInit(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrFailure;
    }
    pstBtrMgrSiHdl->hBTRMgrSiGstHdl = NULL;
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState  = eBTRMgrStateDeInitialized;

    g_free((void*)pstBtrMgrSiHdl);
    pstBtrMgrSiHdl = NULL;

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_GetDefaultSettings (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
    (void)leBtrMgrSiGstRet;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_GetCurrentSettings (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
    (void)leBtrMgrSiGstRet;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_GetStatus (
    tBTRMgrSiHdl            hBTRMgrSiHdl,
    stBTRMgrMediaStatus*    apstBtrMgrSiStatus
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
    (void)leBtrMgrSiGstRet;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_Start (
    tBTRMgrSiHdl    hBTRMgrSiHdl,
    int             aiInBufMaxSize,
    int             aiBTDevFd,
    int             aiBTDevMTU,
    unsigned int    aiBTDevSFreq
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstStart(pstBtrMgrSiHdl->hBTRMgrSiGstHdl, aiInBufMaxSize, aiBTDevFd, aiBTDevMTU, aiBTDevSFreq)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState  = eBTRMgrStatePlaying;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_Stop (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstStop(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState  = eBTRMgrStateStopped;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_Pause (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstPause(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState  = eBTRMgrStatePaused;

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_Resume (
    tBTRMgrSiHdl hBTRMgrSiHdl
) {
    eBTRMgrRet      leBtrMgrSiRet  = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstResume(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState  = eBTRMgrStatePlaying;

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_SendBuffer (
    tBTRMgrSiHdl    hBTRMgrSiHdl,
    char*           pcInBuf,
    int             aiInBufSize
) {
    eBTRMgrRet      leBtrMgrSiRet = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    //TODO: Implement ping-pong/triple/circular buffering if needed
#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstSendBuffer(pstBtrMgrSiHdl->hBTRMgrSiGstHdl, pcInBuf, aiInBufSize)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    return leBtrMgrSiRet;
}


eBTRMgrRet
BTRMgr_SI_SendEOS (
    tBTRMgrSiHdl    hBTRMgrSiHdl
) {
    eBTRMgrRet      leBtrMgrSiRet = eBTRMgrSuccess;
    stBTRMgrSIHdl*  pstBtrMgrSiHdl = (stBTRMgrSIHdl*)hBTRMgrSiHdl;

#ifdef USE_GST1
    eBTRMgrSIGstRet leBtrMgrSiGstRet = eBTRMgrSIGstSuccess;
#endif

    if (pstBtrMgrSiHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSiGstRet = BTRMgr_SI_GstSendEOS(pstBtrMgrSiHdl->hBTRMgrSiGstHdl)) != eBTRMgrSIGstSuccess) {
        BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrSiGstRet);
        leBtrMgrSiRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState  = eBTRMgrStateCompleted;

    return leBtrMgrSiRet;
}


// Outgoing callbacks Registration Interfaces

/* Incoming Callbacks Definitions */
#ifdef USE_GST1
static eBTRMgrSIGstRet
btrMgr_SI_GstStatusCb (
    eBTRMgrSIGstStatus  aeBtrMgrSiGstStatus,
    void*               apvUserData
) {
    stBTRMgrSIHdl*  pstBtrMgrSiHdl  = (stBTRMgrSIHdl*)apvUserData;
    eBTRMgrRet      leBtrMgrSiRet   = eBTRMgrSuccess;
    gboolean        bTriggerCb      = FALSE;

    if (pstBtrMgrSiHdl) {

        switch (aeBtrMgrSiGstStatus) {
        case eBTRMgrSIGstStInitialized:
        case eBTRMgrSIGstStDeInitialized:
        case eBTRMgrSIGstStPaused:
        case eBTRMgrSIGstStPlaying:
            break;
        case eBTRMgrSIGstStUnderflow:
            pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32UnderFlowCnt++;
            break;
        case eBTRMgrSIGstStOverflow:
            pstBtrMgrSiHdl->lstBtrMgrSiStatus.ui32OverFlowCnt++;
            break;
        case eBTRMgrSIGstStCompleted:
        case eBTRMgrSIGstStStopped:
            break;
        case eBTRMgrSIGstStWarning:
            pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState = eBTRMgrStateWarning;
            bTriggerCb = TRUE;
            break;
        case eBTRMgrSIGstStError:
            pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState = eBTRMgrStateError;
            bTriggerCb = TRUE;
            break;
        case eBTRMgrSIGstStUnknown:
        default:
            pstBtrMgrSiHdl->lstBtrMgrSiStatus.eBtrMgrState = eBTRMgrStateUnknown;
            break;
        }

        //TODO: Move to a local static function, which, if possible, can trigger the callback in the context
        //      of a task thread.
        if ((pstBtrMgrSiHdl->fpcBSiStatus) && (bTriggerCb == TRUE)) {
            stBTRMgrMediaStatus lstBtrMgrSiMediaStatus;

            memcpy (&lstBtrMgrSiMediaStatus, &(pstBtrMgrSiHdl->lstBtrMgrSiStatus), sizeof(stBTRMgrMediaStatus));

            leBtrMgrSiRet = pstBtrMgrSiHdl->fpcBSiStatus(&lstBtrMgrSiMediaStatus, pstBtrMgrSiHdl->pvcBUserData);
            BTRMGRLOG_TRACE("Return Status = %d\n", leBtrMgrSiRet);
        }
    }

    return eBTRMgrSIGstSuccess;
}
#endif
