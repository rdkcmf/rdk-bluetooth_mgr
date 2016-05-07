#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "btmgr.h"

static void printOptions (void)
{
    printf ("\n\n");
    printf (" 0. Get Number of Adapter\n");
    printf (" 1. Set Name to your Adapter\n");
    printf (" 2. Get Name of your Adapter\n");
    printf (" 3. Set Adapter Power; 0 to OFF & 1 to ON\n");
    printf (" 4. Get Adapter Power\n");
    printf (" 5. Set Discoverable\n");
    printf (" 6. Is it Discoverable\n");
    printf (" 7. Start Discovering\n");
    printf (" 8. Stop Discovering\n");
    printf (" 9. Get List of Discovered Devices\n");
    printf ("10. Pair a Device\n");
    printf ("11. UnPair a Device\n");
    printf ("12. Get List of Paired Devices\n");
    printf ("13. Connect to Device\n");
    printf ("14. DisConnect from Device\n");
    printf ("15. Get Device Properties\n");
    printf ("16. Set Preferred Device to Streamout Audio\n");
    printf ("17. Set Preferred Audio to Streamout; 0 to Primary & 1 to Secondary\n");
    printf ("18. Start Streaming\n");
    printf ("19. Stop Streaming\n");
    printf ("20. Get Streaming Status\n");
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

void getName (char* mychoice)
{
    scanf("%s", mychoice);
    getchar();//to catch newline
}

int main()
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;
    int loop = 1, i = 0;
    char array[32] = "";

    rc = BTMGR_Init();

    if (BTMGR_RESULT_SUCCESS != rc)
    {
        printf ("Failed to init BTMgr.. Quiting.. \n");
        loop = 0;
    }

    while(loop)
    {
        printOptions();
        i = getUserSelection();
        switch (i)
        {
            case 0:
                {
                    unsigned char numOfAdapters = 0;
                    rc = BTMGR_GetNumberOfAdapters(&numOfAdapters);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... Count = %d\n", numOfAdapters);
                }
                break;
            case 1:
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
            case 2:
                {
                    memset (array, '\0', sizeof(array));
                    rc = BTMGR_GetAdapterName(0, array);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("We received @@%s@@\n", array);
                }
                break;
            case 3:
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
            case 4:
                {
                    unsigned char power_status = 0;

                    rc = BTMGR_GetAdapterPowerStatus (0, &power_status);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success; Status = %u\n", power_status);
                }
                break;
            case 5:
                {
                    unsigned char power_status = 0;

                    printf ("Please set the timeout for the discoverable");
                    power_status = (unsigned char) getUserSelection();

                    rc = BTMGR_SetAdapterDiscoverable(0, 1, power_status);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 6:
                {
                    unsigned char power_status = 0;

                    rc = BTMGR_IsAdapterDiscoverable(0, &power_status);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success; Status = %u\n", power_status);
                }
                break;
            case 7:
                {
                    rc = BTMGR_StartDeviceDiscovery(0);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 8:
                {
                    rc = BTMGR_StopDeviceDiscovery(0);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("Success;\n");
                }
                break;
            case 9:
                {
                    BTMGR_Devices_t discoveredDevices;

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
                            printf ("%s \t\t %s", discoveredDevices.m_deviceProperty[j].m_name, discoveredDevices.m_deviceProperty[j].m_deviceAddress);
                        }
                        printf ("\n\n");
                    }
                }
                break;
            case 10:
                {
                    memset (array, '\0', sizeof(array));
                    printf ("Please Enter the name of the device that you want to pair \t: ");
                    getName(array);

                    printf ("We received @@%s@@ from you..  Hope this is correct. Let me try to set it..\n", array);
                    rc = BTMGR_PairDevice(0, array);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 11:
                {
                    memset (array, '\0', sizeof(array));
                    printf ("Please Enter the name of the device that you want to Unpair \t: ");
                    getName(array);

                    printf ("We received @@%s@@ from you..  Hope this is correct. Let me try to set it..\n", array);
                    rc = BTMGR_UnpairDevice(0, array);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 12:
                {
                    BTMGR_Devices_t pairedDevices;

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
                            printf ("%s \t\t %s", pairedDevices.m_deviceProperty[j].m_name, pairedDevices.m_deviceProperty[j].m_deviceAddress);
                        }
                        printf ("\n\n");
                    }
                }
                break;
            case 13:
                {
                    memset (array, '\0', sizeof(array));
                    printf ("Please Enter the name of the device that you want to Connect \t: ");
                    getName(array);

                    printf ("We received @@%s@@ from you..  Hope this is correct. Let me try to connect to it..\n", array);
                    rc = BTMGR_ConnectToDevice(0, array, BTMGR_DEVICE_TYPE_AUDIOSINK);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 14:
                {
                    memset (array, '\0', sizeof(array));
                    printf ("Please Enter the name of the device that you want to DisConnect \t: ");
                    getName(array);

                    printf ("We received @@%s@@ from you..  Hope this is correct. Let me try to connect to it..\n", array);
                    rc = BTMGR_DisconnectFromDevice(0, array);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess....\n");
                }
                break;
            case 15:
                {
                    BTMGR_DevicesProperty_t deviceProperty;

                    memset (array, '\0', sizeof(array));
                    memset (&deviceProperty, 0, sizeof(deviceProperty));

                    printf ("Please Enter the name of the device that you want to query \t: ");
                    getName(array);

                    printf ("We received @@%s@@ from you..  Hope this is correct. Let me try to connect to it..\n", array);
                    rc = BTMGR_GetDeviceProperties(0, array, &deviceProperty);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                    {
                        printf ("\nSuccess.... Properties are, \n");
                        printf ("Name: %s \nAddress : %s \nType : %d \nRSSI : %d \n\n", deviceProperty.m_name, deviceProperty.m_deviceAddress, deviceProperty.m_deviceType, deviceProperty.m_rssi);
                    }
                }
                break;
            case 16:
                {
                    memset (array, '\0', sizeof(array));
                    printf ("Please Enter the name of the device that you want to Set Default Device to Connect \t: ");
                    getName(array);

                    printf ("We received @@%s@@ from you..  Hope this is correct. Let me try to connect to it..\n", array);
                    rc = BTMGR_SetDefaultDeviceToStreamOut(array);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 17:
                {
                    BTMGR_StreamOut_Type_t stream_pref;

                    printf ("Please set the Streaming Pref \t");
                    stream_pref = (BTMGR_StreamOut_Type_t) getUserSelection();

                    rc = BTMGR_SetPreferredAudioStreamOutType(stream_pref);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 18:
                {
                    //unsigned char index = 0;
                    //printf ("Please set the Adapter Index\t");
                    //index = (unsigned char) getUserSelection();

                    rc = BTMGR_StartAudioStreamingOut(0);
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 19:
                {
                    rc = BTMGR_StopAudioStreamingOut();
                    if (BTMGR_RESULT_SUCCESS != rc)
                        printf ("failed\n");
                    else
                        printf ("\nSuccess.... \n");
                }
                break;
            case 20:
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
