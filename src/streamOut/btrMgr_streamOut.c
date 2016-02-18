#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <glib.h>

#include "btrMgr_streamOut.h"

#ifdef USE_GST1
#include "btrMgr_streamOutGst.h"
#endif

#ifdef USE_GST1
static stBTRMgrSOGst const* gstBtrMgrSoGst;
#endif

void
BTRMgr_SO_Init (
    void
) {
#ifdef USE_GST1
    gstBtrMgrSoGst = BTRMgr_SO_GstInit();
#endif
    return;
}


void
BTRMgr_SO_DeInit (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstDeInit(gstBtrMgrSoGst);
    (void) leBtrMgrSoGstStatus;
#endif
    return;
}


void
BTRMgr_SO_Start (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstStart(gstBtrMgrSoGst);
    (void) leBtrMgrSoGstStatus;
#endif
    return;
}

void
BTRMgr_SO_Stop (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstStop(gstBtrMgrSoGst);
    (void) leBtrMgrSoGstStatus;
#endif
    return;
}


void
BTRMgr_SO_SendBuffer (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstSendBuffer(gstBtrMgrSoGst);
    (void) leBtrMgrSoGstStatus;
#endif
    return;
}


void
BTRMgr_SO_SendEOS (
    void
) {
#ifdef USE_GST1
    eBTRMgrSOGstStatus leBtrMgrSoGstStatus = BTRMgr_SO_GstSendEOS(gstBtrMgrSoGst);
    (void) leBtrMgrSoGstStatus;
#endif
    return;
}
