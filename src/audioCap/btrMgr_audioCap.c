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
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#if defined(USE_ACM)
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#endif

/* Ext lib Headers */
#include <glib.h>

/* Interface lib Headers */
#if defined(USE_AC_RMF)
#include "rmfAudioCapture.h"
#endif

#if defined(USE_ACM)
#include "libIBus.h"
#include "libIARM.h"

#include "audiocapturemgr_iarm.h"
#endif

#include "btrMgr_logger.h"
#ifdef RDK_LOGGER_ENABLED
int b_rdk_logger_enabled = 0;
#endif

/* Local Headers */
#include "btrMgr_Types.h"
#include "btrMgr_mediaTypes.h"
#include "btrMgr_audioCap.h"

/* Local defines */
#if defined(USE_ACM)
//TODO: Should match the value in src/rpc/btmgr_iarm_interface.h. Find a better way
#define IARM_BUS_BTRMGR_NAME        "BTRMgrBus" 
#endif

#if defined(BUILD_SKY)
#define TV_CUSTOM_DELAY_COMP 1
#endif

/* Local types */
#if defined(USE_ACM)
typedef enum _eBTRMgrACAcmDCOp {
    eBTRMgrACAcmDCStart,
    eBTRMgrACAcmDCStop,
    eBTRMgrACAcmDCResume,
    eBTRMgrACAcmDCPause,
    eBTRMgrACAcmDCExit,
    eBTRMgrACAcmDCUnknown
} eBTRMgrACAcmDCOp;
#endif

typedef struct _stBTRMgrACHdl {
    stBTRMgrMediaStatus         stBtrMgrAcStatus;
#if defined(USE_AC_RMF)
    RMF_AudioCaptureHandle      hBTRMgrRmfAcHdl;
    RMF_AudioCapture_Settings   stBtrMgrRmfAcDefSettings;
    RMF_AudioCapture_Settings   stBtrMgrRmfAcCurSettings;
    RMF_AudioCapture_Status     stBtrMgrRmfAcStatus;
#endif

#if defined(USE_ACM)
    GThread*                    pBtrMgrAcmDataCapGThread;
    GAsyncQueue*                pBtrMgrAcmDataCapGAOpQueue;
    session_id_t                hBtrMgrIarmAcmHdl;
    audio_properties_ifce_t     stBtrMgrAcmDefSettings;
    audio_properties_ifce_t     stBtrMgrAcmCurSettings;
    audio_properties_ifce_t*    pstBtrMgrAcmSettings;
    char                        pcBtrMgrAcmSockPath[MAX_OUTPUT_PATH_LEN];
    int                         i32BtrMgrAcmDCSockFd;
    int                         i32BtrMgrAcmExternalIARMMode;
#endif

    tBTRMgrAcType               pcBTRMgrAcType;

    fPtr_BTRMgr_AC_DataReadyCb  fpcBBtrMgrAcDataReady;
    void*                       vpBtrMgrAcDataReadyUserData;

    fPtr_BTRMgr_AC_StatusCb     fpcBBtrMgrAcStatus;
    void*                       vpBtrMgrAcStatusUserData;
} stBTRMgrACHdl;


/* Static Function Prototypes */

/* Local Op Threads */
#if defined(USE_ACM)
static gpointer btrMgr_AC_acmDataCapture_InTask (gpointer user_data);
#endif

/* Incoming Callbacks */
#if defined(USE_AC_RMF)
static rmf_Error btrMgr_AC_rmfBufferReadyCb (void* pContext, void* pInDataBuf, unsigned int inBytesToEncode);
static rmf_Error btrMgr_AC_rmfStatusChangeCb (void* pContext);
#endif


/* Static Function Definition */

/* Local Op Threads */
#if defined(USE_ACM)
static gpointer
btrMgr_AC_acmDataCapture_InTask (
    gpointer user_data
) {
    stBTRMgrACHdl*      pstBtrMgrAcHdl = (stBTRMgrACHdl*)user_data;
    gint64              li64usTimeout = 0;
    guint16             lui16msTimeout = 500;
    eBTRMgrACAcmDCOp    leBtrMgrAcmDCPrvOp  = eBTRMgrACAcmDCUnknown;
    eBTRMgrACAcmDCOp    leBtrMgrAcmDCCurOp  = eBTRMgrACAcmDCUnknown;
    gpointer            lpeBtrMgrAcmDCOp = NULL;
    void*               lpInDataBuf = NULL;


    if (pstBtrMgrAcHdl == NULL) {
        BTRMGRLOG_ERROR("Fail - eBTRMgrFailInArg\n");
        return NULL;
    }

    BTRMGRLOG_DEBUG ("Enter\n");

    do {
        /* Process incoming events */
        {
            li64usTimeout = lui16msTimeout * G_TIME_SPAN_MILLISECOND;
            if ((lpeBtrMgrAcmDCOp = g_async_queue_timeout_pop(pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue, li64usTimeout)) != NULL) {
                leBtrMgrAcmDCCurOp = *((eBTRMgrACAcmDCOp*)lpeBtrMgrAcmDCOp);
                g_free(lpeBtrMgrAcmDCOp);
                BTRMGRLOG_DEBUG ("g_async_queue_timeout_pop %d\n", leBtrMgrAcmDCCurOp);
            }
        }


        /* Set up operation changes */
        if (leBtrMgrAcmDCPrvOp != leBtrMgrAcmDCCurOp) {
            leBtrMgrAcmDCPrvOp = leBtrMgrAcmDCCurOp;

            /* eBTRMgrACAcmDCStart - START */
            if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCStart) {
                struct sockaddr_un  lstBtrMgrAcmDCSockAddr;
                int                 li32BtrMgrAcmDCSockFd = -1;
                int                 li32BtrMgrAcmDCSockFlags;
                int                 lerrno = 0;

                lui16msTimeout = 1;
                BTRMGRLOG_INFO ("eBTRMgrACAcmDCStart\n");

                if (!strlen(pstBtrMgrAcHdl->pcBtrMgrAcmSockPath)) {
                    BTRMGRLOG_ERROR("eBTRMgrACAcmDCStart - Invalid Socket Path\n");
                }
                else {
                    BTRMGRLOG_DEBUG ("pcBtrMgrAcmSockPath = %s\n", pstBtrMgrAcHdl->pcBtrMgrAcmSockPath);

                    lstBtrMgrAcmDCSockAddr.sun_family = AF_UNIX;
                    strncpy(lstBtrMgrAcmDCSockAddr.sun_path, pstBtrMgrAcHdl->pcBtrMgrAcmSockPath, 
                        strlen(pstBtrMgrAcHdl->pcBtrMgrAcmSockPath) < (sizeof(lstBtrMgrAcmDCSockAddr.sun_path) - 1) ?
                            strlen(pstBtrMgrAcHdl->pcBtrMgrAcmSockPath) + 1 : sizeof(lstBtrMgrAcmDCSockAddr.sun_path));  //CID:136289 - Buffer size and 23363 - Overrun


                    if ((li32BtrMgrAcmDCSockFd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
                        lerrno = errno;
                        BTRMGRLOG_ERROR("eBTRMgrACAcmDCStart - Unable to create socket :FAILURE - %d\n", lerrno);
                    }

                    if ((li32BtrMgrAcmDCSockFd != -1) &&
                        ((li32BtrMgrAcmDCSockFlags = fcntl(li32BtrMgrAcmDCSockFd, F_GETFL, 0)) != -1) &&
                        (fcntl(li32BtrMgrAcmDCSockFd, F_SETFL, li32BtrMgrAcmDCSockFlags | O_NONBLOCK) != -1)) {
                        BTRMGRLOG_DEBUG("eBTRMgrACAcmDCStart - Socket O_NONBLOCK : SUCCESS\n");
                    }
                    
                    if ((li32BtrMgrAcmDCSockFd != -1) && 
                        (connect(li32BtrMgrAcmDCSockFd, (const struct sockaddr*)&lstBtrMgrAcmDCSockAddr, sizeof(lstBtrMgrAcmDCSockAddr)) == -1)) {
                        lerrno = errno;
                        BTRMGRLOG_ERROR("eBTRMgrACAcmDCStart - Unable to connect socket :FAILURE - %d\n", lerrno);
                        if( li32BtrMgrAcmDCSockFd > 0 ) {
                            close(li32BtrMgrAcmDCSockFd);
                            li32BtrMgrAcmDCSockFd = -1;
                        }
                        lerrno = errno;
                    }
                    else {
                        lerrno = errno;
                        if (!(lpInDataBuf = malloc(pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold))) {
                            BTRMGRLOG_ERROR("eBTRMgrACAcmDCStart - Unable to alloc\n");
                            break;
                        } 
                    }
                }

                pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd = li32BtrMgrAcmDCSockFd;
                BTRMGRLOG_INFO ("eBTRMgrACAcmDCStart - Read socket : %d - %d\n", pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd, lerrno);
            }
            /* eBTRMgrACAcmDCStop - STOP */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCStop) {
                lui16msTimeout = 50;
                BTRMGRLOG_INFO ("eBTRMgrACAcmDCStop\n");

                if (pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd == -1) {
                    BTRMGRLOG_ERROR("eBTRMgrACAcmDCStop :FAILURE\n");
                }
                else {
                    iarmbus_acm_arg_t   lstBtrMgrIarmAcmArgs;
                    IARM_Result_t       leBtrMgIarmAcmRet = IARM_RESULT_SUCCESS;

                    // Flush the read queue before closing the read socket
                    if (lpInDataBuf) {
                        int li32InDataBufBytesRead = 0;
                        unsigned int lui32EmptyDataIdx = 8; // BTRMGR_MAX_INTERNAL_QUEUE_ELEMENTS

                        do {
                            li32InDataBufBytesRead = (int)read( pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd, 
                                                                lpInDataBuf, 
                                                                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold);
                        } while ((li32InDataBufBytesRead > 0) && --lui32EmptyDataIdx);

                        free(lpInDataBuf);
                        lpInDataBuf  = NULL;
                    }

                    if (pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd  >  0) {
                        close(pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd);  //CID:23436
                        pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd = -1;
                    }

                    memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
                    lstBtrMgrIarmAcmArgs.session_id = pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl;

                    if ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                            IARMBUS_AUDIOCAPTUREMGR_STOP,
                                                            (void *)&lstBtrMgrIarmAcmArgs,
                                                            sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS) {
                        BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_STOP:Return Status = %d\n", leBtrMgIarmAcmRet);
                    }

                    if (lstBtrMgrIarmAcmArgs.result != 0) {
                        BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
                    }

                }
            }
            /* eBTRMgrACAcmDCPause - PAUSE */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCPause) {
                lui16msTimeout = 500;
                BTRMGRLOG_INFO ("eBTRMgrACAcmDCPause\n");

            }
            /* eBTRMgrACAcmDCResume - RESUME */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCResume) {
                lui16msTimeout = 1;
                BTRMGRLOG_INFO ("eBTRMgrACAcmDCResume\n");

            }
            /* eBTRMgrACAcmDCExit - EXIT */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCExit) {
                BTRMGRLOG_INFO ("eBTRMgrACAcmDCExit\n");
                break;
            }
            /* eBTRMgrACAcmDCUnknown - UNKNOWN */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCUnknown) {
                g_thread_yield();
            }
        }


        /* Process Operations */
        {
            /* eBTRMgrACAcmDCStart - START */
            if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCStart) {
                int li32InDataBufBytesRead = 0;

                if((pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd >= 0) && (lpInDataBuf)) {
                    li32InDataBufBytesRead = (int)read( pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd,
                                                        lpInDataBuf,
                                                        pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold);
                }  //CID:23331 and 23351- Negative returns, 23362 - Forward null


                if (pstBtrMgrAcHdl->fpcBBtrMgrAcDataReady && (li32InDataBufBytesRead > 0)) {
                    if (pstBtrMgrAcHdl->fpcBBtrMgrAcDataReady(lpInDataBuf,
                                                              li32InDataBufBytesRead,
                                                              pstBtrMgrAcHdl->vpBtrMgrAcDataReadyUserData) != eBTRMgrSuccess) {
                        BTRMGRLOG_ERROR("AC Data Ready Callback Failed\n");
                    }
                }
            }
            /* eBTRMgrACAcmDCStop - STOP */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCStop) {
                g_thread_yield();
            }
            /* eBTRMgrACAcmDCPause - PAUSE */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCPause) {
            }
            /* eBTRMgrACAcmDCResume - RESUME */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCResume) {
            }
            /* eBTRMgrACAcmDCExit - EXIT */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCExit) {
                g_thread_yield();
            }
            /* eBTRMgrACAcmDCUnknown - UNKNOWN */
            else if (leBtrMgrAcmDCCurOp == eBTRMgrACAcmDCUnknown) {
                g_thread_yield();
            }
        }

    } while(1);

    if(lpInDataBuf != NULL) {
        free(lpInDataBuf);  //CID:23332 - Resource leak
    }
    BTRMGRLOG_DEBUG ("Exit\n");

    return NULL;
}
#endif


/* Interfaces */
eBTRMgrRet
BTRMgr_AC_Init (
    tBTRMgrAcHdl*   phBTRMgrAcHdl,
    tBTRMgrAcType   api8BTRMgrAcType
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = NULL;

#if defined(USE_AC_RMF)
    rmf_Error               leBtrMgrRmfAcRet = RMF_SUCCESS;
    RMF_AudioCaptureType    lrmfAcType;
#endif

#if defined(USE_ACM)
    iarmbus_acm_arg_t   lstBtrMgrIarmAcmArgs;
    IARM_Result_t       leBtrMgIarmAcmRet = IARM_RESULT_SUCCESS;
    const char*         lpcProcessName = IARM_BUS_BTRMGR_NAME;
#endif

    if ((pstBtrMgrAcHdl = (stBTRMgrACHdl*)g_malloc0 (sizeof(stBTRMgrACHdl))) == NULL) {
        BTRMGRLOG_ERROR("Unable to allocate memory\n");
        return eBTRMgrInitFailure;
    }

#if defined(USE_AC_RMF)
    if ((api8BTRMgrAcType != NULL) &&
        (!strncmp(api8BTRMgrAcType, BTRMGR_AC_TYPE_AUXILIARY, strlen(BTRMGR_AC_TYPE_AUXILIARY)))) {
        lrmfAcType = RMF_AC_TYPE_AUXILIARY;
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Open_Type(&pstBtrMgrAcHdl->hBTRMgrRmfAcHdl, lrmfAcType)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("RMF_AudioCapture_Open:Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrInitFailure;
        }
    }
#endif

#if defined(USE_ACM)
    if ((api8BTRMgrAcType == NULL) ||
        (!strncmp(api8BTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        memset(pstBtrMgrAcHdl->pcBtrMgrAcmSockPath, '\0', MAX_OUTPUT_PATH_LEN);
        pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd = -1;

        if(!IARM_Bus_IsConnected(lpcProcessName, &pstBtrMgrAcHdl->i32BtrMgrAcmExternalIARMMode)) {
            BTRMGRLOG_TRACE("Error in IARM_Bus_IsConnected\n");
        }  //CID:101819 - Checked return

        if (!pstBtrMgrAcHdl->i32BtrMgrAcmExternalIARMMode) {
            IARM_Bus_Init(lpcProcessName);
            IARM_Bus_Connect();
        }


        pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl = -1;
        memset(&pstBtrMgrAcHdl->stBtrMgrAcmDefSettings, 0, sizeof(audio_properties_ifce_t));
        memset(&pstBtrMgrAcHdl->stBtrMgrAcmCurSettings, 0, sizeof(audio_properties_ifce_t));


        memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
        lstBtrMgrIarmAcmArgs.details.arg_open.source = 0; //primary
        lstBtrMgrIarmAcmArgs.details.arg_open.output_type = REALTIME_SOCKET;

        if ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                IARMBUS_AUDIOCAPTUREMGR_OPEN,
                                                (void *)&lstBtrMgrIarmAcmArgs,
                                                sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_OPEN:Return Status = %d\n", leBtrMgIarmAcmRet);
            leBtrMgrAcRet = eBTRMgrInitFailure;
        }

        if ((leBtrMgrAcRet != eBTRMgrSuccess) || (lstBtrMgrIarmAcmArgs.result != 0)) {
            BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
            leBtrMgrAcRet = eBTRMgrInitFailure;
        }
        else {
            pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl = lstBtrMgrIarmAcmArgs.session_id;

            if (((pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue = g_async_queue_new()) == NULL) ||
                ((pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread = g_thread_new("btrMgr_AC_acmDataCapture_InTask", btrMgr_AC_acmDataCapture_InTask, pstBtrMgrAcHdl)) == NULL)) {
                leBtrMgrAcRet = eBTRMgrInitFailure;
            }

            BTRMGRLOG_DEBUG ("btrMgr_AC_acmDataCapture_InTask : %p\n", pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread);
        }
    }
#elif defined(USE_AC_RMF)
    else if ((api8BTRMgrAcType == NULL) ||
             (!strncmp(api8BTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        lrmfAcType = RMF_AC_TYPE_PRIMARY;
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Open(&pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("RMF_AudioCapture_Open:Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrInitFailure;
        }
    }
#endif

    if (api8BTRMgrAcType)
        pstBtrMgrAcHdl->pcBTRMgrAcType = g_strndup(api8BTRMgrAcType, 32);

    if (leBtrMgrAcRet != eBTRMgrSuccess) {
        BTRMgr_AC_DeInit((tBTRMgrAcHdl)pstBtrMgrAcHdl);  //CID:127655 - Use after free
        return leBtrMgrAcRet;
    }

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
    rmf_Error           leBtrMgrRmfAcRet = RMF_SUCCESS; 
#endif

#if defined(USE_ACM)
    iarmbus_acm_arg_t   lstBtrMgrIarmAcmArgs;
    IARM_Result_t       leBtrMgIarmAcmRet = IARM_RESULT_SUCCESS;
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

#if defined(USE_AC_RMF)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType != NULL) &&
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_AUXILIARY, strlen(BTRMGR_AC_TYPE_AUXILIARY)))  &&
        (pstBtrMgrAcHdl->hBTRMgrRmfAcHdl != NULL)) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Close(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        pstBtrMgrAcHdl->hBTRMgrRmfAcHdl = NULL;
    }
#endif

#if defined(USE_ACM)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        if (pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread) {
            gpointer    lpeBtrMgrAcmDCOp = NULL;
            if ((lpeBtrMgrAcmDCOp = g_malloc0(sizeof(eBTRMgrACAcmDCOp))) != NULL) {
                *((eBTRMgrACAcmDCOp*)lpeBtrMgrAcmDCOp) = eBTRMgrACAcmDCExit;
                g_async_queue_push(pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue, lpeBtrMgrAcmDCOp);
                BTRMGRLOG_DEBUG ("g_async_queue_push: eBTRMgrACAcmDCExit\n");
            }
        }
        else {
            BTRMGRLOG_ERROR("pBtrMgrAcmDataCapGThread: eBTRMgrACAcmDCExit - FAILED\n");
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        if (pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread) {
            g_thread_join(pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread);
            pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread = NULL;
        }

        if (pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue) {
            g_async_queue_unref(pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue);
            pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue = NULL;
        }


        memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
        lstBtrMgrIarmAcmArgs.session_id = pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl;

        if ((lstBtrMgrIarmAcmArgs.session_id != 0) &&
            ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                 IARMBUS_AUDIOCAPTUREMGR_CLOSE,
                                                 (void *)&lstBtrMgrIarmAcmArgs,
                                                 sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS)) {
            BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_CLOSE:Return Status = %d\n", leBtrMgIarmAcmRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        if ((leBtrMgrAcRet != eBTRMgrSuccess) || (lstBtrMgrIarmAcmArgs.result != 0)) {
            BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        memset(&pstBtrMgrAcHdl->stBtrMgrAcmDefSettings, 0, sizeof(audio_properties_ifce_t));
        memset(&pstBtrMgrAcHdl->stBtrMgrAcmCurSettings, 0, sizeof(audio_properties_ifce_t));
        pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl = -1;


        if (!pstBtrMgrAcHdl->i32BtrMgrAcmExternalIARMMode) {
            IARM_Bus_Disconnect();
            IARM_Bus_Term();
        }

        pstBtrMgrAcHdl->i32BtrMgrAcmDCSockFd = -1;
        memset(pstBtrMgrAcHdl->pcBtrMgrAcmSockPath, '\0', MAX_OUTPUT_PATH_LEN);
    }
#elif defined(USE_AC_RMF)
    else if (((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
             (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY))))  &&
             (pstBtrMgrAcHdl->hBTRMgrRmfAcHdl != NULL)) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Close(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        pstBtrMgrAcHdl->hBTRMgrRmfAcHdl = NULL;
    }
#endif

    if (pstBtrMgrAcHdl->pcBTRMgrAcType) {
        g_free(pstBtrMgrAcHdl->pcBTRMgrAcType);
        pstBtrMgrAcHdl->pcBTRMgrAcType = NULL;
    }

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
    rmf_Error           leBtrMgrRmfAcRet = RMF_SUCCESS; 
#endif

#if defined(USE_ACM)
    iarmbus_acm_arg_t   lstBtrMgrIarmAcmArgs;
    IARM_Result_t       leBtrMgIarmAcmRet = IARM_RESULT_SUCCESS;
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    if ((apstBtrMgrAcOutASettings == NULL) || (apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo == NULL)) {
        return eBTRMgrFailInArg;
    }


#if defined(USE_AC_RMF)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType != NULL) &&
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_AUXILIARY, strlen(BTRMGR_AC_TYPE_AUXILIARY)))) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_GetDefaultSettings(&pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        BTRMGRLOG_TRACE ("Default CBBufferReady = %p\n", pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.cbBufferReady);
        BTRMGRLOG_TRACE ("Default Fifosize      = %d\n", (int)pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.fifoSize);
        BTRMGRLOG_TRACE ("Default Threshold     = %d\n", (int)pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.threshold);

        //TODO: Get the format capture format from RMF_AudioCapture Settings
        apstBtrMgrAcOutASettings->eBtrMgrOutAType     = eBTRMgrATypePCM;

        if (apstBtrMgrAcOutASettings->eBtrMgrOutAType == eBTRMgrATypePCM) {
             stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo);

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.format) {
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

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.samplingFreq) {
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
    }
#endif

#if defined(USE_ACM)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
        lstBtrMgrIarmAcmArgs.session_id = pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl;

        if ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                IARMBUS_AUDIOCAPTUREMGR_GET_DEFAULT_AUDIO_PROPS,
                                                (void *)&lstBtrMgrIarmAcmArgs,
                                                sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_GET_DEFAULT_AUDIO_PROPS:Return Status = %d\n", leBtrMgIarmAcmRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        if ((leBtrMgrAcRet != eBTRMgrSuccess) || (lstBtrMgrIarmAcmArgs.result != 0)) {
            BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
            leBtrMgrAcRet = eBTRMgrFailure;
        }


        if (leBtrMgrAcRet == eBTRMgrSuccess) {
            memcpy(&pstBtrMgrAcHdl->stBtrMgrAcmDefSettings, &lstBtrMgrIarmAcmArgs.details.arg_audio_properties, sizeof(audio_properties_ifce_t));

            BTRMGRLOG_TRACE ("Default Fifosize = %d\n", (int)pstBtrMgrAcHdl->stBtrMgrAcmDefSettings.fifo_size);
            BTRMGRLOG_TRACE ("Default Threshold= %d\n", (int)pstBtrMgrAcHdl->stBtrMgrAcmDefSettings.threshold);
            BTRMGRLOG_TRACE ("Default DelayComp= %d\n", pstBtrMgrAcHdl->stBtrMgrAcmDefSettings.delay_compensation_ms);

            //TODO: Get the format capture format from IARMBUS_AUDIOCAPTUREMGR_NAME
            apstBtrMgrAcOutASettings->eBtrMgrOutAType     = eBTRMgrATypePCM;

            if (apstBtrMgrAcOutASettings->eBtrMgrOutAType == eBTRMgrATypePCM) {
                 stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo);

                switch (pstBtrMgrAcHdl->stBtrMgrAcmDefSettings.format) {
                case acmFormate16BitStereo:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
                    break;
                case acmFormate24BitStereo:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
                    break;
                case acmFormate16BitMonoLeft:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
                    break;
                case acmFormate16BitMonoRight:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
                    break;
                case acmFormate16BitMono:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
                    break;
                case acmFormate24Bit5_1:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChan5_1;
                    break;
                case acmFormateMax:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmtUnknown;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanUnknown;
                    break;
                default:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
                    break;
                }

                switch (pstBtrMgrAcHdl->stBtrMgrAcmDefSettings.sampling_frequency) {
                case acmFreqe16000:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq16K;
                    break;
                case acmFreqe32000:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq32K;
                    break;
                case acmFreqe44100:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq44_1K;
                    break;
                case acmFreqe48000:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq48K;
                    break;
                case acmFreqeMax:
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
        }
    }
#elif defined(USE_AC_RMF)
    else if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_GetDefaultSettings(&pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        BTRMGRLOG_TRACE ("Default CBBufferReady = %p\n", pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.cbBufferReady);
        BTRMGRLOG_TRACE ("Default Fifosize      = %d\n", pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.fifoSize);
        BTRMGRLOG_TRACE ("Default Threshold     = %d\n", pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.threshold);

        //TODO: Get the format capture format from RMF_AudioCapture Settings
        apstBtrMgrAcOutASettings->eBtrMgrOutAType     = eBTRMgrATypePCM;

        if (apstBtrMgrAcOutASettings->eBtrMgrOutAType == eBTRMgrATypePCM) {
             stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo);

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.format) {
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

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings.samplingFreq) {
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

#if defined(USE_ACM)
    iarmbus_acm_arg_t   lstBtrMgrIarmAcmArgs;
    IARM_Result_t       leBtrMgIarmAcmRet = IARM_RESULT_SUCCESS;
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    if (apstBtrMgrAcOutASettings == NULL) {
        return eBTRMgrFailInArg;
    }

#if defined(USE_AC_RMF)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType != NULL) &&
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_AUXILIARY, strlen(BTRMGR_AC_TYPE_AUXILIARY)))) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_GetCurrentSettings( pstBtrMgrAcHdl->hBTRMgrRmfAcHdl,
                                                                    &pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        BTRMGRLOG_DEBUG ("Current CBBufferReady = %p\n", pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.cbBufferReady);
        BTRMGRLOG_DEBUG ("Current Fifosize      = %d\n", (int)pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.fifoSize);
        BTRMGRLOG_DEBUG ("Current Threshold     = %d\n", (int)pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.threshold);

        //TODO: Get the format capture format from RMF_AudioCapture Settings
        apstBtrMgrAcOutASettings->eBtrMgrOutAType     = eBTRMgrATypePCM;

        if (apstBtrMgrAcOutASettings->eBtrMgrOutAType == eBTRMgrATypePCM) {
             stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo);

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.format) {
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

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.samplingFreq) {
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
    }
#endif

#if defined(USE_ACM)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
        lstBtrMgrIarmAcmArgs.session_id = pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl;

        if ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                IARMBUS_AUDIOCAPTUREMGR_GET_AUDIO_PROPS,
                                                (void *)&lstBtrMgrIarmAcmArgs,
                                                sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_GET_AUDIO_PROPS:Return Status = %d\n", leBtrMgIarmAcmRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        if ((leBtrMgrAcRet != eBTRMgrSuccess) || (lstBtrMgrIarmAcmArgs.result != 0)) {
            BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
            leBtrMgrAcRet = eBTRMgrFailure;
        }


        if (leBtrMgrAcRet == eBTRMgrSuccess) {
            memcpy(&pstBtrMgrAcHdl->stBtrMgrAcmCurSettings, &lstBtrMgrIarmAcmArgs.details.arg_audio_properties, sizeof(audio_properties_ifce_t));

            BTRMGRLOG_DEBUG ("Current Fifosize = %d\n", (int)pstBtrMgrAcHdl->stBtrMgrAcmCurSettings.fifo_size);
            BTRMGRLOG_DEBUG ("Current Threshold= %d\n", (int)pstBtrMgrAcHdl->stBtrMgrAcmCurSettings.threshold);
            BTRMGRLOG_DEBUG ("Current DelayComp= %d\n", pstBtrMgrAcHdl->stBtrMgrAcmCurSettings.delay_compensation_ms);
                            
            //TODO: Get the format capture format from IARMBUS_AUDIOCAPTUREMGR_NAME
            apstBtrMgrAcOutASettings->eBtrMgrOutAType     = eBTRMgrATypePCM;

            if (apstBtrMgrAcOutASettings->eBtrMgrOutAType == eBTRMgrATypePCM) {
                 stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo);

                switch (pstBtrMgrAcHdl->stBtrMgrAcmCurSettings.format) {
                case acmFormate16BitStereo:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
                    break;
                case acmFormate24BitStereo:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
                    break;
                case acmFormate16BitMonoLeft:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
                    break;
                case acmFormate16BitMonoRight:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
                    break;
                case acmFormate16BitMono:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanMono;
                    break;
                case acmFormate24Bit5_1:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChan5_1;
                    break;
                case acmFormateMax:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmtUnknown;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanUnknown;
                    break;
                default:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                    pstBtrMgrAcOutPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
                    break;
                }

                switch (pstBtrMgrAcHdl->stBtrMgrAcmCurSettings.sampling_frequency) {
                case acmFreqe16000:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq16K;
                    break;
                case acmFreqe32000:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq32K;
                    break;
                case acmFreqe44100:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq44_1K;
                    break;
                case acmFreqe48000:
                    pstBtrMgrAcOutPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq48K;
                    break;
                case acmFreqeMax:
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
        }
    }
#elif defined(USE_AC_RMF)
    else if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_GetCurrentSettings( pstBtrMgrAcHdl->hBTRMgrRmfAcHdl,
                                                                    &pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        BTRMGRLOG_DEBUG ("Current CBBufferReady = %p\n", pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.cbBufferReady);
        BTRMGRLOG_DEBUG ("Current Fifosize      = %d\n", pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.fifoSize);
        BTRMGRLOG_DEBUG ("Current Threshold     = %d\n", pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.threshold);

        //TODO: Get the format capture format from RMF_AudioCapture Settings
        apstBtrMgrAcOutASettings->eBtrMgrOutAType     = eBTRMgrATypePCM;

        if (apstBtrMgrAcOutASettings->eBtrMgrOutAType == eBTRMgrATypePCM) {
             stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(apstBtrMgrAcOutASettings->pstBtrMgrOutCodecInfo);

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.format) {
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

            switch (pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.samplingFreq) {
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
    tBTRMgrAcHdl                hBTRMgrAcHdl,
    stBTRMgrOutASettings*       apstBtrMgrAcOutASettings,
    fPtr_BTRMgr_AC_DataReadyCb  afpcBBtrMgrAcDataReady,
    fPtr_BTRMgr_AC_StatusCb     afpcBBtrMgrAcStatus,
    void*                       apvUserData
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)hBTRMgrAcHdl;

#if defined(USE_AC_RMF)
    rmf_Error                   leBtrMgrRmfAcRet = RMF_SUCCESS; 
    RMF_AudioCapture_Settings*  pstBtrMgrRmfAcSettings = NULL;
#endif

#if defined(USE_ACM)
    iarmbus_acm_arg_t           lstBtrMgrIarmAcmArgs;
    IARM_Result_t               leBtrMgIarmAcmRet = IARM_RESULT_SUCCESS;
#endif

    if (pstBtrMgrAcHdl == NULL) {
        return eBTRMgrNotInitialized;
    }

    if (apstBtrMgrAcOutASettings == NULL) {
        return eBTRMgrFailInArg;
    }

    pstBtrMgrAcHdl->fpcBBtrMgrAcDataReady       = afpcBBtrMgrAcDataReady;
    pstBtrMgrAcHdl->vpBtrMgrAcDataReadyUserData = apvUserData;

    pstBtrMgrAcHdl->fpcBBtrMgrAcStatus          = afpcBBtrMgrAcStatus;
    pstBtrMgrAcHdl->vpBtrMgrAcStatusUserData    = apvUserData;

#if defined(USE_AC_RMF)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType != NULL) &&
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_AUXILIARY, strlen(BTRMGR_AC_TYPE_AUXILIARY)))) {

        if (pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.fifoSize)
            pstBtrMgrRmfAcSettings = &pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings;
        else
            pstBtrMgrRmfAcSettings = &pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings;


        pstBtrMgrRmfAcSettings->cbBufferReady       = btrMgr_AC_rmfBufferReadyCb;
        pstBtrMgrRmfAcSettings->cbBufferReadyParm   = pstBtrMgrAcHdl;
        pstBtrMgrRmfAcSettings->cbStatusChange      = btrMgr_AC_rmfStatusChangeCb;
        pstBtrMgrRmfAcSettings->cbStatusParm        = pstBtrMgrAcHdl;
        pstBtrMgrRmfAcSettings->fifoSize            = 8 * apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;
        pstBtrMgrRmfAcSettings->threshold           = apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;

  #if defined(TV_CUSTOM_DELAY_COMP)
        //TODO: Work on a intelligent way to arrive at this value. This is not good enough
        if (pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold > 4096) {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 140;
        }
        else if (pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold >= 3584) {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 120;
        }
        else {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 100;
        }

        if ((apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0) && (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0xFFFFu)) {
            // There is no ideal BT device capable of rendering instantaneously, as the external device is
            // specifying delay, deduct from aggregate values of trials of device which donot specify any delay param
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms -= 100;
            // As the external device is specifying delay, add it to total Compensation
            // Device delay is in 1/10 of millisecond
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms += apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay/10;
            // AVDTP 1.3 - 5.5 Delay Reporting
            // The delay reporting feature enables synchronization of audio and video playback by
            // reporting of SNK delay values caused by buffering, decoding and rendering

            if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1500) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 50;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1400) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 40;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1200) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 20;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1000) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 0;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 800) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 20;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 600) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 30;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 400) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 40;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 200) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 60;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 100) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 80;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 50) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 100;
            }
        }
  #else
        //TODO: Work on a intelligent way to arrive at this value. This is not good enough
        if (pstBtrMgrRmfAcSettings->threshold > 4096) {
            pstBtrMgrRmfAcSettings->delayCompensation_ms = 400;
        }
        else if (pstBtrMgrRmfAcSettings->threshold >= 3584) {
            pstBtrMgrRmfAcSettings->delayCompensation_ms = 380;
        }
        else {
            pstBtrMgrRmfAcSettings->delayCompensation_ms = 360;
        }

        if ((apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0) && (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0xFFFFu)) {
            // There is no ideal BT device capable of rendering instantaneously, as the external device is
            // specifying delay, deduct from aggregate values of trials of device which donot specify any delay param
            pstBtrMgrRmfAcSettings->delayCompensation_ms -= 120;
            // As the external device is specifying delay, add it to total Compensation
            // Device delay is in 1/10 of millisecond
            pstBtrMgrRmfAcSettings->delayCompensation_ms += apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay/10;
            // AVDTP 1.3 - 5.5 Delay Reporting
            // The delay reporting feature enables synchronization of audio and video playback by
            // reporting of SNK delay values caused by buffering, decoding and rendering
        }
  #endif
        //TODO: Bad hack above, need to modify before taking it to stable2


        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Start(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl, 
                                                       pstBtrMgrRmfAcSettings)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }
    }
#endif

#if defined(USE_ACM)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
        lstBtrMgrIarmAcmArgs.session_id = pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl;


        if ((pstBtrMgrAcHdl->stBtrMgrAcmCurSettings.fifo_size != 0) && (pstBtrMgrAcHdl->stBtrMgrAcmCurSettings.threshold != 0))
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings = &pstBtrMgrAcHdl->stBtrMgrAcmCurSettings;
        else
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings = &pstBtrMgrAcHdl->stBtrMgrAcmDefSettings;


        pstBtrMgrAcHdl->pstBtrMgrAcmSettings->fifo_size = 8 * apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;
        pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold = apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;

  #if defined(TV_CUSTOM_DELAY_COMP)
        //TODO: Work on a intelligent way to arrive at this value. This is not good enough
        if (pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold > 4096) {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 140;
        }
        else if (pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold >= 3584) {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 120;
        }
        else {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 100;
        }

        if ((apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0) && (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0xFFFFu)) {
            // There is no ideal BT device capable of rendering instantaneously, as the external device is
            // specifying delay, deduct from aggregate values of trials of device which donot specify any delay param
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms -= 100;
            // As the external device is specifying delay, add it to total Compensation
            // Device delay is in 1/10 of millisecond
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms += apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay/10;
            // AVDTP 1.3 - 5.5 Delay Reporting
            // The delay reporting feature enables synchronization of audio and video playback by
            // reporting of SNK delay values caused by buffering, decoding and rendering

            if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1500) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 50;
            }
	    else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1400) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 40;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1200) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 20;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 1000) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  -= 0;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay >= 800) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 20;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 600) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 30;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 400) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 40;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 200) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 60;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 100) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 80;
            }
            else if  (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay > 50) {
                pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms  += 100;
            }
        }
  #else
        //TODO: Work on a intelligent way to arrive at this value. This is not good enough
        if (pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold > 4096) {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 240;
        }
        else if (pstBtrMgrAcHdl->pstBtrMgrAcmSettings->threshold >= 3584) {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 220;
        }
        else {
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms = 200;
        }

        if ((apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0) && (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0xFFFFu)) {
            // There is no ideal BT device capable of rendering instantaneously, as the external device is
            // specifying delay, deduct from aggregate values of trials of device which donot specify any delay param
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms -= 120;
            // As the external device is specifying delay, add it to total Compensation
            // Device delay is in 1/10 of millisecond
            pstBtrMgrAcHdl->pstBtrMgrAcmSettings->delay_compensation_ms += apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay/10;
            // AVDTP 1.3 - 5.5 Delay Reporting
            // The delay reporting feature enables synchronization of audio and video playback by
            // reporting of SNK delay values caused by buffering, decoding and rendering
        }
  #endif
        //TODO: Bad hack above, need to modify before taking it to stable2

        memcpy(&lstBtrMgrIarmAcmArgs.details.arg_audio_properties, pstBtrMgrAcHdl->pstBtrMgrAcmSettings, sizeof(audio_properties_ifce_t));

        if ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                IARMBUS_AUDIOCAPTUREMGR_SET_AUDIO_PROPERTIES,
                                                (void *)&lstBtrMgrIarmAcmArgs,
                                                sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_SET_AUDIO_PROPERTIES:Return Status = %d\n", leBtrMgIarmAcmRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        if ((leBtrMgrAcRet != eBTRMgrSuccess) || (lstBtrMgrIarmAcmArgs.result != 0)) {
            BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
            leBtrMgrAcRet = eBTRMgrFailure;
        }



        memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
        lstBtrMgrIarmAcmArgs.session_id = pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl;

        if ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                IARMBUS_AUDIOCAPTUREMGR_GET_OUTPUT_PROPS,
                                                (void *)&lstBtrMgrIarmAcmArgs,
                                                sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_GET_OUTPUT_PROPS:Return Status = %d\n", leBtrMgIarmAcmRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        if ((leBtrMgrAcRet != eBTRMgrSuccess) || (lstBtrMgrIarmAcmArgs.result != 0)) {
            BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        strncpy(pstBtrMgrAcHdl->pcBtrMgrAcmSockPath, lstBtrMgrIarmAcmArgs.details.arg_output_props.output.file_path,
                strlen(lstBtrMgrIarmAcmArgs.details.arg_output_props.output.file_path) < MAX_OUTPUT_PATH_LEN ?
                        strlen(lstBtrMgrIarmAcmArgs.details.arg_output_props.output.file_path) : MAX_OUTPUT_PATH_LEN - 1);

        BTRMGRLOG_DEBUG ("IARMBUS_AUDIOCAPTUREMGR_GET_OUTPUT_PROPS : pcBtrMgrAcmSockPath = %s\n", pstBtrMgrAcHdl->pcBtrMgrAcmSockPath);


        memset(&lstBtrMgrIarmAcmArgs, 0, sizeof(iarmbus_acm_arg_t));
        lstBtrMgrIarmAcmArgs.session_id = pstBtrMgrAcHdl->hBtrMgrIarmAcmHdl;

        if ((leBtrMgIarmAcmRet = IARM_Bus_Call (IARMBUS_AUDIOCAPTUREMGR_NAME,
                                                IARMBUS_AUDIOCAPTUREMGR_START,
                                                (void *)&lstBtrMgrIarmAcmArgs,
                                                sizeof(lstBtrMgrIarmAcmArgs))) != IARM_RESULT_SUCCESS) {
            BTRMGRLOG_ERROR("IARMBUS_AUDIOCAPTUREMGR_START:Return Status = %d\n", leBtrMgIarmAcmRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        if ((leBtrMgrAcRet != eBTRMgrSuccess) || (lstBtrMgrIarmAcmArgs.result != 0)) {
            BTRMGRLOG_ERROR("lstBtrMgrIarmAcmArgs:Return Status = %d\n", lstBtrMgrIarmAcmArgs.result);
            leBtrMgrAcRet = eBTRMgrFailure;
        }


        if (pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread) {
            gpointer    lpeBtrMgrAcmDCOp = NULL;
            if ((lpeBtrMgrAcmDCOp = g_malloc0(sizeof(eBTRMgrACAcmDCOp))) != NULL) {
                *((eBTRMgrACAcmDCOp*)lpeBtrMgrAcmDCOp) = eBTRMgrACAcmDCStart;
                g_async_queue_push(pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue, lpeBtrMgrAcmDCOp);
                BTRMGRLOG_DEBUG ("g_async_queue_push: eBTRMgrACAcmDCStart\n");
            }
        }
        else {
            BTRMGRLOG_ERROR("pBtrMgrAcmDataCapGThread: eBTRMgrACAcmDCStart - FAILED\n");
            leBtrMgrAcRet = eBTRMgrFailure;
        }
    }
#elif defined(USE_AC_RMF)
    else if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
             (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {

        if (pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings.fifoSize)
            pstBtrMgrRmfAcSettings = &pstBtrMgrAcHdl->stBtrMgrRmfAcCurSettings;
        else
            pstBtrMgrRmfAcSettings = &pstBtrMgrAcHdl->stBtrMgrRmfAcDefSettings;


        pstBtrMgrRmfAcSettings->cbBufferReady       = btrMgr_AC_rmfBufferReadyCb;
        pstBtrMgrRmfAcSettings->cbBufferReadyParm   = pstBtrMgrAcHdl;
        pstBtrMgrRmfAcSettings->cbStatusChange      = NULL;
        pstBtrMgrRmfAcSettings->cbStatusParm        = NULL;
        pstBtrMgrRmfAcSettings->fifoSize            = 8 * apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;
        pstBtrMgrRmfAcSettings->threshold           = apstBtrMgrAcOutASettings->i32BtrMgrOutBufMaxSize;

        //TODO: Work on a intelligent way to arrive at this value. This is not good enough
        if ((apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0) && (apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay != 0xFFFFu))
            pstBtrMgrRmfAcSettings->delayCompensation_ms = apstBtrMgrAcOutASettings->ui32BtrMgrDevDelay;
        else if (pstBtrMgrRmfAcSettings->threshold > 4096)
            pstBtrMgrRmfAcSettings->delayCompensation_ms = 400;
        else
            pstBtrMgrRmfAcSettings->delayCompensation_ms = 360;
        //TODO: Bad hack above, need to modify before taking it to stable2

        BTRMGRLOG_WARN ("Current Fifosize = %d\n", pstBtrMgrRmfAcSettings->fifoSize);
        BTRMGRLOG_WARN ("Current Threshold= %d\n", pstBtrMgrRmfAcSettings->threshold);
        BTRMGRLOG_WARN ("Current DelayComp= %d\n", pstBtrMgrRmfAcSettings->delayCompensation_ms);

        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Start(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl, 
                                                       pstBtrMgrRmfAcSettings)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }
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
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType != NULL) &&
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_AUXILIARY, strlen(BTRMGR_AC_TYPE_AUXILIARY)))) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Stop(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }
    }
#endif

#if defined(USE_ACM)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {

        if (pstBtrMgrAcHdl->pBtrMgrAcmDataCapGThread) {
            gpointer    lpeBtrMgrAcmDCOp = NULL;
            if ((lpeBtrMgrAcmDCOp = g_malloc0(sizeof(eBTRMgrACAcmDCOp))) != NULL) {
                *((eBTRMgrACAcmDCOp*)lpeBtrMgrAcmDCOp) = eBTRMgrACAcmDCStop;
                g_async_queue_push(pstBtrMgrAcHdl->pBtrMgrAcmDataCapGAOpQueue, lpeBtrMgrAcmDCOp);
                BTRMGRLOG_DEBUG ("g_async_queue_push: eBTRMgrACAcmDCStop\n");
            }
        }
        else {
            BTRMGRLOG_ERROR("pBtrMgrAcmDataCapGThread: eBTRMgrACAcmDCStop - FAILED\n");
            leBtrMgrAcRet = eBTRMgrFailure;
        }

        pstBtrMgrAcHdl->fpcBBtrMgrAcDataReady = NULL;
    }
#elif defined(USE_AC_RMF)
    if ((pstBtrMgrAcHdl->pcBTRMgrAcType == NULL) ||
        (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_PRIMARY, strlen(BTRMGR_AC_TYPE_PRIMARY)))) {
        if ((leBtrMgrRmfAcRet = RMF_AudioCapture_Stop(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl)) != RMF_SUCCESS) {
            BTRMGRLOG_ERROR("Return Status = %d\n", leBtrMgrRmfAcRet);
            leBtrMgrAcRet = eBTRMgrFailure;
        }
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


eBTRMgrRet
BTRMgr_AC_Resume (
    tBTRMgrAcHdl hBTRMgrAcHdl
) {
    eBTRMgrRet      leBtrMgrAcRet  = eBTRMgrSuccess;

    return leBtrMgrAcRet;
}

// Outgoing callbacks Registration Interfaces


/* Incoming Callbacks */
#if defined(USE_AC_RMF)
static rmf_Error
btrMgr_AC_rmfBufferReadyCb (
    void*           pContext,
    void*           pInDataBuf,
    unsigned int    inBytesToEncode
) {
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)pContext;

    if (pstBtrMgrAcHdl && pstBtrMgrAcHdl->fpcBBtrMgrAcDataReady) {
        if (pstBtrMgrAcHdl->fpcBBtrMgrAcDataReady(pInDataBuf, inBytesToEncode, pstBtrMgrAcHdl->vpBtrMgrAcDataReadyUserData) != eBTRMgrSuccess) {
            BTRMGRLOG_ERROR("AC Data Ready Callback Failed\n");
        }
    }

    return RMF_SUCCESS;
}


static rmf_Error
btrMgr_AC_rmfStatusChangeCb (
    void*   pContext
) {
    stBTRMgrACHdl*  pstBtrMgrAcHdl = (stBTRMgrACHdl*)pContext;
    bool            bTriggerStatusChanged = false;

    if (pstBtrMgrAcHdl) {
        RMF_AudioCapture_Status    lstBtrMgrRmfAcStatus;
        RMF_AudioCapture_Status*   pstBtrMgrRmfAcStatus = &pstBtrMgrAcHdl->stBtrMgrRmfAcStatus;

        BTRMGRLOG_WARN("Status Changed\n");
        memset(&lstBtrMgrRmfAcStatus, 0, sizeof(RMF_AudioCapture_Status));
        RMF_AudioCapture_GetStatus(pstBtrMgrAcHdl->hBTRMgrRmfAcHdl, &lstBtrMgrRmfAcStatus);

        if (pstBtrMgrRmfAcStatus->started != lstBtrMgrRmfAcStatus.started) {
            BTRMGRLOG_WARN("Status Changed - Started = %d\n", lstBtrMgrRmfAcStatus.started);
            pstBtrMgrRmfAcStatus->started = lstBtrMgrRmfAcStatus.started;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->format != lstBtrMgrRmfAcStatus.format) {
            BTRMGRLOG_WARN("Status Changed - Format = %d\n", lstBtrMgrRmfAcStatus.format);
            pstBtrMgrRmfAcStatus->format = lstBtrMgrRmfAcStatus.format;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->samplingFreq != lstBtrMgrRmfAcStatus.samplingFreq) {
            BTRMGRLOG_WARN("Status Changed - Sampling Freq = %d\n", lstBtrMgrRmfAcStatus.samplingFreq);
            pstBtrMgrRmfAcStatus->samplingFreq = lstBtrMgrRmfAcStatus.samplingFreq;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->fifoDepth != lstBtrMgrRmfAcStatus.fifoDepth) {
            BTRMGRLOG_WARN("Status Changed - Fifo Depth = %d\n", (int)lstBtrMgrRmfAcStatus.fifoDepth);
            pstBtrMgrRmfAcStatus->fifoDepth = lstBtrMgrRmfAcStatus.fifoDepth;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->overflows != lstBtrMgrRmfAcStatus.overflows) {
            BTRMGRLOG_WARN("Status Changed - Overflow = %d\n", lstBtrMgrRmfAcStatus.overflows);
            pstBtrMgrRmfAcStatus->overflows = lstBtrMgrRmfAcStatus.overflows;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->underflows != lstBtrMgrRmfAcStatus.underflows) {
            BTRMGRLOG_WARN("Status Changed - Underflow = %d\n", lstBtrMgrRmfAcStatus.underflows);
            pstBtrMgrRmfAcStatus->underflows = lstBtrMgrRmfAcStatus.underflows;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->muted != lstBtrMgrRmfAcStatus.muted) {
            BTRMGRLOG_WARN("Status Changed - Muted = %d\n", lstBtrMgrRmfAcStatus.muted);
            pstBtrMgrRmfAcStatus->muted = lstBtrMgrRmfAcStatus.muted;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->paused != lstBtrMgrRmfAcStatus.paused) {
            BTRMGRLOG_WARN("Status Changed - Paused = %d\n", lstBtrMgrRmfAcStatus.paused);
            pstBtrMgrRmfAcStatus->paused = lstBtrMgrRmfAcStatus.paused;
            bTriggerStatusChanged = true;
        }

        if (pstBtrMgrRmfAcStatus->volume != lstBtrMgrRmfAcStatus.volume) {
            BTRMGRLOG_WARN("Status Changed - Volume = %f\n", lstBtrMgrRmfAcStatus.volume);
            pstBtrMgrRmfAcStatus->volume = lstBtrMgrRmfAcStatus.volume;
            bTriggerStatusChanged = true;
        }

        if (bTriggerStatusChanged) {
            stBTRMgrMediaStatus     lstBtrMgrAcMediaStatus;
            stBTRMgrMediaStatus*    pstBtrMgrAcMediaStatus = &pstBtrMgrAcHdl->stBtrMgrAcStatus;

            if (pstBtrMgrRmfAcStatus->paused) {
                pstBtrMgrAcMediaStatus->eBtrMgrState = eBTRMgrStatePaused;

                if ((pstBtrMgrAcHdl->pcBTRMgrAcType != NULL) &&
                    (!strncmp(pstBtrMgrAcHdl->pcBTRMgrAcType, BTRMGR_AC_TYPE_AUXILIARY, strlen(BTRMGR_AC_TYPE_AUXILIARY)))) {
                }
            }
            else {
                pstBtrMgrAcMediaStatus->eBtrMgrState = eBTRMgrStatePlaying;
            }

            switch (pstBtrMgrRmfAcStatus->format) {
            case racFormat_e16BitStereo:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChanStereo;
                break;
            case racFormat_e24BitStereo:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChanStereo;
                break;
            case racFormat_e16BitMonoLeft:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChanMono;
                break;
            case racFormat_e16BitMonoRight:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChanMono;
                break;
            case racFormat_e16BitMono:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChanMono;
                break;
            case racFormat_e24Bit5_1:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmt24bit;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChan5_1;
                break;
            case racFormat_eMax:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmtUnknown;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChanUnknown;
                break;
            default:
                pstBtrMgrAcMediaStatus->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
                pstBtrMgrAcMediaStatus->eBtrMgrAChan = eBTRMgrAChanStereo;
                break;
            }

            switch (pstBtrMgrRmfAcStatus->samplingFreq) {
            case racFreq_e16000:
                pstBtrMgrAcMediaStatus->eBtrMgrSFreq = eBTRMgrSFreq16K;
                break;
            case racFreq_e32000:
                pstBtrMgrAcMediaStatus->eBtrMgrSFreq = eBTRMgrSFreq32K;
                break;
            case racFreq_e44100:
                pstBtrMgrAcMediaStatus->eBtrMgrSFreq = eBTRMgrSFreq44_1K;
                break;
            case racFreq_e48000:
                pstBtrMgrAcMediaStatus->eBtrMgrSFreq = eBTRMgrSFreq48K;
                break;
            case racFreq_eMax:
                pstBtrMgrAcMediaStatus->eBtrMgrSFreq = eBTRMgrSFreqUnknown;
                break;
            default:
                pstBtrMgrAcMediaStatus->eBtrMgrSFreq = eBTRMgrSFreq48K;
                break;
            }

            pstBtrMgrAcMediaStatus->ui32OverFlowCnt = pstBtrMgrRmfAcStatus->overflows;
            pstBtrMgrAcMediaStatus->ui32UnderFlowCnt= pstBtrMgrRmfAcStatus->underflows;

            if (!pstBtrMgrRmfAcStatus->muted) {
                pstBtrMgrAcMediaStatus->ui8Volume       = (unsigned char) (255 * pstBtrMgrRmfAcStatus->volume);
            }
            else {
                pstBtrMgrAcMediaStatus->ui8Volume       = 0;
            }

            memcpy (&lstBtrMgrAcMediaStatus, pstBtrMgrAcMediaStatus, sizeof(stBTRMgrMediaStatus));

            if (pstBtrMgrAcHdl->fpcBBtrMgrAcStatus) {
                if (pstBtrMgrAcHdl->fpcBBtrMgrAcStatus(&lstBtrMgrAcMediaStatus, pstBtrMgrAcHdl->vpBtrMgrAcStatusUserData) != eBTRMgrSuccess) {
                    BTRMGRLOG_ERROR("AC Status Callback Failed\n");
                }
            }

        }
    }

    return RMF_SUCCESS;
}
#endif

