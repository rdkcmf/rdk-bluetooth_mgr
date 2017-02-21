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
 * @file btrMgr_audioCap.c
 *
 * @description This file implements bluetooth manager's Generic Audio Capture interface from external modules
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */

/* Ext lib Headers */
#include <glib.h>

/* Interface lib Headers */
#if defined(USE_AC_RMF)
#include "rmfAudioCapture.h"
#else

#endif

/* Local Headers */
#include "btrMgr_mediaTypes.h"
#include "btrMgr_audioCap.h"


/* Local types */
typedef struct _stBTRMgrACHdl {
    stBTRMgrMediaStatus         lstBtrMgrAcStatus;
#if defined(USE_AC_RMF)
    RMF_AudioCaptureHandle      hBTRMgrRmfAcHdl;
    RMF_AudioCapture_Settings   lstBtrMgrRmfAcDefSettings;
    RMF_AudioCapture_Settings   lstBtrMgrRmfAcCurSettings;
#else
#endif
    BTRMgr_AC_DataReadyCb       lfptrBtrMgrAcDataReadycB;
    void*                       lvpBtrMgrAcDataReadyUserData;
} stBTRMgrACHdl;


/* Callbacks Prototypes */
#if defined(USE_AC_RMF)
static rmf_Error cbBufferReady (void* pContext, void* pInDataBuf, unsigned int inBytesToEncode);
#endif

#if defined(USE_AC_RMF)
static rmf_Error
cbBufferReady (
    void*           pContext,
    void*           pInDataBuf,
    unsigned int    inBytesToEncode
) {
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)pContext;

    if (pstBtrMgrAcHdl && pstBtrMgrAcHdl->lfptrBtrMgrAcDataReadycB) {
        if (pstBtrMgrAcHdl->lfptrBtrMgrAcDataReadycB(pInDataBuf, inBytesToEncode, pstBtrMgrAcHdl->lvpBtrMgrAcDataReadyUserData) != eBTRMgrSuccess) {
            g_print ("%s:%d:%s - AC Data Ready Callback Failed\n", __FILE__, __LINE__, __FUNCTION__);
        }
    }

    return RMF_SUCCESS;
}
#endif



eBTRMgrRet
BTRMgr_AC_Init (
    tBTRMgrAcHdl*   phBTRMgrAcHdl
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = NULL;

#if defined(USE_AC_RMF)
    rmf_Error       leBtrMgrRmfAcRet = RMF_SUCCESS; 
#endif

    if ((pstBtrMgrAcHdl = (stBTRMgrACHdl*)g_malloc0 (sizeof(stBTRMgrACHdl))) == NULL) {
        g_print ("%s:%d:%s - Unable to allocate memory\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrInitFailure;
    }

#if defined(USE_AC_RMF)
    if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Open(&pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrRmfAcRet);
        leBtrMgrAcRet = eBTRMgrInitFailure;
    }
#endif

    *phBTRMgrAcHdl = (tBTRMgrAcHdl)pstBtrMgrAcHdl;
    
    return leBtrMgrAcRet;
}


eBTRMgrRet
BTRMgr_AC_DeInit (
    tBTRMgrAcHdl hBTRMgrAcHdl
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)hBTRMgrAcHdl;

#if defined(USE_AC_RMF)
    rmf_Error       leBtrMgrRmfAcRet = RMF_SUCCESS; 
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#if defined(USE_AC_RMF)
    if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Close(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrRmfAcRet);
        leBtrMgrAcRet = eBTRMgrFailure;
    }
    pstBtrMgrAcHdl->hBTRMgrRmfAcHdl = NULL;
#endif

    g_free((void*)pstBtrMgrAcHdl);
    pstBtrMgrAcHdl = NULL;

    return leBtrMgrAcRet;
}


eBTRMgrRet
BTRMgr_AC_GetDefaultSettings (
    tBTRMgrAcHdl            hBTRMgrAcHdl,
    stBTRMgrOutASettings*   apstBtrMgrAcOutASettings
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)hBTRMgrAcHdl;

#if defined(USE_AC_RMF)
    rmf_Error       leBtrMgrRmfAcRet = RMF_SUCCESS; 
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    if ((apstBtrMgrAcOutASettings == NULL) || (apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo == NULL)) {
        return eBTRMgrFailInArg;
    }


#if defined(USE_AC_RMF)
    if ((leBtrMgrRmfAcRet = RMF_AudioCapture_GetDefaultSettings(&pstBtrMgrAcHdl->lstBtrMgrRmfAcDefSettings)) != RMF_SUCCESS) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrRmfAcRet);
        leBtrMgrAcRet = eBTRMgrFailure;
    }

    g_print("%s:%d:%s - Default CBBufferReady = %p\n", __FILE__, __LINE__, __FUNCTION__, pstBtrMgrAcHdl->lstBtrMgrRmfAcDefSettings.cbBufferReady);
    g_print("%s:%d:%s - Default Fifosize      = %d\n", __FILE__, __LINE__, __FUNCTION__, pstBtrMgrAcHdl->lstBtrMgrRmfAcDefSettings.fifoSize);
    g_print("%s:%d:%s - Default Threshold     = %d\n", __FILE__, __LINE__, __FUNCTION__, pstBtrMgrAcHdl->lstBtrMgrRmfAcDefSettings.threshold);

    //TODO: Get the format capture format from RMF_AudioCapture Settings
    apstBtrMgrAcOutASettings->eBtrMgrOutAType     = eBTRMgrATypePCM;

    if (apstBtrMgrAcOutASettings->eBtrMgrOutAType == eBTRMgrATypePCM) {
         stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo);

        switch (pstBtrMgrAcHdl->lstBtrMgrRmfAcDefSettings.format) {
        case racFormat_e16BitStereo:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
            break;
        case racFormat_e24BitStereo:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
            break;
        case racFormat_e16BitMonoLeft:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
            break;
        case racFormat_e16BitMonoRight:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
            break;
        case racFormat_e16BitMono:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
            break;
        case racFormat_e24Bit5_1:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChan5_1;
            break;
        case racFormat_eMax:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmtUnknown;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanUnknown;
            break;
        default:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
            pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
            break;
        }

        switch (pstBtrMgrAcHdl->lstBtrMgrRmfAcDefSettings.samplingFreq) {
        case racFreq_e16000:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq16K;
            break;
        case racFreq_e32000:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq32K;
            break;
        case racFreq_e44100:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq44_1K;
            break;
        case racFreq_e48000:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq48K;
            break;
        case racFreq_eMax:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreqUnknown;
            break;
        default:
            pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq48K;
            break;
        }
    }
    else {
        leBtrMgrAcRet  = eBTRMgrFailure;
    }
#endif

    return leBtrMgrAcRet;
}

eBTRMgrRet
BTRMgr_AC_GetCurrentSettings (
    tBTRMgrAcHdl            hBTRMgrAcHdl,
    stBTRMgrOutASettings*   apstBtrMgrAcOutASettings
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)hBTRMgrAcHdl;

#if defined(USE_AC_RMF)
    rmf_Error       leBtrMgrRmfAcRet = RMF_SUCCESS; 
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    if (apstBtrMgrAcOutASettings == NULL) {
        return eBTRMgrFailInArg;
    }

#if defined(USE_AC_RMF)
    if ((leBtrMgrRmfAcRet = RMF_AudioCapture_GetCurrentSettings( pstBtrMgrAcHdl->hBTRMgrRmfAcHdl,
                                                                &pstBtrMgrAcHdl->lstBtrMgrRmfAcCurSettings)) != RMF_SUCCESS) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrRmfAcRet);
        leBtrMgrAcRet = eBTRMgrFailure;
    }
#endif

    return leBtrMgrAcRet;
}


eBTRMgrRet
BTRMgr_AC_GetStatus (
    tBTRMgrAcHdl            hBTRMgrAcHdl,
    stBTRMgrMediaStatus*    apstBtrMgrAcStatus
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;

    return leBtrMgrAcRet;
}


eBTRMgrRet
BTRMgr_AC_Start (
    tBTRMgrAcHdl            hBTRMgrAcHdl,
    stBTRMgrOutASettings*   apstBtrMgrAcOutASettings,
    BTRMgr_AC_DataReadyCb   afptrBtrMgrAcDataReadycB,
    void*                   apvUserData
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)hBTRMgrAcHdl;

#if defined(USE_AC_RMF)
    rmf_Error                   leBtrMgrRmfAcRet = RMF_SUCCESS; 
    RMF_AudioCapture_Settings*  pstBtrMgrRmfAcSettings = NULL;
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    if (apstBtrMgrAcOutASettings == NULL) {
        return eBTRMgrFailInArg;
    }

    pstBtrMgrAcHdl->lfptrBtrMgrAcDataReadycB    = afptrBtrMgrAcDataReadycB;
    pstBtrMgrAcHdl->lvpBtrMgrAcDataReadyUserData= apvUserData;

#if defined(USE_AC_RMF)
    if (pstBtrMgrAcHdl->lstBtrMgrRmfAcCurSettings.fifoSize)
        pstBtrMgrRmfAcSettings = &pstBtrMgrAcHdl->lstBtrMgrRmfAcCurSettings;
    else
        pstBtrMgrRmfAcSettings = &pstBtrMgrAcHdl->lstBtrMgrRmfAcDefSettings;



    pstBtrMgrRmfAcSettings->cbBufferReady      = cbBufferReady;
    pstBtrMgrRmfAcSettings->cbBufferReadyParm  = pstBtrMgrAcHdl;
    pstBtrMgrRmfAcSettings->fifoSize           = 8 * apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;
    pstBtrMgrRmfAcSettings->threshold          = apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;

    //TODO: Work on a intelligent way to arrive at this value. This is not good enough
    if (pstBtrMgrRmfAcSettings->threshold > 4096)
        pstBtrMgrRmfAcSettings->delayCompensation_ms = 240;
    else
        pstBtrMgrRmfAcSettings->delayCompensation_ms = 200;
    //TODO: Bad hack above, need to modify before taking it to stable2


    if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Start(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl, 
                                                   pstBtrMgrRmfAcSettings)) != RMF_SUCCESS) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrRmfAcRet);
        leBtrMgrAcRet = eBTRMgrFailure;
    }
#endif

    return leBtrMgrAcRet;
}


eBTRMgrRet
BTRMgr_AC_Stop (
    tBTRMgrAcHdl hBTRMgrAcHdl
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)hBTRMgrAcHdl;

#if defined(USE_AC_RMF)
    rmf_Error       leBtrMgrRmfAcRet = RMF_SUCCESS; 
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#if defined(USE_AC_RMF)
    if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Stop(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
        g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrRmfAcRet);
        leBtrMgrAcRet = eBTRMgrFailure;
    }
#endif

    return leBtrMgrAcRet;
}


eBTRMgrRet
BTRMgr_AC_Pause (
    tBTRMgrAcHdl hBTRMgrAcHdl
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;

    return leBtrMgrAcRet;
}


eBTRMgrRet BTRMgr_AC_Resume (
    tBTRMgrAcHdl hBTRMgrAcHdl
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;

    return leBtrMgrAcRet;
}
