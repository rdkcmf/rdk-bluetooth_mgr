#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "btrMgr_streamOut.h"

int main (
    int     argc,
    char*   argv[]
) {
    BTRMgr_SO_Init();

    BTRMgr_SO_Start();

    BTRMgr_SO_SendBuffer();
    BTRMgr_SO_SendEOS();

    BTRMgr_SO_Stop();

    BTRMgr_SO_DeInit();

	return 1;
}
