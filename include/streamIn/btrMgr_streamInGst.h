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
/**
 * @file btrMgr_streamInGst.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces using GStreamer to external BT devices
 *
 */
#ifndef __BTR_MGR_STREAMIN_GST_H__
#define __BTR_MGR_STREAMIN_GST_H__

typedef void* tBTRMgrSiGstHdl;

typedef enum _eBTRMgrSIGstRet {
   eBTRMgrSIGstFailure,
   eBTRMgrSIGstFailInArg,
   eBTRMgrSIGstSuccess
} eBTRMgrSIGstRet;

typedef enum _eBTRMgrSIGstStatus {
    eBTRMgrSIGstStInitialized,
    eBTRMgrSIGstStDeInitialized,
    eBTRMgrSIGstStPaused,
    eBTRMgrSIGstStPlaying,
    eBTRMgrSIGstStUnderflow,
    eBTRMgrSIGstStOverflow,
    eBTRMgrSIGstStCompleted,
    eBTRMgrSIGstStStopped,
    eBTRMgrSIGstStWarning,
    eBTRMgrSIGstStError,
    eBTRMgrSIGstStUnknown
} eBTRMgrSIGstStatus;


/* Fptr Callbacks types */
typedef eBTRMgrSIGstRet (*fPtr_BTRMgr_SI_GstStatusCb) (eBTRMgrSIGstStatus aeBtrMgrSiGstStatus, void *apvUserData);


/* Interfaces */
eBTRMgrSIGstRet BTRMgr_SI_GstInit (tBTRMgrSiGstHdl* phBTRMgrSoGstHdl, fPtr_BTRMgr_SI_GstStatusCb afpcBSiGstStatus, void* apvUserData);
eBTRMgrSIGstRet BTRMgr_SI_GstDeInit (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstStart (tBTRMgrSiGstHdl hBTRMgrSoGstHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU, unsigned int aiBTDevSFreq);
eBTRMgrSIGstRet BTRMgr_SI_GstStop (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstPause (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstResume (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
eBTRMgrSIGstRet BTRMgr_SI_GstSendBuffer (tBTRMgrSiGstHdl hBTRMgrSoGstHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrSIGstRet BTRMgr_SI_GstSendEOS (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);

#endif /* __BTR_MGR_STREAMIN_GST_H__ */

