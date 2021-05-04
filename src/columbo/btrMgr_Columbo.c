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

#include "btrMgr_Types.h"
#include "btrMgr_logger.h"
#include "btrMgr_Columbo.h"


/* Interfaces */
eBTRMgrRet BTRMGR_Columbo_GetData(BTRMGR_ColumboChar_t aenColumboChar, char* aData)
{
    eBTRMgrRet      rc = eBTRMgrSuccess;

    switch (aenColumboChar)
    {
        case BTRMGR_SYSDIAG_COLUMBO_STATUS:
        {
            strncpy(aData, "Getting Status", BTRMGR_COL_STR_LEN_MAX - 1);
        }
        break;
        case BTRMGR_SYSDIAG_COLUMBO_REPORT:
        {
            strncpy(aData, "Getting Report", BTRMGR_COL_STR_LEN_MAX - 1);
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

eBTRMgrRet BTRMGR_Columbo_SetData(BTRMGR_ColumboChar_t aenColumboChar, char* aData)
{
    eBTRMgrRet      rc = eBTRMgrSuccess;

    switch (aenColumboChar)
    {
        case BTRMGR_SYSDIAG_COLUMBO_START:
        {
            FILE *fPipe;
            char lData[256];
            fPipe = popen("/usr/bin/hwst_flex_demo.sh launch", "r");
            
            if (NULL == fPipe)
            {  /* check for errors */
                rc = eBTRMgrFailure;
                BTRMGRLOG_DEBUG("Pipe failed to open\n");
            }
            else
            {
                while(NULL != fgets(lData, 255, fPipe))
                {
                    BTRMGRLOG_DEBUG("%s", lData);
                }
                BTRMGRLOG_DEBUG("Script has finished it's execution\n");
		pclose(fPipe);   //CID:135219 - Forward Null
            }
        }
        break;
        case BTRMGR_SYSDIAG_COLUMBO_STOP:
        {
            FILE *fPipe;
            char lData[256];
            fPipe = popen("/usr/bin/hwst_flex_demo.sh exit", "r");
            if (NULL == fPipe)
            {  /* check for errors */
                rc = eBTRMgrFailure;
                BTRMGRLOG_DEBUG("Pipe failed to open\n");
            }
            else
            {
                while(NULL != fgets(lData, 255, fPipe))
                {
                    BTRMGRLOG_DEBUG("%s", lData);
                }
                BTRMGRLOG_DEBUG("Script has finished it's execution\n");
		pclose(fPipe);
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


