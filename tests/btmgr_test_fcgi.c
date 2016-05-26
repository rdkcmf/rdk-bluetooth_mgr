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
const char *pUnPairTo       = "UnPairTo=";
const char *pGetName        = "GetName";
const char *pStartDiscovery = "StartDiscovery";
const char *pShowDiscovered = "ShowDiscovered";
const char *pShowAllPaired  = "ShowAllPaired";
const char *pStartPlaying   = "StartPlaying=";
const char *pStopPlaying    = "StopPlaying";

const char *pMainPage = ""
            "<tr> "
            "<td> Power On/Off Adapter </td> "
            "<td> <button id=\"power\" class=\"button\" onclick=\"ChangePower()\">Go</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Get Name Of the Adapter </td> "
            "<td> <button class=\"button\" onclick=\"GetName()\">Go</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Show Paired Devices </td> "
            "<td> <button class=\"button\" onclick=\"ShowAllPaired()\">Go</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Start Discovery </td> "
            "<td> <button class=\"button\" onclick=\"StartDiscovery()\">Go</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Show Discovered Devices </td> "
            "<td> <button class=\"button\" onclick=\"ShowDiscovered()\">Go</button> </td> "
            "</tr> "
            " "
            "<tr> "
            "<td> Stop Playing </td> "
            "<td> <button class=\"button\" onclick=\"StopPlaying()\">Go</button> </td> "
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

const char* pPowerPage = ""
            "<script type=\"text/javascript\"> "
            " "
            "function SetPower() { "
            "    location.href = \"SetPower=\" + document.getElementById(\"power\").value; "
            "} "
            " "
            "</script> "
            " "
            "</body> "
            "</html> "
            " ";

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

void startStreaming (char* array)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    /* Connect to a device first */
    rc = BTMGR_ConnectToDevice(0, array, BTMGR_DEVICE_TYPE_AUDIOSINK);
    if (BTMGR_RESULT_SUCCESS != rc)
        printf ("<tr>" "<td> Connecting to %s failed\n </td>" "</tr>" "</table> ", array);
    else
    {
        rc = BTMGR_SetDefaultDeviceToStreamOut(array);
        if (BTMGR_RESULT_SUCCESS != rc)
            printf ("<tr>" "<td> Failed Set %s as the default device to streamout\n </td>" "</tr>" "</table> ", array);
        else
        {
            sleep(1);
            rc = BTMGR_StartAudioStreamingOut(0);
            if (BTMGR_RESULT_SUCCESS != rc)
                printf ("<tr>" "<td> Failed to Stream out to %s\n </td>" "</tr>" "</table> ", array);
            else
                printf ("<tr>" "<td> Enjoy the show...\t\t :-)\n </td>" "</tr>" "</table> ");
        }
    }
}

void stopStreaming (char* array)
{
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    rc = BTMGR_StopAudioStreamingOut();
    if (BTMGR_RESULT_SUCCESS != rc)
        printf ("<tr>" "<td> Failed to Stop Streaming... :( </td>" "</tr>" "</table> ");
    else
    {
#if 0
        sleep(1);
        rc = BTMGR_DisconnectFromDevice(0, array);
        if (BTMGR_RESULT_SUCCESS != rc)
            printf ("Failed to Disconnect from the device\n");
        else
            printf ("Successfully Stopped and Disconnected...\n");
#else
        printf ("<tr>" "<td> Successfully Stopped... </td>" "</tr>" "</table> ");
#endif
    }
}

void GetNumberOfAdapters ()
{
    unsigned char numOfAdapters = 0;
    if (BTMGR_RESULT_SUCCESS != BTMGR_GetNumberOfAdapters(&numOfAdapters))
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
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

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
                    rc = BTMGR_Init();

                    if (BTMGR_RESULT_SUCCESS != rc)
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
                        unsigned char power_status = 0;
                        char temp1[32] = "ON";
                        char temp2[32] = "OFF";

                        rc = BTMGR_GetAdapterPowerStatus (0, &power_status);
                        if (BTMGR_RESULT_SUCCESS != rc)
                            power_status = 1;

                        if (0 == power_status)
                        {
                            strcpy (temp1, "OFF");
                            strcpy (temp2, "ON");
                        }

                        printf ("<tr> "
                                "<td>The Device is Currently %s; Power %s Adapter </td> "
                                "<td> <button id=\"power\" class=\"button\" onclick=\"SetPower()\" value=%s>%s</button> </td> "
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

                        rc = BTMGR_SetAdapterPowerStatus(0, power);

                        if (BTMGR_RESULT_SUCCESS != rc)
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

                        rc = BTMGR_SetAdapterName(0, array);
                        if (BTMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Set the name as %s... </td>" "</tr>" "</table> ", array);
                        else
                            printf ("<tr>" "<td> Successfully Set the name to %s... </td>" "</tr>" "</table> ", array);
                    }
                    else if (NULL != (pInput = strstr (func, pPairTo)))
                    {
                        int length = strlen(pPairTo);

                        memset (array, '\0', sizeof(array));
                        strncpy (array, (pInput+length), 30);
                        findAndReplaceSplCharWithSpace(array);

                        rc = BTMGR_PairDevice(0, array);
                        if (BTMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Pair to %s </td>" "</tr>" "</table> ", array);
                        else
                            printf ("<tr>" "<td> Successfully Pair to %s </td>" "</tr>" "</table> ", array);

                    }
                    else if (NULL != (pInput = strstr (func, pUnPairTo)))
                    {
                        int length = strlen(pUnPairTo);

                        memset (array, '\0', sizeof(array));
                        strncpy (array, (pInput+length), 30);
                        findAndReplaceSplCharWithSpace(array);

                        rc = BTMGR_UnpairDevice(0, array);

                        if (BTMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to UnPair to %s </td>" "</tr>" "</table> ", array);
                        else
                            printf ("<tr>" "<td> Successfully UnPair to %s </td>" "</tr>" "</table> ", array);
                    }
                    else if (NULL != (pInput = strstr (func, pGetName)))
                    {
                        memset (array, '\0', sizeof(array));

                        rc = BTMGR_GetAdapterName(0, array);
                        if (BTMGR_RESULT_SUCCESS != rc)
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
                        rc = BTMGR_StartDeviceDiscovery(0);
                        if (BTMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Start Discovery... </td>" "</tr>" "</table> ");
                        else
                            printf ("<tr>" "<td> Discovery Start Successfully... </td>" "</tr>" "</table> ");
                    }
                    else if (NULL != (pInput = strstr (func, pShowDiscovered)))
                    {
                        BTMGR_Devices_t discoveredDevices;
                        rc = BTMGR_StopDeviceDiscovery(0);
                        if (BTMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Stopping Discovery Failed... </td>" "</tr>" "</table> ");
                        else
                        {
                            memset (&discoveredDevices, 0, sizeof(discoveredDevices));
                            rc = BTMGR_GetDiscoveredDevices(0, &discoveredDevices);
                            if (BTMGR_RESULT_SUCCESS != rc)
                            {
                                printf ("<tr>" "<td> Failed to Get the Discovered Devices List... </td>" "</tr>" "</table> ");
                            }
                            else
                            {
                                int j = 0;
                                for (; j< discoveredDevices.m_numOfDevices; j++)
                                {
                                    printf ("<tr>");
                                    printf ("<td> %s </td>", discoveredDevices.m_deviceProperty[j].m_name );
                                    printf ("<td> %s </td>", discoveredDevices.m_deviceProperty[j].m_deviceAddress);
                                    printf ("<td> <button onclick=\"location.href='PairTo=%s';\" class=\"button\"> Pair This </button> </td>", discoveredDevices.m_deviceProperty[j].m_name);
                                    printf ("</tr>");
                                }
                                printf ("</table>\n");
                            }
                        }
                    }
                    else if (NULL != (pInput = strstr (func, pShowAllPaired)))
                    {
                        BTMGR_Devices_t pairedDevices;
                        memset (&pairedDevices, 0, sizeof(pairedDevices));

                        rc = BTMGR_GetPairedDevices(0, &pairedDevices);
                        if (BTMGR_RESULT_SUCCESS != rc)
                            printf ("<tr>" "<td> Failed to Get the Paired Devices List... </td>" "</tr>" "</table> ");
                        else
                        {
                            int j = 0;
                            for (; j< pairedDevices.m_numOfDevices; j++)
                            {
                                printf ("<tr>");
                                printf ("<td> %s </td>", pairedDevices.m_deviceProperty[j].m_name );
                                printf ("<td> <button onclick=\"location.href='UnPairTo=%s';\" class=\"button\"> UnPair </button> </td>", pairedDevices.m_deviceProperty[j].m_name);
                                printf ("<td> <button onclick=\"location.href='StartPlaying=%s';\" class=\"button\"> StartPlaying </button> </td>", pairedDevices.m_deviceProperty[j].m_name);
                                printf ("</tr>");
                            }
                            printf ("</table>\n");
                        }
                    }
                    else if (NULL != (pInput = strstr (func, pStartPlaying)))
                    {
                        int length = strlen(pStartPlaying);

                        memset (array, '\0', sizeof(array));
                        strncpy (array, (pInput+length), 30);
                        findAndReplaceSplCharWithSpace(array);

                        startStreaming(array);
                    }
                    else if (NULL != (pInput = strstr (func, pStopPlaying)))
                    {
                        //memset (array, '\0', sizeof(array));
                        //strncpy (array, (pInput+length), 30);
                        //findAndReplaceSplCharWithSpace(array);

                        stopStreaming(NULL);

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
