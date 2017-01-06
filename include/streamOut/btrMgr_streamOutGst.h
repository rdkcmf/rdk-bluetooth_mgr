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
 * @file btrMgr_streamOutGst.h
 *
 * @description This file defines bluetooth manager's data streaming interfaces using GStreamer to external BT devices
 *
 */
#ifndef __BTR_MGR_STREAMOUT_GST_H__
#define __BTR_MGR_STREAMOUT_GST_H__

typedef void* tBTRMgrSoGstHdl;

#define BTRMGR_AUDIO_SFMT_SIGNED_8BIT       "S8"
#define BTRMGR_AUDIO_SFMT_SIGNED_LE_16BIT   "S16LE"
#define BTRMGR_AUDIO_SFMT_SIGNED_LE_24BIT   "S24LE"
#define BTRMGR_AUDIO_SFMT_SIGNED_LE_32BIT   "S32LE"
// Add additional sampling formats as supported by Gst SO layer

#define BTRMGR_AUDIO_CHANNELMODE_MONO       "mono"
#define BTRMGR_AUDIO_CHANNELMODE_DUAL       "dual"
#define BTRMGR_AUDIO_CHANNELMODE_STEREO     "stereo"
#define BTRMGR_AUDIO_CHANNELMODE_JSTEREO    "joint"
// Add additional chennel modes as supported by Gst SO layer

typedef enum _eBTRMgrSOGstRet {
   eBTRMgrSOGstFailure,
   eBTRMgrSOGstFailInArg,
   eBTRMgrSOGstSuccess
} eBTRMgrSOGstRet;

/* Interfaces */
eBTRMgrSOGstRet BTRMgr_SO_GstInit (tBTRMgrSoGstHdl* phBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstDeInit (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstStart (tBTRMgrSoGstHdl hBTRMgrSoGstHdl, 
                                    int ai32InBufMaxSize,
                                    const char* apcInFmt,
                                    int ai32InRate,
                                    int ai32InChannels,
                                    int ai32OutRate,
                                    int ai32OutChannels,
                                    const char* apcOutChannelMode,
                                    unsigned char aui8SbcAllocMethod,
                                    unsigned char aui8SbcSubbands,
                                    unsigned char aui8SbcBlockLength,
                                    unsigned char aui8SbcMinBitpool,
                                    unsigned char aui8SbcMaxBitpool,
                                    int ai32BTDevFd,
                                    int ai32BTDevMTU);
eBTRMgrSOGstRet BTRMgr_SO_GstStop (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstPause (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstResume (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);
eBTRMgrSOGstRet BTRMgr_SO_GstSendBuffer (tBTRMgrSoGstHdl hBTRMgrSoGstHdl, char* pcInBuf, int aiInBufSize);
eBTRMgrSOGstRet BTRMgr_SO_GstSendEOS (tBTRMgrSoGstHdl hBTRMgrSoGstHdl);

#endif /* __BTR_MGR_STREAMOUT_GST_H__ */

