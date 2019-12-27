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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "btmgr.h"
#include "rfcapi.h"

/* Tile Specific TOA msg feilds */
# define _1_BYTE_TOA_CID                    "00"
# define _4_BYTE_TOA_SESSION_TOKEN          "00000000"
# define _14_BYTE_TOA_RAND_A                "0000000000000000000000000000"
# define _14_BYTE_TOA_MSG_PAYLOAD           "0000000000000000000000000000"
# define _14_BYTE_TOA_SONG_PAYLOAD          "020103"
# define _4_BYTE_TOA_MIC                    "00000000"

# define _1_BYTE_TOA_CMD_OPEN_CHANNEL       "10"
# define _1_BYTE_TOA_CMD_READY              "12"
# define _1_BYTE_TOA_CMD_SONG_PLAY          "05"

# define _1_BYTE_TOA_RSP_OPEN_CHANNEL       "12"
# define _1_BYTE_TOA_RSP_READY              "01"

volatile int           wait                  = 1;
volatile unsigned char gReceivedNotification = 0;
char gNotificationData[BTRMGR_MAX_STR_LEN]   = "\0";
int     uselection    = 0;
int     cliDisabled   = 0;
int     cliArgCounter = 0;
char**  gArgv;
int     gArgc;
BTRMgrDeviceHandle gDeviceHandle = 0;

BTRMGR_LeCustomAdvertisement_t stCustomAdv =
{
    0x02 ,
    0x01 ,
    0x06 ,
    0x05 ,
    0x03 ,
    0x0A ,
    0x18 ,
    0xB9 ,
    0xFD ,
    0x0B ,
    0xFF ,
    0xA3 ,
    0x07 ,
    0x0101,
    {
        0xC8,
        0xB3,
        0x73,
        0x32,
        0xEA,
        0x3D
    }
};

static void printOptions (void)
{
    printf ("\n\n");
    printf (" 1. Get Number of Adapter\n");
    printf (" 2. Set Name to your Adapter\n");
    printf (" 3. Get Name of your Adapter\n");
    printf (" 4. Set Adapter Power; 0 to OFF & 1 to ON\n");
    printf (" 5. Get Adapter Power\n");
    printf (" 6. Set Discoverable\n");
    printf (" 7. Is it Discoverable\n");
    printf (" 8. Start Discovering\n");
    printf (" 9. Stop Discovering\n");
    printf ("10. Get List of Discovered Devices\n");
    printf ("11. Pair a Device\n");
    printf ("12. UnPair a Device\n");
    printf ("13. Get List of Paired Devices\n");
    printf ("14. Connect to Device\n");
    printf ("15. DisConnect from Device\n");
    printf ("16. Get Device Properties\n");
    printf ("17. Start Streaming\n");
    printf ("18. Stop Streaming\n");
    printf ("19. Get StreamingOut Status\n");
    printf ("20. Check auto connection of external Device\n");
    printf ("21. Accept External Pair Request\n");
    printf ("22. Deny External Pair Request\n");
    printf ("23. Accept External Connect Request\n");
    printf ("24. Deny External Connect Request\n");
    printf ("25. Accept External Playback Request\n");
    printf ("26. Deny External Playback Request\n");
    printf ("27. Start Streaming-In\n");
    printf ("28. Stop Streaming-In\n");
    printf ("29. Perform Media Control Options\n");
    printf ("30. Get Current Media Track Information\n");
    printf ("31. Get Media Current Play Position\n"); 
    printf ("32. Get StreamingIn Status\n"); 
    printf ("33. Get LE Property\n");
    printf ("34. Perform LE Operation\n");
    printf ("35. Reset Bluetooth Adapter\n");
    printf ("36. Get Discovery State \n");
    printf ("37. Set StreamOut Type Auxiliary\n");
    printf ("40. Ring A Tile (Just for POC)\n");
    printf ("41. (RFC)Set AudioIn - Enabled/Disabled\n");
    printf ("42. Set AudioIn - Enabled/Disabled\n");
    printf ("43. Set Media Element Active\n");
    printf ("44. Get Media Element List\n");
    printf ("45. Select Media Element to Play/Explore\n");
    printf ("50. Set advertisement data and services and start advertisement\n");
    printf ("51. Add service/gatt/descriptor info\n");
    printf ("52. Stop Advertisement\n");
    printf ("53. Begin advertisement for Xcam2\n");
    printf ("55. Quit\n");
    printf ("\n\n");
    printf ("Please enter the option that you want to test\t");

    return;
}

static void printOptionsCli (void)
{
    printf ("\n\n");
    printf (" 1. Get Number of Adapter\n Usage: btrMgrTest 1\n");
    printf (" 2. Set Name to your Adapter\n Usage: btrMgrTest 2 <Name>\n");
    printf (" 3. Get Name of your Adapter\n Usage: btrMgrTest 3\n");
    printf (" 4. Set Adapter Power; 0 to OFF & 1 to ON\n Usage: btrMgrTest 4 <PowerMode>\n");
    printf (" 5. Get Adapter Power\n Usage: btrMgrTest 5\n");
    printf (" 6. Set Discoverable\n Usage: btrMgrTest 6 <Discoverable> <TimeOut> \n 1 or 0 to Make it Discoverable ON or OFF\n");
    printf (" 7. Is it Discoverable\n Usage: btrMgrTest 7\n");
    printf (" 8. Start Discovering\n Usage: btrMgrTest 8 <ScanType> \n 0 - Normal(BR/EDR) | 1 - LE (BLE)\n");
    printf (" 9. Stop Discovering\n Usage: btrMgrTest 9 <ScanType> \n 0 - Normal(BR/EDR) | 1 - LE (BLE)\n");
    printf ("10. Get List of Discovered Devices\n Usage: btrMgrTest 10\n");
    printf ("11. Pair a Device\n Usage: btrMgrTest 11 <Handle>\n");
    printf ("12. UnPair a Device\n Usage: btrMgrTest 12 <Handle>\n");
    printf ("13. Get List of Paired Devices\n Usage: btrMgrTest 13\n");
    printf ("14. Connect to Device\n Usage: btrMgrTest 14 <Handle> <ConnectType>\n Device ConnectAs  Type : [0 - AUDIO_OUTPUT | 1 - AUDIO_INPUT | 2 - LE ]\n");
    printf ("15. DisConnect from Device\n Usage: btrMgrTest 15 <Handle>\n");
    printf ("16. Get Device Properties\n Usage: btrMgrTest 16 <Handle>\n");
    printf ("17. Start Streaming\n Usage: btrMgrTest 17 <Handle> <StreamingPref>\n[0 - AUDIO_OUTPUT | 1 - AUDIO_INPUT | 2 - LE ]\n");
    printf ("18. Stop Streaming\n Usage: btrMgrTest 18 <Handle>\n");
    printf ("19. Get StreamingOut Status\n Usage: btrMgrTest 19 \n");
    printf ("20. Check auto connection of external Device\n Usage: btrMgrTest 20 \n");
    printf ("21. Accept External Pair Request\n Usage: btrMgrTest 21 \n");
    printf ("22. Deny External Pair Request\nUsage: btrMgrTest 22 \n");
    printf ("23. Accept External Connect Request\n Usage: btrMgrTest 23 \n");
    printf ("24. Deny External Connect Request\n Usage: btrMgrTest 24 \n");
    printf ("25. Accept External Playback Request\n Usage: btrMgrTest 25 \n");
    printf ("26. Deny External Playback Request\n Usage: btrMgrTest 26 \n");
    printf ("27. Start Streaming-In\n Usage: btrMgrTest 27 <Handle> <StreamingPref> \n");
    printf ("28. Stop Streaming-In\n Usage: btrMgrTest 28 <Handle> \n");
    printf ("29. Perform Media Control Options\n Usage: btrMgrTest 29 <Handle> <MediaControlOpt> \n Medio Control Options [0 - Play | 1 - Pause | 2 - Stop | 3 - Next  | 4 - Previous \n");
    printf ("30. Get Current Media Track Information\n Usage: btrMgrTest 30 <Handle> \n");
    printf ("31. Get Media Current Play Position\n Usage: btrMgrTest 31 <Handle>\n"); 
    printf ("32. Get StreamingIn Status\n Usage: btrMgrTest 32 \n"); 
    printf ("33. Get LE Property\n Usage: btrMgrTest 33 <Handle> <UUID> <Property>\n property to query: [0 - Uuid | 1 - Primary | 2 - Device | 3 - Service | 4 - Value | 5 - Notifying | 6 - Flags | 7 -Character]\n");
    printf ("34. Perform LE Operation\n Usage: btrMgrTest 34 <Handle> <UUID> <Option> \nEnter Option : [ReadValue - 0 | WriteValue - 1 | StartNotify - 2 | StopNotify - 3]\n");
    printf ("35. Reset Bluetooth Adapter\n Usage: btrMgrTest 35\n");
    printf ("36. Get Discovery State \n");
    printf ("37. Set StreamOut Type Auxiliary\n Usage: btrMgrTest 37\n");
    printf ("40. Ring A Tile (Just for POC)\n");
    printf ("55. Quit\n");
    printf ("\n\n");

    return;
}

static int getUserSelection (void)
{
    int mychoice = 0;
    if (cliDisabled)
    {
    printf("Enter a choice...\n");
    scanf("%d", &mychoice);
    getchar();//to catch newline
    }
    else
    {
    cliArgCounter++;
    if (cliArgCounter < gArgc){
       mychoice = atoi(gArgv[cliArgCounter]);
    }
    else{
       printf("\n No Value entered , Sending 0\n");
    }
    }
    return mychoice;
}

static BTRMgrDeviceHandle getDeviceSelection(void)
{
    BTRMgrDeviceHandle mychoice = 0;
    if (cliDisabled)
    {
    printf("Enter a choice...\n");
    scanf("%llu", &mychoice);
    getchar();//to catch newline
    }
    else
    {
        cliArgCounter++;
        if (cliArgCounter < gArgc){
           mychoice = strtoll(gArgv[cliArgCounter],NULL, 0);
        }
        else{
           printf("\n No Value entered , Sending 0\n");
        }

    }
    return mychoice;
}


void getString (char* mychoice)
{
    if (cliDisabled)
    {
    char *tmp = NULL;
    fgets (mychoice, BTRMGR_NAME_LEN_MAX, stdin);
    tmp = strchr(mychoice, '\n');
    if (tmp)
        *tmp = '\0';
    }
    else
    {
        cliArgCounter++;
        if (cliArgCounter < gArgc){
           strcpy(mychoice , gArgv[cliArgCounter]);
        }
        else{
           printf("\n No Value entered , Sending NULL \n");
        }
    }
}

static int
getBitsToString (
    unsigned short  flagBits
) {
    unsigned char i = 0;
    for (; i<15; i++) {
        printf("%d", (flagBits >> i) & 1);
    }
    return 0;
}


const char* getEventAsString (BTRMGR_Events_t etype)
{
  char *event = "\0";
  switch(etype)
  {
    case BTRMGR_EVENT_DEVICE_OUT_OF_RANGE               : event = "DEVICE_OUT_OF_RANGE_OR_LOST";         break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_STARTED          : event = "DEVICE_DISCOVERY_STARTED";            break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE           : event = "DEVICE_DISCOVERY_UPDATE";             break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE         : event = "DEVICE_DISCOVERY_COMPLETE";           break;
    case BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE           : event = "DEVICE_PAIRING_COMPLETE";             break; 
    case BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE         : event = "DEVICE_UNPAIRING_COMPLETE";           break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE        : event = "DEVICE_CONNECTION_COMPLETE";          break;
    case BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE        : event = "DEVICE_DISCONNECT_COMPLETE";          break;
    case BTRMGR_EVENT_DEVICE_PAIRING_FAILED             : event = "DEVICE_PAIRING_FAILED";               break;
    case BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED           : event = "DEVICE_UNPAIRING_FAILED";             break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_FAILED          : event = "DEVICE_CONNECTION_FAILED";            break;     
    case BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED          : event = "DEVICE_DISCONNECT_FAILED";            break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST    : event = "RECEIVED_EXTERNAL_PAIR_REQUEST";      break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST : event = "RECEIVED_EXTERNAL_CONNECT_REQUEST";   break;
    case BTRMGR_EVENT_DEVICE_FOUND                      : event = "DEVICE_FOUND";                        break;
    case BTRMGR_EVENT_MEDIA_TRACK_STARTED               : event = "MEDIA_TRACK_STARTED";                 break;
    case BTRMGR_EVENT_MEDIA_TRACK_PLAYING               : event = "MEDIA_TRACK_PLAYING";                 break;
    case BTRMGR_EVENT_MEDIA_TRACK_PAUSED                : event = "MEDIA_TRACK_PAUSED";                  break;
    case BTRMGR_EVENT_MEDIA_TRACK_STOPPED               : event = "MEDIA_TRACK_STOPPED";                 break;
    case BTRMGR_EVENT_MEDIA_TRACK_POSITION              : event = "MEDIA_TRACK_POSITION";                break;
    case BTRMGR_EVENT_MEDIA_TRACK_CHANGED               : event = "MEDIA_TRACK_CHANGED";                 break;
    case BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED              : event = "MEDIA_PLAYBACK_ENDED";                break;
    case BTRMGR_EVENT_DEVICE_OP_READY                   : event = "DEVICE_OP_READY";                     break;
    case BTRMGR_EVENT_DEVICE_OP_INFORMATION             : event = "DEVICE_OP_INFORMATION";               break;
    case BTRMGR_EVENT_MEDIA_PLAYER_NAME                 : event = "MEDIA_PLAYER_NAME";                   break;
    case BTRMGR_EVENT_MEDIA_PLAYER_VOLUME               : event = "MEDIA_PLAYER_VOLUME";                 break;
    case BTRMGR_EVENT_MEDIA_PLAYER_EQUALIZER_OFF        : event = "MEDIA_PLAYER_EQUALIZER_OFF";          break;
    case BTRMGR_EVENT_MEDIA_PLAYER_EQUALIZER_ON         : event = "MEDIA_PLAYER_EQUALIZER_ON";           break;
    case BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_OFF          : event = "MEDIA_PLAYER_SHUFFLE_OFF";            break;
    case BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_ALLTRACKS    : event = "MEDIA_PLAYER_SHUFFLE_ALLTRACKS";      break;
    case BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_GROUP        : event = "MEDIA_PLAYER_SHUFFLE_GROUP";          break;
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_OFF           : event = "MEDIA_PLAYER_REPEAT_OFF";             break;
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_SINGLETRACK   : event = "MEDIA_PLAYER_REPEAT_SINGLETRACK";     break;
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_ALLTRACKS     : event = "MEDIA_PLAYER_REPEAT_ALLTRACKS";       break;
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_GROUP         : event = "MEDIA_PLAYER_REPEAT_GROUP";           break;
    case BTRMGR_EVENT_MEDIA_ALBUM_INFO                  : event = "MEDIA_PLAYER_ALBUM_INFO";             break;
    case BTRMGR_EVENT_MEDIA_ARTIST_INFO                 : event = "MEDIA_PLAYER_ARTIST_INFO";            break;
    case BTRMGR_EVENT_MEDIA_GENRE_INFO                  : event = "MEDIA_PLAYER_GENRE_INFO";             break;
    case BTRMGR_EVENT_MEDIA_COMPILATION_INFO            : event = "MEDIA_PLAYER_COMPILATION_INFO";       break;
    case BTRMGR_EVENT_MEDIA_PLAYLIST_INFO               : event = "MEDIA_PLAYER_PLAYLIST_INFO";          break;
    case BTRMGR_EVENT_MEDIA_TRACKLIST_INFO              : event = "MEDIA_PLAYER_TRACKLIST_INFO";         break;
    default                                            : event = "##INVALID##";
  }
  return event;
}


BTRMGR_Result_t eventCallback (BTRMGR_EventMessage_t event)
{
#if 0
    printf ("\n\t@@@@@@@@ %s : %s eventCallback ::::  Event ID %d @@@@@@@@\n", BTRMGR_GetDeviceTypeAsString(event.m_pairedDevice.m_deviceType)
                                                                             , event.m_pairedDevice.m_name
                                                                             , event.m_eventType);
#endif
    switch(event.m_eventType) {
    case BTRMGR_EVENT_DEVICE_OUT_OF_RANGE: 
        printf("\tReceived %s Event from BTRMgr\n", getEventAsString(event.m_eventType));
        printf("\tYour device %s has either been Lost or Out of Range\n", event.m_pairedDevice.m_name);
        break;
    case BTRMGR_EVENT_DEVICE_FOUND: 
        printf("\tReceived %s Event from BTRMgr\n", getEventAsString(event.m_eventType));
        printf("\tYour device %s is Up and Ready\n", event.m_pairedDevice.m_name);

        if(event.m_pairedDevice.m_isLastConnectedDevice) {
            if( 20 == uselection ) {
                printf("\tDo you want to Connect? (1 for Yes / 0 for No)\n\t");
                if ( getUserSelection() ) {
                    if (BTRMGR_StartAudioStreamingOut(0, event.m_pairedDevice.m_deviceHandle, 1) == BTRMGR_RESULT_SUCCESS)
                        printf ("\tConnection Success....\n");
                    else
                        printf ("\tConnection Failed.....\n");
                }
                else {
                    printf ("\tDevice Connection skipped\n");
                }
                wait = 0;
            }
            else {
                printf("\tDefault Action: Accept connection from Last connected device..\n");
                BTRMGR_StartAudioStreamingOut(0, event.m_pairedDevice.m_deviceHandle, 1);
            }
        }
        break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST:
        printf ("\tReceiver External Pair Request\n");
        printf ("\t DevHandle =  %lld\n", event.m_externalDevice.m_deviceHandle);
        printf ("\t DevName   = %s\n", event.m_externalDevice.m_name);
        printf ("\t DevAddr   = %s\n", event.m_externalDevice.m_deviceAddress);
        printf ("\t PassCode  = %06d\n", event.m_externalDevice.m_externalDevicePIN);
        if (event.m_externalDevice.m_requestConfirmation) {
            printf ("\t Enter Option 21 to Accept Pairing Request\n");
            printf ("\t Enter Option 22 to Deny Pairing Request\n");
            gDeviceHandle = event.m_externalDevice.m_deviceHandle;
        }
        else {
            printf("\n\n\t@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
            printf("\tEnter PIN: %06d in Your \"%s\" to make them paired\n", event.m_externalDevice.m_externalDevicePIN, event.m_externalDevice.m_name);
            printf("\t@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n\n");
        }
        break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST:
        printf ("\tReceiver External Connect Request\n");
        printf ("\t DevHandle =  %lld\n", event.m_externalDevice.m_deviceHandle);
        printf ("\t DevName   = %s\n", event.m_externalDevice.m_name);
        printf ("\t DevAddr   = %s\n", event.m_externalDevice.m_deviceAddress);
        printf ("\t Enter Option 23 to Accept Connect Request\n");
        printf ("\t Enter Option 24 to Deny Connect Request\n");
        gDeviceHandle = event.m_externalDevice.m_deviceHandle;
        break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST:
        printf ("\tReceiver External Playback Request\n");
        printf ("\t DevHandle =  %lld\n", event.m_externalDevice.m_deviceHandle);
        printf ("\t DevName   = %s\n", event.m_externalDevice.m_name);
        printf ("\t DevAddr   = %s\n", event.m_externalDevice.m_deviceAddress);
        printf ("\t Enter Option 25 to Accept Playback Request\n");
        printf ("\t Enter Option 26 to Deny Playback Request\n");
        gDeviceHandle = event.m_externalDevice.m_deviceHandle;
        break;
    case BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE:
    case BTRMGR_EVENT_DEVICE_PAIRING_FAILED:
        printf("\tReceived %s %s Event from BTRMgr\n", event.m_discoveredDevice.m_name, getEventAsString(event.m_eventType));
        printf("\t DevHandle = %lld\n", event.m_discoveredDevice.m_deviceHandle);
        printf("\t DevType   = %s\n", BTRMGR_GetDeviceTypeAsString(event.m_discoveredDevice.m_deviceType));
        printf("\t DevAddr   = %s\n", event.m_discoveredDevice.m_deviceAddress);
        break;
    case BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE:
    case BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED:
    case BTRMGR_EVENT_DEVICE_CONNECTION_FAILED:
    case BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED:
    case BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE:
    case BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE:
        printf("\tReceived %s %s Event from BTRMgr\n", event.m_pairedDevice.m_name, getEventAsString(event.m_eventType));
        printf("\t DevHandle = %lld\n", event.m_pairedDevice.m_deviceHandle);
        printf("\t DevType   = %s\n", BTRMGR_GetDeviceTypeAsString(event.m_pairedDevice.m_deviceType));
        printf("\t DevAddr   = %s\n", event.m_pairedDevice.m_deviceAddress);
        break;
    case BTRMGR_EVENT_MEDIA_TRACK_STARTED:
    case BTRMGR_EVENT_MEDIA_TRACK_PLAYING:
    case BTRMGR_EVENT_MEDIA_TRACK_PAUSED:
    case BTRMGR_EVENT_MEDIA_TRACK_STOPPED:
    case BTRMGR_EVENT_MEDIA_TRACK_POSITION:
        printf("\t[%s]   at position   %d.%02d   of   %d.%02d\n", getEventAsString(event.m_eventType)
                                                                , event.m_mediaInfo.m_mediaPositionInfo.m_mediaPosition/60000
                                                                , (event.m_mediaInfo.m_mediaPositionInfo.m_mediaPosition/1000)%60
                                                                , event.m_mediaInfo.m_mediaPositionInfo.m_mediaDuration/60000
                                                                , (event.m_mediaInfo.m_mediaPositionInfo.m_mediaDuration/1000)%60);    
        break;
    case BTRMGR_EVENT_MEDIA_TRACK_CHANGED:
        printf("\tRecieved %s Event from BTRMgr\n", getEventAsString(event.m_eventType));
        printf("\tDevice %s changed track successfully\n", event.m_mediaInfo.m_name);
        printf ("\t---Current Media Track Info--- \n"
                "\tAlbum           : %s\n"
                "\tArtist          : %s\n"
                "\tTitle           : %s\n"
                "\tGenre           : %s\n"
                "\tNumberOfTracks  : %d\n"
                "\tTrackNumber     : %d\n"
                "\tDuration        : %d\n\n"
                , event.m_mediaInfo.m_mediaTrackInfo.pcAlbum
                , event.m_mediaInfo.m_mediaTrackInfo.pcArtist
                , event.m_mediaInfo.m_mediaTrackInfo.pcTitle
                , event.m_mediaInfo.m_mediaTrackInfo.pcGenre
                , event.m_mediaInfo.m_mediaTrackInfo.ui32NumberOfTracks
                , event.m_mediaInfo.m_mediaTrackInfo.ui32TrackNumber
                , event.m_mediaInfo.m_mediaTrackInfo.ui32Duration);

        break;
    case BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED:
        printf("\tRecieved %s Event from BTRMgr\n", getEventAsString(event.m_eventType));
        printf("\tDevice %s ended streaming successfully\n", event.m_mediaInfo.m_name);
        break;
    case BTRMGR_EVENT_DEVICE_OP_READY:
        printf("\tRecieved %s Event from BTRMgr\n", getEventAsString(event.m_eventType));
        printf("\tDevice %s Op Ready\n", event.m_deviceOpInfo.m_name);
        break;
    case BTRMGR_EVENT_DEVICE_OP_INFORMATION:
        printf("\tRecieved %s Event from BTRMgr\n", getEventAsString(event.m_eventType));
        printf("\tDevice %s Op Information\n", event.m_deviceOpInfo.m_name);
        printf("\tUUID is %s\n", event.m_deviceOpInfo.m_uuid);
        if (BTRMGR_DEVICE_TYPE_TILE == event.m_deviceOpInfo.m_deviceType)
        {
            printf("\t%s\n", event.m_deviceOpInfo.m_notifyData);
        }
        else if(BTRMGR_LE_OP_WRITE_VALUE == event.m_deviceOpInfo.m_leOpType)
        {
            printf("\t%s\n", event.m_deviceOpInfo.m_writeData);
            {
                BTRMGR_SysDiagInfo(0, event.m_deviceOpInfo.m_uuid, event.m_deviceOpInfo.m_writeData, event.m_deviceOpInfo.m_leOpType);
            }
        }
        else if (BTRMGR_LE_OP_READ_VALUE == event.m_deviceOpInfo.m_leOpType)
        {
            BTRMGR_SysDiagInfo(0, event.m_deviceOpInfo.m_uuid, event.m_deviceOpInfo.m_writeData, event.m_deviceOpInfo.m_leOpType);
            printf("Writing %s for UUID %s\n", event.m_deviceOpInfo.m_writeData, event.m_deviceOpInfo.m_uuid);

            /* Send event response */
            BTRMGR_EventResponse_t  lstBtrMgrEvtRsp;
            
            memset(&lstBtrMgrEvtRsp, 0, sizeof(lstBtrMgrEvtRsp));
            lstBtrMgrEvtRsp.m_eventResp = 1;
            lstBtrMgrEvtRsp.m_eventType = BTRMGR_EVENT_DEVICE_OP_INFORMATION;
            strncpy(lstBtrMgrEvtRsp.m_writeData, event.m_deviceOpInfo.m_writeData, BTRMGR_MAX_DEV_OP_DATA_LEN - 1);
            if (BTRMGR_RESULT_SUCCESS != BTRMGR_SetEventResponse(0, &lstBtrMgrEvtRsp)) {
                printf("Failed to send event response");
            }
            gDeviceHandle = 0;
        }
        break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE:
        printf ("\n\tDiscovered %s device of type %s\n", event.m_discoveredDevice.m_name, BTRMGR_GetDeviceTypeAsString(event.m_discoveredDevice.m_deviceType));
        break;
    case BTRMGR_EVENT_MEDIA_PLAYER_NAME:
        printf("\n\t[%s] set  %s  %s \n", event.m_mediaInfo.m_name, getEventAsString(event.m_eventType), event.m_mediaInfo.m_mediaPlayerName);
        break;
    case BTRMGR_EVENT_MEDIA_PLAYER_VOLUME:
        printf("\n\t[%s] set  %s  %d%% \n", event.m_mediaInfo.m_name, getEventAsString(event.m_eventType), event.m_mediaInfo.m_mediaPlayerVolumeInPercentage);
        break;
    case BTRMGR_EVENT_MEDIA_PLAYER_EQUALIZER_OFF:
    case BTRMGR_EVENT_MEDIA_PLAYER_EQUALIZER_ON:
    case BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_OFF:
    case BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_ALLTRACKS:
    case BTRMGR_EVENT_MEDIA_PLAYER_SHUFFLE_GROUP:
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_OFF:
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_SINGLETRACK:
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_ALLTRACKS:
    case BTRMGR_EVENT_MEDIA_PLAYER_REPEAT_GROUP:
        printf("\n\t[%s] set  %s\n", event.m_mediaInfo.m_name, getEventAsString(event.m_eventType));
        break;
        break;
    case BTRMGR_EVENT_MEDIA_ALBUM_INFO:
    case BTRMGR_EVENT_MEDIA_ARTIST_INFO:
    case BTRMGR_EVENT_MEDIA_GENRE_INFO:
    case BTRMGR_EVENT_MEDIA_COMPILATION_INFO:
    case BTRMGR_EVENT_MEDIA_PLAYLIST_INFO:
    case BTRMGR_EVENT_MEDIA_TRACKLIST_INFO:
        if (event.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_IsPlayable) {
            printf("\t%50s\t\t%020llu\n", event.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_mediaTrackInfo.pcTitle
                                        , event.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_mediaElementHdl);
        }
        else {
            printf("\t%50s\t\t%020llu\n", event.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_mediaElementName
                                        , event.m_mediaInfo.m_mediaTrackListInfo.m_mediaElementInfo[0].m_mediaElementHdl);
        }
        break;

     default:
        printf("\tReceived %s Event from BTRMgr\n", getEventAsString(event.m_eventType));  
        break;
    }
                                      
    return BTRMGR_RESULT_SUCCESS;
}


int main(int argc, char *argv[])
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    int loop = 1, i = 0;
    char array[32] = "";
    BTRMgrDeviceHandle handle = 0;
    int counter;
    size_t sz;

    rc = BTRMGR_Init();

    if (BTRMGR_RESULT_SUCCESS != rc)
    {
        printf ("Failed to init BTRMgr.. Quiting.. \n");
        loop = 0;
        return 0;
    }

    BTRMGR_RegisterEventCallback (eventCallback);
    if(argc==1){
        printf("\nNo Extra Command Line Argument Passed Other Than Program Name");
        cliDisabled = 1;
    }
    else
    {
        printf("\n Executing in CLI mode\n");
            printOptionsCli();
        gArgv = malloc(argc * sizeof (char*));
        gArgc = argc;
        for (counter = 0; counter < argc; ++counter) {
            sz = strlen(argv[counter]) + 1;
            gArgv[counter] = malloc(sz * sizeof (char));
            strcpy(gArgv[counter], argv[counter]);
        }

        for (counter = 0; counter < argc; ++counter) {
            printf("%s\n", gArgv[counter]);
        }

    }
    do
    {
        printOptions();
        i = getUserSelection();
        switch (i)
        {
            case 1:
                {
                    unsigned char numOfAdapters = 0;
                    rc = BTRMGR_GetNumberOfAdapters(&numOfAdapters);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... Count = %d\n", numOfAdapters);
                }
                break;
            case 2:
                {
                    memset (array, '\0', sizeof(array));
                    printf ("Please Enter the name that you want to set to your adapter\t: ");
                    getString(array);

                    printf ("We received @@%s@@ from you..  Hope this is correct. Let me try to set it..\n", array);
                    rc = BTRMGR_SetAdapterName(0, array);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 3:
                {
                    memset (array, '\0', sizeof(array));
                    rc = BTRMGR_GetAdapterName(0, array);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("We received @@%s@@\n", array);
                }
                break;
            case 4:
                {
                    unsigned char power_status = 0;

                    printf ("Please set the power status \t");
                    power_status = (unsigned char) getUserSelection();

                    rc = BTRMGR_SetAdapterPowerStatus(0, power_status);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success\n");
                }
                break;
            case 5:
                {
                    unsigned char power_status = 0;

                    rc = BTRMGR_GetAdapterPowerStatus (0, &power_status);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success; Status = %u\n", power_status);
                }
                break;
            case 6:
                {
                    unsigned char power_status = 0;
                    int timeout = -1;

                    printf ("Please enter 1 or 0 to Make it Discoverable ON or OFF \t");
                    power_status = (unsigned char) getUserSelection();

                    printf ("Please set the timeout for the discoverable \t");
                    timeout = (int) getUserSelection();
                    printf ("timeout = %d\t\n",timeout);

                    rc = BTRMGR_SetAdapterDiscoverable(0, power_status, timeout);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 7:
                {
                    unsigned char power_status = 0;

                    rc = BTRMGR_IsAdapterDiscoverable(0, &power_status);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success; Status = %u\n", power_status);
                }
                break;
            case 8:
                {
                    int ch = 0;
                    BTRMGR_DeviceOperationType_t discoveryType = BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT;
                    printf ("Enter Scan Type : [0 - Normal(BR/EDR) | 1 - LE (BLE) | 2 - HID ]\n");
                    ch = getDeviceSelection();
                    if (0 == ch)
                        discoveryType = BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT;
                    else if (1 == ch)
                        discoveryType = BTRMGR_DEVICE_OP_TYPE_LE;
                    else if (2 == ch)
                        discoveryType = BTRMGR_DEVICE_OP_TYPE_HID;

                    rc = BTRMGR_StartDeviceDiscovery(0, discoveryType);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 9:
                {
                    int ch = 0;
                    BTRMGR_DeviceOperationType_t discoveryType = BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT;
                    printf ("Enter Scan Type : [0 - Normal(BR/EDR) | 1 - LE (BLE) | 2 - HID ]\n");
                    ch = getDeviceSelection();
                    if (0 == ch)
                        discoveryType = BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT;
                    else if (1 == ch)
                        discoveryType = BTRMGR_DEVICE_OP_TYPE_LE;
                    else if (2 == ch)
                        discoveryType = BTRMGR_DEVICE_OP_TYPE_HID;

                    rc = BTRMGR_StopDeviceDiscovery(0, discoveryType);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 10:
                {
                    BTRMGR_DiscoveredDevicesList_t discoveredDevices;

                    memset (&discoveredDevices, 0, sizeof(discoveredDevices));
                    rc = BTRMGR_GetDiscoveredDevices(0, &discoveredDevices);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                    {
                        int j = 0;
                        printf ("\nSuccess....   Discovered Devices (%d) are, \n", discoveredDevices.m_numOfDevices);
                        printf ("\n\tSN %-17s %-30s %-17s   %s\n\n", "Device Id", "Device Name", "Device Address", "Device Type");
                        for (; j< discoveredDevices.m_numOfDevices; j++)
                        {
                            printf ("\t%02d %-17llu %-30s %17s   %s\n", j,
                                                              discoveredDevices.m_deviceProperty[j].m_deviceHandle,
                                                              discoveredDevices.m_deviceProperty[j].m_name,
                                                              discoveredDevices.m_deviceProperty[j].m_deviceAddress,
                                                              BTRMGR_GetDeviceTypeAsString(discoveredDevices.m_deviceProperty[j].m_deviceType));
                        }
                        printf ("\n\n");
                    }
                }
                break;
            case 11:
                {
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to pair \t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_PairDevice(0, handle);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 12:
                {
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to Unpair \t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_UnpairDevice(0, handle);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 13:
                {
                    BTRMGR_PairedDevicesList_t pairedDevices;

                    memset (&pairedDevices, 0, sizeof(pairedDevices));
                    rc = BTRMGR_GetPairedDevices(0, &pairedDevices);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                    {
                        int j = 0;
                        printf ("\nSuccess....   Paired Devices (%d) are, \n", pairedDevices.m_numOfDevices);
                        printf ("\n\tSN %-17s %-30s %-17s   %s\n\n", "Device Id", "Device Name", "Device Address", "Device Type");
                        for (; j< pairedDevices.m_numOfDevices; j++)
                        {
                            printf ("\t%02d %-17llu %-30s %17s   %s\n", j,
                                                              pairedDevices.m_deviceProperty[j].m_deviceHandle,
                                                              pairedDevices.m_deviceProperty[j].m_name,
                                                              pairedDevices.m_deviceProperty[j].m_deviceAddress,
                                                              BTRMGR_GetDeviceTypeAsString(pairedDevices.m_deviceProperty[j].m_deviceType));
                        }
                        printf ("\n\n");
                    }
                }
                break;
            case 14:
                {
                    handle = 0;
                    int ch = 0;
                    printf ("Please Enter the device Handle number of the device that you want to Connect \t: ");
                    handle = getDeviceSelection();
                    printf ("Enter Device ConnectAs  Type : [0 - AUDIO_OUTPUT | 1 - AUDIO_INPUT | 2 - LE | 3 - HID | 4 - UNKNOWN]\n");
                    ch = getDeviceSelection();

                    rc = BTRMGR_ConnectToDevice(0, handle, (1 << ch));
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 15:
                {
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to DisConnect \t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_DisconnectFromDevice(0, handle);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 16:
                {
                    BTRMGR_DevicesProperty_t deviceProperty;
                    int i = 0;

                    handle = 0;
                    memset (array, '\0', sizeof(array));
                    memset (&deviceProperty, 0, sizeof(deviceProperty));

                    printf ("Please Enter the device Handle number of the device that you want to query \t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_GetDeviceProperties(0, handle, &deviceProperty);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                    {
                        printf ("\nSuccess.... Properties are, \n");
                        printf ("Handle       : %llu\n", deviceProperty.m_deviceHandle);
                        printf ("Name         : %s\n", deviceProperty.m_name);
                        printf ("Address      : %s\n", deviceProperty.m_deviceAddress);
                        printf ("RSSI         : %d\n", deviceProperty.m_rssi);
                        printf ("Paired       : %d\n", deviceProperty.m_isPaired);
                        printf ("Connected    : %d\n", deviceProperty.m_isConnected);
                        printf ("Vendor ID    : %u\n", deviceProperty.m_vendorID);
                        for (i = 0; i < deviceProperty.m_serviceInfo.m_numOfService; i++)
                        {
                            printf ("Profile ID   : 0x%.4x\n", deviceProperty.m_serviceInfo.m_profileInfo[i].m_uuid);
                            printf ("Profile Name : %s\n", deviceProperty.m_serviceInfo.m_profileInfo[i].m_profile);
                        }
                        printf ("######################\n\n\n");
                    }
                }
                break;
            case 17:
                {
                    BTRMGR_DeviceOperationType_t stream_pref;

                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to start play\t: ");
                    handle = getDeviceSelection();

                    printf ("Please set the Streaming Pref\t[0 - AUDIO_OUTPUT | 1 - AUDIO_INPUT | 2 - LE ]\n");
                    stream_pref = (BTRMGR_DeviceOperationType_t) getUserSelection();


                    rc = BTRMGR_StartAudioStreamingOut(0, handle, (1 << stream_pref));
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 18:
                {
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to stop play\t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_StopAudioStreamingOut(0, handle);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 19:
                {
                    unsigned char index = 0;
                    rc = BTRMGR_IsAudioStreamingOut(0, &index);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....; Streaming status = %u\n", index);
                }
                break;
            case 20:
               {
                   uselection=20;
                   printf ("\nNow you can power Off and power On to check the auto connection..\n");
                   while(wait) { usleep(1000000); }
                   uselection=0; wait=1;
               }
               break;
            case 21:
               {
                   BTRMGR_EventResponse_t  lstBtrMgrEvtRsp;
                   memset(&lstBtrMgrEvtRsp, 0, sizeof(lstBtrMgrEvtRsp));

                   lstBtrMgrEvtRsp.m_deviceHandle = gDeviceHandle;
                   lstBtrMgrEvtRsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST;
                   lstBtrMgrEvtRsp.m_eventResp = 1;

                   if (BTRMGR_RESULT_SUCCESS != BTRMGR_SetEventResponse(0, &lstBtrMgrEvtRsp)) {
                       printf ("Failed to send event response");
                   }
                   gDeviceHandle = 0;
               }
               break;
            case 22:
               {
                   BTRMGR_EventResponse_t  lstBtrMgrEvtRsp;
                   memset(&lstBtrMgrEvtRsp, 0, sizeof(lstBtrMgrEvtRsp));

                   lstBtrMgrEvtRsp.m_deviceHandle = gDeviceHandle;
                   lstBtrMgrEvtRsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST;
                   lstBtrMgrEvtRsp.m_eventResp = 0;

                   if (BTRMGR_RESULT_SUCCESS != BTRMGR_SetEventResponse(0, &lstBtrMgrEvtRsp)) {
                       printf ("Failed to send event response");
                   }
                   gDeviceHandle = 0;
               }
               break;
            case 23:
               {
                   BTRMGR_EventResponse_t  lstBtrMgrEvtRsp;
                   memset(&lstBtrMgrEvtRsp, 0, sizeof(lstBtrMgrEvtRsp));

                   lstBtrMgrEvtRsp.m_deviceHandle = gDeviceHandle;
                   lstBtrMgrEvtRsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST;
                   lstBtrMgrEvtRsp.m_eventResp = 1;

                   if (BTRMGR_RESULT_SUCCESS != BTRMGR_SetEventResponse(0, &lstBtrMgrEvtRsp)) {
                       printf ("Failed to send event response");
                   }
                   gDeviceHandle = 0;
               }
               break;
            case 24:
               {
                   BTRMGR_EventResponse_t  lstBtrMgrEvtRsp;
                   memset(&lstBtrMgrEvtRsp, 0, sizeof(lstBtrMgrEvtRsp));

                   lstBtrMgrEvtRsp.m_deviceHandle = gDeviceHandle;
                   lstBtrMgrEvtRsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST;
                   lstBtrMgrEvtRsp.m_eventResp = 0;

                   if (BTRMGR_RESULT_SUCCESS != BTRMGR_SetEventResponse(0, &lstBtrMgrEvtRsp)) {
                       printf ("Failed to send event response");
                   }
                   gDeviceHandle = 0;
               }
               break;
            case 25:
               {
                   BTRMGR_EventResponse_t  lstBtrMgrEvtRsp;
                   memset(&lstBtrMgrEvtRsp, 0, sizeof(lstBtrMgrEvtRsp));

                   lstBtrMgrEvtRsp.m_deviceHandle = gDeviceHandle;
                   lstBtrMgrEvtRsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST;
                   lstBtrMgrEvtRsp.m_eventResp = 1;

                   if (BTRMGR_RESULT_SUCCESS != BTRMGR_SetEventResponse(0, &lstBtrMgrEvtRsp)) {
                       printf ("Failed to send event response");
                   }
                   gDeviceHandle = 0;
               }
               break;
            case 26:
               {
                   BTRMGR_EventResponse_t  lstBtrMgrEvtRsp;
                   memset(&lstBtrMgrEvtRsp, 0, sizeof(lstBtrMgrEvtRsp));

                   lstBtrMgrEvtRsp.m_deviceHandle = gDeviceHandle;
                   lstBtrMgrEvtRsp.m_eventType = BTRMGR_EVENT_RECEIVED_EXTERNAL_PLAYBACK_REQUEST;
                   lstBtrMgrEvtRsp.m_eventResp = 0;

                   if (BTRMGR_RESULT_SUCCESS != BTRMGR_SetEventResponse(0, &lstBtrMgrEvtRsp)) {
                       printf ("Failed to send event response");
                   }
                   gDeviceHandle = 0;
               }
               break;
            case 27: 
                {
                    BTRMGR_DeviceOperationType_t stream_pref;

                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to start play from\t: ");
                    handle = getDeviceSelection();

                    printf ("Please set the Streaming Pref \t");
                    stream_pref = (BTRMGR_DeviceOperationType_t) getUserSelection();


                    rc = BTRMGR_StartAudioStreamingIn(0, handle, stream_pref);
                    if (BTRMGR_RESULT_SUCCESS != rc) 
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 28: 
                {
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to stop play from\t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_StopAudioStreamingIn(0, handle);
                    if (BTRMGR_RESULT_SUCCESS != rc) 
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 29:
                {
                    int opt=0;  
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();
                    printf ("Please Enter the Media Control Options\n"
                            "[0 - Play | 1 - Pause | 2 - Stop | 3 - Next  | 4 - Previous | 5 - FF | 6 - Rewind]\n"
                            "[7 - VolUp | 8 - VolDown | 9 - EQZ Off | 10 - EQZ On | 11 - Shuffle Off | 12 - shuffle AllTracks]\n"
                            "[13 - shuffle Group | 14 - Repeat Off | 15 - Repeat SingleTrack | 16 - Repeat AllTracks | 17 - Repeat Group]\n");
                    opt = getDeviceSelection();

                    rc = BTRMGR_MediaControl(0, handle, opt);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 30:
            case 46:
                {
                    BTRMGR_MediaTrackInfo_t mediaTrackInfo;
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_GetMediaTrackInfo(0, handle, &mediaTrackInfo);

                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\n---Media Track Info--- \n"
                                "Album           : %s\n"
                                "Artist          : %s\n"
                                "Title           : %s\n"
                                "Genre           : %s\n"
                                "NumberOfTracks  : %d\n"
                                "TrackNumber     : %d\n"
                                "Duration        : %d\n\n"
                                , mediaTrackInfo.pcAlbum
                                , mediaTrackInfo.pcArtist
                                , mediaTrackInfo.pcTitle
                                , mediaTrackInfo.pcGenre
                                , mediaTrackInfo.ui32NumberOfTracks
                                , mediaTrackInfo.ui32TrackNumber
                                , mediaTrackInfo.ui32Duration);

                }
                break;
            case 31:
                {
                    BTRMGR_MediaPositionInfo_t  m_mediaPositionInfo;
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_GetMediaCurrentPosition (0, handle, &m_mediaPositionInfo);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nPosition : %d | Duration : %d\n", m_mediaPositionInfo.m_mediaPosition
                                                                   , m_mediaPositionInfo.m_mediaDuration);
                }
                break;
            case 32:
                {
                    unsigned char index = 0;
                    rc = BTRMGR_IsAudioStreamingIn(0, &index);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....; Streaming status = %u\n", index);
                }
                break;
            case 33:
                {
                    char luuid[BTRMGR_MAX_STR_LEN] = "\0";
                    unsigned char opt = 0;
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();
                    printf ("Enter the UUID : ");
                    getString (luuid);
                    printf ("Select the property to query...\n");
                    printf ("[0 - Uuid | 1 - Primary | 2 - Device | 3 - Service | 4 - Value |"
                            " 5 - Notifying | 6 - Flags | 7 -Character]\n");
                    opt = getDeviceSelection();

                    if (!opt) {
                        BTRMGR_DeviceServiceList_t deviceServiceList;
                        int i=0;
                        if ((rc = BTRMGR_GetLeProperty (0, handle, luuid, opt, (void*)&deviceServiceList)) == BTRMGR_RESULT_SUCCESS) {
                            if (deviceServiceList.m_numOfService) {
                                printf("\n\nObtained 'Flag' indices are based on the below mapping..\n"
                                       "0  - read\n"
                                       "1  - write\n"
                                       "2  - encrypt-read\n"
                                       "3  - encrypt-write\n"
                                       "4  - encrypt-authenticated-read\n"
                                       "5  - encrypt-authenticated-write\n"
                                       "6  - secure-read (Server only)\n"
                                       "7  - secure-write (Server only)\n"
                                       "8  - notify\n"
                                       "9  - indicate\n"
                                       "10 - broadcast\n"
                                       "11 - write-without-response\n"
                                       "12 - authenticated-signed-writes\n"
                                       "13 - reliable-write\n"
                                       "14 - writable-auxiliaries\n");

                                printf("\n\t%-40s Flag\n", "UUID");
                                for (; i<deviceServiceList.m_numOfService; i++) {
                                    printf("\n\t%-40s ", deviceServiceList.m_uuidInfo[i].m_uuid);
                                    getBitsToString(deviceServiceList.m_uuidInfo[i].flags);
                                }
                            } else {
                                printf("\n\n\tNo UUIDs found...\n\n");
                            }
                        }
                    } else
                    if (opt == 1 || opt == 5 ){
                       unsigned char val;
                       if ((rc = BTRMGR_GetLeProperty (0, handle, luuid, opt, (void*)&val)) == BTRMGR_RESULT_SUCCESS) {
                          printf("\nResult : \n\t%d", val);
                       }
                    } else
                    if (opt == 2 || opt == 3 || opt == 4 || opt == 7) {
                       char val[BTRMGR_MAX_STR_LEN] = "\0";
                       if ((rc = BTRMGR_GetLeProperty (0, handle, luuid, opt, (void*)&val)) == BTRMGR_RESULT_SUCCESS) {
                          printf("\nResult : \n\t%s", val);
                       }
                    } else
                    if (opt == 6) {
                       char val[5][BTRMGR_NAME_LEN_MAX]; int i=0;
                       memset (val, 0, sizeof(val));
                       if ((rc = BTRMGR_GetLeProperty (0, handle, luuid, opt, (void*)&val)) == BTRMGR_RESULT_SUCCESS) {
                          printf("\n Result :");
                          for(; i<5 && val[i][0]; i++) {
                             printf("\n\t- %s", val[i]);
                          }
                       }
                    }
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");               
                }
                break;
            case 34:
                {
                    char luuid[BTRMGR_MAX_STR_LEN] = "\0";
                    char res[BTRMGR_MAX_STR_LEN] = "\0";
                    char arg[BTRMGR_MAX_STR_LEN] = "\0";
                    unsigned char opt = 0;
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();
                    printf ("Enter the Char UUID : ");
                    getString (luuid);
                    printf ("Enter Option : [ReadValue - 1 | WriteValue - 2 | StartNotify - 3 | StopNotify - 4]\n");
                    opt = getDeviceSelection();

                    if (opt == 2) {
                        printf ("Enter the Value to be Written : ");
                        getString (arg);
                    }

                    rc = BTRMGR_PerformLeOp (0, handle, luuid, opt, arg, res);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else {
                        if (opt == 0) {
                           printf ("\n\tObtained Value : %s\n", res);
                        }
                        printf ("\nSuccess.... \n" );
                    }
                }
                break;
            case 35:
                {
                    unsigned char index = 0;
                    rc = BTRMGR_ResetAdapter(index);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....; Reset status = %u\n", index);
                }
                break;
            case 36:
                {
                    BTRMGR_DiscoveryStatus_t aeDiscoveryStatus;
                    BTRMGR_DeviceOperationType_t aenBTRMgrDevOpType;

                    rc = BTRMGR_GetDiscoveryStatus (0, &aeDiscoveryStatus, &aenBTRMgrDevOpType);

                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....; Device Discovery Status = %u and Device Type = %u \n", aeDiscoveryStatus, aenBTRMgrDevOpType);

                }
                break;
            case 37:
                {
                	BTRMGR_StreamOut_Type_t type = BTRMGR_STREAM_AUXILIARY;

                    int ch = 0;
                    printf ("Enter StreamOut Type : [0 - Primary | 1 - Auxiliary]\n");
                    ch = getDeviceSelection();
                    if (0 == ch)
                        type = BTRMGR_STREAM_PRIMARY;
                    else if (1 == ch)
                        type = BTRMGR_STREAM_AUXILIARY;
                    else
                        type = BTRMGR_STREAM_AUXILIARY;

                    rc = BTRMGR_SetAudioStreamingOutType(0, type);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....; Set StreamOut Type = %d \n",  type);
                }
                break;
            case 40:
                {
                  /*Connect to Device */
                  /*Notify to Tile Device in char: 9d410019-35d6-f4dd-ba60-e7bd8dc491c0 */
                  /*Wrire to Tile Device in char: 9d410018-35d6-f4dd-ba60-e7bd8dc491c0 */
                    handle = 0;
                    printf ("Please Enter the Tile device Handle number of the device that you want to Connect \t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_ConnectToDevice(0, handle, BTRMGR_DEVICE_OP_TYPE_LE);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                    {
                        sleep(3);
                        printf ("\nSuccessfully connected to Tile device.\n");
                        /*Notify to Tile Device in char: 9d410019-35d6-f4dd-ba60-e7bd8dc491c0 */
                        {
                            char res[BTRMGR_MAX_STR_LEN] = "\0";
                            char notifyUuid[] = "9d410019-35d6-f4dd-ba60-e7bd8dc491c0";
                            char arg[BTRMGR_MAX_STR_LEN] = "\0";
                            rc = BTRMGR_PerformLeOp (0, handle, notifyUuid, BTRMGR_LE_OP_START_NOTIFY, arg, res);
                            if (BTRMGR_RESULT_SUCCESS != rc)
                                printf ("[%d]Failed to set notify.\n", __LINE__);
                            else
                            {
                                printf ("\nSuccessfully set notification. \n" );
                                sleep(3);
                                /*Wrire to Tile Device in char: 9d410018-35d6-f4dd-ba60-e7bd8dc491c0 */
                                char writeCharUuid[] = "9d410018-35d6-f4dd-ba60-e7bd8dc491c0";
                                char wValue_TOA_CMD_OPEN_CHANNEL[] = "0000000000100000000000000000000000000000";
                                char wValue_TOA_CMD_READY[] = "0212000000000000000000000000000000000000";
                                char wValue_TOA_CMD_SONG[] = "020502010300000000";

                                rc = BTRMGR_PerformLeOp (0, handle, writeCharUuid, BTRMGR_LE_OP_WRITE_VALUE, wValue_TOA_CMD_OPEN_CHANNEL, res);
                                if (BTRMGR_RESULT_SUCCESS != rc) {
                                    printf ("[%d]Failed to set TOA_CMD_OPEN_CHANNEL.\n", __LINE__);
                                }
                                else {
                                    printf ("\nSuccessfully set TOA_CMD_OPEN_CHANNEL. \n" );
                                    sleep(3);
                                    /* @TODO: On write, need to listen notify response and parse the message to get CID */
                                    rc = BTRMGR_PerformLeOp (0, handle, writeCharUuid, BTRMGR_LE_OP_WRITE_VALUE, wValue_TOA_CMD_READY, res);
                                    if (BTRMGR_RESULT_SUCCESS != rc) {
                                        printf ("[%d]Failed to set TOA_CMD_READY.\n", __LINE__);
                                    }
                                    else
                                    {
                                        printf ("\nSuccessfully set TOA_CMD_OPEN_READY. \n" );
                                        sleep(3);
                                        /* @TODO: On write, need to listen notify response and parse the message to get CID */
                                        rc = BTRMGR_PerformLeOp (0, handle, writeCharUuid, BTRMGR_LE_OP_WRITE_VALUE, wValue_TOA_CMD_SONG, res);
                                        if (BTRMGR_RESULT_SUCCESS != rc) {
                                            printf ("[%d]Failed to set TOA_CMD_SONG.\n", __LINE__);
                                        }
                                        else
                                        {
                                            printf ("\nSuccessfully set TOA_CMD_SONG. \n" );
                                        }
                                    }
                                }
                            }
                        }
                    }
                    BTRMGR_DisconnectFromDevice(0, handle);
                }
                break;
            case 41:
                {
                    int choice = 0;
                    WDMP_STATUS status = WDMP_FAILURE;

                    printf ("\n\nATTENTION! Disconnect all existing AudioIn connection before flipping the AudioIn Service state.\n\n");
                    printf ("Press 1 to Enable and 0 to Disable AudioIn Service.\n");
                    scanf("%d", &choice);

                    status = setRFCParameter("btrMgrTest", "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.BTR.AudioIn.Enable",
                                                           (choice)? "true" : "false", WDMP_BOOLEAN);

                    if (status != WDMP_SUCCESS) {
                        printf("\nsetRFCParameter Failed : %s\n", getRFCErrorString(status));
                    }
                    else {
                        if (choice) {
                            printf("\nSuccessfully Enabled AudioIn.\n");
                        }
                        else {
                            printf("\nSuccessfully Disabled AudioIn.\n");
                        }
                    }
                }
                break;
            case 42:
                {
                    int choice = 0;

                    printf ("\n\nATTENTION! Disconnect all existing AudioIn connection before flipping the AudioIn Service state.\n\n");
                    printf ("Press 1 to Enable and 0 to Disable AudioIn Service.\n");
                    scanf("%d", &choice);

                    rc = BTRMGR_SetAudioInServiceState (0, choice);

                    if (BTRMGR_RESULT_SUCCESS == rc) {
                        if (choice) {
                            printf("\nSuccessfully Enabled AudioIn.\n");
                        }
                        else {
                            printf("\nSuccessfully Disabled AudioIn.\n");
                        }
                    }
                    else {
                        printf("\nCall Failed : %d\n", rc);
                    }
                }
                break;
            case 43:
                {
                    BTRMgrMediaElementHandle mediaElementHdl = 0;
                    int mediaElementType    = 0;
                    handle = 0;
                    printf ("\nPlease Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();
                    printf("\nEnter the Media Browser Element Handle\t: ");
                    scanf("%llu", &mediaElementHdl);
                    printf("\n0. Unknown | 1. Album | 2. Artist | 3. Genre | 4. Compilation | 5. PlayList | 6. TrackList"
                           "\nEnter the Media Element type\t: ");
                    scanf("%d", &mediaElementType);

                    rc = BTRMGR_SetMediaElementActive(0, handle, mediaElementHdl, mediaElementType);

                    if (BTRMGR_RESULT_SUCCESS == rc) {
                        printf ("\nSuccess...\n");
                    }
                    else {
                        printf("\nFailed...\n");
                    }
                }
                break;
            case 44:
                {
                    BTRMgrMediaElementHandle mediaElementHdl = 0;
                    unsigned short startIdx = 0;
                    unsigned short endIdx   = 0;
                    int mediaElementType    = 0;
                    BTRMGR_MediaElementListInfo_t   mediaEementListInfo;
                    handle = 0;
                    printf ("\nPlease Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();
                    printf("\nEnter the Media Browser Element Handle\t: ");
                    scanf("%llu", &mediaElementHdl);
                    printf("\nEnter the Start index of List\t: ");
                    scanf("%hu", &startIdx);
                    printf("\nEnter the End index of List\t: ");
                    scanf("%hu", &endIdx);
                    printf("\n0. Unknown | 1. Album | 2. Artist | 3. Genre | 4. Compilation | 5. PlayList | 6. TrackList"
                           "\nEnter the Media Element type\t: ");
                    scanf("%d", &mediaElementType);

                    memset (&mediaEementListInfo, 0, sizeof(BTRMGR_MediaElementListInfo_t));

                    rc = BTRMGR_GetMediaElementList(0, handle, mediaElementHdl, startIdx, endIdx, 0, mediaElementType, &mediaEementListInfo);

                    if (BTRMGR_RESULT_SUCCESS == rc) {
                        unsigned short ui16LoopIdx = 0;
                        printf ("\nSuccess...\n");

                        printf ("\n%4s\t%-20s\t\t%-60sPlayable\n", "SN", "MediaElementId", "MediaElementName");
                        while (ui16LoopIdx < mediaEementListInfo.m_numberOfElements) {
                            if (strlen(mediaEementListInfo.m_mediaElementInfo[ui16LoopIdx].m_mediaElementName) > 50) {
                                strcpy (&mediaEementListInfo.m_mediaElementInfo[ui16LoopIdx].m_mediaElementName[50],"...");
                            }
                            printf("\n%4u\t%020llu\t\t%-60s%d", ui16LoopIdx
                                                              , mediaEementListInfo.m_mediaElementInfo[ui16LoopIdx].m_mediaElementHdl
                                                              , mediaEementListInfo.m_mediaElementInfo[ui16LoopIdx].m_mediaElementName
                                                              , mediaEementListInfo.m_mediaElementInfo[ui16LoopIdx].m_IsPlayable);
                            ui16LoopIdx++;
                        }
                        printf ("\n\nRetrived %u Media Elements.\n", mediaEementListInfo.m_numberOfElements);
                    }
                    else {
                        printf("\nFailed...\n");
                    }
                }
                break;
            case 45:
                {
                    BTRMgrMediaElementHandle  mediaElementHdl = 0;
                    int mediaElementType    = 0;
                    handle = 0;
                    printf ("\nPlease Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();
                    printf("\nEnter the Media Element Handle to select\t: ");
                    scanf("%llu", &mediaElementHdl);
                    printf("\n0. Unknown | 1. Album | 2. Artist | 3. Genre | 4. Compilation | 5. PlayList | 6. TrackList"
                           "\nEnter the Media Element type\t: ");
                    scanf("%d", &mediaElementType);

                    rc = BTRMGR_SelectMediaElement (0, handle, mediaElementHdl, mediaElementType);

                    if (BTRMGR_RESULT_SUCCESS == rc) {
                        printf ("\nSuccess...\n");
                    }
                    else {
                        printf("\nFailed...\n");
                    }
                }
                break;
            case 50:
                {
                    char lPropertyValue[BTRMGR_MAX_STR_LEN] = "";

                    BTRMGR_SysDiagInfo(0, BTRMGR_DEVICE_MAC, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    strncpy((char*)stCustomAdv.device_mac, lPropertyValue, strlen(lPropertyValue));

                    printf("Adding default local services : DEVICE_INFORMATION_UUID - 0x180a, RDKDIAGNOSTICS_UUID - 0xFDB9\n");
                    BTRMGR_LE_SetServiceInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, 1);
                    BTRMGR_LE_SetServiceInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, 1);

                    /* Get model number */
                    BTRMGR_SysDiagInfo(0, BTRMGR_SYSTEM_ID_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    printf("Adding char for the default local services : 0x180a, 0xFDB9\n");
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SYSTEM_ID_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                                /* system ID            */
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_MODEL_NUMBER_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                             /* model number         */
                    /*Get HW revision*/
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_HARDWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Hardware revision    */
                    /* Get serial number */
                    BTRMGR_SysDiagInfo(0, BTRMGR_SERIAL_NUMBER_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SERIAL_NUMBER_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                            /* serial number        */
                    /* Get firmware revision */
                    BTRMGR_SysDiagInfo(0, BTRMGR_FIRMWARE_REVISION_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_FIRMWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Firmware revision    */
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SOFTWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Software revision    */
                    /* Get manufacturer name */
                    BTRMGR_SysDiagInfo(0, BTRMGR_MANUFACTURER_NAME_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_MANUFACTURER_NAME_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                        /* Manufacturer name    */

                    /* 0xFDB9 */
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_DEVICE_STATUS_UUID, 0x1, "READY", BTRMGR_LE_PROP_CHAR);                                       /* DeviceStatus         */
                    BTRMGR_SysDiagInfo(0, BTRMGR_FWDOWNLOAD_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_FWDOWNLOAD_STATUS_UUID, 0x103, lPropertyValue, BTRMGR_LE_PROP_CHAR);                          /* FWDownloadStatus     */
                    BTRMGR_SysDiagInfo(0, BTRMGR_WEBPA_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_WEBPA_STATUS_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                                 /* WebPAStatus          */
                    BTRMGR_SysDiagInfo(0, BTRMGR_WIFIRADIO1_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_WIFIRADIO1_STATUS_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                            /* WiFiRadio1Status     */
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_WIFIRADIO2_STATUS_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);                            /* WiFiRadio2Status     */
                    BTRMGR_SysDiagInfo(0, BTRMGR_RF_STATUS_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_RDKDIAGNOSTICS_UUID, BTRMGR_RF_STATUS_UUID, 0x1, "NOT CONNECTED", BTRMGR_LE_PROP_CHAR);                                   /* RFStatus             */

                    /* Begin advertisement */
                    BTRMGR_LE_StartAdvertisement(0, &stCustomAdv);
                }
                break;
            case 51:
                {
                    int lChoice = 1;
                    char lUUID[BTRMGR_MAX_STR_LEN] = "";
                    unsigned char lServiceType = 1;
                    char lParentUUID[BTRMGR_MAX_STR_LEN] = "";
                    int lFlag = 0;
                    unsigned short lFlagMask = 0x0;
                    char lValue[BTRMGR_MAX_STR_LEN] = "";

                    do
                    {
                        printf("\nAdd test Gatt info\n");
                        printf("0 - No | 1 - Service | 2 - Characteristic | 3 - Descriptor\n");
                        scanf("%d", &lChoice);
                        switch (lChoice)
                        {
                            case 1:
                            {
                                printf("Enter UUID of Service\n");
                                scanf("%s", lUUID);
                                printf("Enter Service Type\n");
                                printf("1 - Primary | 0 - Secondary\n");
                                scanf("%d", (int*)&lServiceType);
                                BTRMGR_LE_SetServiceInfo(0, lUUID, (lServiceType != 0 ? 1 : 0));
                            }
                            break;
                            case 2:
                            case 3:
                            {
                                printf("Enter UUID\n");
                                scanf("%s", lUUID);
                                printf("Enter UUID of parent\n");
                                scanf("%s", lParentUUID);
                                do {
                                    printf("Enter the characteristic flags \n");
                                    printf("\n"
                                        "0  - read\n"
                                        "1  - write\n"
                                        "2  - encrypt-read\n"
                                        "3  - encrypt-write\n"
                                        "4  - encrypt-authenticated-read\n"
                                        "5  - encrypt-authenticated-write\n"
                                        "6  - secure-read (Server only)\n"
                                        "7  - secure-write (Server only)\n"
                                        "8  - notify\n"
                                        "9  - indicate\n"
                                        "10 - broadcast\n"
                                        "11 - write-without-response\n"
                                        "12 - authenticated-signed-writes\n"
                                        "13 - reliable-write\n"
                                        "14 - writable-auxiliaries\n"
                                        "15 - Done");
                                    printf("\nPress enter when done");
                                    scanf("%d", &lFlag);
                                    if ((lFlag >= 0) || (lFlag <= 14))
                                    {
                                        lFlagMask |= (1 << lFlag);
                                    }
                                } while (lFlag != 15);
                                if (0x1 == (lFlagMask & 0x1))
                                {
                                    printf("Enter value of the characteristic\n");
                                    scanf("%s", lValue);
                                }
                                BTRMGR_LE_SetGattInfo(0, lParentUUID, lUUID, lFlagMask, lValue, (lChoice == 2?BTRMGR_LE_PROP_CHAR: BTRMGR_LE_PROP_DESC));
                            }
                            break;
                            default:
                            break;
                        }
                    } while (lChoice);
                    BTRMGR_LE_StopAdvertisement(0);
                    BTRMGR_LE_StartAdvertisement(0, &stCustomAdv);
                }
                break;
            case 52:
                {
                    BTRMGR_LE_StopAdvertisement(0);
                }
                break;
            case 53:
                {
                    char lPropertyValue[BTRMGR_MAX_STR_LEN] = "";

                    printf("Adding char for the default local services \n");
                    BTRMGR_LE_SetServiceInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, 1);
                    BTRMGR_LE_SetServiceInfo(0, BTRMGR_LEONBRDG_SERVICE_UUID_SETUP, 1);

                    /* Get system ID - device MAC */
                    BTRMGR_SysDiagInfo(0, BTRMGR_SYSTEM_ID_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SYSTEM_ID_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);          /* system ID            */
                    /* Get model number */
                    BTRMGR_SysDiagInfo(0, BTRMGR_MODEL_NUMBER_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);                                           
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_MODEL_NUMBER_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);       /* model number         */
                    /*Get HW revision*/
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_HARDWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);  /* Hardware revision    */
                    /* Get serial number */
                    BTRMGR_SysDiagInfo(0, BTRMGR_SERIAL_NUMBER_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);                                          
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SERIAL_NUMBER_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);      /* serial number        */
                    /* Get firmware revision */
                    BTRMGR_SysDiagInfo(0, BTRMGR_FIRMWARE_REVISION_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_FIRMWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);  /* Firmware revision    */
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_SOFTWARE_REVISION_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);  /* Software revision    */
                    /* Get manufacturer name */
                    BTRMGR_SysDiagInfo(0, BTRMGR_MANUFACTURER_NAME_UUID, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_DEVICE_INFORMATION_UUID, BTRMGR_MANUFACTURER_NAME_UUID, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);  /* Manufacturer name    */

                    BTRMGR_SysDiagInfo(0, BTRMGR_LEONBRDG_UUID_QR_CODE, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_LEONBRDG_SERVICE_UUID_SETUP, BTRMGR_LEONBRDG_UUID_QR_CODE, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);
                    BTRMGR_SysDiagInfo(0, BTRMGR_LEONBRDG_UUID_PROVISION_STATUS, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_LEONBRDG_SERVICE_UUID_SETUP, BTRMGR_LEONBRDG_UUID_PROVISION_STATUS, 0x101, lPropertyValue, BTRMGR_LE_PROP_CHAR);
                    BTRMGR_SysDiagInfo(0, BTRMGR_LEONBRDG_UUID_PUBLIC_KEY, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_LEONBRDG_SERVICE_UUID_SETUP, BTRMGR_LEONBRDG_UUID_PUBLIC_KEY, 0x1, lPropertyValue, BTRMGR_LE_PROP_CHAR);
                    //BTRMGR_SysDiagInfo(0, BTRMGR_LEONBRDG_UUID_WIFI_CONFIG, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_LEONBRDG_SERVICE_UUID_SETUP, BTRMGR_LEONBRDG_UUID_WIFI_CONFIG, 0x2, lPropertyValue, BTRMGR_LE_PROP_CHAR);
                    BTRMGR_LE_SetGattInfo(0, BTRMGR_LEONBRDG_SERVICE_UUID_SETUP, BTRMGR_LEONBRDG_UUID_SSID_LIST, 0x1, " ", BTRMGR_LE_PROP_CHAR);
                    //BTRMGR_LE_SetGattInfo(0, BTRMGR_LEONBRDG_UUID_PROVISION_STATUS, "0x2902", 0x1, "0x0100", BTRMGR_LE_PROP_DESC);
                    printf("Starting the ad\n");
                    BTRMGR_LE_StartAdvertisement(0, &stCustomAdv);
                    
                    usleep(40000000);
                    //BTRMGR_LE_SetGattPropertyValue(0, "0x2902", "0x01", BTRMGR_LE_PROP_DESC);
#if 1
 
                    //char lPropertyValue[BTRMGR_MAX_STR_LEN] = "";
                    //usleep(10000000);

                    do {
                        printf("Getting the provision status\n");
                        BTRMGR_SysDiagInfo(0, BTRMGR_LEONBRDG_UUID_PROVISION_STATUS, lPropertyValue, BTRMGR_LE_OP_READ_VALUE);

                        printf("BTRMGR_LEONBRDG_UUID_PROVISION_STATUS is %s\n", lPropertyValue);
                        /* Add condition to notify only if different from the last */
                        {
                            BTRMGR_LE_SetGattPropertyValue(0, BTRMGR_LEONBRDG_UUID_PROVISION_STATUS, lPropertyValue, BTRMGR_LE_PROP_CHAR);
                        }
                        if (!strncmp("0x102", lPropertyValue, 4))
                        {
                            printf("Waiting for wifi config\n");
                            break;
                        }

                        usleep(1000000);

                    } while (1);
                    
#endif              
                }
                break;
            case 55:
                loop = 0;
                break;
            default:
                printf ("Invalid Selection.....\n");
                break;
        }
    }while(loop && cliDisabled);

    if (cliDisabled ==0)
    {
        for(counter = 0; counter < argc; ++counter) free(gArgv[counter]);
        free(gArgv);
    }
    BTRMGR_DeInit();
    return 0;
}
