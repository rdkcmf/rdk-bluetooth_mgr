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
    tBTRMgrSoHdl            hBTRMgrSoHdl,
    stBTRMgrSOInASettings*  apstBtrMgrSoInASettings,
    stBTRMgrSOOutASettings* apstBtrMgrSoOutASettings
) {
    eBTRMgrSORet    leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

    eBTRMgrSOSFmt   leBtrMgrSoInSFmt  = eBTRMgrSOSFmtUnknown;
    eBTRMgrSOAChan  leBtrMgrSoInAChan = eBTRMgrSOAChanUnknown;
    eBTRMgrSOSFreq  leBtrMgrSoInSFreq = eBTRMgrSOSFreqUnknown;

    eBTRMgrSOAChan  leBtrMgrSoOutAChan = eBTRMgrSOAChanUnknown;
    eBTRMgrSOSFreq  leBtrMgrSoOutSFreq = eBTRMgrSOSFreqUnknown;

    const char*  lpcBtrMgrInSoSFmt = NULL;
    unsigned int lui32BtrMgrInSoAChan;
    unsigned int lui32BtrMgrInSoSFreq;

    const char*  lpcBtrMgrOutSoAChanMode = NULL;
    unsigned int lui32BtrMgrOutSoAChan;
    unsigned int lui32BtrMgrOutSoSFreq;

    unsigned char   lui8SbcAllocMethod  = 0;
    unsigned char   lui8SbcSubbands     = 0;   
    unsigned char   lui8SbcBlockLength  = 0;
    unsigned char   lui8SbcMinBitpool   = 0; 
    unsigned char   lui8SbcMaxBitpool   = 0; 


#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrSONotInitialized;
    }

    if ((apstBtrMgrSoInASettings == NULL) || (apstBtrMgrSoOutASettings == NULL) ||
        (apstBtrMgrSoInASettings->pstBtrMgrSoInCodecInfo == NULL) || (apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo == NULL)) {
        return eBTRMgrSOFailInArg;
    }

    if (apstBtrMgrSoInASettings->eBtrMgrSoInAType == eBTRMgrSOATypePCM) {
        leBtrMgrSoInSFmt  = ((stBTRMgrSOPCMInfo*)(apstBtrMgrSoInASettings->pstBtrMgrSoInCodecInfo))->eBtrMgrSoSFmt;
        leBtrMgrSoInAChan = ((stBTRMgrSOPCMInfo*)(apstBtrMgrSoInASettings->pstBtrMgrSoInCodecInfo))->eBtrMgrSoAChan;
        leBtrMgrSoInSFreq = ((stBTRMgrSOPCMInfo*)(apstBtrMgrSoInASettings->pstBtrMgrSoInCodecInfo))->eBtrMgrSoSFreq;
    }


    if (apstBtrMgrSoOutASettings->eBtrMgrSoOutAType == eBTRMgrSOATypeSBC) {
        leBtrMgrSoOutAChan = ((stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo))->eBtrMgrSoSbcAChan;
        leBtrMgrSoOutSFreq = ((stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo))->eBtrMgrSoSbcSFreq;
        lui8SbcAllocMethod = ((stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo))->ui8SbcAllocMethod;
        lui8SbcSubbands    = ((stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo))->ui8SbcSubbands;
        lui8SbcBlockLength = ((stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo))->ui8SbcBlockLength;
        lui8SbcMinBitpool  = ((stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo))->ui8SbcMinBitpool;
        lui8SbcMaxBitpool  = ((stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo))->ui8SbcMaxBitpool;
    }


#ifdef USE_GST1
    switch (leBtrMgrSoInSFmt) {
    case eBTRMgrSOSFmt8bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_8BIT;
        break;
    case eBTRMgrSOSFmt16bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_16BIT;
        break;
    case eBTRMgrSOSFmt24bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_24BIT;
        break;
    case eBTRMgrSOSFmt32bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_32BIT;
        break;
    case eBTRMgrSOSFmtUnknown:
    default:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_16BIT;
        break;
    }

    switch (leBtrMgrSoInAChan) {
    case eBTRMgrSOAChanMono:
        lui32BtrMgrInSoAChan = 1;
        break;
    case eBTRMgrSOAChanDualChannel:
        lui32BtrMgrInSoAChan = 2;
        break;
    case eBTRMgrSOAChanStereo:
        lui32BtrMgrInSoAChan = 2;
        break;
    case eBTRMgrSOAChanJStereo:
        lui32BtrMgrInSoAChan = 2;
        break;
    case eBTRMgrSOAChan5_1:
        lui32BtrMgrInSoAChan = 6;
        break;
    case eBTRMgrSOAChan7_1:
        lui32BtrMgrInSoAChan = 8;
        break;
    case eBTRMgrSOAChanUnknown:
    default:
        lui32BtrMgrInSoAChan = 2;
        break;
    }

    switch (leBtrMgrSoInSFreq) {
    case eBTRMgrSOSFreq8K:
        lui32BtrMgrInSoSFreq = 8000;
    case eBTRMgrSOSFreq16K:
        lui32BtrMgrInSoSFreq = 16000;
        break;
    case eBTRMgrSOSFreq32K:
        lui32BtrMgrInSoSFreq = 32000;
        break;
    case eBTRMgrSOSFreq44_1K:
        lui32BtrMgrInSoSFreq = 44100;
        break;
    case eBTRMgrSOSFreq48K:
        lui32BtrMgrInSoSFreq = 48000;
        break;
    case eBTRMgrSOSFreqUnknown:
    default:
        lui32BtrMgrInSoSFreq = 48000;
        break;
    }

    switch (leBtrMgrSoOutAChan) {
    case eBTRMgrSOAChanMono:
        lui32BtrMgrOutSoAChan   = 1;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_MONO;
        break;
    case eBTRMgrSOAChanDualChannel:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_DUAL;
        break;
    case eBTRMgrSOAChanStereo:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_STEREO;
        break;
    case eBTRMgrSOAChanJStereo:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_JSTEREO;
        break;
    case eBTRMgrSOAChan5_1:
        lui32BtrMgrOutSoAChan   = 6;
        break;
    case eBTRMgrSOAChan7_1:
        lui32BtrMgrOutSoAChan   = 8;
        break;
    case eBTRMgrSOAChanUnknown:
    default:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_STEREO;
        break;
    }

    switch (leBtrMgrSoOutSFreq) {
    case eBTRMgrSOSFreq8K:
        lui32BtrMgrOutSoSFreq = 8000;
    case eBTRMgrSOSFreq16K:
        lui32BtrMgrOutSoSFreq = 16000;
        break;
    case eBTRMgrSOSFreq32K:
        lui32BtrMgrOutSoSFreq = 32000;
        break;
    case eBTRMgrSOSFreq44_1K:
        lui32BtrMgrOutSoSFreq = 44100;
        break;
    case eBTRMgrSOSFreq48K:
        lui32BtrMgrOutSoSFreq = 48000;
        break;
    case eBTRMgrSOSFreqUnknown:
    default:
        lui32BtrMgrOutSoSFreq = 48000;
        break;
    }

    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstStart( pstBtrMgrSoHdl->hBTRMgrSoGstHdl,
                                                apstBtrMgrSoInASettings->iBtrMgrSoInBufMaxSize,
                                                lpcBtrMgrInSoSFmt,
                                                lui32BtrMgrInSoSFreq,
                                                lui32BtrMgrInSoAChan,
                                                lui32BtrMgrOutSoSFreq,
                                                lui32BtrMgrOutSoAChan,
                                                lpcBtrMgrOutSoAChanMode,
                                                lui8SbcAllocMethod,
                                                lui8SbcSubbands,
                                                lui8SbcBlockLength,
                                                lui8SbcMinBitpool,
                                                lui8SbcMaxBitpool,
                                                apstBtrMgrSoOutASettings->iBtrMgrSoDevFd,
                                                apstBtrMgrSoOutASettings->iBtrMgrSoDevMtu)) != eBTRMgrSOGstSuccess) {
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
