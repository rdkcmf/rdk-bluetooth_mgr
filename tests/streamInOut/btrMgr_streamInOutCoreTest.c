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
/*BT.c file*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>     //for malloc
#include <unistd.h>     //for close?
#include <errno.h>      //for errno handling
#include <poll.h>

#include <glib.h>

#include <sys/time.h>

/* Interface lib Headers */
#include "btrCore.h"            //basic RDK BT functions
#include "btrCore_service.h"    //service UUIDs, use for service discovery

#include "btrMgr_streamOut.h"
#include "btrMgr_streamIn.h"

#include "rmfAudioCapture.h"

typedef struct appDataStruct{
    tBTRCoreHandle          hBTRCore;
    RMF_AudioCaptureHandle  hAudCap;
    tBTRMgrSoHdl            hBTRMgrSoHdl;
    tBTRMgrSiHdl            hBTRMgrSiHdl;
    unsigned long           bytesWritten;
    unsigned int            samplerate;
    unsigned int            channels;
    unsigned int            bitsPerSample;
    int                     iDataPath;
    int                     iDataReadMTU;
    int                     iDataWriteMTU;
} appDataStruct;


#define NO_ADAPTER 1234


//test func
void test_func(tBTRCoreHandle hBTRCore, stBTRCoreAdapter* pstGetAdapter);
static int streamOutTestMainAlternate (int argc, char* argv[]);
static int streamOutLiveTestMainAlternateStart (int argc, char* argv[], appDataStruct *pstAppData);
static int streamOutLiveTestMainAlternateStop (appDataStruct *pstAppData);
static int streamInLiveTestMainAlternateStart (int fd_number, int MTU_size, appDataStruct *pstAppData);
static int streamInLiveTestMainAlternateStop (appDataStruct *pstAppData);


static int acceptConnection = 0;
static int connectedDeviceIndex=0;


static void
GetTransport (
    appDataStruct* pstAppData
) {
    pstAppData->iDataPath = 0;
    pstAppData->iDataReadMTU = 0;
    pstAppData->iDataWriteMTU = 0;

    BTRCore_AcquireDeviceDataPath ( pstAppData->hBTRCore, 
                                    connectedDeviceIndex,
                                    enBTRCoreMobileAudioIn,
                                    &pstAppData->iDataPath,
                                    &pstAppData->iDataReadMTU,
                                    &pstAppData->iDataWriteMTU);

    printf("Device Data Path = %d \n",      pstAppData->iDataPath);
    printf("Device Data Read MTU = %d \n",  pstAppData->iDataReadMTU);
    printf("Device Data Write MTU= %d \n",  pstAppData->iDataWriteMTU);
}


static int
getChoice (
    void
) {
    int mychoice;
    printf("Enter a choice...\n");
    scanf("%d", &mychoice);
        getchar();//suck up a newline?
    return mychoice;
}

static char*
getEncodedSBCFile (
    void
) {
    char sbcEncodedFile[1024];
    printf("Enter SBC File location...\n");
    scanf("%s", sbcEncodedFile);
        getchar();//suck up a newline?
    return strdup(sbcEncodedFile);
}


static void 
sendSBCFileOverBT (
    char*   fileLocation,
    int     fd,
    int     mtuSize
) {
    FILE* sbcFilePtr = fopen(fileLocation, "rb");
    int    bytesLeft = 0;
    void   *encoded_buf = NULL;
    int bytesToSend = mtuSize;
    struct pollfd pollout = { fd, POLLOUT, 0 };
     int timeout;


    if (!sbcFilePtr)
        return;

    printf("fileLocation %s", fileLocation);

    fseek(sbcFilePtr, 0, SEEK_END);
    bytesLeft = ftell(sbcFilePtr);
    fseek(sbcFilePtr, 0, SEEK_SET);

    printf("File size: %d bytes\n", (int)bytesLeft);

    encoded_buf = malloc (mtuSize);

    while (bytesLeft) {

        if (bytesLeft < mtuSize)
            bytesToSend = bytesLeft;

        timeout = poll (&pollout, 1, 1000); //delay 1s to allow others to update our state

        if (timeout == 0)
            continue;
        if (timeout < 0)
            fprintf (stderr, "Bluetooth Write Error : %d\n", errno);

        // write bluetooth
        if (timeout > 0) {
            fread (encoded_buf, 1, bytesToSend, sbcFilePtr);
            write(fd, encoded_buf, bytesToSend);
            bytesLeft -= bytesToSend;
        }

        usleep(26000); //1ms delay //12.5 ms can hear words
    }

    free(encoded_buf);
    fclose(sbcFilePtr);
}


int
cb_connection_authentication (
    stBTRCoreConnCBInfo* apstConnCbInfo
) {
#if 0
    printf("\n\nConnection attempt by: %s\n",path);
#endif
    printf("Choose 35 to accept the connection/verify pin-passcode or 36 to deny the connection/discard pin-passcode\n\n");

    if (apstConnCbInfo->ui32devPassKey) {
        printf("Incoming Connection passkey = %6d\n", apstConnCbInfo->ui32devPassKey);
    }    

    do {
        usleep(20000);
    } while (acceptConnection == 0);

    printf("you picked %d\n", acceptConnection);
    if (acceptConnection == 1) {
        printf("connection accepted\n");
        acceptConnection = 0;//reset variabhle for the next connection
        return 1;
    }
    else {
        printf("connection denied\n");
        acceptConnection = 0;//reset variabhle for the next connection
        return 0;
    }
}


void
cb_unsolicited_bluetooth_status (
    stBTRCoreDevStateCBInfo* p_StatusCB,
    void*                    apvUserData
) {
    //printf("device status change: %d\n",p_StatusCB->eDeviceType);
    printf("app level cb device status change: new state is %d\n",p_StatusCB->eDeviceCurrState);
    if ((p_StatusCB->eDevicePrevState == enBTRCore_DS_Connected) && (p_StatusCB->eDeviceCurrState == enBTRCore_DS_Playing)) {
        if (p_StatusCB->eDeviceType == enBTRCoreMobileAudioIn) {
            printf("transition to playing, get the transport info...\n");
            GetTransport((appDataStruct*)apvUserData);
        }
    }

    return;
}

static void
printMenu (
    void
) {
    printf("Bluetooth Test Menu\n\n");
    printf("1. Get Current Adapter\n");
    printf("2. Scan\n");
    printf("3. Show found devices\n");
    printf("4. Pair\n");
    printf("5. UnPair/Forget a device\n");
    printf("6. Show known devices\n");
    printf("7. Connect to Headset/Speakers\n");
    printf("8. Disconnect to Headset/Speakers\n");
    printf("9. Connect as Headset/Speakerst\n");
    printf("10. Disconnect as Headset/Speakerst\n");
    printf("11. Show all Bluetooth Adapters\n");
    printf("12. Enable Bluetooth Adapter\n");
    printf("13. Disable Bluetooth Adapter\n");
    printf("14. Set Discoverable Timeout\n");
    printf("15. Set Discoverable \n");
    printf("16. Set friendly name \n");
    printf("17. Check for audio sink capability\n");
    printf("18. Check for existance of a service\n");
    printf("19. Find service details\n");
    printf("20. Check if Device Paired\n");
    printf("21. Get Connected Dev Data path\n");
    printf("22. Release Connected Dev Data path\n");
    printf("23. Send SBC data to BT Headset/Speakers\n");
    printf("24. Send WAV to BT Headset/Speakers - btrMgrStreamOutTest\n");
    printf("25. Start Send Live Audio to BT Headset/Speakers\n");
    printf("26. Start Send audio data from device to settop with BT\n");
    printf("27. Stop Send Live Audio to BT Headset/Speakers\n");
    printf("28. Stop Send audio data from device to settop with BT\n");
    printf("30. install agent for accepting connections NoInputNoOutput\n");
    printf("31. install agent for accepting connections DisplayYesNo\n");
    printf("32. Uninstall agent - allows device-initiated pairing\n");
    printf("33. Register connection-in intimation callback.\n");
    printf("34. Register connection authentication callback to allow accepting or rejection of connections.\n");
    printf("35. Accept a connection request\n");
    printf("36. Deny a connection request\n");
    printf("37. Increase Device Volume of External BT Device\n");
    printf("38. Skip to Next track on External BT Device\n");

    printf("88. debug test\n");
    printf("99. Exit\n");
}


int
main (
    void
) {
    tBTRCoreHandle lhBTRCore = NULL;

    int choice;
    int devnum;
    int default_adapter = NO_ADAPTER;
	stBTRCoreGetAdapters    GetAdapters;
    stBTRCoreAdapter        lstBTRCoreAdapter;
	stBTRCoreStartDiscovery StartDiscovery;

    char  default_path[128];
    char* agent_path = NULL;
    char myData[2048];
    int myadapter = 0;
    int bfound;
    int i;

    char *sbcEncodedFileName = NULL;
    
    char myService[16];//for testing findService API

    appDataStruct stAppData;

    memset(&stAppData, 0, sizeof(stAppData));

    snprintf(default_path, sizeof(default_path), "/org/bluez/agent_%d", getpid());

    if (!agent_path)
        agent_path = strdup(default_path);

    //call the BTRCore_init...eventually everything goes after this...
    BTRCore_Init(&lhBTRCore);

    //Init the adapter
    lstBTRCoreAdapter.bFirstAvailable = TRUE;
    if (enBTRCoreSuccess ==	BTRCore_GetAdapter(lhBTRCore, &lstBTRCoreAdapter)) {
        default_adapter = lstBTRCoreAdapter.adapter_number;
        BTRCore_LOG("GetAdapter Returns Adapter number %d\n",default_adapter);
    }
    else {
        BTRCore_LOG("No bluetooth adapter found!\n");
        return -1;
    }

    stAppData.hBTRCore = lhBTRCore;
    //register callback for unsolicted events, such as powering off a bluetooth device
    BTRCore_RegisterStatusCallback(lhBTRCore, cb_unsolicited_bluetooth_status, &stAppData);

    //display a menu of choices
    printMenu();

    do {
        printf("Enter a choice...\n");
        scanf("%d", &choice);
        getchar();//suck up a newline?
        switch (choice) {
        case 1: 
            printf("Adapter is %s\n", lstBTRCoreAdapter.pcAdapterPath);
            break;
        case 2: 
            if (default_adapter != NO_ADAPTER) {
                StartDiscovery.adapter_number = default_adapter;
                BTRCore_LOG("Looking for devices on BT adapter %d\n",StartDiscovery.adapter_number);
                StartDiscovery.duration = 13;
                BTRCore_LOG("duration %d\n",StartDiscovery.duration);
                StartDiscovery.max_devices = 10;
                BTRCore_LOG("max_devices %d\n",StartDiscovery.max_devices);
                StartDiscovery.lookup_names = TRUE;
                BTRCore_LOG("lookup_names %d\n",StartDiscovery.lookup_names);
                StartDiscovery.flags = 0;
                BTRCore_LOG("flags %d\n",StartDiscovery.flags);
                printf("Performing device scan. Please wait...\n");
                BTRCore_StartDiscovery(lhBTRCore, &StartDiscovery);
                printf("scan complete\n");
            }
            else {
                BTRCore_LOG("Error, no default_adapter set\n");
            }
            break;
        case 3:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                printf("Show Found Devices\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
            }
            break;
        case 4:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                printf("Pick a Device to Pair...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
                devnum = getChoice();

                printf(" adapter_path %s\n", lstBTRCoreAdapter.pcAdapterPath);
                printf(" agent_path %s\n",agent_path);
                if ( BTRCore_PairDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                    printf("device pairing successful.\n");
                else
                  printf("device pairing FAILED.\n");
            }
            break;
        case 5:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("UnPair/Forget a device\n");
                printf("Pick a Device to Remove...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_UnPairDevice(lhBTRCore, devnum);
            }
            break;
        case 6:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Show Known Devices...using BTRCore_GetListOfPairedDevices\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList); //TODO pass in a different structure for each adapter
            }
            break;
        case 7:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Connect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                printf("device connect process completed.\n");
            }
            break;
        case 8:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Disconnect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreSpeakers);
                printf("device disconnect process completed.\n");
            }
            break;
        case 9:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Connect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                printf("device connect process completed.\n");
            }
            break;
        case 10:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Disonnect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreMobileAudioIn);
                printf("device disconnect process completed.\n");
            }
            break;
        case 11:
            printf("Getting all available adapters\n");
            //START - adapter selection: if there is more than one adapter, offer choice of which adapter to use for pairing
            BTRCore_GetAdapters(lhBTRCore, &GetAdapters);
            if ( GetAdapters.number_of_adapters > 1) {
                printf("There are %d Bluetooth adapters\n",GetAdapters.number_of_adapters);
                printf("current adatper is %s\n", lstBTRCoreAdapter.pcAdapterPath);
                printf("Which adapter would you like to use (0 = default)?\n");
                myadapter = getChoice();

                BTRCore_SetAdapter(lhBTRCore, myadapter);
            }
            //END adapter selection
            break;
        case 12:
            lstBTRCoreAdapter.adapter_number = myadapter;
            printf("Enabling adapter %d\n",lstBTRCoreAdapter.adapter_number);
            BTRCore_EnableAdapter(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 13:
            lstBTRCoreAdapter.adapter_number = myadapter;
            printf("Disabling adapter %d\n",lstBTRCoreAdapter.adapter_number);
            BTRCore_DisableAdapter(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 14:
            printf("Enter discoverable timeout in seconds.  Zero seconds = FOREVER \n");
            lstBTRCoreAdapter.DiscoverableTimeout = getChoice();
            printf("setting DiscoverableTimeout to %d\n",lstBTRCoreAdapter.DiscoverableTimeout);
            BTRCore_SetDiscoverableTimeout(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 15:
            printf("Set discoverable.  Zero = Not Discoverable, One = Discoverable \n");
            lstBTRCoreAdapter.discoverable = getChoice();
            printf("setting discoverable to %d\n",lstBTRCoreAdapter.discoverable);
            BTRCore_SetDiscoverable(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 16:
            {
                char lcAdapterName[64] = {'\0'};
                printf("Set friendly name (up to 64 characters): \n");
                fgets(lcAdapterName, 63 , stdin);
                printf("setting name to %s\n", lcAdapterName);
                BTRCore_SetAdapterDeviceName(lhBTRCore, &lstBTRCoreAdapter, lcAdapterName);
            }
            break;
        case 17:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Check for Audio Sink capability\n");
                printf("Pick a Device to Check for Audio Sink...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                if (BTRCore_FindService(lhBTRCore, devnum, BTR_CORE_A2SNK,NULL,&bfound) == enBTRCoreSuccess) {
                    if (bfound) {
                        printf("Service UUID BTRCore_A2SNK is found\n");
                    }
                    else {
                        printf("Service UUID BTRCore_A2SNK is NOT found\n");
                    }
                }
                else {
                    printf("Error on BTRCore_FindService\n");
                }
            }
            break;
        case 18:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Find a Service\n");
                printf("Pick a Device to Check for Services...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                printf("enter UUID of desired service... e.g. 0x110b for Audio Sink\n");
                fgets(myService,sizeof(myService),stdin);
                for (i=0;i<sizeof(myService);i++)//you need to remove the final newline from the string
                      {
                    if(myService[i] == '\n')
                       myService[i] = '\0';
                    }
                bfound=0;//assume not found
                if (BTRCore_FindService(lhBTRCore, devnum, myService,NULL,&bfound) == enBTRCoreSuccess) {
                    if (bfound) {
                        printf("Service UUID %s is found\n",myService);
                    }
                    else {
                        printf("Service UUID %s is NOT found\n",myService);
                    }
                }
                else {
                    printf("Error on BTRCore_FindService\n");
                }
            }
            break;
        case 19:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Find a Service and get details\n");
                printf("Pick a Device to Check for Services...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                printf("enter UUID of desired service... e.g. 0x110b for Audio Sink\n");
                fgets(myService,sizeof(myService),stdin);
                for (i=0;i<sizeof(myService);i++)//you need to remove the final newline from the string
                      {
                    if(myService[i] == '\n')
                       myService[i] = '\0';
                    }
                bfound=0;//assume not found
                /*CAUTION! This usage is intended for development purposes.
                myData needs to be allocated large enough to hold the returned device data
                for development purposes it may be helpful for an app to gain access to this data,
                so this usage  can provide that capability.
                In most cases, simply knowing if the service exists may suffice, in which case you can use
                the simplified option where the data pointer is NULL, and no data is copied*/
                if (BTRCore_FindService(lhBTRCore, devnum,myService,myData,&bfound)  == enBTRCoreSuccess) {
                    if (bfound) {
                        printf("Service UUID %s is found\n",myService);
                        printf("Data is:\n %s \n",myData);
                    }
                    else {
                        printf("Service UUID %s is NOT found\n",myService);
                    }
                }
                else {
                    printf("Error on BTRCore_FindService\n");
                }
            }
            break;
         case 20:
            {
                stBTRCoreScannedDevicesCount lstBTRCoreScannedDevList;
                printf("Pick a Device to Find (see if it is already paired)...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices(lhBTRCore, &lstBTRCoreScannedDevList);
                devnum = getChoice();
                if ( BTRCore_FindDevice(lhBTRCore, devnum) == enBTRCoreSuccess)
                    printf("device FOUND successful.\n");
                else
                  printf("device was NOT found.\n");
            }
            break;
        case 21:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Get Data tranport parameters...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();

                stAppData.iDataPath = 0;
                stAppData.iDataReadMTU = 0;
                stAppData.iDataWriteMTU = 0;

                BTRCore_AcquireDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers, &stAppData.iDataPath, &stAppData.iDataReadMTU, &stAppData.iDataWriteMTU);

                printf("Device Data Path = %d \n", stAppData.iDataPath);
                printf("Device Data Read MTU = %d \n", stAppData.iDataReadMTU);
                printf("Device Data Write MTU= %d \n", stAppData.iDataWriteMTU);
            }
            break;
        case 22:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to ReleaseData tranport...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ReleaseDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers);

                stAppData.iDataPath      = 0;
                stAppData.iDataReadMTU   = 0;
                stAppData.iDataWriteMTU  = 0;
            }
            break;
        case 23:
            printf("Enter Encoded SBC file location to send to BT Headset/Speakers...\n");
            sbcEncodedFileName = getEncodedSBCFile();
            if (sbcEncodedFileName) {
                printf(" We will send %s to BT FD %d \n", sbcEncodedFileName, stAppData.iDataPath);
                sendSBCFileOverBT(sbcEncodedFileName, stAppData.iDataPath, stAppData.iDataWriteMTU);
                free(sbcEncodedFileName);
                sbcEncodedFileName = NULL;
            }
            else {
                printf(" Invalid file location\n");
            }
            break;
        case 24:
            printf("Sending /opt/usb/streamOutTest.wav to BT Dev FD = %d MTU = %d\n", stAppData.iDataPath, stAppData.iDataWriteMTU);
            {
                char cliDataPath[4] = {'\0'};
                char cliDataWriteMTU[8] = {'\0'};
                snprintf(cliDataPath, 4, "%d", stAppData.iDataPath);
                snprintf(cliDataWriteMTU, 8, "%d", stAppData.iDataWriteMTU);
                {
                    char *streamOutTestMainAlternateArgs[5] = {"btrMgrStreamOutTest\0", "0\0", "/opt/usb/streamOutTest.wav\0", cliDataPath, cliDataWriteMTU};
                    streamOutTestMainAlternate(5, streamOutTestMainAlternateArgs);
                }
            }
            break;
        case 25:
            printf("Start Sending Live to BT Dev FD = %d MTU = %d\n", stAppData.iDataPath, stAppData.iDataWriteMTU);
            {
                char cliDataPath[4] = {'\0'};
                char cliDataWriteMTU[8] = {'\0'};
                snprintf(cliDataPath, 4, "%d", stAppData.iDataPath);
                snprintf(cliDataWriteMTU, 8, "%d", stAppData.iDataWriteMTU);
                {
                    char *streamOutLiveTestMainAlternateArgs[5] = {"btrMgrStreamOutTest\0", "0\0", "/opt/usb/streamOutTest.wav\0", cliDataPath, cliDataWriteMTU};
                    streamOutLiveTestMainAlternateStart(5, streamOutLiveTestMainAlternateArgs, &stAppData);
                }
            }
            break;
        case 26:
             printf("Start Streaming from remote device to settop with BT Dev FD = %d MTU = %d\n", stAppData.iDataPath, stAppData.iDataReadMTU);
             streamInLiveTestMainAlternateStart(stAppData.iDataPath, stAppData.iDataReadMTU, &stAppData);
             break;
        case 27:
            printf("Stop Sending Live to BT Dev FD = %d MTU = %d\n", stAppData.iDataPath, stAppData.iDataWriteMTU);
            streamOutLiveTestMainAlternateStop(&stAppData);
            break;
        case 28:
             printf("Stop Streaming from remote device to settop with BT Dev FD = %d MTU = %d\n", stAppData.iDataPath, stAppData.iDataReadMTU);
             streamInLiveTestMainAlternateStop(&stAppData);
             break;
        case 30:
            printf("install agent - NoInputNoOutput\n");
            BTRCore_RegisterAgent(lhBTRCore, 0);// 2nd arg controls the mode, 0 = NoInputNoOutput, 1 = DisplayYesNo
            break;
        case 31:
            printf("install agent - DisplayYesNo\n");
            BTRCore_RegisterAgent(lhBTRCore, 1);// 2nd arg controls the mode, 0 = NoInputNoOutput, 1 = DisplayYesNo
            break;
        case 32:
            printf("uninstall agent - DisplayYesNo\n");
            BTRCore_UnregisterAgent(lhBTRCore);
            break;
        case 33:
            printf("register connection-in Intimation CB\n");
            BTRCore_RegisterConnectionIntimationCallback(lhBTRCore, cb_connection_authentication, NULL);
            break;
        case 34:
            printf("register authentication CB\n");
            BTRCore_RegisterConnectionAuthenticationCallback(lhBTRCore, cb_connection_authentication, NULL);
            break;
        case 35:
            printf("accept the connection\n");
            acceptConnection = 1;
            break;
        case 36:
            printf("deny the connection\n");
            acceptConnection = 2;//anything but 1 means do not connect
            break;
        case 37:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to increase volume...\n");
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_MediaPlayControl(lhBTRCore, devnum, enBTRCoreSpeakers, enBTRCoreMediaVolumeUp);
            }
            break; 
        case 38:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Go to Next track...\n");
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_MediaPlayControl(lhBTRCore, devnum, enBTRCoreMobileAudioIn, enBTRCoreMediaNext);
            }
            break; 
        case 88:
            test_func(lhBTRCore, &lstBTRCoreAdapter);
            break;
        case 99: 
            printf("Quitting program!\n");
            BTRCore_DeInit(lhBTRCore);
            exit(0);
            break;
        default: 
            printf("Available options are:\n");
            printMenu();
            break;
        }
    } while (1);

    return 0;
}


/* System Headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>




/* Local Defines */
#define IN_BUF_SIZE     2048
#define OUT_MTU_SIZE    1024


#define WAV_HEADER_RIFF_HEX    0x46464952
#define WAV_HEADER_WAV_HEX     0x45564157
#define WAV_HEADER_DC_ID_HEX   0x61746164


typedef struct _stAudioWavHeader {

    unsigned long   ulRiff32Bits;
    unsigned long   ulRiffSize32Bits;
    unsigned long   ulWave32Bits;
    unsigned long   ulWaveFmt32Bits;
    unsigned long   ulWaveHeaderLength32Bits;
    unsigned long   ulSampleRate32Bits;
    unsigned long   ulByteRate32Bits;
    unsigned long   ulMask32Bits;
    unsigned long   ulDataId32Bits;
    unsigned long   ulDataLength32Bits;

    unsigned short  usWaveHeaderFmt16Bits;
    unsigned short  usNumAudioChannels16Bits;
    unsigned short  usBitsPerChannel16Bits;
    unsigned short  usBitRate16Bits;
    unsigned short  usBlockAlign16Bits;
    unsigned short  usBitsPerSample16Bits;

    unsigned char   ucFormatArr16x8Bits[16];
    
} stAudioWavHeader;


static int 
extractWavHeader (
    FILE*               fpInAudioFile,
    stAudioWavHeader*   pstAudioWaveHeader
) {
    if (!fpInAudioFile || !pstAudioWaveHeader)
        return -1;

    if (fread(&pstAudioWaveHeader->ulRiff32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (pstAudioWaveHeader->ulRiff32Bits != WAV_HEADER_RIFF_HEX) {
        fprintf(stderr,"RAW data file detected.");

        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }   

    if (fread(&pstAudioWaveHeader->ulRiffSize32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->ulWave32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (pstAudioWaveHeader->ulWave32Bits != WAV_HEADER_WAV_HEX) {
        fprintf(stderr,"Not a WAV file.");

        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }   

    if (fread(&pstAudioWaveHeader->ulWaveFmt32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->ulWaveHeaderLength32Bits,4,1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->usWaveHeaderFmt16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (fread(&pstAudioWaveHeader->usNumAudioChannels16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (pstAudioWaveHeader->usNumAudioChannels16Bits > 2) {
        fprintf(stderr,"Invalid number of usNumAudioChannels16Bits (%u) specified.", pstAudioWaveHeader->usNumAudioChannels16Bits);

        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }   

    if (fread(&pstAudioWaveHeader->ulSampleRate32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (fread(&pstAudioWaveHeader->ulByteRate32Bits, 4, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (fread(&pstAudioWaveHeader->usBitsPerChannel16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }

    if (fread(&pstAudioWaveHeader->usBitRate16Bits, 2, 1, fpInAudioFile) != 1) {
        fseek( fpInAudioFile, 0, SEEK_SET);
        return -1; 
    }


    if (pstAudioWaveHeader->ulWaveHeaderLength32Bits == 40 && pstAudioWaveHeader->usWaveHeaderFmt16Bits == 0xfffe) {

        if (fread(&pstAudioWaveHeader->usBlockAlign16Bits,2,1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (fread(&pstAudioWaveHeader->usBitsPerSample16Bits, 2, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (fread(&pstAudioWaveHeader->ulMask32Bits, 4, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        fread(&pstAudioWaveHeader->ucFormatArr16x8Bits, 16, 1, fpInAudioFile);
    }   
    else if (pstAudioWaveHeader->ulWaveHeaderLength32Bits == 18 && pstAudioWaveHeader->usWaveHeaderFmt16Bits == 1) {

        if (fread(&pstAudioWaveHeader->usBlockAlign16Bits, 2, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

    }   
    else if (pstAudioWaveHeader->ulWaveHeaderLength32Bits != 16 && pstAudioWaveHeader->usWaveHeaderFmt16Bits != 1) {
        fprintf(stderr,"No PCM data in WAV file. ulWaveHeaderLength32Bits = %lu, Format 0x%x", pstAudioWaveHeader->ulWaveHeaderLength32Bits,pstAudioWaveHeader->usWaveHeaderFmt16Bits);
    }   

    do {
        if (fread(&pstAudioWaveHeader->ulDataId32Bits, 4, 1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (fread(&pstAudioWaveHeader->ulDataLength32Bits,4,1, fpInAudioFile) != 1) {
            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }

        if (pstAudioWaveHeader->ulDataId32Bits == WAV_HEADER_DC_ID_HEX) {
            break;
        }
        if (fseek( fpInAudioFile, pstAudioWaveHeader->ulDataLength32Bits, SEEK_CUR)) {
            fprintf(stderr,"Incomplete chunk found WAV file.");

            fseek( fpInAudioFile, 0, SEEK_SET);
            return -1; 
        }
    } while(1);  

    return 0;
}


static void
helpMenu (
    void
) {
        fprintf(stderr,"\nbtrMgrStreamOutTest <Mode: 0=BTDev/1=FWrite> <Input Wav File> <BTDev FD/Output File> <Out BTDevMTU/FileWriteBlock Size>\n");
        return;
}

typedef void*  tBTRMgrCapSoHandle;

typedef struct _stBTRMgrCapSoHdl {
    FILE*   inFileFp;
    char*   inDataBuf;     
    int     inFileBytesLeft; 
    int     inBytesToEncode;
    int     inBufSize;      
    tBTRMgrSoHdl hBTRMgrSoHdl;
} stBTRMgrCapSoHdl;


void*
doDataCapture (
    void* ptr
) {
    tBTRMgrCapSoHandle  hBTRMgrSoCap = NULL;
    int*                penCapThreadExitStatus = malloc(sizeof(int));

    FILE*   inFileFp        = NULL;
    char*   inDataBuf      = NULL;
    int     inFileBytesLeft = 0;
    int     inBytesToEncode = 0;
    int     inBufSize       = IN_BUF_SIZE;
    tBTRMgrSoHdl hBTRMgrSoHdl;

    struct timeval tv;
    unsigned long int    prevTime = 0, currTime = 0;

    hBTRMgrSoCap = (stBTRMgrCapSoHdl*) ptr;
    printf("%s \n", "Capture Thread Started");

    if (!((stBTRMgrCapSoHdl*)hBTRMgrSoCap)) {
        fprintf(stderr, "Capture thread failure - BTRMgr Capture not initialized\n");
        *penCapThreadExitStatus = -1;
        return (void*)penCapThreadExitStatus;
    }

    inFileFp        = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inFileFp; 
    inDataBuf       = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inDataBuf;
    inFileBytesLeft = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inFileBytesLeft;
    inBytesToEncode = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inBytesToEncode;
    inBufSize       = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inBufSize;
    hBTRMgrSoHdl    = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->hBTRMgrSoHdl;

    while (inFileBytesLeft) {

        if (inFileBytesLeft < inBufSize)
            inBytesToEncode = inFileBytesLeft;

        g_thread_yield();

        fread (inDataBuf, 1, inBytesToEncode, inFileFp);
      
        gettimeofday(&tv,NULL);
        currTime = (1000000 * tv.tv_sec) + tv.tv_usec;
        printf("inBytesToEncode = %d sleeptime = %lf Processing =%lu\n", inBytesToEncode, (inBytesToEncode * 1000000.0/192000.0) - 1666.666667, currTime - prevTime);
        prevTime = currTime;

        BTRMgr_SO_SendBuffer(hBTRMgrSoHdl, inDataBuf, inBytesToEncode);
        inFileBytesLeft -= inBytesToEncode;
        usleep((inBytesToEncode * 1000000.0/192000.0) - 1666.666667);
    }

    *penCapThreadExitStatus = 0;
    return (void*)penCapThreadExitStatus;
}



static int 
streamOutTestMainAlternate (
    int     argc,
    char*   argv[]
) {

    char    *inDataBuf      = NULL;

    int     inFileBytesLeft = 0;
    int     inBytesToEncode = 0;
    int     inBufSize       = IN_BUF_SIZE;

    int     streamOutMode   = 0;
    FILE*   inFileFp        = NULL;
    FILE*   outFileFp       = NULL;
    int     outFileFd       = 0;
    int     outMTUSize      = OUT_MTU_SIZE;

#if 0
    struct timeval tv;
    unsigned long int    prevTime = 0, currTime = 0;
#endif

    tBTRMgrSoHdl     hBTRMgrSoHdl;
    stAudioWavHeader lstAudioWavHeader;

    if (argc != 5) {
        fprintf(stderr,"Invalid number of arguments\n");
        helpMenu();
        return -1;
    }

    printf("%s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3], argv[4]);

    streamOutMode = atoi(argv[1]);

    inFileFp    = fopen(argv[2], "rb");
    if (!inFileFp) {
        fprintf(stderr,"Invalid input file\n");
        helpMenu();
        return -1;
    }

    printf("streamOutMode = %d\n", streamOutMode);
    if (streamOutMode == 0) {
        outFileFd = atoi(argv[3]);
    }
    else if (streamOutMode == 1) {
        outFileFp   = fopen(argv[3], "wb");
        if (!outFileFp) {
            fprintf(stderr,"Invalid output file\n");
            helpMenu();
            return -1;
        }

        outFileFd = fileno(outFileFp);
    }
    else {
        helpMenu();
        return -1;
    }

    outMTUSize  = atoi(argv[4]);


    printf("outFileFd = %d\n", outFileFd);
    printf("outMTUSize = %d\n", outMTUSize);

    fseek(inFileFp, 0, SEEK_END);
    inFileBytesLeft = ftell(inFileFp);
    fseek(inFileFp, 0, SEEK_SET);

#if defined(DISABLE_AUDIO_ENCODING)
    (void)extractWavHeader;
#else
    memset(&lstAudioWavHeader, 0, sizeof(lstAudioWavHeader));
    if (extractWavHeader(inFileFp, &lstAudioWavHeader)) {
        fprintf(stderr,"Invalid output file\n");
        return -1;
    }
#endif

    BTRMgr_SO_Init(&hBTRMgrSoHdl);

    inDataBuf = (char*)malloc(inBufSize * sizeof(char));
    inBytesToEncode = inBufSize;

    BTRMgr_SO_Start(hBTRMgrSoHdl, inBytesToEncode, outFileFd, outMTUSize);

#if 0
    while (inFileBytesLeft) {

        if (inFileBytesLeft < inBufSize)
            inBytesToEncode = inFileBytesLeft;
       
        usleep((inBytesToEncode * 1000000.0/192000.0) - 1666.666667);

        gettimeofday(&tv,NULL);
        currTime = (1000000 * tv.tv_sec) + tv.tv_usec;
        printf("inBytesToEncode = %d sleeptime = %lf Processing =%lu\n", inBytesToEncode, (inBytesToEncode * 1000000.0/192000.0) - 1666.666667, currTime - prevTime);
        prevTime = currTime;

        fread (inDataBuf, 1, inBytesToEncode, inFileFp);
        BTRMgr_SO_SendBuffer(hBTRMgrSoHdl, inDataBuf, inBytesToEncode);
        inFileBytesLeft -= inBytesToEncode;
    }
#else
    GThread*    dataCapThread = NULL;
    void*      penDataCapThreadExitStatus = NULL;

    stBTRMgrCapSoHdl lstBTRMgrCapSoHdl;

    lstBTRMgrCapSoHdl.inFileFp        = inFileFp;
    lstBTRMgrCapSoHdl.inDataBuf       = inDataBuf;
    lstBTRMgrCapSoHdl.inFileBytesLeft = inFileBytesLeft;
    lstBTRMgrCapSoHdl.inBytesToEncode = inBytesToEncode;
    lstBTRMgrCapSoHdl.inBufSize       = inBufSize;
    lstBTRMgrCapSoHdl.hBTRMgrSoHdl    = hBTRMgrSoHdl;

    if((dataCapThread = g_thread_new(NULL, doDataCapture, (void*)&lstBTRMgrCapSoHdl)) == NULL) {
         fprintf(stderr, "%s:%d:%s - Failed to create data Capture Thread\n", __FILE__, __LINE__, __FUNCTION__);
    }

    penDataCapThreadExitStatus = g_thread_join(dataCapThread);

    (void)penDataCapThreadExitStatus;
#endif

    BTRMgr_SO_SendEOS(hBTRMgrSoHdl);

    BTRMgr_SO_Stop(hBTRMgrSoHdl);

    free(inDataBuf);

    BTRMgr_SO_DeInit(hBTRMgrSoHdl);

    if (streamOutMode == 1)
        fclose(outFileFp);

    fclose(inFileFp);

    return 0;
}


rmf_Error 
cbBufferReady (
    void*        context, 
    void*        inDataBuf, 
    unsigned int inBytesToEncode
) {

#if 0
    struct timeval tv;
    static unsigned long int prevCbTime = 0;
    unsigned long int currTime = 0;

    gettimeofday(&tv,NULL);
    currTime = (1000000 * tv.tv_sec) + tv.tv_usec;
    printf("CB Interval uSecs =%lu\n", currTime - prevCbTime);
    prevCbTime = currTime;
#endif

    appDataStruct *data = (appDataStruct*) context;
    BTRMgr_SO_SendBuffer(data->hBTRMgrSoHdl, inDataBuf, inBytesToEncode);

    data->bytesWritten += inBytesToEncode;
#if 0
    printf("audioCaptureCb size=%06d bytesWritten=%06lu\n", inBytesToEncode, data->bytesWritten);
    fflush(stdout);

    gettimeofday(&tv,NULL);
    printf("CB Time uSec =%lu\n", ((1000000 * tv.tv_sec) + tv.tv_usec) - currTime);
#endif

    return RMF_SUCCESS;
}


static int 
streamOutLiveTestMainAlternateStart (
    int             argc, 
    char*           argv[],
    appDataStruct*  pstAppData
)  {

    int     inBytesToEncode = IN_BUF_SIZE;
    int     outFileFd       = 0;
    int     outMTUSize      = OUT_MTU_SIZE;


    if (argc != 5) {
        fprintf(stderr,"Invalid number of arguments\n");
        helpMenu();
        return -1;
    }

    printf("%s %s %s %s %s\n", argv[0], argv[1], argv[2], argv[3], argv[4]);

    outFileFd = atoi(argv[3]);
    outMTUSize  = atoi(argv[4]);


    RMF_AudioCapture_Settings settings;



    BTRMgr_SO_Init(&(pstAppData->hBTRMgrSoHdl));

    /* could get defaults from audio capture, but for the sample app we want to write a the wav header first*/
    pstAppData->bitsPerSample = 16;
    pstAppData->samplerate = 48000;
    pstAppData->channels = 2;

    if (RMF_AudioCapture_Open(&(pstAppData->hAudCap))) {
        goto err_open;
    }

    RMF_AudioCapture_GetDefaultSettings(&settings);
    settings.cbBufferReady      = cbBufferReady;
    settings.cbBufferReadyParm  = pstAppData;

    settings.fifoSize = 16 * inBytesToEncode;
    settings.threshold= inBytesToEncode;


    BTRMgr_SO_Start(pstAppData->hBTRMgrSoHdl, inBytesToEncode, outFileFd, outMTUSize);

    if (RMF_AudioCapture_Start(pstAppData->hAudCap, &settings)) {
        goto err_open;
    }

    printf("BT AUDIO OUT - STARTED \n");

err_open:
    return 0;
}


static int 
streamOutLiveTestMainAlternateStop (
    appDataStruct*  pstAppData
)  {
    RMF_AudioCapture_Stop(pstAppData->hAudCap);

    BTRMgr_SO_SendEOS(pstAppData->hBTRMgrSoHdl);
    BTRMgr_SO_Stop(pstAppData->hBTRMgrSoHdl);


    RMF_AudioCapture_Close(pstAppData->hAudCap);
    BTRMgr_SO_DeInit(pstAppData->hBTRMgrSoHdl);

    printf("BT AUDIO OUT - STOPPED\n");

    return 0;
}


static int
streamInLiveTestMainAlternateStart (
    int             fd_number,
    int             MTU_size,
    appDataStruct*  pstAppData
) {

    int     inBytesToEncode= IN_BUF_SIZE;
    int     inFileFd       = fd_number;
    int     inMTUSize      = MTU_size;



    BTRMgr_SI_Init(&pstAppData->hBTRMgrSiHdl);

    /* could get defaults from audio capture, but for the sample app we want to write a the wav header first*/
    pstAppData->bitsPerSample = 16;
    pstAppData->samplerate = 48000;
    pstAppData->channels = 2;


    BTRMgr_SI_Start(pstAppData->hBTRMgrSiHdl, inBytesToEncode, inFileFd, inMTUSize);

    printf("BT AUDIO IN - STARTED \n");

    return 0;
}


static int
streamInLiveTestMainAlternateStop (
    appDataStruct*  pstAppData
) {
    BTRMgr_SI_SendEOS(pstAppData->hBTRMgrSiHdl);
    BTRMgr_SI_Stop(pstAppData->hBTRMgrSiHdl);

    BTRMgr_SI_DeInit(pstAppData->hBTRMgrSiHdl);

    printf("BT AUDIO IN - STOPPED\n");

    return 0;
}
