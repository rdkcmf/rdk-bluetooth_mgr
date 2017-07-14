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
#include <time.h>
#include <unistd.h>
#include "btmgr.h"

extern void BTRMGR_BeginIARMMode();
extern void BTRMGR_TermIARMMode();

int
main (
    void
) {
    time_t curr = 0;
    BTRMGR_Result_t rc = BTRMGR_RESULT_SUCCESS;

    BTRMGR_BeginIARMMode();

    rc = BTRMGR_Init();
    if (BTRMGR_RESULT_SUCCESS == rc) {
        while(1) {
            time(&curr);
            printf ("I-ARM BTMgr Bus: HeartBeat at %s\n", ctime(&curr));
            sleep(10);
        }
    }
    else {
        printf ("I-ARM BTMgr Bus: Failed it init\n");
    }

    BTRMGR_TermIARMMode();

    return 0;
}
