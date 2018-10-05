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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "btmgr.h"
#include <fcgi_stdio.h>

const char* pSplChar        = "%20";
const char *pChangePower    = "ChangePower";
const char *pSetPower       = "SetPower=";
const char *pSaveName       = "SaveName=";
const char *pPairTo         = "PairTo=";
const char *pUnPairTo       = "UnpairTo=";
const char *pGetName        = "GetName";
const char *pStartDiscovery = "StartDiscovery";
const char *pShowDiscovered = "ShowDiscovered";
const char *pShowAllPaired  = "ShowAllPaired";
const char *pStartPlaying   = "StartPlaying=";
const char *pStopPlaying    = "StopPlaying";

const char *pMainPage = ""
            "<tr> "
            "<td> Power On/Off Adapter </td> "
            "<td> <button id=\"power\" class=\"button\" onclick=\"ChangePower()\">....Go....</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Get Name Of the Adapter </td> "
            "<td> <button class=\"button\" onclick=\"GetName()\">....Go....</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Show Paired Devices </td> "
            "<td> <button class=\"button\" onclick=\"ShowAllPaired()\">....Go....</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Start Discovery </td> "
            "<td> <button class=\"button\" onclick=\"StartDiscovery()\">....Go....</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Show Discovered Devices </td> "
            "<td> <button class=\"button\" onclick=\"ShowDiscovered()\">....Go....</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Stop Playing </td> "
            "<td> <button class=\"button\" onclick=\"StopPlaying()\">....Go....</button> </td> "
            "</tr> "
            "</table> "
            " "
            "<script type=\"text/javascript\"> "
            " "
            "function ChangePower() { "
            "    location.href = \"ChangePower\" "
            "} "
            " "
            "function GetName() { "
            "    location.href = \"GetName\" "
            "} "
            " "
            "function StartDiscovery() { "
            "    location.href = \"StartDiscovery\" "
            "} "
            " "
            "function ShowDiscovered() { "
            "    location.href = \"ShowDiscovered\" "
            "} "
            " "
            "function ShowAllPaired() { "
            "    location.href = \"ShowAllPaired\" "
            "} "
            " "
            "function StopPlaying() { "
            "    location.href = \"StopPlaying\" "
            "} "
            " "
            "</script> "
            " "
            "</body> "
            "</html> "
            " ";

//            "font-family: helvetica; "
//            "    background-color: black; "
//            "    color: white; "
//            "    border-radius: 4px 4px 0 0; "

const char* pStylePage = ""
            "<html> "
            "<head> "
            " "
            "<meta http-equiv=\"pragma\" content=\"no-cache\" />"
            "<meta http-equiv=\"expires\" content=\"0\" />"
            " "
            "<style> "
            ".button { "
            "    padding: 5px 15px; "
            "    text-decoration: none; "
            "    text-align: center; "
            "    display: inline-block; "
            "    font-size: 14px; "
            "    float: right "
            "} "
            " "
            ".button:hover "
            "{ "
            "    background-color: orange; "
            "} "
            " "
            ".button:active {"
            "      background-color: #A0B0C0; "
            "      transform: translateY(4px); "
            "}"
            " "
            "table, th, td { "
            "color: black;"
            "} "
            " "
            "</style> "
            "</head> "
            " "
            "<body bgcolor=\"#E0F48E\"> "
            " "
            "<table align=\"center\"> "
            "";

const char* pGoHome = ""
            "<hr> "
            "<p> </p> "
            "<button onclick=\"location.href='home';\" class=\"button\">Back</button> "
            " ";

const char* pGetNamePage = ""
            "<script type=\"text/javascript\"> "
            " "
            "function ChangeName() { "
            "    location.href = \"SaveName=\" + document.getElementById(\"name1\").value; "
            "} "
            " "
            "</script> "
            " "
            "</body> "
            "</html> "
            " ";

void findAndReplaceSplCharWithSpace (char* ptr)
{
    if (ptr)
    {
        int l1 = 0;
        int l2 = 0;
        int l3 = 0;
        char* p = NULL;
        char* t = ptr;
        char temp[1024];

        memset(temp, '\0', sizeof(temp));

        l1 = strlen(pSplChar);
        while (1)
        {
            p = strstr(ptr, pSplChar);
            if (p)
            {
                l2 = strlen(ptr);
                l3 = strlen(p);

                strncat (temp, ptr, (l2 - l3));
                strcat (temp, " ");
                ptr = p+l1;
            }
            else
            {
                strcat(temp, ptr);
                break;
            }
        }
        strcpy (t, temp);
    }
}

unsigned long long int gLastPlaying = 0;

void startStreaming (unsigned long long int handle )
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    /* Connect to a device first */
    rc = BTRMGR_ConnectToDevice(0, handle, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
    if (BTRMGR_RESULT_SUCCESS != rc)
        printf ("<tr>" "<td> Connection establishment failed\n </td>" "</tr>" "</table> ");
    else
    {
        sleep(5);
        rc = BTRMGR_StartAudioStreamingOut(0, handle, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
        if (BTRMGR_RESULT_SUCCESS != rc)
            printf ("<tr>" "<td> Failed to Stream out to this device\n </td>" "</tr>" "</table> ");
        else
        {
            gLastPlaying = handle;
            printf ("<tr>" "<td> Enjoy the show...\t\t :-)\n </td>" "</tr>" "</table> ");
        }
    }
}

void stopStreaming ()
{
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    rc = BTRMGR_StopAudioStreamingOut(0, gLastPlaying);
    if (BTRMGR_RESULT_SUCCESS != rc)
        printf ("<tr>" "<td> Failed to Stop Streaming... :( </td>" "</tr>" "</table> ");
    else
    {
        gLastPlaying = 0;
        printf ("<tr>" "<td> Successfully Stopped... </td>" "</tr>" "</table> ");
    }
}

void GetNumberOfAdapters ()
{
    unsigned char numOfAdapters = 0;
    if (BTRMGR_RESULT_SUCCESS != BTRMGR_GetNumberOfAdapters(&numOfAdapters))
        printf ("<tr>" "<td> Failed to get the count\n\n\n</td>" "</tr>" "</table> ");
    else
        printf ("<tr>" "<td> We found %d Bluetooth adapters in this Platform\n\n\n</td>" "</tr>" "</table> ", numOfAdapters);

    return;
}

int main ()
{
    int initOnce = 0;
    char *pInput = NULL;
    char array[32] = "";
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    while (FCGI_Accept() >= 0)
    {
        const char* func = getenv("REQUEST_URI");

        if (!func)
        {
            fprintf(stderr, "No No No.. This is totally not good..! :(\n");
        }
        else
        {
            fprintf (stderr, "The func is ..  %s\n", func);
            if (NULL != strstr (func, "/btmgr/"))
            {
                if (0 == initOnce)
                {
                    rc = BTRMGR_Init();

                    if (BTRMGR_RESULT_SUCCESS != rc)
                    {
                        fprintf (stderr, "Failed to init BTMgr.. Quiting.. \n");
                        return 0;
                    }
                    initOnce = 1;
                }

                /* Reset the input pointer to NULL; as it is being reused */
                pInput = NULL;

                /* Begin Rendeing the html page */
                printf("Content-type: text/html\r\n\r\n");
                printf ("%s", pStylePage);

                if (NULL != strstr (func, "home"))
                {
                    printf ("%s", pMainPage);
                }
                else
                {
                    if (NULL != (pInput = strstr (func, pChangePower)))
                    {
                        unsigned char power_status = 1;
                        char temp1[32] = "ON";
                        char temp2[32] = "OFF";

                        rc = BTRMGR_GetAdapterPowerStatus (0, &power_status);
                        if (BTRMGR_RESULT_SUCCESS != rc)
                            power_status = 1;

                        if (0 == power_status)
                        {
                            strcpy (temp1, "OFF");
                            strcpy (temp2, "ON");
                        }


                        printf ("<tr> "
                                "<td>The Device is Currently %s; Power %s Adapter </td> "
                                "<td> <button onclick=\"location.href='SetPower=%s';\" class=\"button\"> %s</button> </td>"
                                "</tr> "
                                "</table> "
                                " ", temp1, temp2, temp2, temp2);
                    }
                    else if (NULL != (pInput = strstr (func, pSetPower)))
                    {
                        int length = strlen(pSetPower);
                        unsigned char power = 1;
                        if (0 == strcmp("OFF", (pInput+length)))
                            power = 0;

                        rc = BTRMGR_SetAdapterPowerStatus(0, power);

                        if (BTRMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed tp Set the Power... </td>" "</tr>" "</table> ");
                        else
                            printf ("<tr>" "<td> Successfully Set the power... </td>" "</tr>" "</table> ");
                    }
                    else if (NULL != (pInput = strstr (func, pSaveName)))
                    {
                        int length = strlen(pSaveName);

                        memset (array, '\0', sizeof(array));
                        strncpy (array, (pInput+length), 30);
                        findAndReplaceSplCharWithSpace(array);

                        rc = BTRMGR_SetAdapterName(0, array);
                        if (BTRMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Set the name as %s... </td>" "</tr>" "</table> ", array);
                        else
                            printf ("<tr>" "<td> Successfully Set the name to %s... </td>" "</tr>" "</table> ", array);
                    }
                    else if (NULL != (pInput = strstr (func, pPairTo)))
                    {
                        int length = strlen(pPairTo);
                        unsigned long long int handle = 0;

                        memset (array, '\0', sizeof(array));
                        strncpy (array, (pInput+length), 30);
                        handle = strtoll(array, NULL, 0);

                        rc = BTRMGR_PairDevice(0, handle);
                        if (BTRMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Pair to %s </td>" "</tr>" "</table> ", array);
                        else
                            printf ("<tr>" "<td> Successfully Pair to %s </td>" "</tr>" "</table> ", array);

                    }
                    else if (NULL != (pInput = strstr (func, pUnPairTo)))
                    {
                        int length = strlen(pUnPairTo);
                        unsigned long long int handle = 0;

                        memset (array, '\0', sizeof(array));
                        strncpy (array, (pInput+length), 30);
                        handle = strtoll(array, NULL, 0);

                        rc = BTRMGR_UnpairDevice(0, handle);

                        if (BTRMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to UnPair to %s </td>" "</tr>" "</table> ", array);
                        else
                            printf ("<tr>" "<td> Successfully UnPair to %s </td>" "</tr>" "</table> ", array);
                    }
                    else if (NULL != (pInput = strstr (func, pGetName)))
                    {
                        memset (array, '\0', sizeof(array));

                        rc = BTRMGR_GetAdapterName(0, array);
                        if (BTRMGR_RESULT_SUCCESS != rc)
                        {
                            printf ("<tr>" "<td> Failed to Get the name of this Device </td>" "</tr>" "</table> ");
                        }
                        else
                        {
                            printf ("<tr> "
                                    "<td>Name of this Adapter is </td> "
                                    "<td><input type=\"text\" id=\"name1\" style=\"background-color:#CBD5E4;color:black\" value=%s></td> "
                                    "<td> <button id=\"nameChange\" class=\"button\" onclick=\"ChangeName()\">Change Name</button> </td> "
                                    "</tr> "
                                    "</table> "
                                    " ", array);

                        }
                    }
                    else if (NULL != (pInput = strstr (func, pStartDiscovery)))
                    {
                        rc = BTRMGR_StartDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
                        if (BTRMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Start Discovery... </td>" "</tr>" "</table> ");
                        else
                            printf ("<tr>" "<td> Discovery Start Successfully... </td>" "</tr>" "</table> ");
                    }
                    else if (NULL != (pInput = strstr (func, pShowDiscovered)))
                    {
                        BTRMGR_DiscoveredDevicesList_t discoveredDevices;
                        rc = BTRMGR_StopDeviceDiscovery(0, BTRMGR_DEVICE_OP_TYPE_AUDIO_OUTPUT);
                        if (BTRMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Stopping Discovery Failed... </td>" "</tr>" "</table> ");
                        else
                        {
                            memset (&discoveredDevices, 0, sizeof(discoveredDevices));
                            rc = BTRMGR_GetDiscoveredDevices(0, &discoveredDevices);
                            if (BTRMGR_RESULT_SUCCESS != rc)
                            {
                                printf ("<tr>" "<td> Failed to Get the Discovered Devices List... </td>" "</tr>" "</table> ");
                            }
                            else
                            {
                                int j = 0;
                                for (; j< discoveredDevices.m_numOfDevices; j++)
                                {
                                    memset (array, '\0', sizeof(array));
                                    printf ("<tr>");
                                    printf ("<td> %s </td>", discoveredDevices.m_deviceProperty[j].m_name );
                                    printf ("<td> %s </td>", discoveredDevices.m_deviceProperty[j].m_deviceAddress);
                                    sprintf (array, "%llu", discoveredDevices.m_deviceProperty[j].m_deviceHandle);
                                    printf ("<td> <button onclick=\"location.href='PairTo=%s';\" class=\"button\"> Pair This </button> </td>", array);
                                    printf ("</tr>");
                                }
                                printf ("</table>\n");
                                printf ("<p> We have %d devices discovered </p>\n", discoveredDevices.m_numOfDevices);
                            }
                        }
                    }
                    else if (NULL != (pInput = strstr (func, pShowAllPaired)))
                    {
                        BTRMGR_PairedDevicesList_t pairedDevices;
                        memset (&pairedDevices, 0, sizeof(pairedDevices));

                        rc = BTRMGR_GetPairedDevices(0, &pairedDevices);
                        if (BTRMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Get the Paired Devices List... </td>" "</tr>" "</table> ");
                        else
                        {
                            int j = 0;
                            for (; j< pairedDevices.m_numOfDevices; j++)
                            {
                                memset (array, '\0', sizeof(array));
                                printf ("<tr>");
                                printf ("<td> %s </td>", pairedDevices.m_deviceProperty[j].m_name );
                                sprintf (array, "%llu" , pairedDevices.m_deviceProperty[j].m_deviceHandle);
                                printf ("<td> <button onclick=\"location.href='StartPlaying=%s';\" class=\"button\"> StartPlaying </button> </td>", array);
                                printf ("<td> <button onclick=\"location.href='UnpairTo=%s';\" class=\"button\"> UnPair </button> </td>", array);
                                printf ("</tr>");
                            }
                            printf ("</table>\n");
                            printf ("<p> We have %d devices paired </p>\n", pairedDevices.m_numOfDevices);
                        }
                    }
                    else if (NULL != (pInput = strstr (func, pStartPlaying)))
                    {
                        int length = strlen(pStartPlaying);
                        unsigned long long int handle = 0;

                        memset (array, '\0', sizeof(array));
                        strncpy (array, (pInput+length), 30);
                        handle = strtoll(array, NULL, 0);

                        startStreaming(handle);
                    }
                    else if (NULL != (pInput = strstr (func, pStopPlaying)))
                    {
                        stopStreaming();

                    }
                    else if (NULL != strstr (func, "GetNumberOfAdapters"))
                    {
                        GetNumberOfAdapters();
                    }
                    else
                    {
                        printf ("<tr>" "<td> Seems new to me.. Let me handle this soon..  %s \n</td>" "</tr>" "</table> ", func);
                    }

                    /* Close the HTML Page */
                    printf ("%s", pGoHome);
                    printf ("</body> " "</html> ");
                }
            }
            else
            {
                fprintf (stderr, "how come it landed here!?!?!\n");
            }
        }
        FCGI_Finish();
    }

    return 0;
}


/* End of File */
