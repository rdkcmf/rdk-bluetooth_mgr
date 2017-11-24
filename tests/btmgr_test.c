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

volatile int wait = 1;
int uselection = 0;
BTRMgrDeviceHandle gDeviceHandle = 0;

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
    printf ("55. Quit\n");
    printf ("\n\n");
    printf ("Please enter the option that you want to test\t");

    return;
}

static int getUserSelection (void)
{
    int mychoice;
    printf("Enter a choice...\n");
    scanf("%d", &mychoice);
    getchar();//to catch newline
    return mychoice;
}

static BTRMgrDeviceHandle getDeviceSelection(void)
{
    BTRMgrDeviceHandle mychoice;
    printf("Enter a choice...\n");
    scanf("%llu", &mychoice);
    getchar();//to catch newline
    return mychoice;
}


void getName (char* mychoice)
{
    char *tmp = NULL;
    fgets (mychoice, 30, stdin);
    tmp = strchr(mychoice, '\n');
    if (tmp)
        *tmp = '\0';
}

const char* getEventAsString (BTRMGR_Events_t etype)
{
  char *event = "\0";
  switch(etype)
  {
    case BTRMGR_EVENT_DEVICE_OUT_OF_RANGE               : event = "DEVICE_OUT_OF_RANGE_OR_LOST";       break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_UPDATE           : event = "DEVICE_DISCOVERY_UPDATE";           break;
    case BTRMGR_EVENT_DEVICE_DISCOVERY_COMPLETE         : event = "DEVICE_DISCOVERY_COMPLETE";         break;
    case BTRMGR_EVENT_DEVICE_PAIRING_COMPLETE           : event = "DEVICE_PAIRING_COMPLETE";           break; 
    case BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE         : event = "DEVICE_UNPAIRING_COMPLETE";         break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE        : event = "DEVICE_CONNECTION_COMPLETE";        break;
    case BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE        : event = "DEVICE_DISCONNECT_COMPLETE";        break;
    case BTRMGR_EVENT_DEVICE_PAIRING_FAILED             : event = "DEVICE_PAIRING_FAILED";             break;
    case BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED           : event = "DEVICE_UNPAIRING_FAILED";           break;
    case BTRMGR_EVENT_DEVICE_CONNECTION_FAILED          : event = "DEVICE_CONNECTION_FAILED";          break;     
    case BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED          : event = "DEVICE_DISCONNECT_FAILED";          break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_PAIR_REQUEST    : event = "RECEIVED_EXTERNAL_PAIR_REQUEST";    break;
    case BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST : event = "RECEIVED_EXTERNAL_CONNECT_REQUEST"; break;
    case BTRMGR_EVENT_DEVICE_FOUND                      : event = "DEVICE_FOUND";                      break;
    case BTRMGR_EVENT_MEDIA_TRACK_STARTED               : event = "MEDIA_TRACK_STARTED";               break;
    case BTRMGR_EVENT_MEDIA_TRACK_PLAYING               : event = "MEDIA_TRACK_PLAYING";               break;
    case BTRMGR_EVENT_MEDIA_TRACK_PAUSED                : event = "MEDIA_TRACK_PAUSED";                break;
    case BTRMGR_EVENT_MEDIA_TRACK_STOPPED               : event = "MEDIA_TRACK_STOPPED";               break;
    case BTRMGR_EVENT_MEDIA_TRACK_POSITION              : event = "MEDIA_TRACK_POSITION";              break;
    case BTRMGR_EVENT_MEDIA_TRACK_CHANGED               : event = "MEDIA_TRACK_CHANGED";               break;
    case BTRMGR_EVENT_MEDIA_PLAYBACK_ENDED              : event = "MEDIA_TRACK_ENDED";                 break;
    default                                            : event = "##INVALID##";
  }
  return event;
}


void eventCallback (BTRMGR_EventMessage_t event)
{
    printf ("\n\t@@@@@@@@ %s eventCallback ::::  Event ID %d @@@@@@@@\n", event.m_pairedDevice.m_name, event.m_eventType);

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
        printf ("\t PassCode  = %d\n", event.m_externalDevice.m_externalDevicePIN);
        printf ("\t Enter Option 21 to Accept Pairing Request\n");
        printf ("\t Enter Option 22 to Deny Pairing Request\n");
        gDeviceHandle = event.m_externalDevice.m_deviceHandle;
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
    case BTRMGR_EVENT_DEVICE_UNPAIRING_COMPLETE:
    case BTRMGR_EVENT_DEVICE_CONNECTION_COMPLETE:
    case BTRMGR_EVENT_DEVICE_DISCONNECT_COMPLETE:
    case BTRMGR_EVENT_DEVICE_PAIRING_FAILED:
    case BTRMGR_EVENT_DEVICE_UNPAIRING_FAILED:
    case BTRMGR_EVENT_DEVICE_CONNECTION_FAILED:
    case BTRMGR_EVENT_DEVICE_DISCONNECT_FAILED:
        printf("\tReceived %s %s Event from BTRMgr\n", event.m_pairedDevice.m_name, getEventAsString(event.m_eventType));
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
     default:
        printf("\tReceived %s Event from BTRMgr\n", getEventAsString(event.m_eventType));  
        break;
    }
                                      
    return;
}


int main()
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;
    int loop = 1, i = 0;
    char array[32] = "";
    BTRMgrDeviceHandle handle = 0;

    rc = BTRMGR_Init();

    if (BTRMGR_RESULT_SUCCESS != rc)
    {
        printf ("Failed to init BTRMgr.. Quiting.. \n");
        loop = 0;
    }

    BTRMGR_RegisterEventCallback (eventCallback);

    while(loop)
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
                    getName(array);

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
                    rc = BTRMGR_StartDeviceDiscovery(0);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 9:
                {
                    rc = BTRMGR_StopDeviceDiscovery(0);
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
                        for (; j< discoveredDevices.m_numOfDevices; j++)
                        {
                            printf ("%d \t %llu \t %s \t %s\n", j,
                                                              discoveredDevices.m_deviceProperty[j].m_deviceHandle,
                                                              discoveredDevices.m_deviceProperty[j].m_name,
                                                              discoveredDevices.m_deviceProperty[j].m_deviceAddress);
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
                        for (; j< pairedDevices.m_numOfDevices; j++)
                        {
                            printf ("%d \t %llu \t %s \t %s\n", j,
                                                              pairedDevices.m_deviceProperty[j].m_deviceHandle,
                                                              pairedDevices.m_deviceProperty[j].m_name,
                                                              pairedDevices.m_deviceProperty[j].m_deviceAddress);
                        }
                        printf ("\n\n");
                    }
                }
                break;
            case 14:
                {
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to Connect \t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_ConnectToDevice(0, handle, BTRMGR_DEVICE_TYPE_AUDIOSINK);
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
                    BTRMGR_DeviceConnect_Type_t stream_pref;

                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to start play\t: ");
                    handle = getDeviceSelection();

                    printf ("Please set the Streaming Pref \t");
                    stream_pref = (BTRMGR_DeviceConnect_Type_t) getUserSelection();


                    rc = BTRMGR_StartAudioStreamingOut(0, handle, stream_pref);
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
                    BTRMGR_DeviceConnect_Type_t stream_pref;

                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to start play from\t: ");
                    handle = getDeviceSelection();

                    printf ("Please set the Streaming Pref \t");
                    stream_pref = (BTRMGR_DeviceConnect_Type_t) getUserSelection();


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
                    printf ("Please Enter the Medio Control Options(0-Play/1-Pause/2-Stop/3-Next/4-Previous)\n");
                    opt = getDeviceSelection();

                    rc = BTRMGR_MediaControl(0, handle, opt);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 30:
                {
                    BTRMGR_MediaTrackInfo_t mediaTrackInfo;
                    handle = 0;
                    printf ("Please Enter the device Handle number of the device\t: ");
                    handle = getDeviceSelection();

                    rc = BTRMGR_GetMediaTrackInfo(0, handle, &mediaTrackInfo);
                    if (BTRMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\n---Current Media Track Info--- \n"
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
            case 55:
                loop = 0;
                break;
            default:
                printf ("Invalid Selection.....\n");
                break;
        }
    }

    BTRMGR_DeInit();
    return 0;
}
