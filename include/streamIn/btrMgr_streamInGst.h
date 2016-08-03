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
 * @file btrMgr_streamInGst.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces using GStreamer to external BT devices
 *
 * Copyright (c) 2015  Comcast
 */
#ifndef __BTR_MGR_STREAMIN_GST_H__
#define __BTR_MGR_STREAMIN_GST_H__

typedef void* tBTRMgrSiGstHdl;

typedef enum _eBTRMgrSIGstRet {
   eBTRMgrSIGstFailure,
   eBTRMgrSIGstFailInArg,
   eBTRMgrSIGstSuccess
} eBTRMgrSIGstRet;

/* Interfaces */
eBTRMgrSIGstRet BTRMgr_SI_GstInit (tBTRMgrSiGstHdl* phBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstDeInit (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstStart (tBTRMgrSiGstHdl hBTRMgrSoGstHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU);
eBTRMgrSIGstRet BTRMgr_SI_GstStop (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstPause (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstResume (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstSendBuffer (tBTRMgrSiGstHdl hBTRMgrSoGstHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrSIGstRet BTRMgr_SI_GstSendEOS (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);

#endif /* __BTR_MGR_STREAMIN_GST_H__ */

