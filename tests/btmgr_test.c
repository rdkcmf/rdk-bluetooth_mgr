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
    printf ("19. Get Streaming Status\n");
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

static BTMgrDeviceHandle getDeviceSelection(void)
{
    BTMgrDeviceHandle mychoice;
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

void eventCallback (BTMGR_EventMessage_t event)
{
    printf ("@@@@@@@@ %s ::::  Event ID %d @@@@@@@@\n", __FUNCTION__, event.m_eventType);
    return;
}


int main()
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    int loop = 1, i = 0;
    char array[32] = "";
    BTMgrDeviceHandle handle = 0;

    rc = BTMGR_Init();

    if (BTMGR_RESULT_SUCCESS != rc)
    {
        printf ("Failed to init BTMgr.. Quiting.. \n");
        loop = 0;
    }

    BTMGR_RegisterEventCallback (eventCallback);

    while(loop)
    {
        printOptions();
        i = getUserSelection();
        switch (i)
        {
            case 1:
                {
                    unsigned char numOfAdapters = 0;
                    rc = BTMGR_GetNumberOfAdapters(&numOfAdapters);
                    if (BTMGR_RESULT_SUCCESS != rc)
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
                    rc = BTMGR_SetAdapterName(0, array);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 3:
                {
                    memset (array, '\0', sizeof(array));
                    rc = BTMGR_GetAdapterName(0, array);
                    if (BTMGR_RESULT_SUCCESS != rc)
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

                    rc = BTMGR_SetAdapterPowerStatus(0, power_status);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success\n");
                }
                break;
            case 5:
                {
                    unsigned char power_status = 0;

                    rc = BTMGR_GetAdapterPowerStatus (0, &power_status);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success; Status = %u\n", power_status);
                }
                break;
            case 6:
                {
                    unsigned char power_status = 0;
                    unsigned char timeout = 0;

                    printf ("Please enter 1 or 0 to Make it Discoverable ON or OFF \t");
                    power_status = (unsigned char) getUserSelection();

                    printf ("Please set the timeout for the discoverable \t");
                    timeout = (unsigned char) getUserSelection();

                    rc = BTMGR_SetAdapterDiscoverable(0, power_status, timeout);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 7:
                {
                    unsigned char power_status = 0;

                    rc = BTMGR_IsAdapterDiscoverable(0, &power_status);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success; Status = %u\n", power_status);
                }
                break;
            case 8:
                {
                    rc = BTMGR_StartDeviceDiscovery(0);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 9:
                {
                    rc = BTMGR_StopDeviceDiscovery(0);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 10:
                {
                    BTMGR_DiscoveredDevicesList_t discoveredDevices;

                    memset (&discoveredDevices, 0, sizeof(discoveredDevices));
                    rc = BTMGR_GetDiscoveredDevices(0, &discoveredDevices);
                    if (BTMGR_RESULT_SUCCESS != rc)
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

                    rc = BTMGR_PairDevice(0, handle);
                    if (BTMGR_RESULT_SUCCESS != rc)
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

                    rc = BTMGR_UnpairDevice(0, handle);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 13:
                {
                    BTMGR_PairedDevicesList_t pairedDevices;

                    memset (&pairedDevices, 0, sizeof(pairedDevices));
                    rc = BTMGR_GetPairedDevices(0, &pairedDevices);
                    if (BTMGR_RESULT_SUCCESS != rc)
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

                    rc = BTMGR_ConnectToDevice(0, handle, BTMGR_DEVICE_TYPE_AUDIOSINK);
                    if (BTMGR_RESULT_SUCCESS != rc)
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

                    rc = BTMGR_DisconnectFromDevice(0, handle);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 16:
                {
                    BTMGR_DevicesProperty_t deviceProperty;
                    int i = 0;

                    handle = 0;
                    memset (array, '\0', sizeof(array));
                    memset (&deviceProperty, 0, sizeof(deviceProperty));

                    printf ("Please Enter the device Handle number of the device that you want to query \t: ");
                    handle = getDeviceSelection();

                    rc = BTMGR_GetDeviceProperties(0, handle, &deviceProperty);
                    if (BTMGR_RESULT_SUCCESS != rc)
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
                    BTMGR_DeviceConnect_Type_t stream_pref;

                    handle = 0;
                    printf ("Please Enter the device Handle number of the device that you want to start play\t: ");
                    handle = getDeviceSelection();

                    printf ("Please set the Streaming Pref \t");
                    stream_pref = (BTMGR_DeviceConnect_Type_t) getUserSelection();


                    rc = BTMGR_StartAudioStreamingOut(0, handle, stream_pref);
                    if (BTMGR_RESULT_SUCCESS != rc)
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

                    rc = BTMGR_StopAudioStreamingOut(0, handle);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 19:
                {
                    unsigned char index = 0;
                    rc = BTMGR_IsAudioStreamingOut(0, &index);
                    if (BTMGR_RESULT_SUCCESS != rc)
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

    BTMGR_DeInit();
    return 0;
}
