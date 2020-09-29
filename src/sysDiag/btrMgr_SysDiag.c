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
 * @file btrMgr_SysDiag.c
 *
 * @description This file implements bluetooth manager's 
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* System Headers */
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/* Ext lib Headers */

#include "libIBus.h"
#include "libIARM.h"
#ifdef BTRTEST_SYS_DIAG_ENABLE
#include "wifiSrvMgrIarmIf.h"
#endif
#include "sysMgr.h"

#include "btrMgr_Types.h"
#include "btrMgr_logger.h"
#include "btrMgr_SysDiag.h"

static int btrMgr_SysDiag_getDeviceMAC(char* aFileName, unsigned char* aData);
static int btrMgr_SysDiag_getDiagInfoFromFile(char* aFileName, char* aData);
static int btrMgr_SysDiag_getDiagInfoFromPipe(char* aCmd, char* aData);

static int btrMgr_SysDiag_getDeviceMAC(char* aFileName, unsigned char* aData)
{
    FILE *fPtr;
    unsigned char lElement;
    int index = 0;
    char temp;
    unsigned int lDeviceMac[16];
    int count = 0;
    int lDataLen = 0;
    int ch;
    int leBtrMgrAcRet = 0;
    
    fPtr = fopen(aFileName, "r");
    if (NULL == fPtr)
    {
        printf("File cannot be opened\n");
    }
    else
    {
        while ((lElement = fgetc(fPtr)) != '\n')
        {
            if (lElement != ':')
            {
                snprintf(&temp, 6, "%c", lElement);
                sscanf(&temp, "%x", &ch);
                lDeviceMac[index] = ch;
                index++;
            }
        }
    
        lDataLen = index;
        index = 0;
        while (index < lDataLen)
        {
            aData[count] = lDeviceMac[index] << 4;
            index++;
            aData[count] |= lDeviceMac[index++];
            count++;
        }
        fclose(fPtr);
    }
    printf("device mac addr is %s\n", aData);
    return leBtrMgrAcRet;
}

static int btrMgr_SysDiag_getDiagInfoFromFile(char* aFileName, char* aData)
{
    FILE* fPtr = NULL;
    int leBtrMgrAcRet = 0;

    fPtr = fopen(aFileName, "r");
    if (NULL == fPtr)
    {
        leBtrMgrAcRet = -1;
        printf("File cannot be opened\n");
    }
    else
    {
        if (NULL == fgets(aData, BTRMGR_STR_LEN_MAX, fPtr))
        {
            BTRMGRLOG_DEBUG("Could not parse output of <%s>\n", aFileName);
        }
        else
        {
            if ('\n' == aData[strlen(aData) - 1])
            {
                aData[strlen(aData) - 1] = '\0';
            }
        }
        pclose(fPtr);
    }
    return leBtrMgrAcRet;
}

static int btrMgr_SysDiag_getDiagInfoFromPipe(char* aCmd, char* aData)
{
    FILE *fPipe;
    int leBtrMgrAcRet = 0;
    
    fPipe = popen(aCmd, "r");
    if (NULL == fPipe)
    {  /* check for errors */
        leBtrMgrAcRet = -1;
        BTRMGRLOG_DEBUG("Pipe failed to open\n");
    }
    else
    {
        if (NULL == fgets(aData, BTRMGR_STR_LEN_MAX, fPipe))
        {
            BTRMGRLOG_DEBUG("Could not parse output of <%s>\n", aCmd);
        }
        else
        {
            if ('\n' == aData[strlen(aData) - 1])
            {
                aData[strlen(aData) - 1] = '\0';
            }
        }
        pclose(fPipe);
    }

    return leBtrMgrAcRet;
}


/* Interfaces */
eBTRMgrRet BTRMGR_SysDiag_GetData(BTRMGR_SysDiagChar_t aenSysDiagChar, char* aData)
{
    eBTRMgrRet      rc = eBTRMgrSuccess;
    IARM_Result_t lIARMStatus = IARM_RESULT_SUCCESS;

    switch (aenSysDiagChar)
    {
        case BTRMGR_SYS_DIAG_DEVICEMAC:
        {
            unsigned char lData[BTRMGR_STR_LEN_MAX] = "\0";

            btrMgr_SysDiag_getDeviceMAC("/tmp/.estb_mac", lData);
            int ret = snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", lData);
	    if (ret > (BTRMGR_STR_LEN_MAX - 1)) {
                BTRMGRLOG_DEBUG("BTRMGR_SYS_DIAG_DEVICEMAC truncated\n");
	    }
        }
        break;
        case BTRMGR_SYS_DIAG_BTRADDRESS:
        {
            btrMgr_SysDiag_getDiagInfoFromPipe("hcitool dev |grep hci |cut -d$'\t' -f3", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_SYSTEMID:
        case BTRMGR_SYS_DIAG_HWREVISION:
        case BTRMGR_SYS_DIAG_MODELNUMBER:
        {
            btrMgr_SysDiag_getDiagInfoFromFile("/tmp/.model_number", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_SERIALNUMBER:
        {
            btrMgr_SysDiag_getDiagInfoFromPipe("grep Serial /proc/cpuinfo | cut -d ' ' -f2 | tr '[:lower:]' '[:upper:]'", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_FWREVISION:
        case BTRMGR_SYS_DIAG_SWREVISION:
        {
            btrMgr_SysDiag_getDiagInfoFromFile("/tmp/.imageVersion", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_MFGRNAME:
        {
            btrMgr_SysDiag_getDiagInfoFromPipe("grep MFG_NAME /etc/device.properties | cut -d'=' -f2", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_DEVICESTATUS:
        {
        }
        break;
        case BTRMGR_SYS_DIAG_FWDOWNLOADSTATUS:
        {
            IARM_Bus_SYSMgr_GetSystemStates_Param_t param = { 0 };
            char lValue[BTRMGR_STR_LEN_MAX] = "";

            memset(&param, 0, sizeof(param));

            btrMgr_SysDiag_getDiagInfoFromPipe("grep BOX_TYPE /etc/device.properties | cut -d'=' -f2", lValue);
            if (!strcmp(lValue, "pi"))
            {
                BTRMGRLOG_DEBUG("Box is PI \n");
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "COMPLETED");
            }
            else
            {
                lIARMStatus = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_API_GetSystemStates, (void *)&param, sizeof(param));
                if (IARM_RESULT_SUCCESS != lIARMStatus)
                {
                    BTRMGRLOG_DEBUG("Failure : Return code is %d", lIARMStatus);
                    rc = eBTRMgrFailure;
                }
                else
                {
                    BTRMGRLOG_DEBUG("Iarm call fw state :%d\n", param.firmware_download.state);

                    if (IARM_BUS_SYSMGR_IMAGE_FWDNLD_DOWNLOAD_INPROGRESS == param.firmware_download.state)
                    {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "IN PROGRESS");
                    }
                    else
                    {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "COMPLETED");
                    }
                }
            }
        }
        break;
        case BTRMGR_SYS_DIAG_WEBPASTATUS:
        {
            char lValue[BTRMGR_STR_LEN_MAX] = "";
            char* lCmd = "tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.TR069support.Enable 2>&1 1>/dev/null";

            btrMgr_SysDiag_getDiagInfoFromPipe(lCmd, lValue);
            if (0 == strcmp(lValue, "true"))
            {
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "UP");
            }
            else
            {
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "DOWN");
            }
            BTRMGRLOG_DEBUG("Webpa status:%s", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_WIFIRADIO1STATUS:
        case BTRMGR_SYS_DIAG_WIFIRADIO2STATUS:
        {
#ifdef BTRTEST_SYS_DIAG_ENABLE
            IARM_BUS_WiFi_DiagsPropParam_t param = { 0 };
            memset(&param, 0, sizeof(param));

            lIARMStatus = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_getRadioProps, (void *)&param, sizeof(param));

            BTRMGRLOG_DEBUG("Wifi status is %s", param.data.radio.params.status);
            if (IARM_RESULT_SUCCESS != lIARMStatus)
            {
                BTRMGRLOG_DEBUG("Failure : Return code is %d", lIARMStatus);
                rc = eBTRMgrFailure;
            }
            else
            {
                if (!strcmp("UP", param.data.radio.params.status))
                {
                    snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "UP");
                }
                else
                {
                    snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "DOWN");
                }
            }
#else
            BTRMGRLOG_DEBUG("Wifi diagnostics is not available\n");
            snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "DOWN");
#endif /* #ifdef BTRTEST_SYS_DIAG_ENABLE */
        }
        break;
        case BTRMGR_SYS_DIAG_RFSTATUS:
        {
            IARM_Bus_SYSMgr_GetSystemStates_Param_t param = { 0 };
            char lValue[BTRMGR_STR_LEN_MAX] = "";

            memset(&param, 0, sizeof(param));

            btrMgr_SysDiag_getDiagInfoFromPipe("grep GATEWAY_DEVICE /etc/device.properties | cut -d'=' -f2", lValue);
            BTRMGRLOG_DEBUG("Is Gateway device:%s", lValue);

            if (!strcmp(lValue, "false"))
            {
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "NOT CONNECTED");
            }
            else
            {
                lIARMStatus = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_API_GetSystemStates, (void *)&param, sizeof(param));
                if (IARM_RESULT_SUCCESS != lIARMStatus)
                {
                    BTRMGRLOG_DEBUG("Failure : Return code is %d", lIARMStatus);
                    rc = eBTRMgrFailure;
                }
                else
                {
                    BTRMGRLOG_DEBUG(" Iarm call fw state :%d\n", param.rf_connected.state);

                    if (0 == param.rf_connected.state)
                    {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "NOT CONNECTED");
                    }
                    else
                    {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "CONNECTED");
                    }
                }
            }
        }
        break;
        default:
        {
            rc = eBTRMgrFailure;
        }
        break;
    }

    return rc;
}

eBTRMgrRet BTRMGR_Wifi_ConnectToWifi(char* aSSID, char* aPassword, int aSecurityMode) 
{
    eBTRMgrRet      rc = eBTRMgrSuccess;

#ifdef BTRTEST_SYS_DIAG_ENABLE
    IARM_Result_t retVal = IARM_RESULT_SUCCESS;
    IARM_Bus_WiFiSrvMgr_Param_t param;

    strcpy(param.data.connect.ssid, aSSID);
    strcpy(param.data.connect.passphrase, aPassword);
    param.data.connect.security_mode = (SsidSecurity)aSecurityMode;

    retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_connect, (void *)&param, sizeof(param));

    BTRMGRLOG_DEBUG("\n \"%s\", status: \"%s\"", IARM_BUS_WIFI_MGR_API_connect, ((retVal == IARM_RESULT_SUCCESS && param.status) ? "Success" : "Failure"));
#else
    BTRMGRLOG_DEBUG("Wifi not available\n");
#endif /* #ifdef BTRTEST_SYS_DIAG_ENABLE */

    return rc;
}
