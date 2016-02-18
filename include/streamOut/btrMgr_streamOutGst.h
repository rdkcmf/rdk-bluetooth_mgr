/**
 * @file btrMgr_streamOutGst.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces using GStreamer to external BT devices
 *
 * Copyright (c) 2015  Comcast
 */
#ifndef __BTR_MGR_STREAMOUT_GST_H__
#define __BTR_MGR_STREAMOUT_GST_H__

typedef struct _stBTRMgrSOGst {
    void* pPipeline;
    void* pSrc;
    void* pLoop;
    guint busWId;
} stBTRMgrSOGst;

typedef enum _eBTRMgrSOGstStatus {
   eBTRMgrSOGstFailure,
   eBTRMgrSOGstFailInArg,
   eBTRMgrSOGstSuccess
} eBTRMgrSOGstStatus;

/* Interfaces */
stBTRMgrSOGst const* BTRMgr_SO_GstInit (void);
eBTRMgrSOGstStatus BTRMgr_SO_GstDeInit (stBTRMgrSOGst const* pstBtrMgrSoGst);
eBTRMgrSOGstStatus BTRMgr_SO_GstStart (stBTRMgrSOGst const* pstBtrMgrSoGst);
eBTRMgrSOGstStatus BTRMgr_SO_GstStop (stBTRMgrSOGst const* pstBtrMgrSoGst);
eBTRMgrSOGstStatus BTRMgr_SO_GstSendBuffer (stBTRMgrSOGst const* pstBtrMgrSoGst);
eBTRMgrSOGstStatus BTRMgr_SO_GstSendEOS (stBTRMgrSOGst const* pstBtrMgrSoGst);

#endif /* __BTR_MGR_STREAMOUT_GST_H__ */

