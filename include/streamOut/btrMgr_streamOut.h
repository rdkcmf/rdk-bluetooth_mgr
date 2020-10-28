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
 * @file btrMgr_streamOut.h
 *
 * @defgroup Stream_Out  StreamOut Interface
 * This file defines bluetooth manager's data streaming interfaces to external Bluetooth  devices.
 * @ingroup  BTR_MGR
 *
 */

#ifndef __BTR_MGR_STREAMOUT_H__
#define __BTR_MGR_STREAMOUT_H__

typedef void* tBTRMgrSoHdl;

/**
 * @addtogroup  Stream_Out
 * @{
 *
 */

/* Fptr Callbacks types */
typedef eBTRMgrRet (*fPtr_BTRMgr_SO_StatusCb) (stBTRMgrMediaStatus* apstBtrMgrSoStatus, void *apvUserData);


/* Interfaces */
/**
 * @brief This API invokes BTRMgr_SO_GstInit() and also set the state as initialized.
 *
 * @param[in]  phBTRMgrSoHdl            Handle to the stream out  interface.
 * @param[in]  afpcBSoStatus            Callback function.
 * @param[in]  apvUserData              Data for the callback function.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSOGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_Init (tBTRMgrSoHdl* phBTRMgrSoHdl, fPtr_BTRMgr_SO_StatusCb afpcBSoStatus, void* apvUserData);

/**
 * @brief This API invokes BTRMgr_SO_GstDeInit() for the deinitializations.
 *
 * It also set the state as deinitialized.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the stream out interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSOGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_DeInit (tBTRMgrSoHdl hBTRMgrSoHdl);

/**
 * @brief This API is used to load the default settings used by this interface.
 *
 * @param[in]  hBTRMgrSoHdl            Handle to the stream out interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSOGstSuccess  on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_GetDefaultSettings (tBTRMgrSoHdl hBTRMgrSoHdl);

/**
 * @brief This API will fetch the current settings used by this interface.
 *
 * @param[in]  hBTRMgrSoHdl            Handle to the bluetooth manager stream out   interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_GetCurrentSettings (tBTRMgrSoHdl hBTRMgrSoHdl);

/**
 * @brief This API fetches the media file status.
 *
 * @param[in]  hBTRMgrSoHdl          Handle to the bluetooth manager stream out   interface.
 * @param[out]  apstBtrMgrSoStatus    Structure which holds the status.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_GetStatus (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrMediaStatus* apstBtrMgrSoStatus);

/**
 * @brief This API will set the current settings used by this interface.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the bluetooth manager stream out interface.
 * @param[in]  pstBtrMgrSoStatus        Status of media device that has to be fetched.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_SetStatus (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrMediaStatus* apstBtrMgrSoStatus);

/**
 * @brief This API will set the current volume used by this interface.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the bluetooth manager stream out interface.
 * @param[in]  ui8Volume                Volume of media device that has to be set.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_SetVolume (tBTRMgrSoHdl hBTRMgrSoHdl, unsigned char ui8Volume);

/**
 * @brief This API will fetches the current volume used by this interface.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the bluetooth manager stream out interface.
 * @param[in]  ui8Volume                Volume of media device that has to be fetched.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_GetVolume (tBTRMgrSoHdl hBTRMgrSoHdl, unsigned char* ui8Volume);

/**
 * @brief This API will set the Mute used by this interface.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the bluetooth manager stream out interface.
 * @param[in]  Mute                     Mute of media device that has to be set.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_SetMute   (tBTRMgrSoHdl hBTRMgrSoHdl, gboolean Mute);

/**
 * @brief This API will fetches the Mute used by this interface.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the bluetooth manager stream out interface.
 * @param[in]  Mute                     Mute of media device that has to be fetched.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_GetMute   (tBTRMgrSoHdl hBTRMgrSoHdl, gboolean* Mute);

/**
 * @brief This API fetches the maximum transmission rate.
 *
 * @param[in]  hBTRMgrSoHdl              Handle to the bluetooth manager stream out   interface.
 * @param[in]  apstBtrMgrSoInASettings    Structure which holds the audio input settings.
 * @param[out] apstBtrMgrSoOutASettings   Saves the MTU information.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_GetEstimatedInABufSize (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrInASettings* apstBtrMgrSoInASettings, stBTRMgrOutASettings* apstBtrMgrSoOutASettings);
/**
 * @brief This API uses BTRMgr_SO_GstStart(), starts the pipeline.
 *
 * It also sets the state as playing.
 *
 * @param[in]  hBTRMgrSoHdl              Handle to the bluetooth manager stream out  interface.
 * @param[in]  apstBtrMgrSoInASettings   Structure which holds the audio input settings.
 * @param[in]  apstBtrMgrSoOutASettings  Structure which holds the audio output settings.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_Start (tBTRMgrSoHdl hBTRMgrSoHdl, stBTRMgrInASettings* apstBtrMgrSoInASettings, stBTRMgrOutASettings* apstBtrMgrSoOutASettings);

/**
 * @brief This API uses BTRMgr_SO_GstStop() for closing the pipeline.
 *
 * Sets the Bluetooth manager state as stopped.
 *
 * @param[in]  hBTRMgrSoHdl              Handle to the bluetooth manager stream out  interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_Stop (tBTRMgrSoHdl hBTRMgrSoHdl);

/**
 * @brief This API uses BTRMgr_SO_GstPause() for pausing the current operation.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the bluetooth manager stream out interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_Pause (tBTRMgrSoHdl hBTRMgrSoHdl);

/**
 * @brief This API uses BTRMgr_SO_GstResume() to resume the status.
 *
 * @param[in]  hBTRMgrSoHdl             Handle to the bluetooth manager stream out interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_Resume (tBTRMgrSoHdl hBTRMgrSoHdl);

/**
 * @brief Invokes BTRMgr_SO_GstSendBuffer() to add the buffer to the queue.
 *
 * @param[in]  hBTRMgrSoHdl            Handle to the bluetooth manager stream outinterface.
 * @param[in]  pcInBuf                 The buffer to be added to the queue.
 * @param[in]  aiInBufSize             Buffer size.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_SendBuffer (tBTRMgrSoHdl hBTRMgrSoHdl, char* pcInBuf, int aiInBufSize);

/**
 * @brief This API is used to indicate the End of stream.
 *
 * Invokes the BTRMgr_SO_GstSendEOS() to push the end of stream.
 * Also sets the bluetooth manager status as completed.
 *
 * @param[in]  hBTRMgrSoHdl            Handle to the bluetooth manager stream out interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_SO_SendEOS (tBTRMgrSoHdl hBTRMgrSoHdl);
/** @} */

#endif /* __BTR_MGR_STREAMOUT_H__ */
