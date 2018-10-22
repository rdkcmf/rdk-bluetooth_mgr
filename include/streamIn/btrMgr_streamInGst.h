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


#define BTRMGR_AUDIO_INPUT_TYPE_SBC         "SBC"
#define BTRMGR_AUDIO_INPUT_TYPE_AAC         "MP4A-LATM"
#define BTRMGR_AUDIO_INPUT_TYPE_PCM         "PCM"
// Add additional support Audio input types

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


/**
 * @addtogroup  Stream_In
 * @{
 *
 */

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
/**
 * @brief This API initializes the streaming interface.
 *
 * Uses gstreamer element "fdsrc" for initialization.
 *
 * @param[in]  phBTRMgrSoGstHdl         Handle to the stream in interface.
 * @param[in]  afpcBSiGstStatus         Stream In callback function.
 * @param[in]  apvUserData              Data for the callback function.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstInit (tBTRMgrSiGstHdl* phBTRMgrSoGstHdl, fPtr_BTRMgr_SI_GstStatusCb afpcBSiGstStatus, void* apvUserData);

/**
 * @brief This API performs the cleanup operations.
 *
 * Cancels the threads that are running within and frees all associated memory.
 *
 * @param[in]  hBTRMgrSoGstHdl          Handle to the stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstDeInit (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);

/**
 * @brief This API starts the playback and listens to the events associated with it.
 *
 * @param[in]  hBTRMgrSoGstHdl           Handle to the stream in interface.
 * @param[in]  aiInBufMaxSize            Maximum buffer size.
 * @param[in]  aiBTDevFd                 Input file descriptor.
 * @param[in]  aiBTDevMTU                Block size to  read.
 * @param[in]  aiBTDevSFreq              The Clock rate.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstStart (tBTRMgrSiGstHdl hBTRMgrSoGstHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU, unsigned int aiBTDevSFreq, const char* apcAudioInType);

/**
 * @brief This API stops the current  playback and sets the state as NULL.
 *
 * @param[in]  hBTRMgrSoGstHdl           Handle to the stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstStop (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);

/**
 * @brief This API pauses the current playback and listens to the events.
 *
 * Checks for the current state, if it is in playing state pause state is set.
 *
 * @param[in]  hBTRMgrSoGstHdl           Handle to the stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstPause (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);

/**
 * @brief This API resumes the current operation and listens to the events.
 *
 * Checks for the current state, if it is in paused state, playing state is set.
 *
 * @param[in]  hBTRMgrSoGstHdl             Handle to the stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstResume (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);

/**
 * @brief This API pushes the buffer to the queue.
 *
 * @param[in]  hBTRMgrSoGstHdl         Handle to the stream in interface.
 * @param[in]  pcInBuf                 The buffer to be added to the queue.
 * @param[in]  aiInBufSize             Buffer size.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstSendBuffer (tBTRMgrSiGstHdl hBTRMgrSoGstHdl, char* pcInBuf, int aiInBufSize);

/**
 * @brief This API is used to push EOS(End of Stream) to the queue.
 *
 * @param[in]  hBTRMgrSoGstHdl             Handle to the stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrSIGstRet BTRMgr_SI_GstSendEOS (tBTRMgrSiGstHdl hBTRMgrSoGstHdl);
/** @} */

#endif /* __BTR_MGR_STREAMIN_GST_H__ */

