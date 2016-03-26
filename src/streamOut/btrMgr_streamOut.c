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
 * @file btrMgr_streamOut.c
 *
 * @description This file implements bluetooth manager's Generic streaming interface to external BT devices
 *
 * Copyright (c) 2016  Comcast
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

#ifdef USE_GST1
static tBTRMgrSoGstHdl gBTRMgrSoGstHdl = NULL;
#endif

void
BTRMgr_SO_Init (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstInit(&gBTRMgrSoGstHdl);
    g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstStatus);
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}


void
BTRMgr_SO_DeInit (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstDeInit(gBTRMgrSoGstHdl);
    g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstStatus);
    gBTRMgrSoGstHdl = NULL;
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}


void
BTRMgr_SO_Start (
    int aiInBufMaxSize,
    int aiBTDevFd,
    int aiBTDevMTU
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstStart(gBTRMgrSoGstHdl, aiInBufMaxSize, aiBTDevFd, aiBTDevMTU);
    g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstStatus);
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}

void
BTRMgr_SO_Stop (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstStop(gBTRMgrSoGstHdl);
    g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstStatus);
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}

void
BTRMgr_SO_Pause (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstPause(gBTRMgrSoGstHdl);
    g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstStatus);
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}

void
BTRMgr_SO_Resume (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstResume(gBTRMgrSoGstHdl);
    g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstStatus);
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}



void
BTRMgr_SO_SendBuffer (
    char* pcInBuf,
    int   aiInBufSize
) {
    //TODO: Implement ping-pong/triple/circular buffering if needed
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstSendBuffer(gBTRMgrSoGstHdl, pcInBuf, aiInBufSize);
    (void)leBtrMgrSoGstStatus;
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}


void
BTRMgr_SO_SendEOS (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstSendEOS(gBTRMgrSoGstHdl);
    g_print("%s:%d:%s - Return Status = %d\n", __FILE__, __LINE__, __FUNCTION__, leBtrMgrSoGstStatus);
#else
    // TODO: Implement Stream out functionality using generic libraries
#endif
    return;
}
