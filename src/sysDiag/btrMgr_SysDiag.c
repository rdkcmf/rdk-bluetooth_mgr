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
#ifdef BTRTEST_SYS_DIAG_ENABLE
#include "libIBus.h"
#include "libIARM.h"
#include "wifiSrvMgrIarmIf.h"
#include "sysMgr.h"
#include "pwrMgr.h"
#endif

/* Interface lib Headers */
#include "btrMgr_logger.h"

/* Local Headers */
#include "btrMgr_Types.h"
#include "btrMgr_SysDiag.h"


typedef struct _stBTRMgrSDHdl {
#ifdef BTRTEST_SYS_DIAG_ENABLE
    IARM_Bus_PWRMgr_PowerState_t    _powerState;
#endif
    stBTRMgrSysDiagStatus           lstBtrMgrSysDiagStat;
    fPtr_BTRMgr_SD_StatusCb         fpcBSdStatus;
    void*                           pvcBUserData;
} stBTRMgrSDHdl;

stBTRMgrSDHdl* gpstSDHandle = NULL;

/* Static Function Prototypes */
static int btrMgr_SysDiag_getDeviceMAC(char* aFileName, unsigned char* aData);
static int btrMgr_SysDiag_getDiagInfoFromFile(char* aFileName, char* aData);
static int btrMgr_SysDiag_getDiagInfoFromPipe(char* aCmd, char* aData);

/* Incoming Callbacks Prototypes */
#ifdef BTRTEST_SYS_DIAG_ENABLE
static void btrMgr_SysDiag_powerModeChangeCb (const char *owner, IARM_EventId_t eventId, void *data, size_t len); 
#endif

/* Static Function Definitions */
static int
btrMgr_SysDiag_getDeviceMAC (
    char*           aFileName,
    unsigned char*  aData
) {
    FILE *fPtr;
    unsigned char lElement;
    int index = 0;
    char temp[6];
    unsigned int lDeviceMac[16];
    int count = 0;
    int lDataLen = 0;
    int ch = 0;
    int leBtrMgrAcRet = 0;
    
    fPtr = fopen(aFileName, "r");
    if (NULL == fPtr) {
        printf("File cannot be opened\n");
    }
    else {
        while ((lElement = fgetc(fPtr)) != '\n') {
            if (lElement != ':') {
                snprintf(temp, sizeof(temp), "%c", lElement);
                sscanf(temp, "%x", &ch);
                lDeviceMac[index] = ch;
                index++;
            }
        }
    
        lDataLen = index;
        index = 0;
        while (index < lDataLen) {
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

static int
btrMgr_SysDiag_getDiagInfoFromFile (
    char* aFileName,
    char* aData
) {
    FILE* fPtr = NULL;
    int leBtrMgrAcRet = 0;

    fPtr = fopen(aFileName, "r");
    if (NULL == fPtr) {
        leBtrMgrAcRet = -1;
        printf("File cannot be opened\n");
    }
    else {
        if (NULL == fgets(aData, BTRMGR_STR_LEN_MAX, fPtr)) {
            BTRMGRLOG_DEBUG("Could not parse output of <%s>\n", aFileName);
        }
        else {
            if ('\n' == aData[strlen(aData) - 1]) {
                aData[strlen(aData) - 1] = '\0';
            }
        }
        fclose(fPtr);   //CID:115333 - Alloc free mismatch
    }

    return leBtrMgrAcRet;
}

static int
btrMgr_SysDiag_getDiagInfoFromPipe (
    char* aCmd,
    char* aData
) {
    FILE *fPipe;
    int leBtrMgrAcRet = 0;
    
    fPipe = popen(aCmd, "r");
    if (NULL == fPipe) {    /* check for errors */
        leBtrMgrAcRet = -1;
        BTRMGRLOG_DEBUG("Pipe failed to open\n");
    }
    else {
        if (NULL == fgets(aData, BTRMGR_STR_LEN_MAX, fPipe)) {
            BTRMGRLOG_DEBUG("Could not parse output of <%s>\n", aCmd);
        }
        else {
            if ('\n' == aData[strlen(aData) - 1]) {
                aData[strlen(aData) - 1] = '\0';
            }
        }

        pclose(fPipe);
    }

    return leBtrMgrAcRet;
}


/* Interfaces - Public Functions */
eBTRMgrRet
BTRMgr_SD_Init (
    tBTRMgrSDHdl*           hBTRMgrSdHdl,
    fPtr_BTRMgr_SD_StatusCb afpcBSdStatus,
    void*                   apvUserData
) {
    stBTRMgrSDHdl* sDHandle = NULL;

    if ((sDHandle = (stBTRMgrSDHdl*)malloc (sizeof(stBTRMgrSDHdl))) == NULL) {
        BTRMGRLOG_ERROR ("BTRMgr_SD_Init FAILED\n");
        return eBTRMgrFailure;
    }

    memset(sDHandle, 0, sizeof(stBTRMgrSDHdl));
    sDHandle->lstBtrMgrSysDiagStat.enSysDiagChar = BTRMGR_SYS_DIAG_UNKNOWN;
#ifdef BTRTEST_SYS_DIAG_ENABLE
    sDHandle->_powerState = IARM_BUS_PWRMGR_POWERSTATE_OFF;
#endif
    sDHandle->fpcBSdStatus= afpcBSdStatus;
    sDHandle->pvcBUserData= apvUserData;

    gpstSDHandle = sDHandle;
    *hBTRMgrSdHdl = (tBTRMgrSDHdl)sDHandle;
    return eBTRMgrSuccess;
}


eBTRMgrRet
BTRMgr_SD_DeInit (
    tBTRMgrSDHdl hBTRMgrSdHdl
) {
    stBTRMgrSDHdl*  pstBtrMgrSdHdl = (stBTRMgrSDHdl*)hBTRMgrSdHdl;

    if (NULL != pstBtrMgrSdHdl) {
        gpstSDHandle = NULL;
        free((void*)pstBtrMgrSdHdl);
        pstBtrMgrSdHdl = NULL;
        BTRMGRLOG_INFO ("BTRMgr_SD_DeInit SUCCESS\n");
        return eBTRMgrSuccess;
    }
    else {
        BTRMGRLOG_WARN ("BTRMgr SD handle is not Inited(NULL)\n");
        return eBTRMgrFailure;
    }
}


eBTRMgrRet
BTRMGR_SysDiag_GetData (
    tBTRMgrSDHdl         hBTRMgrSdHdl,
    BTRMGR_SysDiagChar_t aenSysDiagChar,
    char*                aData
) {
    stBTRMgrSDHdl*  pstBtrMgrSdHdl = (stBTRMgrSDHdl*)hBTRMgrSdHdl;
    eBTRMgrRet      rc = eBTRMgrSuccess;
#ifdef BTRTEST_SYS_DIAG_ENABLE
    IARM_Result_t   lIARMStatus = IARM_RESULT_SUCCESS;
#endif

    if (NULL == pstBtrMgrSdHdl)
        return eBTRMgrFailure;

    switch (aenSysDiagChar) {
        case BTRMGR_SYS_DIAG_DEVICEMAC: {
            unsigned char lData[BTRMGR_STR_LEN_MAX] = "\0";

            btrMgr_SysDiag_getDeviceMAC("/tmp/.estb_mac", lData);
            int ret = snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", lData);
            if (ret > (BTRMGR_STR_LEN_MAX - 1)) {
                    BTRMGRLOG_DEBUG("BTRMGR_SYS_DIAG_DEVICEMAC truncated\n");
            }
        }
        break;
        case BTRMGR_SYS_DIAG_BTRADDRESS: {
            btrMgr_SysDiag_getDiagInfoFromPipe("hcitool dev |grep hci |cut -d$'\t' -f3", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_SYSTEMID:
        case BTRMGR_SYS_DIAG_HWREVISION:
        case BTRMGR_SYS_DIAG_MODELNUMBER: {
            btrMgr_SysDiag_getDiagInfoFromFile("/tmp/.model_number", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_SERIALNUMBER: {
            btrMgr_SysDiag_getDiagInfoFromPipe("grep Serial /proc/cpuinfo | cut -d ' ' -f2 | tr '[:lower:]' '[:upper:]'", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_FWREVISION:
        case BTRMGR_SYS_DIAG_SWREVISION: {
            btrMgr_SysDiag_getDiagInfoFromFile("/tmp/.imageVersion", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_MFGRNAME: {
            btrMgr_SysDiag_getDiagInfoFromPipe("grep MFG_NAME /etc/device.properties | cut -d'=' -f2", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_DEVICESTATUS: {
        }
        break;
        case BTRMGR_SYS_DIAG_FWDOWNLOADSTATUS: {
            char lValue[BTRMGR_STR_LEN_MAX] = "";
#ifdef BTRTEST_SYS_DIAG_ENABLE
            IARM_Bus_SYSMgr_GetSystemStates_Param_t param = { 0 };
            memset(&param, 0, sizeof(param));
#endif

            btrMgr_SysDiag_getDiagInfoFromPipe("grep BOX_TYPE /etc/device.properties | cut -d'=' -f2", lValue);
            if (!strcmp(lValue, "pi")) {
                BTRMGRLOG_DEBUG("Box is PI \n");
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "COMPLETED");
            }
            else {
#ifdef BTRTEST_SYS_DIAG_ENABLE
                lIARMStatus = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_API_GetSystemStates, (void *)&param, sizeof(param));
                if (IARM_RESULT_SUCCESS != lIARMStatus) {
                    BTRMGRLOG_DEBUG("Failure : Return code is %d\n", lIARMStatus);
                    rc = eBTRMgrFailure;
                }
                else {
                    BTRMGRLOG_DEBUG("Iarm call fw state :%d\n", param.firmware_download.state);

                    if (IARM_BUS_SYSMGR_IMAGE_FWDNLD_DOWNLOAD_INPROGRESS == param.firmware_download.state) {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "IN PROGRESS");
                    }
                    else {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "COMPLETED");
                    }
                }
#endif
            }
        }
        break;
        case BTRMGR_SYS_DIAG_WEBPASTATUS: {
            char lValue[BTRMGR_STR_LEN_MAX] = "";
            char* lCmd = "tr181Set -g Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.TR069support.Enable 2>&1 1>/dev/null";

            btrMgr_SysDiag_getDiagInfoFromPipe(lCmd, lValue);
            if (0 == strcmp(lValue, "true")) {
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "UP");
            }
            else {
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "DOWN");
            }
            BTRMGRLOG_DEBUG("Webpa status:%s\n", aData);
        }
        break;
        case BTRMGR_SYS_DIAG_WIFIRADIO1STATUS:
        case BTRMGR_SYS_DIAG_WIFIRADIO2STATUS: {
#ifdef BTRTEST_SYS_DIAG_ENABLE
            IARM_BUS_WiFi_DiagsPropParam_t param = { 0 };
            memset(&param, 0, sizeof(param));

            lIARMStatus = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_getRadioProps, (void *)&param, sizeof(param));

            BTRMGRLOG_DEBUG("Wifi status is %s\n", param.data.radio.params.status);
            if (IARM_RESULT_SUCCESS != lIARMStatus) {
                BTRMGRLOG_DEBUG("Failure : Return code is %d\n", lIARMStatus);
                rc = eBTRMgrFailure;
            }
            else {
                if (!strcmp("UP", param.data.radio.params.status)) {
                    snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "UP");
                }
                else {
                    snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "DOWN");
                }
            }
#else
            BTRMGRLOG_DEBUG("Wifi diagnostics is not available\n");
            snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "DOWN");
#endif /* #ifdef BTRTEST_SYS_DIAG_ENABLE */
        }
        break;
        case BTRMGR_SYS_DIAG_RFSTATUS: {
            char lValue[BTRMGR_STR_LEN_MAX] = "";
#ifdef BTRTEST_SYS_DIAG_ENABLE
            IARM_Bus_SYSMgr_GetSystemStates_Param_t param = { 0 };
            memset(&param, 0, sizeof(param));
#endif

            btrMgr_SysDiag_getDiagInfoFromPipe("grep GATEWAY_DEVICE /etc/device.properties | cut -d'=' -f2", lValue);
            BTRMGRLOG_DEBUG("Is Gateway device:%s\n", lValue);

            if (!strcmp(lValue, "false")) {
                snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "NOT CONNECTED");
            }
            else {
#ifdef BTRTEST_SYS_DIAG_ENABLE
                lIARMStatus = IARM_Bus_Call(IARM_BUS_SYSMGR_NAME, IARM_BUS_SYSMGR_API_GetSystemStates, (void *)&param, sizeof(param));
                if (IARM_RESULT_SUCCESS != lIARMStatus) {
                    BTRMGRLOG_DEBUG("Failure : Return code is %d\n", lIARMStatus);
                    rc = eBTRMgrFailure;
                }
                else {
                    BTRMGRLOG_DEBUG(" Iarm call fw state :%d\n", param.rf_connected.state);

                    if (0 == param.rf_connected.state) {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "NOT CONNECTED");
                    }
                    else {
                        snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", "CONNECTED");
                    }
                }
#endif
            }
        }
        break;
        case BTRMGR_SYS_DIAG_POWERSTATE: {
#ifdef BTRTEST_SYS_DIAG_ENABLE
            IARM_Bus_PWRMgr_GetPowerState_Param_t param;
            IARM_Result_t res = IARM_Bus_Call(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_API_GetPowerState,
                    (void *)&param, sizeof(param));

            snprintf(aData, (BTRMGR_STR_LEN_MAX - 1), "%s", BTRMGR_SYS_DIAG_PWRST_UNKNOWN);
            if (res == IARM_RESULT_SUCCESS) {

                if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_ON)
                    snprintf(aData, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_ON);
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY)
                    snprintf(aData, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_STANDBY);
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP)
                    snprintf(aData, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_STDBY_LIGHT_SLEEP);
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP)
                    snprintf(aData, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_STDBY_DEEP_SLEEP);
                else if (param.curState == IARM_BUS_PWRMGR_POWERSTATE_OFF)
                    snprintf(aData, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_OFF);
                

                pstBtrMgrSdHdl->lstBtrMgrSysDiagStat.enSysDiagChar = BTRMGR_SYS_DIAG_POWERSTATE;
                strncpy(pstBtrMgrSdHdl->lstBtrMgrSysDiagStat.pcSysDiagRes, aData, BTRMGR_STR_LEN_MAX - 1);
                pstBtrMgrSdHdl->_powerState = param.curState;

                if (param.curState != IARM_BUS_PWRMGR_POWERSTATE_ON) {
                    BTRMGRLOG_WARN("BTRMGR_SYS_DIAG_POWERSTATE PWRMGR :%d - %s\n", param.curState, aData);
                    IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, btrMgr_SysDiag_powerModeChangeCb);
                }
            }
            else {
                BTRMGRLOG_DEBUG("BTRMGR_SYS_DIAG_POWERSTATE Failure : Return code is %d\n", res);
                /* In case of Failure to call GetPowerState registet the event handler anyway */
                IARM_Bus_RegisterEventHandler(IARM_BUS_PWRMGR_NAME, IARM_BUS_PWRMGR_EVENT_MODECHANGED, btrMgr_SysDiag_powerModeChangeCb);
                rc = eBTRMgrFailure;
            }
#else
            rc = eBTRMgrFailure;
#endif
        }
        break;
        default: {
            rc = eBTRMgrFailure;
        }
        break;
    }

    return rc;
}

eBTRMgrRet
BTRMGR_SysDiag_ConnectToWifi (
    tBTRMgrSDHdl    hBTRMgrSdHdl,
    char*           aSSID,
    char*           aPassword,
    int             aSecurityMode
) {
    stBTRMgrSDHdl*  pstBtrMgrSdHdl = (stBTRMgrSDHdl*)hBTRMgrSdHdl;
    eBTRMgrRet      rc = eBTRMgrSuccess;

    if (NULL == pstBtrMgrSdHdl)
        return eBTRMgrFailure;


#ifdef BTRTEST_SYS_DIAG_ENABLE
    IARM_Result_t retVal = IARM_RESULT_SUCCESS;
    IARM_Bus_WiFiSrvMgr_Param_t param;

    strcpy(param.data.connect.ssid, aSSID);
    strcpy(param.data.connect.passphrase, aPassword);
    param.data.connect.security_mode = (SsidSecurity)aSecurityMode;

    retVal = IARM_Bus_Call(IARM_BUS_NM_SRV_MGR_NAME, IARM_BUS_WIFI_MGR_API_connect, (void *)&param, sizeof(param));

    BTRMGRLOG_DEBUG("\n \"%s\", status: \"%s\"\n", IARM_BUS_WIFI_MGR_API_connect, ((retVal == IARM_RESULT_SUCCESS && param.status) ? "Success" : "Failure"));
#else
    BTRMGRLOG_DEBUG("Wifi not available\n");
#endif /* #ifdef BTRTEST_SYS_DIAG_ENABLE */

    return rc;
}

/*  Incoming Callbacks */
#ifdef BTRTEST_SYS_DIAG_ENABLE
static void
btrMgr_SysDiag_powerModeChangeCb (
    const char *owner,
    IARM_EventId_t eventId,
    void *data,
    size_t len
) {
    if (strcmp(owner, IARM_BUS_PWRMGR_NAME)  == 0) {
        switch (eventId) {
        case IARM_BUS_PWRMGR_EVENT_MODECHANGED: {
            IARM_Bus_PWRMgr_EventData_t *param = (IARM_Bus_PWRMgr_EventData_t *)data;
            BTRMGRLOG_WARN("BTRMGR_SYS_DIAG_POWERSTATE Event IARM_BUS_PWRMGR_EVENT_MODECHANGED: new State: %d\n", param->data.state.newState);

            if (gpstSDHandle != NULL) {

                if (param->data.state.newState == IARM_BUS_PWRMGR_POWERSTATE_ON)
                    snprintf(gpstSDHandle->lstBtrMgrSysDiagStat.pcSysDiagRes, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_ON);
                else if (param->data.state.newState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY)
                    snprintf(gpstSDHandle->lstBtrMgrSysDiagStat.pcSysDiagRes, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_STANDBY);
                else if (param->data.state.newState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP)
                    snprintf(gpstSDHandle->lstBtrMgrSysDiagStat.pcSysDiagRes, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_STDBY_LIGHT_SLEEP);
                else if (param->data.state.newState == IARM_BUS_PWRMGR_POWERSTATE_STANDBY_DEEP_SLEEP)
                    snprintf(gpstSDHandle->lstBtrMgrSysDiagStat.pcSysDiagRes, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_STDBY_DEEP_SLEEP);
                else if (param->data.state.newState == IARM_BUS_PWRMGR_POWERSTATE_OFF)
                    snprintf(gpstSDHandle->lstBtrMgrSysDiagStat.pcSysDiagRes, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_OFF);
                else
                    snprintf(gpstSDHandle->lstBtrMgrSysDiagStat.pcSysDiagRes, BTRMGR_STR_LEN_MAX - 1, "%s", BTRMGR_SYS_DIAG_PWRST_UNKNOWN);

                gpstSDHandle->lstBtrMgrSysDiagStat.enSysDiagChar = BTRMGR_SYS_DIAG_POWERSTATE;


                if (gpstSDHandle->_powerState != param->data.state.newState && (param->data.state.newState != IARM_BUS_PWRMGR_POWERSTATE_ON && param->data.state.newState != IARM_BUS_PWRMGR_POWERSTATE_STANDBY_LIGHT_SLEEP)) {
                    BTRMGRLOG_WARN("BTRMGR_SYS_DIAG_POWERSTATE - Device is being suspended\n");
                }

                if (gpstSDHandle->_powerState != param->data.state.newState && param->data.state.newState == IARM_BUS_PWRMGR_POWERSTATE_ON) {
                    BTRMGRLOG_WARN("BTRMGR_SYS_DIAG_POWERSTATE - Device just woke up\n");
                    if (gpstSDHandle->fpcBSdStatus) {
                        stBTRMgrSysDiagStatus   lstBtrMgrSysDiagStat;
                        eBTRMgrRet              leBtrMgrSdRet = eBTRMgrSuccess;

                        memcpy(&lstBtrMgrSysDiagStat, &gpstSDHandle->lstBtrMgrSysDiagStat, sizeof(stBTRMgrSysDiagStatus));
                        if ((leBtrMgrSdRet = gpstSDHandle->fpcBSdStatus(&lstBtrMgrSysDiagStat, gpstSDHandle->pvcBUserData)) != eBTRMgrSuccess) {
                            BTRMGRLOG_ERROR("BTRMGR_SYS_DIAG_POWERSTATE - Device woke up - NOT PROCESSED\n");
                        }
                    }
                }

                gpstSDHandle->_powerState = param->data.state.newState;
            }
        }
            break;
        default:
            break;
        }
    }
}
#endif /* #ifdef BTRTEST_SYS_DIAG_ENABLE */
