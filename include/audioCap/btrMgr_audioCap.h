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
 * @file btrMgr_audioCap.h
 *
 * @defgroup Audio_Cap AudioCaptureInterface
 * This file defines bluetooth manager's audio capture interfaces to receiver
 * data from audio capture modules.
 * @ingroup  BTR_MGR
 *
 */

#ifndef __BTR_MGR_AUDIOCAP_H__
#define __BTR_MGR_AUDIOCAP_H__

#include "btrMgr_Types.h"

typedef void* tBTRMgrAcHdl;

typedef char* tBTRMgrAcType;


const tBTRMgrAcType BTRMGR_AC_TYPE_PRIMARY      = "primary";
const tBTRMgrAcType BTRMGR_AC_TYPE_AUXILIARY    = "auxiliary";


/* Fptr Callbacks types */
typedef eBTRMgrRet (*fPtr_BTRMgr_AC_DataReadyCb) (void* apvAcDataBuf, unsigned int aui32AcDataLen, void *apvUserData);
typedef eBTRMgrRet (*fPtr_BTRMgr_AC_StatusCb) (stBTRMgrMediaStatus* apstBtrMgrSoStatus, void *apvUserData);

/* Interfaces */
/**
 * @addtogroup  Audio_Cap
 * @{
 *
 */

/**
 * @brief This API initializes the bluetooth manager audio capture interface.
 *
 * @param[in] phBTRMgrAcHdl Handle to the bluetooth manager audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_Init (tBTRMgrAcHdl* phBTRMgrAcHdl, tBTRMgrAcType api8BTRMgrAcType);

/**
 * @brief This API deinitializes the bluetooth manager audio capture interface.
 *
 * @param[in] hBTRMgrAcHdl      Handle to the bluetooth manager audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_DeInit (tBTRMgrAcHdl hBTRMgrAcHdl);

/**
 * @brief This API will fetch default RMF AudioCapture Settings.
 *
 * Once AudioCaptureStart gets called with RMF_AudioCapture_Status, this API will
 * continue to return the default capture settings.
 *
 * @param[in]  hBTRMgrAcHdl              Handle to the bluetooth manager audio capture interface.
 * @param[out] apstBtrMgrAcOutASettings  Structure which holds the output settings.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate  error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_GetDefaultSettings (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrOutASettings* apstBtrMgrAcOutASettings);

/**
 * @brief This API fetches the current settings to capture data as part of the specific Audio capture context.
 *
 * RMF_AudioCapture_Settings has been successfully set for the context.
 *
 * @param[in]  hBTRMgrAcHdl              Handle to the bluetooth manager audio capture interface.
 * @param[out] apstBtrMgrAcOutASettings  Structure which holds the output settings.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_GetCurrentSettings (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrOutASettings* apstBtrMgrAcOutASettings);

/**
 * @brief This API fetches the status of the operation.
 *
 * @param[in]  hBTRMgrAcHdl        Handle to the bluetooth manager audio capture interface.
 * @param[out] apstBtrMgrAcStatus  Status of the operation.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_GetStatus (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrMediaStatus* apstBtrMgrAcStatus);

/**
 * @brief This function will start the Audio capture with the default capture settings.
 *
 * If RMF_AudioCapture_Settings is NULL, This function will start the Audio capture.
 * If a RMF_AudioCapture_Settings is NOT NULL then will reconfigure with the provided capture
 * settings as part of RMF_AudioCapture_Settings and start audio capture.
 *
 * @param[in]  hBTRMgrAcHdl             Handle to the bluetooth manager audio capture interface.
 * @param[out] apstBtrMgrAcOutASettings Audio capture settings.
 * @param[in]  afpcBBtrMgrAcDataReady   Call back to notify AC data ready.
 * @param[in]  apvUserData				User data of audio capture.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_Start (tBTRMgrAcHdl hBTRMgrAcHdl, stBTRMgrOutASettings* apstBtrMgrAcOutASettings, fPtr_BTRMgr_AC_DataReadyCb afpcBBtrMgrAcDataReady,  fPtr_BTRMgr_AC_StatusCb afpcBBtrMgrAcStatus, void* apvUserData);

/**
 * @brief This function will stop the audio capture.
 *
 * Start can be called again after a Stop, as long as Close has not been called.
 *
 * @param[in] hBTRMgrAcHdl      Handle to the audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_Stop (tBTRMgrAcHdl hBTRMgrAcHdl);

/**
 * @brief  This API pauses the state of audio capture.
 *
 * @param[in] hBTRMgrAcHdl     Handle to the audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_Pause (tBTRMgrAcHdl hBTRMgrAcHdl);

/**
 * @brief  This API resumes the state of audio capture.
 *
 * @param[in] hBTRMgrAcHdl     Handle to the audio capture interface.
 *
 * @return Returns the status of the operation.
 * @retval eBTRMgrSuccess on success, appropriate error code otherwise.
 */
eBTRMgrRet BTRMgr_AC_Resume (tBTRMgrAcHdl hBTRMgrAcHdl);

/** @} */

#endif /* __BTR_MGR_AUDIOCAP_H__ */

