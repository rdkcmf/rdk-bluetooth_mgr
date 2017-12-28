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
#include "btmgr_priv.h"         //for rdklogger

/* Local Headers */
#include "btrMgr_streamInGst.h"


/* Local defines */
#define BTRMGR_SLEEP_TIMEOUT_MS            1   // Suspend execution of thread. Keep as minimal as possible
#define BTRMGR_WAIT_TIMEOUT_MS             2   // Use for blocking operations
#define BTRMGR_MAX_INTERNAL_QUEUE_ELEMENTS 16  // Number of blocks in the internal queue

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5



typedef struct _stBTRMgrSIGst {
    void*        pPipeline;
    void*        pSrc;
    void*        pSrcCapsFilter;
    void*        pAudioDec;
    void*        pAudioParse;
    void*        psbcdecCapsFilter;
    void*        pRtpAudioDePay;
    void*        pSink;
    void*        pLoop;
    void*        pLoopThread;
    guint        busWId;
    GstClockTime gstClkTStamp;
    guint64      inBufOffset;
} stBTRMgrSIGst;


/* Local function declarations */
static gpointer btrMgr_SI_g_main_loop_run_context (gpointer user_data);
static gboolean btrMgr_SI_gstBusCall (GstBus* bus, GstMessage* msg, gpointer data);
static GstState btrMgr_SI_validateStateWithTimeout (GstElement* element, GstState stateToValidate, guint msTimeOut);


/* Local function definitions */
static gpointer
btrMgr_SI_g_main_loop_run_context (
    gpointer user_data
) {
    GMainLoop*  loop = (GMainLoop*)user_data;

    if (loop) {
        BTRMGRLOG_INFO ("GMainLoop Running\n");
        g_main_loop_run (loop);
    }

    BTRMGRLOG_INFO ("GMainLoop Exiting\n");
    return NULL;
}


static gboolean
btrMgr_SI_gstBusCall (
    GstBus*     bus,
    GstMessage* msg,
    gpointer    data
) {
    GMainLoop*  loop = (GMainLoop*)data;

    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS: {
            BTRMGRLOG_INFO ("End-of-stream\n");
            if (loop) {
                g_main_loop_quit(loop);
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

            if (loop) {
                g_main_loop_quit(loop);
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


/* Interfaces */
eBTRMgrSIGstRet
BTRMgr_SI_GstInit (
    tBTRMgrSiGstHdl* phBTRMgrSiGstHdl
) {
    GstElement*     fdsrc;
    GstElement*     rtpcapsfilter;
    GstElement*     rtpauddepay;
    GstElement*     audparse;
    GstElement*     auddec;
    GstElement*     fdsink;
    GstElement*     pipeline;
    stBTRMgrSIGst*  pstBtrMgrSiGst = NULL;

    GThread*      mainLoopThread;
    GMainLoop*    loop;
    GstBus*       bus;
    guint         busWatchId;


    if ((pstBtrMgrSiGst = (stBTRMgrSIGst*)malloc (sizeof(stBTRMgrSIGst))) == NULL) {
        BTRMGRLOG_ERROR ("Unable to allocate memory\n");
        return eBTRMgrSIGstFailure;
    }

    memset((void*)pstBtrMgrSiGst, 0, sizeof(stBTRMgrSIGst));

    gst_init (NULL, NULL);

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

    /* Create a new pipeline to hold the elements */
    pipeline = gst_pipeline_new ("btmgr-si-pipeline");

    if (!fdsrc || !rtpcapsfilter || !rtpauddepay || !audparse || !auddec || !fdsink || !loop || !pipeline) {
        BTRMGRLOG_ERROR ("Gstreamer plugin missing for streamIn\n");
        return eBTRMgrSIGstFailure;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId = gst_bus_add_watch (bus, btrMgr_SI_gstBusCall, loop);
    g_object_unref (bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), fdsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);
    gst_element_link_many (fdsrc, rtpcapsfilter, rtpauddepay, audparse, auddec, fdsink, NULL);

    mainLoopThread = g_thread_new("btrMgr_SI_g_main_loop_run_context", btrMgr_SI_g_main_loop_run_context, loop);

        
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (btrMgr_SI_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTRMGR_SLEEP_TIMEOUT_MS)!= GST_STATE_NULL) {
        BTRMGRLOG_ERROR ("Unable to perform Operation\n");
        return eBTRMgrSIGstFailure;
    }


    pstBtrMgrSiGst->pPipeline       = (void*)pipeline;
    pstBtrMgrSiGst->pSrc            = (void*)fdsrc;
    pstBtrMgrSiGst->pSrcCapsFilter   = (void*)rtpcapsfilter;
    pstBtrMgrSiGst->pRtpAudioDePay  = (void*)rtpauddepay;
    pstBtrMgrSiGst->pAudioParse     = (void*)audparse;
    pstBtrMgrSiGst->pAudioDec       = (void*)auddec;
    pstBtrMgrSiGst->pSink           = (void*)fdsink;
    pstBtrMgrSiGst->pLoop           = (void*)loop;
    pstBtrMgrSiGst->pLoopThread     = (void*)mainLoopThread;
    pstBtrMgrSiGst->busWId          = busWatchId;
    pstBtrMgrSiGst->gstClkTStamp    = 0;
    pstBtrMgrSiGst->inBufOffset     = 0;

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

    GstElement* pipeline        = (GstElement*)pstBtrMgrSiGst->pPipeline;
    GstElement* fdsrc           = (GstElement*)pstBtrMgrSiGst->pSrc;
    GMainLoop*  loop            = (GMainLoop*)pstBtrMgrSiGst->pLoop;
    GThread*    mainLoopThread  = (GThread*)pstBtrMgrSiGst->pLoopThread;
    guint       busWatchId      = pstBtrMgrSiGst->busWId;

    (void)pipeline;
    (void)fdsrc;
    (void)loop;
    (void)busWatchId;

    /* cleanup */
    if (pipeline) {
        gst_object_unref (GST_OBJECT(pipeline));
        pipeline = NULL;
    }

    if (busWatchId) {
        GSource *busSource = g_main_context_find_source_by_id(g_main_context_get_thread_default(), busWatchId);
        if (busSource)
            g_source_destroy(busSource);

        busWatchId = 0;
    }

    if (loop) {
        g_main_loop_quit (loop);
    }

    if (mainLoopThread) {
        g_thread_join (mainLoopThread);
        mainLoopThread = NULL;
    }

    if (loop) {
        g_main_loop_unref (loop);
        loop = NULL;
    }

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
    char*   pcInBuf,
    int     aiInBufSize
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

