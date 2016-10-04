#ifndef __BTMGR_PRIV_H__
#define __BTMGR_PRIV_H__

#include "btmgr.h"
#include "rdk_debug.h"
#include "stdlib.h"
#include "string.h"
#if 1
#define BTMGRLOG_ERROR(format...)       RDK_LOG(RDK_LOG_ERROR,  "LOG.RDK.BTMGR", format)
#define BTMGRLOG_WARN(format...)        RDK_LOG(RDK_LOG_WARN,   "LOG.RDK.BTMGR", format)
#define BTMGRLOG_INFO(format...)        RDK_LOG(RDK_LOG_INFO,   "LOG.RDK.BTMGR", format)
#define BTMGRLOG_DEBUG(format...)       RDK_LOG(RDK_LOG_DEBUG,  "LOG.RDK.BTMGR", format)
#define BTMGRLOG_TRACE(format...)       RDK_LOG(RDK_LOG_TRACE1, "LOG.RDK.BTMGR", format)
#else
#define BTMGRLOG_ERROR(format...)       printf (format)
#define BTMGRLOG_WARN(format...)        printf (format)
#define BTMGRLOG_INFO(format...)        printf (format)
#define BTMGRLOG_DEBUG(format...)       printf (format)
#define BTMGRLOG_TRACE(format...)       printf (format)
#endif

#endif /* __BTMGR_PRIV_H__ */
