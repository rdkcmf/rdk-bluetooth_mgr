/**
 * @file btrMgr_ctrl.c
 *
 * @description This file defines bluetooth manager's Controller functionality
 *
 * Copyright (c) 2015  Comcast
 */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "btmgr.h"

extern void btmgr_BeginIARMMode();
extern void btmgr_TermIARMMode();

int main ()
{
    time_t curr = 0;
    BTMGR_Result_t rc = BTMGR_RESULT_SUCCESS;

    btmgr_BeginIARMMode();

    rc = BTMGR_Init();
    if (BTMGR_RESULT_SUCCESS == rc)
    {
        while(1)
        {
            time(&curr);
            printf ("I-ARM BTMgr Bus: HeartBeat at %s\n", ctime(&curr));
            sleep(10);
        }
    }
    else
        printf ("I-ARM BTMgr Bus: Failed it init\n");

    btmgr_TermIARMMode();
    return 0;
}
