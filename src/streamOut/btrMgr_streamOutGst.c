/*
 * ============================================================================
 * RDK MANAGEMENT, LLC CONFIDENTIAL AND PROPRIETARY
 * ============================================================================
 * This file (and its contents) are the intellectual property of RDK Management, LLC.
 * It may not be used, copied, distributed or otherwise  disclosed in whole or in
 * part without the express written permission of RDK Management, LLC.
 * ============================================================================
 * Copyright (c) 2016 RDK Management, LLC. All rights reserved.
 * ============================================================================
 */
/**
 * @file btrMgr_streamOutGst.c
 *
 * @description This file implements bluetooth manager's GStreamer streaming interface to external BT devices
 *
 * Copyright (c) 2016  Comcast
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


/* Local Headers */
#include "btrMgr_streamOutGst.h"


/* Local defines */
#define BTR_MGR_SLEEP_TIMEOUT_MS            1   // Suspend execution of thread. Keep as minimal as possible
#define BTR_MGR_WAIT_TIMEOUT_MS             2   // Use for blocking operations

#define GST_ELEMENT_GET_STATE_RETRY_CNT_MAX 5



typedef struct _stBTRMgrSOGst {
    void* pPipeline;
    void* pSrc;
    void* pSink;
    void* pRtpPay;
    void* pLoop;
    void* pLoopThread;
    guint busWId;
} stBTRMgrSOGst;


/* Local function declarations */
static gpointer btrMgr_SO_g_main_loop_run_context (gpointer user_data);
static gboolean btrMgr_SO_gstBusCall (GstBus* bus, GstMessage* msg, gpointer data);
static GstState btrMgr_SO_validateStateWithTimeout (GstElement* element, GstState stateToValidate, guint msTimeOut);


/* Local function definitions */
static gpointer
btrMgr_SO_g_main_loop_run_context (
    gpointer user_data
) {
  g_main_loop_run (user_data);
  return NULL;
}


static gboolean
btrMgr_SO_gstBusCall (
    GstBus*     bus,
    GstMessage* msg,
    gpointer    data
) {
    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS: {
            g_print ("%s:%d:%s - End-of-stream\n", __FILE__, __LINE__, __FUNCTION__);
            break;
        }   
        case GST_MESSAGE_ERROR: {
            gchar*  debug;
            GError* err;

            gst_message_parse_error (msg, &err, &debug);
            g_printerr ("%s:%d:%s - Debugging info: %s\n", __FILE__, __LINE__, __FUNCTION__, (debug) ? debug : "none");
            g_free (debug);

            g_print ("%s:%d:%s - Error: %s\n", __FILE__, __LINE__, __FUNCTION__, err->message);
            g_error_free (err);
            break;
        }   
        default:
            break;
    }

    return TRUE;
}


static GstState 
btrMgr_SO_validateStateWithTimeout (
    GstElement* element,
    GstState    stateToValidate,
    guint       msTimeOut
) {
    GstState    gst_current;
    GstState    gst_pending;
    float       timeout = BTR_MGR_WAIT_TIMEOUT_MS;
    gint        gstGetStateCnt = GST_ELEMENT_GET_STATE_RETRY_CNT_MAX;

    do { 
        if ((GST_STATE_CHANGE_SUCCESS == gst_element_get_state(GST_ELEMENT(element), &gst_current, &gst_pending, timeout * GST_MSECOND)) && (gst_current == stateToValidate)) {
            g_print("%s:%d:%s - gst_element_get_state - SUCCESS : State = %d, Pending = %d\n", __FILE__, __LINE__, __FUNCTION__, gst_current, gst_pending);
            return gst_current;
        }
        usleep(msTimeOut * 1000); // Let element safely transition to required state 
    } while ((gst_current != stateToValidate) && (gstGetStateCnt-- != 0)) ;

    g_print("%s:%d:%s - gst_element_get_state - FAILURE : State = %d, Pending = %d\n", __FILE__, __LINE__, __FUNCTION__, gst_current, gst_pending);

    if (gst_pending == stateToValidate)
        return gst_pending;
    else
        return gst_current;
}


/* Interfaces */
eBTRMgrSOGstStatus
BTRMgr_SO_GstInit (
    tBTRMgrSoGstHdl* phBTRMgrSoGstHdl
) {
    GstElement*     appsrc;
    GstElement*     sbcenc;
    GstElement*     rtpsbcpay;
    GstElement*     fdsink;
    GstElement*     pipeline;
    stBTRMgrSOGst*  pstBtrMgrSoGst = NULL;

    GThread*      mainLoopThread;
    GMainLoop*    loop;
    GstBus*       bus;
    guint         busWatchId;


    if ((pstBtrMgrSoGst = (stBTRMgrSOGst*)malloc (sizeof(stBTRMgrSOGst))) == NULL) {
        g_print ("%s:%d:%s - Unable to allocate memory\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    memset((void*)pstBtrMgrSoGst, 0, sizeof(stBTRMgrSOGst));

    gst_init (NULL, NULL);

    /* Create elements */
    appsrc   = gst_element_factory_make ("appsrc",   "btmgr-so-appsrc");
#if defined(DISABLE_SBC_ENCODING)
    sbcenc   = gst_element_factory_make ("queue",    "btmgr-so-sbcenc");
    rtpsbcpay= gst_element_factory_make ("queue",    "btmgr-so-rtpsbcpay");
#else
    sbcenc   = gst_element_factory_make ("sbcenc",   "btmgr-so-sbcenc");
    rtpsbcpay= gst_element_factory_make ("rtpsbcpay","btmgr-so-rtpsbcpay");
#endif
    fdsink   = gst_element_factory_make ("fdsink",   "btmgr-so-fdsink");

    /* Create and event loop and feed gstreamer bus mesages to it */
    loop = g_main_loop_new (NULL, FALSE);

    /* Create a new pipeline to hold the elements */
    pipeline = gst_pipeline_new ("btmgr-so-pipeline");

    if (!appsrc || !sbcenc || !rtpsbcpay || !fdsink || !loop || !pipeline) {
        g_print ("%s:%d:%s - Gstreamer plugin missing for streamOut\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId = gst_bus_add_watch (bus, btrMgr_SO_gstBusCall, loop);
    g_object_unref (bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), appsrc, sbcenc, rtpsbcpay, fdsink, NULL);
    gst_element_link_many (appsrc, sbcenc, rtpsbcpay, fdsink, NULL);

    mainLoopThread = g_thread_new("btrMgr_SO_g_main_loop_run_context", btrMgr_SO_g_main_loop_run_context, loop);

        
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTR_MGR_SLEEP_TIMEOUT_MS)!= GST_STATE_NULL) {
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }


    pstBtrMgrSoGst->pPipeline   = (void*)pipeline;
    pstBtrMgrSoGst->pSrc        = (void*)appsrc;
    pstBtrMgrSoGst->pSink       = (void*)fdsink;
    pstBtrMgrSoGst->pRtpPay     = (void*)rtpsbcpay;
    pstBtrMgrSoGst->pLoop       = (void*)loop;
    pstBtrMgrSoGst->pLoopThread = (void*)mainLoopThread;
    pstBtrMgrSoGst->busWId      = busWatchId;


    *phBTRMgrSoGstHdl = (tBTRMgrSoGstHdl)pstBtrMgrSoGst;

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstDeInit (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst const* pstBtrMgrSoGst = (stBTRMgrSOGst const*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline        = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc          = (GstElement*)pstBtrMgrSoGst->pSrc;
    GMainLoop*  loop            = (GMainLoop*)pstBtrMgrSoGst->pLoop;
    GThread*    mainLoopThread  = (GThread*)pstBtrMgrSoGst->pLoopThread;
    guint       busWatchId      = pstBtrMgrSoGst->busWId;

    (void)pipeline;
    (void)appsrc;
    (void)loop;
    (void)busWatchId;

    /* cleanup */
    g_object_unref (GST_OBJECT(pipeline));
    g_source_remove (busWatchId);

    g_main_loop_quit (loop);
    g_thread_join (mainLoopThread);
    g_main_loop_unref (loop);

    memset((void*)pstBtrMgrSoGst, 0, sizeof(stBTRMgrSOGst));
    free((void*)pstBtrMgrSoGst);

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstStart (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl,
    int aiInBufMaxSize,
    int aiBTDevFd,
    int aiBTDevMTU
) {
    stBTRMgrSOGst const* pstBtrMgrSoGst = (stBTRMgrSOGst const*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailInArg;
    }

    GstElement* pipeline    = (GstElement*)pstBtrMgrSoGst->pPipeline;
    GstElement* appsrc      = (GstElement*)pstBtrMgrSoGst->pSrc;
    GstElement* fdsink      = (GstElement*)pstBtrMgrSoGst->pSink;
    GstElement* rtpsbcpay   = (GstElement*)pstBtrMgrSoGst->pRtpPay;
    guint       busWatchId  = pstBtrMgrSoGst->busWId;

    GstCaps* appsrcSrcCaps  = NULL;

    (void)pipeline;
    (void)appsrc;
    (void)busWatchId;

    /* Check if we are in correct state */
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        g_print ("%s:%d:%s - Incorrect State to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    /*TODO: Set the caps dynamically based on input arguments to Start */
    appsrcSrcCaps = gst_caps_new_simple ("audio/x-raw",
                                         "format", G_TYPE_STRING, GST_AUDIO_NE (S16),
                                         "layout", G_TYPE_STRING, "interleaved",
                                         "rate", G_TYPE_INT, 48000,
                                         "channels", G_TYPE_INT, 2,
                                          NULL);

    g_object_set (appsrc, "caps", appsrcSrcCaps, NULL);
    g_object_set (appsrc, "blocksize", aiInBufMaxSize, NULL);
    g_object_set (appsrc, "min-latency", 0, NULL);
    g_object_set (appsrc, "format", GST_FORMAT_TIME, NULL);
    g_object_set (appsrc, "do-timestamp", 1, NULL);

    g_object_set (rtpsbcpay, "mtu", aiBTDevMTU, NULL);

    g_object_set (fdsink, "fd", aiBTDevFd, NULL);


    gst_caps_unref(appsrcSrcCaps);

    /* start play back and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) { 
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstStop (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst const* pstBtrMgrSoGst = (stBTRMgrSOGst const*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
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

    /* stop play back */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_NULL, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_NULL) {
        g_print ("%s:%d:%s - - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstPause (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst const* pstBtrMgrSoGst = (stBTRMgrSOGst const*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
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
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        g_print ("%s:%d:%s - Incorrect State to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    /* pause playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PAUSED);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    } 

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstResume (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst const* pstBtrMgrSoGst = (stBTRMgrSOGst const*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
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
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PAUSED, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PAUSED) {
        g_print ("%s:%d:%s - Incorrect State to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    /* Resume playback and listed to events */
    gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    if (btrMgr_SO_validateStateWithTimeout(pipeline, GST_STATE_PLAYING, BTR_MGR_SLEEP_TIMEOUT_MS) != GST_STATE_PLAYING) {
        g_print ("%s:%d:%s - Unable to perform Operation\n", __FILE__, __LINE__, __FUNCTION__);
        return eBTRMgrSOGstFailure;
    }

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstSendBuffer (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl,
    char*   pcInBuf,
    int     aiInBufSize
) {
    stBTRMgrSOGst const* pstBtrMgrSoGst = (stBTRMgrSOGst const*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
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

    /* push Buffers */
    {
        GstBuffer *gstBuf;
        GstMapInfo gstBufMap;

        gstBuf = gst_buffer_new_and_alloc (aiInBufSize);
        gst_buffer_map (gstBuf, &gstBufMap, GST_MAP_WRITE);

        //TODO: Repalce memcpy and new_alloc if possible
        memcpy (gstBufMap.data, pcInBuf, aiInBufSize);
        gst_app_src_push_buffer (GST_APP_SRC (appsrc), gstBuf);

        gst_buffer_unmap (gstBuf, &gstBufMap);
    }

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstSendEOS (
    tBTRMgrSoGstHdl hBTRMgrSoGstHdl
) {
    stBTRMgrSOGst const* pstBtrMgrSoGst = (stBTRMgrSOGst const*)hBTRMgrSoGstHdl;

    if (!pstBtrMgrSoGst) {
        g_print ("%s:%d:%s - Invalid input argument\n", __FILE__, __LINE__, __FUNCTION__);
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

    /* push EOS */
    gst_app_src_end_of_stream (GST_APP_SRC (appsrc));

    return eBTRMgrSOGstSuccess;
}

