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
 * @file btrMgr_streamOutGst.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces using GStreamer to external BT devices
 *
 * Copyright (c) 2015  Comcast
 */
#ifndef __BTR_MGR_STREAMOUT_GST_H__
#define __BTR_MGR_STREAMOUT_GST_H__

typedef void* tBTRMgrSoGstHdl;

typedef enum _eBTRMgrSOGstRet {
   eBTRMgrSOGstFailure,
   eBTRMgrSOGstFailInArg,
   eBTRMgrSOGstSuccess
} eBTRMgrSOGstRet;

/* Interfaces */
eBTRMgrSOGstRet BTRMgr_SO_GstInit (tBTRMgrSoGstHdl* phBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstDeInit (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstStart (tBTRMgrSoGstHdl hBTRMgrSoGstHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU);
eBTRMgrSOGstRet BTRMgr_SO_GstStop (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstPause (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstResume (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstSendBuffer (tBTRMgrSoGstHdl hBTRMgrSoGstHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrSOGstRet BTRMgr_SO_GstSendEOS (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);

#endif /* __BTR_MGR_STREAMOUT_GST_H__ */

