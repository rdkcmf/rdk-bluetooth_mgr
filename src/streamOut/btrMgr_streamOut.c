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
#include "btmgr_priv.h"         //for rdklogger

/* Local Headers */
#include "btrMgr_mediaTypes.h"
#include "btrMgr_streamOut.h"
#ifdef USE_GST1
#include "btrMgr_streamOutGst.h"
#endif

/* Local types */
typedef struct _stBTRMgrSOHdl {
    stBTRMgrMediaStatus lstBtrMgrSoStatus;
#ifdef USE_GST1
    tBTRMgrSoGstHdl     hBTRMgrSoGstHdl;
#endif
} stBTRMgrSOHdl;



eBTRMgrRet
BTRMgr_SO_Init (
    tBTRMgrSoHdl*   phBTRMgrSoHdl
) {
    eBTRMgrRet      leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = NULL;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if ((pstBtrMgrSoHdl = (stBTRMgrSOHdl*)g_malloc0 (sizeof(stBTRMgrSOHdl))) == NULL) {
        BTMGRLOG_ERROR ("%s:%d:%s - Unable to allocate memory\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrInitFailure;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstInit(&(pstBtrMgrSoHdl->hBTRMgrSoGstHdl))) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrInitFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    if (leBtrMgrSoRet != eBTRMgrSuccess) {
        BTRMgr_SO_DeInit((tBTRMgrSoHdl)pstBtrMgrSoHdl);
        return leBtrMgrSoRet;
    }

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrState = eBTRMgrStateInitialized;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSFreq = eBTRMgrSFreq48K;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrSFmt  = eBTRMgrSFmt16bit;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrAChan = eBTRMgrAChanJStereo;

    *phBTRMgrSoHdl = (tBTRMgrSoHdl)pstBtrMgrSoHdl;

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_DeInit (
    tBTRMgrSoHdl    hBTRMgrSoHdl
) {
    eBTRMgrRet      leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstDeInit(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrFailure;
    }
    pstBtrMgrSoHdl->hBTRMgrSoGstHdl = NULL;
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrState  = eBTRMgrStateDeInitialized;

    g_free((void*)pstBtrMgrSoHdl);
    pstBtrMgrSoHdl = NULL;

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_GetDefaultSettings (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrRet      leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
    (void)leBtrMgrSoGstRet;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_GetCurrentSettings (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrRet      leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
    (void)leBtrMgrSoGstRet;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_GetStatus (
    tBTRMgrSoHdl            hBTRMgrSoHdl,
    stBTRMgrMediaStatus*    apstBtrMgrSoStatus
) {
    eBTRMgrRet      leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
    (void)leBtrMgrSoGstRet;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_GetEstimatedInABufSize (
    tBTRMgrSoHdl            hBTRMgrSoHdl,
    stBTRMgrInASettings*    apstBtrMgrSoInASettings,
    stBTRMgrOutASettings*   apstBtrMgrSoOutASettings
) {
    eBTRMgrRet          leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*      pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

    stBTRMgrPCMInfo*    pstBtrMgrSoInPcmInfo = NULL;
    stBTRMgrSBCInfo*    pstBtrMgrSoOutSbcInfo = NULL;

    unsigned int        lui32InBitsPerSample = 0;
    unsigned int        lui32InNumAChan = 0;
    unsigned int        lui32InSamplingFreq = 0;

    unsigned short      lui16OutFrameLen = 0;
    unsigned short      lui16OutBitrateKbps = 0;
    unsigned short      lui16OutMtu = 0;

    unsigned int        lui32InByteRate = 0;
    unsigned int        lui32OutByteRate = 0;
    float               lfOutMtuTimemSec = 0.0;

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    if ((apstBtrMgrSoInASettings == NULL) || (apstBtrMgrSoOutASettings == NULL) ||
        (apstBtrMgrSoInASettings->pstBtrMgrInCodecInfo == NULL) || (apstBtrMgrSoOutASettings->pstBtrMgrOutCodecInfo == NULL)) {
        return eBTRMgrFailInArg;
    }

    if (apstBtrMgrSoInASettings->eBtrMgrInAType == eBTRMgrATypePCM) {
        pstBtrMgrSoInPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrSoInASettings->pstBtrMgrInCodecInfo);

        switch (pstBtrMgrSoInPcmInfo->eBtrMgrSFreq) {
        case eBTRMgrSFreq8K:
            lui32InSamplingFreq = 8000;
        case eBTRMgrSFreq16K:
            lui32InSamplingFreq = 16000;
            break;
        case eBTRMgrSFreq32K:
            lui32InSamplingFreq = 32000;
            break;
        case eBTRMgrSFreq44_1K:
            lui32InSamplingFreq = 44100;
            break;
        case eBTRMgrSFreq48K:
            lui32InSamplingFreq = 48000;
            break;
        case eBTRMgrSFreqUnknown:
        default:
            lui32InSamplingFreq = 48000;
            break;
        }

        switch (pstBtrMgrSoInPcmInfo->eBtrMgrSFmt) {
        case eBTRMgrSFmt8bit:
            lui32InBitsPerSample = 8;
            break;
        case eBTRMgrSFmt16bit:
            lui32InBitsPerSample = 16;
            break;
        case eBTRMgrSFmt24bit:
            lui32InBitsPerSample = 24;
            break;
        case eBTRMgrSFmt32bit:
            lui32InBitsPerSample = 32;
            break;
        case eBTRMgrSFmtUnknown:
        default:
            lui32InBitsPerSample = 16;
            break;
        }

        switch (pstBtrMgrSoInPcmInfo->eBtrMgrAChan) {
        case eBTRMgrAChanMono:
            lui32InNumAChan = 1;
            break;
        case eBTRMgrAChanDualChannel:
            lui32InNumAChan = 2;
            break;
        case eBTRMgrAChanStereo:
            lui32InNumAChan = 2;
            break;
        case eBTRMgrAChanJStereo:
            lui32InNumAChan = 2;
            break;
        case eBTRMgrAChan5_1:
            lui32InNumAChan = 6;
            break;
        case eBTRMgrAChan7_1:
            lui32InNumAChan = 8;
            break;
        case eBTRMgrAChanUnknown:
        default:
            lui32InNumAChan = 2;
            break;
        }

    }

    if (apstBtrMgrSoOutASettings->eBtrMgrOutAType == eBTRMgrATypeSBC) {
        pstBtrMgrSoOutSbcInfo = (stBTRMgrSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrOutCodecInfo);
        lui16OutFrameLen    = pstBtrMgrSoOutSbcInfo->ui16SbcFrameLen;
        lui16OutBitrateKbps = pstBtrMgrSoOutSbcInfo->ui16SbcBitrate;
        lui16OutMtu         = apstBtrMgrSoOutASettings->i32BtrMgrDevMtu;
    }


    lui16OutMtu = (lui16OutMtu/lui16OutFrameLen) * lui16OutFrameLen;
    lui32OutByteRate = (lui16OutBitrateKbps * 1024) / 8;
    lfOutMtuTimemSec = (lui16OutMtu * 1000.0) / lui32OutByteRate;
    lui32InByteRate  = (lui32InBitsPerSample/8) * lui32InNumAChan * lui32InSamplingFreq;
    apstBtrMgrSoInASettings->i32BtrMgrInBufMaxSize = (lui32InByteRate * lfOutMtuTimemSec)/1000;

    // Align to multiple of 256
    apstBtrMgrSoInASettings->i32BtrMgrInBufMaxSize = (apstBtrMgrSoInASettings->i32BtrMgrInBufMaxSize >> 8) + 1;
    apstBtrMgrSoInASettings->i32BtrMgrInBufMaxSize = apstBtrMgrSoInASettings->i32BtrMgrInBufMaxSize << 8;

    BTMGRLOG_INFO("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);
    BTMGRLOG_INFO("Effective MTU = %d\n", lui16OutMtu);
    BTMGRLOG_INFO("OutByteRate = %d\n", lui32OutByteRate);
    BTMGRLOG_INFO("OutMtuTimemSec = %f\n", lfOutMtuTimemSec);
    BTMGRLOG_INFO("InByteRate = %d\n", lui32InByteRate);
    BTMGRLOG_INFO("InBufMaxSize = %d\n", apstBtrMgrSoInASettings->i32BtrMgrInBufMaxSize);

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_Start (
    tBTRMgrSoHdl            hBTRMgrSoHdl,
    stBTRMgrInASettings*  apstBtrMgrSoInASettings,
    stBTRMgrOutASettings* apstBtrMgrSoOutASettings
) {
    eBTRMgrRet    leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

    eBTRMgrSFmt   leBtrMgrSoInSFmt  = eBTRMgrSFmtUnknown;
    eBTRMgrAChan  leBtrMgrSoInAChan = eBTRMgrAChanUnknown;
    eBTRMgrSFreq  leBtrMgrSoInSFreq = eBTRMgrSFreqUnknown;

    eBTRMgrAChan  leBtrMgrSoOutAChan = eBTRMgrAChanUnknown;
    eBTRMgrSFreq  leBtrMgrSoOutSFreq = eBTRMgrSFreqUnknown;

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
        return eBTRMgrNotInitialized;
    }

    if ((apstBtrMgrSoInASettings == NULL) || (apstBtrMgrSoOutASettings == NULL) ||
        (apstBtrMgrSoInASettings->pstBtrMgrInCodecInfo == NULL) || (apstBtrMgrSoOutASettings->pstBtrMgrOutCodecInfo == NULL)) {
        return eBTRMgrFailInArg;
    }

    if (apstBtrMgrSoInASettings->eBtrMgrInAType == eBTRMgrATypePCM) {
        stBTRMgrPCMInfo* pstBtrMgrSoInPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrSoInASettings->pstBtrMgrInCodecInfo);
        leBtrMgrSoInSFmt  = pstBtrMgrSoInPcmInfo->eBtrMgrSFmt;
        leBtrMgrSoInAChan = pstBtrMgrSoInPcmInfo->eBtrMgrAChan;
        leBtrMgrSoInSFreq = pstBtrMgrSoInPcmInfo->eBtrMgrSFreq;
    }

    if (apstBtrMgrSoOutASettings->eBtrMgrOutAType == eBTRMgrATypeSBC) {
        stBTRMgrSBCInfo* pstBtrMgrSoOutSbcInfo = (stBTRMgrSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrOutCodecInfo);

        leBtrMgrSoOutAChan = pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan;
        leBtrMgrSoOutSFreq = pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq;
        lui8SbcAllocMethod = pstBtrMgrSoOutSbcInfo->ui8SbcAllocMethod;
        lui8SbcSubbands    = pstBtrMgrSoOutSbcInfo->ui8SbcSubbands;
        lui8SbcBlockLength = pstBtrMgrSoOutSbcInfo->ui8SbcBlockLength;
        lui8SbcMinBitpool  = pstBtrMgrSoOutSbcInfo->ui8SbcMinBitpool;
        lui8SbcMaxBitpool  = pstBtrMgrSoOutSbcInfo->ui8SbcMaxBitpool;
    }


    switch (leBtrMgrSoInSFreq) {
    case eBTRMgrSFreq8K:
        lui32BtrMgrInSoSFreq = 8000;
    case eBTRMgrSFreq16K:
        lui32BtrMgrInSoSFreq = 16000;
        break;
    case eBTRMgrSFreq32K:
        lui32BtrMgrInSoSFreq = 32000;
        break;
    case eBTRMgrSFreq44_1K:
        lui32BtrMgrInSoSFreq = 44100;
        break;
    case eBTRMgrSFreq48K:
        lui32BtrMgrInSoSFreq = 48000;
        break;
    case eBTRMgrSFreqUnknown:
    default:
        lui32BtrMgrInSoSFreq = 48000;
        break;
    }

    switch (leBtrMgrSoInSFmt) {
    case eBTRMgrSFmt8bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_8BIT;
        break;
    case eBTRMgrSFmt16bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_16BIT;
        break;
    case eBTRMgrSFmt24bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_24BIT;
        break;
    case eBTRMgrSFmt32bit:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_32BIT;
        break;
    case eBTRMgrSFmtUnknown:
    default:
        lpcBtrMgrInSoSFmt = BTRMGR_AUDIO_SFMT_SIGNED_LE_16BIT;
        break;
    }

    switch (leBtrMgrSoInAChan) {
    case eBTRMgrAChanMono:
        lui32BtrMgrInSoAChan = 1;
        break;
    case eBTRMgrAChanDualChannel:
        lui32BtrMgrInSoAChan = 2;
        break;
    case eBTRMgrAChanStereo:
        lui32BtrMgrInSoAChan = 2;
        break;
    case eBTRMgrAChanJStereo:
        lui32BtrMgrInSoAChan = 2;
        break;
    case eBTRMgrAChan5_1:
        lui32BtrMgrInSoAChan = 6;
        break;
    case eBTRMgrAChan7_1:
        lui32BtrMgrInSoAChan = 8;
        break;
    case eBTRMgrAChanUnknown:
    default:
        lui32BtrMgrInSoAChan = 2;
        break;
    }


    switch (leBtrMgrSoOutAChan) {
    case eBTRMgrAChanMono:
        lui32BtrMgrOutSoAChan   = 1;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_MONO;
        break;
    case eBTRMgrAChanDualChannel:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_DUAL;
        break;
    case eBTRMgrAChanStereo:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_STEREO;
        break;
    case eBTRMgrAChanJStereo:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_JSTEREO;
        break;
    case eBTRMgrAChan5_1:
        lui32BtrMgrOutSoAChan   = 6;
        break;
    case eBTRMgrAChan7_1:
        lui32BtrMgrOutSoAChan   = 8;
        break;
    case eBTRMgrAChanUnknown:
    default:
        lui32BtrMgrOutSoAChan   = 2;
        lpcBtrMgrOutSoAChanMode = BTRMGR_AUDIO_CHANNELMODE_STEREO;
        break;
    }

    switch (leBtrMgrSoOutSFreq) {
    case eBTRMgrSFreq8K:
        lui32BtrMgrOutSoSFreq = 8000;
    case eBTRMgrSFreq16K:
        lui32BtrMgrOutSoSFreq = 16000;
        break;
    case eBTRMgrSFreq32K:
        lui32BtrMgrOutSoSFreq = 32000;
        break;
    case eBTRMgrSFreq44_1K:
        lui32BtrMgrOutSoSFreq = 44100;
        break;
    case eBTRMgrSFreq48K:
        lui32BtrMgrOutSoSFreq = 48000;
        break;
    case eBTRMgrSFreqUnknown:
    default:
        lui32BtrMgrOutSoSFreq = 48000;
        break;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstStart( pstBtrMgrSoHdl->hBTRMgrSoGstHdl,
                                                apstBtrMgrSoInASettings->i32BtrMgrInBufMaxSize,
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
                                                apstBtrMgrSoOutASettings->i32BtrMgrDevFd,
                                                apstBtrMgrSoOutASettings->i32BtrMgrDevMtu)) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrState  = eBTRMgrStatePlaying;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_Stop (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrRet    leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstStop(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrState  = eBTRMgrStateStopped;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32OverFlowCnt = 0;
    pstBtrMgrSoHdl->lstBtrMgrSoStatus.ui32UnderFlowCnt= 0;

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_Pause (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrRet      leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstPause(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrState  = eBTRMgrStatePaused;

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_Resume (
    tBTRMgrSoHdl hBTRMgrSoHdl
) {
    eBTRMgrRet      leBtrMgrSoRet  = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstResume(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrState  = eBTRMgrStatePlaying;

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_SendBuffer (
    tBTRMgrSoHdl    hBTRMgrSoHdl,
    char*           pcInBuf,
    int             aiInBufSize
) {
    eBTRMgrRet      leBtrMgrSoRet = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    //TODO: Implement ping-pong/triple/circular buffering if needed
#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstSendBuffer(pstBtrMgrSoHdl->hBTRMgrSoGstHdl, pcInBuf, aiInBufSize)) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    return leBtrMgrSoRet;
}


eBTRMgrRet
BTRMgr_SO_SendEOS (
    tBTRMgrSoHdl    hBTRMgrSoHdl
) {
    eBTRMgrRet      leBtrMgrSoRet = eBTRMgrSuccess;
    stBTRMgrSOHdl*  pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

#ifdef USE_GST1
    eBTRMgrSOGstRet leBtrMgrSoGstRet = eBTRMgrSOGstSuccess;
#endif

    if (pstBtrMgrSoHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#ifdef USE_GST1
    if ((leBtrMgrSoGstRet = BTRMgr_SO_GstSendEOS(pstBtrMgrSoHdl->hBTRMgrSoGstHdl)) != eBTRMgrSOGstSuccess) {
        BTMGRLOG_ERROR("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstRet);
        leBtrMgrSoRet = eBTRMgrFailure;
    }
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif

    pstBtrMgrSoHdl->lstBtrMgrSoStatus.eBtrMgrState  = eBTRMgrStateCompleted;

    return leBtrMgrSoRet;
}
