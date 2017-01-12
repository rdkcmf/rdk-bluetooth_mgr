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
BTRMgr_SO_GetEstimatedInABufSize (
    tBTRMgrSoHdl            hBTRMgrSoHdl,
    stBTRMgrSOInASettings*  apstBtrMgrSoInASettings,
    stBTRMgrSOOutASettings* apstBtrMgrSoOutASettings
) {
    eBTRMgrSORet        leBtrMgrSoRet  = eBTRMgrSOSuccess;
    stBTRMgrSOHdl*      pstBtrMgrSoHdl = (stBTRMgrSOHdl*)hBTRMgrSoHdl;

    stBTRMgrSOPCMInfo*  pstBtrMgrSoInPcmInfo = NULL;
    stBTRMgrSOSBCInfo*  pstBtrMgrSoOutSbcInfo = NULL;

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
        return eBTRMgrSONotInitialized;
    }

    if ((apstBtrMgrSoInASettings == NULL) || (apstBtrMgrSoOutASettings == NULL) ||
        (apstBtrMgrSoInASettings->pstBtrMgrSoInCodecInfo == NULL) || (apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo == NULL)) {
        return eBTRMgrSOFailInArg;
    }

    if (apstBtrMgrSoInASettings->eBtrMgrSoInAType == eBTRMgrSOATypePCM) {
        pstBtrMgrSoInPcmInfo = (stBTRMgrSOPCMInfo*)(apstBtrMgrSoInASettings->pstBtrMgrSoInCodecInfo);

        switch (pstBtrMgrSoInPcmInfo->eBtrMgrSoSFreq) {
        case eBTRMgrSOSFreq8K:
            lui32InSamplingFreq = 8000;
        case eBTRMgrSOSFreq16K:
            lui32InSamplingFreq = 16000;
            break;
        case eBTRMgrSOSFreq32K:
            lui32InSamplingFreq = 32000;
            break;
        case eBTRMgrSOSFreq44_1K:
            lui32InSamplingFreq = 44100;
            break;
        case eBTRMgrSOSFreq48K:
            lui32InSamplingFreq = 48000;
            break;
        case eBTRMgrSOSFreqUnknown:
        default:
            lui32InSamplingFreq = 48000;
            break;
        }

        switch (pstBtrMgrSoInPcmInfo->eBtrMgrSoSFmt) {
        case eBTRMgrSOSFmt8bit:
            lui32InBitsPerSample = 8;
            break;
        case eBTRMgrSOSFmt16bit:
            lui32InBitsPerSample = 16;
            break;
        case eBTRMgrSOSFmt24bit:
            lui32InBitsPerSample = 24;
            break;
        case eBTRMgrSOSFmt32bit:
            lui32InBitsPerSample = 32;
            break;
        case eBTRMgrSOSFmtUnknown:
        default:
            lui32InBitsPerSample = 16;
            break;
        }

        switch (pstBtrMgrSoInPcmInfo->eBtrMgrSoAChan) {
        case eBTRMgrSOAChanMono:
            lui32InNumAChan = 1;
            break;
        case eBTRMgrSOAChanDualChannel:
            lui32InNumAChan = 2;
            break;
        case eBTRMgrSOAChanStereo:
            lui32InNumAChan = 2;
            break;
        case eBTRMgrSOAChanJStereo:
            lui32InNumAChan = 2;
            break;
        case eBTRMgrSOAChan5_1:
            lui32InNumAChan = 6;
            break;
        case eBTRMgrSOAChan7_1:
            lui32InNumAChan = 8;
            break;
        case eBTRMgrSOAChanUnknown:
        default:
            lui32InNumAChan = 2;
            break;
        }

    }

    if (apstBtrMgrSoOutASettings->eBtrMgrSoOutAType == eBTRMgrSOATypeSBC) {
        pstBtrMgrSoOutSbcInfo = (stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo);
        lui16OutFrameLen    = pstBtrMgrSoOutSbcInfo->ui16SbcFrameLen;
        lui16OutBitrateKbps = pstBtrMgrSoOutSbcInfo->ui16SbcBitrate;
        lui16OutMtu         = apstBtrMgrSoOutASettings->i32BtrMgrSoDevMtu;
    }


    lui16OutMtu = (lui16OutMtu/lui16OutFrameLen) * lui16OutFrameLen;
    lui32OutByteRate = (lui16OutBitrateKbps * 1024) / 8;
    lfOutMtuTimemSec = (lui16OutMtu * 1000.0) / lui32OutByteRate;
    lui32InByteRate  = (lui32InBitsPerSample/8) * lui32InNumAChan * lui32InSamplingFreq;
    apstBtrMgrSoInASettings->i32BtrMgrSoInBufMaxSize = (lui32InByteRate * lfOutMtuTimemSec)/1000;

    // Align to multiple of 32
    apstBtrMgrSoInASettings->i32BtrMgrSoInBufMaxSize = apstBtrMgrSoInASettings->i32BtrMgrSoInBufMaxSize >> 5;
    apstBtrMgrSoInASettings->i32BtrMgrSoInBufMaxSize = apstBtrMgrSoInASettings->i32BtrMgrSoInBufMaxSize << 5;

    g_print("%s:%d:%s\n", __FILE__, __LINE__, __FUNCTION__);
    g_print("Effective MTU = %d\n", lui16OutMtu);
    g_print("OutByteRate = %d\n", lui32OutByteRate);
    g_print("OutMtuTimemSec = %f\n", lfOutMtuTimemSec);
    g_print("InByteRate = %d\n", lui32InByteRate);
    g_print("InBufMaxSize = %d\n", apstBtrMgrSoInASettings->i32BtrMgrSoInBufMaxSize);

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
        stBTRMgrSOPCMInfo* pstBtrMgrSoInPcmInfo = (stBTRMgrSOPCMInfo*)(apstBtrMgrSoInASettings->pstBtrMgrSoInCodecInfo);
        leBtrMgrSoInSFmt  = pstBtrMgrSoInPcmInfo->eBtrMgrSoSFmt;
        leBtrMgrSoInAChan = pstBtrMgrSoInPcmInfo->eBtrMgrSoAChan;
        leBtrMgrSoInSFreq = pstBtrMgrSoInPcmInfo->eBtrMgrSoSFreq;
    }

    if (apstBtrMgrSoOutASettings->eBtrMgrSoOutAType == eBTRMgrSOATypeSBC) {
        stBTRMgrSOSBCInfo* pstBtrMgrSoOutSbcInfo = (stBTRMgrSOSBCInfo*)(apstBtrMgrSoOutASettings->pstBtrMgrSoOutCodecInfo);

        leBtrMgrSoOutAChan = pstBtrMgrSoOutSbcInfo->eBtrMgrSoSbcAChan;
        leBtrMgrSoOutSFreq = pstBtrMgrSoOutSbcInfo->eBtrMgrSoSbcSFreq;
        lui8SbcAllocMethod = pstBtrMgrSoOutSbcInfo->ui8SbcAllocMethod;
        lui8SbcSubbands    = pstBtrMgrSoOutSbcInfo->ui8SbcSubbands;
        lui8SbcBlockLength = pstBtrMgrSoOutSbcInfo->ui8SbcBlockLength;
        lui8SbcMinBitpool  = pstBtrMgrSoOutSbcInfo->ui8SbcMinBitpool;
        lui8SbcMaxBitpool  = pstBtrMgrSoOutSbcInfo->ui8SbcMaxBitpool;
    }


#ifdef USE_GST1
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
                                                apstBtrMgrSoInASettings->i32BtrMgrSoInBufMaxSize,
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
                                                apstBtrMgrSoOutASettings->i32BtrMgrSoDevFd,
                                                apstBtrMgrSoOutASettings->i32BtrMgrSoDevMtu)) != eBTRMgrSOGstSuccess) {
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
