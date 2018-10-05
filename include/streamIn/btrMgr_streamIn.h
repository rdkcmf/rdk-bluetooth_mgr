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
 * @file btrMgr_streamIn.h
 *
 * @defgroup Stream_In  Stream In Interface
 * This file defines bluetooth manager's data streaming interfaces to external bluetooth devices.
 * @ingroup  BTR_MGR
 *
 */

#ifndef __BTR_MGR_STREAMIN_H__
#define __BTR_MGR_STREAMIN_H__

typedef void* tBTRMgrSiHdl;

/**
 * @addtogroup  Stream_In
 * @{
 *
 */

/* Fptr Callbacks types */
typedef eBTRMgrRet (*fPtr_BTRMgr_SI_StatusCb) (stBTRMgrMediaStatus* apstBtrMgrSiStatus, void *apvUserData);


/* Interfaces */
/**
 * @brief This API invokes BTRMgr_SI_GstInit() for the stream in initializations.
 *
 * @param[in]  phBTRMgrSiHdl             Handle to the stream in interface.
 * @param[in]  afpcBSiStatus             Stream In callback function.
 * @param[in]  apvUserData               Data for the callback function.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_Init (tBTRMgrSiHdl* phBTRMgrSiHdl, fPtr_BTRMgr_SI_StatusCb afpcBSiStatus, void* apvUserData);

/**
 * @brief This API invokes BTRMgr_SI_GstDeInit() for the deinitializations.
 *
 * @param[in] hBTRMgrSiHdl             Handle to the stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_DeInit (tBTRMgrSiHdl hBTRMgrSiHdl);

/**
 * @brief This API is used to load the default settings used by this interface.
 *
 * @param[in]  hBTRMgrSiHdl             Handle to the stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSIGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_GetDefaultSettings (tBTRMgrSiHdl hBTRMgrSiHdl);

/**
 * @brief This API will fetch the current settings used by this interface.
 *
 * @param[in]  hBTRMgrSiHdl             Handle to the bluetooth manager stream in  interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_GetCurrentSettings (tBTRMgrSiHdl hBTRMgrSiHdl);

/**
 * @brief This API will fetch the current settings used by this interface.
 *
 * @param[in]  hBTRMgrSiHdl             Handle to the bluetooth manager stream in  interface.
 * @param[in]  pstBtrMgrSiStatus        Status of media device that has to be fetched.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_GetStatus (tBTRMgrSiHdl hBTRMgrSiHdl, stBTRMgrMediaStatus* apstBtrMgrSiStatus);

/**
 * @brief This API uses BTRMgr_SI_GstStart(), starts the pipeline.
 *
 * @param[in]  hBTRMgrSiHdl              Handle to the bluetooth manager stream in interface.
 * @param[in]  aiInBufMaxSize            Maximum buffer size.
 * @param[in]  aiBTDevFd                 Input file descriptor.
 * @param[in]  aiBTDevMTU                Block size to  read.
 * @param[in]  aiBTDevSFreq              The Clock rate.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_Start (tBTRMgrSiHdl hBTRMgrSiHdl, int aiInBufMaxSize, int aiBTDevFd, int aiBTDevMTU, unsigned int aiBTDevSFreq);

/**
 * @brief This API uses BTRMgr_SI_GstStop() for closing the pipeline.
 *
 * @param[in]  hBTRMgrSiHdl             Handle to the bluetooth manager stream in interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_Stop (tBTRMgrSiHdl hBTRMgrSiHdl);

/**
 * @brief This API uses BTRMgr_SI_GstPause() for pausing the current operation.
 *
 * @param[in]  hBTRMgrSiHdl             Handle to the bluetooth manager audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_Pause (tBTRMgrSiHdl hBTRMgrSiHdl);

/**
 * @brief This API uses BTRMgr_SI_GstResume() to resume the status.
 *
 * @param[in]  hBTRMgrSiHdl             Handle to the bluetooth manager audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_Resume (tBTRMgrSiHdl hBTRMgrSiHdl);

/**
 * @brief Invokes BTRMgr_SI_GstSendBuffer() to add the buffer to the queue.
 *
 * @param[in]  hBTRMgrSiHdl            Handle to the bluetooth manager audio capture interface.
 * @param[in]  pcInBuf                 The buffer to be added to the queue.
 * @param[in]  aiInBufSize             Buffer size.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_SendBuffer (tBTRMgrSiHdl hBTRMgrSiHdl, char* pcInBuf, int aiInBufSize);

/**
 * @brief This API is used to indicate the End of stream.
 *
 * Invokes the BTRMgr_SI_GstSendEOS() to push the end of stream.
 * Also sets the bluetooth manager status as completed.
 *
 * @param[in]  hBTRMgrSiHdl            Handle to the bluetooth manager audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SI_SendEOS (tBTRMgrSiHdl hBTRMgrSiHdl);

/** @} */
#endif /* __BTR_MGR_STREAMOUT_H__ */
