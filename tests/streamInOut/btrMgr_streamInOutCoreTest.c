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

#include "btrMgr_Types.h"
#include "btrMgr_mediaTypes.h"
#include "btrMgr_streamOut.h"
#include "btrMgr_streamIn.h"
#include "btrMgr_audioCap.h"


typedef struct appDataStruct{
    tBTRCoreHandle          hBTRCore;
    stBTRCoreDevMediaInfo   stBtrCoreDevMediaInfo;
    tBTRMgrAcHdl            hBTRMgrAcHdl;
    tBTRMgrSoHdl            hBTRMgrSoHdl;
    tBTRMgrSiHdl            hBTRMgrSiHdl;
    unsigned long           bytesWritten;
    unsigned int            inSampleRate;
    unsigned int            inChannels;
    unsigned int            inBitsPerSample;
    int                     iDataPath;
    int                     iDataReadMTU;
    int                     iDataWriteMTU;
} appDataStruct;


#define NO_ADAPTER 1234


//test func
static int streamOutTestMainAlternate (int argc, char* argv[], appDataStruct *pstAppData);
static int streamOutLiveTestMainAlternateStart (int argc, char* argv[], appDataStruct *pstAppData);
static int streamOutLiveTestMainAlternateStop (appDataStruct *pstAppData);
static int streamInLiveTestMainAlternateStart (int fd_number, int MTU_size, unsigned int aui32InSFreq, appDataStruct *pstAppData);
static int streamInLiveTestMainAlternateStop (appDataStruct *pstAppData);


static int acceptConnection = 0;
static int connectedDeviceIndex=0;


static void
GetTransport (
    appDataStruct* pstAppData
) {

	if (!pstAppData)
		return;

    BTRCore_GetDeviceMediaInfo ( pstAppData->hBTRCore, connectedDeviceIndex, enBTRCoreMobileAudioIn, &pstAppData->stBtrCoreDevMediaInfo);

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

    if (pstAppData->stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
		if (pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
        	printf("Device Media Info SFreq         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
        	printf("Device Media Info AChan         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
        	printf("Device Media Info SbcAllocMethod= %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
        	printf("Device Media Info SbcSubbands   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
        	printf("Device Media Info SbcBlockLength= %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
        	printf("Device Media Info SbcMinBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
        	printf("Device Media Info SbcMaxBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
        	printf("Device Media Info SbcFrameLen   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
        	printf("Device Media Info SbcBitrate    = %d\n", ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
		}
    }
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


enBTRCoreRet
cb_connection_intimation (
    stBTRCoreConnCBInfo* apstConnCbInfo,
    int*                 api32ConnInIntimResp,
    void*                apvUserData
) {
    printf("Choose 35 to verify pin-passcode or 36 to discard pin-passcode\n\n");

    if (apstConnCbInfo->ui32devPassKey) {
        printf("Incoming Connection passkey = %6d\n", apstConnCbInfo->ui32devPassKey);
    }    

    do {
        usleep(20000);
    } while (acceptConnection == 0);

    printf("you picked %d\n", acceptConnection);
    if (acceptConnection == 1) {
        printf("Pin-Passcode accepted\n");
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInIntimResp = 1;
    }
    else {
        printf("Pin-Passcode denied\n");
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInIntimResp = 0;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
cb_connection_authentication (
    stBTRCoreConnCBInfo* apstConnCbInfo,
    int*                 api32ConnInAuthResp,
    void*                apvUserData
) {
    printf("Choose 35 to accept the connection or 36 to deny the connection\n\n");

    do {
        usleep(20000);
    } while (acceptConnection == 0);

    printf("you picked %d\n", acceptConnection);
    if (acceptConnection == 1) {
        printf("connection accepted\n");
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInAuthResp = 1;
    }
    else {
        printf("connection denied\n");
        acceptConnection = 0;//reset variabhle for the next connection
        *api32ConnInAuthResp = 0;
    }

    return enBTRCoreSuccess;
}


enBTRCoreRet
cb_unsolicited_bluetooth_status (
    stBTRCoreDevStatusCBInfo*   p_StatusCB,
    void*                       apvUserData
) {
    //printf("device status change: %d\n",p_StatusCB->eDeviceType);
    printf("app level cb device status change: new state is %d\n",p_StatusCB->eDeviceCurrState);
    if ((p_StatusCB->eDevicePrevState == enBTRCoreDevStConnected) && (p_StatusCB->eDeviceCurrState == enBTRCoreDevStPlaying)) {
        if (p_StatusCB->eDeviceType == enBTRCoreMobileAudioIn) {
            printf("transition to playing, get the transport info...\n");
            GetTransport((appDataStruct*)apvUserData);
        }
    }

    if ((p_StatusCB->eDevicePrevState == enBTRCoreDevStPlaying) && (p_StatusCB->eDeviceCurrState == enBTRCoreDevStDisconnected)) {
        if ((p_StatusCB->eDeviceType == enBTRCoreSpeakers) || (p_StatusCB->eDeviceType == enBTRCoreHeadSet)) {
            printf("DISCONNECTED WHILE PLAYING...\n");
            streamOutLiveTestMainAlternateStop((appDataStruct*)apvUserData);
        }
    }

    return enBTRCoreSuccess;
}

static int
getBitsToString (
    unsigned short  flagBits
) {
    unsigned char i = 0;
    for (; i<16; i++) {
        printf("%d", (flagBits >> i) & 1);
    }

    return 0;
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
    printf("38. Perform Media Control Options on External BT Device\n");
    printf("39. Play wav file forever\n");
    printf("40. Check if Device Is Connectable\n");
    printf("41. Get Track Information for Current Media item\n");
    printf("42. Get Media Properties\n");
    printf("43. Scan for LE Devices\n");
    printf("44. Connect to the scanned LE Device\n");
    printf("45. Get LE Property\n");
    printf("46. Perform LE Operation\n");
    printf("47. Connect to HID/Unknown\n");
    printf("48. Disconnect to HID/Unknown\n");
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
        printf("GetAdapter Returns Adapter number %d\n",default_adapter);
    }
    else {
        printf("No bluetooth adapter found!\n");
        return -1;
    }

    stAppData.hBTRCore = lhBTRCore;
    //register callback for unsolicted events, such as powering off a bluetooth device
    BTRCore_RegisterStatusCb(lhBTRCore, cb_unsolicited_bluetooth_status, &stAppData);

    stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaSbcInfo) > sizeof(stBTRCoreDevMediaMpegInfo)) ? sizeof(stBTRCoreDevMediaSbcInfo) : sizeof(stBTRCoreDevMediaMpegInfo));

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
                printf("Looking for devices on BT adapter %s\n", lstBTRCoreAdapter.pcAdapterPath);
                printf("Performing device scan for 15 seconds . Please wait...\n");
                BTRCore_StartDiscovery(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, enBTRCoreUnknown, 15);
                printf("scan complete\n");
            }
            else {
                printf("Error, no default_adapter set\n");
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
            BTRCore_SetAdapterDiscoverableTimeout(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, lstBTRCoreAdapter.DiscoverableTimeout);
            break;
        case 15:
            printf("Set discoverable.  Zero = Not Discoverable, One = Discoverable \n");
            lstBTRCoreAdapter.discoverable = getChoice();
            printf("setting discoverable to %d\n", lstBTRCoreAdapter.discoverable);
            BTRCore_SetAdapterDiscoverable(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, lstBTRCoreAdapter.discoverable);
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
                if (BTRCore_FindService(lhBTRCore, devnum, BTR_CORE_A2SNK, NULL, &bfound) == enBTRCoreSuccess) {
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

                BTRCore_GetDeviceMediaInfo(lhBTRCore, devnum, enBTRCoreSpeakers, &stAppData.stBtrCoreDevMediaInfo);

                stAppData.iDataPath = 0;
                stAppData.iDataReadMTU = 0;
                stAppData.iDataWriteMTU = 0;

                BTRCore_AcquireDeviceDataPath(lhBTRCore, devnum, enBTRCoreSpeakers, &stAppData.iDataPath, &stAppData.iDataReadMTU, &stAppData.iDataWriteMTU);

                printf("Device Data Path = %d \n", stAppData.iDataPath);
                printf("Device Data Read MTU = %d \n", stAppData.iDataReadMTU);
                printf("Device Data Write MTU= %d \n", stAppData.iDataWriteMTU);

                if (stAppData.stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
					if (stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo) {
                    	printf("Device Media Info SFreq         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq);
                    	printf("Device Media Info AChan         = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->eDevMAChan);
                    	printf("Device Media Info SbcAllocMethod= %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcAllocMethod);
                    	printf("Device Media Info SbcSubbands   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcSubbands);
                    	printf("Device Media Info SbcBlockLength= %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcBlockLength);
                    	printf("Device Media Info SbcMinBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMinBitpool);
                    	printf("Device Media Info SbcMaxBitpool = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui8DevMSbcMaxBitpool);
                    	printf("Device Media Info SbcFrameLen   = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcFrameLen);
                    	printf("Device Media Info SbcBitrate    = %d\n", ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui16DevMSbcBitrate);
					}
                }
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
                    streamOutTestMainAlternate(5, streamOutTestMainAlternateArgs, &stAppData);
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
            streamInLiveTestMainAlternateStart(stAppData.iDataPath, stAppData.iDataReadMTU, ((stBTRCoreDevMediaSbcInfo*)(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo))->ui32DevMSFreq, &stAppData);
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
            BTRCore_RegisterConnectionIntimationCb(lhBTRCore, cb_connection_intimation, NULL);
            break;
        case 34:
            printf("register authentication CB\n");
            BTRCore_RegisterConnectionAuthenticationCb(lhBTRCore, cb_connection_authentication, NULL);
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
                enBTRCoreMediaCtrl mediaCtrl = enBTRCoreMediaCtrlVolumeUp;
                printf("Pick a Device to increase volume...\n");
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();

                if (enBTRCoreSuccess != BTRCore_MediaControl(lhBTRCore, devnum, enBTRCoreSpeakers, mediaCtrl))
                   printf("Failed  to set Volume!!!\n");
            }
            break; 
        case 38:
            {
                int ch = 0;
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to perform media control...\n");
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                printf("Enter Media choice ....(0-Play/1-Pause/2-Stop/3-Next/4-Previous)\n");
                ch = getChoice();

                if (enBTRCoreSuccess != BTRCore_MediaControl(lhBTRCore, devnum, enBTRCoreMobileAudioIn, ch))
                   printf("Failed  to set the Media Control Option!!!\n");
            }
            break;
        case 39:
            printf("play wav file forever\n");
            printf("Sending /opt/usb/streamOutTest.wav to BT Dev FD = %d MTU = %d\n", stAppData.iDataPath, stAppData.iDataWriteMTU);
            {
                char cliDataPath[4] = {'\0'};
                char cliDataWriteMTU[8] = {'\0'};
                snprintf(cliDataPath, 4, "%d", stAppData.iDataPath);
                snprintf(cliDataWriteMTU, 8, "%d", stAppData.iDataWriteMTU);
                char *streamOutTestMainAlternateArgs[5] = {"btrMgrStreamOutTest\0", "0\0", "/opt/usb/streamOutTest.wav\0", cliDataPath, cliDataWriteMTU};
                do
                {
                    streamOutTestMainAlternate(5, streamOutTestMainAlternateArgs, &stAppData);
                } while (1);
            }
            break;
        case 40:
            printf("Pick a Device to Check if Connectable...\n");
            devnum = getChoice();
            BTRCore_IsDeviceConnectable(lhBTRCore, devnum);
            break;
        case 41:
            {
                stBTRCoreMediaTrackInfo     lstBTRCoreMediaTrackInfo;
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to get media track information...\n");
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                if (enBTRCoreSuccess != BTRCore_GetMediaTrackInfo(lhBTRCore, devnum, enBTRCoreMobileAudioIn, &lstBTRCoreMediaTrackInfo))
                   printf("Failed  to get Media Track Info!!!\n");
                else
                   printf("\n\n---Current Media Track Info---\n\n"
                          "Album           : %s\n"
                          "Artist          : %s\n"
                          "Title           : %s\n"
                          "Genre           : %s\n"
                          "NumberOfTracks  : %d\n"
                          "TrackNumber     : %d\n"
                          "Duration        : %d\n"
                          , lstBTRCoreMediaTrackInfo.pcAlbum
                          , lstBTRCoreMediaTrackInfo.pcArtist
                          , lstBTRCoreMediaTrackInfo.pcTitle
                          , lstBTRCoreMediaTrackInfo.pcGenre
                          , lstBTRCoreMediaTrackInfo.ui32NumberOfTracks
                          , lstBTRCoreMediaTrackInfo.ui32TrackNumber
                          , lstBTRCoreMediaTrackInfo.ui32Duration);
              }
              break;
        case 42:
              {
                enBTRCoreRet rc = enBTRCoreSuccess;
                char prop[64] = "\0";
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to get Media property...\n");
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                printf("Query for any of the mentioned property....(Name|Type|Subtype|Position|Status|Repeat|Shuffle|Browsable|Searchable)\n");
                scanf("%s",prop);

                if (!strcmp(prop, "Position") || !strcmp(prop, "Browsable") || !strcmp(prop, "Searchable")) {
                   unsigned int val = 0;
                   if ((rc = BTRCore_GetMediaProperty (lhBTRCore, devnum, enBTRCoreMobileAudioIn, prop, (void*)&val)))
                      printf("\n %s : %d\n", prop, val);
                }
                else if (!strcmp(prop, "Name") || !strcmp(prop, "Status") || !strcmp(prop, "Subtype") ||
                         !strcmp(prop, "Type") || !strcmp(prop, "Repeat") || !strcmp(prop, "Shuffle")) { 
                   char* val = "\0";
                   if ((rc = BTRCore_GetMediaProperty (lhBTRCore, devnum, enBTRCoreMobileAudioIn, prop, (void*)&val)))
                      printf("\n %s : %s\n", prop, val);
                }
                else
                   printf("\n Enter valid property...\n");
                
                if (enBTRCoreSuccess != rc)
                   printf("Failed  to get Media Property!!!\n");
              }  
              break;  
        case 43:
            if (default_adapter != NO_ADAPTER) {
                printf("%d\t: %s - Looking for LE devices on BT adapter %s\n", __LINE__, __FUNCTION__, lstBTRCoreAdapter.pcAdapterPath);
                printf("%d\t: %s - Performing LE scan for 30 seconds . Please wait...\n", __LINE__, __FUNCTION__);
                BTRCore_StartDiscovery(lhBTRCore, lstBTRCoreAdapter.pcAdapterPath, enBTRCoreLE, 30);
                printf("%d\t: %s - scan complete\n", __LINE__, __FUNCTION__);
            }
            else {
                printf("%d\t: %s - Error, no default_adapter set\n", __LINE__, __FUNCTION__);
            }
            break;
        case 44: 
            {
                stBTRCoreScannedDevicesCount lListOfScannedDevices;
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfScannedDevices (lhBTRCore, &lListOfScannedDevices);
                printf("Pick a Device to Connect...\n");
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreLE);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection
                printf("device connect process completed.\n");
                break;
            }
        case 45:
            {
                char leUuidString[BTRCORE_UUID_LEN] = "\0";
                unsigned char propSelection = 0;
                printf("Pick a Device to Connect...\n");
                devnum = getChoice();
                printf ("Enter the UUID : ");
                scanf ("%s", leUuidString);
                printf ("Select the property to query...\n");
                printf ("[0 - Uuid | 1 - Primary | 2 - Device | 3 - Service | 4 - Value |"
                        " 5 - Notifying | 6 - Flags | 7 -Character]\n");
                propSelection = getChoice();

                if (!propSelection) {
                    stBTRCoreUUIDList lstBTRCoreUUIDList;
                    unsigned char i = 0;
                    BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&lstBTRCoreUUIDList);
                    if (lstBTRCoreUUIDList.numberOfUUID) {
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
                        printf("\n\t%-40s Flag\n", "UUID List");
                        for (; i<lstBTRCoreUUIDList.numberOfUUID; i++) {
                            printf("\n\t%-40s ", lstBTRCoreUUIDList.uuidList[i].uuid);
                            getBitsToString(lstBTRCoreUUIDList.uuidList[i].flags);
                        }
                    } else {
                        printf ("\n\n\tNo UUIDs Found...\n");
                    }
                } else
                if (propSelection == 1 || propSelection == 5) {
                    unsigned char val = 0;
                    BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&val);
                    printf("\n\tResult : %d\n", val);
                } else
                if (propSelection == 4 || propSelection == 2 ||
                    propSelection == 3 || propSelection == 7 ){
                    char val[BTRCORE_MAX_STR_LEN] = "\0";
                    BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&val);
                    printf("\n\tResult : %s\n", val);
                } else
                if (propSelection == 6) {
                    char val[15][64]; int i=0;
                    memset (val, 0, sizeof(val));
                    BTRCore_GetLEProperty(lhBTRCore, devnum, leUuidString, propSelection, (void*)&val);
                    printf("\n\tResult :\n");
                    for (; i < 15 && val[i][0]; i++) {
                        printf("\t- %s\n", val[i]);
                    }
                }
            }
            break;
        case 46:
            {
                char leUuidString[BTRCORE_UUID_LEN] = "\0";
                printf("%d\t: %s - Pick a connected LE Device to Perform Operation.\n", __LINE__, __FUNCTION__);
                printf("Pick a Device to Connect...\n");
                devnum = getChoice();
                printf ("Enter the UUID : ");
                scanf ("%s", leUuidString);

                printf("%d\t: %s - Pick a option to perform method operation LE.\n", __LINE__, __FUNCTION__);
                printf("\t[0 - ReadValue | 1 - WriteValue | 2 - StartNotify | 3 - StopNotify]\n");

                enBTRCoreLeOp aenBTRCoreLeOp = getChoice();

                if (leUuidString[0]) {
                    char val[BTRCORE_MAX_STR_LEN] = "\0";
                    BTRCore_PerformLEOp (lhBTRCore, devnum, leUuidString, aenBTRCoreLeOp, (void*)&val);
                    if (aenBTRCoreLeOp == 0) {
                        printf("%d\t: %s - Obtained Value [%s]\n", __LINE__, __FUNCTION__, val  );
                    }
                }
            }
            break;
        case 47:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Connect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_ConnectDevice(lhBTRCore, devnum, enBTRCoreUnknown);
                connectedDeviceIndex = devnum; //TODO update this if remote device initiates connection.
                printf("device connect process completed.\n");
            }
            break;
        case 48:
            {
                stBTRCorePairedDevicesCount lstBTRCorePairedDevList;
                printf("Pick a Device to Disconnect...\n");
                lstBTRCoreAdapter.adapter_number = myadapter;
                BTRCore_GetListOfPairedDevices(lhBTRCore, &lstBTRCorePairedDevList);
                devnum = getChoice();
                BTRCore_DisconnectDevice(lhBTRCore, devnum, enBTRCoreUnknown);
                printf("device disconnect process completed.\n");
            }
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


    if (stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo)
        free(stAppData.stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo);

    return 0;
}


/* System Headers */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>




/* Local Defines */
#define IN_BUF_SIZE     3072 // Corresponds to MTU size of 895 TODO: Arrive at this value by calculating
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
    FILE*           inFileFp;
    char*           inDataBuf;     
    int             inFileBytesLeft; 
    int             inBytesToEncode;
    int             inBufSize;      
    unsigned int    inSampleRate;
    unsigned int    inChannels;
    unsigned int    inBitsPerSample;
    tBTRMgrSoHdl hBTRMgrSoHdl;
} stBTRMgrCapSoHdl;


void*
doDataCapture (
    void* ptr
) {
    tBTRMgrCapSoHandle  hBTRMgrSoCap = NULL;
    int*                penCapThreadExitStatus = malloc(sizeof(int));

    FILE*           inFileFp        = NULL;
    char*           inDataBuf       = NULL;
    int             inFileBytesLeft = 0;
    int             inBytesToEncode = 0;
    int             inBufSize       = IN_BUF_SIZE;
    unsigned int    inSampleRate    = 0;
    unsigned int    inChannels      = 0;
    unsigned int    inBitsPerSample = 0;

    tBTRMgrSoHdl hBTRMgrSoHdl;

    struct timeval tv;
    unsigned long int    prevTime = 0, currTime = 0, processingTime = 0;
    unsigned long int    sleepTime = 0;

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
    inSampleRate    = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inSampleRate;
    inChannels      = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inChannels;
    inBitsPerSample = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->inBitsPerSample;
    hBTRMgrSoHdl    = ((stBTRMgrCapSoHdl*)hBTRMgrSoCap)->hBTRMgrSoHdl;

    if (inSampleRate && inChannels && inBitsPerSample)
        sleepTime = inBytesToEncode * 1000000.0/((float)inSampleRate * inChannels * inBitsPerSample/8);
    else
        sleepTime = 0;

    while (inFileBytesLeft) {
        gettimeofday(&tv,NULL);
        prevTime = (1000000 * tv.tv_sec) + tv.tv_usec;

        if (inFileBytesLeft < inBufSize)
            inBytesToEncode = inFileBytesLeft;

        fread (inDataBuf, 1, inBytesToEncode, inFileFp);
      
        if (eBTRMgrSuccess != BTRMgr_SO_SendBuffer(hBTRMgrSoHdl, inDataBuf, inBytesToEncode)) {
            fprintf(stderr, "BTRMgr_SO_SendBuffer - FAILED\n");
        }

        inFileBytesLeft -= inBytesToEncode;

        gettimeofday(&tv,NULL);
        currTime = (1000000 * tv.tv_sec) + tv.tv_usec;
        
        processingTime = currTime - prevTime;


        if (sleepTime > processingTime) {
            //printf("inBytesToEncode = %d sleeptime = %lu Processing =%lu\n", inBytesToEncode, sleepTime - processingTime, processingTime);
            usleep(sleepTime - processingTime);
        }
        else {
            printf("inBytesToEncode = %d sleeptime = 0 Processing =%lu\n", inBytesToEncode, processingTime);
        }
    }

    *penCapThreadExitStatus = 0;
    return (void*)penCapThreadExitStatus;
}



static int 
streamOutTestMainAlternate (
    int             argc,
    char*           argv[],
    appDataStruct*  pstAppData
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

    stBTRMgrInASettings   lstBtrMgrSoInASettings;
    stBTRMgrOutASettings  lstBtrMgrSoOutASettings;

    lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo   = NULL;
    lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo = NULL;

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

    lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo   = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ? 
                                                                    (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));
    lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) > sizeof(stBTRCoreDevMediaMpegInfo) ?
                                                                    (sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) : sizeof(stBTRCoreDevMediaMpegInfo));

    if (!(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo) || !(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo)) {
        fprintf(stderr, "BTRMgr_SO_Init - MEMORY ALLOC FAILED\n");
        return -1;
    }


    if (eBTRMgrSuccess != BTRMgr_SO_Init(&hBTRMgrSoHdl, NULL, NULL)) {
        fprintf(stderr, "BTRMgr_SO_Init - FAILED\n");
        return -1;
    }

    inBytesToEncode = inBufSize;

    /* TODO: Choose these valus based on information parsed from the wav file */
    lstBtrMgrSoInASettings.eBtrMgrInAType     = eBTRMgrATypePCM;

    if (lstBtrMgrSoInASettings.eBtrMgrInAType == eBTRMgrATypePCM) {
        stBTRMgrPCMInfo* pstBtrMgrSoInPcmInfo = (stBTRMgrPCMInfo*)(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo);

        /* TODO: Set these valus based on information parsed from the wav file */
        pstBtrMgrSoInPcmInfo->eBtrMgrSFmt  = eBTRMgrSFmt16bit;
        pstBtrMgrSoInPcmInfo->eBtrMgrAChan = eBTRMgrAChanStereo;
        pstBtrMgrSoInPcmInfo->eBtrMgrSFreq = eBTRMgrSFreq48K;
    }


    if (pstAppData) {
        if (pstAppData->stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
            stBTRMgrSBCInfo*          pstBtrMgrSoOutSbcInfo = ((stBTRMgrSBCInfo*)(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo));
            stBTRCoreDevMediaSbcInfo*   pstBtrCoreDevMediaSbcInfo = ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo));
            
            lstBtrMgrSoOutASettings.eBtrMgrOutAType   = eBTRMgrATypeSBC;
            if (pstBtrMgrSoOutSbcInfo && pstBtrCoreDevMediaSbcInfo) {

                if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 8000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq8K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 16000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq16K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 32000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq32K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 44100) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq44_1K;
                }
                else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 48000) {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq48K;
                }
                else {
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreqUnknown;
                }


                switch (pstBtrCoreDevMediaSbcInfo->eDevMAChan) {
                case eBTRCoreDevMediaAChanMono:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanMono;
                    break;
                case eBTRCoreDevMediaAChanDualChannel:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanDualChannel;
                    break;
                case eBTRCoreDevMediaAChanStereo:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanStereo;
                    break;
                case eBTRCoreDevMediaAChanJointStereo:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanJStereo;
                    break;
                case eBTRCoreDevMediaAChan5_1:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan5_1;
                    break;
                case eBTRCoreDevMediaAChan7_1:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan7_1;
                    break;
                case eBTRCoreDevMediaAChanUnknown:
                default:
                    pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanUnknown;
                    break;
                }

                pstBtrMgrSoOutSbcInfo->ui8SbcAllocMethod  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcAllocMethod;
                pstBtrMgrSoOutSbcInfo->ui8SbcSubbands     = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcSubbands;
                pstBtrMgrSoOutSbcInfo->ui8SbcBlockLength  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcBlockLength;
                pstBtrMgrSoOutSbcInfo->ui8SbcMinBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMinBitpool;
                pstBtrMgrSoOutSbcInfo->ui8SbcMaxBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMaxBitpool;
                pstBtrMgrSoOutSbcInfo->ui16SbcFrameLen    = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcFrameLen;
                pstBtrMgrSoOutSbcInfo->ui16SbcBitrate     = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcBitrate;
            }
        }
    }


    lstBtrMgrSoOutASettings.i32BtrMgrDevFd      = outFileFd;
    lstBtrMgrSoOutASettings.i32BtrMgrDevMtu     = outMTUSize;


    if (eBTRMgrSuccess != BTRMgr_SO_GetEstimatedInABufSize(hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) {
        fprintf(stderr, "BTRMgr_SO_GetEstimatedInABufSize FAILED\n");
        lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize = inBytesToEncode;
    }
    else {
        inBytesToEncode = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;
    }

    inBufSize = inBytesToEncode ;
    inDataBuf = (char*)malloc(inBytesToEncode * sizeof(char));

    if (eBTRMgrSuccess != BTRMgr_SO_Start(hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) {
        fprintf(stderr, "BTRMgr_SO_Start - FAILED\n");
        return -1;
    }

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

        if (eBTRMgrSuccess != BTRMgr_SO_SendBuffer(hBTRMgrSoHdl, inDataBuf, inBytesToEncode)) {
            fprintf(stderr, "BTRMgr_SO_SendBuffer - FAILED\n");
        }

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
    lstBTRMgrCapSoHdl.inBitsPerSample = 16;     //TODO: Get from Wav file
    lstBTRMgrCapSoHdl.inChannels      = 2;      //TODO: Get from Wav file
    lstBTRMgrCapSoHdl.inSampleRate    = 48000;  //TODO: Get from Wav file
    lstBTRMgrCapSoHdl.hBTRMgrSoHdl    = hBTRMgrSoHdl;

    if((dataCapThread = g_thread_new(NULL, doDataCapture, (void*)&lstBTRMgrCapSoHdl)) == NULL) {
         fprintf(stderr, "Failed to create data Capture Thread\n");
    }

    penDataCapThreadExitStatus = g_thread_join(dataCapThread);

    (void)penDataCapThreadExitStatus;
#endif

    if (eBTRMgrSuccess != BTRMgr_SO_SendEOS(hBTRMgrSoHdl)) {
        fprintf(stderr, "BTRMgr_SO_SendEOS - FAILED\n");
    }

    if (eBTRMgrSuccess != BTRMgr_SO_Stop(hBTRMgrSoHdl)) {
        fprintf(stderr, "BTRMgr_SO_Stop - FAILED\n");
    }


    if (lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo)
        free(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo);

    if (lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo)
        free(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo);

    if (inDataBuf)
        free(inDataBuf);

    if (eBTRMgrSuccess != BTRMgr_SO_DeInit(hBTRMgrSoHdl)) {
        fprintf(stderr, "BTRMgr_SO_DeInit - FAILED\n");
    }

    if (streamOutMode == 1)
        fclose(outFileFp);

    fclose(inFileFp);

    return 0;
}


/*  Incoming Callbacks */
static eBTRMgrRet
btmgr_ACDataReadyCallback (
    void*           apvAcDataBuf,
    unsigned int    aui32AcDataLen,
    void*           apvUserData
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

    eBTRMgrRet      leBtrMgrSoRet = eBTRMgrSuccess;
    appDataStruct *data = (appDataStruct*)apvUserData;

    if ((leBtrMgrSoRet = BTRMgr_SO_SendBuffer(data->hBTRMgrSoHdl, apvAcDataBuf, aui32AcDataLen)) != eBTRMgrSuccess) {
        fprintf(stderr, "BTRMgr_SO_SendBuffer - FAILED\n");
    }

    data->bytesWritten += aui32AcDataLen;

#if 0
    printf("audioCaptureCb size=%06d bytesWritten=%06lu\n", aui32AcDataLen, data->bytesWritten);
    fflush(stdout);

    gettimeofday(&tv,NULL);
    printf("CB Time uSec =%lu\n", ((1000000 * tv.tv_sec) + tv.tv_usec) - currTime);
#endif

    return leBtrMgrSoRet;
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

    stBTRMgrOutASettings    lstBtrMgrAcOutASettings;
    stBTRMgrInASettings     lstBtrMgrSoInASettings;
    stBTRMgrOutASettings    lstBtrMgrSoOutASettings;

    lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ?
                                                                    (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));
    lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo   = (void*)malloc((sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) > sizeof(stBTRMgrMPEGInfo) ? 
                                                                    (sizeof(stBTRMgrPCMInfo) > sizeof(stBTRMgrSBCInfo) ? sizeof(stBTRMgrPCMInfo) : sizeof(stBTRMgrSBCInfo)) : sizeof(stBTRMgrMPEGInfo));
    lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo = (void*)malloc((sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) > sizeof(stBTRCoreDevMediaMpegInfo) ?
                                                                    (sizeof(stBTRCoreDevMediaPcmInfo) > sizeof(stBTRCoreDevMediaSbcInfo) ? sizeof(stBTRCoreDevMediaPcmInfo) : sizeof(stBTRCoreDevMediaSbcInfo)) : sizeof(stBTRCoreDevMediaMpegInfo));

    if (!(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo) || !(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo) || !(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo)) {
        fprintf(stderr, "BTRMgr_SO_Init - MEMORY ALLOC FAILED\n");
        return -1;
    }

    if (!pstAppData) {
        fprintf(stderr, "Invalid Input argument FAILED\n");
        goto err_open;
    }

    if (eBTRMgrSuccess != BTRMgr_SO_Init(&(pstAppData->hBTRMgrSoHdl), NULL, NULL)) {
        fprintf(stderr, "BTRMgr_SO_Init - FAILED\n");
        goto err_open;
    }

    if (eBTRMgrSuccess != BTRMgr_AC_Init(&(pstAppData->hBTRMgrAcHdl))) {
        fprintf(stderr, "BTRMgr_AC_Init - FAILED\n");
        goto err_open;
    }


    if (eBTRMgrSuccess != BTRMgr_AC_GetDefaultSettings(pstAppData->hBTRMgrAcHdl, &lstBtrMgrAcOutASettings)) {
        fprintf(stderr, "BTRMgr_AC_GetDefaultSettings - FAILED\n");
        goto err_open;
    }


    lstBtrMgrSoInASettings.eBtrMgrInAType     = lstBtrMgrAcOutASettings.eBtrMgrOutAType;

    if (lstBtrMgrSoInASettings.eBtrMgrInAType == eBTRMgrATypePCM) {
        stBTRMgrPCMInfo* pstBtrMgrSoInPcmInfo = (stBTRMgrPCMInfo*)(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo);
        stBTRMgrPCMInfo* pstBtrMgrAcOutPcmInfo = (stBTRMgrPCMInfo*)(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo);

        memcpy(pstBtrMgrSoInPcmInfo, pstBtrMgrAcOutPcmInfo, sizeof(stBTRMgrPCMInfo));
    }


    if (pstAppData->stBtrCoreDevMediaInfo.eBtrCoreDevMType == eBTRCoreDevMediaTypeSBC) {
        stBTRMgrSBCInfo*          pstBtrMgrSoOutSbcInfo = ((stBTRMgrSBCInfo*)(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo));
        stBTRCoreDevMediaSbcInfo* pstBtrCoreDevMediaSbcInfo = ((stBTRCoreDevMediaSbcInfo*)(pstAppData->stBtrCoreDevMediaInfo.pstBtrCoreDevMCodecInfo));
        
        lstBtrMgrSoOutASettings.eBtrMgrOutAType   = eBTRMgrATypeSBC;
        if (pstBtrMgrSoOutSbcInfo && pstBtrCoreDevMediaSbcInfo) {

            if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 8000) {
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq8K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 16000) {
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq16K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 32000) {
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq32K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 44100) {
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq44_1K;
            }
            else if (pstBtrCoreDevMediaSbcInfo->ui32DevMSFreq == 48000) {
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreq48K;
            }
            else {
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcSFreq  = eBTRMgrSFreqUnknown;
            }


            switch (pstBtrCoreDevMediaSbcInfo->eDevMAChan) {
            case eBTRCoreDevMediaAChanMono:
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanMono;
                break;
            case eBTRCoreDevMediaAChanDualChannel:
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanDualChannel;
                break;
            case eBTRCoreDevMediaAChanStereo:
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanStereo;
                break;
            case eBTRCoreDevMediaAChanJointStereo:
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanJStereo;
                break;
            case eBTRCoreDevMediaAChan5_1:
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan5_1;
                break;
            case eBTRCoreDevMediaAChan7_1:
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChan7_1;
                break;
            case eBTRCoreDevMediaAChanUnknown:
            default:
                pstBtrMgrSoOutSbcInfo->eBtrMgrSbcAChan  = eBTRMgrAChanUnknown;
                break;
            }

            pstBtrMgrSoOutSbcInfo->ui8SbcAllocMethod  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcAllocMethod;
            pstBtrMgrSoOutSbcInfo->ui8SbcSubbands     = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcSubbands;
            pstBtrMgrSoOutSbcInfo->ui8SbcBlockLength  = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcBlockLength;
            pstBtrMgrSoOutSbcInfo->ui8SbcMinBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMinBitpool;
            pstBtrMgrSoOutSbcInfo->ui8SbcMaxBitpool   = pstBtrCoreDevMediaSbcInfo->ui8DevMSbcMaxBitpool;
            pstBtrMgrSoOutSbcInfo->ui16SbcFrameLen    = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcFrameLen;
            pstBtrMgrSoOutSbcInfo->ui16SbcBitrate     = pstBtrCoreDevMediaSbcInfo->ui16DevMSbcBitrate;
        }
    }

    lstBtrMgrSoOutASettings.i32BtrMgrDevFd      = outFileFd;
    lstBtrMgrSoOutASettings.i32BtrMgrDevMtu     = outMTUSize;


    if (eBTRMgrSuccess != BTRMgr_SO_GetEstimatedInABufSize(pstAppData->hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) {
        fprintf(stderr, "BTRMgr_SO_GetEstimatedInABufSize FAILED\n");
        lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize = inBytesToEncode;
    }
    else {
        inBytesToEncode = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;
    }


    if (eBTRMgrSuccess != BTRMgr_SO_Start(pstAppData->hBTRMgrSoHdl, &lstBtrMgrSoInASettings, &lstBtrMgrSoOutASettings)) {
        fprintf(stderr, "BTRMgr_SO_Start - FAILED\n");
        return -1;
    }


    lstBtrMgrAcOutASettings.i32BtrMgrOutBufMaxSize = lstBtrMgrSoInASettings.i32BtrMgrInBufMaxSize;

    if (eBTRMgrSuccess != BTRMgr_AC_Start(pstAppData->hBTRMgrAcHdl,
                                          &lstBtrMgrAcOutASettings,
                                          btmgr_ACDataReadyCallback,
                                          pstAppData
                                         )) {
        goto err_open;
    }

    printf("BT AUDIO OUT - STARTED \n");

err_open:
    if (lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo)
        free(lstBtrMgrSoOutASettings.pstBtrMgrOutCodecInfo);

    if (lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo)
        free(lstBtrMgrSoInASettings.pstBtrMgrInCodecInfo);

    if (lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo)
        free(lstBtrMgrAcOutASettings.pstBtrMgrOutCodecInfo);

    return 0;
}


static int 
streamOutLiveTestMainAlternateStop (
    appDataStruct*  pstAppData
)  {
    int ret = 0;

    if (eBTRMgrSuccess != BTRMgr_AC_Stop(pstAppData->hBTRMgrAcHdl)) {
        fprintf(stderr, "BTRMgr_AC_Stop - FAILED\n");
        ret = -1;
    }

    if (eBTRMgrSuccess != BTRMgr_SO_SendEOS(pstAppData->hBTRMgrSoHdl)) {
        fprintf(stderr, "BTRMgr_SO_SendEOS - FAILED\n");
        ret = -1;
    }

    if (eBTRMgrSuccess != BTRMgr_SO_Stop(pstAppData->hBTRMgrSoHdl)) {
        fprintf(stderr, "BTRMgr_SO_Stop - FAILED\n");
        ret = -1;
    }

    if (eBTRMgrSuccess != BTRMgr_AC_DeInit(pstAppData->hBTRMgrAcHdl)) {
        fprintf(stderr, "BTRMgr_AC_DeInit - FAILED\n");
        ret = -1;
    }

    if (eBTRMgrSuccess != BTRMgr_SO_DeInit(pstAppData->hBTRMgrSoHdl)) {
        fprintf(stderr, "BTRMgr_SO_DeInit - FAILED\n");
        ret = -1;
    }

    pstAppData->hBTRMgrAcHdl = NULL;
    pstAppData->hBTRMgrSoHdl = NULL;

    printf("BT AUDIO OUT - STOPPED\n");

    return ret;
}


static int
streamInLiveTestMainAlternateStart (
    int             fd_number,
    int             MTU_size,
    unsigned int    aui32InSFreq,
    appDataStruct*  pstAppData
) {

    int     inBytesToEncode= IN_BUF_SIZE;
    int     inFileFd       = fd_number;
    int     inMTUSize      = MTU_size;



    BTRMgr_SI_Init(&pstAppData->hBTRMgrSiHdl, NULL, NULL);

#if 0
    /* could get defaults from audio capture, but for the sample app we want to write a the wav header first*/
    pstAppData->bitsPerSample = 16;
    pstAppData->samplerate = 48000;
    pstAppData->channels = 2;
#endif

    BTRMgr_SI_Start(pstAppData->hBTRMgrSiHdl, inBytesToEncode, inFileFd, inMTUSize, aui32InSFreq);

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
