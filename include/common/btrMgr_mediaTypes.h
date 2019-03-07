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
 * @file btrMgr_mediaTypes.h
 *
 * @defgroup Media_types  Media Types
 * This file defines bluetooth manager's mediatypes for internal use.
 * @ingroup  BTR_MGR
 *
 */

#ifndef __BTR_MGR_MEDIA_TYPES_H__
#define __BTR_MGR_MEDIA_TYPES_H__

/**
 * @addtogroup  Media_types
 * @{
 *
 */

/**
 * @brief Represents the status of media player.
 */
typedef enum _eBTRMgrState {
    eBTRMgrStateInitialized,
    eBTRMgrStateDeInitialized,
    eBTRMgrStatePaused,
    eBTRMgrStatePlaying,
    eBTRMgrStateCompleted,
    eBTRMgrStateStopped,
    eBTRMgrStateWarning,
    eBTRMgrStateError,
    eBTRMgrStateUnknown
} eBTRMgrState;

/**
 * @brief Represents the audio codec types.
 */
typedef enum _eBTRMgrAType {
    eBTRMgrATypePCM,
    eBTRMgrATypeSBC,
    eBTRMgrATypeMPEG,
    eBTRMgrATypeAAC,
    eBTRMgrATypeUnknown
} eBTRMgrAType;


/**
 * @brief Represents the frequency supported.
 */
typedef enum _eBTRMgrSFreq {
    eBTRMgrSFreq8K,
    eBTRMgrSFreq16K,
    eBTRMgrSFreq32K,
    eBTRMgrSFreq44_1K,
    eBTRMgrSFreq48K,
    eBTRMgrSFreqUnknown
} eBTRMgrSFreq;

/**
 * @brief Represents the PCM bits supported.
 */
typedef enum _eBTRMgrSFmt {
    eBTRMgrSFmt8bit,
    eBTRMgrSFmt16bit,
    eBTRMgrSFmt24bit,
    eBTRMgrSFmt32bit,
    eBTRMgrSFmtUnknown
} eBTRMgrSFmt;

/**
 * @brief Represents the Audio channel supported.
 */
typedef enum _eBTRMgrAChan {
    eBTRMgrAChanMono,
    eBTRMgrAChanDualChannel,
    eBTRMgrAChanStereo,
    eBTRMgrAChanJStereo,
    eBTRMgrAChan5_1,
    eBTRMgrAChan7_1,
    eBTRMgrAChanUnknown
} eBTRMgrAChan;

/**
 * @brief Represents the PCM codec details.
 */
typedef struct _stBTRMgrPCMInfo {
    eBTRMgrSFreq  eBtrMgrSFreq;
    eBTRMgrSFmt   eBtrMgrSFmt;
    eBTRMgrAChan  eBtrMgrAChan;
} stBTRMgrPCMInfo;

/**
 * @brief Represents the Subband coding (SBC) audio codec information.
 *
 * This technique is used to connect headphones via wireless Bluetooth links.
 */
typedef struct _stBTRMgrSBCInfo {
    eBTRMgrSFreq    eBtrMgrSbcSFreq;        //!< frequency
    eBTRMgrAChan    eBtrMgrSbcAChan;        //!< channel_mode
    unsigned char   ui8SbcAllocMethod;      //!< allocation_method
    unsigned char   ui8SbcSubbands;         //!< subbands
    unsigned char   ui8SbcBlockLength;      //!< block_length
    unsigned char   ui8SbcMinBitpool;       //!< min_bitpool
    unsigned char   ui8SbcMaxBitpool;       //!< max_bitpool
    unsigned short  ui16SbcFrameLen;        //!< frameLength
    unsigned short  ui16SbcBitrate;         //!< bitrate
} stBTRMgrSBCInfo;

/**
 * @brief Represents the MPEG codec details.
 */
typedef struct _stBTRMgrMPEGInfo {
    eBTRMgrSFreq    eBtrMgrMpegSFreq;       //!< frequency
    eBTRMgrAChan    eBtrMgrMpegAChan;       //!< channel_mode
    unsigned char   ui8MpegCrc;             //!< crc
    unsigned char   ui8MpegLayer;           //!< layer
    unsigned char   ui8MpegMpf;             //!< mpf
    unsigned char   ui8MpegRfa;             //!< rfa
    unsigned short  ui16MpegBitrate;        //!< bitrate
} stBTRMgrMPEGInfo;

/**
 * @brief Represents the audio input settings.
 */
typedef struct _stBTRMgrInASettings {
    eBTRMgrAType    eBtrMgrInAType;
    void*           pstBtrMgrInCodecInfo;
    int             i32BtrMgrInBufMaxSize;
    int             i32BtrMgrDevFd;
    int             i32BtrMgrDevMtu;
} stBTRMgrInASettings;

/**
 * @brief Represents the audio output settings.
 */
typedef struct _stBTRMgrOutASettings {
    eBTRMgrAType    eBtrMgrOutAType;
    void*           pstBtrMgrOutCodecInfo;
    int             i32BtrMgrOutBufMaxSize;
    int             i32BtrMgrDevFd;
    int             i32BtrMgrDevMtu;
} stBTRMgrOutASettings;

/**
 * @brief Represents the media file status.
 */
typedef struct _stBTRMgrMediaStatus {
    eBTRMgrState    eBtrMgrState;
    eBTRMgrSFreq    eBtrMgrSFreq;
    eBTRMgrSFmt     eBtrMgrSFmt;
    eBTRMgrAChan    eBtrMgrAChan;
    unsigned int    ui32OverFlowCnt;
    unsigned int    ui32UnderFlowCnt;
    unsigned char   ui8Volume;
} stBTRMgrMediaStatus;

/** @} */


#endif /* __BTR_MGR_MEDIA_TYPES_H__ */

