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
 * @file btrMgr_ctrl.c
 *
 * @description This file defines bluetooth manager's Controller functionality
 *
 */
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "btmgr.h"
#include "btrMgr_IarmInternalIfce.h"


static bool gbExitBTRMgr = false;

static void
btrMgr_SignalHandler (
    int i32SignalNumber
) {
    if (i32SignalNumber == SIGTERM)
        gbExitBTRMgr = true;
}


int
main (
    void
) {
    time_t curr = 0;
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    BTRMgr_BeginIARMMode();

    rc = BTRMGR_Init();
    if (BTRMGR_RESULT_SUCCESS == rc) {

        signal(SIGTERM, btrMgr_SignalHandler);

        while (1) {
            if (gbExitBTRMgr == true)
                break;

            time(&curr);
            printf ("I-ARM BTMgr Bus: HeartBeat at %s\n", ctime(&curr));
            fflush(stdout);
            sleep(10);
        }
    }
    else {
        printf ("I-ARM BTMgr Bus: Failed it init\n");
        fflush(stdout);
    }

    BTRMgr_TermIARMMode();

    rc = BTRMGR_DeInit();

    time(&curr);
    printf ("I-ARM BTMgr Bus: BTRMgr_TermIARMMode %s  BTRMGR_DeInit - %d\n", ctime(&curr), rc);
    fflush(stdout);

    return 0;
}
