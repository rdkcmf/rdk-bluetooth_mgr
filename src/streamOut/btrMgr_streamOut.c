#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>

#include "btrMgr_streamOut.h"

#ifdef USE_GST1
#include "btrMgr_streamOutGst.h"
#endif


void 
BTRMgr_SO_Init (
    void
) {
#ifdef USE_GST1
    BTRMgr_SO_GstInit(0, NULL);
#endif
    return;
}
