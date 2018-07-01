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
 * @file btrMgr_streamInGst.c
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
#include "btrMgr_streamInGst.h"


/* Local defines */
#define BTRMGR_SLEEP_TIMEOUT_MS            1   // Suspend execution of thread. Keep as minimal as possible
#define BTRMGR_WAIT_TIMEOUT_MS             2   // Use for blocking operations
#define BTRMGR_MAX_INTERNAL_QUEUE_ELEMENTS 16  // Number of blocks in the internal queue

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5

#define ENABLE_MAIN_LOOP_CONTEXT 0


/* Local Types & Typedefs */
typedef struct _stBTRMgrSIGst {
    void*        pPipeline;
    void*        pSrc;
    void*        pSrcCapsFilter;
    void*        pAudioDec;
    void*        pAudioParse;
    void*        psbcdecCapsFilter;
    void*        pRtpAudioDePay;
    void*        pSink;

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

    fPtr_BTRMgr_SI_GstStatusCb  fpcBSiGstStatus;
    void*                       pvcBUserData;

} stBTRMgrSIGst;


/* Static Function Prototypes */
static GstState btrMgr_SI_validateStateWithTimeout (GstElement* element, GstState stateToValidate, guint msTimeOut);

/* Local Op Threads Prototypes */
static gpointer btrMgr_SI_g_main_loop_Task (gpointer apstBtrMgrSiGst);

/* Incoming Callbacks Prototypes */
#if !(ENABLE_MAIN_LOOP_CONTEXT)
static gboolean btrMgr_SI_g_main_loop_RunningCb (gpointer apstBtrMgrSiGst);
#endif
static gboolean btrMgr_SI_gstBusCallCb (GstBus* bus, GstMessage* msg, gpointer apstBtrMgrSiGst);


/* Static Function Definition */
static GstState 
btrMgr_SI_validateStateWithTimeout (
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
           BTRMGRLOG_INFO ("gst_element_get_state - SUCCESS : State = %d, Pending = %d\n", gst_current, gst_pending);
            return gst_current;
        }
        usleep(msTimeOut * 1000); // Let element safely transition to required state 
    } while ((gst_current != stateToValidate) && (gstGetStateCnt-- != 0)) ;

    BTRMGRLOG_ERROR ("gst_element_get_state - FAILURE : State = %d, Pending = %d\n", gst_current, gst_pending);

    if (gst_pending == stateToValidate)
        return gst_pending;
    else
        return gst_current;
}


/* Local Op Threads */
static gpointer
btrMgr_SI_g_main_loop_Task (
    gpointer apstBtrMgrSiGst
) {
    stBTRMgrSIGst*  pstBtrMgrSiGst = (stBTRMgrSIGst*)apstBtrMgrSiGst;

    if (!pstBtrMgrSiGst || !pstBtrMgrSiGst->pLoop) {
        BTRMGRLOG_ERROR ("GMainLoop Error - In arguments Exiting\n");
        return NULL;
    }

#if !(ENABLE_MAIN_LOOP_CONTEXT)
    GstElement*     fdsrc           = NULL;
    GstElement*     rtpcapsfilter   = NULL;
    GstElement*     rtpauddepay     = NULL;
    GstElement*     audparse        = NULL;
    GstElement*     auddec          = NULL;
    GstElement*     fdsink          = NULL;
    GstElement*     pipeline        = NULL;
    GstBus*         bus             = NULL;
    GSource*        idleSource      = NULL;
    GMainLoop*      loop            = NULL;
    GMainContext*   mainContext     = NULL;
    guint           busWatchId;


    if (!pstBtrMgrSiGst->pContext) {
        BTRMGRLOG_ERROR ("GMainLoop Error - No context\n");
        return NULL;
    }


    mainContext = pstBtrMgrSiGst->pContext;
    g_main_context_push_thread_default (mainContext);

    idleSource = g_idle_source_new ();
    g_source_set_callback (idleSource, (GSourceFunc) btrMgr_SI_g_main_loop_RunningCb, pstBtrMgrSiGst, NULL);
    g_source_attach (idleSource, mainContext);
    g_source_unref (idleSource);


    /* Create elements */
    fdsrc           = gst_element_factory_make ("fdsrc",        "btmgr-si-fdsrc");

    /*TODO: Select the Audio Codec and RTP Audio DePayloader based on input format*/
    rtpcapsfilter   = gst_element_factory_make ("capsfilter",   "btmgr-si-rtpcapsfilter");
    rtpauddepay     = gst_element_factory_make ("rtpsbcdepay",  "btmgr-si-rtpsbcdepay");
    audparse        = gst_element_factory_make ("sbcparse",     "btmgr-si-rtpsbcparse");
    auddec          = gst_element_factory_make ("sbcdec",       "btmgr-si-sbcdec");

    // make fdsink a filesink, so you can  write pcm data to file or pipe, or alternatively, send it to the brcmpcmsink
    fdsink          = gst_element_factory_make ("brcmpcmsink",  "btmgr-si-pcmsink");

    /* Create a new pipeline to hold the elements */
    pipeline        = gst_pipeline_new ("btmgr-si-pipeline");

    loop =  pstBtrMgrSiGst->pLoop;

    if (!fdsrc || !rtpcapsfilter || !rtpauddepay || !audparse || !auddec || !fdsink || !loop || !pipeline) {
        BTRMGRLOG_ERROR ("Gstreamer plugin missing for streamIn\n");
        return NULL;
    }

    pstBtrMgrSiGst->pPipeline       = (void*)pipeline;
    pstBtrMgrSiGst->pSrc            = (void*)fdsrc;
    pstBtrMgrSiGst->pSrcCapsFilter  = (void*)rtpcapsfilter;
    pstBtrMgrSiGst->pRtpAudioDePay  = (void*)rtpauddepay;
    pstBtrMgrSiGst->pAudioParse     = (void*)audparse;
    pstBtrMgrSiGst->pAudioDec       = (void*)auddec;
    pstBtrMgrSiGst->pSink           = (void*)fdsink;
    pstBtrMgrSiGst->pLoop           = (void*)loop;
    pstBtrMgrSiGst->gstClkTStamp    = 0;
    pstBtrMgrSiGst->inBufOffset     = 0;


    bus                     = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId              = gst_bus_add_watch (bus, btrMgr_SI_gstBusCallCb, pstBtrMgrSiGst);
    pstBtrMgrSiGst->busWId  = busWatchId;
    g_object_unref (bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), fdsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);
    gst_element_link_many (fdsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);

#endif


    BTRMGRLOG_INFO ("GMainLoop Running\n");
    g_main_loop_run (pstBtrMgrSiGst->pLoop);


#if !(ENABLE_MAIN_LOOP_CONTEXT)
    if (busWatchId) {
        GSource *busSource = g_main_context_find_source_by_id(mainContext, busWatchId);
        if (busSource)
            g_source_destroy(busSource);

        busWatchId = 0;
        pstBtrMgrSiGst->busWId = busWatchId;
    }


    g_main_context_pop_thread_default (mainContext);
#endif

    BTRMGRLOG_INFO ("GMainLoop Exiting\n");
    return pstBtrMgrSiGst;
}


/* Interfaces */
eBTRMgrSIGstRet
BTRMgr_SI_GstInit (
    tBTRMgrSiGstHdl*            phBTRMgrSiGstHdl,
    fPtr_BTRMgr_SI_GstStatusCb  afpcBSiGstStatus,
    void*                       apvUserData
) {
#if (ENABLE_MAIN_LOOP_CONTEXT)
    GstElement*     fdsrc           = NULL;
    GstElement*     rtpcapsfilter   = NULL;
    GstElement*     rtpauddepay     = NULL;
    GstElement*     audparse        = NULL;
    GstElement*     auddec          = NULL;
    GstElement*     fdsink          = NULL;
    GstElement*     pipeline        = NULL;
    GstBus*         bus             = NULL;
    guint           busWatchId;
#else
    GMainContext*   mainContext     = NULL;
#endif

    stBTRMgrSIGst*  pstBtrMgrSiGst  = NULL;
    GThread*        mainLoopThread  = NULL;
    GMainLoop*      loop            = NULL;


    if ((pstBtrMgrSiGst = (stBTRMgrSIGst*)malloc (sizeof(stBTRMgrSIGst))) == NULL) {
        BTRMGRLOG_ERROR ("Unable to allocate memory\n");
        return eBTRMgrSIGstFailure;
    }

    memset((void*)pstBtrMgrSiGst, 0, sizeof(stBTRMgrSIGst));

    gst_init (NULL, NULL);

#if (ENABLE_MAIN_LOOP_CONTEXT)
    /* Create elements */
    fdsrc           = gst_element_factory_make ("fdsrc",        "btmgr-si-fdsrc");

    /*TODO: Select the Audio Codec and RTP Audio DePayloader based on input format*/
    rtpcapsfilter   = gst_element_factory_make ("capsfilter",   "btmgr-si-rtpcapsfilter");
    rtpauddepay     = gst_element_factory_make ("rtpsbcdepay",  "btmgr-si-rtpsbcdepay");
    audparse        = gst_element_factory_make ("sbcparse",     "btmgr-si-rtpsbcparse");
    auddec          = gst_element_factory_make ("sbcdec",       "btmgr-si-sbcdec");

    // make fdsink a filesink, so you can  write pcm data to file or pipe, or alternatively, send it to the brcmpcmsink
    fdsink          = gst_element_factory_make ("brcmpcmsink",  "btmgr-si-pcmsink");

    /* Create an event loop and feed gstreamer bus mesages to it */
    loop = g_main_loop_new (NULL, FALSE);
#else
    g_mutex_init (&pstBtrMgrSiGst->gMtxMainLoopRunLock);
    g_cond_init (&pstBtrMgrSiGst->gCndMainLoopRun);

    mainContext = g_main_context_new();

    /* Create and event loop and feed gstreamer bus mesages to it */
    if (mainContext) {
        loop = g_main_loop_new (mainContext, FALSE);
    }
#endif


#if (ENABLE_MAIN_LOOP_CONTEXT)
    /* Create a new pipeline to hold the elements */
    pipeline = gst_pipeline_new ("btmgr-si-pipeline");

    if (!fdsrc || !rtpcapsfilter || !rtpauddepay || !audparse || !auddec || !fdsink || !loop || !pipeline) {
        BTRMGRLOG_ERROR ("Gstreamer plugin missing for streamIn\n");
        //TODO: Call BTRMgr_SI_GstDeInit((tBTRMgrSiGstHdl)pstBtrMgrSiGst);
        return eBTRMgrSIGstFailure;
    }

    pstBtrMgrSiGst->pPipeline       = (void*)pipeline;
    pstBtrMgrSiGst->pSrc            = (void*)fdsrc;
    pstBtrMgrSiGst->pSrcCapsFilter  = (void*)rtpcapsfilter;
    pstBtrMgrSiGst->pRtpAudioDePay  = (void*)rtpauddepay;
    pstBtrMgrSiGst->pAudioParse     = (void*)audparse;
    pstBtrMgrSiGst->pAudioDec       = (void*)auddec;
    pstBtrMgrSiGst->pSink           = (void*)fdsink;
    pstBtrMgrSiGst->pLoop           = (void*)loop;
    pstBtrMgrSiGst->gstClkTStamp    = 0;
    pstBtrMgrSiGst->inBufOffset     = 0;
    pstBtrMgrSiGst->fpcBSiGstStatus = NULL;
    pstBtrMgrSiGst->pvcBUserData    = NULL;


    if (afpcBSiGstStatus) {
        pstBtrMgrSiGst->fpcBSiGstStatus = afpcBSiGstStatus;
        pstBtrMgrSiGst->pvcBUserData    = apvUserData;
    }


    bus                     = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId              = gst_bus_add_watch (bus, btrMgr_SI_gstBusCallCb, pstBtrMgrSiGst);
    pstBtrMgrSiGst->busWId  = busWatchId;
    g_object_unref (bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), fdsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);
    gst_element_link_many (fdsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);


    mainLoopThread = g_thread_new("btrMgr_SI_g_main_loop_Task", btrMgr_SI_g_main_loop_Task, pstBtrMgrSiGst);
    pstBtrMgrSiGst->pLoopThread = (void*)mainLoopThread;

#else
    pstBtrMgrSiGst->pContext        = (void*)mainContext;
    pstBtrMgrSiGst->pLoop           = (void*)loop;
    pstBtrMgrSiGst->fpcBSiGstStatus = NULL;
    pstBtrMgrSiGst->pvcBUserData    = NULL;

    if (afpcBSiGstStatus) {
        pstBtrMgrSiGst->fpcBSiGstStatus = afpcBSiGstStatus;
        pstBtrMgrSiGst->pvcBUserData    = apvUserData;
    }


    g_mutex_lock (&pstBtrMgrSiGst->gMtxMainLoopRunLock);
    {
        mainLoopThread = g_thread_new("btrMgr_SI_g_main_loop_Task", btrMgr_SI_g_main_loop_Task, pstBtrMgrSiGst);
        pstBtrMgrSiGst->pLoopThread = (void*)mainLoopThread;

        //TODO: Infinite loop, break out of it gracefully in case of failures
        while (!loop || !g_main_loop_is_running (loop)) {
            g_cond_wait (&pstBtrMgrSiGst->gCndMainLoopRun, &pstBtrMgrSiGst->gMtxMainLoopRunLock);
        }
    }
    g_mutex_unlock (&pstBtrMgrSiGst->gMtxMainLoopRunLock);

#endif
        
    gst_element_set_state(GST_ELEMENT(pstBtrMgrSiGst->pPipeline), GST_STATE_NULL);
    if (btrMgr_SI_validateStateWithTimeout(pstBtrMgrSiGst->pPipeline, GST_STATE_NULL, BTRMGR_SLEEP_TIMEOUT_MS)!= GST_STATE_NULL) {
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        BTRMgr_SI_GstDeInit((tBTRMgrSiGstHdl)pstBtrMgrSiGst);
        return eBTRMgrSIGstFailure;
    }

    
    *phBTRMgrSiGstHdl = (tBTRMgrSiGstHdl)pstBtrMgrSiGst;

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstDeInit (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSIGstFailInArg;
    }

    GstElement*     pipeline        = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GMainLoop*      loop            = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    GThread*        mainLoopThread  = (GThread*)pstBtrMgrSiGst->pLoopThread;
#if (ENABLE_MAIN_LOOP_CONTEXT)
    guint           busWatchId      = pstBtrMgrSiGst->busWId;
#else
    GMainContext*   mainContext     = (GMainContext*)pstBtrMgrSiGst->pContext;
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

    if (pstBtrMgrSiGst->fpcBSiGstStatus)
        pstBtrMgrSiGst->fpcBSiGstStatus = NULL;


#if !(ENABLE_MAIN_LOOP_CONTEXT)
    g_cond_clear(&pstBtrMgrSiGst->gCndMainLoopRun);
    g_mutex_clear(&pstBtrMgrSiGst->gMtxMainLoopRunLock);
#endif


    memset((void*)pstBtrMgrSiGst, 0, sizeof(stBTRMgrSIGst));
    free((void*)pstBtrMgrSiGst);
    pstBtrMgrSiGst = NULL;

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstStart (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl,
    int             aiInBufMaxSize,
    int             aiBTDevFd,
    int             aiBTDevMTU,
    unsigned int    aiBTDevSFreq
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline        = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* fdsrc           = (GstElement*)pstBtrMgrSiGst->pSrc;
    GstElement* rtpcapsfilter   = (GstElement*)pstBtrMgrSiGst->pSrcCapsFilter;
    GstElement* auddec          = (GstElement*)pstBtrMgrSiGst->pAudioDec;

    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    GstCaps* fdsrcSrcCaps  = NULL;

    (void)pipeline;
    (void)fdsrc;
    (void)auddec;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        BTRMGRLOG_ERROR ("Incorrect State to perform Operation\n");
        return eBTRMgrSIGstFailure;
    }

    pstBtrMgrSiGst->gstClkTStamp = 0;
    pstBtrMgrSiGst->inBufOffset  = 0;

   fdsrcSrcCaps = gst_caps_new_simple ("application/x-rtp",
                                         "media", G_TYPE_STRING, "audio",
                                         "encoding-name", G_TYPE_STRING, "SBC",
                                         "clock-rate", G_TYPE_INT, aiBTDevSFreq,
                                         "payload", G_TYPE_INT, 96,
                                          NULL);

    g_object_set (fdsrc, "fd",          aiBTDevFd,  NULL);
    g_object_set (fdsrc, "blocksize",   aiBTDevMTU, NULL);
    g_object_set (fdsrc, "do-timestamp",TRUE, NULL);
    g_object_set (rtpcapsfilter, "caps",fdsrcSrcCaps, NULL);



    gst_caps_unref(fdsrcSrcCaps);

    /* start play back and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) { 
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        return eBTRMgrSIGstFailure;
    }

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstStop (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* fdsrc       = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)fdsrc;
    (void)loop;
    (void)busWatchId;

    /* stop play back */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        BTRMGRLOG_ERROR ("- Unable to perform Operation\n");
        return eBTRMgrSIGstFailure;
    }

    pstBtrMgrSiGst->gstClkTStamp = 0;
    pstBtrMgrSiGst->inBufOffset  = 0;

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstPause (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* fdsrc       = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)fdsrc;
    (void)loop;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        BTRMGRLOG_ERROR ("Incorrect State to perform Operation\n");
        return eBTRMgrSIGstFailure;
    }

    /* pause playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PAUSED);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        return eBTRMgrSIGstFailure;
    } 

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstResume (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* fdsrc       = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)fdsrc;
    (void)loop;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        BTRMGRLOG_ERROR ("Incorrect State to perform Operation\n");
        return eBTRMgrSIGstFailure;
    }

    /* Resume playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTRMGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        return eBTRMgrSIGstFailure;
    }

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstSendBuffer (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl,
    char*           pcInBuf,
    int             aiInBufSize
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* fdsrc       = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)fdsrc;
    (void)loop;
    (void)busWatchId;

    /* push Buffers */
    {
        GstBuffer *gstBuf;
        GstMapInfo gstBufMap;

        gstBuf = gst_buffer_new_and_alloc (aiInBufSize);
        gst_buffer_map (gstBuf, &gstBufMap, GST_MAP_WRITE);

        //TODO: Repalce memcpy and new_alloc if possible
        memcpy (gstBufMap.data, pcInBuf, aiInBufSize);

        //TODO: Arrive at this vale based on Sampling rate, bits per sample, num of Channels and the 
		// size of the incoming buffer (which represents the num of samples received at this instant)
        GST_BUFFER_PTS (gstBuf)         = pstBtrMgrSiGst->gstClkTStamp;
        GST_BUFFER_DURATION (gstBuf)    = GST_USECOND * (aiInBufSize * 1000)/(48 * (16/8) * 2);
        pstBtrMgrSiGst->gstClkTStamp   += GST_BUFFER_DURATION (gstBuf);

        GST_BUFFER_OFFSET (gstBuf)      = pstBtrMgrSiGst->inBufOffset;
        pstBtrMgrSiGst->inBufOffset    += aiInBufSize;
        GST_BUFFER_OFFSET_END (gstBuf)  = pstBtrMgrSiGst->inBufOffset - 1;

        gst_app_src_push_buffer (GST_APP_SRC (fdsrc), gstBuf);

        gst_buffer_unmap (gstBuf, &gstBufMap);
    }

    return eBTRMgrSIGstSuccess;
}


eBTRMgrSIGstRet
BTRMgr_SI_GstSendEOS (
    tBTRMgrSiGstHdl hBTRMgrSiGstHdl
) {
    stBTRMgrSIGst* pstBtrMgrSiGst = (stBTRMgrSIGst*)hBTRMgrSiGstHdl;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("Invalid input argument\n");
        return eBTRMgrSIGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* fdsrc       = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop        = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    guint       busWatchId  = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)fdsrc;
    (void)loop;
    (void)busWatchId;

    /* push EOS */
    gst_element_send_event (pipeline, gst_event_new_eos ());

    return eBTRMgrSIGstSuccess;
}

// Outgoing callbacks Registration Interfaces


/*  Incoming Callbacks */
#if !(ENABLE_MAIN_LOOP_CONTEXT)
static gboolean
btrMgr_SI_g_main_loop_RunningCb (
    gpointer    apstBtrMgrSiGst
) {
    stBTRMgrSIGst*  pstBtrMgrSiGst = (stBTRMgrSIGst*)apstBtrMgrSiGst;

    if (!pstBtrMgrSiGst) {
        BTRMGRLOG_ERROR ("GMainLoop Error - In arguments Exiting\n");
        return G_SOURCE_REMOVE;
    }

  BTRMGRLOG_INFO ("GMainLoop running now");

  g_mutex_lock (&pstBtrMgrSiGst->gMtxMainLoopRunLock);
  g_cond_signal (&pstBtrMgrSiGst->gCndMainLoopRun);
  g_mutex_unlock (&pstBtrMgrSiGst->gMtxMainLoopRunLock);

  return G_SOURCE_REMOVE;
}
#endif

static gboolean
btrMgr_SI_gstBusCallCb (
    GstBus*     bus,
    GstMessage* msg,
    gpointer    apstBtrMgrSiGst
) {
    stBTRMgrSIGst*  pstBtrMgrSiGst = (stBTRMgrSIGst*)apstBtrMgrSiGst;

    switch (GST_MESSAGE_TYPE (msg)) {

    case GST_MESSAGE_EOS: {

        BTRMGRLOG_INFO ("End-of-stream\n");
        if (pstBtrMgrSiGst) {
            if (pstBtrMgrSiGst->pLoop) {
                g_main_loop_quit(pstBtrMgrSiGst->pLoop);
            }
        }
        break;
    }   


    case GST_MESSAGE_STATE_CHANGED: {
        if (pstBtrMgrSiGst && (pstBtrMgrSiGst->pPipeline) && (GST_MESSAGE_SRC (msg) == GST_OBJECT (pstBtrMgrSiGst->pPipeline))) {
            GstState prevState, currentState;

            gst_message_parse_state_changed (msg, &prevState, &currentState, NULL);
            BTRMGRLOG_INFO ("From: %s -> To: %s\n",
                        gst_element_state_get_name (prevState), gst_element_state_get_name (currentState));
        }
        break;
    }


    case GST_MESSAGE_ERROR: {
        gchar*  debug;
        GError* err;

        gst_message_parse_error (msg, &err, &debug);
        BTRMGRLOG_DEBUG ("Debugging info: %s\n", (debug) ? debug : "none");
        g_free (debug);

        BTRMGRLOG_ERROR ("Error: %s\n", err->message);
        g_error_free (err);

        if (pstBtrMgrSiGst) {
            if (pstBtrMgrSiGst->pLoop) {
                g_main_loop_quit(pstBtrMgrSiGst->pLoop);
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
        break;
    }

    default:
        break;

    }

    return TRUE;
}

