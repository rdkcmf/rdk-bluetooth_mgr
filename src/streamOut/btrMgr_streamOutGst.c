#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "btrMgr_streamOutGst.h"


static gboolean
btrMgr_SO_gstBusCall (
    GstBus*     bus,
    GstMessage* msg,
    gpointer    data
) {
    GMainLoop*  loop = (GMainLoop*) data;

    switch (GST_MESSAGE_TYPE (msg)) {
        case GST_MESSAGE_EOS: {
            g_print ("End-of-stream\n");

            g_main_loop_quit (loop);
            break;
        }   
        case GST_MESSAGE_ERROR: {
            gchar*  debug;
            GError* err;

            gst_message_parse_error (msg, &err, &debug);
            g_printerr ("Debugging info: %s\n", (debug) ? debug : "none");
            g_free (debug);

            g_print ("Error: %s\n", err->message);
            g_error_free (err);

            g_main_loop_quit (loop);
            break;
        }   
        default:
            break;
    }

    return TRUE;
}


stBTRMgrSOGst const*
BTRMgr_SO_GstInit (
    void
) {
    GstElement*     appsrc;
    GstElement*     sbcenc;
    GstElement*     a2dpsink;
    GstElement*     pipeline;
    stBTRMgrSOGst*  pstBtrMgrSoGst = NULL;

    GMainLoop*    loop;
    GstBus*       bus;
    guint         busWatchId;

    gst_init (NULL, NULL);

    /* Create elements */
    appsrc   = gst_element_factory_make ("appsrc",  "btmgr-so-appsrc");
    sbcenc   = gst_element_factory_make ("sbcenc",  "btmgr-so-sbcenc");
    a2dpsink = gst_element_factory_make ("a2dpsink","btmgr-so-a2dpsink");

    /* Create and event loop and feed gstreamer bus mesages to it */
    loop = g_main_loop_new (NULL, FALSE);

    /* Create a new pipeline to hold the elements */
    pipeline = gst_pipeline_new ("btmgr-so-pipeline");

    if (!appsrc || !sbcenc || !a2dpsink || !loop || !pipeline) {
        g_print ("Gstreamer plugin missing for streamOut\n");
        return NULL;
    }

    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    busWatchId = gst_bus_add_watch (bus, btrMgr_SO_gstBusCall, loop);
    g_object_unref (bus);

    /* setup */
    gst_bin_add_many (GST_BIN (pipeline), appsrc, sbcenc, a2dpsink, NULL);
    gst_element_link_many (appsrc, sbcenc, a2dpsink, NULL);

    if ((pstBtrMgrSoGst = (stBTRMgrSOGst*)malloc (sizeof(stBTRMgrSOGst))) != NULL) {
        memset((void*)pstBtrMgrSoGst, 0, sizeof(stBTRMgrSOGst));
        pstBtrMgrSoGst->pPipeline = (void*)pipeline;
        pstBtrMgrSoGst->pSrc      = (void*)appsrc;
        pstBtrMgrSoGst->pLoop     = (void*)loop;
        pstBtrMgrSoGst->busWId    = busWatchId;
    }

    return pstBtrMgrSoGst;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstDeInit (
    stBTRMgrSOGst const* pstBtrMgrSoGst
) {
    if (!pstBtrMgrSoGst) {
        g_print ("BTRMgr_SO_GstDeInit - Invalid input argument\n");
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

    /* cleanup */
    g_object_unref (GST_OBJECT(pipeline));
    g_source_remove (busWatchId);
    g_main_loop_unref (loop);

    memset((void*)pstBtrMgrSoGst, 0, sizeof(stBTRMgrSOGst));
    free((void*)pstBtrMgrSoGst);

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstStart (
    stBTRMgrSOGst const* pstBtrMgrSoGst
) {
    if (!pstBtrMgrSoGst) {
        g_print ("BTRMgr_SO_GstDeInit - Invalid input argument\n");
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

    /* start play back and listed to events */
    gst_element_set_state (GST_ELEMENT(pipeline), GST_STATE_PLAYING);
    g_main_loop_run (loop);

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstStop (
    stBTRMgrSOGst const* pstBtrMgrSoGst
) {
    if (!pstBtrMgrSoGst) {
        g_print ("BTRMgr_SO_GstDeInit - Invalid input argument\n");
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
    gst_element_set_state (GST_ELEMENT(pipeline), GST_STATE_NULL);

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstSendBuffer (
    stBTRMgrSOGst const* pstBtrMgrSoGst
) {
    gint i = 0;

    if (!pstBtrMgrSoGst) {
        g_print ("BTRMgr_SO_GstDeInit - Invalid input argument\n");
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
    for (i = 0; i < 10; i++) {
        GstBuffer *buf;
        GstMapInfo map;

        buf = gst_buffer_new_and_alloc (100);
        gst_buffer_map (buf, &map, GST_MAP_WRITE);
        memset (map.data, i, 100);
        gst_buffer_unmap (buf, &map);

        g_print ("%d: pushing buffer for pointer %p, %p\n", i, map.data, buf);
        gst_app_src_push_buffer (GST_APP_SRC (appsrc), buf);
    }

    return eBTRMgrSOGstSuccess;
}


eBTRMgrSOGstStatus
BTRMgr_SO_GstSendEOS (
    stBTRMgrSOGst const* pstBtrMgrSoGst
) {
    if (!pstBtrMgrSoGst) {
        g_print ("BTRMgr_SO_GstDeInit - Invalid input argument\n");
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
