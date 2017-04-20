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
#define BTMGRLOG_ERROR(format...)       fprintf (stderr, format)
#define BTMGRLOG_WARN(format...)        fprintf (stderr, format)
#define BTMGRLOG_INFO(format...)        fprintf (stderr, format)
#define BTMGRLOG_DEBUG(format...)       fprintf (stderr, format)
#define BTMGRLOG_TRACE(format...)       fprintf (stderr, format)
#endif

#endif /* __BTMGR_PRIV_H__ */
