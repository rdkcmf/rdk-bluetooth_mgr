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
 * @file btrMgr_streamOutGst.c
 *
 * @description This file implements bluetooth manager's GStreamer streaming interface to external BT devices
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


/* System Headers */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* Ext lib Headers */
#include <gst/gst.h>
#include <gst/audio/audio.h>
#include <gst/app/gstappsrc.h>


/* Interface lib Headers */
#include "btrMgr_logger.h"         //for rdklogger

/* Local Headers */
#include "btrMgr_streamOutGst.h"


/* Local defines */
#define BTRMGR_SLEEP_TIMEOUT_MS            1   // Suspend execution of thread. Keep as minimal as possible
#define BTRMGR_WAIT_TIMEOUT_MS             2   // Use for blocking operations
#define BTRMGR_MAX_INTERNAL_QUEUE_ELEMENTS 8   // Number of blocks in the internal queue

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5

#define ENABLE_MAIN_LOOP_CONTEXT 0


/* Local Types & Typedefs */
typedef struct _stBTRMgrSOGst {
    void*        pPipeline;
    void*        pSrc;
    void*        pSink;
    void*        pAudioResample;
    void*        pAudioEnc;
    void*        pAECapsFilter;
    void*        pRtpAudioPay;
    void*        pVolume;

#if !(ENABLE_MAIN_LOOP_CONTEXT)
    void*        pContext;
    GMutex       gMtxMainLoopRunLock;
    GCond        gCndMainLoopRun;
#endif

    void*        pLoop;
    void*        pLoopThread;
    guint        busWId;

    GstClockTime gstClkTStamp;
    guint64      inBufOffset;
    int          i32InBufMaxSize;
    int          i32InRate;
    int          i32InChannels;
    unsigned int ui32InBitsPSample;

    fPtr_BTRMgr_SO_GstStatusCb  fpcBSoGstStatus;
    void*                       pvcBUserData;

    GMutex       pipelineDataMutex;
    gboolean     bPipelineError;
} stBTRMgrSOGst;


/* Static Function Prototypes */
static GstState btrMgr_SO_validateStateWithTimeout (GstElement* element, GstState stateToValidate, guint msTimeOut);

/* Local Op Threads Prototypes */
static gpointer btrMgr_SO_g_main_loop_Task (gpointer apstBtrMgrSoGst);

/* Incoming Callbacks Prototypes */
#if !(ENABLE_MAIN_LOOP_CONTEXT)
static gboolean btrMgr_SO_g_main_loop_RunningCb (gpointer apstBtrMgrSoGst); 
#endif
static void btrMgr_SO_NeedDataCb (GstElement* appsrc, guint size, gpointer apstBtrMgrSoGst);
static void btrMgr_SO_EnoughDataCb (GstElement* appsrc, gpointer apstBtrMgrSoGst);
static gboolean btrMgr_SO_gstBusCallCb (GstBus* bus, GstMessage* msg, gpointer apstBtrMgrSoGst);


/* Static Function Definition */
static GstState 
btrMgr_SO_validateStateWithTimeout (
    GstElement* element,
    GstState    stateToValidate,
    guint       msTimeOut
) {
    GstState    gst_current;
    GstState    gst_pending;
    float       timeout = BTRMGR_WAIT_TIMEOUT_MS;
    gint        gstGetStateCnt = GST_ELEMENT_GET_STATE_RETRY_CNT_MAX;

    do { 
        if ((GST_STATE_CHANGE_SUCCESS == gst_element_get_state(GST_ELEMENT(element), &gst_current, &gst_pending, timeout * GST_MSECOND)) && (gst_current == stateToValidate)) {
            BTRMGRLOG_INFO("gst_element_get_state - SUCCESS : State = %d, Pending = %d\n", gst_current, gst_pending);
            return gst_current;
        }
        usleep(msTimeOut * 1000); // Let element safely transition to required state 
    } while ((gst_current != stateToValidate) && (gstGetStateCnt-- != 0)) ;

    BTRMGRLOG_ERROR("gst_element_get_state - FAILURE : State = %d, Pending = %d\n", gst_current, gst_pending);

    if (gst_pending == stateToValidate)
        return gst_pending;
    else
        return gst_current;
}


/* Local Op Threads */
static gpointer
btrMgr_SO_g_main_loop_Task (
    gpointer apstBtrMgrSoGst
) {
    stBTRMgrSOGst*  pstBtrMgrSoGst = (stBTRMgrSOGst*)apstBtrMgrSoGst;

    if (!pstBtrMgrSoGst || !pstBtrMgrSoGst->pLoop) {
        BTRMGRLOG_ERROR ("GMainLoop Error - In arguments Exiting\n");
        return NULL;
    }

#if !(ENABLE_MAIN_LOOP_CONTEXT)
    GstElement*     appsrc      = NULL;
    GstElement*     volume      = NULL;
    GstElement*     audresample = NULL;
    GstElement*     audenc      = NULL;
    GstElement*     aecapsfilter= NULL;
    GstElement*     rtpaudpay   = NULL;
    GstElement*     fdsink      = NULL;
    GstElement*     pipeline    = NULL;
    GstBus*         bus         = NULL;
    GSource*        idleSource  = NULL;
    GMainLoop*      loop        = NULL;    
    GMainContext*   mainContext = NULL;
    guint           busWatchId;


    if (!pstBtrMgrSoGst->pContext) {
        BTRMGRLOG_ERROR ("GMainLoop Error - No context\n");
        return NULL;
    }
    
    mainContext = pstBtrMgrSoGst->pContext;
    g_main_context_push_thread_default (mainContext);

    idleSource = g_idle_source_new ();
    g_source_set_callback (idleSource, (GSourceFunc) btrMgr_SO_g_main_loop_RunningCb, pstBtrMgrSoGst, NULL);
    g_source_attach (idleSource, mainContext);
    g_source_unref (idleSource);


    /* Create elements */
    appsrc      = gst_element_factory_make ("appsrc", "btmgr-so-appsrc");
    volume      = gst_element_factory_make ("volume", "btmgr-so-volume");
#if defined(DISABLE_AUDIO_ENCODING)
    audresample = gst_element_factory_make ("queue", "btmgr-so-aresample");
    audenc      = gst_element_factory_make ("queue", "btmgr-so-sbcenc");
    aecapsfilter= gst_element_factory_make ("queue", "btmgr-so-aecapsfilter");
    rtpaudpay   = gst_element_factory_make ("queue", "btmgr-so-rtpsbcpay");
#else
	/*TODO: Select the Audio Codec and RTP Audio Payloader based on input*/
    audresample = gst_element_factory_make ("audioresample", "btmgr-so-aresample");
    audenc      = gst_element_factory_make ("sbcenc", "btmgr-so-sbcenc");
    aecapsfilter= gst_element_factory_make ("capsfilter", "btmgr-so-aecapsfilter");
    rtpaudpay   = gst_element_factory_make ("rtpsbcpay", "btmgr-so-rtpsbcpay");
#endif
    fdsink      = gst_element_factory_make ("fdsink", "btmgr-so-fdsink");


    /* Create a new pipeline to hold the elements */
    pipeline    = gst_pipeline_new ("btmgr-so-pipeline");

    loop =  pstBtrMgrSoGst->pLoop;

    if (!appsrc || !volume || !audenc || !aecapsfilter || !rtpaudpay || !fdsink || !loop || !pipeline) {
        BTRMGRLOG_ERROR ("Gstreamer plugin missing for streamOut\n");
        return NULL;
    }

    pstBtrMgrSoGst->pPipeline       = (void*)pipeline;
    pstBtrMgrSoGst->pSrc            = (void*)appsrc;
    pstBtrMgrSoGst->pVolume         = (void*)volume;
    pstBtrMgrSoGst->pSink           = (void*)fdsink;
    pstBtrMgrSoGst->pAudioResample  = (void*)audresample;
    pstBtrMgrSoGst->pAudioEnc       = (void*)audenc;
    pstBtrMgrSoGst->pAECapsFilter   = (void*)aecapsfilter;
    pstBtrMgrSoGst->pRtpAudioPay    = (void*)rtpaudpay;
    pstBtrMgrSoGst->gstClkTStamp    = 0;
    pstBtrMgrSoGst->inBufOffset     = 0;
    pstBtrMgrSoGst->bPipelineError  = FALSE;
    g_mutex_init(&pstBtrMgrSoGst->pipelineDataMutex);


    bus                     = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId              = gst_bus_add_watch(bus, btrMgr_SO_gstBusCallCb, pstBtrMgrSoGst);
    pstBtrMgrSoGst->busWId  = busWatchId;
    g_object_unref(bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), appsrc, volume, audenc, aecapsfilter, rtpaudpay, fdsink, NULL);
    gst_element_link_many (appsrc, volume, audenc, aecapsfilter, rtpaudpay, fdsink, NULL);

    g_signal_connect (appsrc, "need-data", G_CALLBACK(btrMgr_SO_NeedDataCb), pstBtrMgrSoGst);
    g_signal_connect (appsrc, "enough-data", G_CALLBACK(btrMgr_SO_EnoughDataCb), pstBtrMgrSoGst);

#endif


    BTRMGRLOG_INFO ("GMainLoop Running\n");
    g_main_loop_run (pstBtrMgrSoGst->pLoop);


#if !(ENABLE_MAIN_LOOP_CONTEXT)
    if (busWatchId) {
        GSource *busSource = g_main_context_find_source_by_id(mainContext, busWatchId);
        if (busSource)
            g_source_destroy(busSource);

        busWatchId = 0;
        pstBtrMgrSoGst->busWId = busWatchId;
    }

 
    g_main_context_pop_thread_default (mainContext);
#endif


    BTRMGRLOG_INFO ("GMainLoop Exiting\n");
    return pstBtrMgrSoGst;
}


/* Interfaces */
eBTRMgrSOGstRet
BTRMgr_SO_GstInit (
    tBTRMgrSoGstHdl*            phBTRMgrSoGstHdl,
    fPtr_BTRMgr_SO_GstStatusCb  afpcBSoGstStatus,
    void*                       apvUserData
) {
#if (ENABLE_MAIN_LOOP_CONTEXT)
    GstElement*     appsrc          = NULL;
    GstElement*     volume          = NULL;
    GstElement*     audresample     = NULL;
    GstElement*     audenc          = NULL;
    GstElement*     aecapsfilter    = NULL;
    GstElement*     rtpaudpay       = NULL;
    GstElement*     fdsink          = NULL;
    GstElement*     pipeline        = NULL;
    GstBus*         bus             = NULL;
    guint           busWatchId;
#else
    GMainContext*   mainContext     = NULL;
#endif

    stBTRMgrSOGst*  pstBtrMgrSoGst  = NULL;
    GThread*        mainLoopThread  = NULL;
    GMainLoop*      loop            = NULL;    


    if ((pstBtrMgrSoGst = (stBTRMgrSOGst*)malloc (sizeof(stBTRMgrSOGst))) == NULL) {
        BTRMGRLOG_ERROR ("Unable to allocate memory\n");
        return eBTRMgrSOGstFailure;
    }

    memset((void*)pstBtrMgrSoGst, 0, sizeof(stBTRMgrSOGst));

    gst_init (NULL, NULL);

#if (ENABLE_MAIN_LOOP_CONTEXT)
    /* Create elements */
    appsrc      = gst_element_factory_make ("appsrc", "btmgr-so-appsrc");
    volume      = gst_element_factory_make ("volume", "btmgr-so-volume");
#if defined(DISABLE_AUDIO_ENCODING)
    audresample = gst_element_factory_make ("queue", "btmgr-so-aresample");
    audenc      = gst_element_factory_make ("queue", "btmgr-so-sbcenc");
    aecapsfilter= gst_element_factory_make ("queue", "btmgr-so-aecapsfilter");
    rtpaudpay   = gst_element_factory_make ("queue", "btmgr-so-rtpsbcpay");
#else
	/*TODO: Select the Audio Codec and RTP Audio Payloader based on input*/
    audresample = gst_element_factory_make ("audioresample", "btmgr-so-aresample");
    audenc      = gst_element_factory_make ("sbcenc", "btmgr-so-sbcenc");
    aecapsfilter= gst_element_factory_make ("capsfilter", "btmgr-so-aecapsfilter");
    rtpaudpay   = gst_element_factory_make ("rtpsbcpay", "btmgr-so-rtpsbcpay");
#endif
    fdsink      = gst_element_factory_make ("fdsink", "btmgr-so-fdsink");

    /* Create and event loop and feed gstreamer bus mesages to it */
    loop = g_main_loop_new (NULL, FALSE);
#else
    g_mutex_init (&pstBtrMgrSoGst->gMtxMainLoopRunLock);
    g_cond_init (&pstBtrMgrSoGst->gCndMainLoopRun);

    mainContext = g_main_context_new();

    /* Create and event loop and feed gstreamer bus mesages to it */
    if (mainContext) {
        loop = g_main_loop_new (mainContext, FALSE);
    }
#endif


#if (ENABLE_MAIN_LOOP_CONTEXT)
    /* Create a new pipeline to hold the elements */
    pipeline = gst_pipeline_new ("btmgr-so-pipeline");

    if (!appsrc || !volume || !audenc || !aecapsfilter || !rtpaudpay || !fdsink || !loop || !pipeline) {
        BTRMGRLOG_ERROR ("Gstreamer plugin missing for streamOut\n");
        //TODO: Call BTRMgr_SO_GstDeInit((tBTRMgrSoGstHdl)pstBtrMgrSoGst);
        free((void*)pstBtrMgrSoGst);
        return eBTRMgrSOGstFailure;
    }

    pstBtrMgrSoGst->pPipeline       = (void*)pipeline;
    pstBtrMgrSoGst->pSrc            = (void*)appsrc;
    pstBtrMgrSoGst->pVolume         = (void*)volume;
    pstBtrMgrSoGst->pSink           = (void*)fdsink;
    pstBtrMgrSoGst->pAudioResample  = (void*)audresample;
    pstBtrMgrSoGst->pAudioEnc       = (void*)audenc;
    pstBtrMgrSoGst->pAECapsFilter   = (void*)aecapsfilter;
    pstBtrMgrSoGst->pRtpAudioPay    = (void*)rtpaudpay;
    pstBtrMgrSoGst->pLoop           = (void*)loop;
    pstBtrMgrSoGst->gstClkTStamp    = 0;
    pstBtrMgrSoGst->inBufOffset     = 0;
    pstBtrMgrSoGst->fpcBSoGstStatus = NULL;
    pstBtrMgrSoGst->pvcBUserData    = NULL;
    pstBtrMgrSoGst->bPipelineError  = FALSE;
    g_mutex_init(&pstBtrMgrSoGst->pipelineDataMutex);


    if (afpcBSoGstStatus) {
        pstBtrMgrSoGst->fpcBSoGstStatus = afpcBSoGstStatus;
        pstBtrMgrSoGst->pvcBUserData    = apvUserData;
    }


    bus                     = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId              = gst_bus_add_watch(bus, btrMgr_SO_gstBusCallCb, pstBtrMgrSoGst);
    pstBtrMgrSoGst->busWId  = busWatchId;
    g_object_unref(bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), appsrc, volume, audenc, aecapsfilter, rtpaudpay, fdsink, NULL);
    gst_element_link_many (appsrc, volume, audenc, aecapsfilter, rtpaudpay, fdsink, NULL);


    mainLoopThread = g_thread_new("btrMgr_SO_g_main_loop_Task", btrMgr_SO_g_main_loop_Task, pstBtrMgrSoGst);
    pstBtrMgrSoGst->pLoopThread = (void*)mainLoopThread;

    g_signal_connect (appsrc, "need-data", G_CALLBACK(btrMgr_SO_NeedDataCb), pstBtrMgrSoGst);
    g_signal_connect (appsrc, "enough-data", G_CALLBACK(btrMgr_SO_EnoughDataCb), pstBtrMgrSoGst);
        
#else
    pstBtrMgrSoGst->pContext        = (void*)mainContext;
    pstBtrMgrSoGst->pLoop           = (void*)loop;
    pstBtrMgrSoGst->fpcBSoGstStatus = NULL;
    pstBtrMgrSoGst->pvcBUserData    = NULL;

    if (afpcBSoGstStatus) {
        pstBtrMgrSoGst->fpcBSoGstStatus = afpcBSoGstStatus;
        pstBtrMgrSoGst->pvcBUserData    = apvUserData;
    }


    g_mutex_lock (&pstBtrMgrSoGst->gMtxMainLoopRunLock);
    {
        mainLoopThread = g_thread_new("btrMgr_SO_g_main_loop_Task", btrMgr_SO_g_main_loop_Task, pstBtrMgrSoGst);
        pstBtrMgrSoGst->pLoopThread = (void*)mainLoopThread;
        
        //TODO: Infinite loop, break out of it gracefully in case of failures
        while (!loop || !g_main_loop_is_running (loop)) {
            g_cond_wait (&pstBtrMgrSoGst->gCndMainLoopRun, &pstBtrMgrSoGst->gMtxMainLoopRunLock);
        }
    }
    g_mutex_unlock (&pstBtrMgrSoGst->gMtxMainLoopRunLock);

#endif
        
    gst_element_set_state(GST_ELEMENT(pstBtrMgrSoGst->pPipeline), GST_STATE_NULL);
    if (btrMgr_SO_validateStateWithTimeout(pstBtrMgrSoGst->pPipeline, GST_STATE_NULL, BTRMGR_SLEEP_TIMEOUT_MS)!= GST_STATE_NULL) {
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        BTRMgr_SO_GstDeInit((tBTRMgrSoGstHdl)pstBtrMgrSoGst);
        return eBTRMgrSOGstFailure;
    }

    
    *phBTRMgrSoGstHdl = (tBTRMgrSoGstHdl)pstBtrMgrSoGst;

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstDeInit (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst* pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement*     pipeline        = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GMainLoop*      loop            = (GMainLoop*)pstBtrMgrSoGst->pLoop;
    GThread*        mainLoopThread  = (GThread*)pstBtrMgrSoGst->pLoopThread;
#if (ENABLE_MAIN_LOOP_CONTEXT)
    guint           busWatchId      = pstBtrMgrSoGst->busWId;
#else
    GMainContext*   mainContext     = (GMainContext*)pstBtrMgrSoGst->pContext;
#endif

    /* cleanup */
    if (pipeline) {
        gst_object_unref(GST_OBJECT(pipeline));
        pipeline = NULL;
    }

#if (ENABLE_MAIN_LOOP_CONTEXT)
    if (busWatchId) {
        GSource *busSource = g_main_context_find_source_by_id(g_main_context_get_thread_default(), busWatchId);
        if (busSource)
            g_source_destroy(busSource);

        busWatchId = 0;
    }
#endif

    if (loop) {
        g_main_loop_quit(loop);
    }

    if (mainLoopThread) {
        g_thread_join(mainLoopThread);
        mainLoopThread = NULL;
    }

    if (loop) {
        g_main_loop_unref(loop);
        loop = NULL;
    }

#if !(ENABLE_MAIN_LOOP_CONTEXT)
    if (mainContext) {
        g_main_context_unref(mainContext);
        mainContext = NULL;
    }
#endif

    if (pstBtrMgrSoGst->fpcBSoGstStatus)
        pstBtrMgrSoGst->fpcBSoGstStatus = NULL;


    g_mutex_lock(&pstBtrMgrSoGst->pipelineDataMutex);
    pstBtrMgrSoGst->bPipelineError  = FALSE;
    g_mutex_unlock(&pstBtrMgrSoGst->pipelineDataMutex);

    g_mutex_clear(&pstBtrMgrSoGst->pipelineDataMutex);


#if !(ENABLE_MAIN_LOOP_CONTEXT)
    g_cond_clear(&pstBtrMgrSoGst->gCndMainLoopRun);
    g_mutex_clear(&pstBtrMgrSoGst->gMtxMainLoopRunLock);
#endif


    memset((void*)pstBtrMgrSoGst, 0, sizeof(stBTRMgrSOGst));
    free((void*)pstBtrMgrSoGst);
    pstBtrMgrSoGst = NULL;

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstStart (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl,
    int             ai32InBufMaxSize,
    const char*     apcInFmt,
    int             ai32InRate,
    int             ai32InChannels,
    int             ai32OutRate,
    int             ai32OutChannels,
    const char*     apcOutChannelMode,
    unsigned char   aui8SbcAllocMethod,
    unsigned char   aui8SbcSubbands,
    unsigned char   aui8SbcBlockLength,
    unsigned char   aui8SbcMinBitpool,
    unsigned char   aui8SbcMaxBitpool,
    int             ai32BTDevFd,
    int             ai32BTDevMTU
) {
    stBTRMgrSOGst* pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst || !apcInFmt) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSoGst->pSrc;
    GstElement* volume      = (GstElement*)pstBtrMgrSoGst->pVolume;
    GstElement* fdsink      = (GstElement*)pstBtrMgrSoGst->pSink;
    GstElement* audresample = (GstElement*)pstBtrMgrSoGst->pAudioResample;
    GstElement* audenc      = (GstElement*)pstBtrMgrSoGst->pAudioEnc;
    GstElement* aecapsfilter= (GstElement*)pstBtrMgrSoGst->pAECapsFilter;
    GstElement* rtpaudpay   = (GstElement*)pstBtrMgrSoGst->pRtpAudioPay;
    guint       busWatchId  = pstBtrMgrSoGst->busWId;

    GstCaps* appsrcSrcCaps  = NULL;
    GstCaps* audEncSrcCaps  = NULL;

    unsigned int lui32InBitsPSample = 0;

    (void)pipeline;
    (void)appsrc;
    (void)volume;
    (void)audenc;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        BTRMGRLOG_ERROR ("Incorrect State to perform Operation\n");
        return eBTRMgrSOGstFailure;
    }

    if ((ai32InRate != ai32OutRate) || (ai32InChannels != ai32OutChannels)) {
        //TODO: Audio resampling from 48KHz to 44.1KHz results in some timestamp issues
        //      and discontinuties which we need to fix
        gst_bin_add(GST_BIN(pipeline), audresample);
        gst_element_unlink(volume, audenc);
        gst_element_link_many (volume, audresample, audenc, NULL);
    }

    pstBtrMgrSoGst->gstClkTStamp = 0;
    pstBtrMgrSoGst->inBufOffset  = 0;

    if (strcmp(apcInFmt, BTRMGR_AUDIO_SFMT_SIGNED_8BIT))
        lui32InBitsPSample = 8;
    else if (strcmp(apcInFmt, BTRMGR_AUDIO_SFMT_SIGNED_LE_16BIT))
        lui32InBitsPSample = 16;
    else if (strcmp(apcInFmt, BTRMGR_AUDIO_SFMT_SIGNED_LE_24BIT))
        lui32InBitsPSample = 24;
    else if (strcmp(apcInFmt, BTRMGR_AUDIO_SFMT_SIGNED_LE_32BIT))
        lui32InBitsPSample = 32;
    else
        lui32InBitsPSample = 16;


    /*TODO: Set the caps dynamically based on input arguments to Start */
    appsrcSrcCaps = gst_caps_new_simple ("audio/x-raw",
                                         "format", G_TYPE_STRING, apcInFmt,
                                         "layout", G_TYPE_STRING, "interleaved",
                                         "rate", G_TYPE_INT, ai32InRate,
                                         "channels", G_TYPE_INT, ai32InChannels,
                                          NULL);

    (void)aui8SbcAllocMethod;
    (void)aui8SbcMinBitpool;

    /*TODO: Set the Encoder ouput caps dynamically based on input arguments to Start */
    audEncSrcCaps = gst_caps_new_simple ("audio/x-sbc",
                                         "rate", G_TYPE_INT, ai32OutRate,
                                         "channels", G_TYPE_INT, ai32OutChannels,
                                         "channel-mode", G_TYPE_STRING, apcOutChannelMode,
                                         "blocks", G_TYPE_INT, aui8SbcBlockLength,
                                         "subbands", G_TYPE_INT, aui8SbcSubbands,
                                         "allocation-method", G_TYPE_STRING, "loudness",
                                         "bitpool", G_TYPE_INT, aui8SbcMaxBitpool,
                                          NULL);

    g_object_set (appsrc, "caps", appsrcSrcCaps, NULL);
    g_object_set (appsrc, "blocksize", ai32InBufMaxSize, NULL);

    g_object_set (appsrc, "max-bytes", BTRMGR_MAX_INTERNAL_QUEUE_ELEMENTS * ai32InBufMaxSize, NULL);
    g_object_set (appsrc, "is-live", 1, NULL);
    g_object_set (appsrc, "block", 1, NULL);

#if 0
    g_object_set (appsrc, "do-timestamp", 1, NULL);
#endif
    g_object_set (appsrc, "min-percent", 16, NULL);
    g_object_set (appsrc, "min-latency", GST_USECOND * (ai32InBufMaxSize * 1000)/((ai32InRate/1000.0) * (lui32InBitsPSample/8) * ai32InChannels), NULL);
    g_object_set (appsrc, "stream-type", GST_APP_STREAM_TYPE_STREAM, NULL);
    g_object_set (appsrc, "format", GST_FORMAT_TIME, NULL);

    g_object_set (audenc, "perfect-timestamp", 1, NULL);

    g_object_set (aecapsfilter, "caps", audEncSrcCaps, NULL);

    g_object_set (rtpaudpay, "mtu", ai32BTDevMTU, NULL);
    g_object_set (rtpaudpay, "min-frames", -1, NULL);
    g_object_set (rtpaudpay, "perfect-rtptime", 1, NULL);

    g_object_set (fdsink, "fd", ai32BTDevFd, NULL);
    g_object_set (fdsink, "sync", FALSE, NULL);


    gst_caps_unref(audEncSrcCaps);
    gst_caps_unref(appsrcSrcCaps);


    g_mutex_lock(&pstBtrMgrSoGst->pipelineDataMutex);
    pstBtrMgrSoGst->bPipelineError  = FALSE;
    g_mutex_unlock(&pstBtrMgrSoGst->pipelineDataMutex);


    /* start play back and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) { 
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        return eBTRMgrSOGstFailure;
    }

    pstBtrMgrSoGst->i32InBufMaxSize     = ai32InBufMaxSize;
    pstBtrMgrSoGst->i32InRate           = ai32InRate;
    pstBtrMgrSoGst->i32InChannels       = ai32InChannels;
    pstBtrMgrSoGst->ui32InBitsPSample   = lui32InBitsPSample;

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstStop (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst* pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSoGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSoGst->pLoop;
    guint       busWatchId  = pstBtrMgrSoGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;


    g_mutex_lock(&pstBtrMgrSoGst->pipelineDataMutex);
    pstBtrMgrSoGst->bPipelineError  = FALSE;
    g_mutex_unlock(&pstBtrMgrSoGst->pipelineDataMutex);


    /* stop play back */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        BTRMGRLOG_ERROR ("- Unable to perform Operation\n");
        return eBTRMgrSOGstFailure;
    }

    pstBtrMgrSoGst->gstClkTStamp = 0;
    pstBtrMgrSoGst->inBufOffset  = 0;

    pstBtrMgrSoGst->i32InBufMaxSize     = 0;
    pstBtrMgrSoGst->i32InRate           = 0;
    pstBtrMgrSoGst->i32InChannels       = 0;
    pstBtrMgrSoGst->ui32InBitsPSample   = 0;


    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstPause (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst* pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSoGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSoGst->pLoop;
    guint       busWatchId  = pstBtrMgrSoGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        BTRMGRLOG_ERROR ("Incorrect State to perform Operation\n");
        return eBTRMgrSOGstFailure;
    }

    /* pause playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PAUSED);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        return eBTRMgrSOGstFailure;
    } 

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstResume (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst* pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSoGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSoGst->pLoop;
    guint       busWatchId  = pstBtrMgrSoGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        BTRMGRLOG_ERROR ("Incorrect State to perform Operation\n");
        return eBTRMgrSOGstFailure;
    }

    /* Resume playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        return eBTRMgrSOGstFailure;
    }

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstSetVolume (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl,
    unsigned char   ui8Volume
) {
    stBTRMgrSOGst* pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* volume      = (GstElement*)pstBtrMgrSoGst->pVolume;

    (void)pipeline;


    BTRMGRLOG_DEBUG("Volume at StreamOut Gst = %d, %f\n", ui8Volume, ui8Volume/255.0);
    g_object_set (volume, "volume", (double)(ui8Volume/255.0),  NULL);

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstSendBuffer (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl,
    char*           pcInBuf,
    int             aiInBufSize
) {
    stBTRMgrSOGst*  pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;
    int             i32InBufOffset = 0;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSoGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSoGst->pLoop;
    guint       busWatchId  = pstBtrMgrSoGst->busWId;

    gboolean    lbPipelineEr = FALSE;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;


    g_mutex_lock(&pstBtrMgrSoGst->pipelineDataMutex);
    lbPipelineEr = pstBtrMgrSoGst->bPipelineError;
    g_mutex_unlock(&pstBtrMgrSoGst->pipelineDataMutex);


    if (lbPipelineEr == TRUE) {
#if 1
        return eBTRMgrSOGstSuccess;
#else
        //TODO: This is the correct code. But we dont have 
        //      an upper layer, which can call DeInit to free the 
        //      the memory effectively without causing a deadlock 
        //      at this moment.
        return eBTRMgrSOGstFailure;
#endif
    }


    /* push Buffers */
    while (aiInBufSize > pstBtrMgrSoGst->i32InBufMaxSize) {
        GstBuffer*  gstBuf;
        GstMapInfo  gstBufMap;

        gstBuf = gst_buffer_new_and_alloc (pstBtrMgrSoGst->i32InBufMaxSize);
        gst_buffer_set_size (gstBuf, pstBtrMgrSoGst->i32InBufMaxSize);

        gst_buffer_map (gstBuf, &gstBufMap, GST_MAP_WRITE);

        //TODO: Repalce memcpy and new_alloc if possible
        memcpy (gstBufMap.data, pcInBuf + i32InBufOffset, pstBtrMgrSoGst->i32InBufMaxSize);

        //TODO: Arrive at this vale based on Sampling rate, bits per sample, num of Channels and the 
        // size of the incoming buffer (which represents the num of samples received at this instant)
        GST_BUFFER_PTS (gstBuf)         = pstBtrMgrSoGst->gstClkTStamp;
        GST_BUFFER_DURATION (gstBuf)    = GST_USECOND * (pstBtrMgrSoGst->i32InBufMaxSize * 1000)/((pstBtrMgrSoGst->i32InRate/1000.0) * (pstBtrMgrSoGst->ui32InBitsPSample/8) * pstBtrMgrSoGst->i32InChannels);
        pstBtrMgrSoGst->gstClkTStamp   += GST_BUFFER_DURATION (gstBuf);

        GST_BUFFER_OFFSET (gstBuf)      = pstBtrMgrSoGst->inBufOffset;
        pstBtrMgrSoGst->inBufOffset    += pstBtrMgrSoGst->i32InBufMaxSize;
        GST_BUFFER_OFFSET_END (gstBuf)  = pstBtrMgrSoGst->inBufOffset - 1;

        gst_buffer_unmap (gstBuf, &gstBufMap);

        //BTRMGRLOG_INFO ("PUSHING BUFFER = %d\n", pstBtrMgrSoGst->i32InBufMaxSize);
        gst_app_src_push_buffer (GST_APP_SRC (appsrc), gstBuf);

        aiInBufSize     -= pstBtrMgrSoGst->i32InBufMaxSize;
        i32InBufOffset  += pstBtrMgrSoGst->i32InBufMaxSize;
    }


    {
        GstBuffer*  gstBuf;
        GstMapInfo  gstBufMap;

        gstBuf = gst_buffer_new_and_alloc (aiInBufSize);
        gst_buffer_set_size (gstBuf, aiInBufSize);

        gst_buffer_map (gstBuf, &gstBufMap, GST_MAP_WRITE);

        //TODO: Repalce memcpy and new_alloc if possible
        memcpy (gstBufMap.data, pcInBuf + i32InBufOffset, aiInBufSize);

        //TODO: Arrive at this vale based on Sampling rate, bits per sample, num of Channels and the 
        // size of the incoming buffer (which represents the num of samples received at this instant)
        GST_BUFFER_PTS (gstBuf)         = pstBtrMgrSoGst->gstClkTStamp;
        GST_BUFFER_DURATION (gstBuf)    = GST_USECOND * (aiInBufSize * 1000)/((pstBtrMgrSoGst->i32InRate/1000.0) * (pstBtrMgrSoGst->ui32InBitsPSample/8) * pstBtrMgrSoGst->i32InChannels);
        pstBtrMgrSoGst->gstClkTStamp   += GST_BUFFER_DURATION (gstBuf);

        GST_BUFFER_OFFSET (gstBuf)      = pstBtrMgrSoGst->inBufOffset;
        pstBtrMgrSoGst->inBufOffset    += aiInBufSize;
        GST_BUFFER_OFFSET_END (gstBuf)  = pstBtrMgrSoGst->inBufOffset - 1;

        gst_buffer_unmap (gstBuf, &gstBufMap);

        //BTRMGRLOG_INFO ("PUSHING BUFFER = %d\n", aiInBufSize);
        gst_app_src_push_buffer (GST_APP_SRC (appsrc), gstBuf);
    }

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstRet
BTRMgr_SO_GstSendEOS (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst* pstBtrMgrSoGst = (stBTRMgrSOGst*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSoGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSoGst->pLoop;
    guint       busWatchId  = pstBtrMgrSoGst->busWId;

    gboolean    lbPipelineEr = FALSE;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;


    g_mutex_lock(&pstBtrMgrSoGst->pipelineDataMutex);
    lbPipelineEr = pstBtrMgrSoGst->bPipelineError;
    g_mutex_unlock(&pstBtrMgrSoGst->pipelineDataMutex);


    if (lbPipelineEr == TRUE) {
        return eBTRMgrSOGstFailure;
    }


    /* push EOS */
    gst_app_src_end_of_stream (GST_APP_SRC (appsrc));
    return eBTRMgrSOGstSuccess;
}


// Outgoing callbacks Registration Interfaces


/*  Incoming Callbacks */
#if !(ENABLE_MAIN_LOOP_CONTEXT)
static gboolean
btrMgr_SO_g_main_loop_RunningCb (
    gpointer    apstBtrMgrSoGst
) {
    stBTRMgrSOGst*  pstBtrMgrSoGst = (stBTRMgrSOGst*)apstBtrMgrSoGst;

    if (!pstBtrMgrSoGst) {
        BTRMGRLOG_ERROR ("GMainLoop Error - In arguments Exiting\n");
        return G_SOURCE_REMOVE;
    }

  BTRMGRLOG_INFO ("GMainLoop running now\n");

  g_mutex_lock (&pstBtrMgrSoGst->gMtxMainLoopRunLock);
  g_cond_signal (&pstBtrMgrSoGst->gCndMainLoopRun);
  g_mutex_unlock (&pstBtrMgrSoGst->gMtxMainLoopRunLock);

  return G_SOURCE_REMOVE;
}
#endif


static void
btrMgr_SO_NeedDataCb (
    GstElement* appsrc,
    guint       size,
    gpointer    apstBtrMgrSoGst
) {
    stBTRMgrSOGst*  pstBtrMgrSoGst = (stBTRMgrSOGst*)apstBtrMgrSoGst;
    
    //BTRMGRLOG_INFO ("NEED DATA = %d\n", size);
    if (pstBtrMgrSoGst && pstBtrMgrSoGst->fpcBSoGstStatus) {
        pstBtrMgrSoGst->fpcBSoGstStatus(eBTRMgrSOGstStUnderflow, pstBtrMgrSoGst->pvcBUserData);
    }
}


static void
btrMgr_SO_EnoughDataCb (
    GstElement* appsrc,
    gpointer    apstBtrMgrSoGst
) {
    stBTRMgrSOGst*  pstBtrMgrSoGst = (stBTRMgrSOGst*)apstBtrMgrSoGst;
    
    //BTRMGRLOG_INFO ("ENOUGH DATA\n");
    if (pstBtrMgrSoGst && pstBtrMgrSoGst->fpcBSoGstStatus) {
        pstBtrMgrSoGst->fpcBSoGstStatus(eBTRMgrSOGstStOverflow, pstBtrMgrSoGst->pvcBUserData);
    }
}


static gboolean
btrMgr_SO_gstBusCallCb (
    GstBus*     bus,
    GstMessage* msg,
    gpointer    apstBtrMgrSoGst
) {
    stBTRMgrSOGst*  pstBtrMgrSoGst = (stBTRMgrSOGst*)apstBtrMgrSoGst;

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS: {
        BTRMGRLOG_INFO ("End-of-stream\n");
        if (pstBtrMgrSoGst) {
            if (pstBtrMgrSoGst->pLoop) {
                g_main_loop_quit(pstBtrMgrSoGst->pLoop);
            }

            if (pstBtrMgrSoGst->fpcBSoGstStatus) {
                BTRMGRLOG_INFO ("EOS: Trigger Final Status cB\n");
                pstBtrMgrSoGst->fpcBSoGstStatus(pstBtrMgrSoGst->bPipelineError == TRUE ? eBTRMgrSOGstStError : eBTRMgrSOGstStCompleted,
                                                pstBtrMgrSoGst->pvcBUserData);
            }
        }
        break;
    }

    case GST_MESSAGE_STATE_CHANGED: {
        if (pstBtrMgrSoGst && (pstBtrMgrSoGst->pPipeline) && (GST_MESSAGE_SRC (msg) == GST_OBJECT (pstBtrMgrSoGst->pPipeline))) {
            GstState prevState, currentState;

            gst_message_parse_state_changed (msg, &prevState, &currentState, NULL);
            BTRMGRLOG_INFO ("From: %s -> To: %s\n", 
                        gst_element_state_get_name (prevState), gst_element_state_get_name (currentState));
        }
        break;
    }

    case GST_MESSAGE_ERROR: {
        gchar*      debug;
        GError*     err;

        if (pstBtrMgrSoGst) {
            g_mutex_lock(&pstBtrMgrSoGst->pipelineDataMutex);
            pstBtrMgrSoGst->bPipelineError  = TRUE;
            g_mutex_unlock(&pstBtrMgrSoGst->pipelineDataMutex);
        }

        gst_message_parse_error (msg, &err, &debug);
        BTRMGRLOG_DEBUG ("Debugging info: %s\n", (debug) ? debug : "none");
        g_free (debug);

        BTRMGRLOG_ERROR ("Error: %s\n", err->message);
        g_error_free (err);

        if (pstBtrMgrSoGst) {
            if (pstBtrMgrSoGst->pLoop) {
                g_main_loop_quit(pstBtrMgrSoGst->pLoop);
            }

            if (pstBtrMgrSoGst->fpcBSoGstStatus) {
                BTRMGRLOG_INFO ("ERROR: Trigger Final Status cB\n");
                pstBtrMgrSoGst->fpcBSoGstStatus(pstBtrMgrSoGst->bPipelineError == TRUE ? eBTRMgrSOGstStError : eBTRMgrSOGstStCompleted,
                                                pstBtrMgrSoGst->pvcBUserData);
            }
        }
        break;
    }

    case GST_MESSAGE_WARNING:{
        gchar*    debug;
        GError*   warn;

        gst_message_parse_warning (msg, &warn, &debug);
        BTRMGRLOG_DEBUG ("Debugging info: %s\n", (debug) ? debug : "none");
        g_free (debug);

        BTRMGRLOG_WARN ("Warning: %s\n", warn->message);
        g_error_free (warn);

        if (pstBtrMgrSoGst && pstBtrMgrSoGst->fpcBSoGstStatus) {
            pstBtrMgrSoGst->fpcBSoGstStatus(eBTRMgrSOGstStWarning, pstBtrMgrSoGst->pvcBUserData);
        }

      break;
    }

    default:
        break;

    }

    return TRUE;
}

